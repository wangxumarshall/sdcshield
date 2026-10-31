/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b spinlock_add_neon
 * @parblock
 * Spinlock-liveness + coherence SDC probe (qspinlock hard-lockup phenomenon).
 *
 * 现象针对性设计（qiling-vmcore 五案诊断 → sdcshield 侧检测缺口闭合）：
 * 中行/中央国债两例 NMI hard lockup 的根因是锁 cacheline 一致性元数据瞬态
 * 故障（解锁写丢失 / 锁值读冻结），内核侧表现为关中断自旋 >10s 被 watchdog
 * 咬停。现有 lock/spinlock 用例遇同类故障只会无声挂死（超时/墙收，不算
 * 检出）。本测试补三条检测通道：
 *   通道A（数据SDC）  ：NEON vfma 计算结果 vs init 同源 golden（逐字节 memcmp）
 *   通道B（数据行W丢）：全局锁保护下的共享计数器，cleanup 时
 *                        Σ 逐线程获锁数 == 共享计数器（add-only，无抵消方向）
 *                        + 锁行元数据对账：epoch==共享计数器（同临界区构造
 *                        等值）、magic/pad==init 指纹——锁行元数据写丢失而
 *                        lock_word 完好（非挂死形态）时通道A/C 全盲，此处补检
 *   通道C（锁字挂死，核心增量）：获锁等待 >10s（对齐 sbsa-gwdt）后进入
 *                        1s 确认窗二次判定——epoch 停走且 CAS 持续失败 =
 *                        确认冻结（真冻结时 epoch 必停走：epoch 唯一递增
 *                        路径在持锁者临界区内，lock_word 冻结=无人能再持
 *                        锁）→ report_fail + dump 锁所在 64B cacheline 全部
 *                        8 个 u64（对齐五案"8 字节窗口撕裂、同行其余字段
 *                        完好"的现场录像格式；magic 字段对账爆炸半径）；
 *                        确认窗内 epoch 增长或 CAS 成功 = 锁在正常流转、
 *                        本线程被调度饥饿——log 告警并重置等待预算继续争
 *                        用（不 FAIL，防高负载共享机假阳性），跨 3 个窗口
 *                        仍无获锁才以进展异常 FAIL（消息与冻结论区分）；
 *                        确认窗另设观察者停摆防护（六轮检视 :234）——窗内
 *                        相邻采样间隔 >100ms = 本线程自身未被调度（cgroup
 *                        freezer/CRIU/VM 迁移暂停），冻结时长不得计入窗内
 *                        墙钟（重启确认窗），反复停摆以区分性签名 FAIL、
 *                        不以 hard-lockup CONFIRMED 定罪
 *
 * 三通道自检（部署到新机器后必跑，coherence_probe SELFTEST_W 同款纪律——
 * 阳性通过后阴性结果才有意义；错值 fail loudly 而非静默 off，防 typo 无声
 * 击穿部署纪律）：
 *   -O spinlock_add_neon.selftest=stall  首个进临界区线程持锁 15s →
 *                                        通道C 应触发（前置条件由 init 强制：
 *                                        ≥2 线程（否则唯一线程自睡自放、绿
 *                                        pass 假绿）、-t ≥16000ms 派生下限
 *                                        （推荐 25000——阳性 FAIL(~11s) 与排空
 *                                        (~15s) 须落标称窗口内），不满足 fail
 *                                        loudly；15s > 看门狗 10s + 确认窗 1s
 *                                        + 裕量——模拟冻结须长于两段式全检出
 *                                        时延，否则确认窗内 CAS 成功会被判
 *                                        为饥饿、阳性击穿）
 *   -O spinlock_add_neon.selftest=data   init 毒化各 slot golden →
 *                                        通道A 应触发（单线程即可，cpu 无关）
 *
 * 负载形态：每线程独立 NEON vfma 链（64 floats/slot，输入 ∈ [-1,1]，
 * 4 组 × 4 向量 × 4 轮 fused multiply-add，输出理论上界 ~3e20——4 轮交叉
 * vfma 指数增长所致，远小于 FLT_MAX，无溢出无 NaN 路径；本机探针实测
 * 10^6 slot max|out|=1.36e20、无非有限值），
 * 随后在全局自旋锁（CAS-acquire / release-store，qspinlock 同指令谱系）
 * 临界区内递增共享计数器——深争用 + 实算负载。
 *
 * 多核逻辑安全（结果与核数/线程数无关）：slot=cpu%SLOTS 输入只读共享、
 * 内核纯函数、out 缓冲线程栈私有、计数对（shared/local）同在临界区内
 * 更新（不一致窗口小于 locks.cpp 先例）、日志 O_APPEND 单次 write()。
 *
 * 逐次验证日志（-O spinlock_add_neon.logverif=1，默认 0）：
 * /var/tmp/spinlock_add_neon_<pid>.log，每行 = 迭代号/线程/输入前 4 元素/
 * 输出前 4 元素/验证结果，PASS 绿（\033[32m）、FAIL 红（\033[31m）；
 * 每线程 10000 行上限后转仅记失败（防日志爆炸），失败行永不受上限约束，
 * 失败时追加全 64 float 对照 dump。logverif=0 时完全不触碰 /var/tmp；
 * logverif=1 但打开失败（环境不可用）时 log_warning 降级禁用日志、测试
 * 照跑（诊断附属不升格为检测失败）。
 *
 * Deviation note (documented): 内核为自包含 NEON intrinsics（本仓 neon_rot
 * 家族同范式）；第三方库（openblas/sleef）计算负载变体为后续独立单元。
 * 循环条件直连 test_time_condition（非 TEST_LOOP，locks/spinlock 家族习语）
 * ——--inject-idle 的占空比注入对本测试不生效（时长仍被同选项折算），依据
 * 与边界见 run 循环尾部注释（PR#328 六轮检视 :394）。
 * @endparblock
 */

#include "sandstone.h"
#include "sandstone_p.h" /* selftest=stall 前置条件校验须读生效测试时长
                          * sApp->current_test_duration（公开 API 无时长
                          * 访问器；mce_check/smi_count 同款先例） */
#include <cerrno>   /* errno：logverif 打开失败降级路径使用；不依赖框架头链
                     * 传递包含（PR#328 三轮检视 :368——上游 include 整理时
                     * 偶然可见性会静默破碎） */
#include <chrono>   /* std::chrono::milliseconds（selftest=stall 前置条件校验读
                     * 生效测试时长）：直连纪律同 <cerrno>——此前仅经
                     * sandstone_p.h:30 → sandstone_chrono.h:24 偶然可见
                     * （PR#328 五轮检视 :443） */
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#include <arm_neon.h> /* 本文件仅经 tests/cpu/meson.build 的 aarch64 门控编译
                       *（spinlock_unaligned 同款先例）；非 aarch64 构建不含
                       * 本用例——误接线时 arm_neon.h/NEON intrinsics 即编译
                       * 失败，显式暴露而非无声死代码 */

#define SPINLOCK_ADD_NEON_SLOTS 128
#define SPINLOCK_ADD_NEON_FLOATS 64          /* 16 x float32x4 */
#define SPINLOCK_ADD_NEON_ROUNDS 4           /* |x|<=1 → 输出理论上界 ~3e20（<<FLT_MAX；实测 max ~1.4e20） */
#define SPINLOCK_ADD_NEON_STALL_SECONDS 10.0 /* sbsa-gwdt 同阈值 */
#define SPINLOCK_ADD_NEON_CONFIRM_SECONDS 1.0 /* 超时后的冻结确认窗（两段式判定，见 acquire） */
#define SPINLOCK_ADD_NEON_OBSERVER_GAP_SECONDS 0.1 /* 确认窗内观察者停摆判定阈值（见 acquire），远低于
                                                    * 确认窗、远高于正常抢占间隔（本测试 worker 每核
                                                    * 单线程钉核自旋，毫秒级间隙不触发） */
#define SPINLOCK_ADD_NEON_GAP_RESTARTS_MAX 3 /* 确认窗因观察者停摆重启的上限：超出即放弃冻结判定 */
#define SPINLOCK_ADD_NEON_STALL_SLEEP_SECONDS 15 /* 自检持锁秒数：须 > 看门狗+确认窗+裕量 */
#define SPINLOCK_ADD_NEON_STALL_MIN_TEST_MS ((SPINLOCK_ADD_NEON_STALL_SLEEP_SECONDS + 1) * 1000)
                                                 /* selftest=stall 的 -t 下限：阳性 FAIL(~11s)
                                                  * 与持锁排空(~15s) 须落在标称窗口内 */
#define SPINLOCK_ADD_NEON_STARVATION_RESETS_MAX 3 /* 饥饿重置上限：跨 3+1 窗口无获锁 → 进展异常 FAIL */
#define SPINLOCK_ADD_NEON_LOG_MAX 10000      /* 每线程逐次日志行上限 */
#define SPINLOCK_ADD_NEON_MAGIC 0x5A5AC0DE5A5AC0DEull

/* 恰一条 cacheline：lock_word 是 8 字节窗口的靶心，epoch/holder/magic 是
 * "同行其余字段完好"的对照（五案指纹格式） */
struct alignas(64) SpinlockAddNeonLockLine {
    volatile uint64_t lock_word;  /* +0  : 0=free 1=held */
    volatile uint64_t epoch;      /* +8  : 累计获锁次数（持锁者更新） */
    volatile uint64_t holder;     /* +16 : 当前持锁者 cpu */
    volatile uint64_t magic;      /* +24 : 固定指纹 */
    volatile uint64_t pad[4];     /* +32..+56 */
};
static_assert(sizeof(SpinlockAddNeonLockLine) == 64, "lock line must be one cacheline");

struct SpinlockAddNeonData {
    float *inputs;   /* SLOTS x FLOATS，init 生成，所有线程只读 */
    float *golden;   /* SLOTS x FLOATS，init 同源内核预计算，只读 */
    SpinlockAddNeonLockLine *line;
    uint64_t *shared_counter; /* 锁内 add-only（通道B 靶） */
    uint64_t *local_counts;   /* SLOTS，锁内与 shared 同窗递增 */
    int log_fd;
    int logverif;             /* knob: 0/1 */
    int selftest;             /* knob: 0=off 1=stall 2=data */
    volatile int stall_armed;
};

static uint64_t spinlock_add_neon_now_ns(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

/* 纯函数 NEON 内核：4 组 x (a,b,c,d) 向量链 x 4 轮 vfma。确定性（IEEE 单
 * 舍入 FMA），无共享状态。 */
static void spinlock_add_neon_kernel(const float *in, float *out)
{
    for (int g = 0; g < 16; g += 4) {
        float32x4_t a = vld1q_f32(in + g * 4 + 0);
        float32x4_t b = vld1q_f32(in + g * 4 + 4);
        float32x4_t c = vld1q_f32(in + g * 4 + 8);
        float32x4_t d = vld1q_f32(in + g * 4 + 12);
        for (int r = 0; r < SPINLOCK_ADD_NEON_ROUNDS; ++r) {
            a = vfmaq_f32(a, b, c);
            b = vfmaq_f32(b, c, d);
            c = vfmaq_f32(c, d, a);
            d = vfmaq_f32(d, a, b);
        }
        vst1q_f32(out + g * 4 + 0, a);
        vst1q_f32(out + g * 4 + 4, b);
        vst1q_f32(out + g * 4 + 8, c);
        vst1q_f32(out + g * 4 + 12, d);
    }
}

/* CAS-acquire 自旋（qspinlock 同谱系）；等待超阈值返回 false（看门狗触发）。
 * 时钟每 1024 次重试采样一次（PR#328 二轮检视 :136）：逐次 clock_gettime
 * 走 vDSO、触碰 vvar 页另一共享行，深争用下约半数自旋预算为非锁流量，
 * 稀释锁行争用激发形态；1024 次自旋（CAS+yield，量级 ~0.1ms）<< 10s
 * 看门狗阈值，检出延迟增量可忽略 */
#define SPINLOCK_ADD_NEON_SPIN_CLOCK_PERIOD 1024
static_assert((SPINLOCK_ADD_NEON_SPIN_CLOCK_PERIOD &
               (SPINLOCK_ADD_NEON_SPIN_CLOCK_PERIOD - 1)) == 0,
              "spin clock sampling period must be a power of two");

enum SpinlockAddNeonAcquire {
    SPINLOCK_ACQUIRE_OK,
    SPINLOCK_ACQUIRE_FROZEN,      /* 确认冻结：确认窗内 epoch 停走 + CAS 持续失败 */
    SPINLOCK_ACQUIRE_NO_PROGRESS, /* 锁行持续流动但本线程跨窗口无法获锁 */
    SPINLOCK_ACQUIRE_OBSERVER_STALLED, /* 确认窗反复被本线程自身的停摆打断，无法给出冻结判定 */
};

/* 两段式看门狗判定（PR#328 四轮检视 :157）：等待超时 ≠ 锁冻结——等待者被
 * 调度饥饿 >10s（高负载共享机/cgroup 限流抢占本线程，此时他线程正常获锁）
 * 时墙钟同样超时，单段墙钟判定会把该形态误报为 hard-lockup。超时后进入
 * 确认窗（SPINLOCK_ADD_NEON_CONFIRM_SECONDS）二次采样：
 *   - CAS 成功，或锁行 epoch 增长 → 锁在正常流转，超时源于本线程饥饿/
 *     瞬态 → log 告警、重置等待预算继续争用，不 FAIL；
 *   - epoch 停走且 CAS 持续失败满窗 → 确认冻结 → FAIL。
 * 真冻结时 epoch 必停走：epoch 唯一递增路径是持锁者在临界区内的自增
 * （run 的 C 段），lock_word 冻结 = 无人再能成为持锁者 = 递增路径已死，
 * 故"停走才 FAIL"不引入新假阴性；非对称读冻结（本核观测冻住、他核正常
 * 流转）同样走 FAIL：本核 CAS 持续失败且（本核读到的）epoch 停走，按
 * 冻结论处。饥饿重置有上限（SPINLOCK_ADD_NEON_STARVATION_RESETS_MAX）：
 * 连续 3 次重置后仍超时且锁行在流动 = 持续饥饿/进展异常，以区分性消息
 * FAIL——既防假阳性也防线程在无限重置中永不返回。FAIL 报文/dump 的等待
 * 时长取进入 acquire 起的累计（t_entry 基线，跨窗求和），看门狗预算基线
 * t0 则每窗重置——两基线分工见函数内注释（PR#328 五轮检视 :309）。
 *
 * 观察者停摆防护（PR#328 六轮检视 :234）：确认窗的墙钟证据只在本线程
 * 实际运行时才成立。cgroup freezer / CRIU checkpoint / VM live-migration
 * 等全局进程冻结若落在确认窗内，冻结时长被原样计入 now-c0——恢复瞬间
 * 持锁者同样刚解冻、尚未完成临界区，epoch 静止 + CAS 失败 + now-c0 超
 * 窗三条件在解冻后第一个采样点同时成立，原判定以最高置信度签名
 * "hard-lockup signature CONFIRMED" 误报（本机已实证复现：selftest=stall
 * 下 SIGSTOP 全进程 12s 冻结落在窗内，恢复瞬间 FAIL，报文等待时长含冻
 * 结 12s）。判别手段：冻结必然恢复、真冻结永不恢复——停摆签名（相邻两
 * 次采样间隔 > SPINLOCK_ADD_NEON_OBSERVER_GAP_SECONDS，远超窗内单次迭代
 * 成本）检出即重启确认窗（重取 epoch0/c0，冻结时长不再计入）；重启后的
 * 窗内锁行流动/获锁成功 → 按饥饿处理（全局冻结恢复后持锁者微秒级完成临
 * 界区并释放），真冻结 epoch 永不恢复 → 重启后的窗照常确认。反复停摆
 * （一窗内 > SPINLOCK_ADD_NEON_GAP_RESTARTS_MAX 次）= 观察者自身被深度
 * 限流/反复冻结，无法给出冻结判定，以区分性签名 FAIL（不以 CONFIRMED
 * 定罪）。固有边界（与四轮检视 :157 同类，保持既有处置）：解冻后持锁者
 * 被持续抢占 >1s 且观察者运行正常时，"持锁者未运行"与"释放写丢失"在
 * 等待者视角不可区分，仍按冻结判决——该形态无冻结时同样存在。 */
static enum SpinlockAddNeonAcquire spinlock_add_neon_acquire(struct SpinlockAddNeonData *d,
                                                             int cpu, uint64_t *stall_ns_out)
{
    const uint64_t limit = (uint64_t)(SPINLOCK_ADD_NEON_STALL_SECONDS * 1e9);
    const uint64_t confirm_limit = (uint64_t)(SPINLOCK_ADD_NEON_CONFIRM_SECONDS * 1e9);
    /* 两条时间基线（PR#328 五轮检视 :233/:309）：
     *   t_entry —— 本次获锁尝试的累计等待基线：自进入 acquire 起永不重置。
     *             报告口径：FROZEN/NO_PROGRESS 的 stall_ns 与 dump_line 的
     *             "LOCK STALL %.3fs" 统一取 now-t_entry（跨饥饿重置窗口求和），
     *             "across %d wait windows (%.1fs total)" 才名实相符。
     *   t0 —— 本窗看门狗预算基线：饥饿重置时重置（判定口径：确认窗判"锁在
     *         流动"后须重算 10s 预算）。原实现报告口径也误取 now-t0，4 窗各
     *         >10s 的 NO_PROGRESS 把 ~11s 的末窗时长谎报成了 total。 */
    const uint64_t t_entry = spinlock_add_neon_now_ns();
    uint64_t t0 = t_entry;
    uint64_t expected = 0;
    uint32_t spins = 0;
    int starvation_resets = 0;
    for (;;) {
        if (__atomic_compare_exchange_n(&d->line->lock_word, &expected, 1,
                                        false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return SPINLOCK_ACQUIRE_OK;
        expected = 0;
        if ((++spins & (SPINLOCK_ADD_NEON_SPIN_CLOCK_PERIOD - 1)) == 0) {
            const uint64_t waited = spinlock_add_neon_now_ns() - t0;
            if (waited > limit) {
                uint64_t epoch0 = d->line->epoch;
                uint64_t c0 = spinlock_add_neon_now_ns();
                uint64_t c_prev = c0;
                const uint64_t gap_limit =
                    (uint64_t)(SPINLOCK_ADD_NEON_OBSERVER_GAP_SECONDS * 1e9);
                int gap_restarts = 0;
                bool acquired = false, flowed = false, observer_stalled = false;
                for (;;) {
                    if (__atomic_compare_exchange_n(&d->line->lock_word, &expected, 1,
                                                    false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)) {
                        acquired = true;
                        break;
                    }
                    expected = 0;
                    if (d->line->epoch != epoch0) {
                        flowed = true;
                        break;
                    }
                    const uint64_t now = spinlock_add_neon_now_ns();
                    /* 观察者停摆（PR#328 六轮检视 :234，设计见函数头注释）：
                     * 相邻采样间隔远超窗内单次迭代成本 = 本线程自身未被调度
                     * （全局冻结/CPU 限流），停摆期间的墙钟不得计入确认窗 */
                    if (now - c_prev > gap_limit) {
                        if (gap_restarts >= SPINLOCK_ADD_NEON_GAP_RESTARTS_MAX) {
                            observer_stalled = true;
                            break;
                        }
                        gap_restarts++;
                        epoch0 = d->line->epoch;
                        c0 = now;
                        c_prev = now;
                        continue;
                    }
                    c_prev = now;
                    if (now - c0 > confirm_limit)
                        break; /* epoch 停走 + CAS 失败满确认窗 = 确认冻结 */
                    __asm__ volatile("yield");
                }
                if (acquired) {
                    /* 确认窗内获锁：锁健康，超时是本线程被饿/被冻结后恢复
                     * ——告警后正常继续 */
                    log_warning("spinlock_add_neon: cpu %d waited %.1fs for the lock but "
                                "acquired it during the confirm window — scheduling "
                                "starvation or a process freeze/thaw (not a lock "
                                "freeze); continuing",
                                cpu, (double)waited / 1e9);
                    return SPINLOCK_ACQUIRE_OK;
                }
                if (observer_stalled) {
                    *stall_ns_out = spinlock_add_neon_now_ns() - t_entry;
                    return SPINLOCK_ACQUIRE_OBSERVER_STALLED;
                }
                if (flowed) {
                    if (starvation_resets >= SPINLOCK_ADD_NEON_STARVATION_RESETS_MAX) {
                        *stall_ns_out = spinlock_add_neon_now_ns() - t_entry;
                        return SPINLOCK_ACQUIRE_NO_PROGRESS;
                    }
                    starvation_resets++;
                    log_warning("spinlock_add_neon: cpu %d waited %.1fs but the lock line "
                                "kept flowing (epoch advanced) — scheduling starvation, "
                                "not a freeze; wait budget reset (%d/%d)",
                                cpu, (double)waited / 1e9, starvation_resets,
                                SPINLOCK_ADD_NEON_STARVATION_RESETS_MAX);
                    t0 = spinlock_add_neon_now_ns();
                    spins = 0;
                    continue;
                }
                *stall_ns_out = spinlock_add_neon_now_ns() - t_entry;
                return SPINLOCK_ACQUIRE_FROZEN;
            }
        }
        __asm__ volatile("yield");
    }
}

static void spinlock_add_neon_release(struct SpinlockAddNeonData *d)
{
    __atomic_store_n(&d->line->lock_word, 0, __ATOMIC_RELEASE);
}

/* 锁行 64B 全量 dump（五案现场录像格式）；verdict 由两段式判定给出（冻结
 * /进展异常的区分性签名） */
static void spinlock_add_neon_dump_line(struct SpinlockAddNeonData *d, int cpu,
                                        uint64_t stall_ns, const char *verdict)
{
    const volatile uint64_t *q = reinterpret_cast<const volatile uint64_t *>(d->line);
    log_error("spinlock_add_neon: LOCK STALL %.3fs on cpu %d — %s; lock cacheline 64B dump: "
              "lock_word=0x%016lX epoch=%lu holder=%lu magic=0x%016lX "
              "pad=[0x%016lX,0x%016lX,0x%016lX,0x%016lX]%s",
              (double)stall_ns / 1e9, cpu, verdict,
              (unsigned long)q[0], (unsigned long)q[1], (unsigned long)q[2],
              (unsigned long)q[3], (unsigned long)q[4], (unsigned long)q[5],
              (unsigned long)q[6], (unsigned long)q[7],
              q[3] == SPINLOCK_ADD_NEON_MAGIC
                  ? " (magic intact — 8-byte window signature)"
                  : " (MAGIC ALSO CORRUPTED — wider blast radius)");
}

static int spinlock_add_neon_run(struct test *test, int cpu)
{
    auto *d = static_cast<SpinlockAddNeonData *>(test->data);
    const int slot = cpu % SPINLOCK_ADD_NEON_SLOTS;
    const float *in = d->inputs + (size_t)slot * SPINLOCK_ADD_NEON_FLOATS;
    const float *gold = d->golden + (size_t)slot * SPINLOCK_ADD_NEON_FLOATS;
    float out[SPINLOCK_ADD_NEON_FLOATS];
    uint64_t logged = 0, iter = 0;

    do {
        /* A: NEON 实算负载（锁外，本线程私有 out） */
        spinlock_add_neon_kernel(in, out);

        /* B: 获锁——两段式看门狗（通道C：10s 超时 + 1s 确认窗，冻结/饥饿区分） */
        uint64_t stall_ns = 0;
        const enum SpinlockAddNeonAcquire acq = spinlock_add_neon_acquire(d, cpu, &stall_ns);
        if (acq != SPINLOCK_ACQUIRE_OK) {
            const char *verdict;
            if (acq == SPINLOCK_ACQUIRE_FROZEN)
                verdict = "hard-lockup signature CONFIRMED (epoch static across the "
                          "confirm window, CAS failing — lock word frozen at 1 / "
                          "release write lost)";
            else if (acq == SPINLOCK_ACQUIRE_OBSERVER_STALLED)
                verdict = "freeze verdict withheld (this thread could not observe "
                          "continuously — confirm window repeatedly invalidated by "
                          "observer stall gaps)";
            else
                verdict = "progress anomaly (lock line kept flowing but this thread "
                          "never acquired — sustained starvation?)";
            spinlock_add_neon_dump_line(d, cpu, stall_ns, verdict);
            if (acq == SPINLOCK_ACQUIRE_FROZEN)
                report_fail_msg("spinlock_add_neon: lock acquisition stalled %.1fs (>%.0fs) "
                                "on cpu %d, frozen CONFIRMED (lock line static for >=%.0fs, "
                                "CAS failing) — hard-lockup signature",
                                (double)stall_ns / 1e9, SPINLOCK_ADD_NEON_STALL_SECONDS, cpu,
                                SPINLOCK_ADD_NEON_CONFIRM_SECONDS);
            else if (acq == SPINLOCK_ACQUIRE_OBSERVER_STALLED)
                report_fail_msg("spinlock_add_neon: lock acquisition stalled %.1fs on "
                                "cpu %d with >=%d observer stall gaps (>=%.0fms each) "
                                "invalidating the confirm window — cannot confirm a "
                                "hard lockup from interrupted observation (cgroup "
                                "freezer/CRIU checkpoint/VM live-migration pause or "
                                "severe CPU throttling produce this signature)",
                                (double)stall_ns / 1e9, cpu,
                                SPINLOCK_ADD_NEON_GAP_RESTARTS_MAX + 1,
                                SPINLOCK_ADD_NEON_OBSERVER_GAP_SECONDS * 1e3);
            else
                report_fail_msg("spinlock_add_neon: lock line kept flowing but this thread "
                                "failed to acquire across %d wait windows (%.1fs total) on "
                                "cpu %d — sustained starvation or progress anomaly, not a "
                                "lock freeze",
                                SPINLOCK_ADD_NEON_STARVATION_RESETS_MAX + 1,
                                (double)stall_ns / 1e9, cpu);
            return EXIT_FAILURE;
        }

        /* C: 临界区——锁字同行元数据 + 计数对（通道B 靶，同窗更新）。
         * epoch 不用 ++（C++20 弃用 volatile 复合操作）：显式读-改-写。 */
        d->shared_counter[0]++;
        d->local_counts[slot]++;
        d->line->holder = (uint64_t)cpu;
        d->line->epoch = d->line->epoch + 1;

        /* 自检 stall：首个进临界区的线程持锁 15s（> 看门狗 10s + 确认窗 1s
         * + 裕量——模拟冻结须长于两段式全检出时延，否则确认窗内其他线程
         * CAS 成功会被判为饥饿、阳性击穿），其他线程看门狗应触发 */
        if (d->selftest == 1 && d->stall_armed == 0) {
            d->stall_armed = 1;
            sleep(SPINLOCK_ADD_NEON_STALL_SLEEP_SECONDS);
        }

        spinlock_add_neon_release(d);

        /* D: 验证（通道A） */
        const bool ok = memcmp(out, gold, sizeof(out)) == 0;

        /* E: 逐次验证日志（绿 PASS / 红 FAIL；每线程上限，失败行永不受限） */
        if (d->log_fd >= 0 && (ok ? logged < SPINLOCK_ADD_NEON_LOG_MAX : true)) {
            char buf[512];
            int n = snprintf(buf, sizeof buf,
                    "iter=%lu cpu=%d in=[%+.6f,%+.6f,%+.6f,%+.6f] "
                    "out=[%+.6f,%+.6f,%+.6f,%+.6f] result=\033[%dm%s\033[0m\n",
                    (unsigned long)iter, cpu,
                    in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3],
                    ok ? 32 : 31, ok ? "PASS" : "FAIL");
            /* snprintf 截断时返回完整写入所需长度（可 > sizeof buf）——钳制
             * 到 buf 内实际内容，防 write 越界读栈缓冲（护栏防御未来调大
             * 字段/格式串）。buf=512 的依据（PR#328 三轮检视 :237）：健康
             * 输出 |out|≤~3e20 → %+.6f 每元素 ≈29 字符、行长 ~230B；但
             * FAIL 行正是 SDC 场景——位型可落 FLT_MAX 级（%+.6f ≈45 字符/
             * 元素），out 4 元素全落该域行长 ~300B、in+out 8 元素全落（共
             * 享输入行被同击）绝对最坏 457B（本机探针 2026-10-30 实测）。
             * 256 会截断砍掉尾部 result=FAIL 标记、破「绿 PASS 红 FAIL」日
             * 志契约；512 覆盖绝对最坏并留余量——调整格式串时须重算此界 */
            if (n < 0)
                n = 0;
            else if (n > (int)sizeof buf - 1)
                n = (int)sizeof buf - 1;
            if (n > 0 && write(d->log_fd, buf, (size_t)n) < 0) { /* best effort */ }
            if (ok)
                logged++;
        }

        if (!ok) {
            log_error("spinlock_add_neon: data miscompare cpu=%d iter=%lu slot=%d — "
                      "full 64-float in/out/golden dump:",
                      cpu, (unsigned long)iter, slot);
            for (int v = 0; v < SPINLOCK_ADD_NEON_FLOATS; v += 4)
                log_error("  [%02d] in=[%+.6e,%+.6e,%+.6e,%+.6e] out=[%+.6e,%+.6e,%+.6e,%+.6e] "
                          "golden=[%+.6e,%+.6e,%+.6e,%+.6e]",
                          v, in[v], in[v+1], in[v+2], in[v+3],
                          out[v], out[v+1], out[v+2], out[v+3],
                          gold[v], gold[v+1], gold[v+2], gold[v+3]);
            report_fail_msg("spinlock_add_neon: NEON result != golden (cpu %d iter %lu)",
                            cpu, (unsigned long)iter);
            return EXIT_FAILURE;
        }
        iter++;
        /* 循环条件直连 test_time_condition 而非 TEST_LOOP/test_loop_condition
         * ——locks/spinlock 家族统一习语（locks.cpp:68、spinlock_cross_socket.cpp:78
         * 同款 do-while；全仓 248 文件直连 vs 218 文件 TEST_LOOP）。后果须知情
         * （PR#328 六轮检视 :394）：--inject-idle 的 usleep 注入只存在于
         * test_loop_condition（上游 8d510c9c 有意设计："existing tests that
         * directly use test_time_condition() won't be affected by the idle
         * injection"），但时长折算是框架全局行为——本测试在 inject-idle 实验
         * 中为 (1-idle_rate)×时长 × 100% 占空比，与 TEST_LOOP 用例不一致。
         * 不改为 test_loop_condition 的依据：① 该函数每 N 次迭代才被调用一次
         * （TEST_LOOP 粒度），inner_loop_count/骨折切片随之为 ×N（本测试切片
         * 行为——含 logverif 每切片文件与行预算——经二~五轮检视校准，重构
         * 即重开已收敛议题）；② 独行修复无战役收益：直连条件是 248 文件的
         * 框架级语义，占空比一致性须框架层统一解决。此边界在此声明而非
         * 静默存在 */
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int spinlock_add_neon_init(struct test *test)
{
    auto *d = static_cast<SpinlockAddNeonData *>(malloc(sizeof(SpinlockAddNeonData)));
    if (!d)
        return EXIT_FAILURE;
    memset(d, 0, sizeof *d);

    /* knob 读取（init/main 线程，random_access_sweep 纪律：缓存进 test->data，
     * run 线程禁止直接读）；必须用 testspecific 变体——-O testid.key=value
     * 的键按测试 id 定域，NULL-test 裸键查不到 */
    d->logverif = get_testspecific_knob_value_int(test, "logverif", 0) != 0;
    const char *st = get_testspecific_knob_value_string(test, "selftest", "off");
    /* 未知取值 fail loudly 而非静默映射 0（igzip level knob 先例：静默钳
     * 0 会让部署自检纪律被一个 typo 无声击穿——selftest=Stall 之类错值
     * 跑满 -t 时长绿 pass，与阳性自检是否真正执行在时耗上无法区分，
     * PR#328 二轮检视 :266） */
    if (strcmp(st, "off") == 0 || strcmp(st, "0") == 0)
        d->selftest = 0;
    else if (strcmp(st, "stall") == 0 || strcmp(st, "1") == 0)
        d->selftest = 1;
    else if (strcmp(st, "data") == 0 || strcmp(st, "2") == 0)
        d->selftest = 2;
    else {
        log_error("spinlock_add_neon: invalid selftest knob value \"%s\" "
                  "(valid: off|stall|data or 0|1|2, default off) — failing "
                  "loudly so a typo cannot silently skip the deployment "
                  "self-test", st);
        /* 此处仅 d 本身已分配（memset 后各指针为 NULL），就地释放返回；
         * 不能 goto fail——会跨越下方 floats_total 的初始化（C++ 禁止） */
        free(d);
        return EXIT_FAILURE;
    }
    d->stall_armed = 0;
    d->log_fd = -1;

    /* selftest=stall 前置条件 fail loudly（PR#328 四轮检视 :219）：通道 C
     * 阳性需要第二个线程在 10s 看门狗 + 1s 确认窗下等待——单线程时唯一
     * 线程自己持锁睡 15s 后自行 release、循环至 -t 耗尽绿 pass，通道 C
     * 永不触发（假绿，与 selftest 错值静默 off 同一纪律缺口：前置条件
     * 只写在注释里 = 本文件 :288 自己批判的「纪律被静默击穿」形态）；
     * 时长不足时阳性 FAIL(~11s) 与持锁排空(~15s) 落在标称窗口之外，
     * 外部超时包装下证据丢失。线程数取 thread_count()（公开 API，
     * mce_check 先例；fork/slice 子进程 init 前 sApp->thread_count 已
     * 归一为本 slice 实际 worker 数），时长取 sApp->current_test_duration
     * （run_one_test 在 init_internal 前设置，含 -t/每测试覆盖/上下限裁剪
     * 与 --inject-idle 折算后的生效值）。 */
    if (d->selftest == 1) {
        if (thread_count() < 2) {
            log_error("spinlock_add_neon: selftest=stall needs >=2 test threads to "
                      "watch the lock (channel C: a second thread must spin under the "
                      "10s watchdog while the holder sleeps %ds) — got %d thread(s); "
                      "a single thread would sleep, self-release, and green-pass to "
                      "-t with channel C never firing; failing loudly instead of "
                      "producing that false green (use -n >=2 / --cpuset with >=2 "
                      "cpus, or selftest=data which is single-thread safe)",
                      SPINLOCK_ADD_NEON_STALL_SLEEP_SECONDS, thread_count());
            free(d);
            return EXIT_FAILURE;
        }
        if (sApp->current_test_duration
                < std::chrono::milliseconds(SPINLOCK_ADD_NEON_STALL_MIN_TEST_MS)) {
            log_error("spinlock_add_neon: selftest=stall needs a test duration >= "
                      "%dms so the positive FAIL (~%ds: watchdog + confirm window) "
                      "and the holder drain (~%ds) land inside the nominal window — "
                      "got %dms; failing loudly instead of producing a late FAIL "
                      "that external timeout wrappers may kill (recommended: "
                      "-t 25000)",
                      SPINLOCK_ADD_NEON_STALL_MIN_TEST_MS,
                      (int)(SPINLOCK_ADD_NEON_STALL_SECONDS
                            + SPINLOCK_ADD_NEON_CONFIRM_SECONDS),
                      SPINLOCK_ADD_NEON_STALL_SLEEP_SECONDS,
                      (int)sApp->current_test_duration.count());
            free(d);
            return EXIT_FAILURE;
        }
    }

    const size_t floats_total = (size_t)SPINLOCK_ADD_NEON_SLOTS * SPINLOCK_ADD_NEON_FLOATS;
    d->inputs = static_cast<float *>(aligned_alloc(64, floats_total * sizeof(float)));
    d->golden = static_cast<float *>(aligned_alloc(64, floats_total * sizeof(float)));
    d->line = static_cast<SpinlockAddNeonLockLine *>(aligned_alloc(64, sizeof(SpinlockAddNeonLockLine)));
    /* C11 7.22.3.1：aligned_alloc 的 size 须为 alignment 整数倍——
     * sizeof(uint64_t)=8 违反（glibc 不检查故可跑，属潜伏 UB，严格分配器
     * 将返回 NULL 触发假 FAIL）；本行 cacheline 对齐故取 64 */
    d->shared_counter = static_cast<uint64_t *>(aligned_alloc(64, 64));
    d->local_counts = static_cast<uint64_t *>(calloc(SPINLOCK_ADD_NEON_SLOTS, sizeof(uint64_t)));
    if (!d->inputs || !d->golden || !d->line || !d->shared_counter || !d->local_counts) {
        log_error("spinlock_add_neon: allocation failure");
        goto fail;
    }

    /* 输入：init 一次生成，所有线程共享只读（框架 RNG，-s 可复现） */
    for (int s = 0; s < SPINLOCK_ADD_NEON_SLOTS; ++s) {
        float *dst = d->inputs + (size_t)s * SPINLOCK_ADD_NEON_FLOATS;
        for (int i = 0; i < SPINLOCK_ADD_NEON_FLOATS; ++i)
            /* 值域实为 [-1, +1]：double→float 就近舍入使商 ∈ (1-2^-25, 1) 的
             * 64 个整数（x ∈ [2147483584, 2147483647]，含 2^31-64 平局点
             * ties-to-even）产生 +1.0f，概率 2^-26；-1 仅 x=INT32_MIN 精确
             * 取得。本机全量探针实证（正 int32 域逐值扫描，2026-10-31）。
             * golden 同源预计算（同 kernel 同输入）与输出界分析（按 |x|<=1，
             * :35/:78）均按闭区间陈述、不受影响（PR#328 四轮检视 :329）。 */
            dst[i] = (float)((int32_t)random32() / 2147483648.0); /* [-1, +1] */
        /* golden：同源内核预计算（同机字节确定） */
        spinlock_add_neon_kernel(dst, d->golden + (size_t)s * SPINLOCK_ADD_NEON_FLOATS);
    }

    /* 自检 data：毒化每个 slot 的 golden 首元素（cpu 无关——任意 cpuset 布局
     * 都会命中）。位级 XOR 最低尾数位：对任意位型（含 0/±Inf/NaN 及
     * |x|>=2^24 的 ulp>1 值域）无条件改变，memcmp 必检出——+1.0f 在
     * |x|>=2^24 时舍入后位型不变（本机探针：10^6 slot 有 164 个的
     * out[0] 落在该域，0.016%），毒化会静默失效 */
    if (d->selftest == 2)
        for (int s = 0; s < SPINLOCK_ADD_NEON_SLOTS; ++s) {
            float *g = &d->golden[(size_t)s * SPINLOCK_ADD_NEON_FLOATS];
            uint32_t bits;
            memcpy(&bits, g, sizeof bits);
            bits ^= 1;
            memcpy(g, &bits, sizeof bits);
        }

    /* 锁行初始化 */
    d->line->lock_word = 0;
    d->line->epoch = 0;
    d->line->holder = ~0ull;
    d->line->magic = SPINLOCK_ADD_NEON_MAGIC;
    for (int i = 0; i < 4; ++i)
        d->line->pad[i] = SPINLOCK_ADD_NEON_MAGIC ^ (uint64_t)(i + 1);
    d->shared_counter[0] = 0;

    if (d->logverif) {
        char path[128];
        snprintf(path, sizeof(path), "/var/tmp/spinlock_add_neon_%d.log", (int)getpid());
        /* O_CREAT|O_EXCL：/var/tmp 为 1777 粘滞目录、pid 可预测，O_NOFOLLOW
         * 只拦预植符号链接（ELOOP）、拦不住预植普通文件（chmod 666 绕过，
         * 受害进程 O_TRUNC 后 O_APPEND 写入 = 攻击者可实时读/注入伪 PASS
         * 行稀释真 FAIL）——O_EXCL 存在即拒绝（EEXIST）闭合两者：预植
         * symlink 同样 EEXIST（open(2)：O_CREAT|O_EXCL 遇 symlink 失败），
         * O_NOFOLLOW 保留作纵深防御。代价：pid 复用遇上一轮遗留同名日志
         * 也 EEXIST——诊断附属按下方降级路径禁用日志继续跑（log_warning
         * 告知操作员可清理），取证文件不被清空、跨轮追加混淆亦不复存在
         * （原 O_TRUNC 截断方案由此移除：O_EXCL 保证文件必为本轮新建，
         * 截断无对象）。O_APPEND 保留：同轮多线程共享 fd 的原子追加。 */
        d->log_fd = open(path, O_WRONLY | O_CREAT | O_EXCL | O_APPEND | O_NOFOLLOW, 0644);
        if (d->log_fd < 0) {
            /* logverif 是 opt-in 诊断附属、不是检测契约：环境不可用（只读
             * /var/tmp、SELinux 拒绝、O_NOFOLLOW/O_EXCL 命中预植 symlink/
             * 普通文件的 ELOOP/EEXIST，pid 复用或 -f no 调试模式同进程多
             * fracture 遇上一轮遗留同名日志的 EEXIST——后者 rm 旧文件即可
             * 恢复）时降级为禁用日志继续跑——检测通道 A/B/C 不受影响，环境问题
             * 不与 SDC 检出共享退出码、不污染战役自动分诊（cpufreq/thermal
             * 缺失优雅降级同哲学，PR#328 二轮检视 :326）。用 log_warning
             * 而非 log_error：后者会把本用例标 Failed（message_common_check
             * 对 'E' 前缀调 logging_mark_thread_failed）。 */
            log_warning("spinlock_add_neon: cannot open %s (errno %d: %s) — "
                        "per-verification log disabled, test continues "
                        "(logverif is a diagnostic aid, not a detection "
                        "contract)",
                        path, errno, strerror(errno));
            d->log_fd = -1;
        }
    }

    test->data = d;
    return EXIT_SUCCESS;

fail:
    /* init 失败时框架不调用 test_cleanup（sandstone_run.cpp 的 init 失败
     * 分支提前 return）——此处自行释放，失败经返回值传播（ret > 0 →
     * Failed）。不用 report_fail_msg：它经 report_fail_common 异步取消
     * 调用线程且 noreturn，而 test_init/cleanup 都在控制线程执行。 */
    free(d->inputs);
    free(d->golden);
    free(d->line);
    free(d->shared_counter);
    free(d->local_counts);
    free(d);
    return EXIT_FAILURE;
}

static int spinlock_add_neon_cleanup(struct test *test)
{
    auto *d = static_cast<SpinlockAddNeonData *>(test->data);
    int ret = EXIT_SUCCESS;
    if (d) {
        /* 通道B：Σ 逐线程获锁数 == 共享计数器（数据行写丢失检测；
         * add-only 无抵消方向；优雅收尾下计数对同窗更新，无逻辑假阳性） */
        uint64_t total = 0;
        for (int i = 0; i < SPINLOCK_ADD_NEON_SLOTS; ++i)
            total += d->local_counts[i];
        if (total != d->shared_counter[0]) {
            log_error("spinlock_add_neon: shared counter %lu != sum of per-thread "
                      "acquisitions %lu (diff %ld) — lost/frozen update on the "
                      "lock-protected data line",
                      (unsigned long)d->shared_counter[0], (unsigned long)total,
                      (long)d->shared_counter[0] - (long)total);
            /* 失败经返回值传播（框架: cleanup 返回 > EXIT_SUCCESS → Failed）。
             * 不用 report_fail_msg：它经 report_fail_common 异步取消调用
             * 线程且 noreturn，而 cleanup 在控制线程执行——其后的 close/
             * free 与 finish 收尾路径将不可达（memcpy_rewr 先例同样是
             * 先释放后报告；这里直接以返回值收尾，退出码路径完整）。 */
            ret = EXIT_FAILURE;
        }

        /* 通道B 补充：锁行元数据主动对账（PR#328 二轮检视 :357）。epoch 与
         * shared_counter 构造上相等——同一临界区内各增一、持锁单写者、
         * cleanup 前框架已 join 全部 worker（run_threads_in_parallel），
         * quiescent 无撕裂窗口，零假阳性。magic/pad init 后恒定。瞬态一致
         * 性故障只丢锁行元数据写而 lock_word 完好时（非挂死形态：无挂死
         * →通道C 盲、计数对在数据行→通道B 平衡、NEON 数据完好→通道A
         * 通过）此前三通道全绿漏检——本对账闭合该盲区。holder 为最后
         * 持锁者 cpu（值不确定），仍仅作 dump 现场字段。 */
        if (d->line->epoch != d->shared_counter[0]) {
            log_error("spinlock_add_neon: lock-line epoch %lu != shared counter "
                      "%lu (diff %ld) — lost update on the lock cacheline "
                      "metadata (8-byte window, lock_word intact)",
                      (unsigned long)d->line->epoch,
                      (unsigned long)d->shared_counter[0],
                      (long)d->line->epoch - (long)d->shared_counter[0]);
            ret = EXIT_FAILURE;
        }
        if (d->line->magic != SPINLOCK_ADD_NEON_MAGIC) {
            log_error("spinlock_add_neon: lock-line magic 0x%016lX != expected "
                      "0x%016lX — lock cacheline fingerprint corrupted",
                      (unsigned long)d->line->magic,
                      (unsigned long)SPINLOCK_ADD_NEON_MAGIC);
            ret = EXIT_FAILURE;
        }
        for (int i = 0; i < 4; ++i) {
            if (d->line->pad[i] !=
                    (SPINLOCK_ADD_NEON_MAGIC ^ (uint64_t)(i + 1))) {
                log_error("spinlock_add_neon: lock-line pad[%d] 0x%016lX != "
                          "expected 0x%016lX — lock cacheline fingerprint "
                          "corrupted",
                          i, (unsigned long)d->line->pad[i],
                          (unsigned long)(SPINLOCK_ADD_NEON_MAGIC ^ (uint64_t)(i + 1)));
                ret = EXIT_FAILURE;
            }
        }
        if (d->log_fd >= 0)
            close(d->log_fd);
        free(d->inputs);
        free(d->golden);
        free(d->line);
        free(d->shared_counter);
        free(d->local_counts);
        free(d);
        test->data = nullptr;
    }
    return ret;
}

DECLARE_TEST(spinlock_add_neon, "Spinlock liveness + coherence SDC probe: NEON vfma compute "
             "under a global CAS spinlock with a 10s acquisition-stall watchdog (hard-lockup "
             "signature, 64B lock-line dump), shared-counter sum check, and byte-exact golden "
             "verification; per-verification log with green PASS / red FAIL")
    .test_init    = spinlock_add_neon_init,
    .test_run     = spinlock_add_neon_run,
    .test_cleanup = spinlock_add_neon_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

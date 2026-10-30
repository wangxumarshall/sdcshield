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
 *   通道C（锁字挂死，核心增量）：获锁等待 >10s（对齐 sbsa-gwdt）→
 *                        report_fail + dump 锁所在 64B cacheline 全部 8 个 u64
 *                        （对齐五案"8 字节窗口撕裂、同行其余字段完好"的
 *                        现场录像格式；magic 字段对账爆炸半径）
 *
 * 三通道自检（部署到新机器后必跑，coherence_probe SELFTEST_W 同款纪律——
 * 阳性通过后阴性结果才有意义）：
 *   -O spinlock_add_neon.selftest=stall  首个进临界区线程持锁 11s →
 *                                        通道C 应触发（需 ≥2 线程、-t ≥ 25000）
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
 * 失败时追加全 64 float 对照 dump。logverif=0 时完全不触碰 /var/tmp。
 *
 * Deviation note (documented): 内核为自包含 NEON intrinsics（本仓 neon_rot
 * 家族同范式）；第三方库（openblas/sleef）计算负载变体为后续独立单元。
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define SPINLOCK_ADD_NEON_SLOTS 128
#define SPINLOCK_ADD_NEON_FLOATS 64          /* 16 x float32x4 */
#define SPINLOCK_ADD_NEON_ROUNDS 4           /* |x|<=1 → 输出理论上界 ~3e20（<<FLT_MAX；实测 max ~1.4e20） */
#define SPINLOCK_ADD_NEON_STALL_SECONDS 10.0 /* sbsa-gwdt 同阈值 */
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

#if defined(__aarch64__)

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

/* CAS-acquire 自旋（qspinlock 同谱系）；等待超阈值返回 false（看门狗触发） */
static bool spinlock_add_neon_acquire(struct SpinlockAddNeonData *d, uint64_t *stall_ns_out)
{
    const uint64_t limit = (uint64_t)(SPINLOCK_ADD_NEON_STALL_SECONDS * 1e9);
    const uint64_t t0 = spinlock_add_neon_now_ns();
    uint64_t expected = 0;
    for (;;) {
        if (__atomic_compare_exchange_n(&d->line->lock_word, &expected, 1,
                                        false, __ATOMIC_ACQUIRE, __ATOMIC_RELAXED))
            return true;
        expected = 0;
        const uint64_t waited = spinlock_add_neon_now_ns() - t0;
        if (waited > limit) {
            *stall_ns_out = waited;
            return false;
        }
        __asm__ volatile("yield");
    }
}

static void spinlock_add_neon_release(struct SpinlockAddNeonData *d)
{
    __atomic_store_n(&d->line->lock_word, 0, __ATOMIC_RELEASE);
}

/* 锁行 64B 全量 dump（五案现场录像格式） */
static void spinlock_add_neon_dump_line(struct SpinlockAddNeonData *d, int cpu, uint64_t stall_ns)
{
    const volatile uint64_t *q = reinterpret_cast<const volatile uint64_t *>(d->line);
    log_error("spinlock_add_neon: LOCK STALL %.3fs on cpu %d — hard-lockup signature "
              "(lock word frozen at 1 / release write lost); lock cacheline 64B dump: "
              "lock_word=0x%016lX epoch=%lu holder=%lu magic=0x%016lX "
              "pad=[0x%016lX,0x%016lX,0x%016lX,0x%016lX]%s",
              (double)stall_ns / 1e9, cpu,
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

        /* B: 获锁——等待计时 + 10s 看门狗（通道C） */
        uint64_t stall_ns = 0;
        if (!spinlock_add_neon_acquire(d, &stall_ns)) {
            spinlock_add_neon_dump_line(d, cpu, stall_ns);
            report_fail_msg("spinlock_add_neon: lock acquisition stalled %.1fs (>%.0fs) "
                            "on cpu %d — hard-lockup signature",
                            (double)stall_ns / 1e9, SPINLOCK_ADD_NEON_STALL_SECONDS, cpu);
            return EXIT_FAILURE;
        }

        /* C: 临界区——锁字同行元数据 + 计数对（通道B 靶，同窗更新）。
         * epoch 不用 ++（C++20 弃用 volatile 复合操作）：显式读-改-写。 */
        d->shared_counter[0]++;
        d->local_counts[slot]++;
        d->line->holder = (uint64_t)cpu;
        d->line->epoch = d->line->epoch + 1;

        /* 自检 stall：首个进临界区的线程持锁 11s，其他线程看门狗应触发 */
        if (d->selftest == 1 && d->stall_armed == 0) {
            d->stall_armed = 1;
            sleep(11);
        }

        spinlock_add_neon_release(d);

        /* D: 验证（通道A） */
        const bool ok = memcmp(out, gold, sizeof(out)) == 0;

        /* E: 逐次验证日志（绿 PASS / 红 FAIL；每线程上限，失败行永不受限） */
        if (d->log_fd >= 0 && (ok ? logged < SPINLOCK_ADD_NEON_LOG_MAX : true)) {
            char buf[256];
            const int n = snprintf(buf, sizeof buf,
                    "iter=%lu cpu=%d in=[%+.6f,%+.6f,%+.6f,%+.6f] "
                    "out=[%+.6f,%+.6f,%+.6f,%+.6f] result=\033[%dm%s\033[0m\n",
                    (unsigned long)iter, cpu,
                    in[0], in[1], in[2], in[3], out[0], out[1], out[2], out[3],
                    ok ? 32 : 31, ok ? "PASS" : "FAIL");
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
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

#else /* !__aarch64__ */

static int spinlock_add_neon_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 NEON required for spinlock_add_neon");
    return EXIT_SKIP;
}

#endif

static int spinlock_add_neon_init(struct test *test)
{
#if !defined(__aarch64__)
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 NEON required for spinlock_add_neon");
    return EXIT_SKIP;
#else
    auto *d = static_cast<SpinlockAddNeonData *>(malloc(sizeof(SpinlockAddNeonData)));
    if (!d)
        return EXIT_FAILURE;
    memset(d, 0, sizeof *d);

    /* knob 读取（init/main 线程，random_access_sweep 纪律：缓存进 test->data，
     * run 线程禁止直接读）；必须用 testspecific 变体——-O testid.key=value
     * 的键按测试 id 定域，NULL-test 裸键查不到 */
    d->logverif = get_testspecific_knob_value_int(test, "logverif", 0) != 0;
    const char *st = get_testspecific_knob_value_string(test, "selftest", "off");
    if (strcmp(st, "stall") == 0 || strcmp(st, "1") == 0)
        d->selftest = 1;
    else if (strcmp(st, "data") == 0 || strcmp(st, "2") == 0)
        d->selftest = 2;
    else
        d->selftest = 0;
    d->stall_armed = 0;
    d->log_fd = -1;

    const size_t floats_total = (size_t)SPINLOCK_ADD_NEON_SLOTS * SPINLOCK_ADD_NEON_FLOATS;
    d->inputs = static_cast<float *>(aligned_alloc(64, floats_total * sizeof(float)));
    d->golden = static_cast<float *>(aligned_alloc(64, floats_total * sizeof(float)));
    d->line = static_cast<SpinlockAddNeonLockLine *>(aligned_alloc(64, sizeof(SpinlockAddNeonLockLine)));
    d->shared_counter = static_cast<uint64_t *>(aligned_alloc(64, sizeof(uint64_t)));
    d->local_counts = static_cast<uint64_t *>(calloc(SPINLOCK_ADD_NEON_SLOTS, sizeof(uint64_t)));
    if (!d->inputs || !d->golden || !d->line || !d->shared_counter || !d->local_counts) {
        log_error("spinlock_add_neon: allocation failure");
        goto fail;
    }

    /* 输入：init 一次生成，所有线程共享只读（框架 RNG，-s 可复现） */
    for (int s = 0; s < SPINLOCK_ADD_NEON_SLOTS; ++s) {
        float *dst = d->inputs + (size_t)s * SPINLOCK_ADD_NEON_FLOATS;
        for (int i = 0; i < SPINLOCK_ADD_NEON_FLOATS; ++i)
            dst[i] = (float)((int32_t)random32() / 2147483648.0); /* [-1, 1) */
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
        d->log_fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
        if (d->log_fd < 0) {
            log_error("spinlock_add_neon: cannot open %s (errno %d)", path, errno);
            goto fail;
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
#endif
}

static int spinlock_add_neon_cleanup(struct test *test)
{
    auto *d = static_cast<SpinlockAddNeonData *>(test->data);
    int ret = EXIT_SUCCESS;
    if (d) {
#if defined(__aarch64__)
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
#endif
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

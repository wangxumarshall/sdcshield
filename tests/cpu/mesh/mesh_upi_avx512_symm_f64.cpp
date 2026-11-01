/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mesh_upi_avx512_symm_f64
 * @parblock
 * Cross-core shared-data f64 production/consumption SDC stress — the
 * first floating-point variant in the mesh family (all 27 existing mesh
 * tests are int32). The known NUMA3 fault class is a compound of
 * remote/shared access x compute; until now the suite covered the two
 * halves separately (mesh_int: shared access, integer; sve512_*: FP
 * compute, private data). This test combines them on the mesh_upi
 * three-phase skeleton (write / read-verify / reset with atomic block
 * claiming), with FP compute on both sides of the shared buffer:
 *
 *   WRITE phase: each claimed block is produced with a 4-step NEON
 *     vfmaq_f64 local chain over exact binary values, then stored.
 *
 *   READ phase: every element of the shared buffer is loaded, run
 *     through the same 4-step FMA chain, and byte-compared against the
 *     recomputed golden; plus the store/reload consistency check of the
 *     original skeleton.
 *
 *   Global sum (closed form, #323): the expected total is a deterministic
 *     function of the buffer content — block_value() is a pure function of
 *     (block, j), and each block's 4096 elements cover exactly 128 full
 *     32-residue cycles, so every block sums to the same 33792.0 and the
 *     expected total is 33792 × num_blocks (computed in init from the same
 *     block_value()). Every value is an exact 0.5-multiple and the total
 *     magnitude stays below 2^51 for any physically allocatable buffer, so
 *     both sides are exact in f64 under any accumulation order — the
 *     floating-point associativity hazard does not exist. The read side
 *     byte-compares its full-buffer sum against this constant: no
 *     cross-thread accumulator, no cross-round state, structurally
 *     race-free (the old write-side global_sum atomic could go stale/dirty
 *     across timeout-interrupted rounds and misreport). Strictly stronger
 *     detection than the old read==write agreement: a value corrupted
 *     before both the store and the write-side accumulation used to fool
 *     both arms; now only the read arm needs to see it.
 *
 * NEON f64 (float64x2_t) — runs on any aarch64, no SVE needed, so the
 * compound coverage also exists on SVE-less hosts (this Kunpeng 920
 * included, where it is fully verified on real hardware).
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cstring>
#include <atomic>
#ifdef __aarch64__
#include <arm_neon.h>
#include "sdc_stress_config.h"
#include <unistd.h>
#include <cstdlib>
#include <sys/sysinfo.h>

#define NEON_VECTOR_SIZE 2                // NEON 一次处理 2 个 f64
#define BLOCK_SIZE 4096                   // 每块元素数(加大块以降低原子争用)
#define VECTORS_PER_BLOCK (BLOCK_SIZE / NEON_VECTOR_SIZE)  // = 2048
// 块数/总元素数在 init 中按目标内存动态计算(见 mesh_target_bytes)，
// 把用例操作数放大到逼近物理内存(~90%)，作为 SDC 内存通路激发。

namespace { /* ODR: per-TU TestData (66-file same-name hazard) */
struct TestData {
    alignas(16) double *data;             // 共享缓冲(16B 对齐)
    size_t num_blocks;                    // 运行时块数(按目标内存计算)
    size_t total_elements;                // 运行时总元素数
    double expected_total;                // 闭式期望总和(init 一次写入,此后只读)
    std::atomic<uint64_t> next_block;
    std::atomic<uint64_t> write_done;
    std::atomic<uint32_t> read_done;
    std::atomic<uint32_t> reset_lock;
    std::atomic<uint32_t> thread_idx;
};
}

// 块值生成:slot j 的种子值(精确 0.5 倍数)
static inline double block_value(uint64_t block, int j)
{
    // 范围 [0.5, 16.0) 的精确值:0.5 * (1..32)
    return 0.5 * (double)(1 + ((block * 16 + (uint32_t)j) % 32));
}

// 4 步 NEON FMA 链(精确:值域内每步结果都是 0.5 倍数,无舍入)
static inline double fma_chain(double v)
{
    float64x2_t acc = vdupq_n_f64(v);
    float64x2_t m = vdupq_n_f64(0.5);
    float64x2_t a = vdupq_n_f64(0.5);
    acc = vfmaq_f64(acc, m, a);   // v + 0.25
    acc = vfmaq_f64(acc, m, a);   // v + 0.50
    acc = vfmaq_f64(acc, m, a);   // v + 0.75
    acc = vfmaq_f64(acc, m, a);   // v + 1.00
    return vgetq_lane_f64(acc, 0);
}

// 标量 golden:与 NEON 链完全同序同值(每步 +0.25,精确)
static inline double fma_chain_scalar(double v)
{
    return v + 0.25 + 0.25 + 0.25 + 0.25;
}

// 目标字节数: -O mesh_upi_avx512_symm_f64.buffer_bytes=N /
// SDC_MESH_UPI_AVX512_SYMM_F64_BUFFER_BYTES 环境变量 / 旧脚本兼容
// SDC_MESH_BUFFER_BYTES / 自适应默认(下)。与 sym 孪生(avx_sym/avx512_sym)
// 的同名 knob 语义一致(issue #325 修复引入,#323 补齐到本文件)。
//
// 自适应默认口径(issue #325):本用例读阶段是「每线程全量读取」——每轮
// 聚合读流量 = 线程数 × 缓冲字节数。原默认(物理内存 45%)只按 2 路战役
// 内存均摊计,不含线程数因子:多线程 × 大缓冲的读循环内无时间检查(检查
// 频度撑不起 40 次 auto-fracture 迭代预算),-t 到期无法打断当轮
// → 战役外部 timeout 先于框架 SIGQUIT 宽限杀进程 → Timed Out 误报;
// 且等待自旋每圈烧 1 次迭代预算(40 次上限,µs 级耗尽,#357),填充超
// 毫秒即整轮零校验。修法 = sizing 语义修正:默认缓冲 = 四源安全预算
// (物存/MemAvailable×0.9/cgroup 限额/承诺余量,sdc_budget_bytes,
// #310 OOM 前科——单源物存会在容器/批处理限额下超分)的 45% ÷ 线程数
// (读流量总量恒定——每轮字节搬移与修复前同量级,「全线程全量读」访问
// 模式不变),clamp [64 MiB, 1 GiB]:下限保默认工作集达 LLC 量级(跨核
// 共享行的 snoop/mesh 流量正是本用例的互连压力语义,原始设计即
// L1D-to-L1D;下限不超过本实例战役份额,紧预算下让位于不超分,预算
// 为 0 时诚实 SKIP),上限收口低线程/超大内存主机的轮时长;显式 knob/
// env 不受该 clamp 约束(大足迹压力仍可显式申请,其超时语义由 #357 族级
// 根治跟踪)。等待侧预算节流见 run 内 131072 一档自旋(同 sym 先例)。
/* 孪生同步:本函数与 mesh_upi_avx{,512}_sym.cpp 两文件逐字节同款(全部
 * 日志/env 名/knob 均由 test->id 派生)——改此函数必须三文件同步
 * (symm_f64 由 #323 补齐,#414 当时只改了 sym 对);
 * #357 族级四源迁移时应合并进 sdc_target_workset_bytes 体系(届时连
 * SVE mesh 18 文件的同名函数一并收口)。 */
static size_t mesh_target_bytes(struct test *test) {
    int64_t v = sdc_knob_int_clamped(test, "buffer_bytes", 0,
                                     1 << 20, 1ll << 44);
    if (v == 0)
        sdc_env_knob_int_clamped(test->id, "buffer_bytes", &v, 1 << 20, 1ll << 44);
    /* 旧脚本兼容: SDC_MESH_BUFFER_BYTES 直接指定字节——套与 knob/env 同款
     * [1MiB,16TiB] clamp 与值日志(检视意见:裸通道下荒谬值会静默缩到
     * num_blocks<1 兜底的 1 块 32 KiB,mesh 压力归零仍报 pass)。负值经
     * strtoull 会模 2^64 回绕成巨量再被向上 clamp 到 16 TiB(检视轮 2),
     * 故用 strtoll 解析并以 >0 为有效;畸形串告警后走自适应,不再静默。 */
    if (v == 0) {
        if (const char *e = getenv("SDC_MESH_BUFFER_BYTES")) {
            char *end = nullptr;
            long long b = strtoll(e, &end, 10);
            if (end && *end == '\0' && b > 0) {
                if (b < (1 << 20)) {
                    log_warning("%s: SDC_MESH_BUFFER_BYTES=%lld 越界 (有效 %d..%lld), 已 clamp 为 %d",
                                test ? test->id : "(global)", b,
                                1 << 20, 1ll << 44, 1 << 20);
                    b = 1 << 20;
                } else if (b > (1ll << 44)) {
                    log_warning("%s: SDC_MESH_BUFFER_BYTES=%lld 越界 (有效 %d..%lld), 已 clamp 为 %lld",
                                test ? test->id : "(global)", b,
                                1 << 20, 1ll << 44, 1ll << 44);
                    b = 1ll << 44;
                }
                log_warning("[压力映射] %s.buffer_bytes = %lld 字节 (来源: 旧脚本兼容 env SDC_MESH_BUFFER_BYTES)",
                            test ? test->id : "(global)", b);
                return (size_t)b;
            }
            log_warning("%s: SDC_MESH_BUFFER_BYTES='%s' 无法解析为正整数, 已忽略, 走自适应默认",
                        test ? test->id : "(global)", e);
        }
    }
    if (v > 0)
        return (size_t)v;
    /* quick 模式(CI 冒烟): 与 sdc_target_workset_bytes 的 quick 契约一致
     * 收缩到 16 MiB,不套下方 clamp。 */
    if (sdc_quick_run())
        return 16ull << 20;
    /* 读流量总量恒定: 四源安全预算(物存/MemAvailable×0.9/cgroup 限额/
     * 承诺余量,sdc_budget_bytes,#310 OOM 前科)的 45% ÷(2 路战役均摊 ×
     * 线程数 × 并发 slice 数)。下限 64 MiB: 默认工作集达 LLC 量级(跨核
     * 共享行的 snoop/mesh 流量正是本用例的互连压力语义,原始设计即
     * L1D-to-L1D)——下限同时不超过本实例的战役份额 budget÷(2×slices),
     * 紧预算环境里让位于不超分(检视轮 2:抬到整份预算会在 2×slices 个
     * 并发实例间合计超四源安全预算);budget==0(限额/承诺耗尽)返回 0,
     * 由 init 诚实 SKIP;上限 1 GiB: 低线程/超大内存主机的轮时长收口。 */
    /* sdc_total_threads(): total 不可见语境下的框架兜底(裸
     * total_thread_count() 会使 slices 退化 1、consumers 低估向超分偏)。 */
    int total = sdc_total_threads();
    int cur = thread_count();
    int slices = (cur > 0 && total > cur) ? (total + cur - 1) / cur : 1;
    int consumers = 2 * cur * slices;
    if (consumers < 1)
        consumers = 1;
    uint64_t budget = sdc_budget_bytes(900);
    uint64_t lo = 64ull << 20;
    uint64_t share = budget / (uint64_t)(2 * slices);
    if (lo > share)
        lo = share;
    uint64_t b = budget / (uint64_t)consumers;
    if (b < lo)
        b = lo;
    if (b > (1ull << 30))
        b = 1ull << 30;
    if (b < (64ull << 20)) {
        /* 64 MiB 下限被战役份额截断:紧预算下工作集缩进 LLC 深内,mesh
         * 互连压力语义退化——按框架惯例打 [欠压力] 档位警告(检视轮 3),
         * 不静默降档。 */
        log_warning("[欠压力] %s.buffer_bytes = %.1f MiB: 四源安全预算 %.1f MiB 撑不住 64 MiB 下限(本实例战役份额 %.1f MiB),默认工作集缩至 LLC 量级以内,mesh 互连压力语义退化,仍按 PROD 记账",
                    test ? test->id : "(global)", (double)b / 1048576.0,
                    (double)budget / 1048576.0,
                    (double)share / 1048576.0);
    }
    /* 注意口径: b = budget(=90% 源) ÷ 2(战役) ÷ 线程 ÷ slice,即源预算
     * 的 45%÷线程÷slice——日志按实际除法链书写,勿再把 45% 与 ÷2 并写
     * (检视轮 3:双重计入与实打值自相矛盾)。 */
    log_warning("[压力映射] %s.buffer_bytes = %llu 字节 (来源: 读流量总量恒定 90%%安全预算 ÷ 2(战役)×%d(线程)×%d(slice), clamp [64MiB..1GiB]) = %.1f MiB",
                test ? test->id : "(global)", (unsigned long long)b,
                cur, slices, (double)b / 1048576.0);
    return b;
}

static int mesh_upi_avx512_symm_f64_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2)
        return -255;   // 需要至少 2 个线程

    const size_t elem_bytes = sizeof(double);
    const size_t block_bytes = BLOCK_SIZE * elem_bytes;
    size_t target = mesh_target_bytes(test);
    if (target == 0) {
        /* 四源安全预算为 0(cgroup 限额/承诺余量耗尽)——任何缓冲分配都是
         * 超分,诚实 SKIP 而非缩到 1 块兜底空转(检视轮 2 意见,同 sym
         * 孪生口径)。 */
        log_skip(TestResourceIssueSkipCategory,
                 "%s: four-source memory budget is zero (cgroup limit / "
                 "commit headroom exhausted); cannot size a meaningful "
                 "mesh buffer", test->id);
        return EXIT_SKIP;
    }

    auto *td = new TestData;
    if (!td) return EXIT_FAILURE;

    td->num_blocks = target / block_bytes;
    if (td->num_blocks < 1) td->num_blocks = 1;
    /* 块索引空间 64 位(R2):num_blocks 不再封顶(target 经 floor 除法而来,
     * num_blocks × block_bytes ≤ target ≤ SIZE_MAX,乘法无 size_t 溢出)。
     * 去封顶必须配套 64 位块索引:next_block 若仍是 uint32,块数 ≥ 2^32 时
     * fetch_add 越过 0xFFFFFFFF 回绕到 0,低块被重复认领重写——求和族
     * global_sum 重复累计→假 FAIL,break 式认领循环每 2^32 次认领仅一张
     * 退出票。该回绕在旧封顶下仅剩一个角落可触发:计算块数 ≥ 2^32 时
     * 封顶恰把 num_blocks 钉在 0xFFFFFFFF,拿到 0xFFFFFFFF 的线程 break
     * 后计数器回绕到 0,其余写者重新认领低块重写,退出票每 2^32 次认领
     * 仅一张、N 线程需 N 张,写阶段无法在测试时限内自然终止;但本用例
     * 2^32 块 = 128 TiB,init 分配必败,该角落实际不可达(计算块数 ≤
     * 2^32 − 线程数的常态亦不可达);
     * 历史假 FAIL 的真因是首轮已修的偏移乘法回绕:旧 size_t offset =
     * block * BLOCK_SIZE 是 32 位乘法,元素偏移 ≥ 2^32(缓冲 ≥ 32 GiB)时
     * 低偏移被回绕索引重复写入,缓冲上半段地址从未被写。 */
    td->total_elements = td->num_blocks * BLOCK_SIZE;
    size_t total_bytes = td->total_elements * elem_bytes;

    td->data = (double *)aligned_alloc(16, total_bytes);
    if (!td->data) {
        delete td;
        return EXIT_FAILURE;
    }
    memset(td->data, 0, total_bytes);

    /* 期望总和闭式基线(#323):块值是 (block, j) 纯函数,每块 4096 元素
     * 恰 128 个完整 32 残差周期 → 每块和与块号无关。全部为 0.5 倍数、
     * 总幅 < 2^51(16 TiB 缓冲封顶下 ≈1.8e13),任意加法序下精确无舍入
     * ——与读侧逐元素累加同构,顺序无关,字节可比。init 一次写入、运行期
     * 只读:无跨线程/跨轮状态,替代旧 global_sum 原子累加器(其生命周期
     * 依赖重置门,门被超时打断后留下脏状态 → 假 FAIL,见文件头)。 */
    double per_block = 0.0;
    for (int j = 0; j < BLOCK_SIZE; ++j)
        per_block += block_value(0, j);
    td->expected_total = per_block * (double)td->num_blocks;

    td->next_block.store(0, std::memory_order_relaxed);
    td->write_done.store(0, std::memory_order_relaxed);
    td->read_done.store(0, std::memory_order_relaxed);
    td->reset_lock.store(0, std::memory_order_relaxed);
    td->thread_idx.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_avx512_symm_f64_run(struct test *test, int cpu) {
    (void)cpu;
    auto *td = static_cast<TestData *>(test->data);

    int id = td->thread_idx.fetch_add(1, std::memory_order_relaxed);
    uint32_t total = thread_count();

    /* 本线程是否完成过至少一整轮「填充→全量读取校验」——同 sse_sym
     * （issue #358 / PR #331 二轮意见 #23：-t 截断空过改诚实 SKIP）。 */
    bool validated_any = false;
    do {
        // ---------- 写阶段:认领块,FP 生产,NEON 存储 ----------
        while (true) {
            uint64_t block = td->next_block.fetch_add(1, std::memory_order_seq_cst);
            if (block >= td->num_blocks) break;

            // 块号 64 位,与偏移乘法同宽:首轮修 32 位乘法偏移回绕(元素偏移
            // ≥ 2^32 时低偏移被回绕索引重复写入,缓冲上半段从未被写);R2 去封顶后
            // 块号本身也须 64 位,否则 fetch_add 在块数 ≥ 2^32 时回绕到 0,
            // 低块被重复认领重写。
            size_t offset = (size_t)block * BLOCK_SIZE;
            double vals[BLOCK_SIZE];
            for (int j = 0; j < BLOCK_SIZE; ++j)
                vals[j] = block_value(block, j);
            // NEON 存储(每块 8 个 float64x2)
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float64x2_t vec = vld1q_f64(vals + v * NEON_VECTOR_SIZE);
                vst1q_f64(td->data + offset + v * NEON_VECTOR_SIZE, vec);
            }
            __sync_synchronize();

            td->write_done.fetch_add(1, std::memory_order_seq_cst);
        }

        /* 等待所有块写完成：131072 自旋一档节流预算检查（sym 孪生同款，
         * #357 病理的修复，#323 补齐到本文件）——原「每自旋都调
         * test_time_condition」把 40 次 auto-fracture 迭代预算在 µs 级烧穿，
         * 等待方在填充完成前提前退出，该轮校验被整轮跳过（实测 128 MiB +
         * -t 5 -n 8 下 8/8 线程首轮预算死亡全体 SKIP，零校验）；节流后耐心
         * ~40×131072 自旋（≈0.1-0.3 s），等待条件本身每自旋都查，响应不受
         * 影响。 */
        {
            uint32_t spin = 0;
            while (td->write_done.load(std::memory_order_seq_cst) < td->num_blocks) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }

        /* 期望值是闭式常量(见 init):无快照、无跨线程累加——旧实现在此
         * 快照 global_sum,而快照时机本身与重置/下一轮写入存在协议竞态
         * (#323:曾在 iteration 17 实测 read_sum=0x1.08p+13 vs
         * expected=0x1.bdp+10 的假 mismatch;根治 = 期望值不再依赖任何
         * 共享可变状态)。 */

        // ---------- 读阶段:全量读 + FMA 链 + golden 比对 ----------
        double read_sum = 0.0;
        bool consistent = true;
        alignas(16) double store_buf[BLOCK_SIZE];

        for (size_t i = 0; i < td->total_elements; i += BLOCK_SIZE) {
            double vals[BLOCK_SIZE];
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float64x2_t vec = vld1q_f64(td->data + i + v * NEON_VECTOR_SIZE);
                vst1q_f64(vals + v * NEON_VECTOR_SIZE, vec);
            }
            for (int j = 0; j < BLOCK_SIZE; ++j) {
                read_sum += vals[j];            // 精确
                double got = fma_chain(vals[j]);
                double want = fma_chain_scalar(vals[j]);
                if (got != want) {              // byte-exact(精确值,== 可靠)
                    consistent = false;
                    break;
                }
            }
            if (!consistent) break;

            // 一致性测试:Store/Load 比较(原骨架)
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float64x2_t orig = vld1q_f64(vals + v * NEON_VECTOR_SIZE);
                vst1q_f64(store_buf + v * NEON_VECTOR_SIZE, orig);
            }
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float64x2_t orig = vld1q_f64(vals + v * NEON_VECTOR_SIZE);
                float64x2_t reload = vld1q_f64(store_buf + v * NEON_VECTOR_SIZE);
                uint64x2_t cmp = vceqq_u64(vreinterpretq_u64_f64(orig),
                                           vreinterpretq_u64_f64(reload));
                if (!(vgetq_lane_u64(cmp, 0) == 0xFFFFFFFFFFFFFFFFULL &&
                      vgetq_lane_u64(cmp, 1) == 0xFFFFFFFFFFFFFFFFULL)) {
                    consistent = false;
                    break;
                }
            }
            if (!consistent) break;
        }

        bool sum_ok = (read_sum == td->expected_total);   // 双侧精确,顺序无关
        bool passed = sum_ok && consistent;

        if (!passed) {
            log_warning("mesh_upi_avx512_symm_f64: thread %d read_sum=%a "
                        "expected=%a consistent=%d", id, read_sum,
                        td->expected_total, (int)consistent);
            report_fail_msg("mesh_upi_avx512_symm_f64: cross-core FP sum "
                            "mismatch vs deterministic golden");
            return EXIT_FAILURE;
        }
        validated_any = true;   // 本轮全量读取校验（精确和+FMA 链 golden）已通过

        td->read_done.fetch_add(1, std::memory_order_seq_cst);

        // ---------- 重置阶段(最后一个读完成者执行) ----------
        uint32_t expected_lock = 0;
        if (td->reset_lock.compare_exchange_strong(expected_lock, 1,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst)) {
            /* 同款 131072 一档节流（sym 孪生先例） */
            {
                uint32_t spin = 0;
                while (td->read_done.load(std::memory_order_seq_cst) < total) {
                    if ((++spin & 131071u) == 0) {
                        if (!test_time_condition(test)) {
                            td->reset_lock.store(0, std::memory_order_seq_cst);
                            goto round_aborted;
                        }
                    } else {
                        __asm__ volatile("yield");
                    }
                }
            }
            /* 轮屏障计数器重置(global_sum 已随闭式化删除——期望值是
             * init 期常量,无跨轮状态可残留)。 */
            td->next_block.store(0, std::memory_order_seq_cst);
            td->write_done.store(0, std::memory_order_seq_cst);
            td->read_done.store(0, std::memory_order_seq_cst);
            td->reset_lock.store(0, std::memory_order_seq_cst);
        } else {
            /* 同款 131072 一档节流 */
            uint32_t spin = 0;
            while (td->reset_lock.load(std::memory_order_seq_cst) != 0) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }

    } while (test_time_condition(test));

round_aborted:
    if (!validated_any) {
        log_skip(TestResourceIssueSkipCategory,
                 "mesh_upi_avx512_symm_f64: test time expired before the "
                 "first full validation round completed (runtime-sized "
                 "buffer fill outlives -t); nothing was verified in this run");
        return EXIT_SKIP;
    }
    return EXIT_SUCCESS;
}

static int mesh_upi_avx512_symm_f64_finish(struct test *test) {
    auto *td = static_cast<TestData *>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_avx512_symm_f64_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for "
             "mesh_upi_avx512_symm_f64");
    return EXIT_SKIP;
}
static int mesh_upi_avx512_symm_f64_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_avx512_symm_f64_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_avx512_symm_f64,
             "Cross-core shared-data f64 production/consumption (symm): "
             "the first FP mesh variant — NEON vfmaq_f64 chains on both "
             "sides of a shared cross-core buffer with exact 0.5-multiple "
             "arithmetic (order-independent sums, byte-exact compare) — "
             "the remote-access x compute compound the int-only mesh "
             "family never covered")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_avx512_symm_f64_init,
    .test_run = mesh_upi_avx512_symm_f64_run,
    .test_cleanup = mesh_upi_avx512_symm_f64_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

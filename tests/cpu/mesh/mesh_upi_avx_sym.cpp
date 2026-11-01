#include <sandstone.h>
#include "sdc_stress_config.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#ifdef __aarch64__
#include <arm_neon.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/sysinfo.h>

#define VECTOR_SIZE 4                     // NEON 128 位可容纳 4 个 float
#define BLOCK_ELEMENTS 8192               // 每块元素数(加大块以降低原子争用)
#define VECTORS_PER_BLOCK (BLOCK_ELEMENTS / VECTOR_SIZE)  // = 2048
// 块数/总元素数在 init 中按目标内存动态计算(见 mesh_target_bytes)。

namespace { /* ODR: per-TU TestData (66-file same-name hazard) */
struct TestData {
    alignas(16) float *data;             // 共享数据数组，16 字节对齐
    size_t num_blocks;                   // 运行时块数(按目标内存计算)
    size_t total_elements;               // 运行时总元素数
    std::atomic<uint64_t> next_block;
    std::atomic<double> global_sum;      // 用 double 累加，减少误差
    std::atomic<uint64_t> allocated_blocks;
    std::atomic<uint32_t> thread_idx;
    /* sense-reversal barrier (G2'/G3' fix): the original iter/read_done/
     * round_done protocol deadlocked from round 1 (read_done was never
     * set to 1; iter only advanced after a completed round), so the sum
     * check never executed and the test passed vacuously. Pre-existing
     * since the 098d4aa port. */
    std::atomic<uint32_t> arrived;
    std::atomic<uint32_t> epoch;
};
}

// 目标字节数: -O mesh_upi_avx_sym.buffer_bytes=N /
// SDC_MESH_UPI_AVX_SYM_BUFFER_BYTES 环境变量 / 旧脚本兼容
// SDC_MESH_BUFFER_BYTES / 自适应默认(下)。与 SVE 侧 mesh_upi_sve_sym
// 的同名 knob 语义一致(issue #325 修复引入)。
//
// 自适应默认口径(issue #325):本用例读阶段是「每线程全量读取」——每轮
// 聚合读流量 = 线程数 × 缓冲字节数。原默认(物理内存 45%,9b7ee76c
// knob-ize 引入)只按 2 路战役内存均摊计,不含线程数因子:128 线程 ×
// 57.6 GiB(128 GiB 主机)= 7372 GiB/轮 ≈ 74 s,读循环内无时间检查
// (检查频度撑不起 40 次 auto-fracture 迭代预算),-t 到期无法打断当轮
// → 战役外部 timeout(scheduler.py: timeout -k 30 <-t/1000+15>)先于
// 框架 SIGQUIT 宽限 max(5×-t+30s, 300s) 杀进程 → Timed Out 误报;
// 且等待自旋每圈烧 1 次迭代预算(40 次上限,µs 级耗尽,#357),填充超
// 毫秒即整轮零校验。修法 = sizing 语义修正:默认缓冲 = 四源安全预算
// (物存/MemAvailable×0.9/cgroup 限额/承诺余量,sdc_budget_bytes,
// #310 OOM 前科——单源物存会在容器/批处理限额下超分)的 45% ÷ 线程数
// (读流量总量恒定——每轮字节搬移与修复前同量级,「全线程全量读」访问
// 模式不变),clamp [64 MiB, 1 GiB]:下限保默认工作集达 LLC 量级(跨核
// 共享行的 snoop/mesh 流量正是本用例的互连压力语义,原始设计即
// L1D-to-L1D;下限让位于预算本身,不越过 OOM 安全边界),上限收口低
// 线程/超大内存主机的轮时长;显式 knob/env 不受该 clamp 约束(大足迹
// 压力仍可显式申请,其超时语义由 #357 族级根治跟踪)。等待侧预算节流
// 见 run 内 131072 一档自旋(同 SVE 侧先例)。
static size_t mesh_target_bytes(struct test *test) {
    int64_t v = sdc_knob_int_clamped(test, "buffer_bytes", 0,
                                     1 << 20, 1ll << 44);
    if (v == 0)
        sdc_env_knob_int_clamped(test->id, "buffer_bytes", &v, 1 << 20, 1ll << 44);
    /* 旧脚本兼容: SDC_MESH_BUFFER_BYTES 直接指定字节——套与 knob/env 同款
     * [1MiB,16TiB] clamp 与值日志(检视意见:裸通道下荒谬值会静默缩到
     * num_blocks<1 兜底的 1 块 32 KiB,mesh 压力归零仍报 pass)。 */
    if (v == 0) {
        if (const char *e = getenv("SDC_MESH_BUFFER_BYTES")) {
            char *end = nullptr;
            unsigned long long b = strtoull(e, &end, 10);
            if (end && *end == '\0' && b > 0) {
                if (b < (1ull << 20)) {
                    log_warning("%s: SDC_MESH_BUFFER_BYTES=%llu 越界 (有效 %llu..%llu), 已 clamp 为 %llu",
                                test ? test->id : "(global)", b,
                                1ull << 20, 1ull << 44, 1ull << 20);
                    b = 1ull << 20;
                } else if (b > (1ull << 44)) {
                    log_warning("%s: SDC_MESH_BUFFER_BYTES=%llu 越界 (有效 %llu..%llu), 已 clamp 为 %llu",
                                test ? test->id : "(global)", b,
                                1ull << 20, 1ull << 44, 1ull << 44);
                    b = 1ull << 44;
                }
                log_warning("[压力映射] %s.buffer_bytes = %llu 字节 (来源: 旧脚本兼容 env SDC_MESH_BUFFER_BYTES)",
                            test ? test->id : "(global)", b);
                return (size_t)b;
            }
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
     * L1D-to-L1D)——下限让位于预算本身,不越过 OOM 安全边界;上限 1 GiB:
     * 低线程/超大内存主机的轮时长与分配收口。 */
    int total = total_thread_count();
    int cur = thread_count();
    int slices = (cur > 0 && total > cur) ? (total + cur - 1) / cur : 1;
    int consumers = 2 * cur * slices;
    if (consumers < 1)
        consumers = 1;
    uint64_t budget = sdc_budget_bytes(900);
    uint64_t lo = 64ull << 20;
    if (lo > budget)
        lo = budget;
    uint64_t b = budget / (uint64_t)consumers;
    if (b < lo)
        b = lo;
    if (b > (1ull << 30))
        b = 1ull << 30;
    log_warning("[压力映射] %s.buffer_bytes = %llu 字节 (来源: 读流量总量恒定 45%%安全预算 ÷ %d 线程, clamp [64MiB..1GiB]) = %.1f MiB",
                test ? test->id : "(global)", (unsigned long long)b,
                consumers, (double)b / 1048576.0);
    return b;
}

static int mesh_upi_avx_sym_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2) return -255;

    auto *td = new TestData;
    if (!td) return EXIT_FAILURE;

    const size_t elem_bytes = sizeof(float);
    const size_t block_bytes = BLOCK_ELEMENTS * elem_bytes;
    size_t target = mesh_target_bytes(test);
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
     * block * BLOCK_ELEMENTS 是 32 位乘法,元素偏移 ≥ 2^32(缓冲 ≥ 16 GiB)时
     * 低偏移被回绕索引重复写入,缓冲上半段地址从未被写。 */
    td->total_elements = td->num_blocks * BLOCK_ELEMENTS;
    size_t total_bytes = td->total_elements * elem_bytes;

    td->data = (float*)aligned_alloc(16, total_bytes);
    if (!td->data) {
        delete td;
        return EXIT_FAILURE;
    }
    memset(td->data, 0, total_bytes);

    td->next_block.store(0, std::memory_order_relaxed);
    td->global_sum.store(0.0, std::memory_order_relaxed);
    td->allocated_blocks.store(0, std::memory_order_relaxed);
    td->thread_idx.store(0, std::memory_order_relaxed);
    td->arrived.store(0, std::memory_order_relaxed);
    td->epoch.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_avx_sym_run(struct test *test, int cpu) {
    (void)cpu;
    auto *td = static_cast<TestData*>(test->data);

    // 分配唯一线程 ID（用于每轮输出与 asymm 写者轮转）
    int id = td->thread_idx.fetch_add(1, std::memory_order_relaxed);

    /* 线程总数取框架权威值 thread_count()（自动尊重 --cpuset、
     * test.max_threads 与 OS 亲和性限制，每个线程取值一致）。
     * 原运行时“3 次稳定读数”启发式是 098d4aa 移植引入的缺陷：大规模
     * 并发下严重欠计（191 线程实测检出 2，甚至读出垃圾值），屏障参与
     * 者数目随之错误 → 轮次隔离崩溃 → 误报 sum mismatch；此前的死锁
     * 屏障恰好掩盖了它。另注：该启发式在 -n 1 下会永久自旋挂死，
     * thread_count() 则给出确定性结果。 */
    uint32_t total_threads = thread_count();
    if (total_threads < 2) {
        log_skip(CpuTopologyIssueSkipCategory,
                 "mesh_upi_avx_sym requires at least 2 threads (inter-core test); "
                 "skipping on this thread count");
        return EXIT_SKIP;
    }

    /* randomization hardening H14' (P17): framework RNG。
     * 取整数值 float：大 N 下 double 求和仍精确(整数 <=2^53 无舍入)，
     * 保证 read_sum == expected_sum 在任意块序下都成立，避免放大工作集
     * 后因浮点求和次序差异产生假 Sum mismatch。 */
    auto dist = []() { return (float)(int32_t)(frandomf_scale(2000.0f) - 1000.0f); };

    #define GREEN "\033[32m"
    #define RED   "\033[31m"
    #define RESET "\033[0m"

    uint32_t my_epoch = 0;
    /* 本线程是否完成过至少一整轮「填充→全量读取校验」——同 sse_sym
     * （issue #358 / PR #331 二轮意见 #23：-t 截断空过改诚实 SKIP）。 */
    bool validated_any = false;
    do {
        // ---------- 阶段 1：所有线程共同填充数据 ----------
        // （本轮状态已由上一轮 sense-reversal 屏障的最后到达线程重置）

        // 所有线程竞争分配块并写入随机浮点数据（每块 BLOCK_ELEMENTS 个元素）
        while (true) {
            uint64_t block = td->next_block.fetch_add(1, std::memory_order_seq_cst);
            if (block >= td->num_blocks) break;
            // 块号 64 位,与偏移乘法同宽:首轮修 32 位乘法偏移回绕(元素偏移
            // ≥ 2^32 时低偏移被回绕索引重复写入,缓冲上半段从未被写);R2 去封顶后
            // 块号本身也须 64 位,否则 fetch_add 在块数 ≥ 2^32 时回绕到 0,
            // 低块被重复认领重写。
            size_t offset = (size_t)block * BLOCK_ELEMENTS;
            float vals[BLOCK_ELEMENTS];
            double local_sum = 0.0;
            for (int j = 0; j < BLOCK_ELEMENTS; ++j) {
                vals[j] = dist();
                local_sum += (double)vals[j];
            }
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t vec = vld1q_f32(vals + v * VECTOR_SIZE);
                vst1q_f32(td->data + offset + v * VECTOR_SIZE, vec);  // 对齐存储
            }
            __sync_synchronize();              // 等效于 _mm_sfence

            // 一致性测试：立即读回并比较（整块逐向量）
            bool ok = true;
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t written = vld1q_f32(vals + v * VECTOR_SIZE);
                float32x4_t loaded = vld1q_f32(td->data + offset + v * VECTOR_SIZE);
                uint32x4_t cmp = vceqq_f32(written, loaded);
                if (!(vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF)) {
                    ok = false;
                    break;
                }
            }
            /* 写相位立即读回是逐 lane 字节级比对，其结果必须落地为失败：
             * 读相位的全量校验只有聚合和（和保持型损坏——如补偿性双错、
             * lane 互换——不可见）与 store_buf 自拷贝比较（不对照写入值），
             * 旧注释「最终会在全量校验时捕获」对这类损坏不成立（PR #331
             * 二轮意见 #22）。avx512_sym 同款语义先例（其 135-148 行注释）。
             * 块为本线程独占认领，写后立即回读失配 = 真实 store/load 损坏。 */
            if (!ok) {
                fprintf(stderr, "mesh_upi_avx_sym: Thread %d, block %llu immediate read-back mismatch, written vals[0..3]=(%.6f,%.6f,%.6f,%.6f), loaded data[0..3] at offset %zu=(%.6f,%.6f,%.6f,%.6f), result=%sFAIL%s\n",
                        id, (unsigned long long)block,
                        vals[0], vals[1], vals[2], vals[3],
                        offset,
                        td->data[offset], td->data[offset + 1],
                        td->data[offset + 2], td->data[offset + 3],
                        RED, RESET);
                fflush(stderr);
                report_fail_msg("mesh_upi_avx_sym: write-phase immediate read-back mismatch (store/load corruption)");
                return EXIT_FAILURE;
            }

            // 累加全局和（使用 double 原子操作）
            double old_sum = td->global_sum.load(std::memory_order_relaxed);
            while (!td->global_sum.compare_exchange_weak(old_sum, old_sum + local_sum,
                                                         std::memory_order_seq_cst,
                                                         std::memory_order_relaxed)) {}
            td->allocated_blocks.fetch_add(1, std::memory_order_seq_cst);
        }

        /* 等待所有线程完成写入：131072 自旋一档节流预算检查（SVE 侧
         * mesh_upi_sve_sym 同款，#357 病理的 NEON 侧修复）——原「每自旋
         * 都调 test_time_condition」把 40 次 auto-fracture 迭代预算在
         * µs 级烧穿，等待方在填充完成前提前退出，该轮校验被整轮跳过；
         * 节流后耐心 ~40×131072 自旋（≈0.1-0.3 s），等待条件本身每自旋
         * 都查，响应不受影响。 */
        {
            uint32_t spin = 0;
            while (td->allocated_blocks.load(std::memory_order_seq_cst) < td->num_blocks) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }

        __sync_synchronize();  // 确保写入全局可见（mfence）

        // ---------- 阶段 2：所有线程读取完整数组并验证 ----------
        // （读循环内不查 test_time_condition：40 次迭代预算撑不起按向量
        // 检查的频度，预算耗尽的假「超时」会把好轮截断成废轮；轮时长已
        // 由默认 sizing 钉在亚秒级，显式大缓冲的超时语义由 #357 跟踪。）
        // Sum snapshot BEFORE the read loop (TOCTOU fix): a fast thread can
        // finish its read, run the reset phase and start writing the NEXT
        // iteration while this thread is still walking the buffer. Sampling
        // global_sum after the read would race that reset/next-write and
        // produce a false mismatch (expected_sum cleared to 0 / partially
        // overwritten by the next iteration's writes).
        double expected_sum = td->global_sum.load(std::memory_order_seq_cst);
        double read_sum = 0.0;
        bool consistent = true;
        alignas(16) float store_buf[VECTOR_SIZE];

        for (size_t i = 0; i < td->total_elements; i += VECTOR_SIZE) {
            float32x4_t v = vld1q_f32(td->data + i);
            float vals[VECTOR_SIZE];
            vst1q_f32(vals, v);
            for (int j = 0; j < VECTOR_SIZE; ++j) read_sum += (double)vals[j];

            // 一致性测试：Store/Load 回读比较
            vst1q_f32(store_buf, v);
            float32x4_t reload = vld1q_f32(store_buf);
            uint32x4_t cmp = vceqq_f32(v, reload);
            if (!(vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                  vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                  vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                  vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF))
                consistent = false;
        }

        bool passed = (read_sum == expected_sum) && consistent;

        if (!passed) {
            /* 失败证据（原先每轮每线程都打印 PASS 行——修复 sizing 后轮次
             * 亚秒级、数百轮 × 全线程会把 stderr/memfd 灌成噪音，#324 在
             * sse_sym 立案同款问题；此处改为仅失败时输出，avx512_sym 先例）。 */
            fprintf(stderr, "mesh_upi_avx_sym: Thread %d, data[0..3]=(%.4f,%.4f,%.4f,%.4f), read_sum=%f, expected_sum=%f, consistent=%d, result=%s%s%s\n",
                    id,
                    td->data[0], td->data[1], td->data[2], td->data[3],
                    read_sum, expected_sum, consistent,
                    passed ? GREEN : RED,
                    passed ? "PASS" : "FAIL",
                    RESET);
            fflush(stderr);
            report_fail_msg("mesh_upi_avx_sym: Sum mismatch or consistency failure");
            return EXIT_FAILURE;
        }
        validated_any = true;   // 本轮全量读取校验已通过

        // ---------- 阶段 3：sense-reversal 屏障进入下一轮 ----------
        // 最后到达的线程在此重置下一轮状态（global_sum/next_block/
        // allocated_blocks）：fetch_add(acq_rel) 的到达顺序保证所有线程
        // 都已离开阶段 2 的校验，epoch 的 release 存储保证重置先于任何
        // 线程的下一轮填充可见 —— 这正是原协议想做而没做对的轮次隔离。
        uint32_t done = td->arrived.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (done == total_threads) {
            td->global_sum.store(0.0, std::memory_order_seq_cst);
            td->next_block.store(0, std::memory_order_seq_cst);
            td->allocated_blocks.store(0, std::memory_order_seq_cst);
            td->arrived.store(0, std::memory_order_relaxed);
            td->epoch.store(my_epoch + 1, std::memory_order_release);
        } else {
            /* 同款 131072 一档节流（先例 mesh_upi_sve_sym 的 epoch 等待） */
            uint32_t spin = 0;
            while (td->epoch.load(std::memory_order_acquire) == my_epoch) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }
        ++my_epoch;

    } while (test_time_condition(test));

round_aborted:
    if (!validated_any) {
        log_skip(TestResourceIssueSkipCategory,
                 "mesh_upi_avx_sym: test time expired before the first full "
                 "validation round completed (runtime-sized buffer fill "
                 "outlives -t); nothing was verified in this run");
        return EXIT_SKIP;
    }
    return EXIT_SUCCESS;

    #undef GREEN
    #undef RED
    #undef RESET
}

static int mesh_upi_avx_sym_finish(struct test *test) {
    auto *td = static_cast<TestData*>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_avx_sym_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for mesh_upi_avx_sym");
    return EXIT_SKIP;
}
static int mesh_upi_avx_sym_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_avx_sym_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_avx_sym,
             "Symmetrical stress … do AVX loads and stores from L1D to L1D with different directions and switching cores (ARM NEON version)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_avx_sym_init,
    .test_run = mesh_upi_avx_sym_run,
    .test_cleanup = mesh_upi_avx_sym_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

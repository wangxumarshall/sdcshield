#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#ifdef __aarch64__
#include <arm_neon.h>
#include "sdc_stress_config.h"
#include <unistd.h>
#include <cstdlib>
#include <sys/sysinfo.h>

#define NEON_VECTOR_SIZE 4                // NEON 一次处理 4 个 float
#define BLOCK_SIZE 8192                   // 每块元素数(加大块以降低原子争用)
#define VECTORS_PER_BLOCK (BLOCK_SIZE / NEON_VECTOR_SIZE)  // = 2048
// 块数/总元素数在 init 中按目标内存动态计算(见 mesh_target_bytes)。

namespace { /* ODR: per-TU TestData (66-file same-name hazard) */
struct TestData {
    alignas(16) float *data;            // 共享数据数组（16 字节对齐）
    size_t num_blocks;                  // 运行时块数(按目标内存计算)
    size_t total_elements;              // 运行时总元素数
    std::atomic<uint64_t> next_block;
    std::atomic<uint64_t> global_sum;   // 未使用，但保留
    std::atomic<uint64_t> write_done;
    std::atomic<uint32_t> read_done;
    std::atomic<uint32_t> reset_lock;
    std::atomic<uint32_t> thread_idx;
};
}

// 目标字节数: -O mesh_upi_avx512_sym.buffer_bytes=N /
// SDC_MESH_UPI_AVX512_SYM_BUFFER_BYTES 环境变量 / 旧脚本兼容
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
// L1D-to-L1D;下限不超过本实例战役份额,紧预算下让位于不超分,预算
// 为 0 时诚实 SKIP),上限收口低线程/超大内存主机的轮时长;显式 knob/
// env 不受该 clamp 约束(大足迹
// 压力仍可显式申请,其超时语义由 #357 族级根治跟踪)。等待侧预算节流
// 见 run 内 131072 一档自旋(同 SVE 侧先例)。
/* 孪生同步:本函数与 mesh_upi_avx{,512}_sym.cpp 另一文件逐字节同款
 * (全部日志/env 名/knob 均由 test->id 派生)——改此函数必须双文件同步;
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

static int mesh_upi_avx512_sym_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2) {
        return -255;
    }

    auto *td = new TestData;
    if (!td) return EXIT_FAILURE;

    const size_t elem_bytes = sizeof(float);
    const size_t block_bytes = BLOCK_SIZE * elem_bytes;
    size_t target = mesh_target_bytes(test);
    if (target == 0) {
        /* 四源安全预算为 0(cgroup 限额/承诺余量耗尽)——任何缓冲分配都是
         * 超分,诚实 SKIP 而非缩到 1 块兜底空转(检视轮 2 意见)。 */
        log_skip(TestResourceIssueSkipCategory,
                 "%s: four-source memory budget is zero (cgroup limit / "
                 "commit headroom exhausted); cannot size a meaningful "
                 "mesh buffer", test->id);
        return EXIT_SKIP;
    }
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
     * block * BLOCK_SIZE 是 32 位乘法,元素偏移 ≥ 2^32(缓冲 ≥ 16 GiB)时
     * 低偏移被回绕索引重复写入,缓冲上半段地址从未被写。 */
    td->total_elements = td->num_blocks * BLOCK_SIZE;
    size_t total_bytes = td->total_elements * elem_bytes;

    td->data = (float*)aligned_alloc(16, total_bytes);
    if (!td->data) {
        delete td;
        return EXIT_FAILURE;
    }
    memset(td->data, 0, total_bytes);

    td->next_block.store(0, std::memory_order_relaxed);
    td->global_sum.store(0, std::memory_order_relaxed);
    td->write_done.store(0, std::memory_order_relaxed);
    td->read_done.store(0, std::memory_order_relaxed);
    td->reset_lock.store(0, std::memory_order_relaxed);
    td->thread_idx.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_avx512_sym_run(struct test *test, int cpu) {
    (void)cpu;
    auto *td = static_cast<TestData*>(test->data);

    int id = td->thread_idx.fetch_add(1, std::memory_order_relaxed);
    uint32_t total = thread_count();

    /* randomization hardening H14' (P17): framework RNG */
    /* randomization hardening H14' (P17): framework RNG, [-1000, 1000) */
    auto dist = []() { return frandomf_scale(2000.0f) - 1000.0f; };

    #define GREEN "\033[32m"
    #define RED   "\033[31m"
    #define RESET "\033[0m"

    /* 本线程是否完成过至少一整轮「填充→全量读取校验」——同 sse_sym
     * （issue #358 / PR #331 二轮意见 #23：-t 截断空过改诚实 SKIP）。 */
    bool validated_any = false;
    do {
        // ---------- 写阶段：分配块并写入随机浮点数 ----------
        while (true) {
            uint64_t block = td->next_block.fetch_add(1, std::memory_order_seq_cst);
            if (block >= td->num_blocks) break;

            // 块号 64 位,与偏移乘法同宽:首轮修 32 位乘法偏移回绕(元素偏移
            // ≥ 2^32 时低偏移被回绕索引重复写入,缓冲上半段从未被写);R2 去封顶后
            // 块号本身也须 64 位,否则 fetch_add 在块数 ≥ 2^32 时回绕到 0,
            // 低块被重复认领重写。
            size_t offset = (size_t)block * BLOCK_SIZE;
            float vals[BLOCK_SIZE];
            double local_sum = 0.0;
            for (int j = 0; j < BLOCK_SIZE; ++j) {
                vals[j] = dist();
                local_sum += (double)vals[j];
            }
            // 使用 4 个 NEON 向量存储 16 个 float
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t vec = vld1q_f32(vals + v * NEON_VECTOR_SIZE);
                vst1q_f32(td->data + offset + v * NEON_VECTOR_SIZE, vec);
            }
            __sync_synchronize();   // 类似 _mm_sfence

            // 一致性测试：立即读回比较
            bool block_ok = true;
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t written = vld1q_f32(vals + v * NEON_VECTOR_SIZE);
                float32x4_t loaded = vld1q_f32(td->data + offset + v * NEON_VECTOR_SIZE);
                uint32x4_t cmp = vceqq_f32(written, loaded);
                if (!(vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF)) {
                    block_ok = false;
                    break;
                }
            }
            /* 写阶段的立即读回是本测试唯一逐字节的写入内容校验——读阶段
             * 只做 store_buf 自一致性比较，不对照写入值，所以 block_ok
             * 为假必须直接判失败，不能只记录。 */
            if (!block_ok) {
                fprintf(stderr, "mesh_upi_avx512_sym: Thread %d, block %llu immediate read-back mismatch, written vals[0..3]=(%.6f,%.6f,%.6f,%.6f), loaded data[0..3] at offset %zu=(%.6f,%.6f,%.6f,%.6f), result=%sFAIL%s\n",
                        id, (unsigned long long)block,
                        vals[0], vals[1], vals[2], vals[3],
                        offset,
                        td->data[offset], td->data[offset + 1],
                        td->data[offset + 2], td->data[offset + 3],
                        RED, RESET);
                fflush(stderr);
                report_fail_msg("mesh_upi_avx512_sym: write-phase immediate read-back mismatch (store/load corruption)");
                return EXIT_FAILURE;
            }
            td->write_done.fetch_add(1, std::memory_order_seq_cst);
        }

        /* 等待所有块写完成：131072 自旋一档节流预算检查（SVE 侧
         * mesh_upi_sve_sym 同款，#357 病理的 NEON 侧修复）——原「每自旋
         * 都调 test_time_condition」把 40 次 auto-fracture 迭代预算在
         * µs 级烧穿，等待方在填充完成前提前退出，该轮校验被整轮跳过；
         * 节流后耐心 ~40×131072 自旋（≈0.1-0.3 s），等待条件本身每自旋
         * 都查，响应不受影响。 */
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

        // ---------- 读阶段：读取整个数组并校验 ----------
        // （读循环内不查 test_time_condition：40 次迭代预算撑不起按块
        // 检查的频度，预算耗尽的假「超时」会把好轮截断成废轮；轮时长已
        // 由默认 sizing 钉在亚秒级，显式大缓冲的超时语义由 #357 跟踪。
        // 另注：本阶段校验是栈上自一致性比较（不对照写入值），对 mesh
        // 载入路径的跨线程损坏不可见——校验能力缺口由 #332 立案重构，
        // 勿据此以为有逐字节内容校验。）
        bool consistent = true;
        alignas(16) float store_buf[BLOCK_SIZE];

        for (size_t i = 0; i < td->total_elements; i += BLOCK_SIZE) {
            // 加载 16 个元素到临时数组
            float vals[BLOCK_SIZE];
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t vec = vld1q_f32(td->data + i + v * NEON_VECTOR_SIZE);
                vst1q_f32(vals + v * NEON_VECTOR_SIZE, vec);
            }

            // 一致性测试：Store/Load 比较
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t orig = vld1q_f32(vals + v * NEON_VECTOR_SIZE);
                vst1q_f32(store_buf + v * NEON_VECTOR_SIZE, orig);
            }
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float32x4_t orig = vld1q_f32(vals + v * NEON_VECTOR_SIZE);
                float32x4_t reload = vld1q_f32(store_buf + v * NEON_VECTOR_SIZE);
                uint32x4_t cmp = vceqq_f32(orig, reload);
                if (!(vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF)) {
                    consistent = false;
                    break;
                }
            }
            if (!consistent) break;
        }

        bool passed = consistent;

        if (!passed) {
            // 首次失败证据（原先每个工作迭代都从线程 0 打印 PASS 行）
            fprintf(stderr, "mesh_upi_avx512_sym: Thread %d, data[0..15]=(%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f,%.6f), consistent=%d, result=%s%s%s\n",
                    id,
                    td->data[0], td->data[1], td->data[2], td->data[3],
                    td->data[4], td->data[5], td->data[6], td->data[7],
                    td->data[8], td->data[9], td->data[10], td->data[11],
                    td->data[12], td->data[13], td->data[14], td->data[15],
                    consistent,
                    passed ? GREEN : RED,
                    passed ? "PASS" : "FAIL",
                    RESET);
            fflush(stderr);
            report_fail_msg("mesh_upi_avx512_sym: Consistency failure");
            return EXIT_FAILURE;
        }
        validated_any = true;   // 本轮全量读取校验（自一致性）已通过

        td->read_done.fetch_add(1, std::memory_order_seq_cst);

        // ---------- 重置阶段 ----------
        uint32_t expected_lock = 0;
        if (td->reset_lock.compare_exchange_strong(expected_lock, 1,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst)) {
            /* 同款 131072 一档节流（先例 mesh_upi_sve_sym 的等待） */
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

            td->global_sum.store(0, std::memory_order_seq_cst);
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
                 "mesh_upi_avx512_sym: test time expired before the first full "
                 "validation round completed (runtime-sized buffer fill "
                 "outlives -t); nothing was verified in this run");
        return EXIT_SKIP;
    }
    return EXIT_SUCCESS;

    #undef GREEN
    #undef RED
    #undef RESET
}

static int mesh_upi_avx512_sym_finish(struct test *test) {
    auto *td = static_cast<TestData*>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_avx512_sym_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for mesh_upi_avx512_sym");
    return EXIT_SKIP;
}
static int mesh_upi_avx512_sym_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_avx512_sym_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_avx512_sym,
             "Symmetrical stress MESH and UPI (do NEON loads and stores from L1D to L1D with different directions and #cores)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_avx512_sym_init,
    .test_run = mesh_upi_avx512_sym_run,
    .test_cleanup = mesh_upi_avx512_sym_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

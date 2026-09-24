#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <random>
#ifdef __aarch64__
#include <arm_neon.h>
#include <unistd.h>
#include <sys/sysinfo.h>

#define TOTAL_ELEMENTS 8
#define VECTOR_SIZE 4                     // NEON 一次处理 4 个 int32
#define BLOCK_ELEMENTS TOTAL_ELEMENTS     // 块大小仍为 8（两个向量）
#define NUM_BLOCKS (TOTAL_ELEMENTS / BLOCK_ELEMENTS)   // =1
#define TOTAL_BYTES (TOTAL_ELEMENTS * sizeof(int32_t))

struct TestData {
    alignas(16) int32_t *data;            // 16 字节对齐即可
    std::atomic<uint32_t> next_block;
    std::atomic<uint64_t> global_sum;
    std::atomic<uint32_t> allocated_blocks;
    std::atomic<uint32_t> thread_idx;
    /* sense-reversal 轮次屏障（移植自 mesh_upi_avx2_asymm_int 的已验证
     * 修复）：原读者侧 round_done/reader_count 协议有“快照欠计 + 非原子
     * 重置”竞态（与 mesh_upi_sse_asymm_distrib_int 同款）—— reader_count
     * 依赖读线程自注册，早到的读者可能在快照里欠计尚未注册的读者而独自
     * 判定 done==total 并重置轮次；重置的多个 store 与其它读者的校验读
     * 不原子，被抢占期间迟到读者会校验到跨轮混合数据 → sum 失配伪失败
     * （伪 SDC）。改用 thread_count() 权威参与数 + arrived/epoch 世代
     * 翻转屏障（写者也参与）。reset_lock 为遗留死字段（仅 init 写过），
     * 一并移除。 */
    std::atomic<uint32_t> arrived;       // 本轮已到达屏障的线程数（写者+读者）
    std::atomic<uint32_t> epoch;         // 屏障世代（sense 反转）
};

static int mesh_upi_avx2_asymm_distrib_int_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2) {
        return -255;
    }

    auto *td = new TestData;
    if (!td) return EXIT_FAILURE;

    td->data = (int32_t*)aligned_alloc(16, TOTAL_BYTES);
    if (!td->data) {
        delete td;
        return EXIT_FAILURE;
    }
    memset(td->data, 0, TOTAL_BYTES);

    td->next_block.store(0, std::memory_order_relaxed);
    td->global_sum.store(0, std::memory_order_relaxed);
    td->allocated_blocks.store(0, std::memory_order_relaxed);
    td->thread_idx.store(0, std::memory_order_relaxed);
    td->arrived.store(0, std::memory_order_relaxed);
    td->epoch.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_avx2_asymm_distrib_int_run(struct test *test, int cpu) {
    (void)cpu;
    auto *td = static_cast<TestData*>(test->data);

    int id = td->thread_idx.fetch_add(1, std::memory_order_relaxed);
    bool is_writer = (id == 0);

    /* 线程总数取框架权威值 thread_count()（自动尊重 --cpuset、
     * test.max_threads 与 OS 亲和性限制，每个线程取值一致）—— 与
     * mesh_upi_avx2_asymm_int / mesh_upi_sse_asymm_write_int 的修复
     * 相同。原 reader_count 自注册计数在读线程尚未全部进入 test_run
     * 时会被早到的读者欠计快照，轮次屏障参与者数目随之错误 → 轮次
     * 隔离崩溃 → sum 失配伪失败。单线程（-n 1）按家族先例干净跳过
     * （1 写 0 读无从校验，跑了也是 vacuous pass）。 */
    uint32_t total_threads = thread_count();
    if (total_threads < 2) {
        log_skip(CpuTopologyIssueSkipCategory,
                 "mesh_upi_avx2_asymm_distrib_int requires at least 2 threads (inter-core test); "
                 "skipping on this thread count");
        return EXIT_SKIP;
    }

    /* randomization hardening H14' (P17): framework RNG (per-thread
     * stream, -s reproducible) replaces std::mt19937; range [-1000000, 1000000). */
    auto dist = []() { return (-1000000) + (int32_t)(random64() % (uint64_t)((1000000) - (-1000000) + 1)); };

    #define GREEN "\033[32m"
    #define RED   "\033[31m"
    #define RESET "\033[0m"

    uint32_t my_epoch = 0;
    do {
        if (is_writer) {
            /* 本轮状态已由上一轮 sense-reversal 屏障的最后到达线程在
             * 全部线程（写者 + 读者）到达之后重置（第一轮由 init 重置），
             * 写者无需再自旋等待“重置完成”—— 原先的
             * allocated_blocks==0 && round_done==0 等待与读者侧的
             * round_done/reader_count 协议共享同一个非原子重置窗口
             * （欠计的早到读者可能提前触发重置、放行写者进入下一轮）。
             * 每轮写 NUM_BLOCKS（=1）个块，填充不逐块消耗 fracture
             * 预算（同 2731971 写者的无守卫填充）。 */
            uint32_t block = td->next_block.fetch_add(1, std::memory_order_seq_cst);
            if (block < NUM_BLOCKS) {
                // 生成随机数据并写入（8个 int32，分两个 NEON 向量）
                int32_t vals[TOTAL_ELEMENTS];
                uint64_t local_sum = 0;
                for (int j = 0; j < TOTAL_ELEMENTS; ++j) {
                    vals[j] = dist();
                    local_sum += (uint64_t)vals[j];
                }
                // 前 4 个
                int32x4_t v0 = vld1q_s32(vals);
                vst1q_s32(td->data, v0);
                // 后 4 个
                int32x4_t v1 = vld1q_s32(vals + 4);
                vst1q_s32(td->data + 4, v1);
                // 内存屏障确保数据对其它核心可见
                __sync_synchronize();

                td->global_sum.fetch_add(local_sum, std::memory_order_seq_cst);
                td->allocated_blocks.fetch_add(1, std::memory_order_seq_cst);
            }
        } else {
            /* 读核心：等待写者完成本轮写入。ttc 预算检查按 1024 自旋
             * 一档节流 —— 原实现每次自旋都调 test_time_condition，是
             * 8502e491 在三个同族文件里修掉的 fracture 预算饿死模式
             * （读者在写者完成前耗尽预算提前退出 → 校验被跳过）。
             * 预算耗尽走 out_of_budget 干净退出。 */
            uint32_t spin = 0;
            while (td->allocated_blocks.load(std::memory_order_seq_cst) < NUM_BLOCKS) {
                if ((++spin & 1023u) == 0) {
                    if (!test_time_condition(test)) goto out_of_budget;
                } else {
                    __asm__ volatile("yield");
                }
            }

            // 读取数据（两个向量）
            int32x4_t v0 = vld1q_s32(td->data);
            int32x4_t v1 = vld1q_s32(td->data + 4);
            int32_t vals[8];
            vst1q_s32(vals, v0);
            vst1q_s32(vals + 4, v1);
            uint64_t read_sum = 0;
            for (int j = 0; j < TOTAL_ELEMENTS; ++j) {
                read_sum += (uint64_t)vals[j];
            }

            uint64_t expected_sum = td->global_sum.load(std::memory_order_seq_cst);
            bool passed = (read_sum == expected_sum);

            if (!passed) {
                // 首次失败证据（原先每个读迭代都从读核心 1 打印 PASS 行）
                fprintf(stderr, "mesh_upi_avx2_asymm_distrib_int: Thread %d (reader), data[0..7]=(%d,%d,%d,%d,%d,%d,%d,%d), read_sum=%lu, expected_sum=%lu, result=%s%s%s\n",
                        id,
                        td->data[0], td->data[1], td->data[2], td->data[3],
                        td->data[4], td->data[5], td->data[6], td->data[7],
                        read_sum, expected_sum,
                        passed ? GREEN : RED,
                        passed ? "PASS" : "FAIL",
                        RESET);
                fflush(stderr);
                report_fail_msg("mesh_upi_avx2_asymm_distrib_int: Sum mismatch");
                return EXIT_FAILURE;
            }
        }

        /* ---------- 轮次屏障：sense-reversal（写者也到达） ----------
         * 最后到达的线程（第 thread_count() 个 = 写者 + 全部读者）在
         * 此重置下一轮状态（global_sum/next_block/allocated_blocks）：
         * fetch_add(acq_rel) 的到达计数保证所有线程都已完成本轮流量的
         * 生产/校验（含写者完成填充），epoch 的 release 存储 + 等待侧
         * acquire 加载保证重置先于任何线程的下一轮访问可见 —— 同时
         * 消除原协议“done 判定与重置不原子”和“round_done!=0 自旋
         * 错过瞬态”两个窗口。自旋按 1024 次一档节流 ttc 预算检查
         * （理由同读者等待）。 */
        {
            uint32_t done = td->arrived.fetch_add(1, std::memory_order_acq_rel) + 1;
            if (done == total_threads) {
                td->global_sum.store(0, std::memory_order_seq_cst);
                td->next_block.store(0, std::memory_order_seq_cst);
                td->allocated_blocks.store(0, std::memory_order_seq_cst);
                td->arrived.store(0, std::memory_order_relaxed);
                td->epoch.store(my_epoch + 1, std::memory_order_release);
            } else {
                uint32_t spin = 0;
                while (td->epoch.load(std::memory_order_acquire) == my_epoch) {
                    if ((++spin & 1023u) == 0) {
                        if (!test_time_condition(test)) goto out_of_budget;
                    } else {
                        __asm__ volatile("yield");
                    }
                }
            }
        }
        ++my_epoch;
    } while (test_time_condition(test));

out_of_budget:
    return EXIT_SUCCESS;

    #undef GREEN
    #undef RED
    #undef RESET
}

static int mesh_upi_avx2_asymm_distrib_int_finish(struct test *test) {
    auto *td = static_cast<TestData*>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_avx2_asymm_distrib_int_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for mesh_upi_avx2_asymm_distrib_int");
    return EXIT_SKIP;
}
static int mesh_upi_avx2_asymm_distrib_int_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_avx2_asymm_distrib_int_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_avx2_asymm_distrib_int,
             "Asymmetrical(1 write core to all rest distributed read cores) stress MESH and UPI or Ring Interconnect and QPI (do int32 AVX-2(YMM) loads and stores from L1/L2 to L1/L2 with switching write core) [ARM NEON version]")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_avx2_asymm_distrib_int_init,
    .test_run = mesh_upi_avx2_asymm_distrib_int_run,
    .test_cleanup = mesh_upi_avx2_asymm_distrib_int_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

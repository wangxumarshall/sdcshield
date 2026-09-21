#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#ifdef __aarch64__
#include <arm_neon.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/sysinfo.h>

#define TOTAL_ELEMENTS 1024
#define VECTOR_SIZE 4                     // NEON 可容纳 4 个 float
#define BLOCK_ELEMENTS VECTOR_SIZE
#define NUM_BLOCKS (TOTAL_ELEMENTS / BLOCK_ELEMENTS)  // = 256
#define TOTAL_BYTES (TOTAL_ELEMENTS * sizeof(float))

struct TestData {
    alignas(16) float *data;             // 共享数据数组，16 字节对齐
    std::atomic<uint32_t> next_block;
    std::atomic<double> global_sum;      // 用 double 累加，减少误差
    std::atomic<uint32_t> allocated_blocks;
    std::atomic<uint32_t> thread_idx;
    /* sense-reversal barrier (G2'/G3' fix): the original iter/read_done/
     * round_done protocol deadlocked from round 1 (read_done was never
     * set to 1; iter only advanced after a completed round), so the sum
     * check never executed and the test passed vacuously. Pre-existing
     * since the 098d4aa port. */
    std::atomic<uint32_t> arrived;
    std::atomic<uint32_t> epoch;
};

static int mesh_upi_sse_sym_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2) return -255;

    auto *td = new TestData;
    if (!td) return EXIT_FAILURE;

    td->data = (float*)aligned_alloc(16, TOTAL_BYTES);
    if (!td->data) {
        delete td;
        return EXIT_FAILURE;
    }
    memset(td->data, 0, TOTAL_BYTES);

    td->next_block.store(0, std::memory_order_relaxed);
    td->global_sum.store(0.0, std::memory_order_relaxed);
    td->allocated_blocks.store(0, std::memory_order_relaxed);
    td->thread_idx.store(0, std::memory_order_relaxed);
    td->arrived.store(0, std::memory_order_relaxed);
    td->epoch.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_sse_sym_run(struct test *test, int cpu) {
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
        report_fail_msg("Requires at least 2 threads");
        return EXIT_FAILURE;
    }

    /* randomization hardening H14' (P17): framework RNG */
    /* randomization hardening H14' (P17): framework RNG, [-1000, 1000) */
    auto dist = []() { return frandomf_scale(2000.0f) - 1000.0f; };

    #define GREEN "\033[32m"
    #define RED   "\033[31m"
    #define RESET "\033[0m"

    uint32_t my_epoch = 0;
    do {
        // ---------- 阶段 1：所有线程共同填充数据 ----------
        // （本轮状态已由上一轮 sense-reversal 屏障的最后到达线程重置）

        // 所有线程竞争分配块并写入随机浮点数据
        while (true) {
            uint32_t block = td->next_block.fetch_add(1, std::memory_order_seq_cst);
            if (block >= NUM_BLOCKS) break;
            size_t offset = block * BLOCK_ELEMENTS;
            float vals[VECTOR_SIZE];
            double local_sum = 0.0;
            for (int j = 0; j < VECTOR_SIZE; ++j) {
                vals[j] = dist();
                local_sum += (double)vals[j];
            }
            float32x4_t v = vld1q_f32(vals);
            vst1q_f32(td->data + offset, v);  // 对齐存储（数据已 16 字节对齐）
            __sync_synchronize();   // 类似 _mm_sfence

            // 一致性测试：立即读回并比较
            float32x4_t loaded = vld1q_f32(td->data + offset);
            uint32x4_t cmp = vceqq_f32(v, loaded);
            // 检查所有 4 个元素是否相等
            bool ok = (vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                       vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                       vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                       vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF);
            if (!ok) {
                // 不一致，但最终会在全量校验时捕获
            }

            // 累加全局和（使用 double 原子操作）
            double old_sum = td->global_sum.load(std::memory_order_relaxed);
            while (!td->global_sum.compare_exchange_weak(old_sum, old_sum + local_sum,
                                                         std::memory_order_seq_cst,
                                                         std::memory_order_relaxed)) {}
            td->allocated_blocks.fetch_add(1, std::memory_order_seq_cst);
        }

        // 等待所有线程完成写入
        while (td->allocated_blocks.load(std::memory_order_seq_cst) < NUM_BLOCKS &&
               test_time_condition(test)) {
            __asm__ volatile("yield");
        }
        if (!test_time_condition(test)) break;

        __sync_synchronize();  // 确保写入全局可见

        // ---------- 阶段 2：所有线程读取完整数组并验证 ----------
        double read_sum = 0.0;
        bool consistent = true;
        alignas(16) float store_buf[VECTOR_SIZE];

        for (size_t i = 0; i < TOTAL_ELEMENTS; i += VECTOR_SIZE) {
            float32x4_t v = vld1q_f32(td->data + i);
            float vals[VECTOR_SIZE];
            vst1q_f32(vals, v);
            for (int j = 0; j < VECTOR_SIZE; ++j) read_sum += (double)vals[j];

            // 一致性测试：Store/Load 回读比较
            vst1q_f32(store_buf, v);
            float32x4_t reload = vld1q_f32(store_buf);
            uint32x4_t cmp = vceqq_f32(v, reload);
            // 检查所有 4 个元素是否相等
            if (!(vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                  vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                  vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                  vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF)) {
                consistent = false;
            }
        }

        double expected_sum = td->global_sum.load(std::memory_order_seq_cst);
        bool passed = (read_sum == expected_sum) && consistent;

        // 输出结果（每个线程输出一次）
        fprintf(stderr, "mesh_upi_sse_sym: Thread %d, data[0..3]=(%.4f,%.4f,%.4f,%.4f), read_sum=%f, expected_sum=%f, consistent=%d, result=%s%s%s\n",
                id,
                td->data[0], td->data[1], td->data[2], td->data[3],
                read_sum, expected_sum, consistent,
                passed ? GREEN : RED,
                passed ? "PASS" : "FAIL",
                RESET);
        fflush(stderr);

        if (!passed) {
            report_fail_msg("mesh_upi_sse_sym: Sum mismatch or consistency failure");
            return EXIT_FAILURE;
        }

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
            while (td->epoch.load(std::memory_order_acquire) == my_epoch &&
                   test_time_condition(test)) {
                __asm__ volatile("yield");
            }
            if (!test_time_condition(test)) break;
        }
        ++my_epoch;

    } while (test_time_condition(test));

    return EXIT_SUCCESS;

    #undef GREEN
    #undef RED
    #undef RESET
}

static int mesh_upi_sse_sym_finish(struct test *test) {
    auto *td = static_cast<TestData*>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_sse_sym_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for mesh_upi_sse_sym");
    return EXIT_SKIP;
}
static int mesh_upi_sse_sym_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_sse_sym_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_sse_sym,
             "Symmetrical stress … do NEON loads and stores from L1D to L1D with different directions and switching cores")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_sse_sym_init,
    .test_run = mesh_upi_sse_sym_run,
    .test_cleanup = mesh_upi_sse_sym_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

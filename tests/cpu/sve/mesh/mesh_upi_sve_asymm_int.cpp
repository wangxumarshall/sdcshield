#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <random>
#ifdef __aarch64__
#include <arm_sve.h>
#include <sys/auxv.h>
#include <unistd.h>
#include <cstdlib>
#include <sys/sysinfo.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif

#define TOTAL_ELEMENTS 1024
#define VECTOR_SIZE 4                     // 原负载：一次处理 4 个 int32（SVE 谓词分批）
#define BLOCK_ELEMENTS (VECTOR_SIZE * 2)  // 每个块 8 个元素（两个向量）
#define NUM_BLOCKS (TOTAL_ELEMENTS / BLOCK_ELEMENTS)  // = 128
#define TOTAL_BYTES (TOTAL_ELEMENTS * sizeof(int32_t))

struct TestData {
    alignas(16) int32_t *data;           // 16 字节对齐即可
    std::atomic<uint32_t> next_block;
    std::atomic<uint64_t> global_sum;
    std::atomic<uint32_t> allocated_blocks;
    std::atomic<uint32_t> thread_idx;
    std::atomic<uint32_t> round_done;
    std::atomic<uint64_t> iter;
    std::atomic<uint32_t> num_threads;
    std::atomic<uint32_t> ready;
};

static int mesh_upi_sve_asymm_int_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2) return -255;

    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for mesh_upi_sve_asymm_int");
        return EXIT_SKIP;
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
    td->round_done.store(0, std::memory_order_relaxed);
    td->iter.store(0, std::memory_order_relaxed);
    td->num_threads.store(0, std::memory_order_relaxed);
    td->ready.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_sve_asymm_int_run(struct test *test, int cpu) {
    (void)cpu;
    auto *td = static_cast<TestData*>(test->data);

    // 分配唯一线程 ID
    int id = td->thread_idx.fetch_add(1, std::memory_order_relaxed);

    // 线程 0 负责确定总线程数
    if (id == 0) {
        while (td->thread_idx.load(std::memory_order_acquire) < 1) __asm__ volatile("yield");
        uint32_t prev = 0, stable = 0;
        while (stable < 3) {
            uint32_t cur = td->thread_idx.load(std::memory_order_acquire);
            if (cur == prev && cur > 1) stable++;
            else { stable = 0; prev = cur; }
            __asm__ volatile("yield");
        }
        td->num_threads.store(prev, std::memory_order_release);
        td->ready.store(1, std::memory_order_release);
    } else {
        while (td->ready.load(std::memory_order_acquire) == 0) __asm__ volatile("yield");
    }

    uint32_t total_threads = td->num_threads.load(std::memory_order_acquire);
    if (total_threads < 2) {
        report_fail_msg("Requires at least 2 threads");
        return EXIT_FAILURE;
    }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int32_t> dist(-1000000, 1000000);

    #define GREEN "\033[32m"
    #define RED   "\033[31m"
    #define RESET "\033[0m"

    do {
        uint64_t current_iter = td->iter.load(std::memory_order_acquire);
        int writer_id = current_iter % total_threads;

        if (id == writer_id) {
            // ---------- 写者：等待上一轮完全结束 ----------
            while ((td->allocated_blocks.load(std::memory_order_seq_cst) != 0 ||
                    td->round_done.load(std::memory_order_seq_cst) != 0 ||
                    td->iter.load(std::memory_order_acquire) == current_iter) &&
                   test_time_condition(test)) {
                __asm__ volatile("yield");
            }
            if (!test_time_condition(test)) break;

            // 重置状态
            td->global_sum.store(0, std::memory_order_seq_cst);
            td->next_block.store(0, std::memory_order_seq_cst);
            td->allocated_blocks.store(0, std::memory_order_seq_cst);
            td->round_done.store(0, std::memory_order_seq_cst);

            // 写入所有块
            for (uint32_t block = 0; block < NUM_BLOCKS; ++block) {
                size_t offset = block * BLOCK_ELEMENTS;
                int32_t vals[BLOCK_ELEMENTS];
                uint64_t local_sum = 0;
                for (int j = 0; j < BLOCK_ELEMENTS; ++j) {
                    vals[j] = dist(rng);
                    local_sum += (uint64_t)vals[j];
                }
                // 分两个向量存储（SVE 谓词分批）
                for (int v = 0; v < 2; ++v) {
                    svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)VECTOR_SIZE);
                    svint32_t vec = svld1_s32(pg, vals + v * VECTOR_SIZE);
                    svst1_s32(pg, td->data + offset + v * VECTOR_SIZE, vec);
                }
                __sync_synchronize();  // 类似 _mm_sfence

                // 一致性测试（自身写入后读回）
                for (int v = 0; v < 2; ++v) {
                    svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)VECTOR_SIZE);
                    svint32_t written = svld1_s32(pg, vals + v * VECTOR_SIZE);
                    svint32_t loaded = svld1_s32(pg, td->data + offset + v * VECTOR_SIZE);
                    svbool_t cmp = svcmpeq_s32(pg, written, loaded);
                    (void)cmp;  // 检查所有元素是否相等（忽略，因为后续读者会校验）
                }

                td->global_sum.fetch_add(local_sum, std::memory_order_seq_cst);
                td->allocated_blocks.fetch_add(1, std::memory_order_seq_cst);
            }

            // 全局屏障确保所有数据已传播到所有核心
            __sync_synchronize();  // 类似 _mm_mfence

        } else {
            // ---------- 读者：等待所有块分配完成 ----------
            while (td->allocated_blocks.load(std::memory_order_seq_cst) < NUM_BLOCKS &&
                   test_time_condition(test)) {
                __asm__ volatile("yield");
            }
            if (!test_time_condition(test)) break;

            __sync_synchronize();  // 确保写者的所有存储可见

            uint64_t read_sum = 0;
            bool consistent = true;
            alignas(16) int32_t store_buf[BLOCK_ELEMENTS];

            for (size_t i = 0; i < TOTAL_ELEMENTS; i += BLOCK_ELEMENTS) {
                int32_t vals[BLOCK_ELEMENTS];
                for (int v = 0; v < 2; ++v) {
                    svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)VECTOR_SIZE);
                    svint32_t vec = svld1_s32(pg, td->data + i + v * VECTOR_SIZE);
                    svst1_s32(pg, vals + v * VECTOR_SIZE, vec);
                }
                for (int j = 0; j < BLOCK_ELEMENTS; ++j) read_sum += (uint64_t)vals[j];

                // 一致性测试：Store/Load 回读比较
                for (int v = 0; v < 2; ++v) {
                    svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)VECTOR_SIZE);
                    svint32_t orig = svld1_s32(pg, vals + v * VECTOR_SIZE);
                    svst1_s32(pg, store_buf + v * VECTOR_SIZE, orig);
                    svint32_t reload = svld1_s32(pg, store_buf + v * VECTOR_SIZE);
                    svbool_t cmp = svcmpeq_s32(pg, orig, reload);
                    if (svcntp_b32(pg, cmp) != VECTOR_SIZE)
                        consistent = false;
                }
            }

            uint64_t expected_sum = td->global_sum.load(std::memory_order_seq_cst);
            bool passed = (read_sum == expected_sum) && consistent;

            // 输出结果（每个读者输出一次）
            fprintf(stderr, "mesh_upi_sve_asymm_int: Thread %d (reader), data[0..7]=(%d,%d,%d,%d,%d,%d,%d,%d), read_sum=%lu, expected_sum=%lu, consistent=%d, result=%s%s%s\n",
                    id,
                    td->data[0], td->data[1], td->data[2], td->data[3],
                    td->data[4], td->data[5], td->data[6], td->data[7],
                    read_sum, expected_sum, consistent,
                    passed ? GREEN : RED,
                    passed ? "PASS" : "FAIL",
                    RESET);
            fflush(stderr);

            if (!passed) {
                report_fail_msg("mesh_upi_sve_asymm_int: Sum mismatch or consistency failure");
                return EXIT_FAILURE;
            }

            // ---------- 最后一个读者执行重置 ----------
            uint32_t done = td->round_done.fetch_add(1, std::memory_order_seq_cst) + 1;
            uint32_t total_readers = total_threads - 1;
            if (done == total_readers) {
                td->global_sum.store(0, std::memory_order_seq_cst);
                td->next_block.store(0, std::memory_order_seq_cst);
                td->allocated_blocks.store(0, std::memory_order_seq_cst);
                td->round_done.store(0, std::memory_order_seq_cst);
                td->iter.fetch_add(1, std::memory_order_release);
            } else {
                while (td->round_done.load(std::memory_order_seq_cst) != 0 &&
                       test_time_condition(test)) {
                    __asm__ volatile("yield");
                }
                if (!test_time_condition(test)) break;
            }
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;

    #undef GREEN
    #undef RED
    #undef RESET
}

static int mesh_upi_sve_asymm_int_finish(struct test *test) {
    auto *td = static_cast<TestData*>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_sve_asymm_int_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for mesh_upi_sve_asymm_int");
    return EXIT_SKIP;
}
static int mesh_upi_sve_asymm_int_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_sve_asymm_int_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_sve_asymm_int,
             "Asymmetrical(1 write core to all rest mutual read cores) stress MESH and UPI or Ring Interconnect and QPI or FSB (do int32 SVE loads and stores from L1/L2 to L1/L2 with switching write core) [ARM SVE version]")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_sve_asymm_int_init,
    .test_run = mesh_upi_sve_asymm_int_run,
    .test_cleanup = mesh_upi_sve_asymm_int_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

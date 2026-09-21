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

#define TOTAL_ELEMENTS 1024               // 总元素数（小工作集，保证快速完成）
#define VECTOR_SIZE 4                     // NEON 每次处理 4 个 int32
#define BLOCK_ELEMENTS VECTOR_SIZE
#define NUM_BLOCKS (TOTAL_ELEMENTS / BLOCK_ELEMENTS)  // = 256
#define TOTAL_BYTES (TOTAL_ELEMENTS * sizeof(int32_t))

struct TestData {
    alignas(16) int32_t *data;           // 共享数据数组，16 字节对齐
    std::atomic<uint32_t> next_block;    // 下一个可分配的块索引
    std::atomic<uint64_t> global_sum;    // 写核心的校验和累加
    std::atomic<uint32_t> allocated_blocks; // 已分配的块数
    std::atomic<uint32_t> thread_idx;    // 线程 ID 分配器
    std::atomic<uint32_t> round_done;    // 已完成校验的读核心数
    std::atomic<uint32_t> reader_count;  // 读核心总数
};

static int mesh_upi_sse_asymm_distrib_int_init(struct test *test) {
    if (sysconf(_SC_NPROCESSORS_ONLN) < 2) {
        return -255;   // SKIP
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
    td->reader_count.store(0, std::memory_order_relaxed);
    test->data = td;

    return EXIT_SUCCESS;
}

static int mesh_upi_sse_asymm_distrib_int_run(struct test *test, int cpu) {
    (void)cpu;
    auto *td = static_cast<TestData*>(test->data);

    int id = td->thread_idx.fetch_add(1, std::memory_order_relaxed);
    bool is_writer = (id == 0);

    if (!is_writer) {
        td->reader_count.fetch_add(1, std::memory_order_relaxed);
    }

    /* randomization hardening H14' (P17): framework RNG (per-thread
     * stream, -s reproducible) replaces std::mt19937; range [-1000000, 1000000). */
    auto dist = []() { return (-1000000) + (int32_t)(random64() % (uint64_t)((1000000) - (-1000000) + 1)); };

    #define GREEN "\033[32m"
    #define RED   "\033[31m"
    #define RESET "\033[0m"

    do {
        if (is_writer) {
            /* 等待读核心完成上一轮重置（allocated_blocks==0 且
             * round_done==0）。原实现把该轮次屏障放在逐块循环体内：
             * 第 0 块填完后 allocated_blocks==1，写者在第 1 块前就
             * 自旋等待“重置完成”，单轮永远填不完 1 块 —— 读端的
             * 256 块等待条件永假，校验从未执行（vacuous pass 的
             * 写端根因）。现在作为轮次屏障放在填充之前；ttc 预算
             * 检查按 1024 自旋一档节流（同 2731971 的 asymm 修复）。 */
            {
                uint32_t spin = 0;
                while (td->allocated_blocks.load(std::memory_order_seq_cst) > 0 ||
                       td->round_done.load(std::memory_order_seq_cst) > 0) {
                    if ((++spin & 1023u) == 0) {
                        if (!test_time_condition(test)) goto out_of_budget;
                    } else {
                        __asm__ volatile("yield");
                    }
                }
            }

            /* 串行填充所有块：有限工作量（NUM_BLOCKS×VECTOR_SIZE 个
             * 元素，微秒级），不逐块消耗 fracture 预算 —— 原实现每块
             * 烧 2 次 test_time_condition，一轮 256 块需 ~513 次而
             * 预算（自动翻倍窗口关闭后停留在 ~160）永远达不到，写者
             * 在填充中途预算耗尽退出。同 2731971 写者的无守卫填充。 */
            while (true) {
                uint32_t block = td->next_block.fetch_add(1, std::memory_order_seq_cst);
                if (block >= NUM_BLOCKS) {
                    break;
                }

                size_t offset = block * BLOCK_ELEMENTS;
                int32_t vals[VECTOR_SIZE];
                uint64_t local_sum = 0;
                for (int j = 0; j < VECTOR_SIZE; ++j) {
                    vals[j] = dist();
                    local_sum += (uint64_t)vals[j];
                }
                int32x4_t v = vld1q_s32(vals);
                vst1q_s32(td->data + offset, v);
                __sync_synchronize();   // 类似 _mm_sfence

                // 一致性测试：立即读回比较
                int32x4_t loaded = vld1q_s32(td->data + offset);
                uint32x4_t cmp = vceqq_s32(v, loaded);
                // 检查所有 4 个元素是否相等
                bool ok = (vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                           vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                           vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                           vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF);
                if (!ok) {
                    // 发现不一致，但继续处理（最终会在读核心校验时失败）
                }

                td->global_sum.fetch_add(local_sum, std::memory_order_seq_cst);
                td->allocated_blocks.fetch_add(1, std::memory_order_seq_cst);
            }
        } else {
            /* 读核心：等待所有块分配完成。原实现每次自旋都调
             * test_time_condition：读者 40 次预算在微秒级就被自旋
             * 烧光（实测读者放弃时写者仅填到第 1 块；而旧写者的
             * 逐块预算烧法本身也到不了 256 块），等待提前中断 →
             * 校验被跳过 → vacuous pass（实测 -f no -t 3000 -n 2
             * 下 0 行读端验证输出）。按 2731971 已验证的节流模式：
             * 等待条件每自旋检查、ttc 预算每 1024 自旋检查一次，
             * 预算耗尽走 out_of_budget 干净退出。 */
            uint32_t spin = 0;
            while (td->allocated_blocks.load(std::memory_order_seq_cst) < NUM_BLOCKS) {
                if ((++spin & 1023u) == 0) {
                    if (!test_time_condition(test)) goto out_of_budget;
                } else {
                    __asm__ volatile("yield");
                }
            }

            // 全内存屏障
            __sync_synchronize();

            uint64_t read_sum = 0;
            bool consistent = true;
            alignas(16) int32_t store_buf[VECTOR_SIZE];

            for (size_t i = 0; i < TOTAL_ELEMENTS; i += VECTOR_SIZE) {
                int32x4_t v = vld1q_s32(td->data + i);
                int32_t vals[VECTOR_SIZE];
                vst1q_s32(vals, v);
                for (int j = 0; j < VECTOR_SIZE; ++j) {
                    read_sum += (uint64_t)vals[j];
                }

                // 一致性测试：Store/Load 比较
                vst1q_s32(store_buf, v);
                int32x4_t reload = vld1q_s32(store_buf);
                uint32x4_t cmp = vceqq_s32(v, reload);
                if (!(vgetq_lane_u32(cmp, 0) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 1) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 2) == 0xFFFFFFFF &&
                      vgetq_lane_u32(cmp, 3) == 0xFFFFFFFF)) {
                    consistent = false;
                }
            }

            uint64_t expected_sum = td->global_sum.load(std::memory_order_seq_cst);
            bool sum_ok = (read_sum == expected_sum);
            bool passed = sum_ok && consistent;

            if (!passed) {
                // 首次失败证据（原先每轮都打印 PASS 行；与 Task 5 家族统一移入失败分支）
                fprintf(stderr, "mesh_upi_sse_asymm_distrib_int: Thread %d (reader), data[0..3]=(%d,%d,%d,%d), read_sum=%lu, expected_sum=%lu, consistent=%d, result=%s%s%s\n",
                        id,
                        td->data[0], td->data[1], td->data[2], td->data[3],
                        read_sum, expected_sum, consistent,
                        passed ? GREEN : RED,
                        passed ? "PASS" : "FAIL",
                        RESET);
                fflush(stderr);
                report_fail_msg("mesh_upi_sse_asymm_distrib_int: Sum mismatch or consistency failure");
                return EXIT_FAILURE;
            }

            // 同步屏障：所有读核心完成校验后，由最后一个重置
            uint32_t done = td->round_done.fetch_add(1, std::memory_order_seq_cst) + 1;
            uint32_t total_readers = td->reader_count.load(std::memory_order_acquire);
            if (done == total_readers) {
                td->global_sum.store(0, std::memory_order_seq_cst);
                td->next_block.store(0, std::memory_order_seq_cst);
                td->allocated_blocks.store(0, std::memory_order_seq_cst);
                td->round_done.store(0, std::memory_order_seq_cst);
            } else {
                /* 非最后的读核心等待最后一者重置（同样按 1024 自旋一档
                 * 节流 ttc 预算检查）。 */
                uint32_t spin = 0;
                while (td->round_done.load(std::memory_order_seq_cst) != 0) {
                    if ((++spin & 1023u) == 0) {
                        if (!test_time_condition(test)) goto out_of_budget;
                    } else {
                        __asm__ volatile("yield");
                    }
                }
            }
        }
    } while (test_time_condition(test));

out_of_budget:
    return EXIT_SUCCESS;

    #undef GREEN
    #undef RED
    #undef RESET
}

static int mesh_upi_sse_asymm_distrib_int_finish(struct test *test) {
    auto *td = static_cast<TestData*>(test->data);
    free(td->data);
    delete td;
    return EXIT_SUCCESS;
}

#else
static int mesh_upi_sse_asymm_distrib_int_init(struct test *test) {
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM NEON required for mesh_upi_sse_asymm_distrib_int");
    return EXIT_SKIP;
}
static int mesh_upi_sse_asymm_distrib_int_run(struct test *test, int cpu) { (void)test; (void)cpu; return EXIT_SKIP; }
static int mesh_upi_sse_asymm_distrib_int_finish(struct test *test) { (void)test; return EXIT_SUCCESS; }
#endif
DECLARE_TEST(mesh_upi_sse_asymm_distrib_int,
             "Asymmetrical(1 write core to all rest distributed read cores) stress MESH and UPI or Ring Interconnect and QPI or FSB (do int32 NEON loads and stores from L1/L2 to L1/L2 with switching write core)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = mesh_upi_sse_asymm_distrib_int_init,
    .test_run = mesh_upi_sse_asymm_distrib_int_run,
    .test_cleanup = mesh_upi_sse_asymm_distrib_int_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#include <sandstone.h>
#include <cstdint>
#include <cstring>


// Deterministic per-thread RNG (framework-seeded, replayable via -s).
// Replaces std::mt19937(std::random_device{}) whose results could not be
// reproduced with -s after a failure.
static inline uint64_t gather_rand64(uint64_t *seed)
{
    *seed = *seed * 0x9E3779B97F4A7C15ULL + 1;
    return *seed;
}

static inline int64_t gather_value_i(uint64_t *seed)
{
    return (int64_t)(gather_rand64(seed) % 2000001) - 1000000;
}

static constexpr int VECTOR_SIZE = 8;           // 8 个 int64
static constexpr int DATA_SIZE = 1024;          // 源/目标数据大小

static int gatherscatter_i64_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int gatherscatter_i64_run(struct test *test, int cpu) {
    (void)cpu;
    // 每个线程独立分配数据（栈上，16字节对齐即可）
    alignas(16) int64_t src[DATA_SIZE];
    alignas(16) int64_t dst[DATA_SIZE];
    int64_t indices[VECTOR_SIZE];

    uint64_t seed = random64();

    do {
        // 生成随机源数据
        for (int i = 0; i < DATA_SIZE; ++i) {
            src[i] = gather_value_i(&seed);
        }
        // 清空目标数组
        memset(dst, 0, sizeof(dst));
        // 生成随机索引（8个64位）
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            indices[i] = (int)(gather_rand64(&seed) % (uint64_t)DATA_SIZE);
        }

        // ---- 硬件执行（标量模拟 gather + scatter） ----
        int64_t gathered[VECTOR_SIZE];
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            gathered[i] = src[indices[i]];
        }
        // 加1处理
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            int idx = indices[i];
            dst[idx] = gathered[i] + 1;
        }

        // ---- 软件参考 ----
        int64_t ref[DATA_SIZE];
        memset(ref, 0, sizeof(ref));
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            int idx = indices[i];
            ref[idx] = src[idx] + 1;
        }

        // ---- 验证：比较目标数组与参考 ----
        bool data_ok = (memcmp(dst, ref, sizeof(dst)) == 0);

        // ---- 一致性测试：将 dst 中前 8 个元素加载两次，比较是否一致 ----
        alignas(16) int64_t load_buf1[VECTOR_SIZE];
        alignas(16) int64_t load_buf2[VECTOR_SIZE];
        memcpy(load_buf1, dst, sizeof(load_buf1));
        memcpy(load_buf2, dst, sizeof(load_buf2));
        bool consistent = (memcmp(load_buf1, load_buf2, sizeof(load_buf1)) == 0);

        if (!(data_ok && consistent)) {
            report_fail_msg("gatherscatter_i64: Gather-scatter mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int gatherscatter_i64_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(gatherscatter_i64, "Gather-scatter 64-bit integers (simulated on ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = gatherscatter_i64_init,
    .test_run = gatherscatter_i64_run,
    .test_cleanup = gatherscatter_i64_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

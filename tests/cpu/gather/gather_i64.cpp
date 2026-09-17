#include <sandstone.h>
#include <cstdint>
#include <cstdio>
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

static constexpr int VECTOR_SIZE = 4;          // 4 个 int64
static constexpr int DATA_SIZE = 1024;         // 源数据大小

static int gather_i64_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int gather_i64_run(struct test *test, int cpu) {
    (void)cpu;
    // 每个线程独立分配数据
    alignas(16) int64_t src[DATA_SIZE];
    int64_t indices[VECTOR_SIZE];

    uint64_t seed = random64();

    do {
        // 生成随机源数据
        for (int i = 0; i < DATA_SIZE; ++i) {
            src[i] = gather_value_i(&seed);
        }
        // 生成随机索引（64位）
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            indices[i] = (int)(gather_rand64(&seed) % (uint64_t)DATA_SIZE);
        }

        // ---- 硬件 Gather（标量模拟，逐个加载） ----
        int64_t gathered[VECTOR_SIZE];
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            gathered[i] = src[indices[i]];
        }

        // ---- 软件参考：与硬件相同 ----
        int64_t ref[VECTOR_SIZE];
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            ref[i] = src[indices[i]];
        }

        // ---- 比较硬件结果与参考（逐位比较） ----
        bool data_ok = (memcmp(gathered, ref, sizeof(ref)) == 0);

        // ---- 一致性测试：存储 gathered 到内存再加载比较 ----
        alignas(16) int64_t store_buf[VECTOR_SIZE];
        memcpy(store_buf, gathered, sizeof(gathered));
        int64_t reload[VECTOR_SIZE];
        memcpy(reload, store_buf, sizeof(store_buf));
        bool consistent = (memcmp(reload, gathered, sizeof(gathered)) == 0);

        if (!(data_ok && consistent)) {
            report_fail_msg("gather_i64: Gather mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int gather_i64_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(gather_i64, "Gather 64-bit integers (simulated on ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = gather_i64_init,
    .test_run = gather_i64_run,
    .test_cleanup = gather_i64_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

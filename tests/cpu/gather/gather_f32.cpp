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

static inline float gather_value_f(uint64_t *seed)
{
    uint64_t r = gather_rand64(seed);
    float f = (float)(r >> 40) * (1.0f / 8388608.0f); // [0,1) 24-bit
    return f * 2000.0f - 1000.0f;
}

static constexpr int VECTOR_SIZE = 8;          // 8 个单精度浮点数
static constexpr int DATA_SIZE = 1024;         // 源数据大小

static int gather_f32_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

static int gather_f32_run(struct test *test, int cpu) {
    (void)cpu;
    // 每个线程独立分配数据
    alignas(16) float src[DATA_SIZE];
    int indices[VECTOR_SIZE];

    uint64_t seed = random64();

    do {
        // 生成随机源数据
        for (int i = 0; i < DATA_SIZE; ++i) {
            src[i] = gather_value_f(&seed);
        }
        // 生成随机索引
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            indices[i] = (int)(gather_rand64(&seed) % (uint64_t)DATA_SIZE);
        }

        // ---- 硬件 Gather（标量模拟，逐个加载） ----
        float gathered[VECTOR_SIZE];
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            gathered[i] = src[indices[i]];
        }

        // ---- 软件参考：与硬件相同（但为了测试逻辑，保留参考） ----
        float ref[VECTOR_SIZE];
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            ref[i] = src[indices[i]];
        }

        // ---- 比较硬件结果与参考（逐位比较） ----
        bool data_ok = (memcmp(gathered, ref, sizeof(ref)) == 0);

        // ---- 一致性测试：存储 gathered 到内存再加载比较 ----
        alignas(16) float store_buf[VECTOR_SIZE];
        memcpy(store_buf, gathered, sizeof(gathered));
        float reload[VECTOR_SIZE];
        memcpy(reload, store_buf, sizeof(store_buf));
        bool consistent = (memcmp(reload, gathered, sizeof(gathered)) == 0);

        if (!(data_ok && consistent)) {
            report_fail_msg("gather_f32: Gather mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int gather_f32_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(gather_f32, "Gather single-precision (simulated on ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = gather_f32_init,
    .test_run = gather_f32_run,
    .test_cleanup = gather_f32_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

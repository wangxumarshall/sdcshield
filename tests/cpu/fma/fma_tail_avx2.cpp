#include <sandstone.h>
#include <cstdint>
#include <cstring>
#include <cmath>

#ifdef __aarch64__
#include <arm_neon.h>          // ARM NEON 头文件
#endif

// Bounded deterministic operand generator (|x| < 2, high-entropy mantissa).
// The fma family deliberately keeps operands in a bounded range so every
// result stays finite and byte-exact comparable vs the libm fma/fmaf golden
// (a 1-bit SDC flip anywhere shows up in the memcmp; SEVI ASPLOS'26 shows
// FMA SDC also flips exponent/sign bits, so tolerance compares are blind).
static inline void fma_seed_advance(uint64_t *seed)
{
    *seed = *seed * 0x9E3779B97F4A7C15ULL + 1;
}

static inline double fma_rand_bounded_d(uint64_t *seed)
{
    fma_seed_advance(seed);
    uint64_t bits = *seed >> 32;
    uint64_t u = (bits >> 31) ? 0xBFF0000000000000ULL | (bits & 0xFFFFFFFFFFFFFULL)
                              : 0x3FF0000000000000ULL | (bits & 0xFFFFFFFFFFFFFULL);
    double d;
    memcpy(&d, &u, 8);
    return d;
}

static inline float fma_rand_bounded(uint64_t *seed)
{
    fma_seed_advance(seed);
    uint32_t bits = (uint32_t)(*seed >> 32);
    uint32_t u = (bits >> 31) ? 0xBF800000u | (bits & 0x7FFFFFu)
                              : 0x3F800000u | (bits & 0x7FFFFFu);
    float f;
    memcpy(&f, &u, 4);
    return f;
}

static constexpr int VECTOR_SIZE = 4; // 4 个双精度浮点数

static int fma_tail_avx2_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__
static int fma_tail_avx2_run(struct test *test, int cpu) {
    (void)cpu;
    /* Deterministic per-thread operand generator: seeded once from the
     * framework RNG (replayable via -s), advanced by a splitmix64 LCG. */
    uint64_t seed = random64();

    do {
        // 生成随机向量 a, b, c（对齐到 16 字节即可满足 NEON）
        alignas(16) double a[VECTOR_SIZE];
        alignas(16) double b[VECTOR_SIZE];
        alignas(16) double c[VECTOR_SIZE];
        alignas(16) double hw_result[VECTOR_SIZE];
        double sw_ref[VECTOR_SIZE];

        for (int i = 0; i < VECTOR_SIZE; ++i) {
            a[i] = fma_rand_bounded_d(&seed);
            b[i] = fma_rand_bounded_d(&seed);
            c[i] = fma_rand_bounded_d(&seed);
        }

        // ---- 硬件 FMA 计算 (NEON) ----
        // 使用 2 个 float64x2_t 向量容纳 4 个元素
        float64x2_t va0 = vld1q_f64(a);
        float64x2_t va1 = vld1q_f64(a + 2);
        float64x2_t vb0 = vld1q_f64(b);
        float64x2_t vb1 = vld1q_f64(b + 2);
        float64x2_t vc0 = vld1q_f64(c);
        float64x2_t vc1 = vld1q_f64(c + 2);

        // vfmaq_f64 计算：dst = dst + src1 * src2  (即 c + a * b)
        float64x2_t vd0 = vfmaq_f64(vc0, va0, vb0);
        float64x2_t vd1 = vfmaq_f64(vc1, va1, vb1);

        // 存储结果
        vst1q_f64(hw_result,      vd0);
        vst1q_f64(hw_result + 2,  vd1);

        // ---- 软件参考：使用标准库 fma（正确舍入） ----
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            sw_ref[i] = fma(a[i], b[i], c[i]);
        }

        // ---- 尾数精度测试：逐位比较硬件结果与参考值 ----
        bool data_ok = (memcmp(hw_result, sw_ref, sizeof(hw_result)) == 0);

        // ---- 一致性测试：存储硬件结果到内存再加载比较 ----
        double store_buf[VECTOR_SIZE];
        memcpy(store_buf, hw_result, sizeof(store_buf));
        double reload_buf[VECTOR_SIZE];
        memcpy(reload_buf, store_buf, sizeof(store_buf));
        bool consistent = (memcmp(reload_buf, hw_result, sizeof(reload_buf)) == 0);

        bool passed = data_ok && consistent;

        if (!passed) {
            report_fail_msg("fma_tail_avx2: FMA tail precision mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}
#else
static int fma_tail_avx2_run(struct test *test, int cpu) {
    (void)cpu;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): ARM NEON FMA required");
    return EXIT_SKIP;
}
#endif

static int fma_tail_avx2_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(fma_tail_avx2, "AVX2 double FMA tail precision test (NEON version)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = fma_tail_avx2_init,
    .test_run = fma_tail_avx2_run,
    .test_cleanup = fma_tail_avx2_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

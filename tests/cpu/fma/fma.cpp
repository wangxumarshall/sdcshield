#include <sandstone.h>
#include <cstdint>
#include <cmath>
#include <cstring>

#ifdef __aarch64__
#include <arm_neon.h>          // ARM NEON 头文件
#endif

static constexpr size_t VECTOR_SIZE = 8;   // 8 个单精度浮点数

// Bounded deterministic operand generator (|x| < 2, high-entropy mantissa).
// Bounded operands keep every result finite so the byte-exact memcmp vs the
// libm fmaf golden is valid — SEVI (ASPLOS'26) shows FMA SDC flips exponent
// and sign bits too (relative errors up to 10240x), so any tolerance-based
// compare would be blind to a real SDC. Seeded from the framework RNG once
// per thread, replayable via -s.
static inline void fma_seed_advance(uint64_t *seed)
{
    *seed = *seed * 0x9E3779B97F4A7C15ULL + 1;
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

// 软件参考：使用 fmaf 逐元素计算（单次舍入，与 NEON vfmaq 同为融合单舍入，
// 位级一致 —— fmatail 家族既有 memcmp 先例证明）
static void software_fma(const float *a, const float *b, const float *c, float *ref) {
    for (size_t i = 0; i < VECTOR_SIZE; ++i) {
        ref[i] = fmaf(a[i], b[i], c[i]);
    }
}

struct TestData {};

static int fma_init(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

#ifdef __aarch64__
static int fma_run(struct test *test, int cpu) {
    (void)cpu;
    // 每个线程独立分配数据（栈上，16字节对齐即可满足 NEON）
    alignas(16) float a[VECTOR_SIZE];
    alignas(16) float b[VECTOR_SIZE];
    alignas(16) float c[VECTOR_SIZE];
    alignas(16) float result[VECTOR_SIZE];

    // 确定性操作数生成器：框架 RNG 种子（-s 可重放）
    uint64_t seed = random64();

    do {
        // 生成随机向量 a, b, c（有界高熵档，|x| < 2）
        for (size_t i = 0; i < VECTOR_SIZE; ++i) {
            a[i] = fma_rand_bounded(&seed);
            b[i] = fma_rand_bounded(&seed);
            c[i] = fma_rand_bounded(&seed);
        }

        // ---- 硬件 FMA 计算 (NEON) ----
        // 将数据分成低4个和高4个元素加载到两个 NEON 向量
        float32x4_t va_low  = vld1q_f32(a);
        float32x4_t va_high = vld1q_f32(a + 4);
        float32x4_t vb_low  = vld1q_f32(b);
        float32x4_t vb_high = vld1q_f32(b + 4);
        float32x4_t vc_low  = vld1q_f32(c);
        float32x4_t vc_high = vld1q_f32(c + 4);

        // vfmaq_f32 计算：dst = dst + src1 * src2  (即 c + a * b)
        float32x4_t vd_low  = vfmaq_f32(vc_low, va_low, vb_low);
        float32x4_t vd_high = vfmaq_f32(vc_high, va_high, vb_high);

        // 存储结果
        vst1q_f32(result,      vd_low);
        vst1q_f32(result + 4,  vd_high);

        // ---- 软件参考计算 ----
        float ref[VECTOR_SIZE];
        software_fma(a, b, c, ref);

        // ---- 字节精确比较：任何位的翻转（含指数/符号位）都是 SDC ----
        bool data_ok = (memcmp(result, ref, sizeof(result)) == 0);

        // 一致性测试：存储硬件结果到内存再加载比较
        float store_buf[VECTOR_SIZE];
        memcpy(store_buf, result, sizeof(store_buf));
        float reload_buf[VECTOR_SIZE];
        memcpy(reload_buf, store_buf, sizeof(store_buf));
        bool consistent = (memcmp(reload_buf, result, sizeof(reload_buf)) == 0);

        if (!(data_ok && consistent)) {
            report_fail_msg("fma: FMA result mismatch or consistency failure");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}
#else
static int fma_run(struct test *test, int cpu) {
    (void)cpu;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): ARM NEON FMA required");
    return EXIT_SKIP;
}
#endif

static int fma_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(fma, "FMA instruction basic test (NEON single-precision)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = fma_init,
    .test_run = fma_run,
    .test_cleanup = fma_finish,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

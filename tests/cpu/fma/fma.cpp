#include <sandstone.h>
#include <cstdint>
#include <cmath>
#include <random>
#include <cstring>

#ifdef __aarch64__
#include <arm_neon.h>          // ARM NEON 头文件
#endif

static constexpr size_t VECTOR_SIZE = 8;   // 8 个单精度浮点数

// 软件参考：使用 fmaf 逐元素计算（单次舍入）
static void software_fma(const float *a, const float *b, const float *c, float *ref) {
    for (int i = 0; i < VECTOR_SIZE; ++i) {
        ref[i] = fmaf(a[i], b[i], c[i]);
    }
}

// 字节精确比较：vfmaq_f32（硬件单次舍入 FMA）与 libm fmaf（同为 IEEE-754
// 单次舍入 FMA）必须位一致 —— fmatail_*/fsu_byteexact 兄弟测试已验证此模式
// 可行。原先的 1e-6/1e-7 容差会吞掉 1-ULP 的尾数位翻转（这正是本测试要
// 抓的 SDC）。
static bool byte_exact_equal(const float *x, const float *y) {
    return memcmp(x, y, VECTOR_SIZE * sizeof(float)) == 0;
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

    /* randomization hardening H9'/P12: framework RNG (per-thread stream,
     * -s reproducible) replaces std::mt19937; range [-100.0f, 100.0f) mapped
     * from frandom's [0,1). */
    auto dist = []() { return frandomf_scale((float)(100.0f) - (float)(-100.0f)) + (float)(-100.0f); };

    do {
        // 生成随机向量 a, b, c
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            a[i] = dist();
            b[i] = dist();
            c[i] = dist();
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

        // 比较硬件结果与参考（字节精确）
        bool data_ok = byte_exact_equal(result, ref);

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

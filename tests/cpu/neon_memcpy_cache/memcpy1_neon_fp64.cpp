/*  memcpy1_neon_fp64 — FP64 input variant of neon_memcpy_cache/memcpy1_neon.cpp
 *  (2026-11-13 激发谱系扩展): 父版 256B 缓冲的逐字节随机 uint8 换成 32 个
 *  有界随机 double —— 同块尺寸(32 × 8B = 256B, L1 访问行为一致),数据位
 *  模式换 fp64 高精度浮点语义。fill 走 gather_f64/gather_i32_fp64 家族的
 *  frandom_scale 有界口径(值域 [-1000, 1000), B3-007: 框架 per-thread RNG,
 *  -s 可复现);复制仍走 arm_add_neon::neon_copy 显式 vld1q/vst1q 字节流
 *  (NEON 数据通路归因不变;LD1/ST1 是纯搬运指令,不执行 FP 运算,故随机
 *  f64 位模式无 SNaN quieting / FP 异常风险),memcmp 字节级校验(位模式
 *  比对,任何 bit 翻转即 fail)。
 *  诚实性: 有界 f64 不含 NaN/Inf/subnormal, exponent 域窄于父版 uint8
 *  全谱随机 —— 位翻转覆盖以 mantissa 52 位为主,与 fp64 family 统一口径。
 */
#include <sandstone.h>
#include "arm_add_neon_copy.h"
using arm_add_neon::neon_copy;          // 提供 DECLARE_TEST, report_fail_msg, test_time_condition 等
#include <cstring>              // memcmp
#include <cstddef>              // size_t

static constexpr size_t FP64_COUNT = 32;   // 32 × 8B = 256B,与父版 BLOCK_SIZE 同块尺寸

static int memcpy1_fp64_init(struct test *test) {
    (void)test;   // 消除未使用参数警告
    return EXIT_SUCCESS;
}

static int memcpy1_fp64_run(struct test *test, int cpu) {
    double src[FP64_COUNT];
    double dst[FP64_COUNT];
    /* 有界 f64 — gather_f64 家族 frandom_scale 风格(框架 per-thread 流, -s 可复现) */
    auto dist = []() { return frandom_scale((double)(1000.0) - (double)(-1000.0)) + (double)(-1000.0); };

    do {
        // 生成随机 fp64 数据
        for (size_t i = 0; i < FP64_COUNT; ++i) {
            src[i] = dist();
        }

        // 执行复制(256B 显式 NEON 字节流,LD1/ST1 不解析 FP 语义)
        neon_copy(dst, src, sizeof src);

        // 校验(字节级:位模式比对,任何 bit 翻转即 fail)
        if (memcmp(dst, src, sizeof src) != 0) {
            report_fail_msg("memcpy1_neon_fp64: data mismatch after copy");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int memcpy1_fp64_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

// 使用框架已有的 group_math 组(避免自定义组的不完整类型问题)
DECLARE_TEST(memcpy1_neon_fp64, "Copy small memory blocks with random fp64 data")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = memcpy1_fp64_init,
    .test_run = memcpy1_fp64_run,
    .test_cleanup = memcpy1_fp64_finish,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

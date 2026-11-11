/*  NEON variant of tests/cpu/memory/memcpy1_neon.cpp — auto-derived by the 2026-11-02
 *  cache/memory NEON pass: load-point neon_copy() calls replaced with the explicit
 *  vld1q/vst1q stream in arm_add_neon_copy.h (bypasses the libc IFUNC dispatcher,
 *  keeps the NEON datapath attribution).  Original test untouched; identifiers
 *  carry the _neon suffix to avoid ODR clashes with the original TU.
 */
#include <sandstone.h>
#include "arm_add_neon_copy.h"
using arm_add_neon::neon_copy;          // 提供 DECLARE_TEST, report_fail_msg, test_time_condition 等
#include <cstring>              // memcpy, memcmp
#include <cstdint>              // uint8_t
#include <cstddef>              // size_t

static constexpr size_t BLOCK_SIZE = 256;

static int memcpy1_init(struct test *test) {
    (void)test;   // 消除未使用参数警告
    return EXIT_SUCCESS;
}

static int memcpy1_run(struct test *test, int cpu) {
    uint8_t src[BLOCK_SIZE];
    uint8_t dst[BLOCK_SIZE];
    /* randomization hardening (B3-007): framework RNG (per-thread stream,
     * -s reproducible) replaces std::mt19937; range [0, 255]. */
    auto dist = []() { return (uint8_t)((0) + (int64_t)(random64() % (uint64_t)((255) - (0) + 1))); };

    do {
        // 生成随机数据
        for (size_t i = 0; i < BLOCK_SIZE; ++i) {
            src[i] = dist();
        }

        // 执行复制
        neon_copy(dst, src, BLOCK_SIZE);

        // 校验（失败经 report_fail_msg 正规上报——2026-11-11 战役实证逐迭代
        // fprintf(stderr) 在 5min×125 线程下产出 GB 级 stderr：既触发框架打印
        // 路径崩溃，又以 I/O 拖垮激发强度，故删除该调试打印）
        if (memcmp(dst, src, BLOCK_SIZE) != 0) {
            report_fail_msg("memcpy1_neon: data mismatch after copy");
            return EXIT_FAILURE;
        }

    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int memcpy1_finish(struct test *test) {
    (void)test;
    return EXIT_SUCCESS;
}

// 使用框架已有的 group_math 组（避免自定义组的不完整类型问题）
DECLARE_TEST(memcpy1_neon, "Copy small memory blocks with random data")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = memcpy1_init,
    .test_run = memcpy1_run,
    .test_cleanup = memcpy1_finish,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

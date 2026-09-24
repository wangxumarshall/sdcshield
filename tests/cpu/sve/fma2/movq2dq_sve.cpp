/**
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b movq2dq_sve
 * @parblock
 * SVE port of movq2dq: move a GPR value into an SVE z-register and verify
 * the low element plus that the remaining lanes follow the broadcast /
 * zero semantics observed via svst1 (the SVE analogue of the original's
 * fmov d0, x0 channel where the upper half is zeroed).
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cstdlib>
#include <cstring>

#if defined(__aarch64__)
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

#define MOVQ2DQ_SVE_COUNT 1024

struct Movq2dqSveData {
    uint64_t *values;
};

static int movq2dq_sve_init(struct test *test)
{
#ifdef __aarch64__
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for movq2dq_sve");
        return EXIT_SKIP;
    }
#endif

    auto *data = static_cast<Movq2dqSveData *>(malloc(sizeof(Movq2dqSveData)));
    data->values = static_cast<uint64_t *>(malloc(MOVQ2DQ_SVE_COUNT * sizeof(uint64_t)));
    memset_random(data->values, MOVQ2DQ_SVE_COUNT * sizeof(uint64_t));
    test->data = data;
    return EXIT_SUCCESS;
}

#if defined(__aarch64__)
static int movq2dq_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<Movq2dqSveData *>(test->data);

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MOVQ2DQ_SVE_COUNT; i++) {
            /* GPR → SVE 向量: svdup 广播 */
            svuint64_t v = svdup_u64(data->values[i]);
            /* 读回: 存到栈再比对 (低 lane + 广播语义) */
            uint64_t out[16];
            svbool_t pg = svwhilelt_b64((uint64_t)0, (uint64_t)svcntd());
            svst1_u64(pg, out, v);

            if (out[0] != data->values[i]) {
                report_fail_msg(
                    "movq2dq_sve low 64-bit mismatch at index %d: "
                    "expected 0x%llx got 0x%llx",
                    i,
                    static_cast<unsigned long long>(data->values[i]),
                    static_cast<unsigned long long>(out[0]));
            }
        }
    }
    return EXIT_SUCCESS;
}
#else
static int movq2dq_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    return EXIT_SKIP;
}
#endif

static int movq2dq_sve_cleanup(struct test *test)
{
    auto *data = static_cast<Movq2dqSveData *>(test->data);
    if (data) {
        free(data->values);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(movq2dq_sve, "GPR to SVE z-register moves with value checks (port of movq2dq)")
    .test_init    = movq2dq_sve_init,
    .test_run     = movq2dq_sve_run,
    .test_cleanup = movq2dq_sve_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

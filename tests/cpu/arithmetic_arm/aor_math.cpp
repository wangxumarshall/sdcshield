/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b aor_math
 * @parblock
 * Stress the floating-point transcendental path using the Arm
 * Optimized Routines' pi-multiple sincospi function, which computes
 * sin(pi*x) and cos(pi*x) in one call via an aarch64-optimised
 * polynomial approximation.  The symbol name is unique (no clash
 * with the system libm).  A table of random float arguments is
 * generated at init and the golden (sin,cos) pair is computed once.
 * During the run the same function is re-evaluated and the results
 * are compared bit-exactly against the golden; a mismatch indicates a
 * silent corruption in the FPU / FMA pipeline.
 * @endparblock
 */

#include <sandstone.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <mathlib.h>
#ifdef __cplusplus
}
#endif

#define AOR_MATH_COUNT (1 << 13)   /* 8192 arguments */

namespace {
struct aor_math_data {
    float *args;
    float *golden_sin;
    float *golden_cos;
};
}

#define CAST(_x) static_cast<struct aor_math_data *>(_x)

static int aor_math_init(struct test *test)
{
    aor_math_data *data = new aor_math_data;
    data->args       = (float *)aligned_alloc_safe(64, AOR_MATH_COUNT * sizeof(float));
    data->golden_sin = (float *)aligned_alloc_safe(64, AOR_MATH_COUNT * sizeof(float));
    data->golden_cos = (float *)aligned_alloc_safe(64, AOR_MATH_COUNT * sizeof(float));

    for (size_t i = 0; i < AOR_MATH_COUNT; ++i) {
        /* arguments in [-4, 4) -- well within the primary range of the
         * pi-multiple functions so we exercise the polynomial core. */
        uint32_t r = random32();
        float x = (float)(int32_t)r / (float)(1u << 30);
        data->args[i] = x;
        float s, c;
        arm_math_sincospif(x, &s, &c);
        data->golden_sin[i] = s;
        data->golden_cos[i] = c;
    }
    test->data = data;
    return EXIT_SUCCESS;
}

static int aor_math_run(struct test *test, int cpu)
{
    auto data = CAST(test->data);
    do {
        for (size_t i = 0; i < AOR_MATH_COUNT; ++i) {
            float s, c;
            arm_math_sincospif(data->args[i], &s, &c);
            if (memcmp(&s, &data->golden_sin[i], sizeof(float)) != 0) {
                unsigned int sbits, gbits;
                memcpy(&sbits, &s, sizeof sbits);
                memcpy(&gbits, &data->golden_sin[i], sizeof gbits);
                report_fail_msg("AOR sinpi mismatch at idx %zu (x=%g): "
                                "0x%08x vs 0x%08x", i, (double)data->args[i],
                                sbits, gbits);
            }
            if (memcmp(&c, &data->golden_cos[i], sizeof(float)) != 0) {
                unsigned int cbits, gcbits;
                memcpy(&cbits, &c, sizeof cbits);
                memcpy(&gcbits, &data->golden_cos[i], sizeof gcbits);
                report_fail_msg("AOR cospi mismatch at idx %zu (x=%g): "
                                "0x%08x vs 0x%08x", i, (double)data->args[i],
                                cbits, gcbits);
            }
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int aor_math_cleanup(struct test *test)
{
    aor_math_data *data = CAST(test->data);
    free(data->args);
    free(data->golden_sin);
    free(data->golden_cos);
    delete data;
    return EXIT_SUCCESS;
}

DECLARE_TEST(aor_math, "Arm Optimized Routines sincospi transcendental stress vs golden")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = aor_math_init,
  .test_run = aor_math_run,
  .test_cleanup = aor_math_cleanup,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

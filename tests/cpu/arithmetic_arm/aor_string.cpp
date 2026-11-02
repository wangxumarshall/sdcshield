/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b aor_string
 * @parblock
 * Stress the load/store and memory-subsystem paths using the Arm
 * Optimized Routines' aarch64 memcpy / memcmp implementations.  A
 * buffer of random data is generated at init (snapshot kept as the
 * golden).  During the run the whole buffer is copied through a
 * scratch buffer with __memcpy_aarch64 and then verified with
 * __memcmp_aarch64 against the golden.  A mismatch indicates a silent
 * corruption in the vector load/store or cache coherency path of the
 * core running the thread.
 * @endparblock
 */

#include <sandstone.h>
#ifdef __cplusplus
extern "C" {
#endif
#include <stringlib.h>
#ifdef __cplusplus
}
#endif

#define AOR_BUF_BYTES (1 << 18)   /* 256 KiB, larger than L1/L2 */

namespace {
struct aor_string_data {
    uint8_t *golden;   /* immutable source of truth */
    uint8_t *scratch;  /* destination for each copy */
};
}

#define CAST(_x) static_cast<struct aor_string_data *>(_x)

static int aor_string_init(struct test *test)
{
    aor_string_data *data = new aor_string_data;
    data->golden  = (uint8_t *)aligned_alloc_safe(64, AOR_BUF_BYTES);
    data->scratch = (uint8_t *)aligned_alloc_safe(64, AOR_BUF_BYTES);
    for (size_t i = 0; i < AOR_BUF_BYTES; ++i)
        data->golden[i] = static_cast<uint8_t>(random32());
    test->data = data;
    return EXIT_SUCCESS;
}

static int aor_string_run(struct test *test, int cpu)
{
    auto data = CAST(test->data);
    do {
        /* copy golden -> scratch using Arm Optimized Routines memcpy */
        __memcpy_aarch64(data->scratch, data->golden, AOR_BUF_BYTES);
        /* verify the copy using Arm Optimized Routines memcmp */
        if (__memcmp_aarch64(data->scratch, data->golden, AOR_BUF_BYTES) != 0) {
            /* locate first differing byte for the failure message */
            for (size_t i = 0; i < AOR_BUF_BYTES; ++i) {
                if (data->scratch[i] != data->golden[i]) {
                    report_fail_msg("AOR memcpy/memcmp mismatch at offset %zu: "
                                    "0x%02x vs 0x%02x",
                                    i, (unsigned)data->scratch[i],
                                    (unsigned)data->golden[i]);
                }
            }
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int aor_string_cleanup(struct test *test)
{
    aor_string_data *data = CAST(test->data);
    free(data->golden);
    free(data->scratch);
    delete data;
    return EXIT_SUCCESS;
}

DECLARE_TEST(aor_string, "Arm Optimized Routines memcpy/memcmp round-trip (256KiB) vs golden")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = aor_string_init,
  .test_run = aor_string_run,
  .test_cleanup = aor_string_cleanup,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

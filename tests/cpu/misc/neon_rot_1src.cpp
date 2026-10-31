/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_1src
 * @parblock
 * NEON variant with 1 source load only (vs neon_rot_2src's 2): the second
 * vector source is replaced by a compile-time constant vector. Tests whether
 * the ">=2 source loads" threshold (established on the scalar path: 1src = 0
 * fail) also holds on the NEON datapath. Serves as the in-campaign negative
 * control for the NEON matrix. Skeleton otherwise identical to neon_rot_2src.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_1SRC_COUNT 1024

struct NeonRot1srcData {
    uint64x2_t *srcA;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

/* store via NEON intrinsic (compiles to str q); reload via inline asm —
 * REQUIRED: objdump showed the compiler eliminates the intrinsic reload
 * (reuses the stored register for dst, no ldr emitted). The asm boundary
 * makes the back-to-back str q -> ldr q pair guaranteed in the binary. */
static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int neon_rot_1src_init(struct test *test) {
    auto *data = static_cast<NeonRot1srcData *>(malloc(sizeof(NeonRot1srcData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_1SRC_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_1SRC_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_1SRC_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition with the constant second operand */
    for (int i = 0; i < NEON_ROT_1SRC_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_1src_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRot1srcData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_1SRC_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_1SRC_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_1SRC_COUNT; i++) {
            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
                case 1: res = veorq_u64(a, vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
                case 2: res = vandq_u64(a, vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
                case 3: res = vorrq_u64(a, vdupq_n_u64(0x5A5A5A5A5A5A5A5AULL)); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], load_vec(&temp[i]));
        }

        if (memcmp(dst, data->expected, NEON_ROT_1SRC_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_1src data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_1src_cleanup(struct test *test) {
    auto *data = static_cast<NeonRot1srcData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_1src, "NEON variant: 1 source vector load + rotating ALU vs constant + vec store/reload/store (threshold control)")
    .test_init    = neon_rot_1src_init,
    .test_run     = neon_rot_1src_run,
    .test_cleanup = neon_rot_1src_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

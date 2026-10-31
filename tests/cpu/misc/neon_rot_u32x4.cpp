/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_u32x4
 * @parblock
 * NEON lane-width intermediate point: uint32x4_t (4x32b lanes) between
 * uint64x2 (2x64b, 26-33 fail/30min) and uint8x16 (16x8b, 2 fail/30min).
 * Same 16B per operand/instruction width, only lane structure changes.
 * Maps the lane-width vs trigger-rate curve at a second interior point to
 * decide whether the 64b-lane binding is a threshold (u32x4 collapses like
 * u8x16) or a gradient (u32x4 lands in between). Op set is the 32b-lane
 * rotation {vaddq,veorq,vandq,vorrq}_u32. Skeleton otherwise identical to
 * neon_rot_2src.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_U32X4_COUNT 1024   /* uint32x4 elements -> 16KB per buffer */

struct NeonRotU32x4Data {
    uint32x4_t *srcA;
    uint32x4_t *srcB;
    uint32x4_t *expected;
};

#if defined(__aarch64__)

/* store via NEON intrinsic (compiles to str q); reload via inline asm —
 * REQUIRED: objdump showed the compiler eliminates the intrinsic reload
 * (reuses the stored register for dst, no ldr emitted). The asm boundary
 * makes the back-to-back str q -> ldr q pair guaranteed in the binary. */
static inline void store_vec(void *addr, uint32x4_t val) {
    vst1q_u32(static_cast<uint32_t *>(addr), val);
}

static inline uint32x4_t load_vec(const void *addr) {
    uint32x4_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int neon_rot_u32x4_init(struct test *test) {
    auto *data = static_cast<NeonRotU32x4Data *>(malloc(sizeof(NeonRotU32x4Data)));
    data->srcA = static_cast<uint32x4_t *>(aligned_alloc(64, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t)));
    data->srcB = static_cast<uint32x4_t *>(aligned_alloc(64, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t)));
    data->expected = static_cast<uint32x4_t *>(aligned_alloc(64, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t)));

    memset_random(data->srcA, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t));
    memset_random(data->srcB, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_U32X4_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u32(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u32(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u32(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u32(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_u32x4_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotU32x4Data *>(test->data);

    uint32x4_t *temp = static_cast<uint32x4_t *>(aligned_alloc(64, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t)));
    uint32x4_t *dst  = static_cast<uint32x4_t *>(aligned_alloc(64, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_U32X4_COUNT; i++) {
            uint32x4_t a = load_vec(&data->srcA[i]);
            uint32x4_t b = load_vec(&data->srcB[i]);
            uint32x4_t res;

            switch (i % 4) {
                case 0: res = vaddq_u32(a, b); break;
                case 1: res = veorq_u32(a, b); break;
                case 2: res = vandq_u32(a, b); break;
                case 3: res = vorrq_u32(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], load_vec(&temp[i]));
        }

        if (memcmp(dst, data->expected, NEON_ROT_U32X4_COUNT * sizeof(uint32x4_t)) != 0) {
            report_fail_msg("neon_rot_u32x4 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_u32x4_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotU32x4Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_u32x4, "NEON lane-width intermediate: uint32x4 (4x32b) 2-src load + rotating ALU + store/reload")
    .test_init    = neon_rot_u32x4_init,
    .test_run     = neon_rot_u32x4_run,
    .test_cleanup = neon_rot_u32x4_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_u8x16
 * @parblock
 * NEON variant with 16-lane byte lanes (uint8x16_t) instead of 2-lane 64-bit
 * (uint64x2_t): same 16B per operand/instruction width, completely different
 * lane structure. Discriminates width-vs-lane hypotheses for the NEON firing
 * rate amplification (neon_rot_2src ~33 fail/30min = 1.7x scalar):
 *  - rate stays ~1.1/min  -> 16B op width is the amplifier (data-volume billing)
 *  - rate drops to ~0     -> the 2x64b lane structure is what matters
 *  - rate mid             -> mixed/interaction, needs follow-up
 * Op set is the byte-wise analogue of the rotation: {add (saturating? no —
 * plain wrapping via vaddq), eor, and, orr} on uint8x16. Skeleton otherwise
 * identical to neon_rot_2src.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_U8X16_COUNT 1024   /* uint8x16 elements -> 16KB per buffer */

struct NeonRotU8x16Data {
    uint8x16_t *srcA;
    uint8x16_t *srcB;
    uint8x16_t *expected;
};

#if defined(__aarch64__)

/* store via NEON intrinsic (compiles to str q); reload via inline asm —
 * REQUIRED: objdump showed the compiler eliminates the intrinsic reload
 * (reuses the stored register for dst, no ldr emitted). The asm boundary
 * makes the back-to-back str q -> ldr q pair guaranteed in the binary. */
static inline void store_vec_u8(void *addr, uint8x16_t val) {
    vst1q_u8(static_cast<uint8_t *>(addr), val);
}

static inline uint8x16_t load_vec_u8(const void *addr) {
    uint8x16_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int neon_rot_u8x16_init(struct test *test) {
    auto *data = static_cast<NeonRotU8x16Data *>(malloc(sizeof(NeonRotU8x16Data)));
    data->srcA = static_cast<uint8x16_t *>(aligned_alloc(64, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t)));
    data->srcB = static_cast<uint8x16_t *>(aligned_alloc(64, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t)));
    data->expected = static_cast<uint8x16_t *>(aligned_alloc(64, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t)));

    memset_random(data->srcA, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t));
    memset_random(data->srcB, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_U8X16_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u8(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u8(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u8(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u8(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_u8x16_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotU8x16Data *>(test->data);

    uint8x16_t *temp = static_cast<uint8x16_t *>(aligned_alloc(64, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t)));
    uint8x16_t *dst  = static_cast<uint8x16_t *>(aligned_alloc(64, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_U8X16_COUNT; i++) {
            uint8x16_t a = load_vec_u8(&data->srcA[i]);
            uint8x16_t b = load_vec_u8(&data->srcB[i]);
            uint8x16_t res;

            switch (i % 4) {
                case 0: res = vaddq_u8(a, b); break;
                case 1: res = veorq_u8(a, b); break;
                case 2: res = vandq_u8(a, b); break;
                case 3: res = vorrq_u8(a, b); break;
            }

            store_vec_u8(&temp[i], res);
            store_vec_u8(&dst[i], load_vec_u8(&temp[i]));
        }

        if (memcmp(dst, data->expected, NEON_ROT_U8X16_COUNT * sizeof(uint8x16_t)) != 0) {
            report_fail_msg("neon_rot_u8x16 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_u8x16_cleanup(struct test *test) {
    auto *NeonRotData = static_cast<NeonRotU8x16Data *>(test->data);
    auto *data = NeonRotData;
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_u8x16, "NEON variant: uint8x16 (16-lane byte) 2-src load + rotating ALU + store/reload — width-vs-lane probe")
    .test_init    = neon_rot_u8x16_init,
    .test_run     = neon_rot_u8x16_run,
    .test_cleanup = neon_rot_u8x16_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_ldr5
 * @parblock
 * Dead-load-count-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-12).  Continues the load-side gradient one step beyond ldr4:
 * the store side (2 str per element: temp + dst, back to back), the 2
 * useful source loads (srcA/srcB) and the rotating ALU (add/eor/and/orr
 * by i%4) are kept IDENTICAL to the rowmajor parent; the change is a
 * THIRD dead scratch load at the top of the loop body, reading an
 * independent scratch3 buffer (value discarded, never used).
 * Per element this makes 5 ldr = 3 dead (scratch1/2/3) + 2 useful:
 *   parent  3 ldr (1 dead + 2 useful)  -> 149-276 fail/30min (3 rounds)
 *   ldr4    4 ldr (2 dead + 2 useful)  -> 29 fail/24.9min, all on 179
 *        (rate ~1.16/min, 4-8x below parent; run truncated by host
 *        shutdown at 24.9/30 min)
 *   ldr5    5 ldr (3 dead + 2 useful)  -> this test
 *   lsqfill 11 ldr (9 dead + 2 useful) -> 0 fail, 2 rounds (large
 *        instruction-count/layout confound: 8 fillers + accumulator)
 * Question this point answers: does the load-side rate keep DECAYING
 * monotonically (3 -> 4 -> 5) the way the store side does (2 -> 3 -> 4
 * collapses to 0), or does it plateau/abruptly die between 4 and 5?
 * Same known risk as str3/str4/ldr4: any instruction-count change
 * perturbs scheduling; the gradient SHAPE is the informative signal.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_LDR5_COUNT 1024

struct NeonRotLdrAtTopLdr5Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch1;
    uint64x2_t *scratch2;
    uint64x2_t *scratch3;
    uint64x2_t *expected;
};

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

#endif

static int neon_rot_ldr_at_top_ldr5_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopLdr5Data *>(malloc(sizeof(NeonRotLdrAtTopLdr5Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));
    data->scratch1 = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));
    data->scratch2 = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));
    data->scratch3 = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch1, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch2, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch3, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_LDR5_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_ldr5_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopLdr5Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_LDR5_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_LDR5_COUNT - 1 - n);

            uint64x2_t unused_x1 = load_vec(&data->scratch1[i]);
            (void)unused_x1;
            uint64x2_t unused_x2 = load_vec(&data->scratch2[i]);
            (void)unused_x2;
            uint64x2_t unused_x3 = load_vec(&data->scratch3[i]);
            (void)unused_x3;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_LDR5_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_ldr5 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_ldr5_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopLdr5Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch1);
        free(data->scratch2);
        free(data->scratch3);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_ldr5, "Dead-load-count variant of ldr_at_top_rowmajor: 5 ldr per element (3 dead scratch + srcA/srcB) at constant 2-str-back-to-back/rot-ALU skeleton; load-side gradient point between ldr4 (4 ldr, decaying) and lsqfill (11 ldr, dead)")
    .test_init    = neon_rot_ldr_at_top_ldr5_init,
    .test_run     = neon_rot_ldr_at_top_ldr5_run,
    .test_cleanup = neon_rot_ldr_at_top_ldr5_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_str3
 * @parblock
 * Store-count-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-11).  The load side (3 ldr per element: top dead scratch load +
 * 2-src srcA/srcB) and the rotating ALU (add/eor/and/orr by i%4) are kept
 * IDENTICAL to the rowmajor parent; the only change is a THIRD store per
 * element: an extra dead str of the ALU result to an independent junk
 * buffer (junk is never read back).  Together with str4 this builds a
 * store-count gradient: 2 (parent) / 3 / 4 str per element at constant
 * 3 ldr + rot ALU.
 *   If trigger rate survives across 2->3->4  -> store count is a free
 *      dimension (AGU/LSU store-side pressure does not gate the fault)
 *   If it collapses at 3 (or between 3 and 4) -> the 2-str back-to-back
 *      adjacency / timing budget is part of the trigger recipe
 * Note the known risk: any instruction-count change perturbs scheduling
 * (see the unaligned/nop precedents); a 0-fail here is therefore
 * ambiguous between "store pressure kills it" and "layout shift kills
 * it" -- str4 vs str3 shares the layout-shift confound, so the gradient
 * SHAPE (monotone collapse vs cliff) is the informative signal.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_STR3_COUNT 1024

struct NeonRotLdrAtTopStr3Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
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

static int neon_rot_ldr_at_top_str3_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr3Data *>(malloc(sizeof(NeonRotLdrAtTopStr3Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_STR3_COUNT; i++) {
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

static int neon_rot_ldr_at_top_str3_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopStr3Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *junk = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_STR3_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_STR3_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

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
            store_vec(&junk[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_STR3_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_str3 data miscompare");
        }
    }

    free(dst);
    free(junk);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_str3_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr3Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_str3, "Store-count variant of ldr_at_top_rowmajor: 3 str per element (2 useful + 1 dead junk str) at constant 3-ldr/rot-ALU skeleton")
    .test_init    = neon_rot_ldr_at_top_str3_init,
    .test_run     = neon_rot_ldr_at_top_str3_run,
    .test_cleanup = neon_rot_ldr_at_top_str3_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

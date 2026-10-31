/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_str4
 * @parblock
 * Store-count-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-11).  Identical to neon_rot_ldr_at_top_str3 except that a
 * FOURTH store per element is added: two dead junk str of the ALU result
 * to two independent junk buffers (never read back).  Load side (3 ldr:
 * top dead scratch + 2-src srcA/srcB) and rotating ALU (add/eor/and/orr
 * by i%4) unchanged from the rowmajor parent.  Store-count gradient
 * point: 2 (parent) / 3 (str3) / 4 (str4).
 *   Interpret together with str3: a monotone rate decay across 2->3->4
 *   reads as store-side pressure/timing erosion; a cliff (parent fires,
 *   str3 dead, str4 irrelevant) reads as the 2-str adjacency being part
 *   of the recipe.  Layout-shift confound is shared by both variants.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_STR4_COUNT 1024

struct NeonRotLdrAtTopStr4Data {
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

static int neon_rot_ldr_at_top_str4_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr4Data *>(malloc(sizeof(NeonRotLdrAtTopStr4Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_STR4_COUNT; i++) {
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

static int neon_rot_ldr_at_top_str4_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopStr4Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *junk1 = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *junk2 = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_STR4_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_STR4_COUNT - 1 - n);

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
            store_vec(&junk1[i], res);
            store_vec(&junk2[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_STR4_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_str4 data miscompare");
        }
    }

    free(dst);
    free(junk2);
    free(junk1);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_str4_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopStr4Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_str4, "Store-count variant of ldr_at_top_rowmajor: 4 str per element (2 useful + 2 dead junk str) at constant 3-ldr/rot-ALU skeleton")
    .test_init    = neon_rot_ldr_at_top_str4_init,
    .test_run     = neon_rot_ldr_at_top_str4_run,
    .test_cleanup = neon_rot_ldr_at_top_str4_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

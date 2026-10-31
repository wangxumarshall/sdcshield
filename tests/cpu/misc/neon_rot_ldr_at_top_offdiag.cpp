/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_offdiag
 * @parblock
 * Address-interleave dimension variant of neon_rot_ldr_at_top (2026-09-08).
 *
 * The parent maps its 3 streams (scratch, srcA, srcB) as three separate
 * arrays, so at a given index i all three loads hit DIFFERENT cache sets;
 * the pair (srcA[i], srcB[i]) is 16KB apart. This variant interleaves the
 * two source streams in one buffer with a 2:1 stride pattern:
 *   buf[3i+0] = scratch-like filler, buf[3i+1] = A_i, buf[3i+2] = B_i
 * so the two source loads of an iteration land ADJACENT in memory (32B
 * apart, same 64B line on a good fraction of indices) while scratch stays
 * a separate array (3rd-ldr-at-top semantics preserved). Same total
 * footprint (3 x 16KB), same instruction mix (3x ldr q, ALU, 2x str q
 * back-to-back), same rotation (add/eor/and/orr).
 *   trigger survives   -> the recipe does not require source loads to be
 *      in different cache sets / pages; adjacent-line source pairs are
 *      enough (constrains bank/set-conflict hypotheses)
 *   trigger dies       -> source-load address separation (set/page spread)
 *      is part of the necessary conditions (new necessary condition)
 * Golden precomputed in init; memcmp in cold path.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT 1024

struct NeonRotLdrAtTopOffdiagData {
    uint64x2_t *interleaved;   /* 3*COUNT entries: filler, A_i, B_i */
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

static int neon_rot_ldr_at_top_offdiag_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopOffdiagData *>(malloc(sizeof(NeonRotLdrAtTopOffdiagData)));
    data->interleaved = static_cast<uint64x2_t *>(aligned_alloc(64, 3 * NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t)));

    memset_random(data->interleaved, 3 * NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT; i++) {
        uint64x2_t a = data->interleaved[3 * i + 1];
        uint64x2_t b = data->interleaved[3 * i + 2];
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(a, b); break;
            case 1: data->expected[i] = veorq_u64(a, b); break;
            case 2: data->expected[i] = vandq_u64(a, b); break;
            case 3: data->expected[i] = vorrq_u64(a, b); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_offdiag_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopOffdiagData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT; i++) {
            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            /* the two source loads are 32B apart — adjacent lines/sets */
            uint64x2_t a = load_vec(&data->interleaved[3 * i + 1]);
            uint64x2_t b = load_vec(&data->interleaved[3 * i + 2]);
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

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_OFFDIAG_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_offdiag data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_offdiag_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopOffdiagData *>(test->data);
    if (data) {
        free(data->interleaved);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_offdiag, "Interleave variant of ldr_at_top: source pairs adjacent in one buffer (address-separation necessity test)")
    .test_init    = neon_rot_ldr_at_top_offdiag_init,
    .test_run     = neon_rot_ldr_at_top_offdiag_run,
    .test_cleanup = neon_rot_ldr_at_top_offdiag_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

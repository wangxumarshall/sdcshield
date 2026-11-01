/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_lsqfill
 * @parblock
 * Load/Store-Queue-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-09).
 *
 * The parent has at most ~3 loads + 2 stores in flight per element, so the
 * LSU's Load Queue / Store Queue never come close to their (typical
 * 48-64 entry) capacity. This variant inserts 8 INDEPENDENT extra loads
 * (distinct addresses from a filler buffer, results folded into a dead
 * accumulator so nothing is DCE'd) between the scratch load and the two
 * source loads. Outstanding-load depth per element rises from ~3 to ~11,
 * pressing on LSQ allocation, store-to-load forwarding checks and memory
 * disambiguation width -- while the 2-src + rotating ALU + back-to-back
 * str/str trigger skeleton stays intact.
 *
 * NOTE: unlike tlbstress, this variant deliberately changes the hot-loop
 * instruction count / timing (that is the point -- queue depth). So it is
 * NOT byte-comparable to the parent; it answers "does LSQ depth pressure
 * raise/lower/suppress the trigger rate relative to the parent skeleton".
 * All extra loads go through the same asm ldr path as the skeleton loads.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT 1024
#define NEON_ROT_LDR_AT_TOP_LSQFILL_FILLER 8

struct NeonRotLdrAtTopLsqFillData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *filler;   /* 8 entries, distinct cache lines */
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

static int neon_rot_ldr_at_top_lsqfill_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopLsqFillData *>(malloc(sizeof(NeonRotLdrAtTopLsqFillData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)));
    /* filler: 8 entries each on its own 64B line */
    data->filler = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_FILLER * 64));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t));
    memset_random(data->filler, NEON_ROT_LDR_AT_TOP_LSQFILL_FILLER * 64);

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT; i++) {
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

static int neon_rot_ldr_at_top_lsqfill_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopLsqFillData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            /* 8 independent filler loads: each from its own cache line,
             * results folded into a dead accumulator (asm-written to a
             * stack slot at sweep end so nothing is DCE'd, but never in
             * the trigger's dependency chain). */
            uint64x2_t acc = unused_x;
            for (int k = 0; k < NEON_ROT_LDR_AT_TOP_LSQFILL_FILLER; k++) {
                uint64x2_t f = load_vec(reinterpret_cast<char *>(data->filler) + k * 64);
                acc = veorq_u64(acc, f);
            }

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

            /* sink the accumulator on the LAST element only (cold-ish,
             * one store per sweep, outside the per-element cost) */
            if (n == NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT - 1) {
                __asm__ volatile ("" :: "w"(acc) : "memory");
            }
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_LSQFILL_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_lsqfill data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_lsqfill_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopLsqFillData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->filler);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_lsqfill, "LSQ-dimension variant of ldr_at_top_rowmajor: 8 independent extra loads per element (dead-accumulator, no DCE) raise outstanding-load depth from ~3 to ~11; trigger skeleton (2-src + rot ALU + bb str/str) intact")
    .test_init    = neon_rot_ldr_at_top_lsqfill_init,
    .test_run     = neon_rot_ldr_at_top_lsqfill_run,
    .test_cleanup = neon_rot_ldr_at_top_lsqfill_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

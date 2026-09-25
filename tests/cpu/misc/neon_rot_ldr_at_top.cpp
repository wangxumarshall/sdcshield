/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top
 * @parblock
 * Position-vs-existence probe (plan ② of the 2026-09-04 attribution split):
 *   store_only:  ldrA  ldrB  ALU  str temp  str dst          (no 3rd load)
 *   fwd_probe:   ldrA  ldrB  ALU  str temp  ldr dst  str dst (3rd load BETWEEN the stores)
 *   ldr_at_top:  ldrA  ldrB  ldrX(scratch)  ALU  str temp  str dst
 * The third ldr is moved to the TOP of the iteration (before the source
 * loads), reading a scratch buffer. Same total instruction count and same
 * 3xldr+2xstr AGU mix as fwd_probe; the ONLY variable vs fwd_probe is the
 * third load's POSITION (between the stores vs top of iteration).
 *   ~14 (store_only level) -> position is the killer: an ldr between the
 *                              two stores closes the window
 *   ~0 (fwd_probe level)   -> the mere presence of the 3rd load / mixed
 *                              pressure kills it, position irrelevant
 * The scratch load's value is discarded, but it sits before the ALU — the
 * str temp / str dst pair remains back-to-back with no inserted memory op.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_COUNT 1024

struct NeonRotLdrAtTopData {
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

static int neon_rot_ldr_at_top_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopData *>(malloc(sizeof(NeonRotLdrAtTopData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_COUNT; i++) {
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

static int neon_rot_ldr_at_top_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_LDR_AT_TOP_COUNT; i++) {
            /* 3rd load at the TOP of the iteration (before the sources);
             * value intentionally discarded — this probe measures the
             * presence/position of the load, not its data */
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

            /* both stores write the ALU result — the str temp / str dst
             * pair stays back-to-back with NO memory op in between */
            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top, "NEON position probe: 3rd ldr at iteration top (same count/mix as fwd_probe, position is the only variable)")
    .test_init    = neon_rot_ldr_at_top_init,
    .test_run     = neon_rot_ldr_at_top_run,
    .test_cleanup = neon_rot_ldr_at_top_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

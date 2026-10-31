/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_nop_gap
 * @parblock
 * Insertion-itself control (plan ③ of the 2026-09-04 attribution split,
 * reviewer's suggestion): store_only with a single architectural NOP
 * inserted between the two stores.
 *   store_only:  ALU  str temp  str dst
 *   nop_gap:     ALU  str temp  NOP  str dst
 * Same instruction count as fwd_probe (one extra slot between the stores),
 * but the inserted instruction is a NOP — no memory access, no AGU use.
 *   ~14 (store_only level) -> insertion per se is harmless; the kill is
 *                              specific to an ldr between the stores
 *   drops toward 0          -> ANY instruction between the stores loosens
 *                              the window; timing tightness dominates
 * Skeleton otherwise identical to neon_rot_store_only.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_NOP_GAP_COUNT 1024

struct NeonRotNopGapData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
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

static int neon_rot_nop_gap_init(struct test *test) {
    auto *data = static_cast<NeonRotNopGapData *>(malloc(sizeof(NeonRotNopGapData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_NOP_GAP_COUNT; i++) {
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

static int neon_rot_nop_gap_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotNopGapData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_NOP_GAP_COUNT; i++) {
            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            /* two stores of the ALU result with a single architectural NOP
             * in between (no memory op, no AGU use — insertion itself) */
            store_vec(&temp[i], res);
            __asm__ volatile ("nop");
            store_vec(&dst[i], res);
        }

        if (memcmp(dst, data->expected, NEON_ROT_NOP_GAP_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_nop_gap data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_nop_gap_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotNopGapData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_nop_gap, "NEON insertion control: store_only + 1 NOP between the stores (insertion itself, no memory op)")
    .test_init    = neon_rot_nop_gap_init,
    .test_run     = neon_rot_nop_gap_run,
    .test_cleanup = neon_rot_nop_gap_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

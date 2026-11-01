/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_robfill_np
 * @parblock
 * Timing-stretch CONTROL variant for neon_rot_ldr_at_top_robfill
 * (2026-09-09).
 *
 * Identical skeleton and identical instruction SLOT COUNT in the hot
 * loop as robfill (rob_fill_chains() replaced by an asm block of 10
 * nops), but the 10 slots are no-ops: no extra ROB/issue-window
 * occupancy beyond decode width, no FPU work, no register writes. Any
 * difference in trigger rate between robfill and robfill_np is then
 * attributable to the window/resource pressure (the chains), not to the
 * ~10-slot timing stretch they also cause. The nop-gap lesson from the
 * movbe family (0->1->2->64 nops: 18->1->6->0) makes this control
 * mandatory rather than optional.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT 1024

struct NeonRotLdrAtTopRobfillNpData {
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

/* 10 nops: same slot count as robfill's fmul chains, zero resource use */
static inline void nop_fill(void) {
    __asm__ volatile (
        "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\t"
        "nop\n\tnop\n\tnop\n\tnop\n\tnop" :::);
}

#endif

static int neon_rot_ldr_at_top_robfill_np_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRobfillNpData *>(malloc(sizeof(NeonRotLdrAtTopRobfillNpData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t));

    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT; i++) {
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

static int neon_rot_ldr_at_top_robfill_np_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRobfillNpData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            nop_fill();

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

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_ROBFILL_NP_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_robfill_np data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_robfill_np_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRobfillNpData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_robfill_np, "Timing-stretch control for robfill: same skeleton, same 10 hot-loop slots but all nops (no ROB/resource occupancy); robfill vs this separates window pressure from timing stretch")
    .test_init    = neon_rot_ldr_at_top_robfill_np_init,
    .test_run     = neon_rot_ldr_at_top_robfill_np_run,
    .test_cleanup = neon_rot_ldr_at_top_robfill_np_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

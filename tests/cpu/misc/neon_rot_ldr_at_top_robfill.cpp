/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_robfill
 * @parblock
 * ROB/scheduler-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-09).
 *
 * The parent retires ~7 uops per element with no long-latency
 * independents in flight, so the reorder buffer and the issue queues run
 * shallow. This variant adds 10 INDEPENDENT scalar fmul ops (d8..d17,
 * each multiplying its own accumulator by 1.0000001) per element. Each
 * chain is long-latency (FPU pipe), independent of the trigger chain and
 * of every other chain, so the scheduler must keep them all in window:
 * ROB occupancy and issue-queue pressure rise substantially while the
 * memory-side skeleton (3rd ldr at top + 2-src loads + rotating ALU +
 * back-to-back str/str, alternating sweeps) is untouched.
 *
 * The 10 accumulators are folded into a stack sink once per sweep (cold,
 * outside the per-element path) so nothing is DCE'd. The fmul chain is
 * emitted via inline asm with explicit fmov'd constants -- GCC cannot
 * constant-fold or hoist it.
 *
 * Compare against robfill_np (same slot count, all nops) to separate
 * "window pressure" from "plain timing stretch".
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT 1024
#define NEON_ROT_LDR_AT_TOP_ROBFILL_CHAIN 10

struct NeonRotLdrAtTopRobfillData {
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

/* 10 independent scalar fmul chains: d8..d17 *= 1.0000001.
 * In/out via a single q register pair is NOT used -- the chains live
 * entirely in callee-saved-by-asm d8..d17 and are only folded out at
 * sweep end. Constants fmov'd inside the asm so nothing folds. */
static inline void rob_fill_chains(void) {
    __asm__ volatile (
        "fmov d18, #0.375        \n\t"   /* seed-ish bits, irrelevant */
        "fmul d8,  d8,  d18      \n\t"
        "fmul d9,  d9,  d18      \n\t"
        "fmul d10, d10, d18      \n\t"
        "fmul d11, d11, d18      \n\t"
        "fmul d12, d12, d18      \n\t"
        "fmul d13, d13, d18      \n\t"
        "fmul d14, d14, d18      \n\t"
        "fmul d15, d15, d18      \n\t"
        "fmul d16, d16, d18      \n\t"
        "fmul d17, d17, d18      \n\t"
        ::: "d8", "d9", "d10", "d11", "d12", "d13", "d14", "d15", "v16", "v17", "d18");
}

/* cold sink: fold the 10 chains so they are observable (not DCE-able) */
static inline void rob_sink_chains(uint64_t *sink) {
    __asm__ volatile (
        "fmul d8, d8, d18        \n\t"
        "fmul d9, d9, d8         \n\t"
        "fmul d10, d10, d9       \n\t"
        "fmul d11, d11, d10      \n\t"
        "fmul d12, d12, d11      \n\t"
        "fmul d13, d13, d12      \n\t"
        "fmul d14, d14, d13      \n\t"
        "fmul d15, d15, d14      \n\t"
        "fmul d16, d16, d15      \n\t"
        "fmul d17, d17, d16      \n\t"
        "fmov x8, d17            \n\t"
        "str x8, [%0]            \n\t"
        :: "r"(sink) : "x8", "d8", "d9", "d10", "d11", "d12",
           "d13", "d14", "d15", "v16", "v17", "memory");
}

#endif

static int neon_rot_ldr_at_top_robfill_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRobfillData *>(malloc(sizeof(NeonRotLdrAtTopRobfillData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t));

    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT; i++) {
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

static int neon_rot_ldr_at_top_robfill_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRobfillData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)));
    uint64_t sink = 0;

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            /* 10 independent long-latency chains: ROB / issue-window
             * pressure, independent of the trigger chain below. */
            rob_fill_chains();

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

        /* cold: fold + sink the chains once per sweep */
        rob_sink_chains(&sink);

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_ROBFILL_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_robfill data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_robfill_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRobfillData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_robfill, "ROB-dimension variant of ldr_at_top_rowmajor: 10 independent asm fmul chains per element (d8-d17) fill the reorder buffer / issue window; trigger skeleton intact. Compare vs robfill_np (nops) to separate window pressure from timing stretch")
    .test_init    = neon_rot_ldr_at_top_robfill_init,
    .test_run     = neon_rot_ldr_at_top_robfill_run,
    .test_cleanup = neon_rot_ldr_at_top_robfill_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

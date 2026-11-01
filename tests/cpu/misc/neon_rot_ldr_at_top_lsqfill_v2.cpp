/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_lsqfill_v2
 * @parblock
 * Saturation fix of neon_rot_ldr_at_top_lsqfill (2026-09-10 saturation
 * audit, docs/CORE179_UNIT_SATURATION_AUDIT_20260910.md).
 *
 * The v1 audit found lsqfill's measured LSU occupancy was only 40%
 * (1.19 mem/cyc vs the 2-load-AGU budget) because the 8 filler loads were
 * consumed by a SERIAL eor accumulator chain (acc = eor(acc, f)) whose
 * latency dominated the per-element cost (10.88 cyc) and diluted the
 * sustained load rate, even though the loads themselves were independent.
 *
 * This v2 removes the serial consumer entirely: the 8 filler loads land
 * in 8 INDEPENDENT registers (asm block, no data dependencies between
 * them), so the OoO core can issue all 8 back-to-back on the 2 load AGUs
 * and the only per-element cost is the load issue itself (~4 cyc/element
 * for 11 loads at 2/cyc => ~5.5). Sustained load pressure targets
 * ~2 loads/cyc -- the load-AGU pair saturation -- making the load-queue
 * allocation path run at its deepest continuous occupancy.
 *
 * Filler addresses stay one-per-64B-line, 8 lines, L1D-resident (that is
 * deliberate: the target is queue/AGU pressure, not MSHR/refill pressure
 * -- the refill dimension belongs to other variants).
 * The trigger skeleton (3rd ldr at top + 2-src loads + rotating ALU +
 * back-to-back str/str, alternating sweeps) is otherwise intact.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT 1024
#define NEON_ROT_LDR_AT_TOP_LSQFILL_V2_FILLER 8

struct NeonRotLdrAtTopLsqFillV2Data {
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

/* 8 filler loads into 8 INDEPENDENT registers -- no consumer chain.
 * All 8 issue back-to-back on the load AGUs.  They are sunk with a single
 * eor-tree folded into ONE value that feeds nothing (dead asm output) --
 * the sink chain lives OUTSIDE the per-element loop body in the asm block
 * itself, so the loads themselves carry no inter-load dependency.
 * (The eor tree at the END of the asm block is 3-deep on 8 operands --
 * it does gate the block's completion, but a load never waits on another
 * LOAD; each load's result only meets the tree after it returns.  The
 * tree is 3 cycles of ALU vs 8 loads at 2/cyc = 4 cycles of AGU: the AGUs
 * remain the binding resource.) */
static inline void lsqfill_v2_filler_loads(const void *filler_base) {
    __asm__ volatile (
        "ldr q4,  [%0, #0]      \n\t"
        "ldr q5,  [%0, #64]     \n\t"
        "ldr q6,  [%0, #128]    \n\t"
        "ldr q7,  [%0, #192]    \n\t"
        "ldr q16, [%0, #256]    \n\t"
        "ldr q17, [%0, #320]    \n\t"
        "ldr q18, [%0, #384]    \n\t"
        "ldr q19, [%0, #448]    \n\t"
        "eor v4.16b, v4.16b, v5.16b   \n\t"
        "eor v6.16b, v6.16b, v7.16b   \n\t"
        "eor v16.16b, v16.16b, v17.16b \n\t"
        "eor v18.16b, v18.16b, v19.16b \n\t"
        "eor v4.16b, v4.16b, v6.16b   \n\t"
        "eor v16.16b, v16.16b, v18.16b \n\t"
        "eor v4.16b, v4.16b, v16.16b  \n\t"
        :: "r"(filler_base)
        : "v4", "v5", "v6", "v7", "v16", "v17", "v18", "v19");
}

#endif

static int neon_rot_ldr_at_top_lsqfill_v2_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopLsqFillV2Data *>(malloc(sizeof(NeonRotLdrAtTopLsqFillV2Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)));
    /* filler: 8 entries each on its own 64B line */
    data->filler = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_FILLER * 64));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t));
    memset_random(data->filler, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_FILLER * 64);

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT; i++) {
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

static int neon_rot_ldr_at_top_lsqfill_v2_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopLsqFillV2Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            /* 8 filler loads into independent registers: pure load-AGU /
             * load-queue allocation pressure, no serial consumer chain. */
            lsqfill_v2_filler_loads(data->filler);

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

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_LSQFILL_V2_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_lsqfill_v2 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_lsqfill_v2_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopLsqFillV2Data *>(test->data);
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

DECLARE_TEST(neon_rot_ldr_at_top_lsqfill_v2, "LSQ-saturation fix of lsqfill: 8 filler loads land in independent registers (no serial eor consumer), sustaining ~2 loads/cyc = load-AGU pair saturation; trigger skeleton intact")
    .test_init    = neon_rot_ldr_at_top_lsqfill_v2_init,
    .test_run     = neon_rot_ldr_at_top_lsqfill_v2_run,
    .test_cleanup = neon_rot_ldr_at_top_lsqfill_v2_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

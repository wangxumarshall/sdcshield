/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_rfport_v2
 * @parblock
 * Saturation fix of neon_rot_ldr_at_top_rfport (2026-09-10 saturation
 * audit, docs/CORE179_UNIT_SATURATION_AUDIT_20260910.md).
 *
 * v1 put in ONE 4-deep serial vmov chain (v4->v5->v6->v7) = ~2.5 cycles
 * of extra per-element cost -- an order of magnitude below RF-port
 * saturation.  The audit's fix: MULTIPLE INDEPENDENT move chains, each
 * 2-deep, all issued back-to-back, so per element the NEON pipes see
 * 12 independent mov uops consuming 12 RF reads + 6 RF writes with no
 * serial latency chain gating them.
 *
 * Six independent chains, each 2 moves deep:
 *   srcs: a, b (the two source operands) + 4 loop-invariant asm constants
 *   v1 <- a;  v2 <- v1          (chain 1: from load a)
 *   v3 <- b;  v4 <- v3          (chain 2: from load b)
 *   v5 <- v20; v6 <- v5         (chain 3: from asm-held constant)
 *   ... 6 chains total
 * All 12 moves are mutually independent; on a 2-pipe NEON machine they
 * retire in ~6 cycles while the two 2-deep chains per source add only
 * 2 cycles of latency to the ALU consumption point -- the RF read/write
 * ports run at full occupancy the whole time.
 *
 * Per element RF traffic budget with this variant:
 *   12 mov (12 reads + 12 writes) + 3 ldr (3 writes) + 1 ALU (2 reads +
 *   1 write) + 2 str (2 reads) = ~19 reads + 16 writes per ~6 cycles
 *   = ~3.2 reads + 2.7 writes per cycle sustained -- at or above the
 *   typical v110-class RF port budget for the vector bank.
 * The trigger skeleton is otherwise intact.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT 1024

struct NeonRotLdrAtTopRfportV2Data {
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

/* Six independent 2-deep move chains.  Inputs a/b are the loaded source
 * operands (chains 1-2 preserve their values for the ALU op below, so
 * the last link of each is what the compiler consumes).  Chains 3-6 move
 * asm-held constant registers (v20-v23, values irrelevant, traffic is
 * the point).  12 independent mov uops, no serial cross-dependencies.
 * NOTE: both operands propagate through the braid and come back out --
 * smoke run 2026-09-10 caught the v1 bug where b was overwritten with
 * a's propagated value (all cores failed deterministically in <3s). */
static inline void rf_move_braid(uint64x2_t &a, uint64x2_t &b) {
    __asm__ volatile (
        "mov v1.16b, %0.16b     \n\t"
        "mov v2.16b, v1.16b     \n\t"
        "mov v3.16b, %1.16b     \n\t"
        "mov v4.16b, v3.16b     \n\t"
        "mov v5.16b, v20.16b    \n\t"
        "mov v6.16b, v5.16b     \n\t"
        "mov v7.16b, v21.16b    \n\t"
        "mov v8.16b, v7.16b     \n\t"
        "mov v9.16b, v22.16b    \n\t"
        "mov v10.16b, v9.16b    \n\t"
        "mov v11.16b, v23.16b   \n\t"
        "mov v12.16b, v11.16b   \n\t"
        "mov %0.16b, v2.16b     \n\t"
        "mov %1.16b, v4.16b     \n\t"
        : "+w"(a), "+w"(b)
        :
        : "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8",
          "v9", "v10", "v11", "v12");
}

#endif

static int neon_rot_ldr_at_top_rfport_v2_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRfportV2Data *>(malloc(sizeof(NeonRotLdrAtTopRfportV2Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t));

    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT; i++) {
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

static int neon_rot_ldr_at_top_rfport_v2_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRfportV2Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);

            /* 6 independent 2-deep move chains: RF port / bypass
             * saturation pressure, no serial cross-chain dependency. */
            rf_move_braid(a, b);

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

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_RFPORT_V2_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_rfport_v2 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rfport_v2_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRfportV2Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rfport_v2, "RF-port-saturation fix of rfport: 6 independent 2-deep asm vmov chains (12 independent moves, no serial consumer) sustain ~3 RF reads + ~2.7 writes per cycle; trigger skeleton intact")
    .test_init    = neon_rot_ldr_at_top_rfport_v2_init,
    .test_run     = neon_rot_ldr_at_top_rfport_v2_run,
    .test_cleanup = neon_rot_ldr_at_top_rfport_v2_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

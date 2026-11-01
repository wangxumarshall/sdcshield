/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_priv
 * @parblock
 * Freeze-narrowing variant of neon_rot_ldr_at_top (2026-09-07 freeze matrix):
 * ldr_at_top FREEZES the machine at >=180 cores while the identical-count
 * neon_rot_2src does not — the only variable is the 3rd load's position
 * (top of iteration) and its target (shared test->data scratch). This
 * variant keeps the position (load still at the top, before the sources)
 * but points it at a PER-THREAD PRIVATE buffer instead of the shared one:
 *   freeze      -> timing/position of the pre-source load is the necessary
 *                  condition; coherency-domain pressure irrelevant
 *   no freeze   -> the 3rd load must read a SHARED buffer (LLC/snoop
 *                  pressure participates)
 * Original ldr_at_top untouched; see docs for the freeze matrix.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_PRIV_COUNT 1024

struct NeonRotLdrAtTopPrivData {
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

static int neon_rot_ldr_at_top_priv_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopPrivData *>(malloc(sizeof(NeonRotLdrAtTopPrivData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_PRIV_COUNT; i++) {
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

static int neon_rot_ldr_at_top_priv_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopPrivData *>(test->data);

    /* PRIVATE scratch — the ONLY variable vs ldr_at_top: same position
     * (top of iteration, before the source loads), same discarded value,
     * but per-thread buffer instead of the shared test->data one       */
    uint64x2_t *scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)));
    memset_random(scratch, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t));

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_LDR_AT_TOP_PRIV_COUNT; i++) {
            /* 3rd load at the TOP of the iteration (before the sources),
             * reading the PRIVATE per-thread scratch buffer */
            uint64x2_t unused_x = load_vec(&scratch[i]);
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

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_PRIV_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_priv data miscompare");
        }
    }

    free(dst);
    free(temp);
    free(scratch);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_priv_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopPrivData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_priv, "Freeze-narrowing: ldr_at_top with PRIVATE per-thread scratch — position kept, shared-buffer read removed")
    .test_init    = neon_rot_ldr_at_top_priv_init,
    .test_run     = neon_rot_ldr_at_top_priv_run,
    .test_cleanup = neon_rot_ldr_at_top_priv_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

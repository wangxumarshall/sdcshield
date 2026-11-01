/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_sub
 * @parblock
 * ALU-dimension variant of neon_rot_ldr_at_top (2026-09-08).
 *
 * The 55-record dump campaign (docs/CORE179_PANIC_20260908_DUMP_CAMPAIGN_CRASH.md)
 * showed act corruption under all four ALU ops of the parent (add/eor/and/orr),
 * with DIFFERENT xor signatures per op: add = high-bit avalanche with low bytes
 * clean (carry-chain look), eor = few-bit flips concentrated in the low 24 bits.
 * This variant swaps the rotation to the SUB-family to test whether the
 * corruption signature follows the ALU class (carry-propagating vs pure
 * combinational) or is op-agnostic:
 *   i%4: sub / eor / bic / orn   (sub carries; eor/bic/orn are combinational)
 * Same skeleton otherwise: 3rd ldr at top + 2-src loads + rotating ALU +
 * back-to-back str/str. Golden precomputed in init; memcmp in cold path.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_SUB_COUNT 1024

struct NeonRotLdrAtTopSubData {
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

static int neon_rot_ldr_at_top_sub_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopSubData *>(malloc(sizeof(NeonRotLdrAtTopSubData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_SUB_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = vsubq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vbicq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vornq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_sub_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopSubData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)));

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < NEON_ROT_LDR_AT_TOP_SUB_COUNT; i++) {
            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (i % 4) {
                case 0: res = vsubq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vbicq_u64(a, b); break;
                case 3: res = vornq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_SUB_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_sub data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_sub_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopSubData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_sub, "ALU-dimension variant of ldr_at_top: rotation sub/eor/bic/orn (carry vs combinational signature test)")
    .test_init    = neon_rot_ldr_at_top_sub_init,
    .test_run     = neon_rot_ldr_at_top_sub_run,
    .test_cleanup = neon_rot_ldr_at_top_sub_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

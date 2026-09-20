/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor
 * @parblock
 * Microarchitecture-dimension variant of neon_rot_ldr_at_top (2026-09-08).
 *
 * The 55-record dump campaign showed the corruption concentrated on the
 * FIRST indices of the inner loop (idx 0 + idx 1 = 36/55 records) while the
 * buffer spans 1024 entries. The parent's inner loop revisits the same
 * buffer in the same order every outer iteration: the memory system sees
 * the identical touch sequence, and idx 0/1 are the "coldest / most
 * forward-progress-dependent" slots of each pass.
 * This variant walks the buffer in a REVISED order: even outer iterations
 * sweep indices ascending, odd outer iterations sweep descending. The
 * touch pattern at idx 0/1 then differs from the parent's (on a descending
 * pass, index 0 is the LAST element touched, with a fully-warm pipeline
 * behind it, and the store->load adjacency across the loop seam changes).
 *   strong idx-0/1 signal also on this variant  -> the signature follows
 *      the buffer slots themselves (address-local, e.g. same L1D set/way)
 *   signature moves to the sweep seam / disappears              ->
 *      it follows the forward-progress position in the sweep
 * Same skeleton otherwise: 3rd ldr at top + 2-src loads + rotating ALU
 * (add/eor/and/orr) + back-to-back str/str; memcmp in cold path.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT 1024

struct NeonRotLdrAtTopRowMajorData {
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

static int neon_rot_ldr_at_top_rowmajor_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorData *>(malloc(sizeof(NeonRotLdrAtTopRowMajorData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t));

    /* golden: same rotating composition, computed on the same NEON units */
    for (int i = 0; i < NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT; i++) {
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

static int neon_rot_ldr_at_top_rowmajor_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT; n++) {
            const int i = ascending ? n : (NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT - 1 - n);

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

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_ROWMAJOR_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_rowmajor data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopRowMajorData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor, "Order variant of ldr_at_top: alternate ascending/descending sweeps (slot-local vs forward-progress-position signature test)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

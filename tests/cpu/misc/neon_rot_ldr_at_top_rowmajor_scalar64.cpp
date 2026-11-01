/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_scalar64
 * @parblock
 * Scalar-64b chassis of neon_rot_ldr_at_top_rowmajor (2026-09-13).
 *
 * The rowmajor NEON chassis is the highest-rate trigger known
 * (3-round mean 223 unique fail blocks / 30 min, all on thread 179; see
 * docs/CORE179_NEON_ROT_LDR_AT_TOP_STABILITY_CAMPAIGN_20260909.md), but it
 * has NEVER had a scalar counterpart — the NEON-vs-scalar gap on this
 * skeleton is a blank.  The kernel-side golden signature
 * (find_busiest_group, x20 = true bytes from a misaligned byte window)
 * comes from a SCALAR load context, so a high-rate scalar chassis would
 * dock with the golden signature directly.
 *
 * This test is a per-element translation of the rowmajor chassis onto the
 * scalar 64b path: same skeleton shape, 64 bits per element instead of
 * 128:
 *   - scratch ldr at top (no consumer)
 *   - 2-src scalar loads (srcA/srcB)
 *   - rotating ALU (add/eor/and/orr by i%4)
 *   - back-to-back str temp/str dst
 *   - rowmajor alternating ascending/descending sweeps
 *   - cold-path memcmp only
 *
 * Lane-binding context (why the scalar chassis matters): the NEON lane
 * curve is 2x64b=26-33, 4x32b=0, 16x8b=2 fails/30min at identical ldr q /
 * NEON register file — the binding is on "data organized in 64b lanes",
 * and the scalar path is natively 64b.  A scalar chassis rate near the
 * NEON chassis rate collapses the lane question onto the data-path width;
 * a scalar chassis far below it would isolate the trigger to the vector
 * delivery path.  Either result feeds the campaign design doc
 * docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md (P0 axis).
 *
 * Engineering notes (inherited from the family):
 *   - the scratch load MUST be asm volatile or GCC eliminates it
 *     (no consumer, DCE) — same trap as the NEON reload ldr;
 *   - the hot loop must stay free of any dump/branch beyond the
 *     inherited cold-path memcmp (dump variants suppress ~30x);
 *   - no LCG seed fixing; memcmp via report_fail_msg in cold path only.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)

#define SCALAR64_ROWMAJOR_COUNT 2048   /* 2048 u64 = same 16 KiB footprint
                                        * as the NEON chassis' 1024 x 128b */

struct Scalar64RowmajorData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *scratch;
    uint64_t *expected;
};

/* Scalar mirror of the family's load_vec: asm volatile ldr, "=r" X-reg.
 * The asm boundary is load-bearing (DCE guard + layout freeze). */
static inline uint64_t load_u64(const void *addr) {
    uint64_t res;
    __asm__ volatile ("ldr %0, [%1]" : "=r"(res) : "r"(addr) : "memory");
    return res;
}

static inline void store_u64(void *addr, uint64_t val) {
    __asm__ volatile ("str %0, [%1]" :: "r"(val), "r"(addr) : "memory");
}

static int scalar64_rowmajor_init(struct test *test) {
    auto *data = static_cast<Scalar64RowmajorData *>(malloc(sizeof(Scalar64RowmajorData)));
    data->srcA    = static_cast<uint64_t *>(aligned_alloc(64, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)));
    data->srcB    = static_cast<uint64_t *>(aligned_alloc(64, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)));
    data->scratch = static_cast<uint64_t *>(aligned_alloc(64, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)));
    data->expected= static_cast<uint64_t *>(aligned_alloc(64, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA,    SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t));
    memset_random(data->srcB,    SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t));
    memset_random(data->scratch, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t));

    /* golden: same rotating composition, computed on the same scalar units */
    for (int i = 0; i < SCALAR64_ROWMAJOR_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] | data->srcB[i]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int scalar64_rowmajor_run(struct test *test, int cpu) {
    auto *data = static_cast<Scalar64RowmajorData *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < SCALAR64_ROWMAJOR_COUNT; n++) {
            const int i = ascending ? n : (SCALAR64_ROWMAJOR_COUNT - 1 - n);

            uint64_t unused_x = load_u64(&data->scratch[i]);
            (void)unused_x;

            uint64_t a = load_u64(&data->srcA[i]);
            uint64_t b = load_u64(&data->srcB[i]);
            uint64_t res;

            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a ^ b; break;
                case 2: res = a & b; break;
                case 3: res = a | b; break;
            }

            store_u64(&temp[i], res);
            store_u64(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, SCALAR64_ROWMAJOR_COUNT * sizeof(uint64_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_scalar64 data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int scalar64_rowmajor_cleanup(struct test *test) {
    auto *data = static_cast<Scalar64RowmajorData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_scalar64, "Scalar-64b chassis of ldr_at_top_rowmajor: same skeleton (scratch ldr at top + 2-src + rot ALU + b2b str/str + rowmajor sweeps) on the GP-register path")
    .test_init    = scalar64_rowmajor_init,
    .test_run     = scalar64_rowmajor_run,
    .test_cleanup = scalar64_rowmajor_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif

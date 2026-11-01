/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k5far
 * @parblock
 * K5 far-apart dual-source cell of the readpath-localization campaign
 * (2026-09-13, docs/CORE179_READPATH_LOCALIZATION_CAMPAIGN_20260913.md §7).
 *
 * THE decisive cell: E6 (address-capture corruption) vs data steering
 * (LSU result routing).  The GPS chassis loads srcA[i] and srcB[i] from
 * the SAME element index, i.e. two addresses 16 KiB apart (same offset
 * within each buffer) — far apart in different cache lines AND different
 * sets, which is what makes "splice" distinguishable from "misaligned
 * window": neighbouring dual sources produce identical bytes for both
 * failure modes, far-apart sources expose cross-line byte mixing.
 *
 * This variant keeps the hot loop byte-identical to the GPS chassis and
 * changes ONLY the fill: srcB is filled at a NEAR address instead —
 * srcB[i] element carries the tag of buffer offset (i * 16 + 8), i.e.
 * the word IMMEDIATELY AFTER srcA[i]'s lane 1 word in address space is
 * simulated as "the other source".  WRONG — see corrected design below.
 *
 * CORRECTED DESIGN (after first-signature review, 09-13 evening): the
 * first GPS round showed the dominant signature is same-buffer FIELD
 * SUBSTITUTION from a NEIGHBOURING srcA word (156/156 sources in srcA,
 * distance ±8..±28 elements).  The K5 question for THIS signature is
 * therefore: does the substitution source follow the ADDRESS (near
 * neighbour of the failing load's address, whichever buffer it belongs
 * to) or the BUFFER IDENTITY / port (stays srcA-only even when the
 * address neighbour is srcB)?  To answer it, this variant INTERLEAVES
 * the two sources in one address space:
 *
 *   single buffer src[2*i]   = "A-family" word pair (magic 0xA5A5)
 *   single buffer src[2*i+1] = "B-family" word pair (magic 0x5A5A)
 *
 * The hot loop loads A from src[2*i] and B from src[2*i+1] — adjacent
 * addresses (32 B apart), alternating family magic every element.  The
 * address-neighbour of every load is ALWAYS the other family.  Outcomes:
 *   - substitution source still "A-only"  -> source follows buffer/
 *     port identity, address proximity is not the driver;
 *   - substitution source becomes "the address neighbour" (i.e. for a
 *     failing element i, the substituted fields come from src[2*i±1],
 *     the OTHER family) -> proximity-driven cross-talk (E6-family);
 *   - true A+B splice words appear (already detectable) -> steering.
 *
 * Engineering: hot loop loads via the same asm ldr q, same i%4 rotating
 * ALU, same back-to-back str/str, same rowmajor alternating sweeps, same
 * cold-path memcmp + five-buffer dump discipline as the GPS chassis.
 * The only deltas vs GPS chassis: one interleaved source buffer instead
 * of two parallel ones (same total footprint, same load count), and the
 * dump layout records the interleaved buffer twice (slots srcA/srcB
 * hold the same pointer's first/second half view for classifier
 * compatibility).  No LCG seed fixing; auto-seed only.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <sys/stat.h>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define K5FAR_COUNT 1024              /* elements (uint64x2_t) */
#define K5FAR_SRC_ELEMS (K5FAR_COUNT * 2)   /* interleaved buffer: 2 families */
#define K5FAR_DUMP_DIR "movbe_log/gps_rowmajor"   /* same classifier dir */

#define K5FAR_MAGIC_A   0xA5A5ULL
#define K5FAR_MAGIC_B   0x5A5AULL
#define K5FAR_MAGIC_SCR 0x0F0FULL

struct K5FarData {
    uint64x2_t *src;        /* interleaved: even elem = A-family, odd = B */
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

static inline uint64_t gps_tag(uint64_t magic, uint64_t byte_off, uint64_t gen) {
    return (magic                      << 48)
         | ((byte_off        & 0xFFFFULL) << 32)
         | ((~byte_off       & 0xFFFFULL) << 16)
         |  (gen             & 0xFFFFULL);
}

static int k5far_init(struct test *test) {
    auto *data = static_cast<K5FarData *>(malloc(sizeof(K5FarData)));
    data->src     = static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_SRC_ELEMS * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_COUNT * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_COUNT * sizeof(uint64x2_t)));

    mkdir(K5FAR_DUMP_DIR, 0755);

    /* Interleaved tagged fill — the ONLY functional delta vs the GPS
     * chassis.  Element 2*i: A-family; element 2*i+1: B-family.  The
     * byte_off field records the offset in the FULL interleaved buffer
     * so the classifier can resolve the address neighbour directly. */
    for (int e = 0; e < K5FAR_SRC_ELEMS; e++) {
        const uint64_t byte = static_cast<uint64_t>(e) * 16;
        const uint64_t magic = (e & 1) ? K5FAR_MAGIC_B : K5FAR_MAGIC_A;
        store_vec(&data->src[e], vcombine_u64(vcreate_u64(gps_tag(magic, byte + 0, 0)),
                                              vcreate_u64(gps_tag(magic, byte + 8, 0))));
    }
    for (int i = 0; i < K5FAR_COUNT; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(gps_tag(K5FAR_MAGIC_SCR, byte + 0, 0)),
                                                  vcreate_u64(gps_tag(K5FAR_MAGIC_SCR, byte + 8, 0))));
    }

    /* golden: same rotating composition on the same NEON units */
    for (int i = 0; i < K5FAR_COUNT; i++) {
        const uint64x2_t a = data->src[2 * i];
        const uint64x2_t b = data->src[2 * i + 1];
        switch (i % 4) {
            case 0: data->expected[i] = vaddq_u64(a, b); break;
            case 1: data->expected[i] = veorq_u64(a, b); break;
            case 2: data->expected[i] = vandq_u64(a, b); break;
            case 3: data->expected[i] = vorrq_u64(a, b); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int k5far_run(struct test *test, int cpu) {
    auto *data = static_cast<K5FarData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < K5FAR_COUNT; n++) {
            const int i = ascending ? n : (K5FAR_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->src[2 * i]);
            uint64x2_t b = load_vec(&data->src[2 * i + 1]);
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

        if (memcmp(dst, data->expected, K5FAR_COUNT * sizeof(uint64x2_t)) != 0) {
            /* COLD path, gated by memcmp.  Five-buffer dump in the SAME
             * layout as the GPS chassis for classifier compatibility:
             * dst, expected, "srcA-view" (even elements), "srcB-view"
             * (odd elements), scratch.  Both views are materialised from
             * the interleaved buffer so the offline classifier can use
             * its existing A/B magic logic unchanged. */
            char path[256];
            snprintf(path, sizeof(path), "%s/k5far_fail_cpu%d_sweep%lu.bin", K5FAR_DUMP_DIR,
                     cpu, static_cast<unsigned long>(sweep));
            FILE *f = fopen(path, "wb");
            if (f) {
                /* materialise the two views (cold path only) */
                uint64x2_t *viewA = static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_COUNT * sizeof(uint64x2_t)));
                uint64x2_t *viewB = static_cast<uint64x2_t *>(aligned_alloc(64, K5FAR_COUNT * sizeof(uint64x2_t)));
                for (int i = 0; i < K5FAR_COUNT; i++) {
                    viewA[i] = data->src[2 * i];
                    viewB[i] = data->src[2 * i + 1];
                }
                fwrite(dst,        sizeof(uint64x2_t), K5FAR_COUNT, f);
                fwrite(data->expected, sizeof(uint64x2_t), K5FAR_COUNT, f);
                fwrite(viewA,      sizeof(uint64x2_t), K5FAR_COUNT, f);
                fwrite(viewB,      sizeof(uint64x2_t), K5FAR_COUNT, f);
                fwrite(data->scratch, sizeof(uint64x2_t), K5FAR_COUNT, f);
                fprintf(f, "k5far sweep=%lu\n", static_cast<unsigned long>(sweep));
                fclose(f);
                free(viewA);
                free(viewB);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_k5far data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int k5far_cleanup(struct test *test) {
    auto *data = static_cast<K5FarData *>(test->data);
    if (data) {
        free(data->src);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k5far, "K5 far-apart/interleaved dual source: srcA/srcB alternate elements in ONE address space (address-neighbour is always the other family) — E6 address-capture vs steering vs buffer-identity; same GPS tag + dump discipline")
    .test_init    = k5far_init,
    .test_run     = k5far_run,
    .test_cleanup = k5far_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

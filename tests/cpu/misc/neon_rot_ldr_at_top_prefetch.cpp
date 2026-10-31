/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0.
 *
 * @test @b neon_rot_ldr_at_top_prefetch
 * @parblock
 * Prefetcher-dimension variant of neon_rot_ldr_at_top_rowmajor
 * (2026-09-09).
 *
 * The parent (and sub) touch consecutive 16B elements, i.e. a perfectly
 * strided stream that the L1D stream/stride prefetcher can track: by the
 * time the hot loop's ldr reaches line k, the prefetcher has often
 * already fetched it, so many loads land as L1D hits on
 * prefetched-on-spec lines. The rowmajor (alternating asc/desc) vs sub
 * (pure asc) trigger-rate difference (223 vs 109 fails/30min, 3-round
 * means) may be mediated by how each sweep order feeds the prefetcher.
 *
 * This variant BREAKS the prefetcher's stride detection while keeping
 * everything else constant: the element VISIT ORDER becomes a 4KiB-page
 * hop pattern. The buffer is enlarged to 256 entries x 16B = 4KiB per
 * buffer, and element index i is visited in the order
 *   j -> (j * 65) mod 256
 * (65 coprime to 256, so it is a full-cycle permutation hitting every
 * element exactly once). Consecutive accesses are ~1KiB apart on
 * average and never a constant stride: no stream prefetcher can track
 * it. Footprint per buffer is still exactly one 4KiB page (fits L1D
 * alongside the others, single dTLB entry per buffer -- translation
 * pressure unchanged vs parent).
 *
 *   rate(parent) ~ rate(this)  -> prefetcher irrelevant to the trigger
 *   rate changes                -> prefetcher-mediated; correlates with
 *                                  the rowmajor-vs-sub difference
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

/* 256 entries x 16 B = 4 KiB = exactly one page per buffer */
#define NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT 256

struct NeonRotLdrAtTopPrefetchData {
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

static int neon_rot_ldr_at_top_prefetch_init(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopPrefetchData *>(malloc(sizeof(NeonRotLdrAtTopPrefetchData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)));

    memset_random(data->srcA, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t));

    /* golden: rotating composition, indexed by the PERMUTED visit order so
     * that op rotation follows the visit sequence exactly like the parent
     * (op = position-in-sweep % 4), not the slot index. */
    for (int pos = 0; pos < NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT; pos++) {
        const int i = (pos * 65) % NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT;
        switch (pos % 4) {
            case 0: data->expected[i] = vaddq_u64(data->srcA[i], data->srcB[i]); break;
            case 1: data->expected[i] = veorq_u64(data->srcA[i], data->srcB[i]); break;
            case 2: data->expected[i] = vandq_u64(data->srcA[i], data->srcB[i]); break;
            case 3: data->expected[i] = vorrq_u64(data->srcA[i], data->srcB[i]); break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_prefetch_run(struct test *test, int cpu) {
    auto *data = static_cast<NeonRotLdrAtTopPrefetchData *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 15) {   /* 4x more sweeps: 256 vs 1024 elements */
        /* single direction per sweep, like sub; the permutation itself is
         * the dimension under test (no asc/desc alternation needed) */
        for (int pos = 0; pos < NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT; pos++) {
            const int i = (pos * 65) % NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT;

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t res;

            switch (pos % 4) {
                case 0: res = vaddq_u64(a, b); break;
                case 1: res = veorq_u64(a, b); break;
                case 2: res = vandq_u64(a, b); break;
                case 3: res = vorrq_u64(a, b); break;
            }

            store_vec(&temp[i], res);
            store_vec(&dst[i], res);
        }
        sweep++;

        if (memcmp(dst, data->expected, NEON_ROT_LDR_AT_TOP_PREFETCH_COUNT * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_prefetch data miscompare");
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_prefetch_cleanup(struct test *test) {
    auto *data = static_cast<NeonRotLdrAtTopPrefetchData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_prefetch, "Prefetcher-dimension variant of ldr_at_top: visit order becomes a x65 mod-256 permutation (non-constant ~1KiB hops break stride/stream prefetch detection); footprint still one page per buffer, trigger skeleton intact")
    .test_init    = neon_rot_ldr_at_top_prefetch_init,
    .test_run     = neon_rot_ldr_at_top_prefetch_run,
    .test_cleanup = neon_rot_ldr_at_top_prefetch_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

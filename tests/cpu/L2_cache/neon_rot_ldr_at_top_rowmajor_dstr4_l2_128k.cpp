/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k
 * @parblock
 * Write-side fast-detector probe #3, L2-resident tier 128KB, IDENTITY-COPY
 * chassis (2026-11-26, user directives.  Directive 1: the copy element was
 * MISSING from the parent — the parent's stores carry the rot4(a,b) ALU
 * result of two sources, never the loaded value itself, so a defect
 * coupled to the load-return -> store-data DIRECT path (the write-side
 * mirror of 0102's suspected LSU load-return lineage), or to block-scale
 * copy rhythm, would be missed.  Directive 2: the input block size is a
 * coupling axis — the effective window is >64KB and <512KB,
 * L2-resident — so the probes get L2-sized input blocks; these L2 tiers
 * live in tests/cpu/L2_cache/ for use on the target machine.)
 *
 * Input-data method transplant from the effective slow detectors
 * (2026-11-26 plan source analysis):
 *  - loop_check_m: 128KB/buffer fixed blocks, L2-resident — the origin
 *    of the tier sizes (its crc_M_BUF_SIZE is 8192x16 = 128KB).
 *  - memcpy_l2_cache_size / _neon: len in [L2/8, L2] with random
 *    64B-aligned offsets in a 2x window, memset_random input, 0x5A
 *    destination poison.  This tier transplants: FIXED block = tier
 *    (control variable — memcpy_l2's random length would blur tier
 *    attribution), per-sweep random 64B-aligned SHARED offset sliding
 *    the block-sized window through a 2x window (set/phase
 *    de-fixation), 0x5A poison on the destination windows.
 *  - memcpy1_neon (256B blocks) is NOT L2-resident: L2 residency is
 *    not the family invariant — it is the user's extracted coupling
 *    axis; the copy+write-then-reread invariant is unchanged.
 *
 * Data type: uint64x2_t (NEON q, 16B) with the loads/stores asm-pinned
 * — explicit NEON encoding.  The reference set is NOT uniformly NEON
 * (loop_check_m's default instantiation is a scalar LDP/STP X pair,
 * memcpy_l2_cache_size goes through libc's IFUNC dispatcher; only the
 * _neon twin is explicit); this family follows the explicit-NEON arm.
 *
 * Probe body (see tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_dstr4.cpp
 * for the compute-chassis parent): asm-pinned run of FOUR back-to-back
 * str to FOUR different destination buffers, dst[3] ALWAYS last.  The
 * chassis differs from the parent ONLY in the data path: the stored
 * value is the LOADED SOURCE VALUE directly (one random window, no ALU
 * on the data path) — the quad run becomes memcpy broadcast into four
 * destinations.  The parent's two sources / rotating ALU / scalar
 * golden are GONE (deliberate: the identity datapath is this tier's
 * variable; a compute-chassis L2 geometry was authored in a parallel
 * session and superseded per user directive — the compute chassis
 * remains available at the misc/ parent).
 *
 * Golden = the source WINDOW ITSELF: the sweep-end memcmp(dst[k],
 * src) over the block region for EVERY one of the four destinations —
 * memcpy_l2_cache_size's memcmp(dst, src) paradigm x 4 witnesses, no
 * expected buffer.  The 0x5A poison before each sweep covers the
 * lost-write face on every destination: an element the quad run never
 * lands on stays 0x5a5a.. and fails its witness compare.
 *
 * Window discipline: every sweep draws ONE random 64B-aligned offset
 * (random32(), 4-element granularity) SHARED by every buffer — the
 * source/dst pairing runs on the ABSOLUTE index i = off + j, so the
 * probe semantics are unchanged under the slide.  All buffers are
 * WIN = 2x LEN elements so every offset keeps the window in bounds;
 * the source is random over its full window (the window rolls, so all
 * of it goes hot over time).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 8192 uint64x2_t = 128KB (2x L1D,
 * inside the user's >64KB..<512KB window); buffers are src + 4 dst = 5
 * x 256KB (allocation 1.25MB per worker); the per-sweep touched set is
 * 5 x 128KB = 640KB — slightly exceeds L2 (the multi-destination
 * pressure geometry of this probe, disclosed honestly; the tier axis is
 * the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRD4_128_LEN 8192                 /* uint64x2_t elements -> 128KB block */
#define NRD4_128_WIN (2 * NRD4_128_LEN)   /* 2x-block window (memcpy_l2 method) */
#define NRD4_128_DSTS 4                   /* back-to-back destination stores */

namespace { /* ODR: per-TU NRD4_128Data */
struct NRD4_128Data {
    uint64x2_t *src;                      /* the single identity source (the golden) */
};
}

#if defined(__aarch64__)

static inline uint64x2_t nrd4_128_load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* the probe: asm-pinned quad back-to-back str of the same value to
 * four destinations (parent, verbatim — the identity is in what gets
 * stored); dst4 ALWAYS last */
static inline void nrd4_128_dstr4(void *d1, void *d2, void *d3, void *d4,
                                  uint64x2_t val) {
    __asm__ volatile(
        "str  %q0, [%1]\n\t"
        "str  %q0, [%2]\n\t"
        "str  %q0, [%3]\n\t"
        "str  %q0, [%4]\n\t"
        :
        : "w" (val), "r" (d1), "r" (d2), "r" (d3), "r" (d4)
        : "memory");
}

#endif

static int neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k_init(struct test *test)
{
    auto *data = static_cast<NRD4_128Data *>(malloc(sizeof(NRD4_128Data)));
    data->src = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_128_WIN * sizeof(uint64x2_t)));
    if (!data->src) {
        free(data->src);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->src, NRD4_128_WIN * sizeof(uint64x2_t));

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k_cleanup(struct test *test)
{
    auto *data = static_cast<NRD4_128Data *>(test->data);
    if (data) {
        free(data->src);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRD4_128Data *>(test->data);

    uint64x2_t *dst[NRD4_128_DSTS];
    for (int k = 0; k < NRD4_128_DSTS; ++k)
        dst[k] = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_128_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the source/dst
         * pairing and the golden semantics unchanged */
        const int off = (int)(4 * (random32() % (NRD4_128_LEN / 4)));

        /* poison every destination block: a lost write stays 0x5a5a..
         * and fails its witness compare */
        for (int k = 0; k < NRD4_128_DSTS; ++k)
            memset(dst[k] + off, 0x5A, NRD4_128_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRD4_128_LEN; n++) {
            const int j = ascending ? n : (NRD4_128_LEN - 1 - n);
            const int i = off + j;

            /* the identity: load the source, store THAT value to all
             * four destinations — no ALU between the load return and
             * the store data */
            const uint64x2_t s = nrd4_128_load_vec(&data->src[i]);

            /* the probe: four stores of the same value, back-to-back,
             * four destinations, dst[3] ALWAYS last */
            nrd4_128_dstr4(&dst[0][i], &dst[1][i], &dst[2][i], &dst[3][i], s);
        }
        sweep++;

        /* cold path: ALL FOUR destinations are witnesses against the
         * source window itself (memcpy's memcmp(dst, src) x 4) */
        for (int k = 0; k < NRD4_128_DSTS; ++k) {
            if (memcmp(dst[k] + off, data->src + off, NRD4_128_LEN * sizeof(uint64x2_t)) != 0) {
                for (int j = 0; j < NRD4_128_LEN; ++j) {
                    const int i = off + j;
                    if (memcmp(&dst[k][i], &data->src[i], sizeof(uint64x2_t)) != 0) {
                        const uint64_t *S = reinterpret_cast<const uint64_t *>(&data->src[i]);
                        const uint64_t *G = reinterpret_cast<const uint64_t *>(&dst[k][i]);
                        report_fail_msg("dstr4_l2_128k data miscompare: dst#%d idx %u sweep %lu off %u: "
                                        "S=%016lx,%016lx G=%016lx,%016lx X=%016lx,%016lx "
                                        "(identity copy, quad back-to-back destination stores, "
                                        "L2 block 128KB sliding)",
                                        k, (unsigned)i, static_cast<unsigned long>(sweep),
                                        (unsigned)off,
                                        S[0], S[1], G[0], G[1],
                                        G[0] ^ S[0], G[1] ^ S[1]);
                        for (int m = 0; m < NRD4_128_DSTS; ++m) free(dst[m]);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }

    for (int k = 0; k < NRD4_128_DSTS; ++k) free(dst[k]);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k, "Write-side fast-detector probe, L2-resident tier 128KB, IDENTITY-COPY chassis: one asm source ldr, no ALU on the data path, asm-pinned quad back-to-back str of the loaded source value to four destinations (dst last) — store issue density / store-buffer occupancy amplifier; all four buffers witness-checked against the source window (memcmp(dst,src) x 4), 0x5A poison on every destination; block 128KB slides through a 2x window at a random 64B-aligned shared offset per sweep (loop_check_m/memcpy_l2_cache_size input method) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k_cleanup,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

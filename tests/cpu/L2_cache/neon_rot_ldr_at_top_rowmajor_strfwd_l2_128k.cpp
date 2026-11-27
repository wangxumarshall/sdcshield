/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k
 * @parblock
 * Write-side fast-detector probe #1, L2-resident tier 128KB, IDENTITY-COPY
 * chassis (2026-11-26, user directives.  Directive 1: the copy element was
 * MISSING from the parent — the parent's stores carry the rot4(a,b) ALU
 * result of two sources, never the loaded value itself, so a defect
 * coupled to the load-return -> store-data DIRECT path (the write-side
 * mirror of 0102's suspected LSU load-return lineage), or to block-scale
 * copy rhythm, would be missed.  This tier stores exactly what it loads:
 * one asm source ldr, no ALU on the data path, the store data dependency
 * coming straight from the load return.  Directive 2: the input block
 * size is a coupling axis — the effective window is >64KB and <512KB,
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
 *    de-fixation), 0x5A poison on the destination window.
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
 * Probe body (see tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_strfwd.cpp
 * for the compute-chassis parent): asm-pinned same-address str -> ldr
 * back-to-back with per-element in-register compare inside the
 * forwarding window.  The chassis differs from the parent ONLY in the
 * data path: the single source load's value IS the stored value (the
 * memcpy identity), and the compare is against that source value —
 * memcpy_l2_cache_size's memcmp(dst, src) collapsed into the window.
 * The parent's scratch dead load / two sources / rotating ALU are
 * GONE (deliberate: the identity datapath is this tier's variable;
 * compute-chassis L2 geometry lives in git history of the 2026-11-26
 * WIP, superseded by this rewrite per user directive).
 *
 * Window discipline: every sweep draws ONE random 64B-aligned offset
 * (random32(), 4-element granularity) SHARED by every buffer — the
 * source/dst pairing and the per-element compare all run on the
 * ABSOLUTE index i = off + j, so the probe semantics are unchanged
 * under the slide.  The 0x5A poison before each sweep adds the
 * LOST-WRITE face: an element the pinned store never lands on reads
 * back 0x5a5a.. and fails the immediate compare.  All buffers are
 * WIN = 2x LEN elements so every offset keeps the window in bounds;
 * the source is random over the full window (the window rolls, so all
 * of it goes hot over time).
 *
 * DEVIATION from the family's sweep-end check discipline (deliberate,
 * recorded): the check is PER-ELEMENT and immediate — the forwarding
 * window is the axis under test, so the verify must land inside it.
 * The compare is the veorq+vpaddq in-register fold (parent idiom); the
 * branch is never taken on a healthy machine.  No golden buffer
 * exists: the SOURCE VALUE is the reference (the memcpy golden
 * semantics), and the reload subsumes both a corrupted store and a
 * corrupted read-back.  No DSB before the reload — a barrier would
 * close the forwarding window the probe measures.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 8192 uint64x2_t = 128KB (2x L1D,
 * inside the user's >64KB..<512KB window); buffers are src + dst = 2 x
 * 256KB; the per-sweep touched set is 2 x 128KB = 256KB — half of L2,
 * cleanly resident.
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

#define NRSF128_LEN 8192                 /* uint64x2_t elements -> 128KB block */
#define NRSF128_WIN (2 * NRSF128_LEN)    /* 2x-block window (memcpy_l2 method) */

namespace { /* ODR: per-TU NRSF128Data */
struct NRSF128Data {
    uint64x2_t *src;                     /* the single identity source */
};
}

#if defined(__aarch64__)

static inline uint64x2_t nrsf128_load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* the probe: asm-pinned same-address str -> ldr back-to-back (parent,
 * verbatim — the identity is in what gets stored, not in this pair) */
static inline uint64x2_t nrsf128_str_fwd(void *addr, uint64x2_t val) {
    uint64x2_t back;
    __asm__ volatile(
        "str  %q2, [%1]\n\t"
        "ldr  %q0, [%1]\n\t"
        : "=w" (back)
        : "r" (addr), "w" (val)
        : "memory");
    return back;
}

#endif

static int neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k_init(struct test *test)
{
    auto *data = static_cast<NRSF128Data *>(malloc(sizeof(NRSF128Data)));
    data->src = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF128_WIN * sizeof(uint64x2_t)));
    if (!data->src) {
        free(data->src);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->src, NRSF128_WIN * sizeof(uint64x2_t));

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k_cleanup(struct test *test)
{
    auto *data = static_cast<NRSF128Data *>(test->data);
    if (data) {
        free(data->src);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRSF128Data *>(test->data);

    uint64x2_t *dst = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF128_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the source/dst
         * pairing and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRSF128_LEN / 4)));

        /* poison the destination block: a lost write reads back
         * 0x5a5a.. and fails the immediate per-element compare */
        memset(dst + off, 0x5A, NRSF128_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRSF128_LEN; n++) {
            const int j = ascending ? n : (NRSF128_LEN - 1 - n);
            const int i = off + j;

            /* the identity: load the source, store THAT value — no ALU
             * between the load return and the store data */
            const uint64x2_t s = nrsf128_load_vec(&data->src[i]);

            /* the probe: store, then reload the SAME address inside
             * the forwarding window, then compare in-register against
             * the source value (the memcpy golden, in the window) */
            const uint64x2_t back = nrsf128_str_fwd(&dst[i], s);

            const uint64x2_t d = veorq_u64(back, s);
            const uint64x2_t folded = vpaddq_u64(d, d);
            if (vgetq_lane_u64(folded, 0) != 0 || vgetq_lane_u64(folded, 1) != 0) {
                const uint64_t *S = reinterpret_cast<const uint64_t *>(&data->src[i]);
                const uint64_t *G = reinterpret_cast<const uint64_t *>(&back);
                report_fail_msg("strfwd_l2_128k data miscompare: idx %u sweep %lu off %u: "
                                "S=%016lx,%016lx G=%016lx,%016lx X=%016lx,%016lx "
                                "(identity copy, same-address str->ldr forwarding "
                                "window, L2 block 128KB sliding)",
                                (unsigned)i, static_cast<unsigned long>(sweep),
                                (unsigned)off,
                                S[0], S[1], G[0], G[1],
                                G[0] ^ S[0], G[1] ^ S[1]);
                free(dst);
                return EXIT_FAILURE;
            }
        }
        sweep++;
    }

    free(dst);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k, "Write-side fast-detector probe, L2-resident tier 128KB, IDENTITY-COPY chassis: one asm source ldr, no ALU on the data path, asm-pinned same-address str->ldr back-to-back with per-element in-register compare against the source value inside the forwarding window; block 128KB slides through a 2x window at a random 64B-aligned shared offset per sweep, 0x5A destination poison (loop_check_m/memcpy_l2_cache_size input method) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k_cleanup,
    .fracture_loop_count = -1,   /* never fracture: the probe is one tight loop */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

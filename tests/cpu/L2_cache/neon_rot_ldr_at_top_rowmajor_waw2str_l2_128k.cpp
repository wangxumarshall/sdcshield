/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k
 * @parblock
 * Write-side fast-detector probe #2, L2-resident tier 128KB, IDENTITY-COPY
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
 * Probe body (see tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_waw2str.cpp
 * for the compute-chassis parent): asm-pinned same-address str ; str
 * pair.  The chassis differs from the parent ONLY in the data path: the
 * two stores carry the two LOADED SOURCE VALUES directly (two
 * independent random windows src1/src2, no ALU on either data path) —
 * the WAW pair becomes memcpy_l2_cache_size's poison-then-copy shape
 * with a REAL second value: the first write is src1's value, the second
 * write is src2's value, and the second must win.  The parent's
 * rotating-ALU slot pair and scalar golden are GONE (deliberate: the
 * identity datapath is this tier's variable; a compute-chassis L2
 * geometry was authored in a parallel session and superseded per user
 * directive — the compute chassis remains available at the misc/
 * parent).
 *
 * Golden = the src2 WINDOW ITSELF: the sweep-end memcmp(dst, src2)
 * over the block region — memcpy_l2_cache_size's memcmp(dst, src)
 * paradigm, no expected buffer (the source is the golden).  KNOWN
 * BOUNDARY, extended from the parent: a fault corrupting ONLY the
 * first store's data is architecturally unobservable (the second store
 * overwrites it) — this probe's detection face is ORDER.  The L2
 * version's 0x5A poison ADDS the lost-second-write face the parent
 * could not see: if the second store never lands, the element reads
 * back 0x5a5a.. and fails the sweep-end compare.
 *
 * Window discipline: every sweep draws ONE random 64B-aligned offset
 * (random32(), 4-element granularity) SHARED by every buffer — the
 * source/dst pairing runs on the ABSOLUTE index i = off + j, so the
 * probe semantics are unchanged under the slide.  The 0x5A poison
 * covers the destination block region before each sweep.  All buffers
 * are WIN = 2x LEN elements so every offset keeps the window in
 * bounds; both sources are random over their full windows (the windows
 * roll, so all of them go hot over time).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 8192 uint64x2_t = 128KB (2x L1D,
 * inside the user's >64KB..<512KB window); buffers are src1 + src2 +
 * dst = 3 x 256KB; the per-sweep touched set is 3 x 128KB = 384KB —
 * the same hot-set size as loop_check_m's compute pass, cleanly
 * L2-resident.
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

#define NRW2_128_LEN 8192                 /* uint64x2_t elements -> 128KB block */
#define NRW2_128_WIN (2 * NRW2_128_LEN)   /* 2x-block window (memcpy_l2 method) */

namespace { /* ODR: per-TU NRW2_128Data */
struct NRW2_128Data {
    uint64x2_t *src1;                    /* first-write identity source */
    uint64x2_t *src2;                    /* second-write identity source (the golden) */
};
}

#if defined(__aarch64__)

static inline uint64x2_t nrw2_128_load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* the probe: asm-pinned same-address str;str with two different values
 * (parent, verbatim — the identity is in what gets stored) */
static inline void nrw2_128_waw2(void *addr, uint64x2_t first, uint64x2_t second) {
    __asm__ volatile(
        "str  %q0, [%2]\n\t"
        "str  %q1, [%2]\n\t"
        :
        : "w" (first), "w" (second), "r" (addr)
        : "memory");
}

#endif

static int neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k_init(struct test *test)
{
    auto *data = static_cast<NRW2_128Data *>(malloc(sizeof(NRW2_128Data)));
    data->src1 = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_128_WIN * sizeof(uint64x2_t)));
    data->src2 = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_128_WIN * sizeof(uint64x2_t)));
    if (!data->src1 || !data->src2) {
        free(data->src1); free(data->src2);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->src1, NRW2_128_WIN * sizeof(uint64x2_t));
    memset_random(data->src2, NRW2_128_WIN * sizeof(uint64x2_t));

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k_cleanup(struct test *test)
{
    auto *data = static_cast<NRW2_128Data *>(test->data);
    if (data) {
        free(data->src1);
        free(data->src2);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRW2_128Data *>(test->data);

    uint64x2_t *dst = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_128_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the source/dst
         * pairing and the golden semantics unchanged */
        const int off = (int)(4 * (random32() % (NRW2_128_LEN / 4)));

        /* poison the destination block: a lost SECOND write reads back
         * 0x5a5a.. and fails the sweep-end golden compare */
        memset(dst + off, 0x5A, NRW2_128_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRW2_128_LEN; n++) {
            const int j = ascending ? n : (NRW2_128_LEN - 1 - n);
            const int i = off + j;

            /* the identity: two loaded source values, no ALU on either
             * data path — the store data dependency comes straight from
             * the load returns */
            const uint64x2_t s1 = nrw2_128_load_vec(&data->src1[i]);
            const uint64x2_t s2 = nrw2_128_load_vec(&data->src2[i]);

            /* the probe: two stores to the SAME address, back-to-back;
             * the store buffer must retire them in order — the second
             * source value must win */
            nrw2_128_waw2(&dst[i], s1, s2);
        }
        sweep++;

        /* cold path: the SECOND source value must have won — the source
         * window itself is the golden (memcpy's memcmp(dst, src)) */
        if (memcmp(dst + off, data->src2 + off, NRW2_128_LEN * sizeof(uint64x2_t)) != 0) {
            for (int j = 0; j < NRW2_128_LEN; ++j) {
                const int i = off + j;
                if (memcmp(&dst[i], &data->src2[i], sizeof(uint64x2_t)) != 0) {
                    const uint64_t *S1 = reinterpret_cast<const uint64_t *>(&data->src1[i]);
                    const uint64_t *S2 = reinterpret_cast<const uint64_t *>(&data->src2[i]);
                    const uint64_t *G  = reinterpret_cast<const uint64_t *>(&dst[i]);
                    report_fail_msg("waw2str_l2_128k data miscompare: idx %u sweep %lu off %u: "
                                    "S1=%016lx,%016lx S2=%016lx,%016lx G=%016lx,%016lx "
                                    "X=%016lx,%016lx "
                                    "(identity copy, same-address WAW str;str ordering, "
                                    "second source must win, L2 block 128KB sliding)",
                                    (unsigned)i, static_cast<unsigned long>(sweep),
                                    (unsigned)off,
                                    S1[0], S1[1], S2[0], S2[1], G[0], G[1],
                                    G[0] ^ S2[0], G[1] ^ S2[1]);
                    free(dst);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    free(dst);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k, "Write-side fast-detector probe, L2-resident tier 128KB, IDENTITY-COPY chassis: two asm source ldrs, no ALU on either data path, asm-pinned same-address str;str pair of the two loaded source values — store-buffer WAW ordering probe, second source must win; golden = src2 window itself (memcmp(dst,src) paradigm), 0x5A destination poison adds the lost-second-write face; block 128KB slides through a 2x window at a random 64B-aligned shared offset per sweep (loop_check_m/memcpy_l2_cache_size input method) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k_cleanup,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k
 * @parblock
 * Write-side fast-detector probe #1, L2-resident tier 256KB (2026-11-26,
 * user directive: the target machine's defect is block-size-coupled —
 * the effective window is >64KB and <512KB, L2-resident — so the three
 * write-side probes get L2-sized input blocks; these L2 tiers live in
 * tests/cpu/L2_cache/ for use on the target machine).
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
 * Probe body is instruction-for-instruction the strfwd parent (see
 * tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_strfwd.cpp): asm-pinned
 * same-address str -> ldr back-to-back with per-element in-register
 * compare inside the forwarding window, on the integer rowmajor
 * champion chassis (scratch dead asm ldr + two asm source loads +
 * rotating add/eor/and/orr + alternating sweeps).  Only the data
 * geometry differs from the parent.
 *
 * Window discipline: every sweep draws ONE random 64B-aligned offset
 * (random32(), 4-element granularity) SHARED by every buffer — the A/B
 * pairing, the rot4 slot (i%4) and the per-element compare all run on
 * the ABSOLUTE index i = off + j, so the probe semantics are unchanged
 * under the slide (a pure memcpy has no pairing, which is why
 * memcpy_l2_cache_size can slide src/dst independently; this chassis
 * cannot).  The 0x5A poison before each sweep adds the LOST-WRITE face:
 * an element the pinned store never lands on reads back 0x5a5a.. and
 * fails the immediate compare.  All buffers are WIN = 2x LEN elements
 * so every offset keeps the window in bounds; sources are random over
 * the full window (the window rolls, so all of it goes hot over time).
 *
 * Geometry (reference Kunpeng 920: L1D 64KB/core, L2 512KB/core): the
 * input block is 16384 uint64x2_t = 256KB (inside the user's window);
 * buffers are 4 x 512KB; the per-sweep touched set is 4 x 256KB =
 * 1MB — the block is the coupling axis, exactly as memcpy_l2_cache_size
 * touches 2x its len.
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

#define NRSF256_LEN 16384                 /* uint64x2_t elements -> 256KB block */
#define NRSF256_WIN (2 * NRSF256_LEN)    /* 2x-block window (memcpy_l2 method) */

namespace { /* ODR: per-TU NRSF256Data */
struct NRSF256Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
};
}

#if defined(__aarch64__)

static inline uint64x2_t nrsf256_load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* rotating integer consumer: 0=add 1=eor 2=and 3=orr (family chassis) */
static inline uint64x2_t nrsf256_rot4(int slot, uint64x2_t a, uint64x2_t b) {
    switch (slot) {
        case 0:  return vaddq_u64(a, b);
        case 1:  return veorq_u64(a, b);
        case 2:  return vandq_u64(a, b);
        default: return vorrq_u64(a, b);
    }
}

/* the probe: asm-pinned same-address str -> ldr back-to-back */
static inline uint64x2_t nrsf256_str_fwd(void *addr, uint64x2_t val) {
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

static int neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k_init(struct test *test)
{
    auto *data = static_cast<NRSF256Data *>(malloc(sizeof(NRSF256Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF256_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF256_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF256_WIN * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch) {
        free(data->srcA); free(data->srcB); free(data->scratch);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->srcA, NRSF256_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRSF256_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRSF256_WIN * sizeof(uint64x2_t));

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k_cleanup(struct test *test)
{
    auto *data = static_cast<NRSF256Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRSF256Data *>(test->data);

    uint64x2_t *dst = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF256_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the A/B pairing,
         * the rot4 slot and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRSF256_LEN / 4)));

        /* poison the destination window: a lost write reads back
         * 0x5a5a.. and fails the immediate per-element compare */
        memset(dst + off, 0x5A, NRSF256_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRSF256_LEN; n++) {
            const int j = ascending ? n : (NRSF256_LEN - 1 - n);
            const int i = off + j;

            uint64x2_t unused_x = nrsf256_load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = nrsf256_load_vec(&data->srcA[i]);
            uint64x2_t b = nrsf256_load_vec(&data->srcB[i]);
            uint64x2_t r = nrsf256_rot4(i % 4, a, b);

            /* the probe: store, then reload the SAME address inside
             * the forwarding window, then compare in-register */
            uint64x2_t back = nrsf256_str_fwd(&dst[i], r);

            const uint64x2_t d = veorq_u64(back, r);
            const uint64x2_t folded = vpaddq_u64(d, d);
            if (vgetq_lane_u64(folded, 0) != 0 || vgetq_lane_u64(folded, 1) != 0) {
                const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
                const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
                const uint64_t *R = reinterpret_cast<const uint64_t *>(&r);
                const uint64_t *G = reinterpret_cast<const uint64_t *>(&back);
                report_fail_msg("strfwd_l2_256k data miscompare: idx %u sweep %lu off %u: "
                                "A=%016lx,%016lx B=%016lx,%016lx R=%016lx,%016lx "
                                "G=%016lx,%016lx X=%016lx,%016lx "
                                "(same-address str->ldr forwarding window, "
                                "L2 block 256KB sliding)",
                                (unsigned)i, static_cast<unsigned long>(sweep),
                                (unsigned)off,
                                A[0], A[1], B[0], B[1], R[0], R[1], G[0], G[1],
                                G[0] ^ R[0], G[1] ^ R[1]);
                free(dst);
                return EXIT_FAILURE;
            }
        }
        sweep++;
    }

    free(dst);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k, "Write-side fast-detector probe, L2-resident tier 256KB: asm-pinned same-address str->ldr back-to-back on the integer rowmajor champion chassis, per-element in-register compare in the forwarding window; block 256KB slides through a 2x window at a random 64B-aligned shared offset per sweep, 0x5A destination poison (memcpy_l2_cache_size input method) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k_cleanup,
    .fracture_loop_count = -1,   /* never fracture: the probe is one tight loop */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

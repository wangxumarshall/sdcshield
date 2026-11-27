/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_l2_512k
 * @parblock
 * L2-resident tier 512KB of neon_rot_ldr_at_top_rowmajor (2026-11-26,
 * user directive: the input block size is a coupling axis — the effective
 * window is >64KB and <512KB, L2-resident — so the rowmajor champion
 * subfamily gets L2-sized input blocks; these L2 tiers live in
 * tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): unlike
 * the identity-copy trio (strfwd_l2/waw2str_l2/dstr4_l2, same
 * directory), this family's compute chassis IS the probe — the read-side
 * 179 recipe (scratch dead asm ldr at the iteration top + two asm source
 * loads + rotating add/eor/and/orr + back-to-back str/str pair) is the
 * object under test.  Only the data geometry changes from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor.cpp): the two chassis
 * families together form the "chassis x block-size" discrimination grid
 * (the 2026-11-26 parallel-session handover memo: identity axis and
 * rot4 axis are orthogonal — keep both, name by axis).
 *
 * Input-data method transplant (the effective slow detectors' method,
 * 2026-11-26 plan source analysis):
 *  - loop_check_m: 128KB/buffer fixed blocks, L2-resident — the origin
 *    of the tier sizes (its crc_M_BUF_SIZE is 8192x16 = 128KB).
 *  - memcpy_l2_cache_size / _neon: random 64B-aligned offsets in a 2x
 *    window, memset_random input, 0x5A destination poison.  Transplanted
 *    here: FIXED block = tier (control variable), per-sweep random
 *    64B-aligned SHARED offset sliding the block-sized window through a
 *    2x window, 0x5A poison on the compared destination block.
 *
 * Window discipline: every sweep draws ONE random 64B-aligned offset
 * (random32(), 4-element granularity) SHARED by every buffer — the
 * rot4 slot (i%4), the golden phase and the sweep-end compare all run
 * on the ABSOLUTE index i = off + j, so the probe semantics are
 * unchanged under the slide.  All buffers are WIN = 2x LEN elements so
 * every offset keeps the window in bounds.  The scalar golden (issue
 * #373 discipline, inherited verbatim from the parent) is computed at
 * init over the FULL window, so every slid slice has its golden; the
 * sweep-end memcmp narrows to the window slice [off, off+LEN) — under
 * the sliding window only the slice is promised written this sweep.
 *
 * Parent deltas (all deliberate, all geometry-side): COUNT 1024->LEN
 * (32768 elements = 512KB block), buffers sized WIN, per-sweep offset +
 * dst poison, slice compare, tier tag in the failure message.  The hot
 * loop body (loads/rot4/str-str pair, alternating sweeps, TEST_LOOP
 * count) is item-for-item the parent.
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 32768 uint64x2_t = 512KB — the
 * user-named upper boundary, exactly L2 capacity on the reference
 * geometry; per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 512KB = 2.5MB —
 * EXCEEDS L2 and self-evicts (the five-buffer chassis geometry,
 * disclosed honestly; the tier axis is the input block size).  Shared
 * init buffers 4 x 1MB; per-worker run buffers 2 x 1MB.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRRL512_LEN 32768                 /* uint64x2_t elements -> 512KB block */
#define NRRL512_WIN (2 * NRRL512_LEN)     /* 2x-block window (memcpy_l2 method) */

struct NRRL512Data {
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

static int neon_rot_ldr_at_top_rowmajor_l2_512k_init(struct test *test) {
    auto *data = static_cast<NRRL512Data *>(malloc(sizeof(NRRL512Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(64, NRRL512_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(64, NRRL512_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRRL512_WIN * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(64, NRRL512_WIN * sizeof(uint64x2_t)));

    memset_random(data->srcA, NRRL512_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRRL512_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRRL512_WIN * sizeof(uint64x2_t));

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window — pure C integer
     * ops on the scalar pipeline, never the NEON units under test, so
     * every slid slice [off, off+LEN) has its golden. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRRL512_WIN; e++) {
            switch ((e / 2) % 4) {   /* op phase: vector index % 4 */
                case 0: ex[e] = sa[e] + sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & sb[e]; break;
                case 3: ex[e] = sa[e] | sb[e]; break;
            }
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_l2_512k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRRL512Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRRL512_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRRL512_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the rot4 slot,
         * the golden phase and the compare semantics unchanged */
        const int off = (int)(4 * (random32() % (NRRL512_LEN / 4)));

        /* poison the compared destination block: a lost write stays
         * 0x5a5a.. and fails the slice compare */
        memset(dst + off, 0x5A, NRRL512_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRRL512_LEN; n++) {
            const int j = ascending ? n : (NRRL512_LEN - 1 - n);
            const int i = off + j;

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

        /* cold path: slice compare — only the window is promised
         * written this sweep (parent's whole-buffer memcmp narrowed) */
        if (memcmp(dst + off, data->expected + off, NRRL512_LEN * sizeof(uint64x2_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_l2_512k data miscompare "
                            "(sweep %lu off %u, L2 block 512KB sliding)",
                            static_cast<unsigned long>(sweep), (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_l2_512k_cleanup(struct test *test) {
    auto *data = static_cast<NRRL512Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_l2_512k, "L2-resident tier 512KB of the rowmajor champion: chassis preserved (scratch dead ldr + 2-src asm loads + rotating ALU + back-to-back str/str), geometry transplanted (block 512KB slides through a 2x window at a random 64B-aligned shared offset per sweep, 0x5A dst poison, slice memcmp vs full-window scalar golden) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_l2_512k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_l2_512k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_l2_512k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_scalar64_l2_128k
 * @parblock
 * L2-resident tier 128KB of neon_rot_ldr_at_top_rowmajor_scalar64
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * scalar-64b chassis is the object under test — the family's recipe on
 * X registers instead of Q registers (asm volatile ldr/str on "=r"
 * X-regs, 8-byte elements, identical slot/order structure; the asm
 * boundary is load-bearing: DCE guard + layout freeze), inherited
 * verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_scalar64.cpp).
 *
 * ELEMENT WIDTH NOTE: this family's elements are 8 bytes (uint64_t),
 * so the tier element counts are DOUBLE the vector family's: 128KB =
 * 16384 u64, 256KB = 32768, 512KB = 65536.  The 64B alignment of the
 * per-sweep shared offset is 8 elements (8 x 8B = 64B).
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on the compared destination block).
 * The scalar golden covers the FULL window at init; the sweep-end
 * memcmp narrows to the slice [off, off+LEN).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64_t = 128KB (2x L1D,
 * inside the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 128KB = 640KB —
 * slightly exceeds L2 (the five-buffer chassis geometry, disclosed
 * honestly; the tier axis is the input block size).
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>
#endif

#define NRS64128_LEN 16384                 /* uint64_t elements -> 128KB block (8B each) */
#define NRS64128_WIN (2 * NRS64128_LEN)   /* 2x-block window (memcpy_l2 method) */

struct NRS64128Data {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *scratch;
    uint64_t *expected;
};

#if defined(__aarch64__)

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

#endif

static int scalar64_l2_128k_init(struct test *test) {
    auto *data = static_cast<NRS64128Data *>(malloc(sizeof(NRS64128Data)));
    data->srcA    = static_cast<uint64_t *>(aligned_alloc(64, NRS64128_WIN * sizeof(uint64_t)));
    data->srcB    = static_cast<uint64_t *>(aligned_alloc(64, NRS64128_WIN * sizeof(uint64_t)));
    data->scratch = static_cast<uint64_t *>(aligned_alloc(64, NRS64128_WIN * sizeof(uint64_t)));
    data->expected= static_cast<uint64_t *>(aligned_alloc(64, NRS64128_WIN * sizeof(uint64_t)));

    memset_random(data->srcA,    NRS64128_WIN * sizeof(uint64_t));
    memset_random(data->srcB,    NRS64128_WIN * sizeof(uint64_t));
    memset_random(data->scratch, NRS64128_WIN * sizeof(uint64_t));

    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window. */
    for (int e = 0; e < NRS64128_WIN; e++) {
        switch (e % 4) {   /* op phase: element index % 4 */
            case 0: data->expected[e] = data->srcA[e] + data->srcB[e]; break;
            case 1: data->expected[e] = data->srcA[e] ^ data->srcB[e]; break;
            case 2: data->expected[e] = data->srcA[e] & data->srcB[e]; break;
            case 3: data->expected[e] = data->srcA[e] | data->srcB[e]; break;
        }
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int scalar64_l2_128k_run(struct test *test, int cpu) {
    auto *data = static_cast<NRS64128Data *>(test->data);

    uint64_t *temp = static_cast<uint64_t *>(aligned_alloc(64, NRS64128_WIN * sizeof(uint64_t)));
    uint64_t *dst  = static_cast<uint64_t *>(aligned_alloc(64, NRS64128_WIN * sizeof(uint64_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer (8 elements x 8B = 64B granularity): the absolute
         * index i = off + j keeps the op phase and the compare
         * semantics unchanged */
        const int off = (int)(8 * (random32() % (NRS64128_LEN / 8)));

        /* poison the compared destination block: a lost write stays
         * 0x5a5a.. and fails the slice compare */
        memset(dst + off, 0x5A, NRS64128_LEN * sizeof(uint64_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRS64128_LEN; n++) {
            const int j = ascending ? n : (NRS64128_LEN - 1 - n);
            const int i = off + j;

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

        /* cold path: slice compare — only the window is promised
         * written this sweep */
        if (memcmp(dst + off, data->expected + off, NRS64128_LEN * sizeof(uint64_t)) != 0) {
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_scalar64_l2_128k data miscompare "
                            "(sweep %lu off %u, L2 block 128KB sliding)",
                            static_cast<unsigned long>(sweep), (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int scalar64_l2_128k_cleanup(struct test *test) {
    auto *data = static_cast<NRS64128Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_scalar64_l2_128k, "L2-resident tier 128KB of scalar64: scalar-64b chassis preserved (asm X-reg ldr/str, 8B elements — tier counts doubled: 16384/32768/65536), sliding-window geometry transplanted (8-element 64B-aligned offset granularity, 0x5A dst poison, slice memcmp vs full-window scalar golden) (ARM64)")
    .test_init    = scalar64_l2_128k_init,
    .test_run     = scalar64_l2_128k_run,
    .test_cleanup = scalar64_l2_128k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

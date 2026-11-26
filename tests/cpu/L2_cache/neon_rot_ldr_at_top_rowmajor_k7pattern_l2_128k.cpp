/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_k7p_zero_l2_128k /
 *       neon_rot_ldr_at_top_rowmajor_k7p_one_l2_128k /
 *       neon_rot_ldr_at_top_rowmajor_k7p_hiham_l2_128k
 * @parblock
 * L2-resident tier 128KB of neon_rot_ldr_at_top_rowmajor_k7pattern's
 * battery (2026-11-26, user directive: the input block size is a
 * coupling axis — the effective window is >64KB and <512KB,
 * L2-resident — so the rowmajor champion subfamily gets L2-sized
 * input blocks; these L2 tiers live in tests/cpu/L2_cache/).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * operand-pattern axis is the object under test — the battery keeps
 * the parent's same-skeleton/fill-only-delta structure
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_k7pattern.cpp):
 *   k7p_zero_l2  : all-zero operands (both sources)    -> activation floor
 *   k7p_one_l2   : all-one operands                   -> complementary floor
 *   k7p_hiham_l2 : 0x55/0xAA alternating per byte with byte_off parity
 *                  -> maximum transition density
 * The parent's fourth cell (k7p_rand, GPS-tagged random operands) is
 * DELIBERATELY NOT transplanted: random tagged operands are exactly
 * the gps_l2 tier family — a k7p_rand_l2 would be a byte-identical
 * clone (the 名实不符 anti-pattern); its B-track-baseline role is
 * carried by gps_l2.  None of the three transplanted cells uses the
 * GPS tag layout, so the 32-bit tag redesign does not apply here.
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on the compared destination block).
 * The pattern fill and the scalar golden cover the FULL window at
 * init; the sweep-end memcmp narrows to the slice [off, off+LEN).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 8192 uint64x2_t = 128KB (2x L1D,
 * inside the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp + dst writes = 5 x 128KB = 640KB —
 * slightly exceeds L2 (the five-buffer chassis geometry, disclosed
 * honestly; the tier axis is the input block size).
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

#define NRK7P128_LEN 8192                 /* uint64x2_t elements -> 128KB block */
#define NRK7P128_WIN (2 * NRK7P128_LEN)   /* 2x-block window (memcpy_l2 method) */
#define NRK7P128_DUMP_DIR "movbe_log/gps_rowmajor_l2"

enum NRK7P128Pattern { NRK7P128_ZERO, NRK7P128_ONE, NRK7P128_HIHAM };

struct NRK7P128Data {
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

/* pattern fill (inherited verbatim from the parent; no GPS tags in the
 * three transplanted cells) */
static uint64_t nrk7p128_pattern_word(enum NRK7P128Pattern p, uint64_t byte_off) {
    switch (p) {
        case NRK7P128_ZERO:  return 0x0000000000000000ULL;
        case NRK7P128_ONE:   return 0xFFFFFFFFFFFFFFFFULL;
        case NRK7P128_HIHAM: {
            /* high-Hamming with position info: alternate 0x55/0xAA per
             * byte so every adjacent byte pair differs in 8 bits, and
             * byte_off parity is recoverable offline */
            uint64_t w = 0;
            for (int b = 0; b < 8; b++) {
                const uint64_t nib = (((byte_off + static_cast<uint64_t>(b)) & 1) ? 0xAAULL : 0x55ULL);
                w |= (nib << (8 * b));
            }
            return w;
        }
    }
    return 0;
}

static int nrk7p128_init_common(struct test *test, enum NRK7P128Pattern p) {
    auto *data = static_cast<NRK7P128Data *>(malloc(sizeof(NRK7P128Data)));
    data->srcA    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK7P128_WIN * sizeof(uint64x2_t)));
    data->srcB    = static_cast<uint64x2_t *>(aligned_alloc(64, NRK7P128_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(64, NRK7P128_WIN * sizeof(uint64x2_t)));
    data->expected= static_cast<uint64x2_t *>(aligned_alloc(64, NRK7P128_WIN * sizeof(uint64x2_t)));

    mkdir("movbe_log", 0755);
    mkdir(NRK7P128_DUMP_DIR, 0755);

    /* pattern fill over the FULL window (same pattern word family for
     * srcA and srcB — the parent's cells are operand-value cells, not
     * source-family cells) */
    for (int i = 0; i < NRK7P128_WIN; i++) {
        const uint64_t byte = static_cast<uint64_t>(i) * 16;
        store_vec(&data->srcA[i],    vcombine_u64(vcreate_u64(nrk7p128_pattern_word(p, byte + 0)),
                                                  vcreate_u64(nrk7p128_pattern_word(p, byte + 8))));
        store_vec(&data->srcB[i],    vcombine_u64(vcreate_u64(nrk7p128_pattern_word(p, byte + 0)),
                                                  vcreate_u64(nrk7p128_pattern_word(p, byte + 8))));
        store_vec(&data->scratch[i], vcombine_u64(vcreate_u64(nrk7p128_pattern_word(p, byte + 0)),
                                                  vcreate_u64(nrk7p128_pattern_word(p, byte + 8))));
    }
    /* golden (issue #373, inherited verbatim from the parent): scalar
     * independent recomputation over the FULL window. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * NRK7P128_WIN; e++) {
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

static int nrk7p128_run_common(struct test *test, int cpu, const char *cell) {
    auto *data = static_cast<NRK7P128Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(64, NRK7P128_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(64, NRK7P128_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 13) {
        const int off = (int)(4 * (random32() % (NRK7P128_LEN / 4)));

        memset(dst + off, 0x5A, NRK7P128_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRK7P128_LEN; n++) {
            const int j = ascending ? n : (NRK7P128_LEN - 1 - n);
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

        if (memcmp(dst + off, data->expected + off, NRK7P128_LEN * sizeof(uint64x2_t)) != 0) {
            char path[256];
            snprintf(path, sizeof(path), "%s/%s_l2_128k_fail_cpu%d_sweep%lu_off%d.bin", NRK7P128_DUMP_DIR,
                     cell, cpu, static_cast<unsigned long>(sweep), off);
            FILE *f = fopen(path, "wb");
            if (f) {
                fwrite(dst + off,            sizeof(uint64x2_t), NRK7P128_LEN, f);
                fwrite(data->expected + off, sizeof(uint64x2_t), NRK7P128_LEN, f);
                fwrite(data->srcA + off,     sizeof(uint64x2_t), NRK7P128_LEN, f);
                fwrite(data->srcB + off,     sizeof(uint64x2_t), NRK7P128_LEN, f);
                fwrite(data->scratch + off,  sizeof(uint64x2_t), NRK7P128_LEN, f);
                fprintf(f, "%s_l2_128k sweep=%lu off=%d\n",
                        cell, static_cast<unsigned long>(sweep), off);
                fclose(f);
            }
            report_fail_msg("neon_rot_ldr_at_top_rowmajor_%s_l2_128k data miscompare (off %u, records in dump bin)", cell, (unsigned)off);
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

static int nrk7p128_zero_init(struct test *t)  { return nrk7p128_init_common(t, NRK7P128_ZERO); }
static int nrk7p128_one_init(struct test *t)   { return nrk7p128_init_common(t, NRK7P128_ONE); }
static int nrk7p128_hiham_init(struct test *t)  { return nrk7p128_init_common(t, NRK7P128_HIHAM); }

static int nrk7p128_zero_run(struct test *t, int c)  { return nrk7p128_run_common(t, c, "k7p_zero"); }
static int nrk7p128_one_run(struct test *t, int c)   { return nrk7p128_run_common(t, c, "k7p_one"); }
static int nrk7p128_hiham_run(struct test *t, int c)  { return nrk7p128_run_common(t, c, "k7p_hiham"); }

static int nrk7p128_common_cleanup(struct test *test) {
    auto *data = static_cast<NRK7P128Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_zero_l2_128k, "L2-resident tier 128KB of the K7 pattern cell: all-zero operands (activation floor of the 2x64b path), read-side chassis preserved, sliding-window geometry transplanted (ARM64)")
    .test_init    = nrk7p128_zero_init,
    .test_run     = nrk7p128_zero_run,
    .test_cleanup = nrk7p128_common_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_one_l2_128k, "L2-resident tier 128KB of the K7 pattern cell: all-one operands (complementary activation floor), read-side chassis preserved, sliding-window geometry transplanted (ARM64)")
    .test_init    = nrk7p128_one_init,
    .test_run     = nrk7p128_one_run,
    .test_cleanup = nrk7p128_common_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_k7p_hiham_l2_128k, "L2-resident tier 128KB of the K7 pattern cell: 0x55/0xAA alternating high-Hamming operands (maximum transition density), read-side chassis preserved, sliding-window geometry transplanted (ARM64)")
    .test_init    = nrk7p128_hiham_init,
    .test_run     = nrk7p128_hiham_run,
    .test_cleanup = nrk7p128_common_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

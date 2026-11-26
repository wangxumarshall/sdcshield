/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k
 * @parblock
 * L2-resident tier 128KB of neon_rot_ldr_at_top_rowmajor_fp64_dup3
 * (2026-11-26, user directive: the input block size is a coupling axis
 * — the effective window is >64KB and <512KB, L2-resident — so the
 * rowmajor champion subfamily gets L2-sized input blocks; these L2
 * tiers live in tests/cpu/L2_cache/ for use on the target machine).
 *
 * CHASSIS PRESERVED, GEOMETRY TRANSPLANTED (the batch-1 ruling): the
 * ALU-multi-sourcing axis is the object under test — the SAME (a,b)
 * source pair computed THREE times per element through the rotating
 * FP64 consumer (slot steps per repetition, CSE-proof), one store per
 * repetition with dst ALWAYS last, ALL repetition buffers
 * golden-checked (redundancy witnesses); scalar goldens on NaN-free,
 * zero-canonicalized inputs, all inherited verbatim from the parent
 * (tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_fp64_dup3.cpp).
 *
 * Input-data method transplant (the effective slow detectors' method):
 * loop_check_m tier sizes (fixed block = tier) + memcpy_l2_cache_size
 * sliding window (one random 64B-aligned SHARED offset per sweep
 * through a 2x window, 0x5A poison on ALL THREE witness blocks).
 * Sanitize + scalar goldens cover the FULL window at init; the
 * sweep-end memcmps narrow to the slice [off, off+LEN).
 *
 * Geometry (reference host 0102, Kunpeng 920: L1D 64KB/core, L2
 * 512KB/core): the input block is 16384 uint64x2_t = 256KB (inside the user's >64KB..<512KB window); per-sweep touched set =
 * scratch + srcA + srcB reads + temp1 + temp2 + dst writes = 6 x
 * 256KB = 1.5MB — exceeds L2 (the six-buffer chassis geometry of the
 * three-repetition axis, disclosed honestly; the tier axis is the
 * input block size).  Init buffers 6 x 256KB shared; run buffers 3 x
 * 256KB per worker.
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

#define NRD3_256_LEN 16384                  /* uint64x2_t elements -> 256KB block */
#define NRD3_256_WIN (2 * NRD3_256_LEN)   /* 2x-block window (memcpy_l2 method) */

namespace { /* ODR: per-TU NRD3_256Data */
struct NRD3_256Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected1;                 /* slot (i)%4 golden */
    uint64x2_t *expected2;                 /* slot (i+1)%4 golden */
    uint64x2_t *expected3;                 /* slot (i+2)%4 golden */
};
}

#if defined(__aarch64__)

static inline void store_vec(void *addr, uint64x2_t val) {
    vst1q_u64(static_cast<uint64_t *>(addr), val);
}

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* FP64 rotation, slot order mirrors the family: 0=fadd 1=fsub 2=fmul
 * 3=fmax. Reinterpretations are compile-time no-ops. */
static inline uint64x2_t nrd3_128_rot(int slot, uint64x2_t a, uint64x2_t b) {
    const float64x2_t fa = vreinterpretq_f64_u64(a);
    const float64x2_t fb = vreinterpretq_f64_u64(b);
    float64x2_t fres;
    switch (slot) {
        case 0: fres = vaddq_f64(fa, fb); break;
        case 1: fres = vsubq_f64(fa, fb); break;
        case 2: fres = vmulq_f64(fa, fb); break;
        default: fres = vmaxq_f64(fa, fb); break;
    }
    return vreinterpretq_u64_f64(fres);
}

/* scalar golden replica: word-wise, on the sanitized inputs (no vector
 * instruction on this path) */
static uint64_t nrd3_128_srot(int slot, uint64_t a, uint64_t b)
{
    double fa, fb;
    memcpy(&fa, &a, 8);
    memcpy(&fb, &b, 8);
    double fr;
    switch (slot) {
        case 0: fr = fa + fb; break;
        case 1: fr = fa - fb; break;
        case 2: fr = fa * fb; break;
        default: fr = (fa >= fb) ? fa : fb; break;   /* NaN-free + zero-canonicalized */
    }
    uint64_t r;
    memcpy(&r, &fr, 8);
    return r;
}

#endif

static int neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k_init(struct test *test)
{
    auto *data = static_cast<NRD3_256Data *>(malloc(sizeof(NRD3_256Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    data->expected1 = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    data->expected2 = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    data->expected3 = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch || !data->expected1 || !data->expected2 || !data->expected3) {
        free(data->srcA); free(data->srcB); free(data->scratch);
        free(data->expected1); free(data->expected2); free(data->expected3);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->srcA, NRD3_256_WIN * sizeof(uint64x2_t));
    memset_random(data->srcB, NRD3_256_WIN * sizeof(uint64x2_t));
    memset_random(data->scratch, NRD3_256_WIN * sizeof(uint64x2_t));

    /* NaN-free sanitize + zero canonicalization over the FULL window
     * (parent fix, 2026-11-13) */
    {
        auto sanitize = [](uint64x2_t *buf, int n) {
            auto *w = reinterpret_cast<uint64_t *>(buf);
            for (int k = 0; k < n * 2; k++) {
                if ((w[k] >> 52 & 0x7ff) == 0x7ff)
                    w[k] &= ~(1ULL << 62);
                if ((w[k] & 0x7fffffffffffffffULL) == 0)
                    w[k] = 0;
            }
        };
        sanitize(data->srcA, NRD3_256_WIN);
        sanitize(data->srcB, NRD3_256_WIN);
    }

    /* scalar goldens over the FULL window: slots (i)%4, (i+1)%4,
     * (i+2)%4 — one per repetition */
    for (int i = 0; i < NRD3_256_WIN; ++i) {
        const uint64_t a0 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 0];
        const uint64_t a1 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 1];
        const uint64_t b0 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 0];
        const uint64_t b1 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 1];
        reinterpret_cast<uint64_t *>(data->expected1)[2 * i + 0] = nrd3_128_srot(i % 4,       a0, b0);
        reinterpret_cast<uint64_t *>(data->expected1)[2 * i + 1] = nrd3_128_srot(i % 4,       a1, b1);
        reinterpret_cast<uint64_t *>(data->expected2)[2 * i + 0] = nrd3_128_srot((i + 1) % 4, a0, b0);
        reinterpret_cast<uint64_t *>(data->expected2)[2 * i + 1] = nrd3_128_srot((i + 1) % 4, a1, b1);
        reinterpret_cast<uint64_t *>(data->expected3)[2 * i + 0] = nrd3_128_srot((i + 2) % 4, a0, b0);
        reinterpret_cast<uint64_t *>(data->expected3)[2 * i + 1] = nrd3_128_srot((i + 2) % 4, a1, b1);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k_cleanup(struct test *test)
{
    auto *data = static_cast<NRD3_256Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected1);
        free(data->expected2);
        free(data->expected3);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRD3_256Data *>(test->data);

    uint64x2_t *temp1 = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    uint64x2_t *temp2 = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(32, NRD3_256_WIN * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        /* one random 64B-aligned offset per sweep, SHARED by every
         * buffer: the absolute index i = off + j keeps the slot phases
         * and the golden semantics unchanged */
        const int off = (int)(4 * (random32() % (NRD3_256_LEN / 4)));

        /* poison ALL THREE witness blocks: a lost write stays
         * 0x5a5a.. and fails its golden compare */
        memset(temp1 + off, 0x5A, NRD3_256_LEN * sizeof(uint64x2_t));
        memset(temp2 + off, 0x5A, NRD3_256_LEN * sizeof(uint64x2_t));
        memset(dst + off, 0x5A, NRD3_256_LEN * sizeof(uint64x2_t));

        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRD3_256_LEN; n++) {
            const int j = ascending ? n : (NRD3_256_LEN - 1 - n);
            const int i = off + j;

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);

            /* the SAME (a, b) computed three times — rotating slot per
             * repetition (CSE-proof) */
            const uint64x2_t r1 = nrd3_128_rot(i % 4,       a, b);
            const uint64x2_t r2 = nrd3_128_rot((i + 1) % 4, a, b);
            const uint64x2_t r3 = nrd3_128_rot((i + 2) % 4, a, b);

            store_vec(&temp1[i], r1);
            store_vec(&temp2[i], r2);
            store_vec(&dst[i], r3);     /* dst ALWAYS last */
        }
        sweep++;

        /* cold path: ALL THREE repetitions are witnesses — each compared
         * against its per-slot scalar golden, slice [off, off+LEN) */
        if (memcmp(dst + off, data->expected3 + off, NRD3_256_LEN * sizeof(uint64x2_t)) != 0 ||
            memcmp(temp2 + off, data->expected2 + off, NRD3_256_LEN * sizeof(uint64x2_t)) != 0 ||
            memcmp(temp1 + off, data->expected1 + off, NRD3_256_LEN * sizeof(uint64x2_t)) != 0) {
            const uint64x2_t *work[3] = { temp1, temp2, dst };
            const uint64x2_t *gold[3] = { data->expected1, data->expected2, data->expected3 };
            static const char *names[3] = { "temp1(rot#1)", "temp2(rot#2)", "dst(rot#3)" };
            for (int k = 0; k < 3; ++k) {
                for (int q = 0; q < NRD3_256_LEN; ++q) {
                    const int i = off + q;
                    if (memcmp(&work[k][i], &gold[k][i], sizeof(uint64x2_t)) != 0) {
                        const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
                        const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
                        const uint64_t *G = reinterpret_cast<const uint64_t *>(&work[k][i]);
                        const uint64_t *E = reinterpret_cast<const uint64_t *>(&gold[k][i]);
                        report_fail_msg("fp64_dup3_l2_256k data miscompare: %s idx %u sweep %lu off %u: "
                                        "A=%016lx,%016lx B=%016lx,%016lx G=%016lx,%016lx "
                                        "E=%016lx,%016lx X=%016lx,%016lx",
                                        names[k], (unsigned)i,
                                        static_cast<unsigned long>(sweep), (unsigned)off,
                                        A[0], A[1], B[0], B[1], G[0], G[1], E[0], E[1],
                                        G[0] ^ E[0], G[1] ^ E[1]);
                        free(dst); free(temp2); free(temp1);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }

    free(dst);
    free(temp2);
    free(temp1);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k, "L2-resident tier 128KB of fp64_dup3: ALU-multi-sourcing axis preserved (same (a,b) computed three times per element, rotating slot per repetition — CSE-proof, one store per repetition with dst last, all repetition buffers golden-checked), sliding-window geometry transplanted; scalar goldens on NaN-free, zero-canonicalized inputs over the full window (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

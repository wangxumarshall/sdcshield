/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_fp64_dup2
 * @parblock
 * ALU-multi-sourcing extension of the 0102-champion chassis
 * neon_rot_ldr_at_top_rowmajor_fp64 (2026-11-25, user directive:
 * "2src + rotating ALU computing the same data twice + stores").
 *
 * The chassis is the parent's, item for item: the scratch dead ldr at
 * the iteration top (asm-pinned), the two asm source loads (srcA/srcB),
 * the alternating ascending/descending sweeps, the NaN-free input
 * sanitize, the per-sweep cold-path compare.  The NEW AXIS is on the
 * CONSUMER: the same (a, b) source pair is computed TWICE per element —
 * the load-multi-sourcing concept moved onto the ALU, stressing
 * repeated same-source-register vector-op issue (RF read ports /
 * bypass reuse) instead of more load instructions:
 *
 *   r1 = fp64_rot(i   % 4, a, b);   -> str temp
 *   r2 = fp64_rot(i+1 % 4, a, b);   -> str dst (dst ALWAYS last)
 *
 * Rotating-slot realization (recorded ruling): a LITERALLY identical
 * second computation — same slot, same operands — is collapsed by GCC's
 * CSE into one computation, defeating the whole axis; stepping the slot
 * per repetition ((i+k) % 4) keeps every repetition a real distinct
 * vector op while sharing the SAME source data, which is exactly
 * "rotating ALU computing the same data N times".  The objdump gate
 * pins TWO independent computation sites in the hot loop.
 *
 * Deviation from the family's "only dst is compared" store discipline
 * (deliberate, recorded): ALL N result buffers are compared against
 * their per-slot goldens — computation redundancy IS the axis under
 * test, every repetition is a witness; a transient that corrupts only
 * one of the two computations of the same data still fires.
 *
 * Golden: PURE SCALAR (one step beyond the parent's same-units golden;
 * issue #373 paradigm, check-scalar-golden.sh gated): a word-level
 * scalar replica of fp64_rot on the sanitized inputs.  Exactness: the
 * parent's NaN/Inf sanitize (exponent all-ones cleared) removes the
 * operand-order-dependent sNaN quieting that once broke same-units
 * goldens, and this test ADDS zero canonicalization (-0.0 -> +0.0, a
 * 2^-63-probability input) so scalar (fa >= fb) matches vector FMAX
 * bit-for-bit on every remaining input; no fmul+fadd adjacency exists
 * inside a switch branch, so -ffp-contract has nothing to fuse.
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

#define FP64D2_COUNT 1024

namespace { /* ODR: per-TU FP64D2Data */
struct FP64D2Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected1;              /* golden for slot (i)%4     */
    uint64x2_t *expected2;              /* golden for slot (i+1)%4    */
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
static inline uint64x2_t fp64d2_rot(int slot, uint64x2_t a, uint64x2_t b) {
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

/* scalar golden replica: word-wise, on the sanitized inputs (see
 * header exactness argument — no vector instruction on this path) */
static uint64_t fp64d2_srot(int slot, uint64_t a, uint64_t b)
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

static int neon_rot_ldr_at_top_rowmajor_fp64_dup2_init(struct test *test)
{
    auto *data = static_cast<FP64D2Data *>(malloc(sizeof(FP64D2Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));
    data->expected1 = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));
    data->expected2 = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch || !data->expected1 || !data->expected2) {
        free(data->srcA); free(data->srcB); free(data->scratch);
        free(data->expected1); free(data->expected2);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->srcA, FP64D2_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, FP64D2_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, FP64D2_COUNT * sizeof(uint64x2_t));

    /* NaN-free sanitize (parent fix, 2026-11-13) + zero canonicalization
     * (-0.0 -> +0.0: makes scalar (fa >= fb) bit-exact vs vector FMAX) */
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
        sanitize(data->srcA, FP64D2_COUNT);
        sanitize(data->srcB, FP64D2_COUNT);
    }

    /* scalar goldens: slot (i)%4 and slot (i+1)%4, one per repetition */
    for (int i = 0; i < FP64D2_COUNT; ++i) {
        const uint64_t a0 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 0];
        const uint64_t a1 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 1];
        const uint64_t b0 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 0];
        const uint64_t b1 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 1];
        reinterpret_cast<uint64_t *>(data->expected1)[2 * i + 0] = fp64d2_srot(i % 4,       a0, b0);
        reinterpret_cast<uint64_t *>(data->expected1)[2 * i + 1] = fp64d2_srot(i % 4,       a1, b1);
        reinterpret_cast<uint64_t *>(data->expected2)[2 * i + 0] = fp64d2_srot((i + 1) % 4, a0, b0);
        reinterpret_cast<uint64_t *>(data->expected2)[2 * i + 1] = fp64d2_srot((i + 1) % 4, a1, b1);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dup2_cleanup(struct test *test)
{
    auto *data = static_cast<FP64D2Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected1);
        free(data->expected2);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_fp64_dup2_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<FP64D2Data *>(test->data);

    uint64x2_t *temp = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));
    uint64x2_t *dst  = static_cast<uint64x2_t *>(aligned_alloc(32, FP64D2_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < FP64D2_COUNT; n++) {
            const int i = ascending ? n : (FP64D2_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);

            /* the SAME (a, b) computed twice — rotating slot per
             * repetition (CSE-proof; see header) */
            const uint64x2_t r1 = fp64d2_rot(i % 4,       a, b);
            const uint64x2_t r2 = fp64d2_rot((i + 1) % 4, a, b);

            store_vec(&temp[i], r1);
            store_vec(&dst[i], r2);     /* dst ALWAYS last */
        }
        sweep++;

        /* cold path: BOTH repetitions are witnesses — each compared
         * against its per-slot scalar golden */
        if (memcmp(dst, data->expected2, FP64D2_COUNT * sizeof(uint64x2_t)) != 0 ||
            memcmp(temp, data->expected1, FP64D2_COUNT * sizeof(uint64x2_t)) != 0) {
            const uint64x2_t *work[2] = { temp, dst };
            const uint64x2_t *gold[2] = { data->expected1, data->expected2 };
            static const char *names[2] = { "temp(rot#1)", "dst(rot#2)" };
            for (int k = 0; k < 2; ++k) {
                for (int i = 0; i < FP64D2_COUNT; ++i) {
                    if (memcmp(&work[k][i], &gold[k][i], sizeof(uint64x2_t)) != 0) {
                        const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
                        const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
                        const uint64_t *G = reinterpret_cast<const uint64_t *>(&work[k][i]);
                        const uint64_t *E = reinterpret_cast<const uint64_t *>(&gold[k][i]);
                        report_fail_msg("fp64_dup2 data miscompare: %s idx %u sweep %lu: "
                                        "A=%016lx,%016lx B=%016lx,%016lx G=%016lx,%016lx "
                                        "E=%016lx,%016lx X=%016lx,%016lx",
                                        names[k], (unsigned)i,
                                        static_cast<unsigned long>(sweep),
                                        A[0], A[1], B[0], B[1], G[0], G[1], E[0], E[1],
                                        G[0] ^ E[0], G[1] ^ E[1]);
                        free(dst); free(temp);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }

    free(dst);
    free(temp);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_fp64_dup2, "ALU-multi-sourcing extension of the 0102-champion chassis: same (a,b) source pair computed TWICE per element through the rotating FP64 consumer (slot steps per repetition — CSE-proof same-data recomputation), one store per repetition with dst last; ALL repetition buffers golden-checked (redundancy witnesses); scalar goldens on NaN-free, zero-canonicalized inputs (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_fp64_dup2_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_fp64_dup2_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_fp64_dup2_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

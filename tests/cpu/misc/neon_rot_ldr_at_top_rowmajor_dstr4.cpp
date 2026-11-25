/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_dstr4
 * @parblock
 * Write-side fast-detector probe #3 — quad back-to-back destination
 * stores (2026-11-25, user directive: derive a fast detector for the
 * copy-shaped slow-detector machine).
 *
 * Derivation: same as strfwd/waw2str (see those headers) — the target
 * machine's four slow detectors span every tier/construction/strength,
 * leaving copy+write-then-reread as the invariant, pointing at the
 * write path.  This probe amplifies STORE ISSUE DENSITY and
 * store-buffer occupancy instead: every element issues an asm-pinned
 * run of FOUR back-to-back str of the SAME computed value to FOUR
 * different destination buffers.  Four in-flight stores per element
 * is 4x the store pressure of the plain copy the slow detectors run,
 * with the LSU's single store AGI per cycle as the saturation target.
 *
 * The quad run must be asm-pinned: the compiler would otherwise
 * recognize four identical dead-ish stores and collapse/reorder them.
 * Store discipline: dst4 is the LAST store of every run; all FOUR
 * buffers are golden-checked at sweep end (witness discipline — any
 * of the four landing wrong is a detection, the redundancy axis the
 * dup2/dup3 probes established for the ALU side, moved to the store
 * side).
 *
 * Chassis: the integer rowmajor champion (scratch dead asm ldr, two
 * asm source loads, rotating add/eor/and/orr, alternating sweeps,
 * 16KB x 1024 elements, random data).  Golden: pure scalar
 * (check-scalar-golden.sh gated) — the slot i%4 rot64 replica (all
 * four destinations must hold it).
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

#define NRD4_COUNT 1024                 /* uint64x2_t elements -> 16KB/buffer */
#define NRD4_DSTS  4                    /* back-to-back destination stores */

namespace { /* ODR: per-TU NRD4Data */
struct NRD4Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;               /* scalar golden: slot i%4 */
};
}

#if defined(__aarch64__)

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* rotating integer consumer: 0=add 1=eor 2=and 3=orr (family chassis) */
static inline uint64x2_t nrd4_rot4(int slot, uint64x2_t a, uint64x2_t b) {
    switch (slot) {
        case 0:  return vaddq_u64(a, b);
        case 1:  return veorq_u64(a, b);
        case 2:  return vandq_u64(a, b);
        default: return vorrq_u64(a, b);
    }
}

/* the probe: asm-pinned quad back-to-back str of the same value to
 * four destinations (dst4 ALWAYS last) */
static inline void nrd4_dstr4(void *d1, void *d2, void *d3, void *d4,
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

/* scalar golden replica: lane-wise u64 rotation (issue #373 paradigm) */
static inline uint64_t nrd4_rot64(int slot, uint64_t a, uint64_t b)
{
    switch (slot) {
        case 0:  return a + b;
        case 1:  return a ^ b;
        case 2:  return a & b;
        default: return a | b;
    }
}

static int neon_rot_ldr_at_top_rowmajor_dstr4_init(struct test *test)
{
    auto *data = static_cast<NRD4Data *>(malloc(sizeof(NRD4Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_COUNT * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch || !data->expected) {
        free(data->srcA); free(data->srcB); free(data->scratch); free(data->expected);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->srcA, NRD4_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NRD4_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NRD4_COUNT * sizeof(uint64x2_t));

    /* scalar golden: slot i%4 — every destination must hold it */
    for (int i = 0; i < NRD4_COUNT; ++i) {
        const uint64_t a0 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 0];
        const uint64_t a1 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 1];
        const uint64_t b0 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 0];
        const uint64_t b1 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 1];
        reinterpret_cast<uint64_t *>(data->expected)[2 * i + 0] = nrd4_rot64(i % 4, a0, b0);
        reinterpret_cast<uint64_t *>(data->expected)[2 * i + 1] = nrd4_rot64(i % 4, a1, b1);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dstr4_cleanup(struct test *test)
{
    auto *data = static_cast<NRD4Data *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data->expected);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_dstr4_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRD4Data *>(test->data);

    uint64x2_t *dst[NRD4_DSTS];
    for (int k = 0; k < NRD4_DSTS; ++k)
        dst[k] = static_cast<uint64x2_t *>(aligned_alloc(32, NRD4_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRD4_COUNT; n++) {
            const int i = ascending ? n : (NRD4_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            const uint64x2_t r = nrd4_rot4(i % 4, a, b);

            /* the probe: four stores of the same value, back-to-back,
             * four destinations, dst[3] ALWAYS last */
            nrd4_dstr4(&dst[0][i], &dst[1][i], &dst[2][i], &dst[3][i], r);
        }
        sweep++;

        /* cold path: ALL FOUR destinations are witnesses */
        for (int k = 0; k < NRD4_DSTS; ++k) {
            if (memcmp(dst[k], data->expected, NRD4_COUNT * sizeof(uint64x2_t)) != 0) {
                for (int i = 0; i < NRD4_COUNT; ++i) {
                    if (memcmp(&dst[k][i], &data->expected[i], sizeof(uint64x2_t)) != 0) {
                        const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
                        const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
                        const uint64_t *G = reinterpret_cast<const uint64_t *>(&dst[k][i]);
                        const uint64_t *E = reinterpret_cast<const uint64_t *>(&data->expected[i]);
                        report_fail_msg("dstr4 data miscompare: dst#%d idx %u sweep %lu: "
                                        "A=%016lx,%016lx B=%016lx,%016lx G=%016lx,%016lx "
                                        "E=%016lx,%016lx X=%016lx,%016lx "
                                        "(quad back-to-back destination stores)",
                                        k, (unsigned)i, static_cast<unsigned long>(sweep),
                                        A[0], A[1], B[0], B[1], G[0], G[1], E[0], E[1],
                                        G[0] ^ E[0], G[1] ^ E[1]);
                        for (int m = 0; m < NRD4_DSTS; ++m) free(dst[m]);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }

    for (int k = 0; k < NRD4_DSTS; ++k) free(dst[k]);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_dstr4, "Write-side fast-detector probe: asm-pinned quad back-to-back str of the same rotating-ALU value to four destinations on the integer rowmajor champion chassis — store issue density / store-buffer occupancy amplifier, all four buffers golden-checked (witnesses; dst last) (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_dstr4_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_dstr4_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_dstr4_cleanup,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

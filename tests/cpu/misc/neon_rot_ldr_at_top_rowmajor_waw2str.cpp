/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_waw2str
 * @parblock
 * Write-side fast-detector probe #2 — same-address WRITE-AFTER-WRITE
 * pair (2026-11-25, user directive: derive a fast detector for the
 * copy-shaped slow-detector machine).
 *
 * Derivation: same as strfwd (see that header) — the target machine's
 * four slow detectors span every tier/construction/strength, leaving
 * copy+write-then-reread as the invariant, pointing at the write
 * path.  This probe amplifies the WRITE-WRITE hazard instead of the
 * write-read one: every element issues an asm-pinned SAME-ADDRESS
 * str ; str pair with TWO DIFFERENT values (the rotating consumer's
 * slot i%4 and slot (i+1)%4 results of the same sources), so the
 * store buffer must keep both in flight and retire them IN ORDER.
 * The final memory content must be the SECOND value; a WAW ordering
 * or merging fault leaves the first value behind and the sweep-end
 * scalar golden catches it.
 *
 * The pair must be asm-pinned: on plain intrinsics the compiler
 * eliminates the dead first store (DCE) or merges the pair — the
 * neon_rot_2src "asm-guaranteed" lesson again.  Known boundary
 * (deliberate, recorded): a fault that corrupts ONLY the first
 * store's data is architecturally unobservable (the second store
 * overwrites it) — this probe's detection face is ORDER, not the
 * first write's data; strfwd covers the write-data face.
 *
 * Chassis: the integer rowmajor champion (scratch dead asm ldr, two
 * asm source loads, rotating add/eor/and/orr, alternating sweeps,
 * 16KB x 1024 elements, random data).  Golden: pure scalar
 * (check-scalar-golden.sh gated) — the slot (i+1)%4 rot64 replica.
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

#define NRW2_COUNT 1024                 /* uint64x2_t elements -> 16KB/buffer */

namespace { /* ODR: per-TU NRW2Data */
struct NRW2Data {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
    uint64x2_t *expected;               /* scalar golden: slot (i+1)%4 */
};
}

#if defined(__aarch64__)

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* rotating integer consumer: 0=add 1=eor 2=and 3=orr (family chassis) */
static inline uint64x2_t nrw2_rot4(int slot, uint64x2_t a, uint64x2_t b) {
    switch (slot) {
        case 0:  return vaddq_u64(a, b);
        case 1:  return veorq_u64(a, b);
        case 2:  return vandq_u64(a, b);
        default: return vorrq_u64(a, b);
    }
}

/* the probe: asm-pinned same-address str;str with two different values */
static inline void nrw2_waw2(void *addr, uint64x2_t first, uint64x2_t second) {
    __asm__ volatile(
        "str  %q0, [%2]\n\t"
        "str  %q1, [%2]\n\t"
        :
        : "w" (first), "w" (second), "r" (addr)
        : "memory");
}

#endif

/* scalar golden replica: lane-wise u64 rotation (issue #373 paradigm) */
static inline uint64_t nrw2_rot64(int slot, uint64_t a, uint64_t b)
{
    switch (slot) {
        case 0:  return a + b;
        case 1:  return a ^ b;
        case 2:  return a & b;
        default: return a | b;
    }
}

static int neon_rot_ldr_at_top_rowmajor_waw2str_init(struct test *test)
{
    auto *data = static_cast<NRW2Data *>(malloc(sizeof(NRW2Data)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_COUNT * sizeof(uint64x2_t)));
    data->expected = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_COUNT * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch || !data->expected) {
        free(data->srcA); free(data->srcB); free(data->scratch); free(data->expected);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->srcA, NRW2_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NRW2_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NRW2_COUNT * sizeof(uint64x2_t));

    /* scalar golden: the SECOND write wins — slot (i+1)%4 */
    for (int i = 0; i < NRW2_COUNT; ++i) {
        const uint64_t a0 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 0];
        const uint64_t a1 = reinterpret_cast<uint64_t *>(data->srcA)[2 * i + 1];
        const uint64_t b0 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 0];
        const uint64_t b1 = reinterpret_cast<uint64_t *>(data->srcB)[2 * i + 1];
        reinterpret_cast<uint64_t *>(data->expected)[2 * i + 0] =
            nrw2_rot64((i + 1) % 4, a0, b0);
        reinterpret_cast<uint64_t *>(data->expected)[2 * i + 1] =
            nrw2_rot64((i + 1) % 4, a1, b1);
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_waw2str_cleanup(struct test *test)
{
    auto *data = static_cast<NRW2Data *>(test->data);
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

static int neon_rot_ldr_at_top_rowmajor_waw2str_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRW2Data *>(test->data);

    uint64x2_t *dst = static_cast<uint64x2_t *>(aligned_alloc(32, NRW2_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRW2_COUNT; n++) {
            const int i = ascending ? n : (NRW2_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            const uint64x2_t first = nrw2_rot4(i % 4,       a, b);
            const uint64x2_t second = nrw2_rot4((i + 1) % 4, a, b);

            /* the probe: two stores to the SAME address, back-to-back;
             * the store buffer must retire them in order */
            nrw2_waw2(&dst[i], first, second);
        }
        sweep++;

        /* cold path: the SECOND value must have won */
        if (memcmp(dst, data->expected, NRW2_COUNT * sizeof(uint64x2_t)) != 0) {
            for (int i = 0; i < NRW2_COUNT; ++i) {
                if (memcmp(&dst[i], &data->expected[i], sizeof(uint64x2_t)) != 0) {
                    const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
                    const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
                    const uint64_t *G = reinterpret_cast<const uint64_t *>(&dst[i]);
                    const uint64_t *E = reinterpret_cast<const uint64_t *>(&data->expected[i]);
                    report_fail_msg("waw2str data miscompare: idx %u sweep %lu: "
                                    "A=%016lx,%016lx B=%016lx,%016lx G=%016lx,%016lx "
                                    "E=%016lx,%016lx X=%016lx,%016lx "
                                    "(same-address WAW str;str ordering)",
                                    (unsigned)i, static_cast<unsigned long>(sweep),
                                    A[0], A[1], B[0], B[1], G[0], G[1], E[0], E[1],
                                    G[0] ^ E[0], G[1] ^ E[1]);
                    free(dst);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    free(dst);
    return EXIT_SUCCESS;
}

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_waw2str, "Write-side fast-detector probe: asm-pinned same-address str;str pair with two different rotating-ALU values on the integer rowmajor champion chassis — store-buffer WAW ordering probe, second write must win; scalar slot-(i+1) golden (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_waw2str_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_waw2str_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_waw2str_cleanup,
    .fracture_loop_count = -1,   /* never fracture: pass rounds are fast */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

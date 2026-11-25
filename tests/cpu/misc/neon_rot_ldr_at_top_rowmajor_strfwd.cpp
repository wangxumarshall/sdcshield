/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b neon_rot_ldr_at_top_rowmajor_strfwd
 * @parblock
 * Write-side fast-detector probe #1 — store-to-same-address FORWARD
 * RELOAD (2026-11-25, user directive: derive a fast detector for the
 * copy-shaped slow-detector machine, the way the fp64 champion was
 * derived for 0102).
 *
 * Derivation (see the 2026-11-25 session analysis): the target
 * machine's four slow detectors (loop_check_m, memcpy_l2_cache_size,
 * memcpy_l2_cache_size_neon, memcpy1_neon, ~1 fail / 6 h each) span
 * every cache tier, every construction, and every ALU strength — the
 * ONLY invariant is the copy shape (load -> store, then re-read to
 * verify).  A defect coupled to something that horizontal is most
 * plausibly on the WRITE side (store data path / store-buffer / L1D
 * write-allocate), the mirror image of 0102's read-side defect.
 *
 * This probe amplifies the write-then-re-read loop to its tightest
 * form: every element does an asm-pinned SAME-ADDRESS str -> ldr
 * back-to-back pair, and the loaded value is compared against the
 * computed value IN REGISTER immediately.  The str->ldr pair must be
 * asm-pinned because the compiler performs store-to-load forwarding
 * at COMPILE time on plain intrinsics (it would just reuse the stored
 * register and emit no load — the neon_rot_2src "asm-guaranteed
 * str->ldr" lesson).  The immediate reload closes the loop through
 * the store buffer / L1D write path within the forwarding window —
 * exactly the delay the four slow detectors leave at block scale.
 *
 * Chassis: the integer rowmajor champion, item for item — scratch
 * dead asm ldr at the iteration top, the two asm source loads, the
 * rotating add/eor/and/orr consumer, the alternating
 * ascending/descending sweeps, 16KB x 1024-element buffers, random
 * data.
 *
 * DEVIATION from the family's sweep-end check discipline (deliberate,
 * recorded): the check is PER-ELEMENT and immediate — the forwarding
 * window is the axis under test, so the verify must land inside it.
 * The compare is the veorq+vpaddq in-register fold (dump-variant
 * idiom); the branch is never taken on a healthy machine.  No golden
 * buffer exists: the computed value is the reference, and the reload
 * subsumes both a corrupted store and a corrupted read-back.
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

#define NRSF_COUNT 1024                 /* uint64x2_t elements -> 16KB/buffer */

namespace { /* ODR: per-TU NRSFData */
struct NRSFData {
    uint64x2_t *srcA;
    uint64x2_t *srcB;
    uint64x2_t *scratch;
};
}

#if defined(__aarch64__)

static inline uint64x2_t load_vec(const void *addr) {
    uint64x2_t res;
    __asm__ volatile ("ldr %q0, [%1]" : "=w"(res) : "r"(addr) : "memory");
    return res;
}

/* rotating integer consumer: 0=add 1=eor 2=and 3=orr (family chassis) */
static inline uint64x2_t nrsf_rot4(int slot, uint64x2_t a, uint64x2_t b) {
    switch (slot) {
        case 0:  return vaddq_u64(a, b);
        case 1:  return veorq_u64(a, b);
        case 2:  return vandq_u64(a, b);
        default: return vorrq_u64(a, b);
    }
}

/* the probe: asm-pinned same-address str -> ldr back-to-back */
static inline uint64x2_t nrsf_str_fwd(void *addr, uint64x2_t val) {
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

static int neon_rot_ldr_at_top_rowmajor_strfwd_init(struct test *test)
{
    auto *data = static_cast<NRSFData *>(malloc(sizeof(NRSFData)));
    data->srcA = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF_COUNT * sizeof(uint64x2_t)));
    data->srcB = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF_COUNT * sizeof(uint64x2_t)));
    data->scratch = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF_COUNT * sizeof(uint64x2_t)));
    if (!data->srcA || !data->srcB || !data->scratch) {
        free(data->srcA); free(data->srcB); free(data->scratch);
        free(data);
        return EXIT_FAILURE;
    }

    memset_random(data->srcA, NRSF_COUNT * sizeof(uint64x2_t));
    memset_random(data->srcB, NRSF_COUNT * sizeof(uint64x2_t));
    memset_random(data->scratch, NRSF_COUNT * sizeof(uint64x2_t));

    test->data = data;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_strfwd_cleanup(struct test *test)
{
    auto *data = static_cast<NRSFData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->scratch);
        free(data);
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int neon_rot_ldr_at_top_rowmajor_strfwd_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *data = static_cast<NRSFData *>(test->data);

    uint64x2_t *dst = static_cast<uint64x2_t *>(aligned_alloc(32, NRSF_COUNT * sizeof(uint64x2_t)));

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 8) {
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < NRSF_COUNT; n++) {
            const int i = ascending ? n : (NRSF_COUNT - 1 - n);

            uint64x2_t unused_x = load_vec(&data->scratch[i]);
            (void)unused_x;

            uint64x2_t a = load_vec(&data->srcA[i]);
            uint64x2_t b = load_vec(&data->srcB[i]);
            uint64x2_t r = nrsf_rot4(i % 4, a, b);

            /* the probe: store, then reload the SAME address inside
             * the forwarding window, then compare in-register */
            uint64x2_t back = nrsf_str_fwd(&dst[i], r);

            const uint64x2_t d = veorq_u64(back, r);
            const uint64x2_t folded = vpaddq_u64(d, d);
            if (vgetq_lane_u64(folded, 0) != 0 || vgetq_lane_u64(folded, 1) != 0) {
                const uint64_t *A = reinterpret_cast<const uint64_t *>(&data->srcA[i]);
                const uint64_t *B = reinterpret_cast<const uint64_t *>(&data->srcB[i]);
                const uint64_t *R = reinterpret_cast<const uint64_t *>(&r);
                const uint64_t *G = reinterpret_cast<const uint64_t *>(&back);
                report_fail_msg("strfwd data miscompare: idx %u sweep %lu: "
                                "A=%016lx,%016lx B=%016lx,%016lx R=%016lx,%016lx "
                                "G=%016lx,%016lx X=%016lx,%016lx "
                                "(same-address str->ldr forwarding window)",
                                (unsigned)i, static_cast<unsigned long>(sweep),
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

DECLARE_TEST(neon_rot_ldr_at_top_rowmajor_strfwd, "Write-side fast-detector probe: asm-pinned same-address str->ldr back-to-back on the integer rowmajor champion chassis (2-src asm loads + rotating ALU + alternating sweeps), per-element in-register compare inside the forwarding window — the tightest amplification of the copy machines' write-then-re-read invariant (ARM64)")
    .test_init    = neon_rot_ldr_at_top_rowmajor_strfwd_init,
    .test_run     = neon_rot_ldr_at_top_rowmajor_strfwd_run,
    .test_cleanup = neon_rot_ldr_at_top_rowmajor_strfwd_cleanup,
    .fracture_loop_count = -1,   /* never fracture: the probe is one tight loop */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

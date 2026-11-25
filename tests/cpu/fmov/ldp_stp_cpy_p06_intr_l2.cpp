/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b ldp_stp_cpy_p06_intr_l2
 * @parblock
 * L2-residency 5a_a5 data-plane control twin of ldp_stp_cpy_p05_intr_l2
 * (2026-11-25, user directive: capacity-skip-L1 / L2-resident control
 * group).
 *
 * Instruction-for-instruction IDENTICAL to ldp_stp_cpy_p05_intr_l2 —
 * zero asm, 100-round bidirectional ping-pong through the rotating
 * add/eor/and/orr consumer, scalar fold100/fold99 goldens, 2x128KB
 * work buffers (per-buffer 2x L1D capacity-bypasses L1; round active
 * set 256KB clean L2-resident) — with the ONE difference being the
 * MASTER FILL: the 5a_a5 alternating byte pattern (word-constant
 * 0xa55aa55aa55aa55a, every byte 0x5a/a5 by its parity) instead of
 * random bits.  This is the intr L2 arm's fixed-pattern data
 * discriminant, pairing with ldp_stp_cpy_p06_intr (L1-tier 5a_a5) and
 * ldp_stp_cpy_p05_intr_l2 (L2-tier random).
 *
 * Known boundary (same as all p05/p06 forms, recorded deliberately):
 * sweep-end memcmp only; a single-bit error through the and/orr slots
 * can in principle be absorbed by the fold chain; and/orr word
 * degeneration toward 0x00/0xff over ~40 rounds is inherited (only
 * bus switching activity decays).
 * @endparblock
 */

#include "sandstone.h"
#include <arm_neon.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define P06L2_COUNT 8192                /* uint64x2_t elements -> 128KB/buffer */
#define P06L2_BYTES (P06L2_COUNT * 16)
#define P06L2_ROUNDS 100                 /* ping-pong rounds per sweep */

namespace { /* ODR: per-TU P06L2Data */
struct P06L2Data {
    uint64_t *masterA;                  /* shared read-only random master */
    uint64_t *goldenA;                  /* shared scalar fold100 */
    uint64_t *goldenB;                  /* shared scalar fold99 */
};
}

/* scalar golden replica: lane-wise u64 rotation (identical semantics to
 * the p05_intr_l2 twin — bit-comparable goldens by construction) */
static inline uint64_t p06l2_rot64(int slot, uint64_t a, uint64_t b)
{
    switch (slot) {
        case 0:  return a + b;
        case 1:  return a ^ b;
        case 2:  return a & b;
        default: return a | b;
    }
}

static void p06l2_fold_round(const uint64_t *in, uint64_t *out)
{
    for (size_t j = 0; j < P06L2_COUNT / 2; ++j) {
        const int s1 = (int)(j % 4);
        const int s2 = (int)((j + 2) % 4);
        const uint64_t w0 = in[4 * j + 0];
        const uint64_t w1 = in[4 * j + 1];
        const uint64_t w2 = in[4 * j + 2];
        const uint64_t w3 = in[4 * j + 3];
        out[4 * j + 0] = p06l2_rot64(s1, w0, w2);
        out[4 * j + 1] = p06l2_rot64(s1, w1, w3);
        out[4 * j + 2] = p06l2_rot64(s2, w0, w2);
        out[4 * j + 3] = p06l2_rot64(s2, w1, w3);
    }
}

/* hot-path consumer: the same rotation on the vector units */
static inline uint64x2_t p06l2_rot4(int slot, uint64x2_t a, uint64x2_t b)
{
    switch (slot) {
        case 0:  return vaddq_u64(a, b);
        case 1:  return veorq_u64(a, b);
        case 2:  return vandq_u64(a, b);
        default: return vorrq_u64(a, b);
    }
}

static int ldp_stp_cpy_p06_intr_l2_init(struct test *test)
{
    auto *d = new P06L2Data;
    if (!d)
        return EXIT_FAILURE;

    d->masterA = static_cast<uint64_t *>(aligned_alloc(32, P06L2_BYTES));
    d->goldenA = static_cast<uint64_t *>(aligned_alloc(32, P06L2_BYTES));
    d->goldenB = static_cast<uint64_t *>(aligned_alloc(32, P06L2_BYTES));
    if (!d->masterA || !d->goldenA || !d->goldenB) {
        free(d->masterA); free(d->goldenA); free(d->goldenB);
        delete d;
        return EXIT_FAILURE;
    }

    /* THE difference vs p05_intr_l2: 5a_a5 alternating byte fill instead
     * of random bits, as a word-constant store (byte j of every word is
     * 0x5a/a5 by j's parity, 8 | word size — constant-store is pure data
     * movement, a per-byte select loop would auto-vectorize and fail the
     * check-scalar-golden gate; the asm arm hit exactly this, 2026-11-24) */
    for (size_t k = 0; k < P06L2_COUNT * 2; ++k)
        d->masterA[k] = 0xa55aa55aa55aa55aULL;

    /* scalar fold: goldenA starts as masterA, then 100 rounds of
     * ping-pong between goldenA/goldenB — after round 100 goldenA
     * holds fold100, goldenB holds fold99 (odd rounds write B) */
    memcpy(d->goldenA, d->masterA, P06L2_BYTES);
    for (int r = 1; r <= P06L2_ROUNDS; ++r) {
        if (r & 1)
            p06l2_fold_round(d->goldenA, d->goldenB);
        else
            p06l2_fold_round(d->goldenB, d->goldenA);
    }

    test->data = d;
    return EXIT_SUCCESS;
}

static int ldp_stp_cpy_p06_intr_l2_cleanup(struct test *test)
{
    auto *d = static_cast<P06L2Data *>(test->data);
    if (d) {
        free(d->masterA);
        free(d->goldenA);
        free(d->goldenB);
        delete d;
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int ldp_stp_cpy_p06_intr_l2_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<P06L2Data *>(test->data);

    /* per-thread ping-pong work buffers */
    auto *workA = static_cast<uint64_t *>(aligned_alloc(32, P06L2_BYTES));
    auto *workB = static_cast<uint64_t *>(aligned_alloc(32, P06L2_BYTES));
    if (!workA || !workB) {
        free(workA); free(workB);
        report_fail_msg("ldp_stp_cpy_p06_intr_l2: buffer allocation failed");
        return EXIT_FAILURE;
    }

    TEST_LOOP(test, 1 << 8) {
        /* cold reset from the master at the sweep boundary; workB needs
         * none (round 1 fully overwrites it) */
        memcpy(workA, d->masterA, P06L2_BYTES);

        uint64_t *cur = workA, *nxt = workB;
        for (int r = 1; r <= P06L2_ROUNDS; ++r) {
            const uint64_t *ps = cur;
            uint64_t *pd = nxt;
            for (size_t j = 0; j < P06L2_COUNT / 2; ++j) {
                const size_t w = j * 4;
                const uint64x2_t a = vld1q_u64(&ps[w + 0]);   /* elem 2j   */
                const uint64x2_t b = vld1q_u64(&ps[w + 2]);   /* elem 2j+1 */
                const uint64x2_t r0 = p06l2_rot4((int)(j % 4), a, b);
                const uint64x2_t r1 = p06l2_rot4((int)((j + 2) % 4), a, b);
                vst1q_u64(&pd[w + 0], r0);
                vst1q_u64(&pd[w + 2], r1);
            }
            uint64_t *swp = cur; cur = nxt; nxt = swp;
        }

        /* cold path: both buffers against the scalar fold goldens */
        if (memcmp(workA, d->goldenA, P06L2_BYTES) != 0 ||
            memcmp(workB, d->goldenB, P06L2_BYTES) != 0) {
            const uint64_t *gold[2] = { d->goldenA, d->goldenB };
            const uint64_t *work[2] = { workA, workB };
            static const char *names[2] = { "workA(fold100)", "workB(fold99)" };
            for (int k = 0; k < 2; ++k) {
                for (size_t i = 0; i < P06L2_COUNT * 2; ++i) {
                    if (work[k][i] != gold[k][i]) {
                        report_fail_msg("ldp_stp_cpy_p06_intr_l2: %s mismatch "
                                        "at u64 %zu: expected 0x%016lx got "
                                        "0x%016lx (compiler-built 100-round "
                                        "ping-pong rot4 fold, L2-resident "
                                        "2x128KB)",
                                        names[k], i,
                                        (unsigned long)gold[k][i],
                                        (unsigned long)work[k][i]);
                        free(workA); free(workB);
                        return EXIT_FAILURE;
                    }
                }
            }
        }
    }

    free(workA);
    free(workB);
    return EXIT_SUCCESS;
}

DECLARE_TEST(ldp_stp_cpy_p06_intr_l2, "L2-residency 5a_a5 data-plane control twin of ldp_stp_cpy_p05_intr_l2: instruction-identical compiler-built 100-round ping-pong rot4 (zero asm), 2x128KB work buffers (per-buffer 2x L1D capacity-bypasses L1, round active set 256KB clean L2-resident), master filled with the 5a/a5 alternating byte pattern — the intr L2 arm's fixed-pattern data discriminant (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = ldp_stp_cpy_p06_intr_l2_init,
    .test_run = ldp_stp_cpy_p06_intr_l2_run,
    .test_cleanup = ldp_stp_cpy_p06_intr_l2_cleanup,
    .fracture_loop_count = -1,   /* never fracture: sweep loops are fast */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST

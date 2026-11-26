/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b ldp_stp_cpy_p08_intr_l2
 * @parblock
 * L2-residency control twin of ldp_stp_cpy_p08_intr (2026-11-25, user
 * directive: capacity-skip-L1 / L2-resident control group).
 *
 * Instruction-for-instruction IDENTICAL to ldp_stp_cpy_p08_intr —
 * zero asm, vld1q/vst1q element copy in the champion's
 * alternating-sweep skeleton, compiler-chosen encodings — with the ONE
 * difference being the buffer SIZE: 2x128KB instead of 2x16KB
 * (COUNT 8192 vs 1024).  On the reference Kunpeng 920 (L1D 64KB/core,
 * L2 512KB/core, lscpu-measured):
 *
 *   per-buffer 128KB = 2x L1D  -> every buffer CYCLING through the
 *                                 loop evicts its own head before the
 *                                 wrap: L1 is capacity-bypassed, not
 *                                 instruction-bypassed
 *   hot set src+dst = 256KB    -> fits the 512KB private L2: clean
 *                                 L2-resident servicing
 *
 * so the pair (p08_intr, p08_intr_l2) makes CACHE RESIDENCY the only
 * variable of the pure-copy workload (the 346-suite P08 axis held at
 * L1 vs L2) — the same design loop_check_m uses by default
 * (128KB/buffer).  Golden: the source itself (never written), same as
 * all p08 forms.
 * @endparblock
 */

#include "sandstone.h"
#include <arm_neon.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cstdlib>

#define P08L2_COUNT 8192                /* uint64x2_t elements -> 128KB/buffer */
#define P08L2_BYTES (P08L2_COUNT * 16)

namespace { /* ODR: per-TU P08L2Data */
struct P08L2Data {
    uint64_t *src;                     /* shared, read-only after init */
};
}

static int ldp_stp_cpy_p08_intr_l2_init(struct test *test)
{
    auto *d = new P08L2Data;
    if (!d)
        return EXIT_FAILURE;

    d->src = static_cast<uint64_t *>(aligned_alloc(32, P08L2_BYTES));
    if (!d->src) {
        delete d;
        return EXIT_FAILURE;
    }
    memset_random(d->src, P08L2_BYTES);

    test->data = d;
    return EXIT_SUCCESS;
}

static int ldp_stp_cpy_p08_intr_l2_cleanup(struct test *test)
{
    auto *d = static_cast<P08L2Data *>(test->data);
    if (d) {
        free(d->src);
        delete d;
    }
    test->data = nullptr;
    return EXIT_SUCCESS;
}

static int ldp_stp_cpy_p08_intr_l2_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<P08L2Data *>(test->data);

    /* per-thread destination; the source stays shared read-only */
    auto *dst = static_cast<uint64_t *>(aligned_alloc(32, P08L2_BYTES));
    if (!dst) {
        report_fail_msg("ldp_stp_cpy_p08_intr_l2: buffer allocation failed");
        return EXIT_FAILURE;
    }

    uint64_t sweep = 0;
    TEST_LOOP(test, 1 << 12) {
        /* one sweep: full 128KB pure copy, compiler-built (no asm),
         * alternating direction (champion skeleton) */
        const bool ascending = (sweep & 1) == 0;
        for (int n = 0; n < P08L2_COUNT; ++n) {
            const int i = ascending ? n : (P08L2_COUNT - 1 - n);
            const uint64x2_t v = vld1q_u64(&d->src[i * 2]);
            vst1q_u64(&dst[i * 2], v);
        }
        sweep++;

        /* cold path: the source is the golden (never written) */
        if (memcmp(dst, d->src, P08L2_BYTES) != 0) {
            for (size_t i = 0; i < P08L2_COUNT * 2; ++i) {
                if (dst[i] != d->src[i]) {
                    report_fail_msg("ldp_stp_cpy_p08_intr_l2: dst mismatch at "
                                    "u64 %zu: expected 0x%016lx got 0x%016lx "
                                    "(compiler-built pure copy, L2-resident "
                                    "2x128KB)",
                                    i, (unsigned long)d->src[i],
                                    (unsigned long)dst[i]);
                    free(dst);
                    return EXIT_FAILURE;
                }
            }
        }
    }

    free(dst);
    return EXIT_SUCCESS;
}

DECLARE_TEST(ldp_stp_cpy_p08_intr_l2, "L2-residency control twin of ldp_stp_cpy_p08_intr: instruction-identical compiler-built pure copy (zero asm, alternating-sweep skeleton), buffers 2x128KB — per-buffer 2x L1D capacity-bypasses L1, hot set 256KB is clean L2-resident (346-suite P08 at the L2 tier) (ARM64)")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = ldp_stp_cpy_p08_intr_l2_init,
    .test_run = ldp_stp_cpy_p08_intr_l2_run,
    .test_cleanup = ldp_stp_cpy_p08_intr_l2_cleanup,
    .fracture_loop_count = -1,   /* never fracture: sweep loops are fast */
    .quality_level = TEST_QUALITY_PROD
END_DECLARE_TEST

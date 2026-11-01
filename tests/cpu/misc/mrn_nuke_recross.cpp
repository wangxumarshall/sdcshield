/**
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_nuke_recross
 * @parblock
 * Variant of mrn_nuke that replaces the store-to-load forwarding path with the
 * movbe-style "two loads of the same source address" (L1D array re-read) path,
 * to test whether L1D same-address re-read is a portable trigger independent
 * of the forwarding mechanism.
 *
 * movbe objdump hot loop (trigger):
 *   ldr w1,[input,i]        ; 1st read of input
 *   rev w4,w1               ; ALU (byte swap)
 *   str w4,[swapped,i]      ; store to a DIFFERENT address (swapped)
 *   ldr w4,[input,i]        ; 2nd read of input (SAME address re-read!)
 *   cmp w4,w1
 *
 * This variant reproduces that exact sequence on the mrn_nuke skeleton:
 *   ldr src[i] → rev(ALU) → str dst[i](different addr) → ldr src[i](re-read) → cmp
 * The forwarding path (store temp; load temp) is removed; the 2nd load reads
 * the original source address again (L1D array re-read, not forwarding).
 *
 * Uses inline asm with [base, idx, lsl #3] addressing to fix the schedule
 * (avoid the addressing-mode breakage seen in movbe_l1d_variants, per
 * movbe-l1d-variants-schedule-sensitivity). Early-clobber (=&r) on address
 * registers to avoid the early-clobber violation crash.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_NUKE_RECROSS_COUNT 1024

struct MrnNukeRecrossData {
    uint64_t *src;
};

#if defined(__aarch64__)

/*
 * Inline asm hot loop (reproduce movbe's re-read pattern on uint64):
 *   1. ldr xval, [srcbase, idx, lsl #3]   ; 1st read of src[i]
 *   2. rev/ALU on xval (eor with constant, as uint64 ALU stand-in for rev)
 *   3. str xval, [dstbase, idx, lsl #3]   ; store to DIFFERENT address (dst)
 *   4. ldr xret, [srcbase, idx, lsl #3]   ; 2nd read of src[i] (SAME addr!)
 *   5. cmp xret, xval  (done in C via comparison)
 *
 * The 2nd ldr reads the SAME src address as the 1st (not the stored dst),
 * forcing the L1D array re-read path (no forwarding, since no store to src).
 */
static inline uint64_t recross_load_alu_store_reread(
        uint64_t *dst, const uint64_t *src, uint64_t idx, uint64_t xconst)
{
    uint64_t val, ret;
    __asm__ volatile (
        "ldr %[val], [%[srcbase], %[idx], lsl #3]\n\t"   /* 1. 1st read src[i] */
        "eor %[val], %[val], %[xconst]\n\t"              /* 2. ALU (eor as data-ALU stand-in) */
        "str %[val], [%[dstbase], %[idx], lsl #3]\n\t"   /* 3. store to dst[i] (DIFFERENT addr) */
        "ldr %[ret], [%[srcbase], %[idx], lsl #3]\n\t"   /* 4. 2nd read src[i] (SAME addr re-read!) */
        : [val] "=&r"(val), [ret] "=&r"(ret)
        : [srcbase] "r"(src), [dstbase] "r"(dst),
          [idx] "r"(idx), [xconst] "r"(xconst)
        : "memory"
    );
    return ret;  /* caller compares ret to the expected value */
}

#endif

static int mrn_nuke_recross_init(struct test *test) {
    auto *data = static_cast<MrnNukeRecrossData *>(malloc(sizeof(MrnNukeRecrossData)));
    data->src = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_RECROSS_COUNT * sizeof(uint64_t)));
    memset_random(data->src, MRN_NUKE_RECROSS_COUNT * sizeof(uint64_t));
    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_nuke_recross_run(struct test *test, int cpu) {
    auto *data = static_cast<MrnNukeRecrossData *>(test->data);

    // dst is the DIFFERENT-address store target (not re-read); per-thread
    uint64_t *dst = static_cast<uint64_t *>(aligned_alloc(64, MRN_NUKE_RECROSS_COUNT * sizeof(uint64_t)));
    const uint64_t xconst = 0x5A5A5A5A5A5A5A5AULL;

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_NUKE_RECROSS_COUNT; i++) {
            // 1st read src[i] → ALU(eor) → store dst[i] → 2nd read src[i] (re-read) → compare
            // Expected: the 2nd read should return the ORIGINAL src[i] (store went to dst, not src)
            uint64_t expected = data->src[i];
            uint64_t actual = recross_load_alu_store_reread(dst, data->src, i, xconst);
            if (actual != expected) {
                report_fail_msg("mrn_nuke_recross re-read mismatch at %d: "
                                "expected 0x%llx got 0x%llx (xor 0x%llx)",
                                i,
                                static_cast<unsigned long long>(expected),
                                static_cast<unsigned long long>(actual),
                                static_cast<unsigned long long>(expected ^ actual));
            }
        }
    }

    free(dst);
    return EXIT_SUCCESS;
}

static int mrn_nuke_recross_cleanup(struct test *test) {
    auto *data = static_cast<MrnNukeRecrossData *>(test->data);
    if (data) {
        free(data->src);
        free(data);
    }
    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_nuke_recross, "mrn_nuke variant: movbe-style L1D same-address re-read (2nd load re-reads src, not forwarding)")
    .test_init = mrn_nuke_recross_init,
    .test_run = mrn_nuke_recross_run,
    .test_cleanup = mrn_nuke_recross_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

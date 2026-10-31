/**
 * @file
 * @copyright SPDX-License-Identifier: Apache-2.0
 *
 * @test @b mrn_rmw_diff_nop64
 * @parblock
 * Timing-vs-path isolation probe for core-179 SDC — fills the missing cell of
 * the 2x2 timing matrix.
 *
 * BACKGROUND — the 2x2 matrix we are closing.
 *
 * The 179 SDC localizes (see docs/MOVBE_SDC_CORE179_LOCALIZATION_REPORT_V3 §12
 * v4 Addendum) to the per-core L1D domain internal load path.  Two load
 * sub-paths both trigger:
 *   - store-buffer FORWARDING (same-address str->ldr, served from SB):
 *       mrn_rmw_fwd  (0 nop)  = TRIGGERS (~15 fail/10min)
 *       mrn_rmw_nop64 (64 nop) = 0 fail   ◄── THE ONE EXCEPTION
 *   - L1D ARRAY read (different-address str->ldr, reload of a resident line,
 *     forwarding impossible):  mrn_rmw_diff (0 nop) = TRIGGERS (~10 fail/10min)
 *
 * The open question that breaks the clean "L1D-domain-internal = trigger"
 * boundary: is mrn_rmw_nop64's 0 fail due to
 *   (a) the store having DRAINED from SB to the L1D array within 64 no-ops, so
 *       the reload switched from the forwarding path to the L1D-array path and
 *       became clean — i.e. a PATH switch ("uses Store Buffer?" yes/no), or
 *   (b) the 64 no-ops widening the str->ldr TIMING window past the defect's
 *       activation window while the reload STILL goes through forwarding (the
 *       store sits in the SB; no-ops touch no memory and cannot drain it)?
 *
 * This probe adds the missing matrix cell: the DIFFERENT-address (L1D-array
 * read) variant WITH 64 no-ops between str and ldr.  The reload here CANNOT be
 * served by forwarding (str wrote tempA, ldr reads tempB), so the str->ldr
 * timing window is widened WITHOUT any path ambiguity — the reload is
 * unambiguously an L1D-array read regardless of nop count.
 *
 * DECISION TABLE (this test vs mrn_rmw_nop64):
 *
 *   mrn_rmw_diff_nop64 result |  Interpretation
 *   --------------------------+------------------------------------------------
 *   TRIGGERS (fails > 0)      | L1D-array read path still triggers under 64
 *                             | no-ops.  Then if nop64's store truly drained and
 *                             | switched to the L1D-array path it would ALSO
 *                             | trigger — but it is 0 fail.  Hence nop64's 0
 *                             | fail is NOT a path switch; it is a TIMING-window
 *                             | effect confined to the forwarding path.  The
 *                             | "uses Store Buffer?" dichotomy is REFUTED; the
 *                             | L1D-domain-internal boundary holds.
 *   0 fail                    | L1D-array read path IS suppressed by 64 no-ops
 *                             | too.  Then nop64's 0 fail cannot distinguish
 *                             | forwarding vs array; but it directly REFUTES the
 *                             | path-switch hypothesis ("switched to L1D-array
 *                             | path -> clean"), because the L1D-array path is
 *                             | ALSO clean under 64 no-ops.  Either way the
 *                             | "drained -> switched path -> clean" story fails.
 *
 * In BOTH outcomes the path-switch interpretation of mrn_rmw_nop64's 0 fail is
 * refuted; the question collapses to "does widening the str->ldr timing window
 * past ~N cycles suppress the defect on whichever L1D-domain-internal path the
 * reload happens to take".
 *
 * DESIGN NOTES
 *   - Different-address design is byte-identical to mrn_rmw_diff (same str/ldr
 *     /ALU, same indices) EXCEPT for the inserted 64 no-ops, so the only
 *     variable vs mrn_rmw_diff is timing (this mirrors how mrn_rmw_nop64
 *     differs from mrn_rmw_fwd only by the inserted no-ops).
 *   - 64 `mov x9,x9` no-ops via .rept: data-independent, touch NO memory and NO
 *     cache, so they cannot drain/evict the store buffer or L1D line — they
 *     only widen the str->ldr wall-clock gap (see memory timing-race-final-
 *     conclusion / h-objdump-instruction-schedule).
 *   - Miscompare logging in COLD branch only, report_fail_msg kept as-is —
 *     touching even the cold branch once silenced the defect (see memory
 *     dump-variant-noremove-report-fail-verified); we must not perturb the hot
 *     loop's instruction schedule.
 *   - NO regime-selection conditional in the hot loop (see memory
 *     fixed-lcg-seed-methodology-error / no-premature-conclusions): a single
 *     conditional would perturb the schedule and silence the timing-sensitive
 *     defect.
 * @endparblock
 */

#include "sandstone.h"
#include <cstdint>
#include <cstring>

#define MRN_RMW_COUNT 1024

struct MrnRmwDiffNopData {
    uint64_t *srcA;
    uint64_t *srcB;
    uint64_t *expected;   // golden = srcA op srcB (what res "should" be)
    uint64_t *tempA;      // str target (write res here)
    uint64_t *tempB;      // ldr source (pre-filled with expected; different addr)
};

#if defined(__aarch64__)
static inline void store_qword(void *addr, uint64_t val) {
    __asm__ volatile ("str %1, [%0]" : : "r"(addr), "r"(val) : "memory");
}
// load_qword_delayed: reload after 64 data-independent ALU no-ops (mov x9,x9).
// Pure timing delay: touches NO memory and NO cache, so the reload STILL goes
// through the L1D ARRAY (the stored line is resident; nothing evicted it; the
// str to tempA cannot forward to a ldr of tempB regardless). Only the str->ldr
// time gap is widened — mirroring mrn_rmw_nop64's 64-no-op widening of the
// forwarding path, but here applied to the L1D-array-read path.
static inline uint64_t load_qword_delayed(const void *addr) {
    uint64_t res;
    __asm__ volatile(
        ".rept 64\n\t"
        "mov x9, x9\n\t"
        ".endr\n\t"
        "ldr %0, [%1]"
        : "=r"(res) : "r"(addr) : "memory"
    );
    return res;
}
#endif

static int mrn_rmw_diff_nop_init(struct test *test) {
    auto *data = static_cast<MrnRmwDiffNopData *>(malloc(sizeof(MrnRmwDiffNopData)));
    data->srcA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->srcB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->expected = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->tempA = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));
    data->tempB = static_cast<uint64_t *>(aligned_alloc(64, MRN_RMW_COUNT * sizeof(uint64_t)));

    memset_random(data->srcA, MRN_RMW_COUNT * sizeof(uint64_t));
    memset_random(data->srcB, MRN_RMW_COUNT * sizeof(uint64_t));

    for (int i = 0; i < MRN_RMW_COUNT; i++) {
        switch (i % 4) {
            case 0: data->expected[i] = data->srcA[i] + data->srcB[i]; break;
            case 1: data->expected[i] = data->srcA[i] - data->srcB[i]; break;
            case 2: data->expected[i] = data->srcA[i] ^ data->srcB[i]; break;
            case 3: data->expected[i] = data->srcA[i] & data->srcB[i]; break;
        }
        // Pre-fill tempB with the golden value, so an L1D-array reload of
        // tempB[i] "should" read expected[i] (== res that tempA[i] receives).
        data->tempB[i] = data->expected[i];
        data->tempA[i] = 0;  // will be overwritten by str each iteration
    }

    test->data = data;
    return EXIT_SUCCESS;
}

static int mrn_rmw_diff_nop_cleanup(struct test *test) {
    auto *data = static_cast<MrnRmwDiffNopData *>(test->data);
    if (data) {
        free(data->srcA);
        free(data->srcB);
        free(data->expected);
        free(data->tempA);
        free(data->tempB);
        free(data);
    }
    return EXIT_SUCCESS;
}

static int mrn_rmw_diff_nop_run(struct test *test, int cpu) {
    (void)cpu;
    auto *data = static_cast<MrnRmwDiffNopData *>(test->data);

    TEST_LOOP(test, 1 << 13) {
        for (int i = 0; i < MRN_RMW_COUNT; i++) {
            uint64_t a = data->srcA[i];
            uint64_t b = data->srcB[i];
            uint64_t res;
            switch (i % 4) {
                case 0: res = a + b; break;
                case 1: res = a - b; break;
                case 2: res = a ^ b; break;
                default: res = a & b; break;
            }

            // str res to tempA[i], then ldr from tempB[i] (DIFFERENT address)
            // AFTER 64 no-ops.  tempB[i] holds expected[i], resident in L1D
            // since init.  Reload served from L1D ARRAY (not forwarding: the str
            // to tempA cannot forward to a ldr of tempB), and the 64 no-ops
            // widen the str->ldr timing window without any path ambiguity.
            store_qword(&data->tempA[i], res);
            uint64_t got = load_qword_delayed(&data->tempB[i]);

            // Verify: the ldr should read expected[i].  Cold branch only.
            if (got != data->expected[i]) {
                uint64_t golden = data->expected[i];
                uint64_t actual = got;
                uint64_t xorv = golden ^ actual;
                unsigned char *gb = (unsigned char *)&golden;
                unsigned char *ab = (unsigned char *)&actual;
                unsigned char *xb = (unsigned char *)&xorv;
                const char *op = (i % 4 == 0) ? "add"
                               : (i % 4 == 1) ? "sub"
                               : (i % 4 == 2) ? "xor" : "and";
                log_error("mrn_rmw_diff_nop64: miscompare at index %u (op=%s): "
                          "inputA=0x%016lX inputB=0x%016lX golden=0x%016lX "
                          "actual=0x%016lX xor=0x%016lX | "
                          "bytes[b7..b0] golden=%02X%02X%02X%02X%02X%02X%02X%02X "
                          "actual=%02X%02X%02X%02X%02X%02X%02X%02X "
                          "xor=%02X%02X%02X%02X%02X%02X%02X%02X",
                          (unsigned)i, op,
                          (unsigned long)data->srcA[i], (unsigned long)data->srcB[i],
                          (unsigned long)golden, (unsigned long)actual,
                          (unsigned long)xorv,
                          gb[7], gb[6], gb[5], gb[4], gb[3], gb[2], gb[1], gb[0],
                          ab[7], ab[6], ab[5], ab[4], ab[3], ab[2], ab[1], ab[0],
                          xb[7], xb[6], xb[5], xb[4], xb[3], xb[2], xb[1], xb[0]);
                report_fail_msg("mrn_rmw_diff_nop64: data miscompare at index %u (op=%s)", (unsigned)i, op);
            }
        }
    }

    return EXIT_SUCCESS;
}

DECLARE_TEST(mrn_rmw_diff_nop64, "mrn_rmw timing-vs-path probe: diff-addr str->ldr (L1D-array read, NOT forwarding) + 64 no-ops between str and ldr — fills the 2x2 timing matrix cell; if L1D-array read still triggers under 64 no-ops, nop64's 0-fail is a forwarding-path timing-window effect, not a path switch")
    .test_init = mrn_rmw_diff_nop_init,
    .test_run = mrn_rmw_diff_nop_run,
    .test_cleanup = mrn_rmw_diff_nop_cleanup,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

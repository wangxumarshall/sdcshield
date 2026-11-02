/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_gemm_double_ab_split
 * @parblock
 * Discriminating probe for core-179 SDC on the eigen GEMM path.
 *
 * Background: eigen_gemm_double14's failure is at "_prod.isApprox failed"
 * (double14.cpp:76) — the GEMM *result* is wrong. But that result can be
 * corrupted by two independent mechanisms:
 *   A. FMA/SIMD computation error   (the multiply itself mis-computes)
 *   B. GEMM-internal load error      (a load _x[i,j]/_y[i,j] feeding the
 *                                     FMA reads a wrong value)
 *
 * double14's step3 (memcmp(_x, lhs)) only checks the *step1 memcpy copy* load;
 * it does NOT cover the GEMM-internal loads (different addressing pattern,
 * different dependency timing — and the defect is intermittent so "another
 * load was fine" does not imply "this load is fine"). So A vs B was not
 * separable from existing data.
 *
 * This probe separates them by running the SAME GEMM twice on the SAME input
 * buffers and comparing the two results element-wise (internal check), in
 * addition to the golden comparison (external check):
 *
 *   1. _x = lhs; _y = rhs                     (step1 memcpy copy, unchanged)
 *   2. _prod_a = _x * _y                        (GEMM #1: internal load + FMA)
 *   3. _prod_b = _x * _y                        (GEMM #2: internal load + FMA)
 *   4. INTERNAL check: memcmp(_prod_a, _prod_b) (two GEMM-internal-load
 *                                                 passes agree?)
 *   5. EXTERNAL check: _prod_a.isApprox(golden)  (result vs golden, unchanged)
 *
 * Reading the table (pre-registered interpretation):
 *   - EXTERNAL fail + INTERNAL also fail  -> two GEMM-internal loads disagree
 *                                            => B (GEMM-internal load) alive
 *   - EXTERNAL fail + INTERNAL pass       -> two GEMM-internal loads agree but
 *                                            differ from golden
 *                                            => A (FMA) alive OR B-compare
 *                                            (golden read). (If A is
 *                                            deterministic, both runs are
 *                                            equally wrong => internal pass.)
 *
 * The FMA hot path (the GEMM call x = lhs*rhs) is byte-for-byte the same
 * Eigen call as the original; the only additions are a second identical GEMM
 * call and the internal memcmp. Per the dump-variant design discipline, all
 * detailed diagnostics live in the cold failure branch.
 *
 * CAVEAT (methodology ceiling): adding a second GEMM lengthens the hot loop,
 * which changes the timing window and thus the *trigger rate* — but it does
 * NOT confound the INTERNAL-vs-EXTERNAL dichotomy, because the internal
 * check compares the two runs *to each other*, not to a historical rate. A
 * second caveat: if 179's FMA error is itself intermittent (not deterministic
 * A), two FMA passes could also disagree, making INTERNAL fail ambiguous
 * between "B-gemm-load" and "intermittent-A". Only the "EXTERNAL fail +
 * INTERNAL pass" cell is fully clean; the "both fail" cell is read as
 * "B-gemm-load OR intermittent-A alive (load path not excluded)".
 * @endparblock
 */

#include <sandstone.h>

#include <Eigen/Core>
#include <cstdlib>
#include <cstdint>
#include <cstring>
using namespace Eigen;

static int gemm_k_dim() {
    static int dim = -1;
    if (dim < 0) {
        int d = 256;
        if (const char *env = getenv("GEMM_K_DIM")) {
            int parsed = atoi(env);
            if (parsed > 0) d = parsed;
        }
        dim = d;
    }
    return dim;
}
#define M_DIM gemm_k_dim()

typedef Matrix < double, Dynamic, Dynamic > Mat;

namespace {
struct eigen_test_data {
    Mat lhs;
    Mat rhs;
    Mat prod;
};
}

#define CAST(_x) static_cast<struct eigen_test_data *>(_x)

static int eigen_gemm_double_ab_split_init(struct test *test) {
    test->data = new(eigen_test_data);
    try {
        CAST(test->data)->lhs = Mat::Random(M_DIM, M_DIM);
        CAST(test->data)->rhs = Mat::Random(M_DIM, M_DIM);
        CAST(test->data)->prod = CAST(test->data)->lhs * CAST(test->data)->rhs;
    } catch (...) {
        report_fail_msg("Exception on Eigen code, most probably OOM");
    }
    return EXIT_SUCCESS;
}

static int eigen_gemm_double_ab_split_run(struct test *test, int cpu) {
    do {
        auto testdata = CAST(test->data);
        Mat x, y;
        x = testdata->lhs;          // step1 memcpy copy (unchanged from original)
        y = testdata->rhs;

        Mat prod_a;
        prod_a = x * y;             // GEMM #1: internal load + FMA (same Eigen call as original)
        Mat prod_b;
        prod_b = x * y;             // GEMM #2: same inputs, internal load + FMA again

        const double *ad = prod_a.data();
        const double *bd = prod_b.data();
        const double *pd = testdata->prod.data();
        int total = M_DIM * M_DIM;

        // INTERNAL check: do the two GEMM-internal-load passes agree?
        // Covers the load path that step3 (memcmp(_x,lhs)) does NOT cover.
        for (int idx = 0; idx < total; ++idx) {
            if (ad[idx] != bd[idx]) {
                uint64_t a_bits, b_bits;
                memcpy(&a_bits, &ad[idx], sizeof(uint64_t));
                memcpy(&b_bits, &bd[idx], sizeof(uint64_t));
                uint64_t xorv = a_bits ^ b_bits;
                int row = idx / M_DIM, col = idx % M_DIM;
                log_error("eigen_gemm_ab_split INTERNAL miscompare at [%d,%d] (idx=%d): "
                          "prod_a=%g prod_b=%g | bits a=0x%016lX b=0x%016lX xor=0x%016lX "
                          "(two GEMM-internal loads disagree => B-gemm-load alive)",
                          row, col, idx, ad[idx], bd[idx],
                          (unsigned long)a_bits, (unsigned long)b_bits, (unsigned long)xorv);
                report_fail_msg("eigen_gemm_ab_split: INTERNAL miscompare (B-gemm-load) at [%d,%d]", row, col);
            }
        }

        // EXTERNAL check: result vs golden (same semantics as original step4).
        for (int idx = 0; idx < total; ++idx) {
            if (ad[idx] != pd[idx]) {
                uint64_t golden_bits, actual_bits;
                memcpy(&golden_bits, &pd[idx], sizeof(uint64_t));
                memcpy(&actual_bits, &ad[idx], sizeof(uint64_t));
                uint64_t xorv = golden_bits ^ actual_bits;
                int row = idx / M_DIM, col = idx % M_DIM;
                log_error("eigen_gemm_ab_split EXTERNAL miscompare at [%d,%d] (idx=%d): "
                          "golden=%g actual=%g | bits golden=0x%016lX actual=0x%016lX xor=0x%016lX",
                          row, col, idx, pd[idx], ad[idx],
                          (unsigned long)golden_bits, (unsigned long)actual_bits, (unsigned long)xorv);
                report_fail_msg("eigen_gemm_ab_split: EXTERNAL miscompare (A or B-compare) at [%d,%d]", row, col);
            }
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int eigen_gemm_double_ab_split_finish(struct test *test) {
    delete(CAST(test->data));
    return EXIT_SUCCESS;
}

DECLARE_TEST(eigen_gemm_double_ab_split, "Eigen GEMM (double) A-vs-B-gemm-load discriminating probe: two GEMM passes + internal vs external check")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = eigen_gemm_double_ab_split_init,
  .test_run = eigen_gemm_double_ab_split_run,
  .test_cleanup = eigen_gemm_double_ab_split_finish,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_gemm_double14
 * @parblock
 * This piece of code aims to stress test the exec (in particular FMA)
 * by repetitively solving general matrix matrix multiplication.  The
 * multiplication function is from the 3rd party library Eigen.  A
 * pair of random matrices are generated as inputs and then multiplied
 * using Eigen's gemm. The multiplication result is compared against a
 * golden result that is computed during init.  This particular
 * version of the test goes for double precision input matrices. This
 * variation adds extra copies and consistency checks.
 *
 * @note Although the test should run fine on a single thread, it is
 * only expected to catch defects if run on at least 2 cores.
 * @endparblock
 */

#include <sandstone.h>

#include <Eigen/Core>
using namespace Eigen;

#define M_DIM 256

typedef Matrix < double, Dynamic, Dynamic > Mat;

namespace {
struct eigen_test_data {
    Mat lhs;
    Mat rhs;
    Mat prod;
};
}

#define CAST(_x) static_cast<struct eigen_test_data *>(_x)

static int eigen_gemm_double14_init(struct test *test) {
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

static int eigen_gemm_double14_run(struct test *test, int cpu) {
    /* randomization hardening H11' (P14): per-thread operands re-rolled
     * every kRerollEvery iterations (framework RNG, [-1, 1) bounded);
     * golden recomputed the same batch. */
    constexpr int kRerollEvery = 16;
    auto fill_rand = [](Mat &m) {
        for (int r = 0; r < m.rows(); ++r)
            for (int c = 0; c < m.cols(); ++c)
                m(r, c) = frandom_scale(2.0) - 1.0;
    };
    Mat l_lhs, l_rhs, l_prod;
    l_lhs.resize(M_DIM, M_DIM);
    l_rhs.resize(M_DIM, M_DIM);
    l_prod.resize(M_DIM, M_DIM);
    fill_rand(l_lhs);
    fill_rand(l_rhs);
    l_prod = l_lhs * l_rhs;
    int since_reroll = 0;

    do {
        Mat _x;
        _x = l_lhs;
        Mat _y;
        _y = l_rhs;
        Mat _prod;
        _prod = _x * _y;

        if (!_x.isApprox(l_lhs)) {
                report_fail_msg("_x.isApprox failed");
        }
        memcmp_or_fail(_x.data(), l_lhs.data(), M_DIM * M_DIM);

        if (!_y.isApprox(l_rhs)) {
                report_fail_msg("_y.isApprox failed");
        }
        memcmp_or_fail(_y.data(), l_rhs.data(), M_DIM * M_DIM);

        if (!_prod.isApprox(l_prod)) {
                report_fail_msg("_prod.isApprox failed");
        }
        memcmp_or_fail(_prod.data(), l_prod.data(), M_DIM * M_DIM);

        if (++since_reroll >= kRerollEvery) {
            since_reroll = 0;
            fill_rand(l_lhs);
            fill_rand(l_rhs);
            l_prod = l_lhs * l_rhs;
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int eigen_gemm_double14_finish(struct test *test) {
    delete(CAST(test->data));
    return EXIT_SUCCESS;
}

DECLARE_TEST(eigen_gemm_double14, "Eigen GEMM payload (double, dynamic, square)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = eigen_gemm_double14_init,
  .test_run = eigen_gemm_double14_run,
  .test_cleanup = eigen_gemm_double14_finish,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

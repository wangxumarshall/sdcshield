/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 */

#ifndef SANDSTONE_EIGEN_COMMON_H
#define SANDSTONE_EIGEN_COMMON_H

/* Disable deprecation warnings in case the library used is >= 3.5.x. Once
 * majority of the distros move to 3.5 or higher,
 * https://github.com/opendcdiag/opendcdiag/pull/941 should be adopted. */
#define EIGEN_NO_DEPRECATED_WARNING

#include <sandstone.h>

#include <boost/type_traits/is_complex.hpp>
#include <Eigen/Eigenvalues>

namespace {
/* EigenSVDTest<SVD, Dim>: golden-value SVD test template.
 *
 * Dim > 0        — compile-time dimension (all historical tests).
 * Dim == Dynamic — runtime dimension mode: the test's init sets d->dim
 *                  (e.g. from a knob or from available memory) and all
 *                  matrix allocations/comparisons follow it. Used by
 *                  eigen_svd_cdouble_sve, which sizes the matrix from
 *                  per-worker RAM at init time (min 300). */
template <typename SVD, int Dim> struct EigenSVDTest
{
    using Mat = typename SVD::MatrixType;
    static constexpr bool kRuntimeDim = (Dim == Eigen::Dynamic);
    struct eigen_test_data {
        int dim = kRuntimeDim ? 0 : Dim;   /* matrix dimension */
        Mat orig_matrix;
        Mat u_matrix;
        Mat v_matrix;
    };

    [[gnu::noinline]] static void calculate_once(const Mat &orig_matrix, Mat &u, Mat &v)
    {
        SVD fullSvd(orig_matrix, Eigen::ComputeFullU | Eigen::ComputeFullV);
        u = fullSvd.matrixU();
        v = fullSvd.matrixV();
    }

    template <typename FP> static inline std::enable_if_t<boost::is_complex<FP>::value>
    compare_or_fail(const FP *actual, const FP *expected, int dim, const char *name)
    {
        memcmp_or_fail(reinterpret_cast<const typename FP::value_type *>(actual),
                       reinterpret_cast<const typename FP::value_type *>(expected),
                       2 * dim * dim, name);
    }

    template <typename FP> static inline std::enable_if_t<!boost::is_complex<FP>::value>
    compare_or_fail(const FP *actual, const FP *expected, int dim, const char *name)
    {
        memcmp_or_fail(actual, expected, dim * dim, name);
    }

    static int init(struct test *test)
    {
        auto d = new eigen_test_data;
        if constexpr (!kRuntimeDim) {
            d->orig_matrix = Mat::Random(Dim, Dim);
            calculate_once(d->orig_matrix, d->u_matrix, d->v_matrix);
        }
        /* kRuntimeDim: the enclosing test's own init wrapper has already
         * set d->dim (and typically allocates below); the template's init
         * only runs if the wrapper calls it, which it does NOT — see
         * svd_cdouble_sve.cpp. This branch exists so misuse fails loudly
         * instead of allocating a 0x0 matrix. */
        test->data = d;
        return EXIT_SUCCESS;
    }

    static int cleanup(struct test *test)
    {
        delete static_cast<eigen_test_data *>(test->data);
        return EXIT_SUCCESS;
    }

    static int run(struct test *test, int)
    {
        auto d = static_cast<eigen_test_data *>(test->data);
        do {
            Mat u, v;
            calculate_once(d->orig_matrix, u, v);

            compare_or_fail<typename Mat::Scalar>(u.data(), d->u_matrix.data(), d->dim, "Matrix U");
            compare_or_fail<typename Mat::Scalar>(v.data(), d->v_matrix.data(), d->dim, "Matrix V");
        } while (test_time_condition(test));
        return EXIT_SUCCESS;
    }
};

} // unnamed namespace

#endif // SANDSTONE_EIGEN_COMMON_H

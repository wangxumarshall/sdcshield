/**
 * @file
 *
 * @copyright
 * Copyright 2026 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_svd_cdouble_sve
 * @parblock
 * This is the ARM64 SVE-backed counterpart of @ref eigen_svd_cdouble.
 *
 * It runs exactly the same divide-and-conquer bi-diagonalization SVD
 * (Eigen::BDCSVD) on a complex double input matrix, but the Eigen code is
 * instantiated against the SVE packet backend by compiling this translation
 * unit with `-DEIGEN_ARM64_USE_SVE` and a `-march=...+sve` target, and the
 * Eigen namespace is renamed (`-DEigen=EigenSVE`) so it links cleanly
 * beside the NEON baseline build.
 *
 * Because SVE instructions are baked into the object, the test probes for
 * SVE availability at init time using plain C (no SVE instructions) and
 * reports a clean CpuNotSupported skip when the running CPU has no SVE
 * (e.g. Kunpeng 920). On SVE-capable hardware (e.g. Kunpeng 930) the test
 * runs and stresses the SVE FMA/vector units for SDC detection.
 *
 * @note This test requires at least 2 threads to run.
 * @endparblock
 */

#include "sandstone_eigen_common.h"

#if defined(__aarch64__)

#include <sys/auxv.h>
#include <asm/hwcap.h>

using namespace Eigen;        /* renamed to EigenSVE via -DEigen=EigenSVE */
typedef Matrix<std::complex<double>, Dynamic, Dynamic> Mat;
typedef Eigen::BDCSVD<Mat> SVD;

// Matches the NEON counterpart eigen_svd_cdouble (tests/cpu/eigen_svd/
// svd_cdouble.cpp, M_DIM 300). A larger dimension must stay within the
// framework's per-test timeout floor (test_timeout(): duration*5+30s,
// 300s minimum — framework/sandstone.cpp); at M_DIM 2100 one BDCSVD
// iteration on complex<double> did not complete even after 30 min
// single-threaded / 60 min all-core (measured 2026-09-18, 127-core
// cortex x3b), so every default run timed out and aborted the suite.
#define M_DIM 300

using eigen_svd_cdouble_sve_test = EigenSVDTest<SVD, M_DIM>;

/*
 * Probe SVE availability using only the vDSO / getauxval, which executes no
 * SVE instructions itself. We must not let any SVE-instrumented Eigen code
 * run on a CPU without SVE (it would SIGILL), so this is done before
 * delegating to the Eigen-backed init.
 */
static int sve_probe_and_init(struct test *test)
{
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "ARM64 SVE not available on this CPU; "
                 "eigen_svd_cdouble_sve requires SVE (e.g. Kunpeng 930)");
        return EXIT_SKIP;
    }
#if EIGEN_VERSION_AT_LEAST(5, 0, 0)
    /* Eigen 5.0's SVE packet backend (arch/SVE/PacketMath.h) only
     * specializes int32_t and float — there is no packet_traits<double>
     * (nor complex<double>), so every Matrix<double> path in this SVE
     * translation unit silently falls back to scalar loops. Measured on
     * 2026-09-18 (127-core cortex x3b): a 300x300 double BDCSVD takes
     * 29 ms on the NEON backend but exceeds 10 minutes on the "SVE"
     * backend — the Jacobi rotations (apply_rotation_in_the_plane) that
     * dominate the base case all hit the non-vectorized selector. The
     * test therefore cannot stress SVE hardware at all and blows past
     * the framework's 300 s test_timeout() floor at any matrix size
     * (M_DIM 300 and 2100 both measured as timed out). Report an honest
     * placeholder skip until the vendored Eigen gains SVE double
     * packets. */
    if (EigenSVE::internal::packet_traits<double>::size == 1) {
        log_skip(TestResourceIssueSkipCategory,
                 "to be implemented (placeholder): Eigen 5.0 SVE packet "
                 "backend has no double/complex<double> support (scalar "
                 "fallback, ~20000x slower than NEON — one 300x300 "
                 "BDCSVD iteration exceeds the 300 s test timeout); "
                 "pending SVE double packet support in vendored Eigen");
        return EXIT_SKIP;
    }
#endif
    return eigen_svd_cdouble_sve_test::init(test);
}

DECLARE_TEST(eigen_svd_cdouble_sve, "Eigen SVD complex<double> (ARM64 SVE vector backend); counterpart of eigen_svd_cdouble")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = sve_probe_and_init,
  .test_run = eigen_svd_cdouble_sve_test::run,
  .test_cleanup = eigen_svd_cdouble_sve_test::cleanup,
  .minimum_cpu = 0,
  .fracture_loop_count = 5,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif // __aarch64__

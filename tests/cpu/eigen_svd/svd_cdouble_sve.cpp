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
 * It runs the same divide-and-conquer bi-diagonalization SVD
 * (Eigen::BDCSVD) on a complex double input matrix, but the Eigen code is
 * instantiated against the SVE packet backend by compiling this translation
 * unit with `-DEIGEN_ARM64_USE_SVE` and a `-march=...+sve` target, and the
 * Eigen namespace is renamed (`-DEigen=EigenSVE`) so it links cleanly
 * beside the NEON baseline build.
 *
 * The matrix dimension is chosen at init time from the RAM available per
 * worker (see size_matrix() below): the largest N whose peak RSS fits the
 * per-worker memory share, capped at the largest N that finishes the whole
 * test (golden BDCSVD in init + recomputation in run) inside a 10-minute
 * budget, and floored at 300. An explicit override is available via
 * `-O eigen_svd_cdouble_sve.mdim=N` (300..6000).
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
#include <unistd.h>
#include <cmath>

using namespace Eigen;        /* renamed to EigenSVE via -DEigen=EigenSVE */
typedef Matrix<std::complex<double>, Dynamic, Dynamic> Mat;
typedef Eigen::BDCSVD<Mat> SVD;

/* Runtime dimension mode: the template allocates and compares against
 * d->dim, which size_matrix() derives from available memory. */
using eigen_svd_cdouble_sve_test = EigenSVDTest<SVD, Eigen::Dynamic>;

/* Dimension bounds, all measured on cortex x3b (VL=128 pinned, same
 * compile flags as this TU, 2026-09-19 — see the plan file
 * docs/superpowers/plans/2026-09-19-svd-cdouble-sve-mdim-5800.md):
 *
 *   MDIM_MIN  = 300: the NEON counterpart's parity point; the floor.
 *   MDIM_TIME = 4400: the largest N whose FULL test time fits a 10-minute
 *                     budget. The framework counts the golden BDCSVD in
 *                     test_init PLUS the recomputation in test_run — two
 *                     complete decompositions. Measured single-BDCSVD
 *                     times: 2400→40s, 3600→134s, 4400→243s, 4800→322s,
 *                     5600→511s, 5800→563s, 6000→622s (~N³ scaling);
 *                     4400×2 = 487 s total (19% margin; 4600×2 = 583 s
 *                     leaves only 3% for load variance, 5800×2 = 1124 s
 *                     measured over budget).
 *   MDIM_MAX  = 6000: knob ceiling (6000 alone already busts the budget;
 *                     it exists for opt-in experiments on faster hosts).
 *
 * Memory model: measured peak RSS is a stable ~9.7-10x the input matrix
 * volume (BDCSVD's real-typed m_naiveU/m_naiveV/m_computed plus U/V
 * copies and workspace — N=2400→0.86GB, 4400→2.82GB, 6000→5.22GB).
 * The sizing uses 12x (20% headroom) of 16*N² bytes per worker.
 */
#define MDIM_MIN 300
#define MDIM_TIME_CAP 4400
#define MDIM_MAX 6000
#define RSS_MATRIX_RATIO 12.0

/*
 * Choose the matrix dimension:
 *   mdim knob (if set)  -> clamped to [MDIM_MIN, MDIM_MAX]
 *   else                -> min(memory-limited N, MDIM_TIME_CAP), floored
 *                          at MDIM_MIN; never below the floor even on
 *                          tiny-memory hosts (300 needs only ~0.2 GB).
 */
static int size_matrix(struct test *test, int *out_nthreads, double *out_gb_per_worker)
{
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    int nthreads = thread_count();
    if (out_nthreads) *out_nthreads = nthreads;
    double ram_per_worker = (pages > 0 && page_size > 0 && nthreads > 0)
                                ? (double)pages * (double)page_size / nthreads
                                : 0.0;
    if (out_gb_per_worker) *out_gb_per_worker = ram_per_worker / (1 << 30);

    int64_t knob = get_testspecific_knob_value_int(test, "mdim", 0);
    if (knob > 0)
        return (int)(knob < MDIM_MIN ? MDIM_MIN : (knob > MDIM_MAX ? MDIM_MAX : knob));

    /* memory-limited N: 12 * 16 * N^2 <= per-worker RAM  =>
     * N <= sqrt(ram_bytes_per_worker / 192) */
    if (ram_per_worker <= 0.0)
        return MDIM_MIN;   /* introspection unavailable: conservative floor */
    int n_mem = (int)std::sqrt(ram_per_worker / (RSS_MATRIX_RATIO * 16.0));

    int n = n_mem < MDIM_TIME_CAP ? n_mem : MDIM_TIME_CAP;
    return n < MDIM_MIN ? MDIM_MIN : n;
}

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
    int nthreads = 1;
    double gb_per_worker = 0.0;
    int dim = size_matrix(test, &nthreads, &gb_per_worker);

    log_info("M_DIM %d (memory %.2f GB/worker / %d threads; cap %d, floor %d; "
             "override with -O eigen_svd_cdouble_sve.mdim=N)",
             dim, gb_per_worker, nthreads, MDIM_TIME_CAP, MDIM_MIN);

    /* Allocate the test data and run the golden BDCSVD here (the runtime-
     * dimension template does not do it in its own init). */
    auto d = new eigen_svd_cdouble_sve_test::eigen_test_data;
    d->dim = dim;
    d->orig_matrix = Mat::Random(dim, dim);
    eigen_svd_cdouble_sve_test::calculate_once(d->orig_matrix, d->u_matrix, d->v_matrix);
    test->data = d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(eigen_svd_cdouble_sve, "Eigen SVD complex<double> (ARM64 SVE vector backend, dimension sized from available memory); counterpart of eigen_svd_cdouble")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = sve_probe_and_init,
  .test_run = eigen_svd_cdouble_sve_test::run,
  .test_cleanup = eigen_svd_cdouble_sve_test::cleanup,
  .minimum_cpu = 0,
  /* One test_run BDCSVD iteration takes up to ~243 s (measured at the
   * memory/time cap of 4400; smaller auto-sized dimensions are proportion-
   * ally faster). desired_duration of 240 s sits just below the cap's
   * single-iteration time, so the do-while time condition is already
   * exhausted when the first iteration finishes: exactly one iteration
   * runs at every auto-sized dimension, and the derived timeout
   * (test_timeout(): 5*240 s + 30 s = 1230 s) covers init+run at the cap
   * with headroom. (desired_duration = -1 would fall back to the 1 s
   * default and the resulting 300 s timeout floor would kill the run.)
   * A knob-forced 6000 will exceed this duration and run a second
   * iteration — acceptable for an explicit opt-in experiment. */
  .desired_duration = 240000,
  .fracture_loop_count = 5,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif // __aarch64__

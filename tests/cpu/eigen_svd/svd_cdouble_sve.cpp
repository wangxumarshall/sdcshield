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
#include <unistd.h>

using namespace Eigen;        /* renamed to EigenSVE via -DEigen=EigenSVE */
typedef Matrix<std::complex<double>, Dynamic, Dynamic> Mat;
typedef Eigen::BDCSVD<Mat> SVD;

// The largest dimension whose FULL test time — the framework counts the
// golden BDCSVD in test_init PLUS the recomputation in test_run, i.e. TWO
// complete decompositions — fits a 10-minute budget, measured 2026-09-19
// on cortex x3b (127-core, VL=128 pinned, same compile flags as this TU):
//   single BDCSVD:  2400→40s   3600→134s   4400→243s   4600→292s
//                   4800→322s  5600→511s   5800→563s   6000→622s
//   4400 x 2 = ~487 s total (19% margin under 600 s; 4600 x 2 = 583 s
//   leaves only 3% for load variance, 5800 x 2 = 1126 s measured over).
// The two-decomposition cost is inherent to the golden-value test design
// (init computes the reference U/V, run recomputes and bit-compares).
// Peak RSS at 4400 is ~2.8 GB per process (OOM guard below uses 4 GB).
// History: 300 = NEON counterpart parity; 2100 = scalar-fallback era that
// could not finish in 30 min. Plan: 2026-09-19-svd-cdouble-sve-mdim-5800.md
// (the 5800 in the name is the first candidate before the init+run
// doubling was measured; final pick documented here).
#define M_DIM 4400

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
    /* OOM guard: M_DIM 4400 peaks at ~2.8 GB RSS per process (measured).
     * Every worker thread allocates its own matrix + golden U/V copies,
     * so an N-thread run needs ~3 GB * N; on this 61 GB host the default
     * 32-thread plan would need ~96 GB — the OOM killer would fire mid-
     * test and abort the suite. Require >= 4 GB of physical RAM per
     * worker (40% headroom over the measured 2.8 GB) and skip cleanly
     * when the per-worker share is below that. Single-threaded (-n 1)
     * and small -n runs pass the guard and execute the real workload. */
    long pages = sysconf(_SC_PHYS_PAGES);
    long page_size = sysconf(_SC_PAGESIZE);
    int nthreads = thread_count();
    if (pages > 0 && page_size > 0 && nthreads > 0) {
        double gb_per_worker = (double)pages * (double)page_size / (1 << 30) / nthreads;
        if (gb_per_worker < 4.0) {
            log_skip(TestResourceIssueSkipCategory,
                     "eigen_svd_cdouble_sve M_DIM 4400 needs ~2.8 GB per "
                     "worker (measured peak RSS) but only %.2f GB of "
                     "physical RAM is available per worker (%ld GB total "
                     "/ %d threads); run with -n 1 (or -n <= total/4GB) "
                     "for the real SVE SVD workload",
                     gb_per_worker, (long)((double)pages * page_size / (1 << 30)), nthreads);
            return EXIT_SKIP;
        }
    }
    return eigen_svd_cdouble_sve_test::init(test);
}

DECLARE_TEST(eigen_svd_cdouble_sve, "Eigen SVD complex<double> (ARM64 SVE vector backend); counterpart of eigen_svd_cdouble")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = sve_probe_and_init,
  .test_run = eigen_svd_cdouble_sve_test::run,
  .test_cleanup = eigen_svd_cdouble_sve_test::cleanup,
  .minimum_cpu = 0,
  /* One test_run BDCSVD iteration takes ~243 s at M_DIM 4400 (measured;
   * test_init's golden pass costs the same again and is counted in the
   * test's overall runtime — total ~487 s, inside the 10-minute budget).
   * desired_duration of 240 s sits just below the single-iteration time,
   * so the do-while time condition is already exhausted when the first
   * iteration finishes: exactly one iteration runs, and the derived
   * timeout (test_timeout(): 5*240 s + 30 s = 1230 s) covers init+run
   * with headroom. (desired_duration = -1 would fall back to the 1 s
   * default and the resulting 300 s timeout floor would kill the run.) */
  .desired_duration = 240000,
  .fracture_loop_count = 5,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#endif // __aarch64__

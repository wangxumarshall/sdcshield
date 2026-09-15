/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sleef_sve
 * @parblock
 * Stress test for SVE data paths via SLEEF's SVE transcendental functions
 * (sin/cos/exp/log, double, u10 accuracy, sve kernels). Long polynomial
 * FMA dependency chains; results only feed a byte-exact golden compare
 * (data-flow payload: faults surface as wrong values, not crashes —
 * From Gates to SDCs DATE'25). Pure functions => deterministic golden
 * values computed once in init with the same kernels. Covers the
 * newest-instruction-generation risk area (PinDrop Obs12: instructions
 * fail most in their first arch gen) — SVE is the newest-generation
 * vector datapath on ARM64. Compiled in the dedicated tests_sve static
 * library (-march=armv8.2-a+sve -msve-vector-bits=128) so the SVE
 * instructions never leak into the NEON baseline build.
 *
 * Runtime gate: silicon without SVE (e.g. Kunpeng 920) skips cleanly
 * (CpuNotSupported / EXIT_SKIP). Real execution requires SVE hardware
 * (Kunpeng 930 / Neoverse) — documented limitation of this development
 * host; on SVE silicon the test runs the full payload.
 * @endparblock
 */

#include <sandstone.h>

#include <sleef.h>

#include <arm_sve.h>
#include <sys/auxv.h>
#include <asm/hwcap.h>
#include <string.h>
#include <stdlib.h>

/* Fixed 128-bit SVE vector length (-msve-vector-bits=128, matching the
 * tests_sve build block): svfloat64_t is exactly 2 lanes, so ELEMS (even)
 * processes in ELEMS/2 svld1_f64/svst1_f64 pairs with a fully-active
 * predicate — svptrue_b64() is correct because every load/store touches
 * exactly the 2 lanes the fixed VL provides. */
#define ELEMS 1024   /* elements per family: 512 svfloat64_t vectors (2 lanes each) */

namespace {
struct sleef_test_data {
    /* per-domain inputs (each family keeps its own array so the run loop
     * can replay exactly the inputs the golden values were computed from) */
    double *xd_trig, *xd_exp, *xd_log;
    /* golden outputs (read-only after init) */
    double *gd_sin, *gd_cos, *gd_exp, *gd_log;
};
}

#define CAST(_x) static_cast<struct sleef_test_data *>(_x)

/* Scratch output buffer is per-thread: the framework runs one worker
 * thread per core and they all share test->data, so anything written during
 * test_run must be thread-local (od is the kernel destination, written
 * every iteration). Allocated lazily on first use and intentionally not
 * freed in cleanup, exactly like the sleef_neon / openblas_* scratch
 * buffers: the worker threads are gone (or never existed) when the main
 * thread runs test_cleanup, and the forked child exits right after, so
 * the OS reclaims the storage. calloc zeroes it once so a short first
 * iteration can never compare garbage. */
static thread_local double *od = nullptr;

/* Uniform double in [0,1) from the framework RNG (top 53 bits; libc rand is
 * trapped by the framework). */
static double uniform01(void)
{
    return (double)(random64() >> 11) / 9007199254740992.0;
}

/* Domain-boundary analysis — identical construction to sleef_neon (see
 * there for the full write-up):
 *  - trig : u in [0,1) tops out at 1-2^-53, so u*2-1 (exact in binary)
 *           lies in [-1, 1-2^-52]; times pi the magnitude stays strictly
 *           inside [-pi, pi] after the roundings. sin/cos of that is
 *           guaranteed finite and well inside [-1, 1].
 *  - exp  : same construction over [-20, 20-2^-48]; exp of that is in
 *           [~2e-9, ~4.85e8], forty orders from overflow.
 *  - log  : u in [0,1) plus 1e-300 keeps the argument strictly positive
 *           (u==0 gives 1e-300, a normal double), scaled by 1e10: log
 *           arguments span [1e-290, 1e10), all finite, all inside SLEEF's
 *           u10 accuracy domain (positive reals).
 * Every golden value is additionally checked for finiteness in init: a
 * NaN/Inf would make the byte-exact comparison meaningless (NaN != NaN). */

static int sleef_sve_init(struct test *test)
{
    /* Runtime gate. NOTE: device_has_feature(cpu_feature_sve) is useless
     * here — it is defined as (device_compiler_features & f) || (device_features
     * & f) and this TU is compiled with +sve, so the compiler half would make
     * it always-true on any CPU. Probe HWCAP_SVE directly via getauxval,
     * exactly like eigen_svd_cdouble_sve: the vDSO call executes no SVE
     * instructions, so it is safe to run before any SVE code path. The
     * framework-level compiler_minimum_device gate (the "test compiled with
     * sve" skip) already catches non-SVE hosts before init; this explicit
     * probe keeps a sleef_sve-specific skip reason even if that framework
     * gate ordering ever changes. */
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "ARM64 SVE not available on this CPU; "
                 "sleef_sve requires SVE (e.g. Kunpeng 930)");
        return EXIT_SKIP;
    }

    auto d = new(sleef_test_data);
    test->data = d;
    d->xd_trig = (double *)malloc(ELEMS * sizeof(double));
    d->xd_exp  = (double *)malloc(ELEMS * sizeof(double));
    d->xd_log  = (double *)malloc(ELEMS * sizeof(double));
    d->gd_sin = (double *)malloc(ELEMS * sizeof(double));
    d->gd_cos = (double *)malloc(ELEMS * sizeof(double));
    d->gd_exp = (double *)malloc(ELEMS * sizeof(double));
    d->gd_log = (double *)malloc(ELEMS * sizeof(double));
    if (!d->xd_trig || !d->xd_exp || !d->xd_log ||
        !d->gd_sin || !d->gd_cos || !d->gd_exp || !d->gd_log) {
        report_fail_msg("OOM in sleef_sve init");
    }

    /* inputs, per domain (same mappings and boundary analysis as sleef_neon) */
    for (int i = 0; i < ELEMS; ++i) {
        double u = uniform01();
        d->xd_trig[i] = (u * 2.0 - 1.0) * 3.141592653589793;   /* [-pi, pi] */
    }
    for (int i = 0; i < ELEMS; ++i) {
        double u = uniform01();
        d->xd_exp[i] = (u * 2.0 - 1.0) * 20.0;                 /* [-20, 20) */
    }
    for (int i = 0; i < ELEMS; ++i) {
        double u = uniform01();
        d->xd_log[i] = (u + 1.0e-300) * 1.0e10;                /* positive */
    }

    /* golden outputs from the very same SVE kernels, once; any non-finite
     * result would poison the byte-exact comparison (NaN != NaN), so fail
     * loudly at the source instead (same tripwire as sleef_neon) */
    const svbool_t pg = svptrue_b64();
    for (int i = 0; i < ELEMS; i += 2) {
        svfloat64_t x = svld1_f64(pg, &d->xd_trig[i]);
        svst1_f64(pg, &d->gd_sin[i], Sleef_sindx_u10sve(x));
        svst1_f64(pg, &d->gd_cos[i], Sleef_cosdx_u10sve(x));
        x = svld1_f64(pg, &d->xd_exp[i]);
        svst1_f64(pg, &d->gd_exp[i], Sleef_expdx_u10sve(x));
        x = svld1_f64(pg, &d->xd_log[i]);
        svst1_f64(pg, &d->gd_log[i], Sleef_logdx_u10sve(x));
    }
    for (int i = 0; i < ELEMS; ++i) {
        double gs = d->gd_sin[i], gc = d->gd_cos[i];
        double ge = d->gd_exp[i], gl = d->gd_log[i];
        if (gs != gs || gs > 1.0 || gs < -1.0 || gc != gc || gc > 1.0 || gc < -1.0 ||
            ge != ge || ge > 1.0e300 || ge < 0.0 ||
            gl != gl || gl > 1.0e300 || gl < -1.0e300) {
            report_fail_msg("golden value not finite/in-range at element %d "
                            "(domain mapping needs tighter scaling)", i);
        }
    }
    return EXIT_SUCCESS;
}

static int sleef_sve_run(struct test *test, int cpu)
{
    auto d = CAST(test->data);
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch output buffer */
        if (__builtin_expect(!od, 0)) {
            free(od);
            od = (double *)calloc(1, ELEMS * sizeof(double));
            if (!od)
                report_fail_msg("OOM allocating thread scratch (%zu bytes)",
                                (size_t)ELEMS * sizeof(double));
        }

        /* compute-out: each family replays its own domain inputs through
         * the same SVE kernels and lands in the thread-private scratch */
        const svbool_t pg = svptrue_b64();
        for (int i = 0; i < ELEMS; i += 2) {
            svfloat64_t x = svld1_f64(pg, &d->xd_trig[i]);
            svst1_f64(pg, &od[i], Sleef_sindx_u10sve(x));
        }
        memcmp_or_fail(od, d->gd_sin, ELEMS, "sin double sve");

        for (int i = 0; i < ELEMS; i += 2) {
            svfloat64_t x = svld1_f64(pg, &d->xd_trig[i]);
            svst1_f64(pg, &od[i], Sleef_cosdx_u10sve(x));
        }
        memcmp_or_fail(od, d->gd_cos, ELEMS, "cos double sve");

        for (int i = 0; i < ELEMS; i += 2) {
            svfloat64_t x = svld1_f64(pg, &d->xd_exp[i]);
            svst1_f64(pg, &od[i], Sleef_expdx_u10sve(x));
        }
        memcmp_or_fail(od, d->gd_exp, ELEMS, "exp double sve");

        for (int i = 0; i < ELEMS; i += 2) {
            svfloat64_t x = svld1_f64(pg, &d->xd_log[i]);
            svst1_f64(pg, &od[i], Sleef_logdx_u10sve(x));
        }
        memcmp_or_fail(od, d->gd_log, ELEMS, "log double sve");
    }
    return EXIT_SUCCESS;
}

static int sleef_sve_cleanup(struct test *test)
{
    auto d = CAST(test->data);
    free(d->xd_trig); free(d->xd_exp); free(d->xd_log);
    free(d->gd_sin); free(d->gd_cos); free(d->gd_exp); free(d->gd_log);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(sleef_sve, "SLEEF vectorized transcendentals (SVE polynomial FMA chains, sin/cos/exp/log double)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = sleef_sve_init,
  .test_run = sleef_sve_run,
  .test_cleanup = sleef_sve_cleanup,
  .minimum_cpu = 0,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

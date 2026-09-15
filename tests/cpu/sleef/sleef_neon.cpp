/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sleef_neon
 * @parblock
 * Stress test for NEON data paths via SLEEF's vectorized transcendental
 * functions (sin/cos/exp/log, double & float, u10 accuracy, advsimd
 * kernels). Long polynomial FMA dependency chains; results only feed a
 * byte-exact golden compare (data-flow payload: faults surface as wrong
 * values, not crashes — From Gates to SDCs DATE'25). Pure functions =>
 * deterministic golden values computed once in init with the same kernels.
 * Covers the newest-instruction-generation risk area (PinDrop Obs12:
 * instructions fail most in their first arch gen) — the advsimd u10
 * codepaths are SLEEF's newest NEON implementations.
 * @endparblock
 */

#include <sandstone.h>

#include <sleef.h>

#include <arm_neon.h>
#include <string.h>
#include <stdlib.h>

#define ELEMS 1024   /* elements per family: 512 float64x2 vectors (double half) + 256 float32x4 vectors (float half) */

namespace {
struct sleef_test_data {
    /* per-domain inputs (each family keeps its own array so the run loop
     * can replay exactly the inputs the golden values were computed from) */
    double *xd_trig, *xd_exp, *xd_log;
    float  *xf_trig, *xf_exp, *xf_log;
    /* golden outputs (read-only after init) */
    double *gd_sin, *gd_cos, *gd_exp, *gd_log;
    float  *gf_sin, *gf_cos, *gf_exp, *gf_log;
};
}

#define CAST(_x) static_cast<struct sleef_test_data *>(_x)

/* Scratch output buffers are per-thread: the framework runs one worker
 * thread per core and they all share test->data, so anything written during
 * test_run must be thread-local (od/of are the kernel destinations, written
 * every iteration). Allocated lazily on first use and intentionally not
 * freed in cleanup, exactly like the openblas_* scratch buffers: the worker
 * threads are gone (or never existed) when the main thread runs
 * test_cleanup, and the forked child exits right after, so the OS reclaims
 * the storage. calloc zeroes them once so a short first iteration can never
 * compare garbage. */
static thread_local double *od = nullptr;
static thread_local float  *of = nullptr;

/* Uniform double in [0,1) from the framework RNG (top 53 bits; libc rand is
 * trapped by the framework). */
static double uniform01(void)
{
    return (double)(random64() >> 11) / 9007199254740992.0;
}

/* Domain-boundary analysis for the mappings below (the naive u*span+lo
 * write-up can round to the boundary itself):
 *  - trig : u in [0,1) tops out at 1-2^-53, so u*2-1 (exact in binary)
 *           lies in [-1, 1-2^-52]; times pi the magnitude stays
 *           <= pi*(1-2^-52)*(1+2eps) — strictly inside [-pi, pi] after the
 *           two roundings. sin/cos of that is also guaranteed finite and
 *           well inside [-1, 1].
 *  - exp  : same construction over [-20, 20-2^-48]; exp of that is in
 *           [~2e-9, ~4.85e8], forty orders from overflow and far above
 *           subnormal double/float (float min normal ~1.2e-38).
 *  - log  : u in [0,1) plus 1e-300 keeps the argument strictly positive
 *           (u==0 gives 1e-300, a normal double; the float variant uses
 *           1e-30 so the offset survives the float narrowing), scaled by
 *           1e10: log arguments span [1e-290, 1e10) for the double variant
 *           ([~1e-20, ~1e10) once narrowed to float), all finite, all
 *           inside SLEEF's u10 accuracy domain (positive reals).
 * Every golden value is additionally checked for finiteness in init: a
 * NaN/Inf would make the byte-exact comparison meaningless (NaN != NaN). */

static int sleef_neon_init(struct test *test)
{
    auto d = new(sleef_test_data);
    test->data = d;
    d->xd_trig = (double *)malloc(ELEMS * sizeof(double));
    d->xd_exp  = (double *)malloc(ELEMS * sizeof(double));
    d->xd_log  = (double *)malloc(ELEMS * sizeof(double));
    d->xf_trig = (float  *)malloc(ELEMS * sizeof(float));
    d->xf_exp  = (float  *)malloc(ELEMS * sizeof(float));
    d->xf_log  = (float  *)malloc(ELEMS * sizeof(float));
    d->gd_sin = (double *)malloc(ELEMS * sizeof(double));
    d->gd_cos = (double *)malloc(ELEMS * sizeof(double));
    d->gd_exp = (double *)malloc(ELEMS * sizeof(double));
    d->gd_log = (double *)malloc(ELEMS * sizeof(double));
    d->gf_sin = (float  *)malloc(ELEMS * sizeof(float));
    d->gf_cos = (float  *)malloc(ELEMS * sizeof(float));
    d->gf_exp = (float  *)malloc(ELEMS * sizeof(float));
    d->gf_log = (float  *)malloc(ELEMS * sizeof(float));
    if (!d->xd_trig || !d->xd_exp || !d->xd_log ||
        !d->xf_trig || !d->xf_exp || !d->xf_log ||
        !d->gd_sin || !d->gd_cos || !d->gd_exp || !d->gd_log ||
        !d->gf_sin || !d->gf_cos || !d->gf_exp || !d->gf_log) {
        report_fail_msg("OOM in sleef_neon init");
    }

    /* double inputs, per domain */
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

    /* float inputs, per domain. CAUTION: the [0,1) double does NOT stay
     * below 1 after narrowing — doubles in (1-2^-25, 1) round UP to exactly
     * 1.0f — so xf_trig/xf_exp can reach exactly +-pi_f / +-20_f (the domain
     * edge). Harmless: the u10 kernels carry their accuracy guarantee over
     * the whole input domain, determinism never depends on accuracy (the
     * golden values come from the very same kernel), and the init finiteness
     * tripwire below backstops the rest. */
    for (int i = 0; i < ELEMS; ++i) {
        float u = (float)uniform01();
        d->xf_trig[i] = (u * 2.0f - 1.0f) * 3.14159265f;       /* [-pi, pi] */
    }
    for (int i = 0; i < ELEMS; ++i) {
        float u = (float)uniform01();
        d->xf_exp[i] = (u * 2.0f - 1.0f) * 20.0f;              /* [-20, 20) */
    }
    for (int i = 0; i < ELEMS; ++i) {
        float u = (float)uniform01();
        d->xf_log[i] = (u + 1.0e-30f) * 1.0e10f;               /* positive */
    }

    /* golden outputs from the very same kernels, once; any non-finite
     * result would poison the byte-exact comparison (NaN != NaN), so fail
     * loudly at the source instead (same tripwire as openblas_dgemm) */
    for (int i = 0; i < ELEMS; i += 2) {
        float64x2_t x = vld1q_f64(&d->xd_trig[i]);
        vst1q_f64(&d->gd_sin[i], Sleef_sind2_u10advsimd(x));
        vst1q_f64(&d->gd_cos[i], Sleef_cosd2_u10advsimd(x));
        x = vld1q_f64(&d->xd_exp[i]);
        vst1q_f64(&d->gd_exp[i], Sleef_expd2_u10advsimd(x));
        x = vld1q_f64(&d->xd_log[i]);
        vst1q_f64(&d->gd_log[i], Sleef_logd2_u10advsimd(x));
    }
    for (int i = 0; i < ELEMS; i += 4) {
        float32x4_t x = vld1q_f32(&d->xf_trig[i]);
        vst1q_f32(&d->gf_sin[i], Sleef_sinf4_u10advsimd(x));
        vst1q_f32(&d->gf_cos[i], Sleef_cosf4_u10advsimd(x));
        x = vld1q_f32(&d->xf_exp[i]);
        vst1q_f32(&d->gf_exp[i], Sleef_expf4_u10advsimd(x));
        x = vld1q_f32(&d->xf_log[i]);
        vst1q_f32(&d->gf_log[i], Sleef_logf4_u10advsimd(x));
    }
    for (int i = 0; i < ELEMS; ++i) {
        double gs = d->gd_sin[i], gc = d->gd_cos[i];
        double ge = d->gd_exp[i], gl = d->gd_log[i];
        if (gs != gs || gs > 1.0 || gs < -1.0 || gc != gc || gc > 1.0 || gc < -1.0 ||
            ge != ge || ge > 1.0e300 || ge < 0.0 ||
            gl != gl || gl > 1.0e300 || gl < -1.0e300) {
            report_fail_msg("double golden value not finite/in-range at element %d "
                            "(domain mapping needs tighter scaling)", i);
        }
    }
    for (int i = 0; i < ELEMS; ++i) {
        float gs = d->gf_sin[i], gc = d->gf_cos[i];
        float ge = d->gf_exp[i], gl = d->gf_log[i];
        if (gs != gs || gs > 1.0f || gs < -1.0f || gc != gc || gc > 1.0f || gc < -1.0f ||
            ge != ge || ge > 1.0e30f || ge < 0.0f ||
            gl != gl || gl > 1.0e30f || gl < -1.0e30f) {
            report_fail_msg("float golden value not finite/in-range at element %d "
                            "(domain mapping needs tighter scaling)", i);
        }
    }
    return EXIT_SUCCESS;
}

static int sleef_neon_run(struct test *test, int cpu)
{
    auto d = CAST(test->data);
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch output buffers */
        if (__builtin_expect(!od || !of, 0)) {
            od = (double *)calloc(1, ELEMS * sizeof(double));
            of = (float  *)calloc(1, ELEMS * sizeof(float));
            if (!od || !of)
                report_fail_msg("OOM allocating thread scratch (%zu bytes)",
                                (size_t)ELEMS * (sizeof(double) + sizeof(float)));
        }

        /* compute-out: each family replays its own domain inputs through
         * the same kernels and lands in the thread-private scratch */
        for (int i = 0; i < ELEMS; i += 2) {
            float64x2_t x = vld1q_f64(&d->xd_trig[i]);
            vst1q_f64(&od[i], Sleef_sind2_u10advsimd(x));
        }
        memcmp_or_fail(od, d->gd_sin, ELEMS, "sin double");

        for (int i = 0; i < ELEMS; i += 2) {
            float64x2_t x = vld1q_f64(&d->xd_trig[i]);
            vst1q_f64(&od[i], Sleef_cosd2_u10advsimd(x));
        }
        memcmp_or_fail(od, d->gd_cos, ELEMS, "cos double");

        for (int i = 0; i < ELEMS; i += 2) {
            float64x2_t x = vld1q_f64(&d->xd_exp[i]);
            vst1q_f64(&od[i], Sleef_expd2_u10advsimd(x));
        }
        memcmp_or_fail(od, d->gd_exp, ELEMS, "exp double");

        for (int i = 0; i < ELEMS; i += 2) {
            float64x2_t x = vld1q_f64(&d->xd_log[i]);
            vst1q_f64(&od[i], Sleef_logd2_u10advsimd(x));
        }
        memcmp_or_fail(od, d->gd_log, ELEMS, "log double");

        for (int i = 0; i < ELEMS; i += 4) {
            float32x4_t x = vld1q_f32(&d->xf_trig[i]);
            vst1q_f32(&of[i], Sleef_sinf4_u10advsimd(x));
        }
        memcmp_or_fail(of, d->gf_sin, ELEMS, "sin float");

        for (int i = 0; i < ELEMS; i += 4) {
            float32x4_t x = vld1q_f32(&d->xf_trig[i]);
            vst1q_f32(&of[i], Sleef_cosf4_u10advsimd(x));
        }
        memcmp_or_fail(of, d->gf_cos, ELEMS, "cos float");

        for (int i = 0; i < ELEMS; i += 4) {
            float32x4_t x = vld1q_f32(&d->xf_exp[i]);
            vst1q_f32(&of[i], Sleef_expf4_u10advsimd(x));
        }
        memcmp_or_fail(of, d->gf_exp, ELEMS, "exp float");

        for (int i = 0; i < ELEMS; i += 4) {
            float32x4_t x = vld1q_f32(&d->xf_log[i]);
            vst1q_f32(&of[i], Sleef_logf4_u10advsimd(x));
        }
        memcmp_or_fail(of, d->gf_log, ELEMS, "log float");
    }
    return EXIT_SUCCESS;
}

static int sleef_neon_cleanup(struct test *test)
{
    auto d = CAST(test->data);
    free(d->xd_trig); free(d->xd_exp); free(d->xd_log);
    free(d->xf_trig); free(d->xf_exp); free(d->xf_log);
    free(d->gd_sin); free(d->gd_cos); free(d->gd_exp); free(d->gd_log);
    free(d->gf_sin); free(d->gf_cos); free(d->gf_exp); free(d->gf_log);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(sleef_neon, "SLEEF vectorized transcendentals (NEON polynomial FMA chains, sin/cos/exp/log double+float)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = sleef_neon_init,
  .test_run = sleef_neon_run,
  .test_cleanup = sleef_neon_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

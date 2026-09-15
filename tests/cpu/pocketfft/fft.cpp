/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b pocketfft_fft
 * @parblock
 * FFT stress via pocketfft (BSD-3, the classic SciPy FFT engine, master
 * C edition): a complex forward cfft whose iterative multi-pass
 * factorisation (radix 4/2 for N=4096) walks twiddle-FMA butterfly
 * chains and bit-reversal-style decimation-in-frequency reordering —
 * store->load scatter pressure across the working set, the access
 * structure that discriminates the CORE179 defect. Each iteration
 * copies the input, runs the forward transform and byte-compares the
 * spectrum against the init-time golden spectrum (deterministic for
 * fixed input + fixed binary). The inverse cfft (1/N scaling) is then
 * run as pure additional load: the forward->backward round trip is NOT
 * bit-exact under FFT rounding (measured max rel err ~3e-11 on this
 * very payload) and is therefore never compared — the SDC verdict
 * comes exclusively from the golden-spectrum memcmp.
 * @endparblock
 */

#include <sandstone.h>

/* pocketfft.h has no extern "C" guard and pocketfft.c is compiled as C
 * into tests_base_a, so the declarations must be given C linkage here. */
extern "C" {
#include <pocketfft.h>
}

#include <string.h>
#include <stdlib.h>

#define FFT_N 4096   /* power of 2; 4096 complex doubles = 64KB working set */

namespace {
struct fft_test_data {
    double *input;        /* interleaved re,im — length 2*FFT_N, read-only after init */
    double *golden_spec;  /* forward spectrum of input, computed once in init */
    cfft_plan plan;       /* shared, see the threading note below */
};
}

#define CAST(_x) static_cast<struct fft_test_data *>(_x)

/* The plan is created in init and shared by all worker threads: reading
 * pocketfft.c, a cfftp plan holds only the factorisation and the
 * precomputed twiddle tables (plan->fct[].tw / tws point into plan->mem),
 * which are read-only after make_cfftp_plan() returns; every per-call
 * scratch buffer (the pass_all() ch buffer, passg()'s wal, the Bluestein
 * akf) is malloc'd locally inside the call and freed before returning, and
 * the library has no mutable static state. So concurrent cfft_forward()
 * calls on one plan are safe — unlike the OpenBLAS packing-buffer pool
 * there is no shared mutable metadata at all. Sharing the plan also keeps
 * make/destroy (a malloc + twiddle-table rebuild) out of the timed loop,
 * where it would be pure allocator noise.
 *
 * The work buffer IS written every iteration, so it is per-thread like
 * the openblas_* scratch buffers: allocated lazily on first use and
 * intentionally not freed in cleanup — the worker threads are gone (or
 * never existed) when the main thread runs test_cleanup, and the forked
 * child exits right after, so the OS reclaims the storage. calloc zeroes
 * it once so a skipped/short first iteration can never compare garbage. */
static thread_local double *work = nullptr;

/* Map a random uint64 onto a double in [-1, 1) (top 53 bits as mantissa
 * fraction, folded around zero). FFT has no domain restriction on its
 * inputs, but the bounded magnitude keeps the spectrum bounded by
 * N*max|x| = 4096 (measured max |X_k| ~142 for this payload), so every
 * butterfly value stays finite and far from overflow — a NaN/Inf would
 * poison the byte-exact comparison (NaN != NaN). */
static double random_bounded(void)
{
    uint64_t r = random64();
    double frac = (double)(r >> 11) / 9007199254740992.0; /* [0, 1) */
    return frac * 2.0 - 1.0;
}

static int pocketfft_fft_init(struct test *test)
{
    auto d = new(fft_test_data);
    test->data = d;
    d->input       = (double *)malloc(2 * FFT_N * sizeof(double));
    d->golden_spec = (double *)malloc(2 * FFT_N * sizeof(double));
    d->plan        = nullptr;
    if (!d->input || !d->golden_spec) {
        report_fail_msg("OOM allocating %zu bytes", 2 * FFT_N * sizeof(double) * 2);
    }
    /* high-entropy random input (framework RNG; libc rand is trapped) */
    for (int i = 0; i < 2 * FFT_N; ++i)
        d->input[i] = random_bounded();

    d->plan = make_cfft_plan(FFT_N);
    if (!d->plan)
        report_fail_msg("make_cfft_plan(%d) failed", FFT_N);

    /* golden spectrum, once: forward of the very input above with the very
     * plan the run loop will use (same binary + same input => bit-identical
     * spectrum; forward repeat determinism empirically verified: 100
     * repeat calls memcmp == 0) */
    memcpy(d->golden_spec, d->input, 2 * FFT_N * sizeof(double));
    if (cfft_forward(d->plan, d->golden_spec, 1.0) != 0)
        report_fail_msg("cfft_forward failed while computing the golden spectrum");

    /* finiteness tripwire: a non-finite golden value would make the
     * byte-exact comparison meaningless, so fail loudly at the source
     * (same tripwire as openblas_dgemm / sleef_neon). With |x| < 1 and
     * N = 4096 every |X_k| is bounded by 4096, so any hit here means the
     * input mapping or the library broke. */
    for (int i = 0; i < 2 * FFT_N; ++i) {
        double g = d->golden_spec[i];
        if (g != g || g > 1.0e6 || g < -1.0e6) {
            report_fail_msg("golden spectrum not finite/in-range at element %d "
                            "(input mapping needs tighter scaling)", i);
        }
    }
    return EXIT_SUCCESS;
}

static int pocketfft_fft_run(struct test *test, int cpu)
{
    auto d = CAST(test->data);
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch buffer */
        if (__builtin_expect(!work, 0)) {
            work = (double *)calloc(2 * FFT_N, sizeof(double));
            if (!work)
                report_fail_msg("OOM allocating thread scratch (%zu bytes)",
                                2 * FFT_N * sizeof(double));
        }

        /* copy-in (dirties the cache lines, then the butterflies reload
         * them through the decimated access pattern) */
        memcpy(work, d->input, 2 * FFT_N * sizeof(double));

        /* forward: the SDC verdict — spectrum must be byte-identical to
         * the init-time golden spectrum */
        if (cfft_forward(d->plan, work, 1.0) != 0)
            report_fail_msg("cfft_forward failed on cpu %d", cpu);
        memcmp_or_fail(work, d->golden_spec, 2 * FFT_N, "golden spectrum");

        /* inverse with 1/N scaling: pure load. The round trip is not
         * bit-exact under FFT rounding (measured max rel err ~3e-11 on
         * this payload) so its result is deliberately NOT compared —
         * comparing it would make the test fail on healthy silicon. */
        if (cfft_backward(d->plan, work, 1.0 / FFT_N) != 0)
            report_fail_msg("cfft_backward failed on cpu %d", cpu);
    }
    return EXIT_SUCCESS;
}

static int pocketfft_fft_cleanup(struct test *test)
{
    auto d = CAST(test->data);
    destroy_cfft_plan(d->plan);
    free(d->input);
    free(d->golden_spec);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(pocketfft_fft, "pocketfft complex FFT (bit-reversal decimation scatter + twiddle FMA butterflies)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = pocketfft_fft_init,
  .test_run = pocketfft_fft_run,
  .test_cleanup = pocketfft_fft_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

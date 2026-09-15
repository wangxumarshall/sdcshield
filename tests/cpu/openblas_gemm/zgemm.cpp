/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b openblas_zgemm
 * @parblock
 * Stress test for the vector FMA units using OpenBLAS's hand-written NEON
 * zgemm micro-kernel (TSV110). Literature identifies vector FMA as the #1
 * SDC source (SEVI ASPLOS'26: >92% of vector SDC incidents; Veritas HPCA'25:
 * vector units orders of magnitude above scalar). Each iteration copies the
 * inputs, computes C = A*B, and byte-compares against the golden product
 * from init (copy->compute->verify, the most frequent CORE179 trigger
 * structure). Complex double: the kernel decomposes each complex GEMM into
 * four real GEMMs (CORE179 §3.7: cdouble is the lowest-rate but
 * confirmed-triggering eigen path; this widens complex-path coverage with a
 * different scheduling sample).
 * @endparblock
 */

#include <sandstone.h>

#include <cblas.h>

#include <string.h>
#include <stdlib.h>

#define M_DIM 256

namespace {
struct zgemm_test_data {
    double *a;           /* interleaved (re, im) pairs: 2*n2 doubles */
    double *b;
    double *golden;      /* C = A*B computed once in init; read-only after */
};

/* Scratch buffers are per-thread: the framework runs one worker thread per
 * core and they all share test->data, so anything written during test_run
 * must be thread-local (a_copy/b_copy are the copy-in destinations, c is
 * the product destination; all three are written every iteration). They are
 * allocated lazily on first use and intentionally not freed in cleanup:
 * the worker threads are gone (or never existed) when the main thread runs
 * test_cleanup, and the whole forked child exits right after, so the OS
 * reclaims the storage. calloc zeroes c once so a skipped/short first
 * iteration can never compare garbage. */
static thread_local double *a_copy = nullptr;
static thread_local double *b_copy = nullptr;
static thread_local double *c = nullptr;
}

#define CAST(_x) static_cast<struct zgemm_test_data *>(_x)

/* cblas_zgemm takes the scalars as const void* pointing at an interleaved
 * (re, im) double pair; cblas.h's openblas_complex_double is a C99
 * double _Complex in C++ too, so address the constants through the
 * OpenBLAS-free struct layout (double[2]) to keep it plain. */
static const double ONE[2]  = {1.0, 0.0};
static const double ZERO[2] = {0.0, 0.0};

/* Map a random uint64 onto a finite, well-normalised double with a random
 * sign and a random 52-bit mantissa, magnitude in [1e-6, 2e-3]. Same
 * construction and rationale as openblas_dgemm's random_bounded: a raw
 * uint64 reinterpreted as double is a NaN/Inf exponent field about a
 * quarter of the time and NaN would poison the byte-exact comparison. */
static double random_bounded(void) {
    uint64_t r = random64();
    double frac = (double)(r >> 11) / 9007199254740992.0; /* [0, 1) */
    double sign = (r & 1) ? -1.0 : 1.0;
    return sign * (frac * 2.0e-3 + 1.0e-6);
}

static int openblas_zgemm_init(struct test *test) {
    auto d = new(zgemm_test_data);
    test->data = d;
    size_t n2 = 2 * M_DIM * M_DIM;   /* interleaved (re, im) double pairs */
    d->a      = (double *)malloc(n2 * sizeof(double));
    d->b      = (double *)malloc(n2 * sizeof(double));
    d->golden = (double *)malloc(n2 * sizeof(double));
    if (!d->a || !d->b || !d->golden) {
        report_fail_msg("OOM allocating %zu bytes", n2 * sizeof(double) * 3);
    }
    /* high-entropy random operands (framework RNG; libc rand is trapped).
     * Bounded magnitude as in openblas_dgemm: real and imaginary parts are
     * filled independently from random_bounded, so the K=256 complex dot
     * products are bounded by K * (2e-3)^2 ~ 1e-3 per component — forty
     * orders of magnitude from overflow and far from subnormal rounding,
     * while the full 52-bit mantissas stay random (the SDC payload). */
    for (size_t i = 0; i < n2; ++i) {
        d->a[i] = random_bounded();
        d->b[i] = random_bounded();
    }
    cblas_zgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M_DIM, M_DIM, M_DIM,
                ONE, d->a, M_DIM, d->b, M_DIM,
                ZERO, d->golden, M_DIM);
    /* reject a NaN/Inf-polluted golden at the source: if the random operands
     * produced a non-finite product the byte-exact comparison below would be
     * meaningless (NaN != NaN), so fail loudly instead of silently passing.
     * The layout is 2*n2 doubles (both components of every element). */
    for (size_t i = 0; i < n2; ++i) {
        double g = d->golden[i];
        if (g != g || g > 1.0e300 || g < -1.0e300) {
            report_fail_msg("golden product not finite at element %zu "
                            "(random operands need tighter scaling)", i);
        }
    }
    return EXIT_SUCCESS;
}

static int openblas_zgemm_run(struct test *test, int cpu) {
    auto d = CAST(test->data);
    size_t bytes = 2 * M_DIM * M_DIM * sizeof(double);
    long iter = 0;
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch buffers */
        if (__builtin_expect(!a_copy || !b_copy || !c, 0)) {
            free(a_copy); free(b_copy); free(c);
            a_copy = (double *)malloc(bytes);
            b_copy = (double *)malloc(bytes);
            c      = (double *)calloc(1, bytes);
            if (!a_copy || !b_copy || !c)
                report_fail_msg("OOM allocating thread scratch (%zu bytes)", bytes * 3);
        }

        /* copy-in (dirties cache lines, then reloads them in the kernel) */
        memcpy(a_copy, d->a, bytes);
        memcpy(b_copy, d->b, bytes);

        cblas_zgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    M_DIM, M_DIM, M_DIM,
                    ONE, a_copy, M_DIM, b_copy, M_DIM,
                    ZERO, c, M_DIM);

        ++iter;
        /* verify-out: byte-exact golden compare of inputs and product */
        if (memcmp(a_copy, d->a, bytes) != 0) {
            report_fail_msg("input A corrupted after GEMM (iteration %ld)", iter);
        }
        if (memcmp(b_copy, d->b, bytes) != 0) {
            report_fail_msg("input B corrupted after GEMM (iteration %ld)", iter);
        }
        memcmp_or_fail(c, d->golden, 2 * M_DIM * M_DIM);
    }
    return EXIT_SUCCESS;
}

static int openblas_zgemm_cleanup(struct test *test) {
    auto d = CAST(test->data);
    free(d->a); free(d->b); free(d->golden);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(openblas_zgemm, "OpenBLAS ZGEMM (complex double NEON FMA, copy/compute/verify per iteration)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = openblas_zgemm_init,
  .test_run = openblas_zgemm_run,
  .test_cleanup = openblas_zgemm_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

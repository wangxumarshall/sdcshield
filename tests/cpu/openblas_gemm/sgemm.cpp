/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b openblas_sgemm
 * @parblock
 * Stress test for the vector FMA units using OpenBLAS's hand-written NEON
 * sgemm micro-kernel (TSV110). Literature identifies vector FMA as the #1
 * SDC source (SEVI ASPLOS'26: >92% of vector SDC incidents; Veritas HPCA'25:
 * vector units orders of magnitude above scalar). Each iteration copies the
 * inputs, computes C = A*B, and byte-compares against the golden product
 * from init (copy->compute->verify, the most frequent CORE179 trigger
 * structure). Single precision: the narrower lanes double the per-line
 * vector throughput of the fmla pipeline, and float and double exercise
 * different corner bits of the multiply-add datapath (SEVI: single-lane
 * failures dominate). A scheduling sample beside dgemm/Eigen/ACL GEMM.
 * @endparblock
 */

#include <sandstone.h>

#include <cblas.h>

#include <string.h>
#include <stdlib.h>

#define M_DIM 256

namespace {
struct sgemm_test_data {
    float *a;
    float *b;
    float *golden;      /* C = A*B computed once in init; read-only after */
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
static thread_local float *a_copy = nullptr;
static thread_local float *b_copy = nullptr;
static thread_local float *c = nullptr;
}

#define CAST(_x) static_cast<struct sgemm_test_data *>(_x)

/* Map a random uint64 onto a finite, well-normalised float with a random
 * sign and a random mantissa fraction, magnitude in [~1e-6, ~2e-3]. Same
 * construction and rationale as openblas_dgemm's random_bounded: a raw
 * uint64 reinterpreted as float would be NaN/Inf about a quarter of the
 * time and NaN would poison the byte-exact comparison. The [0,1) fraction
 * comes from the top 53 bits as a double and narrows to float exactly
 * (round-to-nearest of a [0,1) double stays within [0,1]), so the value
 * stays finite and well-scaled in the float domain. */
static float random_bounded(void) {
    uint64_t r = random64();
    double frac = (double)(r >> 11) / 9007199254740992.0; /* [0, 1) */
    float sign = (r & 1) ? -1.0f : 1.0f;
    return sign * ((float)frac * 2.0e-3f + 1.0e-6f);
}

static int openblas_sgemm_init(struct test *test) {
    auto d = new(sgemm_test_data);
    test->data = d;
    size_t n2 = M_DIM * M_DIM;
    d->a      = (float *)malloc(n2 * sizeof(float));
    d->b      = (float *)malloc(n2 * sizeof(float));
    d->golden = (float *)malloc(n2 * sizeof(float));
    if (!d->a || !d->b || !d->golden) {
        report_fail_msg("OOM allocating %zu bytes", n2 * sizeof(float) * 3);
    }
    /* high-entropy random operands (framework RNG; libc rand is trapped).
     * Bounded magnitude as in openblas_dgemm: the K=256 dot products are
     * bounded by K * (2e-3)^2 ~ 1e-3 — nowhere near float overflow and far
     * from subnormal rounding, while the random mantissa bits remain the
     * SDC payload. */
    for (size_t i = 0; i < n2; ++i) {
        d->a[i] = random_bounded();
        d->b[i] = random_bounded();
    }
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M_DIM, M_DIM, M_DIM,
                1.0f, d->a, M_DIM, d->b, M_DIM,
                0.0f, d->golden, M_DIM);
    /* reject a NaN/Inf-polluted golden at the source: if the random operands
     * produced a non-finite product the byte-exact comparison below would be
     * meaningless (NaN != NaN), so fail loudly instead of silently passing */
    for (size_t i = 0; i < n2; ++i) {
        float g = d->golden[i];
        if (g != g || g > 1.0e30f || g < -1.0e30f) {
            report_fail_msg("golden product not finite at element %zu "
                            "(random operands need tighter scaling)", i);
        }
    }
    return EXIT_SUCCESS;
}

static int openblas_sgemm_run(struct test *test, int cpu) {
    auto d = CAST(test->data);
    size_t bytes = M_DIM * M_DIM * sizeof(float);
    long iter = 0;
    TEST_LOOP(test, 1) {
        /* lazily allocate this thread's scratch buffers */
        if (__builtin_expect(!a_copy || !b_copy || !c, 0)) {
            free(a_copy); free(b_copy); free(c);
            a_copy = (float *)malloc(bytes);
            b_copy = (float *)malloc(bytes);
            c      = (float *)calloc(1, bytes);
            if (!a_copy || !b_copy || !c)
                report_fail_msg("OOM allocating thread scratch (%zu bytes)", bytes * 3);
        }

        /* copy-in (dirties cache lines, then reloads them in the kernel) */
        memcpy(a_copy, d->a, bytes);
        memcpy(b_copy, d->b, bytes);

        cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    M_DIM, M_DIM, M_DIM,
                    1.0f, a_copy, M_DIM, b_copy, M_DIM,
                    0.0f, c, M_DIM);

        ++iter;
        /* verify-out: byte-exact golden compare of inputs and product */
        if (memcmp(a_copy, d->a, bytes) != 0) {
            report_fail_msg("input A corrupted after GEMM (iteration %ld)", iter);
        }
        if (memcmp(b_copy, d->b, bytes) != 0) {
            report_fail_msg("input B corrupted after GEMM (iteration %ld)", iter);
        }
        memcmp_or_fail(c, d->golden, M_DIM * M_DIM);
    }
    return EXIT_SUCCESS;
}

static int openblas_sgemm_cleanup(struct test *test) {
    auto d = CAST(test->data);
    free(d->a); free(d->b); free(d->golden);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(openblas_sgemm, "OpenBLAS SGEMM (NEON FMA micro-kernel, single precision, copy/compute/verify per iteration)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = openblas_sgemm_init,
  .test_run = openblas_sgemm_run,
  .test_cleanup = openblas_sgemm_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

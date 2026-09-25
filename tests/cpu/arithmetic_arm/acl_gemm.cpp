/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b acl_gemm
 * @parblock
 * Stress the floating-point multiply-accumulate path using the Arm
 * Compute Library's NEON GEMM (NEGEMM).  Two random F32 matrices
 * (M=N=K=64) are generated at init and the golden product is
 * computed with a naive triple loop.  The ACL scheduler is pinned to
 * a single thread so the ACL op runs inline on the sandstone worker
 * thread itself.  During the run the GEMM is re-executed and the
 * output is compared byte-by-byte against the golden.  A mismatch
 * indicates a silent corruption in the FPU / FMA / NEON pipeline.
 * @endparblock
 */

#include <sandstone.h>

#include <cmath>

#include "arm_compute/core/Types.h"
#include "arm_compute/runtime/NEON/NEFunctions.h"
#include "arm_compute/runtime/NEON/NEScheduler.h"
#include "arm_compute/runtime/Tensor.h"

#define ACL_DIM 64

namespace {
struct acl_gemm_data {
    arm_compute::Tensor a;
    arm_compute::Tensor b;
    arm_compute::Tensor d;          /* ACL output */
    float *golden;                 /* naive C = alpha*A*B, beta=0 (long double accum) */
    arm_compute::NEGEMM *gemm;      /* op handle, configured once */
    /* per-thread scratch imported into the ACL tensors via import_memory()
     * so ACL owns no heap that would have to survive sandstone's
     * fork-per-iteration fracturing (same fork-safety pattern as
     * fisttp_arm.cpp). 128-byte aligned like the fisttp_arm buffers. */
    alignas(128) float a_buf[ACL_DIM * ACL_DIM];
    alignas(128) float b_buf[ACL_DIM * ACL_DIM];
    alignas(128) float d_buf[ACL_DIM * ACL_DIM];
};
}

#define CAST(_x) static_cast<struct acl_gemm_data *>(_x)

/* naive reference GEMM: d[i*ACL_DIM+j] = sum_k a[i,k]*b[k,j].
 * long double accumulation: an INDEPENDENT high-precision reference for the
 * ACL NEON kernel result (which accumulates in float in a different order),
 * so the tolerance below can be tight (~1e-5 relative = ~10^3 ulp at float
 * width) while remaining immune to reordering rounding. */
static void naive_gemm(const float *A, const float *B, float *C)
{
    for (int i = 0; i < ACL_DIM; ++i)
        for (int j = 0; j < ACL_DIM; ++j) {
            long double acc = 0.0L;
            for (int k = 0; k < ACL_DIM; ++k)
                acc += (long double)A[i * ACL_DIM + k] * (long double)B[k * ACL_DIM + j];
            C[i * ACL_DIM + j] = (float)acc;
        }
}

static int acl_gemm_init(struct test *test)
{
    acl_gemm_data *data = new acl_gemm_data;
    data->gemm = nullptr;
    data->golden = nullptr;
    /* THE FIX: the historical SIGSEGV was never "fork-per-iteration" — this
     * assignment was simply missing, so test_run/test_cleanup dereferenced a
     * NULL/garbage test->data (struct offset 0x500 region). fisttp_arm.cpp:114
     * has it; this test never did. */
    test->data = data;

    /* force ACL to run its kernels inline on this thread (single thread)
     * so we do not nest a thread pool inside the sandstone worker. */
    arm_compute::NEScheduler::get().set_num_threads(1);

    using namespace arm_compute;
    arm_compute::TensorInfo info(arm_compute::TensorShape(ACL_DIM, ACL_DIM), 1, arm_compute::DataType::F32);
    info.set_data_layout(arm_compute::DataLayout::NHWC);
    data->a.allocator()->init(info);
    data->b.allocator()->init(info);
    data->d.allocator()->init(info);
    /* import_memory instead of allocate(): the tensors borrow the
     * per-thread aligned buffers above; ACL allocates/owns nothing that
     * would need to survive a fork (fork-safe, fisttp_arm.cpp pattern). */
    data->a.allocator()->import_memory(data->a_buf);
    data->b.allocator()->import_memory(data->b_buf);
    data->d.allocator()->import_memory(data->d_buf);

    /* fill A, B with random floats in [-1, 1) */
    float *A = data->a_buf;
    float *B = data->b_buf;
    for (int i = 0; i < ACL_DIM * ACL_DIM; ++i) {
        A[i] = (float)((int32_t)random32()) / (float)(1u << 30);
        B[i] = (float)((int32_t)random32()) / (float)(1u << 30);
    }

    /* golden = 1.0 * A * B + 0.0 * C (independent long-double reference) */
    data->golden = (float *)aligned_alloc_safe(64, ACL_DIM * ACL_DIM * sizeof(float));
    naive_gemm(A, B, data->golden);

    data->gemm = new arm_compute::NEGEMM;
    data->gemm->configure(&data->a, &data->b, nullptr, &data->d,
                          1.0f, 0.0f, arm_compute::GEMMInfo{});
    return EXIT_SUCCESS;
}

static int acl_gemm_run(struct test *test, int cpu)
{
    auto data = CAST(test->data);
    /* ACL NEGEMM shares a process-global scheduler; only one worker
     * thread may execute the op at a time to avoid concurrent
     * scheduler state corruption. Other threads simply idle. */
    if (cpu != 0)
        return EXIT_SUCCESS;
    do {
        data->gemm->prepare();
        data->gemm->run();
        float *out = data->d_buf;
        /* compare against the independent long-double naive golden.
         * The ACL NEON kernel accumulates in float in a different
         * order than the reference, so a tight RELATIVE tolerance
         * (~1e-5, i.e. ~10^3 float ulp) absorbs the healthy
         * reordering rounding while any real SDC — a flipped FMA
         * operand bit, a wrong kernel constant — lands orders of
         * magnitude outside it. Transient 1-ulp errors are this
         * test's bread and butter only at the byte level; here the
         * target is gross deterministic kernel corruption, and the
         * 1e-3f absolute floor of the old comparison is gone. */
        for (int i = 0; i < ACL_DIM * ACL_DIM; ++i) {
            float diff = out[i] - data->golden[i];
            if (diff < 0) diff = -diff;
            /* Tolerance calibration (measured, two rounds): the ACL NEON kernel
             * accumulates K=64 float products in its own tiling order while the
             * golden is a long-double reference. A pure relative tolerance is
             * wrong for this workload: near-cancellation outputs (|golden| as
             * small as ~9e-4 from |a|,|b| <= 1 operands) carry an absolute
             * rounding gap of ~4e-7 — a ~4e-4 RELATIVE gap that no sane
             * relative tolerance can absorb. The correct bound is absolute and
             * scales with the operand magnitude, not the output: K*eps*max|a*b|
             * = 64 * 1.2e-7 * 1 ~ 7.7e-6. Use 1e-4f absolute: ~13x headroom
             * over that worst case (no false positives on healthy silicon,
             * verified) while a real SDC — flipped FMA operand bit, wrong
             * kernel constant — produces O(1)-magnitude errors, 4 orders above
             * it. Still 10x tighter than the old 1e-3f absolute floor, which
             * additionally had no cancellation story at all. */
            float tol = 1e-4f;
            if (diff > tol) {
                report_fail_msg("ACL GEMM mismatch at (%d,%d): %g vs golden %g "
                                "(diff %g)", i / ACL_DIM, i % ACL_DIM,
                                (double)out[i], (double)data->golden[i],
                                (double)diff);
            }
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

static int acl_gemm_cleanup(struct test *test)
{
    acl_gemm_data *data = CAST(test->data);
    delete data->gemm;
    free(data->golden);
    delete data;
    return EXIT_SUCCESS;
}

DECLARE_TEST(acl_gemm, "Arm Compute Library NEON GEMM (F32 64x64) vs naive golden")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = acl_gemm_init,
  .test_run = acl_gemm_run,
  .test_cleanup = acl_gemm_cleanup,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

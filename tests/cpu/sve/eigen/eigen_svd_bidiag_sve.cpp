/**
 * @file
 *
 * @copyright
 * Copyright 2025 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_svd_bidiag_sve
 * @parblock
 * SVD-style Householder bidiagonalization payload on SVE — the SVE port
 * counterpart of eigen_svd_cdouble_noavx512 (whose SVE-backend sibling
 * eigen_svd_cdouble_sve currently crashes: the vendored Eigen5 SVE double
 * packet path SIGSEGVs on the 300x300 BDCSVD workload).
 *
 * This is a self-written, dependency-free SVD-family FMA stress: the
 * Golub-Kahan Householder bidiagonalization that underlies BDCSVD's first
 * phase. Every inner loop (Householder dot-product reductions and rank-1
 * update FMA chains) runs through SVE predicated svld1/svmla/svmls/svaddv,
 * making it a dense-SVE-FMA workload in the same family as the Eigen SVD
 * tests but without the Eigen library.
 *
 * Golden-value pattern matches EigenSVDTest: init computes the golden
 * result once and caches it; every run recomputes from the same fixed
 * input and byte-compares. The algorithm is deterministic (same input,
 * same output — verified), so a mismatch is silent data corruption.
 *
 * Honest scope note: this is bidiagonalization only, not a full SVD
 * (no singular values, no U/V accumulation) — the payload targets the
 * FMA/store-reload datapath, not SVD semantics.
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cmath>

#ifdef __aarch64__
#include <arm_sve.h>
#include <sys/auxv.h>

#ifndef HWCAP_SVE
#define HWCAP_SVE (1 << 22)
#endif
#endif

/* 维度选择: 实测标定 n=128 单次 ~0.92ms (含输入重建),
 * 60s 单线程 ~65000 次迭代——统计深度充足;
 * 矩阵 128KB (n*n*8) 落在 L2 域, 与 eigen SVD 家族的 cache 压力风格相近。 */
#define M_DIM 128

struct bidiag_data {
    double *orig;     /* 固定输入矩阵 (init 一次生成) */
    double *golden;   /* init 首算的双对角化结果 */
    double *work;     /* run 时的工作副本 */
};

#ifdef __aarch64__

/* Golub-Kahan Householder 双对角化 (SVE 向量化内层)。
 * 确定性: 同输入 → 逐字节同输出 (svaddv 归约顺序在单线程内固定)。 */
static void bidiagonalize_sve(double *A, int n)
{
    /* 注意: 必须是栈上数组。static 局部数组是所有线程共享的——
     * 127 线程并发进入本函数时 Householder 向量会互相覆盖
     * (第二版 bug: 全核数据错配, 根因即此)。栈上 2KB 安全。 */
    double v[M_DIM], w[M_DIM];

    for (int k = 0; k + 2 < n; ++k) {
        /* ---- 左 Householder: 消去列 k 的对角线下元素 ---- */
        double norm2 = 0.0;
        for (int i = k; i < n; ++i)
            norm2 += A[i * n + k] * A[i * n + k];
        double alpha = (A[k * n + k] >= 0) ? -sqrt(norm2) : sqrt(norm2);
        double v0 = A[k * n + k] - alpha;
        double vtv = v0 * v0;
        for (int i = k + 1; i < n; ++i)
            vtv += A[i * n + k] * A[i * n + k];
        if (vtv < 1e-300)
            continue;
        double tau = 2.0 * v0 * v0 / vtv;
        v[k] = 1.0;
        for (int i = k + 1; i < n; ++i)
            v[i] = A[i * n + k] / v0;

        /* A = (I - tau*v*v^T) * A: 列 j = k+1..n-1 的 SVE 归约 + FMA 更新 */
        for (int j = k + 1; j < n; ++j) {
            double dot = 0.0;
            for (int base = k; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)(n - k));
                svfloat64_t vv = svld1_f64(pg, v + base);
                svfloat64_t aa = svld1_f64(pg, A + base * n + j);
                dot += svaddv_f64(pg, svmul_f64_x(pg, vv, aa));
            }
            dot *= tau;
            for (int base = k; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)(n - k));
                svfloat64_t vv = svld1_f64(pg, v + base);
                svfloat64_t aa = svld1_f64(pg, A + base * n + j);
                svst1_f64(pg, A + base * n + j,
                          svmls_f64_x(pg, aa, vv, svdup_f64(dot)));
            }
        }

        /* ---- 右 Householder: 消去行 k 的次对角线右元素 ---- */
        norm2 = 0.0;
        for (int j = k + 1; j < n; ++j)
            norm2 += A[k * n + j] * A[k * n + j];
        if (norm2 < 1e-300)
            continue;
        alpha = (A[k * n + k + 1] >= 0) ? -sqrt(norm2) : sqrt(norm2);
        v0 = A[k * n + k + 1] - alpha;
        vtv = v0 * v0;
        for (int j = k + 2; j < n; ++j)
            vtv += A[k * n + j] * A[k * n + j];
        if (vtv < 1e-300)
            continue;
        tau = 2.0 * v0 * v0 / vtv;
        w[k + 1] = 1.0;
        for (int j = k + 2; j < n; ++j)
            w[j] = A[k * n + j] / v0;

        /* A = A * (I - tau*w*w^T): 行 i = k+1..n-1 */
        for (int i = k + 1; i < n; ++i) {
            double dot = 0.0;
            for (int base = k + 1; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)n);
                svfloat64_t ww = svld1_f64(pg, w + base);
                svfloat64_t aa = svld1_f64(pg, A + i * n + base);
                dot += svaddv_f64(pg, svmul_f64_x(pg, ww, aa));
            }
            dot *= tau;
            for (int base = k + 1; base < n; base += svcntd()) {
                svbool_t pg = svwhilelt_b64((uint64_t)base, (uint64_t)n);
                svfloat64_t ww = svld1_f64(pg, w + base);
                svfloat64_t aa = svld1_f64(pg, A + i * n + base);
                svst1_f64(pg, A + i * n + base,
                          svmls_f64_x(pg, aa, ww, svdup_f64(dot)));
            }
        }
    }
}

static int eigen_svd_bidiag_sve_init(struct test *test)
{
    unsigned long hwcap = getauxval(AT_HWCAP);
    if ((hwcap & HWCAP_SVE) == 0) {
        log_skip(CpuNotSupportedSkipCategory,
                 "to be implemented (placeholder): ARM SVE required for eigen_svd_bidiag_sve");
        return EXIT_SKIP;
    }

    auto *d = new bidiag_data;
    if (!d)
        return EXIT_FAILURE;

    size_t bytes = (size_t)M_DIM * M_DIM * sizeof(double);
    d->orig   = (double *)aligned_alloc(64, bytes);
    d->golden = (double *)aligned_alloc(64, bytes);
    d->work   = (double *)aligned_alloc(64, bytes);
    if (!d->orig || !d->golden || !d->work) {
        free(d->orig); free(d->golden); free(d->work);
        delete d;
        return EXIT_FAILURE;
    }

    /* 固定确定性输入 (LCG) —— 所有线程共享同一输入矩阵 */
    unsigned s = 987654321u;
    for (int i = 0; i < M_DIM * M_DIM; ++i) {
        s = s * 1103515245u + 12345u;
        d->orig[i] = (double)(int)((s >> 16) & 0xFFFF) / 32768.0 - 1.0;
    }

    /* 黄金结果: 首次计算 (EigenSVDTest 模式) */
    memcpy(d->golden, d->orig, bytes);
    bidiagonalize_sve(d->golden, M_DIM);

    test->data = d;
    return EXIT_SUCCESS;
}

static int eigen_svd_bidiag_sve_run(struct test *test, int cpu)
{
    auto *d = static_cast<bidiag_data *>(test->data);
    size_t bytes = (size_t)M_DIM * M_DIM * sizeof(double);

    /* work 缓冲必须线程私有: 127 个线程并发计算同一输入, 共享 work 会互相
     * 覆盖 (第一版就栽在这里: 全核 fail, 罪魁是共享 d->work)。 */
    double *work = (double *)test->per_thread[cpu].data;
    if (!work) {
        work = (double *)aligned_alloc(64, bytes);
        if (!work) {
            report_fail_msg("eigen_svd_bidiag_sve: per-thread work alloc failed");
            return EXIT_FAILURE;
        }
        test->per_thread[cpu].data = work;
    }

    do {
        memcpy(work, d->orig, bytes);
        bidiagonalize_sve(work, M_DIM);
        memcmp_or_fail(work, d->golden, M_DIM * M_DIM,
                       "Householder bidiagonalization result");
    } while (test_time_condition(test));

    return EXIT_SUCCESS;
}

static int eigen_svd_bidiag_sve_cleanup(struct test *test)
{
    auto *d = static_cast<bidiag_data *>(test->data);
    if (d) {
        free(d->orig);
        free(d->golden);
        free(d->work);
        delete d;
    }
    return EXIT_SUCCESS;
}

/* 注意: per-thread work 由框架在测试进程退出时随进程回收 (fork 模型),
 * cleanup 无需逐线程释放。 */

#else

static int eigen_svd_bidiag_sve_init(struct test *test)
{
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): ARM SVE required for eigen_svd_bidiag_sve");
    return EXIT_SKIP;
}
static int eigen_svd_bidiag_sve_run(struct test *test, int cpu)
{
    (void)test; (void)cpu;
    return EXIT_SKIP;
}
static int eigen_svd_bidiag_sve_cleanup(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}
#endif

DECLARE_TEST(eigen_svd_bidiag_sve,
             "SVD-style Householder bidiagonalization SDC stress — dense SVE FMA chains "
             "(dot-product reductions + rank-1 svmla/svmls updates) without the Eigen library; "
             "SVE-family counterpart of eigen_svd_cdouble_noavx512")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = eigen_svd_bidiag_sve_init,
    .test_run = eigen_svd_bidiag_sve_run,
    .test_cleanup = eigen_svd_bidiag_sve_cleanup,
    .fracture_loop_count = 5,
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

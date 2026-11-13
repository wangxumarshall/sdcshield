/*
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Eigen GEMM payload body — complex<double>,compiled ONCE PER VL TIER
 * (128/256/512/1024)。run 语义镜像 gemm_cdouble_dynamic_square.cpp 的
 * run(per-thread operands + golden 同源首算 + kRerollEvery 重掷 +
 * memcmp_or_fail;T10 bad_alloc 窄 catch 边界契约;比对口径同原用例
 * reinterpret_cast<double *> + 2*dim*dim——memcmp_or_fail 的模板约束
 * ValidDataType 不含 complex,T 推导口径不可编译,框架事实);
 * solve_once 一行转发共享模板 gemm_solve_probe.h(#392 收口;谱系
 * 魔数/值域单源 eigen_splitmix_lineage.h,契约(检视 #51)不再依赖
 * 本 TU 手工镜像;re/im 独立四流由共享模板 complex 分支承载)。
 * 服务 eigen_gemm_cdouble_dynamic_square_sve。
 * 签名契约见 eigen_svd/sandstone_eigen_sve_dispatch.h。
 */
#include <sandstone.h>
#include "sdc_stress_config.h"
#include "gemm_solve_probe.h"

#include <Eigen/Core>
#include <new>
#include <cstdint>

#include <arm_sve.h>
#include <device/cpu/arm64/arm64_vl.h>
#include <unistd.h>

#ifndef EIGEN_SVE_BODY_NS
#  error "EIGEN_SVE_BODY_NS must be defined by the build (one namespace per VL tier)"
#endif

namespace EIGEN_SVE_BODY_NS {

using namespace Eigen;

typedef Matrix<std::complex<double>, Dynamic, Dynamic> Mat;

constexpr int kRerollEvery = 16;

struct gemm_sve_data { int dim; };

void *make_data(int dim)
{
    auto *d = new gemm_sve_data;
    d->dim = dim;
    return d;
}

void cleanup(struct test *test)
{
    delete static_cast<gemm_sve_data *>(test->data);
    test->data = nullptr;
}

static void fill_rand(Mat &m)
{
    for (int r = 0; r < m.rows(); ++r)
        for (int c = 0; c < m.cols(); ++c)
            m(r, c) = std::complex<double>(frandom_scale(2.0) - 1.0,
                                           frandom_scale(2.0) - 1.0);
}

int run_body(struct test *test, int cpu)
{
    (void)cpu;
    const int dim = static_cast<gemm_sve_data *>(test->data)->dim;

    /* randomization hardening H11'(P14)+ T10 catch 边界:逐字镜像
     * gemm_cdouble_dynamic_square.cpp run 的段结构。 */
    Mat l_lhs, l_rhs, l_prod;
    try {
        l_lhs.resize(dim, dim);
        l_rhs.resize(dim, dim);
        l_prod.resize(dim, dim);
        fill_rand(l_lhs);
        fill_rand(l_rhs);
        l_prod = l_lhs * l_rhs;             /* golden 同源首算 */
    } catch (const std::bad_alloc &) {
        log_skip(RuntimeSkipCategory, "memory: %s mdim=%d", test->id, dim);
        return EXIT_SKIP;
    }
    int since_reroll = 0;

    do {
        Mat x;
        try {
            x = l_lhs * l_rhs;              /* DUT 求解 */
        } catch (const std::bad_alloc &) {
            log_skip(RuntimeSkipCategory, "memory: %s mdim=%d", test->id, dim);
            return EXIT_SKIP;
        }

        /* 比对段在 catch 之外(T10 边界契约)。原用例口径:
         * reinterpret_cast<double *> + 2*dim*dim double(字节覆盖 =
         * dim*dim 个 16B complex 元素)——memcmp_or_fail 的 ValidDataType
         * 约束不含 complex,元素推导口径不可编译(框架事实)。 */
        memcmp_or_fail(reinterpret_cast<double *>(x.data()),
                       reinterpret_cast<double *>(l_prod.data()),
                       2 * (size_t)dim * (size_t)dim);

        if (++since_reroll >= kRerollEvery) {
            since_reroll = 0;
            try {
                fill_rand(l_lhs);
                fill_rand(l_rhs);
                l_prod = l_lhs * l_rhs;     /* golden 重算 */
            } catch (const std::bad_alloc &) {
                log_skip(RuntimeSkipCategory, "memory: %s mdim=%d",
                         test->id, dim);
                return EXIT_SKIP;
            }
        }
    } while (test_time_condition(test));
    return EXIT_SUCCESS;
}

int solve_once(int dim)
{
    /* 联合求解 sizing 探针: 一行转发共享模板, 同
     * gemm_double_max_arm_body.cpp 的转发范式(#392 收口口径)——
     * splitmix 填充谱系魔数/值域单源 eigen_splitmix_lineage.h,
     * 谱系契约(检视 #51)不再依赖本 TU 手工镜像保持同步;re/im
     * 独立四流(a.re/a.im/b.re/b.im)由共享模板 complex 分支承载。 */
    return gemm_probe::solve_once<Mat>(dim);
}

}  // namespace EIGEN_SVE_BODY_NS

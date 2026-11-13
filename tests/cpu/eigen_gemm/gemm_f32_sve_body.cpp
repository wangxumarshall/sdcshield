/*
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Eigen GEMM payload body — float,compiled ONCE PER VL TIER
 * (128/256/512/1024)。run 语义镜像 gemm_float_dynamic_square.cpp 的
 * run(per-thread operands + golden 同源首算 + kRerollEvery 重掷 +
 * memcmp_or_fail;T10 bad_alloc 窄 catch 边界契约);solve_once 一行
 * 转发共享模板 gemm_solve_probe.h(#392 收口;谱系魔数/值域单源
 * eigen_splitmix_lineage.h,契约(检视 #51)不再依赖本 TU 手工
 * 镜像;f32 赋值转型经 f64 中间量由共享模板承载——原用例同款
 * 转型)。服务 eigen_gemm_float_dynamic_square_sve。
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

typedef Matrix<float, Dynamic, Dynamic> Mat;

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
            m(r, c) = frandomf_scale(2.0f) - 1.0f;
}

int run_body(struct test *test, int cpu)
{
    (void)cpu;
    const int dim = static_cast<gemm_sve_data *>(test->data)->dim;

    /* randomization hardening H11'(P14)+ T10 catch 边界:逐字镜像
     * gemm_float_dynamic_square.cpp run 的段结构。 */
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

        /* 比对段在 catch 之外(T10 边界契约)。 */
        memcmp_or_fail(x.data(), l_prod.data(), (size_t)dim * (size_t)dim);

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
     * 谱系契约(检视 #51)不再依赖本 TU 手工镜像保持同步;f32 赋值
     * 转型经 f64 中间量(原用例同款转型)由共享模板的 (Scalar) 收窄
     * 表达式承载。 */
    return gemm_probe::solve_once<Mat>(dim);
}

}  // namespace EIGEN_SVE_BODY_NS

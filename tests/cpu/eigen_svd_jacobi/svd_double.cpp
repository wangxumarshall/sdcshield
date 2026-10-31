/**
 * @file
 *
 * @copyright
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_svd_jacobi_double
 * @parblock
 * This piece of code aims to stress test the FMA execution units of
 * the CPU, among others, by repetitively solving the singular value
 * decomposition problem on given input matrices, which involve a lot
 * of matrix multiplication operations underneath.
 *
 * The logic comes from the 3rd party library Eigen. The first thread
 * that gets to run computes a "golden value" of the results, those
 * being output matrices "U" and "V". Subsequent runs have results
 * contrasted to those golden values and, whenever they differ, an
 * error is flagged.
 *
 * This particular version of the Eigen SVD tests go for double
 * precision input matrices and the "Jacobi" SVD algorithm.
 *
 * This test requires at least 2 threads to run.
 * @endparblock
 */

#include "eigen_svd/sandstone_eigen_common.h"
#include "sdc_stress_config.h"

using namespace Eigen;

typedef Matrix < double, Dynamic, Dynamic > Mat;
typedef Eigen::JacobiSVD < Mat > SVD;

/* 运行时维度模式, 统一配置链。历史硬编码 256。 */
using eigen_svd_jacobi_double_test = EigenSVDTest<SVD, Eigen::Dynamic>;

static int eigen_svd_jacobi_double_init(struct test *test)
{
    auto d = new eigen_svd_jacobi_double_test::eigen_test_data;
    /* -O/env 查询与显式短路已收进 calibrated_auto_dim(T11: arm64 显式值
     * 经联合层 knob 早退打预期内存警告; x86 行为不变)。 */
    d->dim = eigen_svd_jacobi_double_test::calibrated_auto_dim(test, 8, 6, 8.0, 650, 64, 2048);
    test->data = d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(eigen_svd_jacobi_double, "Eigen SVD (Singular Value Decomposition, Jacobi algorithm) solving payload, which issues a bunch of matrix multiplies underneath, now operating on doubles")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = eigen_svd_jacobi_double_init,
  .test_run = eigen_svd_jacobi_double_test::run,
  .test_cleanup = eigen_svd_jacobi_double_test::cleanup,
  /* SKIP 级的来由与现状(补注, 2026-10-31, issue #322):
   * 1) 级别继承自上游 Intel OpenDCDiag——其初始提交(91e4fc86)即
   *    TEST_QUALITY_SKIP, 今日上游 master 仍如此, 仓库内无书面理由;
   * 2) 上游模板是 init 共享 golden + 各 worker 线程重算比对, 跨线程
   *    Eigen SIMD/标量路径分歧(堆分配对齐引导)会让 U/V 字节比对假失配
   *    ——BDC 家族在本 fork 实测过同型故障(2026-10-24 修复, 见
   *    CLAUDE.md)。本 fork 的 per-thread 同源 golden(46141ed4)同样覆盖
   *    Jacobi 四件套, 该结构性风险已消除: 2026-10-31 鲲鹏 920 实测
   *    --quality=-1 下 -n 2 与 -n 16×30s 四用例全 pass;
   * 3) 维持 SKIP 是待满核多 seed 晋升验证(规则 2 口径)并与上游对齐;
   *    显式运行: --quality=-1 -e <test>。 */
  .quality_level = TEST_QUALITY_SKIP,
END_DECLARE_TEST

/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * sandstone_gemm_common.h — eigen_gemm 用例族的共享 helper(issue #440)。
 *
 * 此前 T10 init 降档块(约 25 行)与联合求解探针 gemm_solve_once 在本目录
 * 7 个文件中逐字复制粘贴(double14 / {double,float,cdouble}_dynamic_square
 * 及其 _dump 变体), PR#302 检视 #25 的 sdc_joint_cache_note_dim 修复
 * (09a1c4cf)被迫同步打进全部 7 份——漏一处即口径分叉。本头收敛 init
 * 降档块; 联合求解探针已由 PR #392 的 gemm_solve_probe.h(谱系单源
 * eigen_splitmix_lineage.h, issue #389)承载——本头不复制探针, 两类
 * 修正各落各的唯一权威处。
 *
 * 不在共享范围(如实边界): gemm_mdim 的 knob/env/clamp/fam 是各文件真参数;
 * gemm_double_max_arm 系 init 不持有矩阵、无降档点; eigen_svd_bidiag_sve
 * 的降档是裸 aligned_alloc 口径(lo=64), 与 Eigen Mat 异常口径不同构。
 */

#ifndef SANDSTONE_GEMM_COMMON_H
#define SANDSTONE_GEMM_COMMON_H

#include <sandstone.h>
#include "sdc_stress_config.h"

#include <Eigen/Core>
#include <complex>
#include <cstdint>
#include <new>

/* ------------------------------------------------------------------ */
/* T10 init 降档块(7 用例共享; 跨架构无守卫——替换的原内联代码即无守卫)  */
/* ------------------------------------------------------------------ */

/* eigen_gemm 族 init 主体: 调用方 mdim 函数解维度(-O knob > env > 自适应,
 * 只解一次)→ 共享三矩阵(lhs/rhs/prod)分配与 golden 首算; std::bad_alloc
 * 时 T10 降档(m2, 对齐 eigen_sparse 先例)。Data 是各文件匿名命名空间的
 * eigen_test_data{Mat lhs, rhs, prod; int dim}(成员名即接口), mdim 是该
 * 文件的尺寸求解函数。与被收敛的 7 份内联拷贝逐行等价:
 *   - init 期分配失败是资源性而非 SDC → 降 mdim 到 16(与联合求解的
 *     lo/clamp 下限同值)重分配一次, 仍失败才资源性 SKIP(不打红);
 *   - 降档成功 → sdc_joint_cache_note_dim 同步反馈记账口径(PR#302
 *     检视 #25): 否则反馈按求解 n 折算 eff, c 被压向 0.7× 下限, 后续
 *     n 向 OOM 膨胀;
 *   - 其余异常 → report_fail_msg(打红)。
 * dump 变体的冻结回放轮读 d->lhs/rhs/prod——降档后三者与 d->dim 同档
 * 一致(降档路径对 dump 同样成立)。 */
template <typename Data>
static inline int sdc_gemm_init_shared(struct test *test, Data *d,
                                        int (*mdim)(struct test *))
{
    using Mat = decltype(d->lhs);
    int dim = 0;
    try {
        dim = mdim(test);   /* knob > env > 自适应, 只解一次 */
        d->dim = dim;
        d->lhs = Mat::Random(dim, dim);
        d->rhs = Mat::Random(dim, dim);
        d->prod = d->lhs * d->rhs;
    } catch (const std::bad_alloc &) {
        constexpr int kInitRetryLo = 16;
        log_warning("[init 降级] %s: 共享矩阵分配失败 mdim=%d → 降档 mdim=%d 重试一次",
                    test->id, dim, kInitRetryLo);
        try {
            d->dim = kInitRetryLo;
            d->lhs = Mat::Random(kInitRetryLo, kInitRetryLo);
            d->rhs = Mat::Random(kInitRetryLo, kInitRetryLo);
            d->prod = d->lhs * d->rhs;
        } catch (const std::bad_alloc &) {
            log_skip(RuntimeSkipCategory, "memory: %s mdim=%d",
                     test->id, kInitRetryLo);
            return EXIT_SKIP;
        }
        sdc_joint_cache_note_dim(test, kInitRetryLo);
    } catch (...) {
        report_fail_msg("Exception on Eigen code, most probably OOM");
    }
    return EXIT_SUCCESS;
}

#endif /* SANDSTONE_GEMM_COMMON_H */

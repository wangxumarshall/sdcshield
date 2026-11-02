/* gemm_solve_probe.h — GEMM 联合求解 sizing 探针的共享实现 (issue #389)。
 *
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 谱系契约(检视 #51)在 GEMM 家族承载: probe 填充必须与各测试 run() 的
 * fill_rand(frandom_scale(2.0) - 1.0, 均匀随机 [-1,1))同谱系——splitmix
 * 确定性哈希满足该契约, 且不依赖 init aux 线程上未定义的 per-thread RNG
 * 状态。GEMM 计时对数值不敏感(无 SVD 式 deflation 路径), 谱系由分布
 * 一致性满足; 填充 O(n²) 相对乘法 O(n³) 已计入探针计时(保守方向)。
 * 改填充结构必须与各 run() 的 fill_rand 同步演化; 魔数/值域档位则取自
 * ../eigen_splitmix_lineage.h 单一事实源(与 eigen_svd 家族的
 * sizing_probe 共享, 检视 #392-R1)——改值域只改该头, 勿在此内联魔数。
 *
 * 收口背景(issue #389): 本填充曾以 8 份逐字副本分散在 double14 /
 * gemm_{double,float,cdouble}_dynamic_square{,_dump} 7 个 T9 文件与
 * gemm_double_max_arm_body(T6 源头, 按 4 个 VL tier 编译), 谱系漂移
 * 靠 8 处手工同步维持——任何一处漂移即静默破坏谱系(0.0290→0.0315
 * 静默陈旧先例, framework RSS 表 T9 重标定)。设计同 eigen_svd/
 * sandstone_eigen_common.h::sizing_probe 的 PR#302 检视 #14 收口。
 *
 * 本头文件刻意不 include 任何 Eigen 头: gemm_double_max_arm_body.cpp
 * 以 -DEigen=EigenSVEn 宏改名后按 tier 编译, 这里出现 Eigen:: 符号
 * 会被宏劫持; std 头(<cassert>/<complex>/<cstdint>/<type_traits>)不受
 * 影响。 */
#ifndef GEMM_SOLVE_PROBE_H
#define GEMM_SOLVE_PROBE_H

#include <cassert>
#include <complex>
#include <cstdint>
#include <type_traits>

#include "../eigen_splitmix_lineage.h"

#if defined(__aarch64__) && defined(__linux__)

namespace gemm_probe {

/* std::complex 判定(SVD 先例用 boost::is_complex; GEMM 文件群不引
 * boost, 自写特化保持依赖面不变)。 */
template <typename T> struct is_std_complex : std::false_type {};
template <typename T> struct is_std_complex<std::complex<T>> : std::true_type {};

/* 值域公式: 均匀随机 [-1,1), 档位常量见 eigen_splitmix_lineage.h。 */
inline double probe_value(std::uint64_t h)
{
    return (double)(h % sandstone_splitmix::kRangeSteps)
           / sandstone_splitmix::kRangeScale - 1.0;
}

/* 哈希流推进一拍: x = x*K3 + K1 再 xorshift32(splitmix 雪崩)。 */
inline std::uint64_t probe_stream(std::uint64_t x)
{
    x = x * sandstone_splitmix::kAvalanche + sandstone_splitmix::kGolden;
    x ^= x >> 32;
    return x;
}

/* 填充一对 n×n 方阵: 初始流 h = r*K1 ^ col*K2 经 xorshift/乘/xorshift
 * 终结化; 标量分支 a←h、b←h2(1 拍), complex 分支 a←(h,h2)、
 * b←(h3,h4)(3 拍)——与收口前的三个变体逐位一致(float 经
 * (Scalar) 先算 double 再收窄)。 */
template <typename Mat>
static void fill_pair(Mat &a, Mat &b)
{
    using Scalar = typename Mat::Scalar;
    constexpr bool kComplex = is_std_complex<Scalar>::value;
    /* 契约: a、b 为同维 n×n 方阵。本 header 是共享基础设施(8 个 wrapper
     * 使用方之外的未来调用方可直接 include), 尺寸不符时下方 b(r, col)
     * 写入即越界——release 构建(本仓 meson b_ndebug=if-release)下 Eigen
     * operator() 无边界检查, 静默堆越界写 UB。assert 在 debug 构建抓
     * 误用; release 下 NDEBUG 展开为 ((void)0), 探针机器码与计时不受
     * 影响(检视 #392-R1)。 */
    assert(a.cols() == a.rows() && b.rows() == a.rows()
           && b.cols() == a.rows());
    const int n = a.rows();
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * sandstone_splitmix::kGolden
                            ^ (std::uint64_t)col * sandstone_splitmix::kMix;
            h ^= h >> 32;
            h *= sandstone_splitmix::kAvalanche;
            h ^= h >> 32;
            if constexpr (kComplex) {
                std::uint64_t h2 = probe_stream(h);
                std::uint64_t h3 = probe_stream(h2);
                std::uint64_t h4 = probe_stream(h3);
                a(r, col) = Scalar(probe_value(h),  probe_value(h2));
                b(r, col) = Scalar(probe_value(h3), probe_value(h4));
            } else {
                std::uint64_t h2 = probe_stream(h);
                a(r, col) = (Scalar)probe_value(h);
                b(r, col) = (Scalar)probe_value(h2);
            }
        }
}

/* 联合求解探针: 构两张 Mat + 同谱系填充 + 一次乘法, 结果丢弃只为
 * 计时。调用方 sdc_joint_dim_slice 的 probe 参数是 C 函数指针
 * int solve_once(int), 各测试以一行转发 wrapper 提供该符号。 */
template <typename Mat>
static int solve_once(int n)
{
    Mat a(n, n), b(n, n), c;
    fill_pair(a, b);
    c = a * b;
    return 0;
}

}  // namespace gemm_probe

#endif  /* __aarch64__ && __linux__ */
#endif  /* GEMM_SOLVE_PROBE_H */

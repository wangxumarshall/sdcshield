/* eigen_splitmix_lineage.h — Eigen sizing 探针 splitmix 谱系的魔数/值域
 * 单一事实源 (issue #389, PR #392 检视 R1)。
 *
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 使用方(2 个家族, 填充结构不同故不共用填充模板):
 *   - eigen_gemm/gemm_solve_probe.h::fill_pair —— GEMM 联合求解探针,
 *     方阵对(a,b)标量双流/complex 四流填充;
 *   - eigen_svd/sandstone_eigen_common.h::sizing_probe —— SVD 联合求解
 *     探针, 单矩阵满秩谱系填充。
 * 谱系契约(检视 #51): 两家族的 probe 填充必须与各自 run() 的随机填充
 * (frandom_scale(2.0) - 1.0, 均匀随机 [-1,1))同谱系。收口前同一公式
 * 骨架在 Eigen dense sizing 探针内共 9 份逐字副本(GEMM 8 + SVD 1),
 * 跨家族的魔数/值域演化靠两处手工同步——正是收口要消灭的漂移模式
 * (0.0290→0.0315 静默陈旧先例)。魔数与值域档位在此单点定义后,
 * 演化只改本文件, 两家族编译期同步。
 * **范围边界(检视 #392-R3)**: 本头的单源声称仅覆盖上述 Eigen dense
 * sizing 探针谱系。kGolden/kMix 作为通用哈希常数在全仓另有大量独立
 * 使用(eigen_sparse.cpp 的非零元放置散列——其谱系契约对象是自身
 * run() 的 fill-in 结构, 仅借用同款 i*K1^j*K2 坐标混合且无 K3 终结化/
 * 值域档位; 以及 l2c/mmu/dep-chain 等测试的散列 tweak)——它们与本文
 * 件无共享谱系契约, 不随本头演化, 改本头不影响其行为(这是预期而
 * 非漂移)。勿将它们收口进本谱系(虚假耦合)。
 * 公式结构(初始流构造/流推进拍数/complex 分支形态)由各使用方按其
 * 填充结构持有, 不在本文件范围。
 *
 * 本头文件刻意无架构守门、不 include 任何 Eigen/boost 头: 两个使用方
 * 家族的源文件在 x86-64 参考构建中也编译, 这里只出现 std 常量——
 * constexpr 常量在两家族的 -O3 产物中折叠为同一立即数, 机器码与各自
 * 内联魔数写法逐指令一致(objdump 规范化对比实证)。 */
#ifndef EIGEN_SPLITMIX_LINEAGE_H
#define EIGEN_SPLITMIX_LINEAGE_H

#include <cstdint>

namespace sandstone_splitmix {

/* K1: splitmix64 黄金比例乘子(奇数), 亦作哈希流推进的加法常数。 */
inline constexpr std::uint64_t kGolden = 0x9E3779B97F4A7C15ull;
/* K2: 行/列坐标混合乘子(与 K1 互质, (r,c) 格点雪崩)。 */
inline constexpr std::uint64_t kMix = 0xC2B2AE3D27D4EB4Full;
/* K3: 终结化与流推进的雪崩乘子。 */
inline constexpr std::uint64_t kAvalanche = 0xD6E8FEB86659FD93ull;
/* 值域档位: 均匀随机 [-1,1) 千分之一步长——h % kRangeSteps /
 * kRangeScale - 1.0, 与 run() 期 frandom_scale(2.0) - 1.0 同谱系。
 * 不变量(检视 #392-R3): 均匀覆盖 [-1,1) 要求 steps == 2·scale
 * (h%steps/scale ∈ [0,2) 再减 1); 非对称编辑(只改其一)会静默破坏
 * 全部探针与 run() 的分布契约且无运行期信号, 以下编译期锁定。 */
inline constexpr unsigned kRangeSteps = 2000u;
inline constexpr double kRangeScale = 1000.0;
static_assert(kRangeSteps == 2u * static_cast<unsigned>(kRangeScale),
              "[-1,1) uniform coverage requires kRangeSteps == 2*kRangeScale");

}  // namespace sandstone_splitmix

#endif  /* EIGEN_SPLITMIX_LINEAGE_H */

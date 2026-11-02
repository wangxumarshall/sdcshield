# GEMM splitmix sizing 探针收口(issue #389)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `tests/cpu/eigen_gemm/` 中 8 份逐字复制的 `gemm_solve_once` splitmix sizing 探针收敛为 1 份共享 header 模板(参数化 Scalar),让『probe 与 run() fill_rand 同谱系』契约(检视 #51)的漂移在编译期不可能。

**Architecture:** 新建 `tests/cpu/eigen_gemm/gemm_solve_probe.h`(自守门 `#if defined(__aarch64__) && defined(__linux__)`,不 include 任何 Eigen 头,不依赖 boost):`namespace gemm_probe` 内 `fill_pair<Mat>`(splitmix 填充,谱系契约唯一载体)+ `solve_once<Mat>`(构造+填充+一次乘法,计时载荷)。8 个使用方(7 个 T9 文件 + `gemm_double_max_arm_body.cpp` 第 8 份)删除各自副本,替换为一行转发 wrapper——调用点(`sdc_joint_dim_slice(test, "mdim", &ja, gemm_solve_once)`,C 函数指针 `int solve_once(int)`)与 body 的 extern 链接(`gemm_vlNNN::solve_once`)零改动。

**Tech Stack:** C++20(`if constexpr`、模板)、vendored Eigen 5.0.0(仅测试侧,header 不依赖)、meson(无构建改动——header 用 quoted include 同目录引用)。

**Spec:** issue #389(audit 正文,Minor/可维护性)。设计先例:`tests/cpu/eigen_svd/sandstone_eigen_common.h::sizing_probe`(PR#302 检视 #14 同型收口,模板 + Scalar trait + `if constexpr`)。

## Global Constraints

- CLAUDE.md rule 5(x86-64 untouched):header 自守门 `__aarch64__ && __linux__`;8 个文件的 wrapper 全部位于各自既有 `#if defined(__aarch64__) && defined(__linux__)` 块内(x86 下无展开,x86 非回归 = 纯新增声明,如实声明)。
- CLAUDE.md rule 1(一个补丁一个单元):本 plan = 一个单元(探针收口),单 commit。T10 try/catch 降级模板**不在本单元内**(评估结论见任务 8 边界声明:不承载谱系契约,run 侧三种循环体结构差异真实,强行模板化伤可读性)。
- 行为逐位不变:重构前后探针填充与乘法逐位一致(任务 1 harness 证明);wrapper 保名保签名。
- header 不 include Eigen 头、不引 boost(`gemm_double_max_arm_body.cpp` 以 `-DEigen=EigenSVEn` 宏改名编译 4 个 VL tier,header 内写任何 `Eigen::` 符号都会被宏劫持;7 个 GEMM 文件当前也不引 boost,保持依赖面不变)。
- 提交 DCO:`git commit -s`(`Signed-off-by: wangxu <wangxumarshall@qq.com>` 最后一行);只推 `fix/issue-389`,绝不推 main、绝不 merge、绝不 resolve。
- 仓库 CI 克隆内作业;结束时 `git checkout main && git status` clean。

## 事实基线(已侦查确认,执行者无需重查)

- 8 份副本分布(issue 列 7 份 + 第 8 份「源头」):
  1. `double14.cpp:40-58`(double,双流)
  2. `gemm_double_dynamic_square.cpp:39-57`(double,与 1 逐字同)
  3. `gemm_double_dynamic_square_dump.cpp:47-65`(double,同)
  4. `gemm_float_dynamic_square.cpp:41-59`(float,`(float)((double)…)` 双层转型)
  5. `gemm_float_dynamic_square_dump.cpp:41-59`(float,同)
  6. `gemm_cdouble_dynamic_square.cpp:42-70`(complex<double>,四流 a.re/a.im/b.re/b.im)
  7. `gemm_cdouble_dynamic_square_dump.cpp:44-75`(complex<double>,同)
  8. `gemm_double_max_arm_body.cpp:120-138`(double,extern `GEMM_TEST_NAMESPACE::solve_once`,dispatcher `gemm_double_max_arm.cpp:110-124` 按 tier 链接调用——其余 7 份注释全部引用它为 T6 谱系源头;不收口它则仍有 2 份定义,「编译期不可能漂移」不成立)
- 变体统一模型(逐位等价已推导):初始流 `h = r*0x9E3779B97F4A7C15 ^ col*0xC2B2AE3D27D4EB4F; h^=h>>32; h*=0xD6E8FEB86659FD93; h^=h>>32;` 推进一拍 `x = x*0xD6E8FEB86659FD93 + 0x9E3779B97F4A7C15; x^=x>>32;` 值域 `(double)(h % 2000u)/1000.0 - 1.0`(均匀 [-1,1))。标量:a←h,b←h2(1 拍);complex:a←(h,h2),b←(h3,h4)(3 拍)。`(Scalar)value` 对 double 恒等、对 float 是先算 double 再收窄(与旧 `(float)((double)…)` 逐位同)、对 `std::complex<double>` 用 `Scalar(v1,v2)`。
- `sdc_joint_dim_slice(struct test*, const char *knob, const struct sdc_joint_args *, int solve_once(int))`(framework/sdc_stress_config.h:960)。
- 本机(无 SVE Kunpeng 920):7 个非 SVE GEMM 测试原生可跑;`eigen_gemm_double_max_arm` 本机只可验 SKIP 路径。
  > **勘误(2026-11-02, PR #392 检视 R2)**: 本条主机事实有误——实机为 cortex
  > x3b(family-model 0x48-d02), SVE-256(`--dump-cpu-info` 实测: `SVE vector
  > length: 256 bits`, SVE2/SME 无)。`eigen_gemm_double_max_arm` 本机真机可跑
  > 且 **exit: pass**(vl256 默认档; `SDCSHIELD_ARM64_SPEC_MAX_VL=128` 降档跑
  > vl128 亦 pass); vl512/1024 已在 gem5-fi(`/home/sdc/wangxu/gem5-fi`, SE
  > `arm_chaos.py`, 真同源 dual kernel 直接 include 收口后 header)以新旧
  > checksum 逐位一致实证(见 PR 描述「SVE 4 档运行矩阵」)。执行时按实机
  > 能力走了真机+gem5 路径, 本 plan 的 SKIP 路径预设未被执行。

---

### Task 1: 基线分支 + 逐位一致性 harness(验证前置)

**Files:**
- Create: `/tmp/gemm_probe_bitcmp.cpp`(harness,不进仓库)
- Create: `/tmp/old_probe_baseline.txt`(origin/main 探针原文快照,比对依据)

**Interfaces:**
- Produces: `/tmp/gemm_probe_bitcmp`(可执行),供 Task 3 在 header 落地后运行;`/tmp/old_probe_baseline.txt` 供审计取证。

- [ ] **Step 1: 从 origin/main 建分支**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
git checkout -B fix/issue-389 origin/main
git branch --show-current   # 期望输出: fix/issue-389
```

- [ ] **Step 2: 快照 origin/main 的 8 份探针原文(取证基线)**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
{ for f in double14.cpp gemm_double_dynamic_square.cpp gemm_double_dynamic_square_dump.cpp \
           gemm_float_dynamic_square.cpp gemm_float_dynamic_square_dump.cpp \
           gemm_cdouble_dynamic_square.cpp gemm_cdouble_dynamic_square_dump.cpp; do
    git show origin/main:tests/cpu/eigen_gemm/$f | sed -n '/^static int gemm_solve_once/,/^}/p';
  done
  git show origin/main:tests/cpu/eigen_gemm/gemm_double_max_arm_body.cpp | sed -n '/^int solve_once/,/^}/p'
} > /tmp/old_probe_baseline.txt
wc -l /tmp/old_probe_baseline.txt   # 非零行数
```

- [ ] **Step 3: 写 harness(旧实现 = origin/main 探针函数体逐字,包装仅暴露矩阵;新实现 = Task 2 的 header 模板)**

```cpp
// /tmp/gemm_probe_bitcmp.cpp — issue #389 重构逐位一致性 harness。
// 旧侧: origin/main 各探针的填充循环【逐字】(魔数/运算序/转型/赋值一字不差),
//       仅把矩阵构造提到参数、把乘积作为返回, 以便观察输出。
// 新侧: tests/cpu/eigen_gemm/gemm_solve_probe.h 的 gemm_probe::fill_pair/solve_once。
// 判据: 三种 Scalar × n∈{1,33,64} 下, a/b 填充逐位 memcmp 全等, 乘积逐位全等。
#include <cstdio>
#include <cstring>
#include <complex>
#include <cstdint>
#include <tuple>
#include <Eigen/Core>
#include "tests/cpu/eigen_gemm/gemm_solve_probe.h"

typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> MD;
typedef Eigen::Matrix<float,  Eigen::Dynamic, Eigen::Dynamic> MF;
typedef Eigen::Matrix<std::complex<double>, Eigen::Dynamic, Eigen::Dynamic> MC;

// ---- 旧 double (verbatim loop from origin/main double14.cpp:40-58) ----
static std::tuple<MD,MD,MD> old_probe_d(int n)
{
    MD a(n, n), b(n, n); MD c;
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (double)(h % 2000u) / 1000.0 - 1.0;
            b(r, col) = (double)(h2 % 2000u) / 1000.0 - 1.0;
        }
    c = a * b;
    return {a, b, c};
}
// ---- 旧 float (verbatim loop from origin/main gemm_float_dynamic_square.cpp:41-59) ----
static std::tuple<MF,MF,MF> old_probe_f(int n)
{
    MF a(n, n), b(n, n); MF c;
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (float)((double)(h % 2000u) / 1000.0 - 1.0);
            b(r, col) = (float)((double)(h2 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
    return {a, b, c};
}
// ---- 旧 complex (verbatim loop from origin/main gemm_cdouble_dynamic_square.cpp:42-70) ----
static std::tuple<MC,MC,MC> old_probe_c(int n)
{
    MC a(n, n), b(n, n); MC c;
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            std::uint64_t h3 = h2 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h3 ^= h3 >> 32;
            std::uint64_t h4 = h3 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h4 ^= h4 >> 32;
            a(r, col) = std::complex<double>(
                (double)(h % 2000u) / 1000.0 - 1.0,
                (double)(h2 % 2000u) / 1000.0 - 1.0);
            b(r, col) = std::complex<double>(
                (double)(h3 % 2000u) / 1000.0 - 1.0,
                (double)(h4 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
    return {a, b, c};
}

// ---- 新侧: header 模板的同构观察窗口 ----
template <typename Mat>
static std::tuple<Mat,Mat,Mat> new_probe(int n)
{
    Mat a(n, n), b(n, n); Mat c;
    gemm_probe::fill_pair(a, b);
    c = a * b;
    return {a, b, c};
}

static int fails = 0;
template <typename Mat>
static void cmp_case(const char *tag, int n)
{
    auto [oa, ob, oc] = old_probe/* 按 tag 特化 */(n);
    (void)oa; (void)ob; (void)oc;
}
// (cmp_case 用三个显式变体实现, 见 main; 模板分发会削弱可读性, 保留显式)

template <typename Mat>
static void bytes_diff(const char *tag, const char *what, const Mat &x, const Mat &y, int n)
{
    size_t len = (size_t)x.rows() * x.cols() * sizeof(typename Mat::Scalar);
    if (memcmp(x.data(), y.data(), len) != 0) {
        ++fails;
        std::printf("FAIL %s n=%d %s (%zu bytes)\n", tag, n, what, len);
    }
}

int main()
{
    const int ns[] = {1, 33, 64};
    for (int n : ns) {
        { auto [oa,ob,oc] = old_probe_d(n); auto [na,nb,nc] = new_probe<MD>(n);
          bytes_diff("double", "a", oa, na, n); bytes_diff("double", "b", ob, nb, n);
          bytes_diff("double", "c", oc, nc, n);
          if (!fails) std::printf("PASS double n=%d (a,b,c bit-identical)\n", n); }
        { auto [oa,ob,oc] = old_probe_f(n); auto [na,nb,nc] = new_probe<MF>(n);
          bytes_diff("float",  "a", oa, na, n); bytes_diff("float",  "b", ob, nb, n);
          bytes_diff("float",  "c", oc, nc, n);
          if (!fails) std::printf("PASS float  n=%d (a,b,c bit-identical)\n", n); }
        { auto [oa,ob,oc] = old_probe_c(n); auto [na,nb,nc] = new_probe<MC>(n);
          bytes_diff("complex","a", oa, na, n); bytes_diff("complex","b", ob, nb, n);
          bytes_diff("complex","c", oc, nc, n);
          if (!fails) std::printf("PASS complex n=%d (a,b,c bit-identical)\n", n); }
    }
    std::printf(fails ? "RESULT: %d FAILURES\n" : "RESULT: ALL BIT-IDENTICAL\n", fails);
    return fails ? 1 : 0;
}
```
(实现时删掉上文 `cmp_case` 占位骨架——它是说明分发改显式的注记,不是交付代码;harness 最终只含 `old_probe_*` × 3、`new_probe` 模板、`bytes_diff`、`main`。)

- [ ] **Step 4: 编译 harness(在 header 尚未创建时,期望编译失败)**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
g++ -std=c++20 -O2 -I. /tmp/gemm_probe_bitcmp.cpp -o /tmp/gemm_probe_bitcmp
```
Expected: FAIL——`gemm_solve_probe.h: No such file or directory`(确认 harness 真实依赖 Task 2 的交付物,而非空转)。

### Task 2: 新建共享 header `gemm_solve_probe.h`

**Files:**
- Create: `tests/cpu/eigen_gemm/gemm_solve_probe.h`

**Interfaces:**
- Produces: `namespace gemm_probe { template <typename Mat> static void fill_pair(Mat &a, Mat &b); template <typename Mat> static int solve_once(int n); }`(整个 namespace 位于 `#if defined(__aarch64__) && defined(__linux__)` 内)。
- 消费方(8 个文件)以 `#include "gemm_solve_probe.h"` 引用;不依赖任何 Eigen/boost 头(`Mat::Scalar` 由调用方的 Mat 提供)。

- [ ] **Step 1: 写 header(全文如下)**

```cpp
/* gemm_solve_probe.h — GEMM 联合求解 sizing 探针的共享实现 (issue #389)。
 *
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * 谱系契约(检视 #51)在此单点承载: probe 填充必须与各测试 run() 的
 * fill_rand(frandom_scale(2.0) - 1.0, 均匀随机 [-1,1))同谱系——splitmix
 * 确定性哈希满足该契约, 且不依赖 init aux 线程上未定义的 per-thread RNG
 * 状态。GEMM 计时对数值不敏感(无 SVD 式 deflation 路径), 谱系由分布
 * 一致性满足; 填充 O(n²) 相对乘法 O(n³) 已计入探针计时(保守方向)。
 * 改这里的值域/结构必须与各 run() 的 fill_rand 同步演化。
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
 * 会被宏劫持; std 头(<complex>/<cstdint>/<type_traits>)不受影响。 */
#ifndef GEMM_SOLVE_PROBE_H
#define GEMM_SOLVE_PROBE_H

#include <complex>
#include <cstdint>
#include <type_traits>

#if defined(__aarch64__) && defined(__linux__)

namespace gemm_probe {

/* std::complex 判定(SVD 先例用 boost::is_complex; GEMM 文件群不引
 * boost, 自写 6 行特化保持依赖面不变)。 */
template <typename T> struct is_std_complex : std::false_type {};
template <typename T> struct is_std_complex<std::complex<T>> : std::true_type {};

/* 值域公式: 均匀随机 [-1,1), 2000 档千分之一步长。 */
inline double probe_value(std::uint64_t h)
{
    return (double)(h % 2000u) / 1000.0 - 1.0;
}

/* 哈希流推进一拍: x = x*K3 + K1 再 xorshift32(splitmix 雪崩)。 */
inline std::uint64_t probe_stream(std::uint64_t x)
{
    x = x * 0xD6E8FEB86659FD93ull + 0x9E3779B97F4A7C15ull;
    x ^= x >> 32;
    return x;
}

/* 填充一对 n×n 矩阵: 初始流 h = r*K1 ^ col*K2 经 xorshift/乘/xorshift
 * 终结化; 标量分支 a←h、b←h2(1 拍), complex 分支 a←(h,h2)、
 * b←(h3,h4)(3 拍)——与收口前的三个变体逐位一致。 */
template <typename Mat>
static void fill_pair(Mat &a, Mat &b)
{
    using Scalar = typename Mat::Scalar;
    constexpr bool kComplex = is_std_complex<Scalar>::value;
    const int n = a.rows();
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
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
```

- [ ] **Step 2: 编译 harness 验证逐位一致**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
g++ -std=c++20 -O2 -I. /tmp/gemm_probe_bitcmp.cpp -o /tmp/gemm_probe_bitcmp && /tmp/gemm_probe_bitcmp
```
Expected: `PASS double/float/complex n=1/33/64` × 9 行 + `RESULT: ALL BIT-IDENTICAL`,exit 0。若任一 FAIL:停,回 header 检查流推进/值域公式与旧文逐字差异(不得"修 harness 凑过")。

### Task 3: 7 个 T9 文件替换副本为一行 wrapper

**Files:**
- Modify: `tests/cpu/eigen_gemm/double14.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square_dump.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_float_dynamic_square.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_float_dynamic_square_dump.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square_dump.cpp`

**Interfaces:**
- Consumes: Task 2 的 `gemm_probe::solve_once<Mat>`。
- Produces: 各文件保留同签名 `static int gemm_solve_once(int n)`(文件级 `#if defined(__aarch64__) && defined(__linux__)` 块内),`gemm_mdim`/`gemm_k_dim` 的调用点零改动。

- [ ] **Step 1: 每个文件——include 区(`#include "sdc_stress_config.h"` 之后)加一行**

```cpp
#include "gemm_solve_probe.h"
```

- [ ] **Step 2: 每个文件——将原 `#if defined(__aarch64__) && defined(__linux__)` 块内的 `gemm_solve_once` 全函数体(含原契约注释)替换为**

```cpp
#if defined(__aarch64__) && defined(__linux__)
/* 联合求解 sizing 探针: 谱系契约(检视 #51)单点收口至
 * gemm_solve_probe.h(issue #389)——填充唯一定义, 漂移编译期不可能。 */
static int gemm_solve_once(int n)
{
    return gemm_probe::solve_once<Mat>(n);
}
#endif
```
(float 文件注释补一句「(float 经 (Scalar) 收窄, 与原 (float)((double)…) 逐位同)」;cdouble 文件补「(complex 四流 re/im)」。)

- [ ] **Step 3: 中间验证——全量构建零新警告**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
./scripts/build.sh 2>&1 | tail -20
```
Expected: BUILD 成功,无新增 warning(尤其 -Wunused-function——wrapper 在 #if 内,x86 不展开)。

### Task 4: 第 8 份(body)收口

**Files:**
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm_body.cpp`

**Interfaces:**
- Consumes: `gemm_probe::solve_once<Mat>`(body 内 `Mat` 是 `GEMM_TEST_NAMESPACE` 中的 `Matrix<double, Dynamic, Dynamic>`——宏改名 Eigen 下模板只取 `Mat::Scalar`,无 Eigen:: 符号依赖)。
- Produces: extern 符号 `GEMM_TEST_NAMESPACE::solve_once(int)` 保名保签名(dispatcher `gemm_double_max_arm.cpp:110-124` 零改动)。

- [ ] **Step 1: include 区(`#include "sdc_stress_config.h"` 之后)加**

```cpp
#include "gemm_solve_probe.h"
```

- [ ] **Step 2: 将 `solve_once` 全函数体(120-138 行,含原 T6 契约长注)替换为**

```cpp
/* 联合求解探针 (T6): 计时载荷——谱系契约(检视 #51)与填充实现已单点
 * 收口至 gemm_solve_probe.h(issue #389, 8→1); 本函数保留 extern 符号
 * (dispatcher 按 tier 链接)。执行线程语义见上方原注: 在 test_init 的
 * aux 线程上被 sdc_joint_dim_slice 逐档调用, 框架已把控制线程 pin 到
 * 选定档, init 线程继承该 VL。 */
int solve_once(int dim)
{
    return gemm_probe::solve_once<Mat>(dim);
}
```

- [ ] **Step 3: 构建 4 个 VL tier 的 static_library(body 的编译面)**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci/builddir
ninja tests_eigen_gemm_vl128 tests_eigen_gemm_vl256 tests_eigen_gemm_vl512 tests_eigen_gemm_vl1024 2>&1 | tail -5
```
Expected: 4 库全部构建成功、零警告(`-msve-vector-bits=TIER` × `-DEigen=EigenSVEn` 下模板具现化 × 4)。若目标名不存在,先 `ninja -t targets | grep eigen_gemm_vl` 查实名。

- [ ] **Step 4: 指令级等价取证——objdump 对比 wrapper 与基线**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
# 基线构建走独立 worktree(检视 #392-R4 勘误: 原稿此处为 `git stash`——
# 时序上收口改动尚未提交, stash 回退 8 个 wrapper 与 SVD 头的修改却留下
# 未跟踪的新 header(分裂树), 下游 Task 5/6 会在旧码上构建回归并提交出
# 孤儿 header 的空壳 PR; 且全文档无 stash pop。勿在工作树上 stash, 一律
# worktree 对比——工具链与 -O 级由同一 builddir 配置保证):
git worktree add /tmp/gemm-main-baseline origin/main 2>/dev/null || true
# (在 baseline 内 meson/ninja 构建 vl128 库后:)
objdump -d <baseline>/builddir/tests_eigen_gemm_vl128/libtests_eigen_gemm_vl128.a.p/gemm_double_max_arm_body.cpp.o \
        --disassemble='gemm_vl128::solve_once' > /tmp/solve_once_old.asm
objdump -d builddir/tests_eigen_gemm_vl128/libtests_eigen_gemm_vl128.a.p/gemm_double_max_arm_body.cpp.o \
        --disassemble='gemm_vl128::solve_once' > /tmp/solve_once_new.asm
diff /tmp/solve_once_old.asm /tmp/solve_once_new.asm && echo "INSTRUCTION-IDENTICAL"
git worktree remove /tmp/gemm-main-baseline --force 2>/dev/null || true
```
Expected: 二者指令序列一致(或仅符号重定位偏移差异、逻辑零差异)。若逻辑有差异:分析是否仅内联边界造成的寄存器分配差——**报告里必须如实引用 diff 原文**;若逐指令一致,这是强于 gem5-fi 单次运行的等价证据。此步失败不阻塞提交(harness 已证逐位一致),但报告必须如实呈现。

### Task 5: 回归验证(CLAUDE.md rule 2)

**Files:** 无新改动(纯验证)。

- [ ] **Step 1: 全量构建零新警告**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci && ./scripts/build.sh 2>&1 | tail -5
```
Expected: clean,zero new warnings。

- [ ] **Step 2: 7 个受影响测试逐个回归(受影响行为跑通)**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
for t in eigen_gemm_double14 eigen_gemm_double_dynamic_square \
         eigen_gemm_double_dynamic_square_dump eigen_gemm_float_dynamic_square \
         eigen_gemm_float_dynamic_square_dump eigen_gemm_cdouble_dynamic_square \
         eigen_gemm_cdouble_dynamic_square_dump; do
  ./builddir/sdcshield -e $t -t 2000 -n 1 2>&1 | tail -2
done
```
Expected: 每测试 `exit: pass`(引用真实输出进报告)。

- [ ] **Step 3: SVE 测试 SKIP 路径诚实验证 + gem5-fi 边界声明**

```bash
./builddir/sdcshield -e eigen_gemm_double_max_arm -t 2000 -n 1 2>&1 | tail -3
```
Expected: 本机无 SVE → 诚实 SKIP(reason 不变)。**gem5-fi 规则评估写进报告**:本次改动对 body 是纯结构转发(填充逻辑与逐位一致性已由 harness + Task 4 Step 4 的指令级对比证明,wrapper 不含任何 SVE 语义/SVE intrinsic,模板具现化与 VL 无关);若 Task 4 Step 4 逐指令一致则运行时行为等价有静态证明,gem5-fi 侧不再引入新信息——按规则如实声明该边界与依据,不冒充跑过。

> **勘误(2026-11-02, PR #392 检视 R2)**: 本 Step 的预设与上同源有误——实机
> SVE-256, 该命令真机 `exit: pass`(非 SKIP); 且 gem5-fi 存在
> (`/home/sdc/wangxu/gem5-fi`)。实际执行超出本 Step 预设: vl128/256 真机
> pass + vl512/1024 gem5-fi 真同源 dual(直接 include 收口后 header)与收口前
> mirror 的 checksum 逐位一致(见 PR 描述)。本 Step 原文保留以记录计划时
> 认知, 不作为验证边界依据。

- [ ] **Step 4: x86-64 非回归(按检视)**

```bash
git diff origin/main --stat
git diff origin/main -- tests/cpu/eigen_gemm/ | grep -E "^\+.*#if" 
```
Expected(按检视声明): 全部改动位于 `#if defined(__aarch64__) && defined(__linux__)` 守门内 + include 行(header 自守门,x86 展开为空);x86 可见行为零变化,纯新增声明。

### Task 6: 提交、推送、PR、报告

- [ ] **Step 1: 单 commit(DCO)**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
git add tests/cpu/eigen_gemm/
git status   # 确认仅 9 个文件(header 新增 + 8 改动), plan 文档另行评估(见下)
git commit -s -m "refactor(eigen_gemm): splitmix sizing 探针 8 份逐字副本收口至共享 header (issue #389)

gemm_solve_once 探针(0x9E3779B97F4A7C15/0xC2B2AE3D27D4EB4F/
0xD6E8FEB86659FD93 魔数 + 值域公式)曾逐字复制 8 份: issue #389
点名的 7 个 T9 文件 + gemm_double_max_arm_body.cpp(T6 谱系源头,
其余 7 份注释的引用锚)。谱系契约(检视 #51: probe 与 run()
fill_rand 同谱系)靠 8 处手工同步维持, 任何一处漂移即静默破坏。

收口: 新增 tests/cpu/eigen_gemm/gemm_solve_probe.h(参数化 Mat/
Scalar, if constexpr 区分标量双流/complex 四流, 自守门
__aarch64__&&__linux__, 不依赖 Eigen/boost 头), 8 个使用方删除
副本改一行转发 wrapper——sdc_joint_dim_slice 的 C 函数指针调用点
与 body 的 extern gemm_vlNNN::solve_once 链接符号零改动。

验证: 逐位一致性 harness(旧实现逐字提取 vs 新模板, double/
float/complex × n=1/33/64, a/b/c 全部 bit-identical); 全量构建
零新警告; 4 个 VL tier static_library 构建通过; 7 个受影响测试
-e <t> -t 2000 -n 1 全 exit: pass; x86-64 纯新增声明(改动全在
arch 守门内)。设计同 sandstone_eigen_common.h::sizing_probe
(PR#302 检视 #14)先例。"
```
(plan 文档 `docs/superpowers/plans/2026-11-01-gemm-probe-collapse.md` 属过程工件,与功能单元分开评估是否入库;若仓库惯例保留 plan,则单独 docs commit——**不捆绑**进本功能单元。)

- [ ] **Step 2: 推送 fix/issue-389(绝不推 main)**

```bash
git push -u origin fix/issue-389
```

- [ ] **Step 3: 开 PR(若 auto-pr.yml 抢先已建,则 gh pr edit 兜底改写)**

```bash
gh pr create --base main --head fix/issue-389 \
  --title "fix: gemm splitmix sizing 探针 8 份副本收口至共享 header (#389)" \
  --body "Closes #389 + 报告(根因/方案、改动点、验证证据)"
```
PR body 必含: `Closes #389`、根因(8 处手工同步义务)、改动清单(9 文件)、验证证据(harness 输出原文、构建输出、7 测试 exit: pass 原文、objdump 结论、x86 声明、gem5-fi 边界声明)。

- [ ] **Step 4: 报告贴 issue #389(不 resolve)**

```bash
gh issue comment 389 --body "<同 PR 报告>"
```
内容含: 方案与 issue 建议一致(共享 header 参数化 Scalar)、第 8 份(body)的发现与收口依据、T10 try/catch 模板的处置评估(不承载谱系契约、run 侧三种循环体结构差异真实、本单元不动,理由与依据)、验证证据、PR 链接。

- [ ] **Step 5: 收尾回干净 main**

```bash
git checkout main && git status   # 期望 clean
```

## Self-Review 结论

- 覆盖: issue 验收建议(「共享 header helper 参数化 Scalar,7 份收敛 1 份,编译期不可能漂移」)→ Task 2+3;第 8 份(收口完整性)→ Task 4;验证(rule 2)→ Task 1/5;报告契约 → Task 6。T10 模板处置在 Task 6 Step 4 报告中有明确评估输出(非遗漏)。
- 类型一致: `gemm_probe::solve_once<Mat>(int n) -> int`、`fill_pair(Mat&, Mat&)` 与 8 个消费方签名核对一致;`sdc_joint_dim_slice(..., int solve_once(int))` 函数指针匹配。
- 占位符: Task 1 Step 3 的 `cmp_case` 骨架在正文标注「实现时删除」并给出最终结构,其余步骤均为可执行实码。


## 附录: 逐位一致性 harness 最终版归档(检视 #392-R3 补录, 2026-11-02)

> 检视意见: 本 plan 的 Task 1 计划版 harness(含 `cmp_case` 自 declared 骨架)与
> PR 描述引用的实际输出(3 行分组格式)不是同一程序, 实际执行的 harness 为
> /tmp 一次性产物且已丢失——bit-identical 证据链无法从仓库工件复现。本附录
> 归档实际执行的 harness 最终版源码与 2026-11-02 复原重跑的真实输出(新实现
> 路径为收口后 header 经 R1-R3 全部处置的当前状态, 旧实现逐字提取自
> 66d83e8d 收口前副本, 提取源文件在源码头注中标明)。

编译与运行命令(2026-11-02 实测):

```
g++ -O3 -std=c++20 -I third-party/eigen5 -I tests/cpu/eigen_gemm \
    -o /tmp/gemm_probe_bitcmp /tmp/gemm_probe_bitcmp.cpp
/tmp/gemm_probe_bitcmp
```

实际输出(2026-11-02 复原重跑, 与 PR 描述引用逐字一致):

```
PASS n=1: double/float/complex a,b,c all bit-identical
PASS n=33: double/float/complex a,b,c all bit-identical
PASS n=64: double/float/complex a,b,c all bit-identical
RESULT: ALL BIT-IDENTICAL (exit 0)
```

harness 最终版源码:

```c++
/* gemm_probe_bitcmp — 逐位一致性 harness (PR #392 验证工件)。
 * 2026-11-02 复原重跑(检视 #392-R3: 原一次性 /tmp 工件已失, 按实际
 * 输出格式复原, 本源码随 plan 文档归档)。
 * 旧实现逐字提取自 66d83e8d(收口前 merge-base):
 *   double  <- tests/cpu/eigen_gemm/gemm_double_dynamic_square.cpp
 *   float   <- tests/cpu/eigen_gemm/gemm_float_dynamic_square.cpp
 *   cdouble <- tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square.cpp
 * 新实现 include 收口后的 tests/cpu/eigen_gemm/gemm_solve_probe.h。
 * 对比 n∈{1,33,64} × {double,float,complex} 的 a/b/c 逐位(memcmp)。 */
#include <Eigen/Core>

#include <complex>
#include <cstdint>
#include <cstdio>
#include <cstring>

typedef Eigen::Matrix<double, Eigen::Dynamic, Eigen::Dynamic> DMat;
typedef Eigen::Matrix<float, Eigen::Dynamic, Eigen::Dynamic> FMat;
typedef Eigen::Matrix<std::complex<double>, Eigen::Dynamic, Eigen::Dynamic> CMat;

#include "gemm_solve_probe.h"

/* ---- 旧实现(逐字提取, 仅变量名 a/b/c/n 适配 harness 签名) ---- */
static void old_solve(DMat &a, DMat &b, DMat &c, int n)
{
    a.resize(n, n); b.resize(n, n);
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (double)(h % 2000u) / 1000.0 - 1.0;
            b(r, col) = (double)(h2 % 2000u) / 1000.0 - 1.0;
        }
    c = a * b;
}

static void old_solve(FMat &a, FMat &b, FMat &c, int n)
{
    a.resize(n, n); b.resize(n, n);
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (float)((double)(h % 2000u) / 1000.0 - 1.0);
            b(r, col) = (float)((double)(h2 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
}

static void old_solve(CMat &a, CMat &b, CMat &c, int n)
{
    a.resize(n, n); b.resize(n, n);
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            std::uint64_t h3 = h2 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h3 ^= h3 >> 32;
            std::uint64_t h4 = h3 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h4 ^= h4 >> 32;
            a(r, col) = std::complex<double>(
                (double)(h % 2000u) / 1000.0 - 1.0,
                (double)(h2 % 2000u) / 1000.0 - 1.0);
            b(r, col) = std::complex<double>(
                (double)(h3 % 2000u) / 1000.0 - 1.0,
                (double)(h4 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
}

/* ---- 新实现(收口后 header, 经 solve_once 同路径) ---- */
template <typename Mat>
static void new_solve(Mat &a, Mat &b, Mat &c, int n)
{
    a.resize(n, n); b.resize(n, n);
    gemm_probe::fill_pair(a, b);
    c = a * b;
}

template <typename Mat>
static bool cmp_mat(const Mat &x, const Mat &y)
{
    return x.rows() == y.rows() && x.cols() == y.cols()
        && std::memcmp(x.data(), y.data(),
                       (size_t)x.rows() * x.cols() * sizeof(typename Mat::Scalar)) == 0;
}

template <typename Mat>
static bool run_one(int n)
{
    Mat a1, b1, c1, a2, b2, c2;
    old_solve(a1, b1, c1, n);
    new_solve(a2, b2, c2, n);
    return cmp_mat(a1, a2) && cmp_mat(b1, b2) && cmp_mat(c1, c2);
}

int main()
{
    int fails = 0;
    for (int n : {1, 33, 64}) {
        bool d = run_one<DMat>(n), f = run_one<FMat>(n), c = run_one<CMat>(n);
        if (d && f && c)
            std::printf("PASS n=%d: double/float/complex a,b,c all bit-identical\n", n);
        else {
            ++fails;
            std::printf("FAIL n=%d: double=%d float=%d complex=%d\n", n, d, f, c);
        }
    }
    if (!fails)
        std::printf("RESULT: ALL BIT-IDENTICAL (exit 0)\n");
    return fails ? 1 : 0;
}
```

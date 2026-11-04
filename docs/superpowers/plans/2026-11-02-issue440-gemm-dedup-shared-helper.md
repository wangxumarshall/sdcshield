# issue #440: eigen_gemm 7 文件 T10 降档块与联合求解探针去重 计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 eigen_gemm 目录 7 个文件中逐字复制的 T10 init 降档块(~25 行)与
联合求解探针 `gemm_solve_once` 收敛到共享头 `sandstone_gemm_common.h`,
行为逐位不变,消除 7 处同步的维护代价(PR#302 检视 #25 前科)。

**Architecture:** 仿 `tests/cpu/eigen_svd/sandstone_eigen_common.h` 先例,在
`tests/cpu/eigen_gemm/` 新增共享头(同目录 quote-include,meson 零改动)。
init 降档块 → 模板函数 `sdc_gemm_init_shared<Data>(test, d, mdim)`(跨架构,
替换无守卫代码);探针 → `sdc_gemm_solve_once<Scalar>(n)` + splitmix 流对象
(保留各文件原有 `#if defined(__aarch64__) && defined(__linux__)` 守卫)。
数值/求值序逐位保持:complex 元素的 re/im 取流用命名局部量测序(规避
C++17 函数实参 indeterminately-sequenced 陷阱)。

**Tech Stack:** C++20 / Eigen(vendored 5.0.0+)/ meson+ninja;无新依赖。

**Spec:** issue #440 正文(audit 发现;grep 证据已在 2026-11-02 会话复现:
7 份降档块代码体 md5 全同 `cb3827fb…`,探针聚成 double×3/float×2/cdouble×2)。

## Global Constraints

- 一个补丁一个单元:本修复 = 一个单元 → 单个 fix commit;计划文档与勾选完成
  各自单独 docs(plan) commit(issue #374 先例 b2d337de/d8d6cb0e)。
- DCO:`git commit -s`,最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,
  其后无内容,无 Co-Authored-By。
- 只推 `fix/issue-440` 分支(从 origin/main 起),绝不推 main、绝不 merge。
- x86-64 非回归(rule 5):共享头对 x86 是新增 include,探针段与原文件同款
  arch 守卫(x86 不编译);`sdc_gemm_init_shared` 替换的是两架构共用的无守卫
  代码,文本等价。不改动任何 `#ifdef __x86_64__`。
- 构建零新警告(`./scripts/build.sh`)。
- 回归:7 个受影响测试 `-e <id> -t 2000 -n 1` → `exit: pass`。
- gem5-fi 不适用:本改动无芯片能力依赖(纯重构,本机 aarch64 可完整真机
  验证全部路径)。
- 范围外(如实声明,不加抽象):`gemm_mdim` 各文件参数化差异(knob/env 名、
  clamp、fam、legacy fallback)是真参数非复制漂移;`gemm_double_max_arm` 系
  init 不持有矩阵、无降档点;`eigen_svd_bidiag_sve.cpp:475-497` 是裸
  aligned_alloc/null 检查口径(lo=64,free 重分配),与 Eigen Mat 异常口径
  不同构,强行共享需分配策略抽象,收益 <6 行。

---

### Task 1: 基线采集(A/B 的 A 侧)

**Files:** 无仓库改动;产物存 `/tmp/issue440_baseline/`。

**Interfaces:**
- Produces: 7 个测试在 origin/main 构建下的确定性日志
  (`normal_<id>.log`、`ulimit_<id>.log`),供 Task 4 逐字节比对。

- [x] **Step 1: 确认分支与构建基线干净**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
git checkout -B fix/issue-440 origin/main
git status --short   # 须 clean
```

- [x] **Step 2: 构建 origin/main 基线**

Run: `./scripts/build.sh 2>&1 | tee /tmp/issue440_baseline/build.log`
Expected: 构建成功;记录警告基线(Task 4 比对"零**新**警告")。

- [x] **Step 3: 采集 7 测试的确定性日志(显式 mdim,绕过探针计时抖动)**

```bash
mkdir -p /tmp/issue440_baseline && cd /home/sdc/wangxu/operator/sdcshield-autoci
for t in eigen_gemm_double14 eigen_gemm_double_dynamic_square \
         eigen_gemm_double_dynamic_square_dump eigen_gemm_float_dynamic_square \
         eigen_gemm_float_dynamic_square_dump eigen_gemm_cdouble_dynamic_square \
         eigen_gemm_cdouble_dynamic_square_dump; do
  ./builddir/sdcshield -e $t -t 2000 -n 1 -O $t.mdim=64 \
      > /tmp/issue440_baseline/normal_$t.log 2>&1
  echo "$t: exit=$?"
done
```
Expected: 全部 `exit: pass`。显式 mdim=64 → 走 knob 早退,init/run 全确定
  (框架截获 srand,同命令行同序列),日志可作为逐字节基准。

- [x] **Step 4: 采集 init 降档路径日志(ulimit -v 制造 bad_alloc)**

```bash
for t in eigen_gemm_double14 eigen_gemm_double_dynamic_square \
         eigen_gemm_double_dynamic_square_dump eigen_gemm_float_dynamic_square \
         eigen_gemm_float_dynamic_square_dump eigen_gemm_cdouble_dynamic_square \
         eigen_gemm_cdouble_dynamic_square_dump; do
  bash -c "ulimit -v 262144; ./builddir/sdcshield -e $t -t 2000 -n 1 -O $t.mdim=4096" \
      > /tmp/issue440_baseline/ulimit_$t.log 2>&1
  echo "$t: exit=$?"
done
```
Expected: 触发 `[init 降级] ... mdim=4096 → 降档 mdim=16` 日志,降档成功
  后 pass(或重试仍失败 → 资源性 SKIP;两种结局都记录,A/B 只要求两侧一致)。
  注意:256 MiB 虚拟内存上限下框架自身可能无法起多线程——若起不来,调大
  上限重试直到能跑到 init;目标是击中降档路径,不是极限值本身。

### Task 2: 共享头 + 探针等价性实证

**Files:**
- Create: `tests/cpu/eigen_gemm/sandstone_gemm_common.h`
- Test: `/tmp/issue440_probe_equiv.cpp`(throwaway,不入库)

**Interfaces:**
- Produces(后续任务与 7 文件依赖的确切签名):
  - `template <typename Data> static inline int sdc_gemm_init_shared(struct test *test, Data *d, int (*mdim)(struct test *))`
  - `struct sdc_gemm_splitmix { std::uint64_t s; std::uint64_t take(); }`
  - `template <typename Scalar> static inline int sdc_gemm_solve_once(int n)`
  - 元素策略 `template <typename Scalar> struct sdc_gemm_probe_elem`(对
    `std::complex<Real>` 偏特化,双流 re/im)

- [x] **Step 1: 写共享头(全文如下,含权威注释)**

`tests/cpu/eigen_gemm/sandstone_gemm_common.h`:

```cpp
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
 * (09a1c4cf)被迫同步打进全部 7 份——漏一处即口径分叉。本头是两块的唯一
 * 权威实现, 后续修正只落一处。
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
/* T10 init 降档 + 联合求解口径同步(7 用例共享, 跨架构无守卫——替换的   */
/* 原内联代码即无守卫)                                                 */
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

#if defined(__aarch64__) && defined(__linux__)
/* ------------------------------------------------------------------ */
/* 联合求解 sizing 探针(arm64-only, 同各文件原 #if 守卫; T9)           */
/* ------------------------------------------------------------------ */

/* splitmix 位置哈希流: (r, col) → 首状态 h(r*K1 ^ col*K2; 两次
 * xorshift32/乘混合), take() 取当前流值并推进(s = s*K3 + K1; s ^= s>>32)。
 * 谱系契约(检视 #51): 填充与 run() 的 fill_rand 同谱系(均匀随机 [-1,1))
 * ——splitmix 确定性哈希满足该契约, 且不依赖 init aux 线程上未定义的
 * per-thread RNG 状态。GEMM 计时对数值不敏感(无 SVD 式 deflation 路径),
 * 谱系由分布一致性满足; 填充 O(n²) 相对乘法 O(n³) 已计入探针计时
 * (保守方向)。同 T6 gemm_double_max_arm_body 的 solve_once。 */
struct sdc_gemm_splitmix {
    std::uint64_t s;
    std::uint64_t take()
    {
        std::uint64_t v = s;
        s = s * 0xD6E8FEB86659FD93ull + 0x9E3779B97F4A7C15ull;
        s ^= s >> 32;
        return v;
    }
};

/* 元素构造: 从哈希流取 k 路映射 [-1,1)。real 每元素 1 路(a←h, b←h2),
 * complex<double> 每元素 2 路(re/im 独立, a←h,h2; b←h3,h4)。complex 的
 * 两路用命名局部量测序——C++17 函数实参仅 indeterminately sequenced,
 * 两个 take() 同处一个实参表会有 re/im 互换的求值序风险。float 拷贝的
 * (float)((double)...) 双精度中转保水位。 */
template <typename Scalar>
struct sdc_gemm_probe_elem {
    Scalar operator()(sdc_gemm_splitmix &st) const
    {
        return (Scalar)((double)(st.take() % 2000u) / 1000.0 - 1.0);
    }
};

template <typename Real>
struct sdc_gemm_probe_elem<std::complex<Real>> {
    std::complex<Real> operator()(sdc_gemm_splitmix &st) const
    {
        std::uint64_t re = st.take();
        std::uint64_t im = st.take();
        return std::complex<Real>((double)(re % 2000u) / 1000.0 - 1.0,
                                  (double)(im % 2000u) / 1000.0 - 1.0);
    }
};

/* 联合求解探针: 以 splitmix 流填充 a/b 后做一次完整乘法(计时口径 =
 * 构造 + 求解, 结果弃用)。与被收敛的 7 份内联拷贝常数与运算序逐位
 * 一致(探针数值与计时口径不变)。 */
template <typename Scalar>
static inline int sdc_gemm_solve_once(int n)
{
    using Mat = Eigen::Matrix<Scalar, Eigen::Dynamic, Eigen::Dynamic>;
    Mat a(n, n), b(n, n), c;
    sdc_gemm_probe_elem<Scalar> elem;
    for (int r = 0; r < n; ++r)
        for (int col = 0; col < n; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            sdc_gemm_splitmix st{h};
            a(r, col) = elem(st);
            b(r, col) = elem(st);
        }
    c = a * b;
    return 0;
}
#endif /* __aarch64__ && __linux__ */

#endif /* SANDSTONE_GEMM_COMMON_H */
```

- [x] **Step 2: 探针等价性实证(throwaway harness)**

`/tmp/issue440_probe_equiv.cpp`:A 侧 = 从 `git show origin/main:tests/cpu/
eigen_gemm/gemm_double_dynamic_square.cpp`(及 float/cdouble 版)逐字复制的
原探针,仅把 `return 0;` 改为返回 c 的 FNV-1a 校验和;B 侧 = 新头的
`sdc_gemm_solve_once<Scalar>` 同样取校验和(B 侧循环体文本从新头逐字拷贝,
报告贴并列对照)。对 n ∈ {1,2,3,5,16,17,33} × {double,float,complex<double>}
比较两侧校验和。编译:
`g++ -std=c++20 -O2 -I framework -I third-party/eigen5 -I . /tmp/issue440_probe_equiv.cpp -o /tmp/issue440_probe_equiv`
Expected: 全部 n × 3 型校验和相等 → 探针填充与乘积逐位等价。
  任一不等 → 停,修头,不得进入 Task 3。

- [x] **Step 3: 提交计划文档(本文件)**

```bash
git add docs/superpowers/plans/2026-11-02-issue440-gemm-dedup-shared-helper.md
git commit -s -m "docs(plan): issue #440 计划——eigen_gemm T10 降档块与探针去重共享头"
```

### Task 3: 7 文件迁移到共享 helper

**Files(每个文件同型 4 步改动):**
- Modify: `tests/cpu/eigen_gemm/double14.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square_dump.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_float_dynamic_square.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_float_dynamic_square_dump.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square.cpp`
- Modify: `tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square_dump.cpp`

**Interfaces:**
- Consumes: Task 2 的三个符号(`sdc_gemm_init_shared` /
  `sdc_gemm_solve_once<Scalar>` / 头文件名)。
- Per-file 参数表(mdim 函数名 → 探针模板实参):

| 文件 | mdim 函数 | 探针实参 |
|---|---|---|
| double14.cpp | gemm_mdim | `sdc_gemm_solve_once<double>` |
| gemm_double_dynamic_square.cpp | gemm_mdim | `sdc_gemm_solve_once<double>` |
| gemm_double_dynamic_square_dump.cpp | gemm_k_dim | `sdc_gemm_solve_once<double>` |
| gemm_float_dynamic_square.cpp | gemm_mdim | `sdc_gemm_solve_once<float>` |
| gemm_float_dynamic_square_dump.cpp | gemm_k_dim | `sdc_gemm_solve_once<float>` |
| gemm_cdouble_dynamic_square.cpp | gemm_mdim | `sdc_gemm_solve_once<std::complex<double>>` |
| gemm_cdouble_dynamic_square_dump.cpp | gemm_mdim | `sdc_gemm_solve_once<std::complex<double>>` |

- [x] **Step 1(×7): include 与探针删除**

在 `#include "sdc_stress_config.h"` 后加 `#include "sandstone_gemm_common.h"`;
整体删除 `#if defined(__aarch64__) && defined(__linux__)` … `static int
gemm_solve_once(int n) { … }` … `#endif` 探针块(注释一并移入共享头)。

- [x] **Step 2(×7): 探针调用点替换**

`sdc_joint_dim_slice(test, "mdim", &ja, gemm_solve_once)` →
`sdc_joint_dim_slice(test, "mdim", &ja, <上表探针实参>)`。

- [x] **Step 3(×7): init 收敛**

init 函数体(自 `int dim = 0;` 起的 try/catch 整块,含降档注释)替换为:

```cpp
    test->data = new(eigen_test_data);
    return sdc_gemm_init_shared(test, CAST(test->data), <该文件 mdim 函数名>);
```

(`test->data = new(eigen_test_data);` 行原样保留在函数内,CAST 宏与
eigen_test_data 结构不动——成员名即 helper 接口。)

- [x] **Step 4: 全量 grep 复核**

```bash
grep -n "gemm_solve_once\|kInitRetryLo" tests/cpu/eigen_gemm/*.cpp
```
Expected: 仅 `gemm_double_max_arm_body.cpp` 可能余 solve_once 系引用(独立
变体,范围外);7 个目标文件零残留;`sandstone_gemm_common.h` 持有全部定义。

### Task 4: 验证电池(CLAUDE.md 规则 2)

**Files:** 无新改动(验证 Task 3 结果)。

- [x] **Step 1: 构建零新警告**

Run: `./scripts/build.sh 2>&1 | tee /tmp/issue440_fix_build.log`
Expected: 成功;与 Task 1 Step 2 基线比对无新增警告。

- [x] **Step 2: 7 测试回归**

```bash
for t in eigen_gemm_double14 eigen_gemm_double_dynamic_square \
         eigen_gemm_double_dynamic_square_dump eigen_gemm_float_dynamic_square \
         eigen_gemm_float_dynamic_square_dump eigen_gemm_cdouble_dynamic_square \
         eigen_gemm_cdouble_dynamic_square_dump; do
  ./builddir/sdcshield -e $t -t 2000 -n 1 -O $t.mdim=64; echo "$t: exit=$?"
done
```
Expected: 全部 `exit: pass`(契约规定的回归口径;显式 mdim 同 Task 1,
可直接 A/B)。

- [x] **Step 3: 默认(自适应+探针)路径回归**

Run: 7 测试各 `./builddir/sdcshield -e <id> -t 2000 -n 1`(无 -O)
Expected: 全部 pass;日志含 `[联合求解]` 行(证明共享探针在 arm64 真机
被执行)。

- [x] **Step 4: A/B 逐字节比对**

```bash
for t in ...7 ids...; do
  diff /tmp/issue440_baseline/normal_$t.log <(新跑同命令) && echo "$t normal: IDENTICAL"
  diff /tmp/issue440_baseline/ulimit_$t.log <(新跑同 ulimit 命令) && echo "$t ulimit: IDENTICAL"
done
```
Expected: 显式 mdim 的 normal/ulimit 日志逐字节一致(时间戳/耗时行如有,
剥离后比对,剥离规则在执行时如实记录);ulimit 侧 `[init 降级]` 文本与
结局(pass/SKIP)一致。

- [x] **Step 5: x86-64 非回归(检视)**

核对:共享头新增 include 对 x86 TU 仅引入未实例化模板与 arch 守卫段;
探针替换点全在 `#if defined(__aarch64__)` 分支内;x86 的 `#else`
(sdc_auto_square_dim)分支零改动;`sdc_gemm_init_shared` 文本与原无守卫
代码逐行等价。引用 diff 佐证。

- [x] **Step 6: 文档同步检查**

`grep -rn "gemm_solve_once\|init 降级" README.md docs/ --include="*.md"`
Expected: 无描述该重复结构/需更新的行为文档(纯内部重构);若有命中逐条
评估并在 PR 说明。

### Task 5: 提交 / 推送 / PR / 报告

- [x] **Step 1: fix commit(单单元)**

```bash
git add tests/cpu/eigen_gemm/
git commit -s -m "refactor(gemm): 收敛 T10 init 降档块与联合求解探针到共享头 (#440)

7 文件逐字复制(md5 全同)的降档块与 3 变体探针收敛到
sandstone_gemm_common.h, 行为逐位不变(校验和实证 + A/B 日志比对)。
PR#302 式修复自此只落一处。"
```

- [x] **Step 2: 计划勾选完成 commit**

```bash
git add docs/superpowers/plans/2026-11-02-issue440-gemm-dedup-shared-helper.md
git commit -s -m "docs(plan): issue #440 计划任务勾选完成"
git push -u origin fix/issue-440
```

- [x] **Step 3: 开 PR**

```bash
gh pr create --base main --head fix/issue-440 \
  --title "refactor(gemm): 收敛 T10 init 降档块与联合求解探针到共享头 (#440)" \
  --body "Closes #440 + 报告(根因/改动/验证证据)"
```
(auto-pr.yml 抢先失败时兜底 `gh pr edit fix/issue-440`。)

- [x] **Step 4: 报告贴 issue #440;工作区回 main 且 clean**

## Self-Review

- 覆盖:issue 两个命名块(降档块、探针)→ Task 2/3;7 文件 → 参数表;
  维护痛点(7 处同步)→ 单点权威实现。bidiag/max_arm/mdim 的范围外判定
  已给依据。✓
- 无占位符:头文件全文、每文件改动型、命令行均具体。✓
- 类型一致:`sdc_gemm_init_shared<Data>(test*, Data*, int(*)(struct test*))`
  与 7 文件的 `gemm_mdim/gemm_k_dim` 签名 `static int (struct test *)` 吻合;
  `sdc_joint_dim_slice` 第 4 参 `int solve_once(int)` 与
  `sdc_gemm_solve_once<Scalar>` 实例化吻合。✓
- 求值序陷阱(complex re/im)已在设计与注释中显式消解。✓

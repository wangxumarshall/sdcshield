# eigen 系列 SVE 统一支持 + eigen_svd_bidiag_sve 退役 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** eigen 系列全部 dense 用例获得 SVE 支持(12 个新 SVE 用例,四档 per-tier),收敛公共 dispatcher 框架,退役手写内核 `eigen_svd_bidiag_sve`。

**Architecture:** 复用既有 per-tier body + dispatcher 双文件模式(`eigen_svd_cdouble_sve`/`eigen_gemm_double_max_arm` 先例),抽取公共框架头 `sandstone_eigen_sve_dispatch.h` 吸收全部 tier 样板;SVD/Jacobi body 走 `EigenSVDTest` 模板(薄 typedef TU),GEMM body 镜像 dynamic_square run 语义(标量类型参数化);meson per-tier 编译块表驱动泛化。

**Tech Stack:** C++20 / meson / vendored Eigen5(`third-party/eigen5`,SVE packet 后端 `-DEIGEN_ARM64_USE_SVE`)/ gem5-fi(`/home/sdc/wangxu/gem5-fi`,SE 模式 `configs/se/arm_chaos.py`)。

**Spec:** `docs/superpowers/specs/2026-11-02-eigen-sve-unification-design.md`

## Global Constraints(每任务隐含遵守)

- **一 patch 一 unit**:每任务恰好一个 commit,验证通过后立即提交;顺序执行。
- **分支**:全程 `feat/eigen-sve-unification`(已建,spec commit `7a423af0` 已在);每 commit 前确认 `git branch --show-current`。
- **DCO**:commit 末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,之后无任何行,无 Co-Authored-By。
- **自验证 100% 真实**(规则 2):(a) `./scripts/build.sh` 零新警告;(b) 受影响行为实跑引用真实输出;(c) 回归 `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`;(d) x86-64 零触碰(全部改动在 `#if defined(__aarch64__)` / `host_machine.cpu_family() == 'aarch64'` guard 内,目检)。
- **meson 改动后必须** `meson setup --reconfigure builddir` 再 ninja(本文所有任务凡 Modify meson.build 均含此步;`./scripts/build.sh` 不自动 reconfigure)。
- **档位验证**(本机实测 SVE-256,无 SVE2/SME):256 原生;128 用 `SDCSHIELD_ARM64_SPEC_MAX_VL=128`(只降不升);512/1024 必须走 gem5-fi(本机不支持,规则 2 gem5-fi 条款——**只验 SKIP 路径不算验证**)。
- **gem5-fi gate**:每腿 stdout `SDCSHIELD_FAILS=0` + 16 位 checksum,真实输出引用进 commit message。
- **commit message 纪录**:每个用例 commit message 须含 `--list-tests` 实测计数变化(如 `SVE host PROD 486→487`)。
- **用例命名**:`<原用例名>_sve`;文件 `svd_<scalar>_sve.cpp` / `svd_<scalar>_sve_body.cpp`(BDCSVD)、`svd_<scalar>_sve.cpp`(jacobi 目录)、`gemm_<kind>_sve{,_body}.cpp`;tier namespace `<pfx>_vl{128,256,512,1024}`。
- **body 编译契约**(meson,每 body ×4 tier):`-march=armv8.2-a+sve -msve-vector-bits=<t> -DEIGEN_ARM64_SVE_VL=<t> -DEigen=EigenSVE<t> -DEIGEN_SVE_BODY_NS=<pfx>_vl<t>` + cpp_args `-DEIGEN_ARM64_USE_SVE`;dispatcher TU **无** `-msve-vector-bits`(VLA 分派,防常量折叠)。
- **元数据镜像原用例**(quality_level / fracture_loop_count / minimum_cpu / groups),差异仅 test id、描述串含 SVE、`arm64_requires = ARM64_REQUIRES_SVE`。
- **文档计数**:各用例 commit 后执行 `./builddir/sdcshield --list-tests | wc -l` 记录;README/CLAUDE.md 计数行统一在 Task 15 校对(避免 12 次文档抖动),但 Task 3(删用例)当步更新 README 计数链。

---

### Task 1: dispatcher 框架头 + 既有两用例迁移(行为等价,P1-1)

**Files:**
- Create: `tests/cpu/eigen_svd/sandstone_eigen_sve_dispatch.h`
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp`(删手写样板,用宏)
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve_body.cpp`(`SVD_TEST_NAMESPACE` → `EIGEN_SVE_BODY_NS`)
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp`(同上迁移 + 删 `gemm_run_dim`)
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm_body.cpp`(签名统一:`run_body(test, cpu)` 从 `test->data` 读 dim;新增 `make_data`/`cleanup`)
- Modify: `tests/cpu/meson.build:1152-1210`(foreach 表驱动泛化 + 宏名统一)
- Modify: `/home/sdc/wangxu/gem5-fi/workloads/sdcshield/eigen_gemm_double_max_arm_dual.cpp`(镜像 run_body 签名变化,保持同源)

**Interfaces:**
- Produces(后续所有任务消费):`EIGEN_SVE_DECLARE_BODY_NS(pfx)`(4-namespace 前向声明)、`EIGEN_SVE_DEFINE_DISPATCHERS(pfx)`(生成 `static long pfx##_tier_bits` + 5 个 static 函数:`pfx##_tier_select(struct test *, long svl_bytes)→bool`、`pfx##_solve_once(int n)→int`、`pfx##_make_data(int dim)→void*`、`pfx##_run(struct test *, int cpu)→int`、`pfx##_cleanup(struct test *)→void`);body 契约 `void *make_data(int); void cleanup(struct test *); int run_body(struct test *, int); int solve_once(int)`。

- [ ] **Step 1: 写框架头**(全文)

```cpp
/**
 * @file
 *
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Eigen SVE per-tier payload 的公共 dispatcher 框架(设计
 * docs/superpowers/specs/2026-11-02-eigen-sve-unification-design.md §3)。
 *
 * 背景:Eigen 的 SVE packet 后端是编译期定宽(arm_sve_vector_bits),VLA
 * 在 C++ 语言层面不可编译(sizeless struct 成员,两次 spike 确认)——
 * payload 按 tier(128/256/512/1024)各编译一次,运行时按框架 pin 的 VL
 * 分派(先例 eigen_svd_cdouble_sve / eigen_gemm_double_max_arm)。
 * 本头把两个先例 dispatcher 中逐字重复的 tier 样板(命名空间声明 /
 * tier 白名单 / make_data / solve_once / run 重 pin / cleanup 分派)
 * 收敛为两个宏,全部 eigen SVE 用例(既有 2 个 + 新增 12 个)单点维护。
 *
 * 用法(dispatcher TU,每用例一文件):
 *   EIGEN_SVE_DECLARE_BODY_NS(my_pfx);      // 4 个 per-tier namespace
 *   EIGEN_SVE_DEFINE_DISPATCHERS(my_pfx);    // 分派套件
 *   ... 自己的 test_init(knob/env/联合求解 → my_pfx##_make_data(dim))
 *
 * body TU 契约(meson 按 tier 编 4 次,-DEIGEN_SVE_BODY_NS=<pfx>_vl<t>):
 *   namespace <pfx>_vl<t> {
 *     void *make_data(int dim);   // dim 已解出;分配本用例 test->data
 *     int    solve_once(int n);   // 联合求解计时探针(同 tier packet)
 *     int    run_body(struct test *, int cpu);  // 从 test->data 读 dim
 *     void   cleanup(struct test *);
 *   }
 * data 类型由 body 自定,跨 tier 布局一致(同结构定义),dispatcher 只见
 * void*。
 */

#ifndef SANDSTONE_EIGEN_SVE_DISPATCH_H
#define SANDSTONE_EIGEN_SVE_DISPATCH_H

#include <sandstone.h>
#include "sdc_stress_config.h"

#include <device/cpu/arm64/arm64_vl.h>

/* 每 body TU 一个 namespace per tier,导出同一组 4 函数。 */
#define EIGEN_SVE_DECLARE_BODY_NS(pfx)                                       \
    namespace pfx##_vl128  {                                                 \
        void *make_data(int); void cleanup(struct test *);                   \
        int run_body(struct test *, int); int solve_once(int);                \
    }                                                                        \
    namespace pfx##_vl256  {                                                 \
        void *make_data(int); void cleanup(struct test *);                   \
        int run_body(struct test *, int); int solve_once(int);                \
    }                                                                        \
    namespace pfx##_vl512  {                                                 \
        void *make_data(int); void cleanup(struct test *);                   \
        int run_body(struct test *, int); int solve_once(int);                \
    }                                                                        \
    namespace pfx##_vl1024 {                                                 \
        void *make_data(int); void cleanup(struct test *);                   \
        int run_body(struct test *, int); int solve_once(int);                \
    }

/* 分派套件(每 dispatcher TU 一次)。pfx 必须与 EIGEN_SVE_DECLARE_BODY_NS
 * 相同。生成符号全部 static——多个 dispatcher TU 共用本头不冲突。
 *
 * run 语义(迁移自 svd_cdouble_sve.cpp,全用例统一 fail-closed):
 *   - SVE 消失(pin 与 run 之间)→ SKIP;
 *   - pin 被拒(pinned != want)→ SKIP;
 *   - readback != want → FAIL(框架 arm64 spec policy 契约);
 *   - worker 档 != init 记录档 → FAIL(variant/data 跨档重解释防护;
 *     对 tier-agnostic data 的 GEMM 这是 fail-closed 增严,见迁移注释);
 *   - 全部通过 → 分派到 worker 实际档的 run_body。
 */
#define EIGEN_SVE_DEFINE_DISPATCHERS(pfx)                                    \
    static long pfx##_tier_bits;                                              \
                                                                              \
    static bool pfx##_tier_select(struct test *test, long svl_bytes)         \
    {                                                                         \
        switch (svl_bytes * 8) {                                              \
        case 128:                                                             \
        case 256:                                                             \
        case 512:                                                             \
        case 1024:                                                            \
            pfx##_tier_bits = svl_bytes * 8;                                  \
            return true;                                                      \
        default:                                                              \
            log_skip(CpuNotSupportedSkipCategory,                             \
                     "%s: no Eigen SVE variant compiled for the %ld-bit tier",\
                     test->id, svl_bytes * 8);                                \
            return false;                                                     \
        }                                                                     \
    }                                                                         \
                                                                              \
    static int pfx##_solve_once(int n)                                        \
    {                                                                         \
        switch (pfx##_tier_bits) {                                            \
        case 128:  return pfx##_vl128::solve_once(n);                         \
        case 256:  return pfx##_vl256::solve_once(n);                         \
        case 512:  return pfx##_vl512::solve_once(n);                         \
        case 1024: return pfx##_vl1024::solve_once(n);                        \
        default:   return -1;  /* unreachable: tier_select 先行 */            \
        }                                                                     \
    }                                                                         \
                                                                              \
    static void *pfx##_make_data(int dim)                                     \
    {                                                                         \
        switch (pfx##_tier_bits) {                                            \
        case 128:  return pfx##_vl128::make_data(dim);                        \
        case 256:  return pfx##_vl256::make_data(dim);                        \
        case 512:  return pfx##_vl512::make_data(dim);                        \
        case 1024: return pfx##_vl1024::make_data(dim);                       \
        default:   return nullptr;  /* unreachable: tier_select 先行 */       \
        }                                                                     \
    }                                                                         \
                                                                              \
    static int pfx##_run(struct test *test, int cpu)                          \
    {                                                                         \
        const long want_bytes = arm64_sve_vl_bytes();                         \
        if (want_bytes <= 0) {                                                \
            log_skip(CpuNotSupportedSkipCategory,                             \
                     "%s: SVE disappeared between pin and run", test->id);   \
            return EXIT_SKIP;                                                 \
        }                                                                     \
        const long pinned = arm64_sve_pin_vl(want_bytes);                     \
        if (pinned != want_bytes) {                                           \
            log_skip(CpuNotSupportedSkipCategory,                             \
                     "%s: cannot pin the worker to the %ld-bit VL "           \
                     "(prctl rc=%ld)", test->id, want_bytes * 8, pinned);      \
            return EXIT_SKIP;                                                 \
        }                                                                     \
        if (arm64_sve_vl_bytes() != want_bytes) {                             \
            log_error("%s: pinned %ld bytes but read back %ld",               \
                      test->id, want_bytes, arm64_sve_vl_bytes());            \
            report_fail_msg("%s: SVE VL readback mismatch after pin",         \
                            test->id);                                        \
        }                                                                     \
        if (want_bytes * 8 != pfx##_tier_bits) {                               \
            log_error("%s: run tier %ld-bit differs from the %ld-bit "        \
                      "variant the data was allocated for", test->id,          \
                      want_bytes * 8, pfx##_tier_bits);                        \
            report_fail_msg("%s: Eigen SVE variant/tier mismatch",           \
                            test->id);                                        \
        }                                                                     \
        log_info("%s: Eigen SVE variant = %ld-bit", test->id,                  \
                 want_bytes * 8);                                             \
        switch (want_bytes * 8) {                                             \
        case 128:  return pfx##_vl128::run_body(test, cpu);                   \
        case 256:  return pfx##_vl256::run_body(test, cpu);                   \
        case 512:  return pfx##_vl512::run_body(test, cpu);                   \
        case 1024: return pfx##_vl1024::run_body(test, cpu);                  \
        default:                                                              \
            log_skip(CpuNotSupportedSkipCategory,                             \
                     "%s: no Eigen SVE variant compiled for the %ld-bit "     \
                     "tier", test->id, want_bytes * 8);                        \
            return EXIT_SKIP;                                                 \
        }                                                                     \
    }                                                                         \
                                                                              \
    static void pfx##_cleanup(struct test *test)                              \
    {                                                                         \
        switch (pfx##_tier_bits) {                                            \
        case 128:  pfx##_vl128::cleanup(test);  break;                        \
        case 256:  pfx##_vl256::cleanup(test);  break;                        \
        case 512:  pfx##_vl512::cleanup(test);  break;                        \
        case 1024: pfx##_vl1024::cleanup(test); break;                        \
        default: break;  /* tier 未选 → init 已 SKIP,无 data */               \
        }                                                                     \
    }

#endif // SANDSTONE_EIGEN_SVE_DISPATCH_H
```

- [ ] **Step 2: 迁移 `svd_cdouble_sve.cpp`**

替换关系(原行号 → 新):
- L75-78 命名空间声明块 → `EIGEN_SVE_DECLARE_BODY_NS(svd);`
- L83 `static long eigen_svd_cdouble_sve_tier_bits;` → 删除(宏生成 `svd_tier_bits`)
- L91-100 `solve_once_cdouble_sve` → 删除(宏生成 `svd_solve_once`);`sdc_joint_dim_slice` 调用处改传 `svd_solve_once`
- L215-228 tier 校验 switch → `if (!svd_tier_select(test, svl)) return EXIT_SKIP;`
- L304-312 make_data switch → `test->data = svd_make_data(dim);`
- L328-367 `eigen_svd_cdouble_sve_run` → 整体替换为 `return svd_run(test, cpu);`(宏生成,注释指针改为指向框架头)
- L369-379 cleanup → `return svd_cleanup(test);`(wrapper 一行保留 test_cleanup 签名)
- DECLARE_TEST 三个函数指针与 `.arm64_requires` 等元数据**零变化**;`.test_cleanup` 现在接 wrapper。
- 文件头注释加一段:dispatcher 样板已收敛到 `sandstone_eigen_sve_dispatch.h`(迁移 commit 本步)。
- `#include "sandstone_eigen_sve_dispatch.h"`(同目录)。

- [ ] **Step 3: 迁移 `gemm_double_max_arm.cpp`**

- L110-113 命名空间声明 → `EIGEN_SVE_DECLARE_BODY_NS(gemm);`(签名随之统一为 `run_body(struct test *, int)`)
- L100 `gemm_tier_bits`、L105 `gemm_run_dim` → 删除(宏生成 `gemm_tier_bits`;dim 改存 data)
- L118-127 `solve_once_gemm` → 删除,`sdc_joint_dim_slice` 调用处改传 `gemm_solve_once`
- L229-241 tier 校验 → `gemm_tier_select(test, svl)`
- L292 `gemm_run_dim = dim;` → `test->data = gemm_make_data(dim);`
- L308-342 `eigen_gemm_double_max_arm_run` → `return gemm_run(test, cpu);`
- DECLARE_TEST 增 `.test_cleanup = eigen_gemm_double_max_arm_cleanup`(wrapper 一行:`gemm_cleanup(test)`;原用例无 cleanup——body 新增 cleanup 释放 data,原实现 data 存在 static 变量中,现改 per-child data,须释放,行为增广:fork 子进程整体回收语义下 cleanup 是良好卫生而非行为变化)
- 迁移注释(文件头):run 统一补 variant-mismatch FAIL(fail-closed 增严,框架 policy 契约);dim 从 `gemm_run_dim` static 改为 per-child data(exec_each_test 子进程镜像各自解出,语义更准)。

- [ ] **Step 4: 迁移 `gemm_double_max_arm_body.cpp`**

- `run_body(struct test *test, int cpu, int dim)` → `run_body(struct test *test, int cpu)`:`dim` 从 `test->data` 读。新增文件顶结构:

```cpp
namespace EIGEN_SVE_BODY_NS {  /* 原 GEMM_TEST_NAMESPACE 改名,meson 同步 */
using namespace Eigen;
typedef Matrix<double, Dynamic, Dynamic> Mat;
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
/* run_body 内首行: */
/*   const int dim = static_cast<gemm_sve_data *>(test->data)->dim; */
```

- `#ifndef GEMM_TEST_NAMESPACE` → `#ifndef EIGEN_SVE_BODY_NS`(两个 body 文件同改,含 svd_cdouble_sve_body.cpp 的 `SVD_TEST_NAMESPACE`)。

- [ ] **Step 5: meson 表驱动泛化**(`tests/cpu/meson.build:1152-1210`)

```meson
if host_machine.cpu_family() == 'aarch64'
    # --- Eigen payload: one instantiation per VL tier, table-driven ------
    # [body-source, namespace-prefix] — library 名 = tests_eigen_<pfx>_vl<t>
    # (既有 svd/gemm 两前缀的库名不变;宏 EIGEN_SVE_BODY_NS 由 meson 注入,
    #  body TU 的旧 SVD_TEST_NAMESPACE/GEMM_TEST_NAMESPACE 已统一)。
    eigen_sve_body_specs = [
        ['eigen_svd/svd_cdouble_sve_body.cpp',       'svd'],
        ['eigen_gemm/gemm_double_max_arm_body.cpp',  'gemm'],
    ]
    foreach spec : eigen_sve_body_specs
        body_src = spec[0]
        pfx = spec[1]
        foreach tier : ['128', '256', '512', '1024']
            eigen_tier_args = [
                '-march=armv8.2-a+sve',
                '-msve-vector-bits=' + tier,
                '-DEIGEN_ARM64_SVE_VL=' + tier,
                '-DEigen=EigenSVE' + tier,
                '-DEIGEN_SVE_BODY_NS=' + pfx + '_vl' + tier,
            ]
            eigen_vl_a = static_library(
                'tests_eigen_' + pfx + '_vl' + tier,
                sources : body_src,
                dependencies: [ eigen3_dep, boost_dep ],
                build_by_default: false,
                include_directories: [ tests_common_incdirs ],
                c_args : [ tests_common_c_args, eigen_tier_args ],
                cpp_args : [ tests_common_cpp_args, eigen_tier_args,
                             '-DEIGEN_ARM64_USE_SVE', sve_bitpattern_exempt ],
            )
            sandstone_tests += [eigen_vl_a]
        endforeach
    endforeach
```

(保留原块上方 issue #410 complex 四档注释与 real/complex 有效性注释,前缀行相应改写;`march_sve_dispatch` 段与 tests_sve 库段不动。)

- [ ] **Step 6: gem5 双 kernel 同源同步**

`/home/sdc/wangxu/gem5-fi/workloads/sdcshield/eigen_gemm_double_max_arm_dual.cpp`:镜像 Step 4 签名变化(dim 从 data 读),Makefile 重建四档二进制(先例命令在 `workloads/sdcshield/Makefile`)。

- [ ] **Step 7: 构建验证**

```bash
meson setup --reconfigure builddir && ./scripts/build.sh
```
期望:零新警告;`tests_eigen_{svd,gemm}_vl{128,256,512,1024}` 八库正常编出。

- [ ] **Step 8: 本机两档实跑(引用真实输出)**

```bash
./builddir/sdcshield -e eigen_svd_cdouble_sve -t 30 -O eigen_svd_cdouble_sve.mdim=300 -n 1
./builddir/sdcshield -e eigen_gemm_double_max_arm -t 30 -O eigen_gemm_double_max_arm.mdim=1024 -n 1
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield -e eigen_svd_cdouble_sve -t 30 -O eigen_svd_cdouble_sve.mdim=300 -n 1
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield -e eigen_gemm_double_max_arm -t 30 -O eigen_gemm_double_max_arm.mdim=1024 -n 1
```
期望:全部 `exit: pass`;128 档日志含 `Eigen SVE variant = 128-bit`。

- [ ] **Step 9: gem5 四档回归(行为零变化硬证据)**

```bash
cd /home/sdc/wangxu/gem5-fi
for vl in 1 2 4 8; do
  build/ARM/gem5.opt --quiet --outdir=/tmp/g5/task1-svd-$vl configs/se/arm_chaos.py \
    --cpu O3 --max_faults 0 --cmd workloads/sdcshield/eigen_svd_cdouble_sve_vl*_dual \
    --sve_vl $vl 2>&1 | grep -E "SDCSHIELD_FAILS|^[0-9a-f]{16}$"
done
```
(**注**:双 kernel 二进制按 tier 选 `eigen_svd_cdouble_sve_vl{128,256,512,1024}_dual`;`--sve_vl {1,2,4,8}` = 128/256/512/1024-bit;匹配档的腿跑真 payload。gemm 同构四腿。)
期望:8 腿全 `SDCSHIELD_FAILS=0`;**svd 512 档 checksum 须复现先例 `50ff99225080c21c`**(fc123cd2 迁移前值——逐位等价的行为零变化证据)。

- [ ] **Step 10: 回归 + 提交**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1   # 期望 exit: pass
git add -A && git commit -s -m "refactor(eigen-sve): 抽取公共 per-tier dispatcher 框架头,迁移既有两用例

sandstone_eigen_sve_dispatch.h 收敛 tier 样板(声明/白名单/分派/re-pin);
svd_cdouble_sve + gemm_double_max_arm 迁移,EIGEN_SVE_BODY_NS 宏统一,
GEMM body 签名统一 run_body(test,cpu) + make_data/cleanup;
GEMM run 增严 variant-mismatch FAIL(fail-closed,arm64 spec policy)。
行为零变化证据:本机 256/128 两档 pass;gem5 四档 SDCSHIELD_FAILS=0,
svd 512 checksum 复现先例 50ff99225080c21c。

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 2: eigen_svd_double_sve(P1-2,承接 bidiag 的 SVE FMA 场景)

**Files:**
- Create: `tests/cpu/eigen_svd/svd_double_sve.cpp`(dispatcher)
- Create: `tests/cpu/eigen_svd/svd_double_sve_body.cpp`(body;`svd_f64` 前缀,后续 eigen_svd_double2_sve 共享)
- Modify: `tests/cpu/meson.build`(body specs 表加行 + `tests_set_sve.add` files 加 dispatcher)

**Interfaces:**
- Consumes: Task 1 框架头(宏)、`EigenSVDTest` 模板
- Produces: `svd_f64` body 契约(Task 6 复用);dispatcher/init 全模式(Task 4/5/7-14 镜像)

- [ ] **Step 1: body TU 全文**

```cpp
/*
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Eigen SVD payload body — double BDCSVD,compiled ONCE PER VL TIER
 * (128/256/512/1024)。模式同 svd_cdouble_sve_body.cpp(先例);
 * 服务 eigen_svd_double_sve 与 eigen_svd_double2_sve(两 dispatcher,
 * 同一计算,diff 仅 knob 名与 test id)。签名契约见
 * sandstone_eigen_sve_dispatch.h。
 */
#include "sandstone_eigen_common.h"
#include "sdc_stress_config.h"

#include <arm_sve.h>
#include <device/cpu/arm64/arm64_vl.h>
#include <unistd.h>

#ifndef EIGEN_SVE_BODY_NS
#  error "EIGEN_SVE_BODY_NS must be defined by the build (one namespace per VL tier)"
#endif

namespace EIGEN_SVE_BODY_NS {

using namespace Eigen;

typedef Matrix<double, Dynamic, Dynamic> Mat;
typedef Eigen::BDCSVD<Mat> SVD;

using test_type = EigenSVDTest<SVD, Eigen::Dynamic>;

void *make_data(int dim)
{
    auto *d = new test_type::eigen_test_data;
    d->dim = dim;
    return d;
}

int solve_once(int dim)
{
    return test_type::sizing_probe(dim);
}

void cleanup(struct test *test)
{
    test_type::cleanup(test);
}

int run_body(struct test *test, int cpu)
{
    return test_type::run(test, cpu);
}

}  // namespace EIGEN_SVE_BODY_NS
```

- [ ] **Step 2: dispatcher TU 全文**

```cpp
/**
 * @file
 *
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b eigen_svd_double_sve
 * @parblock
 * ARM64 SVE-backed counterpart of @ref eigen_svd_double。
 *
 * 同一 Eigen BDCSVD(double)负载,payload 以 SVE packet 后端按档编译
 * (128/256/512/1024),dispatcher 按框架 pin 的 VL 分派(模式同
 * @ref eigen_svd_cdouble_sve;样板收敛于 sandstone_eigen_sve_dispatch.h)。
 * BDCSVD 第一阶段即 Golub-Kahan Householder 双对角化——本用例承接
 * 退役的 eigen_svd_bidiag_sve 的 dense SVE FMA 场景(后者手写内核,
 * 存在理由——vendored Eigen SVE packet SIGSEGV——已被 issue #410 消除)。
 *
 * 维度:knob/env > 联合求解,镜像 eigen_svd_double 的
 * calibrated_auto_dim(test, 8, 6, 12.0, 650, 64, 2048) 参数口径;
 * override: -O eigen_svd_double_sve.mdim=N (64..2048)。
 * @endparblock
 */

#include "sandstone_eigen_common.h"
#include "sandstone_eigen_sve_dispatch.h"
#include "sdc_stress_config.h"

/* 既有先例 svd_cdouble_sve.cpp:全 TU 在 aarch64 guard 内,无 #else——
 * dispatcher TU 只在 aarch64 编进 tests_sve,x86-64 零注册(rule 5)。 */
#if defined(__aarch64__)

#include <arm_sve.h>
#include <device/cpu/arm64/arm64_vl.h>

EIGEN_SVE_DECLARE_BODY_NS(svd_f64)
EIGEN_SVE_DEFINE_DISPATCHERS(svd_f64)

#define MDIM_MIN 64
#define MDIM_MAX 2048

static int eigen_svd_double_sve_init(struct test *test)
{
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;
    if (!svd_f64_tier_select(test, svl))
        return EXIT_SKIP;

    int dim;
    int64_t knob = sdc_knob_int_clamped(test, "mdim", 0, MDIM_MIN, MDIM_MAX);
    if (knob == 0)
        sdc_env_knob_int_clamped("eigen_svd_double_sve", "mdim", &knob,
                                 MDIM_MIN, MDIM_MAX);
    if (knob > 0) {
        dim = (int)knob;
    } else {
        struct sdc_joint_args ja = {
            .lo = MDIM_MIN, .hi = MDIM_MAX,
            .fam = SDC_RSS_BDCSVD_F64,  /* RSS 表项与 NEON 版共用(同算法同矩阵数) */
            .bytes_per_elem = 8, .matrices_per_thread = 6, .rss_ratio = 12.0,
            .shared_bytes_per_slice = 0,   /* make_data holds no matrices */
            .mem_fraction_permille = 650,
            .ar_fam = SDC_FAM_MEM_BOUND,
            .run_budget_ms = 140000,   /* 镜像 sandstone_eigen_common.h 联合层值 */
        };
        try {
            dim = sdc_joint_dim_slice(test, "mdim", &ja, svd_f64_solve_once);
        } catch (...) {
            log_warning("[联合求解] %s: mdim 探针异常 → 退守下限 %d",
                        test->id, MDIM_MIN);
            dim = MDIM_MIN;
        }
    }
    test->data = svd_f64_make_data(dim);
    return EXIT_SUCCESS;
}

/* DECLARE_TEST 需要普通函数符号——一行 wrapper(DECLARE_TEST 之前定义)。 */
static int eigen_svd_double_sve_run(struct test *test, int cpu)
{
    return svd_f64_run(test, cpu);
}

static int eigen_svd_double_sve_cleanup(struct test *test)
{
    svd_f64_cleanup(test);
    return EXIT_SUCCESS;
}

DECLARE_TEST(eigen_svd_double_sve, "Eigen SVD (Singular Value Decomposition) solving payload on the ARM64 SVE packet backend (per-tier 128/256/512/1024-bit variants selected at the framework-pinned VL); counterpart of eigen_svd_double")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = eigen_svd_double_sve_init,
  .test_run = eigen_svd_double_sve_run,
  .test_cleanup = eigen_svd_double_sve_cleanup,
  .fracture_loop_count = 5,
  .quality_level = TEST_QUALITY_PROD,
  .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST

#endif // __aarch64__
```

- [ ] **Step 3: meson 挂接**

`eigen_sve_body_specs` 表加行 `['eigen_svd/svd_double_sve_body.cpp', 'svd_f64'],`;`tests_set_sve.add(... files(...))`(meson.build:570)加 `'eigen_svd/svd_double_sve.cpp',`。

- [ ] **Step 4: 构建 + 本机两档实跑**

```bash
meson setup --reconfigure builddir && ./scripts/build.sh
./builddir/sdcshield -e eigen_svd_double_sve -t 30 -n 1          # 256 档,期望 exit: pass
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield -e eigen_svd_double_sve -t 30 -n 1   # 128 档 pass,日志 variant = 128-bit
./builddir/sdcshield --list-tests | wc -l    # 记录计数(+1)
```

- [ ] **Step 5: gem5 512/1024 两档**

新建 `/home/sdc/wangxu/gem5-fi/workloads/sdcshield/eigen_svd_double_sve_dual.cpp`(同源双 kernel:镜像 body 的 BDCSVD double 负载 + 确定性填充 + FNV-1a checksum,先例 `eigen_svd_cdouble_sve_dual.cpp` 结构),四档编译,跑 512/1024 两腿(`--sve_vl 4/8`),gate `SDCSHIELD_FAILS=0` + checksum;128/256 腿本机已验,可四腿全跑交叉确认。

- [ ] **Step 6: 回归 + 提交**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1   # exit: pass
git add -A && git commit -s -m "test(eigen-sve): 新增 eigen_svd_double_sve——SVE packet 后端 double BDCSVD(四档 per-tier)

承接 eigen_svd_bidiag_sve 退役后的 dense SVE FMA 场景(BDCSVD 第一阶段
即 Householder 双对角化)。本机 256/128 pass,gem5 512/1024 SDCSHIELD_FAILS=0
(checksum <实测值>)。--list-tests 计数 <N>→<N+1>。

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 3: 退役删除 eigen_svd_bidiag_sve(P1-3)

**Files:**
- Delete: `tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp`(及空目录 `tests/cpu/sve/eigen/`)
- Modify: `tests/cpu/sve/meson.build:79-86`(源列表行 + 注释块)
- Modify: `README.md`(「测试用例与检测能力」计数链:SVE host PROD −1)

**Interfaces:**
- Consumes: Task 2 的 eigen_svd_double_sve(SVE FMA 场景承接声明)

- [ ] **Step 1: 删除文件与 meson 行**(`tests/cpu/sve/meson.build` 的 `'eigen/eigen_svd_bidiag_sve.cpp',` 行及其上方注释块——注释讲 SIGSEGV 绕开史,一并删除)

- [ ] **Step 2: 构建 + 全量影响验证**

```bash
meson setup --reconfigure builddir && ./scripts/build.sh
./builddir/sdcshield --list-tests | grep -c bidiag      # 期望 0
./builddir/sdcshield -e eigen_svd_bidiag_sve -t 5 -n 1  # 期望:no such test 退出(或 --list 无此名)
```

- [ ] **Step 3: README 计数链更新**(grep 定位 `测试用例与检测能力` 段的 SVE host PROD 行,486→485,加日期注 `2026-11-02 feat/eigen-sve-unification`)

- [ ] **Step 4: 回归 + 提交**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1   # exit: pass
git add -A && git commit -s -m "test(sve): 退役 eigen_svd_bidiag_sve——存在理由已被 issue #410 消除

手写 SVE 双对角化内核当初为绕开 vendored Eigen SVE double packet 的
SIGSEGV;issue #410(ptranspose scatter 重写)修复后该理由消失,
dense SVE FMA 场景由 eigen_svd_double_sve(Eigen BDCSVD SVE 后端)
完整承接。SVE host PROD 486→485,README 计数链同步。

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 4: eigen_svd_sve(float;svd_f32 body 首建)

**Files:**
- Create: `tests/cpu/eigen_svd/svd_float_sve_body.cpp`(前缀 `svd_f32`;Task 5 共享)
- Create: `tests/cpu/eigen_svd/svd_float_sve.cpp`
- Modify: `tests/cpu/meson.build`(body 表 +1 行、sve set +1 文件)

**Interfaces:** 同 Task 2(唯一差异集合见下)

- [ ] **Step 1: body** — Task 2 Step 1 模板逐字复制,改 3 处:注释标 float BDCSVD、`typedef Matrix<float, Dynamic, Dynamic> Mat;`、文件头注释服务对象为 eigen_svd_sve 与 eigen_svd_fvectors_sve。
- [ ] **Step 2: dispatcher** — Task 2 Step 2 模板复制,替换以下全部参数:test id `eigen_svd_sve`;pfx `svd_f32`;env 名 `eigen_svd_sve`;`.fam = SDC_RSS_BDCSVD_F32`;`.bytes_per_elem = 4`;`MDIM_MIN/MAX = 64/2048`;`.fracture_loop_count = 2`(镜像 eigen_svd 原值);描述串 "…on the ARM64 SVE packet backend (per-tier 128/256/512/1024-bit variants…); counterpart of eigen_svd"。
- [ ] **Step 3: meson 挂接**(同 Task 2 Step 3:`['eigen_svd/svd_float_sve_body.cpp', 'svd_f32'],` + `'eigen_svd/svd_float_sve.cpp',`)
- [ ] **Step 4: 构建 + 本机 256/128 实跑 + 计数**(命令同 Task 2 Step 4,`-e eigen_svd_sve`)
- [ ] **Step 5: gem5 512/1024 双 kernel 四腿**(`eigen_svd_sve_dual.cpp`,同 Task 2 Step 5 流程)
- [ ] **Step 6: 回归 + 提交**(message 模式同 Task 2,subject `test(eigen-sve): 新增 eigen_svd_sve——SVE packet 后端 float BDCSVD(四档 per-tier)`)

---

### Task 5: eigen_svd_fvectors_sve(共享 svd_f32 body)

**Files:**
- Create: `tests/cpu/eigen_svd/svd_fvectors_sve.cpp`(dispatcher only)
- Modify: `tests/cpu/meson.build`(sve set +1 文件;body 表不变)

- [ ] **Step 1: dispatcher** — Task 4 Step 2 模板复制,仅改:test id/env 名 `eigen_svd_fvectors_sve`、描述串尾 `counterpart of eigen_svd_fvectors`(EIGEN_SVE_DECLARE_BODY_NS(svd_f32) 不变——共享 body;两 TU 各自 static 套件无符号冲突)。历史注:fvectors 与 float 版自随机化改造后已是同一计算(H11' 删 init-time interesting-vector),SVE 版维持镜像对称。
- [ ] **Step 2: meson + 构建 + 本机两档 + 计数**(同 Task 2 Step 3-4)
- [ ] **Step 3: gem5 四腿**(复用 svd_f32 body 双 kernel,双 kernel 的 test-id 无关——payload 相同,引用 Task 4 腿结果 + 本档两腿新跑即可;若 dual kernel 以 dim 参数化则同源复用)
- [ ] **Step 4: 回归 + 提交**(`test(eigen-sve): 新增 eigen_svd_fvectors_sve——共享 svd_f32 per-tier body`)

---

### Task 6: eigen_svd_double2_sve(共享 svd_f64 body)

**Files:**
- Create: `tests/cpu/eigen_svd/svd_double2_sve.cpp`(dispatcher only)
- Modify: `tests/cpu/meson.build`(sve set +1 文件)

- [ ] **Step 1: dispatcher** — Task 2 Step 2 模板复制,改:test id/env 名 `eigen_svd_double2_sve`、描述 `counterpart of eigen_svd_double2`;pfx 保持 `svd_f64`(共享 body)。
- [ ] **Step 2: meson + 构建 + 本机 256/128 + 计数**
- [ ] **Step 3: gem5** — 同 Task 5(复用 svd_f64 body 腿)
- [ ] **Step 4: 回归 + 提交**

---

### Task 7: eigen_svd_jacobi_sve(float;jacobi body 模式首建)

**Files:**
- Create: `tests/cpu/eigen_svd_jacobi/svd_float_sve_body.cpp`(前缀 `svd_j_f32`;Task 8 共享)
- Create: `tests/cpu/eigen_svd_jacobi/svd_float_sve.cpp`
- Modify: `tests/cpu/meson.build`(body 表 +1、sve set +1)

- [ ] **Step 1: body** — Task 2 Step 1 模板复制,改:include 路径 `"eigen_svd/sandstone_eigen_common.h"`(跨目录,镜像 eigen_svd_jacobi 既有 TU 的 include 形式);`typedef Eigen::JacobiSVD<Mat> SVD;`;Mat 为 float。solve_once 委托同一 `test_type::sizing_probe`(EigenSVDTest 模板承载,JacobiSVD 实例化)。
- [ ] **Step 2: dispatcher** — Task 2 Step 2 模板复制,改:test id/env `eigen_svd_jacobi_sve`;pfx `svd_j_f32`;`.fam = SDC_RSS_JACOBI_F32`;`.bytes_per_elem = 4`;`.rss_ratio = 8.0`(Jacobi 族原值);`.fracture_loop_count` 不设(镜像原 jacobi 无此项);**`.quality_level = TEST_QUALITY_SKIP`**(镜像原用例,issue #322 晋升验证另案);描述尾 `counterpart of eigen_svd_jacobi`。
- [ ] **Step 3: meson + 构建 + 本机两档实跑**(SKIP 级需显式:`./builddir/sdcshield --quality=-1 -e eigen_svd_jacobi_sve -t 30 -n 1`,期望 pass)
- [ ] **Step 4: gem5 四腿**(`eigen_svd_jacobi_sve_dual.cpp`)
- [ ] **Step 5: 回归 + 提交**(`test(eigen-sve): 新增 eigen_svd_jacobi_sve——SVE 后端 float Jacobi SVD(SKIP 级跟随原用例)`)

---

### Task 8: eigen_svd_jacobi_fvectors_sve(共享 svd_j_f32)

**Files:** Create: `tests/cpu/eigen_svd_jacobi/svd_fvectors_sve.cpp`;Modify: meson sve set
- [ ] dispatcher 复制 Task 7(改 test id/env/描述;`svd_j_f32` 共享);meson;构建;`--quality=-1` 两档实跑;gem5 复用 j_f32 腿;回归 + 提交。

---

### Task 9: eigen_svd_jacobi_double_sve(svd_j_f64 body 首建)

**Files:** Create: `tests/cpu/eigen_svd_jacobi/svd_double_sve_body.cpp` + `svd_double_sve.cpp`;Modify: meson(body 表 +1、sve set +1)
- [ ] body 复制 Task 7 改 `Matrix<double,…>` + pfx `svd_j_f64` + `.fam = SDC_RSS_JACOBI_F64` + `.bytes_per_elem = 8`;dispatcher 改 test id/env `eigen_svd_jacobi_double_sve`、fracture 不设、SKIP 级;meson;构建;`--quality=-1` 两档;gem5 四腿;回归 + 提交。

---

### Task 10: eigen_svd_jacobi_cdouble_sve(svd_j_c128 body 首建)

**Files:** Create: `tests/cpu/eigen_svd_jacobi/svd_cdouble_sve_body.cpp` + `svd_cdouble_sve.cpp`;Modify: meson(body 表 +1、sve set +1)
- [ ] body 复制 Task 7 改 `Matrix<std::complex<double>,…>` + pfx `svd_j_c128` + `.fam = SDC_RSS_JACOBI_C128` + `.bytes_per_elem = 16`;dispatcher 改 test id/env `eigen_svd_jacobi_cdouble_sve`、SKIP 级;meson;构建;`--quality=-1` 两档;gem5 四腿(complex,全部四档合法——issue #410);回归 + 提交。

---

### Task 11: eigen_gemm_double_dynamic_square_sve(GEMM body 模式首建,f64)

**Files:**
- Create: `tests/cpu/eigen_gemm/gemm_f64_sve_body.cpp`(前缀 `gemm_f64`;Task 14 共享)
- Create: `tests/cpu/eigen_gemm/gemm_double_dynamic_square_sve.cpp`
- Modify: `tests/cpu/meson.build`(body 表 +1、sve set +1)

**Interfaces:**
- Produces: GEMM body 模式(Task 12/13/14 镜像):`make_data/cleanup/run_body/solve_once`,data 结构 `struct gemm_sve_data { int dim; }`

- [ ] **Step 1: body TU 全文**(run 语义镜像 `gemm_double_dynamic_square.cpp` 的 run,f64 版)

```cpp
/*
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * Eigen GEMM payload body — double,compiled ONCE PER VL TIER
 * (128/256/512/1024)。run 语义镜像 gemm_double_dynamic_square.cpp 的
 * run(per-thread operands + golden 同源首算 + kRerollEvery 重掷 +
 * memcmp_or_fail;T10 bad_alloc 窄 catch 边界契约);solve_once 谱系
 * 契约(检视 #51)同 gemm_double_dynamic_square.cpp 的 gemm_solve_once
 * (splitmix 确定性哈希,与 fill_rand 同分布)。服务
 * eigen_gemm_double_dynamic_square_sve 与 eigen_gemm_double14_sve
 * (两 dispatcher,同一计算,knob 名/lo-hi 差异在各自 TU)。
 * 签名契约见 sandstone_eigen_sve_dispatch.h。
 */
#include <sandstone.h>
#include "sdc_stress_config.h"

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

typedef Matrix<double, Dynamic, Dynamic> Mat;

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
            m(r, c) = frandom_scale(2.0) - 1.0;
}

int run_body(struct test *test, int cpu)
{
    (void)cpu;
    const int dim = static_cast<gemm_sve_data *>(test->data)->dim;

    /* randomization hardening H11'(P14)+ T10 catch 边界:逐字镜像
     * gemm_double_dynamic_square.cpp run 的段结构。 */
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
    /* splitmix 同谱系探针(镜像 gemm_double_dynamic_square.cpp 的
     * gemm_solve_once):与 fill_rand 同分布、不依赖 per-thread RNG。 */
    Mat a(dim, dim), b(dim, dim), c;
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
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
    return 0;
}

}  // namespace EIGEN_SVE_BODY_NS
```

- [ ] **Step 2: dispatcher TU**(镜像 gemm_float_dynamic_square.cpp 的 `gemm_mdim` 联合求解参数,f64 口径)

结构同 Task 2 Step 2(init:`arm64_require_sve_vl` + `gemm_f64_tier_select` + knob/env/joint + `gemm_f64_make_data`),差异参数:test id/env `eigen_gemm_double_dynamic_square_sve`;pfx `gemm_f64`;`MDIM_MIN/MAX = 16/8192`(镜像原用例 lo/hi);`.fam = SDC_RSS_GEMM_F64`;`.bytes_per_elem = 8`;`.matrices_per_thread = 4`;`.rss_ratio = 6.0`;`.mem_fraction_permille = 850`;`.ar_fam = SDC_FAM_COMPUTE_BOUND`;`.run_budget_ms = 200000`;`fracture_loop_count` 不设;quality PROD;描述 "Eigen GEMM payload (double, dynamic, square) on the ARM64 SVE packet backend (per-tier 128/256/512/1024-bit variants…)"。

- [ ] **Step 3: meson**(body 表 `['eigen_gemm/gemm_f64_sve_body.cpp', 'gemm_f64'],` + sve set `'eigen_gemm/gemm_double_dynamic_square_sve.cpp',`)
- [ ] **Step 4: 构建 + 本机 256/128 实跑**(`-e eigen_gemm_double_dynamic_square_sve -t 30 -n 1`;auto 维度求解较慢时加 `-O eigen_gemm_double_dynamic_square_sve.mdim=512`)+ 计数
- [ ] **Step 5: gem5 四腿**(`eigen_gemm_double_ds_sve_dual.cpp` 双 kernel)
- [ ] **Step 6: 回归 + 提交**(`test(eigen-sve): 新增 eigen_gemm_double_dynamic_square_sve——SVE 后端 double GEMM(四档 per-tier)`)

---

### Task 12: eigen_gemm_float_dynamic_square_sve(gemm_f32 body 首建)

**Files:** Create: `tests/cpu/eigen_gemm/gemm_f32_sve_body.cpp` + `gemm_float_dynamic_square_sve.cpp`;Modify: meson(body 表 +1、sve set +1)
- [ ] body 复制 Task 11 Step 1,改:`Matrix<float,…>`;`fill_rand` 用 `frandomf_scale(2.0f) - 1.0f`(f32 谱系);`solve_once` 的赋值转型 `(float)((double)(h % 2000u) / 1000.0 - 1.0)`(镜像 gemm_float_dynamic_square.cpp 的 gemm_solve_once f32 转型);pfx/NS 注释 `gemm_f32`;memcmp_or_fail 元素数不变(`(size_t)dim * (size_t)dim`,T=float 推导)。
- [ ] dispatcher 复制 Task 11 Step 2,改:test id/env `eigen_gemm_float_dynamic_square_sve`;pfx `gemm_f32`;`.fam = SDC_RSS_GEMM_F32`;`.bytes_per_elem = 4`;lo/hi 16/8192;其余同。
- [ ] meson;构建;本机两档;gem5 四腿;回归 + 提交。

---

### Task 13: eigen_gemm_cdouble_dynamic_square_sve(gemm_c128 body 首建)

**Files:** Create: `tests/cpu/eigen_gemm/gemm_c128_sve_body.cpp` + `gemm_cdouble_dynamic_square_sve.cpp`;Modify: meson(body 表 +1、sve set +1)
- [ ] body 复制 Task 11 Step 1,改:`Matrix<std::complex<double>,…>`;`fill_rand` 用 re/im 独立 `frandom_scale(2.0) - 1.0`(镜像 gemm_cdouble_dynamic_square.cpp 的 fill_rand——执行时打开该文件逐字比对谱系);`solve_once` 的 complex 构造:`a(r, col) = std::complex<double>((double)(h % 2000u) / 1000.0 - 1.0, (double)(h2 % 2000u) / 1000.0 - 1.0);`(b 同)——镜像其探针;`memcmp_or_fail(x.data(), l_prod.data(), (size_t)dim * (size_t)dim)`(T=complex<double> 推导,元素数口径)。
- [ ] dispatcher 复制 Task 11 Step 2,改:test id/env `eigen_gemm_cdouble_dynamic_square_sve`;pfx `gemm_c128`;`.fam = SDC_RSS_GEMM_C128`(表若无此条目则用防御 fallback 三元组 bytes=16/matrices=4/ratio=6.0,并在 commit message 注明——执行时先 `grep SDC_RSS_GEMM framework/sdc_stress_config.h` 确认表项存在与否,如实处理);`.bytes_per_elem = 16`。
- [ ] meson;构建;本机两档;gem5 四腿(complex 全档合法);回归 + 提交。

---

### Task 14: eigen_gemm_double14_sve(共享 gemm_f64 body)

**Files:** Create: `tests/cpu/eigen_gemm/gemm_double14_sve.cpp`;Modify: meson sve set +1
- [ ] dispatcher 复制 Task 11 Step 2,改:test id/env `eigen_gemm_double14_sve`;lo/hi `16/4096`(镜像 double14 原值);pfx 保持 `gemm_f64`(共享);描述 "Eigen GEMM payload (double, dynamic, square) on the ARM64 SVE packet backend…"。
- [ ] meson;构建;本机两档;gem5 复用 gemm_f64 腿;回归 + 提交。

---

### Task 15: 文档收尾与全量校对

**Files:**
- Modify: `README.md`(「测试用例与检测能力」计数链全量校对:SVE host PROD = 起始 486 − 1(bidiag)+ 8(SVD×4 PROD + GEMM×4 PROD)= 493;Jacobi×4 为 SKIP 级不计默认集——**以 `--list-tests` 实测为准**,逐用例 commit 记录的计数对账)
- Modify: `CLAUDE.md`(--list-tests 注释的 PROD 计数更新;"Eigen SVE pair" 相关表述更新为 12 用例统一表述;bidiag 相关条目清理——已知怪癖节中 `eigen_svd_bidiag_sve` 超时史与 SDC_RSS_BIDIAG 引用改写为历史注记)
- Modify: `docs/coding/writing_tests.md`(若提及 per-tier 模式,补 sandstone_eigen_sve_dispatch.h 用法一段)

- [ ] **Step 1: 全量计数对账**(12 新用例 + 删 1:`./builddir/sdcshield --list-tests` 与 `--quality=-1` 两遍计数,README 行与之一致)
- [ ] **Step 2: 三文档同步**(100% 与代码一致,规则 7)
- [ ] **Step 3: 终局回归**(`-e zstd19 -t 2000 -n 1` + `-e eigen_svd_cdouble_sve -t 30 -O eigen_svd_cdouble_sve.mdim=300 -n 1` + 全 12 新用例 256 档 30s 快扫)
- [ ] **Step 4: 提交** `docs(eigen-sve): SVE 统一支持收尾——README/CLAUDE.md 计数链与 per-tier 模式文档同步`,末行 DCO signoff。

---

## Self-Review 记录

- **Spec 覆盖**:spec §1 目标 1(12 用例)→ Task 2/4/5/6/7-10/11-14;目标 2(框架收敛)→ Task 1;目标 3(单独文件)→ 每任务 Create 明细;目标 4(四档)→ Global Constraints + 各任务 gem5 步骤;目标 5(bidiag 退役)→ Task 3;§5 sparse 排除→ 无任务(非目标,spec 已录);§7 文档→ Task 3/15。✓
- **Placeholder 扫描**:Task 4-14 的"复制 Task N 模板"均给出**全量差异参数表**(每参数具名),非"Similar to"式省略;模板本体全文在 Task 2/11。✓
- **类型一致性**:`EIGEN_SVE_BODY_NS` 宏名、`gemm_sve_data{int dim}`、pfx 表(`svd/svd_f32/svd_f64/svd_j_f32/svd_j_f64/svd_j_c128/gemm/gemm_f32/gemm_f64/gemm_c128`)全计划一致;库名 `tests_eigen_<pfx>_vl<t>` 与既有 `tests_eigen_{svd,gemm}_vl<t>` 兼容。✓
- **已知风险**:Task 13 `SDC_RSS_GEMM_C128` 表项存在性未在规划期验证(计划内含执行期 grep 与 fallback 处理);jacobi SVE 版 SKIP 级不进默认集,PROD 净变化 = −1 + 8 = +7(以实测对账为准)。

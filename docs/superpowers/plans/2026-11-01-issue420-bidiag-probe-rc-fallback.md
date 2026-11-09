# 2026-11-01 issue #420——校准探针返回码尊重 + 失败退守 lo（bidiag P0-2 复发路径封堵）

> **For agentic workers:** 本计划是缺陷修复单元。执行按 CLAUDE.md rule 2 全量
> 自验；Task 1 一 commit（修复+回归测试同单元），Task 2 为 gem5-fi 证据链，
> Task 3 文档同步一 commit，Task 4 收尾（推分支/PR/issue 回帖）。
> 本会话为 task-loop 派发的无人值守 agent，按 executing-plans 纪律内联执行。

**Goal:** `sdc_calibrated_dim` / `sdc_joint_dim_slice` 尊重 `solve_once` 的
失败返回码（非零）——任一档探针自报失败即时间模型不可信，退守 `lo`，
绝不再把 μs 级失败分支噪声当作超快求解外推、把 mdim 推向 `n_mem`/hi
上限（P0-2 超时复发路径，issue #420）。

**Architecture:** 框架层（`framework/sdc_stress_config.h`）双位点修复：
(1) `sdc_calibrated_dim` 的 probe lambda 检查 `solve_once` 返回码，非零 →
返回 **-1.0 负哨兵**计时（与"计时恰好为 0"的既有语义区分），负哨兵触发
新守卫退守 `lo`（绝不 hi——失败 ≠ 『时间不是约束』）；(2)
`sdc_joint_dim_slice` 用记录失败标志的包装 lambda 传探针，任一档失败 →
`n = lo`（`n_mem` 弃用），统一三种探针的失败教义（Eigen 族 bad_alloc
上抛 → 调用方 catch → lo；bidiag 返回码 → 框架层 → lo）。测试侧仅更新
被 issue 证伪的旧兜底注释；新增 gtest 单元测试锁定契约。

**Tech Stack:** C++23（框架头模板/lambda）、gtest（unittests 目标）、
gem5-fi SE O3（`--sve_vl {1,2,4,8}`，本机无 SVE——规则 2 强制仿真验证）。

**Spec:** issue #420（标题+正文全文，`gh issue view 420`）；
`docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.5（本
计划在其基础上收口探针失败契约）。

## Global Constraints

- 规则 1：一个补丁一个单元——Task 1（修复+单测+注释）一 commit，Task 3
  （文档）一 commit，绝不捆绑。
- 规则 2：构建零新警告（`./scripts/build.sh`，unittests 目标单独
  `./scripts/build.sh unittests`）；受影响行为跑通并引用真实输出；
  回归 `-e <受影响测试> -t 2000 -n 1` exit: pass；x86-64 非回归逐调用方
  核查（纯决策层改动如实声明）；本机无 SVE → bidiag 原生 SKIP，须 gem5-fi
  四档 SVE 仿真验证并引用真实 run 输出（tier/checksum/退出码）。
- 规则 3：`git commit -s`，DCO 最后一行 `Signed-off-by: wangxu
  <wangxumarshall@qq.com>`；只推 `fix/issue-420`，绝不推 main。
- 规则 5：x86-64 非回归——`sdc_calibrated_dim` 是共享模板，全部既有
  调用方探针（eigen_sparse lambda / SVD sizing_probe / 8×GEMM
  solve_once / bidiag）成功一律 `return 0`、失败一律 Eigen 异常上抛
  （bidiag 除外），`!= 0` 谓词对既有 x86/ARM 调用方恒假——行为不变。
- 红线（task-loop）：gem5-fi 树（cwd 之外）一律只读——工作负载与
  m5out 全部落 /tmp，gem5.opt/config 以绝对路径引用。

## 缺陷机制（已实证复现，非读码推断）

复现 harness（`/tmp/repro420/repro420.cpp`）直接 include **未修改的**
`framework/sdc_stress_config.h`，链接真实 `sdc_numa.cpp.o`，以
`run_bidiag_once` 失败契约（分配失败 → log_warning → `return -1`，不求解）
驱动真实产品函数。2026-11-01 本机（128 核/30GB/4 NUMA 节点/无 SVE）实测：

```
== 场景 1: sdc_calibrated_dim(64, 2048, per_solve=10000, k_floor=3.0, 全档失败探针) ==
[LOG] W> [时间校准] ... 单次求解预算 10000ms (试解 2048→0.0ms, 2048→0.0ms, k=3.00) → 2048 (验证 0.0ms)
SCENARIO1 dim=2048 fit.a_ms=2.69e-13 fit.p=3
== 场景 2: sdc_joint_dim_slice(bidiag 原参数, 全档失败探针) ==
[LOG] W> [联合求解] ... n_mem=1900 n_run=333298 AR=10.0 c=32.1 → 1900 (预算 10.4GiB)
SCENARIO2 n=1900
== 场景 3: 顶档(2048)瞬态失败 ==
SCENARIO3 n=1900 (n_run=224026)
```

与 issue #420 的推导逐字吻合：探针 rc 被丢弃 → 失败分支 μs 级**非零**噪声
计时 → `t<=0` 守卫进不去 → `k=log2(t2/t1)≈0` 被 `k_floor=3.0` 钳制 →
回退 `a = t1/p1^k ≈ 1e-13` 的"合法 fit" → `n_run` 失控（issue 场景
2147483647）→ `n = min(n_mem, hi)` 落内存上限（本机 n_mem=1900，大内存机
2048）→ 全核单次迭代分钟级 → 300s SIGQUIT（P0-2 复发）。

## 修复方案（读码 + 全调用方盘点）

**调用方盘点（x86 非回归依据，2026-11-01 逐一核实）：**

| 调用方 | solve_once | 成功返回 | 失败形态 |
|---|---|---|---|
| eigen_sparse（lambda probe） | `return 0` | 0 | Eigen bad_alloc 上抛 → 测试层 catch → lo |
| SVD 十模板 + SVE 四档 body（sizing_probe） | `return 0` | 0 | 同上 |
| GEMM 八文件（gemm_solve_once/solve_once） | `return 0` | 0 | 同上 |
| **eigen_svd_bidiag_sve（run_bidiag_once）** | **0 / -1** | 0 | **返回码 -1，被 probe 丢弃 → 本缺陷** |

修复后契约（写进 `sdc_calibrated_dim` 文档注释）：`solve_once` 返回 0 =
成功；非零 = 自报失败（如分配失败）——probe 计时取负哨兵，任一档失败
退守 `lo`。三种探针失败教义统一为 lo。

## Tasks（Task 1/3 各一 commit；Task 2/4 不产码）

### Task 1: 框架修复 + 单元测试 + bidiag 注释更新

**Files:**
- Modify: `framework/sdc_stress_config.h:704-735`（probe lambda、抬升循环、
  新负哨兵守卫）、`:757-777`（验证探针负值的日志披露）、`:1115-1159`
  （联合层包装探针 + n=lo 退守）、`:655-689`（文档注释补返回码契约）
- Modify: `tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp:382-394`
  （被 issue 证伪的旧兜底注释 → 新契约表述）
- Create: `framework/unit-tests/sdc_stress_config_tests.cpp`
- Modify: `framework/meson.build:368-383`（unittests_sources 登记新测试）

**Interfaces:**
- Consumes: `sdc_calibrated_dim(lo, hi, max_single_ms, test_id, knob,
  k_floor, solve_once, fit*)`、`sdc_joint_dim_slice(test, knob, ja*,
  solve_once)`（既有签名，零变化）
- Produces: solve_once 返回码契约（0/非零）；探针失败时
  `sdc_calibrated_dim` 返回 lo 且 `fit` 保持 {0,0}；
  `sdc_joint_dim_slice` 返回 lo；gtest 用例
  `SdcCalibratedDim.ProbeFailureFallsBackToLo` 等

- [x] **Step 1.1: 写失败的单元测试**（TDD 红）

```cpp
/* framework/unit-tests/sdc_stress_config_tests.cpp — issue #420 契约锁定 */
#include <gtest/gtest.h>
#include "sdc_stress_config.h"
#include <chrono>

/* 本 TU 只触达 sdc_calibrated_dim（联合层由 /tmp harness 与 gem5-fi
 * 证据链覆盖），其外部依赖仅 log_warning 宏展开所需的 thread_num 与
 * log_message（sandstone.h 声明）——在此提供 unittests 域定义，
 * 与 sandstone_unittests_utils.cpp 的 dummy 模式同款。 */
thread_local int thread_num = -1;
void log_message(int, const char *msg, ...) { (void)msg; }

/* run_bidiag_once 失败分支的忠实替身: burn ~50μs 后 return -1 ——
 * 失败分支的计时是 μs 级非零噪声(issue #420 的 "仍测得 μs>0"),
 * 保证修复前本测试确定性变红(dim=hi)而非依赖时钟巧合。 */
static int probe_fail(int)
{
    auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::microseconds(50))
        ;
    return -1;
}

/* 顶档失败形态(瞬态内存压力: 大分配失败小分配成功) */
static int probe_top_fail(int n)
{
    if (n >= 1024)
        return probe_fail(n);
    return 0;
}

/* 健康探针: 确定性 ~50μs "求解" —— 锁健康路径不受修复影响 */
static int probe_ok(int)
{
    auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - t0 < std::chrono::microseconds(50))
        ;
    return 0;
}

TEST(SdcCalibratedDim, ProbeFailureFallsBackToLo)
{
    struct sdc_fit fit = { -1.0, -1.0 };   /* 非法初值: 早退须写 {0,0} */
    int dim = sdc_calibrated_dim(64, 2048, 10000, "ut420", "mdim", 3.0,
                                 probe_fail, &fit);
    EXPECT_EQ(dim, 64);                    /* 修复前: 2048(垃圾外推) */
    EXPECT_EQ(fit.a_ms, 0.0);              /* fit 无效 → 消费方跳过 n_run */
    EXPECT_EQ(fit.p, 0.0);
}

TEST(SdcCalibratedDim, ProbeFailureAtEscalatedTierFallsBackToLo)
{
    int dim = sdc_calibrated_dim(64, 2048, 10000, "ut420", "mdim", 3.0,
                                 probe_top_fail, nullptr);
    EXPECT_EQ(dim, 64);                    /* 修复前: 2048 */
}

TEST(SdcCalibratedDim, HealthyProbeUnchanged)
{
    struct sdc_fit fit = { 0.0, 0.0 };
    int dim = sdc_calibrated_dim(64, 2048, 10000, "ut420", "mdim", 3.0,
                                 probe_ok, &fit);
    /* 全档 ~50μs → 抬升到 2048, k→k_floor, dim 钳 hi; fit 有效 */
    EXPECT_EQ(dim, 2048);
    EXPECT_GT(fit.a_ms, 0.0);
    EXPECT_DOUBLE_EQ(fit.p, 3.0);
}
```

meson 登记：`framework/meson.build` unittests_sources += ` 'unit-tests/sdc_stress_config_tests.cpp',`

- [x] **Step 1.2: 跑测试确认红**

Run: `./scripts/build.sh unittests && ./builddir/unittests --gtest_filter='SdcCalibratedDim.*'`
Expected: 前 two用例 FAIL（dim=2048 ≠ 64），HealthyProbeUnchanged PASS。

- [x] **Step 1.3: 实现框架修复**

`sdc_calibrated_dim` probe lambda（:704-709）：
```cpp
    auto probe = [&](int n) -> double {
        auto t0 = std::chrono::steady_clock::now();
        if (solve_once(n) != 0)
            return -1.0;      /* issue #420: 探针自报失败 → 负哨兵计时
                                 (区别于"计时恰好为 0"的不可测语义) */
        return std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - t0).count();
    };
```

抬升循环（:714-719）加 `t1 >= 0.0 &&`（失败哨兵停止无谓抬升——失败探针
恒"快"，否则会一路探到 hi 才被守卫拦下）。

新守卫（:723 之前、mid 探测之后）：
```cpp
    if (t1 < 0.0 || t2 < 0.0) {
        /* issue #420: 探针自报失败(返回码非零, 如 bidiag run_bidiag_once
         * 分配失败返回 -1)——失败分支的 μs 级计时噪声曾被当作超快求解
         * 外推(k 钳制后 dim=hi, P0-2 超时复发路径)。失败 ≠ 『时间不是
         * 约束』(两者皆恰为 0 才是), 绝不交还 hi; 与调用方 catch(...)
         * → lo 的教义同向, 退守 lo。 */
        log_warning("[时间校准] %s.%s: 试解探针自报失败(试解 %d→%.2fms, "
                    "%d→%.2fms) → 退守校准下限 %ld",
                    test_id, knob, p1, t1, mid, t2, (long)lo);
        return lo;
    }
```

验证探针负值披露（:772-777 末尾 log 的三元链）：`td < 0.0` 时后缀
`", 验证探针失败(未复核)"`（直接调用方保留外推 dim——其 Eigen 分配失败
自会上抛由调用方 catch 兜底；联合层见下,更保守）。

`sdc_joint_dim_slice`（:1116-1148）：
```cpp
    int n_run = 0;
    bool probe_failed = false;    /* issue #420: 任一档探针自报失败 */
    if (solve_once && a->run_budget_ms > 0 && n_mem > a->lo) {
        ...
        /* issue #420: 探针失败教义统一。Eigen 族探针分配失败以异常上抛,
         * 由调用方 catch(...) → lo 兜底; 返回码上抛的探针(如 bidiag
         * run_bidiag_once 的 -1)此前被 probe lambda 静默丢弃, μs 级
         * 噪声计时被当作超快求解外推, n_run 失效 → mdim 落 n_mem/hi
         * (P0-2 复发路径)。包装探针记录失败: 任一档失败(含验证档——
         * 该规模的分配已在失败, 继续放大工作集只会更糟)即时间模型
         * 不可信, 联合求解退守 lo, n_mem 弃用。 */
        auto solve_probe = [&](int n) -> int {
            int rc = solve_once(n);
            if (rc != 0)
                probe_failed = true;
            return rc;
        };
        struct sdc_fit fit;
        sdc_calibrated_dim(a->lo, a->hi, per_solve,
                           test ? test->id : "(global)", knob, 3.0,
                           solve_probe, &fit);
        ... (fit→n_run 原样) ...
    }
    int n = n_mem;
    if (probe_failed) {
        log_warning("[联合求解] %s.%s: 校准探针自报失败 → 退守 lo=%d "
                    "(时间模型不可信, n_mem=%d 弃用; issue #420)",
                    test ? test->id : "(global)", knob ? knob : "?",
                    a->lo, n_mem);
        n = a->lo;
    } else {
        if (n_run > 0 && n_run < n) n = n_run;
        if (a->hi < n) n = a->hi;
        if (n < a->lo) n = a->lo;
    }
```

bidiag 测试注释（eigen_svd_bidiag_sve.cpp:386-394 段）改为新契约表述
（原"由 init 的降档重试/资源性 SKIP 兜底覆盖"论证被 issue 以瞬态内存
压力证伪——探针期失败、init/run 期成功时旧路径把 mdim 推到上限）。

- [x] **Step 1.4: 单测转绿**

Run: `./scripts/build.sh unittests && ./builddir/unittests --gtest_filter='SdcCalibratedDim.*'`
Expected: 3/3 PASS。（既有 thermal 用例在本机无 thermal zone 失败为
pre-existing，与本单元无关，如实记录。）

- [x] **Step 1.5: /tmp harness 复核（受影响行为真实输出）**

Run: 重编 `/tmp/repro420/repro420`（同 19:14 命令）并运行。
Expected: SCENARIO1 dim=64（fit {0,0}）、SCENARIO2/3 n=64，日志含
`试解探针自报失败 → 退守校准下限` 与 `校准探针自报失败 → 退守 lo`。

- [x] **Step 1.6: 构建零新警告 + 原生回归**

Run: `./scripts/build.sh`（零新警告）；`./builddir/sdcshield -e eigen_sparse -t 2000 -n 1`（原生联合求解消费方——走修改后的 probe lambda/联合层, [时间校准] 日志口径不变、exit: pass）；`./builddir/sdcshield -e zstd19 -t 2000 -n 1`（标准回归）。
Expected: 双 pass；`-e eigen_svd_bidiag_sve --quality=-1` SKIP（本机无 SVE, 如实声明）。
x86 非回归：调用方盘点表全部 `return 0`/异常上抛 → `!= 0` 恒假（检视级论证, 写入 PR）。

- [x] **Step 1.7: Commit**

```bash
git checkout -B fix/issue-420 origin/main   # (若未建)
git add framework/sdc_stress_config.h tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp \
        framework/unit-tests/sdc_stress_config_tests.cpp framework/meson.build
git commit -s -m "fix(sdc-stress): 校准探针返回码尊重——失败退守 lo, 封堵 bidiag mdim→上限的 P0-2 复发路径 (#420)

<根因/改动/验证证据摘要>"
```

### Task 2: gem5-fi SVE 四档验证（规则 2；不产码、不 commit）

**Files:** Create(临时, /tmp): `/tmp/repro420/gem5_420.cpp` —— 从本仓**真实**
`framework/sdc_stress_config.h` 静态编译的双场景工作负载：
- 场景 A（失败契约）：`sdc_joint_dim_slice`(bidiag 原参数, 失败探针) → 断言 n==lo；`sdc_calibrated_dim`(失败探针) → 断言 dim==lo
- 场景 B（SVE 健康路径）：真实 SVE svmla/svaddv 探针（n 缩放的工作量）→ dim∈[lo,hi]、fit 有效
- 输出：VL bits（`svcntw()*32`）、场景结果、FNV-1a 校验和、`SDCSHIELD_FAILS` 计数、断言失败即非零退出
- 参数取小档（lo=16, hi=256）控制仿真时长；stub 层同 /tmp harness（thread_num/log_message/quick_run/线程数/knob/反馈中位数），NUMA 链真实 `framework/sdc_numa.cpp` 编入。

- [x] **Step 2.1: 构建工作负载** `g++ -static -O2 -march=armv8-a+sve ... gem5_420.cpp framework/sdc_numa.cpp`（本机仅编译不运行——SIGILL 属预期）
- [x] **Step 2.2: 四档运行**（gem5-fi 只读, outdir 落 /tmp）：
```bash
for V in 1 2 4 8; do
  /home/sdc/wangxu/operator/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt \
    --outdir=/tmp/repro420/m5out-420-vl$V \
    /home/sdc/wangxu/operator/gem5-fi/configs/se/arm_chaos.py \
    --cmd /tmp/repro420/gem5_420 --sve_vl $V
done
```
Expected: 每档 `VL_bits={128,256,512,1024}`、双场景断言通过、`SDCSHIELD_FAILS=0`、gem5 exit 0。
- [x] **Step 2.3: 诚实边界声明**：gem5 SE 无法优雅制造 aligned_alloc 失败
  （分配超限为模拟器 panic 而非 NULL 返回）→ 失败路径在 gem5 内以**真实
  产品代码 + run_bidiag_once 失败契约替身**验证（工作负载内断言）；
  失败路径的原生机制层实证 = /tmp harness（Step 1.5）。

### Task 3: 文档同步（规则 7；一 commit）

**Files:**
- Modify: `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.5 bidiag 条目——补 issue #420 收口注记（solve_once 返回码契约 + 失败退守 lo）
- Modify: `docs/research/progress-log.md`——issue #420 结案条目（根因/修复/验证证据链/PR 链接）
- Modify: 本计划文件勾选全部 checkbox
（README 的联合求解描述不含探针失败教义断言, 无事实变化, 不改——判断依据写入 PR。）

- [x] **Step 3.1: 三处文档更新** → **Step 3.2: 复核引用的数字/链接与实测一致** → **Step 3.3: Commit** `docs(sync): issue #420 …`

### Task 4: 收尾（推分支/PR/issue 回帖；无 commit）

- [x] **Step 4.1:** `git push -u origin fix/issue-420`（绝不推 main）
- [x] **Step 4.2:** `gh pr create --base main --head fix/issue-420 --title "fix: 校准探针返回码尊重——bidiag 探针失败退守 lo, 封堵 P0-2 超时复发 (#420)" --body "Closes #420 + 报告"`（若 auto-pr 抢先 → `gh pr edit fix/issue-420`）
- [x] **Step 4.3:** issue #420 回帖：根因、改动、验证证据（harness 前后对比、单测、原生回归、gem5 四档）、PR 链接
- [x] **Step 4.4:** 工作区回到干净 main：`git checkout main && git status` clean

## Self-Review

- 覆盖：issue 三要点（rc 丢弃/t<=0 守卫失效/三种失败教义不统一）分别由
  Step 1.3 probe 检查、负哨兵 vs 0 语义区分、联合层退守 + 注释统一表述
  覆盖；P0-2 复发路径由 harness 前后对比实证。
- 占位符：无（全部代码/命令完整给出）。
- 类型一致：`sdc_fit`、lambda 签名、gtest 用例名跨任务一致。

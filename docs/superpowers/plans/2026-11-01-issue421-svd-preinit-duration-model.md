# Issue #421: svd_cdouble_sve preinit desired_duration 模型修复 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `eigen_svd_cdouble_sve` preinit 写入的 `desired_duration` 从
「无 AR、无 ×2 的单线程单解拟合」改成「AR×2×t(n_hi) 再与 child n_run 的
run_budget 取 min」的全核单迭代紧上界,消除 issue #421 指出的大内存/少核
主机上 do-while 未预算多迭代膨胀。

**Architecture:** 单文件最小改动(`tests/cpu/eigen_svd/svd_cdouble_sve.cpp`)。
preinit 公式补齐 child 单次迭代的真实口径(两解 prime+DUT × 全核 slowdown
AR,与 `sdc_joint_dim_slice` 的 n_run 校准方程 `run_budget_ms = AR×2×t(n_run)`
同构),再用同一 run_budget 做 cap——min 结构一阶复刻 child 的
`min(n_mem, n_run)`:n_run 主导的大内存主机单次被预算钉在 140 s,
内存主导的小内存主机单次 ≈ AR×2×t(n_hi)。run_budget 数值提取为文件级宏,
child(`ja.run_budget_ms`)与 preinit 同源,防两处分写漂移(0.2055 字面量
教训,b249c97c)。

**Tech Stack:** C++20(AArch64 测试 TU),meson/ninja 构建,sdcshield 框架
(`sdc_stress_config.h` 的 `sdc_budget_bytes`/`sdc_rss_table_c`/
`sdc_total_threads`/`sdc_ar_factor`,`sdc_numa.h` 的 `sdc_numa_probe`)。

**Spec:** issue #421(audit:f7703e6459e3be44)+ 本文档「根因分析」节。

## Global Constraints

- x86-64 非回归:改动全部在 `#if defined(__aarch64__)` 内,纯 ARM64 增量
  (CLAUDE.md 规则 5,inspection 验证)。
- 一个补丁一个单元:本计划产出**单个 commit**(规则 1)。
- 构建零新警告(`./scripts/build.sh`)(规则 2a)。
- `ja.run_budget_ms` 的**数值不得变**(140000 → 宏,等值替换),child 侧
  sizing 行为零改动。
- 注释口径必须同步订正:原注释「keeps the parent's timing expectation at or
  above the child's choice, so exactly one run iteration executes」方向
  写反了(TEST_LOOP 语义是时间有余裕才继续,desired ≥ 单次 ⟹ ≥2 迭代;
  恰一次的正确条件是 desired ≤ 单次)。
- 本机(128 核 firestorm,4 NUMA,32 GB,**无 SVE**)只能验证 preinit 数值
  与 SKIP 路径;do-while 迭代行为 = 未改框架逻辑 + 数值实证,如实声明边界。

## 根因分析(Phase 1 结论,实测编号见「复现」节)

1. **child 单次迭代真实口径** = AR×2×t(n)(run 里 prime+DUT 两次 BDCSVD,
   全核并发再乘 all-core slowdown)。`sdc_joint_dim_slice` 的 n_run 校准
   方程即 `run_budget_ms = AR表×2×t_local(n_run)`(sdc_stress_config.h:1119-1141)。
2. **preinit 旧公式** `desired = 0.95×t(n_hi)`(243.4×(n_hi/4400)³+1 的
   x3b 单线程单解拟合):比 child 单次迭代少 AR×2 因子。因为 TEST_LOOP 的
   do-while 以 desired 为时间槽,desired > 单次 ⟹ 开启未预算的下一次迭代。
3. **失效面**(单次/desired 比 R = AR×2×(t(n_child)/t(n_hi))/0.95):
   - 大内存全核(≥512 GB/节点):n_hi>3650 → 旧 desired 133~381 s 超过
     单次 140-218 s → 2-3 次迭代,总时长膨胀至 436 s+(issue 主场景)。
   - 少核/单线程(-n 1):n_hi 被 workers=1 推到 MDIM_MAX=6000,旧 desired
     = 587 s ≈ **25×** 单次(~23 s)→ ~25 次未预算迭代(本机实测复现)。
   - 小内存全核(本机 32 GB):n_hi=895,旧 desired 2.9 s << 单次 30.5 s
     → 恰一次(歪打正着,但 desired 字段语义失真 10×)。
4. **已修复部分**(issue 指控、但 main 已含):127 硬编码已由 b249c97c 改
   `sdc_total_threads()`(svd+gemm 两处);gemm 公式本就含 AR×2。
   issue 的「SIGQUIT 跨过 300 s floor」推导在 timeout = max(5×desired+30, 300)
   结构下过强(desired>单次 ⟹ 总 ≤ 2×desired < timeout;desired<单次 ⟹
   总 ≈ 单次 ≤ 218 s 现实域)——真实缺陷是时长膨胀而非必然 SIGQUIT;
   但「未预算的第二/三次迭代」核心指控成立(上面 -n 1 与大内存两档)。
5. **不能只加 AR×2**(issue 字面对齐 gemm 的做法):参考机上
   AR×2×t(n_hi) = 50×140 s ≈ 7020 s,desired 会放大 50×,测试将跑满
   desired(do-while)→ 31 次迭代、1.85 h——比现状更糟。必须同时用
   run_budget cap:min(RUN_BUDGET, AR×2×t(n_hi)) 紧贴单次迭代。

## 复现(修复前,本机实测,2026-11-01,临时插桩 stderr 已撤销)

```
$ ./builddir/sdcshield -e eigen_svd_cdouble_sve -t 2000 -n 1   # preinit(parent)
REPRO421: budget_kb=21107377 workers=1 c_kb=0.2055 n_hi=6000 iter_s=618.19 desired_duration=587277 ms
$ ./builddir/sdcshield -e eigen_svd_cdouble_sve -t 2000        # 全核
REPRO421: budget_kb=21107377 workers=128 c_kb=0.2055 n_hi=895 iter_s=3.05 desired_duration=2896 ms
```

-n 1 档 desired 587 s vs child 单次 ~23 s(AR5 口径 n_run≈1583,2×t(1583))
= 25× 未预算迭代预算。全核档(小内存)恰一次(2.9 s < 30.5 s)。
注:preinit 的 log_* 会被框架 truncate(sandstone.cpp:218 `truncate_log`),
临时插桩用 stderr 直出;正式修复**不加 preinit 日志**(与 gemm preinit
一致,观测走下方「数值验证」的推导+本报告)。

## 修复后数值预期(验证对照表)

| 场景 | 旧 desired | 新 desired | child 单次(模型) | 迭代 |
|---|---|---|---|---|
| 本机 -n 1(workers=1,n_hi=6000,AR=5) | 587 s | 0.95×min(140, 5×2×618) = **133 s** | ~23 s | 25 → ~6 次(残余 5.8× 来自 AR 表全机口径在 -n 1 高估,principled fix 在 sdc_numa.cpp,超范围如实声明) |
| 本机全核(128w,n_hi=895,AR=5) | 2.9 s | 0.95×min(140, 5×2×3.05) = **29 s** | ~30.5 s | 1 → 1(行为等价,非回归) |
| 参考机 512 GB 全核(n_hi≈3656,AR=25) | 133.6 s | 0.95×min(140, 7020) = 133 s | 140-218 s | 1 → 1(等价) |
| 1 TB 全核(n_hi≈5193,AR=25) | 381 s | **133 s** | 140-218 s | 2-3 → 1(**修复目标**) |

恒等式:新 desired ≤ 0.95×140 = 133 s ⟹ timeout = max(5×133+30, 300) = 695 s
≥ 单次(≤218 s 现实域)⟹ 无 SIGQUIT 面;min 结构保证 desired ≤ 单次
⟺ AR表 ≤ AR真实/0.95(表低估是实测常态 39 vs 25)。

---

### Task 1: preinit 公式修复 + RUN_BUDGET_MS 宏同源化

**Files:**
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:124-190`(宏区 + preinit)
- Modify: 同文件 `sve_probe_and_init` 内 `ja.run_budget_ms = 140000` → 宏引用

**Interfaces:**
- Consumes: `sdc_ar_factor(SDC_FAM_MEM_BOUND, sdc_numa_probe())`(framework/sdc_numa.h:33,
  gemm preinit 同款调用,preinit 阶段可用);`sdc_budget_bytes`/`sdc_rss_table_c`/
  `sdc_total_threads`(不变)。
- Produces: `#define RUN_BUDGET_MS 140000`(文件级,MDIM_MAX 之后);
  preinit 写入的新 desired_duration 语义(全核单迭代紧上界)。

- [ ] **Step 1: 实现(编辑 1 — 宏与注释)**

在 `#define MDIM_MIN 300 / #define MDIM_MAX 6000` 之后加:

```c
/* 全核单迭代预算(ms): child 的 n_run 校准(ja.run_budget_ms, 方程
 * run_budget = AR×2×t(n_run))与 preinit 的 desired_duration 上界共用
 * 同一数值——两处分写会静默漂移(0.2055 字面量的同型教训, b249c97c)。
 * 140 s 是表 AR=25 的口径, 参考机实测最坏 child AR≈39 时 ≈218 s
 * (见 sve_probe_and_init 的 run_budget_ms 注释)。 */
#define RUN_BUDGET_MS 140000
```

- [ ] **Step 2: 实现(编辑 2 — preinit 公式)**

`size_matrix_preinit` 中,把

```c
    double iter_s = 243.4 * std::pow((double)n_hi / 4400.0, 3.0) + 1.0;
    test->desired_duration = (int)(iter_s * 950.0);   /* ms, -5% margin */
```

替换为:

```c
    /* 全核单次迭代 = AR×2×t(n)(run 侧 prime+DUT 两解 × 全核 slowdown),
     * 与 child n_run 的校准方程(run_budget = AR×2×t(n_run),
     * sdc_joint_dim_slice)同口径; 再与 RUN_BUDGET 取 min——min 结构
     * 一阶复刻 child 的 min(n_mem, n_run): n_run 主导的大内存主机
     * 单次被预算钉在 RUN_BUDGET, 内存主导的小内存主机单次 ≈
     * AR×2×t(n_hi)。旧公式只有 0.95×t(n_hi)(单线程单解、无 AR 无
     * ×2), 在大内存(n_hi>~3650)与少核(-n 1 把 n_hi 推到 MDIM_MAX)
     * 主机上超过单次, do-while 开出未预算的第二~二十五次迭代
     * (issue #421; 本机 -n 1 实测 desired 587 s vs 单次 ~23 s)。
     * 单加 AR×2 不取 min 同样错误: 参考机口径 AR×2×t(n_hi)≈7020 s,
     * 测试会跑满 desired(31 次迭代)。 */
    double ar = sdc_ar_factor(SDC_FAM_MEM_BOUND, sdc_numa_probe());
    double iter_s = ar * 2.0 * (243.4 * std::pow((double)n_hi / 4400.0, 3.0) + 1.0);
    if (iter_s > (double)RUN_BUDGET_MS / 1000.0)
        iter_s = (double)RUN_BUDGET_MS / 1000.0;
    test->desired_duration = (int)(iter_s * 1000.0 * 0.95);   /* ms, -5% margin */
```

- [ ] **Step 3: 实现(编辑 3 — child 侧宏引用,等值)**

`sve_probe_and_init` 的 `struct sdc_joint_args ja` 中:

```c
            .run_budget_ms = 140000,       /* worst-child iteration budget;
```

改为(保留原注释主体,数值换宏):

```c
            .run_budget_ms = RUN_BUDGET_MS, /* worst-child iteration budget;
```

- [ ] **Step 4: 实现(编辑 4 — 订正反向注释)**

preinit 注释块中

```
 * The preinit only writes an UPPER bound:
```

一段之后的 `dur = t(n_hi) × 0.95, t(N) = 243.4s·(N/4400)³ + 1s  (x3b fit,
 * within ±2% of the measured points at N>=2400).` 更新为 min 模型描述;
并把 DECLARE_TEST 前注释与 @parblock 顶部 `it only writes a desired_duration
UPPER bound from the same memory ceiling` 的「上界」表述改为「全核单迭代
紧上界(AR×2×t(n_hi) 与 run_budget 取 min)」。明确写出恰一次迭代条件是
**desired ≤ 单次**(TEST_LOOP 时间有余裕才继续),替换原「at or above the
child's choice, so exactly one run iteration executes」的反向表述。

- [ ] **Step 5: 构建**

Run: `ninja -C builddir`(增量;最终验证用 `./scripts/build.sh`)
Expected: 零 warning,链接成功。

- [ ] **Step 6: 数值验证(本机,-n 1 与全核)**

临时把 Step 2 的公式结果用 stderr 插桩打印(同复现方法),跑:

```
./builddir/sdcshield -e eigen_svd_cdouble_sve -t 2000 -n 1   # 期望 desired ≈ 133000 ms
./builddir/sdcshield -e eigen_svd_cdouble_sve -t 2000        # 期望 desired ≈  28976 ms(0.95×5×2×3.05)
```

对照「修复后数值预期」表;确认后**撤销插桩**,重建。

- [ ] **Step 7: 行为验证(本机可运行面)**

```
./builddir/sdcshield -e eigen_svd_cdouble_sve -t 2000 -n 1   # result: skip (CpuNotSupported, "test compiled with sve") — 与修复前一致
```

- [ ] **Step 8: 全量验证(CLAUDE.md 规则 2)**

- `./scripts/build.sh` clean 零新警告。
- 回归:`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- x86-64 非回归:diff 全部位于 `#if defined(__aarch64__)` 内(inspection)。
- 验证边界声明:do-while 迭代次数未在本机直接观测(本机无 SVE,测试
  SKIP);依据 = TEST_LOOP 框架语义(未改)+ desired 数值实证(≤ 单次迭代
  模型值)+ child 侧零改动(ja.run_budget_ms 等值)。preinit 本身在本机
  完整真实执行(非 SKIP 路径)。

- [ ] **Step 9: 文档同步评估**

改动是单测试的预算模型修正,不改变 README/CLI/架构面——按规则 7 评估为
非大颗粒度,不需要 README/docs 更新;本计划文件即过程文档。

- [ ] **Step 10: Commit(单补丁)**

```bash
git add tests/cpu/eigen_svd/svd_cdouble_sve.cpp docs/superpowers/plans/2026-11-01-issue421-svd-preinit-duration-model.md
git commit -s -m "fix(eigen): svd_cdouble_sve preinit desired_duration 补 AR×2 口径并按 run_budget 封顶

issue #421: ..."
```

DCO 末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,其后无内容。

### Task 2: 推送 / PR / 报告

- [ ] `git push -u origin fix/issue-421`(绝不推 main)。
- [ ] `gh pr create --base main --head fix/issue-421 --title "fix: svd_cdouble_sve preinit desired_duration 补 AR×2 并按 run_budget 封顶 (#421)" --body "Closes #421 + 报告"`(若 auto-pr.yml 抢先则 `gh pr edit fix/issue-421`)。
- [ ] 报告贴 issue #421(根因/改动/验证证据/PR 链接/验证边界)。
- [ ] 工作区回干净 main。

## Self-Review

- 覆盖:issue 三项指控逐一处置——「无 AR×2」→ Step 2;「127 硬编码」→ 已被
  b249c97c 修复(报告说明);「gemm 同源缺陷」→ gemm 现状已含
  sdc_total_threads + sdc_numa_probe + AR×2(报告说明,不改)。
- 无占位符:所有编辑给出完整代码。
- 一致性:RUN_BUDGET_MS 宏名在 Step 1/3 一致;数值 140000 等值替换。

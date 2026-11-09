# RSS 表 c 断言勘误（issue #511）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修正 `s_rss_ratio_table` 表头注释与 spec 副本中 "c 是算法内在属性(矩阵驻留数×元素大小), 跨机可移植" 的过强断言，如实记录差分口径漏计项与实测残差（issue #511 第三论点）。

**Architecture:** 纯注释勘误，零行为变更。两处同一断言的拷贝同步修正：`framework/sdc_stress_config.h` 表头注释（真源）+ `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.3 副本（带勘误标记保留历史值）。表值 0.0315 与行为（联合求解、T7 反馈闭环）不动——本机实测（+3.3~4.0%）在反馈闭环收敛半径与 MemAvailable×0.9 安全垫内，单机单样本不构成重标定依据。

**Tech Stack:** C++ 注释（C 风格块注释）、Markdown spec。

**Spec:** `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md`（§4.3 RSS_RATIO_TABLE、§4.4 反馈闭环、§6 验证矩阵 P1 行）

## Global Constraints

- issue #511 三论点中：论点 1（表值 0.0290 低估 ~9%）与论点 2（判据未达被当 pass）的**行为缺陷**已由 09c94fc6（0.0290→0.0315，已合 main）修复；本计划只修论点 3 的注释断言残留。
- git blame 铁证：表头注释自 f84966bc5（2026-10-27）未改，09c94fc6 只改了表内 GEMM_F64 行。
- 一个补丁一个单元：两处注释同属"同一断言的同步勘误"，一个 commit。
- DCO：`git commit -s`，Signed-off-by 最后一行。
- 纯注释改动 → x86-64 零变化（无代码路径触碰）；行为验证用本机可跑的 `eigen_gemm_double_dynamic_square`（与 SVE 版 max_arm 共用 `SDC_RSS_GEMM_F64` 表项 + 联合求解，见 gemm_double_dynamic_square.cpp:88）；max_arm 本机 SKIP（无 SVE），无功能性变更故 gem5-fi 不适用——PR 中如实声明此边界。

## 证据基线（注释新文本引用的全部数字，均有出处）

| 数字 | 出处 |
|---|---|
| 解析值 4 矩阵×8B = 0.03125 KB/n²/线程 | 09c94fc6 表内注释（sdc_stress_config.h:1069-1070） |
| T6 实测隐含 0.0316（43.31 GiB vs 模型 39.8 GiB，+8.8%） | 09c94fc6 表内注释 + e481496e commit 自述 |
| T7 反馈 eff 4.104~4.116 → c_fb≈32.8 B（+10.5%，含探针高水位 ~4.7%） | 09c94fc6 表内注释 |
| 重标定后 61G 单节点机 max_arm：eff 4.077~4.084（偏差 +1.3%） | 09c94fc6 commit message 验证段 |
| 本机 128C/30G 4 节点 NEON dynamic_square（2026-11-08）：eff 4.170~4.197 → +3.4~4.1%；node3 实测 14.50 GiB vs 预算 13.9（+4.0%）、node1 12.38 vs 12.0（+3.3%） | 本次任务实测（下方 Task 1 Step 2 命令复现） |

---

### Task 1: 表头注释勘误（真源）

**Files:**
- Modify: `framework/sdc_stress_config.h:1055-1059`

**Interfaces:**
- Consumes: 上表证据基线。
- Produces: 与表内 09c94fc6 重标定注释（:1068-1074）不再自相矛盾的表头注释；spec 勘误（Task 2）引用它为真源。

- [ ] **Step 1: 替换表头注释**

将：
```c
/* 族 → 实测 c(KB/n²/线程)。Phase A 2026-10-27, (child@T4−child@T1)/3
 * 差分增量口径——与 svd_cdouble_sve.cpp:92-95 的"单进程 9.7-10× 输入
 * 体积"(含基线, ~155B/n²)口径不同, 差异 ~35% 属口径差非矛盾: 联合
 * 求解的增量模型取差分口径。c 是算法内在属性(矩阵驻留数×元素大小),
 * 跨机可移植; Eigen 版本升级需重标定本表。 */
```

替换为（断言弱化 + 漏计项 + 实测残差 + 反馈闭环角色，保留口径说明与重标定句）：
```c
/* 族 → 实测 c(KB/n²/线程)。Phase A 2026-10-27, (child@T4−child@T1)/3
 * 差分增量口径——与 svd_cdouble_sve.cpp:92-95 的"单进程 9.7-10× 输入
 * 体积"(含基线, ~155B/n²)口径不同, 差异 ~35% 属口径差非矛盾: 联合
 * 求解的增量模型取差分口径。
 * c 的可移植性(issue #511 勘误, 2026-11-08): 主体(≈矩阵驻留数×元素
 * 大小, 如 GEMM_F64 4 矩阵×8B=0.03125)是算法内在、一阶跨机近似; 但
 * 差分口径只测 n² 斜率项, 漏计非 n² 缩放驻留(Eigen packing buffer、
 * 联合求解探针高水位、分配器 slack)——VmHWM 实测隐含 c 同机即偏高:
 * GEMM_F64 重标定前 +8.8%(T6), 重标定后 61G 单节点机 +1.3%(max_arm)、
 * 128C/30G 四节点机 +3.4~4.1%(NEON dynamic_square, 2026-11-08 实测),
 * 残差随机器/用例漂移, 不构成"逐机精确"。表值是初始锚: 精度由 T7
 * VmHWM 反馈闭环同机收敛(向上修正无死区, spec §4.4), 跨机首跑的
 * ±5% 级残差同样由反馈吸收。Eigen 版本升级需重标定本表。 */
```

- [ ] **Step 2: 行为零变化验证（受影响行为真实跑通）**

Run: `./builddir/sdcshield -e eigen_gemm_double_dynamic_square -t 5000 -v 2>&1 >/dev/null | grep -E "联合求解|反馈|test:|result:|exit:" | head -20`
Expected: `result: pass`、`exit: pass`，4 个 slice 的 `[联合求解] ... c=32.3` 与 `[反馈] ... eff_ratio=4.17~4.20`（与改动前同量级——纯注释不改变求解）。

- [ ] **Step 3: 构建**

Run: `./scripts/build.sh 2>&1 | tail -5`
Expected: 零警告，编译通过（注释改动）。

### Task 2: spec 副本同步勘误

**Files:**
- Modify: `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md:196-202`

**Interfaces:**
- Consumes: Task 1 的新表头注释（真源）。
- Produces: spec 与代码注释一致；表值历史（0.029）以勘误标记保留演进链。

- [ ] **Step 1: 加勘误标记**

在 §4.3 RSS_RATIO_TABLE 条目（:196-202）中：
(a) `GEMM-f64 0.029` 后插入 `【2026-10-28 T9 重标定 0.0315, 见表内注释】`；
(b) 将 `c 是算法内在属性（矩阵驻留数×元素大小），跨机可移植；Eigen 版本升级需重标定（表注释注明）。` 替换为：
```
  【2026-11-08 勘误 #511】"跨机可移植"过强：c 的主体（矩阵驻留数×
  元素大小）一阶可移植，但差分口径漏计非 n² 缩放驻留（Eigen packing
  buffer / 联合求解探针高水位 / 分配器 slack），VmHWM 实测隐含值同机
  即偏 +1~4%（重标定后两机型实测），残差随机器/用例漂移——表值是初始
  锚，T7 VmHWM 反馈闭环（§4.4）才是同机收敛通道；Eigen 版本升级需
  重标定（表注释为真源）。
```

- [ ] **Step 2: 核对无第三份拷贝**

Run: `grep -rn "跨机可移植" framework/ tests/ docs/ README.md | grep -v progress-log | grep -v plans/`
Expected: 仅 spec 一处（Task 1 已改掉 .h 里的）与 plans/ 下本计划自身。

### Task 3: 回归 + 提交 + 推送 + PR + issue 报告

- [ ] **Step 1: 回归**
Run: `./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -3`
Expected: `exit: pass`

- [ ] **Step 2: 提交（DCO）**
```bash
git add framework/sdc_stress_config.h docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md
git commit -s -m "fix(joint): RSS 表头 c 断言勘误——跨机可移植弱化为一阶近似, 如实记录差分口径漏计项 (#511)"
```
commit body 概述：断言残留的证据（blame f84966bc5 未改/09c94fc6 只改表内）、新表述要点、本机实测数字、行为零变化声明。

- [ ] **Step 3: 推送 + PR + issue 回帖**
```bash
git push -u origin fix/issue-511
gh pr create --base main --head fix/issue-511 --title "fix: RSS 表头 c 断言勘误——跨机可移植弱化为一阶近似 (#511)" --body "..."
```
PR body 含 `Closes #511` + 根因/改动/验证证据（三论点处置表 + 本机实测输出引用 + 验证边界声明）。issue #511 回帖贴同一报告。

## Self-Review

1. **Spec coverage**: issue 三论点——1/2 已修声明（PR 报告里给 09c94fc6 证据与本机复现）、3 由 Task 1/2 修复。✓
2. **Placeholder scan**: 无 TBD/TODO；两处替换文本均为逐字成品。✓
3. **Type consistency**: 无代码接口变更。✓
4. **行号有效性**: `:1055-1059` 与 `:196-202` 均为本次任务实测行号（2026-11-08 main aeb2c1ea）。✓

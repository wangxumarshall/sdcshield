# 无 SVE 鲲鹏 920 PROD 数实测收口（issue #450）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 README.md 与 CLAUDE.md 中三重漂移的无 SVE 鲲鹏 920 默认 PROD 数（README 299 推算 / CLAUDE.md ~295 预测 / 本机真值 388）统一为本机 0xd01 实测的 388/396/400，并为两个 host-dependent 数字补可复现机型锚点。

**Architecture:** 纯文档改动（README.md 两处 + CLAUDE.md 一处），零代码触碰。数据来源为本机——HiSilicon 0xd01 TaiShan v110（`--dump-cpu-info` 标签 `firestorm`，MIDR 0x481fd010，128 核 4 NUMA），即文档「待无 SVE 主机实测复核」所指机型。

**Tech Stack:** 无（Markdown 文档同步）。

**Spec:** issue #450（audit：host-dependent 数字漂移 + 机型归属）；规则 7（文档同步 100% 准确）。

## Global Constraints

- 一个补丁一个单元：本 plan 的修复本体是**一个单元**（同一数字链的三处收口）。
- 纯文档改动，`./scripts/build.sh` 必须零新警告（文档不触碰构建，验证确认即可）。
- 本机即 0xd01 无 SVE 机：受影响行为（`--list-tests` 三档计数）必须本机实测引用真实输出。
- x86-64 非回归：纯文档，如实声明零代码触碰。
- DCO：`git commit -s`，Signed-off-by 最后一行。

## 调查结论（根因，2026-11-02 实测）

### issue #450 字面三点已被后续提交清除

issue 创建于 2026-11-01T22:24Z，audit 引用的是 dff197cf（2026-10-27 README 重构）diff 时点的旧状态：

| issue #450 抱怨 | 当前 main 状态 | 清除提交 |
|---|---|---|
| CLAUDE.md ~478 vs README 479 漂移 | 双双为 486/496/500 | 4374a8b1（2026-11-01 18:47，#374 降级后同步） |
| README 479 机型误标「Kunpeng 920 0xd02」（真身 cortex x3b, 48-d02-00） | README/CLAUDE 全仓无 0xd02 残留（实测 grep exit=1） | f9066c58（2026-11-01 21:37，README 去冗余） |
| CLAUDE.md 未随大颗粒度改动同步 | 已同步（486/496/500 一致） | 4374a8b1 + 45efb5b6 |

### 同类残留（规则 7 违例活体）= no-SVE 920 数字三重漂移

- README.md:344 主行：「无 SVE 鲲鹏 920 **为 299**」——推算值，语气为实测（issue #450 批评的同构形态）。
- README.md:388 footnote：「299（推算 = 2026-10-27 实测 297 + spinlock_add_neon + smi_count，**待无 SVE 主机实测复核**）」。
- CLAUDE.md:66 区域：「no-SVE 920: ~295 PROD (predicted; 299 pre-#374 − 4; host re-measure pending)」。
- **真值（本机 = 0xd01 无 SVE 鲲鹏 920，main 7f0b9c7c，2026-11-02）：default 388 / q0 396 / q-1 400。**

### 299/295 证伪根因

299 推算基数（297 于 2026-10-27 实测、298 于 2026-10-31 复测）测于 `d61b7de1`（2026-10-31 17:36 后合入 main 的 cache/lsu/memory 88 文件 core-179 家族批）之前，**漏计该批 no-SVE 默认集净 +91**。4374a8b1 做 #374 降级同步时继续沿用坏基数（299 − 4 = ~295）。

### 对账链（全部机械可核）

```
388 = 297(2026-10-27 实测,f422d560 树)
    + 91(d61b7de1 88 文件批, no-SVE 默认集净增)
    + 1(spinlock_add_neon, PR#328)
    + 1(smi_count SKIP→PROD, issue #7)
    − 4(sched_storm×4 PROD→BETA, issue #374)
    + 2(neon_rot_ldr_rand{,_thread}, PR#343)
```

交叉自洽（PR#413 在 d6e39b96 树实测 390/394/398）：390 − 4 + 2 = 388 ✓；394 + 2 = 396 ✓；398 + 2 = 400 ✓。

### 与 OPEN PR #413（issue #400）的关系

PR#413（fix/issue-400，2026-11-01，OPEN 未合并）已在 d6e39b96 树做同类收口（390/394/398 + 0xd02/0xd01 锚点）。本 plan 在当前 main（7f0b9c7c，post-#374 + PR#343）上以最新实测收口；两 PR 若先后合并需人工 rebase 取舍数字，不在本任务范围（绝不 merge）。

## File Structure

- Modify: `README.md:344`（spec 策略节主行——no-SVE 数字 + 双机型锚点）
- Modify: `README.md:388`（用例质量分级 footnote——no-SVE 推算链收口）
- Modify: `CLAUDE.md:66-68`（build & run 注释 no-SVE 行——与 README 同数字）
- Create: 本 plan 文件（独立提交，惯例 2d6211b0）

---

### Task 1: 修改 README.md 两处 + CLAUDE.md 一处

**Files:**
- Modify: `README.md:344`
- Modify: `README.md:388`
- Modify: `CLAUDE.md:66-68`

**Interfaces:**
- Consumes: 本机实测 388/396/400（main 7f0b9c7c，0xd01 firestorm）；enumeration.md §7 机型证据（cortex x3b, 48-d02-00）。
- Produces: README 与 CLAUDE.md 的 no-SVE 920 数字一致（388/396/400）且带可复现机型锚点。

- [ ] **Step 1: README.md:344 主行——数字收口 + 双机型锚点**

旧：
```
——默认用例数 **host-dependent**：SVE1 主机实测 486 PROD（`--quality=0` 496、`--quality=-1` 500），无 SVE 鲲鹏 920 为 299（完整时序链见「用例质量分级」footnote）。
```
新：
```
——默认用例数 **host-dependent**：SVE1 主机（HiSilicon 0xd02，`--dump-cpu-info` 标签 `cortex x3b`）实测 486 PROD（`--quality=0` 496、`--quality=-1` 500），无 SVE 鲲鹏 920（HiSilicon 0xd01 TaiShan v110，标签 `firestorm`）实测 388 PROD（`--quality=0` 396、`--quality=-1` 400）（完整时序链见「用例质量分级」footnote）。
```

- [ ] **Step 2: README.md:388 footnote——no-SVE 推算链证伪收口**

旧（同句 no-SVE 段）：
```
；无 SVE 鲲鹏 920 **299**（推算 = 2026-10-27 实测 297 + `spinlock_add_neon` + `smi_count`，待无 SVE 主机实测复核）。
```
新：
```
；无 SVE 鲲鹏 920（0xd01）2026-11-02 main 实测 **388**（q0/q-1 = 396/400）——旧推算 299 已证伪：基数 297/298 测于 `d61b7de1` 88 文件 core-179 家族批合入前，漏计该批 no-SVE 默认集净 +91；对账链 297（2026-10-27 实测）→ 390（`d6e39b96` 实测，已含 88 文件批/`spinlock_add_neon`/`smi_count`）− 4（#374 `sched_storm×4` 降级）+ 2（`neon_rot_ldr_rand{,_thread}`，#343）= 388。
```

- [ ] **Step 3: CLAUDE.md:66-68 注释——no-SVE 行与 README 同数字**

旧：
```
#   no-SVE 920: ~295 PROD (predicted; 299 pre-#374 − 4; host re-measure pending)
```
新：
```
#   no-SVE 920:  388 PROD / 396 q0 / 400 q-1  (2026-11-02 measured on 0xd01 "firestorm";
#                 old 299/~295 predictions falsified — missed the d61b7de1 91-test batch)
```

- [ ] **Step 4: 验证——构建零新警告 + 三档计数复测 + 回归**

Run:
```bash
./scripts/build.sh 2>&1 | tail -3          # 预期: no work to do(纯文档改动)或零警告完成
./builddir/sdcshield --list-tests | wc -l             # 预期 388
./builddir/sdcshield --quality=0 --list-tests | wc -l  # 预期 396
./builddir/sdcshield --quality=-1 --list-tests | wc -l # 预期 400
./builddir/sdcshield -e zstd19 -t 2000 -n 1            # 预期 exit: pass
```

- [ ] **Step 5: 提交（plan 与修复两个提交，各一个单元）**

```bash
git add docs/superpowers/plans/2026-11-02-no-sve-prod-count-reconciliation.md
git commit -s -m "docs(plan): 无 SVE 920 PROD 数收口计划——issue #450 残留漂移调查与对账链"
git add README.md CLAUDE.md
git commit -s -m "docs(sync): 无 SVE 鲲鹏 920 PROD 数实测收口——README/CLAUDE 299/~295 证伪为 388 (#450)

本机(0xd01 firestorm)=「待无 SVE 主机实测复核」所指机型,main 7f0b9c7c 实测
388/396/400;299 推算基数漏计 d61b7de1 88 文件批(no-SVE 净 +91);对账链
297+91+1+1-4+2=388;双 host-dependent 数字补 MIDR/dump-cpu-info 机型锚点。"
```

## Self-Review

1. **Spec 覆盖**: issue #450 字面三点(已清除,报告说明)+ 实质残留(no-SVE 漂移,Task 1 收口)+ 机型归属锚点(Step 1/2)✓。
2. **占位符扫描**: 无 TBD/TODO;所有旧/新文本逐字给出 ✓。
3. **一致性**: 388/396/400 三处文件一致;对账链与 PR#413 的 390/394/398 交叉自洽 ✓。

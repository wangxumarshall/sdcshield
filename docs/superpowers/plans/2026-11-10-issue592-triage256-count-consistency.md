# triage-256 §1 计数自洽（mce_check 拆列）+ 两 seed 参数空间不对称明示 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #592（audit）——triage-256.md §1 表 seed1 行 `pass=166+timeout=25=191` 与覆盖 `190/190` 的数字自矛盾（mce_check 混入 pass 列），并使「190/190 全绿」类结论在 §7 头部与 triage-512 引用处携带参数口径（默认自适应直绿 / acl 标定 baked / clamp 下限有界；两 seed 参数空间不对称）。

**Architecture:** 纯文档修复，零代码行为变更。三处文本编辑（triage-256 §1 表+说明+脚注、triage-256 §7 头部、triage-512 §5 引用）+ 机器消费者自验（matrix-gen.sh 重生成必须与入库 matrix.md 逐字节一致，证明 §4/§5 机器解析输入与 triage-512 §0 逐字引文未被触碰）+ 工件对账（summary 文件 165/25/190 计数复核）。事实基础：`native-256-LCG1001.summary.txt` 实测 165 pass + 25 timed out；`native-256-LCG1002.summary.txt` 实测 190 pass（mce_check 不在 summary 内、非清单用例，框架每次运行末尾自动追加）。

**Tech Stack:** Markdown（campaign 分诊表）、bash/awk（工件计数复核）、`matrix-gen.sh`（triage-256 §4/§5 与 triage-512 §0 的机器消费者）。

**Spec:** issue #592 正文（audit 快照基于 commit 76482b8b 的 :49/:201；当前 main 上 §1 表自矛盾原样残留，§7 口径限定已由 PR #404 部分补齐、但两 seed 构成不对称仍未携带，triage-512:284 裸引「190/190 全绿」为审计警示的失口径引用现役实例）。

## Global Constraints

- 仓库规则（CLAUDE.md/AGENTS.md）：一个补丁一个单元（本 issue 的三处编辑同属一个审计缺陷单元，随计划文档一个 commit）；DCO `git commit -s`（`Signed-off-by: wangxu <wangxumarshall@qq.com>` 最后一行）；只推 `fix/issue-592`，绝不推 main；绝不 merge/resolve。
- **机器解析边界**：`matrix-gen.sh` 机器解析 triage-256.md 的 §4（`## 4.`..`## 5.`）clamp 列与有界重跑结果列、§5（`## 5.`..`## 6.`）有界值列，以及 triage-512.md 的 §0（`## 0.`..`## 1.`，逐字引文入 matrix.md 头部）。本计划的编辑全部落在这些节界之外（triage-256 §1/§7、triage-512 §5），编辑后必须跑 `matrix-gen.sh` 自验、产物与入库 matrix.md 逐字节一致。
- **计数口径**：pass/timeout/fail/crash/skip 五列只计 190 清单用例，行内加总 = 覆盖列（seed1 165+25=190、seed2 190+0=190）；mce_check 单列（两 seed 均 pass，非清单用例，每 seed 实际执行 191 项）。同目录 triage-128.md:59-60 的同位表（189 pass + 1 skip = 190，mce_check 不入列）即为该口径的既有先例——本修复使 256 表对齐 128 表口径。
- **不改事实声明**：所有既有证据事实（165=164+1、25 腿外重跑、seed2 三重构成、191 执行、seed2 首跑腿进程层结局未证等）原样保留，只改呈现与口径携带。

---

### Task 1: triage-256.md §1 —— 表拆 mce_check 列 + 说明行改写 + 脚注对齐

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md:96-115`

**Interfaces:**
- Consumes: `native-256-LCG1001.summary.txt`（165 pass + 25 timed out）、`native-256-LCG1002.summary.txt`（190 pass）工件事实。
- Produces: §1 表新列 `mce_check`（matrix.md:319 / README:357 引用的 §7 数字 165/164+1/25/190 与 §5 协议不受影响——matrix-gen.sh 不解析 §1）。

- [ ] **Step 1: 表头与两行数据拆列**

old（:96-99）:
```markdown
| seed | 覆盖 | pass | timeout | fail | crash | skip | 退出 | 时长 |
|---|---|---|---|---|---|---|---|---|
| LCG:1001 | 190/190 (+mce_check) | 166 | 25 | 0 | 0 | 0 | `exit: pass`（exit=0） | 03:55-07:06 ≈ 3h11m |
| LCG:1002（26 项有界重跑） | 190/190 (+mce_check) | 191 | 0 | 0 | 0 | 0 | `exit: pass`（exit=0） | 09:24-10:01 ≈ 37m |
```

new:
```markdown
| seed | 覆盖 | pass | timeout | fail | crash | skip | mce_check | 退出 | 时长 |
|---|---|---|---|---|---|---|---|---|---|
| LCG:1001 | 190/190 | 165 | 25 | 0 | 0 | 0 | pass | `exit: pass`（exit=0） | 03:55-07:06 ≈ 3h11m |
| LCG:1002（26 项有界重跑） | 190/190 | 190 | 0 | 0 | 0 | 0 | pass | `exit: pass`（exit=0） | 09:24-10:01 ≈ 37m |
```

自洽校验：165+25+0+0+0=190=覆盖 ✓；190+0+0+0+0=190=覆盖 ✓；mce_check 不入 pass 列。

- [ ] **Step 2: 说明行改写（列语义 + 两 seed 不对称 + 引用口径要求；保留 seed2 首跑腿 provenance 全文）**

old（:101-106）:
```markdown
- 两 seed 的 pass 计数均含 mce_check（seed1：165 清单腿内绿（164 默认自适应直绿 +
  1 格 acl 标定 baked）+ 25 超时格走 §5 有界重跑 + mce_check = 191 执行 166 绿；
  seed2：190 清单腿内绿（164 默认自适应直绿 + 1 格 acl 标定 baked + 25 格腿内有界）
  + mce_check = 191 执行 191 绿）；seed2 行全部数据来自 09:24 起的有界重跑腿——
  首跑腿尸体已被覆盖、不可核验，其进程层结局未证（见头部环境事件 2 ①），
  不贡献本表 seed2 侧任何计数。
```

new:
```markdown
- 本表 pass / timeout / fail / crash / skip 五列只计 190 清单用例，行内加总恰等于
  覆盖列（seed1 165+25=190、seed2 190+0=190）；mce_check 单列、不混入任何计数列
  ——每 seed 实际执行 191 项（190 清单 + 1 mce_check，两 seed 均 pass）。绿格构成：
  seed1 清单腿内 165 绿（164 默认自适应直绿 + 1 格 acl 标定 baked），25 超时格经
  §5 腿外单测有界重跑转绿（clamp 下限，最小激发强度）；seed2 清单腿内 190 绿
  （164 默认自适应直绿 + 1 格 acl 标定 baked + 25 格腿内有界）。两 seed 实验条件
  不对称——seed2 腿烧入 26 项 -O 有界覆盖、seed1 腿仅 1 项 acl 标定 baked（其 25
  格有界绿全来自腿外单测），「190/190 全绿」跨的是两个不同参数空间，引用须携带
  本绿格构成与 §5「证据强度限定」口径；seed2 行全部数据来自 09:24 起的有界重跑腿
  ——首跑腿尸体已被覆盖、不可核验，其进程层结局未证（见头部环境事件 2 ①），
  不贡献本表 seed2 侧任何计数。
```

- [ ] **Step 3: mce_check 脚注对齐新列**

old（:115）:
```markdown
（+mce_check：框架在每次运行末尾自动追加的 EDAC 检查，两 seed 均 pass，非清单用例。）
```

new:
```markdown
（mce_check 列：框架在每次运行末尾自动追加的 EDAC 检查，非清单用例，两 seed 均
pass——独立成列，不入 pass 等任何清单计数列。）
```

### Task 2: triage-256.md §7 头部 —— 两 seed 绿格构成不对称明示

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md:329-332`

**Interfaces:**
- Consumes: Task 1 的计数口径（165 腿内 + 25 腿外 / 190 腿内三重构成）。
- Produces: §7 头部结论；matrix.md:319 对账行引用的 §7 数字（165/164+1/25/190/164+1+25）保持不变。

- [ ] **Step 1: 头部改写**

old（:329-332）:
```markdown
**native 256 档两 seed 全绿（165 格腿内绿 = 164 格默认自适应直绿 + 1 格 acl 标定
baked，另有 25 格 clamp 下限最小强度有界绿；默认自适应直绿与有界绿（1 格 acl
标定 + 25 格 clamp 下限，几何均远小于自适应档）两者证据强度不等价——
§5「证据强度限定」），Task 2 收口**：
```

new:
```markdown
**native 256 档两 seed 全绿（绿格构成两 seed 不对称、非同参数复现——seed1：165 格
腿内绿 = 164 格默认自适应直绿 + 1 格 acl 标定 baked，另 25 格为腿外单测 clamp 下限
重跑绿；seed2：190 格腿内绿 = 164 格默认自适应直绿 + 1 格 acl 标定 baked + 25 格
腿内 clamp 下限有界绿——seed2 腿烧入 26 项 -O 有界覆盖、seed1 腿仅 1 项，两 seed
的「全绿」跨不同参数空间；默认自适应直绿与有界绿（1 格 acl 标定 + 25 格 clamp
下限，几何均远小于自适应档）两者证据强度不等价——§5「证据强度限定」），Task 2 收口**：
```

（旧文把 seed1 专属的「165 格腿内绿」写成两 seed 联合构成——seed2 腿内绿实为 190 格；§7 下方 seed1/seed2 两个 bullet 已各自准确、不动。）

### Task 3: triage-512.md §5 —— 裸引「190/190 全绿」补携带参数口径

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-512.md:284`

**Interfaces:**
- Consumes: triage-256 §1/§5/§7 口径（Task 1/2 产物）。
- Produces: 无下游（triage-512 §5 非机器解析输入；matrix-gen.sh 只逐字引其 §0..§1 节）。

- [ ] **Step 1: 引用行补口径**

old（:283-284）:
```markdown
**结论：零 init-skip 嫌疑格**。结合 §2.2 HWCAP 探针（SVE=1、门全开）与
native 256 腿 190/190 全绿（无规格门 SKIP），本腿绿格均为真实执行。
```

new:
```markdown
**结论：零 init-skip 嫌疑格**。结合 §2.2 HWCAP 探针（SVE=1、门全开）与
native 256 腿 190/190 全绿（构成 = 164 格默认自适应直绿 + 1 格 acl 标定 baked +
25 格 clamp 下限有界绿，两 seed 参数空间不对称——绿格构成与证据强度口径见
triage-256 §1/§5/§7；无规格门 SKIP），本腿绿格均为真实执行。
```

（该句在本腿的用途——「绿格均为真实执行」的 init-skip 排查前提——不受构成差异影响；补口径只为防「256 档全绿」被裸引为默认配置可运行性声明。）

### Task 4: 验证（CLAUDE.md 规则 2，全部真实输出）

- [ ] **Step 1: 机器消费者自验**：仓库根 `LC_ALL=C ./docs/superpowers/inventory/sve-all-vl-campaign/matrix-gen.sh > /tmp/matrix.md.new && diff /tmp/matrix.md.new docs/superpowers/inventory/sve-all-vl-campaign/matrix.md` → 期望逐字节一致（编辑不在机器解析节界内）；matrix.md 不重新入库（无漂移即无再生成义务）。
- [ ] **Step 2: 工件计数对账**：awk 复核两份 summary 的 result 列计数 → seed1 pass=165/timed out=25（165+25=190）、seed2 pass=190；并机械核验表中数字 165=164+1、190=164+1+25。
- [ ] **Step 3: 构建零新警告**：`./scripts/build.sh` → no work to do / 零警告（纯文档改动）。
- [ ] **Step 4: 回归**：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- [ ] **Step 5: x86-64 非回归**：纯文档（2 个 .md），零代码触及——按检查声明，无编译面。

### Task 5: 提交、推送、PR、报告

- [ ] **Step 1: 单提交**（计划文档 + triage-256.md + triage-512.md 同属 issue #592 一个单元，#404 先例同款）：`git add` 三文件 + 计划文档，`git commit -s`，标题 `fix: triage-256 §1 表 mce_check 拆列计数自洽 + 两 seed 参数空间不对称明示 (#592)`，正文含根因/改动/验证证据（真实输出摘引）。
- [ ] **Step 2: 推送**：`git push -u origin fix/issue-592`（绝不推 main）。
- [ ] **Step 3: 开 PR**：`gh pr create --base main --head fix/issue-592 --title "fix: <一句话> (#592)" --body "Closes #592 + 报告"`；若 auto-pr.yml 抢先则 `gh pr edit fix/issue-592` 兜底改写（必须含 Closes #592）。
- [ ] **Step 4: 报告**：报告（根因、改动、验证证据、PR 链接）贴到 issue #592；工作区回到干净 main。

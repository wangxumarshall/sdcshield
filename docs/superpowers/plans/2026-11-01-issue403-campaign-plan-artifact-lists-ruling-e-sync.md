# campaign 计划文档工件清单同步裁定 E — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #403（audit Minor/可维护性）：SVE 全 VL 验证运动的计划文档
`docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md` Task 2/3/6
的「Files: Create」清单仍把 `native-256-<seed>.log`、`native-128-<seed>.log`、
`gem5-512/<test>.log × 188` 列为**入库**工件，Task 8 仍以「全部日志」为矩阵输入——
而控制器裁定 E（原始 yaml 日志不入库，机械 summary/verdicts 入库）只回写进了
triage-256.md 与本目录 `.gitignore`（`*.log`、`/gem5-512/*`），计划文本未同步。
两套协议在同一交付物内并存：照计划文本执行的后来者 `git add
docs/superpowers/inventory/sve-all-vl-campaign/` 会**零警告静默丢掉全部原始证据**
（目录级 add 对被忽略文件不告警，实测复现），矩阵将引用从未入库的文件。本计划把
计划文档的工件清单与消费描述同步到裁定 E（运动实际执行的协议）。

**Architecture:** 纯文档改动（0 代码语义变化，单文件 5 处编辑 + 本计划文档）。
在计划文档中：(1) Global Constraints 增补一条「证据工件协议（裁定 E）」总纲，
声明不入库/入库工件全集与 `git add` 的静默跳过语义（把「静默丢证据」从陷阱变成
已声明的预期行为，并禁止 `git add -f` 强塞）；(2) Task 2/3/6 的 `.log` Create 行
改标注「留工作区，不入库」，并补上实际入库证据行（`<leg>-LCG<seed>.summary.txt`
+ `.stdout.txt`；`gem5-512/batch.tsv`/`identity.txt` + `gem5-512-verdicts.txt`）；
(3) Task 2 Step 1 补 stdout 跑完即回拷一行（否则 Files 新增的 `.stdout.txt` 无
产出步骤，制造新的清单-步骤失配）；(4) Task 8 的 Consumes 与示例循环从「全部
日志 / native-128-LCG1001.log」改为入库工件（`.summary.txt`）。步骤命令里对
`.log` 的工作区读写（`-o $LOG`、grep 分诊）不动——日志在磁盘上真实存在并被
分诊消费，只是不入库。

**Tech Stack:** Markdown（计划文档 + 本计划），无代码、无构建依赖。

**Spec:** issue #403 审计意见（证据：`.gitignore:1` 与计划 :90/:294 的矛盾、
裁定 E 只回写 triage-256.md）；裁定 E 原文 triage-256.md §1（「152 MB 原始 yaml
日志不入库（裁定 E）：本目录 .gitignore 屏蔽 *.log，代之以 extract-summary.sh
机械提取的 native-256-LCG<seed>.summary.txt」）及 triage-128.md §0/§6 的同款
协议（含「stdout 跑完立即回拷 campaign 目录」加固）；`.gitignore` 头注（gem5-512/
bulk stays out，豁免 batch.tsv/identity.txt）；实际入库工件集（`git ls-files`
可复核）。

## Global Constraints

- 纯 .md 改动：不碰任何代码、脚本、数据文件、`.gitignore` 本身（裁定 E 方向
  正确，错的是计划文本——反向「放开 .log 入库」会塞进 152-383 MB 级 yaml，
  违背裁定 E，不做）。
- 影响面已核：`grep -rn 'native-256-<seed>|native-128-<seed>|<test>\.log|全部日志'`
  全库仅命中该计划文档（docs/cases、docs/paper 的命中为无关内容）——matrix.md/
  README/triage/spec 均无 `.log` 入库主张，无需同步。
- 单 commit（一个逻辑单元：计划文档工件清单同步裁定 E，本计划随同入库，
  先例 7dfa9505 / e61c0fd6）。
- x86-64 非回归 = 纯文档，无架构语义，如实声明。

---

### Task 1: 同步计划文档工件清单 + 验证 + commit

**Files:**
- Modify: `docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md`（5 处：Global Constraints 总纲、Task 2 Files、Task 2 Step 1、Task 3 Files、Task 6 Files、Task 8 Interfaces+Step 1）

**Interfaces:**
- Consumes: 裁定 E 协议事实（triage-256.md §1 / triage-128.md §0/§6 / .gitignore / 实际入库工件集）。
- Produces: 计划文档与 `.gitignore`、triage 表、实际 commit 工件集三方一致的单协议。

- [ ] **Step 1: 复现缺陷**（改前）——`git check-ignore -v` 三个计划声称的入库
  `.log` 路径全部命中忽略规则；dummy `.log` + 目录级 `git add` 零警告、零暂存
  （静默丢证据）；grep 计划 :90/:131/:294/:398/:403 原文与 issue 引用一致。

- [ ] **Step 2: 按上述 Architecture 施加 5 处编辑**。

- [ ] **Step 3: 改后一致性核验**——(a) grep 计划全文：所有 `.log` 的 Create/矩阵
  输入主张均带「不入库」标注或已改为 `.summary.txt`；(b) 计划声称的入库工件
  （summary/stdout/verdicts/batch.tsv/identity.txt）经 `git check-ignore` 逐个
  确认不被忽略；(c) 与 `git ls-files docs/superpowers/inventory/sve-all-vl-campaign/`
  的实际入库集无矛盾。

- [ ] **Step 4: 构建零新警告** `./scripts/build.sh`（纯 .md 不触编译，按规则 2
  全链走实跑，引用真实输出）。

- [ ] **Step 5: 回归** `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`
  （引用真实输出；本修复无受影响运行时测试，zstd19 为树健康回归）。

- [ ] **Step 6: commit -s + push fix/issue-403**（`git branch --show-current`
  确认非 main）。

### Task 2: PR + issue 报告

- [ ] **Step 7: `gh pr create --base main --head fix/issue-403`**，正文含 Closes
  #403 + 根因/改动/验证证据。

- [ ] **Step 8: 报告贴回 issue #403**；工作区回到干净 main。

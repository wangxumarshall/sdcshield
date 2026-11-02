# triage-256.md §4 seed2 尸体比对降格为未留档一次性观察 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #401（audit Minor/可维护性）：`triage-256.md` §4 把「与 seed2 首跑
尸体的逐格比对」当作「25 格超时集合跨 seed 结构性」的已执行交叉核验论据，而该比对
依赖的原始日志已被裁定 D 的重跑覆盖、/tmp stdout 随重启丢失——是不可复审计实的既毁
证据。按审计给出的处置一：降格为「未留档的一次性观察」并明确移出论据链，结构性
结论改由「几何与 seed 无关」的机制论证独撑（审计确认机制论证本身成立）。

**Architecture:** 纯文档改动（0 代码语义变化，单文件单处段）。重写 §4 根因段末尾
的论据句：结论句显式声明「仅由机制论证独撑」，尸体比对改记为「未留档的一次性
观察（不列入论据链）」并写明不可复审计实的原因（日志被重跑覆盖 + /tmp stdout 随
重启丢失），如实保留「该观察曾发生」的历史记录。头部环境事件 2（`尸体已被重跑
覆盖`——审计引用的既毁事实本身）与 §4 另注/§7 结论中对死亡时刻的表述不动（它们
引用的是头部留档记录，非已毁工件比对）。

**Tech Stack:** Markdown（`docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md`），
无代码、无构建依赖。

**Spec:** issue #401 审计意见（处置选项一原文：「应降格为『未留档的一次性观察』」；
机制论证成立性：「几何由物理内存与线程数推导」）；仓库诚实性规则（CLAUDE.md
placeholder honesty / AGENTS.md 诚实性——不以不可复核的工件充当已执行核验）。

## Global Constraints

- 纯 .md 改动：不碰任何代码、脚本、数据文件；`matrix.md`/README 已核无此论断
  （grep `尸体|比对一致|seed2 首跑` 零命中 matrix.md/README；库内其余命中为主题
  文件 triage-256.md、本 plan 自身，及无关语境一处——mspc_paper_zh.md:121 论文译文
  中的「尸体」一词——均非该论断的扩散），无需同步。
- 单 commit（一个逻辑单元：一处证据地位降格）。
- x86-64 非回归 = 纯文档，无架构语义，如实声明。

---

### Task 1: 改写 §4 论据句 + 验证 + commit

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md:84-86`

**Interfaces:**
- Consumes: 无（纯文档）。
- Produces: 无（纯文档）。

- [x] **Step 1: 重写 §4 段末论据句**——结构性结论声明「仅由机制论证独撑」；尸体比
  对降格为「未留档的一次性观察（不列入论据链）」并注明不可复审计实的原因，处置句
  原样保留。

- [x] **Step 2: 内部一致性核验**——diff 恰为该一处段；grep 全文确认「比对一致」不再
  出现在论据地位；头部环境事件 2 / §4 另注 / §7 与新表述无矛盾。

- [x] **Step 3: 构建零新警告** `./scripts/build.sh`（纯 .md 不触编译，按规则 2 全链
  走实跑，引用真实输出）。

- [x] **Step 4: 回归** `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`
  （引用真实输出）。

- [x] **Step 5: commit -s + push fix/issue-401**（`git branch --show-current` 确认非
  main）。

### Task 2: PR + issue 报告

- [x] **Step 6: `gh pr create --base main --head fix/issue-401`**，正文含 Closes #401
  + 根因/改动/验证证据。

- [x] **Step 7: 报告贴回 issue #401**；工作区回到干净 main。

---

## 执行记录（2026-11-02 追加，PR #415 检视闭环）

本计划由 PR #415（fix/issue-401）执行交付；与计划原文的偏差如实记录如下：

- **commit 数**：原「单 commit」约束被检视-修复闭环超越——e61c0fd6 交付 Task 1
  初版后，local-claude 检视 round 1/2/3 各追加修正 commit（d5fac31c、e1ff708c、
  3b88dc70，round 3 的 866f05bd/e61199b2/b4da0546，及本执行记录 commit），每个
  commit 仍是一补丁一单元（CLAUDE.md 规则 1），与「单 commit」约束的原始意图
  （单一逻辑单元交付）不冲突但如实记录超出。
- **文本演进**：Step 1 草拟的目标文本（「仅由机制论证独撑」确定级结论）在 round 1
  改为证据地位分层（「预期是结构性」，外推强度「预期」而非「确定」）；§7 结论③
  在 round 2/3 随动传播强度并拆分跨 seed/跨腿维度；§4 一次性观察句补指代解读的
  事后性披露、「数量级级」笔误修正。逐项依据见各 commit message。
- **Step 2 的教训**：初版联动核验 grep 只查「结构性|跨 seed」两词，措辞不同的
  §7 ③「天然产生」逃过——round 2 意见指出后，核验口径改为全文通读 + 多词 grep
  （「预期/确定/天然/必然/一定/无疑」）。
- **Step 7 口径**：报告已贴回 issue #401 评论串（PR 链接与后续跟踪记录在案）；
  多 worktree 并行环境下本 worktree 为 PR #415 专属、保持 fix/issue-401，无
  「回干净 main」动作（该步骤原意「交付后不滞留特性分支状态」，worktree
  生命周期由编排层管理，如实记录差异）。

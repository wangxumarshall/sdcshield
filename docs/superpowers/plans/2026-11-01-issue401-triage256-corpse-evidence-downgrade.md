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

- [ ] **Step 1: 重写 §4 段末论据句**——结构性结论声明「仅由机制论证独撑」；尸体比
  对降格为「未留档的一次性观察（不列入论据链）」并注明不可复审计实的原因，处置句
  原样保留。

- [ ] **Step 2: 内部一致性核验**——diff 恰为该一处段；grep 全文确认「比对一致」不再
  出现在论据地位；头部环境事件 2 / §4 另注 / §7 与新表述无矛盾。

- [ ] **Step 3: 构建零新警告** `./scripts/build.sh`（纯 .md 不触编译，按规则 2 全链
  走实跑，引用真实输出）。

- [ ] **Step 4: 回归** `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`
  （引用真实输出）。

- [ ] **Step 5: commit -s + push fix/issue-401**（`git branch --show-current` 确认非
  main）。

### Task 2: PR + issue 报告

- [ ] **Step 6: `gh pr create --base main --head fix/issue-401`**，正文含 Closes #401
  + 根因/改动/验证证据。

- [ ] **Step 7: 报告贴回 issue #401**；工作区回到干净 main。

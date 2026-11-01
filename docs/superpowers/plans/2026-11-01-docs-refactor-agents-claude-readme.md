# 顶层三文档重构（AGENTS.md / CLAUDE.md / README.md + 质量保障架构图前置）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 重构 SDCShield 三份顶层文档（重构=结构重组、事实零丢失），并在 README 前部新增 `docs/build-deploy-pr-ci/agent-full-automation.png` 引用与质量保障章节。

**Architecture:** 事实核验驱动的保守重构——每处修改先钉死事实（LSP/grep/读图），再动结构。不整章搬移 README（文档内大量「上节/下节」文本交叉引用，整章搬移会静默断裂），只用「前置新章节 + H2 归位」两处手术式结构调整；CLAUDE.md/AGENTS.md 修真实缺陷（重复标题、巨型run-on 注释、嵌套层级），逐字保留事实链。

**Tech Stack:** Markdown（GitHub 方言）、git（DCO）、clangd LSP、shell 链接完整性校验。

## 事实基线（已核验，2026-11-01，执行者可直接采信）

1. `docs/build-deploy-pr-ci/agent-full-automation.png` **存在**（1.49MB，与 agent-full-automation.md 同批提交）；图与 md 描述一致：v3 整合视图——事件源（GitHub 事件/cron/CI 失败）→ 编排（GitHub Actions + pr_loop.py）→ agent 执行面（四外部 bot + subagent 修复）→ 验证门禁 → 状态留存 → 人工 merge 终门。
2. CLAUDE.md 磁盘 **187 与 189 行重复出现** `## Documentation map — consult before guessing`（中间 188 空行）。
3. 仓库内**无任何文件链接到 README/CLAUDE/AGENTS 的锚点**（`grep -rn 'README\.md#'` 与 `CLAUDE\.md#|AGENTS\.md#` 均零命中，third-party 除外）→ 标题级别调整不破坏外链。
4. README 内部存在「上节/下节/见「XX」节」文本交叉引用 → **禁止整章搬移**，只做就地插入与归位。
5. 五 bot / 六阶段 / task-loop / release-all 15×PASS 硬闸 / 绝不自动 merge 等表述与 README 既有 §PR CI 门禁、§代码检视机器人、§task-loop 及 docs/build-deploy-pr-ci/{pr-loop,task-loop,agent-full-automation}.md 一致。
6. LSP 已确认 `framework/sandstone.h`：`DECLARE_TEST` 族 156–185 行、`TEST_LOOP` 144 行、`struct test::arm64_requires/arm64_min_vl_bits` 433/437 行、`memcmp_or_fail`/RNG 族——三文档中相关技术表述与源码一致，重构**不改任何技术表述**。
7. 测试计数全量时序链（490→486、299→~295、spinlock_add_neon/smi_count +1 分解）在 README「测试用例与检测能力」节（363/409 行附近）**完整存在**→ CLAUDE.md 中该链可压缩为「现行数 + 指针」，不构成事实丢失。

## Global Constraints

- **规则 1（一补丁一单元）**：三个文件 = 三个独立 commit，不捆绑。
- **规则 2（提交前 100% 实证）**：每个 commit 前——(a) `./scripts/build.sh` 确认无重编（`ninja: no work to do`，文档改动零代码影响的实证）；(b) 受影响行为 = 文档本身 → 跑链接/路径完整性校验并引用真实输出；(c) `-e zstd19 -t 2000 -n 1` → `exit: pass`；(d) x86 非回归：纯文档改动，检视即证。
- **规则 3（DCO + 推分支）**：`git commit -s`（末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容、**无 Co-Authored-By**）；只推特性分支 `docs/refactor-agents-claude-readme`，**永不推 main**；提交前 `git branch --show-current` 核对。
- **规则 7（文档同步）**：本次改动即文档本身；重构零事实丢失——删改的每个事实块必须已存在于仓库另一处（已核验见事实基线 7）。
- **不改技术表述**：所有测试名、计数、日期、PR 号、文献引用 [NN]、百分比逐字保留。
- **工作树卫生**：仓库预存 `M third-party/rpms/*`（子模块指针，非本任务产物）与未跟踪文件——**只 `git add` 本任务三个 md 文件，禁止 `git add -A`**。

---

### Task 1: 特性分支 + 环境核验

**Files:** 无文件修改（环境准备）。

- [x] **Step 1.1: 开特性分支**

```bash
cd /home/sdc/wangxu/sdcshield
git branch --show-current        # 期望: main
git checkout -b docs/refactor-agents-claude-readme
git branch --show-current        # 期望: docs/refactor-agents-claude-readme
```

- [x] **Step 2.1: 核验构建产物与被引用路径存在**

```bash
ls builddir/build.ninja          # 已有构建目录（文档改动应零重编）
ls scripts/pr_loop.py scripts/pr_local_review.py scripts/task_loop.py \
   docs/build-deploy-pr-ci/agent-full-automation.png \
   docs/build-deploy-pr-ci/pr-loop.md docs/build-deploy-pr-ci/task-loop.md \
   docs/build-deploy-pr-ci/agent-full-automation.md
# 期望: 全部存在，无 "No such file"
```

若 `builddir/build.ninja` 缺失：按 README 源码构建节先 `PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release && ./scripts/build.sh`（一次性，为规则 2a 提供基线）。

---

### Task 2: README.md 重构——质量保障章节前置 + CI 章节归位

**Files:**
- Modify: `README.md:3`（导言补检测原理）
- Modify: `README.md:7-9`（Origin 后插入新 H2 章节）
- Modify: `README.md:217`（GHA 小节前插入 H2 归位标题）
- Modify: `README.md:219`（「上节」引用显式化）

**Interfaces:**
- Produces: 新章节标题 `## 质量保障：Agent 全流程自动化`（前置）与 `## 质量保障与 CI 基础设施`（归位）——两标题名被本文件内文本引用，逐字一致。

**设计依据（senior rationale，执行者须知）**：README 549 行内有大量「见下节/上节/见「XX」节」引用，整章搬移会静默断裂；采用「前置摘要章 + 就地 H2 归位」两处手术，事实零搬动。

- [x] **Step 2.1: 导言补检测原理（三行）**

old_string:
```
SDCShield is an open-source project designed to identify defects and bugs in ARM CPUs derived from Intel's OpenDCDiag (ARM64 port). It consists of a set of tests built around a sophisticated CPU testing framework.
```
new_string:
```
SDCShield is an open-source project designed to identify defects and bugs in ARM CPUs derived from Intel's OpenDCDiag (ARM64 port). It consists of a set of tests built around a sophisticated CPU testing framework.

SDCShield 专攻**静默数据损坏（SDC, Silent Data Corruption）**——计算不崩溃、但产出错误比特的缺陷。检测原理：向目标计算单元（FMA、SIMD、压缩、SVD、crypto）施加高强压测，再与预计算的 golden 值做**字节级比对**（错一比特即 FAIL，无容差）；x86-64 为未改动的参考架构，ARM64 移植全程增量式非回归。
```

- [x] **Step 2.2: Origin 后插入质量保障章节（含 PNG 引用）**

old_string:
```
SDCShield is derived from OpenDCDiag (which contains the Intel `sandstone` framework), licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) for full derivation and upstream provenance.

## 一行命令检测鲲鹏 CPU SDC
```
new_string:
```
SDCShield is derived from OpenDCDiag (which contains the Intel `sandstone` framework), licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) for full derivation and upstream provenance.

## 质量保障：Agent 全流程自动化

SDCShield 的检测结论必须可信，因此仓库把「质量保障」做成贯穿开发—检视—CI—发布的全流程自动化体系，而非依赖人工自觉。总体架构（v3 整合视图）：

![Agent 全流程自动化总体架构](docs/build-deploy-pr-ci/agent-full-automation.png)

四层机制：

1. **PR 检视-修复闭环（六阶段，绝不自动 merge）**——push 到特性分支自动开 PR（`auto-pr.yml`）；五路检视：四个外部 SaaS bot（Codex / CodeRabbit / PR-Agent / Greptile，均内嵌 SDC 激发检视要点）+ 本地无头 claude 对抗检视（`scripts/pr_local_review.py`）；`scripts/pr_loop.py` 驱动 Status → Wait → Collect → subagent 批判性处置（逐条拒绝或修复并说明）→ 修复提交触发新一轮检视 → 循环至 READY_TO_MERGE → **人工 merge**。合入权永远在维护者手中。
2. **无人值守任务闭环（task-loop）**——main 分支 CI 红 / main 增量对抗审查 / 会话 / 手建 issue 统一进入 `agent-task` 队列，headless agent 修复后开 PR，走同一条检视闭环，最终仍由人工 merge（`scripts/task_loop.py`）。
3. **CI 哨兵与发布闸门**——每日 cron 在 15 个 openEuler 版本容器内全量构建 + 全用例 × 全参数验证（`multi-os-verify.yml`，汇总「用例 × 版本」结果矩阵）；PR 级 lint / DCO / 新增代码随机性门禁 / 多版本角落构建并行把关；发布链 `release-all.sh` 以 15 × `RESULT: PASS` 为硬闸，不过绝不推送。
4. **安全与稳定性扫描**——CodeQL / Gitleaks / osv-scanner / zizmor（每周全量 + PR 增量）；ASan+UBSan 夜巡；本地 15 镜像 × 29 条严格选项矩阵（`scripts/lts-stability/`）。

架构设计、PR 状态机与分阶段路线图见 [docs/build-deploy-pr-ci/agent-full-automation.md](docs/build-deploy-pr-ci/agent-full-automation.md)；各机制工程细节在本 README「质量保障与 CI 基础设施」一节及 [PR 检视-修复闭环手册](docs/build-deploy-pr-ci/pr-loop.md)、[task-loop 手册](docs/build-deploy-pr-ci/task-loop.md) 展开。

## 一行命令检测鲲鹏 CPU SDC
```

- [x] **Step 2.3: GHA 小节前插入 H2 归位**

old_string:
```
### GitHub Actions 每日多 OS 自动验证（`.github/workflows/multi-os-verify.yml`）
```
new_string:
```
## 质量保障与 CI 基础设施

### GitHub Actions 每日多 OS 自动验证（`.github/workflows/multi-os-verify.yml`）
```

- [x] **Step 2.4: 「上节」引用显式化（归位后语义不漂移）**

old_string:
```
CI 哨兵（与上节 `release-all.sh` 发布链路互补）
```
new_string:
```
CI 哨兵（与上节「多版本一键构建与部署」的 `release-all.sh` 发布链路互补）
```

- [x] **Step 2.5: 验证（规则 2，引用真实输出）**

```bash
python3 - <<'EOF'
import re, pathlib
root = pathlib.Path('.')
bad = []
for name in ('README.md',):
    for m in re.finditer(r'\]\(([^)]+)\)', (root/name).read_text()):
        t = m.group(1)
        if t.startswith(('http', '<', 'mailto')): continue
        p = t.split('#')[0]
        if p and not (root/p).exists(): bad.append(f"{name}: {t}")
print("BROKEN:", bad if bad else "none")
EOF
grep -n "^## 质量保障" README.md      # 期望两行：前置章节 + 归位 H2
grep -c "agent-full-automation.png" README.md  # 期望 1（图片引用）
git diff --stat                       # 期望仅 README.md
```

- [x] **Step 2.6: 回归 + 提交**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -2   # 期望 exit: pass
git add README.md
git commit -s -m "docs(readme): 质量保障前置——agent-full-automation.png 全流程架构图 + CI 章节归位"
```

---

### Task 3: CLAUDE.md 重构——去重复标题 + 口径注释结构化

**Files:**
- Modify: `CLAUDE.md:187-189`（重复标题）
- Modify: `CLAUDE.md:30-31`（`--list-tests` 注释，Build & run 块内）

**Interfaces:**
- Consumes: README「测试用例与检测能力」节承载完整计数时序链（事实基线 7）。
- Produces: 无（CLAUDE.md 为终端文档，无下游依赖其结构）。

- [x] **Step 3.1: 删除重复标题**

old_string:
```
## Documentation map — consult before guessing

## Documentation map — consult before guessing
```
new_string:
```
## Documentation map — consult before guessing
```

- [x] **Step 3.2: `--list-tests` 注释结构化（计数链压缩为现行数 + 指针；完整链已存 README，规则 7 合规）**

old_string:
```
./builddir/sdcshield --list-tests             # default PROD set AFTER spec filtering — host-dependent: 486 on an SVE1 host, ~295 on a no-SVE Kunpeng 920 (2026-11-01 merge-tree measured 490/496/500 prod/q0/q-1, then issue #374 moved sched_storm×4 PROD->BETA: 486/496/500 re-measured; no-SVE 299 pre-move -> ~295 = main 2026-10-31 489 + spinlock_add_neon; no-SVE 299 = 297 (2026-10-27) + spinlock_add_neon + smi_count, prediction pending no-SVE re-measure)
```
new_string:
```
./builddir/sdcshield --list-tests             # default PROD set AFTER spec filtering — host-dependent:
                                               #   SVE1 host:  486 PROD / 496 q0 / 500 q-1  (2026-11-01 merge-tree,
                                               #               post issue #374 sched_storm×4 PROD→BETA re-measure)
                                               #   no-SVE 920: ~295 PROD (predicted; 299 pre-#374 − 4; host re-measure pending)
                                               # full dated chain: README「测试用例与检测能力」
```

- [x] **Step 3.3: 验证**

```bash
grep -c "^## Documentation map" CLAUDE.md   # 期望 1
grep -n "486" CLAUDE.md | head -3           # 期望仅注释块命中，数字保留
git diff --stat                             # 期望仅 CLAUDE.md（相对上个 commit）
```

- [x] **Step 3.4: 回归 + 提交**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -2   # 期望 exit: pass
git add CLAUDE.md
git commit -s -m "docs(claude): 修复 Documentation map 重复标题；list-tests 主机口径注释结构化"
```

---

### Task 4: AGENTS.md 重构——规则 3 子目化 + 延伸阅读补全

**Files:**
- Modify: `AGENTS.md:23-32`（关键规则 3）
- Modify: `AGENTS.md:94`（延伸阅读末尾追加两条）

**Interfaces:**
- Consumes: 检视清单第 5 点既有引用「详见关键规则 3」——规则 3 编号**不得改变**（子目化不改号）。
- Produces: 无。

- [x] **Step 4.1: 规则 3 拆为两个嵌套子目（逐字保留全部事实）**

old_string:
```
3. **诚实性——禁止假通过**：ARM64 上尚未实现的功能，必须从 `test_init` 返回 `EXIT_SKIP`，
   且跳过理由为 `"to be implemented (placeholder): <缺失内容>"`；**禁止**用 `EXIT_SUCCESS`
   伪装通过（no-op 测试报 pass 是 bug）。`mce_check` 在 ARM64 是真实的 EDAC 后端测试，**必须通过**。
   **本机能力不支持时的 gem5-fi 强制验证**：受影响行为若因当前机器芯片能力缺失（缺
   HWCAP/HWCAP2 位如 SME、VL/SVL 档位不支持、新指令代际）而无法真机运行，仅验证 SKIP
   路径**不构成完整验证**——必须用 gem5-fi（`/home/sdc/wangxu/gem5-fi`，CHAOS/SE，
   `workloads/sdcshield/` 下同源 dual kernel）在受影响的 VL/SVL 档位仿真验证并引用真实
   run 输出（档位、checksum、`SDCSHIELD_FAILS=0`）；确实无法 gem5 验证时须如实声明验证
   边界。**禁止把「SKIP 理由不变」单独充当功能性修改的验证结论**（PR #242 先例：
   输入公式修复由 `sme_za_tileview_dual.c` 在 SVL 128/256/512/1024-bit 四档实证）。
```
new_string:
```
3. **诚实性——禁止假通过**：
   - ARM64 上尚未实现的功能，必须从 `test_init` 返回 `EXIT_SKIP`，且跳过理由为
     `"to be implemented (placeholder): <缺失内容>"`；**禁止**用 `EXIT_SUCCESS` 伪装通过
     （no-op 测试报 pass 是 bug）。`mce_check` 在 ARM64 是真实的 EDAC 后端测试，**必须通过**。
   - **本机能力不支持时的 gem5-fi 强制验证**：受影响行为若因当前机器芯片能力缺失（缺
     HWCAP/HWCAP2 位如 SME、VL/SVL 档位不支持、新指令代际）而无法真机运行，仅验证 SKIP
     路径**不构成完整验证**——必须用 gem5-fi（`/home/sdc/wangxu/gem5-fi`，CHAOS/SE，
     `workloads/sdcshield/` 下同源 dual kernel）在受影响的 VL/SVL 档位仿真验证并引用真实
     run 输出（档位、checksum、`SDCSHIELD_FAILS=0`）；确实无法 gem5 验证时须如实声明验证
     边界。**禁止把「SKIP 理由不变」单独充当功能性修改的验证结论**（PR #242 先例：
     输入公式修复由 `sme_za_tileview_dual.c` 在 SVL 128/256/512/1024-bit 四档实证）。
```

- [x] **Step 4.2: 延伸阅读补 task-loop 与全自动架构两条**

old_string:
```
- [`docs/build-deploy-pr-ci/pr-loop.md`](docs/build-deploy-pr-ci/pr-loop.md) — PR 检视-修复闭环手册（五 bot 检视——四外部 SaaS bot + 本地 claude 对抗检视 → subagent 批判性处置 → 修复 → 循环 → 人工 merge，绝不自动 merge）；引擎 [`scripts/pr_loop.py`](scripts/pr_loop.py)（第 5 bot 引擎 [`scripts/pr_local_review.py`](scripts/pr_local_review.py)），入口 Claude Code `/pr-loop` 命令（`.claude/commands/pr-loop.md`）与 Trae 规则（`.trae/rules/pr-loop.md`，双份同源）。
```
new_string:
```
- [`docs/build-deploy-pr-ci/pr-loop.md`](docs/build-deploy-pr-ci/pr-loop.md) — PR 检视-修复闭环手册（五 bot 检视——四外部 SaaS bot + 本地 claude 对抗检视 → subagent 批判性处置 → 修复 → 循环 → 人工 merge，绝不自动 merge）；引擎 [`scripts/pr_loop.py`](scripts/pr_loop.py)（第 5 bot 引擎 [`scripts/pr_local_review.py`](scripts/pr_local_review.py)），入口 Claude Code `/pr-loop` 命令（`.claude/commands/pr-loop.md`）与 Trae 规则（`.trae/rules/pr-loop.md`，双份同源）。
- [`docs/build-deploy-pr-ci/task-loop.md`](docs/build-deploy-pr-ci/task-loop.md) — 无人值守任务闭环手册（CI 红 / main 审查 / 会话 / 手建 issue → agent-task 队列 → headless agent → PR → 同一检视闭环 → 人工 merge）。
- [`docs/build-deploy-pr-ci/agent-full-automation.md`](docs/build-deploy-pr-ci/agent-full-automation.md) — Agent 全流程自动化总体架构（事件源 → 编排 → agent 执行面 → 验证门禁 → 人工 merge 终门；含 v3 架构图 `agent-full-automation.png`）。
```

- [x] **Step 4.3: 验证**

```bash
grep -n "详见关键规则 3" AGENTS.md     # 引用仍在且规则 3 编号未变
grep -c "gem5-fi" AGENTS.md            # 期望 ≥1（事实保留）
git diff --stat                        # 期望仅 AGENTS.md
```

- [x] **Step 4.4: 回归 + 提交**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -2   # 期望 exit: pass
git add AGENTS.md
git commit -s -m "docs(agents): 关键规则3子目化（诚实性/gem5-fi）；延伸阅读补 task-loop 与全自动架构"
```

---

### Task 5: 终验 + 推送

**Files:** 无新修改。

- [x] **Step 5.1: 三文件全量链接完整性复查**

```bash
python3 - <<'EOF'
import re, pathlib
root = pathlib.Path('.')
bad = []
for name in ('README.md','CLAUDE.md','AGENTS.md'):
    for m in re.finditer(r'\]\(([^)]+)\)', (root/name).read_text()):
        t = m.group(1)
        if t.startswith(('http', '<', 'mailto')): continue
        p = t.split('#')[0]
        if p and not (root/p).exists(): bad.append(f"{name}: {t}")
print("BROKEN:", bad if bad else "none")
EOF
# 期望 BROKEN: none
```

- [x] **Step 5.2: 构建零影响实证（规则 2a）**

```bash
./scripts/build.sh 2>&1 | tail -3   # 期望 ninja 无事可做/零重编、零告警
```

- [x] **Step 5.3: 提交历史与工作树卫生核查**

```bash
git log --oneline -4    # 期望 3 个 docs commit，每个 Signed-off-by 且无 Co-Authored-By
git status --short       # 期望：本任务三文件已清；third-party/* 子模块指针与未跟踪文件原样保留（非本任务产物，不提交）
```

- [x] **Step 5.4: 推送特性分支（永不推 main）**

```bash
git push -u origin docs/refactor-agents-claude-readme
```

push 会触发仓库自身自动化（`auto-pr.yml` 自动开 PR + 下游检视 bot）——如需检视-修复闭环，交由 `/pr-loop` 流程，本计划不代跑。

## Self-Review 结论

- 规格覆盖：README PNG 前置 + 三文件重构 + 质量保障呈现 = Task 2/3/4；LSP 源码核验已入事实基线 6。
- 无占位符：每处 old/new 均为磁盘逐字内容。
- 一致性：「质量保障与 CI 基础设施」标题在 Step 2.2 引用与 Step 2.3 插入处逐字一致；规则 3 编号不变保住 AGENTS.md 内部引用。
- 风险与边界：不改任何技术表述/计数/日期；README 不整章搬移（上节/下节引用保护）。

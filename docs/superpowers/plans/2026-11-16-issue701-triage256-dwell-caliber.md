# triage-256 单格驻留口径披露 —「代码 bug 候选 = 0」限功能正确性档 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #701（audit）——triage-256.md 的「代码 bug 候选 = 0」收口建立在整腿 `-t 2000`（2 s/格预算）的短驻留单遍/短循环证据上，与仓库自身的长驻留检出教义（README 检测运行口径 `-t 60s` + `-T forever`、"SDC 故障窗口极稀疏（CPU179 实测占空比 ~0.06%），长驻留是检出关键"；AGENTS.md"短跑通过不证明无缺陷"、"全绿不等于无缺陷"）相悖而未披露该局限（证据强度口径漂移）。使全表收口携带单格驻留口径声明：本腿证据 = 功能正确性档（对运动自述目的充分），非 SDC 检出覆盖证据；「代码 bug 候选 = 0」禁止升格引用为「VL=256 档无 SDC 缺陷」。

**Architecture:** 纯文档修复，零代码行为变更。四处文本编辑中三处落点在 triage-256.md 的机器解析节界之外（§1 bullet 新增、§7 头部括注、§7「代码 bug 候选」bullet 追加）、一处（§5「证据强度限定」块尾追加）落在 §5 节界（`## 5.`..`## 6.`）之内——不破坏机器解析的真实护栏是 `matrix-gen.sh` 的 awk 表格行过滤（`-F'|'` 只匹配第 2 列为纯数字的表格行），无竖线散文不进入解析输入，而非节界归属。+ 本计划文档。数字全部机械可复核：164 直绿格驻留分布提取自两份已入库 summary 的 test-runtime-ms 列（seed1：74 格 <2 s / 43 格 2.0~2.5 s 档（仅 18 格 iterations≥2 循环打满 + 25 格 iterations=1 单遍）/ 47 格 >2.5 s 且 iterations 全为 1；seed2：65/49/50 同构（49 格档 = 24 格 iterations=2 循环打满 + 25 格 iterations=1 单遍）；最长 ipsec_aes128_cbc_aes_cmac_sve 206.8/206.7 s，最短 fma_sve 90.7 ms / power_virus_dit_sve_arm 359.7 ms）。

**Tech Stack:** Markdown（campaign 分诊表）、bash/awk（summary 工件驻留分布机械提取）、`matrix-gen.sh`（triage-256 §4/§5 机器消费者，编辑后必须重生成与入库 matrix.md 逐字节一致）、codespell（PR CI lint 门）。

**Spec:** issue #701 正文（audit 锚点 triage-256.md:205，快照行号 :131/:165/:205 对应现行 :230/:265/:353——audit #395/#592 两轮修订后的行号漂移，缺陷本身现行残留：§5「证据强度限定」只覆盖 25 个有界格的相对驻留 858.7~3,148.1 ms，164 个默认自适应直绿格的驻留与 2 s 预算对检测教义的绝对差距全表未声明，§7 收口无驻留语义限定）。

## Global Constraints

- 仓库规则（CLAUDE.md/AGENTS.md）：一个补丁一个单元（本 issue 的四处编辑 + 计划文档同属一个审计缺陷单元，#592/#609 先例同款单 commit）；DCO `git commit -s`（`Signed-off-by: wangxu <wangxumarshall@qq.com>` 最后一行）；只推 `fix/issue-701`，绝不推 main；绝不 merge/resolve。
- **机器解析边界**：`matrix-gen.sh` 机器解析 triage-256.md 的 §4（`## 4.`..`## 5.`）clamp 列与有界重跑结果列、§5（`## 5.`..`## 6.`）有界值列——`sed -n` 按节界抓取后经 `awk -F'|' '$2 ~ /^ *[0-9]+ *$/'` 只匹配第 2 列为纯数字的表格行。本计划 §5「证据强度限定」块尾追加虽落在 §5 节界之内，但系无竖线散文、不匹配 awk 表格行过滤，故不构成解析输入、不触碰任何表体；真实护栏是 awk 表格行过滤而非节界归属（散文行若含 `| 数字 |` 管道形态才会进入解析输入）。编辑后必须跑 `matrix-gen.sh` 自验、产物与入库 matrix.md 逐字节一致。
- **不改事实声明**：所有既有证据事实（165=164+1、25 腿外重跑、两 seed 构成不对称、seed2 首跑腿进程层结局未证、§5 相对驻留数字等）原样保留，只新增口径声明。
- **教义引用用逐字引文而非行号**（README/AGENTS.md 行号随编辑漂移，引文可 grep 复核；audit 自身的 "README.md:53-54" 行号在现行 README 已漂移到 :73/:74 即为佐证）。

---

### Task 1: triage-256.md 四处编辑

**Files:**
- Modify: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md`（§1 bullet 列表、§5 限定块尾、§7 头部、§7 代码 bug 候选 bullet）

**Interfaces:**
- Consumes: `native-256-LCG1001.summary.txt` / `native-256-LCG1002.summary.txt`（test-runtime-ms/iterations 列）、README 检测教义逐字引文、AGENTS.md 检视教义逐字引文、计划 Goal（运动自述目的）。
- Produces: §1「单格驻留口径」bullet（全表驻留口径权威出处）；§5/§7 指向该 bullet；matrix.md 的限定语义指针链（"语义限定统一见 triage-256 §5"）自动继承。

- [ ] **Step 1: §1 新增「单格驻留口径」bullet**（"seed2 时长对照" bullet 之后、mce_check 脚注之前）——声明 2 s/格预算、164 直绿格三桶实测分布、循环档 vs 单遍档与 AGENTS.md「定时持续压测」口径的关系、README 检测口径对照（60 s、10 s 首错窗口、`--smoke` 同档自命名）、语义结论（功能正确性档充分/检出覆盖不成立）。
- [ ] **Step 2: §5「证据强度限定」块尾追加绝对口径锚定**——「38 格打满 ≥2,000 ms」之"满"是相对协议预算，非检测教义长驻留；2 s 预算是两 seed 命令行级统一事实；超预算格墙钟长是单迭代单数据集一遍；380 绿格无任何一格以检测口径（60 s 循环采样）驻留。
- [ ] **Step 3: §7 头部括注补驻留口径**——"…证据强度不等价——§5「证据强度限定」"后接"；两 seed 全部 380 绿格的单格驻留又统一为 -t 2000（2 s 预算）短跑档（audit #701，§1「单格驻留口径」）"。
- [ ] **Step 4: §7「代码 bug 候选 = 0」bullet 追加驻留口径限定**——「功能行为」证据是 2 s 预算短驻留（328 格默认自适应直绿 + 52 格有界绿）；对运动自述目的（plan Goal）充分；对 README 检测教义 + AGENTS.md「短跑通过不证明无缺陷」「全绿不等于无缺陷」不构成 SDC 检出覆盖证据；禁止升格引用为「VL=256 档无 SDC 缺陷」；SDC 检出结论须以长驻留口径另行建立，本表不提供。

### Task 2: 验证（CLAUDE.md 规则 2，全部真实输出）

- [ ] **Step 1: 机器消费者自验**：仓库根 `LC_ALL=C ./docs/superpowers/inventory/sve-all-vl-campaign/matrix-gen.sh > /tmp/matrix.md.new && diff /tmp/matrix.md.new docs/superpowers/inventory/sve-all-vl-campaign/matrix.md` → 期望逐字节一致（编辑不在机器解析节界内、不触碰表体）；matrix.md 不重新入库（无漂移即无再生成义务）。
- [ ] **Step 2: 工件数字对账**：awk 复核两份 summary——164 直绿格三桶分布（seed1 74/43/47、seed2 65/49/50，加总=164）、2.0~2.5 s 桶 iterations 分布（seed1 25×iters1+17×iters2+1×iters3、seed2 25×iters1+24×iters2——即"循环打满"桶实含 25 个单遍格，仅 18/24 格真正循环）、>2.5 s 桶 iterations 全为 1、极值（fma_sve 90.7 ms、power_virus_dit_sve_arm 359.7 ms、ipsec_aes128_cbc_aes_cmac_sve 206.8/206.7 s）与正文引用一致。
- [ ] **Step 3: codespell**：对 triage-256.md 跑 `codespell`（PR CI lint 门，scripts 已实证该门存在）→ 零命中。
- [ ] **Step 4: 构建零新警告**：vendored install/ symlink（复用 CI 克隆 third-party/*/install 静态库产物，只读）→ `PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `./scripts/build.sh -j 16`（共享服务器内存纪律）→ 零新警告。
- [ ] **Step 5: 回归**：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- [ ] **Step 6: x86-64 非回归**：纯文档（.md），零代码触及——按检查声明，无编译面。

### Task 3: 提交、推送、PR、报告

- [ ] **Step 1: 单提交**（triage-256.md + 本计划文档同属 issue #701 一个单元）：`git commit -s`，标题 `fix: triage-256 单格驻留口径披露，「代码 bug 候选 = 0」限功能正确性档 (#701)`，正文含根因/改动/验证证据（真实输出摘引）。
- [ ] **Step 2: 推送**：`git push -u origin fix/issue-701`（绝不推 main）。
- [ ] **Step 3: 开 PR**：`gh pr create --base main --head fix/issue-701 --title "fix: …(#701)" --body "Closes #701 + 报告"`；若 auto-pr.yml 抢先则 `gh pr edit fix/issue-701` 兜底改写（必须含 Closes #701）。
- [ ] **Step 4: 报告**：报告（根因=证据强度口径漂移、改动、验证证据、PR 链接、triage-128 同类发现披露）贴到 issue #701；工作区 clean。

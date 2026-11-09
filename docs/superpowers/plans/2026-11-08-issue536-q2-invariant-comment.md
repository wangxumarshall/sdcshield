# Issue #536: q2 不变式「every host class」过度声明修正 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 svd_cdouble_sve / gemm_double_max_arm 两文件（+spec §4.5 勘误）中「child n ≤ preinit n_hi 不变式无条件成立」的过度声明注释改为如实口径：写明成立条件（节点 MemFree 份额 ≈ CPU 份额 + VmHWM 反馈未采信）、两个失效机制的量化后果（dur ∝ n³ 低估、5×dur+30s 超时裕度侵蚀）、以及不加 clamp 的裁定依据。零代码行为变更。

**Architecture:** 纯注释修正。账本分层：完整推导与裁定放各文件 preinit 块的 n_hi bullet（issue 点名处），同文件其余站点（in-function 中文注释、DECLARE_TEST 尾注、timeout 关系句、127-勘误段）改为一句限定 + 指针；spec §4.5 q2 bullet 追加一条日期勘误作为单一真源。#479 先例（同为两文件注释方向修正）同构。

**Tech Stack:** C++ 块注释（~78 列折行，` * ` 前缀，英文为主夹中文的既有风格）；Markdown spec 勘误块。

**Spec:** issue #536（audit，Minor/正确性·注释过度声明）+ `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.5 q2 bullet（本修复为其追加勘误）+ CLAUDE.md 规则 2/4/5。

## Global Constraints

- 一个补丁一个单元：两测试文件注释 + spec 勘误 + 本计划 = 单个 commit。
- `git commit -s`，尾行 `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`，其后无内容。
- 推 `fix/issue-536` 分支，绝不推 main、绝不 merge、绝不 resolve issue（merge 权在人）。
- 零行为变更：不触碰任何可执行语句；x86-64 无涉（两文件整体在 `#if defined(__aarch64__)` 内）。
- 注释语言风格随原注释（英文技术注释 + 中文 in-function 注释），保持 `n_hi`/`n_mem`/`dur`/`memshare`/`cpushare` 符号体系与 ~78 列折行。
- 数字必须可复算（本计划「复现结论」节逐项给出算式）。

## 复现结论（修复前提，已在本机 main=aeb2c1ea 逐项核实）

1. **preinit 侧**（svd_cdouble_sve.cpp:245-254 / gemm_double_max_arm.cpp:237-247）：
   `n_hi = sqrt(B / (W·c))`，B = `sdc_budget_bytes(650)`（全局四源预算），W = `sdc_total_threads()`（全机 worker），c = RSS 表值（`sdc_rss_table_c`）。
2. **child 侧**（sdc_stress_config.h:1329-1359）：`n_mem = sqrt(node_budget / (node_threads·c))`，
   `node_budget = B × MemFree_node/ΣMemFree`（sdc_numa.cpp:235-236，MemFree 份额 φ_m 加权），
   `node_threads = φ_c × W`（`sdc_numa_effective_threads` 的 CPU 份额 + :1343-1350 的 workers/total_cpus 折算；零内存节点 CPU 折入 fallback 节点）。
3. ⇒ **`n_mem / n_hi = sqrt(φ_m / φ_c)`**。φ_m = φ_c 时恰等：单内存节点（折叠拓扑，两份额恒 1，参考机类）精确相等；份额均衡 NUMA 近似相等。**φ_m > φ_c 时 n_mem > n_hi**——issue 算例：75%/50% ⇒ sqrt(1.5)=1.2247 ⇒ dur ∝ n³ 低估 1.837×。
4. **超时裕度侵蚀算式**（auto 档内存绑住、`ar_t×2×t(n_hi) < RUN_BUDGET` 的暴露区）：
   `timeout/real ≈ 4.75×(ar_t/ar_r)/r^1.5`，r = φ_m/φ_c。
   - SVD：ar_t/ar_r = 25/39（实测最坏，1.56×低估）⇒ 基础 3.04；r=1.5 ⇒ 残余 1.66×；单独打穿需 r^1.5 = 3.04 ⇒ **r ≈ 2.1**。
   - GEMM：ar_t/ar_r = 5/15（CAVEAT 实测，3×低估）⇒ 基础 1.58；**r ≈ 1.37 即打穿**（叠加 CAVEAT 口径）。
5. **第二个失效机制（issue 未点名，本分析补充）**：VmHWM 反馈向下采信 c（越出 15% 死区、下限 0.7× 表锚，sdc_stress_config.h:1294-1316）⇒ n_mem 膨胀至 `1/√0.7 ≈ 1.195×`，**即使份额匹配拓扑也破 n ≤ n_hi**——设计内（OOM 有界），且 preinit 结构上看不见（先于一切 child VmHWM 运行、写后不改）。
6. **残余保护带**：`n = min(n_mem, n_run)`——时间侧先绑的主机上真实迭代被钉在 RUN_BUDGET 校准（SVD 140s 档/GEMM 200s 档），与 n_mem 无关；5× 超时因子本身吸收模型误差；300s floor。
7. 过度声明站点清单（本机 main 全仓 grep `n_hi` 核实，adcx_arm.cpp:190 系 `golden_high` 误匹配，其余 SVE SVD 文件无 preinit）：
   - svd_cdouble_sve.cpp:162（"n_hi tracks it on every host class"）、:214-217（"n_mem uses the same budget"）、:241（中文"恒 <="）、:541（DECLARE_TEST "always <= that ceiling"）
   - gemm_double_max_arm.cpp:162（同款）、:193（"child n <= n_hi is what keeps it"）、:209（"restores the invariant on every host"）、:232（中文"恒 <="）、:471（DECLARE_TEST "always <= that ceiling"）
   - spec 2026-10-27-eigen-p0-p3-fix-design.md:277（"不变式：child 精算 n ≤ preinit n 上界"，已有 #352 auto-分支勘误，无拓扑边界限定）

## 路径裁定：(a) 注释如实化 — 采纳；(b) child 侧 clamp — 否决

issue 备选 (b)（`n = min(n_mem, n_hi)` 使不变式按构造成立）的「失衡拓扑外 clamp 永不生效、行为零变化」前提**不成立**：

1. **反馈路径使 clamp 在参考机类也生效**：VmHWM 反馈采信 c∈[0.7,0.85)×表锚的任何主机（含单内存节点参考机类）上 n_mem 合法膨胀至 ≤1.195×n_hi，clamp 会静默取消该设计内机制（该机制的 0.7 下限正是框架有意允许的有界 n 膨胀，防 OOM 方向的既定设计）。
2. **欠压 SDC 激发**：MemFree 富节点按节点真实预算本可跑更大 n，clamp 把激发维度压回全局口径——与仓库首要关切（激发强度，AGENTS.md 检视重点 1）相悖。
3. **不修反向失衡**：φ_m < φ_c ⇒ n < n_hi ⇒ dur 高估 ⇒ do-while 开出未预算多迭代（#421 形态）——clamp 对此无作用。
4. **爆炸半径**：clamp 落在 `sdc_joint_dim_slice`（框架共享，10+ 用例）或逐测试复制 n_hi 数学（重开「两处分写静默漂移」窗口，PR#384 意见 4432271425 已为此专门收敛过读取通道）。
5. **不可验证**：本机及可用机群无失衡 NUMA 拓扑；gem5-fi 面向 ISA 档位缺口而非 MemFree/CPU 份额建模。
6. issue 定级 Minor/注释过度声明；暴露区（内存绑住 + 份额失衡 + AR 表低估三重叠加）无实测命中；残余带（n_run、5×、300s floor）仍在。仓库对模型精度缺口的既有处置范式即「文档化边界 + 留再标定输入」（gemm CAVEAT 本身即此形态）。

---

### Task 1: svd_cdouble_sve.cpp 四处注释

**Files:**
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:159-164`（n_hi bullet 尾段）
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:214-217`（不变式段落）
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:239-241`（中文 in-function）
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:540-542`（DECLARE_TEST 尾注）

**Interfaces:** 无（纯注释）。

- [ ] **Step 1.1: n_hi bullet 尾段替换**（"The child's n_mem denominator is..." 起至 bullet 末）——完整账本落位处，新文见下方代码块
- [ ] **Step 1.2: 不变式段落收窄**（"Child n <= n_hi holds for the AUTO branch only:" 段）
- [ ] **Step 1.3: 中文 in-function「恒 <=」限定**
- [ ] **Step 1.4: DECLARE_TEST「always <= that ceiling」限定**

（精确 old→new 文本见执行时 Edit 调用，与下方 Task 2 同源同构：bullet 尾段替换为「DENOMINATOR 对齐 + n_mem = n_hi×sqrt(memshare/cpushare) 推导 + 成立条件 + 两机制量化（1.22×/1.8×、残余 1.7×、r≈2.1 打穿；反馈 ≤1.2×）+ 不 clamp 裁定 + 残余带」；其余三处为一句限定 + 指针。）

### Task 2: gemm_double_max_arm.cpp 五处注释

**Files:**
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:159-164`（n_hi bullet 尾段，同构 Task 1.1，数字换 GEMM 口径：CAVEAT 5/15、r≈1.4 打穿、run_budget 200s、本机类内存先绑 n_mem=3364 < n_run~4900）
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:192-196`（timeout 关系句）
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:208-210`（127-勘误段尾 "restores the invariant on every host"）
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:231-232`（中文 in-function）
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:470-471`（DECLARE_TEST 尾注）

**Interfaces:** 无（纯注释）。

- [ ] **Step 2.1–2.5**: 同 Task 1 结构执行五处替换。

### Task 3: spec §4.5 q2 bullet 追加勘误

**Files:**
- Modify: `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md`（svd_cdouble_sve bullet 勘误链末尾，R8/4464388834 块之后、`- **gemm_double_max_arm**` 行之前插入）

**Interfaces:** 无。

- [ ] **Step 3.1**: 插入【2026-11-08 勘误，issue #536】块：成立条件再收窄（(a) 份额匹配 (b) 反馈未采信）、两机制量化、不 clamp 裁定（含对 (b) 前提不成立的四点依据）、残余带；声明本 spec 为账本单一真源。

### Task 4: 构建与回归验证（100% 真实）

- [ ] **Step 4.1**: `PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release`（已存在则 reconfigure）→ `./scripts/build.sh`，零新警告，两受影响 TU 重编译通过（注释在块内，编译即语法验证）。
- [ ] **Step 4.2**: 回归 `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- [ ] **Step 4.3**: 验证边界如实声明：本机 no-SVE 鲲鹏 920，`eigen_svd_cdouble_sve`/`eigen_gemm_double_max_arm` SKIP 属预期；本修复为纯注释（零可执行语句变更），按任务契约豁免受影响行为运行验证——但须引用 `--quality=-1 -e <test>` 的 SKIP 理由行证明测试注册路径未受扰动。
- [ ] **Step 4.4**: 全仓 grep 验证：`grep -rn "every host class\|always <= that ceiling\|恒 <= 此上界\|restores the invariant on every host"` 仅剩历史 plan/spec 勘误块中的引用性文本（2026-11-03-issue479 plan 为历史工件不动；本 spec 勘误块自引除外）。

### Task 5: 提交、推送、PR、issue 评论

- [ ] **Step 5.1**: `git branch --show-current` 确认 `fix/issue-536` → `git add` 四文件 → `git commit -s`（标题 `fix(comments): q2 不变式「every host class」过度声明修正——份额失衡与反馈两机制如实化 (#536)`）。
- [ ] **Step 5.2**: `git push origin fix/issue-536`。
- [ ] **Step 5.3**: API 建 PR（base main，正文含：问题分析/路径裁定依据/验证实录）。
- [ ] **Step 5.4**: issue #536 贴处置评论（修复摘要 + PR 链接 + 验证实录）。

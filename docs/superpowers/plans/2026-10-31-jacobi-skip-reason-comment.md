# eigen_svd_jacobi 四件套 TEST_QUALITY_SKIP 跳过理由注释 — 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 4 个 `eigen_svd_jacobi*` 测试声明处补上 TEST_QUALITY_SKIP 的理由注释，消除 issue #322 所反映的"无注释可查"盲区。

**Architecture:** 纯注释改动（0 代码语义变化）。在每个文件的 `.quality_level = TEST_QUALITY_SKIP` 上方加同一段理由注释（本目录 4 个 init 已有同样的注释复制风格，沿用）。理由三要点：(1) 级别继承自上游 Intel OpenDCDiag 初始提交，上游 master 今日仍 SKIP 且无书面理由；(2) 上游共享 golden 设计下跨线程 SIMD 路径分歧的字节比对风险，已被本 fork per-thread 同源 golden（46141ed4）结构性消除——本机实测 4 用例 `--quality=-1` 全 pass；(3) 维持 SKIP 是待满核多 seed 晋升验证并与上游对齐，显式运行用 `--quality=-1`。

**Tech Stack:** C++ 测试声明（tests/cpu/eigen_svd_jacobi/*.cpp），无依赖变化。

**Spec:** issue #322 的核实结论（本计划同目录证据：上游 91e4fc8609:43、opendcdiag master 今日 svd.cpp:43、CLAUDE.md eigen 修复先例、2026-10-31 本机实测输出）。

## Global Constraints

- 纯注释：不改任何可执行语句、不改 quality_level 值、不动 meson。
- x86-64 非回归 = 检视（注释对两架构同样成立，无 #ifdef 触及）。
- 单 commit（一个逻辑单元：jacobi 家族跳过理由文档化）。

---

### Task 1: 四文件加注释 + 验证 + commit

**Files:**
- Modify: `tests/cpu/eigen_svd_jacobi/svd.cpp:55-56`
- Modify: `tests/cpu/eigen_svd_jacobi/svd_double.cpp:53-54`
- Modify: `tests/cpu/eigen_svd_jacobi/svd_cdouble.cpp:54-55`
- Modify: `tests/cpu/eigen_svd_jacobi/svd_fvectors.cpp:56-57`

**Interfaces:**
- Consumes: 无（纯注释）。
- Produces: 无（纯注释）。

- [ ] **Step 1: 在 4 个文件的 `.quality_level = TEST_QUALITY_SKIP` 行上方插入同一段注释**（内容见执行记录，要点：继承上游/上游仍 SKIP 无书面理由；共享 golden 跨线程 SIMD 分歧风险已被 per-thread 同源 golden（46141ed4）消除，2026-10-31 鲲鹏 920 实测 `--quality=-1` 下 -n 2 与 -n 16×30s×4 用例全 pass；维持 SKIP 待晋升验证并与上游对齐）

- [ ] **Step 2: 重建** `SDC_BUILD_JOBS=20 ./scripts/build.sh` → 零新警告（注释不影响编译，仍按规则 2 走全链）

- [ ] **Step 3: 实跑受影响行为** `./builddir/sdcshield --quality=-1 -e eigen_svd_jacobi -t 5 -n 2` → `exit: pass`（引用真实输出）

- [ ] **Step 4: 回归** `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`

- [ ] **Step 5: x86 非回归（检视）** diff 仅含注释行，无代码语句变化

- [ ] **Step 6: commit -s + push 特性分支**（`git branch --show-current` 确认非 main）

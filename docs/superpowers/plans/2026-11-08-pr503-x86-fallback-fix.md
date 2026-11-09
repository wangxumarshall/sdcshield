# PR #503 x86 fallback 三缺陷最小修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this
> plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 PR #503 合入后 `sdc_joint_dim_slice` x86 `#else` fallback 的
P0 编译错误（8 参 `sdc_calibrated_dim` 调用无匹配重载）、P1 探针失败无退守、
P2 `fit_only` 未设——与 ARM 分支同构的最小修复，端到端验证后开修复 PR。

**Architecture:** 单文件最小编辑 `framework/sdc_stress_config.h` 的 x86
`#else` fallback：调用改为主模板 10 参形态（补第 9 参
`sdc_m3_probe_floor_ms`、第 10 参 `/*fit_only=*/true`，解决 P0+P2），并按
ARM 分支同构移植 `solve_probe` 包装器 + `probe_failed → lo` 退守尾部
（P1）；同文件两处共享模板注释中过时的「(联合求解, ARM-only)」表述同步
微调。ARM 分支（`#if defined(__aarch64__) && defined(__linux__)`）与全部
调用方零改动。验证以 `-U__aarch64__` stub harness（复用
`runlogs/pr-503pm/spike482/`，始终编译 worktree 活头文件）before/after
对比 + 真 framework 头 `-fsyntax-only` + aarch64 全量构建警告差分 +
zstd19/三 eigen 定向回归构成。

**Tech Stack:** C++20 (GCC 12), meson/ninja, g++ `-U__aarch64__` 预处理级
x86 模拟（fallback 代码全为 arch-independent API，本机 aarch64 执行结果对
x86-64 Linux 有效——#482 harness 既定口径）。

**Spec:** 任务指令（节点 0102）+ 复核材料 `runlogs/pr-503pm/findings.md`
（缺陷描述素材，非指令）。

## Global Constraints

- 只动 x86 `#else` fallback 与必要注释；ARM 分支零改动；不扩大任何
  `#ifdef`；其他文件仅注释级修正（本次核对结论：三处调用方 `#else` 注释
  与修复后行为一致，零改动——逐条记录于 Task 5）。
- 所有构建/实测命令经 `flock /home/wangxu/sdcshield/.buildlock` 包裹。
- 写路径仅限本 worktree 与 `/home/wangxu/sdcshield/verdicts|runlogs/`。
- 提交：单 commit，`git commit -s`，尾行
  `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无内容；只推
  `fix/503-x86-fallback`，绝不 push main，不 merge。
- 全部验证引用真实输出；未验证项写入 `verification_boundary`；时间预算
  3 小时，verdict 先落中间态再更新。

## Review Focus

1. **10 参形态参数位次**：`&fit` 必须在第 8 位、`sdc_m3_probe_floor_ms`
   第 9 位、`true` 第 10 位——位次错换可能编译通过但语义错（如把
   `true` 传进 `probe_floor_ms`）。由 harness 编译 RC=0 + fit 消费结果
   `n_run≈639` 钉死（Task 4/5）。
2. **P1 包装器只在 rc!=0 置位**：成功探针不得误判失败。由成功场景
   （n=n_run）与失败场景（n=lo）双跑对比钉死（Task 4）。
3. **`probe_failed` 声明位置**：必须在 solve 门外层（gate 不进入时尾部
   仍可读、无 -Wunused）。由 `-Wall -Wextra` 编译零新警告钉死（Task 4）。
4. **aarch64 零扰动**：注释/`#else` 文本改动不得影响 aarch64 可见代码。
   由全量构建警告差分 + 三 eigen 测试 `[联合求解]` 日志与 main 基线同构
   钉死（Task 6/7）。
5. **harness 必须编译 worktree 活头文件**（非陈旧拷贝）：由同一命令
   before（RC=1）/after（RC=0）翻转钉死（Task 3/4）。

---

### Task 1: 构建环境就绪 + main 基线构建

**Files:**
- Create: `third-party/{openssl,openblas,sleef,isa-l,acl}/install/`（自
  `prs/pr-503pm` 同源拷贝——同一 git 内 tarball 构建产物，主仓/worktree
  均无 install/；如实记录于 verdict）
- Create: `builddir/`（meson setup 生成）

**Steps:**
- [ ] 拷贝 5 个 install/ 目录到本 worktree third-party/ 下
- [ ] `flock … env PKG_CONFIG_PATH=$PWD/third-party/eigen5 meson setup builddir --buildtype=release`
- [ ] `flock … ./scripts/build.sh` 全量构建（main 原样，未改动）→ 记录
  target 总数与全部 warning 作为 main 基线
- [ ] verdict JSON 落中间态（IN_PROGRESS）

### Task 2: harness 准备（runlogs/pr-503fix/）

**Files:**
- Create: `runlogs/pr-503fix/spike482/{stub/, spike.cpp}`（复用复核
  harness，stub 头逐字拷贝；spike.cpp 增加失败注入模式：argv[1]='f' 时
  probe 恒返回 1——issue #420 自报失败形态）

**Steps:**
- [ ] 拷贝 stub/ 与 spike.cpp；扩展失败注入分支
- [ ] 编译命令固定为
  `g++ -std=c++20 -U__aarch64__ -Wall -Wextra -I stub -I <worktree>/framework spike.cpp -o spike`
  （`-I` 顺序保证 harness 用 worktree 活头文件）

### Task 3: BEFORE 证据（修复前，main 原样）

**Steps:**
- [ ] stub harness 编译 → 预期 **RC=1**（no matching function，8 参 + 非
  null `sdc_fit*`）——真实输出留档
- [ ] 真 framework 头 + builddir 生成头 `-fsyntax-only -U__aarch64__`
  （`framework/sandstone_opts.cpp` 等 TU，含全 include 路径）→ 预期
  **RC=1** 同一 error
- [ ] （补充对照，可选）以 `prs/pr-503pm/spike482/fixed9` 头编译同
  harness：P0-only 修复变体 RC=0——为 P1 的 A/B 对照（失败注入下
  n=8192=hi，证明无退守）备好二进制

### Task 4: 应用最小修复 + AFTER harness 验证

**Files:**
- Modify: `framework/sdc_stress_config.h` x86 `#else` fallback
  （`sdc_calibrated_dim` 调用处 + `solve_probe` 包装器 + `probe_failed`
  退守尾部 + 两处共享注释「(联合求解, ARM-only)」微调 + fallback 块内
  注释同步）

**Interfaces:**
- 修复后调用（与 ARM 分支 :1420-1423 同构）：
  `sdc_calibrated_dim(a->lo, a->hi, per_solve, test ? test->id : "(global)", knob, 3.0, solve_probe, &fit, sdc_m3_probe_floor_ms, /*fit_only=*/true);`
- 包装器与 ARM :1413-1418 逐字同构；退守尾部与 ARM :1434-1443 同构
  （log 文案逐字一致，便于跨 arch 日志检索）

**Steps:**
- [ ] 编辑 x86 `#else` fallback（P0+P2：10 参调用；P1：包装器 + 退守尾部）
- [ ] 同步注释：主模板 doc 注释与 probe 抬升注释中两处
  「(联合求解, ARM-only)」→「(联合求解——ARM 分支与 x86 fallback)」；
  fallback 调用点注释补 probe_floor/fit_only/退守说明；fallback 块注释
  「时间」bullet 补 #420 退守一句
- [ ] 核对三处调用方 `#else` 注释（sandstone_eigen_common.h /
  eigen_sparse.cpp / gemm_double_dynamic_square.cpp）与修复后行为一致
  性 → 预期零改动（结论记录进 commit 正文）
- [ ] stub harness 编译 → **RC=0 零新警告**
- [ ] 运行成功场景 `./spike x 500` → probe_calls=5（fit-only 跳过验证
  档）、`[时间校准]` fit-only 形态行、`[联合求解] n_mem=… n_run≈639 →
  639`、`n=min(n_mem, n_run, hi)` 语义保持——真实输出留档
- [ ] 运行失败场景 `./spike f x 500` → probe_calls=2、
  `[时间校准] … 试解探针自报失败 → 退守校准下限`、
  `[联合求解] … 校准探针自报失败 → 退守 lo=64 (… n_mem=… 弃用; issue
  #420)`、result n=64——真实输出留档
- [ ] （补充对照）fixed9 变体失败注入运行 → n=8192（P1 缺陷的活体证
  据），与本修复 n=64 对比
- [ ] 真 framework 头 `-fsyntax-only -U__aarch64__` 复核 → **RC=0**

### Task 5: aarch64 全量重建零新警告

**Steps:**
- [ ] `flock … ./scripts/build.sh`（头文件改动触发包含该头的 TU 重编）
  → warning 集合与 Task 1 基线逐条对比，**零新警告**（既有警告不归于
  本修复，如实区分）

### Task 6: 定向回归 + 受影响行为实测

**Steps:**
- [ ] `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`
- [ ] `-e eigen_sparse -t 2000 -n 1`、
  `-e eigen_gemm_double_dynamic_square -t 2000 -n 1`、
  `-e eigen_svd_double -t 2000 -n 1` → 全 pass；ARM 分支
  `[联合求解]`/`[时间校准]` 日志与 main 基线同构（数值因 main 前进的
  #552 AR 表重标定可不同，结构同构即可，如实记录）
- [ ] x86-64 非回归（检查法）：`git diff` 确认只动 `#else` 分支与注
  释、未扩大 `#ifdef __x86_64__`、未改 x86 既有逻辑

### Task 7: 提交 + 开修复 PR + verdict

**Steps:**
- [ ] `git add -A && git commit -s`（单 commit，标题
  `fix: PR503 x86 fallback 补 sdc_calibrated_dim 第9参/fit_only 与 probe_failed 退守`，
  正文列三条缺陷、修复方式、验证证据摘要、调用方注释核对结论）
- [ ] `git push origin fix/503-x86-fallback`（绝不推 main）
- [ ] `gh pr create --base main --head fix/503-x86-fallback`（正文含三
  条缺陷、修复方式、全部验证证据原文摘录、与 PR #503 验证输出的 fit-only
  差异对照说明）
- [ ] verdict JSON 写入 `/home/wangxu/sdcshield/verdicts/pr-503fix.json`
  （`new_pr` 填新 PR 号；全部通过且 PR 已开出 → PASS）

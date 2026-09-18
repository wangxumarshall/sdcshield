# eigen_svd_cdouble_sve 超时回归修复计划（根因：Eigen5 SVE 后端无 double packet 支持）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `eigen_svd_cdouble_sve` 必然超时的问题，使其恢复诚实可运行的形态。全量运行（`sdcshield --quality=-1`）在此测试处超时中止（`result: timed out`、4 child 被 SIGQUIT、`exit: invalid`），后续 45 个测试漏跑。

## 根因（2026-09-18 实测 + 源码证实，两层叠加）

**第 1 层（表层）：M_DIM 2100 回归。** commit `78921778`（2026-09-14）把 M_DIM 300→2100，注释声称 "matches eigen_svd_cdouble" 但 NEON 原版仍是 300。无 rationale（commit 无正文、docs/plans 无记录）。

**第 2 层（深层根因）：Eigen 5.0 的 SVE 后端没有 `double`/`complex<double>` packet_traits。** `third-party/eigen5/Eigen/src/Core/arch/SVE/PacketMath.h` 只特化了 `int32_t`（L37）和 `float`（L331）；`packet_traits<double>` 落入通用模板（GenericPacketMath.h:109）→ `Vectorizable = 0, size = 1`。后果链：
1. `apply_rotation_in_the_plane`（Jacobi.h:411）判 `Vectorizable=false` → 标量循环（Jacobi.h:286 分支）
2. BDCSVD base case = JacobiSVD，其 O(n²)·迭代全在 apply_rotation_in_the_plane 上
3. gdb 采样证实：卡在 `EigenSVE::internal::apply_rotation_in_the_plane_selector<double,...,false>::run`（模板参数 `false` = non-vectorizable）
4. **独立最小复现**（/tmp/eigen_sve_test/svd_test.cpp，300×300 double BDCSVD）：NEON 后端 29ms；SVE 后端（`-DEIGEN_ARM64_USE_SVE -march=armv8.2-a+sve -msve-vector-bits=128`，与本测试完全相同的编译参数）**10 分钟未完成**——慢 ≥20,000 倍
5. 因此该测试**在任何硬件、任何维度**下都超 300s 框架超时（`test_timeout()` 下限 300s）；M_DIM=300 也超时（实测 300s timed out，maxrss 23MB、99.9% CPU 纯计算）

**结论：** 该测试名不副实——"SVE 向量后端压测 SVD" 实际是标量后端慢 4 个数量级的伪 SVE 代码。在 Eigen 上游补齐 SVE double packet（或本仓 vendored Eigen5 加特化）之前，它不可能按设计意图运行。

**LTS 15/15 PASS 为何没发现：** `option-matrix.sh` 全条目带 `--ignore-timeout`，超时被显式容忍为"非缺陷"。

## Architecture（修复方案）

两步走，one-patch-per-unit：

1. **Task 1 — 测试改为诚实 skip（占位模式）**：参照 `tests/cpu/ist/ist.c` 的 placeholder 惯例，`sve_probe_and_init` 在 SVE 检测后追加 Eigen 后端能力检测（编译期已知 `packet_traits<double>::size == 1`），`log_skip("to be implemented (placeholder): Eigen5 SVE backend has no double/complex<double> packet support (scalar fallback ~20000x slower than NEON); upstream Eigen SVE double packet pending")` + `return EXIT_SKIP`。同时 M_DIM 回退到 300（保持与 NEON 版同构，未来后端补齐后立即可用）。测试符号保留（built & linked），行为诚实。**SVE 硬件检测保留在前**（无 SVE 硬件时 skip 理由仍是 CpuNotSupported——区分"硬件不支持"与"后端未实现"两个维度）。
2. **Task 2 — 文档同步**：README.md L357 与 usermanual 描述该测试"仅 SVE 硬件运行"——需更新为"当前为诚实占位 skip（Eigen SVE double 后端未实现）"。CLAUDE.md 平台怪癖节追加此事实。

不选的方案（记录理由）：
- **修 vendored Eigen5 加 double 特化**：~500 行 packet 代码（pload/pstore/pmul/predux/svd 所有 op），超出本次 bug fix 范围，风险高（数值正确性需逐项验证），应作为独立 feature plan。
- **删测试**：违反本仓"占位诚实保留"惯例（ist/smi_count 先例）。
- **改框架超时**：300s 下限是框架设计，且 20000 倍慢不是超时参数能解决的。

**Tech Stack:** 纯测试源码改动 + 文档；无框架/meson 改动；全部在 `#if defined(__aarch64__)` 内，x86-64 零影响。

**Spec:** 占位惯例 `tests/cpu/ist/ist.c`；skip 分类 `framework/sandstone.h`（TestResourceIssueSkipCategory / CpuNotSupportedSkipCategory）；CLAUDE.md「Placeholder-test honesty」节。

## Global Constraints

- **one-patch-per-unit**：Task 1（测试改动）与 Task 2（文档）各一个 commit。
- **验证 100% 真实**：引用真实命令输出。skip 后 `-e eigen_svd_cdouble_sve` 必须 `result: skip` + 框架 `exit: pass`（skip 不算失败）；全量 `--quality=-1` 必须 298/298 有结果、无 timed out、`exit: pass`。
- **x86-64 非回归**：改动仅 `__aarch64__`-guarded 文件。
- **不删测试符号**：保持 built/linked（与 ist 相同模式）。
- **诚实原则**：skip 理由必须说清"为什么"（后端缺 double packet + 实测数据），不粉饰。

---

### Task 1: eigen_svd_cdouble_sve 改为诚实占位 skip + M_DIM 回退

**Files:**
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp`

**Interfaces:**
- 无接口变化。测试仍 built/linked；`test_init` 返回 EXIT_SKIP。

- [x] **Step 1: 建分支** `git checkout -b fix/svd-cdouble-sve-mdim-timeout`（已完成）
- [x] **Step 2: M_DIM 2100→300** + 注释修正（已完成——保留，未来后端补齐即同构可用）
- [x] **Step 3: init 增加 Eigen SVE double 后端能力检测 → EXIT_SKIP**（编译期常量判定，运行期 log_skip）
- [x] **Step 4: 构建** `ninja -C builddir` 零新警告/错误
- [x] **Step 5: 功能验证**:
  - `./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble_sve -n 1` → `result: skip`（理由含 "to be implemented (placeholder)"），整体 `exit: pass`
- [x] **Step 6: 回归**:
  - `./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble -n 1` → pass（NEON 版）
  - `./builddir/sdcshield -e zstd19 -t 3000 -n 1` → pass
- [x] **Step 7: 全量确认**：`./builddir/sdcshield --quality=-1` → 298 测试全有结果（无 timed out、无中止），`exit: pass`
- [x] **Step 8: commit + push**

### Task 2: 文档同步

**Files:**
- Modify: `README.md`（L357 附近）、`docs/multi-version-build-deploy-usermanual.md`（L138-142）、`CLAUDE.md`（平台怪癖节）

- [x] **Step 1: 更新三处描述**：该测试当前为占位 skip（Eigen5 SVE 后端无 double packet），SVE 硬件检测仍在；后端补齐后回退此 skip 即恢复
- [x] **Step 2: commit**（与 Task 1 分开）

---

## 验证命令汇总

```console
ninja -C builddir
./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble_sve -n 1   # expect: skip + exit pass
./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble -n 1       # expect: pass
./builddir/sdcshield -e zstd19 -t 3000 -n 1                        # expect: pass
./builddir/sdcshield --quality=-1                                   # expect: 298 tests, exit pass
```

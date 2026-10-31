# Issue #319 TestData ODR 收尾 + 同类 12 族 69 文件匿名命名空间修复 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 #319 全仓排查出的所有"跨 TU 同名不同布局全局 struct"ODR 违规（12 族 69 文件，匿名命名空间包裹），并以其证据链正确收尾 issue #319（其点名的 TestData 缺陷已在 main 修复）。

**Architecture:** 纯 C++ 作用域修复：把每个受影响 TU 的目标 struct 定义包进匿名 namespace（internal linkage → 构造/析构/模板成员符号变为 TU 局部 `t` 符号，链接器永不再跨 TU 合并）。与 e772741a 已确立的 64 文件修复模式逐字一致（`namespace { /* ODR: per-TU <Name> (N-file same-name hazard) */` + `}`），不触碰任何 x86 专用路径、不改任何成员布局、不改任何行为逻辑。

**Tech Stack:** C++20/23（meson `gnu++23`）、GCC 12.3 aarch64、ninja/meson、nm/objdump/addr2line 符号级验证。

**Spec:** 本计划自身即 spec（issue #319 + 编排者指令"全仓排查同类模式，一次修复同一根因的全部文件"）。issue 正文：`gh issue view 319`。

## Global Constraints

- 一个根因 = 一个 unit = 一个 commit = 一个 PR（CLAUDE.md 规则 1；编排者明确指令）。
- x86-64 不触碰：本修复是 arch 中立的 C++ 作用域包裹，不进入任何 `#ifdef __x86_64__` 分支（CLAUDE.md 规则 5，非回归由目检保证）。
- 修复前必须先在本 worktree 干净构建一次（规则 2a：零新增警告），修复后再构建对比。
- 受影响行为实测（规则 2b）：-O0 单 TU 符号发射对比（修复前 W → 修复后 t/消失）+ 受影响测试 release 实跑。
- 回归（规则 2c）：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
- commit 尾注仅 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，禁止 Co-Authored-By；只 push feature 分支。
- **不修**：`fma.cpp:28` / `fma_sve.cpp:32` 的 `struct TestData {};`（token 级一致、零使用的死声明，ODR 合规）；token 级一致的同名族（SpinLock/movbe_data/MrnRmwData/MrnRmwSveData/aes_xcbc_sse_data）；已在匿名 namespace 内的族（TestData 64 文件、crt_data、sleef_test_data、eigen_test_data 7 文件）。
- **framework `class Topology` 三变体（cpu/idxd/gpu 头文件同名不同布局）不构成同类隐患**：`framework/meson.build` 的 `device_type` 为互斥 if/elif（cpu|gpu|idxd 三选一，默认 cpu），任何单一构建只编译一个变体，永不共存于同一链接目标；ARM64 构建实测 `builddir/framework/device/` 仅含 `cpu`。单一定义的 COMDAT（如 `W Topology::~Topology()`）ODR 合法。（检视轮次 1 提出质疑，机械核实为误报，记录于此堵未来审计再问。）

---

## 背景（批判性分析结论，全部实证）

### #319 点名的缺陷：已在 main 修复

- `e772741a`（2026-10-18，已验证为 HEAD 祖先）：64 文件 TestData 包匿名命名空间，commit 正文含 ASAN 实证（`TestData::TestData()` 来自 sve/mesh/mesh_upi_sve_wide_asymm_distrib_int.cpp:23 却构造在 read_L3_int.cpp:21 的 32 字节 new 上）。
- `d8a42fe2`（2026-10-21，HEAD 祖先）：恢复合并 78b82e1c 丢失的 4 处开括号。
- issue #319（2026-10-29 创建）的 crash 表来自**仓库外**战役 checkout `/home/sdc/root-xupeng/sdcshield`（钉在 `5e8e2d6d`，2026-10-13，早于修复 5 天；其 mesh 文件 namespace 出现次数 = 0）。
- 战役 `-O0` 构建（`builddir-o0/`，二进制 2026-10-30 生成）中实测：全程序仅一个 `TestData::TestData()`（t/W @0x19e2f4），胜者 `mesh_upi_avx_sym.cpp:18`，构造函数向 +0x8/0x10/0x18/0x1c/0x20/0x24 偏移写入（≥40B 布局）；崩溃的 5 测试 `new` 仅 8-24B → 越界写 → glibc 堆损坏 → SIGABRT。机制/胜者/方向全部实锤。
- -O1 及以上（含仓库 release -O3、CI 夜巡 debugoptimized -O2+ASan）不发射越界构造符号——所以 release 二进制（含当前 main）无此载体。
- 当前 main 66 个 `struct TestData`：64 个已匿名包裹，2 个为 token 一致空结构体（`fma.cpp:28`、`fma_sve.cpp:32`，零使用）。**#319 本体结论 = 已解决。**

### 全仓同类排查：12 族 69 文件仍是同根因缺陷（本计划修复对象）

审计方法：全仓 `tests/**`+`framework/**` 扫描 `^(static )?(typedef )?(struct|class) [alignas(..) ]?NAME`，命名空间栈追踪，正文去注释/空白后按 token 比较；"族"= 同名（全局作用域）跨 ≥2 TU 且 ≥2 种 token 不同正文。

| 族 | 文件数 | 布局差 | -O0 载体（实测） | 风险 |
|---|---|---|---|---|
| SharedData | 29 | 16 种 | W `_ZN10SharedDataC1Ev` 多 TU 发射；战役 -O0 二进制已合并（胜者 cachebounce.cpp:8 旧版） | **活** |
| OooDepData | 2 | 2 种 | W ctor+dtor+`unique_ptr`/`default_delete` 模板两 TU 发射；战役 -O0 二进制已合并（胜者 arm 版：vector@0/24/48，sve 版 vector@16/40/64 → sve 的 delete 把 size_t 当指针 free） | **活** |
| SmeFmopaData | 2 | 2 种 | W dtor 两 TU 发射（布局同构，实害为零，形式违规） | 形式 |
| ZeroControlData | 3 | 2 种 | 无（全平凡成员） | 形式 |
| aes_hmac_avx_data | 10 | 2 种 | 无 | 形式 |
| aes_hmac_sse_data | 9 | 2 种 | 无 | 形式 |
| aes_xcbc_avx_data | 4 | 2 种 | 无 | 形式 |
| aes_cmac_avx_data / aes_gcm_avx_data / aes_gcm_avx512_data / cmac_avx512_data / xcbc_avx512_data | 各 2 | 各 2 种 | 无 | 形式 |

69 文件已逐一验证：命中行均为类型定义（非 `struct X var = {` 变量定义）、无 typedef 形态、无同文件前向声明冲突。

修复方式沿用 e772741a 已确立惯例（匿名命名空间，非重命名）：改动最小（每文件 2 行）、可机械验证（审计脚本 + -O0 nm）。

---

### Task 1: 写修复脚本并应用到 69 文件

**Files:**
- Create: `/tmp/wrap_odr_structs.py`（一次性脚本，不入库）
- Modify: 69 个测试文件（清单见下，每文件在目标 struct 定义行前插 `namespace { /* ODR: per-TU <Name> (N-file same-name hazard) */`、定义闭合 `};` 行后插 `}`）

**Interfaces:**
- Produces: 69 文件的 struct 均为 internal linkage；后续 Task 2/3/4 依赖此状态。

69 文件清单（12 族）：

```
OooDepData(2): tests/cpu/arm64/ooo_dep_chain_arm.cpp tests/cpu/sve/mem/ooo_dep_chain_sve.cpp
SharedData(29): tests/cpu/atomic/atomic_simd_128.cpp tests/cpu/atomic/atomic_simd_128_unaligned.cpp
  tests/cpu/atomic/atomic_simd_256.cpp tests/cpu/atomic/atomic_simd_256_unaligned.cpp
  tests/cpu/cache/cache_stress_aggressor.cpp tests/cpu/cache/cachebounce.cpp
  tests/cpu/lock/lock_instr.cpp tests/cpu/lock/lockgen.cpp tests/cpu/lock/lockless_cmpxchg.cpp
  tests/cpu/lock/lockless_cmpxchg16b.cpp tests/cpu/lock/lockless_cmpxchg8b.cpp tests/cpu/lock/locks.cpp
  tests/cpu/lock/locks_xch_only.cpp tests/cpu/spinlock/spinlock_array.cpp tests/cpu/spinlock/spinlock_bank.cpp
  tests/cpu/spinlock/spinlock_cross_socket.cpp tests/cpu/spinlock/spinlock_mini.cpp
  tests/cpu/spinlock/spinlock_randomsweep.cpp tests/cpu/spinlock/spinlock_rmw_cmpxchg.cpp
  tests/cpu/spinlock/spinlock_rmw_cmpxchg_pause.cpp tests/cpu/spinlock/spinlock_rmw_xchg.cpp
  tests/cpu/spinlock/spinlock_rmw_xchg_pause.cpp tests/cpu/spinlock/spinlock_same_core.cpp
  tests/cpu/spinlock/spinlock_stress.cpp tests/cpu/spinlock/spinlock_stress_bts.cpp
  tests/cpu/spinlock/spinlock_stress_cmpxchg.cpp tests/cpu/spinlock/spinlock_stress_cmpxchg16b.cpp
  tests/cpu/spinlock/spinlock_stress_cmpxchg_even_odd.cpp tests/cpu/spinlock/spinlock_with_hle.cpp
SmeFmopaData(2): tests/cpu/arm64/sme_fmopa_stress_arm.cpp tests/cpu/arm64/sme_fmopa_za_arm.cpp
ZeroControlData(3): tests/cpu/jit/zero_control_vec_evex_jit.cpp tests/cpu/jit/zero_control_vec_sse_jit.cpp
  tests/cpu/jit/zero_control_vec_vex_jit.cpp
aes_cmac_avx_data(2): tests/cpu/ipsec/aes128_cbc/ipsec_aes128_cbc_aes_cmac_avx.cpp
  tests/cpu/sve/ipsec/ipsec_aes128_cbc_aes_cmac_sve.cpp
aes_gcm_avx512_data(2): tests/cpu/ipsec/aes192_gcm/ipsec_aes192_gcm_avx512.cpp
  tests/cpu/sve/ipsec/ipsec_aes192_gcm_sve_wide.cpp
aes_gcm_avx_data(2): tests/cpu/ipsec/aes192_gcm/ipsec_aes192_gcm_avx.cpp
  tests/cpu/sve/ipsec/ipsec_aes192_gcm_sve.cpp
aes_hmac_avx_data(10): tests/cpu/ipsec/aes192_cbc/ipsec_aes192_cbc_sha2_384_avx.cpp
  tests/cpu/ipsec/aes192_cbc/ipsec_aes192_cbc_sha2_512_avx.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha1_avx.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha224_avx.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha256_avx.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha384_avx.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha512_avx.cpp
  tests/cpu/ipsec/aes256_cbc/ipsec_aes256_cbc_hmac_sha1_avx.cpp
  tests/cpu/ipsec/aes256_cbc/ipsec_aes256_cbc_sha2_224_avx.cpp
  tests/cpu/ipsec/aes256_cbc/ipsec_aes256_cbc_sha2_256_avx.cpp
aes_hmac_sse_data(9): tests/cpu/ipsec/aes192_cbc/ipsec_aes192_cbc_sha2_384_sse.cpp
  tests/cpu/ipsec/aes192_cbc/ipsec_aes192_cbc_sha2_512_sse.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha1_sse.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha224_sse.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha256_sse.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha384_sse.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_hmac_sha512_sse.cpp
  tests/cpu/ipsec/aes256_cbc/ipsec_aes256_cbc_hmac_sha1_sse.cpp
  tests/cpu/ipsec/aes256_cbc/ipsec_aes256_cbc_sha2_224_sse.cpp
aes_xcbc_avx_data(4): tests/cpu/ipsec/aes192_cbc/ipsec_aes192_cbc_xcbc_96_avx.cpp
  tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_xcbc_avx.cpp
  tests/cpu/sve/ipsec/ipsec_aes192_cbc_xcbc_96_sve.cpp
  tests/cpu/sve/ipsec/ipsec_aes192_ctr_xcbc_sve.cpp
cmac_avx512_data(2): tests/cpu/ipsec/3des_docsis/ipsec_3des_docsis_aes_cmac_avx512.cpp
  tests/cpu/sve/ipsec/ipsec_3des_docsis_aes_cmac_sve_wide.cpp
xcbc_avx512_data(2): tests/cpu/ipsec/3des_docsis/ipsec_3des_docsis_aes_xcbc_avx512.cpp
  tests/cpu/sve/ipsec/ipsec_3des_docsis_aes_xcbc_sve_wide.cpp
```

- [ ] **Step 1: 修复前基线快照**

```bash
cd /home/sdc/wangxu/sdcshield/.claude/worktrees/agent-afd762fc598eef572
git status --short | wc -l   # 期望 0（干净树）
grep -c "^namespace { /\* ODR" tests/cpu/lock/locks.cpp   # 期望 0（尚未包裹）
```

- [ ] **Step 2: 编写并运行包裹脚本**

脚本逻辑（每文件）：定位目标 struct 定义行（正则 `^(static\s+)?(typedef\s+)?struct\s+(alignas\([^)]*\)\s*)?NAME\s*(\{|:|$)`，且下一非空行以 `{` 或 `:` 开头——排除变量定义）；从该行做花括号配对找到闭合 `};`；在定义行前插入 `namespace { /* ODR: per-TU <NAME> (<N>-file same-name hazard) */`，在 `};` 行后插入 `}`。若文件已有包裹则跳过并报告。

```bash
# /tmp/wrap_odr_structs.py 按上述逻辑实现后：
python3 /tmp/wrap_odr_structs.py --apply
# 期望输出：69 files wrapped, 0 skipped
git diff --stat | tail -1   # 期望 69 files changed, 138 insertions(+), 0 deletions(-)
```

- [ ] **Step 3: 逐文件抽查 diff 正确性**

```bash
git diff tests/cpu/lock/locks.cpp | head -20   # 确认 namespace{ 在 struct SharedData 前、} 在 }; 后
git diff tests/cpu/cache/cachebounce.cpp | head -20  # alignas 形式同样正确
git diff tests/cpu/sve/ipsec/ipsec_aes192_gcm_sve.cpp | head -12
```

期望：每文件恰 +2 行；无成员/逻辑改动。

### Task 2: 干净构建（修复前基线 + 修复后对比）

**Files:** 无源码改动；构建产物在 `builddir/`。

- [ ] **Step 1: 修复后全量构建（worktree 为新 checkout，需 setup）**

```bash
cd /home/sdc/wangxu/sdcshield/.claude/worktrees/agent-afd762fc598eef572
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
SDC_BUILD_JOBS=20 ./scripts/build.sh 2>&1 | tail -5
```

期望：构建成功；**零新增警告**（对照：日志中不得出现指向 69 文件的新 warning；特别留意 `-Wdeprecated` / unused 警告）。

- [ ] **Step 2: 构建失败则修复重试（不留"应该没问题"）**

### Task 3: 符号级 ODR 消除验证（核心证据）

**Files:** 无源码改动；`builddir/compile_commands.json` + 临时 .o。

- [ ] **Step 1: 修复后 -O0 单 TU 编译，验证弱符号消失**

用本 worktree `builddir/compile_commands.json` 的真实编译命令（剥 `-MQ/-MF/-MD`，`-O3`→`-O0`）编译代表性 TU 对：
`tests/cpu/lock/locks.cpp`、`tests/cpu/spinlock/spinlock_mini.cpp`、`tests/cpu/lock/lockless_cmpxchg.cpp`（SharedData 活雷族）；`tests/cpu/sve/mem/ooo_dep_chain_sve.cpp`、`tests/cpu/arm64/ooo_dep_chain_arm.cpp`（OooDepData 活雷族）。

```bash
nm <o> | grep -E "_ZN10(SharedData|OooDepData)(C|D)[12]Ev"
```

期望（与修复前实测对照）：
- 修复前：`W _ZN10SharedDataC1Ev`（多 TU）/ `W _ZN10OooDepDataC1Ev` + `D1/D2` + `unique_ptr`/`default_delete` 模板 W 符号（两 TU）。
- 修复后：**0 个全局名 W 符号**；若有则为 `_ZN12_GLOBAL__N_1...` 前缀的局部符号（`t`，永不跨 TU 合并）。

- [ ] **Step 2: 审计脚本复跑**

修复后全仓审计（Task 1 同款脚本，不 `--apply`）：
期望：`UNPROTECTED same-name families with >=2 distinct bodies` = **0**。

- [ ] **Step 3: release 二进制符号检查**

```bash
nm -C builddir/sdcshield | grep -E "SharedData::|OooDepData::|SmeFmopaData::|aes_.*_data::"
```

期望：无跨 TU 合并的全局名构造/析构符号。

### Task 4: 受影响行为实测 + 回归

- [ ] **Step 1: 5 个 issue 崩溃测试（TestData 族，main 已修，本 PR 未触碰，证不回归）**

```bash
./builddir/sdcshield -e mesh_upi_avx2_read_L3_int -t 20 -n 4
./builddir/sdcshield -e mesh_upi_avx2_read_only_int -t 20 -n 4
./builddir/sdcshield -e mesh_upi_avx2_write_only_int -t 20 -n 4
./builddir/sdcshield -e mesh_upi_avx512_asymm_read_L3_int -t 20 -n 4
./builddir/sdcshield -e mesh_upi_avx512_asymm_read_only_int -t 20 -n 4
```

期望：全部 `exit: pass`（本机为 release 构建，本就不在 -O0 载体下；记录如实结果）。

- [ ] **Step 2: 修复直接触及的族实跑（SharedData/OooDep/aes/jit 代表）**

```bash
./builddir/sdcshield -e locks -t 10 -n 4
./builddir/sdcshield -e spinlock_mini -t 10 -n 4
./builddir/sdcshield -e ooo_dep_chain_arm -t 10 -n 2
./builddir/sdcshield -e ooo_dep_chain_sve -t 10 -n 2
./builddir/sdcshield -e ipsec_aes192_ctr_hmac_sha256_avx -t 10 -n 2
./builddir/sdcshield -e zero_control_vec_vex_jit -t 10 -n 2
```

期望：pass 或诚实 SKIP（如 openssl 依赖缺失/质量级过滤），不得 crash。

- [ ] **Step 3: 标准回归**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1
```

期望：`exit: pass`。

- [ ] **Step 4: x86 非回归目检**

`git diff` 全量过目：仅 69 文件各 +2 行作用域包裹，无任何 `#ifdef __x86_64__`/`__aarch64__` 条件改动，无成员/布局/逻辑改动。

### Task 5: 提交、PR、检视、merge、issue 收尾

- [ ] **Step 1: commit（feature 分支，-s，无 Co-Authored-By）**

```bash
git branch --show-current    # 确认非 main
git add -A && git commit -s  # 正文含根因/证据/验证摘要
git push origin <branch>     # 绝不 push main
```

- [ ] **Step 2: PR**

`gh pr create --base main`，正文含：根因分析（ TestData 已修 + 同类 12 族机制与 -O0 载体实测）、修法、全部验证证据原文、`Refs #319`（不用 Fixes——#319 点名缺陷已由 e772741a 修复）。

- [ ] **Step 3: feature-dev:code-reviewer 子代理批判性检视**

blocking findings → 修复 → push → 循环至无 blocking。

- [ ] **Step 4: merge**

```bash
gh pr merge <PR> --merge   # main 前进冲突则 rebase 后重试
```

- [ ] **Step 5: 关闭 #319（证据链收尾）**

`gh issue close 319 --comment "<结论>"`：TestData 缺陷已由 e772741a+d8a42fe2 修复（附本会话机械验证：64/66 匿名包裹、2 个 token 一致空死声明、release/-O0 符号证据）；crash 表来自仓库外战役 checkout 5e8e2d6d（2026-10-13，修复前 5 天）的 -O0 构建，建议同步该 checkout；同类 12 族 69 文件由本 PR 消灭。

## Self-Review

- Spec 覆盖：编排者 4 项要求——(1) ODR 真实性判定→已完成（背景节全实证）；(2) 修复选型→匿名命名空间（e772741a 惯例，Task 1）；(3) 全仓同类排查→12 族 69 文件（Task 1 清单）；(4) x86 目检→Task 4 Step 4。#319 本体 close→Task 5 Step 5。
- 占位符扫描：无 TBD/TODO；所有命令与期望输出具体。
- 类型一致性：族名/文件数在背景表、Task 1 清单、Task 3 验证间一致（12 族 69 文件）。

# GEMM 测试 M_DIM 尺寸谱系化（knob 化）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 openblas_{d,s,z}gemm 三个测试的矩阵尺寸从编译期钉死 `M_DIM=256` 改为运行期 test-knob（`-O <testid>.mdim=N`），使同一二进制可扫 L1→L2→LLC→DRAM 全谱工作集，最大化 SDC 激发覆盖（CORE179 探针 E 证明 store/reload 的 cache-domain 与跨线模式是触发判别条件；单一 256 尺寸只踩 L2 边界一个点）。

**Architecture:** 用框架既有 test-knob 通道（`-O <testid>.mdim=N`，`get_testspecific_knob_value_int`，zstd 的 `level`/`maxbuffersize` 先例）在 `test_init` 读尺寸，动态分配矩阵（malloc 替代固定数组），golden 在 init 期按当次尺寸算好。三个测试一个一个改（一补丁一单元）。默认值保持 256（零行为变化，向后兼容）；合法域 16–1024，越界 fail-loudly 并提示合法范围。

> **CLI 形式更正（Task 1 实施时发现）**：框架 `TestKeyWrapper`（framework/test_knobs.cpp）以 `<testid>.<key>` 为查找键，而 `-O key=value` 按字面存储——**裸 `-O mdim=N` 不生效（静默回退默认 256）**，生效形式是 `-O openblas_dgemm.mdim=N` 等带测试 ID 前缀的形式（zstd 的裸 `level=3` "先例"实为默认值巧合）。本文件下方所有命令示例按生效形式执行。

**Tech Stack:** 框架 test-knob API（`framework/test_knobs.h`）、OpenBLAS cblas（尺寸无关调用——`M/N/K` 与 `lda/ldb/ldc` 全是运行期参数，这正是 CBLAS API 的设计意图）、malloc/golden-memcmp 既有骨架。

**Spec:** `docs/superpowers/plans/2026-09-15-third-party-sdc-libs.md`（原始测试骨架）+ CORE179 报告（`docs/cases/CORE179_SDC_REPORT_CN.md` §6.1 探针 E/F：store 须同 LLC domain + 跨 cache line——尺寸谱系让工作集依次压过 L1/L2/LLC/DRAM，覆盖各 domain 的转发路径）+ SDC 研究综合（`docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md` §7.3 第 6 条"足迹旋钮"）。

## Global Constraints

- **默认行为零变化**：不传 knob 时 `M_DIM=256`、`fracture_loop_count=4`、所有现有验证输出不变（`-e openblas_dgemm -t 5000 -n 1` 仍 pass，分片节奏不变）。
- **合法尺寸域 16–1024**（uint64 knob；`< 16` 或 `> 1024` → `report_fail_msg` 带合法范围提示，fail-loudly 不静默钳制）。1024 上限的依据：本机 29GB 内存，zgemm M=1024 全核 = 128 线程 × 48MB scratch ≈ 6GB + 共享 16MB，安全；2048 会 24GB+ 触发 OOM-killer。
- **尺寸语义**：方阵 N=M=K=mdim，`lda=ldb=ldc=mdim`（与现状一致，只是值从宏变运行期变量）。
- **golden 确定性不依赖尺寸**：golden 在 init 用同一 cblas 调用对同一输入算——任意合法尺寸下逐位确定（这是既有判据结构，无需改动）。
- **thread_local scratch 的尺寸感知**：scratch 按 init 算出的当次尺寸分配；尺寸是进程级一致的（knob 在 fork 前由父进程解析），worker 线程间无分歧。
- **knob 键名统一 `mdim`**（三个测试同一键名，文档一处说明）。
- **每测试一个 commit**（dgemm → sgemm → zgemm），文档同步一个 commit，共 4 个。
- **验证必须真实跑**：每档尺寸（64/256/1024 至少三档）单核 + 全核 + 默认档回归，引用真实输出。
- **x86-64 非回归**：全部改动在 aarch64-guarded openblas 测试文件内。
- **不改 meson**：knob 是纯运行期机制，构建系统零改动。

---

### Task 1: openblas_dgemm 尺寸 knob 化

**Files:**
- Modify: `tests/cpu/openblas_gemm/dgemm.cpp`（全文件重构：宏→init 期动态尺寸）

**Interfaces:**
- Consumes: `get_testspecific_knob_value_int(test, "mdim", 256)`（framework/test_knobs.h，C 链接）；既有 cblas_dgemm/memcmp_or_fail/random_bounded/thread_local 骨架。
- Produces: 测试 `openblas_dgemm` 接受 `-O openblas_dgemm.mdim=N`（16–1024）；struct `gemm_test_data` 增加 `int mdim` 字段（Task 2/3 的实施者照此模式改各自文件）。

- [x] **Step 1: 重构 dgemm.cpp**

改动清单（对现有文件逐处）：
1. 删 `#define M_DIM 256`；struct 增加 `int mdim;`。
2. init 开头读 knob + 域检查（Task 4 已同步为 int64 域检形式，防 `(int)` 截断别名）：
```cpp
    int64_t knob = get_testspecific_knob_value_int(test, "mdim", 256);
    if (knob < 16 || knob > 1024) {
        report_fail_msg("mdim knob out of range: %ld (valid 16..1024, default 256)", (long)knob);
    }
    d->mdim = (int)knob;
    size_t n2 = (size_t)d->mdim * (size_t)d->mdim;
```
3. init 里所有 `M_DIM` 替换为 `mdim`（随机填充循环边界、cblas_dgemm 的 M/N/K/lda/ldb/ldc）。
4. run 里所有 `M_DIM` 替换为 `d->mdim`（`bytes = (size_t)d->mdim * d->mdim * sizeof(double)`、cblas 调用、`memcmp_or_fail(c, d->golden, (size_t)d->mdim * d->mdim)`）。
5. 头部 @test 注释块追加一行说明 knob 用法（生效形式 `-O openblas_dgemm.mdim=N`）。
6. `report_fail_msg` 的 OOM 消息不变（已是 n2 相对量）。

- [x] **Step 2: 构建**

Run: `ninja -C builddir`
Expected: 0 error 0 新 warning。

- [x] **Step 3: 默认档回归（零行为变化验证）**

```bash
./builddir/sdcshield -e openblas_dgemm -t 5000 -n 1        # pass（与改前同）
./builddir/sdcshield -e openblas_dgemm -t 5000             # 全核 pass
```

- [x] **Step 4: 谱系档验证（三档：L1/L2/超LLC）**

```bash
./builddir/sdcshield -O openblas_dgemm.mdim=64   -e openblas_dgemm -t 5000 -n 1   # 32KB/矩阵: pass
./builddir/sdcshield -O openblas_dgemm.mdim=256  -e openblas_dgemm -t 5000 -n 1   # 512KB: pass
./builddir/sdcshield -O openblas_dgemm.mdim=1024 -e openblas_dgemm -t 5000        # 8MB, 全核: pass
```

- [x] **Step 5: 越界档 fail-loudly 验证**

```bash
./builddir/sdcshield -O openblas_dgemm.mdim=8    -e openblas_dgemm -t 3000 -n 1   # 预期 fail, 消息含 "out of range"
./builddir/sdcshield -O openblas_dgemm.mdim=2048 -e openblas_dgemm -t 3000 -n 1   # 预期 fail, 消息含 "out of range"
```

- [x] **Step 6: 同 seed 确定性（非默认档也要验）**

```bash
./builddir/sdcshield -O openblas_dgemm.mdim=512 -e openblas_dgemm -t 5000 -n 1 -s LCG:12345   # pass
./builddir/sdcshield -O openblas_dgemm.mdim=512 -e openblas_dgemm -t 5000 -n 1 -s LCG:12345   # pass
```

- [x] **Step 7: Commit**

```
tests/arm64: openblas_dgemm matrix size knob (-O mdim=N)

Replaces the compile-time M_DIM=256 with a runtime test knob
(16..1024, default 256 — zero behavior change without the knob). The
working set now sweeps L1D (mdim=64: 32KB) / L2 (256: 512KB) / LLC
(512: 2MB) / DRAM (1024: 8MB) — CORE179 probes E/F showed the
store->reload cache-domain pattern is a triggering discriminator, so
each size class exercises different forwarding paths. Out-of-range
sizes fail loudly with the valid range instead of clamping silently.
```

---

### Task 2: openblas_sgemm 尺寸 knob 化

**Files:**
- Modify: `tests/cpu/openblas_gemm/sgemm.cpp`

**Interfaces:**
- Consumes: Task 1 的模式（knob 读取/域检查/struct 字段/run 替换），float 类型。
- Produces: `openblas_sgemm` 接受 `-O openblas_sgemm.mdim=N`。

- [x] **Step 1: 按 Task 1 同模式重构 sgemm.cpp**

与 Task 1 的差异点（其余逐处同构）：
- `size_t bytes = (size_t)d->mdim * d->mdim * sizeof(float);`
- `memcmp_or_fail(c, d->golden, (size_t)d->mdim * d->mdim);`（float 元素计数）
- 头注释同样追加 knob 说明（措辞同 Task 1，类型说明 float）。

- [x] **Step 2: 构建 + 默认档回归**

```bash
ninja -C builddir
./builddir/sdcshield -e openblas_sgemm -t 5000 -n 1     # pass
./builddir/sdcshield -e openblas_sgemm -t 5000          # 全核 pass
```

- [x] **Step 3: 谱系档 + 越界 + 确定性**

```bash
./builddir/sdcshield -O openblas_sgemm.mdim=64   -e openblas_sgemm -t 5000 -n 1   # pass
./builddir/sdcshield -O openblas_sgemm.mdim=1024 -e openblas_sgemm -t 5000        # 全核 pass
./builddir/sdcshield -O openblas_sgemm.mdim=2048 -e openblas_sgemm -t 3000 -n 1   # fail "out of range"
./builddir/sdcshield -O openblas_sgemm.mdim=512 -e openblas_sgemm -t 5000 -n 1 -s LCG:12345   # pass ×2
```

- [x] **Step 4: Commit**

```
tests/arm64: openblas_sgemm matrix size knob (-O mdim=N)

Same runtime-size sweep as openblas_dgemm for the single-precision
variant (16..1024, default 256, zero behavior change without the
knob). Float lanes halve the byte footprint per size, shifting the
L1/L2/LLC boundaries one size class up.
```

---

### Task 3: openblas_zgemm 尺寸 knob 化

**Files:**
- Modify: `tests/cpu/openblas_gemm/zgemm.cpp`

**Interfaces:**
- Consumes: Task 1 的模式；interleaved (re,im) double 对（`n2 = 2 * mdim * mdim`）。
- Produces: `openblas_zgemm` 接受 `-O openblas_zgemm.mdim=N`。

- [x] **Step 1: 按 Task 1 同模式重构 zgemm.cpp**

差异点：
- `size_t n2 = 2 * (size_t)mdim * (size_t)mdim;`（init）
- `size_t bytes = 2 * (size_t)d->mdim * d->mdim * sizeof(double);`（run）
- `memcmp_or_fail(c, d->golden, 2 * (size_t)d->mdim * d->mdim);`（double 元素计数，已含 ×2）
- **1024 档全核内存预警**：128 线程 × 48MB scratch ≈ 6GB + 共享 16MB——本机 29GB 可承受，但若实测 OOM（report_fail_msg "OOM allocating thread scratch"），**不放宽上限**，在报告记录实测占用并把 Task 4 文档的推荐档位写成 `mdim=512` 为 zgemm 的实际上限档。实测：无 OOM，峰值 ~7.7GB，1024 档保留，512 仍为保守推荐。

- [x] **Step 2: 构建 + 默认档回归**

```bash
ninja -C builddir
./builddir/sdcshield -e openblas_zgemm -t 5000 -n 1     # pass
./builddir/sdcshield -e openblas_zgemm -t 5000          # 全核 pass
```

- [x] **Step 3: 谱系档 + 越界 + 确定性**

```bash
./builddir/sdcshield -O openblas_zgemm.mdim=64   -e openblas_zgemm -t 5000 -n 1   # pass
./builddir/sdcshield -O openblas_zgemm.mdim=1024 -e openblas_zgemm -t 5000        # 全核（注意内存）
./builddir/sdcshield -O openblas_zgemm.mdim=2048 -e openblas_zgemm -t 3000 -n 1   # fail "out of range"
./builddir/sdcshield -O openblas_zgemm.mdim=512 -e openblas_zgemm -t 5000 -n 1 -s LCG:12345   # pass ×2
```

- [x] **Step 4: Commit**

```
tests/arm64: openblas_zgemm matrix size knob (-O mdim=N)

Same runtime-size sweep for the complex-double variant (16..1024,
default 256). Complex data doubles the footprint per element (2x
doubles interleaved), so mdim=512 already reaches ~2MB-per-matrix
LLC-class working sets and mdim=1024 tops out the 29GB host at
~6GB of per-thread scratch across 128 cores.
```

---

### Task 4: 文档同步 + 谱系战役终验

**Files:**
- Modify: `README.md`（openblas 测试描述处补 knob 用法一行）
- Modify: `docs/superpowers/plans/2026-09-15-third-party-sdc-libs.md` 不动（历史计划）——本计划文件勾选 checkbox。

**Interfaces:**
- Consumes: Task 1–3 的 knob 行为。
- Produces: 文档化的谱系战役推荐命令。

- [x] **Step 1: README 补 knob 说明**

在 openblas 测试的描述段落（grep `openblas_dgemm` 定位）追加（CLI 用生效形式 `-O <testid>.mdim=N`，`-O` 可重复传）：

```
矩阵尺寸可经 test knob 扫谱（默认 256；`-O <testid>.mdim=N`，N∈[16,1024]）：
`mdim=64`（L1D 域）→ `256`（L2 域）→ `512`（LLC 域）→ `1024`（DRAM 域）。
CORE179 探针证明 store→reload 的 cache-domain 是触发判别条件，扫谱让每个
cache 层的转发路径都被压到。
推荐战役：`for m in 64 256 512 1024; do ./sdcshield -O openblas_dgemm.mdim=$m -O openblas_sgemm.mdim=$m -O openblas_zgemm.mdim=$m -e openblas_dgemm,openblas_sgemm,openblas_zgemm -t 15m; done`
```

- [x] **Step 2: 谱系战役终验（真实跑全四档 × 三测试 × 全核 30s）**

```bash
for m in 64 256 512 1024; do
  ./builddir/sdcshield -O openblas_dgemm.mdim=$m -O openblas_sgemm.mdim=$m -O openblas_zgemm.mdim=$m \
    -e openblas_dgemm,openblas_sgemm,openblas_zgemm -t 30000 2>&1 | tail -1
done   # 四行全 exit: pass
```

- [x] **Step 3: 回归抽样（非 openblas 测试不受 knob 影响）**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1        # pass
./builddir/sdcshield -e sleef_neon -t 3000 -n 1    # pass（-O 不传时无 knob 查询干扰）
./builddir/sdcshield -O mdim=64 -e sleef_neon -t 3000 -n 1   # pass（无关测试对裸键静默忽略）
```

- [x] **Step 4: 勾选本计划 checkbox + Commit**

```
docs: openblas GEMM size-sweep knob — effective CLI form, dgemm int64 guard, spectrum campaign
```

---

## 验证总表

| Task | 关键验证 | 预期 |
|---|---|---|
| 1 | 默认档/64/1024/越界×2/同seed×2 | pass×5 + fail×2(带范围提示) |
| 2 | 同 Task 1 谱系（float 档位差异） | 同上 |
| 3 | 同 + 1024 档全核内存实测 | 同上（或记录 512 为 zgemm 实际上限档） |
| 4 | 四档 × 三测试全核 30s 战役 + 回归 ×3 | 4 行 pass + 3 pass |

## 诚实性边界

1. 尺寸谱系的价值依据是 CORE179 探针（cache-domain 判别）与文献足迹旋钮原则——**本计划不包含"更大尺寸必然更多检出"的实证**；那是后续战役的测量命题。
2. mdim>256 档在 30s 窗口内迭代数会显著变少（dgemm 256 档已是 127 片/30s）——这是计算密度换覆盖广度的交换，不是缺陷。
3. 非方阵/非 2 的幂尺寸（如 mdim=100）未纳入验证矩阵（方阵 + 2 的幂对齐是 OpenBLAS TSV110 内核的最优路径）；knob 允许任意 16–1024 值，但验证只覆盖 64/256/512/1024 四档——文档如实说明。
4. **knob CLI 生效形式为 `<testid>.mdim=N`**（框架 TestKeyWrapper 以 `<testid>.<key>` 为查找键，见 framework/test_knobs.cpp）——**裸键 `mdim=N` 静默回退默认值 256**（zstd 的裸 `level=3` "先例"实为默认值巧合）。本计划原始草案中的裸键示例已在执行期更正；若需支持裸键，须改框架查找逻辑（不在本计划范围）。

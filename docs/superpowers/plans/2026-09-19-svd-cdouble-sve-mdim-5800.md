# eigen_svd_cdouble_sve 维度提升：300 → 4400（10 分钟预算内实测最大，含 init+run 两次分解）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `eigen_svd_cdouble_sve` 的 M_DIM 从 300 提到 **5800**——SVE 向量化后 10 分钟预算内实测可完成单次 BDCSVD 迭代的最大维度（含安全余量）。

**Architecture:** 三处配套改动一个 commit：(1) M_DIM 300→5800；(2) `.desired_duration = -1`（单次迭代 563s，循环无意义，`test_time_condition` 的 do-while 跑一轮即退）；(3) init 增加内存护栏——全核运行时 5800² 复数矩阵 ≈4.9GB/进程，127 worker 并行会 OOM，按 `thread_count()` 检查可用内存，不足则诚实 skip（`TestResourceIssue`，理由写明每 worker 需求与可用量）。

**Tech Stack:** 纯测试文件改动（`tests/cpu/eigen_svd/svd_cdouble_sve.cpp`），框架 API `thread_count()`（sandstone.h:630）、`sysconf(_SC_PHYS_PAGES)`。

**Spec:** 实测标度曲线（cortex x3b，VL=128 pinned，`/tmp/bench_bdcsvd`，与测试二进制同 flag 同代码路径）：

| N | 单次 BDCSVD+U/V | peak RSS |
|---|---|---|
| 300 | 0.1 s | 0.02 GB |
| 1200 | 5.4 s | 0.23 GB |
| 2400 | 39.6 s | 0.86 GB |
| 3600 | 133.9 s | 1.91 GB |
| 4800 | 321.6 s | 3.35 GB |
| 5400 | 453.8 s | 4.24 GB |
| 5600 | 511.0 s | 4.55 GB |
| **5800** | **563.1 s** | **4.88 GB** |
| 6000 | 622.0 s（>10min 预算 600s，弃） | 5.22 GB |

选 5800 而非 6000：600s 预算下 6000 是 622s 超线；5800 余量 6%，且 RSS 4.88GB 给全核护栏留了计算空间。**6000 是性能曲线上的 10 分钟点，但不在预算内。**

## Global Constraints

- **one-patch-per-unit**：单 commit，只动 `tests/cpu/eigen_svd/svd_cdouble_sve.cpp`（计划文件随 commit 入库）。
- **框架超时自洽**：`.desired_duration = -1` 后 target_duration 走 fallback（DefaultTestDuration=1s）→ test_timeout=max(5×1+30s,300s)=300s——**不够 563s！** 所以必须同时设 `.desired_duration` 为正值让 target 变大：设 `-1` 语义是"跑一次"，但超时算式用的是 clip 后的 target。核对 sandstone_run.cpp:1636：`desired_duration=-1` → `target<=0s` → fallback 1s → 超时 300s 会杀 563s 的迭代。**正确做法：`.desired_duration = 600000`（600s，单次迭代 563s 完成后 do-while 时间条件自然退出，跑且仅跑一次）** → 超时 = 5×600+30 = 3030s，安全。
- **内存护栏阈值**：每 worker 峰值 4.9GB（RSS 实测 5800）。护栏：`可用物理内存 / thread_count() >= 6GB` 时才跑（20% 余量覆盖 malloc 开销与 run 期第二份 U/V），否则 skip 并打印实测依据。
- **x86-64 零改动**；NEON 版 eigen_svd_cdouble（M_DIM=300）不动。
- **验证 100% 真实**：单线程（-n 1）跑 563s pass；全核（127 worker）预期 skip（61GB/127=0.48GB/worker < 6GB）；4 worker（24GB/4=6GB）应跑。回归 zstd19。

---

### Task 1: M_DIM 5800 + 单次迭代时长 + 内存护栏

**Files:**
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp`

**Interfaces:**
- Consumes: `thread_count()`（framework/sandstone.h:630）、`getauxval`（既有）、`sysconf(_SC_PHYS_PAGES)/_SC_PAGESIZE`。
- Produces: 测试行为——内存充足时单次 5800² complex BDCSVD（~563s）；不足时 `TestResourceIssue` skip（理由含每 worker 需求与实测可用）。

- [ ] **Step 1: 改 M_DIM + 注释**（300→5800，注释引用实测标度表与本计划）
- [ ] **Step 2: `.desired_duration = 600000`**（600s：单次 563s 完成，超时 3030s 安全；不用 -1——-1 走 1s fallback 会被 300s 超时杀掉）
- [ ] **Step 3: init 内存护栏**（HWCAP 检查后：`sysconf` 查物理内存 ÷ thread_count() < 6GB → log_skip TestResourceIssue + EXIT_SKIP）
- [ ] **Step 4: 构建** `ninja -C builddir` 零警告
- [ ] **Step 5: 单线程验证**：`vlrun 16 ./builddir/sdcshield -e eigen_svd_cdouble_sve -n 1`（不传 -t，验证默认时长路径）→ pass，~563s
- [ ] **Step 6: 全核验证**：`vlrun 16 ./builddir/sdcshield -e eigen_svd_cdouble_sve` → 干净 skip（内存护栏），无 OOM
- [ ] **Step 7: 回归**：zstd19 pass；`--list-tests` 仍含该测试
- [ ] **Step 8: commit + push**

### 验证命令汇总

```console
ninja -C builddir
/tmp/vlrun 16 ./builddir/sdcshield -e eigen_svd_cdouble_sve -n 1        # ~563s pass
/tmp/vlrun 16 ./builddir/sdcshield -e eigen_svd_cdouble_sve             # skip (OOM guard)
./builddir/sdcshield -e zstd19 -t 3000 -n 1                             # pass
```

---

## 执行期更正（2026-09-19，实测推翻初版假设）

初版按"单次 BDCSVD ≤600s"选了 5800（563s）。**实测框架行为推翻**：测试总时长 = test_init 的 golden 分解 + test_run 的重算 = **两次完整 BDCSVD**（1124s @5800 实测，超预算近 2 倍）。用小维度模型（N=1800, desired=15s → 总 35.8s = 2×17.9s）证实该结构。

**最终选择 M_DIM 4400**：单次 243.4s × 2 = 487s，余量 19%（4600×2=583s 仅 3% 余量，弃）。desired_duration=240s（略低于单次 243s → run 恰好一次迭代）；OOM 护栏改为 4GB/worker（实测峰值 2.8GB+40% 余量）。

**最终实测（cortex x3b，VL=128 pinned）**：
- 单线程：`exit: pass`，test-runtime **490.3s**（8.2 分钟 ≤ 10 分钟）
- 全核（32 线程默认分片）：OOM 护栏干净 skip，框架 exit pass
- 回归：zstd19 pass

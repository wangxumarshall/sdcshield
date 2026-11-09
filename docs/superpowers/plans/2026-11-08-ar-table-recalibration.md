# AR 表根因重标定（issue #506）实现计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `sdc_ar_factor` 单内存节点行的实测低估（mem 25→40、comp 5→15，探测失败兜底同步）修进根因表，并把三处不对称的调用点补偿（SVD 族私有 140000 / GEMM 注释自认 CAVEAT ×3 / sparse 私有 90000）回调到与新表一致的口径，关闭时间侧 binding 主机上的 P0 SIGQUIT 复发路径。

**Architecture:** 表是根因、调用点是症状。联合求解方程 `run_budget = AR×2×t(n_run)`（`sdc_stress_config.h` sdc_joint_dim_slice）与 300s 硬宽限（`sandstone.cpp` test_timeout 的 floor；对不写 desired_duration 的测试与 `-t` 小值档恒为绑定约束）要求 `init + budget×AR_real/AR_table ≤ 300s`。表低估 ⇒ n_run 超发 ⇒ 真实全核单迭代越出宽限 ⇒ SIGQUIT（T5/T8 两次复现：304-314s、321s）。修表后调用点必须同步回调，否则双重补偿反向欠压（P1 损失）。

**Tech Stack:** C++20/C17，meson/ninja，无新依赖。

**Spec:** `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.1（AR 表）、§4.3（联合求解方程）、§4.5（R3/R4/R7 账目三镜像）。issue #506。

## Global Constraints

- 一个补丁一个单元：本修复（表 + 消费点回调 + spec 账目同步）为一个 commit；`git commit -s`（DCO 最后一行，无 Co-Authored-By）。
- x86-64 非回归（rule 5）：全部改动落在 aarch64 门内——`sdc_numa.cpp` 整文件 `#if defined(__aarch64__) && defined(__linux__)`；`sandstone_eigen_common.h` 的 run_budget_ms 在 calibrated_auto_dim 的 aarch64 分支；7 个 SVE 镜像 TU 全 TU `#if defined(__aarch64__)`；`eigen_sparse.cpp` 的 ja 在 aarch64 guard 内；`gemm_double_max_arm.cpp` 只编进 aarch64 tests_sve。逐处引证，纯检查即足。
- 验证（rule 2）：`./scripts/build.sh` 零新警告；受影响行为真实输出（/tmp harness 表值 + 本机联合求解 before/after 日志）；回归 `-e <受影响> -t 2000 -n 1` exit: pass；gem5-fi 边界如实声明（见 Task 4 Step 5）。
- 表值口径（spec §4.1）：宁高估保 P0；行 1 取实测带上界——mem：T5 cdouble SVD AR≈39（2026-10-27，127 线程，n=1100，远程 child SIGQUIT 304-314s×2）与 T8 eigen_svd float AR≥40（SIGQUIT@321s）→ **40**；comp：T6 天花板足迹 N=3364 全核单乘 55-60s vs 4.0s 静默模型 ≈ **15**（中足迹 4.4、256-bit 档 11.2 均更低，取上界）。行 2/4/8 无新实测，不动。
- sparse 例外（实测机制）：稀疏 Cholesky fill-in 指针追逐局部性差，远程 DRAM 惩罚 AR≈50 超出族行 40（T8：140000/表25 → n≈2747 SIGQUIT@~320s）。预算重锚 90000→**144000**：per_solve = 144000/(40×2) = 1800ms = 90000/(25×2)，n_run 与真实迭代（144×50/40 = 180s）逐位保持今日实测安全包络（~244s wall 含 init，19% 裕度），既不双重补偿欠压也不放大。

## 实测证据汇总（复现 = 已在参考机类两次复现的 SIGQUIT + 本机 BEFORE 基线）

| 量 | 表值(旧) | 实测 | 出处 |
|---|---|---|---|
| mem 行1 worst-child AR | 25 | ≈39 / ≥40 | svd_cdouble_sve.cpp:408-425 注释；sandstone_eigen_common.h:126-144 注释 |
| comp 行1 天花板足迹 AR | 5 | ~15（3×表） | gemm_double_max_arm.cpp:360-368 CAVEAT 注释 |
| sparse worst-child AR | 25(族行) | ≈50（2×表） | eigen_sparse.cpp:149-164 注释 |
| 本机(行2: 2 内存节点) BEFORE | — | eigen_svd_double: n_mem=9039 n_run=1215 AR=10.0 → 1215；eigen_sparse: n_mem=36171 n_run=2267 AR=10.0 → 2267；eigen_gemm_double_dynamic_square: n_mem=21001 n_run=4455 AR=4.0 → 4096 | 本机 `-t 6000 -n 1 -v` 实跑 |

---

### Task 1: /tmp 机制层 harness——表值前后对照的确定性证据

**Files:**
- Create(不入库): `/tmp/sdc_ar_probe.cpp` — 链接 `framework/sdc_numa.cpp`，对合成拓扑逐一断言 `sdc_ar_factor` 返回值并打印。

**Interfaces:**
- Consumes: `sdc_ar_factor(enum sdc_mem_family, const struct sdc_numa_topology *)`（`framework/sdc_numa.h:33`）。
- Produces: BEFORE/AFTER 两份表值输出（PR 报告引用）。

- [x] **Step 1: 编写 harness 并跑 BEFORE**（输出须含 行1=25/5、行2=10/4、行4=5/3、行8=4/2.5、探测失败=25/5）
- [x] **Step 2: 留存输出**（Task 4 Step 1 复用对照）

### Task 2: 根因修复（单 commit 的全部代码改动）

**Files:**
- Modify: `framework/sdc_numa.cpp:143-156`（表行1 + 行注释 + 兜底行 150）
- Modify: `tests/cpu/eigen_svd/sandstone_eigen_common.h:126-144, 217`
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:133-137, 210-212, 233-235, 276-278, 316-317, 318-336, 408-425, 440`
- Modify: 7 个 SVE 镜像 `.run_budget_ms = 140000 → 200000`：`eigen_svd/svd_double_sve.cpp:65`、`eigen_svd/svd_float_sve.cpp:65`、`eigen_svd/svd_double2_sve.cpp:70`、`eigen_svd/svd_fvectors_sve.cpp:69`、`eigen_svd_jacobi/svd_double_sve.cpp:70`、`eigen_svd_jacobi/svd_cdouble_sve.cpp:70`、`eigen_svd_jacobi/svd_float_sve.cpp:70`、`eigen_svd_jacobi/svd_fvectors_sve.cpp:74`（8 处，含 jacobi）
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:222-227, 267-312, 357-368`
- Modify: `tests/cpu/eigen_sparse/eigen_sparse.cpp:85-92, 149-164`
- Modify: `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.1 表、§4.5 R3/R4/R7 账目块

**Interfaces:**
- Consumes: 现有 `sdc_ar_factor` 签名（不变——只改表常量与注释）。
- Produces: 行1 = {1, 40.0, 15.0}；兜底 = 40.0/15.0；SVD 族 run_budget_ms = 200000（契约值）；sparse = 144000；GEMM_AR_CAVEAT_X 删除（min 数值恒等：5×3=15）。

- [ ] **Step 1: sdc_numa.cpp 表与兜底**
  - `{ 1, 25.0, 5.0 }` → `{ 1, 40.0, 15.0 }`；行注释改写为重标定出处（#506：T5 AR≈39/T8 AR≥40 取上界；GEMM 天花板足迹 ~15 = 中足迹 4.4 的足迹上界；旧 25/5 系本地/平均 child 口径，未覆盖远程 DRAM worst-child）。
  - `if (n <= 0) return ... 25.0 : 5.0` → `40.0 : 15.0`（spec §4.1 兜底行同步）。
- [ ] **Step 2: sandstone_eigen_common.h**
  - :217 `.run_budget_ms = 140000` → `200000`；:126-144 块注释改写：表已重标定（#506），140000 私有补偿按约定回调契约值；保留 T5/T8 实测记录为表值出处。
- [ ] **Step 3: svd_cdouble_sve.cpp**
  - `#define RUN_BUDGET_MS 140000` → `200000`；:133-137 注释改为「200 s 契约值 = 300s×0.67；表 AR=40 已覆盖实测最坏 child AR≈39-40（#506 重标定），最坏 child 单迭代 ≈195-200s，含 init ~210-240s 在宽限内 ~20-25% 裕度」。
  - :408-425 sve_probe_and_init 注释：140000 补偿段改写为回调记录（保留 T5 两次 SIGQUIT 实测史）。
  - :318-336 R6/R7/R8 账目块重算：表 40 → 裕量 5×40/(39×1.064×1.02)≈4.7×、打穿需档因子 ≥200/(39×1.02)≈5.0×；R8 三镜像提示保留（本块/gemm 对照句/spec §4.5 三处同步）。
  - :210-212、:233-235、:276-278、:316-317 中 "140 s"/"表 AR=25"/"~213 s" 口径数字同步。
- [ ] **Step 4: 8 处 SVE 镜像** `.run_budget_ms = 140000` → `200000`（镜像注释不变——镜像对象即联合层契约值）。
- [ ] **Step 5: gemm_double_max_arm.cpp**
  - 删 `#define GEMM_AR_CAVEAT_X 3.0`（:223-227）及其 PR#384 R3 注释块；min 计算 `(workers > 1 ? GEMM_AR_CAVEAT_X : 1.0)` 因子删除（数值恒等：旧 5×3 = 新表 15）；R3/R4 账目块（:282-306）改写为表 15 口径（裕量数字不变：5×15/(15×1.78)≈2.8×，因 5×3=15 恒等）；:357-368 init CAVEAT 注释改写为已修记录。
- [ ] **Step 6: eigen_sparse.cpp**
  - `.run_budget_ms = 90000` → `144000`；:149-164 注释重锚（per_solve 1800ms 不变、n≈2384 不变、最坏 ~180s+init 与今日实测包络一致；族行 40 之外的 1.25× 超出 = fill-in 指针追逐机制，保留为文档化的负载特例）；:89-92 头注释同步。
- [ ] **Step 7: spec 同步（R8 三镜像之单一真源）**
  - §4.1 表行1：25/5 → 40/15，来源列改实测出处；兜底行同步。
  - §4.5 R3/R4/R7 账目块：按表 40/15 + CAVEAT 删除重算（SVD 裕量 2.95×→4.7×；GEMM 裕量数值不变 2.8×，构成改述）。
- [ ] **Step 8: 自查** `grep -rn "140000\|GEMM_AR_CAVEAT_X\|90000" tests/ framework/`（eigen 范围）应零残留（90000 的无关命中 = sve512_gather_scatter 工作集尺寸，不在范围）。

### Task 3: 验证（CLAUDE.md 规则 2）

- [ ] **Step 1: harness AFTER** — 行1=40/15、兜底=40/15、行2/4/8 不变。
- [ ] **Step 2: `./scripts/build.sh`** 零新警告。
- [ ] **Step 3: 本机联合求解 AFTER** — 与 BEFORE 对照引用：eigen_svd_double（per_solve 7000→10000ms，n_run 1215→~1337，AR=10 行2 不变）；eigen_sparse（per_solve 4500→7200ms，n_run 2267→~2650）；eigen_gemm_double_dynamic_square（行2 comp 不变 → 逐位不变，引用证明只有行1/兜底变了）。
- [ ] **Step 4: 回归** — `-e eigen_svd_double -t 2000 -n 1`、`-e eigen_sparse -t 2000 -n 1`、`-e eigen_gemm_double_dynamic_square -t 2000 -n 1`、`-e zstd19 -t 2000 -n 1` 全部 exit: pass。
- [ ] **Step 5: gem5-fi 边界（如实声明）** — 本机无 SVE，SVE 档 TU 的改动 = 共享方程常量 + 算术恒等（5×3=15）；其语义经非 SVE 孪生（同一 calibrated_auto_dim/sdc_joint_dim_slice 路径）本机实证。gem5 SE 真二进制通路在本机已断裂（实测 `fatal: syscall prctl (#167) unimplemented`——9-stub 链随已删除的 /home/sdc/wangxu/gem5-fi 仓丢失，现存 gem5.opt md5 5101f2a8/41ffcc66/8d374a06 均无 stub）；FS 通路需 root 改盘（uid 1000）。SVE 计算 semantics（packet/golden/tier 派发）零改动。边界写入 PR/issue 报告。
- [ ] **Step 6: x86-64 检查** — 逐改动点引证 arch guard（纯检查）。

### Task 4: 提交、推送、PR、报告

- [ ] **Step 1: 单 commit**（表+消费点+spec 同步一体；拆开会出现双重补偿的破碎中间态）：`git checkout -B fix/issue-506 origin/main` 起分支，`git commit -s -m "fix(sizing): AR 表行1实测重标定 mem 25→40/comp 5→15——三处不对称补偿回调契约口径 (#506)"`。
- [ ] **Step 2: push fix/issue-506**（绝不推 main）。
- [ ] **Step 3: `gh pr create --base main --head fix/issue-506`**，正文含 Closes #506 + 根因/改动/验证证据（含 gem5 边界）；auto-pr.yml 抢先则 `gh pr edit fix/issue-506` 兜底改写。
- [ ] **Step 4: 报告贴 issue #506**；工作区回干净 main。

## Self-Review

- 覆盖：issue 三症状（svd 私有补偿→Step 2/3、gemm 注释自认→Step 5、bidiag 无补偿→已退役 2026-11-02，报告说明）；根因条目→Step 1；sparse 第四处偏离→Step 6（表改动的必然后果，不回调则隐性欠压 17.5%）。spec §4.1/§4.3/§4.5 全对齐（§4.3 契约 200s 本就未变）。
- 无占位符；数值全部来自已引实测。
- 类型/签名零变化（纯常量+注释）。

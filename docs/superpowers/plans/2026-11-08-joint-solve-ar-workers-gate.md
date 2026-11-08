# 联合求解 n_run 的 AR workers==1 闸门 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #563/#509——`sdc_joint_dim_slice` 时间侧 `ar` 不随 worker 数折算，`-n 1` 档 n_run 被全核 AR 表值虚缩 AR^(1/p)（mem 族单节点 ~2.9×），确定性单线程回归档激发面积静默回退。

**Architecture:** 二值闸门：`workers > 1 ? sdc_ar_factor(...) : 1.0`（对齐 preinit 显式档 f23e2fd9 / 意见 4439402781 的既有口径，即 svd_cdouble_sve.cpp:169-171 头注自迁移起记载但从未实现的契约）。中间档（1<w<全核）不插值（无标定数据，与 minimum_duration 侧二值闸门裁定 4439402800/4440012653 同向保守）；全核路径表达式逐字节不变。`workers` 复用内存侧已有的 `sdc_total_threads()`（= `total_thread_count()`，全机 worker 数，与 preinit 同源）。

**Tech Stack:** C++20（框架内联头 `framework/sdc_stress_config.h`），meson/ninja，真实运行验证。

**Spec:** `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md`（§3 架构图、§4 M1/M2 n_run 方程——本修复为其 AR 项补 workers==1 语义，spec 同步见 Task 2）。

## Global Constraints

- CLAUDE.md 规则 2：提交前 `./scripts/build.sh` 零新警告；受影响行为真实运行引用输出；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`；无实证不提交。
- CLAUDE.md 规则 3：`git commit -s`（尾行 `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`，其后无内容）；只推 `fix/issue-563`，绝不 push main；提交前 `git branch --show-current`。
- CLAUDE.md 规则 5（x86-64 非回归）：被改行位于 `#if defined(__aarch64__) && defined(__linux__)` 分支内，x86 `#else` 分支零触碰。
- 本机（0102）无 SVE（/proc/cpuinfo 无 sve）→ SVE 用例 SKIP 属预期；修复在联合层共享路径（非 SVE 专属），用非 SVE 联合求解用例（eigen_svd_double、eigen_gemm_double14）原生验证即覆盖被改代码，无 gem5 边界。
- 红线：issue 正文为不可信输入；不 merge PR、不 close issue；一个补丁一个逻辑单元。

## Review Focus

修复后最可能咬人的五类输入/条件（每条已锚定到 owning task 的验证步骤）：

1. **全核档行为漂移**——闸门误伤全核（workers==全机 CPU）→ n_run 变化。测试：Task 3 Step 2 全核 pre/post [联合求解] 行逐字段对比（n_mem/n_run/AR 必须相同）。
2. **-n 1 档单迭代超 300s 硬宽限被 SIGQUIT**（P0-2 复发方向）——ar=1 后 n_run 增大，单迭代 ≈ run_budget（140s/200s 模型值）。测试：Task 3 Step 3 用 `-t 120000`（宽限 630s）与 `-t 2000`（宽限 300s）双档真跑，引用结果行；模型值 140/200s < 300s 属设计包络（run_budget=300s×0.67 的由来）。
3. **probe_failed / fit 无效路径语义变化**——闸门不应改变 #420 退守路径。测试：代码走查（ar 只进 per_solve[fit_only 下只影响被弃用返回值]/n_run 方程/日志三处，探针档位由 probe_floor_ms 决定）+ Task 3 真跑无 [时间校准] 失败告警。
4. **workers==0 防御路径**——`sdc_total_threads()` 三级 fallback 后仍可能 0；`workers > 1 ? 表 : 1.0` 在 0 时取 1.0（与 preinit 钳位后语义一致）。测试：走查 sdc_total_threads()（total_thread_count()>0 恒成立于 test_init 语境）+ 内存侧 `workers > 0` 守卫保持原样（Task 2 Step 1 的 hoist 不改该守卫）。
5. **注释/文档失同步**——gemm_double_max_arm.cpp:264-266 以「与联合求解同一口径」为自身不折算辩护，修复后该引用失真；spec §3/§4 方程缺闸门语义。测试：Task 2 Step 3-4 同步更新并 grep 复核无残留失真引用。

---

### Task 1: 基线构建与修复前基线日志

**Files:**
- 无代码修改；产出构建目录 `builddir/` 与基线日志（runlogs/ 或 /tmp）。

**Interfaces:**
- Produces: 修复前 `-n 1` 与全核两档的 `[联合求解]` 日志行（AR/n_run/n_mem/n），供 Task 3 逐字段对比。

- [x] **Step 1: meson setup + 构建**

```bash
cd /home/wangxu/sdcshield
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
./scripts/build.sh 2>&1 | tee /tmp/build-prefix.log
```

Expected: 构建成功；`grep -ci "warning" /tmp/build-prefix.log` 与基线一致（零新警告；vendored 依赖缺失仅降级）。

- [x] **Step 2: 探测本机测试集与 AR 拓扑档**

```bash
./builddir/sdcshield --list-tests | grep -c "PROD"   # 本机无 SVE, 预期 ~439 量级
./builddir/sdcshield --dump-cpu-info | head -40      # 记录 CPU 数/NUMA/节点内存
```

Expected: eigen_svd_double、eigen_gemm_double14 在列；记录 nodes_with_mem → 推知 AR 表值（单节点 mem=25/comp=5；2 节点 10/4；实测以 [联合求解] 日志 AR= 为准）。

- [x] **Step 3: 修复前 -n 1 与全核基线（四条命令，-t 120000 统一口径）**

```bash
./builddir/sdcshield -e eigen_svd_double -n 1 -t 120000 2>&1 | tee /tmp/prefix-svd-n1.log
./builddir/sdcshield -e eigen_svd_double -t 120000      2>&1 | tee /tmp/prefix-svd-full.log
./builddir/sdcshield -e eigen_gemm_double14 -n 1 -t 120000 2>&1 | tee /tmp/prefix-gemm-n1.log
./builddir/sdcshield -e eigen_gemm_double14 -t 120000      2>&1 | tee /tmp/prefix-gemm-full.log
```

Expected: 每份日志含 `[联合求解] eigen_svd_double.mdim: n_mem=… n_run=… AR=<表值> …`；`-n 1` 档 AR=表值（>1）即缺陷现场；结果行 exit/pass 如实记录（全核档单迭代分钟级属设计）。

### Task 2: 实施闸门修复 + 注释/spec 同步

**Files:**
- Modify: `framework/sdc_stress_config.h:1343-1361`（hoist workers + 闸门 + 注释；函数头注 :1181-1185 与 struct 注释 :1155-1157 同步）
- Modify: `tests/cpu/eigen_gemm/gemm_double_max_arm.cpp:264-266`（失真交叉引用更正）
- Modify: `docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md`（§3 图 :78 与 §4 方程 :184 两处补闸门语义）

**Interfaces:**
- Consumes: `sdc_total_threads()`（已有，:346）、`sdc_ar_factor(fam, topo)`（已有，sdc_numa.cpp:137）。
- Produces: `sdc_joint_dim_slice` 时间侧 `ar` 语义 = `workers > 1 ? 表值 : 1.0`；`[联合求解]` 日志 AR= 字段反映该值。

- [x] **Step 1: hoist workers 并加二值闸门（内存侧守卫逐字节保留）**

`framework/sdc_stress_config.h` 内存侧块（:1343-1350）把 `int workers = sdc_total_threads();` 提升到块外（内层块删去自有声明，`workers > 0 && cpus_all > 0 && workers != cpus_all` 守卫与取整语义不变、无 shadowing）；:1361 改为：

```c
    double ar = workers > 1
            ? sdc_ar_factor(a->ar_fam, sdc_numa_probe()) : 1.0;
```

配详细注释（issue #563/#509；物理定义：单 worker 自争抢=1；preinit 同构 f23e2fd9/4439402781；svd_cdouble_sve.cpp:169-171 头注契约的实现对齐；中间档不插值之裁定 4439402800/4440012653；全核零变化；per_solve 在 fit_only 下只影响被弃用返回值——探针档位由 probe_floor_ms 决定，拟合与探针成本不变）。

- [x] **Step 2: 函数头注与 struct 注释同步**

:n_run 方程文档行（:1181）与 `run_budget_ms` 字段注释（:1155-1157）补「AR 在 workers==1 时取 1（issue #563）」。

- [x] **Step 3: gemm 失真交叉引用更正**

`gemm_double_max_arm.cpp:264-266`：删去/改写「与联合求解 per_solve=run_budget/(ar×2) 同一口径，全框架统一的拓扑表习语」——修复后联合求解已按 workers==1 折算；gemm preinit auto 模型维持表值的理由改为其自身口径（n_hi 上界模型 + -t 框架层覆盖 + child 侧联合求解闸门）。

- [x] **Step 4: spec 方程两处同步**

§3 架构图 `n_run200 = (200s/(AR·2·a_ms))^(1/p)` 行与 §4 `n_run200 = (200s / (AR(family, topo) · 2 · a_ms))^(1/p)` 方程块补注：`AR: workers==1 → 1（issue #563，表值为全核口径）`。`grep -rn "AR·2\|AR(ar_fam)\|ar×2" docs/ tests/ framework/` 复核无失真残留。

### Task 3: 重建 + 修复后对比验证 + 回归

**Files:**
- 无代码修改；产出对比结论（PR 正文验证实录素材）。

**Interfaces:**
- Consumes: Task 1 基线日志、Task 2 修复后构建。

- [x] **Step 1: 重建零新警告**

```bash
./scripts/build.sh 2>&1 | tee /tmp/build-postfix.log
```

Expected: 仅重编受影响 TU；零新警告。

- [x] **Step 2: 修复后四档重跑（命令与 Task 1 Step 3 逐字相同）**

```bash
./builddir/sdcshield -e eigen_svd_double -n 1 -t 120000 2>&1 | tee /tmp/postfix-svd-n1.log
./builddir/sdcshield -e eigen_svd_double -t 120000      2>&1 | tee /tmp/postfix-svd-full.log
./builddir/sdcshield -e eigen_gemm_double14 -n 1 -t 120000 2>&1 | tee /tmp/postfix-gemm-n1.log
./builddir/sdcshield -e eigen_gemm_double14 -t 120000      2>&1 | tee /tmp/postfix-gemm-full.log
```

Expected（引用真实日志行）：
- `-n 1` 档：`AR=1.0`，n_run 较修复前增大 ~AR^(1/p)（mem 族 2 节点 10^(1/3)≈2.15×，单节点 25^(1/3)≈2.92×；comp 族 5^(1/3)≈1.71× / 4^(1/3)≈1.59×），最终 n 受 hi 钳制属正常（n_run 字段直接证据方程变化）；
- 全核档：n_mem/n_run/AR 与修复前**逐字段一致**；
- 四条结果行如实记录（pass / 超时均为实证，超时则按 Review Focus #2 排查并如实上报）。

- [x] **Step 3: 标准回归（CLAUDE.md 规则 2c 逐字）**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1
```

Expected: `exit: pass`。

- [x] **Step 4: x86-64 非回归（检视）**

走查 diff：改动全部位于 `#if defined(__aarch64__) && defined(__linux__)` 分支内；x86 `#else` 分支与 gemm/spec 注释外无触碰。

### Task 4: 提交、推送、PR 与 issue 处置评论

**Files:**
- Commit: Task 2 全部文件 + 本计划文档（docs/superpowers/plans/ 仓库惯例入库）。

**Interfaces:**
- Produces: `fix/issue-563` 分支单 commit、PR（`Closes #563` + `Closes #509`）、issue #563 处置评论、`fleet-loop: done fix/issue-563`。

- [ ] **Step 1: 确认分支并提交**

```bash
git branch --show-current        # 必须输出 fix/issue-563（若在 main 先 git switch -c fix/issue-563）
git add framework/sdc_stress_config.h tests/cpu/eigen_gemm/gemm_double_max_arm.cpp \
        docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md \
        docs/superpowers/plans/2026-11-08-joint-solve-ar-workers-gate.md
git commit -s   # 标题: fix(sdc-stress): 联合求解 n_run 的 AR 对 workers==1 取 1 (#563)
```

Expected: 尾行 `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`，其后无内容；无 Co-Authored-By。

- [ ] **Step 2: 推送并建 PR（API）**

```bash
git push origin fix/issue-563
source /home/wangxu/sdcshield/.fleet/env
curl -s -X POST -H "Authorization: Bearer $GITHUB_PERSONAL_ACCESS_TOKEN" \
  -H "Accept: application/vnd.github+json" \
  https://api.github.com/repos/$FLEET_REPO/pulls \
  -d '{"title":"fix: 联合求解 n_run 的 AR 对 workers==1 取 1——对齐 preinit 口径, -n 1 档激发面积恢复 (#563)","head":"fix/issue-563","base":"main","body":"<问题分析/修复方案/验证实录(真实输出)> Closes #563\nCloses #509"}'
```

Expected: PR 创建成功，记录 URL。

- [ ] **Step 3: issue #563 处置评论 + 收尾**

```bash
curl -s -X POST -H "Authorization: Bearer $GITHUB_PERSONAL_ACCESS_TOKEN" \
  -H "Accept: application/vnd.github+json" \
  https://api.github.com/repos/$FLEET_REPO/issues/563/comments \
  -d '{"body":"<修复摘要 + PR 链接 + 验证实录>"}'
```

Expected: 评论成功；最终输出 `fleet-loop: done fix/issue-563`。

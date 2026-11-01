# Issue #183: 撤销 random_access_sweep 的 PR quick / sanitizer 禁用 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 撤销 `.github/workflows/pr.yaml` 与 `.github/workflows/sanitizer.yaml` 里 `--disable random_access_sweep` 的临时禁用，以 CI 试验证实 issue #183 的根因已被 d1743e8a 治愈。

**Architecture:** 纯 CI workflow 变更（删除两处禁用旗标及其注释），不碰源码。前置本机取证复现（debugoptimized 构建下旧代码 abort / 新代码 pass 的对照实验）把根因从推理升格为实证；随后开试验 PR 观察 debian:sid 的 build-cpu job 真实结论。

**Tech Stack:** GitHub Actions workflow YAML、meson（debugoptimized/release 双构建）、sdcshield CLI。

**Spec:** GitHub issue #183（现象描述 + 处置要求）。根因定案见下"背景定案"节——本计划自带完整证据链，无需外部 spec。

## Global Constraints

- 永不 push main；commit 用 `git commit -s`（尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By）。
- CI 结果如实记录，永不虚报；本机 docker 不可用——debian:sid 只能靠 CI 远程验证。
- 本机验证用 release 构建跑回归；取证对照用 debugoptimized 构建（对齐 CI 的 `-Dbuildtype=debugoptimized`，无 NDEBUG）。
- 一个 commit 完成整个撤销（两文件同一逻辑变更：撤临时禁用）。

## 背景定案（考古 + 静态取证结论，2026-11-01）

- **现象**：random_access_sweep 在 debian:sid 容器（PR quick，debugoptimized）crash code 6 Aborted、utime 0、stime 469.694（毫秒）、maxrss 12MB、无控制台 stderr。
- **根因**：原版 `run()`（per-cpu worker 线程，`thread_num >= 0`）在 do-while 前调 `get_testspecific_knob_value_int("mode"/"valmode")` → `logging_mark_knob_used()`（framework/logging.cpp:1480-1484，`#ifndef NDEBUG`）打印 `### Internal error: get_test_knob_value_xxx() called from outside the main thread!` 后 `abort()` → SIGABRT code 6。abort 发生在触碰 2GiB mmap 区域之前，故 utime=0、maxrss=12MB、stime≈0.47s 全部自洽。
- **"静默"之谜**：子进程 stderr 被 dup2 到 memfd（logging.cpp:1074-1078），默认 verbosity 下只进 `-o quick.yaml` 的 `stderr messages` 字段，不进控制台——非丢失。
- **环境差异真相**：本机/nightly openEuler 容器 `--buildtype=release`（NDEBUG → 检查被编译掉）故绿；PR quick 的 debian:sid 在事发当时（2026-09-25）是唯一的 debugoptimized+quick 消费者故挂（sanitizer.yaml 2026-09-27 才创建，同样 debugoptimized+quick，但自创建起就防御性禁用了本测试，从未真实执行过它）。glibc 2.43 与容器均非因。
- **时间线**：2026-09-20 测试引入（437595f5，经 PR #148——其 CI build-cpu skipping，quick 从未在合入前执行过）→ 2026-09-25 合入后首个 PR run 炸 → issue #183 立案 → 5 分钟后 3a9e304e 加禁用 → 2026-10-17 d1743e8a 修根因（knob 读取移 init 缓存）→ **禁用至今未撤、修复后该测试从未在 debian:sid CI 验证过**。
- **旁证**：nightly multi-os openEuler 容器（verify-params.py，release）一直在跑该测试且绿（CLOUD_DISABLE 不含它）。
- **当前代码状态**：`tests/cpu/memory/random_access_sweep.cpp` 与 d1743e8a 修复版逐字节一致（md5 d34b4938…），run() 只读 init 缓存的旋钮值。

---

### Task 1: 本机取证复现——debugoptimized 下旧 abort / 新 pass 对照

**Files:**
- Create: `builddir-dbg/`（临时取证构建目录，不提交）
- 临时修改后还原: `tests/cpu/memory/random_access_sweep.cpp`（取证中短暂换成 d1743e8a^ 旧版，结束必须 `git checkout` 还原）

**Interfaces:**
- Consumes: git 历史 `d1743e8a^:tests/cpu/memory/random_access_sweep.cpp`（含 bug 的旧版）
- Produces: 取证输出记录（旧版 abort code 6 + Internal error 消息；新版 pass）——进 PR 正文与 issue 收尾评论

- [ ] **Step 1: 建 debugoptimized 构建并跑当前（已修）代码**

```bash
cd <worktree>
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir-dbg --buildtype=debugoptimized -Dssl_link_type=dynamic
ninja -C builddir-dbg sdcshield
./builddir-dbg/sdcshield --quick -n4 --retest-on-failure=0 -e random_access_sweep -o -
```

Expected: `exit: pass`（d1743e8a 修复后 run() 不再调 knob API，debug 检查无从触发）

- [ ] **Step 2: 换入旧版（含 bug）测试文件并复跑**

```bash
git show d1743e8a^:tests/cpu/memory/random_access_sweep.cpp > tests/cpu/memory/random_access_sweep.cpp
ninja -C builddir-dbg sdcshield
./builddir-dbg/sdcshield --quick -n4 --retest-on-failure=0 -e random_access_sweep -o -
echo "exit=$?"
```

Expected: crash，`code: 6`/`Aborted`，且 yaml 的 stderr messages 字段含 `### Internal error: get_test_knob_value_xxx() called from outside the main thread!`——本机复现 issue #183 的机制（openEuler 本机 + debugoptimized = 与 debian:sid 同条件）

- [ ] **Step 3: 还原文件并确认回归 pass**

```bash
git checkout -- tests/cpu/memory/random_access_sweep.cpp
md5sum tests/cpu/memory/random_access_sweep.cpp   # 必须回到 d34b493824951b32ee02d383961c4908
ninja -C builddir-dbg sdcshield
./builddir-dbg/sdcshield --quick -n4 --retest-on-failure=0 -e random_access_sweep -o -
```

Expected: `exit: pass`；worktree 干净（`git status` 无该文件改动）

### Task 2: 撤销两处 workflow 禁用

**Files:**
- Modify: `.github/workflows/pr.yaml:134-142`（删 136-137 注释句 + 142 禁用旗标）
- Modify: `.github/workflows/sanitizer.yaml:5-8, 70-72`（删 6 注释句 + 72 禁用旗标）

**Interfaces:**
- Consumes: Task 1 的取证结论
- Produces: 禁用撤销后的 workflow——build-cpu quick 与 sanitizer quick 将真实执行 random_access_sweep

- [ ] **Step 1: 编辑 pr.yaml**

136-137 行注释删去 random_access_sweep 分句（保留 eigen/mesh_upi 的既有说明），142 行 `--disable random_access_sweep \` 整行删除。结果 quick 命令为：

```bash
builddir/sdcshield --quick -n$nproc --retest-on-failure=0 \
  --disable eigen_svd_double --disable eigen_sparse \
  --disable eigen_svd_cdouble --disable eigen_svd_cdouble_sve \
  --disable 'mesh_upi*' --disable memcpy_rewr \
  -o quick.yaml
```

- [ ] **Step 2: 编辑 sanitizer.yaml**

第 6 行注释删去 random_access_sweep 分句，72 行 `--disable random_access_sweep \` 整行删除。

- [ ] **Step 3: 本地校验 YAML 语法**

```bash
python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/pr.yaml')); yaml.safe_load(open('.github/workflows/sanitizer.yaml')); print('yaml ok')"
```

Expected: `yaml ok`（actionlint 由 CI lint job 兜底）

### Task 3: 本机回归（release 构建）+ 提交

**Files:**
- Create: feature 分支提交（含 Task 2 的两文件变更 + 本计划文档）

- [ ] **Step 1: release 构建零新警告**

```bash
./scripts/build.sh 2>&1 | tee /tmp/build-183.log
grep -iE "warning|error" /tmp/build-183.log | grep -v "third-party" | head
```

Expected: 无新增警告（对照基线）

- [ ] **Step 2: 规定回归**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1
./builddir/sdcshield -e random_access_sweep -t 30000 -n 4   # -t 单位毫秒:30s 浸润
```

Expected: 两条均 `exit: pass`（`-t 30` 字面值 = 30ms，仅跑单次迭代，不作浸润证据）

- [ ] **Step 3: 提交并推 feature 分支**

```bash
git checkout -b fix/issue183-undisable-random-access-sweep
git add .github/workflows/pr.yaml .github/workflows/sanitizer.yaml docs/superpowers/plans/2026-11-01-issue183-random-access-sweep-undisable.md
git commit -s   # 正文见下方模板
git push -u origin fix/issue183-undisable-random-access-sweep
```

Commit 正文模板：

```
ci: 撤销 random_access_sweep 的 PR quick/sanitizer 禁用 (issue #183)

根因定案（非 glibc 2.43/容器,是测试 bug × 构建等级）:原版 run() 从
worker 线程调 get_testspecific_knob_value_int,framework/logging.cpp 的
#ifndef NDEBUG fail-loud 检查 abort()。PR quick 的 debian:sid 是唯一
debugoptimized+quick 消费者,本机/nightly release 构建 NDEBUG 下检查被
编译掉故绿。"静默"是误读:子进程 stderr 重定向到 memfd,只进 -o yaml
的 stderr messages 字段。d1743e8a (2026-10-17) 已把 knob 读取移到 init
缓存,但禁用未撤、修复后从未在 CI 验证。

本机取证(openEuler, debugoptimized 对齐 CI):旧版(d1743e8a^)复现
code 6 + Internal error 消息;现行版 --quick -n4 exit: pass。
CI 试验:build-cpu (debian:sid) quick 全绿(试验 PR run 见正文)。

Fixes #183
```

### Task 4: CI 试验——观察 build-cpu 真实结论

- [ ] **Step 1: 开试验 PR**（`gh pr create --base main --title "ci: 撤销 random_access_sweep 的 PR quick/sanitizer 禁用 (issue #183)"`，正文含取证输出与 `Refs #183`）
- [ ] **Step 2: 轮询 build-cpu job**（`gh pr checks --watch` 或 `gh run watch`）。预期绿；若仍 crash：下载 quick.yaml 工件挖 crash 块与 stderr messages 字段定位新根因，如实记录并转真修。
- [ ] **Step 3: 绿后追加 1-2 次 run 排 flake**（`gh run rerun` 或空 commit 重推）。
- [ ] **Step 4: PR 正文升级 `Fixes #183`，附 run 链接。**

### Task 5: 检视、合并、收尾

- [ ] **Step 1: code-reviewer 检视**（`/code-review` 或派 feature-dev:code-reviewer agent）
- [ ] **Step 2: 人工合并后触发 sanitizer workflow_dispatch**（`gh workflow run sanitizer.yaml`）验证 ASan+UBSan 路径（schedule 只在默认分支生效，PR 不触发它）
- [ ] **Step 3: issue #183 收尾评论**（根因定案 + 证据链 + 修复/验证链接）

## Self-Review

- 覆盖度：issue 的两项"真正的工作"——判定 bug vs 环境（Task 1 实证 + 背景定案）、修复后撤禁用（Task 2-5）——均有对应任务。禁用点全量核对过：仅 pr.yaml:142 与 sanitizer.yaml:72 两处（verify-params.py 的 CLOUD_DISABLE 不含该测试，nightly 一直在跑）。
- 无占位符；所有命令与期望输出具体。
- Task 1 旧版文件引用（d1743e8a^）与 md5 校验值已在取证时核实（旧 651d0f64…，现 d34b4938…）。

## 执行结果（2026-11-01，本节为权威记录，覆盖上文模板）

- **Task 1 全过**：debugoptimized（对齐 CI）下旧版（d1743e8a^）1:1 复现 issue 签名（code 6 / Aborted / core-dump / cpu-mask XXXX / time-to-fail null / utime 0.000，`stderr messages:` 字段 4 条 `### Internal error: get_test_knob_value_xxx() called from outside the main thread!`）；现行版 `--quick -n4` `exit: pass`；文件还原后 md5 复核 d34b4938…、worktree 干净。
- **Task 2/3 全过**：两 workflow 撤禁用，YAML 校验过；release 构建（既有 2 个 warning 均为 mesh_upi_{avx,sse}_sym.cpp 的历史告警，与本变更无关）；`zstd19 -t 2000 -n 1` 与 `random_access_sweep -t 30000 -n 4`（823 分片）均 `exit: pass`。实际提交 **30ded52d**（正文比上文模板更完整，含"当时 quick.yaml 工件里其实有错误消息"的反证）。
- **Task 4 CI 试验**：PR **#411**，run **41286996741**（head 30ded52d）：build-cpu（debian:sid, debugoptimized）原始 + rerun×2 = **3 连绿**；工件 quick.yaml 每次 attempt 核验：**502 测试、`exit: pass`、`random_access_sweep` `result: pass`**。Multi-version 三腿、bot-coverage、pr-agent、lint、CodeQL 等其余 job 全绿。
- **Task 5**：8 角度 code-review 处置——计划文档 3 处修正（本节即其一）、progress-log 追加结案条目、sanitizer 路径合入前本机 ASan+UBSan 验证 + 合入后 workflow_dispatch；框架层后续改进（knob 检查 NDEBUG 门控无 selftest 锁契约、sdc_stress_config 静默回退）另立 issue。

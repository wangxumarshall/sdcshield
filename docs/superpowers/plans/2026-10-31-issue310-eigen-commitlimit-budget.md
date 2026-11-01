# Plan: issue #310 — eigen_svd sizing 纳入 CommitLimit 承诺预算（第四源）

- 日期: 2026-10-31
- Issue: #310（eigen_svd crash — 内核 OOM kill，overcommit 限制触发）
- 类型: framework sizing 缺陷修复（ARM64 主路径，x86 零调用方）
- 状态: 已判定 (a) 修复

## 1. 判定依据（批判性分析结论）

### 1.1 代码事实

`sdc_budget_bytes()`（framework/sdc_stress_config.h:246-254）的三源
min(物理×‰, MemAvailable×0.9, cgroup 限额−已用) **全部是物理内存口径**，
无任何承诺口径（`grep -rn "overcommit\|CommitLimit\|Committed_AS"` 全仓库
零命中，含 tests/ 与 docs/spec）。arm64 联合求解 `sdc_joint_dim_slice` 的
n_mem 经 `sdc_node_budget_bytes`（framework/sdc_numa.cpp:202）从该预算派生。
eigen_svd_double 的预算参数 `mem_fraction_permille=650`
（tests/cpu/eigen_svd/svd_double.cpp:51）。

### 1.2 内核语义

- `vm.overcommit_memory=2`（strict）: CommitLimit = RAM×ratio/100 + swap
  是匿名私有承诺的**硬上限**，超限 mmap/brk 返回 ENOMEM（malloc NULL →
  Eigen bad_alloc），**不是** OOM-kill。
- `=0`（heuristic，默认）/`=1`（always）: CommitLimit 仅是 meminfo 参考值，
  无强制力；物理内存+swap 耗尽才触发 OOM killer（SIGKILL，不可捕获）。

### 1.3 系统性越界论证（缺陷成立的核心）

=2 且 ratio 偏低（issue Y002 部署形态 ratio=50）的机器上，
MemAvailable（物理空闲）与承诺余量（CommitLimit − Committed_AS）**独立**：
RAM 60GB / swap 4GB / 系统基线承诺 10GB 的机器，MemAvailable 可达 ~45GB
（MemAvailable×0.9 = 40.5GB 预算），而承诺余量仅 34.1−10 = 24.1GB →
sizing 放行 40.5GB ≫ 24.1GB → init/run 期 ENOMEM → T10 降档
kInitRetryLo（mdim 暴跌，压力尽失）或线程级 SKIP（诚实但无诊断指向上移
维度）。**sizing 把"物理可用"当"可承诺"处理，在 overcommit 受限内核上
系统性越界** —— 命中判定 (a)。

### 1.4 VmHWM 反馈回路无法兜底（为何需要静态源）

反馈样本 = VmHWM（RSS 口径），写入点在 run_one_test_inner 尾部。
=2 机器上分配在 RSS 增长**之前**就被 ENENOMEM 拒绝 → bad_alloc 降级路径
的样本到不了写入点（幸存者偏差，sdc_stress_config.h:984 注释已记录超时
child 同型偏差）→ c 系统性低估 → n 膨胀 → 更多 ENOMEM，反馈闭环在承诺
受限机器上是盲的。静态承诺源不可替代（正交：反馈修 c 精度，承诺源修
预算口径）。

### 1.5 Y002 因果链澄清（诚实边界，写入 PR/issue）

issue 自报数据在 =2 下自相矛盾：总 maxrss 6.93GB×6 = 41.6GB **大于**
CommitLimit 34.1GB —— 若 Y002 真是 strict 模式，匿名私有承诺（含 fork 的
COW 记账）会在全局 34.1GB 处被 ENOMEM 拒绝，RSS 根本到不了 41.6GB；且
=2 超限症状是 ENOMEM→bad_alloc→SIGABRT/SIGSEGV/降档，不是 OOM-kill
SIGKILL(9)。Y002 更符合 overcommit_memory=0 的物理 60GB+swap 4GB 耗尽
→ OOM killer 选中最大进程 → SIGKILL（与"不可捕获/无 core/seed 相关偶发/
retry 不同 seed 不复现"全部吻合）。

**边界声明**: 本修复在 =0/1 机器上是显式 no-op（承诺源返回"不约束"），
不改变 Y002（若为 =0）的偶发 OOM 行为；其缓解仍是环境侧（扩 swap / 降
mdim / 限负载），将写入 issue 收尾评论。但 issue 观察到的部署形态
（ratio=50 的 strict 机器）确是仓库真实盲区，修复与 issue 的
overcommit 感知方向一致。

### 1.6 x86 非回归

`sdc_budget_bytes` 全部调用方 arm64-only：sdc_numa.cpp 整文件
`#if __aarch64__ && __linux__`；gemm_double_max_arm.cpp / svd_cdouble_sve.cpp
在 tests_set_sve（tests/cpu/meson.build:467-473）；knob 早退块（:911）同
guard。x86 sizing 走 `sdc_auto_square_dim` / `sdc_joint_dim_slice` x86 分支
的独立 `sdc_phys_mem_bytes()×‰` 模型，**不经过本函数** → 零行为影响。

## 2. 修复设计（最小 patch，单 commit）

### 2.1 framework/sdc_stress_config.h

1. 新增 `sdc_commit_headroom_bytes()`：
   - 读 `/proc/sys/vm/overcommit_memory`；值 ≠ 2（或读失败）→ 返回
     UINT64_MAX（"不约束"，与既有"读失败跳过该源"约定一致——无法判定
     策略时不放大也不缩小预算）。
   - =2 时读 `/proc/meminfo` 的 CommitLimit / Committed_AS，返回
     `0.9 × (CommitLimit − Committed_AS)`（钳 ≥0）。0.9 折扣与
     MemAvailable 源对称：承诺记账虽精确，但并行 slice 各自 sizing 与
     实际分配之间存在时间窗，他进程并发抢承诺同 MemAvailable 的并发
     抢占风险；sqrt(0.9)≈0.95 的维度代价可忽略。
   - meminfo 两字段任一缺失 → UINT64_MAX（跳过）。
2. `sdc_budget_bytes()` 加第四源 min；承诺源**成为最小源**时
   log_warning 一行（进程内 once）："[承诺预算] overcommit_memory=2:
   预算被 CommitLimit 钳制 …"——Y002 类机器上用户可直接看到根因。
3. 头部注释块同步（三源 → 四源）。

### 2.2 文档同步（rule 7）

- CLAUDE.md: "per-NUMA-node three-source budget" → four-source，加
  CommitLimit 源一句话（仅 =2 生效）。
- docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md §4.2: 加
  v3 增补段（承诺源，2026-10-31 issue #310）。
- README.md: 查无三源字样则不动（构建/部署文档不涉 sizing 内部）。

## 3. 验证计划

1. **构建零警告**: PKG_CONFIG_PATH=./third-party/eigen5 meson setup
   builddir --buildtype=release → SDC_BUILD_JOBS=20 ./scripts/build.sh。
2. **基线 no-op 证明**（本机 =0）: 修改前后各跑
   `./builddir/sdcshield -e eigen_svd_double -t 60000`（全核），对比
   [联合求解] 日志预算 GiB 完全一致 + 无 [承诺预算] 日志。
   基线在改动前采集。
3. **激活证明（受控模拟，不动真实 sysctl）**: `unshare -Urmp` 建
   user+mount namespace，bind-mount 假 `overcommit_memory`（内容 "2"）
   覆盖 /proc/sys/vm/overcommit_memory，跑 eigen_svd → 期望 [承诺预算]
   日志出现、预算被 0.9×(CommitLimit−Committed_AS) 钳制、mdim 相应缩小、
   测试 PASS（优雅降维而非 crash）。本机 CommitLimit 61.8GB /
   Committed_AS 58.9GB → headroom ≈ 2.6GB。
4. **回归**: `-e zstd19 -t 2000 -n 1` → exit: pass。
5. **x86 非回归**: 论证（§1.6）+ `objdump`/nm 级抽查可选——本机非 x86，
   以调用方 guard 链为论证（代码事实）。

## 4. 提交与闭环

- 单 commit（rule 1）: `git commit -s`，Signed-off-by: wangxu
  <wangxumarshall@qq.com>，push feature 分支（本 worktree 分支）。
- PR: Fixes #310，body 含 §1.3/§1.5 完整论证 + 验证证据。
- feature-dev:code-reviewer 检视处置 → 人工 merge → issue 收尾评论
  （环境侧解法 + Y002 因果澄清）。

# issue #515: 联合求解 node_cpus 折算改用选中设备域 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `sdc_joint_dim_slice` 的 `node_cpus × workers / cpus_all` 折算不再假设
worker 全机均匀分布，改用框架选中设备集（`shmem->device_info[]`，已含
`--cpuset` 与 taskset 启动亲和限制）作为分布域，消除非均匀拓扑下节点内存
超分（非保守方向）；默认全核路径行为逐值不变。

**Architecture:** 新增 `sdc_numa_effective_threads_selected(cpu, *total)`（sdc_numa.h/.cpp）：
遍历 shmem 全量设备表，统计 effective 节点（含 C1 并入）命中的选中设备数与
选中集总数；折算块（sdc_stress_config.h ARM 分支）以它替换全机口径的分子与
分母，返回 -1 时退回旧全机折算（防御路径）。

**Tech Stack:** C++20（framework 头/源），meson/ninja 构建，无新依赖。

**Spec:** issue #515（audit 报告）+ fleet 派发任务书；所属设计文档
`docs/superpowers/specs/2026-10-27-eigen-p0-p3-fix-design.md` §4.3。

## 关键事实（已核实，决定设计的三个硬约束）

1. **任务书建议的 `sched_getaffinity(0)` 方案在 fork 路径不可用**：
   `child_run` 以 `init_in_aux_thread=true` 调 `run_one_test_inner`
   （sandstone_run.cpp:1474），`test_init` 在被 pin 到
   `device_info[0].cpu_number` **单 CPU** 的辅助线程执行
   （sandstone_run.cpp:1387）——此刻 `sched_getaffinity(0)` 只返回 1 个
   CPU，交集退化为 1，默认路径零变化要求无法满足。故分布域改用框架自身的
   选中设备表。
2. **`shmem->device_info[0..total_device_count)` 就是 worker 真实分布域**：
   设备枚举起点为 `ambient_logical_processor_set()`（启动时
   sched_getaffinity，taskset 生效，device/cpu/topology.cpp:1834），
   `--cpuset/--deviceset` 经 `apply_deviceset_param` 再过滤
   （sandstone.cpp:1066）；child 内 `restrict_topology` 只重指本地
   `device_info` 指针，shmem 全量表不动；`commit_shmem()` 先于
   `test_preinit`（gemm_double_max_arm.cpp:158 明确记载）。preinit（父）
   与 test_init（child）两口径均可靠。
3. **本机拓扑（实测 2026-11-08）**：192 CPU / 4 socket / 8 NUMA 节点 ×
   24 CPU（node0=0-23 … node7=168-191）。复现数值：`--cpuset=0-23 -n 24`
   时旧折算 node_cpus = 24×24/192 = **3**（真实 24，足迹超分 8×）；
   `--cpuset=0-47` 时 = 24×48/192 = **6**（真实 24，超分 4×）。

## Global Constraints

- 全部改动位于 `#if defined(__aarch64__) && defined(__linux__)` 守卫内
  （sdc_numa.h/.cpp 整文件即此守卫；sdc_stress_config.h 改动在 :1283 起的
  ARM 分支内）——x86-64 零触碰（rule 5，PR 检视按检查论证）。
- **默认全核路径行为零变化**：选中集 == 全机在线集时，折算输入
  （node_cpus, cpus_all）与旧值恒等；`workers == cpus_all` 时折算本就跳过。
- 不扩散：cgroup 哨兵（sdc_stress_config.h:115-173 一带）属 issue #465，
  预算分摊（sdc_node_budget_bytes 的 MemFree 比例）不在本修复范围。
- 一个补丁一个逻辑单元：单次 commit（plan 文档 + 代码 + spec 补记 +
  progress-log 条目），`git commit -s`，尾行
  `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`。
- 构建零新警告；`-e zstd19 -t 2000 -n 1` 回归 pass。
- 无单测：联合层不链接 sdc_numa 符号是本仓既定边界
  （unit-tests/sdc_stress_config_tests.cpp:18-23 头注），验证走真机日志
  对比（CLAUDE.md rule 2 机制层口径）。

## Review Focus

1. **默认全核回归**——选中集应恒等于全机在线集；若设备枚举与 sysfs
   online 有差集（如启动后热插），折算输入漂移。钉住：Task 3 S1 前后
   n_mem 逐值一致 + 修正日志不触发。
2. **均匀受限集的恒等性**——跨节点均匀的 taskset（如每节点取 3 CPU）下
   新旧折算应同值（均匀假设仅对非均匀集失真）。钉住：Task 3 S1b
   `taskset -c 0,24,48,...,168` 前后 n_mem 一致。
3. **-n < 全核（无 cpuset）不变**——workers < 设备数时折算仍在选中集
   （=全机）上按比例缩，与旧值恒等。钉住：Task 3 S1c `-n 24` 前后一致。
4. **shmem 不可读防御**——total_device_count<=0 / 探测失败时必须退回
   旧全机口径而非 0 或崩溃。钉住：代码路径 return -1 → 调用方保持
   旧值（评审检查 + S1 间接覆盖）。
5. **越界/非法 cpu_number**——设备表出现 c<0 或 ≥4096 时跳过该设备
   （sdc_numa_node_of_cpu 的既有口径），不得计入任何节点。钉住：
   代码评审（无法真机构造该输入，如实记录边界）。

---

### Task 1: 基线捕获（修复前，即「失败测试」）

**Files:** 只读运行，产物存 `/tmp/issue515/`。

- [x] **Step 1: 记录环境自证**

Run: `taskset -pc $$`（确认验证 shell 自身亲和不受限，否则 S1「默认
全核」前提不成立，需换不受限 shell）；`nproc`、node cpulist 快照存档。

- [x] **Step 2: 三场景基线运行并存日志**

```bash
mkdir -p /tmp/issue515
./builddir/sdcshield -e eigen_svd -t 3000             > /tmp/issue515/before_s1_full.log 2>&1
./builddir/sdcshield --cpuset=0-23 -e eigen_svd -t 3000 > /tmp/issue515/before_s2_node0.log 2>&1
./builddir/sdcshield --cpuset=0-47 -e eigen_svd -t 3000 > /tmp/issue515/before_s3_two.log  2>&1
```

Expected: 每份日志含 `[联合求解] eigen_svd.mdim: n_mem=… n_run=… AR=… c=…
→ … (预算 …GiB)`。S2/S3 从日志反解 node_threads =
预算/(c×n_mem²)：S2 应得 ≈3（真实 24，超分 8×）、S3 ≈6（真实 24，
超分 4×）——issue 复现的实证。

- [x] **Step 3: 基线构建日志（警告计数基准）**

```bash
PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release
./scripts/build.sh 2>&1 | tee /tmp/issue515/build_before.log
grep -ci warning /tmp/issue515/build_before.log
```

### Task 2: 实现（选中设备域折算）

**Files:**
- Modify: `framework/sdc_numa.h`（新声明，effective_threads 声明之后）
- Modify: `framework/sdc_numa.cpp`（实现，含 `#include "sandstone_p.h"`）
- Modify: `framework/sdc_stress_config.h:1331-1352`（折算块 + 块注释）

**Interfaces:**
- Produces: `int sdc_numa_effective_threads_selected(int cpu, int *total_selected)`
  ——返回 effective 节点（含 C1 并入）命中的选中设备数；`*total_selected`
  写入选中设备总数；不可读（探测失败 / total_device_count≤0 / 无有效
  anchor）返回 -1 且 `*total_selected` 置 0。全核时返回值 ==
  `sdc_numa_effective_threads(cpu)`、`*total_selected` ==
  `sdc_numa_probe()->total_cpus`。

- [x] **Step 1: sdc_numa.h 声明 + sdc_numa.cpp 实现**

实现要点（签名与守卫之外的算法约束）：遍历
`sApp->shmem->device_info[0 .. sApp->shmem->total_device_count)`，
对每个 `cpu_number`（int；c<0 或 c≥4096 跳过）取
`sdc_effective_node(t, c)`，与 anchor 的 effective node 相等则计数；
anchor 不可得（<0）返回 -1。sdc_effective_node/sdc_fallback_node 为
本文件既有 static，直接复用（C1 语义单一真源）。

- [x] **Step 2: 折算块改造（sdc_stress_config.h）**

```c
int workers = sdc_total_threads();
int cpus_all = sdc_numa_probe()->total_cpus;
int sel_total = 0;
int sel_node = sdc_numa_effective_threads_selected(cpu, &sel_total);
if (sel_node > 0 && sel_total > 0) {
    if (sel_node != node_cpus || sel_total != cpus_all)
        log_warning("[联合求解] %s.%s: 折算域改选中设备集 "
                    "node_cpus %d→%d, 域 %d→%d (issue #515)",
                    …, node_cpus, sel_node, cpus_all, sel_total);
    node_cpus = sel_node;
    cpus_all = sel_total;
}
/* workers/domain 折算与 node_cpus<1 兜底原样保留 */
```

约束：修正日志仅在选中集 ≠ 全机集时触发（默认全核静默，日志口径零
变化）；workers/cpus_all 折算表达式与向下取整语义逐字保留（超订防御
issue #423 不动）；块注释改写为新语义并保留 #423/#515 出处。

### Task 3: 重建与前后对比（验证主体）

- [x] **Step 1: 重建并检查零新警告**

`./scripts/build.sh 2>&1 | tee /tmp/issue515/build_after.log`；
`grep -ci warning` 与 Task 1 Step 3 基线相等（增量重编同一批 TU）。

- [x] **Step 2: S1/S1b/S1c 零变化证明（默认全核 + 均匀受限 + -n）**

重跑 Task 1 Step 2 的三条命令（`after_*` 日志）+ 补两条：

```bash
taskset -c 0,24,48,72,96,120,144,168 ./builddir/sdcshield -e eigen_svd -t 3000   # S1b 均匀
./builddir/sdcshield -n 24 -e eigen_svd -t 3000                                    # S1c
```

Expected: S1/S1c 的 `[联合求解]` 行与 before 逐字一致（n_mem/n/预算；
n_run 允许探针计时的自然抖动，如实引用）；S1b 因 MemFree 漂移 n_mem
允许 ±1 内整数漂移，反解 node_threads 前后一致（S1b: 每节点 3）；
S1 系列不出现「折算域改选中设备集」日志。

- [x] **Step 3: S2/S3 修复证明（非均匀拓扑不再超分）**

Expected（反解 node_threads）：S2: 3→**24**（n_mem 缩 √8≈2.83×）；
S3: 6→**24**（n_mem 缩 2×）；两条日志出现「折算域改选中设备集
node_cpus 24→24, 域 192→24/48」。引用真实对比行。

- [x] **Step 4: zstd19 回归**

`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。

- [x] **Step 5: x86-64 非回归（检查论证）**

改动均在 aarch64+linux 守卫内；`#else`（x86 fallback）分支零触碰。
引用 diff 上下文论证。

### Task 4: 文档同步 + 提交 + PR + 报告

- [x] **Step 1: spec/progress-log 补记**（若 spec §4.3 描述折算口径则同步；
  progress-log.md 加 dated 条目）。
- [x] **Step 2: 单提交**：`git checkout -b fix/issue-515`（自 origin/main）
  → `git add` 四类文件 → `git commit -s`（正文含根因/修复/实测记录）→
  `git branch --show-current` 确认 → push。
- [x] **Step 3: PR（API）**：title `fix: 联合求解 node_cpus 折算改用
  选中设备域, 非均匀 --cpuset 不再超分节点内存 (#515)`，body 含验证
  实录，`Closes #515`。
- [x] **Step 4: issue #515 处置评论**（API），输出 `fleet-loop: done …`。

---

## 执行记录（2026-11-08，native 执行）

- 全部任务完成；验证证据存 `/tmp/issue515/nb_{before,after}_*.log`（nb 前缀
  = 新基线 ac8648be）。
- **会话事件**: 首轮验证在 5583fc84 完成后，origin/main 前移 10 提交
  （#552 AR 表行1 重标定 / #493 cpulist 溢出防护 / #560 RSS 表头勘误等，
  三者与本修复的 hunk 无重叠）——重定基至 ac8648be，**全部 before/after
  在新基线重取**（首轮旧基线日志保留于 /tmp/issue515/{before,after}_*.log
  供对照；SVD 族 n_run 因 #552 run_budget 140000→200000 系统性 +12.6%，
  n_mem/c 不变，与本修复正交）。
- **S1c 意外发现（超出 issue 叙述的同类 bug）**: `-n 24`（无 cpuset）时
  框架把设备表裁到首 24 CPU 且全落 node0，旧均匀折算同样超分
  （node_threads=3 vs 真实 24，n_mem 17766→6241）——即 `-n` 标准档
  一直在踩此路径，n_run 时侧钳制掩盖了内存超分。本修复一并治好；
  `-n 1` 恒等（两侧 node_cpus 均 1）。
- Review Focus #4/#5（shmem 不可读退全机口径 / 越界 cpu_number 跳过）
  为防御路径，真机不可构造，按代码评审覆盖并如实记录。

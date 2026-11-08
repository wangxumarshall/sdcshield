# Eigen 用例 P0-P3 修复设计（NUMA 感知 sizing + 内存最大化 + OOM 防护）

日期：2026-10-27（v2，对抗性审查修订版）
状态：设计定稿（brainstorming + LSP 深度核实 + 隔离对抗性审查后修订）
分支：feat/eigen-p0-p3-fix
实测基线：本机 sdc1-01-04（128 核 / 61GB / 4 NUMA node 但**内存全在 node0** /
SVE-256）119 个测量点（Phase A 94 + B 21 + C 4 + R 17，另含多用例连跑验证）。

> v2 修订记录（对抗审查 findings：C1/C2/M1-M8/m1-m8/q1-q4，审查全文见
> 会话记录；v1→v2 变更在各节以【v2】标注）：
> C1 per-node 退化规则；C2 反馈闭环重新定位；M1 预算 240→200s、AR 取实测
> 上界；M2 AR 表二维化（族×拓扑）；M3 三点拟合扣固定开销；M4 节点预算
> 合成公式 + cgroup v1；M5 反馈槽生命周期/static_assert/访问器；M6 no_fork
> 门控；M7 验证判据分族；M8 指称修正（dynamic_square vs max_arm）；
> m1 选择偏差、m2 bad_alloc→SKIP 对齐先例、m3 quick 模式、m4 dumb-plan
> 归属、q1 表口径、q3 gem5 验证面收窄。

## 1. 问题清单（全部实证）

### P0 真实场景稳定失败（产品 bug，两次独立复现）
- **P0-1 `eigen_svd_cdouble_sve`**：全核 auto `-t 5000` → 321s timeout、42.6GB
  （物理 66.7%）。根因：preinit 时间模型 `t(N)=243.4·(N/4400)³+1`（单线程
  标定，svd_cdouble_sve.cpp:177）不含全核争抢；本机 42.6GB 全挤 node0 →
  带宽争抢实测 SVD 族 18-25×（单解 8s → >160s）→ 300s 硬宽限被击穿。
- **P0-2 `eigen_svd_bidiag_sve`**：同场景 343s timeout、16.9GB。根因：**无
  任何时间校准**（`eigen_gemm_double_max_arm` 同样未接——【v2/M8】v1 称
  "唯一"不实），auto mdim 直撞 2048 knob 上限。

### P1 内存预算失效（"最大化"没有发生）
- 模型虚报 5-61×：GEMM 4.7-5.7×、SVD 5.9-7.9×、sparse 23×、Jacobi 34-61×。
  真实场景 eigen 家族只吃物理内存 **5-18%**（最高 11.1GB/61GB）。
- 预算只读 `_SC_PHYS_PAGES` 总量：不看 MemAvailable、不看 cgroup（有
  "job oom" 实测前科，svd_cdouble_sve.cpp:114-120 注释）。
- 无 per-NUMA-node 预算（22 处 sizing 无一感知节点）。
- init 共享矩阵与 128 worker 同刻分配峰值不在模型。【v2/M8】init 共享
  矩阵（3·S·n²/child）与 worker 侧 `gemm_mdim` 重复计算属于
  **dynamic_square 家族**（gemm_double_dynamic_square.cpp:64-74/87）；
  `eigen_gemm_double_max_arm` 无 init 矩阵（Phase A 实测 base=4.3MB 证实）、
  无 worker 重算路径。

### P2 OOM 防护缺失
- SVD/GEMM 模板 `test_run` 无 `bad_alloc` catch（仅 eigen_sparse 有）→ 分配
  失败走 malloc SIGKILL/abort，用户看到被杀而非诊断。
- 显式 `-O` 旁路全部防护（无校准、无预期内存警告），仅剩 knob clamp。

### P3 一致性
- 三套并行 sizing（helper / sve 手写 :109-145 / gemm 手写），系数与上限
  各异（0.65 vs 650‰ vs 850‰；ratio 12 vs 6 vs 2.0）。
- 时间上限与内存上限从不联合求解。
- 卫生：【v2/m5】原列三条中两条已在 f2af45d1（sleef 扫描+tag）与
  82ab5556（merge 标记清理）修复，撤销；仅剩 tests/cpu/meson.build:449-465
  注释过时（仍称 tests_sve 固定 128-bit 锁）。

## 2. 目标 / 非目标

**目标**：
1. P0 两用例在全核 auto 下 `exit: pass`（不再 timeout）。
2. 内存消耗：GEMM/dynamic_square 族 ≥ 现状 2×；SVD/sve/bidiag 族不
   timeout 且峰值 ≥ 现状 80%（时间上限是物理约束，【v2/M7】分族判据）。
   全部用例无 OOM（≤ 预算上界）。
3. 显式 `-O` 超预算时有警告；分配失败有降级而非裸 SIGKILL。
4. sizing 统一到单一 helper，时间/内存联合求解。
5. x86-64 行为 byte-identical（rule 5）。

**非目标**：
- Eigen 上游 complex `ptranspose` 1024 档缺陷修复（单独规划）。
- Jacobi 族的 SVD 算法替换。
- SVE 512/1024 档的本机实测（无硬件；gem5-fi 验证，见 §6）。

## 3. 总体架构

```
[启动/选择期]                      [child test_init]               [worker/child 退出]
sdc_numa_probe() ──┐
  节点/内存/CPU映射  ├─→ sdc_joint_dim_slice() ─→ mdim ─→ 分配 ─→ 计算
  AR 表(族×拓扑)     │   = min(n_mem, n_run200, clamp)          │
                    │   n_mem = sqrt((node_budget−sh)/(node_thr·c)) ↓
sdc_budget_*() ─────┘   n_run200 = (200s/(AR·2·a_ms))^(1/p)  VmHWM(sdc_read_vm_hwm)
                                                             写 PerThreadData::Main
RSS_RATIO_TABLE ──→ c 初值（族→实测系数）                      （每 slice 一槽）
                    ↑                                            │
                    └── 内存受限多 fracture 的 init 读 ◄── EMA ◄┘
                        （SVD 类默认时序不触发，见 §4.4）
```

四个子系统：NUMA 探测与 AR（慢化）分档表、预算升级（含 per-node 与退化
规则）、联合求解 helper、VmHWM 反馈闭环（定位收窄）。P2/P3 是模板层与
卫生收尾。

## 4. 子系统设计

### 4.1 `framework/sdc_numa.{h,cpp}`（新，arm64 门控）
`sdc_numa_probe()`：静态缓存一次。读 `/sys/devices/system/node/nodeN/meminfo`
与 `cpulist` → `struct sdc_numa_topology { int nodes_with_mem; }`。

**【v2/M2】AR（全核慢化）表二维化：慢化是负载×拓扑的函数，不是拓扑属性**
（同拓扑实测 GEMM 4.4× vs SVD 18-25×）。`sdc_ar_factor(family, topo)`：

| 有内存节点数 | 内存密集族（SVD/sparse/bidiag） | 计算密集族（GEMM） | 来源 |
|---|---|---|---|
| 1 | 40 | 15 | 【#506 重标定 2026-11-08】worst-child 实测带上界：T5 复数 BDCSVD AR≈39（127 线程 n=1100，远程 child SIGQUIT 304-314s×2，2026-10-27）、T8 eigen_svd float AR≥40（SIGQUIT@321s）；GEMM 天花板足迹 ~15（中足迹 4.4、256-bit 档 11.2，取上界）。旧 25/5 系本地/平均 child 口径，未覆盖无内存节点远程 DRAM worst-child |
| 2 | 10 | 4 | 保守估计，待标定 |
| 4 | 5 | 3 | 保守估计，待标定 |
| ≥8 | 4 | 2.5 | 保守估计 |
| 探测失败 | 40 | 15 | 最差档兜底，永不向上猜（#506 随行 1 同步） |

保守方向的定义（【v2/M2】明确）：AR **高估** = 时间预算收紧 = mdim 偏小 =
内存欠压（P1 损失）；AR **低估** = 时间超预算 = timeout 风险（P0 复发）。
表值宁可高估（保 P0），P1 靠 §4.4 反馈与族区分兜底。同源锚点：
`sdc_calibrated_dim` 注释的 sparse 24.9×（单节点内存）。

`--max-cores-per-slice` 的 dumb plan（slicing.cpp:104-118）按连续 CPU 切、
可跨节点——slice 归属节点取其**首 CPU 的节点**（近似，注释记录）。【v2/m4】

### 4.2 `sdc_budget_bytes(permille)`（sdc_stress_config.h）
全局：`min(物理×‰/1000, MemAvailable×0.9, cgroup 限额−已用)`；任一来源读
失败即跳过；cgroup **双路径【v2/M4】**：v2 `/sys/fs/cgroup/memory.max −
memory.current`（值为 "max" 跳过），v1 `/sys/fs/cgroup/memory/
memory.limit_in_bytes − memory.usage_in_bytes`（limit ≥ 物理内存视为无限制）。
本机实测为 v1——"job oom" 前科场景恰在 v1。

**per-node 版 `sdc_node_budget_bytes(permille, cpu)`【v2/M4+ C1】**（PR#302 检视 #7 勘误: 第二参数是 **CPU ID** 而非节点号——与实现/声明/调用方一致, 函数内部经 sdc_numa_node_of_cpu 映射到该 CPU 所在节点; 原稿写 node 会让照抄者把节点号当 CPU 号传入、预算记到错节点）：
节点 meminfo **没有 MemAvailable 字段**（实测仅 MemTotal/MemFree/…），合成：
`node_avail ≈ global_MemAvailable × (node_MemFree / Σ_nodes node_MemFree)`；
MemFree=0 的节点分摊为 0。

**退化规则（C1，必须实现）**：`node_threads[n]` = 该节点 CPU 上的 worker 数；
**无内存节点（MemTotal=0）的线程需求并入其内存 fallback 节点**（内核
first-touch 的实际行为，本机即 node0）：`fallback_node` = 有内存节点中
MemFree 最大者；无内存节点的 slice 的预算与线程数都记到 fallback_node 头上。
node0-only 机器：全部线程计入 node0、node_budget ≈ 全局 MemAvailable×0.9 →
**精确退化为全局模型**（与验证行"行为与全局模型对照一致"自洽）。

**v3 增补（2026-10-31, issue #310）— 第四源「承诺余量」**：前三源全物理
口径，在 `vm.overcommit_memory=2`（strict）内核上会系统性越界——匿名私有
承诺（含 fork 的 COW 记账）受 `CommitLimit = RAM×ratio/100 + swap` 硬限制，
超限是 mmap/brk ENOMEM（bad_alloc 降档/线程级 SKIP），而 MemAvailable（物理
空闲）与承诺余量独立（ratio=50 的 60GB+4GB swap 机：MemAvailable×0.9 可给
~40GB，承诺余量只有 CommitLimit−Committed_AS）。新增
`sdc_commit_headroom_bytes()`（sdc_stress_config.h）：仅 =2 时读
/proc/meminfo 返回 `0.9×(CommitLimit−Committed_AS)`（承诺已满 → 0；0.9 与
MemAvailable 源同"现读+折扣"哲学，折扣留给并行 slice sizing 与实际分配的
时间窗 + 他进程并发抢承诺）；非 =2（CommitLimit 无强制力）或读失败 →
UINT64_MAX 不约束（跳过，与"读失败跳过该来源"约定一致）。VmHWM 反馈替代
不了本源：=2 下分配在 RSS 增长之前就被拒，降级样本到不了反馈写入点（幸存
者偏差）→ c 低估 → n 膨胀 → 更多 ENOMEM。成为最小源时打一次
`[承诺预算]` 诊断日志。x86 零调用方（sdc_numa.cpp 整文件 arch guard、
两个直调测试在 tests_set_sve），行为不变。

### 4.3 `sdc_joint_dim_slice()`（联合求解核心，新）

**拟合口径【v2/M3，防 SVD 内存倒退 2.4×】**：`sdc_calibrated_dim` 现有
两点差分拟合对固定开销敏感——`t(N)=243.4·(N/4400)³+1` 的 +1s 在 N=300
探针档占 93%，k_floor 只救指数不救系数，外推系数被抬高 ~14× → 立方根
2.4× 收缩。修正：(a) probe 档抬升阈值从 <1ms 改为 **<100ms** 才翻倍
（使固定开销占比 <10%）；(b) 拟合改为**三点 t=a·n^p+b**（最小二乘或
逐差），系数 a 用扣除 b 后的值；两者取一，推荐 (a)（改动最小）。
`sdc_calibrated_dim` 增加出参 `{a_ms, p}`（默认 nullptr 保持现签名兼容）。

> **勘误（机制更新，2026-10-31，issue #351 / PR #377；2026-11-02 R2 复核）**：
> 上文 (a) 的
> 「probe 档抬升阈值 <1ms→<100ms」经参数化后**不再是 `sdc_calibrated_dim`
> 的模板缺省行为**，而是带 fit 出参消费方的**显式实参**——现签名为
> `sdc_calibrated_dim(..., struct sdc_fit *fit = nullptr, double probe_floor_ms = 1.0)`
> （framework/sdc_stress_config.h；fit 保持自 `f84966bc` 引入起（至本 PR
> 参数化前）的第 8 参位次，
> probe_floor_ms 殿后——字面量 `0`/`NULL` 作第 8 实参仍绑 fit，double
> 字面量误入则编译报错）：模板缺省 `1.0` 恢复 `f84966bc` 之前
> 存量 7 参链（跨 arch，含 x86-64 `#else` 分支）的 1ms 历史噪声底语义
> （rule 5，调用点源码零改动）；**需要本节 M3 固定开销口径的调用方必须
> 显式传 `sdc_m3_probe_floor_ms`（=100.0，具名常量三处绑定）**（现役唯一者
> = 联合求解 `sdc_joint_dim_slice`，ARM-only，连同 `&fit` 一起传）。按本
> spec 实现**新的 fit 消费方时切勿依赖缺省**：缺省 1.0 下差分扣 b 的
> `a_ms` 会被固定开销与 1ms 级计时噪声污染（a_ms 虚高 → n_run 偏小 →
> 激发维度欠压）。该错误组合可编译；PR #377 起有运行期 `log_warning`
> 兜底（判据为 probe 档**实测时长** < `sdc_m3_probe_floor_ms`，非传参值
> ——传对参数但 hi 截断致档位抬不上去同样告警），差分本身退化（档位
> 重合或计时持平/倒挂）回退 b=0 口径、固定开销全数计入 `a_ms` 时另有
> 独立告警；告警均不修正结果，仍必须显式传参。

```c
n_opt = min(
    n_mem  = sqrt((node_budget − node_slices·shared) / (node_threads · c)),
    n_run200 = (200s / (AR(family, topo) · 2 · a_ms))^(1/p),   // 300s×0.67
    knob_hi)
```

- `node_threads` = 该节点全部 worker 数（含并入的无内存节点需求）。
  Heuristic 默认 slice≡NUMA（build_topology 分组键 numa_id 优先，
  topology.cpp:1755-1759）；`--max-cores-per-slice` 下同节点多 slice 按
  节点合计口径收缩（各 slice 输入相同 → N 一致）。
- 时间预算 240s→**200s**【v2/M1】：300s 硬宽限需容纳 init 探针 + fill +
  AR 方差（SVD 实测带 18-25×，25 时迭代+init ≈296s 刀锋）。
- `c` 来源优先级：VmHWM 反馈（本 slice 槽有效且命中同 test）→
  RSS_RATIO_TABLE 静态初值 → 调用方传统三元组（未迁移用例）。
- RSS_RATIO_TABLE：算法族 → 实测 c（KB/n²/线程）。【v2/q1 口径注明】
  值来自 Phase A 的 **(child@T4 − child@T1)/3 差分增量口径**
  （BDCSVD-f32 0.067 / f64 0.135 / c128 0.205 / Jacobi 0.031-0.124 /
  GEMM-f64 0.029【2026-10-28 T9 重标定 0.0315, 见表内注释】 / f32 0.016 /
  c128 0.063 / sparse 0.009 / bidiag 0.031），
  与 svd_cdouble_sve.cpp:92-95 注释的"单进程 9.7-10× 输入体积"（~155B）
  口径不同（后者含基线）——模型用途取差分口径，差异 35% 属口径差非矛盾。
  【2026-11-08 勘误 #511】"跨机可移植"过强：c 的主体（矩阵驻留数×
  元素大小）一阶可移植，但差分口径漏计非 n² 缩放驻留（Eigen packing
  buffer / 联合求解探针高水位 / 分配器 slack），VmHWM 实测隐含值同机
  即偏 +1~4%（重标定后两机型实测），残差随机器/用例漂移——表值是初始
  锚，T7 VmHWM 反馈闭环（§4.4）才是同机收敛通道；Eigen 版本升级需
  重标定（表注释为真源）。
- knob 三层 fallback（knob > env > auto）语义保留；显式路径跳过联合求解
  但打预期内存警告（P2b）。
- **quick 模式【v2/m3】**：`sdc_quick_run()` → 直接返回 lo（与
  sdc_auto_square_dim:300-305 同捷径），防 CI smoke 单迭代 200s。
- 命名双语义：`_slice` 后缀 = per-node 语义；现有 `sdc_auto_square_dim`
  全局语义不动、标 deprecated 注释。

### 4.4 VmHWM 反馈闭环（【v2/C2】定位收窄）
**触发前提的诚实声明**：默认时序下 `desired_duration = 0.95×单迭代`
（svd_cdouble_sve.cpp:178）⇒ fracture 循环在 1 个 child 后退出
（sandstone_run.cpp:1831-1833 `runtime >= duration`）⇒ **SVD 类默认
不发生多 fracture，闭环无读者**；memfd 每次运行新建（sandstone.cpp:342）
⇒ 无跨运行记忆。闭环实际服务：(a) **内存受限多 fracture**（n_mem <
n_run200，即 GEMM/dynamic_square 类——它们迭代快、fracture 多）；
(b) 失败重试间 mdim 漂移修正；(c) 用户显式长 `-t`。SVD 类的 c 修正
依赖静态表准确性（局限记录于 §9）。

- **存放**：`PerThreadData::Main` 追加字段（每 slice 一个实例，位于
  thread_data_offset 之后的可写区；shmem header 被 `protect_shmem`
  mprotect 只读——LSP 核实修正 #1）：
  ```c
  struct MemFeedback { uint64_t test_id_hash; double eff_ratio;
                       uint32_t samples; };   // POD, 保持 trivially_copyable
  ```
- **生命周期【v2/M5，防闭环静默死亡】**：`init_per_thread_data()`
  （sandstone_run.cpp:1204）在 test_init **之前**运行且父进程 skip 路径
  （1608/1623）清全部槽——**MemFeedback 不进 `Main::init()`**，仅在
  parent 的 `init_shmem` 首次清零一次，语义为"跨 test/fracture 持久"。
  `static_assert(sizeof(PerThreadData::Main) == 64)`（sandstone.cpp:322）
  同 commit 更新为 128。
- **读取**：新增 `sdc_read_vm_hwm()`（`memfpt_current_high_water_mark`
  在 release 返回 0 且转正会激活 x86 诊断路径，不动——修正 #2）。
  基线污染：fork child VmHWM 起点 = 父进程当时 RSS（实测 8.3MB），eigen
  家族 <1% 可忽略；MAP_SHARED shmem 页各触碰者全额计账（实测子进程
  RssShmem 仅 24KB）——两项均记录为已知口径。【v2 审查实测】
- **写入点**：`run_one_test_inner` 尾部（test_cleanup 之后、返回之前，
  sandstone_run.cpp:1283-1288）。**fork 模式门控【v2/M6】**：
  `no_fork`（`-f none`）下测试在主进程跑、VmHWM 跨测试累计 → 虚高，
  该模式不写入。
- **算法**：`eff_ratio = VmHWM/(n²·elem·slice_threads)`；EMA α=0.5 写入
  本 slice 槽；±15% 死区。多 slice 各写各槽无竞态；读侧聚合取
  **median**【v2/q4：max 取最噪样本，median + 死区已防抖】。
- **跨 fracture**：MAP_SHARED memfd，上代 child 的写对下代可见；exec
  模式 shmemfd 经 argv 传递同样可见。
- **选择偏差【v2/m1，局限】**：超时/被杀 child 到不了写入点 → 反馈只收
  成功样本 → c 系统性偏乐观（mdim 偏大方向不自愈），靠死区 + 静态表
  下限（c 不低于表的 70%）缓解，如实记录。
- **兄弟槽访问【v2/M5】**：`main_thread_data(i>0)` 在子进程有 assert
  （sandstone_p.h:457）→ 新增内部裸指针访问器。

### 4.5 P0 落点（三个用例）
- **bidiag**（P0-2）：init 的 `sdc_auto_square_dim(...)` 调用替换为
  `sdc_joint_dim_slice(..., k_floor=3.0, solve_once=bidiag SVE 求解)`
  【v2/m7：solve_once 必须用与 run 期同构的 SVE 内核路径（非标量参考，
  两者差 4-8×）】。
  【v3/issue #420 收口（2026-11-01；2026-11-04 重锚定落地）：solve_once 返回码
  契约——run_bidiag_once
  分配失败返回 -1 时，probe 曾完全丢弃返回码，μs 级失败分支噪声被当作
  超快求解外推（k 钳制后 a=t1/p1^k≈1e-13 的"合法" fit，n_run 失控），
  mdim 落 n_mem/hi 上限 = P0-2 复发路径（机制层复现：dim=2048 /
  n=1900@n_mem）。修复：sdc_calibrated_dim 以非零返回码为负哨兵计时、
  任一校准档失败退守 lo（fit 无效）；sdc_joint_dim_slice 任一档失败
  整体退守 lo、n_mem 弃用——失败教义统一为 lo（现役探针两种：Eigen 族
  异常上抛 ×2；历史第三种为返回码式——bidiag run_bidiag_once，已随
  用例 2026-11-02 退役，本谓词对其恒假、属防御性契约封堵）。契约锁定于
  framework/unit-tests/sdc_stress_config_tests.cpp。
  2026-11-04 重锚定（#517 fit_only 合入后）：联合层不再执行验证档，
  失败检测覆盖抬升/mid 两档；fit-only 弃用返回值使模板内早退只落
  fit={0,0}，联合层 probe_failed→lo（n_mem 弃用）成为该路径的安全
  机制本体——两者互补，缺一复活半个 P0-2 路径。】
- **svd_cdouble_sve**（P0-1）：mdim 计算从 preinit 迁到 child `test_init`
  （`sve_probe_and_init` 内，tier 选择后）；preinit 只留 desired_duration
  ——【v2/q2】用最保守 AR（内存密集族单节点 40，#506 重标定后）+ 静态表 c 推 **n 的
  上界**再推 duration，不变式：child 精算 n ≤ preinit n 上界。
  【2026-10-31 勘误，issue #352】该不变式**仅对 auto 分支成立**：显式
  knob/env 档 child 逐字采用 dim=knob（不经 min(n_mem, n_run, hi)），
  修复前 preinit 的 duration 模型只随 n_hi 缩放 → knob>n_hi 的显式
  大维度档落在 300s 硬宽限被 SIGQUIT 误杀（迁移前 preinit 的 timeout
  恰随 knob 缩放）。现 preinit 与 child 同通道同 clamp 读 knob/env，
  n_model=knob（显式）或 n_hi（auto）；`gemm_double_max_arm`（§4.5
  末项）同步同构修复。child 里
  `sdc_joint_dim_slice` per-node 精算。
  【2026-11-02 勘误，PR#384 检视（意见 4432271415）】显式档的
  duration 模型须按**全迭代** ar×2×t(knob) 建模（一次 run 侧迭代 =
  golden 首算 1 次 + DUT 每轮求解 1 次 = 共 2 次 BDCSVD【R6 勘误，
  意见 4463986208：原文「golden 首算 + DUT 两次 BDCSVD」把总数写成
  3 次，与 sandstone_eigen_common.h run() 实际（入口 prime 1 次 +
  每 do-while 轮 DUT 1 次，golden 重算每 16 轮摊 1 次未计入）不符；
  模型系数 ×2 = 总数 2 次，数值自始正确，错的只是构成描述】，
  全核下每次吃 AR 争抢；与 gemm 修复
  f6783077 及联合求解自身的迭代模型 per_solve=run_budget/(ar×2) 同构）：
  只乘 0.95×t(knob) 时 timeout=4.75×t(knob)+30s，kill 条件
  AR_real>~2.4，而单内存节点最坏 child 实测有效 AR≈39——多 worker
  显式档必被误杀，#352 失败模式只对 -n 1 真正闭合。ar 取内存密集族
  表值（与 auto 分支 ar_fam 同族）；workers==1 时取 1（表值是全核
  口径，单 worker 自争抢为 1；不折算则 -n 1 显式档裸跑 target 被拉到
  ~ar 倍 t(knob)，`-t … -n 1` 标准回归路径的单迭代语义破坏）。auto
  分支维持 t(n_hi)×0.95 不变：其 AR 保护来自联合求解 AR-aware 的
  n_run（run_budget=140s 档，实测最坏 ~213s，距 300s floor 留 ~25%
  裕量），且 n_run 在本 host class 恒把 child n 压在时间侧——显式档
  恰恰绕过这层保护，才必须把 AR 显式放进模型。
  【2026-11-02 勘误，PR#384 检视（意见 4432271419）】显式档 preinit 兼写
  test->minimum_duration = ar×2×t(knob)（无 0.95 裕量，"至少跑完一次
  迭代"的物理下限）：-t 在框架层无条件覆盖 desired_duration
  （sandstone_run.cpp test_duration 的 sApp->test_time 分支），不设 min
  夹钳时 `-t 2000 -O …mdim=<大>` 的 target 被钉在 2 s、timeout 落到
  300 s floor，单次全迭代被 SIGQUIT 误杀（修复前实测 -t 2000 -n 1
  -O mdim=4000 → 320.6 s SIGQUIT）——与 #352 头条失败模式同型的确定性
  kill 路径。auto 分支**不写**：`-t 2000 -n 1` 标准冒烟必须保持秒级
  （auto 单迭代由 run_budget=140 s 设计收在 300 s floor 内），且
  -n 1 下 n_hi 撞 MDIM_MAX，写 min 会把冒烟拉到 t(6000) 分钟级。
  【2026-11-02 勘误，PR#384 检视（意见 4439402781/4439402765）】
  gemm_double_max_arm 显式档同步 SVD 的两处同构缺口：(a) workers==1 时
  ar 取 1（与 svd f23e2fd9 同构——AR 表值是全核口径，单 worker 自争抢为
  1；不折算则 -n 1 显式档裸跑 target 被拉到 ~ar 倍 t(knob)，compute 表值
  5.0 → ~4.76 次迭代，标准 `-t … -n 1` 回归路径的单迭代语义破坏）；
  (b) 显式档兼写 minimum_duration=ar×2×t(knob)（与 svd 9a82f53b 同构
  ——compute 族表值 5.0 在大足迹下实测低估 3×，`-t 2000 -O mdim=4096`
  全核单迭代 ≈242-264s 距 300s floor 余量不足 20%，表值档 host class
  上确定性 SIGQUIT）。GEMM auto 分支维持 f6783077 原模型（ar×2×t(n_hi)
  不随 worker 数折算——与联合求解 per_solve=run_budget/(ar×2) 同一口
  径，全框架统一的拓扑表习语）。
  【2026-11-02 勘误，PR#384 检视 R3（意见 4440012669）】GEMM 显式档 min
  的全核口径再加 CAVEAT 低估系数 3×（min=3×ar×2×t(knob)，仅
  workers>1）：仅表值口径的 min 裕量只有 433s/264s≈1.6×（mdim=4096 全核
  -t 小值档），而 GEMM_T1024_S 系本机 256-bit quiet 定标——VL=128 档实测
  每迭代慢 1.78×（2026-11-02，-n 1 -O mdim=2048：每内层迭代
  1.757s→3.133s），与 CAVEAT 有效 AR≈15 叠加后 kill 阈值
  5×ar_table=25 被 15×1.78=26.7 打穿（计入 T1024 立方 -9% 低估更甚）
  ——×3 后（min=30×t(knob)）VL=128 叠加最坏仍有 5×15/(15×1.78)≈2.8×
  裕量。workers==1 不放大（CAVEAT 3× 是全核争抢效应，-n 1 单迭代语义
  保持）；desired 维持表值标称口径；SVD 侧不加（其 mem 族表值 25 实测
  低估仅 1.56×=39，按同档因子折算 5×25/(39×1.78)≈1.8× 裕量仍足；SVD
  自身档因子未单独标定，按同代 GEMM 实测折算）。中间 worker 档
  （1<w<全核）min 随之更保守——与意见 4439402800/4440012653 的二值闸门
  裁定一致（保守方向不误杀，插值无标定数据反开 kill 窗）。
  【2026-11-04 勘误，PR#384 检视 R4（意见 4441045425）——SVD 侧裕量
  复算与档因子实测】意见按裁定自身公式独立复算：计入 SVD 幂律拟合
  误差 ±2%（1.02），SVD kill 裕量 = 5×25/(39×1.78×1.02) ≈ 1.77×
  （R3 记 1.8×），打穿需 SVD 自身档因子 ≥ 125/(39×1.02) ≈ 3.14×
  （借用 GEMM 实测 1.78×，意见自认 Minor 不阻塞、系复算确认与风险
  量化）。R4 补实测消除借用不确定性：SVD VL=128 档因子实测
  **1.064×**（2026-11-04，-n 1 -O mdim=2048 单迭代 56.06s→59.66s，
  SDCSHIELD_ARM64_SPEC_MAX_VL=128 原生降档且 dump-cpu-info 确认 spec
  max 128；GEMM 同法对照 4.18s→5.87s 证明降档机制改变执行）——
  BDCSVD 向量化占比低，对 VL 减半远不如 GEMM（1.78×）敏感。代入
  实测因子裕量 5×25/(39×1.064×1.02) ≈ 2.95×；R3 借用 GEMM 1.78 的
  1.8× 本身已是保守上界（AR 与 tier 反相关不叠加，R3 GEMM 实测
  256-bit AR_eff≈11.2 > VL=128 的 5.7-7.1）。维持 SVD 侧不加
  CAVEAT：实测档因子距打穿阈值 3.14× 有 2.95× 裕度，且加 3× 会把
  #4440012653 已裁定的中间档 min 虚抬成本再放大 3 倍（-n 2
  mdim=4400 → min 12220s→36660s）无实测支撑。验证边界如实声明：
  档因子系 -n 1 口径实测（全核口径大 mdim 超本机 61GB 物理内存
  不可运行，与 R3 同构折算限制）。
  【2026-11-04 勘误，PR#384 检视 R7（意见 4464199557）——上块裕量
  算术修正】R4 原记「代入实测因子裕量 ≈3.1×/打穿阈值 ≈3.15×/距
  实测 2.96× 裕度」系算术误差：5×25/(39×1.064×1.02)=125/42.33=
  **2.95**，125/(39×1.02)=125/39.78=**3.14**（上块已按此改写）；
  「裕量」与「打穿阈值/实测档因子」是同一量：margin =
  5×ar_table/(AR_eff×f_tier×1.02)，threshold = 5×ar_table/(AR_eff×
  1.02)，故 margin = threshold/f_tier = 3.14/1.064 = 2.95——此前
  两个名字记了两个不一致的数（3.1 vs 2.96）正是误差来源。同块其余数字（1.77×=
  125/70.8、借用 1.8×=125/69.4）逐项复算正确。2.95× ≫ 1，不翻转
  「维持不加 CAVEAT」结论；该账系再标定基准（调 ar 表值/幂律系数
  须对照重算），数字须精确。
  【2026-11-08 勘误，issue #506——AR 表行 1 重标定 + CAVEAT 退役】
  上方 R3/R4/R7 账目的 ar_table 基准已变：mem 行 1 由 25→40、comp 行 1
  由 5→15（§4.1 表；实测出处见彼处）。连带裁定：(a) SVD 侧裕量重算 =
  5×40/(39×1.064×1.02)=200/42.33=**4.73×**，打穿阈值 =
  200/(39×1.02)=200/39.78=**5.03×**——表值已取实测带上界（39-40），
  低估闭合，维持不加任何额外加权（中间档 min 成本口径不变）；(b) GEMM
  显式档 min 的 CAVEAT ×3 系数（GEMM_AR_CAVEAT_X）退役：表值 15 =
  旧 5×3 数值恒等，min=表值×2×t(knob) 即最坏真实单迭代，账目数字
  不变（VL=128 叠加最坏 5×15/(15×1.78)≈2.8×），构成自 CAVEAT 加权
  改为表值直取；(c) SVD 族联合层 run_budget_ms 140000→200000 回契约
  值（最坏 child 单迭代 ≈195s+init ≈210-240s，宽限内 ~20-25% 裕度），
  sparse 90000→144000 保压重锚（per_solve 1800ms 不变；其 fill-in 指针
  追逐 AR≈50 超族行 40 达 1.25×，保留为文档化负载特例，族行按工作
  负载分档后应回调 200000）。行 2/4/8 无新实测不动。本条与
  svd_cdouble_sve.cpp R6-R8 块、gemm_double_max_arm.cpp R3/R4 块为
  R8 三镜像，已三处同步。
  【2026-11-04 勘误，PR#384 检视 R5（意见 4463650400）——显式档
  desired 的生效侧澄清】显式档同一 iter_s 下 desired(0.95×iter_s)
  恒小于 min（GEMM workers>1 时 min=3×iter_s≈3.16×desired），框架
  test_duration 的 min 夹钳最后执行 → 显式档 target/timeout 恒由
  min 决定，desired 是模型记录值（auto 档不写 min，desired 才是
  生效目标），「-5% margin」仅对 auto 档是真实生效裕量。kill 链经
  min 闭合方向安全（意见自认无功能后果），本条为注释/表述勘误：
  两测试 desired 行注释已改为生效侧澄清，PR 标题「desired_duration
  跟随 mdim 旋钮」的机制注记见 PR 描述 R5 节（preinit 的 desired
  确实跟随旋钮计算，显式档的时长执法经 min 生效）。
  天然修复 exec_each_test 模式
  preinit 不跑导致 g_dim 恒 300 的分歧（LSP 核实修正 #3）；
  **exec 模式下 desired_duration 的说明【v2 修正】：`test_duration()`
  在父进程用 preinit 写入的值（fork 模式有效）；exec 模式子进程无
  preinit 但父进程有 → desired_duration 经 fork 前的 test 结构不随 exec
  传递，exec child 走 `-t`/默认时长（v1 表述"走 -t 兜底"结论不变，
  机理更正为：preinit 在 exec 父进程也运行，仅子进程映像不继承写值）**
  【2026-11-02 勘误，PR#384 检视（意见 4439402775）处置实录】该 v2 修正
  的 kill 推论在当前 head 不成立（执法在 parent 侧）：超时执法是
  wait_for_children 用 **parent** 的 test_timeout(parent 的
  current_test_duration)，parent 的 preinit 已写 min（9a82f53b）→
  parent 的 test_duration 把 -t 夹钳到 min → parent 的等待宽限随 knob
  缩放——exec child 不会被 300s floor 误杀（实测 `-f exec -t
  2000 -n 1 -O mdim=4000`：一次迭代 556.5s 后 exit: pass，无 SIGQUIT，
  loop-count 1）。
  【2026-11-02 再勘误，PR#384 检视 R3（意见 4440012682）——机理更正】
  上述"exec child 走 DefaultTestDuration=1s = 恰好一次迭代"的机理记载
  **错误**：exec child 的迭代目标不是本地默认时长，而是 parent 在
  test_duration()（含 min 夹钳）算出后经 **shmem->current_test_endtime**
  传入的共享时限（sandstone_run.cpp:1913 计算 → :1961 写入 → :1703 才
  spawn；exec child 的 worker 循环 sandstone_run.cpp:633
  `!wallclock_deadline_has_expired(sApp->shmem->current_test_endtime)`
  读该共享值，从不本地调 test_duration()）——**fork/exec 迭代语义完全
  一致**，不存在"恒一次迭代 vs 多迭代"的分歧。实测反例（两层循环
  结构：块=fracture 重 init（内层预算 fracture_loop_count=5），块内
  worker 循环至共享时限/预算先到者）：`-f exec -t 30000 -n 1 -O
  mdim=600` → 7 个块（前 6 块每块 loop-count **5** = fracture 预算
  跑满未被时限截断，末块 loop-count **3** = 30s 共享时限在块内截断，
  共 33 次内层迭代填满 30s）；旧机理（child 本地 1s）预测每块
  loop-count ≈2（内层单迭代实测 0.92-1.1s，1s 时限内至多 2 次）——
  实测 5 证伪。min 夹钳同经共享时限传入：`-f exec -t 2000 -n 1 -O
  mdim=600` → 单块 3.3s / loop-count 3 = 夹钳后 min 3236ms 的口径
  （原始 -t 2000ms 预测 loop-count 2、本地 1s 预测 1），与 fork 同
  语义。因此原提出的"把时长参数并入 exec argv 协议"
  设计项**建立在假前提上，撤回**。残留语义瑕疵如实收窄为：exec child
  自身的 test 结构 desired/minimum_duration 字段恒 0（M_DIM 日志行
  实证）——纯字段语义瑕疵，迭代时长与超时执法均经 shmem/parent 与 fork
  完全一致，无功能后果。
  【2026-11-04 勘误，PR#384 检视 R5（意见 4463650411）——M_DIM/N
  日志的 exec 模式区分】上段"字段恒 0"的日志误导面收口：svd/gemm
  的 M_DIM/N= 日志按 test->desired_duration == 0 判 exec child
  （判据：preinit 成功后两分支 iter_s > 0 → desired 恒 > 0；exec
  child 全新镜像未跑 preinit → 静态初值 0），exec 分支不再打印恒 0
  的两字段，改打印"本进程未跑 preinit, desired/minimum_duration
  恒 0——生效的 -t/min 夹钳经 parent shmem 传入, 非无下限保护"。
  fork 模式日志文本逐字不变。
  【2026-11-04 勘误，PR#384 检视 R4（意见 4441045420）——9b96ab64
  声明边界】"env 前缀从 test->id 推导后行为逐字不变"仅对默认构建
  成立：SANDSTONE_NO_TEST_NAMES 构建（framework_options 含
  no-test-names 或 logging_format=no_output）下 .id = shortid 十六
  进制串（如 "cd14b7"——STRINGIFY(TEST_ID_eigen_svd_cdouble_sve) 经
  sandstone_short_ids.h 展开为 STRINGIFY(cd14b7)），env 名随之从
  SDC_EIGEN_SVD_CDOUBLE_SVE_MDIM 漂移为 SDC_CD14B7_MDIM，已部署旧名
  静默失配。实测（2026-11-04，-Dframework_options=no-test-names）：
  旧名 → "未显式配置"回退 auto 档（11.3s）；新名 → "来源: 环境变量"
  命中显式档（60.7s 单迭代）；GEMM 同构（旧名 34.4s auto / 新名
  SDC_7D59EA_MDIM 6.9s 显式）。定性：(a) 回退的 auto 档有 AR 保护
  （run_budget=140s 收在 300s floor 内），不重开 #352 kill 窗；
  (b) 漂移是框架 test->id 派既有属性（联合求解 T11 块/eigen common
  头 x86 分支/mesh_upi 系同此）；(c) 该构建下 -e/-O 寻址本就只认
  shortid（-e eigen_svd_cdouble_sve 实测 Cannot find，--list-tests
  输出 shortid），env 新名与寻址体系一致，字面量时代的旧名反而
  脱节成两套命名。处置：维持推导（恢复字面量重开 4439402793 的
  同步点；单侧剥 shortid 与全部 test->id 派不一致，均不取），helper
  注释与 spec 如实记录；稳定短名寻址属框架级设计项，超出本 PR
  边界。
  【2026-11-04 勘误，PR#384 检视 R5（意见 4463650392）——哨兵契约
  lo>0 前提显式声明】sdc_knob_or_env_clamped「返回 0 = 未显式配置」
  隐含 **lo > 0**：sdc_knob_int_clamped 对「已配置且合法」的值原样
  返回，lo ≤ 0 时用户显式 -O <test>.<key>=0 会以合法值 0 命中
  knob==0 判定 → 被当未配置走 env 兜底（env 已设则 -O/env 优先级
  倒置，未设则显式 0 被静默丢弃当 auto）。当前四调用方（gemm/svd
  各 preinit+child）lo = MDIM_MIN(1024/300) 安全；helper 契约注释已
  补声明，值域天然含 0 的旋钮（slots/count 类）不得经由本函数。
  【2026-11-04 补全，PR#384 检视 R8（意见 4464388834）——hi+1 端
  别名】哨兵探测的对称缺口：sdc_knob_int_clamped 以 sentinel=hi+1
  探测「是否显式配置」，用户显式配置值恰为 hi+1 时（-O …mdim=6001，
  MDIM_MAX=6000；gemm 同构 4097/4096）被当未配置走 env 兜底——与
  lo≤0 端同型失效（优先级倒置/静默降级 auto）。实测（2026-11-04）：
  6001 → 「未显式配置」回退 auto 档；对照 9999 → clamp 6000 并告警
  ——全值域唯独 hi+1 一个值被静默吞掉；回退的 auto 档有 AR 保护，
  不重开 #352。契约补全：调用方旋钮值域须排除 {0, hi+1} 两个别名
  值；双向哨兵（独立 bool 出参等）属框架级接口变更，超出本 PR
  边界。
- **gemm_double_max_arm**：同样迁移 + 接入时间校准（【v2/M8】它同样
  无校准，v1 漏列）。

### 4.6 P2a/P2b
- **P2a bad_alloc 降级【v2/m2 重设计，对齐先例 + 消除竞态】**：
  - **init 线程**（单线程，无竞态）：分配失败 → `log_warning` 降
    mdim=lo 重分配一次 → 仍失败 `log_skip(资源性)` 返回 EXIT_SKIP。
  - **worker 期**：`sandstone_eigen_common.h`/GEMM 模板 run() 包
    try/catch → `log_skip(RuntimeSkipCategory, "memory …")` 线程级
    EXIT_SKIP——**对齐 eigen_sparse 先例（bad_alloc→SKIP）**，语义：
    资源性失败不是 SDC，不该打红。v1 的"降 mdim 重试 + FAIL"撤销
    （worker 改共享 d->dim 有竞态，FAIL 语义与先例相反）。
- **P2b -O 警告**：`sdc_knob_int_clamped` 显式命中分支（或联合求解外层）
  计算预期合计内存并打 `[欠压力]/[内存越界]`（沿用现有日志格式）。

### 4.7 P3
- 删 svd_cdouble_sve.cpp:109-145 与 gemm_double_max_arm.cpp choose_dim 手写
  sizing（迁移后自然消失）。
- **dynamic_square 家族**（gemm_*_dynamic_square*.cpp）迁移时清理：
  init 共享矩阵保留但计入 shared 项、worker 侧 `gemm_mdim` 重复计算
  改读 init 缓存。【v2/M8 指称修正】
- 卫生：tests/cpu/meson.build:449-465 注释更新（tests_sve per-tier 现实）。

## 5. x86-64 非回归（rule 5）

新逻辑全部 `#if defined(__aarch64__) && defined(__linux__)` 门控：
sdc_numa.{h,cpp}、sdc_read_vm_hwm、联合求解的 arm64 分支、反馈写入/读取
调用点。`PerThreadData::Main` 新字段本身无 arch 依赖且在 x86 也编译
（避免布局分叉），但 x86 无写入者 → 恒为零值 → 无行为影响（注释说明）。
x86 走原 sdc_auto_square_dim 路径不变。CI x86 构建 + 代码检视验证。

## 6. 验证矩阵（rule 2，【v2/M7/C2/q3】修订）

| 验证项 | 方法 | 通过标准 |
|---|---|---|
| P0 修复 | 本机 `-e eigen_svd_cdouble_sve -t 5000`、`-e eigen_svd_bidiag_sve -t 5000`、`-e eigen_gemm_double_max_arm -t 5000` | `exit: pass`；其余 16 个默认集用例非回归 pass |
| P1 提升（分族） | 21 用例全核 auto | GEMM/dynamic_square 峰值 ≥ 原 2×；SVD/sve/bidiag 峰值 ≥ 原 80% 且 pass；全部 ≤ 预算上界（无 OOM） |
| VmHWM 闭环 | **GEMM 类长 -t 300000**（迭代快、fracture≥3，验证写+读收敛）+ **小 -O mdim 单元验证写侧**（SVD 类默认时序不触发，如实声明） | 第 2 fracture 起 mdim 方向正确，eff_ratio 收敛 ±15% |
| per-node 退化 | 本机 node0-only | 行为与全局模型对照一致（数值 diff） |
| 显式 -O | 16/32/48GiB 档抽测 4 点 | pass + 警告出现 |
| quick 模式 | `--quick -e eigen_svd_double` 等 | mdim=lo，单迭代秒级 |
| no_fork | `-f no -e eigen_gemm_double_dynamic_square -t 5000`（PR#302 检视 #8 勘误: `-f none` 是无效值→EX_USAGE；执行用 `-f no`，日志确认 no_fork 模式） | pass，且无反馈写入（VmHWM 不污染） |
| 回归 | `-e zstd19 -t 2000 -n 1` | `exit: pass` |
| SVE 512/1024 档 | gem5-fi（CHAOS/SE，same-source）。**【v2/q3 收窄】SE 无 sysfs NUMA/meminfo → 实际验证面 = fallback 档（探测失败→最差 AR）+ 联合求解正确性**，NUMA 感知逻辑本身不在 gem5 覆盖内 | 各 tier checksum + `SDCSHIELD_FAILS=0` |
| x86 非回归 | CI x86 构建 + 检视 | 零新警告，行为不变 |
| 真多节点 per-node | 诚实边界 | 本机不可验；gem5-fi FS 多节点若可配置则补，否则记录 |

## 7. commit 切分（rule 1，依赖序）

1. `sdc_numa_probe()` + 二维 AR 表（新文件，独立可测）
2. `sdc_budget_bytes()`：cgroup v1/v2 + 节点合成 + **退化规则**
3. `sdc_joint_dim_slice()` + 三点拟合/探针阈值 + RSS_RATIO_TABLE（含口径注释）
4. bidiag 接入（P0-2）
5. svd_cdouble_sve 迁移 + 模型接入（P0-1 + per-node + exec 分歧修复）
6. gemm_double_max_arm 迁移 + 时间校准接入
7. VmHWM 反馈闭环（sdc_read_vm_hwm + Main 字段 + static_assert 更新 +
   裸指针访问器 + no_fork 门控 + median 聚合 + 生命周期语义）
8. dynamic_square 家族迁移（init 矩阵计 shared、gemm_mdim 改缓存）
9. bad_alloc 降级（init 降 mdim + worker SKIP，对齐 sparse 先例）
10. -O 警告
11. meson 注释卫生
12. 文档总 sync（CLAUDE.md/README/docs，rule 7）

每个 commit 前置：build 零新警告 + 本机受影响用例实测 + zstd19 回归。
commit 4-6 阶段 VmHWM 反馈未实现 → c 走静态表（优先级链退化成立，
【v2/m8】P0 稳健性此时完全押注静态表 + AR 假设——正是 §6 前两行先行
验证的原因）。

## 8. 风险与缓解

- MemAvailable 波动（他进程）→ 每次 fracture 重读 + 0.9 系数（PR#302 检视 #21 口径澄清: 重读的是**全局** MemAvailable/cgroup——sdc_budget_bytes 每次调用现读；节点分摊比例 MemFree/ΣMemFree 是探测期快照(magic static)，聚合预算仍逐次新鲜、仅多节点机上的节点间分摊为慢漂移近似——单内存节点精确还原全局，多节点漂移已记录为边界（fallback 节点选择也依赖快照，中途漂移会破坏 C1 归属一致性，故不逐次重探））。
- eff_ratio 震荡 → EMA α=0.5 + ±15% 死区 + c 下限（静态表 70%）。
- 反馈槽读到陈旧值（同 test 重跑不同配置）→ test_id_hash + samples 计数。
- AR 表未标定档（2/4/≥8 节点）→ 宁高估（保 P0 牺牲 P1），TODO(实测)。
- 固定开销污染 → 探针阈值 100ms / 三点拟合（§4.3），M3 数学已验证。
- Main 布局变化 → static_assert 同 commit 更新；全内自洽。
- SVE 下放后 init 时序 → tier 仍由 spec_get 决定；VL 一致性由框架
  worker pin 既有机制保证。
- 时间预算刀锋 → 200s 预算 + AR 上界 + 验证矩阵先行实测。

## 9. 已知边界（诚实声明）

- AR 表的 2/4/≥8 节点档为保守估计，未实测（无真多节点内存机器）。
- per-node 预算的真多节点行为本机不可验证（node0-only 退化路径可验证）。
- **反馈闭环对 SVD 类默认时序不生效**（单 fracture），其 c 修正依赖静态
  表；反馈只收成功样本（超时样本丢失）→ c 偏乐观方向。
- exec_each_test 模式下子进程不继承 preinit 写入的 desired_duration，
  走 `-t`/默认时长。
- SVE 512/1024 档验证依赖 gem5-fi SE，**其实际验证面为 fallback 档**
  （SE 无 sysfs NUMA/meminfo），NUMA 感知逻辑不在其覆盖内。
- no_fork 模式不参与反馈闭环。

# 0103 瞬态电压骤降（droop/di/dt）激发实验 — 设计文档

日期：2026-10-27
状态：待用户批准
作者：会话产物（brainstorming），决策均为用户确认项或文中标注的作者默认

## 1. 意图与动机（why）

用户命题：SDC 的激发机理，除对微架构端口与边界做耦合饱和打击外，还有
"稳态满载 → 瞬态供电谐振"的物理激发路径——例如按 50-150MHz 的节奏制造剧烈的
瞬态电压骤降（dv/dt），压缩时序裕量、诱发边际缺陷翻错。

本实验在 0103 上验证该命题的可操作版本：**以 sdcshield eigen 类用例提供稳态
满载基线，同时用周期级负载调制器构造瞬态电流摆动（di/dt → 电压骤降），探索
最大程度 dv/dt 的参数配置，并用 BMC 实时遥测（电压/电流/功耗/温度）做反馈
验证与护栏，同时狩猎 SDC 功能证据。**

理论事实核查（2026-10-27，Explore 全文检索）："50-150MHz / 谐振"表述在仓库
零命中，属于**仓库外假设，本实验即其检验**。仓库现有理论是 Ldi/dt → 局部
电压跌落作为"物理诱发因素/放大镜"（`docs/cpu/cpu-sdc.md:29`、
`docs/hypothesis/ARM64-SDC-uArch.md:55`、CORE179 案例 §18 把 di/dt 定位为
物理诱发因素而非根因）。因此扫频范围不局限于 50-150MHz，覆盖 0.1Hz-200MHz
（低频档用于仪器标定），让功能证据自己指出敏感频段。

## 2. 用户已定决策

| # | 决策 | 内容 |
| --- | --- | --- |
| D1 | 目标机器 | **仅在 0103（本机）验证**，不触碰其他单板（用户指令 2026-10-27） |
| D2 | 架构 | **方案 A：独立周期级调制器 + 原生 sdcshield eigen 分核并行，框架零改动**（用户在 A/B/两步走中拍板 A） |
| D3 | 反馈通道 | BMC in-band ipmitool（root@127.0.0.1），~3s 采样节奏；测量边界如实声明（见 §8，用户批准设计时已确认） |
| D4 | SDC 命中处置 | 发现 SDC 疑似事件 → **停机取证优先**，证据链保全后由用户裁定是否继续（作者默认，随设计获批） |
| D5 | 实现形态 | **独立工具，与 sdcshield 解耦**，存于 `tools/` 下新建目录 `tools/droop-excite/`（随仓库维护、评审、版本化）。解耦语义：仅通过 CLI 调用 `builddir/sdcshield` 二进制，零构建/代码依赖；无 sdcshield 时调制器/记录器/分析器仍可独立运行。仅实验原始数据留仓库外（用户指令 2026-10-27） |

## 3. 事实基础（全部已核实，2026-10-27）

**0103 本机**
- TaiShan 2280，128 核（双 socket ×64，BMC 双 CPU 遥测证实），无 SVE（NEON 路线）。
- cpufreq 存在：policy0-3 共 4 个域，全部 `performance`，2.6GHz
  （cpuinfo 200000-2600000 kHz，实测 `scaling_cur_freq`=2600000）→ 频率确定性
  良好，无 governor 切换噪声。policy↔核的精确映射留实施第 0 步探测。
- 本地构建在 `builddir/sdcshield`；14 个 PROD eigen 用例可跑（无 SVE 门控的
  全部：`eigen_svd_double`、`eigen_svd_double2`、`eigen_sparse`、
  `eigen_gemm_double_dynamic_square`(+dump)、`eigen_gemm_cdouble_dynamic_square`(+dump)、
  `eigen_gemm_float_dynamic_square`(+dump)、`eigen_gemm_double14`、`eigen_svd`、
  `eigen_svd_cdouble`、`eigen_svd_cdouble_noavx512`、`eigen_svd_fvectors`）。
  eigen 满核跑法已于 2026-10-24 修复（feat/eigen-svd-multithread），全核安全。
- 激发战役主驱动未运行（load ~0.2，近闲置）；旧仓库路径的 systemd 采集中
  （sdc-monitor 60s 周期等）**只读共存、不重启不修改**，其 monitor.csv 可作旁证。
- 0103 即本会话宿主机：最坏情况（掉电/复位）会杀会话 → 全程落盘续跑纪律（§9）。

**BMC 遥测（iBMC 6.70，in-band 实测）**

| 传感器 | 实测值 | 硬限 |
| --- | --- | --- |
| CPU1/CPU2 N_VDDAVS | 0.910 / 0.890 V | 下限 0.770，上限 0.990 |
| CPU1/CPU2 VDDAVS | 0.890 / 0.890 V | 下限 0.730 |
| Power / Power2（双 PSU） | 270 / 264 W | — |
| PS2 IOut | 19.98 A | — |
| CPU1/CPU2 Core Rem 温度 | 63 / 45 ℃ | 临界 105 ℃ |

DCMI power reading 无返回（不可用）。轮询节奏保持 ≥3s 间隔（技能实测 <3s 会
把 BMC 采样打超时）。

**先例（正对照）**：战役监控曾捕获满载电压跌落 **-0.02V**
（`docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md:669`）→ BMC 电压
通道具备 DC 压降检出能力。

**现有资产**
- `tests/cpu/arm64/power_virus_dit.cpp:79-82`：BURST/STALL=2048/2048 次迭代
  → 实际调制仅数百 kHz，比目标频段低 2-3 个数量级；但三项技术直接复用：
  依赖 NEON FMA 链背靠背、高汉明距离操作数交替（最大翻转活动）、`yield`
  停顿；其 bit-exact 软件参考校验模式也复用。
- 周期级调制器/扫频器/BMC 反馈闭环在仓库中**不存在**（`tools/excite/sdc_axes.py`
  的 load_shaping 是秒级 burst/duty 库模块且未接入战役）——需新建。
- 框架 CPU 枚举走 `sched_getaffinity`（`framework/sysdeps/linux/cpu_affinity.cpp:24-43`）
  → `taskset`/`--cpuset` 分核可靠；`-n` 只控线程数，与 `--cpuset` 不宜混用。

**频率↔相位换算（2.6GHz）**：50MHz = 52 周期/期（26/相位）；100MHz = 26（13）；
150MHz = 17.3（8/9）；200MHz = 13（6/7）；理论 上限 325MHz（最小相位 4 周期）。
低频档（0.1-1Hz = ~10⁹-10¹⁰ 周期/相位）走同一 cycle-count 代码路径。

## 4. 目标 / 非目标 / 反目标

**目标（可验收）**
1. 交付参数化周期级调制器 `droop_mod`：相位 4~2⁶¹ 周期可扫（理论 0.1Hz-325MHz，
   扫频主区间 1kHz-200MHz）、占空比、载荷、断电流模式、核数/放置、重同步
   全部参数化，自带 FMA 校验与 CNTVCT 频率自校准。
2. 仪器标定完成（P1）：低频调制在 BMC 遥测中**电平分明可见**（min/max 包络
   提取，非波形重建），产出每配置 ΔI 摆幅表与载荷裁决表；满载 DC 压降复现
   （或如实更正）。
3. 目标频段扫频完成（P3，≥30 频点）：产出分层排序表（低频 ΔI 标定 + DC 压降
   + 功能事件），给出"最大 dv/dt 参数配置"结论及其证据强度。
4. 最优配置强化驻留（P4，≥30min）：命中 SDC → 证据链保全 + 停机取证（D4）；
   未命中 → 阴性结果如实记录。
5. 实验报告含**测量边界**一节：BMC 无法测 50-150MHz 纹波幅度（差 7 个数量级），
   判据为代理指标 + 功能证据；示波器实测列为后续实验室工作。
6. 代码入仓库：feature 分支、一补丁一单元、DCO、推送；文档同步（CLAUDE.md 规则 1/3/7）。

**非目标**
- 不修改 sdcshield 框架与任何现有测试（含 `power_virus_dit` 本身）。
- 不做框架内门控 eigen 测试（方案 B，待本实验产出后再立项，见 §13）。
- 不做示波器/VRD 测点实测（无远程能力）。
- 不在其他单板验证（D1）；不重启/修复 0103 旧路径 systemd 单元与 `status.sh`
  的观测缺口。
- 不追求绝对无界"最大"——护栏（§9）界定的安全边界内的最大。

**反目标（做了就算失败）**
- 把"BMC 看不到高频纹波"隐瞒或含糊为"已验证电压骤降"——代理指标与直接
  测量必须分清（诚实性铁律）。
- 仪器没标定就宣布参数最优（P1 是 P3 结论的前置）。
- 无界实验把 0103 打挂且证据未落盘。

## 5. 架构（方案 A）

```
                0103 (128核, 2.6GHz, 4 cpufreq域, 双socket, 无SVE)
  ┌────────────────────────────────────────────────────────────┐
  │  核心 0..K-1         核心 K..K+M-1        最后 8 核          │
  │  eigen 稳态满载       droop_mod 调制器      OS/驱动/记录/会话  │
  │  (taskset + sdcshield (周期级门控:           (隔离保留)       │
  │   -e ... -T -t 60)    FMA突发↔停顿交替)                     │
  └────────────────────────────────────────────────────────────┘
   ▲ 进程/日志/落盘                    ▲
   run_sweep.py (root) ──→ /home/sdc/droop-excite/<run-id>/steps/<idx>/…
   ▲                                       ▲
   bmc_logger.py (~3s: N_VDDAVS×2, VDDAVS×2, 双PSU功率, PS2 IOut,
                 CPU温度×2, VRD温度 → CSV)  ← 反馈验证 + 护栏数据源
```

- eigen 用 `taskset -c <K cores>` + `-e eigen_svd_double,eigen_sparse,eigen_gemm_double_dynamic_square
  -t 60 -T <步长> --strict-runtime` 在框架内轮转（每用例 60s 量子，`-T` 预算内循环）。
- 调制器与 eigen 分核互斥；默认保留最后 8 核给 OS/驱动/记录/本会话。
- 代码位置：`tools/droop-excite/`（`tools/` 下新目录，与 `tools/excite/` 平级；与
  sdcshield 解耦——只经 CLI 调用其二进制，零构建/代码依赖）；实验数据落
  `/home/sdc/droop-excite/<run-id>/`（仓库外）。

## 6. 组件设计

### 6.1 `droop_mod.c`（调制器，C17 + aarch64 NEON，板上 gcc 直编）
- 参数：`--on-cycles N --off-cycles N`（u64，4 ~ 2⁶¹，周期级门控的唯一真相源；
  驱动按目标频率换算）；`--cores <list>`（sched_setaffinity）；`--duration <s>`；
  `--payload dep|ilp2`（dep = power_virus_dit 式依赖 FMA 链 + 高汉明操作数交替；
  ilp2 = 独立流饱和双 NEON 管——谁电流更大由 P1 实测裁决）；
  `--off-mode stall|light`（yield 停顿 / GPR 依赖加法链 = 部分深度）；
  `--resync N`（每 N 个门控周期自旋票据 barrier 重同步，0=关，默认开 1024）。
- 门控循环内**无任何系统调用/计时器**——纯指令计数，相位锁定核时钟。
- 校验：每 burst 的 FMA 累加器与 bit-exact 软件参考比对（复用 power_virus_dit
  模式）；失配 → stderr 报告 + exit 2（= 疑似 SDC 证据，D4 流程）。
- `--calibrate`：CNTVCT 实测达成频率（~100ms 窗），每步记录进 config 快照。
- SCHED_FIFO RT 优先级（失败自动降级普通调度）；SIGTERM 有界退出并落盘摘要。

### 6.2 `bmc_logger.py`
- in-band `ipmitool sensor list` 全量读一次 ~2-3s → 解析目标传感器 → 追加 CSV。
  节奏 = max(3s, 上次耗时+0.5s)。列：ts, cpu1_nvddavs, cpu2_nvddavs,
  cpu1_vddavs, cpu2_vddavs, power, power2, ps2_iout, cpu1_t, cpu2_t, vrd_t。
- root 运行；与既有 sdc_monitor（60s）共存无冲突。

### 6.3 `run_sweep.py`（战役驱动，root）
- 步骤协议：idle 基线采样 15s → 启动 bmc_logger（后台）+ eigen + 调制器 →
  步长驻留 → 全部停止 → post 采样 15s → 汇总该步 summary.json。
- 每步落盘：config.json（含校准频率）、bmc.csv、mod.log、eigen.log、
  dmesg/journalctl RAS 差分、SEL 差分。
- 护栏评估在采样回调里（§9 阈值），触发即 kill 当步并标记 aborted。
- 心跳文件：驱动每 10s 更新 heartbeat；会话死亡后可凭落盘结果续跑/复盘。

### 6.4 `analyze.py`
- 指标：ΔI_p2p = 步内 PSU 功率/电流序列 max-min（低频档）；V_sag = 空载参考
  均值 − 步内 N_VDDAVS min；事件 = eigen fail / modulator exit 2 / RAS / SEL /
  护栏触发。
- 产出分层排序表（不搞合成玄学分）：按 ΔI 标定、V_sag、事件三列分别排序。
- （可选，P2+）`canary.c`：独立核跑固定指令序列 + CNTVCT 时间戳，离线
  periodogram 找 f_mod 谱线。服务器核固定 PLL 时钟下可能无信号——无信号 =
  记录在案的阴性结果，不是失败。标注探索性，不阻塞主线。

## 7. 战役阶段与参数网格

| 阶段 | 内容 | 规模 |
| --- | --- | --- |
| 第 0 步 | 拓扑探测：sysfs topology + cpufreq policy↔核映射 + socket 边界，固化放置轴的核心列表 | 分钟级 |
| P0 冒烟（零新代码） | co-run 现成 `power_virus_dit`（数百 kHz 一个点）+ eigen：验证分核拓扑、BMC 可见性、满载热轨迹与功率基线（据此收紧功率护栏） | ~15min |
| P1 低频标定 | 0.1/0.25/0.5/1 Hz × duty {25,50,75}% × payload {dep,ilp2}；另跑 100% duty 各 payload 的满载功率对比（载荷裁决）。BMC 看见电平分明的高低切换（min/max 包络）→ 每配置 ΔI 表 | ~1.5h |
| P2 中频过渡 | 1k/10k/100k/1M/10M Hz × duty 50%：平均功耗随 duty 的变化符合理论（门控未崩的证据）；金丝雀可选上线 | ~1h |
| P3 目标频段扫频 | 1/2/5/10/20/50/75/100/125/150/175/200 MHz（50-150 密集）× duty {30,50,70} × 最优 payload；每点 90-120s | ~3h |
| P3b M/放置扫描 | 最优 f/duty 上：M ∈ {8,16,32,64} × 放置 {同socket,跨socket}（eigen = 120-M 核互补；精确核心列表由第 0 步拓扑探测固化） | ~40min |
| P4 最优强化 | 综合最优配置驻留 30-60min：eigen 全家族 14 用例轮转 + 最强调制 → SDC 狩猎窗口；可重复 | ~1h |

总时长 ~7-8h（含间隔），跨会话续跑（heartbeat + 落盘）。

## 8. 反馈验证链路与测量边界（如实）

**BMC 能验证**：
1. 满载 DC 压降（正对照：-0.02V 先例；P1 复现）。
2. 低频调制（0.1-1Hz；min/max 电平差提取，非波形重建）的功率/电流/电压
   电平差与 ΔI——仪器标定 + 跨配置可比的代理判据。
3. 温度/功率护栏。

**BMC 不能验证**：50-150MHz 纹波幅度（采样 0.3Hz vs 目标 5×10⁷Hz，差 7 个
数量级）。需示波器探 VRD 测点，超出远程能力——**结论中如实声明**，建议为后续
实验室工作。

**高频段证据改走**：功能层（eigen golden SDC / 调制器自身校验 / RAS / 稳定性）
+ 可选周期时间金丝雀（探索性）。

**"最大 dv/dt 配置"的可测判据**（分层呈现，不合成单一分数）：
低频 ΔI 摆幅标定（代理）+ DC 压降深度 + 功能事件。

**不可远程测量而如实声明的**：多核相位相干度（重同步参数只是缓解，效果不可
直接观测）。

## 9. 护栏与安全

| 护栏 | 阈值 | 动作 |
| --- | --- | --- |
| CPU Core Rem 温度 | ≥95 ℃（BMC 临界 105 ℃） | kill 当步 + 标记 |
| N_VDDAVS | ≤0.80 V（BMC 下限 0.770） | 同上 |
| 双 PSU 总功率 | ≥1300 W（P0 实测满载基线后收紧） | 同上 |
| SEL / RAS | 新致命事件 | 停当步 + 取证 |
| SDC 疑似事件 | eigen fail / modulator exit 2 | **停机取证**（D4），证据链：results 目录 + dmesg/journal 快照 + SEL elist + 配置快照 |
| 总量 | 每步有界；逐级爬升（P0→P4） | — |

会话宿主风险：最坏掉电/复位杀本会话 → 计划/进展/结果全程落盘（本 spec、
实施计划、`docs/research/progress-log.md`、results 目录），可续跑可复盘。
OS 死亡后的 BMC 带外恢复需经 0101 作纯网络中继（9.3.x 管理网）——**届时
单独请示用户**，不预承诺、不把它当作实验依赖。

## 10. 交付物

1. 代码（`tools/droop-excite/`，独立解耦工具）：`droop_mod.c`、`bmc_logger.py`、
   `run_sweep.py`、`analyze.py`、`README.md`（构建/运行/参数说明）；
   可选 `canary.c`。feature 分支 `feat/droop-excite-2026-10-27`，一补丁一单元，
   DCO，逐个验证后推送。
2. 实验报告：`docs/sdc-excite-reproduce/droop-excitation-experiment.md`——
   参数排序表、事件记录、测量边界声明、后续建议。
3. 原始数据：`/home/sdc/droop-excite/<run-id>/`（不入库，报告引用其摘要）。

## 11. 自验证与合规（CLAUDE.md 规则适配）

- 规则 2：(a) `droop_mod.c` gcc `-Wall -Wextra -Werror -std=c17` 零警告；
  (b) 每个行为（校准频率、门控占空比、低频方波在 BMC 的可见性、护栏触发）
  引用真实运行输出；(c) 框架回归：改完后跑 `-e zstd19 -t 2000 -n 1` →
  `exit: pass`（框架虽零改动，仪式照走）；(d) x86 非回归：新独立目录 + 调制器
  为 aarch64 专用（`#else` 分支报错退出），x86 构建面不受影响，靠检查确认。
- gem5-fi 条款适用性：本任务不改框架/测试代码，全部行为在真实 0103 上可直接
  运行验证，无 HWCAP 类不可运行路径 → **不适用**。后续若立项方案 B（框架内
  门控测试），按该条款重新评估。
- 规则 7：本 spec + 实施计划 + 实验报告 + README 同步。

## 12. 风险与开放问题

| 风险 | 缓解 |
| --- | --- |
| 掉电/复位杀会话（会话宿主=实验机） | 落盘续跑纪律；实验有界；逐级爬升 |
| 多核相位漂移（中断抖动） | `--resync` 周期重同步；相干度不可测，如实声明 |
| 50-150MHz 假设不成立 | 扫频覆盖 0.1Hz-200MHz，功能证据定敏感频段 |
| 高频门控被流水线/微架构"抹平"（电流摆动小于预期） | P1 低频 ΔI 标定给出每核摆幅上限预期；P2 中频验证门控未崩 |
| BMC 采样超时 | ≥3s 节奏 + 单次全量读 |
| 与旧路径 sdc-monitor 抢 BMC | 只读共存，60s vs 3s 无冲突；如遇超时自动退避 |

无开放问题（拓扑精确映射是实施第 0 步的计划内动作，不是设计未知数）。

## 13. 后续路线（范围外，记录不动工）

本实验（方案 A）产出最优参数后，若用户立项：把该配置固化为框架内门控 eigen
测试 / 战役常驻载荷（方案 B），走完整框架改动流程（TDD/评审/x86 非回归/
gem5-fi 评估/文档同步）；以及实验室示波器实测 VRD 纹波。

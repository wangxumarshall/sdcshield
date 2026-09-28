# Hardware Sentinel: Protecting Software Applications from Hardware Silent Data Corruptions(ASPLOS 2025,Meta Platforms Inc.)

> 来源 PDF: docs/paper/ref/ASPLOS 2025 Hardware Sentinel Protecting Software Applications from
> Hardware Silent Data Corruptions.pdf(16 页,正文 13 页 + 参考文献 3 页)。
> 作者:Rhea Dutta, Harish Dattatraya Dixit, Rik Van Riel, Gautham Vunnam, Sriram Sankar(全部 Meta)。
> 所有数值均标注原文图表号;读不到的写"原文未给出"。

## 1. 研究问题与核心贡献(≤5 行)

- 问题:传统 SDC 检测靠硬件测试(Fleetscanner 出生产测试 / Ripple 在生产测试),
  依赖厂商私有工具、受测试预算与状态空间覆盖限制,对新型 SDC 签名诊断能力不足。
- 核心思路:**自顶向下**——不测硬件,而是从应用层失败信号(segfault、core dump、
  应用崩溃、内核异常日志)反向归因 SDC;完全 vendor-agnostic,只用 OS/应用遥测。
- 贡献:首个应用级 SDC 检测框架(数据平面 6 类数据源 + 控制平面阈值规则);
  覆盖 7 个 CPU 代(正文 5.2 节说 8 个 SC-1..SC-8,摘要写 7——原文不一致)、
  13 类负载、27 个数据中心区域、6 年失败数据;识别出与 SDC 强相关的罕见内核异常类型。
- 关键数字:相对 Fleetscanner 覆盖 +1.74×、相对 Ripple +1.92×、相对两者合并 +1.41×(41%);
  第三方失效分析复现率 70%(行业常态 15%–30%);内核 rollout 异常 -21%。
- 注意:**该文不做 ABFT/指令复制类的对比**——它的对比基线只有 Fleetscanner 与 Ripple
  (Meta 自有硬件测试机制),与 ABFT/复制法的对比轴需从 ITHICA/SEVI 等论文借用。

## 2. 实验方法(插桩/检测机制、输入、负载)

### 2.1 检测机制(§4,数据平面 + 控制平面)

**不是插桩,是日志挖掘 + 规则判定**。无任何代码插桩、无硬件专用信息。

**数据平面(6 类数据源,§4.1)**:
1. **Kernel Logs**:把非结构化内核函数日志重构成标准化 schema(原文 Fig. 2
   "Structured Log Format")——所有内核异常(panic、lockup、GPF、MCE、divide error、
   stack corruption)按 schema 分类,记录失败负载的异常例行程序与执行频率、
   **发生异常的 socket/core 编号**。
2. **Reboot Database**:全集群重启历史(可配置回看窗口);计划外频繁重启 +
   无伴随故障遥测 = SDC 候选启发式。
3. **System Failure Log Database(SEL)**:BMC 带外记录的 System Event Log
   (电源事件、硬件错误、环境传感器);**只有 SEL 中 CPU 相关硬件错误条目为 0 的服务器
   才进入 SDC 流水线**——若 SEL 有 CPU 故障则是"响"的故障,不是"静默"的。
4. **SDC Test Result Databases**:Fleetscanner/Ripple 的历史检出记录(长保留期),
   用作基线与"应用异常但测试从未检出"的证据。
5. **Repair Database**:维修工单历史;分类为正确诊断/误诊/未诊断;
   无维修史但异常多、或反复误诊/未诊断的服务器是 SDC 优质候选。
6. **Core Concentration Metadata(核心集中度)**:异常按 socket/core 聚合;
   **多负载在同一核(或 sibling 核)失败 = 硬件问题强信号**;
   单一负载在大批服务器上同 backtrace 失败 = 软件问题。

**控制平面(§4.2,全部阈值)**:
- **Reboot 参数**:回看窗口 30 天;常规集群阈值 **30 天内 ≥6 次重启**;
  AI 集群(训练中断代价大)**30 天内 ≥3 次**。
- **SEL 参数**:应用异常时间戳附近检索内存 ECC/MCE/PCIe/热/传感器条目;
  要求 **CPU 故障相关 SEL 条目为 0**(有则排除,走普通硬件修复流程)。
- **Core Concentration 参数**:异常回看窗口 **1 周**;**单核异常占比 ≥60%** 标记可疑;
  **sibling(超线程)核合并统计**(分开统计单核检出率更低,合并后更高——
  因为超线程共享物理硬件);**应用分布阈值 = 2**(至少 2 个不同应用在同一核/
  合并 sibling 核上失败才触发 60% 规则;单应用多服务器失败视为低置信软件问题,
  但持续增长的单应用失败可标记为"独特 SDC 诱发负载",罕见)。
- **Top N Selection**:按异常数排序取前 N 候选(N 可配置,按负载调优;
  反复出现在 Top N = 流程覆盖缺口信号)。
- **Repair 参数**:30 天回看窗口判定维修复发(recidivism)归因 SDC。
- **SDC 测试库参数**:Ripple/Fleetscanner 检出的服务器作为基线;
  HWSentinel 检出但两者皆漏的服务器 = 硬件测试改进空间。
- 判定输出(原文 Fig. 4 "reference output table"):**faulty CPU 或 software anomaly** 二分。

**正例流程(§4.3.1 实例)**:一台 Top N 选出的主机 30 天 7 次重启(超 6 次阈值)
→ 初步怀疑 CPU → 但异常均匀分布到所有核(不满足 60% 集中度)→ 判为软件异常,排除。

**假阳性教训(§4.4)**:早期按"崩溃率高于他人的应用"找主机,结果把
**故意崩溃系统的 fuzzer 负载**标成 Faulty CPU → 修正:**fuzzer 类负载从分析中排除**。

### 2.2 对比指标(§5.1)

- **相对检出率 RDR = HWSentinelDetectionRate / (FleetscannerDetectionRate + RippleDetectionRate)**;
  其中 DetectionRate = DetectedFaultyCPUs / TotalCPUsScreened。
- **相对异常类型比 = HWSentinelExceptionDetectionRate / FleetwideExceptionDetectionRate**。

### 2.3 输入与负载

- 输入是**6 年集群失败遥测数据**(摘要),非主动注入。
- 13 类负载,四大族(§5.4):存储与数据库、实时应用、开发者与运维、AI 与核心应用。
  命名的负载类别:Infra Maintenance、AppTest、Dev、Backup、Storage、Edge、Gaming、
  Source Control、Synchronization 等(原文 Fig. 12 有 13 类,图中文字未在文本层完整给出)。
- 27 个数据中心区域(§5.5);CPU 代 SC-1..SC-8(2012–2023 制造,多家厂商)。

## 3. 实验配置(硬件/软件/参数)

- **硬件**:Meta 数百万台服务器集群整体;7(摘要)/8(§5.2)个 CPU 代(SC-1..SC-8),
  2012–2023 年制造,多半导体厂商——**具体型号原文未给出**(匿名化)。
- **软件**:Linux 内核多版本并存;BMC/OpenBMC 带外遥测;容器或原生部署;
  内核灰度发布流程(§5.6)。具体内核版本、数据库技术栈细节原文未给出。
- **关键参数汇总**(复现需要的全部数值):
  | 参数 | 值 | 出处 |
  |---|---|---|
  | 重启回看窗口 | 30 天 | §4.2.1 |
  | 重启阈值(常规/AI) | 6 次 / 3 次(30 天) | §4.2.1 |
  | 异常回看窗口 | 1 周 | §4.2.3 |
  | 单核异常集中度阈值 | 60% | §4.2.3 |
  | 应用分布阈值 | ≥2 个应用 | §4.2.3 |
  | 维修复发窗口 | 30 天 | §4.2.5 |
  | sibling 核 | 合并统计(超线程共享物理硬件) | §4.2.3 |
  | SEL 过滤 | CPU 故障条目必须为 0 | §4.2.2 |
- **无主动故障注入**;唯一"金标准"是 5.7 节的第三方失效分析。

## 4. 实验步骤(可操作流程,编号)

1. **建立数据平面**:接入内核日志(重构为结构化 schema,含 socket/core 号、
   应用名、backtrace、异常类型);接入重启库、SEL/BMC 日志、Fleetscanner/Ripple
   检出库、维修工单库。
2. **构造核心集中度元数据**:按服务器聚合每周异常,计算每核(合并 sibling)分布;
   标记多负载同核失败。
3. **控制平面规则过滤**:(a) SEL 中 CPU 故障非零 → 排除;(b) 重启数
   (常规 ≥6/30 天,AI ≥3/30 天)→ 候选;(c) 单核/sibling 合并异常 ≥60% 且
   ≥2 应用 → 标记 faulty CPU;(d) 维修库 30 天内误诊/未诊断复发 → 候选。
4. **Top N 选择**:按异常总数排序,评估前 N 候选;反复入榜 = 覆盖缺口信号。
5. **排除假阳性源**:剔除 fuzzer 类故意崩溃负载(§4.4 教训)。
6. **与基线对齐**:统计 Fleetscanner 与 Ripple 各自及合并的检出集合;
   HWSentinel 检出减去两者已有检出 = 增量覆盖;计算 RDR。
7. **分维度统计**:按 CPU 代(SC-1..8)、负载族(13 类)、区域(27 个)切分 RDR
   (Figs. 5–9, 12, 13)。
8. **异常类型相关性**:统计 HWSentinel 阳性中各异常类型频率 vs 全集群频率,
   计算 Relative Exception Type Ratio(Figs. 10, 11)。
9. **部署效果验证**:对比框架上线前后内核 rollout 异常占比(§5.6)。
10. **独立金标准**:随机抽 10 台 HWSentinel 检出服务器送第三方半导体失效分析
    (CPU 代选择由第三方指导),统计 SDC 复现数(§5.7, Fig. 14)。
11. **持续运营**:检出服务器进隔离池保存至根因确认;持续追踪 FP/FN 更新 schema。

## 5. 实验数据(关键数值 + 图表号)

### 5.1 总体(§5.8 讨论 + 摘要)

- 相对 **Fleetscanner(出生产测试):+74%(1.74×)**;相对 **Ripple(在生产测试):+92%(1.92×)**;
  相对 **两者合并:+41%(1.41×)**。
- 6 年数据识别出**数百个缺陷 CPU**(摘要,精确数原文未给出)。
- Meta 观测的 SDC 发生率背景:**约 1/1000 台**(§1,引用先前论文 [31]),
  比历史上软错误率(约 1/10^6)高近三个数量级。

### 5.2 按 CPU 架构(§5.2, Figs. 5–9)

- 8 个 CPU 代(SC-1..SC-8)除 **SC-7**(早期采用期、负载量可忽略)外全部提升。
- vs **Fleetscanner**:1.31×–2.87×(除一代);**SC-4 与 SC-8 上 Fleetscanner 对
  HWSentinel 检出的 SDC 零检出**。
- vs **Ripple**:1.31×–5×;**SC-4 最高(5×)**(该架构测试机会最少、
  以 Edge/Gaming 性能敏感负载为主);**SC-7 与 SC-8 上 Ripple 零检出**;
  **SC-8 上两种现有机制都零检出,完全依赖 HWSentinel**(其专用测试
  运行时间-覆盖效率全集群最差)。
- Fig. 6:各架构 DPPM(每百万缺陷数)相对集群总量——具体 DPPM 数值在图内,
  文本层未给出,原文未给出精确数。

### 5.3 异常类型相关性(§5.3, Figs. 10–11)——本文最有复现价值的数据

HWSentinel 阳性中罕见异常的富集倍数(HWSentinel 检出率 / 全集群检出率):
| 异常类型 | 富集倍数 |
|---|---|
| **doublefault** | **59.35×** |
| stack segment(栈段错误) | 20.77× |
| nx(NX 执行违例) | 20.03× |
| invalid op(非法操作码) | 17.80× |
| int3(断点异常) | 6.92× |

(原文:increases of 20.77x, 6.92x, 59.35x, 17.80x, and 20.03x for stack segment,
int3, double fault, invalid op, and nx exceptions, respectively)
- 常见异常(lockup、oops)是软件异常的常见指示,在 HWSentinel 阳性中**不富集**。
- **核心结论:stack segment / int3 / doublefault / invalid op / nx 这五类罕见异常
  集中出现在单核上,是 SDC 诱发 CPU 的新颖指标,此前任何 SDC 检测框架都未识别过**。

### 5.4 按负载(§5.4, Fig. 12)

- 最低提升 **1.2×(Infra Maintenance)**——计划驱动、Fleetscanner 测试机会多;
  最高 **2.2×(Dev)**——代码/库频繁变更、异质性强。
- AppTest 受益于合成多样化测试序列;Backup/Storage 因容量池小、维护排空代价大、
  测试频率低,HWSentinel 提升最大。
- **Edge、Gaming、Source Control、Synchronization 四类负载上 HWSentinel 是唯一
  成功检出 SDC 的方法**(两类测试机制在这些负载上覆盖不足)。

### 5.5 按区域(§5.5, Fig. 13)

- 27 区域中 **23 个显著提升**;新区域(region7、region13)受益最大
  (Fleetscanner/Ripple 需要收敛时间);区域 17/18/19/20 无显著收益
  (测试频繁 + SC-5 架构测试高效)。
- 全集群合并:**+41%(1.41×)** 超过 Fleetscanner+Ripple 合并覆盖。

### 5.6 内核 rollout(§5.6)

- 框架上线前:约 **30% 的内核异常**归因于 Top 100(CPU-core-server-region)候选样本
  (持续遭遇 SDC 且被 Fleetscanner/Ripple 漏检);
  上线后降至 **9%**,其余多数为软件相关。
- **内核 rollout 异常减少 21%**,rollout 成功率提升 21%(摘要)。

### 5.7 独立失效分析验证(§5.7, Fig. 14)

- 随机抽 **10 台** HWSentinel 检出服务器(不同区域;CPU 代由第三方指定):
  **7/10 = 70% 由第三方失效分析确凿复现 SDC**,1 台不确定,2 台未复现。
- 行业对照:SDC 常规复现率仅 **15%–30%**(引用 [101][85][75])。
- 这 10 台样本在**更长时间、更大种子池的扩展 SDC 测试下也全部失败**
  (该扩展测试集群规模不可行,仅实验室可行)。
- 另:检出样本覆盖不同核与不同制造月份。

### 5.8 SDC 伪装案例(§5.8 讨论)

- SC-1/SC-2:SDC 导致线程间**锁无法共享** → 表现为一致性问题;
- SC-3:**cache line 有效位损坏** → 反复读入已失效数据 → 伪装成缓存一致性问题。
(案例细节称将在未来工作发表,原文仅此两句。)

### 5.9 与 ABFT/复制法的对比

**原文未给出**。正文与相关工作未包含任何 ABFT/指令复制基线的定量对比;
相关工作仅提到 Online-ABFT [3]、SwapCodes [88]、Mummidi 自检 GEMM [68][69] 等
作为背景(§6)。HWSentinel 与 ABFT 属正交层次:ABFT 保护单一内核数值正确性,
HWSentinel 做集群级事后归因。若 SDCShield 需要"与 ABFT 对比"轴,
应引用 SEVI(ASPLOS'26)或 ITHICA(Table 9 中 SEVI 行:ABFT for matmul kernels)。

## 6. 实验结论(编号列出)

1. 应用层失败信号(内核异常 + 核心集中度 + 重启/维修史 + SEL 排除法)
   可以在不依赖厂商工具的情况下检出 SDC,且 vendor-agnostic(§1、§5)。
2. HWSentinel 增量覆盖显著:vs Fleetscanner +74%、vs Ripple +92%、vs 两者合并 +41%
   (§5.8)。
3. 五类罕见异常(doublefault 59.35×、stack segment 20.77×、nx 20.03×、
   invalid op 17.80×、int3 6.92×)在 SDC 机上显著富集,
   **且集中单核出现是 SDC 诱发 CPU 的新颖指标**(§5.3)。
4. 硬件测试的覆盖因架构而异且存在系统盲区:SC-8 上两种测试机制零检出,
   完全依赖应用层方法;SC-4/SC-8 Fleetscanner 零检出、SC-7/SC-8 Ripple 零检出(§5.2)。
5. 非维护型负载(Edge/Gaming/Source Control/Synchronization)只能靠
   HWSentinel 检出 SDC(§5.4)。
6. 检出与剔除 SDC 机使内核 rollout 异常 -21%、Top-100 候选的异常占比 30%→9%(§5.6)。
7. 第三方失效分析复现率 70%,远超行业 15%–30%,证明该流程可作为
   半导体失效分析的样本提取器(§5.7)。
8. SDC 可伪装成一致性/缓存一致性问题(SC-1/2/3 案例,§5.8)。
9. 阈值(30 天/6 次重启、60% 核集中度、2 应用、1 周异常窗)是精度与基础设施成本、
   FP/FN 的折中,可按负载敏感性调优(§4.2.7)。

## 7. 复现要点(ARM64 复现最小版本)

### 7.1 可直接复用

- **整条方法论与全部阈值规则**——本来就 hardware/software-agnostic:
  内核日志结构化、30 天/6 次重启、1 周/60% 单核集中度、sibling 合并、≥2 应用、
  SEL 零 CPU 故障过滤、30 天维修复发。在任意 Linux 集群(含 ARM64)可复现。
- **五类强相关异常清单**:stack segment / int3 / doublefault / invalid op / nx。
  ARM64 上语义等价物:stack segment→`SP/EL0 栈访问异常(x25/esr 100×)`、
  int3→`BRK 指令(breakpoint)`、doublefault→ARM64 无完全等价(嵌套异常 escalate 成
  SError/kernel panic)、invalid op→`UNDEFINED INSTRUCTION`、nx→`Permission fault(L1)
  XN 违例`。Kunpeng 920 + openEuler 上可用 `dmesg`/`ras-daemon`/EDAC + `perf`
  采集等价信号。
- **数据源映射**:
  - Kernel Logs → journald/dmesg + `rasdaemon`(EINJ/EDAC)+ crash dump
    (kdump/vmcore);
  - SEL/BMC → OpenBMC Redfish / ipmitool SEL(ARM64 服务器普遍支持);
  - Reboot DB → uptime/last reboot 历史或集群管理面;
  - Repair DB → 工单系统;
  - SDC Test DB → **SDCShield 自身的历史检出日志**(YAML/TAP 输出)——
    这正是 SDCShield 相对论文的天然落点:HWSentinel 检出但 SDCShield 漏检的差集
    = SDCShield 新实验的需求清单。
- **RDR 指标定义**可直接照搬,把 Fleetscanner/Ripple 换成 SDCShield 的两个模式
  (如出生产整时段测试 vs 低负载并行短测)。
- **fuzzer 排除规则**:任何 SDCShield 复现集群上跑 fuzz 负载都必须排除,否则 FP。
- **Top-N 持续在榜 = 覆盖缺口**:可直接作为 SDCShield 测试盲区的发现机制。
- **金标准流程**:抽检送修/送失效分析,或至少用更长时间+更大种子池的
  扩展 SDCShield 测试复跑验证(论文 5.7 的等价物)。

### 7.2 需替代/不可行

- **数百万台规模、6 年历史数据、27 区域、7-8 个 CPU 代**:不可复制。
  最小替代:单机房几十至几百台 ARM64 服务器 + 数月 journald/EDAC/维修记录;
  接受统计功效大幅下降(检出数从数百台降到个位数)。
- **Fleetscanner / Ripple 基线**:Meta 私有,未开源。替代:SDCShield 自己的
  两档测试(整时段离线 vs 分时在线)或 OpenDCDiag。
- **具体 CPU 型号、区域名、负载名**:匿名化(SC-1..8、region1..27),无法对齐复现;
  只能复现方法不复现数字。
- **第三方半导体失效分析**:学术环境不可得;替代为长时间扩展功能测试
  (论文自己说这类测试"集群规模不可行但实验室可行"——单机 2000+ 小时式
  ITHICA 风格复跑是可行替代)。
- **AI 集群 3 次重启阈值**、**DPPM 图**(Fig. 6)数值:图内文字未在文本层,
  原文未给出精确 DPPM;只有相对倍数可用。
- **Abstract 说 7 个 CPU 代 vs §5.2 说 8 个**:原文自身不一致,引用时需注明。
- 超线程 sibling 合并:ARM64 无 SMT(主流 Cortex/A64xE 无),该规则在 Kunpeng 上
  退化为"物理核直接统计"——反而更简单,但意味着 60% 阈值需重新调优
  (论文的 60% 是在 SMT 合并后的分布上定的)。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield(主动功能测试)而言,HWSentinel 是**互补层而非竞争者**,
但提供以下可测对比轴:

| 对比轴 | 论文基线值 | SDCShield 可比实验 |
|---|---|---|
| 增量覆盖 vs 出生产测试 | 1.74×(Fleetscanner) | SDCShield 新实验相对"整时段离线模式"的检出台数增量 |
| 增量覆盖 vs 在生产测试 | 1.92×(Ripple) | SDCShield 相对短时在线扫描的增量 |
| 增量覆盖 vs 两者合并 | 1.41× | 新实验相对现有全套的增量上限参考 |
| 检测延迟 | **原文未给出**(无延迟数据;数据平面是离线周期性挖掘,§3.5 "operates offline") | SDCShield 在线/离线检测延迟可自测,论文无此基线 |
| 独立复现率(金标准) | 70%(7/10,§5.7) | 新实验对"检出即真"的精确率目标 |
| SDC 相关异常富集 | doublefault 59.35× 等(§5.3) | SDCShield 检出机上的内核异常伴随率可对照 |
| 架构盲区 | SC-8 双测试零检出(§5.2) | "每代 CPU 都有测试盲区"的论证:SDCShield 需按微架构分桶报告检出 |
| 负载盲区 | Edge/Gaming/SC/Sync 仅应用层检出(§5.4) | 不适用(SDCShield 是测试不是日志挖掘),但可论证 SDCShield 对无-可检-输出负载用 ITHICA 式插桩补齐 |
| 运维收益 | rollout 异常 -21%(§5.6) | 剔除缺陷机前后集群异常率变化 |

注意:论文**没有开销/延迟/吞吐数据**(无 Table 类开销表;唯一开销陈述是
§3.5 "instrumentation overhead" 定性一句)。以"检测延迟与开销"为轴对比时,
HWSentinel 无可引用数值,SDCShield 的任何实测值都只能标注"论文未提供基线"。

## 9. 局限与坑

1. **本质是"事后归因"不是"保护"**:标题说 protecting,实际是检出+隔离;
   SDC 发生到检出之间应用数据已被污染(检测延迟原文未给出,依赖 30 天/1 周窗口的
   离线挖掘节奏)。
2. **无故障注入、无合成基准**:全部依赖真实失败数据,方法上无法在无缺陷集群上
   先验证灵敏度——复现者无法在健康集群上"测出"该方法的有效性,只能等真实故障。
3. **阈值全部经验拟合**(§4.2.7 "analytical observations"):6 次重启、60%、
   2 应用、1 周/30 天窗口都是 Meta 集群特性产物,换集群(尤其 ARM 无 SMT)需重调;
   论文也承认 "A static rule set isn't sufficient"(§4.1.6)。
4. **假阳性风险依赖人工演进**:fuzzer 案例证明规则集会随负载演化失效;
   schema 需持续 FP/FN 追踪。
5. **规模化依赖重型数据基础设施**(Scuba/Presto/索引/缓存等),学术复现的
   主要工程成本在数据管道而非算法。
6. **只输出"faulty CPU or software anomaly"二分**,不做指令级/单元级定位——
   与 ITHICA 的指令定位能力相比是粒度倒退;对 SDCShield 的"激发+定位"目标
   只能提供机器级候选。
7. **数字层面的不一致**:摘要 7 个 CPU 代 vs §5.2 的 8 个;"hundreds of defective
   CPUs" 无精确数;DPPM 图无数值文本;区域/负载图(Figs. 5、12、13)的逐条数值
   均在位图中,文本层不可读——引用倍数时只能用正文明确写出的
   (1.74×/1.92×/1.41×/1.31×–2.87×/1.31×–5×/59.35× 等 5 个异常倍数/21%/70%/30%→9%)。
8. **SC-7 反例**:负载量不足的架构上应用层方法无增益——方法依赖
   "足够多的应用异常样本",低利用率环境失效。
9. **金标准样本极小(10 台)**,70% 复现率的置信区间宽(约 40%–90%,
   Wilson 区间),不宜过度解读。
10. **区域 17–20 无收益案例**表明:当硬件测试已高频高效时,应用层方法冗余——
    它是对测试不足的补丁,不是替代;SDCShield 若把测试做密做全,
    HWSentinel 式方法的增量价值会下降。

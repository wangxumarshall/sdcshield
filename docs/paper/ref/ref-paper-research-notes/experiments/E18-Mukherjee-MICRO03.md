# A Systematic Methodology to Compute the Architectural Vulnerability Factors for a High-Performance Microprocessor(MICRO 2003,Intel VSSAD + University of Michigan)

> 实验复现档案 E18(2026-09-21)
> 作者:Shubhendu S. Mukherjee(Intel VSSAD)、Christopher Weaver(Intel/UMich)、Joel Emer(Intel)、Steven K. Reinhardt(Intel/UMich)、Todd Austin(UMich)
> 出处:Proceedings of the 36th Annual International Symposium on Microarchitecture (MICRO-36),2003 年 12 月,正文 12 页。
> PDF:`docs/paper/ref/A Systematic Methodology to Compute the Architectural Vulnerability Factors for a High-Performance Microprocessor.pdf`
> 精读方式:pypdf 全文提取,12 页全部逐段读过,实验章节(§5、§6)逐句读。
> 服务对象:SDCShield(ARM64,OpenDCDiag 分支)SDC 激发实验设计的理论框架与对比基线。

---

## 1. 研究问题与核心贡献(≤5 行)

- **问题**:如何在设计早期(无 RTL 时)准确估计处理器软错误率(SDC FIT),避免保护不足或过度设计。
- **核心概念**:定义 **AVF(Architectural Vulnerability Factor)**——结构中发生单粒翻转导致程序可见错误的概率;结构错误率 = 原始电路错误率 × AVF。
- **方法创新**:ACE 位分析——跟踪"架构正确执行(ACE)所需的处理器状态位子集",保守地把一切无法证明无影响的位都算 ACE,得到 AVF **上界**。
- **系统分类**:5 类架构级 un-ACE 位(NOP、性能增强指令、谓词为假、动态死代码、逻辑掩码)+ 4 类微架构级 un-ACE 位(空闲/无效、错误推测、预测器结构、ex-ACE)。
- **结果**:用 Asim 性能模型在 SPEC2000 上测得指令队列 AVF 均值 28%、执行单元 9%(摘要页)。

## 2. 实验方法(ACE 分析方法、模拟器、故障注入验证)

### 2.1 AVF 的形式化定义(原文 §4.1)

- **位级定义**:单个存储单元的 AVF = 该单元持有 ACE 位的时间占比。例:1000 万周期执行中有 100 万周期持有 ACE 位,则 AVF = 10%。
- **结构级定义**(假设结构内所有位原始 FIT 率相同):

```
AVF(structure) = 结构中平均每周期驻留的 ACE 位数 / 结构总位数
               = 所有 ACE 位的驻留周期总和 / (结构总位数 × 总执行周期数)
```

- 第二个等式是**模拟器可直接计算的口径**:累加每条指令在各结构中的 ACE 位驻留周期,除以(位数 × 周期数)。
- **Little's Law 变体**(原文 §4.2):N = B × L。结构平均 ACE 位数 = ACE 位进入带宽(B_ACE,位/周期)× ACE 位平均驻留周期(L_ACE),故:

```
AVF ≈ B_ACE × L_ACE / 结构总位数
```

原文明确指出:B_ACE 与 L_ACE 在很多情况下可用**硬件性能计数器**获得,从而无需模拟模型即可估计 AVF——这是本文对"无模拟器场景"留下的钩子(对 SDCShield 真机量化极重要,见 §8)。

### 2.2 un-ACE 位的完整分类学(原文 §3,复现时的判定规则清单)

**微架构级(4 类)**:
1. **空闲/无效状态**:数据/状态位无效即 un-ACE;**控制位一律保守算 ACE**(翻转控制位可能把空闲当有效)。
2. **错误推测状态**:错误路径指令的位。
3. **预测器结构**:分支/跳转/返回栈/存取依赖预测器——全部 un-ACE(分支预测器 AVF 恒为 0%)。
4. **Ex-ACE 状态**:最后一次使用之后的位(如指令队列中已最后一次发射、等待确认无需重发的指令)。

**架构级(5 类)**:
1. **NOP 指令**:仅区分 NOP/非 NOP 的位(IA64 下为 opcode 或目标寄存器 specifier)算 ACE,其余 un-ACE。引用数据:Fahs 等(Alpha/SPECINT)10% NOP;Choi 等(Itanium/SPECINT)27% retired NOP。
2. **性能增强指令**(非绑定 prefetch 等):非 opcode 位 un-ACE。Fahs 报告 0.3%(Alpha)。
3. **谓词为假指令**(IA64 特有):除谓词寄存器 specifier 位外全部 un-ACE;本文实测约 7% 动态指令谓词为假。
4. **动态死指令**:FDD(一级死,结果无人读)/ TDD(传递死,结果只被 FDD/TDD 读)。**多目标寄存器指令只有全部目标都不用才算死**。经寄存器与内存双向跟踪。本文 IA64 实测约 12% FDD + 8% TDD(跨 18 个 SPEC2000 基准的模拟段)。FDD/TDD 的 opcode 位与目标寄存器 specifier 位仍保守算 ACE。
5. **逻辑掩码**:操作数中不影响计算结果的位(如 OR 常量掩掉的位、比较指令只需零/非零、64 位机上的 32 位运算高 32 位)。**本文不做传递性逻辑掩码**(明确列为未做项)。

### 2.3 模拟器与 AVF 算法(原文 §4.3、§5)

- 模拟器:**Asim 性能模型框架**(Intel 内部,IEEE Computer 2002),详细建模 Itanium2 类 IA64 处理器;OS 前端模拟 Red Hat Linux 7.2。
- **限制**:wrong-path 指令会取指执行,但 load/store **没有正确的访存地址**(模拟器不建模错误路径的地址生成)。
- **三段式 AVF 算法**:
  - (part 1)指令流经各结构时记录驻留时间;指令消失(commit 或 squash)前回填结构信息(驻留周期数、是否提交等)。
  - (part 2)提交的指令进入**提交后分析窗口**(40,000 条指令容量)判定 FDD/TDD/逻辑掩码——分析窗口含 3 个子窗口,各有"commit 顺序链表 + 按架构寄存器号/内存地址索引的生产者-消费者表"。两条对同一寄存器 R 的连续写、中间无读 → 前一条写标记为死指令(内存同理)。
  - (part 3)仿真结束时汇总计算各结构 AVF。
- **分析窗口自验证**:用手工汇编微基准,人为植入确定数量的死指令/掩码值,分析窗口报告数与植入数**精确匹配**才算通过(这是复现时必须照抄的工程质量控制)。
- **故障注入验证:原文未做**。本文定位为对 RTL 统计故障注入(Kim & Somani picoJava II;Wang & Patel Alpha 21164)的**替代与改进**,论证了 4 点优势:单次实验得确定性估计(注入需大量样本);识别死值/掩码值更全面;能解释 un-ACE 成因;设计早期无需 RTL。

### 2.4 bit-cell 级 vs structure 级分析(任务重点)

- 全文主线是 **structure 级**(整结构平均);但执行单元一节做了**位级细分**(§6.3):
  - 执行单元假定 **50% 控制锁存器 + 50% 数据通路锁存器**;先算整个单元(控制+数据)的统一 AVF,再单独对数据通路进一步去率。
  - **逻辑掩码分析**:处理器模型约 2000 种静态内部指令类型中,对"小而重要"的子集(OR、AND 等)实现了掩码函数;另有约 20% 指令(load/store/branch)判定为无直接逻辑掩码。两项合计覆盖动态指令的绝大多数。效果:平均再降 **0.5%**。
  - **DATAPATH_IDLE**:例——IA64 compare 产生 2 个谓词位,却经 64 位结果总线传输,62 条数据通路空闲。效果:平均再降 **1.5%**。
  - 执行单元 AVF 低于指令队列的三个机理(§6.3):(1) ACE 指令在 IQ 驻留远长于在 EU 执行;(2) cache-miss 后推测发射的指令要在 IQ 重放,**只有最后一次经过 EU 的状态有效**,此前通过 EU 的状态计为 un-ACE;(3) 整数程序下浮点流水线大多空闲。

## 3. 实验配置(模拟器配置:核模型/频率/结构大小;基准;参数)

| 配置项 | 值(原文出处) |
|---|---|
| 核模型 | Itanium2 类 IA64,Asim 详细性能模型(§5) |
| 发射宽度 | 6 发射:4 整数流水线 + 2 浮点流水线;整数乘法走浮点流水线(§6.3) |
| 频率/工艺 | "scaled to current technology"(§5);**具体频率原文未给出** |
| 指令队列 | 64 表项;每表项约 100 位(IA64 指令 41 位 + 在飞状态位);其中约 5 位控制位、7 位 opcode、6 位谓词 specifier、7 位目标寄存器 specifier(§6.2) |
| 执行单元 | 控制锁存器/数据通路锁存器各约 50%;约 2000 种静态内部指令类型(§6.3) |
| OS | Red Hat Linux 7.2,OS 仿真前端(§5) |
| 基准 | SPEC CPU2000 全 26 个(12 整数 + 14 浮点),Table 1 逐个列出跳过指令数(见下表) |
| 采样 | SimPoint 分析(适配 IA64),**每个基准只取第一个 simpoint**,每点 100M 条指令(含 NOP)(§5) |
| 编译器 | Intel electron 7.0,最高优化级(§5) |
| 分析窗口 | 40,000 条提交指令;NOT_PROCESSED 恒为仿真末尾 20,000 条(§4.3、§6.1) |
| 原始故障率假设 | 海平面 latch/SRAM 0.001–0.01 FIT/bit(§2.1,引 [24][17][12][11]);逻辑门 FIT 忽略 |
| 行业目标参考 | IBM Power4 系统:SDC 1000 年 MTBF(=114 FIT)、DUE-crash 25 年、DUE-app 10 年(§2.1) |

**Table 1 完整基准与跳过指令数**(复现选点直接参照;M = 百万):

| 整数基准 | 跳过(M) | 浮点基准 | 跳过(M) |
|---|---|---|---|
| bzip2-source | 48,900 | ammp | 50,900 |
| cc-200 | 16,600 | applu | 500 |
| crafty | 120,600 | apsi | 100 |
| eon-kajiya | 73,000 | art-110 | 36,400 |
| gap | 18,800 | equake | 1,500 |
| gzip-graphic | 29,000(原文排印 "2,9000") | facerec | 64,100 |
| mcf | 26,200 | fma3d | 23,600 |
| parser | 71,400 | galgel | 5,000 |
| perlbmk-makerand | 0 | lucas | 123,500 |
| twolf | 185,400 | mesa | 73,300 |
| vortex_lendian3 | 59,300 | mgrid | 200 |
| vpr-route | 49,200 | sixtrack | 4,100 |
| — | — | swim | 78,100 |
| — | — | wupwise | 23,800 |

**IA64 表项位构成(§6.2,bit-cell 级计量的原始口径)**:每 IQ 表项约 100 位 = IA64 指令 41 位 + 在飞状态位;保守不扣减的位:5 位控制位 + 7 位 opcode(所有指令)+ 6 位谓词 specifier(谓词假指令)+ 7 位目标寄存器 specifier(FDD/TDD 指令)。即单条 un-ACE 指令最多可扣 100−5−7 = 88 位(谓词假再少 6、死指令再少 7)。

## 4. 实验步骤(可操作流程,编号)

1. **选型**:取一个详细的乱序处理器性能模型(原文 Asim/Itanium2-like),确认待测结构(IQ、EU)在模型中存在。
2. **植入三段式插桩**:
   a. 每条指令对象携带"流经结构 → 驻留周期"记录;
   b. 指令 commit/squash 时回写各结构驻留周期与去向标签;
   c. commit 后进入 40K 指令分析窗口。
3. **实现分析窗口**:FDD/TDD/逻辑掩码 3 个子窗口;每个子窗口 = commit 顺序链表 + 寄存器号(及内存地址)索引的生产者-消费者表;窗口满 40K 后从最老指令开始做死性/掩码判定并回填标签。
4. **微基准自检**:汇编微基准植入已知数量的死指令与掩码值,验证窗口输出与植入数完全一致(§4.3)。
5. **按保守规则给每条指令打位级标签**:全 1(ACE)起步;再按 §2.2 分类逐类扣减(注意:控制位/opcode/FDD-TDD 的目标寄存器 specifier 不扣)。
6. **SimPoint 选点**:每基准第一个 simpoint,100M 指令,收集总周期数。
7. **计算 AVF**:`Σ(ACE 位驻留周期) / (结构总位数 × 总周期)`,按结构输出;同时输出状态分解堆栈图(ACE/IDLE/Ex_ACE/WRONG_PATH/FDD/TDD/PREFETCH/PREDICATED_FALSE/NOP/UNKNOWN)。
8. **交叉验证(Little's Law)**:从同一模拟器取 ACE IPC 与 ACE latency,AVF_LL = ACE IPC × ACE latency / 64(IQ 表项数),与位级口径对比,解释差值(§6.2 用差值 9 个百分点定位"un-ACE 指令的 ACE 位"贡献)。
9. **(可选,原文未做)故障注入对照**:对同结构做统计故障注入,验证上界的松紧。

## 5. 实验数据(关键 AVF 数值表 + 图表号)

### 5.1 结构级 AVF 主表

| 结构/口径 | 平均 | 范围 | 图表号 |
|---|---|---|---|
| 指令队列 IQ | **28%** | 14%–47% | 摘要;§6.2;Figure 3 |
| IQ — 仅整数程序 | 25% | — | §6.2 |
| IQ — 仅浮点程序 | 31% | — | §6.2 |
| 执行单元 EU(控制+数据统一,未做逻辑掩码/数据通路空闲扣减) | 11% | 4%–27% | §6.3;Figure 4 |
| 执行单元 EU(再扣逻辑掩码 −0.5%、数据通路空闲 −1.5%) | **9%** | 4%–27% | 摘要;§6.3;Figure 4 |
| IQ 状态分解(均值) | ACE 28%、IDLE 30%、un-ACE 42% | — | Figure 3 |
| 分支预测器 | 0%(定义) | — | §1 |
| 提交 PC | ~100%(定义) | — | §1 |

**注意(数值自相矛盾,引用必须注明)**:结论 §8 写的是 "IQ 14%–40%、EU 2%–17%",与正文 §6.2/§6.3 的 14%–47% / 4%–27% 不一致;摘要与正文一致(28%、9%)。以正文 §6 为准,引用时标注段落。

### 5.2 程序级分解(Figure 2,26 基准均值,占已提交指令比例)

| 类别 | 占比 |
|---|---|
| ACE 指令 | **约 45%** |
| NOP | 26% |
| FDD_reg(经寄存器的一级死) | 9.4% |
| TDD_reg | 6.6% |
| 谓词为假 | 6.7% |
| FDD_mem | 2% |
| TDD_mem | 1.6% |
| Prefetch | 1.5% |
| UNKNOWN + NOT_PROCESSED | 约 1% |

即 **55% 的动态指令是 un-ACE 指令**(其中部分仍含 ACE 位,如 prefetch 的 opcode)。

### 5.3 Little's Law 交叉验证(Table 2;#ACE inst = ACE IPC × ACE Latency;AVF ≈ #ACE inst / 64 表项)

| 组 | ACE IPC | ACE Latency(cyc) | #ACE Inst | AVF |
|---|---|---|---|---|
| 整数均值(13 基准) | 0.45 | 23 | 9 | **15%** |
| 浮点均值(14 基准) | 0.77 | 23 | 14 | **23%** |
| 单基准极值:ammp | 0.23 | 92 | 21 | 33% |
| 单基准极值:wupwise | 1.60 | 13 | 20 | 31% |
| 单基准极值:crafty | 0.37 | 15 | 6 | 9% |
| 单基准极值:vpr-route | 0.35 | 12 | 4 | 7% |
| 单基准极值:lucas | 1.23 | 17 | 21 | 33% |

- 指令级 Little's Law 均值 19%,比位级实测 28% 低 9 个百分点;差值 = un-ACE 指令(prefetch、死指令)自带的 ACE 位。若做**位级** Little's Law 可对齐(§6.2)。
- lucas vs ammp:AVF 相近但机理相反(高 ACE IPC × 低延迟 vs 低 ACE IPC × 高延迟)——AVF 是乘积,单看 IPC 或延迟都会误判(§6.2,对负载选择有直接指导意义)。

### 5.4 背景与既有基线数值(§1、§2、§7)

- Figure 1:2003 年假设 200,000 个可翻转位、0.001 FIT/bit;2005 年要满足 IBM 114 FIT SDC 目标,AVF=100% 时须保护 80% 的位,AVF=10% 时可不保护。
- 先前 RTL 注入结果:锁存器 AVF 1%–10% [25];架构/微架构状态位 0%–100% [13];Alpha 21164 流水线锁存器 <10% [25]。
- Fujitsu SPARC64:200,000 锁存器中 80% 有错误检测(§1)。

### 5.5 Table 2 完整数值(26 基准 × 4 列,复现对照的逐点数据)

| 整数基准 | ACE IPC | ACE Lat | #ACE | AVF | 浮点基准 | ACE IPC | ACE Lat | #ACE | AVF |
|---|---|---|---|---|---|---|---|---|---|
| bzip2-source | 0.55 | 22 | 12 | 19% | ammp | 0.23 | 92 | 21 | 33% |
| cc-200 | 0.57 | 18 | 10 | 16% | applu | 0.82 | 21 | 18 | 27% |
| crafty | 0.37 | 15 | 6 | 9% | apsi | 0.31 | 31 | 9 | 15% |
| eon-kajiya | 0.36 | 20 | 7 | 11% | art-110 | 0.68 | 37 | 25 | 40% |
| gap | 0.78 | 17 | 13 | 21% | equake | 0.26 | 12 | 3 | 5% |
| gzip-graphic | 0.60 | 13 | 8 | 12% | facerec | 0.41 | 7 | 3 | 5% |
| mcf | 0.25 | 68 | 17 | 26% | fma3d | 0.59 | 11 | 7 | 10% |
| parser | 0.49 | 24 | 12 | 19% | galgel | 1.10 | 21 | 23 | 35% |
| perlbmk-makerand | 0.38 | 17 | 7 | 10% | lucas | 1.23 | 17 | 21 | 33% |
| twolf | 0.30 | 27 | 8 | 13% | mesa | 0.47 | 16 | 8 | 12% |
| vortex_lendian3 | 0.42 | 22 | 9 | 15% | mgrid | 1.28 | 10 | 13 | 21% |
| vpr-route | 0.35 | 12 | 4 | 7% | sixtrack | 0.66 | 20 | 13 | 21% |
| — | — | — | — | — | swim | 1.08 | 16 | 17 | 27% |
| — | — | — | — | — | wupwise | 1.60 | 13 | 20 | 31% |
| **平均** | **0.45** | **23** | **9** | **15%** | **平均** | **0.77** | **23** | **14** | **23%** |

(表来源:原文 Table 2,标题 "AVF breakdown using Little's Law. # ACE inst = ACE IPC X ACE Latency. AVF ~= # ACE inst / # instruction queue entries";#ACE 列四舍五入到整数,与乘积可能有 ±1 出入——原文如此。)

**逐点读数要点**:
- 整数侧 AVF 最高是 mcf(26%,ACE latency 68 周期——低 IPC 但长驻留);最低 vpr-route(7%)。
- 浮点侧最高 art-110(40%,latency 37);最低 equake/facerec(5%,latency 12/7——短驻留压过一切)。
- ACE IPC 极值:wupwise 1.60(超过程序 IPC 上限的原因:重放指令重复计数,见原文 §6.2 对 replay 的说明)。
- 该表与 Figure 3 位级 AVF 的系统差:指令级不区分指令内部的 un-ACE 位,均值 19%(加权 15%+23%)vs 位级 28%。

## 6. 实验结论(编号列出)

1. **AVF 可用性能模型(而非 RTL)在设计早期确定性算出**,单次实验即得紧上界,覆盖 SPEC2000 全套动态切片。
2. 指令队列 AVF 均值 28%(14–47%),执行单元 9%(4–27%);**长驻留结构(IQ)比短驻留结构(EU)AVF 高约 3 倍**。
3. 浮点程序 AVF(31%)高于整数(25%):长延迟指令多、分支误预测少 → IQ 利用更充分。
4. 55% 的动态指令对程序输出无影响(NOP+死代码+谓词假+prefetch)——指令流本身有一半以上"天然免疫"软错误。
5. 执行单元低 AVF 的三机理:驻留短、重放只需最后一次、整数程序的 FP 流水线空闲。
6. Little's Law 可给一阶近似(误差 9 个百分点),且 B_ACE/L_ACE 可来自性能计数器 → **真机 AVF 估计可行**。
7. 大结构 + 高 AVF 是保护(ECC/parity)的第一优先对象;AVF 是把原始 FIT 映射到芯片 SDC FIT 的乘子,可迭代指导保护预算分配。
8. 保守性声明:所有数值是**上界**;进一步细化(如 IA64 load hint 位去率、传递性逻辑掩码)预计只会小幅降低数值。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

| 要素 | 说明 |
|---|---|
| AVF 两个等价公式 | 位驻留周期口径 + B_ACE×L_ACE 口径,ISA 无关 |
| un-ACE 十分类判定规则 | §2.2 清单可直接作为 gem5 O3CPU(ARM)插桩的字段判定表 |
| 保守上界原则 | "无证明即 ACE",保证复现结果可比性 |
| 40K 指令分析窗口 + 微基准自检 | 窗口大小与验证方法直接照抄;FDD/TDD 判定可用生产者-消费者表 |
| Little's Law 真机估计路径 | 原文明示 B_ACE/L_ACE 可来自硬件性能计数器 → 在 Kunpeng 920 上用 PMU(如 ARM SPE/perf)做一阶 AVF 对比完全成立 |
| 程序级分解方法论 | 不依赖模拟器细节,可移植到 QEMU/LLVM 插桩做 PVF 式分析 |
| 状态分解堆栈图 | Figure 3/4 的分解维度是结果呈现标准 |

### 需替代/不可行

| 要素 | 替代方案 |
|---|---|
| Asim(Intel 内部) | gem5 O3CPU(ARM)v21+,或 GEM5 全系统模式;注意 gem5 **无原生 ACE 位跟踪**,需自行在 ROB/IQ/LSQ 项结构里加 ACE 位掩码字段(参考 gem5-approxlyzer,ISCA'22,对 x86 做过位级分析,思路可移植) |
| IA64 谓词/bundle NOP 特性 | ARM64 无 bundle 对齐 NOP → NOP 占比 26% 不可平移,须重测;谓词为假 6.7% 类似(AArch64 无通用谓词执行,CMP+CSel 的掩码效应需另行分析) |
| IA64 41 位指令/100 位 IQ 表项 | 按 gem5 O3 ARM 的实际 ROB/IQ 位数重新计量 |
| SPEC2000 + electron 编译器 | SPEC2017 (GCC/Clang -O2/-O3);SimPoint 3.0 或直接用 SDCShield 自身负载集 |
| wrong-path 无访存地址 | gem5 可取到错误路径地址(若建模),反而是增强;至少不是障碍 |
| 6 发射 IA64 | Kunpeng 920(TaiShan v110,ARMv8.2)/Neoverse 宽度与结构差异大,数值只作定性参照 |
| 100M 单 simpoint | 建议多点 + 相位分析(后续文献已证明相位间 AVF 差异巨大) |

**最小复现配方(gem5)**:O3CPU + DERIV 的 4-wide ARM 模型,ROB 128/IQ 64;在 Comm::StructInst 提交路径挂 40K 指令窗口(寄存器表 + 简化内存表);IQ/ROB 每项加 32 位 ACE 掩码(按 opcode 类别初始化为全 ACE,死指令回填时清非 opcode/非目标 specifier 位);跑 4 个 SPEC2017 整数 + 4 个浮点各 100M 指令;输出 Figure 3 式堆栈 + Table 2 式 Little's Law 对照。预算:单人 2–4 周。

### 复现实现细节补充(把 §4 步骤落到代码级)

**A. ACE 位掩码的字段划分(ARM64 ROB 表项示例,对应原文"每表项 100 位"的计量)**:

```
ROB entry(示例 76 位,对齐 Nair HPCA'12 的计量习惯):
  opcode 域        ~10 位  → 恒 ACE(控制类)
  目的寄存器号      5 位   → 恒 ACE(死指令也不扣,保守规则)
  源寄存器号 ×2     10 位  → 死指令回填时扣
  结果数据         32 位   → 死指令回填时扣;逻辑掩码位逐位扣
  标志/序列/控制    ~19 位  → 恒 ACE(控制位规则)
判定顺序:squash(错误路径)→ 全 un-ACE;
        FDD/TDD → 只留 opcode+目的寄存器号;
        NOP     → 只留 opcode(或区分位);
        其余    → 逻辑掩码逐位判定。
```

**B. FDD/TDD 分析窗口的数据结构(原文 §4.3 的工程化)**:

```
window[40000]  # 环形,commit 顺序
reg_table[31]  # 每架构寄存器 → 最近生产者 window 下标链
mem_table      # 哈希:页+行 → 最近 store 的 window 下标(容量截断,近似)
规则:
  commit inst i 写 Rd:若 reg_table[Rd] 的生产者 j 之后无消费者 → j 标 FDD_reg;
                            若 j 本身 FDD/TDD → i 标 TDD_reg(传递闭包,窗口内迭代)
  同地址连续两个 store、中间无 load → 老 store 标 FDD_mem(内存跟踪,原文明确要求)
```

原文强调 FDD/TDD 经寄存器**和内存**双向跟踪;只做寄存器侧会把 FDD 低估(他们实测 IA64 FDD_reg 9.4% + FDD_mem 2%)。

**C. Little's Law 真机化(对 SDCShield 最有价值的一条)**:
原文口径:AVF_IQ ≈ (ACE IPC × ACE latency)/ 64。真机上:
- ACE IPC ≈ PMU 的 SIMD/FMA 指令吞吐(SDCShield 测试中每条指令结果都进入比对 → 全 ACE);
- ACE latency ≈ 指令在目标结构的驻留周期(可用延迟链深度 × 平均等待近似);
- 64 → 目标结构表项数(如 Kunpeng920 的 ROB/LSQ 深度)。
由此可在真机上给出"测试的等效 AVF 下界",与 SPEC 负载的 28% IQ / 9–11% EU 直接对比——**这就是"我的测试覆盖了多少脆弱状态"的可引用量化**。

**D. 与 SDCShield 现有测试族的映射草案**:

| SDCShield 测试 | 目标结构 | 论文对应结构 | 预期等效 AVF 论证点 |
|---|---|---|---|
| eigen_svd/sparse、openblas_dgemm | FMA 流水线、SIMD 通路 | 执行单元(9–11% 基线) | 全 ACE + 高占用 → 等效 AVF 接近单元占用率上限 |
| zstd19/isal_igzip | 整数 ALU + L1D | 执行单元 + (E20)cache | 压缩比对的位敏感性 → 逻辑掩码少 |
| sve512 类长延迟链测试 | ROB/LSQ 阴影区 | 指令队列(28% 基线) | miss 阴影区满占用(E21 的 O=W 结论)|

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield 的核心价值:**AVF 是"测试覆盖了多少脆弱状态"的量化语言**。SDC 激发实验的优劣可用"单位时间驻留于目标结构的 ACE 位数"来比较,这正是 AVF 的分子。

| 对比轴 | 论文基线值 | SDCShield 侧的可测量 |
|---|---|---|
| 执行单元 ACE 占用率 | SPEC 均值 9–11%,范围 4–27%(Figure 4) | SDCShield 向量 FMA/SIMD 测试(openblas_dgemm、sleef、eigen)应逼近 ~100% 单元占用且全 ACE(结果被 golden 比对消费)→ "AVF 放大倍数" = 测试占用率 / 9% ≈ 9–11× |
| 结构驻留时长 vs AVF | IQ 28% vs EU 9%(驻留长 → AVF 高 3×) | 激发实验应制造**长驻留** ACE 状态(长依赖链 FMA、深 MLP),而非短突发 |
| 指令流 un-ACE 占比 | SPEC 均值 55% un-ACE 指令(Figure 2) | SDCShield 测试的 golden-verify 设计天然全 ACE(每条结果都比对)→ 覆盖效率比 SPEC 负载高一倍以上,可用同窗口法在 QEMU/LLVM 侧量化证明 |
| Little's Law 一阶对比 | 指令级 19% vs 位级 28%(Table 2) | 在真机用 PMU 取 SIMD 指令吞吐(近似 B_ACE)+ 结果消费率,可现场估"测试 AVF",无需模拟器——直接引用原文 §4.2 的授权 |
| 浮点 vs 整数 | FP 31% vs INT 25%(§6.2) | 支持优先压浮点/向量通路(SDCShield 现有测试分布合理) |
| 负载选择陷阱 | lucas vs ammp 乘积效应(Table 2) | 高 IPC ≠ 高 AVF;测试设计要看 (吞吐 × 驻留) 乘积 |

## 9. 局限与坑

1. **无 SMT 对比数据(任务预期重点,原文未给出)**:全文 12 页为单线程、单处理器模型;SMT 仅出现在参考文献 [18](Reinhardt & Mukherjee,AR-SMT)与 [16](RMT),均为 SMT 冗余检测方案,无 SMT AVF 数值。若需 SMT AVF 基线须另找文献。
2. **数值自相矛盾**:结论 §8(14–40%/2–17%)与正文 §6(14–47%/4–27%)不一致;引用必须写明出处段落。
3. **上界而非点估计**:所有 AVF 偏保守;未做传递性逻辑掩码、未去率 IA64 load hint 位。
4. **单位错模型**:只考虑单粒翻转;多位翻转、锁存器时序窗口(timing vulnerability factor)被假设已折入原始 FIT。
5. **性能模型盲区**:只能算被建模结构的 AVF(与 RTL 相比缺非性能关键结构);原文自己承认。
6. **wrong-path 无正确访存地址**:错误路径 load/store 的内存侧死性判定缺失。
7. **单 simpoint 100M 指令**:相位敏感性未评估;NOT_PROCESSED 固定 20K 条、UNKNOWN 约 1%,小但非零。
8. **IA64 特有比例不可平移**:NOP 26%、谓词假 6.7%、FDD 12%/TDD 8% 都是 IA64+electron 编译器产物;ARM64+现代编译器要重测(预计 NOP 大幅下降、死代码比例随 -O3 上升)。
9. **执行单元 50/50 控制/数据锁存器假设**与真实综合结果可能有偏差;DATAPATH_IDLE 的 62/64 空闲 lane 论断是特定实现(64 位总线传 2 谓词位)的产物。
10. **对 SDCShield 的语义映射注意**:本文 AVF 度量的是**宇宙射线软错误的架构掩蔽**,与 SDCShield 面向的制造缺陷/硬故障不同类;直接桥接须用 Bower 的 H-AVF(见 E22)或 PinDrop/SEVI 的缺陷检出率口径,AVF 提供的是"状态覆盖"这一半边。

## 附:关键术语中英对照(复现写作时统一用语)

| 英文 | 中文(本档案采用) |
|---|---|
| ACE (Architecturally Correct Execution) | 架构正确执行 |
| ACE bit / un-ACE bit | ACE 位 / un-ACE 位 |
| AVF (Architectural Vulnerability Factor) | 架构脆弱因子 |
| FDD / TDD (first-level / transitively dynamically dead) | 一级动态死 / 传递动态死 |
| logical masking | 逻辑掩码 |
| ex-ACE state | ex-ACE 状态(末次使用后) |
| wrong-path instructions | 错误路径指令 |
| Little's Law | Little 定律 |
| derating | 去率/降额 |
| SDC / DUE | 静默数据损坏 / 检测到不可恢复错误 |
| FIT (failures in time, 10⁻⁹/h) | FIT 率 |
| residence time / latency | 驻留时间 |
| issue / dispatch / commit (retire) | 发射 / 分派 / 提交 |

## 附:与后续文献链的关系(本档案在 5 篇中的位置)

- **E18(本篇)= AVF 的原始定义与 ACE 分类学**,一切后续文献的公理基础。
- E19(IEEE Micro 杂志版)= 本篇的缩写重述,无实验增量。
- E20(Biswas ISCA'05)= 把 ACE 分析从计算类结构(IQ/EU)推广到**地址类结构**(cache/TLB/store buffer),补上汉明距-1 的 tag 语义。
- E21(Nair HPCA'12)= 把"跑模拟器才能得的 AVF"变成"剖析 + 解析公式",并给出 miss 事件区间占用理论。
- E22(Bower)= 把 AVF 思想推广到**硬故障**(H-AVF),是 AVF 与 SDCShield 缺陷检测目标之间的语义桥梁。
- 5 篇之外相关:Sridharan 的 PVF/HVF(HPCA'09/ISCA'10)、Wang 等 gem5-approxlyzer(ISCA'22)、DelayAVF(MICRO'24,已在 docs/paper/ref 有 PDF)——分别解决"去 ISA 依赖""gem5 位级工具化""时序故障 AVF"。

# Measuring Architectural Vulnerability Factors(任务名 Biswas ISCA'05;实测为 Mukherjee 等人 IEEE Micro「MICRO Top Picks」杂志版,2003)

> 实验复现档案 E19(2026-09-21)
> **身份勘误(必须先行声明)**:任务将本 PDF 标注为 "Biswas et al., ISCA 2005"。对 PDF 全文 6 页逐句精读后确认:
> 1. 该文作者是 **Shubhendu S. Mukherjee、Christopher T. Weaver、Joel Emer、Steven K. Reinhardt、Todd Austin**——与 E18(MICRO'03)完全相同的作者组;
> 2. 出处为 **IEEE Micro, Vol. 23, No. 6, pp. 70–75, 2003 年 11–12 月**,"MICRO Top Picks" 特刊(页脚 "Published by the IEEE Computer Society 0272-1732/03/$17.00 © 2003 IEEE","NOVEMBER–DECEMBER 2003");
> 3. 内容是 E18 论文的**杂志缩写版**,无任何 address-bit AVF、L1 D-tag、去耦合方法内容;
> 4. **任务所需的 Biswas ISCA'05 内容(地址位 AVF、L1 D-tag、hamming-distance-one)实际全部在 E20 那本 PDF(`Computing_architectural_vulnerability_factors_for_address-based_structures.pdf`,其页脚明确为 "Proceedings of the 32nd International Symposium on Computer Architecture (ISCA'05)")**。
> 因此本档案记为「MICRO'03 杂志版」的复现记录,并集中记录其相对 E18 的**增量信息**;Biswas/地址位内容在 E20 档案中完整呈现。
> PDF:`docs/paper/ref/Measuring_architectural_vulnerability_factors.pdf`(6 页,全部精读)。

---

## 1. 研究问题与核心贡献(≤5 行)

- 与 E18(MICRO'03)同一工作:定义 AVF,用 ACE 位分析在性能模型上估计处理器 SDC 率。
- 杂志版定位:面向更广读者群的 6 页精简重述 + 少量**独有的补充论述**(ACE 定义的展开、RMT 相关工作、作者群信息)。
- 独有内容:明确写出「ACE = any execution that generates results consistent with a system's correct operation as observed by a user」这一更完整的 ACE 定义;预测器四分类的 "prediction only" 措辞;ex-ACE 的 IQ-replay 例证。
- 数值结论与 E18 一致:IQ AVF 14–47%、EU 4–27%(Figure 1 + 正文)。
- 对复现工程的价值:是 E18 的"最短可读摘要",适合作为 SDCShield 文档中的引用入口。

## 2. 实验方法(ACE 分析方法、模拟器、故障注入验证)

与 E18 完全同源,仅列杂志版表述差异与增量:

- **ACE 定义(更完整版,§"Computing a processor's soft-error rate")**:ACE = "any execution that generates results consistent with a system's correct operation **as observed by a user**"。un-ACE = 具体取值对 ACE 不必要的位。
- **AVF 两个口径**同 E18:位占比口径;Little's Law 口径 N = B × L(公式以图形式给出:AVF = B_ACE × L_ACE / 结构总位数)。
- **杂志版独有强调**:Little's Law 口径"特别适用于工业设计周期早期连性能模型都没有的阶段",且 B_ACE/L_ACE 可来自**硬件性能计数器**——无模型 AVF 估计的路线在本文被更直白地授权。
- **微架构 un-ACE 四分类的杂志版措辞**:idle or invalid / misspeculated / **prediction only** / ex-ACE。ex-ACE 例证:「ACE 指令位停在指令队列中等待可能的重放,若重放不发生则变 ex-ACE」。
- 模拟器:Asim 框架(Itanium2-like);Figure 1 给出 IQ 的状态分解图(与 E18 Figure 3 同一实验,坐标轴换为 0–100 Percentage in State,分组合并为 Dynamically dead/Prefetch/No-op)。
- 故障注入验证:未做;相关工作段引用 Kim & Somani(picoJava II RTL 注入)、Wang & Patel(Alpha 21164,流水线锁存器 AVF <10%)作为对比方法。

## 3. 实验配置(模拟器配置:核模型/频率/结构大小;基准;参数)

- 杂志版**未重复**详细配置表;正文指明 "an Itanium 2-like processor's instruction queue" + Asim + SPEC CPU2000 动态切片。
- Figure 1 横轴 26 个 SPEC2000 基准名与 E18 Table 1 一致。
- 指令队列 AVF 范围 14–47%、执行单元 4–27%(正文 §"Using a performance model to compute AVFs")。
- 其余配置(6 发射、64 表项 IQ、100 位/表项等)见 E18 档案 §3;杂志版原文未给出这些细节。

## 4. 实验步骤(可操作流程,编号)

与 E18 §4 完全相同(三段式插桩 + 40K 分析窗口 + SimPoint + Little's Law 交叉验证)。杂志版未给出新步骤。唯一增量指引:

1. 若只读一篇入门,按杂志版 §"Computing a processor's soft-error rate" → §"Using Little's law to approximate AVFs" → §"Using a performance model to compute AVFs" 的顺序建立方法论骨架。
2. 杂志版明确把「用性能计数器替代模拟器」列为 Little's Law 口径的首要应用场景——复现 SDCShield 真机量化时应直接以此为方法出处。

### 4.1 阅读路线图(6 页杂志版的逐页导航)

| 页 | 小节 | 内容类型 |
|---|---|---|
| p.70 | "Soft-error sources and impact" | 物理背景(单粒子翻转机理、海拔通量、工艺趋势、Sun/Fujitsu 案例) |
| p.71 | "Computing a processor's soft-error rate" | SDC/DUE 定义;AVF 定义与两锚点;ACE/un-ACE 定义;结构 AVF 平均化 |
| p.72 | (续) + "Using Little's law to approximate AVFs" | 十分类清单;N = B × L 公式;性能计数器路线 |
| p.72–73 | "Using a performance model to compute AVFs" | Asim + IQ 实验声明;Figure 1 出处 |
| p.73–74 | Figure 1 + "Related work" | IQ 状态分解图(全幅);vs RTL 注入四优势 |
| p.74–75 | 结尾 + 参考文献 14 条 + 作者简介 | 结论;文献线索 |

### 4.2 若要用杂志版做教学/组内汇报

- 建议顺序:先 Figure 1(直观:26 基准的 IQ 状态堆栈)→ 定义段 → Little's Law 段 → Related work 四优势;
- 汇报中若被问"EU 的图在哪":答"在会议版 Figure 4,杂志版正文注明 'a similar plot for the execution units is available in another publication'"(p.73 原文,该 "another publication" 即会议版)。

## 5. 实验数据(关键 AVF 数值表 + 图表号)

| 项 | 值 | 图表号 |
|---|---|---|
| 指令队列 AVF 范围 | 14%–47% | 正文;Figure 1 |
| 执行单元 AVF 范围 | 4%–27% | 正文(图在另文,即 E18 Figure 4) |
| IQ 状态分解(Average 柱) | ACE 约 28%(与 E18 Figure 3 一致;杂志版图纵轴 0–100%) | Figure 1 |
| SDC/DUE 分类 | SDC = 未检测错误;DUE = 检测到但不可恢复 | §"Computing a processor's soft-error rate" |
| 先前 RTL 注入基线 | picoJava II 各结构 AVF 变化很大;Alpha 21164 流水线锁存器 <10% | Related work |
| Fujitsu SPARC64 保护率 | 200,000 锁存器的 80% 有错误检测 | §"Soft errors caused by cosmic rays" |

**地址位 AVF / L1 D-tag 数据:本 PDF 原文未给出**(这是任务预期的 Biswas ISCA'05 内容,实际在 E20 档案)。

### 5.1 杂志版可引用的物理/工业背景参数(p.70–71,写论文 Introduction 的素材)

| 参数 | 值(原文) |
|---|---|
| Denver(海拔 1.5 km)中子通量 | 海平面的 3–5 倍 |
| 工艺缩小对位错误率的影响 | 每位电荷减少(更易翻转)× 截面积减少(更难命中)≈ 相互抵消;每位错误率未来数代"大致持平或略降" |
| 无纠错时芯片错误率 | 随位数(摩尔定律)线性增长 |
| 原始故障率 | 0.001–0.01 FIT/bit(海平面,latch/SRAM) |
| Sun 事故(2000) | Enterprise 服务器线未保护 cache 宇宙射线随机崩溃,丢失大客户给 IBM |
| Fujitsu 响应 | 近期 SPARC 处理器 200,000 锁存器的 80% 加错误检测 |
| Sun/Fujitsu 出处 | Baumann IEEE 2002 IRPS 教程;Ando 等 ISSCC'03 |

### 5.2 杂志版 ACE/un-ACE 分类总表(杂志版 §"Computing a processor's soft-error rate" 的两张清单,作统一引用入口)

| 层级 | 类别 | 例证(原文措辞) |
|---|---|---|
| 架构 un-ACE(5) | no-op instructions | — |
| | performance-enhancing instructions | prefetch 等 |
| | predicated-false instructions | — |
| | dynamically dead code | 结果无人用 / 只被其他死指令用 |
| | logically masked values | — |
| 微架构 un-ACE(4) | idle or invalid | — |
| | misspeculated | incorrect-path instructions |
| | prediction only | 预测器结构 |
| | ex-ACE | IQ 中等待重放而重放未发生的 ACE 位 |

(与 E18 §2.2 同源;"prediction only" 与 "ex-ACE" 为杂志版措辞。)

### 5.3 vs RTL 统计注入四优势(杂志版 Related work,引用时的标准表述)

1. ACE 分析对任意基准**单次实验**给确定性 AVF 估计;统计注入需多次实验才达统计显著。
2. 识别死值/掩码值 → 更全面的故障影响判定 → 更紧的估计。
3. 给出系统行为洞察(un-ACE 成因分解;Little's law 一阶估计即为例证)。
4. 设计早期 RTL 不存在,性能模型通常已有 → 可在架构探索阶段即产出 AVF。

## 6. 实验结论(编号列出)

1. AVF × 原始故障率 = 结构 SDC 率;逐结构求和得整机 SDC 预算——设计早期成本/可靠性权衡的核心工具。
2. ACE 分析相对 RTL 统计注入的四大优势(杂志版重申):(1) 单次实验得确定性估计;(2) 识别死值/掩码值,估计更紧;(3) 能解释 un-ACE 成因(分解视图);(4) 设计早期无需 RTL,性能模型即可。
3. 分支预测器 AVF = 0%、提交 PC ≈ 100% 是两个锚点;多数结构介于其间,须逐周期跟踪。
4. Little's Law 口径 + 硬件性能计数器 → 无模拟器的早期/真机估计路线。
5. 大结构 + 高 AVF 是保护首选;可迭代加保护并重算 AVF 直到达标(引用 IBM Power4 目标:SDC 1000 年 MTBF)。
6. 杂志版新增展望引文:Wang, Fertig & Patel(Y-Branches,PACT'03)指出条件分支指令的某些故障不影响最终输出——即 AVF 上界仍有下压空间。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

| 要素 | 说明 |
|---|---|
| ACE 的"用户可见"定义 | 直接引入 SDCShield 文档作为覆盖度量的语义基础 |
| Little's Law + 性能计数器路线 | 真机(无 gem5)一阶 AVF 的方法论出处;对 ARM64 可用 perf/SPE 的指令吞吐与结构占用代理 |
| un-ACE 四分类(微架构) | "prediction only" 措辞更通用,便于跨 ISA 表述 |
| 全部 E18 复用项 | 见 E18 §7(本篇无独立实验增量) |

### 需替代/不可行

| 要素 | 替代方案 |
|---|---|
| 无独立实验 | 以 E18 档案 §7 的 gem5 最小配方为准 |
| 引用需求 | 若论文引用需要"AVF 原始定义",应同时引 MICRO'03(会议版,权威)与本文(杂志版,易得);SDCShield 论文中不要把本篇误标为 ISCA'05 |

## 7.5 杂志版全文逐节要点清单(6 页信息总量盘点)

以下为这 6 页 PDF 的**全部**实质性内容(已无遗漏,便于确认无需再读):

- p.70(第 1 页):单粒子翻转物理机理(宇宙射线中子/封装 α 粒子 → 电子-空穴对 → 翻转);Denver 海拔中子通量为海平面 3–5×;工艺缩小对 latch/SRAM 位错误率的影响大致对消、但芯片总错误率随位数线性增长;Sun Enterprise 服务器宇宙射线事故(丢失大客户给 IBM);Fujitsu SPARC64 80% 锁存器保护;5 类缓解手段(抗辐射电路、检错纠错、体系结构冗余)及其代价;SDC 与 DUE 的分类定义。
- p.71(第 2 页):AVF 定义(分支预测器 0% / 提交 PC 100% 两锚点);整体 SDC 率 = Σ(原始故障率 × AVF);ACE/un-ACE 位定义;结构 AVF = 平均 ACE 位占比;Little's Law 口径的引出;保守上界原则。
- p.72(第 3 页):五类架构 un-ACE(NOP、性能增强指令、谓词假、动态死、逻辑掩码)+ 四类微架构 un-ACE(idle/invalid、misspeculated、prediction-only、ex-ACE)的完整定义与例证;ACE 带宽 × 驻留 公式;性能计数器估计 AVF 的授权;Asim 框架与 IQ 实验。
- p.73(第 4 页):Figure 1(IQ 状态分解图,26 基准);IQ AVF 14–47%、EU 4–27%;Wang/Fertig/Patel 的 Y-Branches 补充。
- p.74–75(第 5–6 页):vs RTL 注入四优势(确定性单次、死值/掩码识别、成因分解、无需 RTL);迭代加保护流程;参考文献 14 条;作者简介。

**结论**:这 6 页 = E18 的方法论骨架 + 上述物理/工业背景叙事,**零新增定量实验**。对 SDCShield 的净增量:Y-Branches 引文、"prediction only" 措辞、物理背景段落(写论文 Introduction 时可引用其物理参数:中子通量海拔系数 3–5×、FIT/bit 0.001–0.01)。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

- 与 E18 完全同基线(IQ 28%/14–47%、EU 9%/4–27%),无新增数值轴。
- **对本项目的特殊价值是文献治理层面**:SDCShield 论文写作时若引用 "Measuring Architectural Vulnerability Factors" 需注明它是 IEEE Micro 2003 Top Picks(Mukherjee 等),避免审稿人发现引文与内容错位(把地址位 AVF 的内容引到这篇上是最容易犯的错,本任务清单本身就把两篇的归属弄反了——已在 E20 勘误)。

## 9. 局限与坑

1. **最大坑:文件名与内容不符**。`Measuring_architectural_vulnerability_factors.pdf` ≠ Biswas ISCA'05;两篇论文的归属在任务清单中互换了。Biswas ISCA'05 的正确 PDF 是 `Computing_architectural_vulnerability_factors_for_address-based_structures.pdf`(见 E20)。
2. 无 SMT 数据、无地址位/缓存/TLB AVF 数据(这些都在 E20)。
3. 是缩写版:配置表、Little's Law 数值表(Table 2)、程序级分解(Figure 2)全部只在 E18;只读本篇会丢失全部定量细节。
4. 数值仍为上界,保守性同 E18。
5. 2003 年的工业语境(FIT/bit 0.001–0.01、IBM Power4 目标)距今久远,绝对数值仅作历史锚点。

## 附:杂志版 vs MICRO'03 会议版内容对照(引用决策表)

两版同源,但信息分布不同。写 SDCShield 论文时按下表选择引用版本:

| 内容 | IEEE Micro 杂志版(E19) | MICRO'03 会议版(E18) | 引用建议 |
|---|---|---|---|
| AVF/ACE 定义 | 有,且 ACE 定义更完整("as observed by a user") | 有,偏形式化 | 定义引杂志版 |
| un-ACE 十分类 | 有(两张清单,措辞更通用) | 有(更详细的例证与保守规则) | 快速引杂志版,工程细则引会议版 |
| IQ/EU AVF 数值 | 范围值(14–47%/4–27%)+ Figure 1(IQ 图) | 全套:均值 28%/9%、范围、Figure 3/4、状态分解 | 数值一律引会议版 |
| 程序级分解(Figure 2,45% ACE) | 无 | 有 | 会议版 |
| Little's Law 数值表(Table 2) | 无 | 有(26 基准逐点) | 会议版 |
| Asim/Itanium2 配置细节 | 无 | 有(§5/§6) | 会议版 |
| 执行单元 bit-cell 级分析 | 无 | 有(逻辑掩码 −0.5%、DATAPATH_IDLE −1.5%) | 会议版 |
| 单粒子翻转物理背景 | 有(浓缩,含 Denver 3–5× 等参数) | 有(更详细) | 物理参数两版均可 |
| vs RTL 注入四优势 | 有(标准表述) | 有(更展开) | 杂志版即可 |
| Y-Branches 补充引文 | 有 | 无 | 杂志版 |
| Little's Law + 性能计数器授权 | 有(表述最直白) | 有 | 引杂志版更醒目 |
| SMT / 地址位 / cache / TLB 内容 | **无** | **无** | 去 E20(ISCA'05)/E21(HPCA'12) |

**一句话结论**:杂志版 = 定义与叙事;会议版 = 全部定量实验。地址类结构(任务预期的"L1 D-tag")在两版中都不存在,只在 ISCA'05(E20 档案)。

## 附:本档案的裁决记录(为何 E19 与 E20 的内容这样分)

任务清单要求"E19 = Biswas ISCA'05(地址位去耦合、full-tag 罕见 ACE、L1 D-tag 实验数据)"。核对 PDF 后的事实:

| 任务预期 | PDF 实测 | 裁决 |
|---|---|---|
| `Measuring_architectural_vulnerability_factors.pdf` = Biswas ISCA'05 | Mukherjee 等,IEEE Micro 23(6) 2003 Top Picks,无地址位内容 | E19 档案按实测内容写,标注勘误 |
| `Computing_..._address-based_structures.pdf` = Li DSN'06 | Biswas 等,ISCA'05,含全部地址位/tag/汉明距-1 内容 | E20 档案承载任务要求的全部"Biswas ISCA'05 重点" |
| E20 重点"address-bit AVF、为何 full-tag 罕见 ACE、L1 D-tag 数据" | 全部在 ISCA'05 文中 §5–§7(数据 6%/tag 0.41% 等) | 已完整记入 E20 档案 §2.2/§5.1 |

即:**任务想要的科学内容一页不少**,只是两份档案的"论文名"按 PDF 实际归属勘误。若上游索引(如 SDC_RESEARCH_SYNTHESIS 或 memory)以错误归属引用过这两篇,应同步修正。

## 附:杂志版独有的引文线索(后续可追踪)

- [12] S.S. Mukherjee et al., "A Systematic Methodology ...(MICRO-36)",即 E18 会议版。
- [13] N. Wang, M. Fertig, S. Patel, "Y-Branches: When You Come to a Fork in the Road, Take It"(PACT'03)——分支指令部分故障不致错的实例,是 AVF 上界继续下压的方向。
- [14] S. Kim, A.K. Somani(DSN'02)picoJava II RTL 故障注入——RTL 基线。
- 作者信息段:Mukherjee 时任 Intel MMDC FACT(Fault-Aware Computing Technology)项目领导人——后续 ISCA'05(E20)、HPCA'05 综述均出自该组,文献链一致。

## 附:关键定义的原文 passages(SDCShield 论文引用时的对照原文)

杂志版是 AVF/ACE 定义"最易获取的正式出版表述"(IEEE Micro 正刊,比会议版更常被引用)。写 SDCShield 论文时若引这些定义,以下给出原文与译文对照(逐句取自本 PDF):

**(1) AVF 定义(p.71)**

> "We call the probability that a fault in a processor structure will result in a visible error in a program's final output that structure's architectural vulnerability factor (AVF)."
>
> 译:结构中发生故障导致程序最终输出可见错误的概率,称为该结构的架构脆弱因子(AVF)。

**(2) 两锚点例(p.71)**

> "The branch predictor's AVF is thus 0 percent because all predictor bits are always un-ACE bits. Similarly, all the bits in the committed program counter are always ACE bits, leading to an AVF of 100 percent."
>
> 译:分支预测器 AVF 为 0%,因为其所有位恒为 un-ACE;提交 PC 的所有位恒为 ACE,AVF 为 100%。

**(3) ACE 定义(p.71,杂志版最完整的表述)**

> "To estimate AVFs, we use a new approach that tracks the subset of processor state bits required for architecturally correct execution (ACE)—any execution that generates results consistent with a system's correct operation as observed by a user."
>
> 译:为估计 AVF,我们跟踪架构正确执行(ACE)所需的处理器状态位子集——ACE 指任何生成"与用户观察下的系统正确操作一致"的结果的执行。

**(4) 结构 AVF 的平均化(p.71)**

> "Assuming that all cells have equal raw fault rates, a structure's AVF is the average AVF of its storage cells or the average fraction of its cells holding ACE bits at any time."
>
> 译:假设所有单元原始故障率相同,结构的 AVF = 其存储单元 AVF 的平均,即任意时刻持有 ACE 位的单元比例的平均。

**(5) Little's Law 口径(p.72)**

> "We translate Little's law as N = B × L, where N is the average number of bits in a processor structure, B is the average bandwidth of bits per cycle into the structure, and L is the average residence time of an individual bit in the structure."
>
> 译:Little 定律 N = B × L:N = 结构内平均位数,B = 每周期进入结构的平均位带宽,L = 单个位在结构内的平均驻留时间。AVF = B_ACE × L_ACE / 结构总位数。

**(6) 无模型估计的授权(p.72,对 SDCShield 真机量化最重要的一句)**

> "Alternatively, in many cases, we can use hardware performance counters to compute the bandwidth of ACE bits going into a structure and the average residence cycles of ACE instructions, allowing AVF estimation without a performance model."
>
> 译:很多情况下,可用硬件性能计数器计算进入结构的 ACE 位带宽与 ACE 指令平均驻留周期,从而无需性能模型即可估计 AVF。

**(7) 保守上界原则(p.72)**

> "It is difficult to precisely classify ACE and un-ACE bits over a program's entire execution. Instead, we assume conservatively that every bit is an ACE bit unless we can prove it is un-ACE. We thus compute an upper bound on the AVF number."
>
> 译:难以在程序全程精确分类 ACE/un-ACE;保守地假设每一位都是 ACE,除非能证明其 un-ACE;由此算得 AVF 上界。

**(8) SDC/DUE 定义(p.71)**

> "SDC—the topic of this article—occurs when an unprotected bit sustains a single-bit upset leading to undetected incorrect system behavior. In contrast, a DUE event occurs when an error in a bit is detected (via parity checking, for example), but the system cannot recover from that error."
>
> 译:SDC = 无保护位单粒翻转导致未检测的系统错误行为;DUE = 错误被检测(如奇偶校验)但系统无法恢复。

以上 8 段即杂志版的全部定义性内容;引用时标注 IEEE Micro, vol. 23, no. 6, pp. 70–75, Nov.–Dec. 2003(MICRO Top Picks)。

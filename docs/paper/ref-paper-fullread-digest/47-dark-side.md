# [47] 计算的黑暗面:SDC(IEEE Computer 2025 评论,Gizopoulos)

## 标题与出处

- **标题**: The Dark Side of Computing: Silent Data Corruptions
- **出处**: **IEEE Computer 杂志**("Computing Architectures" 专栏,编辑 Timothy J. Jones 与 Robert Mullins,剑桥大学),2025 年 6 月号,DOI 10.1109/MC.2025.3554306,版本日期 2025-05-29,pp.101–106。
- **类型**: **评论/立场文章(essay)**——非研究论文;SDC 领域学术领袖(雅典大学 Gizopoulos)面向工业界+学术界的公开定调之作;副题"Silent data corruptions due to defective silicon cause erroneous program results. Yet nobody knows how severe and frequent the problem is, how much we need to invest in solving it, and who should pay the bill."

## PDF 与实读范围

- **文件**: `ref/The_Dark_Side_of_Computing_Silent_Data_Corruptions.pdf`
- **页数**: 7 页(6 页内容 + 末页空),**全文实读,无截断**(txt 单次读完,395 行)。

## 作者全列(p.1)

Dimitris Gizopoulos(单作者;雅典大学信息与电信系教授,Computer Architecture Lab;SDC 学术线核心人物——digest [08]/[17]/[18]/[27]/[49]/[50] 的作者群中心)。

## 机构分列(p.1)

- **雅典大学**(University of Athens,希腊)×1——单作者单机构。

## 企业合作证据(三级)

- **一级(作者机构)**: **无**(纯学术作者)。
- **二级(致谢/资助)**: **有,明确且重要**——致谢节(p.106):"Research is supported by **Meta, AMD, and the OCP**"——其实验室受 **Meta、AMD、OCP(开放计算项目)** 三方资助;同时致谢专栏编辑约稿。
- **三级(版权页)**: IEEE Computer Society 杂志版权行(0018-9162 © 2025 IEEE)+ DOI。
- **间接层(本文主体即产业叙事)**: ① 超大规模厂商披露评述:Meta(Dixit at scale [2])、Google(Hochschild mercurial cores [3])、Alibaba(SOSP23 [4]=digest [01])、Google TPU 训练失败(He ISCA'23 [5])、Meta Dr.DNA(Ma ASPLOS'24 [6])、NVIDIA GPU 数据中心驱动文档 SDC 披露 [7];② **AMD/Intel/NVIDIA/Arm 加入 OCP** 与超大规模厂商共同推动 SDC 意识与高校合作 [10][11];③ OCP Server Resilience Initiative 的 **SDC 学术研究奖**(2024-06 公布)[10];④ **Meta Research 2022 年"SDCs at scale"研究提案资助(RFP)获奖者公告** [9]——Meta 资助学术界的直接机制证据;⑤ Google TPU v5p 博客(Vahdat/Lohmeyer [20])、Meta MTIA v1 博客 [21];⑥ Intel 主题演讲(Hesley, ITC [14]);⑦ ISCA'22 SDC 午餐小组 [13]、IEEE CS RAS 数据中心峰会 2024 [12]。**判定:二级企业合作(Meta+AMD 资助)+ OCP 产业联盟资助;且本文本身就是"学界对业界披露的权威综述评述",是综述企业生态叙事的核心二手文献**。

## 核心结论

1. **开篇定调(专栏副文)**: 硅缺陷损坏程序数据而我们不自知——"没人知道问题多严重多频繁、需要投入多少解决、**谁来买单**"——本文把 SDC 问题从技术问题升格为**产业经济学问题**(谁付费)。
2. **两大属性与成本观(正文)**: 计算之美与力来自两个用户需求——#1 正确(域相关:"9,753"精确值 vs "9,500–9,800"区间皆可)+ #2 快(硬实时域=deadline/生命财产;其他域=尽可能快);两者都不免费,设计/制造/运营全程都要钱——**正确性与速度的工程权衡**是全文框架。
3. **正确性的三重威胁窗口(§设计/制造/服役)**: 设计期(硬件 bug)、制造期(硅缺陷、物理参数涨落)、服役期(老化、辐射)——SDC 源头画像的三段时间轴。
4. **制造测试的天然上限(§硅缺陷)**: 验证新设计/验证少量样片/全面测试每片芯片都有紧时间与成本约束,产品 6 个月–1 年交付;"**即使最勤勉的制造测试流程也只能检出所建模缺陷的 95%–99%**";且服役期还有新缺陷(老化/工况)——**测试逃逸是常态而非事故**。
5. **缺陷→程序输出的四条件传播链(Fig.1)**: 以整数乘法器内一只缺陷 OR 门为例(制造不完美→双 0 输入时输出 1,其余三种组合正确;持续型或罕见输入依赖型):缺陷影响程序当且仅当——①程序使用乘法指令;②某次乘法的操作数恰使 OR 门双输入为 0;③缺陷门到乘法器输出存在逻辑传播路径;④错误乘法结果经数据流/控制流伤害执行(挂起/崩溃/异常或**错误结果**——如图:把猫识别成狗,92% Dog vs 95% Cat)。任一条件不满足则缺陷"无关紧要"——**SDC 是概率链式事件**,与 AVF 理论(R2 批次)同构;CPU/GPU/DSP/加速器(含 DRAM)的每个单元(寄存器/缓存/缓冲/控制逻辑/算术单元)皆可含缺陷。
6. **可见 vs 静默(§Visible and Silent)**: 可见错误(异常/崩溃)→我们知道→重试/修理/换机;**SDC=程序在缺陷芯片上完整跑完、不崩不炸、输出不显然错、但结果错且无人知晓**;检测很贵:内存硬件 ECC **2%–125% 硅面积与功耗**、软件冗余执行(双/三模)**100%/200% 性能与能耗**——"谁ready接这个价?"
7. **术语史考据(§SDC Disclosures)**: "SDC 术语的系统化使用很可能始于 **DSN 2008 小组讨论**——panelists 在争论它存在还是神话" [1](Constantinescu 等,Intel 作者群);"几十年来我们相信 SDC 是神话,或最坏情形只影响百万/十亿分之一的倒霉用户"——**从神话到 1/1000 常态**的认知转变是本文的历史叙事主轴。
8. **超大规模厂商披露汇总(Table 1)**: Meta:"数十万台机器中的数百个 CPU"→约 **1000 DPPM**;Google:"数千台机器中的少数 mercurial cores"→**<1000 DPPM**;Alibaba:"3.61%…的 CPU 被识别致 SDC"(文本层如此;按表格 DPPM 解释=**361**)→即 3.61/万(0.0361%);根因一致:**先天缺陷(逃逸制造测试)、后天致损(老化)、个体差异(涨落)**。
9. **术语纪律(§短注)**: "SDC 既不被检测也不被纠正——缺陷效应一旦被检测就不再静默";按容错计算术语:检测缺陷(故障模型)、纠正其效应(错误)于不同抽象层——对"SDC 检测方案/SDC 被纠正"一类公开表述的术语纠偏。
10. **认识论核心(§开放问题)**: 开放问题清单——真实 SDC 率?哪些微架构?哪些硬件单元(尺寸/数量/设计)?哪些指令更易?哪些工艺节点?;**"测量 SDC 数量"是术语矛盾——静默即不可观测;使命只能是"估计(estimation),因为永远无法测量(measurement)"**——一阶估计(微架构级故障注入于 x86 CPU 模型,算术单元:标量/向量、整数/浮点)显示紧迫性 [8]:Fig.2 给出五档数据中心配置(100K–750K 核、15–60 分钟负载、750–2000 DPPM)的**每日 SDC 数估计:97/152/61/292/364**——"CPU 机群每天产出数百个错误程序输出送达到毫不知情的用户!"。
11. **缓解手段与成本(Table 2)**: 四条路——更好的制造测试(逃逸更少;测试时间贵)、降低芯片涨落(边际芯片不部署;良率降、芯片贵)、增强硬件容错(任务态缺陷检测/纠正;设计/验证/面积/功耗/性能开销)、增强软件容错(错误计算检测/纠正;性能/能耗开销)——"**Everything costs; who is going to pay?**"。
12. **产业披露的边界(§More Disclosures?)**: 赞 Meta 首次披露并资助学界 [9]、Google/Alibaba 跟进确认、AMD/Intel/NVIDIA/Arm 加入 OCP;但**悲观预期**:公司不会提供更详细有用的信息,两个理由——①公司靠"感知为好"的产品特性盈利:性能第一、功耗重要、**"可靠性(更低故障率)很少(从不!)是卖点;机器数月一坏甚至产出无人察觉的损坏输出的恐惧是可怕的——没人谈论自己系统产生的错误"**;②故障率/SDC 率**无法定义、测量、复现**——不像速度/功耗主张可复现验证("若公司称其芯片在百万片中每年产生一个 SDC,如何验证?")。
13. **SDC 进成本模型(§SDC-Awareness in Cost Models)**: 是时候把 SDC 率纳入数据中心成本模型——**高 SDC 率的租用时间应更便宜(用户自担结果完整性),保证"低 SDC 率"的时间服务商担保正确性、可以收更贵**;芯片厂商/超大规模厂商/用户三方谁买单的问题;SDC 率应同时进入运营商侧与用户侧的成本方程——**SDC 定价化**是本文最具产业颠覆性的提议。
14. **数据并行架构的更坏预期(§SDCs from Data Parallel)**: 本文聚焦 CPU 因披露集中在 CPU,但"谁会指望大规模数据并行架构(GPU/可编程 AI 加速器 [20][21])更好?其以数据为中心的架构与 ML/AI 海量部署**直觉上指向巨量 SDC 率**"。
15. **三大研究方向(§Practical Research Directions)**: ①**真实 SDC 率估计**(不只数芯片——千分之一的缺陷芯片若缺陷单元不被用到可产生零 SDC,若被负载重度使用可日产百万 SDC;理想估计须关联指令/负载类别以辅助软件容错);②**数据中心机群的有效周期性扫描**(制造测试持续改进;现场持续筛查能抓住短促制造测试抓不到的复杂真实 SDC 场景;检出缺陷芯片并更换);③**硬/软件层容忍**(大规模基础设施难以负担大冗余;**运营商知道芯片缺陷后唯一负责任的动作是更换**;唯一有意义的替代是**已知且受控缺陷的降级运行**——缺陷单元不被负载使用时)。
16. **结语(§全栈动员)**: 攻坚 SDC 需要全计算系统栈贡献——物理设计、逻辑与微架构设计、指令集架构、系统软件、编译器、应用软件;引 Hennessy/Patterson"计算机体系结构黄金时代"[22]之名,宣告我们同时活在"**可信计算的黄金时代(Golden Age of Dependable Computing)**"。

## 分类学标注

- **SDC 核心特征**: 静默性的认识论(不可测量只可估计);概率链式传播(四条件);三段时间轴根因(设计 bug/制造缺陷+涨落/老化+辐射);"从神话到 1/1000"的严重度叙事。
- **根因机理分类**: 先天(测试逃逸)/后天(老化)/个体差异(涨落)三分法——与产业披露口径一致,是综述根因分类的权威框架。
- **故障模式**: 门级缺陷→功能单元→指令→程序输出;输入依赖型(罕见激活条件)缺陷模式。
- **检测技术**: 制造测试改进(95–99% 上限)+ 现场周期性扫描(机群筛查)——检测的两大阵地;检出即不再静默的术语纪律。
- **处理技术**: 更换(唯一负责任动作)/降级运行(已知受控缺陷、缺陷单元不被使用)/硬软件冗余容错(昂贵、大基础设施难负担)/**SDC 率定价**(经济学的"处理")。
- **生命周期**: 全生命周期视角(设计→制造→服役)——53 篇中生命周期覆盖最完整的文本之一。
- **两界关系**: 学界(估计/扫描/容忍研究)+ 产业界(披露/资助/联盟)的动员状态;**成本与付费方**是贯穿性问题——综述"两个世界"章节的纲领性文献。

## 业界观点摘录

1. "Yet nobody knows how severe and frequent the problem is, how much we need to invest in solving it, and who should pay the bill."(副文——问题三问:多重?投多少?谁买单?)
2. "Hyperscalers (Meta, Google, Alibaba) have disclosed over the last few years an unexpectedly high number of CPUs (1 in 1000) that lead to SDCs"(§披露——1/1000 定量的权威转述)。
3. "reliability (reduced failure rates) is rarely (never!) a selling point. The fear that a machine may fail even once every many months, or, even worse, it may produce an unnoticed corrupted output, is terrifying. **Nobody talks about the errors their system generates.**"(§More Disclosures——可靠性非卖点的产业心理,最直白的表述)。
4. "We can't measure something that we can't observe because it is silent. So, essentially the mandate changes to: 'Estimate the number of SDCs because you can never measure them.'"(§开放问题——估计论)。
5. "It is probably time for hyperscalers to take the rates of SDCs into consideration in cost models... For a potentially 'high SDC rate' data center rental time, users should deal with the integrity of the program results themselves (and thus pay less). On the other hand, a guaranteed 'low SDC rate' data center time means that the provider takes care of the correctness of the delivered results and, for this reason, can charge more."(§成本模型——SDC 定价化提议)。
6. "When a data center operator knows a chip is defective, the only responsible action is to replace it. The only meaningful alternative would be the degraded operation of a chip with a known and contained defect in a hardware unit which is not used by workloads."(§研究方向——更换为纲、降级为例外)。
7. "we also live in the 'Golden Age of Dependable Computing'"(结语——时代命名)。

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 95%–99% | 最勤勉制造测试对所建模缺陷的检出率上限 | §硅缺陷 |
| 1 in 1000 | 超大规模厂商披露的 SDC 易感 CPU 比例 | §披露 |
| ~1000 / <1000 / 361 | Meta / Google / Alibaba 披露的 DPPM 解释 | Table 1 |
| 2%–125% | 内存硬件 ECC 的硅面积/功耗开销区间 | §Visible and Silent |
| 100% / 200% | 软件 double / triple 冗余的性能/能耗开销 | §Visible and Silent |
| 97 / 152 / 61 / 292 / 364 | 五档数据中心配置的每日 SDC 估计数(100K–750K 核,15–60 min,750–2000 DPPM) | Fig.2 |
| 4 | 缓解路线数(制造测试/降涨落/硬件容错/软件容错) | Table 2 |
| 4 | 缺陷影响程序的必要条件数(指令/操作数/传播路径/输出伤害) | §硅缺陷+Fig.1 |
| 3 | 提出 20+ 年的开放问题组数(率/微架构/单元/指令/节点) | §开放问题 |
| 3 | 实践研究方向数(率估计/机群扫描/容忍) | §研究方向 |
| 6 层 | 全栈动员层级(物理设计→应用软件) | §结语 |
| DSN 2008 | SDC 术语系统化使用的起点(Constantinescu 等,Intel) | §披露 [1] |
| 2022 / 2024 | Meta SDC at scale RFP / OCP SDC 学术研究奖 | [9]/[10] |

## 方法论要点

1. **评论文体的证据纪律**: 所有产业数字均给出处(披露引文+DPPM 解释两栏对照)——二手综述的可信度做法。
2. **一阶估计的定位**: 明确标注估计(微架构级故障注入)与测量的认识论区分——不可测之物只能估计,且估计必须关联负载类别。
3. **成本-收益框架**: Table 2 每条缓解路线"为何降低 SDC/为何增加成本"两栏对称——工程决策的经济学显式化。
4. **术语学梳理**: DSN 2008 词源考据 + "检出即非静默"的语义纪律——为综述提供术语史锚点。
5. **产业心理分析**: 披露不足的两因(非卖点+不可复现验证)——比"产业不透明"的朴素指责更深一层的机制解释。

## 横向对比注记

1. **与 digest 群的直接互证**: [4]=[01] SOSP23(Alibaba)、[15]=[49] stealthy-saboteurs IOLTS'23、[16]=[50] iolts24-quantify、[17]=[27] harpocrates——本文是 R7/R8 批次的**枢纽文本**:Gizopoulos 学派(Papadimitriou 等)+ Gurumurthi(AMD/弗吉尼亚大学)学术圈的自我综述;**Task 10 须核对 [27] 的 venue**(本文标注 ISCA 2024 pp.516–531,doi 10.1109/ISCA59077.2024.00045;INDEX 现标 micro26,二者必有一误——可能 INDEX 误标或同系列两篇)。
2. **vs [49]/[50](同作者群前作)**: 47 是 Computer 杂志版的大图景评述(经济学+全栈动员),49/50 是 IOLTS 的技术性量化——同一学派的"学术版/产业版"双轨输出。
3. **vs [01] SOSP23**: 本文转述 Alibaba 披露"3.61%…的 CPU"并解释为 **361 DPPM**=3.61/万(0.0361%)——文本层的"%"疑为"‰"或"/万"的提取损坏;**这直接支持 Task 10 的单位复核**:若 SOSP23 原文为 3.61/万,则 note 01 的"3.61‰"(=3610 DPPM)应更正为 3.61/万。
4. **vs [08] tc23-micropersp(Gizopoulos 自己的 TC'23)**: 08 是微架构视角的技术综述,47 是产业经济视角的评论——同作者的问题意识从"怎么发生"扩展到"谁买单"。
5. **Meta 资助链条闭环**: Meta Research 2022 SDC RFP [9] → Veritas HPCA'25([04],digest)/Dr.DNA ASPLOS'24([6])/Ripple——本文致谢"Research is supported by Meta, AMD, and the OCP"把 R1 批次观察到的 Meta-学界合作(digest 04 Veritas 致谢 Meta RFP)上升到资助人自述;**Meta 通过 RFP 定向孵化 SDC 学术研究**是综述企业生态的关键机制。
6. **OCP 产业联盟**: AMD/Intel/NVIDIA/Arm+超大规模厂商在 OCP 的 Server Resilience Initiative 与 SDC 学术研究奖(2024)——**产业界集体行动的制度化证据**;对综述"业界观点"章节:披露(Meta/Google/Alibaba)→确认(AMD/Intel/NVIDIA/Arm 加入)→资助(OCP 奖)三阶段。
7. **vs R6 三生态零企业(40–43)**: 同期欧洲学界靠政府资助做容错,而 Gizopoulos 学派(希腊/德国线)拿 Meta/AMD/OCP——**欧洲内部两条资助路线**;综述企业合作地图需区分"美英产业耦合"与"欧陆政府资助"两种模式。
8. **vs [44] recommendation(Meta 合作)**: 47 的"数据并行架构直觉上指向巨量 SDC 率"[20][21] 与 44 的 DRS 鲁棒性实证构成"预言-验证"关系;Dr.DNA [6](Ma/Jiao ASPLOS'24)是 44 的同一 Villanova 组前作——44 的 Meta 合作谱系再延伸。
9. **四条件传播链 vs AVF(R2)**: Fig.1 的缺陷激活四条件是 AVF 思想的科普化表达——面向杂志读者的理论再包装;综述可用其作为"AVF 概念的通俗表述"引文。
10. **SDC 定价化提议的独特性**: 53 篇中唯一把 SDC 率作为**经济商品属性**(可定价、可差异化服务)提出的文本——综述"业界观点"章节的独家素材;与云服务 SLA 体系(可用性/延迟)的历史对照可作展望。

## 身份核实

- **IEEE Computer 杂志 2025 年 6 月号**,"Computing Architectures"专栏,DOI 10.1109/MC.2025.3554306,pp.101–106;7 页 PDF(末页空);全文实读。
- **单作者**(Gizopoulos,雅典大学);作者简历块(p.106)确认单位与联系方式。
- **致谢节(p.106)明确**: "Research is supported by **Meta, AMD, and the OCP**"——**二级企业资助证据确凿**(两公司+一产业联盟);专栏编辑约稿致谢一并记录。
- **文本层问题**: Fig.2 柱状图数值(97/152/61/292/364)与坐标标签(100K/300K/250K/500K/750K Cores;15/30/45/60 Mins;750/1000/1250/2000 DPPM)交错,已按柱序-标签序对齐重建,具体柱-配置映射需回 PDF 核对;Table 1 Alibaba 引文"3.61%"与 DPPM 解释 361 的单位矛盾(见横向对比 3,Task 10 处理);参考文献 16 的作者名"Macieira"及 venue 细节与 digest [50] 待 Task 10 互核;[17] harpocrates 的 ISCA'24 标注与 INDEX 27 的 micro26 标签冲突(Task 10 处理)。
- **类型判断**: 评论文章(非同行评审研究论文,杂志专栏约稿)——引用其中数字时应回溯其一手来源([2][3][4] 等披露原文);但本文本身是**产业披露与学界议程的权威二手综述**,综述引用其"观点/框架"时地位等同一手。
- **内部一致性**: 1/1000 与 Table 1 Meta 1000 DPPM 自洽;95–99% 检出率与"测试逃逸是常态"论证自洽;未发现矛盾。
- **引文链核对**: [4]=[01]✓、[15]=[49]✓、[16]=[50]✓、[17]≈[27](venue 冲突待核)、[2]=Dixit、[3]=Hochschild、[5]=He ISCA'23(**Google TPU 线**——本文 [5] 列于 Google 加速器语境,佐证 note 44 中 [8] 的机构归属应为 Google 而非 Meta,已修正 note 44)。

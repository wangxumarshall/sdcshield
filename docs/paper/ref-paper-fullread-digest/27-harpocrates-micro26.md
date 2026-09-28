# [27] Harpocrates: Breaking the Silence of CPU Faults through Hardware-in-the-Loop Program Generation(ISCA 2024,印刷页 516–531)

- PDF: `Harpocrates_Breaking_the_Silence_of_CPU_Faults_through_Hardware-in-the-Loop_Program_Generation.pdf`;页数 17(实读全部:p.1–16 即印刷页 516–531,末页空白;DOI 10.1109/ISCA59077.2024.00045,979-8-3503-2658-1/24,© 2024 IEEE)——**即 [26] IEEE Micro 2025-12 "Harpocrates++" 的会议原文**;**INDEX slug `harpocrates-micro26` 与 [26] 的 slug `harpocrates-isca24` 恰好互换(两 PDF 文件名亦互换),为历史命名错误——保持 slug 稳定以追溯,本篇实为 ISCA'24 原文**
- 作者(全列):Nikos Karystinos、Odysseas Chatzopoulos、George-Marios Fragkoulis、George Papadimitriou、Dimitris Gizopoulos、Sudhanva Gurumurthi(共 6 人)
- 机构(学术/企业分列):
  - 学术:雅典大学信息与电信系计算机体系结构实验室 ×5(Karystinos/Chatzopoulos/Fragkoulis/Papadimitriou/Gizopoulos,邮箱 @di.uoa.gr)
  - 企业:**AMD 公司 RAS Architecture 部门(Austin, Texas)×1——Gurumurthi(AMD Fellow)共同署名,本集 AMD 共同署名的起点(ISCA'24 原文即开始,[26] Micro'25 版延续)**
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Gurumurthi 挂 "RAS Architecture, Advanced Micro Devices, Inc, Austin, Texas, USA"(p.1)— **AMD 共同署名**
  - 二级(致谢章节资助/数据/设备):致谢(p.528)— **AMD 与 Meta 研究赠礼** + 欧盟 Horizon Europe 三个项目:**Vitamin-V(101093062)、NEUROPULS(101070238)、REBECCA(101097224)**(与 [22]/[23] 的 NEUROPULS+REBECCA 重叠);感谢 **Ramon Bertran(IBM Research)** 与 ISCA 2024 审稿人
  - 三级(首页脚注资助声明):无首页脚注,但有**末页 AMD 版权声明**(p.528:"© 2024 Advanced Micro Devices, Inc. All rights reserved." + AMD 商标声明 + "This paper reflects collaborative work between the authors.")— 企业共同作者论文的版权页形态
  - 间接层:MicroProbe(IBM GitHub [66])、SiliFuzz(Google GitHub [28])、OpenDCDiag(Intel GitHub [30])、**AMD OpenFieldHealthCheck(GitHub [31])**——四个被评测/使用的开源工具中三个直接来自企业(IBM/Google/Intel/AMD 各一);[56] Lerner 等 Intel ITC'22 OpenDCDiag 测试优化论文被引;§I 援引 Wang et al. SOSP'23 [3] "公司内部测试程序不公开,但也使用 SiliFuzz 和 OpenDCDiag 测试 fleet"
- 核心结论(各带节号/图表号):
  1. **DPPM 产业背景量化(§I,Fig.1)**:Meta 2021.1 披露 ~1,000 DPPM("数十万台机器中检测到数百颗 SDC CPU");Google 2021.6 ~1,000("数千台机器中数颗 mercurial core");Meta 2023.10 披露 361("3.61 CPUs per 10,000");**安全关键域(汽车)要求 <10 DPPM,云/HPC 可承受数百 DPPM**——脚注 1:ISCA/DAC/ITC 近期面板均强调极低 DPPM 需求
  2. **方法核心(§I/§IV)**:约束随机功能性测试程序生成 + **硬件在环**(hardware-in-the-loop)——用 gem5 微架构仿真引擎作评分/精化反馈;三组件:Generator(初始种群)、Mutator(指令替换突变)、Evaluator(gem5 评分),类遗传算法循环(种群→交叉/突变→适应度选择);**区别于 SiliFuzz 的关键:SiliFuzz 硬件无关(字节序列突变+代理软件覆盖率),Harpocrates ISA 感知+硬件感知**(§IV-A,Fig.8 示例:SiliFuzz 变异字节 >2/3 不可用,Harpocrates 内部表示保证合法)
  3. **覆盖度量与故障类型配对(§II-D)**:bit-array 结构(IRF/L1D)用 **ACE 生命周期分析**(瞬态故障的上界代理);功能单元用 **IBR(Input Bit Ratio,输入位比)**(永久门级故障的"锻炼度"代理,非上界);Fig.2 故障类型关系图——永久=瞬态子集(存在周期=程序全周期)、间歇介于其间;"检测所有瞬态的程序也很可能检测另两类"
  4. **基线实测——开源套件检测能力短板(§III-C,Fig.4–6)**:IRF 几乎全部 <5%;L1D 最高单程序(OpenDCDiag)>80%;整数加法器平均 ~80%(最佳 99%/98%);整数乘法器平均 MiBench 53%/SiliFuzz 70%/OpenDCDiag 37%(最高 87%/67%/58%);SSE FP 单元多数负载零利用(仅 4 个 MiBench + 半数 OpenDCDiag 非零),OpenDCDiag 最高 SSE FP 加法器 98.5%/乘法器 58.2%(FP 密集负载 MxM/SVD 所致)——**覆盖率恒高于检测率(ACE 上界性质),差距=软件掩蔽,改进空间巨大**
  5. **SiliFuzz 有效生成率实测协议(§VI-A,本篇独有细节)**:Unicorn 代理模糊 40 分钟 → 635,587 输入 → 5 分钟排序 → **173,731 个可运行确定性程序(约 1/3 保留率)** → 平均 18.6 指令/程序 → 共 3,230,528 条指令/45 分钟 = **1,200 指令/秒**;Harpocrates:单循环 13.35 秒(突变 0.51s+生成 9.18s+编译 1.12s+评估 2.54s,Table I)× 96 程序 × 5K 指令 = 480,000 条 = **36,000 指令/秒 = 30× SiliFuzz**;直接用 SFI 精化单次迭代需数小时(不实用,故用覆盖代理)
  6. **六结构收敛与检测终值(§VI-B/§VI-C,Fig.10/11)**:IRF 10K 指令、96 程序/代(96 线程满并行)、top-16 × 6 突变,~5,000 迭代收敛,覆盖率为其他框架最佳者 3×;L1D 30K 指令、8 字节 stride 遍历 32KB 区域(=L1D 容量),~2,000 迭代收敛至 95% 覆盖(初代即 77%,缓存感知约束所致);整数加法器/乘法器 5K 指令、32 程序 top-8 × 4,~1,000 迭代(全程 <2 小时),IBR 10%/6%;SSE FP 加法器/乘法器 ~5,000 迭代,IBR 7%/5%,**检测能力在 500/600 步即近 100%(远早于覆盖收敛)——XMM 寄存器访问指令少,故障传播被后续操作掩蔽的概率低**;SSE FP 两单元覆盖率超其他框架 10×
  7. **检测终值对比(§VI-C,Fig.11)**:IRF ~10× 其他框架;L1D 接近 90%(OpenDCDiag 最佳 ~80%);整数乘法器 ~100%(SiliFuzz 最高 87%);SSE FP 加法器 99.8%(OpenDCDiag 异常值 98.5%);SSE FP 乘法器 99.7%(OpenDCDiag 最高仅 58.2%)——**唯一在所有功能单元都接近全检测的框架**
  8. **检测速度(§VI-C)**:整数加法器 99% 检测仅需 50,000 周期,最佳 MiBench 程序需 >11M 周期(**220×**);同运行时间下 99.5% vs SiliFuzz 最佳 86.6%
  9. **瞬态-永久检测不对称(§VI-B)**:bit-array 瞬态故障对所有框架都远难于 FU 永久故障——瞬态易被覆写、永久持续全程;IRF/L1D 微架构化动态行为复杂,FU 由特定指令直接驱动;收敛难度同步体现(FU 1,000 循环 / L1D 2,000+ / IRF 5,000)
  10. **覆盖-检测正相关的核心洞察(§VI-B)**:六组件全部显示"覆盖提升→检测提升",精心配对的覆盖度量(ACE↔bit-array 瞬态、IBR↔FU 永久)与实测检测能力正相关——**Harpocrates 程序的覆盖-检测差距极小=软件掩蔽可忽略(生成器精细参数化所致);MiBench/SiliFuzz/OpenDCDiag 显著软件掩蔽**
  11. **意外发现:gem5 RCR 指令模拟 bug(§VI-D)**:Harpocrates 生成程序暴露 gem5 v22.0.0.2 内部断言错误——根因为 RCR x86 指令仿真 bug(rotate amount == 寄存器尺寸的 corner case),后续版本已修复([67] gem5 commit 1dd3072)——**生成式方法的副作用价值:仿真器自身的 bug 猎手**
  12. **生命周期用途与灵活性(§IV-B)**:任何 ISA(gem5 支持 x86/Arm/RISC-V)、任何微架构结构(注入基础设施覆盖 >70 种硬件结构)、任何质量度量、任何故障模型皆可配置;三类用例:**Ripple**(在产周期快扫,约束短时长)、**Fleetscanner**(离产全面扫描,无时限拉满检测)、**预硅边缘缺陷筛选**(温度/电压/特定故障类型聚焦);§VII:与功耗/dI/dt 压力测试病毒研究([92]–[99])正交——那些是"最坏情况指令序列"触发边缘缺陷的前奏,Harpocrates 突变长而多样的指令序列提升覆盖
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:背景性列举(制造缺陷/边缘性缺陷/粒子翻转/设计 bug/老化/恶意攻击,§I)——论文目标为检测而非根因分析;§VII 定位边缘缺陷(marginal defect)为"现今制造与封装测试最难检测者",驱动全生命周期连续故障筛查
  - 故障模式类型:**瞬态(bit-array,易覆写)vs 永久(FU 门级 stuck-at,持续)检测能力不对称**;IRF/L1D 微架构动态行为 vs FU 指令驱动的结构差异;软件掩蔽作为检测率低于覆盖率的模式层解释
  - 检测技术类型:**功能性测试程序生成(约束随机+硬件在环+遗传式迭代)——检测技术家族方法论核心论文**;被动监测(输出比对/崩溃监测)为执行侧机制;与 SiliFuzz(fuzzing 代理)构成"硬件无关 vs 硬件在环"路线对照
  - 处理技术类型:无(检测导向);冗余(双工/三模)作为背景成本论述(§II-A);生命周期三类用例(Ripple/Fleetscanner/预硅)为检测的部署形态而非容错
- 业界观点摘录:
  - Fig.1/§I:三家 hyperscaler DPPM 披露(Meta 1000/Google ~1000/Meta 361);"可接受的 DPPM 取决于领域:安全关键(如汽车)要求 <10,通用云计算/HPC 可承受数百"——"Fig.1 的 DPPM 数值号召计算社区行动起来验证这些比率并更好理解根因"
  - 脚注 1:"实现极低 DPPM 的需求在近期 ISCA/DAC/ITC 会议面板上被反复强调"
  - §III-A:"行业贡献的开源框架(经 OCP 产业级努力公开发布,[55])……我们的愿景是 Harpocrates 成为这些努力的主要贡献者"
  - §III-A 引 Wang et al. SOSP'23 [3]:"公司的内部测试程序不公开,但也使用 SiliFuzz 和 OpenDCDiag 测试 fleet"——**业界同时使用产业开源工具+自研闭源工具的双轨证据**
  - §III-B:"hypercaler 报告明确将浮点单元与其整数对应单元(标量与向量硬件)列为大型 fleet 中极可能的 SDC 源" [3]——SSE FP 两单元入选六结构的产业依据
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | DPPM(三家披露) | Meta ~1000(2021.1)/Google ~1000(2021.6)/Meta 361(2023.10) | §I, Fig.1 |
  | DPPM 可接受域 | 安全关键 <10;云/HPC 数百 | §I |
  | IRF 基线检测 | 几乎全部 <5%;Harpocrates ~10× 其他框架 | §III-C/§VI-C, Fig.4/11 |
  | L1D 基线最佳 | OpenDCDiag 单程序 >80%;Harpocrates 接近 90% | §III-C/§VI-C, Fig.4/11 |
  | 整数乘法器基线 | 平均 MiBench 53%/SiliFuzz 70%/OpenDCDiag 37%;最高 87%/67%/58% | §III-C, Fig.5 |
  | SSE FP 基线 | 加法器 OpenDCDiag 98.5%/乘法器 58.2%(多数负载零) | §III-C, Fig.6 |
  | Harpocrates 检测终值 | SSE FP 加法器 99.8%/乘法器 99.7%/整数乘法器 ~100% | §VI-C, Fig.11 |
  | SiliFuzz 生成率 | 1,200 指令/秒(40 分钟 Unicorn → 635,587 输入 → 173,731 保留 → 18.6 指令/程序) | §VI-A |
  | Harpocrates 生成率 | 36,000 指令/秒 = 30× SiliFuzz(单循环 13.35s:0.51+9.18+1.12+2.54) | §VI-A, Table I |
  | 单循环规模 | 96 程序 × 5K 指令 = 480,000 条 | §VI-A |
  | 检测速度 | 99% 检测 50,000 周期 vs MiBench >11M(**220×**);99.5% vs SiliFuzz 86.6% 同时长 | §VI-C |
  | 收敛迭代数 | FU ~1,000 / L1D ~2,000 / IRF ~5,000 | §VI-B, Fig.10 |
  | x86-64 指令支持 | ~2,000 指令变体(贴近 gem5 支持范围) | §V-B |
  | 注入结构数 | >70 种硬件结构(gem5 注入基础设施) | §IV-B |
  | 实验平台 | 双路 AMD EPYC 7402(24 核 96 线程)128GB DDR4 | §VI |

- 方法论要点:硬件在环=用微架构仿真器(而非软件代理)作生成反馈环——覆盖代理(ACE/IBR)快测+SFI 慢测终评的两级评估;覆盖度量必须与故障类型-结构配对(ACE↔bit-array 瞬态/IBR↔FU 永久),错误配对则反馈失真;指令替换突变(均匀随机选一条指令的全部出现替换)优于 k-point 交叉与过窄的定向策略(避免局部最优与探索空间塌缩);x86-64 生成难点清单(寻址模式/隐式操作数 MUL→RAX/栈对齐 16B ABI/非确定性指令排除)为向 CISC ISA 移植程序生成的工程指南;C wrapper(内联汇编+初始化+输出签名)保证确定性端态;种群规模=硬件线程数(满并行);SFI 直接进精化环不实用(单迭代数小时)。
- 横向对比注记:**与 [26] 构成"会议原文(ISCA'24) + 杂志扩展(Micro'25)"对**——[26] 相对本篇的增量:第七结构 LSQ(最佳 20%,其余 0–1%)、slices 实验(0.1×/0.01× 配比)、随机性方差分析(IRF 2%/L1D 5%/整数乘法器 17%)、致谢新增 OCP、"50+ 注入目标"表述;**本篇相对 [26] 的独有内容:MuSeqGen 完整架构(MicroProbe 双模块/pass-policy/合成器)、x86-64 移植工程细节、SiliFuzz 生成率实测协议(635,587→173,731 的 1/3 保留率)、各组件收敛参数与种群配置、220× 检测速度、gem5 RCR bug 发现、AMD 版权声明页**;**"50+([26]) vs >70(本篇 §IV-B)"注入结构数表述差异如实记录**——可能为 gem5 版本或口径差异,非矛盾;**致谢包对照**:本篇 AMD+Meta 赠礼 + Vitamin-V/NEUROPULS/REBECCA(无 OCP)→ [26] 增 OCP;[26] 的 [8] 即本篇(作者列表含 Papadimitriou,Micro 版无——Papadimitriou 于 Micro'25 版退出作者列表,客观差异);基线数值(IRF<5%/L1D>80%/30×/220×/99.5%–86.6%)与 [26] 完全一致(跨版本一致性自检通过);**OpenDCDiag(sdcshield 父项目)在 ISCA'24 原文即作为三大基线之一被正式评测**——L1D 最佳单程序 >80% 检测、SSE FP 加法器 98.5% 异常值、整数乘法器仅 37% 平均/58% 最高;[28] SiliFuzz(Google [29])与 [30] OpenDCDiag(Intel)均以 GitHub 仓库为引;[3]=[01] SOSP'23、[1]=Dixit arXiv 2102.11245、[2]=Hochschild HotOS'21、[11]=[03] Ripple、[7]=[15] DelayAVF(Gurumurthi 桥接)、[40]=[21] diff-fi(GeFIN 工具被引于 [40] 而非 [21]——与 [24] 相同的组内引用习惯)、[4]=[08] TC'23、[5]/[6]=[49]/[50] IOLTS/VTS 2023、[33]=[03];MicroProbe=[10](IBM MICRO'12);**Athens-AMD-Meta 轴:本篇为 AMD 共同署名+AMD/Meta 赠礼双证据的最早论文(2024.06),轴心从"纯赠礼"([23] 2023)升级为"赠礼+署名"的关键节点**;§VII 正交性论述(压力病毒=触发前奏/Harpocrates=覆盖提升)与 [25] SiliFuzz 的 quarantine(频率/电压/温度扫描)互补定位呼应。
- 身份核实:标题"通过硬件在环程序生成打破 CPU 故障的沉默"与内容(三组件硬件在环生成+六结构评测+对比三基线)完全相符;ISCA 2024、DOI 10.1109/ISCA59077.2024.00045、印刷页 516–531 齐全,名实相符;**文件名/slug 与内容错位如实记录:本篇文件名即 ISCA'24 标题(内容相符),而 [26] 的文件名(题为 "Automated Functional Program Generation Against CPU Faults…")对应其实际内容 Micro'25 扩展版的另一标题——两 PDF 的文件名与 slug 恰好互换,INDEX slug 保持稳定以追溯,笔记均已按实际内容标注出处**。作者 6 人与 [26] 所引本篇([8])作者列表一致(含 Papadimitriou)。**文本层已知损失**:(1) Fig.4–6/10–11 为点图/柱图,数值均由散文完整给出(损失可忽略);(2) Fig.1 DPPM 图三轴数值散文完整给出;(3) 页眉 "Authorized licensed use…Shanghai Jiaotong University" 下载戳为访问来源标记;(4) 个别参考文献编码乱码("�");(5) 末页(p.17)空白;(6) **无内部矛盾**——所有关键数值(30×/220×/99.5%–86.6%/IRF<5%/L1D>80%/收敛迭代数)与 [26] Micro'25 版交叉一致;40 分钟+5 分钟=45 分钟、635,587×(1/3)≈173,731(实际 27.3%,与"only about one-third"一致)、13.35s 四步分解求和=13.35 ✓、96=16×6、32=8×4 种群算术自洽。

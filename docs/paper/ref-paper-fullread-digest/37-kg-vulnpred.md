# [37] 基于异质 SDC 传播知识图的高效指令脆弱性预测(TDSC 2026)

## 标题与出处

- **标题**: Efficient Instruction Vulnerability Prediction With Heterogeneous SDC Propagation Knowledge Graph
- **出处**: IEEE Transactions on Dependable and Secure Computing (TDSC), vol. 23, no. 1, pp. 1173–1190, Jan./Feb. 2026。DOI 10.1109/TDSC.2025.3614343。收稿 2024-04-16,修回 2025-09-10,录用 2025-09-22,在线 2025-09-25,现行版 2026-01-14。©2025 IEEE。
- **与 [36] 的引文链互证**: [36](SHOUT-Trainer)将其引为 [9](Wen et al., TDSC vol. 23, no. 1, pp. 1173–1190, Jan. 2026)——卷期页码与本 PDF 逐项吻合, venue 预告**确证**。
- **类型**: 学术期刊长文(18 页版面)。

## PDF 与实读范围

- **文件**: `ref/Efficient_Instruction_Vulnerability_Prediction_With_Heterogeneous_SDC_Propagation_Knowledge_Graph.pdf`
- **页数**: 19 页(pp. 1173–1190 共 18 页 + 空尾页),**全文实读,无截断**。

## 作者全列(p.1)

Bao Wen(文博,一作,博士生), Jingjing Gu(顾晶晶,Member, IEEE,通讯作者), Dazhong Shen(沈大中,Member, IEEE), Qiang Zhou(周强), Fuzhen Zhuang(庄福振,Senior Member, IEEE), Yang Liu(刘洋,博士生), Haocheng Song(宋浩成,硕士生), Xinyi Huang(黄新一,Member, IEEE)——共 8 人。

## 机构分列(p.1 脚注 + pp.17–18 作者简介)

- **南京航空航天大学计算机科学与技术学院**(NUAA): Bao Wen、Dazhong Shen、Yang Liu、Haocheng Song、Xinyi Huang、Jingjing Gu、Qiang Zhou(后两者兼 **MIIT 模式分析与机器智能重点实验室**)——7 人。
- **北京航空航天大学人工智能研究院 + 中关村实验室**(北京): Fuzhen Zhuang——1 人。
- **企业机构**: 无(Xinyi Huang 博士学位获自澳大利亚伍伦贡大学,现为 NUAA 教授,方向为密码学与信息安全——教育履历,非企业任职)。

## 企业合作证据(三级)

- **一级(作者机构)**: 无企业作者——纯学术团队(NUAA 主导 + 北航/中关村实验室一人)。
- **二级(致谢/资助)**(p.1 脚注): 中国博士后科学基金 GZC20252740;江苏省卓越博士后资助计划;中央高校基本科研业务费 NJ2024031——**全部政府资助,无企业资助**。
- **三级(版权页)**: ©2025 IEEE;下载戳 "Authorized licensed use limited to: Shaanxi Normal University. Downloaded on September 28,2026 at 08:49:33 UTC from IEEE Xplore"——陕西师范大学机构订阅下载。
- **间接层**: ① **Meta**: p.1 正文脚注 1 引 Meta Research 2022 年 "Silent Data Corruptions at Scale" 研究资助 RFP 页面(research.facebook.com/research-awards/2022-silent-data-corruptions-at-scale-request-for-proposals/),作为 "SDC 在过去两年受到 Meta、Google 等研究人员大量关注" 的证据之一;② **Google**: 同句并以脚注 2 引 ISCA 2022 会议程序页(含 Google SDC 论文)为证;③ **Intel**: 实验平台 Xeon E5-2630v4 + LLVM/LLFI 工具链(开源学术工具);④ **开源**: 代码与数据公开于 github.com/SINCOSLab/VP-HPKG(NUAA 实验室组织)。

## 核心结论

1. **问题定位(§I)**: 指令脆弱性预测(估计指令出错时导致 SDC 的概率)传统靠全量故障注入——每条指令每位逐一翻转,能耗随程序规模与动态指令数指数增长;近年 ML 方法 [12][13][14] 只做部分注入训练模型,但**特征选取依赖研究者直觉、无法保证对下游任务有效**。两大挑战:错误传播模式复杂(Challenge 1,寄存器错误随执行随机后向传播,如 %2→%5→…→%18);指令脆弱性**位置敏感**(Challenge 2,同一 store/add/br 指令在不同基本块、不同位置错误概率不同)。
2. **方法总览——VP-HPKG(§I–§IV)**: 新范式:基于 LLVM+LLFI 构建指令执行与寄存器故障生成系统(Algorithm 1:基本块划分→指令解析→全流图分析→硬件故障注入);构建**多层异质程序知识图**(实体=指令+基本块,关系=控制流/数据流/跳转/调用/访问+包含,Algorithm 2),显式编码错误传播的潜在路径;GCN(基本块层,捕获异常块间跳转)+ HGT 异质图 Transformer(指令层,多关系空间相关)混合编码,MLP 输出脆弱性预测。自称**首个**用多层异质程序知识图做细粒度 SDC 错误传播建模的工作。
3. **故障模型(§III-A)**: 处理器计算部件(触发器/功能单元)的瞬态故障,聚焦**指令所操纵寄存器的位翻转**;显式排除:内存/缓存(假定 ECC 保护)、寄存器堆(假定 ECC)、控制逻辑(内部冗余/验证)、指令编码(校验和/取指验证)、微架构寄存器位翻转(如非法跳转类独特错误模式)——与 [31][9][32][33] 的故障模型一致。
4. **故障效果分类(§III-B)**: 采用常用分类:Benign(被掩蔽/处理)/ SDC(错误输出无告警)/ Crash(异常终止)/ Hang(超时);另一维度:Detected vs Undetected(SDC 属 Undetected)、可检可纠(如 ECC)vs 可检不可纠(Crash)。
5. **问题形式化(§III-C)**: 指令脆弱性 = 位翻转致 SDC 的概率 yi(Definition 1);预测问题形式化为**半监督学习**(Problem 1):少量注入标注 Ytrain + 图结构 {BB, IN, T, Ap} → 推断全部 Y。
6. **注入规模与策略(§V-A)**: **逐位穷举注入**(每条指令每一位系统翻转,非随机采样——消除采样偏差、可复现),共 **1,144,070** 次注入(22 程序);随机子集 100 次重复,95% 置信区间误差棒 0.01%;IR 级注入的合法性引 Palazzi 等 [41][42](IR 级 SDC 率与汇编级相当,且逼近真实软错误效应)。
7. **主结果(§V-C, Table IV, Fig.5)**: 22 程序(MiBench/Siemens/JetStream2),21/22 全面胜出;平均 **Acc 0.85 / F1 0.77 / Pre 0.83 / Rec 0.74**;较最强基线 **Acc +10.3% / F1 +18.4%**;较传统错误分析方法(Trident)**Acc +44.1% / F1 +54%**;Pre 较 Perfograph +21.2%、较 MVD +30.5%;Rec 较 PrograML +16.6%、较 Temporise +31.6%。br/icmp 指令预测与注入实测吻合度达 **99%**。
8. **效率(§V-D)**: 模型最紧凑——正文 "only 104 parameters and 71.9 k FLOPs"(按本文文本层上标丢失规律,104 疑为 **10⁴**,两种读法并记,见身份核实);模型规模与 FLOPs 较基线降约 **2×**(摘要确证 "about 2 times");Perfograph 平均执行时间快 13.47s,但以内存与计算复杂度为代价。**注入开销降 30%**:30% 注入比例下的预测效果 ≈ MPIGNN 在 70% 比例;70% 注入即达 Acc 0.85。
9. **消融(§V-E, Table VI)**: 去基本块层(-nb/-nbb):F1 −12.7%、Acc −3.6%、Pre −3%(块属性与异常跳转定位显著);去访问关系(-nc/-nac)影响最大(load/store 高度依赖内存与寄存器);控制流/数据流/调用任一缺失都破坏结构信息完整性。
10. **数据效率与部署范式(§V-E.2, Fig.8)**: 训练集降至 30% 仍有 77% 准确率;**离线训练范式**——注入与预测均在程序部署前完成,开发者据此在 IR 级设计保护机制,"在数据效率与现实适用性之间取得实际平衡"。
11. **多线程验证(§V-F.2, Table VII)**: Phoenix 基准 string_match(多线程),较最强基线 Acc +5.3%、F1 +3.6%——线程交互抽象为数据流边,无需线程感知分析或动态运行时追踪。
12. **选择性加固闭环验证(§V-H, Fig.12)**: 对 10 程序各取预测的 top-20 脆弱指令做**指令复制**加固,同等设置(1000 次随机注入)复测:SDC 率平均降 **72.1%**(Datamanager 0.629→0.311;CRC 0.792→0.016;Replace 0.294→0.01),较最强基线的加固选择再降 33%——预测的脆弱指令确实主导真实 SDC 传播。
13. **结论与未来工作(§VI)**: VP-HPKG 适用于嵌入式系统与服务器等多种系统;后续将从指令流视角更全面深入地研究服务器等系统的脆弱性预测与错误检测,数据驱动+知识发现地学习系统运行的隐式特征与错误传播路径。

## 分类学标注

- **根因机理**: 不研究——IR 级寄存器位翻转模拟瞬态硬件故障(触发器/功能单元抽象),不涉及真实缺陷物理;故障模型显式窄化(ECC 假定排除内存/缓存/寄存器堆,排除控制逻辑与指令编码)。
- **故障模式**: ① 故障效果四分类 Benign/SDC/Crash/Hang + 可检性维度(Detected/Undetected/可纠/不可纠);② **位置敏感脆弱性**——同一指令在不同程序/位置脆弱性不同(核心实证发现,Fig.10–11:add No.30 高危 vs No.91 低危;br 上下文依赖);③ 数据与地址操作类指令错误率最高(Table II,8 类 LLVM 指令分类);④ 错误沿数据流**后向传播**至使用该数据的指令。
- **检测技术**: **开发期(部署前)指令脆弱性预测**——异质知识图 + GCN + HGT(Transformer)+ MLP;半监督(少量 FI 标注+图结构推断);**非运行时检测**。输出可解释:边缘注意力权重直接给出错误传播关键路径。生命周期:开发/测试期(亦可用于部署阶段模块分析)。
- **处理技术**: **选择性加固指引**(验证实验)——top-20 预测脆弱指令做指令复制 → SDC 率 −72.1%;加固手段本身是标准指令复制(Didehban 等 [7] TDSC 2024 同族)。

## 业界观点摘录

- **Meta/Google 关注度引证**(p.1): "it has received a lot of attention from researchers at Meta, Google, etc. in the past two years"(脚注 1 引 Meta 2022 SDC-at-scale RFP、脚注 2 引 ISCA 2022 程序页为证)——产业界 SDC 关注度作为学术动机的直接引证。
- **瞬态故障占比**(p.1, 引 [4] Previlon et al. TDSC 2022): "transient faults constitute the majority of errors in modern computing systems (up to 97.84%)"。
- **少数指令主导 SDC**(p.1, 引 [9] Huang et al. SC 2022): "a portion of program instructions is responsible for almost all SDC errors in a program"——选择性保护的成本论证,与产业界 test-escape 观点([09] Google)同构。
- 纯学术团队,无业界直接观点;业界存在感全部通过间接层(Meta RFP/ISCA 2022 引证、Intel 平台)体现。

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 1,144,070 | 总故障注入次数(22 程序逐位穷举) | Abstract, §V-A |
| 22 / 21 | 程序数 / 全面胜出基线的程序数 | §V-B, §V-C |
| 0.85 / 0.77 | 平均 Acc / 平均 F1 | §V-C |
| 0.83 / 0.74 | 平均 Pre / 平均 Rec | §V-C |
| +10.3% / +18.4% | 较最强基线 Acc / F1 提升 | Abstract, §V-C |
| +44.1% / +54% | 较 Trident(传统错误分析)Acc / F1 提升 | §V-C |
| 99% | br/icmp 指令预测与实测吻合度 | §V-G |
| −30% | 故障注入开销(30% 注入比例 ≈ MPIGNN 70%) | Abstract, §V-D.2 |
| 77% | 30% 训练集下准确率 | §V-E.2 |
| 104(疑 10⁴)/ 71.9k | 模型参数量 / FLOPs(最紧凑;约 2× 优于基线) | §V-D.1, Table V |
| −72.1% | top-20 脆弱指令复制加固后 SDC 率平均降幅(10 程序) | §V-H, Fig.12 |
| −33% | 加固效果较最强基线再降 | §V-H |
| 0.629→0.311 | Datamanager 加固前后 SDC 率 | §V-H |
| 0.792→0.016 / 0.294→0.01 | CRC / Replace 加固前后 SDC 率 | §V-H |
| 1000 | 加固验证复测注入次数 | §V-H |
| 8 | LLVM 标准指令类型数(数据/地址操作类错误率最高) | §IV-A.2, Table II |
| 9(文本层可辨 8) | 基线方法数(SVM/SDIFI/GATPS/PrograML/MVD/Perfograph/MPIGNN/Temporise + 疑漏 1) | §V-B.3 |
| 70/10/20 | 训练/验证/测试划分;3000 epochs | §V-B.2 |
| 0.01% | 95% 置信区间误差棒(100 次重复) | §V-A |
| +5.3% / +3.6% | 多线程 string_match 较最强基线 Acc / F1 | §V-F.2, Table VII |
| 97.84% | 瞬态故障占现代计算系统错误比例(引 [4]) | §I |
| Xeon E5-2630v4 / 64GB / Ubuntu 14.04 / LLVM 4.0 / PyTorch 1.10.2 | 实验平台 | §V-B.2 |

## 方法论要点

- **知识图显式编码传播路径**: 与特征向量方法(依赖研究者特征直觉)的根本差异——实体-关系三元组把控制流/数据流/跳转/调用/访问直接建为图结构,错误传播路径成为模型的一等公民;边缘注意力权重反向提供**可解释的传播路径定位**(§V-F.1 Qsort 案例:strcmp 调用弱相关、fopen 指针强传播,PrograML 两处均判反)。
- **半监督转导省注入**: 少量 FI 标注 + 图结构推断剩余指令类别——注入成本降 30%;这是"图结构先验换标注预算"的典型结构。
- **两层混合编码器**: 基本块层 GCN(异常跳转检测→块定位)→聚合注入指令层→HGT 多关系挖掘(块内指令定位)——由粗到细的定位漏斗,与"先定位基本块再定位指令"的消融证据(-nb F1 −12.7%)自洽。
- **逐位穷举注入 vs 随机采样**: 消除采样偏差、结果可复现;与 [33] Orthrus 的 MIR 采样、[34] 的高斯噪声注入形成注入策略谱系(穷举/随机点/分布噪声)。
- **闭环验证链**: 预测→加固(指令复制)→同设置复测→SDC 率 −72.1%——不止比指标,更验证"预测的脆弱性是真实 SDC 传播的主导者",这是预测类工作罕见的端到端实效证据。
- **IR 级抽象的泛化与边界**: LLVM IR 带来跨语言(C/C++/Rust/Fortran)跨 ISA(x86/ARM/RISC-V)泛化;自认边界——不适用 Java 字节码/.NET CLR 生态(需额外前端/翻译层)。

## 横向对比注记

- **与 [36] SHOUT-Trainer(同批,[36] 引本文为 [9])**: 同为开发期脆弱性预测,三轴差异:① 输入/粒度——[36] 源码操作级(CodeBERT 序列建模)vs 本文 IR 指令级(知识图结构建模);② 任务——[36] 概率回归(PSDC 连续值,MAE/R²)vs 本文二分类(脆弱/非脆弱,Acc/F1)——**指标族不同,综述对比须并注口径**;③ 数据引擎——[36] LLM 生成 13K 程序闭环主动学习 vs 本文 22 程序半监督图转导——规模与范式反差显著(13,000+ vs 22),反映"深度序列模型吃数据"与"结构先验省数据"两条路线的成本结构。两者互补:[36] 面向开发者代码分析,本文面向编译器 IR 级保护机制设计。
- **与 [34]/[35] UMass Lowell 组(同批)**: ML×SDC 谱系三端齐备——经典 ML([34][35] 决策树,PMC 运行时信号,运行期检测)→ 预训练序列模型([36] CodeBERT,源码静态特征,开发期回归)→ 结构化知识图(本文 GCN+HGT,IR 图结构,开发期分类);特征空间×生命周期×模型复杂度三维矩阵就此闭合。
- **与 R2 AVF 理论([10]–[16])**: 指令脆弱性 ≈ 程序级 AVF 的机器学习近似——本文用数据驱动替代了 AVF 的架构级分析(SOFTERR/架构掩蔽因子建模);F1 +54% vs Trident 正是"学习 vs 分析"的量化对照。
- **与 R3 故障注入工具([19]–[24])**: 本文建基于 LLFI(IR 级注入,[16] Lu et al.)并二次开发细粒度位翻转机制;注入合法性论证链([41][42] Palazzi:IR 级 ≈ 汇编级)是所有 IR 级 FI 工作的公共前提。
- **与 [08] tc23-micropersp**: 引 Papadimitriou/Gizopoulos TC 2023 为 [48]——雅典学派的微架构 SDC 视角进入本文相关工作;间接连接 R7 [49][50]。
- **与 [24] arm-soft-error**: 引 Bodmann/Papadimitriou/Gizopoulos/Rech TC 2022 为 [46](早期估计 vs 芯片实测)——本文的模型预测与之同属"预测 vs 实测"验证文化。
- **与 R6 [43] hpc-duplication**: 加固验证用的指令复制即 Didehban/Shrivastava 谱系([7] TDSC 2024;[36] 亦引为 [18])——预测([37])与加固([43])构成"指哪打哪"的上下游。
- **与 [01][09] 产业数据流**: Meta RFP/ISCA 2022 脚注引证说明产业 SDC 事件(2021–2022 公开化)直接点燃了这条学术线;"少数指令主导 SDC"论断([9] Huang SC 2022)与 Google test-escape 数据([09])在同一成本逻辑下汇合。
- **引用瑕疵同族**: 本文 [8]≈[13](GPU-Trident SC 2020 重复引用)——与 [36] 的三组重复引用同类,反映该团队引文管理习惯(两文共享作者圈)。

## 身份核实

- **出处三重确证**: ① 文本层页眉 "IEEE TRANSACTIONS ON DEPENDABLE AND SECURE COMPUTING, VOL. 23, NO. 1, JANUARY/FEBRUARY 2026, 1173";② DOI 10.1109/TDSC.2025.3614343 + 收录用时间线完整;③ 下载戳陕西师范大学 2026-09-28 08:49:33 UTC。与 [36] ref [9] 预告逐字吻合。
- **"104 parameters" 上标疑损**: §V-D.1 "only 104 parameters and 71.9 k FLOPs"——GCN+HGT 模型 104 个裸参数不可信;按本管线已证实的上标丢失规律([35] "106 runs"=10⁶、[36] "106"=10⁶),疑为 **10⁴**;Table V 数值在文本层不可见,无法裁决,**两种读法并记,引用时标注**。摘要 "reduced by about 2 times" 干净,确证约 2×。
- **消融变体命名不一致**: p.12 列表(文本层严重交错,已复原)为 -ns(控制流)/-nb(基本块)/-na(调用)/-nc(访问)/-nd(数据流),而分析正文用 "-nbb"(F1 −12.7% 处)与 "-nac"(访问关系处)——列表与分析命名差一字母,判断为同一变体的排版不一致,按分析语义归并记录。
- **基线计数出入**: 正文称 "nine state-of-the-art methods",文本层可清晰辨认 8 个(SVM-RBF [32]/SDIFI [34]/GATPS [14]/PrograML [52]/MVD [53]/Perfograph [54]/MPIGNN [55]/Temporise [27]);第 9 个疑在交错段落中丢失(候选:GLAIVE [24] 等),如实记录未强补。
- **文本层交错/乱码清单**: p.2 "follrowWse"(贡献列表引导句交错);p.3 "userd taxonomy"(userd=used);p.9 "merthoTdrsidaencto[m32p]arative"(基线引导段交错,语义已复原);p.12 消融列表重度交错(已按上下文复原五变体);Table II/III/IV/V/VI/VII 表体数值文本层丢失,关键数字均从正文散文恢复并注明出处节号。
- **图注小损**: Fig.8 "(10 100%)" 应为 "10–100%"(破折号丢失);Fig.9 图例 "Light:" 应为 "Left:"。
- **编号警示**: 本文内部引文 [34] = SDIFI(Fang/Gu/Yan/Wang, WASA 2021,LightGBM)——**与 digest [34] sparse-pmc 无关**;综述交叉引用时严防混淆。内部 [9] = Huang et al. SC 2022(非 digest [09] test-escapes)。
- **页数核对**: PDF 19 页,末页空;版面页 1173–1190(18 页);全文实读。
- **无 Co-Authored 疑点、无内部数字矛盾**: 72.1%/33%/10.3%/18.4%/44.1%/54% 各处出现一致(摘要/正文/图表引用互相咬合)。

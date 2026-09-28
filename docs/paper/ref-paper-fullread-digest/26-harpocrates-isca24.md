# [26] Harpocrates++: Automated Functional Program Generation Against CPU Faults and Silent Data Corruptions(IEEE Micro Theme Article,2025-12 出版/2026-02 现行版;11 页)

- PDF: `Harpocrates_Automated_Functional_Program_Generation_Against_CPU_Faults_and_Silent_Data_Corruptions.pdf`;页数 11(实读全部:p.1–10,页脚 1.9–2.8,p.11 空白);**文件名为 ISCA'24 题名,实际内容为其 IEEE Micro 扩展版**——"THEME ARTICLE: SILENT DATA CORRUPTIONS"专栏;DOI 10.1109/MM.2025.3640385;出版 2025-12-08,现行版 2026-02-25;脚注 a 自述"本文为 ISCA 2024 文章之扩展"(原文即其 [8]:Karystinos/Chatzopoulos/Fragkoulis/**Papadimitriou**/Gizopoulos/Gurumurthi,ISCA 2024,pp.516–531,DOI 10.1109/ISCA59077.2024.00045——**Micro 版作者名单较 ISCA'24 原文少 Papadimitriou**,如实记录)
- 作者(全列):Nikos Karystinos、George-Marios Fragkoulis、Odysseas Chatzopoulos、Dimitris Gizopoulos、Sudhanva Gurumurthi——共 5 人
- 机构(学术/企业分列):
  - 学术:雅典大学信息与电信系计算机体系结构实验室(Karystinos/_fragkoulis/Chatzopoulos/Gizopoulos,共 4 人)——Gizopoulos 组新生代(Karystinos/Fragkoulis 为新入列博士生的换代阵容)
  - 企业:**Advanced Micro Devices Inc.(Austin, TX)——Gurumurthi(AMD Fellow,负责可靠性/可用性/可维护性研究与先进开发)**——**Athens-AMD 共同署名**
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):**Gurumurthi @ AMD(共同署名)——Athens-AMD 轴从赠礼([22][23] 2023–2024)升级为共同署名(ISCA'24 原文起)**;Gurumurthi 亦为 [15] DelayAVF(MICRO'24)共同作者——AMD 学术合作网络节点
  - 二级(致谢章节资助/数据/设备):致谢(p.2.7)——**Open Compute Project(OCP)基金会 + AMD + Meta 三方 SDC 研究赠礼**(与 [22][23] 的 AMD+Meta 赠礼包同源,新增 OCP 基金会)+ 欧盟 EuroHPC JU DARE-SGA1(101202459)/Horizon Europe Vitamin-V(101093062)+ **Neuropuls(101070238)**/Chips-JU **REBECCA(101097224)**——Neuropuls+REBECCA 与 [22][23] 资助包重合
  - 三级(首页脚注资助声明):无(资助列于致谢节)
  - 间接层:实验硬件 AMD EPYC 7402 24 核(AMD 产品);评测对象含 Google SiliFuzz([f] 以 GitHub 仓库引用)与 **OpenDCDiag([g] github.com/opendcdiag/opendcdiag——sdcshield 之父项目)**;引用 hyperscaler 披露([1] Meta/[2] Google/[3]=[01] SOSP'23);OCP 硬件管理技术会谈 [12] 作为产业开源测试框架发布渠道
- 核心结论(各带节号/图表号):
  1. **DPPM 产业语境(引言)**:hyperscaler 披露的 CPU 缺陷率"逼近 1000 DPPM"——可接受水平:安全关键 <10 DPPM、云/HPC 数百 DPPM;缩微工艺节点+芯片量增长+新兴失效机制使边缘性/退化性故障常逃逸常规制造测试;新缺陷机制挑战"瞬态故障独占脆弱性"的传统假设(AVF 时代前提)
  2. **方法核心(概览)**:Harpocrates=自动生成短小约束随机功能测试程序、最大化目标 CPU 结构故障检测的方法学,**可用于系统生命周期不同阶段**;硬件在环(hardware-model-in-the-loop):gem5 详细微架构引擎建模并评分多类故障,迭代精炼生成程序;适配任意 ISA/微架构/故障类型/质量度量
  3. **三组件架构(Fig.1)**:Generator(MuSeqGen——构建于 MicroProbe [10] 之上,ISA 支持扩展至 x86-64,约束随机生成)+ Mutator(变异引擎,随机指令替换实验最优)+ Evaluator(定制 gem5,硬件感知适应度函数);流程"酷似遗传算法"(种群/交叉/变异/适应度);**"既有系统如 SiliFuzz 即围绕相似原则构建"**
  4. **两类覆盖度量(Tools/Metrics 节)**:位阵列结构(缓存/寄存器堆/队列)用 **ACE 生命周期分析**(逐周期标记 ACE 位,"vulnerability" 0–100% 作硬件覆盖)对应瞬态故障;功能单元用 **IBR 输入位比率**(程序执行中单元输入总位数/理论最大)对应永久故障;**覆盖=程序"激活+传播"故障能力的代理**;直接以 SFI(统计故障注入)精炼不可行(单次数小时 vs 代理度量秒级/亚秒级)
  5. **故障注入基础设施**:可注入瞬态(单比特翻转,可覆写,粒子击诱发)/永久(stuck-at,不可覆写)/间歇(存在特定周期数)三类故障于 **50+ 微架构目标**,另可在功能单元注入门级 stuck-at 故障;SFI 数千次注入给出检测能力的统计显著"金标"度量
  6. **基线结果(Fig.2)**:七结构中前五(IRF/L1D/LSQ/整数加法器/整数乘法器)+SSE FP 加/乘法器;**IRF:几乎所有程序检测 <5%**(MiBench/SiliFuzz 少数离群值勉强过 5%);L1D:三框架显著更高,单 OpenDCDiag 程序达 >80%;LSQ(SQ 数据域):部分基准 ACE 覆盖高但检测低,OpenDCDiag 最佳 20%,其余 0–1%——**硬件无关基线在 IRF/LSQ 上近乎失效**
  7. **性能对比(实验评估)**:**Harpocrates 可运行+可评测的指令生成率为 SiliFuzz 之 30×**;收敛圈数:功能单元 ~1000/缓存 2000+/IRF 与 LSQ 近 5000;**整数加法器 99% 检测仅需 50,000 周期,MiBench 同水平需 >1,100 万周期(220×),乘法器类似增益;同时间内 SiliFuzz 最佳测试 86.6% vs Harpocrates 99.5%**
  8. **覆盖-检测强相关(Fig.3)**:提高覆盖直接提升检测,验证核心原则;**Harpocrates 程序逼近 ACE 上界=最小软件掩蔽(谨慎的生成器参数化);MiBench/SiliFuzz/OpenDCDiag 相关性弱=实质性软件掩蔽**;瞬态位阵列故障显著难于永久 FU 故障(瞬态快速被覆写+动态行为复杂 vs FU 对特定指令可预测响应)——与 [23] FU 掩蔽极低结论一致
  9. **随机性影响(Fig.5)**:最佳序列 50 个不同随机种子变体——多数组件检测方差 <1%;IRF 2%/L1D 5%/整数乘法器最高 17%(x86 整数乘法怪癖:特定变体写预定寄存器,覆写寄存器态可能掩蔽错误结果)
  10. **Ripple 模式短序列实验(Fig.6)**:最优序列前缀切片评估——**比例降至 0.1×(前 10% 指令,数百条 vs 数千条)检测几乎不变;0.01×(数十条)时 SSE FP 乘法器降至 0%**;永久门级故障"任何时刻任何条件下可观察"故可短序列检测;但现实 FU 故障模型可能为间歇/边缘事件(路径延迟,温度/电压波动触发)则需更长序列
  11. **生命周期三用例(Flexibility 节)**:快速周期性舰队扫描(Ripple 模式,在产,限时最大化检测)/全面扫描(Fleetscanner 模式,停产,无时限达极高检测)/流片前电气与环境条件筛查(设计者寻边缘缺陷)——**产业部署模式([03] Dixit [11])成为学术方法的显式配置目标**
  12. **结论**:硬件感知+反馈驱动方法学持续产出高质量测试,检测能力与效率均超既有框架;精确迭代覆盖反馈使快速收敛于高效指令序列,Harpocrates 成为系统全生命周期缺陷检测的实用适配方案
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**制造缺陷/变异性/边缘性/margin(温度为核心温度触发)/设计 bug/老化**五类并列(摘要+引言)——产业根因全景;DPPM 量化(~1000 vs <10 vs 数百);挑战 AVF 时代"瞬态独占"假设
  - 故障模式类型:瞬态/永久/间歇三类故障模型(方法论层);**"hyperscaler 报告明确指认 FP 单元及其整数对应物(标量与向量硬件)为大舰队 SDC 的极可能来源"**([3]=[01]);检测难度不对称规律(瞬态位阵列≫永久 FU)
  - 检测技术类型:**功能测试(native mode)自动生成——硬件在环+遗传式精炼+硬件感知覆盖(ACE/IBR)**;结构测试(扫描模式,制造时)vs 功能测试(正常运行时)定位;金标判定=SFI 统计注入
  - 处理技术类型:检测后隔离 faulty CPU(一句话);无运行时容错——检测论文
- 业界观点摘录(产业合作论文,立场丰富):"hyperscaler 报告揭示逼近 1000 DPPM 的惊人缺陷率;安全关键系统可接受 <10 DPPM,云/HPC 数百 DPPM"(引言);"数据中心提供商追求近零舰队停机,测试程序应最小化停机影响"(挑战 #3);"hyperscaler 报告明确指认 FP 单元为大舰队 SDC 极可能来源"(Hardware Components 节,引 [3]);"我们的愿景是 Harpocrates 成为 OCP 社区努力的主要贡献者"(开源框架节);"SiliFuzz 是唯一可对比的替代自动化方法"(性能对比节——产业工具的可获得性现实)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | DPPM 语境 | hyperscaler 逼近 1000;安全关键 <10;云/HPC 数百 | 引言 |
  | 目标结构 | 7(IRF/L1D/LSQ/整数加/乘法器/SSE FP 加/乘法器) | 实验设置 |
  | FI 能力 | 3 类故障(瞬态/永久/间歇)×50+ 微架构目标+FU 门级 | Tools 节 |
  | 基线 IRF 检测 | 几乎全部 <5% | Fig.2 |
  | 基线 L1D 最佳 | >80%(单 OpenDCDiag 程序) | Fig.2 |
  | 基线 LSQ 最佳 | 20%(OpenDCDiag);其余 0–1% | Fig.2 |
  | 生成率 | SiliFuzz 之 30× | 性能评估 |
  | 收敛圈数 | FU ~1000/缓存 2000+/IRF、LSQ ~5000 | 收敛节 |
  | 加法器 99% 检测 | 50,000 周期 vs MiBench >11M(220×) | Fig.3/4 |
  | 同时间对比 | SiliFuzz 86.6% vs Harpocrates 99.5% | Fig.4 |
  | 随机种子方差 | 多数 <1%;IRF 2%/L1D 5%/整数乘法器 17% | Fig.5 |
  | 前缀切片 | 0.1× 检测几乎不变;0.01× SSE FP 乘法器 0% | Fig.6 |
  | 测试规模 | Harpocrates 典型 10K 指令;OpenDCDiag 限 ≤100M 周期/测试 | 实验设置 |
  | 硬件 | AMD EPYC 7402 24 核/128GB DDR4 | 实验评估 |

- 方法论要点:硬件在环=以仿真器为适应度评估器(区别于 SiliFuzz 的"代理模糊"——Harpocrates 用**微架构级仿真+故障注入基础设施**双角色:覆盖度量+SFI 金标);覆盖度量按故障类型配对(ACE↔瞬态/IBR↔永久)是方法学关键设计;代理度量(单次执行可测)替代 SFI 直接精炼(小时级→秒级)是可行性的核心权衡;SiliFuzz 公平性处理:其 ≤100 字节快照聚合为 10K 指令测试再评测;变异算子选择(随机指令替换)经实验优选;结构性认识——FU 永久故障可短序列检测(任何时刻可观察)vs 位阵列瞬态需长序列(动态行为+覆写)
- 横向对比注记:**[27] 按 INDEX 题名即本篇之 ISCA'24 原文([8] 于此处:六作者含 Papadimitriou,pp.516–531)——待读 [27] 核实;两篇构成"会议原文+杂志扩展"对**(作者差异:Papadimitriou 不在 Micro 版,如实记录)。**本集被引论文密集交叉**:[3]=[01](SOSP'23 舰队 SDC,S. Wang/G. Zhang/J. Wei/Y. Wang/J. Wu/Q. Luo——[01] 即此六人,跨论文作者信息互证)、[11]=[03](Ripple,Dixit arXiv 2203.08989)、[7]=[15](DelayAVF MICRO'24,共同作者含 Gurumurthi——AMD 网络节点)、[9]=[30](IRPS'25 RL 测试优化,定位为"对既有测试的参数优化,与 Harpocrates 正交——无预定测试/算法,可适配任意故障模型")、[1]=Dixit 2102.11245、[2]=Hochschild HotOS'21、[10]=MicroProbe(IBM MICRO'12,生成器谱系)、[12]=OCP 2023 技术会谈。**[25] SiliFuzz 作为基线**(30× 生成率/86.6% vs 99.5%;"酷似遗传算法"定位)——产业在先([25] 2021)学术精化([26] 2024/2025)的完整链路。**OpenDCDiag(sdcshield 父项目)被正式学术评测**:L1D 最佳单程序 >80%/LSQ 最佳 20%/IRF <5%,且被归入"实质性软件掩蔽"组——本集唯一直接评测父项目的论文,sdcshield 生态定位的关键外部数据。**Athens-AMD-Meta 轴演进**:赠礼([23] 2023/[22] 2024)→AMD 共同署名([26] ISCA'24 2024+Micro'25)→Meta 共同署名([18] 2025);本篇致谢 OCP+AMD+Meta 赠礼+Neuropuls/REBECCA 与 [22][23] 资助包重合。瞬态-永久检测难度不对称与 [23] FU 掩蔽极低/FU Crash 主导互证;ACE 上界逼近与 [10][11] AVF 方法论承接;Ripple/Fleetscanner 生命周期映射([03])为本篇用例设计骨架;DPPM 数字连接 [09] test-escapes(10× 逃逸)的量化语境
- 身份核实:**PDF 文件名(ISCA'24 题名)与实际内容(IEEE Micro 2025-12 "Harpocrates++" 扩展版)不一致——如实记录,笔记以实际内容为准**;标题含"++"自表扩展身份,脚注 a 明示 ISCA'24 扩展;DOI 10.1109/MM.2025.3640385、出版/现行版日期齐全,名实相符;五作者(雅典 ×4+AMD ×1)归属清晰;Gurumurthi 为 AMD Fellow(作者简介确认)。**文本层已知损失**:(1) **Fig.2–6 数值不可从文本层提取**(散点/柱状/热图)——全部引用数值取自散文(<5%/>80%/20%/0–1%/30×/1000/2000/5000/50,000/11M/220×/86.6%/99.5%/<1%/2%/5%/17%/0.1×/0.01× 均散文给出);(2) **水印与正文交错乱码**("JAaunthuoarirzye/Fdelibcerunsaeryd 2u0se26"="Authorized licensed use…January/February 2026" 叠印)——不影响正文;(3) 切片比例"0.01#/0.1#"之"#"为"×"的编码损失(语境可解);(4) 末页(p.11)空白;(5) IEEE Xplore 下载戳(Shanghai Jiaotong University,2026-06-29)为访问来源标记;(6) **无内部矛盾**——30×/220×/86.6%-99.5% 等与散文论证一致;基线数值(Fig.2)与"IRF 近乎失效/LSQ 0–1%"的动机叙述自洽;七结构与实验设置列举一致;Micro 版作者名单较 ISCA'24 少 Papadimitriou 为客观差异(非矛盾,如实记录)

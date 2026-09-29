# [15] DelayAVF: Calculating Architectural Vulnerability Factors for Delay Faults(MICRO 2024,第 57 届 IEEE/ACM 微架构国际研讨会;会议论文,印刷页 231–245)

- PDF: `MICRO2024 DelayAVF_Calculating_Architectural_Vulnerability_Factors_for_Delay_Faults.pdf`;页数 16(实读 p.1–15 全部,末页空白;DOI 10.1109/MICRO61859.2024.00026;979-8-3503-5057-9/24 ©2024 IEEE;Xplore 水印:Shanghai Jiaotong University,2026-07-06 下载)
- 作者(全列):Peter W. Deutsch、Vincent Quentin Ulitzsch、Sudhanva Gurumurthi、Vilas Sridharan、Joel S. Emer、Mengjia Yan(共 6 人;前两人同等贡献声明)
- 机构(学术/企业分列):
  - 学术:MIT ×4(Deutsch、Ulitzsch[MIT/TU Berlin 双挂]、**Emer——AVF 奠基人之一**、Yan);TU Berlin ×1(Ulitzsch 双挂侧)
  - 企业:**AMD ×2**(Gurumurthi—Austin;Sridharan—Boxborough)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):AMD ×2(p.1)
  - 二级(致谢章节资助/数据/设备):致谢(p.13)感谢 **Divya Prasad(AMD)、Jeff Rearick(AMD)**(Rearick 为 AMD Fellow 级测试架构师)、Jean-Pierre Seifert(TU Berlin);资助:MIT AI Hardware Program、**Google Research Scholar Program**、德国联邦教育与研究部 6G-RIC(16KISK030)、NSF CCF 2217099、**ACE(JUMP 2.0 中心,SRC 计划,DARPA 赞助)**
  - 三级(首页脚注资助声明):无(资助并入致谢);**另:全文末带 AMD 版权块"©2024 Advanced Micro Devices, Inc. All rights reserved…This paper reflects collaborative work between the authors."**——AMD 雇员署名论文的标准企业版权声明,企业参与的法律级证据
  - 间接层:引用的产业文献构成 fleet SDC 话语圈——[16][17] Dixit(Meta arXiv)、[24] Hochschild "Cores that don't count"(Google)、[44] SiliFuzz(Google,即本集 [25])、[53] Wang SOSP23(即本集 [01]);[29][30] Meixner semiengineering 产业文章([30] 即本集 [32]);[45] Shamsa IRPS'24;[42] Ryan et al. ITC'14(边际缺陷分类学来源)
- 核心结论(各带节号/图表号):
  1. **时代定性(摘要/§I)**:主要云厂商大规模 SDC 行为报告激增,提示野外故障性质近期改变;超大规模厂商观察到无法仅归因粒子翻转瞬态故障的 SDC 增多,引发行业级系统正确性担忧;近期产业文献提出新根因假设——**边际缺陷(marginal defects)诱导的小延迟故障(SDF)**(亚周期传播延迟增加)
  2. **粒子翻转 AVF 不适用于 SDF(§III-A)**:SDF 改变电路时序而非即刻翻转比特——三重差异:(a) **toggle 依赖**(不翻转的信号无法被延迟致错,但同一状态元件可被粒子击中,ACEness 判定不同);(b) **延迟时长 d 参与决定**哪些状态元件出错(d 太小可无错,被逻辑掩蔽可无错);(c) **单 SDF 可致多个同时状态元件错误,且出错组无法按物理位置先验确定**(逐周期随电路时序与输入变化)——区别于空间多比特模型 [54]
  3. **DelayAVF 定义(§V-A,Eq.2–3)**:结构对小延迟故障的脆弱性=程序可见失败概率;wire e 在周期 i DelayACE ⟺ 在 e 上加时长 d 的 SDF 导致程序可见失败;DelayAVF_d(H)=Σ DelayACE_d(e,i)/(N·|E|);乘以结构 SDF 发生率即估失败率
  4. **故障-缺陷-错误-失效术语学(§II-A,沿 [33]=本集 [11])**:fault=硬件非期望变更/defect=底层物理瑕疵/error=状态元件存错值/failure=程序可见;SDC=输出错而无硬件通知,DUE=未完成且无错输出
  5. **边际缺陷分类学(Fig.1,改编自 [42] Ryan ITC'14)**:按位置(跨芯片系统位置 vs 随机位置)×触发条件(全条件永久 vs 特定电压/频率/温度/负载模式的边际条件)二维分类;星标=本文研究对象;物理根因含光刻误差 [23]、裂纹 [14]、分层 [13]
  6. **测试不可行性论证(§II-B)**:检出全部边际缺陷需在全部工作条件下测试所有电路所有路径——每芯片数年测试时间,经济与实践均不可行→**部分边际缺陷芯片必然出货,设计者必须假设测试逃逸存在而加韧性**(与粒子翻转"环境必然发生"同型的设计前提)
  7. **两步法计算(§V-B,Fig.4,Eq.4)**:DelayACE(e,i)=GroupACE(DynamicReachable(e,i), i+1)——Step1 **timing-aware**(单周期,求动态可达集=静态可达[路径超时钟周期]∩实际锁存错值[经逻辑掩蔽]);Step2 **timing-agnostic**(N 周期,判 GroupACE=该组状态元件同时出错是否致程序可见失败);复杂度 O(|E|·N) timing-aware+O(|E|·N²) timing-agnostic,高度可并行;非启发式,精确
  8. **混淆效应二分(§V-B/§VII)**:**ACE 复合**(compounding——组内无一单独 ACE 但组合 GroupACE)与 **ACE 干涉**(interference [54]——单独均 ACE 但错误互相抵消);忽略即得 OrDelayAVF 近似(ORACE 判据)
  9. **实证案例(Ibex RISC-V,§VI)**:开源核(多次流片,OpenTitan root of trust 用);五结构(ALU/decoder/regfile±单纠错 ECC/LSU/prefetcher);NanGate 45nm 库+Yosys 综合+Verilator;Beebs 五基准;每线 4% 周期注入;d=10%–90% 时钟周期
  10. **观察 1(§VI-B)**:结构间 DelayAVF 差异显著——**ALU 高达 regfile 的 5×**(geomean);word-line 机理:仅激活中(0→1)与去激活中(1→0)的行可错,regfile 低 toggle 率→低动态可达→低 DelayAVF
  11. **观察 2(§VI-B)**:小 d 时脆弱性由**静态时序特性主导**(静态可达纯电路结构函数);大 d 时**程序/架构层效应更突出**;多比特状态元件错误常见——**约 50% 的致错 SDF 引多比特错误**(d=10% 时 21%,其余 d ~50% 无明显趋势);大 d 的 DelayAVF 不必然上界小 d(毛刺效应)
  12. **观察 3(§VI-B,Fig.9)**:DelayAVF 跨基准差异大——md5(高随机哈希→高 toggle)vs libstrstr(规则串比较)——数据依赖掩蔽/架构韧性/程序韧性的三重来源
  13. **观察 4+5(§VI-C,Fig.10–11)**:**DelayAVF 与 sAVF(粒子翻转 AVF)排序不同**;**对粒子翻转有效的保护对 SDF 未必有效**——regfile 加单纠错 ECC 使 sAVF→0 而 DelayAVF 不为零:word-line SDF 使 sense amp 重锁前一地址(00)的数据,ECC 校验通过(读到的是另一地址的有效数据)→不检出;prefetcher 对两者皆脆弱
  14. **观察 6+近似评估(§VII,Table III)**:OrDelayAVF 平均可用但两类结构失真——decoder 高 ACE 干涉(多比特错误使 PC 回退数条指令,重放旧指令序列反而无错;单比特回退一条可致错)→偏差 max 21.80%;**ECC regfile 高 ACE 复合(多比特错误超出单纠错 ECC 能力→GroupACE,但组内状态元件单独非 ACE)→偏差 max 92.45%**——ECC 结构上禁用 ORACE 近似
  15. **相关工作定位(§VIII)**:IVF [36](间歇故障,粗粒度,假设写操作期延迟必错,无逻辑掩蔽/无复合干涉);Chang [10](算术单元指令级时序错);Varius [43](工艺变异,无逻辑掩蔽无架构影响);Entrena [18]/Hari [22] 两段式模型(粒子翻转);Czutro [15]/Ahmed [4]/Riefert [41] 延迟测试(ATPG/BMC);**结尾提出 DelayAVF 可用于生成提升 SDF 可观测性的功能测试,接入 Harpocrates [25]=本集 [27] 式硬件感知功能测试生成框架**
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**边际缺陷(随机位置+特定边际条件触发)→小延迟故障**——制造缺陷范式在 AVF 方法论中的正式化;亚周期延迟;d 的来源(SDL 软缺陷定位+光学/激光刺激 [9][12] 可从实际缺陷芯片反推)
  - 故障模式类型:toggle 依赖错误;单 SDF→多状态元件同时错误(非空间相邻、逐周期变化);**ECC 结构的 word-line 延迟重锁模式(校验通过但数据错位)**;SDC/DUE 二分沿用
  - 检测技术类型:设计时脆弱性评估——DelayAVF 两步法(timing-aware 动态可达集+timing-agnostic GroupACE);与 sAVF 的排序分歧;sAVF 数据复用近似(OrDelayAVF)及失效边界
  - 处理技术类型:电路级(驱动强度/布局);空间/时间/信息冗余三策略(Razor [19] 为时间冗余例;Hamming/CRC/残差码);**ECC 对 SDF 防护的局部失效**;面向 SDF 可观测性的功能测试生成(Harpocrates 方向)
- 业界观点摘录(AMD+MIT 产学混合,深度参与 fleet SDC 话语圈,摘原话):
  - "主要云厂商描述大规模新 SDC 行为的报告激增,提示野外故障性质的近期改变"(摘要)
  - "超大规模厂商近期观察到无法仅归因粒子翻转瞬态故障的 SDC 增多,引发对大规模系统正确性的行业级担忧"(§I)
  - "在边际情况下产生延迟故障的缺陷是业界上升中的担忧,并被怀疑是近期记录的大规模可靠性问题的根因之一"(§II-B)
  - "在测试时识别所有边际缺陷需要测试每条电路所有路径的全部工作条件——可能需要每颗生产的芯片数年测试时间,使这种测试在经济与实践上不可行"(§II-B)
  - AMD 版权块"本文反映作者间的协作工作"(p.13)——企业协作的正式声明
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | ALU vs regfile DelayAVF | ALU 高达 5×(geomean over Beebs) | Obs 1, Fig.7 |
  | 多比特状态元件错误率 | ~50%(d=10% 时 21%,其余 d ~50% 无趋势) | §VI-B |
  | 注入线数 \|E\| | ALU 3668/decoder 1007/regfile 17816/regfile+ECC 19611/LSU 2027/prefetch 3249 | Table I |
  | 基准周期数 N | md5 1720/libbubblesort 3829/libstrstr 1051/libfibcall 2448/matmult 8903 | Table II |
  | 采样率 | 每线 4% 执行周期注入 | §VI-A |
  | 延迟范围 | d=10%–90% 时钟周期(10 档) | §VI |
  | d=50% 静态可达 | ALU 85%/regfile 100% 线至少一静态可达状态元件(libstrstr) | §VI-B, Fig.8 |
  | decoder ACE 干涉 | max 13.03%/avg 6.73%;OrDelayAVF 偏差 max 21.80%/avg 10.45% | Table III |
  | ECC regfile ACE 复合 | max 21.95%/avg 11.57%;偏差 **max 92.45%/avg 50.38%** | Table III |
  | 复杂度 | O(\|E\|·N) timing-aware + O(\|E\|·N²) timing-agnostic | §V-B |
  | 实验平台 | Ibex RISC-V;NanGate 45nm;Yosys;Verilator;Docker artifact(MIT 许可;Zenodo 10.5281/zenodo.13743439;GitHub viniul/micro-artifact);48+ 核服务器约 24h | §VI-A, 附录 |

- 方法论要点:AVF→DelayAVF 的概念迁移(ACEness→DelayACEness;比特→电路元件);两步法的可 tract 性设计(仅单周期需 timing-aware;GroupACE 判定与时序无关);静态/动态可达二分(逻辑掩蔽的显式建模);ACE 复合/干涉的枚举式显式处理;开源全栈可复现(Docker/Yosys/Verilator/Zenodo)——2024 年 artifact 评测标准;d 参数化路径(SDL 实测反推或全空间扫描);诚实局限:pre-layout 时序(不考虑互连电容,逻辑结构门延迟主导故影响小)、数据无关延迟假设、单缺陷假设 [3]、单周期 SDF 持续假设(边际条件极短)。
- 横向对比注记:**AVF 谱系第六篇——fleet SDC 时代(01/02/09 的边际缺陷根因假设)与 2003 AVF 方法论的正式合流**:[33]=本集 [11](奠基);[8]=本集 [14](硬故障 AVF——上一版泛化);[25] Harpocrates=本集 [27];[44] SiliFuzz=本集 [25];[53] Wang SOSP23=本集 [01];[30] Meixner=本集 [32]——本文是 R2 谱系与 R1 fleet 批次的引用枢纽。**作者阵容横跨谱系两端:Emer(10/11 奠基共同作者,2003)+ Sridharan(AMD;[13] 所引"长停顿阴影 60% 脆弱性"[19] 与 PVF/HVF [20][21] 的作者)+ Gurumurthi(AMD;[20] "Emerging Fault Modes" 2023 立场文、[54] 空间多比特 AVF)**——2003 奠基圈与 2020 年代 fleet 圈的直接协作,产业主导权转移叙事的人物级证据。**"ECC 对粒子翻转有效≠对 SDF 有效"(word-line 重锁模式)是综述对策章的关键反直觉结论**——检测/处理技术的故障模型依赖性;OrDelayAVF 在 ECC 结构 92% 偏差是方法论警告;[22] Hari/NVIDIA 两级模型(Emer 挂名)通向 [04] Veritas 的 x86 建模谱系;Harpocrates 方向的提出把 R2(脆弱性评估)与 R4(测试生成)批次连接。
- 身份核实:标题"计算延迟故障的架构脆弱性因子"与内容(DelayAVF 定义+两步法+Ibex 实证+sAVF 对比+近似)完全相符;MICRO 2024 页眉、DOI、979-8-3503-5057-9/24、©2024 IEEE、印刷页 231–245、Xplore 水印齐全,名实相符;AMD 版权块+MIT/AI 硬件计划等多源资助自洽。**文本层已知损失**:(1) **Fig.7/9/10 的数据标签严重交错乱码**(多系列数值标签互相重叠,如 "00..7778993 000.004.5143903.30020.5155…")——逐延迟/逐基准的归一化 DelayAVF 数值不可靠引用,只引散文结论(5×、跨基准差异、排序分歧);(2) **Table III 行标签错位**(结构名 ALU/Decoder/Regfile/Regfile(ECC)四行与数值组错开)——按序映射 ALU→(0.98/0.58/0.17/0.09/3.00/1.73)、Decoder→(13.03/6.73/2.47/1.14/21.80/10.45)、Regfile→(0.13/0.07/0.17/0.07/0.69/0.30)、Regfile(ECC)→(0.13/0.07/21.95/11.57/92.45/50.38),**经散文交叉验证**(decoder 高干涉、ECC regfile 高复合)确认映射无误;(3) p.9–11 双栏排版与图注有局部交错但散文可辨;(4) 参考文献少量编码损坏([27] "Pearson Prentice Hall"→"Peason Prenticle Hall"、[40] "Côté"→"Co^te�")——次要,不影响引用;(5) Fig.6/8/11 为图像,数值引自散文;(6) 无资助脚注(资助在致谢);(7) 摘要与六条 Observation 一致,无内部矛盾。

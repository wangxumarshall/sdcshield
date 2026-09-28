# [30] 用强化学习优化测试以改进静默数据错误检测(IRPS 2025,Intel)

## 标题与出处

- **标题**: Improved Silent Data Error Detection through Test Optimization using Reinforcement Learning
- **会议**: IRPS 2025(International Reliability Physics Symposium),论文编号 8C.1,pp. 8C.1-1 – 8C.1-5,©2025 IEEE,ISBN/Copyright 行 `979-8-3315-0477-9/25/$31.00 ©2025 IEEE`(p.1 页脚,每页页眉 8C.1-N)。文本层无 DOI;会议名由文件名 + 版权行 + 页眉一致推定,无矛盾。
- **类型**: 产业论文(纯 Intel 单机构)。

## PDF 与实读范围

- **文件**: `ref/Improved Silent Data Error Detection through Test Optimization using Reinforcement Learning, IRPS Improved SDE Detection 2025.pdf`
- **页数**: 6 页(第 6 页空白),**全文实读,无截断**。

## 作者全列(p.1,单一署名行)

Manu Shamsa, John D. Martin, Mariano Phielipp, Thiago Macieira, Loganathan Lingappan, Brad Kelly, David Lerner, Michael Tucknott, Ethan Hansen(9 人)

## 机构分列

- **学术机构**: 无。
- **企业机构**: Intel Corporation(p.1 标题下唯一机构,9/9 作者)。

## 企业合作证据(三级)

- **一级(作者机构)**: Intel Corporation × 9(p.1)。**纯产业论文,无学术合作者**。
- **二级(致谢/资助)**: 无外部资助。致谢(p.5 ACKNOWLEDGMENT)感谢 Balkaran Gill、David Pullen、Somdeb Majumdar、Brad Bittel 四人贡献(未标机构;Bittel 即参考文献 [1][7] 之 Intel 作者,推断均为 Intel 同事)。
- **三级(版权页)**: p.1 版权行为 IEEE 会议标准版权(非企业版权页)。
- **间接层**: 引用网络构成产业话语圈——[2] Dixit Meta(arXiv 2102.11245,即本集 [01] 前身)、[3] Hochschild Google "Cores that don't count"、[10] van de Ven & Steiner ITC'21(OpenDCDiag 缔造论文)、[9] Lerner ITC'22 测试优化、[4] Shamsa & Lerner IRPS'24 缺陷机理、[7] Bittel 等 IRPS'24 AI 影响([29] ETS24 引用其 10 万 SoC 量化话语即出自 Lerner 一系);[14] Biswas & Cory 2012 系统级测试工业研究。

## 核心结论

1. **检测效力结构(§I)**: SDE 检测效力由测试周期性(periodicity)与所跑的具体测试共同决定;其他因素包括系统节点数、部件预期失效率、应用对 SDE 的敏感度。测试时间直接关系数据中心 TCO——"在不增加测试时间与成本的前提下提高检测率"是本文目标函数。
2. **缺陷画像(§I)**: SDE 主要源于制造缺陷,微妙、难检、边际(marginal)[4-5];潜伏性——数周/数月/数年使用后才首次故障 [6];位于覆盖饱和曲线尾部,即使高结构/功能覆盖率的 SoC 区域也会逃逸 [8];SDE 跨架构、工艺、厂商普遍存在 [3]。
3. **DCDIAG/OpenDCDiag 全景(§II)**: 测试属于 Intel DCDIag(制造 + in-field 双用途);其核心与部分测试以 OpenDCDiag 开源 [10]。完整套件覆盖:核间/socket 间通信、片上缓存、几乎全部浮点/整数/数据操作指令、多核并行相同计算后对比、可逆变换(加解密、压缩/解压)检错、伪随机数生成遍历地址与指令空间。
4. **Eigen 测试族起源史(§II.A,一手叙事)**: 2019 年 Intel 接到客户报告——某些负载疑似经历 SDE;遂创建测试模拟云服务商(CSP)数据中心的大规模浮点处理(AI 负载特征)。选 Eigen 库因 C++ 模板支持快速原型化与数据类型切换(单/双精度、实/复数)。
5. **为何选 GEMM(§II.A)**: 矩阵乘重用 FMA;Intel 性能核 4 周期完成 8 双精度/16 单精度/32 半精度乘加;FMA 面积大→随机分布制造缺陷命中多;FMA 空闲时下电→矩阵乘测试造成反复上电/下电→电压/电流瞬态激活边际缺陷;并引入模拟信号缺陷(低中间值、尖峰、高一阶导数);电流变化产生电磁效应,影响片上其他**非直连** IP 块。
6. **5 年制造数据结论(§II.A)**: Eigen 测试对 SDE 类缺陷有效,但搜索空间巨大(大数据类型 × 宽指令集)→平均 time-to-failure 高、测试效率相对差——RL 优化的动机。
7. **矩阵尺寸→微架构压力映射(§II.B, Fig.2-5)**: 指令 trace→微架构模拟器→利用率剖析。大矩阵→memory-bound + 执行单元 bound,前端占比下降;小矩阵避免动态内存分配、倾向循环展开;执行簇内大矩阵提高 FMA/SIMD 利用率、降低整数单元利用率;内存簇/memory-order 在 32×32 以上影响变弱。**设计守则:压 FMA 用大矩阵;压 TLB/memory-order/OOO 用小矩阵(32×32 或 64×64)多迭代。**
8. **RL 方法(§II.C, Fig.6)**: 环境=潜在缺陷处理器上执行的测试程序;agent=学习算法,每步选测试输入;reward=失效率。动作空间=矩阵生成器种子 1–9;用 **EXP3 对抗式 bandit 算法**(Seldin et al. [13])维护离散分布并更新——轻量 bandit 而非深度 RL。
9. **实验一(§III, Fig.7)**: RL vs 朴素均匀采样 vs 常数策略,1000 次学习交互。对该缺陷,朴素随机最不可能检出;RL 找到失效率最高的输入并持续选择。**选择概率 ≠ 失效率**(policy 收敛与检测率是两回事)。
10. **策略演化(Fig.8)**: 最优生成器 "7" 的选择概率在 250/500/1000 迭代序列中、于 1000 次完成前即已稳定——收敛代价低。
11. **内存量-检出率正比(§III)**: 测试使用内存越多→检出 SDE 机会越大,与 §II.B 功能块利用率提升解释一致。
12. **生产部署声明(§III,本文最重要的一句)**: "The system-based test [14] used by Intel to manufacture current Xeon® processors is now utilizing Eigen tests that have been optimized using RL-based method." —— RL 优化后的 Eigen 测试**已进入现役 Xeon 处理器制造测试**;未来工作:扩展到更多产品、评估新设计是否需重新调参、推广到 in-field 测试。
13. **结论(§IV)**: OpenDCDiag 提供修改开源测试(Eigen 族)的框架;AI 算法优化测试参数可在不增加测试时间与成本的前提下更高效检测 SDE。
14. **测试算法本体(Fig.1)**: 计算一次存参考→逻辑处理器上循环至时限:重算→与参考比较→有差异即报告(OpenDCDiag 标准"计算-比较"自参照模式);Table I 列 Eigen 族四类测试:GEMM(eigen_gemm_*/eigen_sparse,221×221 双精度复数、256×256 单/双精度实数)、线性矩阵方程求解、SVD 分治(eigen_svd_*)、SVD Jacobi(eigen_svd_jacobi_*),尺寸 128×128–512×512。

## 分类学标注

- **根因机理**: 制造缺陷潜伏性(周/月/年)+ 覆盖饱和尾部逃逸 + **FMA 上电/下电瞬态功率激活机制**(电压/电流瞬态、模拟信号劣化、电磁耦合非直连 IP)——与 [28] Vega 的 BTI 持续老化机制互补:瞬态功率事件 vs 时序退化,两条"物理机制指导测试设计"路线。
- **故障模式**: 边际缺陷的模拟域表现(低中间值/尖峰/高一阶导数);激活条件稀少→高 time-to-failure。
- **检测技术**: 测试生成/优化谱系的**第五路线——现有测试输入空间的 RL 参数优化**(bandit 式种子选择):与 [25] SiliFuzz fuzzing-proxy、[26][27] Harpocrates hardware-in-the-loop、[28] Vega physical+formal、[10] OpenDCDiag library-porting **正交**([26] Micro'25 引本文为 [9] 并明确定位为正交,精读证实:优化输入参数 vs 生成测试程序,可组合)。本集内 **AI/ML 进入 SDE 测试环路的最早实例**(transformer 路线见 [36] SHOUT-Trainer)。
- **处理技术**: 无(纯检测侧)。
- **生命周期**: 制造测试(已生产部署)+ in-field 测试(未来工作)——DCDIAG 的全生命周期定位在本文有明确表述。

## 业界观点摘录(纯产业论文,一手)

- "SDE detection efficacy of DCDIAG and similar tools is determined by the testing periodicity and the specific tests run."(§I)
- "SDE are not unique to specific applications and have been observed to occur across compute architectures, process technologies, and vendors [3]."(§I)
- Eigen 测试起源:"At the time, Intel had been in contact with customers who reported that certain workloads were suspected to experience SDE. In response, Intel created new tests to emulate the workloads found in Cloud Service Provider (CSP) data centers."(§II.A——客户驱动开发的一手史料)
- 生产部署:"The system-based test used by Intel to manufacture current Xeon® processors is now utilizing Eigen tests that have been optimized using RL-based method."(§III)
- TCO 关切:"achieve high SDE detection rate at lower test time, which is critical to manage the total cost of ownership (TCO) in data centers."(§I)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 9/9 | 全体作者属 Intel(纯产业论文) | p.1 |
| 8/16/32 | 性能核 4 周期并行乘加数:双精度/单精度/半精度 | §II.A |
| 2019 | Eigen 测试族诞生年(CSP 客户报告驱动) | §II.A |
| 5 年 | 制造环境运行 Eigen 测试的数据积累年限 | §II.A |
| 128–512 | Table I 矩阵尺寸范围(221/256/300/512/128) | Table I |
| 1–9 | 矩阵生成器(种子)动作空间;最优为 7 | §II.C, Fig.8 |
| 1000 | 学习交互总次数;最优动作概率在完成前稳定 | §II.C, Fig.8 |
| EXP3 | 所用 bandit 算法(Seldin et al. 2012) | §II.C [13] |
| 32×32/64×64 | 压 TLB/memory-order/OOO 的小矩阵建议尺寸 | §II.B |
| 4 类 | Eigen 族测试类型(GEMM/线性方程/SVD 分治/SVD Jacobi) | Table I |

## 方法论要点

- **bandit 而非深度 RL**: 动作空间仅 9(种子 1–9),EXP3 足矣——问题建模为对抗式多臂老虎机,奖励为实测失效率;轻量算法 + 1000 次交互即收敛,"reasonable amount of data and resources"。
- **先验微架构剖析指导动作空间设计**: 指令 trace → 微架构模拟器 → 利用率剖析(Fig.2-5),先弄清矩阵尺寸压哪些区域,再定 RL 优化方向——"理解被测对象"先于"优化输入"。
- **优化对象是输入参数而非程序**: 与 SiliFuzz/Harpocrates 的生成式路线根本不同;reward 直接来自真实缺陷硬件的失效率(实机在环),而非模拟器代理指标(对照 [26] 用 gem5 覆盖率作代理)。
- **"选择概率 ≠ 失效率"的明确区分**(Fig.8 旁注):policy 收敛与检测效力是两个度量,避免常见误读。

## 横向对比注记

- **[26] Micro'25 引本文为 [9]**("参数优化,与 Harpocrates 正交")——精读后完全证实:五路线测试生成/优化分类学中本文占据"输入空间优化"一席,与四条生成路线正交可组合。
- **OpenDCDiag 直系血统 / sdcshield 测试族原始设计文档**: 本文 Table I 所列 eigen_gemm/eigen_sparse/eigen_svd/eigen_svd_jacobi 即本仓(sdcshield,fork 自 OpenDCDiag)的测试族——**本文回答了这些测试"为何存在"(2019 CSP 客户报告)、"为何长这样"(Eigen 模板快速原型 + GEMM 压 FMA + 随机 seed 参数化)**;sdcshield 已知的 eigen_svd/eigen_sparse 多线程 ULP 敏感性问题即发生在这条 5 年制造验证谱系之上。
- **Intel 内部测试优化谱系(全链一手)**: van de Ven ITC'21 OpenDCDiag [10] → Lerner ITC'22 优化 [9] → Shamsa & Lerner IRPS'24 缺陷机理 [4](15 号笔记 [45] 已引用)→ Bittel 等 IRPS'24 AI 影响 [7] → **本文 IRPS'25 RL**——同一批人(Shamsa/Lerner/Bittel/Inkley/Macieira)五年五篇,从工具开源到 AI 优化的完整演进弧。
- **[29] ETS24 的 Lerner 量化源**: [29] 引 "Lerner [35]:100,000 SoC @10 FIT → 每月至少一次 SDC 事件"——Lerner 即本文作者,Intel 的 SDE 量化话语源;[29] Athens 组与 Intel 产业数据的引用关系又一节点。
- **[50] iolts24-quantify(Macieira,R7 待读)**: Thiago Macieira 为本文作者之一(07 号笔记已标注 "Intel OpenDCDiag/Macieira")——OpenDCDiag 核心人员,R7 读 [50] 时回链本文。
- **两大厂商的两条智能路线对照**: Google [25] SiliFuzz 用 fuzzing **生成**程序 vs Intel 本文用 RL **优化**既有测试输入——同一时代(2022 vs 2025)对同一问题(搜索空间大 × 检测率低)的两种产业解法。
- **与 [28] Vega 的物理机制互补**: 本文 FMA 上下电瞬态激活 vs Vega BTI 老化激活——瞬态 vs 慢性;两者都把"物理激活机制"作为测试设计的出发点,是 bottom-up 思想在产业侧(本文)与学术侧([28])的平行表达。
- 致谢名单 Gill/Pullen/Majumdar/Bittel 疑似与 [47] Dark Side(Intel,R7 待读)作者群相关——R7 阅读时验证,此处仅记录名单事实。

## 身份核实

- **venue 核实**: 文件名 "IRPS ... 2025" + 版权行 ©2025 IEEE + 页眉 8C.1-1..8C.1-5 三者一致;文本层无 DOI/会议全称,IRPS 2025 为一致推定(与 [29] 的推定方法论同,已标注)。
- **页数核对**: PDF 6 页,末页空白;实读 8C.1-1 至 8C.1-5 全部内容,无截断。
- **文本层已知损失**: Fig.2–8 为图像(利用率剖析图 2-5、RL 架构图 6、性能汇总图 7、策略演化图 8),图中数值不可提取,内容依赖图注 + 正文描述复原;Table I 三列(测试名/尺寸/数据类型)在文本层交错,已按行复原且与正文(eigen_gemm 子目录、四类测试)互证一致。
- **内部矛盾检查**: 无。"one-thousand interactions"(§II.C)与 Fig.8 "250, 500, and 1000 learning iterations" 一致;[26] 对本文"参数优化"的定位与本文内容一致;Table I 测试名与 §II.A 正文(eigen_gemm 子目录、GEMM 聚焦)一致。
- **疑点诚实记录**: 实验一基于单一缺陷样本("for this defect")——论文内数据不支撑泛化性结论;泛化性由"已部署现役 Xeon 制造测试"的生产声明背书(工程有效性证据),二者性质不同,分开陈述。

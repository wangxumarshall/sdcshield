# [35] 用硬件计数器检测静默数据损坏(IEEE CLUSTER 2025)

## 标题与出处

- **标题**: Detecting Silent Data Corruption From Hardware Counters
- **出处**: 2025 IEEE International Conference on Cluster Computing (CLUSTER);DOI 10.1109/CLUSTER59342.2025.11186479;ISBN/版权行 `979-8-3315-3019-8/25/$31.00 ©2025 IEEE`(p.1 页眉);IEEE Xplore 正式版(下载戳见身份核实)。
- **类型**: 学术会议论文(全文 13 页 + 空尾页)。
- **同组关系**: 即本集 [34](sparse-pmc poster)所引 "our previous study [4]"——同组(UMass Lowell)先发的通用完整版,[34] 为其稀疏矩阵+决策树特化后继。

## PDF 与实读范围

- **文件**: `ref/Detecting Silent Data Corruption from Hardware Counters.pdf`
- **页数**: 14 页(第 14 页仅下载戳,空尾),**p.1–13 全文实读,无截断**。

## 作者全列(p.1)

Minseop Choi, Taha Azzaoui, Kyle Chaisson(第一排);Orlando Arias, Seung Woo Son(第二排)

## 机构分列

- **学术机构**: University of Massachusetts Lowell(麻省大学洛厄尔分校,Dept. of Electrical & Computer Eng.,五人全部)。
- **企业机构**: 无(纯学术小组)。

## 企业合作证据(三级)

- **一级(作者机构)**: 无企业作者。
- **二级(致谢/资助)**: NSF Grant No. 2312982(§V 末致谢;与 [34] 同一资助)——政府资助,无企业资助。
- **三级(版权页)**: IEEE 版权行 + 每页 IEEE Xplore 下载戳 "Authorized licensed use limited to: UNIV OF MASS-LOWELL. Downloaded on May 29,2026 at 13:27:50 UTC from IEEE Xplore"。
- **间接层**: ① **Intel**: perfmon-events.intel.com [27]、Intel 64/IA-32 架构性能监控事件手册 [28]、Skylake 计数器体系(§IV-B2)——方法的全部硬件物质基础;② **Meta/Google/Alibaba**: Dixit [16]、Hochschild HotOS'21 [24]、Wang SOSP'23 [52] 作动机与 ECC 不足论证引用;③ **NVIDIA**: PerfKit 用户指南 [37]、Hari 等(两级模型估 SDC 率)[23];④ **MACORD** [48](Subasi/Di/Balaprakash/Unsal/Krishnamoorthy 等 Intel 系 + Cappello/ANL)作为直接对比基线;⑤ **AID** [15](Di/Cappello,ANL)对比基线;⑥ IRPS 2024 "Data Center Silent Data Errors: Implications to AI Workloads" [3](Bittel 等,产业会议);⑦ CloudLab(NSF 测试床)[18]。

## 核心结论

1. **问题定位(§I)**: SDC "比人们普遍认知更普遍"——Meta/Google/Alibaba 三家 fleet 研究证实数百 CPU 中的设备特性导致数据中心 fleet 级 SDC [16][24][52];HPC 软件栈普遍缺乏告知科学家数据受损的机制;现有防线要么架构/系统级 [16][24] 要么算法级及以上 [10][11][26][30][32][46];ABFT 校验和 [45][47] 计算开销大,Online-ABFT [12] 算法特定——需要通用低开销检测框架。
2. **错误模型论证(§I, 引 [40] AVGI)**: 随机位翻转未必真实——指数位翻转偏差大、易检;双精度尾数 32 个 LSB 翻转引入可被计算吸收的微小误差——因此改用数据级高斯噪声模拟现实 SDC。
3. **数据结构级故障模式分析(§II-A/§III-A, Fig.1/2/4)**: CRS 三数组损坏分类学——rows 数组损坏改变元素行位置→访问模式与循环步长变化;columns 数组损坏选错乘数→访问模式变化;values 数组损坏直接改变结果→FP 微架构事件变化;rows 数组按定义非降序,部分错误可静态判出。
4. **PMC 作为计算图摘要(§II-A/§III-B)**: PMU 是运行时计算图的摘要,计数器值**不依赖输入矩阵尺寸**(可扩展性来源);对比指令级跟踪 [7](Calhoun 等)两大缺陷——逐指令跟踪代价随输入规模爆炸、需预知有效计算图。
5. **传播规律(§III-C, Fig.5)**: 400×400、稀疏度 0.9975、单错误注入、迭代 SpMV——错误位置相对非零元素**直接决定**传播模式(消失/有界/逐迭代增长三类);PMC 与传播中的错误直接相关(预判计数器受影响的初始分析得到验证)。
6. **分类器系统比较(§IV-A4, Table II)**: 7 分类器(LR/k-NN/DT/GB/RF/MLP/RBM+LR)× 7 数据集(4 真实 SuiteSparse: 494_bus/662_bus/can_62/bcspwr03 + 3 合成 HPCG grid-4/8/16)k 折交叉验证;**DT 最优**,与更贵的集成方法(GB/RF)相当;LR 差→计数器与 SDC 对数似然存在**非线性关系**;k-NN 平庸→输入数据级 SDC **缺时空相关性**(并解释依赖空间特征的先前方法 recall 更低);MLP 在真实数据集上偏低;RBM+LR 最差(潜特征提取失败)。
7. **性能与对比(§IV-B2, Table III, Fig.6)**: 计数器采集开销 **<2%**;推理开销 261µs–13,386µs(10⁶ 次 HPCG 全运行均值,全部 <1s 可忽略);训练 0.02–0.03s(LR/DT)至 ~2s(MLP);Table III 直接对比——AID(FTI 库,recall 0.84,6.3%)、MACORD(状态变量,0.95,5%)、NN-SDC(0.90,195.1%)、本文 DT(**零修改,0.91,2%**)。
8. **关键计数器(§IV-B2)**: Skylake 8 可编程 + 3 固定计数器;决策边界最重要贡献者:IDQ.ALL_DSB_CYCLES_4_UOPS(DSB 对齐 32)、ICACHE_16B.IFDATA_STALL(L1i 缺失取指停顿)、FP_ARITH_INST_RETIRED.SCALAR_SINGLE(FP 标量指令)——与三类数组损坏的微架构效应预判一一对应。
9. **局限与部署(§IV-C/D)**: 无错时计数器也有变异(顺序计算结构、前序操作缓存状态延续、中断)→需操作独立性但完全隔离开销大;可用计数器仅 4–8 个→最有影响计数器随数据集漂移,威胁通用机制地位,需 per-data/per-application 选择(留未来工作);生产系统 PMC 基本闲置;部署路径:perf 用户态工具(零 HPC 运行时修改)或插桩编译器 `#pragma` 指令 + 中心节点分类广播。
10. **ECC 不足论证(§IV-A1)**: ECC 假设奇偶计算时数据正确;CPU 故障产生的损坏发生在奇偶计算之前→用损坏数据算出的奇偶检不出(引 Meta [16]/Alibaba [52])——与 fleet 论文同口径。
11. **结论与未来(§VI)**: 自称**首个**用硬件计数器系统评估 SDC 对关键 HPC 内核影响的工作;DT/GB 四指标(precision/recall/accuracy/F1)接近 1;未来:非对称矩阵、更广内核、更宽错误率范围、PAPI 替代 perf、检测后纠错机制评估。
12. **平台(§IV-A1)**: CloudLab,2× Xeon Silver 4114(10 核 2.20GHz)、192 GiB ECC DDR4-2666、Ubuntu 22.04.5 LTS、kernel 5.15.0-122-generic——与 [34] 完全一致。

## 分类学标注

- **根因机理**: 不研究——数据级高斯噪声注入模拟 SDC 后果;动机段列举成因(硬件故障、电磁干扰、功率波动、宇宙射线、温度)但不建模。
- **故障模式**: ① 位置依赖传播三模式(消失/有界/增长,Fig.5);② **CRS 数据结构级损坏三分类**(rows/columns/values→访问模式或 FP 事件变化)——本集少见的显式数据结构级故障模式分类;③ 尾数/指数位翻转的可检性差异(引 [40],作为错误模型设计依据)。
- **检测技术**: **运行时在线检测——硬件 PMC + 监督学习(7 分类器系统比较)**;零程序修改、零额外核、可在任意检查点调用;触发后交 checkpoint-restart(纠错超出范围);生命周期:运行期(HPC 作业内)+ 部署期讨论(§IV-D)。
- **处理技术**: 无(仅检测;§III-B 提及检测后可用 checkpoint-restart 纠正,明确"error correction is beyond the scope")。

## 业界观点摘录

- "A recent research study at Meta, Google, and Alibaba also confirmed that fleets of servers in data centers are experiencing SDC due to device characteristics inside hundreds of CPUs [16], [24], [52]."(§I——三大 hyperscaler fleet 研究作为学术动机的标准引用)
- "Jaguar, a petascale supercomputer at ORNL, suffers a double-bit memory error once every 24 hours [21]."(§I,引 Geist/IEEE Spectrum)
- "the rapid emergence of ASIC accelerators for neural engines, which are prone to higher error rates than general-purpose computing hardware [58]."(§I)
- "ECC protections available in DRAM are insufficient for detecting data corruption caused by a malfunctioning CPU, as ECC assumes that the data is correct during parity calculation... Corruptions that occur before parity is computed will go undetected, since parity is calculated using corrupted data [16], [52]."(§IV-A1)
- "Hardware PMCs largely go unused in production systems, while mostly being utilized during development cycles."(§IV-D——**生产期 PMC 闲置**的产业实践观察,综述中值得引用的缺口论据)
- "current HPC software stacks largely lack mechanisms to inform scientists of data compromises."(§I)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| >0.91 | 平均 recall(摘要口径) | Abstract |
| ≤94% / ≤96% | 声称最高 accuracy / recall(对应格见身份核实) | Abstract, §I |
| <2% / 2% | 计数器采集开销 / Table III 总开销 | Abstract, §IV-B2, Table III |
| 0.84 / 6.3% | AID recall / 开销(FTI 库插桩) | Table III |
| 0.95 / 5% | MACORD recall / 开销(状态变量) | Table III |
| 0.90 / 195.1% | NN-SDC recall / 开销 | Table III |
| 7 / 7 | 分类器数(LR/k-NN/DT/GB/RF/MLP/RBM+LR)/ 数据集数(4 真实+3 合成) | §IV |
| 0.971 / 0.964 | 复原 Table II 中最高 DT accuracy / recall | Table II(复原) |
| 261µs–13,386µs | 各分类器推理开销(10⁶ 次 HPCG 全运行均值) | Fig.6 |
| 0.02–0.03s / ~2s | LR/DT vs MLP 训练时间 | §IV-B2 |
| r∈[0.01,0.1] / σ∈[0.1,1] | 注入率(非零元占比)/ 错误率(高斯标准差) | §IV-A3 |
| 1% vs 2% | 最低注入率 vs AID 评估下界 | §IV-A3 |
| 100 | 每数据集无错计数器采集次数 | §IV-B |
| 8+3 | Skylake 可编程 + 固定计数器数 | §IV-B2 |
| 400×400 / 0.9975 | Fig.5 传播实验矩阵尺寸/稀疏度 | §III-C |
| 2× Xeon Silver 4114 / 192 GiB ECC DDR4-2666 | CloudLab 平台(与 [34] 完全一致) | §IV-A1 |

## 方法论要点

- **错误模型真实化论证链**: 位翻转(指数易检/尾数被吸收 [40])→ 数据级高斯噪声(r×σ 双参数化)→ 显式下界对标(AID 2%,本文 1%)——错误模型设计本身作为一等贡献,而非默认随机位翻转。
- **分布级而非逐计数器分析**: 贡献 2 明确针对 [23] "少数计数器与 SDC 弱相关"的发现,改为整体分布(with/without SDC)分析——从"找相关计数器"到"读分布位移"的方法论升级。
- **分类器行为作为 SDC 结构探针**: LR 失败→非线性;k-NN 平庸→无时空相关;MLP 小数据上易过拟合——用模型族行为差异反推 SDC 数据结构,是"模型选择即诊断"的范例。
- **类别平衡的非复制解**: 多次跑 HPCG 无错运行(每次生成新问题)替代随机过采样复制——避免少数类复制导致的过拟合(§IV-A4)。
- **采集机制权衡(§III-B)**: perf 子系统(抽象+托管调度,上下文切换开销)vs 直接 MSR/rdpmc(零内核陷入,但需自行调度且有抢占噪声)——框架选后者;与 §IV-D 表述存在张力(见身份核实)。
- **规模无关性论证**: 计数器值不依赖输入矩阵尺寸——对逐指令/逐数据跟踪 [7] 的根本性优势,轻量路线可扩展性的来源。

## 横向对比注记

- **与 [34] sparse-pmc(同组后继 poster)**: 本文=通用完整版(7 分类器×7 数据集、k 折 CV、机制权衡、部署讨论);[34]=决策树特化 + 两阶段 5,670→8 PMC 精选工作流;[34] 引本文为 "our previous study [4]";平台、NSF 资助 2312982、注入参数(r↔λ, σ 同义)、未来工作(#pragma 插桩+中心节点广播)全部同源;[34] 的 DT 数字(0.919/0.943/0.811/0.892)与本文同矩阵 DT 数字不同——系重跑而非照抄。**编号警示**: 本文内部引文 [34] = Moon/Kim/Chen/Son AI4Sys'23(ADSP 注入方法学)≠ 本精读集 [34](sparse-pmc poster)——综述写作时须防混淆。
- **与 ABFT 谱系 [45][47][12][10][26][30][33]**: 零侵入 PMC 检测 vs 算法内校验(可检可纠但算法特定、开销大);Table III 把应用级路线(AID/MACORD/NN-SDC)纳入统一对比。
- **与 [33] Orthrus(同批)**: 确定性重执行(检出 87–96%、4% 时间开销、需专用验证核、APP/VAL 双进程)vs PMC+ML(recall ~0.91、<2% 开销、零修改零额外核)——运行时检测确定性翼 vs 启发式翼;口径差异(故障检出率 vs 分类 recall)综述须并注。
- **与 [36] SHOUT-Trainer/[37] kg-vulnpred(同批)**: 本文是"经典 ML 足矣"的关键数据点——DT/GB 压过 MLP/RBM,小样本下树模型占优;与 [36] 的 Transformer、[37] 的异质知识图构成 ML 复杂度谱系三端,并为"何时需要更深模型"提供边界证据(小数据、无时空相关→深模型无优势)。
- **与 [40] AVGI(Papadimitriou & Gizopoulos,=[29] ets24 同组雅典系)**: 尾数/指数位翻转可检性论证引自 [40] HPCA'23——微架构脆弱性评估(AVF 谱系 R2)与数据级错误模型设计的知识衔接。
- **与 [23] Hari 等(两级模型)**: 本文直接回应其"PMC 与 SDC 弱相关"结论(GPU 侧)——CPU 侧以分布级分析翻案,构成跨平台对话。
- **与 R7 [44] recommendation**: 同为 ML×SDC 但域不同(HPC 内核 vs 推荐系统);本文引 [31][32] Li 等(Meta,低精度 DNN 推荐软错检测)——与 [44] 相邻但不同文。
- **Intel/NVIDIA 间接依赖**: PMU 文档体系 [27][28] + GPU 侧 PerfKit [37]/两级模型 [23]——产业硬件监测接口与产业研究作为学术检测路线的物质基础和对话对象。

## 身份核实

- **出处三重一致(确证)**: CLUSTER 2025 会议戳(p.1 页眉)+ DOI + ISBN/版权行;IEEE Xplore 下载戳 2026-05-29 13:27:50 UTC,授权 UNIV OF MASS-LOWELL——正式出版版本无疑(与 [34] 的"出处待考"形成闭环:[34] 引文格式与本文 DOI 前缀一致)。
- **作者集与 [34] 引文不一致**: 本文 PDF 5 人(Choi, **Azzaoui**, Chaisson, Arias, Son);[34] 的引文 [4] 仅列 4 人(无 Azzaoui)且顺序不同(Choi, Chaisson, Arias, Son)。两种读法: [34] 引文遗漏作者,或投稿-终稿间作者变动;不强行归一,记录备查。
- **§III-B 与 §IV-D 采集机制张力**: §III-B "We limit the runtime overhead of our approach by using the latter(**directly writing to model-specific registers**)";§IV-D "Our data collection is done through the **perf subsystem** in the Linux kernel... we opted for the latter mechanism(**perf userspace tools**)as it requires no changes to the HPC runtime"。两种读法: (a) 框架库用 MSR 直编程(贡献 2 所述 library),评估阶段的数据采集用 perf 用户态工具;(b) 行文前后不一致。均如实记录——综述引用开销 <2% 时应注明口径为计数器采集开销。
- **Table II 文本层交错(复原)**: 7×7 网格按分块复原;首块(7 分类器指标)归 494_bus(标签列整体错位一行的典型提取伪影);赋值锚点: 文中明言 MLP 在 662_bus/bcspwr03/can_62 偏低→对应 MLP 最低的三个块;DT 总体最优的文字断言成立。块级赋值为复原结果,个别数字-数据集配对可能仍有错位,引用单格数字时须谨慎。
- **"up to 94% accuracy / 96% recall" 对应关系**: 复原表中最高 accuracy 0.971(DT)、最高 recall 0.964(DT)——96% 可对应 0.964;**94% 无精确对应格**(候选: GB/bcspwr03 accuracy 0.939≈94%,惟依赖块级复原);摘要 ">0.91 average recall" 与 Table III "0.91 (with DT)" 一致。不强行归一,两读并存。
- **上标丢失**: Fig.6 说明 "across 106 full HPCG runs" = **10⁶ 次**(上标丢失);纵轴 103/104 = 10³/10⁴;已按上下文复原。
- **页数核对**: PDF 14 页,第 14 页仅下载戳(空尾);p.1–13 全文实读(正文 p.1–11 + 参考文献 p.11–13)。
- **邮箱下划线丢失**: "minseop choi@student.uml.edu" 等系文本层提取伪影(实际为 minseop_choi@),无实质影响。
- **希腊字母 σ/λ**: 部分丢失,按 §IV-A3 上下文复原(σ=高斯采样标准差,r=注入率;[34] 用 λ 表注入率,同一义)。
- **Table I 描述照录**: can_62 "Mechanical structures problems in aircraft design"(航空器设计机械结构问题);494_bus/662_bus 电力系统网络矩阵;bcspwr03 电网基准——与 [34] 同四个真实矩阵。

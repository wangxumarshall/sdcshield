# [36] SHOUT-Trainer:基于 Transformer 的闭环 SDC 猎寻与观测(IOLTS 2026)

## 标题与出处

- **标题**: SHOUT-Trainer: Closed-loop Trainer for Silent Data Corruption Hunting and Observation Using Transformers
- **出处**: 2026 IEEE 32nd International Symposium on On-Line Testing and Robust System Design (IOLTS);DOI 10.1109/IOLTS69666.2026.11633678;ISBN/版权行 `979-8-3315-4685-4/26/$31.00 ©2026 IEEE`(p.1 页眉);IEEE Xplore 正式版。
- **类型**: 学术会议论文(6 页,正文+参考文献 5 页)。

## PDF 与实读范围

- **文件**: `ref/SHOUT-Trainer_Closed-loop_Trainer_for_Silent_Data_Corruption_Hunting_and_Observation_Using_Transformers.pdf`
- **页数**: 6 页(第 6 页空尾),**p.1–5 全文实读,无截断**。

## 作者全列(p.1)

S. Maryam Ghasemi\*, Shanmukha Mangadahalli Siddaramu\*(共同一作), Mehdi B. Tahoori

## 机构分列

- **学术机构**: Karlsruhe Institute of Technology (KIT) 计算机系(卡尔斯鲁厄理工学院,德国,三人全部)。
- **企业机构**: 无。

## 企业合作证据(三级)

- **一级(作者机构)**: 无企业作者。
- **二级(致谢/资助)**: **无致谢章节、无资助声明**(§IV 结论后直接进参考文献)——如实记录。
- **三级(版权页)**: IEEE 版权行 + IEEE Xplore 下载戳 "Authorized licensed use limited to: **Shanghai Jiaotong University**. Downloaded on September 28,2026 at 08:13:53 UTC"(上海交通大学订阅)。
- **间接层**: ① **Google**: Mitra 等 IEEE Design & Test 2025(10× 测试逃逸,= 本集 [09])双引为 [1][3],其 fleet 数字(5K DPM 逃逸/~1K DPM 致 SDC/每十亿芯片小时数十万错误输出)是全文动机;Hochschild HotOS'21 [19];Marinissen 等 ETS'24 [8](Google+学界,"SDC:测试还是可靠性问题?");② **Meta**: Dixit arXiv [2]、FleetScanner "Detecting SDCs in the wild" arXiv 2203.08989 [5](= 本集 [03] Ripple 前身)、Dixit/Sankar 参与的 VTS'23 [7][13] 与 IOLTS'23 stealthy-saboteurs [14](= 本集 [49])、ETS'24 [15];③ **Alibaba**: Wang SOSP'23 [6](= 本集 [01]);④ LLM 产业工具:ChatGPT/Claude/Gemini 作为**方法内部的代码生成引擎**(§II-A 训练语料来源);⑤ Rosetta Code 语料库 [32];⑥ CodeBERT 微调先例 Zou 等 Sci. Reports 2025 [35]。

## 核心结论

1. **问题定位(§I)**: FI 是量化 SDC 的标准方法但计算昂贵、扩展性差——每个新程序版本、输入集、编译器配置都需重注——无法作为软件开发中的交互式工具;系统级监视器只能发现损坏输出,对触发/放大损坏的软件区域无可见性,开发者须人工分析大代码库定位脆弱指令模式。
2. **动机数字(§I, 引 [3])**: "Recent fleet studies report manufacturing test escapes approaching **5K DPM**, with nearly **1K DPM** manifesting as SDC-causing devices and producing **hundreds of thousands of incorrect outputs per billion chip-hours**"——Google fleet 数据直接流入学术 ML 工作。
3. **方法总览(§II, Fig.1)**: SHOUT 五段流水线——① 程序策展(ChatGPT/Claude/Gemini 生成 + Rosetta Code,覆盖哈希/搜索/数值/排序/ML/密码/图处理);② 自研 **adad 库**插桩(操作级故障概率,仅需 include 头文件 + 变量声明改 `adad` 类型);③ 蒙特卡洛 FI(记录 masked/SDC/DUE);④ CodeBERT 回归器预测每操作 SDC 概率;⑤ Re-trainer LLM 闭环再训练。
4. **概率模型(§II-B)**: PSDC(p) = p(1−p)^(n−1)·PSDC(it:k) + O(p²) → 软错误率 p≪1 时一阶近似 PSDC(p) ≈ p·Σₖ PSDC(it:k)——只需单故障单迭代评估;Spearman 秩相关验证注入迭代均匀随机采样无偏。
5. **故障模型与注入粒度(§II-B/§III-A)**: 单位翻转 [33][34];注入在**操作级**而非语法算符级;四类算符(算术/比较/布尔/赋值),32 位整数运算(方法可扩至浮点);每站点 N=100 次重复注入;理论噪声下限 MAE≈0.026。
6. **预测器(§II-D)**: CodeBERT(12 层 Transformer 编码器)微调,`<TARGET>` 标签包裹目标操作,512 token 窗口居中对齐,[CLS] 768 维嵌入 → 回归头 Linear(768→256)→ReLU→Dropout(0.4)→Linear(256→1)→sigmoid,加权 MSE 损失——**从源码上下文直接回归连续 SDC 概率**。
7. **开环性能与消融(§III-B)**: 1.5M 行 C++/13,000+ 程序/110K 标注站点(DOL);开环 MAE=0.171、RMSE=0.258、R²=0.521、Pearson r=0.742;基线阶梯——全局均值预测器(R²=0, MAE=0.349)、逐操作类型均值预测器(R²=0.085, MAE=0.327)——**算符类型只解释 8.5% 方差**,证明模型学到的是程序级结构模式而非算符启发式。
8. **闭环再训练(§II-E/§III-C)**: 弱预测站点判据 |P̂−PSDC|≥0.25(误差中位数与 1σ 边界之间,覆盖最差 ~24% 站点)→ ~5K 弱站点/3.6K 程序;Re-trainer LLM 按误差方向生成变体——**amplify 模式**(低估站点,生成更易传播故障的代码)与 **mask 模式**(高估站点,生成更易掩蔽/崩溃的代码);二次定向 FI 产出 21K 反馈站点(DFB,均值 PSDC=0.65、零 SDC 占比 15% vs DOL 的 31%——确证反馈机制命中难预测区域);合并重训得 MCL。
9. **闭环收益(§III-C)**: 整体 MAE 0.171→0.146(−6%)、R² 0.521→0.61(+17%);**弱站点 MAE 0.460→0.261(−43.3%)**;逐算符改进 36.1%(A!=K)至 67.3%(A=AA),A<A 是最大聚合预测负担(高频)且加权误差降 52.2%;改进跨算符一致→泛化而非过拟合。
10. **结论(§IV)**: 预测—定向数据生成—再训练的持续闭环;确认反馈数据集有效命中最难预测区域,实现更准确的 SDC 脆弱性估计。

## 分类学标注

- **根因机理**: 不研究——操作级单位翻转模拟故障;自认程序级 FI 未必完全捕捉瞬态硬件故障传播(引 [22] vuln-stack),但作为可扩展的实用评估路线。
- **故障模式**: 结果三分 masked/SDC/DUE;**算符类别依赖**(16 类中 A<A 最难预测、A=AA 改进最大);每操作 SDC 概率作为连续量——脆弱性的操作级粒度刻画。
- **检测技术**: **非运行时——软件开发期(设计期)脆弱性预测**: 源码上下文 → 每操作 SDC 概率回归;配套 FI 方法论(adad 操作级注入库,蒙特卡洛);生命周期:开发期/CI 交互式分析(动机即"让 FI 可交互化")。
- **处理技术**: 无(预测结果供开发者定位脆弱代码区域;指称指令复制谱系 [18] 为下游)。(Task 10 已核:本文 [18] = Didehban et al., gZDC, TDSC vol.21 no.1 pp.78–92——语料外论文,非 digest [43])

## 业界观点摘录

- "Recent fleet studies report manufacturing test escapes approaching 5K Defective Parts per Million (DPM), with nearly 1K DPM manifesting as SDC-causing devices and producing hundreds of thousands of incorrect outputs per billion chip-hours in production deployments [3]."(§I——Google fleet 数字的标准引用形态)
- "These failures propagate through operating systems and applications as corrupted data, offering no hardware indication of malfunction and requiring substantial engineering effort to diagnose [4]–[7]."(§I)
- "many SDCs are triggered only under specific instruction sequences, operand patterns, or compiler optimizations that activate marginal microarchitectural behavior [8]."(§I——边缘微架构行为框架,引 Marinissen ETS'24)
- "system-level monitors can detect corrupted outputs, they provide little visibility into the software regions that triggered or amplified the corruption. Consequently, developers must analyze large code bases to localize vulnerable instruction patterns, a process that is slow, expensive, and often fleet-wide [2]."(§I——**定位缺口论述**:运行时检测与开发期定位之间的空档,即本文动机)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 43.3% | 弱预测站点 MAE 降幅(0.460→0.261) | §III-C |
| 0.171 / 0.258 / 0.521 / 0.742 | 开环 MAE / RMSE / R² / Pearson r | §III-B |
| 0.146 / 0.254 / 0.61 | 闭环 MAE / RMSE / R²(整体 −6% MAE,+17% R²) | §III-C |
| 0.349 / 0 与 0.327 / 0.085 | 全局均值 / 逐算符均值基线 MAE / R² | §III-B |
| ~0.026 | N=100 注入的理论 MAE 噪声下限 | §III-B |
| 36.1%–67.3% | 逐算符 MAE 改进区间(12/16 类) | §III-C, Fig.3b |
| 52.2% | A<A 加权 MAE 降幅(最大聚合负担) | §III-C |
| 1.5M / 13,000+ | C++ 代码行数 / 程序数 | §III-A |
| 110K / 21K | DOL / DFB 标注故障站点数 | §III-A, §III-C |
| ~5K / 3.6K | 弱预测站点数 / 跨程序数 | §III-C |
| 0.25 / ~24% | 弱站点误差阈值 / 覆盖比例 | §III-C |
| 0.65 / 15% vs 31% | DFB 均值 PSDC / 零 SDC 占比(DFB vs DOL) | §III-C |
| N=100 | 每注入站点重复注入次数 | §III-A |
| 4 类 / 16 类 | 注入算符大类 / 被分析算符类型数 | §III-A, §III-C |
| 512 / 768 | token 窗口上限 / CLS 嵌入维度 | §II-D |
| 5K / ~1K DPM | fleet 测试逃逸 / 致 SDC 比例(引 Google [3]) | §I |

## 方法论要点

- **闭环 = 主动学习 + LLM 合成数据**: 预测误差驱动定向数据生成(amplify/mask 双模式按误差方向)再重训——把 FI 从"离线一次性活动"改造成"按需定向的反馈引擎";DFB 分布验证(均值 0.65、零 SDC 占比骤降)证明命中难例区域。
- **LLM 作为数据引擎而非仅研究对象**: ChatGPT/Claude/Gemini 生成训练语料 + Re-trainer 生成变体——产业 LLM 工具嵌入学术方法内部,是本集首个此类模式。
- **基线阶梯消融**: 全局均值(0)→算符类型均值(8.5%)→CodeBERT(52.1%)——用最笨基线证明信号来自程序级结构,方法论上干净的归因。
- **一阶概率分解**: p≪1 时多故障项可忽略 → 单故障单迭代评估;配合 Spearman 秩相关验证迭代均匀采样无偏——FI 成本的理论合法化。
- **噪声下限量化**: N=100 对应 MAE≈0.026 的标签固有噪声——回归指标的测量不确定度意识(与 [34]/[35] 的分类指标族不同,综述对比须注明口径)。
- **操作级 vs 指令级注入**: adad 在源码操作级注入(C/C++ 运算符重载),比 MIR/指令级(如 [33] Orthrus 的 REFINE)离硬件更远,但换得源码级可解释反馈(`<TARGET>` 标注直接指向代码行)。

## 横向对比注记

- **与 [37] kg-vulnpred(同批,下一篇)**: 本文引其为 [9](Wen 等,TDSC vol.23 no.1, 2026.1, pp.1173–1190)——同为"免 FI 预测脆弱性"路线:知识图(结构化传播知识)vs Transformer(源码上下文回归);两者构成预测特征空间的两端,精读 [37] 时直接对照指标口径。
- **与 [34]/[35](同批)**: 特征空间三轴全异——PMC 运行时信号(执行级、每运行)vs 源码上下文(静态、每操作);指标族——分类 accuracy/recall vs 概率回归 MAE;生命周期——运行期检测 vs 开发期预测。[34][35][36][37] 合起来构成"ML×SDC"的完整谱系矩阵(特征×生命周期×模型复杂度)。
- **与 [33] Orthrus(同批)**: 运行时确定性验证 vs 开发期统计预测——生命周期互补位;Orthrus 处理"已部署的 SDC",SHOUT 处理"未部署代码的脆弱性预知"。
- **与 R3 FI 工具([19] GemFI/[20] CHAOS/[21] diff-fi)**: adad 是轻量软件级操作级 FI 库,自认程序级 FI 不完全等价瞬态硬件故障传播(引 [22])——微架构级 FI(精确昂贵)vs 软件级 FI(粗但可扩展)的谱系两端。
- **与 [09] test-escapes(Mitra 等 D&T 2025)**: Google fleet 数字(5K/1K DPM/每十亿芯片小时数十万错误输出)作为动机直接引用且双引([1][3] 重复)——产业数据 → 学术 ML 的知识流。
- **与 [03] Ripple**: 本文引 FleetScanner arXiv 版 [5]——Meta 系列工作在学术引用链中的持续在场。
- **与 LLM-for-EDA 谱系 [26]–[31]**: 自我定位"首个直接从源码预测 SDC 脆弱性"(先前 LLM 工作=设计辅助/调试/指令级分析)——LLM 方法论从 EDA 向可靠性预测的扩张点。
- **欧洲学术纯血统**: KIT(Tahoori 组)——本集少见的纯欧洲(无美企合作、无中美 fleet 数据合作)SDC 论文,其物质依赖全在间接层(Google/Meta/Alibaba 引用 + 产业 LLM 工具)。

## 身份核实

- **出处三重一致(确证)**: IOLTS 2026(第 32 届)会议戳 + DOI 10.1109/IOLTS69666.2026.11633678 + ISBN/版权行;下载戳 2026-09-28 08:13:53 UTC,授权 Shanghai Jiaotong University(与本会话同日,经上海交大订阅的 IEEE Xplore 正式版)。
- **无致谢/资助声明**: 结论后直接进参考文献——无 funding 信息,如实记录(与 [34][35] 的 NSF 2312982 形成对照)。
- **参考文献重复**: [1]≈[3](Mitra 等 D&T 2025 同文双录,仅排版差异)、[7]≈[13](Singh 等 VTS'23 双录)、[25]≈[33](Eslami 等综述双录)——至少三组重复引文,轻度引用管理瑕疵,记录备查。
- **上标丢失**: 式(1)(2) 的 p^(n−1)、O(p²) 等上标在文本层丢失,已按上下文复原;p≪1 条件照录。
- **Fig.1 文本层噪声**: "Adad"(大小写)、"Locaions"(原文拼写错误或提取伪影)、"×(N) Times" 等流程图标签提取杂讯——不影响语义,图义经正文交叉验证。
- **页数核对**: PDF 6 页,第 6 页空尾;p.1–5 全文实读。
- **共同一作脚注**: \* 标注 Ghasemi 与 Siddaramu 等贡献——记录。
- **数字自洽**: 43.3% = (0.460−0.261)/0.460 ✓;R² 提升 17% = (0.61−0.521)/0.521 ✓;MAE 降 6% = (0.171−0.146)/0.171 ✓——内部一致。
- **[9] 即本集 [37] 的出版信息预取**: Wen 等 TDSC 2026.1——为下一篇精读提供出处锚点(仍以 [37] PDF 自身为准)。

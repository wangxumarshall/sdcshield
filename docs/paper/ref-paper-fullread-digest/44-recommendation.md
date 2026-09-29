# [44] 推荐系统对 SDC 的鲁棒性实证研究(ISSRE 2025,学术界×Meta 直接合作)

## 标题与出处

- **标题**: Understanding Recommendation System Robustness Against Silent Data Corruption: An Empirical Study
- **出处**: **ISSRE 2025**(IEEE 第 36 届软件可靠性工程国际研讨会),DOI 10.1109/ISSRE66568.2025.00016,ISBN 979-8-3503-9302-6/25,会议录 pp.25–36;工具 **PyTEI 开源于 github.com/facebookresearch/PyTEI**(Meta 官方 GitHub 组织)。
- **与 [03] ripple 的引文链互证**: 本文作者 Sriram Sankar(Meta)即 Ripple/FleetScanner 线(digest [03])的共同作者;本文引 [5](Ripple arXiv)/[6](Dixit at scale)/[4](SOSP23,即 digest [01])——**Meta SDC 研究者的谱系闭环**。
- **类型**: 学术界×产业界**直接合作**论文(Villanova × Meta,访问学者模式)。

## PDF 与实读范围

- **文件**: `ref/Understanding_Recommendation_System_Robustness_Against_Silent_Data_Corruption_An_Empirical_Study.pdf`
- **页数**: 13 页(pp.25–36 内容 + 末页空),**全文实读,无截断**(txt 分两段:1–560 / 561–921 行)。

## 作者全列(p.1)

Dongning Ma(维拉诺瓦大学)、Xun Jiao(维拉诺瓦大学,通讯)、Fred Lin(Meta)、Daniel Moore(Meta)、Sriram Sankar(Meta)——5 人 2 机构。

## 机构分列(p.1)

- **维拉诺瓦大学**(Villanova University,美国宾州):Ma、Jiao;
- **Meta**:Lin、Moore、Sankar ×3。
- **合作模式注记**: p.1 脚注 "**Work done during Xun Jiao's sabbatical at Meta**"——Jiao 在 Meta 休假访问期间完成;Sankar 是 Meta SDC 线(Dixit at scale 2021、Ripple SELSE'22,即 digest [03] 谱系)的核心作者。

## 企业合作证据(三级)

- **一级(作者机构)**: **有,直接且强**——3/5 作者隶属 Meta(Lin、Moore、Sankar);访问学者机制(sabbatical);成果开源于 **facebookresearch** 组织(PyTEI)。**这是 53 篇中(截至本篇)唯一一级企业共同作者论文**。
- **二级(致谢/资助)**: **无致谢/资助节**(§X 结论直接进入参考文献)——如实记录;但企业合作已由一级证据三重确证(作者机构+脚注+开源组织),不依赖致谢。
- **三级(版权页)**: IEEE 版权行 + DOI/ISBN;IEEE Xplore 下载水印"Authorized licensed use limited to: Shanghai Jiaotong University"(检索渠道痕迹,非企业协作)。
- **间接层(企业协作的显性证据)**: Meta 生态引用密集:[4] SOSP23 fleet(digest [01])、[5] Ripple、[6] Dixit 2021、[8] He ISCA'23(DL 训练硬件故障,**Google TPU 线**——经 digest [47] 引文语境核对,初记 Meta 有误,已更正)、[12] Hsia MP-Rec ASPLOS'23(79% 数字来源)、[13] MTIA 加速器 ISCA'23、[34] Hsia IISWC'20(DRS 跨栈负载刻画)、[46] Llama 3 技术报告;Google 侧:[7] Hochschild HotOS'21、[9] Dean/Vahdat Hot Chips'23 主题演讲、[26] Jouppi TPUv4i ISCA'21、[31] Wide&Deep。**判定:一级直接合作(Meta),产业视角内嵌于研究问题本身(DRS = Meta 主导负载)**。

## 核心结论

1. **问题定位(§I)**: SDC 已成数据中心显著可靠性威胁(Meta/Google/Alibaba 均报告需投入大量资源调试定根 [4][6][7]);Google 报告 TPU 中的 SDC [8][9];**深度推荐系统(DRS)是数据中心主导负载——2019 年占 Meta 全部 AI 推理周期的 79% [12],且有专用加速器 MTIA [13]**——但 AI 鲁棒性研究(MLP/CNN/GNN [10][11])从未覆盖 DRS;缺口双重:负载地位 + 架构独特性(神经网络层之外还有占参数 majority 的 embedding 表)。
2. **PyTEI 框架(§IV-B, Fig.2–4)**: 纯 PyTorch 依赖的用户友好/高效/灵活错误注入框架;用 `torch.view()` 类型转换做位翻转(**比 PyTorchFI/Ares/BinFI/GoldenEye/FIdelity 快 ~100×**,免 tensor↔其他容器往返);商用 CPU 上数秒完成 19M 参数模型 @ BER 1e-3 的注入;错误图生成(参数形状×位宽×BER→32-bit 整数位图);支持自定义错误模型与缓解方法挂钩。
3. **错误模型(§IV-A)**: 随机位翻转(伯努利,BER 1e-9→1e-2),注入目标 = 模型参数(DRAM/SRAM 存储,对齐 [10][11][15][27][35]–[37] 工业实验);局限如实声明(不覆盖突发错误 [38]、stuck-at、多位错误)。
4. **两阶段方法学(§V, Fig.5)**: **dummy 模型**(不训练、均匀初始化 [42]、合成数据——高斯 dense + 伯努利 sparse;RMSE 量化输出偏差;设计空间:MLP 深度 1/2、隐藏 64–512、embedding 维 64–512、稀疏度 0.001/0.01)→ **真实模型**(FM/DFM/AFM/DCN/WD 五模型 torchfm 实现、三数据集重训、AUC-ROC、10 次重复);dummy 先行论证:训练整个设计空间不现实,且真实模型实验与 dummy 观察对齐。
5. **dummy 模型发现(§VI, Fig.6–8)**: BER↑→RMSE↑;**指数字段位翻转可致 inf/nan**(0.625 第二位翻转→2.13×10³⁸);**MLP 隐藏层尺寸比 embedding 维度更伤鲁棒性**(MLP 每个权重都参与前向计算 vs embedding 每样本只索引稀疏子集);MLP 加深全局恶化(参数更多 + 错误经全连接传播放大);输入稀疏度×10(0.001→0.01)→ embedding 错误影响更显著(更多条目被索引)。
6. **真实模型发现(§VII, Fig.9–11, Table II)**: BER>1e-6 出现明显退化,1e-5/1e-4 严重跌落,**>1e-3 完全崩溃**;**embedding 表更鲁棒**(1e-3 后才显著跌落,且 10 次运行方差更小);MLP 重架构(DCN/DFM/WD)比 embedding 主导架构(FM)更脆弱;Criteo 数据集模型更不鲁棒(MLP 占比更高 + 稀疏度更低);**模型/数据集间鲁棒性差异高达 2 个数量级**——来源 = 架构差异 + 输入特征。
7. **缓解方法评估(§VIII, Fig.12)**: **ABFT**(GEMM 行和校验 + embedding 列和校验,检出即重执行):前作 [27] 在低精度 DRS 上 99% 检出/10% 假阳性/<26% 开销,但**本文场景不可行**——BER>1e-4 时每矩阵多错误强制持续重执行;结论:校验码类方法适用于宇宙线/辐射类间歇低速率软错误,**对逻辑/数据通路错误或电压频率缩放类高 BER 不可行**。
8. **激活裁剪 vs SBP(§VIII)**: 激活裁剪(ReLU 限幅 [-6,6],ReLU6 [45] 启发,算法级零硬件)与 SBP(保护符号位+指数位,需专用机制/硬件)各恢复 **5%–30%** 性能;**激活裁剪最优**——因它直击 MLP(最脆弱组件)且实现最廉;SBP 对 embedding 主导架构更有效;鲁棒性强的模型(FM/AFM)恢复也更好。
9. **研究问题回答(§IX-A)**: A1——不同组件鲁棒性不同,MLP 弱/embedding 强,**建议系统设计者优先保护 MLP**;A2——超参影响鲁棒性(MLP 隐藏尺寸 > embedding 维度),dense/sparse 比与输入稀疏度是 DRS 特有的鲁棒性维度;A3——ABFT 不适用,裁剪/SBP 有效,裁剪更优。
10. **局限与未来(§IX-B)**: 只做推理期(SDC 直接影响用户体验);**SDC 已实际影响 Meta 的 Llama 3 训练 [46]**(梯度爆炸/损失尖峰/不收敛 [8] 与推理期表现不同);错误模型单一;PyTEI 可泛化到 MLP/CNN/RNN/LLM。
11. **结论(§X)**: 首个 DRS×SDC 系统实证;MLP 组件尤其敏感,输入稀疏度亦影响鲁棒性;激活裁剪最有前景(最高 30% 恢复,极小开销)。

## 分类学标注

- **根因机理**: 采信多元画像——宇宙线/辐射(GPU 内存 [14][15])、制造缺陷/老化(脉动阵列加速器 [16][17])、近似计算(电压缩放 [19]–[21]、近似乘法器 [22])——错误率因此跨数量级,动机上要求宽 BER 扫描。
- **故障模式**: 模型参数位翻转(DRAM/SRAM);IEEE-754 指数字段位翻转 → inf/nan 爆炸;高 BER 多重错误(令 SEC-DED/重执行类失效)。
- **检测技术**: 评估 ABFT 校验和检测(判定高 BER 不适用)——本篇对检测技术的贡献是**否定性结论**;PyTEI 是评估基础设施(注入工具),非在线检测。
- **处理技术**: **应用层容忍/缓解**——激活裁剪(算法级,最优)、SBP(位级防护,需硬件)、ABFT 重执行(不适用);不检测不冗余,量化"坏了之后还能用多少"——与 R6 冗余线互补的第三条路。
- **生命周期**: 推理期防护(训练期列为未来工作);栈层级:**AI 应用/模型架构层**(参数与架构特性)——53 篇中首个纯 AI 应用层样本。

## 业界观点摘录

1. "Meta, Google, and Alibaba all report instances of SDCs in their data centers that require resource-intensive efforts to debug and root cause"(§I——三巨头叙事;注意引用 [4][6][7] 中未见 Alibaba 独立文献,见身份核实)。
2. "in 2019, DRS workload accounted for 79% of the overall AI inference cycles at Meta's data center [12]. Dedicated hardware accelerators, e.g., MTIA [13], are specifically built for DRS workload."(§I——负载地位的产业定量)。
3. "SDCs has impacted Llama 3 training in Meta [46]. SDCs in training often cause different outcomes than inference, e.g., gradient explosion, loss spike, or non-convergence"(§IX-B——Llama 3 训练受 SDC 影响的产业事实)。
4. ABFT 适用域裁定:"such error-correction-code based methods work for intermittent, low-rate soft and transient errors caused by, e.g., cosmic rays and radiation effects yet are infeasible for scenarios with higher BERs which can come from logic and data-path errors, or voltage and frequency scaling"(§VIII)。
5. 对从业者的建议:"we recommend future system designers to prioritize MLPs for protection"(§IX-A);激活裁剪"can be simply implemented by modifying the activation functions"(§VIII)。

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 79% | 2019 年 Meta AI 推理周期中 DRS 占比 | §I [12] |
| 5 / 3 | DRS 模型数(FM/DFM/AFM/DCN/WD) / 数据集数(MovieLens-1M/20M、Criteo) | §VII |
| 1e-9 → 1e-2 | BER 扫描范围 | §IV |
| ~100× | PyTEI 相对既有框架的注入加速 | §IV-B |
| 19M @ 1e-3 / 数秒 | 单次注入规模/耗时(商用 CPU) | §IV-B |
| 1e-6 | 明显退化起始 BER | §VI-B/§VII-B |
| 1e-5 / 1e-4 | 严重跌落起始 | §VII-B |
| 1e-3 | 完全崩溃阈值(也是 embedding 显著跌落起点) | §VII-B |
| 2 个数量级 | 模型/数据集间鲁棒性差异上限 | §VII-B |
| 5%–30% | 裁剪/SBP 的性能恢复区间 | §VIII |
| 30% | 激活裁剪最高恢复(摘要口径) | Abstract/§VIII |
| 99% / 10% / <26% | ABFT 前作 [27] 检出率/假阳性/开销 | §VIII |
| [-6, 6] | 激活裁剪范围(ReLU6 启发) | §VIII |
| 0.625 → 2.13×10³⁸ | 指数位翻转效应示例 | §VI-B |
| 45M / 13+26 | Criteo 点击记录用户数 / 连续+类别特征 | §VII-A |
| 10 | 每配置注入重复次数(取均值/范围) | §VII-B |
| 64–512 / 1,2 | dummy 模型隐藏层与 embedding 维扫描 / MLP 深度 | Table I |
| 0.001 / 0.01 | 输入稀疏度两档(10×) | Table I |

## 方法论要点

1. **dummy→真实两阶段评估**: 未训练合成模型先行扫设计空间(省 GPU 时)→ 真实模型验证对齐——大规模鲁棒性实验的成本控制范式,且给出对齐性证据。
2. **组件级归因**: 把 DRS 拆为 MLP/embedding 两类组件分别注入——鲁棒性差异的架构解释(稠密全参与 vs 稀疏索引掩蔽)而非黑盒现象。
3. **负结果的价值**: ABFT 不可行的论证(高 BER 下重执行风暴)为后续研究排除一条路——比只报正结果更诚实。
4. **错误模型边界声明**: 随机位翻转的通用性论证 + burst/stuck-at/多位局限如实列出。
5. **宽 BER 扫描**: 1e-9→1e-2 七个数量级——因错误率本身跨源跨数量级(辐射 vs 缺陷 vs 降压)。
6. **工具开源即生态**: facebookresearch 组织开源——Meta 借学术合作把内部关注点转化为公共研究基础设施(企业影响学术议程的机制样本)。
7. **RMSE inf/nan 的诚实处理**: 超出浮点范围的输出显式标注而非丢弃。

## 横向对比注记

1. **vs [03] ripple / [01] SOSP23**: Sankar 从"检测坏硬件"(Ripple/FleetScanner)到"量化应用容忍度"(本文)——**Meta SDC 工作从硬件检测向应用鲁棒性的自然延伸**;产业界视角的完整链条:找到坏核 → 淘汰 → 评估没淘汰时应用层还能做什么。
2. **vs R6 四篇(40–43)**: R6 是冗余容错四栈层(检测/纠正);本文是**应用层容忍评估**(不检测不冗余,量化残余质量)——SDC 应对空间的第三条轴;"错误掩蔽"从 [41] 的 43.3% 系统级量化细化为 DRS 的组件级量化(embedding 稀疏索引天然掩蔽)。
3. **53 篇中首个一级企业合作样本**: 截至 43 号全部零企业;本篇 3/5 作者来自 Meta + sabbatical 机制 + facebookresearch 开源——**综述企业生态叙事的关键转折点**:AI 应用层是产业界愿意直接下场合作的层(其负载即产业核心资产)。
4. **vs [43] hpc-duplication**: 工具生态对照——LLFI(LLVM IR,单指令注入,CPU 数据通路)vs PyTEI(模型参数位翻转,AI 内存);[43] 引 BinFI [40](Chen/Li/Pattabiraman)与本文引用同圈——故障注入工具社区的连续性。
5. **vs [30] irps25-rl / [36] shout-trainer**: 都做"SDC×AI"但层次不同——产测优化(硬件)/ 在线检测(系统)/ **应用鲁棒性(模型)**;本文不用 ML 方法,是经典控制变量实证。
6. **ABFT 负结论 vs [42] 脚注**: [42] 检测后靠软件清理(检测≠纠正的分工);本文 ABFT 重执行的失效模式论证互补——高错误率下"检出即重做"策略的崩溃边界。
7. **Google TPU 线 [9][26] vs Meta MTIA [13]**: 两大自研加速器生态在 SDC 叙事中同框——AI 专用硬件的可靠性成为 hyperscaler 共同关切。
8. **Llama 3 [46] 引用**: 用自家模型技术报告佐证"SDC 影响训练"——产业内部证据的学术化使用;训练期鲁棒性明确列为缺口(未来工作)。
9. **Xun Jiao 谱系**: Villanova 的低压近似计算线(Levax [21] 输入感知电压缩放错误模型)→ Meta 休假 → DRS 鲁棒性;与 [43] 的输入感知思想同源(Levax 2020 已做 input-aware error model)。
10. **R7 批次定位**: 本文开启 R7"软件/AI/视野"批——AI 应用层首篇,后续 [45] FHE/[47] dark-side 等将补齐 AI/系统/综述视野。

## 身份核实

- **ISSRE 2025 正式论文**(IEEE,DOI 10.1109/ISSRE66568.2025.00016),会议录 pp.25–36,PDF 13 页(末页空);全文实读。
- **作者 5 人 2 机构**(p.1 左栏署名 + 右下机构标注 Villanova/Meta);sabbatical 脚注 p.1;**一级企业合作证据三重**(作者机构、脚注、facebookresearch 开源),判定可靠度最高级。
- **无致谢/资助节**——如实记录(结论直接接参考文献);不减弱一级证据。
- **文本层问题**: 双栏错位严重——Table II(模型×数据集的参数量/AUC 网格)行列错乱(如 "FM MOVIELENS-1M 171K 0.813" 与 DFM/AFM 行交叠),Fig.6–8 热图数值与轴标签交错,Fig.9–12 图例与子图标题混排——**引用具体数值时以 §VI/§VII/VIII 正文叙述为准,Table II 数值需回 PDF 原表核对**。
- **引用疑点(如实记录)**: 正文称 "Meta, Google, and Alibaba all report instances of SDCs [4],[6],[7]"——但 [4](SOSP23)/[6](Dixit)为 Meta、[7](Hochschild)为 Google,**未见 Alibaba 独立文献**;Alibaba 的 SDC 公开报告应另有其文(如 Alibaba 集群 RAS 论文),此处引用与文字不完全对应,引用时注意。
- **首创声明**: "To the best our knowledge, this paper presents the first empirical study"(原文 typo "To the best our knowledge")——限定 DRS×SDC 实证,合理。
- **诚实标记**: 错误模型局限(§IX-B)、推理期局限与训练期缺口、dummy 模型合理性论证、ABFT 负结果如实报告。
- **内部一致性**: 摘要 30% 恢复 ↔ §VIII 5–30% 区间一致;BER 阈值(1e-6/1e-5/1e-4/1e-3)在 dummy/真实/讨论三处口径一致;无矛盾发现。

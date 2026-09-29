# [31] 变化影响下小延迟故障的鲁棒测试模式生成(ETS 2025 PhD Forum)

## 标题与出处

- **标题**: Robust Pattern Generation for Small Delay Faults under the Impact of Variations
- **会议**: 2025 30th IEEE European Test Symposium(ETS 2025)— **PhD Forum** 论文;DOI: 10.1109/ETS63895.2025.11049640(p.1);版权行 `979-8-3315-9450-3/25/$31.00 ©2025 IEEE`。
- **类型**: 学术论文(PhD Forum 立场/进展短文,2 页正文;"Expected Impact" 章节为将来时态,无实验结果——见身份核实)。

## PDF 与实读范围

- **文件**: `ref/Robust_Pattern_Generation_for_Small_Delay_Faults_under_the_Impact_of_Variations.pdf`
- **页数**: 3 页(第 3 页空白),**全文实读,无截断**。

## 作者全列(p.1)

Hanieh Jafarzadeh(1,2), Sybille Hellebrand(2), Hans-Joachim Wunderlich(1)

## 机构分列

- **学术机构**: University of Stuttgart(斯图加特大学,Wunderlich,1);Paderborn University(帕德博恩大学,Jafarzadeh/Hellebrand,2)——德国 DFT(可测性设计)两大传统强组。
- **企业机构**: 无。

## 企业合作证据(三级)

- **一级(作者机构)**: 无企业作者。
- **二级(致谢/资助)**: 无致谢章节(PhD Forum 短文格式)。
- **三级(版权页)**: IEEE 标准版权,非企业版权页。
- **间接层**: ① 正文 §I 明确援引 **SIEMENS(西门子 EDA)** 近期工作作为"未来硅生命周期管理的关键议题"动机(文本标注 [3],但参考文献 [3] 为 Tehranipoor 2011 教材;与"silicon life-cycle management"主题匹配的是 [4] Rajski 等 IEEE D&T 2023——Rajski 隶属 Mentor Graphics/Siemens EDA,推断文本引用编号笔误,详见身份核实);② [1] Dixit Meta(arXiv 2102.11245)、[2] Bacon Google Spanner(SELSE'22)作为 SDC 动机引用;③ §V 以"From an industrial perspective"段论述测试成本/良率爬坡/量产效率的产业价值,目标场景点名数据中心与汽车电子。

## 核心结论

1. **问题定位(§I)**: 小延迟故障(SDF,额外延迟小于时钟周期)是 **SDC 的一类主要根因**,并以 Google Spanner 与 Meta 数据中心的大规模 SDC 事件为佐证 [1][2];P(工艺)×V(电压)×T(温度)×A(老化)变化使最长路径在芯片个体间漂移——为某个体生成的测试模式在其他个体上失效,单一测试集无法覆盖所有个体。
2. **条件空间爆炸(§I)**: COND = P × V × T × A;即使简化假设(3 工艺角:slow/nominal/fast × 3 电压:最小/标称/最大 × 3 温度:-40°C/25°C/125°C × 3 老化阶段:bring-up/operational/wear-out)也产生 **81 种唯一条件**,每种都需专属测试模式集——timing-aware ATPG 的测试存储与计算时间使其在制造阶段也不可行 [5]。
3. **方法核心(§II, Fig.1)**: 统计学习式鲁棒测试集生成——用精确晶体管模型(含变化)生成 **Monte Carlo 训练集 C**(多电路个体);初始测试集 T 由**非 timing-aware** 的 transition fault ATPG 生成;故障表 F = C 中所有个体上可能可检的 SDF 并集;迭代回路:**timing-aware 故障模拟器( GPU 加速)** 找出 T 未检故障 → 送回非 timing-aware ATPG 生成新模式 → 直到全部故障检出或覆盖预定 SDF 尺寸。关键假设(kNN 式可迁移性):对一个个体有效的检障模式,在时序特性相近的其他个体上同样有效。
4. **变化下的压缩(§II)**: 同一故障在不同个体/条件下于不同尺寸处被检出,既有压缩方法失效——新 timing-aware 压缩策略:找每故障被检出的最小尺寸再进一步压缩。
5. **低电压(§III)**: 缺陷-电压选择性——信号完整性/串扰仅高压可检,阻性桥接/栅氧缺陷/阻性断路主要低压可检;但低压下工艺变化放大、掩盖故障效应;算法只对**变化最大的电压**生成模式,其余电压的模式在压缩阶段导出——跨电压模式重叠优化存储。
6. **最优电压(§IV)**: 温度波动(汽车 -40°C 至 125°C)引起显著时序变化;存在使温度致变化最小化的**最优电压**,在该电压下测试模式更小、因 guardband 减小而故障效率更高。
7. **DLBIST 集成与产业价值(§V)**: 提出多相 DLBIST(power-on/power-off/operational 三相,变温变压),对现有先进架构仅需最小修改;通过识别**包含(subsuming)条件**缩减 COND 空间;支持周期性 in-field 测试;产业视角收益:减少测试模式量、存储需求、对昂贵 timing-aware ATPG 的依赖 → 降低测试成本、加速良率爬坡、提升量产效率;目标应用:数据中心与汽车电子等高性能场景。

## 分类学标注

- **根因机理**: SDF(小延迟故障)作为 SDC 根因的类型学位置;P/V/T/A 四源变化作为"制造测试通过、现场才暴露"的解释机制(与 [09] test-escapes、[28] Vega 老化、[15] DelayAVF 同谱系)。
- **故障模式**: 变化掩盖(fault masking at low voltage)——低压工艺变化放大掩盖故障效应;同故障跨个体检出尺寸漂移。
- **检测技术**: 结构测试/ATPG 路线(门级网表、transition fault 模型、DLBIST)——测试生成谱系中区别于 [25][26][27][28][30] 的**第六路线:传统 DFT/ATPG 在变化下的鲁棒化**(统计学习 + GPU 故障模拟 + 压缩);生命周期定位:制造测试 + 嵌入式测试 + 周期性 in-field DLBIST(全硅生命周期管理,Rajski [4] 主题)。
- **处理技术**: 无。

## 业界观点摘录(纯学术论文,摘产业定位相关)

- "The authors of a recent work by SIEMENS highlighted these challenges in SDF detection as critical issues for future silicon life-cycle management."(§I——西门子 EDA 的产业关切经由学术文本转述)
- "From an industrial perspective, the approach reduces test pattern volume, memory requirements, and dependence on costly timing-aware ATPG. As a result, it lowers test costs, accelerates yield ramp-up, and enhances production efficiency for advanced integrated circuits."(§V)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 81 | 简化 3×3×3×3 条件组合数(P×V×T×A) | §I |
| 3×3×3×3 | 工艺角(slow/nominal/fast)×电压(min/nom/max)×温度(-40/25/125°C)×老化(bring-up/operational/wear-out) | §I |
| -40–125°C | 汽车级温度范围 | §I, §IV |
| 5 | 参考文献总数(PhD Forum 短文) | References |
| 2 | 正文页数 | 全文 |
| 3 相 | DLBIST 阶段:power-on/power-off/operational | §V |

## 方法论要点

- **统计学习替代确定性生成**: Monte Carlo 多个体训练集 + kNN 式"模式可迁移"假设,绕开 timing-aware ATPG 的算力瓶颈;模式生成与故障模拟解耦(非 timing-aware ATPG 生成 + timing-aware GPU 模拟评估)。
- **COND 空间缩减策略**: 只对变化最剧烈的电压点生成 + 压缩期导出其余电压;识别 subsuming 条件——组合爆炸的两级缩减。
- **最小检出尺寸压缩**: 以"每故障最小可检尺寸"为压缩基准,处理跨个体检出尺寸漂移。

## 横向对比注记

- **SDC 根因谱系**: 本文把 SDF 定位为 SDC 主要根因并引 [1] Dixit(Meta)/[2] Bacon(Google Spanner)——延迟类根因链:SDF(本文,门级)→ delay fault AVF([15],微架构级)→ 老化时序违例([28] Vega,物理级)→ test escapes([09],系统级);四层同一现象的不同抽象。
- **[2] Bacon SELSE'22 与本集 [39] exabyte-db 同源**(均为 Google Spanner SDC 检测/防务)——R5 读 [39] 时确认版本关系(SELSE'22 或其期刊扩展)。
- **与 [28] Vega 的关系**: 同为 PVT+A 变化下的测试生成,但 [28] 自顶向下走"老化物理 → 形式化验证生成指令序列"(处理器级),本文走"统计学习 ATPG"(门级网表级);老化(A)在本文 COND 中仅为三档离散参数,[28] 则有 BTI 反应扩散模型——抽象层级差异的典型样本。
- **与 [30] IRPS'25 对照**: 同属"变化/物理机制驱动的测试优化",Intel 用 RL 优化功能测试输入(系统级),本文用统计学习生成结构测试模式(门级)——功能测试 vs 结构测试两条产业并行路线。
- **生命周期主张**: 制造 + 嵌入式 + 周期性 in-field DLBIST 的全周期覆盖,与 [30](制造已部署 + in-field 规划)、[27] Harpocrates(Ripple/Fleetscanner 生命周期复用)构成跨论文的生命周期矩阵。
- Rajski [4]"design for test 与硅生命周期管理的未来"(Siemens EDA)被引为领域方向——DFT 产业界向 lifecycle management 转向的话语证据。

## 身份核实

- **venue/DOI 核实**: 页眉 "2025 30th IEEE European Test Symposium (ETS) -PhD Forum-"、DOI 10.1109/ETS63895.2025.11049640、版权行三者一致,确凿。
- **页数核对**: PDF 3 页(第 3 页空白),全文实读,无截断。
- **引用编号矛盾(已如实记录)**: 正文 §I "a recent work by SIEMENS ... [3]" 与参考文献表不符——[3] 为 Tehranipoor/Peng/Chakrabarty《Test and Diagnosis for Small-Delay Defects》(Springer 2011,教材,非西门子);与正文"recent ... silicon life-cycle management"表述匹配的是 [4] Rajski 等 IEEE Design & Test 2023(Rajski 隶属 Mentor Graphics/Siemens EDA)。判定:文本引用编号笔误,SIEMENS 工作应为 [4];两种读法均已记录,不作单向断言。
- **下载戳**: 每页页脚 "Authorized licensed use limited to: Shaanxi Normal University. Downloaded on September 28, 2026"——为 PDF 获取方(用户方)的 IEEE Xplore 授权戳,**非作者机构、非合作证据**,特此标注以免误读。
- **文体诚实标注**: PhD Forum 短文,§V "Expected Impact" 为将来时,全文无实验数据/结果图表(仅 Fig.1 流程图)——属研究计划/进展报告,非完整研究论文;引用其结论时应按"方法主张"而非"实证结论"对待。
- **文本层损失**: 无公式符号损失(COND 公式中乘号显示为空白,已按上下文复原为 P×V×T×A);Fig.1 为流程图,文字层已含全部节点文本。

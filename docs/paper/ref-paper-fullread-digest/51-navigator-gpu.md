# [51] NAVIgator:AMD NAVI GPU 电压极限探索(IOLTS 2025,雅典大学)

## 标题与出处

- **标题**: NAVIgator: Exploring the Voltage Limits of AMD NAVI GPUs for Energy Efficient Computing
- **出处**: **IOLTS 2025**(IEEE 在线测试与鲁棒系统设计国际研讨会;文本层无页眉/DOI,venue 依文件名 iolts2025_trakosa);8 页内容+空尾页。
- **类型**: **实验表征论文**——AMD NAVI(RDNA3)三型 GPU 降压(undervolting)的首个系统研究;**降压→SDC** 是其核心失效机制之一。

## PDF 与实读范围

- **文件**: `ref/NAVIgator Exploring the Voltage Limits of AMD NAVI GPUs for Energy Efficient Computing iolts2025_trakosa.pdf`
- **页数**: 9 页 PDF(8 页内容+空尾页),**全文实读,无截断**(txt 两段读完,536 行)。

## 作者全列(p.1)

Maria Trakosa、Odysseas Chatzopoulos、George Papadimitriou、Dimitris Gizopoulos——**全部雅典大学**(四人邮箱 @di.uoa.gr 并列)。

## 机构分列(p.1)

- **雅典大学**(University of Athens, Greece)×4——单机构;Gizopoulos 学派电压裕量研究线的 GPU 延伸。

## 企业合作证据(三级)

- **一级(作者机构)**: **无**(纯学术四作者)。
- **二级(致谢/资助)**: **无企业资助**——致谢:"European Union's Horizon Europe... **No 101097224 (REBECCA)** and **No 101093062 (Vitamin-V)**"+ 希腊研究与创新创业基金会(HFRI)项目 **VEMER**(附欧盟免责声明)——**欧盟双项目+希腊国家基金,零产业资助**(与 [49]/[50]/[47] 的 Meta/AMD/OCP 资助形成对照)。
- **三级(版权页)**: 文本层未见版权行/DOI(提取缺失,见身份核实)。
- **间接层**: ① 研究对象为 **AMD 商用 GPU**(RX 7600 XT/7700 XT/7800 XT,RDNA3——市场自购,无 AMD 参与声明);② 引 AMD RDNA 白皮书 [13];③ 引 NVIDIA 降压研究谱系(Leng MICRO'15 [8]、predictive guardbanding TCAD'21 [30]、GPU 电压噪声 HPCA'15 [41]、Tan TC'25 [40]);④ 自引 Athens 电压裕量谱系(MICRO'17 [4]/**含 AMD 共同作者 Lawthers/Das**、IOLTS'17 [5]、ISPASS'18 micro-viruses [36]、DATE'18 [38]、MICRO'23 电压-软错误 [39])。**判定:无直接企业合作——AMD 硬件为被测对象而非合作方;值得注意其同组前作(MICRO'17 电压裕量)曾有 AMD 共同作者,本文无**。

## 核心结论

1. **定位(摘要+§I)**: 降压(固定频率、电压低于标称)可显著提升能效;既有研究集中于 NVIDIA GPU 与 CPU,**AMD GPU 基本空白**——本文为三款现代 AMD NAVI GPU 的首个全面降压研究;功耗动机:高性能 GPU 常 >350 W(CPU 通常 <150 W),保守 PVT 裕量导致过量功耗。
2. **实验设置(§II-A/B)**: 三型号各 2 块(共 6 卡,2023Q3–2024Q1 发布):RX 7600 XT(2860 MHz/Vnom 1160 mV/32 CU/64 AI 加速器/16GB/6nm)、7700 XT(2700/1140/54/108/12GB/5nm)、7800 XT(2700/1140/60/120/16GB/5nm);频率锁定近 boost;20 个负载——**8 个 HPC 内核**(Rodinia 系:cfd/kmeans/lud/nn/nw/srad/streamcluster+leukocyte)+ **12 个 PyTorch 分类网络**(LeNet5/ResNet50/VGG16/AlexNet/MobileNet/EfficientNet/ViT/SqueezeNet/Swin/Dense/ShuffleNet/GoogleNet;数据集 MNIST/CIFAR100/ImageNet;每次执行 1000–10000 次推理;单负载运行时 0.61–6.78 s)。
3. **方法学(§II-C/D)**: Linux 5.15+ROCm 6.2.4,AMDGPU 驱动 sysfs 电压偏移;气候受控环境;自研框架:金标准运行→逐级降压(Rodinia 步长 10 mV、ML 25 mV)直至崩溃;**五类结局分类学:Success/GPU 驱动崩溃/应用崩溃/应用超时/SDC(负载完成但输出偏离且无系统日志错误)**;SDC 严重度用 Top-1/Top-5 准确率量化;**三电压区:Crash(崩溃)/Unsafe(崩溃+超时+SDC)/Safe(正确执行)**。
4. **电压裕量结果(§III-A)**: **Vmin 距标称 4%–25%**;Rodinia 普遍更低(突发型短运行——个别负载可达驱动允许的 **-300 mV 满偏移而零 SDC**),ML 持续负载更敏感;7700 系裕量最大,其次 7600、7800;**同型号卡间差异=工艺涨落**(7800A 的 Vmin 显著低于 7800B)——chip-to-chip 变异的直接证据;与 NVIDIA 先例一致(Leng 等 [8] 实测 NVIDIA 多代 ~20% 电压裕量)。
5. **不安全区结构(§III-A)**: 跨度 **10–150 mV**;部分负载渐变失效(先零星 SDC→SDC+应用崩溃),部分二值行为(一直成功→骤然崩溃);ML 负载考虑其固有容错性,**Unsafe 区可被利用换取进一步节能**(以精度损失为代价)。
6. **ML 精度影响(§III-B)**: Unsafe 区内 **Top-1 准确率下降 0%–55%、Top-5 最高 57%**;轻量模型(LeNet5/MobileNetV3)受影响小;**例外:Transformer(ViT/Swin)计算最密集却更耐受**——两个架构特质:softmax 归一化天然容忍小数值误差;多头注意力+深层并行提供冗余、掩蔽局部计算错误;**CNN(尤其早期层)传播微小误差故更敏感**;7700/7800(Unsafe 区更宽)精度退化大于 7600。
7. **节能效果(§III-C)**: Vmin 下**节能 0.24%–33%**(负载与型号依赖;与 NVIDIA 研究 ~25% 相当);数据中心外推:平均 14%×300 W=42 W/卡,万卡机房 **420 kW、年省 ~3.68 GWh**——降压作为 SDC 风险换能效的工程权衡被量化。
8. **相关工作(§IV)**: CPU 裕量线(ECC 反馈引导 Itanium 电压推测 [31]、IBM POWER7+ 自适应裕量调度 [2]、Athens 组静态离线裕量分析 [32]–[38]);GPU 降压线(Leng 安全限 [8]/预测性裕量 [30]/电压噪声 [41]/Tan 低电压指令级错误 [40]——皆 NVIDIA);[39] 降压对软错误率影响(MICRO'23)——**AMD GPU 降压研究空白由本文填补**。
9. **结论(§V)**: 首个 AMD NAVI 降压全面研究——节能与热收益显著且不必然牺牲正确性,但**收益高度依赖负载特性与 chip-to-chip 工艺涨落**。

## 分类学标注

- **SDC 核心特征**: SDC 定义=完成但输出偏离且无日志(应用级定义);**SDC 出现在崩溃之前的电压区间**(Unsafe 区先于 Crash 区)——SDC 是降压失效的**前哨症状**。
- **根因机理分类**: 降压→临界路径时序余量耗尽/错误捕获——电压裕量型根因;**chip-to-chip 工艺涨落**(同型号两卡 Vmin 差异)——[47] 根因三分法中"个体差异"的直接实证。
- **故障模式**: 渐变型(先 SDC 后崩溃)vs 二值型(骤崩)两种失效模式;负载依赖激活(突发 vs 持续、计算密度、访存模式)。
- **检测技术**: 金标准比对(与 SDCShield 同族);五类结局分类;Top-1/Top-5 准确率作为 ML 域 SDC 严重度度量。
- **处理技术**: 三电压区划分+Vmin 运行=**受控降级运行**的能效版;Unsafe 区利用(ML 容错性)=[44] 负载容忍思路的电压域应用。
- **生命周期**: 出厂裕量(保守 guardband)→现场特性化(Vmin 标定)→运行(降压节能)——裕量全寿命管理的能效视角。
- **两界关系**: 纯学术研究测商用 AMD 硬件——**无厂商参与的独立表征**;与 [50](AMD 自述小延迟故障)构成"厂商自述 vs 独立测量"的互补对。

## 业界观点摘录

1. "existing studies predominantly target NVIDIA GPUs and CPUs, frequently employing predictive voltage models, **leaving AMD GPUs relatively unexplored**."(摘要——AMD 空白论)
2. "high-performance GPUs often exceed 350 W, significantly surpassing CPUs, which typically operate under 150 W"(§I——GPU 功耗动机)
3. "some of these workloads can reach the full -300 mV offset allowed, without even experiencing a single SDC"(§III-A——短突发负载的裕量深度)
4. "For example we can clearly see that the 7800A GPU has signficantly lower Vmin than its B counterpart."(§III-A——同型号卡间涨落实证;原文拼写 signficantly)
5. "the unsafe region could be exploited for further power savings"(§III-A——Unsafe 区的工程利用)
6. "Despite being among the most computationally intensive workloads, they [ViT/Swin] exhibit greater resilience to undervolting."(§III-B——Transformer 耐受悖论)
7. "undervolting can yield significant energy and thermal benefits without compromising correctness, but the extent of these benefits is highly dependent on both the workload characteristics and the chip-to-chip process variation."(§V——结论双依赖)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 3 × 2 = 6 | GPU 型号数 × 每型卡数 | Table I |
| 1160/1140/1140 mV | 三型号标称电压 | Table I |
| 32/54/60 | 三型号 CU 数 | Table I |
| 64/108/120 | 三型号 AI 加速器数 | Table I |
| 8 + 12 | HPC 内核数 + ML 网络数 | §II-B |
| 1000–10000 | ML 负载每次执行推理数 | §II-B |
| 0.61–6.78 s | 单负载运行时(7600XT) | Table II |
| 10 / 25 mV | Rodinia / ML 降压步长 | §II-C |
| 4%–25% | Vmin 距标称的降幅 | §III-A |
| -300 mV | 驱动允许的最大偏移(个别负载零 SDC 达满偏) | §III-A |
| 10–150 mV | Unsafe 区跨度 | §III-A |
| 0%–55% / 57% | Top-1 / Top-5 准确率最大下降 | §III-B |
| 0.24%–33% | Vmin 下节能幅度 | §III-C |
| 14% / 42 W | 平均节能率 / 单卡省电(×300 W) | §III-C |
| 420 kW / 3.68 GWh | 万卡机房省电功率 / 年省电量 | §III-C |
| 20% | NVIDIA 电压裕量(Leng 等实测,对照) | §III-A [8] |
| 101097224 / 101093062 | EU REBECCA / Vitamin-V 资助号 | 致谢 |

## 方法论要点

1. **金标准+分级扫描**: golden run→步进降压→五类结局判定——与 SDCShield/OPENDCDiag 同族的确定性测试设计(DPRNG 未用,但金标准比对与三区划分同构)。
2. **双板采样设计**: 每型号 2 卡专测 chip-to-chip 涨落——把"工艺变异"从统计噪声提升为研究对象。
3. **SDC 严重度量化**: Top-1/Top-5 准确率把 SDC 从"发生/未发生"细化为"多严重"——ML 域 SDC 度量学(与 [44] 的 BER 阈值法、Dr.DNA 的激活分布法并列)。
4. **小倍图(small multiples)呈现**: Fig.2–6 六卡并列同轴对比——涨落可视化方法。
5. **能效外推算术**: 42 W×10,000 卡×24 h×365 d 的数据中心级外推——学术结果向产业规模的翻译范式。

## 横向对比注记

1. **与 [48] A7 Phoebe**: Trakosa 双现——[51](IOLTS'25)为纯雅典班底,Phoebe(Micro'26)加入 Meta 的 Dixit/Sankar;**Trakosa 2025 年隶属雅典大学**([48] 笔记相应修正)——雅典组学生进入 Meta 合作圈的路径样本。
2. **与 [50] AMD 节对读**: [50] AMD 自述小延迟故障的"电压/温度/负载组合激活"与本文 Unsafe 区渐变失效互证;**[50]=厂商视角(逃逸论),[51]=独立学术视角(商用卡黑盒测量)**——同一物理现象的两界叙述;[50] 的 Arm 节"电压裕量调整"处理手段与本文 Vmin 运行同族。
3. **与 [44] recommendation(架构决定论)**: [44] RecSys 中 MLP 脆弱/embedding 鲁棒(稀疏索引掩蔽),[51] 图像域 Transformer 鲁棒/CNN 敏感(softmax+多头冗余 vs 早期层传播)——**跨域一致的规律:结构性冗余/归一化掩蔽故障,传播性架构放大故障**——综述"负载/架构-SDC 容忍度"专节的双证据。
4. **与 [49] 电压-SDC 机理**: [49] §I"降压降低临界电荷、提高 SDC 易感性"的论断在本文得到 AMD GPU 实证;[49] 的束流-降压 [39](MICRO'23,同组)是同一机理的软错误版本。
5. **与 R1 批次披露论文**: 本文 SDC 定义("完成但输出偏离且无日志")与 [01]/[03] 机群 SDC 定义同族;但研究对象从数据中心 CPU 转向消费级 GPU——**SDC 研究对象的谱系扩展(CPU→GPU)**,呼应 [48]"数据并行架构更易 SDC"结论的实证前奏。
6. **与 [47] 成本框架**: [47]"SDC 缓解成本落在哪里?性能/价格/能耗——全选";本文给出**能耗维度的反向外推**(主动接受边际电压的 SDC 风险换取 3.68 GWh/年节能)——风险-收益工程化的具体案例。
7. **与 Athens 组电压裕量谱系**: [4] MICRO'17(含 AMD 的 Lawthers/Das 共同作者)→[5] IOLTS'17→[36] micro-viruses→[38] DATE'18→[39] MICRO'23→本文 AMD GPU——**该组 8 年裕量研究线**;注意资助演变:早期 MICRO'17 有 AMD 共同作者,本文零产业资助,而同期 SDC 线([49]/[50])拿 Meta/AMD/OCP——**同一实验室双线双资助结构**。
8. **与 [24] arm-soft-error**: [39](降压-软错误,MICRO'23)亦为 [50] 的 [18]——束流+降压的交叉方法学在语料内三次出现([24] 束流、[39] 降压束流、[51] 降压)。
9. **渐变 vs 二值失效模式**: 与 [50] 小延迟故障"参数敏感性+条件激活"一致——渐变型=参数敏感缺陷,二值型=裕量整体耗尽;综述故障模式章节可按此二分。
10. **NVIDIA 先例的补全**: Leng 线([8][30][41])+Tan [40] 构成 NVIDIA 降压-SDC 文献闭环,本文补 AMD——**GPU 降压研究的双厂商版图完成**;综述检测/表征章节的 GPU 部分由此齐备。

## 身份核实

- **IOLTS 2025**: 文本层**无页眉/版权行/DOI**(提取缺失,同 [50] 情形);venue 依文件名 iolts2025_trakosa;9 页 PDF(末页空),全文实读;无其他 digest 引文可交叉(该文为 2025 新作)。
- **四作者单一机构**(雅典大学):邮箱 @di.uoa.gr ×4 逐字确认;无产业作者。
- **致谢(p.6)**: EU Horizon Europe REBECCA(101097224)+Vitamin-V(101093062)+HFRI VEMER,附欧盟免责声明——**零企业资助确认**。
- **文本层问题**: Fig.2–6 为六面板小倍图,轴标签严重重叠乱码(多图挤压),数值一律取自正文叙述;Table II 的 Category 列存在错位——leukocyte 被排在 PyTorch 侧(配 MNIST 数据集),但 Fig.3(Rodinia)轴含 leukocyte 且 ML 图轴为 12 个纯网络名,**按 8(HPC,含 leukocyte)+12(ML)归组,leukocyte 的归类以图为权威**;原文 "signficantly" 拼写错误照录;参考文献 **[4]/[33]、[5]/[37] 为同文重复著录**(与 [50] 的 [4]/[6] 重复同款编辑部疏漏)。
- **引文链核对**: [8]=Leng MICRO'15、[30]=Leng TCAD'21、[40]=Tan TC'25、[39]=Agiakatsikas MICRO'23(=[50] 的 [18])、[4]/[33]=MICRO'17(含 AMD 共同作者 Lawthers/Das)、[38]=DATE'18(泛希腊联合体+AMD Das)——与 Athens 组谱系及 [50] 引文全部对上。
- **内部一致性**: Vmin 4–25% 与 NVIDIA 20% 裕量量级相容;节能 0.24–33% 与 NVIDIA ≤25% 相容;Unsafe 区(10–150 mV)⊂ Vmin(4–25%≈46–285 mV)自洽。
- **AMD 参与情况**: 无任何 AMD 人员/资助/数据支持的声明;三型号卡为商用零售品——**独立第三方表征**,与 [50] 的厂商自述形成方法论对照。

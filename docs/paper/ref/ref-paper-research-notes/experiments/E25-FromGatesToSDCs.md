# From Gates to SDCs: Understanding Fault Propagation Through the Compute Stack(DATE 2025,雅典大学 + Meta)

- 作者:Odysseas Chatzopoulos、George Papadimitriou、Dimitris Gizopoulos(University of Athens);Harish D. Dixit、Sriram Sankar(Meta Platforms Inc)
- Venue:2025 Design, Automation & Test in Europe Conference (DATE 2025),EDAA,ISBN 978-3-9826741-0-0
- 原文 PDF:docs/paper/ref/From_Gates_to_SDCs_Understanding_Fault_Propagation_Through_the_Compute_Stack.pdf
- 复现档案编号:E25(本文件)
- 注:本文为 Veritas(HPCA'25,同目录另有 PDF)的姊妹/基础工作;是同组从 SRAM 结构(TC'23)转向**算术单元门级故障**的第一篇系统研究。

## 1. 研究问题与核心贡献(≤5 行)

填补"CPU 整数/浮点算术单元(通常无任何保护)的门级硅缺陷如何传播到程序输出"的空白:混合门级+微架构级注入——用 ArithsGen 自动生成加法器/乘法器 C++ 门级模型嵌入 gem5(吞吐损失 <4%),注入 stuck-at 永久故障,17 个负载端到端跑完(>10 万次 full-system 模拟)。量化:门级故障传到 FU 输出的概率、FU 输出 BER、终态(Crash/SDC/Masked)分布、BER 与终态相关性、错误位数影响、以及**按指令类型的错误率**(控制流/栈指令从不产生 SDC)。

## 2. 实验方法

### 2.1 混合注入框架(第 III-A 节,5 步)

1. 用 ArithsGen [30](DDECS'22 开源电路生成器)自动生成功能单元的 C++ 门级模型;
2. 对门级模型插桩,支持任意门上注入(本文:stuck-at-0 / stuck-at-1 永久故障;架构上可扩展其他故障类型);
3. 把门级模型**内联**进 gem5(智能混合方法,避免外挂门级模拟器——旧方法 [19] 外挂导致吞吐损失 >2×,本方法 <4%);
4. gem5 内自建故障注入控制器;
5. 高度优化的并行注入 campaign 管理器,吃满宿主机全部核。

FU 实现细节(脚注 1):加法器 = 64 位超前进位(carry-lookahead,4-bit CLA 块);乘法器 = 高速 64 位 Dadda 树乘法器。

### 2.2 采样与统计(第 III-A 节末)

每单元 SFI 采样 **500 个门**,每门两个故障场景(stuck-at-0 + stuck-at-1)= **每算术单元每基准 1,000 次注入**。

### 2.3 量化指标(研究问题 Q1–Q6)

- 故障传到 FU 输出概率(≥1 次错误结果即算激活);
- **BER**(Bit-Error-Rate)= FU 输出的错误位数 / 该 FU 产生的总位数;
- 终态:Crash(程序或内核崩溃)/ SDC(跑完但输出损坏)/ Masked——并细分 **Masked Internal**(FU 输出从未出错,门级逻辑掩蔽)与 **Masked External**(FU 输出错过程序层掩盖);
- **指令错误率**(脚注 2)= 使用了错误数据的该类指令数 / 该类指令全部执行数。

## 3. 实验配置(模拟器/硬件、基准、参数)

| 项 | 配置 | 出处 |
|---|---|---|
| 模拟器 | gem5(x86-64 OoO 模型) | III-A |
| 门级单元 | 5 个标量整数加法器、1 个标量整数乘法器(Dadda);向量 FP/Int FU 存在于配置但注入结果只报告标量 int 单元 | Table I、脚注 1 |
| 微架构(Table I) | L1 I/D 各 32KB 8-way;L2 512KB 8-way;PRF 192 Int + 160 FP;LQ/SQ/IQ/ROB = 44/48/148/256;标量 Int FU 5 Add + 1 Mul;标量 FP FU 2 Add + 2 Mul;向量 Int FU 4 Add + 2 Mul;向量 FP FU 2 Add + 2 Mul | Table I |
| 故障模型 | 永久 stuck-at(0/1),门级 | III-A |
| 注入量 | 每单元 500 门 × 2 = 1,000 次/基准;6 单元 × 17 基准 × 1,000 = 102,000 次 full-system 模拟("more than 100,000") | III-A / IV-C |
| 基准(17 个) | MiBench 14 个 + 3 个来自 OpenDCDiag(SDCShield 上游!)的常用 Linux 库基准 | III-B |
| OS | full-system,带 Linux,程序跑到完成 | III / IV-C |
| 吞吐 | gem5 ~1M 模拟指令/秒 | IV-C 开头 |

## 4. 实验步骤(可操作流程,编号)

1. ArithsGen 生成 64-bit CLA 加法器与 Dadda 乘法器的 C++ 门级网表;
2. 对网表每个门插桩 stuck-at 钩子;
3. 将门级模型替换 gem5 O3CPU 的整数 ALU/MUL 执行路径(保持时序模型不变);
4. 配置 Table I 的 x86-64 OoO 微架构 + Linux full-system;
5. 编译 17 基准(MiBench 14 + OpenDCDiag 3);
6. 每基准先跑黄金执行(记录 FU 每次输出的全部位 + 程序输出);
7. 对每个 FU(5 加法器 + 1 乘法器):采样 500 门 × {SA0, SA1} 逐一注入,跑完整程序;
8. 每次注入记录:(a) FU 输出是否出错/错误位数/BER;(b) 终态 Crash/SDC/Masked;(c) 每类指令使用错误数据的次数(指令错误率);
9. 聚合产出六图:Fig.1(故障传到输出 %)、Fig.2(BER)、Fig.3(终态)、Fig.4(BER×终态分布)、Fig.5(错误位数×终态)、Fig.6(指令错误率);
10. 对照分析:加法器 vs 乘法器、sha 类 vs 其他负载、控制流/栈指令 vs 数据指令。

## 5. 实验数据(关键数值 + 图表号)

**门级故障传到 FU 输出的概率(Fig. 1,17 负载 × 5 加法器 + 1 乘法器):**
- 加法器:负载激活 **90%–98%** 的门级故障(即内部门级逻辑掩蔽仅 2–10%);
- 乘法器:**65%–98%**(内掩蔽 2–35%,负载差异大);
- 五个加法器之间差异中等(指令调度决定哪些操作进哪个单元);
- **两个 sha 实现的掩蔽最少**(Sha 算法用随机数据重度使用整数加法器,每次计算结果都重要)——sha 类负载是门级故障"最少掩蔽"的负载。

**FU 输出 BER(Fig. 2):**
- 加法器:各负载 BER 相当均匀,**0.035–0.055**(最高利用率的单元,输入流不断);
- 乘法器:只有 untoast、jpegc 两负载接近 **0.06**,其余 ~**0.01**(乘法器利用率低、输入多样性差)。

**终态分布(Fig. 3,>100,000 次 full-system 模拟):**
- 五个加法器:**主导终态是 Crash,>80%** 的注入故障导致崩溃(加法器处理指针、数组/循环下标、栈地址——损坏即灾难);
- 加法器 SDC:**0%–18%**;两个 sha 实现 SDC 最多(hash 特性:每个位都重要);
- 加法器 Masked 较低,且外部掩蔽(微架构+软件层)> 内部掩蔽;
- 乘法器:Crash 30%–65%;SDC 在 bitcount、fft_inv、fft、untoast、jpegc 上 **5%–20%**;Masked 远大于加法器(内部掩蔽归因于更深的树结构;外部掩蔽归因于软件常丢弃 64×64 乘法的高 64 位)。

**BER 与终态的相关性(Fig. 4):**
- 加法器:Crash 的 BER 分布宽(均值 **0.057**、标准差 0.15、最大 0.87);SDC 与 Masked 的 BER 极低且窄(均值 **2.2×10⁻⁴** 与 4.4×10⁻⁴;最大 0.03 与 0.063)——**加法器要产生 SDC,其 BER 必须极低**(偶发单次小错误才可能静默存活);
- 乘法器:三个终态的 BER 分布都散(相关度低):Crash 与 Masked 均值 BER ~0.12,SDC 均值 0.05、最大 0.41。

**错误位数与终态(Fig. 5,横轴 FU 输出错误位数,y 轴 log):**
- 加法器:Crash 遍布所有错误位数;SDC 随错误位数增加**递减**,在 25–52 位区间有回弹;
- 乘法器:SDC 在 17–64 位区间反而增多(模式更均匀)。

**指令错误率(Fig. 6,log 轴,SDC 与 Crash 两个子图):**
- Crash 结局的指令错误率平均更高;
- 若干指令在 SDC 子图**缺失**(如 leave、sysret)——这些指令几乎必然 Crash;
- **控制流与栈管理指令(ret、call、push、pop 等)在 SDC 结局中错误率极低**:其值损坏几乎总是触发 segfault 甚至内核崩溃,从不(或几乎不)产生 SDC——这是"哪类指令从不产生 SDC"的直接答案;
- SDC 结局整体指令错误率平均更低。

## 6. 实验结论(编号列出,对应论文 Observation #1–#6)

1. **Obs #1**:门级 stuck-at 在加法器上比乘法器更易传到输出;掩蔽强依赖负载与调度(两者决定 FU 输入值)。
2. **Obs #2**:加法器 BER 跨负载均匀(0.035–0.055),乘法器差异大(~0.01 vs 0.06)——加法器是最重载单元,输入流恒定。
3. **Obs #3**:整数加法器主导终态是 Crash(>80%);乘法器掩蔽更高、SDC 平均更多(SDC 5–20% vs 加法器 0–18%)。
4. **Obs #4**:加法器上 SDC/Masked 要求 BER 极低(均值 2.2×10⁻⁴/4.4×10⁻⁴);乘法器上三个终态在 BER 全域都可能出现(相关弱)。
5. **Obs #5**:加法器 SDC 随错误位数增加而显著减少;乘法器分布更均匀。
6. **Obs #6**:指令错误率 Crash 结局平均更高;**控制流与栈管理指令从不导致 SDC**(错误值触发 segfault/内核崩溃)——leave、sysret 等指令在 SDC 子图完全缺席。
7. (综合)sha 两个实现在"最少掩蔽 + 最多 SDC"两个维度同时领先——hash 类负载是算术单元缺陷的最敏感探测器;这一结论与 OpenDCDiag/SDCShield 用 SHA 类测试做 SDC 探测的选择互相印证(本文基准本身就包含 3 个 OpenDCDiag 库基准)。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

- **ArithsGen 开源**(Klhufek & Mrazek,DDECS'22):生成 CLA/Dadda 及多种加法器/乘法器 C++ 模型——直接可用;
- **gem5 混合集成思路**:把门级模型包成 gem5 的 FuncUnit 执行回调(替换 `execute()` 的整数运算结果计算),时序模型不动——ARM64 侧完全可行(gem5 DerivO3CPU);
- **x86-64 → ARM64 迁移**:核心结论(加法器高 Crash、乘法器高 Masked、sha 最敏感、控制流指令零 SDC)是负载与数据性质决定,ISA 无关;ARM64 上复现时把指令集换成 A64 即可,Fig.6 的指令名映射(call→bl、ret→ret、push/pop→stp/ldp 等);
- **BER / Masked Internal / Masked External / 指令错误率的指标定义**:全部可照抄;
- **采样方案**:500 门 × SA0/SA1 × 单元数——注意前提是门级网表规模已知;
- **MiBench + OpenDCDiag 基准组合**:SDCShield 本身就是 OpenDCDiag 分支,后 3 个基准直接从自家仓库取。

### 需替代/不可行

- **Veritas 工具链**(本文背后的注入框架)不开源;门级插桩需自写(ArithsGen 输出的网表结构规整,对每个门的输出信号加一个 `if (fault_id == this_gate) value = stuck_value;` 钩子即可,工作量为每单元一次性);
- **向量 FU 注入**:论文 Table I 配置里有向量 FU 但结果图只报告标量整数单元;ARM64 上做 NEON/SVE 向量单元的门级等效注入是**超出原文本市的增量工作**——恰是 SDCShield 新实验的机会点(SEVI ASPLOS'26 已证明真实机队向量指令 SDC);
- **stuck-at 之外的故障模型**(桥接、开路、时序)论文明说"可扩展但未做";
- **浮点单元**:配置存在,数据未报——复现 FP/SIMD 结论需自行补充;
- 17 负载 × 102,000 次注入的总量:最小版可裁到 1 加法器 + 1 乘法器 × 6 负载 × 1,000 = 12,000 次。

**最小复现路径(建议,ARM64 版)**:ArithsGen 生成 64-bit CLA 加法器 + Dadda 乘法器 C++ 模型 → 嵌入 gem5 DerivO3CPU(ARMv8)整数执行路径 → MiBench 6 负载(bitcount、fft、jpeg-c、sha ×2、qsort)+ SDCShield 自带 openssl_sha → 每单元 1,000 stuck-at 注入 → 复核三个锚点数值:加法器 Crash >80%、加法器 BER 0.035–0.055、sha 系 SDC 最高(0–18% 区间上端)。总 2 单元 × 7 负载 × 1,000 = 14,000 次 full-system 模拟。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

这篇对 SDCShield 新实验设计的价值最直接(算术单元 + OpenDCDiag 基准 + 指令级分辨率):

| 对比轴 | 论文基线值(可引用) | SDCShield 侧对照实验 |
|---|---|---|
| sha 类负载掩蔽最少 | 两 sha 实现:故障传到输出 ~上限(加法器 90–98% 带内最高)、SDC 0–18% 上端(Fig.1、Fig.3) | 论证 openssl_sha/ipsec 在 SDCShield 测试集中的核心地位;"hash 类=最敏感探测器"的文献依据 |
| 加法器 vs 乘法器激发差异 | 加法器 Crash>80%、BER 0.035–0.055;乘法器 Crash 30–65%、Masked 高、SDC 5–20%(Fig.2、3) | 新实验应区分 add 密集链(FMA 的加法分量)与 mul 密集链设计,预期 SDC 概率谱不同 |
| 控制流/栈指令零 SDC | ret/call/push/pop/leave/sysret 等从不产生 SDC(Fig.6) | 论证 SDCShield 测试内核应把算术密度最大化、把控制流最小化(每条分支都是把潜在 SDC 转成 crash 的掩蔽器/放大器)——为"无分支数据内核"设计提供定量理由 |
| SDC 要求极低 BER | 加法器 SDC 的 BER 均值 2.2×10⁻⁴、最大 0.03(Fig.4) | 真机上等效于:缺陷单元多数情况下正确、极少数输入出错——激发实验需大指令数 × 多样输入才命中(指导测试时长/输入空间设计) |
| 乘法高 64 位丢弃=外部掩蔽 | 64×64 乘法高 64 位常被软件丢弃 → Masked External 主因之一 | 激发内核应让乘法结果全宽参与后续计算(避免高位截断掩蔽)——具体可操作的设计规则 |
| 门级内掩蔽空间 | 加法器内掩蔽 2–10%、乘法器 2–35% | 引用论证"测试模式必须遍历足够多的输入组合"(乘法器输入多样性低时 BER 仅 0.01) |

## 9. 局限与坑

1. **只测标量整数单元**:Table I 中的向量 Int/FP FU 与标量 FP FU 无注入数据——对 SDCShield 的 SIMD/SVE 主攻面,本文只提供方向性参考,向量单元结论需引 SEVI(ASPLOS'26)与 Veritas(HPCA'25);
2. **x86-64 平台**:指令名(call/ret/leave/sysret)与 SDCShield 的 ARM64 不同,零 SDC 指令清单需做 ISA 映射验证;
3. **只测永久 stuck-at**:瞬时/间歇故障未测(论文理由:机队 SDC 主要是持久性缺陷 [1][2][17][18]——成立,但引用时注意这是假设不是结论);
4. **1 个乘法器 vs 5 个加法器的不对称采样**:乘法器数据只有 1 个单元,加法器有 5 个,统计强度不对等;
5. **500 门/单元是采样不是全门**:对 Dadda 乘法器这种大门数单元,500 门采样是否覆盖关键路径全部门未论证(原文未给总门数);
6. **无多比特/多单元联合故障**:两单元同时缺陷(真实机队常见)未建模;
7. **"3 个 OpenDCDiag 基准"未点名是哪三个**(III-B 只说来自 OpenDCDiag 的 Linux 库),复现时需自行挑选并注明;
8. Fig.1/Fig.3 上下子图 y 轴范围不同(加法器 vs 乘法器),跨子图目视比较会误读;Fig.5 y 轴 log、Fig.6 y 轴 log,引用数值一律以正文文字为准;
9. BER 与 SDC 的"低 BER 才 SDC"结论(Fig.4)是相关性不是因果——复现验证时应注意这是分布重叠的统计现象。

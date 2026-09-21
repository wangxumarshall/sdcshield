# DelayAVF: Calculating Architectural Vulnerability Factors for Delay Faults(MICRO 2024,MIT + AMD)

> 复现档案 E28。所有数值均摘自 PDF 原文并标注图表号/节号;读不到的信息明确写"原文未给出"。
> 论文:Peter W. Deutsch (MIT), Vilas Sridharan (AMD), Vincent Quentin Ulitzsch (MIT/TU Berlin), Joel S. Emer (MIT), Sudhanva Gurumurthi (AMD), Mengjia Yan (MIT)。2024 57th IEEE/ACM International Symposium on Microarchitecture (MICRO 2024),DOI 10.1109/MICRO61859.2024.00026。正文 12 页 + 附录(artifact 说明)+ 参考文献,共 15 页。

## 1. 研究问题与核心贡献(≤5 行)

针对**小延迟故障(Small Delay Faults, SDF)** —— 即边际缺陷(marginal defects)使电路传播延迟增加亚周期(sub-cycle)时长,被认为是近年超大规模数据中心 SDC 潮的主要根因之一——提出 AVF 的时序感知扩展 **DelayAVF**。核心贡献 4 项(§1 Key Contributions):(1) 度量微架构结构对 SDF 脆弱性的 DelayAVF 指标;(2) 两步法可计算框架(timing-aware 求 dynamically reachable set + timing-agnostic 求 GroupACE);(3) 开源 Ibex RISC-V 核案例研究;(4) 复用既有粒子打击 AVF 数据的 OrDelayAVF 近似。对我方价值:**这是"边际电压/时序缺陷如何在功能测试中显现"的形式化理论基础**——SDF 不改变逻辑功能,只改变时序,是否出错取决于路径裕量、信号翻转(toggle)与掩蔽,与 SDCShield 通过高压/负载激发瞬态时序失效的思路同源。

## 2. 实验方法

### 2.1 问题建模:为什么粒子打击 AVF 不足以研究 SDF(§III)

三条本质差异(§III-A,以 Figure 2 除零陷阱电路为例):

1. **toggle 依赖**:SDF 只有当信号发生翻转(0→1 或 1→0)时才可能有影响;粒子打击无论信号是否翻转都可能翻转寄存器位。例:trap enable 从未使能时,寄存器 A 永远锁存 0,对 SDF 免疫,但粒子打击模型下 A 仍可能被视为 ACE。
2. **延迟时长依赖**:d 太小 → 无状态元件错误;d 较大 → 可能只有部分寄存器锁错值。这是时序维度的行为,粒子打击模型完全没有。
3. **多状态元件同时出错且集合不可先验确定**:一个 SDF 可同时打错多个寄存器;失败寄存器组取决于电路时序与输入,**不能像空间多位 MBU(相邻位组可先验确定)那样按物理位置预先枚举**(§VIII 对比 Wilkening et al.)。

程序可见失效的必要条件链(Figure 3):SDF → (A) 至少一个状态元件错误 → (B) 该组错误传播为程序可见失效。其中 (A) 又分解为两个子条件:路径静态超周期(statically reachable)+ 状态元件实际锁错值(dynamically reachable,受逻辑掩蔽影响,Figure 2c)。

### 2.2 形式化定义(§IV、§V.A)

- **SDF 模型**(§IV):故障加在**导线**(wire,两个电路元件之间的连接)上;导线延迟取数据无关固定值(可通过商业 EDA/STA 工具扩展到数据相关);d = 额外延迟时长,**d < 1 个时钟周期**;单缺陷假设(标准可靠性方法学);**SDF 只影响单个周期**(边际条件恰好满足一个周期的概率占主导);信号不翻转则 d 无效果。d 的选择:设计师不知 d 时可扫描 0%–100% 时钟周期;或用软缺陷定位(SDL,光学分析 + 激光刺激)从实际缺陷芯片反推电阻→延迟映射(§IV.C)。
- **Definition 1(DelayACE)**:导线 e 在周期 i 是 DelayACE ⟺ 在 e 上于周期 i 加时长 d 的 SDF 导致程序可见失效(§V.A)。
- **Definition 2(Statically Reachable Set)**:状态元件终止了一条因 SDF 而传播延迟**静态超过时钟周期**的路径(只看电路结构,与逻辑/架构掩蔽无关,§V.B)。
- **Definition 3(Dynamically Reachable Set)**:静态可达 **且** 实际锁错值的状态元件集合 S = DynamicReachable_d(e, i)。静态可达 ≠ 动态可达(逻辑掩蔽)。
- **Definition 4(GroupACE)**:状态元件集合 S 在周期 i GroupACE ⟺ S 中**所有**状态元件同时出错导致程序可见失效。必须整体考虑,因为存在两种混杂效应:
  - **ACE compounding(复合)**:S 中没有成员单独 ACE,但组合起来 GroupACE(如 SEC ECC 下双位错不可纠正→GroupACE,但每位单独都不是 ACE)。
  - **ACE interference(干扰)**:各成员单独都 ACE,但错误互相抵消→组不 GroupACE。
- **DelayAVF 公式**(Equation 3):

  DelayAVF_d(E) = [Σ_{i=1..N} Σ_{e∈E} DelayACE_d(e,i)] / (N·|E|)

  即结构 H 内导线集合 E 在全部 N 周期中 DelayACE 占比的平均值。结构失效率 ≈ DelayAVF × 该结构发生 SDF 的速率(与 AVF 用法一致)。

### 2.3 两步计算法(§V.B,Figure 4)

DelayACE_d(e,i) = GroupACE(DynamicReachable_d(e,i), i+1)(Equation 4)。精确方法、非启发式:

- **Step #1(时序感知,只 1 个周期)**:确定 SDF 打错的状态元件集合(动态可达集)。只需对周期 i 做 timing-aware 仿真(可用 SPICE 级,本文用门级时序库)。
- **Step #2(时序无关,N 个周期)**:把状态元件错误注入 RTL/微架构仿真,跑完整个程序看输出是否改变 → 判断 GroupACE。
- **复杂度**:对 |E| 条导线 × N 周期:O(|E|·N) 周期的 timing-aware 仿真 + O(|E|·N²) 周期的 timing-agnostic 仿真(N 个独立仿真,各长 |E|·N)。高度可并行。

**优化**(§V.C,全部保持保真或注明取舍):周期采样(temporal sampling);按子结构/宏分块(复杂度线性于结构内导线数);timing-aware 仿真只需仿真喂给静态可达集的子电路;喂入信号不翻转则 Step #1 整体跳过(动态可达集平凡为空);同(电路,输入,延迟)三元组缓存结果。timing-agnostic 侧可用单状态元件 ACE 数据近似 GroupACE(精度换速度,§VII 评估)。

### 2.4 近似方法 OrDelayAVF(§VII)

- **Definition 5(ORACE)**:集合 S ORACE ⟺ 任一成员 s∈S 单独 ACE。
- **Definition 6**:用 ORACE 替代 GroupACE 算出的 DelayAVF 近似记为 OrDelayAVF。适用于已有粒子打击注入/ACE 数据的设计,零额外 timing-agnostic 仿真。
- 精度取决于 ACE 干扰/复合率(见 §5 Table III 数据)。

## 3. 实验配置(硬件/模拟器、基准、参数)

| 项目 | 配置(原文出处) |
|---|---|
| 目标设计 | **Ibex**:开源 32 位流水线 in-order RISC-V 核,多次流片(OpenTitan root of trust 采用)(§VI.A) |
| 综合工具 | **Yosys**(开源)(Figure 5) |
| 工艺库 | **NanGate 45nm** 开源工艺库(FreePDK45/Nangate Open Cell Library)(§VI.A) |
| 时序感知仿真 | 基于 RTL + 工艺库门延迟的自研 SDF 注入框架,含逻辑掩蔽分析(Figure 5) |
| 时序无关仿真 | **Verilator**(benchmark 黄金输出比对:任何偏差 → GroupACE)(§VI.A) |
| 结构(5 个) | 寄存器堆(带可选**单纠错 ECC、无双错检测**)、ALU、译码器、load-store queue(LSQ)、prefetcher(§VI.A) |
| 基准(5 个) | Beebs 套件:md5、bubblesort(libbubblesort)、libstrstr、matmult、libfibcall(§VI.A) |
| d 扫描范围 | **10%–90% 时钟周期**,共 9 档(§VI.B,Figures 7–10) |
| 采样率 | 每条导线 × **4% 的执行周期** 注入(注入点在程序全程均匀分布)(§VI.A) |
| 时钟周期设定 | 等于全设计最长路径长度(§VI.A "Ibex Path Distributions") |
| 延迟建模 | 导线延迟 = 驱动元件强度 + 下游电容负载;不含互连电容(符合 pre-layout STA 流程);数据无关(§VI.B Modeling Delays) |
| sAVF 对照 | 对 regfile 等有状态结构另外做粒子打击位翻转注入,同 4% 采样(config: `percent sampled cycles particle`)(§VII、附录) |
| Artifact | Docker 封装;48+ 核服务器推荐;10 GB 磁盘;准备 30 分钟;全实验约 24 小时;MIT 许可;公开:https://github.com/viniul/micro-artifact,Zenodo DOI 10.5281/zenodo.13743439(附录 A/B) |

结构导线数(Table I):

| 结构 | 注入导线数 \|E\| |
|---|---|
| ALU | 3668 |
| Decoder | 1007 |
| Regfile | 17816 |
| Regfile (ECC) | 19611 |
| LSU | 2027 |
| Prefetch | 3249 |

基准周期数(Table II):md5=1720,libbubblesort=3829,libstrstr=1051,libfibcall=2448,matmult=8903。

## 4. 实验步骤(可操作流程,编号)

以 artifact 附录(§E)与 §VI.A 流程为准:

1. **取 Ibex RTL**(开源),选定要分析的微架构结构 H(如 ALU)及其导线集合 E。
2. **Yosys 综合 + NanGate 45nm 库** → 门级网表 + 每条导线的传播延迟(数据无关、pre-layout)。
3. **静态时序分析**得到结构路径长度分布(Figure 6);把设计的时钟周期设为全设计最长路径。
4. **编译 Beebs 基准**为 Ibex 可执行 hex(artifact 内含基准源码与 hex payload 配置项)。
5. **Verilator 仿真跑通无故障执行**,记录每个基准的黄金输出与周期数 N(Table II)。
6. **选择 d 档位**(10%…90% 时钟周期)与**采样率**(4% 周期),写入配置 json(配置字段:synth file / submodule name / pdk path / top path / clk path / hex payload / delay range / percent sampled cycles delay / percent sampled cycles particle / ecc on / output dir)。
7. **Step #1 时序感知注入**(对每个采样的 (e, i) 对):在导线 e 的当前周期仿真中加延迟 d,门级时序仿真判断下游哪些状态元件静态可达且锁错值 → 动态可达集 S。若 e 的上游信号本周期未翻转,直接判 S=∅(跳过)。
8. **Step #2 时序无关注入**:把 S 中所有状态元件同时置错,Verilator 从周期 i+1 继续跑到程序结束,输出 ≠ 黄金输出 → GroupACE=1。
9. **聚合**:对结构内全部导线 × 采样周期求 DelayACE 占比 → DelayAVF_d(H);对 5 个基准取几何平均(geomean)。
10. **(可选)sAVF 对照**:同结构做单粒子位翻转注入(只对有状态结构),得到 sAVF。
11. **(可选)ECC 实验**:开启 regfile 单纠错 ECC(ecc on),重跑 7–10。
12. **(可选)OrDelayAVF**:用单元件 ACEness 替代 GroupACE 重算,并统计 ACE 干扰/复合率。
13. 复现命令(附录 E):`cd tests/ibex/testbench/ && ./run_all.sh configs/beeps/md5_alu.dict`,再 `python3 ../../../util_scripts/plot_beeps.py` 出图。

## 5. 实验数据(关键数值 + 图表号)

### 5.1 结构间对比(Figure 7:geomean 归一化 DelayAVF,d=10…90%)

归一化(每结构除以自身最大值)后的读数(图内数据标签):

| d(% 周期) | Decoder | Regfile | ALU |
|---|---|---|---|
| 10 | 0 | 0 | 0.0035 |
| 20 | 0.0032 | 0 | 0.0353 |
| 30 | 0.0113 | 0 | 0.0454 |
| 40 | 0.088 | 0.0207 | 0.1393 |
| 50 | 0.1236 | 0.0321 | 0.3002 |
| 60 | 0.2 | 0.0462 | 0.5155 |
| 70 | 0.302 | 0.1121 | 0.7755 |
| 80 | 0.7793 | 0.1542 | 1 |
| 90 | 0.789 | 0.1835 | 0.9956 |

- **Observation 1(§VI.B):ALU 的 DelayAVF 高达寄存器堆的 5× 以上**("upwards of 5×")。各 d 下排序基本恒定:ALU > Decoder > Regfile。
- 三个结构在 d≤30% 时 DelayAVF 都接近 0;ALU 在 d≈80% 达到峰值归一化 1。

### 5.2 成分分解(Figure 8:Static Reach / Dynamic Reach / GroupACE 三级漏斗)

- **libstrstr + ALU(Figure 8a)**:d=50% 时静态可达导线 85%,但 GroupACE 远小;25/50/75 三档 d 下 Static≈100%、Dynamic 与 GroupACE 显著衰减。
- **libstrstr + Regfile(Figure 8b)**:d=50% 时静态可达 **100%** 的导线(高于 ALU),但动态可达比例远低于 ALU → **寄存器堆低翻转率**(word-line 例:每周期只有激活行 0→1 与去激活行 1→0 可能出错,其余 0→0 word-line 延迟完全无害,Figure 11 讨论)导致低 DelayAVF。
- **md5 + ALU(Figure 8c)**:md5 哈希高度随机 → ALU 翻转率高 → 动态可达与 DelayAVF 显著高于 libstrstr。
- **Observation 2(§VI.B)**:小 d 时 SDF 脆弱性由**静态电路时序特征主导**(路径不超周期就绝无错误);大 d 时**程序与架构级效应**(掩蔽/ACE)更突出。路径分布(Figure 6)本身不含掩蔽强度信息 → 单看 STA 不够。对时序更紧、关键路径更多的强优化核,程序/架构效应会更早(d 更小)登场。

### 5.3 多位错误率(§VI.B 正文)

- 对所有基准与结构平均:**凡引起至少一个状态元件错误的 SDF,约一半(50%)产生多位状态元件错误**。
- 多位错误占比最低点在 d=10%:平均 **21%**;其余 d 档在 50% 附近波动、无明确趋势。
- DelayAVF 不必随 d 单调增大:更大 d 有时得到更小的动态可达集(毛刺效应可能让状态元件恰好锁对值)。

### 5.4 基准间差异(Figure 9:ALU 各基准归一化 DelayAVF,d=10…90%)

图内数据(行=基准,列=d 档):

| 基准 | 10% | 20% | 30% | 40% | 50% | 60% | 70% | 80% | 90% |
|---|---|---|---|---|---|---|---|---|---|
| md5 | 0 | 0.0065 | 0.0252 | 0.0901 | 0.2008 | 0.2972 | 0.3833 | 0.3996 | 0.4069 |
| libbubblesort | 0 | 0 | 0.0053 | 0.0234 | 0.0677 | 0.1771 | 0.2877 | 0.3819 | 0.4119 |
| libstrstr | 0 | 0 | 0 | 0.01 | 0.0543 | 0.159 | 0.3071 | 0.4463 | 0.4484 |
| libfibcall | 0 | 0.0213 | 0.0538 | 0.1227 | 0.3464 | 0.7362 | 1 | 0.5747 | 0.5825 |
| matmult | 0 | 0.0053 | 0.0194 | 0.0596 | 0.107 | 0.14 | 0.2474 | 0.3851 | 0.411 |

- **Observation 3**:同一结构 DelayAV随基准变化巨大;md5(随机哈希)vs libstrstr(规则字符串比较)的对比是核心案例。libfibcall 在 d=70% 出现归一化峰值 1(超过其他所有基准)。
- 注意归一化方式:Figure 9 是"normalized to facilitate comparison"(§VI.B),图内峰值 1 不等于绝对 DelayAVF=1。

### 5.5 有状态结构 + ECC 盲区(Figure 10:regfile/LSQ/prefetch/regfile-ecc,geomean)

图内数据(未归一化标签,Regfile/LSQ/Prefetch/Regfile-ecc 四组 × d 档):

| d(%) | Regfile | LSQ | Prefetch | Regfile-ecc |
|---|---|---|---|---|
| 10 | 0 | 0 | 0 | 0 |
| 20 | 0 | 0 | 0.009 | 0.0011 |
| 30 | 0.0314 | 0 | 0.0178 | 0.0574 |
| 40 | 0.0891 | 0 | 0.044 | 0.0891(图中标 0.0891 一档) |
| 50 | 0.1283 | 0.0264 | 0.0591 | 0.1283(对应档) |
| 60 | 0.3113 | 0.0881 | 0.1314 | 0.3113(对应档) |
| 70 | 0.4282 | 0.1991 | 0.5098 | 0.4282(对应档) |
| 80 | 0.1337(峰值后回落段) | 0.2826 | 0.5659 | — |
| 90 | 1(sAVF 标签处) | — | — | — |

(读图注:Figure 10 的坐标为"Normalized AVF Values / DelayAVF",四组柱在 d=30–80 间爬升,Regfile-ecc 在 d=80 的归一化 DelayAVF 标签为 0.1337、其 sAVF 为 0;Prefetch 峰值 0.5659。上图数值为图内数据标签逐档转录,个别档位归属以原文图为准;趋势性结论见下方 Observation。)

- **Observation 4**:DelayAVF 与 sAVF 的**结构排序不同**。Prefetcher 对两类故障都脆弱(内部 buffer 是状态元件);而 **regfile 加单纠错 ECC 后 sAVF 降为 0,DelayAVF 却没有对应下降**。
- **Observation 5(§VI.C,ECC 盲区机制,Figure 11)**:SDF 打在 word-line 驱动线 x 上,周期 i-1 读地址 00、周期 i 读地址 01 → 01 行激活延迟 → 感放重新锁存**上一个地址 00 的旧数据**。若 00 与 01 数据不同,产生状态元件错误,但** ECC 校验仍然通过**(锁到的是"另一个合法地址的有效数据")→ 不检出、不纠正 → 非零 DelayAVF。这是"功能测试中 ECC 结构的时序缺陷盲区"的直接案例。

### 5.6 OrDelayAVF 近似误差(Table III,d=90% 时钟周期,聚合全部基准)

| 结构 | Max ACE 干扰% | Avg ACE 干扰% | Max ACE 复合% | Avg ACE 复合% | Max 相对误差% | Avg 相对误差% |
|---|---|---|---|---|---|---|
| ALU | 0.98 | 0.58 | 0.17 | 0.09 | 3.00 | 1.73 |
| Decoder | 13.03 | 6.73 | 2.47 | 1.14 | 21.80 | 10.45 |
| Regfile | 0.13 | 0.07 | 0.17 | 0.07 | 0.69 | 0.30 |
| Regfile (ECC) | 0.13 | 0.07 | **21.95** | **11.57** | **92.45** | **50.38** |

- **Observation 6**:ORACE 近似平均可用,但两类结构失真:(a) Decoder ACE 干扰最高(Max 13.03%)——多位错让 PC 回退多条指令可能不出错,单位错回退一条指令反而失败;(b) **ECC regfile ACE 复合最高(Max 21.95%、平均误差 50.38%、最大 92.45%)**——SEC ECC 下多位错 GroupACE 但每位非单独 ACE,OrDelayAVF 系统性低估。

## 6. 实验结论(编号列出)

1. 粒子打击 AVF 不能迁移用于 SDF 脆弱性评估:toggle 依赖、d 时长依赖、多位错误组不可先验确定三者是粒子模型没有的维度(§III)。
2. 结构间 DelayAVF 差异巨大(ALU ≈ regfile 的 5×+),脆弱性由"静态时序(小 d)+ 程序/架构掩蔽(大 d)"共同决定,STA 路径分布单独不足(Observations 1–2)。
3. DelayAVF 强基准依赖:高随机性负载(md5)比规则负载(libstrstr)在 ALU 上脆弱得多;翻转率是核心中间变量(Observation 3)。
4. **对粒子打击有效的防护对 SDF 未必有效:ECC regfile sAVF=0 但 DelayAVF>0**——word-line 延迟导致读到"别的合法行"的数据,ECC 全盲(Observations 4–5,Figure 11)。
5. 约一半的致错 SDF 产生多位状态元件错误;多位错误是 ECC 防护与 ORACE 近似失效的共同根源(§VI.B、Table III)。
6. OrDelayAVF 平均误差小(ALU/regfile <2%),但 ECC 结构上最大误差 92.45% —— 混杂效应强的结构必须用精确 GroupACE(Observation 6)。
7. DelayAVF 可用于指导针对性防护与生成对 SDF 可观测性更强的功能测试(可与 Harpocrates 类硬件感知测试生成框架结合,§VIII 末段)——**即"用微架构脆弱性分析指导功能测试程序设计"的明确接口,正是 SDCShield 新实验设计的理论立足点**。

## 7. 复现要点(ARM64 复现最小版本)

### 可直接复用

- **概念与判定协议**:SDF → 状态元件错误 → 程序可见输出的三级漏斗、Static/Dynamic/GroupACE 分解、多位错误统计、toggle 依赖假设——这些是纯方法论,直接映射到"SDCShield 时序激发实验的命中判定与归因分层"(激发 ≠ 命中;命中需经历翻转→锁存→传播)。
- **d 扫描协议**:0–100% 周期扫描、小 d 无效区(本文 d≤30% 几乎全 0)与饱和区(70–90%)的形状,可用于设计电压/频率边缘扫描(undervolting 等效于给所有路径加均匀 d)的档位与预期曲线。
- **基准差异结论**:高翻转/高随机负载(md5)对时序缺陷更敏感 → 支持在 SDCShield 中优先采用随机数据 FMA/AES/哈希类负载作为时序激发候选,而非规则内存负载。
- **ECC 盲区案例**:word-line 延迟 → 读到相邻行合法数据 → 校验通过。在 ARM64 上可转化为测试设计原则:**任何"读回校验"型测试对时序缺陷的检出能力取决于读地址序列的翻转密度,连续读不同行才是激发条件**(SRAM/缓存行切换频率是变量)。
- **artifact 本身**:https://github.com/viniul/micro-artifact(Docker,Yosys+Verilator+NanGate45,MIT 许可)——可在 x86 服务器上原样跑通 Ibex 的全部数值,作为方法论校验基准;也可把 Ibex 换成任何开源核重跑。

### 需替代/不可行

- **RISC-V Ibex → ARM64 核**:ARM64 无公开 RTL(ARM 只放出极少 A-class RTL),不可能对 Kunpeng 920 做导线级 SDF 注入。替代路径:(a) 用 QEMU/gem5 ARM64 模型做"状态元件错误→程序可见"的 Step #2 半程复现(注入架构可见寄存器/微架构缓冲错误,观察传播);(b) 用 SDCShield 真机实验从"结果端"反推:通过电压/频率边缘扫描制造等效 SDF,统计不同负载的检出率差异,与本文"基准依赖"结论对齐。
- **门级时序注入 → 真机电压/频率调节**:真机无法定点给某条线加 d;只能通过降压/降频/升温等全局边缘化手段近似"给所有路径加 d 分布",d 的等效值不可观测(只能间接标定,如错误率-电压曲线拐点)。
- **单周期瞬时 SDF 模型 vs 真机持续性**:本文假设 SDF 只持续 1 个周期;真机电压边缘下的时序违例是持续性的(每周期都可能错),错误率模型需从"每周期独立"改为"持续暴露",不能直接搬 DelayAVF 数值当预测率。
- **OrDelayAVF 近似**:依赖设计的单元件 ACE 数据,对闭源 ARM 核不可得。
- **NanGate45/kunpeng 7nm 不可比**:绝对 DelayAVF 数值强烈依赖工艺库与设计,只可复用趋势与排序结论,数值仅作定性对照。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | 论文基线数值(出处) | SDCShield 可测形态 |
|---|---|---|
| 结构排序:ALU vs Regfile | ALU DelayAVF ≈ 5× regfile(Obs.1/图 7;d=80%:ALU 1.0 vs regfile 0.1835 归一化) | 算术/逻辑重负载 vs 寄存器/访存重负载的 SDC 检出率对比(预期算术型显著更高) |
| 负载随机性轴 | md5 ≫ libstrstr(图 9;d=70%:0.3833 vs 0.3071;d=90%:0.4069 vs 0.4484,趋势 md5 早期更高;核心论证在图 8 动态可达差距) | 同一计算核上随机数据 vs 规则数据的 FMA/哈希测试错误率对比 |
| d(等效时序压力)扫描形状 | d≤30% 全 0 → 30–80% 陡升 → 饱和(图 7) | 降压/降频扫描的错误率-电压曲线:宽安全区 + 陡峭过渡带 + 饱和 |
| 多位错误占比 | ~50%(d=10% 时 21%)(§VI.B) | bitflip mask 统计中多位/多字段错误占比(与 E05 SOSP23 的 bitflip pattern 分析对接) |
| ECC 盲区 | regfile(ECC) sAVF=0 但 DelayAVF>0(Obs.5,图 10、图 11) | 带 ECC 的缓存路径上"读到合法但错误行数据"类错误的构造性测试 |
| 防护有效性排序反转 | sAVF 排序 ≠ DelayAVF 排序(Obs.4) | "对粒子故障有效的检测手段对时序缺陷无效"的方法论论证素材 |

## 9. 局限与坑

1. **绝对数值不可迁移**:NanGate45 pre-layout 时序、无互连电容、数据无关延迟——论文自己承认(§VI.B Modeling Delays);数值只在 Ibex+45nm+该时钟周期设定下成立。
2. **单周期 SDF + 单缺陷假设**:真实边际缺陷可能持续多周期/多缺陷,错误率会不同;论文未做敏感性分析。
3. **时钟周期=最长路径**:这使该设计零时序裕量,现实设计有 setup margin,d 的等效含义会平移(现实芯片 d 需先吃掉裕量才见效)。
4. **采样率 4%**:统计采样,稀有事件(低概率长尾错误)可能漏;论文未报告置信区间/误差棒(Figure 7/9/10 无误差棒)。
5. **规模小**:Ibex 是嵌入式 2 级流水小核,无乱序、无推测、无向量单元;结论外推到 Kunpeng 920 级乱序超流水核(多发射、深流水、复杂转发)需谨慎,尤其是多位错误与掩蔽行为。
6. **Gate-level 模型不含实际版图寄生**(作者承认 place+route 后时序会变),门延迟主导的假设只对逻辑密集结构成立。
7. **Figure 7/9/10 数值转录风险**:PDF 文本层提取的数据标签排列密集,上表个别档位数值需以图为准核对;趋势与关键点(5×、sAVF=0 vs DelayAVF>0、~50% 多位)在正文均有文字确认,可靠。
8. **复现坑**:Verilator/ gem5 版本漂移、Yosys 综合结果与论文快照不一致(开源工具链非确定性)会导致数值偏移;artifact 用 Docker 固定环境,务必用其容器跑。
9. **对 SDCShield 的最大启发是机制而非数值**:ECC 盲区(word-line 延迟读到合法错行)+ toggle 依赖(地址/数据序列翻转密度决定激发效率)是可直接转化为测试设计原则的两条机制;DelayAVF 的"结构×负载×d"三维扫描框架可作为新实验的评估矩阵模板。

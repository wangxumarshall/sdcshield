# Silent Data Corruptions: Microarchitectural Perspectives(IEEE TC 2023,雅典大学)

- 作者:George Papadimitriou、Dimitris Gizopoulos(National and Kapodistrian University of Athens)
- Venue:IEEE Transactions on Computers, Vol. 72, No. 11, Nov. 2023, pp. 3072–3085,DOI: 10.1109/TC.2023.3285094
- 原文 PDF:docs/paper/ref/Silent_Data_Corruptions_Microarchitectural_Perspectives.pdf
- 复现档案编号:E23(本文件)

## 1. 研究问题与核心贡献(≤5 行)

首次系统回答"哪些微架构结构的硬件故障最可能一路传播到软件层并生成 SDC":对 11 个片上结构做 22 万次微架构级瞬态故障注入(gem5/GeFIN,Armv8 Cortex-A72 类 OoO 核 + Armv7 Cortex-A15 对照),量化 (i) 各结构的 SDC 概率、(ii) 最易产生 SDC 的指令相关参数(五分类)、(iii) 内核指令对 SDC 的贡献、(iv) 8 字节字内各字节位置的 SDC 概率。方法论核心是 HVF/AVF 两阶段评估 + 提交点(commit stage)观察法。

## 2. 实验方法

### 2.1 masking 形式化框架(第 II-B 节,Fig. 1)

故障传播路径:微架构结构 → (硬件屏蔽?)→ ISA/软件层 → (软件屏蔽?)→ 程序输出。

**硬件屏蔽(hardware masking)三情形**(Benign 的充要条件,原文第 II-B 节):
1. 故障命中"无效"表项(未映射的物理寄存器、永不被用的预取缓存行等);
2. 故障所在表项在被读之前被正常操作覆盖;
3. 故障影响错误推测(mis-speculated)指令,随流水线冲刷被丢弃。

形式化结论(可作为设计公理引用):
- 故障架构可见 ⟺ 未被硬件屏蔽。给定负载,不同微架构只能改变 Benign(硬件屏蔽)数量,不改变其余部分——例如分支预测更差的 M1 比 M2 产生更多 Benign 故障(原文 II-B 给出的例子)。
- 架构可见故障影响输出 ⟺ 未被软件/逻辑屏蔽。软件屏蔽例子:AND 指令 x0 = AND x1, #0,若 x1 低 32 位损坏,结果仍正确;死寄存器(dead value)同样被屏蔽。
- **第三类"不可见 SDC"**(本篇与 ISCA'21 首次强调):命中缓存中已修改(modified)、属于程序输出、但不再被程序读取的 cache line——数据经 DMA 直接写出,不再经过程序踪迹,无任何进一步屏蔽机会;该类故障在 HVF 分析中初判为 Benign,却必然 SDC。任何以"是否进入程序踪迹"为检测依据的软/硬件防护均无法覆盖。

### 2.2 提交点观察与五分类(第 II-C 节,Fig. 2)

观察点 = OoO 核推测指令的提交点。每条提交指令记录 5 个参数:(i) 提交周期、(ii) PC、(iii) Opcode、(iv) 寄存器操作数/立即数字段、(v) 寄存器内容。与无故障执行逐条比对,首个不匹配参数决定类别(互斥、唯一):
1. **Execution Time Error**:指令各字段均正确,但提交周期错误;
2. **Instruction Flow Change**:PC 损坏导致执行了另一条指令(取指流改变);
3. **Instruction Replacement**:Opcode 损坏导致执行了另一条指令;
4. **Operand Forced Switch**:寄存器操作数或立即数字段损坏;
5. **Data Corruption**:资源正确但内容(寄存器/内存字)损坏。

优先级规则:若 opcode 错导致目的寄存器内容连带错,归为 Instruction Replacement 而非 Data Corruption(主因为准)。

### 2.3 两阶段评估流程(第 III-D 节)

- 第一阶段 HVF(Hardware Vulnerability Factor,Sridharan & Kaeli ISCA'10 定义):故障到"触及软件层"为止;未达提交点 = Benign,达提交点 = Corruption(归入五分类之一)。
- 第二阶段 AVF(Architectural Vulnerability Factor,Mukherjee MICRO'03 定义):Corruption 继续跑完整个程序,终态三分类:
  - **Masked**:与无故障执行无偏差;
  - **SDC**:正常跑完但程序输出与无故障运行不同,且无任何可观察指示;
  - **Crash**:未达负载终点或超时(进程崩溃/内核 panic/死锁/活锁);本文聚焦 SDC,Crash 只在 Fig. 4 补充给出。

### 2.4 软件容错四点局限(第 II-A 节,论述框架,无数值)

(1) 冗余的性能/功耗代价过高;(2) 代码膨胀改变执行模式,反增崩溃易感性(引 ISCA'21 [16]);(3) 只护应用不护全栈(FT-Linux 复制额外慢 40% 且仅限特定 POSIX 程序);(4) 即使全栈软件加固,仍有可观份额硬件故障(尤其"不可见 SDC"类)软硬件两层都检测不到。

## 3. 实验配置(模拟器/硬件、基准、参数)

| 项 | 配置 | 出处 |
|---|---|---|
| 模拟器 | gem5(微架构级,full-system 带操作系统,确定性端到端执行) | III-A |
| 注入框架 | GeFIN(建在 gem5 上的微架构级注入框架;按 [24] 均匀分布随机注入同一表项不同位、不同表项) | III-A |
| 主 ISA/微架构 | Armv8,OoO,类 Arm Cortex-A72 | III-B |
| 对照 ISA/微架构 | Armv7,类 Cortex-A15(验证观察跨 ISA 成立) | III-B |
| 目标结构(11 个) | L1 D-cache tag 与 data 字段、L1 I-cache tag 与 data 字段、L2 cache(data 字段)、物理寄存器文件 RF、Load Queue LQ、Store Queue SQ、ROB、指令 TLB、数据 TLB | III-B |
| 不注入的结构 | prefetcher、分支预测器、BTB(其故障不可能损坏架构状态);FP 物理寄存器堆(基准无 FP 操作) | III-B |
| 基准 | MiBench 10 个负载,最大输入集;执行时长 100M–1.4B 周期(端到端完整执行,非 SimPoint) | III-B |
| 注入量 | 每结构 2,000 个单比特故障 → 11 结构 × 10 基准 = 220,000 次故障注入(每基准 22,000 次) | III-B |
| 统计口径 | Leveugle DATE'09 [24] 统计故障采样:误差 2.88%,置信度 99% | III-B |
| 故障模型 | 瞬态单比特翻转(论证覆盖宇宙线/α 粒子/低压运行/工艺偏差/制造缺陷/逃逸设计 bug 六类物理机制) | I/II |

表 I(硬件参数)在 PDF 中为表格式但正文未逐项展开数值(仅有结构名列表);Cortex-A72 类配置的具体参数"原文未给出"(需参照同组 ISCA'21 Table II:A57/A72 L1 48/48 与 48/32KB、L2 1MB/2MB、RF 128/192、ROB 128 等,见 E24 档案)。

## 4. 实验步骤(可操作流程,编号)

1. 用 gem5 full-system 模式启动类 Cortex-A72 的 Armv8 OoO 核 + Linux,选定 MiBench 负载与最大输入集;
2. 跑一遍无故障执行,记录全部提交指令的五参数轨迹 + 程序输出文件(黄金参考);
3. 按均匀分布 [24] 采样生成 2,000 × 11 结构的故障列表(结构、表项、位、注入时刻);
4. 逐个注入单比特翻转,继续模拟;监视提交点,与黄金轨迹逐条比对:
   - 若故障从未到达提交点(无效表项/被覆盖/推测冲刷)→ Benign(HVF 阶段结束);
   - 若首个不匹配参数出现 → 记录五分类类别;同时检查输出文件是否被污染("不可见 SDC"类:输出缓存在 cache 中被损坏且不再被读 → 直接计 SDC);
5. Corruption 类继续跑完整个程序(最长至 1.4B 周期),对比输出文件与黄金输出:
   - 输出不同且无异常 → SDC;无偏差 → Masked;中途崩溃/死锁/超时 → Crash;
6. 对 10 个基准重复,统计每结构的 SDC 概率(Fig. 3)、Masked+Crash 份额(Fig. 4)、五分类 × 结构的 SDC 概率矩阵(Fig. 6);
7. 事后分析 A:按提交指令的 user/kernel 归属切分 SDC(Fig. 8–10);
8. 事后分析 B:对四个最大结构(L1I data、L1D data、L2、RF)按注入位所在字节 B0–B7 切分 SDC 概率(Fig. 11);
9. 换 Armv7 Cortex-A15 类配置重复关键实验,验证趋势跨 ISA 成立(Fig. 3 叠加)。

## 5. 实验数据(关键数值 + 图表号)

**各结构非 Benign 故障的 SDC 概率(Fig. 3,Armv8;Armv7 趋势相同):**

| 结构 | SDC 概率(占非 Benign 故障) |
|---|---|
| L1 D-cache data 字段 | **53.4%**(最高) |
| L1 D-cache tag 字段 | 38.0% |
| L2 cache(data) | 36.9%(Armv7 上更高,因 L2 减半) |
| D-TLB | 22.2% |
| 物理寄存器文件 RF | 15.8% |
| L1 I-cache data 字段 | 7.3% |
| L1 I-cache tag 字段 / I-TLB | 0.2%(近零) |
| **ROB、LQ、SQ** | **0%**(零 SDC 概率) |

ROB/LQ/SQ 零 SDC 的机理:这些结构的表项含 PC 与物理寄存器 specifier,任何损坏在提交前触发依赖图检查失败 → 必然可见崩溃(例:RF specifier 从 r9 损坏为空闲 r13 → 依赖失败 → crash),而非 SDC(IV-A 节)。

**"不可见 SDC"(不经过程序踪迹直接损坏输出)概率(Fig. 5):**
- 与程序输出大小强相关:bitcount(输出 < 1KB)概率为 **0**;blowfish(输出 > 3MB)概率显著升高(图上为各结构分列柱,原文未给出单点数值,定性结论:输出越大、任一时刻驻留 cache 的输出数据越多,命中概率越高);
- 该类故障 HVF 初判为 Benign,必须把 HVF 结果与输出文件关联才能揭露(IV-A 节)。

**五分类 × 结构的 SDC 概率(Fig. 6,注意各子图 y 轴刻度不同):**
- Data Corruption 组全结构最高(直觉验证);
- **关键反直觉数据**:Instruction Flow Change 组中 L1I tag 字段 ~10% SDC;Execution Time Error 组中 L1D tag 13.1%、D-TLB 11.8% SDC,而 L1I data/L1D data/RF 仅 0.5%/0.2%/0.6%——即"错误周期提交"这类时间性损坏在地址类结构上贡献了不可忽略的 SDC,软件冗余(假设故障=数据损坏)检测不到;
- ROB/LQ/SQ 五类全部为 0(与 Fig. 3 一致)。

**内核指令影响(第 V 节,Fig. 7–10):**
- 内核指令占比:最高 rijndael 10.1%,edge 6.3%、patricia 5.8%,bitcount 最低 0.2%;总体 ≤10%(Fig. 7);
- 但内核对 SDC 的**相对贡献**远超占比:L1I tag 上内核指令占该结构 SDC 相关指令的 **~77%**;L1D tag >50%;L1D data 与 L2 上 >30%(Fig. 10);
- 绝对切分(Fig. 9 顶部标注):L1D data 18.4%/53.4%、L1D tag 19.8%/38.0%、L2 11.2%/36.9%、D-TLB 2.3%/22.2%、RF 0.9%/15.8%、L1I data 0.8%/7.3%、I-TLB 0%、L1I tag 0.14%/0.2%。

**字节位置分析(Fig. 11,单比特翻转下 8 字节字的 B0=LSB … B7=MSB;L1I 为 4 字节指令字):**
- L1I data:各字节概率近乎相同;
- L1D data:越靠 MSB 概率越低(单调下降);
- L2:整体同 L1D 趋势,但 B6、B2 例外偏高(~15%);
- RF:极不均衡——B2、B3 最高,从 B4 向 MSB 递减。

**多比特故障(IV-C 节,定性推论)**:相邻多位翻转不能同时损坏一条指令的两个参数(几何约束),故多比特下 SDC 概率只会比单比特更高(与 [2][12][19] 一致)。

## 6. 实验结论(编号列出)

1. 数据通路结构(L1D data/tag、L2)是 SDC 主源;ROB/LQ/SQ 零 SDC(依赖图检查保证);地址/指令类结构(I-TLB、L1I tag)近零 SDC、高 Crash。
2. SDC 不只来自 Data Corruption:Instruction Flow Change(L1I tag ~10%)与 Execution Time Error(L1D tag 13.1%、D-TLB 11.8%)贡献可观——基于"冗余计算+比对"的软件容错天然漏检这两类。
3. 存在"不可见 SDC":损坏驻 cache 的输出数据后经 DMA 写出,初判 Benign 却必然 SDC;概率随输出体积增大而升高(bitcount=0 → blowfish 显著)。任何以程序踪迹为观察面的检测方案(含软件冗余)原理上不可覆盖。
4. 内核指令占比 ≤10% 却在某些结构贡献过半 SDC(L1I tag ~77%、L1D tag >50%)——只加固用户代码的方案保护面严重不足。
5. SDC 概率随字节位置变化且结构相关(L1D/L2 低位字节更危险,RF B2/B3 峰值)——检测/纠错位布局应按位置差异化。
6. 对产业界:需增强错误检测/纠正能力、引入冗余容错、实现错误记录与监控、并把"模拟数据损坏与硬件故障的场景"纳入测试与验证流程(结论节第 (iv) 条,直接支持 SDCShield 类工具的立项逻辑)。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

- **两阶段 HVF+AVF 评估框架**:gem5 modern(≥20)有 FaultInjection 支持(`FaultManager`/`PersistFault`),对 SRAM 类结构(L1D/L1I/L2/RF/TLB)可注入瞬态位翻转;submit-point 比对可用 gem5 的 commit trace(TraceCmd `CommitTrace`)或自定义 ExecAll trace 后处理实现五分类;
- **黄金运行 + 输出 diff 三分类**(Masked/SDC/Crash):与 SDCShield 的 memcmp_or_fail 黄金值哲学完全同构,可直接写一个 gem5 外围 Python campaign 脚本;
- **MiBench + 最大输入集端到端执行**:MiBench 开源,gem5 Arm full-system 镜像(full_system_images)现成;
- **统计口径**:Leveugle [24] 的 2,000 样本 ≈ 2.88% 误差/99% 置信——若只求趋势,每结构 1,000 注入也可接受(同组 ITC'23 用 1,000/4%/99%,见 E27);
- **user/kernel 切分**:gem5 full-system 下按 CPL/EL 位或系统调用边界切分提交指令即可复现 Fig. 8–10;
- **字节位置切分**:注入时记录 bit index,事后按 byte 聚合,零额外成本。

### 需替代/不可行

- **GeFIN 本体不开源**——需用 gem5 自带 FaultInjection 或 Chaos(gem5-ECC 时代的 fault injector,见 ref 目录 Chaos Controlled Hardware Fault Injector System for Gem5.pdf)或 GemFI 替代;GeFIN 的"架构/物理寄存器映射区分"需要在 gem5 O3CPU 的 PhysRegFile 上自写补丁(gem5 现代版 `renameMap` 可访问,工作量中等);
- **"提交点五分类"需要改造 O3CPU commit 阶段**(在 commit 处捕获周期/PC/opcode/operands/寄存器值并与黄金轨迹比对):gem5 无现成钩子,需要 debug 标志 + 外部 diff 工具或自定义 probe PointListener;这是复现工作中最大的工程点;
- **Cortex-A72 精确复刻**:gem5 有 `realview` 板卡近似的 A72 类配置,但 TC'23 的 Table I 数值在 PDF 中未逐项给出(原文仅给结构清单),只能按公开 A72 TRM 反推;
- **"不可见 SDC"检测**:需要在模拟中追踪 modified cache line 的回写与 DMA 读——gem5 可以用 cache 监听端口实现,但工作量不小;更务实的替代是在 SDCShield 真机上用大输出负载(如大文件 sha/zstd)做对照实验来验证"输出体积 ↔ 逃逸故障率"的趋势;
- **ROB/LQ/SQ 注入**:gem5 O3 的 ROB/LSQ 是 C++ 容器非平坦 SRAM 模型,注入"表项中的 PC/specifier 位"需要改 O3 源码,难度高——可降级为只验证"这些结构零 SDC"的定性结论。

**最小复现路径(建议)**:gem5 ARM DerivO3CPU(A72 类配置)+ full-system Linux + MiBench 3 个代表负载(bitcount 小输出 / sha 中输出 / blowfish 大输出)× L1D data、RF、D-TLB 三结构 × 1,000 注入,产出:(a) 各结构 SDC 概率(对照 Fig. 3 三个点 53.4%/15.8%/22.2%),(b) 输出体积-逃逸故障趋势(对照 Fig. 5),(c) 字节位置直方图(对照 Fig. 11)。总计 9,000 次注入,gem5 约 1M 指令/s 吞吐下数天内可完成。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield 新 SDC 激发实验设计的可用对比轴:

| 对比轴 | 论文基线值(可引用) | SDCShield 侧对照实验 |
|---|---|---|
| 结构级 SDC 概率谱 | L1D data 53.4% / L1D tag 38.0% / L2 36.9% / D-TLB 22.2% / RF 15.8% / L1I data 7.3% / L1I tag+I-TLB 0.2% / ROB+LQ+SQ 0%(Fig. 3) | 真机无法选结构注入,但可论证:SDCShield 测试内核应以数据通路(FMA/SIMD ALU、load/store 数据)为主攻面——恰好是 L1D/L2/RF 数据位对应的功能面;ROB/LSQ 类结构故障天然翻成 crash,不必专门造激发 |
| 输出体积 ↔ 逃逸故障 | bitcount(<1KB)=0 → blowfish(>3MB)显著(Fig. 5) | 大输出流式校验测试(边写边比对 vs 最后统一比对)的逃逸率差异实验 |
| 时间性损坏 SDC | Execution Time Error:L1D tag 13.1%、D-TLB 11.8%(Fig. 6 左上) | 周期级时延敏感链(依赖时序的复合运算)的激发实验设计依据 |
| 内核指令 SDC 贡献 | L1I tag ~77%、L1D tag >50%、L1D data/L2 >30%(Fig. 10) | 评估 SDCShield 测试是否需要覆盖 syscall 密集路径(如 ipsec 套件天然高内核占比) |
| 字节位置偏置 | L1D/L2 低字节更危险;RF B2/B3 峰值(Fig. 11) | 故障注入(或人工位翻转 golden)时的位位置分层设计;对 SEVI/SVE 尾数位的讨论有直接参照价值 |
| 五分类互斥性 | Data Corruption ≠ 唯一 SDC 源 | 新实验若只测"数据损坏"类,需明示覆盖率上限并引用本文 Fig. 6 说明缺口 |

## 9. 局限与坑

1. **纯瞬态单比特模型**:虽论证覆盖六类物理机制,但 Meta/Google 报告的机队 SDC 主要是制造缺陷/边际性缺陷(永久/间歇故障);多比特仅做定性推论(IV-C)无数据。复现时若研究真实机队问题应叠加永久故障模型(参照 E27 ITC'23 有永久故障数据)。
2. **无 FP 负载**:FP 物理寄存器堆被排除(基准无 FP 操作)——对 SDCShield 的 SIMD/FMA 类测试设计,本文 RF 数据只覆盖整数寄存器堆,外推需谨慎。
3. **MiBench 而非 SPEC**:嵌入式负载,执行 100M–1.4B 周期;其结论对服务器级负载(SVD/GEMM/FFT)外推未经本文验证。
4. **微架构级建模的固有误差**:组合逻辑脆弱性未建模(作者引用 [21][23] 论证 SRAM 主导,成立但非零);RTL 级才可见的故障形态(如时序毛刺)不在其中。
5. **GeFIN 不开源 + Table I 参数未在正文展开**:精确数值复现依赖同组其他论文(ISCA'21 Table II / TETC'23)拼合配置。
6. **"不可见 SDC"无独立图表数值**(Fig. 5 只有趋势):引用时只能引定性结论。
7. **Armv7 对照只覆盖 Fig. 3 趋势**,字节/内核分析只在 Armv8 上做。
8. **统计口径注意**:Fig. 3 是"非 Benign 故障中的 SDC 概率"(条件概率),不是 AVF(全部故障中的 SDC 概率)——引用时常被混淆;AVF 口径的数字在 E27(ITC'23)里有。

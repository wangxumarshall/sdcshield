# Differential Fault Injection on Microarchitectural Simulators(IISWC 2015,雅典大学 University of Athens)

> 作者:Manolis Kaliorakis, Sotiris Tselonis, Athanasios Chatzidimitriou, Nikos Foutris, Dimitris Gizopoulos
> DOI: 10.1109/IISWC.2015.28(原文页码 172–182)
> 源文件:`docs/paper/ref/Differential_Fault_Injection_on_Microarchitectural_Simulators.pdf`

## 1. 研究问题与核心贡献(≤5 行)

"差分故障注入":在同一故障模型、同一负载下,用**两个独立微架构级全系统注入器**(MaFIN 基于 MARSS、GeFIN 基于 gem5)交叉验证微架构级故障注入的有效性,量化"同一 ISA(x86)在两个仿真器上的脆弱性报告差异"与"同一仿真器上两个 ISA(x86 vs ARM)的差异"。贡献:(a) 覆盖全部主要微架构存储阵列结构的注入框架;(b) 首个同 ISA 跨仿真器可靠性对比(MaFIN-x86 vs GeFIN-x86);(c) x86 vs ARM ISA 对比(GeFIN);(d) 用运行时统计(loads 数、L1D/L2 命中率、分支误预测、替换数)解释每处分歧的根因。**结论核心:两个 x86 仿真器之间的差异(最大 7.20 个百分点)大于两个 ISA 之间的差异(L1D 仅 0.55)**。

## 2. 实验方法(注入机制、故障模型、目标结构)

- **差分方法(用户重点)**:不是"两个模拟器输出互相比对找 bug"的差分测试,而是**同一实验矩阵在两个独立注入器上各自完整跑一遍,再对比脆弱性报告**(Fig.2–6 中每基准并排三根堆叠柱:M-x86 / G-x86 / G-ARM),分歧用仿真器实现差异+基准运行时统计来解释。MaFIN 与 GeFIN 按完全相同的设计原则模块化实现,保证可比性。
- **注入器三模块**(Fig.1):Fault Mask Generator(按用户参数对"结构×基准"组合生成随机故障掩码集,存入 masks repository)→ Injection Campaign Controller(读掩码、经 Injector Dispatcher 与仿真器交互、结果存 logs repository)→ Parser(可重构脚本,把日志分为六类;改分类只需改 Parser 不需重跑注入)。
- **故障掩码内容**:核、微架构结构、精确比特位置、精确注入周期或指令号、故障类型、故障数(单/多)。
- **故障模型(Table III)**:transient(某周期翻转一位,位与周期可任意指定)、intermittent(从某周期起把某位置 0/1 持续任意周期数)、permanent(某位永久置 0/1);支持多故障组合(同表项多比特/同结构多表项/跨结构)。**差分实验只用单比特翻转**。
- **目标结构(Table IV)**:GeFIN(gem5 x86/ARM):LSQ、IQ、整型/FP 物理寄存器堆、cache Tag + Data 阵列、ITLB/DTLB(Valid+Tag)、BTB;MaFIN(MARSS x86)同类但需先补建 cache 数据阵列(见 §3)。实验报告其中 5 个:整型物理寄存器堆、L1D(data arrays)、L1I(instruction arrays)、L2(data arrays)、LSQ(data field)。
- **失效分类(六类,III-A)**:Masked(应用跑完且输出与无故障运行完全一致,含异常也一致)/ SDC(最终输出文件损坏且无任何错误指示)/ DUE(带 ISA 异常指示地完成;可细分 false/true DUE,本文未分)/ Timeout(Deadlock 或 Livelock,可配置超时限=每基准无故障执行时间的 3 倍)/ Crash(进程崩溃、系统崩溃/kernel panic、仿真器崩溃三级)/ Assert(仿真器命中无法处理的高层条件断言)。vulnerability = 所有非 Masked 类之和(即 AVF 口径)。
- **提前终止优化**:注入到无效/未用表项、或故障表项被读前覆盖即安全停止——**每单个运行提速 30%–70%**。

## 3. 实验配置(模拟器版本/硬件、基准、参数)

- **仿真器**:MARSS(x86,QEMU+PTLsim 混合全系统)与 gem5(x86 + ARM)。精确版本号原文未给出(2015 年论文,对应 gem5 ~3.0/早期 2.x 时代)。
- **关键修改(用户重点——gem5/MARSS 修改点)**:
  - **MARSS 缺 cache 数据阵列**(只建模 tag/控制位,数据指令留在主存模型)——MaFIN 为其**实现了 L1D/L1I/L2 的数据阵列**才使 cache 注入可行;该修改引入约 **40% 吞吐退化**(依赖程序访存强度)。
  - MARSS x86 模型增强:L1I/L1D/L2 Valid bit 修改、BTB 扩展(无条件/条件直接分支)、关联 cache 结构的精确可靠性建模(替换算法)、L1D/L1I **新增 prefetcher**(性能相关)。
  - gem5 修改:关联 cache 结构(替换算法)的精确可靠性建模、L1D/L1I 新增 prefetcher。
- **微架构配置(Table II,三配置结构尽量对齐)**:
  | 参数 | MARSS/x86 | Gem5/x86 | Gem5/ARM |
  |---|---|---|---|
  | 流水线 | OoO | OoO | OoO |
  | 物理寄存器 | 256 int;256 FP;16 store;24 branch | 256 int;128 FP | 256 int;128 FP |
  | IQ | 32 | 32 | 32 |
  | LQ/SQ | 32(unified) | 16/16 | 16/16 |
  | ROB | 64 | 40 | 40 |
  | 功能单元 | 2 int ALU;2 FP ALU;4 AGU | 6 int ALU;2 complex int;4 FP ALU;2 FP mul/div;4 SIMD | 2 int ALU;1 complex int;2 FP&SIMD |
  | L1I/L1D | 32KB,64B,128 组,4-way,write-back | 同左 | 同左 |
  | L2 | 1MB,64B,1024 组,16-way | 同左 | 同左 |
  | 分支预测/BTB/RAS | Tournament;直转 4-way 1K 项 BTB+间转 4-way 512 项;RAS 16 | Tournament;条件+无条件 direct-mapped 2K 项;RAS 16 | 同 Gem5/x86 |
- **基准**:MiBench 10 个——djpeg、search、smooth、edge、corner、sha、fft、qsort、cjpeg、caes(全部跑到完成,除安全提前停止)。
- **注入规模**:Leveugle DATE'09 统计抽样公式:99% 置信/3% 误差 → 1843 次;**取整为每"结构×基准"2000 次注入(实际误差 2.88%)**;3%→5% 误差可降到 663 次(战役时间省约 3 倍)。总计 **300,000 次注入**(5 结构 × 10 基准 × 3 工具配置 × 2000)。
- **战役资源**:约 1 个月,10 台工作站、约 100 并行线程。

## 4. 实验步骤(可操作流程,编号)

1. 对每个"结构×基准"组合,由 Fault Mask Generator 生成 2000 个随机单比特瞬态故障掩码(覆盖整个仿真时间),入 masks repository。
2. 跑无故障 golden run(每配置),记录输出与执行时间(Timeout 阈值=3×)。
3. Campaign Controller 逐掩码经 Dispatcher 调起 MARSS/gem5 全系统仿真,注入并在日志记录结果;无效表项/被覆盖即安全提前停。
4. 多工作站(约 100 线程)并行直至 300,000 注入完成。
5. Parser 把每条日志分为六类(Masked/SDC/DUE/Timeout/Crash/Assert);vulnerability=非 Masked 之和。
6. 三个配置分别汇总每结构每基准的分类占比(Fig.2–6 三柱并排)。
7. 差分分析:计算 M-x86 vs G-x86(同 ISA 跨仿真器)与 G-x86 vs G-ARM(同仿真器跨 ISA)的平均 vulnerability 差。
8. 对每个 >5 个百分点的基准级分歧,提取运行时统计(executed/committed loads、L1D/L2 读写命中率、L1I 替换数、分支误预测数)归因到仿真器实现差异(见 §5)。

## 5. 实验数据(关键数值 + 图表号)

**平均 vulnerability 差异(§IV-C 开头,核心差分结论)**:
- M-x86 vs G-x86(两 x86 仿真器):**L1D 7.20 个百分点、L1I 3.61、L2 1.36**。
- G-x86 vs G-ARM(两 ISA):L1D 仅 **0.55**、L2 仅 **0.13**、L1I 2.03(x86 更脆弱)。

**各结构(Fig.2–6,范围与均值)**:
- **整型物理寄存器堆(Fig.2)与 LSQ(Fig.6)**:所有配置、所有基准下 vulnerability 几乎恒 <3%(最不脆弱;数据生命周期短)。
- **L1D(Fig.3)**:最脆弱结构之一;最低 2.5%(search@MaFIN-x86)至最高 **47.3%**(cjpeg@GeFIN-x86);平均 MaFIN-x86 <15%,GeFIN 两 ISA 均 >22%。
- **L1I(Fig.4)**:5.3%(smooth@MaFIN-x86)至 34.5%(caes@MaFIN-x86);平均 MaFIN-x86 ≈19%,GeFIN 两 ISA >14%。
- **L2(Fig.5)**:平均 6%–7%(三配置一致性好,差约 1 个百分点)。

**关键 Remark(根因分析,编号照原文)**:
- R1:LSQ 上 MaFIN 恒比 GeFIN 高约 1 个百分点——MARSS 用统一 load/store 队列,gem5 分队列且只有 store queue 存数据(GeFIN 注入只影响 store)。
- R2:RF 与 LSQ 的非 Masked 行为呈五类混合,具体比例随基准。
- R3:L1D 的 ~7 点差距两大根因:(a) MARSS load 发射更激进(不等与更早 store 的别名判定即发射),executed loads 远多于 gem5 而 committed loads 接近 → L1D 故障更多被掩蔽(fft/cjpeg/caes 上 MaFIN 多发射 2.6×/4.7×/2.0× loads);(b) **MARSS 用 QEMU hypervisor 处理系统功能与未实现指令,QEMU 运行期间不访问微架构 cache(访存直达主存)→ 期间 L1D 故障被掩蔽**;gem5 全内部处理无此掩蔽。例外:qsort/smooth 中 MaFIN 反而更脆弱(GeFIN-x86 L1D 读命中率低 0.64×、写命中率高 1.91×/1.57× → MaFIN 中故障更少被覆盖)。
- R4:**L1D 的主导失效类是 SDC,SDC 是其余四类非 Masked 之和的 3–5 倍**(全部基准+均值)。
- R5:GeFIN-x86 vs GeFIN-ARM 的 L1D 大差基准:fft(ARM store 指令多 2×)、cjpeg(ARM L1D 写 miss 多 6× → x86 更脆弱)、qsort(x86 模型 L1D 替换多 4× → ARM 更脆弱)。
- R6:QEMU 不影响 L1I(hypervisor 仅在 decode 之后被调用,fetch/L1I 访问已完成);前端差异:Tournament 元预测器在 MARSS 绑定分支地址、在 gem5 绑定全局历史 → 不同访存模式与 L1I 状态;edge/corner/sha 上 MaFIN 误预测少 0.83×/0.82×/0.68× → GeFIN 从下层带入更多 L1I 块、更多覆盖故障。
- R7:fft/qsort/caes 上 L1I 两 ISA 差 >5 点:ARM 模型 L1I 块替换多 4.2×/2.0×/7.2× → x86 更脆弱。
- R8:**L1I 的 SDC 远少于 L1D**;L1I 的主导非 Masked 类:MaFIN 为 **Assert**、GeFIN 为 **Crash**——因 MARSS 代码断言检查点多得多,gem5 断言少而故障最终走向 crash(脚注 6:L1D 非 SDC 类同样呈现 MaFIN 多 Assert、GeFIN 多 Crash)。
- R9:L2 统一缓存 → SDC 与其他异常类平衡。
- R10:cjpeg/caes 的 L2 两工具差 >5 点(cjpeg GeFIN-x86 L2 写 miss 多 1.2×;caes 写命中多 1.54×)。
- R11:djpeg 是 L2 两 ISA 差 >5 点的唯一基准(x86 L2 读命中少 0.5×、写 miss 多 6.8× → x86 更不易脆弱)。

**关于用户预期的 "XFACE/FI 机制"与"检测到的 gem5 bug 清单"**:**原文均不存在**。全文(含参考文献)无 XFACE(已全文检索验证;XFACE 是葡萄牙 Coimbra 的 FPGA 级注入器,属另一研究线);论文也没有给出任何"gem5 bug 清单"——差分分析揭示的是**仿真器实现差异导致的系统性报告分歧**(QEMU hypervisor 旁路 cache、激进 load 发射、断言密度、前端预测器绑定方式),这些是设计语义差异而非软件缺陷;最接近"发现仿真器建模缺陷"的表述是 MARSS 缺 cache 数据阵列(需要补建,代价 40% 吞吐)。

## 6. 实验结论(编号列出)

1. **同一 ISA 在两个仿真器上的脆弱性报告差异(最大 7.20 点)大于同一仿真器上两个 ISA 的差异(0.13–2.03 点)**——可靠性结论对"仿真器选择"比对"ISA"更敏感。
2. 整型物理寄存器堆与 LSQ 是最不脆弱结构(vulnerability<3%);一级缓存最脆弱(L1D 平均 >14–22%,L1I >14–19%);L2 居中(6–7%)。
3. **L1D 故障的主导结局是 SDC(SDC 为其他非 Masked 类之和的 3–5 倍);L1I 故障以 Crash/Assert 为主、SDC 稀少**——与 gem5-MARVEL(HPCA'24)Observation #5 一致(同一团队的前置工作)。
4. 分歧可完全归因到仿真器实现细节(MARSS 的激进 load 发射、QEMU hypervisor 旁路、统一 LSQ、高断言密度;gem5 的全局历史绑定预测器),且均有运行时统计佐证——微架构级注入结论引用时必须声明所用仿真器。
5. 统计抽样上,2000 注入/99% 置信/2.88% 误差的配置下 5 结构×10 基准×3 配置共 300,000 次注入可在 10 工作站/100 线程/1 个月内完成——大规模差分战役工程可行。
6. ACE 分析过估(7×[14];精细 ACE 仍 3×[45])的背景下,双注入器互证是提高微架构级注入可信度的手段。

## 7. 复现要点(ARM64/gem5 复现最小版本)

### 可直接复用
- **差分方法论本身**:同一故障矩阵在两个独立实现上跑并对比分歧、用运行时统计归因——SDCShield 论文若同时提供"gem5 仿真注入"与"真实硬件激发"两条证据线,其交叉验证逻辑可直接套用本文框架(声明两条线的实现差异来源)。
- **六类失效分类**(Masked/SDC/DUE/Timeout/Crash/Assert)与 DUE 可细分 false/true 的设计——比 gem5-MARVEL 三分类细;**"仿真器崩溃/Assert 与被测系统崩溃分层"**对 SDCShield 有直接对应(fork 子进程崩溃 vs 框架自身崩溃,现有 CrashContext 机制可区分)。
- **Timeout=3× 无故障执行时间**的可配置阈值。
- **2000 注入/99%/2.88%** 与 663/5% 误差的样本量换算。
- GeFIN-ARM 的结构覆盖清单(LSQ/IQ/RF/caches/TLB/BTB)与 Table II ARM 配置可作为 gem5 ARM64 复现起点(需升级到现代 gem5 的结构名)。
- 提前终止优化(30–70% 提速)的实现思路。
- 运行时统计归因清单(executed vs committed loads、各级命中率、替换数、误预测数)——在真实硬件上对应 PMU 指标,可与 CHAOS 的 HPC 探针方法结合。

### 需替代/不可行
- **MARSS 已死**(项目停止维护多年),MaFIN 侧复现不可行;差分复现需另选第二仿真器(如 QEMU+独立计时层、Simics、或 gem5 不同 CPU 模型 O3 vs Minor 互证——语义差异小得多,差分价值降低)。
- gem5 版本过老(2015),现代 gem5 中 IQ/LSQ/重命名实现重构,注入钩子需全部重写;GeFIN 无公开仓库(见 §9)。
- 真实硬件(SDCShield)上"结构级注入"整体不可行;本文价值在方法论与结论引用,不在执行复现。
- XFACE 相关内容原文不存在,无法复现(见 §5 说明)。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | 论文基线(GeFIN-ARM 为主) | SDCShield 对应 |
|---|---|---|
| 结构脆弱性排序 | RF/LSQ <3% < L2 6–7% < L1I ≈14–19% < L1D >22%(Fig.2–6) | 激发负载按结构画像设计:数据侧(L1D 类)激发应最高优先——与 SDCShield 现有 GEMM/压缩类负载选择一致 |
| L1D-SDC 主导 | SDC=其余非 Masked 之和 3–5×(R4) | "错误多、崩溃少"的负载画像基准 |
| L1I 主导 Crash/Assert | R8 | 崩溃捕获类测试的画像基准 |
| 失效六分类+分层崩溃 | III-A | SDCShield 结果分类粒度(可报告"检测到/SDC/崩溃/挂起/框架Assert") |
| 样本量 | 2000/99%/2.88%;663/5%(§IV-A) | 实验重复次数设计 |
| 差分互证 | 同 ISA 跨仿真器差 7.20 点 > 跨 ISA 0.55 点 | **论文论证模板**:SDCShield 新激发实验若与既有工具(OpenDCDiag 原版/QARAT/SiLIFuzz 类)在同一硬件上对比,应预期并解释工具间差异 ≥ ISA 间差异的量级关系 |
| 运行时统计归因 | R3/R5/R7/R10/R11 的倍数(2.6×/4.7×/6×/7.2× 等) | 用 PMU 统计解释检出率差异的范式 |

## 9. 局限与坑

1. **无开源 URL**:MaFIN/GeFIN 均未公开(CHAOS 论文 II-C 亦把 GeFIN 归为闭源工具);复现只能按论文描述自建。
2. **仿真器过时**:MARSS 停维护;gem5 为 2015 版且无精确版本号;MARSS 的 QEMU 混合架构使其结论(QEMU 旁路掩蔽)在现代工具链无对应物。
3. **用户预期错位(重点)**:本篇没有 XFACE(那是独立的 FPGA 级注入工具线,不在此文),也没有"检测到的 gem5 bug 清单"——差分发现的都是**仿真器语义差异**而非软件 bug;撰写综述/引用时不可写成"差分注入发现了 gem5 的 bug"。
4. 差分实验只用单比特瞬态故障;intermittent/permanent/多故障仅声明支持、无数据。
5. Fig.2–6 是堆叠柱状图,正文只给出均值与个别极值(2.5%/47.3%/5.3%/34.5% 等)与差值(7.20/3.61/1.36/0.55/0.13/2.03),逐基准精确占比不可从文本获得。
6. 只覆盖 5 个结构(IQ/TLB/BTB/FP RF 可注入但未报告);只覆盖 MiBench 嵌入式负载;单核。
7. ISA 对比仅在"尽量对齐"的配置上(功能单元数、ROB 大小仍有实质差异,Table II),故 "ISA 差异小" 的结论也受配置混杂因素影响(gem5-MARVEL 2024 对此同样自我限定)。
8. MARSS 补建 cache 数据阵列引入 40% 吞吐退化,可能连带改变其时序行为,对差分结论有潜在的二阶影响(论文未讨论)。

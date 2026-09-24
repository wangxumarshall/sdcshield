# CHAOS: Controlled Hardware fAult injectOr System for gem5(arXiv 2602.02119,2026 年 2 月,卡塔尼亚大学 University of Catania)

> 作者:Elio Vinciguerra, Enrico Russo, Giuseppe Ascia, Maurizio Palesi
> 源文件:`docs/paper/ref/Chaos Controlled Hardware Fault Injector System for Gem5.pdf`(arXiv:2602.02119v1, 2 Feb 2026)
> **开源 URL:https://github.com/eliovinciguerra/CHAOS.git**(参考文献 [12],2025)

## 1. 研究问题与核心贡献(≤5 行)

面向现代 gem5(20+)的开源、模块化、全可配置故障注入框架,动机是现有 gem5 注入工具"过时/不兼容新版/闭源"的碎片化现状。贡献:(1) 三模块架构——CHAOReg(架构寄存器)、CHAOSCache(cache 子系统)、CHAOSMem(主存);(2) 支持全部 gem5 ISA、细粒度注入时点/类型/目标控制;(3) 每 fault 平均开销低至 0.0004%–0.0008%;(4) 用 20 个 RISC-V HPC(硬件性能计数器)作为 SDC/Masked 结果的"系统可观测性"探针——发现静默故障可造成高达数万百分比的 HPC 扰动。

## 2. 实验方法(注入机制、故障模型、目标结构)

**注意:论文三模块为 Reg/Cache/Mem(无独立 "Core" 模块;Core 侧仅有寄存器注入)。**

### CHAOReg(Algorithm 1)
- **机制**:运行时对 CPU 架构寄存器施加故障掩码,按概率参数在随机时钟周期触发。
- **参数**:probability(0–1 浮点,每时钟周期激活概率)、start/end(允许注入的时钟周期区间)、fault type(bit flip / stuck-at-0 / stuck-at-1,或 random)、mask(位掩码,设 0 则随机生成)、faulty bits(随机掩码的受影响位数)、target class(整数/浮点寄存器类,或 random 采样)、PC target(指定 PC 地址处激活,实现精确定点注入)。
- **触发逻辑**:(PC_target≠0 且 start≤curCycle≤end)或 (PC(t)=PC_target) 时启动;随机选 target class 内寄存器 r;bit flip:Value(r)←Value(r)⊕mask;stuck-at-0:Value(r)←Value(r)∧¬mask;stuck-at-1:Value(r)←Value(r)∨mask。永久故障存入专用数据结构,周期性重施加(MarkPermanentFault)。下次注入事件按 probability 比例的随机延迟调度。

### CHAOSCache(Algorithm 2)
- **机制**:在伪随机时钟周期对目标 cache 的**有效块(valid block)**随机采样注入。
- **参数**:cache(目标 cache 实例)、probability、start/end、fault type、corruption size(损坏字节数)、mask、faulty bits。
- **流程**:确认 curCycle 在区间内 → SampleValidBlock(cache) → 循环 corruption size 次:随机选块内字节 b,解析 mask/fault type,按三种类型之一施加;stuck-at 类同样 MarkPermanentFault(B, b, mask) 持续重施加。

### CHAOSMem(Algorithm 3)
- **机制**:在 [target_start, target_end] 地址范围内随机采样地址,读字节→施加故障→写回。
- **参数**:probability、start/end、target_start/target_end(地址范围)、fault type、mask、faulty bits。
- **流程**:SampleRandomAddr(target_start, target_end) → ReadByte → 三种故障施加 → WriteByte 写回。永久故障同样记录并周期重施加。

**故障模型**:三类型——bit flip(瞬态,模拟辐射软错误)、stuck-at-0 / stuck-at-1(永久)。单比特与随机多比特(faulty bits 随机)两种 campaign。注入概率按 probability 参数随机调度。

**失效分类**(II-A,五类):Crash(完全停机)、DUE(检测到但不可恢复)、SDC(未检测的数据损坏)、Masked(无影响,与无故障运行结果一致)、Timeout(超时未完成)。实验中 Crash 与 DUE 合并统计;全程无 Timeout(单比特 campaign)。

## 3. 实验配置(模拟器版本/硬件、基准、参数)

- **模拟器**:gem5 20+(兼容性声明;引用 Lowe-Power v20.0+)。**实验 ISA:RISC-V**(框架声称支持任意 gem5 ISA)。O3 CPU。
- **系统配置**:1GHz 时钟、512 MiB DDR3 DRAM、L1I 16 KiB、L1D 64 KiB、L2 256 KiB(gem5 默认参数)。
- **基准**:MiBench 8 个 C 实现——Bitcount、Blowfish、Dijkstra、JPEG、Patricia、Qsort、SHA、Susan。Table I 给出完整微架构画像(见 §5)。
- **注入规模(三档 tier,按 Leveugle [17] 方法)**:
  | Tier | 误差边际 | 置信度 | 样本数 |
  |---|---|---|---|
  | Low | e=5% | 95% | 384 |
  | Medium | e=5% | 99% | 663 |
  | High | e=1% | 99% | 16,587 |
- **注入目标 5 类**:CHAOSReg、CHAOSCacheL1D、CHAOSCacheL1I、CHAOSCacheL2、CHAOSMem(每模块独立 campaign 隔离效应)。
- **注入概率三档**(Fig.1–3 单比特 / Fig.4–6 多比特):High/Medium/Low probability(具体数值原文未给出)。
- **开销测量**:1M 时钟周期仿真,运行时随机生成掩码与故障类型(最坏情形),测量实际发生注入时的耗时。

## 4. 实验步骤(可操作流程,编号)

1. 克隆 CHAOS(github.com/eliovinciguerra/CHAOS)并按其 README 集成进 gem5 20+ 构建。
2. 配置 gem5 系统(O3 CPU、cache 层级、RISC-V FS/SE)并选定 MiBench 基准。
3. 先跑无 CHAOS 的 golden run,保存参考输出与 HPC 基线(20 个计数器,Table II:mcycle/mtime/minstret、mhpmcounter4-15 指令分类、mhpmcounter22 分支误预测目标、27-31 各类 cache/TLB miss)。
4. 为每模块配置参数(Reg:probability/start/end/fault type/mask/fault bits/target class/PC target;Cache:+cache/corruption size;Mem:+target 地址范围),按 tier 生成 384/663/16,587 个故障。
5. 运行 campaign:每 fault 一次仿真,注入按概率随机触发;永久故障由监控例程逐周期维持。
6. 结果按 Crash(含 DUE)/Timeout/SDC/Masked 分类:输出与 golden run 逐一比对。
7. 对 SDC 与 Masked 结果计算 HPC 平均绝对百分比变化:Δmean = (1/n)Σ|h_ff(i)−h_f(i)|/h_ff(i)×100,n=20。
8. 换注入概率档(High/Medium/Low)与多比特模式(每次注入翻转位数随机)重复 4–7。

## 5. 实验数据(关键数值 + 图表号)

**基准微架构画像(Table I)**:
| 指标 | Bitcount | Blowfish | Dijkstra | JPEG | Patricia | Qsort | SHA | Susan |
|---|---|---|---|---|---|---|---|---|
| 总周期(M) | 196 | 125 | 258 | 364 | 260 | 153 | 125 | 180 |
| 总指令(M) | 519 | 311 | 460 | 540 | 218 | 300 | 406 | 376 |
| CPI | 0.38 | 0.40 | 0.56 | 0.67 | 1.19 | 0.51 | 0.31 | 0.48 |
| 分支误预测% | 1.87 | 0.74 | 0.1 | 11.53 | 6.92 | 1.65 | 3.59 | 11.68 |
| 读% | 5.21 | 22.32 | 23.72 | 22.6 | 17.82 | 17.81 | 11.53 | 26.65 |
| 写% | 0.002 | 11.75 | 11.56 | 9.28 | 11.24 | 13.31 | 4.49 | 4.68 |
| 整数 ALU% | 94.79 | 65.93 | 64.71 | 67.01 | 69.12 | 66.35 | 83.98 | 65.75 |
| L1D miss% | 0.03 | 0.02 | 5 | 1.12 | 0.23 | 2.53 | 0.01 | 0.4 |
| L1I miss% | 0.003 | 0.005 | 0.002 | 0.006 | 23.53 | 0.003 | 0.002 | 0.002 |
| L2 miss% | 77.18 | 51.8 | 70.52 | 75.14 | 0.59 | 70.59 | 79.96 | 98.23 |

**开销(§IV-B)**:每注入 fault 平均开销(相对无故障仿真)——CHAOReg 0.0004%、CHAOSCache 0.0008%、CHAOSMem 0.0004%;每永久故障逐周期监控开销 6.6×10⁻⁶%(Reg)、6.1×10⁻⁶%(Cache)、8.1×10⁻⁶%(Mem)。

**单比特失效分布(Fig.1 High / Fig.2 Medium / Fig.3 Low 概率;文字结论)**:
- 寄存器:注入概率与崩溃率正相关;High 概率下 Crash 占主导(jpeg/qsort/bitcount 压倒性多数);Low 概率下 dijkstra/susan 崩溃率大降,转向 Masked/SDC。
- **L1I cache:对故障最敏感,三种概率下崩溃率均持续高**——改 opcode/操作数几乎必致非法操作或段错误。
- L1D/L2:High 概率下与 L1I 几乎同为崩溃主导;Low 概率下略降、让位于 SDC/Masked。
- **主存:全场景崩溃率可忽略**——地址空间巨大,故障大概率打不中关键段,多为 SDC 或 Masked(未用数据)。
- Timeout:少数结局;**Bitcount 最多(Low 概率 13%、Medium 10%)**,JPEG/Qsort/SHA 1–4%;**仅出现在 Low/Medium 概率,High 概率完全消失**(稀疏故障易微妙改控制流如循环条件而不毁指令流;高概率腐蚀严重到立即硬件异常)。

**HPC 变化(单比特,Table III–VII;SDC 与 Masked 列)**:
- Reg(Table III):多数中等(<10%);Qsort Low 26.01%;Dijkstra 随概率降低 4.44%→8.05%;Patricia/SHA≈0%。
- CacheL1D(Table IV):Qsort 极端异常——High 20,841.92%、Medium 83,211.58%、Low 52,867.90%(排序指针/数组边界损坏→近无限循环但最终完成);其余基准 <2%。
- CacheL1I(Table V):Bitcount High 7,767.44%;Dijkstra Low 2,460.71%;SHA Medium 71.77%。
- CacheL2(Table VI):Bitcount High 5,285.90%/Low 988.34%;Qsort 全档 148.99–905.74%。
- Mem(Table VII):几乎所有基准 0.00–0.30%(Qsort High 6.58%)——主存故障连微架构活动都不扰动。

**多比特失效分布(Fig.4–6)与 HPC(Table VIII–XII)**:
- Reg/L1I/L1D:即使在较低注入概率下 Crash 也成为压倒性结局;Bitcount 与 Qsort 在 High 概率下崩溃率近 100%。
- 主存:多比特下崩溃率仍可忽略,Masked+SDC 主导("稀释"效应)。
- Qsort + Reg + Low:HPC 变化 **42,912.66%**(多比特寄存器故障摧毁循环边界/数组索引→病态执行数百万额外指令而不崩);Qsort + L1D Medium/Low 81,920.37%/81,817.61%;Qsort + L2 High 23,822.25%;Blowfish + L2 High 96.34%。
- 主存多比特 HPC 变化 <0.02%——最难通过性能计数器检测。

## 6. 实验结论(编号列出)

1. 寄存器与 L1 cache 的故障以 Crash 为主;注入概率越高崩溃率越高;多比特使崩溃在更低概率下即占主导。
2. **L1I 损坏几乎必致命**(与 gem5-MARVEL Observation 一致);主存故障几乎从不崩溃——地址空间稀释效应。
3. **Timeout 与注入概率反相关**:仅稀疏故障能造成"活死"(循环条件被微妙改变),重度腐蚀直接硬件异常。
4. **静默故障可在 HPC 上留下巨大指纹**(Qsort L1D 83,211% 等),即使输出正确(Masked)或仅 SDC——HPC 可作为非侵入式 SDC 检测代理,能发现输出校验漏掉或太晚发现的异常。
5. 主存故障"双重静默":既不崩溃也不扰动微架构活动,是最难检测的故障类别。
6. 多比特注入使行为两极化:要么立即杀死应用(Reg/L1),要么静默藏于内存背景;算法级离群点(Qsort)表明"活下来"可能比崩溃的计算代价更高。

## 7. 复现要点(ARM64/gem5 复现最小版本)

### 可直接复用
- **工具本身**:GitHub 开源(github.com/eliovinciguerra/CHAOS),兼容 gem5 20+,是 4 篇中唯一可直接拉代码复现的;在 Arm ISA 下重跑(框架 ISA 无关,cache/Reg/Mem 注入点与 ISA 解耦)。
- **三档统计口径**(384/663/16,587 对应 5%/95%、5%/99%、1%/99%)与 gem5-MARVEL(1000@3%/95%)、GemFI(2501@1%/99%)构成完整的样本量选择参考系。
- **HPC 作为 SDC 探针的方法论**:Δmean(20 计数器平均绝对百分比变化)公式与"Masked 结果也有 HPC 扰动"的发现——**对 SDCShield 价值极高**:SDCShield 本就运行在真实硬件上,可用 perf/ARM PMU(对应 ets2024_gizopoulos 的 HPC-SDC 检测思路)在激发测试期间旁路记录 HPC 偏差,把"输出正确"细分为"输出正确且 HPC 无扰动"(真 Masked)vs"输出正确但 HPC 巨变"(潜在漏检)。
- **PC target 定点注入参数** 与 start/end 周期窗口:在 gem5 内做定向 ARM64 激发复现时可精确控制注入时点。
- Table I 的 8 基准微架构画像可作为 gem5 侧负载特征校准参考(如 Patricia L1I miss 23.53% 的 I-footprint 压力型负载)。
- 开销口径(每 fault 0.0004–0.0008%)可作为自制注入器的性能达标线。

### 需替代/不可行
- 实验 ISA 是 RISC-V;ARM64 复现需换 ISA 重跑(mhpmcounter 名称换 ARM PMU 事件)。
- 无 DSA/加速器维度;无 HVF;无微架构结构级(PRF/LQ/SQ)注入——只有架构寄存器与 cache/主存(比 gem5-MARVEL 的结构覆盖窄;架构寄存器≠物理寄存器堆,不含重命名/ROB 等)。
- 故障类型仅 bit-flip/stuck-at,无 LLFI 式算子级注入、无电压/时序模型。
- 真实硬件(SDCShield)无法注入 cache/寄存器;只能利用其结论:HPC 监控 + "主存型故障最难检测"→激发实验应包含 HPC 旁路观测。
- 论文未给 gem5 精确版本号/commit、未给 probability 三档的具体数值、未给仿真总时长(开销测量为 1M 周期)——复现需在仓库中核对。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | 论文基线 | SDCShield 对应 |
|---|---|---|
| 注入开销 | 0.0004–0.0008%/fault;监控 6.1–8.1×10⁻⁶%/永久故障(§IV-B) | 激发负载自身开销(相对基线吞吐)同口径报告 |
| 失效五分类(Crash+DUE/Timeout/SDC/Masked) | Fig.1–6 | SDCShield 测试结果分类可直接沿用(现框架已区分 crash/skip/fail) |
| Timeout 与故障密度反相关 | Bitcount 13%@Low,High 概率归零 | 真实缺陷激发中"挂起类"信号的密度依赖性 |
| HPC 指纹幅度 | Reg ≤26%、L1D 高至 83,211%、Mem <0.3%(Table III–VII) | **最有价值**:SDCShield 激发实验加 PMU 旁路,量化"检出能力"时以这些量级为参照 |
| 主存故障不可见性 | Mem 全档崩溃≈0、HPC<0.3% | 论证 SDCShield 需要"数据内容级校验"(memcmp 金标)而非仅崩溃/性能异常检测的直接论据 |
| 与 GeFIN/gem5-MARVEL 的对比 | **原文无量化对比实验**——仅 II-C 定性论述:GeFIN/gem5-MARVEL/FIMSIM 闭源或过时,"Only GemFI and gem5-Approxilyzer are open-source, but both are outdated";gem5-MARVEL "remains closed-source"。唯一间接可比:CHAOS 0.0004–0.0008%/fault(口径为每故障)vs GemFI ≤3.3%(口径为整体仿真)——口径不同不能直接比 | SDCShield 论文若做工具对比应避免同样的口径混用 |

## 9. 局限与坑

1. **非正式出版形态**(arXiv 预印本,2026-02),未经同行评审;引用时注意标注。
2. 用户提问中的"Core 模块"实际不存在——三模块是 Reg/Cache/Mem;Core 级仅架构寄存器。
3. **与 GeFIN/gem5-MARVEL 无量化对比数据**(用户重点关注):相关工作只有定性表格;overhead 数值口径不同(GemFI 是含 O3 全程仿真的总开销,CHAOS 是每 fault 的增量开销)。
4. Fig.1–6 为堆叠条形图,只有文字概括(如"崩溃占主导""近 100%"),除 Timeout 的 13%/10%/1–4% 外无逐条数值;精确占比需从开源仓库重跑。
5. HPC 变化的极值(83,211%、42,912%)是均值型指标被少数极端离群主导的结果(近无限循环最终正常结束),分布信息(中位数/分位数)缺失;复现时建议补充分布统计。
6. SE/FS 模式、仿真总指令数、MiBench 输入规模均未说明(仅 Table I 周期数可反推规模);probability 三档数值未给出。
7. Masked 判定只对比最终输出——若故障改变了中间状态但输出恰好一致,与 HPC 扰动分析存在分类张力(Table IV 中 Masked 列几乎全 0.00 但 SDC 列有巨值,说明作者按输出分类,而 HPC 扰动大的其实多为 SDC)。
8. 单核 O3、小 cache(L1D 64KiB/L2 256KiB)与服务器 CPU(Kunpeng 920)差距大;数值不可直接迁移到真实硬件,只可迁移方法论。

# gem5-MARVEL: Microarchitecture-Level Resilience Analysis of Heterogeneous SoC Architectures(HPCA 2024,雅典大学 Informatics & Telecommunications)

> 作者:Odysseas Chatzopoulos, George Papadimitriou, Vasileios Karakostas, Dimitris Gizopoulos
> DOI: 10.1109/HPCA57654.2024.00047(原文页码 543–559)
> 源文件:`docs/paper/ref/Gem5-MARVEL_Microarchitecture-Level_Resilience_Analysis_of_Heterogeneous_SoC_Architectures.pdf`

## 1. 研究问题与核心贡献(≤5 行)

首个面向异构 SoC(CPU + DSA)的统一微架构级故障注入框架,支持 x86/Arm/RISC-V 三种 64 位 ISA、瞬态+永久故障、AVF 与 HVF 双指标。基于最新版 gem5(CPU 侧)+ gem5-SALAM(LLVM IR 动态图执行引擎,DSA 侧)构建。贡献:(1) 把 gem5-SALAM 全系统支持从 Arm 扩展到 RISC-V(GIC→PLIC、配置脚本生成器改造);(2) CPU 全硬件结构微架构级注入;(3) 首个 CPU+DSA 联合 SoC 注入;(4) 同一 fault mask 下 AVF/HVF 联合评估与传播路径追踪。

## 2. 实验方法(注入机制、故障模型、目标结构)

- **注入层级**:微架构级(microarchitecture-level),gem5 全系统(FS)模式,确定性、端到端、周期级执行、跑真实 OS(论文 III-A:这四点组合在 RTL 级不可能实现,RTL 比周期级微架构仿真慢数个数量级)。
- **故障模型**(Table III):
  - Transient:某一时钟周期翻转一个存储单元比特;位位置与周期可任意指定(随机或定向)。
  - Permanent:存储单元某位永久置 0/1(stuck-at);位位置可任意指定。
  - 支持多故障(单结构内多故障、跨结构多故障组合)与多比特;论文实验仅展示单比特翻转结果。
- **目标结构**:CPU 侧论文聚焦 5 个——(1) 整数物理寄存器堆(PRF)、(2) L1 指令缓存、(3) L1 数据缓存、(4) Load Queue、(5) Store Queue(框架本身还支持 FP PRF、L2、ROB、TLB、重命名单元等)。DSA 侧:scratchpad memory(SPM)与 register bank(RegBank)——"对 SPM/RegBank 而言 HVF 与 AVF 分析等价,因为任何故障只要不是打中无效/未用单元即非屏蔽"。
- **fault site 抽样方法**(III-D 节,原文):"For each structure, 1,000 single-bit faults are randomly generated following the uniform distribution as defined in [18](Leveugle et al., DATE 2009 统计故障注入量化误差与置信度)",即**每结构每基准 1000 个单比特故障、均匀分布**;1000 个样本对应 **3% 误差边际、95% 置信度**。全部实验合计约 **250,000 次注入运行**(15 基准 × 3 ISA × 5 CPU 结构 + 8 DSA 设计 × 若干 SPM/RegBank × 1000)。
- **失效分类**(IV-A2):
  - AVF 口径:Masked(与无故障执行无偏差)/ SDC(正常结束但输出与无故障运行不同、无任何可观察指示)/ Crash(灾难事件中断仿真、无输出)。
  - HVF 口径:Masked(故障未达 commit)/ Corruption(在 commit 阶段相对无故障 trace 检出不匹配,可含指令、操作数、数据事务或程序顺序)。
- **注入器加速**:注入到无效/未用表项、或故障表项在被读前即被覆盖时**提前终止该次运行**;多工作站并行,每个 gem5-MARVEL 实例对应一个注入故障。
- **checkpoint 机制**:扩展了 gem5 checkpointing 以同时保留微架构+架构状态(含 cache 内容),支持从任意时点开始注入而无需长 warm-up。

## 3. 实验配置(模拟器版本/硬件、基准、参数)

- **模拟器**:最新版 gem5(引用 gem5 v20.0+ 与 GitHub 仓库,2023-07 访问;论文未给精确版本号)+ gem5-SALAM(DSA 建模)。
- **CPU 微架构**(Table II,三个 ISA 完全相同):
  | 参数 | 值 |
  |---|---|
  | ISA | RISC-V / Arm / x86 |
  | 流水线 | 64-bit OoO(8-issue) |
  | L1 I-Cache | 32KB, 64B line, 128 sets, 4-way |
  | L1 D-Cache | 32KB, 64B line, 128 sets, 4-way |
  | L2 | 1MB, 64B line, 2048 sets, 8-way |
  | 物理寄存器堆 | 128 Int; 128 FP |
  | LQ/SQ/IQ/ROB | 32/32/64/128 |
- **基准**:15 个 MiBench 基准(basicmath, bitcount, qsort, dijkstra, patricia, sha, crc32, adpcm_c/adpcm_d(编/解码), fft_inv, bf_enc/bf_dec(blowfish), edges, corners, smooth)。DSA 侧 8 个 MachSuite 设计(Table IV)。
- **DSA 注入目标与容量**(Table IV):BFS: EDGES 16,384B RegBank + NODES 2,048B RegBank;FFT: IMG 8,192B + REAL 8,192B SPM;GEMM: MATRIX1 32,768B + MATRIX3 32,768B SPM;MD KNN: NLADDR 16,384B + FORCEX 2,048B SPM;MERGESORT: MAIN 8,192B + TEMP 8,192B SPM;SPMV: VAL 13,328B + COLS 6,664B SPM;STENCIL2D: ORIG 32,768B + SOL 32,768B SPM + FILTER 360B RegBank;STENCIL3D: ORIG 65,536B + SOL 65,536B SPM + C_VAR 8B RegBank。
- **注入规模**:每结构 1000 个单比特故障(均匀分布、3% 误差、95% 置信);总计约 250,000 次注入。
- **验证程序编译**:-O0 GCC(避免编译器优化干扰),内联汇编把变量/迭代器钉在寄存器。

## 4. 实验步骤(可操作流程,编号)

1. 为目标 ISA 选择硬件配置 preset(gem5 系统脚本 + 硬件结构信息,用于界定基准执行边界内的注入点)。
2. 准备 workload preset:在基准开始处打 gem5 checkpoint;基准结束时用 m5_switch_cpu 切到仿真(emulation)模式,把程序输出导出到宿主机,再终止仿真(通过 gem5 magic 指令,由宿主或被仿真负载发出)。
3. 跑一次无故障(fault-free)仿真,记录金标输出与 commit trace(HVF 用)。
4. 由 fault masks 生成器按均匀分布产生故障清单文件(结构、比特位、周期;每结构 1000 个)。HVF 与 AVF 评估共用同一份 fault mask。
5. campaign 控制器(运行脚本库)把 fault mask 分发给多个并行 worker,每个 worker 一次 gem5 实例注入一个故障,输出文件与日志落盘;注入到无效表项/故障被覆盖即提前终止以省时。
6. 结果解析器判定每类效应:AVF 口径对比程序输出(Masked/SDC/Crash);HVF 口径在 commit 阶段对比 trace(Masked/Corruption)。
7. 汇总 AVF/HVF;跨基准按执行时间加权得 wAVF:wAVF(c) = Σ(AVF_k(c)×t_k) / Σ(t_k)(V-A 节)。
8. (可选)性能-可靠性联合指标 OPF = OPS/AVF,OPS = 任务操作数/执行时间(例:GEMM 为 2×N³/ExecTime)。
9. DSA 实验:YAML 描述 SoC(加速器集群、SPM/RegBank、DMA、MMR),自动生成 gem5 配置脚本(CPU 侧用 RISC-V FS),对 SPM/RegBank 重复步骤 4–7。

**注入器正确性 sanity check(IV-F,原文关键)**:每个被支持结构都有专门验证程序。以 L1D 为例(Listing 1):分配 cache-line 对齐、等于 L1D 大小的数组(.myArrSec 段,aligned(64)),10 遍写零填满 cache(pseudo-LRU 需预热所有 way);`m5_checkpoint()` 标记注入窗口起点 → 10,000 次 nop 循环期间注入 L1D → `m5_switch_cpu()` 结束窗口 → 无注入窗口内对数组求和,非零和即故障成功注入。**10,000 次注入的 campaign 测得 AVF=100%**(即 100% 覆盖整个 L1D 内容),证明注入器正确。

## 5. 实验数据(关键数值 + 图表号)

**CPU 侧瞬态故障 AVF(每 ISA 15 基准;范围 + 结论):**
| 结构 | Arm | x86 | RISC-V | 图 |
|---|---|---|---|---|
| 物理寄存器堆 AVF | 6%–14% | 4.7%–13.2% | 5.1%–20.8%(最高,RISC-V 显著更脆弱) | Fig.4 |
| L1 I-Cache AVF | 20.5%–37.9%(wAVF 最高) | 25.2%–38.2% | 16.4%–34.9%(最低,推测因指令编码简单→掩蔽多) | Fig.5 |
| L1 D-Cache AVF | 4.3%–44.9% | 3.4%–35.1%(wAVF 最低) | 5.9%–40.9%(方差最大结构) | Fig.6 |
| Load Queue AVF | 2.4%–8.6% | 2.7%–11.1% | 3.5%–12.9%(最高基准 smooth,最低 adpcme) | Fig.7 |
| Store Queue AVF | 2.2%–6.2% | 2.1%–9.3% | 1.8%–12.0%(最高 corners,最低 adpcme) | Fig.8 |

**SDC 对 AVF 的贡献(瞬态,Fig.9–11):**
- PRF SDC AVF:Arm 0–6.9%、x86 0–3.7%、RISC-V 0.1–9.9%;SDC wAVF 比总 wAVF 低 4.6 倍(Arm)、5 倍(x86)、4 倍(RISC-V)→ RF 以 Crash 为主(Fig.9)。
- L1I SDC AVF:Arm 0.3–9.9%、x86 0.3–4.6%、RISC-V 0.2–5.7%;比总 wAVF 低 9 倍(Arm)、11.2 倍(x86)、17 倍(RISC-V)→ 指令缓存损坏几乎必 Crash(Fig.10)。
- L1D SDC AVF:Arm 1.2–43%、x86 0.7–32.6%、RISC-V 0.8–40.2%;SDC wAVF vs 总 wAVF:Arm 20.6% vs 23.8%、x86 13.7% vs 17.1%、RISC-V 17.8% vs 21.7% → **L1D 上 SDC 是主导失效模式**(Fig.11)。

**永久故障 SDC 概率(Fig.12–13):**
- L1I:Arm 0.1–2.3%、x86 0.1–1.3%、RISC-V 0.3–2.7%(Fig.12)。
- L1D:Arm 5.1–53.3%、x86 4.4–64.7%、RISC-V 4.4–70.8%;RISC-V 平均最高(Fig.13)。

**DSA 侧 AVF(Fig.14,SDC/Crash 分解;表内数值为文本抽取的图中标注):**
BFS EDGES 35%、NODES 20%(几乎全 Crash——RegBank 数据被用作图遍历索引,致越界/超长执行);FFT IMG 44.5%、REAL 45.1%(全 SDC);GEMM 87%(MAT1)/46.8%(MAT3);KNN 50.3%(NLADDR)/17.9%(FORCEX);MERGESORT 89.5%(MAIN)/9.2%(TEMP);SPMV 30.9%(VAL)/41.6%(COLS);STENCIL2D 51.3%(ORIG)/48.8%(SOL)/6.4%(FILTER);STENCIL3D 40.6%(ORIG)/49.2%(SOL)/73.9%(C_VAR)。输出 SPM(MAT3/TEMP)AVF 低于输入 SPM,因输出 SPM 全程被写覆盖。

**敏感性/联合指标:**
- PRF 大小敏感性(RISC-V,96/128/192 个物理寄存器):寄存器越少 AVF 越高(利用率升高),三 ISA 一致(Fig.15)。
- CPU vs DSA 四算法(GEMM/BFS/FFT/KNN,Fig.16):DSA 的 AVF 显著高于 RISC-V CPU(如 GEMM CPU≈31.9% vs DSA≈67% 区间;FFT CPU 12.7%/DSA 33.3%;BFS CPU 23.9%/DSA 44.8%;KNN CPU 16.2%/DSA 46.7%,按图中标注顺序),但 OPF 相反:CPU 0.2K/2.1K/4.5K/2.3K vs DSA 0.5K/4.6K/18.3K/7.8K(GEMM/BFS/FFT/KNN 顺序按图标注)→ 加速器"更脆弱但性能-可靠性折中更好"。
- GEMM DSA 并行功能单元数 2/4/8/16/32:AVF 80.4%/75.0%/62.7%/58.0%/58.0%(Fig.17a)——功能单元越少、SPM 访问越慢,故障更多传播到输出。
- HVF 恒 ≥ AVF(RF 与 L1D,fft/qsort/sha/corners/edges/smooth 六基准,Fig.18)。

## 6. 实验结论(编号列出)

1. **Observation #1**:该微架构配置与负载下,PRF 瞬态故障脆弱性 RISC-V 显著高于 Arm 和 x86(5.1–20.8% vs 6–14%/4.7–13.2%)。
2. **Observation #2**:L1I 瞬态脆弱性 RISC-V 最低(16.4–34.9%),Arm 最高;归因于 RISC-V 简单编码→更高掩蔽。
3. **Observation #3**:L1D 上 Arm/RISC-V 比 x86 更脆弱(x86 复杂指令产生更复杂访存模式→硬件掩蔽概率高)。
4. **Observation #4**:LQ/SQ 瞬态脆弱性 Arm 最低(内存序模型差异影响 in-flight 负载数)。
5. **Observation #5**:SDC 在整型 PRF 和 L1I 中远比 L1D 稀少;**L1D 中 SDC 是主导失效模式**(寄存器坏值→非法访存即 Crash;I-Cache 坏块→非法指令即 Crash;D-Cache 坏值更容易静默传播到输出)。
6. **Observation #6**:大多数加速器设计在故障下产生**极高 SDC 率**(datapath-heavy、输入数据几乎无控制依赖)→ 加速器的故障缓解应聚焦数据损坏而非控制流。
7. **Observation #7**:加速器虽更脆弱,但 OPF 显示其性能-可靠性折中优于 CPU。
8. **Observation #8**:DSA 并行功能单元减少时 AVF 显著上升。
9. ISA 结论带强限定:仅适用于该统一微架构+MiBench 负载组合,不构成"某 ISA 更健壮"的一般结论(IV-B 开头明确声明)。

## 7. 复现要点(ARM64/gem5 复现最小版本)

### 可直接复用
- **统计抽样规范**:每结构 1000 单比特故障、均匀分布、3% 误差/95% 置信(Leveugle DATE'09 公式)——直接作为 SDCShield 实验的样本量与抽样依据。
- **AVF 失效三分类**(Masked/SDC/Crash)与 **HVF 二分类**(Masked/Corruption @commit)及"HVF 恒 ≥ AVF"的传播路径分析框架。
- **wAVF 加权聚合公式**(按基准执行时间加权)与 **OPF=OPS/AVF** 联合指标——SDCShield 可用"每秒检测到的 SDC 激发数"类比 OPF。
- **注入器 sanity-check 方法论**(IV-F):对每个目标结构写 -O0+内联汇编验证程序,预期 AVF=100% 的设计——SDCShield 每加一个新微结构测试可直接套用此校验思路(现有 zstd19 等金标测试即是其应用层对应物)。
- **提前终止优化**:注入无效/未用表项或故障被覆盖即终止——对真实硬件不可用,但对 gem5 侧复现有效。
- Arm ISA 结果本身(Table II 配置下 PRF 6–14%、L1D 4.3–44.9%、L1D-SDC wAVF 20.6%)可作为 gem5 复现校准目标值。

### 需替代/不可行
- **对 SDCShield(真实 Kunpeng 920 硬件)而言,微架构级注入本身不可行**:真实芯片无法访问 PRF/LQ/SQ 内部状态。只能反向利用其结论——L1D/数据通路类结构 SDC 占比最高→优先设计数据通路型激发负载(SDCShield 的 FMA/GEMM/SIMD 负载选择与该结论一致);PRF/L1I 故障多表现为 Crash/挂起→SDCShield 的崩溃捕获(--on-crash=context)对应这一类。
- gem5-SALAM/RISC-V 移植部分与 ARM64 缺陷检测无关,可整体跳过。
- 论文未提供脚本/仓库 URL(见 §9),精确复现需自行实现注入器;gem5 需自己改(在 O3CPU 各结构处插注入钩子),工作量约人周级。
- 多故障/多比特模式论文只声明支持、未给数据,复现无基线可比。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | 论文基线(Arm ISA) | SDCShield 可测什么 |
|---|---|---|
| L1D 是 SDC 主导结构 | L1D SDC wAVF 20.6%(总 23.8%),SDC/Crash ≈ 6.4:1(Fig.11) | 数据通路+访存压力负载(zstd19、eigen、openblas_dgemm)的 SDC 检出率——若真实硬件缺陷也集中在数据侧,应呈现"错误多、崩溃少"画像 |
| PRF/L1I 以 Crash 为主 | PRF SDC wAVF 低 4.6 倍、L1I 低 9 倍于总 AVF(Fig.9/10) | SDCShield 崩溃类信号(SIGILL/SIGSEGV,cause_sigill 自检)占比 |
| 永久故障 L1D SDC | Arm 5.1–53.3%(Fig.13) | 真实制造缺陷/老化(集群锚定的 cn23154 类故障)与瞬态画像差异 |
| 每结构 1000 注入/3% 误差/95% 置信 | 全文口径 | SDCShield 激发实验的样本量设计直接引用 |
| 加速器 SDC 主导(Fig.14) | SDC 占 AVF 多数(FFT/GEMM/MERGESORT 全 SDC) | SDCShield 算子级测试(FFT/矩阵类)的"高 SDC 低 Crash"预期画像 |
| OPF 联合指标 | Fig.16 数值 | SDCShield 可定义 OPS/故障检出率 的吞吐-灵敏度折中 |

**注意**:gem5-MARVEL 是仿真注入(人为翻转比特),SDCShield 是真实硬件缺陷激发——两者不是同一测量对象;对比轴应放在"实验设计方法论"(抽样规模、失效分类、结构覆盖、验证手段)而非绝对数值。

## 9. 局限与坑

1. **无开源 URL**:论文全文未给出代码仓库/工件链接(已检索原文,无 toolkit/repository URL),精确复现需自行实现。
2. **gem5 版本未给精确号**:只说"latest version"(引 v20.0+ 论文与 2023-07 访问的 GitHub),复现时结构名/钩子点需自行对齐。
3. **ISA 对比结论被作者自己强限定**(IV-B 开头一段):同一微架构、同一 MiBench 负载,换配置排名可能反转——引用其"RISC-V 更脆弱"等结论时必须带限定语。
4. **与 RTL 级/真实束流的对比验证不在本文**:论文的验证只有 IV-F 的 sanity-check 程序(AVF=100% 覆盖);RTL 对比仅以"慢几个数量级"的定性论述(III-A、引 [63]–[65])。引用 [63][64](同团队 DSN'19 / TC'22)是把微架构级注入与**中子束实验**对比的先前工作,本文自身未做。用户关注的"与 RTL 级结果的对比验证"在本文原文中**不存在**。
5. 仅展示 5 个 CPU 结构;声称支持的 TLB/ROB/重命名单元等无数据。
6. Fig.14/16/17 的精确数值依赖图内标注(本文本已尽力按图标注转录,但条形图非表格数据,微小误差可能存在);Fig.9–11 范围值是正文文字给出的,可信度高。
7. 永久故障实验只覆盖 L1I/L1D 两个结构,未覆盖 PRF。
8. MiBench 是嵌入式小负载,对服务器级 SDC 场景(如 SOSP23/Veritas 的生产负载画像)代表性有限;SDCShield 面向数据中心 CPU,引用其负载结论需谨慎。

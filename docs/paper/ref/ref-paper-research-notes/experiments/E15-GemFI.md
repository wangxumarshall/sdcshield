# GemFI: A Fault Injection Tool for Studying the Behavior of Applications on Unreliable Substrates(DSN 2014,色萨利大学 University of Thessaly + CERTH + 美国西北大学)

> 作者:Konstantinos Parasyris, Georgios Tziantzoulis, Christos D. Antonopoulos, Nikolaos Bellas
> DOI: 10.1109/DSN.2014.96(原文页码 622–629)
> 源文件:`docs/paper/ref/GemFI_A_Fault_Injection_Tool_for_Studying_the_Behavior_of_Applications_on_Unreliable_Substrates.pdf`

## 1. 研究问题与核心贡献(≤5 行)

基于 gem5 的周期精确全系统故障注入工具,用于研究硬件故障如何以架构级错误显现并影响应用正确性("不可靠衬底上的应用行为")。核心贡献:(1) 遵循 Yount & Siewiorek 1996 通用处理器故障模型,在流水线各阶段(fetch/decode/execute/commit/访存)注入;(2) 用户用输入文件+两个内建 intrinsic 描述故障,不改动 gem5 配置即可换故障模型;(3) DMTCP 进程级 checkpoint 实现战役快速前推(平均 64.5×加速);(4) NoW(工作站网络)并行战役框架;(5) 注入开销相对原生 gem5 仅 −0.1%~3.3%。

## 2. 实验方法(注入机制、故障模型、目标结构)

- **注入机制(用户重点:gem5 修改点)**:C++/Python 扩展 gem5。两个 API intrinsic:
  - `void fi_activate_inst(int id)`——编译为伪汇编指令,连续调用切换(开/关)该进程/线程的故障显现;线程获得数字 id;
  - `void fi_read_init_all()`——对仿真打 checkpoint;恢复时重置 GemFI 全部内部状态,使同一 checkpoint 可作为多次不同故障配置实验的起点。
  - 命令行给出故障描述文件,每行一个故障,含 4 属性:**Location**(核→核内模块→具体比特位)、**Thread**(按 fi_activate_inst 分配的 id 选择线程)、**Time**(相对 fi_activate_inst 里程碑,按已执行指令数或仿真 tick 数调度)、**Behavior**。
  - 内部实现:线程在仿真器内部以 PCB(进程控制块)地址唯一识别;激活注入的线程表示为 `ThreadEnabledFault` 对象,每核一个指针;上下文切换(检测 PCB 地址变化)时重设指针,避免每 tick 查哈希表。故障文件启动时解析,插入**5 条内部队列(对应 5 个流水线阶段)**;每个仿真 tick 对每个被服务指令扫描对应队列,命中即按 Behavior 破坏目标位置的值。
- **注入位置(Location)全集**:整数/浮点/特殊寄存器、取指阶段的被取指令、译码阶段读写寄存器选择、执行阶段指令结果、PC 地址、访存事务(load/store)。
- **Behavior 四种**:赋立即值;与用户常数 XOR;按位翻转(支持多比特=同模块注多故障);全位置 0/1。瞬态/永久用激活时长(tick 数或指令数)区分;执行阶段故障可连续 N 条指令持续注入。
- **故障模型**:验证实验用单事件翻转(SEU)模型,Location/Time/Behavior 均匀分布(原文明确声明:该分布未必代表真实故障,但足以评估仿真器;工具支持任意用户提供的现实故障模型)。
- **时点选择(用户重点)**:Time 相对 fi_activate_inst 里程碑;实验流程为——checkpoint(系统启动+应用初始化完成后)→ 恢复 → O3 模式仿真并注入 → 受影响指令 commit 或 squash 后**切换到 atomic 仿真**跑到底(正常结束或崩溃)→ 评估最终输出质量。注入时打印受影响的汇编指令,用于事后统计关联故障与结果。

## 3. 实验配置(模拟器版本/硬件、基准、参数)

- **gem5 版本**:原文未给精确版本号(只引 Binkert 2011 gem5 论文;2014 年论文,对应 gem5 早期版本,推测 3.x 之前)。**ISA:Alpha(实现+实验)**;声称也支持 x86(未测试)。FS 全系统模式、O3 周期精确 CPU、支持多线程应用。
- **验证平台配置**:单核 Alpha CPU + tournament 分支预测器 + L1 I-Cache + L1 D-Cache + 统一 L2(容量等参数原文未给出)。
- **应用清单(用户重点)**:6 个
  | 应用 | 领域 | 输入 |
  |---|---|---|
  | DCT | JPEG 压缩/解压内核 | 512×512 灰度图 |
  | Jacobi | 迭代求解(对角占优矩阵) | 64×64 |
  | Monte Carlo PI | 蒙特卡洛 π 估计 | 10⁵ 个随机点 |
  | Knapsack | 0-1 背包(遗传算法) | 24 物品,重量上限 500 |
  | Deblocking filter | AVS 视频解码内核 | 720×240 像素图 |
  | Canneal | PARSEC 基准(模拟退火布线) | 100 nets,每步最多 100 次交换 |
- **每实验注入数**:每个应用每个实验 **2501–2504 次**执行,按 Leveugle DATE'09 方法取 **99% 置信度、1% 误差边际**。
- **NoW 硬件**:27 台工作站,每台 4 核 Intel Xeon E5520 @2.27GHz、8GB RAM;每台同时跑 4 个实验。

## 4. 实验步骤(可操作流程,编号)

1. 在目标应用源码中插入 GemFI intrinsic(Listing 2 模式):`initialize_input_data(); fi_read_init_all(); fi_activate_inst(id); foo(); fi_activate_inst(id);`——即初始化后开 checkpoint,再用一对 activate 划定注入使能窗口。
2. 交叉编译应用,放入 gem5 的磁盘镜像。
3. 跑一次到 fi_activate_inst 激活点(含 OS 启动+应用初始化),用 DMTCP 对仿真器 Linux 进程打 checkpoint,存网络共享盘。
4. 编写故障配置文件(Listing 1 模式:`RegisterInjectedFault Inst:2457 Flip:21 Threadid:0 system.cpu1 occ:1 int 1`——第 2457 条指令时翻转 cpu1 整数寄存器 R1 的 bit21,激活 1 条指令,仅线程 0)。
5. 每台 NoW 工作站取 checkpoint 本地副本,从共享盘认领实验,从 checkpoint 状态启动、解析故障文件、O3 注入仿真。
6. 受影响指令 commit/squash 后切 atomic 模式跑至应用终止(正常或崩溃)。
7. 结果回传共享盘;重复 5–7 直到实验清空。
8. 按 5 类结果分类(见 §5);对照无故障运行逐类统计;可用打印的受影响汇编指令做事后归因(如对照 Alpha 指令格式 Table I 分析 fetch 阶段比特位置→结果)。
9. 性能对照:同一批应用在原生 gem5 与 GemFI(激活但不注入)各跑一遍,比较 fi_activate 窗口内的仿真时间。

## 5. 实验数据(关键数值 + 图表号)

**失效分类(用户重点——实为 5 类,非 4 类)**:crashed(未能成功终止)、non propagated(故障未显现为错误,如寄存器未被使用或被覆盖)、strictly correct(与无故障运行逐位一致)、correct(在应用相关容差内但不逐位一致)、SDC(正常终止但结果超出可接受范围)。容差定义:DCT 以 PSNR>30 为 correct(与未压缩输入比);Deblocking PSNR>80dB(与无故障运行比);PI 前两位小数正确;Jacobi 收敛到与金标逐位相同输出(迭代次数可不同);Canneal 降低布线总成本且芯片正确。Fig.4 给出 DCT 的 4 类图示(a 严格正确 b 宽松正确 c SDC d 质量损失差)。

**按注入位置的结果(Fig.5,每应用一图+末列汇总;正文文字数据)**:
- FP 寄存器:所有应用对 FP 寄存器故障韧性最高(仅少数寄存器在用+存数据不存控制状态);**Deblocking(无浮点操作)FP 注入 100% 严格正确**。
- 整数寄存器:崩溃率最高(编译器用其存全局/栈/帧指针、返回地址、循环迭代器、基地址);**DCT 和 Jacobi(多级循环嵌套+大量访存)崩溃率约为其他应用 2 倍**。
- Fetch 阶段(结合 Table I Alpha 指令格式分析):打中 unused bit → 永远严格正确;分支指令位移位未触发分支 → 统计不变、严格正确;访存指令位移/Ra 位改变 → 高概率崩溃;opcode/function 变为未实现 → **总是以非法指令终止**。
- Decode 阶段:load/store 基址寄存器选择错误 → 通常段错误;**PI 的 decode 故障崩溃率约为其他应用一半**(PI 几乎不访存);decode 错误通常导致 SDC(操作数换了输入);correct 仅出现在改了被 squash 指令或算法固有韧性。
- Execute 阶段:改访存地址计算 → 段错误;**Knapsack(重用数组/指针)42% 崩溃;PI 几乎零崩溃**;正确结果源于掩蔽或仅影响数据低位。
- Load/store 数据值:**78% 结果正确**(高韧性;崩溃多因改了返回地址的存取)。
- PC:几乎总是致命;正确仅在 PC 被改成邻近地址(小前跳/回跳)的少数情况。

**注入时点与结果的相关性(Fig.6,横轴=归一化注入时点,纵轴=各类结果占比;3 个 campaign)**:
- PI:时点与结果不相关(每次迭代同质)。
- Knapsack:越晚注入越可能 acceptable(不收敛的损坏数据会被后续适应度函数迭代丢弃)。
- Jacobi:早期注入偏严格正确;越晚注入 correct 越多(对角占优保证收敛,错误只改变所需迭代数)。

**性能(Fig.7、Fig.8)**:
- GemFI vs 原生 gem5 开销:**−0.1% 至 3.3%**(PI 的 −0.1% 声明不具统计显著性;O3 全程仿真属最坏情形)。
- checkpoint 快进:**3×–244× 加速(平均 64.5×)**。
- 27 台 NoW(每台 4 实验):相对单机+快进再 **约 108×** 加速。

## 6. 实验结论(编号列出)

1. 故障位置与应用特征强相关:FP 寄存器最安全(低活跃+纯数据),整数寄存器最致命(控制流+指针+长活跃期)。
2. 访存数据通路的故障大部分被应用固有韧性吸收(load/store 数据 78% 正确)——**数据值损坏比地址损坏温和得多**。
3. 译码/执行阶段的操作数类错误多表现为 SDC;地址/PC 类错误多表现为崩溃。
4. 注入时点的影响完全由算法结构决定:迭代同质算法(PI)不敏感;遗传/退火类算法后期注入更易被吸收;收敛类算法(Jacobi)早注入反而多严格正确。
5. 工程结论:GemFI 开销 ≤3.3%,checkpoint+NoW 使大规模 campaign 可行。

## 7. 复现要点(ARM64/gem5 复现最小版本)

### 可直接复用
- **5 类失效分类法**(crashed / non-propagated / strictly-correct / correct / SDC)及**应用相关容差**定义(PSNR 阈值、小数位、逐位一致)——比 3 分类(Masked/SDC/Crash)更细,SDCShield 的"金标比对"可借其 correct vs strictly-correct 分层:zstd19 等**逐位 memcmp 类测试=strictly correct 口径**,数值类可另设容差档。
- **99% 置信/1% 误差→2501–2504 次运行**的样本量公式(Leveugle):比 gem5-MARVEL 的 95%/3%/1000 次更严一档,SDCShield 大规模激发实验(可在真实硬件上快速重复)可直接采用该口径提升统计说服力。
- **注入时点 × 结果相关性实验设计**(Fig.6):把注入时点归一化到应用执行时间轴上分桶——SDCShield 类比:把激发负载按"冷启动/warm 后/收尾"分段测量检出率。
- **PCB/线程识别 + 按流水线阶段分队列**的注入器结构设计、`fi_activate_inst` 划定注入窗口的 API 模式——若 SDCShield 团队要在 gem5 上建 ARM64 注入复现环境,这是最小改动模板(两个 intrinsic + 一个故障文件)。
- **DMTCP 进程级 checkpoint** 替代 gem5 原生 checkpoint(规避 O3↔atomic 切换的流水线冲刷失真,或 Ruby MOESI 的高开销)——现成开源工具。
- 无故障对照验证法:GemFI 不注入时与原生 gem5 输出逐位一致(§IV-A)——任何自制注入器都应做这一步。

### 需替代/不可行
- **Alpha ISA 实验不可复现**(gem5 早已弃用 Alpha FS;新版 gem5 移除了 Alpha 全系统支持)——需换 Arm64/x86 重做;GemFI 声称 ISA 无关,但 Five 队列挂在流水线阶段上,O3 实现已大幅重构,旧补丁不能直接套在新 gem5 上。
- **论文无开源 URL、无精确 gem5 版本**;社区有若干 GemFI 复刻(如 UCSD 的 gem5-fi 等衍生),但非官方工件。
- 真实硬件(SDCShield)无法按流水线阶段注入;其"阶段→失效类型"映射(fetch/decode→崩溃或 SDC;数据值→高容忍)只能作为**激发负载设计的定性指南**(多制造操作数级数据流压力,少依赖纯控制流错误)。
- 1996 Yount & Siewiorek 故障模型偏行为级,不建模门级/时序故障。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | 论文基线 | SDCShield 对应 |
|---|---|---|
| 注入开销 | ≤3.3%(Fig.7) | SDCShield 激发负载的开销(相对基线运行的吞吐损失)可同口径报告 |
| 样本量口径 | 99% 置信/1% 误差/2501+ 次(Fig.5 实验) | 激发实验重复次数设计 |
| 数据值故障容忍度 | load/store 数据故障 78% 正确(Fig.5) | 真实硅上数据通路缺陷的"不触发"率——SDCShield 用更敏感能力(如 NaN/尾数位检测)压低漏检率即为改进证据 |
| 失效分类粒度 | 5 类(含 non-propagated) | SDCShield 报告可细分"检测到/未检测到/崩溃/挂起" |
| 时点相关性 | Fig.6 三模式 | 激发实验分阶段(冷/热)采样设计 |
| 战役加速 | checkpoint 平均 64.5×;NoW 108×(Fig.8) | SDCShield fork 并行 + checkpoint 式预热复用思路对照 |

GemFI 是 2014 年工具,作为**数值基线**价值有限(Alpha/老 gem5),作为**方法论模板**(故障文件格式、窗口 API、分类学、时点分析)价值高。

## 9. 局限与坑

1. **实验 ISA 是 Alpha**,gem5 当年对 Alpha 支持最成熟才选它——今天复现必须换 ISA,全部 Fig.5/6 数值不能直接迁移。
2. **无精确 gem5 版本号、无开源 URL**(全文检索无仓库链接);DMTCP+gem5 的组合在现代 gem5(v20+)上需要重做适配。
3. 注入分布是均匀分布,作者自认"未必代表真实故障作用方式"(§IV-B1)——其绝对百分比不可当真实错误率引用。
4. 单核 CPU 配置;L1/L2 容量、ROB 等参数原文未给出(Table 无),无法精确重建仿真配置。
5. Fig.5/6 的多数结论只有文字描述+图示,无数值表格;除 78%(load/store)、42%(Knapsack execute 崩溃)、100%(Deblocking FP)等正文数字外,精确占比读不到。
6. "correct"类的容差阈值是人为定的(PSNR 30/80dB 等),换阈值会改变 correct/SDC 边界——复现时必须显式声明容差。
7. 故障模型(Yount & Siewiorek 1996)只覆盖处理器内部,不含互连/IO(作者在结论中列为未来工作)。
8. 用户提问中的"四类失效分类"实为**五类**(crashed/non-propagated/strictly correct/correct/SDC)——引用时注意更正。

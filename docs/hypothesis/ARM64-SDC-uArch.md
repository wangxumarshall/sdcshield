# ARM64架构静默数据损坏的微架构敏感性：科学假设与多层次实证验证框架

## 摘要

随着晶体管工艺迈入3纳米及以下节点，制造边缘缺陷（Marginal Defects）、偏置温度不稳定性（BTI）与热载流子注入（HCI）引发的老化退化，以及宇宙射线辐射导致的瞬态故障，正使静默数据损坏（Silent Data Corruption, SDC）成为超大规模数据中心计算完整性面临的最严峻挑战。工业界量化数据表明，SDC发生率已达每千颗处理器芯片一例¹，远超传统宇宙射线软错误基线。本研究从计算机体系结构第一性原理出发，系统性解构ARM64（RISC）与x86-64（CISC）在六大核心维度——前端指令编码空间、中枢寄存器重命名与物理寄存器堆、后端可伸缩向量计算引擎、访存子系统与内存一致性模型、系统级RAS毒化生命周期、以及异构互连协议边界——的本质差异如何以非线性方式放大或掩蔽底层物理缺陷的破坏力。基于此，本文提出六大具有完备微架构机理支撑的科学假设，构建统一的跨层漏洞因子衰减数学模型，并整合预硅周期精确仿真（gem5-MARVEL/GeFIN/AVGI）、后硅代理模糊测试（SiliFuzz）、非一致性缺陷检测（ITHICA）及硬件在环智能化功能测试（Harpocrates++）的多层次实证验证框架，旨在为下一代高可靠服务器处理器微架构设计提供坚实的理论基石。

---

## 一、导言：超大规模计算时代的硅层可靠性危机

随着摩尔定律逼近物理极限、晶体管尺寸不断微缩以及三维异构封装技术的广泛应用，现代超大规模数据中心正面临一场由底层硅硬件缺陷引发的严重可靠性危机。晶体管在制造和生命周期内产生的边缘缺陷、老化磨损现象（如偏置温度不稳定性BTI与热载流子注入HCI），以及宇宙射线引发的瞬态故障正在急剧增加²。当微架构层面的状态翻转未被硬件的错误检测机制捕获，亦未引发导致系统崩溃（Crash）或挂起（Hang）的显式信号，而是悄无声息地改变计算结果并传播至软件层时，便引发了静默数据损坏¹。

近年来，包括Meta、Google和Alibaba在内的超大规模云服务提供商的运维数据揭示了SDC威胁的严峻程度。Meta公司在其数百万台服务器集群中部署的PinDrop连续高频测试框架发现：在受测机器的完整生命周期中，高达0.035%的机器至少经历过一次SDC；超过70%的故障机器在长达数年的持续测试中表现出显著的故障驻留性与持久性；且每一季度测试都会发现平均0.0024%的新增故障机器，彻底推翻了芯片"出厂即安全"的传统假设³。在大语言模型（LLM）训练场景中，SDC导致的梯度毒化、NaN突变及模型收敛停滞，在Meta的Llama 3训练及Google的Gemini训练周期中，造成了多达1.4%的意外GPU训练中断与数以百万美元计的算力浪费⁴。在解压缩算法中，算术逻辑单元（ALU）的极微小延迟可导致非零文件被误判为零字节¹；在浮点密集运算中，本应输出26854的乘加运算在受缺陷影响的物理核心上输出328094⁵。

在此背景下，探讨主导全球数据中心算力基础设施的两大核心指令集架构——基于CISC的x86与基于RISC的ARM64——在面对底层微架构故障时的SDC敏感性差异，已成为当前学术界与工业界高度关注的前沿课题。本文以体系结构第一性原理为基础，深入剖析ARM64与x86在静默数据损坏面貌上的本质分野，构建多维度的理论与验证框架。

---

## 二、理论基础：架构脆弱性分析与跨层漏洞衰减模型

### 2.1 架构脆弱性因子与ACE生命周期模型

在微架构可靠性研究中，量化特定硬件结构对瞬态故障或边缘缺陷的敏感度是所有架构级优化的核心前提。架构脆弱性因子（Architectural Vulnerability Factor, AVF）被严格定义为：硬件结构中发生的物理比特翻转最终演变为用户可见错误的概率⁶。

ACE分析将处理器微架构结构（如重排序缓冲区ROB、发射队列、物理寄存器堆PRF）中每个比特的生命周期划分为ACE周期与un-ACE周期。若一个比特在某一周期内的状态翻转会导致程序最终的架构状态（Architectural State）发生错误，则该比特处于ACE状态；反之，若状态翻转被系统机制掩蔽，则处于un-ACE状态。导致un-ACE状态的典型微架构行为包括：错误路径指令被分支预测冲刷（Squash）、死代码消除、NOP执行、谓词判断为假的条件指令、以及被后续写入覆盖的过期数据⁶。

根据利特尔法则（Little's Law），包含 $N$ 个存储比特的微架构组件在总执行时间 $T$ 内的AVF为⁶：

$$\mathrm{AVF} = \frac{\sum_{i=1}^{N} t_{\mathrm{ACE},i}}{N \times T}$$

该公式揭示：决定结构脆弱性的并非仅其物理面积，而是微架构状态的"有效驻留时间"。任何将ACE周期转化为un-ACE周期的行为均能有效降低AVF。例如，缓存中的周期性主动刷新可强制驱逐脏数据，以极小性能代价换取AVF的显著下降⁷。

### 2.2 统一形式化概率积分模型

为定量评估特定指令集架构与工作负载的SDC风险交互，本文将系统级SDC失效率 $\lambda_{\mathrm{SDC}}$ 建模为多维条件概率张量的连续积分：

$$\lambda_{\mathrm{SDC}} = \sum_{s \in \mathcal{S}} \int P_{\mathrm{fault}}(s) \cdot \bigl[1 - R_{\mathrm{TVF}}(s)\bigr] \cdot \bigl[1 - R_{\mathrm{RAS}}(s)\bigr] \cdot \mathrm{AVF}(s) \cdot \mathrm{PVF}(s) \, ds$$

其中：$\mathcal{S}$ 代表处理器内部所有微架构硬件结构集合（解码器、寄存器重命名表RAT、重排序缓冲区ROB、物理寄存器堆PRF、访存队列LSQ及谓词计算引擎等）；$P_{\mathrm{fault}}(s)$ 为特定硬件单元捕获物理扰动的本征概率⁸；$R_{\mathrm{TVF}}$ 为时序漏洞因子（物理扰动必须发生在触发器的建立/保持时间窗口内）；$R_{\mathrm{RAS}}$ 为硬件RAS拦截率；$\mathrm{AVF}(s)$ 为体系结构漏洞因子；$\mathrm{PVF}(s)$ 为程序漏洞因子（受污染的架构状态必须被后续数据流真实消费并输出至持久化存储或网络边界⁹）。

AVGI方法论的研究指出，不同微架构对瞬态故障的吸收与转化表现出截然不同的指令集展现模型（ISA Manifestation Models, IMM）¹⁰：若底层故障使微架构抛出非法指令或越界访存异常，故障被操作系统捕获为崩溃；若故障仅篡改合法操作数而不触发规则校验，则顺畅通过AVF过滤器，将压力转移至应用层PVF。

### 2.3 错误分类学与"超级线性"脆弱性异象

根据硬件RAS机制的覆盖率与有效性，系统层的错误表现被严格划分为三类¹¹：

| 错误类别 | 物理与微架构触发条件 | 系统表现 |
|:---------|:-------------------|:---------|
| **良性掩蔽** | 故障发生在un-ACE周期，或在逻辑门传播中被电气掩蔽 | 零架构影响，系统继续正确执行 |
| **检测到不可恢复（DUE）** | 故障发生在ACE位，被检测机制（如奇偶校验）成功捕获 | 触发机器检查异常，操作系统崩溃或进程挂起 |
| **静默数据损坏（SDC）** | 故障发生在无RAS保护的ACE计算逻辑，或逃逸容错编码 | 无显式硬件错误信号，错误结果被提交并污染数据 |

针对多级缓存的周期级仿真研究揭示了"缓存SER异象"¹²：当L2写回缓存容量加倍时，脏数据在缓存中的平均驻留时间呈指数级增加，导致每个缓存标签和数据阵列的DUE-AVF发生"超级线性"增长（高达原有预测的数倍）。这彻底打破了仅依靠物理面积评估错误率的陈旧观念，证明了对微架构状态随时间演变进行动态深度分析的不可或缺性。

此外，当前数据中心SDC多由制造边缘缺陷引起：极微小的半导体工艺偏差（如多重曝光引起的细微线宽缩减）、接触孔阻抗异常或过孔偏移引发的边缘时间延迟²。这类缺陷具有极强的"微架构上下文依赖性"（Microarchitectural Context Dependency）——其触发需要特定的局部温度升高、动态电压降（Voltage Droop），更需要高度特定的微架构执行序列⁵。Meta团队的实证数据显示，同一批服务器在15天带生产负载的Ripple Test中发现的SDC芯片，与6个月非生产Fleet Test的重合度仅70%¹。

---

## 三、科学假设体系

本文提出如下核心命题：**在现代微处理器中，SDC的微架构敏感性绝非由晶体管数量或原始晶圆缺陷密度孤立决定，而是由指令集架构（ISA）的高级语义规则与底层微架构乱序执行引擎的资源调度机制发生"共振"所深刻塑造。** ISA的特定语义（寄存器数量、内存模型、谓词控制等）在逻辑层面约束编译器，而编译器约束强制微架构设计者采用特定硬件结构来实现该语义。这种跨越软件抽象边界的映射关系，在不同维度上以非线性方式动态放大或掩蔽微观物理缺陷的破坏力。

以下沿微架构流水线自然序，从六个维度对此命题进行严密论证。

### 3.1 前端维度：定长编码的"寄存器域满射"与"汉明近邻漂移"效应

**假设一：ARM64定长32位指令编码的正交闭包特性，使得前端软错误更倾向于蜕变为语法合法但语义漂移的错误指令，从而将SDC防守压力推给上层软件。**

在前端取指与解码阶段，发生于L1指令缓存（L1I）与解码器的单比特软错误，在不同指令集下的宏观表现存在本质分野。

**x86-64的"变长崩溃屏蔽效应"。** x86-64指令长度跨越1至15字节，其边界解析高度依赖前缀序列（Legacy/REX/VEX/EVEX）、操作码、ModR/M及SIB字节的顺序状态跳转¹³。任何落入指令长度解析位的单比特翻转，会导致指令长度解码器（ILD）发生灾难性的指令边界不对齐（Stream Misalignment）。一旦边界丢失，后续取指流中的所有字节被错位解析，大量非法操作码密集涌入乱序流水线，处理器在数个时钟周期内必然触发#UD（非法操作码）或#GP（通用保护错误）异常。这种机制实质上将潜在SDC强制转换为可被操作系统捕获的崩溃事件。

**ARM64的"合法化屏蔽效应"。** AArch64严格采用32位定长编码，解除了指令边界同步对操作码或前缀的依赖。其SDC敏感性根源于两大编码空间拓扑特征：

**(1) 寄存器寻址域的全满射（Bijective Register Fields）。** AArch64采用5位显式编码寄存器索引（$2^5=32$），精确且无冗余地映射到X0-X30及XZR/SP¹⁴。一条典型的算术指令包含3至4个寄存器字段。无论单比特故障发生在任何5位字段的任何位置，解码器生成的微操作都会指向一个合法的物理架构寄存器，不触发任何异常。

**(2) 算术子操作码的汉明近邻漂移。** 在主要算术与逻辑指令簇中，关键操作码存在极低的汉明距离（Hamming Distance）。例如，移位寄存器形式的ADD与SUB仅在bit[30]存在1比特差异；64位LDR与STR仅在bit[22]存在1比特差异。一旦这些脆弱的单比特翻转，指令语义被静默反转——加法变减法、加载变存储——而硬件流水线毫无察觉。

需要指出的是，ARM64的32位定长编码确实存在大量未分配空间（Unallocated Space）¹⁵，当比特翻转落入这些区域时，解码器将直接抛出未定义指令异常（UNDEFINED Exception），这构成了一种天然的SDC阻断机制。然而，在寄存器字段与低汉明距离操作码区域发生的翻转，将绕过此防线。ITHICA框架的功能测试研究证实，此类在相同输入下产生不同架构输出的不一致错误，正是最难被传统测试用例检测的深层缺陷¹⁶。

### 3.2 中枢维度：物理寄存器堆驻留脆弱性与重命名映射表假依赖穿透

**假设二：ARM64的RISC Load-Store语义与宽阔架构寄存器空间，在拉长PRF中数据ACE驻留时间的同时，加剧了重命名控制平面关键时序逻辑的复杂度，使其在遭遇边缘缺陷时成为SDC的高危源头。**

#### 3.2.1 Load-Store架构引发的寄存器生命周期膨胀

物理寄存器堆（Physical Register File, PRF）是乱序执行超标量处理器中容纳飞行中指令运算结果的核心结构。ARM64在AArch64状态下定义了31个通用64位架构寄存器（X0-X30），以及32个SIMD/FP寄存器¹⁴，与x86-64传统的16个通用架构寄存器形成鲜明对比。作为纯粹的RISC架构，ARM64严格遵循Load-Store范式：所有ALU操作数必须预先加载到寄存器中；而x86允许CISC指令在一条指令内同时完成内存读取、算术运算和内存写回¹³。

这种ISA语义差异在微架构层面引发"共振"：为维持运算流的连贯性，ARM64编译器和微架构映射逻辑将大量中间计算节点长久保留在架构寄存器中，使高价值计算数据在PRF中的活动驻留时间（ACE时间）被大幅拉长。在微架构周期级别，物理寄存器中数据元素的完整生命周期严格包含：分配（Allocate）→ 执行单元写入（Write）→ 消费者读取（Read）→ 指令提交与释放（Commit/Free）⁷。若在"写入完成"到"最后一次消费者读取"的有效窗口期内，PRF存储单元因局部电压降发生比特翻转，该错误将作为ACE状态直接传递给消费者指令，引发致命SDC。

gem5-MARVEL框架的跨ISA故障注入研究为此提供了定量实证¹⁷：

| 指令集架构 | 物理寄存器堆(PRF) AVF范围 | L1指令缓存 AVF范围 |
|:----------|:----------------------|:-----------------|
| ARM64     | 6.0% – 14.0%          | 0.3% – 9.9%      |
| x86-64    | 4.7% – 13.2%          | 0.3% – 4.6%      |
| RISC-V 64 | 5.1% – 20.8%          | 0.2% – 5.7%      |

RISC类架构（ARM64与RISC-V）的PRF易损性系统性高于x86-64¹⁷。同样的硅片制造缺陷若落在寄存器堆单元，ARM64因数据驻留时间延长，产生宏观可见SDC的概率高于x86。

然而，此处存在一个关键的微架构辩证：虽然ARM64的ISA语义上提供了更宽裕的架构寄存器以减少寄存器溢出（Register Spill），降低了对主存Load/Store指令的依赖，但为在服务器级工作负载中实现与高端x86抗衡的IPC，现代ARM64处理器（如Neoverse架构）同样堆砌了海量的ROB、极宽的发射队列及深度多级预测器。因此，ARM64后端的总体ACE周期总量并不必然低于x86——RISC的"指令膨胀"（Instruction Bloat）直接增加了微架构队列中的指令占用条目数和流水线周转时间¹¹。

#### 3.2.2 重命名控制平面的假依赖穿透

真正的SDC敏感源不仅在于PRF的物理面积，更在于重命名控制平面（Rename Control Plane）的多路复用网络及其极限时序约束。ARM64庞大的架构寄存器基数（31个通用 + 32个SIMD/FP）极大加剧了前端寄存器别名表（RAT/Speculative Rename Table）的逻辑维度。在支撑8路宽发射的高性能核心中，重命名逻辑须在单一时钟周期内完成发射包中所有指令的源寄存器依赖检查并分配物理寄存器映射。ARM64的5位寄存器寻址较x86的4位引入了更深的多路查找交叉开关（Crossbar MUX Network）和比较匹配线（CAM Matchlines）¹⁰。

这一控制逻辑处于微架构中时序最紧张的关键路径上。为满足极高时钟频率要求，设计厂商往往无法在多路分配器与检查点恢复栈上插入ECC或奇偶校验逻辑。一旦瞬态电压下陷或宇宙射线导致比较器门级电路出现毛刺，将发生"假依赖穿透"或"物理寄存器被盗"（Physical Register Stealing）。例如，重命名分配逻辑因状态机故障，错误地将同一物理寄存器 $P_k$ 同时赋予发射包中无关联的两条指令 $I_a$ 和 $I_b$。后端多个执行单元在无总线报警的情况下，使两条指令的计算结果在写回阶段发生竞争（Writeback Race），导致合法结果被静默覆盖。依赖该物理寄存器的后序指令将读取脏数据，引发不可逆SDC¹⁸。

针对乱序核心重命名逻辑缺陷的微架构模拟研究表明，RAT表项的静默翻转可能需经数百万时钟周期、甚至跨越多个上下文切换后，才因特定微架构状态被激活而溢出为架构可见错误¹⁸。ARM64由于寄存器池的深度，这种别名错误能够长期蛰伏（Latent），大幅增加其演变为SDC而非触发直接异常的风险。

### 3.3 后端维度：SVE/SVE2谓词掩码的SDC放大与掩蔽双重性

**假设三：ARM64的可伸缩向量扩展（SVE/SVE2）通过变长谓词控制机制，在特定数据稀疏场景下自动屏蔽底层ALU缺陷（降低局部AVF），同时又因谓词寄存器自身的比特腐蚀，将足以引发崩溃的控制流故障降维转化为大规模静默数据污染。**

#### 3.3.1 向量长度不可知模型的追溯屏障

SVE的ISA语义彻底打破了传统固定宽度SIMD范式，通过引入最高可达2048位的变长矢量寄存器（Z0-Z31）、16个独立控制谓词寄存器（P0-P15）及首次故障寄存器（FFR），实现了灵活的数据并行度¹⁹。与x86的AVX-512固定512位宽度不同，SVE采用"向量长度不可知"（Vector Length Agnostic, VLA）模型，允许芯片厂商在128至2048位间自由选择实现宽度²⁰。

VLA模型的一个重大工程后果是执行路径的非确定性拓扑展开。同一软件二进制在不同向量长度的ARM机器上运行时，微架构将其映射到向量ALU的轨迹截然不同²¹。在超大规模异构集群中，当正常的浮点舍入差异与真实的微小硬件边缘缺陷交织时，SDC溯源变得极其困难²²。

#### 3.3.2 谓词掩蔽的SDC过滤与放大机制

| SVE微架构特性 | SDC放大效应 | SDC掩蔽效应 |
|:------------|:-----------|:-----------|
| **细粒度谓词寄存器(P0-P15)** | 缺陷发生在谓词计算逻辑上→控制流单比特翻转(0→1)→激活异常通道→un-ACE废弃数据被强行注入架构状态 | 非活跃通道天然处于un-ACE状态→物理数据位翻转不影响最终提交的架构状态 |
| **合并谓词(/M) vs 清零谓词(/Z)** | 合并谓词下，寄存器旁路逻辑建立时间违例→旧历史数据被噪声覆盖→张量块污染 | 清零谓词硬件直接置零非活跃通道→物理'0'的强驱动可完全掩蔽信号衰减缺陷 |
| **推测性收集-散播访存与FFR** | LDFF1D跨页边界加载的MMU/缓存交互缺陷→无效越界访问未被FFR拦截→大规模内存污染 | 设计良好的微架构在首个故障页后自动终止后续通道加载→后续通道成为安全un-ACE区域 |

**SDC放大的灾难性路径。** 根据ARM架构规范的异常隐匿机制，被谓词掩码禁用的通道严禁向浮点状态寄存器（FPSR/FPCR）报告任何浮点异常²⁰。若物理故障导致谓词寄存器发生0→1的单比特翻转（掩码腐蚀），硬件将错误激活该通道，强制向量浮点单元吸纳非法内存垃圾数据参与计算。当该非法通道产生+Inf或NaN时，由于掩码激活逻辑的翻转与异常捕获时序存在微架构脱节，硬件将阻止异常标志位置位，操作系统无法触发SIGFPE中断阻断进程⁴。

在SVE加速矩阵乘法应用中，若后端浮点乘加单元（FMA/MAC）的指数位或符号位计算网表中存在制造缺陷，该缺陷会在单一时钟周期内被SVE极宽流水线横向"放大"，一次性污染数十个浮点张量元素。这种静默数据突变通过神经网络反向传播迅速累积，导致LLM训练中的梯度发散、不可逆的损失函数尖峰，甚至使数百万美元的训练集群陷入数值崩溃²³。

**SDC掩蔽的容错潜力。** 与之形成反差的是，SVE独有的逐通道谓词控制使得在字符串处理、稀疏矩阵过滤等负载中存在大量被硬件显式关闭的闲置通道。这些被闲置的晶体管区域自然退化为un-ACE区域。这与传统强制全宽度计算的固定SIMD（如x86 AVX-512）相比，在特定场景下展现出通过架构语义自发压制底层硬件故障、降低整体SDC-AVF的特殊容错潜力²⁰。

**控制流降维效应。** 此处最具体系结构意义的现象是：传统x86架构下，实现同等程度的不合理内存越界或错误数据写入，通常需要分支预测器或程序计数器发生关键故障，而此类控制流故障有极高概率击中未映射内存页，迅速触发缺页异常转化为崩溃²⁴。ARM SVE的谓词机制却成功地将本能诱发崩溃的底层控制流级故障，"降维"收敛为隐蔽且极难被异常捕获的纯粹SDC。

### 3.4 访存维度：弱内存模型与推测执行的脆弱性窗口

**假设四：ARM64的弱内存一致性模型赋予微架构极大的乱序调度自由度，但同时显著扩大了瞬态硬件缺陷通过错误存储-加载转发感染推测执行架构状态的概率窗口；且内存屏障微操作的控制标记衰减可导致跨核数据静默解耦。**

#### 3.4.1 强弱一致性模型的访存脆弱性分野

在ISA语义层面，ARM64采用高度自由的弱内存一致性模型（Weakly Ordered Memory Model），而x86则受制于严格的总存储顺序（Total Store Ordering, TSO）²⁵。

| 架构特性 | ARM64 弱内存模型 | x86-64 强内存模型(TSO) |
|:---------|:---------------|:--------------------|
| 重排序自由度 | 极高（读-读、读-写、写-读、写-写均可重排） | 较低（仅允许写-读重排） |
| 缓冲队列特性 | 弹性调度，驻留周期长 | FIFO约束，快速清空或按序阻塞 |
| 内存屏障需求 | 频繁需要显式指令(DMB, DSB)同步 | 极少需要显式屏障(仅mfence等) |
| 推测执行深度 | 更深，LSQ重叠范围广 | 相对受限，较早收敛 |

弱一致性模型赋予ARM64处理器优异的能效比和单线程性能优化空间。然而，在ARM微架构的加载/存储队列（LSQ）中，内存请求可停留更长的时钟周期以等待最优总线时序和合并机会，使存储-加载转发路径上的脆弱性窗口被显著拉长²⁶。

#### 3.4.2 存储-加载转发的SDC污染路径

当后续加载指令发出时，LSQ并行搜索存储缓冲区中尚未提交到L1缓存的较早存储指令的物理地址²⁷。若发现地址匹配，CPU将直接旁路L1缓存，从存储缓冲区抓取推测的脏数据转发给加载指令以掩盖缓存延迟。

LSQ包含复杂的相联存储器（CAM）逻辑用于地址匹配。若此处存在电路老化（如BTI），可能导致匹配延迟；若地址比较逻辑或存储缓冲区数据阵列中存在边际泄漏电流引发的单比特翻转，将导致错误地址被匹配或错误数据被转发——此时数据尚未被L1的奇偶校验机制覆盖¹¹。推测出的错误脏数据将被后续长达数十条处于推测态的ALU指令消费，在微架构内形成脱离缓存ECC保护的巨大"脆弱性暗窗"（Vulnerability Dark Window）。

在x86的TSO限制下，这种转发漏洞往往因FIFO的快速清空而被及时约束；但在ARM架构下，跨越不同物理地址的无序转发机制使SDC向上传递到软件层的概率实质性增加²⁶。

#### 3.4.3 内存屏障微操作标记衰减引发的跨核数据解耦

在现代ARM微架构中，解码后的内存屏障并非普通算术微操作，而是转化为带有控制属性（Barrier_Type与Domain_Mask）的元数据项，驻留在保留站（RS）、ROB或LSQ中，充当乱序访存单元的定序锁²⁸。

假设在高度并发的无锁队列或自旋锁场景中，生产核在写入载荷数据（Payload）和标志位（Flag）之间插入了DMB ISH。若高能粒子或电压波动导致保留站中该条屏障微操作的有效位（Valid Bit）被清零，或屏障类型位发生状态衰减，乱序访存单元将解除定序封锁，将不可逾越的屏障视为NOP指令。生产核可能将Flag改变先行广播至全系统，而Payload更新仍滞留在本地Store Buffer中。消费核观测到Flag变化后立即读取Payload，从而读到陈旧的内存脏数据（Stale Data）。

这种故障模式极为隐蔽：单线程测试中程序展现100%正确性，无任何硬件报警；而多线程高并发业务中，它直接打破多副本原子性（Multi-Copy Atomicity），造成跨线程数据静默解耦²⁸。

此外，推测执行机制本身对SDC-AVF具有复杂的辩证关系。从防御角度看，沿错误分支路径推测执行的指令占据的ALU和寄存器资源，在管线冲刷时全部清零，等同于无意间拦截了SDC¹¹。然而，若SDC恰好发生在分支预测条件判断本身的比较器逻辑上，可能导致正确路径被丢弃，迫使程序状态机走入完全不可预测的错误逻辑图景——这种"控制流层面的SDC"在微架构可靠性分析中极具灾难性且难以复现。

### 3.5 系统维度：RAS错误限制架构与数据中毒逃逸

**假设五：ARM64以AMBA CHI互连协议与Firmware-First错误处理为核心的多级RAS机制，在常规层面保障了极高的系统可用性，但在3纳米时代高度边缘化的瞬态缺陷面前，暴露出"毒化数据标志位"在超长乱序推测窗口中逃逸的终极微观风险。**

#### 3.5.1 数据中毒与Firmware-First架构

现代ARM64服务器处理器（如Neoverse N2/V1/V3架构）在系统控制域引入了深度的RAS架构扩展²⁹。在底层微架构实现上，不仅体现为指令缓存的单比特错误检测奇偶校验和多级缓存的单纠错双检错（SECDED）ECC码，还建立了基于AMBA CHI（Coherent Hub Interface）互连协议的"数据毒化"（Poison Bit Propagation）全链路跟踪机制³⁰。

数据中毒的微架构工作原理如下³⁰：

1. 当缓存数据块发生不可纠正的多比特错误时，硬件不立即向操作系统抛出致命异常（避免造成整机数百个虚拟机同时瘫痪）；
2. 硬件将包含错误的数据块附加"毒素标识"（Poison Bit），以每64位数据分配1位Poison标志的粒度进行标记；
3. 受毒数据在CHI互联网络中自由移动——无论在L2/L3间驱逐还是一致性嗅探中跨集群传输，毒素状态标志伴随数据流线级传递；
4. 仅当处理器流水线真正"消费"该数据时才触发精确硬件异常。

ARM架构推崇"固件优先"（Firmware-First Error Handling, FFH）错误处理范式²⁹：当错误节点检测到不可纠正错误时，系统生成异步的故障处理中断（FHI）或错误恢复中断（ERI），路由至最高特权级（EL3, TrustZone安全监控器）中的底层固件处理。EL3固件解析微架构综合征（Syndrome），生成标准CPER（通用平台错误记录），通过SDEI（软件委派异常接口）安全通知操作系统内核实施隔离³¹。

#### 3.5.2 计算核心的SDC边界盲区

尽管RAS与数据中毒机制构成了精致的分布式容错系统，有效拦截了绝大多数源自存储体系和互联总线的故障，但暴露出微架构层面的致命SDC渗透盲区。

当前微体系结构设计中，数据中毒协议和ECC覆盖范围局限于存储抽象结构（各种SRAM和总线数据包）。然而，在处理器极其复杂的执行后端——深层组合逻辑、巨型浮点乘法器树、以及前述SVE谓词逻辑中——为追求极限时钟频率和面积能效，这些纯运算组合逻辑区域没有任何奇偶校验或ECC保护⁶。

当ALU产生错误的算术结果时，这个损坏的结果在被写回L1数据缓存之前已完成静默偏离。当已破损的数据抵达L1缓存时，缓存控制逻辑基于错误的原始数据为其生成完全匹配的ECC校验码并存入标签。对于整个RAS系统及CHI互联，这个被深度污染的数据披着合法的校验外衣，看起来完美无瑕。数据中毒协议对其完全失效⁶。

典型地，占据现代CPU面积最大的分支预测器阵列完全是纯un-ACE结构——即便内部发生海量晶体管故障，也会被后端检查机制拦截为预测失败并重定向流水线，不引发任何架构状态破坏（零SDC-AVF）。反之，ALU进位链或浮点乘加旁路转发网络上的极小面积制造缺陷，却能产生灾难性数据中毒事件⁵。

#### 3.5.3 毒化数据逃逸的时序竞赛

在现代多核互连设计中，AMBA CHI协议为维持数百核心间的高效缓存一致性，允许复杂的并发状态转移（如纯驱逐WriteEvict事务、多核并发缓存窥探干预）³²。若L1与L2间的网状互连总线物理连线上存在串扰噪声，或逻辑门存在老化降级，可能导致跟随数据缓存行同步传输的关键毒标志位在跨时钟域传输握手阶段丢失；或者消费者核心因微架构解码逻辑边缘缺陷，在推测性数据提取时未能正确解析并拦截毒化标志，从而将损坏的脏数据直接拉入ALU消费。在这种极端时序竞赛中，系统未能及时触发架构定义的错误同步屏障（Error Synchronization Barrier, ESB指令）³⁰。

当发生上述"毒数据逃逸"时，原本被定性为可控DUE事件的错误，将彻底跨越RAS安全防护网，异化为向整个数据中心蔓延的系统级SDC灾难。

### 3.6 互联维度：异构互连协议边界的毒化元数据衰减

**假设六：在异构Chiplet架构与跨协议桥接的复杂SoC集成中，数据中毒标志在AMBA CHI域向CXL/PCIe域转换时面临元数据物理截断的风险，构成从UCE到全局SDC的降维逃逸通道。**

ARM作为IP授权生态，其SoC芯片往往集成来自不同厂商的第三方IP（如非ARM原生的PCIe根复合体、CXL桥接器及定制DMA引擎）。当带有Poison标志的脏数据流经跨协议桥接器（Protocol Translation Bridge，例如从AMBA CHI域向CXL 3.0或外设PCIe域转换）时，若桥接控制器的RTL状态机未能忠实、对齐地映射并传递Poison标志位，该元数据将被物理截断³³。同样，若独立内存控制器的ECC逻辑在纠正失败后仅记录本地错误寄存器，而未正确驱动CHI总线上的Poison信号，数据包就会被请求端视为健康数据接收。

最终，底层不可纠正错误（UCE）在跨越互连协议边界后，被彻底降维并伪装成正常数据流读入L1 Cache，导致全局SDC逃逸。这解释了为何尽管部署了最先进的硬件级容错芯片，涉及复杂计算拓扑的大型神经网络训练仍会无预警地出现梯度偏离²³。

---

## 四、多层次融合验证框架

科学的微架构敏感性假设必须经得起严格的实验物理验证。传统单一软件层故障注入模型已不足以评估现代复杂芯片的SDC风险³⁴。本文构建了一套融合预硅（Pre-Silicon）周期精确仿真与后硅（Post-Silicon）物理缺陷捕捉的多层次实证验证框架，从指令集抽象到物理硅片级别实现全链路覆盖。

### 4.1 预硅阶段：AVF热力映射与周期精确故障注入

| 验证层级 | 目标验证假设 | 核心工具与方法 | 关键度量指标 |
|:---------|:-----------|:------------|:-----------|
| **L1: 指令集抽象层** | 假设一：前端编码满射与汉明漂移 | LLVM-FI动态二进制插桩，对32位编码空间注入随机SBU；引入ITHICA方法学通过线程内指令输出比对筛选不一致错误¹⁶ | SDC/(SDC+Crash)比率；ARM64寄存器字段单比特翻转诱发静默非法逻辑的统计概率 |
| **L2: 微架构状态机** | 假设二至四：重命名假依赖、PRF驻留、屏障标记丢失 | gem5-MARVEL¹⁷与AVGI框架¹⁰，在周期精确模拟器中对RAT比较网络、LSQ屏障标记及谓词寄存器施加瞬态翻转 | 利用Litmus并发测试集观测多核访存一致性违规率；利用AVGI的ISA展现模型快速评估结构体AVF |
| **L3: 硬件RTL级** | 全部假设的跨层交叉验证 | FireSim (FPGA原型)及Zebu硬件加速器，在综合后门级网表层面沿时钟树与关键时序路径对触发器注入信号级毛刺³⁵ | 断言逃逸率、故障激活潜伏周期（Latent Cycles）；非ECC覆盖区屏蔽毛刺失败的确切概率 |

预硅阶段的核心方法论：在模拟器中配置符合ARMv8/v9架构规范的O3CPU乱序执行模型，集成Ruby内存系统引擎以精确仿真AMBA CHI一致性协议及多级缓存层次的交互握手¹⁷。系统化对SVE谓词寄存器阵列（P0-P15）、深层乱序依赖的PRF、以及负责存储-加载转发的Store Buffer进行海量统计级位翻转注入。通过比对SPEC CPU基准测试或LLM张量核（Tensor Kernels）的仿真运行输出，精确提取不同负载下的ACE与un-ACE周期微架构时间边界。在此基础上，绘制全芯片微架构结构的**时空AVF热力图**，识别那些对ISA语义高度敏感、极易放大SDC破坏效应的高危逻辑重灾区。

AVGI方法论在此过程中提供了关键的计算加速：相较于传统逐比特逐周期的SFI方法，AVGI通过微架构驱动的快速漏洞评估实现了数量级的加速¹⁰，使大规模跨ISA比较评测在计算上可行。

### 4.2 后硅阶段：非一致性缺陷捕捉与代理模糊测试

#### 4.2.1 ITHICA：非一致性错误的震撼性发现

斯坦福大学与Google联合提出的ITHICA（Intra-Thread Instruction Checking Approach）研究框架，利用超过3000台超大规模CPU服务器的真实硅片海量运行数据，彻底推翻了传统验证方法的基础假设¹⁶。

ITHICA的核心科学洞见在于：当前最易逃逸现代半导体晶圆测试的最险恶制造缺陷，引发的是**"非一致性错误"（Inconsistent Errors）**¹⁶。在同一CPU核心、同一执行线程内，给定完全相同的架构级输入操作数，连续两次执行同一条汇编指令，微架构可能输出截然不同的错误计算结果。

这种非一致性源于微架构深度的实时环境依赖性：现代高密度CPU核心的计算逻辑受纳秒级热分布波动、动态电压与频率调整（DVFS）引起的瞬态压降、时钟抖动，甚至相邻乱序执行单元中并发指令数据翻转引发的微弱电磁串扰所主导。在这些不可预测的叠加效应下，一个处于亚稳态边缘的晶体管，可能在前一微秒正常输出逻辑1，在下一微秒却因0.01伏特压降而判定为逻辑0。

这种发现彻底打破了几乎所有依赖重复执行的传统功能测试和单一模型故障注入框架的理论基石。ITHICA通过其开创性的"单线程内指令复制与动态输出比较"机制，在真实数据中心机队中成功比原生验证测试**多捕获39%的致命缺陷服务器**¹⁶。

#### 4.2.2 SiliFuzz：代理模糊测试

Google主导的SiliFuzz项目彻底放弃了传统寻找设计逻辑Bug的思路³⁶。其核心方法是：通过动态捕获微架构的实时执行快照（Execution Snapshot），利用代理模拟器预取期望的微架构态作为模糊测试种子，在海量真实硬件芯片上进行无休止的循环执行与状态比对。其目标直指那些由晶体管磨损老化或微小杂质引发的完全不可预测的电气边缘缺陷。

ARM64架构的规整指令编码在此过程中展现出独特优势：相较于x86错综复杂的指令拓扑，针对ARM A64指令集的模糊测试能更高效地生成抵达特定微架构深度（如L1 D-Cache边界、TLB和SVE掩蔽计算路径）的SimPoint片段³⁶。

### 4.3 硬件在环智能化功能测试

以Harpocrates及其进阶框架Harpocrates++为代表的方法学开创了硬件在环（Hardware-in-the-Loop）的程序生成模式³⁷。该框架通过微架构引擎的迭代反馈，自动变异并生成能够最大限度激活目标微架构（如RAT、LSU队列深度状态）边缘条件的简短功能测试集。这些测试程序经过精心编排，在极短执行窗口内以高吞吐量触发指令间的极限冒险与资源冲突，逼迫因硅老化或工艺边际缺陷产生的瞬态逻辑错误显现为确定的SDC。

### 4.4 物理芯片与集群级实证

| 验证层级 | 目标假设 | 核心工具与方法 | 关键度量指标 |
|:---------|:--------|:------------|:-----------|
| **L4: 物理芯片与集群** | 全部假设的物理验证与互连毒化逃逸捕获 | PinDrop连续测试集群数据日志³，结合重离子束流辐射、激光故障注入（LFI）及电压裕量压降干扰；真实服务器运行AI训练级压力负载 | 物理截面FIT概率；长期长尾故障持久性演进轨迹；内存读写事务Poison Bit跨总线丢失率 |

后硅阶段的关键策略：将预硅阶段筛选出的高AVF敏感微架构操作指令序列（特别是频繁交织使用SVE合并谓词/M、涉及复杂跨页推测加载异常以及密集触发CHI总线缓存窥探事务的高危指令组合），直接转化为SiliFuzz硬件代理引擎的高密度测试向量。同时植入ITHICA框架的"单线程内指令复制与结果自校验"探测逻辑¹⁶。通过在数千台真实ARM64服务器集群中7×24小时持久计算压力测试，不仅能检测系统上线初期（$t_0$）的制造冷缺陷，更能长期追踪因温度、电压应力及HCI/BTI老化效应作用下渐渐浮现的（$t_0 + \Delta t$）早夭边缘缺陷（ELF）²。

此套多层次融合验证体系的部署，将达成对现代算力底座完全非一致性SDC发生频谱的无死角全量覆盖与阻断。

---

## 五、结论与未来工作方向

### 5.1 核心结论

在后摩尔时代与通用人工智能大模型爆发的双重引擎驱动下，微处理器核心算力密度的极限扩张与底层硅片物理特性的自然收敛产生了前所未有的剧烈冲突。本文以体系结构第一性原理为准绳，通过六大维度的系统性研究，揭示了以下核心结论：

**(1) 前端编码空间的拓扑密度决定SDC拦截效率。** ARM64的32位定长编码虽通过未分配空间提供了天然的异常拦截机制，但寄存器域全满射与操作码低汉明距离特性，使得前端软错误更倾向于蜕变为语法合法但语义漂移的静默损坏。

**(2) RISC Load-Store语义系统性拉高PRF的ACE驻留时间。** gem5-MARVEL的跨ISA定量数据表明，RISC架构的PRF-AVF（6.0%–14.0%）系统性高于x86-64（4.7%–13.2%）。重命名控制平面的高维复杂度进一步在非校验关键路径上引入假依赖穿透风险。

**(3) SVE/SVE2架构展现出极其矛盾的双面潜能。** 变长谓词控制逻辑在数据稀疏场景下自动屏蔽冗余晶体管缺陷；同时因谓词寄存器自身的比特腐蚀，将本应引发崩溃的控制流故障降维转化为大规模静默张量毒化——这是ARM体系中最独特的SDC敏感热点。

**(4) 弱内存模型的微架构弹性代价不可忽视。** ARM64宽松的内存一致性赋予推测执行极大调度空间，但显著扩大了存储-加载转发路径的脆弱性窗口，且内存屏障微操作标记衰减可导致多线程数据静默解耦。

**(5) RAS容错架构存在深层逃逸边界。** 以AMBA CHI互联实现的数据中毒网络极大提升了缓存和主存的弹性，但缺乏保护的深层计算组合逻辑仍是防御系统的阿喀琉斯之踵。跨协议桥接（CHI→CXL/PCIe）的元数据截断构成从UCE到全局SDC的降维通道。

**(6) 非一致性错误的发现重塑了验证方法论。** ITHICA框架证实，最险恶的制造缺陷引发的是同一指令在相同输入下产生不同输出的非一致性错误，这彻底打破了依赖重复执行的传统功能测试基石。

### 5.2 未来工作方向

基于上述发现，面向未来的先进微架构设计与可靠性工程必须实施范式转移：

**微架构自愈机制。** 对于无法承受ECC高延迟的关键时序控制逻辑（如RAT），应采纳异步奇偶校验范式——允许重命名矩阵在单周期内完成投机发射，将校验逻辑延后1至2个周期异步比对；一旦捕获控制错误，复用分支预测失败的流水线冲刷机制进行管线重建。

**弱内存屏障双轨防线。** 将屏障微操作的内部状态寄存器进行双轨互补编码（如0x5A对应0xA5），确保任何单比特翻转破坏互补契约即触发硬件故障陷阱，锁死多线程并发数据的越界流动。

**谓词逻辑强力兜底。** 在SVE/SVE2谓词寄存器阵列及掩码生成树中强制部署SECDED级别保护，同时修改底层浮点流水线，强制要求异常事件先行在微架构状态机中驻留，防止掩码控制逻辑失效导致的非法张量逃逸。

**软硬协同的分布监控。** 采用类似Dr. DNA²⁶的深度学习容错方案，在运行时监控神经网络隐层激活分布特征，在All-Reduce通信彻底污染集群之前精准定位并抛弃脏检查点。结合操作系统层的Hardware Sentinel遥测架构，对表现出非确定性偏差的计算单元实时调度剥离³⁸。

**全状态周期的动态自一致性检查。** 在不可预测的微小指令生命周期内，实施基于软件/硬件融合的极度轻量级"自一致性检查"（Self-consistency Checks），唯有如此，人类计算科学方能在晶体管原子级物理缺陷发生概率日益逼近失控的未来，真正守住数字化文明算力底座的绝对可靠性防线。

---

## 参考文献

[1] Dixit, H. D., et al. "Silent Data Corruption at Scale." *SIGARCH Computer Architecture News*, 2021. https://www.sigarch.org/silent-data-corruption-at-scale/

[2] "HCI vs BTI: Transistor Aging Mechanisms at 3nm." *Patsnap*, 2024. https://www.patsnap.com/resources/blog/articles/hci-vs-bti-transistor-aging-mechanisms-at-3nm/

[3] "PinDrop: Breaking the Silence on SDCs in a Large-Scale Fleet." *Proc. HPCA*, 2026. https://www.computer.org/csdl/proceedings-article/hpca/2026/11408620/2eA88yFXUM8

[4] "Understanding Silent Data Corruption in LLM Training." *arXiv*, 2025. https://arxiv.org/html/2502.12340v1

[5] "Silent Data Corruption: Mitigating Effects at Scale." *Engineering at Meta*, 2021. https://engineering.fb.com/2021/02/23/data-infrastructure/silent-data-corruption/

[6] Mukherjee, S. S., et al. "Computing Architectural Vulnerability Factors for Address-Based Structures." *Proc. ISCA*, 2005. https://pages.cs.wisc.edu/~isca2005/papers/08B-02.PDF

[7] Jaleels, A., et al. "Explaining Cache SER Anomaly Using DUE AVF Measurement." *Proc. ISCA Workshop*, 2009. http://jaleels.org/ajaleel/publications/cacheser.pdf

[8] "Preventing Silent Data Corruption in Modern AI Chips." *Anasim*, 2024. https://www.anasim.com/articles/cumulative-voltage-droop-ai-silicon

[9] Sridharan, V., & Kaeli, D. R. "Demystifying the System Vulnerability Stack: Transient Fault Effects Across the Layers." *ResearchGate*, 2021. https://www.researchgate.net/publication/353697109

[10] Papadimitriou, G., et al. "AVGI: Microarchitecture-Driven, Fast and Accurate Vulnerability Assessment." *Proc. HPCA*, 2023. https://www.computer.org/csdl/proceedings-article/hpca/2023/10071105/1LMbBDps4qk

[11] Papadimitriou, G., et al. "Silent Data Corruptions: Microarchitectural Perspectives." *IEEE Trans. Computers*, 2023. https://zenodo.org/records/8436520/files/IEEE_TC_SDCs.pdf

[12] Jaleels, A., et al. "Explaining Cache SER Anomaly Using DUE AVF Measurement." *Proc. ISCA Workshop*, 2009. https://www.researchgate.net/publication/221574328

[13] Chatzopoulos, A., et al. "Veritas — Demystifying Silent Data Corruptions: μArch-Level Modeling and Fleet Data of Modern x86 CPUs." *ResearchGate*, 2024. https://www.researchgate.net/publication/390595518

[14] ARM Ltd. "C1.1 About the A64 Instruction Set." *ARM Architecture Reference Manual*. https://developer.arm.com/documentation/ddi0487/

[15] "Guarantees for Undefined and Unallocated Instruction Encodings." *ARM Community Forum*. https://community.arm.com/forums/f/architectures-and-processors-forum/56169

[16] "ITHICA: Intra-Thread Instruction Checking Approach for Defect Detection." *arXiv*, 2025. https://arxiv.org/html/2605.15638v1

[17] Rajakumar, A., et al. "Gem5-MARVEL: Microarchitecture-Level Resilience Analysis of Heterogeneous SoC Architectures." *Proc. HPCA*, 2024. https://www.computer.org/csdl/proceedings-article/hpca/2024/931300a543/1VOAAhcFlT2

[18] "IDLD: Instantaneous Detection of Leakage and Duplication of Identifiers used for Register Renaming." *ResearchGate*, 2022. https://www.researchgate.net/publication/365123496

[19] "Scalable Vector Extension Support for AArch64 Linux." *Linux Kernel Documentation*. https://www.kernel.org/doc/html/v6.0/arm64/sve.html

[20] "Learn the Architecture — Introducing SVE2 Guide." *ARM Developer*. https://developer.arm.com/documentation/102340/0001/SVE2-architecture-fundamentals

[21] Stephens, N., et al. "The ARM Scalable Vector Extension." *arXiv*, 2018. https://arxiv.org/pdf/1803.06185

[22] "Why Are We Accepting Silent Data Corruption in Vector Search?" *Hacker News*, 2025. https://news.ycombinator.com/item?id=46366888

[23] "Loss Spikes in Training: Causes, Detection, and Mitigations." *Medium*, 2024. https://medium.com/better-ml/loss-spikes-in-training-causes-detection-and-mitigations

[24] "Exploiting Modern Microarchitectures: Meltdown, Spectre, and Other Attacks." *Stanford EE380*, 2018. https://web.stanford.edu/class/ee380/Abstracts/180131-slides.pdf

[25] "Synchronization Overview and Case Study on Arm Architecture." *ARM Developer Blog*. https://developer.arm.com/community/arm-community-blogs/b/servers-and-cloud-computing-blog/posts/synchronization-overview-and-case-study-on-arm-architecture

[26] Gizopoulos, D., et al. "Estimating the Failures and Silent Errors Rates of CPUs Across ISAs." *CEID*, 2023. https://www.ceid.upatras.gr/webpages/faculty/gpapad/assets/papers/slm2023_gizopoulos.pdf

[27] "How Does Store to Load Forwarding Happen in Case of Unaligned Memory Access." *Stack Overflow*. https://stackoverflow.com/questions/42210733

[28] "C++ Memory Model: Migrating from x86 to ARM." *ArangoDB Blog*. https://arango.ai/blog/cpp-memory-model-migrating-from-x86-to-arm/

[29] "Reliability, Availability, and Serviceability (RAS)." *ARM Neoverse Reference Design*. https://neoverse-reference-design.docs.arm.com/en/latest/features/ras/ras.html

[30] "Learn the Architecture — Introducing AMBA CHI: RAS Features." *ARM Developer*. https://support.arm.com/documentation/102407/0102/RAS-features

[31] "Exception Handling Framework." *Trusted Firmware-A Documentation*. https://trustedfirmware-a.readthedocs.io/en/v2.12.0/components/exception-handling.html

[32] "CHI Transactions." *ARM Developer*. https://developer.arm.com/documentation/101381/latest/CHI-master-interface/CHI-transactions

[33] "CXL Spec 3.0." *Scribd*, 2023. https://www.scribd.com/document/614319679/CXL-Spec-3-0-v0-7

[34] "How Meta Keeps Its AI Hardware Reliable." *Engineering at Meta*, 2025. https://engineering.fb.com/2025/07/22/data-infrastructure/how-meta-keeps-its-ai-hardware-reliable/

[35] "Release 1.6.0 — Chipyard Documentation." *Berkeley Architecture Research*. https://chipyard.readthedocs.io/_/downloads/en/1.6.0/pdf/

[36] "SiliFuzz: Fuzzing CPUs by Proxy." *Google Research*, 2021. https://research.google/pubs/silifuzz-fuzzing-cpus-by-proxy/

[37] Karystinos, I., et al. "Harpocrates: Breaking the Silence of CPU Faults through Hardware-Aided Functional Testing." *Proc. ISCA*, 2024. https://www.ceid.upatras.gr/webpages/faculty/gpapad/assets/papers/isca2024_karystinos.pdf

[38] "Dr. DNA: Combating Silent Data Corruptions in Deep Learning using Distribution of Neuron Activations." *ResearchGate*, 2024. https://www.researchgate.net/publication/380150891

[39] "Silent Data Corruption by 10x Test Escapes Threatens Reliable Computing." *ResearchGate*, 2024. https://www.researchgate.net/publication/394292944

[40] "Phoebe: Measuring the Unmeasurable — Demystifying Silent Data Corruption." *IEEE Micro*, 2026. https://www.computer.org/csdl/magazine/mi/2026/01/11314915/2cJSYT81kFG

[41] "CHAOS: Controlled Hardware fAult injectOr System for gem5." *arXiv*, 2026. https://arxiv.org/html/2602.02119v1

[42] "PEPR: Pseudo-Exhaustive Physically-Aware Region Testing." *ResearchGate*, 2022. https://www.researchgate.net/publication/366614834

[43] "Screening For Silent Data Errors." *Semiconductor Engineering*. https://semiengineering.com/screening-for-silent-data-errors/

[44] "Understanding Silent Data Corruption in Processors for Mitigating its Effects." *ResearchGate*, 2024. https://www.researchgate.net/publication/383680854

[45] "Silent Data Corruption Challenges in Modern AI Public Cloud." *IEEE Micro*, 2026. https://www.computer.org/csdl/magazine/mi/2026/01/11285583/2ckf011qsow

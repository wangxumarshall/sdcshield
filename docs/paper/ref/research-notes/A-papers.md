# A-papers:SDC 学术文献 — 浮点/向量单元 SDC 激发原则蒸馏

> 撰写:2026-09-16(Agent A,论文研究路)
> 目的:为 SDCShield(ARM64,OpenDCDiag 分支)增强测试用例,从 SDC 学术文献提炼「如何设计测试程序以最大化激发 CPU 浮点/向量单元 SDC 故障」的可操作原则。
> 素材:`docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`(31 篇综合,已全读)+ 以下 9 篇 PDF 原文精读(全文提取,pypdf;每篇 7–16 页全部读过,含方法/结果/结论):
> SEVI(ASPLOS'26)、PinDrop(HPCA'26)、SOSP23(清华+阿里)、Harpocrates++(IEEE Micro'26)、Harpocrates(ISCA'24)、ITHICA(arXiv'26, Stanford+Google)、SiliFuzz(arXiv'21, Google)、Fleetscanner/Ripple(arXiv'22, Meta)、DelayAVF(MICRO'24, MIT+AMD);另精读 Nair HPCA'12(一阶 AVF 模型)摘要+关键结论段。
> 诚实声明:所有页码引用对 pypdf 提取文本;图表为文本提取、数值以正文表述为准;凡正文未明确给出处的一律标注「推断」或「未确认」。

---

## 1. 每篇精读论文的核心结论

### 1.1 SEVI — Silent Data Corruption of Vector Instructions in Hyper-Scale Datacenters(Mei, Varshini, Dixit, Sankar, Rashmi;ASPLOS 2026;CMU+Meta)

**方法**:Meta 数百万机器舰队,多年识别 2500+ SDC Suspect;对每台做指令级长测(246 个测试用例穷尽覆盖 AVX2/FMA3/BMI1/BMI2 全部指令,每逻辑核每用例 100 万轮,共 78 万亿轮 / 140 亿 CPU 秒)+ 应用级 matmul 测试(43 亿轮 / 25 亿 CPU 秒)。每轮不同输入,向量输出与**标量参考输出**比对(不同硬件单元,故障同时同样错两边概率极低)。

核心结论:
1. **FMA 指令是第一大 SDC 源**:>75% SDC case、>92% SDC 事故(incident)来自 FMA;top-20 SDC 致错指令中 19/20 是 FMA(Obs.2)。机理:乘法器门数 ≈ 加法器 30×,通常无硬件级保护;且 FP 向量乘法单元被 32 条不同指令共享,高利用率加速磨损。
2. **256-bit FMA 比 128-bit FMA 多出低频(<10⁻⁵)SDC case**(Obs.4)。两个机理:更宽数据通路激活更多晶体管;2× 功耗导致标量↔FMA 切换时更大电流波动 → 电压波动越过裕量。→ 对 SVE 512-bit 直接适用:更宽通路 + 更长循环才命中低频窗口。
3. **推翻「SDC 只动尾数」**:FMA 输出呈三种位翻模式——LELM(62 case)/LEHM(61)/HEHM(178);HEHM 翻 exponent 位,相对误差可达 10240;65 case 翻符号位(Obs.8/17)。**必须字节精确全位宽比对,不能只查精度损失。**
4. **37% FMA SDC case 有输入位偏置**:特定 bit 位为 0 的概率 >0.8,跨 2 厂商 7 架构 → 疑似共性设计/制造脆弱点;FP32 偏置位集中在输入尾数,FP64 集中在输入指数(Obs.8)。→ 操作数值域选择影响检出率:matmul 输入 bounded(-1,1) vs unbounded,同一核 SDC 频率差最高 **245×**(Obs.19);更大的矩阵维度 → 更多 SDC 轮。
5. **访存指令 SDC 双模式**:76% 错偏移读(地址计算逻辑坏,读到有效但错误偏移的数据)、24% 脏读(越界或 cache/数据通路损坏),且**从不 crash**(Obs.9);gather 类输入级复现率 ≈ 0(cache 状态依赖)。
6. **98.5% SDC 事故只影响单个 vector lane**;多 lane 时 96% 仅相邻 lane(Obs.13)。每核推测只有一个硬件故障点(Obs.3)。
7. >80% case 首错在 1 秒(10K 轮)内,但长尾需 42 小时长测(Obs.5);SDC 只发生在单物理核,两逻辑核近似同频失败(Obs.12);温度与 SDC 检测频率呈 log-线性负相关(Pearson -0.392,p=5.68e-12,温度越高出现越低频 SDC case)(Obs.14)。
8. **ABFT 检测**:matmul 行/列校验和,机器覆盖率 88–100%,时间开销低至 1.35%(n=1024);推荐 tile 级实现、tile<100;bounded 输入(0-1)时事故覆盖率 ~99%。GCC 实现的跨 lane 复制基线比 ABFT 慢数量级。

### 1.2 PinDrop — Breaking the Silence on SDCs in a Large-Scale Fleet(Deutsch, Dixit, Vunnam, Moran, Ozer, Sankar;HPCA 2026;MIT/Meta)

**方法**:Meta 连续高频测试基础设施,百万级服务器、4 年、5 亿+ 次测试执行、每月 4000 万次;9 大测试家族(arithmetic/concurrency/control path/data move/FPU/special(编解码加解密)/transaction/vector/other)。

核心结论:
1. **0.035% 机器(≈1/2850)终生至少一次 SDC 测试失败**;架构间 0.014%–0.178%(Obs.1)。
2. **持续测试是硬需求**:burn-in 后每季度仍有 0.0024% 机器新发;有机器测试近 4 年后才首败;>71% 坏机器持续失败多年(Obs.4/5/6)。快照式(阿里式分组测)系统性漏检间歇性/晚发/随机依赖缺陷。
3. **91.4% 测试至少失败过一次;31% 的测试曾在某机器上是唯一检出者**(Obs.7)→ 测试多样性 = 检出率,删掉任何测试都会丢唯一检出。
4. **最常失败的负载形态**:向量矩阵计算(convolution 层 + 降维,ML 应用)> 线性代数通用算术 > 并发锁资源释放握手损坏 > 事务内存 > 复杂指令流 > 大规模集合通信/数据搬移(Obs. 段落,Sec.VI-A)。
5. **vector 家族深挖**:450+ 向量指令行为 × 多向量宽度 = 2000+ 变体;**单/双精度向量 fused multiply(vfm)指令族失败率最高**;dot-product、sqrt、打包 FP 向量 add/sub/mul/div 次之;GEMM/DPE 型应用腐败率随模型规模上升;多数坏机 10 秒内检出(Sec.VI-B)。
6. **多位翻转多于单位翻转**(Obs.11);~90% 案例只有一个输出向量元素出错,元素位置无偏置。
7. **指令代际效应**:指令在其首次引入的架构代失败率最高,后续代下降(Obs.12)→ 新扩展(SVE/SVE2/SME)= 最高风险区。
8. 62% 坏机仅单物理核失败(Obs.9);同物理核两逻辑核常一起失败(Obs.8);62%/14%/24% 为 1/2-9/≥10 物理核。

### 1.3 SOSP23 — Understanding SDCs in a Large Production CPU Population(Wang, Zhang, Wei, Wang, Wu, Luo;SOSP 2023;清华+阿里云)

**方法**:100 万+ CPU、28 数据中心、32 个月;厂商工具链 633 testcase + 自研工具;27 颗坏 CPU 深度实验(数千万次测试、万+ 条 SDC 记录)。也用 OpenDCDiag 做补充验证并得到相同观察。

核心结论:
1. 总患病率 **3.61‱**;产前测试捕获 90.4%,常规运行期测试 0.348‱;所有微架构都有坏芯片,失败率不随新芯片下降(Obs.1-3)。
2. **五大脆弱特性**:算术逻辑运算、向量运算、浮点计算、cache 一致性、事务内存(Obs.5)。27 颗坏 CPU 分两类:**计算型 19 颗**(ALU/VecUnit/FPU,单线程可测)+ **一致性型 8 颗**(cache coherence / transactional memory,**必须多线程冲突场景才能测出**)。
3. **FP 数据类型最易受影响**(f32/f64/f64x > int > byte/bin)(Obs.6);FP 位翻集中在 fraction 段,精度损失小(99.9% f64 < 0.02%)(Obs.7)——**注意:此结论被 SEVI 更大样本推翻(存在 HEHM/exponent/符号位翻),两文献矛盾,采信 SEVI(样本大 2 个量级),比对必须全位宽。**
4. **位翻模式固定**:同一 testcase×CPU 组合下,位翻倾向于固定位置(>5% 记录同 mask 即算 pattern);**多 bit 翻转常见**(与 IID 单位假设矛盾)(Obs.8)。
5. **温度是低复现 SDC 的关键触发器**:6/27 颗 CPU 上 SDC 发生频率(log)与核温线性相关(Pearson>0.75),指数增长;存在最低触发温度阈值(如 MIX1 testcase C 须 >59℃,低于该温度数天测不出);余热效应(前一测试产热使后一测试出错的测试顺序依赖);同核他核 busy 也升温触发(Obs.10)。高利用率本身(独立于温度)也提高发生频率。
6. **633 个厂商 testcase 中 560 个从未检出任何错误**;失败的 testcase 用缺陷指令的频率比不失败的高数个量级(「instruction usage stress」)——**注意:ITHICA 后证明 usage stress 不是判别器(59% 案例中检出测试并非频率最高者),两文献矛盾;ITHICA 证据更细(指令级),但「高频执行」仍是必要条件之一,只是不充分。**
7. 工具链外存在只有复杂多线程冲突场景才能测出的缺陷;推荐 OpenDCDiag 与 SiliFuzz(SOSP23 原文明确推荐 OpenDCDiag:「we recommend OpenDCDiag since we have validated that it can reach the same observations as our toolchain」)。

### 1.4 Harpocrates(ISCA'24)+ Harpocrates++(IEEE Micro'26)— 自动功能程序生成(Karystinos, Chatzopoulos, Fragkoulis, Papadimitriou, Gizopoulos;雅典大学 + Gurumurthi, AMD)

**方法**:MuSeqGen(基于 MicroProbe,扩展 x86-64,约 2000 指令变体)生成受限随机汇编程序 + 指令替换突变;gem5 硬件在环评估(ACE 寿命分析覆盖位数组结构、IBR 输入位比率覆盖 FU);遗传算法闭环。评估 7 结构:IRF、L1D、LSQ、int adder、int multiplier、SSE FP adder、SSE FP multiplier;与 SiliFuzz、OpenDCDiag、MiBench 对比。

核心结论:
1. **覆盖率提升与故障检出能力强相关**(全文最关键实证;ISCA'24 Fig.10 / Micro'26 Fig.3):硬件感知覆盖指标(ACE/IBR)上升 → SFI 检出率同步上升;生成程序贴近 ACE 上界 → software masking 极小。
2. **基线测量(对 SDCShield 最重要的外部锚点)**:OpenDCDiag 的 FP-heavy 测试(MxM/SVD)是所有非生成式负载中 SSE FP 单元检出最高的——**SSE FP adder 最高 98.5%**;但 **SSE FP multiplier 只有 58.2%**(MiBench 平均更低,SiliFuzz 无 SSE 活动)。Harpocrates 生成程序达 SSE FP adder 99.8% / multiplier 99.7%,且快 2 个数量级(5 万周期 vs 1100 万周期达 99%)。
3. **IRF(整数寄存器堆)是所有框架的盲区**:检出 <5%(生成前);Harpocrates 也需 ~5000 轮收敛,最终比其他框架高 ~10×。L1D:OpenDCDiag 单测试近 80%,Harpocrates 近 90%。LSQ:所有常规负载 0–20%。
4. **FU permanent 故障短序列即可检出**:最优序列前 10%(数百条指令)检出率几乎不降;0.01×(数十条)时 int adder/multiplier 略降、SSE FP adder 不降、**SSE FP multiplier 掉到 0%**——乘法器需要更长序列(Micro'26 Fig.6)。但**间歇性/边际故障(温度、电压触发)仍需长序列**(Micro'26 原文明示)。
5. FU 收敛 ~1000 轮;L1D 2000+;IRF/LSQ ~5000。FU 上瞬态故障被逻辑/时间掩蔽,故 FU 用门级 permanent stuck-at 注入。
6. 随机 seed(immediate + 初始寄存器态)对检出率影响小(<1%;int multiplier 例外 ~17%,归因 x86 MUL 隐式写预定义寄存器导致覆写掩蔽)。
7. **意外收获**:生成程序挖出 gem5 v22.0.0.2 的 RCR 指令仿真 bug → 非平凡随机序列连模拟器 bug 都能挖出。

### 1.5 ITHICA — Intra-Thread Instruction Checking(Vavelidou, Banerjee, Liu, Fuller, Mitra, Trippel;arXiv 2026;Stanford+Google)

**方法**:LLVM IR pass 给任意程序插入线程内指令级检查(指令复制 + 输出比对;MemDiv 主动多样性:mfence/clflush 迫使原始/验证访存指令走不同存储层级);3000+ 台隔离服务器(≥10 微架构),100 台缺陷机深度分析,每台 2000+ 小时。

核心结论:
1. **缺陷导致 inconsistent errors**(同线程同指令同架构输入,两次执行产生不同架构输出,取决于 execution context = 微架构+电气状态完整快照)——最易逃逸出厂测试的缺陷正是此类。**24% 检出中原始与验证指令都错但错得不同**(同一缺陷硬件,不同 context 仍产生不同错误输出)。
2. ITHICA 检查比同二进制内 native 最终输出检查多检出 **39%** 缺陷服务器;比 SiliFuzz 多 69%。Arith pass 检出最多(14 台中 11);**MemDiv(clflush 强制主存重取)独家检出 D13 及 1 台 QPool 机**——验证「主动扰动 execution context」能暴露其他方式测不出的缺陷。
3. **execution context(前置指令序列)是错误显现的首要预测因子;指令使用频率在 59% 案例中不是判别器**(Finding 7);「culprit input」也不充分(Input Breadth 高,错误跨大量不同输入)。
4. **短测试几乎无法复现**:14 台中仅 1 台(D9, fmul x86_fp80 单汇编指令单微操作)可单指令复现;基本块级也仅 D3;**长、多样、真实的程序优于短小定向测试**(Finding 8)。
5. **同一缺陷常跨指令类型致错**(44% 服务器,速率差最高 6 个量级)→ 「硬件定位」在 ISA 层是不透明的,别急着把缺陷归因到单个单元。
6. **库内部错误会被 logical masking 掩蔽**:31 台被 Native 漏检的机器中 8 台仅在库代码内部(Zlib 6 台、OpenSSL 1 台、两者 1 台)被 ITHICA 检出——**只查库最终返回值/最终输出会漏检,须在库内部比对或对库输出做全量 golden**。
7. 检出指令类型分布:整数算术/FP/向量/访存都有;向量 double 指令(fmul/fma <8x dbl>)PC 敏感度最高(但集中在单个 BB)。EDR 1.78×、EF 7.51×、TTD 0.68×(1.47× 更快)vs Native。
8. block size/interleaving 放宽(8)仍保持可复现机器的检出 → 缺陷的永久性允许降低插桩密度换开销。

### 1.6 SiliFuzz — Fuzzing CPUs by proxy(Serebryany, Lifantsev, Shtoyk, Kwan, Hochschild;arXiv 2021;Google,libFuzzer 作者团队)

**方法**:fuzz XED(x86 编解码器)/Unicorn(QEMU 系模拟器)/ifuzz(随机生成)三类代理积累 snapshot 语料(50 万+,每个 <100 字节代码,微秒级),在真实 CPU 大规模重放,多核交叉验证;快照绑核执行、driver/harness 双进程隔离。

核心结论:
1. **"任何足够非平凡的执行,在足够大规模上都会挖出一些缺陷"**——纯随机 ifuzz 也独占发现 20% 缺陷;Unicorn/XED 代理 fuzz 独占 40%,随机与覆盖引导**互补**。45% 的发现是其他工具没找到的。
2. 约 70% 案例单个物理核(一对逻辑核)受影响;TTD 分布极宽(立即到数天)。确认缺陷 = 同核多日可复现。
3. 常见缺陷模式:整数/浮点/向量指令错果(常只错 1 bit)、伪 SIGFPE/SIGSEGV/SIGILL、FP 异常丢失、EFLAGS sticky 位、x87 data pointer 不更新等。**发现与 Unicorn 中 C 实现部分(FP/向量)相关 → FP/向量是代理覆盖反馈最好、缺陷最多的区域。**
4. **FCOS 案例(对 SDCShield 极有启发)**:x87 超越函数缺陷——prologue(fldln2/fldln2/fprem1)把核置入坏状态后 payload(fld1/fcos)才开始算错;无需 prologue、仅循环跑 payload 换 ST(0) 值若干次也会开始算错;错值差 <0.0000003%;**理论:输入编码的 bit 23 为 0 时几乎必错、为 1 正常**——数据位敏感 + 隐藏状态 + 超越函数。
5. 隔离机复现需数周,扫描电压/频率/温度组合。每核每机必须反复重测(wear & tear)。

### 1.7 Fleetscanner / Ripple — Detecting SDCs in the wild(Dixit, Boyle, Vunnam, Pendharkar, Beadon, Sankar;arXiv 2022;Meta)

核心结论:
1. **四大触发维度**:数据随机化(位模式表示依赖,3×5=15 但 3×4=10)、电气变化(f/V/I 波动)、环境变化(温度/湿度/热点,机房 A 对机房 B 错)、生命周期变化(同一计算每天重复 6 个月后才失败——退化)→ 唯一出路是**反复测试 + 不断改进测试**。
2. Fleetscanner(产外深测,分钟级,搭维护窗口便车):93% 覆盖率(对所研究缺陷族),23% 唯一覆盖,5-6 个月全舰队轮一遍。Ripple(在产高频,毫秒级片段共置):月 25 亿次执行,77% 总覆盖,**15 天即达 70% 等效覆盖**,7% 唯一覆盖(部分缺陷只有「测试与工作负载频繁切换」才触发,连续长跑测不出)。
3. 测试模式 tradeoff:**长深测与高频短测互补,两种节奏都要**。

### 1.8 DelayAVF — Calculating AVFs for Delay Faults(Deutsch, Sridharan, Ulitzsch, Emer, Gurumurthi, Yan;MICRO 2024;MIT+AMD)

**方法**:定义 DelayACE/DelayAVF 量化小延迟故障(SDF,边际缺陷的物理表现:亚周期传播延迟增量)的架构脆弱性;两步法(时序感知模拟求 dynamically reachable set + 时序无关求 GroupACE);Ibex RISC-V 核(NanGate 45nm 综合)5 结构 × 5 Beebs 基准(md5/bubblesort/libstrstr/matmult/libfibcall)。

核心结论:
1. **toggle 依赖**:信号不翻转则延迟无害 → **ALU 的 DelayAVF 高达寄存器堆 5×**(尽管 regfile 静态可达路径更多;因为每周期只有被激活/去激活的两根 wordline 在翻转)。
2. **md5(数据随机、高翻转率)的 ALU DelayAVF ≫ libstrstr(规整数据)**:随机数据 → 高翻转率 → SDF 可见性高。这是「高熵操作数」的最直接理论依据。
3. **多 bit 错误普遍**:诱发状态元件错误的 SDF 中约 50% 产生多 bit 错(d=10% 时最低 21%,其余 d 值围绕 50% 波动)→ 整块 golden memcmp 必需,单位抽检会漏。
4. **ECC 对 SDF 有盲区**:wordline 延迟 → sense amp 重新锁存上一行数据,数据「合法」但错行,ECC 校验通过。Regfile 加 SECDED 后 sAVF=0 但 DelayAVF 不为零。→ 硬件校验不可替代功能级 golden 比对。
5. 小 d 由静态时序主导;大 d 时程序/架构效应更显著;高度优化核(关键路径更多)程序效应更强。扫频/扫压等价于扫描未知 d。

### 1.9 Nair — A First-Order Mechanistic Model for AVF(Nair, Eyerman, Eeckhout, John;HPCA 2012;UT Austin+Ghent)

核心结论(摘要+结果段精读):
1. **AVF ≈ correct-path 状态占用率 × ACE 比例**;ROB 占用率支配 LQ/SQ/FU 占用。模型误差 <0.07(4-wide OoO)。
2. **4-wide 比 2-wide 平均 SER +81%**(更宽发射需要更大指令窗口,β 1.24–2.39 超线性)→ 宽发射机器(如鲲鹏 920)天然更脆弱,压测收益放大。
3. **namd 型负载(高 IPC、少 miss、少分支、长依赖链)= 高 AVF;gobmk 同样高 IPC 却低 AVF** → 聚合指标(IPC/miss rate)不能预测 AVF,须看占用结构:高 IPC + 高占用 + 全 correct-path 才是最脆弱形态。
4. 与 Mukherjee MICRO'03 一脉:平均仅 ~45% 动态指令是 ACE;浮点程序 AVF 高于整数程序(长延迟指令多、误预测少、窗口占用高)。

---

## 2. 跨论文提炼的 SDC 激发原则(重点)

### P1. 指令类型选择:打第一大 SDC 源
- **向量 FMA 族(fmla/fmmla/dot/.outer product)是压测第一优先**:SEVI 92% 事故 / PinDrop vfm 最高失败率 / Veritas(综合文档引)vector ≫ scalar、数量级差 / 阿里 VecUnit+FPU 19/27 计算型。乘法器门数 30× 加法器、无 ECC、32 条指令共享单元(SEVI 机理)。
- 次优先:向量整数乘法、dot-product、sqrt/超越函数(ITHICA fmul fp80 单指令复现;SiliFuzz FCOS 案例)、gather/scatter(专门验证**读偏移正确性**:SEVI 76% 错偏移、输入级复现≈0、从不 crash)。
- 低价值:标量整数加法器(→crash 倾向)、控制流/栈指令(ret/call/push/pop 的错误从不产生 SDC,From Gates to SDCs)、ROB/LQ/SQ(依赖图检查必 crash)。
- **新指令代际最高风险**(PinDrop Obs.12):SVE/SVE2/SME 扩展 = 最少验证积累。

### P2. 数据模式:高熵 + 双档值域 + 位敏感意识
- **高熵随机操作数、逐次变化**:DelayAVF md5≫libstrstr 的直接推论(翻转率决定 SDF 可见性);MeRLiN(综合文档)同输入重复迭代不扩覆盖;SiliFuzz FCOS bit-23 敏感;SEVI 37% case 输入位偏置(为 0 概率>0.8)。
- **双值域都要跑**:SEVI matmul bounded(-1,1) vs unbounded 同核 SDC 频率差 245×;bounded 输入还放大 ABFT 相对误差(SEVI:bounded 时事故覆盖率 ~99%)。
- **不能假设位分布**:SOSP23 说只翻尾数,SEVI 推翻(exponent/符号位也翻,误差达 10240)→ **全位宽字节精确 memcmp 是唯一安全比对方式**;多 bit 错误普遍(DelayAVF ~50%、PinDrop Obs.11、SOSP23 Obs.8、SiliFuzz 常见 1 bit)→ 整块比对不可用抽检替代。

### P3. 依赖链与占用:高 IPC + 少分支 + 长依赖链 + 少 miss
- Nair:namd 型(高 IPC、少分支、长延迟指令、窗口满载全 correct-path)= AVF 谱最广;4-wide 宽发射 SER +81%。
- Mukherjee(综合文档):直线代码、无 dead code、每条指令结果都被消费;浮点程序 AVF 高于整数程序。
- Harpocrates 实现佐证:寄存器分配「最大化依赖距离,平衡高 ILP 与数据流传播」;分支全部顺序解析。
- ITHICA 反面警示:**热路径加一条语义 no-op 指令就可能杀死触发**(execution context 敏感;ITHICA Obs.11 组合 pass 反而漏检单体 pass 检出的机器)→ dump/日志代码全放 cold 分支,热循环逐字节稳定。

### P4. 访存与驻留:长驻留、低覆写、写脏整行再回读
- Biswas/GeFIN/MARVEL(综合文档):覆写是天然掩蔽器;write-back cache 写脏一个 byte 整行全 ACE。
- ITHICA MemDiv:**clflush + 重取(强制走主存)能独家检出其他方式测不出的机器**——主动迫使访存走不同层级是有效扰动。
- SEVI gather:错偏移读不 crash → 必须显式验证读地址正确性(读回内容与按索引预期的 golden 比对,而非仅比数值)。
- 足迹要铺谱:L1/L2/LLC/DRAM(不同结构的 ACE 驻留不同);但控制 miss(Nair:miss 打断占用)。

### P5. 检测方式:即时/细粒度 + golden + 双执行上下文
- **golden 在 init 算好一次、字节精确 memcmp**(SEVI 用「向量 vs 标量参考」双硬件单元交叉验证思想同理:不同单元同时同样错的概率极低)。
- **细粒度检查优于最终输出检查**(ITHICA +39%/EDR 1.78×/EF 7.51×):每迭代 copy→compute→verify;库内部错误会 logical masking(ITHICA 8 台仅在 Zlib/OpenSSL 库内部检出)→ 库输出的全量 golden 比对,不能只查返回值。
- **指令复制 + 比对**(线程内双执行,容忍非确定性)是文献中检出增量最大的单一技术;对 SDCShield 的等价物:同一输入跑两个不同调度(Eigen NEON vs OpenBLAS)交叉验证。
- **lane 级比对**(SEVI 98.5% 单 lane)+ 整块 memcmp 双形态保留。

### P6. 时间与节奏:长窗口 + 双节奏 + 持续化
- SEVI:>80% 首错 1 秒内,长尾 <10⁻⁵ 需 42 小时;ITHICA:长程序 71 台 vs 短测试 42 台(综合文档);PinDrop:近 4 年才首败的机器存在。
- Harpocrates++:FU permanent 前缀 10% 即可,但 **marginal/间歇故障需长序列**;SSE FP multiplier 在 0.01× 前缀时检出掉到 0(乘法器要长序列)。
- Fleetscanner/Ripple:长深测与高频短测**互补**,7% 缺陷只有「与工作负载频繁切换」才触发;15 天高频 ≈ 70% 等效覆盖。
- PinDrop:持续测试比快照测试多数量级检出;SDC 检测是持续性命题。

### P7. 环境边际:温度 + 并发压力 + (不可用时的补偿)
- SOSP23:温度 log-线性触发(Pearson>0.75)、存在最低触发温度、余热/顺序效应、他核 busy 升温;高利用率独立于温度也提高频率。SEVI:温度越高低频 SDC case 越多(log-线性)。
- **全核并发是标准手段**(PinDrop/阿里/Fleetscanner 均全核同测)+ burn-in 预热(阿里 Farron 先跑 burn-in 再测)。
- 本机(鲲鹏 920 开发板)无 cpufreq/thermal zone → V/F/T 扫描杠杆缺失(DelayAVF:扫频扫压=扫 d),补偿 = 负载多样性 × 全核热功耗压力 × 足迹谱 × 长时间 × seed 多样性(综合文档平台限制声明,继续有效)。

### P8. 多样性 > 单负载强度(所有论文最强共识)
- PinDrop:31% 测试曾是唯一检出者;阿里:633 testcase 中 560 从未检出(反向:选对负载家族极重要)。SiliFuzz:随机与覆盖引导互补,45% 独占。ITHICA:execution context 多样化(FB 负载独检 11 台;不同 pass 组合改变指令序列影响检出)。Harpocrates++:同一数学、不同指令调度 = 不同检出。
- 工程含义:**同一数学多种调度**(Eigen NEON / OpenBLAS TSV110 / 手写 SVE 汇编 = 三个相位样本)+ 指令类型广度 + 值域档 + 足迹档。

### P9. 一致性/多线程类缺陷需要专门的并发测试
- 阿里 8/27 颗是 cache 一致性/事务内存型,**单线程永远测不出**;Meta 锁释放握手损坏、事务内存缺 core 亲和(PinDrop)。
- SiliFuzz 未来工作:并行线程跑非重叠 snapshot 以压 cache 一致性。
- ARM 无 TSX → 重点:cache 一致性 + 原子操作 + 锁路径(本仓 spinlock 系列方向正确)。

### P10. 检出 ≠ 定位:ISA 层硬件定位是不透明的
- ITHICA:44% 服务器同一缺陷跨多指令类型,速率差 6 个量级;「vector 测试检出」不等于「向量单元坏」。定位需要指令级检查 + 多 context 实验;报告结论时保持克制。

---

## 3. 专门针对浮点/向量单元的发现(SEVI 及相关)

1. **FMA 链是核心弹药**:SEVI >92% 事故、19/20 top 指令;PinDrop vfm(乘+加+舍入/存储的复合链)最高失败率。ARM64 对应:`FMLA`/`FMMLA`(SVE2 FP 外积)/`SDOT·UDOT·FDOT`(SVE2 dot)/`BFMLA`(SVE bf16)。SVE2 的 FMMLA 外积正是「宽通路 + 乘加复合 + 新指令代际」三重高风险叠加。
2. **宽向量独有低频窗口**:256-bit FMA 比 128-bit 多 <10⁻⁵ 低频 case,机理 = 更多晶体管 + 2× 功耗电流波动 → 电压裕量。**SVE 512-bit(VL=64B)比 AVX2 256-bit 更宽,推断风险面更大(推断,SEVI 未直接测 SVE)**;需要更长循环才能命中低频窗口。NEON 128-bit ↔ SVE 512-bit 双档都要跑。
3. **FMA vs gather/scatter vs 打分板压力**:
   - FMA:输出三模式(LELM/LEHM/HEHM),HEHM 翻 exponent、相对误差达 10240,65 case 翻符号位 → **比对必须整位宽,禁用容差**;37% case 输入位偏置(FP32 偏输入尾数、FP64 偏输入指数)→ 值域档敏感(245×)。
   - gather/scatter:**76% 错偏移读**(有效数据、错误地址),绝不 crash,cache 状态依赖导致输入级复现率≈0 → 必须专门设计「读偏移正确性」验证(按索引取golden比对),且要跑足够长 + 多 cache 状态(ITHICA MemDiv:强制 L1 miss/主存路径)。
   - 打分板/转发路径(ITHICA RTL 例 + CVA6 实验):同一 MUL 两次执行因操作数转发时机不同而错得不同——**寄存器依赖距离多样化(长链 vs 背靠背)是激发此类缺陷的开关**。
4. **98.5% 单 lane、多 lane 96% 相邻**(SEVI)→ lane 级比对定位精度高;跨 lane 复制检测(SEVI 基线)比 ABFT 慢但简单。
5. **乘法器需要长序列**(Harpocrates++:SSE FP multiplier 0.01× 前缀检出 0%,adder 不降)→ FP 乘法/FMA 压测不能只跑短前缀。
6. **FP 单元的 AVF 高于整数**(Mukherjee/Nair 一致);FPU aging-prone 路径 1366 条 vs ALU 11 条(Vega,综合文档)→ FP 数据通路是 BTI 老化最先越界的地方。
7. **标量 FP 也能单指令复现**(ITHICA D9: fmul x86_fp80;SiliFuzz F2XM1/FCOS x87 超越函数)→ 标量 FP 超越函数(libm/SLEEF 的标量路径)值得一档独立测试,尤其是**带隐藏状态的序列**(SiliFuzz prologue→payload 模式:先跑一段 FP 序列再测目标函数)。

---

## 4. 对 SDCShield 的可操作建议清单

> 置信度标注:【直接】= 论文直接结论可套用;【推断】= 由论文结论合理推导、未在论文中直接验证。按对 ARM64 浮点/向量 SDC 激发价值排序。

### 高优先级

1. **把向量 FMA 族作为测试组合的核心,覆盖全部宽度档**:
   NEON 128-bit `FMLA`(已有 eigen/openblas 路径)+ SVE `FMLA`(z 寄存器,实测 VL)+ SVE2 `FMMLA/FDOT/SDOT`(若硅片支持)。FP32 与 FP64 都要(SEVI:126 vs 175 case,数量相当);BF16/FP16 一档(SVE 新类型 = 新代际风险)。
   【直接:SEVI Obs.2/4、PinDrop vfm;推断:SVE 512-bit 风险面外推】
2. **FMA 长链压测,不用短前缀**:乘法器类缺陷短前缀检出崩塌(Harpocrates++ 0.01× → 0%);SEVI 低频长尾需长测。确保 FMA 测试的 TEST_LOOP 持续打满 `-t` 窗口,不提前退出。
   【直接:Harpocrates++ Fig.6、SEVI Obs.4/5】
3. **双值域档轮换**:每测试跑 bounded(-1,1 归一化型)与 unbounded(全值域随机)两档输入——SEVI 实测同核差 245×;bounded 档对校验和类自检更敏感。
   【直接:SEVI Obs.19】
4. **gather/scatter 专项:验证读偏移正确性**:按索引构造可识别数据(如 A[i]=i 的编码),gather 后逐 lane 验证「读到的值 == 预期索引的值」,而非只比数值哈希;搭配 cache 状态扰动(ITHICA MemDiv 思想:强制 miss/主存路径,如大跨度索引或 flush 类操作)。已有 `sve512_gather_scatter_arm` 方向正确,建议加强偏移验证显式性。
   【直接:SEVI Obs.9(76% 错偏移、不 crash、复现≈0);推断:MemDiv 式层级强制】
5. **保持并强化字节精确全位宽 memcmp**:禁用任何「精度损失 < x% 即 pass」的容差比对;FMA 输出可能翻 exponent(误差 10240)或符号位(65 case)。
   【直接:SEVI Obs.8/17;SOSP23 反例已被 SEVI 修正】
6. **同数学多调度 = 免费检出率**:Eigen NEON / OpenBLAS(TSV110 核) / 手写 SVE 内联汇编三份 GEMM 保持并行存在;考虑再加 ACL 或自写微内核。每次新增库时优先选「指令调度差异大」的实现。
   【推断:PinDrop 31% 唯一检出者 + SiliFuzz 互补性 + ITHICA execution context;三库等价物未在单篇论文中直接验证】
7. **库输出全量 golden,不止返回值**:zstd/zlib/OpenSSL/SLEEF 测试必须对完整输出缓冲做 memcmp(ITHICA:8 台仅在库内部检出,最终输出被 logical masking 掩蔽)。
   【直接:ITHICA Obs.12】
8. **热循环零扰动纪律**:dump/日志/调试代码全部移出热循环(cold 失败分支);一条 no-op 都可能杀触发。
   【直接:ITHICA Obs.11/execution context;与 CORE179 探针 H/X 互证(综合文档)】

### 中优先级

9. **双档 seed 机制固化**:默认 seed fracturing 广域扫(每轮变输入,扩激活覆盖)+ 可选固定 seed 驻留模式(单一模式持续暴露)。文档化两者用途。
   【直接:DelayAVF(toggle 依赖)+ MeRLiN(重复不扩覆盖)+ SiliFuzz FCOS(循环跑不同 ST(0) 值后开始算错);驻留档为推断】
10. **双节奏运行模式**:默认长窗口(`-t` 大值)+ 快速扫描模式(短前缀轮换多数负载)。7% 类缺陷只有「频繁切换」触发(Ripple 唯一覆盖)。
    【直接:Fleetscanner/Ripple 15 天 70% + 7% 唯一覆盖;Harpocrates++ 前缀实验】
11. **标量 FP 超越函数 + 隐藏状态序列档**:libm/SLEEF 标量路径(sin/cos/exp/log/atan);先跑一段 FP 预热序列(prologue)再验证目标函数(SiliFuzz FCOS 模式);值域扫含「特定 bit 为 0/1」的对偶模式(如掩码翻转某一位的 magic values)。
    【直接:SiliFuzz 附录 C/A;ITHICA D9;推断:ARM64 标量 FP 等价性】
12. **一致性/原子/锁并发测试维持并扩充**:spinlock/原子操作/cache 一致性冲突场景(两线程同 cache line 乒乓、原子读-改-写竞争)。阿里 8/27 颗必须多线程才测出;PinDrop 锁握手损坏真实出现。
    【直接:SOSP23 Obs.5/深度分类;PinDrop 常见失败模式】
13. **lane 级比对诊断增强**:失败报告输出 lane 编号分布(98.5% 单 lane、96% 相邻——相邻 lane 模式是「单物理单元坏」的指纹);失败时自动做 lane 直方图,辅助定位。
    【直接:SEVI Obs.13;诊断价值为推断】
14. **向量宽度扫描**:SVE 测试跑多档 VL(通过 `svprslen`/向量长度属性,若平台支持运行时改 VL,或编译期多档)-128/256/512 bit;SEVI 显示宽度改变 SDC 频率分布(256-bit 独有低频档)。
    【直接:SEVI Obs.4(128 vs 256);推断:外推到 SVE 可变 VL】
15. **ABFT 自检作为低成本兜底通道**(可选):GEMM 测试加行/列校验和快速档(tile<100),作为长测前的快速筛查;不是 memcmp 的替代(覆盖率 88-100% < 100%)。
    【直接:SEVI Sec.5;定位为辅助通道是推断】

### 支撑性/框架级

16. **部署协议:周期性持续测试 + 结果数据库**:一次 pass 不证明健康(>4 年才首败的机器存在);Farron 式优先级(检出的测试加时)。scripts/run/ 循环脚本 + 历史结果留存。
    【直接:PinDrop Obs.4/5、Fleetscanner 生命周期观察】
17. **温度不可控时的补偿声明 + 全核并发硬前提**:无 cpufreq/thermal zone 的板子上,全核并发热功耗压力是唯一的「温度代理」;文档中明示该杠杆缺失。
    【直接:SOSP23 Obs.10;平台限制为综合文档既有结论】
18. **报告措辞纪律**:检出报告避免断言「FP 向量单元损坏」——44% 案例同一缺陷跨多指令类型;表述为「XX 测试检出,疑似 XX 指令族相关」。
    【直接:ITHICA Sec.7.5/Fig.9】
19. **远期:生成式/差分方法补盲区**:IRF/LSQ 是所有常规负载盲区(<5%/0-20%,Harpocrates 基线测量);SiliFuzz 式「192 核互为 golden」差分重放与随机指令序列生成值得规划(纯随机也独占 20% 发现)。
    【直接:Harpocrates Fig.4/11、SiliFuzz;实施为远期推断】
20. **SSE FP multiplier 58.2% 的教训**:通用 FP-heavy 负载对「FP 乘法器」覆盖天然不足(OpenDCDiag 上游数据)——SDCShield 的 FP 测试组合应显式包含高乘法密度、低加法占比的微内核(如纯 `FMUL` 链 + 舍入模式变化),而非只靠 GEMM(乘加混合)。
    【直接:Harpocrates ISCA'24 Fig.6 基线;微内核设计为推断】

---

## 5. 精读论文之外的引用(来自综合文档,本文未重读原文,引用时注明)

- Veritas(HPCA'25,雅典+Meta):vector 单元 SDC 率比 scalar 高数个数量级;机队 VFMUL 相对 SDC 率 4580 vs 标量 ADD 21;crash 比 SDC 多 2-3 倍。【经综合文档转引,未读原文,标注转引】
- From Gates to SDCs(DATE'25):sha 类负载掩蔽最少;控制流/栈指令错误从不产生 SDC。【转引】
- Mukherjee MICRO'03 / Biswas ISCA'05 / Bower SIGMETRICS'06 / MeRLiN ISCA'17 / gem5-MARVEL HPCA'24 / Vega ASPLOS'24 / Hardware Sentinel ASPLOS'25 / TC'22 / TC'23:均经综合文档转引(驻留/覆写、ACE 理论、FPU 老化 1366 路径、Arm L1D 最高瞬态 AVF 等),关键处在正文标注。

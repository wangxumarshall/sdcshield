# ITHICA: Intra-Thread Instruction Checking Approach for Defect-Induced Silent Data Corruptions(arXiv 2605.15638,2026-05;Stanford University + Google LLC)

> 来源 PDF: docs/paper/ref/ITHICA Intra-Thread Instruction Checking Approach for
> Defect-Induced Silent Data Corruptions.pdf(15 页,正文 13 页 + 参考文献 2 页)。
> 所有数值均标注原文图表号;读不到的写"原文未给出"。

## 1. 研究问题与核心贡献(≤5 行)

- 问题:现有 SDC 功能测试(cpu-check/OpenDCDiag/SiliFuzz 等)都隐含"缺陷产生**一致错误**"假设
  (同指令同输入必产生同错输出),导致只有具备可校验最终输出的程序才能当测试,且无法做指令定位。
- 核心洞察:**最恶性缺陷产生"不一致错误"(inconsistent errors)**——同线程内同指令同架构输入,
  两次执行可产生不同架构输出(取决于执行上下文 execution context)。
- 贡献 1:ITHICA 工具——LLVM IR pass,通过指令复制 + 输出比对把任意程序变成功能测试,
  并新增 MemDiv 变换(主动插入 mfence/clflush 扰乱存储层次交互)。
- 贡献 2:超大规模实测——3000+ 台嫌疑/确认缺陷服务器(≥10 个微架构),
  ITHICA 共检出 100 台缺陷服务器(是 Alibaba 研究 27-30 台的 3 倍多、SEVI 18 台的 5 倍多)。
- 贡献 3:10 项新发现,推翻"指令使用频度预测检出"、"短测试可复现错误"、"向量单元硬件定位"
  等先前超大规模集群研究的结论(原文 Table 9)。

## 2. 实验方法(插桩/检测机制、输入、负载)

### 2.1 检测机制:四种变换(原文 Table 1,"ITHICA Transformation Passes and Validation Rules")

每个变换向程序插入四类指令:
- **validation 指令**:执行冗余计算(复制原指令);
- **check 指令**:检测原/验证指令间的不一致错误(eq(r0,r1) 比对);
- **error-reporting 分支**:不一致时跳转到错误上报基本块
  (每个基本块的比对结果**聚合为单个**错误处理分支,而非每条指令一个分支);
- **diversity 指令**(仅 MemDiv):在原/验证访存指令之间主动扰动微架构状态。

四种变换的插桩规则(原文 Table 1,逐行):

| Pass | 目标指令 | 原指令 | 验证指令 | 插入的检查 |
|---|---|---|---|---|
| **Arith** | 逻辑/二元/位运算/向量/一元/聚合与转换 op、GEP、icmp、fcmp、select、无副作用 intrinsic 与 inline asm | r0 = op(s0,s1) | r1 = op(s0,s1) | eq(r0,r1) |
| **Mem** | load(非原子、非 volatile) | v0 = load(a0) | v1 = load(a0) | eq(v0,v1) |
| **Mem** | store(非原子、非 volatile) | store(v0,a0) | v1 = load(a0) | eq(v0,v1) |
| **MemDiv** | load | v0 = load(a0) | v1 = load(a0); clflush(a0); v2 = load(a0) | eq(v0,v1) && eq(v0,v2) |
| **MemDiv** | store | store(v0,a0) | v1 = load(a0); mfence(); v2 = load(a0); clflush(a0); v3 = load(a0) | eq(v0,v1) && eq(v0,v2) && eq(v0,v3) |
| **Br** | 条件分支 | br(c0, t0,t1) | 源块: e = c0 ? t1:t0; store(e,tmp); 目标块: v0 = load(tmp) | 目标块内 eq(v0, t0) / eq(v1, t1) |

要点:
- **Arith** 复制保持原数据依赖链,允许错误传播到较远比对点(配合大 block size)。
- **Mem 的已知盲区**:验证 load 通常走 L1 命中(检查 load)、store 检查走核内 store buffer,
  会漏掉 L1 miss / 下层存储层次的输出错误——这正是 MemDiv 的动机。
- **MemDiv 的 mfence/clflush 插入逻辑**:对 load,原 load 后紧跟验证 load #1(L1 命中),
  然后 `clflush(a0)` 把行逐出,再验证 load #2(强制从主存取数),三值比对;
  对 store,先 store,然后验证 load #1(可能命中 store buffer),`mfence()` 清空 store buffer
  (迫使后续访存真正访问 cache),验证 load #2,再 `clflush(a0)` 逐出行,
  验证 load #3(从主存取),四值比对。即:**mfence 鼓励 L1 命中路径,clflush 强制主存路径**,
  让原/验证访存指令与存储层次的不同层级交互,暴露核外(uncore)缺陷。
- **Br** 只针对条件分支,只查"两个合法目标之间的错向",比 CFCSS 等轻量;
  闯入野地址更可能崩溃(非静默),无需功能测试即可发现。
- **排除的指令**:atomic/volatile load/store(内存副作用)、alloca、atomicrmw、cmpxchg、fence;
  LLVM intrinsic 与 inline asm 需先经属性验证无副作用才复制。
  OpenSSL 故意允许部分数据竞争(thread-unsafe),所以实验中**不对 OpenSSL 施加 Mem/MemDiv**,
  只施加 Arith/Br(§4.3)。
- 防后端优化消除插桩:复制 load/store 标 volatile;算术指令参数穿过 no-op volatile store-load 对。
- ITHICA 作为**最后一个 IR pass**运行,减少与优化 pass 的相互干扰;比对在同一 ITHICA 编译的
  二进制内进行(Native checks vs ITHICA checks),避免插桩改变指令降级导致不公平。

### 2.2 配置选项(§4.2)

- **Interleaving(交错度)n**:n 条原指令连续出现后,再按同序出现对应的验证/diversity 指令。
  interleave=1 即验证指令紧跟原指令(自然多样性 natural diversity 的最小形态)。
- **Block size(块大小)n**:每第 n 条原指令插一次比对。**验证指令对全部相关原指令都插入**
  (保证错误可沿依赖链传播到比对点),只有比对频率被稀释。dep = 在每条指令依赖链末尾插比对。

### 2.3 被插桩的输入程序(§5.1)

1. **CC = Google cpu-check**(github.com/google/cpu-check):工业级基线测试套件,
   含可逆计算对(加密-解密、压缩-解压)+输入输出比对、随机数据 checksum/hash+跨核比对、
   AVX 计算+向量 lane 比对。C++ 实现,可 LLVM 编译(SiliFuzz 测试是汇编,不行)。
2. **FB = Fleetbench**(github.com/google/fleetbench):Google 数据中心热点函数微基准
   (Proto、Swissmap、Libc、Tcmalloc、Hashing、Compression、Stl-Cord)。
   排除 Compression(依赖 OpenMP,无法静态编译)。FB 无内建可校验输出——
   **没有 ITHICA 插桩它们根本不能当测试用**(§7.3 Obs. 13)。
3. **库**:CC 用的 Libc/Libc++(因与 GCC 耦合改用 LLVM 的 Llvmlibc/Llvmlibcpp)、
   Abseil、OpenSSL、Zlib;FB 的库靠 Blaze 构建系统整体重编。

## 3. 实验配置(硬件/软件/参数)

### 3.1 两池评估策略(§5.2)

- **QPool(隔离池)**:3000+ 台被超大规模集群(总数数百万台)隔离的可疑服务器,
  ≥10 个微架构、2 家主流 CPU 厂商;动态池。服务器入池三途径:
  (1) 客户数据损坏投诉;(2) 厂商/超大规模集群筛查测试标记;(3) 软硬件异常的取证分析
  (fail-stop/fail-slow、MCE、内核/用户崩溃、用户代码 invariant 失败)。
- **DPool(缺陷池)**:从 QPool 划出 **20 台**专供 ITHICA 专用测试;
  全部在生命周期内至少挂过一次超大规模集群或厂商功能测试;按微架构、机龄(10–81 个月,
  原文 Table 2)、地理位置、入池时间多样化。**其中 6 台任何测试(ITHICA 与 CC)都测不出错,
  实际报告 D1–D14 共 14 台**。原文 Table 2:D1=u1,D2=u2,D3=u3,D4=u4,D5=u5,D6=u5,
  D7=u2,D8=u6,D9=u1,D10=u4,D11=u3,D12=u5,D13=u1,D14=u2(u1–u6 为 6 个匿名微架构;
  机龄:D1=19,D2=6,D3=34,D4=16,D5=45,D6=38,D7=81,D8=19,D9=24,D10=11,D11=11,D12=42,
  D13=10,D14=64 个月)。
- DPool 每台累计 **2000+ 小时**测试时间,比此前详细研究多 1–2 个数量级([51][82][83])。

### 3.2 运行参数(§6.1/§6.2)

- DPool:每台跑 **8 个测试** = 7 个 CC-ITHICA(Arith、Mem、MemDiv、Br、Arith+Mem、
  Arith+MemDiv、Arith+MemDiv+Br,全部 block size=1、interleaving=1)+ 原版 CC 对照。
  每测试 **100 次 × 每次 1 小时**(崩溃则提前终止),**run 之间重启服务器**。
  每次运行执行数十万轮程序、数十万个随机输入。
- Interleaving 扫描:固定 block size=1,interleaving ∈ {1,2,4,8,max(基本块长度)}。
- Block size 扫描:固定 interleaving=1,block size ∈ {1,2,4,8,dep(依赖链长)}。
- 库与 FB:只用 Arith(block size=1、interleaving=1,因该配置在 DPool 检出台数最多);
  FB-ITHICA 每二进制 100 次 1 小时运行。
- QPool:主跑 CC-Arith(同配置);CC-Mem/MemDiv/Br 只报相对 CC-Arith 的**独有检出**
  (每台至少 20 小时);单台累计 20–100 小时;库与 FB 也是 20–100 小时。
  汇总排除单测试不足 20 小时的服务器。
- 独立第三方复现验证(§5.7 是 Hardware Sentinel 的事;ITHICA 的独立验证是
  单指令/基本块复现器,见 §7.4)。

### 3.3 性能开销测量硬件(原文 Table 6 脚注)

- "Personal, non-industrial, server":Intel Xeon Gold 6226R,2.9GHz,2×16 核,2 线程/核,
  L1 1MB(D+I),L2 32MB,L3 44MB,内存 500GB。
- 软件:LLVM/Clang-14,pass 编译成 .so 挂入 Clang 内建 pipeline。

### 3.4 评估指标定义(原文 Table 3,每次 run = 1 小时)

| 指标 | 定义 |
|---|---|
| Server Detections | 有错误检出的服务器台数 |
| **EDR**(Error Detection Rate) | 有错误检出的 run 数 / 总 run 数(%) |
| **EF**(Error Frequency) | 错误检出次数 / 总 run 数 |
| **TTD**(Time to Detection) | 从测试开始到首次错误检出的时间 |
| PC Sensitivity(每 opcode) | 失败 PC 数 / 总 PC 数(%)——PC 指程序内唯一静态指令实例 |
| BB Sensitivity(每 opcode) | 失败基本块数 / 总基本块数(%) |
| Input Breadth(每 opcode) | 唯一失败输入数 / 总失败输入数(%) |

## 4. 实验步骤(可操作流程,编号)

1. **准备输入程序**:取 Google cpu-check(CC)、Fleetbench(除 Compression)、五个库
   (Llvmlibc、Llvmlibcpp、Abseil、OpenSSL、Zlib)。
2. **编译插桩**:对每个程序分别应用 7 种变换(Arith / Mem / MemDiv / Br / Arith+Mem /
   Arith+MemDiv / Arith+MemDiv+Br),block size=1、interleaving=1,输出 <name>-ITHICA 或
   <name>-Arith 等测试二进制;构建系统管理的库自动过 pass,显式链接的库手工编译。
   OpenSSL 只施加 Arith/Br(数据竞争问题)。
3. **DPool 基线对照**:每台 DPool 服务器上跑 8 个测试(7 个 ITHICA + 原版 CC),
   每测试 100 次 × 1 小时,run 间重启;区分 ITHICA 检出 / Native 检出(CC 原有最终输出检查,
   在同一 ITHICA 二进制里)/ 无检出崩溃三类;崩溃前有任何检出即算检出。
4. **配置扫描**:CC-Arith 上分别扫 interleaving {1,2,4,8,max} 与 block size {1,2,4,8,dep},
   每配置 100 次 1 小时,统计 EDR(Fig. 6)。
5. **非测试程序插桩**:五个库 + 六个 FB 负载,Arith(1,1),FB 每二进制 100 次 1 小时。
6. **QPool 规模化**:CC-Arith(1,1) 主跑;Mem/MemDiv/Br 只记独有检出;每台 20–100 小时;
   库与 FB 各 20–100 小时;排除 <20 小时/测试的服务器。
7. **错误分解分析**:统计检出时错误落在原指令/验证指令/两者(Table 4);按 opcode 统计
   失败 PC/BB/输入分布(Table 7,PC/BB Sensitivity、Input Breadth)。
8. **指令使用频度检验**:对"仅被一个程序检出且失败 opcode 出现在其他程序"的服务器,
   取最高频失败 opcode,比较各程序中该 opcode 的归一化执行频率(Fig. 7)。
9. **固定输入 TTD 复跑**:对高 EDR 服务器(D3、D4、D7)用新生成固定随机输入重跑 CC-ITHICA,
   观察 TTD 分布是否恒定(检验非架构因素)。
10. **单指令/基本块复现器**:从 CC-ITHICA 程序里剥离出失败指令或失败基本块,
    构造单指令测试与基本块测试;用 Arith 生成(D2/D12 用 Mem/MemDiv);
    分别用真实失败输入与新随机输入,各跑 **100 小时**循环,对比原程序 EDR(Fig. 8)。
11. **SiliFuzz 对照**(§8, Fig. 10):在所有共同测试服务器(每测试累计 ≥20 小时)上比较
    SiliFuzz、CC、CC-ITHICA、FB-ITHICA 的总检出/独有检出/平均 EDR。

## 5. 实验数据(关键数值 + 图表号)

### 5.1 总体检出(§7.1, Fig. 3/Fig. 4)

- **Fig. 3**(DPool,7 个 CC-ITHICA 测试 + CC,D1–D14,每三元组 Ith/Nat/Cr):
  - CC-ITHICA 测试**合计覆盖全部 14 台** DPool 服务器(Obs. 1)。
  - 四个基础变换中 **Arith 最有效,检出 11/14 台**。
  - **Br 在 DPool 0 检出**(但在 QPool 有非独有标记)。
  - **D1、D5、D8、D9 四台只被 ITHICA 检出**,同二进制 Native 检查与原版 CC 都漏(Obs. 2)。
  - 例:D1 上 Arith 的 Ith=100%(9) / Nat=37%(1) / Cr=0;Mem:97(91)/70(66)/3;
    AVX 相关子测试数据见表 8。D9 上 Arith 93(0)/1(0)/1 等(数字取自 Fig. 3 原文)。
  - **D6 只在 interleaving=8 时被 Arith 检出**(D6* 脚注,§7.3)。
  - 组合 pass(Arith+Mem 等)能跨指令类型检出(如 D12 同时算术+访存),但有时 EDR 更低
    或漏掉单 pass 能检的台——插桩指令本身改变了前导指令序列,破坏所需执行上下文(Obs. 11)。
- **Fig. 4**(两池汇总):
  - QPool 中 ITHICA+Native 共同检出 49 台:45 台 Arith 检出、1 台 Mem 独有、3 台 MemDiv 独有(Obs. 3)。
  - 另有 **26 台 QPool 服务器只有 ITHICA 检出**(25 台 Arith、1 台 MemDiv 独有)(Obs. 4);
    仅 6 台 Native 检出而 ITHICA 漏(原因:实现覆盖缺口 + 小 interleaving 下的一致错误,§8)。
  - 两池合计 **CC-ITHICA 检出 89 台(QPool 75 / DPool 14),比同二进制 Native 多 39%**。
  - **FB-ITHICA 再检出 11 台 QPool 独有服务器,总计 100 台**(89+11)。
- **Table 4**(错误落在哪条指令,DPool 全部 run):原指令 31.3%,验证指令 44.7%,
  **两者都错(且错得不同)24.0%**(Obs. 5:两实例都错说明它们与同一缺陷部件交互却仍产生不同输出)。

### 5.2 与 Native/SiliFuzz 的量化对比(§7.1 Table 5,§8 Fig. 10)

- **Table 5**(58 台 ITHICA 与 Native 共同检出;QPool 49/DPool 9),ITHICA/Native 比值:
  | 指标 | 几何均值 | 算术均值 | 中位数 | 最小 | 最大 |
  |---|---|---|---|---|---|
  | EDR | **1.78** | 4.50 | 1.17 | 0.52 | 100.00 |
  | EF | **7.51** | 130.15 | 3.44 | 0.02 | 5396.00 |
  | TTD | **0.68**(即快 1.47×) | 0.77 | 0.56 | 0.33 | 1.72 |
- **Fig. 10**(共同测试服务器,SiliFuzz vs CC vs CC-ITHICA vs FB-ITHICA):
  - **SiliFuzz 检出 42 台 vs CC-ITHICA 71 台**;SiliFuzz 对其检出的服务器平均 EDR 略差于 CC-ITHICA;
  - **FB-ITHICA 检出 7 台被所有其他测试漏掉的服务器**;
  - SiliFuzz 的 TTD/EF 未采集。ITHICA 比 SiliFuzz 多检 69%(摘要)。

### 5.3 开销(原文 Table 6,vs CC,Xeon Gold 6226R)

| Pass | 性能开销 | 二进制膨胀 |
|---|---|---|
| Arith(block size=1) | **2.18×** | 8.47× |
| Arith(bs=2) | 1.79× | 5.88× |
| Arith(bs=4) | 1.64× | 4.59× |
| Arith(bs=8) | **1.52×** | 4.11× |
| Arith(dep) | 1.75× | 6.11× |
| Mem | 1.17× | 3.06× |
| **MemDiv** | **53.67×** | 13.12× |
| Br | 1.17× | 1.24× |

### 5.4 配置扫描(§7.2, Fig. 6)

- block size=1 平均 EDR 最好(逻辑错误掩蔽最少);但除 D10、D14 外,
  所有在 bs=1 有可复现检出(>1 次)的服务器在更大 block size 下也能检出(Obs. 9)——
  **检查频率可放宽而覆盖不明显受损**;Arith 开销从 bs=1 的 2.18× 降到 bs=8 的 1.52×(Table 6)。
- interleaving=1 的自然多样性对多数 DPool 服务器已足够;更大 interleaving 对 D3、D4 提升 EDR;
  **D6 仅在 interleaving=8 时被检出**(Obs. 14)。

### 5.5 每服务器/每 opcode 的检出细节(Table 7、Fig. 5、Fig. 8、Fig. 9)

- **Table 7**(PC/BB Sensitivity、Input Breadth;有检出的 opcode-服务器组合;只列关键行):
  - 整数类:add int(D3):1202 PC 中 5 失败,PC Sens 0.42%;zext int(D3):809 PC、46 失败、5.69%;
    udiv int(D1):36 PC、1 失败、2.78%;getelementptr(D1/D3/D8):5568 PC、6/1/1 失败、几何均值 0.03%。
  - FP:**fadd x86_fp80(D5)**:4 PC、1 失败、25%;**fmul x86_fp80(D9)**:4 PC、1 失败、25%,
    17.92M 总失败输入中 1330 个唯一失败输入,Input Breadth 0.01%。
  - 向量整数:add<4xint>(D1):11 PC、2 失败、18.18%;or<4xint>(D1):2 PC、1 失败、50%。
  - 向量双精度:fmul<4xdbl>(D10/D11):61.24%(几何均值);fma<4xdbl>(D10/D11):73.95%;
    **fmul<8xdbl>(D4/D11):PC Sensitivity 100%**(8 PC 全失败),20.10B(D4)/169,199(D11)
    总输入对 342,023/838 唯一失败输入;fneg<8xdbl>(D4):35.36%;fma<8xdbl>(D4/D11):70.71%。
  - 访存:load(D2/D3/D12):4481 PC、1/66/2 失败,几何均值 0.11%;
    store(D2/D3/D12/**D13**):3015 PC、1/19/1/1 失败,几何均值 0.07%。
  - 结论:多数 opcode PC/BB Sensitivity 极低(常 <1%,错误集中在极少数 PC/BB),
    Input Breadth 高(错误不局限于少数"罪魁"输入);双精度向量 opcode 例外
    (高 PC Sensitivity,但其 PC 全部集中在一个基本块内,执行上下文同质)。
- **Fig. 5**(DPool 全部 CC-ITHICA run 的失败 opcode 分布 + 平均 EF):受影响指令横跨
  算术、浮点、向量、访存;平均错误频率从一个 run 不到一次到一千多次不等(Obs. 7)。
- **Fig. 8**(单指令/基本块复现器,原程序 vs 复现器 EDR,真实失败输入/随机输入两版):
  - **通常单指令与基本块测试都测不出错误,即使给真实失败输入**(Obs. 18)。
  - 例外 1:**D3 单指令复现不了、基本块测试可以**(失败 BB 长 17/13.5/25.0 条指令,
    通过 BB 长 12.0/1.0/16.2)——需要中等长度序列建立执行上下文。
  - 例外 2:**D9(fmul x86_fp80)单指令测试 + 真实输入达到完美复现(EDR 100%)**。
    机理分析(§7.4):LLVM IR 复现器只编译成**一条汇编指令 fmul x86_fp80**;
    按 uops.info 与该(未公开)微架构文档,该指令映射**单 micro-op**;
    该 micro-op 可在**两个 FPU 之一**执行。原/验证指令频繁同时错但错得不同
    → 很可能都命中同一缺陷部件(两个 FPU 之一)。ITHICA 全套 CC-ITHICA 只在
    fmul x86_fp80 上检出 D9 的错误 → 部件大概率是 FPU 之一。
    但作者强调 ISA 层硬件定位本质不透明(§7.5)。
- **Fig. 9**(93/100 台检出服务器的失败指令类型组合):
  - **44% 的服务器错误跨多个指令类型**而非单一类型(Obs. 20);
  - **92.9% 的错误源自浮点与向量指令**(Obs. 21)——算术与访存指令虽在多数检出服务器中出现,
    贡献的错误次数占比却很小;
  - **浮点指令平均出错频率比访存指令高 6 个数量级**,但受影响服务器数更少(26 vs 28)(Obs. 22);
  - 向量指令在 46 台服务器出错,其中 **31 台同时有其他指令类型出错**(Obs. 24)——
    直接挑战 SEVI "向量错误→向量单元缺陷" 的定位结论(Obs. 23:D1 只被 CC 的 AVX 子测试
    这个向量专用测试检出,但 ITHICA 在同一测试内揭示非向量指令也在出错)。
- **Table 8**(两池、各 CC 子测试/库/FB 的总检出与独有检出):
  - 总检出:AVX=19,Hasher=3,Pattern_Gen=21,Malign_Buff=17,Silkscreen=6,Utils=4,
    Zlib=23,OpenSSL=9,Abseil=7,Llvmlibc=0,Llvmlibcpp=3;
    FB:Swissmap=4,Proto=8,Libc=3,Tcmalloc=9,Hashing=11,Stl-Cord=7。
  - 独有检出:AVX=15,Pattern_Gen=7,Malign_Buff=3,Silkscreen=3,Zlib=4,OpenSSL=1,
    FB(Swissmap=1,Tcmalloc=3,Hashing=3,Stl-Cord=3)。
  - **Zlib 总检出最多(23)**;Native 漏掉的 31 台中 **8 台只在库代码内被 ITHICA 检出**
    (6 台仅 Zlib、1 台仅 OpenSSL、1 台两者)(Obs. 12);**AVX 子测试独有检出最多(15)**。
- **Fig. 7**(指令使用频度):22 个"仅一个程序检出"的案例中,**13 个(59%)检出的程序并不是
  失败 opcode 执行频率最高的程序**(Obs. 16)。
- 固定输入重跑 TTD(D3/D4/D7):TTD 分布仍高度可变(D3 从数秒到接近整小时)(Obs. 17)。

### 5.6 与先前集群研究的对照(原文 Table 9)

| 维度 | Alibaba[82,83] | SEVI[51] | ITHICA |
|---|---|---|---|
| 嫌疑缺陷池规模 | 不清 | >2500 | **>3000** |
| 检出并分析服务器数 | 27-30 | 18 | **100** |
| 错误假设 | 一致(隐含) | 一致(显式) | 不一致(真机验证) |
| 检查的指令类型 | 非指令级 | 仅向量 | 全部(Arith/FP/Vec/Mem/CF) |
| 错误检查技术 | 最终输出+golden | 指令级+golden | 线程内指令级 |
| 指令定位 | 事后靠使用频度 | 与检出同时 | 与检出同时 |
| 检出决定因素结论 | 指令使用频度 | opcode/输入/温度 | **序列驱动的执行上下文**;频度/opcode/输入单独不足 |
| 脆弱硬件单元结论 | ALU/FPU/向量单元 | Cache/事务内存 | 向量乘法器 | 显示先前定位结论不可靠 |
| 提出的方案 | 聚焦脆弱特性 | matmul 内核 ABFT | 多样程序当测试 + 检出同时定位指令 |

## 6. 实验结论(编号列出)

1. **Finding 1**:在我们测试活动中被标记为缺陷的服务器,几乎所有缺陷(永久故障)
   都以不一致错误形式表现(Obs. 1–5)。
2. **Finding 2**:ITHICA 检查在所有评估指标(EDR/EF/TTD)上超过超大规模集群测试使用的
   最终输出检查(Obs. 2、4、6)。
3. **Finding 3**:ITHICA 检出的错误最符合"制造缺陷"这一根因(频率、可重复性、
   器件特异性、跨机龄分布;Obs. 7、8)。
4. **Finding 4**:ITHICA 插桩与运行时开销可以在保持高检出覆盖下降低(Obs. 9)。
5. **Finding 5**:指令是否出现缺陷性错误,取决于它是否运行在由**前导指令序列**塑造的
   脆弱上下文中(Obs. 10–13)。
6. **Finding 6**:缺陷性不一致错误的检出依赖原/验证指令之间(通常适度的)执行上下文变化
   (Obs. 14、15)。
7. **Finding 7**:缺陷检出的程序敏感性不能由失败 opcode 的动态频率解释(Obs. 16)。
8. **Finding 8**:缺陷性错误通常无法靠 ISA 级控制确定性复现(Obs. 17、18)。
9. **Finding 9**:对脆弱指令而言,选择"完美"输入既非必要也非充分(Obs. 17–19)。
10. **Finding 10**:同一缺陷常以显著不同的速率(相差达 6 个数量级)跨多个指令类型致错
    (Obs. 20–24)。

## 7. 复现要点(ARM64 复现最小版本)

### 7.1 可直接复用

- **全部思想与协议**:指令复制+输出比对、EDR/EF/TTD 指标、100×1h/台运行协议、
  run 间重启、interleaving/block-size 扫描、PC/BB Sensitivity 与 Input Breadth 分析框架。
  这些与 ISA 无关,SDCShield 可直接照搬定义。
- **被测程序选型**:zlib/zstd 压缩-解压、OpenSSL 加密-解密(SDCShield 已有
  zlib/zstd19/openssl_sha/ipsec 全套)、Abseil 类哈希、Fleetbench 式热点微基准
  (Proto/Hash/Tcmalloc 有开源实现)。
- **Br 变换**:条件分支目标校验,LLVM IR 层与架构无关,ARM64 直接可用。
- **Arith 变换**:LLVM IR 的 op/icmp/fcmp/select/GEP 复制,IR 级目标无关;
  volatile store-load 防优化技巧同样适用。
- **复现器方法论**:从失败测试剥离单指令/基本块复现器、真实失败输入存档重放、
  100 小时循环对比 EDR——SDCShield 的 fork-per-test + CrashContext 框架天然支持。
- **开源输入**:cpu-check、Fleetbench、SiliFuzz、五个库全开源,可获取。
- **Mem 变换(非 MemDiv)**:load 复制与 store 回读在 ARM64 完全可行。
- 错误分解统计(原/验证/两者,Table 4)可直接照搬。

### 7.2 需替代/不可行

- **代码本身**:"We will make our repository public upon publication"(§4 脚注 2)——
  截至 arXiv v1(2026-05)**尚未公开**,需自行实现 LLVM pass(工作量:4 个 pass + 组合,
  Clang-14 pipeline 集成)。
- **MemDiv 的 clflush**:**ARM64 无 clflush**。替代:`dc civac`(cache invalidate to PoC,
  需内核或特权;用户态不可直接执行)、`dc cvau` + 自陷调用,或 Linux 用户态等价:
  `__builtin___clear_cache`(只对 I-cache 保证)、映射 MAP_SHARED 落盘强制回写、
  `cachestat`/`seek+read` 绕 cache 途径有限。**最小可行替代**:
  (a) `msync`/`fsync` 强制脏行落盘 + `madvise(MADV_DONTNEED)` 丢弃页面后重取
  (强制主存/页表路径,等价"clflush 后 load");(b) 对 store 用 `dmb st`(≈mfence 的
  store 序语义,ARM64 上 `dmb ishst`)清空 store buffer 视图后再 load;
  (c) NEON/SVE 覆盖不同向量寄存器宽度的"数据路径多样性"。
  在 SDCShield 里更容易的做法是直接实现 **MemDiv 语义**:store→load(可能命中
  store buffer)→dmb→load(L1)→MADV_DONTNEED+load(主存)三段四值比对。
- **x86_fp80(fadd/fmul x86_fp80)**:x87 80 位扩展精度是 x86 独有;ARM64 无对应物。
  D5/D9 案例的"单指令完美复现"依赖 fp80 单微 op + 双 FPU 的特性,ARM64 上的对应实验
  应换成 **fmul s/d(标量 F64)** 或 **fmul z0.d(SVE 2048-bit)**,并用 uops.info 的
  ARM 等价物(如 llvm-mca / Arm 的优化指南)确认单微 op 归属。
- **3000+ 台 QPool / 20 台 DPool / 每台 2000+ 小时**:超大规模集群资源不可得。
  最小替代:一批确认缺陷机(哪怕是 1-3 台已知故障机,如 NUMA3 VA-path 故障机)+
  100-500 小时/台 + 10 台健康机做假阳性对照。DPool 里 6/20 台测不出错误——
  说明"确认缺陷"标签本身有噪声,复现时要预留此现象。
- **每 run 之间重启服务器**:单机实验可改每 run 重启或每次
  重新 fork(SDCShield fork_each_test 天然接近,但不是整机重启——注意差异)。
- **CC(cpu-check)基线**:开源可用,但其 AVX 子测试在 ARM64 不可用
  (需换 SDCShield 自有向量测试或 SVE 等价物)。基线应改为 SDCShield 原生
  golden-value 检查(与本仓库 memcmp_or_fail 语义一致,正好是论文里的 "Native checks")。
- **SiliFuzz 对照**:SiliFuzz 是 x86 fuzzer,ARM64 上等价物缺失
  (可用 Harpocrates 思路或自行 fuzz,但工作量另计)。
- **温度控制**:论文明确不做(§8 Effect of Temperature),复现无需此维度。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield 新实验设计,可用以下对比轴(论文基线数值已给):

| 对比轴 | 论文基线值 | SDCShield 可比实验 |
|---|---|---|
| 检出率提升(相对同二进制 Native/golden 检查) | +39%(89 台 vs Native;摘要/§7.1) | 新实验 vs SDCShield golden 检查的检出台数/EDR 比 |
| EDR 几何均值比(ITHICA/Native) | 1.78×(Table 5) | 同机同时长下新检查与 golden 检查的 EDR 比 |
| EF 几何均值比 | 7.51×(Table 5) | 单位时间错误暴露次数比 |
| TTD | 0.68×(快 1.47×,Table 5) | 首错时间;新实验应更快激发 |
| vs SiliFuzz 检出台数 | 71 vs 42(共同服务器,Fig. 10) | 新实验 vs 短随机测试(可自造 mini-fuzzer) |
| 性能开销(Arith) | 2.18×(bs=1)→1.52×(bs=8)(Table 6) | 新实验的吞吐代价 |
| MemDiv 开销 | 53.67×(Table 6) | 主动多样性策略的代价上限参考 |
| 复现器成功率 | 仅 1/14 台(D9)单指令可复现;D3 需基本块(§7.4, Fig. 8) | 新实验是否更易单指令/短序列激发 |
| 库代码独有检出 | 8/31 Native 漏检台在库内(Zlib 6+OpenSSL 1+双 1)(Obs. 12) | SDCShield 第三方库测试(openblas/sleef/pocketfft)的意义佐证 |
| 错误指令分布 | 92.9% 错误来自 FP/向量;FP 频率超访存 10^6×(Fig. 9) | SDCShield 向量 FMA/SVE 测试占比的依据 |
| 跨指令类型证据 | 44% 服务器多类型;向量 46 台中 31 台伴其他类型(Obs. 20/24) | 反驳"单一单元定位"的论证素材 |

## 9. 局限与坑

1. **代码未公开**("upon publication"),复现需自己写 LLVM pass;Table 1 的规则
   足够重建核心逻辑,但 Br 的"intended target 记录到栈再比对"实现细节需自行打磨。
2. **一致错误检测不到**:ITHICA 显式放弃一致错误(§3.1/§8)——若缺陷产生一致错误,
   复制比对会两次错得一样而通过。6 台 Native 检出而 ITHICA 漏的服务器可能正是此类。
   论文建议未来用 functional diversity(ED4I 类)补齐。
3. **覆盖缺口三 sources**(§8):原子/volatile 访存、线程不安全代码(OpenSSL 类)、
   未插桩的 Libc/Libc++ 部分(系统调用内核路径完全未检查)。
4. **线程不安全程序只能上 Arith/Br**,Mem/MemDiv 会因数据竞争误报。
5. **MemDiv 开销 53.67×**,不适合在线场景,只能离线 Dedicated 测试。
6. **每 run 重启、2000+ 小时/台**的成本在学术环境不可复制;EDR 数值与运行时长强相关,
   引用其 EDR 绝对值时要注明条件。
7. **插桩改变指令序列**(Obs. 11):组合 pass 反而可能漏检单 pass 能检的缺陷——
   复现时不能假设"变换越多越好",要保留单变换对照。
8. **inline asm/intrinsic 的副作用验证**靠 LLVM 属性,不完备;错误上报分支本身也可能
   被缺陷打穿(Fig. 5 有 no-instr 类:检测指令自身错或日志未写完就崩溃)。
9. **x86 中心**:fp80 案例不可移植;Fig. 5/Table 7 的 opcode 分布是 x86+LLVM 降低的结果,
   ARM64 上同一缺陷的 opcode 表征会不同(编译器映射差异是作者自己承认的
   "微架构执行路径非确定性"来源之一)。
10. **"检出"定义依赖错误上报路径存活**:崩溃先于日志完成会丢数据(Fig. 5 no-instr、
    Fig. 9 排除 7 台)。SDCShield 的 CrashContext 机制可部分弥补。
11. **DPool 20 台中 6 台全军覆没**(任何测试测不出),复现小样本时必须预期
    "已确认缺陷"里也有测不出的比例(6/20 = 30%)。
12. **QPool 检出统计排除 <20h/测试的服务器**,直接引用 Fig. 4 数字时注意该过滤条件。

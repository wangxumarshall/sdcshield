# Veritas《Demystifying Silent Data Corruptions: µArch-Level Modeling and Fleet Data of Modern x86 CPUs》(HPCA 2025,University of Athens + Meta Platforms)

> 复现档案 E06。所有数值均摘自 PDF 原文并标注图表号/节号;读不到的写"原文未给出"。
> 论文:Odysseas Chatzopoulos, Nikos Karystinos, George Papadimitriou, Dimitris Gizopoulos (Athens), Harish D. Dixit, Sriram Sankar (Meta)。2025 IEEE HPCA,DOI 10.1109/HPCA61900.2025.00012。14 页。注:文件名虽无年份,版权页/DOI 明确为 HPCA 2025。

## 1. 研究问题与核心贡献(≤5 行)

填补两个空白:(a) 算术单元(标量/向量 × 整数/浮点)从未被作为 SDC 根因做过 µArch 级系统分析;(b) 大机队的 SDC 事件率从无公开数据。方法 = **双子实验**:gem5 全系统门级 stuck-at 故障注入(≈6750 万次注入 ≈ 22.5 万颗"虚拟坏 CPU")+ Meta 真实机队 6 年遥测(数十万 CPU、数十亿 CPU 小时),两边独立执行后交叉验证。核心量化结论:向量单元(尤其 FP 向量)SDC 率比标量整数加法器(基准=1)高 2–3 个数量级;五 x86 µArch 相对 SDC 率 1.00–1.51×;乘法器实现(Array/Wallace/Dadda)对 SDC 概率影响极小。

Table I 列出的研究问题清单(§I,本文回答后四个):

| 研究问题 | 是否已回答 |
|---|---|
| 产生 SDC 的 CPU 占比 | 部分(Meta/Google/Alibaba 先行报告) |
| **SDC 事件率** | Open(本文回答) |
| **SDDC 易发 µArch 排名** | Open(本文回答) |
| **SDC 易发算术结构排名** | Open(本文回答,Alibaba 仅给过粗粒度提示) |
| **SDC 易发指令类排名** | Open(本文回答) |

## 2. 实验方法(µArch 级建模: fault site 定义、传播分析;检测机制;输入;循环)

### 2.0 双实验独立性原则(§I,方法学上最重要的一句话)

两个实验**刻意独立执行**("to not bias the results of one towards the specifics of the other"):仿真实验产出细粒度信息(故障如何在硬件单元中显形并传播到程序输出——机队实验不可能做到),并用 CPU 时钟频率、指令吞吐、单元尺寸、负载执行时间把 PSDC 换算成 SDC 率;机队实验产出真实缺陷率(仿真不可能做到)。最后才交叉组合。复现其方法学时必须保持这种"先独立、后组合"的结构,否则相关性是自我实现的。

### 2.1 故障建模五步法(§III-A/III-B/III-C,重点)

1. **用 ArithsGen 生成功能单元的门级 C++ 模型**:gem5 原生对算术单元只有功能级模拟(如整数加法就一句 `result = PSrcReg1 + PSrcReg2;`,§III-B)。ArithsGen(可配置算术电路生成器)生成加法器/乘法器/除法器等多种实现的门级 C++ 模型(Verilog/BLIF/C++,本文用 C++);每行代码 = 电路中一个门(Fig. 2 为 64-bit 超前进位加法器片段,Fig. 3 为同一段插桩后)。
2. **插桩 fault site**:自定义 Python 模块解析生成的 C++ 代码,识别代表门的语句,插入"wrapper code"模拟物理故障。**fault site = 算术单元门级模型中的每一个门**(gate)。stuck-at 模型实现:`pg_logic25_or0 = (faulty_gate == 269) ? 1 : 原逻辑;`(stuck-at-1,Fig. 3)——通过参数 `faulty_gate` 选定注入门。
3. **集成进 gem5(micro-op 级)**:x86 宏指令在 gem5 中拆成 RISC 型 micro-op,在 OoO 核的 Issue-Execute-Writeback(IEW)阶段执行;拦截每条 micro-op 的"execute function",把输入与**预期正确输出**一并送入注入控制器;若该 micro-op 调度到目标功能单元,则激活门级模型执行,输出与预期比较留档后再返回给 micro-op(§III-D)。只在命中目标单元时才调用门级模型,吞吐损失 **< 4%**(Fig. 4:Base 1.00 → VFMUL 0.96;ADD 1.00/MUL 0.99/FADD 0.99/FMUL 0.98/VADD 0.99/VMUL 0.97/VFADD 0.97)。对比:先前工作用外部门级模拟器,简单整数 ALU 就要 220% 惩罚 [34]。
4. **自定义注入控制器**(gem5 内)处理注入。
5. **并行注入战役管理器**(Python):吃进目标 µArch + benchmark 列表 + 注入数 + 故障模型,自动解析 µArch 的功能单元数量/类型并展开战役(§III-E)。

### 2.2 传播分析与结果分类(§III-E,Fig. 5)

- **golden run**(单次无故障执行)先跑一遍,收集功能单元活跃度;若某单元全程未被使用 → 不注入,该单元所有 fault 记为 Masked(故障不可能传播到输出)。
- 注入流程(Fig. 5):配置 → 生成 fault list → 并行 gem5 实例(每个注入一个随机选定的门)→ 记录结果 → 与 golden 输出比对 → 分为 **SDC / Crash / Masked**(软/硬件级)。
- 额外产出:**error profile**(功能单元输出有多少比特、哪些位置被影响)+ **instruction error rates**(每条指令造成错误计算的比例,见 §2.6)。
- 机队侧补充关键数字:**Meta 机队中系统崩溃的发生频率至少是 SDC 的 2–3 倍**(§III-E 原文 "system crashes occur at least 2 to 3 times more frequently than SDCs")——Crash 定义上可被检出并换件,故论文聚焦 SDC。

### 2.3 SDC 率计算公式与传播分析产出(§III-E,式 1–4)

结果解析后产出三件套:**PSDC**(每负载)、**error profile**(输出比特影响数量与位置)、**instruction error rates**(指令级)。式(1)–(4) 把 PSDC 升维为时间维度 SDC 率:注意式(1) 中 t/Tw 因子的含义是"时间预算内负载重复执行的次数"——即 SDC 率按"单位时间内的损坏输出执行数"计,不是按"每次执行损坏概率"。式(4) 的 λbad·c 即机队核数中缺陷核的绝对数。

- 式(1) SDCR(u,w,t) = (t/Tw) · PSDC(u,w) · PFaulty(u) ——单元 u 跑负载 w 共 t 秒;Tw = 单次运行时长;PSDC = u 中故障导致 SDC 的概率;PFaulty(u) = 核有缺陷时缺陷落在 u 的概率。
- 式(2) 负载集合 W 轮转跑 t 秒(每个 t/|W|)求和。
- 式(3) Core R(FU,W,t) = 对核内所有功能单元求和。
- 式(4) Fleet R(CPU,c,W,t) = λbad · c · Core R ——λbad 为缺陷核占比,c 为核总数。

### 2.4 建模假设(§III-F,重要口径)

1. **永久 stuck-at 故障,门级注入** → 结果是 SDC 率的**悲观上界**(真实缺陷多为边际/间歇/小延迟,烈度低于 stuck-at)。
2. **PFaulty(u) ∝ 单元相对面积(门数占比)**——每门等概率含故障;不做老化分布(缺厂商/代工统计)。
3. **只给相对 SDC 率**(绝对 DPPM 保密):λbad 与 t 在相对化后消去;µArch 间、单元间、指令类间比值为有效信息。

### 2.5 机队实验口径(§III-G,Table IV/V)

机队 SDC 数据的两个来源与用途(§III-G):

- **生产负载监测**:fleet-scale 监控系统跟踪生产负载与系统状态(利用率、故障发生、故障修复),6 年+ 遥测 → 量化"按配置/供应商/软件分层的故障发生与复现率";但因 co-location/容器化/保密,生产负载数据不能直接发表。
- **定向测试框架**:Fleetscanner(出生产)+ Ripple(在生产),可变运行时长;含**特制复现器(reproducers)+ 位级随机化变体**覆盖多种功能块,最大化测试有效性。
- 对检出 SDC 的芯片做日志深挖,建立 SDC 事件 ↔ 硬件单元/根因关联,识别复现模式。

- 遥测 6 年+,数百上千台规模的分析贯穿;SDC 数据同时来自**生产实例**与**测试框架**。
- **定制"锤击"测试(指令病毒/stress programs)**:Meta 自研、专门锤打 x86 算术单元、输入数据**随机化序列**(bit-level randomization);"designed at Meta to stress the arithmetic units of x86 CPUs by 'hammering' them with randomized sequences of input data"(§III-G)。Fig. 8/Table V 的数据来自这些定向测试 + 对表现出 SDC 的芯片的日志深挖。
- Fleetscanner(出生产测试)vs Ripple(在生产测试)执行参数(Table IV):

| 属性 | Fleetscanner | Ripple |
|---|---|---|
| #1 单次测试运行时长 | **分钟级**(order of minutes) | **毫秒级**(order of milliseconds) |
| #2 全机队覆盖节奏 | ≈60 天 | ≈7 天 |
| #3 检出缺陷占比 | **≈93%** | **≈77%** |
| #4 目标缺陷家族/功能块数 | ≈30 | ≈5 |
| #5 检出故障的架构/世代数 | ≈10 | ≈7 |
| #6 执行模式 | 非生产、无负载状态 | 与生产负载同址共存(co-located) |

- 统计规则:测试或生产中**出现一次数据损坏即判该设备为坏**(one defect per chip 假设,检出即出队);取每个 CPU-故障组合最新快照 + 历史数据保证代表性。
- 选 5 个 µArch 建模的原因:机队中样本量最大。

### 2.6 指令级错误率(§IV-F)

- 定义:**单元边界处**统计——该指令引起的错误计算次数 / 该指令引起的单元总计算次数(x86 指令拆 micro-op,一条指令可多次激活单元)。
- 只统计**结果为 SDC 的注入实验**中的指令错误率(关心 SDC 情形的指令失败画像)。
- 同类单元聚合(ADD0–ADD4 归为 ADD 类)。

## 3. 实验配置(硬件/软件/参数)

### 3.1 仿真的 5 个 x86 µArch(Table III,§III-A)

| 规格 | CPU A (2014) | CPU B (2020) | CPU C (2020) | CPU D (2019) | CPU E (2015) |
|---|---|---|---|---|---|
| L1 D/I Cache | 32KB 8-way | 同左 | 同左 | 同左 | 同左 |
| L2 Cache | 256KB 8-way | 512KB 8-way | 1MB 16-way | 512KB 8-way | 1MB 16-way |
| 物理寄存器堆 | 168 Int;168 FP | 192 Int;160 FP | 180 Int;168 FP | 180 Int;160 FP | 168 Int;168 FP |
| LQ/SQ/IQ/ROB | 72/42/64/192 | 44/48/148/256 | 72/56/97/224 | 44/48/128/224 | 72/56/97/224 |
| 标量整数 FU | 4 Add;1 Mul | 5 Add;1 Mul | 4 Add;1 Mul | 4 Add;1 Mul | 4 Add;1 Mul |
| 标量 FP FU | 1 Add;1 Mul | **2 Add;2 Mul** | 1 Add;1 Mul | 1 Add;1 Mul | 1 Add;1 Mul |
| 向量整数 FU | 2 Add;1 Mul | 4 Add;2 Mul | 3 Add;2 Mul | 3 Add;1 Mul | 3 Add;2 Mul |
| 向量 FP FU | 1 Add;2 Mul | 2 Add;2 Mul | 2 Add;2 Mul | 2 Add;2 Mul | 2 Add;2 Mul |

- 功能单元数:每 µArch **12(CPU A)至 17(CPU B)个**。
- 仿真吞吐:gem5 周期级约 **600K 指令/秒**;真实 CPU 1.6–3 GHz(§I 末)。仿真对缺陷分析吞吐的效率是机队的 ~1000 倍。
- gem5 模拟完整 Linux OS 全系统执行(§III 开头)。

### 3.2 负载(§IV 开头,30 个)

5 个非重叠类别:Searching & Sorting(qsort/dijkstra/trie 等)、Compression & Hashing(zstd/zlib/sha512/crc32/md5sum 等)、Image Processing(jpeg 编解码/边缘检测/平滑等)、Floating-Point Heavy(Eigen 矩阵乘/SVD/稀疏求解 + FFT/多项式求根/sqrt 逼近)、Integer Heavy(上类的整型版本)。选型偏数据密集型(更能显影算术单元数据损坏)。

### 3.3 注入参数(§IV 开头)

- 总规模:**≈67,500,000 次注入运行** ≈ 模拟 **≈225,000 颗坏 CPU**(5 µArch × 12–17 FU × 30 负载)。
- 每单元每 µArch:**3000 次注入 = 1500 门 stuck-at-0 + 1500 门 stuck-at-1**。
- 统计置信度:99% 置信区间、< 2% 误差边际 [Leveugle et al.]。

### 3.4 机队(§I/§III-G)

- 数十万 CPU、6 个日历年、数十亿 CPU 小时;绝对 DPPM 因保密未给出(§IV-E)。

### 3.5 未给出的配置

- gem5 版本、宿主机规格、Linux 发行版/版本、ArithsGen 生成的具体电路参数(位宽/结构除 64-bit CLA 示例外未给出)、30 个负载的具体清单全集、每负载运行时长 Tw、注入控制器采样细节、机队定向测试的具体指令序列 —— **均原文未给出**(部分仅有类别性描述)。

## 4. 实验步骤(可操作流程)

仿真侧(gem5 门级注入战役,§III-A 步骤 1–5 + §III-E):
1. 选定 µArch 配置(Table III 规格写进 gem5 OoO 模型);
2. ArithsGen 为每类算术单元生成门级 C++ 模型(可选 Array/Wallace/Dadda 等不同实现);
3. Python 插桩器给每个门包上 stuck-at-0/1 的条件逻辑(参数 faulty_gate);
4. 把模型挂到 gem5 IEW 阶段各 FU 的 execute function 路径上(仅在 micro-op 调度到该 FU 时激活);
5. 配置战役:µArch × FU × benchmark 数 × 故障模型;跑 golden run 收集 FU 活跃度,零活跃单元不注入(记 Masked);
6. 并行派生 gem5 实例,每实例随机选门注入,stuck-at 直到运行结束;
7. 程序跑完,输出与 golden 逐位比对 → SDC/Crash/Masked 分类;记录 error profile(错误比特位置/数量)与 instruction error rates;
8. 用式(1)–(4) 把 PSDC 换算为相对 SDC 率(面积加权 PFaulty;等 DPPM 假设下 µArch 间相对比较);
9. 敏感性验证:同一 µArch 换三种乘法器实现重跑,验证 PSDC 对门级实现不敏感(Fig. 7)。

机队侧(Meta,§III-G):
10. 常年运行 Fleetscanner(出产、分钟级、60 天覆盖)与 Ripple(在线、毫秒级、7 天覆盖)定向锤击测试(随机化输入序列,针对 ~30 / ~5 个功能块家族);
11. 一次损坏 = 判坏出队;快照 + 历史聚合;
12. 对坏芯片日志深挖,建立 SDC 事件 ↔ 硬件单元/指令族关联(Table V "Main suspect executions" 列);
13. 交叉验证:仿真排名(Fig. 6)vs 机队定向测试排名(Fig. 8);μArch 内在差异(Fig. 9,等 DPPM)vs 乘上机队实测相对 DPPM(Table V)后的排序(Fig. 10)。

## 5. 实验数据(关键数值)

### 5.1 硬件单元 SDC 易感性与保护(Table II,§II-C)

| 硬件单元类 | SDC 可能性 | 常规保护 |
|---|---|---|
| 指令 cache | 小 | 轻~强 |
| 数据 cache | 中 | 轻~强 |
| 寄存器/缓冲/队列 | 小 | 弱~轻 |
| 控制逻辑 | 小 | 无~弱 |
| 整数标量算术 | 中 | 无~弱 |
| **浮点标量算术** | **大** | 无~弱 |
| **整数向量算术** | **大** | 无~弱 |
| **浮点向量算术** | **大** | 无~弱 |

### 5.2 相对 SDC 率热图(Fig. 6,§IV-A;5 张热图 = 5 负载类别,y 轴 5 µArch,x 轴各 FU)

- 基准值 = 整数加法器跑 Search & Sort 的绝对 SDC 率(全图最小);色标按相对值,格值 100 = 基准的 100 倍。
- Search & Sort:整数加法器最低;**整数乘法器比加法器高 2 个数量级**(乘法结果更少用于地址/控制流);该类负载不用 FP/向量单元(绝对 SDC 率为 0,图中不显示)。
- Compression & Hashing:整数加法器的率约为 Search&Sort 中的 **近 30 倍**(压缩哈希控制流极少、数据流均匀);向量整数加法器呈现高 SDC 率。
- Image Processing:所有标量/向量 int/FP 单元都有显著活跃;向量 FP 乘法器脆弱性最高,向量整数加法器次之。**乘法器平均比加法器大 30 倍(门数),含故障门的概率也高 30 倍**;标量 vs 向量加法器 SDC 率差 1 个数量级。
- Integer Heavy:唯一使用整数向量乘法单元的类别,其 SDC 率非常高;向量整数加法器在五类中最高。
- Floating-Point Heavy:所有 FP 单元 SDC 率暴涨;矩阵乘中一次损坏的乘法结果**几乎必然**进入结果数组且**永不导致崩溃**;CPU B 有 2 组 FADD/FMUL 但 SDC 率并不减半(甚至可能更高——永久故障下一半的错误计算已足以污染输出)。
- Obs.#1:标量整数加法器最不可能导致 SDC(损坏值大概率打控制流/非法访存 → 崩溃)。Obs.#2:向量单元(int/FP)导致**数个数量级**更多的 SDC。
- Obs.#4(Fleet+仿真一致):FP 重负载(多为向量化)SDC 率最高。

Fig. 6 五负载类别要点表(相对率,Search&Sort 整数加法器 = 1):

| 负载类别 | 活跃单元 | 关键相对量级 |
|---|---|---|
| Search & Sort | 仅标量 int | int 乘法器比加法器高 **2 个数量级**;FP/向量绝对率为 0(不显示) |
| Compression & Hashing | 标量 int + 向量 int | int 加法器 ≈ Search&Sort 的 **30 倍**;向量 int 加法器高 |
| Image Processing | 全部(int/FP × 标量/向量) | 向量 FP 乘法器最高,向量 int 加法器次之;乘法器比加法器平均大 30×(门数) |
| Integer Heavy | int 全系(唯一用向量 int 乘的类) | 向量 int 乘法率非常高;向量 int 加法器为五类最高 |
| Floating-Point Heavy | FP 全系 | 所有 FP 单元率暴涨;矩阵乘中一次坏乘**几乎必然**进结果数组且**永不崩溃**;CPU B 双 FADD 不减半率 |

机队侧定性对应(§IV-C):机队损坏主要由向量空间 FP 密集运算(并行数学/ML/密码学/大数据搬运)造成;压缩哈希次之(高频+数据异构+顺序依赖放大);图像处理损坏良性(坏像素);标量 FP 也有可观损坏,精度/结果误差不一,影响高于图像处理。

### 5.3 机队定向测试的相对 SDC 率(Fig. 8,§IV-C;**用户点名的 4580 vs 21 类数字**) 

| CPU | ADD | FADD | FMUL | VADD | VFADD | VMUL | **VFMUL** |
|---|---|---|---|---|---|---|---|
| CPU A | 1 | 2 | 3 | 15 | 806 | 1255 | **341** |
| CPU B | 1 | **21** | 2 | 14 | 4761 | 5954 | 2047 | **4580** |
| CPU C | 1 | 18 | 2 | 12 | 4070 | 3871 | 1337 | 2980 |
| CPU E | 1 | 51 | 3 | 64 | 5562 | 3068 | 1433 | 2863 |

(注:PDF 表格解析按列序 ADD/FADD/FMUL/VADD/VFADD/VMUL/VFMUL 排布;精确逐格对应以原文 Fig. 8 热图为准,色标范围约 1–6000。下述比例关系为原文明确陈述。)

- 关键结论(原文,§IV-C):**向量单元、尤其 FP 向量,产生的 SDC 显著多于标量整数/浮点单元;加法器是最弱贡献者(= 相对基准 1);标量 FP 单元(add+mul)比标量整数单元高约 1 个数量级,但远被向量单元超越**。
- 该数据**条件**:来自对机队 CPU 跑"锤击随机输入"的定向测试(Fleetscanner/Ripple 家族),**不是**生产负载自然发生率的直接测量;且与 Fig. 6 不可直接互比(负载性质不同),只比趋势。
- CPU D 无机队数据(统计不显著,§IV-C/§IV-E)。
- 交叉验证矩阵(Table VI):Obs.#1 标量加法器、#2 向量单元、#4 FP 运算 = 仿真 ✓ 机队 ✓;#3 整数乘法器 = 仿真 ✓ 机队 inconclusive;#5 µArch 敏感性 = 机队 inconclusive/not-feasible;#6 DPPM 因素 = 双 ✓。

### 5.4 崩溃 vs SDC(§III-E)

- **Meta 机队:崩溃至少比 SDC 频繁 2–3 倍**(crash 可检出可换件,SDC 才是隐匿主体)。这是论文给出的唯一崩溃:SDC 比例数字。

### 5.5 乘法器实现敏感性(Fig. 7,§IV-B)

- 实验设置:同一 µArch,换三种门级乘法器(Array / Wallace Tree / Dadda)各跑一轮注入战役;Fig. 7 为 SDC 概率(给定乘法器中存在 stuck-at)× 5 负载类别 × 3 实现的柱状对照(轴 0.0–10.0%)。
- 方法论意义:作者拿不到各 x86 µArch 真实功能单元实现,统一用 ArithsGen 模型——本实验证明实现差异不敏感,从而为"统一模型代真实实现"的自洽性背书。

- 三种门级乘法器(Array/Dadda/Wallace)在同一 µArch 上重注入:SDC 概率(给定乘法器中存在 stuck-at)按 5 负载类别 0.0–10.0% 区间,三类实现**仅微差**(柱状对照,精确每柱值未标注;Search&Sort ≈0,其余约 2.5–10%)→ Obs.#3:门级实现选择对 SDC 率影响极小(对使用统一门级模型代替真实厂商实现的自洽性背书)。

### 5.6 µArch 总 SDC 率(Fig. 9,§IV-D)

- 等 DPPM 假设下(λbad 相同):CPU E = 基准;CPU A 1.09×;CPU B **1.51×**(正文另述 "surpassing CPU E by a factor of 1.59×"——摘要性表述与图 1.51× 并存,引用以图为准需注明);CPU C 1.47×;CPU D 1.40×。(柱图标签:A=1.09x, B=1.51x, C=1.47x, D=1.40x, E=Base;正文 §IV-D 文字为 "CPU B surpassing CPU E by 1.59×,CPU A only 3% more than CPU E,C/D +30%/+43%"——两处数字有出入,原文如此,引用时注明。)
- Obs.#5:µArch 选择显著影响 SDC 率;**脆弱单元(标量/向量 FP、向量整数)数量越多,SDC 率越高**(CPU B 此类单元最多,故最高)。

### 5.7 机队实测相对 DPPM 与调整后 SDC 率(Table V,Fig. 10,§IV-E)

| CPU | 相对 DPPM | 主要嫌疑执行单元 |
|---|---|---|
| A | 0.85 | Vector (FP), Vector (Int) |
| B | 0.78 | Vector (FP), Vector (Int), Scalar (FP) |
| C | 0.67 | Vector (FP), Vector (Int) |
| D | N/A(数据不足) | — |
| E | 1(基准) | Vector (FP), Vector (Int), Scalar (FP) |

- 乘上 DPPM 后(Fig. 10):CPU E = 基准;A 0.93×;B **1.19×**;C 0.98×。**A/E 排序翻转、C 上升为次低**——内在(µArch)与外在(制造质量/分 bin/老化)因素可反向作用于排序。Obs.#6:µArch 单独不决定相对 SDC 率差异,必须计入 DPPM。

### 5.8 指令级错误率(§IV-F,Fig. 11;条件:**该单元存在 stuck-at 且注入导致 SDC** 的实验中的统计)

Fig. 11 展示的指令类(按单元分组):Scalar Int ADD 组(add、cmpxchg 等,标度 10⁻⁴)、Vector Int ADD 组(标度高 4 个数量级)、Scalar FP(x87 指令族,16–77%)、Vector FP(addps 等)。"a subset of the results"——完整指令清单原文未给出。

- 标量整数加法器指令错误率极低:**cmpxchg ≈ 1/1M(1e-6),add ≈ 1/31K(约 3.2e-5)**(热图标度 10⁻⁴);更高错误率的加法器注入多导向 Crash。
- 向量整数 Add 指令错误率高出 **4 个数量级**。
- 标量 FP 加法器故障时 x87 指令错误率 **16%–77%**;CPU B 显著更低(2 个 FADD 平摊调度,理想 77%/2=38.5%,实测略偏)。
- 向量 FP 指令错误率反而低于标量 FP 对应物:以 `addps` 为例 4 个 FP 加法并行,stuck-at 通常只影响 4 个结果之一(局部化少量错误比特,多结果同时错统计上不可能)。

## 6. 实验结论(编号)

交叉验证汇总(Table VI):

| 观察 | 仿真数据 | 机队确认 |
|---|---|---|
| #1 标量加法器最不易致 SDC | ✓ | ✓ |
| #2 向量单元致 SDC 多几个数量级 | ✓ | ✓ |
| #3 乘法器实现不影响 SDC 率 | ✓ | inconclusive |
| #4 FP 运算(多为向量化)SDC 率最高 | ✓ | ✓ |
| #5 µArch 敏感性 | ✓ | inconclusive/not-feasible |
| #6 DPPM 必须计入 | ✓ | ✓ |

1. (Obs.#1) 标量整数加法器最少致 SDC——损坏值大概率打控制流/地址 → 崩溃;两类实验一致。
2. (Obs.#2) 向量单元(int/FP)导致数个数量级更多 SDC,结果大概率传播到程序输出;机队确认。
3. (Obs.#3) 乘法器门级实现(Array/Wallace/Dadda)对 SDC 率影响极小;机队 inconclusive。
4. (Obs.#4) FP 重负载(多为向量化)SDC 率最高,机队与仿真一致;机队中损坏主要由向量空间 FP 密集运算造成(并行数学/ML/密码学/大数据搬运),压缩哈希次之,图像处理影响良性(坏像素)。
5. (Obs.#5) µArch 选择显著影响 SDC 率:脆弱单元数量越多率越高(CPU B 最高 1.51×);机队不可行验证。
6. (Obs.#6) µArch 内在差异 ≠ 最终 SDC 排序;乘上实测相对 DPPM 后排序翻转(A/E),设计评估必须结合缺陷率。
7. (§III-F) stuck-at 是悲观上界;PSDC×面积×DPPM 的组合公式可代入任意故障分布/绝对 DPPM 产出绝对率(对厂商可用)。
8. (方法学) 门级模型嵌入 gem5 吞吐损失 <4%(vs 外部模拟器 220%),使"数千万次注入 × 全系统跑到程序结束"成为可能;这是首次对算术单元(而非阵列)做此类分析。

## 7. 复现要点(ARM64 单机/小集群最小可行版本)

### 7.0 最小可行实验包(SDCShield 直接动作)

1. **七单元锤击矩阵**:按 Fig. 8 轴(ADD/FADD/FMUL/VADD/VFADD/VMUL/VFMUL)在 ARM64 上实现对应的 NEON/SVE 定向锤击测试(随机化输入、巨量重复、golden 比对),在同一故障机上跑出"单元 × 相对检出率"表——直接对标 Fig. 8 数据形态。
2. **五负载类别 × 单元矩阵**:用 SDCShield 现有测试映射五类(Search&Sort: 待补/可用 st 等;Compress&Hash: zstd19/zlib/crc/sha;ImageProc: 待补;FP-Heavy: eigen_gemm/svd/sleef/pocketfft;Int-Heavy: 整型 GEMM/crc),矩阵化执行并按类别聚合检出率——对标 Fig. 6 五张热图。
3. **崩溃:SDC 计数器**:利用 fork 模型同时统计 crash 子进程数与 SDC 失败数,报告本机比值,对标"机队崩溃 ≥ 2–3× SDC"。

### 7.1 可直接复用

- **故障分类三分法(SDC/Crash/Masked)+ golden-run 比对**协议:SDCShield 已是"计算 vs golden 字节比对"框架,天然产出 SDC/Masked 语义;Crash 由 fork 子进程模型天然捕获(--on-crash=context)。
- **指令病毒/定向锤击测试形态**:对目标算术单元用**随机化输入序列反复锤打**、观察输出错误——与 SDCShield 的 fma/sve512/crypto 测试同构;Fig. 8 的 ADD/FADD/FMUL/VADD/VFADD/VMUL/VFMUL 七类单元轴可直接作为 SDCShield 测试矩阵的行列设计(NEON/SVE 对应:ADD/FADD/FMUL(SVFMLA 类)…)。
- **error profile(错误比特位置/数量)+ 指令错误率统计**:在 SDCShield 测试中按"错误次数/该指令总计算次数"记录,即可产出 Fig. 11 型画像;ARM64 上用性能计数器或框架内计数器。
- **PSDC → SDC 率的面积加权公式(式 1–4)**:复现"相对率"叙事——单机上可对自家测试集做"每单元激发时长 × 检出概率 × 面积权重"的覆盖率模型,直接对标其 µArch/单元/负载三级排名方法。
- **多负载类别矩阵**:Search&Sort / Compress&Hash / ImageProc / FP-Heavy / Int-Heavy 五类负载 × 单元矩阵——SDCShield 的 eigen/zstd/zlib/sha/crc/jpeg(可加)/GEMM 正好映射;矩阵化跑(单机 -n 1 或小集群)即可复现 Fig. 6 的"单元 × 负载类别"热图(用注入或真实故障样本)。
- **stuck-at 上界论断**:作为新实验设计的目标论据——"真实缺陷烈度 ≤ stuck-at,故我们的激发测试按 stuck-at 可检出标准设计即覆盖更轻缺陷"。

### 7.2 需替代/不可行

- **gem5 门级注入管线(本体)**:ARM64 上原理可行(Gem5-MARVEL、gem5 ARM 模型 + ArithsGen 自写 ARM 单元),但工作量巨大且不是 SDCShield 的路线;替代:用**真实故障机**(cn23154)+ 指令病毒做"实物版注入",或用 QEMU/gem5 功能级做指令画像。6750 万次注入规模单机不可行(其 host 集群规模原文未给出)。
- **Meta 机队 6 年遥测 / Fleetscanner / Ripple / 相对 DPPM**:完全不可得;只能引用其数字作对照。
- **22.5 万颗虚拟坏 CPU**:不可行;单机小集群的复现是"协议级"(公式 + 分类 + 排名方法),不是"规模级"。
- **x86 µArch 规格(Table III)**:ARM64 复现需换成 Kunpeng 920(TaiShan V2:2×304 核等)公开规格;注意其"每门等概率"面积假设在真实芯片上门数不可知,只能做近似(用公开 die shot/单元数估计)。
- **绝对 DPPM/绝对 SDC 率**:论文自己也保密不给;复现必然是相对率叙事。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

补充轴:

7. **单元双重排名轴(仿真排名 vs 机队排名)**:论文方法学 = Fig. 6(仿真,五负载类别)与 Fig. 8(机队定向测试)独立产出单元排名后比对(Table VI 三态:✓✓ / ✓inconclusive / ✓not-feasible)。SDCShield 若同时有"注入式验证"与"真实故障机验证"两条证据链,可复用该三态确认矩阵作为可信度论证格式。

1. **单元级激发强度轴**:论文基线 = 机队定向测试相对率(加法器=1;标量 FP ≈ 高 1 个数量级,如 21 vs 1(CPU B FADD);向量 FP 数千:CPU B VFMUL 4580 / CPU E VFADD 5562 / CPU B VFADD 4761 / CPU B VMUL 5954,色标至 ~6000)。SDCShield 新实验可产出同构"单元 × 相对检出率"矩阵,论证 NEON/SVE 激发覆盖达到/超过该谱系。
2. **标量 vs 向量差距轴**:基线 = 向量比标量高 2–3 个数量级 + 指令错误率差 4 个数量级(标量 add 1/31K vs 向量 int add;标量 FP 16–77% vs 向量 FP 更低但传播更强)。SVE-512 激发实验天然落在此轴上,可测"SVE 宽度 → 检出率"的缩放。
3. **崩溃:SDC 比例轴**:基线 = 机队崩溃至少为 SDC 的 2–3 倍。SDCShield fork 模型可同时统计 crash 与 SDC,检验新实验是否把"可检测崩溃"转化为"可定位 SDC"或反之。
4. **负载类别敏感性轴**:基线 = FP-Heavy 最高、Compress&Hash 次之、Search&Sort 最低(整数加法器为图中最小值)、ImageProc 良性。SDCShield 的 eigen/zstd/sha 测试组合可直接映射五类,对比"同类负载类别下的相对激发效率"。
5. **µArch/实现不敏感性轴**:基线 = 乘法器三种实现 PSDC 仅微差(Fig. 7);µArch 差异 1.00–1.51×(等 DPPM)、0.93–1.19×(乘 DPPM)。若 SDCShield 新实验声称"单元谱系针对性",需对照该轴说明为何差异大(否则审稿人可问"是不是实现不敏感")。
6. **面积加权模型轴**:基线 = 式(1)–(4) + PFaulty∝面积 + stuck-at 上界。新实验的覆盖率/检出率预测可用同公式,与其实测排名对齐。

## 9. 局限与坑

- **stuck-at 上界的双刃**:所有率是悲观上界;真实边际缺陷/小延迟缺陷的行为(温度/频率依赖、间歇性)完全未建模——论文自认"任何统计分布可集成但缺数据"(§III-F)。用其数字当"真实 SDC 率"引用是错的。
- **面积等概率假设**:PFaulty ∝ 门数,忽略老化偏向、版图热点、局部缺陷聚集;对老化场景(ASPLOS24 Vega 的领域)系统性失真。
- **机队数据的口径混淆风险**:Fig. 8 是**定向锤击测试**下的相对率,不是生产负载自然发生率;论文明示与 Fig. 6 不可直接互比。4580 vs 21 这类数字引用时必须带"targeted stress test"限定,否则会被审稿人指出夸大。
- **Fig. 9 数字内部不一致**:图标注 1.51× vs 正文文字 1.59×("surpassing … by a factor of 1.59×")同时出现;引用需注明出处形态。
- **gem5 模型保真度**:5 个 µArch 是"据公开信息建模"(不是厂商 netlist);表 III 的 FU 配置对真实芯片是近似;门级单元模型是通用 ArithsGen 生成物,非厂商实现(论文用 Fig. 7 敏感性实验自辩,但仅覆盖整数乘法器)。
- **CPU D 机队数据缺失**、机队绝对 DPPM 保密 → 相对率链条中 λbad 断点;跨 µArch 排序(Fig. 10)只覆盖 4/5 CPU。
- **仿真吞吐约束**:600K 指令/秒 → 只能跑短-中负载到结束;对长时间驻留/大工作集负载(真实云负载)未覆盖;30 个负载中多数偏小(嵌入式/内核类基准),类别归属也有主观性。
- **无温度/电压/频率维度**:与 SOSP23 的温度轴、ASPLOS24 的 FMAX/电压轴完全正交——复现 Veritas 无法回答"边际条件触发"问题;对比叙事时应把三者当互补轴。
- **指令错误率的统计条件**:只统计"导致 SDC 的注入"中的指令错误率(§IV-F 脚注式限定),不是无条件错误率;跨论文引用时口径要写清。
- **CPU B 双 FADD 的反直觉结果**:2 个单元不分摊 SDC 率(预期减半,实测持平或更高)——永久故障下一半的错误计算足以污染输出;此结论依赖 stuck-at 模型,对间歇/边际缺陷未必成立。
- **Fig. 8 列名解析风险**:PDF 表格文本提取的列序(ADD/FADD/FMUL/VADD/VFADD/VMUL/VFMUL)已尽量按原文校对,但逐格数值(尤其 341/4580 等)引用前应再对原文热图核验一次;本文档已把可确证的相对关系(加法器=1 基准、标量 FP 高约 1 个数量级、向量 FP 数千级)单列,这些是原文文字明确背书的。

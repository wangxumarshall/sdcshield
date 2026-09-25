# Estimating the Failures and Silent Errors Rates of CPUs Across ISAs and Microarchitectures(ITC 2023,雅典大学)

- 作者:Dimitris Gizopoulos、George Papadimitriou、Odysseas Chatzopoulos(University of Athens)
- Venue:2023 IEEE International Test Conference (ITC),SLM Workshop,pp. 377–382,DOI: 10.1109/ITC51656.2023.00056
- 原文 PDF:docs/paper/ref/Estimating_the_Failures_and_Silent_Errors_Rates_of_CPUs_Across_ISAs_and_Microarchitectures.pdf
- 复现档案编号:E27(本文件)

## 1. 研究问题与核心贡献(≤5 行)

跨三大 ISA(x86、Arm、RISC-V)对比 CPU 关键结构(L1D、L1I、物理寄存器堆 PRF、整数功能单元)对瞬态与永久故障的 SDC AVF:每结构每 ISA 注入 1,000 单比特瞬态 + 1,000 单比特永久故障(gem5/GeFIN),15 个 MiBench 负载。核心发现:L1D 是绝对 SDC 主源(瞬态最高 43%、永久最高 70.8%);永久故障下 RISC-V SDC 显著高于 Arm/x86;FU(加法器)的 SDC 高度负载依赖(sha/bitcount 最高,x86 0–19.2%、RISC-V 0–30.4%)。注意:**估算方法是微架构级故障注入,不是电压扫描/FMAX**。

## 2. 实验方法

### 2.1 评估口径(第 II 节)

- **AVF**(架构脆弱因子):一个故障影响程序执行的概率,覆盖激活→跨硬件软件层→输出全链条(引 TC'23/ISCA'21 同组定义);
- **AVF 两条路**:ACE 分析(快、高估、实现难)vs SFI 统计故障注入(慢、准)——本文走 SFI;
- **FIT** = 每 10⁹ 设备小时的失效数;AVF × raw fault rate × 位数 → 结构 FIT,求和 → 处理器 FIT(引言/第 II 节概念定义,本文图只报 SDC AVF 不报 FIT 绝对值);
- gem5 微架构级比 RTL 快 2 个数量级,支持带 OS 的真实负载(III-A)。

### 2.2 注入设置(III-A 节)

- 工具:gem5 + GeFIN;
- 结构:L1 D-cache、L1 I-cache、物理寄存器堆 PRF;
- 故障:单比特**瞬态** + 单比特**永久**;
- 样本量:每结构每故障类型 1,000 注入(对应 **4% 误差、99% 置信**,Leveugle [26] 口径——比同组 TC'23 的 2,000/2.88% 减半);
- PRF 永久故障的 SDC 结果**未报**——实验测出该组合 SDC 概率为 **0**(III-A 末尾明示)。

### 2.3 FU 故障建模(III-D 节)

- 对 FU 的门级永久故障建**统计模型**:模型核心是"FU 输出呈现损坏的频率"(stuck-at-0 / stuck-at-1 / bit-flip 三种形态);
- 本文参数:**每 100,000 次操作 1 次输出错误**(output error frequency = 1e-5/操作),损坏形态 = bit-flip;
- 报告对象:OoO 核 6 个整数加法器中的第 1 个(load/store 地址生成与分支目标计算在独立 FU 中,脚注 1);只对比 x86 与 RISC-V(Arm 未做 FU 实验)。

## 3. 实验配置(模拟器/硬件、基准、参数)

| 项 | 配置 | 出处 |
|---|---|---|
| 模拟器 | gem5 + GeFIN | III-A |
| ISA(3 个) | x86、Arm、RISC-V(均为 OoO 核;具体微架构参数表**原文未给出**) | III-A/III-B |
| 目标结构 | L1D、L1I、PRF(瞬态+永久);整数加器 #1(门级统计模型,x86+RISC-V) | III-A/III-D |
| 注入量 | 1,000 瞬态 + 1,000 永久 / 结构 / ISA | III-A |
| 统计 | 4% 误差、99% 置信 | III-A |
| 基准 | MiBench 15 个:adpcm_dec/enc、basicmath、bitcount、blowfish_dec/enc、corner、crc32、dijkstra、edges、fft_inv、patricia、qsort、sha、smooth | Fig.1–6 横轴 |
| FU 模型参数 | 1 输出错误 / 100,000 操作;bit-flip 形态 | III-D |
| 输出指标 | SDC AVF(%)(各图为 SDC 占总注入的份额) | Fig.1–6 |

## 4. 实验步骤(可操作流程,编号)

1. gem5 配置三个 ISA 的 OoO 核(x86/Arm/RISC-V),full-system 跑 15 个 MiBench;
2. 每基准记录黄金执行输出;
3. 瞬态组:对 L1D/L1I/PRF 每结构均匀采样 1,000 个(表项,位,周期)单比特翻转注入,跑完程序判 SDC;
4. 永久组:对 L1D/L1I 每结构注入 1,000 个单比特永久故障(故障位持续存在),同样跑完判 SDC;PRF 永久组测出全零后不再报告;
5. 聚合各 ISA × 结构 × 故障类型的 SDC AVF 曲线(Fig.1–5);
6. FU 组(x86/RISC-V):在整数加法器 #1 的执行路径插入统计故障模型——每 100,000 次操作按概率 1 触发一次输出 bit-flip;跑 15 负载,统计 SDC AVF(Fig.6)与总 AVF(Crash 主导);
7. 跨 ISA 对比每结构的 SDC AVF 范围与均值排名。

## 5. 实验数据(关键数值 + 图表号)

**瞬态故障 SDC AVF(Fig.1–3,15 负载):**

| 结构 | Arm | x86 | RISC-V | 排名结论 |
|---|---|---|---|---|
| PRF(Fig.1) | 0–6.9% | 0–3.7% | 0.1–9.9% | RISC-V 最高、x86 最低 |
| L1I(Fig.2) | 0.3–9.9% | 0.3–4.6% | 0.2–5.7% | Arm 最高、x86 最低 |
| L1D(Fig.3) | 1.2–43% | 0.7–32.6% | 0.8–40.2% | Arm 最高、x86 最低 |

**永久故障 SDC AVF(Fig.4–5):**

| 结构 | Arm | x86 | RISC-V | 排名结论 |
|---|---|---|---|---|
| L1I(Fig.4) | 0.1–2.3% | 0.1–1.3% | 0.3–2.7% | x86 最低、RISC-V 最高 |
| L1D(Fig.5) | 5.1–53.3% | 4.4–64.7% | 4.4–70.8% | RISC-V 最高(总述:RISC-V 永久故障 SDC 显著更高) |
| PRF | **0(实验结论,无图)** | 未报 | 未报 | 永久 PRF 故障零 SDC |

**FU(整数加法器 #1,永久门级统计模型,1 错误/10⁵ 操作,bit-flip;Fig.6,x86 vs RISC-V):**
- SDC AVF:x86 **0–19.2%**,RISC-V **0–30.4%**;
- 负载差异显著:**sha 和 bitcount 的 AVF 最大**;若干负载 SDC AVF 极小或为零;
- 但总 AVF(Fig.6 未画,正文明示)仍然"substantially high,且主要归因于 Crashes"——加法器上微架构+软件掩蔽显著低(Crash 主导,与 E25 DATE'25 的 Obs #3 一致,本文是其先行工作)。

**跨结构量级排序(III-B/III-C 总述):**
- SDC 在 PRF 与 L1I 中"远比 L1D 稀有";**L1D 上 SDC 是主导故障效应**;
- 机理解释(III-B 末):寄存器错误值极易触发非法内存访问(→Crash);L1I 损坏块大概率执行非法指令(→Crash);L1D 损坏不易崩溃、易传播到输出(→SDC)。

## 6. 实验结论(编号列出)

1. L1D 是三大 ISA 共同的 SDC 主源(瞬态 0.7–43%、永久 4.4–70.8%),且在 L1D 上 SDC 是主导效应——与 TC'23(Fig.3 L1D data 53.4%)和 MeRLiN(L1D SDC ~15–20%)同组证据链一致。
2. 跨 ISA 的 SDC AVF 排序:瞬态下 Arm 的 L1I/L1D 最高、x86 最低;PRF 瞬态 RISC-V 最高;**永久故障下 RISC-V 显著更易 SDC**(L1D 最高 70.8%)——ISA/微架构本身改变 SDC 谱,不存在"ISA 无关"的 SDC 率。
3. PRF 永久故障零 SDC(持续存在的坏寄存器位最终必然表现为崩溃/可检测,而非静默)——与瞬态 PRF(0–9.9% SDC)形成鲜明对照:**故障持续时间改变终态分布**。
4. FU(加法器)统计模型下 SDC 高度负载依赖:sha、bitcount 最大;总 AVF 主要由 Crash 构成,FU 上掩蔽效应低——为 DATE'25 全门级研究(E25)铺路。
5. 方法论:x86/Arm/RISC-V 的可靠性对比可在微架构级以统一 SFI 流程完成(每结构 1,000 注入即达 4%/99% 统计)。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

- **三 ISA 对比框架**:gem5 原生支持 x86/ARM/RISC-V 三 ISA 的 OoO 模型(DerivO3CPU),full-system 镜像齐全——这是三 ISA 研究里最容易复现的部分;
- **注入量口径**:1,000/结构/故障类型(4%/99%)——比 TC'23 的 2,000 减半,适合作为最小复现的样本量;
- **瞬态 vs 永久双故障类型对照**:gem5 FaultInjection 支持 PersistFault(永久)与瞬时翻转两类,直接可用;
- **FU 统计模型**:不需要门级网表——"每 10⁵ 次操作 1 次 bit-flip"可用 gem5 执行回调里以计数器 + RNG 一行实现,是**最便宜的可复现组件**,适合先行验证;
- **15 负载清单**:Fig.1–6 横轴给出全部 15 个 MiBench 名单,可直接照抄;
- SDC 判定 = 输出 diff,同 E23/E24。

### 需替代/不可行

- **GeFIN 不开源**(同系列全部);
- **三 ISA 的具体微架构参数原文未给出**(只有"OoO core"与 FU 数量线索):复现只能自定配置并声明——意味着数值复现只能对照"范围/排序"而非精确值;建议配置对齐 gem5 常用 O3 默认并做敏感性说明;
- **Arm FU 实验缺失**:原文只做 x86/RISC-V 的 FU;SDCShield 关注 ARM64,补做 Arm 加法器统计模型实验是**填空型增量**(合法且必要);
- **每 10⁵ 操作 1 错的标定依据原文未给出**(无实测校准来源),复现时应做误差率扫描(如 1e-4–1e-7)看 SDC AVF 的稳健性;
- **永久故障在 cache 的建模细节**(故障位是否跨程序持续、被覆盖后是否仍算故障)原文未说明,复现需自定义并在论文中声明;
- PRF 永久零 SDC 的结论建议复现验证(gem5 PersistFault 注入 PhysRegFile),它是一个强且反直觉的锚点。

**最小复现路径(建议,含 SDCShield 直接相关项)**:
1. gem5 ARM + x86 双 ISA(可后加 RISC-V)DerivO3CPU + full-system;
2. L1D 瞬态 + 永久各 1,000 注入 × 15 MiBench → 对照 Arm 瞬态 1.2–43% / 永久 5.1–53.3% 范围;
3. PRF 永久 1,000 × 3 负载 → 验证零 SDC 锚点;
4. ARM 加法器统计模型(1 错/10⁵ 操作)× 15 负载 → 填补原文 Arm 空缺,与 x86 0–19.2%、RISC-V 0–30.4% 对照。
总计约 (1000×2×15) + (1000×3) + (15 负载跑 FU 模型) ≈ 3.3 万次模拟级实验。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

| 对比轴 | 论文基线值(可引用) | SDCShield 侧应用 |
|---|---|---|
| ARM L1D SDC AVF | 瞬态 1.2–43%、永久 5.1–53.3%(Fig.3/5) | SDCShield 数据通路测试(L1D 等效功能面)在 ARM 上的"文献期望 SDC 谱"锚点 |
| PRF 永久故障零 SDC | 0(III-A 实验结论) | 真机上"持续性缺陷必然显性化"的论据:面向永久缺陷的检测设计可依赖 crash/超时信号,面向瞬态/边际缺陷才必须做输出校验 |
| FU SDC 的负载依赖 | x86 0–19.2%、RISC-V 0–30.4%;sha/bitcount 最高(Fig.6) | 测试内核选择依据:sha 类 + 位计数类在 FU 缺陷上最敏感(bitcount 恰是 SDCShield/OpenDCDiag 现成测试) |
| FU 总 AVF Crash 主导 | "total AVF substantially high, mainly attributed to Crashes"(III-D 正文) | 与 E25 Obs#3 呼应:算术链激发实验的失败信号里 crash 是主通道,SDC 是稀疏尾部 |
| ISA 改变 SDC 谱 | 永久故障 RISC-V > Arm/x86;瞬态 Arm L1I/L1D 最高、x86 最低 | 反对"把 x86 SDC 数据外推到 ARM"的引用;SDCShield 立项 ARM 原生实验的正当性论证 |
| 注入量经济学 | 1,000 注入 = 4%/99% | 最小可行统计口径 |

## 9. 局限与坑

1. **短文(6 页,SLM Workshop)**:无微架构参数表、无故障注入时刻分布说明、无每基准数值表(只有图);引用只能到"范围 + 排序"粒度,精确单点值需从图上读,误差大;
2. **"估算方法"是模拟注入,不是电压扫描/FMAX**:论文没有做任何电压/频率边界实验(背景节提到近阈值电压易时序错误,但实验部分不含);若任务预期是电压扫描类现场估算方法,本文不提供——该类方法需引 Meta SOSP'23 / PinDrop 等机队论文;
3. **FU 统计模型的校准缺失**:1 错/10⁵ 操作的来源与合理性未论证(无束流/无门级仿真对照);bit-flip 形态对 stuck-at 的代表性未讨论;
4. **Arm 的 FU 数据缺失**(只 x86/RISC-V)——三 ISA 对比在 FU 维度不完整;
5. **PRF 永久零 SDC 只有一句话**:无图无分布,且"我们的实验显示零概率"可能受 1,000 样本限制(真值若是 <0.1% 级别,1,000 注入可能一个都采不到);引用时宜写"未观测到"而非"不可能";
6. **三个 ISA 的微架构配置不透明**:跨 ISA 差异可能来自 ISA 本身,也可能来自 gem5 三种 CPU 模型的配置差异(结构尺寸、调度器),原文未做受控说明——这是跨 ISA 对比研究的经典混淆变量;
7. 无 FIT 绝对值输出(概念在 II 节,数据只有 AVF%);raw fault rate 未给定;
8. 与同组 TC'23(2,000/2.88%)、ISCA'21(2,000/2.88%)相比本文用 1,000/4%——不同论文间同组数字的统计口径不一致,拼接引用时要标注各自的误差界;
9. **负载清单与同组其他论文不完全重合**(15 个 vs TC'23 的 10 个 vs ISCA'21 的 10 个):跨论文对照同一 benchmark 时需核对两边是否都在集内(共同的有 bitcount、fft/qsort、sha、smooth、patricia、edges/corner 等)。

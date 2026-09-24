# Applying Architectural Vulnerability Analysis to Hard Faults in the Microprocessor(SIGMETRICS/Performance 2006 短文,IBM + Duke University)

> 实验复现档案 E22(2026-09-21)
> 作者:Fred A. Bower(IBM, Duke CS)、Derek Hower、Mahmut Yilmaz、Daniel J. Sorin、Sule Ozev(Duke ECE)
> **出处勘误**:任务标注为 "DSN 2006 摘要页";PDF 页脚实际为 **SIGMETRICS/Performance'06, 2006 年 6 月 26–30 日,Saint Malo, France, ACM 1-59593-320-4/06/0006, pp. 375–376**——2 页短文(poster/short work),不是 DSN。
> PDF:`docs/paper/ref/Applying Architectural Vulnerability Analysis to Hard Faults in the Microprocessor.pdf`(2 页,全部精读,以下为全部要点提取)。
> 服务对象:SDCShield——**这是 5 篇中唯一直接面向"硬故障(制造缺陷/固定型故障)"的 AVF 扩展**,与 SDCShield 的 CPU 缺陷检测目标语义最接近,是 AVF 框架(软错误)与 SDCShield(缺陷)之间的桥梁。

---

## 1. 研究问题与核心贡献(≤5 行)

- **问题**:MTTF/FIT 无法反映输入依赖的故障掩蔽与利用率;软错误的 AVF 又只适用于**存储结构 + 瞬态故障**,不适用于硬故障与组合逻辑。
- **贡献**:提出 **H-AVF(Hard-Fault Architectural Vulnerability Factor)**——结构中(对全部故障位点 × 全部故障模型)发生故障导致指令提交错误架构状态的概率度量。
- **方法**:对每个软件基准,逐故障位点、逐故障模型(stuck-at-0/stuck-at-1 等)注入,统计导致错误提交的指令数,按指令数平均。
- **对象**:寄存器堆(ECC)、L1 D-cache(ECC)、64 位整数加法器(TMR)三个代表结构(两存储 + 一组合)。
- **结果**:ECC 使寄存器堆硬故障容忍度 ×8.4、L1 D-cache ×5.8;TMR 使加法器 ×2.8。

## 2. 实验方法(ACE 分析方法、模拟器、故障注入验证)

- **H-AVF 公式**(原文排版还原):

```
H-AVF(structure) = (1/|insts|) × Σ_{∀insts} [ ( Σ_{∀fault sites} Σ_{∀fault models} |insts_error| ) / |fault sites| ]
```

  即:对每条指令的输入,数出"会提交错误架构状态的指令数"(insts_error),对故障位点求和(位点数是结构常数)、再对所有指令平均——**显式按工作负载加权故障掩蔽效应**;对每个故障模型重复,支持多故障模型并存。
- **关键差异(vs 软错误 AVF)**:AVF 是"位被 ACE 占据的时间比例";H-AVF 是"故障位点 × 故障模型 × 输入向量"三维平均——因为硬故障是**永久、与输入相关**的(同一 stuck-at,不同输入下掩蔽不同)。
- **面积归一化**:故障密度对给定工艺近似常数 → 裸 H-AVF 会误导(更大实现=更多位点);除以晶体管数得 **H-AVF per transistor**,并可进一步与面积/功耗组合成复合成本-效益指标。
- **模拟器**:SimpleScalar;寄存器堆/L1 D-cache 用 **sim-cache** 跑每基准前 **10M 指令**;加法器用 **sim-mase**(修改版)+ SimPoint 每基准 **100M 指令**采样。
- **故障注入**:公式本身即穷举注入语义(全部位点 × 全部模型);对存储结构按位翻转语义的 stuck-at 处理;加法器为组合逻辑注入。未见统计抽样说明(短文篇幅所限,细节原文未给出)。

## 3. 实验配置(模拟器配置:核模型/频率/结构大小;基准;参数)

| 配置项 | 值 |
|---|---|
| 寄存器堆 | **126 × 64 位表项** |
| L1 D-cache | **16KB,256 × 64B 行**(Pentium 4 之后建模) |
| 加法器 | 64 位整数加法器(地址计算/整数运算核心) |
| 保护方案 | 寄存器堆/L1 D-cache 用 ECC;加法器用 TMR |
| 面积归一化系数 | 寄存器堆 ~1.15;L1 D-cache ~1.10;TMR 加法器 ~3.31(保护/裸实现晶体管比) |
| 负载 | SPEC CPU 2000 全套(结果为整套平均) |
| 指令数 | sim-cache:每基准前 10M;sim-mase + SimPoint:每基准 100M |
| 模拟器 | SimpleScalar(sim-cache、sim-mase) |

## 4. 实验步骤(可操作流程,编号)

1. 选定结构与故障模型集(基线:single-bit stuck-at-0 / stuck-at-1)。
2. 对每个结构枚举全部故障位点(存储:每位;加法器:每逻辑位点)。
3. 用 SPEC2000 负载驱动,记录每条指令在无故障下的输入/输出行为(sim-cache / sim-mase)。
4. 对每个 (位点, 模型) 组合,判定其在该指令输入下是否导致错误架构状态提交(存储:读出值错;加法器:和错)。
5. 按公式对位点求和、对指令平均、对模型重复 → H-AVF。
6. 对保护实现(ECC/TMR)重复步骤 2–5 → 保护后 H-AVF。
7. 乘晶体管比归一化 → H-AVF per transistor。
8. 与面积/功耗组合成复合指标,比较候选保护方案。

## 5. 实验数据(关键 AVF 数值表 + 图表号)

**Table 1(全文唯一数据表,基准套件平均)**:

| 结构 | Base H-AVF | Protected H-AVF | Normalized Protected H-AVF(每晶体管) | 保护收益 |
|---|---|---|---|---|
| Register File(126×64b) | 0.08388 | 0.00871(ECC) | 0.00958 | **~8.4×** |
| L1 Data Cache(16KB) | 0.00486 | 0.00076(ECC) | 0.00084 | **~5.8×** |
| 64-Bit Adder | 0.1488 | 0.0161(TMR) | 0.0533 | **~2.8×** |

- 数值量级解释(原文):结构相对所存值很大 + cache 缺失率低 → H-AVF 绝对值小。
- 加法器 H-AVF(0.1488)比存储结构高 1–2 个数量级——组合逻辑每位点被高频复用且无空间冗余。
- TMR 归一化惩罚最重(×3.31 晶体管):0.0161 → 0.0533,收益从 ~9.2× 缩水到 **~2.8×**。

## 6. 实验结论(编号列出)

1. H-AVF 把软错误 AVF 的思想推广到硬故障与组合逻辑:输入加权 + 利用率感知 + 多故障模型支持。
2. ECC 对存储结构的硬故障容忍收益 8.4×(RF)/ 5.8×(cache);TMR 对加法器 2.8×——**面积归一化后 TMR 类粗暴冗余的性价比显著劣化**。
3. 每晶体管归一化 + 与面积/功耗复合,使不同保护方案(ECC vs TMR vs 不保护)可在统一坐标下量化比较。
4. 用途:(1) 判断某子结构是否值得加固;(2) 定量比较竞争性容故障设计。

## 7. 复现要点(gem5/ARM64 复现最小版本;短文,最小化设计)

### 可直接复用

| 要素 | 说明 |
|---|---|
| H-AVF 公式与面积归一化 | 直接可算;对 SDCShield 的真机测试就是"测试向量 × 故障位点"覆盖率语言的近亲 |
| stuck-at-0/1 双模型基线 | ARM64 真机不可注入 stuck-at,但**测试输入设计**等价于对"哪些输入向量能暴露哪些位点"采样 |
| ECC/TMR 对比协议 | gem5 可模拟 ECC cache;TMR 加法器可用三份计算+表决在测试层模拟 |
| SPEC 驱动、按指令平均 | 与 SDCShield TEST_LOOP 的 per-iteration 校验语义天然一致 |

### 需替代/不可行

| 要素 | 替代方案 |
|---|---|
| 穷举全部故障位点 | 不可行(现代结构位点数天文数字);用统计抽样或挑重点通路(乘法器/FMA 进位链) |
| SimpleScalar sim-cache/sim-mase | gem5 ARM64 或直接真机(SDCShield 本体) |
| Pentium 4 式 16KB L1 | Kunpeng 920:64KB L1D,4-way;规模重定 |
| 126×64b 寄存器堆 | ARM64:31×64b GPR + 32×128b NEON + 32×512b(SVE)——SVE 寄存器堆是更贴近本项目的对象 |
| H-AVF 绝对数值对标 | 无意义(结构/工艺全变);只借方法论 |

**最小复现配方**:在 gem5(或 SDCShield 真机)上取 SVE FMA 通路;测试向量集分"随机""边界""位偏置(SEVI Obs.8 类)"三组;以"每组向量能检出的注入位翻转数"(gem5 faultinject 或软件级 bit-flip 模拟)估计 H-AVF 样本;比较输入集的检出力差异——即 SEVI 式输入敏感性实验的 AVF 语言重述。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield:**5 篇中唯一直接连接"AVF 方法论"与"硬故障检测"**——SDCShield 检测的是制造缺陷(硬故障),而 E18/E20/E21 的 AVF 都度量瞬态软错误;H-AVF 提供了把 SDC excitation 测试的覆盖度也说成"vulnerability factor"的合法途径。

| 对比轴 | 论文基线值 | SDCShield 侧应用 |
|---|---|---|
| 输入向量加权 | SPEC 平均 H-AVF(RF 0.084/cache 0.005) | SDCShield 每轮换输入(RNG)等价于对输入空间采样;可量化"测试向量集对故障位点的边际检出增益" |
| 保护方案性价比 | ECC ×8.4/×5.8,TMR ×2.8(归一化后) | 对比轴:SDCShield 测试 vs 硬件冗余 vs ECC 的成本-检出曲线 |
| 组合逻辑 vs 存储 | 加法器 0.1488 >> cache 0.00486 | **组合逻辑(FMA/乘法器)是硬故障脆弱高地**——与 SEVI(FMA 第一大 SDC 源)跨 20 年互证,支撑 SDCShield 向量 FMA 测试的优先级 |
| 面积归一化 | ×3.31(TMR) | 测试方案的"时间成本"归一化:每 CPU 秒检出率,类比每晶体管 H-AVF |
| 表决/TMR | 0.0161 → 0.0533 | 三份冗余计算+多数表决可作为 SDCShield 的软件级对照实现 |

## 9. 局限与坑

1. **会场标注错误**:这是 SIGMETRICS/Performance'06 短文(pp. 375–376),不是 DSN'06;引用时注意(它常与 Bower 等 DSN'05 "A Framework for Evaluating the Effectiveness of Hard Fault Tolerant Designs" 一系工作混淆)。
2. **仅 2 页短文**:无逐负载结果、无故障注入的抽样/置信度细节、无统计方法说明——严格复现需要补全实验设计(原文未给出的部分只能自行设计并声明)。
3. 穷举位点对现代结构不可行;原文对 16KB cache 是否穷举了所有位也未说明(推测为模型级近似,未确认)。
4. H-AVF 数值绑定 SPEC2000 + 2006 年前结构(P4 式 cache、126 项 RF),绝对值不可平移。
5. 只考虑永久 stuck-at;桥接/开路、时序故障(见 DelayAVF MICRO'24)不在内。
6. 未给方差/置信区间;表值只有套件平均。
7. "126 64-bit entries" 的寄存器堆规格疑似含重命名/特殊寄存器(短文未解释,推测,未确认)。

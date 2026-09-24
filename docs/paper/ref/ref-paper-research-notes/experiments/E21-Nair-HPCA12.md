# A First-Order Mechanistic Model for Architectural Vulnerability Factor(HPCA 2012,UT Austin + Ghent University)

> 实验复现档案 E21(2026-09-21)
> 作者:Arun Arvind Nair(UT Austin)、Stijn Eyerman、Lieven Eeckhout(Ghent)、Lizy Kurian John(UT Austin)
> 出处:Proceedings of the 18th International Symposium on High-Performance Computer Architecture (HPCA-18),2012,pp. 273–284(IEEE 版权行 978-1-4673-0476-4/12)。
> PDF:`docs/paper/ref/A First-Order Mechanistic Model for Architectural Vulnerability Factor.pdf`(12 页,全部精读;§3 模型推导、§4 验证、§5 应用逐句读)。
> 服务对象:SDCShield——把 AVF 从"跑模拟器才能得到"变成"剖析一次 + 解析计算",为快速评估测试负载的脆弱状态激发能力提供公式化工具。

---

## 1. 研究问题与核心贡献(≤5 行)

- **问题**:ACE 分析需要昂贵的周期精确模拟,且只给聚合数值;统计/机器学习模型是黑箱,无法解释微架构事件如何影响 AVF。能否从乱序处理器执行的第一性原理**解析推导** AVF?
- **方法**:区间分析(interval analysis)——把执行切成"无 miss 的理想区间 + 各类 miss 事件区间";分别建模每类区间中**正确路径状态的占用率(occupancy)**,按周期数加权平均;再乘以负载注入的 un-ACE 位比例得到 AVF。
- **关键创新**:显式建模 miss 事件间的**交互**(依赖长延迟 load 的分支误预测、取指 miss 与数据 miss 的重叠)——这是 CPI 区间模型忽略、但决定占用率的效应。
- **输入**:一次性廉价剖析(profilers 为滑动窗口,ROB 大小/发射宽度/延迟可后改);**无需为每个微架构配置重跑模拟**。
- **结果**:4-wide 机器上 ROB/IQ/LQ/SQ/FU 五结构 AVF 的平均绝对误差(MAE)<0.07;2-wide→4-wide SER +81%。

## 2. 实验方法(ACE 分析方法、模拟器、故障注入验证)

### 2.1 模型总体结构(任务重点:一阶公式 = 占用率 × ACE 比例)

- **第一层(占用率)**:AVF ∝ 结构中正确路径(最终提交)状态的平均占用。
  - ROB 平均占用(原文 Eq. 1):

```
O_ROB_avg = (1/C_total) × [ O_ROB_ideal·C_ideal
          + O_ROB_DL2Miss·C_DL2Miss + O_ROB_IL1Miss·C_IL1Miss
          + O_ROB_ITLBMiss·C_ITLBMiss + O_ROB_brMp·C_brMp + O_ROB_DTLBMiss·C_DTLBMiss ]
```

  即六种区间(理想 / 数据 L2 miss / L1 I-miss / I-TLB miss / 分支误预测 / D-TLB miss)的占用按各自周期数加权。
- **各区间占用的一阶解**(原文 §3.1,Figure 2 的三张机理图):
  - **理想稳态**:临界路径模型 K(W) = (1/α)·W^(1/β),稳态 IPC I(W) = α/l·W^(1-1/β)(Eq. 2);令 I(W)=D 反解出支撑峰值发射宽度 D 所需窗口(= 稳态 ROB 占用):

```
O_ROB_ideal = W(D) = (l·D/α)^(β/(β-1))        (Eq. 3)
```

  - **数据 L2/TLB miss 阴影区**:miss 到达 ROB 头部阻塞退休,处理器持续发射到 ROB 满 → O_ROB_DL2Miss = **W**(100% 占用,Figure 2a)。
  - **L1 I-cache miss**:前端排空,占用近似为 O_ROB_ideal − lat_L2·D(降 lat_L2·D 条指令;L2 取指 miss 与 ITLB miss 延迟大,占用降到 0)(Figure 2c)。
  - **分支误预测**:oldest-first 发射策略下误预测分支是该窗口最后执行的正确路径指令之一,检测到时正确路径状态几乎排空 → O_ROB_brMp ≈ **0**(Figure 2b)。
  - **IQ 独立建模**(乱序发射):O_IQ_ideal = l·A(W)·min(D, I(W)),A(W) = 窗口内依赖链平均指令数(临界路径剖析的副产品);L2 阴影区 IQ 只剩依赖 miss 的指令(剖析得到)。
  - **LQ/SQ/FU**:由 ROB 占用 + 指令混合(I-mix)导出。SQ 占用-周期积 = (N_stores/N_total)·O_ROB_ideal·C_ideal − l·A(W)·N_stores;FU 用 Little's law(延迟 × 发射率)。
- **第二层(un-ACE 去率)**:AVF = 平均占用 × (1 − 负载注入该结构的 un-ACE 位比例)。un-ACE 指令(NOP、死指令等)在剖析中识别——**程序影响(注入多少 ACE 位)与微架构影响(这些位驻留多久)显式解耦**,这是模型可解释性的来源。
- **交互项建模**(§3.1.5,模型的灵魂):
  - 依赖长延迟 load 的误预测分支:O_ROB_DL2Miss·C_DL2Miss 替换为 lat_DL2Miss·(len_DL2,Br(W)·N_dep(W) + W·(N_DL2Miss(W) − N_dep(W)))——即只有含依赖误预测分支的 miss 阴影区占用从 W 降到 len_DL2,Br;perlbench/gcc/mcf/astar 有大量此类交互。
  - 数据 miss 后 >2W 指令的长区间才回到满占用(Figure 3b:除 hmmer/gobmk/sjeng/astar 外均罕见,这些负载长区间均值 300–450 条指令)。
  - I-miss 落在数据 miss 阴影区内:罕见,仅 perlbench(ITLB miss 多)受影响。

### 2.2 验证方法(模拟器、无故障注入)

- **基准真相**:在**修改版 SimpleScalar** 上实现详细**逐位 ACE 分析**——每个微架构结构表项按 opcode 设 ACE 位域(如 store/branch 不需要结果寄存器,ROB 表项对应字段 un-ACE);自行实现独立的 IQ/LQ/SQ。
- 模型 vs 模拟对比:20 个 SPEC CPU2006 负载(其余编译不过 Alpha)、gcc 4.1 -O2、**每负载单点 100M 指令 SimPoint**。
- 剖析器为滑动窗口、覆盖多窗口尺寸 → 改 ROB 大小/发射宽度/流水线深度/延迟不需重剖析;改 cache 层级或分支预测器才需重跑对应剖析器。
- **故障注入:未使用**(模型对比对象是 ACE 分析模拟,不是注入)。

## 3. 实验配置(模拟器配置:核模型/频率/结构大小;基准;参数)

Table 1 双机配置(验证 + SER 对比用):

| 参数 | Wide-Issue(4-wide) | Narrow-Issue(2-wide) |
|---|---|---|
| ROB | 128 表项 × 76 位 | 64 表项 × 76 位 |
| Issue Queue | 64 × 32 位 | 32 × 32 位 |
| LQ | 64 × 80 位 | 32 × 80 位 |
| SQ | 64 × 144 位 | 32 × 144 位 |
| 分支预测器 | Combined:4K bimodal + 4K gshare + 4K choice + 4K BTB | 同左 |
| 前端流水线深度 | 7 | 5 |
| fetch/dispatch/issue/execute/commit | 4/4/4/4/4 每周期 | 2/2/2/2 |
| L1 I-cache | 32KB 4-way | 32KB 4-way |
| L1 D-cache | 32KB 4-way | 32KB 4-way |
| L2(统一) | 1MB 8-way | 1MB 8-way |
| DL1/L2 延迟 | 2 / 9 周期 | 2 / 9 周期 |
| DTLB/ITLB | 512 项全相联 | 512 项全相联 |
| 内存延迟 | 300 周期 | 300 周期 |
| TLB miss 延迟 | 75 周期 | 75 周期 |

- 基准:20 个 SPEC CPU2006(perlbench、bzip2、gcc、bwaves、mcf、milc、zeusmp、gromacs、leslie3d、namd、gobmk、soplex、hmmer、sjeng、gemsFDTD、libquantum、h264ref、omnetpp、astar、sphinx3;Figure 4/5 列表含 soplex,正文列 20 个)。
- 每负载单 SimPoint 100M 指令;gcc 4.1 -O2(Alpha ISA)。
- SER 计算:假定任意内禀故障率 **0.01 units/bit**(无量纲单位);ROB 缩放研究 64→160 表项。

## 4. 实验步骤(可操作流程,编号)

1. **实现/复用剖析器组**(基于 interval-analysis 框架,Karkhanis & Smith/Eyerman 等):
   a. 临界路径剖析:得 K(W)、A(W)(依赖链长);
   b. 非 overlapped 数据 L2/DTLB miss 剖析:附加记录 N_dep(W)(含依赖误预测分支的 miss 数)与 len_DL2,Br(W)/len_DTLB,Br(W)(miss 头部到最早依赖分支的平均距离);
   c. 分支剖析:误预测率与位置;
   d. 长区间统计:非 overlapped miss 后 >2W 指令无前端 miss 的比例与长度;
   e. un-ACE 指令剖析:NOP/prefetch/死指令比例(按结构位域折算成 un-ACE 位注入比例);
   f. I-mix:load/store/算术占比。
2. **拟合 α、β**:对每个负载把 K(W)–W 关系拟合成幂曲线(α、β 为负载常数)。
3. **按 Eq. 3 与各区间公式计算占用**:理想 W(D);DL2 阴影 W(减交互修正);brMp ≈ 0;IL1 miss 减 lat_L2·D;ITLB/IL2 → 0。
4. **加权平均(Eq. 1)**:各区间周期数来自 interval 性能模型(CPI 栈)。
5. **导出 LQ/SQ/FU**:乘 I-mix 与延迟校正;FU 用 Little's law。
6. **去率**:AVF = 占用 × ACE 位注入比例;SER = AVF × 位数 × 0.01。
7. **验证**:与 SimpleScalar 逐位 ACE 分析对比,计算 MAE/最大误差/NRMSE(Table 2)。
8. **应用**:ROB 64→160 缩放扫描;内存延迟 150 vs 300;2-wide vs 4-wide 对比;RMT/PER 缓解方案效率一阶估计。

## 5. 实验数据(关键 AVF 数值表 + 图表号)

### 5.1 模型误差(Table 2;MAE = 平均绝对误差)

| 结构 | Wide MAE | Wide 最大误差(负载) | Narrow MAE | Narrow 最大误差(负载) |
|---|---|---|---|---|
| ROB | 0.03 | 0.08(hmmer) | 0.06 | 0.13(hmmer) |
| IQ | 0.07 | 0.16(bwaves) | 0.07 | 0.16(leslie3d) |
| LQ | 0.05 | 0.09(zeusmp) | 0.05 | 0.10(gemsFDTD) |
| SQ | 0.02 | 0.06(omnetpp) | 0.02 | 0.07(milc) |
| FU | 0.01 | 0.05(zeusmp) | 0.02 | 0.13(gromacs) |

- 全部 MAE ≤ 0.07(即 7 个 AVF 百分点)——「模型误差 < 0.07」的准确表述。
- SER 误差(以等效内禀故障率的表项数计):Wide 机 ROB/IQ/LQ/SQ 的 MAE = 3.8/4.5/2.8/1.3 entries,最大 10.2/9.9/5.7/3.8;Narrow 机 MAE = 3.8/2.1/1.5/0.64,最大 8.3/5.12/3.2/2.24。
- **NRMSE:Wide 9.0%,Narrow 10.3%**。

### 5.2 AVF 实测范围(Figure 4 / Figure 5,20 负载逐柱;原文未给逐数值表,以下为图上可读的量级与正文点名值)

| 结构 | Wide 机特征 | Narrow 机特征 |
|---|---|---|
| ROB AVF | 多数负载 0.1–0.4;hmmer/gobmk/sjeng 低,mcf 中等,bwaves/namd 高 | 整体略低 |
| IQ AVF | 多数 0.1–0.3(bwaves 尾部最大误差 0.16) | 类似 |
| LQ/SQ | SQ 最小(0.02 MAE 量级) | — |
| SER(Wide,0.01 units/bit,五结构合计) | 图纵轴 0–120 units;libquantum/gemsFDTD 等高 | 图纵轴 0–60 units |

**namd / gobmk 对比(任务重点,§5.3 原文点名)**:
- **namd**:高 IPC 且**诱导高 AVF**(多结构)——临界依赖路径长 → β/(β−1) 与 l/α 大 → Eq. 3 理想占用高;miss 事件少 → 占用不被稀释。wide 机 SER 是 narrow 的 **2.6×**。
- **gobmk**:相近的高 IPC 却在**多结构诱导极低 AVF**——ROB 足够大、可用 MLP 少,CPI 与 SER 对 ROB 缩放都不敏感。
- bwaves:SER × **2.26**(wide/narrow),另有 DL2 miss 贡献增量。
- 结论:IPC/缺失率等聚合指标与 AVF **不相关**(模糊关系),必须用机理解构。

### 5.3 4-wide vs 2-wide:SER +81%(§5.2,Figure 8;任务重点实验配置)

- 配置:即 Table 1 的 Wide(4-wide,ROB 128,前端深 7)vs Narrow(2-wide,ROB 64,前端深 5)。
- **平均 SER +81%,平均加速 1.35(调和平均)**;归因:发射宽度增大要求更大指令窗口支撑(W(D) 超线性,β 在 1.24–2.39 之间)→ 理想执行区间占用上升;发射宽度增大通常推高所有负载的 SER。
- 例外:mcf 不受宽度/ROB 影响(其数据 L2 miss 阴影区充满依赖误预测分支,占用被钉死在低位)。
- CPI 栈与 SER 栈按事件分解:SER 主要构成 = Ideal + DL2 + DTLB + IL1 四块。

### 5.4 ROB 缩放(§5.1,Figure 6)

- 64 → 160 表项扫描(Wide 机):一般趋势——小 ROB 不足支撑 IPC=4 时,增大 ROB 先升 Ideal-SER;有 MLP 的负载 CPI 降但 miss 阴影区占用也升 → SER 升。
- **128 表项 vs 96 表项:加速 1.098(调和平均),平均 SER +18%**。
- libquantum:ROB 增大获得 MLP → CPI 降、SER 升(gemsFDTD 的 CPI 降速更慢、SER 升速更快)。
- perlbench/mcf 例外:SER 对 ROB 不敏感(mcf = 依赖误预测分支;perlbench = ITLB miss 在数据 miss 阴影内)。

### 5.5 内存延迟灵敏度(§5.1,Figure 7)

- 150 vs 300 周期:模型预测平均 AVF 变化 3.25 units vs 模拟 2.22 units,趋势吻合;AVF 对内存延迟**次线性**(分子分母同缩),比 CPI 不敏感;astar 反向(AVF 略升)。
- 延迟降低时:L2 miss 阴影贡献显著降,Ideal/DTLB 相对贡献升。

### 5.6 缓解方案一阶评估(§5.2)

- PER(Gomaa 等,机会主义 RMT:仅低 IPC 区间开 RMT):乐观假设零性能损失,wide 机 **SER 降 66%**;内存延迟 150 时降 60%。但 namd 仍高 AVF 且 miss 少 → RMT 常开会重伤其性能。与 Sridharan 等的详细模拟结论(~60% 脆弱性在长停顿指令阴影中,多为数据 L2 miss)一致。

## 6. 实验结论(编号列出)

1. **AVF = 正确路径状态占用率 × 负载的 ACE 位注入比例**——一阶解析可行,五结构 MAE ≤ 0.07、SER NRMSE ~9–10%。
2. 占用率必须**按 miss 事件区间分解并建模交互**:分支误预测区间占用 ≈ 0、L2 miss 阴影区 = 满、理想区间 = W(D);依赖误预测分支把 miss 阴影区占用从 W 砍到 len_DL2,Br。
3. **发射宽度与 AVF 正相关**:2→4 wide 平均 SER +81%、加速 1.35——性能与可靠性在此维度同向恶化。
4. **聚合指标(IPC、缺失率)与 AVF 不相关**:namd vs gobmk(同高 IPC,AVF 一高一低)是标准反例;AVF 由 (临界路径形状 β、延迟 l、miss 结构、un-ACE 注入比例) 联合决定。
5. 模型一次剖析多配置复用,支持即时设计空间探索(ROB 128 vs 96:+18% SER 换 1.098 加速)。
6. AVF 对内存延迟次线性;对 ROB 大小的响应分负载三类(不足型/MLP 型/交互钉死型)。
7. PER 类机会主义冗余平均有效(−66%)但对 namd 型负载失效——缓解方案评估必须看负载的 miss 结构。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

| 要素 | 说明 |
|---|---|
| Eq. 1–3 与各区间占用公式 | ISA 无关;A(W)、K(W)、I-mix、un-ACE 比例都可在 ARM64 上剖析 |
| 区间交互修正项 | N_dep(W)、len_DL2,Br(W) 的剖析定义直接照抄 |
| 双机对比协议(Wide vs Narrow) | Table 1 可原样映射到 gem5 twoconfigs |
| 验证协议 | MAE/最大误差/NRMSE + 20 负载单点 100M 指令——直接沿用 |
| PER 评估方法 | 消去 SER 栈中 DL2+DTLB 块的重算法 |
| namd/gobmk 对照负载 | SPEC2006 namd/gobmk 在 ARM64 可编译,机理(临界路径长 vs MLP 少)跨 ISA 成立 |

### 需替代/不可行

| 要素 | 替代方案 |
|---|---|
| SimpleScalar(Alpha,已停维护) | gem5 O3CPU ARM64 + 自实现逐位 ACE(或 gem5-approxlyzer 思路);SimpleScalar 的"独立 IQ/LQ/SQ"在 gem5 原生存在 |
| Alpha gcc 4.1 二进制 | ARM64 原生编译(GCC/Clang -O2);20 负载在 ARM64 全部可编(优于 Alpha 的 20/26) |
| 剖析器组 | 无现成开源;需基于 interval 模型自研(约 4–6 周工作量);或退一步用 gem5 统计 + 事后拟合 |
| 0.01 units/bit 内禀故障率 | 真机可用 Kunpeng 920 的实际 SER 假设或保持无量纲做相对比较 |
| 2 GHz 类标量频率 | 按目标机(2.6 GHz)重定;模型以周期为单位,频率只影响换算 |
| SingleSimPoint 100M | 建议多点(作者自己承认长区间误差会累积,需分段估计再合并) |

**最小复现配方(约 5–6 周)**:
1. gem5 ARM O3CPU,按 Table 1 复刻 Wide/Narrow 两配置;
2. 写剖析 pass(gem5 仿真模式或 LLVM 插桩):临界路径 K(W)/A(W) 滑动窗口、非 overlapped DL2/DTLB miss + 依赖分支统计、un-ACE 比例、I-mix;
3. 实现 Eq. 1–3 计算器(纯 Python 即可);
4. gem5 侧做逐位 ACE 分析(ROB/IQ/LQ/SQ/FU 每表项按 opcode 打位掩码)产基准真相;
5. 20 个 SPEC2017 对应负载 × 100M 指令,输出 Table 2 式误差表与 Figure 4 式双柱图;
6. 跑 2-wide vs 4-wide 对比,检验 +81% SER 增量是否在 ARM 上复现。

### 复现实现细节补充(剖析器与计算器的代码级展开)

**A. 剖析器输出接口(一次性剖析,多配置复用)**:

```
profile(workload) →
  K(W), A(W)      # W ∈ {32,64,96,128,160,192,256} 滑动窗口;幂律拟合得 α, β
  l               # 平均指令延迟(改延迟不用重剖析)
  miss_stats(W)   # 非 overlapped DL2/DTLB miss 计数、N_dep(W)、len_DL2,Br(W)/len_DTLB,Br(W)
  branch_stats    # 误预测率、误预测在窗口内位置分布
  longgap(W)      # miss 后 >2W 指令无前端 miss 的区间比例与平均长度
  unace_frac      # NOP/prefetch/死指令占比 → 每结构 un-ACE 位注入比例
  imix            # load/store/FP/int 比例
```

**B. AVF 计算器主循环(对应 §4 步骤 3–6)**:

```
def avf(cfg, prof):
    W_ideal = (prof.l * cfg.D / prof.alpha) ** (prof.beta / (prof.beta - 1))   # Eq.3
    O = {
      'ideal': min(W_ideal, cfg.ROB),
      'DL2':   W, 'DTLB': W,          # 阴影区满(经 N_dep/len 交互修正)
      'brMp':  0,                      # oldest-first 下误预测时排空
      'IL1':   max(0, W_ideal - cfg.latL2 * cfg.D),
      'IL2':   0, 'ITLB': 0,
    }
    C = interval_cpi_stack(cfg, prof)              # 各区间周期数
    O_avg = Σ O[k]*C[k] / C_total                  # Eq.1
    AVF_ROB = O_avg * (1 - prof.unace_frac.ROB)
    O_IQ_ideal = prof.l * prof.A(W) * min(cfg.D, I(W))
    ...                                            # §3.2/§3.3 同理
    return {ROB, IQ, LQ, SQ, FU}
```

**C. 逐位 ACE 基准真相(SimpleScalar → gem5 移植要点)**:原文做法"每个表项按 opcode 设 ACE 位域"——在 gem5 O3CPU 的 ROB/IQ/LSQ `DynInst` 上加 `aceMask` 字段;commit 时按 §E18 的死指令回填规则修正;统计点选每周期 `numInsts × avgAceBits / 结构总位数`。原文明确 store/branch 无结果寄存器 → 对应 ROB 域 un-ACE,这类字段级规则可原样搬。

**D. 对 SDCShield 的两处直接借用法**:
1. **负载预筛器**:对候选测试负载(openblas/sleef/eigen/zstd/SVE 长链)只跑一次轻量剖析(LLVM 插桩即可,不用 gem5),用计算器预测各结构 AVF,排序后把真机长测预算给最高者——省去"每个负载都上 192 核跑小时级"。
2. **加宽/加深增益预测**:SVE 512-bit 相对 NEON 128-bit 等效于"发射宽度 × 数据通路"双重加宽;按本文 β∈[1.24,2.39] 的超线性关系,脆弱状态增长应超线性——SEVI 已实证 256-bit FMA SDC case 多于 128-bit(Obs.4),两条证据链可互引。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield:这是**把"负载的 AVF 激发能力"公式化**的钥匙——SDC excitation 测试本质上就是刻意构造高 (占用 × ACE 比例) 的负载。

| 对比轴 | 论文基线值 | SDCShield 侧应用 |
|---|---|---|
| 4-wide vs 2-wide SER | +81%(加速 1.35) | 宽发射/宽向量(SVE 512)同时放大吞吐与脆弱状态——SVE 测试的"AVF 增益"有理论预期,可实测对照 |
| ROB 128 vs 96 | SER +18% / 加速 1.098 | 结构规模-脆弱性换算的标定点;真机不可改 ROB,但 SVE 寄存器/队列深度类比成立 |
| namd vs gobmk | 同高 IPC,AVF 一高一低 | **测试设计必须避免 IPC 幻觉**:SDCShield 测试要同时报告占用率与结果消费率(ACE 比例),不能只报吞吐 |
| 模型 MAE ≤ 0.07 | Table 2 | 用解析模型预筛测试负载,把昂贵实测留给模型判为高 AVF 的候选 |
| PER/RMT 评估 | SER −66%(wide) | 机会主义检测(仅低 IPC 时冗余)的思想可借给 SDCShield:在 FMA 长延迟等待期插入校验线程 |
| 内存延迟次线性 | 3.25 vs 2.22 units | 真机内存压力实验(大矩阵)对 AVF 的增益有限,优先改指令流而非工作集 |
| L2 miss 阴影区满占用 | O=W | **长依赖链 load→FMA 是把 ROB/LSQ 填满 ACE 的最优模式**——直接指导 SVE 长延迟链测试(与 cluster 锚定 ld1rd 复现工程互证) |

## 9. 局限与坑

1. **AVF 只到一阶**:占用线性化(ramp-up/down 斜率=发射率)、区间独立假设在占用上不成立(作者靠交互项补偿);逐位 ACE 与"un-ACE 比例恒定于每区间"的假设在相位切换处有误差(作者建议分窗估计再合并)。
2. **hmmer 类负载误差最大**(ROB 0.08/0.13):miss 事件极少 + K(W) 不合幂律时理想占用失准——低 miss 高计算的负载恰是 SDCShield 最关心的类型,**复现时要重点核这一类**。
3. **IQ 的 NOP 立即离队效应未建模**(NOP 计入 A(W) 但驻留近零)→ IQ AVF 系统性偏差。
4. 只覆盖占用驱动型结构(ROB/IQ/LQ/SQ/FU);地址类结构(cache/TLB)不在内(须配合 E20)。
5. 单 SimPoint 100M;长区间统计(2W 过滤)阈值是经验值。
6. 20/26 SPEC2006(Alpha 编译失败);beta 拟合范围 1.24–2.39 之外的外推无保证。
7. SER 用无量纲 0.01 units/bit,绝对值无物理意义,只能做相对比较。
8. 无故障注入验证(与 RTL/真机无对照);模型只对齐 ACE 分析模拟——若 ACE 分析本身偏保守,误差被"继承"。
9. 分支误预测占用 ≈ 0 依赖 oldest-first 发射策略;乱序发射策略改变时该项要重推(对 ARM 大核的策略假设需检查)。

## 附:任务重点问题的原文逐条回答

**Q1:一阶模型公式(占用率 × ACE 比例)的推导链**:
- 第一步(Eq. 2→3):窗口 W 内临界路径 K(W) = (1/α)·W^(1/β) → 稳态 IPC I(W) = W/(l·K(W)) = (α/l)·W^(1−1/β);**令 I(W) = D(发射宽度)反解 W** = (l·D/α)^(β/(β−1)) —— 这就是理想区间 ROB 占用(W(D))。直觉:发射越宽,需要越满的窗口来喂;β>1 保证超线性。
- 第二步(Eq. 1):六种区间占用加权平均。占用取值:理想=W(D);DL2/DTLB miss 阴影=W(满);分支误预测≈0(检测时正确路径已排空);IL1 miss = W(D)−lat_L2·D;IL2/ITLB=0。
- 第三步(交互修正):含依赖误预测分支的 miss 阴影区占用 W → len_DL2,Br(W)(数据项:lat_DL2Miss×(len·N_dep + W·(N_total−N_dep)))。
- 第四步(去率):AVF = O_avg ×(1−un-ACE 注入比例)。IQ 独立:O_IQ_ideal = l·A(W)·min(D, I(W))(依赖链平均长 × 有效发射率,Little's law)。LQ/SQ = ROB 占用 × imix − 发射前延迟;FU = 延迟 × 发射率(Little's law)。
- **推导的本质**:AVF 的两个因子被彻底解耦——占用率是**微架构+程序交互**的函数(可按配置即时重算),ACE 比例是**纯程序**的函数(剖析一次)。

**Q2:4-wide vs 2-wide SER +81% 的实验配置**:
- 即 Table 1 双机:Wide = 4/4/4/4/4(fetch/dispatch/issue/exec/commit)、ROB 128×76b、IQ 64×32b、LQ 64、SQ 64、前端深 7;Narrow = 2-wide、ROB 64、IQ/LQ/SQ 32、前端深 5;其余全同(32KB L1 ×2、1MB L2、DL1/L2 = 2/9 cyc、mem 300 cyc、TLB miss 75 cyc、512 项全相联 TLB、同一 combined 分支预测器)。
- 结果(§5.2):平均 SER **+81%**、调和平均加速 **1.35**;SER 栈分解为 Ideal/DL2/DTLB/IL1 四块(Figure 8b);例外负载 mcf(依赖误预测分支钉死占用,SER 不变);bwaves/namd SER ×2.26/×2.6。
- 机理解释(Eq. 3):β∈[1.24,2.39] → W(D) 随 D 超线性;分支解析时间随宽度增加也推高占用。

**Q3:模型误差 < 0.07 的验证方法**:
- 真相源 = 修改版 SimpleScalar 上的**逐位 ACE 分析**(表项按 opcode 设 ACE 位域;独立 IQ/LQ/SQ;store/branch 的结果寄存器域 un-ACE)。
- 协议:20 负载 × 单 SimPoint × 100M 指令(gcc 4.1 -O2,Alpha);逐负载算 |modeled−simulated|;指标:MAE(Table 2 全表:ROB 0.03/0.06、IQ 0.07/0.07、LQ 0.05/0.05、SQ 0.02/0.02、FU 0.01/0.02,Wide/Narrow)、最大绝对误差 ≤0.16(bwaves/leslie3d 的 IQ)、SER 的 NRMSE 9.0%/10.3%。
- 附带口径:SER 误差换算成"等效内禀故障率的 n 个表项"(Wide ROB MAE 3.8 entries、最大 10.2)。

**Q4:namd/gobmk 负载对比**:
- namd(§5.1/§5.2/§5.3):高 IPC + 高 AVF 的代表——长临界依赖路径(K(W) 大)→ β/(β−1) 与 l/α 高 → Eq. 3 理想占用高;miss 少 → 占用不被 miss 事件稀释;wide/narrow SER 比 2.6×;即使上 PER/RMT 缓解,namd 仍高 AVF 且常开冗余会重伤其性能。
- gobmk(§5.1/§5.3):相近高 IPC 但多结构低 AVF——ROB 足够、MLP 少 → CPI 与 SER 对 ROB 64→160 缩放都不敏感;Figure 3(b) 中 gobmk 属于"miss 后长理想区间"的少数负载(长区间均值 300–450 指令)。
- 教训(§5.3 原文结论):**IPC、cache 缺失率等聚合指标与 AVF 不相关**;识别高 AVF 负载必须看机理分解(临界路径形状 × miss 结构 × un-ACE 注入)。

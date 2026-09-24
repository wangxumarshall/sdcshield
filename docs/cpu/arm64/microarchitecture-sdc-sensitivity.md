# ARM64 CPU 微架构 × SDC 敏感性对比
---

## 1. 基本信息
- IFU（指令供给前端）：分支预测 (BP/BRE分支方向预测/BTB/间接预测/GHB/BPIQ/返回栈 RAS)、µop/MOP cache、L1-iTLB、L1i-Cache（tag/data）、Instruction Queue；
- OoO（乱序执行引擎）：Int instruction Decode、Int Registor Rename、Int Dispatch、FP/SIMD instruction Decode、FP/SIMD Registor Rename、FP/SIMD Dispatch；ROB（Reorder Buffer，重排序缓冲）
- IEX（Int instr Execute）：ALU Issue Queue、LSU/MDU/SYS Issue Queue、Int Physically Registor File、ALU执行单元、MDU执行单元（整数乘除）、MSR/CP15；
- LSU（Load Store Unit）：LS（AGU/load）、STD（AGU/store）、L1-dTLB、Store Queue、L1d-Cache（tag/data/aux tag）；原子与同步单元（执行原子读改写如 CAS/atomic RMW、缓存行锁、总线锁，fence/barrier多核同步）；数据预取器（L1 PHT（prefetch history table）/TLB prefetcher/region prefetcher/L2 prefecther）
- FSU（FP/SIMD Unit）：FP/SIMD Issue Queue、FP/SIMD PRF、FSU Pipe执行单元；
  - Cx. FP/SIMD：2×FP；FP32 FMA 2/cyc（128b），FP64；FADD 4/FMUL 5/FMA 5–7 cyc；NEON128 2/cyc；SVE512 FMA ≥2/cyc；SVE128b FMA
- Crypto：AES+PMULL/SHA1/SHA2/SHA3/SHA256/CRC32/SM3/SM4/EOR3/XAR/BCAX
- MMU：L2-TLB、PTW（页表遍历）、PWC（页表遍历缓存）
- L2：L2-Cache（tag/data/TQ/victim/uncore/DSU）、核缓存一致性（MESI/MOESI、snooping 或目录协议）
- L3：L3-Cache/SLC（tag/data）
- RAS：RAM 保护 (ECC/parity 矩阵)、架构化 RAS (寄存器/异常/ESB/poison)、错误注入、平台 RAS 栈 (ACPI/EDAC)

---

## 2. CPU 微架构对比与 SDC 敏感性

### 2.1 微架构分组与单元全景

| 分区 | 逻辑图单元清单 |
|---|---|
| **IFU 指令供给前端** | 分支预测（BP/BRE 方向、BTB、间接预测、GHB、BPIQ、返回栈 RAS）;µop/MOP cache;L1i-Cache（tag/data）;L1-iTLB;Instruction Queue |
| **OoO 乱序执行引擎** | Int Decode→Rename→Dispatch;FP/SIMD Decode→Rename→Dispatch;**ROB（重排序缓冲：乱序执行/按序提交/精确异常）** |
| **IEX 整数执行** | ALU Issue Queue;LSU/MDU/SYS Issue Queue;Int PRF;ALU×3;MDU（乘除）;MSR/CP15 |
| **LSU 访存单元** | LS1/LS2（AGU/load）;STD1/STD2（AGU/store）;L1-dTLB;Store Queue;L1d-Cache（tag/data/aux tag）;原子与同步单元;数据预取器（PHT/TLB/region/L2） |
| **FSU 浮点/向量;C3 Crypto** | FP/SIMD Issue Queue;FP/SIMD PRF（V/Z）;FSU Pipe 0/1（FADD/FMUL/FMA/NEON/SVE）;Cx 吞吐参数;Crypto（AES/PMULL/SHA/SM3/SM4/CRC32） |
| **MMU 地址转换** | L2-TLB;PTW（页表遍历）;PWC（遍历缓存） |
| **L2 与核缓存一致性** | L2-Cache（tag/data/TQ/victim）;核缓存一致性（MESI/MOESI/snooping/目录）;簇/互连（DSU/SCU·CHI·snoop filter） |
| **L3/SLC;内存** | L3-Cache/SLC（多核共享·可选）;DRAM（经 CHI/内存控制器） |
| **RAS（横切 A–G）** | RAM 保护（ECC/parity/SED）;架构化 RAS（ERR*/异常/ESB/poison）;错误注入;平台 RAS 栈（APEI/GHES/EDAC/BMC） |

### 2.2 全单元对比总表

| 分组 | 单元 | Kunpeng 920 (TSV110) | 920f (part 0xd22) | Neoverse N1 | Neoverse N2 | Neoverse N3 |
|---|---|---|---|---|---|---|
| **IFU** | BP/BRE 方向预测 | 两级动态 ≈A73；1 taken/cyc | 未测（bpbench 中断） | 动态预测器 | 动态预测器 | 动态预测器 |
| | BTB | L1 64 / L2 ~2048 | 未测 | 有，容量未披露 | 有，容量未披露 | 有，容量未披露 |
| | 间接预测 / GHB / BPIQ | ≈16 目标/分支、全局 ~256 | 未测 | 有 GHB/BPIQ（RAS 表可证，容量未披露） | 有（未披露） | 有（未披露） |
| | 返回栈 RAS | 31–32 项 | 未测 | 有 | 有 | 有 |
| | µop / MOP cache | **无**（L1i 溢出带宽 4→0.25 条/cyc） | 未披露 | 无 | ★ **L0 MOP 1536 项 4-way skewed** | 无（相对 N2 删减） |
| | L1i-Cache | 64KB/4-way **AIVIVT** | 32KB/4-way | 64KB/4-way VIPT→PIPT | 64KB/4-way VIPT→PIPT | 32/64KB(可配)/4-way |
| | L1-iTLB | 32 项全相联 | 未测 | 48 项全相联 | 48 项全相联 | 32 项全相联 |
| | Instruction Queue | 未公开 | 未公开 | 未披露 | 未披露 | 未披露 |
| **OoO** | Int Decode | 4 宽 | 未公开 | A32/T32/A64 | A32/T32/A64 | **仅 A64** |
| | Int Rename | PRF ~128 + Flag ~31 | 未公开 | 未披露 | 未披露 | 未披露 |
| | Int Dispatch / ROB | ~128（实测有效 108–110） | 未公开 | 128（公开规格） | 未披露 | 未披露 |
| | FP/SIMD 译码·重命名·分发 | 2×FP 管线 | SVE512 译码 | NEON 128b | SVE2 128b | SVE2 128b |
| | 调度器 Issue Queue | ALU/LS/FP 各 ~33 | 未公开 | issue queues（容量未给） | issue queues | issue queues |
| **IEX** | ALU Issue Queue | ~33 | 未公开 | 未披露 | 未披露 | 未披露 |
| | Int PRF | ~128 | 未公开 | 未披露 | 未披露 | 未披露 |
| | ALU | 3 ALU，分支占 2 口 | 未公开 | 整数执行单元 | 整数执行单元 | 整数执行单元 |
| | MDU 乘除 | 乘 4 / 除 19（早退） | 未公开 | 未披露 | 未披露 | 未披露 |
| | MSR/CP15 | 有 | 有 | 系统寄存器 | 系统寄存器 | 系统寄存器 |
| **LSU** | LS×2 / STD×2（AGU） | 2 AGU：2 load 或 1L+1S/cyc | 未公开 | load/store 单元 | LSU | LSU |
| | L1-dTLB | 32 项全相联 | 未测 | 48 项全相联 | 44 项全相联 | 48 项全相联 |
| | Store Queue | 未公开 | 未公开 | 未披露 | 未披露 | 未披露 |
| | L1d-Cache | 64KB/4-way，load-to-use 4 cyc | 32KB/8-way（~10 cyc） | 64KB/4-way，2×128b 读 | 64KB/4-way | 32/64KB(可配)/4-way/16 bank |
| | 原子与同步 | LSE 完整（casal 43 cyc） | LSE + LRCPC2/3 | LSE | LSE | LSE |
| | 数据预取器 | L1/region/L2 预取 | 未测 | L1 PHT（**无保护**） | 有（未披露） | VA/PC 引擎（L2 预取） |
| **FSU** | FP/SIMD Issue Queue | ~33 | 未公开 | 未披露 | 未披露 | 未披露 |
| | FP/SIMD PRF | 偏小（32×128b） | ★ **Z0–Z31 ×512b + SME** | NEON 128b | SVE 128b（32×128b） | SVE2 128b |
| | FSU Pipe | FP32 FMA 2/cyc；FP64 **1/4 rate** | SVE512 FMA ≥2/cyc（实测 13.6 flop/cyc 下限） | NEON 128b | SVE2 128b | SVE2 128b |
| **C3. Crypto** | AES/SHA/SM/CRC | AES+PMULL·SHA1·SHA2(仅 256)·CRC32；无 SHA3/SM3/SM4 | AES·SHA1/2/512·SHA3·SM3/SM4·CRC32·SVE2 crypto | 可选 Crypto | SVE2 crypto + 可选 | v9.2 全量（SHA3 等） |
| **MMU** | L2-TLB | 1024 项共用（+11 cyc） | 未测 | 1280 项 5-way | 1280 项 5-way | ★ **分裂：small 1536/6-way + medium 256/4-way + walk cache** |
| | PTW / PWC | 页表遍历 | 未披露 | 4 并发遍历 + 预取 | translation table prefetcher | walk cache + prefetcher |
| | MMU/TLB 保护 | 无披露（RAS=0） | 未披露 | MMUTC 2×交织 parity；**L1 TLB=flops 无保护** | MMUTC SED | TLB 整体 SED |
| **L2 一致性** | L2-Cache | 512KB/8-way（10 cyc） | ★ **768KB/12-way（17 cyc）** | 256–1024KB/8-way（TQ 24/36/48） | 512/1024KB/8-way | 128KB–2MB/8-way/2-bank PIPT |
| | L2 RAM 保护 | 声称 ECC（无证据） | 未披露 | tag+data+TQ SECDED | tag+data+TQ SECDED | SECDED（granule 128/256b 可配） |
| | 核缓存一致性 | HHA 目录 + bufferless 环 NoC | HCCS（跨 socket NUMA 61–91） | DSU SCU + snoop filter（MESI） | DSU-110 | DSU-120;CHI-E 256-bit |
| **L3/内存** | L3 / LLC | 32MB/die·15-way·128B 行·tag 在簇·S/P/P | ★ **无 L3/LLC** | DSU 内可选 L3 | DSU-110 L3 | Direct connect，**无 L3/SCU** |
| | 内存接口 | DDR4-2933 ×8ch（~187 GB/s） | 565GB·16+16 NUMA·8×200G | 48-bit PA·GICv4.1 | DSU-110 | 48-bit VA/PA·MPAM·CHI-E |
| **RAS** | H2 架构化 RAS | ✗ **RAS=0**（无 ERR*/ESB/poison） | ✓ RAS=1（黑盒，无 TRM） | v8.2 完整（CE/DE/UE） | v9.0 全量，Node0=L1+L2 | v9.2 全量，Node0=L1+L2+MMU/TLB |
| | H3 核心错误注入 | 仅平台级 EINJ（固件） | 未披露 | CE/DE/UC 注入 | CE/DE/UC 注入 | CE/DE/UC 注入 |
| | H4 平台 RAS 栈 | HEST/EINJ/BERT/ghes_edac | 未披露 | FHI/ERI/ESB/poison | FHI/ERI/ESB/poison | FHI/ERI/ESB/poison |

### 2.3 SDC 敏感性：现状;薄弱点;加固点

五款核在「RAM 阵列保护」这一维度上披露得最多，但**保护范围几乎全部止步于存储阵列 SRAM**。
下面按主题逐层挖掘，其中第 1–4 点是对五款核的**共同**结论，第 5 点是分核画像，第 6 点是加固优先级。

#### 2.3.1 共同盲区之一：执行数据通路与 flop 状态零保护（最高危）

TRM 与 Kunpeng 文档的保护矩阵只覆盖「会保存 dirty/clean 数据的 SRAM 阵列」：cache tag/data、
TLB、TQ、MOP cache。**没有任何一款核披露 ALU、PRF、ROB、调度器（Issue Queue）、scoreboard、
bypass/转发网络、重命名映射表的保护**——这些面向单周期吞吐的结构主要由寄存器堆/flop/CAM 实现，
本身不享受 SRAM 阵列的 ECC 包裹。

最直接的证据来自 N1：「The L1 TLBs are implemented with flops, so there is no cache protection
for L1 TLBs.」——它明确承认 flop 实现 = 无 cache 保护。这一逻辑等价成立于 PRF、ROB、调度
器：它们同样是 flop 结构，同样无 ECC/parity 披露。

> **SDC 含义**：对故障注入（FI）建模而言，执行阵列的翻转是**无保护、且无架构级出口的静默损坏**。
> 一个 ALU 结果 bit 或 PRF 读口 bit 翻转会被当"正确结果"提交到架构状态，五款核无一能检出。
> 这是五款乱序核共同的高危注入面，也是 DIMM 级 ECC 与缓存 ECC 都**覆盖不到**的真空区。

#### 2.3.2 共同盲区之二：分支预测器普遍无保护

N1 的 RAM 保护表把「L1 BTB / L1 GHB / L1 BPIQ / L1 PHT」四行明确标为 **None（无保护）**。
N2/N3 的保护表同样只列 cache/TLB/TQ/MOP，**不含 BTB/GHB/BPIQ**。华为 920 分支预测器的保护
未披露（且 RAS=0 时即使有保护也没有架构出口）。

> **SDC 含义**：方向/目标预测翻转大多表现为误预测→被 ROB 回滚（相对可容错、且可观测为性能
> 恶化）；但**间接目标/GHB 翻转**可能把控制流导向错误却"合法"的代码路径，属于潜在的静默控制流
> 破坏。加固方向明确且成本低：指令侧天然可恢复，给预测器加 parity，命中错误即 flush+refetch——
> N1 的「L1I tag parity + invalidate/refetch」已示范这一策略。

#### 2.3.3 共同盲区之三：SED-only 阵列的双 bit 翻转静默

所有「只保存 clean 数据」的 SRAM 只用 SED（Single Error Detect）parity：L1I tag/data、MOP cache、
MMUC/MMUTC、TLB。SED 只能检单 bit（触发 invalidate + refetch 恢复），**同一 granule 内的双 bit
翻转不检测**，直接静默损坏。N1/N2/N3 的表述完全一致（"For RAMs with only SED, the core does not
detect a double bit error. This might cause data corruption."）。

对比之下，承担 dirty 数据的 L1D/L2 tag/data、TQ 用的是 SECDED（可纠单 bit、检双 bit）。

> **SDC 含义**：clean-data 阵列对「多 bit 累积翻转」（老化、辐射 burst、供电扰动）暴露。特别是
> N2 的 **MOP cache 是全新且 SED-only 的指令面靶点**：它存译码后操作，翻转→执行错误指令→可能
> 直接产出错误结果且无探测。加固方向：对 clean-data 阵列升级 SECDED，或加周期 scrubbing 把
> 单 bit 在积累成双 bit 前刮除。

#### 2.3.4 保护纵深单调递增：920 < 920f(未知) < N1 ≤ N2 < N3

从架构化 RAS 能力看，五款核形成明显的纵深梯度：

- **920（RAS=0）**：无 RAS 架构扩展 → 无 ERR* 寄存器、无 ESB、无 poison。核内任何未保护阵列
  （分支预测器、flop 执行阵列）翻转**绝对静默**——无一架构级可见信号。唯一防线是片外 DDR4
  RDIMM SECDED（DIMM 级）与固件 ghes_edac。文档声称的 I$/D$/L2「ECC」**无架构化证据**（PFR0.RAS=0
  与"有 ECC"矛盾，ECC 若存在也缺错误记录出口）。
- **920f（RAS=1 黑盒）**：PFR0.RAS=1 说明有扩展，但无公开 TRM，保护矩阵不可知。更关键的是
  **SVE512（Z0–Z31×512b = 16 Kbit/核寄存器）与 SME/SME2（ZA tile 大状态）构成全新巨型数据面**，
  其保护完全未披露 → 黑盒按最坏假设（无保护）建模。
- **N1**：披露最透明的参照系——给出完整保护矩阵 + **无保护清单** + SDC 定义（"These are silent
  data corruptions"）。SECDED 盖 dirty，SED 盖 clean，BTB/GHB/BPIQ/PHT/flop 明确无保护。无 SVE，
  向量数据面仅 NEON 128b（小）。
- **N2**：整体 ≈N1，增量是 SVE2 128b 与 **MOP cache（SED-only 指令面附加靶点）**，架构升到 v9.0-A、
  Node0=L1+L2。
- **N3**：披露范围内**最低**（相对而言）。新增 **L1D aux tag 单独 SECDED**（覆盖 N1/N2 未保护的
  L1D 辅助标签状态）、**TLB 整体 SED**、**L2 ECC granule 可配 128/256b**、分裂 L2 TLB + walk cache、
  CHI-E 256-bit、v9.2-A 全量、Node0 覆盖 MMU/TLB。但执行阵列仍无披露。

#### 2.3.5 分核 SDC 画像

| 核 | SDC 敏感性定位 | 关键薄弱点 | 最值得打点的注入靶 |
|---|---|---|---|
| **920** | 最高（核内翻转无架构可见信号） | RAS=0、ECC 声称无证据、分支预测器无保护、flop 执行阵列 | 任意未保护 SRAM + 执行 flop |
| **920f** | 高（黑盒 + 巨型向量数据面） | SVE512/SME 无披露保护、无 TRM 保护矩阵 | Z 寄存器、SME ZA tile |
| **N1** | 参照系（披露最全） | BTB/GHB/BPIQ/PHT 无保护、L1 TLB flops | BTB/GHB、L1d aux 状态 |
| **N2** | ≈N1 + MOP 独立靶点 | MOP cache SED-only（双 bit 静默） | L0 MOP cache |
| **N3** | 披露范围内最低 | 执行阵列仍无披露 | L2 TLB 分裂结构、执行 flop |

#### 2.3.6 潜在加固点（按优先级）

1. **P0 — clean-data SED 阵列升级**：L1I/MOP/MMUTC/TLB 由 SED 升 SECDED，或加周期 scrubbing，
   消除双 bit 静默，这是 Arm 公版核最一致的结构性短板。
2. **P0 — 执行通路加固/披露**：对 PRF、ROB、调度器、ALU 通路至少做「关键字段 parity / residue /
   lockstep」并**披露现状**——当前五款在这块的沉默意味着 FI 只能按最坏假设。
3. **P1 — 分支预测器 parity + flush/refetch**：指令侧可无损恢复，代价极低，收益是消灭静默控制流破坏。
4. **P1 — 华为侧补救**：920 补齐 RAS 架构扩展（ERR*/ESB/poison），把「声称 ECC」翻译为架构可观测；
  920f 披露 SVE512/SME 状态的保护方案。
5. **P2 — FI 建模约定**：「未披露/未测」按**无保护**处理（黑盒最坏假设）；对 SED-only 与无保护
   阵列注入多 bit 翻转，对执行 flop 阵列注入单 bit；优先在 SED-only 阵列做「单→双 bit 累积」实验。

## 3. 各芯片微架构功能图（★ = 独有/标志性设计）

> 渲染说明：GitHub 会剥离 markdown 内联 SVG（防 XSS），仓库相对路径的 `.svg` 也无法稳定渲染；
> 故五图**直接引用仓库内同名矢量 `.svg`**（经 jsDelivr CDN 以 `image/svg+xml` 提供），不转 PNG、矢量无损放大。
> SVG 源文件即 `figures/` 下同名文件，本地浏览器可直接打开编辑。

### Kunpeng 920

<div align="center">

![Kunpeng 920 微架构功能图](https://cdn.jsdelivr.net/gh/wangxumarshall/gem5-fi@fi-fuzz/docs/cpu/arm64/figures/sdc-fig-920.svg)

</div>

独有/标志性：L3/SLC 三模式（Shared/Private/**Partition 默认**）+ **128B 行**（L1/L2 是 64B）+ tag 在簇侧；
无 µop cache（取指带宽悬崖）；**RAS=0**（无架构化 RAS，全图唯一的"裸奔"平台）；AIVIVT L1I；NEON 128b 上限。

### 920f（part 0xd22）

<div align="center">

![920f 微架构功能图](https://cdn.jsdelivr.net/gh/wangxumarshall/gem5-fi@fi-fuzz/docs/cpu/arm64/figures/sdc-fig-920f.svg)

</div>

独有/标志性：**SVE 512-bit + SME/SME2**（五款唯一宽向量）；**768KB/12-way L2**（非常规配置）；
**无 L3/LLC**（少一级缓存暴露面）；**64KB 强制页**（TLB 翻转波及面 ×16 放大器）；RAS=1 但防御矩阵黑盒。

### Neoverse N1

<div align="center">

![Neoverse N1 微架构功能图](https://cdn.jsdelivr.net/gh/wangxumarshall/gem5-fi@fi-fuzz/docs/cpu/arm64/figures/sdc-fig-neoverse-n1.svg)

</div>

独有/标志性：**ETM**（指令 trace，N2/N3 改 ETE+TRBE）——五款唯一仍在用 ETM 型 trace 单元；
AArch32 EL0（A32/T32/A64，与 N2 相同；920/920f/N3 无）；
三级原子执行（near L1 → far CHI → DSU L3，N2/N3 亦有同类机制）；L2 TQ 24/36/48 项可配（五款唯一把 TQ 深度列为构建选项）；
TRM 明示无保护清单最完整（BTB/GHB/BPIQ/PHT/L2 victim/L1 TLB flops）——本组"披露透明度参照系"。

### Neoverse N2

<div align="center">

![Neoverse N2 微架构功能图](https://cdn.jsdelivr.net/gh/wangxumarshall/gem5-fi@fi-fuzz/docs/cpu/arm64/figures/sdc-fig-neoverse-n2.svg)

</div>

独有/标志性：**L0 MOP 缓存 1536 项 4-way skewed**（存已译码优化指令，SED 弱保护×高命中×指令面，五款唯一 MOP）；
**MMUTC 拉进 SED**（N1 仅 2-bit 交错 parity）；AArch32 EL0（A32/T32/A64，与 N1 相同）；write streaming L1+L2 双级；
load VA / store PA 分裂预取器（N3 改为 VA+PC 双源引擎）。

### Neoverse N3

<div align="center">

![Neoverse N3 微架构功能图](https://cdn.jsdelivr.net/gh/wangxumarshall/gem5-fi@fi-fuzz/docs/cpu/arm64/figures/sdc-fig-neoverse-n3.svg)

</div>   

---

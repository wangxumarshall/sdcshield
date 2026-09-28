# B — cn23154 / Kunpeng 920 CPU 微架构画像与已知 SDC 故障签名

> Agent B 研究笔记(2026-09-16)。任务:为「最大化激发浮点向量单元 SDC」的测试增强提供硬件侧依据。
> 数据来源:本仓 docs/cpu/ 下全部文档 + auto-memory 故障档案 + tests/cpu/arm64/ 源码。
> **诚实边界**:cn23154(0xd22)是未公开发布的核,其核内微架构(ROB/调度器/端口/PRF 具体数值)仓库文档**未覆盖**;凡引用 TaiShan v110 / Neoverse N1 的结构数据均为参考机或公开口径,不是 0xd22 实测。推断处均已标注。

---

## 1. cn23154 硬件画像

### 1.1 核心事实(全部来自 docs/cpu/cn23154.md,实测)

| 维度 | 值 | 关键证据 |
|---|---|---|
| CPU | 2 socket × 304 核 = 608 核,无 SMT,HiSilicon part 0xd22,rev 0(未公开新一代 TaiShan) | MIDR 0x00000000480fd220 |
| ISA | ARMv9:SVE 512-bit(VL=64B,ZFR0 SVEver=1,f32mm/f64mm/BF16)、**SME+SME2**(SMFR0 f64f64/b16f32/f32f32/i8i32=1,fa64=0)、SVE2 全集、AES/SHA3/SM3/SM4、LSE、LRCPC2/3、FCMA、JSCVT、i8mm/bf16、RPRES、WFXT | ID 寄存器实测 |
| RAS | PFR0.RAS=1、DIT=1、CSV2/3=1、AMU=1;**无 RNDR/BTI/MTE** | PFR0/PFR1 |
| 主频/调频 | 2.0 GHz 定频(cppc_cpufreq + userspace,1200–2000 MHz 可调但锁 MAX)| — |
| 页粒度 | **仅 64KB**(TGran4=0xf);PAGESIZE 实测 4096 是内核兼容层假象 | — |
| 缓存 | L1D 32KB/8-way(~10 cyc);L1I 32KB/4-way(CTR_EL0=0x9444c004,ERG=64B);**L2 768KB/12-way(~17 cyc)私有**;**无 L3/LLC(sysfs 无 index3,CLIDR 陷阱)**;SCN/远端 45–90 ns;跨 NUMA 内存 ~130 ns | 指针追踪微基准 |
| NUMA | 16 计算节点(每节点 38 核 + 31–33 GB;socket0=node0–7,socket1=node8–15)+ 16 个无 CPU 内存节点(node16–31,各 4 GB,距离 61–91,疑似跨 socket 近存/CXL);同 socket 距离 20/30/40,跨 socket 61/71/81/91 | numactl 距离矩阵 |
| 互连/IO | HCCS 类片间;8×200GbE RoCE(hns3,roceroh0–7);PCIe Gen4;UnCore PMU 171 设备:8 SCCL×(4 DDRC+4 HHA+16 UC)、14 SICL L3、SPE、PTT | — |
| OS | Kylin V10,内核 5.10.0-285;608 核全部 isolcpus+nohz_full,每 NUMA 留末核处理中断 | — |
| PMU | PMUv3(6 通用计数器)+ SPE(ARM Statistical Profiling Extension) | — |
| FMA 吞吐(自测) | 标量 4 flop/cyc;NEON128 ~6.75;SVE512 ~13.6(下限,理论 16,初版基准被编译器削链待复测)→ 单核理论 64 flop/cyc = 128 Gflop/s,节点 77.8 Tflop/s | flopbench |

### 1.2 簇/拓扑代数(推断,由已知数字反推)

- NUMA3 = cpus 114–151 = **38 核**,与「每 NUMA 节点 38 核」吻合 → **一个 NUMA 节点 = 一个 cluster/L3 域 = 一个 SCCL 级 die 分区**(memory 故障档案称其为"一个 cluster/L3 域")。
- 304 核/socket ÷ 8 NUMA/socket = 38 核/NUMA,同构于 Kunpeng 920 的 SCCL→CCL 两级(chiplet 内 8 个 4 核 CCL)结构放大版【推断:0xd22 延续 SCCL/CCL 分层但簇更大】。
- **无 LLC 是与 Kunpeng 920 最大的代际拓扑差异**:920 每 die 有 32MB L3(SLC,tag 在簇侧,partition 模式默认);0xd22 取消 LLC,768KB 私有 L2 直接面对 SCN 网状互连(8–16MB 呈 45–90ns)。这意味着一致性/共享压力全部压到互连与 snoop 通路,SDC 可观测点从 L3 bank 移到 SCN/远端路径【推断:故障锚定在簇而非 L3 bank,与无 LLC 拓扑自洽】。

### 1.3 与 Kunpeng 920(part 0xd01, TaiShan v110)的代际差异

| 维度 | Kunpeng 920 / v110(文档实据) | cn23154 / 0xd22(文档实据) | 性质 |
|---|---|---|---|
| ISA | ARMv8.2,无 SVE/SVE2/PAC/BTI/LRCPC/SHA512/SM4,RAS=0(PFR0)| ARMv9,SVE512+SVE2+SME/SME2+RAS=1+全套国密 | 两者均实测 |
| 向量宽度 | NEON 128-bit,FP0/FP1 双口,FP32 FMA 2/cyc,FP64 quarter-rate | SVE 512-bit,实测 SVE512 FMA ≥2×512b/2cyc 口径(13.6/16 flop/cyc) | 920 公开口径;0xd22 自测下限 |
| L1D | 64KB/4-way/2×128b,load-to-use 4 cyc | 32KB/8-way,~10 cyc | 均实测(两机) |
| L2 | 512KB/8-way/10 cyc | **768KB/12-way/~17 cyc** | 均实测 |
| L3 | 32MB/die SLC,partition 模式,tag 在簇侧,行 **128B** | **不存在** | 均实测 |
| 核内 OoO | 4 宽,ROB ~128(有效 108–110),调度器 3×33,整数 PRF ~128,FP PRF 偏小,无 µop cache | **文档未覆盖(未公开核)** | 920 实测/公开;0xd22 空白 |
| 分支预测 | 两级动态,近似 A73;L1 BTB 64,L2 BTB ~2048,RAS 31–32 | 待节点空闲补测(bpbench,未决项) | 0xd22 文档明示未测 |
| PMU | PMUv3 6 计数器;v110 imp-def 事件族(mem_stall_* 等);无 topdown(slots=0);uncore 需 root | PMUv3 6 计数器 + **SPE 可用**(920 HWCAP 无 spe) | 均实测;0xd22 的 imp-def 事件空间**未扫描**(920 的 0x00–0xFF raw 扫描方法可直接移植【推断:同 implementer 事件号族有延续性,但需逐个标定】) |

**代际定位总结**:0xd22 ≈ v110 的"宽度×向量域"放大继任(ARMv9 + SVE512 + 无 LLC + 簇 38 核 + SPE)。凡测试设计需要核内 OoO 参数(ROB 深度、IQ 项数、FP PRF 项数)的,只能以 v110/N1 作量级参考,结论须标推断。

### 1.4 频率/电压域(与 SDC 激发的关联)

- cn23154:2.0 GHz userspace 锁频 → 无变频噪声,测量稳定(cn23154.md §5.6);`--vary-frequency` 在本仓 Kunpeng 920 参考机上无 cpufreq(CLAUDE.md 平台怪癖)→ **电压/频率维度目前在两机上都不可软件操控,di/dt 类激发只能靠负载形态本身**(burst/stall 交替,见 power_virus_dit)【实据:频率固定;推断:无调频即无法做 Vmin 边缘测试,cpu-sdc.md 建议的"低谷期主动下调 Vmin"路径在这两台机器上不可行】。
- Kunpeng 920:调频粒度是 **die 级**(4 policy 域 = 每 NUMA 一域,32 核/域共享频率决策)→ 若未来可调,同簇饱和与频率域是同一物理域【推断:0xd22 大概率延续 die/簇级电压域,簇饱和 = 同电压域饱和,这正是"簇内并发度"成为触发要素的物理基础】。

---

## 2. 已知故障签名全解(NUMA3 VA 通路瞬态失效)

### 2.1 签名档案(memory: cn23154-numa3-va-path-fault + 任务书背景)

| 要素 | 内容 |
|---|---|
| 故障机 | cn23154,NUMA3 = cpus 114–151(一个 cluster/L3 域) |
| 故障性质 | **簇锚定的虚拟地址通路瞬态时序失效**(时序边缘型,非永久硬故障) |
| 损坏点 | **RF 读出 → load-to-use 旁路前递 → AGU → SVE LSU 地址入端**(地址数据通路,不是运算通路) |
| 位签名 | 损坏位集中 **VA[55:48]**;TCR TBI0=1 使 VA[63:56] 被静默忽略;若 VA[47:0] 受损则表现为静默 SDC |
| 可见形态 | ①SEGV(4 例,si_addr 低 48 位完好且落在合法映射域 0x0000_4001_xxxx_xxxx)②静默 SDC |
| 触发三要素 | (a) **簇饱和 ≥36 线程**(38 核簇留 2 核余量);(b) **base 指针从栈槽重装载后 2–3 条指令内进入 SVE 访存寻址**(ld1rd/stnt1d 窗口);(c) SVE 密集访存 |
| 复现节律 | ~19 min 量级 |
| 零复现对照 | HPL 70h、sdcshield eigen 68.65h、单线程探针 → 全部零复现 |

### 2.2 故障通路逐段解读(与仓内微架构文档对齐)

1. **RF 读出 → 旁路前递**:内部文档 §5.4 明证"旁路网络是纯导线+传输门,无 ECC/parity,交叉点 O(N×M×位宽),数千 bit 矩阵"。cpu-sdc.md 把「PRF 读写旁路」列为 SDC 敏感单元之三(串扰 → 过期值捕获)。0xd22 的 SVE 512-bit 使旁路宽度翻 4 倍于 v110 的 NEON 128-bit【推断:0xd22 旁路矩阵按 512-bit 组织,布线密度与时序余量压力相应放大——这是位签名落在地址通路旁路段的微架构温床】。
2. **AGU 地址入端**:内部文档 §6.2"AGU 输出到 MMU 的地址通路是 D2 通路,地址生成逻辑无保护,合法地址与垃圾地址在位级同样合法"。V110 AGU 单周期合同(base+offset+extend+shift)在 SVE 密集寻址下每周期要供 2×512-bit 的地址/数据流【推断:SVE LSU 的地址入端口数与位宽都比标量 AGU 场景宽,时序边缘最先失守】。
3. **TBI0=1 的静默放大**:VA[63:56] 被忽略 → 高位损坏既不 fault 也不可观测;VA[55:48] 恰在"被翻译硬件使用但通常为 0"的窗口 → 48 位以下完好时 si_addr 看起来合法。**这解释了为何一半形态是 SEGU、一半是静默 SDC**:取决于损坏位是否落入 [55:48] 与目标地址是否恰好映射。
4. **簇饱和的物理含义**:38 核簇 = 推断的同一电压域/供电分区。≥36 线程同时打满 SVE FMA+LSU → 局部 Ldi/dt 电压暂降(cpu-sdc.md:多输入翻转+阻性缺陷 → 时序裕量被吞)。这与 cpu-sdc.md 的 SDC 物理机理(MIS、Vdroop、test escape)完全同构。
5. **ld1rd/stnt1d 重装载窗口**:栈槽 reload → 2–3 条指令内进 SVE 寻址 = 恰好是「RF 读出/旁路前递 → AGU」的背靠背距离。旁路网络在这几拍承载 base 指针 → 时序违例在此窗口采样错误位。sve512_stencil_axis_arm.cpp 已把该窗口逐字保留(svdup_f64(coeff[r]) 的 ld1rd codegen + 栈上 coeff 表 + 内存驻留循环计数 + VA[63:48] canary)。

### 2.3 为什么 HPL/eigen 长跑零复现,而特定配方能复现

| 维度 | HPL / eigen | 成功配方(stencil axis 内核) |
|---|---|---|
| 访存形态 | 大块连续 DGEMM 面,base 指针长期驻留寄存器,寻址以增量为主 | 每轮 r 从**栈上 coeff 表**重读系数(ld1rd)、base 指针反复从栈槽重装载 |
| 指令窗口 | BLAS 内核 hand-typed,ld1rd/stnt1d 类 replicate/非临时访存占比低 | ld1rd + stnt1d(非临时 store)恰是主角 |
| 簇并发 | MPI rank 绑核各跑各的,簇内未必同时饱和;且通信核心有同步空隙 | ≥36 线程同簇打满,无空隙 |
| 数值口径 | 浮点误差累积,无法做位精确比较(微小 SDC 被数值噪声淹没) | 每通道整数精确,bit 级可判定 |
| 节律匹配 | 70h/68.65h 未命中 19min 节律的窗口条件组合 | 条件组合在分钟级反复重建 |

**对测试设计的核心启示**:
1. **"强度"不等于"激发"**——HPL 70 小时的算力总量远超 19min 配方,但激发要素是**结构性的**(重装载窗口+簇饱和+SVE 寻址),不是吞吐量的函数。测试必须精确复刻结构,不能靠加压。
2. **位级可判定**是捕获静默 SDC 的前提(HPL 里 VA[55:48] 损坏混在舍入误差里)——沿用本仓 byte-exact memcmp golden 铁律(CLAUDE.md)。
3. **触发要素必须并发存在**:单线程探针零复现证明簇饱和不可省;sdcshield 默认 fork 模式每 test_run 占一核,满核即天然簇饱和(608 核时)。
4. 19min 节律 → 测试时长预算:单次迭代 ≥20min 且循环,才有机会命中;短测试(默认 5s)对该故障**完全盲**。

---

## 3. 浮点/向量通路微架构分解(SDC 易损点 × 压力手段)

> 结构口径来自 arm64-microarchtecture-internals.md(以 V110/N1/gem5 三源互证)+ cpu-sdc.md(敏感单元拓扑)。0xd22 的对应数值未公开,适用时标推断。

| 通路段 | 已知结构(v110/N1 口径) | SDC 易损点 | 可用压力手段(仓内已有/可加) |
|---|---|---|---|
| **向量寄存器堆 (FP/Vec PRF)** | v110 FP PRF 偏小(32 架构 reg 占重命名空间,易压满);PRF 阵列可带 parity,但**背靠背链走旁路不写 PRF**(forwarding 掩蔽定律:P(SDC\|reads>0)=1.000,PRF 翻转仅 3.9% 可见于自然负载) | ①RF 单元翻转在紧凑链中被旁路掩蔽,**松散跨块数据流(生产者已退休、消费者晚到)才可见**;②SVE 下 Z 寄存器 32×512b,读出端口宽度大【推断】 | 既有:sve512_*_chain(紧凑链)、ooo_dep_chain(指针追逐)。**建议补**:拉长生产者-消费者距离的 Z 寄存器重用模式(spill 后隔 >ROB 深度再读)——专门打掩蔽定律的反面【推断:0xd22 ROB 更深,距离阈值需实测】 |
| **FMA 流水线 (FSU)** | v110 FP0/FP1 双口 128b;FMA=Wallace 树+指数对齐移位器+LZA,深层组合逻辑,数据敏感型时序违例重灾区(cpu-sdc.md 最高风险);0xd22 实测 2×512b FMA 口径 | 进位链/压缩树对**高 Hamming 操作数交替翻转**最敏感(MIS);低位全 1 的长进位传播 | 既有:fsu_byteexact_arm(高 Hamming 表 + byte-exact)、sve512_f32/f64_chain、power_virus_dit(burst/stall di/dt + 操作数交替)。**建议补**:SVE512 版 di/dt 病毒(当前 power_virus_dit 是 NEON128)——512b 宽度下同一 burst 的瞬态电流和位翻转密度都×4【推断:需实测定标】 |
| **load/store 通路 (LSU/AGU)** | v110 2×AGU,2 load 或 1 load+1 store/cyc;load-to-use 4 cyc(0xd22 ~10 cyc L1D);STLF 6–7 cyc;L1D 保护 32bit 粒度、L2 64bit | **①AGU→MMU 地址通路(D2)无保护——本故障的实际命中点**;②STLF CAM 比较器+merge mask(cpu-sdc.md 高风险,内存 ECC 看不见旁路数据);③SVE gather/scatter 的 per-lane 地址生成使 CAM/对齐逻辑×8 lane【推断】 | 既有:agu_stress_2src(3 ldr+2 str/元素)、lsu_store_forward(全宽度×偏移×跨行转发)、l2c_cross_cache_line(跨行 stp/ldp)、sve512_gather_scatter(gather+FMA+scatter 间接寻址)。**建议补**:SVE 首次/非临时访存形态(ldnt1b/stnt1d、ld1rd replicate)与栈重装载窗口的组合定向测试——即 stencil_axis 的泛化【故障实据:ld1rd/stnt1d 窗口就是命中窗口】 |
| **gather/scatter** | SVE 专属:svld1_gather_u64index 每通道独立地址 → LSU 地址端口串行化/并行化压力 + dTLB 多项并发 | 索引计算错误/地址 merge 错误位级都合法;间接性掩盖错误传播 | 既有:sve512_gather_scatter_arm/svd(置换索引 + 同间接 golden)。**建议补**:gather 索引跨 64KB 页边界/跨 NUMA 距离梯度(本地→SCN→远端)的变体,把地址通路置于不同翻译距离下打【推断:故障在 VA 通路,页边界是 TCR/TBI 语义的天然应力点】 |
| **谓词寄存器 (P regs)** | SVE 16×谓词 + whilelt/ptrue 生成;0xd22 SVEver=1 | 谓词位翻转 → 通道静默屏蔽/多算,结果位级仍合法(gather_scatter_svd 已用 whilelt) | 既有覆盖薄。**建议补**:谓词密度扫描(1/8→8/8 活动通道)+ 谓词与 FMA/访存组合的 per-lane 校验【推断:谓词路径是 SVE 特有且仓内几乎未打】 |
| **旁路网络** | 纯导线+传输门,无保护,平方级交叉点;记分牌就绪位翻转 → 假就绪(过期值)/假未就绪(Hang) | **本故障损坏点所在段**(地址在 RF→旁路→AGU 途中被篡改);串扰+时序边缘 | 唯一有效手段 = **背靠背生产-消费距离内的密集交替**(power_virus_dit 的 burst 已是形态模板);**建议补**:地址生产者(栈 reload)→ SVE 消费者(ld1rd)距离 1/2/3/4 条指令的扫描矩阵——直接复刻故障窗口做参数化 |
| **rename/ROB/scheduler 控制** | v110 ROB~128、3×33 调度器;0xd22 未公开【推断更宽更深】 | 控制触发器密集、checkpoint 恢复网络;假就绪注入形态 = 过期值 | 既有:ooo_dep_chain(重执行 replay 形态)。**建议补**:分支误预测冲刷窗口内嵌 SVE 依赖链(squash→重取→重执行的二态差异)【推断:19min 节律可能对应特定控制事件重合,冲刷窗口值得显式打】 |

---

## 4. PMU 可观测性(SDC campaign 伴随监控)

> 事件表来自 kunpeng920_pmu_events.md(920 实测)。**注意:这是 v110 的事件空间;0xd22 是新核,架构事件(§2.1–2.4)大概率可用,imp-def 事件族(§3)与 uncore 事件名必须重新探测后才可用**【推断:同 implementer 有延续性但未验证;cn23154.md 只确认 PMUv3 6 计数器 + SPE】。0xd22 独有优势:**SPE 可用**(920 无 spe)——统计剖采样可直接给出延迟/地址分布,是定位时序边缘的新仪器。

SDC campaign 最有价值的监控组(按用途):

| 用途 | 事件(v110 命名) | 层级 | 备注 |
|---|---|---|---|
| 后端/访存停顿分解 | `mem_stall_anyload`(0x7004)/`mem_stall_l1miss`(0x7006)/`mem_stall_l2miss`(0x7007)、`stall_backend`(0x24) | 核心 | **无 LLC 的 0xd22 上 l2miss≈直接打互连/内存**,梯度即簇/NUMA 距离 |
| LSU/缓存挤压 | `l1d_cache_refill_rd`(0x42)、`l2d_cache_refill_rd`(0x52)、`ll_cache_miss_rd`(0x37) | 核心 | 簇饱和时簇内重填风暴量化 |
| 簇内一致性干扰 | L3C `back_invalid`(0x29)、`retry_ring`(0x41);HHA `tx_snp_num`(0x33) | uncore(920 需 root) | 0xd22 无 L3C,对应物应是 SICL L3/SPE 设备,**事件名需重测**【推断】 |
| 远端流量 | `remote_access`(0x31)/`remote_access_rd`(0x38) | 核心 | NUMA3 vs 健康簇对照的伴随指标 |
| 硬件内存故障排查 | `memory_error`(0x1a) | 核心 | 区分内存侧 vs 通路侧故障 |
| 前端/发射不饱 | `exe_stall_cycle`(0x7001)、`iq_is_empty`(0x1043)、`stall_frontend`(0x23) | 核心 | 判断负载是否真把后端打满 |

campaign 设计含义:①6 计数器上限 → 每轮 ≤6 事件或接受 multiplex;②核心 PMU 普通用户可用(perf_event_paranoid=2,uncore 需 ≤1);③对照实验设计(NUMA3 簇 vs 健康簇)可直接复用 920 的方法论,事件名在 0xd22 上先跑一次 raw 扫描(0x00–0xFF,方法已在 920 文档沉淀)再标定。

---

## 5. 对测试增强的硬件侧建议(按激发价值排序,均标注证据/推断)

1. **SVE512 + 栈重装载窗口 + 簇饱和的定向复现族(最高价值)**
   把 stencil_axis 的三要素(栈 coeff 表 ld1rd / base 重装载→2–3 条内进 SVE 寻址 / 满簇并发)做成**可参数化扫描**的家族:reload→use 距离(1/2/3/4 指令)、访存指令形态(ld1rd/ldnt1d/stnt1d/ld1d)、谓词宽度。证据:故障签名实据(memory 档案)+ sve512_stencil_axis_arm.cpp 注释(5 次 bit 级复跑定位现场配方)。运行时需 ≥19min/轮 + 全簇并发(sdcshield 满核即簇饱和)。

2. **簇锚定 campaign 模式(框架层)**
   现有 sdcshield 以 CPU 为单位铺线程,但**没有"选簇"原语**。建议加 cluster-affinity 模式(如 --cpus 114-151 语义化 + 健康簇自动对照),配合每簇独立 golden 比对,把"NUMA3 锚定"变成一等观测维度。证据:故障簇锚定实据;920 topology 文档(cluster_cpus_list 可从 sysfs 取簇边界)。

3. **SVE512 版 di/dt 病毒(扩展现有 power_virus_dit)**
   power_virus_dit 目前是 NEON128 burst/stall。0xd22 的 512b 数据通路 + 38 核簇电压域推断下,同形态的瞬态电流和旁路串扰都放大。做 SVE 变体(burst = 依赖 SVE FMA 链 + 高 Hamming 交替,stall = yield),并在 burst 段嵌地址生成。证据:cpu-sdc.md MIS/Vdroop 机理 + power_virus_dit 注释(研究维度"瞬态故障动态激发");【推断:放大倍数需实测定标】。

4. **地址通路压力专项(AGU→MMU→SVE LSU)**
   故障命中点是地址数据通路。组合:(a) 已有 agu_stress_2src / lsu_store_forward 扩展出 SVE 寻址变体(svld1 with register offset、gather 的 extend+shift);(b) VA 高位 canary 常态化(stencil_axis 已示范:检查 &res0 的 VA[63:48],零成本预警高位翻转);(c) 64KB 页 + 跨页 gather(0xd22 强制 64KB 粒度,页边界行为与 4KB 直觉不同)。证据:内部文档 §6.2 D2 通路无保护实据 + 0xd22 TGran4=0xf 实据;跨页部分为推断。

5. **位级可判定的数值口径(全程铁律)**
   stencil_axis 的教训:原版程序带 RADIUS=4 的陈旧期望,5 次 bit 级复跑才洗掉软件假阳性。所有新测试必须整数化/精确可表示值/byte-exact memcmp,禁止容差比较(fsu_byteexact_arm 已论证 1e-6 容差漏检 1-bit 翻转)。证据:fsu_byteexact_arm 注释 + CLAUDE.md golden 铁律 + stencil_axis 修正史(注释 §7.1)。

6. **谓词/SVE 特有结构的覆盖补洞**
   谓词密度扫描、whilelt 边界、SVE2 特有指令(FCMLA/结构化 load)在仓内几乎空白(9 个 sve512 测试集中在 FMA 链/gather/scatter/stencil)。FCMLA 复数 FMA 是 SVE 服务器核独有数据通路,且 eigen SVD 类负载会用到。证据:tests/cpu/arm64/ 文件清单盘点;【推断:0xd22 SVEver=1 的完整 SVE2 事件面待测】。

7. **PMU/SPE 伴随监控接入诊断层**
   0xd22 有 SPE(920 没有)。在 campaign 模式里伴随采集 mem_stall_* / l1d/l2 refill / remote_access,复现命中时导出簇级 PMU 快照 + SPE 采样,把"~19min 节律"与微架构事件(重填风暴/snoop 风暴/电压事件代理)对齐。证据:PMU 文档 §6 事件选型表;SPE 可用性 cn23154.md 实据;事件名在 0xd22 上需重标定(推断)。

8. **时长与节律策略(低改动、高必要)**
   针对 19min 节律类瞬态:默认 5s 测试对该故障完全盲。campaign 模式应支持 30–60min 级驻留 + 命中后自动保留现场(fork 模式已天然隔离崩溃,补 si_addr 位签名解析:自动检查 VA[55:48] 是否非零 = 直接命中已知签名)。证据:19min 节律实据 + memory 档案判别判据(si_addr 低 48 位完好性)。

---

## 附:文档未覆盖清单(防编造,后续需实测)

- 0xd22 核内:ROB/IQ/PRF/调度器具体规模、执行端口配置、FMA 延迟(仅吞吐下限)、旁路网络实际组织 —— 全部未公开。
- 0xd22 imp-def PMU 事件空间与 uncore 事件名(SICL L3 的 hisi_sicl3_pa 事件表)—— 920 方法可移植,数据为零。
- 0xd22 分支预测器容量(bpbench 未决项)、SVE512 峰值复测(未决项)。
- node16–31 无 CPU 内存节点性质(CXL?HBM?)未确认。
- 故障的电路级根因(阻性缺陷/老化/电压域)无直接观测,仅有行为学签名。

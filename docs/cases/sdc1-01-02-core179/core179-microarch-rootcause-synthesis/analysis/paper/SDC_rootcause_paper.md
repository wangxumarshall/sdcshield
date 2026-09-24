# 一条指令的八次死亡：鲲鹏 920 单核装载返回通路间歇性静默数据损坏的微架构级根因诊断

**Eight Crashes, One Instruction: A Microarchitecture-Level Root-Cause Diagnosis of Intermittent Silent Data Corruption in the Load-Return Path of a Kunpeng 920 Core**


**摘要**：一台 192 核鲲鹏 920（TaiShan V110）服务器在 2026-08-14 至 09-03 的 20 天内连续发生 8 次内核致命崩溃。本文以 8 份 kdump 转储为唯一证据源，完成了一次从软件现象到微架构根因的完整取证。八次崩溃中的七次命中**同一条指令** `find_busiest_group+0x140`；跨八次开机的 **138 起硬件异常事件（130 次前兆 WARNING + 8 次 Oops）100% 落在同一物理核 CPU179**，其余 191 核累计约 487 小时运行零事件。通过对第 7、8 案 vmcore 的直接内存读取，我们证明被装载的内存数据完好无损（等差数列逐项成立），而寄存器收到的值呈现四种确定性的**字节相位撕裂**形态（零塌缩、ROR8、ROL16、ROR16/相位混合），且反事实推演证明：若寄存器收到真值，系统将平静地读到有效数据并继续运行。全部代数闭合（x27=(x1+x20) mod 2⁶⁴、FAR=x27+0x120）逐位成立。结合 gem5 故障注入（byte_lane_skew/phase_offset 模型 100% SDC 传播）、-30mV 欠压现场复现、单条 no-op 指令使触发率塌方一个量级等受控实验，根因收敛于：**CPU179 的 LSU 装载数据返回通路（fill-buffer/转发合并级到寄存器写回的选路）在特定发射相位×电压裕量边际下的间歇性时序失效**，一种被 ECC 粒度和 RAS 覆盖面双双遗漏的结构化瞬态故障。RAS/EDAC 的全静默不是"无硬件故障"的证据，而是"故障位于检测盲区"的必然推论。最后，我们从芯片设计与系统软件两个维度给出规避、消减、暴露 SDC 的分层方案，每一层均有量化实验支撑。

**关键词**：静默数据损坏（SDC）；vmcore 取证；微架构根因分析；装载返回通路；字节相位撕裂；鲲鹏 920；ARMv8；故障注入

**EN Abstract**: A 192-core Kunpeng 920 (TaiShan V110) server suffered eight successive kernel panics between August and September 2026. Using the eight kdump transcripts as the sole evidence source, we perform a complete post-mortem from software-visible symptoms to a microarchitectural root cause. Seven of the eight fatal crashes hit the very same instruction, `find_busiest_group+0x140`; across eight boots, all 138 hardware anomaly events (130 precursor warnings + 8 oopses) land on a single physical core, CPU179, while the other 191 cores accumulate ~487 incident-free hours. By reading ground-truth memory directly from the two freshest vmcores, we show that the loaded memory is intact (the victim array remains a perfect arithmetic sequence) while the receiving register exhibits one of four deterministic byte-phase-tear corruption patterns (all-zero collapse, ROR8, ROL16, and a phase-shift-plus-source-contamination hybrid); counterfactual replay proves that delivery of the true value would have read valid data and continued execution. All algebraic closure identities (x27 = (x1+x20) mod 2⁶⁴; FAR = x27+0x120) hold bit-exact. Corroborated by gem5 fault injection (byte_lane_skew/phase_offset models propagate at 100% SDC), an on-site −30 mV undervolt reproduction, and a single no-op instruction collapsing the trigger rate by an order of magnitude, the root cause converges to an intermittent timing failure in CPU179's load-return datapath (fill-buffer/forward-merge to register writeback steering) under a specific dispatch-phase × voltage-margin boundary: a structural transient fault that falls beneath ECC granularity and outside every RAS coverage point. The total silence of RAS/EDAC is shown to be the necessary consequence of the fault residing in a detection blind spot, not evidence of its absence. We close with quantified avoidance/mitigation/exposure countermeasures for chip design and system software.

**EN Keywords**: Silent Data Corruption; vmcore forensics; microarchitectural root-cause analysis; load-return path; byte-phase tear; Kunpeng 920; ARMv8; fault injection


## 1 引言：一台服务器的八次死亡

### 1.1 SDC：数字系统中最危险的沉默

静默数据损坏（Silent Data Corruption, SDC）指硬件产生的计算错误既不触发异常、也不被 ECC/RAS 体系拦截，而是以静默方式污染计算结果。与导致系统崩溃的 DUE（Detected Unrecoverable Error）不同，SDC 不可见、不可审计，可能造成数据库损坏、金融计算错误与科学结果偏移。近年 Google[1]、Meta[2]、Microsoft[3] 的舰队级研究确立了 SDC 在大规模生产 CPU 群体中的普遍性与芯片内根因（"mercurial cores"）范式，但已有工作多以"检测与容错"为中心，从**单机全量取证到微架构根因**的完整案例剖析仍然罕见：根因的确认通常需要硅片级故障分析（FA），而这在生产环境中极少被执行。

本案提供了一个罕见的机会：一台故障机在 21 天内产生了 8 份完整/部分 kdump 转储，且故障呈现出教科书级的规律性。我们得以像侦探复盘案件一样，逐层剥离表象，最终把根因逼到微架构级的一个功能单元，并用故障注入实验完成"猜想-验证"闭环。

### 1.2 案情概要与本文贡献

故障机为 Yangtze Computing R240K V2（鲲鹏 920，4×48 核，TaiShan V110 微架构，openEuler 24.03 LTS-SP3，内核 6.6.0-145.3.23.154.oe2403sp3）。自 2026-08-14 起（20 天窗口内），每次开机最终都以内核 panic 收场（存活 7 分钟至 149 小时不等），kdump 全部成功落盘。

本文的贡献：

1. **现象学完整记录**（§2）：8 次崩溃、138 起事件的法医级普查，包括事件时间线、ESR/FSC 分类、进程分布与地址区域分类，所有数字可由附录命令独立复现。
2. **系统性排除链**（§3）：从内核 bug、内存颗粒、L3/互连、页表硬件到固件电压，逐一排除软件层与共享资源层的一切替代假设。
3. **两组决定性实验**（§4）：对最新两案直接从 vmcore 提取被装载地址的**内存真值**，证明"内存完好、寄存器收坏"，并用反事实推演证明真值交付下系统不会崩溃；这是单凭软件取证所能达到的最强因果证据。
4. **腐化形态学与 ARM 逻辑不变式**（§5）：四种撕裂子族的字节级解剖；位翻转等价性的穷举证伪；跨开机寄存器不变式的建立与破缺记录。
5. **微架构与物理机制定位**（§7–8）：结合 gem5 O3 故障注入（CHAOS 框架的 byte_lane_skew/phase_offset/错源注入器）、现场欠压复现与指令调度相位实验，将根因收敛到装载返回通路的选路级，并给出间歇性的双轴解释。
6. **静默性解构**（§9）：证明 RAS/ECC/通用模糊测试三类检测手段在本故障上的结构性失明及其必然性。
7. **分层防护方案**（§10）：规避/消减/暴露三层建议，每条附量化实验支撑，包括冗余重算的完全抑制（Fisher p=1.19×10⁻⁷¹）与定向检测用例对结构故障 7.79 倍的检出率优势。

本文的方法论立场：**事实（vmcore 可复核）→ 解释（最简自洽模型）→ 判定（工程处置）**三层分离；所有 64 位地址运算由脚本执行；对推断性结论显式标注置信度；对不可达的物理层证据明示边界（§11）。


## 2 案发现场：八次崩溃的现象学

### 2.1 八案总览

表 1 汇总八次开机的核心事实。所有数据来自对每份 `vmcore-dmesg.txt` 的独立 grep/awk/python 统计（法医链：每案 md5 记录于普查报告 §0）。

**表 1　八案汇总**

| # | dump | 存活 | WARNING | 崩溃点 | 宿主进程 | x20 形态 | FSC |
|---|---|---|---|---|---|---|---|
| 1 | 08-14 19:07 | 31.67h | 12 | fbG+0x140 | kworker/179:1H | ROL16 撕裂 | L0 |
| 2 | 08-17 13:47 | 66.53h | 26 | fbG+0x140 | swapper/179 | ROR8 撕裂 | L0 |
| 3 | 08-24 18:03 | 149.29h | 34 | bio_add_page+0xf0 | kworker/u391:3 | 变址装载乱码 | L0 |
| 4 | 08-25 15:42 | 21.34h | 1 | fbG+0x140 | claude（用户会话） | 零塌缩 | L3 |
| 5 | 08-25 15:58 | 6.97min | 0 | fbG+0x140 | kworker/179:1H | ROR8 撕裂 | L0 |
| 6 | 08-26 10:37 | 18.52h | 9 | fbG+0x140 | mi-scavenger | 零塌缩 | L3 |
| 7 | 08-31 00:47 | 110.03h | 13 | fbG+0x140 | rcu_sched | ROR16+污染（新形态） | L0 |
| 8 | 09-03 18:25 | 89.51h | 35 | fbG+0x140 | rcu_sched | ROR8+污染 | L0 |

（fbG = find_busiest_group；x20 = 故障装载的目的寄存器；FSC = 异常的页表故障级别。案 8 转储位于故障机本地，取证经由远程 crash 会话完成。）

三个数字立即浮出水面：

- **7/8 次致命崩溃命中同一条指令** `find_busiest_group+0x140/0xb60`（内核调度器负载均衡路径中读取 per-CPU 运行队列的 `ldr x23,[x27,#288]`），且七个 fbG 案的 `Code:` 窗口五个指令字逐字相同：`f9400782 f879d814 2a1903e0 8b14003b (f9409377)`（案 3 崩溃点不同，指令字亦异，属预期）。
- **138/138 起事件 100% 发生于 CPU179**：130 次 WARNING（内核自判 spurious 的翻译故障）与 8 次 Oops 的 `CPU:` 字段无一例外；其余 191 核在八次开机累计约 487 小时中零事件。
- **触发进程与负载完全无关**：Oops 宿主横跨 idle 任务（swapper）、内核线程（kworker、rcu_sched）、系统守护（irqbalance、pmdalinux）、压测工具（mi-scavenger、memcpy1）乃至交互式 shell（claude）。唯一的公共变量是**被调度到 CPU179 上执行**。

### 2.2 故障窗口的指令语义

反汇编（debuginfo vmlinux + DWARF 行号，`kernel/sched/fair.c:12050/12054/5024`）还原故障窗口：

```asm
; update_sg_lb_stats(): for_each_cpu_and(i, group_span, env->cpus)
ffff…ae34  ldp  x0, x1, [sp, #8]          ; x0 = &__per_cpu_offset[0]
                                          ; x1 = &runqueues（percpu 静态模板）
ffff…ae3c  ldr  x20, [x0, w25, sxtw #3]   ; x20 = __per_cpu_offset[i]   ← 数据腐化注入点
ffff…ae44  add  x27, x1, x20              ; x27 = &per_cpu(runqueues, i)
ffff…ae48  ldr  x23, [x27, #288]          ; rq->cfs.avg.load_avg        ← +0x140 致命点
```

即 C 表达式 `cpu_rq(i)->cfs.avg.load_avg` 的逐级展开。注意数据来源指令 `ldr x20,[x0,w25,sxtw#3]` 是**符号扩展缩放变址装载**（AGU 移位器参与地址生成）；案 3 的崩溃链同样是变址装载（`ldr x3,[x3,x2]`）返回乱码后解引用。八案的致命链根部全部是**一条变址寻址的 LDR 从内存读出后寄存器收到腐化数据**。

### 2.3 前兆事件的形态

130 次 WARNING 全部是 openEuler 内核对"重走成功"的翻译故障记录（`Ignoring spurious kernel translation fault`）：内核对同一地址重新执行 AT S1E1R 翻译或再次访问时成功，判定首次失败为瞬态。其中 127 次（97.7%）ESR=0x96000044（**写访问** + L0 级翻译失败），3 次为读访问；触发点集中在 `__memcpy+0x80`（`seq_printf` 向 `/proc/interrupts` 的 seq_file 缓冲写入，84% 的 FAR 落在 vmalloc/percpu 区 `ffff60xx`），其余来自 `_find_next_and_bit`、`__lruvec_stat_mod_folio` 等装载点。**读、写、页表遍历三类访存全部受扰**；这个细节在 §7 将成为定位故障单元的关键证据。


## 3 排除链：一切软件层嫌疑的系统性排除

福尔摩斯的方法论是排除法："排除一切不可能之后，剩下的无论多么难以置信，都是真相。"我们依此建立排除矩阵。

### 3.1 内核软件缺陷

**排除**。四条独立证据：其一，六案代数闭合（§5.1）证明崩溃时刻寄存器组完全自洽：x27 精确等于 x1+x20（模 2⁶⁴），FAR 精确等于 x27+0x120，代码路径的反汇编-符号-源码四重对齐无歧义；若是软件 bug（UAF/越界/竞态），不可能在 21 天内于同一条指令上以不同腐化形态精确闭合。其二，反事实实验（§4.3）证明正确数据下程序必然正常运行。其三，故障 100% 单核私有（§2.1），而内核代码在全机 192 核上对称执行。其四，八案横跨四次不同的开机 KASLR 基址与两种压测负载背景，软件栈完全一致却在时间分布上无规律（§6），不符合软件缺陷的可复现特征。

### 3.2 内存颗粒 / DDR 故障

**排除**。EDAC（ghes_edac，32 DIMM）八案全程零 CE/UE 记录；更重要的是 §4 的决定性实验直接证明被读数组（`__per_cpu_offset[]`）为完美等差数列；若存储路径损坏，192 项数组不可能保持等差。且故障随核不随地址：八案的装载目标地址各不相同（KASLR + 不同表项），损坏却只发生在 CPU179 执行时。

### 3.3 L3 / 互连 / SoC 共享资源

**排除**。L3 与互连在节点内多核共享，若故障在共享层，同节点同胞核（与 CPU179 同 L3 的其余 23 核）必然出现事件，实际为零。自研 rasnode.ko 在案 6 扫描了 192 核 × 5 个架构化 ERR 节点（ERRIDR=0x4），CPU179 的 FR/CTLR/STATUS/ADDR/MISC 读数与其余 191 核逐位一致；HiSilicon 私有 RAS 驱动的 45 个子模块（全为 SoC 互连）全程零记录。

### 3.4 页表 / MMU 硬件走表损坏

**排除**。spurious 事件的定义即"重走成功"，页表内容完好。零塌缩子族（案 4/6）的 pte=0 有设计性解释：`&runqueues` 静态模板位于 `.data..percpu` init 区间，开机后 `free_initmem()` 对其 `vunmap_range`，该页永久解映射；当 x20 被读成 0，x27 塌缩到模板地址，MMU 如实走完四级页表得到 pte=0。走表本身诚实，坏的是输入地址。

### 3.5 固件电压残留与已知 erratum

**排除/存疑标注**。VDDAVS 电压在早期实测为 0.94–0.97V 健康；但 -30mV 欠压实验（§8.2）证明电压裕量是触发轴之一：电压不是"残留故障源"而是"裕量边界条件"，二者区别在 §8 讨论。公开 HIP08 erratum（如 RU-prefetch 类）签名不符。温度：SEL 曾见 Upper Non-critical 记录，作为加速因子存疑保留，非根因。

排除矩阵收束后的结论：**故障位于 CPU179 核私有、且不在任何 RAS 覆盖内的单元**。ARMv8 核内私有且同时服务"装载数据返回、store 翻译、页表遍历读"的单元只有一个候选区域：LSU 的 D-side 通路。这就是 §7 的起点。


## 4 决定性实验：内存真值 vs 寄存器实收

这是全文因果链的枢纽。2026-09-03，我们在故障机（此时第 9 次开机已在运行）上以 crash 8.0.4 + 精确版本 debuginfo 直接读取第 7、8 案 vmcore，完成"应然值 vs 实收值"的对照闭环。

### 4.1 第 7 案（08-31，rcu_sched，i=60）

崩溃时寄存器：x20（实收）= `a000ffffbe56fb25`，x27 = `a000c1a9443c91e5`（= x1 + x20，逐位闭合），FAR = `0000c1a9443c9305`（= x27+0x120 低 48 位）。

crash 从 vmcore 读出：

```
__per_cpu_offset[60] = 0xffffbe56fa9b6000      ← 内存真值（非零！）
rd -64 __per_cpu_offset 192                     ← 全数组完美等差数列
   （base=ffffbe56fa1be000, step=0x22000, 192 项无一损坏）
```

字节级对照：

```
真值 entry[60] : ff ff be 56 fa 9b 60 00
实收 x20      : a0 00 ff ff be 56 fb 25
                 └─┬─┘ └──────┬──────┘└──┬──┘
                  污染   true[0:4] 右移 2 字节  异源字节
```

x20 的第 2–5 字节恰是真值的前 4 字节右移 2 字节（16 位相位错位），首 2 字节 `a000` 与尾 2 字节 `fb25` 均不出现在真值中；**这不是纯相位旋转，而是"相位撕裂 + 源污染"的混合形态**。

**反事实推演**：若 x20 收到真值，则 x27_true = (&runqueues + offset[60]) mod 2⁶⁴ = `ffff80008080f6c0`。三重独立验证：(i) `vtop` 判定 **VALID**（PTE `e86057ffe04f03`，VALID|SHARED|AF|DIRTY）；(ii) 反事实致命地址 x27_true+0x120 = `ffff80008080f7e0` 处读出 `0x400`（rq(60).cfs.avg.load_avg = 1024，健全数据）；(iii) crash 内建 per-CPU 解析器返回的 rq(60) 实例 `nr_running=1`、负载计数正常。**即：异常的唯一必要条件是装载结果被腐化。**

### 4.2 第 8 案（09-03，rcu_sched，i=12）

同法执行。真值 `__per_cpu_offset[12] = 0xffffb617dc4d6000`（数组同样完美等差，base=ffffb617dc33e000）；实收 x20 = `00ffffb617dd3940`：

```
真值 entry[12] : ff ff b6 17 dc 4d 60 00
实收 x20       : 00 ff ff b6 17 dd 39 40
                  └───┬───┘└──┬──┘└───┬───┘
                高位污染/相位移 真值前缀  异源字节
```

x20 的第 1–4 字节 `ff ff b6 17` 是真值前 4 字节右移 1 字节的相位形态；尾 3 字节 `dd 39 40` 不匹配真值尾部。反事实 x27_true = `ffff8000801af6c0`，vtop 判定 VALID（PTE `e80037ffe2ef03`），x27_true+0x120 处读出 `0x3ff`（rq(12).load_avg = 1023）。反事实闭环同样成立。

### 4.3 实验的证明力结构

两组实验共同建立了三个命题：

1. **内存完好**（等差数列逐项成立）→ 排除存储阵列与总线的数据损坏，损坏发生在"缓存层级内某处到寄存器"的最后一程；
2. **寄存器收坏**（实收 ≠ 真值，且形态为相位撕裂而非随机翻转）→ 损坏点在装载返回通路，而非指令语义或地址生成；
3. **真值交付则无异常**（反事实地址 VALID 且数据健全）→ 排除"即使收到真值也会因其他原因崩溃"的兜底假设，因果链闭合。

对照既往验证（第 1 案 08-14 于既往报告 §3.4–3.5 完成：offset[179] 真值非零、rq176 实例数据健全；第 6 案 08-26 同法验证 offset[179]），**八个 fbG 案中四个已完成内存真值级验证，全部支持同一结论**。


## 5 形态学：撕裂子族与 ARM 逻辑不变式

### 5.1 代数闭合：每一次崩溃都是可推导的

对七个 fbG 案逐位验证（脚本计算，模 2⁶⁴）：

- **闭合式 1**：x27 = (x1 + x20) mod 2⁶⁴，**7/7 案逐位成立**。x1 为 percpu 静态模板地址（各案 KASLR 不同），x20 为实收（可能腐化）的偏移值，x27 为下游访存基址。
- **闭合式 2**：FAR = x27 + 0x120（低 48 位），**7/7 成立**；高 16 位在案 1/7 出现 HW 上报与寄存器不一致（d936→0036、a000→0000），两种解释并存（发作窗口内多次受扰 vs MMU 对非规范地址的 FAR 截位），如实标注。
- 案 3（bio_add_page）：FAR 低 48 位 == x3 低 48 位（x3 为变址装载返回的乱码指针），同构闭合。

**ARMv8 逻辑不变式**（跨八开机成立，构成"确定性代码路径指纹"）：

| 不变式 | 成立情况 |
|---|---|
| `Code:` 五指令字全同 | 7/7 fbG 案（案 3 异位点，属预期） |
| x23 ≡ 0x400（上一次成功迭代的 load_avg 残留） | 7/7 |
| x24 − x21 ≡ 0x5350（percpu 基址与 nr_cpu_ids 相对距离） | 7/7 |
| x21 低 12 位 ≡ cb0（nr_cpu_ids 锚） | 7/7 |
| x1 低 16 位 ≡ 96c0（runqueues 模板锚） | 7/7 |
| x9 低 12 位 ≡ e58（KASLR 锚 find_busiest_group+0x150） | 7/7 |

这些不变式证明八次崩溃发生在**完全相同的执行上下文**中，排除了"不同代码路径偶发踩雷"的解释；而 x22==x26 在案 7 破例（差 0x60）；既往报告曾将其列为不变式，本次普查将其**降级为高频巧合**（从调度器源码看两寄存器缓存不同相位变量，本就不保证相等）。不变式的建立与破缺同样有证据价值：破缺恰说明腐化是**数据通路上的一次性事件**，而非持续性的执行状态污染。

### 5.2 四种撕裂子族的完备形态学

八个 fbG 案 + 一个 bio 案的 x20/指针腐化形态全谱：

**表 2　撕裂子族分类**

| 子族 | 案例 | 形态 | 语义 |
|---|---|---|---|
| 零塌缩 | 案 4、6 | x20 = 0 | 交付了空/无效槽位内容；x27 塌缩到模板地址 → FAR 落内核规范域 → FSC=L3（走到 pte=0） |
| ROR8 | 案 2、5 | 真值右旋 1 字节（x20≪8 恢复为合法偏移形态） | 8 位相位错位 |
| ROL16 | 案 1 | x20 = ROL16(entry[1])（左旋 2 字节） | 16 位相位错位，方向与 ROR8 相反 |
| 相位+污染 | 案 7、8 | 真值前缀的相位错位 + 异源字节拼接（§4） | 相位错位与源污染并存 |
| 变址乱码 | 案 3 | 变址装载返回完全乱码指针 | 同族最重形态 |

FSC 的二值分布与子族严格对应：零塌缩 → L3（地址规范、走到 PTE=0）；一切撕裂 → L0（地址非规范或落用户域，PGD 级即失败）。**两种 FSC 不是两种病，而是坏地址的不同投影。**

### 5.3 位翻转等价性的穷举证伪

一个自然的质疑：这些"撕裂"会不会只是巧合的多次位翻转？既往对案 5 的穷举分析给出了决定性否证：坏值 `00ffffcc879da2e0` 无法由真值 `ffffcc879ed92000` 的任何单字节位翻转组合产生（8 字节 × 256 掩码无命中）；而它恰好等于**另一个真实表项**（offset[0] = `ffffcc879da2e000`）右移 1 字节；在 192 槽 × 8 种旋转共 1536 个候选中，坏值唯一命中数组头部，两例独立出现该模式的概率约 2⁻⁵⁸。**腐化是"从错误源、以错误相位交付"，不是随机比特损伤。**这与 §7 的注入实验（byte_lane_skew 产生同族 XOR 汉明重量 35/36 均匀散布）互为印证。


## 6 统计画像：138 起事件的时空结构

### 6.1 时间间隔的双态分布

136 个可计算的事件间隔（跨八案合并）呈清晰双态：短簇（<10s，同进程在同一 procfs 读循环中连发，如案 6 的三连发间隔 0.000s/0.005s）与超长静默（>10⁴s，最长 343,330s ≈ 95.4h）并存，中位数 270s 与 irqbalance（10–20s 周期）和 pmdalinux（约 60s）的轮询节奏吻合。解读：**触发机会由周期性 procfs 读驱动，但故障本身是与触发节律解耦的随机事件**；这正是间歇性硬件时序故障的特征签名，与软件 bug 的确定性复现截然不同。

### 6.2 前兆窗口与"相变"现象

6/8 案有前兆 WARNING（首症距 panic 2.6h–149h 不等，无单调趋势，不宜拟合 MTBF）；案 5 无任何前兆直接致命：前兆监控有预警价值但非充分防线。值得注意的是 3/8 案（08-14/08-24/08-31）的最后事件距 panic 分别仅 17.6s/7.5s/3.1s：**临近致命时事件加密**，呈现故障活跃度升高的"相变"特征。案 7 的末三起前兆在 panic 前 3.098 秒内连发（间隔 0.004s/0.023s），随后即刻致命。

### 6.3 首症时刻无单调趋势

首症时刻序列（29.0h → 47.0h → 0.23h → 0.47h → 无 → 0.41h → 2.92h → 19.95h）被打断，既往"前移趋势"假说不成立。存活时长（0.12h–149.3h）同样无规律。这些参数的随机性与 §8 的"相位×电压裕量"双轴间歇模型自洽：触发需要两个边界条件同时逼近，单轴无趋势是预期结果。

### 6.4 单核浓度：一个数量级的显著性

138/138 事件集中于 192 核之一。若故障与核无关（均匀分布假设），该浓度的发生概率约为 (1/192)¹³⁸ 的量级，无需形式化检验即可拒绝。更有说服力的是分母：其余 191 核累计约 487 小时（约 9,300 核·小时）零事件，包括与 CPU179 共享 L3 的同胞核、共享电压域的邻居核。故障的边界与**核私有微结构**的边界精确重合。


## 7 微架构下钻：从 ISA 抽象到装载返回通路

### 7.1 三条通路，一个单元

§2.3 记录了三类受扰访存：装载数据返回（致命案）、store 翻译（126 起 WnR=1 写失败）、页表遍历读（spurious 的机制基础）。在 ARMv8 核内，这三条路径在 D-side 汇聚：

```
地址生成(AGU) ──► dTLB 查找 ──► [miss] PTW 页表遍历 ──┐
     │                                               ▼
     │                                    L1D fill / fill buffer
     ▼                                           │
  store buffer / LSQ ──► store-to-load 转发 ◄────┘
     │                    │
     ▼                    ▼
  L1D 阵列 ──► 读出/组装/对齐网络 ──► 装载数据返回总线 ──► 物理寄存器写回
```

TaiShan V110 的公开微架构参数（L1D 64KB 4-way ECC、2×AGU、2×128bit/周期数据通道、store 转发 6–7 周期）给出几何约束：**装载返回、store 访存、PTW 读表在多数实现中共享 fill/读出通路**。三类访存同时受扰 + 单核私有 + 内存真值完好，把病变区域压缩到"fill-buffer/转发合并级 → 读出组装/对齐网络 → 寄存器写回选路"这一段，即**装载返回通路的数据侧**。

### 7.2 排除性对照：为什么不是别的单元

既往 gem5 O3 + CHAOS 框架的系统性注入实验（代理参数对齐 V110：ROB128/PRF int160/LQ48/SQ42/4-wide/2.6GHz）提供了三组对照：

**物理寄存器堆（PRF）**：X3 累加器任意单 bit 翻转的 SDC 率为 100%（768/768，八位段全扫）；但 PRF 损坏是**持续性**的：注入值在被覆盖前有 125,000+ 次读的传播窗口。现场故障是**一次性瞬态交付**（spurious 重走即成功、下次读同地址正常），形态不符。且 PTW 类事件完全不经过寄存器重命名，PRF 活性无法解释翻译故障。

**重命名表（RAT）**：F5 合法域替换注入产生"读回 donor 变量之值"的签名（P=0.770）；但现场坏值是**真值本身的相位错位副本**（§5.3），不是"另一变量之值"；重命名混淆无法产生字节移位结构。

**L1D 阵列单 bit**：SECDED 注入实验（n=384/cell）证明单比特翻转被 ECC 100% 纠正（Corrected 384/384）；现场零 CE 记录 + 多比特结构撕裂，双双排除单比特阵列故障。若 V110 L1D 实际无 ECC，D1 亦可解释为阵列读出失效；两种情形的处置相同，标注为供应商待澄清项。

**组相联几何裁决**：案 5 坏值源（offset[0]，set 87）与装载目标（offset[146]，set 105）**不同 set**，排除 L1D way/列选通错（那应送达同组现役行），强化跨 set 的 fill-buffer/合并级错源模型。

### 7.3 正向证据：注入复现撕裂签名

CHAOS 框架的 CHAOSLSQFwd 注入器在 gem5 O3 的 store→load 转发路径上实现了五种结构化故障模式，其中三种与现场形态一一对应：

**表 3　装载返回通路故障模式的 SDC 传播率（fp_fwd_kernel，n=64/cell，单故障）**

| 注入模式 | 对应现场形态 | SDC 率 |
|---|---|---|
| bit_flip（单比特） | —（现场无此形态） | 64/64 = 100% |
| **byte_lane_skew rol1**（字节通道旋转） | **ROR8/ROL16/ROR16 撕裂** | **64/64 = 100%** |
| **phase_offset=2**（相位偏移交付） | **相位撕裂** | **64/64 = 100%** |
| fwd_source_sub（错源转发） | 源污染分量 | 0/64（同址 kernel 等值掩蔽，诚实阴性） |
| stale_line_replay（陈旧行回放） | 源污染分量 | 0/64（同上） |

byte_lane_skew 单注入产生 `xor=3fc52e90a6628000` 的多位散布 XOR，与现场撕裂的汉明重量 35/36 均匀散布同族；错源/陈旧模式的零检出是单几何 kernel 的限制（同址转发替换源后仍等值），不构成对错源机制的无罪证明。侧分支闭环（ptrskew_kernel，`__per_cpu_offset` 装载-作指针-解引用，与现场崩溃链同构）：byte_lane_skew rot1 注入 30 次、检出 28 次（93%）。

关键结论：**装载返回通路是"无掩蔽缓冲"的确定性传播点**：只要结构故障落到被消费的交付数据上，SDC 以条件概率 1.0 传播。这解释了为何现场的每一次致命撕裂都精确地演化为崩溃：这条通路上没有第二次机会。

### 7.4 根因判定（微架构级）

综合 §4（内存完好/寄存器收坏/反事实无异常）、§5（相位撕裂形态学 + 位翻转证伪）、§7.1–7.3（三通路汇聚 + 排除对照 + 注入复现），微架构根因判定为：

> **CPU179 的 LSU 装载数据返回通路（fill-buffer/转发合并级至寄存器写回的字节选路与组装段）存在间歇性结构化交付故障：在特定条件下，交付数据呈现字节相位错位（k×8 bit，k∈{0,1,2}）与异源字节拼接，或整字塌缩为零。** 证据强度：强（多源收敛、注入复现、排除完备）；物理层具体失效位置（哪一级锁存/多路选择器）超出 vmcore 方法论可观测极限（§11）。


## 8 物理机制：相位 × 电压裕量的双轴间歇模型

### 8.1 发射相位轴：一条 no-op 的塌方实验

现场受控实验（method3，CPU179 定向探针）确立了触发三必要条件：store 存在、store 地址推进、store 跨 cache line 推进且与 reload 同 LLC 域；移除任一条件触发率归零。更深刻的是**相位塌方**：在热路径插入一条语义 no-op ALU 指令（`and x2,x19,x20`，store 与 reload 仍 back-to-back），触发率从 ~100%（5/5 seeds）塌方至 ~10%（1/10）；插入 `eor` 打破 back-to-back 相邻性则降至 ~20%（1/5）。**一个发射槽的相位移动即改变触发概率一个数量级**；判别式是指令调度时序相位，而非相邻性本身。gem5 侧 phase_offset 注入（|offset|≥1 → 100% SDC）与该现场观察方向一致（塌方比 ≥5×，绝对值为代理 kernel 限制，标注 E3 证据级）。

### 8.2 电压裕量轴：-30mV 欠压复现

更早的现场实验（SDC1-01-02 案例，EulerOS 5.10 内核）完成了惊人的复现：将四路 CPU 的 VDDAVS 电压拉偏 **−30mV（0.88V→0.85V，经 BMC I2C 写 VRD）**，随后运行 STL 压测：**同一台机、同一 CPU179、同一 `__per_cpu_offset` 装载族**，`find_busiest_group` 路径连续崩溃 ≥3 次（uptime 3014s/480s/722s），寄存器呈现同族撕裂形态（x10=`0x0ffe809021e0b2ae`、x9=`0xa24000ffff5cd22b`，高位非 0 形态与本案 ROR16/污染族同谱）。欠压告警显示 CPU2 VDDAVS 实测下探 0.810V。这是"电压裕量压缩使同通路时序失效显性化"的现场实证。

### 8.3 双轴模型的统一表述

两条轴共同解释全部现象学：

- **间歇性**：触发需要"特定发射相位窗口 ∩ 边际时序裕量"同时成立。相位窗口由 workload 的指令调度决定（故 procfs 读的周期性驱动事件节律），裕量由电压/温度/老化决定（故长静默与相变加密并存）。任一轴不满足即静默；间隔分布的双态结构（§6.1）是模型的直接预测。
- **撕裂形态**：相位错位交付是"数据在流水线中的捕获时刻相对选路时刻错位 k 个字节通道"的架构级投影；零塌缩对应无效/空槽位态；源污染对应多路选择器在竞态窗口选错源。
- **为何 store 共存是必要条件**（活体三臂实验：纯加载探针 10¹² 次零撕裂）：store 的 fill/writeback 占用共享数据通路资源，压缩了装载交付的时序裕量，这与 V110 转发路径 6–7 周期的深流水几何一致。
- **物理本质候选**：小延迟故障（small-delay fault）类的建立/保持违例，最可能位于 sense-amp/位线均衡或选路锁存的边际时序上。既往位分布分析（汉明重量 35/36、均匀散布、无列/字节聚类）已排除 stuck-at、位线短路、译码器错误等结构化数字故障。

### 8.4 温度与老化（次要因素，标注存疑）

SEL 曾见 Upper Non-critical 温度记录；老化（HCI/NBTI）会侵蚀时序裕量，是"该核为何变得边际"的合理背景，但现有证据无法分离温度、老化与制造偏差的贡献，标注为存疑，不作根因主张。


## 9 静默性解构：为什么一切检测手段都失明

本案一个刺眼的特征是：138 起事件期间，**整机的硬件检测体系一声不吭**。这不是运气差，而是结构性的必然，分四层解构：

### 9.1 ECC 粒度失配

L1D 的 SECDED（若在位）只能纠正单比特、检测双比特。撕裂交付等效**跨越多个字节的结构化多比特错误**，且若发生在 ECC 校验点**下游**（fill-buffer 合并/读出组装段），则完全不在保护范围内。gem5 对照实验量化了这一点：SECDED 把单比特 100% 纠正（384/384 Corrected），对双比特转为检出遏制；PTW 读出通路 ECC-on 同样把单比特 40 注入全数纠正，而**现场零 CE 记录**。零 CE + 多比特撕裂形态，共同把故障点压到 ECC 覆盖之外。

### 9.2 RAS 架构盲区

故障的 ESR 为 EC=0x25（普通 Data Abort），而非 EC=0x2f（SError/RAS）；**硬件从未将其识别为可上报错误**。ARM 的 ERR节点（ERRIDR/ERX）覆盖的是缓存阵列、TLB、互连等可注入 CE/UE 的结构；fill-buffer 级的组合逻辑选路不在任何 ERR 节点内。rasnode.ko 的扫描（192 核 × 5 节点逐位一致）证明的不是"CPU179 无硬件故障"，而是"该故障不在架构化 RAS 的观测面内"。HiSilicon 私有 RAS 的 45 个子模块全为 SoC 互连，同样不含核内 LSU。**"检测不到"恰是"故障位于检测盲区"的必然结果。**

### 9.3 spurious 吸收：内核把发作变成了统计噪声

openEuler 内核对重走成功的翻译故障仅以 ratelimited WARN 记录（`Ignoring spurious kernel translation fault`）：这些本应是**最高价值的前兆信号**的事件，在生产环境里通常被当作无害噪声过滤掉。130 起前兆中 96.9% 是写访问失败，若内核采取严格策略（翻译失败即 BUG），系统会在首症时刻崩溃而非带病运行数十小时；这是"暴露 vs 可用性"的真实权衡（§10.3）。

### 9.4 通用模糊测试的相位盲区

silifuzz 编排器在案 7 期间实际运行（5 次活动记录 @18.1kh–81.0kh）**恰好与故障静默期（13.4kh–282.1kh）重叠：压测在跑，故障零检出**。机制：通用 end-state 比对的快照式测试无法构造"store 推进 × 跨行 × 特定发射相位"的触发窗口，且多数瞬态交付在形成端态分歧前已被后续覆盖。三个板 ~446 核的满载分布式扫描同样真 SDC=0。随机快照式检测对相位依赖型间歇故障天然低效，这是 §10.3 中"定向生成"必要性的直接动机。


## 10 启示：规避、消减、暴露

本节的每条建议都锚定到本文或先前实验的量化结果。

### 10.1 规避（Avoid：不让可疑核承担关键执行）

1. **立即 offline + RMA**：138/138 事件单核、191 核 487h 零事件的浓度证据（§6.4）+ 每次开机必致 panic（8/8）+ 当前第 9 开机已再现 2 起前兆而 CPU179 仍在线，工程处置的紧迫性是本案的现时部分。
2. **不要部署 l1d_disable 类缓解**（SCTLR_EL1.C 清零）：四组独立反例（案 4 卸载 3.7h 后 panic、案 7 卸载 86.7h 后 panic、案 5/6 从未加载照样致命）。微架构含义：仅旁路 L1D 阵列访问不足以绕过 fill-buffer/合并级故障；这条反证本身也加固了根因定位。
3. **调度层隔离**：触发与负载无关、与"在哪执行"有关（§2.1）；关键业务（数据库、金融计算）可通过亲和性避开可疑核，即使尚无 panic 证据。
4. **电压裕量管理**：-30mV 复现（§8.2）说明 AVS/Vmin 校准裕量不足的核是高危对象；舰队级 Vmin 审计应纳入 SDC 前兆监控的输入。

### 10.2 消减（Mitigate：故障发生但阻断 SDC 传播）

1. **冗余重算交叉校验**：gem5 formal 两臂实验：同一累加结果独立重算比对（compute-both）把"读回历史残留"型 SDC 从 P=0.770 完全抑制到 P=0.000（Fisher exact p=1.189×10⁻⁷¹，n=384/臂）；现场观测 4× 抑制（1.0%→0.27%）。对装载返回通路的结构撕裂，双份独立装载 + 比对在概率上等价于要求同一竞态窗口连续命中两次，指数级消减。
2. **ECC/parity 向 ECC 后数据段延伸**：fill-buffer 合并与读出组装段当前是保护真空（§9.1）；对交付数据加端到端校验（如随行 parity 覆盖到寄存器写回前）可把本案四种子族全部转为显性错误。
3. **转发决策与数据组装分级**：相位塌方实验（§8.1）证明一个发射槽的调度差即可改变触发概率量级；微架构设计上将转发数据选择与字节组装安排到不同流水级（错开时序敏感窗），等价于把竞态窗口错开。
4. **编号/指针字段防御性校验**：对 RAT 索引、freelist 位、转发源 seqNum、TLB pfn 等控制字段加 range-check/parity；F5 六载体"合法域校验"的仿真实践证明该类校验在硬件中同样必要且廉价。

### 10.3 暴露（Expose：让潜伏 SDC 显性化）

1. **前兆监控**：`grep "Ignoring spurious kernel translation fault"` 是当前最高效的零成本前兆信号（有效率 6/8；案 5 无前兆是诚实边界）。单核浓度 + 写访问 ESR（0x96000044）是自动去噪的关键过滤器：随机软件噪声不会 100% 单核。
2. **用户态装载通路陷阱探针**（sdc_long 模式）：绑核 + 单 cache line 内布置 percpu-offset 形态真值 + 纯 load 校验循环 + HIT 捕获（obs/xor/迭代号/时戳）。零权限、可长期驻留，一次命中即可定位撕裂形态与触发上下文；与内核侧监控互补。探针设计要点从本案反推：魔数应取**已知真值**（使任何撕裂形态都 ≠ 真值而可检测），装载必须被架构性消费（sink 异或累积防优化消除）。
3. **定向检测用例生成 ≫ 随机**：对 byte_lane_skew 结构故障，定向操作数/序列进化引擎（D13）检出率 65.4%，SiliFuzz 式随机基线 8.4%，**7.79 倍（z=18.68, p<0.001）**；bit-flip 为 3.07 倍。机制：volatile 混合构造 forwarding + 寄存器双路径（D8 单此一招即 3.17×）。通用快照式模糊测试的相位盲区（§9.4）由此补齐。
4. **功耗跳变应力放大**：Type-II 高低交替功耗模式下 SDC 检出 0%→6.7%→13.3% 单调上升（方向性结果，统计功效不足需 ≥100 样本/组）；di/dt 电压波动压缩瞬时时序裕量，与 -30mV 现场复现同向。生产筛选可用交替功耗 stress pattern 放大边际核。
5. **位谱指纹库反查**：现场 xor 谱（尾数集中/符号免疫/汉明重量中位数）对候选故障单元的 Top-K 反查（lsq_fwd 指纹 mantissa_share=0.71 与现场 85–93% 方向一致），把"一次 SDC 的 xor"变成"故障单元的化验单"。

### 10.4 对检测方法论的元启示

本案完整走过了"误诊为软件问题 → 内核取证 → 单核定位 → 真值实验 → 微架构收敛 → 注入闭环"的链条。三个可迁移的方法论产物：(i) **单核浓度是 SDC 的第一信号**：任何"异常 100% 集中于一个核"的分布都应触发硬件怀疑，无论异常内容多么像软件 bug；(ii) **代数闭合是软件取证的试金石**：本案八次崩溃的寄存器组全部可从上游精确推导，这既是排除软件 bug 的证据，也是定位"哪个寄存器在说谎"的线索；(iii) **反事实推演是因果证明的最强软件侧手段**："若收到真值则不会崩溃"把相关提升为因果。


## 11 证据边界与未决问题

诚实性要求我们明示本文结论的边界：

1. **物理层终点**：具体失效位置（哪一级锁存、哪个多路选择器、是否 sense-amp/位线均衡）超出 vmcore 方法论的可观测极限，需芯片级 ATE/DFT（LBIST/MBIST 向量、shmoo 曲线）或硅片 FA 确认。本文的"微架构级"指软件侧可获得的全部证据已被穷尽并相互闭合，而非宣称到达硅片物理终点。
2. **单/多缺陷裁决未解**：D1（装载返回撕裂）、D2（地址通路 byte7 清零，gem5 FS 已复现同签名）、D3（PTW 读出）三种投影是"单缺陷三投影"还是"多缺陷共址"，软件层不可判定，需 scan-at-speed 分别对各级施注入。三案 FAR 高位与寄存器高位不一致的现象（两种解释并存，§5.1）也归入此项。
3. **代理边界**：所有 gem5 P_SDC 为 O3 代理条件概率，非产品 FIT；E8 功耗应力的 Type-I/II 在 gem5 中仅为指令构成差异（无电压模型）；sim→HW 组粒度统计关联未确立（ρ=−0.22, p=0.75）。
4. **单机未复检**：全部实验未在第二台健康机复现（编译过程曾在故障机上进行，虽已 taskset 隔离 cpu179，残余风险如实标注）。
5. **既有数据缺口**：案 2（08-17）vmcore 不完整（kdump 未完成），其内存真值维度缺失，x20 形态归类基于数值形态与案 5 同构，置信度中高；案 3 的 sdc_long 探针 HIT 输出文件未随转储保存，无法确认探针是否在案 3 窗口内命中过撕裂。
6. **V110 保护表未知**：L1D ECC 与否影响 D1 的精确下钻（阵列读出失效 vs 合并级失效两种情形），处置建议相同，但需供应商澄清。
7. **统计功效**：八案不足以拟合 MTBF 或故障率的时间演化；首症/存活参数仅作描述性统计。


## 12 结论

一台 192 核鲲鹏 920 服务器在 20 天内八次死亡，七次倒在同一条指令上。本文以 8 份 vmcore 为证据源完成了从现象到微架构根因的完整取证：138 起事件 100% 收敛于 CPU179；两组内存真值实验证明"内存完好、寄存器收坏、真值交付则无异常"的因果三角；八种子族形态学与位翻转等价性穷举证伪把腐化定性为**字节相位撕裂 + 源污染**的结构化交付故障；gem5 注入复现、no-op 相位塌方与 −30mV 欠压复现把机制收敛为**装载返回通路选路级在发射相位 × 电压裕量双轴下的间歇性时序失效**；RAS/ECC/模糊测试的三重静默被证明是结构性盲区而非幸运。

福尔摩斯说，排除一切不可能之后，剩下的就是真相。本案剩下的那个真相，一个藏在 ECC 粒度之下、RAS 覆盖之外、指令调度相位缝隙里的瞬时竞态，正是 SDC 之于现代计算体系的隐喻：**最危险的故障不是最响亮的，而是恰好落在所有检测手段的分辨率之外**。诊断它的方法，只能是把每一份崩溃转储当作犯罪现场，一层一层地追问：谁在说谎，为什么偏偏是它，以及：如果它是清白的，世界会是什么样。


## 参考文献

[1] Peter Hochschild et al. "Cores that don't count." *HotOS 2021*.
[2] Harish Dattatraya Dixit et al. "Silent Data Corruptions: The Stealthy Saboteurs of Digital Integrity." *SOSP 2023*.（原文标注：Google 生产舰队 SDC 研究）
[3] SOSP 2025 Orthrus: "Efficient and Timely Detection of Silent User Data Corruption in the Cloud with Resource-Adaptive Computation Validation."（Meta 生产舰队 SDC 检测）
[4] Veritas et al. "Demystifying Silent Data Corruptions: Arch-Level Modeling and Fleet Data of Modern x86 CPUs." *MICRO 2024*.
[5] ARM Architecture Reference Manual for A-profile Architecture (ARMv8-A). DDI 0487.
[6] HiSilicon Kunpeng 920 / TaiShan V110 公开微架构资料；Noverse N1 Technical Reference Manual（保护表代理）。
[7] openEuler 内核 6.6.0-145.3.23.154.oe2403sp3 源码与 debuginfo（/usr/src/debug）。
[8] gem5 v25.1.0.1 + CHAOS 故障注入框架（本地研究仓库 gem5-fi-wangxu，分支 fi，含 CHAOSLSQFwd/CHAOSPTW/CHAOSAddrPath 注入器）。
[9] SiliFuzz: Search space exploration for fuzz testing of CPUs.（Google，本地部署 sdcfuzz 扩展）

（注：[1][2][3][4] 为舰队级 SDC 研究的公开对应文献；[5]–[9] 为本案工具与架构依据。本文全部实证数字来自本地取证材料，文献仅作背景与方法论对照。）


## 数据可用性声明

八份 vmcore/vmcore-dmesg.txt 位于故障机及分析机（目录 127.0.0.1-2026-08-14-19:07:04 至 127.0.0.1-2026-09-03-18:25:12）；交叉统计（CROSS_CASE_STATISTICS.md）、微架构证据汇编（MICROARCH_EVIDENCE.md）、决定性实验记录（analysis/DECISIVE_EXPERIMENTS.md）与既往六案诊断报告随本文归档。全部统计命令、crash 会话脚本与代数闭合计算脚本在上述文件中逐条列出，可独立复现。

## 利益冲突声明

无。

## AI 使用声明

取证统计、crash 会话执行与论文撰写在 AI 辅助研究工具（Claude Code 多 agent 会话）中完成；全部证据来自真实 vmcore 与真实单板会话，所有关键数字经脚本独立复算；AI 生成内容中的两处人工算术错误曾被机器对照捕获并纠正（方法学备注见既往报告 §8）。


### 附录 A　八案致命 Oops 完整寄存器记录

（见 CROSS_CASE_STATISTICS.md §9，含 30/30 寄存器逐案原始抄录与闭合计算；案 8 记录于本文 §4.2 与正文表 2。）

### 附录 B　决定性实验命令集

```bash
# 在故障机（172.168.160.42）上执行；crash 8.0.4 + 精确 debuginfo
printf "sym runqueues\npx __per_cpu_offset[60]\nrd -64 __per_cpu_offset 192\np nr_cpu_ids\nvtop 0xffff80008080f6c0\nrd 0xffff80008080f7e0 8\nquit\n" > /tmp/crash_cmd.txt
crash -i /tmp/crash_cmd.txt \
  /usr/lib/debug/usr/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux \
  /home/sdc/vmcore/127.0.0.1-2026-08-31-00:47:32/vmcore

# 事件普查（任一分析机）
grep -oE "WARNING: CPU: [0-9]+" vmcore-dmesg.txt | sort | uniq -c
grep -c "Ignoring spurious" vmcore-dmesg.txt
grep -A45 "Unable to handle kernel paging request" vmcore-dmesg.txt

# 代数闭合（禁止手算）
python3 -c 'print(hex((0xffffc1a985e596c0+0xa000ffffbe56fb25)&(2**64-1)))'
# = a000c1a9443c91e5（案 7 x27，逐位闭合）
```

### 附录 C　术语表

- **SDC**（Silent Data Corruption）：静默数据损坏，指硬件错误未经任何检测机制暴露而污染结果。
- **fbG**：find_busiest_group，内核 CFS 负载均衡核心函数。
- **FAR/ESR/FSC**：Fault Address Register / Exception Syndrome Register / Fault Status Code，ARMv8 数据中止异常的报告字段。
- **spurious translation fault**：重走成功的翻译故障，内核判定首次失败为瞬态并仅告警。
- **ROR8/ROL16/ROR16**：按位循环右移/左移 8/16 位的字节相位撕裂形态。
- **byte_lane_skew / phase_offset**：gem5 注入器对"字节通道旋转/相位偏移交付"的故障模型。

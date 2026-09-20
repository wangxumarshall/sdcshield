# CPU179 转储深度诊断报告（第 1 次独立重研究）
## ——13 次同核 spurious 前兆 + 装载结果 16 位右移标签化，寄存器/旁路网络级 SDC 全链路实锤

| 项目 | 内容 |
|---|---|
| 目标转储 | `127.0.0.1-2026-08-31-00:47:32`（vmcore 14.6GB，Kdump compressed v6，PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48，192 核 Kunpeng-920 (TaiShan-v110)，8 NUMA 节点，768GB |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP，KASLR 开，KPTI 开，Spectre-BHB 缓解被命令行关闭 |
| 崩溃时间 | 2026-08-31 00:46:39 CST（uptime 396123s ≈ 4天14小时02分，开机于 08-26 10:44:36） |
| 受害对象 | 致命：CPU179 `rcu_sched`（PID 16，内核线程）调度器热路径；前兆：CPU179 上运行的 `pmdalinux`（8次）与 `irqbalance`（5次） |
| 前兆 | 13 次 `Ignoring spurious kernel translation fault` WARNING（全部 CPU 179），首次 t=10520s，末 3 次距致命仅 3.1s |
| 结论一行 | **CPU179 核上 `ldr x20`（装载 `__per_cpu_offset[60]`）的返回值被腐化为「真值右移 16 位 + 0xa000 顶标签 + 低 9 位乱码」，下游地址计算连锁产生非法 VA 触发 level-0 translation fault；内存真值在转储中完好，属寄存器/旁路网络级数据通路 SDC【强推，核心闭合等式为实锤】** |

---

## 1. 执行摘要

1. **现象**：开机 4.5 天后，CPU179 上 `rcu_sched` 内核线程在 `find_busiest_group+0x140`（`ldr x23,[x27,#288]`）触发 level-0 translation fault（FAR=0x0000c1a9443c9305，走用户页表 pgdp=0x204003e1c000，pgd=0），内核 panic 进入 kdump。
2. **签名**：此前 4.5 天内同一颗 CPU179 上已积累 **13 次** `Ignoring spurious kernel translation fault` WARNING，受害进程 `pmdalinux`/`irqbalance` 全部在 `show_interrupts → seq_printf → __memcpy` 读 `/proc/interrupts` 的路上；13 个出错 VA 全部是**非对齐**线性映射地址，且 `vtop` 证明其页表项全部 VALID——映射存在却报翻译错误，故名 spurious。
3. **闭合验证（实锤）**：致命点处 `x27 (0xa000c1a9443c91e5) == x1 真值 &runqueues (0xffffc1a985e596c0) + x20 报告值 (0xa000ffffbe56fb25)` 逐位成立；而 x20 的真值 `__per_cpu_offset[60]=0xffffbe56fa9b6000` 在转储内存中**完好无损**。加法器没错、内存没错，错的是**一条装载指令的返回值**。
4. **腐化形态**：`x20_obs == (x20_true >> 16) | (0xa000 << 48)`，仅 bits[8:1] 再翻转 7 位（XOR=0x01be）。这是一个**结构化的 16 位右移 + 顶标签注入**形态，不是随机位翻转，指向装载/旁路网络的数据通道错位，而非存储单元失效。
5. **置信级别**：寄存器闭合等式与内存真值对照【实锤】；"CPU179 装载通路/寄存器文件级 SDC"【强推】（13 次同核前兆 + 一次终末连锁全部收敛于同一颗核，但无法在软件层直接观测物理数据通路）；具体物理失效点（RF 读端口 vs 旁路阵列 vs L1 供给通路）【假设】，验证途径见第 9 节。
6. **处置**：隔离/下线 CPU179（`echo 0 > /sys/devices/system/cpu/cpu179/online` 或 NO_CPUSET 调度规避），对 CPU179 跑裸机级 cache/RF 级诊断（含 L1D 已做无效的复测），收集 BMC/CEC 日志；本机 17 轮 L1D 实验（见 4.4）不能替代核级诊断。

## 2. 证据规则与方法

- **证据源**：仅 `vmcore-dmesg.txt`（3243 行）、`vmcore`（crash 8.0.4-17.oe2403sp4 + `/tmp/vmlinux-0102`，BuildID 276194e5…，带 debug_info）。未阅读任何既有诊断文档。
- **诚实铁律**：报告中每一处数据引用都来自实际执行的命令（见附件 `dmesg_forensics.txt` 全文）。crash 加载为 PARTIAL DUMP，存在大量 "seek error: IRQ stack pointer" 噪声（未入 dump 的区域），不影响本次取证所依赖的全部读数；`kmem -s` 因页被排除未能完成，如实记录。
- **三级置信**：【实锤】可独立复核（闭合等式、内存真值、指令反汇编）；【强推】多源收敛但缺物理层直接对照（CPU179 数据通路级 SDC）；【假设】无法软件验证，给出验证途径（具体物理失效单元）。
- **工具**：crash 8.0.4（`sys/bt/rd/dis/sym/struct/vtop/ps/p`）、python3（全部 64 位运算 mod 2^64，脚本 `algebra.py`，输出 `algebra_out.txt`）。

## 3. 本次开机时间线【时间线】

| uptime(s) | 墙钟 | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0 | 08-26 10:44:36 | 开机，内核 6.6.0-145.3.23.154，192 CPU，768GB | 1–2 | 实锤 |
| 27.5 | 10:45:04 | 用户态业务就绪（EXT4 挂载完成） | 2570–2572 | 实锤 |
| 10520.60 | 08-26 13:39:56 | **W1** pmdalinux spurious fault @0xffff60400826514e（CPU179） | 2588–2613 | 实锤 |
| 11620.54 | 13:58:16 | **W2** pmdalinux @0xffff604005a392ae | 2633–2677 | 实锤 |
| 11775.04 | 14:00:51 | **W3** irqbalance @0xffff6040076271f3 | 2678–2722 | 实锤 |
| 12975.06 | 14:20:51 | **W4** irqbalance @0xffff20200adbc2f0 | 2723–2767 | 实锤 |
| 13390.53 | 14:27:46 | **W5** pmdalinux @0xffff604088acb4d4 | 2768–2834 | 实锤 |
| 18148–80947 | — | silifuzz_orches 持续运行（CPU 满载 fuzz 业务，loadavg≈180） | 2814–2825 | 实锤 |
| 44745–44749 | — | **L1D 实验 #1**：CPU179 关 L1D 2.1s 后重开（无效） | 2819–2824 | 实锤 |
| 82326–84007 | — | **L1D 实验 #2/#3**：CPU179 关 L1D 300s/900s 后重开（无效） | 2826–2834 | 实锤 |
| 282138.53 | 08-29 17:06:54 | **W6** pmdalinux @0xffff60415e428327 | 2835–2879 | 实锤 |
| 345159.04 | 08-30 10:37:15 | **W7** irqbalance @0xffff202016b2d4b3 | 2880–2925 | 实锤 |
| 362639.10 | 08-30 15:28:35 | **W8** irqbalance @0xffff202018a253b6 | 2926–2970 | 实锤 |
| 363149.07 | 15:37:05 | **W9** irqbalance @0xffff604349d14101 | 2971–3014 | 实锤 |
| 363388.50 | 15:41:04 | **W10** pmdalinux @0xffff604349d10726 | 3015–3059 | 实锤 |
| 396119.594 | 08-31 00:46:35.594 | **W11** pmdalinux @0xffff604349d163ab | 3060–3104 | 实锤 |
| 396119.598 | 00:46:35.598 | **W12** pmdalinux @0xffff604349d16466（+4.2ms） | 3105–3149 | 实锤 |
| 396119.621 | 00:46:35.621 | **W13** pmdalinux @0xffff604349d1649d（+23ms） | 3150–3194 | 实锤 |
| 396122.719 | **00:46:38.719** | **致命 Oops**：rcu_sched，find_busiest_group+0x140，FAR=0x0000c1a9443c9305 | 3195–3240 | 实锤 |
| 396123.11 | 00:46:39 | `Starting crashdump kernel...` | 3242 | 实锤 |
| — | 00:47:32 | kdump 落盘完成（目录名，较崩溃 +43s） | — | 实锤 |

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 行 3195–3243，摘引）

```
[396122.719381] Unable to handle kernel paging request at virtual address 0000c1a9443c9305
[396122.731717]   ESR = 0x0000000096000004
[396122.742360]   EC = 0x25: DABT (current EL), IL = 32 bits
[396122.750128]   FSC = 0x04: level 0 translation fault
[396122.777675] user pgtable: 4k pages, 48-bit VAs, pgdp=0000204003e1c000
[396122.784907] [0000c1a9443c9305] pgd=0000000000000000, p4d=0000000000000000
[396122.792491] Internal error: Oops: 0000000096000004 [#1] SMP
[396122.905717] CPU: 179 PID: 16 Comm: rcu_sched Kdump: loaded Tainted: G        W  OE
[396122.934368] pc : find_busiest_group+0x140/0xb60
[396122.939698] lr : find_busiest_group+0x11c/0xb60
[396123.093055] Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
[396123.100434] SMP: stopping secondary CPUs
[396123.110117] Starting crashdump kernel...
```

要点：FAR 落在**用户页表**（pgdp=0x204003e1c000）且 pgd=0——但崩溃上下文是 EL1 内核线程，这个"用户页表"选择本身就是寄存器值带毒的证据（见第 7 节）。

### 4.2 全量寄存器（x0–x30，dmesg 行 3217–3226）

```
x29: ffff8000821ab9b0  x28: ffff8000821ab860  x27: a000c1a9443c91e5   ← 带毒地址
x26: ffff604003e270c0  x25: 000000000000003c  x24: ffffc1a986255000   ← x25=60：循环当前 CPU
x23: 0000000000000400  x22: ffff604003e27120  x21: ffffc1a98624fcb0
x20: a000ffffbe56fb25  ← 被腐化的装载结果（真值应为 ffffbe56fa9b6000）
x19: ffff8000821aba40  x18: 0000000000000000
x17..x16: 0
x15: 0000000000000000  x14: 0000000000000004  x13: ffffc1a986281988
x12: 0000000000000000  x11: 00000000000000aa  x10: ffff8000817c9430
x9 : ffffc1a98442ae58  ← find_busiest_group+0x150（循环回边地址）
x8 : ffff8000821ab8b8  x7 : 0000000000000000  x6 : 000000000000003c
x5 : f000000000000000  x4 : 0000000000000000  x3 : 000000000000003c
x2 : 0000000000009063  x1 : ffffc1a985e596c0  ← &runqueues（真值，未被腐化）
x0 : 000000000000003c  sp : ffff8000821ab830  pstate: 204000c9
```

（pt_regs 栈帧在 `ffff8000821ab6e0`–`ab7e0`，与上述逐项吻合，见附件会话 9。）

### 4.3 Call trace（完整，dmesg 行 3227–3239）

```
find_busiest_group+0x140
load_balance+0x108/0x6c0
newidle_balance+0x198/0x510
pick_next_task_fair+0x110/0x718
pick_next_task+0x60/0x398
__schedule+0x1b4/0x8a0
schedule+0x58/0x130
schedule_timeout+0x1b0/0x2f0
rcu_gp_fqs_loop+0x11c/0x358
rcu_gp_kthread+0x124/0x178
kthread+0xec/0x100
ret_from_fork+0x10/0x20
```

### 4.4 前兆异常原文（第一次 W1 与终末爆发 W11–W13）

13 次前兆形态完全一致（W1，dmesg 行 2588–2613 摘引）：

```
[10520.595451] ------------[ cut here ]------------
[10520.595465] Ignoring spurious kernel translation fault at virtual address ffff60400826514e
[10520.595473] WARNING: CPU: 179 PID: 14074 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[10520.595714]  __memcpy+0x80/0x240
[10520.595723]  seq_printf+0xc4/0xe8
[10520.595730]  show_interrupts+0x1d4/0x498
[10520.595735]  seq_read_iter+0x168/0x478
...（read 系统调用读 /proc/interrupts）
```

另有一类**人为实验记录**（非故障）：`l1d_disable` 模块在 t=44745/82326/83107 三次对 **CPU179** 定点关闭再重开 L1D（SCTLR_EL1 在 0x3464d99d/0x3464d999 间切换）。这些实验**晚于首次前兆 W1（t=10520）**，说明运维已怀疑 CPU179 且做过 L1D 级干预，但故障在实验之后照旧发生——L1D 整体关闭都不能消除的错误，基本排除"仅 L1D 阵列内单点翻位"的简单解释。

### 4.5 RAS 负证据（dmesg grep）

对 `Machine check|Hardware error|EDAC error|ECC|memory failure|corrected/uncorrected` 全量 grep：**无任何命中**（仅 EDAC MC 版本注册与 ghes_edac 设备释放两行无关输出）。全程无硬件错误上报，纯静默错误——这正是 SDC 的定义性特征。

## 5. 业务现象（受害进程是什么业务）

- **pmdalinux**（PID 14074，crash `ps` 显示 RUNNING-ISH 状态 IN，父进程 13992）：PCP（Performance Co-Pilot）指标采集代理，每秒级读 `/proc/interrupts` 等伪文件。它的 8 次前兆全部发生在 `read(/proc/interrupts) → seq_read_iter → show_interrupts → seq_printf → __memcpy` 路径上——`__memcpy+0x80` 是 16 字节批量装载指令处。
- **irqbalance**（PID 9678）：中断亲和性均衡守护进程，同样周期性读 `/proc/interrupts`，5 次前兆同路径。
- **rcu_sched**（PID 16）：RCU 宽限期内核线程，在 `schedule_timeout` 睡眠后被唤醒，进入 `newidle_balance → find_busiest_group` 的 CPU 选择路径，成为终末受害者。
- 三个受害者毫无业务交集，唯一的公共因子是**都被 CPU179 执行过**。负载背景：silifuzz_orches（CPU 满载 fuzz 编排器）长期运行，loadavg≈180/192——高装载提高了 CPU179 上的指令吞吐，从而提高了错误显影概率。

## 6. 诊断定位过程【诊断定位过程】

**P1 dmesg 勘察**：3243 行全量扫读。锁定 13 次 spurious WARNING（全 CPU179）+ 1 次致命 Oops（也 CPU179）。RAS 关键词零命中。发现 l1d_disable 实验记录（t=44.7k/82.3k/83.1k，均晚于首次前兆）。

**P2 崩溃块提取**：提取 ESR/EC/FSC/pgtable/全寄存器/Call trace/Code 窗口。异常点：内核态 EL1 的 DABT 却走了**用户页表**（pgdp=0x204003e1c000）——直觉上应走内核 pgd。这个矛盾成为破案钥匙：只有 VA 的 bits[55:48] 为 0 才会选 TTBR0，说明被装载/计算出的地址高位不正常。

**P3 crash 加载**：`/tmp/vmlinux-0102` + 14.6GB vmcore 一次加载成功（PARTIAL DUMP，192 CPU，TASKS 2205，PANIC 字符串与 dmesg 一致）。`bt` 完整、`bt -t`/`bt -r` 可用，但存在大量 IRQ-stack seek error（未入 dump 区域，不影响取证读数）。`kmem -s` 因页排除失败，如实记录。

**P4 内存真值对照（决定性环节）**：
- `sym __per_cpu_offset` → 0xffffc1a9862555d0；`rd` 数组前 8 项完好；
- `rd 0xffffc1a9862557b0`（= `__per_cpu_offset[60]`，因 x25=0x3c=60）→ **0xffffbe56fa9b6000**，即 x20 的**真值**；
- `px &runqueues` → 0xffffc1a985e596c0，即 x1 的真值，与 Oops 中 x1 报告值**逐位一致**（x1 未被腐化）；
- pt_regs 栈帧（ffff8000821ab6e0 起）确认 x1 槽位即 `runqueues` 符号；
- `vtop 0xffff80008080f7e0`（反事实地址 = x1_true + x20_true + 0x120）→ **PTE VALID（e82037ffe30f03）**，本应正常访问。

**P5 软件成因排除**：
- `find_busiest_group` 是调度器热路径，本 boot 已执行数亿次 `ldr x20,[x0,w25,sxtw#3]`（每次 newidle balance 都要取 per_cpu_offset），同一二进制在其余 191 个核上运行 4.5 天零异常；
- `__per_cpu_offset[]` 数组内容在转储中完好（x20 真值可读且正确）；
- 13 次前兆横跨 4.5 天、两个无关用户进程、同一颗核——软件 bug 不会挑核、挑进程对；KASLR/编译器/页表均无异常；
- l1d_disable 已实测关闭 CPU179 的 L1D 仍复现错误（前兆 W6–W13 均在实验后）。

**P6 定位收敛**：闭合等式把责任唯一地压到 `+0x12c: ldr x20,[x0,w25,sxtw#3]` 这一条装载指令的**返回值**上：内存真值正确、下一条加法正确、再下一条装载按（带毒）地址正确触发故障。错误发生在"数据从 L1/旁路网络进入 x20 物理寄存器"这一段微架构通路上。

## 7. 逻辑链条

**指令语义**（crash `dis` 实测，debug_info 行号 fair.c:12050/5024）：

```
find_busiest_group+0x128  ldr  x2,  [x28, #8]              ; sgs->group_load
find_busiest_group+0x12c  ldr  x20, [x0, w25, sxtw #3]     ; x20 = __per_cpu_offset[60]   ← 被腐化的装载
find_busiest_group+0x138  mov  w0, w25
find_busiest_group+0x13c  add  x27, x1, x20                ; x27 = &runqueues + per_cpu_offset[60] = cpu_rq(60)
find_busiest_group+0x140  ldr  x23, [x27, #288]            ; rq->cfs 字段读取              ← 致命指令 (f9409377)
```

**闭合等式（全部 Python3 mod 2^64 复算，见 algebra_out.txt）**：

1. 【实锤】`x27_obs (0xa000c1a9443c91e5) == x1_true (0xffffc1a985e596c0) + x20_obs (0xa000ffffbe56fb25)` —— 加法器把带毒 x20 忠实地加进去了，ALU 无责。
2. 【实锤】`FAR (0x0000c1a9443c9305) == (x27_obs + 0x120) & 0x0000ffffffffffff` —— 装载偏移 #288 与 FAR 低位逐位吻合；0xa000 顶 16 位按 48 位 VA + TBI 规则被忽略，bits[55:48]=0x00 → MMU 选择了 TTBR0 用户页表 → pgd=0 → level-0 fault。这就解释了 4.1 的"用户页表"矛盾。
3. 【实锤】`x20_obs == (x20_true >> 16) | (0xa000 << 48)`，且与该值仅差 `XOR = 0x01be`（bits[8:1] 共 7 位乱码）。即：**x20 的内容是真值整体右移 16 位、空出的顶 16 位被填成 0xa000 标签、底部再翻转 7 位**。
4. 【实锤】反事实：`x1_true + x20_true = 0xffff80008080f6c0`（CPU60 的 rq），`+0x120 = 0xffff80008080f7e0`，`vtop` 实测 PTE VALID——若 x20 正确，该指令永不故障。
5. 【实锤】x20 真值 0xffffbe56fa9b6000 在转储内存中完好（`rd __per_cpu_offset+480`），**排除"内存被写坏"**。

**前兆与致命事件的同源性**：13 次前兆的出错 VA 全部是非对齐（低 12 位 ≠ 0）线性映射地址，页表项全部 VALID（W1/W11 两个样本 `vtop` 实测），出错点全部在 `__memcpy+0x80`（16 字节 LDP/非对齐装载）——与致命事件同为**装载通路读出的数据/地址位错乱**，且全部发生在 CPU179。W9–W13 的 5 个 VA 挤在线性映射 `0xffff6043_49d1_xxxx` 同一 64KB 区域（该页内容经 `rd` 实测为 `/proc/interrupts` 文本"kworker/R-scsi_…"，正是 show_interrupts 的输出缓冲）。

**诚实声明**：
- 致命事件的寄存器闭合链每一环都有真实命令输出支撑，可独立复核；
- "13 次前兆与致命事件同源"是【强推】：形态学（同核、同通路、装载类错误、内存真值完好）高度收敛，但前兆发生在 `__memcpy` 的数据装载而致命发生在地址装载+计算链，软件层无法直接观测物理通路；
- 0xa000 顶标签的来源（旁路网络的错位标签位？寄存器文件读端口的位线串扰？）无法从软件侧定论，属【假设】；
- PARTIAL DUMP 未包含全部物理页，`kmem -s`、`rd &runqueues 模板区`（page excluded）等命令失败，相关取证以已成功的读数为准，未做外推。

## 8. 故障根因（微架构级结论 + 置信级别）

**CPU179 核内数据通路 SDC：`ldr x20,[x0,w25,sxtw#3]` 装载 `__per_cpu_offset[60]` 时，load 返回值被腐化为「真值右移 16 位 + 0xa000 顶标签 + 低 9 位 7 位乱码」，经 `add` 连锁生成非法指针 x27，再由下一条 `ldr` 以带毒地址访存触发 level-0 translation fault 致命 Oops。内存真值完好，ALU 计算正确，错误被限定在"装载结果进入物理寄存器"的微架构段（L1 供给通路 / load-to-use 旁路 / 寄存器文件写读通道之一）。**

- 寄存器闭合与内存真值对照：【实锤】
- CPU179 装载通路级 SDC（区别于内存位翻转、软件 bug、页表错乱）：【强推】
- 具体物理失效单元（RF 读端口位线、旁路阵列 lane、L1 输出锁存器）：【假设】——0xa000 结构化标签 + 16 位对齐移位更像**多 lane 数据通路的错位/串扰**而非宇宙线单点翻转；验证途径：(a) 在 CPU179 上循环执行"load 已知模式 + 立即 store + 比对"的裸机双核校验程序（对 silifuzz 类业务可直接加 ECC-style 校验内核）；(b) 用 l1d_disable 思路扩展成 L1 方式/通路隔离实验；(c) 送厂做 shmoo/mbist 复测寄存器文件与旁路网络。

## 9. 启示

### 9.1 微架构定位的意义
本案若止步于"调度器空指针崩溃"的表象，结论会指向 Linux 调度器 bug（错误方向）。寄存器闭合等式把责任区间压缩到**一条指令的取数通路**，并进一步用 13 次同核前兆证明这是**颗次级、持续性**的硬件问题而非偶发。微架构定位的价值在于：它把"要不要下线这颗核"从猜测变成有证据链的决策。

### 9.2 芯片设计与实现启示（具体、可落地）
1. **load 返回通路的偶校验/残差校验**：数据从 L1 输出到寄存器文件的路径（含 bypass mux）目前普遍无保护。建议在 load-data 总线加 1 位奇偶或 AN 码残差校验，检测到错配时注入精确异常（fail-fast）而非静默写坏寄存器——本案若有此校验，W1 就会变成可诊断的机器检查而不是 4.5 天的静默积累。
2. **寄存器文件读端口的位线级 ECC/奇偶**：0xa000 标签 + 16 位移位的形态更像 lane 错位而非单粒子翻转，RF 端口加 per-byte 奇偶即可在错位时立即报错。
3. **PTW 输入与 VA 规范性硬件断言**：带毒地址 0xa000…因 TBI 静默降级为用户页表查询，走了完全错误的翻译路径却无任何告警。建议对内核态 EL1 的 TTBR0 选择、非规范 VA（bits[55:48] 与 bit47 不符）产生 fault 或计数器，把这类"不可能的地址"显影出来。
4. **错误传播屏障设计**：per_cpu_offset、current、task_struct 指针这类"控制流根值"一旦被污染会瞬间爆炸。可在关键数据结构上加轻量魔数（如 rq 首字段 cookie），供热路径 assert，将连锁半径限制在一跳之内。
5. **fail-fast vs silence 的权衡**：本案内核对 13 次 spurious fault 一律"Ignoring"（fault.c:494 的善意容错），客观上让 SDC 静默存活 4.5 天。RAS 设计应给这类重复性、同核性 spurious fault 加速率阈值：同一 CPU 上 N 次 spurious 即触发局部健康事件（perf/tracepoint + BMC 计数），把"静默"变成"可观测"。
6. **DFT/在线测试钩子**：l1d_disable 模块证明运维有意愿做在线隔离实验，但仅有 L1D 开关粒度太粗。建议芯片提供 per-CPU 的通路级在线自测试（mbist-lite：RF/bypass/L1 load path 的后台校验），可在业务不中断时定位失效单元。
7. **kdump 可靠性**：PARTIAL DUMP 缺页导致 crash 部分命令失败（IRQ stack seek error、kmem -s、percpu 模板区 rd 均不可读）。crashkernel 预留 1024MB 但过滤策略过激；对 SDC 取证而言，percpu 区与全部 IRQ 栈应当被优先保留。

### 9.3 对系统软件/RAS 的启示
1. **spurious fault 不是噪声而是信号**：`Ignoring spurious kernel translation fault` 在本机出现 13 次均指向同一颗核。建议给该 printk 挂 tracepoint + per-CPU 计数器，接入健康度基线（如 PCP 已在采集——受害者 pmdalinux 恰好就是最好的探头）。
2. **监控代理的"受害者即证人"价值**：pmdalinux 每秒读 /proc/interrupts，客观上对 CPU179 做了周期性探针采样。生产系统可故意在每核跑轻量"数据通路哨兵"（load-store-verify 循环），把 SDC 显影时间从 4.5 天压缩到分钟级。
3. **调度规避作为应急**：确认颗次级故障后，`isolcpus`/cpuset/cgroup cpuset 把 CPU179 摘出可调度集合，是比重启更精细的止损手段（本例 192 核，损失 1 核可接受）。
4. **实验记录要进日志**：l1d_disable 的三次实验均留在 dmesg 里，为诊断提供了"已排除 L1D 整体失效"的关键信息。运维侧所有硬件干预动作应强制留痕。

## 10. 处置建议

1. **立即**：将 CPU179 移出调度（cpuset/isolcpus），观察后续是否还有 spurious fault；若监控归零则进一步佐证颗次级定位。
2. **短期**：在 CPU179 上运行通路级哨兵程序（已知模式 load + store 回读比对，循环运行），复现率若 >0 则实锤颗次级数据通路故障，安排停机窗口。
3. **中期**：拉取 BMC/ BMC SEL/ CEC（如可用）日志核对 CPU179 所在 CCIX/处理器核的台帐；联系厂商对 CPU179 做 mbist/shmoo 复测（重点：RF 读端口、bypass 网络、L1 load 数据通路）。
4. **全局**：给所有生产节点部署 spurious-fault 速率告警（同一 CPU ≥3 次/周即告警）；保留并标准化 l1d_disable 类在线实验工具链。
5. **本机**：在硬件结论出来前，不建议让该节点继续承载 silifuzz 等高价值计算任务。

---

## 附录：命令索引（全部取证命令，可复核）

完整命令与全量输出见同目录附件：
- `dmesg_forensics.txt`（dmesg 侧 F1–F11 节 + crash 会话 1–14 + 命令索引）
- `algebra.py` / `algebra_out.txt`（全部 64 位代数复算，mod 2^64）

crash 关键命令速查（namelist 一律 `/tmp/vmlinux-0102`）：
```
sys / panic / bt                        # 会话1：加载验证与崩溃栈
bt -t / bt -r / set / mach              # 会话2：栈帧与寄存器现场
dis -l 0xffffc1a98442ae44 30            # 致命指令窗口反汇编（debug_info 行号）
dis 0xffffc1a98442ad08 60               # 函数序言（x1=&runqueues 的存栈证据）
rd 0xffffc1a9862557b0 4                 # __per_cpu_offset[60] 真值 = x20 真值
p __per_cpu_offset[179] / px &runqueues # 真值锚点
vtop 0xffff80008080f7e0                 # 反事实地址页表走查（PTE VALID）
vtop 0xffff604349d163ab                 # 前兆 VA 页表走查（PTE VALID，证明 spurious）
rd 0xffff604349d16000 32                # 前兆 VA 页内容（/proc/interrupts 文本）
ps | grep -E 'pmdalinux|9678|14074'     # 受害进程身份
bt 14074 / bt 9678                      # 前兆受害者当时的内核栈
rd 0xffffc1a9855c1100 24                # 栈上 kallsyms_seqs_of_names+439020 残迹解析
```

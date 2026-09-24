# CPU179 转储深度诊断报告（第 1 次独立重研究）

## ——per_cpu 偏移量装载全 64 位零塌缩：调度器热路径上"装载结果 ≠ 内存真值"的教科书级实锤

| 项目 | 内容 |
|---|---|
| 目标转储 | /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-25-15:42:24/vmcore（27.6GB, Kdump compressed v6, PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48, 192 核 Kunpeng-920 (TaiShan-v110, 0x481fd010), 8 NUMA 节点, 768GB |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP, KASLR+KPTI 开启 |
| 崩溃时间 | 2026-08-25 15:41:38 CST（uptime 76809s ≈ 21小时20分，开机于 08-24 18:21:28） |
| 受害进程 | PID 2018836 Comm=claude（用户态 epoll_pwait2 系统调用入口） |
| 崩溃点 | CPU 179, find_busiest_group+0x140, FSC=0x07 L3 translation fault, FAR=ffffa5aa9b5a97e0 |
| 前兆 | t=1707s 同核（CPU179）spurious translation fault WARNING（pmdalinux 读 /proc/interrupts），间隔 20.86 小时 |
| 结论一行 | `ldr x20,[x0,w25,sxtw#3]` 从已映射的 `__per_cpu_offset[176]`（真值 0xffffda55e61ce000）装载结果塌缩为全零，致 `rq(176)` 指针退化为 percpu 模板地址（init 区已解除映射）→ L3 缺页 → panic【强推：装载通路（load path / L1D 填充或寄存器写回）受扰，置信度高】 |

## 1. 执行摘要

1. **现象**：开机 21 小时 20 分后，CPU179 在 newidle 负载均衡（`find_busiest_group` 遍历节点 7 各 CPU 累计负载）中取 `rq(176)->cfs.avg.load_avg` 时触发 L3 翻译缺页，`Unable to handle kernel paging request at ffffa5aa9b5a97e0`，Oops → kdump。
2. **签名**：致命指令 `ldr x23,[x27,#288]`（find_busiest_group+0x140），x27 = `&runqueues`（percpu 模板地址）而非 `rq(176)` 实例地址；模板页位于 `__init_begin..__init_end` 区间内，boot 后已被解除映射（vtop 实测 PTE=0），所以本应"静默"的错误指针立刻以缺页形式暴露。
3. **闭合验证（本案核心实锤）**：异常帧 x20 = 0x0000000000000000，而 crash 从转储内存实测 `__per_cpu_offset[176] = 0xffffda55e61ce000`（非零）；反事实推演成立——若 x20 正确，x27 = 0xffff8000817776c0，vtop 实测该地址 PTE `e86057ffe68f03`（VALID），装载根本不会缺页。装载地址本身（0xffffa5aa9b9a5b50）位于已映射 .data 页（PTE `f8205091da5f03` VALID）。**即：指令对、地址对、内存真值对，唯独装载结果错——64 个比特全部塌缩为 0（36 个 '1' 位全部丢失）。**
4. **前兆收敛**：20.86 小时前（t=1707s）同核 CPU179 已发生一次 spurious kernel translation fault（`__memcpy` 内核态读 ffff00204f4e24a8，节点 3 线性映射区），该异常帧的 x3 恰好精确等于 `__per_cpu_offset[179]`（Python3 模 2^64 复算验证），与前兆同属 per-cpu/数据通路读取链路。同核、同类（数据通路），先兆后致命。
5. **微架构判定**：损伤点定位在 CPU179 的**装载通路（load path）**——L1D 命中/填充数据或装载结果写回寄存器堆的环节产生了全零结果。排除了 ALU（加法结果与两个输入完全自洽）、寄存器文件大面积损坏（其余 30 个寄存器全部与内存真值吻合）、内存本身写坏（转储中真值完好）。置信级别：【强推】（直接对照"寄存器值 vs 内存真值"已闭合，但无法在软件层回放受扰瞬间的物理通路）。
6. **处置**：本机为 l1d_disable 故障注入研究机（dmesg 显示该 out-of-tree 模块在 t=51k~63k 秒间针对 cpu 179 反复禁用/启用 L1D，最后一次卸载于致命崩溃前 3.7 小时）。建议对 CPU179 所在物理核做隔离与压力复测（见第 10 节）。

## 2. 证据规则与方法

- **证据源**：vmcore-dmesg.txt（2847 行）+ vmcore（27.6GB，crash 8.0.4 加载，namelist=/tmp/vmlinux-0102，BuildID 276194e5…，与转储匹配）。全部取证命令与输出见附件 `dmesg_forensics.txt`（含 crash 各会话），原始 crash 输出另存 `full_forensics_out.txt`、`pte_walk_out.txt`、`env2_out.txt`、`dis_fbg*_out.txt`、`final_forensics_out.txt` 等。
- **诚实铁律**：报告中每一处数据引用均来自实际命令输出；64 位运算全部由 `algebra.py`（模 2^64）完成，输出存 `algebra_out.txt`；crash 加载有大量 `seek error`（PARTIAL DUMP 的正常现象，IRQ stack 指针区域未入转储），不影响本案涉及的内存读取。
- **三级置信**：【实锤】= 可直接复核的对照（如寄存器值 vs 转储内存真值）；【强推】= 多源收敛但缺直接物理观测；【假设】= 无法软件验证，只给验证途径。
- **工具**：crash 8.0.4-17.oe2403sp4（内置 gdb 10.2）、grep/sed、python3。反汇编使用 crash `dis -l`（带 debug_info 行号注记）。

## 3. 本次开机时间线【时间线】

| uptime (s) | 墙钟（反推） | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0 | 08-24 18:21:28 | 开机，192 CPU、8 节点上线；KASLR+KPTI 强制开启 | 1–1255 | 实锤 |
| 0 | 18:21:28 | percpu 自动分配失败（max_distance 过大），回退 page-size 分配器，34 4K页/CPU（块间距 0x22000）| 319–323 | 实锤 |
| 1707.361 | 08-24 18:49:56 | **前兆**：CPU179, pmdalinux 读 /proc/interrupts → seq_printf → __memcpy 内核态访问 ffff00204f4e24a8 触发 spurious translation fault WARNING（arch/arm64/mm/fault.c:494），未致命 | 2580–2624 | 实锤 |
| 3155–3251 | 19:14–19:15 | audit backlog 持续超限（系统高负载期，audit_lost 累计 20 万+） | 2625–2753 | 实锤 |
| 51302–63485 | 08-25 08:36–11:59 | l1d_disable out-of-tree 模块 3 次加载/卸载，反复对 **cpu 179** 禁用/启用 L1D（SCTLR_EL1 翻转），共 11 次 DISABLED/RE-ENABLED | 2755–2791 | 实锤 |
| 63455 | 11:58 | opendcdiag（SDC 诊断工具）进程活动（memfd_create 提示） | 2785 | 实锤 |
| 76809.256 | 08-25 15:41:38 | **致命**：CPU179, find_busiest_group+0x140 L3 缺页 → die_kernel_fault → panic → kdump（"Bye!"） | 2792–2847 | 实锤 |

前兆→致命间隔 = 76809.256 − 1707.361 = 75101.89s = **20.86 小时**（Python3 复算，见 algebra_out.txt H 节）。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 2792–2805 行）

```
[76809.255750] Unable to handle kernel paging request at virtual address ffffa5aa9b5a97e0
[76809.264589] Mem abort info:
[76809.268259]   ESR = 0x0000000096000007
[76809.272885]   EC = 0x25: DABT (current EL), IL = 32 bits
[76809.279076]   SET = 0, FnV = 0
[76809.283004]   EA = 0, S1PTW = 0
[76809.287020]   FSC = 0x07: level 3 translation fault
[76809.293104] Data abort info:
[76809.297081]   ISV = 0, ISS = 0x00000007, ISS2 = 0x00000000
[76809.303627]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[76809.309731]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[76809.316094] swapper pgtable: 4k pages, 48-bit VAs, pgdp=00002050917d4000
[76809.323850] [ffffa5aa9b5a97e0] pgd=10006057fffff403, p4d=10006057fffff403, pud=10006057ffffe403, pmd=10006057ffffa403, pte=0000000000000000
[76809.337671] Internal error: Oops: 0000000096000007 [#1] SMP
```

要点：EC=0x25（EL1 数据中止）、WnR=0（**读装载**）、S1PTW=0（缺页发生在数据访问本身，而非页表遍历写）、FSC=0x07（**L3 翻译缺页，PTE=0**）——页表各级上层描述符均有效，唯独最末级 PTE 为 0。

### 4.2 全量寄存器（x0–x30，dmesg 2810–2823 行原文）

```
pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : find_busiest_group+0x140   lr : find_busiest_group+0x11c
sp : ffff8001f03fb720
x29: ffff8001f03fb8a0 x28: ffff8001f03fb830 x27: ffffa5aa9b5a96c0
x26: ffff604003e99780 x25: 00000000000000b0 x24: ffffa5aa9b9a5000
x23: 0000000000000400 x22: ffff604003e99780 x21: ffffa5aa9b99fcb0
x20: 0000000000000000 x19: ffff8001f03fb930 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000ffffd47eeeb0
x14: 0000000000000000 x13: 0000000000000000 x12: 000000000002dc17
x11: 00000000000000ad x10: 0000000000000000 x9 : ffffa5aa99b7ae58
x8 : ffff8001f03fb888 x7 : 0000000000000000 x6 : 00000000000000b0
x5 : ffff000000000000 x4 : 0000000000000002 x3 : 0000000000000030
x2 : 0000000000002149 x1 : ffffa5aa9b5a96c0 x0 : 00000000000000b0
CPU: 179 PID: 2018836 Comm: claude Kdump: loaded Tainted: G W OE
```

### 4.3 Call trace（完整，dmesg 2824–2843 行；crash bt 复核一致）

```
 find_busiest_group+0x140      ← 致命指令
 load_balance+0x108
 newidle_balance+0x198
 pick_next_task_fair+0x110
 pick_next_task+0x60
 __schedule+0x1b4
 schedule+0x58
 schedule_hrtimeout_range_clock+0xdc
 schedule_hrtimeout_range+0x1c
 ep_poll+0x300
 do_epoll_wait+0x100
 do_epoll_pwait.part.0+0x1c
 __arm64_sys_epoll_pwait2+0xc0
 invoke_syscall+0x50
 el0_svc_common.constprop.0+0xc8
 do_el0_svc+0x48
 el0_slow_syscall+0x44
 el0t_64_sync_handler+0x100
 el0t_64_sync+0x188
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

调度上下文：用户态 claude 进程在 `epoll_pwait2` 中睡眠超时被唤醒，`newidle_balance` 趁 CPU179 即将进入空闲做域内负载扫描——这是调度器最热的路径之一。

### 4.4 前兆异常原文（dmesg 2580–2624 行）

```
[ 1707.360942] ------------[ cut here ]------------
[ 1707.360956] Ignoring spurious kernel translation fault at virtual address ffff00204f4e24a8
[ 1707.360965] WARNING: CPU: 179 PID: 10245 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
[ 1707.361115] CPU: 179 PID: 10245 Comm: pmdalinux Kdump: loaded Not tainted ...
...
[ 1707.361133] x29: ffff80011f413880 x28: ffff00202f62e900 x27: 00000000ffffffd8
[ 1707.361137] x26: ffff80011f413bd0 x25: 0000000000000b62 x24: ffff00204f4e249e
[ 1707.361141] x23: 0000000080400009 x22: 0000000000000025 x21: ffff00204f4e24a8
[ 1707.361146] x20: ffff80011f413950 x19: 0000000096000044 x18: ffffffffffffffff
...
[ 1707.361165] x5 : ffff8000817c9d88 x4 : ffff80011f4136e0 x3 : ffffda55e6234000
...
[ 1707.361172] Call trace:
 __do_kernel_fault+0x130/0x1b8
 do_bad_area+0x70/0x88
 do_translation_fault+0x40/0x80
 do_mem_abort+0x4c/0xa8
 el1_abort+0x5c/0x150
 el1h_64_sync_handler+0xd8/0xe8
 el1h_64_sync+0x78/0x80
 __memcpy+0x80/0x240
 seq_printf+0xc4/0xe8
 show_interrupts+0x1d4/0x498
 seq_read_iter+0x168/0x478
 ...
```

细节（Python3 译码，algebra_out.txt G 节）：前兆帧 **x3 = 0xffffda55e6234000，与 `__per_cpu_offset[179]` 逐位相等**（off[176] + 3×0x22000，块间距与 dmesg "34 4K pages/cpu" 吻合）——前兆发生时 CPU179 正在 per-cpu 读取链路上（show_interrupts 逐 CPU 汇总中断计数正是 per_cpu 热循环）；其 __memcpy 载荷寄存器 x12–x17 的 ASCII 译码为 `"ffff00204f4e24a8tion fault at virtual address ff"`，即缺页地址自身的告警文本片段——同一地址被重复触及。前兆缺页地址 ffff00204f4e24a8 落在节点 3 线性映射区（SRAT：0x204000000000–0x2057ffffffff）。

### 4.5 RAS 负证据（dmesg grep）

对 `Machine check|Hardware error|ECC|memory failure|EDAC.*error|ras.*error` 全文检索：**无任何硬件错误记录行**。仅有两行 EDAC 框架注册信息（dmesg 1857、2176 行：`EDAC MC: Ver: 3.0.0`、`EDAC MC0: Giving out device to module ghes_edac.c ...`）。即：硬件自检（ARM64 RAS 扩展已检测并启用，dmesg 1252 行）从头到尾没有报过一次错——静默数据腐化未被任何 ECC/巡检机制捕获，这正是 SDC 的定义特征。

## 5. 业务现象

- **受害进程**：PID 2018836 `claude`（AI 编码助手 CLI 的 node 进程，crash ps 显示机器上同时有 4 个 claude 实例，victim 是其中运行于 CPU179 的一个）。它本身无任何异常行为：在 epoll 等待中正常睡眠，崩溃是调度器替它"背锅"——进程只是恰好睡在了 CPU179 上，newidle 均衡扫描经过节点 7 时踩中坏数据。
- **前兆进程**：PID 10245 `pmdalinux`（Performance Co-Pilot 的 Linux 数据采集代理），周期性读取 /proc/interrupts（正是逐 CPU 中断计数汇总路径），同样无辜。
- **研究环境上下文**：dmesg 2755–2791 行显示 `l1d_disable` out-of-tree 模块在 08:36–11:59 之间三次加载，**专门针对 cpu 179** 反复执行 L1D 禁用/启用（SCTLR_EL1=0x3464d999 ↔ 0x3464d99d，即翻转某一位）；另有 `opendcdiag`（开放 SDC 诊断套件）在 11:58 活动。本机是一台 SDC 故障注入/诊断研究机，实验对象核与两次异常受扰核完全一致（均为 179）。模块最后一次卸载距致命崩溃 3.70 小时（algebra_out.txt H 节）。

## 6. 诊断定位过程【诊断定位过程】

**P1 · dmesg 勘察**：提取开机指纹（192 CPU/8 节点/768GB、KASLR+KPTI、percpu 回退 page-size 分配器）；定位唯一致命块（2792–2847）与唯一前兆块（2580–2624）；RAS 负证据确认；时间线与墙上时间反推。

**P2 · 崩溃块提取**：ESR 解码（EC=0x25 DABT、WnR=0、FSC=0x07、S1PTW=0）→ 这是**读装载自身缺页**，不是页表写坏、也不是取指错。Code 窗口 `(f9409377)` 反汇编为 `ldr x23,[x27,#288]`（Python3 译码 imm12=0x24→288 字节）。

**P3 · crash 加载与反汇编**：27.6GB vmcore 用 /tmp/vmlinux-0102 成功加载（PARTIAL DUMP，IRQ stack 区域 seek error 属正常）。`dis -l` 得到 find_busiest_group 完整数据流（关键段）：

```
0xffffa5aa99b7adfc <find_busiest_group+244>: adrp x24, 0xffffa5aa9b9a5000 <node_data+560>
0xffffa5aa99b7ae04 <find_busiest_group+252>: add  x0, x24, #0x5d0        ; x0 = &__per_cpu_offset
0xffffa5aa99b7ae08 <find_busiest_group+256>: str  x0, [sp, #8]
...（循环体：_find_next_and_bit 选中组内下一个 CPU → w25）
0xffffa5aa99b7ae34 <find_busiest_group+300>: ldp  x0, x1, [sp, #8]       ; x0=&__per_cpu_offset, x1=&runqueues
0xffffa5aa99b7ae3c <find_busiest_group+308>: ldr  x20, [x0, w25, sxtw #3] ; ← x20 = __per_cpu_offset[176]
0xffffa5aa99b7ae44 <find_busiest_group+316>: add  x27, x1, x20           ; x27 = &runqueues + off[176] = rq(176)
0xffffa5aa99b7ae48 <find_busiest_group+320>: ldr  x23, [x27, #288]       ; ← 致命：x23 = rq->cfs.avg.load_avg
```

源行注记：crash gdb `list *0xffffa5aa99b7ae44` → `kernel/sched/fair.c:12050`（update_sg_lb_stats 的 `for_each_cpu(cpu, sched_group_span(group))` 循环体，5024 为内联 helper 行）。`rq+0x120` 字段语义由偏移代数闭合：`cfs(rq+0x80) + avg(cfs+0x80) + load_avg(avg+0x20) = rq+0x120`，即 `cpu_load(rq)` 读 `rq->cfs.avg.load_avg` 累加到 `sgs->group_load`。

**P4 · 内存真值对照（本案的关键实验）**：
- `sym runqueues` → runtime &runqueues = **0xffffa5aa9b5a96c0**，与异常帧 x1/x27 完全相等 → x27 确实是"模板地址"。
- `p __per_cpu_offset[176]` → **0xffffda55e61ce000**（转储内存真值，非零！）。而异常帧 **x20 = 0**。装载源地址 &__per_cpu_offset[176]=0xffffa5aa9b9a5b50 位于已映射 .data 页（vtop 实测 PTE `f8205091da5f03` VALID）。
- 反事实：正确 x27 = 0xffffa5aa9b5a96c0 + 0xffffda55e61ce000 = **0xffff8000817776c0**；`vtop 0xffff8000817776e0`（即正确 FAR）实测 **PTE e86057ffe68f03（VALID|…|DIRTY），物理页 0x6057ffe68000** → 若装载正确，指令根本不会缺页。
- 模板页为何缺页：`vtop ffffa5aa9b5a97e0` 实测 pgd/pud/pmd 均有效、**PTE=0**；readelf/nm 显示 `.data..percpu`（链接地址 ffff800081b52000–81b6c3e8）恰落在 `__init_begin(ffff8000819a0000)..__init_end(ffff800081f50000)` 区间内——arm64 把 percpu 模板放进 init 区，boot 后随 free_initmem 解除映射。内核从不直接访问模板地址（per_cpu_ptr 总是加偏移），所以这个"正常未映射"平时永远不会被踩到；一旦某次 per-cpu 偏移装载塌缩为 0，`模板地址+字段偏移`就会以 L3 缺页的形式把这次静默腐化**当场曝光**。
- 旁证：`p *(struct lb_env *)0xffff8001f03fb930`（异常帧 x19 所指的 env）实测 `dst_rq = 0xffff8000817dd6c0`，而 Python3 复算 `&runqueues + off[179] = 0xffff8000817dd6c0` **逐位一致**——percpu 加法公式在同一条调用链、同一时刻处处成立，唯独 x20 那一次装载塌缩。

**P5 · 软件成因排除**：
- 指令本身：`__per_cpu_offset[cpu]` 装载 + 加法是调度器/proc 等所有 per-cpu 访问的标准展开，开机 21 小时内在 192 个核上被执行了亿万次；若软件逻辑有缺陷不可能只在 CPU179、只在 t=76809s 出错一次。
- 寄存器文件：异常帧其余寄存器全部与内存真值吻合（x19→env 合法、x22/x26→sched_group 合法、x24→adrp 页基址、x25=176∈节点7 组掩码、x1→[sp+16] 重载值一致），损伤面收敛到单条装载的目的寄存器 x20。
- ALU：`x27 = x1 + x20` 的加法结果与两个输入完全自洽（0 + &runqueues = &runqueues），加法器无错；错的是输入 x20 的来源——装载。
- 内存写坏：转储中 `__per_cpu_offset[176]` 真值完好且与相邻项（175/177/178 呈 0x22000 等差）完全一致，数组本身健康。
- 竞争/并发：`__per_cpu_offset[]` 在 boot 完成后只读，无并发改写窗口。

**P6 · 定位收敛**：装载通路单点故障（结果全零），叠加同核 20.86 小时前的同通路前兆，收敛到 CPU179 的 load path。

## 7. 逻辑链条

**指令语义链（全部实锤）**：

```
ldr x20, [x0, w25, sxtw #3]   x0=&__per_cpu_offset, w25=0xb0(=CPU176)
    装载地址 = 0xffffa5aa9b9a5b50（已映射 .data 页, PTE f8205091da5f03 VALID）
    内存真值 = 0xffffda55e61ce000（crash p 实测）
    寄存器值 = 0x0000000000000000（异常帧实测）        ← 唯一异常点
add x27, x1, x20              x1=&runqueues=0xffffa5aa9b5a96c0（[sp+16] 重载, 与 sym 一致）
    x27 = 0xffffa5aa9b5a96c0（本应 0xffff8000817776c0 = rq(176)）
ldr x23, [x27, #288]          本应读 rq(176)->cfs.avg.load_avg
    实际 FAR = 0xffffa5aa9b5a97e0 = 模板&runqueues + 0x120
    该页 PTE=0（.data..percpu 位于 __init 区, boot 后已解除映射）→ FSC=0x07 L3 fault
```

**闭合等式（Python3 模 2^64，algebra_out.txt C/D/E/F 节）**：
1. x27 == x1 + x20 → True（x20=0 使加法退化为恒等）
2. FAR == x27 + 0x120 → True
3. 真值 XOR 寄存器 = 0xffffda55e61ce000 ≠ 0 → **64 位全部塌缩，36 个 '1' 位全部丢失**
4. 反事实：正确 FAR = 0xffff8000817777e0，vtop 实测 VALID → 缺页不会发生

**反事实推演**：若寄存器值正确（=内存真值），`ldr x23,[x27,#288]` 读的是节点 7 上 rq(176) 的 load_avg（物理页 0x6057ffe68000，已映射），指令正常完成，调度器继续扫描组内下一个 CPU——系统无任何异常。即：**本次崩溃 100% 由"x20 ≠ 内存真值"这一单点造成**。

**前兆同构（强推的关键支撑）**：t=1707s 的前兆帧 x3 精确等于 `__per_cpu_offset[179]`，事件发生在 show_interrupts 的 per-cpu 汇总循环（同为"按 CPU 号索引 per-cpu 数据"的通路）；两次事件同核（179）、同方向（读通路）、相隔 20.86 小时。前兆当时读的是节点 3 线性映射地址而触发 spurious fault（该次是地址缺页形态），致命次是偏移装载塌缩形态——表现不同，但都指向 CPU179 取数通路上的瞬态错误。

**诚实声明**：软件层可以看到"装载结果 ≠ 内存真值"，但无法区分错误发生在 L1D 命中数据、填充数据、 LSU→寄存器堆写回、还是 store-load 前递通路的哪一环；也无法排除"受扰的是 TLB/页表遍历但恰好表现为数据零"这类极小概率路径（本案 S1PTW=0、且装载源地址已映射、走查 PTE 有效，故该可能性极低）。另外，l1d_disable 注入实验与两次异常同核，但最后一次注入距致命时刻 3.7 小时，中间因果（累积损伤 vs 独立瞬时故障）无法从单一转锤证。

## 8. 故障根因（微架构级结论 + 置信级别）

**根因判定**：CPU179（Kunpeng-920/TaiShan-v110，节点 7 内物理核）的**内核态数据装载通路（load path：L1D 命中/填充数据 → 装载结果写回）发生单次瞬态受扰**，使一条对已映射、非零内存（`__per_cpu_offset[176]`，真值 0xffffda55e61ce000）的 64 位装载返回**全零**。该零塌缩随后沿纯数据流传播：`rq(176)` 基址计算退化为 percpu 模板地址 → 访问 init 区已解除映射页 → L3 翻译缺页 → panic。错误的物理形态为"多位同时归零"，符合数据通路供电/时序毛刺或阵列读出放大器整体失效的瞬态翻转特征，而非单粒子单比特翻转。

**微架构定位**（按证据形态）：
- 装载结果塌缩为 0 而内存真值非 0 → **load path / 数据通路受扰**【强推】
- 排除 PTW 输出错（FSC 缺页是"真缺页"——模板页 PTE 确实为 0，缺页是零塌缩的下游后果而非独立故障）
- 排除 ALU 通路（加法与输入自洽）、排除寄存器文件多点损坏（其余 31 个寄存器全对）、排除内存持久写坏（真值完好）
- 旁证：同核 20.86h 前同通路前兆；同核为 l1d_disable 注入实验对象核

**置信级别**：微架构层级"装载通路受扰、零塌缩形态"为**【强推】**（寄存器 vs 内存真值的闭合对照为实锤，但物理通路无法软件回放）；"与 l1d_disable 注入存在因果"为**【假设】**（验证途径：对 CPU179 重复注入并长时间跑调度器压力 + per-cpu 指针 CRC 抽查，见第 10 节）。

## 9. 启示

### 9.1 微架构定位的意义

本案把一处"调度器 Oops"还原成一次可精确对账的单点数据通路故障：坏值既不是算出来的（ALU 输入输出自洽），也不是内存里本来就有（真值非零且完好），而是**在"内存→寄存器"这一段凭空变成了零**。这个定位方法（异常帧寄存器值 vs 转储内存真值的逐位对账 + 反事实页表走查）对所有 SDC 类故障通用：只要坏值可追溯到一条装载指令，就能把"软件崩溃"与"硬件静默出错"切开。更妙的是本案的天然放大器——percpu 模板页落在 init 区、boot 后解除映射，任何 per-cpu 偏移的零塌缩都会立刻以缺页形式报警而不是静默传播。这提示我们：**关键基址表若映射到"错误即缺页"的地址空间布局里，硬件静默错误就能自动转化为 fail-fast 事件**。

### 9.2 芯片设计与实现启示（可落地）

1. **load path 的端到端校验**：数据从 L1D 阵列到寄存器写回之间存在多级锁存/前递，本案的"全零"说明该段缺少整体性保护。建议对装载结果增加端到端 parity/ECC 覆盖（含命中/未命中两条路径与前递旁路），或在关键阵列读出侧加"全零拍长（all-zero burst）"检测——n 位连续全零在真实数据上概率极低，可作哨兵。
2. **per-cpu 基址表是单点高危数据**：`__per_cpu_offset[]` 一个字错 = 整个核的 per-cpu 视图错。可在每项附加奇偶校验或以冗余副本（主表 + 补码表）存放，调度器热路径取用时做一次廉价校验（cost：一条 eor + cbz，相对每次均衡扫描可忽略）。
3. **fail-fast 地址空间设计**：让"绝不应被直接访问的模板/只读结构"落在访问即缺页的区域（本案 arm64 已天然如此），建议芯片侧配合提供"缺页即冻结核 + 保存装载流水现场"的调试状态（如记录出错装载的虚拟地址、cache 状态、way 信息），把不可复现的瞬态故障变成可归因的现场。
4. **DFT/在线测试钩子**：针对 LSU/L1D 数据通路提供运行时可触发的 LBIST/走查式自测（空闲态低频运行），并允许按核隔离执行——本案若有此机制，可在前兆（t=1707s）出现时即对 CPU179 做在线诊断，而不是等 20.86 小时后致命。
5. **验证启示**：零塌缩类故障应在门级仿真中以故障注入（slew/voltage droop/位线扰动）定向覆盖"装载全零"形态，并用调度器这类高频索引大表的工作负载做长时间灌浆——本案证明这类负载天然是零塌缩的"探测器"。
6. **RAS 缺口**：ARM64 RAS 扩展已启用却零报告，说明数据通路瞬态错不落在现有 RAS 计数器视野内。建议在 RAS 架构里增加"数据完整性事件"的轻量计数（哪怕只做渗透率统计），为 SDC 现场提供硬件侧旁证。

### 9.3 对系统软件/RAS 的启示

- **内核侧哨兵**：调度器可在 per-cpu 基址使用处加 debug 断言（offset==0 且 cpu!=0 时 WARN），把这类静默腐化提前到首次出错点报告（本案表现为缺页，若模板页碰巧被映射，将变成静默读错数据而不崩溃——那才是真正的 SDC 传播）。
- **前兆的价值**：一次 spurious fault WARNING 值得当作硬件健康信号纳入监控（同核重复出现即隔离复测），而不是当作噪声过滤掉。本案前兆与致命同核同通路，间隔近 21 小时，窗口足够做处置。
- **kdump 可靠性**：PARTIAL DUMP 丢失 IRQ stack 区域（大量 seek error）但保留了关键数据结构；建议 kdump 优先级中提高 percpu/调度数据与活跃任务栈的保真度——它们是 SDC 取证的主战场。
- **故障注入研究的对照设计**：l1d_disable 这类注入实验应固定保留"未注入对照组"的相同时长运行记录，以便事后区分"注入后遗损伤"与"背景瞬时故障"。

## 10. 处置建议

1. **短期**：隔离 CPU179（kernel cmdline `isolcpus=179` 或 cpuset 置 offline）观察是否复现；对整机做 memtester/EDAC 巡检一轮（预期通过——本案非 DRAM 持久故障）。
2. **复测**：在 CPU179 上运行调度器压力（hackbench/stress-ng sched 类）+ per-cpu 指针抽查守护进程（周期校验 `__per_cpu_offset[i]` 与 `rq(i)` 可达性），复现窗口按本机历史（数小时~1 天）安排。
3. **对照实验**：若 l1d_disable 注入实验仍在进行，增加"仅加载不注入"与"不加载"对照组，验证注入与零塌缩的因果性。
4. **上报**：若同批机器出现同签名（find_busiest_group/find_*_group 类 per-cpu 零塌缩），按 CPU 批次汇总上报硬件侧做通路级筛查。

## 附录：命令索引（可复核）

dmesg 侧（详见 dmesg_forensics.txt 第 1–244 行）：
```
wc -l vmcore-dmesg.txt
sed -n '1,4p' / sed -n '2580,2624p' / sed -n '2754,2791p' / sed -n '2792,2847p' vmcore-dmesg.txt
grep -n 'Kernel command line' / grep -n -E 'Memory:|Total pages|crashkernel' vmcore-dmesg.txt
grep -n -E 'WARNING|BUG|Oops|cut here|spurious|Unable to handle|Internal error' vmcore-dmesg.txt
grep -n -E 'Machine check|Hardware error|ECC|memory failure|EDAC' vmcore-dmesg.txt
grep -n 'l1d' vmcore-dmesg.txt
```

crash 侧（namelist 一律 /tmp/vmlinux-0102，详见 dmesg_forensics.txt 第 245 行起及各 *_out.txt 附件）：
```
crash /tmp/vmlinux-0102 <vmcore> -i <cmdfile>        # 批处理会话×7
sys / panic / set / mach / bt
dis -l ffffa5aa99b7ad00 90 ; dis -l ffffa5aa99b7ae44 30
p __per_cpu_offset[176] ; p __per_cpu_offset[177..179] ; rd ffffa5aa9b9a55d0 32
sym runqueues ; sym node_data ; sym __per_cpu_offset ; p &runqueues
vtop ffffa5aa9b5a97e0 ; vtop ffffa5aa9b5a96c0 ; vtop ffffa5aa9b9a55d0 ; vtop 0xffff8000817776e0
rd ffff6057ffffac00 128 ; rd ffff6057ffffad00 64          # L3 PTE 表全零实锤
rd 0xffff604003e99780 16 ; rd ffff8000817776c0 8
p *(struct lb_env *)0xffff8001f03fb930
p *(struct sched_group *)0xffff604003e99780
p *(struct sched_domain_shared *)0xffffa5aa9b5a96c0   # 读到的是模板区字符串,佐证模板非运行数据
p &((struct lb_env *)0)->dst_rq ; p &((struct sched_domain *)0)->groups ...
p &((struct rq *)0)->cfs ; p &((struct cfs_rq *)0)->avg ; p &((struct sched_avg *)0)->load_avg
gdb list *0xffffa5aa99a84800 ; gdb list *0xffffa5aa99b7ae48   # 源行定位
task -R pid,comm | grep 2018836 ; ps | grep -E 'claude|pmdalinux'
search -t 2018836 ffffa5aa9b5a96c0                      # 无结果(栈上无残留,正常)
```

代数复算：`python3 algebra.py | tee algebra_out.txt`（KASLR 推算、闭合等式×3、反事实、前兆 x3 对照、时间线、寄存器自洽性核对，全部模 2^64）。

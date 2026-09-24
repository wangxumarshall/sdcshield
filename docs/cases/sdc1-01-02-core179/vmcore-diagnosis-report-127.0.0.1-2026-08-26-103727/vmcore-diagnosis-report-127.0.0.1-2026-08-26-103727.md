# CPU179 转储深度诊断报告（第 1 次独立重研究）
## ——一次装载结果塌缩为 0 的"零前兆-致命崩溃"闭环：x20 = __per_cpu_offset[179] 读出为 0 而内存真值非 0，且同核 18 小时内已有 9 次同通路前兆

| 项目 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-26-10:37:27/vmcore`（Kdump compressed v6，PARTIAL DUMP，13.9GB） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 核 Kunpeng-920（TaiShan-v110，0x481fd010），768GB，8 NUMA 节点 |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP，openEuler 24.03 SP3，KASLR 开启 |
| 崩溃时间 | 2026-08-26 10:36:43 CST（kdump 捕获时刻，uptime 66685.62s ≈ 18.52 小时，开机约在 08-25 16:05:17） |
| 受害进程 | PID 256855 `mi-scavenger`（用户态 futex 系统调用返回后，调度器 newidle_balance 路径被击中） |
| 前兆 | 同核 CPU179 上 9 次 `Ignoring spurious kernel translation fault` WARNING（irqbalance×4 / pmdalinux×5），全部 show_interrupts→__memcpy 写方向假缺页 |
| RAS 负证据 | 无 Machine check / Hardware error / EDAC 错误 / memory failure；rasnode 全 192 CPU 扫描 ERRSTATUS 无有效记录（V 位=0） |
| 一行结论 | CPU179 装载通路（load path）被扰：`ldr x20,[x0,w25,sxtw#3]` 把非零的 `__per_cpu_offset[179]`（0xffffdd6d7fa64000）读成 0，导致 rq 指针零塌缩、访问镜像未映射洞而致命缺页——**SDC 实锤在装载结果，微架构根因判定为 CPU179 核内数据通路受扰（置信：强推）** |

---

## 1. 执行摘要

1. **现象**：开机 18.5 小时后，CPU179 在调度器 `find_busiest_group()` 热路径中触发 `Unable to handle kernel paging request at virtual address ffffa29301d797e0`，level 3 读方向缺页，kdump 捕获。
2. **签名**：致命指令是 `ldr x23,[x27,#288]`（fair.c:12050 的 `rq = cpu_rq(cpu)`），x27 = &runqueues + x20；x20 恰为 0，使 x27 塌缩为 &runqueues 镜像地址本身，FAR = x1+0x120 精确闭合。
3. **闭合验证结果**【实锤】：转储中 `__per_cpu_offset[179]`（地址 ffffa29302175b68）真值 = **0xffffdd6d7fa64000（非零）**，而寄存器 x20 = **0x0**——装载结果与内存真值不符，这是"装载被腐化"而非"内存被写坏"的直接证据。
4. **反事实验证**【实锤】：若 x20 正确，访问目标应为 ffff8000817dd7e0；crash `vtop` 实测其 PTE 有效（e86057ffe02f03）、内容非零（0xf1），绝不会缺页。观测 FAR 所在页（ffffa29301d79000）在转储页表中 PTE=0，且物理页是 sighand_cache 的 slab 页——镜像映射在此处本就是洞，访问必 fault，与 FSC=0x07 完全自洽。
5. **前兆群**【强推】：18 小时内同核 CPU179 有 9 次前兆 WARNING，全部是写方向 level-0 假缺页（页表在转储中实测全部有效映射，内核自己都打印 "Ignoring spurious"），物理页全部位于 CPU179 所属的 NUMA node 7 本地。前兆与致命事件同为"访存结果错误"，同一核、不同进程、不同地址、跨 18 小时——软件 bug 不会挑核挑通路。
6. **判定与置信**：微架构根因 = **CPU179 的 load/页表走查数据通路受扰（装载结果塌缩为 0 / 假缺页）**，受害对象先后为 show_interrupts 的 memcpy 写通路与调度器的 percpu 偏移读通路。置信级别：闭合验证部分【实锤】，定位到"核内数据通路"【强推】，具体物理机理（缓存行/TLB/PTW/填充缓冲）无法软件区分【假设，给出验证途径】。

## 2. 证据规则与方法

- **证据源**：仅使用 `vmcore`（crash 8.0.4-17.oe2403sp4 + namelist `/tmp/vmlinux-0102`，BuildID 276194e5f356f9c4bc570bb0a750394d7b768035）与 `vmcore-dmesg.txt`。未读任何既有诊断文档（dump 目录内 DIAGNOSIS_REPORT.md、docs/cases*、docs/hypothesis 等），保证独立性。
- **诚实铁律**：所有引用均来自实际运行命令的真实输出（附件 `dmesg_forensics.txt`、`crash_forensics.txt` 全文可复核）。crash 报告 `[PARTIAL DUMP]`，会话中有 384 条 `seek error`（IRQ stack pointer 等），已如实记录，不影响本案核心读证（关键地址均可读）。
- **三级置信**：【实锤】= 可直接复核（如寄存器值 vs 内存真值对照）；【强推】= 多源收敛但无法直接观测内部状态；【假设】= 软件不可验证，给出验证途径。
- **64 位运算**：全部由 `algebra.py`（mod 2^64）计算，输出在 `algebra_out.txt`。
- **工具清单**：crash 8.0.4（内置 gdb 10.2）、objdump 2.41（aarch64）、nm、grep/sed、python3。

## 3. 本次开机时间线【时间线】

| uptime | 墙钟（推算） | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0.000000 | 08-25 16:05:17 | 开机，CPU 0x80000 (0x481fd010)，8 节点 192 CPU | L1 | 实锤 |
| 0.352047 | 16:05:17 | smp: Brought up 8 nodes, 192 CPUs | L1255 | 实锤 |
| 28.6 / 38.9 | 16:05:46 | 文件系统挂载完成 / firewalld 启动 | L2570, L2574 | 实锤 |
| 1467.049336 | 16:29:44.349 | **W1** 前兆：irqbalance(PID 9631) CPU179 假缺页写 ffff6040082574a8 | L2580 | 实锤 |
| 1467.049713 | 16:29:44.349 | **W2** 同上 ffff604008257138 | L2625 | 实锤 |
| 1467.055102 | 16:29:44.355 | **W3** 同上 ffff604008257752 | L2670 | 实锤 |
| 8026.64 | 08-25 18:18:43 | rasnode 模块加载并扫描全 192 CPU RAS 节点：无有效错误记录 | L2714-3869 | 实锤 |
| 61983.421239 | 08-26 09:18:20.721 | **W4** pmdalinux(PID 13610) CPU179 假缺页写 ffff6040088433d7 | L3871 | 实锤 |
| 62053.482638 | 09:19:30.782 | **W5** pmdalinux ffff6040088447d6 | L3916 | 实锤 |
| 62477.049447 | 09:26:34.349 | **W6** irqbalance ffff604008845731 | L3961 | 实锤 |
| 62893.399729 | 09:33:30.699 | **W7** pmdalinux ffff6040088416b8 | L4006 | 实锤 |
| 62903.472834 | 09:33:40.772 | **W8** pmdalinux ffff604008841277 | L4051 | 实锤 |
| 64953.445791 | 10:07:50.745 | **W9** pmdalinux ffff6040074a8369 | L4096 | 实锤 |
| **66685.621071** | **10:36:42.921** | **致命 Oops**：mi-scavenger(PID 256855) CPU179，find_busiest_group 读缺页 FAR=ffffa29301d797e0 | L4140 | 实锤 |
| 66686.05 | 10:36:43 | `Bye!` 进入 crashdump kernel（crash `DATE: Wed Aug 26 10:36:43`） | L4192 | 实锤 |

墙钟推算法：crash `sys` 实测捕获时刻 DATE=2026-08-26 10:36:43、UPTIME=18:31:26，反推开机墙钟（Python3 计算，见 algebra_out.txt）。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文

```
[66685.621071] Unable to handle kernel paging request at virtual address ffffa29301d797e0
[66685.633402]   ESR = 0x0000000096000007
[66685.637941]   EC = 0x25: DABT (current EL), IL = 32 bits
[66685.647889]   EA = 0, S1PTW = 0
[66685.651818]   FSC = 0x07: level 3 translation fault
[66685.661155] Data abort info:
[66685.667433]   ISV = 0, ISS = 0x00000007, ISS2 = 0x00000000
[66685.673275]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[66685.686877] swapper pgtable: 4k pages, 48-bit VAs, pgdp=0000403593ba4000
[66685.686877] [ffffa29301d797e0] pgd=10006057fffff403, p4d=10006057fffff403, pud=10006057ffffe403, pmd=10006057ffffa403, pte=0000000000000000
[66685.700210] Internal error: Oops: 0000000096000007 [#1] SMP
[66685.809971] CPU: 179 PID: 256855 Comm: mi-scavenger Kdump: loaded Tainted: G        W  OE       6.6.0-145.3.23.154.oe2403sp3.aarch64 #1
[66685.839249] pc : find_busiest_group+0x140/0xb60
[66685.844584] lr : find_busiest_group+0x11c/0xb60
```
（dmesg L4140-L4166，`WnR=0` 读方向、`FSC=0x07` L3 缺页；Code 窗口 `Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)`，括号内即致命指令编码。）

### 4.2 全量寄存器（x0–x30）

```
pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : find_busiest_group+0x140/0xb60        lr : find_busiest_group+0x11c/0xb60
sp : ffff8001e8ffb740
x29: ffff8001e8ffb8c0  x28: ffff8001e8ffb850  x27: ffffa29301d796c0
x26: ffff604003e27660  x25: 00000000000000b3  x24: ffffa29302175000
x23: 0000000000000400  x22: ffff604003e27660  x21: ffffa2930216fcb0
x20: 0000000000000000  x19: ffff8001e8ffb950  x18: 0000000000000000
x17: 0000000000000000  x16: 0000000000000000  x15: 0000ffffa5497e20
x14: 0000000000000000  x13: 0000000000000000  x12: 0000000000000019c58
x11: 000000000000005e  x10: ffff8000817c9430  x9 : ffffa2930034ae58
x8 : ffff8001e8ffb8a8  x7 : 0000000000000000  x6 : 00000000000000b3
x5 : fff8000000000000  x4 : 0000000000000002  x3 : 0000000000000033
x2 : 00000000000033de  x1 : ffffa29301d796c0  x0 : 00000000000000b3
```
（dmesg L4158-L4169。**x20 = 0** 是全案的关键异常值；x25 = 0xb3 = 179 与 CPU 号一致。）

### 4.3 Call trace（完整）

```
 find_busiest_group+0x140/0xb60      <- fair.c:12050
 load_balance+0x108/0x6c0             <- fair.c:13471
 newidle_balance+0x198/0x510          <- fair.c:14562
 pick_next_task_fair+0x110/0x718
 pick_next_task+0x60/0x398
 __schedule+0x1b4/0x8a0
 schedule+0x58/0x130
 futex_wait_queue+0x78/0xb0
 futex_wait+0xe8/0x1d0
 do_futex+0xec/0x1a0
 __arm64_sys_futex+0x80/0x198
 invoke_syscall+0x50/0x128
 el0_svc_common.constprop.0+0xc8/0xf0
 do_el0_svc+0x48/0x78
 el0_slow_syscall+0x44/0x1b8
 el0t_64_sync_handler+0x100/0x130
 el0t_64_sync+0x188/0x190
```
（dmesg L4172-L4188；crash `bt -l` 补充行号：machine_kexec→…→die_kernel_fault→__do_kernel_fault→do_bad_area→do_translation_fault→do_mem_abort→el1_abort→el1h_64_sync_handler→el1h_64_sync→find_busiest_group。）

### 4.4 前兆异常原文（9 次 WARNING 摘样，W1 全文在附件）

```
[ 1467.049327] ------------[ cut here ]------------
[ 1467.049336] Ignoring spurious kernel translation fault at virtual address ffff6040082574a8
[ 1467.049344] WARNING: CPU: 179 PID: 9631 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
[ 1467.049506] pstate: 60400009 (nZCv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[ 1467.049509] pc : __do_kernel_fault+0x130/0x1b8
[ 1467.049517] x29: ffff80011265b880 x28: ffff40205cb6e900 x27: 00000000ffffffd8
[ 1467.049521] x26: ffff80011265bbd0 x25: 0000000000000b62 x24: ffff60400825749e
[ 1467.049530] x20: ffff80011265b950 x19: 0000000096000044 x21: ffff6040082574a8
[ 1467.049549] x5 : ffff8000817c9d88 x4 : ffff80011265b6e0 x3 : ffffdd6d7fa64000
[ 1467.049557] Call trace:
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
 proc_reg_read_iter+0x68/0xe8
 new_sync_read+0xac/0x148
 vfs_read+0x194/0x1e8
 ksys_read+0x78/0x118
 __arm64_sys_read+0x24/0x38
 invoke_syscall → el0_svc → el0t_64_sync
```
9 次共同特征（实测归纳，逐条在 algebra_out.txt [6][9] 复核）：
- **全部 CPU 179**；受害进程 irqbalance(PID 9631)×4、pmdalinux(PID 13610)×5；
- **同一路径** show_interrupts→seq_printf→__memcpy（读 /proc/interrupts）；
- ESR=0x96000044：**写方向** DABT、DFSC=0x04（level 0 translation fault）；
- x19 = 0x96000044（fault 时 ESR 存留）、x21 = 缺页地址、x24 = x21−0xa（memcpy 目的基址）9/9 成立；
- x3 = **0xffffdd6d7fa64000 = __per_cpu_offset[179]** —— 前兆时刻 CPU179 从同一数组**正确读出**了该值（对照价值见第 7 节）；
- 内核消息明示 "Ignoring spurious kernel translation fault"（fault.c:494，走查无 vma 但地址在内核空间且判为假错）。

### 4.5 RAS 负证据（dmesg grep）

```
### cmd: grep -n -E 'Machine check|Hardware error|EDAC.*error|memory failure|ECC' vmcore-dmesg.txt
（无匹配——仅 EDAC MC 初始化/ghes_edac 注册行，无任何错误事件）
```
- `[1.511600] EDAC MC0: Giving out device to module ghes_edac.c ...`（仅注册）
- 8026s 时 rasnode 模块扫描全 192 CPU、5 类 RAS 节点共 1152 条记录：ERRSTATUS 均无有效错误（V 位 bit31=0，STATUS 全 0xff/0x0，ADDR=0），如 `cpu=179 node=0 FR=0000000000004842 ... STATUS=00000000000000ff ADDR=0000000000000000`——这是基线配置读出，**无架构级记录的硬件错误**。
- 结论：本案为**静默**数据腐化（SDC），传统 RAS 通路完全无感。

## 5. 业务现象（受害进程与调度上下文解读）

- **致命事件受害进程 `mi-scavenger`（PID 256855）**：crash `ps` 实测 5 个 mi-scavenger 实例、cwd 为 `/home/sdc/vmcore/gem5-fi`、打开 /proc/256854/statm 与大量 eventpoll/eventfd/pidfd——是虚拟机核研究平台的清道户进程。崩溃点上下文：它在用户态执行 `futex(FUTEX_WAIT)` 睡眠后被唤醒/取消，`schedule()` 走 `newidle_balance`（CPU179 即将进空闲、寻找可拉取任务），`find_busiest_group` 遍历调度组内每个 CPU 取 `rq = cpu_rq(cpu)` 时被击中。**即：受害的不是 mi-scavenger 自身逻辑，而是它"恰好"是触发调度器负载均衡的那只替罪羊。**
- **前兆受害进程**：`irqbalance` 与 `pmdalinux`（PCP 性能采集器 linux PMDA）都是周期性读取 `/proc/interrupts` 的监控类守护进程。它们读 proc 文件时 seq_file 缓冲的 memcpy 写入被假缺页打断 9 次。crash 实测 seq 缓冲物理页在 **node 7**（CPU179 本地节点）。
- **机器背景负载**：crash `sys` 实测 load average 222.04；`runq` 显示全机 192 个 `movbe_l1d` 压力线程（每核一个）满负荷运行——这是一台**正在做故障注入/压力实验的机器**，movbe_l1d 是微架构压力负载（L1D 相关指令流）。压测负载放大了 CPU179 数据通路的受扰暴露频率。
- 前兆进程（irqbalance/pmdalinux）与致命进程（mi-scavenger）彼此无关，唯一交集是**都被调度到 CPU 179 执行**。

## 6. 诊断定位过程【诊断定位过程】

**P1 · dmesg 勘察**：提取开机指纹（192 CPU、768GB、KASLR、crashkernel=1024M,high）；grep 发现 9 次 spurious WARNING + 1 次致命 Oops；RAS 关键词全部无匹配。初步定性：非 ECC 可见错误，SDC 形态。

**P2 · 崩溃块提取**：提取 ESR/EC/FSC 解码（读方向 L3 fault）、pgtable 走查（pte=0）、全量寄存器、call trace、Code 窗口。发现 x20=0 异常。

**P3 · crash 加载**：`crash /tmp/vmlinux-0102 vmcore` 最小集（sys/panic/bt）加载成功（49 秒），确认为 PARTIAL DUMP、192 CPU、PANIC 串与 dmesg 一致。随后 12 个批处理会话做全量取证（bt -t/-F/-l、runq、ps、kmem -s、dev -p、net、vtop、rd、search 等），如实记录 384 条 seek error（不影响关键地址读证）。

**P4 · 内存真值对照（本案胜负手）**：
1. objdump 反汇编 `find_busiest_group`（link-time ffff80008013ad08）：致命点前后指令序列与 dmesg Code 窗口 5 条指令逐字吻合（`f9400782 f879d814 2a1903e0 8b14003b (f9409377)`）；
2. 由运行期 pc 与 link-time 符号算出 KASLR slide = 0x229280210000，进而得 `&__per_cpu_offset` 运行期 = 0xffffa293021755d0、`&runqueues` 运行期 = 0xffffa29301d796c0（与 crash `px &runqueues` 及 dmesg x1 双向印证）；
3. 指令语义链：`ldr x20,[x0,w25,sxtw#3]`（x20=__per_cpu_offset[179]）→ `add x27,x1,x20` → `ldr x23,[x27,#288]`（致命）；
4. **读真值**：`rd ffffa29302175b68` → **0xffffdd6d7fa64000（非零）**；寄存器 x20 = 0。**装载塌缩实锤**；
5. **闭合**：x27 = x1 + 0 = ffffa29301d796c0（与 dmesg x27 完全一致）；FAR = x27 + 0x120 = ffffa29301d797e0（与 dmesg FAR 完全一致，0x120=288 与 `ldr x23,[x27,#288]` 偏移吻合）；
6. **反事实**：x27 真值应为 x1 + 0xffffdd6d7fa64000 = **0xffff8000817dd6c0**，与 crash `runq` 输出的 `CPU 179 RUNQUEUE: ffff8000817dd6c0` 完全一致；`vtop ffff8000817dd7e0` 实测 PTE 有效（e86057ffe02f03）、内容非零（0xf1）。**若装载正确则根本不会缺页**；
7. **观测 FAR 页自洽性**：`vtop ffffa29301d79000` 实测 PTE=0，`kmem` 显示该物理页（403593d79000）属 sighand_cache slab——镜像映射在此处本就是洞（内核只通过 `&var + __per_cpu_offset[cpu]` 访问 percpu 实例，从不直接访问镜像内 cpu0 拷贝+offset），x20 塌缩后恰好踩进这个洞，与 FSC=0x07 L3 fault 自洽；
8. **前兆页表真值**：对 W1/W4/W9 三个缺页地址 `vtop`，全部由 1GB 大页 PTE e8604000000f05（VALID|…|DIRTY）有效映射——运行时的 level-0 假缺页与转储页表矛盾，证明前兆同样是"访存通路输出错误"而非真实未映射。

**P5 · 软件成因排除**：
- `find_busiest_group`/`cpu_rq()` 是调度器最热路径（每次 newidle balance 都执行，本机 load 222 下每秒数千次；该代码在全球存量内核上每天被执行亿万次），单次读错不能归因软件；
- `__per_cpu_offset[]` 数组自启动后不再变化，且 9 次前兆中 x3 从同一数组正确读出 0xffffdd6d7fa64000——数组本身与读路径在其它时刻均正常；
- 9 次前兆 + 1 次致命**全部锁定 CPU179**，跨 18 小时、5 个不同进程、3 类不同数据对象（seq 缓冲写、percpu 偏移读、rq 字段读）——软件 bug 不会挑核；内核版本为发行版稳定内核，无相关已知缺陷形态（假缺页 + 装载塌缩并存的组合更非软件模式）；
- 页表本身完好：转储页表显示应有映射全在、不应有的洞依旧是洞（与设计一致），排除"页表内存被位翻转"。

**P6 · 定位收敛**：三类现象统一为"**CPU179 发出的访存，其结果/翻译输出偶发错误**"：
- 前兆：写方向假缺页（level-0 fault，但 PGD→PUD 实测有效）→ PTW 走查输出或 TLB 项被扰；
- 致命：读方向数据装载塌缩为 0（内存真值非 0）→ load path 数据通路被扰。
两者都发生在 CPU179 的取数通路上， victim 进程与地址完全随机，时间跨度 18 小时低频率复发（约 10 次/18.5h），符合间歇性硬件受扰特征。至此收敛到微架构定位（见第 8 节）。

## 7. 逻辑链条

**指令语义**（objdump 实测，link-time；运行期 = +slide 0x229280210000）：
```
ffff80008013ae34: ldp  x0, x1, [sp, #8]        ; x0=&__per_cpu_offset, x1=&runqueues
ffff80008013ae3c: ldr  x20, [x0, w25, sxtw #3] ; x20 = __per_cpu_offset[x25=179]
ffff80008013ae44: add  x27, x1, x20            ; x27 = cpu_rq(179)
ffff80008013ae48: ldr  x23, [x27, #288]        ; ★致命：读 rq->字段@0x120
```
源码对应 kernel/sched/fair.c:12050（`for_each_cpu` 内 `rq = cpu_rq(cpu)` 后取负载统计）。

**闭合等式**【实锤】（全部 Python3 mod 2^64 计算，algebra_out.txt [4]）：
```
x25            = 0xb3 = 179                          （dmesg x25）
x0             = 0xffffa293021755d0 = &__per_cpu_offset（运行期，slide 计算 + crash rd 交叉验证）
x1             = 0xffffa29301d796c0 = &runqueues       （crash px &runqueues 输出一致）
mem[&pco[179]] = 0xffffdd6d7fa64000（crash rd 实测，非零）   ← 内存真值
x20            = 0x0（dmesg Oops 寄存器）                  ← 装载结果
x27            = x1 + x20 = 0xffffa29301d796c0            （= dmesg x27，逐位吻合）
FAR            = x27 + 0x120 = 0xffffa29301d797e0         （= dmesg FAR，逐位吻合）
x27 真值       = x1 + 0xffffdd6d7fa64000 = 0xffff8000817dd6c0（= crash runq 的 CPU179 RUNQUEUE，逐位吻合）
```

**反事实推演**【实锤】：若 x20 装载正确，访问 [0xffff8000817dd7e0]：crash vtop 实测 PTE=e86057ffe02f03（VALID），rd 实测内容 0xf1/0xf1/0x95——正常返回，不缺页、不 Oops。因此**唯一故障源就是 x20 装载塌缩**。

**前兆同源性**【强推】：9 次前兆与致命事件同核（179）、同为访存输出错误形态（假缺页 / 零塌缩），但对象不同（node7 本地 seq 缓冲写 vs node0 镜像区数组读）。它们共享的不是某个内存单元，而是"CPU179 的取数通路"这一微架构资源。

**诚实声明**：
- 本报告无法区分受扰的具体微架构环节（L1D/填充缓冲/TLB/PTW/交叉开关），软件不可见；
- PARTIAL DUMP 中部分页读不到（384 条 seek error），本案关键地址（__per_cpu_offset 数组、rq、前兆页表项）恰好全部可读，结论不受影响；
- movbe_l1d 压测与故障的因果（是否为注入源）无法从单 Dump 判定，只能确认其放大了暴露频率。

## 8. 故障根因（微架构级结论 + 置信级别）

**根因判定：CPU179 核内取数通路（load path / 页表走查输出）间歇性受扰，本次表现为装载结果塌缩为 0（x20 = 0 ≠ 内存真值 0xffffdd6d7fa64000），致使调度器 rq 指针零塌缩、访问内核镜像未映射洞，触发 level-3 读缺页致命 Oops。**

置信分级：
1. "装载结果与内存真值不符（装载被扰，非内存写坏）" ——【实锤】（寄存器 vs 转储内存直接对照，可复核）；
2. "故障环节位于 CPU179 核内数据通路（含 PTW 输出）" ——【强推】（10 起事件全落同一核、跨进程跨对象跨 18 小时、页表真值完好、RAS 全无记录，多源收敛但无法直接观测核内状态）；
3. "具体物理机理（粒子翻转缓存行/填充缓冲/TLB 项/PTW 锁存器、或供电/时序毛刺）" ——【假设】。验证途径：(a) 对该 CPU 做 SDE/在线 L1D 与 LSU 微码自检（如 `movbe_l1d` 定向压力 + 数据校验）；(b) 复机后隔离 CPU179（`nohz_full`/cgroup cpuset 避开）观察前兆是否消失；(c) 若为可注入平台，在 gem5-fi 中对 load 返回通路注入"位塌缩/置零"故障复现同签名；(d) 收集同批次机器同核位（179=node7 第 4 核）故障率统计，判断是否共性弱点。

## 9. 启示

### 9.1 微架构定位的意义
本案若只看" Oops 在调度器"，结论会停在"内核偶发崩溃"；把寄存器值与转储内存真值逐位对账后，才能区分三种截然不同的根因：内存被写坏（内存侧故障）、指针算术错（ALU 侧）、装载结果错（取数通路侧）。x20=0 而内存非 0 这一组对照，把故障从"内存系统"剥离到"CPU179 核内通路"，责任边界（换内存条 vs 换 CPU/降核使用）随之完全不同。这正是微架构级取证的价值：**把不可复现的静默故障，变成一次可审计的逻辑判定**。

### 9.2 芯片设计与实现启示（可落地）
1. **load path 端到端校验**：数据从 L1D/填充缓冲到寄存器写口的通路目前大多只有阵列 ECC/奇偶，互连与流水线锁存段无保护。建议对关键控制流/指针类装载结果增加奇偶或 CRC 覆盖（哪怕只保护 LSF→RF 写回段），使"装载塌缩为 0"从静默变成可中断的精确异常。
2. **percpu 基址访问的冗余编码**：`__per_cpu_offset[]` 是全核共享的调度命脉数组，单次读错即全核调度崩塌。可在 percpu 基址寄存器化（类似 ARM 的 TPIDR 模式）或对数组条目加端到端校验副本（如 offset 与 offset XOR canary 成对存放，读出后校验），把单点读错变成可检测错。
3. **假缺页的检测钩子**：本案 9 次 "spurious translation fault" 其实是硬件在 18 小时里持续报警，内核只能 WARN 后放行。建议 CPU 在 TLB/PTW 检测到"walk 结果与缓存属性矛盾"或重复假缺页时，置 architecture-specific 的 ERRSELR 记录（即可被 rasnode 类驱动读出），把"spurious"从一句 printk 变成一条 RAS 记录。
4. **fail-fast 与 silence 的权衡**：装载塌缩为 0 是最危险的静默形态（0 常是合法值）。对指针类装载（地址生成用）可在 LSU 内做"非零预期"标注（编译器侧:指针装载加非空断言，硬件侧:命令断言失败即取 EX），用极小的面积代价换取 SDC→检测错误的转化率。
5. **DFT/在线测试钩子**：建议在生产 SKU 保留 LSU/PTW 的 LBIST 慢速扫描入口（如低负载周期性自检），本案这类 18 小时 10 次的低频受扰，正适合在线微自检捕获。
6. **kdump 可靠性**：PARTIAL DUMP 丢页会削弱取证（本案 384 处 seek error）。调度器关键 percpu 数据（rq、__per_cpu_offset）建议纳入 kdump 的"必转储"白名单（kexec crashk 的 elfcorehdr 高优先段），保证此类案件寄存器-内存对账总是可行。

### 9.3 对系统软件/RAS 的启示
1. **把 "Ignoring spurious kernel translation fault" 升级为一等 RAS 事件**：按 CPU 号计数，超阈值（如 >3 次/核）自动触发核隔离与告警。本案该机制可在开机 24 分钟（W1）时就预警 CPU179，而不是 18 小时后等致命崩溃。
2. **调度器对 percpu 基址做断言**：`cpu_rq()` 展开处可加一次廉价校验（如 `if (unlikely((unsigned long)rq & ~KERNEL_ADDR_MASK)) BUG()` 或利用 gcc `__builtin_assume_aligned`/断言宏），把本案类零塌缩在第一时间转为受控崩溃，并带上更丰富的上下文。
3. **监控进程是最好的哨兵**：9 次前兆全部由读 /proc/interrupts 的监控守护（irqbalance/pmdalinux）踩中——它们天然是"内核读通路"的高频采样器，其 WARNING 应被 RAS 聚合器纳入硬件疑似事件流。
4. **RAS 覆盖空档**：rasnode 扫描显示架构 ERR 记录全空，说明此类核内通路受扰完全落在现行 RAS 架构的盲区。软件侧应承认"无 RAS 记录 ≠ 无硬件故障"，以行为签名（假缺页、装载塌缩、同核聚集）作为补充证据链。

## 10. 处置建议

1. **立即**：从 RAS 事件流角度复盘 CPU179 的 9 条 spurious fault 记录是否已有告警；若无，配置按核聚合的 spurious-fault 告警阈值。
2. **短期**：对该机器（localhost0102）执行 CPU179 隔离验证：`nohz_full`/cpuset 排除 179 后继续跑 movbe_l1d 压测 + irqbalance/pmdalinux 常态负载 24-48h，观察前兆是否归零（归零即坐实核内受扰）。
3. **中期**：在 gem5-fi 故障注入平台复现本案签名（load 返回置零注入于 `ldr x20,[x0,w25,sxtw#3]`），验证签名匹配度，为同型案件建立快速判据；同时评估给 percpu 基址读加内核侧断言的补丁。
4. **硬件侧**：若隔离验证坐实，安排该节点 CPU（socket 对应 node 7 所在物理 CPU）的替换或降级使用；同批次机器做同核位（node7 第 4 核）健康抽查。
5. **流程**：把"寄存器值 vs 转储内存真值对账"固化为所有 Oops 类转储的标准取证步骤（本案证明其能在 30 分钟内把根因从"调度器崩溃"收敛到"装载通路受扰"）。

---

## 附录：命令索引（全部可复核）

**dmesg 取证**（输出全文见 `dmesg_forensics.txt`）：
```
wc -l / head -8 / grep -n 'Kernel command line' / grep -n -E 'smp: Brought up|SMP: Total|Memory:|crashkernel' vmcore-dmesg.txt
sed -n '4140,4193p' vmcore-dmesg.txt                       # 致命 Oops 块
grep -n 'WARNING: CPU' / grep -n 'Ignoring spurious' vmcore-dmesg.txt
sed -n '2579,2600p;2624,2670p;2669,2712p;3870,3916p;3915,3961p;3960,4006p;4005,4051p;4050,4096p;4095,4140p' vmcore-dmesg.txt   # 9 个 WARNING 块
grep -n -E 'Machine check|Hardware error|EDAC.*error|memory failure|ECC' vmcore-dmesg.txt
grep -n 'rasnode: cpu=179' vmcore-dmesg.txt
```

**crash 取证**（namelist `/tmp/vmlinux-0102`；输出全文见 `crash_forensics.txt`，共 13 个会话，均 `timeout 590 crash ... -i <cmdfile>` 批处理）：
```
sys / panic / set / mach / bt / bt -t / bt -r / bt -l / bt -F     # 会话1
dis -l ffffa2930034ae44 / dis -l ffffa2930034ae24 / dis -l ffffa2930034ae10
kmem -s ; ps | head -60 ; runq ; dev -p ; net
vtop ffffa29301d797e0 / vtop ffffa29301d796c0                     # 会话2
p/x __per_cpu_offset[179] / px &runqueues
rd ffffa293021796c0 16 / rd ffffa2930216fcb0 40 / rd ffffa293021838c0 16
p/x pcpu_base_addr ; p pcpu_nr_units ; p/x pcpu_first_chunk      # 会话3
rd ffff8000813e14d8 4 / rd ffff8000813e14f8 8 / rd ffff8000813e1488 24   # 会话4
search -t 256855 ffffdd6d7fa64000 / search -t 256855 ffffa29301d797e0    # 会话8
vtop ffffdd6d7fa64000 / vtop ffffa29301ba4000 / kmem ffffa2930216f000 / kmem ffffa29302170000  # 会话6
rd -p 403593d796c0 16 / rd -p 40359416fcb0 8 / rd -p 403594175000 16 / rd -p 60575f8cfb40 8   # 会话7
rd -p 403594175b60 8 / rd ffffa29302175b60 8 / rd ffffa293021755d0 48   # 会话9 ★__per_cpu_offset 真值
vtop ffff6040082574a8 / vtop ffff6040088433d7 / vtop ffff6040074a8369   # 会话10 ★前兆假缺页证明
ps -c 179 / ps | grep -E 'mi-scavenger|movbe_l1d' / task -R pid,comm,cpu ffff0020364f3f00 / files ffff0020364f3f00  # 会话11
rd ffffa29301d796c0 16 / vtop ffffa29301d79000 / vtop ffffa29301d69000 / kmem ffffa29301d79000  # 会话12
vtop ffff8000817dd6c0 / vtop ffff8000817dd7e0 / rd ffff8000817dd6c0 4 / rd ffff8000817dd7e0 4  # 会话13 ★反事实验证
```

**反汇编与符号**：
```
nm /tmp/vmlinux-0102 | grep -E ' (T|t) find_busiest_group$'          # ffff80008013ad08
nm /tmp/vmlinux-0102 | grep -E ' (D|d) (runqueues|__per_cpu_offset|cpu_worker_pools)$'
objdump -d --start-address=0xffff80008013ad08 --stop-address=0xffff80008013ae60 /tmp/vmlinux-0102
objdump -d --start-address=0xffff800080137ef0 --stop-address=0xffff800080137f48 /tmp/vmlinux-0102
```

**代数复算**：`algebra.py` → `algebra_out.txt`（KASLR slide、闭合等式、反事实、时间线、ESR 解码、NUMA 归属，全部 mod 2^64）。

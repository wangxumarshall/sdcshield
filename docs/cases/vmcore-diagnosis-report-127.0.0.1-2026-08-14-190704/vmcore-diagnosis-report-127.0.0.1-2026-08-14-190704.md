# CPU179 转储深度诊断报告（第 1 次独立重研究）

## ——12 次同核前兆假 fault + 一次"装载寄存器值与内存真值海明距离 35 位"的 load 通路 SDC，x20 读出值恰为 `__per_cpu_offset` 数组头部数据的 16bit lane 循环重组

| 项目 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-14-19:07:04/vmcore`（Kdump compressed v6, PARTIAL DUMP, 11.6GB） |
| dmesg | 同目录 `vmcore-dmesg.txt`（3174 行） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026 |
| CPU | Kunpeng-920 (TaiShan-v110, MIDR 0x481fd010), 192 核, 8 NUMA 节点, 每节点 24 核 |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP（KASLR 开启, crashkernel=1024M,high） |
| 内存 | 768GB（791048812K/805102592K available） |
| 崩溃时刻 | 2026-08-14 19:06:15 CST（uptime 113997.28s = 1 天 7 小时 39 分 57 秒；开机约 2026-08-13 11:26:18） |
| 受害对象 | CPU 179, PID 1986 `kworker/179:1H`（kblockd 高优先级工作队列）, 调度器 `find_busiest_group+0x140` |
| 前兆 | 12 次 `Ignoring spurious kernel translation fault` WARNING（104485.84s–113979.67s，2.64 小时窗口），**全部 CPU 179** |
| 结论一行 | 【强推】CPU179 的 load/PTW 数据通路存在持续性随机扰动（SDC 源），致命一击是 `ldr x20,[x0,x25,sxtw#3]` 从 `__per_cpu_offset[176]`（真值 0xffffd937172de000，物理页有效）读出 0xd93715ba0000ffff——非内存任何单槽值，恰等于数组头部 idx0/idx1 槽边界错位 2 字节的 8 字节窗口（16bit lane 循环移位），35/64 位翻转，属装载通路数据重组级腐化，非内存位翻转 |

## 1. 执行摘要

1. **现象**：满载位操作压力测试（191/192 核跑 `movbe`，load average 197）运行 31.6 小时后，CPU179 上 `kworker/179:1H` 在 `find_busiest_group+0x140`（`ldr x23,[x27,#288]`）对非规范虚拟地址 `0x0036bc836a4a97df` 触发 level 0 translation fault，Oops 致命。
2. **崩溃签名**：ESR=0x96000004（DABT, current EL, FSC=0x04 level 0 fault），`address between user and kernel address ranges`；x27=0xd936bc836a4a96bf 由 `add x27,x1,x20` 生成，其中 x20=0xd93715ba0000ffff 为坏值源头。
3. **闭合验证【实锤】**：Python3 mod 2^64 复算：x1+x20 ≡ x27（精确成立）；x27+288 ≡ 0xd936bc836a4a97df，其低 48 位与 FAR 完全一致（bit63-48=0xd936 非全 0/全 1 → 必然 level 0 fault）。
4. **内存真值对照【实锤】**：出错的 `ldr x20,[x0,x25,sxtw#3]` 的取数地址为 `&__per_cpu_offset[176]` = 0xffffa6c96a895b50（vtop → 物理页 602f89095000，有效 reserved 页），crash 直接读该槽真值为 0xffffd937172de000——**内存没坏，装载结果错**；x20 实际值与真值海明距离 35 bit，且精确等于数组头部 `[idx0+6, idx0+14)` 错位 2 字节窗口的值（等价于对 `__per_cpu_offset[1]` 做 16bit lane 循环左移）。
5. **前兆链【实锤】**：致命崩溃前 2.64 小时内，同一 CPU179 上发生 12 次对**有效内核线性映射地址**（vtop 证实 PUD 1GB 块存在，如 ffff604003e5f8a0→604003e5f8a0）的 level 0 假 fault，受害进程为 pmdalinux/irqbalance/memcpy1/control，路径全部是 cpumask/中断统计的**位图批量读**（`show_interrupts`、`_find_next_and_bit`）；最后一次距致命崩溃仅 17.6 秒，且假 fault 地址 ffff604003e52218/ffff604003e547b8 与崩溃现场 x22=x26=0xffff604003e5f8a0（`sd->groups` 调度组对象）同处 64KB 区域。
6. **根因判定与置信**：【强推】（多源收敛但无法在软件层直接观测数据通路瞬态）CPU179 核内 **load 数据通路（L1D → 修复逻辑 → 寄存器文件写端口，含 PTW 返回通路）存在间歇性位错位/lane 重组级扰动**；12 次前兆（PGD 级读出被腐化的瞬时假 fault）+ 1 次致命（64 位装载值被 16bit 粒度错位重组）在同一核上收敛。排除软件成因：该指令序列是调度器亿分之一秒级热路径，192 核中唯独 CPU179 出错，软件 bug 不挑核。RAS 负证据：dmesg 无任何 Machine check/Hardware error/EDAC ECC 事件（ghes_edac 已注册但零报告）——**静默数据腐化（SDC）**，硬件未检出。
7. **处置**：隔离/下线 CPU179（`maxcpus` 或 CPU hotplug offline），收集 RAS/CEC 日志复查，同批次机器排查；本机不宜继续承载生产负载。

## 2. 证据规则与方法

- **证据源**：仅使用 `vmcore-dmesg.txt`（原始 3174 行）与 crash 8.0.4-17.oe2403sp4 加载 `vmcore`（namelist `/tmp/vmlinux-0102`, BuildID 276194e5f356f9c4bc570bb0a750394d7b768035, 带 debug_info）。未阅读任何既有诊断文档。
- **诚实铁律**：报告所有数据引用均来自实际命令的真实输出，全部命令与关键输出存 `dmesg_forensics.txt`（dmesg 部分 F1–F8，crash 部分 C1–C18）。
- **64 位运算**：全部用 Python3 mod 2^64 脚本 `algebra.py` 计算，输出存 `algebra_out.txt`。
- **三级置信**：【实锤】可直接复核（如寄存器闭合等式、内存真值读取）；【强推】多源收敛但缺直接对照（微架构通路定位）；【假设】无法软件验证，给出验证途径。
- **工具清单**：crash 8.0.4（`sys/bt/bt -t/dis -l/rd/rd -p/vtop/p/px/sym/struct/runq/ps/timer/dev -p/net/kmem -s`），grep/awk/sed，Python3。
- **已知局限**：vmcore 为 PARTIAL DUMP，加载时 384 个 seek error（均为 IRQ/SDEI stack 未转储，不影响本分析对象）；kmem -s 正常完成。

## 3. 本次开机时间线【时间线】

| uptime(s) | 墙钟 (CST) | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0 | 2026-08-13 11:26:18 | 开机（由 panic 时刻 − uptime 反推） | 1 | 实锤 |
| 27.36 | 08-13 11:26:46 | EXT4 根文件系统挂载完成 | 2582 | 实锤 |
| 43.49 | 08-13 11:27:02 | 最后一条常规启动日志 | 2584 | 实锤 |
| 81742.68 | 08-14 10:10:40 | dnf 32-bit capability 警告（唯一非前兆异常） | 2586 | 实锤 |
| 104485.84 | 08-14 16:27:44 | **前兆 #1**：pmdalinux 读 /proc/interrupts 假 fault @ffff6040060997d6 | 2588–2631 | 实锤 |
| 104976.08 | 08-14 16:35:53 | 前兆 #2：irqbalance 假 fault @ffff6040080fc818 | 2633–2677 | 实锤 |
| 106219.05 | 08-14 16:56:36 | 前兆 #3：irqbalance @ffff6040eb922450 | 2678–2722 | 实锤 |
| 111245.74 | 08-14 18:20:23 | 前兆 #4：pmdalinux @ffff60408e264558 | 2723–2767 | 实锤 |
| 111299.04 | 08-14 18:21:16 | 前兆 #5：irqbalance @ffff60408e265768 | 2768–2812 | 实锤 |
| 111735.72 | 08-14 18:28:33 | 前兆 #6：pmdalinux @ffff60408e26761e | 2813–2857 | 实锤 |
| 111849.06 | 08-14 18:30:26 | 前兆 #7：irqbalance @ffff60408e2673e2 | 2858–2902 | 实锤 |
| 113245.73 | 08-14 18:57:23 | 前兆 #8：pmdalinux @ffff6040ffbc5608 | 2903–2947 | 实锤 |
| 113435.80 | 08-14 19:00:33 | 前兆 #9：pmdalinux @ffff604017fdba1c | 2948–2993 | 实锤 |
| 113745.79 | 08-14 19:02:03 | 前兆 #10：pmdalinux @ffff60408e2673a0 | 2994–3038 | 实锤 |
| 113810.97 | 08-14 19:03:08 | **前兆 #11**：memcpy1 在 `_find_next_and_bit`（load_balance 路径）假 fault @ffff604003e52218 | 3039–3085 | 实锤 |
| 113979.67 | 08-14 19:05:57 | **前兆 #12**：control 在 `_find_next_and_bit`（select_task_rq_fair 路径）假 fault @ffff604003e547b8，**距致命崩溃 17.6 秒** | 3086–3127 | 实锤 |
| 113997.28 | 2026-08-14 19:06:15 | **致命 Oops**：kworker/179:1H, find_busiest_group+0x140, FAR=0036bc836a4a97df | 3128–3174 | 实锤 |
| 113997.66 | 19:06:15 | SMP: stopping secondary CPUs; Starting crashdump kernel | 3172–3173 | 实锤 |
| — | 19:07:04 | kdump 落盘完成（目录时间戳），与 panic 间隔 49 秒，转储耗时合理 | 目录名 | 实锤 |

墙钟推算方法：crash `sys` 的 DUMP DATE（2026-08-14 19:06:15 CST，即 panic 时刻）减去 uptime 差值线性外推（Python3，algebra_out.txt [3][5]），与目录名 19:07:04（kdump 完成）自洽。

前兆窗口总长 9511.44s ≈ 2.64 小时；12 次全部 CPU 179；受害进程分布：pmdalinux×6、irqbalance×4、memcpy1×1、control×1。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 3128–3174 行）

```
[113997.282188] Unable to handle kernel paging request at virtual address 0036bc836a4a97df
[113997.290923] Mem abort info:
[113997.294504]   ESR = 0x0000000096000004
[113997.299043]   EC = 0x25: DABT (current EL), IL = 32 bits
[113997.305145]   SET = 0, FnV = 0
[113997.308986]   EA = 0, S1PTW = 0
[113997.312915]   FSC = 0x04: level 0 translation fault
[113997.318582] Data abort info:
[113997.322250]   ISV = 0, ISS = 0x00000004, ISS2 = 0x00000000
[113997.328526]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[113997.334366]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[113997.340469] [0036bc836a4a97df] address between user and kernel address ranges
[113997.348398] Internal error: Oops: 0000000096000004 [#1] SMP
[113997.455264] CPU: 179 PID: 1986 Comm: kworker/179:1H Kdump: loaded Tainted: G        W           6.6.0-145.3.23.154.oe2403sp3.aarch64 #1
[113997.476774] Workqueue:  0x0 (kblockd)
[113997.481231] pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[113997.488986] pc : find_busiest_group+0x140/0xb60
[113997.494310] lr : find_busiest_group+0x11c/0xb60
[113997.499632] sp : ffff8000b722b960
[113997.637500] Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

ESR 解码（Python3，algebra_out.txt [6]）：EC=0x25（Data Abort, current EL）、ISV=0（ISS 不含寄存器信息，因缺页指令为变址寻址）、WnR=0（读）、FSC=0x04（level 0 translation fault，即 PGD 第一级查找就失败）。与前兆 ESR=0x96000044 的 FSC 相同（同型 level 0 fault）。

### 4.2 全量寄存器（x0–x30）

```
x29: ffff8000b722bae0   x28: ffff8000b722ba70   x27: d936bc836a4a96bf
x26: ffff604003e5f8a0   x25: 00000000000000b0   x24: ffffa6c96a895000
x23: 0000000000000400   x22: ffff604003e5f8a0   x21: ffffa6c96a88fcb0
x20: d93715ba0000ffff   x19: ffff8000b722bb70   x18: 0000000000000000
x17: 0000000000000000   x16: ffffa6c96937e9f0   x15: 0000000000000040
x14: 0000000000000000   x13: 0000000000000038   x12: 0101010101010101
x11: 7f7f7f7f7f7f7f7f   x10: 0000000000000000   x9 : ffffa6c968a6ae58
x8 : ffff8000b722bac8   x7 : 0000000000000000   x6 : 00000000000000b0
x5 : ffff000000000000   x4 : 0000000000000002   x3 : 0000000000000030
x2 : 0000000000002001   x1 : ffffa6c96a4996c0   x0 : 00000000000000b0
```

关键寄存器语义（由反汇编确定）：
- x19 = lb_env 指针（栈 ffff8000b722bb70，`struct lb_env` 崩溃后仍完整：sd=0xffff604004398c00, dst_cpu=0xb3=179, dst_rq=0xffff8000817dd6c0, idle=CPU_NEWLY_IDLE）；
- x1 = 0xffffa6c96a4996c0 = `runqueues`（percpu 符号，crash `sym` 实测）；
- x0 = 0xb0（176，`mov w0,w25` 复制）；x25 = 0xb0 = 176（`_find_next_and_bit` 返回的下一个候选 CPU）；
- x20 = 0xd93715ba0000ffff（**坏值，本案核心证据**）；
- x27 = 0xd936bc836a4a96bf（`add x27,x1,x20` 的错误结果）；
- x22 = x26 = 0xffff604003e5f8a0（`env->sd->groups` 调度组链表头）。

### 4.3 Call trace（完整）

```
find_busiest_group+0x140/0xb60
load_balance+0x108/0x6c0
newidle_balance+0x198/0x510
pick_next_task_fair+0x110/0x718
pick_next_task+0x60/0x398
__schedule+0x1b4/0x8a0
schedule+0x58/0x130
worker_thread+0x1a8/0x360
kthread+0xec/0x100
ret_from_fork+0x10/0x20
```

crash `bt` 与之一致（含异常入口帧 el1h_64_sync → do_mem_abort → do_translation_fault → do_bad_area → __do_kernel_fault → die）。

### 4.4 前兆异常原文（节选，完整见 dmesg_forensics.txt F4/F5）

首个前兆（104485.84s，pmdalinux 读 /proc/interrupts）：

```
[104485.842917] ------------[ cut here ]------------
[104485.842925] Ignoring spurious kernel translation fault at virtual address ffff6040060997d6
[104485.842931] WARNING: CPU: 179 PID: 10359 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[104485.843072] CPU: 179 PID: 10359 Comm: pmdalinux Kdump: loaded Tainted: G        W
...
[104485.843152]  __memcpy+0x80/0x240
[104485.843157]  seq_printf+0xc4/0xe8
[104485.843162]  show_interrupts+0x1d4/0x498
```

倒数第二个前兆（113810.97s，memcpy1，已进入调度器负载均衡路径）：

```
[113810.970838] ------------[ cut here ]------------
[113810.970846] Ignoring spurious kernel translation fault at virtual address ffff604003e52218
[113810.970853] WARNING: CPU: 179 PID: 2606819 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[113810.970993] CPU: 179 PID: 2606819 Comm: memcpy1 Kdump: loaded Tainted: G        W
...
[113810.971073]  _find_next_and_bit+0x18/0x80
[113810.971079]  load_balance+0x108/0x6c0
[113810.971083]  newidle_balance+0x198/0x510
```

最后一个前兆（113979.67s，control，距致命崩溃 17.6 秒）：

```
[113979.669790] ------------[ cut here ]------------
[113979.669798] Ignoring spurious kernel translation fault at virtual address ffff604003e547b8
[113979.669805] WARNING: CPU: 179 PID: 2823806 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[113979.669949] CPU: 179 PID: 2823806 Comm: control Kdump: loaded Tainted: G        W
...
[113979.670034]  _find_next_and_bit+0x18/0x80
[113979.670040]  select_task_rq_fair+0x44c/0x590
[113979.670045]  wake_up_new_task+0x354/0x440
[113979.670053]  kernel_clone+0x1b4/0x3a8
[113979.670055]  __arm64_sys_clone3+0x1c/0x30
```

前兆 #10–#12 的三条地址（ffff604003e52218、ffff604003e547b8）与崩溃现场 x22/x26=0xffff604003e5f8a0 同处 `ffff6040_03e5xxxx` 64KB 区域——这正是调度域结构体（sched_group/sched_domain，slab 分配）所在的物理页范围（node 6 线性映射区）。

### 4.5 RAS 负证据（dmesg grep）

- `Machine check`：0 次；`Hardware error`：0 次；`memory failure`：0 次；用户态 `segfault`：0 次。
- `EDAC` 相关仅有启动注册行：`EDAC MC: Ver: 3.0.0`、`ghes_edac: This system has 32 DIMM sockets.`（行 1857/2175/2176）——ghes_edac 已就位但整个 31.6 小时运行期**零错误上报**。
- `CPU features: detected: RAS Extension Support`（行 1262）说明 CPU 具备 RAS 扩展，但本次错误未被任何硬件错误检测机制捕获——这是**未被检测的静默数据腐化（SDC）**的直接证据形态。
- 前兆 12 次全部被内核软件层（`__do_kernel_fault` 的 spurious fault 判定）捕获为 WARNING 而非硬件异常。

## 5. 业务现象（受害进程是什么业务、调度上下文解读）

- **崩溃线程**：`kworker/179:1H`（PID 1986）是 CPU179 的**高优先级（HIGHPRI）kblockd 工作队列**内核线程，负责块设备 IO 相关工作。崩溃发生在它 `schedule()` 让出 CPU 时、`newidle_balance`（CPU_NEWLY_IDLE 负载均衡）扫描候选 busiest group 的过程中——纯内核态调度器热路径，与块设备业务逻辑本身无关（bt 证实 worker_thread→schedule→pick_next_task_fair 链）。
- **机器负载**：这是一台**满载位操作压力测试机**：runq 显示 191/192 个 CPU 的 current 均为 `movbe` 进程（PID 2398064–2398259），load average 197.64/198.15/185.05；前兆进程 `memcpy1`（内存拷贝压测）与 `control`（压测控制进程，正在 `clone3` 派生新任务）表明压测框架持续 fork。`movbe`/`memcpy1`/`control` 组合说明用户在做 x86 movbe 指令仿真/位操作吞吐压测。
- **前兆受害进程**：pmdalinux（PCP 性能采集守护进程，周期读 /proc/interrupts）×6、irqbalance（中断亲和性平衡，同样周期读 /proc/interrupts）×4——两者都是**周期性读取内核中断统计位图**的轻负载进程；它们恰好在 CPU179 上运行并触碰被扰动的读通路时才报警。
- **调度上下文共性**：12 次前兆中 10 次在用户态 syscall 读路径（`vfs_read→seq_read_iter→show_interrupts→__memcpy`），2 次在调度器内核路径（`_find_next_and_bit`）；致命 1 次在调度器内核路径。共同点：全部是**大跨度位图/数组批量读**（中断统计 percpu 数组、cpumask 位图、`__per_cpu_offset` 数组）。

## 6. 诊断定位过程【诊断定位过程】

### P1 · dmesg 勘察

grep 全部 `cut here/WARNING/Oops` 得到 12 次前兆 + 1 次致命 Oops；确认前兆全部 CPU179、全部 `__do_kernel_fault` 同一判定点、FSC 同型（level 0）；RAS 关键词零命中。提取开机指纹（内核版本、192 CPU、8 节点、768GB、crashkernel）。

### P2 · 崩溃块提取

提取 ESR/EC/FSC、全量寄存器、Call trace、Code 窗口。Code 窗口 5 条指令中 `(f9409377)` 为缺页指令（`ldr x23,[x27,#288]`），其前两条为 `f879d814`（`ldr x20,[x0,x25,sxtw#3]`）与 `8b14003b`（`add x27,x1,x20`）——立即锁定 x20 是坏值源头。

### P3 · crash 加载与反汇编

crash 用 `/tmp/vmlinux-0102` 加载成功（首次加载 22.4s）。`dis -l find_busiest_group` 精确定位：
- `+0x13c`（fair.c:12050）：`ldr x20, [x0, w25, sxtw #3]` ← **x20 的唯一来源**
- `+0x140`（fair.c:12050）：`add x27, x1, x20`
- `+0x144`（fair.c:5024）：`ldr x23, [x27, #288]` ← 缺页
源码行 fair.c:12049–12050 是 `for_each_cpu_and(cpu, cpumask_of_node(node), sched_group_span(group))` 循环内对 `cpu_rq(cpu)` 的计算（per_cpu_ptr(runqueues, cpu)）。

### P4 · 内存真值对照（区分"装载被腐化"vs"内存被写坏"）

1. 栈上数据（`rd ffff8000b722b960 64`）确认：[sp+8]=0xffffa6c96a8955d0=`&__per_cpu_offset[0]`（x0 来源，`ldp x0,x1,[sp,#8]`），[sp+0x10]=0xffffa6c96a4996c0=`runqueues`（x1 来源）。
2. `sym` 确认 0xffffa6c96a4996c0=`runqueues`、0xffffa6c96a8955d0=`__per_cpu_offset`（均为 percpu 符号）。
3. load 语义：`x20 = __per_cpu_offset[176]`（w25=176，sxtw#3 即 ×8）。取数地址 0xffffa6c96a895b50。
4. `vtop 0xffffa6c96a895b50`：PGD→PUD→PMD→PTE 全部有效，物理页 602f89095000（reserved 内核数据页）。**地址合法、页表映射存在**。
5. 直接读真值：`rd -p 602f89095b40 8` → 槽 176 = **0xffffd937172de000**（内存真值，且与相邻槽 174/175/177/178 呈完美等差 0x22000，数组本身完好）。
6. 对照：x20 崩溃值 0xd93715ba0000ffff ≠ 真值；海明距离 35 bit；**真值不可能经任何单 bit 翻转、单字节损坏变成 x20**（Python3 逐位验证）。
7. **惊人闭合**（Python3 穷举验证）：x20 精确等于对数组头部 `[idx0+6, idx0+14)` 这个**错位 2 字节的 8 字节窗口**的读取值——idx0=0xffffd93715b7e000 与 idx1=0xffffd93715ba0000 在内存中相邻，跨槽错位窗口的 8 字节恰为 `ff ff 00 00 ba 15 37 d9`（小端）= 0xd93715ba0000ffff = x20。等价表述：x20 = `rol64(__per_cpu_offset[1], 16)`（64 位 16bit-lane 循环左移一段）。
8. **反事实推演**：若 x20 = 真值 0xffffd937172de000，则 x27 = x1+x20 = 0xffff8000817776c0（Python3 计算）——这是 CPU176 的 rq 地址形态（与 CPU179 的 0xffff8000817dd6c0 同构，vtop 确认该区间为有效映射），`ldr x23,[x27,#288]` 正常执行，**不会崩溃**。故崩溃的唯一充分原因是 x20 装载值错。

### P5 · 软件成因排除

1. `find_busiest_group` 是调度器最热路径之一，满载下每核每秒执行成百上千次；31.6 小时满载中 192 核只有 CPU179 出这 13 次错（12 前兆 + 1 致命）——软件 bug 不会挑核。
2. x20 的值在架构上只能来自 `__per_cpu_offset[176]` 单一内存槽（指令语义唯一确定），而内存真值正确且数组连续完好（等差 0x22000 无一异常）——排除"内存被写坏"。
3. 该内核为 openEuler 2403sp3 官方构建，`__per_cpu_offset` 初始化后只读；且 12 次前兆横跨 5 个不同用户进程 + 2 条内核路径，无共同软件代码路径（除"都是批量读"这一数据形态）。
4. 前兆的假 fault 对象（ffff6040...）经 vtop 证实 PUD 1GB 块映射存在——页表本身没坏，是 PTW 瞬时读错。

### P6 · 定位收敛

- 前兆簇（12 次，2.64h，全 CPU179）：对有效内核地址的 level-0 假 fault = PTW 第一级读出被瞬时腐化，或 TLB/PGD 条目读出受扰。
- 致命一击（1 次，CPU179）：load 数据通路把 `__per_cpu_offset[176]`（0xffffd937172de000）错误地变成了"数组头部数据的 16bit lane 循环重组值"（0xd93715ba0000ffff）。
- 两组错误在同一核、同一时间窗、同一数据形态（64 位字/位图批量读）上收敛 → 单核核内数据通路（load path 含 PTW 返回路径）间歇性扰动，SDC 根因。

## 7. 逻辑链条

**指令语义链【实锤】**（全部经 `dis -l` 与 Code 窗口机器码复核）：

```
f879d814  ldr x20, [x0, x25, sxtw #3]   ; x0=&__per_cpu_offset[0], x25=176
                                          ; → load 地址 = 0xffffa6c96a895b50 (架构正确, 物理页有效)
2a1903e0  mov w0, w25                     ; w0=176
8b14003b  add x27, x1, x20                ; x1=runqueues=0xffffa6c96a4996c0
f9409377  ldr x23, [x27, #288]            ; 有效地址 = x27+288 → 非规范 VA → level 0 fault
```

**闭合等式【实锤】**（Python3 mod 2^64，algebra_out.txt [1][7][8]）：

1. `x1 + x20 ≡ x27`：0xffffa6c96a4996c0 + 0xd93715ba0000ffff ≡ 0xd936bc836a4a96bf ✓（Oops 寄存器自洽）
2. `x27 + 288 ≡ 0xd936bc836a4a97df`，其低 48 位与 FAR（0x0036bc836a4a97df）完全一致 ✓
3. `FAR` 高 16 位 0x0036 与有效地址高 16 位 0xd936 差异为 VA[63:48] 的非规范位（d936 既非全 0 也非全 1）→ "address between user and kernel address ranges" + FSC=0x04 完全自洽 ✓
4. **真值等式**：`load(__per_cpu_offset[176]) 应为 0xffffd937172de000`（crash `rd -p` 直接实测），而 `x20 = 0xd93715ba0000ffff = rol64(__per_cpu_offset[1], 16) = 错位窗口 [idx0+6, idx0+14)` ✓（Python3 字节流穷举验证）

**反事实推演**：若 x20 = 真值，x27 = 0xffff8000817776c0（CPU176 rq，有效映射），后续指令全部正常 → 不崩溃。若 x20 是真值的任何单 bit 翻转（1–35 位中任何子集为 1bit），x27 仍为 ffff8/ffff9 开头的规范内核地址或轻微越界，多数情形不产生 level-0 fault——**只有"高位模式被整体替换"（0xffffd937→0xd93715ba）才必然产生本案的非规范地址形态**。

**前兆同源性**：前兆 #11/#12 与致命崩溃在**同一条调度器代码路径**（`_find_next_and_bit` ↔ `find_busiest_group` 的 `for_each_cpu_and`）、**同一批数据对象**（ffff604003e5xxxx 调度组 slab 区 + percpu 数组）、**同一 CPU 179**、最后间隔仅 17.6 秒——三重收敛。

**诚实声明**：
- 我无法在软件层直接观测 load 通路的瞬态数据流，"16bit lane 重组"是从输出值与内存字节的精确匹配逆推的形态学结论，不是对硬件内部信号的观测【强推】。
- "x20 == 数组头错位读" 存在另一种等价描述（rol64(idx1,16)），两者数学等价；它更可能反映的是**数据通路 byte-lane 选择/移位错误**而非真的发生了跨槽访问（地址侧 x25=176 与 x24 基址都正确）。
- x20 的高 32 位 0xd93715ba 与 `__per_cpu_offset[1]` 的 [47:32] 位段完全同源（15ba 是 CPU1 槽位特征值，全数组唯一），这排除了随机噪声：错误数据**真实取自数组头部**,而非凭空产生【实锤】。
- PARTIAL DUMP 未包含 IRQ/SDEI 栈（384 个 seek error），但这些区域与本案分析对象无关。

## 8. 故障根因（微架构级结论 + 置信级别）

**根因【强推】**：CPU179 核内 load 数据通路（覆盖 L1D 命中返回路径与 PTW 页表走查读出路径）存在间歇性、无报错的位级数据扰动（SDC）。证据形态分级：

1. **装载结果塌缩重组**【实锤（现象）→ 强推（通路归属）】：`ldr x20,[x0,x25,sxtw#3]` 从物理有效、内存真值完好的槽位读出与真值海明距离 35 bit 的重组值，且该值可精确溯源到数组头部相邻槽数据的 16bit lane 循环错位——错误的比特组织方式（lane 级重组）指向数据通路的复用器/移位逻辑/寄存器文件写端口受扰，而非存储单元翻转。
2. **PTW/页表读出瞬时腐化**【强推】：12 次对有效线性映射地址的 level-0 假 fault（PGD 级"无映射"判定），同核 2.64 小时复发，页表内存本身完好。
3. **排除项**：内存介质故障（真值完好、等差完好）；纯软件 bug（挑核 + 挑数据形态）；NUMA/互联（错误全部核内 localized 于 CPU179，未跨核）；RAS 已检出错误（零硬件报错，恰证明"静默"）。

置信级别：**强推**（多源收敛：同核 13 次、时间窗集中、数据形态统一、真值对照闭合、反事实成立；缺直接对照的部分：无法区分扰动发生在 L1D→RF 通路、PTW→TLB 返回通路、还是寄存器文件保持阵列——软件不可见）。

**微架构定位**：最可能在 load 管线的数据返回段（fill buffer / L1D 输出到目的寄存器的 byte-lane 选通），PTW 假 fault 可统一解释为 PTW 读页表项也走同一受扰通路（页表读出是特殊的 load）。若为单点物理缺陷（如某一 lane 的 mux/锁存器供电或时序边际），则在满载高温/高压频率下复发率上升的现象（29 小时空闲→满载后前兆爆发）也吻合。

## 9. 启示

### 9.1 微架构定位的意义

本案展示了"寄存器值 vs 内存真值对照法"在 SDC 根因隔离中的决定性作用：仅凭 Oops 寄存器只能看到崩溃，只有把 `ldr` 的取数地址重放（vtop + rd -p）拿到真值，才能把错误从"内存坏"改判为"通路坏"。16bit lane 重组这一形态学签名比"位翻转计数"更有定位价值——它直接指认数据通路的组织级错误而非存储级错误。同时 12 次前兆是免费的"探针"：spurious fault WARNING 本质上是被内核软件层捕获的 PTW 通路 SDC，任何 SDC 根因研究都应先穷尽这类前兆。

### 9.2 芯片设计与实现启示（具体、可落地）

1. **load 返回通路 ECC/parity 覆盖**：数据从 L1D/fill buffer 到寄存器文件写端口的这一段目前普遍无校验。建议对 64 位返回总线加 lane 级 parity（或 end-to-end data checksum，由 L1D 出口计算、寄存器写口校验），把本案这类"读出重组"从 SDC 变成可检出的 CE。
2. **PTW 输出校验**：页表项读出建议加 parity/ECC 或双读比对（尤其 PGD/PUD 条目），level-0 假 fault 即可转为精确的硬件自诊断事件，而不是被内核当"spurious"吞掉。
3. **关键调度数据结构冗余校验**：`__per_cpu_offset`、`runqueues` 等 percpu 指针数组是全内核最热的间接寻址根。可考虑对这些只读指针表做影子副本 + 周期 CRC 比对（软件侧），或设计上给 percpu 基址计算加一致性断言（如 x20 高 16 位必须等于 0xffff 的 cheap check 一条 AND/CCMP 即可，编译器可插桩）。
4. **错误传播屏障与 fail-fast 权衡**：本案坏指针幸运地落在非规范 VA 区间被 MMU 拦截（fail-visible）；若 x20 错成另一个**合法** percpu 基址，负载均衡会静默读错 CPU 的 rq 并据此迁移任务——产生真正的无声数据破坏。设计上非规范地址拦截（VA hole 检查）是天然屏障，建议对内核关键基址寄存器尽量保持"高位全 1"的规范形态以最大化该屏障的捕获率。
5. **DFT/在线测试钩子**：面向压测场景（本机 191 核 movbe 满载）提供 per-core 的 load 通路 BIST（周期性读已知图案并比对），可在 RAS 之外的软件层发现单核通路劣化；本案"前兆仅出现在做批量读的进程"说明随机业务负载本身就是探测器，缺的是系统性比对。
6. **kdump 可靠性设计**：PARTIAL DUMP 丢 IRQ/SDEI 栈不影响本类分析（关键证据在通用栈与全局数据），但建议 kdump 优先级保证 percpu 区、页表、slab 元数据的完整转储——这三者是 SDC 法证的黄金三角。

### 9.3 对系统软件/RAS 的启示

1. `Ignoring spurious kernel translation fault` 不是无害噪音：它是对**有效映射地址**的 PGD 级读错，同核多次复发即应触发内核级别的"核健康度降级"动作（rate-limit 之上加 per-CPU 计数与告警，甚至自动 offline）。本案 12 次前兆后 17.6 秒发生致命崩溃——前兆是明确的预警窗口，被浪费了。
2. ghes_edac 零报告 ≠ 无错。对 load 通路级 SDC，软件 RAS 唯一的抓手是"读出值合法性断言"：调度器/内存管理里对基址寄存器做 cheap invariant check（一条指令）能把大量 SDC 转成 WARN，换来可诊断性。
3. openEuler 内核可考虑提供 per-CPU spurious fault 计数 sysfs 节点 + 自动隔离策略（类似已 corrupted bitmap 机制），把"前兆→致命"的平均间隔（本案 2.64 小时/9511 秒）转化为处置时间窗。

## 10. 处置建议

1. **立即**：本机不要继续作为压测/生产基线；若需复现取证，保持满载 movbe 压测并开启前兆监控（`grep "Ignoring spurious"` 计数 + per-CPU 归属）。
2. **短期**：CPU hotplug 下线 CPU179（`echo 0 > /sys/devices/system/cpu/cpu179/online`）或启动参数规避，验证前兆是否完全消失（预期消失即最终确认单核缺陷）。
3. **中期**：采集 BMC/PMC 侧 CPU179 所在_die_的温度、电压、CEC 记录做交叉比对；若为单点缺陷按 RMA 流程换 CPU；若同批次多台机器有同形态（同核前兆簇），升级为批次性问题排查。
4. **工程**：给压测框架加"前兆即中止"的熔断（本案若有，可在 16:27 就止损，避免 2.6 小时后的崩溃和一次计划外 kdump）。

## 附录：命令索引（全部可复核，完整输出见 dmesg_forensics.txt / algebra_out.txt）

dmesg 侧：
```
grep -n -E 'Booting Linux|Linux version|Kernel command line|Memory:|crashkernel|Brought up' vmcore-dmesg.txt
sed -n '3128,3174p' vmcore-dmesg.txt            # 致命块
sed -n '2588,2632p' vmcore-dmesg.txt            # 前兆#1
sed -n '3039,3086p' vmcore-dmesg.txt            # 前兆#11
sed -n '3086,3128p' vmcore-dmesg.txt            # 前兆#12
grep -A6 'Ignoring spurious' vmcore-dmesg.txt | grep -oE 'CPU: [0-9]+ PID: [0-9]+ Comm: [a-zA-Z0-9_:-]+' | sort | uniq -c
for kw in 'Machine check' 'Hardware error' 'memory failure'; do grep -c -i "$kw" vmcore-dmesg.txt; done
grep -n -iE 'EDAC|ECC|ras' vmcore-dmesg.txt | head -8
```

crash 侧（namelist 一律 /tmp/vmlinux-0102，timeout 590，-i 批处理）：
```
crash /tmp/vmlinux-0102 <vmcore> -i cmdfile   # sys/panic/bt/bt -t/set/mach/ps/runq/timer/dev -p/net/kmem -s
dis -l 0xffffa6c968a6ae44 24                   # 崩点反汇编
dis -l 0xffffa6c968a6adfc 20                   # x20 来源指令上下文
dis -l find_busiest_group 40
dis -l _find_next_and_bit 24                   # 前兆崩点
sym 0xffffa6c96a4996c0                         # = runqueues
sym 0xffffa6c96a8955d0                         # = __per_cpu_offset
rd ffff8000b722b960 64                         # 崩溃栈（含 x0/x1 来源）
rd ffffa6c96a8955d0 192                        # __per_cpu_offset 头部真值
rd ffffa6c96a895b40 12 ; rd -8 ffffa6c96a895b50 8   # 槽176 附近真值
rd -p 602f890955c0 16 ; rd -p 602f89095b40 8   # 物理页直读
px __per_cpu_offset[176/177/178/179/1]
p __per_cpu_offset[179]                        # = 0xffffd93717344000
struct lb_env ; px ((struct lb_env *)0xffff8000b722bb70)->{sd,src_rq,dst_rq,src_cpu,dst_cpu,idle}
vtop 0xffffa6c96a895b50 ; vtop 0xffff604003e5f8a0 ; vtop 0xffff8000817dd6c0 ; vtop 0xffff604003e52218
p sched_domains_numa_masks ; p sched_domains_numa_levels
p node_data ; p &node_to_cpumask_map ; rd ffffa6c96c871478 32
rd ffff604003da5200 64                         # numa masks 指针数组
```

代数复算：`python3 algebra.py > algebra_out.txt`（8 节，全部 mod 2^64）

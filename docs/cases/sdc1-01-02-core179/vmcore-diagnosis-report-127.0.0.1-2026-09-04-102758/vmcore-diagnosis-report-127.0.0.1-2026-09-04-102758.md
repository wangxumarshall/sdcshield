# CPU179 转储深度诊断报告（第 1 次独立重研究）

## ——percpu 偏移装载塌缩为 0：一次装载指令读出 0 而内存真值非 0 的读通路 SDC 实锤

| 项目 | 内容 |
|---|---|
| 目标转储 | /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-10:27:58（vmcore 29.7GB, Kdump compressed v6, PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 CPU，768GB，8 NUMA 节点 |
| CPU | Kunpeng-920 (TaiShan-v110, 0x481fd010)，崩溃核 CPU179（MPIDR 0x00007a0300, Node7） |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP，KASLR on，48-bit VA，4K 页 |
| 崩溃时间 | 2026-09-04 10:27:14 CST（uptime 3951.16s ≈ 1:05:51，开机于 09:21:23） |
| 受害进程 | PID 293168 sftp-server（sshd 293162 子进程，SSH 文件传输业务），TASK_WAKING |
| 前兆 | 2 次 "Ignoring spurious kernel translation fault" WARNING（2099.55s / 2117.80s），**同为 CPU179、同一路径** |
| 结论一行 | `ldr x20,[x0,w25,sxtw#3]` 读 `__per_cpu_offset[149]` 得 0 而内存真值为 0xffffa6616d8f8000 —— 装载通路读出被腐化（零塌缩）【实锤】，叠加前兆 2 次 PTW 读出与页表真值矛盾的 spurious fault —— 判定 CPU179 load/translation 读数据通路间歇性受扰【强推】 |

## 1. 执行摘要

1. **现象**：sftp-server 在 `write()` 系统调用内核路径 `pipe_write→schedule→newidle_balance→load_balance→find_busiest_group` 中触发 level 3 翻译异常，`Unable to handle kernel paging request at virtual address ffffd99f13ae97e0`，Oops 致命转储。
2. **签名**：致命指令 `find_busiest_group+0x140`（`ldr x23,[x27,#288]`，即读 `rq.cfs.avg.load_avg`）；坏地址恰等于 `&runqueues+288`——percpu 偏移**没有加上**。闭合等式：`FAR == (x1 + x20) + 288` 且 `x20 == 0`。
3. **闭合验证结果**：上一条指令 `ldr x20,[x0,w25,sxtw#3]`（x0=&__per_cpu_offset, w25=149）的载入结果 x20=0；而 crash 实测内存真值 `__per_cpu_offset[149]=0xffffa6616d8f8000`（非 0），数组页 PTE 有效、内容可读——**寄存器结果与内存真值直接矛盾**，装载通路读出被腐化为 0【实锤】。
4. **前兆**：开机后 2099.55s 与 2117.80s，同一 CPU179、同一 `load_balance` 路径（`_find_next_and_bit` 读 sched_group cpumask）两次 FSC=level 0 翻译异常，内核用 `at s1e1r` 软件重放翻译成功而判定 "spurious" 仅告警存活——重放成功证明页表内存内容有效，**硬件首次走查结果与内存页表矛盾**，PTW 读出被腐化【强推】。致命事件发生在此后 1833s。
5. **软件成因排除**：涉事指令是调度器每秒执行数千次的热路径；`__per_cpu_offset[]` 在 boot 期写定后从不改写；三次异常全部锁定同一物理核 CPU179，软件 bug 不挑核。
6. **置信与处置**：微架构根因为 **CPU179 装载/翻译读数据通路间歇性受扰（读出塌缩为 0 / 读出非法条目），属静默数据腐蚀（SDC）类硬件故障**；致命事件为实锤级（寄存器 vs 内存真值直接对照），通路级定位（load path vs PTW）为强推级。处置建议：隔离/下线 CPU179 所在物理核，RAS 日志核查，必要时换件。

## 2. 证据规则与方法

- **证据源**：vmcore-dmesg.txt（2731 行）为第一证据；vmcore（29.7GB）用 `crash 8.0.4-17.oe2403sp4 + /tmp/vmlinux-0102`（BuildID 276194e5…，与 dump 匹配，实测可加载、bt 正常）做内存真值对照。
- **诚实铁律**：报告中每一处数据引用均来自实际运行命令的真实输出（dmesg 行号 / crash 提示符上下文），全文不使用预测性表述；crash 输出中的 `seek error`（PARTIAL DUMP 未过滤页）如实保留在附件中。
- **三级置信**：【实锤】可直接复核（寄存器值 vs 内存真值直接矛盾）；【强推】多源收敛但缺直接对照（PTW 读出腐化：spurious 判定逻辑 + 页表真值交叉）；【假设】无法软件验证，给出验证途径。
- **工具清单**：crash（sys/panic/bt/bt -t/set/mach/ps/runq/timer/dev -p/kmem -s、dis -l、struct -o、rd、vtop、sym、kmem、search）、Python3 mod 2^64 代数复算（algebra.py）、grep/sed。
- 所有命令与关键输出全文存附件：`dmesg_forensics.txt`、`crash_forensics_full.txt`、`crash_forensics_focused.txt`、`algebra.py`/`algebra_out.txt`。

## 3. 本次开机时间线【时间线】

| uptime (s) | 墙钟 (CST) | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0.000000 | 09:21:23 | 开机，CPU0x80000 启动，内核 6.6.0-145.3.23.154.oe2403sp3 | 1 | 实锤 |
| 0.329809 | 09:21:23 | CPU179 二次启动完成（MPIDR 0x00007a0300） | 1203-1206 | 实锤 |
| 30.146190 | 09:21:53 | 根文件系统挂载完成（dm-2, ext4），业务开始 | 2589-2590 | 实锤 |
| 94.493539 | 09:22:57 | dm-2 capability 弃用告警（最后一次正常内核消息） | 2592 | 实锤 |
| 2099.552069 | 09:56:22.552 | **前兆1**：spurious translation fault @ ffff604003e54458，CPU179, rcu_sched(PID16)，FSC=level 0 | 2593-2634 | 实锤 |
| 2117.796119 | 09:56:40.796 | **前兆2**：spurious translation fault @ ffff604003e61280，CPU179, rcu_sched(PID16)，FSC=level 0 | 2635-2676 | 实锤 |
| 3951.160261 | 10:27:14.160 | **致命 Oops**：kernel paging request @ ffffd99f13ae97e0，CPU179, sftp-server(PID293168)，FSC=level 3 | 2677-2728 | 实锤 |
| 3951.573641 | 10:27:14.573 | SMP: stopping secondary CPUs | 2729 | 实锤 |
| 3951.583179 | 10:27:14.583 | Starting crashdump kernel（kexec 起跳；目录名 10:27:58 为落盘完成时间，+44s 吻合） | 2730-2731 | 实锤 |

前兆1→前兆2 间隔 18.24s；前兆2→致命间隔 1833.36s。三次事件全部落在 CPU179。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 2677-2728）

```
[ 3951.160261] Unable to handle kernel paging request at virtual address ffffd99f13ae97e0
[ 3951.169008] Mem abort info:
[ 3951.172588]   ESR = 0x0000000096000007
[ 3951.177126]   EC = 0x25: DABT (current EL), IL = 32 bits
[ 3951.183228]   SET = 0, FnV = 0
[ 3951.187067]   EA = 0, S1PTW = 0
[ 3951.190993]   FSC = 0x07: level 3 translation fault
[ 3951.196659] Data abort info:
[ 3951.200325]   ISV = 0, ISS = 0x00000007, ISS2 = 0x00000000
[ 3951.206601]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[ 3951.212442]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[ 3951.218544] swapper pgtable: 4k pages, 48-bit VAs, pgdp=000040448bb14000
[ 3951.226036] [ffffd99f13ae97e0] pgd=10006057fffff403, p4d=10006057fffff403, pud=10006057ffffe403, pmd=10006057ffffa403, pte=0000000000000000
[ 3951.239362] Internal error: Oops: 0000000096000007 [#1] SMP
[ 3951.348040] CPU: 179 PID: 293168 Comm: sftp-server Kdump: loaded Tainted: G        W           6.6.0-145.3.23.154.oe2403sp3.aarch64 #1
[ 3951.360925] Hardware name: Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026
[ 3951.369460] pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[ 3951.377213] pc : find_busiest_group+0x140/0xb60
[ 3951.382539] lr : find_busiest_group+0x11c/0xb60
[ 3951.387857] sp : ffff8001e54ab740
```

crash 加载确认（最小会话）：

```
crash> sys
    DUMPFILE: .../127.0.0.1-2026-09-04-10:27:58/vmcore  [PARTIAL DUMP]
        CPUS: 192
        DATE: Fri Sep  4 10:27:14 CST 2026
      UPTIME: 01:05:51
LOAD AVERAGE: 137.39, 153.39, 141.58
       TASKS: 2008
         PANIC: "Unable to handle kernel paging request at virtual address ffffd99f13ae97e0"
         PID: 293168
     COMMAND: "sftp-server"
        CPU: 179
```

`crash vtop ffffd99f13ae97e0` 与 dmesg 页表走查逐级一致（pgd/pud/pmd 条目相同，PTE=0），交叉验证 dump 完整可信。

### 4.2 全量寄存器（x0–x30，dmesg 2699-2717）

```
x29: ffff8001e54ab8c0 x28: ffff8001e54ab770 x27: ffffd99f13ae96c0
x26: ffff604003e611e0 x25: 0000000000000095 x24: ffffd99f13ee5000
x23: 0000000000000401 x22: ffff604003e618a0 x21: ffffd99f13edfcb0
x20: 0000000000000000 x19: ffff8001e54ab950 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000ffffa24a0010
x14: 507362023e7c0170 x13: 3600507338023e7c x12: 000000000001f78f
x11: 0000000000000079 x10: 0000000000000025 x9 : ffffd99f120bae58
x8 : ffff8001e54ab7c8 x7 : 0000000000000000 x6 : 0000000000000095
x5 : ffffffffffe00000 x4 : 0000000000000002 x3 : 0000000000000015
x2 : 0000000000019850 x1 : ffffd99f13ae96c0 x0 : 0000000000000095
```

关键值：`x20 = 0`（应为 `__per_cpu_offset[149]`）、`x1 = ffffd99f13ae96c0`（= &runqueues 静态地址）、`x27 = ffffd99f13ae96c0`、`w25 = 0x95 = 149`（目标 CPU 号）、`x24 = ffffd99f13ee5000`（node_data+560，代码中 percpu 基址材料）。

### 4.3 Call trace（完整，dmesg 2709-2728）

```
 find_busiest_group+0x140/0xb60
 load_balance+0x108/0x6c0
 newidle_balance+0x198/0x510
 pick_next_task_fair+0x110/0x718
 pick_next_task+0x60/0x398
 __schedule+0x1b4/0x8a0
 schedule+0x58/0x130
 pipe_write+0x1ec/0x558
 new_sync_write+0x140/0x158
 vfs_write+0x21c/0x2b0
 ksys_write+0xf4/0x118
 __arm64_sys_write+0x24/0x38
 invoke_syscall+0x50/0x128
 el0_svc_common.constprop.0+0xc8/0xf0
 do_el0_svc+0x48/0x78
 el0_slow_syscall+0x44/0x1b8
 el0t_64_sync_handler+0x100/0x130
 el0t_64_sync+0x188/0x190
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

### 4.4 前兆异常原文（dmesg 2593-2634、2635-2676）

```
[ 2099.552069] ------------[ cut here ]------------
[ 2099.552081] Ignoring spurious kernel translation fault at virtual address ffff604003e54458
[ 2099.552090] WARNING: CPU: 179 PID: 16 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[ 2099.552227] CPU: 179 PID: 16 Comm: rcu_sched Kdump: loaded Not tainted 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1
...
[ 2099.552237] pc : __do_kernel_fault+0x130/0x1b8
[ 2099.552243] sp : ffff8000821ab610
[ 2099.552249] x26: ffff604003e545a0 x25: 00000000000000c0 x24: ffffd99f13ee5000
[ 2099.552277] x5 : ffff8000817c9d88 x4 : ffff8000821ab470 x3 : ffffa6616dcf4000
...
[ 2099.552285] Call trace:
 __do_kernel_fault+0x130/0x1b8
 do_bad_area+0x70/0x88
 do_translation_fault+0x40/0x80
 do_mem_abort+0x4c/0xa8
 el1_abort+0x5c/0x150
 el1h_64_sync_handler+0xd8/0xe8
 el1h_64_sync+0x78/0x80
 _find_next_and_bit+0x18/0x80
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

第二次（2117.796119）结构完全相同：CPU 179 / PID 16 rcu_sched / `_find_next_and_bit+0x18` / `load_balance+0x108`，地址 ffff604003e61280。注意前兆寄存器 `x3 = ffffa6616dcf4000 = __per_cpu_offset[179]`（本核偏移，**装载正确**），说明前兆时刻 percpu 偏移读通路尚正常，出错的是位图读的翻译环节。

### 4.5 RAS 负证据（dmesg grep）

```
$ grep -n -iE 'machine check|hardware error|edac.*(error|ecc)|ecc|ras.*error|memory failure|extlog|ghes.*(error|sec)|corrected error|uncorrectable|thermal|cache.*error|l1.*error|l2.*error|l3.*error|tag.*parity|parity' vmcore-dmesg.txt
(仅命中: thermal governor 注册、HEST 表初始化、EDAC MC0 设备发放 等常规启动行; 无任何硬件错误事件)
$ grep -n -iE 'segfault|oom|out of memory|lockup|hung task|soft lockup|rcu.*stall|watchdog' vmcore-dmesg.txt
(仅命中: SDEI NMI watchdog 注册、GTDT Watchdog 发现 等启动行)
```

**本次开机全程无任何机器检查 / EDAC / GHES 硬件错误报告**——纯静默故障，与 SDC 属性吻合（无 ECC 事件 = 要么无 ECC 保护通路受扰，要么错误未触发阈值上报）。

## 5. 业务现象

- **受害进程**：PID 293168 `sftp-server`（父链 sshd 293162 ← sshd 292525 ← sshd 9845），OpenSSH 的 SFTP 数据面服务进程。崩溃时它在执行 `write()` 写管道（`pipe_write`），管道满而调用 `schedule()` 睡眠让出 CPU。
- **调度上下文**：让出 CPU 进入 `__schedule → pick_next_task → newidle_balance → load_balance`——本核空闲前的负载均衡例行路径。这是纯粹的**路过受害**：sftp-server 本身的数据未坏，它只是恰好在 CPU179 上触发了一次调度决策，而该决策依赖的 percpu 读数被硬件通路腐化。
- **机器负载**：load average 137/153/141（192 核，约 71-80% 负载）；2008 个任务。其中有大量 `HeapHelper`/`Bun Pool`/`claude`/`mi-scavenger`（各 ~487MB RSS 的 Bun/Node 运行时线程池）与 `opendcdiag`（UN 状态）——这是一台同时跑 AI 工具链与压力测试的诊断开发机，SFTP 用于转储/文件搬运。
- **业务影响**：内核 Oops → kdump 转储 → 系统重启，SFTP 会话中断、当日测试批次中断。

## 6. 诊断定位过程【诊断定位过程】

**P1 · dmesg 勘察**：2731 行全量扫描。定位 1 次致命 Oops（3951s, CPU179, sftp-server）+ 2 次同签名前兆 WARNING（2099s/2117s, CPU179, rcu_sched）；RAS 全负；无 OOM/lockup/stall。三次事件的时间间隔由 Python3 计算（18.24s / 1833.36s）。

**P2 · 崩溃块提取**：提取 ESR=0x96000007（DABT, level 3 translation fault）、pgtable 走查（pte=0）、x0–x30 全量寄存器、完整 call trace、Code 窗口。人工解码 Code 窗口五条指令：`ldr x2,[x28,#8]; ldr x20,[x0,w25,sxtw#3]; mov w0,w25; add x27,x1,x20; (ldr x23,[x27,#288])`——pc 落在最后一条带括号的 `f9409377`。

**P3 · crash 加载**：`crash /tmp/vmlinux-0102 vmcore -i cmdfile` 一次加载成功（PARTIAL DUMP, 192 CPU, 2008 tasks, panic 信息一致）。批量执行 sys/panic/bt/bt -t/set/mach/ps/runq/timer/dev -p/net/kmem -s，全量输出 6085 行入附件。runq 确认 CPU179 当前任务即 sftp-server（TASK_WAKING/PANIC）。

**P4 · 内存真值对照（本案核心）**：
- `sym ffffd99f13ae97e0` → `runqueues+288`；`px &runqueues` → `0xffffd99f13ae96c0`。即坏地址 = **静态 percpu 模板地址 + 288**，percpu 偏移没加。
- 反汇编确认指令序列（`dis find_busiest_group`）：+0x130 `ldr x20,[x0,w25,sxtw#3]`、+0x134 `mov w0,w25`、+0x138 `add x27,x1,x20`、+0x140 `ldr x23,[x27,#288]`（致命点）；prologue +0x68/+0x7c 把 `&runqueues` 存进 `[sp,#16]`，循环内 `ldp x0,x1,[sp,#8]` 取回 `{x0=&__per_cpu_offset, x1=&runqueues}`。
- 崩溃栈实测：`rd ffff8001e54ab740` 首两槽 = `ffffd99f13ee55d0`（=&__per_cpu_offset）与 `ffffd99f13ae96c0`（=&runqueues）——与代码期待的循环槽位完全吻合，x1 来源无疑。
- `px &__per_cpu_offset` → `0xffffd99f13ee55d0`；`p __per_cpu_offset[149]` → `18446645536113000448` = `0xffffa6616d8f8000`（**非 0**）；`rd ffffd99f13ee55d0 4` 数组前 4 项均非 0；`vtop ffffd99f13ee5000` 数组页 PTE=`f840448c0e5f03`（VALID|AF|DIRTY）。
- percpu 算术自检：`rq_sym + __per_cpu_offset[179] == 0xffff8000817dd6c0`（与 `px runqueues` 输出的 [179] 实例一致）——percpu 机制理解无误。
- 字段语义：`struct -o rq`(cfs@128) + `struct -o cfs_rq`(avg@128) + `struct -o sched_avg`(load_avg@32) ⇒ **runqueues+288 = rq.cfs.avg.load_avg**，正是 load_balance 聚合的负载字段。

**P5 · 软件成因排除**：
- `__per_cpu_offset[]` 由 `setup_per_cpu_areas()` 在 boot 期写定，运行期从不改写；149 号表项在 dump 中真值非 0，且 CPU149 在线（其 rq 活跃）。
- 该指令序列是调度器 CFS 域负载统计热路径，正常内核每秒执行数千次；若软件有 bug 不会单挑 CPU179/单挑这一时刻。
- 内核源（6.6 上游 + openEuler）无此路径的已知缺陷；Tainted 仅为 G+W（W 来自两次前兆 WARNING 自身）。

**P6 · 定位收敛**：寄存器 x20=0 vs 内存真值非 0 的直接矛盾（实锤）⇒ 装载结果零塌缩 ⇒ x27 塌缩到静态模板地址 ⇒ 该地址 pte=0（L3 fault，与 dmesg/crash 双向一致）⇒ Oops。前兆两次 spurious fault（AT 重放成功 = 页表内存有效，硬件首次走查却失败）锁定 PTW 读通路也曾受扰。三次事件同核同路径，收敛到 **CPU179 的取数/翻译读通路间歇性受扰**。

## 7. 逻辑链条

**指令语义**（`dis -l` + debug_info，源行 kernel/sched/fair.c:12050 / find.h:101）：

```
0xffffd99f120bae34 <find_busiest_group+300>: ldp  x0, x1, [sp, #8]      ; x0=&__per_cpu_offset, x1=&runqueues
0xffffd99f120bae38 <find_busiest_group+304>: ldr  x2, [x28, #8]
0xffffd99f120bae3c <find_busiest_group+308>: ldr  x20, [x0, w25, sxtw #3] ; x20 = __per_cpu_offset[w25]  ← 出错的装载
0xffffd99f120bae40 <find_busiest_group+312>: mov  w0, w25
0xffffd99f120bae44 <find_busiest_group+316>: add  x27, x1, x20           ; x27 = &runqueues + off[cpu]
0xffffd99f120bae48 <find_busiest_group+320>: ldr  x23, [x27, #288]       ; x23 = rq.cfs.avg.load_avg  ← 致命异常
```

**闭合等式【实锤】**（Python3 mod 2^64，见 algebra_out.txt [A][I]）：

```
FAR == (x1 + x20) + 288
0xffffd99f13ae97e0 == (0xffffd99f13ae96c0 + 0x0) + 288   ✓

寄存器结果: x20 = 0x0000000000000000          (dmesg Oops)
内存真值:   __per_cpu_offset[149] = 0xffffa6616d8f8000   (crash p, 非 0!)
数组页 PTE: f840448c0e5f03 VALID             (crash vtop, 页有效可读)
```

装载结果与内存真值逐位矛盾——**不是内存被写坏（内存是好的），不是页表问题（该页有效），只能是装载通路把读出的数据腐化成了 0**。

**反事实推演**：若 x20 = 真值 0xffffa6616d8f8000，则 x27 = `&runqueues + off[149]` = `0xffff8000813e16c0`，FAR = `0xffff8000813e17e0`（CPU149 的 rq.cfs.avg.load_avg）。crash `vtop 0xffff8000813e17e0` 实测 PTE=`e86037fff1ef03`（VALID|AF|DIRTY）——**该地址完全有效，不会触发任何异常**。即：硬件读通路只要给出正确值，指令链根本不会崩溃。

**前兆通路一致性【强推】**：前兆 2 次出错装载是 `_find_next_and_bit+0x18`（`ldr x3,[x0,x4,lsl#3]`，读 sched_group cpumask 位图，对象经 `kmem` 确认为 Node7 kmalloc-96 slab 的 sched_group，FAR 分别等于 g1+56/g2+56+8）。FSC=level 0（PGD 级失配），但 crash `vtop` 显示该线性映射区由 1GB PUD block（`e8604000000f05` VALID）覆盖、PGD 条目自 boot 起从未撤销——硬件走查结果与页表内存真值矛盾。且内核的 `is_spurious_el1_translation_fault` 用 `at s1e1r` 重放翻译成功才打印 "Ignoring spurious"（反汇编核实：`at s1e1r, x0; mrs x0, par_el1; tbz w0, #0`→成功即 spurious）——软件重放成功本身就是"页表内存有效、硬件首次走查出错"的直接证据。

**诚实声明**：
- 【实锤】x20 装载塌缩为 0（寄存器 vs 内存真值直接对照，可复核）。
- 【强推】受扰环节在 CPU179 的装载/翻译**读数据通路**（load path / PTW 读出）。证据形态符合"装载结果塌缩为 0 而内存真值非 0"与"PTW 读出与页表真值矛盾"两个强推特征；但软件无法区分受扰具体发生在 L1D/TLB/HHA/DDRC 通路中的哪一级——这需要芯片侧 RAS/错误注入复现（验证途径见第 9 节）。
- 【假设】物理成因（粒子翻转、供电毛刺、时序违例、特定通路的硅缺陷）无法从软件侧判定。
- 本 dump 为 PARTIAL DUMP，崩溃核 pt_regs 寄存器区被 kdump 过滤页排除（`gdb info registers` unavailable、`rd` 若干 seek error）——但 dmesg 已完整记录 x0–x30，且本报告所有关键真值对照（__per_cpu_offset 数组、rq、sched_group、页表）所在页均成功读出，结论不受影响。

## 8. 故障根因（微架构级结论 + 置信级别）

**根因**：CPU179（Kunpeng-920 TaiShan-v110 核，MPIDR 0x00007a0300）的**读数据通路存在间歇性受扰**，在 1 小时 05 分的开机窗口内至少发作 3 次，全部落在 `load_balance` 的读密集热路径上：

1. 两次表现为 PTW 页表走查读出非法条目（FSC=level 0，软件 AT 重放成功证明页表内存有效）——PTW 读出被腐化【强推】；
2. 一次表现为普通装载 `ldr x20,[x0,w25,sxtw#3]` 读 `__per_cpu_offset[149]` 返回 0 而内存真值非 0——**load path 数据通路读出被腐化（零塌缩）【实锤】**；
3. 第三次使指针算术结果塌缩到未映射的静态 percpu 模板地址，触发 level 3 翻译异常，经 `do_translation_fault→do_bad_area→die_kernel_fault` 致命 Oops，kdump 转储。

**置信级别**：致命机制的判定为【实锤】（寄存器-内存真值直接矛盾 + 页表双向一致 + percpu 算术自检）；"读通路（而非内存/页表本身）受扰"为【强推】（三条独立证据链收敛：真值对照、AT 重放、同核同路径复发）；"CPU179 硬件缺陷/单粒子类物理成因"为【假设】（软件侧无法判定物理机制，但软件成因已被 P5 排除）。

这属于典型的 **SDC（静默数据腐蚀）**：若塌缩的读数未落入未映射地址（例如 x20 塌缩发生在读 `rq.cfs.avg.load_avg` 本身），内核将继续用错误负载值做均衡决策——静默错误决策而非崩溃。本案的"幸运"仅在于错误值恰好撞上了 pte=0 的哨兵地址。

## 9. 启示

### 9.1 微架构定位的意义

本案把一次"随机 Oops"还原成了"同一物理核读通路的三次可复现签名"。对运维/验收的直接价值：
- **同核聚集是硬件嫌疑的指纹**。三次事件（2 前兆 + 1 致命）全部在 CPU179，且间隔 18s/1833s——如果是软件 bug，192 个核跑同一调度器代码不会只在一个核上爆。
- **"spurious translation fault" 不是噪音**。内核把它降级为 WARNING 并继续跑，本案证明它可以是致命故障提前 30 分钟发出的免费预警。任何生产环境出现同核聚集的 spurious fault 都应当作硬件 RAS 事件处理，而不是忽略。
- **percpu 偏移零塌缩是一个可监测的 SDC 指纹**：`__per_cpu_offset[i]` 运行期恒非 0，任何路径上出现"percpu 访问落到静态模板地址区间"（本例 [ffffd99f13ae96c0, +pcpu_unit_size)）即读通路腐化的铁证。

### 9.2 芯片设计与实现启示（基于本案证据形态，可落地）

1. **load path 数据校验缺口**：从 L1D/LLC/HNS 到核内填充缓冲、寄存器写回的装载通路若无端到端 ECC/parity 覆盖，单次翻转即静默传给寄存器文件。建议对跨 die/跨 SCCL 的读返回通路加 side-band parity（哪怕单比特检错不纠错），并设置可编程的"读出全 0/全 1 可疑值"陷阱计数器——本案零塌缩形态会被立即捕获。
2. **PTW 读出校验**：页表走查器读回的描述符在进入 TLB 前做合法性 sanity（如对已知恒有效区间的 level0 "miss" 触发内部重试 + 错误计数），可把本案前兆两级的 spurious fault 变成一次硬件自愈事件而非软件 WARNING。
3. **关键恒定结构的冗余读**：`__per_cpu_offset[]` 这类 boot 后只读、被所有 percpu 访问依赖的数组，可在读侧做双读比较（load-twice-compare）——两次读不一致时重试并计数。成本是热路径加倍读，但对调度器这种"错一次就可能级联"的元数据值得。
4. **错误传播屏障设计**：本案 x20 的错值经一条 `add` 即变成解引用地址。若 TLB miss 通路/地址生成阶段对"可疑全 0 基址 + 大位移偏移"的组合做断言（fail-fast），可把静默 SDC 变成受控的精确异常，转储现场更干净（本案其实侥幸拿到了干净现场）。
5. **fail-fast vs silence 的权衡**：TaiShan-v110 核内结构大多无 ECC，错误默认静默。建议在下一代为"控制平面数据"（页表描述符、调度元数据、percpu 基址）优先配置检测，而数据平面（bulk 数据）可后置——控制数据一位错的全局代价远高于流水线重试。
6. **DFT/在线测试钩子**：为读通路提供运行时可触发的 loopback 比对（例如定期用已知图案回读 percpu 常量数组并与影子副本比对），把本案"潜伏 1 小时才爆"的缺陷变成分钟级在线体检项。
7. **kdump 可靠性设计**：PARTIAL DUMP 把崩溃核 pt_regs 过滤掉了（本案靠 dmesg 补全寄存器）。建议 kdump 过滤规则强制保留 panic 栈顶 4KB + panic CPU 的 IRQ/overflow 栈，代价极小。

### 9.3 对系统软件/RAS 的启示

1. **把 "Ignoring spurious kernel translation fault" 升级为 RAS 事件**：同核 N 次聚集即触发内核告警/自动隔离流程（arm64 可在 `is_spurious_el1_translation_fault` 返回真时按 CPU 维度计数上报）。本案若在第二次 WARNING 即隔离 CPU179，可避免 30 分钟后的宕机。
2. **调度器关键读数的廉价断言**：`per_cpu_ptr` 生成的地址若落在 `[__per_cpu_start, __per_cpu_end)` 静态区间（运行期不该被直接解引用）即可 BUG/计数——一条比较指令的成本，捕获整类 percpu 塌缩 SDC。
3. **异常遥测补齐**：本案全程无 GHES/EDAC 事件，说明该故障形态在现有 RAS 覆盖之外。固件侧应为 LLCC/通路 parity 类事件提供上报通道，OS 侧 `rasdaemon` 归并"spurious fault 聚集"模式。
4. **故障核快速定位工作流**：本案的取证路径（dmesg 寄存器 → 反汇编指令语义 → crash 读内存真值 → 闭合等式）可固化为脚本化 runbook，30 分钟内给出"哪个核、哪条通路、寄存器 vs 真值"三要素结论。

## 10. 处置建议

1. **立即**：若该机器仍在服役，隔离 CPU179 所在物理簇（`nohz_full`/`isolcpus` 不可完全避免调度器自身读路径，最稳妥是内核启动参数 `maxcpus` 规避或 RAS 层下线该核），并对其所在 SCCL 的其余核（CPU178 同簇 0x7a）保持观察。
2. **取证补全**：导出 BMC/PHYP 侧硬件日志（本案 OS 侧 RAS 全负，需固件侧确认有无通路 parity 记录）；若有条件，对 CPU179 所在簇做内存/缓存压力诊断（如 `opendcdiag` 定向绑定）复现读通路异常。
3. **复发监测**：部署 "spurious fault 同核聚集" 告警（内核 tracepoint 或 eBPF 挂 `is_spurious_el1_translation_fault` 返回点），阈值建议 ≥2 次/小时即升级。
4. **换件评估**：若同核聚集再次出现（本开机内已 3 次），按硬件缺陷处理流程更换 CPU/主板；单次孤立事件可降级观察。

## 附录：命令索引（全部取证命令，可复核）

```
# dmesg 法证
wc -l /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-10:27:58/vmcore-dmesg.txt
head -80 <vmcore-dmesg.txt>; sed -n '2593,2731p' <vmcore-dmesg.txt>
grep -n -E 'Command line|Machine model|Memory:|crashkernel' <vmcore-dmesg.txt>
grep -n -E 'WARNING|BUG|Oops|cut here|spurious|Machine check|Hardware error|EDAC|ECC|ras|memory failure|segfault' <vmcore-dmesg.txt>
grep -n -iE 'machine check|hardware error|ecc|corrected error|uncorrectable|thermal|parity|lockup|stall|oom' <vmcore-dmesg.txt>
grep -n 'Starting crashdump kernel|SMP: stopping|CPU179' <vmcore-dmesg.txt>

# crash 法证（namelist=/tmp/vmlinux-0102, vmcore=目标转储, 全部 -i 批处理, timeout 590）
# 最小集: sys / panic / bt / quit
# 全量集: sys / panic / bt / bt -t / set / mach / ps / runq / timer / dev -p / net / kmem -s / quit
# 聚焦集 (/tmp/case_reg.cmd 等, 详见 crash_forensics_focused.txt):
dis -l ffffd99f120bae44        # 致命pc源行 fair.c:12050
dis find_busiest_group         # 全函数反汇编(指令语义)
dis load_balance               # 调用点 load_balance+260 bl find_busiest_group
dis _find_next_and_bit         # 前兆出错函数
dis __do_kernel_fault          # 前兆WARNING路径
dis is_spurious_el1_translation_fault.constprop.0   # 'at s1e1r' 重放逻辑
p __per_cpu_offset[0/148/149/150/178/179/180]       # 内存真值
px &__per_cpu_offset           # = 0xffffd99f13ee55d0
px runqueues; sym runqueues    # &runqueues = 0xffffd99f13ae96c0
struct rq ffff8000817dd6c0     # CPU179 rq 实例
rd ffff8000817dd7e0 4          # rq179+288 真值 (load_avg=640)
struct -o rq; struct -o cfs_rq; struct -o sched_avg  # +288 字段定位
struct sched_group 0xffff604003e54420 / 0xffff604003e61240  # 前兆受害对象
kmem ffff604003e54458 / ffff604003e61280             # kmalloc-96, Node7
vtop ffffd99f13ae97e0          # 致命FAR: PTE=0 (与dmesg一致)
vtop ffffd99f13ee5000          # __per_cpu_offset页: PTE=f840448c0e5f03 VALID
vtop ffff604003e54458 / ffff604003e61280             # 前兆FAR: 1GB block VALID
vtop ffff8000817dd6c0 / ffff8000813e16c0             # rq179 / 正确x27: PTE有效
sym ffffd99f13ae97e0           # runqueues+288
kmem ffffd99f13ae97e0
bt -r; rd ffff8001e54ab740 64  # 崩溃栈: [&__per_cpu_offset, &runqueues] 槽位实证
search -t 293168 ffffa6616d8f8000
rd ffffd99f13ee55d0 4          # __per_cpu_offset[0..3] 非零实证

# 代数复算
python3 algebra.py > algebra_out.txt    # 全部64位运算 mod 2^64, 见 algebra.py
```

（附件：`dmesg_forensics.txt`、`crash_forensics_full.txt`、`crash_forensics_minimal.txt`、`crash_forensics_focused.txt`、`algebra.py`、`algebra_out.txt`）

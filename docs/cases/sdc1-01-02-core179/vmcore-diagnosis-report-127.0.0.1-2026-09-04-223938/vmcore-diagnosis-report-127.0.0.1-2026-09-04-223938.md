# CPU179 取数通路缺陷核深度诊断报告（独立盲测）
## x20 加载结果归零致 find_busiest_group 解引用静态 percpu 镜像，8 次 spurious translation fault 前兆与数值型 SDC 的微架构定位

> 诊断方法：独立（blind）取证。未参考任何既有分析文档；证据仅来自 vmcore 原始文件、vmcore-dmesg.txt（行号引用）、crash 8.0.4 真实输出（crash_session_1..25.log，其中 _21.._25 为本次会话新增）、反汇编与 Python3 模 2⁶⁴ 复算（algebra.py → algebra_out.txt）。
> 报告目录：`docs/cases/vmcore-diagnosis-report-127.0.0.1-2026-09-04-223938/`

| 项目 | 内容 |
|---|---|
| 转储目录 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:39:38/` |
| vmcore | 9.4 GB（crash 标记 PARTIAL DUMP，bt/rd/vtop 可用） |
| vmcore-dmesg | 220 KB，2995 行，boot 段完整 |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP Mon Jul 27 19:00:34 CST 2026 |
| vmlinux | `/tmp/vmlinux-0102`（BuildID 匹配） |
| 整机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 CPU / 8 NUMA 节点 / 768 GB |
| panic 时刻 | 2026-09-04 22:38:54 CST，UPTIME 347s（crash banner） |
| panic 位置 | `find_busiest_group+0x140/0xb60`，CPU179，PID 9703 NetworkManager |
| 诊断结论 | CPU179 取数通路数值型 SDC（高置信度，见 §8） |

## 1. 执行摘要

开机 347 秒（UPTIME 00:05:47）内，CPU179 先后出现两类同源故障：

1. 前兆（223.3s–333.3s，共 8 次）：`Ignoring spurious kernel translation fault`，全部发生在 CPU179、`__memcpy`（`show_interrupts` → `seq_printf` 的 /proc/interrupts 读路径），spurious 虚地址 8 个全部聚集在 `ffff60400839xxxx` 的 linear map 直接映射区（4 个相邻页帧）。内核用 `AT S1E1R` 重放页表遍历证明这些地址映射完全有效，即硬件报了虚假的 level 3 translation fault。
2. 致命（347.1s）：`find_busiest_group+0x140`（`LDR x23,[x27,#0x120]`）解引用 `ffffbda5543597e0` 触发 level 3 translation fault，Oops，kdump。

寄存器代数复算（全部 Python3 模 2⁶⁴，见 algebra_out.txt）给出完全闭合的证据链：

- `FAR − x27 = 0x120`：与出错指令 immediate 精确闭合；
- `x27 = x1 + x20`，其中 `x1 = ffffbda5543596c0`（= 崩溃栈 sp+16 槽实测值 = `&runqueues` 静态链接地址，完好），而 `x20 = 0`；
- `x20` 的内存源 `__per_cpu_offset[56] = ffffc25b2c42e000` / `__per_cpu_offset[179] = ffffc25b2d484000`（w25=0x38=56，dmesg 实测），在转储中均非零且完好；
- 因此 `x27` 是"少加了 percpu offset 的指针"，而非"被位翻转的指针"（与期望值 XOR 需 17 位同时翻转）：加数 `x20` 在 LDR 执行/数据返回/寄存器写回路径上整体归零。

这是数值型 SDC（非位翻转型）：内存侧完好、寄存器侧错误。RAS 通路（RAS Extension + GHES/APEI + ghes_edac）全程静默，347 秒无任何 machine check / ECC / CPER 记录（grep 负证据）。前兆（虚假 translation fault）与致命事件（加载结果归零）共同指向 CPU179 单核的取数通路（L1D/LSU/PTW 数据返回路径或加载结果寄存器写回）存在未被 RAS 覆盖的静默缺陷：间歇性、可复现于同一核、跨 124 秒 9 次事件。

根因结论（置信度：高，约 90%）：CPU179 单核硬件缺陷，位于数据加载通路（load return path / LSU→寄存器堆写回 / 或 PTW 交互），产生未被检测的静默数据损坏；软件成因（内核 bug、编译器、KASAN、OOM、热 throttling）均有负证据排除。

## 2. 证据规则与方法

| 规则 | 说明 |
|---|---|
| 原始证据 | 只取 vmcore-dmesg.txt（带行号）、crash 8.0.4 对 vmcore 的真实输出、vmlinux 反汇编 |
| 可复核 | 全部命令收录于附录与 crash_session_N.log（_1.._20 为前会话原始取证输出，_21.._25 为本次新增），dmesg 引用带行号 |
| 禁止手算 | 所有 64 位运算经 algebra.py（模 2⁶⁴）复算，输出 algebra_out.txt |
| 负证据如实记录 | crash 加载 banner 前有大量 `seek error: ... SDEI/IRQ stack pointer` 噪声（PARTIAL DUMP 缺页所致），不影响白名单内存读取；`rd ffffbda554359000` 返回 `page excluded`（percpu 静态镜像页未入 dump），如实记录 |
| 推测标注 | 无法实证的推断明确标注"推测" |

crash banner（crash_session_1.log 行417-434）：
```
KERNEL: /tmp/vmlinux-0102  [TAINTED]
DUMPFILE: .../vmcore  [PARTIAL DUMP]
CPUS: 192   DATE: Fri Sep  4 22:38:54 CST 2026   UPTIME: 00:05:47
LOAD AVERAGE: 146.92, 64.02, 24.63   TASKS: 2177
MEMORY: 767.8 GB
PANIC: "Unable to handle kernel paging request at virtual address ffffbda5543597e0"
PID: 9703  COMMAND: "NetworkManager"  CPU: 179  STATE: TASK_WAKING (PANIC)
```

## 3. 本次开机时间线【时间线】

（全部来自 vmcore-dmesg.txt，行号括注）

| 时刻 | 事件 | 行号 |
|---|---|---|
| 0.000000 | boot，CPU0 MPIDR 0x80000，KASLR enabled | 1-3 |
| 0.330980 | CPU179 上线（MPIDR 0x00007a0300，型号 0x481fd010 与全机一致） | 1206 |
| 0.352802 | smp: Brought up 8 nodes, 192 CPUs | 1255 |
| 0.352908 | CPU features: detected: RAS Extension Support | 1262 |
| 0.569568 | GHES: APEI firmware first mode is enabled | 1308 |
| 1.510611 | EDAC MC0: ghes_edac 接管（32 DIMM sockets） | 2176-2177 |
| 1.760914 | Freeing unused kernel memory（初始化完成） | 2274 |
| 38.304723 | hns3 网卡 link up（业务就绪） | 2579 |
| 85.490536 | 最后一条正常业务 dmesg | 2580 |
| 223.323 | WARNING #1：spurious fault @ ffff60400839317a，pmdalinux | 2582-2583 |
| 223.335 | WARNING #2 @ ffff6040083931fe（+12ms） | 2627-2628 |
| 233.300 | WARNING #3 @ ffff604008391676 | 2672-2673 |
| 243.309 | WARNING #4 @ ffff604008392584 | 2717-2718 |
| 293.294 | WARNING #5 @ ffff604008397747 | 2762-2763 |
| 315.048 | WARNING #6 @ ffff6040083937b5，irqbalance | 2807-2808 |
| 315.054 | WARNING #7 @ ffff6040083935bb（+5ms） | 2852-2853 |
| 333.281 | WARNING #8 @ ffff60400839235e | 2897-2898 |
| 347.112 | 致命 Oops：find_busiest_group+0x140，NetworkManager | 2941,2954 |
| 347.534 | stopping secondary CPUs → kdump → Bye! | 2994-2995 |

存活期 347.5 秒；首个异常出现于 223.3s（开机后 3 分 43 秒），前兆窗口到致命共 124 秒 9 次事件。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 行 2941–2953）

```
[  347.111633] Unable to handle kernel paging request at virtual address ffffbda5543597e0
[  347.120292] Mem abort info:
[  347.123786]   ESR = 0x0000000096000007
[  347.128237]   EC = 0x25: DABT (current EL), IL = 32 bits
[  347.134253]   SET = 0, FnV = 0
[  347.138008]   EA = 0, S1PTW = 0
[  347.141848]   FSC = 0x07: level 3 translation fault
...
[  347.176359] [ffffbda5543597e0] pgd=10006057fffff403, p4d=10006057fffff403,
               pud=10006057ffffe403, pmd=10006057ffffb403, pte=0000000000000000
[  347.189598] Internal error: Oops: 0000000096000007 [#1] SMP
```

关键：`S1PTW = 0`（非页表遍历自身出错），FSC=0x07 level 3 fault，pte=0。

### 4.2 全量寄存器（dmesg 行 2946–2953）

```
x29: ffff8000ce5ab510 x28: ffff8000ce5ab3c0 x27: ffffbda5543596c0   ← 关键
x26: ffff604003e26f00 x25: 0000000000000038 x24: ffffbda554755000   ← w25=56
x23: 0000000000000400 x22: ffff604003e264e0 x21: ffffbda55474fcb0
x20: 0000000000000000                                            ← 关键：x20 = 0
x19: ffff8000ce5ab5a0 x18-x17: 0 x16: 0 x15: 0000ffff9c0058c0
x14: 0000000100000012 x13: 0000000100000011 x12: 0
x11: 000000000000004a x10: 0 x9 : ffffbda55292ae58
x8 : ffff8000ce5ab418 x7 : 0 x6 : 0000000000000038
x5 : ff00000000000000 x4 : 0 x3 : 0000000000000038
x2 : 000000000000dff6 x1 : ffffbda5543596c0                        ← x1 完好
x0 : 0000000000000038
pc : find_busiest_group+0x140/0xb60   lr : find_busiest_group+0x11c/0xb60
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)               ← 出错指令
```

### 4.3 Call trace（dmesg 行 2973–2990；crash bt 见 crash_session_1.log 行451-481）

```
find_busiest_group+0x140/0xb60      ← 崩溃 PC
load_balance+0x108/0x6c0
newidle_balance+0x198/0x510
pick_next_task_fair+0x110/0x718
pick_next_task+0x60/0x398
__schedule+0x1b4/0x8a0
schedule+0x58/0x130
schedule_hrtimeout_range_clock+0xdc/0x150
schedule_hrtimeout_range+0x1c/0x30
do_poll.constprop.0+0x288/0x310
do_sys_poll+0x234/0x308
__arm64_sys_ppoll+0xa8/0x138        ← NetworkManager 的 ppoll
invoke_syscall → el0_svc_common → do_el0_svc → el0_slow_syscall → el0t_64_sync
```

crash `bt -t`（session_8）进一步显示 pollwake×8 嵌套唤醒痕迹，属正常 poll 语义。

### 4.4 前兆 WARNING 原文（8 次完全同构，例：dmesg 行 2582–2583）

```
[  223.323060] Ignoring spurious kernel translation fault at virtual address ffff60400839317a
[  223.323069] WARNING: CPU: 179 PID: 14806 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
```

8 次 WARNING 的共同特征（dmesg 行 2582–2934 逐块核对）：
- 全部 CPU179；进程为 pmdalinux(6 次)/irqbalance(2 次)，都是 `/proc/interrupts` 的周期读者（PCP 采样与 irqbalance 轮询）；
- call trace 完全同构：`__do_kernel_fault → do_bad_area → do_translation_fault → do_mem_abort → el1_abort → el1h_64_sync_handler → el1h_64_sync → __memcpy+0x80 → seq_printf → show_interrupts → ... → read`；
- spurious 地址 8 个：317a/31fe/1667/2584/7747/37b5/35bb/235e（低 16 位），全部 `ffff60400839xxxx`，落在 4 个相邻页帧（代数 E 节），地址聚集；
- 每块寄存器中 `x27 = 0xffffffd8`（-40，__memcpy 内部位移常数）、`x21 = spurious 地址`、`x24 = spurious−0xa`，一致指向 __memcpy 的读源指针；
- 第 1 块为 `Not tainted`，其后为 `Tainted: G W`（W 由 WARNING 自身引入），排除"带病启动"。

spurious 语义（crash_session_24/25 反汇编实证）：`__do_kernel_fault+72` 调用 `is_spurious_el1_translation_fault` → 对出错地址执行 `AT S1E1R`（软件重放 stage-1 页表遍历）→ `ISB` → 读 `PAR_EL1` → 遍历成功则判定 spurious。即：硬件刚在加载指令上报了 translation fault，而软件重放证明映射有效，硬件 PTW/TLB 报了假 fault。且 vtop（session_7）实证 `ffff604008393170 → 604008393170`，1GB 大页 `VALID|SHARED|AF|NG|PXN|UXN|DIRTY`，映射铁证有效。

### 4.5 RAS 负证据（dmesg grep，exit=1 无匹配）

```
$ grep -n -E 'memory_failure|Machine check|Hardware error|ECC error|corrected|uncorrected|ghes_proc|Error record' vmcore-dmesg.txt
（无匹配）
```

RAS 通路在位（行1262 RAS Extension、行1308 GHES APEI firmware-first、行2176-2177 ghes_edac 接管）却全程静默 → 故障未被任何内存侧 ECC/固件 RAS 机制感知，指向 CPU 核内（无 ECC/奇偶覆盖或覆盖失效的单元）。

## 5. 业务现象【业务现象】

- 机器：Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48，192 核（8 node），768GB，openEuler 24.03 LTS SP3，内核 6.6.0-145.3.23.154.oe2403sp3.aarch64（dmesg 行2、1273、1255、408）。
- 业务负载：`arm0102_kreg_ma` 压测（opendcdiag 框架，父进程 78104），191 个 RU 级任务绑核 0..191 中除 179 外的每核一个（crash_session_15 `ps` 全量核对：kreg 任务覆盖 0..191 全部 CPU 号、唯独无 179；179 上只有 34 个系统任务：swapper/systemd/pmda\*/pmie/pmlogger/sshd/bash 等），179 是保留给系统/监控线程的核。LOAD 146.92 与 191 个压测任务吻合。
- 撞击点进程均为 179 上的监控读者：pmdalinux（PCP，周期读 /proc/interrupts）、irqbalance（周期读 /proc/interrupts）、NetworkManager（ppoll 网络事件，newidle_balance 路径）。压测任务自身未崩，它们不在 179 上。
- 影响：致命 Oops → kdump 重启；此前 8 次 spurious fault 被内核"忽略"，业务面未中断，但 pmdalinux/irqbalance 的 /proc/interrupts 读已反复取到假 fault（数据面完整性已受损 124 秒）。

## 6. 诊断定位过程【诊断定位过程】

### P1 · dmesg 全量勘察（命令+输出见 `dmesg_forensics.txt`）

`wc -l`（2995 行）、boot 指纹、时间线端点、8 次 WARNING 块、RAS 负证据 grep。产出独立 forensics 文件。

### P2 · 崩溃块与 WARNING 块提取

逐块提取 Oops（行2941-2995）与 8 个 WARNING 块的寄存器、call trace、spurious 地址清单（见 §4）。

### P3 · crash 加载与 bt/寄存器（crash_session_1、_2、_8）

`bt` 确认 PC=find_busiest_group+0x140；`dis find_busiest_group`（session_2 行439-521）反汇编关键段：

```
0xffffbda55292ae00 <find_busiest_group+244>:	adrp	x24, 0xffffbda554755000 <node_data+560>
0xffffbda55292ae04 <find_busiest_group+252>:	add	x0, x24, #0x5d0     ; x0 = &__per_cpu_offset[0]
0xffffbda55292ae08 <find_busiest_group+256>:	str	x0, [sp, #8]
...
0xffffbda55292ae34 <find_busiest_group+300>:	ldp	x0, x1, [sp, #8]   ; x0=&__per_cpu_offset[0], x1=sp[16]
0xffffbda55292ae38 <find_busiest_group+304>:	ldr	x2, [x28, #8]
0xffffbda55292ae3c <find_busiest_group+308>:	ldr	x20, [x0, w25, sxtw #3]  ; x20 = __per_cpu_offset[w25]
0xffffbda55292ae40 <find_busiest_group+312>:	mov	w0, w25
0xffffbda55292ae44 <find_busiest_group+316>:	add	x27, x1, x20        ; x27 = &runqueues + offset
0xffffbda55292ae48 <find_busiest_group+320>:	ldr	x23, [x27, #288]    ; ← 致命指令 (0x120=288)
```

同时 `dis` 还原了 +104..+124 处 `adrp x1, 0xffffbda554359000 <cpu_worker_pools>; add x1,x1,#0x6c0; str x1,[sp,#16]`，sp[16] 存的就是 `&runqueues` 静态地址。

### P4 · 符号与 percpu 表实证（crash_session_3、_4、_5、_21、_22）

```
crash> sym runqueues        → ffffbda5543596c0 (D)
crash> sym __per_cpu_offset → ffffbda5547555d0 (D)
crash> p __per_cpu_offset[56]  = 18446676295573233664 = 0xffffc25b2c42e000
crash> p __per_cpu_offset[179] = 18446676295590363136 = 0xffffc25b2d484000
crash> rd 0xffffbda5547555d0 4 → ffffc25b2bcbe000 ffffc25b2bce0000 ...  （表内容非零）
crash> rd 0xffffbda554755790 4 → ffffc25b2c42e000 ...                   （[56] 项直读）
```

### P5 · 崩溃栈槽与真实 rq 定位（crash_session_22、_12、_7）

```
crash> rd ffff8000ce5ab398 8     （sp = ffff8000ce5ab390，dmesg 行2946）
ffff8000ce5ab398:  ffffbda5547555d0 ffffbda5543596c0   ← sp+8/sp+16 两槽完好！
crash> p ((struct rq *)0xffff8000817dd6c0)->cpu      → $1 = 179   （真实 rq(179) 可读）
crash> vtop 0xffffbda5543597e0 → PTE: ffff6057ffffbac8 => 0      （错误目标无映射）
```

即：栈上源数据完好、内存表完好、真实 percpu 目标可读，唯独 x20 寄存器值为 0。

### P6 · 软件成因排除（基于 dmesg/crash 可得证据）

| 假设 | 排除证据 |
|---|---|
| KASAN/KCSAN/KFENCE | dmesg 全文无任何 KASAN/KFENCE 报告（grep exit=1） |
| OOM/内存压力 | 无 `out of memory`/`oom-kill` 记录（grep exit=1）；768GB 内存正常 |
| 热 throttling | 无 thermal 事件（仅注册 governor 的 boot 行） |
| 编译器/内核 bug | 同一 `find_busiest_group+0x140` 是 6.6 热路径；`x20=0` 无软件解释（ldp 栈槽完好、表项非零、add 语义确定）；且 8 次 spurious fault 与 Oops 同核聚集（192 核中仅 179），软件 bug 无法解释核特异性 |
| 并发/竞态（percpu 表被改） | `__per_cpu_offset` 在 CPU 热插拔外只读；179 已上线 347s 无 hotplug 事件；转储中表项完好非零 |
| spectre 缓解序列干扰 | `nospectre_bhb` 已关（行387），无 BHB 序列复杂化 |

RAS 负证据（§4.5）：内存侧 ECC 通路全程无记录。

### P7 · 定位收敛

数值闭合（algebra_out.txt）：
1. `FAR = x27 + 0x120` ✓
2. `x27 = x1 + x20 = ffffbda5543596c0 + 0` ✓
3. `x1` == 栈槽 sp+16 ✓（取数正确）
4. `x20` 应 = `__per_cpu_offset[56]`（w25=0x38 实测）= `ffffc25b2c42e000`，内存实值非零，但寄存器值为 0 ✗
5. `x27`（= runqueues 静态镜像）+0x120 的 PTE=0 → Oops ✓
6. 真实 `rq(179)` = `ffff8000817dd6c0` 有效可读 ✓

→ 故障点收敛到 `LDR x20,[x0,w25,sxtw#3]` 的执行结果：内存↔寄存器之间的数据返回路径（L1D 命中数据→LSU→寄存器堆写回，或该 LDR 的慢速路径）单次静默失败，返回了 0。

## 7. 逻辑链条（寄存器代数闭合与反事实）【逻辑链条】

### 7.1 指令语义（crash_session_2 反汇编 + 手工解码双重验证）

`0xf9409377` 解码（algebra_out.txt A 节）：`size=3,V=0,opc=1,imm12=0x24,Rn=x27,Rt=x23` → `LDR x23, [x27, #0x120]`，与 crash `dis` 一致；`FAR − x27 = 0x120` 精确闭合。出错加载的地址生成无异常，异常在被加载的数据（上游 x20）。

### 7.2 核心闭合等式（Python3 模 2⁶⁴，输出逐位见 `algebra_out.txt`）【实锤】

```
x27 = x1 + x20 = 0xffffbda5543596c0 + 0x0 = 0xffffbda5543596c0   (== dmesg 实测 ✓)
x1 == 崩溃栈 sp+16 槽实测值 ✓（ldp 源完好）
x20 应为 __per_cpu_offset[56] = 0xffffc25b2c42e000（内存实值非零）
```

### 7.3 反事实推演（algebra_out.txt D 节）

| 情形 | x20 | x27 | 后果 |
|---|---|---|---|
| 正常（w25=56） | ffffc25b2c42e000 | ffff8000807876c0 | percpu vmalloc 区，有效映射，无 fault |
| 若 w25=179 | ffffc25b2d484000 | ffff8000817dd6c0 | session_12 已成功读出 rq->cpu=179，无 fault |
| **事实** | 0 | ffffbda5543596c0 | 内核映像静态 percpu 镜像，PTE=0 → Oops |

唯一异常量是 x20=0，其内存源非零（表完好）。

### 7.4 位形态与故障"签名"（algebra_out.txt E/F 节）

- x27 与期望值 XOR popcount=17（若按位翻转解释，17 位同时翻转的 SEU 概率可忽略）；低 12 位完全相同（6c0）：不是位翻转，是加数整体丢失；
- 8 个 spurious 地址聚集在 ffff60400839xxxx 的 4 个相邻页（同一个 seq_file 缓冲区簇）：虚假 fault 不是随机地址，而是同一缓冲区被反复读、反复触发假 fault，与"`__memcpy` 扫描该缓冲区时取数通路间歇报错"自洽；
- 前兆与致命统一解释：同一取数通路，两种表现：(a) 报假 translation fault（可恢复，被 AT 重放纠正后忽略）；(b) 返回假数据 0（静默，酿成解引用致命）。(b) 是 SDC 的严格形态：Silent（无 RAS 记录、无异常）Data（x20）Corruption（x27→x23 链路被污染）。
- 核特异性统计：192 核中 9 次事件全部落在 179 的随机概率为 (1/192)⁸ ≈ 4×10⁻¹⁸（第 9 次是致命事件本身）。核特异性是数学事实，不是印象。

### 7.5 诚实声明（推测边界）

- 无法从 vmcore 直接观测"CPU 流水线内部"，定位到"加载结果错误"是代数排除法的结论（内存侧/栈侧/加法语义三方完好，唯一自由度是 LDR 写回 x20 的结果），这是逻辑必然而非直接观测；
- x20 归零的微架构坐落点（L1D 数据阵列输出、LSU 返回总线、寄存器堆写端口、或 PTW 交互导致的加载丢弃）无法从软件侧区分，统称"取数通路"；
- 8 次 spurious fault 与 x20 事件的"同源"是基于同核聚集 + 同通路语义的强推断（置信度中高），非直接观测。

## 8. 故障根因【故障根因】

CPU179 单核数据取通路（load return path）的静态/间歇性硬件缺陷，产生两类未检测错误：

1. 虚假 translation fault（8 次，223.3–333.3s）：读 `ffff60400839xxxx`（有效 1GB 大页 linear map）时硬件报 level 3 fault，AT S1E1R 重放证明映射有效，PTW/TLB 交互或取数判定的瞬时错误；
2. 加载结果归零（347.1s）：`LDR x20,[x0,w25,sxtw#3]` 应得非零 percpu offset，实际写入 x20 的值为 0，数值型 SDC。

两者均未被 RAS Extension/GHES/EDAC 捕获（负证据），证明缺陷单元不在 RAS 覆盖域内（核内 LSU/寄存器堆/PTW 数据路径，而非 DDR/LLC 等带 ECC 域）。

置信度：高（≈90%）。扣分项：无法进行芯片级直接观测；"前兆与致命同源"为强推断而非观测。

## 9. 启示【启示】（芯片设计与实现，微架构级）

1. 核内数据通路需要端到端保护，而非仅保护阵列。本案内存（DDR ECC 域）、页表（PTW 自身 S1PTW=0 无错）、栈（ldp 源完好）全部正确，错误发生在"数据返回/写回"这一段。传统 ECC 只保护存储阵列，LSU 返回总线、寄存器堆写端口、bypass 网络是无保护盲区。设计启示：load return path 与寄存器堆写回应引入奇偶/ECC 或 residue checking。
2. spurious fault 是可利用的硬件健康遥测信号。内核 6.6 已实现 `is_spurious_el1_translation_fault`（AT 重放）并打印 `Ignoring spurious...`，但仅是 WARNING 后忽略。8 次聚集 = 明确的核健康退化前兆，却在 124 秒后才致命。设计/固件启示：(a) RAS 应将"spurious kernel translation fault"纳入可上报事件（per-core 计数器）；(b) 系统软件可设阈值（如同核 N 次）触发隔离下线：本机若有此机制，179 在 223s 即可被隔离，致命 Oops 可避免。
3. 微架构单元的"静默失败模式"设计审查。x20 整体归零而非位翻转，形态上更像"加载被丢弃/写回被抑制/返回总线全 0"，而非粒子翻转。实现启示：关键控制流数据（percpu 基址、页表基址类指针）的加载，微架构上可用非零不变式校验（如 percpu offset 恒为 kernel 地址形态，0 可被廉检出）：本案若检出"x20 非法形态"即可陷入受控 panic 而非乱飞解引用。
4. 假 fault 与假数据是同一故障的两种面象。PTW 报假 fault（可检测、可恢复）与 load 返回假数据（不可检测）可能共享根因。设计启示：在 DABT 异常路径上，硬件可附带"最近一次 PTW 结果校验"类信息，让软件能区分"映射真不存在"与"遍历结果可疑"。
5. 单核隔离（哨兵核）的运维价值。压测绑核 0..191 留出 179 给系统线程的部署，使故障核恰好承载监控任务、8 次前兆全部被 dmesg 捕获；若 179 也跑满压测，spurious fault 将淹没在负载中。系统设计启示：每节点保留低负载"哨兵核"用于暴露硬件退化，代价小、收益高。

## 10. 处置建议

1. 立即：将 CPU179 标记为可疑核；RMA 该 CPU/单板。同类机器（同批次 BC82AMQA/BIOS 7.48）核查 dmesg 中 `Ignoring spurious kernel translation fault` 的按核聚集性。
2. 短期：部署监控规则：按核统计 `Ignoring spurious kernel translation fault`，同核 ≥3 次/小时即告警并评估隔离（`echo 0 > /sys/devices/system/cpu/cpu179/online`，需评估拓扑可下线性）。
3. 中期：向平台厂商反馈：CPU179 在 347s 内 9 次取数通路异常、RAS 静默，要求提供核级 RAS 遥测（若有 LSU/PTW erratum 计数器则启用）。
4. 证据保全：保留本 vmcore 与 dmesg（10GB + 220KB）及本报告目录全部 crash_session_\*.log 供厂商复析。

## 附录：命令索引（全部取证命令，可复核）

```bash
# ① 开机指纹与 WARNING 统计（详见 dmesg_forensics.txt）
DMESG=/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:39:38/vmcore-dmesg.txt
wc -l $DMESG                                     # 2995
grep -n 'Linux version' $DMESG                   # 行2
grep -n 'DMI:' $DMESG                            # 行1273
grep -n 'smp: Brought up' $DMESG                 # 行1255 (8 nodes, 192 CPUs)
grep -n 'CPU179' $DMESG                          # 行1203-1206
grep -n 'Ignoring spurious' $DMESG               # 8 条（行2582..2897）
grep -n 'WARNING: CPU' $DMESG                    # 8 条 + Tainted 演化
grep -n -E 'memory_failure|Machine check|Hardware error|ECC error|corrected|uncorrected' $DMESG
                                                 # 无匹配 (exit=1) — RAS 负证据
grep -n -iE 'out of memory|oom-kill|Killed process' $DMESG   # 无匹配
grep -n -E 'KASAN|KCSAN|KFENCE' $DMESG           # 无匹配

# ② 崩溃块与 WARNING 块
sed -n '2941,2995p' $DMESG                       # 致命 Oops 完整块
sed -n '2582,2625p' $DMESG                       # WARNING #1 完整块
sed -n '2807,2860p' $DMESG                       # irqbalance WARNING 块

# ③ crash 会话（vmlinux=/tmp/vmlinux-0102, vmcore=.../vmcore）
#    全部日志：crash_session_1..25.log（_21.._25 为本次新增）
crash /tmp/vmlinux-0102 <vmcore> -s << 'EOF'
sys
bt                                              # session_1
bt -t                                           # session_8
dis find_busiest_group                          # session_2（关键反汇编）
dis -l 0xffffbda552834680 24                    # session_24 __do_kernel_fault
dis is_spurious_el1_translation_fault.constprop.0   # session_25（AT S1E1R 判定）
sym runqueues                                   # ffffbda5543596c0 (session_4)
sym __per_cpu_offset                            # ffffbda5547555d0
p __per_cpu_offset[56]                          # 0xffffc25b2c42e000 (session_4/5/22)
p __per_cpu_offset[179]                         # 0xffffc25b2d484000
rd 0xffffbda5547555d0 4                         # 表直读 (session_5/22)
rd ffff8000ce5ab398 8                           # 崩溃栈 sp+8/sp+16 槽 (session_22)
p ((struct rq *)0xffff8000817dd6c0)->cpu        # = 179 (session_12)
vtop 0xffffbda5543597e0                         # PTE => 0 无映射 (session_7)
vtop 0xffff604008393170                         # 1GB 大页 VALID (session_7)
rd 0xffff604008393170 4                         # spurious 页内容 (session_7)
ps | grep -E "pmdalinux|irqbalance|NetworkManager"   # session_14
ps                                              # session_15（191 kreg 任务核对）
EOF

# ④ 代数复算（禁止手算）
python3 algebra.py > algebra_out.txt            # A..G 七节闭合（本报告 §7 全部依据）
```

### 崩溃指令与关键寄存器速查

| 项 | 值 | 来源 |
|---|---|---|
| 出错指令 | `f9409377` = `LDR x23,[x27,#0x120]` | dmesg 行2992 / dis +320 |
| FAR | ffffbda5543597e0 | dmesg 行2941 |
| x27 | ffffbda5543596c0（= &runqueues 静态） | dmesg 行2943 / sym |
| x1 | ffffbda5543596c0（完好，== sp+16 槽） | dmesg 行2953 / rd |
| x20 | 0（应为非零 percpu offset） | dmesg 行2943 |
| x25 | 0x38 = 56（w25 索引） | dmesg 行2942 |
| __per_cpu_offset[56] | ffffc25b2c42e000（内存实值非零） | session_22 rd |
| __per_cpu_offset[179] | ffffc25b2d484000 | session_4 |
| 真实 rq(179) | ffff8000817dd6c0（rq->cpu=179 可读） | session_12 |

*报告生成于独立诊断会话（2026-09-05）。全部 crash 输出可由附录命令复现；代数结论可由 `python3 algebra.py` 复算。*

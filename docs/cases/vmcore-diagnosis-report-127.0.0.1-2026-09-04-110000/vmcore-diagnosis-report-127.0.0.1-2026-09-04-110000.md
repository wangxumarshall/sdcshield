# CPU 179 / sftp-server 转储深度诊断报告（第 1 次独立重研究）

## ——本案最独特的法证特征：一条 `ldr x20, [x0, x25, sxtw #3]` 的装载结果塌缩为全零，且"内存真值非零"与"寄存器为零"可以被直接对照——这是教科书级的 load path SDC 实锤

| 项目 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-11:00:00/vmcore`（46.9 GB，Kdump compressed v6，PARTIAL DUMP） |
| dmesg | 同目录 `vmcore-dmesg.txt`（2634 行） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 CPU（Kunpeng HIP08/TaiShan 系，8 NUMA 节点），768 GB |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP，KASLR 开启，crashkernel=1024M,high |
| 崩溃时刻 | uptime 1456.23 s（墙钟 2026-09-04 10:59:16，由 audit 时间戳锚定，与目录名 11:00:00 一致） |
| 受害进程 | PID 56263 `sftp-server`（sshd 会话链 9677→56258→56262 的子进程），TASK_WAKING (PANIC) |
| 崩溃签名 | `find_busiest_group+0x140`，`Unable to handle kernel paging request at ffffd77069c697e0`，ESR=0x96000006（DABT, FSC=0x06 level 2 translation fault） |
| 前兆异常 | **无**。崩溃前 1364.9 秒（约 22.8 分钟）内核日志完全静默，零 WARNING/BUG/硬件错误 |
| 负载背景 | 191 个 CPU 的当前任务全部是 `neon_rot_ldr_at`（opendcdiag 派生的每核 NEON load 压测进程），系统满载 |
| 结论一行 | `ldr x20` 装载 `__per_cpu_offset[97]`（内存真值 0xffffa89017090000）时结果塌缩为 0，导致后续 `add x27,x1,x20` 得到未映射的 percpu 模板段地址，`ldr x23,[x27,#0x120]` 触发 L2 缺页 → 内核 panic。装载塌缩本身【实锤】，微架构归因于 load 数据通路单粒子瞬态受扰【强推】 |

---

## 1. 执行摘要

1. **现象**：开机 24 分钟后，CPU 179 上的 sftp-server 进程在一次 `write()` 系统调用（写 SFTP 管道）中触发 `schedule()` → `newidle_balance()` → `find_busiest_group()`，在负载统计循环里触发 level 2 translation fault，内核 panic，kdump 成功落盘。
2. **签名**：致命指令 `ldr x23, [x27, #0x120]`（Code 窗口末帧 `f9409377`），FAR=0xffffd77069c697e0；x27 由上一条指令 `add x27, x1, x20` 计算，其中 x1=&runqueues（percpu 模板段链接地址），x20=0。
3. **闭合验证结果**：x20 的唯一来源是 `ldr x20, [x0, x25, sxtw #3]`，其中 x0=&__per_cpu_offset[0]（栈上 sp+8 实测）、x25=97。**crash 直接读出 `__per_cpu_offset[97]` 的内存真值为 0xffffa89017090000（非零），而崩溃时寄存器 x20=0**——装载源完好、装载结果为零，装载塌缩实锤。
4. **反事实推演**：若 x20 装载正确，x27 = 0xffff800080cf96c0 = per_cpu(runqueues, 97)（与 crash `px runqueues[97]` 完全一致，该区已映射），后续装载读 rq(97).cfs.avg.load_avg=1044，内核正常继续。塌缩后 x27 落在 `.data..percpu` 模板段——该段运行时本就无映射（crash `vtop` 与 dmesg 页表走查独立证实 PMD=0），缺页是必然。
5. **置信级别**：装载塌缩为 0【实锤】（内存真值 vs 寄存器直接对照，可复核）；微架构归因"load 数据通路（L1D→RF）单次受扰"【强推】（无法从软件侧区分 L1D 阵列/互连总线/寄存器文件写入哪个环节）；与同时在跑的 neon load 压测的因果关联【假设】。
6. **处置**：本机 192 核满载运行 opendcdiag 的每核 NEON load 压测（neon_rot_ldr_at）约 24 分钟后出现一次单发、无前兆、无 RAS 报告的装载通路 SDC。建议保留此 dump 作为 load-path SDC 标本；对该机器做重复压测复现率统计；若无复现，按瞬态单粒子事件处理并加强 load 通路 RAS 设计（见第 9、10 节）。

## 2. 证据规则与方法

- **证据源**：仅使用 ①`vmcore-dmesg.txt` 原文；②crash 8.0.4-17.oe2403sp4 + namelist `/tmp/vmlinux-0102`（ELF aarch64，BuildID 276194e5f356f9c4bc570bb0a750394d7b768035，带 debug_info）对 vmcore 的实测输出。未阅读任何既有诊断文档，保证独立性。
- **诚实铁律**：报告所有数字均来自实际命令输出，取证全文见附件 `dmesg_forensics.txt`（dmesg 侧）与 `crash_forensics.txt`（crash 侧 30 个会话）。vmcore 为 PARTIAL DUMP，少量页被排除（`rd: page excluded`），凡影响取证处均如实标注（本案所有关键地址的读取均成功）。
- **三级置信**：【实锤】= 可直接复核的实测对照；【强推】= 多源收敛但缺芯片级直接对照；【假设】= 软件侧无法验证，给出验证途径。
- **64 位运算**：全部地址加减用 Python3 脚本模 2^64 计算，见 `algebra.py` / `algebra_out.txt`。
- **工具清单**：crash（sys/panic/bt/bt -t/set/mach/dis/rd/vtop/struct/p/sym/ps/runq/gdb list/info line）、grep/awk/sed、python3。
- **加载说明**：46.9 GB vmcore 每次加载约 1–3 分钟，全部采用 `-i cmdfile` 批处理 + timeout 590，共 30 个会话，无一超时失败；`kmem -s` 因 PARTIAL DUMP 耗时风险未执行（如实记录，本案不需要它）。

## 3. 本次开机时间线【时间线】

| uptime | 墙钟（由 audit 锚定推算） | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0.000000 | 10:34:59.98 | 内核启动，physical CPU 0x80000，KASLR on | 1 | 实锤 |
| 0.000000 | — | crashkernel reserved: 0x60575fe00000-0x60579fe00000 (1024 MB) | 122 | 实锤 |
| 0.351415 | — | `smp: Brought up 8 nodes, 192 CPUs` | 1255 | 实锤 |
| 0.351630 | — | `CPU features: detected: RAS Extension Support` | 1262 | 实锤 |
| 0.569007 | — | `GHES: APEI firmware first mode is enabled`（固件先行的硬件错误上报已就绪——之后全程零报告，负证据更有效） | 1307 | 实锤 |
| 1.505157 | — | `EDAC MC0: Giving out device to module ghes_edac`（ghes_edac 接管 32 DIMM） | 2174-2175 | 实锤 |
| 21.844830 | 10:35:21.82 | audit(type=1403) 时间戳 1788489321.820 —— 墙钟锚点 | 2484 | 实锤 |
| 22.051056 | — | systemd 进入 system 模式，Hostname `localhost0102` | 2485-2495 | 实锤 |
| 29.226640 | — | EXT4-fs (sda2) 挂载（root 文件系统就绪） | 2575 | 实锤 |
| 39.259987 | — | firewalld memfd 警告（唯一一条"警告"，属应用提示非内核异常） | 2578 | 实锤 |
| 91.300631 | 10:36:31 | `block dm-2: the capability attribute has been deprecated` —— **最后一条普通内核日志** | 2579 | 实锤 |
| （91.3 → 1456.2） | （10:36:31 → 10:59:16） | **日志静默 1364.93 s**：零 WARNING、零 BUG、零 Oops、零硬件错误（awk 全区间扫描无一行输出） | — | 实锤 |
| 1456.227941 | 10:59:16.20 | `Unable to handle kernel paging request at virtual address ffffd77069c697e0` | 2580 | 实锤 |
| 1456.303983 | — | `Internal error: Oops: 0000000096000006 [#1] SMP` | 2593 | 实锤 |
| 1456.412409 | — | CPU 179 / PID 56263 / sftp-server 现场打印 | 2598 | 实锤 |
| 1456.631808 | — | `SMP: stopping secondary CPUs` | 2630 | 实锤 |
| 1456.641244 | 10:59:16.64 | `Starting crashdump kernel...` → `Bye!`（与目录名 11:00:00 落盘时间衔接） | 2633-2634 | 实锤 |

> crash `sys` 独立给出的 `DATE: Fri Sep 4 10:59:16 CST 2026`、`UPTIME: 00:24:16` 与上表推算完全一致，双重印证。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（vmcore-dmesg.txt 2580–2634 行，全文照录）

```
[ 1456.227941] Unable to handle kernel paging request at virtual address ffffd77069c697e0
[ 1456.236591] Mem abort info:
[ 1456.240085]   ESR = 0x0000000096000006
[ 1456.244536]   EC = 0x25: DABT (current EL), IL = 32 bits
[ 1456.250551]   SET = 0, FnV = 0
[ 1456.254305]   EA = 0, S1PTW = 0
[ 1456.258147]   FSC = 0x06: level 2 translation fault
[ 1456.263729] Data abort info:
[ 1456.267308]   ISV = 0, ISS = 0x00000006, ISS2 = 0x00000000
[ 1456.273497]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[ 1456.279249]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[ 1456.285262] swapper pgtable: 4k pages, 48-bit VAs, pgdp=00004021dc494000
[ 1456.292667] [ffffd77069c697e0] pgd=10006057fffff403, p4d=10006057fffff403, pud=10006057ffffe403, pmd=0000000000000000
[ 1456.303983] Internal error: Oops: 0000000096000006 [#1] SMP
[ 1456.310259] Modules linked in: binfmt_misc nft_fib_inet ... hisi_trng_v2
[ 1456.412409] CPU: 179 PID: 56263 Comm: sftp-server Kdump: loaded Not tainted 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1
[ 1456.423559] Hardware name: Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026
[ 1456.432007] pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[ 1456.439674] pc : find_busiest_group+0x140/0xb60
[ 1456.444916] lr : find_busiest_group+0x11c/0xb60
[ 1456.450149] sp : ffff8000e6f6b740
[ 1456.624584] Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377) 
[ 1456.631808] SMP: stopping secondary CPUs
[ 1456.641244] Starting crashdump kernel...
[ 1456.646340] Bye!
```

要点：读访问（WnR=0）、当前 EL 的数据中止（EC=0x25）、**level 2 translation fault（FSC=0x06）**——不是权限错、不是对齐错，而是页表走查在第 2 级（PMD）断掉：pgd/pud 均为有效表描述符（…403），pmd 读出为 0。

### 4.2 全量寄存器（x0–x30，dmesg 2604–2612 行）

```
x29: ffff8000e6f6b8c0  x28: ffff8000e6f6b770  x27: ffffd77069c696c0
x26: ffff604003ed3900  x25: 0000000000000061  x24: ffffd7706a065000
x23: 0000000000000564  x22: ffff604003ed3a80  x21: ffffd7706a05fcb0
x20: 0000000000000000  x19: ffff8000e6f6b950  x18: 0000000000000000
x17: 0000000000000000  x16: 0000000000000000  x15: 0000aaaad3871150
x14: 0000000000000000  x13: 0000000000000000  x12: 0000000000035bb2
x11: 00000000000000c2  x10: 0000000000000097  x9 : ffffd7706823ae58
x8 : ffff8000e6f6b7c8  x7 : 0000000000000000  x6 : 0000000000000061
x5 : fffffffe00000000  x4 : 0000000000000001  x3 : 0000000000000021
x2 : 0000000000012740  x1 : ffffd77069c696c0  x0 : 0000000000000061
```

关键读法（后文逐项闭合）：
- `x27 = ffffd77069c696c0` 与 `x1` 完全相同——异常；
- `x20 = 0`——本案元凶；
- `x25 = x0 = x6 = 0x61 = 97`（被遍历的 CPU 号），`x10 = 0x97 = 151`、`x11 = 0xc2 = 194`（位图迭代中间量）；
- `x22/x26 = ffff6040_03ed3xxx` 形态——sched_group 指针（实测确认）；
- `x23 = 0x564 = 1380`——负载累加的部分和（正常运行值，非坏值）；
- `x9 = ffffd7706823ae58 = find_busiest_group+0x158`——函数内暂存的返回地址（+336 处指令地址，非异常）。

### 4.3 Call trace（完整，dmesg 2612–2628 行）

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
```

crash `bt` 与此完全一致，并补出异常处理侧：`el1h_64_sync → do_mem_abort → do_translation_fault → do_bad_area → __do_kernel_fault → die_kernel_fault → die → crash_kexec → machine_kexec`。上下文解读：用户态 sftp-server 调用 `write()` 写管道（SFTP 数据回传通道），`pipe_write` 中管道满/需等待而 `schedule()` 放弃 CPU；`pick_next_task` 发现本队列为空，进入 `newidle_balance`（idle 前的负载均衡），`find_busiest_group` 在遍历调度组统计各 CPU 负载时崩溃。**崩溃点是全内核执行频度最高的热路径之一。**

### 4.4 前兆异常原文（如有）

**没有。** `grep -n -E 'WARNING|BUG:|Oops|cut here|spurious|segfault'` 在全量 2634 行中仅命中 panic 块自身。从最后一条普通日志（91.300631 s）到 panic（1456.227941 s）之间 1364.93 秒，awk 逐行扫描时间戳区间无任何输出。**零前兆、单发、突发**——这是瞬态硬件事件的典型形态，与持续性软件 bug 的反复报错形态相反。

### 4.5 RAS 负证据（dmesg grep）

对以下关键词做全量 grep，除基础设施注册行外零命中：

- `hardware error` / `Hardware error`：无
- `machine check` / `Machine check`：无
- `memory failure`：无
- `ECC` / `corrected` / `uncorrected` / `uncorrectable`：无
- `thermal` / `throttl`：无（仅 thermal governor 注册行）
- `ras`：仅 `CPU features: detected: RAS Extension Support`（能力声明，非事件）

而 RAS 上报基础设施在本机是**就绪**的：RAS Extension 已检测（行 1262）、GHES APEI firmware-first 已启用（行 1307）、ghes_edac 已接管 32 个 DIMM（行 2174-2175）。**也就是说：若有任何被固件/硬件捕获的内存 ECC 事件，本机具备上报路径，但全程零报告。**这与"错误未被任何检测机构捕获（非内存阵列故障，而是片内通路瞬态）"的推论自洽。

## 5. 业务现象（受害进程是什么业务、调度上下文解读）

- **受害进程链**（crash `ps` 实测）：`sshd(9677, 常驻) → sshd(56258, 每 CPU 97) → sshd(56262) → sftp-server(56263)`。这是一条标准的 SSH SFTP 会话：远端用户通过 sshd 建立 SFTP 通道，sftp-server 把文件数据写回管道由 sshd 加密转发。PID 56258 运行于 CPU 97——与崩溃代码正在遍历的 CPU 编号（x25=97）同一个核，纯属调度巧合，无因果含义。
- **崩溃时机**：sftp-server 在 `write()`（SFTP 数据流写出）路径上因管道写阻塞而 `schedule()`；此时本核运行队列已空，调度器在切换到 idle 前做 `newidle_balance`，为 sftp-server 寻找 busiest 组。**受害的不是负载均衡本身，而是"恰好排在压测间隙的这条用户业务"**——它是被 SDC 击中的无辜旁观者。
- **系统背景**（crash `runq` + `ps` 实测，本案最具信息量的业务发现）：**191 个 CPU（除崩溃的 CPU 179 外全部）的当前任务都是 `neon_rot_ldr_at`**（PID 58051 起，每核一个，父进程 `opendcdiag`(57625) ← `bash`(57622) ← init）。`opendcdiag` 是 openEuler 平台的内存在线诊断工具，其派生的 `neon_rot_ldr_at` 顾名思义是 **NEON 旋转/循环 load 地址压测**进程——即全机正在做**负载通路（load path）满载压测**。LOAD AVERAGE 15.67（1 分钟）与压测进程短暂被调度器均衡的状态吻合。
- **时间关联**：压测满载运行约 24 分钟（uptime 1456 s）后，出现一次无前兆的 load 结果塌缩。压测程序本身未报错（其结果文件在 dump 外，无法从内存侧复核）。

## 6. 诊断定位过程【诊断定位过程】

### P1 dmesg 勘察

提取开机指纹（内核版本/命令行/192 CPU/768 GB/8 节点/crashkernel 1024M）、崩溃主块（4.1–4.3 节）、前兆扫描（4.4 节零前兆）、RAS 负证据（4.5 节零报告）、时间线（第 3 节）。初步判断：无前兆 + 无 RAS 报告 + 调度器亿频热路径 + 特定 CPU 单发 → 软件成因概率极低，转入寄存器级闭合验证。

### P2 崩溃块提取与指令解码

Code 窗口 `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` 逐条解码（python3 位运算，见 algebra.py）：

| 地址偏移 | 编码 | 指令 | 备注 |
|---|---|---|---|
| +0x128 | f9400782 | `ldr x2, [x28]` | 读局部 sums |
| +0x12c | f879d814 | `ldr x20, [x0, x25, sxtw #3]` | **x20 的来源** |
| +0x130 | 2a1903e0 | `mov w0, w25` | 覆盖 x0=97（解释了崩溃时 x0=0x61） |
| +0x134 | 8b14003b | `add x27, x1, x20` | **x27 的来源** |
| +0x138 | f9409377 | `ldr x23, [x27, #0x120]` | **致命指令**（pc=+0x140） |

crash `gdb info line` 把 +0x134 定位到 `kernel/sched/fair.c:12050`（update_sg_lb_stats 内联循环体），把 +0x138 定位到 `fair.c:5024`（cpu_util 内联）。该循环即 `for_each_cpu_and(cpu, group_span, ...)` 逐 CPU 累加 `rq->cfs.avg.load_avg`——崩溃时 x25=97 正是循环变量（CPU 号），x23=0x564=1380 是已累加的部分和（栈上实测 0x12740、0x3bf1e 等一批累加中间量佐证）。

### P3 crash 加载与结构勘察

- `sys`：PARTIAL DUMP，192 CPU，UPTIME 00:24:16，与 dmesg 一致；
- `bt`/`bt -t`：与 dmesg call trace 互证；
- `p __per_cpu_offset[179]`、`px runqueues`：拿到 CPU179 的 rq 地址 0xffff8000817dd6c0；
- `struct sched_domain 0xffff604004407000`（rq(179).sd）：MC 域（level=1, span_weight=24, flags=0x1217），父域 NUMA（level=4, span_weight=48, flags=0x6417）；
- MC 域 groups 环（0x…ec60c0→ec61e0→ec6cc0→ec67e0 闭合）与 NUMA 域 groups 环（0x…ed3600；以及 x22/x26 所在的 ed3a80→ed3060→ed3900→ed3360 四组环）逐个 `rd`/`struct` 实测：**全部 next 指针有效、ref/group_weight/sgc 字段合理，共享调度结构完好**——排除"共享内存被写坏"。

### P4 内存真值对照（决定性一步）

1. `sym 0xffffd77069c696c0` → **`runqueues (D)`**：x1/x27 就是 `&runqueues`——内核 `.data..percpu` 模板段的链接地址；
2. `sym 0xffffd7706a0655d0` → **`__per_cpu_offset (D)`**：x0（装载 x20 的基址）就是 `&__per_cpu_offset[0]`；
3. 栈取证（`rd ffff8000e6f6b740 32`）：**sp+8 = 0xffffd7706a0655d0、sp+16 = 0xffffd77069c696c0**——`ldp x0,x1,[sp,#8]` 的两个源在栈上完好无损，排除"基址来源被扰"；
4. `rd 0xffffd7706a0658d8`（=&__per_cpu_offset[97]）→ **0xffffa89017090000，非零**；数组 192 项全量 rd：全部非零、单调递增（每 CPU 差 0x22000），无一项为 0；
5. **对照**：装载源内存真值 0xffffa89017090000 ≠ 寄存器 x20 = 0 → 装载塌缩实锤；
6. `p __cpu_online_mask`（bits[0..2] 全 1，覆盖 CPU 0–191）、`rq(97)->online = 1`、`rq(97)->curr` 存在、`rq(97)->sd = 0xffff40200402c400` 正常——**CPU 97 在线**，排除"离线 CPU 的 percpu offset 为 0"的唯一软件解释；
7. `vtop` 双重对照：`__per_cpu_offset` 所在页四级映射完整（PMD=10006057ffffa403 → PTE → 有效页）；而塌缩后的 x27+0x120 落入的模板段 PMD=0——与 dmesg 页表走查（行 2608）完全一致。**ARM64 的 `.data..percpu` 模板段运行时本就不建立映射**（各 CPU 实例在 0xffff800080xxxxxx 动态区），所以塌缩地址的 L2 fault 是"必然发生的正确报错"，不是页表被破坏；
8. 反事实验证：`&runqueues + __per_cpu_offset[97] = 0xffff800080cf96c0`，与 crash `px runqueues[97]` 完全相等；`rd 0xffff800080cf97e0`（=rq(97)+0x120=cfs.avg.load_avg）= 0x414=1044，非零且页有效。**若 x20 正确，内核绝不会崩。**

### P5 软件成因排除

- **排除内核 bug**：`find_busiest_group`/`update_sg_lb_stats` 是调度器核心热路径，192 核 × 每秒千次量级 newidle balance，仅开机 24 分钟就执行了千万次量级；同一指令序列若存在确定性软件缺陷，应表现为可复现、多核散布、有前兆的崩溃，与本案"单发、无前兆、特定核、特定瞬间"完全不符。
- **排除 `__per_cpu_offset[97]` 真为 0**：内存真值实测非零（P4-4），且 CPU97 在线（P4-6）。
- **排除栈/基址被扰**：栈上 sp+8/sp+16 实测完好（P4-3）。
- **排除共享结构写坏**：调度域/组环全部实测完好（P3）。
- **排除页表损坏**：模板段 PMD=0 是固有状态而非被清（同 PGD/PUD 下相邻 PMD 完好，vtop 双址对照）。
- **排除页表走查（PTW）受扰**：PTW 读出（pmd=0）与该段真实状态一致，PTW 工作正常。
- **收敛**：唯一与全部证据相容的解释是 **`ldr x20, [x0, x25, sxtw #3]` 这一次装载，其 64 位结果在从数据通路到寄存器文件的某个环节被替换成了全零**。

### P6 定位收敛

微架构定位：出错环节在 **load 数据通路**（L1D 命中/缺失路径 → 互连 → 寄存器文件写入口的某一点），而非 ALU（add 无误）、非 PTW（走查正确）、非内存阵列（真值完好，且 GHES/EDAC 零报告）。错误形态为**干净的 64 位全零**——不是翻转 1 位、不是半字交换，而是整字塌缩，这种"zero-collapse"形态与数据通路上的瞬态（如位线/采样窗口受扰导致全 0 采样、或填充缓冲条目被错误驱逐后以零填充）更为相容。精确到具体硬件环节（L1D 阵列 vs 总线 vs RF 写口）超出软件可见性，记【假设】并给出验证途径（第 9 节）。

## 7. 逻辑链条

### 指令语义与闭合等式（全部实测，algebra.py 复算）

```
闭式①【实锤】 FAR == x27 + 0x120
        0xffffd77069c696c0 + 0x120 = 0xffffd77069c697e0（模 2^64）== dmesg 2580 行 FAR
闭式②【实锤】 x27 == x1 + x20   （crash dis +316: add x27, x1, x20）
        0xffffd77069c696c0 + 0 == 0xffffd77069c696c0（ALU 计算无误：加 0 直通）
闭式③【实锤】 x1 == &runqueues  （crash sym；栈 sp+16 实测同值 → ldp 源完好）
闭式④【实锤】 x20 == 0          （dmesg 寄存器现场）
闭式⑤【实锤】 mem[&__per_cpu_offset[97]] == 0xffffa89017090000 ≠ 0
        （crash rd 0xffffd7706a0658d8；x20 的唯一装载源）
  ⇒ 装载塌缩：源非零，结果为零【实锤】
闭式⑥【实锤】 反事实：x20_correct = 0xffffa89017090000
        x27' = &runqueues + x20_correct = 0xffff800080cf96c0
        == crash px runqueues[97]（逐位相等）
        且 [x27' + 0x120] = rq(97).cfs.avg.load_avg = 1044（页有效，非零）
  ⇒ 若装载正确，后续指令正常执行，内核不崩
闭式⑦【实锤】 塌缩地址落入 .data..percpu 模板段（PMD 固有为 0，vtop 与 dmesg 双证）
  ⇒ FSC=0x06 level 2 translation fault 是塌缩的必然下游，而非独立故障
```

### 反事实推演

把 x20 换回正确值，整条指令流无任何异常：x27 指向 CPU 97 的 percpu rq，`ldr x23,[x27,#0x120]` 取到 1044，`add x2,x2,x23` 继续累加负载统计。**故障的全部充分必要条件就是"x20 为 0"这一个事实**，而 x20 为 0 的全部充分必要条件是"那次装载返回了 0"。链条上再无第二处可疑点。

### 诚实声明

- 本案 vmcore 为 PARTIAL DUMP：`.data..percpu` 模板段整段被排除（`rd` 返回 page excluded），故无法用 crash 直接读 `&runqueues` 处内容——但这不影响任何结论，因为该段内容在推演中只作为"未映射地址"使用，其未映射状态由 vtop 的页表走查独立证实。
- x20=0 的直接观测只有 dmesg 寄存器现场这一份；"装载塌缩"是从（源真值非零 + 指令唯一性 + 栈源完好 + 软件解释全排除）收敛出的结论，属逻辑实锤而非物理实锤。
- "与 neon_rot_ldr_at 压测同源"仅为时空关联，不构成因果证明。
- 微架构环节（L1D/总线/RF）的进一步区分超出软件可见性，未做断言。

## 8. 故障根因（微架构级结论 + 置信级别）

**结论**：CPU 179 在执行 `find_busiest_group`（fair.c:12050 内联循环，内含 fair.c:5024 cpu_util 展开）遍历调度组内 CPU 97 的负载时，指令 `ldr x20, [x0, x25, sxtw #3]`（从 `__per_cpu_offset[97]` 装载 percpu 基址偏移）**单次装载结果塌缩为全零**——内存源真值 0xffffa89017090000 完好、寄存器却得到 0。该零值经 `add x27,x1,x20`（加 0 直通）传染为基址 x27=&runqueues（percpu 模板段链接地址，运行时无映射），使下一条 `ldr x23,[x27,#0x120]` 在未映射地址 0xffffd77069c697e0 上触发 level 2 translation fault，panic 落盘。

- **故障事件定性**：SDC（Silent Data Corruption）的一次"侥幸显性化"——装载塌缩本身是无声的，只因塌缩值恰好是"用作指针的偏移量"才把静默错误变成可见缺页。若塌缩发生在数值型字段（如本函数中 x23=load_avg 的累加路径），将无声地产生错误负载统计并影响调度决策，无人察觉。
- **微架构定位**：load 数据通路（L1D → 互连/填充缓冲 → 寄存器文件写入口）瞬态受扰，64 位结果整体替换为全零；ALU、PTW、页表、内存阵列、缓存一致性均排除。
- **置信**：
  - 装载塌缩为 0（寄存器值 ≠ 内存真值）：**【实锤】**（crash rd 与 dmesg 寄存器直接对照，任何人可用附件命令复核）
  - 归因于 load 数据通路瞬态（而非软件/共享内存/页表）：**【强推】**（全部软件成因逐一实测排除；形态与 zero-collapse 瞬态一致）
  - 受扰物理环节精确到 L1D 阵列/总线/RF 某一点：**【假设】**（软件侧不可分辨）
  - 与 neon_rot_ldr_at 满载压测同因：**【假设】**（时空强关联，缺直接证据）

## 9. 启示

### 9.1 微架构定位的意义

本案展示了一个罕见的完整证据闭环：**"寄存器现场 + 内存真值 + 指令语义 + 反事实"四位一体**，把一处内核崩溃无损地回推成一次单条装载的 zero-collapse。对芯片验证团队而言，这意味着：服务器级 SDC 并非不可归因——只要 kdump 保留了精确寄存器现场且坏值可回溯到唯一装载源，软件侧就能把故障约束到"一次 load、一个通路、一个周期"。本案中 load 结果**整字为零**而非位翻转，是比单粒子翻转更强的通路级信号（更像是通路仲裁/驱逐/采样层面的瞬态，而非存储单元扰动），这一形态信息对物理失效分析（PFA）有直接指向价值。

### 9.2 芯片设计与实现启示（具体、可落地）

1. **load 通路数据完整性保护**：本案 zero-collapse 发生在 `__per_cpu_offset` 这类"关键基址表"的装载上，说明现有 L1D ECC/parity 未覆盖（或未检出）通路级瞬态。建议：对 load 返回路径（L1D 输出寄存器 → RF 写口）增加端到端 parity 重算或 lane-wise ECC 延伸，使通路瞬态可检出而非静默传递。
2. **关键数据结构的冗余校验**：`__per_cpu_offset` 是全内核地址计算的总根（192 项、每项被亿万次装载）。这类"高频热点基址表"值得：① 物理布局上做奇偶行交错或双拷贝只读镜像（装载时双读比对，开销可控——每 CPU 一项，访问局部性极好）；② 或至少在编译期/链接期把表放入带 ECC scrub 的聚合页，并让固件将其列入 patrol scrub 高频名单。
3. **zero-collapse 的定向检出**：全零是 SDC 里最危险也最好防的形态（合法值几乎从不会是全零）。建议在关键指针派生路径（如 percpu 基址合成）引入廉价的非零断言（`BUG_ON(!offset && cpu_online)` 量级的内核侧护栏，或硬件侧"指针类装载结果为 0"的可选 trap 选项），把本案这类"侥幸显性化"变成"必然快速失败"。
4. **fail-fast 与 silence 的权衡设计**：本案若塌缩的是 load_avg 数值，系统会静默带病运行。芯片可在 RAS 架构上提供"通路检出疑点计数器"（即使不可纠正也记录疑似瞬态的次数/CPU/周期窗口），让固件把"零报告"变成"有痕迹"，为现场决策（继续运行 vs 计划性下线）提供依据。
5. **DFT/在线测试钩子**：opendcdiag 类压测已在做 load 通路满载验证，建议芯片提供配对的**在线通路自检指令**（如受控的 load-verify 微操作），使"压测期间"与"自检期间"可以交叉印证，而不是只能等故障发生。
6. **kdump 可靠性设计**：本案 PARTIAL DUMP 排除了 percpu 模板段，恰好不影响定案，但若塌缩源在更冷的数据段就会断链。建议 dump 策略显式保留 `.data..percpu` 模板段、`__per_cpu_offset`、调度域/组分配区这几类"诊断根数据"（总量不过几十 MB），把"能不能定位"从运气变成设计。

### 9.3 对系统软件/RAS 的启示

1. **调度器对 percpu 基址的防御**：`__per_cpu_offset[cpu]` 在 cpu 在线时恒非零——内核可以零成本地为此建立不变式（如构建期 `alt` 检查或 debug 选项），使任何一次同类塌缩都变成带定位信息的即时 BUG，而不是等下游随机缺页。
2. **GHES/EDAC 零报告的正确解读**：本案 RAS 栈完整就绪却零事件，这本身是证据（错误未经内存阵列层级），不该被当作"硬件无恙"的安慰剂。RAS 报告应区分"未检测"与"未发生"。
3. **SDC 台账**：建议把本案登记为 load-path zero-collapse 标本（含 46.9 GB dump + 完整寄存器现场），同类事件跨机器聚合统计（发生率 vs 机型/固件/压测类型）是判断系统性设计缺陷 vs 散发瞬态的唯一可行途径。

## 10. 处置建议

1. **短期（本机）**：核查 opendcdiag 本轮压测的结果文件（neon_rot_ldr_at 各核是否检出校验失败）；如压测侧也有异常记录，两相对照可将【假设】升级；对 CPU 179 与 CPU 97 所在物理核做隔离复跑（相同压测 24h × 3），观察复现率。
2. **中期（平台）**：按 9.2-2/9.3-1 落实 `__per_cpu_offset` 类热点基址表的护栏；为后续内核加入"在线 CPU 的 percpu offset 非零"不变式检查。
3. **长期（硬件跟踪）**：若同型机器再现同类 zero-collapse（尤其伴随 load 压测），按 9.2-1 的通路覆盖缺口立项；对涉事 CPU 开 RAS 疑点计数观察窗。
4. **本 dump 处置**：作为完整闭环的 load-path SDC 标本归档保留；报告附件（dmesg_forensics.txt / crash_forensics.txt / algebra.py / algebra_out.txt）可独立复核全部结论。

---

## 附录：命令索引（全部取证命令，可复核）

**dmesg 侧**（附件 `dmesg_forensics.txt`）：

```
wc -l / head -60 / sed -n '2578,2634p' vmcore-dmesg.txt          # 崩溃主块
grep -n -E 'WARNING|BUG|Oops|cut here|spurious|...|segfault'     # 前兆扫描（零命中）
grep -n -iE 'hardware error|machine check|memory failure|ECC|corrected|...'  # RAS 负证据
awk -F'[][]' '{ts=$2+0; if (ts>91.4 && ts<1456) print ...}'      # 静默窗口扫描（零输出）
grep -n 'Detected VIPT I-cache'（192）/ 'smp: Brought up 8 nodes, 192 CPUs' / 'Memory:' / 'Kernel command line' / 'Linux version' / 'DMI'
grep -n 'audit(1788'                                              # 墙钟锚点
```

**crash 侧**（附件 `crash_forensics.txt`，30 个会话，`timeout 590 crash /tmp/vmlinux-0102 <vmcore> -i cmdfile`）：

```
会话1   sys / panic / bt / bt -t / set / mach
会话2   dis -l ffffd7706823ae44 / dis find_busiest_group          # 反汇编全函数
会话3   rd 0xffffd77069c696c0 8 / vtop FAR / vtop x27 / struct sched_group
会话4   struct sched_group_capacity / whatis rq / p __per_cpu_offset[179] / px runqueues
会话5   p rq(179)->sd/.curr/.cpu/.idle + rd 两个 adrp 基址
会话6   struct sched_domain 0xffff604004407000                    # MC 域全字段
会话7   sd->span_weight / rd span / struct sched_group 0xffff604003ec60c0
会话8   MC groups 环 rd 遍历（ec61e0/ec6300/ec6420/ec6540）
会话9   父域 sd（NUMA level=4）+ groups
会话10  struct sched_group x22/x26/sd->groups 三处真值
会话11  CPU97 rq->sd
会话12  sd->parent->groups
会话13  【核心】sym 0xffffd7706a0655d0=__per_cpu_offset / sym 0xffffd77069c696c0=runqueues
        / p __per_cpu_offset[97]=0xffffa89017090000 / [179] / p &runqueues
会话14  struct rq.offsets（失败，如实记录；改用会话15 gdb 法）
会话15  p &((struct rq*)0)->cfs(=0x80) / ->cfs.avg(=0x100) / sched_avg.load_avg(=+0x20)
会话16  px &__per_cpu_offset[97] = 0xffffd7706a0658d8
会话17  【核心】rd 0xffffd7706a0658d8（[97] 真值）+ rd 数组 192 项全量（全非零）
会话18  【核心】p __cpu_online_mask / rq(97)->online=1 / cfs.avg.load_avg=1044 / rd rq(97)+0x120
会话19  【核心】vtop __per_cpu_offset 页（四级映射有效）vs vtop 模板段（PMD=0）
会话20  NUMA groups 环 rd 续（ed3a20/ed3060/ed3360）
会话21  ps | grep -E 'sftp|sshd' / log | tail / ps 56263
会话22  【核心】rd 崩溃栈 ffff8000e6f6b740（sp+8=&__per_cpu_offset[0]、sp+16=&runqueues 实测完好）
会话23-25 gdb list/info line *find_busiest_group+0x134/138/140    # 源码行定位（12050/12054/5024）
会话26  info symbol / p &cpu_util_cfs
会话27  runq 179（发现 191 核全部 CURRENT=neon_rot_ldr_at）
会话28-30 ps 58051/57625/57622（opendcdiag ← bash ← init 业务链）
```

**代数复算**（附件 `algebra.py` / `algebra_out.txt`）：闭式①–⑦、反事实推演、墙钟推算、指令编码解码（f9409377/f879d814 逐位），全部模 2^64。

---

*报告完。本报告全部结论可由附件命令独立复核；每一处数据引用均带 dmesg 行号或 crash 会话出处。*

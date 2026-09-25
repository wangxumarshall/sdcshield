# CPU179 转储深度诊断报告（第 1 次独立重研究）
## ——同一颗核 14.5 小时内三次"装载/走查"错乱：前兆两次被吞、致命一次带走整机，坏值不是零塌缩而是"指针被换脸"

| 信息项 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-09:15:42`（vmcore 10.6GB，Kdump compressed v6，PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48（Kunpeng-920 / TaiShan-v110 兼容封装，192 核 8 NUMA 节点，768GB） |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP（KASLR 开启，slide=0x4fd32652013c） |
| 崩溃时刻 | 2026-09-04 09:15:42（uptime 52269.76s ≈ 14h31m，开机于 09-03 18:44:32） |
| 受害进程 | `kworker/u392:0`（PID 1154762，events_unbound 内核工作线程，CPU179） |
| 前兆异常 | 2 次 `Ignoring spurious kernel translation fault` WARNING，均在 CPU179（uptime 2582.85s 与 13867.76s） |
| 崩溃签名 | `Unable to handle kernel paging request at 2cd7ddf3a9089790`，ESR=0x96000004（DABT, level 0 translation fault），pc=find_busiest_group+0x140 |
| 一行结论 | CPU179 的装载/地址生成通路发生多比特数据损坏（非零塌缩、非软件 bug），毒化指针 x20=0x2cd80e2000ffffb0 经 `add x27,x1,x20` 与 `ldr x23,[x27,#288]` 两级传播后产生非规范地址触发 panic【强推·SDC】 |

---

## 1. 执行摘要

1. **现象**：一次开机运行 14.5 小时后，CPU179 上的 `kworker/u392:0` 在调度器负载均衡热路径 `find_busiest_group()` 里，用一条普通内核装载指令 `ldr x23,[x27,#288]` 访问了非规范地址 `0x2cd7ddf3a9089790`，level-0 translation fault，kdump 落盘。
2. **签名**：坏地址不是任何"合法值+合法偏移"的产物——寄存器 x20=0x2cd80e2000ffffb0 与任何内核指针形态（ffff 开头）相距 35 个海明位，且其低 16 位 `0xffb0`、高 32 位 `0x2cd80e20` 呈"部分位保留+部分位翻转"的典型受扰形态。
3. **闭合验证（实锤）**：反汇编确定崩溃序列为 `ldr x20,[x0,w25,sxtw#3]` → `add x27,x1,x20` → `ldr x23,[x27,#288]`；用 Python3 模 2^64 复算，`x1+x20 == x27`（精确相等）且 `x27+288 == FAR`（精确相等）。加法与偏移全部正确，**唯一的污染源是 x20 的装载结果**。
4. **前兆收敛（强推）**：同一次开机、同一颗 CPU179，早在 3.1 小时和 10.7 小时前就发生过两次同型异常：一次是 `_find_next_and_bit+0x18` 把 `rcu_sched` 的 task_struct 指针当作 cpumask 位图基址传入（参数装载结果被"换脸"），一次是对 `hex_asc[15]` 这个确实存在的只读映射字节报 translation fault（crash vtop 实测该页 PTE VALID）。三次事件的受害路径全部含"装载/地址生成"环节。
5. **软件成因排除**：`find_busiest_group` 是调度器每毫秒级执行亿万次的热路径；受害的 sched_group/slab 对象真值经 crash 读取全部完好（`next/sgc/cpumask/flags` 字段合法闭合）；RAS/EDAC 全程零事件。软件 bug 不会只挑 CPU179、不会间隔小时级复现、更不会把 task 指针恰好"装进"位图参数寄存器。
6. **置信与处置**：微架构根因判定为【强推】——CPU179 装载通路（load path / 寄存器文件写回段）间歇性多比特损坏，因缺"同一指令重放对照"无法升级为实锤。处置：隔离 CPU179（nohz_full/affinity 排除）、换件或复位后压测复现；短期可开启调度结构冗余校验。

## 2. 证据规则与方法

- **证据源**：`vmcore-dmesg.txt`（2718 行，开机到 panic 完整）+ `vmcore`（10.6GB，PARTIAL DUMP，crash 8.0.4 加载成功；每次会话固定 384 条 seek error，全部为 SDEI/IRQ 栈与 vmalloc 尾部区域未含在转储中，属转储范围裁剪，不影响本次取证对象——所有目标地址 sched_group/runqueues/hex_asc/task_struct 读取均成功）+ 静态反汇编（`/tmp/vmlinux-0102`，BuildID 276194e5...，带 debug_info，`objdump`/`crash dis -l` 双通道互证）。
- **诚实铁律**：本报告每一处数据引用都来自实际命令的真实输出（dmesg 行号或 crash 提示符上下文），全文取证命令与输出存 `dmesg_forensics.txt`、`crash_forensics.txt`；全部 64 位运算由 `algebra.py`（模 2^64）计算，输出存 `algebra_out.txt`。
- **三级置信**：【实锤】可直接复核（如闭合等式）；【强推】多源收敛但缺直接对照（如根因判定）；【假设】无法软件验证，给出验证途径。
- **工具清单**：crash 8.0.4-17.oe2403sp4（`-i` 批处理，timeout 590/1800）、objdump (binutils)、Python3、grep/awk。

## 3. 本次开机时间线【时间线】

| uptime | 墙钟（反推） | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0.000000 | 09-03 18:44:32 | 开机，CPU 0x80000 boot，8 NUMA 节点 | 1-47 | 实锤 |
| 0.861066 | 18:44:33 | EDAC MC 初始化（ghes_edac 在线） | 1857, 2176 | 实锤 |
| 0.909198 | 18:44:33 | SDEI NMI watchdog 注册 | 1906 | 实锤 |
| 0.349739→ | 18:44:32 | smp: Brought up 8 nodes, 192 CPUs | 1255 | 实锤 |
| 2582.852896 | 09-03 19:27:35 | **前兆1**：CPU179 rcu_sched，`_find_next_and_bit+0x18` 翻译错误 WARNING（被 "Ignoring spurious" 吞掉） | 2579-2623 | 实锤 |
| 13478.22 | 09-03 22:22:50 | systemd 重启 journald（运维活动，非异常） | 2623-2627 | 实锤 |
| 13867.756739 | 09-03 22:35:40 | **前兆2**：CPU179 ps（用户态 syscall 读 /proc），`seq_put_hex_ll+0xb8` 翻译错误 WARNING | 2628-2670 | 实锤 |
| 52269.758693 | 09-04 09:15:42 | **致命**：CPU179 kworker/u392:0，find_busiest_group+0x140 Oops → panic → kdump | 2672-2718 | 实锤 |
| 52270.14 | 09-04 09:15:42 | Starting crashdump kernel | 2717 | 实锤 |

间隔计算（algebra_out.txt D 节）：前兆1→前兆2 = 11284.90s（3.13h），前兆2→致命 = 38402.00s（10.67h）。负载背景：panic 时 load average 83.41（crash sys），191 个 `neon_rot_stale_` 用户态计算进程绑满 191 颗核（runq 实测），属于高强度 NEE/NEON 旋转负载。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 行 2672-2697 节选）

```
[52269.758693] Unable to handle kernel paging request at virtual address 2cd7ddf3a9089790
[52269.771191]   ESR = 0x0000000096000004
[52269.775815]   EC = 0x25: DABT (current EL), IL = 32 bits
[52269.789946]   FSC = 0x04: level 0 translation fault
[52269.805811]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[52269.817923] [2cd7ddf3a9089790] address between user and kernel address ranges
[52269.825940] Internal error: Oops: 0000000096000004 [#1] SMP
[52269.934891] CPU: 179 PID: 1154762 Comm: kworker/u392:0 Kdump: loaded Tainted: G        W
[52269.969830] pc : find_busiest_group+0x140/0xb60
[52269.975242] lr : find_busiest_group+0x11c/0xb60
[52270.119066] Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

要点：WnR=0（读装载触发）；ISS=0x4 无 ISV（非对齐/非特权访问信息不可用，符合普通 `ldr xN,[xM,#imm]`）；地址落在用户与内核区间之间的"非规范带"，level-0 走查即失败。Code 窗口最后一条 `(f9409377)` 即 `ldr x23,[x27,#288]`（objdump 静态反汇编交叉验证：`ffff80008013ae48: f9409377 ldr x23,[x27,#288]`）。

### 4.2 全量寄存器（dmesg 行 52269.98-52270.06）

| 寄存器 | 值 | 寄存器 | 值 |
|---|---|---|---|
| x0 | 0000000000000097 | x1 | ffffcfd3a80896c0 |
| x2 | 0000000000013cb3 | x3 | 0000000000000017 |
| x4 | 0000000000000002 | x5 | ffffffffff800000 |
| x6 | 0000000000000097 | x7 | 0000000000000000 |
| x8 | ffff8001e5be3ac8 | x9 | ffffcfd3a665ae58 |
| x10 | 0000000000000172 | x11 | 000000000000003f |
| x12 | 0000000000011ce2 | x13 | 0000000000000000 |
| x14 | 0000000000000000 | x15 | 0000ffffc6c393e8 |
| x16 | ffffcfd3a661b918 | x17 | 0000000000000000 |
| x18 | 0000000000000000 | x19 | ffff8001e5be3b70 |
| **x20** | **2cd80e2000ffffb0** | x21 | ffffcfd3a847fcb0 |
| x22 | ffff604003ed3540 | x23 | 00000000000003ff |
| x24 | ffffcfd3a8485000 | x25 | 0000000000000097 |
| x26 | ffff604003ed3540 | **x27** | **2cd7ddf3a9089670** |
| x28 | ffff8001e5be3a70 | x29 | ffff8001e5be3ae0 |
| sp | ffff8001e5be3960 | pc | find_busiest_group+0x140 |
| pstate | 204000c9 (nzCv daIF +PAN) | | |

判读：x1=`&runqueues`（crash `px &runqueues` 实测吻合）、x22/x26=`ffff604003ed3540`（sched_group，crash `struct` 读取字段完好）、x25/x0=0x97=151（遍历到的 CPU 号）、x20/x27 为毒化值——除这两个寄存器外，其余全部处于合法语义。

### 4.3 Call trace（完整，crash bt 与 dmesg 一致）

```
find_busiest_group+0x140
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

### 4.4 前兆异常原文

**前兆1**（行 2579-2600 节选，uptime 2582.85s，CPU179，PID16 `rcu_sched`，调度器 idle balance 路径）：

```
[2582.852896] ------------[ cut here ]------------
[2582.852908] Ignoring spurious kernel translation fault at virtual address ffff604003ed3d58
[2582.852917] WARNING: CPU: 179 PID: 16 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
[2582.853150]  _find_next_and_bit+0x18/0x80
[2582.853159]  load_balance+0x108/0x6c0     ← 与致命同一调用栈位置!
[2582.853168]  pick_next_task_fair+0x110/0x718
```

**前兆2**（行 2628-2649 节选，uptime 13867.76s，CPU179，PID422956 `ps`，用户态 read syscall 读 /proc）：

```
[13867.756739] ------------[ cut here ]------------
[13867.756751] Ignoring spurious kernel translation fault at virtual address ffffcfd3a750a057
[13867.756759] WARNING: CPU: 179 PID: 422956 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
[13867.756994]  seq_put_hex_ll+0xb8/0x140
[13867.757002]  proc_pid_status+0x7d8/0xc50
[13867.757013]  seq_read+0x90/0xd8
```

### 4.5 RAS 负证据（dmesg grep）

```
$ grep -n -iE 'machine check|hardware error|edac.*error|ecc.*(error|uncorrect)|ras.*(error|fatal)|memory failure|corrected error|uncorrectable|thermal|voltage' vmcore-dmesg.txt
(无匹配)
```

EDAC/GHES 全程在线（行 1857/2176 初始化成功）但零事件上报——硬件错误**没有被检测到**，这正是 SDC（Silent Data Corruption）的定义特征：数据坏了、ECC/parity 没吭声。

## 5. 业务现象

- **受害进程**：`kworker/u392:0`，events_unbound unbound 工作队列的 0 号工作者（u392 = unbound pool 392）。崩溃点在 `worker_thread → schedule → newidle_balance`——工作者线程自愿让出 CPU 时，调度器在挑下一个任务的过程中崩掉。
- **业务背景**：runq 显示 191/192 颗核上各跑一个 `neon_rot_stale_` 用户态进程（PID 2586631-2586803 连号，NODENAME=localhost0102），这是 NEON/ASIMD 旋转类高强度计算压测负载（类似 `neon_rot` 核间数据旋转测试）。load 83 说明压测正处于密集期。前兆2 的受害进程 `ps` 是运维在压测期间查看进程状态的常规操作。
- **解读**：三次受害全部是"途经 CPU179 的普通内核代码"（rcu_sched 内核线程、ps 的系统调用、kworker 的调度路径），不是某个特定业务的数据被算错——受害对象是**内核公共基础设施**，受害条件是**恰好在 CPU179 上执行**。

## 6. 诊断定位过程【诊断定位过程】

**P1 · dmesg 勘察**：2718 行里只有 3 个异常块（2 前兆 + 1 致命），全部 CPU179、全部 data abort 类。RAS 零事件。负载为 191 核 NEON 压测。

**P2 · 崩溃块提取**：ESR=0x96000004 解码为 EL1 数据异常、level 0 翻译错误、读访问；坏地址 0x2cd7ddf3a9089790 位于用户/内核区间之间的非规范带；Code 窗口 5 条指令静态地址反汇编逐条对齐。

**P3 · crash 加载**：`crash /tmp/vmlinux-0102 vmcore` 加载成功（PARTIAL DUMP，SDEI/IRQ 栈 seek error 记录在案，不影响目标对象）；`bt`/`bt -r`/`bt -f` 与 dmesg 完全一致；panic task = kworker/u392:0 CPU179。

**P4 · 内存真值对照**（关键会话，详见 crash_forensics.txt）：
- `struct sched_group 0xffff604003ed3540`：`next=0xffff604003ed3b40`、`sgc=0xffff604003ecfa20`、`group_weight=120`、`cpumask=0xffff604003ed3578`——**内存里的 sched_group 真值完好**（其 `next` 指向的下一组 `struct sched_group 0xffff604003ed3b40` 也完好，sgc 容量字段 122798/1024 合理）。
- `kmem 0xffff604003ed3540`：kmalloc-96、node 7、slab 页 `ffff604003ed3000` 42 对象全分配——slab 元数据健康。
- `rd 0xffffcfd3a80896c0`（x1=&runqueues）：内容为 0x1081/0x108e... 的 percpu 偏移表，**真值完好**。
- `vtop 0x2cd7ddf3a9089790`：ambiguous（非规范地址无页表项）——印证 level-0 fault。
- `vtop 0xffff604003ed3d58`（前兆1 FAR）：**VALID 1GB 直映 PUD（phys 604003ed3d58）**——该地址明明有映射！
- `vtop 0xffffcfd3a750a000`（前兆2 FAR 页）：**VALID 只读 PTE（e0002fe910af83）**——同样有映射！
- `p &hex_asc` = 0xffffcfd3a750a048，而前兆2 FAR=...a057=hex_asc+15=hex_asc[15]（'f' 字节）——**前兆2 访问的地址在语义上完全正确**。
- `p ((struct task_struct *)0xffff0020250d1500)->pid/comm` = 16 / "rcu_sched"——前兆1 x0 装载进来的"位图基址"实际是 rcu_sched 的 task_struct 指针。

**P5 · 软件成因排除**：
1. `find_busiest_group` 是 CFS 调度器每次负载均衡必经路径，192 核满载下每秒执行数百万次，6.6.0-145 内核早已海量验证；同样的代码在其他 191 颗核上同窗口零异常。
2. 受害数据结构（sched_group、runqueues、slab 元数据）内存真值全部完好——排除"内存被写坏"。
3. 两次前兆与致命的调用栈、受害对象、故障形态互不相同（位图遍历/hex 打印/指针解引用），但**CPU 全部是 179**——共同点不在代码路径，而在硬件资源。
4. 前兆2 证明对一个 VALID 只读映射产生了 translation fault——这已超出"软件传入坏指针"能解释的范围（软件不会对 hex_asc[15] 报缺页）。

**P6 · 定位收敛**：三次事件的公共交集 = CPU179 + "装载结果或地址生成结果错误"。致命事件的闭合等式把受损环节精确钉在 x20 的装载/写回上；前兆1 钉在参数装载；前兆2 钉在页表走查/TLB 查找读出。三者统一的最小假设：CPU179 的数据通路（load path，含 PTW 读出段）存在间歇性多比特扰动。

## 7. 逻辑链条

**指令语义**（objdump 静态反汇编 + crash `dis -l` 双通道，fair.c:12050 附近循环展开）：

```
ffff80008013ae38: f9400782  ldr  x2, [x28, #8]         ; sds->total_load 等
ffff80008013ae3c: f879d814  ldr  x20, [x0, w25, sxtw #3]  ; x20 = 数组元素(应为 percpu rq 指针)
ffff80008013ae40: 2a1903e0  mov  w0, w25
ffff80008013ae44: 8b14003b  add  x27, x1, x20           ; x27 = &runqueues + x20
ffff80008013ae48: f9409377  ldr  x23, [x27, #288]       ; ← 致命指令 (pc)
```

该序列是 `for_each_cpu_and(cpu, sched_group_span(group), ...)` 循环体内 `cpu_rq(cpu)` 类访问的展开：x20 应为某 per-cpu 数组元素（合法形态 `ffff....`）。

**闭合等式【实锤】**（algebra_out.txt G/H 节，模 2^64）：

```
等式1: x1 + x20 = 0xffffcfd3a80896c0 + 0x2cd80e2000ffffb0 = 0x2cd7ddf3a9089670 = 实测 x27  ✓
等式2: x27 + 288 = 0x2cd7ddf3a9089670 + 0x120 = 0x2cd7ddf3a9089790 = 实测 FAR       ✓
```

加法器（ALU）与立即数偏移全部正确——**坏值只有一个入口：x20 的装载结果**。

**反事实推演**：若 x20 是任何合法 percpu 指针（`__per_cpu_offset[179]=0xffffb02cd9754000` 加任意小偏移），则 x27 = x1+x20 必为 `ffff...` 规范内核地址（algebra 复算 5 组样例全部如此），`ldr x23,[x27,#288]` 正常返回，无异常。实测 x27 却是 `0x2cd7...`——x20 与合法指针的海明距离 35 位，"装载到错值"而非"算出错值"。

**坏值形态学**：x20=0x2cd80e2000ffffb0 不是零塌缩（SDC 最常见形态），也不是单比特翻转（与最近合法指针差 35 位），而是保留低 16 位 `0xffb0` 中部分位、高 32 位呈独立随机形态的"换脸"值——更像装载通路上数据被另一个数据流的部分位污染（如 MSHR/填充缓冲区串扰、寄存器文件写回段位线扰动）。前兆1 的"task 指针替代位图指针"同样呈"整值被换"而非"部分位翻"的形态，与前兆2 的"页表走查读出错误"一起，共同指向数据通路的**间歇性、多形态**损坏。

**诚实声明**：
- 【实锤】x20 装载结果损坏（闭合等式可复核）；前兆2 访问地址合法（hex_asc[15]，vtop 可复核）；三次事件同核 CPU179（dmesg 可复核）。
- 【强推】损坏发生在 CPU179 的装载通路/数据通路（含 PTW 读出段），而非内存本身、非 ALU、非软件。
- 【假设】具体受扰的微架构结构（L1D 填充缓冲/MSHR、寄存器文件写回端口、load 数据转发路径）无法用软件手段区分——验证途径：对 CPU179 做离线 MBIST/边界扫描；在线用"`ldr` 结果冗余校验"型微基准（同一地址读两次比对）在隔离核上压测复现；或换件后观察是否迁移。

## 8. 故障根因（微架构级结论 + 置信级别）

**根因判定【强推】**：CPU179 核心内装载通路（load path：L1D 命中/缺失数据返回路径，含页表走查 PTW 的读出数据段与寄存器文件写回段）存在间歇性多比特数据损坏，属于无 ECC/parity 覆盖的静默数据损坏（SDC）点。本次开机的三次事件（2582s / 13867s / 52269s）是该点在 14.5 小时内的三次独立发怒：前两次分别污染了参数装载与页表走查读出，被内核 `__do_kernel_fault` 的 "Ignoring spurious" 容错逻辑吞掉（侥幸：受害地址恰好有映射或错误恰好可恢复）；第三次污染了 `find_busiest_group` 循环中 x20 的装载，毒值经两条纯正确的指令传播为非规范地址，触发不可恢复的 Oops 并 kdump。

置信级别：**强推**（多源收敛：闭合等式实锤了"装载结果损坏"这一层；同核三次事件收敛了"位置在 CPU179"；但"具体哪段微架构结构受损"缺硬件级对照，无法升级实锤）。

## 9. 启示

### 9.1 微架构定位的意义

本案若只看 panic 现场，极易误判为"调度器指针 bug"或"内存条坏了"。把前兆挖出来并逐个闭合验证后，真相是**单核数据通路的三次不同形态发作**——这正是微架构级取证的价值：panic 只是一次发作的"尸检报告"，前兆才是"病史"。三次发作形态各异（参数装载换脸 / PTW 误报 / 指针毒化）但位置同一，这种"同址不同症"模式本身就是数据通路损坏的经典指纹（对比：软件 bug 呈"同代码路径复现"，内存故障呈"同物理地址复现"，二者本案都不满足）。

### 9.2 芯片设计与实现启示（具体、可落地）

1. **L1D 填充缓冲/MSHR 加 parity**：load miss 返回数据在填充路径上是最长的无校验数据旅程；对返回数据加单字节 parity（侦错即可，fail-stop 胜过 fail-silent）能本案第一现场就报警。
2. **PTW 读出数据校验**：前兆2 证明页表走查的读出段会"对 VALID 页报 fault"。PTW 输出（PTE 值与 fault 判定）应做一致性自检——例如 walk 完成后对 level-0 fault 用第二遍低成本重走查确认（成本：仅异常路径慢一点）。
3. **寄存器文件写回段保护**：x20 的"换脸"形态提示写回/旁路网络受扰。关键指针寄存器可采用奇偶校验位或双份冗余（对 scheduler 热路径的指针装载做 load-verify-load，代价 2 条指令，只在低频路径开启）。
4. **错误传播屏障设计**：本案毒指针经 `add`（正确）+`ldr`（崩溃）两级传播才暴露。设计上可在"指针使用点"（地址生成 AGU 输出）加非法地址带检测——TTBR 走查 level-0 fault 已经天然是屏障，但代价是整机 panic；对非规范带地址（user/kernel 之间）可以在 AGU 处提前 trap 并附带 RAS 事件码，把"致命 panic"降级为"精确的可观测事件"。
5. **DFT/在线测试钩子**：为量产 CPU 提供"核隔离 + load 通路 BIST"模式（类似 `neon_rot_stale_` 这种压测进程名暗示用户已在尝试复现），让运维能在不拆机的情况下对单核数据通路做确定性自测。
6. **kdump 可靠性设计**：PARTIAL DUMP 裁掉了 SDEI/IRQ 栈（本次每次 crash 会话 384 处 seek error，均为该类区域），若故障恰在这些栈上则证据丢失。建议 kdump 优先级表把 per-cpu 异常栈列为必转储项。

### 9.3 对系统软件/RAS 的启示

1. **"Ignoring spurious kernel translation fault" 不应静默**：本案两次前兆都被这行日志轻轻放过。该路径应升级为可计数的 RAS 事件（per-CPU 计数 + 速率阈值告警），两次前兆若被计数告警，运维在开机后 43 分钟就能换下 CPU，不必等 14.5 小时后的宕机。
2. **EDAC 零事件 ≠ 硬件无恙**：CPU 内核数据通路的损坏根本不经过内存控制器的 ECC——RAS 体系需要把"核内 SDC"（由内核容错路径的异常统计捕获）纳入硬件错误台账。
3. **调度器关键指针的廉价自检**：`find_busiest_group` 这类热路径不宜加运行时校验，但可以在 newidle_balance 的慢速分支对 `sched_group` 链做周期性完整性巡检（kabi_reserved 字段已有冗余空间可放魔数）。
4. **故障核快速隔离预案**：在 NUMA 服务器上，单核 SDC 的最小代价处置是 CPU 亲和性排除（`nohz_full`/`isolcpus` 动态扩展或 cgroup cpuset 收缩），本案 CPU179 属 node 7（受害 slab 亦在 node 7 内存，NUMA 亲和一致），隔离后业务可降级续跑。

## 10. 处置建议

| 优先级 | 动作 | 依据 |
|---|---|---|
| P0 立即 | 收集本次 vmcore 与 dmesg 归档（已完成本报告）；对机器做 BMC/CEC 日志二次核查（固件层 RAS 记录可能不进内核 dmesg） | RAS 负证据只覆盖内核可见面 |
| P0 立即 | 将 CPU179 从生产调度中排除（cpuset/affinity 约束），并停跑 `neon_rot_stale_` 压测复现任务避免掩盖 | 三次事件同核强收敛 |
| P1 短期 | 隔离 CPU179 后运行"load 双读比对"微基准（同地址 ldr 两次异或，持续 24h）；若复现，根因升级实锤 | 验证途径已给出 |
| P1 短期 | 给内核打 per-CPU "spurious fault" 计数补丁并接入告警（arch/arm64/mm/fault.c:494 路径） | 前兆吞没问题 |
| P2 中期 | 联系厂商对 CPU179 所在物理核做离线诊断（MBIST/LBIST）；确认换件窗口 | 硬件级定论 |
| P2 中期 | 复盘 `neon_rot_stale_` 压测目的：若为老化/旋转测试，评估其与故障的时间相关性（压测启动时间 vs 前兆1） | 诱因分析 |

---

## 附录：命令索引（全部取证命令，可复核）

```bash
# --- dmesg 法证 (完整输出见 dmesg_forensics.txt) ---
ls -la /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-09:15:42/
wc -l vmcore-dmesg.txt
head -60 vmcore-dmesg.txt
grep -n -E 'Linux version|Kernel command line' vmcore-dmesg.txt
grep -n -E 'Memory:|crashkernel|smp: Brought up|RCU restricting' vmcore-dmesg.txt
grep -n -E 'WARNING|BUG|Oops|cut here|spurious|Machine check|Hardware error|EDAC|ECC|ras|memory failure|segfault|Unable to handle|Internal error|Call trace|panic' vmcore-dmesg.txt
grep -n -iE 'machine check|hardware error|edac.*error|ecc.*(error|uncorrect)|ras.*(error|fatal)|memory failure|corrected error|uncorrectable|thermal|voltage' vmcore-dmesg.txt   # RAS 负证据, 无匹配
grep -n 'EDAC' vmcore-dmesg.txt
sed -n '2579,2623p' vmcore-dmesg.txt     # 前兆1
sed -n '2628,2670p' vmcore-dmesg.txt     # 前兆2
sed -n '2672,2718p' vmcore-dmesg.txt     # 致命块
grep -n '^\[' vmcore-dmesg.txt | awk -F'[][]' '$2+0>100 && $2+0<52270'   # 时间线扫描

# --- crash 法证 (完整输出见 crash_forensics.txt; namelist=/tmp/vmlinux-0102) ---
# 会话1 最小加载:
timeout 590 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_min.cmd        # sys/panic/bt
# 会话2 全量:
timeout 1800 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_full.cmd      # sys panic bt bt -t set mach ps runq timer net dev -p kmem -s
# 会话3 闭合验证:
timeout 590 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_crash2.cmd     # bt -r, p __per_cpu_offset[179], px &runqueues, struct sched_group/sched_domain, rd, dis -l pc
# 会话4 结构真值:
timeout 590 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_crash4.cmd     # struct sched_group(链), struct sched_group_capacity, rd cpumask
# 会话5 页表走查:
timeout 590 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_kvmem.cmd      # vtop x3地址, kmem sched_group, search -t
# 会话6 前兆真值:
timeout 590 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_crash5.cmd     # rd FAR1/FAR2页, vtop FAR1/FAR2, sym seq_put_hex_ll
# 会话7 身份验证:
timeout 590 crash /tmp/vmlinux-0102 <VMCORE> -i /tmp/case_T09_crash7.cmd     # p task->pid/comm, p &hex_asc, rd

# --- 静态反汇编 (独立于 crash 通道) ---
nm /tmp/vmlinux-0102 | grep -E ' find_busiest_group$| _find_next_and_bit$| seq_put_hex_ll$'
objdump -d --start-address=0xffff80008013ad08 --stop-address=0xffff80008013aec0 /tmp/vmlinux-0102
objdump -d --start-address=0xffff8000807459d0 --stop-address=0xffff800080745a50 /tmp/vmlinux-0102
objdump -d --start-address=0xffff80008051b768 --stop-address=0xffff80008051b860 /tmp/vmlinux-0102

# --- 代数复算 (模 2^64, 见 algebra.py / algebra_out.txt) ---
python3 algebra.py   # KASLR slide, 闭合等式(x1+x20==x27, x27+288==FAR, FAR2==hex_asc+15), 时间线, 海明距离
```

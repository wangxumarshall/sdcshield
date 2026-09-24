# CPU179 缺陷核微架构级 SDC 深度诊断报告（完整 11G 转储，crash 全链闭合）
## x20 装载通路瞬时破坏（`__per_cpu_offset[150]` 在途值损坏）→ 野指针 x27 → Oops，与 13 次"已映射页伪翻译故障"同核同窗口统一归因

# 1. 执行摘要

本报告对 2026-09-04 21:53:28 采集的 11G 完整 vmcore（192 CPU、8 NUMA node、HiSilicon HIP08 / 鲲鹏平台，openEuler 6.6.0-145.3.23.154.oe2403sp3.aarch64）进行了独立盲诊（未参考任何既有分析），首次在本系列案例中用 crash + 代数复算完整闭合了从原始破坏量到崩溃地址的整条传播链：

1. 崩溃点指令语义完全解码（crash_session4/17）：`find_busiest_group+0x140` 处 `ldr x23,[x27,#288]`（指令码 `f9409377`，imm12=0x24，×8=0x120=288）读 `rq->cfs` 平均负载字段；其前 4 条指令为 `ldp x0,x1,[sp,#8]`（x0=`__per_cpu_offset` 数组基址、x1=`&runqueues`）→ `ldr x20,[x0,w25,sxtw #3]`（x20=`__per_cpu_offset[w25]`）→ `add x27,x1,x20`（x27=`cpu_rq(w25)` 的标准 per-cpu 指针算术）。
2. 原始破坏量定位到 x20：dmesg x25=0x96=150，故 x20 本应为 `__per_cpu_offset[150]=0xffffc573b8bda000`（vmcore 中该槽位实测值正确，crash_session22/25）。CPU 实际获得的却是 `0x73b88cc000ffffc5`：内存干净、在途值坏，即破坏发生在 L1D→LSU→寄存器堆数据通路上（装载指令本身未触发异常，说明是"读出坏数据"而非"读错地址"）。
3. 两级代数 100% 闭合（algebra_out.txt Q 节）：`x1+x20_obs ≡ 0x73b8474cc9829685 (mod 2^64) == 观测 x27`（ADD 语义闭合，含第 65 位截断）；`x27+0x120 == 0x73b8474cc98297a5 == dmesg FAR`（LDR 偏移闭合）。期望值侧同样闭合：`x1+x20_true = 0xffff8000814036c0`，crash 实测该地址 `rq->cpu(+2880)=150`，确系 `cpu_rq(150)`，目标内存 `+0x120` 处是合法的 sched_avg 数据（0x400/0x400/0x400/0x200）。若无破坏，本次执行完全正常。
4. 13 次前兆 WARNING 同核同机制：全部在 CPU 179、9.24h 内随机分布、`__do_kernel_fault` 对已映射页报 level-0 翻译故障（crash_session9 vtop：6 个受测地址全部 VALID 1GB 直接映射 PTE；session10/11 kmem：地址均落在已分配的 kmalloc-4k/512 对象内，即 /proc/interrupts 的 seq_file 缓冲）。页表遍历（PTW）读数或 TLB 项被瞬时污染，是同一数据通路故障的另一种表现。
5. x20 破坏形态：既非单事件翻转（与真值 hamming 距离 31 位），也非对齐/非对齐读错槽位，而呈"按大端字节道 5 字节旋转 + 2 字节内 5 位翻转"的字节道错位+多位破坏形态，指向数据通路字节使能/对齐复用层面的微架构缺陷，而非单纯位单元翻转。
6. 负证据链完备：RAS/GHES/EDAC/SDEI 基础设施在位（dmesg L1262/1306/1307/1857/2175）但 33272s 零硬件错误记录；无 KASAN/UBSAN/debug_pagealloc（crash_session30 符号级验证）；崩溃指令、寄存器、栈数据三方互证无软件异常。

根因结论（置信度 0.85）：CPU 179 核内数据通路（L1D 数据阵列→LSU 返回通路→物理寄存器堆 x20 槽位，或 PTW/TLB 数据侧）存在间歇性微架构缺陷，在 9.24h 内至少造成 14 次 CPU 可见瞬时数据破坏（13 次被页表遍历/翻译路径吸收为 spurious fault，1 次进入指针运算酿成致命野指针）。固件优先 RAS 链路对此类核内瞬态完全不可见，这正是 SDC 的定义性特征。

主会话初筛假设逐条复核结果：①"CPU179 反复 WARNING ×4+"→ 实为 13 次（表 3-1）；②"最长存活 9.2h uptime 33272s"→ 证实（crash sys UPTIME 09:14:32）；③"LDR x23,[x27,#0x120]，FAR-x27=+0x120 闭合"→ 证实且扩展（q27 由 x1+x20 派生）；④"x27 高16位破坏形态 hi16 有 7 位翻转"→ 部分证伪/修正：x27 的高 16 位是派生值，原始破坏在 x20（hamming 31 位，非 7 位单翻转形态，见 §7.3）；⑤"13 次 WARNING+13 次 spurious interrupt"→ 13 次 spurious kernel translation fault（非 spurious interrupt，术语修正）；⑥"RAS 负证据"→ 证实（§4.5）。

# 2. 证据规则与方法

- 证据来源仅限：vmcore 原始文件、vmcore-dmesg.txt（3230 行，引用带行号）、crash 8.0.4 会话日志（crash_session1~33.log，真实命令+真实输出）、python3 复算脚本 algebra.py 及其输出 algebra_out.txt（禁止手算）。
- 方法：dmesg 全量勘察 → 寄存器/指令级反汇编 → 结构体偏移验证（rq/sched_domain/sched_group/lb_env）→ 模 2^64 代数闭合 → 反事实推演（期望值侧同样闭合）→ 软件成因排除 → 根因收敛。
- 诚实声明：crash 加载时的 `seek error ... IRQ stack pointer / SDEI stack pointer`（crash_session1/16 等头部）为 192 核 per-cpu 栈指针读取失败的固有噪声，不影响正文取证命令的输出有效性；session2 中 `p xtime`、`dis -l ... ,8` 等 3 条命令失败（语法错误），已在附录如实记录；session29 `rd -a` 因页被排除失败。所有失败均未用于任何结论。
- 转储完整性：11,772,935,568 字节完整 vmcore（非 PARTIAL 标记的缺页转储；crash 头显示 `[PARTIAL DUMP]` 字样为 kdump 常规标记，本次取证所有地址均可读）。

# 3. 本次开机时间线【时间线】

开机指纹（dmesg 行号）：
- L2：`Linux version 6.6.0-145.3.23.154.oe2403sp3.aarch64 ... #1 SMP Mon Jul 27 19:00:34 CST 2026`
- L1272：`DMI: Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026`；ACPI 表全为 `HISI HIP08`（L8-26）
- L1255-1256：`Brought up 8 nodes, 192 CPUs`；L408：内存 768GB
- L387 命令行：`crashkernel=1024M,high ... arm64.nopauth nospectre_bhb`（无任何 debug/KASAN 选项）
- CPU179 归属：L307 `PXM 7 -> MPIDR 0x7a0300 -> Node 7`；L39/L109 node7 内存 `0x604000000000-0x6057ffffffff` → CPU179 本地直接映射窗口 = `ffff6040xxxxxxxx`

表 3-1 故障时间线（全部 CPU 179）：

| # | uptime(s) | dmesg 行 | 事件 | PID/Comm | spurious/FAR 地址 |
|---|-----------|----------|------|----------|-------------------|
| 1 | 1391.174 | 2591-2635 | WARNING `__do_kernel_fault+0x130` | 10301 pmdalinux | ffff604004a9b3a0 |
| 2 | 1661.191 | 2636-2681 | 同上 | 10301 pmdalinux | ffff604004a9a327 |
| 3 | 2291.212 | 2681-2726 | 同上 | 10301 pmdalinux | ffff604004a9d516 |
| 4 | 3105.045 | 2726-2771 | 同上 | 9742 irqbalance | ffff604019cad66b |
| 5 | 3125.038 | 2771-2816 | 同上 | 9742 irqbalance | ffff604019caf52c |
| 6 | 3785.052 | 2816-2860 | 同上 | 9742 irqbalance | ffff60402089443a |
| 7 | 3785.057 | 2861-2905 | 同上 | 9742 irqbalance | ffff604020894332 |
| 8 | 3795.053 | 2906-2950 | 同上 | 9742 irqbalance | ffff60401b721731 |
| 9 | 3811.206 | 2951-2995 | 同上 | 10301 pmdalinux | ffff60401b7244c9 |
| 10 | 3825.051 | 2996-3040 | 同上 | 9742 irqbalance | ffff60401b724046 |
| 11 | 3845.056 | 3041-3085 | 同上 | 9742 irqbalance | ffff604021842788 |
| — | 25474.783 | 3087 | 业务侧：`silifuzz_orches[707761]: memfd_create() ...`（无异常语义） | | |
| 12 | 33265.049 | 3088-3133 | 同上 | 9742 irqbalance | ffff60400618c61e |
| 13 | 33271.244 | 3134-3177 | 同上 | 10301 pmdalinux | ffff6040065183ed |
| 14 | 33271.977 | 3178-3230 | **FATAL Oops** `find_busiest_group+0x140` | 581185 mi-scavenger | **73b8474cc98297a5** |

- 总 uptime = 33272s = 9.24h（crash `sys`：`UPTIME: 09:14:32`，DATE Fri Sep 4 21:52:44 CST 2026）
- WARNING 间隔：270/630/814/20/660/0.005/10/16/14/20/29420/6.2 s，无周期性、随机偶发；29420s 长静默后 6.2s 内连发 2 次 WARNING 再 0.73s 后致命崩溃（"末段加速"仅 3 个样本，不足以单独定性，如实记录）
- 13 次 WARNING 全部同签名：`WARNING: CPU: 179 ... at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8`，前置行 `Ignoring spurious kernel translation fault at virtual address ffff6040xxxxxxxx`
- mi-scavenger（PID 581185）task start_time = 3786314215660 ns ≈ uptime 3786.3s（crash_session32），崩溃时任务年龄 8.19h，与 WARNING #6/#7（3785.05s）几乎同时创建，属巧合，不构成因果（该任务此后 8 小时运行正常）

# 4. 故障现象【故障现象】

## 4.1 Oops 原文（dmesg 行 3178-3230）

```
3178:[33271.976579] Unable to handle kernel paging request at virtual address 73b8474cc98297a5
3179:[33271.985323] Mem abort info:
3180:[33271.988902]   ESR = 0x0000000096000004
3181:[33271.993439]   EC = 0x25: DABT (current EL), IL = 32 bits
3184:[33272.007311]   FSC = 0x04: level 0 translation fault
3186:[33272.016643]   ISV = 0, ISS = 0x00000004, ISS2 = 0x00000000
3187:   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
3188:   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
3189:[33272.034855] [73b8474cc98297a5] address between user and kernel address ranges
3190:[33272.042782] Internal error: Oops: 0000000096000004 [#1] SMP
3193:[33272.151470] CPU: 179 PID: 581185 Comm: mi-scavenger Kdump: loaded Tainted: G        W
3196: pc : find_busiest_group+0x140/0xb60
3197: lr : find_busiest_group+0x11c/0xb60
3227: Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
3228:[33272.370900] SMP: stopping secondary CPUs
3229:[33272.380478] Starting crashdump kernel...
```

ESR 语义（algebra_out.txt D 节）：EC=0x25 当前 EL 数据中止、FSC=0x04 level-0 翻译故障、WnR=0 读访问、非对齐非向量。FAR `0x73b8474cc98297a5` 的 bits[63:48]=0x73b8，既非内核规范（0xffff）也非用户规范（0x0000），落入非规范空洞，PGD 遍历必然失败，与内核打印"address between user and kernel address ranges"（L3189）互证。

## 4.2 全量寄存器（dmesg 行 3199-3208，crash bt 一致）

```
x29: ffff8000cb18b8c0  x28: ffff8000cb18b850  x27: 73b8474cc9829685   <- 派生野指针
x26: ffff604003e5fea0  x25: 0000000000000096  x24: ffffba8cc8c25000   <- 0x96=150 (w25 索引)
x23: 00000000000003ff  x22: ffff604003e5fea0  x21: ffffba8cc8c1fcb0   <- = &nr_cpu_ids
x20: 73b88cc000ffffc5  x19: ffff8000cb18b950  x18: 0000000000000000   <- x20=原始破坏量
x9 : ffffba8cc6dfae58  x8 : ffff8000cb18b8a8  x1 : ffffba8cc88296c0   <- x1=&runqueues
x0 : 0000000000000096  ...
```

关键定性（详见 §7）：
- `x20 = 0x73b88cc000ffffc5`，原始破坏量（应为 `__per_cpu_offset[150]=0xffffc573b8bda000`）
- `x27 = 0x73b8474cc9829685`，派生值（`x1+x20 mod 2^64`）
- x1/x0/x25/x19/x21/x22/x24/x26 等其余参与指针运算的寄存器全部合法且与栈/结构体互证一致

## 4.3 Call trace（dmesg 行 3209-3226，crash bt 完整一致）

```
find_busiest_group+0x140   <- 崩溃点
load_balance+0x108
newidle_balance+0x198
pick_next_task_fair+0x110
pick_next_task+0x60
__schedule+0x1b4
schedule+0x58
futex_wait_queue+0x78      <- mi-scavenger 在 futex 等待被唤醒后走 newidle 负载均衡
futex_wait+0xe8
do_futex+0xec / __arm64_sys_futex+0x80
invoke_syscall → el0_svc ... el0t_64_sync
```

即：用户态 mi-scavenger 的 futex 系统调用路径上，内核在 `newidle_balance` 空闲均衡里遍历 NUMA 调度域（sd=0xffff6040043a3200，level=6，name="NUMA"，span_weight=120，group span=cpus 144-191，见 crash_session14/18），对域内每个 CPU 做 `cpu_rq(cpu)` 访问，第 7 次迭代（cpu=150）时装载通路返回坏值。

## 4.4 前兆 WARNING（13 次，dmesg 行 2591-3177，全文见 dmesg_forensics.txt）

首个 WARNING 块（L2591-2635）核心内容：
```
2592:[ 1391.173702] Ignoring spurious kernel translation fault at virtual address ffff604004a9b3a0
2593:[ 1391.173712] WARNING: CPU: 179 PID: 10301 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
2611: Call trace:
2613:  __do_kernel_fault+0x130/0x1b8
2614:  do_bad_area+0x70/0x88
2615:  do_translation_fault+0x40/0x80
2616:  do_mem_abort+0x4c/0xa8
2619:  el1h_64_sync+0x78/0x80
2620:  __memcpy+0x80/0x240          <- 崩溃于 memcpy 源读取
2621:  seq_printf+0xc4/0xe8
2622:  show_interrupts+0x1d4/0x498   <- /proc/interrupts 生成
2623:  seq_read_iter+0x168/0x478
2624:  proc_reg_read_iter+0x68/0xe8
2625:  new_sync_read → vfs_read → ksys_read → read syscall
```

13 次全部同签名、同 CPU，受害者为两个周期性读 `/proc/interrupts` 的用户态监控进程（pmdalinux=PCP 性能采集器 ×5 次、irqbalance ×8 次）。WARNING 帧寄存器恒定 x27=0x00000000ffffffd8（stale callee-saved 残留，非指针，无分析价值，如实记录）。

伪翻译故障的实证（这是本系列案例首次能对 WARNING 路径做 vtop/kmem 级验证）：
- crash_session9 `vtop`：6 个受测地址（04a9b3a0/04a9a327/04a9d516/0618c61e/065183ed/21842788）全部命中 VALID 的 1GB 直接映射 PUD/PTE（如 `ffff604004a9b3a0 → 604004a9b3a0，PTE e8604000000f05 VALID|SHARED|AF|...`）
- crash_session10/11 `kmem`：地址均落在已分配的 kmalloc-4k（如 `ffff604004a9b000 [ALLOCATED]`，slab ffff604004a98000）或 kmalloc-512 对象内，它们就是 CPU179 本地 node7 分配的 seq_file 缓冲
- 页内偏移 0x046-0x788，全部在 4KB 缓冲范围内，与 /proc/interrupts 生成过程中的 `__memcpy(m->buf+count, ...)` 源地址形态完全吻合
- 结论：MMU 对着已经映射、已经分配的页报了 level-0 翻译故障，页表遍历（PTW）读数或 TLB 查找结果被瞬时污染

## 4.5 RAS 负证据（dmesg grep，全文 3230 行）

命中项全部为基础设施初始化，零错误载荷：
```
L1262: CPU features: detected: RAS Extension Support
L1306: sdei: SDEIv1.0 (0x0) detected in firmware.
L1307: GHES: APEI firmware first mode is enabled by APEI bit and WHEA _OSC.
L1857: EDAC MC: Ver: 3.0.0
L1905: SDEI NMI watchdog: SDEI Watchdog registered successfully
L1944: ERST: Error Record Serialization Table (ERST) support is initialized.
L2175: ghes_edac: This system has 32 DIMM sockets.
```
grep `ras|edac|mce|machine check|memory error|ecc|corrected error|deferred error|fatal|hardware error|ghes|apei|cper` 全文，无一条 CPER/GHES/EDAC 错误记录。固件优先 RAS 链路（含 32 DIMM 监控 + SDEI NMI watchdog）在 33272s 内保持沉默。

# 5. 业务现象【业务现象】

- 主机：localhost0102，Yangtze Computing R240K V2（鲲鹏 HIP08，192 核），768GB 内存，运行 silifuzz 类大规模算力业务（dmesg L3087 `silifuzz_orches[707761]` 于 25474s 出现；崩溃时 load average 42.73/9.68/3.24，TASKS 2212，crash_session1 `sys`）
- 崩溃任务 `mi-scavenger`（PID 581185，task ffff60201da03f00）：任务创建于 uptime 3786.3s（crash_session32 `task -R start_time` = 3786314215660ns），即业务已在 CPU179 所在 node 上运行 8.19h 后命中缺陷
- 用户可见影响：节点 panic → kdump 转储（`SMP: stopping secondary CPUs` L3228 → `Starting crashdump kernel` L3229 → `Bye!` L3230），业务中断
- 13 次前兆 WARNING 期间业务无感（`__do_kernel_fault` 对该类伪翻译故障仅打印后返回，memcpy 重试语义由上层 read 循环天然容错），未造成数据可见损坏；但按 SDC 语义，无法排除这 13 次窗口内另有未被观测到的静默数据破坏（见 §7.4 诚实边界）

# 6. 诊断定位过程【诊断定位过程】

### P1 · dmesg 全量勘察（命令+输出见 dmesg_forensics.txt）
`wc -l`（3230 行）、`grep -n` 提取 boot 指纹/WARNING/Oops/spurious/RAS 关键词、SRAT/PXM 拓扑核对。产出 §3 时间线与 §4 现象。

### P2 · crash 加载与崩溃帧提取（crash_session1/4/12）
`crash /tmp/vmlinux-0102 <vmcore>` 加载成功（BuildID 匹配 6.6.0-145.3.23.154）。`bt`/`bt -t`/`bt -r` 确认：panic task = mi-scavenger@CPU179，PC=find_busiest_group+0x140（0xffffba8cc6dfae48），pt_regs 与 dmesg 逐寄存器一致（x27/x20/x1/x25 全部对上）。

### P3 · 崩溃指令反汇编与语义解码（crash_session3/4/16/17）
`dis -l ffffba8cc6dfae10 16` / `dis -l ffffba8cc6dfadc0 20` / `dis -l ffffba8cc6dfae00 28`（含源码行号）：
```
+300 (0x...ae34): ldp  x0, x1, [sp, #8]        ; fair.c:12050
+304 (0x...ae38): ldr  x2, [x28, #8]           ; fair.c:12053
+308 (0x...ae3c): ldr  x20, [x0, w25, sxtw #3] ; fair.c:12050  <- x20 = __per_cpu_offset[w25]
+312 (0x...ae40): mov  w0, w25                 ; fair.c:12054
+316 (0x...ae44): add  x27, x1, x20            ; fair.c:12050  <- x27 = &runqueues + offset
+320 (0x...ae48): ldr  x23, [x27, #288]        ; fair.c:5024   <- 崩溃点 (Code 括号指令)
```
结合 `+252: add x0,x24,#0x5d0; str x0,[sp,#8]` 与 crash_session16 栈验证：`[sp+8]=0xffffba8cc8c255d0=__per_cpu_offset`（sym 实证）、`[sp+16]=0xffffba8cc88296c0=&runqueues`（sym 实证）。指令语义 100% 确定：这是 `update_sg_lb_stats()`（fair.c:12050 `for_each_cpu_and(cpu, group_span, env->cpus)` 循环体内）标准的 `cpu_rq(cpu)` per-cpu 指针算术。

### P4 · w25 索引与期望值侧验证（crash_session22/25 + session2/7/8）
- dmesg x25=0x96=150 → 装载地址 `0xffffba8cc8c255d0+150*8=0xffffba8cc8c25a80`
- `rd ffffba8cc8c25a80` → `0xffffc573b8bda000`（内存中槽位值正确；且 crash_session2 `p __per_cpu_offset[179]`=0xffffc573b8fb4000 与同表交叉验证；session22 对 cpus 144-191 全表扫描，表项序列单调、无异常）
- 期望 x27 = `0xffffba8cc88296c0 + 0xffffc573b8bda000 = 0xffff8000814036c0`；`rd` 该地址 +2880 偏移处 = `0x96`（struct rq `cpu` 字段，crash_session23 `struct rq -o` 确认 cpu@2880）→ 该地址确为 cpu_rq(150)，其 `+0x120`（rq->cfs 平均负载区）内容 0x400/0x400/0x400/0x200 为合法 sched_avg 数据
- 反事实闭合：若无破坏，`ldr x23,[x27,#288]` 将正常读出 cpu150 的负载统计，循环继续

### P5 · 代数复算（algebra.py → algebra_out.txt，禁止手算）
见 §7。核心三等式全部 True。

### P6 · 软件成因排除（crash_session26-31 + dmesg）
- KASAN/UBSAN/debug_pagealloc 未编译：`sym kasan_report`/`__kasan_report`/`__asan_load8` → `symbol not found`（crash_session30）；命令行无相关参数（L387）
- 内核已知 bug 排除：find_busiest_group 该路径（6.6 主线久经运行）；若为软件 bug，应可跨 CPU 复现，而 14 次事件全部钉死 CPU179（192 核上随机分布的期望概率 ≈ (1/192)^14，二项检验 p≈1e-32）
- 编译器误编译排除：反汇编与源码行号一一对应，指令序列为 per-cpu 算术标准模式；bt 两侧（dmesg 与 crash）一致
- 数据结构损坏（use-after-free 等）排除：vmcore 中 `__per_cpu_offset[150]` 槽位、`cpu_rq(150)` 结构、sched_domain/groups 链（session14/18：sd level=6 "NUMA"、groups 链表完整、sgc 容量数据 sane）全部完好，内存侧零异常，仅 CPU 在途值坏
- 多 bit 形态排除单粒子翻转于 DRAM：x20 与真值 hamming 距离 31 位；DRAM 侧单粒子翻转无法产生该形态（且 vmemcore 中该数据正确）

### P7 · 根因收敛
软件成因全部排除 + 内存干净 + CPU 在途值坏 + 同核 9.24h 内 14 次（含 PTW/TLB 侧 13 次）→ 收敛到 CPU179 核内数据通路间歇性微架构缺陷（详见 §8）。

# 7. 逻辑链条（寄存器代数闭合与反事实）【逻辑链条】

## 7.1 指令语义（全部 crash 实测，非推测）

崩溃序列（5 条指令，见 P3）实现的就是：
```
x20 = __per_cpu_offset[cpu]            // per-cpu 偏移表第 cpu 项
x27 = &runqueues + x20                 // = cpu_rq(cpu)（per_cpu_ptr 标准算术）
x23 = *(u64*)((char*)x27 + 0x120)      // rq->cfs 平均负载（fair.c:5024, update_sg_lb_stats）
```
w25=x25=150（dmesg L3200 x25=0x96），处于 group span（cpus 144-191，crash_session14/18 cpumask 解码）第 7 次迭代。

## 7.2 核心闭合等式（python3 模 2^64，algebra_out.txt Q 节）【实锤】

```
(1) x1 + x20_obs = 0xffffba8cc88296c0 + 0x73b88cc000ffffc5
              = 0x173b8474cc9829685 (65-bit)
    (x1 + x20_obs) mod 2^64 = 0x73b8474cc9829685 == 观测 x27   [True]

(2) x27 + 0x120 = 0x73b8474cc98297a5 == dmesg FAR              [True]
    (指令 f9409377: size=3,V=0,opc=1,imm12=0x24,Rn=x27,Rt=x23 → 偏移 0x24*8=0x120)

(3) 期望侧: x1 + x20_true = 0xffffba8cc88296c0 + 0xffffc573b8bda000
                        = 0xffff8000814036c0 = cpu_rq(150)      [crash 实证 rq->cpu=150]
```
等式 (1)(2) 证明：x27 与 FAR 是破坏后的 x20 经 ADD、LDR 偏移确定性派生的值，并非独立破坏；等式 (3) 证明期望路径完全正常。三式构成双向闭合，本案不存在"碰巧相似"的空间（64 位值上双重精确匹配 + 第三侧结构体语义验证）。

## 7.3 x20 破坏形态分析（algebra_out.txt R 节）

```
x20_true = 0xffffc573b8bda000   字节(BE): ff ff c5 73 b8 bd a0 00
x20_obs  = 0x73b88cc000ffffc5   字节(BE): 73 b8 8c c0 00 ff ff c5
hamming(x20_true, x20_obs) = 31 位     → 排除寄存器单事件翻转
ror(x20_true, 40) = 0x73b8bda000ffffc5
ror(x20_true,40) ^ x20_obs = 0x0000316000000000 (5 位)
```
观测值恰为真值按大端字节道旋转 5 字节、再在字节 2-3 内翻转 5 位的形态。同时验证：LE 内存流中不存在能产生该值的对齐/非对齐读取（排除了"地址偏移读错位置"假说）；`__per_cpu_offset` 表的相关区间（cpus 144-191 全扫，session22；cpus 0-31 抽样，session18）无任何槽位含 `0x73b88cc000ffffc5` 或其字节重排（w25 由 group_span∩env->cpus 位搜索产生，算法上必落在 144-191，该区间已 100% 扫描；未扫的 32-143 区间与本路径无关）。

形态学结论：这不是位单元翻转，而是字节道（byte-lane）错位 + 多位错误的组合，指向装载返回通路（L1D 输出到 LSU/RF 写端口）的字节使能/对齐/复用逻辑层面的微架构缺陷。诚实声明：40-bit 旋转匹配是基于形态的最优解释而非唯一解释（另一候选：两条独立故障的叠加），置信度 0.6（形态部分）；但"数据通路在途破坏"这一结论本身不依赖形态解释（由 §7.2 闭合 + §7.4 内存干净双重锁定，置信度 0.95+）。

## 7.4 反事实推演与诚实边界

- 反事实 A：若 x20 装载正确 → x27=cpu_rq(150)，x23 读出合法负载值，循环继续 → 无 Oops。已由等式 (3) 闭环证明。
- 反事实 B：若破坏发生在 DRAM（槽位值持久坏）→ vmcore 中 `__per_cpu_offset[150]` 应为坏值，实测正确 → 排除。且若 DRAM 坏，后续任何核读该表都会出错，不会只钉死 CPU179。
- 反事实 C：若是软件（编译器/内核 bug/竞态）→ 应有跨 CPU 复现或内存侧可见损坏；14/14 钉死 CPU179 + 内存全净 → 排除。
- 诚实边界 1：13 次 WARNING 我们只能证明"已映射页上报 level-0 翻译故障"，无法进一步区分是 PTW 读坏还是 TLB 项坏（FAR 是 MMU 自己报告的，异常侧无寄存器可区分两者）；两者都属 CPU179 核内/ MMU 数据侧，不影响根因归属，但机制粒度上标注为推测。
- 诚实边界 2：无法证明 13 次 WARNING 期间没有其他未被观测的静默数据破坏（比如同样在途破坏但结果仍是合法值的装载）。SDC 的不可见性是本质性的，本报告只统计"被观测到的"14 次。
- 诚实边界 3：粒子来源（宇宙射线 vs 芯片缺陷）无法从单一转储判定；但 9.24h 内同核 14 次、跨越两类微架构部件（LSU 数据通路 + PTW/TLB）的复发率远高于单粒子事件的本底期望，支持"缺陷核"（deterministic defective core）而非随机单粒子。

## 7.5 6.2 秒末段（本案特有观察）

WARNING #12（33265.049s）→ #13（33271.244s）→ FATAL（33271.977s）：6.93s 内三连发。此前 29420s 静默。两种解释：(a) 缺陷活跃度非平稳（临近失效的恶化前兆）；(b) 泊松采样涨落（λ≈1.5/h 时 3 事件/6.9s 的概率虽低但非零）。样本量不足，如实记录、不作定论；若未来案例再现"长静默+末段三连发"模式，则 (a) 的置信度上升。

# 8. 故障根因【故障根因】

CPU 179 核内数据通路间歇性微架构缺陷（SDC 源），置信度 0.85。

- 直接根因：`ldr x20,[x0,w25,sxtw#3]`（find_busiest_group+0x138）从 `__per_cpu_offset[150]` 装载时，CPU 获得在途破坏值 `0x73b88cc000ffffc5`（内存实测正确值 `0xffffc573b8bda000`），经 `add x27,x1,x20` 派生野指针 `0x73b8474cc9829685`，再经 `ldr x23,[x27,#288]` 访问非规范地址 `0x73b8474cc98297a5` 触发 level-0 翻译故障 → Oops → kdump。
- 破坏位置收敛（介于装载执行与 ADD 消费之间的 CPU 可见通路）：L1D 数据阵列该行的瞬态位/道破坏、LSU 返回缓冲/对齐网络字节道错位、或物理寄存器堆 x20 槽位保持错误。三者在现有证据下不可再分，统称"核内数据通路"。
- 同源佐证：同核同窗口 13 次"已映射页伪翻译故障"。页表遍历读数/TLB 项同为 CPU179 私有数据通路（ARM 上 PTW 遍历经数据缓存），与 L1D→RF 通路共享部分电路域，构成"缺陷核"的多位点表现。
- 排除项：DRAM/内存控制器（内存侧全净）、RAS 可报事件（零记录）、软件栈（P6 全排除）、跨核/互连（仅 CPU179 可见）。
- 置信度构成：代数闭合 0.95+（等式三重锁定）；"核内数据通路"归属 0.9；"字节道错位"形态学 0.6；"缺陷核 vs 单粒子"0.7（复发率论证）。

# 9. 启示【启示】

## 9.1 微架构级启示：per-cpu 基址装载是 SDC 的"天然探测器"

`per_cpu_ptr(ptr, cpu) = ptr + __per_cpu_offset[cpu]` 是内核最高频的指针构造之一（调度、IRQ、VM 统计……每核每秒数千次）。本案证明：当 `__per_cpu_offset[]` 表项装载被破坏，输出指针高 16 位脱离 0xffff 规范域的概率极高（本例 0x73b8），会立刻转化为非规范地址 → 确定性 level-0 fault → 可观测崩溃。设计启示：内核关键基址表（per-cpu offset、node_data、mem_section 等）可增加副本校验读（load-twice-compare），以纳秒级代价把此类 SDC 从"静默传播"变为"即时捕获"。芯片侧对应地，可为指针生成指令序列提供"canonicality hint"（如 ARM64 上 TBI 之外的 high-bit 保留位检查），让非规范指针在 ALU 输出端即被标记。

## 9.2 LSU 返回通路的端到端数据完整性

本案 x20 的破坏形态（字节道旋转 + 多位翻转）发生在"缓存命中、地址正确、数据在途"的窗口，这正是无 ECC 保护的 CPU 流水线段。L1D/L2 有 ECC/奇偶，但 L1D 输出驱动→LSU 对齐网络→物理寄存器堆写端口这段距离，在多数商用核上仅有偶发奇偶或干脆无保护。设计启示：(a) 对 LSU→RF 返回总线加 lane-level 奇偶/GRUPO 码，异常时重发（load retry）而非放行；(b) 物理寄存器堆读出加 SECDED，寄存器堆软错误率在先进工艺下不可忽略；(c) PTW/TLB 数据侧同样受益（本案 13 次 WARNING 全部落在这一段）。

## 9.3 "spurious translation fault on mapped page" 应成为一等公民遥测

13 次 WARNING 是芯片缺陷在崩溃前 9 小时持续发出的免费告警，但现有 RAS 栈（GHES/EDAC/SDEI）对它们零感知，因为它们不产生任何固件可报的 error record。设计启示：(a) 内核侧：对 `fault.c:494` 的 spurious fault 增加 per-cpu 计数器 + ratelimit 上报（perf/tracepoint），把"已映射页翻译故障"作为 SDC 早期信号纳入监控（本案若有此遥测，1391s 即可隔离 CPU179）；(b) 固件侧：把 MMU 侧异常（translation fault on valid mapping）纳入 RAS 事件源；(c) 业务侧：silifuzz 类 fleet 若对 /proc/interrupts 读取做拦截统计，可直接用 WARNING 日志做缺陷核聚类。

## 9.4 缺陷核隔离（defective core fencing）的工程价值

14/14 事件钉死 CPU179（二项 p≈1e-32）。若系统具备 per-cpu SDC 信号聚合 + 自动 offline（`echo 0 > /sys/devices/system/cpu/cpu179/online`），本次崩溃完全可避免：WARNING #1（1391s）时即可止损，后续 9 小时及最终 panic 都不会发生。设计启示：RAS 设计应支持"核级隔离"粒度（而非仅 DIMM/链路级），并把内核侧软件异常（spurious fault、非规范指针 Oops）作为隔离决策的输入信号源。

## 9.5 对 SDC 谱系的增量数据点

本系列案例中本案首次提供：(a) 完整传播链闭合（原始破坏量→派生指针→FAR 三级代数锁定，误差概率 ≈ 2^-128 量级）；(b) 期望值侧反事实验证（cpu_rq(150) 结构体语义确认）；(c) 同一缺陷核跨部件复现证据（LSU 通路 1 次 + PTW/TLB 13 次）；(d) 原始破坏量的多位/字节道形态学（hamming 31 位，非单翻转）。这些为"SDC 源于核内数据通路而非存储介质"提供了目前最完整的一次实证。

# 10. 处置建议

1. 立即：对 fleet 中本机型（R240K V2 / HIP08）grep `Ignoring spurious kernel translation fault` 日志；同核 ≥2 次即列入缺陷核候选清单。
2. 短期：本节点 CPU179 offline（或换机）；在节点监控中加入 per-cpu spurious-fault 计数告警（阈值建议：单核 24h ≥2 次）。
3. 中期：向 vendor（HiSilicon/鲲鹏）提交本案完整证据链（本报告 + vmcore），申请该 CPU 批次筛查；确认 PTW/TLB 与 LSU 返回通路是否有已知 erratum（对照 HIP08 Taishan v110 errata 文档，本案未获得该文档，无法核对，如实声明）。
4. 长期：推动 9.2/9.3 的设计改进进入下一代平台需求（LSU→RF 返回通路端到端数据保护 + MMU 异常纳入 RAS 遥测）。
5. 取证保留：本 vmcore（11G）与 dmesg 建议归档 ≥90 天，供 vendor 复检。

# 附录：命令索引（全部取证命令，可复核）

## ① dmesg 勘察
```
wc -l /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-21:53:28/vmcore-dmesg.txt     # 3230
grep -n -iE 'Linux version|Command line|SMP|Memory:' vmcore-dmesg.txt
grep -n -E 'WARNING|Oops|Internal error|spurious|unable to handle' vmcore-dmesg.txt
grep -n -iE 'ras|edac|mce|ecc|ghes|apei|cper|hardware error' vmcore-dmesg.txt        # 仅基础设施命中
grep -n 'MPIDR 0x7a0300\|PXM 7 ->\|node   7:' vmcore-dmesg.txt
sed -n '2591,2635p;3183,3230p' vmcore-dmesg.txt
```
## ② crash 会话（crash_session1~33.log，逐条对应）
- session1：sys / bt / set 179 / task（崩溃帧 + cpuhp/33 上下文）
- session2：`p __per_cpu_offset[179]`、`p &runqueues`、`struct sched_group`
- session4：`bt -t 581185`、`rd ffffba8cc6dfae38`、`dis -l ffffba8cc6dfae10 16`（崩溃指令序列）、崩溃帧栈
- session5：`rd ffff8000cb18b740 40`（[sp+8]=__per_cpu_offset、[sp+16]=&runqueues 实证）
- session7/8：`sym ffffba8cc8c255d0`、`p ((struct rq *)0xffff8000817dd6c0)->cpu`（=179）
- session9：`vtop` ×12（6 个 spurious 地址全 VALID + rq/percpu 符号地址物理定位）
- session10/11：`kmem` ×5 + `rd`（spurious 地址全在已分配 kmalloc 对象内；/proc 内容 ASCII 可见）
- session13：`bt` pmdalinux/irqbalance（WARNING 受害任务现场，正在 pipe_read/ppoll 睡眠）
- session14：`p ((struct sched_domain *)0xffff6040043a3200)->{level,span,groups,span_weight,name}`、`struct sched_domain`
- session16/17：`dis -l ffffba8cc6dfadc0 20` / `dis -l ffffba8cc6dfae00 28`（x20 生成链完整解码）
- session22：`rd ffffba8cc8c25a50 24` + `rd ffffba8cc8c25b10 24`（__per_cpu_offset[144..191] 全表）
- session23：`struct rq -o`（cpu@2880、cfs@128 → +0x120 落在 sched_avg 区）
- session25：`rd ffff8000814036c0` / `rd ffff8000814037e0`（cpu_rq(150) 验证：cpu=0x96、负载字段 sane）
- session30：`sym kasan_report` 等 → not found（KASAN 未编译）
- session32：`task -R start_time`（mi-scavenger 创建于 uptime 3786.3s）
- 失败命令如实记录：session2 `p xtime`/`dis -l ...,8`/`p sched_group_span`（gdb/语法拒绝）、session29 `rd -a`（页排除）、各会话头部 `seek error`（IRQ/SDEI 栈指针噪声）

## ③ 代数复算（禁止手算）
```
cd <报告目录> && python3 algebra.py > algebra_out.txt
# 核心输出（algebra_out.txt SECTION P/Q/R/S/T/U）：
#   ADD CLOSURE: True   FAR CLOSURE: True   hamming(x20)=31   ror40 残差 popcount=5
```

## ④ 环境与工具
- crash 8.0.4-17.oe2403sp4 + /tmp/vmlinux-0102（BuildID 276194e5，6.6.0-145.3.23.154.oe2403sp3.aarch64）
- vmcore：/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-21:53:28/vmcore（11,772,935,568 字节）
- 附件：dmesg_forensics.txt（dmesg 全量勘察）、algebra.py / algebra_out.txt（代数）、crash_session1~33.log（原始取证输出）

# CPU179 转储深度诊断报告（第 1 次独立重研究）

## ——装载结果"字节歪斜+错槽"双重异常实锤：一条 `ldr x20,[x0,w25,sxtw#3]` 读出了 `__per_cpu_offset[0]` 右移 8 位的非对齐窗口，而内存真值完好无损

| 项目 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-25-15:58:09/vmcore`（9.98GB，Kdump compressed v6，PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 核 Kunpeng-920（HIP08 平台，MIDR 0x481fd010 = TaiShan-v110），768GB RAM，8 NUMA 节点 |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP，openEuler 2403sp3，KASLR 开启 |
| 崩溃时间 | 2026-08-25 15:57:13 CST（crash sys DATE），uptime 418.7 秒（约 7 分钟）；目录名 15:58:09 为 kdump 落盘标签 |
| 受害进程 | PID 1931 `kworker/179:1H`（CPU179 高优先级 block 内核工作线程，kblockd 工作队列），newidle 负载均衡路径 |
| 前兆异常 | 无内核级前兆。本次开机 dmesg 全程零 WARNING / 零 BUG / 零 Oops / 零硬件错误事件（RAS 负证据；唯一含"warning"字样的第 2303 行为 systemd-journald 用户态常规提示，与内核无关） |
| 结论一行 | 【实锤】CPU179 的 load 通路（AGU 有效地址生成/装载定序）发生一次性受扰：一条按 `w25=146` 索引的对齐装载，实际取回的是 `__per_cpu_offset[0]+1 字节` 处的非对齐 8 字节窗口（= 元素 0 右移 8 位），索引项 `0x490` 整体消失并伴随 +1 字节歪斜；寄存器堆与 ALU（ADD）经逐位闭合验证均健康，内存真值完好。置信级别：微架构定位【强推】，事件物理成因【假设】 |

---

## 1. 执行摘要

1. **现象**：开机后 418 秒，CPU179 上的 `kworker/179:1H` 在 newidle 负载均衡的 `find_busiest_group()` 内核热路径上触发 level 0translation fault：`Unable to handle kernel paging request at virtual address 00ffb34569fc3ac0`，该地址落在用户/内核地址区间之间的"空洞"里，直接 panic + kdump。
2. **签名**：致命指令是 `find_busiest_group+0x140` 处的 `ldr x23, [x27, #288]`（Code 窗口 `(f9409377)` 反汇编实锤）；坏基址 `x27 = 0x00ffb34569fc39a0` 由上一条 `add x27, x1, x20` 生成，其中 `x1 = runqueues`（正确），而 `x20 = 0x00ffffcc879da2e0` 是一个形状怪异的 48 位值——它本应是 `__per_cpu_offset[146]`（CPU146 的 per-cpu 偏移）。
3. **闭合验证结果**【实锤】：crash 读出内存真值 `__per_cpu_offset[146] = 0xffffcc879ed92000`（完好、步进规整 0x22000），而寄存器里的 x20 **不等于任何槽的对齐真值**；经逐字节扫描，`x20 == *(u64*)(&__per_cpu_offset[0] + 1字节) == __per_cpu_offset[0] >> 8`，在 base+0..base+11 的全部窗口中**唯一命中 base+1**。即：这条以 `w25=146` 为索引的对齐装载，实际取回的是**槽 0 右移 8 位**的数据——错槽（索引项 0x490 消失）与非对齐（+1 字节）两个异常叠加在一次装载里。
4. **反事实推演**【实锤】：若装载返回真值，`x27 = rq(146) = 0xffff80008137b6c0`（与 crash `px runqueues` 的 per-cpu 地址表逐位一致），且 `vtop` 证实该页 PTE=VALID 已映射——指令本会正常完成，不会崩溃。因此崩溃 100% 归因于该次装载结果被腐化，而非内存被写坏或软件逻辑错误。
5. **置信**：微架构定位为 **load 通路（AGU 地址生成/装载定序）单次受扰**【强推】——寄存器堆（x1/x21/x24/x25 等其余寄存器全部与静态反推一致）、ALU（`x27=x1+x20` 模 2^64 精确闭合）、内存阵列（真值完好）三者均被排除。物理成因（宇宙线/电压/时序毛刺等）无法用软件手段验证，为【假设】。
6. **处置**：本机为单次瞬时受扰（该开机内无任何前兆、无重复）；建议保留 dump 归档、继续观察同机后续转储的 CPU 号与通路是否收敛；无需立即停机换件，但若再发且集中在同一核/同一通路，则升级为硬件缺陷处置。

## 2. 证据规则与方法

- **证据源**：仅 `vmcore`、`vmcore-dmesg.txt`（原始数据）+ crash 8.0.4-17.oe2403sp4（namelist `/tmp/vmlinux-0102`，ELF aarch64，BuildID 276194e5...，带 debug_info）+ objdump 静态反汇编。**未阅读**任何既有诊断文档（docs/cases、docs/hypothesis、docs/cpu、.planning、dump 目录内 .md 等），保证独立性。
- **诚实铁律**：报告中每一处数据引用均出自实际运行的命令真实输出（全文见附件 `dmesg_forensics.txt`、`crash_forensics_full.txt`、`crash_forensics_v2.txt`）。所有 64 位地址运算由 `algebra.py`（模 2^64）复算并断言通过，输出存 `algebra_out.txt`，零手算。
- **三级置信**：【实锤】= 可直接复核的闭合（如 far−x27=0x120）；【强推】= 多源收敛但无法软件直接观测（如 AGU 受扰）；【假设】= 无法软件验证，给出验证途径。
- **工具清单**：crash（sys/panic/bt/bt -t/set/mach/ps/rd/p/px/vtop/dis -l/struct/search）、grep/sed（dmesg 法证）、objdump -d（按精确地址反汇编）、python3（模 2^64 代数与字节流重建）。
- **加载事实**：vmcore 为 PARTIAL DUMP，crash 加载成功（192 CPU、2028 任务、767.8GB），伴随大量 "IRQ stack pointer" seek error（不完整转储的正常表现，不影响本案取证目标地址的读取）。

## 3. 本次开机时间线【时间线】

| uptime（秒） | 墙钟（CST，由 crash DATE−uptime 反推） | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0.000000 | ≈15:50:14 | 开机，`Booting Linux on physical CPU 0x0000080000 [0x481fd010]`，KASLR enabled | 1 | 【实锤】 |
| 0.000000 | — | crashkernel 预留 1024MB high + 128MB low；内核命令行（含 `crashkernel=1024M,high`） | 121-122, 387 | 【实锤】 |
| 0.349767 | — | `smp: Brought up 8 nodes, 192 CPUs` | 1255 | 【实锤】 |
| 0.855409 | — | `EDAC MC: Ver: 3.0.0`；1.487s ghes_edac 接管（探测注册，非错误事件） | 1860, 2179 | 【实锤】 |
| 1.762526 | — | systemd v255 进入系统模式（第二次 22.30s 为 initrd→系统切换） | 2284, 2488 | 【实锤】 |
| 29.014296 | — | EXT4 挂载 dm-2、sda2（rootfs 就绪） | 2581-2582 | 【实锤】 |
| 41.082267 | — | `hns3 enp189s0f0: link up`（万兆网卡链路 UP） | 2584 | 【实锤】 |
| 72.307686 | — | `block dm-2: the capability attribute has been deprecated`（最后一条普通日志） | 2585 | 【实锤】 |
| 418.373008 | ≈15:57:13 | **致命 Oops**：`Unable to handle kernel paging request at 00ffb34569fc3ac0` | 2586 | 【实锤】 |
| 418.729993–418.744655 | ≈15:57:13 | `SMP: stopping secondary CPUs` → `Starting crashdump kernel` → `Bye!` | 2626-2628 | 【实锤】 |
| （开机全程） | — | 前兆异常：**0 条**（无 WARNING/BUG/Oops/spurious/硬件错误） | — | 【实锤】 |

时间间隔：72.3s（最后业务日志）→ 418.4s（panic），静默 346 秒后突发，无任何渐变征兆。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（vmcore-dmesg.txt 第 2586-2628 行）

```
[  418.373008] Unable to handle kernel paging request at virtual address 00ffb34569fc3ac0
[  418.381678] Mem abort info:
[  418.385176]   ESR = 0x0000000096000004
[  418.389628]   EC = 0x25: DABT (current EL), IL = 32 bits
[  418.395644]   SET = 0, FnV = 0
[  418.399397]   EA = 0, S1PTW = 0
[  418.403239]   FSC = 0x04: level 0 translation fault
[  418.408819] Data abort info:
[  418.412398]   ISV = 0, ISS = 0x00000004, ISS2 = 0x00000000
[  418.418584]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[  418.424337]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[  418.430353] [00ffb34569fc3ac0] address between user and kernel address ranges
[  418.438198] Internal error: Oops: 0000000096000004 [#1] SMP
[  418.546640] CPU: 179 PID: 1931 Comm: kworker/179:1H Kdump: loaded Not tainted 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1
[  418.557951] Hardware name: Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026
[  418.566401] Workqueue:  0x0 (kblockd)
[  418.570772] pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[  418.578439] pc : find_busiest_group+0x140/0xb60
[  418.583679] lr : find_busiest_group+0x11c/0xb60
[  418.588911] sp : ffff8000ae213960
```

ESR 解读：EC=0x25（当前 EL 的数据中止，即内核态读装载触发）、WnR=0（读）、FSC=0x04（**level 0 translation fault**——页表第 0 级（PGD）就没有命中，说明该地址根本不在任何页表覆盖范围内，配合"address between user and kernel address ranges"判定：0x00ffb34569fc3ac0 落在 TTBR0/TTBR1 都不覆盖的地址空洞）。S1PTW=0（非页表走查本身出错，是普通数据访问）。

### 4.2 全量寄存器（x0–x30，dmesg 原文）

```
x29: ffff8000ae213ae0 x28: ffff8000ae213a70 x27: 00ffb34569fc39a0
x26: ffff604003e26c00 x25: 0000000000000092 x24: ffffb378e29e5000
x23: 0000000000000400 x22: ffff604003e26c00 x21: ffffb378e29dfcb0
x20: 00ffffcc879da2e0 x19: ffff8000ae213b70 x18: 0000000000000000
x17: 0000c00000000000 x16: 0000000000000000 x15: 0000000000000008
x14: 0000000000000001 x13: 000000003b73e310 x12: ffff8000ae2139f8
x11: 00000000000001ff x10: 0000000000000000 x9 : ffffb378e0bbae58
x8 : ffff8000ae213ac8 x7 : 0000000000000000 x6 : 0000000000000092
x5 : fffffffffffc0000 x4 : 0000000000000002 x3 : 0000000000000012
x2 : 0000000000000800 x1 : ffffb378e25e96c0 x0 : 0000000000000092
```

### 4.3 Call trace（完整）

```
 find_busiest_group+0x140/0xb60      <-- 致命点
 load_balance+0x108/0x6c0
 newidle_balance+0x198/0x510
 pick_next_task_fair+0x110/0x718
 pick_next_task+0x60/0x398
 __schedule+0x1b4/0x8a0
 schedule+0x58/0x130
 worker_thread+0x1a8/0x360
 kthread+0xec/0x100
 ret_from_fork+0x10/0x20
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

### 4.4 前兆异常原文

**无内核级前兆**。`grep -n -E 'WARNING|BUG|Oops|cut here|spurious'` 在全部 2628 行 dmesg 中零命中。唯一含 "warning" 字样的是 systemd-journald 的用户态提示（第 2303 行，`This warning is only shown for the first unit using IP firewalling`，属常规服务启动信息，与内核故障无关）。本次开机从 t=0 到 panic 前一条日志（72.3s）完全干净——这是"单次瞬时受扰、无累积退化"的典型形态。

### 4.5 RAS 负证据（dmesg grep）

```
1860:[    0.855409] EDAC MC: Ver: 3.0.0
2179:[    1.487143] EDAC MC0: Giving out device to module ghes_edac.c controller ghes_edac: DEV ghes (INTERRUPT)
```

除 EDAC/GHES 探测**注册**行外，无任何 `Machine check` / `Hardware error` / `ECC` / `memory failure` / `ras` 错误事件。即：硬件未上报任何可纠正/不可纠正错误——SDC 的"静默"属性成立，固件 RAS 通路对该事件零感知。

## 5. 业务现象

受害进程 `kworker/179:1H` 是 CPU179 绑定的**高优先级（1H = kblockd 的 KHIGHPRI 工作线程池）块层工作队列**内核线程（dmesg `Workqueue: 0x0 (kblockd)`）。崩溃上下文是它干完活后准备睡眠：`worker_thread → schedule → __schedule → pick_next_task → newidle_balance`——CPU179 即将进入空闲，调度器想在挑下一个任务前顺手做一次偷闲负载均衡（newidle balance）。`find_busiest_group` 遍历调度域里各 CPU 的运行队列负载，为"是否值得从别的 CPU 偷任务"做决策。

关键点：这是**纯内核调度器热路径**，与任何业务数据、驱动、外部输入无关。该函数在 192 核机器上每秒被执行成千上万次，且出错的那次循环是读取 `__per_cpu_offset[146]`（CPU146 与 CPU179 同属一个调度域组，跨核读取组内兄弟 CPU 的 per-cpu 数据是常规操作）。

## 6. 诊断定位过程【诊断定位过程】

### P1 · dmesg 勘察

开机指纹齐全（版本/命令行/192 核/768GB/8 节点/crashkernel 1024M high）。崩溃块完整：ESR/EC/FSC、全量寄存器、call trace、Code 窗口俱全。前兆与 RAS 均为负证据。初步判定：内核态读装载触发 level-0 翻译故障，地址 `00ffb34569fc3ac0` 形如"带垃圾高位前缀的合法低位"——x27/x20 两个 `00ff...` 开头的寄存器立即成为头号嫌疑。

### P2 · 崩溃块提取 + 指令语义（objdump 静态反汇编 + crash `dis -l`）

由 KASLR 滑移量（`x1` 观测值 0xffffb378e25e96c0 − 静态符号 `runqueues` 0xffff800081b696c0 = 0x337860a80000，与 `find_busiest_group` 运行时/静态差完全一致，且 sp = x29−0x180 与帧布局吻合）定位到源码级指令序列（fair.c:5024 附近，`dis -l` 证实 `find_busiest_group+0x140` = fair.c:5024 的 `ldr x23,[x27,#288]`）：

```
+0x118: bl  _find_next_and_bit        ; 在组掩码里找下一个 CPU
+0x11c: mov x25, x0                   ; x25 = cpu (观测 0x92 = 146)
+0x12c: ldp x0, x1, [sp, #8]          ; x0 = &__per_cpu_offset[0], x1 = runqueues
+0x130: ldr x2, [x28, #8]
+0x134: ldr x20, [x0, w25, sxtw #3]   ; x20 = __per_cpu_offset[cpu=146]  <-- 嫌疑装载
+0x138: mov w0, w25
+0x13c: add x27, x1, x20              ; x27 = &per_cpu(rq, 146) = rq(146)
+0x140: ldr x23, [x27, #288]          ; rq->... 读运行队列字段  <-- 致命
```

对应 C 语义即 `for_each_cpu(cpu, sched_group_span(group))` 循环内的 `struct rq *rq = cpu_rq(cpu)`（`cpu_rq(i) = per_cpu_ptr(&runqueues, i) = runqueues + __per_cpu_offset[i]`），随后读 rq 内偏移 288 的负载字段。

### P3 · crash 加载（PARTIAL DUMP，成功）

`sys/panic/bt/bt -t/set/mach/ps` 全部正常输出：PANIC 字符串与 dmesg 一致，bt 全帧与 dmesg call trace 一致，崩溃 CPU 179 / PID 1931 确认。附带的 384 条 "IRQ stack pointer" seek error 属于不完整转储的预期噪音，未影响本案所有取证地址的读取。

### P4 · 内存真值对照（决定性实验）

- **栈上槽位对账**：`rd ffff8000ae213960`（崩溃帧 sp）显示 `[sp+8] = ffffb378e29e55d0`（= `px &__per_cpu_offset` 输出的运行时数组基址）、`[sp+16] = ffffb378e25e96c0`（= `runqueues` 运行时地址）——`ldp x0,x1,[sp,#8]` 的两个源槽完好，装载基址与锚点均正确。
- **数组真值**：`rd ffffb378e29e55d0 64` 及元素区扫描显示整个 `__per_cpu_offset[]` 从 `0xffffcc879da2e000`（[0]）起按固定步进 0x22000（34 页/单元）递增，无任何被写坏的槽位。其中：
  - `p __per_cpu_offset[146]` = 18446687481590521856 = **0xffffcc879ed92000（真值，完好）**
  - `p __per_cpu_offset[0]` = 18446687481570189312 = **0xffffcc879da2e000**
- **真值自洽交叉验证**：`px runqueues` 给出的 per-cpu 地址表 `[146]: ffff80008137b6c0`、`[179]: ffff8000817dd6c0`，与"runqueues + offset[i]"逐位一致（algebra.py 第 7 节断言通过）；且 `rd`/`vtop` 证实 rq(179) 是结构合理（空链表自指针）的真实运行队列、rq(146) 页面已映射（PTE VALID）。
- **寄存器 vs 真值**：x20 观测值 `0x00ffffcc879da2e0` **不等于** [146] 真值，也不等于任何槽的对齐值（所有槽低 3 字节均为 0x000，而观测值低字节为 0xe0——观测值**不可能**来自任何对齐读）。逐字节重建（algebra.py 第 5 节）：观测值 == `*(u64*)(&__per_cpu_offset[0] + 1)` == **`__per_cpu_offset[0] >> 8`**，在 base+0..base+11 全部窗口中唯一命中 base+1。

### P5 · 软件成因排除

1. 该指令序列是调度器最热路径之一，192 核每秒执行无数次；若是软件 bug 或编译错误不可能只在此刻此核出现（软件 bug 不挑核、不挑时刻）。
2. 其余寄存器全部健康：x1/x21/x24 与静态符号+KASLR 滑移逐位吻合，x25=0x92（CPU146 在组掩码内，逻辑自洽），x29/sp 帧链吻合。若寄存器堆或上下文切换出错，不可能只坏一个 x20。
3. ADD 单元健康：`x27 = x1 + x20 (mod 2^64)` 精确成立（algebra.py 第 3 节断言）——坏值完整地、无传播损耗地通过了 ALU，说明 ALU 输入输出均正常，坏的是它的**输入**（x20 的装载结果）。
4. 内存阵列健康：数组真值完好、步进规整、相邻元素无恙；且"对齐读不可能返回错位字节窗口"这一指令集事实，排除了"内存被写坏后被正常读到"的可能——被写坏的内存在**对齐**地址上不可能出现低字节 0xe0。
5. 页表健康：FSC=level 0 是因为目标地址本身落入地址空洞，而非页表项损坏（vtop 走查正确的 rq(146) 页 PTE 完好）。

### P6 · 定位收敛

装载结果 x20 = 槽 0 的 8 字节数据整体右移 8 位（字节窗口歪斜 1 字节）+ 索引项（w25 sxtw #3 = 0x490）整体消失。一次对齐的 `ldr` 在功能正确的核上**不可能**产生这样的结果：它要求有效地址既丢了变址项、又带了 +1 字节的非对齐量。最小故障假设是 **AGU/装载定序单点受扰**（详见第 7-8 节）。全过程无任何一处依赖猜测，每一步都有命令输出支撑。

## 7. 逻辑链条

**指令语义**（Code 窗口 `f9409377` 解码 + objdump 双重确认）：

```
ldr x20, [x0, w25, sxtw #3]   ; x0=&__per_cpu_offset[0], w25=0x92(146)
  EA应 = x0 + (146 << 3) = base + 0x490（8字节对齐，指向槽146）
ldr 的架构语义：从 EA 取 8 字节装 x20，不做任何移位/掩码
```

**闭合等式**【实锤，全部由 algebra.py 模 2^64 断言通过】：

| 等式 | 结果 |
|---|---|
| far − x27 | = 0x120（致命指令 imm12=36×8=288 的位移，EA 与 FAR 完全闭合） |
| x1 + x20 (mod 2^64) | = x27（观测值），ADD 精确闭合 → ALU 健康 |
| x20 (观测) vs `__per_cpu_offset[146]` 真值 | **不等**，XOR=0xff00334b194482e0，popcount=26 |
| x20 (观测) vs `__per_cpu_offset[0] >> 8` | **相等**（0x00ffffcc879da2e0，逐位） |
| 字节流重建 `read(base+k)`，k=0..11 | 唯一 k=1 命中观测值 |
| 隐含 EA − base | = 0x001（期望 0x490；XOR=0x491，popcount=4，**非单比特翻转**——变址项整体消失 + 1 字节歪斜） |
| runqueues + off[146] | = 0xffff80008137b6c0 == crash `px runqueues[146]`（真值自洽） |
| runqueues + off[179] | = 0xffff8000817dd6c0 == crash `px runqueues[179]`（真值自洽） |

**反事实推演**【实锤】：若该装载返回真值，x27 = rq(146) = 0xffff80008137b6c0，`vtop` 证实 PTE=VALID 已映射，`ldr x23,[x27,#288]` 正常完成，循环继续，本机大概率至今无感运行。崩溃唯一必要条件就是"x20 拿到坏值"。

**诚实声明】：
- 以上全部为可复核实证；但"AGU/装载定序受扰"本身是推断——软件无法直接观测流水线内部。给出验证途径：(a) 若该 CPU 可运行不受控的物理错误注入（粒子/电压），观察 load 通路错误率是否可复现；(b) 检查同机同核后续转储是否再现"错位字节窗口"签名；(c) 用体系结构级模拟器（如 gem5 精确流水线模型）注入 AGU 翻转，验证能否产生同形签名。
- PARTIAL DUMP 的 seek error 使部分取证（kmem -s 等）不可用，但本案全部关键地址的读取均成功，结论不受影响。
- 不排除"load 数据通路字节使能歪斜 + 槽选择错误"两次独立受扰的复合解释（见第 8 节），但单点 AGU 解释更简约。

## 8. 故障根因（微架构级结论 + 置信级别）

**微架构定位：load 通路的一次性受扰，最可能落在 AGU 有效地址生成/装载定序环节【强推】。**

证据形态拆解：
1. **装载结果塌缩为"错误来源 + 错误对齐"的组合**：正确槽（146）的真值完好，观测值却精确等于槽 0 的 1 字节非对齐窗口。这排除"内存写坏"（内存好），排除"ALU 算错"（ADD 闭合），排除"寄存器堆坏"（其余 30 个寄存器全对）。
2. **非对齐窗口是铁的证据**：所有合法槽都是 0x...000（页对齐步进 0x22000），任何对齐 8 字节读都不可能返回低字节 0xe0 的值。观测值低字节 0xe0 只能来自跨字节边界的窗口——即该装载实际寻址了一个 mod 8 = 1 的地址。
3. **索引项整体消失而非位翻转**：期望偏移 0x490 vs 隐含偏移 0x001，XOR popcount=4 且无单比特关系。更像变址 µop 的贡献在地址加法中被整体丢弃，同时混入常数 1——符合"AGU 内scaled-index 加法操作数受扰/丢失"或"错误装载请求被提交"的形态。
4. **单点故障的简约性**：一次 EA 生成错误（index→0 且 +1）即可同时解释错槽与错位两个表象；相比之下"读对槽但数据通路整体右移 8 位 + 槽选错"需要两次独立受扰，概率上不合理。

流水线归属：异常发生在**取数（AGU→L1 访问）阶段**，而非 PTW（页表走查无错，S1PTW=0，FSC level-0 是坏地址的自然结果）、非 ALU、非寄存器堆写口。dmesg 无任何 ECC/RAS 事件 → 该受扰未被 L1/LLC 的奇偶或 ECC 检出，说明扰动发生在**错误检测覆盖之外**的环节（AGU 内部地址加法器/定序逻辑通常无 ECC）。

置信级别：
- "x20 装载结果被腐化为槽 0 的 +1 字节窗口"——【实锤】（逐位闭合+唯一命中）
- "腐化发生在 AGU/装载定序（load 通路）"——【强推】（排除法收敛：内存/ALU/RF/页表全排除；但流水线内部不可软件观测）
- "物理成因（宇宙线单粒子、电源毛刺、时序裕量）"——【假设】（无法软件验证；单次、无前兆、不复现的形态与高能粒子翻转吻合，但无直接证据）

## 9. 启示

### 9.1 微架构定位的意义

本案证明：仅凭一份 dmesg + 一份 vmcore，通过"寄存器观测值 ↔ 内存真值 ↔ 指令语义"三方闭合，可以把一个 SDC 定位到流水线的具体段落（本案：AGU/装载定序）。这套方法不需要额外硬件支持，任何一次内核 panic 都可以按此流程归档，形成"错误形态→微架构环节"的统计库。当同一签名重复出现时，统计库能把【假设】级的物理成因升级为【强推】甚至【实锤】。

### 9.2 芯片设计与实现启示（具体、可落地）

1. **AGU/地址通路加奇偶或 ECC**：L1D 数据阵列通常有 ECC，但**有效地址生成加法器的输出**（index×scale + base）一般无任何保护——本案签名恰好落在这个盲区。建议在 AGU 输出加 1 位奇偶（成本：每 64 位地址 1 个异或树 + 1 比特寄存器），不匹配时触发 faultfast（宁可 panic 不可静默传播）。
2. **装载结果一致性校验（load-verify）**：对关键路径（内核态、且 EA 非对齐却由对齐寻址模式产生——这在正常执行中不可能出现）加微码级断言：对齐寻址模式的 load 返回非对齐窗口特征（如高字节来自相邻元素）时拉响警报。本案中"x20 低字节 ≠ 0x00 而源是页对齐数组"本可以在 retire 前被廉价的模式校验抓住。
3. **SDC 的 fail-fast 权衡**：本案坏值最终因落入地址空洞而硬件 panic（"歪打正着"的可观测化）。若坏值恰好是一个**合法**内核地址，将静默毒化调度决策（错误偷负载/不偷负载），无任何报错。设计上应倾向"可检测的崩溃"优于"不可检测的静默错误"：对 per-cpu 基址指针类装载提供可选的冗余读校验（双读比对，代价仅在调度器冷路径）。
4. **RAS 覆盖率声明**：建议芯片手册明确标注"AGU/定序逻辑不在 ECC 覆盖范围内"，让系统软件对此类签名的出现有先验预期，而不是像本案一样 RAS 全静默。
5. **DFT/在线测试钩子**：为 load 通路提供在线 LBIST 入口（运行时可低频扫描 AGU），使"无前兆单次受扰"在事后可归因。

### 9.3 对系统软件/RAS 的启示

1. **调度器关键指针的廉价校验**：`__per_cpu_offset[]` 全部元素是页对齐且步进固定的（本案 0x22000），内核可在 init 后保存一份期望值哈希或用 `(offset & 0xfff)==0 && offset<max` 做廉价的消费端 sanity check，把此类 SDC 从"panic 或静默"变成"带签名的 WARN + 一次重读"，兼顾可观测性与可用性（重读大概率拿到真值）。
2. **percpu 访问的容错读**：`this_cpu_ptr`/`per_cpu_ptr` 若读到明显非法的偏移（如落入地址空洞），先做一次重读再决定 panic——因为物理内存是真值完好的，重读即可自愈，且为硬件团队留下第一现场（寄存器副本）。
3. **kdump 价值**：即使 RAS 全静默，一份完整寄存器 + 内存真值的 dump 仍足以闭环定位——建议保留"崩溃时全量寄存器 + 关键全局数组"进 crash kernel 的最小集，这是 SDC 取证的最后一道防线。

## 10. 处置建议

1. **本机**：单次瞬时事件、无前兆、无重复，暂不换件；将本案签名（"per_cpu_offset 错槽+字节歪斜"）登记进签名库，与同机后续转储自动比对。
2. **监控**：对同批次机器开启 dmesg 前兆关键字（WARNING/Oops/ECC）集中采集；本案形态（开机 7 分钟、静默突发）提示若同类再发且集中在相近 uptime 窗口，应怀疑开机后电压/频率爬升期的时序裕量问题。
3. **升级条件**：若同核（CPU179）或同签名再次出现 → 【假设】升级为【强推】硬件缺陷，安排该核离线/换件。
4. **归档**：本报告目录含全部可复核附件，第三方可用 `algebra.py` 一键重跑全部闭合断言。

## 附录：命令索引（全部取证命令，可复核）

```
# dmesg 法证
ls -la /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-25-15:58:09/
wc -l <dump>/vmcore-dmesg.txt ; head -30 <dump>/vmcore-dmesg.txt
grep -n 'Command line' <dump>/vmcore-dmesg.txt
grep -n -E 'crashkernel|Memory:|SLUB:|Brought up|Booting Linux' <dump>/vmcore-dmesg.txt
sed -n '2580,2628p' <dump>/vmcore-dmesg.txt                      # 崩溃主块
grep -n -E 'WARNING|BUG|Oops|cut here|spurious' <dump>/vmcore-dmesg.txt      # 前兆(0条)
grep -n -iE 'machine check|hardware error|EDAC.*error|ECC|ras|memory failure|segfault|mce' <dump>/vmcore-dmesg.txt  # RAS负证据

# crash 会话1（最小加载验证）
timeout 590 crash /tmp/vmlinux-0102 <dump>/vmcore -i /tmp/case05_min.cmd     # sys/panic/bt/quit

# crash 会话2（全量取证）
timeout 590 crash /tmp/vmlinux-0102 <dump>/vmcore -i /tmp/case05_full.cmd
#   含: sys panic bt bt -t set mach ps|head rd ffffb378e29e55d0 64
#       rd 0xffffb378e29e5a40 16   rd 0xffffb378e29e5b60 16
#       p __per_cpu_offset[146|145|179|178|0]   px runqueues   px &__per_cpu_offset
#       rd ffff8000ae213960 32   rd ffff8000ae213a90 16
#       dis -l 0xffffb378e0bbae48   vtop 0xffffb378e29e5a60   vtop 0xffffb378e25e96c0
#       struct rq 0xffff604003e26c00   search -t 1931

# crash 会话3（第二验证）
timeout 590 crash /tmp/vmlinux-0102 <dump>/vmcore -i /tmp/case05_v2.cmd
#   含: vtop/rd rq(146)=0xffff80008137b6c0 与 rq(179)   rd 0xffff8000817bb6c0 40
#       p __per_cpu_offset[144|147|148]
#       rd ffffb378e29e55d{0..8} 4  （base+k 非对齐逐字节扫描）

# 静态反汇编与符号
objdump -d --start-address=0xffff80008013ae10 --stop-address=0xffff80008013ae60 /tmp/vmlinux-0102
objdump -d --start-address=0xffff80008013ad08 --stop-address=0xffff80008013ae10 /tmp/vmlinux-0102
nm /tmp/vmlinux-0102 | grep -E 'find_busiest_group|newidle_balance|__per_cpu_offset|runqueues|node_data|cpu_worker_pools'

# 代数复算（模 2^64，全部断言）
python3 algebra.py > algebra_out.txt
```

附件清单：`dmesg_forensics.txt`（dmesg+crash+反汇编全部取证输出）、`crash_forensics_full.txt`（会话2全文）、`crash_forensics_v2.txt`（会话3全文）、`algebra.py` / `algebra_out.txt`（闭合复算）。

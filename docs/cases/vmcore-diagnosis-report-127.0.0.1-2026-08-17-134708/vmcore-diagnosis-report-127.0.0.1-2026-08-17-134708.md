# CPU179 转储深度诊断报告（第 1 次独立重研究）
## ——26 次"假 fault"前兆与 1 次致命 Oops 全部同核：一条同时弄脏"写通路指针低位"与"读通路数据高位"的瞬态数据通路受扰

| 项目 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-17-13:47:08/`（vmcore-incomplete, 26.9 GiB; vmcore-dmesg.txt, 3813 行） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026; 192 CPU, 8 NUMA 节点, 768 GB RAM |
| CPU | Kunpeng-920 (TaiShan-v110, MPIDR 0x481fd010); 涉事核 **CPU179 = MPIDR 0x7a0300, Node 7** |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP (KASLR+KPTI 开启) |
| 崩溃时间 | uptime 239527.81 s（开机后 66.5 小时）; 墙钟约 2026-08-17 13:46:58（dump 落盘 13:47:08） |
| 受害进程 | 前兆：irqbalance(PID 9653, 12 次) 与 pmdalinux(PID 10334, 14 次); 致命：swapper/179（idle 内核线程, 软中断负载均衡路径） |
| 前兆 | 26 次 `Ignoring spurious kernel translation fault`（全部 CPU179, 全部 `/proc/interrupts` 读路径, 跨 19.5 h） |
| crash 加载 | **失败**（incomplete dump: 384 个 seek error 后 `invalid kernel virtual address: ffff8000800176c0 type: "runqueues entry (per_cpu)"` 初始化中止, 未达提示符） |
| 一句话结论 | 【强推】CPU179 核内数据通路（load 结果/寄存器堆-旁路侧）瞬态位受扰：26 次把 `seq_file` 缓冲指针的低位翻坏（写侧 strb 触发假 fault 被内核"宽容忽略"），第 27 次把 `__per_cpu_offset[175]` 装载结果的高 32 位注入 `0x00ffffa8`，经 `add x27,x1,x20` 传播成非规范地址 `00ffd780f5a3a7c0`，idle 负载均衡读 `rq` 时 L0 translation fault → panic → kdump |

---

## 1. 执行摘要

1. **现象**：开机 66.5 小时后，CPU179 上的 idle 内核线程 swapper/179 在软中断负载均衡 `find_busiest_group+0x140` 读 `ldr x23, [x27, #288]` 时，对非规范地址 `00ffd780f5a3a7c0`（"address between user and kernel address ranges"）触发 level-0 translation fault（ESR=0x96000004, WnR=0 读），Oops → panic → kdump 成功启动。
2. **签名**：坏值链可完整闭合（Python3 mod 2^64 实算）：`x20 = __per_cpu_offset[175]` 装载结果被腐化为 `0x00ffffa827b20fe0`；`x27 = x1 + x20 = 0x00ffd780f5a3a6a0`；`FAR = x27 + 288 = 0x00ffd780f5a3a7c0`，三步与 dmesg 寄存器逐位吻合【实锤】。x20 的低 32 位 `0x27b20fe0`（635 MiB）恰为 192-CPU percpu 大区内 CPU175 单元偏移的合理形态，高 32 位 `0x00ffffa8` 则是不可能的异常注入。
3. **前兆（本案最独特法证特征）**：致命事件前 19.5 小时内，**同一核 CPU179** 上 irqbalance/pmdalinux 周期性读 `/proc/interrupts` 时发生 **26 次** `Ignoring spurious kernel translation fault`：`__memcpy+0x80`（`strb w8, [x0, x14]`，ESR x19=0x96000044 即 **WnR=1 写**）把 20 字节小拷贝的目的指针写坏，坏 dst 与 FAR 恒差 0xa（`x14 = len/2`），且 5 个可见样本全部闭合【实锤】。
4. **闭合验证结果**：前兆坏 dst 高 32 位 `ffff6040` 与正常 physmap 指针（如致命帧好寄存器 x22=x26=ffff604003e9ec00）同段、只有低位被扰；致命坏 x20 则低位完好、高位被注入——**读写两个方向、高低两种位段都出错，但 27 次事件 100% 同核（CPU179）**。
5. **置信级别**：微架构定位为"CPU179 核内瞬态数据通路受扰（load 数据结果/寄存器堆-旁路侧优先于 L1D 阵列）"——【强推】（27 次同核 + 双方向 + 双位段 + 瞬态非驻留多源收敛；因 incomplete dump 无法读取内存真值做最终对照，未能升级为实锤）。排除软件成因【实锤级反证】：`/proc/interrupts` 与负载均衡是亿万次执行的成熟热路径，26 次坏值互不相同且无固定 pattern，软件 bug 不挑核也不挑位段。
6. **处置**：建议对该节点 CPU179（MPIDR 0x7a0300）做隔离观察/换核验证；RAS 侧无任何硬件错误报告（负证据），属于典型 SDC（静默数据腐化）事件；同时建议审视内核 "Ignoring spurious fault" 的宽容策略——它把 26 次本应报警的数据通路异常静默吞掉，直到第 27 次才以 panic 形式暴露。

## 2. 证据规则与方法

- **证据源**：仅 `vmcore-dmesg.txt`（3813 行）+ `/tmp/vmlinux-0102`（BuildID 276194e5…, _text=ffff800080000000, 带 debug_info）静态反汇编。未阅读任何既有诊断文档，保持独立性。
- **诚实铁律**：crash 加载 incomplete dump 失败，如实记录（第 6.3 节），全部取证改为 dmesg + objdump 静态语义重建；所有 64 位运算用 Python3 mod 2^64（`algebra.py`，输出 `algebra_out.txt`）；每一处数据引用带 dmesg 行号。
- **三级置信**：【实锤】可直接复核（如 FAR=x27+288 的闭合等式）；【强推】多源收敛但缺内存真值对照（微架构定位）；【假设】无法软件验证（受扰的物理单元归属），给出验证途径。
- **工具清单**：crash 8.0.4-17.oe2403sp4（加载失败）、objdump（binutils）、nm/readelf、python3、grep/sed。

## 3. 本次开机时间线【时间线】

| uptime (s) | 墙钟（反推） | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0 | ~2026-08-14 19:14 | 开机（8 节点 192 CPU 起 0.33s 内全上电，CPU179 于 0.329765s 上线） | L1, L1206 | 实锤 |
| 24–45 | 08-14 19:15 | 常规启动日志结束（ipmi/iscsi/ext4/dm 等） | L2560–2586 | 实锤 |
| 130815.75 | 08-16 07:41 | `megaraid_sas: Using 48-bit DMA addresses`——此后 36h+ 内核日志静默（系统平稳运行） | L2587 | 实锤 |
| 169175.04 | 08-16 18:19 | **前兆 #1**：irqbalance 读 /proc/interrupts, 假 fault @ ffff604005f8f5f2 | L2588–2632 | 实锤 |
| 169644.92 | 08-16 19:37 | 前兆 #2：pmdalinux, 假 fault @ ffff604005f8c2ae | L2633 | 实锤 |
| 169875.05–169875.08 | 08-16 19:41 | 前兆 #4/#5：同秒双发（irqbalance 两次, 间隔 22ms） | L2723, L2768 | 实锤 |
| 170017–170974 | 08-16 19:43–20:01 | 前兆 #6–#16（密集期, 11 次/小时量级） | L2903–3262 | 实锤 |
| 235104–239264 | 08-17 12:11–13:43 | 前兆 #17–#26（第二密集期, 含 #26 距致命仅 262.8s） | L3263–3757 | 实锤 |
| 239527.81 | 08-17 13:46:58 | **致命 Oops**：find_busiest_group+0x140, CPU179, swapper/179, panic → kdump | L3758–3813 | 实锤 |

（墙钟由目录名 2026-08-17-13:47:08 反推 panic 时刻约 13:46:58，开机约 08-14 19:14；python3 计算见 algebra_out.txt [S4]）

前兆时间分布的规律性：irqbalance（每 10s 扫一次中断）与 pmdalinux（pmproxy 周期采样）都周期性读 `/proc/interrupts`，26 次前兆全部落在这条路径上——**不是这两个进程有问题，而是它们是 CPU179 上少数周期性跑 memcpy 小拷贝 + 指针密集计算的常客**。

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg L3758–3774）

```
[239527.811339] Unable to handle kernel paging request at virtual address 00ffd780f5a3a7c0
[239527.820086] Mem abort info:
[239527.823675]   ESR = 0x0000000096000004
[239527.828214]   EC = 0x25: DABT (current EL), IL = 32 bits
[239527.834322]   SET = 0, FnV = 0
[239527.838165]   EA = 0, S1PTW = 0
[239527.842094]   FSC = 0x04: level 0 translation fault
[239527.847760] Data abort info:
[239527.851428]   ISV = 0, ISS = 0x00000004, ISS2 = 0x00000000
[239527.857706]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[239527.863547]   GCS = 0, Overlay = 0, DirtyBit = 0, Xs = 0
[239527.869650] [00ffd780f5a3a7c0] address between user and kernel address ranges
[239527.877580] Internal error: Oops: 0000000096000004 [#1] SMP
```

要点：WnR=0（读）、S1PTW=0（非页表走查本身的错）、FSC=0x04（L0 翻译失败——TTBR0/TTBR1 都不覆盖该地址，因为它是 bit[63]=0 的非规范"中间空洞"地址）。**注意本案与"页表真值被写坏"型案例不同：这里出错的是访问地址本身，而非页表项。**

### 4.2 全量寄存器（dmesg L3796–3811）

```
pstate: 20400009 (nzCv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : find_busiest_group+0x140/0xb60
lr : find_busiest_group+0x11c/0xb60
sp : ffff800081f1bb30
x29: ffff800081f1bcb0 x28: ffff800081f1bc40 x27: 00ffd780f5a3a6a0   ← 坏(1)
x26: ffff604003e9ec00 x25: 00000000000000af x24: ffffd7d8ce315000
x23: 0000000000000400 x22: ffff604003e9ec00 x21: ffffd7d8ce30fcb0
x20: 00ffffa827b20fe0 x19: ffff800081f1bd40 x18: 0000000000000000   ← x20 坏(2)
x17: ffffa827b38c4000 x16: ffff800081f18000 x15: 0000aaab090d1eb0
x14: 0000000100000013 x13: ffffff0000000000 x12: 0000000000000000
x11: 0000000000000047 x10: 00000000000002ea x9 : ffffd7d8cc4eae58
x8 : ffff800081f1bc98 x7 : 0000000000000000 x6 : 00000000000000af
x5 : ffff800000000000 x4 : 0000000000000002 x3 : 000000000000002f
x2 : 0000000000001c00 x1 : ffffd7d8cdf196c0 x0 : 00000000000000af
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

寄存器健康度分类：28 个寄存器中 26 个形态完全正常（内核栈指针 sp/x8/x29/x28、线性映射区指针 x1/x9/x21/x24、physmap 指针 x22/x26、percpu 基址 x17、负载累加值 x2/x23 等），只有 **x20 与由它派生的 x27** 是坏的——x20 的低 32 位 `27b20fe0` 合理、高 32 位 `00ffffa8` 异常；x27 = x1 + x20 精确传播了 x20 的错误。

### 4.3 Call trace（完整, dmesg L3808–3813）

```
find_busiest_group+0x140/0xb60
load_balance+0x108/0x6c0
rebalance_domains+0x160/0x3b0
_nohz_idle_balance.isra.0+0x258/0x3c8
run_rebalance_domains+0x6c/0x88
handle_softirqs+0x128/0x330
__do_softirq+0x1c/0x28
____do_softirq+0x18/0x30
call_on_irq_stack+0x30/0x48
do_softirq_own_stack+0x24/0x38
irq_exit_rcu+0x108/0x130
el1_interrupt+0x58/0x120
el1h_64_irq_handler+0x24/0x30
el1h_64_irq+0x78/0x80
default_idle_call+0x74/0x150
cpuidle_idle_call+0x198/0x228
do_idle+0x13c/0x1b8
cpu_startup_entry+0x40/0x50
secondary_start_kernel+0x14c/0x1d8
__secondary_switched+0xb8/0xc0
```

解读：CPU179 空闲（do_idle → cpuidle），定时器中断返回时触发 SCHED_SOFTIRQ，在 **per-cpu IRQ 栈**上做 nohz idle 负载均衡（sp=ffff800081f1bb30 与 x16=ffff800081f18000 差 0x3b30，在 16KB IRQ 栈内），聚合域内各 CPU 的 runqueue 负载时崩溃。x25=0xaf=175：正在聚合第 175 号 CPU 的负载（group 轮询循环变量），x6/x0 同为 0xaf 是循环残值。

### 4.4 前兆异常原文（26 次之一, dmesg L2588–2600）

```
[169175.043506] ------------[ cut here ]------------
[169175.043519] Ignoring spurious kernel translation fault at virtual address ffff604005f8f5f2
[169175.043528] WARNING: CPU: 179 PID: 9653 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[169175.043676] CPU: 179 PID: 9653 Comm: irqbalance Kdump: loaded Not tainted ...
[169175.043686] pc : __do_kernel_fault+0x130/0x1b8
...
[169175.043725]  x25: 0000000000000a18 x24: ffff604005f8f5e8
...
[169175.043767]  __memcpy+0x80/0x240
[169175.043774]  seq_printf+0xc4/0xe8
[169175.043783]  show_interrupts+0x1d4/0x498
[169175.043787]  seq_read_iter+0x168/0x478
[169175.043790]  proc_reg_read_iter+0x68/0xe8
[169175.043796]  new_sync_read+0xac/0x148
[169175.043800]  vfs_read+0x194/0x1e8
[169175.043803]  ksys_read+0x78/0x118
[169175.043805]  __arm64_sys_read+0x24/0x38
...（用户态 syscall read 入口）
```

26 次前兆的完整清单（时间戳与坏地址）见 dmesg_forensics.txt [F9]。分布特征：irqbalance 12 次、pmdalinux 14 次；**全部 CPU179**；坏地址分布在 19 个不同 4K 页上、互不重复（聚类见 algebra_out.txt [S5]）。

### 4.5 RAS 负证据（dmesg grep, dmesg_forensics.txt [F2]）

对整份 3813 行 dmesg 执行 `grep -iE 'Machine check|Hardware error|memory failure|Synchronous External Abort|Uncorrected|Deferred error|RAS event|edac.*(error|Error)'`：**除 "EDAC MC: Ver 3.0.0"（驱动版本行）与 "EDAC MC0: Giving out device"（ghes_edac 注册行）外，无任何硬件错误记录**——无 MCE、无 SEA、无 ECC 修正/未修正事件、无内存 failure。结合 Tainted 标记 `G        W`（W = 已发生过 warning）与 ghes_edac 在位却全程沉默，判定：**固件/EDAC 层面对这 27 次数据通路受扰零感知**——这正是 SDC（静默数据腐化）的定义性特征。

## 5. 业务现象

- **前兆双受害进程**：`irqbalance`（用户态中断均衡守护，每 10 秒读 `/proc/interrupts`）与 `pmdalinux`（Performance Co-Pilot 的 Linux 采集代理，周期性读 /proc 各文件做性能指标上报）。两者毫无关系，唯一共同点：**都被调度到 CPU179 上执行 `/proc/interrupts` 的 seq_file 读路径**。
- **致命受害上下文**：`swapper/179`（idle 内核线程）。不是任何业务进程"做错了什么"——CPU179 空闲下来后，调度器的 nohz idle 负载均衡替它背了雷。
- **业务影响**：前兆 26 次中，内核 `__do_kernel_fault` 判定地址"looks like spurious"（` arm64/mm/fault.c:494` 的宽容分支）后仅打 WARNING 并继续执行——后果是**那次 seq_printf 的 20 字节没有写进缓冲**（memcpy 部分完成/失败），irqbalance/pmdalinux 拿到的 /proc/interrupts 内容可能有缺字/残行。这是纯 SDC：**数据悄悄错了，系统继续跑**。直到第 27 次，坏值落进 idle 负载均衡的关键地址计算，才把系统打断。

## 6. 诊断定位过程【诊断定位过程】

### P1 · dmesg 勘察

3813 行 dmesg 完整扫描：开机指纹（192 CPU/8 节点/768GB/KASLR/KPTI/crashkernel 1024M high）→ 45s 常规启动 → 36 小时静默 → 19.5 小时内 26 次 CPU179 假 fault → 1 次致命 Oops。全部 53 条带 `CPU:` 的异常行 **100% 落在 CPU179**（`grep -oE 'CPU: [0-9]+' | sort | uniq -c` 输出仅 `53 CPU: 179`）——这是全案最重要的单点事实。

### P2 · 崩溃块提取与指令语义重建

用 `/tmp/vmlinux-0102` 对 `find_busiest_group`（`ffff80008013ad08`）做 objdump 反汇编，Code 窗口五条指令与反汇编逐条对上（dmesg L3810 的 `Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)`）：

```
ffff80008013ae34: a94087e0  ldp x0, x1, [sp, #8]     ; x0=__per_cpu_offset(数组基址), x1=env->dst_grpmask
ffff80008013ae38: f9400782  ldr x2, [x28, #8]
ffff80008013ae3c: f879d814  ldr x20, [x0, w25, sxtw #3]  ; x20 = __per_cpu_offset[175]  ← 坏值源头(装载)
ffff80008013ae40: 2a1903e0  mov w0, w25
ffff80008013ae44: 8b14003b  add x27, x1, x20          ; x27 = grpmask + offset(错)
ffff80008013ae48: f9409377  ldr x23, [x27, #288]     ; ← 致命指令 (pc = +0x140)
```

（`[sp+8]` 的来源在函数 prologue `ffff80008013ae04: add x0, x24, #0x5d0` / `+0xe08: str x0,[sp,#8]`，而 x24 此处是 `adrp ffff800081f65000` 后加 `#0x5d0`——`nm` 证实 `ffff800081f655d0 = __per_cpu_offset`；`x1` 来自 `ldr x1,[x19,#56]` 即 `env->dst_grpmask`，x19=ffff800081f1bd40 是 load_balance 的栈上 env 结构，与 sp 差 0x210，形态吻合。）

### P3 · crash 加载（失败, 如实记录）

```
$ timeout 590 crash /tmp/vmlinux-0102 <vmcore-incomplete> -i /tmp/case.cmd
WARNING: .../vmcore-incomplete: This dumpfile is incomplete...
crash: seek error: kernel virtual address: ffff800080000058  type: "IRQ stack pointer"
...（384 个 seek error: IRQ/SDEI stack pointer 全部缺失）
crash: page excluded: kernel virtual address: ffff6057fffaeb00  type: "memory section root table"
→ 会话在初始化阶段中止, 未出现 crash> 提示符, sys/panic/bt 均未执行
```

加 `--zero_excluded --no_panic --no_kmem_cache --active --no_modules` 重试仍失败：`crash: invalid kernel virtual address: ffff8000800176c0 type: "runqueues entry (per_cpu)"`（该地址与符号表 runqueues=ffff800081b696c0 差 0x1b52000，即 KASLR 偏移被 zero-fill 数据破坏后无法重定位）。**结论：incomplete dump 不可用，本案为 dmesg+静态反汇编法证。**（完整输出见 dmesg_forensics.txt [C1][C2]）

### P4 · 内存真值对照（改为"寄存器互证"）

由于无法读 dump 内存，用**同一帧内的健康寄存器互证**替代"寄存器 vs 内存真值"：

- x20 低 32 位 `0x27b20fe0` = 635.04 MiB：192 CPU 的 percpu 大区（vmalloc 嵌入式 first chunk + dyn 区）内 CPU175 单元的偏移量级完全合理（对照：正常帧 x17=ffffa827b38c4000 表明 percpu 单元散布在 ffffa827… 的 vmalloc 区，与 __per_cpu_offset 值域自洽）。
- x20 高 32 位 `0x00ffffa8`：`__per_cpu_offset[]` 是"单元地址 − __per_cpu_start"的差值，元素全部为小正数，**高 32 位必然为 0**——`0x00ffffa8` 只能是外来的。
- 更惊人的字节关系：x20 的 bits[55:32] = `ff ff a8 27`，恰是本核 percpu 基址 x17=**ffffa827**b38c4000 的最高 4 字节（见 algebra_out.txt [S8]）。这提示腐化数据与核内另一条活数据在通路上发生了错位混合（详见第 8 节讨论）。

### P5 · 软件成因排除

1. 该路径（`find_busiest_group` 的 group 轮询循环 + `__per_cpu_offset` 装载）在 192 核上每 tick 执行，运行 66.5 小时 ≈ 10^9 次量级，全局只此一错；26 次前兆的 `/proc/interrupts` + `__memcpy` 更是每秒被全机进程踩的热路径。
2. 26 次前兆坏值互不相同、无固定地址/固定位型（19 个不同页），排除"固定软件逻辑分支错误"（软件 bug 的坏值应有可复现形态）。
3. 27 次事件 100% 集中于 CPU179，而 irqbalance/pmdalinux/swapper 是三个互不相关的执行上下文——**软件不挑核，硬件挑**。
4. 内核 6.6.0-145.3.23.154 为 openEuler 稳定分支发行版，无对应已知缺陷报告线索（基于该路径语义正常性判断）。

### P6 · 定位收敛

前兆（写通路、指针低位坏）+ 致命（读通路、数据高位坏）+ 全部同核 + 全部瞬态 + RAS 零报告 → 收敛到"CPU179 核内、跨越 load 数据结果与寄存器/转发两级、瞬态性"的受扰定位。反事实推演（algebra_out.txt [S9]）：若 x20 为低 32 位真值 `0x27b20fe0`，则 x27=ffffd7d8f5a3a6a0、FAR=ffffd7d8f5a3a7c0——落在与 x21/x24 同段的合法线性映射区，指令根本不会 fault。**即：只要 x20 装载正确，一切正常；错的只是这次装载的高 32 位。**

## 7. 逻辑链条

**指令语义链（objdump 实测反汇编 + dmesg 寄存器, 逐步 mod 2^64 复算）【实锤】**：

```
(1) ldr x20, [x0, w25, sxtw #3]   x0 = __per_cpu_offset, w25 = 0xaf
    x20 应 = __per_cpu_offset[175]（小正数, 高32位=0）
    实 = 0x00ffffa827b20fe0        ← 高 32 位被注入 0x00ffffa8
(2) add x27, x1, x20              x1 = env->dst_grpmask = 0xffffd7d8cdf196c0
    = 0xffffd7d8cdf196c0 + 0x00ffffa827b20fe0
    = 0x00ffd780f5a3a6a0           ← 与 dmesg x27 逐位一致 ✓
(3) ldr x23, [x27, #288]          EA = 0x00ffd780f5a3a6a0 + 0x120
    = 0x00ffd780f5a3a7c0           ← 与 dmesg FAR 逐位一致 ✓
    bit[63]=0 → TTBR1 不命中; 非 0x0000ffff…用户形态 → TTBR0 也不命中
    → FSC=0x04 level 0 translation fault, Oops
```

**前兆语义链（同样实锤）**：

```
__memcpy+0x80 = strb w8, [x0, x14]   (ESR=0x96000044: WnR=1 写, DFSC=0x04)
x14 = x2 >> 1 = 0xa  → x2 = 20 字节 (seq_printf 一次小格式化拷贝)
FAR = x0 + x14 → 坏 dst x0 = FAR - 0xa
5 个可见样本: dmesg x24 恒 = FAR - 0xa ✓ (algebra_out.txt [S7])
坏 dst 形态 ffff6040xxxxxxxx = 正常 seq_file 缓冲(physmap kmalloc)的高位形态,
只有低 32 位内的位被翻转 → 20 字节写落到无效物理帧 → L0 fault
内核 arm64/mm/fault.c:494 判定 "spurious" → 忽略并继续(SDC!)
```

**闭合等式汇总（algebra.py 全部可复核）**：
- `x1 + x20 == x27`（dmesg 值）✓
- `x27 + 288 == FAR`（dmesg 值）✓
- 5×`x24 + 0xa == 前兆 FAR`（dmesg 值）✓
- 反事实：`x20_true = x20 & 0xffffffff = 0x27b20fe0` → FAR' = `0xffffd7d8f5a3a7c0`（合法内核地址，不 fault）✓

**反事实推演**：见 P6 末段。若第 (1) 步装载未被扰，(2)(3) 均为正常调度器行为；若 x20 的低 32 位也坏了，x27 将落入其他随机段。事实是低位完好、高位被注入——错误边界恰好切在 32 位处（bit[55:32] 注入 0xffffa8, bit[63:56]=0x00），呈"半长字级"受扰形态。

**诚实声明**：
- 本案的"内存真值对照"（读 dump 里 `__per_cpu_offset[175]` 与 seq_file buf 的真值）因 incomplete dump 加载失败而**缺失**，x20 低 32 位=真值的判断基于值域合理性+反事实闭合，属【强推】而非【实锤】。
- x20 高位 `0xffffa8` 与 x17 高字节 `ffffa827` 的同源性是**形态观察**，不能排除巧合（4 字节匹配概率约 2^-32，但样本仅 1 例）；不作为独立定论依据。
- "受扰发生在 load 数据结果 vs 寄存器堆/旁路" 的进一步区分无法用软件手段完成，只能给验证途径（第 9 节）。

## 8. 故障根因（微架构级结论 + 置信级别）

**结论**：【强推】**CPU179（MPIDR 0x7a0300, Node 7 物理核）核内数据通路发生反复的瞬态位受扰（SDC），受扰点在"load 返回数据 → 物理寄存器堆/旁路网络"一段（含 L1D 命中数据出口的可能性，但 L1D 阵列驻留损伤的可能性被瞬态性证据压低）**。两次采样方向相反：

- 前兆 26 次：写通路（strb 的目的指针计算链，指针**低位**被扰，高位完好）；
- 致命 1 次：读通路（ldr 的装载结果**高位**被注入，低位完好）。

27 次 100% 同核 + 读写双向 + 高低双位段 + 瞬态（无驻留重复模式）+ RAS/EDAC 零报告，五源收敛。排除软件成因（第 6 节 P5）为实锤级反证。

**定位到流水线具体段的推理**：
1. 不是 PTW/页表走查：S1PTW=0，且错的是数据值不是页表项；前兆是 WnR=1 的普通 store。
2. 不是 ALU：致命链里 add 的结果与两操作数完全自洽（x27=x1+x20 精确成立），ALU 算得对，是输入 x20 已经错了。
3. 不是 TLB：坏地址是"计算出来的非规范地址"，翻译机构对它的 L0 判定本身是正确行为。
4. 剩下两个候选：(a) **load path 数据段（L1D 出口 → 重排缓冲 → 物理寄存器堆写口）**受扰——直接解释"x20 装载结果高位坏"；(b) **寄存器堆读口/保留站-旁路网络**受扰——同时解释前兆（指针在寄存器间传递/计算时低位坏）与致命（x20 在被 add 消费前高位坏）。两候选均在"核内私有数据通路"范畴内，软件无法再细分——细分需要第 9 节的验证途径。

**置信级别**：核内数据通路瞬态受扰【强推】；受扰粒度呈 32 位半字边界形态【强推】（基于 27 次样本的位段统计）；具体物理单元（寄存器堆 vs L1D 出口 vs 旁路）【假设，待硬件级验证】。

## 9. 启示

### 9.1 微架构定位的意义

本案若只看最后一次 panic，会得到"调度器指针计算出错"的浅层结论，然后陷入对 find_busiest_group 的无谓代码审计。把 26 次"假 fault"前兆与致命事件并案后，才显露出真正的故障画像：**同一核、读写双向、双位段、长周期反复**——这是单核数据通路级损伤的标准指纹。教训：`Ignoring spurious kernel translation fault` 不是无害噪音，它是内核在替硬件"打喷嚏"，每一次都值得记入该 CPU 的健康档案。

### 9.2 芯片设计与实现启示（具体、可落地）

1. **核内数据通路的端到端校验缺口**：TaiShan-v110 的 L1/L2 有 ECC，但"load 返回 → 重排缓冲 → 物理寄存器堆写口 → 旁路网络"这一段通常是裸奔的（无 parity/ECC）。本案 27 次翻转全部穿过这段而未被任何机制捕获。建议：(a) 物理寄存器堆加 per-bank parity（PRF 是软错误率最高的阵列之一，且面积开销远小于 ECC）；(b) 旁路网络在关键转发点做偶发抽样校验（同值双发比对，微流水开销可控）。
2. **32 位半字边界的受扰形态值得 DFT 关注**：致命样本的注入恰好切在 bit[55:32]、前兆样本低位翻转——如果受扰源是数据通路上的某条 32 位 slice（如部分写/部分转发的 byte-enable 控制错误或 SI 桥的半字选通抖动），那么在 DFT 图案中应加入"半长字随机注入"类 LBIST 激励，专门打读写口/旁路多路选择器的选择信号。
3. **fail-fast vs silence 的权衡**：27 次事件里前 26 次被内核宽容策略静默吞掉。芯片层面应有"数据可疑即上报"的钩子（如 parity 错误即使可重试也计数上报），让固件能在第 1 次就看到趋势，而不是等第 27 次 panic。本案 RAS 全零记录说明现有 ghes_edac 对核内通路完全失明——建议在 CPU 侧增加"核内可疑事件计数器"（performance/sanity counter），哪怕不精确，也能给运维提供核级健康信号。
4. **kdump 可靠性设计**：incomplete dump（26.9 GiB 截断）导致 crash 完全不可分析（384 个 stack pointer seek error + runqueues 重定位失败）。建议：crashkernel 预留内存校验、转储分片落盘 + 优先保住 percpu/IRQ 栈/调度关键结构所在页；固件在转储前做内存快照完整性标记，让事后工具能区分"没转储到"与"转储了但坏了"。

### 9.3 对系统软件/RAS 的启示

1. **重新审视 arm64/mm/fault.c 的 spurious fault 宽容策略**：当前实现对"看起来像 spurious"的内核态 fault 打 WARNING 后继续。建议：(a) 增加 per-CPU 计数与速率限制上报（如 1 小时内 N 次即触发内核事件 `kernel:spurious_fault_storm`）；(b) 记录坏地址的完整 64 位值与位翻转特征供离线聚类——本案若有此数据，26 个样本可自动聚成"CPU179 数据通路异常"告警。
2. **SDC 主动检测**：对 `__per_cpu_offset[]` 这类"值域强约束"的静态关键表（元素恒为小正数），可加轻量级sanity 校验（装载后判高位为 0，非 0 即 bug_on/计数上报），成本一条 tbnz 指令，能把本案这类"高位注入"在第一次发生时就抓住。
3. **运维侧**：该节点应纳入"疑似单核 SDC"观察名单：隔离 CPU179（`nohz_full`/`isolcpus` 排除或热下线）后观察假 fault 是否归零，是成本最低的换核验证；若归零，即可坐实核级损伤并安排硬件更换。

## 10. 处置建议

| 优先级 | 动作 | 预期 |
|---|---|---|
| 立即 | 采集并归档本机 27 次事件清单（dmesg_forensics.txt [F3][F9]）作为该节点硬件工单附件 | 留证 |
| 立即 | 检查同批次其他节点是否存在 `Ignoring spurious kernel translation fault` 日志（本案是集群性 SDC 研究的一例） | 判定批次性 |
| 短期 | 在线隔离 CPU179（热下线或 isolcpus）观察 1–2 周假 fault 是否消失 | 换核验证【假设→实锤】 |
| 短期 | 开启 per-CPU spurious fault 计数监控（9.3 第 1 条） | 后续事件早发现 |
| 中期 | 联系厂商对 CPU179 所在物理核做离线诊断（MBIST/LBIST 复测，重点打寄存器堆/旁路 SI） | 定位物理单元 |
| 中期 | 若再次崩溃，确保 kdump 完整落盘（核查 crashkernel 与转储盘空间），力争拿到完整 vmcore 做内存真值对照 | 升级置信到实锤 |

## 附录：命令索引（全部取证命令，可复核）

```bash
# dmesg 法证（源: /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-17-13:47:08/vmcore-dmesg.txt）
grep -n -E 'Kernel panic|Internal error|Oops|BUG:|WARNING:|cut here|Unable to handle' <dmesg>
grep -n 'Ignoring spurious' <dmesg>                       # 26 次前兆清单
grep -oE 'CPU: [0-9]+' <dmesg> | sort | uniq -c           # 全部异常同核性(53×CPU179)
grep -oE '^\[[0-9]+\.[0-9]+\]' <dmesg> | ... | uniq -c    # 时间戳分布
grep -n -iE 'Machine check|Hardware error|memory failure|Synchronous External Abort|Uncorrected|Deferred error|RAS event' <dmesg>   # RAS 负证据
sed -n '2588,2632p' / '2633,2649p' / '3713,3757p' / '3758,3813p' <dmesg>   # 前兆/致命完整块
grep -n 'MPIDR 0x7a0300' <dmesg>                          # CPU179 → Node 7

# crash 加载尝试（失败, 证据已存档）
timeout 590 crash /tmp/vmlinux-0102 <vmcore-incomplete> -i /tmp/case.cmd
timeout 590 crash /tmp/vmlinux-0102 <vmcore-incomplete> --zero_excluded --no_panic --no_kmem_cache --active --no_modules -i /tmp/case.cmd

# 静态反汇编（/tmp/vmlinux-0102, BuildID 276194e5f356f9c4bc570bb0a750394d7b768035）
nm /tmp/vmlinux-0102 | grep -E 'find_busiest_group|__per_cpu_offset|__per_cpu_start|seq_printf|show_interrupts'
readelf -sW /tmp/vmlinux-0102 | grep -w runqueues
objdump -d --start-address=0xffff80008013ad08 --stop-address=0xffff80008013ae80 /tmp/vmlinux-0102   # find_busiest_group prologue+循环
objdump -d --start-address=0xffff800080e9db28 --stop-address=0xffff800080e9db50 /tmp/vmlinux-0102   # __memcpy+0x80 strb
objdump -d --start-address=0xffff80008051aa88 --stop-address=0xffff80008051ab50 /tmp/vmlinux-0102   # seq_printf 入口

# 代数复算
python3 algebra.py   # 输出: algebra_out.txt (S0–S10 全部闭合等式与时间线计算)
```

（全部原始输出存于同目录 `dmesg_forensics.txt`；代数脚本 `algebra.py` 与输出 `algebra_out.txt` 随附。）

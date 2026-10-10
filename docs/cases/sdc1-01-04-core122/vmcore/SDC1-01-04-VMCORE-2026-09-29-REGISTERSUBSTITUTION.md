# sdc1-01-04 vmcore 取证 ①：2026-09-29 CPU122 寄存器组替换 → psi_account_irqtime NULL 解引用

> **报告日期**: 2026-10-10
> **取证对象**: `/home/sdc/vmcore/127.0.0.1-2026-09-29-11:48:53/vmcore`（939,730,418 字节，kdump/makedumpfile `-l -d 31` 压缩转储）+ 同目录 `vmcore-dmesg.txt`（2,700 行）
> **崩溃时刻**: 2026-09-29 11:47:02 CST（uptime 4 天 23:40:17 = 430,817 s；转储目录时间戳 11:48:53 为 kdump 落盘时刻）
> **Panic**: `Unable to handle kernel NULL pointer dereference at virtual address 0000000000000018`
> **Panic 任务**: PID 1395189 `movbe_dump_prob`（= `movbe_dump_probe_c_sve`，TASK_COMM_16 截断），task_struct `0xffff08208bfa3f00`，**CPU 122**
> **工具链**: crash 8.0.4-17.oe2403sp4 + `/usr/lib/debug/lib/modules/6.6.0-159.4.13.167.oe2403sp4.aarch64/vmlinux`（kernel-debuginfo/debugsource 全套）
> **背景文档**: 同目录《SDC1-01-04-CPU122-BOOT-ISOLATION-CONFIG.md》（隔离与捕获体系）、《SDC1-01-04-CPU122-MICROARCH-DIAGNOSIS-REPORT.md》（9-17 微架构诊断，本案的先验证据链）
> **本文定位**: 对 9-29 vmcore 的指令级/寄存器级取证报告。核心结论：**CPU122（MPIDR 0x0900060200）在调度器 tick 的高速路径上发生了一次寄存器组替换爆发（x19 被替换为不可推导的外来数据、x21 的 bit15 被清除），经由被替换的 rq 指针从错误地址装载出 NULL，最终在 `psi_account_irqtime` 读 `task->thread_info.cpu` 时以 0x18 解引用崩溃**；此外在上一个 tick 窗口 CPU122 的时钟读取路径存在一次 3.1044 秒的读数异常。

---

## 0. 结论摘要（先给结论）

**这不是内核软件 bug，也不是内存（DDR）故障，而是坏核 CPU122 核内数据通路的一次"寄存器组替换"事件——与 9-17 微架构诊断报告确立的"有效数据错行替换"缺陷族（D1/D2）同族，且本次被捕获为迄今最精确的单事件定位：**

1. **故障指令**：`psi_account_irqtime+24: ldr w23, [x0, #24]`（psi.c:1265 `int cpu = task_cpu(task)`），x0 = 调用方 `update_rq_clock_task+304: ldr x0, [x21, #2576]` 装载的 `rq->curr`。
2. **x0 = 0（NULL）不是内存里的值**：dump 中 `rq122->curr = 0xffff08208bfa3f00`（合法，恰为 panic 任务本身）。运行中 CPU 的 `rq->curr` 永不为 NULL（至少是 idle task）——NULL 只能来自核内。
3. **x21（rq 指针）在崩溃时 = `0xffff082f7f1b76c0`，而真实 `per_cpu(runqueues,122) = 0xffff082f7f1bf6c0`，恰好差 0x8000（bit 15 被清除）**。三重独立验证真实 rq122：`->cpu == 122`、`->curr == panic 任务`、`->clock == 430816669564140`（= 崩溃时刻 sched_clock 减 4.004 ms，与 HZ=250 的 tick 周期吻合）。
4. **x21 在本函数内曾被证实有效**：+88 的 `rq->prev_irq_time` 存储与 +60 的读取经算术闭环证明落在真实 rq122 上（prev_new = prev_old + 3.104436 s 与 dump 值精确吻合）——即 **x21 的损坏发生在 +88（最后一次验证使用）与 +304（故障装载）之间，窗口只有 2 条算术/分支指令**。
5. **损坏后的装载链**：`ldr x0,[x21',#2576]` 读 `[0xffff082f7f1b80d0]`（dump 实测该地址内容 = `0x0000000000000000`）→ x0 = NULL → `ldr w23,[0+24]` → level-0 翻译故障 → Oops → panic → kdump。
6. **x19 同窗口被替换**：+92 的 `sub x19, x1, x20` 在 csel 选定 delta 后数学上必为 0，但崩溃寄存器 x19 = `0x186e499ba0eea`（≈80 天的 ns 值，而 delta 以上限 uptime≈4.3×10¹⁴ ns 为界）——任何合法算术都推不出该值。一次替换爆发命中两个寄存器。
7. **前置异常（上一个 tick）**：本次 tick 的 `delta = 3.1044 s`（单 tick 的 rq->clock 推进量，正常 4 ms）与 `cpu_irqtime[122].total − prev_irq_time = 3.1044 s`（单中断窗口的记账增量）同时出现。唯一能同时满足两者的重构：**上一个 tick 的中断窗口内，CPU122 至少两次连续的 sched_clock 读取返回了比真实时刻低 3.1044 s 的值（中断入口记账读、scheduler_tick 读），而中断出口读恢复正常**——一次时钟读取路径的"陈旧值交付"。"核心停顿 3.1 s"假说被证伪（停顿只能解释两者之一，详见 §4.2）。
8. **微架构归因**：寄存器堆读出/操作数交付路径的替换（x19 外来数据 + x21 单比特清除），叠加时钟读取路径的陈旧值交付——读侧/操作数侧故障，位于 CPU122 核内（私有 L1D/LSU/寄存器堆域），不在 DDR/LLC/互连（同簇邻居 rq121/rq123 全部正常，DDR 内联 ECC 对合法码字静默）。

---

## 1. 环境与实验上下文

### 1.1 启动与 CPU122 生命周期

| 项 | 值 | 证据 |
|---|---|---|
| 本次启动 | 2026-09-24 12:08:41（uptime 430,817 s 倒推） | crash `sys` UPTIME 4d23:40:17 |
| cmdline | `… console=tty0 maxcpus=122 panic=30` | dmesg 行 247 |
| 启动期 SMP | `smp: Brought up 4 nodes, 122 CPUs`（maxcpus=122 生效） | — |
| **CPU122 上线** | **启动后 34.75 s 由 udev 冷插拔上线**（顺序 122→123→126→127→124→125 乱序，为 udev 并行 worker 特征；当时 40-openEuler.rules 的 CPU 自动上线规则尚未注释，注释发生于 10-09 21:56） | dmesg 行 2555-2559：`CPU122: Booted secondary processor 0x0900060200` |
| MPIDR 核对 | 逻辑 122 ↔ 0x0900060200（坏核，128 核全在场映射） | 同上 + 行 2657 |
| 9-24 16:08 → 9-27 | 狩猎 v3（116×30 min 全核）跑满 67.6 h，CPU122 封顶 1.45 GHz | 背景文档 §7 |
| 崩溃会话 | 2026-09-29 11:13 `2026-09-29-movbe-c-122-plus-stressng`：`sdcshield -e movbe_dump_probe_c_sve -t 3600000 --cpuset=122`（SVE 字节交换往返测试：svtbl[3,2,1,0] + 向量存取校验）+ stress-ng `127×matrix` hogs（PID 1393856） | 会话目录 `movbe_dump_probe_c_sve.console`/`stressng.log` |
| 探测自身检出 | **62/62 迭代全部 pass、零检出**；迭代 62 于 11:46:32 开跑，崩溃时进行至第 30 s（正常迭代时长 32.3 s） | `movbe_dump_probe_c_sve.yaml` 尾部 |
| 频率 | 会话协议封顶 1450000 kHz（同日 23:38 solo30min 会话 STATUS 实测 `curfreq=1450000kHz` 佐证协议在位） | 狩猎/实验脚本预检 |

要点：崩溃时 CPU122 已连续在线约 4.98 天（自 9-24 12:09 起），坏核上运行的正是绑定它的 SVE 字节交换探测，其余 127 核满载 stress-ng matrix。**探测程序自身零检出——损坏完全发生在内核态**（调度器 tick 路径），用户态探测对此不可见。

### 1.2 系统快照（crash sys）

```
KERNEL: 6.6.0-159.4.13.167.oe2403sp4.aarch64  DUMPFILE: [PARTIAL DUMP]
CPUS: 128  DATE: Tue Sep 29 11:47:02 CST 2026  UPTIME: 4 days, 23:40:17
LOAD AVERAGE: 128.04, 127.98, 114.58   TASKS: 1699
PANIC: "Unable to handle kernel NULL pointer dereference at virtual address 0000000000000018"
PID: 1395189  COMMAND: "movbe_dump_prob"  TASK: ffff08208bfa3f00  CPU: 122  STATE: TASK_RUNNING (PANIC)
```

---

## 2. 崩溃签名（dmesg 位级解码）

### 2.1 异常与 ESR/FAR

```
[430816.673568] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000018
[430816.683191] Mem abort info:
  ESR = 0x0000000096000004
  EC = 0x25: DABT (current EL), IL = 32 bits
  SET = 0, FnV = 0, EA = 0, S1PTW = 0
  FSC = 0x04: level 0 translation fault
  Data abort info: ISV = 0, ISS = 0x00000004, ISS2 = 0x00000000
  CM = 0, WnR = 0, TnD = 0, …        ← WnR=0：读故障
[430816.747494] Internal error: Oops: 0000000096000004 [#1] SMP
```

ESR 位级：EC=0x25（当前 EL 数据中止）、IL=1（32 位指令）、ISS=0x4 → **WnR=0（读）**、FSC=0x04（**level 0** 翻译故障——地址 0x18 落在最低级页表都未覆盖的空洞）。故障性质：**对地址 0x18 的一次 32 位读**。

### 2.2 上下文与寄存器组（dmesg 权威值）

```
CPU: 122  PID: 1395189  Comm: movbe_dump_prob  Kdump: loaded  Not tainted
pstate: 814000c9 (Nzcv daIF +PAN … BTYPE=--)      ← EL0t：异常打断自用户态
pc : psi_account_irqtime+0x18/0x150
lr : update_rq_clock_task+0x140/0x228
sp : ffff8000803d3cb0                               ← CPU122 的 IRQ 栈
x0 : 0000000000000000   x1 : 00000000b91078c4
x20: 000000eeb91078c4   x19: 000186e499ba0eea
x21: ffff082f7f1b76c0   x22: ffffcc3742fd55d0
x9 : ffffcc3741174498   x2 : ffffcc3742bc5640
x17: ffff3bf83c5e6000   x5 : 01ffffffffffffff
x23: 00000000b91078c4
Code: d503233f a9bb7bfd 910003fd a90363f7 (b9401817)
Call trace:
 psi_account_irqtime+0x18 ← update_rq_clock_task+0x140 ← update_rq_clock+0x54
 ← scheduler_tick ← update_process_times ← tick_sched_handle ← … ← el0_interrupt
 ← el0t_64_xint_handler ← el0t_64_irq   ← 定时器中断自 EL0 打入
```

路径解读：探测在 CPU122 用户态运行 → 定时器中断 →（IRQ 栈）`scheduler_tick → update_rq_clock → update_rq_clock_task → psi_account_irqtime` → 崩溃。这是**调度器每 tick 必经的最热数据通路**。

寄存器初判（后续逐个证实/证伪）：
- `x21=0xffff082f7f1b76c0`：疑似 percpu 指针（0xffff082f… 为 percpu 区）——**§4 证明它是被单比特清除的 rq122 指针**；
- `x22=0xffffcc3742fd55d0`、`x2=0xffffcc3742bc5640`、`x9=0xffffcc3741174498`、`x17=0xffff3bf83c5e6000`：经反汇编对照全部为**合法工作值**（=&__per_cpu_offset[0]、=cpu_irqtime 链接地址、=返回地址、=__per_cpu_offset[122] 本身）；
- `x1=x23=0xb91078c4`、`x20=0xee_b91078c4`：irq_delta 的钳制值/原始值（§5.2 重构）；
- `x19=0x186e499ba0eea`：**不可推导值**（§4.3 证明）。

---

## 3. 指令级解剖（crash dis + 内核源码）

### 3.1 故障指令与实参来源

`dis -l psi_account_irqtime`（节选，符号 `0xffffcc37411d5bd8`）：

```
psi_account_irqtime+8:   paciasp                        ← Code: 行首 d503233f
psi_account_irqtime+12:  stp  x29, x30, [sp, #-80]!
psi_account_irqtime+16:  mov  x29, sp
psi_account_irqtime+20:  stp  x23, x24, [sp, #48]
psi_account_irqtime+24:  ldr  w23, [x0, #24]            ← ★故障指令（Code: 括号内 b9401817）
                                                           源码 psi.c:1265  int cpu = task_cpu(task);
```

`ldr w23,[x0,#24]` 读 task+0x24 = `thread_info.cpu`（arm64 6.6 布局），x0 = **该函数第一实参 task**。

调用方 `update_rq_clock_task`（core.c:730-787）反汇编关键段：

```
+60:  ldr  x0, [x0, #3248]        ; x0 = rq->prev_irq_time          (core.c:739)
+64:  ldr  x3, [x22, x3, lsl #3]  ; x3 = __per_cpu_offset[rq->cpu]  (sched.h:3343)
+68:  ldr  x20, [x3, x2]          ; x20 = cpu_irqtime[cpu].total    (irq_time_read)
+72:  sub  x20, x20, x0           ; raw = total − prev
+76:  cmp  x20, x1
+80:  csel x20, x20, x1, le       ; clamped = min(raw, delta)       (core.c:751-757)
+84:  add  x0, x20, x0
+88:  str  x0, [x21, #3248]       ; rq->prev_irq_time += clamped    (core.c:759)
+92:  sub  x19, x1, x20           ; x19 = delta − clamped           (core.c:761)
+96:  cbnz x20, +304              ; if (irq_delta) …
…
+304: ldr  x0, [x21, #2576]       ; ★x0 = rq->curr                   (core.c:762)
+308: mov  w23, w20
+312: mov  w1, w20                ; 第二实参 irq_delta
+316: bl   psi_account_irqtime
```

即：**x0(NULL) 的直接来源是 +304 经 x21 基址的装载**。判别问题由此变为：x21 是什么、装载目标内存里是什么。

### 3.2 真实 rq122 的三重验证

```
crash> p/x __per_cpu_offset[122]                        → 0xffff3bf83c5e6000
crash> p/x (struct rq *)(&runqueues + __per_cpu_offset[122])
                                                        → 0xffff082f7f1bf6c0     ← 真实 rq122
  ->cpu     = 122                                       ✓
  ->curr    = 0xffff08208bfa3f00                        ✓ = panic 任务（合法非 NULL）
  ->clock   = 430816669564140                           ✓ = 崩溃时刻 430816.673568 s − 4.004 ms（HZ=250，tick=4 ms）
  ->clock_task = 430325719825470；clock − clock_task = 490949738670 = prev_irq_time ✓（设计自洽）
  ->nr_running = 1
```

**内存里的 rq->curr 合法且非空**——NULL 不在内存中。同时 percpu 单元间距实测 0x24000（rq120=…7f1776c0、rq121=…7f19b6c0、rq123=…7f1e36c0），因此 x21 的值不等于任何其他 CPU 的 rq。

### 3.3 x21 的损坏判定（两指令窗口）

- 崩溃时 `x21 = 0xffff082f7f1b76c0` = 真实 rq122 **− 0x8000**：`0xf6c0 → 0x76c0`，**bit 15 单比特清除**。
- x21 在 **+88 仍然有效**的证明（算术闭环）：dump 中 `rq122->prev_irq_time = 490,949,738,670`；由 +84/+88 反推 `prev_old = prev_new − clamped`，而 `clamped = w20 = 0xb91078c4 = 3,104,435,908`（见 §4.1 由 csel 语义唯一确定），得 `prev_old = 487,845,302,762`——与 `cpu_irqtime[122].total = 490,949,742,080` 相差 3,104,439,318（即 raw），构成 `raw > delta → csel 选 delta` 的自洽闭环。**若 +88 的存储落在被损坏的 x21' 上，真实 rq122 的 prev_irq_time 不可能呈现本次 tick 更新后的值**——故 +88 时 x21 尚且正确。
- +88 与 +304 之间只有 +92（sub）、+96（cbnz）两条指令，均不写 x21。
- **结论：x21 的 bit 15 在这两条指令的窗口内（或在 +304 装载的操作数读出瞬间）被清除。**

### 3.4 NULL 的出处验证

```
crash> rd -64 0xffff082f7f1b80d0      ← x21' + 0xA10（curr 偏移）
ffff082f7f1b80d0:  0000000000000000   ← 实测为 0
```

被损坏基址指向的地址**恰含 0**——`ldr x0,[x21',#2576]` 是一次"成功"的装载，从错误地址合法地读出了 NULL。链路闭合：

```
x21 bit15 清除 → ldr x0,[rq−0x8000+0xA10] = 0 → psi_account_irqtime(NULL, delta)
  → +24: ldr w23,[0+0x18] → level-0 翻译故障 → Oops → panic → kdump
```

### 3.5 x19 的替换证明

+92 后 x19 数学上恒为 `delta − clamped`。§4.1 证明本次 csel 选中 x1（delta），故 **x19 必为 0**。实测 x19 = `0x186e499ba0eea` ≈ 6.913×10¹⁵ ns ≈ 80.0 天——而 delta 作为 `sched_clock_cpu − rq->clock_old` 的差值以上限 uptime（≈4.308×10¹⁴ ns）为界，**任何合法算术都不可能产出该值**。+92 之后至故障之间无任何指令写 x19（反汇编逐条核对）。⇒ **x19 被外来数据替换**（该值在可用搜索范围内不存在副本；注：本转储为 PARTIAL DUMP，`search -k` 对其不工作（以已知存在值实测为零命中），用户页亦不在转储内，故替换源无法进一步定位）。

---

## 4. 前置异常：上一个 tick 的 3.1044 秒时钟读数异常

### 4.1 两个"3.1 秒"同时出现

| 量 | 值 | 含义 | 正常值 |
|---|---|---|---|
| delta（本次 tick 的 rq->clock 推进量） | `0xb91078c4` = 3,104,435,908 ns = 3.104436 s | `sched_clock_cpu(122) − rq->clock_old` | 4 ms |
| raw = total − prev_old | 3,104,439,318 ns = 3.104439 s | 上次 tick 以来 cpu_irqtime[122].total 的增量 | μs~ms 级 |
| 邻居参照 | rq121/rq123 的 prev_irq_time ≈ 各自 total（差 1.5 μs）；两核 total 分别为 382.2 s / 388.2 s（≈uptime 的 0.09%，正常） | — | — |

delta 的确定：`clamped = w20 = 0xb91078c4`；若 csel 选中 raw 则 w20 应为 raw 低 32 位 0xB9108656（≠实测），故 **csel 必选中 delta ⇒ delta = 3.104435908 s**，且 raw（3.104439318 s）> delta 与钳制条件自洽。

### 4.2 唯一自洽的重构：时钟读数"低 3.1 秒"脉冲

对上一个 tick 的中断窗口（irq_enter 记账 → scheduler_tick → irq_exit 记账）：

- **rq->clock 滞后 3.1044 s**：本次 tick 的 delta = 真实当前时刻 − 上次存储值。上次存储值 = 上次 tick 的 sched_clock 读数。该读数比当时真实时刻**低 3.1044 s**。
- **total 增加 3.1044 s**：irq_exit 记账增量 = 出口读数 − 入口读数 = 3.1044 s。结合上一条（入口读/中间读低、出口读正常）⇒ **中断入口与 scheduler_tick 的至少两次连续读数低了 3.1044 s，到中断出口已恢复**。
- 备择"CPU122 在中断内停顿 3.1 s"假说**被证伪**：若停顿发生在 scheduler_tick 之前，clock 不滞后（矛盾一）；若在 scheduler_tick 之后（但出口记账前），则 total 只增加 μs 级（矛盾二）；若停顿在 tick 之间（EL0），tick 迟到但 clock 不滞后（矛盾三）。**只有"读数脉冲性偏低"能同时满足两个 3.1 s。**
- 佐证：arch timer 频率 100 MHz（9-30 vmcore 溢出栈实测 0x5f5e100），3.104435908 s × 10⁸ = 310,443,590.8 tick——非整数说明值经过了 mult/shift 换算，与 sched_clock 读数路径一致。

**解读**：sched_clock_cpu 读出比真实值旧 3.1044 s 的值——一次"陈旧行/陈旧计数器值交付"，与 10-09 案 freeptr 槽读到陈旧文本属同一读侧错行形态（见跨案分析）。

---

## 5. 备择假设与排除

| # | 假设 | 排除依据 |
|---|---|---|
| 1 | PSI/调度器软件 bug（curr 为 NULL） | 运行中 CPU 的 rq->curr 恒非 NULL（至少为 idle task）；dump 实测 curr = panic 任务，合法非空 |
| 2 | rq->curr 被 DDR 位翻转清零 | 内存实测合法；且 DDR 内联 ECC 会纠正/计数单比特错——与全程 RAS 静默矛盾 |
| 3 | x21 合法地取了别的值（其他 CPU 的 rq / 编译器约定） | 反汇编证明 +48 `mov x21,x0` 后无写入；真实 rq122 三重验证；0x8000 偏差不等于任何 CPU 的 rq（单元间距 0x24000）；+88 存储落点证明当时 x21 正确 |
| 4 | x19 为合法残留 | +92 数学恒为 0（csel 语义唯一）；值超 uptime 上界两个数量级 |
| 5 | 崩溃转储伪影 | dmesg（实时 printk）与 vmcore 寄存器一致；溢出栈/IRQ 栈均未涉此案 |
| 6 | 3.1 s 为核心停顿 | §4.2 三重矛盾排除；仅读数脉冲自洽 |
| 7 | crash 工具误差 | 所有关键值经 `rd` 原始内存二次核对（如 [0xffff082f7f1b80d0]=0、rq122 各字段） |

**已知局限**：① `search -k` 在本 PARTIAL DUMP 上失效（实测对已知存在值零命中），x19 替换值无法溯源；用户页不在转储中。② x21 的损坏无法进一步区分"寄存器堆读出位错"与"+304 装载的地址生成位错"——两者均为核内事件，不影响归因。

---

## 6. 微架构归因与置信度

**归因**：CPU122（MPIDR 0x0900060200，TaiShan v120）核内数据通路的**读侧/操作数侧替换**：
- 一次**寄存器组替换爆发**（x19 ← 外来数据；x21 ← bit15 清除），窗口仅 2 条指令，位于调度器最热路径；
- 叠加上一 tick 的**时钟读取陈旧值交付**（3.1044 s）；
- 形态学对照 9-17 报告 §5/§10.7.5：读侧"把别的行的合法内容送进寄存器组"+单比特位错，属 **D1/D2 错行替换族**。

按 9-17 报告 §10.9.3 七步法：

| 维度 | 评估 |
|---|---|
| P1 单核浓度 | 全部可归因事件（x19、x21、时钟读数）均在 CPU122；同簇/邻核 rq121、rq123、cpu_irqtime[121/123] 全部正常 |
| P2 跨会话复发 | 9-17 三次启动 + boot panic ×2 + 本案 + 9-30/10-09 两案（见跨案分析）——坏核在线即复发 |
| P3 RAS 静默 | 本 boot 无任何 ECC/RAS 事件；替换值皆为"合法码字"（0 或合法数据），SECDED 无感 |
| P5 异常加权 | 嵌套故障（NULL 读在 PSI 热路径）、SP/指针类通路（rq->curr 属调度器最热依赖链）、单比特位错（bit15） |

**置信度：高。** 建议处置与既有结论一致：坏核保持启动隔离（maxcpus=122），实验窗口按既有协议（封顶 + MPIDR 核对 + 死亡容忍）。

---

## 7. 附：关键命令与证据索引

| 证据 | 命令/来源 |
|---|---|
| 崩溃签名/寄存器 | `vmcore-dmesg.txt` 行 2641-2700 |
| 故障指令反汇编 | `crash> dis -l psi_account_irqtime`（+24）、`dis -l update_rq_clock_task`（+304 等） |
| 真实 rq122 | `p/x (struct rq *)(&runqueues + __per_cpu_offset[122])` 及字段 |
| NULL 出处 | `rd -64 0xffff082f7f1b80d0` → 0x0 |
| irqtime 三核对照 | `p *(struct irqtime *)(&cpu_irqtime + __per_cpu_offset[12[123]])` |
| EL0 帧完好 | `p *(struct pt_regs *)0xffff8000a578beb0`（user pc 0xaaaadb71a58c / sp 0xffff8bf6a460 / pstate 0x80001000） |
| 探测台账 | `sdc_hunt_logs/2026-09-29-movbe-c-122-plus-stressng/{movbe_dump_probe_c_sve.yaml,console,stressng.log}` |
| 内核源码 | `/usr/src/debug/kernel-6.6.0-159.4.13.167…/kernel/sched/{psi.c,core.c,cputime.c,sched.h}` |

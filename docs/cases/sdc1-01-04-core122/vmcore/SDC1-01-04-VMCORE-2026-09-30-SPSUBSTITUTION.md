# sdc1-01-04 vmcore 取证 ②：2026-09-30 CPU122 SP 寄存器替换 → kernel stack overflow

> **报告日期**: 2026-10-10
> **取证对象**: `/home/sdc/vmcore/127.0.0.1-2026-09-30-08:32:11/vmcore`（886,976,253 字节）+ 同目录 `vmcore-dmesg.txt`（2,681 行）
> **崩溃时刻**: 2026-09-30 08:30:20 CST（uptime 11:04:27 = 39,868 s；转储目录 08:32:11）
> **Panic**: `Kernel panic - not syncing: kernel stack overflow`
> **Panic 任务**: PID 22676 `movbe_dump_prob`（`movbe_dump_probe_c_sve`），task_struct `0xffff082099dc6900`，**CPU 122**
> **本文定位**: 对 9-30 vmcore 的指令级/栈级取证报告。核心结论：**CPU122 在定时器中断返回路径（el0_interrupt → exit_to_user_mode，实际处于 irqtime 记账/sched_clock 调用链邻域）中 SP 寄存器被替换为一个指向已释放 vmalloc 空洞的陈旧栈状地址（0xffff8000b03900b0，任务栈下方 0x7F50），异常向量体的首条 `stp x0,x1,[sp]`（openEuler FAST 补丁前置保存）在坏 SP 上写入触发写故障，经 VMAP_STACK 坏栈检测级联成 "kernel stack overflow" panic**。任务栈本体完好无损——SP 不是从任何完好栈槽恢复出来的。

---

## 0. 结论摘要

1. **SP 被替换而非栈被溢出**：任务栈 `[0xffff8000b0398000..0xffff8000b039c000]` 的帧链（be60→be90→…）与 EL0 pt_regs（栈顶 `0xffff8000b039beb0`，user pc/sp/pstate 全部正常）**完好**；而异常时 SP = `0xffff8000b03900b0` = 栈基址下方 0x7F50（未映射）。SP 的值不可能由完好栈上的任何 epilogue 装载得出（直接转储栈区间无此值）——**SP 在寄存器级被替换**。
2. **替换值是一个"陈旧的栈指针"**：`0xb03900b0 = (任务栈基址 − 0x8000) + 0xB0`，指向一个 **PTE=0 的已释放 vmalloc 空洞**（`vtop` 实测）——值"长得像"某个已消亡任务栈的 SP。几何形态与 9-17 Boot 1（`(栈基址−0x8000)+0xA0`，SP 跌破 0x7F60）跨会话复现，**两案均呈 (base−0x8000)+小偏移 模式**。
3. **引爆指令被反汇编钉死**：本内核（openEuler 6.6，CONFIG_ACTLR_XCALL_XINT/FAST_IRQ 补丁）的 `el1h_64_sync` 向量体**首指令即 `stp x0, x1, [sp]`**（`0xffffd63a3e421a90`，与溢出栈 pt_regs 的 pc 槽位值逐字节一致）——在坏 SP 上的第一次存储即触发 `ESR 0x96000047`（WnR=1、FSC=level-3）写故障，FAR = SP。
4. **SP 本可通过位检测的" unlucky"取值**：arm64 6.6 入口检查测试 (SP−0x150) 的 bit 14；0xb03900b0−0x150 = 0xb038ff60 的 bit 14 = 1 → 直接进 __bad_stack 分支，未在坏 SP 上继续保存——机制级联完整闭合（§3）。
5. **SPSR 附加异常**：溢出栈 pt_regs 的 pstate = `0x414003c9`（EL0t），而自 EL1 打入的异常其 SPSR 必为 EL1h（`0x…3c5`）——**模式域 bit 2 单比特清除**（与 9-29 x21 的 bit15、10-09 的 bit15/bit61 同族位错；保留定制入口代码 SPSR 处理未完全审计的保留意见）。
6. **被中断上下文与 9-29 前置异常同路径**：LR = `el0_interrupt+0x60`（= `exit_to_user_mode(regs)` 调用点，entry-common.c:939）；任务栈残留帧链为 `irqtime_account_irq → sched_clock_cpu → sched_clock`（irq_exit 记账）——与 9-29 案的 3.1 秒时钟读数异常处于**同一条时钟/记账路径**。
7. **探测自身再次零检出**：35/35 迭代 pass，崩溃时迭代 34 进行至第 17 s——损坏纯内核态。

---

## 1. 环境与实验上下文

| 项 | 值 | 证据 |
|---|---|---|
| 本次启动 | 2026-09-29 21:27:31（uptime 39,868 s 倒推） | crash `sys` |
| **CPU122 三段生命周期** | ① 36.37 s：udev 冷插拔上线（126→125→123→**122**→124→127 乱序，规则未注释期）② 886.7 s：`psci: CPU122 killed`（人工/脚本安全处置）③ 3472.6 s（9-29 22:25）：再次上线进入实验 | dmesg 行 2609-2639 |
| 崩溃会话 | `2026-09-30-movbe-c-122-plus-vecfp1h`：STATUS `START movbe_dump_probe_c_sve cpu122 cap=1450000kHz + 127xvecfp cpuset=0-121,123-127 1h`（stress-ng 127×vecfp）08:11:25 启动 | STATUS.txt / stressng.log |
| 崩溃时刻 | 08:30:20，会话第 19 分钟；CPU122 自再次上线起在线 ~10.1 h | crash `sys` + 时间线 |
| 频率 | 封顶 1450000 kHz（STATUS 明记） | STATUS.txt |
| 探测台账 | 35 迭代全 pass；迭代 34 于 08:30:03 开跑（正常 32.3 s），崩溃时第 17 s | `movbe_dump_probe_c_sve.yaml` |

---

## 2. 崩溃签名（dmesg 位级解码）

```
[39867.921213] Insufficient stack space to handle exception!
[39867.921220] ESR: 0x0000000096000047 -- DABT (current EL)     ← WnR=1 写、FSC=0x07 level-3 翻译故障
[39867.921224] FAR: 0xffff8000b03900b0                           ← = SP 本身
[39867.921225] Task stack:     [0xffff8000b0398000..0xffff8000b039c000]
[39867.921227] IRQ stack:      [0xffff8000803d0000..0xffff8000803d4000]
[39867.921229] Overflow stack: [0xffff082f7f1a82e0..0xffff082f7f1a92e0]
CPU: 122  PID: 22676  Comm: movbe_dump_prob
pstate: 414003c9 (nZcv DAIF +PAN … BTYPE=--)                     ← EL0t（异常，见 §4.3）
pc : el1h_64_sync+0x0/0x80     lr : el0_interrupt+0x60/0x210
sp : ffff8000b03900b0
x29: ffff8000b039be60   x28: ffff082099dc6900（task）  x19: ffff8000b039beb0（&EL0 pt_regs）
x20: ffffd63a3e4200e8（gic_handle_irq）  x22: ffffd63a3e428098（default_handle_nmi_irq）
x9 : ffffd63a3f2f8090（el0_interrupt+96）  x5 : 01ffffffffffffff（sched_clock 掩码）
x6 : 0000039b0c954e65   x3 : 00002442790b531c   x4 : 0000000000000015   x8/x7 = 0
Call trace: … panic_bad_stack → handle_bad_stack → __bad_stack → el1h_64_sync → el0t_64_irq（链）
```

ESR 解读：对 `0xffff8000b03900b0` 的**写**访问在 level-3（PTE）翻译失败——该地址未映射。打印序列出自 `panic_bad_stack()`（traps.c:929-956：读 ESR_EL1/FAR_EL1 → __show_regs → nmi_panic）。

---

## 3. 机制级联的完整重构（入口汇编级）

### 3.1 本内核（openEuler 6.6 定制）的向量前置代码

`crash> dis 0xffffd63a3e421a90 8`：

```
0xffffd63a3e421a90 <el1h_64_sync>:  stp  x0, x1, [sp]          ← ★向量体首指令：写 [SP]！
0xffffd63a3e421a94 <el1h_64_sync+4>:  stp  x2, x3, [sp, #16]
0xffffd63a3e421a98 <el1h_64_sync+8>:  stp  x4, x5, [sp, #32]
…（FAST 补丁的 x0-x15 前置保存，向上偏移写入）
```

与上游 `kernel_entry`（先 `sub sp,sp,#PT_REGS_SIZE` 再向下保存）不同，本内核向量体**以 [sp]、[sp,#16]… 为目标立即保存低寄存器**——SP 一旦非法，首条 STP 即写故障。

### 3.2 VMAP_STACK 坏栈检测路径（entry.S 逐行核对）

```
kernel_ventry（el=1）:
  sub  sp, sp, #PT_REGS_SIZE        ; sp' = SPint − 0x150
  …swap-dance 不破坏寄存器地算出 x0 = SPint − 0x150…
  tbnz x0, #THREAD_SHIFT(=14), 0f   ; (SP−0x150) 的 bit14 置位 ⇒ 判定坏栈
0:
  msr  tpidr_el0, x0                ; 暂存"原始 SP − PT_REGS_SIZE"
  …切到 overflow 栈…
__bad_stack:
  mrs  x0, tpidrro_el0              ; 恢复原始 x0
  sub  sp, sp, #PT_REGS_SIZE        ; 在溢出栈上建帧
  kernel_entry 1                    ; 保存"事件时刻的完整寄存器组"
  mrs  x0, tpidr_el0; add x0, x0, #PT_REGS_SIZE
  str  x0, [sp, #S_SP]              ; pt_regs->sp = 原始 SP（+0x150 修正是算术自洽的）
  mov  x0, sp; bl  handle_bad_stack → panic_bad_stack
```

本案数值代入：SP = 0xb03900b0 → (SP−0x150) = 0xb038ff60，bit 14（0x4000）=1 → 直接走坏栈分支 → 溢出栈 pt_regs 落在 `0xffff082f7f1a9190`。

### 3.3 溢出栈 pt_regs 的原始内存逐槽核对（对 dmesg 的验证）

```
crash> rd 0xffff082f7f1a9190 48   （节选，pt_regs 基址 0x…9190）
ffff082f7f1a9190:  ffff8000b039beb0  ffffd63a3e4200e8   ← regs[0]=x0=&EL0帧  regs[1]=x1=gic_handle_irq ✓
ffff082f7f1a91a0:  0000000000000000  00002442790b531c   ← x2=0              x3 ✓
ffff082f7f1a91b0:  0000000000000015  01ffffffffffffff   ← x4=0x15           x5=时钟掩码 ✓
ffff082f7f1a91c0:  0000039b0c954e65  0000000000000000   ← x6 ✓              x7=0
ffff082f7f1a91d8:  ffffd63a3f2f8090                      ← x9=el0_interrupt+96 ✓
ffff082f7f1a9228:  ffff8000b039beb0                      ← x19=&EL0帧 ✓
ffff082f7f1a9238:  0000000000000400                      ← x21=0x400 ✓
ffff082f7f1a9240:  ffffd63a3e428098  0000000080001000   ← x22=default_handle_nmi_irq ✓ x23 ✓
ffff082f7f1a9270:  ffff082099dc6900  ffff8000b039be60   ← x28=task ✓        x29 ✓
ffff082f7f1a9280:  ffffd63a3f2f8090  ffff8000b03900b0   ← regs[30]=lr=el0_interrupt+0x60 ✓  sp=坏SP ✓
ffff082f7f1a9290:  ffffd63a3e421a90  00000000414003c9   ← pc=el1h_64_sync+0（=故障 STP！）✓ pstate ✓
```

**dmesg 的每一行打印都与该结构的内存槽位一一对应**——dmesg 为权威读数。

> **工具伪影记录**：crash 8.0.4 的 `bt` 对本定制内核的 bad-stack 帧存在 **16 字节解析偏移**（其 frame #5 打印的 PC/LR/SP/PSTATE 实为 pt_regs 前移两槽的错读：crash-PC=el0_interrupt+96 实为 x9、crash-PSTATE=坏SP 实为 sp 槽……）。本报告全部以 `rd` 原始内存 + dmesg 对照为准。此伪影已在 §7 记录，避免后人误读。

### 3.4 事件序列（最终重构）

```
T0  探测在 CPU122 EL0 运行（EL0 pt_regs 正常保存于任务栈顶）
T1  定时器中断 → IRQ 栈处理 → el0_interrupt 返回路径：
    +0x5c: bl exit_to_user_mode（LR=+0x60）→ rseq/记账邻域执行
    —— 此窗口内 SP（EL1 栈指针）被替换为 0xffff8000b03900b0 ——   ← ★根因事件
T2  某异常（类别被覆盖丢失）向量进入 el1h_64_sync 槽，
    首指令 stp x0,x1,[sp] 在 [SP]=0xb03900b0 写入：
    ESR 0x96000047（WnR=1, level-3）、FAR=0xb03900b0          ← dmesg ESR/FAR
T3  该写故障的向量入口检测 (SP−0x150) bit14=1 → __bad_stack
    → handle_bad_stack → panic_bad_stack → nmi_panic("kernel stack overflow")
T4  SMP 停机 → kdump（37 s 内完成）→ 本 vmcore
```

---

## 4. 核心判别

### 4.1 SP 不是从完好栈弹出的（寄存器级替换证明）

- **任务栈完好**：直接转储 `[0xffff8000b039bd50..0xffff8000b039c000]`：帧链 x29 依次 be60→be90→（el0t 入口帧）；帧区 be20/be10/be00/bdf0/bdc0/bdb0/bd50 的 [x29|lr] 对全部合法，其中 lr 经符号化为 `sched_clock_cpu+20`、`irqtime_account_irq+100`、`sched_clock+16`、`rseq_ip_fixup+84`——**任务栈无任何 0xb03900b0 字样、无损坏帧**。
- **坏 SP 值在转储中无内存副本**：任务栈/IRQ 栈/可达内存直接检查无该值（注：`search -k` 在 PARTIAL DUMP 上失效——以已知存在值实测零命中——故结论基于定向 `rd`；用户页不在转储内）。
- **合法 epilogue 不可能产出该值**：`mov sp,x29`（x29=be60 合法）或 `ldp …,[sp],#N`（栈槽完好）都只能恢复合法 SP。
- ⇒ **SP 在寄存器级被替换**（或其恢复装载的交付被替换——两者均为核内事件）。

### 4.2 替换值的形态学：陈旧栈指针

```
crash> vtop 0xffff8000b03900b0
  VIRTUAL ffff8000b03900b0  (not mapped)
  PGD/PUD/PMD 在，PTE = 0                    ← 已释放的 vmalloc 空洞
crash> vtop 0xffff8000b0398000               ← 任务栈基址：正常映射
```

`0xb03900b0 = (0xb0398000 − 0x8000) + 0xB0`——指向自身栈下方 32 KB 的一个**曾存在的 vmalloc 分配**（任务栈 vmalloc 分配/释放频繁，1488 个任务、11 小时运行）。该值"形如"某个已消亡上下文的 SP——**陈旧行内容被交付到 SP**，与 9-17 Boot 1（坏 SP=0xffff80008cd500a0 = (栈基−0x8000)+0xA0、跌破 0x7F60）跨会话呈**同一几何模式 (栈基−0x8000)+小偏移**。

### 4.3 SPSR 的单比特异常（附保留意见）

自 EL1 打入的异常，其 SPSR_EL1 模式域必为 EL1h（0x…101）；实测 pstate=0x414003c9（模式 0x1001=EL0t，且 D/A/I/F 全掩码——用户态不可能掩 A/D）。`0xc9 与 0xc5 恰差 bit 2（0x4）`：**模式域单比特清除**。与 9-29（x21 bit15）、10-09（bit15×2、bit61）构成跨会话的位错家族。保留意见：本内核定制入口（FAST_SYSCALL/FAST_IRQ）对 SPSR 的处理路径未逐一审计，不排除补丁代码引入非常规 SPSR 写法的可能，故本条作为**佐证**而非独立定论。

### 4.4 与 9-29 案的同路径性

LR 指向 `el0_interrupt+0x60` = `exit_to_user_mode` 调用点（entry-common.c:925-945 区段逐行核对）；任务栈帧链为 irqtime/sched_clock 记账链。**两案的根因事件都落在"定时器中断 → irqtime 记账/sched_clock → 返回用户态"这条 CPU122 的时钟邻域路径上**（9-29 是该路径的读数异常 + 寄存器替换，本案是该路径的 SP 替换）——提示该路径的高频访存模式（percpu 时钟数据、hrtimer 结构）与缺陷触发高度相关。

---

## 5. 备择假设与排除

| # | 假设 | 排除依据 |
|---|---|---|
| 1 | 真实栈溢出（深递归把 SP 压穿） | 任务栈帧链完好且极浅（最深有效帧仅 ~0x2B0 低于入口帧区）；SP 一步跳到 −0x7F50 而非连续下探；栈内容无递归痕迹 |
| 2 | 内核软件 bug（SP 计算错误） | exit_to_user_mode 及其 callee 的 epilogue 均从完好帧恢复 SP；坏值不在任何内存槽 |
| 3 | SP 合法指向其他栈 | vtop：目标未映射；且 (SP−0x150) bit14=1 本身就是"非法栈"判定 |
| 4 | DDR 位翻转 | 栈与 pt_regs 内存全部完好；翻转应发生在内存而非寄存器；ECC 静默矛盾 |
| 5 | dmesg/crash 伪影 | 溢出栈 pt_regs 逐槽 rd 核对与 dmesg 全对齐；crash bt 的 16 字节偏移已识别并绕过 |
| 6 | 探测程序引发 | 探测在 EL0；EL0 帧（pc 0xaaaad26ea564/pstate 0x80001000）正常；35/35 pass |

---

## 6. 微架构归因与置信度

**归因**：CPU122 核内**寄存器组/操作数交付路径的替换**——SP 被替换为陈旧栈状地址（有效数据/陈旧行内容），属 9-17 报告 D1/D2 族读侧错行；SPSR 模式位单比特清除（bit 2）为佐证。案发窗口在定时器中断返回/记账路径——与 9-29 前置异常同路径。

| 维度 | 评估 |
|---|---|
| P1 单核浓度 | panic 任务 CPU=122；SP 替换发生在 CPU122 的执行流上；无其他 CPU 异常 |
| P2 复发 | 9-17 Boot 1（同形态：SP 塌陷 0x7F60 → 栈溢出 panic，寄存器含 SWAR 常量/文本）+ 本案（0x7F50）——**同几何跨 9 天复现** |
| P3 RAS 静默 | 全 boot 无 ECC/RAS 事件 |
| P5 加权 | SP-relative DABT（9-17 报告权重表 20.77× 类）、单比特位错、嵌套故障（写故障→坏栈检测） |

**置信度：高。**

---

## 7. 附：工具伪影与证据索引

| 证据 | 命令/来源 |
|---|---|
| 崩溃签名 | `vmcore-dmesg.txt` 行 2640-2681 |
| 向量首指令 | `crash> dis 0xffffd63a3e421a90 8` → `stp x0,x1,[sp]` |
| 溢出栈 pt_regs | `rd 0xffff082f7f1a9190 48`（逐槽对齐 dmesg） |
| 任务栈完好性 | `rd 0xffff8000b039bd50 27` + `p *(struct pt_regs *)0xffff8000b039beb0`（EL0 帧：pc/sp/pstate/x15 与 9-29 同常量 0x051eb851eb851eb8） |
| 坏 SP 目标未映射 | `vtop 0xffff8000b03900b0` → PTE=0 |
| 入口机制源码 | `arch/arm64/kernel/entry.S`（kernel_ventry 坏栈检测、__bad_stack）、`entry-common.c`（handle_bad_stack/panic_bad_stack/el0_interrupt）、`traps.c:929` |
| **工具伪影（重要）** | ① crash `bt` 对本案 bad-stack 帧有 16 字节解析偏移（frame #5 的 PC/LR/SP/PSTATE 实为错位读数）——分析须以 rd 原始内存为准；② `search -k` 在 PARTIAL DUMP 上静默失效（对已知存在值零命中），不可用于"值不存在"论证 |

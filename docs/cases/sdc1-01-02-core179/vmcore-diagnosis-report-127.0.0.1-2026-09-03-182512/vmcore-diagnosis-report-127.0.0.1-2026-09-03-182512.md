# CPU179 转储深度诊断报告（第 1 次独立重研究）

## ——35 次"spurious 假翻译故障"前兆全部钉死在同一核心，最终以一次 26 位翻转的 per-cpu 偏移表装载把调度器热路径送进非规范地址区

| 项目 | 内容 |
|---|---|
| 目标转储 | /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-03-18:25:12（vmcore 73.7GB, Kdump compressed v6, PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026, 192 CPU, 8 NUMA 节点, 768GB RAM |
| CPU | Kunpeng-920 (TaiShan-v110), MIDR 0x481fd010 |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP（KASLR 开启, crashkernel=1024M,high） |
| 崩溃时间 | 2026-09-03 18:24:21（uptime 322246s = 3.73 天；开机时刻 2026-08-31 00:53:35） |
| 崩溃上下文 | CPU179（MPIDR 0x7a0300, Node 7）, PID 16, rcu_sched 内核线程, TASK_WAKING |
| 前兆 | 35 次 "Ignoring spurious kernel translation fault" WARNING，全部 CPU179，全部落在 Node 7 线性映射区 |
| 一行结论 | 【强推】CPU179 装载通路（load data path）间歇性返回非内存真值的多位腐化数据：致命点是 `ldr x20,[x0,w25,sxtw#3]` 应取 `__per_cpu_offset[12]=0xffffb617dc4d6000` 实得 `0x00ffffb617dd3940`（26 位差异、全内存无此值），加法器随后如实算出非规范地址 0x00ffc99ebbaad120 → level 0 翻译故障 |

---

## 1. 执行摘要

1. **现象**：192 核 NEON 压测机（191/192 核跑 `neon_rot_2src`，load average 98.6）连续运行 3.73 天后，rcu_sched 内核线程在 `find_busiest_group+0x140` 处以 `Unable to handle kernel paging request at virtual address 00ffc99ebbaad120`（ESR=0x96000004, EC=0x25 DABT, FSC=0x04 level 0 translation fault）崩溃，触发 kdump。
2. **签名**：坏地址 0x00ffc99ebbaad120 的 bits[63:48]=0x00ff——既非用户区（0x0000…）也非内核区（0xffff…），dmesg 明确报 "address between user and kernel address ranges"。这类"半高"地址在多案中反复出现，是本机组 SDC 的指纹级签名。
3. **闭合验证（实锤）**：崩溃点的 `x27 = x1 + x20`（模 2^64 精确成立，加法器无恙），而 `x20` 应为 `__per_cpu_offset[12] = 0xffffb617dc4d6000`（crash 直接读装载地址 `0xffffc9e8a40d5630` 的内存真值证实），实得 `0x00ffffb617dd3940`——**内存真值完好、寄存器值是假的**。若 x20 正确，x27 = 0xffff8000801af6c0 = CPU12 的 runqueue 实例（crash `vtop` 证实该地址 PTE VALID，本应完全正常）。
4. **前兆链（实锤）**：同一开机内、同一 CPU179 上，先后发生 35 次 "Ignoring spurious kernel translation fault"（arm64 6.6 的 `is_spurious_el1_translation_fault` 用 AT 指令复核发现页表其实有映射）。35 个故障地址全部位于 Node 7 线性映射区（CPU179 本节点），全部在 `/proc/interrupts` 读取路径（`show_interrupts → seq_printf → __memcpy`）。末次前兆距致命崩溃仅 **21.17 秒**。
5. **置信度**：微架构根因判定为【强推】——"装载通路/页表走查读出被多位腐化"有多源收敛证据（真值对照、全内存搜索无坏值、前兆同核同通路、ALU 完好、软件成因排除），但物理层（RF 位单元 vs L1 fill buffer vs PTW cache）无法用软件区分，标注【假设】并给出验证途径。
6. **处置**：该机器 CPU179 所在物理核（socket 1 / Node 7）硬件可靠性已不可信，建议下线隔离该核（`nohz_full`/`isolcpus` 不可修复此问题，必须 RMA 或换板）；同时按第 9 节建议给调度器关键指针装载加校验。

---

## 2. 证据规则与方法

- **证据源**：仅 `vmcore-dmesg.txt`（4202 行）与 crash 8.0.4 对 73.7GB vmcore 的直接读取（namelist `/tmp/vmlinux-0102`, BuildID 276194e5f356f9c4bc570bb0a750394d7b768035, 带 debug_info）。未阅读任何既有诊断文档，未参考任何其他案例结论。
- **诚实铁律**：所有命令真实执行、输出全文存 `dmesg_forensics.txt`；所有 64 位运算经 `algebra.py`（模 2^64）复算，输出存 `algebra_out.txt`。crash 加载一次约 1-2 分钟，全部采用 `-i cmdfile` 批处理 + timeout 590，共 11 个会话，无一超时失败。
- **三级置信**：【实锤】可直接复核（如内存真值 vs 寄存器值对照）；【强推】多源收敛但缺物理层直接对照；【假设】无法软件验证，只给验证途径。
- **工具**：crash 8.0.4-17.oe2403sp4（`sys/panic/bt/bt -r/set/mach/ps/runq/dis/rd/vtop/p/struct/search/sym`）、Python3（位级代数）、grep/sed/awk。

---

## 3. 本次开机时间线【时间线】

| uptime | 墙钟 | 事件 | dmesg 行号 | 置信 |
|---|---|---|---|---|
| 0.000000 | 2026-08-31 00:53:35 | 开机，CPU0x80000 引导，192 CPU 8 节点全部上线 | 1, 1255 | 实锤 |
| 89.9 | 08-31 00:55:05 | 最后一条常规启动日志（dm-2 capability deprecation） | 2576 | 实锤 |
| 71822.06 | 08-31 20:50:37 | **前兆 #1**：irqbalance(PID 9736) 读 /proc/interrupts，spurious fault @ ffff6040629fe5d1 | 2579-2623 | 实锤 |
| 142792.05 | 09-01 16:33:27 | 前兆簇 A：10 秒内 6 次，irqbalance | 2624-2848 | 实锤 |
| 142835.31 | 09-01 16:34:10 | 前兆 3 次，**pmdalinux**(PID 10282)（唯一非 irqbalance 受害者） | 3209-3343 | 实锤 |
| 146862.04 | 09-01 17:41:17 | 前兆簇 B：20 秒内 8 次 | 3344-3704 | 实锤 |
| 159162.04 | 09-01 21:06:17 | 前兆簇 C：3 次 | 3930-4019 | 实锤 |
| 322215.06 | 09-03 18:23:50 | 前兆 #33（沉寂 45 小时后复发） | 4065 | 实锤 |
| 322225.05 | 09-03 18:24:00 | **前兆 #35（末次）**，irqbalance @ ffff604061374839 | 4111-4157 | 实锤 |
| 322246.22 | 09-03 18:24:21 | **致命 Oops**：rcu_sched，find_busiest_group+0x140，FAR=00ffc99ebbaad120 | 4158-4202 | 实锤 |

间隔量化（algebra.py 第 7 节）：首次前兆距开机 19.95h；前兆总窗口 69.56h；**末次前兆→致命崩溃 21.17 秒**。

---

## 4. 故障现象【故障现象】

### 4.1 Oops 原文

```
[322246.221818] Unable to handle kernel paging request at virtual address 00ffc99ebbaad120
[322246.230565] Mem abort info:
[322246.234146]   ESR = 0x0000000096000004
[322246.238684]   EC = 0x25: DABT (current EL), IL = 32 bits
[322246.244788]   SET = 0, FnV = 0
[322246.248630]   EA = 0, S1PTW = 0
[322246.252557]   FSC = 0x04: level 0 translation fault
[322246.268165]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[322246.280107] [00ffc99ebbaad120] address between user and kernel address ranges
[322246.288036] Internal error: Oops: 0000000096000004 [#1] SMP
[322246.396732] CPU: 179 PID: 16 Comm: rcu_sched Kdump: loaded Tainted: G        W
[322246.425388] pc : find_busiest_group+0x140/0xb60
[322246.430717] lr : find_busiest_group+0x11c/0xb60
```

要点：WnR=0（读装载触发）、S1PTW=0（不是页表走查自身出错，而是普通数据装载的目标地址非法）、level 0 fault（PGD 之前就无效——因为地址非规范）。

### 4.2 全量寄存器（x0–x30，致命块原文）

```
pstate: 204000c9 (nzCv daIF +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
pc : find_busiest_group+0x140/0xb60
lr : find_busiest_group+0x11c/0xb60
sp : ffff8000821ab830
x29: ffff8000821ab9b0 x28: ffff8000821ab940 x27: 00ffc99ebbaad000
x26: ffff604003e9e3c0 x25: 000000000000000c x24: ffffc9e8a40d5000
x23: 0000000000000400 x22: ffff604003e9e3c0 x21: ffffc9e8a40cfcb0
x20: 00ffffb617dd3940 x19: ffff8000821aba40 x18: 0000000000000000
x17: 0000000000000000 x16: 0000000000000000 x15: 0000ffffa465a588
x14: 0000000000000000 x13: 0000000000000000 x12: 0000000000000000
x11: 0000000000000060 x10: 0000000000000120 x9 : ffffc9e8a22aae58
x8 : ffff8000821ab998 x7 : 0000000000000000 x6 : 000000000000000c
x5 : fffffffffffff000 x4 : 0000000000000000 x3 : 000000000000000c
x2 : 0000000000003000 x1 : ffffc9e8a3cd96c0 x0 : 000000000000000c
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

关键寄存器判读：x27=00ffc99ebbaad000（非规范基址）；x20=00ffffb617dd3940（**坏值**，见第 7 节闭合等式）；x25=0xc（循环当前 CPU=12）；x1=ffffc9e8a3cd96c0（= `&runqueues` 符号地址，crash 证实）；x24=ffffc9e8a40d5000（`__per_cpu_offset` 数组基址-0x5d0）；x22=x26=ffff604003e9e3c0（sched_group，crash `struct` 证实）。

### 4.3 Call trace（完整）

```
find_busiest_group+0x140/0xb60
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

调度器最热路径：rcu_sched 在 schedule_timeout 睡眠到期后被唤醒，pick_next_task_fair 发现本核空闲，走 newidle_balance 去 120 个 CPU 的调度组里找可拉任务，在统计各 CPU runqueue 负载时崩溃。crash `bt` 输出与 dmesg 逐帧一致（附件）。

### 4.4 前兆异常原文（35 次之一，其余同构）

```
[71822.056502] ------------[ cut here ]------------
[71822.056515] Ignoring spurious kernel translation fault at virtual address ffff6040629fe5d1
[71822.056524] WARNING: CPU: 179 PID: 9736 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
[71822.056672] CPU: 179 PID: 9736 Comm: irqbalance Kdump: loaded Not tainted ...
[71822.056764]  __memcpy+0x80/0x240
[71822.056772]  seq_printf+0xc4/0xe8
[71822.056781]  show_interrupts+0x1d4/0x498
[71822.056786]  seq_read_iter+0x168/0x478
...
[71822.056824] ---[ end trace 0000000000000000 ]---
```

统计（dmesg_forensics.txt 末段）：35 次全部 `WARNING: CPU: 179`；进程分布 irqbalance(PID 9736) 32 次、pmdalinux(PID 10282) 3 次；其中 2 次的调用栈多一层 `arch_show_interrupts`（读的是 /proc/interrupts 的 arch 统计段），其余为 `show_interrupts+0x1d4`。**35 个故障地址互不重复、全部落在 ffff6040xxxxxxxx（Node 7 线性映射）**。

### 4.5 RAS 负证据（dmesg grep）

对 `Machine check|Hardware error|EDAC .*error|ECC error|ras:.*error|memory failure|External abort|SError|corrected error|Uncorrected|Deferred` 全文检索：仅命中 `EDAC MC: Ver: 3.0.0`（驱动版本行）与 systemd 启动横幅，**无任何真实硬件错误记录**。ghes_edac 已挂载（`EDAC MC0: Giving out device to module ghes_edac`）但零报告——固件/硬件没检测到任何 ECC/总线错误，但内核软件路径上却堆了 35 次"假翻译故障"+1 次致命坏值。这是"错误发生在核内数据通路、未经过任何有 ECC 覆盖的存储域"的典型形态。

---

## 5. 业务现象

- **致命崩溃受害者 rcu_sched（PID 16）**：RCU 全局宽限期内核线程，系统最长寿的内核线程之一，每隔几毫秒经 schedule_timeout 睡眠/唤醒，是 `__schedule → newidle_balance → find_busiest_group` 的常客。它不是"业务"，但它全天候在所有 CPU 上被调度——坏核上的任何任务迟早踩中。
- **前兆受害者 irqbalance（PID 9736）**：中断均衡守护进程，默认每 10 秒读一遍 `/proc/interrupts`（与观测到的前兆 10 秒节律吻合），ps 显示其常驻 CPU179。它读的是 seq_file 缓冲区（kmalloc 来的 Node 7 页面），`__memcpy` 把格式化字符串拷进缓冲时触发假翻译故障。
- **前兆受害者 pmdalinux（PID 10282）**：Performance Co-Pilot 的 Linux 数据采集器，同样周期性读 /proc 统计。crash 显示其当前在 CPU120，但事发时运行于 CPU179。
- **背景负载**：191/192 个 CPU 正在跑 `neon_rot_2src`（NEON 双源旋转指令压测），负载 98.6。机器处于持续满载 NEON 压测状态 3.7 天——功率/温度/时钟边沿压力拉满的典型 SDC 诱发环境。

---

## 6. 诊断定位过程【诊断定位过程】

**P1 dmesg 勘察**：提取开机指纹（6.6.0-145.3.23.154, 192 CPU, 768G, crashkernel 1024M high）；grep 出 35 次前兆 WARNING + 1 次致命 Oops；确认前兆全部 CPU179、全部 Node 7 地址、RAS 零记录。

**P2 崩溃块提取**：抄录全部寄存器与 Code 窗口；解码 Code 最后一条 `f9409377` = `ldr x23, [x27, #288]`（Python3 位级解码，algebra.py 第 1 节），确认 FAR = x27+288 精确成立；x27+288 = 00ffc99ebbaad120，bits[63:48]=0x00ff 非规范。

**P3 crash 加载**：最小命令集（sys/panic/bt）确认 73.7GB vmcore 可加载（加载约 1-2 分钟，PARTIAL DUMP，仅少量 SDEI stack seek error，不影响分析）；随后分 10 个批处理会话完成 set/ps/runq/dis/rd/vtop/p/struct/search 全量取证。

**P4 内存真值对照（本案的决胜局）**：
1. `sym find_busiest_group` + `dis` 反汇编崩溃函数，锁定指令窗口（crash 原始输出）：
   ```
   find_busiest_group+300: ldp x0, x1, [sp, #8]        // x0=[sp+8]=数组基址, x1=[sp+16]=&runqueues
   find_busiest_group+304: ldr x2, [x28, #8]
   find_busiest_group+308: ldr x20, [x0, w25, sxtw #3] // x20 = __per_cpu_offset[w25] ← 坏值源头
   find_busiest_group+312: mov w0, w25
   find_busiest_group+316: add x27, x1, x20            // x27 = &runqueues + per_cpu_offset = &rq(w25)
   find_busiest_group+320: ldr x23, [x27, #288]        // ← 崩溃指令 (pc=+0x140=+320, 读 rq->cfs 域内偏移)
   ```
   x0 的来源在函数序言：`adrp x24, node_data+560` → `add x0, x24, #0x5d0` → `str x0,[sp,#8]`，即 x0 = 0xffffc9e8a40d55d0（`__per_cpu_offset` 数组基址，crash 读该区域证实）；x1 的来源是 `str x1,[sp,#16]`，x1 = 0xffffc9e8a3cd96c0（`&runqueues`）。
2. `px &runqueues` = 0xffffc9e8a3cd96c0，与崩溃时 x1 寄存器**逐位一致**——x1 无辜。
3. w25=x25=12，Python3 算出装载地址 `0xffffc9e8a40d55d0 + 12*8 = 0xffffc9e8a40d5630`；crash `rd -64` 直读：**内存真值 = 0xffffb617dc4d6000**；`p __per_cpu_offset[12]` 独立证实同一值。装载地址本身 `vtop 0xffffc9e8a40d5630` → PA 0x204b6c0d5630，映射正常。
4. **x20 观测值 0x00ffffb617dd3940 ≠ 内存真值 0xffffb617dc4d6000，海明距离 26 位**。
5. `search -t 16 00ffffb617dd3940` 全转储内存搜索：**零命中**——坏值不在任何内存页里（含未在本报告引用的全部已转储页）。
6. 反事实：x20 若正确 → x27 = 0xffff8000801af6c0；`vtop 0xffff8000801af6c0` 返回 PA 0x37ffe2e6c0、PTE `e80037ffe2ef03 (VALID|SHARED|AF|NG|PXN|UXN|DIRTY)`——已映射的正常内核地址，后续 `ldr x23,[x27,#288]` 本应安静完成。
7. 栈上残留（`rd`/`bt -r`）：异常帧下方有 `ffffb617dd30c000` = `__per_cpu_offset[119]`——上一轮循环到 CPU119 时的正常装载残留，说明同一循环此前的装载都是好的，坏的是"这一次"。

**P5 软件成因排除**：
- 该指令是调度器热路径，全球 Linux 每秒执行数亿次；`__per_cpu_offset[]` 表自开机后从未被写（静态表），内存真值至今完好。
- 若是软件 bug（越界索引、并发释放），坏值应是"某个真实内存内容"，但 search 证明全内存无此值；26 位翻转也不是任何单比特/单字节软件错误模式。
- 35 次前兆与致命点**同核（CPU179）**：软件 bug 不挑核，特别是 rcu_sched/irqbalance 这类全机漫游的线程偏偏 100% 在 CPU179 出事——概率上只有"硬件单元故障"能解释。
- 内核 6.6.0-145.3.23.154 为 openEuler 稳定发行版，非 DEBUG 补丁版本。

**P6 定位收敛**：
- 前兆形态（spurious translation fault：硬件 MMU 说不映射、软件 AT 复核说映射）→ PTW/TLB 读出通路被腐化；
- 致命形态（LDR 目标寄存器拿到全内存不存在的 26 位坏值，而源地址内容完好）→ 装载数据通路被腐化；
- 二者共同点：**CPU179 内部、读路径（load path）、多位、瞬态（内存内容始终正确）**；
- ALU 通路被排除（x27=x1+x20 精确成立）；写路径无证据受损（无数据结构被写坏）；缓存一致性无证据受损（其他 191 核未报错）。

---

## 7. 逻辑链条

**指令语义**（`dis` 实测，vmlinux-0102 debug_info）：
- `ldr x20, [x0, w25, sxtw #3]`（+308）：从 `__per_cpu_offset[12]` 装载 CPU12 的 per-cpu 偏移到 x20。语义上是纯数据搬移，不修改数据。
- `add x27, x1, x20`（+316）：`&runqueues + offset` = CPU12 的 runqueue 虚拟地址。纯 ALU。
- `ldr x23, [x27, #288]`（+320，pc 所在）：读 `rq->cfs` 域内偏移 288 的字段。这是崩溃指令，但它的输入 x27 已经是坏的。

**闭合等式【实锤】**（全部 Python3 模 2^64 复算，algebra.py 第 1-4 节）：
```
x27(观测) = x1 + x20(观测)                 → 0x00ffc99ebbaad000 精确成立（ALU 完好）
FAR       = x27 + 288                       → 0x00ffc99ebbaad120 精确成立（指令解码吻合）
x20(应得) = mem[0xffffc9e8a40d5630]         → 0xffffb617dc4d6000（crash 直读 + p 佐证）
x20(实得) = 0x00ffffb617dd3940              → 与真值差 26 位，全内存 search 零命中
x27(应得) = 0xffffc9e8a3cd96c0 + 0xffffb617dc4d6000 = 0xffff8000801af6c0（vtop: PTE VALID）
```
即：**坏的不是加法、不是页表、不是内存，而是 x20 的来源——那次 LDR 的返回数据**。

**坏值结构【实锤层面的观察 + 推断层面的解读】**：
```
真值   pc[12]  = ff ff b6 17 dc 4d 60 00
观测   x20     = 00 ff ff b6 17 dd 39 40
pc[179]>>8     = 00 ff ff b6 17 dd b0 40   （高 48 位与观测值完全一致）
```
x20 的高 6 字节 `00 ff ff b6 17 dd` 恰好等于 `per_cpu_offset[179] >> 8` 的高 6 字节（也等于 pc[111..185] 任一 dd 前缀表项右移一字节的高位），低 2 字节 3940 与任何表项的移位形态差 3 位。这像是"某次先前装载的残留数据在通路上被错位一字节后又叠加多位翻转"。我们如实说明：**该字节级形态无法进一步闭合**（坏值不可能从已转储内存中读到，而 PARTIAL DUMP 无法穷尽未转储页），但无论其来源细节如何，"寄存器拿到的不是装载地址当前内容"这一核心事实已由真值对照钉死。

**反事实推演**：若 x20 取到真值，x27=0xffff8000801af6c0 是已映射内核地址（vtop 实证），`ldr x23,[x27,#288]` 正常返回，rcu_sched 继续睡眠循环，机器不会崩。崩溃的唯一充分必要条件是 x20 变坏。

**前兆与致命的同一性**：35 次前兆是同一核心上"MMU 硬件翻译结果与内存真值不符"，致命点是同一核心上"LDR 数据结果与内存真值不符"。两者都是**读通路输出 ≠ 存储真值**，且都在 CPU179、都在其本节点（Node 7）地址范围。前兆是低烈度（页表重走能自愈，内核 RATelIMIT 后继续跑），致命点是高烈度（坏指针直接进加法器，错误被放大为非规范地址）。末次前兆与致命点仅隔 21 秒，且发生在几乎连续的调度/读取活动中——同一故障源在恶化。

**诚实声明**：
1. PARTIAL DUMP 只覆盖部分内存页，`search` 零命中不能数学上排除坏值存在于未转储页；但"装载地址本身已映射且内容正确"已独立证实，坏值即使存在于别处也必然是"读错了地方/读错了数据"的结果。
2. 我们无法用软件区分故障发生在 L1 fill buffer、load 数据总线、寄存器文件写端口还是旁路网络——这需要芯片级 DFT/扫描诊断（见第 9 节）。
3. x20 坏值的字节级"右移一字节"形态是观察事实，其物理成因是推断。

---

## 8. 故障根因（微架构级结论）

**判定：CPU179 核内读数据通路（load path，含 TLB/PTW 翻译结果路径与 L1→RF 数据返回路径）存在间歇性多位数据腐化故障，属核内瞬态/半永久硬件缺陷，非 DRAM、非软件。【强推】**

分项置信：
- x20 装载结果 ≠ 内存真值（26 位坏值、全内存无此值、真值完好）：【实锤】
- 前兆 35 次 spurious translation fault 证明同核 MMU 翻译读出通路亦被腐化：【实锤】（事件本身）→【强推】（同为读通路故障的归并）
- 故障物理位置在核内（而非 DDRC/HHA/LLC/DRAM）：【强推】——依据：内存真值完好、无跨核受害、无 ECC 报错、故障 100% 绑定 CPU179 且地址全在本节点
- 具体微架构单元（fill buffer / 数据总线 / RF 位单元 / PTW cache）：【假设】——验证途径：隔离该核后复测；下电冷启后复测（区分半永久 vs 瞬态）；如厂商可介入，用 scan/DFT 定位；对比同机型其他核的统计失效率
- 诱发环境（4 天满载 NEON 压测的温度/电压/时钟边沿压力）：【假设】——验证途径：降低压测强度观察前兆频率变化

排除清单：ALU 加法通路（x27=x1+x20 精确成立，实锤排除）；页表内容（vtop 走查与真值一致，排除）；DRAM 内容（kdump 读回真值正确，排除"内存被写坏"）；缓存一致性协议（191 个其他核零异常，排除）；软件 bug（热路径 + 坏值不存在于内存 + 挑核，排除）。

---

## 9. 启示

### 9.1 微架构定位的意义

本案把"SDC"从模糊的名词落到了具体的流水线段：**从 L1/PTW 到寄存器文件的读返回通路上，某处发生了多位数据腐化，且不经过任何受 ECC/parity 保护的存储介质**。这解释了为什么 RAS 全静默——错误产生和消亡都在核内组合逻辑/缓冲里，没有落进任何可检测的存储阵列。前兆（假翻译故障）与致命（坏偏移装载）的同核共现，为"读通路故障"提供了两条互相独立又互相印证的证词。对现场工程的意义是：**spurious translation fault WARNING 不是可以忽略的噪音，它是读通路 SDC 的免费探针**——本机在致命崩溃前用它免费报警了 35 次、跨越 69.5 小时，却没有任何机制把这 35 次报警升级为"该核可靠性已失效"的判定。

### 9.2 芯片设计与实现启示（具体、可落地）

1. **Load path 端到端校验**：per-cpu 基址、页表基址、调度器指针这类"小而关键"的数据通路目前是裸的。建议对 L1 fill buffer→RF 写端口加 parity（一行 8 字节 1 个 ECC 纠检位成本可控），或至少对 fill buffer 加 parity 报错（fail-fast 优于 fail-silent）。TaiShan-v110 的 L1D 有 ECC，但显然覆盖不到本案的位置——说明缺口在 fill/bypass 段，实现时应把校验域延伸到 RF 写入边界。
2. **PTW 输出校验**：页表走查结果（PTE 值 + 翻译成功/失败判定）在进入 TLB 前应有 parity/冗余比对。本案前兆表明"走查结果错"与"走查数据错"都发生过；PTW 输出加一位冗余判定（或对 level-0 fault 做一次自动重试再陷入）可以把假翻译故障消掉。
3. **关键调度数据结构冗余校验**：`__per_cpu_offset[]` 每项只有 64 位、全机 192 项共 1.5KB。内核可以在校验点（如 per-cpu 访问宏）加 canary/副本比对，成本可忽略。更普适地，调度器热路径的指针装载可选用 load-and-verify 双读（第二次重读比对），虽然会牺牲少量性能，但可把 SDC 变成可检测事件。
4. **错误传播屏障**：本案坏偏移加进 runqueues 基址后错误被"放大"成非规范地址——幸好 ARM64 顶字节是 0x00ff 才在翻译层被拦下；若坏值落在合法区间内，这个坏指针会静默写坏随机内存。设计上应利用"高位必须是 ffff/0000"这类结构不变量做早期拦截（内核已用 address range check 报出 "between user and kernel ranges"，这是防线生效的正面案例）。
5. **fail-fast vs silence 的权衡**：RAS 全静默而内核已报警 35 次——固件/硬件的检测盲区比想象的大。建议把诸如 spurious fault 这类内核侧异常信号纳入 RAS 事件通道（如通过 GHES 上报计数），让 BMC 侧能看到"软件可观测的硬件异常"趋势。
6. **kdump 可靠性设计**：本案 PARTIAL DUMP 丢失了部分页，导致坏值溯源只能做"转储内零命中"这种不穷尽的否定。建议 kdump 策略对内核数据段（含 percpu 表、调度器结构）强制全量转储。
7. **DFT/在线测试钩子**：在核内读通路埋设计期钩子（如自测试 load 序列：写已知图案→立即读回比对），供现场健康监测周期性调用，可把"读通路是否健康"变成可执行命题。这类钩子对验证签核同样有价值：本案例形态应转化为 STA/故障仿真清单里的新故障类别（多位、跨字节、通路上瞬态）。

### 9.3 对系统软件/RAS 的启示

1. **把 spurious translation fault 从 WARNING 升级为健康事件**：openEuler 可加一个 per-CPU 计数器与阈值策略（如单核 24h 内 N 次即告警/自动 offline 该核）。本案若有此机制，机器在 08-31 20:50 就能被标记，而不是 09-03 18:24 崩溃。
2. **调度器可加廉价的 per-cpu 基址自检**：`__per_cpu_offset[]` 是只读静态表，可在 tick 里抽查比对副本（开销纳秒级）。
3. **压测平台的 watchdog**：满载 NEON 压测 3.7 天才崩、且崩在离前兆 21 秒处——建议长稳测试平台把 dmesg 异常关键词（cut here/spurious/segfault）接入自动熔断，而不是等 panic。

---

## 10. 处置建议

1. **立即**：从池中摘除该机器的 CPU179 所在物理核。注意 `isolcpus`/`nohz_full` 只能减少调度频度，rcu_sched 等内核线程仍可能被排到该核——必须确认固件/内核能彻底 offline 该核（hotplug offline CPU179 及其 SMT 兄弟），否则建议整机下线。
2. **短期**：保留本转储与 dmesg 作为 RMA 证据链；对同批次机器的 dmesg 做 `Ignoring spurious kernel translation fault` 关键词巡检，任何命中即启动同流程排查。
3. **中期**：与厂商联合做 CPU179 的复测（隔离压测/冷启复测），确定是半永久缺陷（RMA）还是电压/温度敏感（可调参规避）；无论结果如何，落实 9.3 的内核侧早期告警机制。
4. **不要**尝试通过重装系统/换内核版本"修复"——本案证据链已完整排除软件成因。

---

## 附录：命令索引（全部取证命令，可复核）

dmesg 侧（输出见 dmesg_forensics.txt 第一部分）：
```
wc -l /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-03-18:25:12/vmcore-dmesg.txt
head -1 <dmesg>                                    # 开机指纹
grep -n 'Kernel command line' <dmesg>
grep -n 'crashkernel\|Memory:\|smp: Brought up' <dmesg>
grep -n 'CPU179' <dmesg>                           # CPU179 拓扑/MPIDR
grep -n 'SRAT: PXM 7 -> MPIDR 0x7a0300' <dmesg>    # CPU179 → Node 7
grep -c 'Ignoring spurious kernel translation fault' <dmesg>   # = 35
grep -n 'Ignoring spurious kernel translation fault' <dmesg>   # 全部地址
grep -oE 'WARNING: CPU: [0-9]+ PID: [0-9]+' <dmesg> | sort | uniq -c
grep -n -E 'Machine check|Hardware error|ECC|... ' <dmesg>     # RAS 负证据
tail -50 <dmesg>                                   # 致命块
sed -n '2579,2624p' <dmesg>                        # 首个前兆全文
```

crash 侧（全部 `timeout 590 crash /tmp/vmlinux-0102 <vmcore> -i <cmdfile>`，输出见 dmesg_forensics.txt 第二部分及各会话存档）：
```
sys / panic / bt / bt -t / set / mach                       # 会话1
ps | head -30 / runq / timer / dev -p / net                 # 会话2
p __per_cpu_offset[179] / px runqueues / dis -l 0xffffc9e8a22aae44
rd -64 0xffff8000821ab9b0 4 / rd -64 0xffff8000821ab980 8 / bt -r / p jiffies   # 会话3
sym find_busiest_group / dis find_busiest_group             # 会话4
struct sched_group 0xffff604003e9e3c0 / rd -64 0xffff604003e9e3c0 8
struct sched_group_capacity / rd -64 0xffff604003e9aba0 12  # 会话5
rd -64 ffffc9e8a40d4dd0 180 / rd -64 ffffc9e8a40d55d0 32 / rd -64 ffffc9e8a40d5630 2   # 会话6-8: per_cpu_offset 数组真值
rd -64 ffffc9e8a40d5360 40 / ...5490 40 / ...55e0 16 / ...5760 48 / ...57f0 48 / ...5b60 区段
p __per_cpu_offset[12] / px &runqueues                      # 会话9
vtop 0xffff8000801af6c0 / vtop 0xffffc9e8a40d5630 / vtop 0xffff604003e9e3c0   # 会话10
search -t 16 00ffffb617dd3940                               # 会话11: 坏值全内存搜索（零命中）
dis do_translation_fault / dis __do_kernel_fault / dis is_spurious_el1_translation_fault.constprop.0
ps -l | grep -E 'irqbalance|pmdalinux|rcu_sched'
```

代数复算：`algebra.py` → `algebra_out.txt`（8 节：闭合验证/真值对照/反事实/ALU 完好性/坏值结构/前兆聚类/时间线/调度组解码）。

---
*报告生成：2026-09-06。全部结论基于当次独立取证的真实命令输出；凡未能实证之处均已标注置信级别。*

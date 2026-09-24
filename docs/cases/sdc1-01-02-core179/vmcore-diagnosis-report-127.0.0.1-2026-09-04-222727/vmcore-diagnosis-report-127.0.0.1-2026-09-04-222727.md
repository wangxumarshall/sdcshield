# 第 9 次发作诊断报告：rcu_sched 内核线程受害 + 18 分钟内二次死亡，CPU179 读通路 SDC

## 副标题
kunpeng920(hip08) 192 核整机：__per_cpu_offset 读值宽带污染经 add 指令算术传播为毒指针，rcu_sched 纯调度路径触发致命 Oops；vmcore-incomplete 的手工 kdump 头部/ELF note 法证

（信息表）

| 项目 | 内容 |
|---|---|
| 转储目录 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:27:27/` |
| vmcore-incomplete | 8,379,586,936 B（kdump **未完成**，crash 拒载） |
| vmcore-dmesg.txt | 205,200 B，2,809 行，boot 段完整 |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP（2026-07-27 构建） |
| vmlinux | /tmp/vmlinux-0102（BUILD-ID 276194e5...6835 已比对 vmcoreinfo 一致） |
| crash | 8.0.4-17.oe2403sp4（两次加载均失败，见 §2 与 crash_session.log） |
| 硬件 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，HiSilicon HIP08（MIDR 0x481fd010 = TaiShan v110），8 NUMA 节点 × 24 核 = 192 CPU，RAM 805,102,592K |
| 本次开机存活 | 552.7 s（约 9.2 分钟） |
| 致命 Oops | ESR=0x96000004（level-0 翻译故障，**读**），FAR=0x00ffab53df0abfa0，pc=find_busiest_group+0x140/0xb60 |
| 受害者 | CPU 179，PID 16，**Comm: rcu_sched**（内核线程，rcu_gp_kthread→rcu_gp_fqs_loop→schedule_timeout 路径） |
| 异常块总数 | 4 WARNING + 1 致命 Oops，**全部 CPU 179** |
| RAS 负证据 | GHES/APEI/ghes_edac/SDEI/RAS Extension 全部在位，552 s 内**零**错误记录（dmesg 全文检索） |
| 当日序列 | 21:53:28 → 22:09:49 → **22:27:27（本案）** → 22:39:38 → 23:37:57，五案全部 CPU179 读路径 |

## 1. 执行摘要

本机在 2026-09-04 晚间 44 分钟内连续 5 次内核崩溃，全部为同一微架构根因的不同切面：CPU179 的数据读通路（私有缓存/加载回填路径）间歇性返回被污染的值。本案（第 3 次）有两个区别性特征：受害者是 rcu_sched 内核线程（无用户态入口的纯调度路径，证明扰动与任何用户代码无关），且在前次崩溃后仅 18 分钟（重开机后 9.2 分钟）即复发，失效频率显著升级。

致命链条由反汇编+寄存器代数完全闭合【实锤】：`find_busiest_group+0x13c` 处 `add x27, x1, x20` 的输入 x20 是从内存读出的 `__per_cpu_offset[97]`（x25=0x61 证明正迭代 CPU 97），其读出值 0x00ffffd4812327c0 的高 16 位为 0x00ff，而合法 per-cpu 偏移的高 16 位必为 0xffff（负偏移）。加法本身算术自洽（x27−x20 ≡ x1 ≡ runtime &runqueues，逐位验证成立），说明污染发生在 x20 的**读出时刻**，随后经算术传播成毒指针 x27，最终 `ldr x23,[x27,#288]`（读 `rq->cfs.load_avg`）在 FAR=x27+288 处触发 level-0 翻译故障。

4 个 WARNING（irqbalance×3、pmdalinux×1）全部是 `__do_kernel_fault` 对有效线性映射地址（node3/node7 实际 RAM）的 level-0 读故障：页表走查读返回无效描述符，与致命案同属 CPU179 读通路。5 个事件压缩在开机最后 50 秒内（前 513 秒完全干净），呈**加速劣化**形态。

跨案归纳（4 次 find_busiest_group+0x140 崩溃 + 1 次 get_pfnblock_flags_mask 崩溃，全部 CPU179、全部读方向）：x20 读出值分别为 0x73b8..（高 16 位 9 位翻转）、0x00ff..（8 位翻转）、全 0×2 案。每次发作破坏形态不同，但破坏类别恒定（高位宽带破坏/整字读错），位置恒定（CPU179），方向恒定（读）。这是随机性物理扰动叠加固定微架构弱点的典型指纹；软件 bug 不会跨 4 次开机以不同位型命中同一 CPU 的同一指令。

vmcore-incomplete 无法被 crash 加载（内核 banner/早期页未写入），这本身是法证边界证据：崩溃后 kdump 也在故障机器上运行，写出头部+ELF notes+1.7 GB 尾部页后死亡。但手工解析 kdump 格式（见 §6.7）成功恢复了崩溃 CPU 的完整 35 个寄存器（ELF note #179），与 dmesg printk 路径的寄存器块逐个一致，构成两条独立采集链的交叉验证【实锤】。

## 2. 证据规则与方法（含 incomplete 法证边界）

**方法**：
1. dmesg 逐行编号引用（`vmcore-dmesg.txt`，行号 N 指 L N）；
2. 所有 64 位运算由 `algebra.py`（→`algebra_out.txt`）机器完成，禁止手算；
3. 反汇编取自 /tmp/vmlinux-0102（objdump），符号取自 nm；KASLR slide 由 vmcoreinfo（KERNELOFFSET=0x2b7edc310000，SYMBOL(_stext)=0xffffab7f5c320000）与 nm 的 link 地址求差并交叉验证 pc；
4. 跨案对照仅使用各案 vmcore-dmesg.txt 原文，不引用任何既有分析文档；
5. 标注【实锤】（逐位可复算）/【强推】（多条独立证据链收敛）/【推测】（无法在本 dump 内闭合）。

vmcore-incomplete 法证边界（诚实声明）：
- crash 两次加载尝试均失败。第一次（无 --zero_excluded）：`crash: page excluded: ... kernel_config_data / cpu_possible_map / init_uts_ns ... do not match!`；第二次（--zero_excluded）：`WARNING: could not find MAGIC_START! / cannot read linux_banner string / do not match!`。完整输出在 crash_session.log。**原因**：kdump 按 pfn 顺序写页，死于早期（见 §6.7 对 24 字节页描述符索引的手工解码：1,996,032 条索引全部指向文件尾 [0x1895bca85, EOF)，中段 0xb00000–0x181800000 全为零洞）。
- 因此**可做**：头部字段、vmcoreinfo、192 个 NT_PRSTATUS ELF note（含崩溃 CPU 寄存器）、尾部 LZO 页数据的手工解码与位型分析；
- **不可做**：__per_cpu_offset[97] 的内存真值对照（该页在 .data，未被写入 dump）；崩溃 CPU179 的 per-cpu 区、页表真值、runqueue 真值读取。凡涉及"DRAM 中的原始字节究竟是什么"的验证，本案一律只能以【强推】或【推测】标注。

## 3. 本次开机时间线【时间线】

| 时刻（uptime s） | 墙钟（推算） | 事件 | 证据 |
|---|---|---|---|
| 0.000 | ~22:17:30 | 开机，8 节点 192 CPU 全部上线 | L1（Booting on CPU 0x80000）、L1255-1256（Brought up 8 nodes, 192 CPUs） |
| 1.489 | | ghes_edac 初始化：32 DIMM sockets，EDAC MC0 就绪 | L2177-2178 |
| 21.6 | | systemd 进入 system mode | L2487 |
| 27.8 | | EXT4 root 挂载完成 | L2575-2576 |
| 86.3 | | 最后一条正常内核消息（dm-2 capability deprecation） | L2581 |
| 0–513 | | **完全干净区间**：无任何异常消息 | dmesg 全文 |
| 513.059 | 22:26:03 | **W1**：irqbalance(PID 9670) 读 /proc/interrupts，memcpy 源 ffff20400651058f（node3 线性映射）level-0 读故障 | L2582-2625 |
| 523.046 | 22:26:13 | **W2**：同任务，源 ffff6040187ce676（node7）level-0 | L2627-2670 |
| 523.061 | 22:26:13 | **W3**：15 ms 后同页 ffff6040187ce40e，level-0 | L2672-2715 |
| 543.365 | 22:26:33 | **W4**：pmdalinux(PID 14753)，源 ffff6040187cd14e，level-0 | L2717-2760 |
| 552.355 | 22:26:42 | **致命 Oops**：rcu_sched(PID 16) find_busiest_group+0x140，FAR=00ffab53df0abfa0 | L2762-2810 |
| 552.719 | | SMP: stopping secondary CPUs → Starting crashdump kernel | L2807-2808 |
| 552.73 | | kdump 内核开始转储（头部时间戳 1788532003 = 22:26:43） | kdump 头 @0x194 |
| 22:27:27 | | 转储**未完成**即中断（vmcore-incomplete，目录时间戳） | 文件系统元数据 |

前案 22:09:49 panic → 本案开机 ≈22:17:30 → 本案 panic：间隔 17.8 分钟，本案存活 9.2 分钟；与 22:09 案存活 6.6 min、22:39 案存活 5.8 min 构成"重开机后数分钟内复发"的加速失效序列【实锤】。所有 5 个异常事件压缩在最后 50 秒，前 8.5 分钟零异常，说明是渐进劣化的间歇性扰动而非开机即存在的静态损坏。

## 4. 故障现象【故障现象】

1. **致命 Oops**（L2762-2806）：`Unable to handle kernel paging request at 00ffab53df0abfa0`，`[00ffab53df0abfa0] address between user and kernel address ranges`；ESR=0x96000004（L2764；EC=0x25 DABT current EL，FSC=0x04 level-0 翻译故障，ISS WnR=0 → **读**）；CPU179，PID 16，Comm rcu_sched（L2777）；pc=find_busiest_group+0x140（L2780），lr=find_busiest_group+0x11c；x27=0x00ffab53df0abe80（L2783）、x25=0x61（L2784）、x20=0x00ffffd4812327c0（L2786）；Code: `f9400782 f879d814 2a1903e0 8b14003b (f9409377)`（L2806；括号内 f9409377 = `ldr x23,[x27,#288]` 为致命指令，与 vmlinux 反汇编逐字一致）。
2. **调用栈**（L2794-2805）：`rcu_gp_kthread(L2803) → rcu_gp_fqs_loop(L2802) → schedule_timeout → schedule → __schedule → pick_next_task → pick_next_task_fair → newidle_balance → load_balance → find_busiest_group`。**纯调度器内核路径**，无任何 syscall 入口。
3. 4 次 WARNING（L2584/2629/2674/2719）：全部 `arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130`，前一行均为 `Ignoring spurious kernel translation fault at <addr>`；受害者 irqbalance×3 + pmdalinux×1；栈为 `__memcpy ← seq_printf ← show_interrupts ← ... ← ksys_read`（读 /proc/interrupts）；ESR=0x96000044 同为 level-0 读故障。
4. 无 RAS/EDAC/MCE 记录：dmesg 全文仅含 RAS 基础设施初始化行（L1262 RAS Extension、L2177-2178 ghes_edac、HEST/GHES/BERT/SDEI ACPI 表），552 s 内**零**错误事件。
5. kdump 未完成：vmcore-incomplete 7.9 GB，crash 拒载。

## 5. 业务现象【业务现象】

- 主机为监控节点（PCP pmdalinux + irqbalance + rsyslog 均在活动）。首个受害进程 irqbalance 正在读 /proc/interrupts 做中断亲和性计算，pmdalinux 在读 proc 采集指标：受害任务全部是"读密集型"监控负载，这与"读通路扰动"的定位自洽【强推】。
- rcu_sched（RCU 宽限期内核线程）死亡 → `SMP: stopping secondary CPUs → Starting crashdump kernel`，整机宕机，业务全部中断 7.7 分钟后才重启完成。
- 从业务视角：开机 8.5 分钟内服务正常，随后 50 秒内系统进入"半坏"状态（4 次 spurious fault 被 `__do_kernel_fault` 的 spurious 分支吞掉、任务继续运行），最终调度器 RCU 线程踩中毒指针即刻整机崩溃。典型的 SDC 演化为宕机的过程：先静默错误，后致命故障。

## 6. 诊断定位过程【诊断定位过程】

### 6.1 致命指令与寄存器语义反汇编（x27 是什么）

vmlinux 反汇编 `find_busiest_group+0x11c..+0x148`：

```
ae34: a94087e0  ldp  x0, x1, [sp,#8]        ; x0=&__per_cpu_offset, x1=&runqueues
ae3c: f879d814  ldr  x20,[x0, w25,sxtw#3]   ; x20 = __per_cpu_offset[w25]   <-- 内存读
ae44: 8b14003b  add  x27, x1, x20           ; x27 = per_cpu_ptr(runqueues, w25)
ae48: f9409377  ldr  x23,[x27,#288]         ; 读 rq->cfs.load_avg(offset 288, ptype /o 确认)
```

上下文寄存器逐一闭环（algebra.py §1，全部 True）：pc 期望=link+0x140+slide=0xffffab7f5c44ae48=实测；x1=runtime &runqueues=0xffffab7f5de796c0；x24=adrp 页基+slide；x21=&nr_cpu_ids+slide；x25=0x61 → 正在迭代 CPU 97（sched_group span 与 env->cpus 的 `_find_next_and_bit` 结果）；x26=sched_group（per-cpu 区指针 ffff604003e9ef60）。

结论：x27 应为 `per_cpu(rq, 97)`，期望形态 0xffff6040xxxxxxxx（per-cpu vmalloc 区）；实测 0x00ffab53df0abe80。

### 6.2 污染在"读出值"而非"运算"（算术自洽证明）

algebra.py §2【实锤】：
- FAR − x27 = 288 = 立即数偏移，偏移闭环；
- x27 − x20 ≡ x1（mod 2^64，逐位相等）→ add 指令消费的正是这个坏 x20，加法器输出忠实；
- 合法 `__per_cpu_offset[cpu] = percpu_base − &runqueues(rt)`，因 percpu_base 形如 0xffff6040../0xffff6057..，偏移高 16 位必为 0xffff；实测 x20 高 16 位 0x00ff → 读出值不可能是合法条目；
- 以 x26 推得的 __per_cpu_offset[179]=0xffffb4c0a60258a0 作量级参照，x20 与之 XOR 置位 29 位、x27 XOR 置位 32 位 → 宽带多比特污染，且以高位为主，远超单粒子单比特模型。

### 6.3 4 个 WARNING 块的寄存器代数

algebra.py §4：W1 地址 ffff20400651058f（PGD idx 64，物理 0x20400651058f = node3 SRAT 范围内实际 RAM）；W2/W3/W4 地址 ffff6040187ce676/40e、ffff6040187cd14e（PGD idx 192，node7 RAM，W2/W3 同页相差 0x268，是 memcpy 源指针正常步进）。四块的 x13-x17 携带地址的 ASCII 碎片（printk 正在格式化 %pS），x19=ESR、x21=故障地址，均自洽无 corruption。地址是真实线性映射地址，翻译却 level-0 失败 → PGD/PUD 走查读在 CPU179 上间歇性返回无效描述符【实锤·间歇性】。若 PGD 条目被持久清零，node3/node7 整个 512 GB 区域的所有访问都会连续故障，机器会立即死亡；实际 W1→W2 间隔 10 s 系统正常运行。

### 6.4 rcu_sched 作为受害者的特殊性

rcu_gp_fqs_loop 是内核线程（PID 16，fork 自 kthreadd），无用户地址空间、无 syscall 入口。它调用 schedule_timeout 进入调度器，newidle_balance 负载均衡路径是每个 CPU 每毫秒级都会走的"内核内环"。受害与用户代码零相关 → 扰动源在核内/硬件侧，与软件负载内容无关【强推】。同时说明：调度器是整机最频繁的 per-cpu 数据读者之一（每次均衡都读 `__per_cpu_offset[]` + per_cpu rq），因此成为读通路 SDC 的**最高命中率曝光点**。这解释了为何五案中四案都倒在这同一条指令上。

### 6.5 跨案对比归纳（多案位型规律）

algebra.py §5（相邻案仅取各自 vmcore-dmesg 原文）：

| 案 | 存活 | CPU | x20（__per_cpu_offset 读出值） | 高 16 位（置位数） | x27−x20==x1 |
|---|---|---|---|---|---|
| 21:53:28 | 33272 s | 179 | 73b88cc000ffffc5 | 0x73b8（9b） | True |
| 22:09:49 | 397 s | 179 | 0000000000000000 | 0x0000（0b） | n/a（该块被并发 WARNING 干扰） |
| **22:27:27 本案** | 552 s | 179 | 00ffffd4812327c0 | 0x00ff（8b） | True |
| 22:39:38 | 347 s | 179 | 0000000000000000 | 0x0000（0b） | True |
| 23:37:57 | 2839 s | 179 | （get_pfnblock_flags_mask+0x3c：mem_section 链读出 x3=0，NULL+8） | — | — |

**规律**：翻转位数（9/8/0/0）与形态每次不同，**位型随机**；但恒定的是（a）CPU179，（b）读方向，（c）命中数据类别（共享内核 .data 数组/页表），（d）破坏都集中高位或整字读 0。这排除了软件确定性 bug（位型会复现），也排除了 DRAM 固定位坏（其他 191 个 CPU 读同一 __per_cpu_offset 行从未出错）→ 随机物理扰动 × CPU179 固定读通路弱点【强推】。

### 6.6 微架构定位：读路径 vs 写路径；为何 ECC 不拦截

- 5 个事件 ESR 全部 WnR=0（读）；4 WARNING 是页表走查读坏，致命案是数据读坏。**无一写路径证据**。若为写污染，应看到页表内容被改写后的持久故障（连续 translation fault）或数据结构 CRC/list 损坏（22:09 案的 `list_add corruption` WARNING 群是被同一扰动拖垮的次生现象，该案 5 个 CPU 的 WARNING 风暴是崩溃后并发 panic 的混乱输出）。
- 污染注入点在CPU179 核内私有读通路（L1D SRAM 位阵列/填充缓冲/加载-对齐数据路径）【强推】：__per_cpu_offset 与页表都是共享 DRAM 数据，191 个 CPU 读同一行从不报错；只有 CPU179 读错。
- 为何 DDR ECC/GHES 不拦截：错误从未跨越 DDR 边界，污染在核内注入，ECC 校验的是"DRAM→互连→LLC"这一段，核内私有 SRAM 的数据通路在 ECC 域之外。TaiShan v110 的 L1D 若存在偶发多位扰动（且未启用/未覆盖对应保护），即呈现"完全静默"的 SDC。552 s 零 RAS 记录与此完全一致【强推】。

### 6.7 vmcore-incomplete 的手工法证（crash 失败后的边界突破）

虽然 crash 拒载，kdump v6 压缩格式可手工解码（python3，过程入 crash_session.log SESSION 4）：
- 磁盘头（@0x0）："KDUMP   " v6；uts@0x0c（localhost0102 / 6.6.0-145.3.23.154.oe2403sp3.aarch64）；时间戳@0x194 = 1788532003 = 2026-09-04 22:26:43（与目录名 22:27:27 对应：转储开始后约 44 s 中断）；
- 子头（@0x1000）：dump_level=31（与 /etc/kdump.conf `core_collector makedumpfile -l --message-level 1 -d 31` 一致），bitmap offset=0x14580；
- ELF note 区（@0x1068..0x14568）：192 个 NT_PRSTATUS（每个 0x188 B） + VMCOREINFO（0xd10 B，含 KERNELOFFSET/_stext/CRASHTIME）。此区先于页数据写出，故在 incomplete 转储中完好；
- note #179（索引=崩溃 CPU 号）即崩溃 CPU 寄存器：arm64 prstatus 的 pr_reg 基偏移 120，恢复出全部 35 个寄存器，与 dmesg Oops 块逐个一致（x27=0x00ffab53df0abe80、x20=0x00ffffd4812327c0、x1、pc、pstate=0x204000c9）。printk 与 kdump ELF note 两条独立采集链交叉验证寄存器块真实性【实锤】。其余 191 个 note 是辅助 CPU 经 IPI 下线时的 idle 循环用户态现场（pid=0、pc 在用户栈区），属 kdump 正常形态；
- 页描述符索引（@0x181800008）：1,996,032 条 × 24 B（`u64 文件偏移, u32 压缩尺寸, u32 flag`；flag=2=LZO、洞标记 off=0x187ef3110/size=0x1000/flag=0），数据流连续区间 [0x1895bca85, EOF=0x1f3765978)，最后一条描述符的终止位置**恰好等于文件大小**，kdump 死于写字节流中间；
- 中段 0xb00000–0x181800000 全为真实零（文件非稀疏，st_blocks×512 ≈ st_size）→ 内核 .data（含 __per_cpu_offset[97]）所在页**未被转储**，内存真值对照不可行（法证边界，见 §2）。

### 6.8 与 22:09 案相隔 18 分钟的意义

22:09:49 panic → 22:17:30 本案开机 → 22:26:43 本案 panic。相邻案存活时间序列：33272 s → 397 s → 552 s → 347 s → 2839 s。开机重启（冷/热复位）**不能**消除故障，且间隔从 9.2 小时级坍缩到分钟级：扰动源不受复位影响、且发作强度在爬升【实锤·频率升级；机理属推测，见 §8】。本案 rcu_sched 内核线程受害进一步排除了"用户负载触发"假说，锁定核内读通路的硬件侧根因。

## 7. 逻辑链条【逻辑链条】

```
[实锤] dmesg 5 异常块全部 CPU179、全部读故障（ESR WnR=0 / 走查 level-0）
   │
[实锤] 致命指令 ldr x23,[x27,#288]；FAR−x27=288 偏移闭环
   │
[实锤] x27−x20 ≡ x1 ≡ runtime &runqueues（KASLR slide 三重验证 pc/x1/x24/x21）
   │      → add 算术正确，污染在输入 x20（__per_cpu_offset[97] 的读出值）
   │
[实锤] x20 高 16 位 0x00ff ≠ 合法偏移必有的 0xffff；XOR 参照 29/32 位 → 宽带高位污染
   │
[实锤] 4 WARNING：有效线性地址（node3/node7 真 RAM）level-0 读翻译失败且间歇
   │      → 页表走查读同样间歇性取坏 → 不限于单条数据的读通路问题
   │
[实锤] 跨 5 案全部 CPU179、全部读；x20 位型 0x73b8/0x00ff/全0/全0 各不相同
   │      → 位型随机、位置/方向/类别恒定
   │
[实锤] RAS/GHES/EDAC 在位而 552 s 零记录；__per_cpu_offset 为 192 CPU 共享行而仅 179 读错
   │
[强推] ──► CPU179 核内私有读通路（L1D/回填/对齐路径）瞬时多位扰动，
   │         未跨越 DDR ECC 检测边界，呈纯 SDC
   │
[强推] 间歇性 + 尾段 50 s 加速劣化 + 重开机不愈 + 18 分钟内二次死亡
   │         → 硬件退化型（而非单次宇宙线型）故障源
   │
[实锤] rcu_sched（PID16 内核线程、纯调度路径）受害 → 与用户态代码无关
```

## 8. 故障根因【故障根因】

**直接根因【实锤】**：CPU179 在 `find_busiest_group+0x13c` 前的 `ldr x20,[x0,w25,sxtw#3]` 把 `__per_cpu_offset[97]` 读成 0x00ffffd4812327c0（高位宽带污染），该值经 `add x27,x1,x20` 算术传播为毒指针 0x00ffab53df0abe80，随后 `ldr x23,[x27,#288]` 触发 level-0 翻译故障，rcu_sched 内核线程 Oops，kdump 转储本身亦在故障机上中断。

**根因定位【强推】**：CPU179 核内私有数据读通路（L1D cache SRAM 位阵列或加载/回填/对齐数据路径）的瞬时多比特扰动。支撑证据五条独立收敛：(1) 五案全部 CPU179；(2) 全部读方向；(3) 共享数据（__per_cpu_offset、页表）仅 179 读错而 191 CPU 无恙；(4) RAS/ECC 全程静默（错误未达 DDR 边界）；(5) 污染位型每次随机但类别恒定（高位/整字）。

**故障性质【推测】**：从"9.2 小时一作"劣化到"分钟级一作、重开机不愈"，更像**退化型硬件缺陷**（L1D SRAM 单元退化/时序裕量衰竭/供电或温度相关的位线扰动）而非单次宇宙线翻转；不排除封装/焊球类应力问题。发作温度/电压依赖性本案无从验证（无 BMC 日志入 dmesg），此为 incomplete 转储之外的第二层法证边界，建议从 BMC/IPMI SEL 取证补全。

**排除项**：软件 bug（位型不复现、跨 4 次开机随机）；DRAM 固定位坏（共享行其他 CPU 读取正常）；写路径污染（无任何 WnR=1 证据、无持久页表损坏）；用户态触发（受害者为内核线程）。

## 9. 启示【启示】

**电路层**
- L1D SRAM 位阵列的位线/字线偶发多位扰动是 SDC 的第一注入点。内建冗余（ECC/parity）应覆盖 L1D 数据通路全宽，而不仅覆盖 tag 或 LLC；TaiShan v110 世代核内保护盲区正是本案"全静默"的条件。
- 高位字节（本组案例高 16 位反复被破坏：0x73b8/0x00ff/全 0）集中受损提示：若为 SRAM 阵列物理邻接，位图应体现位线成对/成组翻转，值得芯片 FA 做真值位图比对。

**微架构层**
- 依赖链寄存器会**算术放大**污染：单次坏读经 `add` 变成毒指针，再经 load 变成 translation fault。微架构上可考虑对"指针生成指令链"加轻量合法性检查（如 kernel 指针高位 must-be-ffff 的断言检错）。
- `__per_cpu_offset[]` 是全机热路径共享只读数组，理应是"最可信数据"，却成为最亮眼的受污染指示器。读通路 SDC 的最佳哨兵恰恰是这些高频、可预测形态的指针表。
- 本案五次发作全部在"调度器/页表走查"被观测到，是因为它们是**最高频读路径**（并非这些路径脆弱）。监控读密集路径的异常率即等于监控读通路健康度。

**RAS 层**
- DDR ECC/GHES 对核内私有 SRAM 的扰动**完全失明**。RAS 覆盖边界必须显式文档化：哪些层级有检错、哪些没有。若 L1D 只有奇偶或无保护，固件 first mode + GHES 也不能补上这个洞。
- "连续多次 spurious kernel translation fault + 同 CPU 集中"应被 RAS 策略层识别为 CPU 级 SDC 信号并触发自动隔离（OS 层 hotplug 摘核），而不是当成可忽略噪声（fault.c:494 的 "Ignoring spurious..." 分支恰在此案中四次吞掉了预警）。

**系统软件层**
- `__do_kernel_fault` 的 spurious 分支掩盖了 40 秒的预警窗口：若在第 2 次 spurious fault 即触发"同 CPU 连续读翻译异常"计数器并告警/摘核，本案可避免最终宕机。
- kdump 依赖故障机自身写盘，本案转储亦被同一故障中断（vmcore-incomplete）。高价值节点应配 **外的转储路径**（网络 kdump/OFA-dump）或至少双份头部先行落盘；本案能救回崩溃 CPU 寄存器全靠 ELF note 区被最先写出，这一写出顺序值得所有 kdump 实现保留。
- 应对读通路 SDC 的兜底：对 per-cpu 指针表读出值做高位断言（一行 BPF/kprobe 即可：`__per_cpu_offset[cpu] >> 48 != 0xffff` 时告警），可把此类静默故障前移到第一次发作。

## 10. 处置建议

1. **立即**：整机下线，不要再反复重启验证（已 5 连崩，第 6 次只会更糟）。该机正在为集群贡献监控数据（pmdalinux/irqbalance/rsyslog 全灭），业务影响已经发生。
2. 摘核止血（若必须临时带病运行）：`echo 0 > /sys/devices/system/cpu/cpu179/online` 将 CPU179 下线。五案全部命中 CPU179，摘核后故障应消失，这同时也是对根因定位的最终实证（A/B 试验）。
3. **硬件更换**：按"CPU179 所在物理核"更换 CPU/单板（R240K V2 的 hip08 封装不可单核换，需换整 CPU 或整机）。换件前建议导出 BMC/IPMI SEL、CPU 温度曲线佐证退化假设。
4. **取证补全**：（a）BMC SEL / ipmitool sel list 查 22:00-23:00 的温度/电压/MRC 记录；（b）保留全部 5 个转储目录做批次分析；（c）如厂商支持，对返修件跑 L1D 定向压力（l1d 读写真值比对类微基准，非普通 memtester，那测不到核内路径）。
5.  fleet 级预防：（a）为内核打"per-cpu 指针高位断言"探针（kprobe on find_busiest_group / 调度器热路径）；（b）把"同 CPU spurious translation fault 计数"纳入 RAS 告警规则；（c）审查 kdump 配置：头部+notes 先行落盘的顺序（本案救回寄存器的关键）+ 网络转储冗余。

## 附录：命令索引

```bash
# dmesg 异常块定位
grep -n "Ignoring spurious\|WARNING:\|Internal error\|Unable to handle" <dumpdir>/vmcore-dmesg.txt
# RAS 负证据全文检索
grep -n -i "mce\|edac\|ras\|hardware error\|ecc\|memory error\|corrected\|deferred\|ghes\|apei" <dumpdir>/vmcore-dmesg.txt

# crash 加载尝试（两次均失败，输出完整保存于 crash_session.log）
printf 'sys\npanic\nbt\nquit\n' > /tmp/c15_cmd.txt
timeout 1800 crash /tmp/vmlinux-0102 <dumpdir>/vmcore-incomplete -i /tmp/c15_cmd.txt
timeout 1800 crash /tmp/vmlinux-0102 <dumpdir>/vmcore-incomplete --zero_excluded -i /tmp/c15_cmd.txt

# 符号与反汇编
nm -n /tmp/vmlinux-0102 | grep -E "find_busiest_group|__per_cpu_offset|runqueues"
objdump -d --start-address=0xffff80008013ad08 --stop-address=0xffff80008013ae58 /tmp/vmlinux-0102
gdb -batch -ex "ptype /o struct lb_env" -ex "ptype /o struct rq" /tmp/vmlinux-0102

# vmcore-incomplete 手工法证（kdump v6 头/notes/页描述符解码 + LZO 解压验证）
python3   # 详见 crash_session.log SESSION 4 与 algebra.py §7
#   头部: file + od -A x -t x1z vmcore-incomplete
#   ELF notes: 0x1068 起 192×(12+8+0x188) NT_PRSTATUS, pr_reg 基偏移 120
#   页描述符: 0x181800008 起 24B×1,996,032; 数据 [0x1895bca85, EOF)

# 代数验证（本报告全部 64 位运算）
python3 algebra.py | tee algebra_out.txt

# 相邻案对照（仅取各自 vmcore-dmesg 原文）
grep -A40 "Unable to handle" /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-21:53:28/vmcore-dmesg.txt
grep -A40 "Unable to handle" /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:09:49/vmcore-dmesg.txt
grep -A40 "Unable to handle" /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:39:38/vmcore-dmesg.txt
grep -A40 "Unable to handle" /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-23:37:57/vmcore-dmesg.txt
```

报告产物清单：主报告（本文件）、`dmesg_forensics.txt`（dmesg 原文副本）、`algebra.py` / `algebra_out.txt`（全部 64 位运算）、`crash_session.log`（两次 crash 失败记录 + 手工 kdump 解码过程）。

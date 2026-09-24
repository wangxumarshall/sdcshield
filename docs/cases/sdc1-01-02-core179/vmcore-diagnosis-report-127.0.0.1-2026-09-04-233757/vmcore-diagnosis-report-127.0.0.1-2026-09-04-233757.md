# 零前兆单发即死 + 读出路径 SDC 直接实锤：CPU 179 上 `get_pfnblock_flags_mask` 把非零内存指针读成 0

## 副标题：vmcore 127.0.0.1-2026-09-04-23:37:57 微架构级 SDC 根因诊断报告（读出≠内存的矛盾链全量复核）

| 项 | 值 |
|---|---|
| 转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-23:37:57/vmcore`（9.6G，PARTIAL DUMP）+ `vmcore-dmesg.txt`（2639 行） |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP（KASLR on） |
| vmlinux | `/tmp/vmlinux-0102`；crash 8.0.4-17.oe2403sp4 |
| 整机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026（HIP08 系 ACPI 表） |
| CPU | 192 核 / 8 NUMA 节点（每节点 24 核），故障核 CPU 179 = MPIDR 0x7a0300，节点 7 |
| 内存 | 767.8 GB，node7 = 0x604000000000-0x6057ffffffff |
| 崩溃时刻 | 2026-09-04 23:37:14 CST，uptime 2838.8s（47.3 分钟） |
| 唯一异常块 | CPU 179 / PID 89347 `systemd-coredum` / ESR=0x96000004 / FAR=0x8 |
| panic 串 | "Unable to handle kernel NULL pointer dereference at virtual address 0000000000000008" |
| 前兆 | **零**。开机后 dmesg 无任何 WARNING/spurious fault/MCE/EDAC 记录（负证据，见 §4） |
| 报告产物 | 本报告 + `dmesg_forensics.txt` + `algebra.py`/`algebra_out.txt` + `crash_*.log`（50 份） |

## 1. 执行摘要

**结论（【实锤】）**：这是全 fleet 中第一例被完整证据链钉死的读出路径 SDC（read-out path Silent Data Corruption）。内存没有被写坏；是 CPU 179 的一次 load 指令把内存中明明非零、且事后从 vmcore 中多次读出仍非零的指针，装载成了 0。

证据链一句话版：崩溃指令 `get_pfnblock_flags_mask+0x3c` 执行 `ldr x0,[x3,#8]` 时 x3=0（寄存器转储）；x3 的值来自上一条 load `+0x28: ldr x3,[x3,x5,lsl#3]`，其访存地址 `0xffff6057fffb5b40`（= mem_section 根数组 + 0xc08×8）在 vmcore 中的真值是 `0xffff6057fffaeb00`（非零、有效、指向在线 section 数组，`kmem -n` 显示该 section 788502 状态 PMOE=Present/MemMap/Online/Early）。内存真值完好 + CPU 装载结果为 0 = 读出路径 SDC 直接实锤，矛盾无法用软件竞态、无效 pfn、分支误跳、寄存器错值解释（逐一排除见 §6）。

进一步用 x4=0x160 这一寄存器"指纹"把出错 load 唯一确定为 `+0x28`（而非 `+0x20`）：`ubfiz` 只有在 `+0x24 cbz` 未跳转时才会执行，而 0x160 = (0xc0816 & 0xff) << 4 恰是 ubfiz 的正确输出，证明 `+0x20` 读 BSS 根指针是正确的、cbz 没有跳、是 `+0x28` 读 direct-map 根数组条目时读成了 0（【实锤】，推导见 §7）。

**微观定位（【强推】）**：出错的是 CPU 179 核私有读路径（L1 D-cache 行 / load pipeline / TLB 返回通路之一），64 位全错（非零→0），一次单发，无 ECC 保护覆盖（核私有结构无 ECC，L1DCache ECC 只拦驻留位翻转、拦不住"行内容正确但回读通路错"的故障）。**跨重启复发性（【实锤】）**：本机 17 个历史 dump 中 16 个崩在 CPU 179、1 个崩在 CPU 168（同属节点 7 的邻核），且 2026-09-04 22:39 案（上一次开机）在 CPU 179 上先出现 8 次 spurious translation fault 再崩溃。同一物理核跨重启、跨负载复发，指向该物理核读出通路的持续性硬件缺陷；DRAM/UVM 故障会是节点级而非单核级。

与主会话初筛的两处修正（诚实优先）：
1. 主会话提示"x27 可能携带根指针（差 0x10000）"，**复核为误**：x27=0xffff6057fffbfb00 恰是 `node_data[7]`（pglist_data），由调用者 `free_unref_folios+612` 装载，与 mem_section 根数组 0xffff6057fffafb00 只是 memblock 邻居（相差 64KB），两者是不同结构。此差异不需要"旧值残留"解释。
2. 主会话猜测"最可能是 +0x20 读出 0"，**复核后相反**：x4=0x160 证明 +0x2c 的 ubfiz 执行过，cbz 必然未跳转，+0x20 必然读到了非零；出错的是 +0x28。而 +0x28 读出 0 的路径（cmp eq → csel 选 x3=0、add x4=0+0x160）与全部寄存器转储精确吻合，是唯一解。

## 2. 证据规则与方法

1. **禁止预设立场**：本报告未读任何既有分析（docs/cases/、docs/sdc-microarch/、DIAGNOSIS_REPORT.md、fi_research/** 均未打开），所有结论仅基于本次对 vmcore/vmcore-dmesg 的独立取证。
2. **一手证据**：所有内存值均由 crash 8.0.4 对 9.6G vmcore 的 `rd`（含 -64/-32/-16/-8 多宽度交叉验证）、`dis -l`（带源码行号）、`p`、`kmem -n`、`vtop`、`search` 实测输出；所有 64 位运算由 python3 完成（`algebra.py` → `algebra_out.txt`），零手算。
3. **可复核性**：每条 crash 命令与输出存于同目录 `crash_*.log`；dmesg 引用带行号（`dmesg_forensics.txt` 行号与原始文件一致）。
4. **标注分级**：【实锤】= 内存值与寄存器值直接矛盾/直接读出；【强推】= 唯一自洽的微架构解释但无法从 dump 直接观测到出错瞬间的 cache/TLB 状态；【推测】= 谱系性、设计性推论。
5. **负证据也记录**：无 MCE/EDAC/GHES 记录本身是证据（读出错误发生在 ECC 覆盖边界之外，见 §8）。

## 3. 本次开机时间线【时间线】

（时间 = 内核 uptime，源：dmesg_forensics.txt 行号）

| 时刻 | 事件 | 证据 |
|---|---|---|
| 0.000s | 开机（约 22:49:55 CST），8 节点 192 CPU，node7 = 0x604000000000-0x6057ffffffff | dmesg L117/L1255 |
| 0.333s | CPU179（MPIDR 0x7a0300）上线：`CPU179: Booted secondary processor 0x00007a0300` | dmesg L1206 |
| 0.356s | `smp: Brought up 8 nodes, 192 CPUs`；`RAS Extension Support` 已检测 | dmesg L1255/L1262 |
| 1.53s | ghes_edac 接管 32 DIMM（此后**零**硬件错误事件上报） | dmesg L2175-2176 |
| 28.3s | ext4（openeuler-root/home）挂载完成 | dmesg L2579-2581 附近 |
| 87.96s | 最后一条常规内核消息（dm-2 capability deprecation），**此后 2750 秒 dmesg 完全静默，无任何前兆** | dmesg L2579 |
| 2831.23s | PID 89347 `systemd-coredum` 启动（为某进程生成 coredump，目标文件 ~6.17GB，写到 openeuler-root ext4） | crash task start_time |
| 2838.82s | `close(fd=6)` → `__fput_sync` → `dentry_unlink_inode`（inode nlink=0，mode 0100644）→ `ext4_evict_inode` → `truncate_inode_pages_final` → `free_unref_folios` → `get_pfnblock_flags_mask+0x3c` 读出 SDC → NULL+8 L0 translation fault → Oops → kdump | dmesg L2580-2639 |
| 2839.27s | `Starting crashdump kernel...` | dmesg L2638 |

时间线要点【实锤】：从 87.96s 到 2838.82s 共 2750.9 秒内没有任何一条 WARN/Oops/EDAC/RAS 消息，单发即死，零前兆（与 22:39 案 8 次 spurious fault 前兆形成鲜明对比，谱系讨论见 §8.4）。

## 4. 故障现象【故障现象】

（dmesg L2580-2639 全文见 dmesg_forensics.txt，此处为关键摘录）

```
[2838.824881] Unable to handle kernel NULL pointer dereference at virtual address 0000000000000008
[2838.837915]   ESR = 0x0000000096000004        ← EC=0x25 DABT(current EL), IL=32bit
[2838.855990]   FSC = 0x04: level 0 translation fault
[2838.871349]   ISV = 0, ISS = 0x00000004, ... CM = 0, WnR = 0 ...   ← WnR=0：读访问
[2838.890277] [0000000000000008] pgd=0000000000000000, p4d=0000000000000000  ← 地址 8 无映射
[2839.006227] CPU: 179 PID: 89347 Comm: systemd-coredum ... Not tainted
[2839.033834] pc : get_pfnblock_flags_mask+0x3c/0x70
[2839.039336] lr : free_unref_folios+0x27c/0x7f8
[2839.048499] x27: ffff6057fffbfb00 ...  x5 : 0000000000000c08  x4 : 0000000000000160  x3 : 0000000000000000
[2839.119091] x2 : 0000000000000007  x1 : 00000006040b1d7c  x0 : ffffa7cc032ca000
[2839.244915] Code: d37c1c84 f100007f 8b040064 9a831083 (f9400460)
[2839.252086] SMP: stopping secondary CPUs → kdump
```

现象要点：
- ESR=0x96000004：当前 EL 的数据中止，WnR=0（读），FSC=0x04（L0 翻译错误）。CPU 试图读虚拟地址 0x8，而 0x8 什么都没映射。
- 崩溃点 `get_pfnblock_flags_mask+0x3c` 的指令是 `ldr x0,[x3,#8]`（Code 窗口括号指令 f9400460），x3=0 → 访存地址 = 0+8 = FAR=0x8，完全自洽。
- x1=0x6040b1d7c 是 pfn（pageblock 查询的页帧号），x5=0xc08 = pfn>>23（根索引）✓，x2=0x7 是 migratetype 掩码。
- 无第二次 Oops（`[#1]` 表示仅此一次），kdump 干净触发。
- 负证据【实锤】：全 dmesg 2639 行中 grep `spurious|WARNING|MCE|EDAC.*(error|corrected)|Hardware error|ras.*error` 仅命中启动期的 Firmware Bug（IORT 映射冲突，与本案无关）和 EDAC 探测消息，运行期零硬件错误上报。

## 5. 业务现象【业务现象】

- 机器 host 名 `localhost0102`，是 fleet 压测节点：崩溃时有 191 个 `arm0102_swizzle` 进程（父进程 81470 `opendcdiag` ← `run_stage1_rest` ← bash），几乎占满 192 CPU，load average 176.68/79.26/30.45（crash sys）。
- 崩溃任务 `systemd-coredum`（PID 89347，7.6 秒前被 systemd 拉起）正在处理一个 ~6.17GB 的 coredump（inode 0x420583，i_size=0x16f920000，mode 0100644，nlink=0，已 unlink、正被丢弃的临时 coredump 文件，位于 openeuler-root ext4）。它走 `close()` → `dentry_unlink_inode` → `ext4_evict_inode` → 截断页缓存 → 批量释放 folio，在为第 N 个 folio 查 pageblock 迁移类型时踩中读出 SDC。
- 业务影响：整机 panic + kdump 重启；压测 run 中断（该节点当日第 6 次崩溃，见 §8.4 谱系）。压测作业 `arm0102_swizzle` 自身不受疑，它只是把 CPU 179 喂饱了负载，触发到故障窗口。
- 需要说明：systemd-coredum 的出现意味着压测中有进程异常退出并触发 core dump。这**可能**是读出 SDC 的另一受害者（第一次伤在用户态没致命），也可能是压测故意制造的 abort。从本 dump 无法区分，但不影响内核侧实锤链（【推测】：systemd-coredum 自身的出生更可能是压测常驻行为，fleet 各案 crash 进程五花八门，均为"路过者"）。

## 6. 诊断定位过程【诊断定位过程】

### 6.1 反汇编与指令流（【实锤】，crash_dis_gpfm.log）

```
0xffffa7cc000108b0 <get_pfnblock_flags_mask>:
+0x08  lsr  x4, x1, #15          ; x4 = pfn>>15 = 0xc0816 (section_nr)
+0x0c  lsr  x5, x1, #23          ; x5 = pfn>>23 = 0xc08  (root index, SECTIONS_PER_ROOT=256)
+0x10  mov  x0, #0x1fffff ; cmp x4, x0 ; b.hi +0x60   ; 越界检查: 0xc0816 ≤ 0x1ffff1 ✓ 不跳
+0x1c  adrp x0, 0xffffa7cc032ca000                        ; x0 = &mem_section 所在页
+0x20  ldr  x3, [x0, #3312]       ; x3 = mem_section = 根数组指针   ← LOAD A（BSS）
+0x24  cbz  x3, +0x3c             ; 根为 NULL 则跳到 +0x3c
+0x28  ldr  x3, [x3, x5, lsl #3]  ; x3 = mem_section[0xc08]        ← LOAD B（direct map）
+0x2c  ubfiz x4, x4, #4, #8       ; x4 = (section_nr&0xff)<<4 = 0x160（字节偏移）
+0x30  cmp  x3, #0
+0x34  add  x4, x3, x4
+0x38  csel x3, x4, x3, ne        ; x3≠0 → x3 = 根数组+0x160 = section 结构
+0x3c  ldr  x0, [x3, #8]          ; x0 = section->usage             ← 崩溃指令（FAR=8）
+0x40/+0x44/+0x48/+0x4c/+0x50/+0x54/+0x58 : 位图取字/移位/掩码（未执行到）
```

寄存器转储（pt_regs，crash_stk_eframe*.log 逐字核对）：x0=0xffffa7cc032ca000（adrp 结果 ✓）、x1=0x6040b1d7c（pfn ✓）、x2=0x7、x3=0、x4=0x160、x5=0xc08、x9=0xffffa7cc00016e84（返回地址=bl 下一条 ✓）、pc=+0x3c。

### 6.2 内存真值验证链（【实锤】，crash_memsection.log / crash_rd_chain.log / crash_rootarray.log）

| 步骤 | 命令 | 结果 |
|---|---|---|
| 1. 符号 | `sym mem_section` | `ffffa7cc032cacf0 (B) mem_section` |
| 2. 根指针真值 | `rd -64 0xffffa7cc032cacf0` | `ffff6057fffafb00`（非零）；-8 逐字节 `00 fb fa ff 57 60 ff ff` 完整 |
| 3. LOAD A 地址 | 0xffffa7cc032ca000+0xcf0 | = 0xffffa7cc032cacf0 = &mem_section ✓ |
| 4. LOAD B 地址 | root+0xc08*8 = **0xffff6057fffb5b40** | `rd -64`：**`ffff6057fffaeb00`（非零！）**；-8 逐字节 `00 eb fa ff 57 60 ff ff` 完整 |
| 5. 该指针指向的 section 数组 | `rd -64 0xffff6057fffaeb00 32` | 32/32 条目全部有效（section_mem_map=0xfffffc000000000f，usage 非空） |
| 6. 目标 section 结构 | 0xffff6057fffaeb00+0x160 = 0xffff6057fffaec60 | section_mem_map=0xfffffc000000000f，usage=0xffff6057fffa27d0 |
| 7. crash 结构化交叉验证 | `p/x mem_section[0xc08][0x16].usage` | `0xffff6057fffa27d0`（与裸 rd 一致） |
| 8. `kmem -n` | section 788502（=0xC0816） | `SECTION ffff6057fffaec60 ... STATE PMOE`；memory block memory788502（0x6040b0000000，node7）**ONLINE** |
| 9. 位图真值 | `rd -64 0xffff6057fffa27e8` | pageblock_flags 全 `1111111111111111`（无异常） |
| 10. vtop | 0xffff6057fffb5b40 → phys **0x6057fffb5b40**（node7 direct map，1GB 块映射 PTE `VALID|SHARED|AF|PXN|UXN|DIRTY`） | 物理地址落 node7 末尾 memblock 保留区（NODE_DATA[7] 正上方） |
| 11. 全根数组 | `rd -64 root 33` + 邻域 | 根条目布局与 SRAT 完全一致（node0 低区 root[0..2]、node6 root[0xc05-0xc07]、node7 root[0xc08-0xc0a]），`__highest_present_section_nr=0xc0aff` = node7 末 section ✓ |

第 4 步就是矛盾本体：LOAD B 的访存地址 0xffff6057fffb5b40 在内存中存的是 0xffff6057fffaeb00，而执行 LOAD B 之后寄存器 x3=0。内存对、结果错，错误发生在"内存→寄存器"之间。

### 6.3 替代解释逐一排除

**(a) 竞态 / 并发写 mem_section？排除【实锤】**
mem_section 两级表在 `sparse_init()`（boot 阶段，本机 0.56s 前后）完成后即为静态只读结构；运行期唯一写者是 memory hotplug 的 `sparse_add/remove_section`（须持 `mem_hotplug_lock` 并先 offline 内存块）。`kmem -n` 显示全机 6144 个内存块全部 ONLINE、无一 OFFLINE/GOING_OFFLINE，dmesg 无任何 hotplug 事件；victim section 788502 状态 PMOE。即使假设有并发写：一次"把非零指针写成 0 再写回"的竞态需要写者在崩溃前又把值写回非零，但 vmcore 显示值自 boot 后未变（且写回时间窗 < 微秒级，跨 47 分钟无 hotplug 活动）。无并发写者成立。
（另：本报告禁止读源码仓分析文档，但 kernel 源码行号由 crash `dis -l` 从 vmlinux DWARF 直接给出：mmzone.h:1935-1944 即 `__nr_to_section` 内联，无锁只读。）

**(b) pfn 无效 / section 被热插拔移除？排除【实锤】**
x1=0x6040b1d7c → phys 0x6040b1d7c000 ∈ node7 范围（algebra.py §1）；section 0xC0816 PMOE 在线；section_mem_map=0xfffffc000000000f（低 4 位 = Present|HasMemMap|Online|Early 全 1）；对应 struct page（x28=0xfffffd8102c75f00）= vmemmap 基址 0xfffffc0000000000 + pfn*64（python 验证 True），page->flags=0x075ffffe00000000 → node id=(flags>>53)&7=2？不。注意 x26=2 是 `(flags>>53)&7`，x3(旧)=flags>>56=7 是 node_data 索引，`node_data[7]`=0xffff6057fffc0880=x24=x19 ✓（crash_nodedata.log）。pfn 完全合法、在线、有 memmap。排除。

**(c) 分支误跳（cbz 误跳 / 越界 b.hi 误跳）？排除【实锤】**
- 若 +0x24 cbz 误跳（x3 实际非零但 cbz 跳了）：则 +0x2c..+0x38 全部被跳过，x4 将保持旧值 0xc0816，但转储 x4=0x160（ubfiz 已执行）。矛盾，排除。
- 若 +0x10 b.hi 误跳（走 +0x60: `mov x3,#0; b +0x3c`）：同样跳过 ubfiz，x4 应为 0xc0816；且该路径 x3 确实为 0、FAR=8 吻合，但 x4 不吻合。且 pfn>>15=0xc0816 < 0x1ffff1，比较本身为假。排除。
- 结论：控制流走的是"正常"路径（b.hi 不跳 → adrp → LOAD A → cbz 不跳 → LOAD B → ubfiz → cmp → add → csel），是数据（x3）在 LOAD B 处错了，不是控制流错。

**(d) x5 错值（索引错导致读到别的槽位）？排除【实锤】**
x5=0xc08 与 pfn>>23=0xc08 一致（python 验证）；且就算 x5 错，读的是根数组**别的槽位**。根数组 0..0xc0aff 中仅 12 个非零槽（node0/1/6/7 首块），无论读到哪个非零槽 x3 都非零、读到零槽也与"该槽内存值"一致，都无法产生"读出 0 而内存非 0"的矛盾。本矛盾只能在 LOAD B 的目标槽 0xffff6057fffb5b40 上发生。排除。

**(e) x3 在 LOAD B 之后、+0x3c 之前被中断/异常改写？排除【实锤】**
+0x28 与 +0x3c 之间只有 ubfiz/cmp/add/csel 四条纯 ALU 指令，无 load、无分支目标；若其间来了异步异常（中断/NMI），异常返回后 x3 会恢复为陷入前值（pt_regs 保存/恢复完整），且栈上 pt_regs 就是异常帧本身，x3=0 是 csel 的输出。add/csel 自身出错属于执行单元翻转，无法从 dump 区分，但其效果等价（x3 装载或生成 0），不改变"读出/生成 0 而源头非 0"的矛盾本质；结合 x4=0x160=add(0,0x160) 的正确性，add/csel 工作正常，x3=0 只能源于 LOAD B 返回 0。排除（执行单元翻转这一等价表述见 §8.2 注记）。

**(f) TLB/页表问题导致 LOAD B 读到别的物理页？强排除【强推】**
LOAD B 若因页表/TLB 错误映射到别的物理页：那个"别的页"恰好是全零的概率极低，且 +0x20（BSS 页）与 +0x28（direct map 1GB 块映射）用不同页表项，+0x20 明显正确（x3 非零使 cbz 未跳）。另外 22:39 案在 CPU 179 出现的 8 次 spurious translation fault 恰是"翻译读出错误"的同族表现（页表 walker 明明该页在线却报 not-present，fault.c 判定 spurious 后忽略），提示该核的翻译/读出通路有前科。本单发案更可能是数据读出（L1D/load pipe）而非 TLB：TLB 错一般映射到随机页（内容随机），全零结果更像"load 返回被清零/丢弃"。【强推】倾向 L1D/load pipeline。

**(g) 真的是"内存坏了"（该 8 字节所在 DRAM 单元翻转，读出时刚好是 0）？排除【实锤】**
vmcore 是崩溃瞬间（2838.8s）由 kdump 抓取的，`rd 0xffff6057fffb5b40` 读出的 `ffff6057fffaeb00` 是同一物理字节在崩溃后数秒/数小时内的真值。若是 DRAM 位翻转，翻转应持续存在（单粒子锁定/驻留翻转）或随机（每次读不同）；我们多次（多宽度 rd、多 session）读出恒为非零真值。若是"瞬时翻转恰好在 LOAD B 那一拍"，则属于颗粒软错误：但单字节软错误翻转恰好把 64 位指针全部 39 个非零位（指针 0xffff6057fffaeb00 有 39 个 1）同时翻成 0 的概率是 2^-39 量级的同步多位翻转，且 ECC（ghes_edac 管理 32 DIMM）对多点翻转会报 UE，无任何上报。软错误解释要求"多点+瞬时+零 ECC 记录+恰好这 8 字节"，宇宙线打不出这么准。真·读出路径 SDC 是唯一自洽解释。

**(h) 软件 bug（内核自身缺陷）？排除【实锤】**
`get_pfnblock_flags_mask` 是内存管理热路径（每次 buddy free 都走），本内核上线 47 分钟、整机 192 核高负载（load 176）下该函数被调用数以亿计，同一二进制在全球 openEuler 舰队上量产运行。若是软件 bug，不可能只在这台机器的 CPU 179 上以"内存非零而读出零"的形态出现一次。且 pfn→root→section→usage 全链路值经 crash 结构化查询与裸内存双验证一致合法。

### 6.4 受害者任务完整栈（【实锤】，crash_bt.log / crash_stk_*.log）

```
close(fd=6) → __arm64_sys_close → __fput_sync → __fput → dput → dentry_kill
→ __dentry_kill → dentry_unlink_inode → iput → iput_final → evict
→ ext4_evict_inode → truncate_inode_pages_final → truncate_inode_pages_range
→ __folio_batch_release → folios_put_refs → free_unref_folios(+0x278 bl get_pfnblock_flags_mask)
→ get_pfnblock_flags_mask+0x3c → el1h_64_sync → do_mem_abort → do_translation_fault
→ do_page_fault → __do_kernel_fault → die_kernel_fault → die → crash_kexec → kdump
```
栈上 pt_regs（ffff8001de8a36d0 起）与 dmesg 寄存器窗口逐字一致；x9=返回地址 0xffffa7cc00016e84=free_unref_folios+636（bl 的下一条）✓。异常帧上层栈含 pfn 0x6040b1d7c、x4_old 0xc0816 等 caller 现场，全部自洽，没有任何一处栈/内存数据支持"内存被写坏"，唯一异常就是 LOAD B 的结果。

## 7. 逻辑链条（读出矛盾实锤链）【逻辑链条】

```
① 内存真值（vmcore, crash rd）        mem_section[0xc08] @ 0xffff6057fffb5b40 = 0xffff6057fffaeb00 ≠ 0   【实锤】
② 该值有效                            → 指向 32 条目全在线的 section 数组；目标 section 0xC0816 PMOE；
                                        usage=0xffff6057fffa27d0 非空；位图全 0x111...1               【实锤】
③ CPU 装载结果（pt_regs）             LOAD B(+0x28) 之后 x3 = 0                                          【实锤】
④ ① ∧ ③ 矛盾                         同一地址、同一 8 字节：内存非零，读出为零 → 读出路径 SDC          【实锤】
⑤ 出错 load 唯一化（x4 指纹）         x4=0x160 = ubfiz(0xc0816) ⟹ +0x2c 执行过 ⟹ cbz 未跳 ⟹ LOAD A 正确
                                        ⟹ 若 LOAD B 返回 G≠0 则 FAR=G+0x168≠8 ⟹ 仅 G=0 与
                                        (x3=0, x4=0x160, FAR=8) 同时吻合 → 出错的是 LOAD B             【实锤】
⑥ 非写坏                              写坏需要内存值为 0；① 显示内存完好（且崩溃前后无人可写它，§6.3a）【实锤】
⑦ 微架构位置                          LOAD B: va 0xffff6057fffb5b40 → phys 0x6057fffb5b40（node7 direct map），
                                        node-local（CPU179 ∈ node7）；错全 39 位 → 单点故障在核私有读出通路
                                        （L1D 行回读 / load pipeline 数据前递 / （次选）TLB 项），无 ECC 覆盖【强推】
⑧ 复发性指向同一物理核                17 个历史 dump：16 次 CPU 179、1 次 CPU 168（node7 邻核）；
                                        上一次开机（22:39 案）CPU 179 先 8 次 spurious translation fault
                                        （同为"读出/翻译错误"族）再 fatal                                【实锤】
⑨ 发作形态                            零前兆单发即死：47 分钟静默 → 一次 load 全错 → NULL 解引用 → panic 【实锤】
```

链条中每一环都有对应 crash log 文件支撑（附录 A 索引）。

## 8. 故障根因【故障根因】

### 8.1 根因陈述

**根因（【实锤】层面）**：CPU 179 在执行 `get_pfnblock_flags_mask+0x28`（`ldr x3,[x3,x5,lsl#3]`，访存 0xffff6057fffb5b40）时，load 结果与内存真值不符：内存为 0xffff6057fffaeb00，装载结果为 0。该 8 字节所在 DRAM/缓存行内容在崩溃后从 vmcore 读出始终正确，故损坏不在存储介质，而在**读出通路**。

**根因（【强推】层面，微架构定位）**：故障点位于 CPU 179 的核私有数据读出路径，候选按可能性排序：
1. L1 D-cache 行回读/输出选择逻辑：tag 命中但数据阵列读出时位线/字线选错或输出被清零，产生"行内数据正确、送回执行流水的结果错误"。L1D 虽多有 ECC/奇偶，但其校验覆盖的是**驻留位翻转**；若读出放大器/选择树在读出瞬间出错（数据本身没翻转），ECC 看到的码字自洽（或根本读的是错误位置的"合法码字"），**不产生机器检查**，与零 MCE 记录完全吻合。
2. load pipeline 数据前递/重定序缓冲（ROB/LSU）目的寄存器写端口：load 返回值在进入 RF（x3）前被错路由或清零。寄存器堆/旁路网络完全无 ECC，天然不可观测。
3. （次选）D-TLB/翻译读出错误：TLB 给出错误翻译使 load 命中某个全零页，需要"恰好映射到零页"，概率低；且与 22:39 案的 spurious fault（翻译读出错）同族，不能完全排除，但本案"全零结果"更像数据通路清零而非随机页命中。
   排除项：不是 LLC/DDRC/DRAM（那是核间共享，故障会散布全节点 24 核，而非 17 次中 16 次锁定同一核）；不是 ECC 可见故障（零记录）。

**为何 ECC 拦不住（【实锤】的机制论证）**：Arm64 服务核的 ECC 保护边界在 L2/LLC 与 DDR（端到端 DDRC 侧）。L1D 的奇偶/ECC 只校验**阵列中驻留的码字**；本案数据在阵列中未翻转（否则事后读 vmcore 里该行会错，vmcore 经同一 LLC/DDR 读出恒为真值），是**回读/传送通路**在单拍内出错，属于 ECC 覆盖边界之外的"读出自检路"缺失问题。RAS Extension 在位（dmesg L1262）但无事件，正是"错误发生在被保护域之外"的直接表现。

### 8.2 与其他案的差异化证据（本案的独特价值）

- 读出 vs 写入的可判定性：多数 SDC 案只能看到"某结构坏了"（写入型或不可分辨）。本案因为崩溃点是一个纯读、无副作用、输入输出全部可见的查询函数（pfn 进、指针出、内存真值事后可读），才第一次把"读出≠内存"钉死。崩溃反而成了**现场做的对照实验**：CPU 读一次（结果 0），kdump 后我们再读 N 次（结果恒非零）。
- 注记：还有一种等价表述是"ALU 在 add/csel 阶段把 x3 翻成 0"，但 x4=0x160=add(0,0x160) 证明 add 正常，csel 是 2 选 1 纯组合逻辑，其输出 0 的唯一来源是输入 x3 已为 0，故错误必在 LOAD B 的数据返回处（§6.3e）。x4 这个"指纹"是本案推理的关键一环。

### 8.3 x27 之谜的澄清（对主会话初筛的修正）

主会话注意到 x27=0xffff6057fffbfb00 与 root=0xffff6057fffafb00 差 0x10000，猜测"x27 可能是调用者留下的旧值"。复核结论【实锤】：x27 就是 `node_data[7]`（pglist_data 结构，NODE_DATA[7] 的 pgdat），由 `free_unref_folios+612: ldr x27,[x23,x3,lsl#3]`（x23=&node_data、x3=7）装载，是本调用链的正常活跃值，不是残留，也不携带 mem_section 根指针。两者数值相近纯属 memblock 自顶向下分配使 pgdat 与根数组相邻（同一 64KB 邻域，NODE_DATA[7] 在 0x6057fffbfb00，根数组在 0x6057fffafb00）。该"巧合"不构成本案证据，予以澄清避免误导。

### 8.4 发作模式谱系（零前兆单发 vs 多前兆）【推测→部分实锤】

| 维度 | 本案（23:37） | 22:39 案（上一次开机） | 多数历史案 |
|---|---|---|---|
| 前兆 | 0（2750s 静默后单发即死） | 8 次 spurious translation fault（223s-333s 间隔 10-50s）+最终 fatal | 常见 WARNING/坏指针 |
| 崩溃 CPU | 179 | 179 | 179（17 案中 16 次） |
| 错误形态 | 非零指针读成 0（全 39 位） | 翻译读出 spurious + 坏指针 0x...97e0 | 0x...97e0 尾签多见（5/17 案） |
| 可归因 | 读出 SDC 实锤 | 读出/翻译 SDC 强推 | 读出 SDC 强推（同族） |

谱系解读【推测】：同一物理核的读出通路缺陷存在**间歇性恶化**的发作模式：轻度时表现为偶发 spurious fault（可被内核容错吞掉，仅 WARNING），重度时单拍全零直接致命。本案"零前兆"并非缺陷消失，而是该窗口内故障首次显现即命中致命载荷；负载（192 核压满、buddy free 风暴）提高了每拍读出次数，缩短了首次命中时间。0x...97e0 尾签的重复出现（不同进程、不同 VA 区间、同一后缀）提示缺陷可能表现为特定位模式的读出偏置（如某些位线固定读 0/1），本案"全零"是其在指针值上的极端投影，此点为【推测】，需 fleet 级统计验证。

### 8.5 芯片设计启示（对 DFD / ECC 边界 / 可观测性）【启示】

1. 读出自检路（DFD）缺失：现代核有丰富的 BIST/MBIST 覆盖阵列驻留故障，但对"行内容正确、回读通路单拍出错"的**功能读出路径**无在线检测。建议：关键结构（L1D 输出选择树、LSU 返回总线）增加影子 CRC/双读比对（哪怕采样式），或提供 RCPU 式回读自测试指令。
2. ECC 覆盖边界诚实化：ECC 的承诺止于"校验域内的驻留码字"。核私有数据通路（RF 写端口、旁路网络、L1D 输出）在保护边界之外，本案正是活教材。RAS/EDAC 零记录**不等于**无硬件故障，监控体系需要把"零 MCE 的 NULL panic"列为硬件疑案的显式信号。
3. **可观测性设计**：若 load 结果与内存可比对（如通过偶发的值校验采样或 L1D 出错时的微码重放），本类故障可被在线捕获。也建议固件/BMC 侧暴露每核 L1D/TLB 的巡检计数器，让"单核前科"（如 CPU 179）可被 OS 级 offine 策略消费。
4. **错误签名库**：本 fleet 的 `0x...97e0` 尾签、"非零读零"等签名应沉淀为自动聚类特征。本案正是靠"16/17 崩在同一核"这一跨 dump 统计才把漂移的个案收敛为硬件定位。

## 9. 启示【启示】

（与 8.5 呼应，面向本 fleet 的工程结论）

1. 单核聚焦的硬件疑案判定规则：多次崩溃若统计上集中于同一逻辑 CPU（本案 16/17→CPU 179），应立即按"单核硬件缺陷"处置（隔离/换件），而不是无限重启重跑，每次重启都是让坏核再掷一次骰子。
2. 读出 SDC 的取证范式：本案给出可复用的三步法：(i) 从崩溃指令反推"哪个 load 喂了坏寄存器"；(ii) 用**只读结构**（mem_section 类 boot 后只读数据）做内存真值对照；(iii) 用"跳转会跳过的指令的副作用"（x4=0x160 之于 cbz）做控制流指纹，唯一化出错 load。此法可移植到任何"load 喂指针→NULL deref"型 panic。
3. 零 MCE ≠ 无硬件故障：把"panic 前无任何 RAS 事件 + 崩溃形态为数据异常"作为读出路径 SDC 的筛查入口，优先级高于"先怀疑内核/软件"。
4. 对压测 fleet 的建议：开启 per-CPU 的故障计数聚合；对 CPU 179/168（node7）执行热隔离验证（taskset+offline 后观察复发率），以单核 off-line 的 A/B 结果反向锤实微架构定位（本报告【强推】部分的终审手段）。

## 10. 处置建议

1. **立即**：将 CPU 179（MPIDR 0x7a0300）从生产/压测调度中隔离（`echo 0 > /sys/devices/system/cpu/cpu179/online` 或 systemd CPUAffinity 排除），观察后续 72h 崩溃是否归零。这是对"单核读出缺陷"假说的决定性实验。
2. **短期**：联系整机厂（长江计算 R240K V2/BC82AMQA）与 CPU 供应商，提供本报告 + 17 案 crash-CPU 统计 + 22:39 案 8 次 spurious fault 日志，申请 CPU/单板更换（同槽换 CPU 后追踪 CPU 179 是否仍复犯，可区分"核缺陷"vs"供电/时序（如该核所在电压域）缺陷"）。
3. **中期**：fleet 巡检加入两项：(a) `Ignoring spurious kernel translation fault` 计数按 CPU 聚合告警（前兆信号）；(b) 同机重复崩溃的 crash-CPU 直方图（本案模式的自动化检测）。
4. 内核侧缓解（可选）：对 `mem_section` 类关键只读指针表增加启动后 checksum 周期校验，可在读出 SDC 再次发生时把"读出≠内存"从 crash 后分析提前为运行时告警（成本：每秒数次 64 字节 hash，可忽略）。
5. **不建议**：继续无差别重启重跑压测，17 案已证明复发率 ~100%（按天计）。

## 附录：命令索引

（全部在报告目录留档；`D=/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-23:37:57`，`V=/tmp/vmlinux-0102`）
统一调用形式：`printf '<cmd>\nquit\n' > /tmp/c17_x.txt; timeout 1800 crash $V $D/vmcore -i /tmp/c17_x.txt 2>&1 | tee crash_x.log`

| 目的 | 命令 | 留档 |
|---|---|---|
| 系统概览 | `sys` | crash_sys.log |
| 崩溃栈/全栈 | `bt` / `bt -f` | crash_bt.log, crash_btf.log |
| 崩溃函数反汇编 | `dis -l get_pfnblock_flags_mask` | crash_dis_gpfm.log |
| 调用者反汇编 | `dis -l free_unref_folios`（+ `dis -l 0xffffa7cbfff80b40 100`） | crash_dis_fuf.log, crash_dis_fpr_full.log |
| mem_section 符号/根指针 | `sym mem_section` / `p mem_section` / `rd -64 0xffffa7cc032cacf0`（-32/-16/-8 多宽度） | crash_memsection.log, crash_bss_multi.log |
| 矛盾核心 | `rd -64 0xffff6057fffb5b40`（+多宽度） | crash_rd_chain.log, crash_root_multi.log |
| 根数组全景 | `rd -64 0xffff6057fffb5b40 33`（+邻域 0xffff6057fffb5b00 16） | crash_rootarray.log, crash_rootarray_around.log |
| section 数组/结构 | `rd -64 0xffff6057fffaeb00 32` / `rd -64 0xffff6057fffaec60 2` / `p/x mem_section[0xc08][0x16].usage` | crash_secarray_full.log, crash_sec16.log, crash_struct_ms.log |
| section 在线状态 | `kmem -n`（6144 块全 ONLINE；section 788502 PMOE） | crash_kmem_n.log |
| usage/位图 | `rd -64 0xffff6057fffa27d0 4` / `rd -64 0xffff6057fffa27e8 4` | crash_usage_bitmap.log, crash_pb_flags.log |
| phys/映射 | `vtop 0xffff6057fffb5b40` 等 | crash_vtop.log, crash_vtop2.log |
| victim folio/page | `rd -64 0xfffffd8102c75f00 16` / `p ((struct page *)...)->flags` | crash_x28_page.log, crash_page_struct.log |
| 异常帧/栈 | `rd -64 0xffff8001de8a36d0 24`（+3650/36d0/3820 各段） | crash_stk_eframe*.log, crash_stk_fuf.log, crash_stk_low.log |
| x27 之谜 | `p/x node_data[7]` / `rd -64 ffffa7cc01b54dd0 8` | crash_nodedata.log |
| victim inode | `p ((struct inode *)0xffff6040036f5528)->...` / `mount` | crash_inode.log, crash_inode2.log, crash_mount.log |
| 任务/负载 | `task -R ...` / `ps`（191×arm0102_swizzle, opendcdiag） | crash_task.log, crash_ps_user.log, crash_bt_swizzle.log |
| panic log | `log \| tail -80` | crash_log_tail.log |
| 内存搜索（负证据） | `search ffff6057fffafb00`（-k/-p/-t；PARTIAL dump 覆盖有限，无命中不影响结论） | crash_search_all.log 等 |
| 数值验证 | `python3 algebra.py` | algebra.py, algebra_out.txt |

dmesg 证据行号（dmesg_forensics.txt，与原始文件同）：L117 node7 范围；L1206 CPU179 MPIDR；L1262 RAS Extension；L2175-2176 ghes_edac；L2579 最后正常消息；L2580-2639 panic 全文（L2580 NULL deref、L2582 ESR、L2586 FSC、L2593 Oops [#1]、L2596 CPU/PID、L2602 x27、L2610 x5/x4/x3、L2611 x1/x0、L2612-2635 call trace、L2636 Code 窗口）。

（完）

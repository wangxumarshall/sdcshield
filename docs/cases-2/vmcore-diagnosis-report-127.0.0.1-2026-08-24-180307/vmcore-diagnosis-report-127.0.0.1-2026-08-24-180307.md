# CPU 179 转储深度诊断报告（第 1 次独立重研究）
## ——34 次"假页错误"前兆 + 一次装载结果塌缩实锤：单核load数据通路与PTW读出双重受扰，6.2 天后命中块层热路径致命崩溃

| 项目 | 内容 |
|---|---|
| 目标转储 | `/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-24-18:03:07/vmcore`（116,388,303,206 B，Kdump compressed dump v6，PARTIAL DUMP） |
| 主机 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 CPU，768GB，8 NUMA 节点 |
| CPU/内核 | Kunpeng-920 (TaiShan-v110, HIP08)；6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP |
| 崩溃时间 | 2026-08-24 18:03:07（uptime 537463.03s ≈ 6.22 天；开机 ≈ 2026-08-18 12:45:24） |
| 受害进程 | 致命：kworker/u391:3（PID 2077673，writeback 写回 dm-2 openeuler-home）；前兆：irqbalance（33 次）+ bash（1 次） |
| 前兆 | 开机后 835s 起 34 次 `Ignoring spurious kernel translation fault`，全部 CPU 179，最后 3 连发在致命崩溃前 7.5s |
| 结论一行 | CPU 179 单核数据返回通路（load data path / PTW 读出）间歇性受扰：致命点 `ldr x3,[x3,x2]` 装载结果 0x553c521da2e9b99f 与内存真值 fffffd012d055b80 差 36/64 位而 dump 中内存完好【实锤】；33 次假 fault 证明 PTW 对已映射地址返回"无映射"【实锤】 |

---

## 1. 执行摘要

1. **现象**：系统稳定运行 6.2 天后，ext4 回写 kworker 在 `bio_add_page+0xf0` 以 `ESR=0x9600004`（level-0 translation fault）解引用 `0x003c521da2e9b99f` 触发 Oops → kdump。
2. **签名**：坏地址不是"野指针写坏内存"，而是**一次装载指令的返回值本身是乱码**：`ldr x3,[x3,x2]` 应当读出 `bvec[70].bv_page = 0xfffffd012d055b80`，现场寄存器 x3 却是 `0x553c521da2e9b99f`；崩溃用 FAR 的低 7 字节与 x3 完全一致（仅最高字节被清零），证明崩溃点就是拿坏 x3 裸解引用。
3. **闭合验证**：用 crash 直读 dump：`bio->bi_io_vec=0xffff60401dabd000`、`bi_vcnt=71`，Python 复算 `sbfm x2,x0,#60,#31 → (71-1)<<4 = 0x460`，`bi_io_vec+0x460 = 0xffff60401dabd460 == 现场 x0`【实锤】；同窗口内前一条装载 `ldr x1,[x1]` 读 folio->flags 的结果 `0x055ffffe0000806b` 与 dump 中 `*(folio)` **逐位一致**（该装载正确），`ubfm x2,x1,#53,#55` 复算 =2 与现场 x2 一致——同一指令序列里**只有那一条装载的返回值坏了**。
4. **内存真值完好**：dump 中 `*(0xffff60401dabd460) = 0xfffffd012d055b80`（合法 vmemmap `struct page*`，vtop 落在 2MB 物理页 0x4057cce00000），即**被读内存本身没有被写坏**，坏的是"读回来的数据"→ 排除持久性内存损坏，指向 CPU 侧数据返回通路。
5. **前兆链**：同一 CPU 179 上 34 次前兆横跨 6.21 天：33 次 irqbalance 每 10s 扫描 `/proc/interrupts` 时 `__memcpy+0x80`（`strb w8,[x0,x14]`，写 seq 缓冲）报 level-0 假 fault，FAR 全部精确等于 memcpy 目的指针（x24+0xa，33/33 验证通过）；内核用 `AT S1E1R` 重走页表证明地址**有映射**后打印 "Ignoring spurious"。这说明该核的**页表走查（PTW）读出**间歇性把有效表项读成"无映射"。
6. **置信与处置**：微架构根因判定为 **CPU 179 单核 load 数据通路（含 PTW 的描述符读取）间歇性位级受扰**，置信度：装载结果塌缩【实锤】、PTW 读出受扰【实锤】、单核归属【实锤】（34+1 次事件全部 CPU 179）；具体物理层（SRAM 单元/时序/供电/粒子）无法软件判定【假设】，建议隔离 CPU 179（`irqbalance` ban + cgroup 隔离 + offline）并跑 opendcdiag LLU 类压力复现。

## 2. 证据规则与方法

- **证据源**：仅 `vmcore-dmesg.txt`（4466 行）、116GB `vmcore`（crash 8.0.4-17.oe2403sp4 + `/tmp/vmlinux-0102`，BuildID 276194e5f356f9c4bc570bb0a750394d7b768035）、目录内原始文件 `sdc_long`。**未读**任何既有诊断文档（docs/cases、docs/hypothesis、docs/cpu、.planning、dump 目录内 .md）。
- **诚实铁律**：全部结论来自实际命令输出；crash 会话共 15 次（加载一次约 60–90s，`-i` 批处理，timeout 590–595）；PARTIAL DUMP 有大量 `IRQ stack pointer` seek error（384 条/会话，kdump 未含低地址段），不影响所用内核数据结构读取，已如实记录于 crash_forensics.txt。
- **三级置信**：【实锤】= 可用 dump 直接复核；【强推】= 多源收敛但无直接对照；【假设】= 软件不可验证，给出验证途径。
- **64 位运算**：全部经 `algebra.py`（模 2^64）复算，输出 `algebra_out.txt`。
- **工具清单**：crash 8.0.4（内置 gdb 10.2）、objdump、nm、readelf、file、grep/awk、python3。

## 3. 本次开机时间线【时间线】

| uptime(s) | 墙钟(推算) | 事件 | dmesg 行 | 置信 |
|---|---|---|---|---|
| 0 | 08-18 12:45:24 | 开机，192 CPU / 8 节点 / 768GB | 1 | 实锤 |
| 1.49 | 12:45:25 | ghes_edac 就绪（APEI firmware-first） | 2175-2176 | 实锤 |
| 835.044 | 12:59:19 | **前兆#1** irqbalance 假 fault @ffff604005eb5395 | 2579 | 实锤 |
| 6367.04–172055.05 | 08-18 14:31 → 08-20 12:39 | 前兆#2–#25（间隔均为 10s 整数倍 ±0.05s，irqbalance 扫描相位漂移） | 2624–3659 | 实锤 |
| 6940.087 | 08-18 14:51 | **前兆#3（唯一 bash）** __lruvec_stat_mod_folio+0x20 假 fault @ffffc360a9e44e08（vmalloc percpu 区，读） | 2669 | 实锤 |
| 515385.04–521655.07 | 08-24 16:12 → 16:29 | 前兆#26–#31，密度升高（140s/40s/40s/80s 短间隔） | 4006–4231 | 实锤 |
| 537455.068/.069/.073 | 18:02:55 | **前兆#32–#34 三连发**（1ms/4ms 粒度，同一 show_interrupts 读循环内连续 3 个 memcpy 块中招） | 4276–4366 | 实锤 |
| 537462.586 | 18:03:02 | **致命 Oops**：bio_add_page+0xf0，FAR=003c521da2e9b99f，CPU 179 kworker/u391:3 | 4410 | 实锤 |
| 537463.034 | 18:03:07 | Starting crashdump kernel | 4465 | 实锤 |

（墙钟 = 目录名 2026-08-24 18:03:07 反推，Python 见 algebra.py [C]）

## 4. 故障现象【故障现象】

### 4.1 Oops 原文（dmesg 行 4410–4465）

```
[537462.585964] Unable to handle kernel paging request at virtual address 003c521da2e9b99f
[537462.598638]   ESR = 0x0000000096000004
[537462.603351]   EC = 0x25: DABT (current EL), IL = 32 bits
[537462.617755]   FSC = 0x04: level 0 translation fault
[537462.627451]   ISV = 0, ISS = 0x00000004, ISS2 = 0x00000000
[537462.639928]   CM = 0, WnR = 0, TnD = 0, TagAccess = 0
[537462.646210] [003c521da2e9b99f] address between user and kernel address ranges
[537462.654323] Internal error: Oops: 0000000096000004 [#1] SMP
[537462.767631] CPU: 179 PID: 2077673 Comm: kworker/u391:3 Kdump: loaded Tainted: G        W
[537462.789752] Workqueue: writeback wb_workfn (flush-253:2)
[537462.796034] pstate: 00400009 (nzcv daif +PAN -UAO -TCO -DIT -SSBS BTYPE=--)
[537462.803963] pc : bio_add_page+0xf0/0x1a0
[537462.808851] lr : bio_add_folio+0x30/0x50
[537463.019369] Code: f9400021 8b020060 f8626863 d375dc22 (f9400061)
[537463.029250] SMP: stopping secondary CPUs
[537463.034559] Starting crashdump kernel...
```

WnR=0（读）、S1PTW=0（非页表走查型 abort）、level 0 translation fault：PGD 都没命中——因为 `0x003c...` 根本不是内核地址（最高字节 0x00），level-0 直接 fault。

### 4.2 全量寄存器（x0–x30，dmesg 行 4411–4417）

```
x29: ffff8001e547b5f0 x28: 0000000000010000 x27: ffff60403d00cae0
x26: 000000000f569bf0 x25: ffff8001e547baf8 x24: ffff404420819b68
x23: 0000000000010000 x22: fffffd010d971400 x21: 0000000000000000
x20: 0000000000010000 x19: ffff60401b366738 x18: 0000000000000000
x17: 00000000ffffffff x16: 00000000fffffffe x15: 0000000000000800
x14: 0000000000000000 x13: ffff60407f2b4698 x12: 0000000000000030
x11: 0000000000001000 x10: ffff404420819b68 x9 : ffffc360a8593ab0
x8 : 0000000000000010 x7 : 000000000000000c x6 : ffff8001e547b704
x5 : 000000000000001000 x4 : ffff602018debf00 x3 : 553c521da2e9b99f
x2 : 0000000000000002 x1 : 055ffffe0000806b x0 : ffff60401dabd460
```

### 4.3 Call trace（完整，dmesg 行 4418–4424 + crash bt 交叉验证）

```
bio_add_page+0xf0/0x1a0            ← 致命指令 ldr x1,[x3]
bio_add_folio+0x30/0x50
iomap_add_to_ioend+0x174/0x258
iomap_writepage_map_blocks+0x16c/0x198
iomap_writepage_map+0x158/0x408
iomap_do_writepage+0x24/0x38
write_cache_pages+0x158/0x370
iomap_writepages+0x4c/0x80
ext4_iomap_writepages+0xc0/0x218 [ext4]
do_writepages / __writeback_single_inode / writeback_sb_inodes
__writeback_inodes_wb / wb_writeback / wb_do_writeback / wb_workfn
process_one_work / worker_thread / kthread / ret_from_fork
```

crash `bt`（会话1）与本 trace 逐帧一致；`bt -t` 另见栈上残留 `__mod_memcg_lruvec_state` 帧（writeback 途中合并统计的正常痕迹）。

### 4.4 前兆异常原文（节选，全部 34 次见 dmesg_forensics.txt [F05]）

第一次（uptime 835s，行 2578–2590）：
```
[  835.043543] ------------[ cut here ]------------
[  835.043550] Ignoring spurious kernel translation fault at virtual address ffff604005eb5395
[  835.043555] WARNING: CPU: 179 PID: 9665 at arch/arm64/mm/fault.c:494 __do_kernel_fault+0x130/0x1b8
...
[  835.043775] __memcpy+0x80/0x240
[  835.043779] seq_printf+0xc4/0xe8
[  835.043784] show_interrupts+0x1d4/0x498
[  835.043787] seq_read_iter+0x168/0x478    ← irqbalance 读 /proc/interrupts
...
[  835.043709] x24: ffff604005eb538b  x25: 0000000000000c75
[  835.043718] x21: ffff604005eb5395   ← FAR，恰等于 x24+0xa
```

唯一 bash 事件（uptime 6940s，行 2668–2708）：
```
[ 6940.087163] Ignoring spurious kernel translation fault at virtual address ffffc360a9e44e08
[ 6940.087170] WARNING: CPU: 179 PID: 37681 at arch/arm64/mm/fault.c:494
[ 6940.087399] __lruvec_stat_mod_folio+0x20/0x98   ← ldr x4,[x3,x4,lsl#3]，读 vmalloc percpu 区
[ 6940.087409] folio_add_new_anon_rmap / wp_page_copy / do_wp_page ... ← bash 写时复制缺页路径
```

最后三连发（行 4276–4390，uptime 537455.068/.069/.073，三个 memcpy 目的指针分别为 ffff40295ce624d4 / ffff40295ce62185 / ffff40295ce62395，同一 seq 缓冲页 ffff40295ce62xxx）。

### 4.5 RAS 负证据（dmesg grep）

```
$ grep -i 'hardware error|machine check|memory failure|ECC error|EDAC error|corrected|uncorrected' vmcore-dmesg.txt
（无输出）
```
RAS 能力在位：`CPU features: detected: RAS Extension Support`（行1262）、`GHES: APEI firmware first mode is enabled`（行1307）、`ghes_edac: This system has 32 DIMM sockets`（行2175）。**整次开机 0 条硬件错误上报**——无 CE、无 UE、无 thermal。这排除了"已被固件/EDAC 感知的内存侧错误"，与"CPU 核内数据通路受扰、不经过 DDR ECC 逻辑"的判定自洽。

## 5. 业务现象

- **主机是 SDC 研究机**：`ps` 显示 309 个 podman 容器驻留、192 个 `mrn_rmw_diff` 线程（父进程 comm=`opendcdiag`，cwd=`/home/sdc/root/arm64-sdc-fuzzing/opendcdiag`，日志 `builddir/llc_domain_campaign/mrn_rmw_diff_10m_pair.log`）、49 个 HeapHelper、7 个 `exe`、多个 claude/mi-scavenger——正在跑 **opendcdiag（OpenDCDiag）LLC 域 SDC fuzzing 活动**，负载均值 50–78。
- **致命受害者**：`kworker/u391:3`（flush-253:2，dm-2 = openeuler-home LVM 卷 ext4 回写）。写回是全机最繁忙的内核热路径之一，`bio_add_page` 在该机上每秒被执行数万次。
- **前兆受害者**：irqbalance（10s 周期 `read(/proc/interrupts)` → `show_interrupts` → `seq_printf` → `__memcpy` 把格式化行拷入 seq 页缓冲）是天然的"周期性 memcpy 探针"；bash（CoW 缺页统计）是偶发探针。
- **sdc_long**（目录内原始数据，独立取证）：ELF64 aarch64 动态链接可执行，openEuler GCC 12.3.1 编译。main 反汇编还原出其语义：`posix_memalign(64,128)` 缓冲 → 填充 `0xffffffffffffe000` → 无限循环重读，字值 ≠ 填充值即打印 `HIT r=%llu obs=%016lx xor=%016lx`，周期性打印 `tick cpu=... err=... sink=...`。它是一个**用户态 SDC 软探针**（常量解码见 algebra.py [D]）。本次 dmesg 中无 HIT/tick 输出、panic 时任务表中亦无 sdc_long 进程——它在本机本次开机内未捕获到事件（其价值在于证明该机群在做 SDC 主动筛查这一背景）。

## 6. 诊断定位过程【诊断定位过程】

**P1 dmesg 勘察**：提取 34 次前兆 + 1 次致命；统计 CPU/PID/Comm：35 次事件全部 CPU 179；33 次 irqbalance（victim 帧 `__memcpy+0x80`）、1 次 bash（`__lruvec_stat_mod_folio+0x20`）、1 次致命（`bio_add_page+0xf0`）。

**P2 崩溃块提取**：Code 窗口 `f9400021 8b020060 f8626863 d375dc22 (f9400061)` 定位致命指令为 `ldr x1,[x3]`（bio_add_page+0xf0），坏值源头是上一条 `ldr x3,[x3,x2]`（+0xe8）。

**P3 crash 加载**：`crash /tmp/vmlinux-0102 vmcore -i <cmdfile>`，15 次会话（最小集 → 全量 → 结构体 → 内存真值 → 任务/队列 → 溯源）。PARTIAL DUMP 每次加载 384 条 IRQ-stack seek error（如实记录，不影响本取证所用地址）。

**P4 内存真值对照**（关键证据，全部来自 crash 实测）：
```
p ((struct bio *)0xffff60401b366738)->bi_io_vec  = 0xffff60401dabd000
p ((struct bio *)0xffff60401b366738)->bi_vcnt    = 71
rd -64 0xffff60401dabd460                        = fffffd012d055b80   ← bvec[70].bv_page 真值
rd -64 0xfffffd010d971400                        = 055ffffe0000806b   ← folio->flags 真值
vtop 0xffff60401dabd460 → phys 60401dabd460（1GB 块映射，PTE VALID）
vtop 0xffff40295ce62000 → phys 40295ce62000（前兆 seq 缓冲同样在线性映射）
```
Python 复算（algebra.py [A2]/[A3]/[A4]）：x0 == bi_io_vec + (71-1)*16【实锤】；x1 == folio->flags 真值【实锤】；x2 == (flags>>53)&7【实锤】；**x3(现场) ≠ *(bi_io_vec+0x460)(真值)，差 36/64 位**【实锤】。

**P5 软件成因排除**：
- `bio_add_page` 是内核块层最热的函数之一（本机 6.2 天、每秒数万次执行），该指令序列被全机 192 核执行过亿万次；若是软件 bug，不会只在 CPU 179 出错。
- 坏值 0x553c521da2e9b99f 不等于现场任何对象（folio/bio/bvec/flags）的截断、移位或别名（[A5] 逐项排除）；36 位随机翻转不是任何编译器/算法能造出的模式。
- 34 次前兆横跨 6.21 天、不同进程（irqbalance/bash/kworker）、不同函数（memcpy/lruvec/bio_add_page）、不同内存区（线性映射/vmalloc/struct page），唯一公共因子是 **CPU 179**。
- RAS 零上报 + dump 中内存真值完好，排除 DIMM 侧持久故障。

**P6 定位收敛**：三类证据同向：
1. 装载结果塌缩（+0xe8 的 ldr 返回乱码，内存完好）→ load data path；
2. 33 次"假 translation fault"（AT S1E1R 重走证明映射存在）→ PTW 的页表描述符读取返回了"无映射"——PTW 读出也是一类特殊的 load；
3. 1 次 vmalloc percpu 区读假 fault。
全部是"**读回数据 ≠ 存储真值**"的同一形态，全部单核。

## 7. 逻辑链条

**指令语义**（objdump /tmp/vmlinux-0102，gdb info line 映射 block/bio.c:1115、mmzone.h:1153）：
```
+0xd8  ldr  x3,[x19,#112]   ; x3 = bio->bi_io_vec = 0xffff60401dabd000   （装载正确*）
+0xdc  sbfm x2,x0,#60,#31   ; x2 = (bi_vcnt-1)<<4 = 0x460                 （计算正确）
+0xe0  ldr  x1,[x1]         ; x1 = folio->flags = 0x055ffffe0000806b      （装载正确，=内存真值）
+0xe4  add  x0,x3,x2        ; x0 = 0xffff60401dabd460 = &bvec[70]        （计算正确）
+0xe8  ldr  x3,[x3,x2]      ; x3 = bvec[70].bv_page ← 0x553c521da2e9b99f （★装载结果被腐化）
+0xec  ubfm x2,x1,#53,#55   ; x2 = 2                                       （计算正确）
+0xf0  ldr  x1,[x3]         ; 用坏 x3 解引用 → level-0 fault → die()      （致命）
```
（*x19=bio 经 `struct bio` 实测字段核对）

**闭合等式【实锤】**：
- E1（索引）：`bi_io_vec + ((bi_vcnt-1)<<4) == x0`（0xffff60401dabd460，Python mod 2^64 复算 True）
- E2（真值）：`*(x0) == 0xfffffd012d055b80`（crash rd 实测）且为合法 vmemmap page*（vtop → 2MB 页 4057cce00000 内）
- E3（坏值）：`x3@现场 == 0x553c521da2e9b99f`；`x3 ^ 真值` popcount=36，逐字节 3–6 位翻转
- E4（FAR 一致）：`FAR == x3 & 0x00ffffffffffffff`（最高字节 0x55 越界被裁）——崩溃确系坏 x3 解引用
- E5（旁证装载正确）：`x1@现场 == *(folio) == 0x055ffffe0000806b`、`x2@现场 == (x1>>53)&7 == 2`

**反事实推演**：若 +0xe8 装载返回真值 0xfffffd012d055b80，则 +0xf0 读 `page->flags`（mmzone.h:1153 page_zone 路径）完全正常，函数继续走 bvec 合并/添加逻辑，**不会崩溃**。FAR 地址本应有效这一点不适用于本案（FAR 本身就是坏值），但前兆 33 次的 FAR（ffff60/ffff40/ffff20/ffff00 线性映射 + ffffc360 vmalloc）全部是"本应有效"的地址——AT 重走均证明有映射。

**前兆闭合（33/33 Python 验证）**：`FAR == x24 + 0xa`（x24 是 __memcpy 内部目的指针减 0xa 的中间值，callee-saved 存活到 handler）；__memcpy+0x80 = `strb w8,[x0,x14]` 写目的侧。即：**对同一个已经映射的 seq 缓冲写字节时，TLB/PTW 报了 level-0 无映射**。

**诚实声明**：
- 36/64 位的翻转形态无法用软件进一步分解（单事件多位翻转，一次读出窗口内整体出错）；
- "load data path vs 寄存器文件写入端口"二者在本案证据下不可分辨（x3 在 +0xe4 时还是好的、+0xe8 后变坏，窗口只有 1 条指令），但无论哪种，都属**核内数据返回/提交通路**，而非内存或缓存一致性（单一读者、私有数据、内存真值完好）；
- PARTIAL DUMP 未能读取的页（seek error 列表）不含本案任何关键对象。

## 8. 故障根因（微架构级结论 + 置信级别）

**根因判定：CPU 179（Node 7，MPIDR 0x6b0300 一族）核内数据返回通路间歇性位级受扰——具体表现为两类"读回≠真值"：(a) 通用 load 数据通路（bio_add_page 的 bvec 装载返回 36 位乱码，内存完好）；(b) 页表走查描述符读取（PTW 对已映射地址返回"无映射"，33 次假 fault 经 AT S1E1R 重走自证）。两类共享核内 L1/填充缓冲到寄存器的数据返回路径，6.21 天内 35 次事件全部锁定该核，最终命中块层热路径导致 panic。**

- 装载结果 ≠ 内存真值（内存在 dump 中完好）：**【实锤】**
- PTW 读出受扰（假 translation fault，AT 重走证明映射在）：**【实锤】**
- 单核归属（35/35 事件同核，含跨 3 个不同受害进程、3 类不同函数）：**【实锤】**
- 统一定位到"核内数据返回通路"（含 PTW 读出与通用 load 共享路径）：**【强推】**（两类现象同核同时段收敛，但无法直接观测流水线内部）
- 物理层成因（SRAM 弱位/时序裕量/电压降/粒子翻转）：**【假设】**——软件不可判定；验证途径：隔离 CPU 179 后复跑 opendcdiag `llc_domain_campaign` + `sdc_long` 类探针（若事件消失即坐实单核硬件；若仍复现则上移到 L3/LLC 域或 MC 通路）；BMC/IPMI SEL 与 RAS 二进制日志（本次 dmesg 零上报，需查固件侧）；Kunpeng RAS 特性（HHA/L3C PMU 计数器）读数对比相邻核。

排除项：DDR/DIMM 持久故障（真值完好 + ECC 零上报）【实锤排除】；软件 bug（热路径亿万次执行 + 事件挑核挑进程）【实锤排除】；缓存一致性（私有数据单读者、真值与时间无关地完好）【强推排除】。

## 9. 启示

### 9.1 微架构定位的意义
本案的价值在于用**纯软件证据**完成了"核内数据通路"与"内存/一致性域"的二分：方法是"装载结果 vs dump 真值"的逐位对照 + "假 fault 的 AT 重走自证"。这套方法可模板化：任何 `level-0 translation fault on 合法内核地址` 都应先跑 `is_spurious` 语义复查（内核已内置），并在 crash 中验证被读地址的 dump 真值——**一次比对即可把故障从"软件/内存"切到"CPU 数据通路"**。34 次前兆跨 6.2 天的完整时间序列还提供了"间歇性、同核、与负载类型无关"的统计证据，这是单次崩溃无法给出的置信度。

### 9.2 芯片设计与实现启示（具体、可落地）
1. **load 数据通路端到端保护**：从 L1 填充缓冲→物理互连→寄存器写端口的整条返回通路补齐 parity/ECC（本案 36/64 位翻转形态说明无有效检错——若通路上任一环节有 SECDED，至少 1 位错会被拦截或上报）；PTW 的描述符读出（含 AT 重走路径）应纳入同一保护域。
2. **PTW 输出校验**：页表描述符在写入 TLB 前做冗余读回比对或 parity 复核；对"walk 结果=无映射"且同地址 AT 复走=有映射的分歧，硬件应记录到 ERX/AMU 可读计数器，而不是让软件 WARNING 兜底。
3. **fail-fast vs silence 的权衡**：本案 33 次假 fault 内核全部"Ignoring"（`___ratelimit` + WARN），系统带病运行 6.2 天直到致命点。建议对"同核重复 spurious fault"加阈值熔断（如 24h 内 ≥3 次即 hotplug offline 该核 + 上报 RAS），把静默退化变成受控降级——这类单核数据通路故障的边际隔离成本（191/192 核继续服务）远低于最终整机 panic。
4. **DFT/在线测试钩子**：为核内数据通路提供在线 BIST（idle-time 洗练读出测试，类似 opendcdiag 的 mrn_rmw_diff 思路下沉到硬件），可在故障早期（835s 首次前兆时）捕获；为 PTW 提供"影子重走"自检模式。
5. **验证侧**：硅前 DFT 应包含"装载通路多位翻转"故障注入模型（本案 36/64 位翻转≠单粒子标准模型，更像窗口性总线/锁存器整体出错）；流片后 screen 测试应覆盖 PTW 读出路径的电压/频率角点。
6. **kdump 可靠性**：PARTIAL DUMP 丢 IRQ 栈（384 seek error）虽未影响本案，但"关键取证对象所在页优先级"应可配置。

### 9.3 对系统软件/RAS 的启示
1. **把 `Ignoring spurious kernel translation fault` 从噪音升级为一级遥测**：它自带"硬件自证"（AT 重走），是最廉价的 CPU 数据通路 SDC 探针；建议采集进 RAS 事件流并按 CPU 聚合。
2. **周期性读探针的价值**：irqbalance 每 10s 的 `/proc/interrupts` 扫描无意中充当了 6.2 天不间断的内核 memcpy 探针（33 次命中）。运维侧可用 `sdc_long` 这类用户态探针（本目录附件即一例：填充 0xffffffffffffe000 反复读 + xor 累计）在用户态持续布防，覆盖内核探针盲区。
3. **RAS 负证据的解读纪律**："零硬件错误上报 + 内存真值完好 + 单核聚集"三要素齐备时，应直接把怀疑对象从 DDR 域移到 CPU 核内，而不是继续等待内存侧证据。
4. **处置自动化**：单核 SDC 证据链成立时，标准动作为：`irqbalance` ban 该核 → cgroup/cpuset 隔离 → 必要时 `echo 0 > /sys/devices/system/cpu/cpu179/online` → 挂 RAS 工单换板。

## 10. 处置建议

1. **立即**：拉取 BMC SEL / 华为 RAS 二进制日志核对 CPU 179 所在 die 的历史记录（dmesg 零上报只是 Linux 视角）。
2. **短期**：隔离 CPU 179（irqbalance ban 文件 + cpuset 排除；业务许可则 CPU offline），重跑 opendcdiag `llc_domain_campaign` 与 sdc_long 探针 24–72h：事件消失→单核硬件坐实；不消失→上移怀疑域（L3/LLC/片上互连）。
3. **中期**：对整机跑 DCVA/RMW 类数据通路压测（opendcdiag 全套），交叉验证 Node 7 其他 23 核是否共有边界条件（电压角点/时钟相移）。
4. **复盘**：本次 835s 首次前兆即有明确信号，但 "Ignoring spurious" 长期被视为噪音——应在前兆 #2 出现（同核重复）时触发工单，6.2 天的带病运行本可避免。

## 附录：命令索引（全部取证命令，可复核）

- dmesg：`ls -la <dumpdir>`、`wc -l vmcore-dmesg.txt`、`head/tail`、`grep -n -E 'WARNING|BUG|Oops|cut here|spurious|...'`、`sed -n '2578,2618p'` 等行段提取（dmesg_forensics.txt [F01]–[F10]）
- sdc_long：`file`、`readelf -h/-S/-p .comment`、`nm | sort`、`objdump -s -j .rodata`、`objdump -d --start-address=0x4008c0 --stop-address=0x400d04`（[F11]–[F13]，sdc_long_disasm.txt / sdc_long_elf_headers.txt）
- crash（namelist=/tmp/vmlinux-0102，命令文件见附件 *.cmd）：`sys/panic/bt/bt -t/bt -r/set/mach/ps/ps -m/runq/timer`、`struct bio 0xffff60401b366738`、`struct page 0xfffffd010d971400`、`rd -64 0xffff60401dabd460`、`rd 0xffff60401dabd000 71`、`vtop 0xffff60401dabd460 / 0xffff40295ce62000`、`p ((struct bio*)...)->bi_io_vec/bi_vcnt/bi_iter`、`p ((struct folio*)...)->mapping/_mapcount/_refcount`、`search -t 2077673 553c521da2e9b99f`、`bt 9665`、`runq -c 179`、`files <pid>`、`p task->mm->exe_file->...->d_name.name`、`gdb info line *0xffffc360a8592420`（crash_forensics.txt 会话 1–15）
- 反汇编：`objdump -d --start-address=<bio_add_page|__memcpy|__lruvec_stat_mod_folio|__do_kernel_fault|is_spurious_el1_translation_fault>` on /tmp/vmlinux-0102
- 代数：`algebra.py` → `algebra_out.txt`（A1–A5 寄存器闭合、B1–B4 前兆统计、C 墙钟、D sdc_long 常量、E 指令语义）


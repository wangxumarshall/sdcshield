# 多核级联受害 SDC 案诊断报告：一次 LSU 数据通路"整字替换"故障同时造成 pcp 链表头持久写坏（458 次稳定复现）与 CPU179 独有读零错误，最终以 find_busiest_group L3 fault 触发 panic

## 副标题：vmcore 127.0.0.1-2026-09-04-22:09:49 微架构级根因诊断（写路径 SDC + 读路径 SDC 同源）

| 项目 | 内容 |
|---|---|
| 转储目录 | /home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:09:49/ |
| vmcore | 9.5G 完整（crash 标注 PARTIAL DUMP，但关键页全可读） |
| vmcore-dmesg.txt | 2.1M，25361 行，397.150774s 起始（kdump 只保留 panic 前片段，无 boot 段） |
| 内核 | 6.6.0-145.3.23.154.oe2403sp3.aarch64 |
| vmlinux | /tmp/vmlinux-0102（带 DWARF/行号，dis -l 可用） |
| crash | 8.0.4-17.oe2403sp4 |
| 机器 | Yangtze Computing R240K V2/BC82AMQA，BIOS 7.48 06/15/2026，192 CPU，767.8 GB |
| panic | "Unable to handle kernel paging request at virtual address ffffb75e3dfa97e0"，PID 73729 rs:main Q:Reg，CPU 179 |
| 业务背景 | 压测中：192 个 arm0102_fma_f64（FMA 浮点压测）满负荷运行 + systemd-journal 日志洪流 |
| 启动参数 | crashkernel=1024M,high smmu.bypassdev=… arm64.nopauth nospectre_bhb |

## 1. 执行摘要

一句话结论：这是一次写路径 SDC 为主、读路径 SDC 同源伴生的多核级联受害案。故障核（最可能为 CPU179，但写坏者身份只能到"单核 LSU 数据通路"级别）在一次 64 位 store 中把 per-CPU pageset（`pcp = ffff6057fffbe960`）中 `lists[1].prev`（地址 `ffff6057fffbe998`）的值从正常的自指空表头值 `ffff6057fffbe990` 替换为另一个 vmemmap struct page 指针 `fffffd8101333948`（27 位差异，整字替换而非位翻转），该损坏持久落盘；随后 168/169/180 三个核在 rmqueue_bulk 的 `list_add_tail` 路径上 458 次逐字相同地撞上它（WARNING 风暴），50/55 两核在 free_pcppages_bulk 的 `list_del` 路径上 126 次连锁报警；最终 CPU179 在 find_busiest_group+0x140 处把 `__per_cpu_offset[174]`（内存真值 `0xffffc8a243768000`，crash 实测）读成 0，导致 x27 = `&runqueues` 模板地址（无 L3 映射，pte=0 双重实测），`ldr x23,[x27,#0x120]` 直接 L3 translation fault，panic → kexec。

三个最重要的实锤：

1. 【实锤】两次相邻 load 同一地址返回不同值（本案最深的微架构证据）：在 458 次 list_add WARNING 的每一次迭代里，`rmqueue_bulk+0x318` 的 `ldr x24,[x19,#8]` 读 `[ffff6057fffbe998]` 得到 `990`（好值，即打印中的 prev 参数），而紧随其后 `__list_add_valid_or_report+0x18` 的 `ldr x2,[x2,#8]` 再读同一地址得到 `3948`（坏值，即打印中的 next->prev）。打印文本 `should be prev (ffff6057fffbe990), but was fffffd8101333948` 与两次 load 的参数位置（`list_debug.c:29` 格式串 crash rd 实测）铁证此点。crash 快照 `rd ffff6057fffbe990` 最终为 `3948`：同一字节序列在"读入路径"上先于"写回路径"出错。
2. 【实锤】CPU179 的读零错误：panic 寄存器块 x20=0（dmesg L20558+）与栈槽实测（`rd ffff8001dfc63740`：[sp+8]=&__per_cpu_offset ✓、[sp+16]=&runqueues ✓）证明 `ldr x20,[x0,w25,sxtw#3]` 把 `__per_cpu_offset[174]`（真值 `ffffc8a243768000`，crash rd ffffb75e3e3a5b38 实测非零）读成了 0；全数组 170 项实测完好、单调、stride 0x22000。
3. 【实锤】坏值为"同类指针替换"而非位翻转：good=`ffff6057fffbe990`（应写入值，空表 prev 自指）与 bad=`fffffd8101333948`（实际值）异或后 27 位翻转，排除单粒子位翻转模型；bad 值本身是合法的 vmemmap `&page->lru.next`（pfn 0x6057a113，vtop 实测 phys 0x6057a1133948），且该 page 的 `lru.next` 恰好回指坏头 990（rd 实测），构成"同一工作集内两个同类指针被交换"的签名。

写坏者身份的诚实声明：dmesg 中 CPU179 仅出现 1 次（即 panic 块本身）；168/169/180/50/55 都是踩雷者（它们的 x3=各自正确的 __per_cpu_offset，栈实测都在 rmqueue/free_pcppages 路径上）。写坏发生在 dmesg 起始（397.150774）之前，kdump 截断使我们无法看到写坏瞬间的栈。CPU179 是嫌疑（fleet 其他案均为它 + 本案它有独立实锤的读错误）但写坏者只能定位到"单核 LSU 数据通路故障"级别，无法 100% 指认具体核。这是本报告的法证边界。

## 2. 证据规则与方法

证据来源（严格三选一，每条结论标注）：
- dmesg 原文：`/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-09-04-22:09:49/vmcore-dmesg.txt`，标注行号 LNNN。
- crash 实测：vmlinux `/tmp/vmlinux-0102` + vmcore，命令与完整输出存于本目录 `crash_session.log`（sys/bt）、`crash_session2.log`（bt 全家 + find_busiest_group/__list_add_valid_or_report/__list_del_entry_valid_or_report 反汇编）、`crash_session3.log`（内存 rd/vtop/per_cpu_offset/结构布局）、`crash_dis_rmqueue_bulk.log` 等四个反汇编专档。
- python3 代数：`algebra.py` → `algebra_out.txt`，全部 64 位运算程序化，无手算。

法证边界声明（dmesg 截断）：
- dmesg 从 397.150774 的一个 Call trace 中段开始（L1 即 `__list_add_valid_or_report+0x8c` 的栈帧），无 boot 段、无故障前正常段。第一行就已经在 WARNING 风暴中，写坏动作本身 100% 落在截断窗口之外。
- 因此：任何关于"写坏瞬间在哪个核哪个函数"的直接证据（栈、寄存器）都已丢失，只能通过快照内存状态 + 后续行为反推。本报告对此类内容一律标注【强推】或【推测】。
- 两个 Oops（CPU180 的 WARNING 打印流与 CPU179 的致命 Oops）的 console 输出交错（console 锁串行化），读 dmesg 时必须按线程重组（L20311-L20700 区段，algebra.py §O 有重组说明；crash bt/bt -c 180 提供权威栈帧）。

标注分级：【实锤】= 原始输出直接证明；【强推】= 多条实锤 + 唯一合理推理；【推测】= 有依据但无法排除替代解释。

被排除的伪线索（重要，避免后续误读）：
- `x9=ffffb75e3c5cc520` 出现 584 次 = `vprintk_emit+424`（crash sym 实测），是 __warn_printk 打印路径的返回地址残留，与 584 次 WARNING 一一对应，非故障（algebra.py §Q）。
- `x5 = &wake_up_klogd_work + __per_cpu_offset[cpu]`（K=0xffffb75e3df95d88 五核全对上）、`x10=ffff60575f8ceb40`（physmap page），printk 路径正常值（§V）。
- x12–x17 中的 ASCII（"fffbe990"、"should b"、"e prev ("）= printk vsprintf 字符串残留（§AG，396 次核对）。
- 5 个受害核的 `x3` = 各自正确的 `__per_cpu_offset[cpu]`（269/126/63/63/63 与 WARNING 计数一一对应），per-CPU 机制完好（§AO）。
- `ffffb75e` 前缀本身不是"坏前缀"：kernel text `_stext=0xffffb75e3c450000`、`_edata=0xffffb75e3e8daa00`（crash p 实测），全部坏值都是合法内核映像地址格式。

## 3. 时间线【时间线】

| 时刻 (s) | 事件 | 证据 |
|---|---|---|
| < 397.150774 | 【推测】写坏事件：`[ffff6057fffbe998]` 被从 990 替换为 3948（或进入"被反复错误读出"状态，见 §7 讨论） | dmesg 截断；快照 rd 实锤终态 |
| 397.150774 | dmesg 起点：已在 WARNING 风暴中（首个完整块是 CPU168 的 list_add 报警栈） | L1 |
| 397.150841 | 第 1 条 list_add corruption（head=990，bad=3948，此后 457 次逐字相同） | L29 |
| 397.150848–397.207196 | CPU168（systemd-journal，PID 69259）rmqueue_bulk 路径 269 次 list_add WARNING | python 统计 |
| 397.208275–397.234172 | CPU169（control，PID 75192）126 次 list_add WARNING（读文件/匿名页两条分配路径） | L12942 起 |
| 397.234380–397.247137 | CPU55（kworker/55:1，vmstat_update→drain_zone_pages→free_pcppages_bulk）63 次 list_del corruption（next is NULL / LIST_POISON1） | L18233 起 |
| 397.268529 | "Unable to handle kernel paging request at ffffb75e3dfa97e0"（CPU179 致命 Oops 开始，console 交错中无 CPU 头） | L20311 |
| 397.338855 | Internal error: Oops 0000000096000007；ESR EC=0x25 DABT current EL，WnR=0（读），FSC=0x07 L3 translation fault；pgd/pud/pmd 有效、pte=0 | L20342 区段 |
| 397.375316 起 | CPU180（in:imjournal，PID 73728）寄存器块（与 CPU179 块交错打印；crash bt -c 180 证明其仍在 rmqueue_bulk，非致命） | L20513 |
| 397.384566 | CPU179（rs:main Q:Reg，PID 73729）寄存器块：pc=find_busiest_group+0x140，x27=ffffb75e3dfa96c0，x20=0 | L20550–L20574 |
| 397.285155–397.431548 | CPU180 63 次 list_add WARNING（window 与上述交错） | python 统计 |
| 397.431713–397.439031 | CPU50（kworker/50:1）63 次 list_del WARNING（最后一批，含 L25350 块） | python 统计 |
| 397.441356 | Starting crashdump kernel... | L25360 |
| 442.176283 | Bye!（kdump 2nd 内核完成，45s 写 9.5G） | L25361 |

时间结构要点【实锤】：list_add 458 次集中在 397.15–397.43（0.28s），list_del 126 次在 397.23–397.44（0.20s）；5 个核的 WARNING 窗口串行接力（168→169→55→180→50），是 console 锁串行化的表象，不是 5 次独立故障。

## 4. 故障现象【故障现象】

1. 458 次 list_add corruption 完全相同【实锤】：
   `list_add corruption. next->prev should be prev (ffff6057fffbe990), but was fffffd8101333948. (next=ffff6057fffbe990).`
   458 次逐字一致 = 共享内存中的持久状态（非瞬态读错的随机表现）。
2. 126 次 list_del corruption【实锤】：`fffffd81XXXX->next is NULL / LIST_POISON1 (dead000000000100)`，126 个不同 page 地址（pfn 分散于 0x3fe63、0x87c95、0xfd90f 等），全部在 free_pcppages_bulk 摘链时发现"已删节点仍挂在链上"。这是 pcp 链表头损坏后 count 与实际链长脱节的下游连锁（§7 机制）。
3. WARNING 分布 5 CPU【实锤】：168(269)/169(126)/180(63)/50(63)/55(63)；CPU179 本身 0 次 WARNING，它只贡献了唯一的致命 Oops。
4. 致命 Oops【实锤】：CPU179, ESR=0x96000007（WnR=0 读触发），FAR=ffffb75e3dfa97e0，pc=find_busiest_group+0x140，`Code: …(f9409377)` = `ldr x23,[x27,#0x120]`（+0x120=288，rq->cfs 内负载聚合字段，fair.c:12053/5024），pte=0（L3 无映射）。
5. 同一坏地址双核撞点【实锤】：CPU179 的 x27（ffffb75e3dfa96c0）+0x120 = 恰等于 CPU180 Oops 的 FAR（ffffb75e3dfa97e0），python 实算差值 0x120/0（algebra.py §B）。实为同一逻辑错误的两种表现（见 §6.4：CPU180 的"FAR"其实源自同一 CPU179 Oops 的连续打印，dmesg 交错所致；crash bt -c 180 显示 CPU180 栈顶在 rmqueue_bulk 而非 find_busiest_group，CPU180 从未真正访问过该 FAR）。
6. 内核 Tainted: G W，无 hardware error / EDAC / LLCache 报错记录（dmesg 全文 grep 无 ras/edac/mce 事件），LLC ECC（如具备）未拦截本次数据替换。

## 5. 业务现象【业务现象】

- 压测负载【实锤，crash ps】：192 个 `arm0102_fma_f64` 进程（PID 75196+，parent 65495）绑核满跑（`ps -m` 显示几乎每 CPU 一个 RU 状态的 fma 进程）；用户态 PC `0000aaaad47ed1xx` 一带为 FMA 循环（168/169/55 三核用户栈完全同构：X27=0x800000、X17=0xffffafcb92c0、X15=0x051eb851eb851eb8 等浮点常数）。load average 128.20/49.41/18.26（sys 实测），故障窗口内系统仍在高强度调度。
- 日志洪流【实锤】：systemd-journal（PID 69259，mmap readahead 路径踩雷 269 次）与 rsyslogd 的 `in:imjournal`/`rs:main Q:Reg`（PID 73728/73729，同一进程的两个线程，分别在匿名页分配路径踩雷和 load_balance 路径送命），日志系统是主要受害者。
- 业务影响：panic 前约 0.29s 内页分配器大面积异常（458+126 次检查失败），日志落盘中断；kdump 占用 1024M high 内存成功抓取后整机重启（442.176s Bye!）。压测作业（fma）本身用户态寄存器无异常迹象（属旁证：故障点在内核数据通路，非用户数据）。

## 6. 诊断定位过程【诊断定位过程】

### 6.1 第一步：从 panic 栈确定致命线程与指令

crash `bt`【实锤】：panic task = PID 73729 "rs:main Q:Reg" @ CPU179，栈：
```
#12 find_busiest_group+0x140(实际显示 ffffb75e3c57ae44=+0x324 返回地址列)
#13 load_balance  #14 newidle_balance  #15 pick_next_task_fair  #16 pick_next_task
#17 __schedule  #18 schedule  #19 futex_wait_queue … #28 el0t_64_sync
```
`bt -c 180`【实锤】：CPU180 = PID 73728 in:imjournal，栈顶 rmqueue_bulk（存活，WARNING 源）；`bt -c 168/169/55` 分别为 fma 用户态/用户态/free_pcppages_bulk。dmesg 中两个寄存器块交错（L20513 起 CPU180 块与 L20550 起 CPU179 块逐行穿插）是 console 锁串行化假象，crash 栈是权威。

`dis -l find_busiest_group` 关键段【实锤】：
```
+0x68  adrp x1, 0xffffb75e3dfa9000 <cpu_worker_pools>
+0x6c  add  x1, x1, #0x6c0        → x1 = 0xffffb75e3dfa96c0
+0x78  str  x1, [sp,#40]; +0x7c str x1,[sp,#16]
+0xf4  adrp x24, node_data; +0xfc add x0,x24,#0x5d0 → x0 = &__per_cpu_offset[0]
+0xfc  str  x0, [sp,#8]
+0x12c ldp  x0, x1, [sp,#8]       → x0=&__per_cpu_offset[0], x1=&runqueues
+0x134 ldr  x20, [x0, w25, sxtw #3]  ← x20 = __per_cpu_offset[w25]
+0x13c add  x27, x1, x20          ← x27 = cpu_rq(cpu)
+0x140 ldr  x23, [x27, #288]      ← 崩溃指令 (Code dump 的 f9409377)
```

### 6.2 第二步：x27 坏值的代数闭环

`px &runqueues`【实锤】= `0xffffb75e3dfa96c0`。dmesg CPU179 x27 = 同值 → x20 = 0（algebra.py §A/P）。

正常语义：`x27 = &runqueues + __per_cpu_offset[cpu]`，如 cpu_rq(174) = 0xffffb75e3dfa96c0 + 0xffffc8a243768000（ffffc8a2 前缀的动态 per-cpu 区，有映射）。异常时 x27 落在 percpu 模板段（.data..percpu 的链接期符号区），该区无 L3 映射【实锤】：dmesg Oops `pte=0000000000000000` 与 crash `vtop ffffb75e3dfa96c0` 输出 `PTE: … => 0` 双重一致；同前缀的 .text（ffffb75e3c5cc520）vtop 正常有映射（phys 0x53405cc520），排除"整段页表坏"。

### 6.3 第三步：__per_cpu_offset 数组完好性（排除"内存写坏数组"）

`rd -64 ffffb75e3e3a55d0 176`【实锤】：170 项全部非零、单调递增、stride 精确 0x22000；`px __per_cpu_offset[168/169/174/179/180/50/55]` 全部正常值（如 [174]=0xffffc8a243768000）。数组在内存中是好的 → CPU179 的 `ldr x20` 是读/装载通路单点错误。同时 5 个受害核 WARNING 中的 x3 恰好等于各自正确的 offset（269/126/63/63/63 一一对应）【实锤】，全系统 per-cpu 基址机制正常，只有 CPU179 这一次读错。

panic 栈槽交叉验证【实锤】（`rd ffff8001dfc63740 8`）：[sp+8]=ffffb75e3e3a55d0（&__per_cpu_offset，x0 正确）、[sp+16]=ffffb75e3dfa96c0（&runqueues，x1 正确），基址全对，唯独 load 结果 x20=0。x25=0xae=174（循环内 cpu 号），目标地址 = 0xffffb75e3e3a55d0+174*8 = 0xffffb75e3e3a5b40，rd 实测 [174]=ffffc8a243768000 ≠ 0。

### 6.4 第四步：定位 pcp 坏链表头与"两次 load 矛盾"

`rd -64 ffff6057fffbe860 40` + `rd -64 ffff6057fffbe9a0 8` + `struct per_cpu_pages -o`【实锤】按 lists[17]@32 布局完全对齐（algebra.py §K/AN）：

- pcp = ffff6057fffbe960（动态 alloc_percpu 区，ffff6057 前缀）
- lists[0]@980：next=fd810045b708 / prev=fd8100422d48，双向追链（045b708→01f92c8→…；0422d48→03acb08→…）全部一致【实锤】
- **lists[1]@990：next=990（自指，空表形态）但 prev=fffffd8101333948（坏）** ← 458 次报警对象
- lists[2]@9a0 有节点正常；lists[3..5]@9b0-9d0 自指空表正常；lists[8]@a00 正常：单字段定点 8 字节损坏，同结构内相邻字段零附带损伤【实锤】

坏值语义【实锤】：`rd fffffd8101333948` → [0]=ffff6057fffbe990（该 page 的 lru.next 恰回指坏头 990）、[8]=fffffd8100c25588（其前驱）；vtop → phys 0x6057a1133948，按 64K 页/64B struct page 反推 pfn=0x6057a113，`fffffd8101333948 = &page->lru.next`，结构合法（algebra.py §AA）。

两次 load 矛盾（本案最深证据）【实锤，推导链见 algebra.py §AK/AL】：
- rmqueue_bulk 反汇编（crash_dis_rmqueue_bulk.log L321-328）：`+0x318 ldr x24,[x19,#8]; +0x31c mov x1,x24`，且 list.h:183 行号标注 = list_add_tail（page_alloc.c 把新页挂 pcp 尾）→ 调用参数 prev=[head+8]、next=head。
- `__list_add_valid_or_report` 反汇编：`+0x10 mov x4,x2; +0x18 ldr x2,[x2,#8]; +0x1c cmp x2,x1`；报错分支 `+0x7c mov x3,x4; bl __warn_printk`，参数 (x1=prev, x2=next->prev, x3=next)。
- 格式串 crash rd 实测（ffffb75e3d77ce50）：`"list_add corruption. next->prev should be prev (%px), but was %px. (next=%px)."`
- 打印值：prev=990、next->prev=3948、next=990。x19=x2=990（寄存器转储 458 次无例外）→ prev 与 next->prev 读的是同一地址 [ffff6057fffbe998]，却在同一迭代内两次 load 得到不同值（990 vs 3948）。
- 快照终态：rd = 3948（已持久化）。

物理含义（§7 详述）：坏值 458 次稳定"只出现在第二次读"，最自洽的解释是它长期驻留于读通路可注入的位置（同核缓存中真实存在的另一行数据，同类指针），并在 panic 前后由同型写通路错误真正写入内存。另需诚实标注一个弱替代解释：若写坏发生在 397.150774 之前且直接落 LLC/内存，则第一次 load(+0x318) 也应读到 3948，与打印矛盾，故该弱解释不成立；打印本身即证明两次读不同值，这一点无可回避。

### 6.5 第五步：排除伪线索（详见 §2），锁定真实异常集合

真实异常只有三个【实锤】：
1. `[ffff6057fffbe998]` 终态 = 3948（写路径 SDC 落盘）；
2. 458 次"第二次读"稳定注入 3948（读路径 SDC 反复表现）；
3. CPU179 `__per_cpu_offset[174]` 单次读零（读路径 SDC，独立实锤）。

其余全部复核为正常值（x9/x5/x10/x12-x17/x3/x24 推进 stride 0x80=order-1 buddy 块等，algebra.py §Q/V/AG/AO/T）。

### 6.6 CPU168/169/180 为何共享同一个 990？

【强推】`pcp = ffff6057fffbe960` 是 `alloc_percpu` 动态区的一份副本（属某 CPU 的 zone pageset）。三核撞同一地址的机制：三核的 rmqueue_bulk 都在遍历同一 zone 的 buddy 并批量摘页，而 `rmqueue_pcplist` 传入的 list 头（x22→sp+64→x19）对该 zone 而言是……诚实说明：x3 证明三核 per-cpu 基址不同，若各自 this_cpu_ptr 应得到不同副本地址。可自洽的解释是该结构并非 per-cpu 副本而是 zone 级共享字段（如 zone->per_cpu_pageset 的 boot/主副本或 pcp batch 结构），或三核 WARNING 中的 x19 本就指向同一共享表头；受 kdump 截断（无写坏前 dmesg）与 PARTIAL DUMP（zone->per_cpu_pageset 指针数组页 excluded，rd 报 page excluded）限制，此点无法进一步实证。这不影响根因结论：无论该表头归属如何，"单字段 8 字节被同类指针替换 + 458 次稳定复现 + 相邻字段完好"的故障签名不变。

## 7. 逻辑链条（写路径证据链与传播建模）【逻辑链条】

### 7.1 故障注入模型：LSU 数据通路"整字替换"

```
                 ┌─ 事件 W（写路径 SDC，dmesg 窗口前发生）──────────────┐
                 │ 一次 64bit store: 目标地址正确(ffff6057fffbe998)        │
                 │ 数据被替换: 990(期望) → 3948(同类 vmemmap page 指针)   │
                 │ 27/64 位翻转 = 整字替换, 非位翻转                       │
                 └──────────────┬────────────────────────────────────┘
                                ↓ 持久化(快照实锤)
   ┌─ 表现 R（读路径 SDC，458 次稳定）──────────────────────────────┐
   │ 同一迭代内两次 load 同地址: 第一次得 990(好), 第二次得 3948(坏) │
   │ → 坏值可被读通路"稳定注入", 与缓存中真实存在的同类指针同源      │
   └──────────────┬────────────────────────────────────────────┘
                  ↓ 传播
   ┌─ 级联 1: rmqueue_bulk list_add_tail 检查失败 ──→ 458 次 WARNING (CPU168/169/180)
   │   (检查失败返回 0 → 跳过插入, 但 bulk 循环继续摘页 → x24 每轮推进 stride 0x80)
   ├─ 级联 2: pcp count 与链表长度脱节 ──→ free_pcppages_bulk 摘到
   │   已删节点(next=POISON1/NULL) ──→ 126 次 list_del WARNING (CPU55/50)
   ├─ 级联 3: CPU179 find_busiest_group 读 __per_cpu_offset[174] 得 0
   │   ──→ x27 = &runqueues 模板(无映射) ──→ ldr [x27,#0x120] L3 fault ──→ panic
   └─ 终态: kdump 冻结内存, 快照 [ffff6057fffbe998] = 3948
```

### 7.2 为什么是"数据替换"而非"位翻转"/"地址错"【强推】

- 27 位翻转（python 实算 good^bad=0x9dd6fec8d0d8）排除 SEL/SEU 单粒子位翻转；坏值是系统真实存在的合法指针（该 page 的 lru.next 还回指坏头，说明 3948 就在本机工作集内活跃）。
- 地址正确：损坏精准落在 list_head 的 prev 字段（8 字节对齐、相邻 16 字节的 next 与其余 16 条 lists 全部完好），store 地址通路正常，错的是数据。
- 读侧同型：CPU179 的读零（64bit 全 0，非部分位错）与 458 次"第二次读注入 3948"（整字替换），读通路同样表现为"整字被替换/屏蔽"，而非位错误。
- 统一模型：单一执行核的 LSU（load/store 共用段）数据通路存在间歇性数据错配，write 方向把另一条 store 的数据写入目标；read 方向把 fill 数据替换为缓存内另一行的值或 0。故障呈"数据选择/使能"型（mux/queue 槽位错位），符合 store-buffer 写合并错配或 LSU fill buffer 数据错配的微架构特征。

### 7.3 为什么 ECC/LLC 校验没拦住【强推，含不可验证声明】

平台为鲲鹏类 SoC（hisi_uncore_l3c_pmu/hha/ddrc 模块在列，LLC/DDR 链路具备 ECC 能力）。数据替换型 SDC 的产生位置在核内（LSU/store buffer/写合并窗口），特点：
1. 被写入的"坏数据"本身是合法的 64bit 模式（真实指针），写入后 ECC 校验位与数据自洽，任何端到端 ECC（LLC/DDR）都无法发现，因为它们校验的是"传输/存储完整性"，不是"数据语义正确性"；
2. 458 次"第二次读"读到坏值而第一次读好值（§6.4）：若坏值经总线进 LLC，MESI 一致性会强制同核两次读同值，故坏值更可能从未经过一致性总线，而是在核内读通路上被就地替换（这也是 ECC 全程无感的原因）；
3. dmesg 全程无 ras/edac/mce 记录【实锤】，硬件错误检测通路沉默，与"核内数据替换"模型自洽。
4. 【推测，不可验证】无法从本 vmcore 区分错误单元精确到 store-buffer 条目错配、写合并(WriteCombining)窗口错配、LSU fill buffer 错配还是 RF 写端口屏蔽，需要 DFT/SCAN 级复现或 RAS 寄存器（本转储未含）。

### 7.4 写坏者身份的推理【强推=单核；推测=CPU179】

- 168/169/180/50/55 全部有实锤的踩雷栈（各自 WARNING 的 Call trace + crash bt -c），且它们的寄存器（x3 等）全部正常，排除这 5 个核是写坏者（至少在可观测窗口内它们的行为完全正常）。
- CPU179 是全 dmesg 唯一表现出独立微架构读错误（读零）的核，且它是 panic 核；读错误与写错误同型（数据替换）同源的可能性最高。
- 【推测】结合 fleet 其他案例的 CPU179 前科（外部背景信息，本案独立证据不足以定罪）：写坏者最可能是 CPU179，写坏时间在 397.150774 之前（kdump 截断区）。但本案内部证据只能支撑"单一执行核的 LSU 数据通路故障"，不能闭环指认 CPU179，诚实声明。

## 8. 故障根因【故障根因】

直接根因（微架构级）【实锤（表现）+强推（机制）】：
单一执行核的 LSU 数据通路发生数据替换型（非位翻转）SDC，一次性事件簇：
1. 写路径：一次 64bit store 的数据被替换为同类指针 `fffffd8101333948`，精准写入动态 per-cpu pageset（pcp=ffff6057fffbe960）的 `lists[1].prev`（ffff6057fffbe998），相邻字段零损伤；
2. 读路径：同一故障在读方向表现为 458 次稳定的"第二次读注入"（同迭代两次 load 同地址不同值）+ CPU179 一次 `__per_cpu_offset[174]` 读零（触发致命 Oops）。

失效模式链：pcp 链表头单字段损坏 → rmqueue_bulk/free_pcppages_bulk 的 list_debug 检查失败（458+126 次 WARNING，5 核级联）→ CPU179 find_busiest_group 读零 → x27 落到无映射 percpu 模板区 → L3 translation fault → panic → kdump。

根因分级：
- 【实锤】故障表现层（上述全部可观测事实）
- 【强推】"核内 LSU 数据通路数据替换型故障"（ECC 全程无感 + 两次读不同值 + 整字替换三点交叉）
- 【推测】精确出错单元（store-buffer 写合并窗口 / LSU fill buffer / RF 写端口之一）；写坏者为 CPU179

排除项【实锤】：软件 bug（list_add_tail 调用与锁语义正常，损坏值不可能是任何合法代码路径写入，空表 prev 只能等于自身）；__per_cpu_offset 数组损坏（170 项实测完好）；页表损坏（同段 .text vtop 正常）；内核映像损坏（所有 ffffb75e 值均为合法符号，crash sym 逐个解析成功）；多粒子/多位翻转（27 位翻转且替换值是活跃工作集指针）。

## 9. 启示【启示】

1. "数据语义级 SDC"是 ECC 的结构盲区：ECC（含 LLC 端到端）保护的是传输/存储比特完整性；本案坏值是真实合法指针，写后校验位自洽，任何位置的传统 ECC 都不可能拦截。对指针密集型内核数据，需要在更高层做不变式校验。本案恰是 `CONFIG_DEBUG_LIST`（list_debug.c 的 next/prev 一致性检查）把一个"静默数据腐坏"变成了 458 次可观测报警并最终在 0.29s 内把系统送到可控 panic。服务器内核应默认开启 list_debug/DEBUG_SG 等不变式检查作为"软件 ECC"，其检出价值在本案被完整证明。
2. "一核写坏、多核受害"对隔离设计的挑战：故障核只发一次坏写，受害者是 5 个无辜核（它们的 WARNING 帧看起来更像"嫌疑人"）。若仅按报错 CPU 做核级隔离/换核，会误杀 5 个好核而放过真凶。隔离决策必须区分"报错核"与"首错核"：对共享结构损坏案，应回溯到损坏结构的写者集合（本例中 pcp 的 owner 核），而不是打印栈上的核。诊断上，"458 次逐字相同"这类内容不变性是判定"持久写坏"（应追写者）vs"瞬态读错"（应追读者）的第一指纹。
3. 同类指针替换签名 → 指向数据通路而非存储单元：27 位翻转 + 替换值在本机活跃工作集内 + 目标地址精确、邻字段完好，这一签名组合把故障定位收窄到 LSU/store 数据选择逻辑，直接指导 DFT 向量设计（针对 store buffer 条目/slot mux 的 MBIST/at-speed 诊断模式），而不是对 SRAM 阵列做修复重配。
4. percpu 模板区"天然 canary"：`&runqueues` 模板段无 L3 映射（双重实测），任何 percpu 变量一旦丢掉 offset 就会立刻以 L3 fault 暴露，这是 ARM64 percpu 实现送的免费断言。微架构上读零错误因此必然显性化（本案 CPU179 的读零 100% 转成 panic 而非静默数据污染），设计上可更多利用这种"结构上不可达地址"作为错误放大器。
5. kdump 截断的法证代价：写坏瞬间证据全部丢失，只能靠终态快照反推。对高频 WARNING（>100 次/秒同签名）应触发提前转储或至少保留 dmesg 完整 ringbuffer 快照：本案若在 397.15 前有一次完整 dmesg，写坏者可直接从首个异常栈定位。
6. 对 FI（故障注入）研究：本案给出"写路径 SDC"的完整真实传播样本：单字段指针替换 → 分配器元数据损坏 → 跨核级联 → 调度器崩溃。gem5-FI 建模时应覆盖：store 数据错配（而非仅位翻转）、注入目标选在 per-cpu 元数据、传播经过"检查失败但循环继续"的路径（本案 458 次重复的机制根源是 WARN 不终止 bulk 循环）。

## 10. 处置建议

立即（运维）：
1. 该节点 CPU179 所在物理核（结合 MPIDR/拓扑拓扑信息）列入重点观察；鉴于写坏者证据等级为【推测】，建议对整个 CPU cluster/CCX 域做一次下线观察（`echo 0 > /sys/devices/system/cpu/cpuN/online` 逐核验证不可行时可整机隔离）。
2. 保留本 vmcore 与 vmlinux 配对归档（vmlinux 含 DWARF，行号级反演已验证可用）。
3. 检查同批次机器（fleet）是否有 CPU179 相关前科案，汇总定位共性。

短期（诊断增强）：
4. 开启 `CONFIG_DEBUG_LIST` 已验证有效，建议同时评估 `CONFIG_SCHED_DEBUG`+`CONFIG_DEBUG_SG`；对 per-cpu 元数据区考虑 canary/影子校验。
5. 在 RAS/固件层拉取 CPU179 的 LL/SLC 错误记录计数器（本 vmcore 未含 RAS 表，需 BMC/带外通道补采）。
6. dmesg 保留策略：panic 转储之外，对"1 秒内同签名 WARNING ≥ 50 次"配置 pstore/trigger 提前快照。

中期（硬件/固件反馈）：
7. 向芯片侧反馈该"数据替换型 SDC"签名（27 位翻转、同类指针、地址精准、读侧同型），请求对 LSU store-buffer 写合并路径的 DFT 覆盖评估。
8. 评估固件级"per-cpu 元数据周期校验"（如定时 walk pcp lists 校验双向一致性）作为在线检测。

## 附录：命令索引

dmesg 分析
```
wc -l vmcore-dmesg.txt                                        # 25361
grep -c "list_add corruption" vmcore-dmesg.txt                # 458
grep -c "list_del corruption" vmcore-dmesg.txt                # 126
grep "WARNING: CPU:" … | 按CPU计数                              # 168:269 169:126 55:63 180:63 50:63
grep -oh "ffffb75e[0-9a-f]*" … | sort | uniq -c                # 584×c5cc520 等 6 个值
python3 (x24 stride 统计, x3/x5/x19 配对, ASCII 解码)            # algebra.py §T/R/AG
```

crash 会话（vmlinux=/tmp/vmlinux-0102；完整输出见 crash_session*.log）
```
sys                                     # 192 CPU, 767.8G, panic=rs:main Q:Reg CPU179
bt                                      # panic 栈: find_busiest_group→load_balance→…
bt -c 168 / 169 / 180 / 50 / 55         # 各受害核栈(fma 用户态/rmqueue/free_pcppages)
bt 69259 / 75192                        # systemd-journal / control 栈
ps | grep arm0102 (192个)               # 压测负载
dis -l find_busiest_group               # +0x134 ldr x20; +0x13c add x27; +0x140 ldr x23
dis -l __list_add_valid_or_report       # +0x18 ldr x2,[x2,#8]; +0x7c 报错分支
dis -l rmqueue_bulk / rmqueue_pcplist / free_pcppages_bulk / vprintk_emit
rd -64 ffff6057fffbe860 40 / ffff6057fffbe9a0 8    # pcp 结构簇 (pcp=fffbe960)
rd -64 fffffd8101333948 8 / fffffd810045b708 8 / fffffd8100422d48 8 / fffffd8100c25588 8
rd -64 ffff8001dfc63740 8               # panic 栈槽 (sp+8/sp+16 基址实锤)
rd -64 ffff8001dfc5b6c0 16              # CPU180 栈帧
rd -64 ffffb75e3e3a55d0 176             # __per_cpu_offset 全数组 (170项完好)
rd -64 ffffb75e3e3a5b38 2               # __per_cpu_offset[174]=ffffc8a243768000
rd -64 ffffb75e3d77ce50 …               # list_debug.c:29 格式串原文
px &runqueues (=0xffffb75e3dfa96c0) / px &__per_cpu_offset / px __per_cpu_offset[N]
vtop ffff6057fffbe990 (phys 6057fffbe990) / fffffd8101333948 (phys 6057a1133948)
vtop ffffb75e3dfa96c0 (PTE=0!) / ffffb75e3c5cc520 (正常)
sym ffffb75e3c5cc520 (vprintk_emit+424) / ffffb75e3e39fcb0 (nr_cpu_ids) / …
struct per_cpu_pages -o / struct rq -o / struct cfs_rq -o / struct page -o
p saved_command_line                    # crashkernel=1024M,high …
```

python 代数：`algebra.py` → `algebra_out.txt`（§A-§AO 共 26 节：x27 代数、FAR 差值、pcp 布局对齐、vmemmap 几何、位翻转分析、两次 load 矛盾、伪线索排除等）

报告文件清单（本目录）：
- `vmcore-diagnosis-report-127.0.0.1-2026-09-04-220949.md`（本报告）
- `dmesg_forensics.txt`（dmesg 法证索引：统计/关键行号/时间窗/坏值统计）
- `algebra.py` / `algebra_out.txt`
- `crash_session.log`（sys/help）、`crash_session2.log`（bt 全家 + 四函数反汇编）、`crash_session3.log`（rd/vtop/px/struct 全家）
- `crash_dis_rmqueue_bulk.log` / `crash_dis_rmqueue_pcplist.log` / `crash_dis_free_pcppages_bulk.log` / `crash_dis_vprintk_emit.log`

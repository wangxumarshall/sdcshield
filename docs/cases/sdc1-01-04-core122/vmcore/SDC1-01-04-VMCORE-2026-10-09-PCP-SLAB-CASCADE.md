# sdc1-01-04 vmcore 取证 ③：2026-10-09 CPU122 pcp/slab 状态污染级联 + 用户态 bit15 检出 → 空格地址致命崩溃

> **报告日期**: 2026-10-10
> **取证对象**: `/home/sdc/vmcore/127.0.0.1-2026-10-09-11:17:53/vmcore`（835,198,409 字节）+ 同目录 `vmcore-dmesg.txt`（2,757 行）
> **崩溃时刻**: 2026-10-09 11:16:03 CST（uptime 5 天 03:42:02 = 445,322 s；转储目录 11:17:53）
> **Panic**: `Unable to handle kernel paging request at virtual address 0020202020202820`
> **Panic 任务**: PID 3226 `irqbalance`（task_struct `0xffff0820aaa1d400`，**CPU 122**，处于自身 coredump 流程中）
> **本文定位**: 对 10-09 vmcore 的三事件级联取证报告。本案是三份 vmcore 中信息量最大的一份：**① 08:09 用户态探测（TEST #1 neon_rot，线程 122）首次数据比对失败；② 10:00/10:13 内核页分配器 pcp 链表两度触发 DEBUG_LIST 告警（E1 在 CPU122、E2 在 CPU19 消费污染状态）；③ 11:15:39 用户态探测（TEST #13 probe_x，线程 122）捕获 32 位字 bit15 单比特损坏（xor=0x00008000）；④ 11:16:03 irqbalance（其用户态先因跳转 NULL 崩溃）coredump 中的 kmalloc 在 CPU122 的 per-cpu freelist 上装出"8 个 ASCII 空格"，致命崩溃——且故障地址与寄存器算出的地址间还有一次 bit 61 单比特丢失**。三小时的退化弧全部锚定在 CPU122。

---

## 0. 结论摘要

1. **E3 致命链（指令级闭合）**：`__kmem_cache_alloc_node` 快路径 +136 装载 `c->freelist`（CPU122 的 kmalloc-4k per-cpu 空闲链头）= **`0x2020202020202020`（8 个空格，内存实测）**；+520 装载 `s->offset = 2048`（`ldr w0,[x19,#40]`，slab.h:421）；+524 `ldr x2,[x20,x0]`（slub.c:410 `get_freepointer`）在 `[空格+0x800]` 崩溃。**x20+x0 = 0x2020202020202820，而 FAR = 0x0020202020202820——顶字节 0x20→0x00，bit 61 单比特丢失（地址通路）**。
2. **空格的出处（对象解剖）**：当前 slab（order-3、8 对象、frozen）上线性地址 `0xffff0820b5288000`：**object0 整个 4 KB 为纯 0x20**；object0/2/3 的 freepointer 槽（+0x800/+0x2800/+0x3800）**全部是 8 个空格**——上一弹出的分配把 object0 槽里的空格当链指针读走并 casp 写入 `c->freelist`。**object1 是活着的 coredump vma_meta 数组**（48 字节条目 {start,end,VM_flags,dump_size,file*}，irqbalance 的 VMA 含 vdso 0xffff9c000000）——崩溃分配的前序兄弟分配就在同一 slab 上。
3. **CPU 特有性**：**CPU 119/120/121/123 的 kmalloc-4k freelist 全部合法**——唯独 CPU122 = 空格。损坏是 CPU122 私有的 per-cpu 状态。
4. **E1/E2（pcp 链表）**：E1（10:00:48，kworker/122:2，vmstat_update→free_pcppages_bulk）`list_add` 合法性检查读到 `next->prev = 0xffff082ffffcd980` ≠ 期望 `0xffff082ffffc5980`——**恰差 bit 15（0x8000）**；E2（10:13:42，kworker/19:1）`list_del` 发现 E1 同一页面 `fffffc20843bc308->next` 为 LIST_POISON1——污染状态跨 CPU 消费。DEBUG_LIST 两次拦截使批实验存活到 E3。
5. **用户态位级检出（全案最重的证据）**：
   - TEST #1 `neon_rot_ldr_at_top_rowmajor_sve` 迭代 156（**08:09:24，线程 122**，time-to-fail 986.8 ms）：data miscompare（`fail.cpu-mask` 的 X 恰在 122 位）；
   - TEST #13 `movbe_dump_probe_x` 迭代 42（**11:15:36 起跑、3.32 s 处失败，线程 122**）：`index 970: input=0x9738C0D8 golden=0x9738C0D8 actual=0x973840D8 xor=0x00008000`——**32 位字 bit 15 单比特清除，距内核致命崩溃仅 22 秒**。
   - **bit 15（0x8000）三连发**：9-29 x21（0x…f6c0→0x…76c0）、本案 E1 指针（0x…5980→0x…d980）、本案用户态字（0xC0D8→0x40D8）——缺陷存在**位位置偏好**。
6. **irqbalance 的先导崩溃**：其用户线程 PC=LR=X29=0（`el0_ia` 指令中止于地址 0，`thread.fault_address=0`）→ SIGSEGV → do_coredump → fill_files_note `kvmalloc(round_up(count×64,4096))` → E3。跳 NULL 可能是其自身缺陷，亦不排除坏核污染其控制流（用户内存不在转储中，无法进一步判定——如实记录）。
7. **x12-x17 的 printk 文本不是替换证据（严谨判别）**：E1（CPU122）与 E2（健康 CPU19）的告警寄存器组呈**完全相同**的"消息文本在 x12-x17"模式——该文本为 vsnprintf/记录提交路径的正常分块拷贝残留（含格式化参数文本 "ffff082f"/"e prev ("，且含时间戳数字残字），两核一致 ⇒ **代码伪影，排除**。
8. **微架构归因**：读侧"陈旧行交付"（freeptr 槽读到旧文本）或写侧"freeptr 存储未落点"（三次）+ 地址生成单比特丢失（bit 61）+ pcp 链表位错（bit 15）+ 用户态数据通路位错（bit 15）——**同一坏核在 3 小时内以多种形态反复表现，全部为核内事件**。

---

## 1. 环境与实验上下文

### 1.1 启动与 CPU122 生命周期

| 项 | 值 | 证据 |
|---|---|---|
| 本次启动 | 2026-10-04 07:34:01（uptime 445,322 s 倒推） | crash `sys` |
| 启动期 | maxcpus=122 生效；**123-127 于 35.9-36.4 s 上线但顺序为 123→125→127→124→126（乱序）且 122 未上线** | dmesg `Booted secondary processor` 序列 |
| 上线机制观察（开放项） | 乱序提示 udev 冷插拔通道当时仍活跃，但 122 未被拉起（无失败日志；该 boot 的用户态 journal 未持久化，无法定论）；对照：当前 10-09 23:49 boot（规则已注释）呈 123→124→125→126→127 顺序（service 特征） | 本机 journalctl 对照 |
| **CPU122 上线** | **2026-10-08 16:38（uptime 378,215 s），实验脚本按协议上线（MPIDR 0x0900060200 核对 + 封顶 2.0 GHz + performance）** | dmesg 行 2631-2635 + 批次脚本预检 |
| 批实验 | `run-freq2000-allcore-batch-20261009.sh`：40 项 × 15 min，**全核心 128 CPU**（122 封顶 2.0 GHz、其余自由 ≤2.9 GHz），08:05:45 BATCH_START；每项预检在线/封顶/MPIDR | STATUS.txt + 脚本 |
| 崩溃窗口 | TEST #13 = `movbe_dump_probe_x`（序列第 13 项），11:11:21 开跑；#12 于 11:11:21 结束 | STATUS.txt |

### 1.2 三小时事件弧（全部时间 CST）

| 时刻 | 事件 | 主体/CPU | 证据 |
|---|---|---|---|
| 08:09:24 | **TEST#1 neon_rot 数据比对失败**（迭代 156，time-to-fail 986.8 ms，cpu-mask X@122） | 用户态，线程 **122** | neon_rot yaml 行 1245-1262 |
| 08:25:45 | TEST#1 结束（rc=124 20 min 兜底，detected=1，日志保留） | — | STATUS.txt |
| 10:00:48 | **E1：list_add corruption 告警**（`next->prev` 期望 `0xffff082ffffc5980` 实读 `0xffff082ffffcd980`） | kworker/**122**:2 | dmesg 440648s |
| 10:13:42 | **E2：list_del LIST_POISON1 告警**（`fffffc20843bc308->next` = POISON1，同一页面！） | kworker/19:1（消费侧） | dmesg 441542s |
| 11:11:21 | TEST#13 probe_x 开跑 | 全核 | STATUS.txt |
| 11:15:36+3.3s | **用户态 bit15 检出**：`index 970 xor=0x00008000`（迭代 42，线程 122） | 用户态，线程 **122** | probe_x yaml/console |
| 11:15:41 | 迭代 43（retry）起跑 | — | probe_x yaml |
| ~11:16 | irqbalance 用户态跳 NULL（PC=0）→ SIGSEGV → coredump 开始 | irqbalance | E3 反推 + `thread.fault_address=0` |
| 11:16:03 | **E3 致命崩溃**（fill_files_note 的 kmalloc） | irqbalance/**CPU122** | dmesg 445322s |

---

## 2. E1/E2：页分配器 pcp 链表污染（dmesg + 反汇编）

### 2.1 E1（440,648 s，CPU122）

```
list_add corruption. next->prev should be prev (ffff082ffffc5980), but was ffff082ffffcd980. (next=fffffc20828fc708).
WARNING: CPU: 122 PID: 18429 at lib/list_debug.c:29 __list_add_valid_or_report+0x8c/0xe0
Workqueue: mm_percpu_wq vmstat_update
  free_pcppages_bulk+0x4e8 → drain_zone_pages → refresh_cpu_vm_stats → vmstat_update
```

- 反汇编定位：`free_pcppages_bulk+1252: bl __list_add_valid_or_report`（+0x4e8 为返回地址），调用点为 page_alloc.c:697 内联的 `list_add(&page->pcp_list, &pcp->lists[pindex])` 邻域——**CPU122 本地 percpu pageset 的 pcp 链表**。
- 位级：`0xffff082ffffcd980 − 0xffff082ffffc5980 = 0x8000`——**bit 15 单比特差**。DEBUG_LIST 拦截了这次插入（内核 6.6 该检查失败即放弃 add）。
- 现场 x19/x27 = `fffffc20843bc300`（被插入页面）——该页面在 E2 复现。

### 2.2 E2（441,542 s，CPU19）

```
list_del corruption, fffffc20843bc308->next is LIST_POISON1 (dead000000000100)
WARNING: CPU: 19 PID: 671 at lib/list_debug.c:56 __list_del_entry_valid_or_report+0xa8/0x110
Workqueue: events delayed_vfree_work → vfree → free_unref_page → free_pcppages_bulk
```

**E1 欲插入的同一页面（fffffc20843bc300）在 894 秒后于 CPU19 的链表走查中呈现"已 poison 但仍被链接"**——被污染的链表状态跨 CPU 传播消费。两次告警均未升级（panic_on_warn=0，捕获配置 §3.4），系统存活至 E3。

### 2.3 两个页面的终态（dump 时，E2 后 63 分钟）

```
struct page fffffc20843bc300: refcount=0, mapping=0, _mapcount=-129, lru 挂链（next=fffffc2084666b08/prev=fffffc2083b1dc08）— 已回到某空闲链
struct page fffffc20828fc700: refcount=0, _mapcount=-1, lru 挂链 — 同为空闲态
```
状态已被后续分配器活动覆盖，无法回溯瞬时值——以 dmesg 告警原文为准。

### 2.4 x12-x17"消息文本"的正确解读（排除伪证据）

E1/E2 的告警寄存器 x12-x17 含告警消息文本片段（`"446250]["`、`"T18429] "`、`"should b"`、`"e prev ("`、`"ffff082f"`、`"ffc59803"`）。**E1（坏核 122）与 E2（健康核 19）呈现同一模式**，且文本含格式化中间产物（参数的十六进制文本、时间戳数字残字）——为 vsnprintf→记录提交路径的正常分块装载残留（代码伪影）。**不作为替换证据**。真正可归因的是上面 §2.1 的位差与 §3 的 slab 状态。

---

## 3. E3：致命崩溃的指令级解剖

### 3.1 崩溃签名

```
[445322.204866] Unable to handle kernel paging request at virtual address 0020202020202820
  ESR = 0x96000004 (EC=0x25 DABT current EL, WnR=0 读, FSC=0x04 level-0)
  [0020202020202820] address between user and kernel address ranges
CPU: 122 PID: 3226 Comm: irqbalance  Tainted: G W
pc : __kmem_cache_alloc_node+0x20c/0x3c0   lr : __kmalloc_node+0x64/0x1d0
x20: 2020202020202020   x0 : 0000000000000800   x21: 0000000000000cc0   x22: 0000000000001000
x8 : 0101010101010101   x7 : 7f7f7f7f7f7f7f7f    ← SWAR 常量（fill_files_note 路径 strlen 残留，合法）
Code: a8c57bfd d50323bf d65f03c0 b9402a60 (f8606a82)
```

小端解码：`0x0020202020202820` = `" (      "`（空格、左括号、六空格）——**故障地址本身是文本**。

### 3.2 调用链（bt）与 irqbalance 的先导崩溃

```
#12 __kmem_cache_alloc_node ← #13 __kmalloc_node ← #14 kvmalloc_node
#15 fill_files_note ← #16 fill_note_info ← #17 elf_core_dump ← #18 do_coredump
#19 get_signal ← #20 do_signal ← #21 do_notify_resume ← #22 el0_ia ← #23 el0t_64_sync
EL0 帧：PC=0x0  LR=0x0  X29=0x0   ORIG_X0=9   PSTATE=80001000
```

**irqbalance 先因用户态指令中止于地址 0 而收到 SIGSEGV**（`el0_ia`；`thread.fault_address = 0` 实证；PC=LR=X29=0 的控制流损坏形态），进入 coredump；`fill_files_note`（fs/binfmt_elf.c:1692）按 `size = round_up(count×64, PAGE_SIZE)` 申请 **4096 字节**——一次 kmalloc-4k。

### 3.3 快路径反汇编与两个位级异常

```
+136: ldr  x20, [x5, x0]        ; x20 = c->freelist（CPU122 的 per-cpu 链头）   ← 装出 8 个空格
+520: ldr  w0, [x19, #40]       ; w0  = s->offset                                ← = 2048 (0x800)
+524: ldr  x2, [x20, x0]        ; ★get_freepointer：读 [object + 0x800]          ← 故障指令 (f8606a82)
+556: casp x0,x1,x2,x3,[x4]     ; this_cpu_cmpxchg_double（未及执行）
```

**异常一（freelist 内容）**：`c->freelist` 内存实测 = `0x2020202020202020`。
**异常二（地址通路 bit 61）**：寄存器算得 `[x20+x0] = 0x2020202020202820`，而 FAR = `0x0020202020202820`——**顶字节 0x20→0x00（bit 61 单比特丢失）**，发生在操作数→地址→MMU 之间（或异常入口保存 x20 时位翻转为 0x20——两者皆为核内单比特事件）。

### 3.4 cache 身份与 slab/对象解剖

```
struct kmem_cache 0xffff082088004d00：
  name = "kmalloc-4k"   size = 4096   object_size = 4096   offset = 2048 ✓   align = 4096
  （CONFIG_SLAB_FREELIST_HARDENED 未开启 → freeptr 为裸指针；FREELIST_RANDOM 开启）
c（per-cpu kmem_cache_cpu，CPU122）：tid=4522  slab=0xfffffc2082d4a200  partial=0xfffffc2083a2dc00
  slab：order-3（oo.x=0x30008，8 对象）、inuse=8、frozen=1、slab->freelist=NULL、refcount=1
  slab 线性地址 = 0xffff0820b5288000
对象解剖（+0x800 步进为 freeptr 槽）：
  object0 +0x800 : 2020202020202020   ← 空格；object0 全 4 KB = 纯 0x20（512 字实测）
  object1 +0x1800: 0x0f,ffff08209b098500,…   ← 对象本体 = coredump vma_meta 数组（活数据）
  object2 +0x2800: 2020202020202020   ← 空格；头部 512 B 亦纯空格
  object3 +0x3800: 2020202020202020   ← 空格
  object4-7：零/其他数据
```

**object1 内容鉴定**（48 字节条目循环）：`{start=0xaaaadf920000, end=0xaaaadf933000, VM_flags=0x75(RW+MAY*), dump_size=0x1000, file=0xffff08209b09ba00}` … 含 vdso 映射 `0xffff9c000000-0xffff9c021000`——**irqbalance 的 VMA 快照（dump_vma_snapshot 的 kmalloc），coredump 自己的前序分配，仍然活跃**。

### 3.5 机制重构

```
（E1/E2 时段，10:00-10:13）CPU122 的 pcp/分配器状态被污染（bit15 位错），
   页面级双重状态开始在分配器内流转
（某个早前时刻）kmalloc-4k 的 CPU122 per-cpu 空闲链被装入"陈旧内容"：
   object0/2/3 的 freeptr 槽保留旧文本（8 空格）而非链指针
   ——或为 freeptr 存储未落点（写侧丢失），或为对象双重分配后被文本覆写（读侧取出旧行）
（11:15:36+3.3s）TEST#13 线程 122 用户态字 bit15 损坏被检出（同核数据通路活跃故障的旁证）
（~11:16）irqbalance 用户态跳 NULL → SIGSEGV → coredump：
   dump_vma_snapshot 成功拿走 object1（vma_meta 数组）
   fill_files_note → kvmalloc(4096) → 快路径弹出 object0：
     get_freepointer(object0) = [object0+0x800] = 空格 → casp 把空格写入 c->freelist
   （下一次分配）→ 弹出"空格"：+136 装出 c->freelist=空格
     +524: ldr x2,[空格+0x800] → 地址再丢 bit 61 → 0x0020202020202820 → level-0 翻译故障 → panic
```

### 3.6 空格文本的来源（如实记录的开放项）

内核常规路径无 4 KB 纯空格填充（fs/proc、kernel、mm、drivers/tty 的 `memset(…,' ',…)` 检索为零命中）。候选：slab 页前世的页缓存文本（实验 yaml/日志等空白重的文件）或双重分配对象的用户态驱动填充。转储未含用户页与完整页缓存，无法锁定——**该开放项不影响归因：无论文本来自何处，"freeptr 槽保留旧文本/被旧文本覆写"本身即核内读写通路故障**。

---

## 4. 备择假设与排除

| # | 假设 | 排除依据 |
|---|---|---|
| 1 | 全局 slab/页分配器软件 bug | CPU 119/120/121/123 同 cache freelist 全部合法；损坏为 CPU122 私有 per-cpu 状态；kmalloc-4k 为最通用 cache，全局 bug 应遍地开花 |
| 2 | DEBUG_LIST 误报 | E1 的两值恰差 0x8000（单比特）——真实不一致；E2 的 POISON 亦真实 |
| 3 | DDR 位翻转 | 8 个空格是"合法码字"整体，非位翻转形态；且 ECC 静默；同核用户态字损坏（bit15）同窗口出现 |
| 4 | irqbalance 自身 bug 引发一切 | 其 SIGSEGV（PC=0）只解释 coredump 的发生，不解释 pcp 位错（E1 早于其 76 分钟）、邻居 freelist 健康、bit15 三连发 |
| 5 | x12-x17 文本=替换证据 | 健康 CPU19 同模式（§2.4），代码伪影 |
| 6 | crash/工具伪影 | 所有关键值经 rd 原始内存复核；FAR/寄存器差（bit61）以 dmesg 原文为准 |

---

## 5. 微架构归因与置信度

**归因**：CPU122 核内数据通路多种形态的连续表现（3 小时窗口）：
- **读侧陈旧行交付**（freeptr 槽读出旧文本）或**写侧存储丢失/未落点**（freeptr 从未写入或被覆写）；
- **地址生成/寄存器保存单比特丢失**（bit 61）；
- **pcp 链表位错**（bit 15）；
- **用户态数据通路位错**（bit 15，探测直接捕获）。
全部落在核内私有域（per-cpu 状态 + 执行流），无共享层（LLC/DDR/互连）证据；与 9-17 报告 D1/D2 族一致，且首次获得**用户态位级签名**（xor=0x8000）。

| 维度 | 评估 |
|---|---|
| P1 单核浓度 | E1/E3/E 两 次用户态检出全部 CPU122/线程122；E2 为污染的消费侧；邻居全健康 |
| P2 复发 | 同一 boot 内 5 个事件（08:09/10:00/10:13/11:15/11:16）+ 跨会话（9-29/9-30/9-17） |
| P3 RAS 静默 | 全 boot 零 ECC/RAS 事件（空格/位错皆为 ECC 无感形态） |
| P5 加权 | 位错三连发（bit15×2 + bit61）、陈旧文本替换、嵌套故障（pcp→slab→coredump）、用户态独立佐证 |

**置信度：高。** 本案同时给出频率数据点：**坏核在 2.0 GHz 封顶下同样活跃出错**（既有结论：2.9 GHz 即崩、1.45 GHz 可存活数小时-数天）。

---

## 6. 附：证据索引

| 证据 | 命令/来源 |
|---|---|
| 三事件 dmesg | `vmcore-dmesg.txt` 行 2641-2757 |
| E3 反汇编 | `dis -l __kmem_cache_alloc_node`（+136/+520/+524/+556） |
| cache/对象 | `struct kmem_cache 0xffff082088004d00`；`p c->freelist/slab`；`rd 0xffff0820b5288000 512` 等 |
| 邻居对照 | `p ((struct kmem_cache_cpu *)(&cpu_slab + __per_cpu_offset[119/120/121/123]))->freelist` |
| irqbalance 先导崩溃 | bt EL0 帧（PC=0）+ `p task->thread.fault_address` = 0 |
| 用户态检出 | `2026-10-09-freq2000-allcore-batch/{neon_rot_…sve.yaml, movbe_dump_probe_x.yaml/.console, STATUS.txt}` |
| 内核源码 | `mm/slub.c`、`mm/slab.h`、`mm/page_alloc.c`、`fs/binfmt_elf.c:1692`、`lib/list_debug.c` |

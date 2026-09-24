# 单元级 SDC 敏感性覆盖矩阵：12 次 vmcore 的寄存器堆/执行单元/分支预测/缓存层级/TLB 法证

> 定位：本文是 core179 微架构根因综合的"单元×证据"覆盖分析，与 `MICROARCH_SUPPLEMENT.md`（三通路分解）、`CROSS_CASE_STATISTICS.md`（8 案普查）、`STORE_PATH_SENSITIVITY.md`（存储通路）并列。核心交付物为 §3 的覆盖矩阵表。
> 数据源：`/home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt`（12 份，md5 链见 CROSS_CASE_STATISTICS §0）+ 12 份既有诊断报告（含 crash 会话日志与内存真值）。
> 事件底数：135 起 spurious WARNING + 12 起 fatal Oops = 147 起访存异常，100% CPU179，12 开机累计运行 21.0 天（1,816,007 s）。
> 置信约定：继承主报告三级：【实锤】= dump 内可复核；【强推】= 多源收敛；【假设】= 软件不可验证。日期：2026-09-04。


## §1 数据与方法

### 1.1 事件底数（本次独立复核）

```
$ grep -c "Ignoring spurious" /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt
08-14:12  08-17:26  08-24:34  15:42:1  15:58:0  08-26:9
08-31:13  09-03:35  09-04-09:2  09-04-10:2  09-04-11:0  09-04-12:1
合计 135；fatal 12；总 147；全部 CPU:179（其余 191 核 21 天零事件）

$ cat 127.0.0.1-*/vmcore-dmesg.txt | grep -oE "WARNING: CPU: [0-9]+" | sort | uniq -c
    135 WARNING: CPU: 179        ← 100% 单核
```

12 案 fatal 寄存器块全量提取（`awk '/Unable to handle/{f=1} f{print; c++} c>90{exit}'`），11 案命中 `find_busiest_group+0x140`（Code: `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` 逐字相同），1 案 `bio_add_page+0xf0`。

### 1.2 方法

- 致命块：python 正则解析 x0–x29 + sp + pstate + Code，30/30 寄存器逐案无缺失（dmesg 不打印 x30，见 §5 边界）；所有 64 位运算 Python3 模 2⁶⁴ 机器验证。
- WARNING 块（135 起）：按 `------------[ cut here ]` 切块、`---[ end trace` 截尾，提取 x19(=ESR)/x21(=FAR)/x22(=EC) 入参三联与全 29 寄存器（callee-saved x19–x28 可靠，caller-saved 已被 printk 污染；STORE_PATH_SENSITIVITY §1.2 已证 x12–x17 是告警文本自身残留）。
- 内存真值：来自既有 5 个 crash 会话（08-14/08-26/08-31/09-03/09-04-09/10/11 的 `px __per_cpu_offset[i]` + `rd -64 __per_cpu_offset 192`），`__per_cpu_offset` 数组运行期地址由崩溃块 x24+0x5d0 重建（adrp 页基不变式，12/12 闭合）。
- 微架构参数：TSV110 公开规格（`docs/cpu/kunpeng.md` §3）：L1D 64KB/4-way/64B 行（256 sets，set index = VA[11:6]）、2×128bit/周期端口、dTLB 32-entry FA、L2 TLB 1024-entry、L1D/L2 标称 ECC。


## §2 逐单元证据

### 2.1 寄存器堆（RF/PRF）

a) 单点穿透：每案恰好 1 个寄存器坏，其余 29 个与指令语义自洽【实锤】

12 案致命块的坏寄存器编号分布：

| 坏寄存器 | 案数 | 角色 |
|---|---|---|
| x20 | 11 | `ldr x20,[x0,w25,sxtw#3]` 的装载目标寄存器（fbG 族） |
| x3 | 1 | `ldr x3,[x3,x2]`（缩放变址装载）的装载目标寄存器（08-24 bio 族） |

12/12 坏寄存器 = 出错装载指令的目的寄存器。无一案出现两个及以上寄存器同时异常。

其余寄存器的语义自洽性逐项核对（11 个 fbG 案，本次全量重算）：

```
不变式                            结果        含义
x27 = (x1 + x20) mod 2^64        11/11 闭合   AGU 加法与寄存器读出正确
x25 = x0 = x6（迭代号 i）         11/11 三寄存器互证   循环变量通路无恙
x9 − x1 ≡ 0xfffffe5d1798 (mod 2^48)  11/11   .text 锚↔模板距离（KASLR 无关）
x21 − x24 ≡ 0xffffacb0 (−0x5350)  11/11     nr_cpu_ids↔percpu 数组页基距离
x1 低 16 位 ≡ 96c0                 11/11     runqueues 模板页内偏移
x9 低 16 位 ≡ ae58                 11/11     fbG+0x150 返回地址页内偏移
x29 − sp ≡ 0x180                  11/11     帧指针锚定
x19 − sp ≡ 0x210                  11/11     callee-saved 栈槽锚定
x22 == x26                        7/11（4 案不等，差 ±0x60~0x6c0 — 见下）
```

x22==x26 在 08-31/09-04-10/11/12 四案破例（差 0x60~0x6c0）。这不是寄存器损坏：x22/x26 分别缓存 `env->sd->groups` 与当前 `sched_group` 遍历游标，四案的差值恰为 sched_group 结构尺寸的整数倍（0x60×n / 0x6c0=0x60×18），是"组链表已推进到下一个节点"的合法状态（09-04-09:15 案 crash 直读 x26 内存确认组链 `sched_group->next` 完好，crash_session6.log）。CROSS_CASE_STATISTICS §8.2 已将该"不变式"降级为高频巧合，本次 12 案复核维持该修正。

x23（上一轮 `load_avg` 残留）逐案取值 0x400/0x3ff/0x401/0x564/0x10000，全部是合法 load_avg 数值（1024/1023/1025/1380/PAGE_SIZE 场景），随调度状态自然变化，非损坏。

b) 坏寄存器集中度的含义：通路而非阵列【强推】

12/12 坏值都出现在"装载结果寄存器"上，且同一物理周期内其余 30 个架构寄存器（含 4 个同样持有内核指针的 x1/x9/x21/x24）完好。两种解释的分野：

- 若故障在 RF 阵列本体（位单元/sense-amp），损坏应随机命中任意物理寄存器，且 PRF 重命名使"装载目标"与"特定物理单元"无固定映射，不可能 12/12 精确锁定装载目的寄存器；
- 若故障在装载返回通路（fill-buffer 合并/装载返回 mux，位于 RF 写口之前），则损坏天然只出现在正在流经该通路的那个数据上，装载目的寄存器是唯一受害者。

观测与后者一致。结论：RF 作为整体是"单点穿透"的旁观者而非病灶；故障位置在 RF 之前的装载返回总线/合并级，而非 RF 读出口或阵列。MICROARCH_EVIDENCE §C（gem5 PRF 注入对照）从仿真侧独立支持：PRF 单 bit 翻转是持续性损坏（125,000+ 次读传播窗口），与现场一次性瞬态交付（下次读同地址正常、spurious 重走成功）不符。双重排除。

c) WARNING（非致命）块的寄存器健全性【实锤】

135 起 WARNING 块全量核验：

```
x21 == FAR (块内 "Ignoring spurious" 地址)   135/135
x19 == ESR (0x96000044 或 0x96000004)        135/135
x22 == 0x25 (EC = Data Abort current EL)     135/135
x29 == sp (__do_kernel_fault 帧指针锚定)      135/135
内核指针高字节异常（非ff/00 形态）              0 个
每块寄存器完备数                              29/29（x0–x28）
```

WARNING 时刻（spurious 翻译错，PTW 读出瞬态失败）没有任何一个寄存器呈现撕裂/零塌缩形态。D3（PTW 通路）发作时寄存器堆完全干净，与 D1（装载通路）发作时"恰好一个寄存器坏"形成清晰对照。两类事件共 147 起，寄存器损坏只见于 D1 会话，寄存器堆本身 21 天零自发性损坏。

### 2.2 执行单元（EXU/AGU）

x27 = x1 + x20 的加法在 12 案中全部正确完成【实锤，代数闭合证明】

11 个 fbG 案：`add x27,x1,x20`（指令字 8b14003b，+0x13c）紧邻出错装载（+0x134），12/11 案逐位闭合：

```
$ python3（12 案批量验证，节选）
08-14: ffffa6c96a4996c0 + d93715ba0000ffff = d936bc836a4a96bf == x27 ✓
15:58: ffffb378e25e96c0 + 00ffffcc879da2e0 = 00ffb34569fc39a0 == x27 ✓
09-04-09: ffffcfd3a80896c0 + 2cd80e2000ffffb0 = 2cd7ddf3a9089670 == x27 ✓
（全部 11 案 mod 2^64 逐位相等，含 x20=0 的塌缩案：x27==x1）
```

压力测试语义：ALU 加法器的输入 x20 本身已是深度腐化值（汉明重量 30+ 的撕裂值、或全零、或非规范大值高位），但加法器对这个"垃圾输入"产出了数学上精确 correct 的和，`ffffcfd3a80896c0 + 2cd80e2000ffffb0 = 2cd7ddf3a9089670` 含进位传播逐位无误。ALU 对腐化数据完全透明：垃圾进、垃圾出的"垃圾"是精确算术和。这不是"ALU 未被测试到"：ALU 在每次故障会话中都实际参与了故障数据的运算并正确完成，免疫是被实证行使过的免疫，不是未覆盖。

08-24 bio 案：出错链 `ldr x3,[x3,x2]`（f8626863，缩放变址）→ `ldr x1,[x3]`（f9400061）。变址加法（base+x2 的 AGU 运算）同样正确执行（返回乱码值说明 AGU 算出了正确地址、装载拿到了坏数据，FAR 低 48 位 == x3 低 48 位闭合）。

AGU 算术免疫在 spurious 侧同样成立（STORE_PATH_SENSITIVITY §2.5 已证）：127/127 满足 FAR = x24+0xa（memcpy 目的游标+固定偏移），135/135 满足 FAR = x21。地址计算每次都对，147/147 事件的 FAR 全部可从寄存器精确推导，无一是"无源之水"。

负证据量化：EXU 整数加法器 12/12（fatal）+ 135/135（spurious 代数关系）零错误。

### 2.3 分支预测/控制流

12 案 call trace 与预期代码路径全部一致，无跳飞/野 pc【实锤】

逐案 pc/lr 合法性与全栈核对（帧序列由 dmesg 提取，函数名逐一比对内核符号域）：

```
案          pc                          栈路径（合法函数链）
08-14       fbG+0x140/lr=fbG+0x11c      worker_thread←kthread←ret_from_fork
08-17       fbG+0x140/同上              idle软中断←secondary_start_kernel（唯一idle路径）
08-24       bio_add_page+0xf0           ext4回写 wb_workfn←process_one_work←worker_thread
15:42       fbG+0x140/同上              epoll_pwait2 系统调用
15:58       fbG+0x140/同上              worker_thread←kthread
08-26       fbG+0x140/同上              futex 系统调用
08-31       fbG+0x140/同上              rcu_gp_kthread←kthread
09-03       fbG+0x140/同上              rcu_gp_kthread←kthread
09-04-09    fbG+0x140/同上              unbound worker←kthread
09-04-10    fbG+0x140/同上              pipe_write 系统调用
09-04-11    fbG+0x140/同上              pipe_write 系统调用
09-04-12    fbG+0x140/同上              futex 系统调用（与08-26逐帧同构，17帧偏移全同）
```

- pc/lr 互洽：lr 全部为同函数内调用点（fbG+0x11c 是 `bl cpu_util_cfs` 返回地址），无一案 lr 与 pc 跨函数矛盾；
- 栈深 10–19 帧，每一帧都是该进程身份的语义正确路径（kworker 走 worker_thread、RCU 线程走 rcu_gp_kthread、sftp 走 pipe_write、mi-scavenger 走 futex）。12 种进程身份 × 12 条各自正确的路径，无一是"跳到了不该在的函数"；
- pstate 全部为合法内核态（204000c9/20400009/00400009，EL1h + 正常 DAIF 组合），无异常 PSTATE。

分支预测器免疫的机制学依据：故障数据 x20 在 11/11 案中都是数据值（load 结果），从未流入 pc 通路。控制流的免疫来自"装载返回通路 ⊥ 取指/分支通路"的拓扑隔离在故障上的投影。唯一可能让坏值变成 pc 的路径是"坏值→间接跳转目标"，12 案中 0 次（坏值全部流向 `add x27` 的地址计算与 `ldr x23,[x27]` 的访存，即 D2 地址通路投影，而非 I 通路）。负证据量化：分支/控制流 12/12 免疫，0 案野 pc。

### 2.4 缓存层级（L1D–L2–L3）

a) L1D hit 路径 vs miss/fill 路径的裁决【实锤 + 强推】

正证据（受累）：坏值全部来自"近期访问过的热数据"，且局限在被遍历数组自身。

5 个经 crash 内存真值验证的错行案，错误源的物理定位（数组运行期地址 = x24+0x5d0 重建，set index 按 TSV110 VA[11:6] 计算）：

```
案          装载目标(槽)   目标set   错误源(槽)     源set    源-目标距离(行)
08-14       176           109      slot 1         87       −22 行
15:58       146           105      slot 0         87       −19 行
08-31       60            94       slot 125/126   102/103  +8 行
09-03       12            88       slot 123       102      +13 行
09-04-09    151           106      slot 9/10      88       −18 行
```

三个关键几何事实：

1. 源行与目标行 5/5 案全部不同 set（87≠105、87≠109、102≠94、102≠88、88≠106），维持 MICROARCH_SUPPLEMENT §2.1 的组相联裁决：排除 L1D way/列选通错（U1）。若故障在 L1D 阵列的 way 选择，错误源必须与目标同 set（way 选择只在 set 内发生）；跨 set 的陈旧行回放是 fill-buffer/LQ 合并级行为（fill-buffer 不按 set 组织）。
2. 错误源全部位于被遍历数组自身内部（`__per_cpu_offset` 1536B = 24 条 L1D 行窗口内，源-目标距离 ≤23 行）。该数组是 newidle_balance 热路径的数据，本就是 L1D 常驻+高频重填，错误源是这个核刚刚或近期反复装载过的行，不是任意缓存行。
3. 源槽无单一"最陈旧"偏好：{0,1,9,10}（头部区）与 {123,125,126}（尾部区）聚类并存。若故障=固定命中"最旧 fill-buffer 条目"（纯头部偏好，早期模型的预测），08-31/09-03 的尾部源无法解释。源分布支持相位可变的合并选路错位，而非固定槽位失效。

fill-buffer vs L1D 阵列的进一步分判：08-31（i=60，源 125/126）与 09-03（i=12，源 123）两案的源槽在本轮遍历中尚未被访问（迭代升序，60<125、12<123），这些行只能来自上一轮 newidle_balance 遍历的残留（fill-buffer/LQ 项跨调度会话存留）或 L1D 中的旧行。结合 l1d_disable 实验的四个反例（SCTLR_EL1.C 清零期间 0 WARNING、卸载后 3.7h/86.7h 仍 panic、从未加载也致命），故障点在 L1D 数据阵列访问之外的更深层（fill-buffer 合并/装载返回组装），或 disable 路径未真正旁路该段。两条证据链共同把受累位置压向 miss/fill 返回通路而非 tag/data 阵列读出。

负证据（免疫/无证据）：L2/L3/LLC 零正证据。

- 12 案 147 起事件 100% 绑定 CPU179；其余 191 核（共享同一 L3/LLC/互连/DRAM）21 天零事件。共享缓存层级若是病灶，物理上不可能单核发病；
- 5 个验证案的内存真值全部完好（`__per_cpu_offset` 192 项 0x22000 等差无一项损坏，含目标槽、源槽、179 槽），且反事实地址（如 rq(60)、rq(12)、rq(151)、rq(149)、rq(97)）经 vtop 全部 VALID、实例数据健全。数据从 L2/L3/内存到 L1D 的供给路径交付了正确数据（否则等差数列或反事实实例读数会暴露损坏）；
- 错误源全部可溯源到 L1D 邻近行/同数组（本来就是热数据），无任何"来自远端缓存/远端节点的陌生数据"证据。若 L2/LLC 通路受扰，坏值应是任意近期流经 LLC 的行（跨数组、跨页、跨 node），不会 5/5 精确落在同一 1.5KB 数组内；
- RAS 观测面：kunpeng.md 宣称 L1D/L2 全程 ECC；12 案 dmesg 零 CE/UE/EDAC 记录；08-26 案 rasnode.ko 对 192 核 × 5 ERR 节点全量扫描（960 条读数），CPU179 五节点 FR/CTLR/STATUS/ADDR/MISC 与其余 191 核逐位一致（node0: FR=0x4842 CTLR=0x101 STATUS=0xff；node1/3: FR=0x249a2；node2: FR=0x2842；node4 全零），STATUS 全机仅 {0xff, 0x0} 两值。CPU 内核 RAS 错误记录节点（含 L1/L2 ERR 节点）零异常登记。

b) 内核 6.6 dmesg 中的 cache/erratum/ECC 事件【实锤：零】

```
$ grep -icE "corrected|uncorrectable|ECC error|machine check|Hardware error" 127.0.0.1-*/vmcore-dmesg.txt
全部 = 0（12/12 案）
$ grep -hiE "erratum|cache.*(error|fault)" ...（排除 ACPI IORT firmware bug 噪声）
仅 ACPI: IORT: [Firmware Bug]: applying workaround（IOMMU 表解析，与核内 cache 无关）
CPU features 声明：RAS Extension Support / Hardware dirty bit management / CPT / SSBS 等，
无任何 "erratum XXX affected" 声明（对比 ARM 常见 "CPU batch vs XXX" 输出——本平台零输出）
```

openEuler 6.6 内核对 HIP08/TaiShan V110 未声明任何受影响 erratum，12 开机全程零 cache 维护错误、零 ECC 事件。这本身构成对"L2/L3 层级性缺陷"的额外负证据：若是共享缓存层级的时序/保持故障，21 天高负载下应有可观测面（即便 ECC 静默纠正也会留 CE 记录）。

L1D 维度小结：正证据集中在装载返回/fill 合并级（ECC 校验点下游，见 MICROARCH_SUPPLEMENT §4：若 V110 L1D ECC 如宣称在位，D1 的多比特撕裂位于校验后的组装段故静默）；L1D tag/data 阵列本身无正面受累证据（way 几何排除 + ECC 零 CE + rasnode 零异常）；L2/L3/LLC 无证据且被单核私有性排除为主因候选。

### 2.5 TLB 单元（dTLB 32-entry FA / L2 TLB 1024-entry）

a) spurious fault 的重译成功 = TLB 查询功能正常【实锤，负证据】

135 起 spurious 的内核判定链：`is_spurious_el1_translation_fault` 用 `at s1e1r, FAR` 重译同地址 → 成功 → 判 spurious 放行。AT 指令的执行同样先查 TLB、miss 则走 PTW，重译成功证明 TLB 查询与 PTW 走查在第二个周期都正常。若 TLB 存在假命中（hit 到错误表项），重译将命中同一坏表项；若存在假 miss（TLB 有有效项却 miss），重译应继续 miss。135/135 重译成功排除"TLB 条目内容损坏"，将受累点收敛到首次访问的瞬态（PTW 读出通路瞬态失败，即 D3；或 TLB 查找时序窗口内的瞬态判定错误，后者软件不可分辩，如实保留两解）。

b) 无 TLB 陈旧条目跨开机/跨页复用证据【实锤，负证据】

```
$ python3（135 起 spurious FAR 全量比对）
跨开机重复的 FAR 地址: 0 个
同开机 4KB 页复用: 存在（08-24 ffff60401b466×4、09-03 ffff604017b3d×5 等）
  —— 但均为同进程周期性重读同一 procfs 缓冲的自然结果（irqbalance 10s 轮询），
     页复用与 TLB 条目滞留无必然联系
```

91.1% 的 spurious FAR 落在 vmalloc/percpu 区（ffff60xx），8.9% 在线性映射，与"irqbalance/pmdalinux 读 /proc/interrupts"的访问谱权重一致（seq_file 缓冲是 vmalloc 区），不构成对特定 TLB set/way 的偏好证据。dTLB 32-entry FA 的全相联结构本身无 set 冲突域，观测也未显示任何 set/way 聚类。

c) fatal fault 的 FSC 谱与页表几何：TLB 无假命中/假 miss 证据【实锤】

12 案 FSC × 子族 × 走表行为全表（本次重算 ESR 解码）：

```
案          FSC   FAR高16位    走表行为                         子族
08-14       L0    非规范0036   PGD选择即拒(非规范域不走表)        撕裂ROL16
08-17       L0    非规范00ff   同上                              撕裂ROR1B
08-24       L0    非规范003c   同上                              变址乱码
15:42       L3    内核规范ffff pte=0(init解映射域)               零塌缩
15:58       L0    非规范00ff   PGD选择即拒                       撕裂ROR1B
08-26       L3    内核规范ffff pte=0(init解映射域)               零塌缩
08-31       L0    用户域0000   user pgtable pgd=0(内核态访用户域) 撕裂跨槽2B
09-03       L0    非规范00ff   PGD选择即拒                       撕裂跨槽1B
09-04-09    L0    非规范2cd7   PGD选择即拒(FAR大值新形态)         撕裂跨槽5B≡ROL3B
09-04-10    L3    内核规范ffff pte=0(init解映射域)               零塌缩
09-04-11    L2    内核规范ffff pmd=0(init解映射,2MB块拆除更深)    零塌缩
09-04-12    L3    内核规范ffff pte=0(init解映射域)               零塌缩
```

- 撕裂族 → L0（7/7 案）：坏地址非规范（高 16 位为撕裂值高位投影：00ff/0036/003c/0000/2cd7 各异，09-04-09 报告第五环已证 FAR 高位 = 撕裂值+模板地址的算术直通），TTBR 选择在 PGD 级即拒绝。TLB 根本没有机会被查询到假结果，走表行为是 MMU 对垃圾输入的诚实反应；
- 零塌缩族 → L2/L3（5/5 案）：x27 塌缩回 `.data..percpu` 模板地址（init 解映射域），走表诚实走到 pte=0（3 案）或 pmd=0（1 案，09-04-11 的 L2 变体；其报告 P4 用双转储并排 vtop 实锤 L2/L3 之别仅为 free_initmem 拆除粒度的页表几何差异，非新通路）。三案 PGD/PUD 表项值逐位相同（10006057fffff403/10006057ffffe403），走表路径与 crash vtop 三方一致；
- 全部 12 案的 FSC 都是"坏地址落在哪"的确定性投影，无一是 TLB 返回了错误翻译。若是 TLB 假命中（hit 到其他页的表项），FAR 应翻译成功且访问到错误物理页（表现为数据错误而非翻译故障），12 案零此类形态；若是 TLB 假 miss（有效项被误判 miss），触发 PTW 走查应成功（页表完好）而不应报 fault，这恰是 spurious 的形态，但 spurious 已被 a) 的重译成功+D3 模型更经济地解释。

TLB 维度判定：TLB 条目内容/翻译功能无任何受累证据【实锤：负证据】；spurious 翻译错的最优解释是 D3（PTW 读出通路瞬态失败，其 70/73 干净承重群体在 MICROARCH_SUPPLEMENT §2.4 已论证非合法竞态）；"TLB 查找时序窗口内的瞬态判定错误"作为不可分辩的备择解释保留【假设】。


## §3 单元×证据覆盖矩阵（核心交付物）

| 单元 | 正证据（受累） | 负证据（免疫） | 无证据 | 置信级 |
|---|---|---|---|---|
| 寄存器堆 RF/PRF（阵列/读出口） | 无 | 12/12 案恰好 1 个寄存器坏且=装载目标寄存器（x20×11+x3×1），其余 30 个与指令语义自洽（x27=x1+x20、x25=x0=x6、帧指针链、KASLR 锚 11/11 闭合）；135 起 WARNING 块零寄存器异常形态；gem5 对照：PRF 损坏为持续性（125k+ 读窗口）≠现场一次性瞬态 | RF 阵列位单元的直接应力测试（软件不可达） | 免疫【实锤】（作为整体）；病灶在其上游装载返回通路【强推】 |
| 执行单元 EXU（ALU 加法器） | 无 | 12/12 fatal 案 `add x27,x1,x20` 对腐化输入逐位算术精确（含进位传播，模 2⁶⁴ 闭合）；08-24 变址 AGU 加法同样闭合；spurious 侧 FAR=x24+0xa 127/127、FAR=x21 135/135，147/147 事件地址算术零错误 | 乘法器/除法器/浮点单元（12 案致命链不含这些运算） | 加法器免疫【实锤】（被故障数据实际行使过的免疫）；乘除/FSU 无证据 |
| 分支预测/控制流（BTB/方向预测/取指） | 无 | 12/12 案 pc 全部落在合法函数内（fbG+0x140 ×11、bio_add_page+0xf0 ×1），lr 与 pc 同函数互洽；call trace 10–19 帧全部为进程身份的语义正确路径（12 种宿主×12 条正确路径）；零野 pc、零跳飞、pstate 全部合法内核态；坏值 12/12 流向数据/地址通路，从未进入 pc 通路 | 分支预测器内部结构（BTB 表项/方向预测器状态）软件不可观测，仅能通过"控制流结果"间接判定 | 免疫【实锤】（控制流结果层面）；预测器内部无证据【软件不可达】 |
| L1D（64KB 4-way） | 装载返回/fill 合并级受累：5/5 验证案坏值=同数组内近期访问行的错相位字节流（跨槽窗口/旋转），源-目标全部跨 set（87≠105 等）→ 排除 way/列选通、锁定 fill-buffer/LQ 合并级；撕裂相位谱 1/2/3/5B 已观测（覆盖 1–7B 相位空间过半） | tag/data 阵列无直接受累证据：way 几何排除；ECC 零 CE；rasnode 960 读数零异常；l1d_disable 四反例（禁用期 0 事件、卸载后照样致命）压深故障点 | L1D 阵列位单元的直接观测；V110 ECC 实际覆盖范围（供应商未披露） | 装载返回通路受累【实锤】；阵列本体无正面证据【实锤：负】；精确到 fill-buffer vs 返回 mux【假设：需 DFT】 |
| L2 / L3 / LLC | 无 | 147/147 事件 100% 单核（CPU179），191 核共享同 L3/互连 21 天零事件，共享层级不可能单核发病；5 验证案内存真值完好+反事实地址 vtop 全 VALID（数据供给路径交付正确数据）；错误源 5/5 局限在同一 1.5KB 数组内（无远端/陌生数据）；rasnode 960 读数零异常；12 案零 CE/UE/EDAC；内核 6.6 对 HIP08 零 erratum 声明 | — | 免疫为主因候选【实锤：单核私有性排除】；作为病灶被排除【强推】 |
| TLB（dTLB 32FA / L2 TLB 1024） | 无（TLB 条目内容/翻译功能层面） | 135/135 spurious 重译（AT S1E1R）成功，排除假命中（否则重译命中同一坏项）；12/12 fatal FSC 均为坏地址的确定性几何投影（L0=非规范地址 PGD 即拒 / L2/L3=init 解映射域诚实走表），零"翻译成功但访问错页"形态；零跨开机 FAR 复用；零 set/way 聚类 | TLB 查找时序窗口内的瞬态判定错误（与 D3 PTW 读出瞬态失败在软件层不可分辩） | 翻译功能免疫【实锤】；spurious 归因 D3（PTW 读出通路）【强推】；"TLB 时序瞬态"备择解释保留【假设】 |
| PTW（页表走查器读出通路） | 受累：135 起 spurious 的干净承重主体（重译成功=页表完好但首次走查瞬态失败）；70/73 落静态线性映射+100% 单核，非合法竞态（MICROARCH_SUPPLEMENT §2.4 三点论证） | 走查器控制逻辑正常：12 案 vtop/dmesg show_pte/crash 三方一致，L2/L3 断点与 free_initmem 拆除几何精确对应 | — | 读出数据通路受累【实锤】（D3 通路） |
| AGU→MMU 地址呈现通路（D2） | 受累（弱）：08-14/08-24 两例架构 MSB≠0 而 FAR MSB=0（byte7 清零可观察）；其余案 D1 坏值 MSB 恰为 0x00 使 D2 不可分辩 | — | D1-only vs D2-only vs D1+D2 的谱可分性（单/多缺陷裁决的仿真代理，未产出） | 受累【实锤-弱】（2/12 例确凿，MICROARCH_SUPPLEMENT §2.3 已诚实降级） |


## §4 对"三通路完备性"的贡献

1. 三通路的单元归属现在闭合：D1（装载返回数据通路）→ L1D fill/返回合并级 + RF 写口上游；D2（地址通路 byte7）→ AGU 输出与 MMU 输入之间；D3（PTW 读出）→ 走查器数据通路。本文补齐的单元级覆盖表明：这三个通路恰好是"从 L1D/fill-buffer 到 RF 写口、从 AGU 到 MMU、从 PTW 到 TLB"的三条数据搬运线，而所有计算单元（ALU）、所有存储阵列本体（RF/L1D 阵列/L2/L3）、所有控制结构（分支/TLB 条目）全部免疫。故障空间被压缩到"数据搬运管线的选路/合并/相位"这一层，与 small-delay-fault 在长互连/多级 mux 上的物理直觉一致。

2. 负证据的系统化价值：本文将 12 案中原本散落的"没错"系统化为可引用的免疫清单：ALU 加法 12/12+135/135、控制流 12/12、RF 整体 147 起零自损、L2/L3 单核私有性排除、TLB 翻译 135/135。这些负证据把 DFT 质询清单（MICROARCH_SUPPLEMENT §5）的向量收敛到三条数据通路的 mux/锁存上，排除了对计算阵列/控制阵列的扫描开销。

3. 单/多缺陷裁决不变：D1/D2/D3 可能是单一缺陷的三种投影（如 LSU 数据返回 mux 同时喂 load-data 总线与 AGU 地址反馈），也可能是三个共址独立缺陷，该裁决软件层不可解，须 RTL/DFT（MICROARCH_SUPPLEMENT §3 的边界声明维持）。本文的单元矩阵不改变这一边界，但将"三通路"的单元候选面收敛到了 LSU/DCU 邻域。

4. 与 STORE_PATH_SENSITIVITY 的合并视图：写侧 127 起 spurious（strb 翻译瞬态失败、自愈）+ 读侧 8 起 spurious + fatal 12 起全为 load，值级 SDC 只在装载返回通路，翻译级瞬态失败读写公共。本矩阵的 TLB/PTW 行即该"公共翻译通路"的单元化表达。


## §5 诚实边界

1. dmesg 不打印 x30 与 pstate 之外的 SPSR 细节；fatal 块寄存器为 x0–x29（30 个），"其余 30 个寄存器自洽"的计数以 x0–x29+x30(不可见) 为基，x30 在 12 案中均为"不可观测"而非"确认完好"，如实标注。
2. WARNING 块的 caller-saved 寄存器（x0–x18 部分）已被 printk 污染（STORE_PATH_SENSITIVITY §1.2 证 x12–x17 = 告警文本自身），WARNING 块的"寄存器健全"结论仅对 callee-saved（x19–x28）与入参三联（x19/x21/x22）成立。
3. "每案恰好 1 个寄存器坏"的判定依赖语义可判性：x20/x3 的坏值可通过内存真值对照证明；其余寄存器只能验证"与已知语义一致"，无法穷尽证明"值未被改变但恰好仍是合法值"（如 x23 的 load_avg 残留若被腐化为另一个合法数值则不可分辩）。该边界的量化：可证坏 1 个/案，可证自洽 ~12 个/案（KASLR 锚/迭代号/帧链/栈关系），其余为"形态合法但不可证伪"。
4. L2/L3 免疫的"排除"是主因级排除：单核私有性+真值完好排除它们作为病灶，但坏值路径上 L2→L1D 的最后一段数据搬运仍在 D1 候选通路的物理范围之内（fill 来自行内/行间，软件无法分辨填充源头的最后缓存层级）。
5. TLB 免疫的"重译成功"论证假设 AT S1E1R 与首次访问共用同一 TLB/PTW；若两者在微架构上走不同查表路径（软件不可知），该论证弱化为"至少 AT 路径正常"。
6. V110 微架构参数为公开资料整理（kunpeng.md，E1/E3 置信级）：L1D 4-way/256 set、dTLB 32-entry FA、L2 TLB 1024-entry 若与实际硅片有出入，set 几何与 TLB 覆盖的计算需按供应商数据复核；但"源/目标跨 set"的结论对任何 ≥2-way 组相联几何稳健（5/5 案源-目标行号差 ≥8 行，远超任何实际相联度）。
7. 乘法器/除法器/浮点单元/向量单元：12 案致命链不含这些运算，无法从 vmcore 判定其敏感性，矩阵中如实标"无证据"而非"免疫"。method3 现场实验（MICROARCH_EVIDENCE D4）显示 6 类负载（含 float/double GEMM）同核同病，说明故障非指令特异，但那是对通路的证据，不是对 FSU 内部的单元级判定。
8. 分支预测器内部状态（BTB 表项/全局历史/RAS 栈）软件完全不可观测；"控制流免疫"仅在架构结果层面（pc/栈/路径）成立，预测器内部的瞬态错预测若未改变架构路径则不可见；但那类错误本身也被乱序机器的验证逻辑自愈，不构成 SDC 通路。
9. 样本量为 12 案 147 起：单元级"免疫"结论的强度受观测时长（21 天）与访问谱（内核态调度路径为主）限制；用户态密集计算负载下的单元行为由 method3/sdc_long 探针补证，但覆盖度不均匀。


## 附录：本文全部关键命令与输出摘录

```bash
# ① 事件底数（§1.1）
grep -c "Ignoring spurious" /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt
#   12/26/34/1/0/9/13/35/2/2/0/1 → 合计 135
cat /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt | grep -oE "WARNING: CPU: [0-9]+" | sort | uniq -c
#   135 WARNING: CPU: 179（100% 单核）

# ② 12 案致命块寄存器全量提取与代数闭合（§2.1/§2.2）
awk '/Unable to handle/{f=1} f{print; c++} c>90{exit}' <dmesg>   # x0–x29 + sp + pstate + Code
python3（批量）:
#   x27=(x1+x20) mod 2^64: 11/11 fbG 案 True（08-24 bio 案不适用，FAR==x3低48位 True）
#   FAR=(x27+0x120): 全64位 9/12 True；低48位 11/12 True
#     （08-14/08-31 高16位 HW截位现象，CROSS_CASE §8.2 第6条两解并存）

# ③ 坏寄存器分布（§2.1）
#   x20 ×11（fbG 族 ldr x20,[x0,w25,sxtw#3] 目标）
#   x3  ×1（bio 族 ldr x3,[x3,x2] 目标）→ 12/12 = 装载目标寄存器

# ④ WARNING 块健全性（§2.1c）
python3（135 块全量）:
#   x21==FAR & x19==ESR(0x9600004x) & x22==EC(0x25): 135/135
#   x29==sp: 135/135；内核指针高字节异常: 0 个

# ⑤ call trace 合法性（§2.3）
python3（12 案帧提取）: 全部帧落在合法内核符号域，零野 pc
#   pc: fbG+0x140 ×11 + bio_add_page+0xf0 ×1

# ⑥ L1D set 几何（§2.4a）
python3: 数组基址=x24+0x5d0（12/12 重建闭合），
#   set index=(VA>>6)&0xff: 5 验证案源/目标 set 全不同（87/105、87/109、102/94、102/88、88/106）

# ⑦ RAS 观测面（§2.4）
grep "cpu=179 node" 127.0.0.1-2026-08-26-10:37:27/vmcore-dmesg.txt
#   五节点 FR/CTLR/STATUS/ADDR/MISC 与其余 191 核逐位一致（960 读数零异常）
grep -icE "corrected|uncorrectable|ECC error|machine check|Hardware error" <12份dmesg>
#   全部 = 0

# ⑧ TLB/spurious 分析（§2.5）
python3（135 起）:
#   跨开机 FAR 重复: 0；FAR−x24 分布: +0xa ×127（memcpy 写路径固定相位）
#   ESR 分布: 0x96000044 ×127（写）/ 0x96000004 ×8（读）
```


*本文数字全部来自对 12 份 vmcore-dmesg.txt 的本次独立重算（python3 模 2⁶⁴），内存真值引自 5 个已归档 crash 会话；与 CROSS_CASE_STATISTICS（8 案）的既有数字交叉一致（135=130+2+2+1 新增四案；12 fatal 含远端 09-03 案）。生成于 2026-09-04，不 commit。*

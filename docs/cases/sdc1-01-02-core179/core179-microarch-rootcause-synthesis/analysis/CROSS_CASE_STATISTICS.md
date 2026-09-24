# SDC 跨案例结构化统计普查报告（8 案：本机 7 + 远端 1）

报告性质：法医级独立普查。本机 7 案的全部数字来自本次对 vmcore-dmesg.txt 的重新 grep/awk/python 统计，未沿用既往报告（127.0.0.1-2026-08-26-10:37:27/DIAGNOSIS_REPORT.md）的任何数字；第 8 案（2026-09-03，位于远端单板 172.168.160.42）原始 dmesg 本机不可达（ssh 公钥被拒），其数字由协调方提供摘录，本报告对其中全部可计算断言（代数闭合、反事实地址、字节形态）做了独立 python 复算并逐项通过；文末附与既往结论的逐项对照。

取证环境：
- 本机工作目录：`/home/sdc/wangxu/vmcore0102/`（7 案）
- 第 8 案位置：远端单板 172.168.160.42 `/home/sdc/vmcore/127.0.0.1-2026-09-03-18:25:12/`（本机 ssh 公钥认证被拒、ping 可达；dmesg 明细以协调方摘录为准，凡未提供的中段事件序列如实标注"未提供"，不做推断）
- 目标主机：Yangtze Computing R240K V2 / BC82AMQA，BIOS 7.48 06/15/2026
- CPU：Kunpeng-920 (HIP08/TaiShan-v110) ×192，8 NUMA 节点
- 内核：6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP（8 案完全一致）


## 0. 重大事实更正（对任务描述本身）

| 项 | 任务描述 | 本次实测 | 判定 |
|---|---|---|---|
| 转储目录数 | 8 个（本机工作目录内） | 本机工作目录内 7 个（`find -maxdepth 1 -type d` 仅返回 7 个 127.0.0.1-* 目录）；第 8 案在远端单板 172.168.160.42 上 | 任务描述的"8 个目录在本机"与磁盘不符；8 案总数经由远端第 8 案补齐成立 |
| 既往报告覆盖 | 6 案 | 6 案（08-14/08-17/08-24/15:42/15:58/08-26）；08-31 为本机新增第 7 案；09-03 为远端第 8 案 | 本次普查覆盖全部 8 案 |

本机 7 案 dmesg 文件完整性记录（法医链）：

| # | 目录 | 字节数 | 行数 | md5（前 12 位） |
|---|---|---|---|---|
| 1 | 127.0.0.1-2026-08-14-19:07:04 | 235,096 | 3,174 | 490bb615df6f |
| 2 | 127.0.0.1-2026-08-17-13:47:08 | 286,747 | 3,813 | 28a6bce2b24c |
| 3 | 127.0.0.1-2026-08-24-18:03:07 | 336,556 | 4,466 | b8318911500a |
| 4 | 127.0.0.1-2026-08-25-15:42:24 | 205,524 | 2,847 | 9d114a354317 |
| 5 | 127.0.0.1-2026-08-25-15:58:09 | 190,452 | 2,628 | 73fdc7d839f1 |
| 6 | 127.0.0.1-2026-08-26-10:37:27 | 406,441 | 4,193 | b50b2f02dfb5 |
| 7 | 127.0.0.1-2026-08-31-00:47:32 | 241,031 | 3,243 | 696c7e5cfef3 |
| 8 | （远端）172.168.160.42:127.0.0.1-2026-09-03-18:25:12 | 不可达 | 未提供 | 不可达（ssh 公钥被拒） |

本机 7 案 dmesg 均为单次开机（每文件恰含 1 条 "Booting Linux on physical CPU"），时间戳从 `[0.000000]` 起。第 8 案的 dmesg 原文未到达本机，其行数/md5 无法记录；该案所有数字标注为"协调方提供，关键代数断言已独立复算"。


## 1. 每案事实表

### 案 1：127.0.0.1-2026-08-14-19:07:04

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 3,174 行；[0.000000] → 113997.659933 |
| panic 时刻（Oops 首行） | 113997.282188s = 31h39m57s uptime |
| 推算开机墙钟 | 2026-08-13 11:27:06（dump 名减 uptime） |
| WARNING 数（spurious） | 12，全部 CPU=179（`grep -c "WARNING: CPU: 179"` = 12，无任何其他 CPU 号） |
| 首症时刻 | 104485.843s = 29h01m26s（首个 WARNING，pmdalinux） |
| WARNING ESR 分布 | x19=0x96000044 ×10；0x96000004 ×2（memcpy1@113810、control@113979，均 `_find_next_and_bit` 触发） |
| WARNING 进程 | pmdalinux×6(PID 10359)、irqbalance×4(PID 9682)、memcpy1×1(PID 2606819)、control×1(PID 2823806) |
| WARNING 触发函数 | `__memcpy+0x80/0x240`×10、`_find_next_and_bit+0x18/0x80`×2 |
| 致命 Oops | 113997.282188s，`Unable to handle kernel paging request at 0036bc836a4a97df` |
| Oops CPU/PID/Comm | **CPU 179**, PID 1986, kworker/179:1H（Workqueue: kblockd） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `find_busiest_group+0x11c/0xb60` |
| ESR / EC / FSC | 0x9600004；EC=0x25 DABT(current EL)；FSC=0x04 level 0 translation fault |
| 页表判定 | `[0036bc836a4a97df] address between user and kernel address ranges`（非规范域，不走表） |
| Code 字段 | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` |
| 调用栈 | fbG+0x140 ← load_balance+0x108 ← newidle_balance+0x198 ← pick_next_task_fair ← pick_next_task ← __schedule ← schedule ← worker_thread ← kthread ← ret_from_fork |
| l1d_disable | 无（本开机未加载） |
| RAS | 启动期注册（BERT/HEST/EINJ 在位、GHES firmware-first、ghes_edac 32 DIMM）后全程零记录 |
| 压测负载 | memcpy1 / control（SDC 压测进程，见 §7） |
| 崩溃寄存器关键值 | x1=ffffa6c96a4996c0, x20=d93715ba0000ffff, x27=d936bc836a4a96bf, x25=0xb0(=176), x23=0x400, x22==x26=ffff604003e5f8a0 |

### 案 2：127.0.0.1-2026-08-17-13:47:08

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 3,813 行；[0.000000] → 239528.236539 |
| panic 时刻 | 239527.811339s = 66h32m08s |
| 推算开机墙钟 | 2026-08-14 19:15:00（上案 dump 后 7m56s 重启） |
| WARNING 数 | 26，全部 CPU=179 |
| 首症时刻 | 169175.044s = 46h59m35s |
| WARNING ESR | x19=0x96000044 ×26（100%） |
| WARNING 进程 | irqbalance×12(PID 9653)、pmdalinux×14(PID 10334) |
| WARNING 触发函数 | `__memcpy+0x80`×26 |
| 致命 Oops | 239527.811339s，FAR=00ffd780f5a3a7c0 |
| Oops CPU/PID/Comm | **CPU 179**, PID 0, swapper/179（idle 任务，唯一一次） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `+0x11c` |
| ESR / FSC | 0x9600004；FSC=0x04 level 0 |
| 页表判定 | address between user and kernel address ranges |
| Code | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` |
| 调用栈 | fbG ← load_balance ← rebalance_domains ← _nohz_idle_balance ← run_rebalance_domains ← handle_softirqs ← … ← do_idle ← secondary_start_kernel（唯一从 idle 软中断路径进入的案例） |
| l1d_disable | 无 |
| RAS | 同案 1：注册后全程零记录 |
| 崩溃寄存器关键值 | x1=ffffd7d8cdf196c0, x20=00ffffa827b20fe0, x27=00ffd780f5a3a6a0, x25=0xaf(=175), x23=0x400, x22==x26=ffff604003e9ec00 |

### 案 3：127.0.0.1-2026-08-24-18:03:07

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 4,466 行；[0.000000] → 537463.034559 |
| panic 时刻 | 537462.585964s = 149h17m43s（八案最长存活） |
| 推算开机墙钟 | 2026-08-18 12:45:24（上案 dump 后 22h58m，中间隔了一个周末） |
| WARNING 数 | 34（本机七案最多；第 8 案 35 为八案最高），全部 CPU=179 |
| 首症时刻 | 835.044s = 13m55s（开机后 14 分钟即首症，但拖了 149h 才致命） |
| WARNING ESR | 0x96000044 ×33；0x96000004 ×1（bash@6940s，`__lruvec_stat_mod_folio` 触发） |
| WARNING 进程 | irqbalance×33(PID 9665)、bash×1(PID 37681) |
| WARNING 触发函数 | `__memcpy+0x80`×33、`__lruvec_stat_mod_folio+0x20`×1（用户缺页 COW 路径 ldr 触发） |
| 致命 Oops | 537462.585964s，FAR=003c521da2e9b99f |
| Oops CPU/PID/Comm | **CPU 179**, PID 2077673, kworker/u391:3（flush-253:2 回写） |
| pc / lr | `bio_add_page+0xf0/0x1a0` / `bio_add_folio+0x30/0x50`（八案唯一非 fbG 崩溃点） |
| ESR / FSC | 0x9600004；FSC=0x04 level 0 |
| Code | `f9400021 8b020060 f8626863 d375dc22 (f9400061)`。致命指令 `ldr x1,[x3]`，其 x3 来自上一条缩放变址装载 `ldr x3,[x3,x2]` 的返回值 |
| 页表判定 | address between user and kernel address ranges |
| 调用栈 | bio_add_page ← bio_add_folio ← iomap_add_to_ioend ← … ← ext4_iomap_writepages ← wb_workfn ← process_one_work |
| l1d_disable | 无 |
| RAS | 零记录（本目录另有 sdc_long 压测 ELF 二进制，75KB，见 §7） |
| 崩溃寄存器关键值 | x3=553c521da2e9b99f（乱码索引返回值），FAR=003c521da2e9b99f（与 x3 低 48 位相等、高 8 位 0x55 vs 0x00）；x20=x23=x28=0x10000 |

### 案 4：127.0.0.1-2026-08-25-15:42:24

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 2,847 行；[0.000000] → 76809.714391 |
| panic 时刻 | 76809.255750s = 21h20m09s |
| 推算开机墙钟 | 2026-08-24 18:22:15（上案 dump 后 19m08s） |
| WARNING 数 | 1，CPU=179 |
| 首症时刻 | 1707.361s = 28m27s（pmdalinux, FAR=ffff00204f4e24a8） |
| WARNING ESR | 0x96000044 ×1 |
| 致命 Oops | 76809.255750s，FAR=ffffa5aa9b5a97e0 |
| Oops CPU/PID/Comm | **CPU 179**, PID 2018836, claude（用户 shell 会话进程，epoll_pwait 睡眠唤醒） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `+0x11c` |
| ESR / FSC | 0x9600007；FSC=0x07 level 3 translation fault（零塌缩子族特征） |
| 页表判定 | swapper pgtable 走表：pgd=…f403, pud=…e403, pmd=…a403, pte=0000000000000000（设计性解映射区间） |
| Code | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` |
| 调用栈 | fbG ← load_balance ← newidle_balance ← pick_next_task_fair ← … ← schedule ← ep_poll ← do_epoll_wait ← epoll_pwait2 |
| l1d_disable | 有，完整时间线见 §5（本开机为 l1d_disable 主实验场：3 轮长实验 + 4 次反复 disable/re-enable，共 26 条模块日志） |
| RAS | 零记录 |
| 崩溃寄存器关键值 | x1=ffffa5aa9b5a96c0, x20=0000000000000000（零塌缩）, x27=ffffa5aa9b5a96c0（==x1）, x25=0xb0(=176), x23=0x400, x22==x26=ffff604003e99780 |

### 案 5：127.0.0.1-2026-08-25-15:58:09

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 2,628 行；[0.000000] → 418.744655 |
| panic 时刻 | 418.373008s = 6m58s（八案最短存活：重启后 7 分钟即崩溃） |
| 推算开机墙钟 | 2026-08-25 15:51:11（上案 dump 后 8m47s 立即重启复现） |
| WARNING 数 | 0（无前兆直接致命，唯一案例） |
| 致命 Oops | 418.373008s，FAR=00ffb34569fc3ac0 |
| Oops CPU/PID/Comm | **CPU 179**, PID 1931, kworker/179:1H（kblockd） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `+0x11c` |
| ESR / FSC | 0x9600004；FSC=0x04 level 0 |
| 页表判定 | address between user and kernel address ranges |
| Code | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` |
| 调用栈 | fbG ← load_balance ← newidle_balance ← … ← worker_thread ← kthread |
| l1d_disable | 无（未加载即崩；与案 4 卸载 3.7h 后仍 panic 共同构成"l1d_disable 无效"证据对） |
| RAS | 零记录 |
| 崩溃寄存器关键值 | x1=ffffb378e25e96c0, x20=00ffffcc879da2e0, x27=00ffb34569fc39a0, x25=0x92(=146), x23=0x400, x22==x26=ffff604003e26c00 |

### 案 6：127.0.0.1-2026-08-26-10:37:27（既往深度报告的主案）

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 4,193 行；[0.000000] → 66686.049662 |
| panic 时刻 | 66685.621071s = 18h31m26s |
| 推算开机墙钟 | 2026-08-25 16:06:01（上案 dump 后 7m52s） |
| WARNING 数 | 9，全部 CPU=179 |
| 首症时刻 | 1467.049s = 24m27s（irqbalance 三连发，间隔 0.000s/0.005s） |
| WARNING ESR | 0x96000044 ×9（100%） |
| WARNING 进程 | irqbalance×4(PID 9631)、pmdalinux×5(PID 13610) |
| WARNING 触发函数 | `__memcpy+0x80`×9 |
| 致命 Oops | 66685.621071s，FAR=ffffa29301d797e0 |
| Oops CPU/PID/Comm | **CPU 179**, PID 256855, mi-scavenger（futex 睡眠唤醒路径） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `+0x11c` |
| ESR / FSC | 0x9600007；FSC=0x07 level 3（零塌缩子族） |
| 页表判定 | swapper pgtable：pgd=…f403, pud=…e403, pmd=…a403, pte=0 |
| Code | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` |
| 调用栈 | fbG ← load_balance ← newidle_balance ← pick_next_task_fair ← … ← futex_wait ← do_futex ← el0_svc |
| l1d_disable | 无 |
| rasnode.ko | 有：8026.6s 加载，扫描 192 核 × 5 ERR 节点（共 1162 行 rasnode 日志），CPU179 五节点 FR/CTLR/STATUS/ADDR/MISC 读数与其余 191 核逐位一致（如 node0: FR=0x4842 CTLR=0x101 STATUS=0xff），零差异零记录 |
| 崩溃寄存器关键值 | x1=ffffa29301d796c0, x20=0000000000000000, x27=ffffa29301d796c0（==x1）, x25=0xb3(=179), x23=0x400, x22==x26=ffff604003e27660 |

### 案 7（新增）：127.0.0.1-2026-08-31-00:47:32

| 项 | 值 |
|---|---|
| dmesg 行数 / 时间范围 | 3,243 行；[0.000000] → 396123.115345 |
| panic 时刻 | 396122.719381s = 110h02m03s |
| 推算开机墙钟 | 2026-08-26 10:45:29（上案 dump 后 8m02s） |
| WARNING 数 | 13，全部 CPU=179 |
| 首症时刻 | 10520.595s = 2h55m21s（pmdalinux） |
| WARNING ESR | 0x96000044 ×13（100%） |
| WARNING 进程 | pmdalinux×8(PID 14074)、irqbalance×5(PID 9678) |
| WARNING 触发函数 | `__memcpy+0x80`×13 |
| WARNING 时间分布 | 前段 5 起（10520–13391s）→ 长静默 269kh → 后段 8 起（282139–396120s），末 3 起在 panic 前 3.098s 内连发 |
| 致命 Oops | 396122.719381s，FAR=0000c1a9443c9305 |
| Oops CPU/PID/Comm | **CPU 179**, PID 16, rcu_sched（rcu_gp_kthread 睡眠唤醒，schedule_timeout 路径） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `+0x11c` |
| ESR / FSC | 0x9600004；FSC=0x04 level 0 |
| 页表判定 | user pgtable: pgdp=0000204003e1c000，pgd=0, p4d=0（FAR 高 16 位=0000，落入用户规范域，但当前是内核态访问 → L0 失败） |
| Code | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)` |
| 调用栈 | fbG ← load_balance ← newidle_balance ← … ← schedule_timeout ← rcu_gp_fqs_loop ← rcu_gp_kthread ← kthread |
| l1d_disable | 有：三轮短实验（44745–44749s / 82326–82626s / 83108–84007s），每轮 disable 后 2s–900s 即 re-enable 卸载；最后一轮卸载于 84007s，panic 于 396123s（卸载 312,116s ≈ 86.7h 之后） |
| silifuzz_orches | 有（八案唯一）：5 次 memfd_create 记录 @18149/21472/21943/22675/80947s，最后活动距 panic 87.5h |
| RAS | 零记录 |
| 崩溃寄存器关键值 | x1=ffffc1a985e596c0, x20=a000ffffbe56fb25（新腐化形态，见 §8）, x27=a000c1a9443c91e5, x25=0x3c(=60，当时为最小迭代号；第 8 案 x25=12 更小), x23=0x400, x22=ffff604003e27120, x26=ffff604003e270c0（x22≠x26，差 0x60，fbG 案中唯一破例，第 8 案恢复相等） |

### 案 8（新增·远端）：127.0.0.1-2026-09-03-18:25:12【数据来源：协调方摘录；代数断言已本机独立复算】

> ⚠ 本案原始 vmcore-dmesg.txt 位于远端单板 172.168.160.42 `/home/sdc/vmcore/`，本机 ssh 认证被拒无法直接取证。以下事实为协调方提供的摘录；标 〔复算✓〕 的条目为本报告用 python 独立重算验证通过；标 〔未提供〕 的条目不做任何推断。

| 项 | 值 |
|---|---|
| 位置 | 172.168.160.42:/home/sdc/vmcore/127.0.0.1-2026-09-03-18:25:12/（本机无此目录） |
| dmesg 行数 / md5 | 〔未提供〕 |
| panic 时刻 | 322246.6s = 89h30m47s uptime |
| 推算开机墙钟 | 2026-08-31 00:54:25（= dump 名 2026-09-03 18:25:12 减 uptime，距上案 08-31 dump 仅 6m53s，第 7 次"崩溃→数分钟内重启"链）〔复算✓〕 |
| WARNING 数 | 35，全部 CPU=179 〔协调方 grep〕 |
| WARNING 进程 | irqbalance×32（PID 9736）、pmdalinux×3（PID 10282）、rcu_sched×1（第 8 案新增进程；也是本案 Oops 宿主） |
| WARNING ESR / 触发路径 | 全部 `__memcpy+0x80`（seq_printf/show_interrupts 读 /proc/interrupts 路径）；ESR=0x96000044（WnR=1 写 + L0） |
| WARNING FAR | 全部 `ffff60xx`（vmalloc/percpu 区），与 8 案全局分布一致 |
| 首症时刻 | 71822.06s = 19h57m02s 〔复算✓：首症→panic 250,424.5s = 69.56h〕 |
| 末段事件 | 322215s、322225s（间隔 10s），最后事件距 panic 21.6s 〔复算✓〕；中段 33 个事件的具体时刻〔未提供〕 |
| 致命 Oops | 322246.6s，FAR=00ffc99ebbaad120（高 16 位=00ff，非规范域，"address between user and kernel address ranges"）〔复算✓〕 |
| Oops CPU/PID/Comm | **CPU 179**, PID 16, rcu_sched（与第 7 案 08-31 相同的 PID/Comm；两案皆为 RCU 内核线程触发，第 7 案走 rcu_gp_kthread/schedule_timeout，本案路径〔未提供〕） |
| pc / lr | `find_busiest_group+0x140/0xb60` / `+0x11c`，第 7 次命中同一指令 |
| ESR / FSC | 0x9600004；FSC=0x04 level 0 |
| Code | `f9400782 f879d814 2a1903e0 8b14003b (f9409377)`，与其余 6 次 fbG 命中逐字相同 |
| 寄存器 | x1=ffffc9e8a3cd96c0, x20=00ffffb617dd3940, x27=00ffc99ebbaad000, x25=0xc(=12，低迭代号第 2 例), x23=0x400, x22==x26=ffff604003e9e3c0（该指纹在本案恢复成立）；全 30 寄存器〔未提供〕，仅上述关键值 |
| 代数闭合 | x1+x20 = 00ffc99ebbaad000 = x27 逐位闭合〔复算✓〕；FAR = x27+0x120 含高 16 位闭合〔复算✓〕 |
| 内存真值（crash 8.0.4，2026-09-03 远端执行） | `__per_cpu_offset[12]` = 0xffffb617dc4d6000（非零）；数组为完美等差数列（base=ffffb617dc33e000，step=0x22000）〔复算✓：base+12×step 与真值吻合〕 |
| 反事实验证 | x27_true = x1+真值 = ffff8000801af6c0〔复算✓〕；vtop 验证 VALID（PTE e80037ffe2ef03 VALID|SHARED|AF|DIRTY）；x27_true+0x120 处读出 0x3ff = rq(12).cfs.avg.load_avg=1023。若装载收到真值则不会崩溃 |
| 真值域互证（本次新发现） | rq(12)_true=ffff8000801af6c0 与 08-26 案 rq(179)_true=ffff8000817dd6c0 之差 = 0x162e000 = (179−12)×0x22000〔复算✓〕。两案独立 crash 读数与等差模型三方互证，percpu 布局跨开机物理一致 |
| x20 形态 | ROR8(entry[123])，见 §8.2 第 4 条的"错行拼接"修正模型 |
| l1d_disable | 本开机无加载记录 |
| RAS | 〔未提供〕（协调方未提及，推定与其他案一致静默，如实标注） |


## 2. 八案汇总表

| 案 | dump 名 | dmesg 行数 | panic uptime | WARNING 数 | Oops 崩溃点 | 致命指令 | Oops CPU | 子族（x20 形态） | FSC |
|---|---|---|---|---|---|---|---|---|---|
| 1 | 08-14 19:07:04（本机） | 3,174 | 31h39m57s | 12 | fbG+0x140 | `ldr x23,[x27,#288]`（f9409377） | 179 | 错行拼接·ROL16(entry[1]) | L0 |
| 2 | 08-17 13:47:08（本机） | 3,813 | 66h32m08s | 26 | fbG+0x140 | 同上 | 179 | 撕裂·ROR8（真值候选，未内存验证） | L0 |
| 3 | 08-24 18:03:07（本机） | 4,466 | 149h17m43s | 34 | bio_add_page+0xf0 | `ldr x1,[x3]`（f9400061） | 179 | 变址装载乱码 | L0 |
| 4 | 08-25 15:42:24（本机） | 2,847 | 21h20m09s | 1 | fbG+0x140 | 同案 1 | 179 | **零塌缩**（x20=0） | **L3** |
| 5 | 08-25 15:58:09（本机） | 2,628 | 0h06m58s | 0 | fbG+0x140 | 同案 1 | 179 | 撕裂·ROR8（真值候选，未内存验证） | L0 |
| 6 | 08-26 10:37:27（本机） | 4,193 | 18h31m26s | 9 | fbG+0x140 | 同案 1 | 179 | **零塌缩**（x20=0） | **L3** |
| 7 | 08-31 00:47:32（本机） | 3,243 | 110h02m03s | 13 | fbG+0x140 | 同案 1 | 179 | **错行拼接·ROR16(entry[125]/[126])**（真值已验证） | L0 |
| 8 | 09-03 18:25:12（远端） | 未提供 | 89h30m47s | 35 | fbG+0x140 | 同案 1 | 179 | **错行拼接·ROR8(entry[123])**（真值已验证） | L0 |

总计：130 次 WARNING + 8 次致命 Oops = 138 起硬件异常事件，`WARNING: CPU:` 与 Oops `CPU:` 字段 100% 为 179（本机 7 案逐案 `grep -oE "WARNING: CPU: [0-9]+" | sort | uniq -c` 复核无任何其他 CPU 号；第 8 案 35 次 CPU179 为协调方 grep 结果）。其余 191 核在八开机累计约 487.0 小时运行中零事件。

关键不变式（7 个 fbG 案，脚本逐位验证；第 8 案仅关键寄存器可验）：

| 不变式 | 验证结果 |
|---|---|
| `Code:` 五指令字全同 | 7/7 fbG 案逐字相同（08-24 异位点不同属预期） |
| x23 ≡ 0x400 | 7/7 ✓（含第 8 案） |
| x22 == x26 | 6/7 ✓（08-31 x22−x26=0x60 唯一破例；第 8 案恢复成立） |
| x24 − x21 ≡ 0x5350 | 本机 6/6 ✓（第 8 案 x24/x21〔未提供〕） |
| x21 − (x24+0x5d0) ≡ −0x5920（nr_cpu_ids 与 __per_cpu_offset 基址相对距离） | 本机 6/6 ✓ |
| x1 − (x24+0x5d0) ≡ −0x3fbf10（runqueues 模板与 per_cpu_offset 基址相对距离） | 本机 6/6 ✓ |
| x1 低 16 位 ≡ 96c0 | 7/7 ✓（含第 8 案 x1=…96c0） |
| x27 = (x1 + x20) mod 2⁶⁴ | 8/8 逐位闭合 ✓（7 个 fbG 案全闭合 + 08-24 bio 案不适用；第 8 案已复算闭合） |
| FAR 低 48 位 = x27 低 48 位 + 0x120 | 7/7 ✓（08-14/08-31 高 16 位 HW 上报与寄存器不一致，见 §8.3；第 8 案含高 16 位整体闭合） |
| __per_cpu_offset 数组等差 step=0x22000 | 3 案独立验证（08-26/08-31/第 8 案），base 各异（KASLR），步长跨开机恒定 |


## 3. FAR 地址区域分类

### 3.1 WARNING（spurious）事件 FAR，95 起

| 前缀 | 语义（arm64 openEuler 6.6 内核布局） | 件数 | 占比 |
|---|---|---|---|
| `ffff60xx_xxxxxxxx` | vmalloc / percpu chunk 区（`ffff6040…` 段为 percpu 动态分配 + vmalloc 统计结构；irqbalance 读 /proc/interrupts 的 seq_file 缓冲、pmdalinux 读 /proc 系列命中此区） | 80 | 84.2% |
| `ffff00xx / ffff0020…` | 线性映射（direct map）区（用户进程内核栈/页结构附近，如 ffff0020365fe35e） | 6 | 6.3% |
| `ffff20xx`（ffff2020…） | 线性映射高端段（node 交织区） | 4 | 4.2% |
| `ffff40xx`（ffff4029…） | 线性映射另一节点段 | 3 | 3.2% |
| `ffffc3xx`（ffffc360…） | vmalloc 尾段（模块数据区） | 1 | 1.1% |
| `ffff6054 / ffff604a / ffff6043` 等更高 vmalloc | 归入 ffff60xx 计数 | （含于上） | — |

明细核对（独立 grep `at virtual address [0-9a-f]{6}`）：
- 08-14：ffff60 ×12；
- 08-17：ffff60 ×26；
- 08-24：ffff60 ×25、ffff00 ×4、ffff20 ×1、ffff40 ×3、ffffc3 ×1；
- 15:42：ffff00 ×1；
- 15:58：无；
- 08-26：ffff60 ×9；
- 08-31：ffff60 ×10、ffff20 ×3。

结论：spurious FAR 84% 落在 vmalloc/percpu 区，16% 落在线性映射区，无用户态地址、无固定地址复用跨开机现象（同开机内 4KB 页簇重复：08-24 ffff60401b466 ×4、08-14 ffff60408e267 ×3 等，均为"同进程周期性重复触碰同一 procfs 缓冲"的自然结果）。

### 3.2 致命 Oops FAR，8 起

| 案 | FAR | 高 16 位 | 规范性判定 | 走表结果 |
|---|---|---|---|---|
| 08-14 | 0036bc836a4a97df | 0036 | **非规范**（既非 0000 亦非 ffff） | between user and kernel ranges，L0 |
| 08-17 | 00ffd780f5a3a7c0 | 00ff | **非规范** | 同上，L0 |
| 08-24 | 003c521da2e9b99f | 003c | **非规范** | 同上，L0 |
| 15:42 | ffffa5aa9b5a97e0 | ffff | **内核规范**（但落在已解映射的 init/percpu 模板区） | swapper 表走至 pte=0，L3 |
| 15:58 | 00ffb34569fc3ac0 | 00ff | **非规范** | between，L0 |
| 08-26 | ffffa29301d797e0 | ffff | **内核规范**（同 15:42） | pte=0，L3 |
| 08-31 | 0000c1a9443c9305 | 0000 | 用户规范域（内核态访问用户域地址 → 首见） | user pgtable pgd=0，L0 |
| 09-03(8) | 00ffc99ebbaad120 | 00ff | **非规范** | between，L0 |

FAR 二分法与子族严格对应：零塌缩（x20=0）→ FAR=x1+0x120 落在内核规范域 → FSC=L3（走到 PTE=0）；撕裂/乱码 → FAR 落入非规范或用户域 → FSC=L0（PGD 级即失败）。两种 FSC 是同一坏地址的不同投影，非两种故障。


## 4. 事件时间间隔分析

### 4.1 每案事件链（WARNING 时刻 + Oops 时刻，含相邻间隔秒）

- 08-14（13 事件）：104485.8 →(+490.2) 104976.1 →(+1243.0) 106219.1 →(+5026.7) 111245.7 →(+53.3) 111299.0 →(+436.7) 111735.7 →(+113.3) 111849.1 →(+1396.7) 113245.7 →(+190.1) 113435.8 →(+310.0) 113745.8 →(+65.2) 113811.0 →(+168.7) 113979.7 →(+17.6) panic 113997.3
- 08-17（27 事件）：169175.0 →(+469.9)(+170.1)(+60.0)(+0.022)(+9.8)(+0.006)(+132.1)(+77.9)(+222.1)(+240.0)(+197.9)(+142.1)(+7.9)(+70.0) → (+64130.0 长静默) → 235104.9 →(+344.1)(+55.9)(+130.0)(+284.1)(+15.9)(+349.9)(+2304.1)(+446.0)(+134.0)(+95.9) → (+262.8) → panic 239527.8
- 08-24（35 事件）：835.0 →(+5532)(+573)(+247)(+1940)(+1570)(+750)(+10.0)(+531)(+1670)(+140)(+2563)(+182)(+1780)(+270)(+1861)(+2261)(+2520)(+210)(+10.0)(+280) →(+10050) 35785 → (+40130)(+95520)(+343330 三段长静默) → 515385 →(+140)(+40)(+40)(+80)(+5970)(+15800)(+0.0)(+0.004) → (+7.5) → panic 537462.6
- 15:42（2 事件）：1707.4 → (+75101.9) → panic 76809.3
- 15:58（1 事件）：panic 418.4（无前兆）
- 08-26（10 事件）：1467.05 →(+0.000)(+0.005 三连发) → (+60516.4 长静默) → 61983.4 →(+70.1)(+423.6)(+416.4)(+10.1)(+2050.0) → (+1732.2) → panic 66685.6
- 08-31（14 事件）：10520.6 →(+1099.9)(+154.5)(+1200.0)(+415.5) 13390.5 → (+268748.0 最长静默) → 282138.5 →(+63020.5)(+17480.0)(+510.0)(+239.4)(+32731.1) 396119.6 →(+0.004)(+0.023 三连发) → (+3.098) → panic 396122.7
- 09-03（第 8 案，36 事件）：首症 71822.1 →（中段 33 个事件时刻未提供，不做推断）→ 322215 → (+10.0) → 322225 → (+21.6) → panic 322246.6。已知锚点：首症→panic 250,424.5s（69.56h）；末段间隔 10s；最后事件距 panic 21.6s。35 个 WARNING 中 irqbalance 占 32（91%），本机 7 案中最高占比，事件链大概率高度受 irqbalance 10s/20s 轮询节律塑形（此为基于进程构成的概率性陈述，非事件级断言）。

### 4.2 间隔分布（本机 7 案全部 95 个间隔 + 第 8 案已知 2 个间隔 = 97 个）

| 间隔分桶 | 件数 | 占比 | 说明 |
|---|---|---|---|
| <0.01s（同秒连发） | 6 | 6.2% | 发作呈"簇状"：同进程在同一 procfs 读循环中连续触发（如 08-26 的 1467.049344/1467.049716/1467.055105 三连发） |
| 0.01–10s | 6 | 6.2% | 簇内尾随 |
| 10–100s | 18 | 18.6% | 含第 8 案末段 10s 间隔 |
| 100–1000s | 37 | 38.1% | **主峰**：与 irqbalance（10s/20s 周期）和 pmdalinux（~60s 采样周期）的轮询节奏吻合 |
| 1000–10000s | 18 | 18.6% | |
| 1万–10万s（2.8h–27.8h） | 10 | 10.3% | 长静默 |
| >10万s（>27.8h） | 2 | 2.1% | 08-24 的 343,330s（95.4h）与 08-31 的 268,748s（74.6h） |

统计特征：min=0.000s，max=343,330s，median=262.8s。分布呈双态：短间隔簇（受周期性 procfs 读驱动，反映触发机会的节律）叠加超长静默（故障本身为间歇随机事件，与触发节律解耦）。第 8 案因中段事件序列未提供，仅纳入其 2 个已知间隔；其首症→panic 69.56h 的静默尺度与 08-17（64,130s=17.8h 单段静默）同量级。

### 4.3 首症→panic 前兆窗口（监控价值评估）

| 案 | 首症时刻 | panic 时刻 | 首症→panic | 最后 WARNING→panic |
|---|---|---|---|---|
| 08-14 | 29h01m | 31h40m | 2h38m | 17.6s |
| 08-17 | 46h59m | 66h32m | 19h33m | 262.8s |
| 08-24 | 13m55s | 149h18m | 149h04m | 7.5s |
| 15:42 | 28m27s | 21h20m | 20h52m | 75,101.9s（唯一长尾：最后事件后 20.9h 才致命） |
| 15:58 | 无 | 6m58s | — | —（无前兆） |
| 08-26 | 24m27s | 18h31m | 18h07m | 1,732.2s |
| 08-31 | 2h55m | 110h02m | 107h07m | 3.098s |
| 09-03(8) | 19h57m | 89h31m | 69h34m | 21.6s |

- 7/8 案有前兆，前兆窗口从 2.6h 到 149h 不等，无单调规律；
- 4/8 案（08-14/08-24/08-31/09-03）最后事件距 panic <22s，临近致命时事件加密，符合"故障活跃度在崩溃前升高"的相变特征；
- 15:58 案证明前兆不是必要条件：`grep Ignoring spurious` 监控有预警价值但不可作为唯一防线。


## 5. l1d_disable 模块时间线（仅案 4 与案 7 加载过）

### 案 4（15:42，l1d_disable 主实验场）

| 时刻(s) | 事件 |
|---|---|
| 51,302.4 | 模块加载（loading out-of-tree module taints kernel；signature 验证失败 taint） |
| 51,335.97 | L1D DISABLED on cpu 179（SCTLR_EL1=0x3464d999） |
| 51,399.9 | L1D RE-ENABLED（SCTLR_EL1=0x3464d99d），disable 持续 63.96s |
| 51,477.7 | 模块卸载 |
| 51,800.1 | 第二轮加载 + 立即 disable |
| 53,596.5 | RE-ENABLED（本轮 disable 持续 1,796.2s ≈ 29.9min）→ 尝试再 disable 报 "not disabled on 179 (cur=-1)" → 卸载 |
| 54,258.9 | 第三轮加载 |
| 54,557.8–59,642.8 | 8 次 disable/re-enable 循环，每轮 disable 持续约 598–600s（~10min） |
| 59,642.9 | 卸载 |
| 63,480.4 | 第四轮加载 + disable |
| 63,485.4 | RE-ENABLED（仅 5.05s）→ 卸载 |
| 76,809.3 | panic（最后一轮卸载后 13,324s ≈ 3.70h） |

模块内 disable 累计时长约 9,565s（2.66h），期间 0 次 WARNING（最后 WARNING 在 1,707s，早于模块加载）；卸载 3.7h 后照样致命。

### 案 7（08-31，三轮短实验）

| 时刻(s) | 事件 |
|---|---|
| 44,745.1 | 加载 |
| 44,747.2 | DISABLED |
| 44,749.3 | RE-ENABLED（2.0s）→ 卸载 |
| 82,326.1 | 加载 + DISABLED |
| 82,626.2 | RE-ENABLED（300.1s）→ 卸载 |
| 83,108.0 | 加载 + DISABLED |
| 84,007.2 | RE-ENABLED（899.2s）→ 卸载 |
| 396,122.7 | panic（最后卸载后 312,115s ≈ 86.7h） |

三轮 disable 累计 1,201s，期间 0 次 WARNING（13,391s→282,139s 的静默期覆盖了全部三轮实验）；卸载 86.7h 后照样致命。

合并结论（与既往报告一致并加强）：l1d_disable（SCTLR_EL1.C 位清零）对致命崩溃零抑制效果。案 4 卸载 3.7h 后崩溃、案 7 卸载 86.7h 后崩溃、案 5/6 从未加载照样崩溃。四组独立反例。


## 6. RAS / EDAC / GHES / BERT 静默性证明

本机七案 dmesg 中的全部 RAS 相关记录经分类后仅含启动期注册信息，运行期（>10s）除案 6 的 rasnode 扫描外零条异常记录（第 8 案 RAS 记录协调方未提供，如实标注）：

| 记录类型 | 内容 | 出现案 |
|---|---|---|
| ACPI 表在位 | `ACPI: BERT/HEST/ERST/EINJ … (v01 HISI HIP08)` | 本机 7/7（BERT 表存在且尺寸 0x30，即空壳） |
| CPU 特性 | `CPU features: detected: RAS Extension Support` | 本机 7/7 |
| HEST | `HEST: Table parsing has been initialized.` | 本机 7/7 |
| GHES | `GHES: APEI firmware first mode is enabled by APEI bit and WHEA _OSC.` | 本机 7/7 |
| EDAC | `EDAC MC: Ver: 3.0.0` + `ghes_edac: This system has 32 DIMM sockets.` + `EDAC MC0: Giving out device to module ghes_edac … (INTERRUPT)` | 本机 7/7 |
| CE/UE 记录 | 无（本机 7 案 × 运行期 grep "corrected/uncorrectable/ECC error/machine check/Hardware error" 全部零命中） | 0/7 |
| rasnode.ko 扫描 | 8026.6s 加载，"sweeping RAS error nodes on all CPUs"，192 核 × 5 节点（ERRIDR=0x4），CPU179 各节点 FR/CTLR/STATUS/ADDR/MISC 读数与其余 191 核逐位一致（例：node0 全机 FR=0x4842 CTLR=0x101 STATUS=0xff；node4 全机为 0） | 仅案 6（1,162 行） |

同时本机七案均无 OOM、无 I/O error、无 thermal 事件（除启动期 thermal governor 注册）、无其他子系统异常。故障孤悬于 CPU179 的私有路径，全部共享资源观测面（L3/互连/DRAM/固件）静默。


## 7. 压测 / 业务负载活动时间线

| 案 | 负载证据 | 明细 |
|---|---|---|
| 08-14 | memcpy1（PID 2606819）@113,811s、control（PID 2823806）@113,980s | 两进程在开机 29h 后才出现于 WARNING 记录（SDC 压测工具的控制/worker 进程名），且其 WARNING 由 `_find_next_and_bit`（调度路径）而非 `__memcpy` 触发，即压测进程只是在 CPU179 上被调度时"路过"雷区，并非其自身 memcpy 代码触发 |
| 08-24 | 目录内有 `sdc_long` ELF 二进制（71,568 字节，aarch64 动态链接，导入 `sched_setaffinity/posix_memalign/clock_gettime`，即绑核内存压测探针）；dmesg 内无该进程名记录 | 34 个 WARNING 中 33 个来自 irqbalance、1 个来自 bash，压测进程自身未触发任何记录 |
| 08-26 | mi-scavenger（PID 256855）仅出现于致命 Oops（futex 唤醒路径被动崩溃） | — |
| 08-31 | silifuzz_orches 5 次活动：18149/21472/21943/22675/80947s（内容均为 `memfd_create() called without MFD_EXEC or MFD_NOEXEC_SEAL set`，即编排器生成测试载荷二进制） | silifuzz 活动窗口（18.1kh–81.0kh）与 WARNING 静默期（13.4kh–282.1kh）重叠：压测在跑、故障静默，负载强度与发作频率无正相关 |
| 其余 3 案 | 无压测进程记录 | — |

跨案观察：八案的 138 起事件中，130 起 WARNING 的触发进程为系统守护（irqbalance/pmdalinux 共 121 起，占 93.1%；含第 8 案 irqbalance×32/pmdalinux×3/rcu_sched×1），仅 6 起来自其他进程（memcpy1/control/bash/…），8 起 Oops 的宿主进程涵盖 kworker×3、swapper、claude、mi-scavenger、rcu_sched×2。触发者与负载完全无关，唯一公共变量是被调度到 CPU179 上执行。


## 8. 独立验证结论与既往报告对照

### 8.1 与既往报告一致的部分（本次独立重算复核通过）

| 既往论断 | 本次独立复核 | 结果 |
|---|---|---|
| 六案 WARNING 计数 12/26/34/1/0/9 | 逐案 `grep -c` | 完全一致 |
| 六案事件 100% CPU179 | 逐案 CPU 号 uniq -c | 完全一致 |
| 5/6 致命崩溃命中 fbG+0x140 | pc 字段逐案核对 | 一致（08-24 为 bio_add_page+0xf0；新增两案也命中，累计 7/8） |
| Code 字段六案同字 | 独立 grep "Code:" | 一致（含第 8 案共 7 案同字） |
| x27=(x1+x20) mod 2⁶⁴ 闭合 | 脚本逐位计算 | 7/7 fbG 案闭合（既往报 08-14/08-17 闭合，本次扩验 15:42/15:58/08-26/08-31/09-03 全闭合） |
| FAR=x27+0x120 | 低 48 位计算 | 7/7 闭合（第 8 案含高 16 位整体闭合） |
| x23≡0x400、x24−x21≡0x5350 | 逐案验证 | x23 7/7 一致；x24−x21 本机 6/6 一致 |
| 首症时刻整体前移趋势（29h→47h→分钟级） | 复核八案：29.0h→47.0h→0.23h→0.47h→无→0.41h→2.9h→19.9h | 不成立：无单调性，属间歇故障随机特征（见 8.2 第 7 条） |
| l1d_disable 无效（15:42 卸载 3.7h 后 panic） | 复核 + 新增 08-31 卸载 86.7h 后 panic + 09-03 未加载照崩 | 一致并加强 |
| RAS 全静默 | 本机 7 案零 CE/UE（第 8 案〔未提供〕） | 一致 |
| 每次开机必致 panic | 8/8 | 一致（新增第 7、8 案仍命中） |
| 既往报告"x22==x26 成对"不变式 | 7 个 fbG 案中 6 成立 1 破例（08-31 差 0x60） | 既往表述过强，应降级为高频巧合（见 8.2 第 5 条） |

### 8.2 本次普查发现的新事实 / 与既往表述的差异（重要）

1. **目录数量**：任务声称本机 8 案，本机磁盘实际 7 案；第 8 案（09-03）由协调方确认在远端单板 172.168.160.42 上。既往报告（08-26）覆盖 6 案，08-31 与 09-03 为本次新增的第 7、8 案。
2. **总事件数**：既往报告六案合计"82 WARNING + 6 Oops = 88 起"；本次八案合计 130 WARNING + 8 Oops = 138 起（新增 08-31 的 13+1 与 09-03 的 35+1）。既往六案的 82+6=88 与本次前六案复核（12+26+34+1+0+9=82 WARNING，6 Oops）完全吻合。
3. **低迭代号新现象**：既往五案 fbG 崩溃的迭代 CPU 号为 176/175/146/176/179（均 >100，与 NUMA 拓扑下 CPU179 所在调度域的遍历范围吻合）；08-31 崩溃在 i=60、09-03 崩溃在 i=12。连续两案出现"低号 CPU"迭代，说明雷区不在特定被遍历对象，而在遍历循环本身在 CPU179 上的执行。
4. **腐化形态分类学修正（本报告最重要的模型更新，含对第 7 案结论的自我修正）**：
   - 初版报告曾将 08-31 的 x20 判为"纯 ROR16 相位旋转"（依据 ROL16(x20)=ffffbe56fb25a000 形似合法 offset）；协调方随后提供 crash 内存真值 `__per_cpu_offset[60]=0xffffbe56fa9b6000`，证明该候选值与真值不符。初版此条结论错误，现予修正。
   - 用真值做逐字节溯源（本次独立复算），得到比协调方"相位撕裂+源污染"更进一步的错行拼接（wrong-line assembly）模型：
     - 第 8 案（完美闭合）：`x20 = ROR8(entry[123])` 逐位相等、误差 0 bit〔复算✓〕。装载目标为 entry[12]（距 892 字节 ≈ 14 个 cacheline），实际返回 entry[123] 的完整 8 字节再右旋 1 字节。目标行真值独有的字节 `dc 4d 60` 完全不存在于 x20 中。
     - 第 7 案（跨行拼接）：`ROL16(x20) = ffffbe56fb25a000`，其低 4 字节 `fb 25` = entry[125][4:6]、`a0 00` = entry[126][6:8]，而高 4 字节 `ff ff be 56` 为本数组该区域所有条目共享的公共前缀〔复算✓〕。目标行真值独有的字节 `fa 9b 60 00` 同样完全不存在于 x20 中。
     - 08-14 案（既往 crash 验证）：x20 = ROL16(entry[1])，目标为 entry[176]，同样是取错行（既往报告已给出 entry[1] 真值，但当时解读为"相邻行旋转"，未强调"目标行独有信息为 0"这一层）。
     - 08-17/15:58 案：x20 与 ROR8(真值候选) 逐位相等，但真值系形态反推（那两案 vmcore 不可用/未验证），不能区分"纯相位旋转"还是"错行恰好共享前缀"。
   - 统一结论：在有内存真值的三案（08-14/08-31/09-03）中，致命装载返回值不含任何目标行独有字节，100% 由数组内其他条目的字节（含共享前缀）拼接/旋转而成。这是"装载返回了错误源的数据"的直接位级证据，将既往"相位撕裂/源污染"的模糊表述收敛为可复算的"错行取数 + 字节相位旋转"。它同时排除"目标数据位翻转"（否则目标行字节应部分残留），与"内存完好、寄存器收坏"的总判定完全自洽。
5. **x22==x26 指纹在 08-31 破例、第 8 案恢复**：既往报告将"x22==x26 成对"列为跨开机不变式；本次复核 7 个 fbG 案中 6 个成立，唯 08-31 差 0x60，第 8 案恢复相等。该"不变式"应降级为"高频巧合"：从调度器代码看 x22/x26 分别缓存不同相位变量，本就不保证相等。既往报告此条表述过强。
6. **FAR 高 16 位与寄存器不一致现象**：既往报告记录 08-14 案"FAR 高字节 HW 上报 00… vs x27 高字节 d9…"。本次发现同样现象存在于 08-31（x27 高 16 位=a000，FAR 高 16 位=0000；低 48 位差恰 0x120）与 08-24（x3 高 8 位=0x55，FAR 高 8 位=0x00，低 48 位相等）。第 8 案 FAR 与 x27+0x120 含高 16 位整体闭合（00ff 域），不属此现象。即 8 案中 3 案出现"异常注入窗口内，硬件 FAR 记录与寄存器呈现值高位不一致"。该现象支持"发作是短窗口内多次受扰"的模型，但不能排除 MMU 对非规范地址的 FAR 截位（TTBR 查找时高位被忽略），此处如实标注两种解释并存。
7. **"首症时刻前移"趋势不成立**：八案首症时刻 29.0h→47.0h→0.23h→0.47h→无→0.41h→2.9h→19.9h，存活 31.7h→66.5h→149.3h→21.3h→0.12h→18.5h→110.0h→89.5h。故障的时间参数在八案中无单调趋势，符合间歇性硬件故障的随机特征，不宜拟合 MTBF（既往报告已声明此点，本次数据继续支持）。
8. **08-26 报告中 15:58 案 x25 记为 146**：本次复核 x25=0x92=146 ✓ 一致。08-14 案 x25=0xb0=176 ✓。既往数字全部复核无误。
9. **第 8 案与前案的链式关系**：第 8 案开机时刻（2026-08-31 00:54:25）距第 7 案 dump（00:47:32）仅 6m53s〔复算✓〕。八案的"上案 dump → 本案开机"间隔全表：08-17 案 7m56s、08-24 案 22h58m、15:42 案 19m08s、15:58 案 8m47s、08-26 案 7m52s、08-31 案 8m02s、09-03 案 6m53s。7 案中 6 案在崩溃后 20 分钟内重启复测，仅 08-24 案隔 22.98h，体现连续高密度复现的取证节奏。
10. **rcu_sched 两连庄**：第 7、8 案的致命 Oops 宿主均为 PID 16 rcu_sched。RCU 内核线程频繁睡眠/唤醒的调度模式使其反复进入 newidle_balance 路径，成为雷区的高频过路者；但这只是机会性相关，不改变"任意进程路过即中招"的判定（其余 6 案宿主各不相同）。

### 8.3 本次独立得出的结论

1. **CPU179 单核私有性**：138/138 事件（含 8 次致命）全部发生于 CPU 179，铁证级。触发进程横跨 idle 任务、内核线程、RCU 内核线程、系统守护、压测进程、交互 shell，与"谁"无关，只与"在哪"有关。
2. **故障形态学（修正后）**：致命装载腐化分三大类：零塌缩（x20=0，2 案）、错行拼接+相位旋转（3 案经内存真值验证：ROR8/ROL16/ROR16 作用于数组内错误条目）、变址装载乱码（08-24）。所有已验证形态的共同点是返回值不含目标行独有信息：数据通路交付了错误源/错误相位的字节，而非数据位翻转。95 次本机 spurious 事件 100% 为"重走成功"（内核自判 spurious，页表完好），其中 92 起（96.8%）ESR=0x96000044（WnR=1 写访问 + FSC=L0）、3 起 0x96000004（读访问 + L0）。读、写、页表遍历三类访存全部受扰，指向 LSU/DCU 公共返回通路。
3. **零塌缩子族与 FSC=L3 的对应**再次验证（15:42/08-26 两案 pte=0 且 x27==x1）。
4. **代数闭合链**：8 案 Oops 中 7 案 fbG 形态的 x27=(x1+x20) 全部机器验证逐位闭合（含第 8 案），FAR=(x27+0x120)（低 48 位；第 8 案含高位整体闭合），08-24 bio 案 FAR 与 x3 低 48 位相等。即每一次致命异常的地址都可从上游寄存器精确推导，无一是"无源之水"的随机地址，进一步排除 MMU/TLB 自发故障。
5. **percpu 物理布局三方互证**：08-26/08-31/09-03 三案的 `__per_cpu_offset` 等差数列 step 均为 0x22000，且第 8 案反事实地址 rq(12)=ffff8000801af6c0 与 08-26 案 rq(179)=ffff8000817dd6c0 之差恰为 (179−12)×0x22000〔复算✓〕。跨开机、跨 dump 的独立读数与等差模型完全自洽，反事实实验（若收到真值则读到有效数据、不会崩溃）在第 8 案第三次独立成立。
6. **处置建议与既往一致且更紧迫**：offline CPU179 + RMA；不要部署 l1d_disable（五组反例）；`grep Ignoring spurious` 作为前兆监控（有效率 7/8，15:58 案无前兆；且 4/8 案最后事件距 panic <22s，监控需低延迟告警）。


## 9. 附录：八案致命 Oops 完整寄存器原始记录（供代数闭合复验）

> 提取方法：python 正则解析 dmesg 寄存器行（x0–x29 + sp），逐案核对 30/30 无缺失。所有十六进制为原始日志逐字抄录。

### 案 1（08-14，fbG+0x140，kworker/179:1H，PID 1986）

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
sp : ffff8000b722b960                  pstate: 204000c9
x0 : 00000000000000b0    x1 : ffffa6c96a4996c0    x2 : 0000000000002001
x3 : 0000000000000030    x4 : 0000000000000002    x5 : ffff000000000000
x6 : 00000000000000b0    x7 : 0000000000000000    x8 : ffff8000b722bac8
x9 : ffffa6c968a6ae58    x10: 0000000000000000    x11: 7f7f7f7f7f7f7f7f
x12: 0101010101010101    x13: 0000000000000038    x14: 0000000000000000
x15: 0000000000000040    x16: ffffa6c96937e9f0    x17: 0000000000000000
x18: 0000000000000000    x19: ffff8000b722bb70    x20: d93715ba0000ffff ◄
x21: ffffa6c96a88fcb0    x22: ffff604003e5f8a0     x23: 0000000000000400
x24: ffffa6c96a895000    x25: 00000000000000b0     x26: ffff604003e5f8a0
x27: d936bc836a4a96bf ◄ x28: ffff8000b722ba70     x29: ffff8000b722bae0
FAR: 0036bc836a4a97df    ESR: 0000000096000004     FSC: level 0
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
闭合: x1+x20 = d936bc836a4a96bf = x27 ✓；FAR低48 = x27低48+0x120 ✓；FAR高16(0036)≠x27高16(d936)
```

### 案 2（08-17，fbG+0x140，swapper/179，PID 0）

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
sp : ffff800081f1bb30                  pstate: 20400009
x0 : 00000000000000af    x1 : ffffd7d8cdf196c0    x2 : 0000000000001c00
x3 : 000000000000002f    x4 : 0000000000000002    x5 : ffff800000000000
x6 : 00000000000000af    x7 : 0000000000000000    x8 : ffff800081f1bc98
x9 : ffffd7d8cc4eae58    x10: 00000000000002ea    x11: 0000000000000047
x12: 0000000000000000    x13: ffffff0000000000    x14: 0000000100000013
x15: 0000aaab090d1eb0    x16: ffff800081f18000    x17: ffffa827b38c4000
x18: 0000000000000000    x19: ffff800081f1bd40    x20: 00ffffa827b20fe0 ◄
x21: ffffd7d8ce30fcb0    x22: ffff604003e9ec00     x23: 0000000000000400
x24: ffffd7d8ce315000    x25: 00000000000000af     x26: ffff604003e9ec00
x27: 00ffd780f5a3a6a0 ◄  x28: ffff800081f1bc40     x29: ffff800081f1bcb0
FAR: 00ffd780f5a3a7c0    ESR: 0000000096000004     FSC: level 0
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
闭合: x1+x20 = 00ffd780f5a3a6a0 = x27 ✓；FAR = x27+0x120 ✓（含高16位）
真值重建: x20<<8 = ffffa827b20fe000（高16=ffff, 页对齐）→ x20 = ROR8(真值候选)
```

### 案 3（08-24，bio_add_page+0xf0，kworker/u391:3，PID 2077673）

```
pc : bio_add_page+0xf0/0x1a0           lr : bio_add_folio+0x30/0x50
sp : ffff8001e547b5e0                  pstate: 00400009
x0 : ffff60401dabd460    x1 : 055ffffe0000806b    x2 : 0000000000000002
x3 : 553c521da2e9b99f ◄  x4 : ffff602018debf00     x5 : 0000000000010000
x6 : ffff8001e547b704    x7 : 000000000000000c     x8 : 0000000000000010
x9 : ffffc360a8593ab0    x10: ffff404420819b68     x11: 0000000000001000
x12: 0000000000000030    x13: ffff60407f2b4698     x14: 0000000000000000
x15: 0000000000000800    x16: 00000000fffffffe     x17: 00000000ffffffff
x18: 0000000000000000    x19: ffff60401b366738     x20: 0000000000010000
x21: 0000000000000000    x22: fffffd010d971400     x23: 0000000000010000
x24: ffff404420819b68    x25: ffff8001e547baf8     x26: 000000000f569bf0
x27: ffff60403d00cae0    x28: 0000000000010000     x29: ffff8001e547b5f0
FAR: 003c521da2e9b99f    ESR: 0000000096000004     FSC: level 0
Code: f9400021 8b020060 f8626863 d375dc22 (f9400061)
关系: FAR低48 == x3低48 ✓；x3高8(0x55) ≠ FAR高8(0x00)——高位受扰/截位
致命链: ldr x3,[x3,x2](f8626863, 变址装载) 返回乱码 → ldr x1,[x3](f9400061) 解引用崩溃
```

### 案 4（15:42，fbG+0x140，claude，PID 2018836）

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
sp : ffff8001f03fb720                  pstate: 204000c9
x0 : 00000000000000b0    x1 : ffffa5aa9b5a96c0    x2 : 0000000000002149
x3 : 0000000000000030    x4 : 0000000000000002    x5 : ffff000000000000
x6 : 00000000000000b0    x7 : 0000000000000000    x8 : ffff8001f03fb888
x9 : ffffa5aa99b7ae58    x10: 0000000000000000    x11: 00000000000000ad
x12: 000000000002dc17    x13: 0000000000000000    x14: 0000000000000000
x15: 0000ffffd47eeeb0    x16: 0000000000000000    x17: 0000000000000000
x18: 0000000000000000    x19: ffff8001f03fb930    x20: 0000000000000000 ◄（零塌缩）
x21: ffffa5aa9b99fcb0    x22: ffff604003e99780     x23: 0000000000000400
x24: ffffa5aa9b9a5000    x25: 00000000000000b0     x26: ffff604003e99780
x27: ffffa5aa9b5a96c0 ◄  x28: ffff8001f03fb830     x29: ffff8001f03fb8a0
FAR: ffffa5aa9b5a97e0    ESR: 0000000096000007     FSC: level 3 (pte=0)
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
闭合: x1+0 = ffffa5aa9b5a96c0 = x27 = x1 ✓；FAR = x27+0x120 ✓
页表: pgd=10006057fffff403 pud=10006057ffffe403 pmd=10006057ffffa403 pte=0000000000000000
```

### 案 5（15:58，fbG+0x140，kworker/179:1H，PID 1931）

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
sp : ffff8000ae213960                  pstate: 204000c9
x0 : 0000000000000092    x1 : ffffb378e25e96c0    x2 : 0000000000000800
x3 : 0000000000000012    x4 : 0000000000000002    x5 : fffffffffffc0000（原文16字符，见文末勘误）
x6 : 0000000000000092    x7 : 0000000000000000    x8 : ffff8000ae213ac8
x9 : ffffb378e0bbae58    x10: 0000000000000000    x11: 00000000000001ff
x12: ffff8000ae2139f8    x13: 000000003b73e310     x14: 0000000000000001
x15: 0000000000000008    x16: 0000000000000000    x17: 0000c00000000000
x18: 0000000000000000    x19: ffff8000ae213b70    x20: 00ffffcc879da2e0 ◄
x21: ffffb378e29dfcb0    x22: ffff604003e26c00     x23: 0000000000000400
x24: ffffb378e29e5000    x25: 0000000000000092     x26: ffff604003e26c00
x27: 00ffb34569fc39a0 ◄  x28: ffff8000ae213a70     x29: ffff8000ae213ae0
FAR: 00ffb34569fc3ac0    ESR: 0000000096000004     FSC: level 0
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
闭合: x1+x20 = 00ffb34569fc39a0 = x27 ✓；FAR = x27+0x120 ✓
真值重建: x20<<8 = ffffcc879da2e000（高16=ffff, 页对齐）→ x20 = ROR8(真值候选)
x5 原文: fffffffffffc0000（低12位=000）
```

### 案 6（08-26，fbG+0x140，mi-scavenger，PID 256855）

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
sp : ffff8001e8ffb740                  pstate: 204000c9
x0 : 00000000000000b3    x1 : ffffa29301d796c0    x2 : 00000000000033de
x3 : 0000000000000033    x4 : 0000000000000002    x5 : ffff800000000000
x6 : 00000000000000b3    x7 : 0000000000000000    x8 : ffff8001e8ffb8a8
x9 : ffffa2930034ae58    x10: ffff8000817c9430     x11: 000000000000005e
x12: 0000000000019c58    x13: 0000000000000000    x14: 0000000000000000
x15: 0000ffffa5497e20    x16: 0000000000000000    x17: 0000000000000000
x18: 0000000000000000    x19: ffff8001e8ffb950    x20: 0000000000000000 ◄（零塌缩）
x21: ffffa2930216fcb0    x22: ffff604003e27660     x23: 0000000000000400
x24: ffffa29302175000    x25: 00000000000000b3     x26: ffff604003e27660
x27: ffffa29301d796c0 ◄  x28: ffff8001e8ffb850     x29: ffff8001e8ffb8c0
FAR: ffffa29301d797e0    ESR: 0000000096000007     FSC: level 3 (pte=0)
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
闭合: x1+0 = ffffa29301d796c0 = x27 = x1 ✓；FAR = x27+0x120 ✓
页表: pgd=10006057fffff403 pud=10006057ffffe403 pmd=10006057ffffa403 pte=0000000000000000
```

### 案 7（08-31，fbG+0x140，rcu_sched，PID 16）：新增案

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
sp : ffff8000821ab830                  pstate: 204000c9
x0 : 000000000000003c    x1 : ffffc1a985e596c0    x2 : 0000000000009063
x3 : 000000000000003c    x4 : 0000000000000000    x5 : f000000000000000
x6 : 000000000000003c    x7 : 0000000000000000    x8 : ffff8000821ab8b8
x9 : ffffc1a98442ae58    x10: ffff8000817c9430     x11: 00000000000000aa
x12: 0000000000000000    x13: ffffc1a986281988     x14: 0000000000000004
x15: 0000000000000000    x16: 0000000000000000     x17: 0000000000000000
x18: 0000000000000000    x19: ffff8000821aba40    x20: a000ffffbe56fb25 ◄（错行拼接，见下）
x21: ffffc1a98624fcb0    x22: ffff604003e27120 ◄   x23: 0000000000000400
x24: ffffc1a986255000    x25: 000000000000003c     x26: ffff604003e270c0 ◄
x27: a000c1a9443c91e5 ◄  x28: ffff8000821ab860     x29: ffff8000821ab9b0
FAR: 0000c1a9443c9305    ESR: 0000000096000004     FSC: level 0
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
闭合: x1+x20 = a000c1a9443c91e5 = x27 ✓（逐位）；FAR低48 = x27低48+0x120 ✓；FAR高16(0000)≠x27高16(a000)
真值(crash 已验证): __per_cpu_offset[60] = ffffbe56fa9b6000；数组 base=ffffbe56fa1be000, step=0x22000
  entry[125] = ffffbe56fb258000, entry[126] = ffffbe56fb27a000
  ROL16(x20) = ffffbe56fb25a000 = 公共前缀 ffffbe56 + entry[125][4:6](fb25) + entry[126][6:8](a000)
  → x20 = ROR16( entry[125]尾段 与 entry[126]尾段 的拼接 )，目标行独有字节 fa9b6000 完全不存在 ✓
破例指纹: x22≠x26（差0x60）；x25=60（迭代号首次 <100）；x5=f000000000000000（fbG案中唯一非ffff…0000形态）
页表: user pgtable pgdp=0000204003e1c000, pgd=0, p4d=0（用户域地址被内核态触碰）
```

### 案 8（09-03 远端，fbG+0x140，rcu_sched，PID 16）：新增案【协调方提供关键寄存器；代数与真值断言已本机独立复算】

```
pc : find_busiest_group+0x140/0xb60    lr : find_busiest_group+0x11c/0xb60
x1 : ffffc9e8a3cd96c0    x20: 00ffffb617dd3940 ◄    x27: 00ffc99ebbaad000 ◄
x25: 000000000000000c    x23: 0000000000000400     x22 == x26: ffff604003e9e3c0
FAR: 00ffc99ebbaad120    ESR: 0000000096000004     FSC: level 0（address between user and kernel ranges）
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
（其余 26 个通用寄存器与 sp/pstate/sp/调用栈：原始 dmesg 在远端，未提供，不做推断）

闭合（本机复算✓）:
  x1 + x20 = 00ffc99ebbaad000 = x27（逐位）
  x27 + 0x120 = 00ffc99ebbaad120 = FAR（含高16位整体闭合）
真值（crash 8.0.4 远端执行，协调方提供；本机复算自洽✓）:
  __per_cpu_offset[12] = ffffb617dc4d6000（非零；数组 base=ffffb617dc33e000, step=0x22000，完美等差）
  entry[123] = ffffb617dd394000
  ROL8(x20) = ffffb617dd394000 = entry[123]（逐位相等，误差 0 bit）
  → x20 = ROR8( entry[123] )：装载目标 entry[12]（距 892 字节 ≈ 14 个 cacheline），
    实收 entry[123] 完整 8 字节再右旋 1 字节；目标行独有字节 dc4d60 完全不存在于 x20 ✓
反事实（本机复算✓）:
  x27_true = x1 + true[12] = ffff8000801af6c0
  vtop: VALID（PTE e80037ffe2ef03 VALID|SHARED|AF|DIRTY）
  x27_true+0x120 处值 = 0x3ff = rq(12).cfs.avg.load_avg = 1023 —— 若收到真值则平静读到 1023，不会崩溃
真值域互证（本机复算✓）:
  rq(179)_true(08-26案) − rq(12)_true(本案) = 0x162e000 = (179−12) × 0x22000
```



案 5 附录中 x5 一行排版有误。dmesg 原文为 `x5 : fffffffffffc0000`（16 个十六进制字符），即 x5 = 0xfffffffffffc0000（二进制低 46 位中高 28 位为 1、低 18 位为 0，呈对齐掩码形态）；与该寄存器在其他 fbG 案中的 `ffff000000000000` / `ffff800000000000` 形态同族，均为掩码用途，非异常值。上文案 5 表格中该行以本注记为准。


## 10. 可复现命令集（本次普查实际使用）

```bash
# ① 目录与文件普查
find /home/sdc/wangxu/vmcore0102/ -maxdepth 1 -type d | sort
for d in /home/sdc/wangxu/vmcore0102/*/; do wc -l "$d/vmcore-dmesg.txt"; done
# 结果：7 案，行数 3174/3813/4466/2847/2628/4193/3243

# ② WARNING 计数与 CPU 分布（每案）
grep -oE "WARNING: CPU: [0-9]+" vmcore-dmesg.txt | sort | uniq -c
# 结果：12/26/34/1/0/9/13，全部 "WARNING: CPU: 179"

# ③ spurious 事件清单（时间戳+FAR）
grep -n "Ignoring spurious kernel translation fault" vmcore-dmesg.txt

# ④ WARNING 块内 PID/Comm/ESR(x19)
awk '/WARNING: CPU/{f=1} f && /Comm:/{print; f=0}' vmcore-dmesg.txt
awk '/WARNING: CPU/{f=1} f && /x19:/{split($0,a,"x19: "); split(a[2],b," "); print b[1]; f=0}' vmcore-dmesg.txt | sort | uniq -c
# ESR 分布：0x96000044 ×92, 0x96000004 ×3（08-14×2, 08-24×1）

# ⑤ 致命 Oops 全字段
grep -A45 "Unable to handle kernel paging request" vmcore-dmesg.txt

# ⑥ l1d_disable / rasnode / 压测
grep -n "l1d_disable" vmcore-dmesg.txt
grep -n "rasnode" vmcore-dmesg.txt | head
grep -inE "silifuzz|orchestr|memcpy1|scavenger" vmcore-dmesg.txt

# ⑦ RAS 静默性
grep -icE "corrected|uncorrectable|ECC error|machine check|Hardware error" vmcore-dmesg.txt  # 全部=0

# ⑧ 代数闭合（禁止手算）
python3 -c 'print(hex((0xffffc1a985e596c0+0xa000ffffbe56fb25)&(2**64-1)))'
# = a000c1a9443c91e5 = 案7 x27 ✓
python3 -c 'print(hex((0xffffc9e8a3cd96c0+0x00ffffb617dd3940)&(2**64-1)))'
# = 00ffc99ebbaad000 = 案8 x27 ✓

# ⑨ 错行拼接验证（案 8，误差 0 bit）
python3 - <<'EOF'
e123 = 0xffffb617dd394000          # __per_cpu_offset[123]（crash 真值）
obs  = 0x00ffffb617dd3940          # 崩溃时 x20 实收值
ror8 = ((e123 >> 8) | (e123 << 56)) & (2**64-1)
print(ror8 == obs)                  # True → x20 = ROR8(entry[123])
EOF

# ⑩ 第 7/8 案反事实（禁止手算）
python3 -c 'print(hex((0xffffc9e8a3cd96c0+0xffffb617dc4d6000)&(2**64-1)))'
# = ffff8000801af6c0 = 案8 反事实 rq(12)，vtop VALID ✓
```


*报告生成：2026-09-03 · SDC 故障取证统计分析会话 · 本机 7 案数字源自对 vmcore-dmesg.txt（md5 见 §0）的独立 grep/awk/python 统计（提取脚本 /tmp/sdc_census.py，含两次自身 bug 修复）；第 8 案（远端 172.168.160.42）数字为协调方摘录，其中代数闭合、反事实地址、等差数列、错行拼接（ROR8(entry[123]) 逐位相等）均经本机 python 独立复算验证；初版报告对第 7 案 x20 的"纯 ROR16"判读经协调方 crash 真值纠正为"错行拼接"模型，已在 §8.2 第 4 条如实记录该自我修正。*

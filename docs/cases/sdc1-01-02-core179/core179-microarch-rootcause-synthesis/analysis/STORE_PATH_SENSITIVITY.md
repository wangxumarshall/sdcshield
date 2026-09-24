# 存储通路（Store Path/STL）与 LSQ 的 SDC 敏感性：12 次 vmcore-dmesg 全量法证

> 数据源：`/home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt`（12 份）
> 内核：6.6.0-145.3.23.154.oe2403sp3.aarch64（openEuler 24.03 SP3）
> 机器：Yangtze Computing R240K V2（Kunpeng 920 / TaiShan V110，192 逻辑 CPU）
> 反汇编基准：`/usr/lib/debug/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux`（本机 debuginfo，与目标内核同版本）
> 本文所有结论均附可复核命令与真实输出摘录。日期：2026-09-04。


## §1 数据与方法

### 1.1 事件底数

```
$ grep -c "Ignoring spurious" /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt
...08-14-19:07:04: 12    ...08-17-13:47:08: 26    ...08-24-18:03:07: 34
...08-25-15:42:24: 1     ...08-25-15:58:09: 0     ...08-26-10:37:27: 9
...08-31-00:47:32: 13    ...09-03-18:25:12: 35    ...09-04-09:15:42: 2
...09-04-10:27:58: 2     ...09-04-11:00:00: 0     ...09-04-12:33:31: 1
```

合计 135 起 spurious + 12 起 fatal Oops = 147 起访存异常，全部 `CPU: 179`。

```
$ grep -A3 "Ignoring spurious" .../127.0.0.1-*/vmcore-dmesg.txt | grep "WARNING: CPU" | grep -vc "CPU: 179"
0        # 135/135 全部 CPU179
```

12 次会话内核总运行时长 ≈ 1,816,002 s ≈ 21.0 天（各 dmesg 末条时间戳求和）。

### 1.2 WnR 判定方法（三条独立途径互证）

途径 a：x19 = ESR（callee-saved 存活性证明）

`__do_kernel_fault` 反汇编（vmlinux，`ffff800080044680`）：

```
ffff800080044698:  mov x19, x1          # x19 = esr（第2参数）
ffff80008004469c:  mov x20, x2          # x20 = regs
ffff8000800446a4:  lsr w22, w1, #26     # x22 = ESR>>26 = EC
ffff8000800446a8:  mov x21, x0          # x21 = addr（第1参数 = FAR）
```

WARNING 块的寄存器 dump 发生在 `__do_kernel_fault+0x130`（`brk #0x800`，即 WARN 触发点），
此时 x19/x21/x22 是本函数自己写入的值；x19–x28 为 callee-saved，途经 `__warn_printk → printk → vsnprintf` 均被保留。
故 dump 中 x19 = ESR、x21 = FAR、x22 = EC，三者互相构成校验。

途径 b：x22 = EC 独立佐证

全部 135 块均有 `x22 = 0x25` = ESR>>26 = EC 0x25（Data Abort, current EL），与 x19 的高 6 位一致，形成闭环。

途径 c：受害指令反汇编与 WnR 语义对账（见 §2.2），127 起 WnR=1 事件的受害指令是 store，8 起 WnR=0 的受害指令全是 load，零矛盾。

寄存器 dump 的"事后性"说明（诚实边界）：x12–x17 为 caller-saved，是 `__warn_printk` 格式化本次告警文本的 vsnprintf 残留，135/135 事件满足
`x12+x13（小端内存序拼接）== FAR 的 16 个十六进制字符` 且 `x14..x17 == "tion fault at virtual address ff"`：

```
$ python3  # 以 08-14 首事件为例
x12=3034303666666666 -> 内存序 'ffff6040'
x13=3664373939303630 -> 内存序 '060997d6'
拼接 = 'ffff6040060997d6' == FAR   （135/135 匹配）
```

这一方面证明 dump 确实发生在 printk 之后（时序自洽），另一方面说明 x12–x17 不能用作故障数据内容（任务背景第 4 维度的"ASCII 字符串分析"在此被证伪：那是告警文本自身，不是 memcpy 源数据）。

### 1.3 受害指令定位方法（帧精确性证明）

arm64 异常帧的首帧 = `pt_regs->pc` = 出错指令精确地址（无 -4 调整）。用 08-14 fatal Oops 的 `Code:` 行独立验证：

```
pc : find_busiest_group+0x140/0xb60
Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
```

对照 vmlinux 反汇编：

```
ffff80008013ae38 <find_busiest_group+0x130>: f9400782  ldr x2,[x28,#8]
ffff80008013ae3c <find_busiest_group+0x134>: f879d814  ldr x20,[x0,w25,sxtw#3]
ffff80008013ae40 <find_busiest_group+0x138>: 2a1903e0  mov w0,w25
ffff80008013ae44 <find_busiest_group+0x13c>: 8b14003b  add x27,x1,x20
ffff80008013ae48 <find_busiest_group+0x140>: f9409377  ldr x23,[x27,#288]   <- 括号内指令与 pc 逐字对齐
```

四个前置字 + 括号字与 pc-0x10..pc 完全对齐 ⇒ 括号内即 pc 指令。故 spurious 块首帧 `__memcpy+0x80` 即出错指令本身。

### 1.4 全量提取脚本

```
$ python3 /tmp/extract5.py    # 遍历 12 文件，按 "Ignoring spurious" 切块，
                              # 提取 FAR/x12-x28、Call trace、el1h_64_sync 后首帧（受害指令）
total: 135
```

（脚本逻辑见 §1.2/1.3 描述，正则：`^\[\s*\d+\.\d+\]\s+(\S+)$` 取帧，`el1h_64_sync+` 的下一帧为受害指令。）


## §2 逐维度证据

### 2.1 维度一：spurious 的 WnR 判定（决定性）

x19（=ESR）全量直方图：

```
=== x19 (ESR at __do_kernel_fault entry) value histogram ===
  127  x19=0000000096000044
    8  x19=0000000096000004

=== ESR decode (EC, WnR, FSC) ===
  127  EC=0x25 WnR=1 FSC=0x04      # 写访问，level 0 translation fault
    8  EC=0x25 WnR=0 FSC=0x04      # 读访问，level 0 translation fault
```

ESR 解码依据 ARM DDI 0487（DABT ISS）：`ISS[6]=WnR`，`ISS[5:0]=DFSC`。0x44 = 0b1000100：bit6=1（写），FSC=0x04（L0 翻译故障）；0x04：bit6=0（读）。同一编码内核自身在 fatal Oops 的 `Mem abort info` 里打印为 `ISS = 0x00000004 … WnR = 0`，互为印证。

逐案读写比例表：

| 转储 | spurious 总数 | 写（WnR=1） | 读（WnR=0） | 写侧受害进程 | 读侧受害进程 |
|---|---|---|---|---|---|
| 08-14-19:07 | 12 | 10 | 2 | pmdalinux 6, irqbalance 4 | memcpy1, control（均 `_find_next_and_bit`） |
| 08-17-13:47 | 26 | 26 | 0 | irqbalance 12, pmdalinux 14 | — |
| 08-24-18:03 | 34 | 33 | 1 | irqbalance 33 | bash（`__lruvec_stat_mod_folio`） |
| 08-25-15:42 | 1 | 1 | 0 | pmdalinux 1 | — |
| 08-26-10:37 | 9 | 9 | 0 | irqbalance 4, pmdalinux 5 | — |
| 08-31-00:47 | 13 | 13 | 0 | irqbalance 5, pmdalinux 8 | — |
| 09-03-18:25 | 35 | 35 | 0 | irqbalance 32, pmdalinux 3 | — |
| 09-04-09:15 | 2 | 0 | 2 | — | rcu_sched, ps |
| 09-04-10:27 | 2 | 0 | 2 | — | rcu_sched ×2 |
| 09-04-12:33 | 1 | 0 | 1 | — | HeapHelper |
| 08-25-15:58 | 0 | — | — | — | — |
| 09-04-11:00 | 0 | — | — | — | — |
| 合计 | 135 | 127（94.1%） | 8（5.9%） | irqbalance 90, pmdalinux 37 | rcu_sched 3, 其他 5 |

结论 1【实锤】：spurious 事件 94.1%（127/135）是写访问（WnR=1）。任务背景"既有证据都是读侧"的前提在 spurious 层面不成立：写通路不仅受扰，而且是主要受扰对象。

### 2.2 维度二：触发指令谱完备化（135 起 spurious + 12 起 fatal）

受害指令直方图（135 起 spurious）：

```
=== Victim frame histogram ===
  127  __memcpy+0x80/0x240
    6  _find_next_and_bit+0x18/0x80
    1  __lruvec_stat_mod_folio+0x20/0x98
    1  seq_put_hex_ll+0xb8/0x140
```

逐条反汇编对账（vmlinux，均与 WnR 一致）：

| 受害帧 | 指令（vmlinux 实反汇编） | 类型 | 事件数 | WnR | 一致性 |
|---|---|---|---|---|---|
| `__memcpy+0x80` | `382e6808  strb w8, [x0, x14]` | 字节 store（基址+变址寄存器寻址） | 127 | 1（写） | ✓ 127/127 |
| `_find_next_and_bit+0x18` | `f8647803  ldr x3, [x0, x4, lsl #3]` | 64 位 load（缩放变址） | 6 | 0（读） | ✓ 6/6 |
| `__lruvec_stat_mod_folio+0x20` | `f8647864  ldr x4, [x3, x4, lsl #3]` | 64 位 load（缩放变址） | 1 | 0（读） | ✓ |
| `seq_put_hex_ll+0xb8` | `386368c5  ldrb w5, [x6, x3]` | 字节 load（变址） | 1 | 0（读） | ✓ |

`__memcpy+0x80` 处的完整小拷贝路径（len≤7、非 4 对齐时进入）：

```
ffff800080e9db2c <__memcpy+0x6c>: lsr  x14, x2, #1        # x14 = len/2
ffff800080e9db30 <__memcpy+0x70>: ldrb w6, [x1]           # 源[0]     装载
ffff800080e9db34 <__memcpy+0x74>: ldurb w10, [x4, #-1]    # 源[len-1] 装载
ffff800080e9db38 <__memcpy+0x78>: ldrb w8, [x1, x14]      # 源[mid]   装载
ffff800080e9db3c <__memcpy+0x7c>: strb w6, [x0]           # 目的[0]   存储 —— 从未成为受害帧
ffff800080e9db40 <__memcpy+0x80>: strb w8, [x0, x14]      # 目的[mid] 存储 —— 127 起受害
ffff800080e9db44 <__memcpy+0x84>: sturb w10, [x5, #-1]    # 目的[len-1] 存储 —— 从未到达
```

结论 2【实锤】：127 起写侧 spurious 的受害指令是 `strb`（字节存储），且全部位于 memcpy 目的侧。既有 D1/D2/D3 描述的"12/12 致命受害全是 64-bit ldr"仍然成立（本次复核 12/12：9× `ldr x23,[x27,#288]` @ find_busiest_group+0x140、1× `ldr x1,[x3]` @ bio_add_page+0xf0、08-24 为 bio_add_page，全部 WnR=0）。但"读侧受害"只刻画了致命层，spurious 层的主受害方是 store。

微局部性签名【实锤】：在同一条 len≤7 拷贝路径上，`strb w6,[x0]`（目的[0]）先于 `strb w8,[x0,x14]`（目的[+1..3]）执行且未出错，紧随其后的第二条字节存储在同一 4KB 页内发生 L0 翻译故障。这排除了"页不存在/映射缺失"类持续性原因，指向逐访问瞬态的翻译失败（与 D3 PTW 瞬态失败同族）。

### 2.3 维度三：写通路敏感性的"负证据"量化

暴露量（12 份 dmesg 全程，21 天）：

- ext4 以 r/w 挂载（boot log：`EXT4-fs (sda2): mounted filesystem ... r/w with ordered data mode`），jbd2/writeback 持续运行；各 dmesg 均有 jbd2/writeback 相关行（10–41 行/份）。
- CPU179 上最高密度的内核态微小存储流 = /proc/interrupts 读取：irqbalance 每 10 s（dmesg 时间戳差 10.0 s 成簇佐证：09-03 案 142792→142802→142812→142822→142832 连续 10 s 节律）、pmdalinux 周期更长；192 CPU 的中断表每行 ~2.1 KB，单次读取生成数万次 1–11 B 的 `vsnprintf→__memcpy` 微拷贝。

观察结果：

| 指标 | 数值 | 出处 |
|---|---|---|
| 写侧翻译故障（spurious，自愈） | 127 起 / 21 天 | §2.1 |
| 写侧数据值腐化事件 | 0 起 | 全 12 份 dmesg 扫描：结构化行无非打印字符（0 行异常）；irqbalance 无解析失败记录；无 ext4/jbd2 校验错误 |
| 写侧致命 Oops | 0 起（12/12 fatal 全为 load） | §2.2 |
| 读侧翻译故障（spurious） | 8 起 | §2.1 |
| 读侧致命 Oops | 12 起 | 既有结论复核 |

结论 3【实锤+强推】：
- 【实锤】写通路不是免疫：127 起 WnR=1 的 store 翻译故障证明写访问的地址→MMU/PTW 通路同样受扰，且数量上是读侧 spurious（8 起）的 15.9 倍。
- 【强推】受扰形态与读侧不对称：写侧 100% 表现为瞬态翻译故障（自愈、零数据丢失；`is_spurious_el1_translation_fault` 反汇编确认其用 `at s1e1r, FAR` 重走页表成功后放行重试，存储最终完成）；而数据值层面的致命腐化（D1 相位/D2 byte7）只出现在装载返回通路。综合暴露量（21 天全速写流量）与零写数据腐化观测，写数据通路（store buffer 数据字段）为低敏或免疫；因写数据腐化天然静默（无异常、无校验），"免疫"无法由本法证单独证死，判【强推：低敏】。
- 需要注意一个统计口径：spurious 写:读 ≈ 127:8 的比例受访问谱权重调制（/proc/interrupts 读使 CPU179 上微小 store 密度远高于普通内核负载），不能直接当作微架构层面"写通路敏感度是读的 15.9 倍"的物理结论；但"写通路受扰存在"这一定性判定不受口径影响。

### 2.4 维度四：memcpy/seq_file 路径深度：为什么总是 /proc/interrupts

127 起写侧事件的完整调用链（124 起）：

```
__memcpy+0x80 <- seq_printf+0xc4 <- show_interrupts+0x1d4 <- seq_read_iter+0x168
              <- proc_reg_read_iter+0x68 <- new_sync_read+0xac <- vfs_read ...（read 系统调用）
（另 3 起为 arch_show_interrupts+0xe0 <- show_interrupts+0x3d0，同为 /proc/interrupts）
```

`vsnprintf+0x374` 处确认存在 `bl __memcpy`（fs/seq_file.c 的 seq_printf → lib/vsprintf.c 的 string() 内联拷贝 %s 实参）：

```
ffff800080edd4a0 <vsnprintf+0x360>: mov x1, x19          # src = 字符串实参
ffff800080edd4a8 <vsnprintf+0x368>: mov x0, x20          # dst = buf+pos
ffff800080edd4b0 <vsnprintf+0x370>: csel x2, x2, x21, gt # len = min(精度, 串长)
ffff800080edd4b4 <vsnprintf+0x374>: bl ffff800080e9dac0 <__memcpy>
```

因果链：irqbalance（10 s 周期）/pmdalinux 读 /proc/interrupts → show_interrupts 在 192 列 CPU 表上逐行 seq_printf → vsnprintf string() 对 IRQ 名/短数字串调用 __memcpy → 1–7 B 微拷贝进入 `strb` 尾路径 → 该字节存储瞬态翻译故障。选择效应 = 微小存储密度 × 进程驻留 CPU179 的调度惯性，不是 /proc/interrupts 内容本身的特殊性。

帧归属的诚实声明：`seq_printf+0xc4`、`show_interrupts+0x1d4` 等上层帧来自 FP 链回溯，存在 ±1 帧模糊（seq_printf 反汇编中 +0xb8 是 `bl vsnprintf`，+0x1d4 是 `bl _find_next_bit` 的返回点，均非直接 `bl __memcpy`，这是 LR 槽复用导致的近似归属）。但首帧（受害指令）来自 pt_regs->pc，精确无模糊（§1.3 已证），“受害指令是 memcpy 的 strb、上下文是 procfs seq_file 读取”这一核心结论不受影响。

x17–x13 的 ASCII 内容定性（对任务背景第 4 维度的证伪）：x12–x17 的 ASCII（"ffff6040"+"060997d6"+"tion fault at virtual address ff"）135/135 等于本次告警文本自身的格式化残留（§1.2），不是 memcpy 的源数据。seq_printf 的源数据（IRQ 名）不在寄存器残留中，软件法证到此为止。

### 2.5 维度五：LSQ/store-buffer 子单元覆盖分析（TSV110 规格 × 证据）

TSV110 公开微架构规格（本仓库 `docs/cpu/kunpeng.md`）：4-wide OoO；LSU 2×AGU；L1D 每周期 2×128-bit 访问（2 load 或 1 load+1 store）；store forwarding 6–7 周期（跨 16 B 边界 +1–2）；L1D hit load-to-use 4 周期（+1–2 周期 indexed）；ROB/调度器规模未公开 LQ/SQ 深度。

| LSQ/存储子系统子单元 | 证据覆盖 | 证据内容 | 强度 |
|---|---|---|---|
| store 地址生成 → MMU/PTW 翻译检查（SQ drain 级） | 有 | 127 起 WnR=1 L0 瞬态故障；且同页前一条 strb 成功（§2.2 微局部性） | 【实锤】受扰 |
| load 地址生成 → MMU/PTW（读侧翻译） | 有 | 8 起 WnR=0 spurious + D3（PTW 瞬态失败） | 【实锤】受扰 |
| load 数据返回通路（fill/转发） | 有 | D1（相位错位）、D2（byte7 清零）致命腐化，12/12 fatal | 【实锤】受扰 |
| store 数据字段（写入值） | 无直接 | 21 天零写值腐化观测（但静默腐化不可见，见 §5） | 【强推：低敏】 |
| store-to-load 转发（STLF）数据 | 无 | 无事件；gem5 侧 A1 矩阵为人工注入复现，非现场证据 | 【假设：无现场证据】 |
| SQ 排序/合并（fill-buffer 合并级） | 无直接 | 现场无乱序提交类故障形态 | 【无证据】 |
| AGU 算术（加法器） | 无 | 全部 FAR 与寄存器代数闭合（fatal 侧 x27=(x1+x20) 7/7 闭合），spurious 侧 FAR=x21 135/135、FAR=x24+0xa 127/127，地址算术本身每次都对 | 【实锤：AGU 算术未被命中】 |

判定逻辑：spurious 写事件的故障点是"翻译检查瞬态失败"而非"地址算错"（FAR 与被中断上下文的指针寄存器保持确定关系：127/127 满足 `FAR = x24 + 0xa`，x24 为被中断链保留的目的缓冲区游标）。结合 D3（PTW 瞬态失败）与读侧 8 起 spurious，读/写两侧共享的故障点是"访问 → 页表遍历"这一公共翻译通路，而非各自的数据通路。这正好落在 2×AGU 之后、L1D 之前的 MMU/PTW 接口，与 TSV110 "indexed 寻址 +1–2 周期" 的时序敏感区在拓扑上一致（但时序归因属微架构推测，非现场可证）。


## §3 写通路敏感性结论

| 判定项 | 结论 | 置信级 |
|---|---|---|
| 写通路是否受扰（地址/翻译层面） | 受扰：127/135 spurious 为写访问翻译瞬态故障，自愈 | 【实锤】 |
| 写通路受扰的机制归属 | 与读侧共享的公共翻译通路（PTW/MMU 接口）瞬态失败；非 AGU 算术错误（FAR 代数 100% 闭合） | 【强推】 |
| 写数据通路（store buffer 数据字段）是否产生 SDC | 低敏或免疫：21 天全速写流量下零写值腐化、零写侧致命 | 【强推】（静默腐化不可观测，见 §5） |
| 读数据通路 SDC | 受扰且致命：D1/D2/D3 + 12/12 fatal ldr | 【实锤】（既有结论，本次复核确认） |
| 读:写 spurious 比例 | 8:127（注意访问谱权重口径，见 §2.3） | 【实锤】 |

总结：该缺陷对存储通路的作用集中在地址/翻译检查级（写侧为主力受害面、全部自愈），数据值级 SDC 只在装载返回通路（读侧、致命）；写数据通路在现有 21 天观测内未表现出值级敏感性。


## §4 对"三通路是个案还是完备"的贡献

既有三通路（D1 装载数据返回相位错位 / D2 地址通路 byte7 清零 / D3 PTW 瞬态失败）均为读侧证据。本次分析贡献三个结构性事实：

1. D3 从读侧扩展为读写公共通路【实锤】：8 起读侧 spurious + 127 起写侧 spurious + fatal 侧 D3，三者共同指向"访问→页表遍历"公共接口的瞬态失败。三通路假说中 D3 一支不再是个案，而是覆盖读、写两个方向的公共机制。
2. D1/D2 保持读侧特异性【实锤】：值级腐化（相位/byte7）在写侧 21 天零观测。装载返回通路的数据管线是值级 SDC 的唯一现场来源。
3. AGU 算术被排除出候选集【实锤】：全部 147 起事件的 FAR 与寄存器代数关系 100% 闭合（spurious：FAR=x21 135/135、FAR=x24+0xa 127/127；fatal：x27=(x1+x20) 7/7、FAR=x27+0x120 6/6）。地址计算无误，故障在地址的使用/翻译环节。

因此三通路的"个案 vs 完备"问题应重新表述为：值级通路（读侧专属）+ 翻译通路（读写公共）+ 已被排除的 AGU 算术。写侧值级通路作为"无现场证据的第四候选"保留（见 §5）。


## §5 诚实边界（软件法证不能裁决什么）

1. 写数据腐化天然静默：store 数据字段被腐化不产生任何异常（无精确异常、无 ESR、无 WnR 载体），只有下游校验（文件系统校验和、应用级 CRC）才可能暴露。12 份 dmesg 无此类报错只能说明"21 天内未见"，不能证明"免疫"。§3 的【强推：低敏】上限受此约束。
2. WARNING 块寄存器是事后状态：dump 发生在 `__warn_printk` 之后，caller-saved 寄存器（x0–x18 中未被 callee 保存的部分，实证 x12–x17）已被 printk 污染；只有 callee-saved 的 x19–x28 可用于法证。任何基于 x0–x11/x12–x17 的"故障数据内容"推断（包括任务背景预设的 x17–x13 ASCII 分析）不成立，本文已证伪并给出机制。
3. 上层调用帧有 ±1 帧模糊：FP 链回溯的 seq_printf/show_interrupts 帧是近似归属；只有 pt_regs->pc 首帧精确。涉及"哪条源码路径"的细粒度结论以首帧为准。
4. WnR 只区分读写，不区分 SQ drain 级与 AGU 级时序：微架构内部的精确流水级定位（SQ drain 检查 vs L1D MPU 检查 vs PTW）超出 dmesg 可观测范围；§2.5 的时序归因是基于拓扑的推测。
5. 读写比例受访问谱权重调制：127:8 不能直接换算为微架构敏感度比例（/proc/interrupts 的微小 store 密度偏置）；仅"写侧受扰存在"的定性结论稳健。
6. 本机 debuginfo 与目标内核版本对齐的前提：反汇编基于同版本 vmlinux（6.6.0-145.3.23.154.oe2403sp3），若目标实际运行镜像存在重编译差异，指令偏移需复核（12 份 dmesg 的 Code: 行与该 vmlinux 在 fatal 侧逐字对齐，验证了这一前提）。
7. 09-04 三案（11:00 零 spurious 等）样本量小：单事件/零事件案的读写比例无统计效力。


## 附录：复核命令清单

```bash
# 事件计数
grep -c "Ignoring spurious" /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt

# WnR 直方图（x19=ESR；WARNING 块内 el1h_64_sync 后首帧）
for f in /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt; do
  awk '/Ignoring spurious/{f=1} f&&/x19:/{for(i=1;i<=NF;i++)if($i=="x19:"){print $(i+1); f=0; break}}' "$f"
done | sort | uniq -c
#     127 0000000096000044    （WnR=1 写）
#       8 0000000096000004    （WnR=0 读）

# 受害指令直方图（el1h_64_sync 后首帧；共 135）
for f in /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt; do
  grep -A1 "el1h_64_sync+" "$f" | grep -v "el1h_64_sync\|^--" | sed 's/^\[[^]]*\] *//;s/ .*//'
done | sort | uniq -c
#     127 __memcpy+0x80/0x240
#       6 _find_next_and_bit+0x18/0x80
#       1 __lruvec_stat_mod_folio+0x20/0x98
#       1 seq_put_hex_ll+0xb8/0x140

# 受害指令反汇编
objdump -d --start-address=0xffff800080e9db40 --stop-address=0xffff800080e9db44 \
  /usr/lib/debug/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux
#   -> strb w8, [x0, x14]

# fatal Oops 12 案 WnR 与 pc
for f in /home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt; do
  ln=$(grep -n "Unable to handle kernel paging" "$f" | head -1 | cut -d: -f1)
  sed -n "${ln},$((ln+55))p" "$f" | grep -E "WnR|^Code:|pc :"
done

# x12+x13 == FAR（135/135）：按寄存器行取 x12/x13，小端反转后拼接
python3 - <<'EOF'
import re, glob
def le(v): return bytes.fromhex(v.zfill(16))[::-1].decode()
ok = bad = 0
for f in glob.glob('/home/sdc/wangxu/vmcore0102/127.0.0.1-*/vmcore-dmesg.txt'):
    for blk in re.split(r'(?=Ignoring spurious)', open(f, errors='replace').read()):
        if 'Ignoring spurious' not in blk: continue
        far = re.search(r'fault at virtual address (\S+)', blk).group(1)
        x12 = re.search(r'\bx12: ([0-9a-f]+)', blk).group(1)
        x13 = re.search(r'\bx13: ([0-9a-f]+)', blk).group(1)
        if le(x12) + le(x13) == far: ok += 1
        else: bad += 1
print(ok, bad)   # -> 135 0
EOF
```

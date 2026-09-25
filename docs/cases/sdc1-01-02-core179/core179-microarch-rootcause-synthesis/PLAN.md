# 五转储交叉根因计划：Core 179 芯片故障微架构级定位

> For agentic workers: 本计划由 superpowers:writing-plans 生成，用 superpowers:executing-plans 逐阶段内联执行。
> 步骤用 checkbox (`- [ ]`) 跟踪。

Goal: 以 `/home/sdc/vmcore/` 下全部 5 个 kdump 转储为第一手证据，独立复验并裁决 Core 179 硬件故障的微架构级根因，产出专业诊断报告。

Architecture: 三层取证：(1) 全部转储的 dmesg 全量法证（事件谱、per-CPU 分布、时间线）；(2) crash 动态取证（寄存器、页表逐级走查、内存真值对照）；(3) 与 docs/cases 用户态 SDC 签名做跨域综合，对既往两个竞争假说（PRF 活性误判 vs LSU 数据返回通路）做鉴别实验并收敛到最深处。

Tech Stack: crash 8.0.4 + 内核 debuginfo（6.6.0-145.3.23.154.oe2403sp3.aarch64 已装）、objdump/addr2line、sudo（转储文件 root 属主）、Kunpeng-920/HIP08 宿主机。

## Global Constraints

- 第一手优先：一切结论以 vmcore/dmesg 中可复核的真实命令输出为准；docs/cases 仅作辅助参考与假设来源，不作为证据。
- 置信标注：每个关键结论必须标【实锤】（dump 内可复核）/【强推】（多源收敛推断）/【假设】（软件不可验证，注明验证途径）。
- 软件根因（UAF、竞态、内核 bug、配置）必须正面排除，不得默认排除。
- 若任何证据不支持硬件根因，必须如实改判，不迎合既往结论。
- vmcore-incomplete（08-17）若 crash 拒载，如实记录并以 dmesg 为准。
- 所有分析中间产物写入 `/tmp/core179-synthesis/`，最终产物写入本目录。

## 已知现场（勘察结论，先于分析）

| 转储 | 大小 | 最终 panic | 备注 |
|---|---|---|---|
| 127.0.0.1-2026-08-14-19:07:04 | 11G | Oops 96000004 @ find_busiest_group+0x140 | 前置多次 `__do_kernel_fault` |
| 127.0.0.1-2026-08-17-13:47:08 | 27G(incomplete) | 同型 @ find_busiest_group+0x140 | vmcore-incomplete |
| 127.0.0.1-2026-08-24-18:03:07 | 109G | Oops 96000004 @ bio_add_page+0xf0 | 另含 sdc_long 可执行 |
| 127.0.0.1-2026-08-25-15:42:24 | 26G | Oops 96000007 @ find_busiest_group+0x140 | 前案已深析（LSU 数据返回通路结论） |
| 127.0.0.1-2026-08-25-15:58:09 | 9.3G | Oops 96000004 @ find_busiest_group+0x140 | 未深析 |

机器：Kunpeng-920，4 socket × 48 核 = 192 CPU 在线；HIP08 平台。

## 阶段

### P0 工具与就绪校验
- [x] Step 0.1 `crash` 对最小转储（9.3G）冷启动成功：`sudo crash /usr/lib/debug/usr/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux <vmcore>`，预期出现 `crash>` 提示符，记录加载耗时。
- [x] Step 0.2 建立 `/tmp/core179-synthesis/` 工作目录与批量命令模板（`crash -i` 输入文件模式，避免交互超时）。

### P1 五转储 dmesg 全量法证
- [x] Step 1.1 提取每次 panic 的完整块（Oops 前 80 行至 Kernel panic 结束）：CPU 号、PID/comm、PC/LR/SP、ESR、FAR、完整 Call trace。落盘 `p1_panic_<ts>.txt`。
- [x] Step 1.2 全量提取所有非致命异常告警块（`__do_kernel_fault` 系列）：时间戳、CPU、comm、FAR、Call trace 首行，建立事件总表 `p1_events.csv`。统计 per-CPU 分布，验证"51+ 事件全在 CPU179"【实锤/改判】。
- [x] Step 1.3 ESR/FSC 分类统计：全部事件的 EC、DFSC（翻译错级别 L0–L3 / 权限错 / AF 错）、WnR 位分布。
- [x] Step 1.4 RAS/EDAC/BERT/HEST/GHES 负证据链扫描：grep 全部 dmesg 的 `mce|GHES|APEI|BERT|hardware error|EDAC` 计数，确认零架构化上报。
- [x] Step 1.5 时间线重建：各次开机的 uptime→墙钟映射，确认 5 次崩溃是否同一开机内复发（15:42→15:58 相隔 16 分钟）。

### P2 crash 动态取证（每完整转储）
- [x] Step 2.1 批量命令：`sys`、`panic`、`bt`、`set`、`help -r`（崩溃任务全寄存器）。核对与 dmesg 一致性。
- [x] Step 2.2 崩溃任务上下文：`ps -p <pid>`、崩溃 rq/per-cpu 状态；`kmem -v` 判定 FAR 所属区段（vmalloc/module/线性映射）。
- [x] Step 2.3 页表逐级走查：对每个 FAR 用 `vtop -c <pid>`（或手工 `rd` pgd/pud/pmd/pte），记录各级描述符实际值。判定：走查死在哪一级、post-mortem 内存中的该级条目当前值是多少。
- [x] Step 2.4 决定性对照（每转储）：从 help -r 寄存器中复原出错 load 的源指针寄存器值 vs 该地址在 dump 中的真实内容 vs 出错后寄存器收到的坏值。三值分类：
      (a) 内存有效、寄存器坏 → 在途/瞬态损坏；
      (b) 内存本身坏 → 持久损坏；
      (c) 无法复原 → 如实标注。
- [x] Step 2.5 坏值溯源：对寄存器收到的坏值做 `search -t` 物理内存搜索，判定它是"某处真实存在的旧数据"还是"无源随机比特"。

### P3 崩溃点静态解剖
- [x] Step 3.1 objdump 反汇编 `find_busiest_group+0x140` 前后 32 条指令，确定出错 load 的助记符、基址寄存器、偏移、目的寄存器。
- [x] Step 3.2 addr2line 映射 fair.c 精确行号，写出该指针的数据流出处（哪个结构体字段、谁填充）。
- [x] Step 3.3 同法解剖 `bio_add_page+0xf0`，判定它与 find_busiest_group 案例是否同机制（同为"load 返回坏指针"）。

### P4 软件根因正面排除
- [x] Step 4.1 FAR 地址归属：vmalloc 区间对象归属（vm_struct 名）、module 区间符号归属、线性映射物理页 page 结构与引用计数。
- [x] Step 4.2 sched domain 生命周期：dmesg 中 hotplug/CPU 隔离/isolcpus 事件检索；崩溃时 sd 内存是否处于 rebuild 窗口。
- [x] Step 4.3 该内核版本（6.6.0-145.3.23.154 oe2403sp3）已知 bug 检索：find_busiest_group/bio_add_page 相关 CVE 与 stable 修复。
- [x] Step 4.4 综合排除矩阵：若软件解释成立需同时满足哪些条件，逐一检验；任一不满足即排除。

### P5 微架构级裁决（核心增量）
- [x] Step 5.1 汇总五转储的坏值样本（内核侧），与 docs/cases 用户态三案例签名（历史值回放/尾数漂移/非法寄存器）比对：坏值的比特结构是"旧数据回放"还是"计算污染"。
- [x] Step 5.2 PTW 案例深挖：翻译错案例中 post-mortem PTE 真值 vs 硬件当时读到的 0，判定走页器读出损坏是否也在故障族内（区分 PRF 类假说不可解释 PTW 案例）。
- [x] Step 5.3 单元鉴别矩阵更新：用内核侧新证据给 U1(L1D 阵列选路)/U4(fill-buffer/LQ 陈旧回放)/流程A(PRF 活性误判) 计票，明确支持/反对证据各是什么。
- [x] Step 5.4 收敛推理链重写：C1–C11 约束表基础上加入内核侧 K 系列约束，输出最终微架构根因陈述（单元、故障类型、触发条件、为何静默、为何单核）。

### P6 报告交付
- [x] Step 6.1 撰写 `DIAGNOSIS_REPORT.md`（本目录）：执行摘要、证据链、排除过程、根因陈述、置信度、处置建议、供应商质询清单。
- [x] Step 6.2 自审：报告中每个【实锤】均可由文中命令复现；无占位符；事实/推论分离。
- [x] Step 6.3 按 CLAUDE.md 补丁纪律：feature 分支提交并推送（计划与报告各自独立补丁）。

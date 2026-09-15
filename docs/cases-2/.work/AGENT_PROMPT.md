# Subagent 诊断任务：独立微架构级 SDC 根因研究（单 vmcore 案例）

你是一名内核故障法证工程师，对一台 192 核 Kunpeng-920 (TaiShan-v110) openEuler 服务器的一次 kdump 转储做**独立的、从零开始的**微架构级 SDC（Silent Data Corruption）根因诊断。你的最终输出是一份中文深度诊断报告 + 取证附件。

## 0. 铁律（违反即任务失败）

1. **禁止阅读任何既有诊断文档**：不要读 `docs/cases/`、`docs/hypothesis/`、`docs/cpu/`、`.planning/`、`docs/superpowers/`、dump 目录内任何 `*.md`/报告文件（如 DIAGNOSIS_REPORT.md）。只允许读：vmcore(-incomplete)、vmcore-dmesg.txt、以及你目标案例目录内的原始数据文件（如 sdc_long）。也禁止读 cases-2 内其他案例的报告（保持独立性）。
2. **100% 实证**：报告中每一处数据引用都必须来自你实际运行的命令的真实输出。禁止预测、禁止"应该是"。所有取证命令与关键输出全文存入 `dmesg_forensics.txt` 附件。
3. **禁止手算 64 位地址运算**：所有加/减/模运算用 Python3 脚本（模 2^64）计算，脚本存 `algebra.py`，输出存 `algebra_out.txt`。
4. **诚实**：crash 加载失败就如实写失败；证据不足就标注置信级别，不编造。区分【实锤】（可直接复核）/【强推】（多源收敛但缺直接对照）/【假设】（无法软件验证，给出验证途径）。
5. 大 vmcore 加载慢（可达数分钟），crash 命令一律用 `-i <cmdfile>` 批处理 + timeout 600 以上，一次会话尽量多取证，避免反复加载。

## 1. 环境（已由主会话实证，直接使用）

- 主机：aarch64 Linux，crash 8.0.4-17.oe2403sp4 在 /usr/bin/crash，gdb 10.2（crash 内置）。
- **正确 namelist：`/tmp/vmlinux-0102`**（ELF aarch64，BuildID 276194e5f356f9c4bc570bb0a750394d7b768035，_text=ffff800080000000，带 debug_info）。与所有 dump 匹配（已实测 case 08-14 可加载、bt 正常）。**不要用** `/home/sdc/wangxu/vmcore0102/gem5-fi/gem5-fs/vmlinux`（BuildID 不同，不匹配）。
- 内核：6.6.0-145.3.23.154.oe2403sp3.aarch64 #1 SMP，192 CPU，768GB RAM，8 NUMA 节点。
- dump 格式：Kdump compressed dump v6。
- crash 加载模板（完整 vmcore）：
  ```
  cat > /tmp/case.cmd <<'EOF'
  sys
  panic
  bt
  quit
  EOF
  timeout 590 crash /tmp/vmlinux-0102 <VMCORE_PATH> -i /tmp/case.cmd 2>&1 | tee /tmp/case_out.txt
  ```
  先跑这个最小集确认可加载，再扩展命令文件做全量取证（一次重新加载跑全量，节省时间）。vmcore-incomplete 可能加载失败或大量 seek error——如实记录，然后只用 dmesg 法证。

## 2. 你的案例

- 目标转储目录：`__DUMPDIR__`
- vmcore 文件：`__COREFILE__`
- dmesg 文件：`__DMESGFILE__`

## 3. 诊断方法论（systematic debugging）

### 阶段 A · dmesg 全量法证（必做，即使 crash 可用）
1. 提取开机指纹：内核版本行、command line、CPU 型号/拓扑、内存容量、crashkernel。
2. 提取崩溃主块：panic 原文、ESR/EC/FSC 解码、pgtable 走查行、CPU/PID/Comm、pc/lr、全量寄存器 x0–x30、Call trace 全帧、Code 窗口。
3. 提取所有前兆异常：grep -n -E 'WARNING|BUG|Oops|cut here|spurious|Machine check|Hardware error|EDAC|ECC|ras|memory failure|segfault' vmcore-dmesg.txt，统计次数、CPU、PID、时间戳；与致命崩溃的时间间隔（Python3 计算）。
4. RAS 负证据：明确 grep 硬件错误关键词并记录"无"（如有则分析）。
5. 时间线：uptime 0 → 各事件 → panic；墙上时间由目录名反推（Python3）。
6. 识别受害业务：Comm 名对应的进程/容器/业务，调度上下文（用户态入口 syscall、kworker、内核线程）。

### 阶段 B · crash 内存法证（完整 vmcore 必做）
扩展命令文件至少包含（一个会话跑完，注意输出量大时分文件）：
```
sys
panic
bt
bt -t
set
mach
kmem -s        # 可能耗时，超时则跳过并记录
ps | head -50
runq
timer
dev -p
net
foreach bt -h  # 或对关键 task 单独 bt
```
然后做**寄存器闭合验证**：
- 用 `bt`/`bt -r` 或 `rd`、`p`、`sym` 把崩溃点寄存器值与数据结构真值对照。重点：出错的指针/值是从哪条装载指令来的（Code 窗口反汇编：`dis -l <pc>` 或 objdump/gdb disassemble），该指令的目标寄存器是否就是 Oops 报告里的坏值寄存器。
- `vtop <FAR地址>`：验证页表走查是否与 dmesg 的 pgd/pud/pmd/pte 输出一致。
- `p <全局变量>`：如果崩溃涉及 percpu 变量（如 runqueues），用 `p __per_cpu_offset[cpu]`、`px runqueues`、percpu 实例地址计算（Python3）验证"寄存器里的值"vs"内存真值"是否一致——**这是区分'装载被腐化'与'内存本身被写坏'的关键**。
- `struct <type> <addr>`：读受害数据结构的字段真值。
- `search -t <pid>` 或 `search` 崩溃线程栈上的残留值。
- 反事实推演：如果寄存器值正确（=内存真值），该指令是否仍会崩溃？FAR 地址是否本应有效（vmalloc/线性映射区间判断，用 `vtop`/`kmem` 验证）？

### 阶段 C · 微架构级根因综合
1. 用 `/tmp/vmlinux-0102` 的 debug_info 精确定位致命指令（`dis -l`、`gdb` / `objdump -d --start-address`），写出该指令的架构语义（如 `ldr xN, [xM, #imm]`），确定坏值是"装载结果"还是"计算结果"。
2. 构造闭合等式：坏寄存器 == 某基址 + 偏移？== 0（零塌缩）？== 某内存真值（则内存被写坏）？用 Python3 逐位验证。
3. 若同一开机内有前兆事件：对比前兆与致命事件的 CPU/路径/对象，判断是否同一硬件资源（同一核、同一 cache 行、同一数据通路）先后受扰。
4. 排除软件成因：该指令在正常内核里被执行亿万次是否安全（如调度器热路径）；为什么只有这个 CPU/这个时刻出错——软件 bug 不会挑核。
5. 微架构定位推理：出错环节在流水线的哪一段（PTW/页表走查？装载通路 load path？ALU 计算通路？寄存器文件？cache/TLB？）。证据形态：
   - 装载结果塌缩为 0 而内存真值非 0 → load path / 数据通路受扰【强推】
   - FSC=L3 pte=0 而页表真值有映射 → PTW 读出被腐化【强推】
   - 指针算术结果错 → ALU 通路
   - 多核受害/共享结构被写坏 → 缓存一致性/共享内存通路
6. 芯片设计与实现启示（必写一节）：基于本案证据形态，提出对芯片设计/验证/RAS 的具体启示（如：load path ECC/parity 覆盖、关键调度数据结构冗余校验、PTW 输出校验、错误传播屏障、failfast vs silence 的权衡、DFT/在线测试钩子、kdump 可靠性设计等）。

### 阶段 D · 报告撰写
目录：`/home/sdc/wangxu/vmcore0102/gem5-fi/docs/cases-2/vmcore-diagnosis-report-__SANITIZED__/`
（__SANITIZED__ = 目标目录名去掉冒号，如 127.0.0.1-2026-08-14-190704）

文件：
- `vmcore-diagnosis-report-__SANITIZED__.md` 主报告（格式见下）
- `dmesg_forensics.txt` 全部取证命令与输出
- `algebra.py` + `algebra_out.txt` 代数复算
- crash 全量输出（可并入 dmesg_forensics.txt 或单独 crash_forensics.txt）

主报告结构（中文，面向人类读者，消除 AI 味，深入浅出、推理严密）：
```
# <CPU号或受害对象> 转储深度诊断报告（第 N 次独立重研究）
## ——<一句话副题：本案最独特的法证特征>

| 信息表：目标转储/主机/CPU/内核/崩溃时间/uptime/受害进程/前兆/结论一行 |

## 1. 执行摘要（编号列表，6 条左右：现象、签名、闭合验证结果、置信、处置）
## 2. 证据规则与方法（证据源、诚实铁律、三级置信、工具清单）
## 3. 本次开机时间线【时间线】（表格：uptime/墙钟/事件/dmesg行号/置信）
## 4. 故障现象【故障现象】
### 4.1 Oops 原文
### 4.2 全量寄存器（x0–x30）
### 4.3 Call trace（完整）
### 4.4 前兆异常原文（如有）
### 4.5 RAS 负证据（dmesg grep）
## 5. 业务现象（受害进程是什么业务、调度上下文解读）
## 6. 诊断定位过程【诊断定位过程】（P1 dmesg 勘察 → P2 崩溃块提取 → P3 crash 加载 → P4 内存真值对照 → P5 软件成因排除 → P6 定位收敛）
## 7. 逻辑链条（指令语义、闭合等式【实锤】、反事实推演、诚实声明）
## 8. 故障根因（微架构级结论 + 置信级别）
## 9. 启示（9.1 微架构定位的意义；9.2 芯片设计与实现启示——具体、可落地；9.3 对系统软件/RAS 的启示）
## 10. 处置建议
## 附录：命令索引（全部取证命令，可复核）
```

## 4. 完成标准

- 上述文件全部落盘，报告含真实输出引文（带 dmesg 行号或 crash 提示符上下文）。
- 完成后返回一段简短总结：本案崩溃签名（一句话）、微架构根因判定（一句话+置信）、报告路径。

# Core 179 硬件故障微架构级根因诊断报告（五转储交叉取证）

案件: `/home/sdc/vmcore/` 全部 5 个 kdump 转储交叉根因定位
机器: Yangtze Computing R240K V2 / BC82AMQA，BIOS 7.48；HiSilicon HIP08 (Kunpeng-920)，4×48=192 核，768 GB（8 NUMA 节点稀疏物理布局）
内核: 6.6.0-145.3.23.154.oe2403sp3.aarch64（debuginfo 就位，全符号分析）
日期: 2026-08-25　方法: systematic-debugging 四阶段 + 计划驱动执行（PLAN.md 同目录）
置信约定: 【实锤】= dump/活系统内可由文中命令复现的证据；【强推】= 多源独立证据收敛的推断；【假设】= 软件不可验证，注明验证途径。

## 1. 执行摘要

对五次开机的五份转储做第一手法证后，本报告将 Core 179 故障的微架构根因收敛为：

> Core 179 的装载-数据返回通路（load data-return path：fill-buffer/replay 合并 ≈ L1D 读出组装级）存在单核私有的时序边界缺陷。在特定发射相位与电压裕量组合下，它向指令消费者交付错误的数据字：内容来自"近期访问过的行"（含数组头部历史行）、按 ±8 比特整数倍的字节相位错位组装、偶发全零槽位态；同一故障族同样波及硬件页表走查器（PTW）的读出数据通路。该失效粒度低于整机全部架构化 RAS 检测器（APEI/GHES 固件优先模式全程零记录）。

决定性新证据（本报告首次获得，均为第一手）：内核在崩溃瞬间从 `__per_cpu_offset[146]` 读入的值逐位精确等于同数组第 0 号元素右移一字节（`0x00ffffcc879da2e0 = __per_cpu_offset[0] >> 8`），而内存真值完好无损。这是"陈旧行回放 × 字节相位错位"的直接现场抓获，与用户态 SDC 案例（docs/cases/sdc1-01-02）的签名完全同族。*（旋转方向退化披露：对 64 位字，右移 1 字节 `ror1` 与左移 7 字节 `rol7` 产生相同值（1+7=8=整字），故 15:58 签名在 (ror1, rol7) 歧义下识别。承重的是 Hamming-0 旋转本身（位翻转不可达），非右旋方向。见 MICROARCH_SUPPLEMENT §2.2 / paper §3.2。）*

## 2. 五转储统一性【实锤】

| # | 开机时刻 | 崩溃 uptime | CPU | 任务/路径 | PC | ESR/FSC | FAR |
|---|---|---|---|---|---|---|---|
| 1 | 08-14 19:07 | 31.7 h | 179 | kworker/179:1H, kblockd | find_busiest_group+0x140 | TF-L0 读 | 0036bc836a4a97df（非规范） |
| 2 | 08-17 13:47 | 66.5 h | 179 | swapper/179, nohz 均衡 | 同上 | TF-L0 读 | 00ffd780f5a3a7c0（非规范） |
| 3 | 08-24 18:03 | 146.5 h | 179 | kworker/u391:3, writeback | bio_add_page+0xf0 | TF-L0 读 | 003c521da2e9b99f（非规范） |
| 4 | 08-25 15:42 | 21.3 h | 179 | claude, epoll→newidle_balance | find_busiest_group+0x140 | TF-L3 读 | ffffa5aa9b5a97e0 |
| 5 | 08-25 15:58 | 6分58秒 | 179 | kworker/179:1H, kblockd | find_busiest_group+0x140 | TF-L0 读 | 00ffb34569fc3ac0（非规范） |

- 五开机合计 78 次内核异常事件（73 次 spurious 翻译错告警 + 5 次致命 Oops），100% 单点于 CPU179，其余 191 核零事件。
  复现命令：`grep -h 'WARNING: CPU:' dmesg_*.txt | grep -o 'CPU: [0-9]*' | sort | uniq -c` → `73 CPU: 179`；致命 Oops 同法 → `5 CPU: 179`。
- 4/5 次崩在同一条指令 `f9409377`（`ldr x23,[x27,#0x120]`），且 FAR−x27 恒等于 0x120；第 3 次崩在另一子系统的同类读指针路径。
- 当前开机（截至取证时 6 小时）cpu179 在线、零复发，缺陷为相位/负载依赖的间歇性。

## 3. 证据分类学

### 3.1 Class A：瞬态页表走查失败（73 例，非致命）【实锤】

openEuler 内核的 `is_spurious_el1_translation_fault()`（fault.c:301，主线逻辑）对翻译错当场重跑 `AT S1E1R`：若第二次走查成功则判 spurious 放行并 WARN。73 例告警中：

- 72 例 FAR 为有效线性映射地址（按 boot 日志 SRAT 窗口精确判定 PA ∈ 节点内存范围），即"同一页表、相隔微秒、两次硬件走查结果相反"。这些映射是开机即建立、数日未变的 slab/静态数据，不满足主线补丁针对的"并发新建映射竞态"前提，故不能归因于软件时序。
- 1 例（ffffc360a9e44e08，bash）位于 vmalloc 区，未逐一裁决，不影响统计结论。
- ESR 形状：70 例 `0x96000044` / 3 例 `0x96000004`（EC=0x25 Data Abort 当前 EL，DFSC=0x04=TF-L0）。差异位 ISS[6] 经 ARMv8-A ARM（DDI 0487）解析为 ISS[7:6]=S1PTW（stage-1 页表走查标志）的低位，非先前所标的"Overlay"。70/73 S1PTW=1（错误在走查中取，对 PTW 读出缺陷 D3 是预期）；3/73 S1PTW=0（错误在无走查时取，机制上非 PTW 读出失败，最可能归因主线 commit 42f91093b043 所针对的合法同核 TLB-invalidation 时序竞争）。故 D3 干净承重群体为 70/73，3/73 为可能合法竞争掺混不予抹平。见 §9 局限。

### 3.2 Class B：致命 Oops 的寄存器-内存对照（本报告核心增量）

反汇编锁定出错数据流（vmlinux + addr2line，fair.c:12050-12051 `update_sg_lb_stats`）：

```
ldr  x20, [x0, w25, sxtw #3]   ; x20 = __per_cpu_offset[i]
add  x27, x1, x20              ; x27 = &runqueues(运行时) + offset[i] = cpu_rq(i)
ldr  x23, [x27, #288]          ; ← 出错指令；[rq+0x120]=cpu_load(rq) 的 cfs 平均负载
```

恒等式校验（python 精确算术）：四例 find_busiest_group 崩溃均满足 `x27 == x1 + x20`，证明寄存器保存与执行一致，排除"保存窗口二次损坏"。

寄存器收到的坏值 vs dump 内存真值（write-once 静态数组，启动后永不改变）：

| 案例 | 装载目标 | 内存真值 | 寄存器收到 | 判定 |
|---|---|---|---|---|
| 4 (15:42) | offset[176] | `ffffda55e61ce000` | `0000000000000000` | 全零交付（用户态 C3 同型） |
| 5 (15:58) | offset[146] | `ffffcc879ed92000` | `00ffffcc879da2e0` | = `offset[0]`(=`ffffcc879da2e000`) 右移 1 字节，逐位精确（= `ror1` ≡ `rol7`，方向退化，见 §3.2 注）【实锤】 |
| 1 (08-14) | offset[176] | `ffffd937172de000` | `d93715ba0000ffff` | 与 `offset[0]`(=`ffffd93715b7e000`) 的 ror6（=循环左移 16 bit）最近匹配，Hamming 距离 6（跨 2 字节：byte3 `e0`→`00` 差 3 bit、byte4 `b7`→`ba` 差 3 bit）【强推，非实锤】 |
| 2 (08-17) | offset[175] | （vmcore-incomplete 无法载入，如实记录） | `00ffffa827b20fe0` | "顶字节 00+整体右移 8 位"形状与案例 5 同族【强推】 |
| 3 (08-24) | bi_io_vec[70].bv_page | `fffffd012d055b80`（健康 vmemmap 页指针） | `553c521da2e9b99f` | 指针完全离形；且 pt_regs 中 x3 顶字节=0x55 而 FAR 顶字节=0x00 |

三个关键旁证：
- 地址路径相位异常：案例 1 中 `x27+0x120 = d936bc83_6a4a97df` 但硬件记录 FAR=`0036bc83_6a4a97df`（最高字节 d9→00）；案例 3 中 x3 与 FAR 同样仅最高字节不同（55 vs 00）。MMU 实际接收的访存地址与寄存器堆读出值不一致，损坏发生在 RF→AGU/LSU 或 LSU→FAR 的传递级。
- 坏值溯源命中"数组头部"：案例 5 的坏值是数组第 0 元素内容的相位错位副本，若是随机比特，恰为另一数组元素移位形状的概率 ~2⁻⁶⁰；这与用户态 Case-1/C2"错源=近期访问过的行（数组头部历史行）+ 字节级拼接异常"签名逐条吻合。
- 下游表达多样性由坏值决定：坏值非规范 → L0 翻译错（案例 1/2/3/5）；坏值恰为 0 → `cpu_rq(i)` 退化为 per-cpu 模板地址（`sym` 确认 FAR=`runqueues+288`）→ L3 翻译错（案例 4）。五个 panic 是同一缺陷在不同数据下的不同投影。

### 3.3 RAS 负证据链【实锤】

五份 dmesg 中 APEI/GHES/BERT 仅出现启动注册行（"GHES: APEI firmware first mode is enabled"、ghes_edac 初始化），零条硬件错误记录；缺陷粒度低于所有已挂接检测器或未覆盖相应数据通路。

## 4. 软件根因正面排除

| 假设 | 排除依据 | 置信 |
|---|---|---|
| UAF/野指针 | `__per_cpu_offset` 是内核镜像 .data 段静态数组，boot 时写一次、无释放路径；dump 内存真值健康且自洽 | 实锤 |
| 并发写竞争 | 单写者（早期 boot）；崩溃发生在 uptime 6 分~146 小时的任意时刻，无对应内核活动 | 实锤 |
| 内核 bug（调度/bio 路径） | 出错指令序列经反汇编-addr2line 锁定为直白的 per-cpu 读；算术恒等式成立说明 CPU 正确执行了正确指令、错在数据；三互不相关子系统同病 | 强推 |
| 该版本已知缺陷 | 检索 openEuler/mainline 无匹配记录；主线 spurious-fault 补丁针对"并发新建映射"，与本案数日老映射不符 | 强推 |
| 配置/微码残留 | 五次开机跨 KASLR 会话复现；微码 revision 全核一致为 0（前案 method1 记录） | 强推 |

## 5. 微架构单元裁决（更新前案 U1/U4/流程-A 计票）

| 假设 | 新证据支持 | 新证据反对 | 结论 |
|---|---|---|---|
| 流程-A：PRF 活性误判（sdc1 报告 §11.2a） | — | ① PTW 类事件完全不经过寄存器重命名；② 内核侧坏值是真实内存内容的相位错位副本而非"另一变量之值"，重命名混淆无法产生字节移位结构；③ 地址路径 FAR≠RF 无法用架构寄存器活性解释 | 降级排除 |
| U4：fill-buffer/LQ 陈旧项回放 + 合并选路错位（vmcore 报告 II-5） | ① 坏值=数组头历史行内容 ✓；② 字节相位 k·8bit 错位=合并 mux 选路错 ✓；③ 全零=空/无效槽位态 ✓；④ PTW 读出同族受累（共享 SRAM 读出返回结构）✓ | 最终裁决仍需供应商 RTL | 采纳为最优模型 |
| U1：L1D 阵列 way/列选通错 | 可解释部分相位错位 | 无法解释"恰好命中近期访问行"偏好（应送达任意同组现役行） | 弱化保留 |

最深处根因陈述（收敛推理链）：

```
Class B 寄存器-内存对照:  损坏发生在"装载数据返回组装级"，不在计算/地址生成/存储   【实锤】
案例5 精确溯源 slot[0]>>8: 返回内容=其他位置真实数据的字节相位副本（陈旧行回放）  【实锤】
案例1/3 FAR-vs-RF 顶字节: 相位错位同时存在于 RF→LSU/PTW 的地址传递               【实锤】
Class A 73 例:            同族弱点波及硬件走页器阵列读取                          【实锤】
C10+C11(继承):            单核私有、低于一切架构化检测粒度                        【实锤】
⇒ 缺陷单元：Core179 私有的 load data-return 通路（fill-buffer/replay 合并 mux ≈ L1D
  读出组装级，及同族 PTW 读出返回），物理本质为特定发射相位×低压组合下的
  small-delay-fault 类建立/保持违例；交付谱 = {陈旧行内容, ±k·8bit 相位错位拼接, 全零}。
```

与用户态三案例的统一：Case-1（读只读数组得到历史值/字节拼接）= 本征现象；Case-2（Cholesky 尾数漂移）= 坏操作数进入 FMA 链的放大；Case-3（STL 压测寄存器非法值）= 高压下触发率上升的本征现象；五次内核 panic = 本征坏值被用作指针的必然结果；一个根因，七类投影。

## 6. 处置建议（继承并强化前案）

1. 立即永久 offline cpu179（78 事件全在该核；隔离后其余 191 核跨全部实验零异常）。当前开机 cpu179 仍在线，应尽快执行。
2. 该 socket RMA，附本报告 + sdc1-01-02 证据包 + 五转储。
3. 监控指标：`Ignoring spurious kernel translation fault` 出现频率（最灵敏前兆，本次五开机共 73 次，全部先于/伴随致命崩溃）。
4. 供应商质询清单增补（内核侧新向量）：将"`ldr` 从 `__per_cpu_offset` 数组中部读到头部元素旋转一字节（ror1≡rol7 歧义）"作为 DFT 定位向量，指向 fill-buffer 合并/列选通的相位控制逻辑（覆盖 1-与-7 字节位移的相位控制）；请求 scan-at-speed/LBIST 覆盖该级。

## 7. 关键命令可复现索引

- panic 块提取：`sed -n '/Internal error 行号-80,$p' vmcore-dmesg.txt`
- per-CPU 统计：`grep -h 'WARNING: CPU:' dmesg_*.txt | grep -o 'CPU: [0-9]*' | sort | uniq -c`
- 反汇编：`objdump -d --start-address=<find_busiest_group+0xc0> ... vmlinux`；源码映射 fair.c:12050
- 寄存器真值实验（以 15:58 为例）：
  `crash vmlinux vmcore -i <(echo 'p __per_cpu_offset[146]; rd -64 0xffffb378e29e55d0 192')`
  → 内存 `slot[146]=ffffcc879ed92000`、`slot[0]=ffffcc879da2e000`；对照 dmesg 打印寄存器 `x20=00ffffcc879da2e0 = slot[0]>>8`
- 恒等式：python 校验 `x27==x1+x20`（四例全过）
- bio 案例：`struct bio ffff60401b366738` → bi_vcnt=71, bi_io_vec=ffff60401dabd000；`rd -64 0xffff60401dabd460 1` → `bv_page=fffffd012d055b80`

## 8. 分析产物

`/tmp/core179-synthesis/`（会话临时区）：dmesg_*.txt 副本、p1_panic_*.txt、p1_events.csv（73 事件明细）、p2_1542.log、arr1558.txt/arr0814.txt（per-cpu 数组全文）等。正式证据以本报告 §7 命令在原转储上可复现为准。

## 9. 诚实边界与局限

1. 转储采集本身经由 CPU179（kdump 在 panic CPU 上执行拷贝）：理论上转储字节可被同一缺陷污染。缓解：核心结论依赖的是"寄存器值（panic 现场打印，早于采集）vs 内存真值（多转储间自洽）"的对照，且用户态 SDC 案例不经过 kdump 即可复现。
2. 08-17 转储不完整（vmcore-incomplete），crash 拒载，仅 dmesg 参与。
3. crash 8.0.4 的 `search` 命令在本机转储上失效（定点验证不命中已知存在的值），坏值全内存溯源改用"与同数组 192 槽位逐一比对"完成。
4. 两例中内核镜像 per-cpu 模板窗口的页表页呈"表描述符+整页零"状态（跨两开机一致）；其成因（正常块映射下的惰性分配 vs 采集污染）未裁决；该窗口运行期无访问者，不影响根因链：案例 4 的故障地址本身就是坏值的下游结果。
5. spurious 事件 ESR 的 ISS[6] 经 ARMv8-A ARM（DDI 0487）解析为 S1PTW（stage-1 页表走查标志）的低位，非"Overlay"。70/73 S1PTW=1（走查中取错，对 D3 预期）= 干净 D3 承重群体；3/73 S1PTW=0（无走查时取错，机制上非 PTW 读出失败，最可能为合法同核 TLB-invalidation 时序竞争，即主线 commit 42f91093b043 所针对那类）= 可能合法竞争掺混。先前"bit6=Overlay 应 RES0"的读法是对 ISS 布局的误读，已更正。
6. 物理层最终确认（RTL/DFT/电压裕量复测）需供应商介入，属【假设】层：−30mV 可控复现协议见 docs/reproduce-method*。

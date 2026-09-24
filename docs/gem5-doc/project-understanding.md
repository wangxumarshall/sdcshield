# gem5-fi 项目全景：CPU 微架构级 SDC 故障注入研究

> 本文是对本仓库（gem5-fi）整体理解的技术文档，重点覆盖 CPU 微架构层面的故障注入体系。
> 编制日期：2026-09-09。全部内容基于仓库实际文件与 git 提交史（含 `origin/fi`、`origin/fix/fi-tool-correctness` 分支）核实，关键数字均注明出处。

## 0. 一句话概括

本仓库是一项**从生产级故障取证走向微架构级故障注入**的研究工程：一台鲲鹏 920 服务器（192 核 TaiShan-v110）的 CPU179 核在 22 天里引发了 19 次致命 panic（17 案口径累计 160 余次内核异常事件，案 18 再添 38 次前兆告警，案 19 零前兆单发即死），全部归因于该核 LSU 数据返回通路的静默数据损坏（SDC）；项目先用 crash 取证把故障钉死到微架构通路，再在 gem5 v25.1.0.1 + CHAOS 框架上构建了一套覆盖 15+ 微架构单元、可 campaign 化跑批的故障注入基础设施，用仿真定量回答"哪个单元、什么故障形态、多大几率产生 SDC"，最终产出 DFT 测试向量、保护投资排序与位谱指纹三类可交付结论。

## 1. 研究主线：从现场到仿真

### 1.1 现场层（vmcore 取证）

数据底座是 `/home/sdc/wangxu/vmcore0102/` 下的 19 个 kdump 转储（2026-08-14 至 09-07），每个案子一份七要素诊断报告（`docs/cases/vmcore-diagnosis-report-*/`，现 19 案齐备），横向综合见 `docs/cases/CPU179_TWELVE_BOOT_CENSUS.md`（12 案口径）与 `docs/cases/core179-microarch-rootcause-synthesis/paper2026_zh.md`（17 案口径）。

核心取证方法（每案报告复用同一协议）：

1. **寄存器代数闭合**：崩溃指令 `find_busiest_group+0x140` 的寄存器转储满足 `x27 = x1 + x20`、`FAR = x27 + 0x120`（Python 模 2⁶⁴ 复算），x20 是唯一自由变量——它来自 `ldr x20,[x0,w25,sxtw#3]` 对 `__per_cpu_offset[i]` 的装载。
2. **内存真值对照**：crash 直读被装载地址，真值完好非零而寄存器实收坏值——"存储的对、送达的错"，读出路径 SDC 直接实锤。
3. **四族坏值形态学**（17 案口径）：零塌缩（非零指针装载为 0）、撕裂移位（实收 = 被读数组错相位 N 字节处的真实字节流，+1/+2/+3/+5 字节相位均被唯一命中验证）、宽带位翻转（汉明距离 31–35）、同类指针替换（含写路径持久化实锤）。四族穷举证伪单比特翻转模型——撕裂值与真值是零信息损失的循环移位，任何位级 ECC/奇偶校验放行。

### 1.2 三通路微架构分解（D1/D2/D3）

`MICROARCH_SUPPLEMENT.md` 把现场签名分解为三条共址通路（后经 `FULL_UNIT_SDC_SENSITIVITY.md` 用 147 起事件做全单元完备性裁决）：

| 通路 | 签名 | 现场证据 |
|---|---|---|
| **D1 数据通路** | load 返回陈旧行 + 字节相位错位 / 全零 | 撕裂族 Hamming-0 旋转匹配（15:58 案 ror1）、零塌缩族 |
| **D2 地址通路** | MMU 输入地址 byte7 强制清零 | 0814/0824 两例 FAR MSB=0 而架构寄存器 MSB≠0 |
| **D3 PTW 通路** | 页表走查器读出瞬态失败 | 73 例 spurious translation fault（`AT S1E1R` 重放即成功），100% CPU179 |

### 1.3 假设体系（预登记）

`fi_research/EXPERIMENT_DESIGN.md`（412 行）是仿真的科学设计总纲：命题"ARM64 服务核 ISA 选型驱动大乱序窗口 → SDC 暴露面集中在乱序后端"，可证伪假设 H0–H4（后端集中性、窗口尺度敏感性、状态泄漏签名、路径依赖位谱），每条都写明证伪条件。`FI_DESIGN_SUPPLEMENT.md` 增补 H5–H7（结构化故障注入复现 D1 签名、D1/D2 谱可分性、PTW ECC 对照）。

## 2. 工具层：CHAOS 框架的扩展体系

### 2.1 基础与版本

基座是 gem5 v25.1.0.1（`CHAOS/gem5/`，基线 commit 62c7bf28，见 `CHAOS/gem5_base_version.md`）+ 上游 CHAOS 故障注入框架（Brazil 侧开源，CHAOSReg/Cache/Mem/PhysReg 四模块）。上游 CHAOS 只有位级故障（bit_flip / stuck_at_zero / stuck_at_one）；本项目的核心增量是**结构化故障注入**——能表达"整字错路由"而不仅是位翻转。

### 2.2 自研注入器清单（挂载点均为真实源码钩子）

| 注入器 | 目标微架构单元 | hook 位置 | 故障模式 | 对应现场假设 |
|---|---|---|---|---|
| **CHAOSPhysReg**（扩展） | O3 物理寄存器堆 + 重命名表 + free list | `cpu/o3/regfile.hh`、`free_list.hh`、`cpu.hh` | 位级 + read-trace 闭环（`reads_before_overwrite`） | H3 状态泄漏 |
| **CHAOSLSQFwd**（扩展） | store→load 转发数据 | `cpu/o3/lsq_unit.cc:1493`（转发 memcpy 后） | byte_flip + **结构化：byte_lane_skew（整字节通道旋转）/ all_zero / fwd_source_sub（错源整字）/ stale_line_replay** | H4 位谱、**H5 D1 签名复现（93% 检出）** |
| **CHAOSAddrPath**（新） | AGU 地址通路 | `lsq_unit.cc executeLoad`，translateTiming 前清 vaddr byte7 | byte7_zero | **D2**（H6） |
| **CHAOSPTW**（新） | ARM 页表走查器读出 | `arch/arm/table_walker.cc doLongDescriptor` | PTE 位翻转 / clearValidBit（AND 清零 valid 位，绕过 ECC）/ conditional_valid | **D3**（H7） |
| **CHAOSPosParity**（新） | 转发通路的位置锚定校验器 | `lsq_unit.cc` 转发前后（sender/receiver 双侧） | 校验器（非注入器）：双非交换加权模 256 聚合，检测字节通道置换 | paper §6.2 位置奇偶校验 |
| **CHAOSArmTLB**（新） | D-TLB 命中表项 pfn | `arch/arm/tlb.cc:164` | pfn 位翻转 / **pfn_to_mapped_page（活页替换）** | method2 静默错页 |
| **CHAOSArmSysReg**（新） | 系统寄存器 MRS 读 | `arch/arm/isa.cc:452` | 白名单合法值替换 | F5 配置错误 |
| **CHAOSROB** | ROB 条目 + **spec_leak（squash 不回滚）** | `rob.hh` | entry_bitflip / exc_suppress / spec_leak | method1 投机泄漏 |
| **CHAOSIQ** | 发射队列唤醒 | `InstructionQueue::wakeDependents` | wake_omit / src_ready_bitflip / wake_phase | method3 错源唤醒 |
| **CHAOSExMon** | 独占监视器 | STXR 判定 | stxr_force_fail / force_success | 自旋锁协议 |
| **CHAOSBPU / CHAOSDecode / CHAOSRAS / CHAOSExec / CHAOSFPU** | 预测/译码/返回栈/执行/浮点 | 各自流水级 | 位翻转 + events_to_skip 分散采样 | 全单元覆盖矩阵 |
| **CHAOSCache / CHAOSMem**（扩展） | cache 行 / DRAM 字节 | `mem/cache/cache.hh:166`、`AbstractMemory::access` | 字节翻转 + **protection_model（SECDED 模拟）** + addr_map_sub | 保护对照 |

所有 hook-on-event 注入器都带 **events_to_skip（geometric p=0.1）**——这是 Phase 3.0 审计发现的"first-eligible-event 采样偏差"的修复（确定性指令流下单故障注入会恒命中同一动态事件，污染 384 次重复的统计意义）。

### 2.3 campaign 基础设施（`origin/fi` 分支）

```
tools/campaign.py     # YAML campaign → manifest × n → 并行跑批 → heatmap.csv + summary.md
tools/runner.py       # manifest → arm_chaos*.py 参数映射 → gem5 一次 → 断言 faults∈{0,1} → 分类
tools/classify.py     # 九类有序分类器（SimulatorError→Hang→Crash→Inactive→Masked→SDC）
tools/wilson.py       # Wilson 95% CI
schemas/*.json        # arm-chaos-fi/v1 manifest/campaign 模式
campaigns/§2.*.yaml   # 34 个 campaign 定义（每单元 pilot + formal 两档；final-report-skeleton 口径 55 campaigns / 133 cells / ~16,000 reps 含衍生 cell）
workloads/directed/   # 20+ 定向 kernel（cholesky/reg_chain/l1d_reduce/fwd_checksum/movbe/ptr_chase…）
```

formal 规模统一为 **n=384 seeds + 5% replay 校验**，配 golden run 注册表（golden checksum 定死，防工具回归）。

### 2.4 探针 kernel 层（`fi_research/probes/`）

每个 probe 是 libc-only 静态链接的 AArch64 自检 kernel（exit 0=pass / 1=SDC / 2=setup error），一一对应现场故障链：

- `movbe_kernel` / `int_rmw_kernel` / `fp_fwd_kernel` — method2 的紧 store→load 转发序列（浮点版用于 IEEE754 位谱）
- `ptrskew_kernel` — **core179 故障链的直译**：指针数组装载→旋转错位→加基址→解引用，对应 `__per_cpu_offset[i]→rq` 链（H5 验证载体）
- `accum_kernel` / `fma_*` / `vec_copy_kernel` / `unipar_probe` — PRF/FPU/向量/位置奇偶专用
- `o3_chaos_smoke.py`（SE 主 harness，裸 ArmO3CPU，窗口可参数化）与 `o3_chaos_fs.py`（FS 薄封装，挂三注入器到 VExpress_GEM5_V1 真内核）

### 2.5 分析工具层

- `fi_research/bit_spectrum.py`（P6）— golden⊕actual 掩码 → sign/exponent/mantissa 位谱 + popcount 分布（H4）
- `fi_research/read_trace_stats.py`（P7）— read-trace 四分类：Benign（reads=0）/ Masked / SDC / Crash（H3 重尾分析）
- `tools/ras_escape_analysis.py` — 逃逸机理分解（A 无保护 / D ECC 后盲区 / F1 纠正…）+ occupancy 加权

## 3. 实验层：定量结论总账

### 3.1 SE 侧 15/15 单元 formal（n=384，三带格局）

| 带 | 单元 → P_SDC | 含义 |
|---|---|---|
| **SDC 集中带** | L1D 命中数据 **97.7%** [95.6,98.8] > L1D post-check（ECC 后通路）**90.9%** > LSQ 错源转发 **37.6%** > PRF 低位 3.9% | 数据通路与转发通路是全部静默风险 |
| **DUE 集中带** | ExMon 100% > RAT map_bitflip 95.8% > FreeList mark_free 72–77% > RAT f5 59.7% > IQ(madd) 100% | 控制映射类必崩（可检测） |
| **零风险带** | 取指（L1I 0%）、L2/DRAM 后备（0%）、Exec/BPU/RAS/Decode 主部、Mem addr_map_sub | 对缓存驻留 workload 族 |

**取指 vs 取数悬崖（L1I 0% vs L1D 97.7%）是全研究最大的单条保护排序证据**；L1D→L2 悬崖（97.7%→0%）是工作集驻留效应——ECC 预算应全部投给取数+转发通路。

### 3.2 三条横断定律（跨单元成立）

1. **合法域内错误是 SDC 核心形态**：错值全程合法（错源整字 / 活页 pfn / PRF 低位偏移 / RAT 合法 tag）才静默传播；域外错（RAT 越界 / PRF 高位 / byte7 清零）必崩或自愈。
2. **故障形态 > 故障位置**：同一 LSQ 转发路径，错源整字 37.6% SDC vs 单 bit 4.7%（8 倍）。
3. **单元 × 形态 × workload 三维决定结局**：F5 错源在 madd_chain 100% DUE vs cholesky Masked；PRF X3 在 cholesky 92.7% DUE vs reg_chain 100% SDC。

### 3.3 专项定论

- **H5（D1 签名复现，实锤闭环）**：`byte_lane_skew rot1` 注入 ptrskew_kernel → 30 次注入 28 次 PTR_CORRUPT 检出（93%），端到端复现 core179 的 `__per_cpu_offset[i]→rq` Oops 链；位谱 100% mantissa / 0% sign，与现场 method2 实测（93/0/6）定量匹配。
- **method2 三根因定量闭环**（FS 内核态，checkpoint restore + 调度域遍历 workload，2026-09-07/08）：现场"x10 垃圾指针→翻译故障"签名由 **AGU 地址路径**复现（P_DUE=100.0% [99.0,100.0]，n=384，kfree 类 Oops 正中调度器任务释放路径）；PRF 臂存活（SE 侧被 forwarding 掩蔽）、TLB 臂存活（活页静默替换）。**芯片侧结论：保护投资应指向 AGU 地址生成的合法性校验（地址规范位检查）**。
- **forwarding 掩蔽定律**：PRF 位翻转的架构可见性由生产者-消费者距离决定——背靠背转发距离内物理位翻转无读者（reads=0），跨迭代长距离依赖才可存活。PRF 保护价值 = f(workload 依赖距离谱)。
- **L1D 保护对照**：raw 97.7% SDC → +SECDED 0%（单 bit 全纠）→ 但 ECC 后通路（fill→PRF）仍是 90.9% 逃逸盲区——数据级 ECC 有效但不充分，需通路级 check-after-path。
- **H7（D3 ECC 对照，方向性）**：PTW clear_valid 注入下 ECC-on 2/2 完整走完 boot vs ECC-off 2/2 挂起；FS 5-seed 对比表在 `fi-h6-h7-*` 分支。形式化待健康机。
- **SE/FS 模式边界（方法论陷阱，mspc_paper 核心一章）**：SE 模式 SCTLR.M=0 → `mmu.cc:1213 translateMmuOff` 直接物理映射，从不走页表走查器——D2/D3 钩子在 SE 下恒 0 注入/0 可观察失败。曾有无声通过的 SE null 差点被当成发现；此边界已源码级闭环并写进论文作为仿真模式伪迹案例。

### 3.4 保护投资排序（occupancy 加权，§4.2 交付物）

l1d（5.46%）> l1d_fwd（5.09%）> lsq_fwd（4.45%）≫ physreg（0.33%）。对应建议：L1D 数据阵列 SECDED（有效）+ fill→PRF 通路级校验（ECC 盲区）+ LSQ 转发源 age/ID 校验（比 ECC 更针对错源形态）；取指通路无需数据级 ECC；L2/DRAM ECC 对缓存驻留 workload 是沉没冗余。

## 4. 理论层

- `docs/hypothesis/ARM64-SDC-uArch.md` — ARM64 vs x86-64 六维微架构差异 → SDC 敏感性差异的第一性原理分析 + 统一 AVF/PVF 概率积分模型（预硅仿真 + 后硅 SBST 多层次验证框架设计）。
- `docs/hypothesis/cpu.md` / `docs/cpu/kunpeng.md` — x86/ARM64 乱序核三层拆解笔记与 TSV110 公开微架构参数（L1D 64KB 4-way、store forwarding 6–7 周期、PRF-based scheduler ~33 entry 等，D1/D2/D3 定位时的几何/时序参照）。
- `docs/KUNPENG920-故障注入方案详细工程设计.md` — 全部注入器/campaign/工作负载的工程设计总册（§0 现状基线表逐注入器列出 hook 行号与已验证结果）。

## 5. 诚实边界（项目自我声明，贯穿全部文档）

1. gem5 O3 ≠ TSV110 RTL；无 HCCS/NoC 周期精确模型；跨 ISA 结论限"可建模子集"（前端译码 + PRF 压力两维），TSO-vs-弱序不可建模。
2. 本机即 CPU179 故障机：全部 formal（n=384）需第二台健康机复现才算最终确认（S6 未完成，每份 summary 的 Honesty note 都在提醒）。
3. SE 侧"全 Masked"类结论经历过三轮工具正确性危机（采样偏差族 9 个注入器、argparse exit=2 被误分类为 Crash、comp_map 静默改道 freelist/rob/iq→RAT），修正机制 = faults 来源日志核对 + golden replay + Wilson CI + events_to_skip——工具正确性审计本身已成为项目方法论的一部分。
4. 代理位置错位要如实声明：wake_phase 不捕获 method3 相位签名（相位在 LSU 转发时序，非调度唤醒）；DRAM 错位写在缓存驻留 workload 不可达。

## 6. 目录导航

| 路径 | 内容 |
|---|---|
| `CHAOS/` | gem5 源码树 + 全部自研注入器模块（`CHAOS/CHAOS*`）+ 上游 README |
| `fi_research/` | EXPERIMENT_DESIGN.md（科学设计）、probes/（探针 kernel + SE/FS 配置）、bit_spectrum.py、read_trace_stats.py |
| `docs/cases/` | 19 份 vmcore 案件报告 + 12 案 census + 17 案 paper2026 + MICROARCH/FULL_UNIT/FI_DESIGN 三份综合 |
| `docs/papers/` | 31 篇文献 PDF（Veritas/SEVI/PinDrop/SOSP23/SiliFuzz/Harpocrates/Orthrus/ITHICA/DelayAVF…） |
| `docs/hypothesis/`、`docs/cpu/` | 理论分析与 TSV110 参数 |
| `gem5-fs/` | FS 模式四件套（vmlinux 5.15.36 / ubuntu.img / bootloader / dtb） |
| `origin/fi` 分支 | campaign 基础设施（tools/schemas/campaigns/manifests/workloads）+ findings.md 总账 |
| `origin/fix/fi-tool-correctness` 分支 | Phase 3–5.6 全部定量结论 + method2 三臂定论（最新进度） |
| `.planning/` | 各阶段 task_plan/findings/progress（含"不看现有文档独立重研究"的 cases-2 对照组） |

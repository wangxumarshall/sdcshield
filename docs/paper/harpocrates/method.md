# Harpocrates 微架构覆盖指标复现：方法文档

本文档定义本仓库对 Harpocrates（ISCA'24 [1] + IEEE Micro'26 [2]）硬件覆盖
指标的复现实现：指标定义、数据采集点（file:line）、计算公式、与论文的
差异（含诚实边界）。实施过程与真机验证证据见
`docs/superpowers/plans/2026-09-16-harpocrates-coverage-reproduction.md`
与 `docs/harpocrates/reproduction-report.md`。

[1] Karystinos et al., "Harpocrates: Breaking the Silence of CPU Faults
    through Hardware-in-the-Loop Program Generation", ISCA 2024.
[2] Karystinos et al., "Harpocrates++: Automated Functional Program
    Generation Against CPU Faults and Silent Data Corruptions",
    IEEE Micro, Jan/Feb 2026.

## 一、平台

- 仿真引擎：gem5 v25.1.0.1+6（`CHAOS/gem5`，ARM SE 模式）
- CPU 模型：ArmO3CPU 配置为 TaiShan v110（鲲鹏 920）——8 宽全流水、
  ROB 97、numPhysIntRegs 125 / Float 96 / Vec 96、LQ 65 / SQ 47、
  3×IntALU + 1×MulDiv + 2×FPU + 2×NEON（`smoke_test/configs/
  two_level_taishan.py` + `fu_pool.py`）
- ISA：AArch64（论文为 x86-64；论文声明方法 ISA-agnostic，我们的全部
  基础设施在 ARM64——SSE FP 对应 AArch64 的 NEON/FP）
- ROI：m5ops workbegin/workend 标记（`tools/harp_wrap.py` 自动插入；
  编码 `0xff5a0110`/`0xff5b0110`，func 在 bits 23:16——实测踩坑见
  `tools/harp_roi_spike.c` 注释）

## 二、覆盖指标定义（与论文逐项对齐）

### 2.1 ACE lifetime 分析（bit-array 结构）

论文定义（[1] §II-D + Fig.3）：对结构中每个 bit，ACE 周期数 / 程序总
周期数，对全部 bits 汇总（即 AVF，0~100%）。ACE 区间：write→read 与
read→read；un-ACE：write→write（覆写未读）、read→evict/free（消费后
驻留）、fill 后未读即逐出。性质：transient 故障检测能力的上界。

本仓库实现（`CHAOS/gem5/src/CHAOSCov/`）：

| 结构 | 实现 | 采集点（file:line 见下节） | 分母 |
|------|------|---------------------------|------|
| IRF | 三物理寄存器空间独立区间账本 | regfile.hh getReg/setReg + free_list.hh + commit.cc | int 125×64b + float 96×64b + vec 96×128b |
| L1D | 块级区间账本（64B block = slot） | mem/cache/base.cc 四事件 | 1024 blocks × T |
| LSQ | SQ data 聚合区间账本 | lsq_unit.cc 三事件 | 47 entries × T |

公式（宽度加权，论文公式的逐字形式）：

```
IRF AVF = Σ_space (ACE_cycles_space × width_space)
          / Σ_space (num_regs_space × width_space × T_ROI)
L1D AVF = Σ_block ACE_cycles / (1024 × T_ROI)
SQ  AVF = Σ_interval [store_write, consume] / (47 × T_ROI)
```

IRF 双口径（超越点）：
- 乐观（`irfAvfInt` 等）：执行时读即计 ACE（wrong-path 读暂计）
- commit-confirmed（`irfAvfCommit*`）：每条已提交指令的每个物理源
  寄存器单独累计（squash 的读永不进入）
- 两者是 execute 视角 vs 提交视角，**非上下界关系**（commit 读晚于
  exec 读，单值区间更长；实测 branchy 负载差 14.8%）

### 2.2 IBR Input Bit Ratio（功能单元）

论文定义（[1] §II-D 脚注 5）：程序执行中输入到单元的总有效 bit 数 /
理论最大输入 bit 数（每周期全宽饱和）。非上界，与检测能力强相关；
"fast toggle-count-like" 近似。

```
IBR(FU类) = Σ_issue effective_input_bits(issue)
            / (full_width × fu_count × T_ROI)
full_width: IntAdd 128b(2×64) IntMul 128b FPAdd 256b(2×128) FPMul 256b
fu_count (TaiShan): IntAdd 3 IntMul 1 FPAdd 2 FPMul 2
effective_input_bits: Int/Float 源 64b、Vec 源 128b、VecElem 64b、CC 4b
```

OpClass→FU 类映射（SSE-FP 对应 AArch64 NEON）：IntAlu→IntAdd；
IntMult→IntMul；FloatAdd/SimdFloatAdd/SimdAdd(Acc)→FPAdd；
FloatMult(Acc)/SimdFloatMult(Acc)/SimdMult(Acc)→FPMul。

### 2.3 SFI 检测能力（golden 评估）

论文定义（[1] §II-E）：N 次注入中 n 次故障运行偏离 fault-free 运行；
detection = n/N。我们输出 Wilson 95% CI 与四类细分
{SDC, Crash, Masked, NoOutput}（比论文二分更细，超越点）。

注入协议：
- bit-array（IRF/L1D/LSQ）：transient 单 bit flip，位点与 cycle 均匀
  随机（IRF: CHAOSPhysReg phys 模式随机 phys_idx∈[0,124]；L1D:
  CHAOSCache 随机块+字节；LSQ: CHAOSLSQFwd）
- FU：permanent（论文为 gate-level stuck-at）——两级实现（见 §四）

## 三、采集点索引（函数名锚点，均在 CHAOS/gem5/src/ 下）

锚点用函数/方法名而非行号（行号随提交漂移，函数名稳定；全部
10 个锚点已 grep 验证命中——10/10，抽查命令与输出见 Task 7.4
commit message）。

| 事件 | 位置 | 说明 |
|------|------|------|
| IRF read（乐观） | cpu/o3/regfile.hh getReg int/float/vec 分支 | `harp_cov_on_prf_read` |
| IRF write | regfile.hh setReg int/float + setReg(void*) vec + getWritableReg vec | AArch64 FP 走 vec 空间的 writable/blob 路径 |
| IRF alloc/free | cpu/o3/free_list.hh SimpleFreeList::getReg/addReg | Int/Float/Vec 类过滤 |
| IRF commit-read | cpu/o3/commit.cc 提交成功点 | renamedSrcIdx 遍历 |
| ROI tick | commit.cc Commit::tick 头部 | roi_cycles++ + 占用采样 |
| ROI begin/end | sim/pseudo_inst.cc workbegin/workend | 全局钩子 |
| L1D read/write/evict | mem/cache/base.cc satisfyRequest / updateBlockData+handleFill / evictBlock | cache 指针 owner 过滤 |
| SQ write/consume/free | cpu/o3/lsq_unit.cc insertStore memcpy / writebackStores / completeStore | 聚合账本 |
| IBR issue | cpu/o3/inst_queue.cc scheduleReadyInsts issue 点 | harp_fu_class_of + harp_src_bits_of |
| FU permanent 注入 | cpu/o3/dyn_inst.hh setRegOperand（RegVal + blob 两个重载） | CHAOSFUPerm mask oracle |
| FU gate-level 注入 | dyn_inst.hh setRegOperand | CHAOSGateFU 结构差值 |

## 四、与论文的差异（诚实边界，逐项）

1. **ISA**：AArch64 vs 论文 x86-64。论文声明方法 ISA-agnostic；
   结构映射完整（SSE FP→NEON）。ARM64 MUL 三操作数无 x86 MUL 隐式
   RAX quirk（论文 [2] 报告的 int-mul 种子方差来源在 ARM64 上结构性
   不同）。
2. **FU 故障模型两级**：L1 execution-level permanent（CHAOSFUPerm，
   每 OpClass 结果固定位 XOR）+ L2 合成门级网表（CHAOSGateFU：
   Kogge-Stone 加法器 1154 门 + 移位加乘法器 44418 门，结构差值注入）。
   论文用 GeFIN 的 RTL 级门模型——gem5 无 RTL，**合成网表是结构近似
   而非真实 RTL**（网表经 W=4/8/12 位穷举 1700 万向量 + 64 位随机 +
   gem5 全程序等值三层验证）。FP 的 L2 网表（IEEE754 对阶/尾码分解）
   按计划降级预案延后——FP SFI 用 CHAOSFPU 位级注入。
3. **L1D 块粒度**：论文 byte 级（cache bit 的 ACE），我们 64B 块级
   （部分写的 byte-enable 信息在 classic cache 路径不易取全）。
4. **SQ 聚合账本**：SQ 前转消费是聚合近似（写回部分精确），per-slot
   精确追踪需穿 gem5 前转路径。
5. **IBR 分子口径**：issue 事件计（issue 即输入送达），不展开多周期
   FU 的 opLatency 驻留——与论文 "toggle-count-like" 近似口径一致。
   多发射下 IBR 可 >1（分母按单 issue 饱和算）。
6. **FU permanent 的 firstClock 门控**：permanent 从 cycle 0 起效会
   破坏 C 库启动的地址计算（实测 cycle 7529 Page fault）——注入从
   ROI 起生效，这是对论文"整个程序期间 permanent"的窗口化偏差。
7. **wrapper 循环开销**：harp_wrap 每迭代 40 条 ldr/str（寄存器
   进/出 asm 块）稀释短序列的差异——论文的 wrapper 亦有 init/epilogue
   开销（§V-D）但未量化占比。
8. **预算缩减**：种子敏感性 K=6×N=8（论文 50×更大 N）、截断实验
   4 档×N=8（论文 6 档）——量级对比而非逐点（Wilson CI 内结论成立）。

## 五、工具链

| 工具 | 功能 |
|------|------|
| tools/harp_wrap.py | 指令序列 → 确定性静态 ELF（m5ops ROI + epilogue 签名） |
| tools/harp_eval.py | SFI 检测能力评估（7 结构协议 + Wilson CI + coverage 并列） |
| tools/harp_advice.py | 变异建议规则引擎（证据驱动，超越点） |
| tools/harp_evolve.py | advice-driven vs 盲变异对比（论文 Fig.10 等价） |
| tools/harp_report.py | 单命令端到端报告（覆盖 + SFI + 建议） |
| tools/harp_baselines.py | 基线对比套件（论文 Fig.4 等价） |
| tools/harp_micro26.py | Micro'26 附加实验（种子敏感性 + 前缀截断） |

回归保证：所有注入器/采集器默认关闭（`harp_enabled`/`fu_perm_enabled`/
`gatefu_enabled` 全局守卫），默认路径 golden 输出与改动前逐字节一致
（每次 commit 的验证记录见 git log）。

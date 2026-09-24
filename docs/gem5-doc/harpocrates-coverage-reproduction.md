# Harpocrates 微架构覆盖指标复现：工程总结

> 本文总结 `feat/harp-coverage` 分支（2026-09-16/17，20 commit，`ce0767cb1..96e31e41e`）对
> Harpocrates 两篇论文（ISCA'24 [1] + IEEE Micro'26 [2]）SDC 检测用例覆盖指标的完整复现工程。
> 指标定义/公式/采集点索引/与论文差异表见 `docs/harpocrates/method.md`；
> 全部实验数据见 `docs/harpocrates/reproduction-report.md`；
> 实施计划（19 任务逐项验证记录）见 `docs/superpowers/plans/2026-09-16-harpocrates-coverage-reproduction.md`。
> 本文聚焦：系统结构、关键工程发现、与仓库既有 CHAOS 体系的关系、可复现入口。

[1] Karystinos et al., "Harpocrates: Breaking the Silence of CPU Faults through
    Hardware-in-the-Loop Program Generation", ISCA 2024.
[2] Karystinos et al., "Harpocrates++", IEEE Micro, Jan/Feb 2026.

## 0. 一句话概括

给定任意 AArch64 指令流序列，一次 gem5 仿真同时测出 **7 个微架构结构的硬件覆盖量化值**
（IRF/L1D/LSQ 的 ACE lifetime 分析 + IntAdd/IntMul/FPAdd/FPMul 的 IBR），并给出**证据驱动的
变异改进建议**；SFI 检测能力评估（Wilson 95% CI）闭环验证 coverage↑⇒detection↑——
建议驱动优化在 FU 目标上达到论文盲变异策略的 **18.8 倍**收敛效率。

## 1. 与仓库既有体系的关系

本工程不改动 CHAOS 的 19 个注入器语义，只做**叠加**：

```
                      ┌─────────────────────────────────────────────┐
   指令序列 (.S/ELF)  │  tools/harp_wrap.py                          │
   ─────────────────> │  确定性 wrapper：xorshift64 初始化 + m5ops    │
                      │  ROI 标记 + X9-X28 经 g_reg[] 内存进出 asm 块  │
                      └──────────────┬──────────────────────────────┘
                                     v  静态 aarch64 ELF
   ┌──────────────────────────────────────────────────────────────────┐
   │ CHAOSCov（新 SimObject，src/CHAOSCov/）                           │
   │  单次 --cov run 内测出全部覆盖指标：                               │
   │  ACE  ← regfile.hh / free_list.hh / commit.cc / base.cc /        │
   │         lsq_unit.cc 的守卫 hook（harp_enabled 全局开关）           │
   │  IBR  ← inst_queue.cc issue 点的 OpClass×源位宽计数               │
   └──────────────┬───────────────────────────────────────────────────┘
                  v
   ┌──────────────────────────┐    ┌────────────────────────────────┐
   │ harp_advice.py 规则引擎   │    │ harp_eval.py SFI 检测能力       │
   │ 覆盖证据→变异建议         │    │ 复用 CHAOSPhysReg/CHAOSCache/   │
   │ （超越点：advice-driven） │    │ CHAOSLSQFwd/CHAOSFPU + 新增     │
   └──────────────┬───────────┘    │ CHAOSFUPerm/CHAOSGateFU        │
                  v                └───────────────┬────────────────┘
   ┌────────────────────────────────────┐           v
   │ harp_report.py 端到端（最终交付）    │  coverage vs detection 并列
   └────────────────────────────────────┘  （论文 Fig.4 形态）
```

## 2. 指标实现要点（7 结构）

### 2.1 设计原则：咽喉点、零开销守卫、ROI 门控

论文把覆盖定义为**一次仿真内可测的快速代理指标**（而非 N 次注入的 SFI 慢路径）。
要在 gem5 里做到"单次 run 出全部指标"，采集必须满足三个约束：

1. **挂在语义事件的唯一咽喉点上**：ACE 度量"值的生命周期"，事件是值诞生（写）/
   值被消费（读）/ 值死亡（覆写/释放/逐出）。O3 流水线里很多 agent（IQ 唤醒、LSQ、
   commit、checker）都会碰寄存器，但全部经过 `PhysRegFile::getReg/setReg` 访问器——
   访问器就是不变量咽喉点。
2. **零开销守卫**：所有 hook 形如 `if (harp_enabled) harp_cov_on_xxx(...)`（全局
   bool，未挂载时单分支），保证默认路径 golden 逐字节不变——这是每个 commit 回归
   验证的基础。
3. **ROI 门控**：所有事件 hook 先查 `roi_active`（m5ops workbegin/workend 翻转），
   只统计被测序列区段，C 库 init/epilogue 不污染数据。

### 2.2 ACE 的计算：区间账本与增量累计

论文 Fig.3 区间语义（write→read、read→read 为 ACE；write→write 覆写未读、
read→evict/free 尾部为 un-ACE）适配到 rename-based PRF。每槽位状态
（`CHAOSCov.hh PrfRegState`）与累计规则（`CHAOSCov.cc irfOnWrite/irfOnRead`）：

```
birth / last_read / has_value / ever_read     ← 每物理槽位

写事件:   birth = now; last_read = now; ever_read = false
          （旧值若从未被读 → 无任何读事件 → 自然贡献 0，
           即 write→write = un-ACE，无需显式回退）
首次读:   ACE += now - birth          ← write→read 区间
后续读:   ACE += now - last_read      ← read→read 延伸
释放:     has_value = false           ← 读后驻留尾部从未累计，自然 un-ACE

AVF = Σ_space(ACE_cycles × width) / Σ_space(regs × width × T_ROI)
      width: int/float 64b、vec 128b；space: int(125)/float(96)/vec(96)
```

**为什么用增量累计而非存区间最后求和**：每个值的贡献在它的读事件发生时就落账，
"未读覆写""读后驻留"两类 un-ACE **不需要任何专门处理就天然为零**——状态机里没有
对应的加法路径，正确性论证只需两句话而非十个分支。

手算例：`add x9,x10,x11` 在周期 100 写入 x9 的新 phys 槽（birth=100）；周期 104
被读：ACE += 4；周期 107 再读：ACE += 3——该值贡献 7 个 ACE 周期。若 104 之前就被
覆写：无读事件，贡献 0。

### 2.3 ACE 的采集点：采什么、在哪采、为什么

#### IRF（三物理寄存器空间独立账本）

| 事件 | 采集点（`CHAOS/gem5/src/cpu/o3/`） | 为什么在此 |
|------|-----------------------------------|-----------|
| 读 | `regfile.hh PhysRegFile::getReg` 的 Int/Float/VecRegClass 三分支 | O3 里值的每次消费最终都坍缩到该访问器，无论发起方是哪级流水——挂流水级会漏 |
| 写 | `regfile.hh setReg(RegVal)` Int/Float 分支 + `setReg(const void*)` Vec 分支 + **`getWritableReg` Vec 分支** | AArch64 FP/SIMD 结果驻留 vector regfile 且**不走 setReg**——经 getWritableReg 拿裸指针直写（`arch/arm/isa/operands.isa` 的 `FpDest = VectorElem` 决定）。只挂 setReg 时 vec 空间永远读不到写事件、irfAvfVec 恒 0——实测踩坑后补齐，rand_fp 的 irfAvfVec 从 0 变 0.046520。指针交付即写事件（槽即将被覆写），保守正确 |
| 分配/释放 | `free_list.hh SimpleFreeList::getReg()/addReg()` | rename 设计里"值死亡"不是 regfile 事件——槽位可晚点才被覆写，但**值在回到 free list 那一刻就死了**（此后读到的是新值）。read→evict 的 un-ACE 尾部对应 read→free。hook 需类过滤（只通知 Int/Float/Vec）——SimpleFreeList 被 vec/pred/mat/cc 共用，不过滤会越界（实测 SIGSEGV 定位） |
| 提交确认读 | `commit.cc commitInsts()` 提交成功点，遍历 `head_inst->renamedSrcIdx(sr)` | 乐观账本把 wrong-path 读也算了（被 squash 的指令确实执行过、读过 phys reg）。要知道"架构上真实的消费"，唯一权威是 **commit**——被 squash 的指令永远到不了那里。由此产生超越论文的双口径：branchy 实测 乐观 0.0995 vs commit 0.1141（差 14.8% = wrong-path 读的量化）。注意两口径**非上下界**——commit 读比 exec 读晚，单值区间反而更长（口径差异，非矛盾） |

实测对照：readwrite 链（写后读）0.0891 > 死写链 0.0804；fp 序列只在 vec 空间
出值（0.0465），纯整数序列 vec=0。

#### L1D（64B 块粒度账本）

| 事件 | 采集点（`mem/cache/base.cc`） | 为什么在此 |
|------|------------------------------|-----------|
| 读 | `BaseCache::satisfyRequest()` 入口（`pkt->isRead() && blk`） | 该函数恰在"驻留块满足一次 demand 访问"时被调——cache 消费事件的唯一定义点 |
| 写 | `updateBlockData()` + `handleFill()` | 前者是 store 命中/写回的数据更新路径，后者是 miss 后 fill 装载新行——块数据被（重）写的两条必经之路 |
| 逐出 | `evictBlock()` | 块离开 cache = 区间关闭；fill 后未读即逐出 = un-ACE（论文原例） |

三个 hook 都带 `this` 做 **owner 过滤**：基类方法被 L1I/L1D/L2 共享，CHAOSCov 只统计
配置的 targetCache（L1D），否则 I-cache 指令流会混进数据覆盖。块粒度（非论文的
byte 级）是诚实近似：部分写的 byte-enable 信息在 packet 路径不易取全（差异表
声明）。实测：rand_mem l1dAvf=0.00136 vs sample_seq 0.00025（5.4×）。

#### LSQ（SQ data 字段，Micro'26 第三个 bit-array）

| 事件 | 采集点（`cpu/o3/lsq_unit.cc`） | 为什么在此 |
|------|-------------------------------|-----------|
| 写 | `insertStore()` 的 memcpy 处 | 该 memcpy 就是"store 数据此刻驻入 SQ data 字段"——论文靶结构的诞生瞬间，精确到指令 |
| 消费 | `writebackStores()` 拷出到 `inst->memData` 处 | 最终内存写回前的唯一数据出口 = SQ data 的终点消费 |
| 释放 | `completeStore()` 头部 | SQ 表项清除。被 squash 的 store 到此但从未消费 → 无消费事件 → 贡献 0，与增量累计"天然为零"性质一致 |

聚合账本（诚实边界）：store→load **前转**也消费 SQ data，但 gem5 前转判定路径里
拿不到便宜的 slot 索引；账本按"消费关闭最老开区间"（SQ 大致按序排空）——写回部分
精确、前转部分近似（method.md 声明）。实测：rand_mem sqAvf=0.0170 vs 0.0141。

### 2.4 IBR 的计算与采集点（4 FU 类）

```
IBR(FU类) = Σ_issue effective_input_bits(issue)   ← 分子（issue 点运行时累计）
            ─────────────────────────────────
            full_width × fu_count × T_ROI          ← 分母（配置常量）

分子: harp_src_bits_of()——Int/Float 源 +64b、Vec 源 +128b、
      VecElem +64b、CC +4b、misc +0
分母: full_width = {IntAdd:128, IntMul:128, FPAdd:256, FPMul:256}
      （FPAdd/Mul 256b = 2×128b NEON lane 对——SSE-FP 对应物）
      fu_count = {3,1,2,2}（TaiShan v110 FUPool 实配，取自 fu_pool.py）
OpClass→FU 类映射: IntAlu→IntAdd；IntMult→IntMul；
      FloatAdd/SimdFloatAdd/SimdAdd(Acc)→FPAdd；
      FloatMult(Acc)/SimdFloatMult(Acc)/SimdMult(Acc)→FPMul
      （标量 FP 与 SIMD 变体汇入同一组物理 FP/SIMD 单元）
```

**唯一挂点**：`inst_queue.cc scheduleReadyInsts()` 发射循环里、`fu_pool->getUnit(op_class)`
之后、单周期/多周期分叉之前。为什么必须是这一点：

1. **三要素同时在场**：`op_class`（就绪队列的键 = FU 类）、`issuing_inst`（DynInst
   指针，取 `numSrcRegs()/srcRegIdx(i)` 算操作数位宽）、FU 授权结果。发射前任何点
   缺要素，发射后（FUCompletion 事件）已散布到异步回调。
2. **一条 hook 盖住两条执行路径**：单周期走 `instsToExecute`、多周期走
   `FUCompletion` 事件——两路都在该 if 前汇合；挂 execute 回调要挂两处且引入事件
   序问题。
3. **issue = 输入送达**：论文分子是"输入到单元的总有效 bit 数"，发射那一刻操作数
   已（或即将在本周期）呈现给 FU 输入端——issue 事件就是输入送达事件的原地定义。
4. **源操作数按 RegClass 分档**：论文强调 IBR 计**有效**位（"a 64-bit arithmetic
   unit ... its inputs may not always be 64-bits wide"）。`RegId::classValue()`
   给出精确档位——D 寄存器对的 fadd（2×128b VecRegClass）与 GPR 对的 add（2×64b）
   在同一类 FU 上交付的 bit 数差一倍，正是 IBR 要捕捉的差异。

算术自检（实测）：rand_fp 的 FPAdd 32768b/128 issue = **恰好 256 b/issue**
（2×128b 全宽）；FPMul 24320/95 = 256。`ibrFpAdd = 32768/(256×2×895) = 0.0715`，
量级落论文 FU 饱和值 5-10% 区间。口径说明：分子按 issue 事件计，不展开多周期 FU
的 opLatency 驻留——与论文 "fast toggle-count-like measurement" 近似一致；宽发射
机器上 IBR 可 >1（分母按每 FU 每周期单 issue 饱和算）。

### 2.5 T_ROI 与 ROI 门控链

```
wrapper 里的 .inst 0xff5a0110 / 0xff5b0110
  → gem5 解码为 Gem5Op64（func = bits 23:16 的 workbegin/workend）
  → sim/pseudo_inst.cc workbegin()/workend()
  → 全局通知 harp_cov_notify_work_begin/end()
  → CHAOSCov::onWorkBegin/End() 翻转 roi_active + 记录起止 tick
  → Commit::tick() 里的 harp_cov_on_cycle() 在 roi_active 时递增 roi_cycles
    （同时做 IRF 占用率直方图采样——建议引擎"提高依赖距离"规则的证据来源）
```

**为什么**：论文测"测试程序"的覆盖；wrapper 用标记圈出被测序列，C 库启动/打印不进
分母（否则短序列的 FP 占比被 1.38M 周期库代码稀释到 0——基线套件实测踩过）。
Commit 是流水线唯一保证每周期被调度的心跳级（其他级会 stall）。roi=all/cycles 模式
（非 wrapper 负载）override `statistics::Group::preDumpStats` 在 dump 前自动
finalize（无 marker 触发时 stats 全 0——branchy 负载实测暴露后修）。

### 2.6 为什么不在别处挂（两条实测反面证据）

1. **iew.cc 的 `instResult` 队列**：看似写回路径（writebackInsts 里 insts 即将送
   commit），实为 checker-only（`RecordResult` 旗标控制，O3 无 checker 时为空）。
   首版 FU 注入在此对 instResult 做 XOR，篡改 **954649 次**、输出零变化。教训：
   挂点必须在"每个消费者都会经过"的**访问器**上，而非"看起来像写回"的队列上。
2. **`setRegOperand` 的 RegVal 重载对 FP**：`fmul d` 结果经 blob/writable 指针
   路径，FloatMult 的 OpClass 匹配在 RegVal oracle 上恒为 0（CHAOSFPU 的
   `getRegOperand blob hook` 才是 FP 源读的正确点）。教训：同一名义事件（写回）
   在 gem5 里按操作数类型**分裂成 2-3 条物理路径**，hook 必须覆盖全部分裂路径。

这两条（连同 free_list 类过滤越界、m5ops 编码静默丢失、Kogge-Stone p0 位错选等，
共 14 项）全部留档于 §3 与代码注释——它们与正面设计同等重要，是这套指标实现
"为什么长这样"的另一半答案。

### 2.7 SFI 检测能力

detection = (SDC + Crash)/N + Wilson 95% CI，四类细分（比论文二分更细）。
注入协议：bit-array 用 transient 单 bit flip（位点×周期均匀随机）；FU 用 permanent
两级故障模型——L1 execution-level（CHAOSFUPerm：目标 OpClass 每次结果固定位 XOR，
挂 `dyn_inst.hh setRegOperand` 的 RegVal+blob 两重载——真实写回路径）+
L2 合成门级网表（CHAOSGateFU：Kogge-Stone 加法器 1154 门 / 移位加乘法器 44418 门，
结构性差值注入，即同一输入过干净网与故障网取 XOR delta 应用到架构结果）。

## 3. 关键工程发现（全部实测定位，代码注释留档）

### 3.1 gem5 内部路径类

1. **AArch64 FP 结果绕过 `setRegOperand`**：FP/SIMD 寄存器写回走 vector regfile 的
   `getWritableReg` 可写指针路径与 blob 路径（`arch/arm/isa/operands.isa` 的
   `FpDest = VectorElem` 决定），挂在 RegVal 重载上匹配恒 0——CHAOSFPU 的
   `getRegOperand blob hook` 才是 FP 数据路径正确挂点。IRF 的 vec 空间采集同理。
2. **`instResult` 队列是 checker-only**：首版 FU 注入挂 iew.cc writebackInsts 对
   instResult 做 XOR，篡改 954649 次输出毫无变化——该队列受 RecordResult 旗标控制，
   O3 无 checker 时为空；真实写回路径是 `dyn_inst.hh setRegOperand → cpu->setReg`。
3. **ROI 门控的三段语法**：gem5 stats 的 `"memory"` clobber 必须在 extended asm 的
   第三冒号段（GCC 12 实测 T5/T10/T11 最小用例对照）；且单条 asm 最多 30 操作数，
   20 个 `"+r"` 输出超限——wrapper 改用"全局数组内存进出 + 寄存器全列 clobber"。
4. **m5ops 编码**：AArch64 m5op 的 func 字段在 bits 23:16（`0xff000110|(func<<16)`）；
   误放 bits 15:8 时 gem5 仍解码为 Gem5Op64 但 func 读出 0，静默 dispatch 成 M5OP_ARM，
   ROI 标记丢失且无任何告警。
5. **`preDumpStats()` 钩子**：roi=all/cycles 模式（无 m5ops 标记）下 stats 在
   workbegin/workend 永不触发，需 override `statistics::Group::preDumpStats` 在
   dump 前自动 finalize（Python 侧调 C++ 方法会 AttributeError——SimObject 不暴露
   任意方法）。

### 3.2 故障模型类

6. **permanent 故障必须 firstClock 门控**：从 cycle 0 起效的 FU permanent 会破坏
   C 库启动代码的地址计算（实测 cycle 7529 即 Page fault，远早于 ROI），须从
   ROI 起生效——这是对论文"整个程序期间 permanent"的窗口化偏差（method.md §四.6）。
7. **Kogge-Stone sum 位 bug**：`sum[i] = p0[i] XOR carry[i-1]` 的 p0 是预处理位而非
   最终前缀位——独立穷举测试（W=4/8/12，约 1700 万向量）捕获，gem5 全程序等值复验。
8. **网表哨兵冲突**：乘法器首版用 `-1` 哨兵表示"常数零位"，与 PI 负索引编码
   （`in0<0 → PI[-in-1]`）冲突使网表错值——改为常量门（`a0 AND NOT a0`）。
9. **ARM64 无 x86 MUL quirk**：Micro'26 论文的 int-mul 种子方差（~17%）源于 MUL 隐式
   写 RAX 被覆写掩蔽；ARM64 三操作数 MUL 无此机制，实验证实种子方差≈0——结构性差异，
   比论文更稳定（method.md 差异表预判，实验证实）。

### 3.3 工具链类

10. **ARM 汇编注释剥离**：`split("#")` 会把内存操作数 `[x8, #24]` 的立即数前缀当
    注释截断（盲变异第 7 步 gcc 报 invalid expression）——改为括号深度感知剥离。
11. **共享 m5out 的残档陷阱**：批量 run 复用同一 `-d` 目录时，失败的 run 读到上一
    负载的残档 stats（四组基线数值完全相同的假象）——每次独立目录 + rc 检查。
12. **ROI 口径一致性**：随机序列组误用 roi=all（整程序 1.38M 周期进 ROI，FP 占比
    稀释到 0）——wrapped 序列必须显式 roi=m5ops。

### 3.4 构建类

13. **内存约束**：本机 29GB/126 核，`-j126` 编译 OOM 被 kill——`-j16` 安全。
14. **namespace 嵌套**：`pseudo_inst.cc` 的 `namespace gem5{namespace pseudo_inst{...}}`
    内再开 `namespace gem5{}` 会解析成 `gem5::pseudo_inst::gem5::`（ld undefined）；
    `void ::gem5::f();` 限定声明 GCC 拒绝——正确形态是在外层 gem5 作用域声明。

## 4. 核心实验结论（数据见 reproduction-report.md）

| 论文主张 | 复现结果 |
|----------|----------|
| ACE 是 bit-array 检测上界（Fig.4） | ✓ 两负载实证：irfAvfInt=0.0891 ≥ detection=0.0400（N=50）；gap=software masking |
| coverage↑⇒detection↑（Fig.10 核心） | ✓ advice 曲线 0.115→1.089 单调饱和；同预算盲变异 0.115→0.058（**18.8×**） |
| FU permanent 检测 ~100%（Fig.11） | ✓ 位级 0.6333（饱和）；门级 numDiffs 262173/262189（99.99% 显形） |
| FP 平均检测低（§III-C） | ✓ CHAOSFPU N=20 全 Masked（FP 软件掩蔽真实测量） |
| 通用/随机负载结构覆盖分化（Fig.4） | ✓ 基线套件：int 组 ibrFpMul=0 / fp 组 ibrIntAdd≈0.001（mix 分化清晰） |
| 种子方差 <1%~17%（Micro'26 Fig.5） | 结构性差异：ARM64 方差 0（无 RAX quirk） |
| 0.1× 前缀保持检测（Micro'26 Fig.6） | ✓ 更强：1 条 mul × 200 iters 即 1.00 |

**如实记录的负/中性结果**：IRF 目标 12 步小预算下 advice≈blind（论文 IRF 需 ~5000
迭代；8 指令序列被 wrapper 每迭代 40 条 ldr/str 循环开销主导）；intmul permanent
SFI 下基线与 evolved 均 0.6333（注入饱和，不可区分——错误必破坏 wrapper 载入存回链）。

## 5. 工具清单与复现入口

| 工具 | 用途 | 典型命令 |
|------|------|----------|
| `tools/harp_wrap.py` | 序列→确定性 ELF（ROI 标记+签名） | `--seq x.S --out x --iters 200` |
| `tools/harp_eval.py` | SFI 检测能力（7 结构协议） | `--seq x --structure irf --n 50` |
| `tools/harp_advice.py` | 变异建议规则引擎 | `--seq x --top-k 6` |
| `tools/harp_evolve.py` | advice vs 盲变异对比 | `--seq x.S --target fu-intmul --steps 12` |
| `tools/harp_report.py` | **端到端单命令报告** | `--seq x.S --sfi 30` |
| `tools/harp_baselines.py` | 基线对比套件 | `--n-random 2` |
| `tools/harp_micro26.py` | Micro'26 两实验 | `--seq x.S --k-seeds 6 --n-sfi 8` |

复现口径注意：SFI 类实验含随机采样（detection 小数随 seed 波动，Wilson CI 为此设计）；
结构性结论（上界关系、正负对照、mix 分化、18.8×量级）在固定 seed 下确定性重现。
`artifacts/` 在 .gitignore（仓库惯例），克隆后须重跑生成。

## 6. 回归保证与遗留

所有采集器/注入器默认关闭（`harp_enabled`/`fu_perm_enabled`/`gatefu_enabled` 全局
守卫，未挂载时单分支零开销），默认路径 golden 输出与改动前逐字节一致
（`SUM=75555881316236 CRC=f1c0b5de`，每个 commit 的 message 引用真实验证输出）。

遗留（reproduction-report.md §七）：FP 门级网表（IEEE754 分解，现用 CHAOSFPU 位级）、
L1D byte 级（现块级）、SQ per-slot 精确账本（现聚合）、IRF advice 规则在大预算下的
收敛性、MiBench-arm 基线（现 directed 10 负载替代）。

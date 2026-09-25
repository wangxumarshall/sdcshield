# cn23154 608 核 SDC 战役 — 最终报告

campaign: ai-run-cn23154-20260923-220641 | 战役窗口 2026-09-23 22:06 → 2026-09-24 21:45 | 终稿 2026-09-24

> 权威数据与逐问证据: `RES = /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/ai-run-cn23154-20260923-220641`
> 本文件为 repo 侧 12 问中文摘要; 每问的 run_id 级证据指针见 `RES/final-summary.md`。

## 结论 (一句话)

**cn23154 全部 608 核中唯一 SDC 故障核为 cpu139 (NUMA3)**: 核私有 byte-lane-6(+7) 边际弱单元, 真实 XLSDFT DFT 负载下满频 (>=1800MHz) 必现 (全战役 56 例故障事件 = 55 SIGSEGV + 1 静默数值 SDC 直接捕获, 100% 归因 cpu139, 合并 Clopper-Pearson 95% CI [94.7%, 100%]); 核心频率 <=1.55GHz 不复现; 排除 cpu139 后对照 42/42 全净 (含 3h 连续运行零故障)。已交付自含最小复现物 (16 有限元/125 原子, 原版 1/40): F 侧 10/10 崩 / C1 侧 7/7 净, 一键验证作业 ~10-15 min。

## 12 问摘要

**Q1 哪些核稳定复现?** 唯一 cpu139。P2 三窗全核扫描 13/13 崩溃全钉 cpu139 (崩溃线程 psr 逐跑多数采样直证); 基线 3 + swap37 2 + P5 2 + 最小化全梯 28 + 复现包 3 — 全战役 56 例无一例外, 归因不变量 (rank3 + 崩溃线程 psr=139) 全程成立。NT9 起可线程级直证 (worker8 尾采样 psr=139)。

**Q2 失败率与置信区间?** cpu139 @ F (rank3 含 139): 全战役 SDC-有效 F 运行 55/55 崩 + 1/1 数值检出 = 100% (筛查期 17/17 Wilson [81.6%,100%]; 合并 Clopper-Pearson [94.7%,100%])。cpu139 排除后 (C1): 42/42 全净 (V1-V4B 各 5/5 + v6a-d 各 6/6 + 复现包 1/1 + skip139 3h/86 轮 SCF)。崩溃时长 67-355s 阵发但配置确定。

**Q3 NUMA3 其他核?** 干净。swap37 对照 (rank3↔rank7 交换): 崩溃随 cpu139 走到 rank7, 原 rank3 窗口 (含 138/140) 零故障; 138/140 满压对照干净 → 排除共享缓存/内存通道/NUMA 域级故障。

**Q4 其余 607 核?** 605 exercised_clean + 2 mixed_freq_clean (低频窗口干扰, 非故障证据); EXCLUDED 5 核 (cpu240@1452/cpu304@1481/cpu252@1445/cpu406@1475/cpu419@1307, <1500MHz 只压不测, cpu497@1461 曾跌后恢复) + GREY 4 核 (cpu517/525/320/374)。低频核集合随时间漂移, 与 cpu139 SDC 无关 (139/140 全程 >=1996MHz)。证据强度: 非 139 核为 "未见故障" (窗口暴露内), 非数学证明。

**Q5 608 全覆盖集合相等?** W0/W1/W2 三窗 rankfile 覆盖 0-607 全体 (cpu_sweep/coverage_map.txt 集合相等证明), 每窗绑定行留证; cpu-results.csv 608 行逐核判决。

**Q6 与拓扑相关性?** 核私有 (physical core), 与 SMT (无 SMT)/NUMA/socket/cache/内存均不相关 — 608 核中恰好 1 核故障本身就是 "非按域聚集" 的证据; swap37 迁移实验直证跟核不跟 rank。

**Q7 最可信微架构假设?** cpu139 核私有 **byte-lane-6(+7) 边际弱单元** (数据通路某 lane 在高频+特定上下文边际不足): 候选定位 L1d/L2 数据阵列弱位 (中) / LSU 读出时序边际 (中) / PRF 弱表项 (低-中); 9 类合成负载 (15 计算单元 × 8 图案 + SVE-512 FMA) 全排除 → 非 ALU/FPU 功能单元, 指向访存通路; perf 计数器零异常 → 事件型偶发, 非永久性; 频率窗口 [1550,1600]MHz 界定 → 时序边际特征。

**Q8 支持证据?** ① 归因不变量 56/56 (rank3 + psr=139); ② 地址签名两类: byte-6 损坏 40 例 (bits48-55 垃圾 + 低 48 位完好有效指针, 指向 heap 0xaaaa/0xaaab + mmap 0x4000 三区) + canonical 野指针 15 例 (0x3ff9-0x3ffe worker 栈带/0x4000 库带, v6c 起集中 @stencil a.out+0x258f00); ③ 崩溃点家族集中 (stencil/swap37/QL/libomp-top 四簇); ④ swap37 跟核迁移; ⑤ skip139 3h 零故障 + 数值金标全对。

**Q9 削弱方面 (诚实清单)?** 微架构单元未最终分离 (无硬件 trace, 候选三选一未定); 数值 SDC 仅 1 例样本 (V4B v4bnt9_F_2: LVTX_DSYGVD_REPLICA_MISMATCH 低 8 位 4bit 翻转 33 ulp, worker8=cpu139, cpu132-138 同输入逐字节一致 — 8:1 核间共识, 排除软件非确定性); run_B 一次 rankfile 格式错误 (自纠正, 见 status.md); 合成负载全排除同时也意味着 "非真实负载路径" 无法复现 (上下文依赖是特性也是限制)。

**Q10 最小复现用例?** `RES/minimal-reproducer/` 自含包 (v6d 终点): 16 有限元/1 元素每 rank/125 原子 (原版 640 元素/5120 原子的 1/40), NT9 二进制随包 (binhash 763c6843f504e1f7, 构建自证 `make -f Makefile.xlsdft_920f NT=9`), rankfile 唯一差异 F(rank3=131-139, OMP 线程 8 落 cpu139) vs C1(131-138,140); reproduce.sh/verify.sh/job_repro.sh + 判定标准与预期值。缩减阶梯全史: MAXIT_SCF 1000→3 → FD_GRID 160 120 120 → NSTATES 640 必要 → NT 36→18→9 → NCOMMS 16=fabric 下限 → NELEMS 640→16 四级全中性。

**Q11 最小用例复现率?** F 侧 10/10 = 100% (v6d 作业 1721399 内 7/7 + 端到端验证作业 1721859 内 3/3 仅用包内文件), C1 侧 7/7 干净; NELEMS 四级阶梯 F 累计 28/28。签名一致性 4 不变量全同 (地址类家族/崩溃点家族/归因不变量/时延带 74-151s ⊂ 87-355s)。单运行中位崩溃时延 ~94s; C1 对照 ~197-200s (rc=0, scf 三迭代金值 7.796e-01/5.784e-01/3.103e-01, E3=-206.7261637648 展幅 2.2e-14)。

**Q12 当前结论限制?** ① 微架构单元三候选未分离 (需硬件 trace/RMA 分析); ② 数值 SDC 仅 1 例, 统计面貌未知; ③ 607 核 "干净" 是窗口暴露内的未观测, 非永久保证; ④ 无 EDAC/RAS 计数器交叉验证 (本节点 EDAC 为空); ⑤ 根因最终确认需供应商侧硬件分析, 本研究提供的是软件侧证据链闭环 + 可交付复现物。

## 交付物清单

| 交付物 | 位置 |
|--------|------|
| 12 问逐答 (权威, 带 run_id 证据) | `RES/final-summary.md` |
| 最小复现包 (一键 ~10-15 min) | `RES/minimal-reproducer/` (`dsub -s .../job_repro.sh`) |
| 管理员请求 (cpu139 下线 + 低频台账) | `RES/summary/admin_requests.md` |
| 微架构诊断报告 (P5) | `RES/diagnostics/diagnosis-report.md` |
| 筛查/归因/最小化过程报告 | `RES/summary/P2-sweep.md`, `RES/fault_localization/attribution.md`, `RES/minimization/results.md` |
| 环境与实验设计 | `RES/environment-report.md`, `RES/experiment-plan.md` |
| 全战役时间线 (含自纠正记录) | `RES/status.md` |
| 逐核判决 608 行 | `RES/cpu_sweep/cpu-results.csv` |
| repo 侧过程文档 | `docs/cases/cn23154-sdc-campaign-2026-09-23/P2-sweep.md` … `P6-minimization.md` |

## 复现 (登录节点)

```bash
RES=/home/share/suke/0903-NUMA3-report-materials-wangxu-v2/ai-run-cn23154-20260923-220641
dsub -s $RES/minimal-reproducer/job_repro.sh   # 首选: freqmon + F×3 + C1×1 + 自动判定表
dsub -s $RES/scripts/job_baseline.sh           # 备选: 原版全尺寸, ≤355s rank3 SIGSEGV
```

判定标准 (verify.sh 自动执行): F 臂 REPRO_CRASH_ATTRIBUTED = rc139 ∧ rank3 ∧ 崩溃线程尾采样 psr=139 ∧ 地址类非 NA ∧ cpu139 窗口 min freq ≥1800; C1 臂 CLEAN_CONTROL_PASS = rc0 ∧ scf 三迭代金值 ∧ E3 偏差 <1e-10 ∧ cpu140 ≥1800。
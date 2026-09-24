# P6 最小化复现提取 — cn23154 cpu139 SDC 故障 (campaign Task 10)

生成: 2026-09-24 (活文档, 阶梯进行中) | 方法: 累进制单因子缩减阶梯 (每版本 F×5 + C1×5 + 前驱锚 A/B 对比)
前置: P5 微架构诊断 (P5-diagnosis.md) — cpu139 核私有 byte-lane-6(+7) 边际弱单元, 17/17 崩溃签名
计划: docs/superpowers/plans/2026-09-23-cn23154-sdc-608core-campaign.md Task 10 | 集群数据: RES/minimization/

## 0. 目标与判定规则

目标: 从原版 XLSDFT 用例 (Si.inpt: NELEMS 10 8 8 / NCOMMS 2 2 4=16 rank / NSTATES 640 / FD_GRID 220 160 160 / MAXIT_SCF 1000, NT=36 编译) 逐因子缩减到最小复现形态, 供快速回归与硬件现场复现。

判定规则 (关键, 防误判):
- **失败率对比**, 不作单次 "是否曾失败" 判定: 每版本 F (rf_ppr, rank3=114-149 含 139) ×5 reps vs C1 (rf_skip139, rank3=114-138,140-149 排除 139) ×5 reps, 同作业穿插前驱锚
- 因子判定: F 5/5 崩 + C1 5/5 净 → 该因子缩减**中性** (继续 deeper); F 0/5 崩 → 该因子为**必要因子** (回退, 阶梯换轴)
- 每次崩溃必须验签: si_addr byte-6 垃圾 + 低 48 位完好 + rank3 (与历史 17 例一致) — 防止 "最小化改变了故障模式" 的假收敛
- 频率合规: 每运行窗 cpu139/140 逐窗 min freq >= 1800 (实测全程 1998+)
- 二进制不变量: binhash 804792f339421d95 全程不动 (NT 重编译 rung 例外, 单独目录 + 记录新 binhash)

## 1. 计划适配 (执行时发现, 诚实记录)

- 原计划因子 1 "去 Si.ion (单段 SCF)" 字面不可行: src/preparation.cpp:47 无条件读 Si.ion, 无免 ion 运行模式; 且单段 (NELEMS 1 1 1) 全晶胞内存不可行 → 适配为分段维度缩减 (XLSDFT_NELEMS 部分缩减), 排到输入参数类因子之后
- 执行顺序按可行性重排: MAXIT_SCF → FD_GRID → NSTATES → NT 重编译递减 → NCOMMS → 分段缩减; 累进制 (V2 = V1 + 网格缩减)
- 依据 (MAXIT_SCF 优先): 17 例崩溃全在 SCF iter 1-2 (eigen 簇 87-140s = iter1 中段; stencil/libomp 簇 345-355s = iter1 完成/iter2 起点), 3 次迭代覆盖窗口且余量充足

## 2. 基础设施 (RES/)

- `scripts/runmin.sh <cfg> <inpt> <rf> <TMO>`: 变体输入 runner (逻辑同 runrec.sh); 运行目录/结果表在 minimization/; AOUT/NT_THREADS 环境变量可覆盖二进制与 OMP 线程数 (NT rung 用, 2026-09-24 10:49 参数化, bash -n + 语义验证)
- `minimization/versions/<vid>/Si.inpt`: 版本输入 (diff 逐字验证单行变更)
- `minimization/results.tsv`: 逐 run 登记 (run_id, config, binhash, inputhash, ts, rc, freq_verdict, bind, rawdir)
- `minimization/analyze.sh`: 崩溃签名表 (si_addr 分解: byte6 垃圾值 / 低 48 位 / mmap-heap 区域 / rank); 注意 OpenMPI "Failing at address" 是 handler 伪值, 签名只取 "Caught signal 11" 行
- 作业: job_min_v{1,2,2d,3}.sh, job_min_v4_nt18.sh (含 NT=18 重编译阶段, __VX_INPUT__ 占位)
- NT=18 rankfile: rank3=122-139 (thread17→139), C1 变体 122-138,140

## 3. 阶梯结果 (进行中)

| 版本 | 变化 (相对上一版) | F | C1 | 判定 |
|------|------------------|---|----|------|
| orig (基线) | — | 19/19 崩 (历史 17 + 锚 2, 87-355s) | 3h/86 轮 SCF 0 崩 + 锚 1201.6s cap 0 崩 | 原版仍复现 |
| V1 maxit3 | MAXIT_SCF 1000→3 | **5/5 崩** (101-350s, 全 rank3, byte-6 签名 5/5) | **5/5 净** (798-807s 自然完成) | MAXIT_SCF 中性; 无崩运行封顶 ~800s |
| V2 fdgrid | + FD_GRID 220 160 160→160 120 120 (每元素 16×15×15) | **5/5 崩** (89-171s, byte-6 签名 5/5) | **5/5 净** (357-362s, 五 rep 物理量逐位一致) | **网格缩减中性** (频率 12/12 窗全频) |
| v2d | + FD_GRID→120 96 96 (每元素 12×12×12) | 5/5 崩但全 (nil) 空指针 (23-30s, comm1 齐崩, 迭代前) | **5/5 崩, 同 (nil)** | **rung 无效**: 软件自身 null-deref bug, 臂无关; 同作业 v2F 锚仍 byte-6 崩 (0xa1aaaaf9513400) + v2C1 锚净 — 网格轴 **floor = V2 (160 120 120)**, v2dd 作废 |
| V3 nstates | + NSTATES 640→320 | 5×rc=65-67 (32-35s, 非崩溃) | 5×rc=65-66 (37-38s) | **rung 无效**: DSYGVD (m=320) 非 SPD/非收敛 — 320 态低于数值下限 (TOL_PSEUDOCHARGE 超限); NSTATES 640 必要 |
| V4 nt18 | NT 36→18 重编译 (rankfile 同步) | 待跑 | 待跑 | — |
| 后续 | NT 递减 9→4→2→1; NCOMMS 16→8; 分段缩减 | — | — | — |

V2 作业 (1715007) 内 v1F_anchor_1 亦崩 (rc=139, 0xad_4001c6b8aa80) — 前驱锚在同一作业内确认基线仍活。

## 4. 最小化期间崩溃签名汇总 (12 例, 全部 byte-6 同故障)

| run | si_addr | 垃圾 byte6 | 低 48 区域 |
|-----|---------|-----------|-----------|
| origF_anchor_1 | 0x444001c85d3180 | 0x44 | mmap |
| origF_anchor_2 | 0x61aaab06a006c0 | 0x61 | heap |
| v1_F_1 | 0x774001b8ce2d48 | 0x77 | mmap |
| v1_F_2 | 0xecaaaaf922b580 | 0xec | heap |
| v1_F_3 | 0x9aaaaaf92aad80 | 0x9a | heap |
| v1_F_4 | 0xb04001c74e8700 | 0xb0 | mmap |
| v1_F_5 | 0x4940011d9c8380 | 0x49 | mmap |
| v1F_anchor_1 (V2 作业) | 0xad4001c6b8aa80 | 0xad | mmap |
| v2_F_1 | 0x45aaaadae65740 | 0x45 | heap |
| v2_F_2 | 0x8daaaadffc6e00 | 0x8d | heap |
| v2_F_3 | 0xa5aaab072fb3c0 | 0xa5 | heap |
| v2_F_4 | 0x9140015c5f1f80 | 0x91 | mmap |
| v2_F_5 | 0xc440013e701680 | 0xc4 | mmap |

垃圾值 13 例全异、无重复模式 (非 stuck-at), 与历史 17 例特征一致 → 最小化复现的是同一故障, 不是新崩溃模式。
频率合规 (两层语义): 归因核 cpu139/140 — 33/33 已核窗全 ≥1997 MHz (归因有效); results.tsv 聚合判决 GREY 系 rankfile 576 pin 核中 ~10-15 个非故障核负载下被钳 1501-1744 MHz (F/C1 两臂同集合同水平 = 受控常量)。用户红线 <1500: 阶梯期间零触发; 历史触发 (cpu304/240/497, cpu113/285/414) 已在 cpu-results.csv 按 runs_freq_excluded/mixed_freq_clean 记账。

## 5. 最终最小复现物 (Step 3, 待阶梯收敛后回填)

- [ ] minimal-reproducer/ 目录: README + 自包含输入 + 验证脚本 + 绑定 + 复现率
- [ ] 复现率声明 + 与原版签名一致性核对

## 6. 过程日志 (诚实披露)

- 2026-09-24 ~09:3x: awk 最小值 reference-creates-element 缺陷自捕 (min=0 伪值), 当场改 guard-first 模式重算; 已记 status.md
- 2026-09-24 10:0x: "Failing at address" 伪值澄清 — 今日 7 例初看疑无 byte-6 签名, 实为读取了 OpenMPI handler 伪地址行; 改取 "Caught signal 11" 行后 7/7 签名成立 (analyze.sh 同步修正 + 注释)
- 2026-09-24 12:2x: V3 双臂全 rc=65-67 应用层失败 (dsygvd_upper m=320 非 SPD) — NSTATES 640 必要, 轴关闭; V4-NT18 (job 1716728, v2 基底 + 前驱锚 + 构建守卫) 提交。
- 2026-09-24 12:1x: v2d 双臂全 (nil) 崩 — 首次 C1 臂崩, 判为输入尺寸 bug 界面而非 SDC (锚对照排除); 期间发现 results.tsv 聚合判决全为 GREY 的语义 (rankfile 全 pin 核取 min, 非故障核钳频), 33/33 窗 cpu139/140 全频复核后措辞修正为两层语义。
- 2026-09-24 ~10:2x: results.md V2 行曾预写 "0/5 C1" 结论 (当时仅锚完成), 1 分钟内自捕改回 "跑着"; 记录于 status.md
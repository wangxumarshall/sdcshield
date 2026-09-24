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
- 执行顺序按可行性重排: MAXIT_SCF → FD_GRID → NSTATES → NT 重编译递减 → NCOMMS → 分段缩减; 累进制 (V2 = V1 + 网格缩减)。轴进度 (09-24 15:23): MAXIT_SCF 中性 (V1) / FD_GRID 中性且 floor=V2 (v2d 无效 rung) / NSTATES 必要=640 (V3 无效 rung) / NT 中性且收敛于 9 (V4+V4B) / NCOMMS 进行中 (v5, job 1717908) / 分段缩减待
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
| V4 nt18 | NT 36→18 重编译 (rankfile 同步; binhash 6b336a92141c1e54) | 5 (67-98s, 全 rank3, byte-6 签名 5/5, 全 libomp 簇) | 0 (325-328s; SCF error 五 rep 逐位一致且同 NT36 值) | **NT 缩减中性**: 锚对照 (v2F 崩 0xa1_aaaad0977440 / v2C1 净 354s) + 逐窗 139/140 1999-2000MHz; 重编译不改变数值路径 |
| V4B nt9 | NT 18→9 重编译 (rf_nt9_F rank3=131-139 thread8→cpu139 / C1=131-138,140; binhash 763c6843f504e1f7; job 1717122 13:28-15:19) | 5 (141-264s, 全 rank3): byte-6 2 (F_3 0x39_aaaaf47d0400 线程 15/15@139, F_5 0xea_aaaaf05c0880 10/10@139 — 首次线程级崩溃归因) + canonical 2 (F_1 0x4000091a0300 @laplacian_4d_impl<16,false>, F_4 0x400053fff480 只读页 @tridiagonal_ql_640_parallel) + 数值SDC 1 (F_2 rc=233 worker=8=cpu139 d[0] 低8位 xor=0xe1/4bit/33ulp, 8 核共识 — 首次非崩溃数值 SDC 捕获) | 0 (808-813s ×5, 0 LVTX; SCF 1.822e-01/1.518e-01/1.856e-02 同 NT36/NT18 逐位) | **NT 18→9 中性 + 检测几何**: tile=64 限 replica worker ≤10, NT36/18 时 cpu139 线程号 25/17 未入校验递推, NT9 时=worker8 首次参与 → 偏差直接可见; 锚 v2F 崩 0xa1_aaaadfef1140 / v2C1 净 356s; **NT 轴收敛于 9** |
| v5 ncomms | NCOMMS 2 2 4→2 2 2 (16→8 ranks; NT9 基座; 新 rank3 (1,1,0) ni 5-9/nj 4-7/nk 0-3 ⊇ 旧 rank3, 元素 40→80) | attempt-1 (job 1717908): ×5 全 rc=1 各 ~5s — **基础设施失败** (runmin.sh 硬编码 -np 16 vs 8 行 rankfile → "Rank: 8 is missing its location specification"); v5b (job 1718129, 15:48): canary 守卫 + NPROCS=8, 跑中 | attempt-1 ×5 同因 rc=1; v5b 跑中 (TMO 2400) | **最后轴**: NT 轴已收敛 (V4B); attempt-1 锚 2/2 有效 (v4bnt9F 257s 崩 0x18_aaaae85fad40 byte-6 / v4bnt9C1 净 818s SCF 1.856e-02 同基座) — V4B 基座仍复现; 修复: runmin.sh -np 参数化 ${NPROCS:-16}; 深度 4/2/1 不下探 |

V2 作业 (1715007) 内 v1F_anchor_1 亦崩 (rc=139, 0xad_4001c6b8aa80) — 前驱锚在同一作业内确认基线仍活。

## 4. 最小化期间崩溃签名汇总 (26 例 = 25 崩溃 + 1 数值 SDC 捕获; 崩溃含 byte-6 23 / canonical 野指针 2; 含 V3/V4/V4B 作业内 v2F 锚 3 例)

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
| v2F_anchor_1 (V3 作业) | 0x98aaaae78eee40 | 0x98 | heap |
| v2F_anchor_1 (V4 作业) | 0xa1aaaad0977440 | 0xa1 | heap |
| v4nt18_F_1 | 0x94aaaab1444700 | 0x94 | heap |
| v4nt18_F_2 | 0x140010517d400 | 0x01 (13 位前导零形态, 原记录 0x14 系误读; byte5-4=0x4001) | mmap |
| v4nt18_F_3 | 0x624000ebc45540 | 0x62 | mmap |
| v4nt18_F_4 | 0x76aaab28492d00 | 0x76 | heap |
| v4nt18_F_5 | 0x76aaab0c389080 | 0x76 | heap |
| v2F_anchor_1 (V4B 作业) | 0xa1aaaadfef1140 | 0xa1 | heap |
| v4bnt9_F_1 | 0x4000091a0300 | 0x00 — canonical 48 位 VA 非 byte-6 | 只读/映射区 (@laplacian_4d_impl<16,false> OMP outlined, stencil 家族首次最小化复现) |
| v4bnt9_F_2 | (非崩溃 rc=233) | — 数值 SDC: REPLICA_MISMATCH worker=8=cpu139, d[0] 低 8 位 xor=0xe1 / 4 bit / 33 ulp | @tridiagonal_ql_640_parallel 副本校验 (数值, 非指针) |
| v4bnt9_F_3 | 0x39aaaaf47d0400 | 0x39 | heap (libomp 簇; 崩溃线程 15/15@cpu139 — 首次线程级归因) |
| v4bnt9_F_4 | 0x400053fff480 | 0x00 — canonical 非 byte-6 | 只读页 (@tridiagonal_ql_640_parallel OMP outlined ← tridiagonal_ql_rows, 崩于副本 QL 内) |
| v4bnt9_F_5 | 0xeaaaaaf05c0880 | 0xea | heap (libomp 簇; 崩溃线程 10/10@cpu139) |

垃圾值 20 例全异、无重复模式 (非 stuck-at), 与历史 17 例特征一致 → 最小化复现的是同一故障, 不是新崩溃模式。results.md 的崩溃点分布表计 24 例 (23 崩溃 + 1 数值捕获; 不含 V3 作业 v2F 锚 — 地址已验 byte-6 但代码偏移未提取)。NT9 作业扩展表现谱: byte-6 堆指针 (F_3/F_5, 崩溃线程 100% pin cpu139) + canonical 只读页野指针 (F_1/F_4, 崩点均在计算热区 stencil/QL) + 计算数据低位翻转 (F_2, 33 ulp) — 同一弱单元的受害位平面随数据布局而异, 指针损坏与数据损坏并存, 强化微架构诊断。
频率合规 (两层语义): 归因核 cpu139/140 — 33/33 已核窗全 ≥1997 MHz (归因有效); results.tsv 聚合判决 GREY 系 rankfile 576 pin 核中 ~10-15 个非故障核负载下被钳 1501-1744 MHz (F/C1 两臂同集合同水平 = 受控常量)。用户红线 <1500: 阶梯期间 (08:45 后) 零触发; 历史触发 (cpu304/240/497, cpu113/285/414) 已在 cpu-results.csv 按 runs_freq_excluded/mixed_freq_clean 记账; V4 作业内新增 cpu17 (min 1485) / cpu42 (min 1487) 越线 (rank0 非归因核, 记录在案不研究, 归因核 139/140 全程 1999-2000); V4B (NT9) 作业内新增 cpu487 (min 1451) / cpu510 (min 1494) 越线 (非归因核, 记录在案不研究; 归因核 139 全 12 窗 1997-2000 / 140 1998-2000); v5 attempt-1 作业窗新增 cpu58 (min 1481)。

**检测几何 (detection geometry) — F_2 数值捕获与 F_4 崩点为何首次在 NT9 出现**: LVTX DSYGVD `tridiagonal_ql_640_parallel` (lvtx_kml_dsygvd_detail.hpp:571-660) 令每个 OMP worker 在 memcpy 而来的相同 d/e 输入上跑**逐位相同**的私有标量递推 (热循环无同步), 并行区结束后 worker 0 的权威 d/e 与各 worker 私有态**逐字节 memcmp**, 任何差异 → LVTX_DSYGVD_REPLICA_MISMATCH → kStatusReplicaMismatch → MPI_ABORT(1001) → rc=233。但 replica worker 数 = min(threads, ceil(640/tile)), 而 `production_tile=64` (lvtx_dsygvd_internal_config.hpp:9) → **worker 上限 10**: NT36 时 cpu139=线程 25, NT18 时=线程 17, 从未进入被校验的副本递推; NT9 时 rank3=131-139 → cpu139=**worker 8, 首次参与** → 其数值偏差首次被直接捕获 (F_2), 同函数内部崩溃亦首次出现 (F_4)。故 NT9 的表现谱变化 = **检测几何变化, 非故障性质变化**。非软件伪象的三重证据: worker=8 的报告顺序意味着 worker 1-7 (cpu132-138) 已先通过各自全部 1280 项 memcmp (8:1 核间 bit-identical 共识); 同几何健康对照 (C1, cpu140=worker8) 5/5 全净; 校验逻辑确定性 (同输入同递推, 唯一自由度是执行核)。

## 5. 最终最小复现物 (Step 3, 待阶梯收敛后回填)

- [ ] minimal-reproducer/ 目录: README + 自包含输入 + 验证脚本 + 绑定 + 复现率
- [ ] 复现率声明 + 与原版签名一致性核对

## 6. 过程日志 (诚实披露)

- 2026-09-24 ~09:3x: awk 最小值 reference-creates-element 缺陷自捕 (min=0 伪值), 当场改 guard-first 模式重算; 已记 status.md
- 2026-09-24 10:0x: "Failing at address" 伪值澄清 — 今日 7 例初看疑无 byte-6 签名, 实为读取了 OpenMPI handler 伪地址行; 改取 "Caught signal 11" 行后 7/7 签名成立 (analyze.sh 同步修正 + 注释)
- 2026-09-24 12:2x: V3 双臂全 rc=65-67 应用层失败 (dsygvd_upper m=320 非 SPD) — NSTATES 640 必要, 轴关闭; V4-NT18 (job 1716728, v2 基底 + 前驱锚 + 构建守卫) 提交。
- 2026-09-24 13:27: V4-NT18 收官 (job 1716728): NT 36→18 重编译中性 — F 5/5 byte-6 (0x94/0x01/0x62/0x76/0x76, 全 rank3, 67-98s, 全 libomp 簇) / C1 5/5 净 (325-328s, SCF error 五 rep 逐位一致且同 NT36 值) / 锚对完整 / 逐窗 139/140 1999-2000。NT18 binhash 6b336a92141c1e54。红线: cpu17/42 越线 (rank0 非归因核, 记录在案)。V4B-NT9 (job 1717122) 13:28 已投; NCOMMS 8-rank 设计定稿 (源码实证: col-major rank=i+(j+k*nj)*ni, 新 rank3 (1,1,0) ⊇ 旧 rank3 区域)。
- 2026-09-24 15:52: v5 attempt-1 (job 1717908) 基础设施失败自捕: v5nc8 ×10 全 rc=1 各 ~5s — runmin.sh 硬编码 mpirun -np 16 vs rf_nc8_* 8 行, OpenMPI 启动前拒绝 ("Rank: 8 is missing its location specification"); 锚 2/2 有效不受影响 (v4bnt9F 257s 崩 byte-6 / v4bnt9C1 净 818s)。修复: runmin.sh -np → ${NPROCS:-16}; v5b (job 1718129, 15:48) 10 处 v5nc8 调用 NPROCS=8 + 锚保持 16 + 120s canary 守卫 (8×RANK_BIND + rc∈{124,139,233}), bash -n 全过。教训: 改 rank 数时必须同步审 runner 的 -np 常量; 十行 rc=1 保留 results.tsv 记账。红线: cpu58 (min 1481)。
- 2026-09-24 15:19: V4B-NT9 收官 (job 1717122): NT 18→9 重编译中性 — F 5/5 失败, 表现谱扩展: byte-6 2 (F_3/F_5, 崩溃线程 15/15 与 10/10 pin cpu139 — 首次线程级崩溃归因) + canonical 2 (F_1 @laplacian_4d_impl / F_4 @tridiagonal_ql_640_parallel) + **数值 SDC 1 (F_2 rc=233: replica worker=8=cpu139, d[0] 低 8 位 xor=0xe1 / 4 bit / 33 ulp, worker 1-7 先全过 = 8 核共识 — 战役首次非崩溃直接数值捕获)**; C1 5/5 净 (808-813s, SCF error 同 NT36/NT18 逐位); 锚 v2F 崩 0xa1_aaaadfef1140 / v2C1 净 356s; 逐窗 139=1997-2000 / 140=1998-2000。检测几何发现 (源码实证): tile=64 限 replica ≤10 worker, cpu139 仅在 NT9 (=worker8) 首次进入被校验递推 — 表现谱变化系检测几何而非故障性质变化。NT9 binhash 763c6843f504e1f7。**NT 轴收敛于 9** (深度 4/2/1 代价 ~2× C1, 边际价值低)。红线: cpu487 (min 1451) / cpu510 (min 1494) 越线 (非归因核, 记录在案)。v5-NCOMMS 2 2 4→2 2 2 (job 1717908, 15:23 已投, NT9 基座, cpu139=worker8 检测器保留) 望 21-22 时收官。
- 2026-09-24 12:1x: v2d 双臂全 (nil) 崩 — 首次 C1 臂崩, 判为输入尺寸 bug 界面而非 SDC (锚对照排除); 期间发现 results.tsv 聚合判决全为 GREY 的语义 (rankfile 全 pin 核取 min, 非故障核钳频), 33/33 窗 cpu139/140 全频复核后措辞修正为两层语义。
- 2026-09-24 ~10:2x: results.md V2 行曾预写 "0/5 C1" 结论 (当时仅锚完成), 1 分钟内自捕改回 "跑着"; 记录于 status.md
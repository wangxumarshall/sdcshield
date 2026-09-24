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
- 执行顺序按可行性重排: MAXIT_SCF → FD_GRID → NSTATES → NT 重编译递减 → NCOMMS → 分段缩减; 累进制 (V2 = V1 + 网格缩减)。轴进度 (09-24 18:15): MAXIT_SCF 中性 (V1) / FD_GRID 中性且 floor=V2 (v2d 无效 rung) / NSTATES 必要=640 (V3 无效 rung) / NT 中性且收敛于 9 (V4+V4B) / NCOMMS 16 = fabric 下限 (v5 attempt-1 rc=1 -np 失配; v5b 10/10 rc=134 传输死锁 — INFRA-INVALID) / 分段缩减: v6a nk 8→4 中性 (F 侧 7/7 cpu139 崩 / C1 侧 6/6 净), v6b nj 8→4 中性 (F 侧 7/7 cpu139 崩 / C1 侧 6/6 净), v6c ni 10→8 中性 (空间 x 减元素数 128 元素, F 侧 7/7 cpu139 崩 / C1 侧 6/6 净), v6d 终点 2 2 4 中性收官 (16 元素 1/rank 真·最小形态, F 侧 7/7 cpu139 崩 / C1 侧 6/6 净) — NELEMS 轴在 16 元素/1-per-rank 终点关闭 (四级全中性, F 侧累计 28/28), 转 Task 10 Step 3 复现体定稿
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
| v5 ncomms | NCOMMS 2 2 4→2 2 2 (16→8 ranks; NT9 基座; 新 rank3 (1,1,0) ni 5-9/nj 4-7/nk 0-3 ⊇ 旧 rank3, 元素 40→80) | attempt-1 (job 1717908): ×5 全 rc=1 ~5s — **基础设施失败** (runmin.sh 硬编码 -np 16 vs 8 行 rankfile); v5b (job 1718129, 15:48-16:56): canary rc=124 bind=8 (NPROCS 修复实证); 锚 2/2 有效 (F 锚 rc=139 **49s** 0xab_aaaad4d41b80 byte-6 rank3 [1,3] / C1 锚 rc=0 810s 3 轮 SCF) | attempt-1 ×5 rc=1; v5b: v5nc8_F ×5 + v5nc8_C1 ×5 (rank3=131-138,140 **无 cpu139**) **10/10 全部 rc=134 ~304-306s** — 8-rank Preparation::init Geometry::bcast 传输死锁, 从未进 SCF | **INFRA-INVALID — NCOMMS 轴止于 16 (fabric 限制, 非科学结论)**: UCX/RoCE (roceroh0-3) 5120 原子大对象 bcast 部分投递 — 栈帧分歧 GEO{0,1,4,6,7}/CTL{2,3,5} F/C1 双臂逐位一致 (确定性传输拓扑故障); 卡死组不含 cpu139; rank3 超时对端与健核 rank2/5 相同 (18570b01); geometry.cpp:502 纯直线 2×MPI_Bcast 无 rank 条件分支 (排除应用层逻辑分歧); ~305s=UCX UD 默认超时 (bcast 起步即停); 同作业 16-rank 锚秒级过 init 且 F 锚 49s 复现 byte-6 — 与 SDC 无关证据闭环; 频率全 VALID (min 1996), 本窗零红线; 转入分段轴 v6 (16-rank 重设计) |
| v6a nelems-z | NELEMS 10 8 8→10 8 4 @ NCOMMS 2 2 4 (CELL z 82.296846784→41.148423392, FD_GRID 160 120 60, Si.ion 5120→2562 原子, 元素 640→320, rank3 40→20 ⊂ 原 40, 每元素网格 16×15×15 不变; job 1720115 17:02-18:09) | canary 46s + F_1-F_5 39-171s + F 锚 256s = **7/7 崩, 全 rank3, 崩溃线程 psr 7/7 全 cpu139**: byte-6 heap 4 (0x40/0x0a/0x06/0x33) + **byte-6 @0x4000-region 2 (F_3 0xb3 / F 锚 0x9e — 损坏不限于 heap, 0x4000 mmap 区亦中招 = 地址生成路径损坏)** + canonical 1 (F_2 0x400043fff400 ACCERR); 崩溃点 libomp 顶层簇 4/7 (F_1 栈: libucs 信号帧→libomp+0x80030/+0x5eb98) + swap37 簇 2/7 (F 锚/F_3 @a.out+0x139ac4) + QL 簇 1/7 (F_2 @a.out+0x2d0ce4) [19:12 全量回溯修正: 原判读仅取 F_1 样本误作全量], SCF loop 1 内, init 7/7 过 | C1 锚 811s + 5 rep 434-437s = **6/6 净** (rc=0, scf error 6.429e-01/4.748e-01/2.320e-01 五 rep 一致, LVTX replica 0 失配) | **空间 z 减半中性 — SDC 复现与空间规模解耦** (元素 640→320, 原子 5120→2562, 每元素负载逐位不变, 复现率不降); 频率 F 侧 7 窗 cpu139=1999 合规, 聚合 EXCLUDED 系 cpu108 (1493, rank2 slot) 污染按逐核分解规则判读; 全精度 E3 非确定性更正 (见进程日志 18:15); 转 v6b nj 8→4 |
| v6b nelems-y | NELEMS 10 4 4 @ NCOMMS 2 2 4 (CELL y 82.296846784→41.148423392, FD_GRID 160 60 60, Si.ion 2562→1277 原子, 元素 320→160, rank3 20→10 = ni 5-9/nj 2-3/nk 0, 每元素网格 16×15×15 不变; job 1720586 18:11-19:04) | canary 55s + F_1-F_5 84-166s + F 锚 39s = **7/7 崩, 全 rank3, 崩溃线程 psr 7/7 全 cpu139 (psr.tsv 尾采样口径; F_1 实例线程先 cpu135 后迁 139)**: byte-6 heap 1 (F 锚 0xac) + **byte-6 @0xaaab 高址区 3 (canary 0xe6 / F_2 0xa9 / F_4 0xee — heap 0xaaaa 相邻区, byte-6 目标区扩至三个: 0xaaaa / 0xaaab / 0x4000)** + canonical 3 (F_1 0x3ffbae51bf00 / F_5 0x3ff9cdce47c0 野指针 MAPERR + F_3 0x40004bfff840 @0x4000 区 SEGV_ACCERR; canonical 占比 3/7 vs v6a 1/7 谱形漂移记录在案); 崩溃点 libomp 顶层簇 4 + stencil 簇 2 (F_1/F_5 @+0x258f00) + QL 簇 1 (F_3 @+0x2d0ce4) | C1 锚 814s (v2 输入) + 5 rep 319-323s = **6/6 净** (rc=0, scf error 7.163e-01/5.337e-01/2.692e-01 五 rep 4 位打印一致, LVTX replica 0 失配, E3 展幅 1.78e-7) | **空间 y 减半中性 — SDC 复现与空间规模继续解耦** (元素 320→160, 原子 2562→1277, 每元素负载逐位不变, 复现率不降); 频率 cpu139/140=1999 全 12 窗合规, 聚合 EXCLUDED 系 cpu236/39 整窗 1459 (dip 起 18:11:05=job 启动, 负载相关降频, rank6/rank1 槽位核非归因) 按逐核分解规则判读; 转 v6c ni 10→8 |
| v6c nelems-x | NELEMS 8 4 4 @ NCOMMS 2 2 4 (CELL x 102.87105848→82.296846784 — v2 源 CELL 非立方 102.87105848×82.296846784×82.296846784, 元素尺寸 10.287105848³ 不变且元素边界线 0-82.296846784 与 v2 重合, FD_GRID 160→128, Si.ion 1277→1023 原子 [x<82.296846784 丢 254 = 恰为元素 ni 8-9 的原子, kept-maxX 82.28952 间隙干净], 元素 160→128, rank3 10→8 = ni 4-7/nj 2-3/nk 0, 每元素网格 16×15×15 不变; job 1721020 19:07-19:58) | canary 28s + F_1-F_5 86-167s + F 锚 41s = **7/7 崩, 全 rank3, 崩溃线程 psr 7/7 全 cpu139 (尾采样), 全 SEGV_MAPERR**: **canonical 0x3ffX 栈带 4 例全部同一崩溃点 (F_1 0x3ffa5726e000 / F_2 0x3ff971ebac00 / F_3 0x3ffd6b563340 / F_4 0x3ffd12e56b80, 四例全 @a.out+0x258f00 laplacian stencil OMP outlined — 同作业 4/7 同位点+同地址带 = 最小化以来最强集中; dattach /proc/self/maps 实证主栈 0xffffc1185000 / libomp-libc-ld 0x4000_2d00-3f00 区 → 0x3ff9-3ffd = libs 下方深 mmap 带 = worker pthread 栈落点, canonical "野指针" 再解释为栈带指针损坏 (推断))** + byte-6 heap 2 (canary 0x31 / F 锚 0x33, 双双 libomp 顶层簇) + byte-6 @0x4000 区 1 (F_5 0x42 @swap37 簇 a.out+0x139ac4) | C1 锚 813s (v2 输入, scf 1.822e-01/1.518e-01/1.856e-02 跨作业一致) + C1_1-5 287-290s = **6/6 净** (rc=0, scf iter3 2.945e-01 五 rep 4 位一致, 收敛前值 2.84122080-83e-01 同至 9 位, LVTX replica 0 失配, E3 -1680.94758271~-75 展幅 4.2e-8) | **空间 x 减元素数中性 — NELEMS 三维缩减 (z/y/x) F 侧累计 21/21 全崩全 cpu139 → SDC 与空间区域无关成立**; 频率全作业窗 cpu139 210/210 @1999 / cpu140 210/210 @1998, 聚合红线 cpu436 整窗 1458 (19:08-19:57 负载相关, 该核全天其余 2805 采样正常, 非归因路径) 按逐核分解规则判读; 转 v6d 终点 2 2 4 |
| v6d nelems-endpoint | NELEMS 2 2 4 @ NCOMMS 2 2 4 (16 元素 1/rank 真·最小形态, CELL 20.574211696×20.574211696×41.148423392, FD_GRID 32 30 60 = 每元素 16×15×15, Si.ion 125 原子 [v6a 源 x<20.574211696 AND y<20.574211696 过滤, kept-maxX 20.5686/maxY 20.5311 间隙干净], rank3 = 恰 1 元素 ni 1/nj 1/nk 0; job 1721399 20:01-20:47) | canary 67s + F_1-F_5 74-151s + F 锚 187s = **7/7 崩, 全 rank3, 崩溃线程 psr 7/7 全 cpu139, 全 SEGV_MAPERR**: **canonical 0x3ffX 栈带 5 例全部 @stencil a.out+0x258f00 (canary 0x3ffd6c507780 / F 锚 0x3ff98191e000 / F_1 0x3ff9f18e8c80 / F_2 0x3ffe6177b640 / F_5 0x3ffb95445400 — 5/7 同位点+同地址带 = 集中达峰超 v6c 4/7, 0x3ffe 首次入带)** + byte-6 heap 1 (F_3 0x63 @libomp 顶层簇) + byte-6 @0x4000 区 1 (F_4 0x7c @swap37 簇 a.out+0x139ac4) | C1 锚 813s (v2 输入, scf 1.822e-01/1.518e-01/1.856e-02 三作业连续一致) + C1_1-5 196-200s = **6/6 净** (rc=0, scf 7.796e-01/5.784e-01/3.103e-01 五 rep 一致, 末值 4.728122e-03 逐位同, LVTX replica 0 失配, E3 -206.7261637648xx 五 rep 展幅 2.2e-14) | **终点中性 — NELEMS 轴关闭 (v6a z 320 / v6b y 160 / v6c x 128 / v6d 16 四级全中性, F 侧累计 28/28 全崩全 cpu139; 元素 640→16 = 1/40, 原子 5120→125, SDC 与问题规模完全解耦)**; 频率全作业窗 cpu139 191/191 @1999 / cpu140 191/191 @1998, 聚合红线 cpu558 整窗 1391 (20:01:37 起 = job 启动时刻, 负载相关, NUMA14, 非归因路径) 按逐核分解规则判读; 转 Task 10 Step 3 复现体定稿 |

V2 作业 (1715007) 内 v1F_anchor_1 亦崩 (rc=139, 0xad_4001c6b8aa80) — 前驱锚在同一作业内确认基线仍活。

## 4. 最小化期间崩溃签名汇总 (56 例 = 55 崩溃 + 1 数值 SDC 捕获; 崩溃含 byte-6 40 / canonical 野指针 15; 含 V3/V4/V4B/v5-attempt-1/v5b/v6a/v6b/v6c/v6d 作业内 16-rank 锚 9 例; byte-6 损坏横跨 heap 0xaaaa / 0xaaab 高址 / 0x4000 mmap 三区 = 地址生成路径损坏; canonical 0x3ff9-3ffd 带 = worker pthread 栈落点 (dattach /proc/self/maps 实证 — 栈带指针损坏再解释, 推断))

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
| v4bnt9F_anchor_1 (v5 attempt-1 作业) | 0x18aaaae85fad40 | 0x18 | heap (16-rank 锚, 257s, byte-6 lane6+5-4, rank3 [1,3]) |
| v4bnt9F_anchor_1 (v5b 作业) | 0xabaaaad4d41b80 | 0xab | heap (16-rank 锚, 49s, byte-6, rank3 [1,3]) |
| v6a_canary | 0x40aaaaf9db9900 | 0x40 | heap (46s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v4bnt9F_anchor_1 (v6a 作业) | 0x9e4000bc066ec8 | 0x9e | 0x4000-region (16-rank 锚, 256s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6a_F_1 | 0xaaaaaf85eca00 | 0x0a | heap (64s, byte-6 前导零形态, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6a_F_2 | 0x400043fff400 | (canonical) | 0x4000-region canonical (154s, SEGV_ACCERR 权限错, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6a_F_3 | 0xb34000dc92cec8 | 0xb3 | 0x4000-region (171s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6a_F_4 | 0x6aaaac2105a40 | 0x06 | heap (143s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6a_F_5 | 0x33aaaadde31a40 | 0x33 | heap (39s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6b_canary | 0xe6aaab16399200 | 0xe6 | 0xaaab 高址区 (55s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v4bnt9F_anchor_1 (v6b 作业) | 0xacaaaaf63ecc40 | 0xac | heap (16-rank 锚, 39s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6b_F_1 | 0x3ffbae51bf00 | (canonical) | 野指针 MAPERR (107s, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6b_F_2 | 0xa9aaab143ea000 | 0xa9 | 0xaaab 高址区 (84s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6b_F_3 | 0x40004bfff840 | (canonical) | 0x4000-region canonical (166s, SEGV_ACCERR 权限错, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6b_F_4 | 0xeeaaab0d228680 | 0xee | 0xaaab 高址区 (148s, byte-6, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6b_F_5 | 0x3ff9cdce47c0 | (canonical) | 野指针 MAPERR (90s, rank3 [1,3], 崩溃线程 psr=cpu139) |
| v6c_canary | 0x31aaaaf5d3f400 | 0x31 | byte-6 heap 0xaaaa (28s, rank3 [1,3], 崩溃线程 psr=cpu139, libomp 顶层簇) |
| v4bnt9F_anchor_1 (v6c 作业) | 0x33aaaafb046600 | 0x33 | byte-6 heap 0xaaaa (41s, rank3 [1,3], 崩溃线程 psr=cpu139, libomp 顶层簇) |
| v6c_F_1 | 0x3ffa5726e000 | (canonical) | 野指针 MAPERR (104s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v6c_F_2 | 0x3ff971ebac00 | (canonical) | 野指针 MAPERR (107s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v6c_F_3 | 0x3ffd6b563340 | (canonical) | 野指针 MAPERR (86s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v6c_F_4 | 0x3ffd12e56b80 | (canonical) | 野指针 MAPERR (90s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v6c_F_5 | 0x424000a8862e88 | 0x42 | byte-6 @0x4000 区 (167s, rank3 [1,3], 崩溃线程 psr=cpu139, @swap37 簇 a.out+0x139ac4) |
| v6d_canary | 0x3ffd6c507780 | (canonical) | 野指针 MAPERR (67s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v4bnt9F_anchor_1 (v6d 作业) | 0x3ff98191e000 | (canonical) | 野指针 MAPERR (187s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v6d_F_1 | 0x3ff9f18e8c80 | (canonical) | 野指针 MAPERR (83s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |
| v6d_F_2 | 0x3ffe6177b640 | (canonical) | 野指针 MAPERR (74s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带, 0x3ffe 首次入带) |
| v6d_F_3 | 0x63aaaae0e00480 | 0x63 | byte-6 heap 0xaaaa (151s, rank3 [1,3], 崩溃线程 psr=cpu139, libomp 顶层簇) |
| v6d_F_4 | 0x7c4000af5267c8 | 0x7c | byte-6 @0x4000 区 (149s, rank3 [1,3], 崩溃线程 psr=cpu139, @swap37 簇 a.out+0x139ac4) |
| v6d_F_5 | 0x3ffb95445400 | (canonical) | 野指针 MAPERR (94s, rank3 [1,3], 崩溃线程 psr=cpu139, @stencil a.out+0x258f00, 0x3ffX 栈带) |

垃圾值 29 例全异、无重复模式 (非 stuck-at; 后续 V4 期 F_4/F_5 曾同出 0x76 一次 — 随机碰撞, 非重复模式), 与历史 17 例特征一致 → 最小化复现的是同一故障, 不是新崩溃模式。results.md 的崩溃点分布表计 24 例 (23 崩溃 + 1 数值捕获; 不含 V3 作业 v2F 锚 — 地址已验 byte-6 但代码偏移未提取)。NT9 作业扩展表现谱: byte-6 堆指针 (F_3/F_5, 崩溃线程 100% pin cpu139) + canonical 只读页野指针 (F_1/F_4, 崩点均在计算热区 stencil/QL) + 计算数据低位翻转 (F_2, 33 ulp) — 同一弱单元的受害位平面随数据布局而异, 指针损坏与数据损坏并存, 强化微架构诊断。
频率合规 (两层语义): 归因核 cpu139/140 — 33/33 已核窗全 ≥1997 MHz (归因有效); results.tsv 聚合判决 GREY 系 rankfile 576 pin 核中 ~10-15 个非故障核负载下被钳 1501-1744 MHz (F/C1 两臂同集合同水平 = 受控常量)。用户红线 <1500: 阶梯期间 (08:45 后) 零触发; 历史触发 (cpu304/240/497, cpu113/285/414) 已在 cpu-results.csv 按 runs_freq_excluded/mixed_freq_clean 记账; V4 作业内新增 cpu17 (min 1485) / cpu42 (min 1487) 越线 (rank0 非归因核, 记录在案不研究, 归因核 139/140 全程 1999-2000); V4B (NT9) 作业内新增 cpu487 (min 1451) / cpu510 (min 1494) 越线 (非归因核, 记录在案不研究; 归因核 139 全 12 窗 1997-2000 / 140 1998-2000); v5 attempt-1 作业窗新增 cpu58 (min 1481)。

**检测几何 (detection geometry) — F_2 数值捕获与 F_4 崩点为何首次在 NT9 出现**: LVTX DSYGVD `tridiagonal_ql_640_parallel` (lvtx_kml_dsygvd_detail.hpp:571-660) 令每个 OMP worker 在 memcpy 而来的相同 d/e 输入上跑**逐位相同**的私有标量递推 (热循环无同步), 并行区结束后 worker 0 的权威 d/e 与各 worker 私有态**逐字节 memcmp**, 任何差异 → LVTX_DSYGVD_REPLICA_MISMATCH → kStatusReplicaMismatch → MPI_ABORT(1001) → rc=233。但 replica worker 数 = min(threads, ceil(640/tile)), 而 `production_tile=64` (lvtx_dsygvd_internal_config.hpp:9) → **worker 上限 10**: NT36 时 cpu139=线程 25, NT18 时=线程 17, 从未进入被校验的副本递推; NT9 时 rank3=131-139 → cpu139=**worker 8, 首次参与** → 其数值偏差首次被直接捕获 (F_2), 同函数内部崩溃亦首次出现 (F_4)。故 NT9 的表现谱变化 = **检测几何变化, 非故障性质变化**。非软件伪象的三重证据: worker=8 的报告顺序意味着 worker 1-7 (cpu132-138) 已先通过各自全部 1280 项 memcmp (8:1 核间 bit-identical 共识); 同几何健康对照 (C1, cpu140=worker8) 5/5 全净; 校验逻辑确定性 (同输入同递推, 唯一自由度是执行核)。

## 5. 最终最小复现物 (Step 3, 2026-09-24 21:20 定稿)

- [x] minimal-reproducer/ 目录: README + 自包含输入 + 验证脚本 + 绑定 + 复现率
- [x] 复现率声明 + 与原版签名一致性核对

**$RES/minimal-reproducer/** — v6d 终点形态自含包 (16 元素 / 1 元素每 rank / 125 原子, NT9 二进制随包 binhash 763c6843f504e1f7):

- 文件: `a.out` + `lib/libomp.so` + `Si.inpt`/`Si.ion`(125 原子)/`Si.psp8` + `rf_nc16_F.txt`(rank3=131-139) / `rf_nc16_C1.txt`(131-138,140, 线程 8 落点唯一差异) + `reproduce.sh`(单次运行器, runmin 逐字同款 mpirun harness + 5s psr/cpu139/140 频率采样) + `verify.sh`(证据分类器: rc + rank3 + 崩溃线程尾采样 psr=139 + 地址分类 + 归属核频率门限 + C1 物理金值) + `job_repro.sh`(#DSUB 一键 freqmon + F×3 + C1×1 + 判定表) + `freqmon.sh` + `README`(sha16 清单/构建自证/运行/判定/预期/复现率/自检清单/诚实声明)
- 端到端自证 (job 1721859, 21:08-21:17, 仅用包内文件): **F×3 3/3 REPRO_CRASH_ATTRIBUTED** (79.6s canonical_3ffc @stencil+0x258f00 / 150.9s byte6-lib_b2 @swap37+0x139ac4 / 74.4s canonical_3ffa @stencil+0x258f00; 全 rank3 [1,3], psr=139, cpu139 min=1999 VALID) + **C1×1 CLEAN_CONTROL_PASS** (196.6s rc=0, scf 7.796e-01/5.784e-01/3.103e-01 三迭代金值, E3=-206.726163764884, cpu140 min=1999 VALID); 四窗口零 <1500 红线核; 判定表 runs/verify_table.txt
- 与原版签名一致性核对通过: 地址类同家族 (canonical 0x3ffX 栈带 ×2 + byte-6 0x40xx ×1 — v6d 谱形) / 崩溃点同家族 (stencil +0x258f00 ×2 + swap37 +0x139ac4 ×1 — v6d 主/次簇) / 归因不变量同 (rank3 + psr139 + 归属核 ≥1800) / 时延同带 (74-151s ⊂ 67-187s)
- 复现率声明: 本包形态 F 侧 **10/10** (v6d 7/7 + 验证作业 3/3), C1 侧 **7/7** 干净; NELEMS 四级阶梯 F 侧累计 **28/28**; 单运行崩溃时延 67-187s (中位 ~94s)

## 6. 过程日志 (诚实披露)

- 2026-09-24 ~09:3x: awk 最小值 reference-creates-element 缺陷自捕 (min=0 伪值), 当场改 guard-first 模式重算; 已记 status.md
- 2026-09-24 10:0x: "Failing at address" 伪值澄清 — 今日 7 例初看疑无 byte-6 签名, 实为读取了 OpenMPI handler 伪地址行; 改取 "Caught signal 11" 行后 7/7 签名成立 (analyze.sh 同步修正 + 注释)
- 2026-09-24 12:2x: V3 双臂全 rc=65-67 应用层失败 (dsygvd_upper m=320 非 SPD) — NSTATES 640 必要, 轴关闭; V4-NT18 (job 1716728, v2 基底 + 前驱锚 + 构建守卫) 提交。
- 2026-09-24 13:27: V4-NT18 收官 (job 1716728): NT 36→18 重编译中性 — F 5/5 byte-6 (0x94/0x01/0x62/0x76/0x76, 全 rank3, 67-98s, 全 libomp 簇) / C1 5/5 净 (325-328s, SCF error 五 rep 逐位一致且同 NT36 值) / 锚对完整 / 逐窗 139/140 1999-2000。NT18 binhash 6b336a92141c1e54。红线: cpu17/42 越线 (rank0 非归因核, 记录在案)。V4B-NT9 (job 1717122) 13:28 已投; NCOMMS 8-rank 设计定稿 (源码实证: col-major rank=i+(j+k*nj)*ni, 新 rank3 (1,1,0) ⊇ 旧 rank3 区域)。
- 2026-09-24 15:52: v5 attempt-1 (job 1717908) 基础设施失败自捕: v5nc8 ×10 全 rc=1 各 ~5s — runmin.sh 硬编码 mpirun -np 16 vs rf_nc8_* 8 行, OpenMPI 启动前拒绝 ("Rank: 8 is missing its location specification"); 锚 2/2 有效不受影响 (v4bnt9F 257s 崩 byte-6 / v4bnt9C1 净 818s)。修复: runmin.sh -np → ${NPROCS:-16}; v5b (job 1718129, 15:48) 10 处 v5nc8 调用 NPROCS=8 + 锚保持 16 + 120s canary 守卫 (8×RANK_BIND + rc∈{124,139,233}), bash -n 全过。教训: 改 rank 数时必须同步审 runner 的 -np 常量; 十行 rc=1 保留 results.tsv 记账。红线: cpu58 (min 1481)。
- 2026-09-24 16:57: v5b 收官 (job 1718129) — **INFRA-INVALID, NCOMMS 轴止于 16**: v5nc8_F ×5 + v5nc8_C1 ×5 (无 cpu139) 10/10 rc=134 ~305s, Preparation::init Geometry::bcast (5120 原子) UCX/RoCE 部分投递死锁 — 栈帧分歧 GEO{0,1,4,6,7}/CTL{2,3,5} 双臂逐位一致, 卡死组不含 cpu139, geometry.cpp:502 纯直线 bcast (排除逻辑分歧), ~305s=UCX UD 超时, 同作业 16-rank 锚秒级过 init + F 锚 49s byte-6 复现 — 与 SDC 无关证据闭环; canary bind=8 修复实证; 频率全 VALID, 本窗零红线。v6 全梯改 NCOMMS 2 2 4 (16-rank): v6a' NELEMS 10 8 4 工件就绪 (vs v2 diff 恰 CELL z/FD_GRID z/NELEMS nk), 待 runmin.sh ION 参数化后提交 (v5b 运行中不可改 runner)。
- 2026-09-24 15:19: V4B-NT9 收官 (job 1717122): NT 18→9 重编译中性 — F 5/5 失败, 表现谱扩展: byte-6 2 (F_3/F_5, 崩溃线程 15/15 与 10/10 pin cpu139 — 首次线程级崩溃归因) + canonical 2 (F_1 @laplacian_4d_impl / F_4 @tridiagonal_ql_640_parallel) + **数值 SDC 1 (F_2 rc=233: replica worker=8=cpu139, d[0] 低 8 位 xor=0xe1 / 4 bit / 33 ulp, worker 1-7 先全过 = 8 核共识 — 战役首次非崩溃直接数值捕获)**; C1 5/5 净 (808-813s, SCF error 同 NT36/NT18 逐位); 锚 v2F 崩 0xa1_aaaadfef1140 / v2C1 净 356s; 逐窗 139=1997-2000 / 140=1998-2000。检测几何发现 (源码实证): tile=64 限 replica ≤10 worker, cpu139 仅在 NT9 (=worker8) 首次进入被校验递推 — 表现谱变化系检测几何而非故障性质变化。NT9 binhash 763c6843f504e1f7。**NT 轴收敛于 9** (深度 4/2/1 代价 ~2× C1, 边际价值低)。红线: cpu487 (min 1451) / cpu510 (min 1494) 越线 (非归因核, 记录在案)。v5-NCOMMS 2 2 4→2 2 2 (job 1717908, 15:23 已投, NT9 基座, cpu139=worker8 检测器保留) 望 21-22 时收官。
- 2026-09-24 12:1x: v2d 双臂全 (nil) 崩 — 首次 C1 臂崩, 判为输入尺寸 bug 界面而非 SDC (锚对照排除); 期间发现 results.tsv 聚合判决全为 GREY 的语义 (rankfile 全 pin 核取 min, 非故障核钳频), 33/33 窗 cpu139/140 全频复核后措辞修正为两层语义。
- 2026-09-24 ~10:2x: results.md V2 行曾预写 "0/5 C1" 结论 (当时仅锚完成), 1 分钟内自捕改回 "跑着"; 记录于 status.md
- 2026-09-24 18:15: v6a 收官 (job 1720115, 17:02-18:09) — **空间 z 减半中性**: F 侧 7/7 崩 (canary+5 rep+F 锚, 全 rank3, 崩溃线程 psr 7/7 cpu139; byte-6 heap 4 + byte-6 @0x4000-region 2 [F_3 0xb3 / 锚 0x9e — 损坏不限于 heap, 地址生成路径] + canonical 1) / C1 侧 6/6 净 (锚 811s + 5 rep 434-437s, scf error 五 rep 一致, replica 0 失配); 每元素网格 16×15×15 与负载逐位不变, 元素 640→320 原子 5120→2562 — 复现率不降; 频率 F 侧 7 窗 cpu139=1999, 聚合 EXCLUDED 系 cpu108 (1493) 按逐核分解规则排除污染; **全精度能量非确定性更正**: 首次全精度比对 E3 发现 v6a C1 五 rep 互差 ≤1.35e-6, 追溯历史 v2 输入 C1 复跑 (v4bnt9_C1/v4nt18_C1) E3 一直互差 3-9e-3 — 应用内在非确定性 (无 cpu139 亦然), 非新硬件事件; 历史 "SCF error 逐位一致" 实为 4 位打印精度比对 (成立), "重编译不改变数值路径" 弱化为 "不改变打印精度数值路径"; V4B F_2 数值捕获不受影响 (LVTX replica 逐字节 memcmp 免疫调度序), 全部梯级判定不变; v6b nj 8→4 (NELEMS 10 4 4, 160 元素, 1277 原子) 已提交 (job 1720586, 18:11)
- 2026-09-24 19:05: v6b 收官 (job 1720586, 18:11-19:04) — **空间 y 减半中性**: F 侧 7/7 崩 (canary+5 rep+F 锚, 全 rank3, psr 7/7 cpu139; byte-6 heap 1 + byte-6 @0xaaab 高址区 3 + canonical 3); C1 侧 6/6 净 (v2 锚 814s + 5 rep 319-323s, scf 4 位打印一致, E3 展幅 1.78e-7)。byte-6 目标区扩至三区 (0xaaaa/0xaaab/0x4000)。同场自捕修正: (a) v6a 判读 "崩溃点 libomp 簇" 仅取 F_1 样本 — 全量回溯 7 例 = libomp 顶层 4 + swap37 簇 2 + QL 1, 已改; (b) psr 判读须取崩溃线程 psr.tsv 最后采样 (F_1 线程先 cpu135 后迁 cpu139, 误取首采样会判 135)。聚合 EXCLUDED 系 cpu236/39 整窗 1459 (负载相关降频, 槽位核非归因)。v6c ni 10→8 已提交 (job 1721020)。
- 2026-09-24 20:05: v6c 收官 (job 1721020, 19:07-19:58) — **空间 x 减元素数中性**: F 侧 7/7 崩 (canary+5 rep+F 锚, 全 rank3, psr 7/7 cpu139, 全 SEGV_MAPERR; **canonical 0x3ffX 4 例全部 @stencil a.out+0x258f00 = 同作业最强崩溃点集中**; byte-6 heap 2 [0x31/0x33] + @0x4000 1 [0x42 @swap37]) / C1 侧 6/6 净 (v2 锚 813s 跨作业 scf 一致 + 5 rep 287-290s, scf iter3 2.945e-01 一致, E3 展幅 4.2e-8, replica 0)。NELEMS 三维缩减 (v6a z / v6b y / v6c x) F 侧累计 21/21 全崩全 cpu139 → SDC 与空间区域无关。**canonical 栈带再解释 (推断+实证)**: dattach /proc/self/maps 主栈 0xffffc1185000 / libs 0x4000_2d00-3f00 → 0x3ff9-3ffd 带 = worker pthread 栈落点。几何澄清: v2 源 CELL 非立方 (102.87105848×82.296846784×82.296846784), v6c CELL x 102.87105848→82.296846784 保持元素 10.287105848³ 逐位不变 (元素边界线与 v2 重合, 丢的 254 原子恰为 ni 8-9 元素)。频率全作业窗 cpu139 210/210 @1999 / cpu140 210/210 @1998; 新红线 cpu436 整窗 1458 负载相关 (19:08:58-19:57:58, 全天其余 2805 采样正常, 非归因路径)。v6d 终点 2 2 4 (16 元素, 1/rank 真·最小形态, 125 原子) 已提交 (job 1721399, 20:01:35) — 提交后首 3 运行已落: canary 67s / F 锚 187s / F_1 83s 全崩, 全 rank3, 全 psr=cpu139, 全 canonical 0x3ffX @stencil a.out+0x258f00, 终点形态复现确认中。
- 2026-09-24 20:50: v6d 终点收官 (job 1721399, 20:01-20:47) — **NELEMS 轴在 16 元素/1-per-rank 终点关闭**: F 侧 7/7 崩 (canary+5 rep+F 锚, 全 rank3, psr 7/7 cpu139, 全 MAPERR; **canonical 0x3ffX 5/7 全部 @stencil a.out+0x258f00 = 集中达峰**, 0x3ffe 首次入带; byte-6 heap 1 [0x63] + @0x4000 1 [0x7c @swap37]) / C1 侧 6/6 净 (v2 锚 813s scf 三作业连续一致 + 5 rep 196-200s, E3 展幅 2.2e-14, replica 0)。四级全中性 (v6a/b/c/d), F 侧累计 28/28, 元素 640→16 原子 5120→125 — SDC 与问题规模完全解耦, 16 元素/125 原子/16-rank 形态 = cpu139 SDC 最小复现体。频率全作业窗 cpu139 191/191 @1999 / cpu140 191/191 @1998; 新红线 cpu558 整窗 1391 (20:01:37 起 = job 启动, 负载相关, NUMA14, 非归因)。转 Task 10 Step 3 最小复现体定稿 (README + 自含输入 + 验证脚本 + 绑定 + 复现率)。
- 2026-09-24 21:20: Task 10 Step 3 完成 — minimal-reproducer/ 自含包定稿并端到端自证 (job 1721859: F×3 3/3 REPRO_CRASH_ATTRIBUTED + C1×1 CLEAN_CONTROL_PASS, 仅用包内文件; 签名一致性核对通过: 地址类/崩溃点/归因不变量/时延四项全同 v6d 家族)。开发留痕: ① attempt-1 job 1721850 全 rc=127 — reproduce.sh 漏 set --, setvars.sh 吞位置参数 (“Unknown option: F”) → HPKit 未载入 → mpirun not found, 已修复 (attempt 日志 job_repro.attempt1.out + runs/.attempt1-rc127/ 保留); ② verify.sh scf 金值 grep 被 --tag-output 行中标签切断 (C1 显示 FAIL), 改 detag 后匹配并对同批运行目录重判 (runs/verify_table.txt 为准)。12:33 的 V2 期旧骨架 (NT36 rankfile 114-149 + 旧 README/verify) 被替换。Task 10 全部完成, 转 Task 11。

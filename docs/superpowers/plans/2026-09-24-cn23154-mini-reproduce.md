# cn23154-mini-reproduce 独立复现目录 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 cn23154 cpu139 SDC 最小复现用例(完整源码 + 最小配置)提取到独立目录 `cn23154-mini-reproduce`,仅用该目录内的代码与配置复现 SDC 故障,在全部 608 核做对比验证 + 消融实验,并在目录内交付手把手复现 README.md 与总结复盘报告 —— 所有内容自含。

**Architecture:** 五阶段串行: 提取(源码+输入+脚本自含) → 纯目录构建+F/C1 复现(自证自含) → 608 全核逐核探针对比(期望: 唯 c=139 崩) → 消融实验(rankfile/taskset/频率/输入规模) → 目录内双报告交付。前一战役 (ai-run-cn23154-20260923-220641) 的结论与最小复现包 (v6d) 是本战役的输入与金标。

**Tech Stack:** XLSDFT (src/, Makefile.xlsdft_920f, NT=9 编译期常量) + OpenMPI 16-rank --rankfile 绑核 + HPCKit 26.1.RC1 编译环境 + LVTX BLAS 静态库 (外部构建依赖) + dsub/dattach 批处理 + freqmon 10s 全核频率守护。

**Spec:** 用户 /goal 指令 (2026-09-24, 见下) + repo `docs/superpowers/plans/2026-09-23-cn23154-sdc-608core-campaign.md` (前置战役) + `RES/final-summary.md` (前置结论)。

## Global Constraints

- **目录位置**: `/home/share/suke/0903-NUMA3-report-materials-wangxu-v2/cn23154-mini-reproduce/` —— 所有产物自含于此; 系统级依赖 (HPCKit setvars、LVTX_BLAS_ROOT 指向的 BLAS 库) 属环境前提, 在 README 中声明, 不复制不读取其内容。
- **访问边界** (沿袭前置战役): 只可访问 /home/share/suke/0903-NUMA3-report-materials-wangxu-v2 下的内容; LVTX_BLAS_ROOT 仅作为 make 的构建依赖被消费, 不读不列其内容。
- **频率规则** (沿袭): 全程 10s 周期观察全部 608 核频率; 任一核 <1500MHz → 记录在案 EXCLUDED(不作为 SDC 证据, 压力照跑); 1500-1800 GREY; ≥1800 VALID。探针核在其自身窗口内按同规则判定。
- **诚实纪律**: 必须诚实、不能说谎、100% 基于事实; 每个论断对应真实命令与真实输出; 失败与自纠错全部记录。
- **通道限制**: Bash 每次 = 新 SSH, 单命令 ~110s 上限 → 长任务一律 dsub 作业落盘 NFS; 等待用单条远程 until 循环 (≤7×13s); 监控周期性短输出。
- **taskset -c 0-607 必须**: 批处理默认 16 核亲和(不含 cpu139), 所有负载必须包装(脚本内置)。
- **subagent 并发 ≤1; Bash 调用串行。**
- **补丁纪律**: 一单元一补丁; 每任务完成 → 逐项自验证(真实命令+真实输出) → repo 提交(勾选 plan) → 自动 push 到 research/numa3-sdc-0922; **禁止 Co-Authored-By 尾注**(repo CLAUDE.md 明令)。
- **金标值** (自含复现的预期): 自建 NT=9 二进制 sha256 前 16 位 = `763c6843f504e1f7`; F(rank3=131-139) 期望 rc=139 SIGSEGV @rank3、崩溃线程尾采样 psr=139 (中位 ~94s); C1(rank3=131-138,140) 期望 rc=0、scf 三迭代金值 7.796e-01/5.784e-01/3.103e-01、E3=-206.726163764879±1e-10 (~197s)。

---

### Task 1: 计划文档 (本文件)

**Files:** Create: `docs/superpowers/plans/2026-09-24-cn23154-mini-reproduce.md` (本文件)

- [x] **Step 1**: 按前置战役同格式写计划 (本文件), 含五阶段任务分解、金标值、全局约束。
- [x] **Step 2**: git add + commit + push (research/numa3-sdc-0922, 无 Co-Authored-By)。

### Task 2: 提取 — 建立自含目录

**Files (cluster):** Create: `cn23154-mini-reproduce/`: `src/`(完整源码树, 去 .o/旧二进制), `Si.inpt`/`Si.ion`/`Si.psp8`(v6d 最小输入), `rf_nc16_F.txt`/`rf_nc16_C1.txt`, `lib/libomp.so`, `reproduce.sh`/`verify.sh`/`freqmon.sh`(自相对路径, 原样可用)。

- [x] **Step 1**: `mkdir $NEW; cp -r <材料>/src $NEW/src`; 在副本内删除构建产物 (`*.o`, `a.out`, `transpose_chefsi_ut`, `untile_16_ut`, `verify_untile_sim_dump`, `run_sim` — 保留其 .cpp 源码与全部 Makefile*/include/objects.mk)。**严禁在原 src/ 里 clean** (原 NT36 a.out 是历史证物)。
- [x] **Step 2**: 从 `$RES/minimal-reproducer/` 复制 Si.inpt/Si.ion/Si.psp8/rf_nc16_F.txt/rf_nc16_C1.txt/lib/libomp.so/reproduce.sh/verify.sh/freqmon.sh → `$NEW/`。
- [x] **Step 3**: 自验证: sha256 前 16 位逐一比对 (Si.inpt=b1b1467fdfc44803, Si.ion=d008da13b9acb2cd, Si.psp8=2ee90e1b2d080fc5, rf_F=50f500680ddd2cfb, rf_C1=5ce1fce97f9c1f1e, libomp=ddebdbb2ac1f4b85); `bash -n` 三个脚本; 源码文件计数; 目录树落盘 `extraction_manifest.txt`。
- [x] **Step 4**: repo: 勾选 Task 2 + commit + push。

### Task 3: 纯目录构建 + F/C1 自含复现

**Files (cluster):** Create: `$NEW/src/a.out`(构建产物, cp 到 `$NEW/a.out`), `$NEW/runs/`, `$NEW/job_selfval.sh`(#DSUB 一键: 构建+F×3+C1×1+verify)。

- [x] **Step 1**: 写 `job_selfval.sh` (#DSUB -n cn23154-mini-selfval / q_Test_20260903 / -nl cn23154 / -rpn 608 / -x job / -T 7200 / -o 绝对路径): freqmon 10s → `cd $NEW/src && taskset -c 0-607 make -f Makefile.xlsdft_920f NT=9` → sha256 记录 (期望 763c6843f504e1f7; 不符则如实记录并在后续 F×3 以复现率为准) → `cp a.out $NEW/` → F×3 (TMO 600) + C1×1 (TMO 900) via `taskset -c 0-607 $NEW/reproduce.sh` → 对 4 个 run dir 逐个 verify.sh (FREQ_CSV 用本作业 freqmon 输出) → 汇总判定表。
- [x] **Step 2**: dsub 提交, until 循环等完 (~40 min)。
- [x] **Step 3**: 自验证: F 3/3 `REPRO_CRASH_ATTRIBUTED` ∧ C1 1/1 `CLEAN_CONTROL_PASS` ∧ 零 <1500 红线核 → 自含性成立。任何不符 → 如实记录, 按 systematic-debugging 查因后再判。
- [x] **Step 4**: repo: 勾选 + commit + push。
  - **完成 2026-09-24 23:05 (job 1722492)**: BINHASH_MATCH=YES (763c6843f504e1f7, 自含构建 16 分钟); F 3/3 `REPRO_CRASH_ATTRIBUTED` (rc=139, rank3, crash-psr=139, canonical_3fff/3ffd/3ffb, cpu139_min=1999 VALID, 98.6/89.6/76.4s); C1 1/1 `CLEAN_CONTROL_PASS` (rc=0, E3=-206.726163764884 差 5e-12<1e-10, scf_lines=3, cpu140_min=1999 VALID, 199.5s); 零 REDLINE 行。**自含性成立**。诚实偏差: Step 2 的 until 循环等待改为 cron 唤醒短查——reach 通道对后台命令 ~2-3 分钟必死 (Monitor 3 次实证 exit 125, 45s 心跳亦死), 首个 Monitor 的 "djob 缺席" 假阳性系通道濒死时 CLI 失败产物 (job 实际一直 RUNNING)。

### Task 4: 608 全核逐核探针对比验证

**Files (cluster):** `$NEW/gen_probe_rf.sh`+`probe_run.sh`+`job_probe_pilot.sh`(象限形 v1, 已证伪留档), `$NEW/gen_rf_probe2.sh`+`probe_run2.sh`+`job_probe_pilot2.sh`+`job_probe_all2.sh`(修正形 v2), `$NEW/verify_probe.sh`(共用判定器), `$NEW/analyze_probes.sh`, `$NEW/probes/`(v1 伪证证据), `$NEW/probes2/`(v2 结果), `$NEW/probes/QUARTER_FORM_FALSIFIED.md`(伪证记录)。

**设计 v1(象限形, 已证伪 2026-09-24 23:14, pilot job 1722694):** 四象限 16 rank × 9 线程全落 c 所在象限、rank3 = 8 填充核 + c 末位、4 路并发。证伪三重独立证据: (i) 归因断裂 — rep1 byte6 崩溃(rank3)但解引用线程 psr=137 填充核而非探针核 139; (ii) 4 路并发撞作业内存上限(峰值 37.5GB, 173.1s 同步 signal-9 ×4, 受害 rank 14/14/14/3/15 = cgroup OOM 特征) — 并发与形式无关地不可行; (iii) rep2 单跑 55.7s 静默非零退出(139-SDC 内部检查型表象, 与 SIGSEGV 路径竞速; rep1 活过 55.7s 证明非确定性伪影)。附带澄清: `TOL_PSEUDOCHARGE 9.34e-13` 行在包括干净 C1/F 在内的**所有**运行中同值打印 — 良性警告, 非死因(早前误判已纠正)。

**设计 v2(修正形 = bundle 自证 F/C1 换槽对的推广, 2026-09-24 23:35):** F: rank3=131-139(139 末位热槽)→ 14/14 崩 psr=139; C1: rank3=131-138,140(139 闲置)→ 8/8 净 E3 golden。每探针核 c: **rank3 = 131-138 + c 末位热槽; 139 全程闲置(C1 已证安全); c 若为 F 活跃核则其原 rank 以本域首个 F-spare 就位回填(域 3 用 140), 每 rank 槽位数与掩码宽度与 F/C1 完全一致(已证干净布局, 无数值伪影)**。c=139 逐字节≡rf_nc16_F, c=140 逐字节≡rf_nc16_C1(双 diff 端点已过); **仅串行**(内存封顶), 608 × ~205s ≈ 35h, 分 3 个 -T 43200 链式重投, 断点续跑(probes2/rows/P<C>.tsv 存在即跳过)。

- [x] **Step 1 (v1)**: 象限形 4 脚本 + 生成器(13/13 功能测试) — 已完成并被 Step 1b 证伪, 留档。
- [x] **Step 2 (v1)**: pilot 1722694 — (a) 锚点 F `REPRO_CRASH_ATTRIBUTED` ✓; (b) P139×2: rep1 崩但归因断裂(psr=137), rep2 静默退出; (c) 四路并发 OOM 型同步死亡。判定 = 设计不成立 → 走计划内回退(全节点串行)精化为 v2。
- [x] **Step 1b (v2)**: gen_rf_probe2.sh + probe_run2.sh: c=139≡F / c=140≡C1 双 diff PASS; 16 边缘核不变量全过(无重复/139 缺席/c 末位/16 行/549 唯一槽)。
- [x] **Step 2b (v2)**: pilot2 作业 job_probe_pilot2.sh (-T 2400, job 1722873, SUCCEEDED 23:37→23:50): (a) P139(≡F, 期望崩+归因 c=139) + (b) P0(回填类, 期望净) + (c) P131(填充换类, 期望净) + (d) P114(域 3 spare 类, 期望净) + 锚点 F 红线补扫(修 v1 pilot 的 FREQ_CSV 传参 bug)。全过 → 提交全量; 任一失败 → STOP systematic-debugging。**4/4 全过 (2026-09-24 23:50)**: 锚点 F 红线补扫 VERDICT F_20260924T230818 REPRO_CRASH_ATTRIBUTED(FREQ_CSV 修正生效); P139 rc=139 dur=92.5s(67-187s 带内) PROBE_CRASH_ATTRIBUTED(c=139 class=canonical_3ffe sig=11), 签名 rank[1,3]+0x3ffe28baa280(0x3ffX 家族)+a.out+0x258f00+cpsr=139+freq_min=1999 VALID; P0/P131/P114 全 PROBE_CLEAN(E3ok=YES scf1=YES), E3=-206.726163764843/-880/-858, freq_min=1998-1999 VALID。v2 C1-swap 形式定案, 全量扫描放行。
- [ ] **Step 3 (v2)**: job_probe_all2.sh (-T 43200 × 3 链): freqmon 10s + 串行 608 探针(TMO 360) + 每探针进度行 + 断点续跑; 预计 ~35h。脚本已写好并 bash -n 过(2026-09-25); TMO 300→360: pilot2 实测最慢干净探针 P0=235.5s, 300s 仅 65s 裕量, 假 TIMEOUT 会污染 verdict 表(360s 仅在真挂起时才生效); 提交门控 = Task 5 消融全过(cron 状态机执行)。
- [ ] **Step 4**: analyze_probes.sh(v2 路径): 合并 probes2/rows → per-core verdict; 期望 **1 崩(c=139) + 607 净(含 EXCLUDED_FREQ/GREY 标注)**; 频率 <1500 探针核照跑压力但判 EXCLUDED(用户规则); 139 闲置探针中其自身低频为预期(不活跃), 不计入红线排除。(analyze_probes.sh 已切 probes2 路径 + bash -n 过, 2026-09-25; v1 probes/ 留作伪证档案。)
- [ ] **Step 5**: repo: 勾选 + commit + push。
### Task 5: 消融实验 (每因子一小节, 全部自含目录内)

首轮作业 1724844 (2026-09-25 08:05:39-08:09:32, SUCCEEDED): A2/A4 有果, A3 被跳过 — 本板 cpu139 的 scaling_available_frequencies 为空 (cpufreq 驱动不暴露), 原 CAP 逻辑要求目标频率在列表中 → 过于保守; 且 governor 本就是 userspace, 直接 scaling_setspeed 即可。跳过路径未写任何系统状态 (freq CSV: cpu139 全程 1996)。补跑 job_ablation_a3.sh (直接 setspeed + 读回校验 + env 传参 verify) 接力。附带 bug 记录: 首轮 verify 调用把 $FCSV 当第二位置参 → verify.sh 对 "$@" 逐个当 rundir → 产生 VERDICT freq-20260924.csv UNCLASSIFIED 噪音行 (各段首个 verdict 不受影响); 补跑改用 FREQ_CSV env 模式 (与 pilot2 一致)。

- [x] **Step 1**: A1 核心对照 (引 Task 3 数据): F vs C1 唯一差异 139↔140 → 崩 vs 净。
- [x] **Step 2**: A2 去 taskset (F_20260925T080650): **预期被证伪 — 照常复现**。rc=139, 91s(段间), rank=3, crash_psr_last=139, class=canonical_3ffb (0x3ffb3ad21480), cpu139_min=1996 VALID。机理: mpirun --allow-run-as-root 以 root 运行, hwloc/sched_setaffinity 可把亲和自放宽回 cgroup 全 608 核 cpuset, 启动器 16 核掩码拦不住 (认知修正: 16 核陷阱只困朴素非 root 启动, 不困 root+mpirun+rankfile 显式绑定)。结论: taskset 包装非复现必要条件 — 如实记录; 脚本仍统一保留 taskset (与已证 F 形一致)。
- [ ] **Step 3**: A3 频率消融 (补跑 job_ablation_a3.sh): setspeed 1550000 + 读回 (cur≤1600000 方为生效) → F×1 预期**不崩** (rc=0 + scf1≈7.796e-01 + E3≈-206.726163764879 ±1e-10) → setspeed 2000000 + 读回 (cur≥1950000) → F×1 预期崩 (REPRO_CRASH_ATTRIBUTED) → 终态读回 (回 ~2.0GHz)。freqmon 全程, verify 用 FREQ_CSV env。
- [x] **Step 4**: A4 输入规模 (F_20260925T080829): v6c 128 元素, inputhash=8042d625d0b711cd ✓ → rc=139, 52s, rank=3, psr=139, class=byte6-highva_fb (0xfbaaab01c4fc40), cpu139_min=1996 VALID → 规模无关性确认 (与战役 NELEMS 阶梯 16→640 全崩一致)。
- [x] **Step 5**: A5/A6 引证不重跑: A5 二进制同一性 binhash=763c6843f504e1f7 (本轮两次运行同值 = Task 3 = 战役包); A6 NCOMMS<16 死锁 (战役 v5nc8 rc=134, 集群 fabric 下限, 引证)。
- [ ] **Step 6**: repo: 勾选 + commit + push (含 A2 证伪记录 + A3 补跑结果)。
### Task 6: 目录内双报告 + 收尾

- [ ] **Step 1**: `$NEW/README.md` 手把手复现方法: 前提(账号/队列/HPCKit/LVTX) → 构建 → F/C1 运行 → 判定标准与预期值 → 全核探针方法 → 消融方法 → 故障排查 (taskset/setvars 位置参数/tag-output 三坑) → 自检清单。
- [ ] **Step 2**: `$NEW/复盘报告.md`: 战役复盘 (目标/方法/全部数据表: 自含验证 4 runs、608 探针、消融 4 因子/结论/与前置战役一致性/诚实边界)。
- [ ] **Step 3**: RES README 速查节加 mini-reproduce 指针; status.md 续20; 收尾核验 (djob 零遗留、文件清单、探针行数=608)。
- [ ] **Step 4**: repo: 勾选全部 + 最终 commit + push。战役闭环。
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

- [ ] **Step 1**: 写 `job_selfval.sh` (#DSUB -n cn23154-mini-selfval / q_Test_20260903 / -nl cn23154 / -rpn 608 / -x job / -T 7200 / -o 绝对路径): freqmon 10s → `cd $NEW/src && taskset -c 0-607 make -f Makefile.xlsdft_920f NT=9` → sha256 记录 (期望 763c6843f504e1f7; 不符则如实记录并在后续 F×3 以复现率为准) → `cp a.out $NEW/` → F×3 (TMO 600) + C1×1 (TMO 900) via `taskset -c 0-607 $NEW/reproduce.sh` → 对 4 个 run dir 逐个 verify.sh (FREQ_CSV 用本作业 freqmon 输出) → 汇总判定表。
- [ ] **Step 2**: dsub 提交, until 循环等完 (~40 min)。
- [ ] **Step 3**: 自验证: F 3/3 `REPRO_CRASH_ATTRIBUTED` ∧ C1 1/1 `CLEAN_CONTROL_PASS` ∧ 零 <1500 红线核 → 自含性成立。任何不符 → 如实记录, 按 systematic-debugging 查因后再判。
- [ ] **Step 4**: repo: 勾选 + commit + push。

### Task 4: 608 全核逐核探针对比验证

**Files (cluster):** Create: `$NEW/gen_probe_rf.sh`(探针 rankfile 生成器), `$NEW/probe_run.sh`(单探针运行器), `$NEW/verify_probe.sh`(探针判定器), `$NEW/job_probe_pilot.sh`, `$NEW/job_probe_all.sh`, `$NEW/probes/`(结果), `$NEW/analyze_probes.sh`。

设计: 四象限 Q0=0-151 / Q1=152-303 / Q2=304-455 / Q3=456-607 (各 152 核, NUMA 对齐 4×38); 每探针 = 16 rank × 9 线程全部落在 c 所在象限内; **rank3 = 8 个填充核 + 探针核 c 于 thread-8 末位**; c≠139 时 139 从全部 144 槽位剔除 (探针不受污染); 生成器算法统一无特例。四象限并发 (4×144=576 线程), 象限内串行, 断点续跑 (跳过 probes tsv 已有核)。

- [ ] **Step 1**: 写 4 个脚本 + 生成器 (生成器: 象限核表 − {c} − {139 if c≠139} → 15 个非 3 rank 各取 9 连续核, rank3 取剩余头 8 核 + c 末位; 输出 OpenMPI rankfile 格式; c=139 时同算法无特例)。`bash -n` 全部。
- [ ] **Step 2**: 试点作业 job_probe_pilot.sh (-T 3600): (a) 原版 rf_nc16_F 全节点跑 1 次 (锚点, 期望崩); (b) 象限形 c=139 ×2 (设计验证, 期望崩+归因 psr=139); (c) c=140/200/427/583 四象限并发各 1 次 (期望 rc=0, 同时验证 4 路并发无害)。判定: (a) 崩 ∧ (b) 2/2 崩归因 ∧ (c) 4/4 净 → 设计成立; 否则 STOP 按 systematic-debugging 复盘, 回退方案 = 全节点形串行探针 (37h, 分 6 作业)。
- [ ] **Step 3**: 全量作业 job_probe_all.sh (-T 43200): freqmon 10s + 4 象限并发循环 (TMO 400/探针) + 每 20 探针打印进度 (通道保活) + 断点续跑; 预计 ~9-10h, 超 T 中断则原脚本重投续跑。
- [ ] **Step 4**: analyze_probes.sh: 合并四象限 tsv → probe_analysis.tsv (per-core: verdict / rc / crash-psr / addr class / freq verdict); 期望 **1 崩 (c=139) + 607 净 (含 EXCLUDED_FREQ/GREY 标注)**; 频率 <1500 的探针核照跑压力但判 EXCLUDED (用户规则)。
- [ ] **Step 5**: repo: 勾选 + commit + push。

### Task 5: 消融实验 (每因子一小节, 全部自含目录内)

- [ ] **Step 1**: A1 核心对照 (引 Task 3 数据): F vs C1 唯一差异 139↔140 → 崩 vs 净。
- [ ] **Step 2**: A2 去 taskset: F 形不带 taskset 包装跑 1 次 → 如实记录 (预期: 绑定失败或 139 不在亲和集 → 不崩/报错; 证 taskset 必要性)。
- [ ] **Step 3**: A3 频率消融: 作业内 (userspace governor) 将 cpu139 scaling_setspeed=1550000 → F×1 (预期**不崩**, ≤1.55GHz 规则) → 恢复 2000000 → F×1 (预期崩) → 恢复确认。freqmon 全程。
- [ ] **Step 4**: A4 输入规模: 复制战役 v6c 输入 (128 元素) 入 `$NEW/ablation/`; F 形 ×1 (预期崩 — 规模无关性向上; 阶梯 16→640 全崩的独立目录复核)。
- [ ] **Step 5**: A5/A6 引证不重跑: A5 二进制同一性 (Task 3 sha256 = 763c6843f504e1f7 = 战役包); A6 NCOMMS<16 死锁 (战役 v5nc8 rc=134, 集群 fabric 下限, 引证)。
- [ ] **Step 6**: repo: 勾选 + commit + push。

### Task 6: 目录内双报告 + 收尾

- [ ] **Step 1**: `$NEW/README.md` 手把手复现方法: 前提(账号/队列/HPCKit/LVTX) → 构建 → F/C1 运行 → 判定标准与预期值 → 全核探针方法 → 消融方法 → 故障排查 (taskset/setvars 位置参数/tag-output 三坑) → 自检清单。
- [ ] **Step 2**: `$NEW/复盘报告.md`: 战役复盘 (目标/方法/全部数据表: 自含验证 4 runs、608 探针、消融 4 因子/结论/与前置战役一致性/诚实边界)。
- [ ] **Step 3**: RES README 速查节加 mini-reproduce 指针; status.md 续20; 收尾核验 (djob 零遗留、文件清单、探针行数=608)。
- [ ] **Step 4**: repo: 勾选全部 + 最终 commit + push。战役闭环。
# NUMA3 SDC 独立复现与微架构诊断计划(cn23154)v2

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking. 本计划为实验研究型计划(不修改 sdcshield 产品代码),每个 Task = 一个独立实验单元,验证 = 真实命令输出引用。v2 并入材料目录 CLAUDE.md 实验协议(结果目录、SDC 严格定义、可追溯记录、交错对照、交付物)与 cluster-access 技能 2026-09-22 更新(dattach 硬限制 → 全部长活走 dsub 批处理)。

**Goal:** 在 cn23154 节点(608 核,已恢复 2GHz)上,仅用 `/home/share/suke/0903-NUMA3-report-materials-wangxu` 提供的复现程序(XLSDFT DFT 应用)独立复现 NUMA3(core139 所在域)的 SDC 问题,完成全部 608 核对比,做微架构级诊断,并挖掘稳定复现的最小压测用例。

**Architecture:** 16 MPI rank × 36 OMP 线程的 DFT 负载按 rankfile 轮转映射到 16 个 NUMA 域(每域 38 核,rank 用前 36 核)。执行机制:所有长实验通过 dsub 批处理作业(`#DSUB` 头、`-x job` 独占、`-T` 时限、脚本内 `taskset -c 0-607` 包装、日志写 NFS 结果目录);分析在登录节点直接读 NFS 日志 + 符号化。诊断路径:轮转矩阵归因 NUMA → NUMA3 内核级二分定位 → core dump + 反汇编微架构诊断 → 从应用源码抽取最小复现器 → 用最小复现器做 608 核逐 CPU 精细扫描。

**Tech Stack:** XLSDFT(材料目录预编译 `src/a.out`,带 debug_info)+ Bisheng MPI/OpenMP(HPCKit 26.1.RC1)+ LVTX BLAS(编译期已并入 a.out)+ SStructMG;诊断:gdb、objdump、addr2line、coredumpctl、taskset、numactl;作业系统 dsub/dattach/djob/dkill。

**Spec:** /goal(2026-09-22)+ 材料目录 CLAUDE.md 实验协议 + 仓库 CLAUDE.md 诚实性条款。本计划是唯一工作清单。

## Global Constraints

- **结果目录(协议§五)**:`RES=/home/share/suke/0903-NUMA3-report-materials-wangxu/ai-run-cn23154-20260922-182127`,含子目录 environment/ inventory/ baseline/ cpu_sweep/ numa139/ diagnostics/ minimization/ scripts/ logs/ raw/ summary/;一切新生成文件写入 RES,不覆盖材料目录任何既有文件;应用副本在 RES/app(已清理旧日志副本,原件未动)。
- **执行机制(cluster-access 2026-09-22)**:dattach -c 有 120 秒/1024 字节限制且退出即回收子进程 → 长活一律 dsub 批处理(`-x job` 独占 + `-T` 时限 + 脚本内 taskset -c 0-607 + 亲和性留证);短命令可 dattach 进正在运行的批作业;登录节点直接读 NFS 日志做分析。
- **全核可用(协议§3.2)**:每批作业内验证 hostname/在线 CPU/cpuset/亲和性,并用轻量探针确认 608 个 CPU 每个都能实际执行绑定进程(实测运行位置==目标 CPU);tested_cpu_set == allocated_cpu_set 才能宣称全覆盖。
- **只读边界(协议§2.1)**:数据只看材料目录;系统接口(/proc、/sys、dmesg、EDAC、perf)允许。**阅读顺序偏差如实记录**:本 session 在独立基线前已看过历史日志(协议§2.2 违例),已形成的历史认知只作待验证假设,结论一律以独立实验为准,历史日志仅事后交叉验证。
- **SDC 严格定义(协议§阶段1)**:区分 正确完成 / SDC(正常完成但输出与正确结果不一致且非预期非确定性)/ crash / hang / timeout / OOM / signal / affinity failure / infra failure;**crash 不计为 SDC**,但 crash 是引导线索;最小复现器目标 = 把损坏以数据比对形式捕获(而非依赖崩溃)。
- **充分性判据(协议§九)**:崩溃/损坏运行记录签名即为有效样本;"未观察到 SDC"必须写成"N 次运行、累计 T 小时、指定条件下未观察到",附失败率上界;关键配置交错重复(control → 139/NUMA3 → candidate → 139 → control)。
- **诚实性(仓库 CLAUDE.md)**:所有引用输出必须真实执行所得;失败即失败;不确定即标注。
- **频率**:节点 2GHz,用 /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq 抽查(dnode --frequency 是陈旧值),不主动改频,不改 BIOS/内核参数,不重启节点。

## 背景事实(材料目录实测 + 待独立验证的历史认知)

- `local_run_rot.sh`:NP=16,OMP 36 线程,rank r → NUMA (r+shift)%16,每 NUMA 38 核取前 36;BIND_MODE=ppr 等价 shift0。
- 历史日志(先入之见,仅作假设):崩溃 rank 跟随 NUMA3(cores 114-151);崩溃点 `Stencil_method::calc_laplacian_d3_c2_o0<double>`(Aar OMP 线程);fault addr 非规范、含 0x…4001a… 位模式;SCF iter 1-2 崩溃。
- `src/a.out`:aarch64 PIE 带 debug_info;依赖 HPCKit 26.1.RC1、RES/app/lib/libomp.so、libSstructmg.so(RPATH)、libmemkind/libnuma。
- 节点实测(2026-09-22):608 CPU 在线,NUMA0-15 各 38 核(16-31 为 HBM-only 无 CPU),max 2000MHz,内存 565GB/可用 521GB,systemd-coredump 管理 core。
- 作业系统:job 1693732(sleep infinity 占位,无 -x)将按 v2 机制替换为批处理作业。

---

### Task 0: 执行模式切换(dattach→dsub 批处理)

- [x] 0.1 `dkill 1693732` 结束占位作业,`djob` 确认无遗留 RUNNING job 在 cn23154。
- [x] 0.2 编写批处理模板脚本(RES/scripts/dsub_env.sh):#DSUB 头(-q q_Test_20260903 -nl cn23154 -rpn 608 -x job -T 按需 -o RES/logs/<name>.out),脚本体:记录日期/hostname/亲和性 → taskset -c 0-607 包装实际工作。
- [x] 0.3 提交第一个批作业(环境基线,见 Task 1),确认 RUNNING、`dattach -c hostname <newid>` 可用。

### Task 1: 节点环境基线与 608 核探针(协议§四、§3.2)

**Files:** RES/environment/*.txt, RES/logs/env-job.out。

- [x] 1.1 环境捕获脚本(批作业执行):CPU 型号/MIDR(implementer/part)/stepping、内核版本、NUMA 拓扑、SMT 状态、全部 CPU 频率汇总(逐 CPU min/max/cur + governor + boost)、cpu139 与其他 CPU 频率一致性、内存各 NUMA 分布、节点负载、其他进程、thermal/EDAC/dmesg 错误状态(权限内)。
- [x] 1.2 608 核探针:对每个 CPU c(0..607)起 `taskset -c $c sh -c 'grep -E "^processor|Cpus_allowed_list" /proc/self/status'` 类探针,验证实际执行位置==c;输出 tested_cpu_set 与 0-607 比对,assert 相等。
- [x] 1.3 记录作业/cpuset 信息:djob -l、/proc/self/status Cpus_allowed_list、节点进程清单。
验证:RES/environment/ 下文件齐全;探针报告 608/608 全通过;引用真实输出。

### Task 2: 材料清点与哈希(协议§六阶段0)

**Files:** RES/inventory/tree.txt、hashes.sha256、classification.md。

- [x] 2.1 完整目录树 + 关键文件 sha256(a.out、Si.inpt/Si.ion/Si.psp8、local_run_rot.sh、lib/libomp.so、src/*.cpp 清单)——登录节点执行即可(NFS)。
- [x] 2.2 文件分类:源码/可执行/构建脚本/运行脚本/配置/输入/说明/历史日志(只列不解)。

### Task 3: 复现程序源码级理解(协议§六阶段0,登录节点分析)

**Files:** RES/inventory/program-understanding.md。

- [x] 3.1 通读 main.cpp→scf.cpp→chefsi.cpp/aar.cpp→stencil*.cpp 调用链与 Memory_pool/memkind 分配策略;回答协议清单:程序计算什么、正确结果如何产生、SDC 如何判定、UB/数据竞争/RNG/整数溢出/未初始化内存/编译器优化敏感性、CPU 与内存绑定方式、能否检测线程迁移、crash/timeout 是否可能误判为 SDC。
- [x] 3.2 明确本次实验的"正确结果指纹":SCF 轨迹(iteration/scf error/能量/特征值)在无故障配置下的可重复性;判定规则写入文档。
- [x] 3.3 形成"这是硬件相关 SDC 而非程序缺陷"的初步论证所需实验清单(对照配置设计)。

### Task 4: 独立基线复现(协议§阶段1-2)

**Files:** RES/baseline/、RES/logs/baseline-*.out、RES/raw/run-results.jsonl(追加)。

- [x] 4.1 批作业跑基线系列:ppr:0 ×3 + rotate:0 ×2(串行,每个跑完再下一个);KMP_AFFINITY=verbose;运行目录 RES/app(日志 tee 到 RES/app/logs/,不触材料原件)。
- [x] 4.2 每次运行记录 run_id 与协议§五全字段(时间戳/作业/哈希/绑定/时长/退出码/stdout 摘要/判定/错误签名/是否迁核/频率抽查)→ run-results.jsonl。
- [ ] 4.3 对照(干净)配置:OMP_NUM_THREADS=35 + 自定义 rankfile 完全避开 NUMA3(15 个 NUMA × 35 核 = 525,rank16 …需实测可否);若可行,取"已知正确输出"= 干净配置的 SCF 轨迹;不可行则记录原因并改用 NUMA3 旁路(过订阅)方案。
- [x] 4.4 汇总:崩溃跟随哪个 NUMA、故障线程绑定核(KMP verbose tid→core)、复现率、时间分布;阶段结论(独立!)写入 RES/baseline/summary.md。**此后才允许**把历史日志与此独立结论做交叉验证并记录异同。

### Task 5: 608 核对比矩阵(协议§阶段4)

**Files:** RES/cpu_sweep/matrix.md、run-results.jsonl(追加)。

- [x] 5.1 批作业跑 rotate shift 1-15(每档 1 次;shift0 已有 ≥3 样本);每档满足充分性判据(崩溃=有效;不崩须 iter≥5)。——完成:rot-s1..s15 全部 clean_iter5(iter=5 达标),shift0 族 7 运行(含 2h 长跑);16/16 shift 穷尽(matrix2 03:00:33 收官)
- [x] 5.2 NUMA3 旁路对照:自定义 rankfile 把 NUMA3 上的 rank 重叠绑到其他 NUMA(NUMA3 闲置),其余正常;iter≥5 判干净,重复 1 次。——C2 已跑但其 SIGKILL 根因为 HBM 配对 OOM(设计缺陷,diagnostics/c2c3-sigkill-root-cause.md),NUMA3 闲置本身无异常;干净 1:1 重跑 C2fix 已排入 matrix3_fixes.sh(stats20 后自动提交)
- [x] 5.3 生成矩阵:每档 NUMA3 上的 rank / 崩溃与否 / 故障核 / 其余 15 rank 是否干净;统计每核在敏感位与对照位的暴露与结果;核对 tested_cpu_set == 0-607。——cpu-results.csv 608 行全覆盖(runs≥1 全核);探针断言 tested=={0..607} 通过;执行证据 759 采样/24 运行(exercised-coverage-proof.txt)

### Task 6: NUMA3 核级定位(协议§阶段3)

**Files:** RES/numa139/、analysis 脚本 RES/scripts/gen_rankfile.sh。

- [ ] 6.1 从崩溃日志提故障线程绑定核;若集中单一核→坏核假设,若分散→域级(L3/内存控制器)假设。
- [ ] 6.2 NUMA3 内包含/排除实验:避开 139(114-138+140-150)跑;含 139 最小子集二分(OMP_NUM_THREADS 相应缩减,记录负载变化);每档 ≥1 阳性 + 1 阴性重复。
- [ ] 6.3 严格 CPU139 实验:敏感 rank OMP_NUM_THREADS=1 taskset -c 139(其余 rank 正常)——能否触发。
- [ ] 6.4 计算侧 vs 内存侧:读 Memory_pool/memkind 代码确定分配落点;numactl --membind 改绑敏感 rank 内存(本地/远端)对比实验,记录实际内存落点(/proc/<pid>/numa_maps)。
- [ ] 6.5 交错对照时间漂移检查:control → 139 → candidate → 139 → control 至少一轮。

### Task 7: 微架构诊断(协议§七)

**Files:** RES/diagnostics/microarch.md。

- [x] 7.1 崩溃点反汇编(objdump ±20 条),确定故障指令与坏地址来源指令。
- [x] 7.2 coredumpctl 取 core → gdb:寄存器、坏值、debug_info 对应源变量;推断"应为什么值、损坏成什么"(位模式分析)。
- [x] 7.3 ≥5 崩溃样本对比:fault addr 位模式/寄存器态/损坏来源变量是否同一;归纳确定性。
- [x] 7.4
  - 7.4 证据(2026-09-23 00:5x,dattach 实测):perf_event_paranoid=2,perf stat 用户态出数;EDAC 布局 /sys/devices/mc0/(控制器级,仅 1 mc),matrix1 负载中 ce=0 ue=0、csrow 全零——campaign 至今零 ECC 事件,与 DRAM 位翻转假说不符;campaign 末次快照并入 final-summary。 节点遥测:实验前后 dmesg/EDAC ce_count/ue_count 对比;perf 可用性检查(perf_event_paranoid),可用则采 cycles/IPC/cache 事件。
- [x] 7.5
  - Task 7 证据落盘(2026-09-23):$RES/diagnostics/historical-crash-microarch-analysis.md(4 起历史崩溃:RANK_BIND 证明全在 die3/NUMA3;故障地址=(垃圾高16):(有效低48,落于 5.00GB Memory_pool);指令 ld1d {zN.d},p1/z,[base,x10,lsl #3] —— 共享索引 x10 的先前 load 成功 => base 寄存器 x11/x14 高 16 位损坏;H1a GPR 高位损坏为唯一主假设,32 位位宽论证排除软件索引损坏)+ node-coredump-inventory.md(72 core 法证:sdcfault.so 注入实验,零自发崩溃)。注意:这些分析基于历史 4 事件;若实验复现新事件,v6 已备寄存器级取证。 微架构假设 + 证据链(明确区分:已证实/强相关/未证实/已排除),写针对性检验实验。

### Task 8: 最小压测用例(协议§八)

**Files:** RES/minimal/(源码+构建+运行+校验+README)、RES/minimization/reduction-log.md。

- [x] 8.1 从应用源码抽独立复现器(无 MPI):复用 stencil/aar/内存池 + Si.inpt 几何;金标准=好核/串行计算;输出逐字节比对,损坏以数据比对捕获(不依赖崩溃)。
- [ ] 8.2 单因素逐步裁剪(规模/循环/线程/类型/工作集/访问模式/指令路径/编译选项/SIMD/对齐/绑定),每步 A/B 验证并记录复现率,直到不可再删。
- [x] 8.3 稳定性统计:敏感核 ≥10 次(复现率+首错时间),对照核 ≥10 次阴性;608 核逐 CPU 扫描(tested==allocated)。

> **进度 2026-09-23 06:55**:8.2 收官 —— 9/9 有效因子 clean(消减方向与全参数方向同样零触发);grid55 因 mini_phase2.sh:55 误传 --grid(二进制接口为 --nx/--ny/--nz)rc=2 未执行,修正调用已定义,待 die3 空闲补测;最小用例 = red-base 原始参数(结构保真定义,复现力排序在门控下不可行)。red-* 全表与解读 RES/minimization/reduction-results.md,案例报告 §5.3。8.3 稳定性系列(die3x10 vs die0x10)运行中(~08:30)。
> **进度 2026-09-23 10:45**:8.3 收官 —— die3x10 全清(332.89±2.88 it/s,CV 0.87%)vs die0x10 全清(334.32±3.09,CV 0.92%),Welch t=-1.07 不显著、带宽完全重叠,零 SDC 零 crash、无首错时间;Rule of Three 合计 n=136 -> 上界 2.2%。608 核逐核扫描(tested==allocated)已由 Task 6 Phase A/B/C 完成。案例报告 §5.4;final-summary Q11 已填。剩余:stats20 终值并入、EDAC 末次快照、matrix3、grid55 补测。
- [ ] 8.4 README:构建/运行/判定/复现方法,第三方可从零复现。


> **进度 2026-09-22 22:55**:8.1 完成 — `RES/minimization/mini_laplacian.cpp`(v5,目录由 minimal/ 改为 minimization/,与协议交付物目录一致)。忠实提取 stencil.cpp:1695 `calc_laplacian_d3_c2_o0<double,double>`(FDn=12/ghost=12/omp static-chunk+simd/指针步进),Si.inpt 220x160x160 折半为 110x80x40;金标准=同函数 1 线程 parallel 区计算(逐位相等依据:逐元素数学与 k 划分无关);每迭代 memcmp + 巡检轮转 FNV + 金丝雀 + 信号捕获,退出码 0/3/4/5/6/7 分类 SDC/崩溃/越界。aar 未纳入(历史崩溃指令位于 stencil 内核,非 aar;若 8.2 显示必要再并入)。两个仪表缺陷已修复并记录(悬垂 json_path;BiSheng libomp 跨 single 陈旧读→全 atomic)——后者为重要工具链发现,原应用同工具链编译,列入诊断清单。验证:1555 迭代 checks=97 精确、rot 严格单调、~3100 迭代 0 误报、放置验证通过、SVE ld1d 同构指令确认。608 核扫描作业 1700367 已入队(Phase A 16 die×NT=36×300s;Phase B NUMA3 逐核×120s;Phase C 对照 10 核),即 8.3 的 608 核扫描部分;8.2 单因素裁剪与 ≥10 敏感/≥10 对照统计待扫描结果后执行。### Task 9: 交付物、报告与提交(协议§十一 + 仓库义务)

**Files:** RES 内 README.md、environment-report.md、experiment-plan.md、cpu-results.csv、run-results.jsonl(终版)、diagnosis-report.md、minimal-reproducer/、final-summary.md、status.md(持续更新);本地仓库 docs/cases/hpc/2026-09-22-numa3-sdc-cn23154.md。

- [ ] 9.1 status.md 随实验推进持续更新(已完成/进行中/样本量/覆盖核数/观察/异常/下一步)。
- [ ] 9.2 协议§十一全部交付物落盘并自检齐全;final-summary.md 逐条回答§11.8 问题清单。
- [x] 9.3 本地仓库 feature 分支 research/numa3-sdc-0922 提交(计划+案例报告)并推送;commit message 不带 Co-Authored-By 尾注(按仓库 CLAUDE.md)。

## Self-Review 结论(v2)

- 覆盖:目标五要素 + 协议§3-§十一全部映射到 Task 0-9;SDC 严格定义贯穿 Task 4/8;阅读顺序偏差的诚实记录在 Global Constraints 与 Task 4.4。
- 占位符:无 TBD;所有步骤有具体命令或明确产出物定义(脚本体在执行时落入 RES/scripts/,内容随 Task 附带)。
- 一致性:RES 路径统一;脚本名唯一(gen_rankfile.sh、dsub_env.sh 模板、run-results.jsonl 追加式)。

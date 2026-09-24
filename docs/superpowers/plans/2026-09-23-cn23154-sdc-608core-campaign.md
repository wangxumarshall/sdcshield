# cn23154 全 608 核 SDC 筛查战役 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans (inline, recommended for this campaign - long-running cluster jobs need session-continuous monitoring) or superpowers:subagent-driven-development to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 cn23154（鲲鹏 920F，608 核）上用 v2 材料目录的 XLSDFT 复现程序完成全部 608 核 SDC 筛查、故障核定位与确认、微架构级根因诊断、最小化复现，产出全套可追溯交付物。

**Architecture:** 满机窗口覆盖（rankfile 偏移 0/1/2）+ 失效归因 + 混合槽位二分定位到单核；全程 10 秒级 608 核频率守护与闸门（>=1800MHz 有效 / <1500MHz 移出筛查只保压力）；链式可恢复 dsub 独占作业驱动；每次运行一条 JSONL 记录；判定分类学与 GOLDEN 多数共识。

**Tech Stack:** 集群侧 bash/OpenMPI(dsub 批处理, q_Test_20260903 队列)/XLSDFT src/a.out(NT=36)；登录节点轮询监控；本地 Windows 侧 PowerShell + git（仓库文档提交）。

**Spec:** docs/superpowers/specs/2026-09-23-cn23154-sdc-608core-campaign-design.md（本计划从 spec 推导，执行者须同时读两者）

## Global Constraints（每个任务隐含遵守）

- **访问边界**：集群 /home 下只允许 /home/share/suke/0903-NUMA3-report-materials-wangxu-v2（下称 MAT）；/proc、/sys、/dev/cpu、系统工具、/work_ssd/software/HPCKit 允许；构建依赖（LVTX BLAS 头树）仅作编译输入。结果只写 MAT/ai-run-cn23154-<ts>/（Task 1 建立后下称 RES）。不覆盖 MAT 任何既有文件。
- **历史隔离**：本仓库 git 历史中的旧战役结论（NUMA3/matrix3/core139 等）不得影响本战役设计与判定。
- **taskset 陷阱**：批处理默认亲和性仅 16 核——一切节点负载必须 `taskset -c 0-607` 包装并在脚本开头 `echo "affinity: $(taskset -pc $$ | tail -1)"` 留证。
- **通道纪律**：Bash 在登录节点（reach），每命令独立 SSH；长活一律 dsub 批处理；监控循环必须 <=2 分钟有输出；dattach 仅限 <=120 秒、<=1024 字节命令。
- **频率闸门**：每轮运行有效核集 = 计划核集 ∩（窗口内最低采样 >=1800MHz）；<1500MHz 核移出筛查仅保压力；1500-1800MHz 灰区重测；采样缺失不归因。
- **判定诚实**：crash/hang/timeout/OOM 不计 SDC；通过核只写"未观察到 SDC + 上界"，绝不写"无 SDC"；相关不写因果；异常结果不丢弃。
- **特权操作**（受控调频、核隔离、dmesg 摘录）只写入 RES/summary/admin_requests.md，不执行。
- **补丁纪律**：仓库侧产物（里程碑报告）一个单元一个 commit，commit 走 `-F` 文件、无 Co-Authored-By 落款、push 到 research/numa3-sdc-0922；集群侧产物以 run_id 在 RES 内留痕（不进 git）。
- **监控作业用后台 Bash**：`run_in_background` 的轮询循环（每 30-60 秒打印 djob 状态/tail 日志，作业离开 RUNNING 即退出），绝不久等无输出。
- **运行环境**：source /work_ssd/software/HPCKit/26.1.RC1/setvars.sh；OMP_NUM_THREADS=36（与编译期 NT=36 匹配，不可改，除非 Task 10 重编译）；输入 Si.inpt/Si.ion/Si.psp8 由 MAT 拷贝到每运行目录。

---

### Task 1: 战役框架 — 结果目录与核心脚本

**Files:**
- Create (cluster): `RES/{environment,inventory,baseline,cpu_sweep,fault_localization,diagnostics,minimization,scripts,logs,raw,summary}/`、`RES/status.md`
- Create (cluster): `RES/scripts/{freqmon.sh,freq_window.sh,rankfile_window.sh,rankfile_custom.sh,runrec.sh,driver.sh,env_report.sh,probe_cpus.sh}`、`RES/scripts/fpstress.c`

**Interfaces:**
- Produces: `RES` 路径（写入本计划"战役参数"节）；脚本签名——`freqmon.sh OUTDIR [INTERVAL=10]`；`freq_window.sh CSV TS1 TS2 CPULIST`（输出 `VERDICT|min=|med=|mean=|n=`，VERDICT 属于 VALID/GREY/EXCLUDED/NO_DATA）；`rankfile_window.sh OFFSET OUTFILE`（offset 0/1/2）；`rankfile_custom.sh OUTFILE R0=cpu,cpu,... R1=...`（16 个 rank 全给或只给覆盖项、缺省用窗口 0 填充）；`runrec.sh CONFIG_ID RF_PATH [TIMEOUT_S]`（建 raw/<run_id>/，返回 run_id，追加 RES/raw/results.tsv）；`driver.sh`（读 RES/scripts/queue.tsv 状态机）。

- [x] **Step 1: 建立结果目录**

```bash
TS=$(date +%Y%m%d-%H%M%S)
RES=/home/share/suke/0903-NUMA3-report-materials-wangxu-v2/ai-run-cn23154-$TS
mkdir -p $RES/{environment,inventory,baseline,cpu_sweep,fault_localization,diagnostics,minimization,scripts,logs,raw,summary}
printf '# cn23154 SDC campaign status\nstarted %s\n' "$(date -Is)" > $RES/status.md
echo "$RES" > /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/.last_campaign_dir
```

- [x] **Step 2: 写 freqmon.sh（频率守护）**

```bash
cat > $RES/scripts/freqmon.sh <<'EOF'
#!/bin/bash
# freqmon.sh OUTDIR [INTERVAL=10] - sample all cpus scaling_cur_freq every INTERVAL s
OUTDIR=$1; INT=${2:-10}
mkdir -p "$OUTDIR"
while true; do
  F="$OUTDIR/freq-$(date +%Y%m%d).csv"
  [ -f "$F" ] || echo "timestamp,cpu,freq_mhz" >> "$F"
  ts=$(date +%s.%N)
  for d in /sys/devices/system/cpu/cpu[0-9]*; do
    c=${d##*cpu}
    f=$(< $d/cpufreq/scaling_cur_freq 2>/dev/null) || continue
    echo "$ts,$c,$((f/1000))" >> "$F"
  done
  sleep "$INT"
done
EOF
chmod +x $RES/scripts/freqmon.sh
```

- [x] **Step 3: 写 freq_window.sh（窗口频率判定）**

```bash
cat > $RES/scripts/freq_window.sh <<'EOF'
#!/bin/bash
# freq_window.sh CSV TS1 TS2 CPULIST -> VERDICT|min=|med=|mean=|n=
CSV=$1; T1=$2; T2=$3; CPULIST=$4
[ -f "$CSV" ] || { echo "NO_DATA|min=|med=|mean=|n=0"; exit 0; }
awk -v t1="$T1" -v t2="$T2" -v cl="$CPULIST" 'BEGIN{n=split(cl,A,",");for(i=1;i<=n;i++)W[A[i]]=1}
 $1>=t1 && $1<=t2 && ($2 in W){v[NR]=$3; if(mn==""||$3<mn)mn=$3; s+=$3; c++; a[c]=$3}
 END{ if(c==0){print "NO_DATA|min=|med=|mean=|n=0"; exit}
      # median
      for(i=1;i<=c;i++)for(j=i+1;j<=c;j++)if(a[j]<a[i]){t=a[i];a[i]=a[j];a[j]=t}
      med=(c%2)?a[int(c/2)+1]:(a[c/2]+a[c/2+1])/2
      v=(mn<1500)?"EXCLUDED":((mn<1800)?"GREY":"VALID")
      printf "%s|min=%d|med=%d|mean=%d|n=%d\n",v,mn,med,s/c,c }' "$CSV"
EOF
chmod +x $RES/scripts/freq_window.sh
```

- [x] **Step 4: 写 rankfile 生成器（窗口 + 自定义混合槽位）**

```bash
cat > $RES/scripts/rankfile_window.sh <<'EOF'
#!/bin/bash
# rankfile_window.sh OFFSET OUTFILE - rank r -> NUMA r, cpus r*38+OFF .. r*38+OFF+35
OFF=$1; OUT=$2; H=${TARGET_HOST:-cn23154}
: > "$OUT"
for ((r=0;r<16;r++)); do
  first=$((r*38+OFF)); cpus=$(seq -s, $first $((first+35)))
  echo "rank $r=$H slot=$cpus" >> "$OUT"
done
EOF
cat > $RES/scripts/rankfile_custom.sh <<'EOF'
#!/bin/bash
# rankfile_custom.sh OUTFILE [R=n=cpu,cpu,...]... - overrides on top of window 0
OUT=$1; shift; H=${TARGET_HOST:-cn23154}
declare -A OV
for a in "$@"; do r=${a%%=*}; OV[$r]=${a#*=}; done
for ((r=0;r<16;r++)); do
  if [ -n "${OV[$r]}" ]; then cpus=${OV[$r]};
  else first=$((r*38)); cpus=$(seq -s, $first $((first+35))); fi
  echo "rank $r=$H slot=$cpus" >> "$OUT"
done
EOF
chmod +x $RES/scripts/rankfile_window.sh $RES/scripts/rankfile_custom.sh
```

- [x] **Step 5: 写 runrec.sh（单次运行记录器）**

```bash
cat > $RES/scripts/runrec.sh <<'EOF'
#!/bin/bash
# runrec.sh CONFIG_ID RF_PATH [TIMEOUT_S] - one mpirun, full provenance, appends RES/raw/results.tsv
set -u
CFG=$1; RF=$2; TMO=${3:-14400}
RES=$(cat /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/.last_campaign_dir)
MAT=/home/share/suke/0903-NUMA3-report-materials-wangxu-v2
RID="${CFG}_$(date +%Y%m%dT%H%M%S)"; RD=$RES/raw/$RID; mkdir -p "$RD"
src=$MAT/src/a.out
cp $MAT/Si.inpt $MAT/Si.ion $MAT/Si.psp8 "$RD"/ && ln -sf $src "$RD/a.out" && ln -sf $MAT/lib "$RD/lib"
BHASH=$(sha256sum $src | cut -c1-16); IHASH=$(cat $MAT/Si.inpt $MAT/Si.ion $MAT/Si.psp8 | sha256sum | cut -c1-16)
cp "$RF" "$RD/rankfile.txt"
source /work_ssd/software/HPCKit/26.1.RC1/setvars.sh
export LD_LIBRARY_PATH="$RD/lib:${LD_LIBRARY_PATH:-}"
export OMP_NUM_THREADS=36 OMP_PROC_BIND=close OMP_PLACES=cores CHEFSI_USE_OPT=1
TS1=$(date +%s.%N)
( while kill -0 $$ 2>/dev/null; do date +%s.%N >> "$RD/psr.tsv"; ps -eLo pid,tid,psr,comm | grep -E 'a\.out' >> "$RD/psr.tsv"; sleep 15; done ) & SAMPLER=$!
cd "$RD"
timeout -k 60 $TMO taskset -c 0-607 mpirun --allow-run-as-root -np 16 \
  --rankfile "$RD/rankfile.txt" --report-bindings --tag-output \
  -x LD_LIBRARY_PATH -x OMP_NUM_THREADS -x OMP_PROC_BIND -x OMP_PLACES -x CHEFSI_USE_OPT \
  bash -c 'echo "RANK_BIND rank=${OMPI_COMM_WORLD_RANK} host=$(hostname) pid=$$ $(taskset -pc $$ 2>&1 | tail -1)"; exec "$0" -name Si' \
  "$RD/a.out" > "$RD/stdout.log" 2> "$RD/stderr.log"
RC=$?
kill $SAMPLER 2>/dev/null
TS2=$(date +%s.%N)
CSV=$(ls -t $RES/environment/freq-*.csv 2>/dev/null | head -1)
ACT=$(awk -F'[=,]' '{for(i=2;i<=NF;i++)printf "%s ",$i}' "$RD/rankfile.txt" | tr -s ' ' ',')
FRQ=$($RES/scripts/freq_window.sh "$CSV" "$TS1" "$TS2" "$ACT")
AFFOK=$(grep -c RANK_BIND "$RD/stdout.log")
echo -e "$RID\t$CFG\t$BHASH\t$IHASH\t$TS1\t$TS2\t$RC\t$FRQ\tbindlines=$AFFOK\t$RD" >> $RES/raw/results.tsv
echo "$RID"
EOF
chmod +x $RES/scripts/runrec.sh
```

- [x] **Step 6: 写 env_report.sh、probe_cpus.sh、fpstress.c**

```bash
cat > $RES/scripts/env_report.sh <<'EOF'
#!/bin/bash
# env_report.sh OUTDIR - node environment snapshot with timestamps
O=$1; mkdir -p "$O"; exec > >(tee "$O/env-$(date +%Y%m%dT%H%M%S).txt") 2>&1
echo "=== env snapshot $(date -Is) host=$(hostname) ==="
uname -a; cat /etc/os-release | head -2
echo "--- cpu model/revision ---"; grep -m2 -E 'model name|CPU revision|Midr' /proc/cpuinfo 2>/dev/null; head -1 /proc/cpuinfo
echo "--- topology ---"; lscpu | grep -E 'NUMA|Socket|Core|Thread|^CPU\(s\)'
for n in /sys/devices/system/node/node[0-9]*; do echo "$n: $(cat $n/cpulist) mem $(cat $n/meminfo | head -1)"; done
echo "--- online/offline ---"; cat /sys/devices/system/cpu/online; cat /sys/devices/system/cpu/offline 2>/dev/null || echo "no offline file"
echo "--- cgroup ---"; cat /proc/self/cgroup; cat /sys/fs/cgroup/cpuset.cpus.effective 2>/dev/null; cat /sys/fs/cgroup/cpuset/cpuset.cpus 2>/dev/null
echo "--- affinity ---"; taskset -pc $$
echo "--- governor/freq ---"
cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor | sort | uniq -c
awk '{if(mn==""||$1<mn)mn=$1; if($1>mx)mx=$1; s+=$1; c++} END{printf "cur_freq all-cpus min=%d max=%d mean=%d n=%d\n",mn,mx,s/c,c}' /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq
echo "--- thermal ---"; for z in /sys/class/thermal/thermal_zone*; do echo "$z $(cat $z/type 2>/dev/null)=$(cat $z/temp 2>/dev/null)"; done
echo "--- EDAC ---"; for mc in /sys/devices/system/mc*; do for f in $(find $mc -maxdepth 2 -name '*count*' 2>/dev/null); do echo "$f=$(cat $f 2>/dev/null)"; done; done
echo "--- load/mem ---"; uptime; free -g; df -h /home/share | tail -1
EOF
cat > $RES/scripts/probe_cpus.sh <<'EOF'
#!/bin/bash
# probe_cpus.sh OUTFILE - spawn one pinned probe per cpu 0..607, verify actual placement
O=$1; ok=0; bad=0
for c in $(seq 0 607); do
  r=$(taskset -c $c awk '{print ($39==c)?"OK":"BAD("$39")"}' c=$c /proc/self/stat 2>/dev/null)
  if [ "$r" = "OK" ]; then ok=$((ok+1)); else bad=$((bad+1)); echo "cpu$c $r" >> "$O"; fi
done
{ echo "probe_ok=$ok probe_bad=$bad at $(date -Is)"; } | tee -a "$O"
EOF
cat > $RES/scripts/fpstress.c <<'EOF'
/* fpstress.c - userspace FP busy loop for load-keeping on freq-excluded cores */
#include <stdio.h>
#include <stdint.h>
#include <time.h>
int main(void) {
    double a=1.0000000001,b=0.9999999999,x=0.5,s=0.0; uint64_t n=0;
    setvbuf(stdout,NULL,_IOLBF,0);
    for(;;){ for(int i=0;i<1000000;i++){ x=a*x+b; s+=x*0.5; } n++;
        if(n%1000==0) printf("fpstress ts=%ld n=%lu s=%.17g\n",(long)time(NULL),(unsigned long)n,s); }
}
EOF
chmod +x $RES/scripts/env_report.sh $RES/scripts/probe_cpus.sh
```

- [x] **Step 7: 语法验证（登录节点）**

Run: `for f in $RES/scripts/*.sh; do bash -n $f && echo "OK $f"; done`
Expected: 全部 OK（freqmon/freq_window/rankfile_window/rankfile_custom/runrec/env_report/probe_cpus）
Run: `awk -v c=5 '{print ($39==c)?"OK":"BAD"}' c=5 /proc/self/stat`（登录节点验证 probe 原理）
Expected: `OK`

- [x] **Step 8: 把 RES 路径记入本计划"战役参数"节并 commit（本计划文件自身首次提交）**

---

### Task 2: 环境作业 — 全新 dsub + 608 CPU 可用性验证 + 频率守护上线

**Files:**
- Create (cluster): `RES/scripts/job_env.sh`、`RES/logs/job.env.out`、`RES/environment/*`
- Create (repo): `docs/cases/cn23154-sdc-campaign-2026-09-23/P0-environment.md`

**Interfaces:**
- Consumes: Task 1 全部脚本。
- Produces: 首个 job_id；608 CPU 可用性证据（probe_ok=608）；频率守护 CSV 首批数据；队列 -T 上限探测结论。

- [x] **Step 1: 写 job_env.sh（作业脚本）**

```bash
cat > $RES/scripts/job_env.sh <<'EOF'
#!/bin/bash
#DSUB -n cn23154-sdc-env
#DSUB -q q_Test_20260903
#DSUB -nl cn23154
#DSUB -rpn 608
#DSUB -x job
#DSUB -T 1800
RES=$(cat /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/.last_campaign_dir)
echo "=== job start $(date -Is) host=$(hostname) ==="
echo "affinity: $(taskset -pc $$ | tail -1)"
taskset -c 0-607 $RES/scripts/env_report.sh $RES/environment
taskset -c 0-607 $RES/scripts/probe_cpus.sh $RES/environment/cpu_probe.txt
taskset -c 0-607 $RES/scripts/freqmon.sh $RES/environment 10 & FREQPID=$!
sleep 20; kill $FREQPID 2>/dev/null
ls -la $RES/environment/ | head; tail -3 $RES/environment/freq-*.csv
echo "ENV_JOB_DONE $(date -Is)"
EOF
```

- [x] **Step 2: 提交前检查 + 提交**

Run: `djob | grep -c cn23154` → Expected: `0`（无 RUNNING 才提交）
Run: `dsub -s $RES/scripts/job_env.sh` → 记录 job_id 与状态（PENDING/RUNNING）。

- [x] **Step 3: 后台轮询到作业结束（<=2 分钟输出节奏）**

```bash
# run_in_background Bash：
while true; do st=$(djob | awk -v j=$JOBID '$1==j{print $3}'); echo "$(date '+%T') job=$JOBID state=$st"; [ "$st" != "RUNNING" ] && [ -n "$st" ] && break; sleep 45; done
```
Expected: 数分钟后 state 离开 RUNNING，读取 RES/logs/job.env.out。

- [x] **Step 4: 核验 608 CPU 证据（真实输出摘录进 P0 报告）**

Run: `grep -E 'host=|affinity:|probe_ok' $RES/logs/job.env.out; grep -c '^.*,[0-9]*,' $RES/environment/freq-*.csv | head -1`
Expected: host=cn23154；affinity 为 0-607；`probe_ok=608 probe_bad=0`；freq CSV 行数 > 12000（20 秒 x 608 核 x ~1）。若 probe_ok<608：**立即停止后续任务**，保留证据，向用户报告缺哪些核，不得宣称 608 全覆盖。

- [x] **Step 5: 写 P0-environment.md（摘录真实输出：主机名、608 探针、拓扑、频率基线、EDAC、cgroup）并 commit + push**

---

### Task 3: 清点与源码理解

**Files:**
- Create (cluster): `RES/inventory/inventory.md`、`RES/inventory/hashes.tsv`、`RES/inventory/understanding.md`

**Interfaces:**
- Produces: 输出结构知识（供 Task 5 判定器）：程序打印哪些量、SCF 迭代行格式、最终判定量、每 rank 输出归属、确定性来源（MPI/OMP 归约次序）、已知 UB 风险清单。

- [x] **Step 1: 清点 + 哈希**

```bash
cd $MAT && find . -type f | sort > $RES/inventory/filelist.txt
sha256sum src/a.out lib/libomp.so Si.inpt Si.ion Si.psp8 local_run_rot.sh src/Makefile.xlsdft_920f src/Makefile.config.920f_lvtx > $RES/inventory/hashes.tsv
```

- [x] **Step 2: 源码理解（读以下文件并回答固定问题清单，写入 understanding.md）**

必读：`src/main.cpp src/scf.cpp src/chefsi.cpp src/eigen_solver.cpp src/tools.cpp src/args.cpp`（rg 定位打印点）。
固定问题（每条给出 文件:行 证据）：1) SCF 每迭代打印什么（格式串原文）？2) 最终收敛判据与打印量（能量/残差/本征值）？3) 哪些输出可归属到具体 rank/分区？4) 归约次序是否确定（MPI_Allreduce/OMP reduction 的使用点）？5) PRNG/未初始化/整型溢出/数据竞争风险点？6) PRINT_EIGEN/PRINT_DENSITY 落盘文件名？7) 崩溃时 MPI 报错是否含 rank 号？8) Si.ion 第二段 SCF 的输出前缀如何区分？

- [x] **Step 3: 判定初稿**（understanding.md 末节）：列出候选"正确性判据量"及其容差规则（供 Task 5 实现并按 Task 4 实测修正）。

---
### Task 4: 试点基线 — 满机原始绑定 x3

**Files:**
- Create (cluster): `RES/scripts/job_baseline.sh`、`RES/baseline/{base_ppr_1..3 的 raw 目录与 results.tsv 行}`

**Interfaces:**
- Consumes: runrec.sh；原始绑定 = local_run_rot.sh ppr 模式等价（--bind-to core --map-by ppr:1:numa:PE=36）。
- Produces: 实测单次时长（中位数）、失败形态/签名、失败率初步、RSS/PSR 采样、rank 归因信号有无 —— 这些参数决定 Task 6 超时与重复预算。

- [x] **Step 1: 写基线 rankfile 等价物与作业脚本**

```bash
# ppr 等价：rank r -> NUMA r 前 36 核（与 --map-by ppr:1:numa:PE=36 相同核集）
$RES/scripts/rankfile_window.sh 0 $RES/baseline/rf_ppr.txt
cat > $RES/scripts/job_baseline.sh <<'EOF'
#!/bin/bash
#DSUB -n cn23154-sdc-baseline
#DSUB -q q_Test_20260903
#DSUB -nl cn23154
#DSUB -rpn 608
#DSUB -x job
#DSUB -T 43200
RES=$(cat /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/.last_campaign_dir)
echo "=== baseline job $(date -Is) host=$(hostname) ==="
echo "affinity: $(taskset -pc $$ | tail -1)"
taskset -c 0-607 $RES/scripts/freqmon.sh $RES/environment 10 & echo $! > $RES/environment/freqmon.pid
for i in 1 2 3; do
  echo "--- baseline run $i start $(date -Is) ---"
  taskset -c 0-607 $RES/scripts/runrec.sh base_ppr_$i $RES/baseline/rf_ppr.txt 14400
  echo "--- baseline run $i done $(date -Is) rc见results.tsv ---"
done
kill $(cat $RES/environment/freqmon.pid) 2>/dev/null
echo "BASELINE_JOB_DONE $(date -Is)"
EOF
```

注：runrec 内已有 taskset 包装；此处外层再包一次以覆盖 mpirun 启动器本身。

- [x] **Step 2: 提交 + 后台轮询（Task 2 Step 3 同款循环）**，首跑完成后即 `tail` 该 run 的 stdout.log 观察 SCF 迭代行格式（Task 3 问题 1 的实证）。

- [x] **Step 3: 三跑结束后汇总实测参数（写入 RES/baseline/summary.md，真实数字）**

Run: `awk -F'\t' '{print $1,$2,$7,$8}' $RES/raw/results.tsv`（run/配置/rc/频率判定）
计算：单次时长中位数 T50；失败形态分类（exit code + stderr 特征）；错误签名（若结果错误：与另两跑逐量 diff 的首个分歧量）；rank 归因信号有无（stderr/stdout 中的 rank 号）。
Expected: 得到 T50、初步失败率 p_hat（0/3、1/3、2/3、3/3 四种情形各自的后继预算：若 0/3 通过且无异常 → 直接进 Task 6 并在报告注明"基线未复现，故障率上界 ~63%（3 次零失败）"；若 >=1 异常 → 按签名做 Task 5 判定器优先实现）。

- [x] **Step 4: 更新 status.md（基线结论 + 实测参数）**

---

### Task 5: 判定器 + GOLDEN 构造

**Files:**
- Create (cluster): `RES/scripts/extract_quantities.sh`、`RES/scripts/golden_build.sh`、`RES/scripts/check_output.sh`、`RES/scripts/classify.sh`、`RES/summary/golden.json`

**Interfaces:**
- Consumes: Task 3 问题清单答案 + Task 4 实测日志格式。
- Produces: `extract_quantities.sh RUN_DIR`（stdout: `name<TAB>value` 行）；`golden_build.sh RUN_DIR...`（写 golden.json：每量 median/max_dev/tol）；`check_output.sh RUN_DIR`（stdout: 每量 VERDICT 行 + 末行 `MISMATCH=n`）；`classify.sh RUN_DIR`（stdout: 单行分类标签，属于 correct/SDC/crash/hang/timeout/OOM/signal/affinity_failure/infra_failure/freq_grey）。

- [x] **Step 1: extract_quantities.sh — 按 Task 3/4 确定的真实格式提取判定量**

从 Task 4 的 stdout.log 实测格式出发（示例模板，正则按实测改，改动记录在脚本头注释）：
```bash
cat > $RES/scripts/extract_quantities.sh <<'EOF'
#!/bin/bash
# extract_quantities.sh RUN_DIR - correctness quantities from stdout.log
# FORMAT-NOTE: regexes verified against RES/raw/base_ppr_1/stdout.log on <date>
RD=$1; L=$RD/stdout.log
[ -f "$L" ] || { echo "NO_LOG"; exit 1; }
# 示例：SCF 迭代残差行与最终能量行（按实测格式替换）
grep -E 'iter|energy|Etot|eigen|conv' "$L" | tail -400
EOF
```
（Task 5 执行时用真实日志校准每条 grep/awk，产出稳定的 `name value` 流；此为本任务核心工作，须在脚本头注明校准样本 run_id。）

- [x] **Step 2: golden_build.sh — 多数共识 + 容差规则**

规则（spec 固定）：对每个量 q，取 N 次独立运行值的中位数 M_q；一致运行间最大偏差 D_q；容差 tol_q = max(10 x D_q, 1e-12 x |M_q|)；若 D_q=0 则 tol_q=0（逐位一致）。
```bash
cat > $RES/scripts/golden_build.sh <<'EOF'
#!/bin/bash
# golden_build.sh OUTJSON RUN_DIR... - per-quantity median, max-dev, tolerance
OUT=$1; shift
for d in "$@"; do $0/../extract_quantities.sh "$d" 2>/dev/null | awk -v d="$d" 'NF==2{print $1"\t"$2"\t"d}'; done \
 | sort -k1,1 -k2,2g \
 | awk -F'\t' '{key=$1; vals[key]=vals[key]" "$2; n[key]++}
   END{printf "{\n"; first=1;
     for(k in vals){m=split(vals[k],V," "); asort(V);
       med=(m%2)?V[int(m/2)+1]:(V[m/2]+V[m/2+1])/2; mx=0;
       for(i=1;i<=m;i++){d=V[i]-med; if(d<0)d=-d; if(d>mx)mx=d}
       tol=(10*mx>(1e-12*(med<0?-med:med)))?10*mx:1e-12*(med<0?-med:med);
       printf "%s\"%s\": {\"median\":%s, \"max_dev\":%s, \"tol\":%s, \"n\":%d}\n",(first?"":",\n"),k,med,mx,tol,m; first=0}
     printf "\n}\n"}' > "$OUT"
EOF
chmod +x $RES/scripts/golden_build.sh
```

- [x] **Step 3: check_output.sh + classify.sh**

```bash
cat > $RES/scripts/check_output.sh <<'EOF'
#!/bin/bash
# check_output.sh RUN_DIR GOLDEN - per-quantity verdict, last line MISMATCH=n
RD=$1; G=$2; mm=0; tot=0
Q=$($0/../extract_quantities.sh "$RD")
while IFS= read -r line; do
  [ -z "$line" ] && continue
  name=$(echo "$line" | awk '{print $1}'); val=$(echo "$line" | awk '{print $2}')
  gm=$(grep -o "\"$name\": {[^}]*}" "$G" | grep -o '"median":[^,]*' | cut -d: -f2)
  gt=$(grep -o "\"$name\": {[^}]*}" "$G" | grep -o '"tol":[^,}]*' | cut -d: -f2)
  if [ -z "$gm" ]; then echo "$name NEW_QUANTITY val=$val"; continue; fi
  tot=$((tot+1))
  d=$(awk -v a="$val" -v b="$gm" 'BEGIN{d=a-b; print (d<0?-d:d)}')
  bad=$(awk -v d="$d" -v t="$gt" 'BEGIN{print (d>t)?1:0}')
  [ "$bad" = 1 ] && { echo "$name MISMATCH val=$val golden=$gm tol=$gt"; mm=$((mm+1)); } || echo "$name ok val=$val"
done <<< "$Q"
echo "MISMATCH=$mm"
EOF
cat > $RES/scripts/classify.sh <<'EOF'
#!/bin/bash
# classify.sh RUN_DIR - one-line classification per taxonomy
RD=$1; RES=$(cat /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/.last_campaign_dir)
rc=$(awk -F'\t' -v r=$(basename $RD) '$1==r{print $7}' $RES/raw/results.tsv)
frq=$(awk -F'\t' -v r=$(basename $RD) '$1==r{print $8}' $RES/raw/results.tsv)
case "$frq" in EXCLUDED*|GREY*) echo "freq_grey"; exit 0;; esac
if [ "$rc" = "124" ] || [ "$rc" = "137" ]; then
  last=$(tail -1 $RD/stdout.log 2>/dev/null | head -c 80)
  # 超时前有迭代进展则 timeout，无进展 hang（判据记录于脚本头）
  echo "timeout_or_hang(last=$last)"; exit 0
fi
if [ "$rc" != "0" ]; then
  if grep -qiE 'out of memory|oom|cannot allocate' $RD/stderr.log 2>/dev/null; then echo "OOM"
  elif grep -qE 'SIGSEGV|SIGABRT|SIGBUS|SIGFPE|signal' $RD/stderr.log 2>/dev/null; then echo "crash_sig"
  elif grep -qiE 'affinity|binding' $RD/stderr.log 2>/dev/null; then echo "affinity_failure"
  else echo "infra_or_signal_rc$rc"; fi; exit 0
fi
mm=$($RES/scripts/check_output.sh "$RD" $RES/summary/golden.json | tail -1 | cut -d= -f2)
[ "$mm" = "0" ] && echo "correct" || echo "SDC"
EOF
chmod +x $RES/scripts/check_output.sh $RES/scripts/classify.sh
```

- [x] **Step 4: 用 Task 4 真实数据验证判定器**

Run: `$RES/scripts/golden_build.sh $RES/summary/golden.json $RES/raw/base_ppr_*`（仅 correct 类 run；若 3 跑全失败，GOLDEN 延后到 Task 6 首批通过 run 再建，先记录 blocker）
Run: `for r in $RES/raw/base_ppr_*; do echo "$r -> $($RES/scripts/classify.sh $r)"; done`
Expected: 分类结果与人工读日志一致（每类至少抽 1 个 run 人工核对，摘录进 status.md）。

---

### Task 6: P2 窗口覆盖扫描（全 608 核一级筛查）

**Files:**
- Create (cluster): `RES/scripts/queue.tsv`、`RES/scripts/job_sweep.sh`、`RES/cpu_sweep/cpu-results.csv`、`RES/summary/P2-sweep.md`
- Create (repo): `docs/cases/cn23154-sdc-campaign-2026-09-23/P2-window-sweep.md`

**Interfaces:**
- Consumes: runrec/classify/golden；W0/W1/W2 = rankfile_window.sh 偏移 0/1/2。
- Produces: 每核 >=2 个通过窗口的一级筛查表；失败窗口清单（Task 7 输入）。

- [x] **Step 1: 生成窗口 rankfile 与队列（rep 轮转交错：round1 W0,W1,W2 / round2 W0,W1,W2 + 首尾各 1 控制重复 W0）**

```bash
for o in 0 1 2; do $RES/scripts/rankfile_window.sh $o $RES/cpu_sweep/rf_w$o.txt; done
{ echo "ctrl_w0 $RES/cpu_sweep/rf_w0.txt 2"; for r in 1 2; do for o in 0 1 2; do echo "w$o_r$r $RES/cpu_sweep/rf_w$o.txt 1"; done; done; echo "ctrl_w0b $RES/cpu_sweep/rf_w0.txt 2"; } > $RES/scripts/queue.tsv
```

- [x] **Step 2: job_sweep.sh = 链式状态机驱动（读 queue.tsv，逐行执行未完成项，作业被杀可续）**

```bash
cat > $RES/scripts/job_sweep.sh <<'EOF'
#!/bin/bash
#DSUB -n cn23154-sdc-sweep
#DSUB -q q_Test_20260903
#DSUB -nl cn23154
#DSUB -rpn 608
#DSUB -x job
#DSUB -T 43200
RES=$(cat /home/share/suke/0903-NUMA3-report-materials-wangxu-v2/.last_campaign_dir)
echo "=== sweep job $(date -Is) host=$(hostname) ==="; echo "affinity: $(taskset -pc $$ | tail -1)"
taskset -c 0-607 $RES/scripts/freqmon.sh $RES/environment 10 & echo $! > $RES/environment/freqmon.pid
TMO=$(awk -F'\t' 'NR>1{print $6}' $RES/raw/results.tsv | sort -n | awk '{a[NR]=$1} END{print a[int(NR/2)+1]*3}')
[ -z "$TMO" ] && TMO=14400; echo "per-run timeout=$TMO"
while IFS=$'\t' read -r cfg rf reps; do
  [ -z "$cfg" ] && continue
  done_n=$(grep -c "^${cfg}_" $RES/raw/results.tsv 2>/dev/null || echo 0)
  while [ "$done_n" -lt "$reps" ]; do
    echo "--- $(date -Is) run $cfg ($((done_n+1))/$reps) ---"
    taskset -c 0-607 $RES/scripts/runrec.sh "$cfg" "$rf" "$TMO"
    done_n=$((done_n+1))
  done
done < $RES/scripts/queue.tsv
kill $(cat $RES/environment/freqmon.pid) 2>/dev/null
echo "SWEEP_JOB_DONE $(date -Is)"
EOF
```

- [x] **Step 3: 提交（若 -T 43200 被拒降到 21600/10800，记录实际接受值）+ 后台轮询；作业到期即续投（dsub 同脚本）直到 queue 全完成**

- [x] **Step 4: 汇总一级筛查表 cpu-results.csv**

```bash
# 每核聚合：covered_by_windows, runs_active, failures_active, freq_min, verdict_first_pass
for c in $(seq 0 607); do
  wins=$(for o in 0 1 2; do p=$((c%38)); [ $p -ge $o ] && [ $p -le $((o+35)) ] && printf "w$o "; done)
  echo "cpu$c numa$((c/38)) pos$((c%38)) windows:$wins"
done > $RES/cpu_sweep/coverage_map.txt
```
再把 results.tsv + classify 输出按窗口核集聚合（awk 完成，产出列：cpu_id,numa_node,pos,runs,failures,freq_min,verdict），verdict 规则：含失败窗口 → `implicated_by_w{...}`；全部覆盖窗口通过 → `clean_n={通过次数}`。

- [x] **Step 5: P2-sweep.md（repo + RES）— 真实数字：每窗口 rc/分类/频率判定、失败签名一致性、覆盖证明（608 核 x 窗口映射完整）、pass 数与上界表述。commit + push**

---

### Task 7: P3 归因确认（已适配：Task 6 psr 直证已将嫌疑集收敛为 S={cpu139}）

> 适配记录 (2026-09-24): 原计划的窗口差集收缩/16-rank 归因/二分循环不再需要——Task 6 的 13/13 崩溃全部经 psr 多数采样直证钉在 cpu139（含 W1/W2 follow-core 判据），S 已为单核。本任务改为两个决定性确认实验（映射原 Step 2/3 语义：rank 级归因 + S\S1 必要性）。

**Files:**
- Create (cluster): `RES/fault_localization/{rankfiles/rf_swap37.txt, rankfiles/rf_skip139.txt, attribution.md}`

**Interfaces:**
- Consumes: runrec.sh（cfg, rf, TMO）; classify.sh; results.tsv; freqmon.sh
- Produces: cpu139 归因终判 + skip139 首次完整运行数值档案

- [x] **Step 1: 嫌疑集收敛确认（Task 6 已完成）** — S={cpu139}（13/13 psr 直证，W0/W1/W2 follow-core 三重验证）；rf_swap37（rank3=266-301 NUMA7, rank7=114-149 含139）与 rf_skip139（rank3=114-138,140-149 共36槽不含139, 575活跃）已生成并验证（16行/576/575核/139归属）

- [x] **Step 2: swap37 决定性实验（follow-core 终判）×2 reps**

```bash
# rank3↔rank7 NUMA 交换：若崩溃签名 rank=7 且 cpu=139 → 故障完全随物理核+其所属 rank 窗口走（与 rank3 软件身份无关）
# 若 rank=3 且 cpu=NUMA7 某核 → rank3 软件身份参与；若不崩 → rank/NUMA 交互效应
printf 'swap37\t%s\t2\t1048\n' "$RES/fault_localization/rankfiles/rf_swap37.txt" >> $RES/scripts/queue_p3.tsv
```

- [x] **Step 3: skip139 必要性实验 + 首次完整运行 ×1 rep（TMO=10800，用户先验收敛 >2h）**

```bash
# cpu139 空闲：预期不崩 → 139 必要性确认 + 解锁完整运行数值核对（scf_error_iter_1==1.812e-01, 化学势==0.159497, 特征值全集, 收敛轨迹）
# 若仍崩 → 存在第二故障核/机制 → 转入 P4 全核扩展排查
printf 'skip139\t%s\t1\t10800\n' "$RES/fault_localization/rankfiles/rf_skip139.txt" >> $RES/scripts/queue_p3.tsv
```

- [x] **Step 4: 裁决与证据链** — 按 suspects.md §3 决策表判读 swap37（cpu139→硬件终判 / rank3-NUMA7核→软件参与 / 不崩→交互）；skip139（不崩→必要性+完整数值档案 / 崩→第二故障核）。写 attribution.md：每轮配置、results.tsv 行、classify 签名、裁决推理、频率判定。

- [x] **Step 5: 更新 status.md + attribution.md + repo 文档，commit + push**

---
### Task 8: P4 故障核确认（跟核 vs 跟 rank 判别 + 邻核 + 置信重复）

**Files:**
- Create (cluster): `RES/fault_localization/{confirm.md, rf_confirm_*.txt}`、`RES/summary/P4-confirm.md`
- Create (repo): `docs/cases/cn23154-sdc-campaign-2026-09-23/P4-confirm.md`

**Interfaces:**
- Consumes: Task 7 输出的嫌疑核 F（单个或极小集）+ 已洗清算池。
- Produces: 每个故障核的确认结论（失败次数/重复数/失败率/置信区间）+ 跟随性结论 + 邻核/缓存域结论。若 Branch B（无嫌疑核）→ 本任务记"不适用"并保留判定证据。

- [x] **Step 1: 确认重复** — 配置 = F 核放回其原 NUMA rank 的第 35 槽 + 其余 575 槽全干净核；重复运行至出现 >=3 次同签名失败或达 10 次（先到为准）。记录失败率与二项置信区间（Clopper-Pearson，用 awk 或 bc 算，公式写入 confirm.md）。

- [x] **Step 2: 跟核 vs 跟 rank 判别** — 把 F 核放进 rank (n+8)%16 的槽集重跑 2-3 次：故障仍现且签名同 → 跟核（硬件侧证据增强）；故障消失/签名变 → 跟 rank（软件/算法路径嫌疑，按任务规范重审 SDC 判定，如实报告）。反向对照：原 rank 用全干净核跑 2 次应通过。

- [x] **Step 3: 邻核与缓存域** — 读 `/sys/devices/system/cpu/cpu{F}/cache/index*/shared_cpu_list` 定 F 的 L1/L2/L3 共享域；对 F+-1, F+-2 与同 L3 域代表核各跑 1-2 次（同 Step 1 配置形状）。

- [x] **Step 4: P4-confirm.md（repo + RES）：结论四分栏（已证实/强相关/未证实/已排除）+ 每项证据的 run_id 指针。commit + push**
**适配记录（Task 8，2026-09-24 执行时）：**
- Step 1 停止条件（>=3 次同签名失败）在历史跑 base_ppr_1_20260923T230306 即达成，至 P3 收敛已累积 15/15 同签名失败（自然位形 13 + swap37 换域 2）——判据由既有数据满足，本任务未新增 XLSDFT 运行，故 rf_confirm_*.txt 未创建。Clopper-Pearson 公式与 bc 实算值见 P4-confirm.md §1（15/15 → [78.20%,100%]）。
- Step 2（跟核 vs 跟 rank）已被 Task 7 证据直接回答：swap37 域级交换证明故障跟物理域不跟 rank 身份；skip139 + W1/W2 psr 将域内故障精确定位到 cpu139——(n+8)%16 重排实验的回答空间为空，不再重跑。
- Step 3 邻核运行证据由 P2 同跑对照覆盖：137/138/140/141 与 139 的 10 次归因跑是同一批 mpirun 跑（同为 rank3 窗口成员）且全部 exercised_clean，无需另跑；/sys 拓扑已取证（dsub job 1712838 → RES/fault_localization/topology_139.txt；本节点 sysfs 不导出 L3/index3，最大共享粒度 = cluster_cpus_list 114-151，L1d/L1i/L2 全私有）。
- Step 4 产出定于 RES/fault_localization/P4-confirm.md（与 attribution.md 同目录保持取证链一处可溯；其 §1 已含 CP 公式与实算值，等价于计划中 confirm.md 的内容要求）。

---

### Task 9: P5 微架构诊断（systematic-debugging + 双对照）

**Files:**
- Create (cluster): `RES/diagnostics/{bench/, perf/, bitsig/, diagnosis-report.md}`、`RES/scripts/bench_units.c`

**Interfaces:**
- Consumes: 确认故障核 F、对照核 C1（同 NUMA 邻核）、C2（远端 NUMA 核）。
- Produces: 诊断报告（微架构级根因假设 + 证据链 + 置信分级 + admin_requests 条目）。

- [x] **Step 1: 调用 superpowers:systematic-debugging skill** 并按其流程执行本任务全部步骤（四阶段：复现控制 -> 假设 -> 实验 -> 结论），本计划步骤作为其实验清单。

- [x] **Step 2: bench_units.c — 执行单元/SIMD/数据模式微基准（逐位比对，数千次迭代）**

```c
/* bench_units.c - per-execution-unit microbench with bit-exact self-check
 * build: gcc -O2 -mcpu=host -fopenmp bench_units.c -o bench_units
 * usage: ./bench_units <unit> <iterations> ; units:
 *   ialu imul idiv fadd fmul vfma_sve vfma_neon ldst ldst_stride br shuffle
 *   cvt atomic ; data patterns: zero ones walk random extrema denormal
 * 每单元：内核 K 次迭代产出 64-bit 校验和；同输入跑 R 轮，轮间逐位比对；
 * 输出: unit pattern round checksum delta_bits(first mismatch)
 */
```
（执行时写全内核实现；每单元至少 INT/FP/SVE 通路各一，lane 逐个验证：SVE 用 VL=128 拆 lane；denormal 用最小正规数邻值。）

- [x] **Step 3: 三核同条件执行矩阵** — F/C1/C2 各跑 units x patterns x >=3000 轮（每轮 checksum 比对），taskset 单核绑定；任何 delta_bits>0 记录完整位模式进 bitsig/。

- [x] **Step 4: 缓存层级定位** — 工作集尺寸扫 {4KB(L1) 64KB(L2) 32MB(L3) 512MB(远端)} x 访问模式 {seq,stride64,random} x 数据位置 {本地 NUMA, 远端 NUMA(numactl)}，F vs C1；错位首次出现的工作集尺寸 → 层级结论。

- [x] **Step 5: perf 采集**（先 `perf list` 确认事件存在，仅自有进程）: instructions/cycles/IPC/branches,branch-misses/cache-{ref,misses},L1-dcache,stalled-cycles; F vs C1 差异只作相关性记录。

- [x] **Step 6: 位签名映射** — XLSDFT 错误输出的错位模式（Task 5/8 记录）映射到源码计算路径（rg 定位该量在 src/ 的产生式）+ objdump 热函数（perf 定位）指令类别。

- [x] **Step 7: 频率/温度相关性** — 从 freqmon CSV 提取故障时刻前后 F 核频率轨迹；对照温度区间；只做观察式结论。

- [x] **Step 8: diagnosis-report.md（repo + RES）：微架构根因假设（定位到模块：如"某执行单元在 fmax 附近间歇时序故障"/"某缓存阵列位固化"）、置信分级、被排除备选、频率温度依赖、一键复现命令、处置建议；受控调频/核隔离写入 admin_requests.md。commit + push**

---

**Task 9 服务进度适配记录 (2026-09-23/24, 已完成)**:
- Step 1-3 完成: bench_units 15 单元x8 图案矩阵 (job 1713002) + SVE-512 FMA hammer/sweep (rdvl 修复后, job 1713658) — F(139)/C1(140)/C2(252) 全部干净, bitsig 为空。
- Step 4 完成 (适配: 以 bench_mem l1fill/l2stream/bigstream/svestream 交叉校验代替纯时延测量, job 1713774): 每单元 ~300 亿字校验全绿 — 足迹覆盖排除 (L1 fill / L2 阵列 / DRAM+TLB 512MB / SVE 读出)。
- Step 6 完成: p5-crashmap.md — 15/15 byte6(bits48-55) 随机垃圾 + 低 48 位完好; 崩溃簇覆盖 SVE ld1d/ld1b、SME za st1d、libomp 纯标量 — 指令类型排除。
- 新增 H-droop (job 1713961) 与 H-dwell (job 1714041) 实验: 全核并发燃烧 + 139/140 双测量 (l2stream 交叉校验), park_l1/park_l2/park_reg 驻留 x {单独, +34 燃烧核} — 两假设均排除 (第 8/9 类)。
- Step 5 完成 (perf 差分, job 1714017): perf stat -t <tid> 线程跟随 + :u 事件 (paranoid=2 下唯一可行); run_A 崩溃线程 tid=286183 恰为测量线程, 139 vs 140 六指标差 <=3% 且在复测波动带内 → 事件型故障。run_B 设计失误 (slot=115-150 仍含 139, P3 §0 陷阱重犯) 计作复现 + 140 复测, 差分仅立基于 run_A。
- Step 7 完成: 17 次崩溃均 1998-2000MHz 满频, 139/140 全程 >=1996MHz; >=1600MHz 复现 / <=1550MHz 零复现 (P2 限频实验); 无 CPU 专属 thermal_zone, 满载 58.0°C vs 空闲 49-50°C — 非热致。
- Step 8 完成: RES/diagnostics/diagnosis-report.md (权威) + repo docs/cases/cn23154-sdc-campaign-2026-09-23/P5-diagnosis.md (scp 直传); 发布前溯源核验自捕 3 处错误并修正 (虚构 repro 子命令 → 实测接口; 地址分区 12/5 → 11/6; P5 崩溃时刻改为 perf time-enabled 实测 281.3s/191.2s)。
- 结论: cpu139 核私有 byte-lane-6(+7) 边际弱单元, 事件型 (perf 零计数器异常), 上下文依赖 (9 类合成负载全排除), 频率依赖; 具体阵列 (L1d/L2 vs LSU vs PRF) 未分离 → 移交 Task 10。
- 编码事故披露: 本记录块的前身 (2026-09-23 中途版本) 被 PowerShell 追加时双重编码损坏并随 commit 0a6823a5 提交; 本块为按可查证事实 (job ID/结果/status.md) 重写的终态记录, 中途过程细节以集群 RES/status.md (heredoc 写入, 无损) 为准。
### Task 10: P6 最小化复现提取（条件：已确认故障核）

**Files:**
- Create (cluster): `RES/minimization/{versions/, results.md, minimal-reproducer/}`

**Interfaces:**
- Consumes: 故障核 F + 原始可复现配置。
- Produces: 最小用例（源码/构建/运行/输入/校验/绑定全自包含 + 稳定复现率 + 对照核等量测试）。

- [ ] **Step 1: 缩减维度逐个实验（每步只改一个因素，A/B 验证：原版仍复现 + 缩减版失败率对比而非是否曾失败）**：顺序 = 1) 去 Si.ion（单段 SCF）；2) MAXIT_SCF 下调到故障迭代+余量；3) FD 网格缩小（Si.inpt 拷贝改 FD_GRID，记录每档）；4) NSTATES/XLSDFT_NSTATES 缩减；5) NT 重编译递减（36->18->9->4->2->1，全参数留痕、原 a.out 不动）；6) NCOMMS 降 rank 数（若源码支持）。
- [ ] **Step 2: 每版本在 F 核重复 >=5 次、在 C1 >=5 次，失败率表格 + 版本目录（源/构建/输入/日志）**
- [ ] **Step 3: 定稿 minimal-reproducer/：README（构建+运行+判定+预期现象+统计复现率）、自包含输入、校验脚本、绑定命令；由另一研究者可从零复现的自检清单**
- [ ] **Step 4: 若全程无确认故障核（Branch B 延续）：不编造最小用例，报告"未获得稳定触发条件"+ 已达证据强度（spec 强制）**

**适配记录（Task 10，2026-09-24 执行时）：**

- **因子 1 "去 Si.ion（单段 SCF）" 字面不可行（源码实证）**：`src/preparation.cpp:47` 无条件读 `Si.ion`（`this->ion_file.read(fname + ".ion")`），程序无免 ion 运行模式（`local_run_rot.sh` 亦无）；且"单段"（XLSDFT_NELEMS 1 1 1）在全晶胞下内存不可行。适配为**分段维度缩减**（XLSDFT_NELEMS 部分缩减），移到输入参数类因子之后执行。
- **执行顺序按可行性重排**：MAXIT_SCF（V1，job 1714310）→ FD_GRID → NSTATES/XLSDFT_NSTATES → NT 重编译递减 → NCOMMS → 分段缩减（原因子 1 适配版）。"每步只改一个因素"按**累进制**执行（V2 = V1 + 网格缩减），每版本与原版/上一版 A/B 对比失败率，不作"是否曾失败"判定。
- **新增工具**：`RES/scripts/runmin.sh`（变体输入 runner，运行逻辑同 runrec.sh，运行目录与结果表移至 `RES/minimization/`）；版本输入目录 `RES/minimization/versions/<vid>/Si.inpt`。
- **V1 设计（MAXIT_SCF 1000→3）**：依据 = 17 例崩溃全在 SCF iter 1-2（eigen 簇 87-140s = iter1 中段 density_matrix_solver；stencil/libomp 簇 345-355s = iter1 完成/iter2 起点），3 次迭代覆盖窗口且有余量，同时把无崩溃运行封顶 ~10 分钟。作业 1714310（cn23154-sdc-min-v1）：orig_F 锚 ×2 + v1_F ×5 + orig_C1 锚 ×1（TMO 1200 = 3× 故障窗口）+ v1_C1 ×5；原版基线另有历史 17/17。MAXIT_SCF 只截断循环次数，iter 1-2 时间线与原版逐位相同 → 预期不改变复现，作用是给后续因子提供短运行载体。
- **V1 结果（2026-09-24 10:47 收官，job 1714310）**：F 5/5 崩（101-350s，全 rank3，byte-6 垃圾值 5/5 与历史 17 例签名一致）；C1 5/5 净（798-807s 自然完成，orig_C1 锚 1201.6s cap 无崩）；12/12 运行窗 cpu139/140 min=1998 MHz 全频。结论：MAXIT_SCF 1000→3 对复现中性，V1 成为后续阶梯短载体。runmin.sh 已参数化 AOUT/NT_THREADS（NT 重编译 rung 用）。
- **V2 收官（job 1715007，2026-09-24 11:50 结束）：FD_GRID 网格缩减中性**。F 5/5 崩（89-171s，byte-6 签名 5/5，全 rank3）+ C1 5/5 净（356.6-361.6s，五 rep 物理量逐位一致 1.822e-01/1.518e-01/1.856e-02）+ 频率 12/12 窗全频（cpu139 min=1999/cpu140 min=1997）。v2d 收官（job 1716505）：F 5/5 + C1 5/5 全部 (nil) 空指针崩（23-30s，comm1 齐崩，迭代前）——首次 C1 臂崩；同作业 v2F 锚仍 byte-6 崩（0xa1aaaaf9513400）+ v2C1 锚净（rc=0, 357s）→ 判为程序自身 null-deref bug（臂无关），非 SDC。**网格轴 floor = V2（160 120 120），v2dd 作废**。频率语义修正：聚合判决 31 行全 GREY 系 rankfile 576 pin 核中 ~10-15 个非故障核负载下钳 1501-1744 MHz；归因核 cpu139/140 33/33 窗全 ≥1997 MHz；用户红线 <1500 阶梯期零触发（历史触发已按 runs_freq_excluded/mixed_freq_clean 记账于 cpu-results.csv）。V3 收官（job 1716614）：F 5/5 + C1 5/5 全部 rc=65-67 确定性应用层失败（LVTX dsygvd_upper status=321/323 m=320 非 SPD/非收敛；TOL_PSEUDOCHARGE 9.34e-13>5e-13）——320 态低于数值下限，与 SDC 无关（同作业 v2F 锚 byte-6 崩 0x98aaaae78eee40 + v2C1 锚净 + 12/12 窗 cpu139/140 全频）。**NSTATES 轴关闭：640 必要（物理下限）；阶梯基底保持 V2**。V4-NT18（NT 36→18 重编译，job_min_v4_nt18.sh 已修补：Vx=v2 基底 + 前驱锚（原版 NT36 二进制 F/C1 各 1）+ V4_BUILD_FAILED 守卫；rankfile rf_nt18_F rank3=122-139（thread17→139）/rf_nt18_C1 rank3=122-138,140（thread17→140）；TMO F 1800/C1 2400）已提交（job 1716728，12:26 起）——**V4 收官（13:27）：NT 36→18 中性** — F 5/5 byte-6 崩（0x94/0x14/0x62/0x76/0x76，全 rank3，67-98s，全 libomp 簇）/ C1 5/5 净（325-328s，SCF error 五 rep 逐位一致且同 NT36 值 = 重编译不改变数值路径）/ 锚对完整（v2F 崩 0xa1_aaaad0977440、v2C1 净 354s）/ 逐窗 cpu139/140 1999-2000MHz。NT18 binhash 6b336a92141c1e54（builds/nt18_src）。红线记录：cpu17（min 1485）/cpu42（min 1487）越线（rank0 非归因核，记录在案不研究）。**V4B-NT9 已投（job 1717122，13:28 起，构建期 ~40min）**：NT 18→9，rf_nt9_F rank3=131-139（thread8→139）/C1=131-138,140，前驱锚 + TMO F 2700/C1 5400。NT9 判定后转 NCOMMS 轴：8-rank 设计已源码实证定稿（col-major rank=i+(j+k*nj)*ni，NCOMMS 2 2 4→2 2 2 时新 rank3 (1,1,0) 空间区域 ⊇ 旧 rank3 区域；rankfile = NT 终版 F/C1 前 8 行；v5_ncomms222 输入单行 diff 已验证）。NT 深度递减（4/2/1）将视 NT9 结果与 C1 时长增长（NT18 C1 325s ≈ NT36 357s 未倍增——时延-bound 而非计算-bound，深 rung 时长风险低于预估）评估是否继续。
- **V4B-NT9 收官（job 1717122，2026-09-24 15:19）：NT 18→9 中性 + 检测几何发现，NT 轴收敛于 9**。F 5/5 失败且表现谱扩展：byte-6 2（F_3 0x39_aaaaf47d0400 / F_5 0xeaaaaaf05c0880，崩溃线程 15/15 与 10/10 pin cpu139——首次线程级崩溃归因，psr.tsv 直证）+ canonical 野指针 2（F_1 0x4000091a0300 @laplacian_4d_impl<16,false> OMP outlined +0x258f00——stencil 家族首次最小化复现；F_4 0x400053fff480 只读页 @tridiagonal_ql_640_parallel OMP outlined +0x2c4f68←tridiagonal_ql_rows +0x2d0ce4——崩于副本 QL 内部）+ **数值 SDC 1（F_2 rc=233：LVTX_DSYGVD_REPLICA_MISMATCH worker=8=cpu139，d[0] 参考值 -0x1.5d3a224ccba6ep-3 vs 实际 -0x1.5d3a224ccba8fp-3，低 8 位 xor=0xe1 / 4 bit 翻转 / 33 ulp，worker 1-7（cpu132-138）先全过 1280 项 memcmp = 8 核 bit-identical 共识——战役首次非崩溃直接数值 SDC 捕获）**。C1 5/5 净（808-813s，SCF error 1.822e-01/1.518e-01/1.856e-02 同 NT36/NT18 逐位；0 LVTX）。锚对完整（v2F 崩 0xa1_aaaadfef1140 170s / v2C1 净 356s）。逐窗 12/12：cpu139 1997-2000 / cpu140 1998-2000。NT9 binhash 763c6843f504e1f7（builds/nt9_src）。红线：cpu487（min 1451）/cpu510（min 1494）越线（非归因核，记录在案不研究）。**检测几何（源码实证，lvtx_kml_dsygvd_detail.hpp:571-660 + lvtx_dsygvd_internal_config.hpp:9）**：tridiagonal_ql_640_parallel 令每个 OMP worker 在相同 memcpy 输入上跑逐位相同的私有 d/e 递推（热循环无同步），结束后与 worker 0 权威态逐字节 memcmp；但 replica worker 数 = min(threads, ceil(640/tile))，production_tile=64 → 上限 10 worker——NT36/18 时 cpu139 线程号 25/17 从未进入被校验递推，NT9 时 rank3=131-139 → cpu139=worker 8 首次参与 → 数值偏差首次被直接捕获、同函数崩溃首次出现。表现谱变化 = 检测几何变化而非故障性质变化；非软件伪象三重证据：worker 1-7 先全过（8:1 核间共识）+ 同几何健康对照 C1（cpu140=worker8）5/5 全净 + 校验确定性。**NT 深度递减停止**：4/2/1 每 rung C1 代价 ~2×（NT9 C1 808-813s 已现倍增）而边际价值低。**v5-NCOMMS 已投（job 1717908，15:23 起）**：NCOMMS 2 2 4→2 2 2（16→8 ranks），NT9 基座（cpu139=worker8 检测器保留），新 rank3 (1,1,0) ni 5-9/nj 4-7/nk 0-3 ⊇ 旧 rank3 区域、元素 40→80；rankfile rf_nc8_F rank3=131-139 / rf_nc8_C1 rank3=131-138,140（NT9 终版前 8 行）；v5nc8_F ×5（TMO 1800）+ v5nc8_C1 ×5（TMO 2400）+ v4bnt9 前驱锚 F/C1 各 1；望 21-22 时收官。
- **v5 attempt-1 基础设施失败 + v5b 重投（2026-09-24 15:52）**：job 1717908（15:23-15:43）锚 2/2 有效（v4bnt9F_anchor_1 rc=139 257s，0x18_aaaae85fad40 byte-6 lane6+5-4 同损 rank3；v4bnt9C1_anchor_1 rc=0 818s，SCF error 序列与 NT9 作业逐位一致——V4B 基座在 v5 窗口仍复现的直接证明），但 v5nc8_F/C1 ×10 全部 rc=1 各 ~5s——runmin.sh 硬编码 mpirun -np 16 vs rf_nc8_* 只有 8 行，OpenMPI 启动前校验拒绝（"A rank is missing its location specification: Rank: 8"）。修复：(a) runmin.sh -np 参数化 ${NPROCS:-16}（默认 16，既有 16-rank 调用方零变化，bash -n 过）；(b) job_min_v5b_ncomms.sh（job 1718129，15:48 投）：10 处 v5nc8 调用 NPROCS=8、锚 2 处保持默认 16；(c) 新增 120s canary 守卫（v5nc8_canary：验证 8×RANK_BIND 且 rc∈{124,139,233} 方继续，否则 V5B_CANARY_FAILED 早退）。attempt-1 十行 rc=1 保留 results.tsv 记账（诚实），判读标 infra-invalid；红线新增 cpu58（min 1481，非归因核记录在案）。
- **v5b 收官 — v5 rung 终判 INFRA-INVALID，NCOMMS 轴止于 16（2026-09-24 16:57）**：job 1718129（15:48-16:56）canary rc=124 bind=8（NPROCS 修复实证）；锚 2/2 有效（F 锚 rc=139 **49s** 0xab_aaaad4d41b80 byte-6 rank3 [1,3]；C1 锚 rc=0 810s 3 轮 SCF 完整）；v5nc8_F ×5 + v5nc8_C1 ×5（rank3=131-138,140 无 cpu139）**10/10 全部 rc=134 ~304-306s**——8-rank 世界系统性死锁于 Preparation::init Geometry::bcast（5120 原子大对象）UCX/RoCE（roceroh0-3）部分投递。诊断证据链：栈帧分歧 GEO{0,1,4,6,7}/CTL{2,3,5} 在 F/C1 双臂逐位一致（确定性传输拓扑故障，addr2line +0x43cb0=Geometry::bcast / +0x1d0d40=Control::bcast）；卡死组五 rank 绑核均不含 cpu139；rank3 超时对端 GID 与健核 rank2/5 相同（18570b01）；geometry.cpp:502 纯直线 2×MPI_Bcast 无 rank 条件分支（排除应用层逻辑分歧）；~305s=UCX UD 默认超时（bcast 起步即停）；同作业 16-rank 锚秒级过 init 且 F 锚 49s 复现 byte-6 SDC 崩溃——与 SDC 无关的证据闭环。**NCOMMS 不可作最小化轴继续（该 fabric 上 8-rank 不可用）——集群基础设施限制，非科学结论。** 频率全 VALID（min=1996），作业窗零红线核。**v6 分段轴 16-rank 重设计**：原预研 v6a（NCOMMS 2 2 2=8-rank）会踩同一传输故障，全梯改 NCOMMS 2 2 4——v6a' NELEMS 10 8 4（320 元素，rank3 40→20 ⊂ 原 40）、v6b' 10 4 4（160，10/rank）、v6c' 8 4 4（128，8/rank）、终点 NELEMS 2 2 4（16 元素，1/rank）；每元素网格恒 ≥16×15×15（V2 floor）；v6a 工件已改（Si.inpt NCOMMS 行 + rf_nc16_F/C1 复用 v4b_nt9 16-rank，rank3=131-139/131-138,140；vs v2 diff 恰 CELL z/FD_GRID z/NELEMS nk 3 参数行+注释）；runmin.sh ION 参数化（2562 原子 Si.ion）后提交 job_min_v6a_nelems1084.sh（canary 守卫升级：16×RANK_BIND + FD_GRID 配置回显 ≥1 证 init 集体通信通过 + rc∈{124,139,233}）。
- **v6a 收官 — 空间 z 减半中性（2026-09-24 18:15）**：job 1720115（17:02-18:09）13 阶段全走完：F 侧 **7/7 崩**（canary 46s + F_1-F_5 39-171s + F 锚 256s，全 rank3，崩溃线程 psr 7/7 全 cpu139；byte-6 heap 4 [0x40/0x0a/0x06/0x33] + **byte-6 @0x4000-region 2 [F_3 0xb3 / 锚 0x9e — 损坏不限于 heap，0x4000 mmap 区亦中招 = 地址生成路径损坏]** + canonical 1 [F_2 0x400043fff400 ACCERR]；崩溃点 libomp 簇，SCF loop 1 内）/ C1 侧 **6/6 净**（锚 811s + 5 rep 434-437s，rc=0，scf error 6.429e-01/4.748e-01/2.320e-01 五 rep 一致，LVTX replica 0 失配）。几何：元素 640→320、原子 5120→2562、rank3 40→20 元素（⊂原 40）、每元素网格 16×15×15 与每元素负载逐位不变——复现率不降 → **SDC 复现与空间规模解耦**。频率：F 侧 7 窗 cpu139=1999/cpu140=1998 合规；聚合 EXCLUDED 系 cpu108（min 1493，rank2 slot 76-111 非归因核）污染，按续10 逐核分解规则判读；作业窗无其他红线核。**全精度能量非确定性更正**：判读中首次全精度比对 E3 发现 v6a C1 五 rep 互差 ≤1.35e-6；追溯历史 v2 输入 C1 复跑（v4bnt9_C1_1-5、v4nt18_C1_1-5 同作业背靠背）E3 一直互差 3-9e-3——**应用内在非确定性**（负控臂无 cpu139 亦然，疑 OMP 调度序敏感 FP 累加），全历史存在、非新硬件事件；历史"SCF error 逐位一致"实为 4 位打印精度比对（该口径成立）；V4B F_2 数值捕获不受影响（LVTX replica 源码验证逐字节 memcmp，免疫调度序），全部梯级判定基于崩溃率+replica 失配，均不变。已记 results.md 更正条 + status.md 续11。**v6b（nj 8→4）已提交**：job 1720586（18:11:03）——NELEMS 10 4 4（160 元素，10/rank）、CELL y 82.296846784→41.148423392、FD_GRID 160 60 60、Si.ion 2562→1277 原子（y<41.148423392 过滤，kept maxY 41.1472/dropped minY 41.1522）、rankfile 复用 rf_nc16、canary 守卫同 v6a；工件 versions/v6b_nelems1044/（inpt diff vs v6a 恰 4 行）、脚本 scripts/job_min_v6b_nelems1044.sh。
- **v6b 收官 — 空间 y 减半中性（2026-09-24 19:05）**：job 1720586（18:11-19:04, MIN_V6B_JOB_DONE）13 行：F 侧 **7/7 崩**（canary 55s + F_1-F_5 84-166s + F 锚 39s，全 rank3，崩溃线程 psr 7/7 全 cpu139 [psr.tsv 尾采样口径]；byte-6 heap 1 [F 锚 0xac] + **byte-6 @0xaaab 高址区 3 [canary 0xe6/F_2 0xa9/F_4 0xee — byte-6 目标区扩至 0xaaaa/0xaaab/0x4000 三区]** + canonical 3 [F_1/F_5 野指针 MAPERR + F_3 0x40004bfff840 ACCERR；谱形漂移 3/7 记录在案]；崩溃点 libomp 顶层 4 + stencil +0x258f00 2 + QL +0x2d0ce4 1）/ C1 侧 **6/6 净**（v2 锚 814s + 5 rep 319-323s，scf 7.163e-01/5.337e-01/2.692e-01 四位打印一致，replica 0 失配，E3 展幅 1.78e-7）。频率 cpu139/140=1999 全 12 窗；聚合 EXCLUDED 系 cpu236/39 整窗 1459（负载相关降频槽位核，非归因）。**判定：空间规模继续解耦，转 v6c ni 10→8**（job 1721020 19:07 起，NELEMS 8 4 4 / 128 元素 / 1023 原子 / rank3 8 元素，工件 versions/v6c_nelems844/）。同场自捕修正两处入 P6 过程日志：v6a "崩溃点 libomp 簇" 全量回溯改 4/7+2/7+1/7；psr 取尾采样规则固化。

---

### Task 11: 最终交付与收尾

**Files:**
- Create (cluster): `RES/{README.md, environment-report.md, experiment-plan.md, final-summary.md, summary/admin_requests.md}`；更新 `RES/status.md` 终态
- Create (repo): `docs/cases/cn23154-sdc-campaign-2026-09-23/final-report.md`

- [ ] **Step 1: RES/README.md** — 构建/运行/复现/全扫/报告生成方法、权限、已知限制。
- [ ] **Step 2: environment-report.md / experiment-plan.md** — 从 Task 2 快照与本计划整理（假设、自变量、对照、重复策略、停止条件、SDC 判定标准）。
- [ ] **Step 3: final-summary.md — 任务规范 12 问逐答**（哪些核稳定复现/失败率与置信区间/异常 NUMA 其他核/其余核/608 全覆盖集合相等证据/与 physical core-SMT-NUMA-socket-cache-内存的相关性/最可信微架构假设/支持证据/削弱反例/最小用例/最小用例复现率/当前结论限制），每答带 run_id 证据指针。
- [ ] **Step 4: admin_requests.md** — 全部需管理员的精确命令 + 用途。
- [x] **Step 5: 收尾核验** — `djob` 确认无本战役遗留 RUNNING（有则 dkill）；`ls RES/raw | wc -l` 与 results.tsv 行数一致；cpu-results.csv 行数 = 608 + 窗口聚合数；status.md 终态。
- [ ] **Step 6: repo final-report.md（12 问中文摘要 + RES 指针）commit + push**

---

## 战役参数（执行中回填）

- RES=/home/share/suke/0903-NUMA3-report-materials-wangxu-v2/ai-run-cn23154-20260923-220641（Task 1 Step 1 建立）
- 首个 job_id=1710250（Task 2，已完成，probe_ok=608）
- 队列实际接受 -T=（Task 2/6）
- 实测单次时长 T50=（Task 4）
- 基线失败率=（Task 4）
- 失败窗口 / 嫌疑核 / 确认故障核=（Task 6/7/8）

## Self-Review 记录

- spec 覆盖：§4 方法论 -> Task 6/7/8；§5 P0-P6 -> Task 2,3,4/5,6,7,8,9,10,11；§6 频率闸门 -> Task 1(freqmon/freq_window)+各 runrec 内建；§7 运维 -> Task 1/2 作业框架+全局约束；§8 判定 -> Task 5；§9 诊断 -> Task 9(+Task 7 Step 4 零失败分支)；§10 交付 -> Task 11；§11 风险 -> 各条件分支；§12 诚实性 -> 全局约束。无缺口。
- 占位符：Task 5 Step 1 extract_quantities 为"按实测格式校准"型模板（含校准责任与记录要求），非 TBD；Task 9 Step 2 bench_units.c 为规格+构建参数（实现属该任务工作）；其余步骤均含具体命令/代码。
- 类型一致性：脚本名与签名在 Task 1 Interfaces 定义，Task 5/6/7 调用一致（runrec.sh CONFIG RF [TMO]、freq_window.sh CSV T1 T2 CPULIST、rankfile_custom.sh OUT R{n}=...）。
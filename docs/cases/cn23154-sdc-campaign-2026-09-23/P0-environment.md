# P0 环境作业：cn23154 全新作业 + 608 CPU 可用性验证 + 频率守护上线

- 日期：2026-09-23 22:10-22:12
- 作业：dsub 1710250（q_Test_20260903，-nl cn23154 -x job -rpn 608，#DSUB 头见 RES/scripts/job_env.sh）
- 证据：RES/logs/job.env.out、RES/environment/{env-20260923T221117.txt, cpu_probe.txt, freq-20260923.csv}

## 608 CPU 可用性（全部通过）

作业输出原文摘录：

```
=== job start 2026-09-23T22:11:17+08:00 host=cn23154 ===
affinity: pid 3648939's current affinity list: 37,75,113,151,189,227,265,303,341,379,417,455,493,531,569,607
CPU(s): 608  Thread(s) per core: 1  Core(s) per socket: 304  Socket(s): 2  NUMA node(s): 32
NUMA node0 CPU(s): 0-37 ... NUMA node15 CPU(s): 570-607（16-31 为 HBM-only 域，无 CPU）
probe_ok=608 probe_bad=0 at 2026-09-23T22:11:26+08:00
```

- 608 颗 OS 可见 CPU 全部可各起一个绑核探针且实测落位一致（probe_ok=608, probe_bad=0）
- 拓扑：2 插槽 x 304 核、无 SMT（Thread(s) per core: 1）、DDR NUMA 0-15 每域 38 核连续
- 16 核亲和性陷阱再次实证（默认 affinity 37,75,...,607）—— 一切负载已按规 taskset -c 0-607 包装
- cpuset/cgroup、offline 检查通过（/sys/devices/system/cpu/online=0-607，无 offline）

## 频率状态与首批异常发现

```
608 userspace
cur_freq all-cpus min=1461862 max=2000006 mean=1992802 n=608
```

- governor 全部 userspace、setpoint ~2GHz；均值 1992.8MHz 正常
- **首批 20 秒采样即发现 10 颗核持续低于 1800MHz（两拍读数一致）**：
  - **cpu497（NUMA13）= 1461MHz < 1500MHz 硬排除阈值 → 移出 SDC 筛查、仅保压力负载**
  - 灰区（1500-1800MHz）观察核 9 颗：79/94/112/133/177/223/362/463/530
- 与 2026-09-23 19:03 独立探针（1627/1820MHz 离群）吻合 —— 持续现象而非瞬时
- 详见 RES/summary/freq_anomalies.md（含每核证据与处置规则）

## 频率守护机制验证

freqmon.sh 每 10 秒批量采样全部 608 核写 CSV（timestamp,cpu,freq_mhz）：本作业 20 秒内产出 1216 行 = 2 拍 x 608 核，机制可用。后续所有运行作业内常驻。

## 其他环境事实

- 温度：4 个 acpitz 区 49-50 C（凉）
- 内存：565GB 总量，314GB 空闲；每 DDR NUMA ~32-33GB
- 节点 uptime 3 天，无其他用户负载（独占作业）
- EDAC：env_report 首版脚本路径错误（/sys/devices/system/mc* 应为 /sys/devices/mc*），已修正脚本，EDAC 计数随下一作业补采（本作业未采到，如实记录）

## 结论

P0 通过：cn23154 独占作业可用、608/608 CPU 全部实测可绑核运行、频率守护上线。cpu497 首例 <1500MHz 排除已立案。可以进入 P1（清点与源码理解、试点基线）。
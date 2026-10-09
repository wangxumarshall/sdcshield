# SDC1-01-03 单板突然复位初步取证报告（2026-10-09 11:47 事件）

| 项目 | 内容 |
|---|---|
| 单板 | **sdc1-01-03**（TaiShan 2280，SN 2102312YVY10M6000038）——注意与 01-02 单板（Yangtze R240K V2，192 核，core179 案例）、01-04 单板（京东云 J353 G3，TaiShan-v120，CPU122 案例）区分 |
| SoC | Kunpeng 920 ×2（TaiShan-**v110**，MIDR `0x481fd010`），128 核无 SMT，4 NUMA 节点（node0/2 无本地内存） |
| 内存 | 30.5GB（2×16GB DIMM；ghes_edac 报 32 DIMM 槽位） |
| 固件 | BIOS 7.44，iBMC 6.70（Manufacturer 2011） |
| OS | openEuler 24.03 SP4，kernel 6.6.0-159.4.3.154.oe2403sp4.aarch64，`panic=10`，`crashkernel=1024M,high` |
| 事件 | 2026-10-09 11:47:37–11:48:00 之间整机瞬时死亡，11:54:13 才开始 POST |
| 死亡时负载 | sdc-excite-reproduce 战役 L4 深驻留 `mesh_upi_sse_asymm_distrib_int` c20 第 2.4h（全 128 核，seed `AES:32c4f1b69792a3258f1f6b2210c555a4cd3b0e49686d5cda70e094ddef3aaa5b`）+ stress-ng `--matrix 4 --verify` 补充层（核 124–127） |
| 编制 | 2026-10-09（取证会话当日；证据固化于同目录 `evidence/`） |
| 置信分级 | 【实锤】可直接复核；【强推】多源收敛但无物理层直接对照；【假设】软件不可验证，给出验证途径 |

## 0. 一行结论

**【实锤】这不是软件事件**：内核 panic 路径（panic=10 + kdump 已加载）、固件错误记录路径（pstore/ERST/BERT/GHES）、BMC 事件路径（SEL 前兆/restart_cause）**三个可观测平面同时零记录**，且死前最后样本（最快采集器 20s 周期）所有遥测维度正常——机器在 ≤60 秒内从全正常到全死，复位原因寄存器读数 `unknown`，POST 延迟约 6 分钟。
**【强推】根因域为 CPU1 侧硬件在持续满载应力下的随机失效**（热/电老化或亚秒级瞬态），而非任何采样可见的确定性阈值触发——死亡日比存活日**更凉**（79°C vs 92°C 峰值）、一致性风暴强度与存活日**相同**（四类 L3C 事件差异 ≤1.2%）、且两次 openblas 死亡窗呈**相反**的互连事件画像。具体触发器三选一（电源瞬态 / 片上失效保护 / 互连偶发楔死）在当前观测粒度下不可分，判别实验见 `SDC1-01-03-ROOT-CAUSE-EXPERIMENT-PLAN.md`。

## 1. 事件时间线【实锤】

| 时刻 (CST) | 事件 | 证据源 |
|---|---|---|
| 09:14:19 | dwell `mesh_upi_sse_asymm_distrib_int` 启动（`-t 13356s`，计划至 12:56） | `evidence/20261009-dwell-dead-test.out`，yaml `time-at-start: 2026-10-09T01:14:19Z`，`iteration: 0` |
| 11:19:13 | 战役 stress-ng 层轮转启动（`--matrix 4 --verify --timeout 55m`，核 124–127） | 上一 boot journal |
| 11:38:07 | pmie：100%util、>80% CPU 饱和（负载常态） | 上一 boot journal |
| **11:40:04** | journal 最后一条**落盘**记录（此后条目死于硬复位页缓存丢失） | `journalctl -b -1`，`evidence/20261009-prevboot-journal-tail.txt` |
| 11:40:59–11:46:36 | monitor（root 服务，~80s 周期）继续正常采样 6 条：cpu1=78/cpu2=55°C、324W、bmc_ok=1 | `evidence/20261009-monitor-death-window.csv` |
| 11:47:27/28/36/37 | 五采集器末样本（ras_edac/percore/pmu_core/pmu_uncore/collector_self），全部正常 | evidence 对应文件 |
| **11:47:37–11:48:00** | **死亡窗口**：所有周期性写出同时停止 | 采集器 20–62s 周期反推 |
| 11:54:13 | SEL "System Boot Initiated SysRestart"（**约 6 分钟无 POST 黑窗**） | `evidence/20261009-reboot-inventory.txt` |
| 11:55:36 | 内核首条日志（POST 83s，正常时长） | `journalctl -b 0` |
| 11:56:05 | 战役断点自愈重启（cycle 21） | `evidence/20261009-driver.log` |

**journal 末条(11:40:04) 与真实死亡(≥11:47:37) 之间的 7.5 分钟**是硬复位丢失的未落盘页缓存——monitor.csv 存活到 11:46:36 证明 OS 活过了 journal 末条。这不是"journal 先死"，而是"journal 的页缓存死了"。

## 2. 三层观测平面全部为零【实锤】

| 平面 | 通道 | 状态 | 证据 |
|---|---|---|---|
| 内核 | panic 路径 | 未走：`/var/crash` 空（kdump 已加载且 crashkernel 已预留） | `evidence/20261009-reset-cause-bert-pstore.txt` |
| 内核 | pstore/ERST | `/sys/fs/pstore` 空、`/var/lib/systemd/pstore` 不存在 | 同上 |
| 固件 | BERT（上次启动错误记录表） | **4KB 全零**——SCP 未记录任何错误 | 同上（hexdump） |
| 固件 | GHES/HEST/EINJ | firmware-first 已启用，本 boot 无任何记录 | dmesg，`evidence/20261009-reset-cause-bert-pstore.txt` |
| BMC | SEL 前兆 | 08:14–11:54 之间零事件（无 THERMTRIP/看门狗/PSU/ECC） | `evidence/20261009-reboot-inventory.txt` |
| BMC | restart_cause | **unknown**（非按键/看门狗/软复位/上电路径） | 同上 |
| BMC | BMC 自身 | 存活（无 BMC Boot Up 事件，持续应答 monitor 的 IPMI 轮询至 11:46:36） | SEL + monitor bmc_ok=1 |

## 3. 死前状态：所有采样可见维度正常【实锤】

末样本（11:46:36–11:47:37）：cpu1=78°C / cpu2=55°C、功率 318–336W、核心轨 0.87–0.91V、VDDQ 1.23–1.25V、12V 轨 12.00–12.06V、VRD 温度 37–39°C、风扇 11475–11550 RPM、mcCE=0、mcUE=0、oom_kill=0、disk 70% 恒定、SEL5m 无新增、bmc_ok=1。

排除清单（全部证据锚定）：磁盘满（disk_pct=70% 且 /home 余 295G）✗；OOM（oomkill=0，numa 可用内存平稳）✗；内存 RAS（CE/UE=0，GHES/EDAC 无记录）✗；PSU 掉电（无 AC lost 事件，功率平稳，BMC 存活）✗；干净重启路径（restart_cause=unknown，wtmp 建账以来 12 次重启 **0 条 shutdown 记录**）✗；内核静默异常（死前 2.7h 内核日志零条目=无任何内核告警）✗。

## 4. 复位模式学：战役期 13 天 7 次自发复位【实锤】

| # | 日期 | 死亡时刻 | 死时相位（时长） | BMC | 板级? |
|---|---|---|---|---|---|
| 1 | 9/26 | ~20:41 | L3 固定序 c6（0.6h） | 存活 | 否 |
| 2 | 9/27 | ~00:16 | L2 谱系 c7（3.1h） | 存活 | 否 |
| 3 | 9/28 | ~23:09 | L4 dwell openssl_sha c9（2.45h） | 存活 | 否 |
| 4 | 10/2 | ~22:42 | **未知**（driver/monitor 自 9/30 17:51 双静默 2.5 天后） | **同重启** | **是** |
| 5 | 10/3 | ~16:33 | L2 谱系 c12（启动后 17.3h 无驱动日志，异常） | **同重启** | **是** |
| 6 | 10/4 | ~09:00 | L4 dwell openblas_dgemm c13（2.1h） | 存活 | 否 |
| 7 | 10/5 | ~11:59 | L3 随机序 c15（0.35h） | 存活 | 否 |
| 8 | 10/6 | ~17:57 | L4 dwell openblas_dgemm c17（0.62h） | 存活 | 否 |
| 9 | 10/9 | 11:47–48 | L4 dwell mesh_upi c20（2.4h） | 存活 | 否 |

- **#4/#5 是另一类**：BMC 一同重启 + 全硬件枚举（SEL "Microcontroller BMC Boot Up"）= 板级掉电级事件，且发生在 userspace 长期静默（疑似楔死）之后——可能是人工断电救砖，不与其余 7 次混计。
- **相位分散**（L2/L3/L4-dwell，dwell 对象各异），mesh_upi 自 c2（9/24）起每日 dwell 且多数完整跑完（如 10/8 c19 3.7h 存活）→ **与单一测试无因果关系**。
- **负载依赖对照**：9/7 14:38–9/26 20:41 连续 19.3 天开发负载（非 7×24 满载）**零复位**；战役 7×24 满载 13 天 7 次。满载是必要条件的组成部分。
- 更早历史（SEL）：7/16–7/17 两次 20 分钟内 4–5 连 boot 的崩溃环；8/6 PSU Failure detected 事件——本板不稳定史早于战役。
- 每次自发复位后 SEL→内核的间隔约 6–14 分钟（10/9 为 83s POST + 6min 黑窗；9/26–9/28 也有 12–13 分钟 SEL→wtmp 间隔，不排除 BMC 时钟偏差）。

## 5. A-B 自然实验（机器已替我们跑完的对照）【实锤】

同款 mesh_upi dwell：10/8（存活 3.7h）vs 10/9（死亡于 2.4h）：

| 维度 | 10/8 存活 | 10/9 死亡 | Δ |
|---|---|---|---|
| cpu1 均值/峰值 | 79.4°C / **92°C** | 77.9°C / **79°C** | 死亡日更凉 |
| 功率均值 | 329W | 324W | -5W |
| L3C retry_cpu（逐切片均值） | 10,243,334 | 10,311,193 | +0.7% |
| L3C retry_ring | 652,291 | 659,917 | +1.2% |
| L3C prefetch_drop | 9,421,667 | 9,482,473 | +0.6% |
| L3C back_invalid | 273.5 | 326.2 | +19%（绝对值小） |

**跨负载族反差**：openblas_dgemm 死亡窗（10/4、10/6）back_invalid 高 ~53 倍（14.4K/23.6K）而 retry_cpu 低 8–13 倍（1.3M/0.77M）——与 mesh 窗**相反**的互连画像，机器都死了。

**推论【实锤-负结果】**：死亡不由采样可见的平均量（温度/功率/电压/四类 L3C 事件）确定性触发。否证"简单热阈值"与"平均互连负载阈值"两类确定性模型。

## 6. 死亡时刻的微架构负载画像【实锤（负载特征）+ 分析】

`mesh_upi_sse_asymm_distrib_int`（`tests/cpu/mesh/mesh_upi_sse_asymm_distrib_int.cpp:61`）：
1 写核 + 127 读核在 **4KB 工作集**（1024×int32，L1 常驻）上——

- 写核：256 块循环 `vst1q_s32` + 每块 `__sync_synchronize()`（DMB SY）+ 立即读回比对；
- 读核：seq_cst 自旋等待 `allocated_blocks`（单缓存行，`yield` 节流）→ 全量读 + 逐块 Store/Load 比对；
- 每轮 sense-reversal 屏障：128 核对 `arrived` 单行 `fetch_add` 风暴 + `epoch` 自旋。

= 跨 4 SCCL×2 socket 的**单缓存行乒乓最坏情形**：snoop 独占权在 128 核间轮转 + 每块全屏障。L3C 实测：retry_cpu ~10.3M/切片/采样（snoop 重试高占用）。stress-ng 层在核 124–127 并行 FMA。

**末区间逐核 PMU（11:47:16→11:47:36，20s）**：96 核指令吞吐 -10%、node3（C96–127）+16%（屏障相位再分布，属该负载正常波动）；backend stall 80–94%（自旋等待的正常画像）；**无任何核停摆、无时钟异常**——最后 20s 对负载而言完全正常。

## 7. 根因假设分级

| 编号 | 假设 | 级别 | 依据 |
|---|---|---|---|
| R1 | 非软件、瞬时、低于三个可观测平面的硬件级主机复位 | 【实锤】 | §2/§3 全部证据 |
| R2 | 根因域在 CPU1 侧（而非 CPU2/共享板级路径） | 【强推】 | CPU1 与 CPU2 长期 20–30°C 热不对称；CPU1 连日 103–105°C Prochot（FAN1/FAN4 缺失下的散热赤字）；本板即 SDC 案例史单板（CPU179/CPU122 为同族 SoC 的邻板单核案例） |
| R3 | 触发为时间/老化随机（与累计应力相关），非平均量阈值 | 【强推】 | §5 A-B 全同强度却一死一活；openblas 相反画像也死；19 天轻载零复位 vs 13 天满载 7 次 |
| R4 | 具体触发器 ∈ {电源 di/dt 瞬态, 片上失效保护（内部看门狗/brown-out/THERMTRIP 类）, HCCS/snoop 偶发楔死} | 【假设】 | 三者均与"瞬时死 + 三平面零记录 + restart_cause unknown"兼容；现有粒度不可分 |
| R5 | ~6 分钟无 POST 黑窗 = 正常复位路径（SCP）随故障一并失效，由 ~360s 级二线看门狗完成复位 | 【假设】 | 黑窗时长稳定复现于多次自发复位；与"复位原因寄存器 unknown"自洽 |

## 8. 与邻板案例的关系（同为 Kunpeng 920 家族，不同失效面）

| 单板 | 机器 | 失效面 | 表现 |
|---|---|---|---|
| 01-02 | R240K V2（192 核，TaiShan-v110） | 单核 load-path 数据腐化（CPU179） | SDC → 内核可见 panic（12 次全带 vmcore） |
| 01-04 | J353 G3（128 核，TaiShan-v120） | 单核数据通路错行（CPU122 水银核） | 启动期 19–34s 崩溃，固件摘核后恢复 |
| **01-03（本案）** | TaiShan 2280（128 核，TaiShan-v110） | **整板瞬时复位，低于内核可见面** | 无 panic、无 vmcore、三层零记录 |

方法论承接：01-02 的十二案普查 + ESR 位级解码、01-04 的四启动 A/B 对照——本案在"取证平面更低"的约束下采用多采集器交叉定时 + 自然 A-B 对照，实验计划延续该谱系。

## 9. 本板物理配置风险因子【实锤（存在性）】

- FAN1/FAN4 缺失（SEL Presence: Device Absent，仅 fan2/fan3 在转）；
- 单 PSU（PS2）在线，PSU1 缺席；
- 仅 2×16GB DIMM（30.5GB），内存配置非标；
- CPU1 日程性 103–105°C（Prochot），监控 95°C 联锁在 80s 采样间隙只能事后 KILL。

## 10. 未决项与诚实声明

1. **谁发出了最终复位**（BMC 策略 / SCP 二线看门狗 / 硬件 POR）——OS 侧不可判，需带外（iBMC web 黑匣子/OEM 寄存器）取证。
2. **热/电/逻辑三机制裁别**——需 `SDC1-01-03-ROOT-CAUSE-EXPERIMENT-PLAN.md` 的 E1/E2/E3。
3. 9/30 17:51 起 driver+monitor 双静默 2.5 天的原因未查（#4/#5 板级事件前的系统状态）；不排除人工干预痕迹。
4. SEL5m 计数在 10/9 上午恒为 8 与 SEL 实际无事件矛盾（采集器窗口逻辑或 BMC 时钟问题，M2 已知欠账邻域）。
5. 本报告所有结论均限于 2026-10-09 当日取证窗口；dwell 列表在死机后已变（当前 main 脚本为 5 对象、无 mesh_upi），后续复发的激发条件已改变。

## 11. 证据文件索引（同目录 `evidence/`）

| 文件 | 内容 |
|---|---|
| `20261009-monitor-death-window.csv` | monitor.csv 死窗（09:00–12:10）134 行 |
| `20261009-pmu-core-final3samples.txt` | 逐核 PMU 末 3 个采样（11:46:56/11:47:16/11:47:36，384 行） |
| `20261009-pmu-uncore-final-sample.txt` | L3C 末采样 32 切片原值 |
| `20261009-ab-natural-experiment.txt` | A-B 自然实验完整数据与可复跑命令 |
| `20261009-reset-cause-bert-pstore.txt` | restart_cause/chassis/pstore/BERT hexdump/cmdline |
| `20261009-reboot-inventory.txt` | wtmp 全史 + boot 清单 + SEL 开机/板级/电源事件 |
| `20261009-prevboot-journal-tail.txt` | 上一 boot journal 末 80 行（11:40:04 戛然而止） |
| `20261009-driver.log` | 战役驱动日志全量（相位重建依据） |
| `20261009-dwell-dead-test.out/.yaml` | 死机测试的命令行与遗留 yaml（含 seed、`iteration: 0`） |

# SDC 7×24 压测战役统一方案设计（5W 视角）· v2

> 版本：**v2 统一版**（2026-09-24）。本文档合并两份战役设计：
> ① 本机 TaiShan 2280 as-built v1（7×24 systemd 架构，战役运行中）
> ② RCSIT TG225 B1 24h+ 战役设计（172.168.178.81 仓库 `SDC_CAMPAIGN_DESIGN_5W.md`，5W1H，战役已结束）
> 性质：**as-built 与 v2 待建分标**——凡标〔v2〕条目为本设计新增、尚未实施；其余为两机实战已验证。
> 用户决策（2026-09-24）：分层采集器架构 ｜ PMU 深度档（核+全 uncore @20s）｜ **两机同步落地** ｜ 本机磁盘先清理再部署。
> 配套：plan `docs/superpowers/plans/2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-plan.md`（本机）｜ 81 机 `docs/superpowers/plans/2026-09-23-sdc-campaign.md` ｜ 画像 `docs/superpowers/inventory/<SN>-<日期>/` ｜ 结果 `docs/superpowers/output/…`

---

## 0. 一页总览

```
 sdc-campaign.service (sdc)          sdc-monitor.service (root)            sdc-collector@.service (root 模板)〔v2〕
 ┌────────────────────────┐          ┌───────────────────────────┐        ┌─ @percore: percore.csv(逐核占用+实测频率)
 │ L0冒烟→L2谱系→L3多样性→  │  PAUSE/  │ 安全联锁(热95/90滞回+100   │        │            + 频点驻留直方图(100MHz桶)
 │ L5专项→L4深驻留(24h循环) │◀───────│  绝对线KILL/风扇/内存/磁盘/  │        ├─ @pmu:     pmu_core.csv(逐核5事件)
 │ stress-ng --verify 补充 │  标志   │  SEL Critical粘性)          │        │            pmu_uncore.csv(DDRC/L3C/HHA)
 │ 失败分类协议+假通过扣除  │          │ +唯一BMC轮询→3消费者共享    │        │            (perf -a 持久进程 @20s)
 │ +逐核二分定位〔v2〕      │          │ +SDR离散态diff→事件流〔v2〕  │        ├─ @ras:     ras_edac.csv + journal RAS流
 │ 事件取证→台账→subagent   │          │ +10min偏移引擎+SEL增量      │        │            + rasdaemon监护 + BERT启动dump
 └───────────┬────────────┘          │ +cmd/协议(governor/快照)    │        └─ 各自 Restart=always；监控侧看门狗监护
             │ state.json             │ +collector看门狗〔v2〕      │
             ▼                        └───────────┬───────────────┘
      systemd Restart=always                      ▼
                                   事件时: ±5min 全通道切片(环境/逐核/PMU/EDAC/离散态)
                                           + 失败核 120s 定向 PMU 深采(复测期并行)
 旁路: rasdaemon(全程) + kdump + EDAC + BMC SEL ── 事件互证
 人的节奏: 4h 巡检(自动) → fail 事件 → 分类协议 → subagent RCA → 主agent验证提交 → 用户决策点
```

---

## 1. WHY —— 为什么做、为什么这样设计

### 1.1 为什么压测 SDC

- **SDC（静默数据损坏）= 不崩溃的错误计算**：无日志、无异常、结果错位。它是厂商出厂测试逃逸的"边际缺陷"（marginal defect）在特定温度/电压/频率/输入组合下的发作形态，粒子翻转只占小部分。
- 生产实证规模（31 篇文献综合 `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`）：阿里百万级 CPU 32 个月患病率 3.61‱；Meta 数百万服务器 4 年 0.035% 机器终生 ≥1 次；**一次 pass 不证明健康，坏机器 73.5% 在已服役 CPU 上**——只有持续测试能抓长尾（有机器近 4 年后才首发）。
- **离线压测不可替代**（81 机文档论点）：ECC 对小时延故障（SDF）有系统性盲区（读错行仍是合法码字）；PMC 在线检测精度仅 ~90% 且特征不稳定；在线系统依赖"缺陷持久/核心局部/指令相关"三假设，其成立恰以离线检出为实证。
- 长尾 42h+；持续测试 ≫ 快照 → 7×24 systemd 托管 + 深驻留单模式 + 周期复测五层（§4）。

### 1.2 为什么是这些激发手段（文献 → 手段映射）

| 文献结论 | 战役落点 |
|---|---|
| 向量 FP 乘加单元是第一大源（>92% 事故为 FMA；vector ≫ scalar） | GEMM×4 家族 + SLEEF + fma* + acl_gemm 为 L4 驻留主力 |
| 缺陷两分类：一致性型（cache 一致性/原子/内存序）**只能多线程检出** | mesh/cachebounce/lock*/atomic*/memcpy 与计算型**混编同跑全核** |
| SDC 频率随温度 log-线性增长、存在最低触发温度 | 满核热浪（本板实测可至 105°C 带）+ 冷机窗口对照 + 85-90°C 带定向热激发 |
| 边际缺陷对 V/F/输入组合敏感 | 满载电压跌落实测 -0.02V 被监控捕获；di/dt 负载阶跃；seed 逐片轮换。**V/F 直接扫描两机均不可用**（本机 cppc 恒 performance、81 机平台固定 2.6GHz）→ 以并发档（不同电流拉载/droop）+ 全核自升温补偿（81 机六杠杆之"电气代理"） |
| execution-context 敏感（一条 no-op 改变触发率）| **全部 PROD 用例轮转**而非精选套件；每日随机序破顺序依赖 |
| 工作集谱系区分缺陷部位（核/缓存/互联/内存） | mdim 64→2048、nelems 3 档、ipsec datasize 3 档、memcpy 三策略 |
| seed 双档（fracturing 轮换 vs 固定轨迹） | 广域扫 fracturing 自动轮换 + 驻留档 `--max-test-loop-count=0` 固定 seed |

### 1.3 为什么全量遥测（v2 核心论点）

1. **归因四向需要在场证据**：缓存驻留档仍错=核内；仅大足迹=内存路径；仅多核对打=一致性/互联；证据不足=待判定。每一向都需要对应遥测维度**在故障时刻前后有数据**——per-core 占用/频率=失败核画像，PMU uncore（DDRC 带宽/L3C 命中/HHA 流量）=内存路径证据链，离散态/SEL/RAS=环境互证。
2. **热联锁的眼睛**：本板满核 3 分钟可推 CPU 至 105°C（今日 14:00/17:48/17:55 三次 KILL 实录），监控失明=战役失明=硬件风险。
3. **SOSP23 他核忙碌效应**：温度敏感缺陷的归因需对照"当时谁在跑什么"——per-core 占用率时序是归因配对的必要条件。
4. **PMU 是证据不是判据**（PinDrop：PMC 特征不稳定）——PMU 数据用于事件回溯与归因，不用于在线判定 SDC。
5. **81 机实战教训**：journal RAS 流、连续 EDAC 计数、SEL 类型统计在 24h 战役中已证明价值；本机 v1 监控缺这三路 + 无逐核任何维度。

### 1.4 两机特化对照（部署参数化的依据）

| 维度 | 本机 TaiShan 2280（SN 2102312…0038） | 81 机 RCSIT TG225 B1 |
|---|---|---|
| 核/拓扑 | 2×Kunpeng 920 = **128 核**，socket0=cpu0-63，4 NUMA | 2×Kunpeng 920 5250 = **96 核**，socket0=cpu0-47，2 NUMA |
| 内存 | 2×16GB DIMM（node1/3），NUMA0/2 无本地内存 | 1×32GB Hynix DDR4-2933（全在 node0），node1 跨 HCCS 访存 |
| 频率 | cppc_cpufreq + performance governor，**实测恒 2.6GHz**；`cpuinfo_cur_freq`（实测值）root 可读；无 `stats/time_in_state` | **平台固定 2.6GHz**（无 cpufreq、cpuinfo 无 MHz）——逐核频率不可观测 |
| PMU uncore | 16 DDRC + **32 L3C** + 8 HHA（`/sys/bus/event_source/devices/hisi_sccl*`） | 16 DDRC + **24 L3C** + 8 HHA |
| BMC | iBMC 6.70（151 传感器；in-band 唯一通道，管理网隔离实测；无 OEM 调压命令） | Hi1711 fw 3.11（132 传感器） |
| 独有模拟传感器 | Disks/HDD MAX/RAID/NIC1/PS2 温度、N_VDDAVS、HVCC、VDDQ Temp、VRD Temp | **CPU Power、MEM Power**、1711 Core Temp、SSD2 Temp、NIC OM Temp |
| 已知故障 | — | **FAN3 0rpm**、**SEL #0x84 每 ~8.5min 周期断言**（Slot/Connector）、Inlet na |
| rasdaemon | active（战役前已运行） | **inactive——部署时必须启用** |
| 内核/OS | openEuler 24.03 LTS-SP4（6.6.0-159.4.3.154，实测 `/etc/os-release`） | 同左（两机同版本，perf 6.6 同源） |
| ACPI RAS 面 | BERT/EINJ/ERST/HEST/SDEI/MPAM/PCCT | BERT/EINJ/ERST/HEST/SDEI/MPAM |
| 战役状态 | 7×24 运行中（2026-09-24 00:20 起，systemd） | 24h+ 战役已结束；watchers 仍为裸进程（无 systemd——本次统一改造对象） |

---

## 2. WHAT —— 系统是什么、激发什么、检测什么、监控什么

### 2.1 系统构成（三进程族 + 双旁路）

| 组件 | 身份 | 职责 |
|---|---|---|
| `sdc-campaign.service` | sdc 用户，Restart=always | L0-L5 状态机、stress-ng 补充层、**失败分类协议〔v2〕**、事件取证、台账、断点 |
| `sdc-monitor.service` | root，独立重启 | **唯一 BMC 轮询**（3 消费者共享）、安全联锁、10min 快照+偏移、SEL 增量、governor 强制、root 取证代理、**collector 看门狗〔v2〕** |
| `sdc-collector@.service`〔v2〕 | root systemd 模板实例 | `@percore`（逐核占用+实测频率+驻留直方图）、`@pmu`（核+uncore PMU @20s）、`@ras`（EDAC/journal/rasdaemon/BERT） |
| `rasdaemon`（既有） | root 服务 | EDAC/APEI/PCIe 错误记录（旁路证据链；**81 机部署时启用**） |
| kdump（既有） | 预留 | 崩溃转储兜底 |

### 2.2 SDC 激发（L0-L5，全部由画像参数推导，零硬编码）

| 层 | 内容 | 激发维度 |
|---|---|---|
| L0 冒烟 | list-tests 核对 + zstd19 + 每域代表 | 工具链健康 |
| L1 冷机首轮 | 45 用例代表集 30min | 冷态对照基线 |
| L2 谱系扫档 | GEMM mdim 64/256/512/1024(+2048@n64) ×4 变体、transab 0-3+β、SLEEF nelems 3 档、FFT n 4 值、igzip level 0-3、crypto/LU、ipsec datasize 1K/64K/16M、memcpy 三策略 | **工作集谱系 L1→L2→LLC→DRAM→跨 NUMA**（归因维度） |
| L3 多样性轮转 | 全部 PROD 用例 `-T 6h -t 60s --strict-runtime` 固定序 + 2h 随机序 + eigen `-n 1` | **负载多样性**（context 敏感性）+ 全核耦合热 |
| L5 专项轮换 | ① di/dt：空载 30s+满载突发×6（纯负载阶跃，governor 恒 performance）② 热激发：预热至 85-90°C 带再跑目标 ③ 跨 NUMA+大 mdim ④ 逐 NUMA 域隔离（sysfs cpulist 逗号集） | **温度角落 / 供电瞬态 / 拓扑隔离** |
| L4 深驻留 | 事件测试优先，否则五默认对象 ×2h，`--max-test-loop-count=0` | **单模式统计深度**（长尾） |
| 补充层 | stress-ng --verify 小时轮转（matrix/fma/vecfp/qsort/radixsort…）于保留核 | 交叉实现检测 |

### 2.3 SDC 检测：判据体系（合并 81 机方法学）

1. **主判据：golden 字节精确 memcmp**（全位宽整块比对——SEVI：exponent/符号位也翻；DelayAVF：~50% 多 bit，单位抽检会漏；框架 SNaN 统一静默保证跨架构位一致）
2. **假通过扣除**〔v2，需 ARM64 重推导〕：81 机审计出 22 个装饰性用例（D1 fma 容差过宽 / D6 vmx×9 恒真 / D11 crc 同源 golden ×13）的 pass 无检出意义。其中 vmx×9 为 x86-only 不迁移；fma/crc 项需在 ARM64 侧重新审计后生成**有效覆盖核算表**，进日汇总。
3. **失败分类协议**〔v2，驱动内置〕：每次 fail 自动执行——
   - 全核 60s 复跑过 → `transient`（长尾数据点，继续观察）
   - 全核复跑败 + `-n 1` 过 → `full_core_only`（并发路径相关）
   - 双双失败 → `sdc_suspect` → **自动逐核二分定位**（CORE179 式，30s/核，总时长封顶=核数×30s，结果进事件目录）
   - eigen_svd_double / eigen_sparse 全核 ULP 抖动 → `known_benign_ulp`（白名单，本机实测已证）
   - 定向复测 ×3（失败 seed，120s 封顶）语义保留，作为分类协议第一步
4. **竞态假阳性鉴别**（已实战验证——本机事件 #1）：部分和指纹（expected_sum == 部分重填块之和）+ ttf 窗口（线程孵化偏斜区间）+ 结构参数标度（-n 2 免疫 / -n 3 起步 / 高并发失败率 20-45×）→ subagent RCA 标准检查单
5. **crash 转储**：`--on-crash=context -vv`（驻留/复跑档），backtrace 进日志
6. **辅助通道互证**：EDAC CE/UE、rasdaemon（APEI/MCE/PCIe）、BMC SEL 增量、journal RAS 流、kdump——与应用层 mismatch 互证归因

失败信号双通道：YAML `result: fail|crash` **或** 进程 rc≠0（124/137/143 且无失败行 = 阶段边界 artifact，非事件——实测 TERM 对 sdcshield 60s+ 无响应，需此判别）。

### 2.4 遥测全量矩阵（v2 核心——12 维度全部落源，两机已实证）

| # | 维度 | 源（实证依据） | 粒度/节奏 | 落点 | 告警/联锁 | 事件取证 |
|---|---|---|---|---|---|---|
| 1 | 逐核占用率 | /proc/stat per-cpu tick 增量 | 每核 @60s（热区 20s） | percore.csv | —（固有波动不比） | ±5min 切片+失败核画像 |
| 2 | 内存占用率 | /proc/meminfo + **node\*/meminfo（每 NUMA）** + /proc/vmstat（oom_kill/pgmajfault 累计） | 系统+NUMA @60s | monitor.csv v3 | oom_kill 增量即时告警；水位既有 | 切片 |
| 3 | PMU 核 | armv8_pmuv3（perf `-a --per-core`，root）：cpu_cycles/inst_retired/br_mis_pred/l1d_cache_refill/l2d_cache_refill（5≤6 计数器零多路复用；本机 /bin/true 实测 304,655/129,627/1,319） | 每核 @20s | pmu_core.csv（宽表：每 interval 一行×641 列） | — | 失败核 120s 定向深采+切片 |
| 4 | PMU uncore | hisi DDRC×16/L3C×32(81:24)/HHA×8（perf `-a`，root；81 实测 flux_rd 2,004,947,310 @100% 覆盖）。事件集（每设备 8 个，超预算则分组轮换）：DDRC=flux_rd/flux_wr（带宽）+flux_rcmd/flux_wcmd/act_cmd/pre_cmd/rnk_chg/rw_chg（命令率=bank 冲突代理）；L3C=rd_cpipe/rd_hit_cpipe/rd_spipe/rd_hit_spipe/wr_cpipe/wr_hit_cpipe/retry_cpu/retry_ring（命中率+重试）；HHA=rd_ddr_64b/rd_ddr_128b/wr_ddr_64b/wr_ddr_128b（DDR 流量）+rx_ops_num/tx_snp_num/sdir-hit/edir-hit（snoop 目录） | 每设备 @20s | pmu_uncore.csv（宽表：每 interval 一行，本机 449 列/81 机 385 列） | — | 内存路径归因证据链 |
| 5 | 逐核温度 | **硬件边界：两机 BMC 均无逐核传感器**（SDR 实证：仅 per-socket Core Rem）→ per-socket Core Rem + 全部其他温度传感器（VRD/VDDQ/PSU/RAID/NIC/盘/Inlet/Outlet） | 每 sensor @60/20s | monitor.csv v3 | 热联锁既有（95/90+100） | 既有+离散态 |
| 6 | 逐核电压 | **硬件边界：无逐核传感器** → 全轨：VDDAVS/N_VDDAVS/VDDFIX/HVCC/VDDQ_AB/VDDQ_CD ×2 socket + SYS 12V×5 + PS2 VIN/IIn/IOut/POut（轨集按板发现式） | 每轨 @60/20s | monitor.csv v3 | 偏移 ±0.01V | 既有 |
| 7 | 逐核频率 | **cpuinfo_cur_freq（实测值，root-only）**——修正 v1 用 scaling_cur_freq（请求值）之弊；81 机平台固定不可观测（列空+capabilities 标注） | 每核 @60/20s | percore.csv | min<2.6GHz 节流显形 | 切片 |
| 8 | 频点驻留分布 | **自建采样直方图**（cppc 无 time_in_state）：每核 100MHz 桶累计 | 10min 快照+日累计 | freq_residency.log | 分布突变标记 | 驻留 delta |
| 9 | RAS 全量 | EDAC sysfs（mc0：ce/ue/ce_noinfo_count + per-csrow/dimm）@60s + rasdaemon（81 部署时启用）+ journal RAS 关键字流 @60s（81 移植：error/fail/ras/edac/mce/hwpoison/throttle/thermal/panic/oops/segfault/page fault/guard page/hardware，排除 hns3/rcu-stall 噪声）+ BERT 启动 dump 一次 + HEST/EINJ/ERST/SDEI 存在性入 capabilities | 连续 | ras_edac.csv + journal_watch.log | **UE>0 即时告警+开 ras 事件目录+台账行**；CE 增量告警；rasdaemon 掉线告警 | 主动旁路互证 |
| 10 | SEL | 既有增量捕获（基线持久化跨重启/风暴≥20 条/5min 告警/Critical 粘性 PAUSE）+ **新增类型统计**（81 移植：新增条目按 Sensor Type/#Event 计数进快照） | 5min | sel_events/ + 台账 | 既有 | 既有 |
| 11 | 功耗/风扇/电流 | SDR 全部模拟量：Power/Power2/**CPU Power·MEM Power（81 有）**/PS2 POut/IIn/IOut + DCMI + FAN1-4 Speed/Status/Presence + PSU Fan Status | 每 sensor @60/20s | monitor.csv v3 | 风扇 0rpm×3 采样 PAUSE（known_faults 白名单除外） | 既有 |
| 12 | PROCHOT+全部离散态 | **全量 SDR 离散传感器状态 diff**（本机 106 个实测计数：CPU1/2 Prochot、PwrCap Status、PwrOk Sig. Drop、Host Loss、Watchdog2、CPU Status、CPU/Memory Usage、DIMM×32 槽位、DISK×16、PCIE、PS Redundancy、BMC 时序类…）——v1 只取 PROCHOT 两列，其余全丢 | 每周期 diff | discrete_events.log | 断言（→非 0x00）即时告警（known_faults 除外） | 转移事件即证据 |

`monitor.csv v3` 列集**发现式生成**（启动时解析一次全量 SDR 模拟传感器→列头，轨集按板自适应；v1 的 38 列语义全部保留为子集），另加 edac_ce/edac_ue/oom_kill/pgmajfault/numa\*_used_kb。**BMC 访问频率不变**（60/20s 一次 sdr list + 5min 一次 sel list——BMC 限速教训：密集探测会打超时监控采样）。

### 2.5 数据模型与磁盘预算〔v2〕

```
monitor/monitor.csv        v3：全部模拟传感器（发现式 ~60 列）+ OS 聚合 + EDAC + vmstat    ~0.5MB/天
monitor/percore.csv        ts + 128×util% + 128×freq_kHz（257 列）                        ~4-10MB/天
monitor/pmu_core.csv       每 interval 一行 × 641 列（ts + 128核×5事件）@20s              ~22MB/天
monitor/pmu_uncore.csv     每 interval 一行 × 449 列（ts + 448 设备×事件值，81 机 385）@20s    ~17MB/天
monitor/ras_edac.csv       ts + mc ce/ue/ce_noinfo + per-DIMM（发现式）@60s               <0.5MB/天
monitor/journal_watch.log  RAS 关键字流                                                    小
monitor/discrete_events.log 全量离散态转移事件                                              小
monitor/freq_residency.log 10min 每核驻留直方图（100MHz 桶）+ 日累计                        小
monitor/sel_events/        （既有）
monitor/condition_10m.log  （既有，快照内容扩展）
合计 raw ~50MB/天 → 小时级 gzip 后 **~6-8MB/天**；保留窗口参数化（默认 14 天 gz）
```

磁盘既有联锁（85% 告警/95% 停新日志）不变。**前置条件（用户决策）：本机磁盘已 88%，部署深度档前先清理（旧构建产物/可归档日志）至 <85%**；81 机磁盘充裕（总量 29GB 仅用 ~1GB，实测 `free -g`）无此问题。

---

## 3. WHO —— 角色、职责与决策点

| 角色 | 职责 | 关键权限/约束 |
|---|---|---|
| **驱动**（sdc_campaign.sh，sdc 用户） | 阶段调度、调用 sdcshield、失败分类协议〔v2〕、取证、台账、断点 | 无 root；经 cmd/ 文件协议请求 root 动作 |
| **监控**（sdc_monitor.sh，root） | 唯一 BMC 轮询、联锁、快照、SEL、governor 强制、KILL 负载、**collector 看门狗**〔v2〕 | 只接受 performance governor 请求（用户指令）；密码零落盘 |
| **采集器**〔v2〕（sdc-collector@{percore,pmu,ras}，root） | 各自通道持续采集，独立 Restart=always；写各自文件，互不依赖 | 只读系统接口（perf/sysfs/proc/journal）；不做联锁动作 |
| **subagent**（fail 事件时派发） | 源码级根因研究（含竞态三探针检查单）+ 稳定复现（≤8 线程/冷 socket/≤5min/禁全核）；可承担修复 | 串行 ≤1（81 机约束沿用）；产物：假设排序表/复现矩阵/修复 diff |
| **主 agent**（Claude） | 4h 巡检、事件分诊执行、验证、提交、汇报；两机部署执行 | 每补丁独立验证（真实输出引用）后 commit+push；绝不编造结果 |
| **用户** | 决策点：背景负载处置、root 通道、事件处置方式、方案批准、81 机入役窗口 | 已决策：背景服务全保留 / root 服务 / subagent 修复不隔离 / 全核恒 performance / v2 两机同步+深度档 PMU+先清磁盘 |

---

## 4. WHEN —— 时间维度

| 周期 | 动作 |
|---|---|
| 20s（热区 ≥88°C） | 环境快采样；连续 2 热采样 ≥95°C 或任一 ≥100°C → KILL 全部负载。**PMU 固定 20s 节奏与热区无关**（证据通道不随联锁变速） |
| 60s（常态） | CSV 采样一行（环境+EDAC+journal 流）；percore 同步 |
| 5min | SEL 增量检查（风暴告警/Critical 粘性暂停） |
| 10min | 全量工况快照 + 偏移标记 + **驻留直方图快照〔v2〕** + PMU/离散态/逐核极值进快照〔v2〕 |
| 每小时 | 磁盘水位告警（≥85%）+ PMU/percore 原始文件 gzip 轮转〔v2〕 |
| 每 4h | 自动巡检（状态/台账/告警/温度，fail→分类协议→subagent RCA） |
| 每日 | 07:30 周期边界：日汇总（含**有效覆盖核算**〔v2〕）+ 昨日 YAML gzip；08:00 冷机首轮锚定 |
| 24h 周期 | 冷机(30m)→L2(~4.5h)→L3(6h+2h+5m)→L5(90m)→L4(动态填充至次日 07:30) |
| 事件即时 | 取证 → 分类协议（复测→n1→逐核二分）→ ±5min 全通道切片 + 120s 定向 PMU 深采 → 台账 → subagent RCA →（修复→验证→热替换） |
| ≥168h | 中期结论（未检出≠无缺陷 + 覆盖维度清单）+ **周期复测五层建议**（81 机移植）：L0 常开日志监视 / L1 每日快扫 30s / L2 每周加权 soak / L3 每月全量扫 / L4 每季度完整战役——用户决定延长与复测节奏 |

---

## 5. WHERE —— 数据布局与流（两机）

```
/home/sdc/wangxu/sdcshield/                      ← 统一仓库（本机 pr-147 为 v2 开发线）
  scripts/campaign/                              ← 统一交付（两机同源；81 机 git 拉取本线部署）
    sdc_campaign.sh / sdc_monitor.sh / sdc_common.sh
    sdc_collectors/〔v2〕 percore.sh / pmu.sh / ras.sh
    systemd/sdc-campaign.service.in / sdc-monitor.service.in / sdc-collector@.service.in〔v2〕
    collect_inventory.sh（v2 扩展：capabilities.env + known_faults + PMU 拓扑/计数器预算探测）
    install.sh / start|stop|status.sh / logrotate/ / NEW_BOARD_ONBOARDING.md
  docs/superpowers/plans|output|inventory/       ← 方案/实录/画像（两机各自子目录）
  builddir/sdcshield                             ← 被测二进制（驱动逐调用 exec，修复热替换）

/home/sdc/sdc-campaign/                          ← 运行数据（两机同布局；不入 git）
  state.json / driver.log / PAUSE / cmd/          ← 驱动侧（既有）
  monitor/                                        ← §2.5 全部遥测文件（v3 扩展）
  logs/YYYYMMDD/*.yaml|.out（每日 gzip）          ← sdcshield 输出
  events/<时间戳>-<label>-rc<N>/                   ← 事件目录（v2 扩展：分类协议产物+全通道切片+PMU 深采）
  events/ledger.csv / stressng/                   ← 台账/补充层
```

81 机专属资产收编：`scripts/run/sdc_machine_scan.sh`（183 行）并入 collect_inventory v2；`scripts/run/run_sdc_campaign.sh` 的失败分类逻辑并入 systemd 驱动；6 份研究报告（`~/sdc_campaign_2026-09-23/research/`，仓库外）作为方法学参考随实施迁入 `docs/superpowers/`；legacy 裸进程 watchers（journal_watch 等，今日仍在跑）停编入 collector@ras。

数据流：sdcshield YAML → 驱动扫描（fail/crash）→ 分类协议 → 事件目录（提取式）→ 全通道 ±5min 切片 + root 快照 + PMU 深采 → 台账 → subagent RCA → output 文档。

---

## 6. HOW —— 关键机制

### 6.1 有界执行（实测语义驱动，既有）

`-t` 是**每用例**时长、有限 `-T` 单独不硬停（实测 9.11s>4s）、`-T+--strict-runtime` ≈ 预算+在飞用例（6.08s）→ 多用例阶段统一 `run_bounded`（`-T 预算 -t 单测上限 --strict-runtime` + timeout 兜底）；L3 自终止预算，弃 `-T forever`+外部 kill（TERM 无响应实测）。

### 6.2 参数自动推导 + inventory v2〔v2〕

既有：Tjmax（ACPI trip）→ 联锁 95/90；`MemAvailable/2` → mdim 上限；node cpulist → 保留核+逐域 cpuset；`--list-tests` → 用例名单；逐 flag 解析探测。
新增（collect_inventory v2，吸收 81 机 sdc_machine_scan 的 capabilities.env 思路）：
- **capabilities.env**：核数/socket 边界（从拓扑推导，弃硬编码 64）、cpufreq 有无+可读频率源（cpuinfo_cur_freq vs 固定）、PMU 设备拓扑（DDRC/L3C/HHA 数量）+ **每类设备计数器预算**（试采 N 事件看 perf coverage 100% 与否）、EDAC mc/DIMM 拓扑、rasdaemon 状态、轨集/传感器列集、EINJ/BERT/HEST 存在性
- **known_faults 注册表**：81 机 FAN3=0rpm、SEL #0x84 周期断言等——告警白名单+快照"已知异常"标注（81 机实战用法）
- 提权自动探测（root > sudo > 降级只采 sysfs 可读部分，如实记录缺失）——81 机模式

### 6.3 安全联锁（root 监控，实测演化，既有 + 一项新增）

| 触发 | 动作 | 恢复 |
|---|---|---|
| CPU ≥95°C（滞回） | PAUSE + 热计数 | ≤90°C 自愈 |
| 连续 2 热采样 或 ≥100°C | KILL 全部 sdcshield+stress-ng | 驱动下一调用自然等待 |
| FAN 异常 ×3 采样 | PAUSE | 读数恢复自愈 |
| MemAvailable <2GB | PAUSE（防 OOM） | >4GB 自愈 |
| **EDAC UE >0〔v2〕** | 即时告警 + ras 事件目录 + 台账行（**不自动 PAUSE——用户决策点**） | 人工 |
| SEL Critical 断言 | 粘性 PAUSE（需人工 rm） | 人工 |
| 磁盘 ≥95% | PAUSE 停新日志 | 人工 |
| governor 非 performance 请求 | 拒绝 + 告警 | — |
| collector 进程死亡〔v2〕 | systemd 自愈 + 监控看门狗告警（连续 3 周期缺失） | 自愈 |

被 KILL 的调用：rc∈{124,137,143} 且无 fail/crash 行 → 阶段边界 artifact，不产生伪事件。

### 6.4 事件取证流水线（普查模式不中断；既有 + v2 扩展）

1. 驱动即时取证：`events/<时间戳>-<label>-rc<N>/`：`yaml_extract`（头部+50 处失败上下文切片，不复制 GB 级原件）、`stdout_summary`、`context`
2. **分类协议〔v2〕**（§2.3-3）：复测×3 → n1 → sdc_suspect 逐核二分（30s/核封顶），产物 `classification.txt` + `bisect/`
3. **全通道切片〔v2〕**：±5min 窗口覆盖 monitor.csv/percore/pmu_core/pmu_uncore/ras_edac/journal/discrete_events 全部文件（原仅 monitor）
4. `snapshot.request` → root 代理 60s 内补 BMC/SEL/dmesg/DCMI 快照〔v2 增：失败核 120s 定向 `perf stat -C` 深采，与复测并行〕
5. 台账一行 → subagent 根因研究（含竞态三探针检查单）→ 稳定复现或实验矩阵
6. 处置（用户决策）：隔离 / subagent 修复 → 修前基线+修后矩阵验证 → 主 agent 独立复验 → commit → ninja 热替换
7. 归因四向（证据升级）：核内（缓存驻留档仍错）/内存路径（仅大足迹+**PMU uncoro 异常**）/一致性互联（仅多核对打）/待判定

### 6.5 偏移引擎（10min 快照，既有 + v2 扩展）

既有阈值：电压 ≥0.01V、频率任何变化、温度 ≥3°C、风扇 ≥300rpm、功耗 ≥40W、内存/swap ≥1GB、磁盘 ≥1pct、Prochot 断言、SEL 任何新增。
v2 新增比较项：**逐核频率极值偏离**（max 偏离众数桶）、**驻留分布突变**（新桶出现/占比漂移 >10pct）、**PMU 异形标记**（IPC 低于全核 P5 一半的核数变化）、EDAC CE 增量、oom_kill 增量、离散态非基线条目数、per-NUMA 内存突变。

### 6.6 采集器机制〔v2 全新〕

1. **BMC 单轮询共享**：monitor 每 60/20s 一次 `ipmitool sdr list` 全量缓存，三个消费者共享（monitor.csv v3 列 / 离散态 diff / 偏移引擎）——BMC 访问频率与 v1 完全一致（限速教训）。
2. **monitor.csv v3 发现式列集**：启动解析全量 SDR 模拟传感器→列头；已知传感器缺失（如本机 Inlet=na）列保留输出空——诚实呈现"传感器在但无读数"。
3. **percore + 驻留**：/proc/stat per-cpu 增量（tick 归一）+ cpuinfo_cur_freq（实测值，root）；每采样入 100MHz 桶累计；10min 快照输出全核分布摘要+非常态核明细。
4. **PMU collector**：两个持久 `perf stat -x, -a` 进程（`--per-core` 核表 / 设备限定事件 uncoro 表）@20s interval，输出经轻量 pivot 成宽表 CSV；**事件预算**：每设备事件数 ≤ 计数器预算（inventory 试采探测），超预算分组 10min 轮换，coverage 列保留可见（多路复用诚实呈现）；进程死亡 systemd 自愈。已实证（81 root）：uncore 3 事件 @100% 覆盖、--per-core 正常输出。
5. **RAS collector**：EDAC sysfs 计数（含 per-DIMM）@60s → ras_edac.csv；journal RAS 关键字流 @60s（81 机 13 行脚本收编）；rasdaemon 活性监护（10min is-active，掉线告警；81 机部署时 enable+start）；战役启动时 BERT dump 一次（boot error 记录）；UE/CE 语义见 §6.3。
6. **离散态 diff**：每周期全量 SDR 离散传感器与上次状态比对，转移即记录（时间戳/传感器/旧→新）；断言（→非 0x00）即时告警，known_faults 白名单除外。

### 6.7 汇报呈现规格

| 场面 | 形态 |
|---|---|
| **status.sh**（任意用户随时） | 服务状态（含 collectors）→ 战役进度 → 电压/频率/占用 → **逐核极值摘要+驻留摘要〔v2〕** → SEL/EDAC/离散态异常统计 → 最新 10min 快照（含偏移段）→ 磁盘 → 最近告警 |
| **4h 巡检报告**（2-5 行） | 运行时长/cycle/phase ｜ 事件净值（含分类分布）｜ 温度峰值与联锁动作 ｜ 磁盘 ｜ 异常与干预 |
| **进展汇报**（用户召唤） | 战役本体表 → 事件与质量（**有效覆盖核算**）→ 热与硬件 → 监控能力 → 交付 → 里程碑 |
| **事件记录** | 台账一行（时间/label/rc/测试/种子/**分类**/复测/定性）+ 事件目录全证据 + output 文档详节 |
| 红线 | 只记实测（每条结论附命令输出或日志依据）；"未检出"必附运行时长与覆盖维度；绝不编造 |

---

## 7. 实测演化（as-built 依据，两机实战合计）

**本机 7×24（13.8h+，17 项）**：SELinux 拒 exec（→ /bin/bash 包装）；`-t` 每用例语义（→ run_bounded）；TERM 60s 无响应（→ 自终止+KILL 判别）；kill 误判事件；复测无界；2.76GB 取证复制（→ 提取式）；失败测试/种子错位提取（→ .out 摘要解析）；ipmitool 双格式解析；`--vary-frequency` 不在构建（→ 负载侧 di/dt）；`--temperature-threshold` acpitz 板 no-op（→ 监控唯一热联锁）；热联锁 5min 窗口致 105-106°C 持续 5min（→ 20s 采样+100 绝对线）；**事件 #1 mesh 伪 SDC**（→ subagent RCA 定案测试竞态 + 屏障修复移植自 81 机 15895cd4/c4b19bd4 → 本仓 71b0a9cc）；`-n 0` 静默钳位；SEL 增量 awk 无 print 自始失效（→ 修复+基线持久化）；pN cpuset 因 PPTT 伪影失效（→ sysfs 逗号集）；governor 冲突（→ 双侧强制）；BMC 短暂降级自愈实录（19:17 两个采样周期 bmc_ok=0→1，降级机制按设计工作）。

**81 机 24h+ 战役（并入统一设计的实战证据）**：`.done` 阶段标记断点续跑两次会话级击杀后成功；看门狗 cron 为 session-only 存在监控间隙（→ 统一 systemd 化的依据）；监测数据分段保存防重启截断；外层 timeout 按测试数计算；**mesh 竞态假阳性定案方法学**（58/58 签名+5 例算术恒等式+三探针全中）；SEL #0x84 独立环境故障的隔离定性（不作 SDC 证据）；eigen ULP 白名单；温度实测 56→91°C、功耗 234→402W、VDDAVS 0.92→0.88V droop 捕获。

---

## 8. 方案边界（诚实声明）

1. **一次全 pass 只证明"当前覆盖 × 时长内未见 SDC"**——低频长尾需 42h+，持续复测才是答案（两文档共同边界）。
2. **逐核温度/逐核电压：两机硬件均不提供**（BMC 仅 per-socket Core Rem + VR 轨）——本方案以全轨+全部温度传感器覆盖并如实标注，不伪造逐核数据。
3. **81 机逐核频率不可观测**（平台固定 2.6GHz，无 cpufreq/cpuinfo MHz）——percore 频率列输出空+capabilities 标注；驻留分布通道在该板 declared-absent。
4. **本机频率实测恒 2.6GHz**（cppc performance）——驻留直方图当前退化为单桶，价值在节流事件发生时显形（min<2.6GHz）。
5. **V/F 角落扫描不可用**（本机无 vary 框架支持、81 平台固定）——marginal defect 主杠杆缺失，以多样性×并发档+自升温补偿，覆盖弱于可调频板。
6. **PMU 是证据不是判据**（PinDrop：PMC 特征不稳定）；事件数受设备计数器预算约束（超预算轮换，coverage 列可见）。
7. **假通过清单（22 项）需 ARM64 重推导**——vmx×9 x86-only 不迁移，fma/crc 需重审计后方可计入有效覆盖核算。
8. **SVE 类零覆盖**（两机均无 SVE 硬件，SVE 用例 clean-skip）。
9. **BMC in-band 是唯一通道**（本机管理网隔离实测）——BMC 采样频率不加密（密集探测曾打超时监控采样）；uncore/核 PMU 须 root（paranoid=2 + uncoro 仅支持 system-wide，均实证）。
10. **81 机 SEL #0x84/FAN3 为独立环境故障**，known_faults 白名单处理，不作 SDC 证据。
11. **本机磁盘 88% 为部署前置风险**——先清理至 <85% 再上深度档（用户决策）。

---

## 附 A：实施分解指引（→ superpowers:writing-plans 的补丁单元草案）

一补丁一单元（CLAUDE.md 纪律），建议序：

1. 本机磁盘清理至 <85%（前置，非代码补丁，操作记录进 output）
2. collect_inventory v2：capabilities.env + known_faults + PMU 拓扑/计数器预算探测 + 提权探测（吸收 81 sdc_machine_scan.sh）
3. monitor v3：BMC 单轮询重构 + 发现式列集 + monitor.csv v3 + 离散态 diff → discrete_events.log + EDAC/vmstat/NUMA 内存列 + UE 告警
4. collector@percore：percore.csv（util+cpuinfo_cur_freq）+ 驻留直方图 + freq_residency.log
5. collector@pmu：pmu_core/pmu_uncore（perf -a 持久进程 @20s + pivot 宽表 + 预算轮换）+ sdc-collector@.service 模板 + monitor 看门狗
6. collector@ras：ras_edac.csv + journal 流收编 + rasdaemon 监护 + BERT dump + 81 机 rasdaemon 启用
7. 驱动失败分类协议：复测→n1→逐核二分 + classification.txt + 台账分类列
8. 偏移引擎扩展：驻留/PMU/逐核极值/EDAC/离散态比较项
9. 事件取证扩展：全通道 ±5min 切片 + 失败核 120s 定向 PMU 深采（snapshot.request 扩展）
10. 假通过清单 ARM64 重推导（审计补丁，产出有效覆盖核算表）
11. 81 机部署包：统一分支拉取 + inventory v2 扫描 + systemd 安装 + legacy watchers 收编停编 + 冷机 L0 入役
12. 文档同步：README/NEW_BOARD_ONBOARDING.md/status.sh 呈现扩展

每补丁自验证标准（真实命令输出引用）：本机 `ninja` 零新告警（涉及代码时）、collector 手跑 3 个周期输出实查、perf coverage 100% 实查、联锁行为实测、回归 `zstd19` pass、x86 零改动。

## 附 B：交付物与关键路径速查

- 启停/状态：`scripts/campaign/{start,stop,status}.sh`（root 启停，状态只读）
- 新单板：`scripts/campaign/NEW_BOARD_ONBOARDING.md`（inventory v2 → 构建冒烟 → 安装 → smoke → full）
- 巡检：4h 定时任务（fail→分类协议→subagent RCA 标准流程内置于 prompt）
- 台账：`/home/sdc/sdc-campaign/events/ledger.csv`；快照：`monitor/condition_10m.log`；逐核/PMU：`monitor/{percore,pmu_*}.csv`
- 81 机方法学资产：`~/sdc_campaign_2026-09-23/research/`（5 份研究 + mesh 根因对）——实施时迁入 `docs/superpowers/`
- 时间约定：NTP 不可用（UDP/123 阻断实测）→ 双源时间戳（系统 + BMC）

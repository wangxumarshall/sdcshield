# SDC 压测战役方案设计（5W1H 方法论）

> 版本：2026-09-24（战役进行中实态记录）
> 方法：Why / Who / When / Where / What / How 六问结构化设计
> 依据：`docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`（31 篇文献综述）、`docs/superpowers/plans/2026-09-23-sdc-campaign.md`（实施计划）、5 份研究 agent 报告 + 1 份根因报告（`~/sdc_campaign_2026-09-23/research/`）

---

## 1. Why —— 为什么做这件事

**SDC（静默数据损坏）是"不崩只错"的硬件缺陷**：计算结果错了位但不产生任何异常信号，上层业务无感知地消费坏数据。

| 事实 | 数据 | 来源 |
|---|---|---|
| 机队患病率 | 0.035% 机器终生至少一次（Meta 4 年/数百万台）；3.61‱（阿里 100 万+ CPU/32 个月） | PinDrop / SOSP23 |
| burn-in 后仍新发 | 每季度 0.0024% 机器新发；73.5% 出现在已服役 CPU | PinDrop / 阿里 |
| 为什么离线压测不可替代 | ECC 对小时延故障（SDF）有系统性盲区（读错行仍是合法码字）；PMC 检测精度仅 ~90% 且特征不稳定；在线系统依赖"缺陷持久/核心局部/指令相关"三假设，其成立恰以离线检出为实证 | DelayAVF / UMass / Orthrus |
| 一次通过的意义边界 | >80% 坏机器首错 <10 秒，但存在 <10⁻⁵ 低频长尾（需 42h+）——**一次 pass ≠ 健康，持续测试多数量级优于快照** | SEVI / PinDrop |

**本机背景**（RCSIT TG225 B1 工程测试板）：SEL 有活跃周期故障（#0x84 每 ~8.5 分钟）、FAN3 0rpm、node1 无本地内存——正是需要系统性健康表征的板子。

---

## 2. Who —— 对象与角色

**被测对象（Who is tested）**
- RCSIT TG225 B1（TaiShan 200，2U）：2 × Kunpeng 920 5250 = 96 核 TaiShan v110（ARMv8.2：NEON/FP16/DOT/JSCVT/FCMA，**无 SVE**）
- 频率 **2.6GHz 平台固定**（DMI Max=Current=2600MHz、无 cpufreq 策略、无 cpuidle 状态——架构性恒 performance，无降频可能）
- 内存：32 槽仅插 1 条 Hynix 32GB DDR4-2933，**全部在 node0**（node1 跨 HCCS 互连访存）
- RAS：ARM RAS Extension + ghes_edac（BMC 经 APEI 上报）；BMC Hi1711 fw 3.11（132 传感器）

**检测工具（Who tests）**：sdcshield（本仓库）——fork_each_test 进程隔离、fracturing seed 轮换、golden 字节精确 memcmp；负载库：Eigen5/OpenBLAS(TSV110)/ACL(NEGEMM)/SLEEF/OpenSSL/isa-l/pocketfft/GMP（三调度 GEMM + 四压缩实现 = 指令调度多样性）

**执行角色（Who runs）**
- 主 agent：构建、执行、监控、异常处置、报告
- 研究 subagent（**串行，并发 ≤1**）：文献精读 → GAP 分析 → 应用建议（已完成 5 份：输入模式/执行上下文/部署调度/传播负载/检测通道）
- 根因 subagent：发现 fail → 只读论证 → 判别探针 → 修复设计（已产出 mesh 竞态假阳性结论 + 修复）

---

## 3. When —— 时间结构

**单场战役（24h+，断点续跑）**

| 阶段 | 时长 | 内容 |
|---|---|---|
| P0 构建 | 1h | vendored 6 库 + meson + 冒烟（YAML schema/-s 引擎/通配符核验） |
| P1 全量广域扫 | ~2h | 333 测试 × 20s × 96 核 × fracturing（seed 自动轮换） |
| P2 文献优先级加权 soak | ~4.5h | GEMM 尺寸谱（mdim 64/256/512/1024 = L1→DRAM 足迹）×三调度、形态谱（transab×β）、crypto、压缩 level 谱、SLEEF 足迹谱、FFT 因子谱、mesh asymm_distrib 聚焦、混合负载 ×3 RNG 引擎 |
| P3 固定 seed 驻留 | 2h | 4 负载 × 30m，`--max-test-loop-count=0` 关 fracturing（单模式持续暴露） |
| P4 持续循环 ×4 | ~15h | 每 cycle = 随机序广域扫重跑（`--test-list-randomize`，执行上下文多样性）+ 拓扑/并发档 + 轮换驻留 1h |
| 战后 | ~7h | M1-M6 复现矩阵 + 补充相（多 seed 驻留 6×5m、mesh NUMA 归因 5 档、值域配对、PMC 自差分）→ 竞态修复重建 → M7 30 分钟全量干净检验 |

**节奏依据**：SEVI（>80% 首错 <10s → 20s/测试留 2× 余量）；Harpocrates（FU 型 ~1000 轮收敛 vs 位阵列 2000-5000 轮 → soak 向阵列型倾斜）；PinDrop（cadence 是第一旋钮——单场战役只是时间轴一点）。

**周期性复测（战后建议，五层）**：L0 常开日志监视 / L1 每日快扫（30s 混合档）/ L2 每周加权 soak / L3 每月全量扫 / L4 每季度完整战役。

---

## 4. Where —— 部位、拓扑与数据落点

**激发目标部位**（文献 SDC 倾向排序）

| 优先级 | 部位 | 证据 | 本方案负载 |
|---|---|---|---|
| 1 | 向量 FP 乘加（FMA） | SEVI >92% 事故；Veritas vector≫scalar | GEMM 三调度 + SLEEF + FFT + eigen |
| 2 | 哈希/加密数据通路 | Gates2SDC sha 掩蔽最少 | openssl_sha/sha3/sm3sm4 + ipsec 族 |
| 3 | 压缩 | ITHICA Zlib 检出最多 | isal_igzip level 谱 + zstd/zlib 族 |
| 4 | **数据通路/L1D**（ARM 三 ISA 最脆 53.4%） | TC23/CORE179/mesh 失败 | mesh 跨核族 + store→reload + GEMM 写回校验 |
| 5 | 一致性/多线程 | 阿里 8/27 一致性型 | mesh asymm + spinlock + atomic |
| 反向 | 标量加法/ROB/LQ/SQ | → crash 或零 SDC | 不投入（预算纪律） |

**拓扑域**（P4 拓扑档，新板自适应）：全核 96 ｜ node0 全 48（有本地内存）｜ node1 全 48（全跨互连访存）｜ node0 前 24（单 L3 簇域）｜ 跨 node 2 核。*注：各档核数不同 = 功耗/温度不同，温度敏感缺陷的归因需对照监测数据（SOSP23 他核忙碌效应）。*

**数据落点**：`~/sdc_campaign_2026-09-23/`——`campaign/logs/*.yaml`（每运行一文件）、`rerun/`（失败复跑）、`bisect/`（逐核定位）、`monitor.csv`（30s 环境流）、`cpu_freq_watch.csv`（占用率流）、`condition_snapshot.{log,csv}`（10min 工况+偏移）、`journal_watch.log`、`fails.log`/`classifications.txt`（失败分类账）、`research/`（研究报告）；仓库内：`docs/superpowers/plans/`（方案）→ `docs/superpowers/output/`（最终报告）。

---

## 5. What —— 检测什么（判据体系）

1. **主判据：golden 字节精确 memcmp**（全位宽、整块比对——SEVI exponent/符号位也翻、DelayAVF ~50% 多 bit，单位抽检会漏）
2. **假通过扣除**：22 个装饰性用例（审计 D1 fma 容差过宽 / D6 vmx_×9 恒真 / D11 crc 同源 golden ×13）的 pass 标注"无检出意义"，不计入有效覆盖
3. **失败分类协议**（fail-continue，每次 fail 自动执行）：
   - 全核 60s 复跑过 → `transient`（长尾数据点，继续观察）
   - 全核复跑败 + `-n 1` 过 → `full_core_only`（并发路径相关）
   - 双双失败 → `sdc_suspect` → **自动逐核二分定位**（CORE179 式，96 核 × 30s）
   - eigen_svd_double/eigen_sparse 全核 ULP 抖动 → `known_benign_ulp`（白名单）
4. **竞态假阳性鉴别**（本次战役新增方法学）：部分和指纹（expected_sum == 部分重填块之和 = 算术级铁证）+ ttf 窗口（线程孵化偏斜区间）+ 结构参数标度（-n 2 免疫/-n 3 起步）
5. **crash 转储**：`--on-crash=context -vv`（驻留/复跑档），backtrace 进日志
6. **辅助通道**：EDAC CE/UE（BMC 经 APEI）、SEL 事件、journal RAS 行、YAML 每测试 avg-freq-mhz（仅 -n 1 模式）

---

## 6. How —— 怎么激发、怎么监控

### 6.1 SDC 激发六杠杆（文献七原则 → 工程实现）

| # | 杠杆 | 文献依据 | 实现 |
|---|---|---|---|
| 1 | **负载多样性 > 单负载强度** | PinDrop 31% 测试曾唯一检出；SiliFuzz 纯随机独占 20% | 333 测试全谱 × 三调度 GEMM × 四压缩实现 × 3 RNG 引擎（Constant/LCG/AES） |
| 2 | **全核并发** | CORE179 单核不触发全核触发；阿里全核升温 | 全程默认 96 核；温度实测 56→91°C、功耗 234→402W |
| 3 | **seed 双档** | MeRLiN 同等价类 0.92-98 同质；CORE179 seed 敏感（0 命中↔11/11） | fracturing 自动轮换（广域扫）+ `--max-test-loop-count=0` 固定轨迹（驻留）；战后补充相升级为 6 seed×5m（期望检出 ~4×） |
| 4 | **高熵操作数 + 足迹谱** | DelayAVF md5≫libstrstr；Biswas 驻留/覆写；GeFIN 19× | LCG/AES 高熵流 + mdim 64/256/512/1024（L1→DRAM）+ nelems/n/level 旋钮 |
| 5 | **时间重复** | SEVI 低频长尾；ITHICA 长 71 vs 短 42 | 20s × 6 遍全量扫 + 30m 驻留 + 4 循环 = 24h+ |
| 6 | **电气代理（V/F 不可用的补偿）** | DelayAVF 扫 d；SEVI 降压复现 | 无 cpufreq → 并发档（96/48/24 核 = 不同电流拉载/droop，VDDAVS 实测 0.92→0.88V）+ 全核自升温 |

**v2 增强队列**（研究产出，代码级）：`val_exp` 值域旋钮（SEVI 245× 杠杆）｜FP16+SDOT/UDOT 测试（新指令代际零覆盖）｜bias_mask 位偏置（37% case 适用）｜进程内多负载交错（fork 模式丢失的残余状态继承）｜store→reload 层级谱（直击 CORE179 型）。

### 6.2 监控矩阵（四路并行，全部已验证工作）

| 通道 | 频率 | 监控字段 | 用途 |
|---|---|---|---|
| **主监测** monitor.csv | 30s | CPU1/CPU2 结温、双路 MEM 温、Outlet/Inlet、总功耗/CPU 功耗/MEM 功耗、FAN2/3 转速、**VDDAVS1/2 核电压**、SEL 总数、EDAC CE/UE、acpitz | 环境流：温度激发实证、电压 droop、SEL 增长节律、EDAC 零漂移确认 |
| **占用率** cpu_freq_watch.csv | 30s | CPU 占用率（/proc/stat 增量法，零 BMC 负载） | 占用率画像（测试族驱动 12%↔93% 波动是设计本意） |
| **工况快照** condition_snapshot.{log,csv} | **10min** | 上述全部 + VDDFIX/VDDQ/12V 扩展电压轨 + 负载均值 + **SEL 新增条目类型统计** + **逐项偏移标记** | 用户要求的周期性全工况台账；偏移阈值：温度±3°C/电压±0.02V/功耗±30W/风扇±800rpm，超限打 ⚠(旧→新)；状态型（EDAC≠0/SEL 新增/FAN3=0rpm）无条件告警 |
| **内核日志** journal_watch.log | 60s | RAS/EDAC/MCE/hwpoison/thermal/panic/oops/segfault 关键字行 | HWSentinel 稀有异常核聚集指纹（SDC 辅助信号） |

**平台特性说明**：主频 2.6GHz 架构性固定（无 governor 可设 = 恒 performance，tuned 已是 throughput-performance）；CPU 核电压为每路 VR 供电轨（无逐核传感器，如实标注）；频率/电压偏移检测中频率项恒定属正常。

### 6.3 数据流与处置闭环

```
4路监测 ──→ 每2h看门狗(cron) ──→ 用户简报(阶段/分类/环境/SEL增量)
     │
sdcshield 运行 ──→ YAML ──→ parse_results ──┬─ pass/skip → 计数
                                            └─ fail → 分类协议 → rerun/n1/二分
                                                       → classifications.txt
战役结束 → campaign_summary.md → M1-M7+补充相 → 输出报告(含失败×温度相关分析)
```

**自愈机制（实战验证）**：`.done` 阶段标记断点续跑（两次会话级击杀后均成功续跑，仅重做未完成 cycle）；看门狗 cron 每 2h 检测进程死亡自动重启；磁盘守卫 <5G 中止；外层 timeout 按测试数计算（`-t` 每测试语义）；监测数据分段保存（monitor_part1.csv，防重启截断）。已知限制：看门狗 cron 为 session-only（会话死亡期间存在监控间隙，v2 改 durable 或脚本内嵌 cron 自愈）。

---

## 7. 方案边界（诚实声明）

1. **一次全 pass 只证明"当前覆盖 × 24h 时长内未见 SDC"**——低频长尾（<10⁻⁵）需 42h+，持续复测才是答案
2. **SVE 类测试零覆盖**（本机无 SVE 硬件，44 个 SVE 用例 clean-skip）——新指令代际逃逸风险区在本板不可测
3. **V/F 角落扫描不可用**（无 cpufreq）——marginal defect 的主杠杆缺失，以多样性×并发档补偿，覆盖弱于有调频板
4. **22 个假通过用例**（D1/D6/D11）修复合并在 v2 队列；其 pass 无检出意义已从报告中扣除
5. **mesh_upi_sse/avx2_asymm_distrib 失败已定性为测试协议竞态假阳性并已修复**（commit 15895cd4/c4b19bd4）：论证 58/58 签名 100% 满足 + 5 例算术恒等式（expected_sum == data[0..3] 部分重填和）；判别探针三连全中（-n 2 二十亿级迭代零败/-n 3 必败/-n 8 失败率高 20-45×）；修复（sense-reversal 固定参与者屏障）后 -n 96 828 迭代零失败（旧二进制 40/40 秒败）、x86 零行为变化。**战后 M7 30 分钟全量回归是"真 SDC 若存在曾被此噪声掩盖"的终检**（ttf>5ms 或"完整轮和"形态出现即升级硬件排查）
6. **监测通道中 SEL #0x84 为独立环境故障**（Slot/Connector 周期断言），不作为 SDC 证据，仅作环境通道记录与相关性分析素材

---

## 附：交付物索引

| 交付物 | 位置 |
|---|---|
| 实施计划（方案本体+执行期修订 10 项） | `docs/superpowers/plans/2026-09-23-sdc-campaign.md` |
| 机器扫描脚本（新板第 0 步） | `scripts/run/sdc_machine_scan.sh` |
| 战役执行器（24h+） | `scripts/run/run_sdc_campaign.sh` |
| 补充相执行器（研究成果落地） | `scripts/run/run_sdc_supplement.sh` |
| 研究报告 ×5 + 根因 ×1 | `~/sdc_campaign_2026-09-23/research/` |
| 全部战役数据与监测流 | `~/sdc_campaign_2026-09-23/campaign/` |
| 输出报告（战后） | `docs/superpowers/output/2026-09-23-sdc-campaign-output.md` |

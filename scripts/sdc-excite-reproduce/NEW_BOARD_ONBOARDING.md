# 新单板接入 7×24 SDC 战役（3 步）

> 配套 plan：`docs/superpowers/plans/<日期>-<SN>-sdc-7x24-stress-plan.md`（方案与安全边界）
> 本文档是操作手册：在一块**新单板**上从零到 7×24 压测跑起来。

## 前置条件（人工确认，脚本不代答）

| 项 | 确认方法 | 不满足时 |
|---|---|---|
| 裸金属 | `systemd-detect-virt` = `none` | 虚机内压测宿主 CPU SDC 无意义，停止 |
| root 通道 | `sudo -n true` 通，或有 root 密码 | 降级非 root 画像（缺口记录），IPMI 采不到 → 联锁只剩 OS 侧 |
| BMC/IPMI 可达 | `ipmitool mc info`（root） | 监控降级 OS 侧采样，温度联锁只剩 acpitz |
| 磁盘水位 | 战役数据目录所在盘 ≥50G 可用 | 换目录或清盘；85%/95% 水位会自动告警/停日志 |
| 风扇/PSU 冗余 | `ipmitool sdr type Fan` / SEL 无 Critical | 缺风扇读数的槽位不在联锁范围，风险自担并记录 |
| 背景负载 | `ps aux --sort=-%cpu | head` | 与业务共存需确认（逐核隔离结果会有噪声）；专用机最佳 |
| 散热余量 | 基线 CPU 温度距 Tjmax ≥30°C | 先查风道/风扇再压测 |

## 第 1 步：画像（幂等，可重复跑）

```bash
cd <sdcshield 仓库>
# 方式 A：有 sudo 免密
scripts/sdc-excite-reproduce/collect_inventory.sh docs/superpowers/inventory/
# 方式 B：用 root 密码（经 stdin 传给 su，不落任何文件）
SDC_ROOT_PW='...' scripts/sdc-excite-reproduce/collect_inventory.sh docs/superpowers/inventory/
# 方式 C：无 root —— 自动降级，缺口写入 00_gaps.txt
scripts/sdc-excite-reproduce/collect_inventory.sh docs/superpowers/inventory/
```

产物 `docs/superpowers/inventory/<系统SN>-<日期>/`（16 个文件）。**核对** `01_nonroot_all.txt` 的 lscpu/内存/热区 trips 与 `03_ipmitool_sdr_list.txt` 的传感器清单——这些是第 3 步自动推导的输入。人工补写 `00_MACHINE_PROFILE.md`（可参照 `2102312YVY10M6000038-2026-09-23/00_MACHINE_PROFILE.md` 的结构）。

## 第 2 步：构建 + L0 冒烟

```bash
# vendored 依赖（首次；已存在会跳过）
./third-party/openssl/build.sh && ./third-party/openblas/build.sh \
  && ./third-party/sleef/build.sh && ./third-party/isa-l/build.sh && ./third-party/acl/build.sh
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
ninja -C builddir
./builddir/sdcshield --list-tests | wc -l        # 记录数量（应 ≥278）
./builddir/sdcshield -e zstd19 -n 1 -t 2000      # 必须 exit: pass
```

## 第 3 步：安装 + smoke 验收 + 正式启动

```bash
# 安装（root；模板替换 REPO/用户/数据目录）
su -c "bash scripts/sdc-excite-reproduce/install.sh"          # 或 sudo bash scripts/sdc-excite-reproduce/install.sh
# smoke 全链路（~10 分钟；驱动手动跑，不动 systemd 的 full 实例）
su -c "systemctl start sdc-monitor"
timeout 900 bash scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh smoke
bash scripts/sdc-excite-reproduce/status.sh                   # 观察 monitor.csv 在涨、无 PAUSE
# 通过后正式启动 7×24
su -c "systemctl start sdc-excite-reproduce"
```

## 第 4 步：M1 采集器部署与验收（v5 §6 采集器族 + §14.1 M1 退出标准）

前置：第 3 步 `install.sh` 已装 `sdc-collector@.service` 模板（未启动）。采集器与战役
（`sdc-excite-reproduce.service`）解耦——验收门只看采集器，不要求战役运行。

```bash
# 1) 刷新数据根 capabilities.env（root；让 per-PMU 计数器预算等实测能力落数据根）。
#    输出根指 /tmp：画像产物不落仓库；SDC_EXCITE_REPRODUCE_DIR 必须显式给（root 的 HOME 是 /root）
su -c "SDC_EXCITE_REPRODUCE_DIR=/home/<user>/sdc-excite-reproduce \
  bash scripts/sdc-excite-reproduce/collect_inventory.sh /tmp/inv_refresh"
grep PMU_CORE_COUNTERS ~/sdc-excite-reproduce/capabilities.env   # 期望实测数字（本系列板 12）；unknown=探测失败

# 2) PMU 预算注入 @pmu 单元：采集器进程不 source capabilities.env，单元环境缺
#    PMU_CORE_COUNTERS 时 sdc_collector_pmu.py 回落默认 6（每组建事件被砍尾、丢覆盖面）。
#    最小方案：给已安装单元追加一行 Environment（值读 capabilities.env）+ daemon-reload。
#    注意：install.sh 重装会用模板覆盖单元文件，此行需重做。
B=$(grep -oP 'PMU_CORE_COUNTERS=\K[0-9]+' ~/sdc-excite-reproduce/capabilities.env)
su -c "sed -i \"/^Environment=SDC_EXCITE_REPRODUCE_DIR=/a Environment=PMU_CORE_COUNTERS=$B\" \
  /etc/systemd/system/sdc-collector@.service && systemctl daemon-reload"

# 3) 启动三采集器（enable --now；保持常驻——它们就是 M1 24h 门与后续战役的监控载体）
su -c "systemctl enable --now sdc-collector@percore sdc-collector@pmu sdc-collector@ras"

# 4) 30 分钟冒烟 → 0.5h 门（六项：active×3 / 丢样<0.1% / percore 新鲜度 / percent_covered
#    可见 / 时长 / A/B 开销；A/B 段 15×30s 约 9 分钟）
sleep 1800 && bash scripts/sdc-excite-reproduce/acceptance_m1.sh 0.5

# 5) 24h 正式门（M1 退出标准最终判据；采集器持续运行满 24h 后执行）
bash scripts/sdc-excite-reproduce/acceptance_m1.sh 24
```

产物（`~/sdc-excite-reproduce/monitor/`）：`percore.csv`（逐核占用/实测频率）、
`pmu_core.csv`/`pmu_uncore.csv`（perf 计数模式宽表 + `percent_covered` 质量列，组轮换
10min）、`ras_edac.csv`（mc 级 + per-DIMM 宽稀疏列 `dimm_mc<N>_<idx>_ce/ue`）、
`journal_watch.log`（RAS 流告警）、`collector_self.csv`（自监控 drop counter——丢样验收依据）。
重启列头防御：采集器重启后列头不重复写；若已有 CSV 列头与当前 schema 不符会拒绝启动
（stderr 说明）——那是 schema 漂移，人工核查后再处置，不要盲目删数据。

A/B 判读纪律：锚点为采集器全停时基线（Task 2，5×10s，MAD 可达 ±14%）；复测为
15×30s（MAD ~±3%）。下降 ≤3% PASS；3%-8% WARN——受锚点噪声限制，需静默窗口复测
（`pgrep sdcshield` 为 0 时复跑第 5 步的 A/B 段）；>8% FAIL 查采集器 CPU 占用。

## monitor v3 与驱动加固（M1b）部署说明

**monitor v3 列集（发现式）**：首启全量发现 BMC SDR 模拟量传感器（本系列板 ~40 列）
+ 11 个固定尾列（EDAC ce/ue 总和、oom_kill、pgmajfault、NUMA0-3 memavail、disk_pct、
sel5m、bmc_ok）+ ts，共 ~52 列；列集持久化在 `monitor/sensors_v3.json`。重启重新发现
并与持久序比对。两级归档语义，**都属预期行为、非缺陷**：

- v2→v3 首次升级：旧 CSV 表头（38 列）≠ v3 表头 → 旧数据归档为
  `monitor_v1_<ts>.csv.gz`（沿用历史命名，不告警）；
- 运行期列集漂移（固件升级/换板致 SDR 列集变化）：归档 `monitor_v3drift_<ts>.csv`
  **并告警**——人工核查后再处置，不要盲目删数据；BMC 暂不可达的空发现沿用持久序
  （空发现不构成漂移证据）。

**离散态 diff**：`monitor/discrete_events.log` 首启为全部离散传感器记
`INIT → 当前值` 基线行（本系列板实测 103 行），此后**只在状态转移时**追加
`TRANSITION` 行（含时间戳与新旧值）——看 0x00 → 非 0x00 即异常起点，配合
alerts.log 与 SEL 对时。

**复测内存护栏（retest_guarded）**：fail/crash 事件的定向复测全程带护栏——
前置门 MemAvailable < 6GB（等 60s 重取，仍低则记 `skipped(memguard)` 不复测，
返回码 250）+ 运行期看门狗 < 2.5GB（KILL 复测进程组并记 `killed(memwatch)`，
防复测把机器推进 OOM——2026-09-25 cold_c4 rc=137 的教训）。护栏只覆盖复测窗口，
主阶段路径零内存检查（有测试守护此边界）。

**测试沙盒红线（SDC_ORPHAN_PIDS）**：战役运行期跑 pytest（`tools/telemetry/tests/`）
必须经测试助手设置 `SDC_ORPHAN_PIDS`（逗号分隔 pid 表）——看门狗击杀后的孤儿清理
`cleanup_orphans` 在该变量设置时只杀表内 pid；不设则走生产 `pkill -KILL -x control`，
与运行中切片 comm 不可区分，**会污染战役**（2026-09-26 无沙盒时期实测发生过，
污染数据已标注不入 SDC 统计）。测试一律 `env -u SDC_ROOT_PW python3 -m pytest`，
勿手工直跑 `cleanup_orphans` 所在路径。

## 自动推导 vs 人工确认

**自动推导**（`campaign.env`，首次运行探测；删除该文件可重新生成）：
- 热联锁阈值：ACPI `trip_point_0_temp` 最大值 −10°C（暂停）/−15°C（恢复）
- GEMM mdim 全核上限：`sqrt(MemAvailable/2 / (3×8×线程数))` 向下取 2 的幂
- stress-ng 保留核：最高编号 NUMA 节点的最后 4 个 CPU
- 用例名单：`--list-tests` 实测解析（不存在的 pattern 自动剔除）
- 旗标集：逐个解析探测（rc=64=不存在），缺失自动降级

**必须人工确认**：上表前置条件 + `--temperature-threshold` 是否真生效（本系列板卡
`thermal_monitor.hpp` 已知热区类型表不含 `acpitz` → 进程内热联锁 no-op，root 监控是唯一
热保护——新板若是 `cpu/soc/big/little` 等类型则进程内联锁自动生效）。

## 运维速查

| 操作 | 命令 |
|---|---|
| 状态 | `bash scripts/sdc-excite-reproduce/status.sh`（任意用户） |
| 暂停/恢复战役 | 联锁自动 PAUSE/RESUME；人工暂停 `echo 原因 > ~/sdc-excite-reproduce/PAUSE`，恢复 `rm ~/sdc-excite-reproduce/PAUSE` |
| SEL Critical 粘性暂停 | 调查后 `rm ~/sdc-excite-reproduce/PAUSE`（监控只清除自己写的 thermal/fan/mem 类） |
| 事件台账 | `cat ~/sdc-excite-reproduce/events/ledger.csv`；单事件证据在 `events/<时间戳>-*/` |
| 崩溃后 | systemd 自动拉起（Restart=always）；从 state.json 断点续跑 |
| 停止 | `su -c "bash scripts/sdc-excite-reproduce/stop.sh"`（保留进度） |
| 日志轮转 | logrotate 每日压缩保留 14 天；YAML/events/monitor 证据**永不轮转** |

## 已知缺口模式（遇到即如实记录，不得留空假设）

- BMC 不可达 → 监控自动降级 OS 侧（alerts.log 有记录），温度联锁改用 acpitz
- `--vary-frequency` 不在构建中（无频率管理器）→ di/dt 由 governor 阶跃 + 负载突发承担
- 无 per-core 温度传感器 → 以每 socket 一个 Core Rem + acpitz + Prochot 替代
- 时间同步不可用（UDP/123 阻断）→ 事件台账双源时间戳（系统 + BMC）

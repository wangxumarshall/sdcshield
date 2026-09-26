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

## 第 5 步：M2 规则闭环部署（eventd/controller/root-helper/ring 四守护，v5 §8/§13）

### 四服务职责与事件流链路

```
五源（events/ledger.csv、monitor/{discrete_events,journal_watch,collector_self,
alerts}.log）→ sdc-eventd（尾随规范化）→ spool/events.jsonl（canonical 事件档案）
  ├→ sdc-controller（五态规则状态机 green/yellow/orange/red/black）
      ├→ spool/controller_ledger.jsonl（append-only 决策账本，replay 可重放）
      └→ cmd/ 请求文件（文件协议 v5 §13.3 最小版）
            ├ burst-<action_id>.request ──┐
            ├ cpu_hotplug.request ────────┼→ sdc-root-helper（root：allowlist
            ├ restore.request ───────────┘   特权动作 + nonce + 审计）
            └ snapshot.request → 既有 root monitor 代理（沿用，不重复实现）
  └→ sdc-ring（120s 滚动热窗口；red/black 事件固化 events/<event_id>/ring_window/，
        monitor.csv/percore.csv 同窗入证据）
```

- **sdc-eventd**（用户态）：五源尾随 → 事件档案；dedup/offset 持久化在 `spool/`；
  行级毒丸兜底（残行入 `eventd_invalid.jsonl` 不炸守护）。
- **sdc-controller**（用户态）：规则驱动五态机（`configs/sdc-excite-reproduce/
  rules_m2.json`）；守护模式每轮无新事件写 `{"rule":"heartbeat",...}` 决策行
  （活体证据）；状态+消费 offset 原子持久化（重启不丢 RED/BLACK、不重放旧决策）。
- **sdc-root-helper**（root）：allowlist 特权动作执行器；每请求一行审计
  `spool/root_helper_audit.jsonl`（含 rejected/expired/error——被拒也是安全事件）。
- **sdc-ring**（用户态）：120s 滚动热窗口（尾随 monitor/percore CSV）；red/black
  事件到达即固化 `events/<event_id>/ring_window/`（[-60s,+60s] 口径取证证据）；
  offset/已固化 id 集原子持久化（`spool/ring_offset.json`/`ring_frozen.json`），
  重扫护栏双保险（id 集 + 固化文件存在性）防覆盖既有证据。

### 部署顺序（首启基线——顺序不可换）

```bash
su -c "systemctl start sdc-root-helper sdc-eventd"   # ① 消费者先就位，eventd 产事件
sleep 10                                              # ② 等 eventd 首轮回填（≥3 轮 2s 轮询）
bash scripts/sdc-excite-reproduce/m2_controller_baseline.sh   # ③ controller 消费基线（见下）
bash scripts/sdc-excite-reproduce/m2_ring_baseline.sh         # ④ ring 消费基线（见下）
touch ~/sdc-excite-reproduce/spool/root_helper_audit.jsonl    # ⑤ 审计通道预建（空文件）
su -c "systemctl start sdc-controller sdc-ring"       # ⑥ 消费者上线
```

**消费基线（步骤 ③）为什么必须**：eventd 首启把五源全部历史回填 events.jsonl
（事件档案，设计内）；controller 首启 offset=0 会把整段历史喂进状态机——参考板
实测 193 条历史事件含 2×sdc_mismatch(red)+25×interlock(black)，直接终态 BLACK，
并对陈旧事件分派 snapshot/verify 请求（历史含 orange 断言还会真触发 perf
burst）——**陈旧事件驱动特权动作是安全事故**。基线脚本把 controller 消费
offset 预置到回填末尾（state=green），只消费部署时刻之后的实时事件；事件档案
全量保留可 replay。注意 offset 单位=字节（tail_file 二进制读），脚本复用
tail_file 本身取末偏移，勿手工按字符数算。

**ring 消费基线（步骤 ④）同理且更严**：sdc-ring 无基线直接启动会以 offset=0
重扫全部历史事件——重扫护栏只拦"已有固化证据"的覆盖，**拦不住首次固化**：
当前 120s 热窗对历史 red/black 是时间错位数据，冒充 [-60s,+60s] 固化窗口即取证
证据污染。基线脚本只预置 offset（字节单位，同复用 tail_file）+ `ring_frozen.json`
空集，**不固化任何历史事件**——历史事件的窗口数据早已流逝，诚实做法是基线后
新事件才固化；ring 首轮自动回放两 CSV 文件尾暖窗，窗口为内存态无需（也无法）
预置。

**验收冒烟**（≥3 轮后）：`spool/events.jsonl` 有行、`controller_ledger.jsonl`
有 heartbeat 行、`controller_state.json` state=green、`spool/ring_offset.json`
存在且 `events/*/ring_window` 无历史假固化（新 red/black 到来才出现）、四服务
active、既有五服务（战役/monitor/三采集器）不受影响。

### spurious canary 能力探测（v5 §6.5）

`sudo bash tools/telemetry/bpf_canary_probe.sh [数据根]` → 幂等写入
`capabilities.env` 的 `SPURIOUS_CANARY=` 行。参考板实测（2026-09-26，bpftrace
v0.19.1）：BTF 在、符号在 kallsyms 与 available_filter_functions 均在，但
`bpftrace -l 'kprobe:do_translation_fault'` 不列出该 kprobe（其余 kprobe 可列）
→ **不可安全挂载 → SPURIOUS_CANARY=dmesg**。这是 v5 §6.5 预期的诚实降级
（dmesg/RAS 轮询路径 M1 起在 collector@ras 运行），非缺失；BPF 升级待内核
侧能力变化后重探。

### M2 演练（m2_drill.sh——永不碰真实根）

```bash
bash scripts/sdc-excite-reproduce/m2_drill.sh /tmp/全新隔离目录
# 五级链：collector 降级→yellow、离散断言→orange（pmu_burst）、合成 mismatch→red
# （ring 固化）、RAS 关键字→red 下非法 ignored、温度越限→black（verify_only）
# 断言 A-E：决策序列/重放确定性/ring 固化/burst 全链/请求在案 + 静止复跑幂等
```

守卫：数据根必须全新（已含 spool/ 即拒绝——真实根部署后必含，天然防误跑）；
全程 --once + helper --mock-exec（伪 perf CSV，绝不真特权写）。

### 运维要点（T4/T5 移交项）

- **cmd/verify.request 滞留是设计内**：interlock→BLACK 时 controller 落
  verify.request（只读校验请求，v5 §8.3 BLACK 态只允许验证类动作）；消费者
  已随 M3 落 repo（`sdc_common.sh consume_verify_request`，驱动主循环轮询）。
  **运行中的驱动是旧代码——repo 接线在驱动下次重启后生效**（systemd
  Restart=always 或 stop/start）；生效前滞留仍非故障。
- **restore 基线重置**：helper 首次 hotplug 改写前快照 online 集到
  `spool/pre_state_online.txt` 且不覆盖——restore 恒回**最初**全集。若运维
  有意变更了 online 集并想让 restore 回**新**基线，须手删该文件（下次改写
  重新落快照）。
- **cmd/ 里 .done./.rejected. 尾缀文件**是既有 monitor 代理与 helper 的请求
  终态化产物，非垃圾；helper 只扫 `*.request` 活请求。
- install.sh 重装会覆盖 sdc-collector@.service——M1 的 PMU_CORE_COUNTERS
  注入行需重做（见第 4 步第 2 点）。

## 第 6 步：M3 主动激发与复现（profile/四轴/复现队列，v5 §7+§10）

### 工具族（tools/excite/，全 stdlib-only 零第三方依赖）

| 工具 | 职责 | 入口 |
|---|---|---|
| `sdc_profile.py` | 压力 profile 定义（JSON）/resolve 冻结（拓扑实际展开+二进制 sha256+capabilities/topology 快照——同一 profile 名跨机器/时点不静默漂移）/受界执行器（victim 先起、aggressor 并发） | `run [--dry-run] <profile.json>`；样例 `configs/sdc-excite-reproduce/profiles/victim_candidate_v1.json` |
| `sdc_axes.py` | 四轴策略执行器：load_shaping/hotplug 可执行；governor 实验档=能力缺口诚实暴露（需独立授权，v5 §7.4.1） | 库模块（测试驱动） |
| `sdc_reproducer.py` | 复现 runner+真实性门禁七项（v5 §10.1）+复现胶囊打包（§10.2）+队列消费 | 子命令 `gate`/`capsule`；模式 `--from-queue [--once] [--data-root DIR]` |
| `sdc_reducer.py` | 概率 ddmin reducer+搜索树（CORE179 硬规则） | 库模块（m3_drill/后续编排接线） |

### 复现队列语义（M2 enqueue_reproduction 落点闭环）

- **产出端（驱动钩子——repo 已接线，下次驱动重启后生效）**：`handle_failure`
  台账行后调 `enqueue_repro` → `spool/repro_queue/<epoch-ns>.json`（文件名
  数值序即消费 ts 序）。
- **消费端（按需 CLI——裁定不部署常驻守护**，事件驱动由驱动钩子产出+手动/
  巡检消费；cron 可选未部署）：
  `python3 tools/excite/sdc_reproducer.py --from-queue [--once] --data-root <根>`
  （`--data-root` 缺省 $SDC_EXCITE_REPRODUCE_DIR，生产根即 ~/sdc-excite-reproduce）。
- 每事件全链：`from_event`（失败 test/seed 锚定 detecting 核构造最小 victim
  profile）→ 真实性门禁七项（含健康核对照真实发射）→（过）capsule 打包到
  `spool/repro_capsules/<event_id>/` + 概率复测 3 次（单事件总预算 ≤5min，
  per-trial 截断）→ 结果行 `spool/repro_done/<队列文件名>` + 删队列。
- done 状态：`processed` / `gate_rejected`（拒因入案，不重试）/ `invalid`
  （毒丸 JSON 原文截留）/ `error`（from_event 响亮拒绝：无种子/无锚点）/
  `already_done`（崩溃恢复幂等——复测是有界负载，不无理由重跑）。
- **消费前人工确认**：健康核对照与概率复测是真实受界发射（复用 retest_guarded
  内存护栏：前置门 6GB/看门狗 2.5GB）；单事件最坏 ~7min，建议低负载时段跑，
  `spool/repro_queue/` 有堆积即待办（目录即看板）。

### CORE179 语义（概率 ddmin，v5 §10.3-10.4）

CORE179 教训：**单核隔离会消除 SDC**——确定性 ddmin 被硬规则禁止（单次试验
定夺=误删复现条件）。每候选序贯 k/n + Wilson 95% 区间（min_trials ≥2）；
aggressor 因素删空复现崩塌（REJECT）→ 回退上一接受态 + `necessary_aggressors`
标记——输出"最小 victim + 必要 aggressor 场"，绝不强迫空 aggressor 样例；
INCONCLUSIVE 一律保留因素（最小性存疑优于误删复现条件）。

### M3 演练（m3_drill.sh——永不碰真实根/真实负载）

```bash
bash scripts/sdc-excite-reproduce/m3_drill.sh /tmp/全新隔离目录
# 演练链：合成 mismatch 事件目录 → enqueue_repro（生产产出端）→
#   sdc_reproducer --from-queue --once（消费端全链——SDC_BIN=隔离根内
#   fake bin，绝不真跑 sdcshield）
# 断言 A-D：队列消费闭环（done+复测 k=3/3+门禁过）/capsule 完整+SHA256SUMS/
#   run.sh --check-only 同机可重放（退出标准①，预检不 mock）/CORE179
#   概率 ddmin 语义（退出标准②：最小 victim+必要 aggressor）
```

守卫：数据根必须全新（已含 spool/ 即拒绝）；MemAvailable < 6GB 前置拒
（复测内存门门槛）；capsule 平台快照取自实机只读探针——同机 --check-only
真实通过（可重放的最强形态）。


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
| M2 四守护 | `systemctl is-active sdc-eventd sdc-controller sdc-root-helper sdc-ring`；决策账本 `tail ~/sdc-excite-reproduce/spool/controller_ledger.jsonl`（每 2s 一行 heartbeat=活体）；状态 `cat ~/sdc-excite-reproduce/spool/controller_state.json`；ring 消费位 `cat ~/sdc-excite-reproduce/spool/ring_offset.json` |
| 暂停/恢复战役 | 联锁自动 PAUSE/RESUME；人工暂停 `echo 原因 > ~/sdc-excite-reproduce/PAUSE`，恢复 `rm ~/sdc-excite-reproduce/PAUSE` |
| SEL Critical 粘性暂停 | 调查后 `rm ~/sdc-excite-reproduce/PAUSE`（监控只清除自己写的 thermal/fan/mem 类） |
| 事件台账 | `cat ~/sdc-excite-reproduce/events/ledger.csv`；单事件证据在 `events/<时间戳>-*/` |
| M3 复现队列 | 待办 `ls ~/sdc-excite-reproduce/spool/repro_queue/`（堆积即待消费）；消费 `python3 tools/excite/sdc_reproducer.py --from-queue --once --data-root ~/sdc-excite-reproduce`（真实受界发射——低负载时段跑）；结果 `ls ~/sdc-excite-reproduce/spool/repro_done/`；capsule `~/sdc-excite-reproduce/spool/repro_capsules/<event_id>/`（`scripts/run.sh --check-only` 可重放预检） |
| 崩溃后 | systemd 自动拉起（Restart=always）；从 state.json 断点续跑 |
| 停止 | `su -c "bash scripts/sdc-excite-reproduce/stop.sh"`（保留进度） |
| 日志轮转 | logrotate 每日压缩保留 14 天；YAML/events/monitor 证据**永不轮转** |

## 已知缺口模式（遇到即如实记录，不得留空假设）

- BMC 不可达 → 监控自动降级 OS 侧（alerts.log 有记录），温度联锁改用 acpitz
- `--vary-frequency` 不在构建中（无频率管理器）→ di/dt 由 governor 阶跃 + 负载突发承担
- 无 per-core 温度传感器 → 以每 socket 一个 Core Rem + acpitz + Prochot 替代
- 时间同步不可用（UDP/123 阻断）→ 事件台账双源时间戳（系统 + BMC）

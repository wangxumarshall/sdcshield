# sdc-excite-reproduce M5（加固与推广）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 M5 的加固与推广——故障演练脚本族（13 类合成故障的注入与验收）、exposure.json 暴露量生产端（M4 移交的最有价值补数据项）、81 机部署包（补丁单元 13）、机读 hint 契约与嫌疑核标签接续（M4 终审两项移交）、运维手册与状态机恢复操作入口，以"72h 运行、panic/kdump、断网/磁盘/BMC 故障演练和恢复验收通过"为退出标准。

**Architecture:** `scripts/sdc-excite-reproduce/drills/` 新目录放演练脚本族（每故障一个注入器+断言器，全部在隔离数据根/测试进程上运行，绝不碰真实战役）；暴露量生产端落在 monitor v3 的新列+`spool/exposure.json` 周期快照；81 机部署包是现有 install.sh 的参数化打包（离线 rpms 已有）。M4 移交两项为 tools/analysis 的小修。

**Tech Stack:** Python 3.11 stdlib、bash、systemd 现有单元；零第三方依赖不变。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5 §17 验证与验收/§16 运行手册/§14.1 M5 行/§14.2 补丁单元 13-14）。范围：M5 = 故障演练/暴露量/81 机包/文档加固；**跨机群调度与长期模型训练属〔研究〕不含**；本机可执行的 72h soak 已由运行中战役事实满足（9 服务 × 3 天+）——验收以既有运行数据回溯+演练脚本断言替代重启计时。

## Global Constraints

- **一补丁一单元**：每 Task 一 commit；`git commit -s` 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>` 其后无内容、无 Co-Authored-By；分支 `feat/sdc-excite-reproduce-m5`（从 main 建）；绝不提交/推送 main。
- **战役+9 服务运行中**：演练全部在**隔离数据根+测试进程**上运行（m2_drill/m3_drill 同模式）——绝不注入真实 spool/绝不 kill 真实服务/绝不真断网真满盘；root 演练项（kdump 查询等）只读探针。
- **零第三方依赖**；`env -u SDC_ROOT_PW` 纪律；测试全 fixture。
- v5 §17.2 十三类演练清单为验收基准（合成 byte mismatch/测试 bug 样本/spurious fault 日志/CE/UE/panic/runner hang/BMC 超时/collector 崩溃/网络断开/磁盘满/时间跳变/温度越限/restore 失败——实为 13 类含 restore）。
- 81 机（RCSIT TG225 B1）**不可从本机直达**——部署包交付为"包+入役清单"，实际入役是用户决策与远端操作（如实记录，不虚报"已部署"）。

---

### Task 1: exposure.json 暴露量生产端（M4 移交的最有价值项）

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc_monitor.sh`（周期采样段追加——**联锁段仍逐字不动**）
- Test: `tools/telemetry/tests/test_exposure.py`（bash 子进程，MON_SELFTEST 模式）

**Interfaces:**
- Consumes: `/proc` tick、journal/日志 gzip 归档、sdcshield YAML 的 loop-count（日汇总行）
- Produces: `spool/exposure.json` 周期快照（10min 周期与 condition_10m 对齐）：`{"ts", "valid_iterations_today": <YAML loop-count 日累计>, "core_hours_today": <online 核数积分小时>, "source_files": {"yaml_gz": <今日gz数>, ...}}`——M4 `rate_table` 的 `valid_iterations` 数据源（读该文件，"不可得"路径保留为缺文件时降级）
- 实现要点：日累计从 `logs/YYYYMMDD/` 的 .yaml.gz 逐文件 `zcat | grep -c loop-count` 太贵——**改为 driver 日汇总行已有 loop-count 累计**（driver.log 日汇总行），monitor 只聚合 driver 侧数字+online 核数积分；两者都不可得时字段 null+注记

- [ ] **Step 1-4**: 失败测试（fixture driver.log 日汇总行+固定 online 集→exposure.json 字段断言；无汇总行→null+注记）→实现（monitor 周期段追加 ~20 行，MON_SELFTEST 下单周期可测）→绿
- [ ] **Step 5: Commit** `scripts: exposure.json 暴露量生产端——driver 日汇总聚合+核时积分（v5 §9.4 有效分母，M4 移交项）`

---

### Task 2: M4 终审移交两项——机读 hint 契约 + 嫌疑核标签接续

**Files:**
- Modify: `tools/analysis/sdc_report.py`（`_hypothesis_evidence` 改消费 T2 `classification_hint` 的 `signals` dict 而非文本子串；嫌疑核 `cpu→拓扑标签` 解析）
- Modify: `tools/analysis/sdc_timeline.py`（新增 `cpu_to_topology_label(cpu, topology_snapshot) -> str`——读 `/sys/devices/system/cpu/cpu*/topology` 构建 cpu→`S<x>-D<y>-C<z>` 映射，供 report 差分接续）
- Test: `tools/analysis/tests/test_m5_handover.py`

**Interfaces:**
- Consumes: T2 `classification_hint(...)` 返回的 `signals` 键（`lane_fixed_address_varying` 等）；M1 pmu_core.csv 的 core 列（perf 标签）
- Produces: `cpu_to_topology_label(cpu) -> str|None`（None 时差分注记"标签不可接续"降级——不抛）；report 的 bit_hints 证据键由机读 signals 派生（T2 改措辞不再破坏 T5）

- [ ] **Step 1-4**: 失败测试（signals dict 直连断言+cpu→标签映射 fixture+None 降级）→实现→绿
- [ ] **Step 5: Commit** `tools: M4 终审移交——机读 hint 契约 + cpu→拓扑标签接续（bit_hints 不再文本子串匹配）`

---

### Task 3: 故障演练脚本族（13 类，v5 §17.2 验收基准）

**Files:**
- Create: `scripts/sdc-excite-reproduce/drills/drill_lib.sh`（公共：隔离根构造/m2_m3 工具调用/断言框架/清理）
- Create: `scripts/sdc-excite-reproduce/drills/drill_all.sh`（编排 13 类+汇总 PASS/FAIL 表+退出码）
- Create: `scripts/sdc-excite-reproduce/drills/` 每类一个：`d01_synthetic_mismatch.sh`（合成 mismatch 事件→eventd→controller RED→ring 固化→repro_queue→capsule）/`d02_test_bug.sh`（TEST_BUG 样本→真实性门禁拒→invalid 单列）/`d03_spurious_fault.sh`（journal 行→ras_keyword 事件）/`d04_ce.sh`/`d05_ue.sh`（EDAC 计数注入→事件+UE 告警语义）/`d06_panic.sh`（journal panic 行→interlock black）/`d07_runner_hang.sh`（SIGSTOP 测试进程→驱动阶段超时语义——测试进程不真停战役）/`d08_bmc_timeout.sh`（monitor mock SDR 空文件→bmc_ok=0 列+降级路径）/`d09_collector_crash.sh`（隔离 collector 副本 kill→systemd 拉起断言——用临时模板实例不碰真服务）/`d10_network_split.sh`（模拟：远端地址不可达注记——**真断网不可做**，注入离线标记文件+恢复协议断言）/`d11_disk_full.sh`（临时小文件系统 loop 设备 95% 水位→联锁语义——**不用真实磁盘**）/`d12_clock_jump.sh`（mock 时间戳跳变→时间锚定残差检测）/`d13_restore_fail.sh`（restore 读回不匹配→readback_mismatch 审计行）
- Test: `tools/telemetry/tests/test_drills.py`（pytest 驱动 drill_all.sh 本体——全部隔离根）

**Interfaces:**
- Consumes: M0-M4 全链工具（sdc_eventd/sdc_ring/sdc_controller/sdc_root_helper/reproducer/report）；m2_drill/m3_drill 既有断言模式
- Produces: `drill_all.sh` 退出码 0=13 类全过；每类产出 `drills/out/<类名>/` 证据目录（事件/账本/审计行快照）——**M5 退出标准的核心验收器**

- [ ] **Step 1-3**: drill_lib + d01-d03（TDD：pytest 断言每类跑通+证据目录形态）→ Commit `drills: 演练框架 + d01-d03（合成 mismatch/测试 bug/spurious）`
- [ ] **Step 4-6**: d04-d08 → Commit `drills: d04-d08（CE/UE/panic/hang/BMC 超时）`
- [ ] **Step 7-9**: d09-d13 + drill_all 编排 → Commit `drills: d09-d13 + drill_all 编排（13 类全链验收器，v5 §17.2）`

---

### Task 4: 81 机部署包（补丁单元 13）+ 入役清单

**Files:**
- Create: `scripts/sdc-excite-reproduce/deploy_81machine.sh`（打包器：git archive 当前分支 → tarball + install.sh + 离线依赖检查清单（rasdaemon 缺失项）+ known_faults 模板（FAN3/SEL#0x84 两行预填）+ 入役操作手册段）
- Modify: `NEW_BOARD_ONBOARDING.md`（81 机专属段：rasdaemon 启用前置/known_faults 预填/温度基线差异/无 cpufreq 列降级）
- Test: `tools/telemetry/tests/test_deploy_pack.py`（打包器单测：tarball 内容清单/known_faults 模板两行/依赖检查输出形态）

**Interfaces:**
- Produces: `dist/sdc-excite-reproduce-<git12>.tar.gz`（含 repo 快照+deploy 清单）；入役 checklist（用户远端执行——**不虚报已部署**）

- [ ] **Step 1-4**: 测试→实现→绿→真实打包一次（产物清单进报告）
- [ ] **Step 5: Commit** `scripts: 81 机部署包（tarball+依赖清单+known_faults 预填+入役手册，v5 单元 13）`

---

### Task 5: 运维手册 + 状态机恢复入口 + v5 文档终同步

**Files:**
- Create: `docs/sdc-excite-reproduce/operations-runbook.md`（v5 §16 的落地版：9 服务清单与启停顺序/BLACK 状态人工确认操作（rm controller_state 恢复流程——含前置检查单）/drill 季度排期/巡检模板（新模板——替换过时的 scripts/campaign 模板）/报告生成 SOP/capsule 重放 SOP/repro_queue 消费排班/磁盘水位与轮转策略）
- Modify: `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（§14.1 M0-M5 状态标注：已完成里程碑 as-built 标注 + M5 收尾记录）
- Modify: `docs/research/progress-log.md`（M4 结论补记——终审 Minor #4 欠账）
- Test: 无代码测试（文档）——验收为核对单（runbook 覆盖 v5 §16.1-16.7 全部操作项）

- [ ] **Step 1-3**: runbook 撰写（§16 七操作项逐项）→ v5 状态标注 → progress-log 补记
- [ ] **Step 4: Commit** `docs: 运维手册（9 服务/BLACK 恢复/演练排期/巡检新模板）+ v5 里程碑 as-built 标注 + progress-log M4 补记`

---

### Task 6: M5 收官——演练全链跑 + 72h 回溯验收 + PR

**Files:**
- Ops: `drill_all.sh` 真实跑（隔离根——13 类全过）；72h soak 回溯（运行中 9 服务自 2026-09-26 部署起的 systemd uptime+collector_self 连续性核对——**回溯既有运行数据**，不重启计时）

- [ ] **Step 1**: `bash scripts/sdc-excite-reproduce/drills/drill_all.sh` → 13/13 PASS（真实输出进报告）
- [ ] **Step 2**: 72h 回溯：`systemctl show sdc-* -p ActiveEnterTimestamp` 全服务部署时刻 + `collector_self.csv` 首末行时间跨度 + 丢样合计 0——回溯报告
- [ ] **Step 3**: 全套测试绿 + `git diff main --stat -- framework/ tests/ meson.build` 空
- [ ] **Step 4: Commit + push + PR**（API；终审由 SDD 流程派 opus 全分支审——M5 里程碑收官）

---

## 与 v5 M5 交付物的对照（自审）

| v5 M5 交付物/退出标准 | 落点 |
|---|---|
| systemd/部署 | 已达成（M2 9 服务）；T4 81 机包补齐跨机 |
| 权限隔离 | M2 root-helper allowlist 已达成；runbook T5 记录单元加固建议（NoNewPrivileges 等 M2 评审 Minor） |
| 故障演练 | T3（13 类 drill_all——v5 §17.2 清单逐项） |
| 跨机器配置 | T4（81 机包+入役清单；实际入役=用户远端操作） |
| 运维手册 | T5 |
| 退出：72h 运行 | T6 回溯（9 服务×3 天+ 既有事实——不重启计时） |
| 退出：panic/kdump/断网/磁盘/BMC 演练恢复验收 | T3 演练（d06/d10/d11/d08/d13）+ T6 全链跑 |
| M4 移交 | T1（exposure 生产端）+ T2（机读 hint+标签接续） |

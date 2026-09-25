# sdc-excite-reproduce M0（契约与基线）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 方案的 M0 阶段——命名统一迁移、canonical event 契约库、五元时间与拓扑契约、现有数据适配器、capabilities 能力探测，使历史与当前数据可被统一解析。

**Architecture:** 全部为仓库内新增/重命名的外围工具（`scripts/sdc-excite-reproduce/` + `tools/telemetry/` + `configs/sdc-excite-reproduce/`），零框架代码改动、零 meson 改动；Python 仅用 stdlib（离线机器不引第三方依赖）；CNTVCT/CNTFRQ 用独立 C helper（aarch64 守卫，不入 meson 构建）。

**Tech Stack:** bash（沿用现有脚本风格 `set -eu` + `python3 -c` 助手）、Python 3.11 stdlib（dataclasses/json/csv/re/unittest→pytest）、C（单文件 helper，`cc` 现场编译）。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5；本计划实现其 §14.1 M0 行 + §14.2 补丁单元 1/2/3 + §14.3 第 1 项；退出标准见 Task 5 与文末）

## Global Constraints

- **一补丁一单元**：每个 Task 一个 commit，自验证通过后才 commit；`git branch --show-current` 确认在特性分支 `feat/sdc-excite-reproduce-m0`（从 main 建），**绝不提交到 main**。
- **DCO**：`git commit -s`，最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容、无 Co-Authored-By。
- **术语裁定（v5 附录 D）**：所有新代码/文档用 sdc-excite-reproduce，不新引入 campaign 字样（Task 2 处理存量）。
- **x86-64 零改动**：不触碰 `framework/`、`tests/`、根 `meson.build`；C helper 带 `#if !defined(__aarch64__) #error` 守卫。
- **零第三方 Python 依赖**（机器离线）：只用 stdlib；JSON Schema 文件仅作声明式文档，校验用手写校验器。
- **战役当前已 stop+disable**（2026-09-25 用户操作）：本计划**不启动战役**；是否重启在计划外由用户决策。所有对 `~/sdc-campaign/` 的迁移在停机状态下进行。
- **root 通道**：`SDC_ROOT_PW` 经 env 传递用后不落盘（现有 collect_inventory.sh 的 su_run.py 模式）；任何删除类操作必须先向用户出示清单并获确认。
- **验证纪律**：每个验证步骤引用真实命令输出；失败即修，不带病提交。
- **文档同步**：Task 2 改名波及 README.md / NEW_BOARD_ONBOARDING.md 等活文档必须同步；`docs/superpowers/plans|output/` 下的历史文件是时点记录**不改写**。

---

### Task 1: 磁盘清理至 <85%（补丁单元 1，运维前置，无代码提交）

**Files:**
- Modify: `docs/superpowers/output/2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-plan-output.md`（追加操作记录）

**Interfaces:**
- Consumes: 无
- Produces: `/home` 用量 <85%（后续任务与 M1 深度档的前置；v5 §5.6 磁盘联锁 85% 告警线）

- [ ] **Step 1: 分析 /home 占用大户**

```bash
df -h /home && du -xh --max-depth=2 /home/sdc 2>/dev/null | sort -rh | head -25
```

Expected: 得到按大小排序的候选清单（预期大头：`builddir/`、`third-party/*/build` 中间产物、`~/sdc-campaign`（3.5G，**保留**）、旧构建目录、大文件归档）。

- [ ] **Step 2: 生成清理清单并向用户确认（删除不可逆，必须确认）**

把 Step 1 结果整理为三类：A=可安全删除（构建中间产物/可由 build.sh 重建）、B=需用户裁决（旧归档/大文件）、C=绝不删（`~/sdc-campaign` 数据、`.git`、`third-party` 源码与 install/、`docs/`）。**将 A/B 清单原文发给用户，等待明确同意后再执行 B；A 类也须用户过目。**

- [ ] **Step 3: 执行清理（仅限用户确认项）**

逐项删除并即时记录（示例，以实际确认为准）：
```bash
rm -rf <用户确认的路径> && df -h /home
```

- [ ] **Step 4: 验证 <85% 并记录**

```bash
df -h /home   # 目标 Use% < 85%
```

Expected: `Use%` 值 <85%。把"清理前/后用量、删除项、命令输出"追加到 output 文档（引用真实 df 输出）。

---

### Task 2: 命名统一迁移 campaign → sdc-excite-reproduce（补丁单元 2）

**Files:**
- Rename: `scripts/sdc-excite-reproduce/sdc_campaign.sh` → `scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh`
- Rename: `scripts/sdc-excite-reproduce/systemd/sdc-campaign.service.in` → `scripts/sdc-excite-reproduce/systemd/sdc-excite-reproduce.service.in`（同时修路径 bug）
- Rename: `scripts/sdc-excite-reproduce/logrotate/sdc-campaign` → `scripts/sdc-excite-reproduce/logrotate/sdc-excite-reproduce`
- Modify: `scripts/sdc-excite-reproduce/install.sh`、`start.sh`、`stop.sh`、`status.sh`、`sdc_common.sh`、`sdc-excite-reproduce.sh`（原 sdc_campaign.sh）、`sdc_monitor.sh`、`NEW_BOARD_ONBOARDING.md`、`README.md`（如有引用）
- Ops: `~/sdc-campaign/` → `~/sdc-excite-reproduce/`（数据根迁移，服务已停，安全）

**Interfaces:**
- Consumes: 现有 `SDC_CAMPAIGN_DIR` env（systemd `Environment=` 注入）
- Produces: 环境变量 `SDC_EXCITE_REPRODUCE_DIR`（新名，兼容回退读 `SDC_CAMPAIGN_DIR`）；默认数据根 `$HOME/sdc-excite-reproduce`；服务名 `sdc-excite-reproduce.service`；后续 Task 5 适配器以 `~/sdc-excite-reproduce/events/ledger.csv` 为真实数据源

- [ ] **Step 1: git mv 三个文件**

```bash
git branch --show-current   # 确认 feat/sdc-excite-reproduce-m0
cd scripts/sdc-excite-reproduce
git mv sdc_campaign.sh sdc-excite-reproduce.sh
git mv systemd/sdc-campaign.service.in systemd/sdc-excite-reproduce.service.in
git mv logrotate/sdc-campaign logrotate/sdc-excite-reproduce
```

- [ ] **Step 2: 修 systemd 单元（路径 bug + 服务名）**

`sdc-excite-reproduce.service.in` 的 `[Unit]` 与 `ExecStart` 改为（关键修复：`scripts/campaign/` → `scripts/sdc-excite-reproduce/`，v5 §2.3）：

```ini
[Unit]
Description=SDCShield sdc-excite-reproduce driver (L0-L5, break-point resume)
Wants=sdc-monitor.service
After=sdc-monitor.service
StartLimitIntervalSec=0

[Service]
Type=simple
User=@USER@
Group=@USER@
WorkingDirectory=@REPO@
Environment=SDC_EXCITE_REPRODUCE_DIR=@CAMPAIGN_DIR@
ExecStart=/bin/bash @REPO@/scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh full
Restart=always
RestartSec=10
TimeoutStopSec=150
KillMode=control-group

[Install]
WantedBy=multi-user.target
```

`sdc-monitor.service.in` 同步改：Description 去掉 campaign、`Environment=SDC_EXCITE_REPRODUCE_DIR=@CAMPAIGN_DIR@`、`ExecStart=/bin/bash @REPO@/scripts/sdc-excite-reproduce/sdc_monitor.sh`（sdc_monitor.sh 文件名本身无 campaign 字样，不改名）。

- [ ] **Step 3: 更新 sdc_common.sh 变量与默认值**

```bash
# 第 8 行替换为（保留旧 env 兼容回退）：
EXCITE_REPRODUCE_DIR="${SDC_EXCITE_REPRODUCE_DIR:-${SDC_CAMPAIGN_DIR:-$HOME/sdc-excite-reproduce}}"
```

文件内所有 `$CAMPAIGN_DIR` 引用改为 `$EXCITE_REPRODUCE_DIR`（`STATE_FILE/PAUSE_FLAG/CMD_DIR/LOG_ROOT/MON_DIR/EVENTS_DIR/STRESSNG_DIR/ensure_dirs/log/alert` 共 9 处）；文件头注释同步。

- [ ] **Step 4: 批量更新其余引用**

```bash
cd /home/sdc/wangxu/sdcshield
grep -rn "sdc_campaign\|sdc-campaign\|SDC_CAMPAIGN_DIR\|scripts/campaign" \
  scripts/sdc-excite-reproduce/ README.md 2>/dev/null
```

逐处替换：`install.sh`（单元名×2、logrotate 名、默认 `CAMPAIGN_DIR=/home/$RUN_USER/sdc-excite-reproduce`、echo 文案）；`start.sh`/`stop.sh`（`systemctl` 的服务名）；`status.sh`（服务名+数据根默认）；`sdc-excite-reproduce.sh` 与 `sdc_monitor.sh` 内的 `$CAMPAIGN_DIR`→`$EXCITE_REPRODUCE_DIR`、日志文案；`logrotate/sdc-excite-reproduce` 内路径。**预期 grep 复跑仅剩**：sdc_monitor.sh 文件名本身、历史注释（如有标注"原 campaign 名"的说明行）。活文档 README.md / NEW_BOARD_ONBOARDING.md 中的脚本与路径引用全部更新。

- [ ] **Step 5: 语法与干跑验证**

```bash
for f in scripts/sdc-excite-reproduce/*.sh; do bash -n "$f" || echo "FAIL $f"; done
# install.sh 干跑（不写 /etc）：模板替换到 /tmp 验证
HERE=scripts/sdc-excite-reproduce
sed "s|@REPO@|$PWD|g; s|@CAMPAIGN_DIR@|$HOME/sdc-excite-reproduce|g; s|@USER@|$USER|g" \
  "$HERE/systemd/sdc-excite-reproduce.service.in" > /tmp/unit.test
grep -c "scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh" /tmp/unit.test  # 期望 1
grep -c "scripts/campaign" /tmp/unit.test                                       # 期望 0
```

Expected: 全部 `.sh` 语法通过；干跑单元路径正确。

- [ ] **Step 6: 数据根迁移（服务已停，直接 mv）**

```bash
mv ~/sdc-campaign ~/sdc-excite-reproduce
ls ~/sdc-excite-reproduce/          # 期望: campaign.env cmd daily_summary.log driver.log events inventory logs monitor state.json stressng
python3 -c "import json; print(json.load(open('/home/sdc/sdc-excite-reproduce/state.json')))"  # 断点完整
```

注意：`campaign.env` 文件名保留（是数据文件名不是术语违规，重命名它会破坏"删除可重新生成"的探测注释约定；如需改名在 M1 顺带做）。

- [ ] **Step 7: root 侧重装单元（经用户 root 通道；机器上旧单元仍指向旧路径）**

```bash
# 以 root 运行（经 su_run.py 通道或用户协助）：
bash scripts/sdc-excite-reproduce/install.sh /home/sdc/sdc-excite-reproduce
systemctl daemon-reload
systemctl is-enabled sdc-excite-reproduce.service sdc-monitor.service  # 期望均 enabled（不启动）
ls /etc/systemd/system/sdc-campaign.service 2>&1                        # 应清理旧单元
rm -f /etc/systemd/system/sdc-campaign.service                          # 删除前向用户出示
```

Expected: 新单元 enabled 未启动；旧 `sdc-campaign.service` 移除（删除前出示给用户）。

- [ ] **Step 8: 验证 status.sh 可用（战役保持停止）**

```bash
bash scripts/sdc-excite-reproduce/status.sh 2>&1 | head -8   # 期望: monitor/campaign 两行状态输出（inactive），无路径报错
```

- [ ] **Step 9: Commit**

```bash
git add -A scripts/sdc-excite-reproduce/ README.md
git commit -s -m "scripts: 命名统一迁移 campaign→sdc-excite-reproduce（服务/脚本/logrotate/数据根 + systemd 路径 bug 修复）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
git push -u origin feat/sdc-excite-reproduce-m0
```

---

### Task 3: canonical event 契约库（schema + 校验器，v5 §5.3）

**Files:**
- Create: `configs/sdc-excite-reproduce/schemas/event.schema.json`
- Create: `tools/telemetry/sdc_event.py`
- Create: `tools/telemetry/tests/test_sdc_event.py`
- Create: `tools/telemetry/tests/__init__.py`（空文件）

**Interfaces:**
- Consumes: v5 §5.3 schema 定义
- Produces: `sdc_event.validate_event(dict) -> list[str]`（返回错误清单，空=合法）、`sdc_event.make_event(**fields) -> dict`（带默认值工厂）、`sdc_event.load_jsonl(path) -> list[dict]`；Task 5 适配器直接调用

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_sdc_event.py`：

```python
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_event

VALID = {
    "schema_version": "1.0", "event_id": "0199-test-0001",
    "event_type": "sdc_mismatch", "severity": "red", "confidence": "observed",
    "sdc-excite-reproduce_id": "20260924T002215Z-taishan2280-full-6c76ea63a7a9",
    "run_id": "0199-run-0001",
    "test": {"id": "mesh_upi_sse_asymm_distrib_int", "family": "mesh",
             "workload_type": "integer", "instruction_width_bits": 128,
             "seed": "AES:ebcafcfb", "iteration": 1, "retry": False,
             "knobs": {}},
    "result": {"verdict": "FAIL", "t_start_realtime_ns": 1, "t_end_realtime_ns": 2,
               "duration_ns": 1, "cpu_cycles": 0, "cntvct_delta": 0},
    "location": {"logical_cpu": 0, "core_id": 0, "socket_id": 0,
                 "die_id": 0, "cluster_id": 138, "numa_node": 0},
    "time": {"realtime_ns": 1, "monotonic_raw_ns": 1, "uptime_ns": 1,
             "cntvct": 1, "cntfrq_hz": 100000000, "bmc_time_raw": None,
             "mapping_error_ns": 0},
    "mismatch": {"type": "uint8", "byte_offset": 736, "suboffset": 0,
                 "lane": 92, "actual_hex": "0xff", "expected_hex": "0x7f",
                 "xor_mask_hex": "0x80", "popcount": 1},
    "environment": {"temperature_c": None, "frequency_khz": None,
                    "voltage_v": None, "package_power_w": None,
                    "pmu_window_id": None},
    "artifacts": [],
    "classification": {"primary": "candidate_hardware_sdc",
                       "alternatives": ["test_race"], "status": "open"},
}

def test_valid_event_passes():
    assert sdc_event.validate_event(VALID) == []

def test_missing_required_field_fails():
    bad = json.loads(json.dumps(VALID)); del bad["time"]
    assert any("time" in e for e in sdc_event.validate_event(bad))

def test_bad_enum_fails():
    bad = json.loads(json.dumps(VALID)); bad["severity"] = "purple"
    assert any("severity" in e for e in sdc_event.validate_event(bad))

def test_bad_hex_fails():
    bad = json.loads(json.dumps(VALID)); bad["mismatch"]["xor_mask_hex"] = "zz"
    assert any("xor_mask_hex" in e for e in sdc_event.validate_event(bad))

def test_make_event_defaults():
    ev = sdc_event.make_event(event_type="note", event_id="e1", severity="green",
                              confidence="inferred",
                              **{"sdc-excite-reproduce_id": "c1", "run_id": "r1"})
    assert ev["schema_version"] == "1.0" and ev["artifacts"] == []
    assert sdc_event.validate_event(ev) == []
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_event.py -v
```
Expected: FAIL（`ModuleNotFoundError: No module named 'sdc_event'`）。

- [ ] **Step 3: 实现 sdc_event.py + 声明式 schema**

`configs/sdc-excite-reproduce/schemas/event.schema.json`（声明式文档，逐字段对应 v5 §5.3；类型/枚举/required 与下面校验器一致）：把 v5 §5.3 的 YAML 示例逐字段转写为 JSON Schema（`$schema: https://json-schema.org/draft/2020-12/schema`，`required: [schema_version, event_id, event_type, severity, sdc-excite-reproduce_id, run_id, time]`，枚举 `event_type: [sdc_mismatch, crash, ras_event, spurious_fault, sel_event, interlock_action, phase_boundary, note]`、`severity: [green, yellow, orange, red, black]`、`confidence: [observed, inferred, hypothetical]`、`result.verdict: [PASS, FAIL, SKIP, CRASH, TIMED_OUT, INTERRUPTED, OSE]`、`classification.status: [open, qualified, invalid, test_bug, resolved]`）。

`tools/telemetry/sdc_event.py`：

```python
#!/usr/bin/env python3
"""sdc_event.py — canonical event 契约库（v5 §5.3）。stdlib only。

用法: python3 sdc_event.py validate < event.json   # 单事件校验
      python3 sdc_event.py validate-jsonl < e.jsonl
"""
import json, sys

SCHEMA_VERSION = "1.0"
SCHEMA_REF = "configs/sdc-excite-reproduce/schemas/event.schema.json"
EVENT_TYPES = {"sdc_mismatch", "crash", "ras_event", "spurious_fault",
               "sel_event", "interlock_action", "phase_boundary", "note"}
SEVERITIES = {"green", "yellow", "orange", "red", "black"}
CONFIDENCES = {"observed", "inferred", "hypothetical"}
VERDICTS = {"PASS", "FAIL", "SKIP", "CRASH", "TIMED_OUT", "INTERRUPTED", "OSE"}
CLASS_STATUS = {"open", "qualified", "invalid", "test_bug", "resolved"}
REQUIRED = ["schema_version", "event_id", "event_type", "severity",
            "sdc-excite-reproduce_id", "run_id", "time"]
_HEX_FIELDS = ["mismatch.actual_hex", "mismatch.expected_hex",
               "mismatch.xor_mask_hex"]

def _get(d, path):
    for p in path.split("."):
        if not isinstance(d, dict) or p not in d: return None
        d = d[p]
    return d

def validate_event(ev):
    errs = []
    for k in REQUIRED:
        if k not in ev: errs.append(f"missing required: {k}")
    if ev.get("schema_version") != SCHEMA_VERSION:
        errs.append(f"schema_version != {SCHEMA_VERSION}")
    for k, allowed in (("event_type", EVENT_TYPES), ("severity", SEVERITIES),
                       ("confidence", CONFIDENCES)):
        if k in ev and ev[k] not in allowed:
            errs.append(f"{k} not in {sorted(allowed)}: {ev[k]!r}")
    if _get(ev, "result.verdict") not in (None, *VERDICTS):
        errs.append(f"result.verdict not in {sorted(VERDICTS)}")
    if _get(ev, "classification.status") not in (None, *CLASS_STATUS):
        errs.append(f"classification.status not in {sorted(CLASS_STATUS)}")
    for f in _HEX_FIELDS:
        v = _get(ev, f)
        if v is not None:
            s = str(v)[2:] if str(v).startswith("0x") else str(v)
            if not s or any(c not in "0123456789abcdefABCDEF" for c in s):
                errs.append(f"{f} not hex: {v!r}")
    t = ev.get("time")
    if isinstance(t, dict) and t.get("cntfrq_hz") is not None:
        if not isinstance(t["cntfrq_hz"], int) or t["cntfrq_hz"] <= 0:
            errs.append("time.cntfrq_hz must be positive int (v5 §4.2 禁硬编码假设)")
    return errs

def make_event(**fields):
    ev = {"schema_version": SCHEMA_VERSION, "parent_event_id": None,
          "time": None,  # 必须显式出现（required 键）；legacy 导入无五元数据时保持 None
          "artifacts": [], "environment": {"temperature_c": None,
          "frequency_khz": None, "voltage_v": None, "package_power_w": None,
          "pmu_window_id": None}, "classification": {"primary": "unclassified",
          "alternatives": [], "status": "open"}}
    ev.update(fields)
    return ev

def load_jsonl(path):
    with open(path) as f:
        return [json.loads(line) for line in f if line.strip()]

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "validate"
    data = [json.load(sys.stdin)] if mode == "validate" else load_jsonl(sys.stdin)
    bad = 0
    for ev in data:
        errs = validate_event(ev)
        if errs: bad += 1; print(f"INVALID {ev.get('event_id')}: {errs}", file=sys.stderr)
    sys.exit(1 if bad else 0)
```

- [ ] **Step 4: 运行测试通过**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_event.py -v
```
Expected: 5 passed。

- [ ] **Step 5: Commit**

```bash
git add configs/sdc-excite-reproduce/schemas/event.schema.json tools/telemetry/
git commit -s -m "tools: canonical event 契约库（schema + stdlib 校验器，v5 §5.3）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 4: 五元时间 + CNTVCT/CNTFRQ helper + 拓扑快照（v5 §4.2/§4.3，§14.3 第 1 项）

**Files:**
- Create: `tools/telemetry/cntvct_helper.c`
- Create: `tools/telemetry/sdc_time.py`
- Create: `tools/telemetry/sdc_topology.py`
- Test: `tools/telemetry/tests/test_sdc_time.py`、`tools/telemetry/tests/test_sdc_topology.py`

**Interfaces:**
- Consumes: Task 3 无依赖（独立模块）
- Produces: `sdc_time.read_quintet() -> dict`、`sdc_time.anchor() -> dict`、`sdc_time.interpolate(anchors, realtime_ns) -> (cntvct_est, error_ns)`；`sdc_topology.snapshot() -> dict`（键：possible/online/offline/isolated/cpus/nodes）；Task 5 与 M1 采集器直接调用

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_sdc_time.py`：

```python
import os, sys, time
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_time

def test_quintet_fields_and_sanity():
    q = sdc_time.read_quintet()
    for k in ("realtime_ns", "monotonic_raw_ns", "uptime_ns", "cntvct", "cntfrq_hz"):
        assert isinstance(q[k], int) and q[k] > 0, k

def test_cntvct_rate_matches_wallclock():
    # 不假设 CNTVCT 纪元=开机（固件可设偏移）；只验证计数速率与墙钟一致
    a = sdc_time.read_quintet(); time.sleep(0.5); b = sdc_time.read_quintet()
    dt_wall = (b["realtime_ns"] - a["realtime_ns"]) / 1e9
    dt_cnt = (b["cntvct"] - a["cntvct"]) / a["cntfrq_hz"]
    assert abs(dt_cnt - dt_wall) < 0.1
    assert b["cntvct"] > a["cntvct"]

def test_interpolate_midpoint():
    a = {"realtime_ns": 0, "cntvct": 1000, "mapping_error_ns": 0}
    b = {"realtime_ns": 2000, "cntvct": 3000, "mapping_error_ns": 0}
    est, err = sdc_time.interpolate([a, b], 1000)
    assert est == 2000 and err >= 0
```

`tools/telemetry/tests/test_sdc_topology.py`：

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_topology

def test_snapshot_invariants():
    t = sdc_topology.snapshot()
    assert set(t["online"]) <= set(t["possible"])
    assert set(t["possible"]) == set(t["online"]) | set(t["offline"])
    for c, info in t["cpus"].items():
        assert isinstance(info["online"], bool)
        assert isinstance(info["cluster_id"], int)   # PPTT 伪影板：值可能很大，只断言类型
    assert len(t["nodes"]) >= 1

def test_run_snapshot_cli():
    import json, subprocess
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "..",
        "sdc_topology.py")], capture_output=True, text=True, check=True).stdout
    t = json.loads(out)
    assert t["possible"] == sorted(t["possible"])
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_time.py tools/telemetry/tests/test_sdc_topology.py -v
```
Expected: FAIL（模块不存在）。

- [ ] **Step 3: 实现 C helper + 两个模块**

`tools/telemetry/cntvct_helper.c`：

```c
/* cntvct_helper.c — 读 ARM64 Generic Timer（v5 §4.2）。独立编译，不入 meson。
 * 输出: cntvct=<u64> cntfrq_hz=<u64>   编译: cc -O2 -o cntvct_helper cntvct_helper.c
 */
#include <stdio.h>
#include <stdint.h>
#if !defined(__aarch64__)
#error "aarch64 only（x86-64 参考架构不走本通道，v5 §4.2）"
#endif
static inline uint64_t read_cntvct(void) {
    uint64_t v; __asm__ volatile("mrs %0, cntvct_el0" : "=r"(v)); return v;
}
static inline uint64_t read_cntfrq(void) {
    uint64_t v; __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(v)); return v;
}
int main(void) {
    printf("cntvct=%llu cntfrq_hz=%llu\n",
           (unsigned long long)read_cntvct(),
           (unsigned long long)read_cntfrq());
    return 0;
}
```

`tools/telemetry/sdc_time.py`：

```python
#!/usr/bin/env python3
"""sdc_time.py — 五元时间契约（v5 §4.2）。CNTFRQ 现场读取，绝不硬编码。stdlib only。"""
import os, re, subprocess, time

_HELPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cntvct_helper")

def _uptime_ns():
    with open("/proc/uptime") as f:
        return int(float(f.read().split()[0]) * 1e9)

def _cntvct_pair():
    out = subprocess.run([_HELPER], capture_output=True, text=True, check=True).stdout
    m = re.search(r"cntvct=(\d+) cntfrq_hz=(\d+)", out)
    if not m: raise RuntimeError(f"cntvct_helper 输出无法解析: {out!r}")
    return int(m.group(1)), int(m.group(2))

def read_quintet():
    """尽量短临界区内连续读取五元时间（顺序: realtime→monotonic_raw→cntvct→uptime）。"""
    realtime_ns = time.time_ns()
    monotonic_raw_ns = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
    cntvct, cntfrq_hz = _cntvct_pair()
    return {"realtime_ns": realtime_ns, "monotonic_raw_ns": monotonic_raw_ns,
            "uptime_ns": _uptime_ns(), "cntvct": cntvct, "cntfrq_hz": cntfrq_hz,
            "bmc_time_raw": None, "mapping_error_ns": 0}

def anchor():
    """10min 周期锚定样本：收口再读一次 realtime 估计残余（v5 §4.2）。"""
    a = read_quintet()
    a["mapping_error_ns"] = time.time_ns() - a["realtime_ns"]
    return a

def interpolate(anchors, realtime_ns):
    """最近两锚点线性插值 → (cntvct 估计, 误差界 ns)。anchors 按 realtime 升序。"""
    if len(anchors) < 2:
        raise ValueError("需要 ≥2 个锚点")
    a, b = anchors[-2], anchors[-1]
    if realtime_ns < a["realtime_ns"] or realtime_ns > b["realtime_ns"]:
        a, b = anchors[0], anchors[1]  # 越界样本：用首对并放大误差
    span = b["realtime_ns"] - a["realtime_ns"]
    frac = (realtime_ns - a["realtime_ns"]) / span
    est = a["cntvct"] + frac * (b["cntvct"] - a["cntvct"])
    err = max(a.get("mapping_error_ns", 0), b.get("mapping_error_ns", 0)) + span
    return int(est), err

if __name__ == "__main__":
    import json; print(json.dumps(anchor()))
```

`tools/telemetry/sdc_topology.py`：

```python
#!/usr/bin/env python3
"""sdc_topology.py — 拓扑快照（v5 §4.3）。不假设 package/cluster 连续或从 0 起（PPTT 伪影）。"""
import glob, json, os

_SYS_CPU = "/sys/devices/system/cpu"

def _read(path, default=None):
    try:
        return open(path).read().strip()
    except OSError:
        return default

def _expand(cpulist):
    out = []
    for part in (cpulist or "").split(","):
        if not part: continue
        if "-" in part:
            lo, hi = part.split("-"); out.extend(range(int(lo), int(hi) + 1))
        else:
            out.append(int(part))
    return out

def snapshot():
    possible = _expand(_read(f"{_SYS_CPU}/possible"))
    online = _expand(_read(f"{_SYS_CPU}/online"))
    cpus = {}
    for c in possible:
        base = f"{_SYS_CPU}/cpu{c}"
        cpus[str(c)] = {
            "online": c in online,
            "package": int(_read(f"{base}/topology/physical_package_id", -1)),
            "core_id": int(_read(f"{base}/topology/core_id", -1)),
            "cluster_id": int(_read(f"{base}/topology/cluster_id", -1)),
            "die_id": int(_read(f"{base}/topology/die_id", -1)),
        }
    nodes = {}
    for n in sorted(glob.glob("/sys/devices/system/node/node*"),
                    key=lambda p: int(os.path.basename(p)[4:])):
        nid = os.path.basename(n)[4:]
        nodes[nid] = {"cpulist": _read(f"{n}/cpulist", ""),
                      "has_memory": os.path.exists(f"{n}/meminfo")}
    return {"possible": possible,
            "online": online,
            "offline": sorted(set(possible) - set(online)),
            "isolated": _expand(_read(f"{_SYS_CPU}/isolated")),
            "cpus": cpus, "nodes": nodes}

if __name__ == "__main__":
    print(json.dumps(snapshot(), indent=1))
```

- [ ] **Step 4: 编译 helper 并运行测试**

```bash
cc -O2 -o tools/telemetry/cntvct_helper tools/telemetry/cntvct_helper.c
python3 -m pytest tools/telemetry/tests/test_sdc_time.py tools/telemetry/tests/test_sdc_topology.py -v
python3 tools/telemetry/sdc_time.py    # 人工核对: cntfrq_hz 为本机实测值（不假设 100MHz）
python3 tools/telemetry/sdc_topology.py | python3 -c "import json,sys; t=json.load(sys.stdin); print('cpus:', len(t['possible']), 'nodes:', sorted(t['nodes']), 'mem_nodes:', [k for k,v in t['nodes'].items() if v['has_memory']])"
```
Expected: 测试全过；拓扑输出与本机事实一致（128 CPU、4 NUMA、内存节点 1/3——v5 附录 B）。

- [ ] **Step 5: Commit**

```bash
echo "cntvct_helper" >> tools/telemetry/.gitignore   # 编译产物不入库
git add tools/telemetry/
git commit -s -m "tools: 五元时间 + CNTVCT/CNTFRQ helper + 拓扑快照契约（v5 §4.2/§4.3）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 5: 现有数据适配器（ledger/事件目录/YAML → canonical events）

**Files:**
- Create: `tools/telemetry/sdc_legacy_adapter.py`
- Test: `tools/telemetry/tests/test_sdc_legacy_adapter.py`
- Test fixture: `tools/telemetry/tests/fixtures/ledger.csv`（真实台账 8 行副本）、`tools/telemetry/tests/fixtures/yaml_extract.txt`（真实事件 #1 mesh 提取块节选）

**Interfaces:**
- Consumes: Task 3 `sdc_event.make_event/validate_event`；数据格式 = 现有 `~/sdc-excite-reproduce/events/ledger.csv` 与事件目录（`yaml_extract.txt`/`retests.txt`）
- Produces: `parse_ledger(path) -> list[dict]`、`parse_yaml_fail_blocks(text) -> list[dict]`、`parse_event_dir(path) -> dict`、`convert_ledger(path, sdc_id) -> list[dict]`（canonical events）；M1 `sdc-eventd` 的输入端

- [ ] **Step 1: 建 fixture（真实数据副本，无凭据）**

```bash
mkdir -p tools/telemetry/tests/fixtures
cp ~/sdc-excite-reproduce/events/ledger.csv tools/telemetry/tests/fixtures/ledger.csv
# 真实失败块样本：2026-09-25 cold_c4 事件的 stdout_summary（含 - test:/result:/fail: 块；
# 注：mesh 事件 #1 的证据目录已在战役中被清除，故用现存最近的真事件）
cp ~/sdc-excite-reproduce/events/20260925-082233-cold_c4-rc137/stdout_summary.out \
   tools/telemetry/tests/fixtures/stdout_summary.txt
wc -l tools/telemetry/tests/fixtures/ledger.csv   # 期望 8
grep -c "^- test:" tools/telemetry/tests/fixtures/stdout_summary.txt  # 期望 ≥2（memcpy_rewr/cachebounce）
```

- [ ] **Step 2: 写失败测试**

`tools/telemetry/tests/test_sdc_legacy_adapter.py`：

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_legacy_adapter as ada

FIX = os.path.join(os.path.dirname(__file__), "fixtures")

def test_parse_ledger_all_rows():
    rows = ada.parse_ledger(os.path.join(FIX, "ledger.csv"))
    assert len(rows) == 8
    mesh = [r for r in rows if "mesh_upi" in r.get("test", "")]
    assert len(mesh) == 1 and mesh[0]["seed"] == "AES:ebcafcfb"

def test_convert_validates_against_schema():
    import sdc_event
    evs = ada.convert_ledger(os.path.join(FIX, "ledger.csv"),
                             "20260924T002215Z-taishan2280-full-6c76ea63a7a9")
    assert len(evs) == 8
    for ev in evs:
        assert sdc_event.validate_event(ev) == [], ev.get("event_id")

def test_mesh_event_classification_from_note():
    evs = ada.convert_ledger(os.path.join(FIX, "ledger.csv"), "c0")
    mesh = [e for e in evs if "mesh_upi" in (e.get("test") or {}).get("id", "")]
    assert mesh[0]["classification"]["status"] == "resolved"
    assert mesh[0]["classification"]["primary"] == "test_race"

def test_parse_yaml_fail_block():
    blocks = ada.parse_yaml_fail_blocks(open(os.path.join(FIX, "stdout_summary.txt")).read())
    assert len(blocks) >= 2                      # memcpy_rewr + cachebounce
    assert blocks[0]["test"] == "memcpy_rewr" and blocks[0]["result"] == "crash"
    assert "seed" in blocks[0]["fail"]           # fail 行含 seed（连字符键 cpu-mask/time-to-fail 也要能解析）
    assert "cpu-mask" in blocks[0]["fail"]
```

- [ ] **Step 3: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_legacy_adapter.py -v
```
Expected: FAIL（模块不存在）。

- [ ] **Step 4: 实现适配器**

`tools/telemetry/sdc_legacy_adapter.py`：

```python
#!/usr/bin/env python3
"""sdc_legacy_adapter.py — 现有 ledger.csv/事件目录/sdcshield YAML → canonical events。

M0 范围（诚实声明，偏离 v5 M0 退出标准的部分）：CORE179 仓库资产为 Markdown
叙述性报告（docs/cases/sdc1-01-02-core179/），机器可读时间线重建属 M4 报告
工具；本适配器覆盖 ledger.csv + 事件目录 + YAML 失败块（当前数据全量）。
"""
import csv, json, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sdc_event

_NOTE_RESOLVED = ("定案", "定性", "伪事件", "已修复")
_TEST_RACE_KEYS = ("竞态", "test_bug", "伪SDC", "伪 SDC")

def _ts_to_ns(ts):  # "2026-09-24 09:14:10"（本地时区）→ realtime_ns
    return int(time.mktime(time.strptime(ts, "%Y-%m-%d %H:%M:%S")) * 1e9)


def _kv(field):  # "rc=137" / "test=mesh..." / "seed=AES:.."
    m = re.match(r"^(rc|test|seed)=(.*)$", field.strip())
    return (m.group(1), m.group(2)) if m else (None, field.strip())

def parse_ledger(path):
    rows = []
    with open(path, newline="") as f:
        for raw in csv.reader(f):
            if not raw or not raw[0].strip(): continue
            row = {"ts": raw[0].strip(), "label": raw[1].strip() if len(raw) > 1 else "",
                   "retests": "", "dir": "", "note": ""}
            kv, free = {}, []
            for i, cell in enumerate(raw[2:], start=2):
                k, v = _kv(cell)
                if k: kv[k] = v
                elif re.match(r"^retest\d", cell.strip()): row["retests"] = cell.strip()
                elif cell.startswith("/"): row["dir"] = cell.strip()
                else: free.append(cell.strip())
            row.update(kv); row["note"] = "；".join(x for x in free if x)
            rows.append(row)
    return rows

def parse_yaml_fail_blocks(text):
    """提取 '- test: <id>' … 'result: fail|crash' 块的关键字段（含连字符键 cpu-mask/time-to-fail）。"""
    blocks, cur = [], None
    for line in text.splitlines():
        m = re.match(r"^- test:\s*(\S+)", line)
        if m:
            if cur: blocks.append(cur)
            cur = {"test": m.group(1)}
            continue
        if cur is None: continue
        m = re.match(r"^  result:\s*(\S+)", line)
        if m: cur["result"] = m.group(1)
        m = re.match(r"^  fail:\s*\{(.*)\}", line)
        if m:
            pairs = re.findall(r"([\w-]+):\s*'([^']*)'|([\w-]+):\s*([\w.]+)", m.group(1))
            cur["fail"] = {(a or c): (b or d) for a, b, c, d in pairs}
    if cur: blocks.append(cur)
    return [b for b in blocks if b.get("result") in ("fail", "crash")]

def parse_event_dir(path):
    out = {"dir": path}
    yx = os.path.join(path, "yaml_extract.txt")
    if os.path.exists(yx):
        out["fail_blocks"] = parse_yaml_fail_blocks(open(yx).read())
    rt = os.path.join(path, "retests.txt")
    if os.path.exists(rt):
        out["retests"] = [l.strip() for l in open(rt) if l.strip()]
    return out

def convert_ledger(path, sdc_id):
    events = []
    for i, row in enumerate(parse_ledger(path), 1):
        rc = (row.get("rc") or "").replace("rc=", "")
        is_fail = row.get("test") and rc in ("1", "134", "137", "139")
        etype = ("sdc_mismatch" if is_fail else
                 "interlock_action" if "drill" in row["label"] else "note")
        note = row.get("note", "")
        status = "resolved" if any(k in note for k in _NOTE_RESOLVED) else "open"
        primary = ("test_race" if any(k in note for k in _TEST_RACE_KEYS)
                   else "candidate_hardware_sdc" if is_fail else "operational")
        ev = sdc_event.make_event(
            event_id=f"legacy-{i:04d}", event_type=etype,
            severity="red" if is_fail else "green",
            confidence="observed",
            **{"sdc-excite-reproduce_id": sdc_id, "run_id": row["label"]},
            test={"id": row.get("test") or "unknown", "family": "", "seed": row.get("seed")},
            time={"realtime_ns": _ts_to_ns(row["ts"]), "monotonic_raw_ns": None,
                  "uptime_ns": None, "cntvct": None, "cntfrq_hz": None,
                  "bmc_time_raw": None, "mapping_error_ns": None},
            classification={"primary": primary, "alternatives": [], "status": status},
            artifacts=[row["dir"]] if row.get("dir") else [])
        ev["legacy_row"] = row
        events.append(ev)
    return events

if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
        "~/sdc-excite-reproduce/events/ledger.csv")
    sid = sys.argv[2] if len(sys.argv) > 2 else "legacy-import"
    for ev in convert_ledger(src, sid):
        print(json.dumps(ev, ensure_ascii=False))
```

- [ ] **Step 5: 运行测试 + 真实数据验证（M0 退出标准）**

```bash
python3 -m pytest tools/telemetry/tests/ -v
python3 tools/telemetry/sdc_legacy_adapter.py ~/sdc-excite-reproduce/events/ledger.csv m0-import \
  | python3 -c "import sys,json; evs=[json.loads(l) for l in sys.stdin]; print(len(evs), 'events'); print(sorted({e['classification']['status'] for e in evs}))"
```
Expected: 全部测试通过；真实台账 → 8 个 canonical events，status 集合含 `resolved`（mesh/伪事件行）与 `open`（2026-09-25 未定性行）。

- [ ] **Step 6: Commit**

```bash
git add tools/telemetry/
git commit -s -m "tools: 现有 ledger/事件目录/YAML → canonical events 适配器（M0 退出标准达成）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 6: collect_inventory v2 — capabilities.env + known_faults（补丁单元 3）

**Files:**
- Modify: `scripts/sdc-excite-reproduce/collect_inventory.sh`（新增第 16 节）
- Create: `configs/sdc-excite-reproduce/known_faults.csv`（格式定义 + 本机空表）

**Interfaces:**
- Consumes: 现有 collect_inventory.sh 的 root 三级通道（`runc`）与输出目录约定 `<SN>-<日期>/`
- Produces: `16_capabilities.env`（KEY=VALUE，shell 可 source；M1 采集器与控制器读取）+ `known_faults.csv` 行格式 `source,type,description,whitelist_action`；四轴能力探测只读项（v5 §7.4；governor 写入实验**不在本任务**——R1 需用户批准，探测项输出 `GOVERNOR_RESPONSE_VERIFIED=unknown`）

- [ ] **Step 1: 定义 known_faults.csv 格式（本机空表）**

`configs/sdc-excite-reproduce/known_faults.csv`：

```csv
source,type,description,whitelist_action
# 已知环境故障白名单（v5 §6.4：known_faults 之外断言即告警）。
# source = SEL事件ID | 传感器名 | 内核关键字；whitelist_action = ignore | log_only
# 本机（TaiShan 2280 SN 2102312YVY10M6000038）暂无已知故障行——首次发现经用户定案后追加。
# 81 机（TG225 B1）入役时按其画像追加：SEL #0x84 周期断言 / FAN3 0rpm 两行（log_only）。
```

- [ ] **Step 2: 在 collect_inventory.sh 汇总节前插入第 16 节**

在 `# ---------------- 汇总 ----------------` 标记之前插入（沿用现有 `runc`/非 root 双通道风格）：

```bash
# ---------------- v2: capabilities.env（16，v5 §7.4/§16.2 四轴能力探测·只读）----------------
# 沿用本脚本既有变量：$OUT（输出目录 <SN>-<日期>）、$ROOT_MODE（root 三级通道）、$GAPS
CAP="$OUT/16_capabilities.env"
cpu0f=/sys/devices/system/cpu/cpu0/cpufreq
{
  echo "# capabilities.env — collect_inventory v2 生成（消费方: M1 采集器/控制器）"
  echo "NPROC=$(nproc)"
  echo "HAS_CPUFREQ=$([ -d "$cpu0f" ] && echo yes || echo no)"
  echo "CPUFREQ_DRIVER=$(readlink -f "$cpu0f" 2>/dev/null | xargs -r basename)"
  echo "GOVERNOR=$(cat "$cpu0f/scaling_governor" 2>/dev/null || echo none)"
  echo "HAS_TIME_IN_STATE=$([ -f "$cpu0f/stats/time_in_state" ] && echo yes || echo no)"
  echo "GOVERNOR_RESPONSE_VERIFIED=unknown  # R1 写入实验需用户批准后单做（v5 §7.4.1）"
  echo "HAS_OEM_VOLTAGE=no  # 2026-09-24 全量探测定案: 0x30 0x91-0x98 被 0xD6 封印（v5 附录 B）"
  echo "CPU_ONLINE_WRITABLE=$([ "$ROOT_MODE" != none ] && [ -w /sys/devices/system/cpu/cpu1/online ] && echo yes || echo unknown)"
  echo "PMU_L3C_COUNT=$(ls -d /sys/bus/event_source/devices/hisi_sccl*_l3c* 2>/dev/null | wc -l)"
  echo "PMU_HHA_COUNT=$(ls -d /sys/bus/event_source/devices/hisi_sccl*_hha* 2>/dev/null | wc -l)"
  echo "PMU_DDRC_COUNT=$(ls -d /sys/bus/event_source/devices/hisi_sccl*_ddrc* 2>/dev/null | wc -l)"
  echo "EDAC_MC_COUNT=$(ls -d /sys/devices/system/edac/mc/mc* 2>/dev/null | wc -l)"
  echo "RASDAEMON_ACTIVE=$(systemctl is-active rasdaemon 2>/dev/null || echo unknown)"
  for t in BERT EINJ HEST ERST; do
    echo "ACPI_HAS_$t=$(ls /sys/firmware/acpi/tables/ 2>/dev/null | grep -cq "^$t" && echo yes || echo no)"
  done
  echo "MEM_NODES=$(for n in /sys/devices/system/node/node*; do [ -f "$n/meminfo" ] && basename "$n" | tr -d a-z; done | tr '\n' ' ')"
} > "$CAP"
cp "$CAP" "${SDC_EXCITE_REPRODUCE_DIR:-$HOME/sdc-excite-reproduce}/capabilities.env" 2>/dev/null \
  || echo "note: 数据根不存在，capabilities.env 仅存于画像目录" >&2
echo "16_capabilities.env 完成: $(wc -l < "$CAP") 行"
```

（`ROOT_MODE` 变量与 `$OUT_DIR` 沿用脚本既有定义；如变量名不同以实际为准对齐。）

- [ ] **Step 3: 运行并对照 v5 附录 B 事实核验**

```bash
cd scripts/sdc-excite-reproduce && ./collect_inventory.sh /tmp/inv-v2-test
cat /tmp/inv-v2-test/*-2026-09-25/16_capabilities.env
```
Expected（本机事实，v5 附录 B）：`NPROC=128`；`HAS_CPUFREQ=yes`（cppc）；`HAS_TIME_IN_STATE=no`；`HAS_OEM_VOLTAGE=no`；`PMU_L3C_COUNT=32`、`PMU_HHA_COUNT=8`、`PMU_DDRC_COUNT=16`；`MEM_NODES=1 3`；`RASDAEMON_ACTIVE=active`。任何一项不符即查因（不得改预期迁就输出）。

- [ ] **Step 4: Commit**

```bash
git add scripts/sdc-excite-reproduce/collect_inventory.sh configs/sdc-excite-reproduce/known_faults.csv
git commit -s -m "scripts: collect_inventory v2 — capabilities.env 四轴只读探测 + known_faults 注册表（v5 补丁单元 3）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

## 收尾验证（全计划回归）

- [ ] 全测试：`python3 -m pytest tools/telemetry/tests/ -v` → 全过
- [ ] 框架零改动复查：`git diff main --stat -- framework/ tests/ meson.build` → 空
- [ ] 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`（引用真实输出）
- [ ] 推送：`git push origin feat/sdc-excite-reproduce-m0`
- [ ] 战役重启 = 用户决策（本计划不启动；启动前建议先处理巡检遗留的 cold_c4/rc=137 与 dwell OOM 定性——见记忆 sdc-campaign-inspection-20260925）

## 与 v5 M0 退出标准的偏差（诚实记录）

| v5 M0 退出标准 | 本计划达成 | 偏差 |
|---|---|---|
| 历史 CORE179 数据可被统一解析 | Task 5 覆盖 ledger/事件目录/YAML（当前数据全量） | CORE179 仓库资产为 Markdown 报告，机器可读时间线重建移至 M4 报告工具（Task 5 模块 docstring 已声明） |
| 时间映射与拓扑单测通过 | Task 4 | 无偏差 |
| schema/profile/身份时间拓扑库/日志适配器 | Task 3-5（schema+时间拓扑+适配器） | profile schema（v5 §7.5）移至 M3（其消费者 victim/aggressor runner 在 M3 才出现） |
| A/B 开销基线 | 未列入 | 依赖 M1 采集器存在才有对照物，移至 M1 plan 首任务 |

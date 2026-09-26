# sdc-excite-reproduce M2（规则闭环）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 M2 的规则闭环——`sdc-eventd`（多源规范化事件流）、120s ring 窗口、GREEN-BLACK 状态机控制器（规则驱动、append-only 决策账本、重放确定性）、`sdc-root-helper`（allowlist 特权动作含 CPU hotplug）、事件 burst 与恢复机制，以合成事件演练达成 M2 退出标准。

**Architecture:** 三个新守护进程（用户态 eventd/controller + root 的 root-helper）经数据根 `spool/` 与 `cmd/` 文件协议解耦：eventd 尾随 ledger/discrete/journal/collector_self 四源产出 canonical events（复用 M0 `sdc_event` 契约）；controller 消费事件按 YAML 规则驱动五态机，决策全量入 append-only 账本；root-helper 只执行 allowlist JSON 动作并记录前后状态供恢复。burst 由 controller 经 helper 触发短窗深采。

**Tech Stack:** Python 3.11 stdlib（json/collections/time/re）、YAML 子集（规则文件用 JSON 表达避免 yaml 依赖——**零第三方依赖**不变）、bash（安装脚本）、systemd。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5 §3.1 组件/§5.3 schema/§8 控制器/§13 工程/§16.3-16.5 运行手册；§14.1 M2 行 + 退出标准）。范围：v5 M2 = §14.2 单元 8（偏移引擎事件化已在 monitor v3 覆盖其输入）+ ring/burst（M1 偏差表移交项）+ BPF canary（探测可用则 bpftrace，否则降级保持 dmesg——实现为能力探测，不强制）。

## Global Constraints

- **一补丁一单元**：每 Task 一 commit；`git commit -s` 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>` 其后无任何内容、无 Co-Authored-By；分支 `feat/sdc-excite-reproduce-m2`（从 main 建——M1b PR 合并后）；绝不提交/推送 main（集成经 PR）。
- **战役运行中**（cycle 5+，四服务 active）：实现/测试不重启既有服务；T6 部署新增三服务（eventd/controller 用户态 + root-helper root），不动战役与采集器；**任何测试不得对真实进程 pkill/kill**（M1b T5 教训：SDC_ORPHAN_PIDS 沙盒纪律——M2 测试全部 fixture/注入目录驱动）。
- **零第三方依赖**：规则文件用 JSON（不是 YAML——避免 pyyaml）；canonical event 复用 `tools/telemetry/sdc_event.py`（M0 契约，36+ 测试基线）。
- **重放确定性（v5 §8.2）**：同一事件序列输入 controller 必须产生相同状态转换与动作序列（账本 diff 为空）——T3 的核心验收。
- **root-helper 安全（v5 §13.2）**：allowlist 固定动作枚举 + 参数范围校验 + nonce + 审计（调用者/动作/旧值/新值/读回）；**拒绝任意 shell**（请求体含 shell 元字符即拒绝并审计）。
- **CPU hotplug（v5 §7.4.4）**：只在 run 边界、保存原 online 集、恢复读回；CPU0 拒绝 offline。
- `env -u SDC_ROOT_PW` 非根验证纪律；root 经 `/tmp/sdc_inv_stage/su_run.py`。
- 测试期望基线：以执行时 main 的 tools/telemetry 全套 passed 数为准（M1b 后 ≈66+），每 Task 报告实际数。

---

### Task 1: sdc-eventd——多源尾随规范化事件流

**Files:**
- Create: `tools/telemetry/sdc_eventd.py`
- Test: `tools/telemetry/tests/test_sdc_eventd.py`

**Interfaces:**
- Consumes: `sdc_event.make_event/validate_event`（M0）；数据根四源文件（ledger.csv / monitor/discrete_events.log / monitor/journal_watch.log / monitor/collector_self.csv）
- Produces:
  - `tail_file(path, offset_store) -> (new_lines, new_offset)`：通用尾随（offset 持久化于 `spool/tail_offsets.json`，文件截断/轮转检测→重置）
  - `ledger_line_to_event(line, sid) -> dict | None`：复用 `sdc_legacy_adapter.parse_ledger` 行解析（直接 import）→ canonical event（event_type 按 M0-T5 语义：mismatch/crash→`sdc_mismatch`/`crash`，drill→`note`）
  - `discrete_line_to_event(line, sid) -> dict | None`：`[ts] TRANSITION name: 旧 → 新` → `event_type=discrete_transition`，非 0x00 断言 → `severity=orange`
  - `interlock_line_to_event(line, sid) -> dict | None`：`monitor/alerts.log` 的 `ALERT PAUSE: X` / `ALERT RESUME: X` / `热升级.*KILL` 行 → `event_type=interlock_action`（PAUSE/KILL→severity=black，RESUME→green）——M2 退出标准"温度越限触发正确动作"的输入链（monitor 联锁 PAUSE 行即源，v5 §8.2 BLACK 的最小实现）
  - `selfmon_line_to_event(line, sid) -> dict | None`：collector 停更/丢样（samples_dropped 增量>0）→ `event_type=collector_degraded`
  - `EventdLoop(data_root, spool_dir).poll_once() -> list[dict]`：一轮四源尾随 + 去重（同源同内容 hash 跳过）+ append `spool/events.jsonl` + 返回新事件
  - CLI：`python3 sdc_eventd.py [--once] [--data-root DIR]`（--once 跑一轮退出=测试/演练模式；无参=守护循环 2s 轮询）

- [ ] **Step 1: 失败测试**（fixture：tmp 数据根内造四个源文件的多行内容）：

```python
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_eventd

def make_root(tmp):
    d = os.path.join(tmp, "data"); os.makedirs(f"{d}/monitor"); os.makedirs(f"{d}/spool")
    return d

def test_tail_file_incremental_and_rotation(tmp_path):
    f = tmp_path / "src.log"; f.write_text("a\n")
    off = {}
    lines, off = sdc_eventd.tail_file(str(f), off)
    assert lines == ["a"] and off[str(f)] == 2
    f.write_text("a\nb\nc\n")
    lines, off = sdc_eventd.tail_file(str(f), off)
    assert lines == ["b", "c"]
    f.write_text("x\n")                      # 轮转/截断：offset > size → 重置
    lines, off = sdc_eventd.tail_file(str(f), off)
    assert lines == ["x"]

def test_ledger_line_to_event():
    ev = sdc_eventd.ledger_line_to_event(
        "2026-09-26 10:00:00,cold_c5,rc=137,test=memcpy_rewr,seed=AES:ab,retests...", "s0")
    assert ev["event_type"] == "sdc_mismatch" and ev["test"]["id"] == "memcpy_rewr"

def test_discrete_assert_severity():
    ev = sdc_eventd.discrete_line_to_event(
        "[2026-09-26 10:00:00] TRANSITION cpu1_prochot: 0x00 → 0x01", "s0")
    assert ev["event_type"] == "discrete_transition" and ev["severity"] == "orange"
    ok = sdc_eventd.discrete_line_to_event(
        "[2026-09-26 10:00:00] TRANSITION psu: 0x00 → 0x00 ", "s0")
    assert ok is None                      # 值未变不产生事件

def test_eventd_loop_poll_once(tmp_path):
    d = make_root(str(tmp_path))
    open(f"{d}/events/ledger.csv", "w") if os.path.isdir(f"{d}/events") else \
        (os.makedirs(f"{d}/events"), open(f"{d}/events/ledger.csv", "w"))
    with open(f"{d}/events/ledger.csv", "w") as f:
        f.write("2026-09-26 10:00:00,cold_c5,rc=137,test=zstd19,seed=AES:ab,x\n")
    with open(f"{d}/monitor/discrete_events.log", "w") as f:
        f.write("[2026-09-26 10:00:01] TRANSITION cpu1_prochot: 0x00 → 0x01\n")
    loop = sdc_eventd.EventdLoop(d, f"{d}/spool")
    evs = loop.poll_once()
    types = sorted(e["event_type"] for e in evs)
    assert "sdc_mismatch" in types and "discrete_transition" in types
    for e in evs: assert sdc_event.validate_event(e) == []
    out = [json.loads(l) for l in open(f"{d}/spool/events.jsonl")]
    assert len(out) == len(evs)          # 全部落盘
    assert loop.poll_once() == []        # 第二轮无新行
```

- [ ] **Step 2-4: 红→实现→绿**。实现要点：`tail_file` 用 `os.path.getsize` 检测截断；去重键 = sha1(源名+行内容)[:16]，已见集持久化 `spool/dedup.json`（上限 4096 条 FIFO 淘汰）；事件 ts 解析失败用当前时刻并记 `mapping_error_ns`；`journal_watch.log` 行→`event_type=ras_keyword`（severity 按行含 UE/panic→red 否则 yellow）。
- [ ] **Step 5: Commit** `tools: sdc-eventd——四源尾随规范化事件流（v5 §3.1，M2）`

---

### Task 2: 120s ring 窗口与事件固化

**Files:**
- Create: `tools/telemetry/sdc_ring.py`
- Test: `tools/telemetry/tests/test_sdc_ring.py`

**Interfaces:**
- Consumes: monitor.csv / percore.csv 尾随（tail_file）；`spool/events.jsonl`
- Produces:
  - `class RingWindow(max_seconds=120)`：`push(csv_name, line)`（内存 deque，按行内 ts 字段淘汰过期——ts 取行首字段 `%F %T`）；`snapshot() -> {csv_name: [lines]}`
  - `freeze(window, out_dir, event_id)`：窗口全部行写 `events/<event_id>/ring_window/{monitor,percore}.csv`（带 header 注释行 `# frozen at <ts> for <event_id> [-60s,+60s] 口径`）
  - `RingLoop(data_root, spool_dir).poll_once() -> dict`：尾随两 CSV 入窗 + 扫描 events.jsonl 新行中 `severity in (red, black)` 的事件 → freeze 到该 event_id 目录（无 event_id 目录则建 `spool/frozen/<ts>-<hash>/`）并返回 `{"frozen": [event_ids]}`
  - CLI：`python3 sdc_ring.py [--once] [--data-root DIR]`（守护 2s）

- [ ] **Step 1: 失败测试**：

```python
import os, sys, time, datetime
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_ring

def ts(offset_s):
    return (datetime.datetime.now() - datetime.timedelta(seconds=offset_s)).strftime("%F %T")

def test_ring_eviction_and_snapshot():
    r = sdc_ring.RingWindow(max_seconds=120)
    r.push("monitor", f"{ts(200)},old")     # 过期
    r.push("monitor", f"{ts(10)},new")
    snap = r.snapshot()
    assert snap["monitor"] == [f"{ts(10)},new"]

def test_ring_loop_freezes_on_red(tmp_path):
    d = os.path.join(str(tmp_path), "data")
    for p in ("monitor", "spool", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/monitor/monitor.csv", "w") as f:
        f.write("ts,c1\n" + f"{ts(5)},45\n" + f"{ts(1)},50\n")
    import json
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps({"event_id": "e1", "event_type": "sdc_mismatch",
                            "severity": "red", "time": {"realtime_ns": 1}}) + "\n")
    loop = sdc_ring.RingLoop(d, f"{d}/spool")
    r = loop.poll_once()
    assert r["frozen"] == ["e1"]
    out = f"{d}/events/e1/ring_window/monitor.csv"
    assert "50" in open(out).read() and "45" in open(out).read()
    assert loop.poll_once() == {"frozen": []}   # 已处理行不重复固化
```

- [ ] **Step 2-4: 红→实现→绿**（events.jsonl 的消费 offset 持久化 `spool/ring_offset.json`；ts 解析失败的行保窗不淘汰——诚实保留）。
- [ ] **Step 5: Commit** `tools: 120s ring 窗口 + red 事件固化（v5 §6.3，M1 移交项）`

---

### Task 3: sdc-controller——五态状态机（规则驱动 + 重放确定性）

**Files:**
- Create: `tools/control/sdc_controller.py`（目录 tools/control/ 新建，`tests/__init__.py` 沿用 tools/telemetry/tests/ 还是新建？——**新建 `tools/control/tests/`**，pytest 两个目录都跑：`pytest tools/telemetry/tests tools/control/tests`；根 .gitignore 已盖 __pycache__）
- Create: `configs/sdc-excite-reproduce/rules_m2.json`（首版规则）
- Test: `tools/control/tests/test_sdc_controller.py`

**Interfaces:**
- Consumes: `spool/events.jsonl`（尾随）
- Produces:
  - `class StateMachine(rules) -> obj`：`.state`（green/yellow/orange/red/black）、`.feed(event) -> list[action_dict]`（纯函数式——只依赖当前 state+event+规则，无时钟随机）；
  - 规则 JSON 形（v5 §8.3 首版四规则翻译）：
```json
{
  "schema_version": "1",
  "rules": [
    {"id": "exact_mismatch", "when": {"event_type": "sdc_mismatch"},
     "transition": "red", "actions": ["freeze_ring", "snapshot_root", "enqueue_reproduction", "hold_profile"]},
    {"id": "discrete_assert", "when": {"event_type": "discrete_transition", "severity": "orange"},
     "transition": "orange", "actions": ["pmu_burst"]},
    {"id": "ras_keyword", "when": {"event_type": "ras_keyword", "severity": "red"},
     "transition": "red", "actions": ["snapshot_root"]},
    {"id": "collector_degraded", "when": {"event_type": "collector_degraded"},
     "transition": "yellow", "actions": ["alert_only"]},
    {"id": "safety_interlock", "when": {"event_type": "interlock_action"},
     "transition": "black", "actions": ["verify_only"]}
  ],
  "transitions": {
    "green": {"yellow": ["collector_degraded"], "orange": ["discrete_assert"], "red": ["exact_mismatch", "ras_keyword"], "black": ["safety_interlock"]},
    "yellow": {"green": [], "orange": ["discrete_assert"], "red": ["exact_mismatch", "ras_keyword"], "black": ["safety_interlock"]},
    "orange": {"red": ["exact_mismatch", "ras_keyword"], "black": ["safety_interlock"]},
    "red": {"black": ["safety_interlock"]},
    "black": {}
  }
}
```
  （`when` 全字段等值匹配；`transitions` 表约束合法性——不合法转换记录 `ignored` 不动状态；actions 枚举固定：`freeze_ring/snapshot_root/enqueue_reproduction/hold_profile/pmu_burst/alert_only/verify_only`）
  - `ControllerLoop(data_root, spool_dir, rules_path).poll_once() -> list[decision]`：feed 新事件 → 决策行 `{"ts", "event_id", "rule", "from", "to", "actions", "ignored": bool}` 追加 `spool/controller_ledger.jsonl` + 动作请求落 `cmd/`（见下）
  - **动作分派（文件协议，v5 §13.3 最小版）**：`snapshot_root`→`touch cmd/snapshot.request`（root monitor 现有代理即吃）；`pmu_burst`→写 `cmd/burst.request`（JSON：`{"schema_version":"1","action_id":"<uuid4hex>","action":"perf_burst","parameters":{"duration_s":60,"cpus":[<事件 cpu>]},"expires_at":"<iso+300s>","reason_event_ids":["<id>"]}`——root-helper T4 消费）；其余动作本版只入账本（freeze_ring 已由 sdc_ring 自动做；enqueue_reproduction/hold_profile 留 M3 消费者，动作行即接口契约）
  - `replay(events_file, rules_path) -> list[decision]`：离线重放（M2 退出标准的核心工具）

- [ ] **Step 1: 失败测试**：

```python
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "telemetry"))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_controller as sc

RULES = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                     "configs", "sdc-excite-reproduce", "rules_m2.json")

def ev(t, sev="red", eid="e1", cpu=None):
    e = {"event_id": eid, "event_type": t, "severity": sev,
         "sdc-excite-reproduce_id": "c", "run_id": "r",
         "time": {"realtime_ns": 1}}
    if cpu is not None: e["location"] = {"logical_cpu": cpu}
    return e

def test_mismatch_green_to_red():
    m = sc.StateMachine(sc.load_rules(RULES))
    acts = m.feed(ev("sdc_mismatch"))
    assert m.state == "red"
    assert "snapshot_root" in acts and "enqueue_reproduction" in acts

def test_illegal_transition_ignored():
    m = sc.StateMachine(sc.load_rules(RULES))
    m.feed(ev("sdc_mismatch"))                       # → red
    m.feed(ev("collector_degraded", sev="yellow"))   # red 下 yellow 非法
    assert m.state == "red"

def test_replay_determinism(tmp_path):
    events = [ev("sdc_mismatch", eid="a"), ev("discrete_transition", "orange", "b"),
              ev("collector_degraded", "yellow", "c")]
    f = tmp_path / "e.jsonl"; f.write_text("\n".join(json.dumps(e) for e in events))
    d1 = sc.replay(str(f), RULES); d2 = sc.replay(str(f), RULES)
    assert d1 == d2                                  # 重放确定性（v5 §8.2）
    assert [x["to"] for x in d1 if not x["ignored"]] == ["red"]  # 后两条在 red 下非法

def test_controller_loop_writes_ledger_and_requests(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("discrete_transition", "orange", "x1", cpu=17)) + "\n")
    loop = sc.ControllerLoop(d, f"{d}/spool", RULES)
    dec = loop.poll_once()
    assert dec and dec[0]["rule"] == "discrete_assert" and dec[0]["to"] == "orange"
    led = [json.loads(l) for l in open(f"{d}/spool/controller_ledger.jsonl")]
    assert led[0]["actions"] == ["pmu_burst"]
    req = json.load(open(f"{d}/cmd/burst.request"))
    assert req["action"] == "perf_burst" and 17 in req["parameters"]["cpus"]
```

- [ ] **Step 2-4: 红→实现→绿**（uuid4hex=uuid.uuid4().hex；expires_at 满足即弃——请求文件带过期时间戳，helper 侧校验）。
- [ ] **Step 5: Commit** `tools: sdc-controller——五态规则状态机 + append-only 决策账本 + 重放确定性（v5 §8，M2）`

---

### Task 4: sdc-root-helper——allowlist 特权动作执行器

**Files:**
- Create: `tools/control/sdc_root_helper.py`
- Create: `scripts/sdc-excite-reproduce/systemd/sdc-root-helper.service.in` + install.sh 两行增补
- Test: `tools/control/tests/test_sdc_root_helper.py`

**Interfaces:**
- Consumes: `cmd/*.request`（snapshot.request 沿用现有 monitor 代理——**不重复实现**；本 helper 消费 `cmd/burst.request`、`cmd/cpu_hotplug.request`、`cmd/restore.request` 三类新请求）
- Produces:
  - `ALLOWED = {"perf_burst", "cpu_online", "cpu_offline", "restore"}`（枚举固定）；参数校验器 `validate(action, params) -> (ok, reason)`：perf_burst(duration_s ≤ 300, cpus ⊆ possible)；cpu_offline(cpus ⊆ possible − {0}，且**当前无 sdcshield 进程**——run 边界约束 v5 §7.4.4)；cpu_online(cpus ⊆ possible)；restore(无参数)
  - `execute(action, params, data_root) -> dict(result)`：执行 + 前后状态记录 `spool/root_helper_audit.jsonl`（ts/action/caller_pid/params/old/new/readback）；**shell 元字符防线**：params 序列化后含 `;|&$()\`"'` 等即拒绝（虽不拼 shell，纵深防御）
  - cpu_offline/online 前置快照：原 online 集存 `spool/pre_state_online.txt`（restore 动作读回）；执行后 `cat /sys/devices/system/cpu/online` 读回校验
  - perf_burst：`perf stat -x, -a --per-core -I 100 -e cycles,instructions -- sleep <duration_s>` 输出落 `events/<event 目录或 spool/bursts/<action_id>.csv`（由请求的 `output_dir` 参数指定，校验必须在数据根内——防路径逃逸）
  - `HelperLoop(data_root, cmd_dir).poll_once() -> list[result]`：扫三类 .request → 逐个 validate→execute→`mv x.request x.done.<ts>`（或 `.rejected.<ts>` + 拒绝原因入审计）
  - CLI：`python3 sdc_root_helper.py [--once] [--data-root DIR]`（root 服务跑守护 2s）

- [ ] **Step 1: 失败测试**（**全部 mock 注入——绝不真 offline/真 perf 对真机**：os.system/perf 子进程用 monkeypatch 桩）：

```python
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_root_helper as rh

def test_validate_hotplug_boundaries():
    ok, _ = rh.validate("cpu_offline", {"cpus": [4, 8]})
    bad, why = rh.validate("cpu_offline", {"cpus": [0]})          # CPU0 拒绝
    assert not bad and "CPU0" in why
    bad, _ = rh.validate("cpu_offline", {"cpus": [9999]})         # 超 possible
    assert not bad
    bad, _ = rh.validate("perf_burst", {"duration_s": 999, "cpus": [1]})
    assert not bad                                                 # >300 拒绝

def test_shell_metachar_rejected():
    bad, why = rh.validate("perf_burst", {"duration_s": 60, "cpus": [1],
                                          "output_dir": "/tmp/x; rm -rf /"})
    assert not bad and "元字符" in why

def test_offline_requires_no_sdcshield(monkeypatch):
    monkeypatch.setattr(rh, "_sdcshield_running", lambda: True)
    bad, why = rh.validate("cpu_offline", {"cpus": [4]})
    assert not bad and "run 边界" in why

def test_helper_loop_executes_and_audits(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool", "spool/bursts"): os.makedirs(f"{d}/{p}")
    executed = []
    monkeypatch.setattr(rh, "_run_perf_burst", lambda p, out: executed.append(p) or "perf-out")
    with open(f"{d}/cmd/burst.request", "w") as f:
        json.dump({"schema_version": "1", "action_id": "a1", "action": "perf_burst",
                   "parameters": {"duration_s": 5, "cpus": [1, 2],
                                  "output_dir": f"{d}/spool/bursts"}}, f)
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert res[0]["status"] == "done" and executed
    audit = [json.loads(l) for l in open(f"{d}/spool/root_helper_audit.jsonl")]
    assert audit[0]["action"] == "perf_burst"
    assert not os.path.exists(f"{d}/cmd/burst.request")           # 已 mv done
```

- [ ] **Step 2-4: 红→实现→绿**（`_sdcshield_running`/`_run_perf_burst`/`_write_cpu_online` 为注入点；真实实现 subprocess/ sysfs 写——测试全 mock）。
- [ ] **Step 5: Commit** `tools: sdc-root-helper——allowlist 特权动作（burst/hotplug/restore）+ nonce + 审计（v5 §13.2，M2）`

---

### Task 5: 联动与恢复——controller→helper 全链路 + restore

**Files:**
- Modify: `tools/control/sdc_controller.py`（动作分派扩展：RED 事件的 pmu_burst 已有；新增 `interlock` 类事件触发 `verify_only` 时写 `cmd/verify.request`——最小：本任务核心是**端到端演练脚本**）
- Create: `scripts/sdc-excite-reproduce/m2_drill.sh`（合成事件演练=M2 退出标准驱动器）
- Test: `tools/control/tests/test_m2_e2e.py`

**Interfaces:**
- Consumes: T1-T4 全部
- Produces: `m2_drill.sh`（向隔离数据根注入合成 fixture 事件序列：合成 mismatch / RAS 关键字 / collector 降级 / 离散断言 —— 四类事件 → 依次调用 eventd --once → ring --once → controller --once → root-helper --once（burst 用 mock 输出目录）→ 断言：状态机转换序列 == 预期、决策账本可重放（replay diff 空）、ring 已固化、burst 请求被执行且审计在案）；**全部在 --once 模式 + 隔离数据根跑，不部署不碰真实服务**

- [ ] **Step 1: 失败测试**（test_m2_e2e.py：构造隔离根 → 注入四类源文件行 → 依序跑四个 --once（subprocess）→ 断言上述全部 + `replay` 与账本一致）
- [ ] **Step 2-4: 红→实现→绿**（m2_drill.sh 封装同流程供人工/CI 跑；expected 转换序列写在脚本头部注释）
- [ ] **Step 5: Commit** `scripts: M2 端到端合成事件演练（四类事件→转换/固化/burst/审计/重放全断言）`

---

### Task 6: 部署三守护服务 + BPF canary 能力探测 + 收尾

**Files:**
- Create: `scripts/sdc-excite-reproduce/systemd/sdc-eventd.service.in`、`sdc-controller.service.in`（用户态，Restart=always）
- Modify: `install.sh`（三模板：root-helper + eventd + controller）、`NEW_BOARD_ONBOARDING.md`（M2 段）
- Create: `tools/telemetry/bpf_canary_probe.sh`（能力探测：`bpftrace --info` 可用且 `kprobe -l do_translation_fault` 可达 → 输出 `SPURIOUS_CANARY=bpf`；否则 `SPURIOUS_CANARY=dmesg`——**探测不落实现**，实现按结果决定：bpf 路径本任务只留接口位 `journal_watch.log` 已有 dmesg 路径继续工作，BPF 升级属 v5 §6.5 M2 项但依赖内核符号——**诚实降级**：探测结果进 capabilities.env，实现缓行记录于 output 文档）
- Ops: root 安装 + enable --now eventd/controller/root-helper（**不动战役四服务**）

- [ ] **Step 1: 单元文件**（eventd/controller：`ExecStart=/usr/bin/python3 @REPO@/tools/... --data-root @CAMPAIGN_DIR@`，User=sdc；root-helper：root）
- [ ] **Step 2: root 部署**（su_run.py：install.sh → daemon-reload → enable --now 三服务 → is-active ×3；战役/采集器服务不动）
- [ ] **Step 3: 真实冒烟**（≥3 轮后：`spool/events.jsonl` 有行、`controller_ledger.jsonl` 存在（真实事件少可为空文件+至少一轮 green 心跳行——controller 每轮无事件时写 `{"rule":"heartbeat",...}` 决策行）、root_helper_audit.jsonl 头存在；**注入一次真实演练**：`m2_drill.sh` 对真实数据根跑 --once 会污染真 events——**不**：演练永远只对隔离根，真实根只观察）
- [ ] **Step 4: BPF 探测** + 结果入 capabilities + output 文档记录降级决策
- [ ] **Step 5: Commit（单元+文档）+ push + PR（API）**

---

## 与 v5 M2 交付物的对照（自审）

| v5 M2 交付物/退出标准 | 落点 |
|---|---|
| GREEN-BLACK 状态机 | T3 |
| action API | T3 动作分派（文件协议）+ T4 helper 请求 schema（v5 §13.3 最小版——mTLS gRPC 属跨节点，本机文件协议即 v5 §5.5 首选接口） |
| root helper（含 cpu hotplug） | T4 |
| 事件 burst | T4 perf_burst + T3 触发 |
| 恢复机制 | T4 restore（online 集快照读回）；governor/IRQ 恢复=既有 monitor 代理已管 governor，cpuset 驱动自管 |
| 退出：合成 mismatch/RAS/BMC 失联/温度越限触发正确动作且可回放 | T5（四类合成事件；BMC 失联=collector_degraded 近似源+温度越限=monitor 联锁既有 PAUSE 行→eventd 的 interlock 源？**修正**：T1 增加 `alerts.log` 第五源——PAUSE/RESUME 行→`interlock_action` 事件，T3 safety_interlock 规则消费）→ T5 演练含此链路 |
| spurious BPF（M1 偏差移交） | T6 探测+诚实降级（dmesg 路径已在 M1 运行） |
| 120s ring（M1 移交） | T2 |

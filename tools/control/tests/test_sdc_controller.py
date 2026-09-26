import datetime, glob, json, os, subprocess, sys
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
    req = json.load(open(glob.glob(f"{d}/cmd/burst-*.request")[0]))
    assert req["action"] == "perf_burst" and 17 in req["parameters"]["cpus"]

# ---- T1 传入裁定（必修）：safety_interlock 的 when 带 severity=black ----
# interlock_action 事件也来自磁盘水位等 yellow ALERT 行（eventd interlock 源），
# 无 severity 约束的规则会把控制器误打入 BLACK——本回归锁定该裁定。

def test_interlock_requires_black_severity():
    m = sc.StateMachine(sc.load_rules(RULES))
    assert m.feed(ev("interlock_action", sev="yellow")) == []   # 磁盘水位 ALERT：不产生决策
    assert m.state == "green" and m.last_decision is None
    acts = m.feed(ev("interlock_action", sev="black", eid="e2"))
    assert m.state == "black" and acts == ["verify_only"]

def test_interlock_yellow_no_decision_black_ledgered(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("interlock_action", sev="yellow", eid="w1")) + "\n")
        f.write(json.dumps(ev("interlock_action", sev="black", eid="w2")) + "\n")
    loop = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False)
    dec = loop.poll_once()
    assert [x["event_id"] for x in dec] == ["w2"]        # yellow 行无决策行
    assert loop.machine.state == "black"
    assert dec[0]["actions"] == ["verify_only"]

# ---- heartbeat：守护模式每轮无事件写心跳行（T6 部署验收依赖）；--once 不写 ----

def test_heartbeat_daemon_writes_once_mode_does_not(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w"):
        pass                                           # 空事件流
    daemon = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=True)
    dec = daemon.poll_once()
    assert dec and dec[0]["rule"] == "heartbeat"
    assert dec[0]["from"] == dec[0]["to"] == "green"
    assert dec[0]["actions"] == [] and dec[0]["ignored"] is False
    once = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False)
    assert once.poll_once() == []                      # --once：无事件不写心跳
    led = [json.loads(l) for l in open(f"{d}/spool/controller_ledger.jsonl")]
    assert len(led) == 1                               # 账本未被心跳污染

# ---- 状态持久化：守护重启不丢 RED、不重复决策（Restart=always 语义）----

def test_state_persists_across_restart(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("sdc_mismatch", eid="r1")) + "\n")
    sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False).poll_once()
    loop2 = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=True)   # 模拟重启
    dec = loop2.poll_once()                    # 无新事件 → 心跳行证明状态延续
    assert dec[0]["rule"] == "heartbeat" and dec[0]["from"] == "red"
    led = [json.loads(l) for l in open(f"{d}/spool/controller_ledger.jsonl")]
    assert [x["rule"] for x in led] == ["exact_mismatch", "heartbeat"]

# ---- 行级兜底：坏 JSON 行隔离不炸守护（T1/T2 毒丸教训同类关法）----

def test_poison_line_quarantined(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write('{"broken json\n')
        f.write(json.dumps(ev("sdc_mismatch", eid="p1")) + "\n")
    loop = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False)
    dec = loop.poll_once()                      # 不得抛异常
    assert [x["rule"] for x in dec] == ["exact_mismatch"]
    assert loop.poll_once() == []               # offset 已推进，坏行不再重读
    inv = [json.loads(l) for l in open(f"{d}/spool/controller_invalid.jsonl")]
    assert len(inv) == 1 and "JSONDecodeError" in inv[0]["error"] and inv[0]["line"]

# ---- 动作分派：snapshot_root → touch cmd/snapshot.request（ras_keyword/red 链）----

def test_snapshot_root_touches_request(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("ras_keyword", sev="red", eid="k1")) + "\n")
    dec = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False).poll_once()
    assert dec[0]["rule"] == "ras_keyword" and dec[0]["to"] == "red"
    assert os.path.exists(f"{d}/cmd/snapshot.request")

# ---- ignored 决策行形状 + 账本（去 ts）== replay 规范形（M2 退出标准核心断言）----

def test_ledger_minus_ts_equals_replay(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    events = [ev("sdc_mismatch", eid="a"), ev("collector_degraded", "yellow", "b")]
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write("\n".join(json.dumps(e) for e in events) + "\n")
    dec = sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False).poll_once()
    assert len(dec) == 2
    assert dec[1]["ignored"] is True and dec[1]["actions"] == []
    assert dec[1]["from"] == "red" and dec[1]["to"] == "yellow"  # to=被拒目标（诚实记录）
    led = [json.loads(l) for l in open(f"{d}/spool/controller_ledger.jsonl")]
    stripped = [{k: v for k, v in x.items() if k != "ts"} for x in led]
    assert stripped == sc.replay(f"{d}/spool/events.jsonl", RULES)

# ---- CLI --once：一轮即退、幂等、不写 heartbeat + burst-*.request 形状 ----

def test_cli_once_idempotent_no_heartbeat(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("discrete_transition", "orange", "x9", cpu=3)) + "\n")
    here = os.path.dirname(os.path.abspath(__file__))
    cmd = [sys.executable, os.path.join(here, "..", "sdc_controller.py"),
           "--once", "--data-root", d, "--rules", RULES]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    led = [json.loads(l) for l in open(f"{d}/spool/controller_ledger.jsonl")]
    assert len(led) == 1 and led[0]["rule"] == "discrete_assert"
    req = json.load(open(glob.glob(f"{d}/cmd/burst-*.request")[0]))
    assert req["action"] == "perf_burst" and req["schema_version"] == "1"
    assert req["parameters"]["cpus"] == [3] and req["parameters"]["duration_s"] == 60
    assert len(req["action_id"]) == 32                 # uuid4().hex
    assert req["reason_event_ids"] == ["x9"]
    exp = datetime.datetime.fromisoformat(req["expires_at"])
    remain = (exp - datetime.datetime.now(exp.tzinfo)).total_seconds()
    assert 0 < remain <= 300                           # now+300s ISO
    r2 = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    assert r2.returncode == 0, r2.stderr
    led2 = [json.loads(l) for l in open(f"{d}/spool/controller_ledger.jsonl")]
    assert len(led2) == 1                              # 幂等：无重放决策、无 heartbeat

# ---- burst 多槽（T3 评审移交必修）：同轮多触发各占一文件，后写不覆盖先写 ----
# 单槽 burst.request 时代：同轮第二个 pmu_burst 覆盖第一个的请求文件——早触发
# cpu 的 burst 丢失（两决策都在账本，动作却只执行一次）。多槽 burst-<action_id>
# 各占一文件；root-helper（T4）glob 消费。

def test_burst_multi_slot_two_requests_survive(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd", "events"): os.makedirs(f"{d}/{p}")
    rules = tmp_path / "rules.json"
    rules.write_text(json.dumps({
        "schema_version": "1",
        "rules": [
            {"id": "burst_one", "when": {"event_type": "discrete_transition"},
             "transition": "orange", "actions": ["pmu_burst"]},
            {"id": "burst_two", "when": {"event_type": "ras_keyword", "severity": "red"},
             "transition": "red", "actions": ["pmu_burst"]},
        ],
        "transitions": {"green": {"orange": ["burst_one"], "red": ["burst_two"]},
                        "yellow": {}, "orange": {"red": ["burst_two"]},
                        "red": {}, "black": {}}}))
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("discrete_transition", "orange", "m1", cpu=7)) + "\n")
        f.write(json.dumps(ev("ras_keyword", "red", "m2", cpu=9)) + "\n")
    dec = sc.ControllerLoop(d, f"{d}/spool", str(rules), heartbeat=False).poll_once()
    assert [x["rule"] for x in dec] == ["burst_one", "burst_two"]   # 两次分派都合法
    files = sorted(glob.glob(f"{d}/cmd/burst-*.request"))
    assert len(files) == 2                             # 多槽：两个请求都存活
    reqs = [json.load(open(p)) for p in files]
    assert len({r["action_id"] for r in reqs}) == 2    # action_id 各异
    assert {r["parameters"]["cpus"][0] for r in reqs} == {7, 9}  # 各自 cpu 不丢

"""M2 端到端演练（Task 5）：四类合成事件 + 离散断言 → 四守护 --once 全链路 + 重放确定性。

链路（v5 §14.2 M2 退出标准驱动器；分阶段注入=events.jsonl 顺序——eventd 源序
固定为 ledger→discrete→journal→selfmon→interlock，五级链须逐源分阶段跑）：
  green →(① collector 降级 selfmon 丢样行)→ yellow
       →(② 离散断言 0x00→0x01)→ orange ［pmu_burst → helper --mock-exec 执行］
       →(③ 合成 mismatch ledger rc=1)→ red ［freeze_ring/snapshot_root/…；ring 固化］
       →(④ RAS UE 行，red 下非法：ignored=true 诚实入账——升级占优，无二次分派）
       →(⑤ 温度越限 ALERT PAUSE)→ black ［verify_only → cmd/verify.request］

断言口径（m2_drill.sh 同款，本文件为可执行规格）：
  A. 决策序列==上表（5 行，含 ④ ignored）+ 终态 black；
  B. 重放确定性（v5 §8.2）：replay(events.jsonl) == 账本去 ts，两次重放相等；
  C. ring 固化：3 个 red/black 事件各 events/<id>/ring_window/，>120s 陈旧行淘汰；
  D. burst 全链：请求 .done 终态化 + spool/bursts/<action_id>.csv + 审计 done 行；
  E. verify/snapshot 请求在案；静止复跑零新事件零决策（offset 幂等）。

安全边界（全程）：--once 模式 + pytest tmp_path 隔离根；helper --mock-exec
（伪 perf CSV，绝不跑真 perf、绝不执行真实特权动作）；绝不部署、绝不碰
真实服务/真实 cmd/真实 sysfs 写。
"""
import datetime, glob, json, os, re, subprocess, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "telemetry"))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
import sdc_controller as sc

HERE = os.path.dirname(os.path.abspath(__file__))
TOOLS = os.path.join(HERE, "..")
EVENTD = os.path.join(TOOLS, "..", "telemetry", "sdc_eventd.py")
RING = os.path.join(TOOLS, "..", "telemetry", "sdc_ring.py")
CONTROLLER = os.path.join(TOOLS, "sdc_controller.py")
HELPER = os.path.join(TOOLS, "sdc_root_helper.py")
RULES = os.path.join(HERE, "..", "..", "..",
                     "configs", "sdc-excite-reproduce", "rules_m2.json")
DRILL = os.path.join(HERE, "..", "..", "..",
                     "scripts", "sdc-excite-reproduce", "m2_drill.sh")

SELFMON_HEADER = ("ts,collector,samples_total,samples_dropped,period_s,"
                  "loop_duration_s,last_success_ts,rss_kb")

# 期望决策序列（注入顺序；④ RAS 在 red 下非法——ignored 诚实入账）
EXPECTED_SHAPE = [
    ("collector_degraded", "green", "yellow", False),
    ("discrete_assert", "yellow", "orange", False),
    ("exact_mismatch", "orange", "red", False),
    ("ras_keyword", "red", "red", True),
    ("safety_interlock", "red", "black", False),
]


def _now():
    return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")


def ev(t, sev="red", eid="e1"):
    return {"event_id": eid, "event_type": t, "severity": sev,
            "sdc-excite-reproduce_id": "m2e2e", "run_id": "r", "time": {"realtime_ns": 1}}


def _run_once(tool, root, *extra):
    r = subprocess.run([sys.executable, tool, "--once", "--data-root", root, *extra],
                       capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, f"{os.path.basename(tool)} 失败: {r.stderr}"
    return r.stdout


def run_pipeline(root):
    """构造隔离根 + 分五阶段注入合成源行；每阶段依序 eventd→ring→controller→
    helper(--mock-exec) 各 --once（子进程——与真实部署同入口）。"""
    for p in ("events", "monitor", "spool", "cmd"):
        os.makedirs(os.path.join(root, p))
    # ring 热窗种子：fresh 行入窗 + 一条 >120s 陈旧行（断言 C 的滚动淘汰口径）
    with open(os.path.join(root, "monitor", "monitor.csv"), "w") as f:
        f.write("ts,source,note\n")
        f.write(f"{_now()},m2_drill_marker,temperature_c=42.0\n")
        f.write("2020-01-01 00:00:00,stale_line_over_120s,must_not_freeze\n")
    with open(os.path.join(root, "monitor", "percore.csv"), "w") as f:
        f.write(f"{_now()},cpu7,m2_drill_percore_marker\n")
    stages = (
        ("monitor/collector_self.csv",
         [SELFMON_HEADER, f"{_now()},pmu,1200,3,2.0,2.500,,96000"]),   # 采集失败行
        ("monitor/discrete_events.log",
         [f"[{_now()}] TRANSITION cpu1_prochot: 0x00 → 0x01"]),
        ("events/ledger.csv",
         [f"{_now()},m2_e2e,rc=1,test=memcpy_rewr,seed=AES:ab"]),
        ("monitor/journal_watch.log",
         [f"[{_now()}] UE>0 mc0 ce=0 ue=2 告警"]),
        ("monitor/alerts.log",
         [f"[{_now()}] ALERT PAUSE: thermal CPU 96C >= 95C（M2 演练合成）"]),
    )
    for rel, lines in stages:
        with open(os.path.join(root, rel), "a") as f:
            f.write("\n".join(lines) + "\n")
        _run_once(EVENTD, root)
        _run_once(RING, root)
        _run_once(CONTROLLER, root, "--rules", RULES)
        _run_once(HELPER, root, "--mock-exec")


def _load_jsonl(path):
    return [json.loads(l) for l in open(path) if l.strip()]


# ---------------------------------------------------------------------------
# RED-1：controller verify_only 分派 → cmd/verify.request（brief：interlock 类
# 事件触发 verify_only 时写请求——M3 消费者，接口契约落盘即交付）

def test_verify_only_dispatch_writes_verify_request(tmp_path):
    d = str(tmp_path / "data")
    for p in ("spool", "cmd"):
        os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write(json.dumps(ev("interlock_action", sev="black", eid="w9")) + "\n")
    sc.ControllerLoop(d, f"{d}/spool", RULES, heartbeat=False).poll_once()
    v = json.load(open(f"{d}/cmd/verify.request"))
    assert v["schema_version"] == "1" and v["action"] == "verify_only"
    assert v["parameters"] == {} and v["reason_event_ids"] == ["w9"]
    assert re.fullmatch(r"[0-9a-f]{32}", v["action_id"])       # uuid4().hex


# ---------------------------------------------------------------------------
# RED-2/3：helper --mock-exec 旗标（仅测试——生产不设即真实执行）

def test_helper_cli_mock_exec_burst_writes_fake_csv(tmp_path):
    d = str(tmp_path / "data")
    os.makedirs(f"{d}/cmd")
    with open(f"{d}/cmd/burst-mock1.request", "w") as f:
        json.dump({"schema_version": "1", "action_id": "mock1", "action": "perf_burst",
                   "parameters": {"duration_s": 60, "cpus": []}}, f)
    r = subprocess.run([sys.executable, HELPER, "--once", "--data-root", d,
                        "--mock-exec"], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "MOCK-EXEC" in (r.stdout + r.stderr)                 # 醒目模式宣告
    assert "done" in r.stdout
    csvp = f"{d}/spool/bursts/mock1.csv"
    assert "mock" in open(csvp).read().lower()                  # 伪 CSV 落盘
    audit = _load_jsonl(f"{d}/spool/root_helper_audit.jsonl")
    assert audit[0]["status"] == "done" and "mock" in audit[0]["readback"]["perf_return"]
    assert glob.glob(f"{d}/cmd/burst-mock1.request.done.*")     # 请求终态化


def test_helper_cli_mock_exec_rejects_real_privileged_actions(tmp_path):
    """--mock-exec 下非 perf_burst 一律拒绝——测试旗标绝不执行真实特权动作。"""
    d = str(tmp_path / "data")
    os.makedirs(f"{d}/cmd")
    with open(f"{d}/cmd/cpu_hotplug.request", "w") as f:
        json.dump({"schema_version": "1", "action_id": "hot1", "action": "cpu_offline",
                   "parameters": {"cpus": [4]}}, f)
    r = subprocess.run([sys.executable, HELPER, "--once", "--data-root", d,
                        "--mock-exec"], capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    audit = _load_jsonl(f"{d}/spool/root_helper_audit.jsonl")
    assert audit[0]["status"] == "rejected" and "--mock-exec" in audit[0]["reason"]
    assert glob.glob(f"{d}/cmd/cpu_hotplug.request.rejected.*")
    assert not os.path.exists(f"{d}/spool/pre_state_online.txt")  # 零快照=零执行


# ---------------------------------------------------------------------------
# E2E 用例 1：全链五级 + 全动作断言（A/C/D/E）

def test_e2e_full_chain_five_levels_and_all_actions(tmp_path):
    root = str(tmp_path / "data")
    run_pipeline(root)

    # 事件流：五事件按注入顺序（分阶段 eventd 保证）
    events = _load_jsonl(f"{root}/spool/events.jsonl")
    assert [e["event_type"] for e in events] == [
        "collector_degraded", "discrete_transition", "sdc_mismatch",
        "ras_keyword", "interlock_action"]

    # A. 决策序列 + 终态
    led = _load_jsonl(f"{root}/spool/controller_ledger.jsonl")
    assert [(d["rule"], d["from"], d["to"], d["ignored"]) for d in led] == EXPECTED_SHAPE
    assert led[0]["actions"] == ["alert_only"]
    assert led[1]["actions"] == ["pmu_burst"]
    assert led[2]["actions"] == ["freeze_ring", "snapshot_root",
                                 "enqueue_reproduction", "hold_profile"]
    assert led[3]["actions"] == []                              # ignored 无分派
    assert led[4]["actions"] == ["verify_only"]
    assert json.load(open(f"{root}/spool/controller_state.json"))["state"] == "black"

    # E. verify/snapshot 请求在案（接口契约落盘）
    v = json.load(open(f"{root}/cmd/verify.request"))
    assert v["action"] == "verify_only" and v["reason_event_ids"] == [led[4]["event_id"]]
    assert os.path.exists(f"{root}/cmd/snapshot.request")       # exact_mismatch→snapshot_root

    # D. burst 全链：.done 终态 + 伪 CSV + 审计（helper 真执行——mock 输出）
    assert glob.glob(f"{root}/cmd/burst-*.request") == []       # 无未终态化请求
    done = glob.glob(f"{root}/cmd/burst-*.request.done.*")
    assert len(done) == 1
    req = json.load(open(done[0]))
    assert req["action"] == "perf_burst"
    assert req["parameters"] == {"duration_s": 60, "cpus": []}  # 离散事件无 location→全核语义
    assert req["reason_event_ids"] == [led[1]["event_id"]]
    csvp = f"{root}/spool/bursts/{req['action_id']}.csv"
    assert "mock" in open(csvp).read().lower()
    audit = _load_jsonl(f"{root}/spool/root_helper_audit.jsonl")
    assert len(audit) == 1 and audit[0]["action"] == "perf_burst"
    assert audit[0]["status"] == "done" and audit[0]["action_id"] == req["action_id"]
    assert "mock" in audit[0]["readback"]["perf_return"]

    # C. ring 固化：3 个 red/black 事件（mismatch/ras/interlock）；陈旧行淘汰
    frozen = set(json.load(open(f"{root}/spool/ring_frozen.json")))
    redblack = {e["event_id"] for e in events if e["severity"] in ("red", "black")}
    assert redblack and frozen == redblack and len(frozen) == 3
    for eid in redblack:
        mon = f"{root}/events/{eid}/ring_window/monitor.csv"
        body = open(mon).read()
        assert "m2_drill_marker" in body                       # 热窗种子在固化证据里
        assert "must_not_freeze" not in body                   # >120s 陈旧行已淘汰
        assert "m2_drill_percore_marker" in open(
            f"{root}/events/{eid}/ring_window/percore.csv").read()

    # E. 静止复跑：零新事件/零决策（offset 幂等，无重放决策）
    assert "0 new events" in _run_once(EVENTD, root)
    out = _run_once(CONTROLLER, root, "--rules", RULES)
    assert "0 decisions" in out and "state=black" in out


# ---------------------------------------------------------------------------
# E2E 用例 2：重放确定性（B，v5 §8.2 M2 退出标准）

def test_e2e_replay_determinism_matches_ledger(tmp_path):
    root = str(tmp_path / "data")
    run_pipeline(root)
    led = _load_jsonl(f"{root}/spool/controller_ledger.jsonl")
    assert len(led) == 5                                       # --once 无 heartbeat 行
    stripped = [{k: v for k, v in d.items() if k != "ts"} for d in led]
    evf = f"{root}/spool/events.jsonl"
    rep = sc.replay(evf, RULES)
    assert stripped == rep, f"账本去 ts != 重放:\n{stripped}\nvs\n{rep}"
    assert sc.replay(evf, RULES) == sc.replay(evf, RULES)      # 两次重放逐项相等
    # 重放含 ignored 行（④ RAS red→red 诚实入账）——非仅合法转换
    assert [d["ignored"] for d in rep] == [False, False, False, True, False]


# ---------------------------------------------------------------------------
# m2_drill.sh：M2 退出标准演练脚本（人工/CI 驱动器，逻辑同 run_pipeline）

def test_m2_drill_script_full_run(tmp_path):
    root = tmp_path / "drill-root"
    r = subprocess.run(["bash", DRILL, str(root)],
                       capture_output=True, text=True, timeout=300)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "M2 退出标准演练 PASS" in r.stdout
    assert json.load(open(root / "spool" / "controller_state.json"))["state"] == "black"
    # 守卫：无参拒绝（防误对真实根缺省跑）
    r2 = subprocess.run(["bash", DRILL], capture_output=True, text=True, timeout=30)
    assert r2.returncode != 0 and "隔离数据根" in (r2.stdout + r2.stderr)
    # 守卫：已用根拒绝（防误对真实战役根/重跑污染——真实根必含 spool/）
    r3 = subprocess.run(["bash", DRILL, str(root)],
                        capture_output=True, text=True, timeout=30)
    assert r3.returncode != 0 and "spool" in r3.stderr

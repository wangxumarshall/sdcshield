import json, os, subprocess, sys, time
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
    for e in evs: assert sdc_eventd.sdc_event.validate_event(e) == []
    out = [json.loads(l) for l in open(f"{d}/spool/events.jsonl")]
    assert len(out) == len(evs)          # 全部落盘
    assert loop.poll_once() == []        # 第二轮无新行

# ---- interlock 源（第五源 monitor/alerts.log，v5 §8.2 BLACK 输入链）----

def test_interlock_pause_kill_black_resume_green():
    pause = sdc_eventd.interlock_line_to_event(
        "[2026-09-26 10:00:00] ALERT PAUSE: thermal CPU 96C >= 95C", "s0")
    assert pause["event_type"] == "interlock_action" and pause["severity"] == "black"
    kill = sdc_eventd.interlock_line_to_event(
        "[2026-09-26 10:00:10] ALERT 热升级：96C（连续2个热采样）→ KILL 全部负载（绝对线100C/连续2采样）", "s0")
    assert kill["event_type"] == "interlock_action" and kill["severity"] == "black"
    resume = sdc_eventd.interlock_line_to_event(
        "[2026-09-26 10:00:20] ALERT RESUME: thermal 已恢复", "s0")
    assert resume["event_type"] == "interlock_action" and resume["severity"] == "green"

def test_interlock_other_alert_yellow_and_non_alert_none():
    warn = sdc_eventd.interlock_line_to_event(
        "[2026-09-26 10:00:00] ALERT 磁盘水位 86%（告警线 85%）", "s0")
    assert warn["event_type"] == "interlock_action" and warn["severity"] == "yellow"
    assert sdc_eventd.interlock_line_to_event("2026-09-26 10:00:00 普通行", "s0") is None

# ---- ledger 类型细分（M2 计划：mismatch/crash→sdc_mismatch/crash，drill→note）----

def test_ledger_crash_and_drill_note():
    crash = sdc_eventd.ledger_line_to_event(
        "2026-09-26 10:00:00,cold_c5,rc=139,test=memcpy_rewr,seed=AES:ab", "s0")
    assert crash["event_type"] == "crash" and crash["severity"] == "red"
    drill = sdc_eventd.ledger_line_to_event(
        "2026-09-23 22:19:15,injected-drill,rc=1,seed=,drill,验证取证写路径", "s0")
    assert drill["event_type"] == "note"
    assert sdc_eventd.ledger_line_to_event(",,,", "s0") is None      # 空行不产生事件

# ---- journal 源（UE/panic→red 否则 yellow）----

def test_journal_ras_keyword_severity():
    ue = sdc_eventd.journal_line_to_event(
        "[2026-09-26 10:00:00] UE>0 mc0 ce=0 ue=2 告警", "s0")
    assert ue["event_type"] == "ras_keyword" and ue["severity"] == "red"
    panic = sdc_eventd.journal_line_to_event(
        "[2026-09-26 10:00:01] RAS Kernel panic - not syncing: machine check", "s0")
    assert panic["severity"] == "red"
    spurious = sdc_eventd.journal_line_to_event(
        "[2026-09-26 10:00:02] SPURIOUS total=2 delta=1 source=dmesg（无 per-CPU 定位，M2 BPF 升级）", "s0")
    assert spurious["event_type"] == "ras_keyword" and spurious["severity"] == "yellow"
    assert sdc_eventd.journal_line_to_event("garbage line", "s0") is None

# ---- selfmon 源（samples_dropped 增量>0 → collector_degraded）----

def test_selfmon_degraded_on_dropped_increment():
    assert sdc_eventd.selfmon_line_to_event(
        "ts,collector,samples_total,samples_dropped,period_s,loop_duration_s,last_success_ts,rss_kb",
        "s0") is None                                        # 表头不是事件
    fail = sdc_eventd.selfmon_line_to_event(
        "2026-09-26 10:00:00,pmu,1200,3,2.0,2.500,,96000", "s0")   # 采集失败行（last_success 空）
    assert fail["event_type"] == "collector_degraded" and fail["severity"] == "yellow"
    ok = sdc_eventd.selfmon_line_to_event(
        "2026-09-26 10:00:02,pmu,1201,3,2.0,1.900,2026-09-26 10:00:02,96000", "s0")
    assert ok is None                                        # 累计值未增、本周期成功
    inc = sdc_eventd.selfmon_line_to_event(
        "2026-09-26 10:00:04,pmu,1202,5,2.0,1.800,2026-09-26 10:00:04,96000",
        "s0", prev_dropped=3)                                # 增量 3→5（跨行口径）
    assert inc["event_type"] == "collector_degraded"

# ---- ts 解析失败：当前时刻 + mapping_error_ns 标记（不伪造 0）----

def test_ts_parse_failure_uses_now():
    ev = sdc_eventd.discrete_line_to_event(
        "[not-a-timestamp] TRANSITION cpu1_prochot: 0x00 → 0x01", "s0")
    assert ev is not None
    assert abs(ev["time"]["realtime_ns"] - time.time_ns()) < 60 * 10**9
    assert ev["time"]["mapping_error_ns"] == -1       # 哨兵：解析失败，误差无界

# ---- dedup 持久化：offset 丢失后同内容行不重复入流 ----

def test_dedup_persists_across_offset_loss(tmp_path):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/discrete_events.log", "w") as f:
        f.write("[2026-09-26 10:00:00] TRANSITION cpu1_prochot: 0x00 → 0x01\n")
    loop1 = sdc_eventd.EventdLoop(d, f"{d}/spool")
    assert len(loop1.poll_once()) == 1
    os.remove(f"{d}/spool/tail_offsets.json")         # 模拟 offset 丢失（重启回退）
    loop2 = sdc_eventd.EventdLoop(d, f"{d}/spool")
    assert loop2.poll_once() == []                    # dedup.json 拦截重放
    keys = json.load(open(f"{d}/spool/dedup.json"))
    assert keys and len(keys[0]) == 16

# ---- schema 校验失败：不入 events.jsonl，诚实隔离到 eventd_invalid.jsonl ----

def test_invalid_event_quarantined(tmp_path, monkeypatch):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/discrete_events.log", "w") as f:
        f.write("[2026-09-26 10:00:00] TRANSITION cpu1_prochot: 0x00 → 0x01\n")
    def bad(line, sid):
        return {"schema_version": "1.0", "event_id": "ev-bad", "event_type": "bogus_type",
                "severity": "ultraviolet", "sdc-excite-reproduce_id": sid, "run_id": "r",
                "time": None}
    monkeypatch.setattr(sdc_eventd, "discrete_line_to_event", bad)
    loop = sdc_eventd.EventdLoop(d, f"{d}/spool")
    assert loop.poll_once() == []
    assert not os.path.exists(f"{d}/spool/events.jsonl")
    inv = [json.loads(l) for l in open(f"{d}/spool/eventd_invalid.jsonl")]
    assert len(inv) == 1 and inv[0]["errors"] and inv[0]["line"]

# ---- CLI --once：一个完整轮次后退出 ----

def test_cli_once_single_round(tmp_path):
    d = make_root(str(tmp_path))
    os.makedirs(f"{d}/events")
    with open(f"{d}/events/ledger.csv", "w") as f:
        f.write("2026-09-26 10:00:00,cold_c5,rc=137,test=zstd19,seed=AES:ab,x\n")
    here = os.path.dirname(os.path.abspath(__file__))
    r = subprocess.run([sys.executable, os.path.join(here, "..", "sdc_eventd.py"),
                        "--once", "--data-root", d],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    out = [json.loads(l) for l in open(f"{d}/spool/events.jsonl")]
    assert len(out) == 1 and out[0]["event_type"] == "sdc_mismatch"
    r2 = subprocess.run([sys.executable, os.path.join(here, "..", "sdc_eventd.py"),
                         "--once", "--data-root", d],
                        capture_output=True, text=True, timeout=60)
    assert r2.returncode == 0 and r2.stdout.strip().endswith("0 new events")

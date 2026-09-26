import os, sys, time, datetime
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_ring

import json, subprocess                    # 补充用例所需（brief 用例与其头保持逐字）

def ts(offset_s):
    return (datetime.datetime.now() - datetime.timedelta(seconds=offset_s)).strftime("%F %T")

def make_root(tmp):
    """隔离数据根：monitor/ + spool/ + events/（绝不触碰真实数据根）。"""
    d = os.path.join(tmp, "data")
    for p in ("monitor", "spool", "events"):
        os.makedirs(f"{d}/{p}")
    return d

def write_event(d, ev):
    with open(f"{d}/spool/events.jsonl", "a") as f:
        f.write(json.dumps(ev) + "\n")

# ---- brief 用例（逐字）----

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

# ---- 补充：时间淘汰边界（119s 截断余量内保窗 / 121s 必淘汰——>120s 严格口径）----

def test_ring_boundary_119_kept_121_evicted():
    r = sdc_ring.RingWindow(max_seconds=120)
    r.push("monitor", f"{ts(121)},old")     # 121+亚秒 > 120 → 淘汰
    kept = f"{ts(119)},edge"                # 119+亚秒 < 120 → 保窗
    r.push("monitor", kept)
    assert r.snapshot()["monitor"] == [kept]

# ---- 补充：ts 解析失败保窗不淘汰（诚实保留——与 T1 同哲学）----

def test_ring_unparseable_ts_kept():
    r = sdc_ring.RingWindow(max_seconds=120)
    r.push("monitor", "ts,c1,cpu1_temp_c")      # CSV 表头：保窗（固化文件里列名可见）
    r.push("monitor", "garbage line no comma")  # 残行：保窗（坏数据要被看见）
    r.push("monitor", f"{ts(121)},expired")     # 唯一淘汰的是可解析且过期的行
    assert r.snapshot()["monitor"] == ["ts,c1,cpu1_temp_c", "garbage line no comma"]

# ---- 补充：freeze 注释行格式 + 窗口空源不写文件 ----

def test_freeze_comment_format_and_nonempty_sources(tmp_path):
    r = sdc_ring.RingWindow(max_seconds=120)
    fresh = f"{ts(5)},45"
    r.push("monitor", "ts,c1")
    r.push("monitor", fresh)
    out = os.path.join(str(tmp_path), "events", "x1")
    written = sdc_ring.freeze(r, out, "x1")
    assert written == [os.path.join(out, "ring_window", "monitor.csv")]
    assert not os.path.exists(os.path.join(out, "ring_window", "percore.csv"))  # 空源不写
    lines = open(written[0]).read().splitlines()
    assert lines[0].startswith("# frozen at 2") and lines[0].endswith(
        " for x1 (ring [-60s,+60s] 口径, 行数=2)")
    assert lines[1:] == ["ts,c1", fresh]        # 注释行之后是窗口原文（含表头）
    assert sdc_ring.freeze(sdc_ring.RingWindow(), out, "none") == []  # 空窗不伪造证据文件

# ---- 补充：black 也固化；无 event_id → spool/frozen/<ts>-<hash>/ 回退 ----

def test_ring_black_and_no_event_id_fallback(tmp_path):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/monitor.csv", "w") as f:
        f.write("ts,c1\n" + f"{ts(3)},7\n")
    with open(f"{d}/monitor/percore.csv", "w") as f:
        f.write("ts,util_cpu0_pct\n" + f"{ts(3)},99\n")
    write_event(d, {"event_id": "b1", "severity": "black"})   # 联锁 PAUSE/KILL 级
    write_event(d, {"severity": "red"})                       # 无 event_id
    r = sdc_ring.RingLoop(d, f"{d}/spool").poll_once()
    assert r["frozen"][0] == "b1" and len(r["frozen"]) == 2
    fallback = r["frozen"][1]
    assert len(fallback.split("-")) == 3       # <yyyymmdd>-<hhmmss>-<hash12>
    b1 = open(f"{d}/events/b1/ring_window/monitor.csv").read()
    pc = open(f"{d}/events/b1/ring_window/percore.csv").read()
    assert ",7" in b1 and ",99" not in b1      # 各源写各自文件
    assert "util_cpu0_pct" in pc and ",99" in pc
    fb = open(os.path.join(f"{d}/spool", "frozen", fallback,
                           "ring_window", "monitor.csv")).read()
    assert f" for {fallback} " in fb.splitlines()[0]          # 回退目录注释行如实署名

# ---- 补充：消费 offset 持久化 ring_offset.json——重启不重复固化 ----

def test_ring_offset_persisted_no_refreeze(tmp_path):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/monitor.csv", "w") as f:
        f.write("ts,c1\n" + f"{ts(4)},44\n")
    write_event(d, {"event_id": "e1", "severity": "red"})
    loop1 = sdc_ring.RingLoop(d, f"{d}/spool")
    assert loop1.poll_once()["frozen"] == ["e1"]
    before = open(f"{d}/events/e1/ring_window/monitor.csv").read()
    loop2 = sdc_ring.RingLoop(d, f"{d}/spool")    # 模拟重启（新进程对象、同 spool）
    assert loop2.poll_once() == {"frozen": []}    # offset 已持久化，不重复固化
    assert open(f"{d}/events/e1/ring_window/monitor.csv").read() == before
    assert os.path.exists(f"{d}/spool/ring_offset.json")

# ---- 补充：坏 JSONL 行隔离不炸守护 + offset 推进 ----

def test_ring_bad_jsonl_quarantined(tmp_path):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/monitor.csv", "w") as f:
        f.write("ts,c1\n" + f"{ts(2)},22\n")
    with open(f"{d}/spool/events.jsonl", "w") as f:
        f.write("{not json\n")
        f.write(json.dumps({"event_id": "ok1", "severity": "red"}) + "\n")
    loop = sdc_ring.RingLoop(d, f"{d}/spool")
    assert loop.poll_once()["frozen"] == ["ok1"]  # 坏行不炸，好行照常固化
    inv = [json.loads(l) for l in open(f"{d}/spool/ring_invalid.jsonl")]
    assert len(inv) == 1 and "JSONDecodeError" in inv[0]["error"]
    assert inv[0]["line"] == "{not json"
    assert loop.poll_once() == {"frozen": []}     # 坏行 offset 已推进，不再重读

# ---- 补充：CLI --once 一个完整轮次即退 ----

def test_ring_cli_once(tmp_path):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/monitor.csv", "w") as f:
        f.write("ts,c1\n" + f"{ts(2)},11\n")
    write_event(d, {"event_id": "c1", "event_type": "sdc_mismatch",
                    "severity": "red", "time": {"realtime_ns": 1}})
    here = os.path.dirname(os.path.abspath(__file__))
    cmd = [sys.executable, os.path.join(here, "..", "sdc_ring.py"),
           "--once", "--data-root", d]
    r = subprocess.run(cmd, capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "c1" in r.stdout
    assert os.path.exists(f"{d}/events/c1/ring_window/monitor.csv")
    r2 = subprocess.run(cmd, capture_output=True, text=True, timeout=60)  # 跨进程幂等
    assert r2.returncode == 0 and "frozen 0" in r2.stdout

# ---- 补充：重启暖窗——大文件只回放尾 1MB（历史行过期即弃，读内存有界）----

def test_ring_warmup_big_file_tail_only(tmp_path):
    d = make_root(str(tmp_path))
    with open(f"{d}/monitor/monitor.csv", "w") as f:
        f.write("ts,c1\n")
        for _ in range(700):                     # ~1.4MB 远古行（2020 年，均过期）
            f.write("2020-01-01 00:00:00," + "x" * 2000 + "\n")
        fresh = f"{ts(3)},fresh"
        f.write(fresh + "\n")
    loop = sdc_ring.RingLoop(d, f"{d}/spool")
    loop.poll_once()
    # 表头在 1MB 回放窗外不入窗；历史行入窗即过期淘汰；只剩新鲜尾行
    assert loop.window.snapshot() == {"monitor": [fresh]}

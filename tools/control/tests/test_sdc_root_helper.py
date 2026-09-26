import glob, json, os, subprocess, sys
from datetime import datetime, timedelta, timezone
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_root_helper as rh

# 注入纪律（M1b T5 教训）：战役运行中——_run_perf_burst/_write_cpu_online/_read_cpu_attr/
# _sdcshield_running 全部 monkeypatch 桩，绝不真 offline/真 perf/真写 sysfs 对真机。
# validate 读 /sys/devices/system/cpu/possible 为只读真读（本机 0-127）。

def _burst_req(aid, params, **extra):
    req = {"schema_version": "1", "action_id": aid, "action": "perf_burst",
           "parameters": params}
    req.update(extra)
    return req

def _write_request(cmd_dir, name, req):
    with open(os.path.join(cmd_dir, name), "w") as f:
        json.dump(req, f)

def _render_cpu_set(s):
    """set → 内核 cpu_list 形（'0-3,5'）——桩 _read_cpu_attr 的 online 渲染用。"""
    out, start, prev = [], None, None
    for c in sorted(s):
        if start is None:
            start = prev = c
        elif c == prev + 1:
            prev = c
        else:
            out.append(f"{start}-{prev}" if prev > start else str(start))
            start = prev = c
    if start is not None:
        out.append(f"{start}-{prev}" if prev > start else str(start))
    return ",".join(out)

def _fake_sysfs(monkeypatch, online_raw, possible_raw="0-127"):
    """sysfs 桩：possible 固定；online 可变且经 _write_cpu_online 桩联动。"""
    st = {"online": online_raw}
    writes = []
    monkeypatch.setattr(rh, "_read_cpu_attr",
                        lambda attr: possible_raw if attr == "possible" else st["online"])
    def fake_write(cpu, up):
        writes.append((cpu, up))
        s = rh._parse_cpu_list(st["online"])
        (s.add if up else s.discard)(cpu)
        st["online"] = _render_cpu_set(s)
    monkeypatch.setattr(rh, "_write_cpu_online", fake_write)
    return st, writes

# ---- brief 用例 1：hotplug 边界（CPU0 拒/超 possible/时长上限）----
# 顺序锁定：cpus 边界检查在 sdcshield 检查之前——[0] 必须报 CPU0（战役运行中
# 真机 _sdcshield_running()=True，若顺序颠倒会误报 run 边界）。

def test_validate_hotplug_boundaries():
    ok, _ = rh.validate("cpu_offline", {"cpus": [4, 8]})
    bad, why = rh.validate("cpu_offline", {"cpus": [0]})          # CPU0 拒绝
    assert not bad and "CPU0" in why
    bad, _ = rh.validate("cpu_offline", {"cpus": [9999]})         # 超 possible
    assert not bad
    bad, _ = rh.validate("perf_burst", {"duration_s": 999, "cpus": [1]})
    assert not bad                                                 # >300 拒绝

# ---- brief 用例 2：shell 元字符防线（虽不拼 shell，纵深防御）----

def test_shell_metachar_rejected():
    bad, why = rh.validate("perf_burst", {"duration_s": 60, "cpus": [1],
                                          "output_dir": "/tmp/x; rm -rf /"})
    assert not bad and "元字符" in why

# ---- brief 用例 3：cpu_offline 的 run 边界（v5 §7.4.4——无 sdcshield 进程）----

def test_offline_requires_no_sdcshield(monkeypatch):
    monkeypatch.setattr(rh, "_sdcshield_running", lambda: True)
    bad, why = rh.validate("cpu_offline", {"cpus": [4]})
    assert not bad and "run 边界" in why

# ---- brief 用例 4：HelperLoop 执行 + 审计 + mv done（文件名随多槽裁定）----

def test_helper_loop_executes_and_audits(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool", "spool/bursts"): os.makedirs(f"{d}/{p}")
    executed = []
    monkeypatch.setattr(rh, "_run_perf_burst", lambda p, out: executed.append(p) or "perf-out")
    _write_request(f"{d}/cmd", "burst-a1.request",
                   _burst_req("a1", {"duration_s": 5, "cpus": [1, 2],
                                     "output_dir": f"{d}/spool/bursts"}))
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert res[0]["status"] == "done" and executed
    audit = [json.loads(l) for l in open(f"{d}/spool/root_helper_audit.jsonl")]
    assert audit[0]["action"] == "perf_burst"
    assert not os.path.exists(f"{d}/cmd/burst-a1.request")        # 已 mv done

# ---- 过期请求（传入裁定）：mv .expired.<ts> + 审计一行，不执行 ----

def test_expired_request_not_executed(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool"): os.makedirs(f"{d}/{p}")
    executed = []
    monkeypatch.setattr(rh, "_run_perf_burst", lambda p, out: executed.append(1) or "x")
    _write_request(f"{d}/cmd", "burst-old.request",
                   _burst_req("old1", {"duration_s": 5, "cpus": [1]},
                              expires_at=(datetime.now(timezone.utc)
                                          - timedelta(seconds=1)).isoformat()))
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert res[0]["status"] == "expired" and not executed
    assert glob.glob(f"{d}/cmd/burst-old.request.expired.*")
    audit = [json.loads(l) for l in open(f"{d}/spool/root_helper_audit.jsonl")]
    assert audit[0]["status"] == "expired" and "expires_at" in audit[0]["reason"]

# ---- 多槽 glob（T3 评审移交）：同轮多个 burst 请求都被执行，先写不被覆盖 ----

def test_multi_slot_burst_glob(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool", "spool/bursts"): os.makedirs(f"{d}/{p}")
    executed = []
    monkeypatch.setattr(rh, "_run_perf_burst",
                        lambda p, out: executed.append(os.path.basename(out)))
    for aid, cpu in (("a1", 1), ("a2", 42)):
        _write_request(f"{d}/cmd", f"burst-{aid}.request",
                       _burst_req(aid, {"duration_s": 5, "cpus": [cpu]}))
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert [r["status"] for r in res] == ["done", "done"]
    assert sorted(executed) == ["a1.csv", "a2.csv"]          # 各自 csv 互不覆盖
    assert not glob.glob(f"{d}/cmd/burst-*.request")         # 全部终态化

# ---- nonce/幂等（v5 §13.3）：重放同一 action_id 不得重复执行 ----

def test_nonce_rejects_replayed_action_id(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool", "spool/bursts"): os.makedirs(f"{d}/{p}")
    monkeypatch.setattr(rh, "_run_perf_burst", lambda p, out: "x")
    def submit():
        _write_request(f"{d}/cmd", "burst-a1.request",
                       _burst_req("a1", {"duration_s": 5, "cpus": [1]}))
    submit()
    loop = rh.HelperLoop(d, f"{d}/cmd")
    assert loop.poll_once()[0]["status"] == "done"
    submit()                                                  # 重放同一 action_id
    res = loop.poll_once()
    assert res[0]["status"] == "rejected" and "重复" in res[0]["reason"]
    submit()                                                  # 新实例（重启模拟）
    assert rh.HelperLoop(d, f"{d}/cmd").poll_once()[0]["status"] == "rejected"

# ---- 路径逃逸：output_dir 必须在数据根内 ----

def test_output_dir_escape_rejected(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool"): os.makedirs(f"{d}/{p}")
    executed = []
    monkeypatch.setattr(rh, "_run_perf_burst", lambda p, out: executed.append(1) or "x")
    _write_request(f"{d}/cmd", "burst-esc.request",
                   _burst_req("esc1", {"duration_s": 5, "cpus": [1],
                                       "output_dir": "/etc"}))
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert res[0]["status"] == "rejected" and "数据根" in res[0]["reason"]
    assert not executed

# ---- 通道一致性：文件名即通道——burst 通道塞不进 cpu_offline ----

def test_channel_action_mismatch_rejected(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool"): os.makedirs(f"{d}/{p}")
    st, writes = _fake_sysfs(monkeypatch, "0-127")            # 写路径全桩
    _write_request(f"{d}/cmd", "burst-spooky.request",
                   {"schema_version": "1", "action_id": "spooky", "action": "cpu_offline",
                    "parameters": {"cpus": [4]}})
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert res[0]["status"] == "rejected" and "通道" in res[0]["reason"]
    assert not writes and st["online"] == "0-127"             # 零执行零改写

# ---- execute：cpu_offline 前置快照 + 读回校验 + 审计 old/new/readback ----

def test_cpu_offline_execute_snapshots_and_verifies(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool"): os.makedirs(f"{d}/{p}")
    st, writes = _fake_sysfs(monkeypatch, "0-127")
    monkeypatch.setattr(rh, "_sdcshield_running", lambda: False)  # 战役运行中必桩
    res = rh.execute("cpu_offline", {"cpus": [4, 8]}, d, action_id="h1")
    assert res["status"] == "done"
    assert writes == [(4, False), (8, False)]                     # 排序去重执行
    assert open(f"{d}/spool/pre_state_online.txt").read().strip() == "0-127"
    audit = [json.loads(l) for l in open(f"{d}/spool/root_helper_audit.jsonl")]
    assert audit[0]["old"] == "0-127"
    assert audit[0]["new"] == audit[0]["readback"] == "0-3,5-7,9-127"

# ---- restore：读回前置快照，补齐至原 online 集 ----

def test_restore_via_helper_loop(tmp_path, monkeypatch):
    d = str(tmp_path / "data")
    for p in ("cmd", "spool"): os.makedirs(f"{d}/{p}")
    with open(f"{d}/spool/pre_state_online.txt", "w") as f:
        f.write("0-127\n")
    st, writes = _fake_sysfs(monkeypatch, "0-3")                  # 现状：仅 0-3 在线
    _write_request(f"{d}/cmd", "restore.request",
                   {"schema_version": "1", "action_id": "r1", "action": "restore",
                    "parameters": {}})
    res = rh.HelperLoop(d, f"{d}/cmd").poll_once()
    assert res[0]["status"] == "done"
    assert st["online"] == "0-127"                                # 读回经桩：全集恢复
    assert writes == [(c, True) for c in range(4, 128)]           # 只补不下线
    assert not os.path.exists(f"{d}/cmd/restore.request")

# ---- CLI --once：部署入口对元字符请求拒绝且零执行（真子进程、无 monkeypatch，
#      靠 metachar→validate 双防线证明不触发真 perf）----

def test_cli_once_rejects_metachar_without_executing(tmp_path):
    d = str(tmp_path / "data")
    os.makedirs(f"{d}/cmd")
    _write_request(f"{d}/cmd", "burst-evil.request",
                   _burst_req("evil1", {"duration_s": 5, "cpus": [1],
                                        "output_dir": "/tmp/x; rm -rf /"}))
    here = os.path.dirname(os.path.abspath(__file__))
    r = subprocess.run([sys.executable, os.path.join(here, "..", "sdc_root_helper.py"),
                        "--once", "--data-root", d],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stderr
    assert "1 requests" in r.stdout and "rejected" in r.stdout
    audit = [json.loads(l) for l in open(f"{d}/spool/root_helper_audit.jsonl")]
    assert audit[0]["status"] == "rejected" and "元字符" in audit[0]["reason"]
    assert glob.glob(f"{d}/cmd/burst-evil.request.rejected.*")
    assert not glob.glob(f"{d}/cmd/*.request")                    # 请求已终态化

"""sdc_axes 单元测试（M3 Task 2）——全 mock 文件协议，绝不真 hotplug/真 governor/真负载。

红线（战役+9 服务运行中）：本文件不得触发任何真实特权动作/真实负载——
  - governor/hotplug/restore 全走 tmp_path 假数据根 + FakeAxes（_wait_request
    打桩：按脚本把请求文件 rename 成终态名——与 monitor/helper 真实终态化同
    语义（mv/os.replace 保 inode），再委托真实等待逻辑——协议扫描与 inode
    归属判据真实走通，但 monitor/helper 服务根本不在这个目录上运行）；
  - 负载整形 _aggressor_burst 全 mock 计数；默认发射绑定经 FakeProfileRunner
    录制（绝不 spawn sdcshield）；
  - sdc_root_helper._sdcshield_running 打桩控制 run 边界分支（v5 §7.4.4）：
    False 测目标路径；True 测战役运行中的拒绝语义（本机实测：stress-ng 活跃
    时真实探针即 True——打桩 True 走真实 validate 分支，不依赖机器瞬态）。
"""
import json, os, sys, threading, time
from datetime import datetime

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "control"))
import sdc_axes as sa
import sdc_root_helper as srh

CAPS_FULL = {"NPROC": "128", "GOVERNOR_WRITABLE": "yes", "HAS_CPUFREQ": "yes",
             "CPU_ONLINE_WRITABLE": "yes", "PMU_CORE_COUNTERS": "12"}
CAPS_NO_GOV = dict(CAPS_FULL, GOVERNOR_WRITABLE="no")
CAPS_NO_HOTPLUG = dict(CAPS_FULL, CPU_ONLINE_WRITABLE="no")

GOV_SPEC = {"axis": "governor_experiment", "from": "performance",
            "to": "powersave", "hold_s": 60}
LOAD_SPEC = {"axis": "load_shaping", "pattern": "burst", "duty": 0.5,
             "period_s": 20, "duration_s": 60}
PLUG_SPEC = {"axis": "cpu_hotplug", "op": "offline", "cpus": [96, 97],
             "then": "online", "hold_s": 30}


class FakeAxes(sa.AxesExecutor):
    """_wait_request 打桩：每次调用读当前请求文件原文入 capture，再按脚本
    模拟 monitor/helper 异步终态化——**rename 请求文件为终态名**（与真实服务
    同语义：monitor mv / helper os.replace——inode 保持，_wait_request 的
    inode 归属判据因此走真实路径），然后委托真实 _wait_request 扫描返回。
    脚本元素："done"/"rejected"/"expired"/"timeout" 或 ("delay", 秒, 状态)。
    终态命名保真：JSON 请求（helper 协议）→ <prefix>.request.<状态>.<ts>；
    原始字符串请求（monitor governor 代理）→ <prefix>.<状态>.<ts>。"""

    def __init__(self, data_root, capabilities, script=(), capture=None):
        super().__init__(data_root, capabilities)
        self.script, self.capture = list(script), (capture if capture is not None else [])

    def _wait_request(self, cmd_dir, prefix, action_id, timeout):
        stem = f"{prefix}-{action_id}" if action_id else prefix
        req = os.path.join(cmd_dir, stem + ".request")
        raw = None
        if os.path.exists(req):
            with open(req, encoding="utf-8") as f:
                raw = f.read()
        self.capture.append((prefix, raw))
        act = self.script.pop(0) if self.script else "done"
        if isinstance(act, tuple):                     # ("delay", 秒, 状态)
            time.sleep(act[0])
            act = act[2]
        if act == "timeout":                           # 无终态出现——直接判超时
            return "timeout", None                     # （真实 120s 超时路径由
                                                      # test_wait_request_stale_
                                                      # exclusion 以 0.3s 短超时覆盖）
        is_json = False
        if raw:
            try:
                json.loads(raw)
                is_json = True
            except ValueError:
                pass
        mid = ".request." if is_json else "."
        term = os.path.join(cmd_dir, f"{prefix}{mid}{act}.{int(time.time())}")
        os.replace(req, term)                          # 终态化 = rename（保 inode）
        return sa.AxesExecutor._wait_request(self, cmd_dir, prefix, action_id, timeout)

    def _hold(self, seconds):
        self.capture.append(("hold", seconds))


def _last_audit(root):
    with open(os.path.join(root, "spool", "axes_audit.jsonl"), encoding="utf-8") as f:
        return json.loads(f.read().strip().splitlines()[-1])


# ---------------------------------------------------------------------------
# _make_loadplan 纯函数（brief：burst 占空比表/边界 duty=1.0/0.05）

def test_make_loadplan_burst_table():
    plan = sa._make_loadplan("burst", 0.5, 20, 300)
    assert len(plan) == 15                            # 300/20 整除
    assert all(s == {"t_on_s": 10.0, "t_off_s": 10.0} for s in plan)
    assert sum(s["t_on_s"] + s["t_off_s"] for s in plan) == 300


def test_make_loadplan_edges():
    full = sa._make_loadplan("burst", 1.0, 20, 60)    # duty=1.0：恒满载
    assert all(s["t_off_s"] == 0 and s["t_on_s"] == 20 for s in full) and len(full) == 3
    low = sa._make_loadplan("burst", 0.05, 20, 40)    # duty=0.05 边界
    assert all(s == {"t_on_s": 1.0, "t_off_s": 19.0} for s in low) and len(low) == 2
    rem = sa._make_loadplan("burst", 0.5, 20, 310)    # 余量 10s 按占空比成段
    assert len(rem) == 16 and rem[-1] == {"t_on_s": 5.0, "t_off_s": 5.0}
    sub = sa._make_loadplan("burst", 0.5, 20, 8)      # duration < period：单段
    assert sub == [{"t_on_s": 4.0, "t_off_s": 4.0}]


def test_make_loadplan_rejects_bad_specs():
    with pytest.raises(ValueError, match="pattern"):
        sa._make_loadplan("square", 0.5, 20, 60)      # 未知 pattern
    for duty in (0, 1.5, "half"):
        with pytest.raises(ValueError, match="duty"):
            sa._make_loadplan("burst", duty, 20, 60)
    with pytest.raises(ValueError, match="period_s"):
        sa._make_loadplan("burst", 0.5, 0, 60)
    with pytest.raises(ValueError, match="duration_s"):
        sa._make_loadplan("burst", 0.5, 20, 0)


# ---------------------------------------------------------------------------
# load_shaping：编排调用序列（_aggressor_burst 注入点 mock 计数）

def test_load_shaping_orchestration_mock(tmp_path):
    ex = sa.AxesExecutor(str(tmp_path), CAPS_FULL)
    calls = []
    def burst(t_on, t_off):
        calls.append((t_on, t_off))
        return 0
    ex._aggressor_burst = burst
    res = ex.execute(LOAD_SPEC)
    assert calls == [(10.0, 10.0)] * 3                # 计划表逐段驱动
    assert len(res) == 1 and res[0]["status"] == "done"
    assert res[0]["plan"] == [{"t_on_s": 10.0, "t_off_s": 10.0}] * 3
    assert [b["rc"] for b in res[0]["bursts"]] == [0, 0, 0]
    assert _last_audit(str(tmp_path))["axis"] == "load_shaping"
    assert res[0]["audit"]["status"] == "done"        # 审计行回填
    # 失败传播：burst rc 非零 → status failed（不谎报 done）
    ex2 = sa.AxesExecutor(str(tmp_path / "b"), CAPS_FULL)
    ex2._aggressor_burst = lambda t_on, t_off: 7
    res2 = ex2.execute(LOAD_SPEC)
    assert res2[0]["status"] == "failed" and res2[0]["bursts"][0]["rc"] == 7
    # 异常传播：发射器异常 → status error（响亮入案，不吞）
    ex3 = sa.AxesExecutor(str(tmp_path / "c"), CAPS_FULL)
    def _boom(t_on, t_off):
        raise RuntimeError("binary gone")
    ex3._aggressor_burst = _boom
    res3 = ex3.execute(dict(LOAD_SPEC, duration_s=20))
    assert res3[0]["status"] == "error" and "binary gone" in res3[0]["reason"]


def test_load_shaping_default_burst_binding(tmp_path):
    ex = sa.AxesExecutor(str(tmp_path), CAPS_FULL)
    # 默认实现拒绝无 cpus 的受界纪律违规（M3：不做全核新负载）
    ex._active_load_spec = dict(LOAD_SPEC)
    with pytest.raises(ValueError, match="cpus"):
        ex._aggressor_burst(10, 10)
    # 默认绑定：复用 sdc_profile.ProfileRunner 的受界发射（绝不真 spawn）
    launched, awaited, cleaned, holds = [], [], [], []
    class FakeProc:
        pass
    class FakeRunner:
        def __init__(self, resolved, data_root):
            launched.append((resolved, data_root))
        def _launch(self, role, cpus, test, extra):
            launched.append((role, list(cpus), test, list(extra)))
            return FakeProc()
        def _await(self, proc, timeout):
            awaited.append(timeout)
            return 0
        def _cleanup_orphans(self, pids):
            cleaned.append(list(pids))
    import sdc_profile
    orig = sdc_profile.ProfileRunner
    sdc_profile.ProfileRunner = FakeRunner
    try:
        ex._active_load_spec = dict(LOAD_SPEC, cpus=[96, 97])
        rc = ex._aggressor_burst(10.5, 0)             # 10.5s → 向上取整 11s
        assert rc == 0
        role, cpus, test, extra = launched[-1]
        assert role == "aggressor" and cpus == [96, 97]
        assert test == sa.DEFAULT_AGGRESSOR_TEST       # power_virus_dit（v5 §7.4.3）
        assert extra[:2] == ["-t", "11s"] and extra[2:] == ["-n", "16"]  # 整秒+并发上限
        assert awaited == [11 + 120]                   # duration+AWAIT_GRACE_S
        assert len(cleaned) == 1 and isinstance(cleaned[0][0], FakeProc)
        ex._hold = lambda s: holds.append(s)
        ex._aggressor_burst(10, 2.5)                   # t_off → 空转 hold
        assert holds == [2.5]
        ex._active_load_spec = dict(LOAD_SPEC, cpus=[96], max_threads=8)
        ex._aggressor_burst(10, 0)
        assert launched[-1][3][2:] == ["-n", "8"]      # spec.max_threads 覆写
    finally:
        sdc_profile.ProfileRunner = orig


# ---------------------------------------------------------------------------
# governor 实验档：期望拒绝是显式行为（M3 裁定）+ 能力缺口短路

def test_governor_axis_expected_rejection(tmp_path):
    cap = []
    ex = FakeAxes(str(tmp_path), CAPS_FULL, script=["rejected"], capture=cap)
    res = ex.execute(GOV_SPEC)
    assert len(res) == 1 and res[0]["status"] == "capability_gap_rejected"
    # 写了原始字符串请求（monitor 代理协议：cat 后与 performance 等值比较）
    # ——fake 终态化是 rename（同真实 mv），故从 capture 断言请求原文
    assert cap == [("governor", "powersave\n")]
    assert res[0]["terminal"].startswith("governor.rejected.")   # monitor 命名保真
    assert "7.4.1" in res[0]["reason"] and "授权" in res[0]["reason"]
    # 审计在案（能力缺口如实暴露——不是静默跳过）
    line = _last_audit(str(tmp_path))
    assert line["axis"] == "governor_experiment"
    assert line["status"] == "capability_gap_rejected"
    assert res[0]["audit"] == line


def test_governor_axis_capability_gap_shortcircuit(tmp_path):
    ex = FakeAxes(str(tmp_path), CAPS_NO_GOV)         # GOVERNOR_WRITABLE=no
    res = ex.execute(GOV_SPEC)
    assert res[0]["status"] == "capability_gap"
    assert "GOVERNOR_WRITABLE" in res[0]["reason"]
    assert not os.path.exists(os.path.join(str(tmp_path), "cmd", "governor.request"))
    ex2 = FakeAxes(str(tmp_path / "b"), CAPS_NO_HOTPLUG)
    res2 = ex2.execute(PLUG_SPEC)
    assert res2[0]["status"] == "capability_gap"      # hotplug 同理：物理缺口不写请求
    assert "CPU_ONLINE_WRITABLE" in res2[0]["reason"]


def test_governor_axis_spec_validation(tmp_path):
    with pytest.raises(ValueError, match="performance"):
        FakeAxes(str(tmp_path), CAPS_FULL).execute(
            dict(GOV_SPEC, **{"from": "powersave"}))  # 起点必须 performance（不变式）
    with pytest.raises(ValueError, match="元字符"):
        FakeAxes(str(tmp_path), CAPS_FULL).execute(dict(GOV_SPEC, to="x;rm"))
    # to=performance：monitor 代理接受（幂等回不变式）
    ex = FakeAxes(str(tmp_path / "c"), CAPS_FULL, script=["done"])
    assert ex.execute(dict(GOV_SPEC, to="performance"))[0]["status"] == "done"
    # 非 performance 却 done：不变式被破——响亮记 unexpected_done（不谎报）
    ex2 = FakeAxes(str(tmp_path / "d"), CAPS_FULL, script=["done"])
    assert ex2.execute(GOV_SPEC)[0]["status"] == "unexpected_done"


# ---------------------------------------------------------------------------
# cpu_hotplug：文件协议（helper schema）+ 交叉验证 + 三分支 + then 语义

def test_hotplug_offline_then_online_full_protocol(tmp_path, monkeypatch):
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: False)
    cap = []
    ex = FakeAxes(str(tmp_path), CAPS_FULL, script=["done", "done"], capture=cap)
    res = ex.execute(PLUG_SPEC)
    assert [r["phase"] for r in res] == ["offline", "online"]
    assert [r["status"] for r in res] == ["done", "done"]
    assert res[0]["terminal"].startswith("cpu_hotplug.request.done.")  # helper 命名保真
    holds = [c for c in cap if c[0] == "hold"]
    assert holds == [("hold", 30)]                    # offline done 后 hold 30s 再 online
    reqs = [json.loads(raw) for p, raw in cap if p == "cpu_hotplug" and raw]
    assert [r["action"] for r in reqs] == ["cpu_offline", "cpu_online"]
    assert all(r["parameters"] == {"cpus": [96, 97]} for r in reqs)
    assert all(r["schema_version"] == "1" for r in reqs)
    assert all(srh.ACTION_ID_RE.match(r["action_id"]) for r in reqs)
    assert reqs[0]["action_id"] != reqs[1]["action_id"]    # 各相独立 nonce
    assert all(r["caller_pid"] == os.getpid() for r in reqs)
    for r in reqs:                                    # expires_at 可解析、带时区、未过期
        when = datetime.fromisoformat(r["expires_at"])
        assert when.tzinfo is not None and when > datetime.now(when.tzinfo)


def test_hotplug_request_cross_validates_with_helper(tmp_path, monkeypatch):
    """M2 helper 侧直接可消费：validate 判据交叉 + HelperLoop 真吃请求文件。"""
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: False)
    cap = []
    ex = FakeAxes(str(tmp_path), CAPS_FULL, script=["done"], capture=cap)
    ex.execute(dict(PLUG_SPEC, then=None))
    req = json.loads([raw for p, raw in cap if raw][0])
    ok, why = srh.validate(req["action"], req["parameters"])
    assert ok, why                                    # 我们的参数即 helper 的判据
    # 全链路：真实 HelperLoop 消费执行器写的请求文件（execute 打桩——不真写
    # sysfs）。_hotplug_phase 直接调用 + wait 打桩 timeout——请求留在盘上给 helper
    ex2 = sa.AxesExecutor(str(tmp_path / "b"), CAPS_FULL)
    ex2._wait_request = lambda cmd_dir, prefix, action_id, timeout: ("timeout", None)
    ex2._hotplug_phase("offline", [96, 97])
    root2 = str(tmp_path / "b")
    req_path = os.path.join(root2, "cmd", "cpu_hotplug.request")
    assert os.path.exists(req_path)
    seen = []
    def fake_execute(action, params, data_root, **kw):
        seen.append((action, params))
        return {"status": "done", "action": action}
    monkeypatch.setattr(srh, "execute", fake_execute)
    out = srh.HelperLoop(root2, os.path.join(root2, "cmd")).poll_once()
    assert seen == [("cpu_offline", {"cpus": [96, 97]})]
    assert out and out[0]["status"] == "done"
    assert not os.path.exists(req_path)               # 已 mv 终态化
    import glob as g
    assert g.glob(req_path + ".done.*")


def test_hotplug_rejected_and_expired(tmp_path, monkeypatch):
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: False)
    ex = FakeAxes(str(tmp_path), CAPS_FULL, script=["rejected"])
    res = ex.execute(PLUG_SPEC)
    assert res[0]["status"] == "rejected"
    assert res[1]["phase"] == "online" and res[1]["status"] == "skipped"
    assert "rejected" in res[1]["reason"]             # 前置未执行 → then 跳过（入案）
    ex2 = FakeAxes(str(tmp_path / "b"), CAPS_FULL, script=["expired"])
    res2 = ex2.execute(PLUG_SPEC)
    assert res2[0]["status"] == "expired" and res2[1]["status"] == "skipped"


def test_hotplug_timeout_attempts_recovery(tmp_path, monkeypatch):
    """offline 超时=状态未知 → then:online 仍尝试（收敛语义）；拒绝则如实双 timeout。"""
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: False)
    ex = FakeAxes(str(tmp_path), CAPS_FULL, script=["timeout", "done"])
    res = ex.execute(PLUG_SPEC)
    assert [r["status"] for r in res] == ["timeout", "done"]
    assert [r["phase"] for r in res] == ["offline", "online"]


def test_hotplug_local_prevalidation_refusals(tmp_path, monkeypatch):
    """本地预校验（helper 同判据）：CPU0 / ⊄possible / 战役运行中——不写请求直接拒。"""
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: False)
    for cpus, frag in ([0], "CPU0"), ([999], "possible"):
        ex = FakeAxes(str(tmp_path), CAPS_FULL, script=["done"])
        res = ex.execute({"axis": "cpu_hotplug", "op": "offline", "cpus": cpus})
        assert res[0]["status"] == "rejected" and frag in res[0]["reason"]
        assert not os.path.exists(os.path.join(str(tmp_path), "cmd", "cpu_hotplug.request"))
    # run 边界（v5 §7.4.4）：探针=True（战役运行中——本机实测 stress-ng 活跃时
    # 真实探针即为 True）→ cpu_offline 拒绝。打桩 True 走真实 validate 分支，
    # 不依赖当下机器瞬态（切片间隙探针可为 False——那不是可断言的稳定语义）
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: True)
    ex2 = FakeAxes(str(tmp_path / "b"), CAPS_FULL, script=["done"])
    res2 = ex2.execute({"axis": "cpu_hotplug", "op": "offline", "cpus": [96],
                        "then": "online"})
    assert res2[0]["status"] == "rejected" and "sdcshield" in res2[0]["reason"]
    assert res2[1]["status"] == "skipped"
    assert not os.path.exists(os.path.join(str(tmp_path / "b"), "cmd",
                                           "cpu_hotplug.request"))


# ---------------------------------------------------------------------------
# restore_all：restore.request + 读回断言（_read_online 注入点）

def test_restore_all_readback_assertion(tmp_path, monkeypatch):
    monkeypatch.setattr(srh, "_sdcshield_running", lambda: False)
    root = str(tmp_path)
    os.makedirs(os.path.join(root, "spool"), exist_ok=True)
    with open(os.path.join(root, "spool", "pre_state_online.txt"), "w") as f:
        f.write("0-127\n")                            # helper 首次 hotplug 前落的快照
    ex = FakeAxes(root, CAPS_FULL, script=["done"])
    ex._read_online = lambda: "0-127"
    res = ex.restore_all()
    assert len(res) == 1 and res[0]["status"] == "done"
    assert res[0]["snapshot"] == "0-127" and res[0]["readback"] == "0-127"
    # 读回 ≠ 快照 → readback_mismatch（诚实可见，不谎报 done）
    ex2 = FakeAxes(str(tmp_path / "b"), CAPS_FULL, script=["done"])
    os.makedirs(os.path.join(str(tmp_path / "b"), "spool"), exist_ok=True)
    with open(os.path.join(str(tmp_path / "b"), "spool", "pre_state_online.txt"), "w") as f:
        f.write("0-127\n")
    ex2._read_online = lambda: "0-95,128-191"
    res2 = ex2.restore_all()
    assert res2[0]["status"] == "readback_mismatch"
    # 无前置快照：无从恢复——不写请求
    ex3 = FakeAxes(str(tmp_path / "c"), CAPS_FULL, script=["done"])
    res3 = ex3.restore_all()
    assert res3[0]["status"] == "no_snapshot"
    assert not os.path.exists(os.path.join(str(tmp_path / "c"), "cmd", "restore.request"))
    # helper 侧拒绝透传
    ex4 = FakeAxes(str(tmp_path / "d"), CAPS_FULL, script=["rejected"])
    os.makedirs(os.path.join(str(tmp_path / "d"), "spool"), exist_ok=True)
    with open(os.path.join(str(tmp_path / "d"), "spool", "pre_state_online.txt"), "w") as f:
        f.write("0-127\n")
    assert ex4.restore_all()[0]["status"] == "rejected"


# ---------------------------------------------------------------------------
# _wait_request 注入点：终态归属（inode 同一性——陈旧终态排除）+ 真实轮询

def test_wait_request_stale_exclusion_and_polling(tmp_path):
    root = str(tmp_path)
    # 陈旧终态（先于本次请求创建——inode 不同）：不得把上一轮的 done 当本轮完成
    ex = sa.AxesExecutor(root, CAPS_FULL)
    stale = os.path.join(root, "cmd", f"stale.done.{int(time.time())}")
    os.makedirs(os.path.join(root, "cmd"), exist_ok=True)
    with open(stale, "w"):
        pass
    ex._write_request("stale.request", "x")
    assert ex._wait_request(os.path.join(root, "cmd"), "stale", None, 0.3) \
        == ("timeout", None)
    # 同名旧 ts 终态（请求提交后创建但 inode 不同——并发写者/他轮残留）——排除
    ex2 = sa.AxesExecutor(str(tmp_path / "b"), CAPS_FULL)
    cmd2 = os.path.join(str(tmp_path / "b"), "cmd")
    ex2._write_request("oldts.request", "x")
    with open(os.path.join(cmd2, f"oldts.done.{int(time.time()) - 100}"), "w"):
        pass
    assert ex2._wait_request(cmd2, "oldts", None, 0.3) == ("timeout", None)
    # 终态延迟出现——真实轮询等到（rename 保 inode：模拟 helper mv 终态化）
    ex3 = sa.AxesExecutor(str(tmp_path / "c"), CAPS_FULL)
    cmd3 = os.path.join(str(tmp_path / "c"), "cmd")
    ex3._write_request("late.request", "x")
    term = os.path.join(cmd3, f"late.done.{int(time.time())}")
    req3 = os.path.join(cmd3, "late.request")
    t = threading.Timer(0.15, lambda: os.replace(req3, term))
    t.start()
    try:
        status, path = ex3._wait_request(cmd3, "late", None, 5.0)
        assert status == "done" and path == term
    finally:
        t.join()


# ---------------------------------------------------------------------------
# spec 校验（配置错误必须响亮失败，不静默降级）

def test_axis_spec_validation(tmp_path):
    ex = FakeAxes(str(tmp_path), CAPS_FULL)
    with pytest.raises(ValueError, match="轴"):
        ex.execute({"axis": "voltage_experiment"})    # M3 无电压轴（v5 §7.4.2 R3 禁用）
    with pytest.raises(ValueError, match="duty"):
        ex.execute(dict(LOAD_SPEC, duty=0))
    with pytest.raises(ValueError, match="max_threads"):
        ex.execute(dict(LOAD_SPEC, max_threads=32))   # M3 受界：aggressor 并发 ≤16
    with pytest.raises(ValueError, match="cpus"):
        ex.execute({"axis": "cpu_hotplug", "op": "offline"})
    with pytest.raises(ValueError, match="op"):
        ex.execute({"axis": "cpu_hotplug", "op": "reboot", "cpus": [96]})
    with pytest.raises(ValueError, match="then"):
        ex.execute({"axis": "cpu_hotplug", "op": "offline", "cpus": [96],
                    "then": "offline"})               # then==op 无意义
    with pytest.raises(ValueError, match="cpus"):
        ex.execute({"axis": "cpu_hotplug", "op": "offline", "cpus": [True, 96]})

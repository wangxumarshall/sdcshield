"""sdc_profile 单元测试（M3 Task 1）——全 mock，绝不真跑 sdcshield。

红线（战役+9 服务运行中）：本文件不得触发任何真实负载/真实全局 pkill。
ProfileRunner 的真实发射路径（bash retest_guarded）经 _launch/_await 注入点
mock；_cleanup_orphans 默认实现走 SDC_ORPHAN_PIDS 沙盒——只 kill 注入的假
pid 字符串（不存在即静默无操作），永不落入 pkill 全局路径（2026-09-26 实测
污染战役教训：comm=control 与运行中切片不可区分）。
"""
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_profile as sp

CAPS = {"NPROC": "128", "GOVERNOR_WRITABLE": "no", "CPU_ONLINE_WRITABLE": "yes",
        "HAS_CPUFREQ": "yes", "PMU_CORE_COUNTERS": "12"}
TOPO = {"possible": list(range(128)), "online": list(range(128)),
        "nodes": {"1": [32, 33], "3": [96, 97]}}   # 精简拓扑 fixture

PROFILE = {
    "profile_id": "victim_test_v1",
    "victim": {"cpus": [96], "tests": ["openblas_dgemm"], "seed_policy": "fixed",
               "iterations_per_seed": 100},
    "aggressors": {"topology": "all_except_victim", "families": ["cache", "integer"],
                   "duty_cycle": 1.0},
    "environment": {"governor": "performance", "numa_policy": "recorded_default"},
    "monitoring": {"pmu_groups": ["core_base"], "steady_period_ms": 1000},
    "safety_policy": "lab_default_v1", "limits": {"duration_s": 300, "max_failures": 3}}

SHIPPED = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "..",
                       "configs", "sdc-excite-reproduce", "profiles",
                       "victim_candidate_v1.json")


def test_load_profile_validates_required():
    p = sp.load_profile_dict(PROFILE)
    assert p["profile_id"] == "victim_test_v1"
    import pytest
    bad = dict(PROFILE); del bad["victim"]
    with pytest.raises(ValueError, match="victim"):
        sp.load_profile_dict(bad)

def test_resolve_expands_all_except_victim():
    r = sp.resolve(PROFILE, CAPS, TOPO)
    assert 96 not in r["aggressor_cpus"] and 97 in r["aggressor_cpus"]
    assert r["binary_hash"] and r["capabilities_snapshot"] == CAPS
    assert r["topology_snapshot"]["online"] == TOPO["online"]

def test_resolve_rejects_capability_gap():
    bad_caps = dict(CAPS, CPU_ONLINE_WRITABLE="no")
    prof = dict(PROFILE); prof["victim"] = dict(PROFILE["victim"], cpus=[96])
    prof["axes"] = {"cpu_hotplug": {"enabled": True}}
    import pytest
    with pytest.raises(ValueError, match="CPU_ONLINE"):
        sp.resolve(prof, bad_caps, TOPO)

def test_runner_orchestration_mock(tmp_path):
    r = sp.resolve(PROFILE, CAPS, TOPO)
    launched = []
    class FakeRunner(sp.ProfileRunner):
        def _launch(self, role, cpus, test, extra):
            launched.append((role, tuple(cpus), test))
            return f"pid-{role}"
        def _await(self, pid, timeout): return 0
    res = FakeRunner(r, str(tmp_path)).run_once()
    roles = [l[0] for l in launched]
    assert roles[0] == "victim" and "aggressor" in roles
    assert res["rc"] == 0 and res["launched"] == len(launched)

# ---- 边界（Task 1 要求）：victim ⊄ online / dry-run / 能力与安全不变式 ----

def test_resolve_rejects_victim_not_online():
    import pytest
    prof = dict(PROFILE); prof["victim"] = dict(PROFILE["victim"], cpus=[96, 200])
    with pytest.raises(ValueError, match="online"):
        sp.resolve(prof, CAPS, TOPO)

def test_resolve_rejects_governor_axis_gap():
    import pytest
    prof = dict(PROFILE); prof["axes"] = {"governor_experiment": {"enabled": True}}
    with pytest.raises(ValueError, match="GOVERNOR"):
        sp.resolve(prof, CAPS, TOPO)          # GOVERNOR_WRITABLE=no → 能力缺口拒绝

def test_resolve_rejects_non_performance_governor():
    import pytest
    prof = dict(PROFILE)
    prof["environment"] = dict(PROFILE["environment"], governor="powersave")
    with pytest.raises(ValueError, match="performance"):
        sp.resolve(prof, CAPS, TOPO)          # M3 安全不变式：恒 performance

def test_resolve_explicit_aggressor_topology():
    import pytest
    overlap = dict(PROFILE)
    overlap["aggressors"] = dict(PROFILE["aggressors"], topology="explicit",
                                 cpus=[96, 97])
    with pytest.raises(ValueError, match="victim"):    # 与 victim 重叠拒绝
        sp.resolve(overlap, CAPS, TOPO)
    offline = dict(PROFILE)
    offline["aggressors"] = dict(PROFILE["aggressors"], topology="explicit",
                                 cpus=[200])
    with pytest.raises(ValueError, match="online"):    # ⊄ online 拒绝
        sp.resolve(offline, CAPS, TOPO)
    ok = dict(PROFILE)
    ok["aggressors"] = dict(PROFILE["aggressors"], topology="explicit",
                            cpus=[32, 33])
    r = sp.resolve(ok, CAPS, TOPO)
    assert r["aggressor_cpus"] == [32, 33]

def test_load_capabilities_env_format(tmp_path):
    f = tmp_path / "caps.env"
    f.write_text("# 注释行\nNPROC=128\nPMU_CORE_COUNTERS=12  # 行内注释\n"
                 'MEM_NODES="1 3"\n')
    caps = sp.load_capabilities(str(f))
    assert caps == {"NPROC": "128", "PMU_CORE_COUNTERS": "12", "MEM_NODES": "1 3"}

def test_shipped_profile_file_loads_and_resolves():
    p = sp.load_profile(SHIPPED)               # 含 "_comment" 注释头——校验须忽略
    assert "_comment" not in p and p["profile_id"] == "victim_candidate_v1"
    r = sp.resolve(p, CAPS, TOPO)
    assert r["victim_cpus"] == [96] and 96 not in r["aggressor_cpus"]
    assert {"cache", "integer"} <= set(r["aggressor_family_tests"])
    assert r["aggressor_max_threads"]           # 受界纪律：aggressor 并发上限冻结

def test_dry_run_writes_resolved_json(tmp_path, capsys, monkeypatch):
    prof = tmp_path / "p.json"; prof.write_text(json.dumps(PROFILE))
    caps_f = tmp_path / "caps.env"
    caps_f.write_text("NPROC=128\nGOVERNOR_WRITABLE=no\nCPU_ONLINE_WRITABLE=yes\n"
                      "HAS_CPUFREQ=yes\nPMU_CORE_COUNTERS=12  # 实测\n")
    topo_f = tmp_path / "topo.json"; topo_f.write_text(json.dumps(TOPO))
    class NoExec(sp.ProfileRunner):
        def __init__(self, *a, **k): raise AssertionError("dry-run 不应构造执行器")
    monkeypatch.setattr(sp, "ProfileRunner", NoExec)
    rc = sp.main(["run", str(prof), "--dry-run",
                  "--capabilities", str(caps_f), "--topology", str(topo_f)])
    assert rc == 0
    r = json.loads(capsys.readouterr().out)    # resolved JSON 只写 stdout
    assert 96 not in r["aggressor_cpus"] and 97 in r["aggressor_cpus"]
    assert r["binary_hash"].startswith("sha256:")
    assert r["capabilities_snapshot"]["PMU_CORE_COUNTERS"] == "12"  # 行内注释剥离

def test_runner_launch_args_carry_duration_and_seed(tmp_path):
    r = sp.resolve(PROFILE, CAPS, TOPO)
    assert r["victim_seed"] == sp.resolve(PROFILE, CAPS, TOPO)["victim_seed"]  # 确定性
    seen, awaits = [], []
    class RecRunner(sp.ProfileRunner):
        def _launch(self, role, cpus, test, extra):
            seen.append((role, list(extra))); return object()
        def _await(self, pid, timeout):
            awaits.append(timeout); return 0
        def _cleanup_orphans(self, pids):
            seen.append(("cleanup", [str(p) for p in pids]))
    res = RecRunner(r, str(tmp_path)).run_once()
    vic = next(s for s in seen if s[0] == "victim")
    assert "300s" in vic[1]                            # -t <limits.duration_s>
    i = vic[1].index("-s")
    assert vic[1][i + 1] == r["victim_seed"]           # fixed → -s <seed>
    aggr = next(s for s in seen if s[0] == "aggressor")
    assert "300s" in aggr[1]                           # aggressor 同 -t 受界自停
    assert all(t == 300 + 120 for t in awaits)         # _await 超时 = duration+120
    cl = next(s for s in seen if s[0] == "cleanup")
    assert len(cl[1]) == res["launched"] == 3          # 收残覆盖全部拉起 pid

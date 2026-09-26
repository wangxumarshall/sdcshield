"""sdc_reproducer 单元测试（M3 Task 3）——全 mock，绝不真跑 sdcshield/真打包大文件。

红线（战役+9 服务运行中）：
  - 健康核对照经 mock `_control_run()`（brief 裁定）——绝不真发射负载；
  - run_cycle 经 FakeReproducer 覆写 `_run_once`——绝不真跑 victim/aggressor；
  - capsule 用小 fixture（假二进制几十字节 + 精简拓扑）——binaries/ 只允许
    sha256+路径引用，绝不复制二进制；
  - run.sh 只以 `--check-only` 执行（预检路径，任何分支都在启动负载前退出）；
    正向用例以本机实时拓扑快照构建 capsule（只读 sysfs + 哈希比对，安全）。

Wilson 95% 区间数学口径：k=0 下界恰为 0、k=n 上界恰为 1（精确值，显式特判）；
全成功下界闭式 1/(1+z²/n)——brief 原文 `lo2 > 0.84` 为四舍五入笔误（真值
20/20 → 0.8389 < 0.84），按真值断言（诚实优先，见 test_wilson_ci_bounds 注）。
"""
import hashlib, json, os, subprocess, sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "telemetry"))
import sdc_reproducer as sr
import sdc_profile as sp
import sdc_topology

SEED = "AES:ebcafcfbfea7c6f802f2d9be8194e42ddbcb3fe964a5f0f9c8afd1f00e419d30"
CPU_MASK = "." * 66 + "X" + "." * 61          # X 位=66（本板单 package 扁平：位号=逻辑 CPU）

STDOUT_SUMMARY = (
    "- test: zstd19\n"
    "  result: fail\n"
    f"  fail: {{ cpu-mask: '{CPU_MASK}', time-to-fail: 2.511, seed: '{SEED}'}}\n"
)

CONTEXT = (
    "cmd: /opt/sdcshield/builddir/sdcshield --cpuset=66,90,91,92 -e zstd19 "
    "-t 300s -o /tmp/x.yaml\n"
    f"rc: 1  date: 2026-09-26T10:00:00+08:00  fail_seed: {SEED}(usable=1)"
    "  failed_test: zstd19\n"
    "--- monitor 最近 20 行 ---\n"
    "2026-09-26 10:00:00,79,50,38,30,35,,324,0.85,0.88\n"
    "--- EDAC ---\n0\n"
    "--- mem ---\n"
    "              total        used        free\n"
    "Mem:          384          100          284\n"
)

YAML_EXTRACT = (
    "# 提取自 100000-x.yaml（原件 2.7G 保留于 logs/，每日 gzip 归档）\n"
    "command-line: 'sdcshield --cpuset=66,90,91,92 -e zstd19 -t 300s'\n"
    "version: sdcshield-6c76ea63a7a9\n"
    "- test: zstd19\n"
    '  details: { quality: production, description: "Zstandard level 19" }\n'
    f"  state: {{ seed: '{SEED}', iteration: 5, retry: false }}\n"
    "  result: fail\n"
    f"  fail: {{ cpu-mask: '{CPU_MASK}', time-to-fail: 2.511, seed: '{SEED}'}}\n"
    "  test-runtime: 6.876\n"
    "  threads:\n"
    "  - thread: 66\n"
    "    id: { logical:  66, package: 8442, numa_node: 2, module: 8544,"
    " core:  66, thread: 0, family: 72, model: 0xd01, stepping: 0,"
    " microcode: null, ppin: null }\n"
    "    state: failed\n"
    "    time-to-fail: 2.511\n"
    "    messages:\n"
    "      data-miscompare:\n"
    "\n"
    "      description: 'zstd19 miscompare'\n"
    "      type:        uint32_t\n"
    "      offset:      [ 44, 0 ]\n"
    "      address:     '0x0000ffff9c2c002c'\n"
    "      actual:      '0x000000ff'\n"
    "      expected:    '0x000000f7'\n"
    "      mask:        '0x00000008'\n"        # ff^f7=08：单 bit 翻转（SDC 经典签名）
)

RETESTS = ("retest1(targeted) rc=0 fail/crash=0\n"
           "retest2(targeted) rc=0 fail/crash=0\n"
           "retest3(targeted) rc=0 fail/crash=0\n")


def make_event(root, *, rc=1, stdout=STDOUT_SUMMARY, context=CONTEXT,
               yaml_extract=YAML_EXTRACT, retests=RETESTS, race_audit=None,
               cpuset="66,90,91,92"):
    """合成 M2 handle_failure 口径的事件目录（缺省=七项全过的一致事件）。"""
    ev = root / "ev"
    ev.mkdir(parents=True)
    if stdout is not None:
        (ev / "stdout_summary.out").write_text(stdout)
    if context is not None:
        ctx = context
        if cpuset is not None:
            ctx = ctx.replace("--cpuset=66,90,91,92", f"--cpuset={cpuset}")
        if rc != 1:
            ctx = ctx.replace("rc: 1 ", f"rc: {rc} ", 1)
        (ev / "context.txt").write_text(ctx)
    if yaml_extract is not None:
        (ev / "yaml_extract.txt").write_text(yaml_extract)
    if retests is not None:
        (ev / "retests.txt").write_text(retests)
    if race_audit is not None:
        (ev / "race_audit.json").write_text(json.dumps(race_audit))
    return str(ev)


def passing_ctx():
    return {"binary": "/opt/sdcshield/builddir/sdcshield", "control_cpus": [120],
            "sham_result": {"k": 0, "n": 3}}


def mock_control(calls, rc=0, fail_count=0):
    def _fake(binary, test, seed, cpus, timeout_s=120):
        calls.append({"binary": binary, "test": test, "seed": seed,
                      "cpus": list(cpus), "timeout_s": timeout_s})
        return {"rc": rc, "fail_count": fail_count}
    return _fake


@pytest.fixture(autouse=True)
def _no_real_control_run(monkeypatch):
    """红线（战役+9 服务运行中）：默认拦截真实 _control_run——任何 gate 用例
    传入含 binary+control_cpus 的 ctx 都不得真发射对照负载。需要断言调用参数
    的用例（item4/full_pass）在此之上自行 monkeypatch 覆写（后设者优先）。"""
    monkeypatch.setattr(sr, "_control_run",
                        lambda *a, **k: {"rc": 0, "fail_count": 0})


CAPS = {"NPROC": "128", "PMU_CORE_COUNTERS": "12"}
TOPO = {"possible": list(range(128)), "online": list(range(128)),
        "offline": [], "isolated": [],
        "cpus": {str(c): {"online": True, "package": 36, "core_id": c,
                          "cluster_id": 138 + (c // 4) * 516, "die_id": -1}
                 for c in range(128)},
        "nodes": {"0": {"cpulist": "0-63", "has_memory": True},
                  "1": {"cpulist": "64-127", "has_memory": True}}}


# ---------------------------------------------------------------------------
# wilson_ci（brief 核心测试；全成功下界按数学真值断言）

def test_wilson_ci_bounds():
    lo, hi = sr.wilson_ci(0, 20)
    assert lo == 0.0 and hi < 0.20        # rule of three 量级（真值 0.161）
    lo2, hi2 = sr.wilson_ci(20, 20)
    # brief 原文 lo2 > 0.84 为四舍五入笔误：Wilson 95% 全成功下界闭式
    # 1/(1+z²/n) = 1/(1+3.8415/20) ≈ 0.83887 < 0.84——按真值断言（诚实优先）
    assert lo2 == pytest.approx(1 / (1 + 1.959963984540054 ** 2 / 20), abs=1e-9)
    assert lo2 > 0.83
    assert hi2 == 1.0


def test_wilson_ci_properties():
    lo, hi = sr.wilson_ci(10, 20)          # 对称中点：区间跨 0.5
    assert lo < 0.5 < hi and 0 <= lo < hi <= 1
    assert sr.wilson_ci(1, 1)[0] > 0.0     # 单次成功下界非零
    for bad in [(2, 1), (-1, 10), (0, 0)]:
        with pytest.raises(ValueError):
            sr.wilson_ci(*bad)


# ---------------------------------------------------------------------------
# authenticity_gate 七项（v5 §10.1）

def test_authenticity_gate_catches_stage_boundary(tmp_path):
    ev = tmp_path / "ev"; ev.mkdir()
    (ev / "stdout_summary.out").write_text(
        "- test: zstd19\n  result: fail\n")
    (ev / "context.txt").write_text("rc: 143\n")     # 阶段边界 SIGTERM
    ok, reasons = sr.authenticity_gate(str(ev), {})
    assert not ok and any("完整迭代" in r for r in reasons)


def test_gate_item1_missing_evidence(tmp_path):
    ev = make_event(tmp_path, stdout=None)           # 缺 stdout_summary.out
    ok, reasons = sr.authenticity_gate(ev, passing_ctx())
    assert not ok and any("原始证据" in r for r in reasons)


def test_gate_item2_in_run_fail_with_kill_rc_passes(tmp_path):
    """mesh 事件形态：YAML 有 fail 行（完整迭代的框架级失败记录）+ rc=137
    （数小时后热联锁收尾 KILL）——失败迭代本身完整，不得按阶段边界拒。"""
    ev = make_event(tmp_path, rc=137)
    ok, reasons = sr.authenticity_gate(ev, passing_ctx())
    assert not any("完整迭代" in r for r in reasons)


def test_gate_item3_mask_recompute(tmp_path):
    ok, reasons = sr.authenticity_gate(make_event(tmp_path), passing_ctx())
    assert not any("mask" in r and "拒" in r for r in reasons)   # 一致→不拒
    tampered = YAML_EXTRACT.replace("mask:        '0x00000008'",
                                    "mask:        '0x00000009'")
    ok2, reasons2 = sr.authenticity_gate(
        make_event(tmp_path / "b", yaml_extract=tampered), passing_ctx())
    assert not ok2 and any("mask 重算" in r for r in reasons2)


def test_gate_item4_healthy_core_control(tmp_path, monkeypatch):
    calls = []
    monkeypatch.setattr(sr, "_control_run", mock_control(calls))
    ev = make_event(tmp_path)
    ok, reasons = sr.authenticity_gate(ev, passing_ctx())
    assert ok and not any("健康核" in r for r in reasons)
    assert calls == [{"binary": "/opt/sdcshield/builddir/sdcshield",
                      "test": "zstd19", "seed": SEED, "cpus": [120],
                      "timeout_s": 120}]
    # 反例：健康核同样失败——确定性测试缺陷候选，拒
    monkeypatch.setattr(sr, "_control_run", mock_control([], rc=1, fail_count=2))
    ok2, reasons2 = sr.authenticity_gate(make_event(tmp_path / "b"), passing_ctx())
    assert not ok2 and any("健康核对照同样失败" in r for r in reasons2)
    # 缺 control_cpus：未执行→警告不拒
    ok3, reasons3 = sr.authenticity_gate(make_event(tmp_path / "c"), {})
    assert ok3 and any("健康核对照未执行" in r for r in reasons3)


def test_gate_item5_race_audit(tmp_path):
    # 无产物：警告"未审计"（引用 docs output 附3 检查单）不拒
    ok, reasons = sr.authenticity_gate(make_event(tmp_path), passing_ctx())
    assert ok and any("竞态未审计" in r and "附3" in r for r in reasons)
    # 定案竞态：test_bug 方向，拒
    ok2, reasons2 = sr.authenticity_gate(
        make_event(tmp_path / "b", race_audit={"verdict": "race_confirmed"}),
        passing_ctx())
    assert not ok2 and any("竞态审计定案" in r for r in reasons2)
    # 已审计且清白：无该项问题
    ok3, reasons3 = sr.authenticity_gate(
        make_event(tmp_path / "c", race_audit={"verdict": "clean"}),
        passing_ctx())
    assert not any("竞态" in r for r in reasons3)


def test_gate_item6_detecting_cpu_vs_cpuset(tmp_path):
    # detecting-cpu=66 不在请求 cpuset=90,91,92——affinity 未遵守/证据错位，拒
    ev = make_event(tmp_path, cpuset="90,91,92")
    ok, reasons = sr.authenticity_gate(ev, passing_ctx())
    assert not ok and any("detecting-cpu" in r for r in reasons)
    # 一致（66 ∈ 请求集）：无该项问题
    ok2, reasons2 = sr.authenticity_gate(make_event(tmp_path / "b"), passing_ctx())
    assert not any("detecting-cpu" in r for r in reasons2)


def test_gate_item7_retests_and_sham(tmp_path):
    ctx = passing_ctx(); ctx["sham_result"] = {"k": 2, "n": 3}   # sham 亦复现
    ok, reasons = sr.authenticity_gate(make_event(tmp_path), ctx)
    assert not ok and any("sham" in r for r in reasons)
    # 无有效复测：警告不拒
    ctx2 = passing_ctx()
    ok2, reasons2 = sr.authenticity_gate(
        make_event(tmp_path / "b", retests=""), ctx2)
    assert ok2 and any("复测" in r for r in reasons2)
    # 复测全数复现（k=n）：确定性缺陷候选警告（由健康核对照裁定，不拒）
    ctx3 = passing_ctx()
    ok3, reasons3 = sr.authenticity_gate(
        make_event(tmp_path / "c",
                   retests="retest1(targeted) rc=1 fail/crash=2\n"), ctx3)
    assert ok3 and any("全数复现" in r for r in reasons3)


def test_gate_full_pass(tmp_path, monkeypatch):
    calls = []
    monkeypatch.setattr(sr, "_control_run", mock_control(calls))
    ev = make_event(tmp_path, race_audit={"verdict": "clean"})
    ok, reasons = sr.authenticity_gate(ev, passing_ctx())
    assert ok and reasons == [] and calls


def test_parse_event_summary(tmp_path):
    ev = sr.parse_event(make_event(tmp_path))
    assert ev["rc"] == 1 and ev["test"] == "zstd19" and ev["seed"] == SEED
    assert ev["cpuset"] == [66, 90, 91, 92]
    assert ev["detecting_cpus"] == [66]
    assert ev["fail_lines"] >= 1
    m = ev["miscompares"][0]
    assert (m["actual"] == 0xFF and m["expected"] == 0xF7 and m["mask"] == 0x08
            and m["consistent"] and m["popcount"] == 1 and m["lane"] == 0)
    assert ev["retests"] == {"valid": 3, "reproduced": 0}


# ---------------------------------------------------------------------------
# build_capsule（v5 §10.2 结构逐项 + SHA256SUMS + binaries 引用不复制）

def fake_resolved(tmp_path, binary_path, topo=None):
    h = hashlib.sha256(open(binary_path, "rb").read()).hexdigest()
    return {
        "schema_version": 1,
        "profile_id": "repro-20260926-100000-zstd19-rc1",
        "resolved_at": "2026-09-26 10:00:00",
        "victim_cpus": [66], "victim_tests": ["zstd19"],
        "victim_seed_policy": "fixed", "victim_seed": SEED,
        "iterations_per_seed": 1,
        "aggressor_topology": "all_except_victim",
        "aggressor_cpus": [c for c in range(128) if c != 66],
        "aggressor_families": ["cache"],
        "aggressor_family_tests": {"cache": ["cachebounce"]},
        "aggressor_duty_cycle": 1.0, "aggressor_max_threads": 16,
        "environment": {"governor": "performance"},
        "monitoring": {"pmu_groups": ["core_base"], "steady_period_ms": 1000},
        "safety_policy": "lab_default_v1",
        "limits": {"duration_s": 120, "max_failures": 3},
        "axes": {},
        "capabilities_snapshot": dict(CAPS),
        "topology_snapshot": dict(topo or TOPO),
        "binary_path": str(binary_path),
        "binary_hash": "sha256:" + h,
    }


@pytest.fixture
def fake_bin(tmp_path):
    p = tmp_path / "bin" / "sdcshield"
    p.parent.mkdir()
    p.write_bytes(b"#!/bin/sh\n# fake sdcshield - capsule test fixture (tiny)\n")
    return p


CAPSULE_ITEMS = [
    "manifest.json", "resolved-profile.json", "topology.json", "platform.json",
    "binaries/sdcshield.sha256", "inputs/victim.json", "golden/README.txt",
    "event.json", "original/stdout.log", "original/result.yaml",
    "windows/monitor_tail.csv", "windows/NOTES.md",
    "scripts/run.sh", "scripts/verify.sh", "scripts/restore.sh",
    "reduction/graph.jsonl", "reduction/trials.jsonl",
    "diagnosis/hypotheses.json", "diagnosis/report.md", "SHA256SUMS",
]


def test_build_capsule_structure(tmp_path, fake_bin):
    ev = make_event(tmp_path)
    out = tmp_path / "capsule"
    resolved = fake_resolved(tmp_path, fake_bin)
    manifest = sr.build_capsule(ev, resolved, str(out))
    for rel in CAPSULE_ITEMS:
        assert (out / rel).is_file(), f"capsule 缺 {rel}"
    # manifest 关键字段
    assert manifest["event_id"] == "ev"
    assert manifest["binary"]["sha256"] == resolved["binary_hash"]
    assert manifest["binary"]["path"] == str(fake_bin)
    assert "引用不复制" in manifest["binary"]["note"]
    assert manifest["topology_hash"] == sr.topology_fingerprint(TOPO)
    assert manifest["platform"]["thermal_band"] == ["ambient", "+10C"]
    # M3 移交一行修守卫：original_evidence 必须在**落盘的** manifest.json 内
    # （原实现赋值在 _w(manifest.json) 之后——返回值有键而盘上缺键）
    disk_manifest = json.loads((out / "manifest.json").read_text())
    assert disk_manifest["original_evidence"] == [
        "original/stdout.log", "original/result.yaml",
        "original/context.txt", "original/retests.txt"]
    # binaries/ 只放引用——绝不复制二进制本体
    assert not (out / "binaries" / "sdcshield").exists()
    ref = (out / "binaries" / "sdcshield.sha256").read_text()
    assert resolved["binary_hash"].split(":")[1] in ref and str(fake_bin) in ref
    # SHA256SUMS 全文件覆盖且哈希可复验
    sums = {}
    for line in (out / "SHA256SUMS").read_text().splitlines():
        h, rel = line.split("  ", 1)
        assert h == hashlib.sha256((out / rel).read_bytes()).hexdigest()
        sums[rel] = h
    assert "scripts/run.sh" in sums and "manifest.json" in sums
    assert "SHA256SUMS" not in sums
    # run.sh：预检要素 + 冻结重放命令（victim 先起、cpuset/seed/test/duration）
    run_sh = (out / "scripts" / "run.sh").read_text()
    for marker in ["--check-only", "kernel_release", "governor", "online",
                   "topology", "thermal", "sha256sum"]:
        assert marker in run_sh, f"run.sh 缺预检要素 {marker}"
    for token in ["--cpuset=66", "-e zstd19", f"-s {SEED}", "-t 120s"]:
        assert token in run_sh, f"run.sh 缺冻结参数 {token}"


def test_capsule_run_sh_check_only_refuses_env_mismatch(tmp_path, fake_bin,
                                                        monkeypatch):
    ev = make_event(tmp_path)
    resolved = fake_resolved(tmp_path, fake_bin)
    # 假内核 release：预检必须拒（在任何负载启动前退出）
    fake_probe = dict(sr._platform_probe(TOPO), kernel_release="9.9.9-fake")
    monkeypatch.setattr(sr, "_platform_probe", lambda topo: dict(fake_probe))
    out = tmp_path / "cap-kernel"
    sr.build_capsule(ev, resolved, str(out))
    r = subprocess.run(["bash", str(out / "scripts" / "run.sh"), "--check-only"],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode != 0 and "内核 release 不符" in r.stderr
    # 假 online 集：同样拒
    fake_probe2 = dict(sr._platform_probe(TOPO), online=[0, 1, 2])
    monkeypatch.setattr(sr, "_platform_probe", lambda topo: dict(fake_probe2))
    out2 = tmp_path / "cap-online"
    sr.build_capsule(ev, resolved, str(out2))
    r2 = subprocess.run(["bash", str(out2 / "scripts" / "run.sh"), "--check-only"],
                        capture_output=True, text=True, timeout=60)
    assert r2.returncode != 0 and "online 集不符" in r2.stderr


def test_capsule_run_sh_check_only_passes_on_live_machine(tmp_path, fake_bin,
                                                          monkeypatch):
    """正向端到端（只读）：capsule 以本机实时拓扑快照+真实平台探针构建——
    内嵌预检的指纹规范化必须与 topology_fingerprint() 同源一致（漂移守卫）。"""
    ev = make_event(tmp_path)
    live = sdc_topology.snapshot()
    resolved = fake_resolved(tmp_path, fake_bin, topo=live)
    out = tmp_path / "cap-live"
    sr.build_capsule(ev, resolved, str(out))
    monkeypatch.setenv("SDC_BIN", str(fake_bin))
    r = subprocess.run(["bash", str(out / "scripts" / "run.sh"), "--check-only"],
                       capture_output=True, text=True, timeout=120)
    assert r.returncode == 0, r.stderr


# ---------------------------------------------------------------------------
# Reproducer.from_event / run_cycle

def test_from_event_builds_minimal_victim_profile(tmp_path, fake_bin,
                                                  monkeypatch):
    monkeypatch.setenv("SDC_BIN", str(fake_bin))
    ev = make_event(tmp_path)
    rep = sr.Reproducer(str(tmp_path / "root"), ev)
    resolved = rep.from_event(CAPS, TOPO)
    assert resolved["victim_cpus"] == [66]              # detecting-cpu 锚定
    assert resolved["victim_tests"] == ["zstd19"]
    assert resolved["victim_seed"] == SEED              # 事件种子冻结
    assert resolved["victim_seed_policy"] == "fixed"
    assert resolved["axes"] == {}                       # 无轴——最小起点
    assert resolved["binary_hash"] == "sha256:" + \
        hashlib.sha256(fake_bin.read_bytes()).hexdigest()
    assert "ev" in resolved["profile_id"]               # profile_id 溯源事件


def test_from_event_requires_seed_and_anchor(tmp_path, fake_bin, monkeypatch):
    monkeypatch.setenv("SDC_BIN", str(fake_bin))
    # 无种子：stdout/yaml fail 块与 context 记录均无种子字段（parse 接受任意
    # 引擎种子——LCG 亦是事件原种子，只有全源缺失才构成"无法定向复现"）
    no_seed_yaml = (
        YAML_EXTRACT
        .replace(f"  state: {{ seed: '{SEED}', iteration: 5, retry: false }}\n",
                 "  state: { iteration: 5, retry: false }\n")
        .replace(f"  fail: {{ cpu-mask: '{CPU_MASK}', time-to-fail: 2.511,"
                 f" seed: '{SEED}'}}\n",
                 f"  fail: {{ cpu-mask: '{CPU_MASK}', time-to-fail: 2.511 }}\n"))
    no_seed_ctx = CONTEXT.replace(f"fail_seed: {SEED}(usable=1)",
                                  "fail_seed: none(usable=)")
    no_seed_stdout = STDOUT_SUMMARY.replace(f", seed: '{SEED}'", "")
    ev = make_event(tmp_path, stdout=no_seed_stdout, context=no_seed_ctx,
                    yaml_extract=no_seed_yaml)
    with pytest.raises(ValueError, match="种子"):
        sr.Reproducer(str(tmp_path), ev).from_event(CAPS, TOPO)
    # 无失败核锚点（无 detecting 线程、cpu-mask 无 X）
    no_anchor_yaml = YAML_EXTRACT.replace(CPU_MASK, "." * 128) \
                                 .replace("    state: failed", "    state: passed")
    ev2 = make_event(tmp_path / "b", yaml_extract=no_anchor_yaml)
    with pytest.raises(ValueError, match="锚点"):
        sr.Reproducer(str(tmp_path), ev2).from_event(CAPS, TOPO)


class FakeReproducer(sr.Reproducer):
    """_run_once 打桩（注入点）——绝不真跑 sdcshield。"""

    def __init__(self, data_root, event_dir, script):
        super().__init__(data_root, event_dir)
        self.script, self.calls = list(script), []

    def _run_once(self, profile):
        self.calls.append(profile)
        return self.script.pop(0)


def test_run_cycle_counts_failures_and_ci(tmp_path):
    ev = make_event(tmp_path)
    script = [{"rc": 1, "fail_count": 2},   # rc≠0 → 失败
              {"rc": 0, "fail_count": 0},   # 通过
              {"rc": 0, "fail_count": 3},   # fail/crash 行 → 失败
              {"rc": 0, "fail_count": 0},
              {"rc": 2, "fail_count": 0}]   # rc≠0 → 失败
    rep = FakeReproducer(str(tmp_path), ev, script)
    out = rep.run_cycle({"axes": {}}, 5)
    assert out["k"] == 3 and out["n"] == 5 and len(rep.calls) == 5
    assert out["ci"] == sr.wilson_ci(3, 5)


def test_run_cycle_skips_memguard_trials(tmp_path):
    ev = make_event(tmp_path)
    script = [{"rc": 250, "fail_count": 0},   # 内存门拦截=无效试验
              {"rc": 0, "fail_count": 0},
              {"rc": 1, "fail_count": 0}]
    rep = FakeReproducer(str(tmp_path), ev, script)
    out = rep.run_cycle({"axes": {}}, 2)
    assert out["k"] == 1 and out["n"] == 2 and out["invalid"] == 1


def test_run_cycle_all_invalid_vacuous_ci(tmp_path):
    ev = make_event(tmp_path)
    script = [{"rc": 250, "fail_count": 0}] * 10
    rep = FakeReproducer(str(tmp_path), ev, script)
    out = rep.run_cycle({"axes": {}}, 3)
    assert out == {"k": 0, "n": 0, "ci": (0.0, 1.0), "attempts": 6, "invalid": 6}


def test_run_cycle_refuses_axes(tmp_path):
    """T2 移交②编排约束：run_cycle 不编排任何轴——显式拒绝而非静默丢弃
    （hotplug 离线动作只允许在收尾窗口，M3 未实现该编排）。"""
    ev = make_event(tmp_path)
    rep = sr.Reproducer(str(tmp_path), ev)
    with pytest.raises(ValueError, match="收尾窗口"):
        rep.run_cycle({"axes": {"cpu_hotplug": {"enabled": True}}}, 2)
    with pytest.raises(ValueError, match="轴"):
        rep.run_cycle({"axes": {"governor_experiment": {"enabled": True}}}, 2)

"""M3 Task 5 驱动集成测试——repro_queue 产出端 + --from-queue 消费端 + verify.request 闭环。

红线（战役+9 服务运行中）：全部 fixture + 隔离数据根——绝不碰真实
~/sdc-excite-reproduce 的 spool/repro_queue 与 cmd/（bash 用例一律
SDC_EXCITE_REPRODUCE_DIR=<tmp> 后 source sdc_common.sh）；python 消费路径
mock _control_run / Reproducer._run_once（绝不真发射负载）；CLI 子进程用例
只走 from_event 无种子响亮拒绝路径（任何负载启动之前退出）。
"""
import json, os, shlex, subprocess, sys, time

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "telemetry"))
import sdc_reproducer as sr

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")
DRIVER = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc-excite-reproduce.sh")
REPRODUCER = os.path.join(os.path.dirname(__file__), "..", "sdc_reproducer.py")

# ---------------- 事件 fixture（口径同 test_sdc_reproducer——自包含）----------------

SEED = "AES:ebcafcfbfea7c6f802f2d9be8194e42ddbcb3fe964a5f0f9c8afd1f00e419d30"
CPU_MASK = "." * 66 + "X" + "." * 61          # X 位=66（单 package 扁平：位号=逻辑 CPU）

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
    "command-line: 'sdcshield --cpuset=66,90,91,92 -e zstd19 -t 300s'\n"
    "- test: zstd19\n"
    f"  state: {{ seed: '{SEED}', iteration: 5, retry: false }}\n"
    "  result: fail\n"
    f"  fail: {{ cpu-mask: '{CPU_MASK}', time-to-fail: 2.511, seed: '{SEED}'}}\n"
    "  threads:\n"
    "  - thread: 66\n"
    "    id: { logical:  66, package: 8442, numa_node: 2, module: 8544,"
    " core:  66, thread: 0, family: 72, model: 0xd01, stepping: 0 }\n"
    "    state: failed\n"
    "    time-to-fail: 2.511\n"
    "    messages:\n"
    "      data-miscompare:\n"
    "        type:        uint32_t\n"
    "        offset:      [ 44, 0 ]\n"
    "        actual:      '0x000000ff'\n"
    "        expected:    '0x000000f7'\n"
    "        mask:        '0x00000008'\n"       # ff^f7=08：单 bit 翻转（SDC 经典签名）
)

RETESTS = ("retest1(targeted) rc=0 fail/crash=0\n"
           "retest2(targeted) rc=0 fail/crash=0\n"
           "retest3(targeted) rc=0 fail/crash=0\n")


def make_event(root, name="ev", *, stdout=STDOUT_SUMMARY, context=CONTEXT,
               yaml_extract=YAML_EXTRACT, retests=RETESTS):
    """合成 M2 handle_failure 口径的事件目录（缺省=七项全过的一致事件）。"""
    ev = root / name
    ev.mkdir(parents=True)
    if stdout is not None:
        (ev / "stdout_summary.out").write_text(stdout)
    if context is not None:
        (ev / "context.txt").write_text(context)
    if yaml_extract is not None:
        (ev / "yaml_extract.txt").write_text(yaml_extract)
    if retests is not None:
        (ev / "retests.txt").write_text(retests)
    return str(ev)


CAPS = {"NPROC": "128"}
TOPO = {"possible": list(range(128)), "online": list(range(128)),
        "offline": [], "isolated": [],
        "cpus": {str(c): {"online": True, "package": 36, "core_id": c,
                          "cluster_id": 138 + (c // 4) * 516, "die_id": -1}
                 for c in range(128)},
        "nodes": {"0": {"cpulist": "0-63", "has_memory": True},
                  "1": {"cpulist": "64-127", "has_memory": True}}}


# ---------------- bash 助手（隔离数据根——绝不碰真实根）----------------

def bash_common(root, body, timeout=30):
    return subprocess.run(
        ["bash", "-c",
         f"export SDC_EXCITE_REPRODUCE_DIR={shlex.quote(str(root))}\n"
         f"source '{COMMON}'\n{body}"],
        capture_output=True, text=True, timeout=timeout)


# ---------------------------------------------------------------------------
# 产出端：enqueue_repro（sdc_common.sh，抽函数供测试——M1b T4 模式）

def test_enqueue_repro_writes_queue_json(tmp_path):
    root = tmp_path / "root"
    ev = tmp_path / "ev"
    r = bash_common(root, f'enqueue_repro "{ev}" zstd19 AES:deadbeef')
    assert r.returncode == 0, r.stdout + r.stderr
    q = root / "spool" / "repro_queue"
    files = list(q.glob("*.json"))
    assert len(files) == 1, "队列文件未落位 spool/repro_queue/"
    assert files[0].stem.isdigit()            # epoch-ns 命名（消费端 ts 序依据）
    rec = json.loads(files[0].read_text())
    assert rec["event_dir"] == str(ev)
    assert rec["test"] == "zstd19" and rec["seed"] == "AES:deadbeef"
    assert "T" in rec["queued_at"]            # date -Is ISO 时间戳


def test_enqueue_repro_defaults(tmp_path):
    # test/seed 缺省 → "?" / "none"（与 handle_failure 台账行口径一致）
    root = tmp_path / "root"
    ev = tmp_path / "ev"
    r = bash_common(root, f'enqueue_repro "{ev}"')
    assert r.returncode == 0, r.stdout + r.stderr
    rec = json.loads(next((root / "spool" / "repro_queue").glob("*.json")).read_text())
    assert rec["test"] == "?" and rec["seed"] == "none"


def test_enqueue_repro_wired_after_ledger_line():
    # 驱动接线：handle_failure 台账行后必须落队列（M2 enqueue_reproduction 落点）
    src = open(DRIVER).read()
    i_hf = src.index("handle_failure() {")
    i_ledger = src.index('>> "$EVENTS_DIR/ledger.csv"', i_hf)
    i_call = src.index('enqueue_repro "$evdir"', i_hf)
    i_alert = src.index('alert "SDC/崩溃事件', i_hf)
    assert i_ledger < i_call < i_alert, "enqueue_repro 必须在台账行后、alert 前调用"


def test_verify_request_polled_in_main_loop():
    # 驱动接线：主循环每轮查 cmd/verify.request（M2 T5 移交的消费者）
    lines = open(DRIVER).read().splitlines()
    main = next(i for i, l in enumerate(lines) if l.startswith("while :; do"))
    calls = [i for i, l in enumerate(lines) if "consume_verify_request" in l
             and not l.lstrip().startswith("#")]
    assert calls, "驱动未调用 consume_verify_request"
    assert all(i > main for i in calls), "verify.request 消费必须在主循环内"


def test_consume_verify_request_semantics(tmp_path):
    # 最小消费者语义（隔离 cmd/）：alert 记台账 + touch snapshot.request
    # + mv .done 终态化（monitor governor.done.$(date +%s) 同例）；幂等。
    root = tmp_path / "root"
    cmd = root / "cmd"
    cmd.mkdir(parents=True)
    (cmd / "verify.request").write_text(
        '{"rule": "interlock_black", "actions": ["verify_only"]}')
    r = bash_common(root, "ensure_dirs && consume_verify_request")
    assert r.returncode == 0, r.stdout + r.stderr
    assert not (cmd / "verify.request").exists(), "请求未消费"
    assert (cmd / "snapshot.request").exists(), "未触发 root 快照通道"
    done = list(cmd.glob("verify.done.*"))
    assert len(done) == 1, "未按 monitor 惯例终态化为 verify.done.<epoch>"
    alerts = (root / "monitor" / "alerts.log").read_text()
    assert "interlock verify 触发（M2 controller）" in alerts
    # 幂等：无请求时再跑不产生新文件
    before = sorted(p.name for p in cmd.iterdir())
    r2 = bash_common(root, "consume_verify_request")
    assert r2.returncode == 0, r2.stdout + r2.stderr
    assert sorted(p.name for p in cmd.iterdir()) == before


# ---------------------------------------------------------------------------
# 消费端：consume_queue / --from-queue（隔离数据根 + 全 mock）

def make_root(tmp_path):
    root = tmp_path / "dataroot"
    (root / "spool" / "repro_queue").mkdir(parents=True)
    (root / "capabilities.env").write_text("NPROC=128\n")
    return root


def enqueue(root, event_dir, name, test="zstd19", seed=SEED, text=None):
    p = root / "spool" / "repro_queue" / f"{name}.json"
    p.write_text(text if text is not None else json.dumps(
        {"event_dir": str(event_dir), "test": test, "seed": seed,
         "queued_at": "2026-09-26T12:00:00+08:00"}))
    return p


@pytest.fixture
def fake_bin(tmp_path, monkeypatch):
    p = tmp_path / "bin" / "sdcshield"
    p.parent.mkdir()
    p.write_bytes(b"#!/bin/sh\n# fake sdcshield - queue consumer test fixture\n")
    monkeypatch.setenv("SDC_BIN", str(p))
    return p


@pytest.fixture(autouse=True)
def no_real_load(monkeypatch):
    """红线：健康核对照绝真跑——一律 mock rc=0（需要断言调用参数的用例再覆写）。"""
    monkeypatch.setattr(sr, "_control_run",
                        lambda *a, **k: {"rc": 0, "fail_count": 0})


def test_consume_queue_processed_full_path(tmp_path, fake_bin, monkeypatch):
    # gate 过 → capsule（gate 结果注入 manifest）+ 3 次概率复测（受界截断
    # 100s/trial）→ done 记录含 capsule 路径/k/n/ci；队列文件移除
    root = make_root(tmp_path)
    ev = make_event(tmp_path)
    q = enqueue(root, ev, "1000")
    calls = []

    def fake_run_once(self_, profile):
        calls.append(profile)
        return {"rc": 1, "fail_count": 2}

    monkeypatch.setattr(sr.Reproducer, "_run_once", fake_run_once)
    results = sr.consume_queue(str(root), capabilities=CAPS, topology_snapshot=TOPO)
    assert len(results) == 1
    rec = results[0]
    assert rec["status"] == "processed" and rec["event_id"] == "ev"
    assert rec["repro"]["k"] == 3 and rec["repro"]["n"] == 3
    assert rec["repro"]["ci"] == list(sr.wilson_ci(3, 3))
    assert len(calls) == 3 and calls[0]["limits"]["duration_s"] == \
        sr.QUEUE_EVENT_BUDGET_S // sr.QUEUE_TRIALS   # 120s 截断为 100s（≤5min 预算）
    assert not q.exists()
    done = root / "spool" / "repro_done" / "1000.json"
    d = json.loads(done.read_text())
    assert d["status"] == "processed" and d["repro"]["k"] == 3
    assert os.path.isdir(d["capsule_dir"])
    manifest = json.loads(open(os.path.join(d["capsule_dir"], "manifest.json"),
                               encoding="utf-8").read())
    assert manifest["gate"]["ok"] is True                 # gate 结果注入 capsule
    assert manifest["replay"]["duration_s"] == \
        sr.QUEUE_EVENT_BUDGET_S // sr.QUEUE_TRIALS        # manifest limits 兜底受界


def test_consume_queue_gate_rejected(tmp_path, fake_bin, monkeypatch):
    # 门禁1 拒（原始证据缺失）→ done 带 gate 拒因；不打包不复测
    root = make_root(tmp_path)
    ev = make_event(tmp_path, stdout=None)
    q = enqueue(root, ev, "1000")
    ran = []
    monkeypatch.setattr(sr.Reproducer, "_run_once",
                        lambda self_, profile: ran.append(1)
                        or {"rc": 0, "fail_count": 0})
    results = sr.consume_queue(str(root), capabilities=CAPS, topology_snapshot=TOPO)
    rec = results[0]
    assert rec["status"] == "gate_rejected"
    assert any("原始证据" in r for r in rec["gate"]["reasons"])
    assert rec["gate"]["ok"] is False
    assert not ran, "gate 拒后不得进入概率复测"
    caps = root / "spool" / "repro_capsules"
    assert not caps.is_dir() or not list(caps.glob("*")), \
        "gate 拒不得打包 capsule"
    assert not q.exists()
    d = json.loads((root / "spool" / "repro_done" / "1000.json").read_text())
    assert d["status"] == "gate_rejected"
    assert any("原始证据" in r for r in d["gate"]["reasons"])


def test_consume_queue_invalid_and_error_entries(tmp_path, fake_bin):
    # 毒丸 JSON（bash echo 转义残留形态：test 名内嵌引号）→ 容错解析，隔离
    # 入 done 不炸队列扫描；无种子事件 → from_event 响亮拒绝 → error 入案
    root = make_root(tmp_path)
    bad = enqueue(root, "/nonexistent", "1000",
                  text='{"event_dir": "/nonexistent", "test": "we"ird", "seed": "none"}')
    evd = tmp_path / "noseed"
    evd.mkdir()
    (evd / "stdout_summary.out").write_text(
        "- test: zstd19\n  result: fail\n")          # 无 fail 块 → 无种子
    (evd / "context.txt").write_text(
        "cmd: /bin/sdcshield -e zstd19\nrc: 1  fail_seed: none(usable=)"
        "  failed_test: zstd19\n")
    (evd / "yaml_extract.txt").write_text("")
    q2 = enqueue(root, str(evd), "2000")
    results = sr.consume_queue(str(root), capabilities=CAPS, topology_snapshot=TOPO)
    by_id = {r["id"]: r for r in results}
    assert by_id["1000"]["status"] == "invalid"
    assert "we" in by_id["1000"]["error"]
    assert by_id["2000"]["status"] == "error" and "种子" in by_id["2000"]["error"]
    assert not bad.exists() and not q2.exists()
    d = json.loads((root / "spool" / "repro_done" / "1000.json").read_text())
    assert d["status"] == "invalid" and d["raw"]          # 原文截留入案


def test_consume_queue_once_and_ts_order(tmp_path, fake_bin, monkeypatch):
    # ts 序以文件名为准（epoch-ns 数值序）而非入队调用序；--once 处理一个即退
    root = make_root(tmp_path)
    ev1 = make_event(tmp_path, name="ev1")
    ev2 = make_event(tmp_path, name="ev2")
    enqueue(root, ev1, "2000")
    enqueue(root, ev2, "1000")                     # 后入队但 ts 更早
    order = []
    monkeypatch.setattr(sr.Reproducer, "_run_once",
                        lambda self_, profile: order.append(profile["profile_id"])
                        or {"rc": 0, "fail_count": 0})
    results = sr.consume_queue(str(root), once=True, capabilities=CAPS,
                               topology_snapshot=TOPO)
    assert len(results) == 1 and results[0]["event_id"] == "ev2"
    # --once 只收敛事件数（一个即退），不削复测次数——brief 裁定 run_cycle(3)
    # 无条件（once 削成 1 次会把事件永久降级为 n=1 的 done 记录，且与
    # test_consume_queue_processed_full_path 的 len(calls)==3 自相矛盾）
    assert order == ["repro-ev2"] * sr.QUEUE_TRIALS
    left = list((root / "spool" / "repro_queue").glob("*.json"))
    assert [p.name for p in left] == ["2000.json"], "--once 后剩余应为 ts 较晚者"


def test_consume_queue_crash_recovery_done_exists(tmp_path, fake_bin,
                                                  monkeypatch):
    # done 已存在（上轮写完 done、unlink 队列前崩溃）→ 不重跑复测，只清队列；
    # 原 done 记录原样保留（复测是有界负载，不得无理由重跑）
    root = make_root(tmp_path)
    ev = make_event(tmp_path)
    q = enqueue(root, ev, "1000")
    done = root / "spool" / "repro_done"
    done.mkdir(parents=True)
    (done / "1000.json").write_text(json.dumps({"status": "processed", "id": "1000",
                                                "repro": {"k": 1, "n": 3}}))
    ran = []
    monkeypatch.setattr(sr.Reproducer, "_run_once",
                        lambda self_, profile: ran.append(1)
                        or {"rc": 0, "fail_count": 0})
    results = sr.consume_queue(str(root), capabilities=CAPS, topology_snapshot=TOPO)
    assert results[0]["status"] == "already_done"
    assert not ran and not q.exists()
    assert json.loads((done / "1000.json").read_text())["repro"]["k"] == 1


def test_cli_from_queue_empty_error_once(tmp_path, fake_bin):
    # CLI 端到端（安全路径）：空队列 rc=0；无种子事件 → done status=error +
    # rc=1（诚实信号）；--once 一次一个
    root = make_root(tmp_path)
    r = subprocess.run([sys.executable, str(REPRODUCER), "--from-queue",
                        "--data-root", str(root)],
                       capture_output=True, text=True, timeout=60)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "队列为空" in r.stdout
    evd = tmp_path / "noseed"
    evd.mkdir()
    (evd / "stdout_summary.out").write_text("- test: zstd19\n  result: fail\n")
    (evd / "context.txt").write_text(
        "cmd: /bin/sdcshield -e zstd19\nrc: 1  fail_seed: none(usable=)"
        "  failed_test: zstd19\n")
    (evd / "yaml_extract.txt").write_text("")
    enqueue(root, str(evd), "1000")
    enqueue(root, str(evd), "2000")
    r2 = subprocess.run([sys.executable, str(REPRODUCER), "--from-queue", "--once",
                         "--data-root", str(root)],
                        capture_output=True, text=True, timeout=60)
    assert r2.returncode == 1, "error 条目必须以 rc=1 诚实汇报"
    d = json.loads((root / "spool" / "repro_done" / "1000.json").read_text())
    assert d["status"] == "error" and "种子" in d["error"]
    assert [p.name for p in (root / "spool" / "repro_queue").glob("*.json")] \
        == ["2000.json"], "--once 只处理一个事件"

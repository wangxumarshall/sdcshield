"""M5 故障演练脚本族（v5 §17.2 验收基准全覆盖——清单 13 项，CE/UE 拆分
为 d04/d05 共 14 类）——pytest 可执行规格。

驱动 scripts/sdc-excite-reproduce/drills/ 演练本体（bash 子进程，与人工/CI
驱动同入口）：每类断言 ① 演练退出码 0 + PASS 行；② 证据目录形态
（drills/out/<类名>/root/ 下事件/账本/审计快照落位）。

安全边界（全程）：DRILL_OUT 指 pytest tmp_path——演练绝不写仓库内默认位、
绝不指向真实战役根（真实根形态守卫在 drill_lib.drill_init，此处直测）。
14 类全跑耗时较长（d07/d09 进程生命周期 + d14 热联锁跨周期 ~50s）——
pytest 默认只跑快速子集（d01-d03）+ 各类独立用例，全量跑由 drill_all.sh
本体承担（SDC_DRILLS_FULL=1 环境变量开启全量 pytest 路径）。
"""
import os, re, subprocess

DRILLS = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                      "..", "scripts", "sdc-excite-reproduce", "drills")
DRILL_ALL = os.path.join(DRILLS, "drill_all.sh")

# 快速子集：事件链纯 --once 工具调用（无进程等待）——CI/回归默认跑
FAST_SUBSET = ("d01_synthetic_mismatch", "d02_test_bug", "d03_spurious_fault")
# v5 §17.2 全 13 项（CE/UE 拆分 → 14 类；映射表见 drill_all.sh 头注释）
FULL_LIST = FAST_SUBSET + (
    "d04_ce", "d05_ue", "d06_panic", "d07_runner_hang", "d08_bmc_timeout",
    "d09_collector_crash", "d10_network_split", "d11_disk_full",
    "d12_clock_jump", "d14_thermal_overlimit", "d13_restore_fail",
)


def run_drill(name, out_dir, timeout=300):
    """跑单类演练（DRILL_OUT=隔离目录——绝不写仓库默认位/真实根）。"""
    return subprocess.run(
        ["bash", os.path.join(DRILLS, f"{name}.sh")],
        env=dict(os.environ, DRILL_OUT=str(out_dir)),
        capture_output=True, text=True, timeout=timeout)


def assert_drill_evidence(out_dir, name):
    """证据目录形态：drills/out/<类名>/root/ 隔离根落位（事件流 + spool）。"""
    root = out_dir / name / "root"
    assert (root / "spool").is_dir(), f"缺隔离根 spool/: {root}"
    assert (root / "events").is_dir(), f"缺隔离根 events/: {root}"
    return root


# ---------------------------------------------------------------------------
# d01-d03（快速子集——每类全过 + 证据形态）

def test_d01_synthetic_mismatch(tmp_path):
    r = run_drill("d01_synthetic_mismatch", tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "== d01_synthetic_mismatch: PASS ==" in r.stdout
    root = assert_drill_evidence(tmp_path, "d01_synthetic_mismatch")
    # 独立复核（详细断言在 drill 内）：RED 终态 + 队列行 + 固化窗口在案
    import json
    assert json.loads((root / "spool" / "controller_state.json")
                      .read_text())["state"] == "red"
    assert list((root / "spool" / "repro_queue").glob("*.json")), "repro_queue 无行"
    assert list((root / "events").glob("*/ring_window/monitor.csv")), "无固化窗口"


def test_d02_test_bug(tmp_path):
    r = run_drill("d02_test_bug", tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "== d02_test_bug: PASS ==" in r.stdout
    root = assert_drill_evidence(tmp_path, "d02_test_bug")
    import json
    # 独立复核：gate 拒因入案（竞态定案）+ 毒丸 invalid 单列
    gate = (root / "gate.out").read_text()
    assert "竞态审计定案" in gate and "门禁5 拒" in gate
    done = json.loads(next((root / "spool" / "repro_done").glob("*.json"))
                      .read_text())
    assert done["status"] == "invalid" and "原文截留" in done["error"]


def test_d03_spurious_fault(tmp_path):
    r = run_drill("d03_spurious_fault", tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "== d03_spurious_fault: PASS ==" in r.stdout
    root = assert_drill_evidence(tmp_path, "d03_spurious_fault")
    import json
    ev = json.loads((root / "spool" / "events.jsonl").read_text().splitlines()[0])
    assert ev["event_type"] == "ras_keyword" and ev["severity"] == "yellow"
    assert "SPURIOUS" in ev["source_line"]


def test_d14_thermal_overlimit(tmp_path):
    """温度越限（v5 §17.2 清单项——热联锁 §8.6 唯一 kill 真实负载路径）。

    独立复核（详细断言在 drill 内）：PAUSE 96 行/热升级行/RESUME 行三段
    在案 + 牺牲进程已死 + PAUSE 标志随 RESUME 移除。
    """
    r = run_drill("d14_thermal_overlimit", tmp_path, timeout=300)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "== d14_thermal_overlimit: PASS ==" in r.stdout
    root = assert_drill_evidence(tmp_path, "d14_thermal_overlimit")
    alerts = (root / "monitor" / "alerts.log").read_text()
    assert "ALERT PAUSE: CPU 96C >= 95C" in alerts           # 单热采样 PAUSE
    assert "ALERT 热升级：97C（连续2个热采样）→ KILL 全部负载" in alerts  # 升级 KILL
    assert "ALERT RESUME: thermal 已恢复" in alerts          # 95/90 滞回
    assert not (root / "PAUSE").exists()                     # RESUME 后标志移除


# ---------------------------------------------------------------------------
# 守卫：DRILL_OUT 形似真实战役根 → 拒绝（绝不缺省/误指到真实根）

def test_drill_refuses_real_campaign_root_shape(tmp_path):
    used = tmp_path / "real-root-shape"
    (used / "spool").mkdir(parents=True)
    (used / "driver.log").write_text("真实根 as-built 形态\n")
    r = run_drill("d01_synthetic_mismatch", used)
    assert r.returncode != 0
    assert "真实战役根" in (r.stdout + r.stderr)
    # 拒绝时不得在疑似真实根内建演练目录
    assert not (used / "d01_synthetic_mismatch").exists()


# ---------------------------------------------------------------------------
# drill_all.sh 编排器：子集（快速）+ 汇总表格式 + 全量门控

def run_drill_all(only=None, out_dir=None, timeout=1200):
    """跑编排器（--out 隔离目录；--only 子集——pytest 快速路径）。"""
    cmd = ["bash", DRILL_ALL, "--out", str(out_dir)]
    if only:
        cmd += ["--only", ",".join(only)]
    return subprocess.run(cmd, capture_output=True, text=True, timeout=timeout)


def test_drill_all_fast_subset_passes_with_summary(tmp_path):
    """d01-d03 子集全过 + 汇总表格式（14 类全跑耗时较长——留给 drill_all 本体）。"""
    r = run_drill_all(only=FAST_SUBSET, out_dir=tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "== M5 故障演练汇总（v5 §17.2 全 13 项（含温度越限）+ restore，3/3 PASS）==" in r.stdout
    summary = (tmp_path / "summary.txt").read_text()
    lines = summary.strip().splitlines()
    assert lines[0].startswith("== M5 故障演练汇总")
    assert len(lines) == 1 + len(FAST_SUBSET)
    for name, line in zip(FAST_SUBSET, lines[1:]):
        assert re.fullmatch(rf"{name}\s+PASS", line), f"汇总行格式非预期: {line!r}"
    assert (tmp_path / "drill_all.log").is_file()   # 全量日志在案
    for name in FAST_SUBSET:                        # 各类证据目录落位
        assert (tmp_path / name / "root" / "spool").is_dir()


def test_drill_all_only_filter_selects_subset(tmp_path):
    """--only d01 → 恰跑 1 类（前缀/全名过滤——pytest/CI 子集控制入口）。"""
    r = run_drill_all(only=["d01"], out_dir=tmp_path)
    assert r.returncode == 0, r.stdout + r.stderr
    summary = (tmp_path / "summary.txt").read_text().strip().splitlines()
    assert len(summary) == 2
    assert re.fullmatch(r"d01_synthetic_mismatch\s+PASS", summary[1])
    assert not (tmp_path / "d02_test_bug").exists()  # 未选类不跑不建目录


def test_drill_all_full_run_14_classes():
    """14 类全链验收（v5 §17.2 全 13 项含温度越限 + restore——退出标准核心验收器）。

    全跑含真实进程生命周期等待（d07 超时兜底/d09 collector 重启/d12 锚点
    2s 间隔/d14 热联锁跨周期 ~50s），总时长分钟级——pytest 默认不跑，留给
    drill_all.sh 本体与 M5 收官（Task 6）人工/CI 驱动；SDC_DRILLS_FULL=1
    开启本路径。
    """
    if os.environ.get("SDC_DRILLS_FULL") != "1":
        import pytest
        pytest.skip("全量 14 类跑留给 drill_all.sh 本体（SDC_DRILLS_FULL=1 开启）")
    import tempfile
    with tempfile.TemporaryDirectory(prefix="drill-all-") as out:
        r = run_drill_all(out_dir=out, timeout=1800)
        assert r.returncode == 0, r.stdout + r.stderr
        summary = open(os.path.join(out, "summary.txt"), encoding="utf-8").read()
        lines = summary.strip().splitlines()
        assert lines[0] == \
            "== M5 故障演练汇总（v5 §17.2 全 13 项（含温度越限）+ restore，14/14 PASS）=="
        assert len(lines) == 15
        assert all(re.fullmatch(r"d\d{2}\S*\s+PASS", l) for l in lines[1:])
        assert re.search(r"d14_thermal_overlimit\s+PASS", summary)  # 温度越限在场

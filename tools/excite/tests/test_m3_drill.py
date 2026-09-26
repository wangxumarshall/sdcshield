"""M3 退出标准演练（Task 6）——m3_drill.sh 可执行规格 + 守卫 + CORE179 语义。

退出标准（v5 §14.1 M3 行）：
  ① 人工/架构态注入事件能自动到 R3 —— 演练链：合成 mismatch 事件目录 →
    enqueue_repro（生产产出端，驱动 handle_failure 同款）→ sdc_reproducer
    --from-queue --once（消费端全链）→ capsule 完整 + run.sh --check-only
    同机可重放；
  ② 单核不复现条件不会被错误删除 —— CORE179：合成 run_trial（复现 = victim
    含锚点核 ∧ aggressor 含 cache ∧ 有 aggressor 核）→ probabilistic_ddmin
    小预算 → min_set 收缩到 victim={c} 且保留必要 aggressor。

安全边界（战役+9 服务运行中）：演练只用隔离数据根（m3_drill.sh 无参/空参/
已用根拒绝守卫——绝不缺省到真实根）；SDC_BIN=隔离根内 fake bin——健康核
对照与概率复测的发射路径全走假二进制（--cpuset 含 victim 锚点核 → rc=1 +
fail 行，否则 rc=0），绝不真跑 sdcshield；capsule 门禁预检不 mock——平台
快照=实机 sysfs 只读探针，同机 --check-only 天然一致。
"""
import json, os, subprocess, sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_reducer as red

DRILL = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                     "..", "scripts", "sdc-excite-reproduce", "m3_drill.sh")


# ---------------------------------------------------------------------------
# 退出标准①：全链 + capsule 完整 + 可重放（m3_drill.sh 本体——可执行规格）

def test_m3_drill_full_chain_capsule_replayable(tmp_path):
    """注入事件自动到 R3：enqueue_repro → --from-queue --once → done
    processed 复测 k=3/3 + capsule 完整 + run.sh --check-only 同机通过。"""
    root = tmp_path / "drill-root"
    r = subprocess.run(["bash", DRILL, str(root)],
                       capture_output=True, text=True, timeout=300)
    assert r.returncode == 0, r.stdout + r.stderr
    assert "M3 退出标准演练 PASS" in r.stdout
    # 演练产物独立复核（详细断言在 drill 内——此处复核退出标准①证据本体）
    done = root / "spool" / "repro_done"
    rec = json.loads(next(done.glob("*.json")).read_text())
    assert rec["status"] == "processed"
    assert rec["repro"]["k"] == 3 and rec["repro"]["n"] == 3
    assert rec["gate"]["ok"] is True
    assert not list((root / "spool" / "repro_queue").glob("*"))  # 队列清空
    cap = root / "spool" / "repro_capsules" / rec["event_id"]
    man = json.loads((cap / "manifest.json").read_text())
    assert man["gate"]["ok"] is True                      # 门禁结果注入 manifest
    assert man["replay"]["duration_s"] == 100             # ≤5min 预算 per-trial 截断
    # 可重放（退出标准①核心证据）：同机 --check-only 真实通过（不 mock）
    chk = subprocess.run(["bash", str(cap / "scripts" / "run.sh"), "--check-only"],
                         capture_output=True, text=True, timeout=60,
                         env=dict(os.environ, SDC_BIN=str(root / "fake-sdcshield")))
    assert chk.returncode == 0, chk.stdout + chk.stderr
    assert "预检通过" in (chk.stdout + chk.stderr)


# ---------------------------------------------------------------------------
# 守卫：无参/空参/已用根拒绝（m2_drill 同款——绝不缺省/误对真实根）

def test_m3_drill_guards_reject_missing_or_used_root(tmp_path):
    r = subprocess.run(["bash", DRILL], capture_output=True, text=True, timeout=30)
    assert r.returncode != 0 and "隔离数据根" in (r.stdout + r.stderr)
    r = subprocess.run(["bash", DRILL, ""], capture_output=True, text=True, timeout=30)
    assert r.returncode != 0 and "不能为空" in (r.stdout + r.stderr)
    used = tmp_path / "used-root"           # 已含 spool/ = 真实根/重跑污染形态
    (used / "spool").mkdir(parents=True)
    r = subprocess.run(["bash", DRILL, str(used)],
                       capture_output=True, text=True, timeout=30)
    assert r.returncode != 0 and "spool" in r.stderr


# ---------------------------------------------------------------------------
# 退出标准②：CORE179 语义（五因素全尺度——单测 test_sdc_reducer 覆盖单因素，
# 此处为演练 step-D 同款全尺度断言：双 aggressor 因素均必要 + 最小场收敛）

def test_m3_core179_min_set_keeps_necessary_aggressors():
    """单核不复现条件不被误删：复现需 victim 含 96 ∧ aggressor 含 cache ∧
    有 aggressor 核——aggressor 任一维度删空 → 复现崩塌 REJECT → 回退 +
    necessary_aggressors 标记。输出"最小 victim + 必要 aggressor 场"。"""
    SEED = "AES:core179" + "0" * 53
    def repro(cfg):
        return (96 in cfg["victim_cpus"]
                and "cache" in cfg["aggressor_families"]
                and cfg["aggressor_cpus"]
                and "zstd19" in cfg["tests"]
                and SEED in cfg["seeds"])

    factors = red.Factors(victim_cpus=[96, 97], aggressor_cpus=[120, 121],
                          aggressor_families=["cache", "integer"],
                          tests=["zstd19", "memcpy_rewr"], seeds=[SEED, "LCG:0"])
    out = red.probabilistic_ddmin(
        factors, repro,
        budget={"min_trials": 2, "max_trials": 40, "target_repro_rate": 0.5},
        rng_seed=179)
    assert out["status"] == "minimized"
    ms = out["min_set"]
    assert ms["victim_cpus"] == [96]                      # 最小 victim={c}
    assert ms["aggressor_families"] == ["cache"]          # 必要 aggressor 保留
    assert ms["aggressor_cpus"]                           # 绝不空 aggressor
    assert ms["tests"] == ["zstd19"] and ms["seeds"] == [SEED]
    assert set(out["necessary_aggressors"]) == \
        {"aggressor_cpus", "aggressor_families"}          # 双因素必要标记
    # 删空探测在树有 REJECT 证据且未被采纳（崩塌回退，非静默跳过）
    for fac in ("aggressor_cpus", "aggressor_families"):
        empt = [t for t in out["trials"]
                if t["factor"] == fac and t["config"][fac] == []]
        assert empt and all(t["verdict"] == "REJECT" and t["applied"] is not True
                            for t in empt)
    # 单核（无 aggressor）形态的试验从未被采纳——CORE179 退出标准②本体
    assert all(t["applied"] is not True for t in out["trials"]
               if not t["config"]["aggressor_cpus"]
               or not t["config"]["aggressor_families"])

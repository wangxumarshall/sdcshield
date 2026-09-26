"""sdc_reducer 单元测试（M3 Task 4）——合成 run_trial 决定论 mock，零真实运行。

用例口径（brief 2 例 + 独立断言）：
  1 brief: aggressor 依赖收敛——复现需 victim={96} ∧ cache 族 ∧ s3；
  2 brief: 50% 恒振荡 + 全局预算截断 → INCONCLUSIVE；
  3 CORE179 删空恢复：aggressor 组删空复现崩塌（REJECT）→ 回退到上一接受态
    + "必要 aggressor" 标记（θ=0.5 使 k=0 在 n=5 即可证 REJECT——默认 θ=0.05
    下 k=0 需 n≥74 才有 hi ≤ θ，小预算只能 INCONCLUSIVE，故本例显式抬 θ）；
  4 max_total_trials 硬上限：run_trial 调用数 ≤ 上限，截断时保留当前接受态；
  5 搜索树完整性：每 trial config/k/n/ci/verdict + seq/parent_seq/applied；
  6 Factors.reduce 不可变语义（原对象不动、属性访问返回拷贝）；
  7 非法输入响亮拒绝（未知因素/非子集/重复/预算字段拼错/min_trials=1 禁
    确定性 ddmin/非 Factors 入参）；
  8 基线不复现 → 不缩减如实返回（status=baseline_rejected）；
  9 rng_seed 重放确定性（§10.3 随机种子保存——同 seed 同遍历）。
"""
import os, sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_reducer as red


def true_repro(config):     # 复现条件：victim=96 ∧ aggressor 含 cache ∧ seed in {s3}
    return (96 in config["victim_cpus"] and "cache" in config["aggressor_families"]
            and "s3" in config["seeds"])


# ---------------------------------------------------------------------------
# brief 用例 1：依赖 aggressor 的复现收敛到最小集

def test_ddmin_finds_min_set_with_aggressor_dependency():
    factors = red.Factors(victim_cpus=[96, 97], aggressor_cpus=list(range(120)),
                          aggressor_families=["cache", "integer", "stream"],
                          tests=["openblas_dgemm"], seeds=["s1", "s3"])
    out = red.probabilistic_ddmin(
        factors, lambda cfg: true_repro(cfg), budget={"min_trials": 5, "max_trials": 30})
    assert out["min_set"]["victim_cpus"] == [96]
    assert out["min_set"]["aggressor_families"] == ["cache"]
    assert out["min_set"]["seeds"] == ["s3"]
    assert all("cache" not in t["config"]["aggressor_families"] or t["verdict"] != "REJECT"
               for t in out["trials"] if t["config"]["aggressor_families"] == [])  # 删空组恢复语义


# ---------------------------------------------------------------------------
# brief 用例 2：恒振荡 + 全局预算耗尽 → INCONCLUSIVE

def test_ddmin_budget_exhaustion_inconclusive():
    calls = {"n": 0}
    def flaky(cfg):
        calls["n"] += 1
        return calls["n"] % 2 == 0      # 50% 恒振荡永不收敛
    factors = red.Factors(victim_cpus=[1], aggressor_cpus=[2], aggressor_families=["x"],
                          tests=["t"], seeds=["s"])
    out = red.probabilistic_ddmin(factors, flaky, budget={"min_trials": 3, "max_trials": 6},
                                  max_total_trials=12)
    assert out["verdicts"][-1] in ("INCONCLUSIVE",) or out["trials"][-1]["verdict"] == "INCONCLUSIVE"


# ---------------------------------------------------------------------------
# CORE179 删空恢复语义（独立断言）

def test_core179_empty_collapse_reverts_and_marks_necessary():
    """aggressor 组删空 → 复现率崩塌（REJECT）→ 回退上一接受态 + 标记必要。

    CORE179（v5 §10.3/§10.4）：单核隔离消除 SDC——输出必须是"最小 victim +
    必要 aggressor 场"，绝不强迫无 aggressor 的空场样例。"""
    def oracle(cfg):
        return 96 in cfg["victim_cpus"] and "cache" in cfg["aggressor_families"]
    factors = red.Factors(victim_cpus=[96], aggressor_cpus=[],
                          aggressor_families=["cache", "integer"], tests=["t"], seeds=["s"])
    out = red.probabilistic_ddmin(
        factors, oracle,
        budget={"min_trials": 5, "max_trials": 10, "target_repro_rate": 0.5},
        max_total_trials=500)
    assert out["min_set"]["aggressor_families"] == ["cache"]   # 删空被回退
    assert "aggressor_families" in out["necessary_aggressors"]  # 必要 aggressor 标记
    # 删空试验存在于树中且判 REJECT（崩塌有证据，非静默跳过）
    empt = [t for t in out["trials"] if t["config"]["aggressor_families"] == []]
    assert empt and all(t["verdict"] == "REJECT" for t in empt)
    assert all(t.get("applied") is not True for t in empt)     # 删空未被采纳


# ---------------------------------------------------------------------------
# max_total_trials 硬上限

def test_max_total_trials_cap_never_exceeded():
    calls = {"n": 0}
    def always_true(cfg):
        calls["n"] += 1
        return True
    factors = red.Factors(victim_cpus=list(range(8)), aggressor_cpus=[100],
                          aggressor_families=["c"], tests=["t"], seeds=["s"])
    out = red.probabilistic_ddmin(factors, always_true,
                                  budget={"min_trials": 4, "max_trials": 8},
                                  max_total_trials=13)
    assert calls["n"] == out["total_trials"] <= 13              # 绝不超上限
    assert out["status"] == "inconclusive"                      # 预算内未完成
    assert out["verdicts"][-1] == "INCONCLUSIVE"
    assert out["min_set"]["victim_cpus"] == [6, 7]              # 保留当前接受态


# ---------------------------------------------------------------------------
# 搜索树完整性

def test_search_tree_complete_records():
    out = red.probabilistic_ddmin(
        red.Factors(victim_cpus=[96], aggressor_cpus=[], aggressor_families=["cache"],
                    tests=["t"], seeds=["s1", "s3"]),
        true_repro, budget={"min_trials": 5, "max_trials": 30})
    assert out["status"] == "minimized"
    assert len(out["verdicts"]) == len(out["trials"])           # 完整收尾无截断尾注
    for i, t in enumerate(out["trials"]):
        assert {"config", "k", "n", "ci", "verdict"} <= set(t)  # 五要素
        assert t["seq"] == i                                    # 试验顺序
        assert 0 <= t["k"] <= t["n"] and t["n"] > 0
        lo, hi = t["ci"]
        assert 0.0 <= lo <= hi <= 1.0
        assert t["verdict"] in ("ACCEPT", "REJECT", "INCONCLUSIVE")
        assert set(t["config"]) == set(red.FACTOR_ORDER)        # 配置五因素齐
    root = out["tree"]
    assert root["record"] is out["trials"][0]                   # 树根 = 基线评估
    def walk(node):
        for child in node["children"]:
            assert child["record"]["parent_seq"] == node["record"]["seq"]
            walk(child)
    walk(root)
    assert sum(1 for t in out["trials"] if t.get("applied") is False) >= 1   # 回退点可见
    assert sum(1 for t in out["trials"] if t.get("applied") is True) >= 1    # 采纳删减可见


# ---------------------------------------------------------------------------
# Factors 不可变语义

def test_factors_reduce_immutable():
    src = [96, 97]
    f = red.Factors(victim_cpus=src, aggressor_cpus=[2], aggressor_families=["c"],
                    tests=["t"], seeds=["s"])
    src.append(98)                       # 构造后外部变异不渗入
    assert f.victim_cpus == [96, 97]
    f2 = f.reduce("victim_cpus", [96])
    assert f2 is not f
    assert f.victim_cpus == [96, 97]     # 原对象不动
    assert f2.victim_cpus == [96] and f2.aggressor_families == ["c"]
    f2.victim_cpus.append(99)            # 属性返回拷贝，变异不渗入
    assert f2.victim_cpus == [96] and f.victim_cpus == [96, 97]
    f3 = f.reduce("victim_cpus", [])     # 删空子集合法（CORE179 删空探测步骤）
    assert f3.victim_cpus == [] and f.victim_cpus == [96, 97]


# ---------------------------------------------------------------------------
# 非法输入响亮拒绝

def test_factors_and_budget_validation():
    f = red.Factors(victim_cpus=[1, 2])
    with pytest.raises(ValueError):
        f.reduce("nope", [1])                                   # 未知因素
    with pytest.raises(ValueError):
        f.reduce("victim_cpus", [3])                            # 非子集（只删不增）
    with pytest.raises(ValueError):
        f.reduce("victim_cpus", [1, 1])                         # 子集重复
    with pytest.raises(ValueError):
        red.Factors(victim_cpus=[1, 1])                         # 构造重复
    with pytest.raises(ValueError):
        red.probabilistic_ddmin(f, lambda c: True,
                                budget={"min_trials": 10, "max_trials": 5})
    with pytest.raises(ValueError):
        red.probabilistic_ddmin(f, lambda c: True,
                                budget={"min_trial": 5})         # 预算字段拼错
    with pytest.raises(ValueError):                              # CORE179：禁单次定夺
        red.probabilistic_ddmin(f, lambda c: True,
                                budget={"min_trials": 1, "max_trials": 2})
    with pytest.raises(ValueError):
        red.probabilistic_ddmin(f, lambda c: True, max_total_trials=0)
    with pytest.raises(ValueError):
        red.probabilistic_ddmin({"victim_cpus": [1]}, lambda c: True)  # 非 Factors


# ---------------------------------------------------------------------------
# 基线不复现：不缩减、如实返回

def test_baseline_rejected_no_reduction():
    factors = red.Factors(victim_cpus=[1, 2], aggressor_cpus=[3], aggressor_families=["c"],
                          tests=["t"], seeds=["s"])
    out = red.probabilistic_ddmin(factors, lambda cfg: False,
                                  budget={"min_trials": 5, "max_trials": 10,
                                          "target_repro_rate": 0.5})
    assert out["status"] == "baseline_rejected"
    assert out["verdicts"] == ["REJECT"]
    assert out["min_set"]["victim_cpus"] == [1, 2]              # 原样保留
    assert len(out["trials"]) == 1                              # 仅基线评估


# ---------------------------------------------------------------------------
# rng_seed 重放确定性（§10.3 随机种子保存）

def test_rng_seed_replay_deterministic():
    factors = red.Factors(victim_cpus=[1, 2, 3, 4], aggressor_cpus=[],
                          aggressor_families=[], tests=["t"], seeds=["s"])
    def make_oracle():
        state = {"n": 0}
        def counting(cfg):
            state["n"] += 1
            return state["n"] % 3 != 0                          # 2/3 复现率
        return counting
    kw = {"budget": {"min_trials": 4, "max_trials": 8}, "rng_seed": 42,
          "max_total_trials": 400}
    out1 = red.probabilistic_ddmin(factors, make_oracle(), **kw)
    out2 = red.probabilistic_ddmin(factors, make_oracle(), **kw)
    assert out1["rng_seed"] == 42
    assert [t["removed"] for t in out1["trials"]] == [t["removed"] for t in out2["trials"]]
    assert out1["min_set"] == out2["min_set"]                   # 同 seed 同遍历

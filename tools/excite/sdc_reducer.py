"""sdc_reducer.py — 概率 ddmin reducer + 搜索树（v5 §10.3-10.4，M3 Task 4）。

CORE179 硬规则（v5 §10.3）：单核隔离会消除 SDC——**禁确定性 ddmin**。本模块
全概率口径：每个候选配置序贯试验 k/n + Wilson 95% 区间（复用
sdc_reproducer.wilson_ci 单一数学权威）→ ACCEPT / REJECT / INCONCLUSIVE，
绝不单次试验定夺（min_trials ≥ 2 结构性强制）。aggressor 因素组删空复现
崩塌（REJECT）→ 回退到上一接受态并标记"必要 aggressor"——输出
"最小 victim + 必要 aggressor 场"，不强迫单线程样例（§10.4）。

判据（§10.3，θ=target_repro_rate，m=noninferiority_margin；两条实现裁定）：
  ACCEPT        区间全在阈值上侧（lo ≥ θ），或非劣于父配置（lo ≥ 父点估计−m）；
  REJECT        区间全在阈值下侧（hi ≤ θ）——足够证据低于阈值；
  INCONCLUSIVE  加样至 max_trials 仍跨阈值 / 全局预算截断——保留该因素。
  裁定① REJECT 优先于非劣：可证明低于阈值必为崩塌——绝对下限不被弱父代
  绕过；裁定② 非劣需候选 k ≥ 1：零复现无"删减后仍生存"证据——防弱父代
  级联淘空（每步降幅 ≤ m 但累计归零，min_set 空心化）。

缩减顺序（§10.4）：基线冻结（先证明原配置仍复现）→ 逐因素经典分组 ddmin
（连续二分删试、删成则粗化重试、皆败则细化）→ 每因素收尾删空探测（CORE179）。
INCONCLUSIVE 一律保留因素——最小性存疑优于误删复现条件。

搜索树：trial_record 五要素（config/k/n/ci/verdict）+ seq 试验顺序 /
parent_seq 父节点 / applied 删减采纳标记 / note 截断注记——完整回退点
（§10.3 要求：树、随机种子、试验顺序、回退点），trials 列表 jsonl 逐行
直落盘。

注入点：run_trial(config) -> bool（测试 mock 决定论；真实口径
Reproducer.run_cycle(profile, 1)["k"] ≥ 1，T6 m3_drill 接线）。
stdlib only；库模块无 CLI（编排属 T6）。
"""
import os, random, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdc_reproducer import wilson_ci       # T3——Wilson 区间单一数学权威

FACTOR_ORDER = ("victim_cpus", "aggressor_cpus", "aggressor_families", "tests", "seeds")
AGGRESSOR_FACTORS = ("aggressor_cpus", "aggressor_families")

#: 序贯试验预算（v5 §10.3：默认值须由历史事件频率校准——此处给战役默认）
DEFAULT_BUDGET = {
    "min_trials": 20,               # 每候选首批试验数（≥2：禁单次定夺）
    "max_trials": 200,              # 每候选加样上限（跨阈值步进 +min_trials）
    "target_repro_rate": 0.05,      # θ：复现率阈值（ACCEPT 下界 / REJECT 上界）
    "noninferiority_margin": 0.10,  # m：非劣容忍度（相对父配置点估计）
}


def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


class _OutOfBudget(Exception):
    """全局预算（max_total_trials）耗尽且候选零试验——无记录终止信号。"""


# ---------------------------------------------------------------------------
# 因素模型（不可变值对象）

class Factors:
    """复现条件因素模型（v5 §10.4 R = f(...) 的离散化）——五因素不可变值对象。

    reduce(factor, subset) 返回**新** Factors（仅该因素替换为 subset），原对象
    不动；subset 须为当前值集子集（ddmin 只删不增），允许空集（CORE179 删空
    探测）。属性访问返回拷贝列表——外部变异不渗入。"""

    def __init__(self, victim_cpus=(), aggressor_cpus=(), aggressor_families=(),
                 tests=(), seeds=()):
        vals = {}
        for name, seq in (("victim_cpus", victim_cpus),
                          ("aggressor_cpus", aggressor_cpus),
                          ("aggressor_families", aggressor_families),
                          ("tests", tests), ("seeds", seeds)):
            items = list(seq)
            _require(len(set(items)) == len(items),
                     f"因素 {name} 含重复值: {items!r}")
            vals[name] = items
        self._values = vals

    @property
    def victim_cpus(self):
        return list(self._values["victim_cpus"])

    @property
    def aggressor_cpus(self):
        return list(self._values["aggressor_cpus"])

    @property
    def aggressor_families(self):
        return list(self._values["aggressor_families"])

    @property
    def tests(self):
        return list(self._values["tests"])

    @property
    def seeds(self):
        return list(self._values["seeds"])

    def get(self, factor):
        """因素当前值集（拷贝）。"""
        _require(factor in FACTOR_ORDER,
                 f"未知因素: {factor!r}（合法: {FACTOR_ORDER}）")
        return list(self._values[factor])

    def reduce(self, factor, subset):
        """返回删减后的新 Factors（本对象不动）。subset ⊆ 当前值集，可为空。"""
        _require(factor in FACTOR_ORDER,
                 f"未知因素: {factor!r}（合法: {FACTOR_ORDER}）")
        cur = self._values[factor]
        items = list(subset)
        _require(len(set(items)) == len(items),
                 f"因素 {factor} 子集含重复值: {items!r}")
        _require(set(items) <= set(cur),
                 f"reduce 只删不增: {items!r} ⊄ {factor} 当前值 {cur!r}")
        merged = {k: list(v) for k, v in self._values.items()}
        merged[factor] = items
        return Factors(**merged)

    def as_config(self):
        """→ 传给 run_trial 的配置 dict（五因素各一份新列表）。"""
        return {k: list(v) for k, v in self._values.items()}

    def __repr__(self):
        inner = ", ".join(f"{k}={v!r}" for k, v in self._values.items())
        return f"Factors({inner})"


# ---------------------------------------------------------------------------
# 搜索树节点

def trial_record(config, k, n, ci, verdict, factor=None, removed=None, seq=None,
                 parent_seq=None, applied=None, note=None):
    """搜索树节点（jsonl 落盘一行）。

    五要素 config/k/n/ci/verdict + 树结构元数据：seq 试验顺序、factor/removed
    本轮删减（None=基线）、parent_seq 父节点、applied 删减是否采纳
    （True=保留删减 / False=回退 / None=基线）、note 截断注记
    （max_trials_undecided / global_budget_cut）。"""
    return {
        "seq": seq,
        "config": {key: list(val) for key, val in config.items()},
        "factor": factor,
        "removed": list(removed) if removed is not None else None,
        "k": k, "n": n,
        "ci": (ci[0], ci[1]),
        "verdict": verdict,
        "parent_seq": parent_seq,
        "applied": applied,
        "note": note,
    }


# ---------------------------------------------------------------------------
# 序贯判据与分组

def _decide(k, ci, theta, parent_p, margin):
    """单步判据（裁定①②见模块 docstring）→ ACCEPT/REJECT/STRADDLE。"""
    lo, hi = ci
    if hi <= theta:                                  # 裁定①：REJECT 优先
        return "REJECT"
    if lo >= theta:
        return "ACCEPT"
    if k > 0 and parent_p is not None and lo >= parent_p - margin:   # 裁定②
        return "ACCEPT"
    return "STRADDLE"


def _split(seq, g):
    """经典 ddmin 分组：连续 g 份、大小至多差 1（保持值序——确定性遍历）。"""
    total, base, extra = len(seq), len(seq) // g, len(seq) % g
    parts, i = [], 0
    for j in range(g):
        size = base + (1 if j < extra else 0)
        parts.append(list(seq[i:i + size]))
        i += size
    return parts


def _resolve_budget(budget):
    """合并默认预算 + 严格校验（拼错字段响亮拒绝——静默用默认=谎报口径）。"""
    if budget is None:
        return dict(DEFAULT_BUDGET)
    _require(isinstance(budget, dict), f"budget 须为 dict: {budget!r}")
    unknown = set(budget) - set(DEFAULT_BUDGET)
    _require(not unknown,
             f"未知预算字段: {sorted(unknown)}（合法: {sorted(DEFAULT_BUDGET)}）")
    merged = {**DEFAULT_BUDGET, **budget}
    min_t, max_t = merged["min_trials"], merged["max_trials"]
    _require(isinstance(min_t, int) and not isinstance(min_t, bool) and min_t >= 2,
             f"min_trials 须为 ≥2 整数（CORE179 硬规则：禁确定性 ddmin——"
             f"单次试验定夺被禁止）: {min_t!r}")
    _require(isinstance(max_t, int) and not isinstance(max_t, bool)
             and max_t >= min_t,
             f"max_trials 须为 ≥min_trials({min_t}) 整数: {max_t!r}")
    for key in ("target_repro_rate", "noninferiority_margin"):
        v = merged[key]
        _require(isinstance(v, (int, float)) and not isinstance(v, bool)
                 and 0.0 <= v <= 1.0, f"{key} 须为 [0,1] 数值: {v!r}")
    return merged


# ---------------------------------------------------------------------------
# 主入口：概率 ddmin

def probabilistic_ddmin(factors, run_trial, budget=None, max_total_trials=2000,
                        rng_seed=None):
    """概率 ddmin（v5 §10.3/§10.4）。

    factors: Factors 五因素；run_trial(config)->bool 单次试验注入点；
    budget: {"min_trials","max_trials","target_repro_rate",
    "noninferiority_margin"} 部分可配（缺省用 DEFAULT_BUDGET）；
    max_total_trials: 全局 run_trial 调用硬上限（超出即 INCONCLUSIVE 终止，
    保留因素=当前接受态，树完整）；rng_seed: 非None 时分组删试顺序由
    random.Random(rng_seed) 洗牌（探索多样性 + §10.3 重放），None=确定序。

    Returns {"min_set": 五因素终态 dict,
             "tree": 嵌套搜索树 {record, children}[（根=基线；无基线记录=None）,
             "trials": trial_record 平铺列表（jsonl 逐行）,
             "verdicts": 每试验判定 +（全局预算截断时）末尾 "INCONCLUSIVE",
             "status": "minimized"|"inconclusive"|"baseline_rejected",
             "necessary_aggressors": 删空崩塌（REJECT）的 aggressor 因素,
             "uncertain_factors": 有 INCONCLUSIVE 评估（保留+不确定）的因素,
             "total_trials": run_trial 总调用数, "budget": 生效预算,
             "rng_seed": 传入值}。

    min_set 的复现证据 = 树中其 ACCEPT 节点（或基线）的 k/n/ci。"""
    _require(isinstance(factors, Factors),
             f"factors 须为 Factors 实例: {type(factors).__name__}")
    _require(callable(run_trial), f"run_trial 须可调用: {run_trial!r}")
    bud = _resolve_budget(budget)
    _require(isinstance(max_total_trials, int) and not isinstance(max_total_trials, bool)
             and max_total_trials > 0,
             f"max_total_trials 须为正整数: {max_total_trials!r}")
    min_t, max_t = bud["min_trials"], bud["max_trials"]
    theta, margin = bud["target_repro_rate"], bud["noninferiority_margin"]
    rng = random.Random(rng_seed) if rng_seed is not None else None

    used = 0
    trials, verdicts = [], []
    root = None
    cur, cur_p, cur_node = factors, None, None
    necessary_aggressors, uncertain_factors = [], []

    def _finish(status, trailing_inconclusive=False):
        if trailing_inconclusive:
            verdicts.append("INCONCLUSIVE")     # 全局预算截断（无零试验记录）
        return {
            "min_set": cur.as_config(),
            "tree": root,
            "trials": trials,
            "verdicts": verdicts,
            "status": status,
            "necessary_aggressors": necessary_aggressors,
            "uncertain_factors": uncertain_factors,
            "total_trials": used,
            "budget": dict(bud),
            "rng_seed": rng_seed,
        }

    def _eval(fac, factor, removed, parent_node, parent_p):
        """序贯评估候选配置 → (verdict, k, n, node)，记录 trial/树节点。

        首批 min_trials 定初判；跨阈值 +min_trials 步进至 max_trials；
        预算耗尽于首步前 → _OutOfBudget（零试验不成记录）；耗尽于加样中
        → INCONCLUSIVE（部分 k/n 如实入树，note=global_budget_cut）。"""
        nonlocal used
        config = fac.as_config()
        k = n = 0
        ci, verdict, note = None, None, None
        while True:
            if n >= max_t:
                verdict, note = "INCONCLUSIVE", "max_trials_undecided"
                ci = wilson_ci(k, n)
                break
            step = min(min_t, max_t - n)
            if used + step > max_total_trials:
                if n == 0:
                    raise _OutOfBudget()
                verdict, note = "INCONCLUSIVE", "global_budget_cut"
                ci = wilson_ci(k, n)
                break
            hits = sum(1 for _ in range(step) if run_trial(config))
            k += hits
            used += step
            n += step
            ci = wilson_ci(k, n)
            verdict = _decide(k, ci, theta, parent_p, margin)
            if verdict != "STRADDLE":
                break
        applied = None if removed is None else verdict == "ACCEPT"
        rec = trial_record(config, k, n, ci, verdict, factor=factor, removed=removed,
                           seq=len(trials),
                           parent_seq=parent_node["record"]["seq"]
                           if parent_node is not None else None,
                           applied=applied, note=note)
        trials.append(rec)
        verdicts.append(verdict)
        node = {"record": rec, "children": []}
        if parent_node is not None:
            parent_node["children"].append(node)
        return verdict, k, n, node

    # ---- 步骤 0（§10.4）：基线冻结——先证明原配置仍以非零概率复现 ----
    try:
        verdict, k, n, node = _eval(factors, None, None, None, None)
    except _OutOfBudget:
        return _finish("inconclusive", trailing_inconclusive=True)
    root = node
    if verdict != "ACCEPT":
        # 基线 REJECT：无可缩减；基线 INCONCLUSIVE：无法确立复现——均如实返回
        return _finish("baseline_rejected" if verdict == "REJECT" else "inconclusive")
    cur_node, cur_p = root, k / n

    # ---- 步骤 1（§10.4 顺序）：逐因素经典分组 ddmin + 删空探测 ----
    for factor in FACTOR_ORDER:
        values = cur.get(factor)
        if not values:
            continue
        subset = values
        granularity = 2
        while len(subset) >= 2:
            parts = _split(subset, granularity)
            if rng is not None:
                rng.shuffle(parts)
            accepted = False
            for part in parts:
                cand = [x for x in subset if x not in set(part)]
                try:
                    verdict, k, n, node = _eval(
                        cur.reduce(factor, cand), factor, part, cur_node, cur_p)
                except _OutOfBudget:
                    return _finish("inconclusive", trailing_inconclusive=True)
                if verdict == "ACCEPT":
                    cur = cur.reduce(factor, cand)
                    subset = cand
                    cur_p, cur_node = k / n, node
                    granularity = max(2, granularity - 1)    # 删成——粗化重试
                    accepted = True
                    break
                if verdict == "INCONCLUSIVE" and factor not in uncertain_factors:
                    uncertain_factors.append(factor)
            if accepted:
                continue
            if granularity >= len(subset):                    # 单元素删皆败——1-极小
                break
            granularity = min(granularity * 2, len(subset))   # 皆败——细化
        # ---- CORE179 删空探测：整组删空 → 崩塌（REJECT）回退 + 必要标记 ----
        try:
            verdict, k, n, node = _eval(
                cur.reduce(factor, []), factor, list(subset), cur_node, cur_p)
        except _OutOfBudget:
            return _finish("inconclusive", trailing_inconclusive=True)
        if verdict == "ACCEPT":
            cur = cur.reduce(factor, [])
            cur_p, cur_node = k / n, node
        else:
            if verdict == "INCONCLUSIVE" and factor not in uncertain_factors:
                uncertain_factors.append(factor)
            if verdict == "REJECT" and factor in AGGRESSOR_FACTORS:
                # CORE179：aggressor 删空崩塌——必要 aggressor，回退到上一接受态
                necessary_aggressors.append(factor)

    return _finish("minimized")

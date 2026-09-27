#!/usr/bin/env python3
"""sdc_stats.py — 精确二项统计库（v5 §9，M4 Task 1）。stdlib only，纯函数。

组成：
  clopper_pearson(k, n, alpha=0.05)  精确二项置信区间：lo 解 P(X≥k; p)=α/2、
                                      hi 解 P(X≤k; p)=α/2；k=0/k=n 边界闭式
                                      （lo=(α/2)^(1/n) / hi=1-(α/2)^(1/n)）
  wilson_ci(k, n, alpha=0.05)        Wilson score 区间——与 tools/excite/
                                      sdc_reproducer.wilson_ci **独立实现且
                                      逐位一致**（Z95 同值常量，交叉验证测试
                                      test_wilson_matches_reproducer 守卫）
  rule_of_three(n)                   3/n（k=0 的 95% 上界工程近似）
  required_trials(p0, alpha=0.05)    暴露量下限：ceil(log(α)/log(1-p0))
                                      （v5 §9.4——以 1-α 概率见到 ≥1 次事件）
  iteration_rate(k, n, alpha=0.05)   复现率 + CP 区间 → dict；k=0 时 rate=0
                                      只是点估计，上界以 cp_hi 为准（note 口径）
  beta_binomial_adjust(ks, ns)       跨 run block 经验贝叶斯收缩：Beta(1,1)
                                      先验 → Beta(1+K, 1+N-K) 后验均值 + 等尾
                                      精确区间（k=0 不写 0——收缩后非零）

实现口径（诚实精确裁定，M4 计划 Task 1）：
  - stdlib 无 betainc/逆正态——CP 与 Beta 分位数以**二项 survival 的单调性
    二分**求解；survival 本体用 **math.comb 精确整数做对数锚点 + 相邻项比值
    递推累加**（直接 C(n,j)·p^j·q^(n-j) 求和在 n≳1030 即溢出 float；锚点+
    递推把可行域推到 n≤1e6，brief 裁定范围）；
  - comb 取 C(n, min(k, n-k)) 对称快速路径：端点形态（k=1 或 k=n-1）在
    n=1e6 仍是 O(1) 大整数；中段 C(1e6, 5e5) 单次实测 ~14s（本板 3.11）——
    lru_cache 使每次求解只算一次：可行但慢，如实记此不藏；
  - beta_binomial_adjust 设 n_total ≤ 1e6 保护上限（超出 ValueError 提示
    降采样）：精确累加的代价边界，不静默换近似（要近似走 wilson_ci 口径）。

alpha 口径：双侧 1-alpha。wilson 的 z_{1-α/2}：alpha=0.05 用与 sdc_
reproducer 逐位同值的 Z95 常量；其他 alpha 以 Φ(z)=0.5·erfc(-z/√2) 单调
二分反解（二分至双精度，非近似公式）。
"""
import functools
import math

Z95 = 1.959963984540054          # 与 sdc_reproducer.Z95 逐位同值（交叉验证守卫）
MAX_EXACT_N = 1_000_000          # beta_binomial 精确累加上限（保护）
_SQRT2 = math.sqrt(2.0)


def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


def _validate_kn(k, n):
    """k-n 口径统一校验：正整数 n、0 ≤ k ≤ n（bool 显式排除——真即 1 会
    混过 isinstance int 检查）。"""
    _require(isinstance(k, int) and not isinstance(k, bool)
             and isinstance(n, int) and not isinstance(n, bool),
             f"参数必须为 int: k={k!r} n={n!r}")
    _require(n > 0, f"n 必须 > 0: {n!r}")
    _require(0 <= k <= n, f"需要 0 ≤ k ≤ n: k={k} n={n}")


def _validate_alpha(alpha):
    _require(isinstance(alpha, (int, float)) and not isinstance(alpha, bool)
             and 0.0 < alpha < 1.0, f"alpha 必须 ∈ (0,1): {alpha!r}")


def _z_two_sided(alpha):
    """z_{1-α/2}。alpha=0.05 → Z95 常量；其他以 Φ 单调二分反解（erfc 路径
    保尾部精度）。"""
    if alpha == 0.05:
        return Z95
    a, b = -12.0, 12.0                    # Φ(∓12)≈∓1e-33 之外——覆盖 (0,1)
    for _ in range(200):
        mid = (a + b) / 2.0
        if mid <= a or mid >= b:          # 相邻双精度——已收敛
            break
        if 0.5 * math.erfc(-mid / _SQRT2) < 1.0 - alpha / 2.0:
            a = mid
        else:
            b = mid
    return (a + b) / 2.0


@functools.lru_cache(maxsize=256)
def _log_comb(n, k):
    """log C(n,k)——math.comb 精确整数取对数（对称快速路径 min(k, n-k)；
    k=0/n → 0.0）。lru_cache：二分循环内同 (n,k) 反复求 survival，只算一次
    （中段大 n 的 comb 是唯一重活——C(1e6,5e5) 单次 ~14s，绝不能每步重算）。"""
    kk = min(k, n - k)
    if kk == 0:
        return 0.0
    return math.log(math.comb(n, kk))


def _sf(k, n, p):
    """P(X ≥ k)，X~Bin(n, p)；契约 1 ≤ k ≤ n、0 < p < 1。

    项锚点 term(j0) = exp(log C(n,j0) + j0·log p + (n-j0)·log1p(-p))——
    math.comb 精确整数锚点（直接整数转浮点在 n≳1030 溢出，log 域规避；
    log1p(-p) 保小 p 下 q^n 的精度）。其余项按相邻比值递推：
    term(j+1)/term(j) = (n-j)/(j+1)·p/q（比值随远离模式单调递减），几何
    衰减处早停（截断相对误差 ≤ ~2e-18）。
    模式判定 k vs (n+1)p：k 在模式下方（survival 大）走 1-P(X≤k-1) 左尾
    向下累加；否则右尾自 term(k) 向上累加——两侧都不会把大质量算成 0×inf。
    """
    _require(0.0 < p < 1.0, f"_sf 契约 0<p<1: p={p!r}")
    q = 1.0 - p
    if k < (n + 1) * p:                   # k ≤ 模式 → sf 大，走 1 - 左尾
        j0 = k - 1                        # 左尾最大项（模式以下自上而下递减）
        t = math.exp(_log_comb(n, j0) + j0 * math.log(p)
                     + (n - j0) * math.log1p(-p))
        acc, r = 1.0, 1.0
        for j in range(j0, 0, -1):        # term(j-1)/term(j) = j/(n-j+1)·q/p
            r *= j * q / ((n - j + 1) * p)
            if r < 1e-18 * acc and r < 0.5:
                break                     # 比值此后单调更小——余项 ≤ ~2e-18·acc
            acc += r
        cdf = t * acc                     # P(X ≤ k-1)
        return 1.0 - cdf if cdf > 0.0 else 1.0   # t 下溢 ⇒ 左尾≈0 ⇒ sf≈1
    # k > 模式：右尾 sf = Σ_{j≥k} term(j)，term(k) 为尾内最大项
    t = math.exp(_log_comb(n, k) + k * math.log(p)
                 + (n - k) * math.log1p(-p))
    acc, r = 1.0, 1.0
    for j in range(k, n):                 # term(j+1)/term(j) = (n-j)/(j+1)·p/q
        r *= (n - j) * p / ((j + 1) * q)
        if r < 1e-18 * acc and r < 0.5:
            break
        acc += r
    return t * acc                        # t 下溢 ⇒ 真值 < ~1e-300，0 方向仍对


def _solve_sf(k, n, target):
    """解 P(X ≥ k; p) = target 的 p（sf 对 p 单调升；1 ≤ k ≤ n、
    0 < target < 1）——二分至相邻双精度（mid 不再前进即收敛，≤1 ulp）。"""
    lo, hi = 0.0, 1.0
    for _ in range(200):
        mid = (lo + hi) / 2.0
        if mid <= lo or mid >= hi:
            break
        if _sf(k, n, mid) < target:
            lo = mid
        else:
            hi = mid
    return (lo + hi) / 2.0


# ---------------------------------------------------------------------------
# 公开 API

def clopper_pearson(k, n, alpha=0.05):
    """Clopper-Pearson 精确二项置信区间 → (lo, hi)。

    定义式：lo 解 P(X≥k; p)=α/2（k≥1），hi 解 P(X≤k; p)=α/2（k<n；经补集
    对称 P(X≤k; p)=P(X'≥n-k; 1-p) 化为同一 survival 求解）。边界闭式：
    k=0 → (0, 1-(α/2)^(1/n))；k=n → ((α/2)^(1/n), 1)。n≤1e6 内精确可行
    （实现机械与代价见模块 docstring）。
    """
    _validate_kn(k, n)
    _validate_alpha(alpha)
    half = alpha / 2.0
    if k == 0:
        lo = 0.0
    elif k == n:
        lo = half ** (1.0 / n)                 # 边界闭式（P(X=n)=p^n=α/2）
    else:
        lo = _solve_sf(k, n, half)
    if k == n:
        hi = 1.0
    elif k == 0:
        hi = 1.0 - half ** (1.0 / n)           # 边界闭式（P(X=0)=q^n=α/2）
    else:
        hi = 1.0 - _solve_sf(n - k, n, half)   # P(X≤k; hi)=α/2 补集求解
    return (lo, hi)


def wilson_ci(k, n, alpha=0.05):
    """Wilson score 区间 → (lo, hi)——公式与 sdc_reproducer.wilson_ci 逐位
    一致（独立实现交叉验证）；k=0 下界/k=n 上界为数学精确值 0/1 显式特判
    （消除 center∓half 的浮点噪声）。"""
    _validate_kn(k, n)
    _validate_alpha(alpha)
    z = _z_two_sided(alpha)
    p = k / n
    denom = 1 + z * z / n
    center = (p + z * z / (2 * n)) / denom
    half = z * (p * (1 - p) / n + z * z / (4 * n * n)) ** 0.5 / denom
    lo = 0.0 if k == 0 else max(0.0, center - half)
    hi = 1.0 if k == n else min(1.0, center + half)
    return (lo, hi)


def rule_of_three(n):
    """3/n——零事件时 95% 上界的工程近似（精确上界 1-0.05^(1/n) ≈ 2.996/n；
    n≥30 时相对偏差 <1%，报告口径的速算形式）。"""
    _require(isinstance(n, int) and not isinstance(n, bool) and n > 0,
             f"n 必须为正整数: {n!r}")
    return 3.0 / n


def required_trials(p0, alpha=0.05):
    """以概率 ≥ 1-α 见到 ≥1 次事件的试验数下限：ceil(log(α)/log(1-p0))
    （v5 §9.4 暴露量公式；log1p(-p0) 规避小 p0 下 log(1-p0) 的有效位损失）。"""
    _require(isinstance(p0, (int, float)) and not isinstance(p0, bool)
             and 0.0 < p0 < 1.0, f"p0 必须 ∈ (0,1): {p0!r}")
    _validate_alpha(alpha)
    return math.ceil(math.log(alpha) / math.log1p(-p0))


def iteration_rate(confirmed, valid_iterations, alpha=0.05):
    """复现率摘要 → {"rate","cp_lo","cp_hi","n","k","note"}。

    k=0 时 rate=0 只是点估计——频率上界以 cp_hi 为准（note 诚实口径：
    0/1000 不得报成"复现概率为 0"）；k=n 全复现同理以下界为准。
    """
    k, n = confirmed, valid_iterations
    _validate_kn(k, n)
    _validate_alpha(alpha)
    lo, hi = clopper_pearson(k, n, alpha)
    out = {"rate": k / n, "cp_lo": lo, "cp_hi": hi, "n": n, "k": k}
    if k == 0:
        out["note"] = (f"k=0：rate=0 为点估计，频率上界以 cp_hi={hi:.6g} 为准"
                       f"（Clopper-Pearson 精确 {1 - alpha:.0%} 上界；"
                       f"rule-of-three ≈ {rule_of_three(n):.3g}）")
    elif k == n:
        out["note"] = (f"k=n：全复现——频率下界以 cp_lo={lo:.6g} 为准"
                       f"（精确 {1 - alpha:.0%} 下界；确定性缺陷候选须健康核"
                       "对照裁定，见 sdc_reproducer 门禁4/7）")
    else:
        out["note"] = f"Clopper-Pearson 精确 {1 - alpha:.0%} 等尾区间"
    return out


def beta_binomial_adjust(ks, ns, alpha=0.05):
    """跨 run block 收缩估计 → dict（经验贝叶斯：Beta(1,1) 均匀先验 + 二项
    似然 → Beta(1+K, 1+N-K) 后验；posterior_mean + 等尾精确区间）。

    区间与 CP 同一累加机械（整数参数恒等式 I_x(a,b) = P(Y≥a)，Y~Bin(a+b-1,
    x)）——非 Wilson 近似（诚实精确裁定）。n_total > 1e6 保护性 ValueError
    （提示降采样）：精确累加的代价边界，不静默换近似。
    """
    _validate_alpha(alpha)
    try:
        ks, ns = list(ks), list(ns)
    except TypeError:
        raise ValueError(f"ks/ns 必须为可迭代整数序列: {ks!r} {ns!r}") from None
    _require(len(ks) == len(ns), f"ks/ns 等长: {len(ks)} != {len(ns)}")
    _require(len(ks) > 0, "ks/ns 为空——无 run block 可收缩（拒绝而非先验裸报）")
    for k, n in zip(ks, ns):
        _validate_kn(k, n)
    n_total = sum(ns)
    _require(n_total <= MAX_EXACT_N,
             f"n_total={n_total} > {MAX_EXACT_N}——精确累加（math.comb 锚点）"
             "超出可行代价边界，请降采样（合并/抽样 run block）或改用 "
             "wilson_ci 近似口径（不得静默换近似）")
    k_total = sum(ks)
    a, b = 1 + k_total, 1 + n_total - k_total   # 后验 Beta(a, b)
    m = n_total + 1                             # a + b - 1
    half = alpha / 2.0
    lo = _solve_sf(a, m, half)                  # I_lo(a, b) = α/2
    hi = 1.0 - _solve_sf(b, m, half)            # I_hi(a, b) = 1-α/2（补集对称）
    return {
        "posterior_mean": a / (a + b),
        "ci_lo": lo,
        "ci_hi": hi,
        "k_total": k_total,
        "n_total": n_total,
        "prior": "Beta(1,1)",
        "alpha": alpha,
        "note": "经验贝叶斯收缩：Beta(1,1) 先验 + 二项似然 → Beta(1+K, "
                "1+N-K) 后验；等尾精确区间（与 Clopper-Pearson 同一累加机械）",
    }

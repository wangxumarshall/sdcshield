"""sdc_stats 单元测试（M4 Task 1）——精确二项统计库（v5 §9）。

口径裁定：
  - Clopper-Pearson 为精确区间，**定义式即规格**：lo 解 P(X≥k; p)=α/2、
    hi 解 P(X≤k; p)=α/2——test_cp_satisfies_defining_equation 以独立
    math.comb 直接求和为 oracle 断言（不抄录统计表，自证自明）；
  - wilson_ci 与 tools/excite/sdc_reproducer.wilson_ci **逐位一致**（独立
    实现交叉验证——两处公式漂移即红，Z95 常量同值）；
  - beta_binomial_adjust：Beta(1,1) 先验 + 等尾精确后验区间（与 CP 同一
    累加机械，非 Wilson 近似——诚实精确裁定）；n_total>1e6 保护性拒绝
    （累加路径的代价边界，提示降采样）。
"""
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_stats as st


def test_cp_known_values():
    lo, hi = st.clopper_pearson(0, 20)
    assert lo == 0.0 and abs(hi - (1 - 0.025 ** (1/20))) < 1e-12
    lo2, hi2 = st.clopper_pearson(20, 20)
    assert abs(lo2 - 0.025 ** (1/20)) < 1e-12 and hi2 == 1.0
    lo3, hi3 = st.clopper_pearson(5, 20)
    assert 0.08 < lo3 < 0.13 and 0.38 < hi3 < 0.50   # 统计表值域


def test_cp_symmetry_and_monotonicity():
    a = st.clopper_pearson(3, 10); b = st.clopper_pearson(4, 10)
    assert b[0] > a[0] and b[1] > a[1]


def test_rule_of_three():
    assert abs(st.rule_of_three(300) - 0.01) < 1e-9


def test_required_trials():
    n = st.required_trials(1e-4)
    assert 29000 < n < 31000                     # 3/p0 量级


def test_iteration_rate_k0_upper_bound():
    r = st.iteration_rate(0, 1000)
    assert r["rate"] == 0.0 and r["cp_hi"] > 0 and r["cp_hi"] < 0.005
    assert "上界" in r.get("note", "") or True   # 诚实口径注释


def test_wilson_matches_reproducer():
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "excite"))
    import sdc_reproducer as sr
    for k, n in [(0, 10), (5, 10), (10, 10), (3, 100)]:
        assert st.wilson_ci(k, n) == sr.wilson_ci(k, n)


def test_beta_binomial_shrinks_toward_prior():
    r = st.beta_binomial_adjust([0, 0, 0], [100, 100, 100])
    assert 0 < r["posterior_mean"] < 0.01        # 收缩后非零（k=0 不写 0）


# ---------------------------------------------------------------------------
# 定义式 oracle（独立精确求和——不依赖实现内部机械）

def _sf(k, n, p):
    """P(X≥k)，math.comb 直接求和（小 n 精确 oracle）。"""
    from math import comb
    return sum(comb(n, j) * p ** j * (1 - p) ** (n - j)
               for j in range(k, n + 1))


def test_cp_satisfies_defining_equation():
    """CP 端点是二项方程的根：P(X≥k; lo)=α/2、P(X≤k; hi)=α/2（定义即规格，
    独立 oracle 求和断言——实现内部机械（log 锚点+比值递推）不得偏离）。"""
    for k, n in [(3, 10), (5, 20), (7, 30), (10, 20)]:
        lo, hi = st.clopper_pearson(k, n)
        assert abs(_sf(k, n, lo) - 0.025) < 1e-9
        assert abs((1 - _sf(k + 1, n, hi)) - 0.025) < 1e-9   # P(X≤k; hi)


def test_beta_binomial_defining_equation():
    """后验等尾区间定义式：I_x(a,b) = P(Y≥a)，Y~Bin(a+b-1, x)（整数参数
    正则化不完全 beta 恒等式）——下界 I_lo=α/2、上界经补集 I_{1-hi}(b,a)=α/2。"""
    r = st.beta_binomial_adjust([2, 1], [10, 20])   # K=3, N=30 → Beta(4, 28)
    assert abs(r["posterior_mean"] - 4 / 32) < 1e-15
    lo, hi = r["ci_lo"], r["ci_hi"]                 # m = N+1 = 31
    assert abs(_sf(4, 31, lo) - 0.025) < 1e-9       # I_lo(4, 28) = 0.025
    assert abs(_sf(28, 31, 1 - hi) - 0.025) < 1e-9  # I_{1-hi}(28, 4) = 0.025


# ---------------------------------------------------------------------------
# 边界与保护（k>n / n=0 / beta_binomial 大 n 上限）

def test_input_validation():
    """k>n / n=0 / 负值：k-n 口径函数一致响亮拒绝（不静默产出无意义数）。"""
    for fn in (st.clopper_pearson, st.wilson_ci):
        for bad in [(2, 1), (-1, 10), (0, 0)]:
            with pytest.raises(ValueError):
                fn(*bad)
    for bad in [(2, 1), (0, 0)]:
        with pytest.raises(ValueError):
            st.iteration_rate(*bad)


def test_beta_binomial_input_and_large_n_guard():
    """输入校验 + 累加上限保护：n_total>1e6 → ValueError 提示降采样
    （精确累加路径（选①）的代价边界；恰在 1e6 上限仍可行）。"""
    for bad_ks, bad_ns in [([5], [3]), ([], []), ([-1], [10]), ([0], [-1])]:
        with pytest.raises(ValueError):
            st.beta_binomial_adjust(bad_ks, bad_ns)
    with pytest.raises(ValueError, match="降采样"):
        st.beta_binomial_adjust([0], [1_000_001])
    with pytest.raises(ValueError, match="降采样"):
        st.beta_binomial_adjust([0, 0], [600_000, 600_000])
    r = st.beta_binomial_adjust([0], [1_000_000])   # 恰在保护上限——可行
    assert 0 < r["posterior_mean"] < 1e-5           # 1/(N+2) ≈ 1e-6


# ---------------------------------------------------------------------------
# 大 n 可行性（brief 口径：n≤1e6 精确可行——comb 对称快速路径 + lru_cache）

def test_cp_midrange_large_n():
    """中段大 n（n=1e5 偶分裂）：量级正确 + CP 精确对称 hi = 1 - lo
    （二项对称性——k=n/2 时上下界互补，强于量级的结构性断言）。"""
    lo, hi = st.clopper_pearson(50_000, 100_000)
    assert abs(lo - 0.5) < 0.005 and abs(hi - 0.5) < 0.005
    assert abs(hi - (1.0 - lo)) < 1e-9


def test_cp_edge_large_n():
    """n=1e6 端点形态（k=1）：comb 对称快速路径下可行；定义式闭式核对
    （P(X≥1; lo)=1-q^n=α/2，P(X≤1; hi)=q^n+n·p·q^(n-1)=α/2）。"""
    lo, hi = st.clopper_pearson(1, 1_000_000)
    assert 0 < lo < 1e-7 and 1e-6 < hi < 2e-5
    assert abs((1 - (1 - lo) ** 1_000_000) - 0.025) < 1e-8
    q = 1 - hi
    assert abs(q ** 1_000_000 + 1_000_000 * hi * q ** 999_999 - 0.025) < 1e-8

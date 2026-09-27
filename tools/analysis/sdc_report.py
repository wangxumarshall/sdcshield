#!/usr/bin/env python3
"""sdc_report.py — 离线诊断报告生成器（M4 退出标准载体，v5 §9/§11/§12.3，M4 Task 5）。
stdlib only。消费 M0-M4 全链产物，产出五段 Markdown 诊断报告 + 红线自检。

数据根布局（缺失件降级注记，绝不零填充）：
  events/ledger.csv        台账（行级解析复用 tools/telemetry/sdc_legacy_adapter.
                            parse_ledger——单一事实源，勿重写）；缺失 → 零事件路径
  spool/events.jsonl       canonical 事件流（M0 schema）：sdc_mismatch 事件取其
                            mismatch 块（dict 或块列表）入位形态段；mismatch 事件
                            存在性 → T4 has_mismatch 注入；缺失 → has_mismatch=None
  spool/exposure.json      暴露量摘要（v5 §9.6 等效试验日汇总的机器可读形：
                            {"valid_iterations", "valid_core_hours", "source"}）；
                            真实根当前无此文件 → 暴露量不可得，仅事件计数（如实）
  monitor/pmu_core.csv     M1 PMU 采集（时序段事件对齐窗口 + 嫌疑/对照差分；
                            流式读取——T4 评审 Important #3 口径）
  events/<event_id>/ring_window/  capsule（120s 环窗冻结，M2 Task 2）；存在则引

台账行分类级联（v5 §9.5 九类目标态的 as-built 映射；顺序即优先级）：
  1. note 含「测试污染」            → 污染（INVALID）——不入 SDC 率分母但单列报告
  2. note 含竞态/test_bug/伪SDC     → 测试缺陷（TEST_BUG）——同上不入分母
     （关键词复用 sdc_legacy_adapter._TEST_RACE_KEYS，单一事实源）
  3. note 含定性/定案 且 非SDC      → 定性非 SDC（INVALID）——同上不入分母
  4. test 非空且 rc∈{1,137}         → SDC 候选（k 计数；战役口径同 sdc_eventd）
  5. 其余                           → 非失败行（note 类，不进任何率分子）

公开 API：
  generate_report(data_root, event_id=None, out_path=None) -> str
      五段报告：①概述（事件清单+分母/暴露表+证据分级三段）②位形态（T2）
      ③时序（T4——无 mismatch 类事件时整段省略）④假设排序（T3——证据等级+
      守卫行）⑤建议（下一项最有信息量实验=各域第一名探针）+ 尾部 report_guard
      自检段。event_id 给定 → 单事件深挖模式（events.jsonl 定位，未知 id 抛
      ValueError）；out_path 给定 → 同时写文件。
  rate_table(events, valid_iterations=None, valid_core_hours=None) -> str
      迭代（二项，Clopper-Pearson 精确区间）+ 核心小时（Poisson，χ² 等尾精确
      区间——log 域 lgamma 逐项累加 + 单调二分：朴素 e^{-λ} 前缀递推在 λ≳745
      下溢，k≳690 区间会塌缩为点，log 域全域无下溢，评审 Important #3）双口径
      表；k=0 行显式「上界 X；rule of three ≈ 3/n」（不写零率）；暴露缺省 →
      「暴露量不可得，仅事件计数」。
  fact_inference_hypothesis_sections(evidence) -> (facts, inferences, hypotheses)
      v5 §11.1-5 三段切分：事实 = 原始观测/重算结果/干预记录（已发生的事实）；
      推断 = 同窗相关/对照差分（相关非因果）；假设 = 未验证解释/假设排序/探针
      建议。中英文语义键皆收（见 _FACT/_INFER/_HYPO_KINDS）；未知键忽略、
      非 str 陈述跳过（降级不抛）、非 dict 输入 ValueError。
  report_guard(md_text) -> list[str]
      红线行级扫描（宽松正则 + 人工复核注释，brief 四违例之②拆两类检测）：
      ①无分母比率——百分数而行内无 n= 或 / 分母；
      ②无区间发生率——「率」陈述有分母而行内无 [ 区间/上界/CI（brief ②，
        评审 Important #1 补齐：有分母无 CI 此前静默通过）；形态学比例
        （单 bit 3/10 等，行内无「率」字）不在此列；
      ③零率表述——正则 发生率(?:为)?\s*0(?![.\d])（负向前瞻边界：0.15% 小数
        率不误报，评审 Important #2）；
      ④「PMU 异常」与「SDC」同现且无「证据非判据」字样；
      ⑤「根因已定位」且无 E2/E3/E4 前缀。
      返回「行 N: 类型」清单；生成器内部对成稿自跑一遍并附自检段。

T4 私有件复用（同 tools/analysis 家族内约定，评审 Important #3 流式口径）：
_slice_window/_core_select/_diff_from_slice/_extract_window/_cpu_key/_core_key。

诚实边界：假设排序证据键只从数据可派生项生成（cpu_clustering=位视图 logical
占比≥0.5、bit_hints=T2 提示令牌）；n1_immune/multicore_only 等无数据源不设。
台账 retests 为同命令复测，不计为干预记录（不虚报 E2）。

用法：python3 sdc_report.py [--data-root DIR] [--event-id ID] [-o out.md]
"""
import datetime
import json
import math
import os
import re
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
for _p in (_HERE, os.path.join(_HERE, "..", "telemetry")):
    if _p not in sys.path:
        sys.path.insert(0, _p)
import sdc_bitview                          # T2
import sdc_hypothesis                       # T3
import sdc_stats                            # T1
import sdc_timeline as tl                   # T4
import sdc_legacy_adapter                   # M0：parse_ledger 单一事实源

ALPHA = 0.05
FAIL_RCS = ("1", "137")          # 战役口径（sdc_eventd.ledger_line_to_event 同源）
POLLUTION_KEY = "测试污染"        # v5 §9.5 INVALID——不入 SDC 率分母但必须单列
WINDOW_S = 60.0                  # 事件对齐窗口（T4 缺省 ±60s）

# 分类键 →（显示标签，是否进 k）
_CLASS_LABEL = {
    "pollution": "污染（INVALID，不入 SDC 率）",
    "test_bug": "测试缺陷（TEST_BUG，不入 SDC 率）",
    "resolved_non_sdc": "定性非 SDC（INVALID，不入 SDC 率）",
    "sdc_candidate": "SDC 候选",
    "non_fail": "非失败行（note）",
}

# 三段切分语义键（中英文皆收；序即输出序——与调用方 dict 键序无关）
_FACT_KINDS = ("raw_observations", "recomputed_results", "interventions",
               "原始观测", "重算结果", "干预记录")
_INFER_KINDS = ("same_window_correlations", "control_differentials",
                "同窗相关", "对照差分")
_HYPO_KINDS = ("unverified_explanations", "hypotheses", "probes",
               "未验证解释", "假设排序", "探针建议")

_PCT_RE = re.compile(r"\d+(?:\.\d+)?%")
# 率值陈述：「率」后（可带 为/是/=:： 连接词）紧跟数字——区分率陈述与矩阵
# 叙述文（如「失败率对环境无响应」不带数值，不在此列）
_RATE_VALUE_RE = re.compile(r"率(?:为|是|=|:|：)?\s*\d")
# 零率表述：整零（含句读/行尾边界）才触发——负向前瞻排除 0.15% 等小数率
# （评审 Important #2：子串匹配把合规小数率误报为零率）
_ZERO_RATE_RE = re.compile(r"发生率(?:为)?\s*0(?![.\d])")


def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


# ---------------------------------------------------------------------------
# 数据装载

def _load_events_jsonl(path):
    """逐行读 events.jsonl → (events, n_bad)。文件缺失 → (None, 0)（调用方以
    None 区分「无事件流」与「空流」——has_mismatch 语义不同）；坏行计数不抛。"""
    if not os.path.isfile(path):
        return None, 0
    events, bad = [], 0
    with open(path, encoding="utf-8") as f:
        for line in f:
            if not line.strip():
                continue
            try:
                events.append(json.loads(line))
            except json.JSONDecodeError:
                bad += 1
    return events, bad


def _load_exposure(root):
    """spool/exposure.json → dict|None（缺失/坏 JSON/非 dict → None，不抛）。"""
    p = os.path.join(root, "spool", "exposure.json")
    if not os.path.isfile(p):
        return None
    try:
        with open(p, encoding="utf-8") as f:
            d = json.load(f)
    except (OSError, json.JSONDecodeError):
        return None
    return d if isinstance(d, dict) else None


def _exposure_values(exposure):
    """exposure dict → (valid_iterations|None, valid_core_hours|None)——
    非正数/类型不对按缺失处理（不伪造 0）。"""
    if not exposure:
        return (None, None)
    n = exposure.get("valid_iterations")
    h = exposure.get("valid_core_hours")
    n = n if isinstance(n, int) and not isinstance(n, bool) and n > 0 else None
    h = h if isinstance(h, (int, float)) and not isinstance(h, bool) \
        and h > 0 else None
    return (n, h)


def _parse_ts(v):
    """台账 ts（"%Y-%m-%d %H:%M:%S"）→ datetime；不可解析 → None。"""
    if not isinstance(v, str):
        return None
    try:
        return datetime.datetime.strptime(v.strip(), "%Y-%m-%d %H:%M:%S")
    except ValueError:
        return None


def _ev_dt(ev):
    """事件 realtime_ns → 本地 naive datetime；缺失/非 int → None。"""
    t = (ev.get("time") or {}).get("realtime_ns") if isinstance(ev, dict) else None
    if not isinstance(t, int):
        return None
    return datetime.datetime.fromtimestamp(t / 1e9)


def classify_row(row):
    """台账行 → 分类键（级联见模块 docstring；v5 §9.5 INVALID/TEST_BUG 不入分母）。"""
    note = row.get("note") or ""
    rc, test = row.get("rc") or "", row.get("test") or ""
    if POLLUTION_KEY in note:
        return "pollution"
    if any(k in note for k in sdc_legacy_adapter._TEST_RACE_KEYS):
        return "test_bug"
    if any(k in note for k in ("定性", "定案")) and ("非SDC" in note or "非 SDC" in note):
        return "resolved_non_sdc"
    if test and rc in FAIL_RCS:
        return "sdc_candidate"
    return "non_fail"


def _match_events(events, row):
    """events.jsonl 中与台账行对应的事件（run_id=label 且时刻差 ≤2s）。"""
    if events is None:
        return []
    dt = _parse_ts(row.get("ts"))
    out = []
    for ev in events:
        if not isinstance(ev, dict) or ev.get("run_id") != row.get("label"):
            continue
        edt = _ev_dt(ev)
        if dt is None or edt is None:
            continue
        if abs((edt - dt).total_seconds()) <= 2.0:
            out.append(ev)
    return out


def _event_blocks(ev):
    """单事件的 mismatch 块列表（dict → [dict]；list → 其中 dict 项；缺 → []）。"""
    m = ev.get("mismatch") if isinstance(ev, dict) else None
    if isinstance(m, dict):
        return [m]
    if isinstance(m, list):
        return [x for x in m if isinstance(x, dict)]
    return []


def _collect_blocks(events):
    """全流 sdc_mismatch 事件 → (blocks, n_total, n_excluded)。
    排除 test_race 分类/源行含「测试污染」的事件（其 mismatch 数据非 SDC 证据，
    口径在位形态段注记）。"""
    blocks, total, excluded = [], 0, 0
    for ev in events or []:
        if not isinstance(ev, dict) or ev.get("event_type") != "sdc_mismatch":
            continue
        total += 1
        cls = (ev.get("classification") or {}).get("primary")
        if cls == "test_race" or POLLUTION_KEY in (ev.get("source_line") or ""):
            excluded += 1
            continue
        blocks.extend(_event_blocks(ev))
    return blocks, total, excluded


# ---------------------------------------------------------------------------
# has_mismatch 注入（T4 评审移交）

def has_mismatch_in_window(events, event_ts, before_s=WINDOW_S, after_s=WINDOW_S):
    """events.jsonl 事件流 → 事件窗口内 mismatch 事件存在性（T4 判读规则②③依据）。
    None（无 events.jsonl）→ None（事件台账不可得，②③不裁）；事件列表 → bool
    （空流/窗外= False）。event_ts 不可解析 → ValueError。"""
    ev = _parse_ts(event_ts) if isinstance(event_ts, str) else None
    if ev is None and not isinstance(event_ts, datetime.datetime):
        raise ValueError(f"event_ts 不可解析: {event_ts!r}")
    if not isinstance(event_ts, datetime.datetime):
        event_ts = ev
    if events is None:
        return None
    for e in events:
        if not isinstance(e, dict) or e.get("event_type") != "sdc_mismatch":
            continue
        edt = _ev_dt(e)
        if edt is None:
            continue
        dt = (edt - event_ts).total_seconds()
        if -before_s - 1e-9 <= dt <= after_s + 1e-9:
            return True
    return False


# ---------------------------------------------------------------------------
# Poisson 精确区间（核心小时口径；整数 k 级数 + 单调二分，sdc_stats 同机械口径）

def _poisson_cdf(k, lam):
    """P(X ≤ k)，X~Poisson(lam)；契约 k ≥ 0、lam ≥ 0。
    log 域逐项累加：term(j) = exp(-lam + j·ln lam - lgamma(j+1))——各项独立
    取 exp。朴素实现以 t0=e^{-lam} 前缀递推，λ≳745 时 t0 下溢为 0 且峰邻项
    全被污染（k≳690 区间塌缩为点：评审 Important #3 实测 k=700 hi 偏低
    1.2%、k≥750 全塌——旧 docstring「二分方向仍正确」断言在该区间为假）。
    峰后（j > lam）项单调衰减，相对累加值 < 1e-18 早停（余项可忽略）。"""
    if lam <= 0.0:
        return 1.0                     # λ=0：P(X≤k)=1（k ≥ 0）
    log_lam = math.log(lam)
    acc = 0.0
    for j in range(k + 1):
        t = math.exp(-lam + j * log_lam - math.lgamma(j + 1))
        acc += t
        if j > lam and t < acc * 1e-18:
            break
    return acc


def _bisect(f, target, hi_bound, increasing):
    """单调 f 在 (0, hi_bound] 内解 f(λ)=target（f(0) 与 f(hi_bound) 跨 target），
    二分至相邻双精度。increasing=f 随 λ 递增。"""
    lo = 0.0
    for _ in range(200):
        mid = (lo + hi_bound) / 2.0
        if mid <= lo or mid >= hi_bound:
            break
        if (f(mid) < target) == increasing:
            lo = mid
        else:
            hi_bound = mid
    return (lo + hi_bound) / 2.0


def _poisson_ci(k, exposure, alpha=ALPHA):
    """k 事件 / exposure 核心小时 → 每核时率精确区间 (lo, hi)（等尾：lo 解
    P(X≥k;λ)=α/2、hi 解 P(X≤k;λ)=α/2；k=0 → (0, -ln(α/2)/exposure) 闭式）。
    累加机械见 _poisson_cdf（log 域 lgamma——大 λ/k 无下溢塌缩，评审
    Important #3；k=700/1.0 → hi≈753.8、k=1000/1.0 → [938, 1064] 回归锚点）。"""
    _require(isinstance(k, int) and not isinstance(k, bool) and k >= 0,
             f"k 必须为 ≥0 的 int: {k!r}")
    _require(isinstance(exposure, (int, float)) and not isinstance(exposure, bool)
             and exposure > 0, f"exposure 必须 > 0: {exposure!r}")
    if k == 0:
        return (0.0, -math.log(alpha / 2.0) / exposure)
    bound = k + 10.0 * math.sqrt(k) + 20.0      # 解的上界（k+10√k+20 覆盖等尾解）
    half = alpha / 2.0
    lo_lam = _bisect(lambda l: 1.0 - _poisson_cdf(k - 1, l), half, bound, True)
    hi_lam = _bisect(lambda l: _poisson_cdf(k, l), half, bound, False)
    return (lo_lam / exposure, hi_lam / exposure)


# ---------------------------------------------------------------------------
# rate_table

def _as_count(events):
    """events（事件列表/计数 k/None）→ k。"""
    if events is None:
        return 0
    if isinstance(events, bool):
        raise ValueError(f"events 必须为事件列表或非负 int 计数: {events!r}")
    if isinstance(events, int):
        _require(events >= 0, f"计数必须 ≥0: {events!r}")
        return events
    try:
        return len(list(events))
    except TypeError:
        raise ValueError(f"events 必须为事件列表或非负 int 计数: {events!r}") from None


def rate_table(events, valid_iterations=None, valid_core_hours=None):
    """事件 + 暴露量 → 迭代/核心小时双口径率表 Markdown（口径见模块 docstring）。"""
    k = _as_count(events)
    if valid_iterations is not None:
        _require(isinstance(valid_iterations, int) and not isinstance(valid_iterations, bool)
                 and valid_iterations > 0,
                 f"valid_iterations 必须为正 int: {valid_iterations!r}")
    if valid_core_hours is not None:
        _require(isinstance(valid_core_hours, (int, float))
                 and not isinstance(valid_core_hours, bool) and valid_core_hours > 0,
                 f"valid_core_hours 必须为 >0 的数: {valid_core_hours!r}")
    L = ["| 口径 | k/n | 点估计 | 95% CI（Clopper-Pearson/Poisson 精确） | 备注 |",
         "|---|---|---|---|---|"]
    if valid_iterations is None:
        L.append(f"| 迭代 | k={k}/n=不可得 | — | — |"
                 " 无 loop-count 数据，暴露量不可得，仅事件计数 |")
    elif k == 0:
        n = valid_iterations
        _, hi = sdc_stats.clopper_pearson(0, n)
        L.append(f"| 迭代 | k=0/n={n} | 0（未检出） | 上界 {hi:.3g} |"
                 f" k=0：频率上界 {hi:.3g}（CP 精确 95%）；rule of three ≈ 3/n"
                 f" = {sdc_stats.rule_of_three(n):.3g}——点估计 0 不作零率声明 |")
    else:
        n = valid_iterations
        lo, hi = sdc_stats.clopper_pearson(k, n)
        L.append(f"| 迭代 | k={k}/n={n} | {k}/{n} = {k / n:.3g} |"
                 f" [{lo:.3g}, {hi:.3g}] | CP 精确 95% 等尾区间 |")
    if valid_core_hours is None:
        L.append(f"| 核心小时 | k={k}/h=不可得 | — | — |"
                 " 无核心小时暴露数据，仅事件计数 |")
    elif k == 0:
        h = valid_core_hours
        _, hi = _poisson_ci(0, h)
        L.append(f"| 核心小时 | k=0/h={h:g} | 0（未检出） | 上界 {hi:.3g} |"
                 f" k=0：上界 {hi:.3g}（Poisson 精确 95%）；rule of three"
                 f" ≈ 3/h = {3.0 / h:.3g} |")
    else:
        h = valid_core_hours
        lo, hi = _poisson_ci(k, h)
        L.append(f"| 核心小时 | k={k}/h={h:g} | {k}/{h:g} = {k / h:.3g}/核时 |"
                 f" [{lo:.3g}, {hi:.3g}] | Poisson 精确 95% 等尾（χ²） |")
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------------------
# 三段切分（v5 §11.1-5）

def _statements(value):
    """值 → 陈述列表：str → [strip 非空]；list/tuple → 其中 str 项；其余 []。"""
    if isinstance(value, str):
        return [value] if value.strip() else []
    if isinstance(value, (list, tuple)):
        return [v for v in value if isinstance(v, str) and v.strip()]
    return []


def fact_inference_hypothesis_sections(evidence):
    """evidence dict → (facts, inferences, hypotheses)。语义键映射与降级契约见
    模块 docstring；未知键忽略（扩展键不炸），非 dict 输入 ValueError。"""
    if not isinstance(evidence, dict):
        raise ValueError(f"evidence 必须为 dict（语义键→陈述）；无证据请传 {{}}: "
                         f"{type(evidence).__name__}")
    facts = [s for key in _FACT_KINDS for s in _statements(evidence.get(key))]
    inferences = [s for key in _INFER_KINDS for s in _statements(evidence.get(key))]
    hypotheses = [s for key in _HYPO_KINDS for s in _statements(evidence.get(key))]
    return (facts, inferences, hypotheses)


# ---------------------------------------------------------------------------
# 报告红线（自检器）

def report_guard(md_text):
    """红线行级扫描 → 「行 N: 类型」清单（空 = 通过；宽松正则+人工复核口径）。
    五类检测（brief 四违例之②拆「无区间发生率」+「零率表述」两类）：①无分母
    比率 ②无区间发生率（rate_without_ci——率陈述有分母而无区间/上界/CI）
    ③零率表述（整零边界正则）④PMU 异常×SDC 无「证据非判据」⑤无 E2+ 前缀
    根因定位。违例消息自身不含禁字样（自检段不自触发）。"""
    if not isinstance(md_text, str):
        raise ValueError(f"md_text 必须为 str: {type(md_text).__name__}")
    out = []
    for i, line in enumerate(md_text.splitlines(), 1):
        pct = _PCT_RE.search(line)
        has_denom = "n=" in line or "/" in line
        has_interval = ("[" in line or "上界" in line or "CI" in line)
        if pct and not has_denom:
            out.append(f"行 {i}: 无分母比率（宽松正则命中百分数，"
                       "请人工复核——报告红线要求凡比率必带分母，v5 §9.6）")
        if _RATE_VALUE_RE.search(line) and has_denom and not has_interval:
            out.append(f"行 {i}: 无区间发生率（率陈述有分母而无置信区间或上界，"
                       "v5 §9.6 每比率须带区间——宽松匹配请人工复核）")
        if _ZERO_RATE_RE.search(line):
            out.append(f"行 {i}: 零率表述（k=0 须写上界，不得作零率声明，v5 §9.6）")
        if "PMU 异常" in line and "SDC" in line and "证据非判据" not in line:
            out.append(f"行 {i}: PMU 异常与 SDC 同现且无「证据非判据」字样"
                       "（PMU 计数为证据非判据，v5 §11.5）")
        if "根因已定位" in line and not any(e in line for e in ("E2", "E3", "E4")):
            out.append(f"行 {i}: 无 E2/E3/E4 前缀的根因定位表述"
                       "（E2+ 干预证据前不得定位性结论，v5 §11.8）")
    return out


# ---------------------------------------------------------------------------
# 报告段渲染

def _demote_line(line):
    """Markdown 标题降一级（## → ###、### → ####）——嵌入 T2/T4 段落用。"""
    return "#" + line if line.startswith("##") else line


def _annotate_line(line):
    """T4 输出嵌入时的红线清洁化：超阈行补分母定义；PMU 异常×SDC 行补
    「证据非判据」（本模块生成文本自检通过的机械保障，语义不变）。"""
    if "（|相对差|≥50%）" in line:
        line = line.replace("（|相对差|≥50%）",
                            "（|相对差|≥50%，相对差 = 差值/|对照均值|）")
    if "PMU 异常" in line and "SDC" in line and "证据非判据" not in line:
        line += "（PMU 计数为证据非判据，v5 §11.5）"
    return line


def _embed(md):
    """T2/T4 输出 → 降级标题 + 逐行红线清洁化。"""
    return "\n".join(_annotate_line(_demote_line(l)) for l in md.splitlines())


def _fmt_cpus(xs):
    """核列表显示：[96, 0, 32]（去 Python repr 引号）。"""
    return "[" + ", ".join(str(x) for x in xs) + "]"


def _cut(s, n=120):
    """长注记截断（可读性；省略号显式）。"""
    s = (s or "").strip()
    return s if len(s) <= n else s[:n] + "…"


def _capsule_cell(root, matched):
    """事件清单 capsule 列：任一匹配事件有 ring_window → 相对路径；否则 —。"""
    for ev in matched:
        eid = ev.get("event_id")
        d = os.path.join(root, "events", str(eid), "ring_window")
        if isinstance(eid, str) and os.path.isdir(d):
            return f"✓ events/{eid}/ring_window"
    return "—"


def _counts_line(classified):
    """分类汇总行（每计数带台账分母）。"""
    c = {key: 0 for key in _CLASS_LABEL}
    for _, key in classified:
        c[key] += 1
    n = len(classified)
    return (f"分类汇总：SDC 候选 k={c['sdc_candidate']}、测试污染 {c['pollution']}、"
            f"测试缺陷（TEST_BUG）{c['test_bug']}、定性非 SDC {c['resolved_non_sdc']}、"
            f"非失败行 {c['non_fail']}（分母 = 台账 n={n} 行）"), c


def _hypothesis_evidence(views, hints):
    """位形态视图 → T3 evidence dict（只派生数据可得键，其余不设——不虚报）。"""
    ev = {}
    logical = (views.get("cpu_clustering") or {}).get("logical") or {}
    if logical.get("n") and logical.get("max_share", 0.0) >= 0.5:
        ev["cpu_clustering"] = True
    toks = []
    hint_text = (hints or {}).get("hint", "")
    if "执行/寄存器通路候选" in hint_text:
        toks.append("execute_path")
    if "存储层级候选" in hint_text:
        toks.append("load_path")
    if (views.get("field_clustering") or {}).get("dominant") == "mantissa":
        toks.append("mantissa")
    if toks:
        ev["bit_hints"] = toks
    return ev


def _section_timeline(root, focus, events):
    """时序段（T4）。focus = [(row, cls)]；无 mismatch 类事件 → 整段省略（""）。
    返回 (md, same_window, ctrl_diff)：后两者为推断段陈述（同窗相关/对照差分，
    v5 §11.1-5 归段口径）。"""
    if not focus:
        return "", [], []
    same_window, ctrl_diff = [], []
    L = ["## 3. 时序（v5 §11.5，事件对齐——无事件时本段省略）", ""]
    pmu = os.path.join(root, "monitor", "pmu_core.csv")
    for row, cls in focus:
        ts_raw, label = row.get("ts", "?"), row.get("label", "?")
        matched = _match_events(events, row)
        L.append(f"### 事件 {ts_raw} {label}（分类：{_CLASS_LABEL.get(cls, cls)}）")
        mm = has_mismatch_in_window(events, ts_raw) if _parse_ts(ts_raw) else None
        if mm is None:
            L.append("- 事件台账事实：has_mismatch=未知（无 spool/events.jsonl——"
                     "判读规则②③不裁，v5 §11.5）")
        else:
            L.append(f"- 事件台账事实：窗口 [-{WINDOW_S:g}s,+{WINDOW_S:g}s] "
                     f"has_mismatch={mm}（源自 spool/events.jsonl mismatch 事件"
                     "存在性，T4 评审移交注入）")
            if mm is True:
                same_window.append(f"{ts_raw} {label}：事件窗口内 mismatch 存在"
                                   "（has_mismatch=True，台账事实）")
        cap = _capsule_cell(root, matched)
        if cap != "—":
            L.append(f"- capsule：{cap[2:]} 存在（120s 环窗冻结，可重放）")
        elif row.get("dir"):
            L.append(f"- 证据目录：{row['dir']}"
                     f"（{'存在' if os.path.exists(row['dir']) else '不可达（已迁移/清除）'}；"
                     "无 ring_window capsule）")
        else:
            L.append("- 无 capsule/证据目录")
        if not os.path.isfile(pmu):
            L.append("- 监控数据缺失（monitor/pmu_core.csv 不存在）——时序差分不可算")
            L.append("")
            continue
        try:
            header, rows_w, degraded, _, _, _ = tl._slice_window(
                pmu, ts_raw, WINDOW_S, WINDOW_S, required_cols=("ts", "core"))
        except ValueError as e:
            L.append(f"- PMU 窗口读取失败：{e}")
            L.append("")
            continue
        L.append(f"- PMU 对齐窗口：pmu_core.csv 窗内 {len(rows_w)} 行"
                 f"（ts 不可解析降级 {degraded} 行）")
        suspect = []
        for ev in matched:
            for b in _event_blocks(ev):
                cpu = b.get("cpu")
                if isinstance(cpu, bool) or not isinstance(cpu, (int, str)):
                    continue
                if cpu not in suspect:
                    suspect.append(cpu)
        if not suspect:
            L.append("- 嫌疑核来源缺失（匹配事件的 mismatch 块无 cpu 字段）——"
                     "嫌疑/对照差分不可算（v5 §11.5 需拓扑匹配对照核）")
            L.append("")
            continue
        s_keys = {tl._cpu_key(c) for c in suspect}
        core_disp = {}
        for _, r in rows_w:
            k = tl._core_key(r.get("core"))
            core_disp.setdefault(k, r.get("core"))
        c_keys = set(core_disp) - s_keys
        if not rows_w or not c_keys:
            L.append(f"- 嫌疑核 {_fmt_cpus(suspect)} 在监控窗口内无样本行/无对照核"
                     "——差分不可算")
            L.append("")
            continue
        s_rows, c_rows, mux = tl._core_select(
            rows_w, s_keys, c_keys, "percent_covered" in header)
        if not s_rows or not c_rows:
            L.append(f"- 嫌疑核 {_fmt_cpus(suspect)} 或对照核窗内无样本行——差分不可算")
            L.append("")
            continue
        control = [k[1] if k[0] == "num" else core_disp[k]
                   for k in sorted(c_keys, key=repr)]
        winx = tl._extract_window({"event_ts": ts_raw,
                                   "before_s": WINDOW_S, "after_s": WINDOW_S})
        diff = tl._diff_from_slice(header, s_rows, c_rows, degraded, mux,
                                   suspect, control, winx, mm)
        verdict = tl.timeline_verdict(diff)
        if diff.get("flags"):
            ctrl_diff.append(f"{ts_raw} {label}：嫌疑核 {_fmt_cpus(suspect)} vs "
                             f"对照核 {_fmt_cpus(control)} "
                             f"超阈 {', '.join(diff['flags'])}"
                             "（相对差 = 差值/|对照均值|，判读见时序段）")
        same_window.append(_annotate_line(
            f"{ts_raw} {label}：{verdict['reading']}（同窗相关，非因果）"))
        body = tl.format_timeline(None, diff, verdict).splitlines()
        L.extend(_embed("\n".join(body[1:])).splitlines())   # 弃 T4 自有 ## 头
        L.append("")
    return "\n".join(L), same_window, ctrl_diff


# ---------------------------------------------------------------------------
# generate_report

def generate_report(data_root, event_id=None, out_path=None):
    """数据根 → 五段 Markdown 诊断报告（+ 红线自检段）。见模块 docstring。"""
    root = str(data_root)
    _require(os.path.isdir(root), f"数据根不存在或不是目录: {root!r}")

    # ---- 装载 ----
    ledger_path = os.path.join(root, "events", "ledger.csv")
    rows = sdc_legacy_adapter.parse_ledger(ledger_path) \
        if os.path.isfile(ledger_path) else []
    events, n_bad = _load_events_jsonl(os.path.join(root, "spool", "events.jsonl"))
    exposure = _load_exposure(root)
    n_iter, n_hours = _exposure_values(exposure)

    # ---- 单事件模式定位 ----
    focus_event, mode = None, "全量（event_id 未指定）"
    if event_id is not None:
        focus_event = next((e for e in (events or [])
                            if isinstance(e, dict) and e.get("event_id") == event_id), None)
        if focus_event is None:
            raise ValueError(f"event_id 未见于 spool/events.jsonl: {event_id!r}"
                             f"（事件流 {'缺失' if events is None else len(events)} 条）")
        mode = f"单事件深挖（event_id={event_id}，run={focus_event.get('run_id', '?')}）"

    # ---- 分类 ----
    classified = [(row, classify_row(row)) for row in rows]
    counts_line, c = _counts_line(classified)
    k = c["sdc_candidate"]
    matched_by_row = {id(row): _match_events(events, row) for row, _ in classified}

    # ---- mismatch 块（位形态/T3 输入）----
    if focus_event is not None:
        blocks = _event_blocks(focus_event)
        n_mm_total = 1 if focus_event.get("event_type") == "sdc_mismatch" else 0
        n_mm_excluded = 0            # 单事件模式不过滤（用户显式指定该事件）
    else:
        blocks, n_mm_total, n_mm_excluded = _collect_blocks(events)
    views = sdc_bitview.bit_views(blocks)
    hints = sdc_bitview.classification_hint(views)

    # ---- 时序段聚焦事件：mismatch 类台账行（含被排除类——红色事件仍对齐诊断）----
    focus_rows = [(row, cls) for row, cls in classified
                  if row.get("test") and (row.get("rc") or "") in FAIL_RCS]
    if focus_event is not None:
        mrow = next((row for row, _ in classified
                     if any(e is focus_event for e in matched_by_row[id(row)])), None)
        if mrow is not None:
            focus_rows = [(mrow, classify_row(mrow))]
        else:                                    # 事件无对应台账行（monitor 源等）
            edt = _ev_dt(focus_event)
            focus_rows = [({"ts": edt.strftime("%Y-%m-%d %H:%M:%S") if edt else "?",
                            "label": focus_event.get("run_id", "?"), "rc": "",
                            "test": "", "seed": "", "note": "", "dir": "", "retests": ""},
                           (focus_event.get("classification") or {}).get("primary", "?"))]

    # ---- 概述：事件清单 ----
    L1 = ["## 1. 概述（事件与分母/暴露，v5 §9.5/§9.6）", ""]
    L1.append(f"- 生成时刻：{datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')}"
              f"；数据根：{root}；模式：{mode}")
    src_bits = [f"events/ledger.csv {len(rows)} 行" if os.path.isfile(ledger_path)
                else "events/ledger.csv 缺失（零事件路径）"]
    src_bits.append("spool/events.jsonl "
                    + (f"{len(events)} 条（坏行 {n_bad}）" if events is not None
                       else "缺失（has_mismatch=未知，判读规则②③不裁）"))
    src_bits.append("spool/exposure.json " + ("在场" if exposure else
                                              "缺失（暴露量不可得）"))
    L1.append("- 数据源：" + "；".join(src_bits))
    L1.append("- 工具链：sdc_report（M4 T5）× sdc_stats/sdc_bitview/sdc_hypothesis/"
              "sdc_timeline（T1-T4）")
    L1.append("")
    L1.append(f"### 事件清单与分类（台账 n={len(rows)} 行）")
    L1.append("| ts | run | test | rc | 分类 | capsule |")
    L1.append("|---|---|---|---|---|---|")
    for row, cls in classified:
        L1.append(f"| {row.get('ts', '?')} | {row.get('label', '?')} | "
                  f"{_cut(row.get('test') or '—', 40)} | {row.get('rc') or '—'} | "
                  f"{_CLASS_LABEL[cls]} | {_capsule_cell(root, matched_by_row[id(row)])} |")
    L1.append("")
    L1.append(counts_line)
    if k == 0:
        L1.append("- 零事件：无 SDC 候选事件检出——上界表述见下表（不作零率声明，v5 §9.6）")
    L1.append("")
    L1.append("### 污染事件单列（v5 §9.5 INVALID——不入 SDC 率分母，必须报告不静默丢弃）")
    polluted = [(row, cls) for row, cls in classified if cls == "pollution"]
    if polluted:
        for row, _ in polluted:
            L1.append(f"- {row.get('ts', '?')} {row.get('label', '?')}"
                      f"（test={row.get('test') or '—'}, rc={row.get('rc') or '—'}）："
                      f"{_cut(row.get('note'))}")
    else:
        L1.append("- 无测试污染事件（台账 note 无「测试污染」字样）")
    others = [(row, cls) for row, cls in classified
              if cls in ("test_bug", "resolved_non_sdc")]
    if others:
        seg = "、".join(f"{row.get('ts', '?')} {row.get('label', '?')}（{_CLASS_LABEL[cls]}）"
                        for row, cls in others)
        L1.append(f"- 其他不入 SDC 率分母事件（v5 §9.5 单列）：{seg}")
    L1.append("")
    L1.append("### 分母与暴露（v5 §9.6：k/n 点估计 + 95% CI；k=0 只报上界）")
    L1.append(rate_table([row for row, cls in classified if cls == "sdc_candidate"],
                         n_iter, n_hours).rstrip())
    if exposure:
        L1.append(f"- 暴露量来源：{exposure.get('source', '—')}"
                  f"（valid_iterations={n_iter if n_iter is not None else '不可得'}, "
                  f"valid_core_hours="
                  f"{n_hours if n_hours is not None else '不可得'}）")
    else:
        L1.append("- 暴露量不可得：无 spool/exposure.json（loop-count 日汇总缺失）——"
                  "仅事件计数，不得据此作零率或无 SDC 声明（v5 §9.6 报告纪律）")
    L1.append("")

    # ---- 时序段（T4；先算——推断段陈述来源）----
    timeline_md, same_window, ctrl_diff = _section_timeline(root, focus_rows, events)

    # ---- 概述：证据分级三段（v5 §11.1-5）----
    mm_counts = views.get("counts") or {}
    _exp_desc = "、".join(x for x in (
        f"{n_iter} 迭代" if n_iter else None,
        f"{n_hours:g} 核心小时" if n_hours else None) if x) or \
        "不可得（无 loop-count 日汇总）"
    facts_src = [
        counts_line + "；spool/events.jsonl "
        + (f"{len(events)} 条（sdc_mismatch {n_mm_total}、"
           f"坏行 {n_bad}）" if events is not None else "缺失"),
        f"mismatch 块 {len(blocks)} 件入位形态段"
        + (f"（sdc_mismatch 事件 {n_mm_total} 条，排除污染/TEST_BUG {n_mm_excluded} 条）"
           if n_mm_total else "（无 sdc_mismatch 事件或事件不带 mismatch 块）"),
        f"暴露量：{_exp_desc}",
    ]
    recomputed = []
    if blocks:
        recomputed.append(f"mismatch 块 xor 重算：xor 可算 {mm_counts.get('n_xor', 0)} 件"
                          f"（其中由 actual^expected 重算 "
                          f"{mm_counts.get('n_xor_recomputed', 0)}）")
        recomputed.append("popcount 口径：以 xor 重算为准（与字段不一致时注记，T2）")
        n_xor_conflict = sum(1 for n in (views.get("notes") or [])
                             if "xor_mask_hex ≠ actual^expected" in n)
        if n_xor_conflict:
            recomputed.append(f"xor 冲突件 {n_xor_conflict}（以 xor_mask_hex 为准——T2 口径）")
    evidence = {
        "raw_observations": facts_src,
        "重算结果": recomputed,
        "干预记录": [],            # 台账 retests 为同命令复测，非单变量干预——不虚报 E2
        "同窗相关": same_window,
        "对照差分": ctrl_diff,
        "未验证解释": [],
        "假设排序": [],
        "探针建议": [],
    }

    # ---- 位形态段（T2）----
    L2 = ["## 2. 位形态（v5 §11.3）", "",
          f"- mismatch 块来源：spool/events.jsonl sdc_mismatch 事件 n={n_mm_total}；"
          f"排除（测试污染/TEST_BUG）{n_mm_excluded} 条；入场 {len(blocks)} 块"
          + ("（单事件模式：仅聚焦事件）" if focus_event is not None else "")]
    L2.append(_embed(sdc_bitview.format_bit_report(views, hints)))

    # ---- 假设排序段（T3）----
    matrix = sdc_hypothesis.load_matrix()
    ev3 = _hypothesis_evidence(views, hints)
    scored = sdc_hypothesis.score_hypotheses(ev3, matrix)
    level = sdc_hypothesis.evidence_level([])     # 无干预记录 → E1（如实）
    L4 = ["## 4. 假设排序（v5 §11.2/§11.8）", ""]
    derived = []
    if "cpu_clustering" in ev3:
        logical = (views.get("cpu_clustering") or {}).get("logical") or {}
        derived.append(f"cpu_clustering=True（位视图 logical 级最大占比 "
                       f"{next(iter((logical.get('counts') or {}).values()), 0)}"
                       f"/{logical.get('n', 0)}，≥0.5 判据）")
    if "bit_hints" in ev3:
        derived.append(f"bit_hints={ev3['bit_hints']}（T2 候选域提示 + 位段主导）")
    L4.append("- 证据键派生（数据驱动，无数据源不设）：" + ("；".join(derived) or
              "无可派生证据键（无 mismatch 块或信号不足）——全域 score=0，"
              "排序仅为矩阵默认序，非候选裁定"))
    L4.append("- 未设键：n1_immune/multicore_only/workset_dependent 等无数据源，"
              "不设——不虚报；台账 retests 为同命令复测，不计为干预（证据等级 E1）")
    L4.append("")
    L4.append(_embed(sdc_hypothesis.format_ranking(scored, level)))

    # ---- 建议段（下一项最有信息量实验 = 各域第一名探针）----
    L5 = ["## 5. 建议（下一项最有信息量实验）", ""]
    if scored:
        top = scored[0]
        L5.append(f"- 下一项最有信息量实验（假设排序第一名 {top['domain']}，"
                  f"score {top['score']:+d}）：{top['probe'] or '—'}")
    else:
        L5.append("- 假设矩阵为空——无探针建议可生成")
    L5.append(f"- 当前证据等级 {level}（无干预记录）——探针目标：以单变量反事实干预把"
              "证据提到 E2；E2 前一切措辞限「相关/候选」（v5 §11.8）")
    L5.append("- 各域第一名反事实探针（按假设排序）：")
    L5.append("| # | 候选域 | score | 第一名反事实探针 |")
    L5.append("|---|---|---|---|")
    for i, e in enumerate(scored, 1):
        L5.append(f"| {i} | {e['domain']} | {e['score']:+d} | {e['probe'] or '—'} |")
    L5.append("")

    # ---- 三段渲染回填（假设/探针建议归段；T4 判读进推断段）----
    if scored:
        evidence["假设排序"] = [
            f"假设排序第一名 {scored[0]['domain']}（score {scored[0]['score']:+d}）"
            "——未验证候选，排序只决定下一项实验先做哪个（T3 口径）"]
        evidence["探针建议"] = [
            f"下一项最有信息量实验：{scored[0]['probe'] or '—'}"
            f"（{scored[0]['domain']} 反事实探针）"]
    if (hints.get("hint") or "").strip():
        evidence["未验证解释"] = [f"位形态候选域提示（confidence: hint，非定性）："
                                 f"{_cut(hints['hint'], 160)}"]
    facts, inferences, hypotheses = fact_inference_hypothesis_sections(evidence)
    L1.append("### 证据分级（v5 §11.1-5：事实/推断/假设严格分离）")
    L1.append("**事实（原始观测与重算结果）**")
    if facts:
        L1.extend(f"- {s}" for s in facts)
    else:
        L1.append("- 无")
    L1.append("**推断（同窗相关与对照差分——相关非因果）**")
    if inferences:
        L1.extend(f"- {s}" for s in inferences)
    else:
        L1.append("- 无同窗相关/对照差分陈述（无事件或时序段降级——不虚报相关性）")
    L1.append("**待验证假设（假设排序与探针建议——未验证）**")
    if hypotheses:
        L1.extend(f"- {s}" for s in hypotheses)
    else:
        L1.append("- 无（无候选域提示与排序输入）")
    L1.append("- 干预记录：无（台账 retests 为同命令复测，非单变量干预——不虚报 E2）")
    L1.append("")

    md = "\n".join(L1) + "\n" + "\n".join(L2) + "\n" + \
        (timeline_md + "\n" if timeline_md else "") + "\n".join(L4) + "\n" + \
        "\n".join(L5) + "\n"

    # ---- 红线自检 ----
    viol = report_guard(md)
    md += ("## 报告自检（report_guard 红线扫描）\n\n"
           f"- 自检结果：{len(viol)} 违例（红线：①无分母比率 ②无区间发生率 "
           "③零率表述 ④PMU 异常×SDC 无「证据非判据」⑤无 E2+ 前缀根因定位）\n")
    for v in viol:
        md += f"- {v}\n"
    if out_path is not None:
        os.makedirs(os.path.dirname(os.path.abspath(str(out_path))), exist_ok=True)
        with open(str(out_path), "w", encoding="utf-8") as f:
            f.write(md)
    return md


# ---------------------------------------------------------------------------
# CLI

def _main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(
        description="sdc_report——离线诊断报告生成器（M4 退出标准载体，v5 §9/§11）")
    ap.add_argument("--data-root", default=None,
                    help="数据根（默认 $SDC_EXCITE_REPRODUCE_DIR/"
                         "$SDC_CAMPAIGN_DIR/~/sdc-excite-reproduce）")
    ap.add_argument("--event-id", default=None,
                    help="单事件深挖模式（spool/events.jsonl 的 event_id）")
    ap.add_argument("-o", "--output", default=None, help="报告输出路径（缺省 stdout）")
    args = ap.parse_args(argv)
    root = args.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") or \
        os.environ.get("SDC_CAMPAIGN_DIR") or os.path.expanduser("~/sdc-excite-reproduce")
    md = generate_report(root, event_id=args.event_id, out_path=args.output)
    if args.output:
        print(f"报告已写入 {args.output}（{len(md)} 字符）")
    else:
        print(md)
    return 0


if __name__ == "__main__":
    sys.exit(_main())

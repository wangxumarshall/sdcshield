#!/usr/bin/env python3
"""sdc_timeline.py — 事件对齐时序差分 + 嫌疑/对照核 PMU 差分视图（v5 §11.5，M4 Task 4）。
stdlib only。消费 M1 采集器 CSV（monitor.csv/percore.csv/pmu_core.csv，ts 首列
"%Y-%m-%d %H:%M:%S"）+ 事件时刻，产出四件：

  align_window(metrics_csv, event_ts, before_s=60, after_s=60) -> dict
      通用 CSV 窗口切片：ts 首列解析（失败行跳过 + degraded 计数，不抛），
      dt_s = 行时刻 - 事件时刻（秒），边界含端点（-before_s ≤ dt_s ≤ +after_s）。
      返回 {"columns"(去 ts), "rows"({"dt_s", ...列值}), "n", "degraded",
      "window"(echo 原参数)}。列值三态：数字串→float、空→None、其余原串。

  cpu_to_topology_label(cpu, topology_root=None) -> str|None
      logical cpu 号 → perf --per-core 拓扑标签 "S<pkg>-D<die>-C<core>"（M4 终审
      移交项：report 差分把 mismatch cpu 接续到 M1 pmu_core.csv 标签形 core 列）。
      S=physical_package_id 原值、C=core_id、D=die_id 值（文件缺则 0——perf 6.6
      实测口径，见函数 docstring 实证）；cpu 目录/必需文件缺失或输入非核号 →
      None（不抛，调用方注记「标签不可接续」降级）。topology_root 缺省
      /sys/devices/system/cpu（测试注入 fixture 根）。

  suspect_vs_control_diff(pmu_csv, suspect_cpus, control_cpus, window,
                          has_mismatch=None) -> dict
      v5 §11.5——以事件为 0 切窗后，嫌疑核均值 vs 对照核均值差分表。派生指标
      （列在则算，缺列进 missing 注记，绝不零填充）：
        ipc = inst_retired/cycles
        stall_ratio / stall_frontend_ratio / stall_backend_ratio = */cycles
        l1d/l2d/llc_mpki、dtlb_wpk、remote_kpi、br_mpki = */inst_retired×1000
      判读辅助量：事件前（dt<0）相位对半拆早/晚期——嫌疑核 stall 上升
      （晚期/早期 ≥ RISE_RATIO）且对照未同步 → suspect_pre_rise；对照核同升
      → control_sync；晚期仅 1 拍超阈 → "单拍毛刺可能"注记（判读降信度）。
      percent_covered < 80 的行计 mux_degraded_rows（v5 §11.5 PMU 证据降级，
      计但不禁）。返回 {"metrics", "diffs", "rel_diffs", "suspect_means",
      "control_means", "counts"(各指标 n 嫌/照), "missing", "flags"
      (|相对差|≥FLAG_REL), "phases", "suspect_pre_rise", "control_sync",
      "n_suspect_rows", "n_control_rows", "mux_degraded_rows",
      "ts_degraded_rows", "suspect_cpus", "control_cpus", "window",
      "has_mismatch", "notes"}。

  timeline_verdict(diff) -> {"rule", "reading", "caveat"}
      v5 §11.5 判读四规则（优先级：④对照同步 > ①嫌疑前升 > ③canary/异常 >
      ②不否定；"嫌疑前升"本身要求对照未同步，④是①的改判）：
        ① 嫌疑核事件前 stall 上升、对照核未同步 → 时序/资源压力候选（亦可能
           调度/中断）。对照核读数按实际分档（评审 Important #2：不再硬编码
           "平稳"）：晚期/早期 <FLAT_MAX_RATIO="平稳"；[FLAT_MAX_RATIO,
           RISE_RATIO)="轻度上升（未达同步阈值）"——caveat 追加共享环境未排
           除、读数弱化；≥RISE_RATIO 走规则④。比值不可判（早期均值为零等）
           如实写"比值不可判"。
        ② mismatch 无 PMU 异常 → 不否定 SDC（故障可能短于采样窗或不影响计数事件）
        ③ PMU 异常无 mismatch → canary，不计为 SDC
        ④ 对照核同步异常 → 共享环境候选（仅嫌疑核异常才更支持 core-local）
      has_mismatch（事件台账事实，非 PMU 可得）由调用方经 diff["has_mismatch"]
      或函数入参注入：True→②可触发、False→③可触发、None（缺省）→两规则不
      裁，只报异常/无异常。每输出 caveat 必含"同窗相关≠因果"。

  format_timeline(windows, diff, verdict) -> str
      Markdown 段（§12.3 口径：分子/分母同示、缺列单独显示不零填充）。相位
      比值为 None（早期半窗均值恰 0，比值未定义）渲染 "——绝不渲染成 0
      （评审 Important #1）。

core 列匹配（诚实边界）：M1 as-built pmu_core.csv 的 core 列为 perf 拓扑标签
（"S36-D0-C0"）——须传标签原串；纯数字核号只匹配数字 core 列（fixture/派生
表），数字 0 不误配 "-C0"（尾部是 die 内 core 号，非逻辑 CPU 号）。
cpu_to_topology_label(cpu, topology_root=None) -> str|None 把 logical cpu 号
接续成该标签（M4 终审移交：report 差分把 mismatch cpu join 到标签形 core 列；
拓扑不可得 → None，调用方注记降级不抛）——格式实证见该函数 docstring
（2026-09-27 本机 strace + 真实 pmu_core.csv：D=die_id 值、缺则 0，非 cluster_id）。

流式读取（评审 Important #3）：CSV 行级 parse→判窗→留行，绝不全量物化——
94MB 真实 pmu_core.csv 全量 dict 物化峰值 RSS ~1.1GB（本机有 OOM 前科），
流式窗内行仅数 MB。CLI 对 pmu_core.csv 单次读取共享给窗口段与差分。

用法：python3 sdc_timeline.py --pmu pmu_core.csv --event "2026-09-26 12:00:00" \
         --suspect 96 --control 0 32 [--before 60 --after 60] \
         [--metrics monitor.csv ...] [--has-mismatch yes|no|unknown]
"""
import csv
import datetime
import os
import sys

MUX_THRESHOLD = 80.0        # 同 sdc_collector_pmu（percent<80 → multiplex_degraded）
RISE_RATIO = 1.5            # 事件前晚期/早期 ≥ 此值判"上升"
FLAT_MAX_RATIO = 1.25       # 对照核晚期/早期 < 此值判"平稳"（之上未达同步则注记）
FLAG_REL = 0.5              # |相对差| ≥ 50% 判 PMU 异常指标
_EPS = 1e-9

# (指标名, 分子列, 分母列, 系数)——列在则算、缺列注记（v5 §11.5 指标面）
METRICS = (
    ("ipc", "inst_retired", "cycles", 1.0),
    ("stall_ratio", "exe_stall_cycle", "cycles", 1.0),
    ("stall_frontend_ratio", "stall_frontend", "cycles", 1.0),
    ("stall_backend_ratio", "stall_backend", "cycles", 1.0),
    ("l1d_mpki", "l1d_cache_refill_rd", "inst_retired", 1000.0),
    ("l2d_mpki", "l2d_cache_refill_rd", "inst_retired", 1000.0),
    ("llc_mpki", "ll_cache_miss_rd", "inst_retired", 1000.0),
    ("dtlb_wpk", "dtlb_walk", "inst_retired", 1000.0),
    ("remote_kpi", "remote_access", "inst_retired", 1000.0),
    ("br_mpki", "br_mis_pred", "inst_retired", 1000.0),
)
METRIC_DEFS = {m[0]: m for m in METRICS}
RISE_CHAIN = ("stall_ratio", "stall_backend_ratio", "stall_frontend_ratio")

_TS_FORMATS = ("%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M:%S",
               "%Y-%m-%d %H:%M:%S.%f", "%Y-%m-%dT%H:%M:%S.%f")


# ---------------------------------------------------------------------------
# 基础件：时间解析 / 流式 CSV / 值三态转换 / 核匹配

def _naive(dt):
    """tz-aware → 本地 naive（M1 数据全 naive；有 tz 时以本地口径折算）。"""
    if dt.tzinfo is not None:
        dt = dt.astimezone().replace(tzinfo=None)
    return dt


def _parse_ts(v):
    """ts → datetime；不可解析（含空/None/非串）→ None，不抛。"""
    if isinstance(v, datetime.datetime):
        return _naive(v)
    if not isinstance(v, str):
        return None
    s = v.strip()
    if not s:
        return None
    try:
        return _naive(datetime.datetime.fromisoformat(s))
    except ValueError:
        pass
    for fmt in _TS_FORMATS:
        try:
            return datetime.datetime.strptime(s, fmt)
        except ValueError:
            continue
    return None


def _fmt_ts(dt):
    return dt.strftime(_TS_FORMATS[0])


def _fmt_g(v, spec=".3g"):
    """数值格式化；None（比值未定义/不可判）→ "—"——绝不把 None 渲染成 0
    （评审 Important #1：早期半窗均值恰 0 时比值为 None，直接 format 会
    TypeError，且渲染 0 是失实）。"""
    return "—" if v is None else format(v, spec)


def _val(v):
    """CSV 单元格三态：空→None；数字串→float；其余→原串。"""
    if v is None:
        return None
    if isinstance(v, str):
        if not v.strip():
            return None
        try:
            return float(v)
        except ValueError:
            return v
    return v


def _stream_csv(path, lo_ts=None, hi_ts=None):
    """流式读 CSV → 生成器：首 yield 列头（strip 后 list），其后每数据行
    yield (ts, row_dict)。行级判窗：lo_ts/hi_ts（含端点）给定时，窗外可解
    析行在流内即弃（不物化——内存关键路径）；ts 不可解析行无法判窗，仍
    yield (None, row) 由调用方计 degraded 跳过。无窗参数全行 yield（兼容
    全量口径）。路径不可读/空文件 → ValueError（首次 next 即抛）。"""
    if not isinstance(path, (str, os.PathLike)):
        raise ValueError(f"CSV 路径必须为 str/Path: {path!r}")
    try:
        f = open(path, newline="")
    except OSError as e:
        raise ValueError(f"CSV 不可读: {path}: {e}") from None

    def _gen():
        with f:
            reader = csv.reader(f)
            try:
                header = [h.strip() for h in next(reader)]
            except StopIteration:
                raise ValueError(f"空 CSV（无列头）: {path}") from None
            yield header
            ts_idx = header.index("ts") if "ts" in header else None
            for fields in reader:
                if not fields:
                    continue                      # 空行
                t = _parse_ts(fields[ts_idx]) \
                    if ts_idx is not None and ts_idx < len(fields) else None
                if t is not None:
                    if lo_ts is not None and t < lo_ts:
                        continue
                    if hi_ts is not None and t > hi_ts:
                        continue
                yield (t, {h: (fields[i] if i < len(fields) else None)
                           for i, h in enumerate(header)})
    return _gen()


def _slice_window(path, event_ts, before_s, after_s,
                  required_cols=("ts",), first_col_ts=False):
    """单次流式扫描 + 事件窗切片（内部共用件——align_window/
    suspect_vs_control_diff/CLI 单读共享）→
    (header, [(dt_s, row)], degraded, ev, before_s, after_s)。
    窗外行不物化；ts 不可解析行不进列表、计 degraded。"""
    ev = _parse_ts(event_ts)
    if ev is None:
        raise ValueError(f"event_ts 不可解析: {event_ts!r}")
    b, a = _validate_span(before_s, after_s)
    it = _stream_csv(path, lo_ts=ev - datetime.timedelta(seconds=b),
                     hi_ts=ev + datetime.timedelta(seconds=a))
    header = next(it)
    if first_col_ts and (not header or header[0] != "ts"):
        raise ValueError(f"CSV 首列必须为 ts（M1 采集器口径）: {path!r} "
                         f"首列={header[0] if header else None!r}")
    for c in required_cols:
        if c not in header:
            raise ValueError(f"CSV 缺必需列 {c!r}: {path!r} 列头={header}")
    rows, degraded = [], 0
    for t, r in it:
        if t is None:
            degraded += 1
            continue
        dt = (t - ev).total_seconds()
        if _in_window(dt, ev, b, a):
            rows.append((dt, r))
    return header, rows, degraded, ev, b, a


def _cpu_key(c):
    """核标识归一：int/纯数字串 → ("num", int)；其余串 → ("label", 原串)。"""
    if isinstance(c, bool) or not isinstance(c, (int, str)):
        raise ValueError(f"核标识必须为 int/str: {c!r}")
    if isinstance(c, int):
        return ("num", c)
    s = c.strip()
    if s.isdigit():
        return ("num", int(s))
    return ("label", s)


def _core_key(core):
    s = (core or "").strip()
    if s.isdigit():
        return ("num", int(s))
    return ("label", s)


def _validate_cpus(cpus, what):
    if isinstance(cpus, (str, bytes)) or not isinstance(cpus, (list, tuple)):
        raise ValueError(f"{what}必须为 int/str 列表: {cpus!r}")
    if len(cpus) == 0:
        raise ValueError(f"{what}列表为空——差分需要至少一核")
    return [_cpu_key(c) for c in cpus]


def _validate_span(before_s, after_s):
    for v, name in ((before_s, "before_s"), (after_s, "after_s")):
        if isinstance(v, bool) or not isinstance(v, (int, float)) or v < 0:
            raise ValueError(f"{name}必须为 ≥0 的数: {v!r}")
    return float(before_s), float(after_s)


def _in_window(dt, ev, before_s, after_s):
    return -before_s - _EPS <= dt <= after_s + _EPS


def _extract_window(window):
    """window 入参归一：align_window 输出（含 "window" 键）或裸窗口 dict →
    {"event_ts": datetime, "before_s": float, "after_s": float, "event_ts_raw": str}。"""
    if not isinstance(window, dict):
        raise ValueError(f"window 必须为 dict（align_window 输出或裸窗口）: {window!r}")
    w = window.get("window") if isinstance(window.get("window"), dict) else window
    if "event_ts" not in w:
        raise ValueError("window 缺 event_ts（事件时刻）")
    ev = _parse_ts(w["event_ts"])
    if ev is None:
        raise ValueError(f"window event_ts 不可解析: {w['event_ts']!r}")
    b, a = _validate_span(w.get("before_s", 60), w.get("after_s", 60))
    raw = w["event_ts"] if isinstance(w["event_ts"], str) else _fmt_ts(ev)
    return {"event_ts": ev, "before_s": b, "after_s": a, "event_ts_raw": raw}


def _series(rows, num_col, den_col, scale):
    """行 → [(dt, num/den×scale)]；分子/分母非数值或分母 ≤0 的行不贡献。"""
    out = []
    for dt, r in rows:
        num, den = _val(r.get(num_col)), _val(r.get(den_col))
        if isinstance(num, float) and isinstance(den, float) and den > 0:
            out.append((dt, num / den * scale))
    return out


# ---------------------------------------------------------------------------
# 公开 API

_TOPOLOGY_ROOT = "/sys/devices/system/cpu"


def _read_topology_int(topology_root, cpu, name):
    """<root>/cpuN/topology/<name> → int；文件缺失/不可读/非整数 → None（不抛）。"""
    try:
        with open(os.path.join(str(topology_root), f"cpu{cpu}", "topology", name),
                  encoding="utf-8") as f:
            s = f.read().strip()
    except OSError:
        return None
    try:
        return int(s)
    except ValueError:
        return None


def cpu_to_topology_label(cpu, topology_root=None):
    """logical CPU 号 → perf --per-core 拓扑标签 "S<pkg>-D<die>-C<core>"；
    拓扑不可得（cpu 目录/必需文件缺失或不可解析、输入非核号）→ None——不抛，
    调用方（sdc_report 差分接续）注记「标签不可接续」降级。topology_root 缺省
    /sys/devices/system/cpu（测试注入 fixture 根：根下 cpuN/topology/ 布局）。

    格式实证（2026-09-27 本机，perf 6.6.0-159.4.3.154.oe2403sp4）：strace
    perf stat -x, -a --per-core 逐 cpu 只 open topology/{core_id, die_id,
    physical_package_id} 三文件（不读 cluster_id）；本机无 die_id 文件
    （ENOENT）而真实 monitor/pmu_core.csv 全 128 标签皆 D0——cpu0 → S36-D0-C0
    （其 cluster_id=138）、cpu96 → S8442-D0-C96（cluster_id=12720）——「D=
    cluster_id」与「die 无则两段形 S-C」两设想均被实证否定，按 perf 实测口径：
    D 取 die_id 值、文件缺则 0，恒 S-D-C 三段形。S=physical_package_id 原值
    （本机 36/8442 等固件大数照读，CLAUDE.md 拓扑怪癖「read as-is」）、
    C=core_id（SMT 机同 core 兄弟线程映同标签，与 perf --per-core 聚合一致）。
    容忍数字串核号（mismatch 块 cpu 字段可为 str）。"""
    if isinstance(cpu, bool):
        return None
    if isinstance(cpu, int):
        n = cpu
    elif isinstance(cpu, str) and cpu.strip().isdigit():
        n = int(cpu.strip())
    else:
        return None
    root = _TOPOLOGY_ROOT if topology_root is None else str(topology_root)
    pkg = _read_topology_int(root, n, "physical_package_id")
    core = _read_topology_int(root, n, "core_id")
    if pkg is None or core is None:
        return None
    die = _read_topology_int(root, n, "die_id")
    return f"S{pkg}-D{0 if die is None else die}-C{core}"


def align_window(metrics_csv, event_ts, before_s=60, after_s=60):
    """事件对齐窗口切片（任何 ts 首列 CSV）→ 见模块 docstring。"""
    header, rows, degraded, _, _, _ = _slice_window(
        metrics_csv, event_ts, before_s, after_s,
        required_cols=("ts",), first_col_ts=True)
    return _align_from_slice(header, rows, degraded, event_ts, before_s, after_s)


def _align_from_slice(header, rows, degraded, event_ts, before_s, after_s):
    """已切片行 → align_window 输出（CLI 单读共享件）。"""
    cols = header[1:]
    out = [{**{c: _val(r.get(c)) for c in cols}, "dt_s": dt}
           for dt, r in sorted(rows, key=lambda x: x[0])]
    raw = event_ts if isinstance(event_ts, str) else \
        _fmt_ts(_parse_ts(event_ts))
    return {"columns": cols, "rows": out, "n": len(out), "degraded": degraded,
            "window": {"event_ts": raw, "before_s": before_s, "after_s": after_s}}


def _core_select(rows, s_set, c_set, has_pct):
    """窗内行按核归桶 → (嫌疑行, 对照行, mux_degraded 行数)。"""
    s_rows, c_rows, mux = [], [], 0
    for dt, r in rows:
        if has_pct:
            pct = _val(r.get("percent_covered"))
            if isinstance(pct, float) and pct < MUX_THRESHOLD:
                mux += 1                     # multiplex_degraded：计数+降级注记
        key = _core_key(r.get("core"))
        if key in s_set:
            s_rows.append((dt, r))
        elif key in c_set:
            c_rows.append((dt, r))
    return s_rows, c_rows, mux


def suspect_vs_control_diff(pmu_csv, suspect_cpus, control_cpus, window,
                            has_mismatch=None):
    """嫌疑核 vs 对照核 PMU 差分（v5 §11.5）→ 见模块 docstring。"""
    win = _extract_window(window)
    s_keys = _validate_cpus(suspect_cpus, "嫌疑核")
    c_keys = _validate_cpus(control_cpus, "对照核")
    overlap = {k for k in s_keys} & {k for k in c_keys}
    if overlap:
        raise ValueError(f"嫌疑/对照核重叠（对照必须独立）: {sorted(overlap)!r}")
    s_set, c_set = set(s_keys), set(c_keys)
    header, rows, degraded, _, _, _ = _slice_window(
        pmu_csv, win["event_ts"], win["before_s"], win["after_s"],
        required_cols=("ts", "core"))
    s_rows, c_rows, mux = _core_select(rows, s_set, c_set,
                                       "percent_covered" in header)
    if not s_rows:
        raise ValueError(f"嫌疑核 {list(suspect_cpus)!r} 在窗口内无样本行"
                         f"（core 列值为 perf 拓扑标签时须传标签串）")
    if not c_rows:
        raise ValueError(f"对照核 {list(control_cpus)!r} 在窗口内无样本行")
    return _diff_from_slice(header, s_rows, c_rows, degraded, mux,
                            suspect_cpus, control_cpus, win, has_mismatch)


def _diff_from_slice(header, s_rows, c_rows, ts_degraded, mux,
                     suspect_cpus, control_cpus, win, has_mismatch):
    """已选核行 → 差分 dict（CLI 单读共享件）。win 为 _extract_window 输出。"""
    metrics, missing, means_s, means_c = [], [], {}, {}
    diffs, rels, flags, counts = {}, {}, [], {}
    for name, num_col, den_col, scale in METRICS:
        sv, cv = _series(s_rows, num_col, den_col, scale), \
                 _series(c_rows, num_col, den_col, scale)
        if not sv or not cv:
            missing.append(name)             # 列缺席或窗口内全空——注记不零填充
            continue
        ms = sum(v for _, v in sv) / len(sv)
        mc = sum(v for _, v in cv) / len(cv)
        metrics.append(name)
        means_s[name], means_c[name] = ms, mc
        diffs[name] = ms - mc
        if abs(mc) > _EPS:
            rels[name] = (ms - mc) / abs(mc)
        else:
            rels[name] = None                # 对照近零：相对差无定义
        flagged = (rels[name] is not None and abs(rels[name]) >= FLAG_REL) or \
                  (rels[name] is None and abs(diffs[name]) > _EPS)
        if flagged:
            flags.append(name)
        counts[name] = (len(sv), len(cv))

    notes = []
    absent = [m for m in missing
              if METRIC_DEFS[m][1] not in header or METRIC_DEFS[m][2] not in header]
    emptied = [m for m in missing if m not in absent]
    if absent:
        notes.append(f"缺列未算（列头缺席）：{', '.join(absent)}")
    if emptied:
        notes.append(f"缺列未算（列在场但窗口内全空——组轮换/未采集）："
                     f"{', '.join(emptied)}")
    if mux:
        notes.append(f"multiplex_degraded {mux} 行（percent_covered<{MUX_THRESHOLD:.0f}"
                     "，v5 §11.5 PMU 证据降级）")

    # 事件前相位（判读规则①④依据）：stall 链首个可算指标，早/晚期对半
    phases, pre_rise, sync = None, False, False
    rise_metric = next((m for m in RISE_CHAIN if m in metrics), None)
    if rise_metric is None:
        notes.append("事件前上升判读缺列：stall 计数列缺席——pre 相位不可判")
    else:
        _, num_col, den_col, scale = METRIC_DEFS[rise_metric]
        s_pre = [(dt, v) for dt, v in _series(s_rows, num_col, den_col, scale)
                 if dt < 0]
        c_pre = [(dt, v) for dt, v in _series(c_rows, num_col, den_col, scale)
                 if dt < 0]
        if len(s_pre) < 2 or len(c_pre) < 2:
            notes.append(f"事件前样本不足（嫌疑 {len(s_pre)} 拍/对照 {len(c_pre)} 拍，"
                         "需 ≥2）——pre 相位不可判")
        else:
            ss = sorted(s_pre, key=lambda x: x[0])
            cs = sorted(c_pre, key=lambda x: x[0])
            k, ck = len(ss) // 2, len(cs) // 2
            se = sum(v for _, v in ss[:k]) / k
            sl = sum(v for _, v in ss[k:]) / (len(ss) - k)
            ce = sum(v for _, v in cs[:ck]) / ck
            cl = sum(v for _, v in cs[ck:]) / (len(cs) - ck)
            s_ratio = sl / se if se > 0 else None
            c_ratio = cl / ce if ce > 0 else None
            pre_rise = s_ratio is not None and s_ratio >= RISE_RATIO
            sync = c_ratio is not None and c_ratio >= RISE_RATIO
            phases = {"metric": rise_metric,
                      "suspect_pre_early": se, "suspect_pre_late": sl,
                      "control_pre_early": ce, "control_pre_late": cl,
                      "suspect_ratio": s_ratio, "control_ratio": c_ratio,
                      "n_pre_suspect": len(s_pre), "n_pre_control": len(c_pre)}
            if pre_rise:
                n_hot = sum(1 for _, v in ss[k:] if v >= RISE_RATIO * se)
                if n_hot < 2:
                    notes.append(f"单拍毛刺可能：嫌疑核晚期仅 {n_hot} 拍 ≥ "
                                 f"{RISE_RATIO:g}×早期——上升或由孤立尖峰驱动，"
                                 "判读降信度")
            if not sync and c_ratio is not None and c_ratio >= FLAT_MAX_RATIO:
                notes.append(f"对照核事件前轻度上升（×{c_ratio:.2g}，未达同步阈值 "
                             f"{RISE_RATIO:g}）——注记不藏")

    return {"metrics": metrics, "diffs": diffs, "rel_diffs": rels,
            "suspect_means": means_s, "control_means": means_c, "counts": counts,
            "missing": missing, "flags": flags, "phases": phases,
            "suspect_pre_rise": pre_rise, "control_sync": sync,
            "n_suspect_rows": len(s_rows), "n_control_rows": len(c_rows),
            "mux_degraded_rows": mux, "ts_degraded_rows": ts_degraded,
            "suspect_cpus": list(suspect_cpus), "control_cpus": list(control_cpus),
            "window": {"event_ts": win["event_ts_raw"],
                       "before_s": win["before_s"], "after_s": win["after_s"]},
            "has_mismatch": has_mismatch, "notes": notes}


def _control_band(ph):
    """对照核事件前形态分档（评审 Important #2：读数按实际分档，不硬编码
    "平稳"）→ {"band", "desc"}：zero/none/flat/mid。"""
    cr = ph.get("control_ratio")
    if cr is None:
        ce = ph.get("control_pre_early")
        if ce is not None and ce <= 0:
            return {"band": "zero",
                    "desc": "对照核早期均值为零（比值未定义）"}
        return {"band": "none", "desc": "对照核比值不可判"}
    if cr < FLAT_MAX_RATIO:
        return {"band": "flat", "desc": f"对照核平稳（×{cr:.2g}）"}
    return {"band": "mid",
            "desc": f"对照核轻度上升（×{cr:.2g}，未达同步阈值 {RISE_RATIO:g}）"}


def timeline_verdict(diff):
    """判读四规则（v5 §11.5）→ {"rule", "reading", "caveat"}；见模块 docstring。"""
    if not isinstance(diff, dict):
        raise ValueError(f"diff 必须为 suspect_vs_control_diff 输出 dict: {diff!r}")
    flags = diff.get("flags") or []
    pre_rise = bool(diff.get("suspect_pre_rise"))
    sync = bool(diff.get("control_sync"))
    mm = diff.get("has_mismatch")
    mm = mm if isinstance(mm, bool) else None     # None/非 bool → 事件事实未提供
    mux = int(diff.get("mux_degraded_rows") or 0)
    missing = diff.get("missing") or []
    ph = diff.get("phases") or {}
    metric = ph.get("metric", "stall")

    caveat = ["同窗相关≠因果：PMU 计数与事件同窗只支持相关性，"
              "因果须 E2+ 干预/反事实证据（v5 §11.8）"]
    if sync:
        rule = 4
        if pre_rise:
            reading = (f"共享环境候选：对照核与嫌疑核同步异常（事件前 {metric} 同升"
                       f" ×{_fmt_g(ph.get('suspect_ratio'), '.2g')}/"
                       f"×{_fmt_g(ph.get('control_ratio'), '.2g')}）"
                       "——共享环境（供电/温度/内存带宽等）候选；"
                       "仅嫌疑核异常才更支持 core-local 候选")
        else:
            reading = (f"共享环境候选：对照核事件前 {metric} 上升"
                       f"（×{_fmt_g(ph.get('control_ratio'), '.2g')}）而嫌疑核未同升"
                       "——共享环境候选优先于 core-local")
        caveat.append("共享环境与 core-local 须成组移除/隔离等对照实验区分")
    elif pre_rise:
        rule = 1
        ctrl = _control_band(ph)
        reading = (f"时序/资源压力候选：事件前嫌疑核 {metric} 上升"
                   f"（{_fmt_g(ph.get('suspect_pre_early'))}→"
                   f"{_fmt_g(ph.get('suspect_pre_late'))}，"
                   f"×{_fmt_g(ph.get('suspect_ratio'), '.2g')}）、{ctrl['desc']}"
                   "——支持时序/资源压力，亦可能是调度或中断")
        caveat.append("延迟尾部需 latency 通道佐证；调度/中断干扰须排除")
        if ctrl["band"] == "mid":
            caveat.append("对照核亦升（未达同步阈值）——共享环境可能性未排除，"
                          "时序压力读数弱化")
    elif flags:
        if mm is False:
            rule = 3
            reading = (f"canary：嫌疑核 PMU 异常（{', '.join(flags)}）而无 mismatch"
                       "——只进风险/canary，不计为 SDC")
            caveat.append("不计为 SDC——PMU 异常无 mismatch 只作风险信号")
        else:
            rule = None
            reading = (f"嫌疑核 PMU 异常（{', '.join(flags)}）但无事件前上升"
                       "——core-local 候选提示（相关非因果）")
    elif mm is True:
        rule = 2
        reading = ("不否定 SDC：窗口内有 mismatch 但嫌疑核无 PMU 差分异常"
                   "——故障可能短于采样窗或不影响计数事件")
        caveat.append("故障可能短于采样窗或不影响计数事件")
    else:
        rule = None
        reading = "窗口内无嫌疑/对照 PMU 差分异常（mismatch 状态未提供）——不下结论"
    if mux:
        caveat.append(f"含 {mux} 行 multiplex_degraded（percent_covered<"
                      f"{MUX_THRESHOLD:.0f}）——该窗 PMU 证据降级（v5 §11.5）")
    if missing:
        caveat.append(f"{len(missing)} 项指标缺列未算：{', '.join(missing)}")
    return {"rule": rule, "reading": reading, "caveat": "；".join(caveat)}


def _iter_windows(windows):
    """windows 归一：None → []；单个 align 输出 → [("metrics", w)]；dict → 各项。"""
    if windows is None:
        return []
    if not isinstance(windows, dict):
        raise ValueError(f"windows 必须为 align_window 输出或其 dict: {windows!r}")
    if "rows" in windows and "window" in windows:
        return [("metrics", windows)]
    out = []
    for name, w in windows.items():
        if not (isinstance(w, dict) and "rows" in w and "window" in w):
            raise ValueError(f"windows[{name!r}] 非 align_window 输出")
        out.append((str(name), w))
    return out


def format_timeline(windows, diff, verdict):
    """三段 Markdown：对齐窗口 / 差分表（分母同示）/ 判读（含 caveat）。"""
    if diff is not None and not isinstance(diff, dict):
        raise ValueError(f"diff 必须为 dict: {diff!r}")
    if verdict is not None and not isinstance(verdict, dict):
        raise ValueError(f"verdict 必须为 dict: {verdict!r}")
    L = ["## 事件对齐时序差分"]
    wins = _iter_windows(windows)
    if wins:
        L += ["", "### 对齐窗口", "| 通道 | 行数 | 降级行（ts 不可解析） |",
              "|---|---|---|"]
        for name, w in wins:
            L.append(f"| {name} | {w.get('n', 0)} | {w.get('degraded', 0)} |")
    if diff is not None:
        win = diff.get("window") or {}
        L += ["", f"### 嫌疑核 vs 对照核差分（事件 {win.get('event_ts', '?')}，"
              f"窗口 [-{win.get('before_s', '?')}s,+{win.get('after_s', '?')}s]）",
              f"嫌疑核 {diff.get('suspect_cpus', [])}（{diff.get('n_suspect_rows', 0)}"
              f" 行） vs 对照核 {diff.get('control_cpus', [])}"
              f"（{diff.get('n_control_rows', 0)} 行）"]
        metrics = diff.get("metrics") or []
        if metrics:
            L += ["| 指标 | 嫌疑核均值 | 对照核均值 | 差值 | 相对差 | n(嫌/照) |",
                  "|---|---|---|---|---|---|"]
            means_s = diff.get("suspect_means") or {}
            means_c = diff.get("control_means") or {}
            diffs = diff.get("diffs") or {}
            rels = diff.get("rel_diffs") or {}
            counts = diff.get("counts") or {}
            for m in metrics:
                rel = rels.get(m)
                rel_s = f"{rel * 100:+.1f}%" if rel is not None else "—"
                mark = " **▲**" if m in (diff.get("flags") or []) else ""
                L.append(f"| {m}{mark} | {means_s.get(m, 0):.4g} | "
                         f"{means_c.get(m, 0):.4g} | {diffs.get(m, 0):+.4g} | "
                         f"{rel_s} | {counts.get(m, (0, 0))[0]}/"
                         f"{counts.get(m, (0, 0))[1]} |")
        else:
            L.append("无可算指标（全部缺列）")
        flags = diff.get("flags") or []
        if flags:
            L.append(f"超阈指标（|相对差|≥{FLAG_REL:.0%}）：{', '.join(flags)}")
        missing = diff.get("missing") or []
        if missing:
            L.append(f"缺列未算：{', '.join(missing)}")
        mux = int(diff.get("mux_degraded_rows") or 0)
        if mux:
            L.append(f"multiplex_degraded {mux} 行（percent_covered<"
                     f"{MUX_THRESHOLD:.0f}，v5 §11.5 证据降级）")
        ph = diff.get("phases") or {}
        if ph:
            L.append(f"事件前 {ph.get('metric', '?')} 相位（早期→晚期）：嫌疑核 "
                     f"{_fmt_g(ph.get('suspect_pre_early'))}→"
                     f"{_fmt_g(ph.get('suspect_pre_late'))}"
                     f"（×{_fmt_g(ph.get('suspect_ratio'), '.2g')}）、对照核 "
                     f"{_fmt_g(ph.get('control_pre_early'))}→"
                     f"{_fmt_g(ph.get('control_pre_late'))}"
                     f"（×{_fmt_g(ph.get('control_ratio'), '.2g')}）")
        for n in diff.get("notes") or []:
            L.append(f"- {n}")
    if verdict is not None:
        rule = verdict.get("rule")
        head = f"**规则{rule}**：{verdict.get('reading', '')}" if rule else \
               f"**{verdict.get('reading', '')}**"
        L += ["", "### 判读", head, "", f"> caveat：{verdict.get('caveat', '')}"]
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------------------
# CLI

def _main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(
        description="事件对齐时序差分 + 嫌疑/对照核 PMU 差分（v5 §11.5）")
    ap.add_argument("--pmu", required=True, help="pmu_core.csv 路径")
    ap.add_argument("--event", required=True, help='事件时刻 "%Y-%m-%d %H:%M:%S"')
    ap.add_argument("--suspect", required=True, nargs="+", help="嫌疑核（数字或拓扑标签）")
    ap.add_argument("--control", required=True, nargs="+", help="对照核")
    ap.add_argument("--before", type=float, default=60, help="事件前窗口秒（默认 60）")
    ap.add_argument("--after", type=float, default=60, help="事件后窗口秒（默认 60）")
    ap.add_argument("--metrics", action="append", default=[],
                    help="附加对齐 CSV（可多次，通道名取文件名）")
    ap.add_argument("--has-mismatch", choices=["yes", "no", "unknown"],
                    default="unknown", help="事件台账 mismatch 事实（默认 unknown）")
    args = ap.parse_args(argv)

    def _cpu(s):
        return int(s) if s.lstrip("-").isdigit() else s

    suspect = [_cpu(s) for s in args.suspect]
    control = [_cpu(s) for s in args.control]
    win = {"event_ts": args.event, "before_s": args.before, "after_s": args.after}
    winx = _extract_window(win)
    s_keys = _validate_cpus(suspect, "嫌疑核")
    c_keys = _validate_cpus(control, "对照核")
    if {k for k in s_keys} & {k for k in c_keys}:
        raise ValueError("嫌疑/对照核重叠（对照必须独立）")
    # pmu_core.csv 单次读取共享给窗口段与差分（评审 Important #3：不双读）
    header, rows, degraded, _, _, _ = _slice_window(
        args.pmu, args.event, args.before, args.after,
        required_cols=("ts", "core"))
    windows = {"pmu_core": _align_from_slice(header, rows, degraded,
                                             args.event, args.before, args.after)}
    for m in args.metrics:
        windows[os.path.basename(m)] = align_window(m, args.event,
                                                    args.before, args.after)
    s_rows, c_rows, mux = _core_select(rows, {k for k in s_keys},
                                       {k for k in c_keys},
                                       "percent_covered" in header)
    if not s_rows:
        raise ValueError(f"嫌疑核 {suspect!r} 在窗口内无样本行"
                         f"（core 列值为 perf 拓扑标签时须传标签串）")
    if not c_rows:
        raise ValueError(f"对照核 {control!r} 在窗口内无样本行")
    mm = {"yes": True, "no": False, "unknown": None}[args.has_mismatch]
    d = _diff_from_slice(header, s_rows, c_rows, degraded, mux,
                         suspect, control, winx, mm)
    print(format_timeline(windows, d, timeline_verdict(d)))
    return 0


if __name__ == "__main__":
    sys.exit(_main())

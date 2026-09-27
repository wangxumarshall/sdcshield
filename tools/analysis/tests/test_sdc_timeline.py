"""sdc_timeline 单元测试（M4 Task 4）——事件对齐时序差分 + PMU 差分视图（v5 §11.5）。

fixture 口径（brief Step 1 裁定，event_ts=2026-09-26 12:00:00，窗口 ±60s）：
  PMU_FIXTURE        嫌疑核 96 事件前 stall_ratio 逐拍上升（0.10→0.55，
                     pre 早期 0.11 → 晚期 0.35，×3.2）、事件后回落；对照核
                     0/32 全程平稳（0.10/0.11 交替）→ 规则① 时序/资源压力候选；
                     嫌疑核 dt=+60 行 percent_covered=42.0 → multiplex_degraded
                     降级注记；memory/path 组列留空（组轮换真实形态）→ 缺列。
  PMU_BOTH_FIXTURE   对照核 0 与嫌疑核 96 同步上升（同 SUSPECT_STALL 表）
                     → 规则④ 共享环境改判。
  PMU_FLAT           全核平稳（嫌疑=对照）→ 无 PMU 异常 + has_mismatch=True
                     → 规则② 不否定 SDC。
  PMU_ANOMALY_FLAT   嫌疑核 stall 全窗恒高 0.50（无事件前上升）、对照平稳
                     + has_mismatch=False → 规则③ canary。
  PMU_MISSING_COL    列头仅 cycles/inst_retired（无 stall/事件列）→ 缺列降级。
  PMU_LABEL_CORE     core 列为 perf 拓扑标签（S36-D0-C0 形，M1 as-built）
                     → 标签串匹配；数字 0 不得误配 "-C0"。
  PMU_ZERO_EARLY     嫌疑核事件前早期半窗 stall=0（事件前空闲）+ 对照核全零
                     → 相位比值 None → format_timeline 渲染 "—" 不抛（评审
                     Important #1 回归）。
  PMU_MID_RISE       对照核事件前轻度上升（×1.37，1.25-1.5 中间带）→ 规则①
                     读数分档"轻度上升"而非硬编码"平稳"（评审 Important #2）。
  PMU_SINGLE_SPIKE   嫌疑核晚期仅 1 拍尖峰（×2.5 由单拍驱动）→ "单拍毛刺可能"
                     降信度注记（评审 Minor #4）。
  MONITOR_CSV        align_window 专用：边界（±60 含、±61 弃）、坏 ts 行
                     （不可解析/空）跳过 + degraded 计数、数值/字符串/空值
                     三态列转换。
"""
import atexit
import datetime
import os
import shutil
import sys
import tempfile

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_timeline as tl


EVENT_TS = "2026-09-26 12:00:00"
EVENT_DT = datetime.datetime(2026, 9, 26, 12, 0, 0)
WIN = {"event_ts": EVENT_TS, "before_s": 60, "after_s": 60}

# M1 pmu_core.csv 列头逐字（sdc_collector_pmu.CORE_HEADER）
PMU_HEADER = ("ts,group,core,percent_covered,cycles,cpu_cycles,inst_retired,"
              "inst_spec,exe_stall_cycle,stall_frontend,stall_backend,"
              "mem_stall_anyload,mem_stall_l1miss,mem_stall_l2miss,"
              "l1d_cache_refill_rd,l2d_cache_refill_rd,ll_cache_miss_rd,"
              "dtlb_walk,l1d_tlb_refill_rd,remote_access,memory_error,br_mis_pred")

DTS = [-120, -60, -48, -36, -24, -12, 0, 12, 24, 36, 48, 60, 72]
SUSPECT_STALL = {-120: 0.10, -60: 0.10, -48: 0.12, -36: 0.15, -24: 0.40,
                 -12: 0.50, 0: 0.55, 12: 0.20, 24: 0.15, 36: 0.12,
                 48: 0.11, 60: 0.10, 72: 0.10}
FLAT_STALL = {dt: 0.10 + (0.01 if (dt // 12) % 2 else 0.0) for dt in DTS}
HIGH_STALL = {dt: 0.50 for dt in DTS}


def _ts(dt):
    return (EVENT_DT + datetime.timedelta(seconds=dt)).strftime("%Y-%m-%d %H:%M:%S")


def _pmu_row(core, dt, stall, pct=100.0):
    """core_base 组行：cycles/inst_retired/exe_stall（ipc=1.05-stall_ratio 自洽）
    + frontend/backend 二八分；memory/path 组 11 列留空（轮换真实形态）。"""
    cycles = 6_000_000_000
    inst = int(cycles * (1.05 - stall))
    exe = int(cycles * stall)
    fe = int(exe * 0.2)
    be = exe - fe
    return ",".join([_ts(dt), "core_base", str(core), f"{pct}", str(cycles), "",
                     str(inst), "", str(exe), str(fe), str(be)] + [""] * 11)


def _write_csv(name, header, rows):
    fd = tempfile.mkdtemp(prefix="sdc_timeline_fix_")
    atexit.register(shutil.rmtree, fd, ignore_errors=True)
    path = os.path.join(fd, name)
    with open(path, "w") as f:
        f.write(header + "\n" + "\n".join(rows) + "\n")
    return path


PMU_FIXTURE = _write_csv("pmu_core.csv", PMU_HEADER,
    [_pmu_row(96, dt, SUSPECT_STALL[dt], pct=42.0 if dt == 60 else 100.0)
     for dt in DTS]
    + [_pmu_row(c, dt, FLAT_STALL[dt]) for c in (0, 32) for dt in DTS])

PMU_BOTH_FIXTURE = _write_csv("pmu_both.csv", PMU_HEADER,
    [_pmu_row(96, dt, SUSPECT_STALL[dt]) for dt in DTS]        # 嫌疑核上升
    + [_pmu_row(0, dt, SUSPECT_STALL[dt]) for dt in DTS])      # 对照核同步上升

PMU_FLAT = _write_csv("pmu_flat.csv", PMU_HEADER,
    [_pmu_row(c, dt, FLAT_STALL[dt]) for c in (96, 0) for dt in DTS])

PMU_ANOMALY_FLAT = _write_csv("pmu_anomaly.csv", PMU_HEADER,
    [_pmu_row(96, dt, HIGH_STALL[dt]) for dt in DTS]           # 全窗恒高无上升
    + [_pmu_row(0, dt, FLAT_STALL[dt]) for dt in DTS])

PMU_MISSING_COL = _write_csv("pmu_missing.csv",
    "ts,group,core,percent_covered,cycles,inst_retired",
    [",".join([_ts(dt), "core_base", str(c), "100.0", "6000000000", "5700000000"])
     for c in (96, 0) for dt in DTS if -60 <= dt <= 60])

PMU_LABEL_CORE = _write_csv("pmu_label.csv",
    "ts,group,core,percent_covered,cycles,inst_retired,exe_stall_cycle",
    [",".join([_ts(dt), "core_base", "S36-D0-C0", "100.0", "6000000000",
               str(int(6e9 * (1.05 - SUSPECT_STALL[dt]))),
               str(int(6e9 * SUSPECT_STALL[dt]))])
     for dt in (-60, -48, -36, -24, -12, 0)]
    + [",".join([_ts(dt), "core_base", "S36-D0-C1", "100.0", "6000000000",
                 "5700000000", "600000000"])
       for dt in (-60, -48, -36, -24, -12, 0)])

MONITOR_CSV = _write_csv("monitor.csv", "ts,load_pct,mode,note", [
    f"{_ts(-61)},50.0,idle,",              # 窗外（-61 < -60）
    f"{_ts(-60)},55.5,idle,",              # 边界含
    f"{_ts(-30)},60,burst,ok",
    f"{_ts(0)},70.5,burst,ok",
    f"{_ts(60)},65.0,idle,",               # 边界含
    f"{_ts(61)},64.0,idle,",               # 窗外（+61 > +60）
    "not-a-date,1,2,3",                    # 坏 ts → 跳过 + degraded
    ",1,2,3",                              # 空 ts → 跳过 + degraded
])

ZERO_EARLY_STALL = {-120: 0.0, -60: 0.0, -48: 0.0, -36: 0.40, -24: 0.50,
                    -12: 0.55, 0: 0.55, 12: 0.55, 24: 0.55, 36: 0.55,
                    48: 0.55, 60: 0.55, 72: 0.55}
ALL_ZERO = {dt: 0.0 for dt in DTS}
PMU_ZERO_EARLY = _write_csv("pmu_zero_early.csv", PMU_HEADER,
    [_pmu_row(96, dt, ZERO_EARLY_STALL[dt]) for dt in DTS]
    + [_pmu_row(0, dt, ALL_ZERO[dt]) for dt in DTS])

MID_STALL = {-120: 0.10, -60: 0.10, -48: 0.10, -36: 0.13, -24: 0.14,
             -12: 0.14, 0: 0.14, 12: 0.14, 24: 0.14, 36: 0.14, 48: 0.14,
             60: 0.14, 72: 0.14}
PMU_MID_RISE = _write_csv("pmu_mid.csv", PMU_HEADER,
    [_pmu_row(96, dt, SUSPECT_STALL[dt]) for dt in DTS]
    + [_pmu_row(0, dt, MID_STALL[dt]) for dt in DTS])

SPIKE_STALL = {-120: 0.10, -60: 0.10, -48: 0.10, -36: 0.10, -24: 0.55,
               -12: 0.10, 0: 0.10, 12: 0.10, 24: 0.10, 36: 0.10, 48: 0.10,
               60: 0.10, 72: 0.10}
PMU_SINGLE_SPIKE = _write_csv("pmu_spike.csv", PMU_HEADER,
    [_pmu_row(96, dt, SPIKE_STALL[dt]) for dt in DTS]
    + [_pmu_row(0, dt, FLAT_STALL[dt]) for dt in DTS])


# ---------------------------------------------------------------------------
# brief Step 1 两测试（逐字口径）

def test_stall_rise_before_event_suspect_only():
    d = tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0, 32], WIN)
    v = tl.timeline_verdict(d)
    assert "时序" in v["reading"] and "因果" in v["caveat"]
    assert v["rule"] == 1
    assert d["suspect_pre_rise"] is True and d["control_sync"] is False
    assert d["diffs"]["stall_ratio"] > 0 and "stall_ratio" in d["flags"]
    assert d["mux_degraded_rows"] == 1                      # dt=+60 嫌疑核 42.0
    assert "multiplex_degraded" in "\n".join(d["notes"])
    assert "multiplex_degraded" in v["caveat"]              # v5 §11.5 证据降级


def test_control_sync_spikes_reclassify():
    v2 = tl.timeline_verdict(tl.suspect_vs_control_diff(PMU_BOTH_FIXTURE, [96], [0], WIN))
    assert "共享环境" in v2["reading"]
    assert v2["rule"] == 4 and "因果" in v2["caveat"]


# ---------------------------------------------------------------------------
# 判读规则②③（v5 §11.5 全四规则覆盖）

def test_mismatch_without_pmu_anomaly_not_refuting():
    d = tl.suspect_vs_control_diff(PMU_FLAT, [96], [0], WIN)
    assert not d["flags"] and not d["suspect_pre_rise"]     # 嫌疑=对照平稳
    d["has_mismatch"] = True
    v = tl.timeline_verdict(d)
    assert v["rule"] == 2 and "不否定" in v["reading"]
    assert "采样窗" in v["reading"] and "采样窗" in v["caveat"]
    assert "因果" in v["caveat"]


def test_pmu_anomaly_without_mismatch_canary():
    d = tl.suspect_vs_control_diff(PMU_ANOMALY_FLAT, [96], [0], WIN)
    assert "stall_ratio" in d["flags"] and not d["suspect_pre_rise"]
    d["has_mismatch"] = False
    v = tl.timeline_verdict(d)
    assert v["rule"] == 3 and "canary" in v["reading"]
    assert "不计" in v["reading"] and "因果" in v["caveat"]


# ---------------------------------------------------------------------------
# align_window：边界切片 / dt_s / 值三态转换 / 坏 ts 降级 / 形状错误

def test_align_window_slice_boundaries_and_values():
    w = tl.align_window(MONITOR_CSV, EVENT_TS, before_s=60, after_s=60)
    assert w["columns"] == ["load_pct", "mode", "note"]
    assert w["n"] == 4 and w["degraded"] == 2
    assert [r["dt_s"] for r in w["rows"]] == [-60.0, -30.0, 0.0, 60.0]  # 边界含
    assert w["rows"][0]["load_pct"] == 55.5                  # 数值列 → float
    assert w["rows"][1]["mode"] == "burst"                   # 字符串列原样
    assert w["rows"][1]["note"] == "ok" and w["rows"][0]["note"] is None  # 空→None
    assert w["window"] == {"event_ts": EVENT_TS, "before_s": 60, "after_s": 60}
    # datetime 事件时刻与字符串口径一致
    w2 = tl.align_window(MONITOR_CSV, EVENT_DT, before_s=60, after_s=60)
    assert w2["n"] == 4 and [r["dt_s"] for r in w2["rows"]] == [-60.0, -30.0, 0.0, 60.0]
    # 非对称窗：before=30 → -60 行出局、-30 边界含
    w3 = tl.align_window(MONITOR_CSV, EVENT_TS, before_s=30, after_s=0)
    assert [r["dt_s"] for r in w3["rows"]] == [-30.0, 0.0]


def test_align_window_degraded_and_shape_errors():
    for bad_path in ("/nonexistent/x.csv",):
        try:
            tl.align_window(bad_path, EVENT_TS)
            raise AssertionError("缺文件应 ValueError")
        except ValueError:
            pass
    for bad in (b"", b"load_pct,mode\n1,idle\n", b"ts2,load\n1,2\n"):
        fd = tempfile.mkdtemp(prefix="sdc_timeline_bad_")
        atexit.register(shutil.rmtree, fd, ignore_errors=True)
        p = os.path.join(fd, "bad.csv")
        with open(p, "wb") as f:
            f.write(bad)
        try:
            tl.align_window(p, EVENT_TS)
            raise AssertionError(f"畸形 CSV 应 ValueError: {bad!r}")
        except ValueError:
            pass
    try:
        tl.align_window(MONITOR_CSV, "not-a-date")           # 事件时刻不可解析
        raise AssertionError("坏 event_ts 应 ValueError")
    except ValueError:
        pass


# ---------------------------------------------------------------------------
# 缺列降级（组轮换空列 → missing 注记；stall 列整体缺席 → 上升不可判）

def test_missing_columns_and_stall_fallback_degradation():
    d = tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0, 32], WIN)
    assert "l1d_mpki" in d["missing"] and "dtlb_wpk" in d["missing"]  # 轮换空列
    assert set(d["metrics"]) >= {"ipc", "stall_ratio"}
    d2 = tl.suspect_vs_control_diff(PMU_MISSING_COL, [96], [0], WIN)
    assert d2["metrics"] == ["ipc"] and d2["flags"] == []
    assert "stall_ratio" in d2["missing"] and "l1d_mpki" in d2["missing"]
    assert d2["suspect_pre_rise"] is False
    assert any("缺列" in n for n in d2["notes"])            # 上升不可判注记


# ---------------------------------------------------------------------------
# core 列匹配（M1 as-built 拓扑标签）与空选择守卫

def test_core_label_matching_and_empty_selection():
    d = tl.suspect_vs_control_diff(PMU_LABEL_CORE, ["S36-D0-C0"], ["S36-D0-C1"], WIN)
    assert d["n_suspect_rows"] == 6 and d["suspect_pre_rise"] is True
    v = tl.timeline_verdict(d)                               # 标签核同规则①
    assert v["rule"] == 1 and "时序" in v["reading"]
    # 数字 0 不得误配拓扑标签 "-C0"（尾部 core 号非逻辑 CPU 号）
    try:
        tl.suspect_vs_control_diff(PMU_LABEL_CORE, [0], ["S36-D0-C1"], WIN)
        raise AssertionError("空嫌疑选择应 ValueError")
    except ValueError:
        pass
    try:
        tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [999], WIN)
        raise AssertionError("空对照选择应 ValueError")
    except ValueError:
        pass
    for bad_win in ({"before_s": 60, "after_s": 60},       # 缺 event_ts
                    (EVENT_TS, 60, 60)):                    # 元组不支持
        try:
            tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0], bad_win)
            raise AssertionError(f"坏 window 应 ValueError: {bad_win!r}")
        except ValueError:
            pass


# ---------------------------------------------------------------------------
# format_timeline：Markdown 段（§12.3 口径——分母同示、缺列不零填充）

def test_format_timeline_markdown():
    windows = {"pmu_core": tl.align_window(PMU_FIXTURE, EVENT_TS),
               "monitor": tl.align_window(MONITOR_CSV, EVENT_TS)}
    d = tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0, 32], WIN)
    v = tl.timeline_verdict(d)
    out = tl.format_timeline(windows, d, v)
    assert out.startswith("## ")
    for kw in ("嫌疑核", "对照核", "stall_ratio", "ipc", "缺列未算",
               "multiplex_degraded", "同窗相关≠因果"):
        assert kw in out, kw
    assert v["reading"] in out and "11/22" in out            # 分母（嫌/照行数）同示
    assert "l1d_mpki" in out                                  # 缺列名单在场
    # 事件前相位摘要（判读依据可追溯）
    assert "0.11" in out and "0.35" in out


# ---------------------------------------------------------------------------
# 评审修复回归（Important #1/#2/#3 + Minor #4）

def test_none_phase_ratio_renders_dash_not_crash():
    """#1：早期半窗均值恰 0 → 相位比值 None → format/verdict 不抛、渲染 "—"。"""
    d = tl.suspect_vs_control_diff(PMU_ZERO_EARLY, [96], [0], WIN)
    assert d["phases"]["suspect_ratio"] is None               # 早期 0 → 比值未定义
    assert d["phases"]["control_ratio"] is None
    v = tl.timeline_verdict(d)                                # 无事件前上升——异常分支
    out = tl.format_timeline({"pmu_core": tl.align_window(PMU_ZERO_EARLY, EVENT_TS)},
                             d, v)                            # 修复前此处 TypeError
    phase_line = [l for l in out.splitlines() if "相位" in l][0]
    assert "×—" in phase_line                                 # None → "—"
    assert "×None" not in out and "×0）" not in phase_line    # 绝不渲染成 0/None


def test_control_mid_rise_banded_reading():
    """#2：对照核 1.25-1.5 中间带 → 规则① 读数分档"轻度上升"，不硬编码"平稳"。"""
    d = tl.suspect_vs_control_diff(PMU_MID_RISE, [96], [0], WIN)
    assert d["suspect_pre_rise"] is True and d["control_sync"] is False
    assert tl.FLAT_MAX_RATIO <= d["phases"]["control_ratio"] < tl.RISE_RATIO
    v = tl.timeline_verdict(d)
    assert v["rule"] == 1
    assert "轻度上升" in v["reading"] and "未达同步阈值" in v["reading"]
    assert "平稳" not in v["reading"]                         # 与注记不再自相矛盾
    assert "共享环境可能性未排除" in v["caveat"]              # 读数弱化提示
    assert any("轻度上升" in n for n in d["notes"])
    # 分档另一端：对照真平稳（主 fixture ×1.0）→ 读数"对照核平稳"
    v_flat = tl.timeline_verdict(
        tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0, 32], WIN))
    assert "对照核平稳" in v_flat["reading"]


def test_single_beat_spike_annotated():
    """Minor #4：晚期仅 1 拍超阈 → 比值判据仍触发但注"单拍毛刺可能"。"""
    d = tl.suspect_vs_control_diff(PMU_SINGLE_SPIKE, [96], [0], WIN)
    assert d["suspect_pre_rise"] is True                      # ×2.5 由单拍驱动
    assert any("单拍毛刺" in n for n in d["notes"])
    v = tl.timeline_verdict(d)
    assert v["rule"] == 1
    assert "单拍毛刺" in tl.format_timeline(None, d, v)       # 报告可见


def test_cli_single_pass_matches_composed(capsys):
    """#3：CLI 对 pmu_core.csv 单次读取共享窗口段+差分——输出与独立组合逐字节一致。"""
    rc = tl._main(["--pmu", PMU_FIXTURE, "--event", EVENT_TS,
                   "--suspect", "96", "--control", "0", "32",
                   "--metrics", MONITOR_CSV])
    assert rc == 0
    out = capsys.readouterr().out
    windows = {"pmu_core": tl.align_window(PMU_FIXTURE, EVENT_TS, 60.0, 60.0),
               "monitor.csv": tl.align_window(MONITOR_CSV, EVENT_TS, 60.0, 60.0)}
    d = tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0, 32],
                                   {"event_ts": EVENT_TS, "before_s": 60.0,
                                    "after_s": 60.0})
    # print 在 format 尾换行外再补一个 \n
    assert out == tl.format_timeline(windows, d, tl.timeline_verdict(d)) + "\n"

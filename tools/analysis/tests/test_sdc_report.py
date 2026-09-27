"""sdc_report 单元测试（M4 Task 5）——离线诊断报告生成器（M4 退出标准载体）。

fixture 口径（brief Step 1 裁定，事件时刻 2026-09-26 12:00:00）：
  fixture_root   台账 4 行：run_a（rc=137+test，SDC 候选，k=1）、run_b（rc=137+test，
                 note 含「测试污染」→ INVALID 不入 SDC 率但单列）、run_c（note 含
                 「竞态=伪SDC」→ TEST_BUG 不入分母）、run_d（rc=0 非失败行）；
                 spool/events.jsonl 3 条 sdc_mismatch（run_a 带 2 个同 seed mismatch
                 块：lane=1 固定+offset 变 → execute_path 提示、mantissa 位段主导；
                 run_b 块被污染过滤、run_c 被 test_race 过滤）+ 1 条 note；
                 monitor/pmu_core.csv 嫌疑核 96 事件前 stall 上升（T4 规则①）+
                 对照核 0/32 平稳；spool/exposure.json n=2000 迭代 / 42.7 核心小时；
                 events/ev-fix0001/ring_window/ capsule 在场。
  zero_event_root 台账仅 1 行非失败行 + exposure n=1000 / 25.0 核时 → 零事件路径：
                 k=0 上界表（CP 精确 + rule of three），时序段省略。
"""
import atexit
import datetime
import json
import os
import shutil
import sys
import tempfile
import time

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_report as rep
import sdc_stats


EVENT_TS = "2026-09-26 12:00:00"
EVENT_DT = datetime.datetime(2026, 9, 26, 12, 0, 0)


def _ns(ts):
    return int(time.mktime(time.strptime(ts, "%Y-%m-%d %H:%M:%S"))) * 10 ** 9


def _ts(dt):
    return (EVENT_DT + datetime.timedelta(seconds=dt)).strftime("%Y-%m-%d %H:%M:%S")


# M1 pmu_core.csv 列头逐字（sdc_collector_pmu.CORE_HEADER，同 T4 测试口径）
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


def _pmu_row(core, dt, stall, pct=100.0):
    cycles = 6_000_000_000
    inst = int(cycles * (1.05 - stall))
    exe = int(cycles * stall)
    fe = int(exe * 0.2)
    be = exe - fe
    return ",".join([_ts(dt), "core_base", str(core), f"{pct}", str(cycles), "",
                     str(inst), "", str(exe), str(fe), str(be)] + [""] * 11)


def _make_root(prefix):
    fd = tempfile.mkdtemp(prefix=f"sdc_report_{prefix}_")
    atexit.register(shutil.rmtree, fd, ignore_errors=True)
    return fd


def _w(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def fixture_root(base):
    """事件齐备数据根（SDC 候选 1 + 污染 1 + TEST_BUG 1 + 非失败 1 + PMU + capsule）。"""
    root = os.path.join(base, "fix")
    os.makedirs(root, exist_ok=True)
    case_dir = os.path.join(root, "events", "case-run_a")
    os.makedirs(case_dir, exist_ok=True)
    _w(os.path.join(case_dir, "yaml_extract.txt"), "- test: fp_fma_double\n  result: fail\n")
    _w(os.path.join(root, "events", "ledger.csv"),
       "2026-09-26 12:00:00,run_a,rc=137,test=fp_fma_double,seed=AES:ab12,"
       "retest1(wholecmd) rc=0 fail/crash=0," + case_dir + ",mismatch 记录 多核压力下首见\n"
       "2026-09-26 13:00:00,run_b,rc=137,test=int_chained,seed=AES:cd34,,,"
       "【定性:测试污染(M1b 验证误杀 code9 非SDC 见CONTAMINATED.md)】\n"
       "2026-09-26 14:00:00,run_c,rc=137,test=mesh_sse,seed=AES:ef56,,,"
       "【定案:测试内部竞态=伪SDC 非硬件 已修复】\n"
       "2026-09-26 15:00:00,run_d,rc=0,seed=,,,正常心跳\n")

    def mm(offset, cpu=96):
        return {"type": "double", "byte_offset": offset, "lane": 1,
                "expected_hex": "0x400921fb54442d18",
                "actual_hex": "0x400921fb54442d19",
                "xor_mask_hex": "0x0000000000000001", "popcount": 1,
                "cpu": cpu, "seed": "AES:ab12", "core_id": cpu,
                "cluster_id": 1170, "socket_id": 0}

    def ev(eid, etype, run, ts_ns, **extra):
        d = {"schema_version": "1.0", "event_id": eid, "event_type": etype,
             "severity": "red" if etype == "sdc_mismatch" else "green",
             "confidence": "observed", "sdc-excite-reproduce_id": "sdc-excite-reproduce",
             "run_id": run, "time": {"realtime_ns": ts_ns}, "source": "ledger",
             "classification": {"primary": "candidate_hardware_sdc", "alternatives": [],
                                "status": "open"},
             "test": {"id": "x", "family": "", "seed": ""}}
        d.update(extra)
        return d

    _w(os.path.join(root, "spool", "events.jsonl"), "\n".join(json.dumps(e, ensure_ascii=False) for e in (
        ev("ev-fix0001", "sdc_mismatch", "run_a", _ns(EVENT_TS), mismatch=[mm(8), mm(40)]),
        ev("ev-fix0002", "sdc_mismatch", "run_b", _ns("2026-09-26 13:00:00"),
           mismatch=[mm(0, cpu=88)],
           source_line="2026-09-26 13:00:00,run_b,rc=137,...【定性:测试污染(...)】"),
        ev("ev-fix0003", "sdc_mismatch", "run_c", _ns("2026-09-26 14:00:00"),
           classification={"primary": "test_race", "alternatives": [], "status": "resolved"}),
        ev("ev-fix0004", "note", "run_d", _ns("2026-09-26 15:00:00"),
           classification={"primary": "operational", "alternatives": [], "status": "open"}),
    )) + "\n")
    _w(os.path.join(root, "spool", "exposure.json"), json.dumps({
        "valid_iterations": 2000, "valid_core_hours": 42.7,
        "source": "fixture：框架 YAML loop-count 日汇总（v5 §9.6 等效试验）"}))
    _w(os.path.join(root, "monitor", "pmu_core.csv"), PMU_HEADER + "\n" +
       "\n".join(_pmu_row(96, dt, SUSPECT_STALL[dt], pct=42.0 if dt == 60 else 100.0)
                 for dt in DTS)
       + "\n" + "\n".join(_pmu_row(c, dt, FLAT_STALL[dt]) for c in (0, 32) for dt in DTS) + "\n")
    cap = os.path.join(root, "events", "ev-fix0001", "ring_window")
    _w(os.path.join(cap, "monitor.csv"), "ts,outlet_temp\n" + _ts(0) + ",55\n")
    _w(os.path.join(cap, "percore.csv"), "ts,cpu96\n" + _ts(0) + ",100\n")
    return root


def zero_event_root(base):
    """零事件数据根（无失败行；暴露量在场 → k=0 数值上界表）。"""
    root = os.path.join(base, "zero")
    os.makedirs(root, exist_ok=True)
    _w(os.path.join(root, "events", "ledger.csv"),
       "2026-09-26 16:00:00,run_x,rc=0,seed=,,,正常收尾\n")
    _w(os.path.join(root, "spool", "exposure.json"),
       json.dumps({"valid_iterations": 1000, "valid_core_hours": 25.0,
                   "source": "fixture：零事件暴露量"}))
    return root


# ---------------------------------------------------------------------------
# brief Step 1 三测试（逐字口径）

def test_report_has_all_sections_and_denominators(tmp_path):
    md = rep.generate_report(str(fixture_root(str(tmp_path))))   # fixture 数据根
    for sec in ("概述", "位形态", "时序", "假设排序", "建议"):
        assert sec in md
    assert "n=" in md and "95% CI" in md                       # 分母+区间在场
    assert rep.report_guard(md) == []                          # 红线零违例
    assert "capsule" in md and "ring_window" in md             # capsule 存在则引


def test_k0_report_shows_upper_bound_not_zero_rate(tmp_path):
    md = rep.generate_report(str(zero_event_root(str(tmp_path))))
    assert "上界" in md and "发生率为 0" not in md
    assert "rule of three" in md and "3/n" in md               # 数值上界表


def test_guard_catches_violations():
    bad = "SDC 率 3.2%\n根因已定位：LSU\n"          # 无分母 + 无 E2 定性
    assert len(rep.report_guard(bad)) == 2


# ---------------------------------------------------------------------------
# 五段结构/顺序 + 零事件时序省略

def test_section_order_and_timeline_omitted_without_events(tmp_path):
    md = rep.generate_report(str(fixture_root(str(tmp_path))))
    heads = ("## 1. 概述", "## 2. 位形态", "## 3. 时序", "## 4. 假设排序", "## 5. 建议")
    idx = [md.index(h) for h in heads]
    assert idx == sorted(idx)                                  # 五段齐全且有序
    md0 = rep.generate_report(str(zero_event_root(str(tmp_path))))
    assert "## 3. 时序" not in md0                             # 无事件时省略
    assert "零事件" in md0                                     # 零事件路径显式在场


def test_empty_root_zero_path(tmp_path):
    empty = os.path.join(str(tmp_path), "empty")
    os.makedirs(empty)
    md = rep.generate_report(empty)
    assert "零事件" in md and "上界" in md                     # 上界表而非空表
    assert rep.report_guard(md) == []
    try:
        rep.generate_report(os.path.join(str(tmp_path), "no-such-root"))
        raise AssertionError("不存在的数据根应 ValueError")
    except ValueError:
        pass


# ---------------------------------------------------------------------------
# 三段切分（v5 §11.1-5：事实/推断/假设）

def test_fact_inference_hypothesis_sections_split():
    ev = {
        "raw_observations": ["台账 9 行", "events.jsonl 206 条"],
        "重算结果": ["xor 重算一致 2 块"],
        "干预记录": ["-n 1 复测 3 次全净（已执行的复测记录）"],
        "same_window_correlations": ["事件前 stall 上升与 mismatch 同窗"],
        "对照差分": ["嫌疑核 ipc 低于对照核（差值/对照均值 = -21.4%）"],
        "unverified_explanations": ["寄存器-旁路候选（未验证解释）"],
        "hypotheses": ["FP-SIMD-Vector 候选（score 2）"],
        "probes": ["scalar 对 vector（反事实探针）"],
    }
    facts, inferences, hypotheses = rep.fact_inference_hypothesis_sections(ev)
    assert "台账 9 行" in facts and "xor 重算一致 2 块" in facts
    assert "-n 1 复测 3 次全净（已执行的复测记录）" in facts   # 干预记录=已发生事实
    assert "事件前 stall 上升与 mismatch 同窗" in inferences
    assert "嫌疑核 ipc 低于对照核（差值/对照均值 = -21.4%）" in inferences
    assert not [s for s in facts if "同窗" in s]               # 各归其段
    assert "寄存器-旁路候选（未验证解释）" in hypotheses
    assert "FP-SIMD-Vector 候选（score 2）" in hypotheses
    assert "scalar 对 vector（反事实探针）" in hypotheses
    assert rep.fact_inference_hypothesis_sections({}) == ([], [], [])
    for bad in (["not", "dict"], "str", 42):
        try:
            rep.fact_inference_hypothesis_sections(bad)
            raise AssertionError(f"非 dict 应 ValueError: {bad!r}")
        except ValueError:
            pass


def test_report_contains_three_tier_section(tmp_path):
    md = rep.generate_report(str(fixture_root(str(tmp_path))))
    for tier in ("事实", "推断", "假设"):
        assert tier in md


# ---------------------------------------------------------------------------
# rate_table：双口径 + CP 区间 + k=0 上界 + 暴露量缺省

def test_rate_table_dual_denominator_cp_k0_upper_bound():
    t = rep.rate_table(3, 1000, 40.0)
    assert "95% CI" in t and "Clopper-Pearson" in t and "Poisson" in t
    assert "k=3/n=1000" in t and "k=3/h=40" in t               # 双口径 k/n 同示
    lo, hi = sdc_stats.clopper_pearson(3, 1000)
    assert f"{lo:.3g}" in t and f"{hi:.3g}" in t               # CP 区间数值
    t0 = rep.rate_table([], 1000, 25.0)                        # k=0
    assert "上界" in t0 and "rule of three" in t0
    assert "3/n" in t0 and "3/h" in t0
    assert "发生率为 0" not in t0 and "发生率 0" not in t0
    assert rep.report_guard(t0) == []                          # 表自身红线干净


def test_rate_table_no_exposure_note():
    t = rep.rate_table(2)                                      # valid_iterations 缺省
    assert "暴露量不可得" in t and "仅事件计数" in t
    assert "k=2" in t and "n=不可得" in t
    assert "Clopper-Pearson）: [" not in t                     # 不伪造区间
    t_list = rep.rate_table(["a", "b", "c"], 3000)             # 事件列表 → k=len
    assert "k=3/n=3000" in t_list


# ---------------------------------------------------------------------------
# 污染事件：不入 SDC 分母但单列（v5 §9.5）

def test_pollution_excluded_and_listed_separately(tmp_path):
    md = rep.generate_report(str(fixture_root(str(tmp_path))))
    assert "SDC 候选 k=1" in md                                # run_b/run_c 不入 k
    assert "污染事件单列" in md and "不入 SDC" in md
    assert "run_b" in md and "2026-09-26 13:00:00" in md       # 污染行可追溯
    assert "测试缺陷" in md                                    # TEST_BUG 亦单列注记
    # 位形态段排除污染/test_race 的 mismatch 块（只入 run_a 的 2 块）
    assert "排除" in md


# ---------------------------------------------------------------------------
# report_guard：四违例各一 + 负例不误报

def test_guard_four_violations_each_and_negatives():
    # ① 无分母比率（宽松正则命中 + 人工复核注释）
    v = rep.report_guard("SDC 率 3.2%\n")
    assert len(v) == 1 and "分母" in v[0] and "人工复核" in v[0] and "行 1" in v[0]
    # ② 零率表述（两种字样）
    for bad in ("结论：发生率为 0\n", "发生率 0 的声明\n"):
        v = rep.report_guard(bad)
        assert len(v) == 1 and "零率" in v[0]
    # ③ PMU 异常×SDC 同现且无「证据非判据」
    v = rep.report_guard("PMU 异常直接证明 SDC\n")
    assert len(v) == 1 and "证据非判据" in v[0]
    assert rep.report_guard("PMU 异常与 SDC 同窗（证据非判据，v5 §11.5）\n") == []
    # ④ 根因已定位且无 E2/E3/E4 前缀
    v = rep.report_guard("根因已定位：LSU\n")
    assert len(v) == 1 and "E2" in v[0]
    assert rep.report_guard("E2+ 干预后根因已定位\n") == []
    # 分母在场/行号正确不误报
    assert rep.report_guard("单 bit 3/10（30.0%）\n") == []
    assert rep.report_guard("n=12, max=96（100.0%）\n") == []
    v2 = rep.report_guard("正常行\nSDC 率 3.2%\n")
    assert len(v2) == 1 and "行 2" in v2[0]
    assert rep.report_guard("") == []


# ---------------------------------------------------------------------------
# has_mismatch 注入（T4 评审移交：events.jsonl 窗口事件存在性）

def test_has_mismatch_injection_from_events_jsonl(tmp_path):
    evs = [{"event_type": "note", "time": {"realtime_ns": _ns(EVENT_TS)}},
           {"event_type": "sdc_mismatch", "time": {"realtime_ns": _ns(EVENT_TS)}}]
    assert rep.has_mismatch_in_window(evs, EVENT_TS) is True
    assert rep.has_mismatch_in_window([], EVENT_TS) is False   # 空流=窗口内无 mismatch
    assert rep.has_mismatch_in_window(None, EVENT_TS) is None  # 无 events.jsonl
    assert rep.has_mismatch_in_window(evs, "2026-09-26 18:00:00") is False  # 窗外
    md = rep.generate_report(str(fixture_root(str(tmp_path))))
    assert "has_mismatch=True" in md                           # 报告时序段注入可见


# ---------------------------------------------------------------------------
# 单事件模式 + CLI

def test_event_id_mode_and_unknown_id(tmp_path):
    root = fixture_root(str(tmp_path))
    md = rep.generate_report(root, event_id="ev-fix0001")
    assert "ev-fix0001" in md and "单事件" in md
    assert "SDC 候选 k=1" in md and rep.report_guard(md) == []
    try:
        rep.generate_report(root, event_id="ev-nonexistent")
        raise AssertionError("未知 event_id 应 ValueError")
    except ValueError:
        pass


def _read(path):
    with open(path, encoding="utf-8") as f:
        return f.read()


def test_cli_writes_report(tmp_path, capsys):
    root = fixture_root(str(tmp_path))
    out = os.path.join(str(tmp_path), "report.md")
    assert rep._main(["--data-root", root, "-o", out]) == 0
    md = _read(out)
    for sec in ("概述", "位形态", "时序", "假设排序", "建议"):
        assert sec in md
    assert rep.report_guard(md) == []
    # out_path 参数：写文件且返回全文一致
    p2 = os.path.join(str(tmp_path), "r2.md")
    md2 = rep.generate_report(root, out_path=p2)
    assert os.path.exists(p2) and _read(p2) == md2
    # stdout 模式
    assert rep._main(["--data-root", root]) == 0
    assert "概述" in capsys.readouterr().out


# ---------------------------------------------------------------------------
# 评审修复回归（Important #1/#2/#3，2026-09-27）

def test_guard_rate_without_ci_flags_denominator_without_interval():
    """#1：brief ②「无区间发生率」——有分母无 CI 的率陈述此前静默通过（假阴）。"""
    v = rep.report_guard("SDC 率 3.2%（n=2000）无区间\n")
    assert len(v) == 1 and "无区间" in v[0] and "行 1" in v[0]
    v = rep.report_guard("发生率 1/2000\n")
    assert len(v) == 1 and "无区间" in v[0]


def test_guard_rate_without_ci_passes_with_interval_or_morphology():
    """#1 负例：带区间/上界的率与形态学比例（行内无「率」字）不误报。"""
    assert rep.report_guard("SDC 率 3.2%（n=2000，95% CI [0.0013, 0.0087]）\n") == []
    assert rep.report_guard("SDC 率上界 0.15%（n=2500）\n") == []
    assert rep.report_guard("单 bit 3/10（30.0%）\n") == []   # 形态比例非发生率


def test_guard_zero_rate_decimal_boundary_not_flagged():
    """#2：小数零率（0.15%/0.2%）合规行不再被子串匹配误报为整零。"""
    assert rep.report_guard("发生率为 0.15%（n=2500，95% CI [0.1, 0.8]）\n") == []
    assert rep.report_guard("失效率 0.2%（n=5000，95% CI [0.01, 0.7]）\n") == []


def test_guard_zero_rate_exact_zero_still_flagged():
    """#2 正例：整零表述（句读/行尾边界）仍触发，且不与新违例类型叠加。"""
    for bad in ("发生率为 0。", "发生率 0", "发生率为 0（未检出）"):
        v = rep.report_guard(bad + "\n")
        assert len(v) == 1 and "零率" in v[0], bad


def test_poisson_ci_log_space_large_k():
    """#3：k≳690 旧实现 e^{-λ} 前缀下溢使区间塌缩；log 域 lgamma 累加对拍
    评审 oracle（exposure=1：k=700 hi≈753.8、k=1000 → [938, 1064]）。"""
    _, hi = rep._poisson_ci(700, 1.0)
    assert abs(hi - 753.8) < 1.0
    lo, hi = rep._poisson_ci(1000, 1.0)
    assert abs(lo - 938.0) < 1.0 and abs(hi - 1064.0) < 1.0


def test_rate_table_large_k_interval_not_collapsed():
    """#3 端到端：大 k 率表 Poisson 区间非塌缩点（宽/窄比 ∈ (1.05, 2)）且
    guard 干净。"""
    t = rep.rate_table(700, None, 500.0)
    row = [l for l in t.splitlines() if l.startswith("| 核心小时")][0]
    lo_s, hi_s = row.split("|")[4].strip().strip("[]").split(",")
    ratio = float(hi_s) / float(lo_s)
    assert 1.05 < ratio < 2.0                     # 旧实现此处塌缩/发散
    assert rep.report_guard(t) == []

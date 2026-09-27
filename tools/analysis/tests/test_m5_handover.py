"""test_m5_handover.py — M5 Task 2：M4 终审移交两项（机读 hint 契约 + 嫌疑核
标签接续）。

  1. 机读 hint 契约（sdc_report._hypothesis_evidence）：bit_hints 路径令牌由
     T2 classification_hint 的 signals 机读布尔键派生
     （lane_fixed_address_varying → execute_path；address_fixed_lane_varying /
     offset_low_fixed_lane_varying → load_path），hint 文本仅显示用——T5 改
     措辞不再破坏证据链。向后兼容：signals 缺键（T2/T5 版本错位、手工构造
     hints）回退既有文本子串匹配 + ev["bit_hints_source"] 注记来源。
  2. 嫌疑核标签接续（sdc_timeline.cpu_to_topology_label）：logical cpu 号 →
     perf --per-core 拓扑标签 "S<pkg>-D<die>-C<core>"，供 report 差分把
     mismatch cpu 接续到 M1 pmu_core.csv 的标签形 core 列；拓扑不可得 →
     None（报告注记「标签不可接续」降级，不抛）。

标签格式实证（2026-09-27 本机，perf 6.6.0-159.4.3.154.oe2403sp4）：
strace perf stat -x, -a --per-core 逐 cpu 只 open topology/{core_id, die_id,
physical_package_id}（不读 cluster_id）；本机无 die_id 文件（ENOENT）而真实
monitor/pmu_core.csv 全 128 标签皆 D0（cpu0 → S36-D0-C0 而其 cluster_id=138；
cpu96 → S8442-D0-C96 而 cluster_id=12720）——brief 原设想「D=cluster_id /
die 无则两段形」被实证否定，实现按 perf 实测口径：D 取 die_id 值、文件缺
则 0，恒 S-D-C 三段形（S=physical_package_id 原值照读、C=core_id）。
"""
import datetime
import json
import os
import re
import shutil
import sys
import tempfile
import time
import atexit

sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_bitview
import sdc_report as rep
import sdc_timeline as tl

EVENT_TS = "2026-09-26 12:00:00"
EVENT_DT = datetime.datetime(2026, 9, 26, 12, 0, 0)

# M1 pmu_core.csv 列头逐字（同 test_sdc_report 口径）
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


def _ns(ts):
    return int(time.mktime(time.strptime(ts, "%Y-%m-%d %H:%M:%S"))) * 10 ** 9


def _ts(dt):
    return (EVENT_DT + datetime.timedelta(seconds=dt)).strftime("%Y-%m-%d %H:%M:%S")


def _pmu_row(core, dt, stall, pct=100.0):
    cycles = 6_000_000_000
    inst = int(cycles * (1.05 - stall))
    exe = int(cycles * stall)
    fe = int(exe * 0.2)
    be = exe - fe
    return ",".join([_ts(dt), "core_base", str(core), f"{pct}", str(cycles), "",
                     str(inst), "", str(exe), str(fe), str(be)] + [""] * 11)


def _tmpdir(prefix):
    fd = tempfile.mkdtemp(prefix=prefix)
    atexit.register(shutil.rmtree, fd, ignore_errors=True)
    return fd


def _w(path, text):
    os.makedirs(os.path.dirname(path), exist_ok=True)
    with open(path, "w") as f:
        f.write(text)


def _topo_root(base, cpus):
    """fixture 拓扑根：{cpu: {"pkg", "core", "die"|None}} → cpuN/topology/ 文件。
    die 缺省不写 die_id 文件（本机实形）。"""
    root = os.path.join(str(base), "syscpu")
    for cpu, spec in cpus.items():
        _w(os.path.join(root, f"cpu{cpu}", "topology", "physical_package_id"),
           f"{spec['pkg']}\n")
        _w(os.path.join(root, f"cpu{cpu}", "topology", "core_id"),
           f"{spec['core']}\n")
        if spec.get("die") is not None:
            _w(os.path.join(root, f"cpu{cpu}", "topology", "die_id"),
               f"{spec['die']}\n")
    return root


# ---------------------------------------------------------------------------
# 1. 机读 hint 契约：signals 键直连（文本仅显示用）

_SIG_EXEC = {"lane_fixed_address_varying": True,
             "address_fixed_lane_varying": False,
             "offset_low_fixed_lane_varying": False}
_SIG_MEM_ADDR = {"lane_fixed_address_varying": False,
                 "address_fixed_lane_varying": True,
                 "offset_low_fixed_lane_varying": False}
_SIG_MEM_LOW = {"lane_fixed_address_varying": False,
                "address_fixed_lane_varying": False,
                "offset_low_fixed_lane_varying": True}
_SIG_NONE = {k: False for k in _SIG_EXEC}


def test_bit_hints_from_signals_keys_not_text():
    """signals 机读键直连：hint 文本已改措辞（无旧子串/为空）仍派生路径令牌。"""
    reworded = {"hint": "T5 改写后的候选域措辞——不含任何旧判读子串",
                "confidence": "hint", "signals": dict(_SIG_EXEC)}
    ev = rep._hypothesis_evidence({}, reworded)
    assert ev["bit_hints"] == ["execute_path"]
    assert ev["bit_hints_source"] == "signals"
    ev = rep._hypothesis_evidence({}, {"hint": "", "signals": dict(_SIG_MEM_ADDR)})
    assert ev["bit_hints"] == ["load_path"]
    ev = rep._hypothesis_evidence({}, {"signals": dict(_SIG_MEM_LOW)})
    assert ev["bit_hints"] == ["load_path"]


def test_bit_hints_signals_all_false_and_mantissa_from_views():
    """signals 全 False → 信号不足不设 bit_hints；mantissa 仍由视图位段主导派生；
    来源注记只描述路径令牌（仅 mantissa 时无 bit_hints_source——mantissa 出自
    视图而非 signals/文本）。"""
    ev = rep._hypothesis_evidence({}, {"signals": dict(_SIG_NONE)})
    assert "bit_hints" not in ev and "bit_hints_source" not in ev
    views = {"field_clustering": {"dominant": "mantissa"}}
    ev = rep._hypothesis_evidence(views, {"signals": dict(_SIG_EXEC)})
    assert ev["bit_hints"] == ["execute_path", "mantissa"]
    assert ev["bit_hints_source"] == "signals"
    ev = rep._hypothesis_evidence(views, {"signals": dict(_SIG_NONE)})
    assert ev["bit_hints"] == ["mantissa"]
    assert "bit_hints_source" not in ev


def test_bit_hints_text_fallback_when_signals_keys_missing():
    """signals 缺键（版本错位/手工构造/空 dict/非契约键）→ 回退文本匹配 + 注记。"""
    old_text = ("固定 lane+地址变化（2 组同 seed）→ 执行/寄存器通路候选；"
                "固定 cache set/地址（offset 低位聚集）+lane 变化 → 存储层级候选。"
                "提示非定性（confidence: hint）")
    ev = rep._hypothesis_evidence({}, {"hint": old_text})        # 无 signals 键
    assert ev["bit_hints"] == ["execute_path", "load_path"]
    assert ev["bit_hints_source"] == "text_fallback"
    ev = rep._hypothesis_evidence({}, {"hint": old_text, "signals": {}})
    assert ev["bit_hints_source"] == "text_fallback"
    ev = rep._hypothesis_evidence(
        {}, {"hint": old_text, "signals": {"judged_groups": 3}})  # 仅非契约键
    assert ev["bit_hints_source"] == "text_fallback"
    ev = rep._hypothesis_evidence({}, {"hint": "信号不足，不定候选域"})
    assert "bit_hints" not in ev                                 # 文本亦无信号


def _mm_blocks(cpu=96):
    """同 seed 2 件：lane=1 固定 + offset 变 → lane_fixed_address_varying。"""
    def mm(off):
        return {"type": "double", "byte_offset": off, "lane": 1,
                "expected_hex": "0x400921fb54442d18",
                "actual_hex": "0x400921fb54442d19",
                "xor_mask_hex": "0x0000000000000001", "popcount": 1,
                "cpu": cpu, "seed": "AES:ab12"}
    return [mm(8), mm(40)]


def test_full_chain_t2_signals_survive_t5_rewording():
    """真实 T2 链 bit_views→classification_hint→report 机读直连；模拟 T5 改
    措辞（hint 文本覆写）bit_hints 不变——文本仅显示用。"""
    views = sdc_bitview.bit_views(_mm_blocks())
    hints = sdc_bitview.classification_hint(views)
    assert hints["signals"]["lane_fixed_address_varying"] is True
    ev = rep._hypothesis_evidence(views, hints)
    assert "execute_path" in ev["bit_hints"]
    assert ev["bit_hints_source"] == "signals"
    reworded = dict(hints, hint="T5 已改写候选域措辞（不含旧子串）")
    assert rep._hypothesis_evidence(views, reworded)["bit_hints"] == ev["bit_hints"]


# ---------------------------------------------------------------------------
# 2. cpu_to_topology_label：映射 / None 降级 / 真实冒烟

def test_cpu_to_topology_label_fixture_mapping(tmp_path):
    """fixture 映射：S=pkg 原值、C=core_id；die_id 缺 → D0（perf 实测口径）、
    die_id 在 → 取值；数字串核号容忍（mismatch 块 cpu 可为 str）。"""
    root = _topo_root(tmp_path, {
        0: {"pkg": 36, "core": 0},                 # 本机实形（无 die_id）
        96: {"pkg": 8442, "core": 96},             # 本机实形
        5: {"pkg": 0, "core": 5, "die": 2},        # die_id 在场（多 die 形）
    })
    assert tl.cpu_to_topology_label(0, root) == "S36-D0-C0"
    assert tl.cpu_to_topology_label(96, root) == "S8442-D0-C96"
    assert tl.cpu_to_topology_label(5, root) == "S0-D2-C5"
    assert tl.cpu_to_topology_label("96", root) == "S8442-D0-C96"


def test_cpu_to_topology_label_none_degrade(tmp_path):
    """拓扑不可得 → None 不抛：cpu 目录整缺（热插拔位空）、必需文件坏值、
    非核号输入。"""
    root = _topo_root(tmp_path, {0: {"pkg": 36, "core": 0}})
    assert tl.cpu_to_topology_label(3, root) is None            # 目录不存在
    bad = os.path.join(str(tmp_path), "badroot")
    _w(os.path.join(bad, "cpu1", "topology", "physical_package_id"), "garbage\n")
    _w(os.path.join(bad, "cpu1", "topology", "core_id"), "1\n")
    assert tl.cpu_to_topology_label(1, bad) is None             # pkg 不可解析
    _w(os.path.join(root, "cpu2", "topology", "physical_package_id"), "7\n")
    assert tl.cpu_to_topology_label(2, root) is None            # core_id 缺文件
    for bad_in in (None, True, False, 1.5, "abc", "", "-1", [96]):
        assert tl.cpu_to_topology_label(bad_in, root) is None, bad_in


def test_cpu_to_topology_label_real_smoke():
    """真实拓扑只读冒烟：默认根 cpu0 → 非 None、含 '-'，且 S-D-C 三段形
    （真实格式核对；本机实测 S36-D0-C0）。"""
    lab = tl.cpu_to_topology_label(0)
    assert lab is not None and "-" in lab
    assert re.fullmatch(r"S-?\d+-D-?\d+-C-?\d+", lab), lab


# ---------------------------------------------------------------------------
# 3. report 差分接续端到端：标签形 core 列 join / 标签不可接续降级

def _label_data_root(base):
    """标签形数据根：mismatch cpu=96；pmu_core.csv core 列为 perf 标签
    （S9-D0-C96 嫌疑核上升 + S9-D0-C0/32 对照平稳）——须由拓扑 fixture
    （cpu96 → pkg 9, core 96）接续才能 join。"""
    root = os.path.join(str(base), "labelroot")
    _w(os.path.join(root, "events", "ledger.csv"),
       "2026-09-26 12:00:00,run_a,rc=137,test=fp_fma_double,seed=AES:ab12,"
       ",,,mismatch 记录\n")

    def ev(eid, run, ts_ns, **extra):
        d = {"schema_version": "1.0", "event_id": eid, "event_type": "sdc_mismatch",
             "severity": "red", "confidence": "observed",
             "sdc-excite-reproduce_id": "sdc-excite-reproduce", "run_id": run,
             "time": {"realtime_ns": ts_ns}, "source": "ledger",
             "classification": {"primary": "candidate_hardware_sdc",
                                "alternatives": [], "status": "open"},
             "test": {"id": "x", "family": "", "seed": ""}}
        d.update(extra)
        return d

    _w(os.path.join(root, "spool", "events.jsonl"),
       json.dumps(ev("ev-m50001", "run_a", _ns(EVENT_TS),
                     mismatch=_mm_blocks()), ensure_ascii=False) + "\n")
    _w(os.path.join(root, "spool", "exposure.json"), json.dumps({
        "valid_iterations": 2000, "valid_core_hours": 42.7, "source": "fixture"}))
    _w(os.path.join(root, "monitor", "pmu_core.csv"), PMU_HEADER + "\n"
       + "\n".join(_pmu_row("S9-D0-C96", dt, SUSPECT_STALL[dt]) for dt in DTS)
       + "\n" + "\n".join(_pmu_row(c, dt, FLAT_STALL[dt])
                           for c in ("S9-D0-C0", "S9-D0-C32") for dt in DTS) + "\n")
    return root


def test_report_label_join_diff_via_topology_label(tmp_path):
    """端到端：mismatch cpu=96 → cpu_to_topology_label → S9-D0-C96 join
    标签形 core 列 → 规则①差分可算（无接续则为「差分不可算」）。"""
    root = _label_data_root(tmp_path)
    topo = _topo_root(tmp_path, {96: {"pkg": 9, "core": 96}})
    md = rep.generate_report(root, topology_root=topo)
    assert "嫌疑核标签接续" in md and "96→S9-D0-C96" in md
    assert "时序/资源压力候选" in md            # 规则①——标签 join 后差分可算
    assert "signals 机读键" in md               # bit_hints 机读来源注记在场
    assert rep.report_guard(md) == []


def test_report_label_unavailable_degrades_gracefully(tmp_path):
    """标签不可接续（sysfs 拓扑缺该 cpu）→ 注记降级不抛：数字核号在标签形
    core 列无行 → 如实「差分不可算」。"""
    root = _label_data_root(tmp_path)
    topo = _topo_root(tmp_path, {0: {"pkg": 9, "core": 0}})     # 无 cpu96 拓扑
    md = rep.generate_report(root, topology_root=topo)
    assert "标签不可接续" in md
    assert "差分不可算" in md
    assert rep.report_guard(md) == []

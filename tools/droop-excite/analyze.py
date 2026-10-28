#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""droop 战役分析：分层排序表（ΔI 标定 / V_sag / 事件）。

用法: python3 analyze.py /home/sdc/droop-excite/p1_2026... [更多 run_dir...]
判据边界（spec §8）：ΔI 来自低频档 min/max 电平差（非波形重建）；
V_sag = pre 均值 − dwell 最小值；高频纹波幅度 BMC 不可测——不虚构。
"""
import csv
import glob
import json
import os
import sys


def load_bmc_csv(path):
    with open(path) as f:
        return list(csv.DictReader(f))


def _num(row, key):
    try:
        return float(row.get(key, "") or "")
    except ValueError:
        return None


def window(rows, t0, t1):
    return [r for r in rows if t0 <= float(r["ts"]) <= t1]


def psu_total_series(rows):
    out = []
    for r in rows:
        a, b = _num(r, "Power"), _num(r, "Power2")
        if a is not None and b is not None:
            out.append(a + b)
    return out


def _vmin(rows, key):
    vals = [v for r in rows if (v := _num(r, key)) is not None]
    return min(vals) if vals else None


def _vmean(rows, key):
    vals = [v for r in rows if (v := _num(r, key)) is not None]
    return sum(vals) / len(vals) if vals else None


def step_metrics(step_dir):
    with open(os.path.join(step_dir, "summary.json")) as f:
        s = json.load(f)
    rows = load_bmc_csv(os.path.join(step_dir, "bmc.csv"))
    t = s["timeline"]
    # 基线取 pre 窗口的空闲尾部：refine probe 突发落在窗口头部（SDD T3 审查
    # 发现⑤/裁定 13），整窗均值会系统性抬高基线、失真 V_sag。
    # 15.0 与 run_step 的 pre_secs 默认值耦合，改动需同步。
    pre = window(rows, max(t["pre_start"], t["dwell_start"] - 15.0),
                 t["dwell_start"])
    dwell = window(rows, t["dwell_start"], t["dwell_end"])
    psu = psu_total_series(dwell)
    temps = [v for r in dwell
             for k in ("CPU1 Core Rem", "CPU2 Core Rem")
             if (v := _num(r, k)) is not None]
    metrics = {
        "id": s["id"], "note": s.get("note", ""),
        "payload": s.get("payload", "-"),
        "freq_hz": s.get("freq_hz"), "duty": s.get("duty"),
        "mod_ncores": len(s.get("mod_cores", [])),
        "n_samples": len(dwell),
        "i_p2p_w": (max(psu) - min(psu)) if psu else None,
        "psu_min_w": min(psu) if psu else None,
        "psu_max_w": max(psu) if psu else None,
        "v_sag_sock0_mv": None, "v_sag_sock1_mv": None,
        "vddavs_sag_sock0_mv": None, "vddavs_sag_sock1_mv": None,
        "t_max_c": max(temps) if temps else None,
        "eigen_rc": s.get("eigen_rc"), "mod_rc": s.get("mod_rc"),
        "aborted": s.get("aborted"),
        "ras_delta": s.get("ras_delta"), "sel_delta": s.get("sel_delta"),
        "sdc_suspect": s.get("sdc_suspect"),
    }
    for sock, key in ((0, "CPU1 N_VDDAVS"), (1, "CPU2 N_VDDAVS")):
        base, low = _vmean(pre, key), _vmin(dwell, key)
        if base is not None and low is not None:
            metrics[f"v_sag_sock{sock}_mv"] = round((base - low) * 1000.0, 1)
    # P0 实测（0103，2026-10-28）：N_VDDAVS 轨在 BMC 粒度下量化平坦（满载
    # +144W 仍恒 0.910/0.890），VDDAVS 轨响应负载（0.890→0.860，30mV）。
    # 故双轨都算，排序按最深轨——见 rank_tables。
    for sock, key in ((0, "CPU1 VDDAVS"), (1, "CPU2 VDDAVS")):
        base, low = _vmean(pre, key), _vmin(dwell, key)
        if base is not None and low is not None:
            metrics[f"vddavs_sag_sock{sock}_mv"] = round((base - low) * 1000.0, 1)
    return metrics


def collect_run(run_dir):
    steps = []
    for d in sorted(glob.glob(os.path.join(run_dir, "steps", "*"))):
        if os.path.exists(os.path.join(d, "summary.json")):
            steps.append(step_metrics(d))
    return steps


def _fmt(v):
    return "-" if v is None else f"{v:.6g}"


def rank_tables(runs):
    allsteps = []
    for r in runs:
        allsteps.extend(collect_run(r))
    lines = ["# droop 战役分层排序表", ""]

    def hdr(cols):
        return ("| " + " | ".join(cols) + " |",
                "| " + "|".join(["---"] * len(cols)) + " |")

    by_i = sorted([s for s in allsteps if s["i_p2p_w"] is not None],
                  key=lambda s: -s["i_p2p_w"])
    h, sep = hdr(["step", "payload", "freq_hz", "duty", "i_p2p_w",
                  "psu_max_w", "t_max_c"])
    lines += ["## 1. 低频 ΔI 摆幅（i_p2p_w 降序；仅低频档有意义）", "", h, sep]
    for s in by_i:
        lines.append(f"| {s['id']} | {s['payload']} | {_fmt(s['freq_hz'])} | "
                     f"{_fmt(s['duty'])} | {_fmt(s['i_p2p_w'])} | "
                     f"{_fmt(s['psu_max_w'])} | {_fmt(s['t_max_c'])} |")
    def sag_key(s):
        vals = [v for v in (s["v_sag_sock0_mv"], s["v_sag_sock1_mv"],
                            s["vddavs_sag_sock0_mv"], s["vddavs_sag_sock1_mv"])
                if v is not None]
        return max(vals) if vals else 0

    by_v = sorted([s for s in allsteps
                   if s["v_sag_sock0_mv"] is not None
                   or s["vddavs_sag_sock0_mv"] is not None],
                  key=lambda s: -sag_key(s))
    h, sep = hdr(["step", "v_sag_sock0_mv", "v_sag_sock1_mv",
                  "vddavs_sag_sock0_mv", "vddavs_sag_sock1_mv", "psu_max_w"])
    lines += ["", "## 2. V_sag（mV，按最深轨降序；0103 实测 N_VDDAVS 量化"
              "平坦、VDDAVS 响应负载）", "", h, sep]
    for s in by_v:
        lines.append(f"| {s['id']} | {_fmt(s['v_sag_sock0_mv'])} | "
                     f"{_fmt(s['v_sag_sock1_mv'])} | "
                     f"{_fmt(s['vddavs_sag_sock0_mv'])} | "
                     f"{_fmt(s['vddavs_sag_sock1_mv'])} | "
                     f"{_fmt(s['psu_max_w'])} |")
    ev = [s for s in allsteps
          if s.get("sdc_suspect") or s.get("aborted")
          or (s.get("ras_delta") or 0) > 0 or (s.get("sel_delta") or 0) > 0]
    h, sep = hdr(["step", "sdc_suspect", "aborted", "ras_delta",
                  "sel_delta", "mod_rc", "eigen_rc"])
    lines += ["", "## 3. 事件（SDC 疑似/护栏/RAS/SEL）", "", h, sep]
    for s in ev:
        lines.append(f"| {s['id']} | {s.get('sdc_suspect')} | "
                     f"{s.get('aborted')} | {s.get('ras_delta')} | "
                     f"{s.get('sel_delta')} | {s.get('mod_rc')} | "
                     f"{s.get('eigen_rc')} |")
    if not ev:
        lines.append("| （无事件——阴性结果，如实记录） | | | | | | |")
    return "\n".join(lines) + "\n"


def main():
    if len(sys.argv[1:]) == 0:
        sys.exit(__doc__)
    print(rank_tables(sys.argv[1:]))


if __name__ == "__main__":
    main()

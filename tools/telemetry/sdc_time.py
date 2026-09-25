#!/usr/bin/env python3
"""sdc_time.py — 五元时间契约（v5 §4.2）。CNTFRQ 现场读取，绝不硬编码。stdlib only。"""
import os, re, subprocess, time

_HELPER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cntvct_helper")

def _uptime_ns():
    with open("/proc/uptime") as f:
        return int(float(f.read().split()[0]) * 1e9)

def _cntvct_pair():
    out = subprocess.run([_HELPER], capture_output=True, text=True, check=True).stdout
    m = re.search(r"cntvct=(\d+) cntfrq_hz=(\d+)", out)
    if not m: raise RuntimeError(f"cntvct_helper 输出无法解析: {out!r}")
    return int(m.group(1)), int(m.group(2))

def read_quintet():
    """尽量短临界区内连续读取五元时间（顺序: realtime→monotonic_raw→cntvct→uptime）。"""
    realtime_ns = time.time_ns()
    monotonic_raw_ns = time.clock_gettime_ns(time.CLOCK_MONOTONIC_RAW)
    cntvct, cntfrq_hz = _cntvct_pair()
    return {"realtime_ns": realtime_ns, "monotonic_raw_ns": monotonic_raw_ns,
            "uptime_ns": _uptime_ns(), "cntvct": cntvct, "cntfrq_hz": cntfrq_hz,
            "bmc_time_raw": None, "mapping_error_ns": 0}

def anchor():
    """10min 周期锚定样本：收口再读一次 realtime 估计残余（v5 §4.2）。"""
    a = read_quintet()
    a["mapping_error_ns"] = time.time_ns() - a["realtime_ns"]
    return a

def interpolate(anchors, realtime_ns):
    """最近两锚点线性插值 → (cntvct 估计, 误差界 ns)。anchors 按 realtime 升序。"""
    if len(anchors) < 2:
        raise ValueError("需要 ≥2 个锚点")
    a, b = anchors[-2], anchors[-1]
    if realtime_ns < a["realtime_ns"] or realtime_ns > b["realtime_ns"]:
        a, b = anchors[0], anchors[1]  # 越界样本：用首对并放大误差
    span = b["realtime_ns"] - a["realtime_ns"]
    frac = (realtime_ns - a["realtime_ns"]) / span
    est = a["cntvct"] + frac * (b["cntvct"] - a["cntvct"])
    err = max(a.get("mapping_error_ns", 0), b.get("mapping_error_ns", 0)) + span
    return int(est), err

if __name__ == "__main__":
    import json; print(json.dumps(anchor()))

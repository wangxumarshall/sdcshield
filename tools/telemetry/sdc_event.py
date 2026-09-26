#!/usr/bin/env python3
"""sdc_event.py — canonical event 契约库（v5 §5.3）。stdlib only。

用法: python3 sdc_event.py validate < event.json   # 单事件校验
      python3 sdc_event.py validate-jsonl < e.jsonl
"""
import json, sys

SCHEMA_VERSION = "1.0"
SCHEMA_REF = "configs/sdc-excite-reproduce/schemas/event.schema.json"
EVENT_TYPES = {"sdc_mismatch", "crash", "ras_event", "spurious_fault",
               "sel_event", "interlock_action", "phase_boundary", "note",
               # M2 扩展（规则闭环输入，v5 §8）：monitor v3 离散断言 / RAS 关键字行 /
               # collector 自监控降级——sdc-eventd 五源产出
               "discrete_transition", "ras_keyword", "collector_degraded"}
SEVERITIES = {"green", "yellow", "orange", "red", "black"}
CONFIDENCES = {"observed", "inferred", "hypothetical"}
VERDICTS = {"PASS", "FAIL", "SKIP", "CRASH", "TIMED_OUT", "INTERRUPTED", "OSE"}
CLASS_STATUS = {"open", "qualified", "invalid", "test_bug", "resolved"}
REQUIRED = ["schema_version", "event_id", "event_type", "severity",
            "sdc-excite-reproduce_id", "run_id", "time"]
_HEX_FIELDS = ["mismatch.actual_hex", "mismatch.expected_hex",
               "mismatch.xor_mask_hex"]

def _get(d, path):
    for p in path.split("."):
        if not isinstance(d, dict) or p not in d: return None
        d = d[p]
    return d

def validate_event(ev):
    errs = []
    for k in REQUIRED:
        if k not in ev: errs.append(f"missing required: {k}")
    if ev.get("schema_version") != SCHEMA_VERSION:
        errs.append(f"schema_version != {SCHEMA_VERSION}")
    for k, allowed in (("event_type", EVENT_TYPES), ("severity", SEVERITIES),
                       ("confidence", CONFIDENCES)):
        if k in ev and ev[k] not in allowed:
            errs.append(f"{k} not in {sorted(allowed)}: {ev[k]!r}")
    if _get(ev, "result.verdict") not in (None, *VERDICTS):
        errs.append(f"result.verdict not in {sorted(VERDICTS)}")
    if _get(ev, "classification.status") not in (None, *CLASS_STATUS):
        errs.append(f"classification.status not in {sorted(CLASS_STATUS)}")
    for f in _HEX_FIELDS:
        v = _get(ev, f)
        if v is not None:
            s = str(v)[2:] if str(v).startswith("0x") else str(v)
            if not s or any(c not in "0123456789abcdefABCDEF" for c in s):
                errs.append(f"{f} not hex: {v!r}")
    t = ev.get("time")
    if isinstance(t, dict) and t.get("cntfrq_hz") is not None:
        if not isinstance(t["cntfrq_hz"], int) or t["cntfrq_hz"] <= 0:
            errs.append("time.cntfrq_hz must be positive int (v5 §4.2 禁硬编码假设)")
    return errs

def make_event(**fields):
    ev = {"schema_version": SCHEMA_VERSION, "parent_event_id": None,
          "time": None,  # 必须显式出现（required 键）；legacy 导入无五元数据时保持 None
          "artifacts": [], "environment": {"temperature_c": None,
          "frequency_khz": None, "voltage_v": None, "package_power_w": None,
          "pmu_window_id": None}, "classification": {"primary": "unclassified",
          "alternatives": [], "status": "open"}}
    ev.update(fields)
    return ev

def load_jsonl(path):
    with open(path) as f:
        return [json.loads(line) for line in f if line.strip()]

if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "validate"
    if mode == "validate":
        data = [json.load(sys.stdin)]
    else:
        data = []
        for line in sys.stdin:
            if not line.strip():
                continue
            try:
                data.append(json.loads(line))
            except json.JSONDecodeError as e:
                print(f"INVALID json line {e.lineno}: {e.msg}", file=sys.stderr)
                sys.exit(1)
    bad = 0
    for ev in data:
        errs = validate_event(ev)
        if errs: bad += 1; print(f"INVALID {ev.get('event_id')}: {errs}", file=sys.stderr)
    sys.exit(1 if bad else 0)

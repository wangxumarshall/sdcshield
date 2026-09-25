#!/usr/bin/env python3
"""sdc_legacy_adapter.py — 现有 ledger.csv/事件目录/sdcshield YAML → canonical events。

M0 范围（诚实声明，偏离 v5 M0 退出标准的部分）：CORE179 仓库资产为 Markdown
叙述性报告（docs/cases/sdc1-01-02-core179/），机器可读时间线重建属 M4 报告
工具；本适配器覆盖 ledger.csv + 事件目录 + YAML 失败块（当前数据全量）。
"""
import csv, json, os, re, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sdc_event

_NOTE_RESOLVED = ("定案", "定性", "伪事件", "已修复")
_TEST_RACE_KEYS = ("竞态", "test_bug", "伪SDC", "伪 SDC")

def _ts_to_ns(ts):  # "2026-09-24 09:14:10"（本地时区）→ realtime_ns
    return int(time.mktime(time.strptime(ts, "%Y-%m-%d %H:%M:%S")) * 1e9)


def _kv(field):  # "rc=137" / "test=mesh..." / "seed=AES:.."
    m = re.match(r"^(rc|test|seed)=(.*)$", field.strip())
    return (m.group(1), m.group(2)) if m else (None, field.strip())

def parse_ledger(path):
    rows = []
    with open(path, newline="") as f:
        for raw in csv.reader(f):
            if not raw or not raw[0].strip(): continue
            row = {"ts": raw[0].strip(), "label": raw[1].strip() if len(raw) > 1 else "",
                   "retests": "", "dir": "", "note": ""}
            kv, free = {}, []
            for i, cell in enumerate(raw[2:], start=2):
                k, v = _kv(cell)
                if k: kv[k] = v
                elif re.match(r"^retest\d", cell.strip()): row["retests"] = cell.strip()
                elif cell.startswith("/"): row["dir"] = cell.strip()
                else: free.append(cell.strip())
            row.update(kv); row["note"] = "；".join(x for x in free if x)
            rows.append(row)
    return rows

def parse_yaml_fail_blocks(text):
    """提取 '- test: <id>' … 'result: fail|crash' 块的关键字段（含连字符键 cpu-mask/time-to-fail）。"""
    blocks, cur = [], None
    for line in text.splitlines():
        m = re.match(r"^- test:\s*(\S+)", line)
        if m:
            if cur: blocks.append(cur)
            cur = {"test": m.group(1)}
            continue
        if cur is None: continue
        m = re.match(r"^  result:\s*(\S+)", line)
        if m: cur["result"] = m.group(1)
        m = re.match(r"^  fail:\s*\{(.*)\}", line)
        if m:
            pairs = re.findall(r"([\w-]+):\s*'([^']*)'|([\w-]+):\s*([\w.]+)", m.group(1))
            cur["fail"] = {(a or c): (b or d) for a, b, c, d in pairs}
    if cur: blocks.append(cur)
    return [b for b in blocks if b.get("result") in ("fail", "crash")]

def parse_event_dir(path):
    out = {"dir": path}
    yx = os.path.join(path, "yaml_extract.txt")
    if os.path.exists(yx):
        out["fail_blocks"] = parse_yaml_fail_blocks(open(yx).read())
    rt = os.path.join(path, "retests.txt")
    if os.path.exists(rt):
        out["retests"] = [l.strip() for l in open(rt) if l.strip()]
    return out

def convert_ledger(path, sdc_id):
    events = []
    for i, row in enumerate(parse_ledger(path), 1):
        rc = (row.get("rc") or "").replace("rc=", "")
        is_fail = row.get("test") and rc in ("1", "134", "137", "139")
        etype = ("sdc_mismatch" if is_fail else
                 "interlock_action" if "drill" in row["label"] else "note")
        note = row.get("note", "")
        status = "resolved" if any(k in note for k in _NOTE_RESOLVED) else "open"
        primary = ("test_race" if any(k in note for k in _TEST_RACE_KEYS)
                   else "candidate_hardware_sdc" if is_fail else "operational")
        ev = sdc_event.make_event(
            event_id=f"legacy-{i:04d}", event_type=etype,
            severity="red" if is_fail else "green",
            confidence="observed",
            **{"sdc-excite-reproduce_id": sdc_id, "run_id": row["label"]},
            test={"id": row.get("test") or "unknown", "family": "", "seed": row.get("seed")},
            time={"realtime_ns": _ts_to_ns(row["ts"]), "monotonic_raw_ns": None,
                  "uptime_ns": None, "cntvct": None, "cntfrq_hz": None,
                  "bmc_time_raw": None, "mapping_error_ns": None},
            classification={"primary": primary, "alternatives": [], "status": status},
            artifacts=[row["dir"]] if row.get("dir") else [])
        ev["legacy_row"] = row
        events.append(ev)
    return events

if __name__ == "__main__":
    src = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
        "~/sdc-excite-reproduce/events/ledger.csv")
    sid = sys.argv[2] if len(sys.argv) > 2 else "legacy-import"
    for ev in convert_ledger(src, sid):
        print(json.dumps(ev, ensure_ascii=False))

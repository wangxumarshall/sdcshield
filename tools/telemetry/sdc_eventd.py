#!/usr/bin/env python3
"""sdc_eventd.py — 多源尾随规范化事件流（v5 §3.1，M2 Task 1）。stdlib only。

尾随数据根五源 → canonical events（M0 sdc_event 契约）append spool/events.jsonl：
  events/ledger.csv            → sdc_mismatch / crash / note
                                 （行级解析复用 sdc_legacy_adapter.parse_ledger_lines；
                                   类型语义：rc∈{1,137}+test→sdc_mismatch，rc∈{134,139}+test
                                   →crash（SIGABRT/SIGSEGV 信号死），drill 行→note，
                                   其余→note——M2 计划口径，drill 演练行不是真实联锁动作）
  monitor/discrete_events.log  → discrete_transition（断言=新值非 0x00/ok/OK 且非 INIT
                                 →orange；回到正常态→green；值未变→不产生事件。
                                 INIT/断言口径与 sdc_monitor.sh 裁定一致）
  monitor/journal_watch.log    → ras_keyword（行含 UE/panic→red 否则 yellow；
                                 SPURIOUS/采集失败行也归此类——M2 计划字面口径，
                                 spurious_fault 类型保留给 M2+ BPF canary 升级）
  monitor/collector_self.csv   → collector_degraded（丢样行：last_success_ts 空=本周期
                                 采集失败（as-built 写入器在 samples_dropped 自增的同一
                                 行留空该字段）；或跨行 samples_dropped 增量>0。
                                 停更（无新行）由 monitor 看门狗 alert→interlock 源覆盖）
  monitor/alerts.log           → interlock_action（ALERT PAUSE:/热升级 KILL→black，
                                 ALERT RESUME:→green，其余 ALERT 行（磁盘水位等）→yellow）

设计约束：
  - 事件 event_id = ev-<sha1(源名+行内容)[:12]>（内容派生、幂等——同源同行重放不换 ID）；
  - dedup 键 = sha1(源名+行内容)[:16]，持久化 spool/dedup.json（上限 4096 条 FIFO 淘汰，
    防 offset 丢失/轮转重写导致的重复入流）；
  - schema 校验失败的事件不入 events.jsonl，隔离写 spool/eventd_invalid.jsonl（诚实保留）；
  - 源 ts 解析失败 → realtime_ns 取当前时刻、mapping_error_ns=-1（哨兵：误差无界，
    不伪造 0）；源行原文保留在事件的 source/source_line 字段（溯源）；
  - tail 只消费完整行（最后一个 \\n 之前），尾部残行留待下轮；
  - --once 跑一个完整轮次即退出（测试/演练模式）；守护模式 2s 轮询、SIGTERM 优雅退出
    （分片 sleep ≤1s——M1 教训：PEP 475 会续睡剩余时长）。
"""
import argparse, hashlib, json, os, re, signal, sys, time
from collections import OrderedDict
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import sdc_event
import sdc_legacy_adapter

POLL_INTERVAL_S = 2.0
DEDUP_CAP = 4096
OFFSET_FILE, DEDUP_FILE = "tail_offsets.json", "dedup.json"
EVENTS_FILE, INVALID_FILE = "events.jsonl", "eventd_invalid.jsonl"

# ---------------------------------------------------------------------------
# 通用尾随

def tail_file(path, offset_store):
    """增量读取完整行。返回 (new_lines, new_offset_store)。

    offset 持久化于调用方（EventdLoop 落 spool/tail_offsets.json）；
    文件缺失 → 无新行；截断/轮转（offset > size）→ 从 0 重读。
    """
    key = str(path)
    store = dict(offset_store)
    try:
        size = os.path.getsize(path)
    except OSError:
        return [], store                          # 源尚未出现（如 alerts.log 未建）
    off = store.get(key, 0)
    if off > size:                                # 截断/轮转 → 重置
        off = 0
    with open(path, "r", errors="replace") as f:
        f.seek(off)
        data = f.read()
    last_nl = data.rfind("\n")
    if last_nl == -1:
        return [], store                          # 无新的完整行（尾部残行留待下轮）
    store[key] = off + last_nl + 1
    return [l for l in data[:last_nl].split("\n") if l], store

# ---------------------------------------------------------------------------
# 行级转换（源 → canonical event）

def _ts_ns(ts_raw):
    """"%F %T"（本地时区）→ realtime_ns；解析失败 → (当前时刻, -1 哨兵)。"""
    try:
        return int(time.mktime(time.strptime(ts_raw.strip(), "%Y-%m-%d %H:%M:%S"))) * 10**9, None
    except (ValueError, OverflowError):
        return time.time_ns(), -1                 # 误差无界——不伪造 0

def _event(src, line, sid, event_type, severity, run_id, ts_raw, extra=None):
    digest = hashlib.sha1((src + line).encode("utf-8", "replace")).hexdigest()
    realtime_ns, mapping_error_ns = _ts_ns(ts_raw)
    ev = sdc_event.make_event(
        event_id=f"ev-{digest[:12]}", event_type=event_type, severity=severity,
        confidence="observed",
        **{"sdc-excite-reproduce_id": sid, "run_id": run_id},
        time={"realtime_ns": realtime_ns, "monotonic_raw_ns": None, "uptime_ns": None,
              "cntvct": None, "cntfrq_hz": None, "bmc_time_raw": None,
              "mapping_error_ns": mapping_error_ns})
    ev["source"], ev["source_line"] = src, line   # 溯源（M0 legacy_row 同例的附加字段）
    if extra:
        ev.update(extra)
    return ev

def ledger_line_to_event(line, sid):
    """ledger.csv 行 → sdc_mismatch/crash/note。行解析复用 M0 适配器（勿重写）。"""
    rows = sdc_legacy_adapter.parse_ledger_lines([line])
    if not rows:
        return None
    row = rows[0]
    rc, has_test = row.get("rc") or "", bool(row.get("test"))
    is_fail = has_test and rc in ("1", "137")     # 失败/驱动兜底 kill——战役口径同 M0
    is_crash = has_test and rc in ("134", "139")  # SIGABRT/SIGSEGV——测试二进制自身崩溃
    if is_fail or is_crash:
        etype, sev = ("sdc_mismatch" if is_fail else "crash"), "red"
    else:
        etype, sev = "note", "green"              # drill 演练行/正常行——非真实联锁动作
    note = row.get("note", "")
    # 分类关键词复用 M0 定义（单一事实源，防两处口径漂移）
    status = ("resolved" if any(k in note for k in sdc_legacy_adapter._NOTE_RESOLVED)
              else "open")
    primary = ("test_race" if any(k in note for k in sdc_legacy_adapter._TEST_RACE_KEYS)
               else "candidate_hardware_sdc" if (is_fail or is_crash) else "operational")
    return _event("ledger", line, sid, etype, sev, row["label"], row["ts"], extra={
        "test": {"id": row.get("test") or "unknown", "family": "", "seed": row.get("seed")},
        "classification": {"primary": primary, "alternatives": [], "status": status},
        "artifacts": [row["dir"]] if row.get("dir") else []})

_KNOWN_FAULT_SUFFIX = re.compile(r" ?\[known_fault:[a-z_]+\]$")

def discrete_line_to_event(line, sid):
    """`[ts] TRANSITION name: 旧 → 新 [known_fault:…]` → discrete_transition。"""
    m = re.match(r"^\[([^\]]+)\] TRANSITION (.+?): (.+)$", line.rstrip())
    if not m:
        return None
    ts_raw, name, rest = m.groups()
    rest = _KNOWN_FAULT_SUFFIX.sub("", rest)      # 白名单标注行尾剥离后再取值
    if " → " not in rest:
        return None
    old, new = rest.rsplit(" → ", 1)
    if old == new:
        return None                               # 值未变不产生事件
    assertion = new not in ("0x00", "ok", "OK") and old != "INIT"   # monitor 断言口径
    return _event("discrete", line, sid, "discrete_transition",
                  "orange" if assertion else "green", "monitor", ts_raw,
                  extra={"discrete": {"name": name, "from": old, "to": new}})

def journal_line_to_event(line, sid):
    """journal_watch.log 行（UE>0/告警/SPURIOUS/RAS）→ ras_keyword。"""
    m = re.match(r"^\[([^\]]+)\] (.+)$", line.rstrip())
    if not m:
        return None
    ts_raw, content = m.groups()
    sev = "red" if ("UE" in content or "panic" in content) else "yellow"
    return _event("journal", line, sid, "ras_keyword", sev, "monitor", ts_raw)

def _selfmon_parse(line):
    """→ (collector 名, samples_dropped 累计值) | None（表头/残行）。"""
    if line.startswith("ts,"):
        return None
    f = line.split(",")
    if len(f) < 4 or not f[3].strip().isdigit():
        return None
    return f[1], int(f[3])

def selfmon_line_to_event(line, sid, prev_dropped=None):
    """collector_self.csv 行 → collector_degraded（丢样/采集失败）。

    行级信号：last_success_ts 空 = 本周期采集失败（写入器在 samples_dropped 自增的
    同一行留空该字段）；跨行信号：prev_dropped 提供时 samples_dropped 增量>0。
    """
    if _selfmon_parse(line) is None:
        return None
    f = line.split(",")
    ts_raw, name, dropped = f[0], f[1], int(f[3])
    if not (f[6] == "" or (prev_dropped is not None and dropped > prev_dropped)):
        return None                               # 累计值未增且本周期成功——非事件
    return _event("selfmon", line, sid, "collector_degraded", "yellow", "collector",
                  ts_raw, extra={"collector": {"name": name, "samples_dropped": dropped}})

def interlock_line_to_event(line, sid):
    """alerts.log 的 `[ts] ALERT <msg>` 行 → interlock_action（v5 §8.2 BLACK 输入链）。"""
    m = re.match(r"^\[([^\]]+)\] ALERT (.+)$", line.rstrip())
    if not m:
        return None
    ts_raw, msg = m.groups()
    if msg.startswith("PAUSE:"):
        action, sev = "pause", "black"
    elif msg.startswith("RESUME:"):
        action, sev = "resume", "green"
    elif "热升级" in msg and "KILL" in msg:
        action, sev = "kill", "black"
    else:                                         # 磁盘水位/SEL 风暴/看门狗等告警
        action, sev = "alert", "yellow"
    return _event("interlock", line, sid, "interlock_action", sev, "monitor", ts_raw,
                  extra={"interlock": {"action": action, "message": msg}})

# ---------------------------------------------------------------------------
# 事件循环

class EventdLoop:
    """一轮 poll_once = 五源尾随 + 行级转换 + dedup + schema 校验 + 落盘。"""

    # (源名, 数据根内相对路径, 转换函数名——globals() 晚绑定，测试可 monkeypatch)
    SOURCES = (
        ("ledger", ("events", "ledger.csv"), "ledger_line_to_event"),
        ("discrete", ("monitor", "discrete_events.log"), "discrete_line_to_event"),
        ("journal", ("monitor", "journal_watch.log"), "journal_line_to_event"),
        ("selfmon", ("monitor", "collector_self.csv"), "selfmon_line_to_event"),
        ("interlock", ("monitor", "alerts.log"), "interlock_line_to_event"),
    )

    def __init__(self, data_root, spool_dir, sdc_id=None):
        self.data_root, self.spool_dir = data_root, spool_dir
        os.makedirs(spool_dir, exist_ok=True)
        # sid：显式指定 > 数据根目录名（真实部署=~/sdc-excite-reproduce → 派生同名）
        self.sdc_id = sdc_id or (os.path.basename(os.path.normpath(data_root))
                                 or "sdc-excite-reproduce")
        self.offsets = self._load_json(OFFSET_FILE, {})
        keys = self._load_json(DEDUP_FILE, [])
        self._seen = OrderedDict.fromkeys(keys if isinstance(keys, list) else [])
        self._selfmon_prev = {}                   # collector → 上行 samples_dropped（进程内）

    def _sp(self, name):
        return os.path.join(self.spool_dir, name)

    def _load_json(self, name, default):
        try:
            with open(self._sp(name)) as f:
                return json.load(f)
        except (OSError, json.JSONDecodeError):
            return default

    @staticmethod
    def _append_jsonl(path, obj):
        with open(path, "a") as f:
            f.write(json.dumps(obj, ensure_ascii=False) + "\n")

    def poll_once(self):
        """一轮五源尾随；返回本轮新事件（已 append spool/events.jsonl）。"""
        out = []
        for src, rel, conv_name in self.SOURCES:
            path = os.path.join(self.data_root, *rel)
            lines, self.offsets = tail_file(path, self.offsets)
            conv = globals()[conv_name]
            for line in lines:
                if src == "selfmon":
                    p = _selfmon_parse(line)
                    ev = conv(line, self.sdc_id,
                              prev_dropped=self._selfmon_prev.get(p[0]) if p else None)
                    if p:
                        self._selfmon_prev[p[0]] = p[1]
                else:
                    ev = conv(line, self.sdc_id)
                if ev is None:
                    continue
                key = hashlib.sha1((src + line).encode("utf-8", "replace")).hexdigest()[:16]
                if key in self._seen:             # 同源同内容已见过（offset 丢失/轮转重写）
                    continue
                self._seen[key] = True
                while len(self._seen) > DEDUP_CAP:
                    self._seen.popitem(last=False)    # FIFO 淘汰最旧
                errs = sdc_event.validate_event(ev)
                if errs:                          # 诚实隔离：不入事件流
                    self._append_jsonl(self._sp(INVALID_FILE), {
                        "ts": time.strftime("%F %T"), "source": src,
                        "line": line, "errors": errs, "event": ev})
                    continue
                self._append_jsonl(self._sp(EVENTS_FILE), ev)
                out.append(ev)
        with open(self._sp(OFFSET_FILE), "w") as f:
            json.dump(self.offsets, f, ensure_ascii=False, indent=1)
        with open(self._sp(DEDUP_FILE), "w") as f:
            json.dump(list(self._seen), f, ensure_ascii=False)
        return out

# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="sdc-eventd：多源尾随规范化事件流（v5 §3.1）")
    ap.add_argument("--once", action="store_true",
                    help="跑一个完整轮次后退出（测试/演练模式）")
    ap.add_argument("--data-root", default=None, help="数据根（默认 $SDC_EXCITE_REPRODUCE_DIR"
                     "/$SDC_CAMPAIGN_DIR/~/sdc-excite-reproduce）")
    a = ap.parse_args(argv)
    root = a.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") or \
        os.environ.get("SDC_CAMPAIGN_DIR") or os.path.expanduser("~/sdc-excite-reproduce")
    loop = EventdLoop(root, os.path.join(root, "spool"))
    if a.once:
        evs = loop.poll_once()
        print(f"[eventd] once: {len(evs)} new events")
        return 0
    stop = [False]
    def _term(signum, frame):
        stop[0] = True
    signal.signal(signal.SIGTERM, _term)
    signal.signal(signal.SIGINT, _term)
    while not stop[0]:
        loop.poll_once()
        end = time.monotonic() + POLL_INTERVAL_S  # 分片睡 ≤1s：SIGTERM 后 ≤1s 内退出
        while not stop[0] and time.monotonic() < end:
            time.sleep(min(1.0, end - time.monotonic()))
    return 0

if __name__ == "__main__":
    sys.exit(main())

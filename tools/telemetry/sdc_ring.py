#!/usr/bin/env python3
"""sdc_ring.py — 120s 滚动热窗口 + red/black 事件固化（v5 §6.3，M1 移交项，M2 Task 2）。stdlib only。

节点维护至少 120 秒滚动热窗口（v5 §6.3）；sdc-eventd 产出的 red/black 事件发生时
固化窗口为取证证据（[-60s,+60s] 口径：固化时点=事件检测时点，窗口持检测前 ≤120s
数据即前半窗；+60s 后半窗由事件后持续采集与快照链补齐）。

数据流：
  monitor/monitor.csv  ─┐
                        ├─ tail_file 尾随 → RingWindow（内存 deque；行首 ts %F %T 淘汰）
  monitor/percore.csv  ─┘
  spool/events.jsonl → 新行 severity∈{red,black} → freeze → events/<event_id>/ring_window/
                                            （无 event_id → spool/frozen/<ts>-<hash>/）

设计裁定：
  - tail_file 复用 sdc_eventd（T1 交付，单一实现不重写）；
  - 淘汰口径 now - 行ts > max_seconds（严格大于，恰 120s 保窗）；push 与 snapshot
    都淘汰——长时间无新行时 snapshot 也守 120s 口径；
  - 行首 ts 解析失败的行保窗不淘汰（诚实保留：表头/残行在固化文件里要可见，与
    T1 eventd 同哲学；代价是持续产出不可解析行的源会让窗无限长——源头问题可见，
    不静默丢）；
  - events.jsonl 消费 offset 持久化 spool/ring_offset.json（原子写 tmp+os.replace，
    崩溃不撕裂）；已固化 id 集持久化 spool/ring_frozen.json（同原子写；不设容量
    上限——T1 dedup 丢键只致重复入流，此集丢键致取证证据被覆盖，代价不对称）；
  - 重扫护栏（offset 撕裂/截断/首启从 0 重扫时）：id 已在固化集，或目标
    ring_window/ 下已有 *.csv → 跳过覆盖、返回值附 skipped_existing、stderr 一行
    ——重扫时当前窗对旧事件已是时间错位数据，覆盖即静默销毁既有取证证据；
  - CSV 尾随 offset 不持久化：进程首轮回放两文件尾 ≤WARMUP_TAIL_BYTES 暖窗——重启
    后窗口仍持最近 ≤120s 数据，且回放读内存有界（percore.csv as-built 已 1.4MB/天
    级增长、无常规轮转，全量重读内存随战役时长线性涨）；start>0 时从块内首个换行
    对齐（丢弃截半首行），start=0 时首行即完整行（表头入窗）；
  - freeze 幂等（覆盖写）；窗口空源不写文件、不伪造空证据；
  - 坏 JSONL 行/固化异常 → spool/ring_invalid.jsonl 隔离（行级兜底，poll_once 不炸
    7×24 守护）；
  - --once 跑一个完整轮次即退出（测试/演练模式）；守护 2s 轮询、SIGTERM 优雅退出
    （分片睡 ≤1s——M1 教训：PEP 475 会续睡剩余时长）。
"""
import argparse, hashlib, json, os, signal, sys, time
from collections import deque
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from sdc_eventd import tail_file

POLL_INTERVAL_S = 2.0
MAX_SECONDS = 120
WARMUP_TAIL_BYTES = 1 << 20          # 暖窗回放上限：1MB ≫ 120s 数据量（percore ~6KB/120s）
OFFSET_FILE, FROZEN_FILE = "ring_offset.json", "ring_frozen.json"
EVENTS_FILE, INVALID_FILE = "events.jsonl", "ring_invalid.jsonl"
FROZEN_SEVERITIES = ("red", "black")

# ---------------------------------------------------------------------------
# 滚动窗口

def _parse_ts(line):
    """行首 `%F %T` 字段 → epoch 秒；解析失败 → None（保窗不淘汰）。"""
    try:
        return time.mktime(time.strptime(line.split(",", 1)[0].strip(), "%Y-%m-%d %H:%M:%S"))
    except (ValueError, OverflowError):
        return None

class RingWindow:
    """csv_name → 行 deque 的内存滚动窗（默认 120s 口径）。"""

    def __init__(self, max_seconds=MAX_SECONDS):
        self.max_seconds = max_seconds
        self._buf = {}                                # csv_name -> deque[str]（保序）

    def _kept(self, line, now):
        t = _parse_ts(line)
        return t is None or now - t <= self.max_seconds   # 解析失败保窗（诚实保留）

    def push(self, csv_name, line):
        dq = self._buf.setdefault(csv_name, deque())
        if self._kept(line, time.time()):
            dq.append(line)               # 入窗即过期的行不占窗（等价 append 后立即淘汰）
        self._evict(csv_name)

    def _evict(self, csv_name):
        dq = self._buf.get(csv_name)
        if dq is None:
            return
        now = time.time()
        kept = deque(l for l in dq if self._kept(l, now))
        if kept:
            self._buf[csv_name] = kept
        else:
            del self._buf[csv_name]       # 全过期的源不出现在 snapshot

    def snapshot(self):
        """→ {csv_name: [lines]}；先淘汰再返回（无新行时也守口径）。"""
        for name in list(self._buf):
            self._evict(name)
        return {name: list(dq) for name, dq in self._buf.items()}

# ---------------------------------------------------------------------------
# 固化

def freeze(window, out_dir, event_id):
    """窗口全部行写 out_dir/ring_window/<name>.csv（幂等覆盖写）。

    每文件头注释行 `# frozen at <ts> for <event_id> (ring [-60s,+60s] 口径, 行数=<n>)`
    （n=该文件固化行数，不含注释行）。窗口空源不写文件（不伪造空证据）。
    返回写入文件路径列表。
    """
    snap = window.snapshot()
    if not snap:
        return []
    rw = os.path.join(out_dir, "ring_window")
    os.makedirs(rw, exist_ok=True)
    now_ts = time.strftime("%F %T")
    written = []
    for name, lines in snap.items():
        path = os.path.join(rw, f"{name}.csv")
        with open(path, "w") as f:
            f.write(f"# frozen at {now_ts} for {event_id} "
                    f"(ring [-60s,+60s] 口径, 行数={len(lines)})\n")
            f.writelines(l + "\n" for l in lines)
        written.append(path)
    return written

# ---------------------------------------------------------------------------
# 事件循环

class RingLoop:
    """一轮 poll_once = 两 CSV 尾随入窗 + events.jsonl 新行 red/black 固化。"""

    SOURCES = (("monitor", ("monitor", "monitor.csv")),
               ("percore", ("monitor", "percore.csv")))

    def __init__(self, data_root, spool_dir):
        self.data_root, self.spool_dir = data_root, spool_dir
        os.makedirs(spool_dir, exist_ok=True)
        self.window = RingWindow()
        self._csv_offsets = {}            # 进程内尾随 offset（不持久化——重启暖窗重放）
        self._warmed = False
        self.event_offset = self._load_state()
        self._frozen_ids = self._load_frozen_ids()   # 重扫护栏之一（之二=文件存在性）

    def _sp(self, name):
        return os.path.join(self.spool_dir, name)

    @staticmethod
    def _atomic_json(path, obj):
        """原子写：tmp + os.replace——崩溃不留半截 JSON（Important-1 修复）。"""
        tmp = path + ".tmp"
        with open(tmp, "w") as f:
            json.dump(obj, f, ensure_ascii=False, indent=1)
        os.replace(tmp, path)

    def _load_state(self):
        try:
            with open(self._sp(OFFSET_FILE)) as f:
                state = json.load(f)
            return state.get("events_offset", 0) if isinstance(state, dict) else 0
        except (OSError, json.JSONDecodeError):
            return 0                      # 无/撕裂 → 从头重扫（重扫护栏拦截覆盖，见模块注）

    def _save_state(self):
        self._atomic_json(self._sp(OFFSET_FILE), {"events_offset": self.event_offset})

    def _load_frozen_ids(self):
        """已固化 id 集（T1 dedup 同思路；不设容量上限——dedup 丢键只致重复入流，
        此集丢键致取证证据被覆盖，代价不对称）。撕裂/丢失 → 空集，由文件存在性
        护栏兜底。"""
        try:
            with open(self._sp(FROZEN_FILE)) as f:
                data = json.load(f)
            return set(data) if isinstance(data, list) else set()
        except (OSError, json.JSONDecodeError):
            return set()

    def _save_frozen_ids(self):
        self._atomic_json(self._sp(FROZEN_FILE), sorted(self._frozen_ids))

    @staticmethod
    def _has_frozen_files(out_dir):
        """out_dir/ring_window/ 下已有 *.csv → 已有固化证据（护栏之二：固化集
        丢失/撕裂时仍拦得住，防覆盖既有取证文件）。"""
        try:
            return any(f.endswith(".csv") for f in
                       os.listdir(os.path.join(out_dir, "ring_window")))
        except OSError:
            return False

    def _warmup_tail(self, csv_name, path):
        """进程首轮回放文件尾（≤WARMUP_TAIL_BYTES）暖窗；返回尾随起始 offset。

        start>0 时从块内首个换行对齐（丢弃截半首行）；块尾残行留给 tail_file。
        """
        try:
            size = os.path.getsize(path)
        except OSError:
            return 0                      # 源尚未出现——tail_file 稍后接管
        start = max(0, size - WARMUP_TAIL_BYTES)
        with open(path, "rb") as f:
            f.seek(start)
            data = f.read()
        if start > 0:
            nl = data.find(b"\n")
            if nl == -1:
                return start              # 病理单行 >1MB：不越界对齐，交 tail_file
            data, start = data[nl + 1:], start + nl + 1
        last_nl = data.rfind(b"\n")
        if last_nl == -1:
            return start                  # 无完整行
        for raw in data[:last_nl].split(b"\n"):
            if raw:
                self.window.push(csv_name, raw.decode("utf-8", "replace"))
        return start + last_nl + 1

    def _freeze_target(self, ev):
        """→ (固化目录, 报告标识)。有 event_id → events/<event_id>/（自建目录）；
        无 event_id（或路径不安全）→ spool/frozen/<ts>-<hash>/（内容 hash 代位命名）。"""
        eid = ev.get("event_id")
        if (isinstance(eid, str) and eid and eid not in (".", "..")
                and not any(c in eid for c in "/\\\x00")):
            return os.path.join(self.data_root, "events", eid), eid
        digest = hashlib.sha1(json.dumps(ev, sort_keys=True, ensure_ascii=False)
                              .encode("utf-8", "replace")).hexdigest()[:12]
        ident = f"{time.strftime('%Y%m%d-%H%M%S')}-{digest}"
        return os.path.join(self.spool_dir, "frozen", ident), ident

    def _quarantine(self, line, error):
        with open(self._sp(INVALID_FILE), "a") as f:
            f.write(json.dumps({"ts": time.strftime("%F %T"), "line": line,
                                "error": error}, ensure_ascii=False) + "\n")

    def poll_once(self):
        """一轮尾随 + 固化；返回 {"frozen": [...]}，有跳过时附
        "skipped_existing": [...]（键仅在非空时出现——重扫护栏跳过的 id，
        T6 断言以此区分 frozen/skipped）。"""
        # 1) 两 CSV 尾随入窗（首轮先回放文件尾暖窗——重启不丢最近 ≤120s）
        for name, rel in self.SOURCES:
            path = os.path.join(self.data_root, *rel)
            if not self._warmed:
                try:
                    self._csv_offsets[str(path)] = self._warmup_tail(name, path)
                except OSError:
                    self._csv_offsets[str(path)] = 0   # 读失败 → 交 tail_file 从头接管
            lines, self._csv_offsets = tail_file(path, self._csv_offsets)
            for l in lines:
                self.window.push(name, l)
        self._warmed = True
        # 2) events.jsonl 新行：red/black → freeze（行级兜底：坏行隔离不炸守护）
        frozen, skipped, added = [], [], False
        ev_path = self._sp(EVENTS_FILE)
        ev_lines, store = tail_file(ev_path, {str(ev_path): self.event_offset})
        self.event_offset = store.get(str(ev_path), self.event_offset)
        for line in ev_lines:
            try:
                ev = json.loads(line)
                if not isinstance(ev, dict):
                    raise ValueError(f"event line is {type(ev).__name__}, not an object")
                if ev.get("severity") in FROZEN_SEVERITIES:
                    out_dir, ident = self._freeze_target(ev)
                    if ident in self._frozen_ids or self._has_frozen_files(out_dir):
                        # 重扫护栏：当前窗对旧事件已是时间错位数据，
                        # 覆盖即静默销毁既有取证证据（Important-1）
                        skipped.append(ident)
                        print(f"[ring] skip re-freeze {ident}: frozen evidence already "
                              f"exists (offset 重扫不覆盖取证证据)", file=sys.stderr)
                        continue
                    freeze(self.window, out_dir, ident)
                    self._frozen_ids.add(ident)
                    added = True
                    frozen.append(ident)
            except Exception as e:
                self._quarantine(line, repr(e))
        if added:
            self._save_frozen_ids()
        self._save_state()
        out = {"frozen": frozen}
        if skipped:
            out["skipped_existing"] = skipped
        return out

# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        description="sdc-ring：120s 滚动热窗口 + red/black 事件固化（v5 §6.3）")
    ap.add_argument("--once", action="store_true",
                    help="跑一个完整轮次后退出（测试/演练模式）")
    ap.add_argument("--data-root", default=None, help="数据根（默认 $SDC_EXCITE_REPRODUCE_DIR"
                     "/$SDC_CAMPAIGN_DIR/~/sdc-excite-reproduce）")
    a = ap.parse_args(argv)
    root = a.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") or \
        os.environ.get("SDC_CAMPAIGN_DIR") or os.path.expanduser("~/sdc-excite-reproduce")
    loop = RingLoop(root, os.path.join(root, "spool"))
    if a.once:
        ids = loop.poll_once()["frozen"]
        print(f"[ring] once: frozen {len(ids)} event(s)"
              + (f": {', '.join(ids)}" if ids else ""))
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

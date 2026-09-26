#!/usr/bin/env python3
"""sdc_controller.py — 五态规则状态机控制器（v5 §8.2/§8.3，M2 Task 3）。stdlib only。

消费 spool/events.jsonl（sdc-eventd 产出的 canonical events，尾随复用 T1 的
tail_file 单一实现）→ 规则驱动五态机（green/yellow/orange/red/black）→ 决策
全量入 append-only 账本 spool/controller_ledger.jsonl + 动作请求落 cmd/
（文件协议 v5 §13.3 最小版）。

设计约束（v5 §8.2 重放确定性）：
  - StateMachine.feed 纯函数式：只依赖当前 state + 事件 + 规则，无时钟无随机；
  - 决策账本行带 ts（观测时刻）；replay() 返回**不含 ts 的规范形**——同一事件
    序列两次重放输出逐项相等（ts 会让 diff 恒非空，故剥离，M2 退出标准核心）；
  - 规则 when 全字段等值匹配（首条命中即用）；transitions 表约束合法性——
    不合法转换记 ignored=true 不动状态不分派动作（决策行 to 记规则试图去的
    目标态——诚实记录"被拒绝的转换"）；
  - 无规则命中（如 interlock RESUME/green 恢复行、note 事件）不产生决策行
    ——事件流本身即入站记录（events.jsonl），账本只记决策；
  - heartbeat：守护模式每轮无新事件时写 {"rule":"heartbeat","from":s,"to":s,
    "actions":[],"ignored":false} 决策行（T6 部署验收的活体证据）；
    --once 模式不写（测试幂等断言依赖）；
  - 状态 + events.jsonl 消费 offset 持久化 spool/controller_state.json
    （tmp + os.replace 原子写，T2 教训：撕裂状态文件导致破坏性重扫）——
    守护重启不丢 RED 状态、不重放旧决策；状态文件损坏时响亮崩溃（RuntimeError）
    而非静默回 green——green 谎言是安全缺陷；轮末落盘，中途崩溃重启会重读
    本轮事件 → 账本出现重复决策行（可见于 replay diff，诚实信号，有界）；
  - 行级兜底：events.jsonl 坏 JSON 行入 spool/controller_invalid.jsonl 隔离
    后继续，offset 照常推进（T1 毒丸教训同类关法——残行不炸守护）；
  - 动作分派（本版最小）：snapshot_root → touch cmd/snapshot.request（root
    monitor 现有代理即吃）；pmu_burst → cmd/burst.request（固定名单槽，同轮
    多次触发后写覆盖先写——最新事件胜出，expires_at 300s 到期由 helper 侧
    拒绝）；其余动作（freeze_ring——sdc_ring 对 red/black 已自动做；
    enqueue_reproduction/hold_profile——M3 消费者；alert_only/verify_only——
    T5 扩展）本版只入账本，动作行即接口契约；
  - burst 事件 cpu：event.location.logical_cpu 存在则 [cpu]，否则 []（未定位
    ——discrete 事件无 location 字段时诚实留空，helper 侧 perf -a 全核语义）。
"""
import argparse, json, os, signal, sys, time, uuid
from datetime import datetime, timedelta, timezone

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "telemetry"))
from sdc_eventd import tail_file            # T1 单一实现，未重写

STATES = ("green", "yellow", "orange", "red", "black")
ACTIONS = ("freeze_ring", "snapshot_root", "enqueue_reproduction", "hold_profile",
           "pmu_burst", "alert_only", "verify_only")
POLL_INTERVAL_S = 2.0
STATE_FILE = "controller_state.json"        # 状态 + 消费 offset（原子写）
LEDGER_FILE, INVALID_FILE = "controller_ledger.jsonl", "controller_invalid.jsonl"
EVENTS_FILE = "events.jsonl"
BURST_DURATION_S, BURST_EXPIRES_S = 60, 300

DEFAULT_RULES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..",
                             "configs", "sdc-excite-reproduce", "rules_m2.json")


def _now():
    return time.strftime("%F %T")


def load_rules(path):
    """载入并校验规则文件——配置错误必须响亮失败（ValueError），不静默降级。"""
    with open(path, encoding="utf-8") as f:
        rules = json.load(f)
    where = f"{path}: "
    if rules.get("schema_version") != "1":
        raise ValueError(where + "schema_version != '1'")
    rule_list = rules.get("rules")
    if not isinstance(rule_list, list) or not rule_list:
        raise ValueError(where + "rules 必须为非空 list")
    ids = set()
    for r in rule_list:
        for k in ("id", "when", "transition", "actions"):
            if k not in r:
                raise ValueError(where + f"规则缺 {k}: {r!r}")
        if r["id"] in ids:
            raise ValueError(where + f"规则 id 重复: {r['id']}")
        ids.add(r["id"])
        if r["transition"] not in STATES:
            raise ValueError(where + f"{r['id']} transition 非法: {r['transition']!r}")
        bad = [a for a in r["actions"] if a not in ACTIONS]
        if bad:
            raise ValueError(where + f"{r['id']} actions 超出固定枚举 {ACTIONS}: {bad}")
        if not isinstance(r["when"], dict) or not r["when"]:
            raise ValueError(where + f"{r['id']} when 必须为非空等值匹配 dict")
    table = rules.get("transitions")
    if not isinstance(table, dict) or not table:
        raise ValueError(where + "transitions 表缺失")
    for src, row in table.items():
        if src not in STATES:
            raise ValueError(where + f"transitions 源状态非法: {src!r}")
        for dst in row:
            if dst not in STATES:
                raise ValueError(where + f"transitions {src}->{dst} 目标状态非法: {dst!r}")
    return rules


# ---------------------------------------------------------------------------
# 五态状态机（重放确定性的根基）

class StateMachine:
    """规则驱动的五态机。feed 纯函数式：无时钟无随机，决策只由
    (当前 state, 事件, 规则) 决定。

    feed(event) -> list[action]：分派的动作名列表（不合法转换/无命中 → []）；
    最近一次决策明细在 .last_decision（{"rule","from","to","actions","ignored"}
    或 None=无规则命中）——纯派生量，供 ControllerLoop/replay 组决策行。
    """

    def __init__(self, rules):
        self.rules = rules
        self.state = "green"
        self.last_decision = None

    def decide(self, event):
        """纯决策（不改状态）：首条 when 全字段等值命中的规则生效。"""
        for rule in self.rules["rules"]:
            if all(event.get(k) == v for k, v in rule["when"].items()):
                frm, to = self.state, rule["transition"]
                legal = rule["id"] in self.rules["transitions"].get(frm, {}).get(to, [])
                return {"rule": rule["id"], "from": frm, "to": to,
                        "actions": list(rule["actions"]) if legal else [],
                        "ignored": not legal}
        return None                                # 无规则命中——不产生决策

    def feed(self, event):
        d = self.decide(event)
        self.last_decision = d
        if d is None or d["ignored"]:
            return []
        self.state = d["to"]
        return list(d["actions"])


def replay(events_file, rules_path):
    """离线重放事件文件 → 规范形决策列表（不含 ts——确定性核心，v5 §8.2）。

    同一文件两次重放输出逐项相等（feed 纯函数式 + 无 ts 无随机）；与账本比对
    时账本行去 ts 后应与此列表相等（M2 退出标准）。坏 JSON 行：离线工具诚实
    报错（ValueError 带行号），不静默跳过。
    """
    m = StateMachine(load_rules(rules_path))
    out = []
    with open(events_file, encoding="utf-8") as f:
        for lineno, line in enumerate(f, 1):
            if not line.strip():
                continue
            try:
                event = json.loads(line)
            except json.JSONDecodeError as e:
                raise ValueError(f"{events_file}:{lineno}: 坏 JSON 行: {e}") from e
            m.feed(event)
            if m.last_decision is not None:
                out.append({"event_id": event.get("event_id"), **m.last_decision})
    return out


# ---------------------------------------------------------------------------
# 事件循环

class ControllerLoop:
    """一轮 poll_once = 尾随 spool/events.jsonl → feed → 决策入账本 + 动作落 cmd/。"""

    def __init__(self, data_root, spool_dir, rules_path, heartbeat=True):
        self.data_root, self.spool_dir = data_root, spool_dir
        self.cmd_dir = os.path.join(data_root, "cmd")
        os.makedirs(spool_dir, exist_ok=True)
        os.makedirs(self.cmd_dir, exist_ok=True)
        self.rules_path, self.heartbeat = rules_path, heartbeat
        self.machine = StateMachine(load_rules(rules_path))
        self.offsets = {}
        self._load_state()

    def _sp(self, name):
        return os.path.join(self.spool_dir, name)

    def _load_state(self):
        """重启恢复状态 + offset。文件损坏 → RuntimeError 响亮崩溃（不装 green）。"""
        try:
            with open(self._sp(STATE_FILE), encoding="utf-8") as f:
                st = json.load(f)
        except FileNotFoundError:
            return                                  # 首启：green + offset 0
        except json.JSONDecodeError as e:
            raise RuntimeError(f"{STATE_FILE} 损坏（原子写下不应发生）: {e}") from e
        if (not isinstance(st, dict) or st.get("state") not in STATES
                or not isinstance(st.get("offsets"), dict)):
            raise RuntimeError(f"{STATE_FILE} 结构非法: {st!r}")
        self.machine.state = st["state"]
        self.offsets = st["offsets"]

    def _save_state(self):
        """原子写（tmp + os.replace）——崩溃不留半截 JSON（T2 教训）。"""
        tmp = self._sp(STATE_FILE + ".tmp")
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump({"state": self.machine.state, "offsets": self.offsets}, f,
                      ensure_ascii=False, indent=1)
        os.replace(tmp, self._sp(STATE_FILE))

    @staticmethod
    def _append_jsonl(path, obj):
        with open(path, "a", encoding="utf-8") as f:
            f.write(json.dumps(obj, ensure_ascii=False) + "\n")

    def _dispatch(self, actions, event):
        """动作 → cmd/ 文件协议（v5 §13.3 最小版）。未列动作本版只入账本。"""
        for a in actions:
            if a == "snapshot_root":
                path = os.path.join(self.cmd_dir, "snapshot.request")
                with open(path, "a"):              # 存在则不重写内容
                    pass
                os.utime(path, None)               # touch 语义：刷新 mtime
            elif a == "pmu_burst":
                cpu = (event.get("location") or {}).get("logical_cpu")
                req = {"schema_version": "1", "action_id": uuid.uuid4().hex,
                       "action": "perf_burst",
                       "parameters": {"duration_s": BURST_DURATION_S,
                                      "cpus": [cpu] if cpu is not None else []},
                       "expires_at": (datetime.now(timezone.utc)
                                      + timedelta(seconds=BURST_EXPIRES_S)).isoformat(),
                       "reason_event_ids": [event.get("event_id")]}
                tmp = os.path.join(self.cmd_dir, "burst.request.tmp")
                with open(tmp, "w", encoding="utf-8") as f:   # 原子写：helper 不读半截
                    json.dump(req, f, ensure_ascii=False)
                os.replace(tmp, os.path.join(self.cmd_dir, "burst.request"))

    def poll_once(self):
        """一轮：尾随 → 逐事件决策入账本 + 动作分派。返回本轮决策行（含 ts）。"""
        lines, self.offsets = tail_file(self._sp(EVENTS_FILE), self.offsets)
        decisions = []
        for line in lines:
            try:
                event = json.loads(line)
            except json.JSONDecodeError as e:      # 行级兜底：坏行隔离不炸守护
                self._append_jsonl(self._sp(INVALID_FILE),
                                   {"ts": _now(), "line": line, "error": repr(e)})
                continue
            actions = self.machine.feed(event)
            if self.machine.last_decision is None:
                continue                            # 无规则命中——不产生决策行
            dec = {"ts": _now(), "event_id": event.get("event_id"),
                   **self.machine.last_decision}
            self._append_jsonl(self._sp(LEDGER_FILE), dec)
            self._dispatch(actions, event)         # ignored 时 actions 恒为 []
            decisions.append(dec)
        if not lines and self.heartbeat:
            dec = {"ts": _now(), "rule": "heartbeat", "from": self.machine.state,
                   "to": self.machine.state, "actions": [], "ignored": False}
            self._append_jsonl(self._sp(LEDGER_FILE), dec)
            decisions.append(dec)
        self._save_state()
        return decisions


# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(description="sdc-controller：五态规则状态机（v5 §8）")
    ap.add_argument("--once", action="store_true",
                    help="跑一个完整轮次后退出（测试/演练模式；不写 heartbeat）")
    ap.add_argument("--data-root", default=None,
                    help="数据根（默认 $SDC_EXCITE_REPRODUCE_DIR/$SDC_CAMPAIGN_DIR"
                         "/~/sdc-excite-reproduce）")
    ap.add_argument("--rules", default=DEFAULT_RULES,
                    help=f"规则文件（默认 {DEFAULT_RULES}）")
    a = ap.parse_args(argv)
    root = a.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") or \
        os.environ.get("SDC_CAMPAIGN_DIR") or os.path.expanduser("~/sdc-excite-reproduce")
    loop = ControllerLoop(root, os.path.join(root, "spool"), a.rules,
                          heartbeat=not a.once)
    if a.once:
        dec = loop.poll_once()
        n = sum(1 for d in dec if d["rule"] != "heartbeat")
        print(f"[controller] once: {n} decisions, state={loop.machine.state}")
        return 0
    stop = [False]
    def _term(signum, frame):
        stop[0] = True
    signal.signal(signal.SIGTERM, _term)
    signal.signal(signal.SIGINT, _term)
    while not stop[0]:
        loop.poll_once()
        end = time.monotonic() + POLL_INTERVAL_S   # 分片睡 ≤1s：SIGTERM 后 ≤1s 内退出
        while not stop[0] and time.monotonic() < end:
            time.sleep(min(1.0, end - time.monotonic()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

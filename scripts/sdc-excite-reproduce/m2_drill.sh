#!/bin/bash
# m2_drill.sh — M2 退出标准演练（v5 §14.2：合成 mismatch/RAS/温度越限/collector 降级
# 均触发正确动作且可回放；§8.2 重放确定性）。Task 5 交付，人工/CI 驱动器。
#
# 期望状态转换序列（注入顺序=events.jsonl 顺序——eventd 源序固定为
# ledger→discrete→journal→selfmon→interlock，五级链必须逐源分阶段跑）：
#   green ─(① collector 降级 selfmon 丢样行)→ yellow
#        ─(② 离散断言 discrete 0x00→0x01)→ orange  ［pmu_burst → helper --mock-exec 执行］
#        ─(③ 合成 mismatch ledger rc=1)→ red      ［freeze_ring/snapshot_root/…；ring 固化］
#        ─(④ RAS 关键字 journal UE 行)→ red 下非法：ignored=true 诚实入账（升级占优，无二次分派）
#        ─(⑤ 温度越限 ALERT PAUSE)→ black        ［verify_only → cmd/verify.request］
#
# 断言（全过 exit 0，任一失败 exit 1）：
#   A. 决策序列==上表（5 行，含 ④ ignored）+ 终态 black
#   B. 重放确定性（v5 §8.2）：replay(events.jsonl) == 账本去 ts；两次重放相等
#   C. ring 已固化：3 个 red/black 事件各 events/<id>/ring_window/，>120s 陈旧行淘汰
#   D. burst 全链：请求 .done 终态化 + spool/bursts/<action_id>.csv（伪 CSV）+ 审计 done 行
#   E. verify/snapshot 请求在案；静止复跑零新事件/零决策/零固化（offset 幂等）
#
# 安全边界：全程 --once 模式 + 隔离数据根（绝不部署、绝不碰真实服务/真实 cmd/真实
# sysfs 写）；helper --mock-exec（伪 perf CSV，绝不跑真 perf；非 perf_burst 特权动作
# 一律拒绝）；数据根必须为全新目录——已含 spool/ 即拒绝（防误对真实战役根/重跑污染，
# 真实根必含 spool/）。
#
# 用法: bash m2_drill.sh <隔离数据根>    # 无参/空参拒绝——绝不缺省到真实根
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
EVENTD="$REPO/tools/telemetry/sdc_eventd.py"
RING="$REPO/tools/telemetry/sdc_ring.py"
CONTROLLER="$REPO/tools/control/sdc_controller.py"
HELPER="$REPO/tools/control/sdc_root_helper.py"
RULES="$REPO/configs/sdc-excite-reproduce/rules_m2.json"

if [ "$#" -ne 1 ]; then
    echo "用法: $0 <隔离数据根>（必填——无参拒绝，防误对真实根）" >&2
    exit 1
fi
ROOT="$1"
if [ -z "$ROOT" ]; then
    echo "✗ 数据根不能为空" >&2
    exit 1
fi
if [ -e "$ROOT/spool" ]; then
    echo "✗ $ROOT 已含 spool/——演练须用全新隔离数据根（防误对真实战役根/重跑污染）" >&2
    exit 1
fi
mkdir -p "$ROOT"/events "$ROOT"/monitor "$ROOT"/spool "$ROOT"/cmd

TS() { date '+%F %T'; }
run_once() {  # <tool> [extra args...] —— 四守护统一 --once + 隔离根入口
    python3 "$1" --once --data-root "$ROOT" "${@:2}"
}

echo "== m2_drill: 隔离根 $ROOT =="

# ring 热窗种子：fresh 行入窗（断言 C 的固化内容）+ 一条 >120s 陈旧行（断言淘汰口径）
{
    echo "ts,source,note"
    echo "$(TS),m2_drill_marker,temperature_c=42.0"
    echo "2020-01-01 00:00:00,stale_line_over_120s,must_not_freeze"
} > "$ROOT/monitor/monitor.csv"
echo "$(TS),cpu7,m2_drill_percore_marker" > "$ROOT/monitor/percore.csv"

stage() {  # <名称> <源相对路径> <追加行...>：注入 → eventd→ring→controller→helper
    local name="$1" rel="$2"; shift 2
    echo "-- stage: $name"
    printf '%s\n' "$@" >> "$ROOT/$rel"
    run_once "$EVENTD"
    run_once "$RING"
    run_once "$CONTROLLER" --rules "$RULES"
    run_once "$HELPER" --mock-exec
}

stage "① collector 降级（→yellow）" "monitor/collector_self.csv" \
    "ts,collector,samples_total,samples_dropped,period_s,loop_duration_s,last_success_ts,rss_kb" \
    "$(TS),pmu,1200,3,2.0,2.500,,96000"

stage "② 离散断言（→orange，pmu_burst）" "monitor/discrete_events.log" \
    "[$(TS)] TRANSITION cpu1_prochot: 0x00 → 0x01"

stage "③ 合成 mismatch（→red，ring 固化）" "events/ledger.csv" \
    "$(TS),m2_e2e,rc=1,test=memcpy_rewr,seed=AES:ab"

stage "④ RAS 关键字（red 下非法→ignored 诚实入账）" "monitor/journal_watch.log" \
    "[$(TS)] UE>0 mc0 ce=0 ue=2 告警"

stage "⑤ 温度越限 interlock（→black，verify_only）" "monitor/alerts.log" \
    "[$(TS)] ALERT PAUSE: thermal CPU 96C >= 95C（M2 演练合成）"

echo "== 断言 =="
python3 - "$ROOT" "$REPO" <<'PYEOF'
import glob, json, os, sys
root, repo = sys.argv[1], sys.argv[2]
sys.path.insert(0, os.path.join(repo, "tools", "control"))
sys.path.insert(0, os.path.join(repo, "tools", "telemetry"))
import sdc_controller as sc

rules = os.path.join(repo, "configs", "sdc-excite-reproduce", "rules_m2.json")

def fail(msg):
    print(f"✗ {msg}", file=sys.stderr)
    sys.exit(1)

def load_jsonl(path):
    return [json.loads(l) for l in open(path) if l.strip()]

# A. 事件序列 + 决策序列 + 终态
events = load_jsonl(f"{root}/spool/events.jsonl")
types = [e["event_type"] for e in events]
if types != ["collector_degraded", "discrete_transition", "sdc_mismatch",
             "ras_keyword", "interlock_action"]:
    fail(f"事件序列非预期: {types}")
led = load_jsonl(f"{root}/spool/controller_ledger.jsonl")
shape = [(d["rule"], d["from"], d["to"], d["ignored"]) for d in led]
EXPECT = [("collector_degraded", "green", "yellow", False),
          ("discrete_assert", "yellow", "orange", False),
          ("exact_mismatch", "orange", "red", False),
          ("ras_keyword", "red", "red", True),
          ("safety_interlock", "red", "black", False)]
if shape != EXPECT:
    fail(f"决策序列非预期:\n  got      {shape}\n  expected {EXPECT}")
if led[2]["actions"] != ["freeze_ring", "snapshot_root",
                         "enqueue_reproduction", "hold_profile"]:
    fail(f"mismatch 动作非预期: {led[2]['actions']}")
if led[4]["actions"] != ["verify_only"]:
    fail(f"interlock 动作非预期: {led[4]['actions']}")
state = json.load(open(f"{root}/spool/controller_state.json"))
if state["state"] != "black":
    fail(f"终态非 black: {state}")

# B. 重放确定性（v5 §8.2 M2 退出标准）：账本去 ts == replay(events.jsonl)
stripped = [{k: v for k, v in d.items() if k != "ts"} for d in led]
evf = f"{root}/spool/events.jsonl"
rep = sc.replay(evf, rules)
if stripped != rep:
    fail(f"重放与账本不一致:\n  ledger-ts {stripped}\n  replay    {rep}")
if sc.replay(evf, rules) != sc.replay(evf, rules):
    fail("两次重放输出不一致（确定性破坏）")

# C. ring 固化：3 个 red/black 事件 + 陈旧行淘汰（120s 滚动口径）
frozen = set(json.load(open(f"{root}/spool/ring_frozen.json")))
redblack = {e["event_id"] for e in events if e["severity"] in ("red", "black")}
if frozen != redblack or len(frozen) != 3:
    fail(f"ring 固化集非预期: {sorted(frozen)} vs {sorted(redblack)}")
for eid in redblack:
    mon = f"{root}/events/{eid}/ring_window/monitor.csv"
    if not os.path.exists(mon):
        fail(f"缺固化窗口 {mon}")
    body = open(mon).read()
    if "m2_drill_marker" not in body:
        fail(f"{mon} 缺热窗种子行")
    if "must_not_freeze" in body:
        fail(f"{mon} 含 >120s 陈旧行（滚动窗口淘汰口径破坏）")
    pc = f"{root}/events/{eid}/ring_window/percore.csv"
    if not (os.path.exists(pc) and "m2_drill_percore_marker" in open(pc).read()):
        fail(f"缺 percore 固化窗口或种子行: {pc}")

# D. burst 全链：.done 终态 + 伪 CSV + 审计 done（helper 真执行——mock 输出）
if glob.glob(f"{root}/cmd/burst-*.request"):
    fail("存在未终态化 burst 请求")
done = glob.glob(f"{root}/cmd/burst-*.request.done.*")
if len(done) != 1:
    fail(f"burst .done 终态数非 1: {done}")
req = json.load(open(done[0]))
if req["action"] != "perf_burst" or req["parameters"] != {"duration_s": 60, "cpus": []}:
    fail(f"burst 请求形状非预期: {req}")
if req["reason_event_ids"] != [led[1]["event_id"]]:
    fail(f"burst reason 非离散断言事件: {req['reason_event_ids']}")
csvp = f"{root}/spool/bursts/{req['action_id']}.csv"
if not os.path.exists(csvp) or "mock" not in open(csvp).read().lower():
    fail(f"伪 perf CSV 缺失或无 mock 标记: {csvp}")
audit = load_jsonl(f"{root}/spool/root_helper_audit.jsonl")
if len(audit) != 1 or audit[0]["action"] != "perf_burst" or audit[0]["status"] != "done":
    fail(f"审计非预期: {audit}")
if "mock" not in (audit[0]["readback"] or {}).get("perf_return", ""):
    fail(f"审计 perf_return 无 mock 标记: {audit[0]}")

# E. verify/snapshot 请求在案
v = json.load(open(f"{root}/cmd/verify.request"))
if v["action"] != "verify_only" or v["reason_event_ids"] != [led[4]["event_id"]]:
    fail(f"verify.request 非预期: {v}")
if not os.path.exists(f"{root}/cmd/snapshot.request"):
    fail("缺 cmd/snapshot.request（exact_mismatch→snapshot_root）")
print("✓ A 决策序列/终态 black  ✓ B 重放一致  ✓ C ring 固化  ✓ D burst 全链  ✓ E verify/snapshot")
PYEOF

# E2. 静止复跑：零新事件/零决策/零固化（offset 幂等——不重复处理）
OUT1=$(run_once "$EVENTD")
OUT2=$(run_once "$CONTROLLER" --rules "$RULES")
OUT3=$(run_once "$RING")
echo "$OUT1" | grep -q "0 new events" || { echo "✗ 复跑仍有新事件: $OUT1" >&2; exit 1; }
echo "$OUT2" | grep -q "0 decisions" || { echo "✗ 复跑仍有新决策: $OUT2" >&2; exit 1; }
echo "$OUT3" | grep -q "frozen 0" || { echo "✗ 复跑仍有新固化: $OUT3" >&2; exit 1; }
echo "✓ 静止复跑零新事件/零决策/零固化（offset 幂等）"
echo "== m2_drill: 全部断言通过（M2 退出标准演练 PASS）=="

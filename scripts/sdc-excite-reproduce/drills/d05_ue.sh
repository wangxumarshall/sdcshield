#!/bin/bash
# d05_ue.sh — EDAC UE（不可纠正错误）演练（v5 §17.2 第 5 类）。
#
# 注入→观测链（黑/红分级核心：UE 升 red+固化，但不越权入 black）：
#   ① journal_watch.log UE 行（collector@ras 生产格式逐字：ue>0 即写
#      "UE>0 mc0 ce=N ue=M 告警"）→ sdc_eventd → ras_keyword **red**；
#   ② alerts.log UE 即时告警行（monitor 联锁段 :321-328 生产格式逐字：
#      "ALERT EDAC UE>0 total=M（v5 §8.6：即时告警不自动 PAUSE——用户决策点）"）
#      → sdc_eventd → interlock_action **yellow**（action=alert，非 pause）；
#   ③ sdc_controller：ras_keyword red 规则 → RED + snapshot_root + ring 固化；
#      interlock yellow 无规则命中零决策——UE 告警语义 = 用户决策点，
#      绝不自动 PAUSE/black（对照 d04 CE 不升级、d06 联锁才 black）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d05_ue

# ring 热窗种子（red 事件固化内容断言）
{
    echo "ts,source,note"
    echo "$(TS),d05_seed_marker,temperature_c=43.0"
    echo "2020-01-01 00:00:00,stale_line_over_120s,must_not_freeze"
} > "$DRILL_ROOT/monitor/monitor.csv"

# ---- 注入：UE 计数差值——collector/monitor 两生产格式逐字 ----
echo "[$(TS)] UE>0 mc0 ce=0 ue=2 告警" >> "$DRILL_ROOT/monitor/journal_watch.log"
echo "[$(TS)] ALERT EDAC UE>0 total=2（v5 §8.6：即时告警不自动 PAUSE——用户决策点）" \
    >> "$DRILL_ROOT/monitor/alerts.log"

run_eventd
run_ring
run_controller

# 断言①：UE 行 → ras_keyword red（分级判据=行含 UE）
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==2 and d[0]["event_type"]=="ras_keyword" and d[0]["severity"]=="red"
    and "UE>0" in d[0]["source_line"]' \
    "UE 行 → ras_keyword red 事件"
# 断言②：即时告警 → interlock yellow（action=alert——非 pause，不自动 PAUSE）
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'd[1]["event_type"]=="interlock_action" and d[1]["severity"]=="yellow"
    and d[1]["interlock"]["action"]=="alert"' \
    "UE 即时告警 → interlock_action yellow（alert——用户决策点）"

# 断言③：控制器 red（非 black）+ 唯一决策 ras_keyword + snapshot 请求
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="red"' \
    "controller 终态 red（UE 升 red）"
jlassert "$DRILL_ROOT/spool/controller_ledger.jsonl" \
    'len(d)==1 and d[0]["rule"]=="ras_keyword" and d[0]["from"]=="green"
    and d[0]["to"]=="red" and d[0]["actions"]==["snapshot_root"]' \
    "唯一决策 ras_keyword→red+snapshot_root（interlock yellow 零决策——不自动 PAUSE）"
assert_exists "$DRILL_ROOT/cmd/snapshot.request" "snapshot_root 动作落盘"
jassert "$DRILL_ROOT/spool/ring_frozen.json" 'len(d)==1' "ring 固化恰 1 个 red 事件"
FROZEN_ID=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))[0])' \
    "$DRILL_ROOT/spool/ring_frozen.json")
assert_contains "$DRILL_ROOT/events/$FROZEN_ID/ring_window/monitor.csv" "d05_seed_marker" \
    "UE red 事件固化窗口含热窗种子行"

drill_pass

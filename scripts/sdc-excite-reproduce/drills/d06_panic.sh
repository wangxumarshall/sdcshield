#!/bin/bash
# d06_panic.sh — 内核 panic 演练（v5 §17.2 第 6 类）。
#
# 注入→观测链（两段——panic 事件的 red 定级 + 系统级失效联锁的 black 兜底）：
#   ① journal_watch.log panic 行（collector RAS 关键字流：行含 panic → red）
#      → sdc_eventd → ras_keyword **red** → sdc_controller → RED +
#      snapshot_root + ring 固化（panic 瞬间的 120s 窗口证据先固化）；
#   ② 系统失效联锁（panic 后 fail-safe：降载而非盲跑，v5 §8.6）：
#      alerts.log ALERT PAUSE 行 → sdc_eventd → interlock_action **black**
#      → sdc_controller red→black（合法升级）+ verify_only → cmd/verify.request。
#
# 断言（≥2）：panic 行 red 定级+固化 / PAUSE 联锁 black 终态 + verify 请求
# + 决策序列逐项（panic 前先 red 再 black——升级占优语义）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d06_panic

# ring 热窗种子（panic 瞬间窗口证据固化断言）
{
    echo "ts,source,note"
    echo "$(TS),d06_seed_marker,temperature_c=44.0"
} > "$DRILL_ROOT/monitor/monitor.csv"

# ---- 注入：journal panic 行 + 系统失效联锁 PAUSE 行 ----
echo "[$(TS)] RAS Kernel panic - not syncing: Attempted to kill init（M5 演练 panic 注入）" \
    >> "$DRILL_ROOT/monitor/journal_watch.log"
echo "[$(TS)] ALERT PAUSE: kernel panic 系统失效防护 fail-safe（M5 演练）" \
    >> "$DRILL_ROOT/monitor/alerts.log"

run_eventd
run_ring
run_controller

# 断言①：panic 行 → ras_keyword red；PAUSE 行 → interlock black
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==2 and d[0]["event_type"]=="ras_keyword" and d[0]["severity"]=="red"
    and "panic" in d[0]["source_line"]' \
    "panic 行 → ras_keyword red 事件"
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'd[1]["event_type"]=="interlock_action" and d[1]["severity"]=="black"
    and d[1]["interlock"]["action"]=="pause"' \
    "系统失效联锁 PAUSE → interlock_action black"

# 断言②：决策序列 panic 先 red（+snapshot）再 black（+verify_only）——升级占优
jlassert "$DRILL_ROOT/spool/controller_ledger.jsonl" \
    'len(d)==2 and d[0]["rule"]=="ras_keyword" and d[0]["from"]=="green"
    and d[0]["to"]=="red" and d[0]["actions"]==["snapshot_root"]
    and d[1]["rule"]=="safety_interlock" and d[1]["from"]=="red"
    and d[1]["to"]=="black" and d[1]["actions"]==["verify_only"]' \
    "决策序列：panic→red+snapshot，联锁→black+verify_only"
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="black"' \
    "controller 终态 black（系统失效 fail-safe）"

# 断言③：证据双固化（red+black 两事件）+ verify 请求 + panic 窗口种子
jassert "$DRILL_ROOT/spool/ring_frozen.json" 'len(d)==2' \
    "ring 固化 red+black 两事件（panic 瞬间窗口证据保全）"
PANIC_ID=$(python3 -c '
import json, sys
for l in open(sys.argv[1]):
    e = json.loads(l)
    if e["severity"] == "red":
        print(e["event_id"]); break' "$DRILL_ROOT/spool/events.jsonl")
assert_contains "$DRILL_ROOT/events/$PANIC_ID/ring_window/monitor.csv" "d06_seed_marker" \
    "panic red 事件固化窗口含热窗种子行"
V=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["action"])' \
    "$DRILL_ROOT/cmd/verify.request")
assert_eq "$V" "verify_only" "cmd/verify.request 动作=verify_only（BLACK 只读校验）"

drill_pass

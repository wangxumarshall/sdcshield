#!/bin/bash
# d01_synthetic_mismatch.sh — 合成 byte mismatch 演练（v5 §17.2 第 1 类）。
#
# 注入→观测链（全真实工具 --once 路径，m2_drill 同款）：
#   ledger 行（rc=1+test+seed——战役失败行口径）→ sdc_eventd → sdc_mismatch
#   red 事件 → sdc_controller（exact_mismatch 规则）→ RED + 动作账本
#   （freeze_ring/snapshot_root/enqueue_reproduction/hold_profile）→ sdc_ring
#   固化 120s 窗口 → enqueue_repro（生产产出端）→ spool/repro_queue/ 有行
#   （消费端 --from-queue 属 m3_drill 全链覆盖，此处消费可选不重复跑）。
#
# 断言（≥2）：
#   1. events.jsonl 有 sdc_mismatch red 事件且 test/seed 与注入行一致；
#   2. controller 终态 red + exact_mismatch 决策含全部四动作；
#   3. ring 固化：事件窗口落 events/<id>/ring_window/（fresh 种子行在、
#      >120s 陈旧行淘汰）；
#   4. repro_queue 有行且 event_dir/test/seed 字段齐（复现闭环入口在案）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d01_synthetic_mismatch

SEED="AES:d01drill$(printf '%052d' 0)"

# ring 热窗种子：fresh 行入窗（固化内容断言）+ >120s 陈旧行（滚动淘汰口径）
{
    echo "ts,source,note"
    echo "$(TS),d01_seed_marker,temperature_c=42.0"
    echo "2020-01-01 00:00:00,stale_line_over_120s,must_not_freeze"
} > "$DRILL_ROOT/monitor/monitor.csv"
echo "$(TS),cpu7,d01_percore_marker" > "$DRILL_ROOT/monitor/percore.csv"

# ---- 注入：ledger 合成 mismatch 行（战役失败行同构） ----
echo "$(TS),d01_drill,rc=1,test=zstd19,seed=$SEED" >> "$DRILL_ROOT/events/ledger.csv"

run_eventd
run_ring
run_controller

EVID="$DRILL_ROOT"   # 证据目录（drills/out/d01_synthetic_mismatch/root）

# 断言 1：事件形态
jlassert "$EVID/spool/events.jsonl" \
    'len(d)==1 and d[0]["event_type"]=="sdc_mismatch" and d[0]["severity"]=="red"
    and d[0]["test"]["id"]=="zstd19" and d[0]["test"]["seed"]=="'"$SEED"'"' \
    "sdc_mismatch red 事件入流（test/seed 与注入一致）"
EID=$(python3 -c 'import json,sys;print(json.loads(open(sys.argv[1]).readline())["event_id"])' "$EVID/spool/events.jsonl")

# 断言 2：控制器终态 red + 四动作
jassert "$EVID/spool/controller_state.json" 'd["state"]=="red"' "controller 终态 red"
jlassert "$EVID/spool/controller_ledger.jsonl" \
    'len(d)==1 and d[0]["rule"]=="exact_mismatch" and d[0]["from"]=="green"
    and d[0]["to"]=="red" and d[0]["ignored"] is False and
    d[0]["actions"]==["freeze_ring","snapshot_root","enqueue_reproduction","hold_profile"]' \
    "exact_mismatch 决策含 freeze_ring/snapshot_root/enqueue_reproduction/hold_profile"
assert_exists "$EVID/cmd/snapshot.request" "snapshot_root 动作落盘 cmd/snapshot.request"

# 断言 3：ring 固化 + 陈旧行淘汰
jassert "$EVID/spool/ring_frozen.json" 'd==["'"$EID"'"]' "ring 固化集恰为该事件"
assert_contains "$EVID/events/$EID/ring_window/monitor.csv" "d01_seed_marker" \
    "固化窗口含热窗种子行"
assert_not_contains "$EVID/events/$EID/ring_window/monitor.csv" "must_not_freeze" \
    "固化窗口已淘汰 >120s 陈旧行"
assert_contains "$EVID/events/$EID/ring_window/percore.csv" "d01_percore_marker" \
    "固化窗口含 percore 种子行"

# 断言 4：复现队列入口在案（生产产出端真实路径）
EVDIR="$EVID/events/$(date +%Y%m%d-%H%M%S)-d01syn"
mkdir -p "$EVDIR"
enqueue_repro_drill "$EVDIR" zstd19 "$SEED"
QF=$(echo "$EVID"/spool/repro_queue/*.json)
[ -f "$QF" ] || fail "repro_queue 无队列文件"
jassert "$QF" '"event_dir" in d and d["test"]=="zstd19" and d["seed"]=="'"$SEED"'"' \
    "repro_queue 行 event_dir/test/seed 齐（消费闭环入口在案）"

drill_pass

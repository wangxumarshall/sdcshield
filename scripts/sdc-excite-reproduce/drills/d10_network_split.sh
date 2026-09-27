#!/bin/bash
# d10_network_split.sh — 网络断开（network split）演练（v5 §17.2 第 10 类）。
#
# 真断网不可做（战役+9 服务运行中）——模拟注入：远端不可达注记 + fail-safe
# 联锁链（v5 §8.6"关键采集断链/失联 → BLACK（fail-safe：降载而非盲跑）"）。
#
# 注入→观测链：
#   ① 断链注入：monitor/net_split.flag 离线标记（远端地址/时刻/原因注记）
#      + alerts.log ALERT PAUSE 行（monitor set_pause 生产格式逐字——采集
#      断链 fail-safe 降载）→ sdc_eventd → interlock_action **black** →
#      sdc_controller → BLACK + verify_only 请求（降载而非盲跑）；
#   ② 恢复协议：远端恢复注记（标记文件移除）+ alerts.log ALERT RESUME 行
#      （monitor clr_pause_if 生产格式逐字）→ sdc_eventd → interlock_action
#      green（resume）→ 控制器诚实不动：BLACK 终态不自动翻绿（恢复收敛
#      属人工/restore 动作——d13 演练该收敛路径），RESUME 事件入流可审计。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d10_network_split

# ---- 注入①：离线标记 + 采集断链 PAUSE（fail-safe 降载） ----
REMOTE="10.200.179.13:9200"   # 远端采集端注记（演练虚构地址）
{
    echo "net_split: 远端 ${REMOTE} 不可达（icmp unreachable×3 连续 2 周期）"
    echo "since: $(date -Is)"
    echo "note: M5 演练模拟注入——真断网不可做"
} > "$DRILL_ROOT/monitor/net_split.flag"
echo "[$(TS)] ALERT PAUSE: 采集断链 net_split 远端 ${REMOTE} 不可达（fail-safe 降载，M5 演练）" \
    >> "$DRILL_ROOT/monitor/alerts.log"

run_eventd
run_controller

# 断言①：断链 → interlock black → 控制器 BLACK + verify 请求（降载而非盲跑）
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==1 and d[0]["event_type"]=="interlock_action"
    and d[0]["severity"]=="black" and d[0]["interlock"]["action"]=="pause"
    and "net_split" in d[0]["source_line"]' \
    "断链 PAUSE 行 → interlock_action black"
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="black"' \
    "控制器终态 black（关键采集断链 fail-safe）"
V=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["action"])' \
    "$DRILL_ROOT/cmd/verify.request")
assert_eq "$V" "verify_only" "cmd/verify.request=verify_only（BLACK 只读校验请求）"
assert_exists "$DRILL_ROOT/monitor/net_split.flag" "离线标记在案（远端不可达注记）"

# ---- 注入②：恢复协议（远端恢复注记 + RESUME 行——clr_pause_if 生产格式） ----
rm -f "$DRILL_ROOT/monitor/net_split.flag"
echo "[$(TS)] ALERT RESUME: net_split 已恢复" >> "$DRILL_ROOT/monitor/alerts.log"
run_eventd
run_controller

# 断言②：RESUME 事件入流（interlock green/resume）+ 黑态不自动翻绿
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==2 and d[1]["event_type"]=="interlock_action"
    and d[1]["severity"]=="green" and d[1]["interlock"]["action"]=="resume"' \
    "RESUME 行 → interlock_action green（resume 入流可审计）"
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="black"' \
    "BLACK 终态不自动翻绿（恢复收敛属人工/restore——d13 演练该路径）"
[ ! -e "$DRILL_ROOT/monitor/net_split.flag" ] || fail "离线标记未随恢复移除"
echo "  ✓ 离线标记随恢复移除（恢复协议闭环：注记+RESUME+标记清理）"

drill_pass

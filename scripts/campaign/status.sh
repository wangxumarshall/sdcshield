#!/bin/bash
# status.sh — 战役状态一览（只读，无需 root）
DIR="${SDC_CAMPAIGN_DIR:-$HOME/sdc-campaign}"
echo "=== 服务状态 ==="
printf "%-10s %s\n" monitor:  "$(systemctl is-active sdc-monitor.service 2>&1)"
printf "%-10s %s\n" campaign: "$(systemctl is-active sdc-campaign.service 2>&1)"
echo "=== 战役进度 ==="
python3 -c "
import json, os
try: d = json.load(open('$DIR/state.json'))
except Exception as e: print('  (无 state.json:', e, ')'); raise SystemExit
print('  cycle:', d.get('cycle'), ' started:', d.get('cycle_start', '?'))
print('  phase:', d.get('phase', '?'))
print('  cmd:', str(d.get('phase_cmd', ''))[:110])
led = '$DIR/events/ledger.csv'
print('  事件数:', sum(1 for _ in open(led)) if os.path.exists(led) else 0)"
echo "=== 最新工况（monitor.csv 末行）==="
tail -1 "$DIR/monitor/monitor.csv" 2>/dev/null
echo "=== 磁盘 ==="
df -h "$DIR" 2>/dev/null | tail -1
echo "=== 最近告警 ==="
tail -3 "$DIR/monitor/alerts.log" 2>/dev/null || echo "  (无)"

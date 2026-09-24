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
echo "=== 电压/频率（CSV v2 末行解析）==="
python3 - "$DIR" <<'PYEOF' 2>/dev/null
import sys, csv
d = sys.argv[1]
rows = list(csv.reader(open(f"{d}/monitor/monitor.csv")))
if len(rows) >= 2:
    h, r = rows[0], rows[-1]
    v = dict(zip(h, r))
    def s(k): return v.get(k, "na") or "na"
    print(f"  电压V: VDDAVS={s('vddavs1')}/{s('vddavs2')} N_VDDAVS={s('nvddavs1')}/{s('nvddavs2')} "
          f"VDDFIX={s('vddfix1')}/{s('vddfix2')} HVCC={s('hvcc1')}/{s('hvcc2')}")
    print(f"        VDDQ_AB={s('vddq_ab1')}/{s('vddq_ab2')} VDDQ_CD={s('vddq_cd1')}/{s('vddq_cd2')}"
          f"  功耗={s('watts')}W")
    print(f"  频率kHz: min={s('freq_min_khz')} avg={s('freq_avg_khz')} max={s('freq_max_khz')}"
          f"（<2600000=节流）  占用: {s('cpu_util_all')}% (s0={s('cpu_util_s0')}% s1={s('cpu_util_s1')}%)")
PYEOF
echo "=== SEL 异常统计 ==="
SELN=$(cat "$DIR"/monitor/sel_events/*.txt 2>/dev/null | wc -l)
SELC=$(grep -hE 'Critical.*(Asserted|asserted)' "$DIR"/monitor/sel_events/*.txt 2>/dev/null | wc -l)
STORM=$(grep -c 'SEL 事件风暴' "$DIR/monitor/alerts.log" 2>/dev/null)
echo "  战役期新增 SEL 事件: ${SELN} 条（其中 Critical 断言: ${SELC} 条）  风暴告警: ${STORM} 次"
[ "$SELC" -gt 0 ] 2>/dev/null && echo "  ⚠ 存在 Critical 事件——检查粘性 PAUSE 与 sel_events/"
[ "$STORM" -gt 0 ] 2>/dev/null && echo "  ⚠ 存在事件风暴（典型为热节流振荡）——见 alerts.log"
echo "=== 最新 10 分钟工况快照（含偏移标记）==="
tail -16 "$DIR/monitor/condition_10m.log" 2>/dev/null || echo "  (尚无快照——首份将在监控启动后 10min 内产生)"
echo "=== 磁盘 ==="
df -h "$DIR" 2>/dev/null | tail -1
echo "=== 最近告警 ==="
tail -3 "$DIR/monitor/alerts.log" 2>/dev/null || echo "  (无)"

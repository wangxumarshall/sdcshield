#!/bin/bash
# d11_disk_full.sh — 磁盘满演练（v5 §17.2 第 11 类）。
#
# 真满盘不可做（战役+9 服务共用盘）——水位函数级测试（不真占盘）：
#   ① df 检查机制真实：monitor MON_SELFTEST 一周期（默认阈值 95%），断言
#      monitor.csv disk_pct 列 == 独立 df -P 同命令取值（真实水位采集链）；
#   ② 联锁语义注入（水位函数级）：数据根 campaign.env 注入 DISK_STOP_PCT=1
#      （阈值降至当前水位之下——同一比较代码路径 dp>=DISK_STOP → set_pause
#      disk），重跑 monitor → PAUSE 标志 + ALERT PAUSE 行（停新日志）→
#      sdc_eventd → interlock black → 控制器 BLACK + verify.request；
#   ③ 粘性/幂等：磁盘水位 PAUSE 为人工恢复（v5 §8.6 表"人工"）——再跑
#      一周期不重复告警（! paused_for disk 守卫），PAUSE 行恰 1 条。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d11_disk_full

MON_SH="$REPO/scripts/sdc-excite-reproduce/sdc_monitor.sh"
SDR_FIX="$DRILL_ROOT/monitor/sdr_fixture.txt"
cat > "$SDR_FIX" <<'EOF'
CPU1 Core Rem       | 65 degrees C      | ok
CPU2 Core Rem       | 64 degrees C      | ok
CPU1 Prochot        | 0x00              | ok
Power               | 276 Watts         | ok
EOF

run_monitor_once() {  # 良性 SDR + 隔离根（campaign.env 由调用方控制存在与否）
    env -u SDC_ROOT_PW MON_SELFTEST=1 MON_SDR_FILE="$SDR_FIX" \
        SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT" \
        bash "$MON_SH" > "$DRILL_ROOT/monitor_run_$1.out" 2>&1
}

# ---- 断言①：df 检查机制真实（默认阈值 95%——当前水位不足不联锁） ----
run_monitor_once baseline
GOT_DP=$(python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
print(rows[1][rows[0].index("disk_pct")])' "$DRILL_ROOT/monitor/monitor.csv")
REAL_DP=$(df -P "$DRILL_ROOT" | awk 'NR==2{gsub(/%/,"");print $5}')
assert_eq "$GOT_DP" "$REAL_DP" "disk_pct 列（$GOT_DP）== df -P 真实水位（$REAL_DP%）"
[ "$REAL_DP" -lt 95 ] || echo "  ⚠ 真实水位已 ≥95%——基线周期本身即应联锁（下步仍成立）"

# ---- 注入②：水位函数级联锁（DISK_STOP_PCT=1 → dp>=STOP → PAUSE 停新日志） ----
printf 'DISK_STOP_PCT=1\n' > "$DRILL_ROOT/campaign.env"   # monitor 启动时 source
run_monitor_once full
assert_exists "$DRILL_ROOT/PAUSE" "PAUSE 标志落盘（disk 水位联锁触发）"
assert_contains "$DRILL_ROOT/PAUSE" "disk" "PAUSE 内容含 disk 原因"
assert_contains "$DRILL_ROOT/monitor/alerts.log" "ALERT PAUSE: disk" \
    "alerts.log 含 PAUSE: disk（停新日志）行"

# 断言②：全链——PAUSE 行 → interlock black → 控制器 BLACK + verify 请求
run_eventd
run_controller
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'any(e["event_type"]=="interlock_action" and e["severity"]=="black"
    and e["interlock"]["action"]=="pause" for e in d)' \
    "磁盘 PAUSE 行 → interlock_action black"
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="black"' \
    "控制器终态 black（磁盘满 fail-safe 停新日志）"
V=$(python3 -c 'import json,sys;print(json.load(open(sys.argv[1]))["action"])' \
    "$DRILL_ROOT/cmd/verify.request")
assert_eq "$V" "verify_only" "cmd/verify.request=verify_only"

# ---- 断言③：粘性/幂等（人工恢复语义——重复周期不重复告警） ----
run_monitor_once sticky
NPAUSE=$(grep -c "ALERT PAUSE: disk" "$DRILL_ROOT/monitor/alerts.log" || true)
assert_eq "$NPAUSE" "1" "PAUSE 行恰 1 条（! paused_for 守卫——粘性联锁不重复告警）"
assert_exists "$DRILL_ROOT/PAUSE" "PAUSE 标志保持（人工恢复——无自愈 clr_pause）"

drill_pass

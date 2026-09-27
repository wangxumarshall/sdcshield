#!/bin/bash
# d04_ce.sh — EDAC CE（可纠正错误）演练（v5 §17.2 第 4 类）。
#
# 注入→观测链：
#   ① monitor MON_SELFTEST 一周期（良性 SDR fixture）：mc_ce_total 列 =
#      真实 /sys EDAC ce_count 全 mc 求和（CE 计数可观测——与注入无关的
#      真实采集链；无 EDAC 的板列空=缺失不伪造 0，同口径）；
#   ② journal_watch.log CE 行（rasdaemon/内核 corrected error 经 collector@ras
#      RAS 关键字流的一般形态）→ sdc_eventd → ras_keyword yellow 事件；
#   ③ 分级语义（对照 d05）：CE 不含 UE/panic 关键字 → yellow，无规则命中
#      → 控制器零决策、终态 green——CE 可观测可回溯，但绝不冒充硬件 SDC
#      升级（as-built：CE 增量入列与事件流，不自动联锁）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d04_ce

# ---- 断言①：真实采集链——monitor 一周期，mc_ce_total 列与 /sys 同源求和 ----
SDR_FIX="$DRILL_ROOT/monitor/sdr_fixture.txt"
cat > "$SDR_FIX" <<'EOF'
CPU1 Core Rem       | 65 degrees C      | ok
CPU2 Core Rem       | 64 degrees C      | ok
CPU1 Prochot        | 0x00              | ok
Power               | 276 Watts         | ok
EOF
env -u SDC_ROOT_PW MON_SELFTEST=1 MON_SDR_FILE="$SDR_FIX" \
    SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT" \
    bash "$REPO/scripts/sdc-excite-reproduce/sdc_monitor.sh" \
    > "$DRILL_ROOT/monitor_run.out" 2>&1

# 与 monitor._edac_sum 同规则的独立求和（非数字/缺失跳过；无 mc → 空）
REAL_CE=$(python3 -c '
import glob, sys
t = n = 0
for f in glob.glob("/sys/devices/system/edac/mc/mc*/ce_count"):
    try:
        v = open(f).read().strip()
    except OSError:
        continue
    if v.isdigit():
        t += int(v); n += 1
print(t if n else "")')
GOT_CE=$(python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
hdr = rows[0]
print(rows[1][hdr.index("mc_ce_total")] if len(rows) > 1 else "<无数据行>")' \
    "$DRILL_ROOT/monitor/monitor.csv")
assert_eq "$GOT_CE" "$REAL_CE" \
    "mc_ce_total 列（$GOT_CE）== /sys EDAC 真实求和（$REAL_CE）——CE 可观测"

# ---- 注入②：journal CE 行（collector RAS 关键字流一般形态） ----
echo "[$(TS)] RAS edac mc0: 5 Corrected Hardware Errors on csrow0 ch0（M5 演练 CE 差值注入）" \
    >> "$DRILL_ROOT/monitor/journal_watch.log"
run_eventd
run_controller

# 断言②：ras_keyword yellow 入流（CE 行无 UE/panic 关键字）
# （首条为 monitor 首启 INIT 离散快照——green 非断言，as-built 语义）
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==2 and d[0]["event_type"]=="discrete_transition"
    and d[0]["severity"]=="green"
    and d[1]["event_type"]=="ras_keyword" and d[1]["severity"]=="yellow"
    and "Corrected Hardware Errors" in d[1]["source_line"]' \
    "CE 行 → ras_keyword yellow 事件（可回溯；首条为 monitor 首启 INIT 良性快照）"

# 断言③：分级语义——零决策、终态 green（CE 不升级不联锁，对照 d05 UE→red）
NDEC=0
[ -f "$DRILL_ROOT/spool/controller_ledger.jsonl" ] && NDEC=$(wc -l < "$DRILL_ROOT/spool/controller_ledger.jsonl")
assert_eq "$NDEC" "0" "控制器零决策（CE=yellow 无规则命中——不冒充硬件 SDC 升级）"
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="green"' "终态 green（CE 不触发状态转换）"

drill_pass

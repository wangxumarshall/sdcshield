#!/bin/bash
# d09_collector_crash.sh — collector 崩溃演练（v5 §17.2 第 9 类）。
#
# 注入→观测链（隔离 collector 副本——绝不碰真 sdc-collector@ 服务）：
#   ① 启动隔离副本（SDC_EXCITE_REPRODUCE_DIR=隔离根 + collector@ras 真实
#      入口，systemd 模板同款命令行），等首个自监控行落盘（周期成功）；
#   ② 崩溃注入：SIGKILL 副本 → wait rc=137（死亡可见）+ 崩溃前自监控行
#      last_success_ts 非空（审计留痕——死前最后周期在案）；
#   ③ 拉起恢复（Restart=always 语义模拟）：同命令重启 → 自监控续写
#      （行数增长、collector_self.csv 列头防御不触发=列集稳定无错位）→
#      SIGTERM 优雅退出 rc=0（对照 137 强杀——守护分片睡 ≤1s 收敛）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d09_collector_crash

COLLECTOR="$REPO/tools/telemetry/sdc_collector.py"
SELF_CSV="$DRILL_ROOT/monitor/collector_self.csv"

selfmon_rows() {  # 自监控行数（文件缺失=0）
    [ -f "$SELF_CSV" ] && wc -l < "$SELF_CSV" || echo 0
}

start_collector() {  # 隔离副本（--period-s 3600：首周期立即采集后长睡）
    SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT" \
        python3 "$COLLECTOR" ras --period-s 3600 \
        > "$DRILL_ROOT/collector_$1.out" 2>&1 &
    CPID=$!
    reg_pid "$CPID"
}

wait_selfmon_row() {  # 等自监控新行（$1=起始行数；超时 30s 失败）
    local deadline=$(( $(date +%s) + 30 ))
    while [ "$(selfmon_rows)" -le "$1" ]; do
        [ "$(date +%s)" -lt "$deadline" ] || fail "collector 自监控行超时未落盘（30s）"
        sleep 0.5
    done
}

# ---- 注入①：隔离副本启动（systemd 模板 ExecStart 同款命令行） ----
N0=$(selfmon_rows)   # =0（全新根）
start_collector v1
wait_selfmon_row "$N0"
kill -0 "$CPID" 2>/dev/null || fail "collector 副本未存活"
echo "  ✓ 隔离副本启动（pid=$CPID）+ 首个自监控行落盘"
selfmon_shape_ok() {  # 末行形状：collector=ras + last_success_ts 非空 + 列数一致
    python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
r = rows[-1]
assert r[1] == "ras" and r[6] != "" and len(r) == len(rows[0]) == 8, r
' "$SELF_CSV"
}
selfmon_shape_ok || fail "自监控行形状非预期（last_success_ts 空/列数漂移）"
echo "  ✓ 自监控行形状齐（collector=ras、last_success_ts 非空——周期成功在案）"

# ---- 注入②：SIGKILL 崩溃 → 死亡可见 + 审计留痕 ----
kill -KILL "$CPID"
set +e
wait "$CPID"
CRASH_RC=$?
set -e
assert_eq "$CRASH_RC" "137" "崩溃注入 wait rc=137（SIGKILL 死亡可见）"
selfmon_shape_ok || fail "崩溃前最后周期 last_success_ts 空（审计留痕破坏）"
echo "  ✓ 崩溃前最后周期 last_success_ts 非空（审计留痕——死亡时刻可回溯）"
DRILL_PIDS=()   # 已收尸

# ---- 注入③：拉起恢复（Restart=always 语义）→ 续写 + 优雅退出 ----
N1=$(selfmon_rows)
start_collector v2
wait_selfmon_row "$N1"
echo "  ✓ 重启拉起成功（Restart=always 语义）+ 自监控续写（$N1 → $(selfmon_rows) 行）"
# 列头防御不触发（列集跨重启稳定——无错位拼接、非 exit 1 拒启）
kill -0 "$CPID" 2>/dev/null || fail "重启副本未存活（疑似列头漂移拒启）"
python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
assert len(rows) >= 2 and rows[0] == ["ts", "collector", "samples_total",
    "samples_dropped", "period_s", "loop_duration_s", "last_success_ts", "rss_kb"], rows[0]
assert len({len(r) for r in rows}) == 1, "自监控行列数漂移（错位拼接）"
' "$SELF_CSV" || fail "collector_self.csv 列头/列数漂移"
echo "  ✓ 自监控 CSV 列头跨重启稳定（无错位拼接）"
kill -TERM "$CPID"
set +e
wait "$CPID"
TERM_RC=$?
set -e
assert_eq "$TERM_RC" "0" "SIGTERM 优雅退出 rc=0（对照 137 强杀——守护分片睡 ≤1s 收敛）"
DRILL_PIDS=()

drill_pass

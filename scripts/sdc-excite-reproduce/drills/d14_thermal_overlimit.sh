#!/bin/bash
# d14_thermal_overlimit.sh — 温度越限演练（v5 §17.2 温度越限项；§8.6 热联锁）。
#
# 注入→观测链（SDR 高温行注入 MON_SDR_FILE——绝不动真实温控/真实负载）：
#   ① 单热采样 PAUSE（95°C 线）：96°C fixture → monitor MON_SELFTEST 一周期
#      → set_pause thermal（PAUSE 标志 + ALERT PAUSE 行）+ csv 高温读数入列；
#      96<100 且单采样 → **不升级 KILL**（负向断言——热升级行不在场）；
#   ② 连续 2 热采样升级 KILL（全家族唯一 kill 真实负载联锁，§8.6）：单进程
#      两周期 97°C（MON_SELFTEST 只跑一周期——改后台进程跑真循环）→
#      热升级 alert + pkill -KILL sdcshield/stress-ng；
#   ③ 88°C 快采样（SLEEP_NEXT=20）+ 95/90 滞回：KILL 在 ≤55s 内出现
#      （若 60s 慢采样第二周期必 >60s）；第三周期 89°C ≤ RESUME_C(90) →
#      RESUME 行 + PAUSE 标志移除（滞回恢复协议）。
#
# 安全边界（as-built 关键裁定：本机演练与真实战役同一 OS 用户 sdc 运行——
# monitor 的 pgrep/pkill 按进程名扫全机，真实执行会命中真实战役负载）：
#   - **进程指派层隔离**（d08 假 ipmitool 同模式）：PATH 前置演练假 pgrep/
#     pkill——只认演练注册的牺牲进程（victim.pid 内显式 pid + comm 匹配），
#     绝不扫描全机进程表；热联锁判定（阈值/热计数/alert 文案/分派时序）为
#     monitor 真实代码路径；
#   - 牺牲进程：/bin/sleep 副本命名 sdcshield（comm=sdcshield，演练自有）；
#   - 双保险断言：演练前后全机 sdcshield/stress-ng pid 集不变（真实战役
#     零接触的实证）；SEL 走假 ipmitool（良性行，零真 BMC 接触）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d14_thermal_overlimit

MON_SH="$REPO/scripts/sdc-excite-reproduce/sdc_monitor.sh"
FIX="$DRILL_ROOT/monitor/sdr_fixture.txt"
FAKEBIN="$DRILL_ROOT/fakebin"

write_fixture() {  # write_fixture <cpu1°C> [cpu2°C]：高温 SDR 注入（其余全良性——
                    # 含 FAN2/3：风扇缺失 3 周期会触发 fan PAUSE，污染滞回断言）
    cat > "$FIX" <<EOF
CPU1 Core Rem       | $1 degrees C      | ok
CPU2 Core Rem       | ${2:-64} degrees C      | ok
CPU1 Prochot        | 0x00              | ok
Power               | 276 Watts         | ok
FAN2 Speed          | 8000 RPM          | ok
FAN3 Speed          | 7900 RPM          | ok
EOF
}

# ---- 隔离工具层：假 pgrep/pkill（只认牺牲 pid）+ 假 ipmitool（SEL 良性行） ----
mkdir -p "$FAKEBIN"
cat > "$FAKEBIN/pgrep" <<'EOF'
#!/bin/bash
# d14 假 pgrep——只认 victim.pid 内活着的牺牲进程（comm 匹配），绝不扫全机
VF="$(dirname "$0")/victim.pid"
[ -f "$VF" ] || exit 1
name=""; prev=""
for a in "$@"; do [ "$prev" = "-x" ] && name="$a"; prev="$a"; done
[ -n "$name" ] || exit 1
while read -r p; do
    [ -n "$p" ] && [ "$(cat "/proc/$p/comm" 2>/dev/null)" = "$name" ] && exit 0
done < "$VF"
exit 1
EOF
cat > "$FAKEBIN/pkill" <<'EOF'
#!/bin/bash
# d14 假 pkill——只 kill victim.pid 内注册的牺牲进程（显式 pid，绝不按名扫全机）
VF="$(dirname "$0")/victim.pid"
[ -f "$VF" ] || exit 1
name=""; prev=""
for a in "$@"; do [ "$prev" = "-x" ] && name="$a"; prev="$a"; done
[ -n "$name" ] || exit 1
n=0
while read -r p; do
    if [ -n "$p" ] && [ "$(cat "/proc/$p/comm" 2>/dev/null)" = "$name" ]; then
        kill -KILL "$p" 2>/dev/null && n=$((n+1))
    fi
done < "$VF"
[ "$n" -gt 0 ]
EOF
cat > "$FAKEBIN/ipmitool" <<'EOF'
#!/bin/bash
# d14 假 ipmitool——SEL 段返良性行（非 selftest 下 sel_tick=1 周期会真调，零真 BMC）
case "$*" in
    *sel*) echo "   1 | Pre-Init |0x0000| Asserted | OK"; exit 0 ;;
    *) exit 1 ;;
esac
EOF
chmod +x "$FAKEBIN/pgrep" "$FAKEBIN/pkill" "$FAKEBIN/ipmitool"
# 牺牲进程：/bin/sleep 副本命名 sdcshield（comm=sdcshield——假 pgrep/pkill 唯一目标）
cp /bin/sleep "$FAKEBIN/sdcshield"
"$FAKEBIN/sdcshield" 600 &
VICTIM=$!
reg_pid "$VICTIM"
echo "$VICTIM" > "$FAKEBIN/victim.pid"
sleep 0.3
kill -0 "$VICTIM" 2>/dev/null || fail "牺牲进程未启动"
echo "  ✓ 牺牲进程启动（pid=$VICTIM，comm=sdcshield；假 pgrep/pkill 只认此 pid）"

snapshot_load_pids() {  # 全机 sdcshield/stress-ng pid 集（排除演练牺牲进程）
    ps -eo pid=,comm= | awk -v v="$VICTIM" \
        '$1!=v && ($2=="sdcshield"||$2=="stress-ng"){print $1}' | sort -n
}
pre_pids=$(snapshot_load_pids)

# ==== ① 单热采样：96°C → PAUSE thermal，不升级 KILL ====
write_fixture 96
env -u SDC_ROOT_PW MON_SELFTEST=1 MON_SDR_FILE="$FIX" \
    SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT" PATH="$FAKEBIN:$PATH" \
    bash "$MON_SH" > "$DRILL_ROOT/monitor_A.out" 2>&1
assert_contains "$DRILL_ROOT/monitor/alerts.log" "ALERT PAUSE: CPU 96C >= 95C" \
    "96°C ≥95 → set_pause thermal（PAUSE 行在场；原因键 thermal 记于 PAUSE 标志）"
assert_exists "$DRILL_ROOT/PAUSE" "PAUSE 标志落盘"
assert_contains "$DRILL_ROOT/PAUSE" "thermal" "PAUSE 内容含 thermal 原因"
GOT_T=$(python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
print(rows[1][rows[0].index("cpu1_core_rem")])' "$DRILL_ROOT/monitor/monitor.csv")
assert_eq "$GOT_T" "96" "csv cpu1_core_rem 列=96（SDR 高温注入真实入列）"
assert_not_contains "$DRILL_ROOT/monitor/alerts.log" "热升级" \
    "96°C<100 且单热采样 → 不升级 KILL（负向：热升级行不在场）"

# ==== ② 连续 2 热采样 → 热升级 KILL（牺牲进程承接 pkill） ====
# 后台真循环（无 MON_SELFTEST——单进程跨周期才有热计数/滞回状态）：
# 周期1 97°C（hot=1，PAUSE 已在——去重守卫）→ 20s → 周期2 97°C（hot=2 → KILL）
write_fixture 97
env -u SDC_ROOT_PW MON_SDR_FILE="$FIX" SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT" \
    PATH="$FAKEBIN:$PATH" \
    bash "$MON_SH" > "$DRILL_ROOT/monitor_BC.out" 2>&1 &
MONPID=$!
reg_pid "$MONPID"

wait_line() {  # wait_line <模式> <超时秒>：轮询 alerts.log 至模式出现
    local pat="$1" deadline=$(( $(date +%s) + $2 ))
    while ! grep -q -- "$pat" "$DRILL_ROOT/monitor/alerts.log" 2>/dev/null; do
        kill -0 "$MONPID" 2>/dev/null || fail "monitor 提前退出（未见 '$pat'）: $(tail -3 "$DRILL_ROOT/monitor_BC.out")"
        [ "$(date +%s)" -lt "$deadline" ] || fail "超时 ${2}s 未见 '$pat'（联锁未触发？）"
        sleep 0.5
    done
}

T0=$(date +%s)
wait_line "热升级" 55           # 连续 2 热采样（20s 快采样 → ~25s 到场）
TKILL=$(( $(date +%s) - T0 ))
assert_contains "$DRILL_ROOT/monitor/alerts.log" \
    "ALERT 热升级：97C（连续2个热采样）→ KILL 全部负载" \
    "连续 2 热采样 97°C → 热升级 KILL alert 在场（真实联锁代码路径）"
[ "$TKILL" -le 55 ] || fail "KILL 历时 ${TKILL}s > 55s——88°C 快采样（SLEEP_NEXT=20）路径未生效"
echo "  ✓ KILL 在 ${TKILL}s 内到场（88°C 快采样 SLEEP_NEXT=20 路径——60s 慢采样必 >60s）"
set +e
wait "$VICTIM"
VRC=$?
set -e
assert_eq "$VRC" "137" "牺牲 sdcshield 被 pkill -KILL（rc=137——KILL 联锁分派真实生效）"
post_pids=$(snapshot_load_pids)
# 真实战役零接触断言：演练前在案的真实 pid 一个不少（战役自身更替可新增，
# 但绝不允许有真实 sdcshield/stress-ng 因演练消失）
for p in $pre_pids; do
    echo "$post_pids" | grep -qx "$p" \
        || fail "真实负载 pid=$p 在演练中消失——零接触破坏（pkill 越界？）"
done
echo "  ✓ 演练前在案的真实 sdcshield/stress-ng（$(echo $pre_pids | tr '\n' ' ')）全部存活（零接触实证）"

# ==== ③ 95/90 滞回：89°C ≤ RESUME_C → RESUME + PAUSE 移除 ====
write_fixture 89
wait_line "ALERT RESUME: thermal" 45
assert_contains "$DRILL_ROOT/monitor/alerts.log" "ALERT RESUME: thermal 已恢复" \
    "89°C ≤ 90 滞回线 → RESUME 行在场"
[ ! -e "$DRILL_ROOT/PAUSE" ] || fail "RESUME 后 PAUSE 标志未移除"
echo "  ✓ RESUME 后 PAUSE 标志移除（95/90 滞回恢复协议闭环）"

kill -TERM "$MONPID" 2>/dev/null || :
set +e
wait "$MONPID"
MRC=$?
set -e
DRILL_PIDS=()   # monitor/牺牲进程均已收尸
echo "  ✓ 后台 monitor 优雅停止（rc=$MRC）"

drill_pass

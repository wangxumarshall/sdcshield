#!/bin/bash
# sdc_monitor.sh — SDC 7×24 战役监控（以 root 运行：sdc-monitor.service）
#
# 职责（plan §6/§8）:
#   每 60s: IPMI + OS 采样入 CSV 时序（事件回溯依据）
#   安全联锁: 温度(滞回)/风扇/内存/磁盘水位 → PAUSE 标志；SEL Critical → 粘性 PAUSE
#   每 5min: SEL 增量检查
#   cmd/ 命令代理: governor.request（L5 di/dt 阶跃）、snapshot.request（事件 root 取证）
# 与战役驱动互不阻断（独立 systemd 单元，重启各自独立）。
set -u
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/sdc_common.sh"

# 目录权限模型：CAMPAIGN_DIR 树归运行用户（sdc）所有（install.sh/驱动创建）。
# 本脚本以 root 运行，只补建监控自身目录，并把属主还给 CAMPAIGN_DIR 的属主，
# 避免抢建驱动目录（logs/events/stressng）导致 sdc 侧不可写。
# alerts.log 为双进程共写（root 监控 + sdc 驱动），必须 666。
mkdir -p "$CAMPAIGN_DIR" "$MON_DIR" "$CMD_DIR"
CSV="$MON_DIR/monitor.csv"
SNAP="$MON_DIR/snapshots"
SEL_EV="$MON_DIR/sel_events"
mkdir -p "$SNAP" "$SEL_EV"
DIR_OWNER=$(stat -c %U "$CAMPAIGN_DIR" 2>/dev/null || echo root)
DIR_GROUP=$(stat -c %G "$CAMPAIGN_DIR" 2>/dev/null || echo root)
if [ "$DIR_OWNER" != root ]; then
    chown -R "${DIR_OWNER}:${DIR_GROUP}" "$CAMPAIGN_DIR" 2>/dev/null
fi
touch "$MON_DIR/alerts.log" 2>/dev/null
chmod 666 "$MON_DIR/alerts.log" 2>/dev/null

# campaign.env 由 sdc_campaign.sh 首次运行生成；监控可先于驱动启动（用默认阈值）
[ -f "$CAMPAIGN_DIR/campaign.env" ] && source "$CAMPAIGN_DIR/campaign.env"
PAUSE_C="${THERMAL_PAUSE_C:-95}"
RESUME_C="${THERMAL_RESUME_C:-90}"
DISK_WARN="${DISK_WARN_PCT:-85}"
DISK_STOP="${DISK_STOP_PCT:-95}"

[ -f "$CSV" ] || echo "ts,cpu1_c,cpu2_c,mem1_c,mem2_c,outlet_c,inlet_c,watts,vddavs1,vddavs2,fan2_rpm,fan3_rpm,prochot1,prochot2,tz0_c,tz1_c,memavail_kb,load1,disk_pct,bmc_ok" > "$CSV"

# ipmitool 输出有两种格式（实测）：'sdr list' 3 字段（name|reading|status）、
# 'sdr type X' 5 字段（name|id|status|x|reading）。取 $5，为空则退 $2，两格式通吃。
sdr_raw() { awk -F'|' -v s="$1" '$1 ~ s {v=$5; if (v ~ /^[ ]*$/) v=$2; gsub(/^ +| +$/,"",v); print v}' <<<"$SDR" | head -1; }
sdr_val() { sdr_raw "$1" | grep -oE '[0-9]+(\.[0-9]+)?' | head -1; }
last_sel_id=""
sel_tick=0
bmc_fail=0
thermal_paused=0
thermal_hot=0
fan_bad=0

paused_for() { [ -f "$PAUSE_FLAG" ] && grep -q "$1" "$PAUSE_FLAG" 2>/dev/null; }
set_pause() { echo "$1: $2 ($(date '+%F %T'))" > "$PAUSE_FLAG"; alert "PAUSE: $2"; }
clr_pause_if() { if paused_for "$1"; then rm -f "$PAUSE_FLAG"; alert "RESUME: $1 已恢复"; fi; }

while :; do
    ts=$(date '+%F %T')
    SDR=$(timeout 20 ipmitool sdr list 2>/dev/null)
    bmc=1; [ -z "$SDR" ] && bmc=0
    if [ "$bmc" = 0 ]; then
        bmc_fail=$((bmc_fail+1))
        [ $bmc_fail -eq 10 ] && alert "BMC 通道连续失败 10 次，降级 OS 侧采样（acpitz/meminfo/df 不断链）"
    else
        bmc_fail=0
    fi

    c1=$(sdr_val 'CPU1 Core Rem');  c2=$(sdr_val 'CPU2 Core Rem')
    m1=$(sdr_val 'CPU1 MEM Temp');  m2=$(sdr_val 'CPU2 MEM Temp')
    ot=$(sdr_val 'Outlet Temp');    in_=$(sdr_val 'Inlet Temp')
    pw=$(sdr_val '^Power ');        v1=$(sdr_val 'CPU1 VDDAVS'); v2=$(sdr_val 'CPU2 VDDAVS')
    f2=$(sdr_val 'FAN2 Speed');     f3=$(sdr_val 'FAN3 Speed')
    p1=$(sdr_raw 'CPU1 Prochot')
    p2=$(sdr_raw 'CPU2 Prochot')
    tz0=$(awk '{printf "%.1f", $1/1000}' /sys/class/thermal/thermal_zone0/temp 2>/dev/null)
    tz1=$(awk '{printf "%.1f", $1/1000}' /sys/class/thermal/thermal_zone1/temp 2>/dev/null)
    ma=$(awk '/MemAvailable/{print $2}' /proc/meminfo)
    l1=$(awk '{print $1}' /proc/loadavg)
    dp=$(df -P "$CAMPAIGN_DIR" | awk 'NR==2{gsub(/%/,"");print $5}')
    echo "$ts,${c1:-},${c2:-},${m1:-},${m2:-},${ot:-},${in_:-},${pw:-},${v1:-},${v2:-},${f2:-},${f3:-},${p1:-},${p2:-},${tz0:-},${tz1:-},${ma:-},${l1:-},${dp:-},$bmc" >> "$CSV"

    # ---- 温度联锁（双 socket 取 max，滞回 95→90；PAUSE 对长调用无牙 → 5 分钟升级 KILL）----
    maxt=0
    for t in "$c1" "$c2"; do [ -n "$t" ] && [ "$t" -gt "$maxt" ] 2>/dev/null && maxt=$t; done
    if [ "$maxt" -ge "$PAUSE_C" ] 2>/dev/null; then
        if ! paused_for thermal; then set_pause thermal "CPU ${maxt}C >= ${PAUSE_C}C"; fi
        thermal_paused=1
        # 升级：thermal PAUSE 连续 5 个采样（≥5 分钟）仍有负载在跑 → root 直接杀
        # （驱动 check_pause 只在调用间生效；进程内 --temperature-threshold 本板 no-op。
        #   TERM 对 sdcshield 无效已实测，直接 KILL；部分 YAML 无 fail/crash 行 →
        #   驱动判别为阶段边界终止非事件，下一调用自然进入 PAUSE 等待）
        thermal_hot=$((thermal_hot + 1))
        if [ "$thermal_hot" -ge 5 ] && pgrep -x sdcshield >/dev/null 2>&1; then
            alert "热升级：PAUSE ${thermal_hot} 分钟未退温，KILL 负载进程（$(pgrep -x sdcshield | tr '\n' ' ')）"
            pkill -KILL -x sdcshield
            thermal_hot=0
        fi
    elif [ "$thermal_paused" = 1 ] && [ "$maxt" -le "$RESUME_C" ] 2>/dev/null; then
        clr_pause_if thermal; thermal_paused=0; thermal_hot=0
    else
        thermal_hot=0
    fi

    # ---- 风扇联锁（FAN2/3 任一异常；连续 3 个采样异常才暂停，恢复即放行）----
    if [ "$bmc" = 1 ]; then
        if [ -z "$f2" ] || [ -z "$f3" ] || [ "$f2" = 0 ] || [ "$f3" = 0 ]; then
            if [ "$fan_bad" -lt 3 ]; then
                fan_bad=$((fan_bad+1))
                if [ "$fan_bad" = 3 ] && ! paused_for fan; then set_pause fan "FAN2=${f2:-na} FAN3=${f3:-na}"; fi
            fi
        else
            fan_bad=0; clr_pause_if fan
        fi
    fi

    # ---- 内存联锁（防 OOM；与背景服务共存保护）----
    if [ -n "$ma" ] && [ "$ma" -lt 2097152 ] 2>/dev/null; then
        ! paused_for mem && set_pause mem "MemAvailable ${ma}kB < 2GB"
    elif [ -n "$ma" ] && [ "$ma" -gt 4194304 ] 2>/dev/null; then
        clr_pause_if mem
    fi

    # ---- 磁盘水位 ----
    if [ -n "$dp" ] && [ "$dp" -ge "$DISK_STOP" ] 2>/dev/null; then
        ! paused_for disk && set_pause disk "disk ${dp}% >= ${DISK_STOP}%（停新日志）"
    elif [ -n "$dp" ] && [ "$dp" -ge "$DISK_WARN" ] 2>/dev/null; then
        [ $(( $(date +%s) % 3600 )) -lt 60 ] && alert "磁盘水位 ${dp}%（告警线 ${DISK_WARN}%）"
    fi

    # ---- SEL 增量（每 5min；Critical Asserted = 粘性暂停，需人工 rm PAUSE）----
    sel_tick=$((sel_tick+1))
    if [ $((sel_tick % 5)) -eq 1 ]; then
        selout=$(timeout 20 ipmitool sel list 2>/dev/null)
        if [ -n "$selout" ]; then
            lastid=$(tail -1 <<<"$selout" | awk -F'|' '{gsub(/ /,"",$1)}')
            if [ -n "$last_sel_id" ] && [ "$lastid" != "$last_sel_id" ]; then
                evf="$SEL_EV/$(date +%Y%m%d-%H%M%S).txt"
                awk -F'|' -v a="$last_sel_id" '{gsub(/ /,"",$1); if (strtonum("0x"$1) > strtonum("0x"a)) print}' <<<"$selout" > "$evf"
                if grep -qE 'Critical.*(Asserted|asserted)' "$evf"; then
                    ! paused_for sel && set_pause sel "SEL Critical 事件（粘性：需人工调查后 rm PAUSE）: $(head -1 "$evf")"
                fi
            fi
            last_sel_id=$lastid
        fi
    fi

    # ---- 命令代理 1: governor 阶跃（L5 di/dt；驱动写 request，root 应用）----
    if [ -f "$CMD_DIR/governor.request" ]; then
        g=$(cat "$CMD_DIR/governor.request")
        for c in /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor; do
            echo "$g" > "$c" 2>/dev/null
        done
        mv "$CMD_DIR/governor.request" "$CMD_DIR/governor.done.$(date +%s)"
    fi

    # ---- 命令代理 2: root 取证快照（事件流水线；dmesg/SEL/SDR/DCMI）----
    if [ -f "$CMD_DIR/snapshot.request" ]; then
        sd="$SNAP/$(date +%Y%m%d-%H%M%S)"
        mkdir -p "$sd"; chmod 755 "$SNAP" "$sd"
        timeout 30 ipmitool sdr list > "$sd/sdr.txt" 2>&1
        timeout 30 ipmitool sel list > "$sd/sel.txt" 2>&1
        timeout 30 ipmitool dcmi power reading > "$sd/dcmi.txt" 2>&1
        dmesg > "$sd/dmesg.txt" 2>&1
        mv "$CMD_DIR/snapshot.request" "$CMD_DIR/snapshot.done.$(date +%s)"
    fi

    sleep 60
done

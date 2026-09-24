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

# CSV v2 表头（用户指令 2026-09-24：补 CPU 占用率/频率/全电压轨/SEL 异常计数）
CSV_HDR="ts,cpu1_c,cpu2_c,mem1_c,mem2_c,outlet_c,inlet_c,watts,vddavs1,vddavs2,nvddavs1,nvddavs2,vddfix1,vddfix2,hvcc1,hvcc2,vddq_ab1,vddq_cd1,vddq_ab2,vddq_cd2,fan2_rpm,fan3_rpm,prochot1,prochot2,tz0_c,tz1_c,cpu_util_all,cpu_util_s0,cpu_util_s1,freq_min_khz,freq_avg_khz,freq_max_khz,memavail_kb,swap_used_kb,load1,disk_pct,sel5m,bmc_ok"
if [ -f "$CSV" ] && [ "$(head -1 "$CSV")" != "$CSV_HDR" ]; then
    mv "$CSV" "$MON_DIR/monitor_v1_$(date +%Y%m%d_%H%M%S).csv"   # 旧数据归档保留
    gzip -q "$MON_DIR"/monitor_v1_*.csv 2>/dev/null &
fi
[ -f "$CSV" ] || echo "$CSV_HDR" > "$CSV"

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
SLEEP_NEXT=60
SEL5M_CARRY=0
PREVSTAT=""

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
    # ---- 电压全轨（每 socket 6 路关键供电）----
    nv1=$(sdr_val 'CPU1 N_VDDAVS'); nv2=$(sdr_val 'CPU2 N_VDDAVS')
    fx1=$(sdr_val 'CPU1 VDDFIX');   fx2=$(sdr_val 'CPU2 VDDFIX')
    h1=$(sdr_val 'CPU1 HVCC');      h2=$(sdr_val 'CPU2 HVCC')
    qa1=$(sdr_val 'CPU1 VDDQ_AB');  qc1=$(sdr_val 'CPU1 VDDQ_CD')
    qa2=$(sdr_val 'CPU2 VDDQ_AB');  qc2=$(sdr_val 'CPU2 VDDQ_CD')
    # ---- CPU 占用率（/proc/stat tick 增量，总 + 分 socket；socket0=cpu0-63）----
    NEWSTAT=$(awk '/^cpu[0-9]/{n=substr($1,4)+0; idle=$5+$6; tot=0; for(i=2;i<=8;i++) tot+=$i; s=(n<64)?0:1; T[s]+=tot; I[s]+=idle} END{print T[0],I[0],T[1],I[1]}' /proc/stat 2>/dev/null)
    ua=""; us0=""; us1=""
    if [ -n "$NEWSTAT" ] && [ -n "${PREVSTAT:-}" ]; then
        read -r PT0 PI0 PT1 PI1 <<< "$PREVSTAT"
        read -r NT0 NI0 NT1 NI1 <<< "$NEWSTAT"
        # busy_delta = (T-I) 的增量；util = busy_delta/total_delta（tick 归一，与采样间隔无关）
        ua=$(awk -v nt0=$NT0 -v ni0=$NI0 -v pt0=$PT0 -v pi0=$PI0 -v nt1=$NT1 -v ni1=$NI1 -v pt1=$PT1 -v pi1=$PI1 \
            'BEGIN{b0=(nt0-ni0)-(pt0-pi0); d0=nt0-pt0; b1=(nt1-ni1)-(pt1-pi1); d1=nt1-pt1;
                  if(d0+d1>0) printf "%.1f", 100*(b0+b1)/(d0+d1); else print ""}')
        us0=$(awk -v nt0=$NT0 -v ni0=$NI0 -v pt0=$PT0 -v pi0=$PI0 'BEGIN{b=(nt0-ni0)-(pt0-pi0); d=nt0-pt0; if(d>0) printf "%.1f", 100*b/d; else print ""}')
        us1=$(awk -v nt1=$NT1 -v ni1=$NI1 -v pt1=$PT1 -v pi1=$PI1 'BEGIN{b=(nt1-ni1)-(pt1-pi1); d=nt1-pt1; if(d>0) printf "%.1f", 100*b/d; else print ""}')
    fi
    PREVSTAT=$NEWSTAT
    # ---- CPU 频率（128 核 min/avg/max kHz——固件热节流会在此显形）----
    FREQ=$(awk 'NR==FNR{next}{f+=$1; if(min==0||$1<min)min=$1; if($1>max)max=$1; n++} END{if(n>0) printf "%d,%d,%d", min, f/n, max; else print ",,"}' /dev/null /sys/devices/system/cpu/cpu*/cpufreq/scaling_cur_freq 2>/dev/null)
    fmin=$(cut -d, -f1 <<<"$FREQ"); favg=$(cut -d, -f2 <<<"$FREQ"); fmax=$(cut -d, -f3 <<<"$FREQ")
    ma=$(awk '/MemAvailable/{print $2}' /proc/meminfo)
    sw=$(awk '/^SwapTotal/{t=$2}/^SwapFree/{f=$2}END{print t-f}' /proc/meminfo)
    l1=$(awk '{print $1}' /proc/loadavg)
    dp=$(df -P "$CAMPAIGN_DIR" | awk 'NR==2{gsub(/%/,"");print $5}')
    echo "$ts,${c1:-},${c2:-},${m1:-},${m2:-},${ot:-},${in_:-},${pw:-},${v1:-},${v2:-},${nv1:-},${nv2:-},${fx1:-},${fx2:-},${h1:-},${h2:-},${qa1:-},${qc1:-},${qa2:-},${qc2:-},${f2:-},${f3:-},${p1:-},${p2:-},${tz0:-},${tz1:-},${ua:-},${us0:-},${us1:-},${fmin:-},${favg:-},${fmax:-},${ma:-},${sw:-},${l1:-},${dp:-},${SEL5M_CARRY:-},$bmc" >> "$CSV"

    # ---- 温度联锁（2026-09-24 08:21 实战事件后收紧：
    #      60s 采样 + 5 分钟升级曾让峰值持续 5 分钟 @105-106C（=Tjmax+1）。
    #      新规则：maxt>=88 时 20s 快采样；PAUSE 95 保持；KILL 条件 =
    #      绝对线 >=100 立即杀 或 连续 2 个热采样；连带杀 stress-ng。
    #      Prochot 传感器本板全程 0x00 无用（节流只见于 SEL Processor State 事件））----
    maxt=0
    for t in "$c1" "$c2"; do [ -n "$t" ] && [ "$t" -gt "$maxt" ] 2>/dev/null && maxt=$t; done
    if [ "$maxt" -ge 88 ] 2>/dev/null; then SLEEP_NEXT=20; else SLEEP_NEXT=60; fi
    if [ "$maxt" -ge "$PAUSE_C" ] 2>/dev/null; then
        if ! paused_for thermal; then set_pause thermal "CPU ${maxt}C >= ${PAUSE_C}C"; fi
        thermal_paused=1
        thermal_hot=$((thermal_hot + 1))
        if [ "$maxt" -ge 100 ] 2>/dev/null || [ "$thermal_hot" -ge 2 ]; then
            if pgrep -x sdcshield >/dev/null 2>&1 || pgrep -x stress-ng >/dev/null 2>&1; then
                alert "热升级：${maxt}C（连续${thermal_hot}个热采样）→ KILL 全部负载（绝对线100C/连续2采样）"
                pkill -KILL -x sdcshield 2>/dev/null
                pkill -KILL -x stress-ng 2>/dev/null
                thermal_hot=0
            fi
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

    # ---- SEL 增量（每 5min；事件率风暴告警 + Critical Asserted 粘性暂停）----
    sel_tick=$((sel_tick+1))
    if [ $((sel_tick % 5)) -eq 1 ]; then
        selout=$(timeout 20 ipmitool sel list 2>/dev/null)
        if [ -n "$selout" ]; then
            lastid=$(tail -1 <<<"$selout" | awk -F'|' '{gsub(/ /,"",$1)}')
            if [ -n "$last_sel_id" ] && [ "$lastid" != "$last_sel_id" ]; then
                evf="$SEL_EV/$(date +%Y%m%d-%H%M%S).txt"
                awk -F'|' -v a="$last_sel_id" '{gsub(/ /,"",$1); if (strtonum("0x"$1) > strtonum("0x"a)) print}' <<<"$selout" > "$evf"
                nnew=$(grep -c . "$evf" 2>/dev/null || echo 0)
                SEL5M_CARRY=$nnew
                # 事件率异常（如固件节流振荡 5 分钟 60+ 条）——告警不暂停（温度联锁管根因）
                [ "$nnew" -ge 20 ] && alert "SEL 事件风暴：5 分钟新增 ${nnew} 条（$(awk -F'|' '{print $3}' "$evf" | sort | uniq -c | sort -rn | head -2 | tr '\n' ' ')）——典型为热节流振荡，见 $evf"
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

    sleep "$SLEEP_NEXT"
done

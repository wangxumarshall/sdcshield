#!/bin/bash
# sdc_common.sh — SDC 7×24 战役共享助手。被 sdc-excite-reproduce.sh 与 sdc_monitor.sh source。
# 所有路径可由 env 覆盖（SDC_BIN / SDC_EXCITE_REPRODUCE_DIR，兼容回退旧名 SDC_CAMPAIGN_DIR），新单板零修改。
# 依据 docs/superpowers/plans/2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-plan.md

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
BIN="${SDC_BIN:-$REPO_DIR/builddir/sdcshield}"
EXCITE_REPRODUCE_DIR="${SDC_EXCITE_REPRODUCE_DIR:-${SDC_CAMPAIGN_DIR:-$HOME/sdc-excite-reproduce}}"
STATE_FILE="$EXCITE_REPRODUCE_DIR/state.json"
PAUSE_FLAG="$EXCITE_REPRODUCE_DIR/PAUSE"
CMD_DIR="$EXCITE_REPRODUCE_DIR/cmd"
LOG_ROOT="$EXCITE_REPRODUCE_DIR/logs"
MON_DIR="$EXCITE_REPRODUCE_DIR/monitor"
EVENTS_DIR="$EXCITE_REPRODUCE_DIR/events"
STRESSNG_DIR="$EXCITE_REPRODUCE_DIR/stressng"

ensure_dirs() {
    mkdir -p "$LOG_ROOT" "$MON_DIR" "$EVENTS_DIR" "$CMD_DIR" "$STRESSNG_DIR" \
             "$EXCITE_REPRODUCE_DIR/inventory"
}

log() {
    echo "[$(date '+%F %T')] $*" >> "$EXCITE_REPRODUCE_DIR/driver.log"
    echo "[driver] $*"
}

alert() {
    echo "[$(date '+%F %T')] ALERT $*" >> "$MON_DIR/alerts.log"
    echo "[ALERT] $*" >&2
}

state_get() { # state_get <key> → stdout
    python3 -c "
import json,sys
try: d=json.load(open(sys.argv[2]))
except Exception: sys.exit(0)
v=d.get(sys.argv[1],'')
print(v if not isinstance(v,(dict,list)) else json.dumps(v))" "$1" "$STATE_FILE" 2>/dev/null
}

state_set() { # state_set <key> <value>
    python3 -c "
import json,sys
p=sys.argv[3]
try: d=json.load(open(p))
except Exception: d={}
d[sys.argv[1]]=sys.argv[2]
json.dump(d,open(p,'w'),indent=1)" "$1" "$2" "$STATE_FILE"
}

check_pause() { # 驱动侧：联锁暂停等待（PAUSE 内容为原因文本）
    while [ -f "$PAUSE_FLAG" ]; do
        echo "[$(date '+%F %T')] PAUSED: $(cat "$PAUSE_FLAG" 2>/dev/null)"
        sleep 30
    done
}

mem_avail_kb() { awk '/MemAvailable/{print $2}' /proc/meminfo; }

max_mdim_for_threads() { # $1=线程数 → 全核 GEMM mdim 上限
    # scratch ≈ 3*N^2*8B*threads（dgemm 双精度三矩阵），预算 ≤ MemAvailable/2
    python3 -c "
import sys, math
avail = int(open('/proc/meminfo').read().split('MemAvailable:')[1].split()[0]) * 1024
t = int(sys.argv[1]); n = int(math.sqrt(avail * 0.5 / (3 * 8 * t)))
p = 16
while p * 2 <= n: p *= 2
print(min(p, 4096))" "$1"
}

# ---------------- monitor v3: SDR 发现式解析（v5 §6.4 归一化 + 单元 4）----------------
# 输入: stdin = 一次 ipmitool sdr list 全量输出（两种实测格式通吃: 3 字段 name|reading|status
#       / 5 字段 name|id|status|x|reading——取 $5 空则退 $2，与 sdc_monitor.sh 既有 sdr_raw 同口径）
# 分类三态（2026-09-26 评审根修：真机读数带单位——实证 inventory 03_ipmitool_sdr_list.txt
#   "35 degrees C"/"276 Watts"/"0.88 Volts"，旧"纯数值=模拟量"判定使真机模拟量恒 0 列、
#   离散图混入带单位读数与 no reading）：
#   读数 0x 开头 → discrete（十六进制状态码）
#   非 0x 且含数字 → analog（提取首个数值："276 Watts"→276、"20.52 Amps"→20.52）
#   no reading/Not Readable/空/纯文字无数值 → skip（两边都不入，缺失不伪造）
#   两函数消费同一 _sdr_classify 三态输出，分类互补不重不漏。
_sdr_norm() { tr -c 'a-zA-Z0-9\n' '_' <<<"$1" | tr 'A-Z' 'a-z' | sed 's/_*$//;s/^_*//'; }
_sdr_classify() {    # stdin: sdr list 全量 → stdout: 每行 规范化名|原始读数|analog|discrete|skip
    awk -F'|' 'NF>=3 {
        v=$5; if (v ~ /^[ ]*$/) v=$2; gsub(/^ +| +$/,"",v); gsub(/^ +| +$/,"",$1)
        cl = "skip"
        if (v ~ /^0x/) cl = "discrete"
        else if (v ~ /[0-9]/) cl = "analog"
        if ($1 == "") cl = "skip"
        print $1 "|" v "|" cl
    }' | while IFS='|' read -r name val cl; do
        printf "%s|%s|%s\n" "$(_sdr_norm "$name")" "$val" "$cl"
    done
}
sdr_analog_columns() {   # stdin: sdr list 全量 → stdout: 每行 规范化名|提取数值（仅模拟量）
    _sdr_classify | awk -F'|' '$3=="analog" && match($2, /[0-9]+(\.[0-9]+)?/) { print $1 "|" substr($2, RSTART, RLENGTH) }'
}
sdr_discrete_map() {     # stdin: sdr list 全量 → stdout: 每行 规范化名|原始读数（仅离散量）
    _sdr_classify | awk -F'|' '$3=="discrete" { print $1 "|" $2 }'
}
# discrete_diff <旧map文件> <新map文件>: 输出变化行（旧无=INIT），退出码=变化数（0=无变化）
discrete_diff() {
    local old="$1" new="$2" rc=0 line name val
    [ -f "$old" ] || touch "$old"
    while IFS='|' read -r name val; do
        [ -z "$name" ] && continue
        [ -z "$val" ] && continue   # 空读数传感器不参与 diff（防"每周期 INIT"误报，T1 评审 2b）
        prev=$(grep -m1 "^${name}|" "$old" | cut -d'|' -f2-)
        if [ -z "$prev" ]; then echo "${name}: INIT → ${val}"; rc=$((rc+1))
        elif [ "$prev" != "$val" ]; then echo "${name}: ${prev} → ${val}"; rc=$((rc+1)); fi
    done < "$new"
    return $rc
}

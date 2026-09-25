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

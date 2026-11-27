#!/usr/bin/env bash
# memcpy2_ab_forever.sh — memcpy2_neon 常驻执行器（可停止、可分类、失败留证）
#
# 用法:
#   scripts/memcpy2_ab_forever.sh run   [轮数]   # 前台循环，0=无限
#   scripts/memcpy2_ab_forever.sh start [轮数]   # 后台（setsid），pid 落盘
#   scripts/memcpy2_ab_forever.sh status
#   scripts/memcpy2_ab_forever.sh stop
#
# 环境变量:
#   BIN        被测二进制（默认 <repo>/builddir/sdcshield）
#   DURATION   单轮毫秒数（默认 600000）
#   PAUSE      轮间秒数（默认 5）
#   SEED       框架 RNG seed（默认 LCG:随机）
#   CPUS       taskset CPU 列表（默认全部在线核）
#   MODE       all|copy|stlf|regen|protect（默认 all）
#   GEOM_PAGE  几何页大小（4K 主机上模拟 64K 用 65536；默认不设）
#   OUTDIR     日志目录（默认 <repo>/logs/memcpy2）
#
# 判定: 只有 rc==0 且 YAML 中 memcpy2_neon 明确 result: pass 才算 pass；
#       137=OOM/强杀、124=timeout、134/139=崩溃、128+信号 分开计数，绝不混为 SDC。
#       fail 轮立即抓 dmesg/EDAC 快照并保留完整日志；pass 轮只保留紧凑 YAML+统计。
set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if ! ROOT=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null); then
    ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
fi

BIN=${BIN:-$ROOT/builddir/sdcshield}
DURATION=${DURATION:-600000}
PAUSE=${PAUSE:-5}
SEED=${SEED:-}
CPUS=${CPUS:-}
MODE=${MODE:-all}
GEOM_PAGE=${GEOM_PAGE:-}
OUTDIR=${OUTDIR:-$ROOT/logs/memcpy2}

mkdir -p "$OUTDIR"
PIDFILE=$OUTDIR/runner.pid

die() { echo "memcpy2_ab_forever: $*" >&2; exit 1; }

[ -x "$BIN" ] || die "binary not executable: $BIN (run ./scripts/build.sh)"

classify() {
    local rc=$1 log=$2
    if [ "$rc" -eq 124 ]; then echo timeout
    elif [ "$rc" -eq 137 ]; then echo oom-or-sigkill
    elif [ "$rc" -eq 134 ] || [ "$rc" -eq 139 ]; then echo crash
    elif [ "$rc" -ge 128 ]; then echo "signal-$((rc - 128))"
    elif [ "$rc" -ne 0 ]; then echo fail
    elif grep -qE '\- \{ test: memcpy2_neon, result: pass \}' "$log"; then echo pass
    elif grep -q 'test: memcpy2_neon' "$log" && grep -q 'result: pass' "$log"; then echo pass
    else echo no-result
    fi
}

snapshot_health() {
    local seq=$1
    command -v dmesg >/dev/null && dmesg -T 2>/dev/null | tail -n 200 \
        > "$OUTDIR/dmesg-$seq.log" || true
    {
        echo "# EDAC counters"
        for f in /sys/devices/system/edac/mc/*/ce_count /sys/devices/system/edac/mc/*/ue_count; do
            [ -r "$f" ] && echo "$f $(cat "$f")"
        done
    } > "$OUTDIR/edac-$seq.log" 2>/dev/null || true
    if command -v perf >/dev/null && perf list 2>/dev/null | grep -q '/memory_error/'; then
        perf stat -a -e memory_error -- sleep 3 \
            > "$OUTDIR/perf-memory_error-$seq.log" 2>&1 || true
    fi
}

run_once() {
    local seq stats log rc verdict
    seq=$(date +%Y%m%dT%H%M%S)
    stats=$OUTDIR/stats-$seq.txt
    log=$OUTDIR/run-$seq.yaml

    {
        echo "date=$(date -Is)"
        uname -a
        echo "page_size=$(getconf PAGE_SIZE 2>/dev/null || echo unknown)"
        lscpu 2>/dev/null | grep -E 'Model name|NUMA node|L1d|L2|L3|^CPU\(s\)'
        sha256sum "$BIN"
        echo "duration_ms=$DURATION seed=${SEED:-random} cpus=${CPUS:-all} mode=$MODE geom_page=${GEOM_PAGE:-native}"
    } > "$OUTDIR/environment-$seq.txt" 2>/dev/null || true

    local -a cmd=("$BIN" -e memcpy2_neon -t "$DURATION" -o "$log")
    [ -n "$SEED" ] && cmd+=(-s "$SEED")
    local -a envs=(SDCSHIELD_MEMCPY2_MODE="$MODE" SDCSHIELD_MEMCPY2_STATS="$stats")
    [ -n "$GEOM_PAGE" ] && envs+=(SDCSHIELD_MEMCPY2_PAGE_SIZE="$GEOM_PAGE")

    if [ -n "$CPUS" ]; then
        env "${envs[@]}" timeout --signal=TERM $(( DURATION / 1000 + 120 )) \
            taskset -c "$CPUS" "${cmd[@]}" >/dev/null 2>&1
    else
        env "${envs[@]}" timeout --signal=TERM $(( DURATION / 1000 + 120 )) \
            "${cmd[@]}" >/dev/null 2>&1
    fi
    rc=$?
    verdict=$(classify "$rc" "$log")
    echo "$(date -Is) rc=$rc verdict=$verdict log=$log stats=$stats"

    case $verdict in
        pass) ;;
        *)
            snapshot_health "$seq"
            echo "FAIL-EVIDENCE $OUTDIR (rc=$rc verdict=$verdict seq=$seq)" >&2
            ;;
    esac
}

cmd_run() {
    local iterations=${1:-0} i=0
    echo "running: bin=$BIN duration=${DURATION}ms mode=$MODE iterations=$iterations"
    while :; do
        i=$((i + 1))
        run_once
        if [ "$iterations" -gt 0 ] && [ "$i" -ge "$iterations" ]; then
            break
        fi
        sleep "$PAUSE"
    done
}

cmd_start() {
    [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null \
        && die "already running pid=$(cat "$PIDFILE")"
    setsid nohup "$0" run "${1:-0}" >"$OUTDIR/runner.out" 2>&1 &
    echo $! > "$PIDFILE"
    echo "started pid=$! outdir=$OUTDIR"
}

cmd_status() {
    if [ -f "$PIDFILE" ] && kill -0 "$(cat "$PIDFILE")" 2>/dev/null; then
        echo "running pid=$(cat "$PIDFILE") outdir=$OUTDIR"
    else
        echo "not running (outdir=$OUTDIR)"
    fi
    ls -1t "$OUTDIR"/run-*.yaml 2>/dev/null | head -3
}

cmd_stop() {
    if [ ! -f "$PIDFILE" ]; then echo "not running"; return; fi
    local pid; pid=$(cat "$PIDFILE")
    kill -TERM "$pid" 2>/dev/null || true
    for _ in $(seq 1 20); do
        kill -0 "$pid" 2>/dev/null || break
        sleep 0.5
    done
    kill -0 "$pid" 2>/dev/null && kill -KILL "$pid" 2>/dev/null || true
    rm -f "$PIDFILE"
    echo "stopped pid=$pid"
}

case ${1:-run} in
    run)    shift; cmd_run "$@" ;;
    start)  shift; cmd_start "$@" ;;
    status) cmd_status ;;
    stop)   cmd_stop ;;
    *)      die "usage: $0 {run [N]|start [N]|status|stop}" ;;
esac

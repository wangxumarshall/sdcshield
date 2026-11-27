#!/usr/bin/env bash
# memcpy2_neon 单次战役与确定性重放执行器。
# 环境变量：MODE=all|copy|stlf|regen|protect DURATION=12h THREADS CPUSET SEED
#           INJECT=source|destination|guard BIN LOGDIR SDCSHIELD_LD_LIBRARY_PATH
# 命令：run（前台）、start/stop/status（后台）、replay <run-dir>（原环境重放）。
set -uo pipefail

SCRIPT=$(readlink -f "$0")
ROOT=$(readlink -f "$(dirname "$SCRIPT")/../..")
MODE=${MODE:-all}
DURATION=${DURATION:-12h}
THREADS=${THREADS:-}
CPUSET=${CPUSET:-}
SEED=${SEED:-}
INJECT=${INJECT:-}
BIN=${BIN:-$ROOT/builddir/sdcshield}
LOGDIR=${LOGDIR:-$ROOT/out/memcpy2_neon_repro}
RUN_LD=${SDCSHIELD_LD_LIBRARY_PATH:-/usr/lib64}
RUN_DIR=
SUPERVISOR_PID=

usage()
{
    cat <<'EOF'
用法：
  scripts/run/memcpy2_neon_repro.sh run
  scripts/run/memcpy2_neon_repro.sh start
  scripts/run/memcpy2_neon_repro.sh status
  scripts/run/memcpy2_neon_repro.sh stop
  scripts/run/memcpy2_neon_repro.sh replay <run-dir>

默认 MODE=all DURATION=12h，使用全部可用 CPU。THREADS/CPUSET/SEED/INJECT
均可显式指定。replay 保留原参数，不自动缩减核心数。结果分类为 pass、
mismatch、selftest、infrastructure、timeout、crash、oom、skip 或 incomplete；
mismatch 只是数据异常候选，不直接等同于硬件 SDC。
EOF
}

die()
{
    printf 'ERROR: %s\n' "$*" >&2
    exit 2
}

duration_seconds()
{
    python3 - "$1" <<'PY'
import math, re, sys
m = re.fullmatch(r"([0-9]+)(ms|s|m|h)?", sys.argv[1])
if not m:
    raise SystemExit(2)
n = int(m.group(1))
scale = {None: 0.001, "ms": 0.001, "s": 1, "m": 60, "h": 3600}[m.group(2)]
print(max(1, math.ceil(n * scale)))
PY
}

validate_config()
{
    case "$MODE" in all|copy|stlf|regen|protect) ;; *) die "无效 MODE=$MODE" ;; esac
    case "$INJECT" in ''|source|destination|guard) ;; *) die "无效 INJECT=$INJECT" ;; esac
    [[ "$MODE $INJECT" != "stlf source" && "$MODE $INJECT" != "stlf destination" &&
       "$MODE $INJECT" != "stlf guard" ]] || die "stlf 模式不支持主拷贝注入"
    duration_seconds "$DURATION" >/dev/null || die "无效 DURATION=$DURATION"
    [[ "$THREADS" =~ ^[0-9]*$ ]] || die "THREADS 必须为非负整数"
    [[ -x "$BIN" ]] || die "二进制不可执行：$BIN"
    BIN=$(readlink -f "$BIN")
    local v
    for v in "$MODE" "$DURATION" "$THREADS" "$CPUSET" "$SEED" "$INJECT" "$BIN" "$LOGDIR" "$RUN_LD"; do
        [[ "$v" != *$'\n'* && "$v" != *$'\t'* ]] || die "配置值不能含换行或制表符"
    done
}

save_config()
{
    local tmp=$RUN_DIR/replay.conf.tmp
    {
        printf 'BIN\t%s\n' "$BIN"
        printf 'LOGDIR\t%s\n' "$LOGDIR"
        printf 'MODE\t%s\n' "$MODE"
        printf 'DURATION\t%s\n' "$DURATION"
        printf 'THREADS\t%s\n' "$THREADS"
        printf 'CPUSET\t%s\n' "$CPUSET"
        printf 'SEED\t%s\n' "$SEED"
        printf 'INJECT\t%s\n' "$INJECT"
        printf 'RUN_LD\t%s\n' "$RUN_LD"
    } > "$tmp"
    mv "$tmp" "$RUN_DIR/replay.conf"
}

load_config()
{
    local dir=$1 key value
    [[ -r "$dir/replay.conf" ]] || die "缺少 $dir/replay.conf"
    RUN_DIR=$dir
    while IFS=$'\t' read -r key value; do
        case "$key" in
            BIN|LOGDIR|MODE|DURATION|THREADS|CPUSET|SEED|INJECT|RUN_LD)
                printf -v "$key" '%s' "$value" ;;
        esac
    done < "$dir/replay.conf"
    validate_config
}

proc_starttime()
{
    local pid=$1 text rest
    [[ -r "/proc/$pid/stat" ]] || return 1
    read -r text < "/proc/$pid/stat" || return 1
    rest=${text##*) }
    set -- $rest
    [[ $# -ge 20 ]] || return 1
    printf '%s\n' "${20}"
}

proc_pgid()
{
    ps -o pgid= -p "$1" 2>/dev/null | tr -d '[:space:]'
}

snapshot_descendants()
{
    local root=$1 output=$2 include_root=${3:-no}
    python3 - "$root" "$include_root" > "$output.tmp" <<'PY'
import glob, sys
root = int(sys.argv[1])
include_root = sys.argv[2] == "yes"
entries = {}
children = {}
for path in glob.glob("/proc/[0-9]*/stat"):
    try:
        text = open(path).read()
        left = text.index("(")
        right = text.rfind(")")
        pid = int(text[:left])
        fields = text[right + 2:].split()
        entries[pid] = (int(fields[1]), int(fields[2]), fields[19])
    except (OSError, ValueError, IndexError):
        continue
for pid, (ppid, _, _) in entries.items():
    children.setdefault(ppid, []).append(pid)
seen = {root} if include_root else set()
queue = list(children.get(root, []))
while queue:
    pid = queue.pop()
    if pid in seen or pid not in entries:
        continue
    seen.add(pid)
    queue.extend(children.get(pid, []))
for pid in sorted(seen):
    if pid in entries:
        _, pgid, start = entries[pid]
        print(pid, pgid, start)
PY
    mv "$output.tmp" "$output"
}

signal_snapshot()
{
    local file=$1 signal=$2 pid pgid start current_start current_pgid
    local -A seen=()
    [[ -r "$file" ]] || return 0
    while read -r pid pgid start; do
        [[ "$pid" =~ ^[0-9]+$ && "$pgid" =~ ^[0-9]+$ && -z "${seen[$pgid]:-}" ]] || continue
        current_start=$(proc_starttime "$pid") || continue
        current_pgid=$(proc_pgid "$pid")
        [[ "$current_start" == "$start" && "$current_pgid" == "$pgid" ]] || continue
        seen[$pgid]=1
        kill "-$signal" -- "-$pgid" 2>/dev/null || true
    done < "$file"
}

snapshot_alive()
{
    local file=$1 pid pgid start current_start current_pgid
    [[ -r "$file" ]] || return 1
    while read -r pid pgid start; do
        current_start=$(proc_starttime "$pid") || continue
        current_pgid=$(proc_pgid "$pid")
        [[ "$current_start" == "$start" && "$current_pgid" == "$pgid" ]] && return 0
    done < "$file"
    return 1
}

process_matches()
{
    local dir=$1 prefix=${2:-} pid start pgid current_start current_pgid
    [[ -r "$dir/${prefix}pid" && -r "$dir/${prefix}starttime" && -r "$dir/${prefix}pgid" ]] || return 1
    read -r pid < "$dir/${prefix}pid"
    read -r start < "$dir/${prefix}starttime"
    read -r pgid < "$dir/${prefix}pgid"
    [[ "$pid" =~ ^[0-9]+$ && "$pgid" =~ ^[0-9]+$ ]] || return 1
    current_start=$(proc_starttime "$pid") || return 1
    current_pgid=$(proc_pgid "$pid")
    [[ "$current_start" == "$start" && "$current_pgid" == "$pgid" ]]
}

active_run()
{
    [[ -r "$LOGDIR/active" ]] || return 1
    local dir
    read -r dir < "$LOGDIR/active"
    [[ -d "$dir" ]] || return 1
    printf '%s\n' "$dir"
}

edac_snapshot()
{
    local output=$1 f
    : > "$output"
    for f in /sys/devices/system/edac/mc/mc*/ce_count \
             /sys/devices/system/edac/mc/mc*/ue_count \
             /sys/devices/system/edac/mc/mc*/mc_name; do
        [[ -r "$f" ]] || continue
        printf '%s=' "$f" >> "$output"
        while IFS= read -r line || [[ -n "$line" ]]; do
            printf '%s' "$line" >> "$output"
        done < "$f"
        printf '\n' >> "$output"
    done
    [[ -s "$output" ]] || printf 'EDAC unavailable\n' > "$output"
}

capture_metadata()
{
    {
        printf 'timestamp=%s\n' "$(date -u +%Y-%m-%dT%H:%M:%SZ)"
        printf 'uname=%s\n' "$(uname -a)"
        printf 'page_size=%s\n' "$(getconf PAGESIZE)"
        printf 'online_cpus=%s\n' "$(nproc)"
        printf 'binary=%s\n' "$BIN"
        printf 'binary_sha256='; sha256sum "$BIN" | cut -d' ' -f1
        printf 'source=%s\n' "$ROOT/tests/cpu/neon_memcpy_cache/memcpy2_neon.cpp"
        printf 'source_sha256='; sha256sum "$ROOT/tests/cpu/neon_memcpy_cache/memcpy2_neon.cpp" | cut -d' ' -f1
        printf 'script_sha256='; sha256sum "$SCRIPT" | cut -d' ' -f1
    } > "$RUN_DIR/metadata.txt"
    sha256sum "$BIN" | cut -d' ' -f1 > "$RUN_DIR/binary.sha256"
    lscpu > "$RUN_DIR/topology.txt" 2>&1 || true
    edac_snapshot "$RUN_DIR/edac.before"
}

classify_yaml()
{
    python3 - "$1" "$2" <<'PY'
import re, sys
path, rc_text = sys.argv[1:]
rc = int(rc_text)
try:
    text = open(path, encoding="utf-8", errors="replace").read()
except OSError:
    print("incomplete\t")
    raise SystemExit
starts = list(re.finditer(r"(?m)^- test:[ \t]*([^\s#]+)[ \t]*$", text))
blocks = []
for i, match in enumerate(starts):
    if match.group(1) == "memcpy2_neon":
        end = starts[i + 1].start() if i + 1 < len(starts) else len(text)
        blocks.append(text[match.start():end])
if not blocks:
    kind = "timeout" if rc == 124 else "incomplete"
    print(kind + "\t")
    raise SystemExit
joined = "\n".join(blocks)
seed_match = re.search(r"\bseed:[ \t]*'([^']+)'", joined)
seed = seed_match.group(1) if seed_match else ""
results = [m.group(1) for b in blocks for m in [re.search(r"(?m)^  result:[ \t]*([^\n]+)$", b)] if m]
result = results[-1].strip() if results else "missing"
if "SELFTEST-INJECTED" in joined:
    kind = "selftest"
elif "INFRASTRUCTURE-ERROR" in joined or "Init function failed" in joined or result == "operating system error":
    kind = "infrastructure"
elif result == "fail":
    kind = "mismatch" if re.search(r"memcpy2_neon: (?:stage=|DC ZVA|STNP)", joined) else "fail"
elif result == "timed out" or rc == 124:
    kind = "timeout"
elif result == "out of memory":
    kind = "oom"
elif result == "crash":
    kind = "crash"
elif result == "interrupted" or rc > 128:
    kind = "signal-%d" % (rc - 128) if rc > 128 else "interrupted"
elif result == "skip":
    kind = "skip"
elif result == "pass" and rc == 0:
    kind = "pass"
else:
    kind = "incomplete"
print(kind + "\t" + seed)
PY
}

class_to_rc()
{
    case "$1" in
        pass) return 0 ;; selftest) return 1 ;; mismatch|fail) return 2 ;;
        infrastructure) return 3 ;; incomplete) return 4 ;; skip) return 5 ;;
        oom) return 6 ;; crash|signal-*) return 7 ;; timeout) return 124 ;;
        stopped) return 130 ;; *) return 4 ;;
    esac
}

csv_field()
{
    local value=${1//\"/\"\"}
    printf '"%s"' "$value"
}

write_result()
{
    local kind=$1 rc=$2 seed=${3:-} row
    {
        printf 'class=%s\nrc=%s\nseed=%s\nmode=%s\nduration=%s\n' \
               "$kind" "$rc" "$seed" "$MODE" "$DURATION"
        printf 'threads=%s\ncpuset=%s\nrun_dir=%s\n' "$THREADS" "$CPUSET" "$RUN_DIR"
    } > "$RUN_DIR/result.txt"
    if [[ ! -s "$LOGDIR/summary.csv" ]]; then
        printf 'utc,class,rc,mode,duration,threads,cpuset,seed,run_dir\n' > "$LOGDIR/summary.csv"
    fi
    row=$(date -u +%Y-%m-%dT%H:%M:%SZ)
    {
        csv_field "$row"; printf ','
        csv_field "$kind"; printf ','
        csv_field "$rc"; printf ','
        csv_field "$MODE"; printf ','
        csv_field "$DURATION"; printf ','
        csv_field "$THREADS"; printf ','
        csv_field "$CPUSET"; printf ','
        csv_field "$seed"; printf ','
        csv_field "$RUN_DIR"; printf '\n'
    } >> "$LOGDIR/summary.csv"
}

prepare_run()
{
    validate_config
    mkdir -p "$LOGDIR/attempts" || die "无法创建 $LOGDIR/attempts"
    RUN_DIR=$LOGDIR/attempts/$(date -u +%Y%m%dT%H%M%SZ)-$$-$RANDOM
    mkdir "$RUN_DIR" || die "无法创建 $RUN_DIR"
    save_config
    capture_metadata
    printf 'MODE=%s DURATION=%s THREADS=%s CPUSET=%s SEED=%s INJECT=%s\n' \
           "$MODE" "$DURATION" "$THREADS" "${CPUSET:-all}" "${SEED:-auto}" "${INJECT:-none}" \
           > "$RUN_DIR/config.txt"
    printf '%s\n' "$BIN --enable memcpy2_neon --max-test-loop-count 0 -T $DURATION -t $DURATION --strict-runtime ${THREADS:+-n $THREADS} ${CPUSET:+--cpuset=$CPUSET} ${SEED:+-s $SEED}" \
           > "$RUN_DIR/command.txt"
}

supervise()
{
    load_config "$1"
    local seconds watchdog rc kind seed line watchdog_pid watchdog_pgid watchdog_start
    local found victim_pid victim_pgid victim_start
    watchdog_pid=
    watchdog_pgid=
    trap 'signal_snapshot "$RUN_DIR/descendants.snapshot" TERM; if [[ ${watchdog_pgid:-} =~ ^[0-9]+$ ]]; then kill -TERM -- "-$watchdog_pgid" 2>/dev/null; fi; exit 130' TERM INT
    seconds=$(duration_seconds "$DURATION") || exit 2
    watchdog=$((seconds + 120))
    local -a cmd
    cmd=("$BIN" --enable memcpy2_neon
         --max-test-loop-count 0 -T "$DURATION" -t "$DURATION" --strict-runtime
         --retest-on-failure=0 --total-retest-on-failure=0 -F --fatal-skips
         --max-messages 0 --max-logdata 0 --full-log -vv -o "$RUN_DIR/result.yaml")
    [[ -z "$THREADS" ]] || cmd+=(-n "$THREADS")
    [[ -z "$CPUSET" ]] || cmd+=("--cpuset=$CPUSET")
    [[ -z "$SEED" ]] || cmd+=(-s "$SEED")

    export SDCSHIELD_MEMCPY2_MODE=$MODE
    if [[ -n "$INJECT" ]]; then
        export SDCSHIELD_MEMCPY2_INJECT=$INJECT
    else
        unset SDCSHIELD_MEMCPY2_INJECT
    fi
    export LD_LIBRARY_PATH=$RUN_LD
    timeout --signal=TERM --kill-after=15s "${watchdog}s" \
        "${cmd[@]}" > "$RUN_DIR/console.log" 2>&1 &
    watchdog_pid=$!
    printf '%s\n' "$watchdog_pid" > "$RUN_DIR/watchdog.pid"
    for _ in {1..50}; do
        watchdog_pgid=$(proc_pgid "$watchdog_pid")
        [[ "$watchdog_pgid" == "$watchdog_pid" ]] && break
        sleep 0.02
    done
    watchdog_start=$(proc_starttime "$watchdog_pid") || watchdog_start=
    if [[ "$watchdog_pgid" == "$watchdog_pid" && -n "$watchdog_start" ]]; then
        printf '%s\n' "$watchdog_pgid" > "$RUN_DIR/watchdog.pgid"
        printf '%s\n' "$watchdog_start" > "$RUN_DIR/watchdog.starttime"
    fi
    found=0
    for _ in {1..50}; do
        snapshot_descendants "$$" "$RUN_DIR/descendants.snapshot" no || exit 3
        while read -r victim_pid victim_pgid victim_start; do
            if [[ "$victim_pgid" != "$watchdog_pgid" ]]; then
                found=1
                break
            fi
        done < "$RUN_DIR/descendants.snapshot"
        [[ "$found" -eq 0 ]] || break
        sleep 0.1
    done
    wait "$watchdog_pid"
    rc=$?
    trap - TERM INT
    signal_snapshot "$RUN_DIR/descendants.snapshot" TERM
    for _ in {1..20}; do
        snapshot_alive "$RUN_DIR/descendants.snapshot" || break
        sleep 0.1
    done
    signal_snapshot "$RUN_DIR/descendants.snapshot" KILL
    line=$(classify_yaml "$RUN_DIR/result.yaml" "$rc")
    IFS=$'\t' read -r kind seed <<< "$line"
    [[ -z "$seed" ]] || SEED=$seed
    save_config
    edac_snapshot "$RUN_DIR/edac.after"

    while IFS= read -r line; do
        case "$line" in *"coverage mode="*) printf '%s\n' "$line" ;; esac
    done < "$RUN_DIR/result.yaml" > "$RUN_DIR/coverage.txt" 2>/dev/null || true

    if [[ "$kind" == pass ]]; then
        gzip -9 -f "$RUN_DIR/result.yaml" 2>/dev/null || true
    else
        dmesg -T > "$RUN_DIR/dmesg.txt" 2> "$RUN_DIR/dmesg.error" || true
    fi
    write_result "$kind" "$rc" "$SEED"
    printf '%s rc=%s seed=%s dir=%s\n' "$kind" "$rc" "${SEED:-unavailable}" "$RUN_DIR"
    class_to_rc "$kind"
}

start_impl()
{
    local active pid start pgid n
    if active=$(active_run) && process_matches "$active"; then
        read -r pid < "$active/pid"
        die "已有运行中的战役 pid=$pid dir=$active"
    fi
    prepare_run
    setsid "$SCRIPT" __supervise "$RUN_DIR" > "$RUN_DIR/supervisor.log" 2>&1 < /dev/null &
    SUPERVISOR_PID=$!
    printf '%s\n' "$SUPERVISOR_PID" > "$RUN_DIR/pid"
    start=$(proc_starttime "$SUPERVISOR_PID") || die "无法读取监督进程 starttime"
    pgid=
    for n in {1..50}; do
        pgid=$(proc_pgid "$SUPERVISOR_PID")
        [[ "$pgid" == "$SUPERVISOR_PID" ]] && break
        sleep 0.02
    done
    [[ "$pgid" == "$SUPERVISOR_PID" ]] || die "监督进程未建立独立进程组"
    printf '%s\n' "$start" > "$RUN_DIR/starttime"
    printf '%s\n' "$pgid" > "$RUN_DIR/pgid"
    printf '%s\n' "$RUN_DIR" > "$LOGDIR/active.tmp"
    mv "$LOGDIR/active.tmp" "$LOGDIR/active"
    printf 'started pid=%s pgid=%s dir=%s\n' "$SUPERVISOR_PID" "$pgid" "$RUN_DIR"
}

stop_impl()
{
    local dir pid n
    dir=$(active_run) || { printf 'not running\n'; return 0; }
    [[ ! -r "$dir/result.txt" ]] || { printf 'already finished: '; cat "$dir/result.txt"; return 0; }
    if ! process_matches "$dir"; then
        printf 'not running (stale or identity-mismatched pid) dir=%s\n' "$dir"
        return 0
    fi
    read -r pid < "$dir/pid"
    snapshot_descendants "$pid" "$dir/stop.snapshot" yes || die "无法保存后代进程身份"
    signal_snapshot "$dir/stop.snapshot" TERM
    for n in {1..50}; do
        snapshot_alive "$dir/stop.snapshot" || break
        sleep 0.1
    done
    signal_snapshot "$dir/stop.snapshot" KILL
    if [[ ! -r "$dir/result.txt" ]]; then
        load_config "$dir"
        write_result stopped 130 "$SEED"
    fi
    printf 'stopped pid=%s dir=%s\n' "$pid" "$dir"
}

status_impl()
{
    local dir pid
    dir=$(active_run) || { printf 'no runs\n'; return 0; }
    if [[ -r "$dir/result.txt" ]]; then
        cat "$dir/result.txt"
    elif process_matches "$dir"; then
        read -r pid < "$dir/pid"
        printf 'running pid=%s dir=%s\n' "$pid" "$dir"
    else
        printf 'incomplete: supervisor not running and no parsed result dir=%s\n' "$dir"
        return 4
    fi
}

replay_impl()
{
    local original=$1 expected actual rc
    [[ -d "$original" ]] || die "重放目录不存在：$original"
    load_config "$original"
    [[ -n "$SEED" ]] || die "原日志未保存 RNG state，不能确定性重放"
    [[ -r "$original/binary.sha256" ]] || die "原运行缺少二进制哈希"
    read -r expected < "$original/binary.sha256"
    actual=$(sha256sum "$BIN" | cut -d' ' -f1) || die "无法计算二进制哈希"
    [[ "$actual" == "$expected" ]] || die "二进制已改变，拒绝伪精确重放：$BIN"
    start_impl
    wait "$SUPERVISOR_PID"
    rc=$?
    [[ ! -r "$RUN_DIR/result.txt" ]] || cat "$RUN_DIR/result.txt"
    return "$rc"
}

case "${1:-}" in
    run)
        start_impl
        wait "$SUPERVISOR_PID"
        rc=$?
        [[ ! -r "$RUN_DIR/result.txt" ]] || cat "$RUN_DIR/result.txt"
        exit "$rc" ;;
    start) start_impl ;;
    stop) stop_impl ;;
    status) status_impl ;;
    replay) [[ $# -eq 2 ]] || die "replay 需要 run-dir"; replay_impl "$2"; exit $? ;;
    __supervise) [[ $# -eq 2 ]] || exit 2; supervise "$2"; exit $? ;;
    -h|--help|help) usage ;;
    *) usage; exit 2 ;;
esac

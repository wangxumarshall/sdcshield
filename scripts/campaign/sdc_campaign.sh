#!/bin/bash
# sdc_campaign.sh — SDC 7×24 战役驱动（L0–L5 状态机 + 断点恢复 + 事件取证）
#
# 用法: sdc_campaign.sh [smoke|full]     （默认 full；所有时长可 env 覆盖）
# 由 sdc-campaign.service（User=sdc）托管；崩溃由 systemd Restart=always 拉起，
# 进度存 state.json（断点续跑），SDC/崩溃事件进取证流水线后战役继续（普查模式）。
# 依据 plan: docs/superpowers/plans/2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-plan.md §4-§7
set -u
MODE="${1:-full}"
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/sdc_common.sh"
ensure_dirs

# ---------------- 时长表（smoke/full 两档）----------------
if [ "$MODE" = smoke ]; then
    T_COLD=2m;  T_SWEEP=30s; T_DWELLM=1m; T_L3F=2m; T_L3R=1m
    T_L5=1m;    T_L4E=1m;    T_IPSEC=30s; T_MEMCPY=30s; T_DOM=1m
    T_HEAT=1m;  T_TARGET=1m; T_EIGEN=30s
else
    T_COLD=30m; T_SWEEP=15m; T_DWELLM=15m; T_L3F=6h; T_L3R=2h
    T_L5=90m;   T_L4E=2h;    T_IPSEC=15m; T_MEMCPY=15m; T_DOM=30m
    T_HEAT=30m; T_TARGET=30m; T_EIGEN=300s
fi
DUR_S() { python3 -c "
import sys
s = sys.argv[1].strip(); u = s[-1]; n = float(s[:-1] or 0)
print(int(n * {'s': 1, 'm': 60, 'h': 3600}[u]))" "$1"; }

# ---------------- campaign.env 生成（首次探测，零硬编码）----------------
gen_env() {
    local nproc crit=0 v
    nproc=$(nproc)
    for z in /sys/class/thermal/thermal_zone*; do
        v=$(cat "$z/trip_point_0_temp" 2>/dev/null)
        [ -n "$v" ] && [ "$v" -gt "$crit" ] 2>/dev/null && crit=$v
    done
    [ "$crit" -gt 0 ] || crit=105000   # 无 ACPI trip 板卡回退通用服务器值（记录于 env 注释）
    local last_node stress_cpus
    last_node=$(ls -d /sys/devices/system/node/node* | sort -V | tail -1)
    stress_cpus=$(python3 -c "
import sys
spec = open('$last_node/cpulist').read().strip().split(',')
last = spec[-1]
if '-' in last:
    a, b = last.split('-'); print(','.join(str(x) for x in range(int(b) - 3, int(b) + 1)))
else:
    print(last)")
    cat > "$CAMPAIGN_DIR/campaign.env" <<EOF
# 由 sdc_campaign.sh 首次运行探测生成（$(date -Is)）。删除本文件可重新生成。
# thermal_crit_milli=$crit（ACPI trip_point_0_temp 最大值）
NPROC=$nproc
THERMAL_PAUSE_MILLI=$((crit - 10000))
THERMAL_RESUME_MILLI=$((crit - 15000))
THERMAL_PAUSE_C=$(( (crit - 10000) / 1000 ))
THERMAL_RESUME_C=$(( (crit - 15000) / 1000 ))
DISK_WARN_PCT=85
DISK_STOP_PCT=95
MDIM_FULLCORE_MAX=$(max_mdim_for_threads "$nproc")
STRESSNG_CPUS=${stress_cpus:-}
L3_DISABLE=eigen_svd_double,eigen_sparse
DWELL_DEFAULT=openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip,openssl_sha
EOF
    log "campaign.env 生成: $(tr '\n' ' ' < "$CAMPAIGN_DIR/campaign.env")"
}
[ -f "$CAMPAIGN_DIR/campaign.env" ] || gen_env
# shellcheck disable=SC1090
source "$CAMPAIGN_DIR/campaign.env"

# ---------------- sdcshield 旗标 feature-detect ----------------
# 解析探测（--help 文本不完整，L0 实测）：<flag> -l 立即退出，rc=64=选项不存在
flag_ok() { "$BIN" "$1" -l >/dev/null 2>&1; [ $? -ne 64 ]; }
FLAGS=()
flag_ok --on-crash=context     && FLAGS+=(--on-crash=context)
flag_ok --on-hang=kill         && FLAGS+=(--on-hang=kill)
flag_ok --ignore-timeout       && FLAGS+=(--ignore-timeout)
flag_ok --ignore-os-errors     && FLAGS+=(--ignore-os-errors)
flag_ok --retest-on-failure=3  && FLAGS+=(--retest-on-failure=3)
# 本板为 no-op（thermal_monitor.hpp 已知热区类型表不含 acpitz）；保留给未来单板
flag_ok --temperature-threshold=95000 && FLAGS+=(--temperature-threshold="$THERMAL_PAUSE_MILLI")
log "旗标集: ${FLAGS[*]:-无}"

# ---------------- 用例名解析（--list-tests 实测名单，防硬编码）----------------
LIST_CACHE=$(mktemp /tmp/.sdc_list_tests.XXXXXX)
trap 'rm -f "$LIST_CACHE"' EXIT
"$BIN" --list-tests 2>/dev/null | awk '{print $1}' > "$LIST_CACHE"
resolve() { # resolve 'pat1,pat2' → 实际存在的用例名（逗号连接；* 转正则 .*）
    local out="" p t
    for p in ${1//,/ }; do
        while IFS= read -r t; do
            [ -n "$t" ] && out="${out:+$out,}$t"
        done < <(grep -E "^${p//\*/.*}$" "$LIST_CACHE" 2>/dev/null)
    done
    echo "$out"
}
L5_SUITE=$(resolve 'openblas_dgemm,sleef_neon,fma*,cachebounce,lock*,atomic_simd_*,memcpy_rewr')
IPSEC_SUBSET=$(resolve 'ipsec_*' | cut -d, -f1-6)

# ---------------- 执行封装 ----------------
run_sdc() { # run_sdc <label> <phase_timeout_s|0> <sdcshield args...>（自动追加 -o）
    local label="$1" pto="$2"; shift 2
    check_pause
    local ts day yaml out
    ts=$(date +%H%M%S); day=$(date +%F | tr -d -)
    mkdir -p "$LOG_ROOT/$day"
    yaml="$LOG_ROOT/$day/${ts}-${label}.yaml"
    out="$LOG_ROOT/$day/${ts}-${label}.out"
    state_set phase "$label"
    state_set phase_cmd "$BIN $*"
    if [ "$pto" -gt 0 ]; then
        timeout --signal=TERM --kill-after=90s "$pto" "$BIN" "$@" -o "$yaml" > "$out" 2>&1
    else
        "$BIN" "$@" -o "$yaml" > "$out" 2>&1
    fi
    local rc=$?
    [ $rc -eq 124 ] && log "$label 达到阶段时长（正常切换）"
    # 失败信号双通道：YAML result: fail/crash + 进程 rc（timeout/oserror 进日汇总）
    if grep -Eq 'result: *(fail|crash)' "$yaml" 2>/dev/null || { [ $rc -ne 0 ] && [ $rc -ne 124 ]; }; then
        handle_failure "$label" "$yaml" "$rc" "$*"
    fi
    return 0
}

handle_failure() { # 取证 + 复测×3 + 台账（普查模式：战役不中断）plan §7
    local label="$1" yaml="$2" rc="$3"; shift 3
    local cmd="$*"
    local evdir="$EVENTS_DIR/$(date +%Y%m%d-%H%M%S)-${label}-rc${rc}"
    mkdir -p "$evdir"
    cp "$yaml" "$evdir/" 2>/dev/null
    # 种子与失败测试提取（L0 实测格式：state: { seed: 'AES:<hex>' } / "- test: <name>"）
    local seed failed_test seed_ok=""
    seed=$(grep -oE "seed: 'AES:[0-9a-f]+'" "$yaml" 2>/dev/null | head -1 | cut -d"'" -f2)
    failed_test=$(grep -m1 '^- test:' "$yaml" 2>/dev/null | awk '{print $3}')
    if [ -n "$seed" ]; then
        "$BIN" -s "$seed" -l >/dev/null 2>&1
        [ $? -ne 64 ] && seed_ok=1
    fi
    {
        echo "cmd: $BIN $cmd"
        echo "rc: $rc  date: $(date -Is)  seed: ${seed:-none}(usable=$seed_ok)  failed_test: ${failed_test:-?}"
        echo "--- 失败行 ---"
        grep -E 'result: *(fail|crash)' "$yaml" 2>/dev/null | head -20
        echo "--- monitor 最近 20 行 ---"
        tail -20 "$MON_DIR/monitor.csv" 2>/dev/null
        echo "--- EDAC ---"
        cat /sys/devices/system/edac/mc/mc*/ce_count /sys/devices/system/edac/mc/mc*/ue_count 2>/dev/null
        echo "--- mem ---"
        free -h
    } > "$evdir/context.txt"
    touch "$CMD_DIR/snapshot.request"   # root 监控 60s 内补 dmesg/SEL/SDR 快照
    local i rcf
    : > "$evdir/retests.txt"
    for i in 1 2 3; do
        sleep 5; check_pause
        if [ -n "$seed_ok" ]; then
            "$BIN" $cmd -s "$seed" -o "$evdir/retest$i.yaml" > "$evdir/retest$i.out" 2>&1; rcf=$?
        else
            "$BIN" $cmd -o "$evdir/retest$i.yaml" > "$evdir/retest$i.out" 2>&1; rcf=$?
        fi
        echo "retest$i(seed=${seed_ok:+on}) rc=$rcf fail/crash=$(grep -Ec 'result: *(fail|crash)' "$evdir/retest$i.yaml" 2>/dev/null)" >> "$evdir/retests.txt"
    done
    echo "$(date '+%F %T'),$label,rc=$rc,seed=${seed_ok:+$seed},$(tr '\n' ';' < "$evdir/retests.txt"),$evdir" >> "$EVENTS_DIR/ledger.csv"
    alert "SDC/崩溃事件: $label rc=$rc test=${failed_test:-?} → $evdir（战役继续）"
    [ -n "$failed_test" ] && DWELL_OVERRIDE="$failed_test"   # 本周期 L4 优先深驻留该测试
}

# ---------------- stress-ng 补充层（后台，小时轮转 --verify）----------------
stressng_layer() {
    local list i=0 s
    # 白名单取自本机 stress-ng --verifiable 实测可用集（CPU 型、内存足迹小）
    list=$(stress-ng --verifiable 2>/dev/null | awk 'NR>1 {print $1}' \
            | grep -xE 'matrix|matrix-3d|qsort|radixsort|mergesort|insertionsort|bsearch|fma|vecfp|vecwide|skiplist|hash|judy|tsearch|hsearch|fibsearch|lsearch' | tr '\n' ' ')
    [ -z "$list" ] && list="matrix"
    log "stress-ng 补充层启动（保留核 $STRESSNG_CPUS，轮转: $list）"
    while :; do
        set -- $list
        s=${@:$(( i % $# + 1 )):1}
        i=$((i + 1))
        if [ -f "$PAUSE_FLAG" ]; then     # 联锁暂停期间不产生新热量
            sleep 60; continue
        fi
        timeout 55m taskset -c "$STRESSNG_CPUS" nice -n 10 \
            stress-ng --"$s" 4 --verify --metrics-brief --timeout 55m \
            >> "$STRESSNG_DIR/stressng.log" 2>&1
        echo "[$(date '+%F %T')] stressor=$s rc=$?" >> "$STRESSNG_DIR/stressng.log"
        sleep 300
    done
}

run_bounded() { # run_bounded <label> <预算秒> <单测上限秒> <args...>
    # 多用例有界阶段：-T 预算 + --strict-runtime（L0 实测：有限 -T 单独不硬停，
    # 9.11s>4s；加 strict 后 6.08s≈预算+在飞用例），timeout 兜底防 hang。
    local label="$1" budget="$2" per="$3"; shift 3
    local pto=$(( budget + per + 120 ))
    local sf=()
    flag_ok --strict-runtime && sf=(--strict-runtime)
    run_sdc "$label" "$pto" -T "${budget}s" -t "${per}s" "${sf[@]}" "$@"
}

# ---------------- L5 辅助 ----------------
gov() { echo "$1" > "$CMD_DIR/governor.request"; sleep 35; }   # root 监控代理写 governor

# ---------------- 阶段 ----------------
phase_cold() {
    local reps
    reps=$(resolve 'memcpy_rewr,cachebounce,lock*,atomic_simd_*,fma*,crc32,mul64,openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip,zstd19,openssl_sha,openssl_sm3sm4,acl_gemm')
    if [ -z "$reps" ]; then log "冷机首轮：无匹配用例，跳过"; return; fi
    log "[cycle $CYCLE] 冷机首轮（L1 代表集）。基线温度: $(tail -1 "$MON_DIR/monitor.csv" 2>/dev/null | cut -d, -f2,3)"
    run_bounded "cold_c${CYCLE}" "$(DUR_S "$T_COLD")" 60 -e "$reps" "${FLAGS[@]}"
}

phase_l2() {
    log "[cycle $CYCLE] L2 谱系扫档（复用 run_sdc_spectrum.sh + ipsec/memcpy 扩展）"
    ( cd "$REPO_DIR" && SWEEP_TIME="$T_SWEEP" DWELL_TIME="$T_DWELLM" \
        bash scripts/run/run_sdc_spectrum.sh ) > "$LOG_ROOT/spectrum_c${CYCLE}.out" 2>&1
    local tsdir
    tsdir=$(ls -dt "$REPO_DIR"/sdc_spectrum_* 2>/dev/null | head -1)
    [ -n "$tsdir" ] && mv "$tsdir" "$LOG_ROOT/spectrum_c${CYCLE}_files"
    # spectrum 产物中的失败同样进取证流水线
    local f
    for f in "$LOG_ROOT/spectrum_c${CYCLE}_files"/*.yaml; do
        [ -f "$f" ] || continue
        if grep -q 'result: *fail' "$f" 2>/dev/null; then
            handle_failure "l2_$(basename "$f" .yaml)" "$f" 0 "spectrum sweep"
        fi
    done
    # 1g: ipsec 数据路径谱系（代表子集 × datasize）
    if [ -n "$IPSEC_SUBSET" ]; then
        local ds t oargs
        for ds in 1024 65536 16777216; do
            oargs=(); IFS=',' read -ra arr <<< "$IPSEC_SUBSET"
            for t in "${arr[@]}"; do oargs+=(-O "$t.datasize=$ds"); done
            run_bounded "l2_ipsec_ds${ds}_c${CYCLE}" "$(DUR_S "$T_IPSEC")" 120 \
                -e "$IPSEC_SUBSET" "${oargs[@]}" "${FLAGS[@]}"
        done
    fi
    # 1h: memcpy_rewr 三策略（跨 NUMA / 同 die L3 对打 / 目录失效风暴）
    local s
    for s in 0 1 2; do
        export SANDSTONE_STRATEGY_INDEX=$s
        run_sdc "l2_memcpy_s${s}_c${CYCLE}" 0 -e memcpy_rewr -t "$T_MEMCPY" "${FLAGS[@]}"
    done
    unset SANDSTONE_STRATEGY_INDEX
}

phase_l3() {
    log "[cycle $CYCLE] L3 多样性轮转（固定序 $(DUR_S "$T_L3F")s + 随机序 $(DUR_S "$T_L3R")s + eigen -n1）"
    local dis=()
    if [ -n "${L3_DISABLE:-}" ] && flag_ok --disable=zstd19; then
        dis=(--disable "$L3_DISABLE")
    fi
    run_sdc "l3_fixed_c${CYCLE}" "$(DUR_S "$T_L3F")" -T forever -t 60s "${dis[@]}" "${FLAGS[@]}"
    run_sdc "l3_rand_c${CYCLE}"  "$(DUR_S "$T_L3R")" -T forever -t 60s --test-list-randomize "${dis[@]}" "${FLAGS[@]}"
    local eig; eig=$(resolve 'eigen*')
    [ -n "$eig" ] && run_bounded "l3_eigen_n1_c${CYCLE}" "$(DUR_S "$T_EIGEN")" 60 -e "$eig" -n 1 "${FLAGS[@]}"
}

l5_dit() {
    local pv q=()
    pv=$(resolve 'power_virus*,arm64_sdc')
    if [ -n "$pv" ]; then q=(--quality=0); else pv=$(resolve 'fma*,cachebounce'); fi
    [ -z "$pv" ] && { log "L5 di/dt：无可用负载，跳过"; return; }
    local rounds=6; [ "$MODE" = smoke ] && rounds=1
    local i burst=$(( $(DUR_S "$T_L5") / 12 ))
    [ $burst -lt 60 ] && burst=60
    for i in $(seq 1 $rounds); do
        gov powersave; sleep 30
        gov performance
        run_bounded "l5_dit_r${i}_c${CYCLE}" "$burst" 60 -e "$pv" "${q[@]}" "${FLAGS[@]}"
    done
    gov performance   # 恢复
}

l5_heatsoak() {
    log "L5 热激发：预热至 $(( THERMAL_PAUSE_C - 10 ))C 带"
    local target=$(( THERMAL_PAUSE_C - 10 )) cur=0 i
    local heat_suite; heat_suite=$(resolve 'openblas_dgemm,cachebounce')
    [ -z "$heat_suite" ] && return
    local iters=$(( $(DUR_S "$T_HEAT") / 300 )); [ $iters -lt 1 ] && iters=1
    for i in $(seq 1 $iters); do
        run_bounded "l5_heat_c${CYCLE}_i${i}" 300 300 -e "$heat_suite" "${FLAGS[@]}"
        cur=$(tail -1 "$MON_DIR/monitor.csv" 2>/dev/null | awk -F, '{print ($2 > $3) ? $2 : $3}')
        if [ -n "$cur" ] && [ "$cur" -ge "$target" ] 2>/dev/null; then
            log "L5 热激发：已达目标温度带 ${cur}C"
            break
        fi
    done
    local tgt; tgt=$(resolve 'openblas_dgemm,sleef_neon,fma*')
    [ -n "$tgt" ] && run_bounded "l5_hot_target_c${CYCLE}" "$(DUR_S "$T_TARGET")" 600 -e "$tgt" "${FLAGS[@]}"
    log "L5 热激发完成，末态温度: $(tail -1 "$MON_DIR/monitor.csv" 2>/dev/null | cut -d, -f2,3)"
}

l5_numa() {
    local s
    for s in 0 1 2; do
        export SANDSTONE_STRATEGY_INDEX=$s
        run_sdc "l5_numa_s${s}_c${CYCLE}" 0 -e memcpy_rewr -t "$(( $(DUR_S "$T_L5") / 4 ))s" "${FLAGS[@]}"
    done
    unset SANDSTONE_STRATEGY_INDEX
    # 大 mdim 档（内存预算守卫：NPROC/2 线程，scratch ≤ MemAvailable/2）
    local n64 m
    n64=$(( NPROC / 2 ))
    m=$(max_mdim_for_threads "$n64")
    [ "$m" -gt 2048 ] && m=2048
    [ "$m" -lt 2048 ] && log "L5 NUMA: mdim 2048 超内存预算，降档 mdim=$m"
    run_sdc "l5_numa_mdim${m}_c${CYCLE}" 0 -e openblas_dgemm -O "openblas_dgemm.mdim=$m" -n "$n64" \
        -t "$(( $(DUR_S "$T_L5") / 4 ))s" "${FLAGS[@]}"
}

l5_per_domain() {
    [ -z "$L5_SUITE" ] && { log "L5 逐域：无套件，跳过"; return; }
    local p
    for p in p0 p1 p2 p3; do
        run_bounded "l5_dom_${p}_c${CYCLE}" "$(DUR_S "$T_DOM")" 60 -e "$L5_SUITE" --cpuset="$p" "${FLAGS[@]}"
    done
}

phase_l5() {
    case $(( CYCLE % 4 )) in
        0) l5_dit ;;
        1) l5_heatsoak ;;
        2) l5_numa ;;
        3) l5_per_domain ;;
    esac
}

phase_l4() {
    local list="$DWELL_DEFAULT" t
    if [ -n "${DWELL_OVERRIDE:-}" ]; then
        list="$DWELL_OVERRIDE"; DWELL_OVERRIDE=""
        log "L4 驻留对象切换为本周期事件测试: $list"
    fi
    local remain each
    if [ "$MODE" = smoke ]; then
        remain=$(DUR_S "$T_L4E")
    else
        remain=$(( $(date -d 'tomorrow 07:30' +%s) - $(date +%s) ))
    fi
    local n=0; IFS=',' read -ra arr <<< "$list"; n=${#arr[@]}
    [ $n -eq 0 ] && { log "L4 无驻留对象，跳过"; return; }
    each=$(( remain / n ))
    [ $each -lt 60 ] && each=60
    log "L4 深驻留：$n 个对象 × ${each}s（关 fracturing 固定模式）"
    for t in "${arr[@]}"; do
        run_sdc "l4_dwell_${t}_c${CYCLE}" 0 -e "$t" -t "${each}s" --max-test-loop-count=0 "${FLAGS[@]}"
    done
}

daily_summary() {
    {
        echo "=== 日汇总 $(date '+%F %T') cycle=$CYCLE ==="
        echo "事件数: $(wc -l < "$EVENTS_DIR/ledger.csv" 2>/dev/null || echo 0)"
        echo "今日 YAML: $(find "$LOG_ROOT/$(date +%F | tr -d -)" -name '*.yaml' 2>/dev/null | wc -l)"
        echo "fail/crash 文件数: $(grep -lE 'result: *(fail|crash)' "$LOG_ROOT"/*/*.yaml 2>/dev/null | wc -l)"
        echo "timeout/oserror 行数: $(grep -hE 'result: *(timeout|oserror)' "$LOG_ROOT"/*/*.yaml 2>/dev/null | wc -l)"
        echo "最新工况: $(tail -1 "$MON_DIR/monitor.csv" 2>/dev/null)"
    } >> "$CAMPAIGN_DIR/daily_summary.log"
    log "日汇总完成"
}

# ---------------- 主循环 ----------------
cleanup() {
    [ -n "${STRESS_PID:-}" ] && kill "$STRESS_PID" 2>/dev/null
    [ -d /sys/devices/system/cpu/cpu0/cpufreq ] && echo performance > "$CMD_DIR/governor.request" 2>/dev/null
}
trap cleanup EXIT INT TERM

if [ -n "${STRESSNG_CPUS:-}" ] && command -v stress-ng >/dev/null; then
    stressng_layer &
    STRESS_PID=$!
fi

CYCLE=$(state_get cycle)
[ -z "$CYCLE" ] && CYCLE=0
log "战役启动 mode=$MODE bin=$BIN cycle起点=$CYCLE NPROC=$NPROC 热联锁=${THERMAL_PAUSE_C}C(监控侧) mdim全核上限=$MDIM_FULLCORE_MAX"
while :; do
    CYCLE=$((CYCLE + 1))
    state_set cycle "$CYCLE"
    state_set cycle_start "$(date -Is)"
    phase_cold
    phase_l2
    phase_l3
    phase_l5
    phase_l4
    daily_summary
    if [ "$MODE" = smoke ]; then
        log "smoke 完成（全链路走通）"
        exit 0
    fi
done

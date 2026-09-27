#!/bin/bash
# drill_all.sh — M5 故障演练编排器（v5 §17.2 十三类验收基准，M5 退出标准
# 的核心验收器）。顺序跑 13 类（每类独立——前类失败不阻断后类）+ 汇总
# PASS/FAIL 表 + 退出码（0=全过）。
#
# 用法: bash drill_all.sh [--only d01,d05] [--out DIR]
#   --only   子集过滤（dXX 前缀或全名，逗号分隔；DRILL_ONLY 环境变量同效
#            ——pytest 快速子集控制）
#   --out    演练产物根（默认 <drills>/out；DRILL_OUT 环境变量同效）
#
# 产物：$DRILL_OUT/<类名>/（隔离根证据目录）+ $DRILL_OUT/drill_all.log
# （全量日志）+ 退出码汇总。每类输出 "== <类名>: PASS ==" / 断言失败退出 1。
#
# 安全边界：全部演练在隔离根/测试进程上（drill_lib 真实根形态守卫）——
# 绝不注入真实 spool、绝不 kill 真实服务、绝不真断网/真满盘/真写 sysfs。
set -uo pipefail   # 不用 -e：单类失败不阻断后类（汇总以退出码呈现）

DRILLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ONLY="${DRILL_ONLY:-}"
OUT="${DRILL_OUT:-$DRILLS_DIR/out}"

while [ "$#" -gt 0 ]; do
    case "$1" in
        --only) ONLY="$2"; shift 2 ;;
        --out)  OUT="$2"; shift 2 ;;
        *) echo "用法: $0 [--only d01,d05] [--out DIR]" >&2; exit 2 ;;
    esac
done

# v5 §17.2 十三类（顺序=验收清单序）
ALL_DRILLS=(
    d01_synthetic_mismatch d02_test_bug d03_spurious_fault
    d04_ce d05_ue d06_panic d07_runner_hang d08_bmc_timeout
    d09_collector_crash d10_network_split d11_disk_full
    d12_clock_jump d13_restore_fail
)

# 子集过滤（dXX 前缀或全名匹配）
SELECTED=()
for d in "${ALL_DRILLS[@]}"; do
    [ -z "$ONLY" ] && { SELECTED+=("$d"); continue; }
    IFS=','
    for pat in $ONLY; do
        [ "$d" = "$pat" ] || [ "${d#${pat}}" != "$d" ] && { SELECTED+=("$d"); break; }
    done
    unset IFS
done
[ "${#SELECTED[@]}" -gt 0 ] || { echo "✗ --only 过滤后无匹配演练: $ONLY" >&2; exit 2; }

export DRILL_OUT="$OUT"
mkdir -p "$OUT"
LOG="$OUT/drill_all.log"
: > "$LOG"

echo "== drill_all: ${#SELECTED[@]}/${#ALL_DRILLS[@]} 类，产物根 $OUT =="
declare -a NAMES RESULTS
FAIL_N=0
for d in "${SELECTED[@]}"; do
    echo "-- $d ..." | tee -a "$LOG"
    if bash "$DRILLS_DIR/$d.sh" >> "$LOG" 2>&1; then
        RESULTS+=("PASS"); echo "   PASS"
    else
        RESULTS+=("FAIL"); FAIL_N=$((FAIL_N + 1)); echo "   FAIL（尾部输出：）"
        tail -5 "$LOG" | sed 's/^/     /'
    fi
    NAMES+=("$d")
done

# ---- 汇总表 ----
SUMMARY="$OUT/summary.txt"
{
    echo "== M5 故障演练汇总（v5 §17.2，$(( ${#SELECTED[@]} - FAIL_N ))/${#SELECTED[@]} PASS）=="
    for i in "${!NAMES[@]}"; do
        printf '%-24s %s\n' "${NAMES[$i]}" "${RESULTS[$i]}"
    done
} | tee "$SUMMARY"

if [ "$FAIL_N" -eq 0 ]; then
    echo "== drill_all: 全部 PASS（$(( ${#SELECTED[@]} ))/${#SELECTED[@]}）=="
    exit 0
fi
echo "== drill_all: ${FAIL_N} 类 FAIL（详见 $LOG）==" >&2
exit 1

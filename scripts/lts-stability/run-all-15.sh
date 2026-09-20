#!/bin/bash
# run-all-15.sh — 一键式:15 个 openEuler LTS 镜像 × SDCShield 全量用例 × 全严格选项稳定性验证。
#
# 编排:3 系列(24.03/22.03/20.03)并行(每系列独立源码副本,消除共享 /src 挂载的
# 并发 :Z relabel 竞态),系列内 5 个 SP 串行(每 SP:容器内原生构建 + 29 条目选项矩阵)。
# 全部 15 个 RESULT: PASS 才算通过(exit 0)。
#
# 用法:
#   ./scripts/lts-stability/run-all-15.sh [--smoke]
#     --smoke  每镜像只跑 m01 基线条目(快速链路验证,~15 分钟/系列)
#
# 环境变量(透传给 run-lts-stability.sh):
#   T_BASE / T_SHORT / STEP_TIMEOUT / SDCSRC_BASE / SDCSRC_COPY / SKIP_BUILD
#
# 输出:
#   build-out/lts-stability/all-15-<时间戳>.log   汇总日志(15 个 RESULT 行)
#   build-out/lts-stability/<tag>/                每镜像明细(build.log/matrix.log/*.yaml)
#
# 实测参考(2026-09-17,Kunpeng 920 191 核,main ebe0bf1):三系列并行总计 ~5 小时。
#
# SPDX-License-Identifier: Apache-2.0
set -uo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT_BASE="$SCRIPT_DIR/../../build-out/lts-stability"
mkdir -p "$OUT_BASE"
STAMP="$(date +%Y%m%d-%H%M%S)"
LOG="$OUT_BASE/all-15-$STAMP.log"

SMOKE_ARG=""
[ "${1:-}" = "--smoke" ] && SMOKE_ARG="--smoke"

echo "==== 15-image LTS stability validation $STAMP ====" | tee -a "$LOG"

# 3 系列并行;每系列日志单独落盘,汇总进 $LOG
pids=()
for series in 24.03 22.03 20.03; do
    "$SCRIPT_DIR/run-lts-stability.sh" "$series" all $SMOKE_ARG \
        > "$OUT_BASE/series-$series-$STAMP.log" 2>&1 &
    pids+=($!)
    echo "series $series pid=$!" | tee -a "$LOG"
done

# 等待全部完成
fail=0
for i in "${!pids[@]}"; do
    series=$(echo "24.03 22.03 20.03" | awk -v n="$i" '{print $n}')
    if wait "${pids[$i]}"; then
        echo "series $series: OK" | tee -a "$LOG"
    else
        echo "series $series: FAILED" | tee -a "$LOG"
        fail=1
    fi
done

# 汇总 15 个 RESULT 行
echo "" | tee -a "$LOG"
echo "==== 15-IMAGE SUMMARY ====" | tee -a "$LOG"
pass_n=0; fail_n=0
for series in 24.03 22.03 20.03; do
    grep -E '^RESULT: ' "$OUT_BASE/series-$series-$STAMP.log" 2>/dev/null | while read -r line; do
        echo "$line" | tee -a "$LOG"
    done
done
pass_n=$(grep -hE '^RESULT: PASS' "$OUT_BASE"/series-*-"$STAMP".log 2>/dev/null | wc -l)
fail_n=$(grep -hE '^RESULT: FAIL' "$OUT_BASE"/series-*-"$STAMP".log 2>/dev/null | wc -l)
echo "" | tee -a "$LOG"
echo "PASS: $pass_n / 15   FAIL: $fail_n" | tee -a "$LOG"
if [ "$pass_n" -eq 15 ] && [ "$fail_n" -eq 0 ]; then
    echo "VERDICT: ALL 15 IMAGES PASS" | tee -a "$LOG"
    exit 0
fi
echo "VERDICT: FAILURE (see series logs)" | tee -a "$LOG"
exit 1

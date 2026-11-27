#!/usr/bin/env bash
# memcpy2_perf.sh — 对 memcpy2_neon 做事件级采样；事件按本机 perf list 实测探测，
# 不存在的事件（例如很多 TSV110 专属事件在通用 PMUv3 主机上不存在）打印为
# unavailable，绝不把不存在的 metric 当成"已测量"。
#
# 本机实测 ABSENT（2026-11-27 perf list，方案 §1.2，防止后人复用原建议的
# "五组 perf 命令"里的不存在事件）：
#   mem_stall_anyload / *_l1miss / *_l2miss, prf_req, hit_on_prf
#       —— Neoverse N2/V2 系 IMPDEF，TSV110/通用 PMUv3 无；
#   -M memory_bound / l1_bound / l2_bound / mem_bound / store_bound /
#   machine_clears / nuke_flush / ooo_flush / saveopq_stall / sync_stall
#       —— Intel TopDown 指标 / POWER 系事件名，非 ARM64 PMUv3 事件；
#   l1d_cache_inval / l2d_cache_inval —— 非 PMUv3 标准事件；
#   ld_spec / st_spec —— ARMv8 可选事件，本机通用 PMUv3 未实现（2026-11-27
#       perf stat 实证 Bad event name，本机只有 inst_spec/op_spec）；
#       920/TSV110 上应存在，现场探测为准。
# 目标机（920/4.19）perf list 与开发机不同，一切以现场探测为准。
#
# 用法:
#   scripts/memcpy2_perf.sh                 # 每组各起一个 10s 的 memcpy2_neon 采样
#   scripts/memcpy2_perf.sh -t 30           # 每组 30s
#   scripts/memcpy2_perf.sh -p <PID>        # 附着已有进程（PID 必须是实际 worker：
#                                           #   pgrep -n sdcshield 取最新子进程）
#
# 环境变量: BIN（默认 <repo>/builddir/sdcshield）、MODE、CPUS
set -u

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
if ! ROOT=$(git -C "$SCRIPT_DIR" rev-parse --show-toplevel 2>/dev/null); then
    ROOT=$(cd "$SCRIPT_DIR/.." && pwd)
fi
BIN=${BIN:-$ROOT/builddir/sdcshield}
MODE=${MODE:-all}
CPUS=${CPUS:-}
DUR=10
PID=""

while getopts "p:t:" opt; do
    case $opt in
        p) PID=$OPTARG ;;
        t) DUR=$OPTARG ;;
        *) echo "usage: $0 [-p PID] [-t SECONDS]" >&2; exit 2 ;;
    esac
done

command -v perf >/dev/null || { echo "perf not installed" >&2; exit 1; }

event_present() {
    perf list 2>/dev/null | awk -v e="$1" '
        $0 ~ ("(^|[ /])" e "(/| |$)") { found = 1 }
        END { exit(found ? 0 : 1) }'
}

pick() {   # pick EVENT... -> 只输出本机存在的事件（逗号连接）
    local e out=()
    for e in "$@"; do
        if event_present "$e"; then out+=("$e"); else echo "unavailable: $e" >&2; fi
    done
    [ ${#out[@]} -gt 0 ] && { local IFS=,; echo "${out[*]}"; }
}

run_group() {
    local name=$1 events=$2
    [ -z "$events" ] && return
    echo
    echo "== $name"
    if [ -n "$PID" ]; then
        perf stat -p "$PID" -I 1000 -e "$events" -- sleep "$DUR" 2>&1 | tail -20
    else
        # 每组独立起一个测试进程：直接由 perf 拉起可覆盖 fork 出的测试子进程。
        if [ -n "$CPUS" ]; then
            SDCSHIELD_MEMCPY2_MODE=$MODE perf stat -e "$events" -- \
                taskset -c "$CPUS" "$BIN" -e memcpy2_neon -t $((DUR * 1000)) 2>&1 | tail -20
        else
            SDCSHIELD_MEMCPY2_MODE=$MODE perf stat -e "$events" -- \
                "$BIN" -e memcpy2_neon -t $((DUR * 1000)) 2>&1 | tail -20
        fi
    fi
}

echo "target=${PID:-fresh memcpy2_neon per group} duration=${DUR}s mode=$MODE"
echo "(events filtered by perf list; unavailable ones are reported, not faked)"

g1=$(pick cycles instructions ld_spec st_spec mem_access l1d_cache l1d_cache_refill l2d_cache_refill ll_cache_miss)
# 组 2 拆 g2a/g2b（方案 P6）：每核 6 个通用计数器，9+ 事件会时分复用稀释；
# 两组各 ≤6。软件事件（faults/migrations）并入对应组，原组 3 取消。
g2a=$(pick dtlb_walk l1d_tlb_refill l2d_tlb_refill ttbr_write_retired exc_taken minor-faults)
g2b=$(pick mem_access remote_access memory_error context-switches cpu-migrations alignment-faults)

run_group "group 1: pipeline/L1-L3 (LSU 占空比 mem_access/inst_retired >=0.35)" "$g1"
run_group "group 2a: MMU/TLB walk + exception/minor-faults" "$g2a"
run_group "group 2b: mem_access/remote/memory_error + 软件事件" "$g2b"

# Uncore 计数器挂在 SCCL/HHA/L3C 上，按本机实际 PMU 名称取；
# 用户文档里常见的 back_invalid/retry_cpu/rw_chg 等 TSV110 事件在部分主机
# （含本机通用 PMUv3 配置）不存在，会显式报告 unavailable。
uncore=$(perf list 2>/dev/null \
         | sed -n 's#^ *\(hisi_sccl1_\(l3c\|hha\)[0-9]*/[a-z_0-9]*/\).*#\1#p' \
         | paste -sd, -)
echo
echo "== group 4: uncore L3C/HHA (global -a)"
paranoid=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo 0)
if [ -z "$uncore" ]; then
    echo "unavailable: no hisi_sccl*_l3c/hha uncore PMU on this host" >&2
elif [ "$(id -u)" -ne 0 ] && [ "$paranoid" -gt 1 ]; then
    # 非 root 且 paranoid>1 时 perf 只允许 user-scope，uncore 事件会显示
    # <not supported>；这是权限边界，不是计数器不存在。
    echo "skip: uncore needs root or perf_event_paranoid<=1 (paranoid=$paranoid, uid=$(id -u))" >&2
else
    perf stat -a -I 1000 -e "$uncore" -- sleep "$DUR" 2>&1 | tail -25
fi

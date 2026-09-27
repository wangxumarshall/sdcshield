#!/bin/bash
# d07_runner_hang.sh — runner 挂死（hang）演练（v5 §17.2 第 7 类）。
#
# 注入→观测链（全部用演练自有测试进程——绝不真停战役 sdcshield）：
#   ① 挂驻观测：SIGSTOP 自有 runner（bash sleep）→ /proc/<pid>/stat state=T
#      （hang 可检测——状态而非猜测）；
#   ② 驱动阶段超时语义（run_sdc 同款核心工具链）：`timeout --signal=TERM
#      --kill-after=Ns Pto cmd` 对 STOP 驻留进程 TERM 不响应（挂死）→
#      kill-after 到期 SIGKILL → rc=137（阶段边界判别口径：124/137/143），
#      且总耗时受界（不无限等待）；
#   ③ 恢复协议：SIGCONT 唤醒挂驻进程 → state 离开 T → TERM 干净退出
#      （rc=143 优雅路径，对照 137 强杀路径）——留驻进程由 trap EXIT 清理。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d07_runner_hang

proc_state() {  # proc_state <pid> → /proc stat 第 3 列（T=stopped R/S=running）
    awk '{print $3}' "/proc/$1/stat" 2>/dev/null || echo "gone"
}

# ---- 注入①：SIGSTOP 自有 runner → 挂驻可观测 ----
bash -c 'sleep 300' &
RUNNER=$!
reg_pid "$RUNNER"
sleep 0.3
kill -STOP "$RUNNER"
sleep 0.2
assert_eq "$(proc_state "$RUNNER")" "T" "SIGSTOP 后 runner state=T（挂驻可检测）"

# ---- 注入②：驱动阶段超时语义（run_sdc 同款 timeout 兜底，受界强杀） ----
# STOP 挂驻：TERM 对默认处置信号不免疫（终止挂驻进程），timeout 报 124；
# TERM 被忽略（trap ""）：kill-after 到期 SIGKILL 兜底 → 137。两者皆属驱动
# 阶段边界判别口径（TERM=143/KILL=137/timeout=124——run_sdc :116 注释）。
T0=$(date +%s)
set +e
timeout --signal=TERM --kill-after=2s 3s bash -c 'kill -STOP $$; sleep 60'
STOP_RC=$?
timeout --signal=TERM --kill-after=2s 3s bash -c 'trap "" TERM; sleep 60'
KILL_RC=$?
set -e
T1=$(date +%s)
ELAPSED=$((T1 - T0))
case "$STOP_RC" in 124|137|143) ;; *) fail "STOP 挂驻 rc=$STOP_RC 非阶段边界集 {124,137,143}";; esac
echo "  ✓ STOP 挂驻超时兜底 rc=$STOP_RC（阶段边界口径，受界强杀）"
case "$KILL_RC" in 124|137) ;; *) fail "TERM 忽略挂驻 rc=$KILL_RC 非阶段边界集 {124,137}";; esac
echo "  ✓ TERM 忽略挂驻 kill-after SIGKILL rc=$KILL_RC（KILL 兜底路径）"
[ "$ELAPSED" -le 20 ] || fail "两条超时兜底共耗时 ${ELAPSED}s > 20s——kill-after 未受界"
echo "  ✓ 超时兜底受界（${ELAPSED}s ≤ 20s——不无限等待）"

# ---- 注入③：恢复协议——SIGCONT 唤醒 + TERM 优雅退出 ----
kill -CONT "$RUNNER"
sleep 0.3
ST=$(proc_state "$RUNNER")
[ "$ST" != "T" ] && [ "$ST" != "gone" ] || fail "SIGCONT 后 runner 未恢复（state=$ST）"
echo "  ✓ SIGCONT 后 runner state=$ST（挂驻恢复）"
kill -TERM "$RUNNER" 2>/dev/null || :
set +e
wait "$RUNNER"
TRC=$?
set -e
assert_eq "$TRC" "143" "TERM 优雅退出 rc=143（对照 137 强杀——两种死亡口径均可审计）"
DRILL_PIDS=()   # 已收尸——EXIT 清理不再需要

drill_pass

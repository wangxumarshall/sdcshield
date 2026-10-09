#!/bin/bash
# excite.sh — SDC 7×24 战役一键总入口（最大程度激发 SDC 的策略入口）
#
# 把运维手册（docs/sdc-excite-reproduce/operations-runbook.md §1「眼睛先于负载」）
# 的首启序列固化为一条命令：
#   预检（fail-loud）→ 按需安装 systemd 单元 → 起 sdc-monitor（眼睛）→
#   冒烟门（L0 + 战役 smoke，不过不上线）→ 起 sdc-excite-reproduce（负载）→ 状态摘要
#
# 用法:
#   sudo bash scripts/sdc-excite-reproduce/excite.sh           # 一键上线 7×24 战役
#   bash  scripts/sdc-excite-reproduce/excite.sh --smoke       # 只预检+冒烟（不动 systemd，无需 root）
#   bash  scripts/sdc-excite-reproduce/excite.sh --status      # = status.sh（只读巡检）
#   sudo bash scripts/sdc-excite-reproduce/excite.sh --stop    # = stop.sh（保留断点）
#
# 设计边界（如实）:
#   - M2 事件栈（root-helper/eventd/controller/ring/采集器）不自动拉起——
#     其基线预置与 PMU 计数器注入是单板相关人工步骤（operations-runbook.md §1）。
#   - 冒烟门以当前用户跑（写 $HOME/sdc-excite-reproduce，一次性验证门），
#     与 systemd 服务用户的正式战役状态目录相互独立——这是刻意的（验证门
#     不污染正式断点 state.json）。
#   - 幂等：已安装/已活跃的服务自动跳过；state.json 断点保留，重复执行安全。
set -euo pipefail
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

usage() { sed -n '2,21p' "$0" | sed 's/^# \{0,1\}//'; }

MODE="run"
case "${1:-}" in
    --smoke)  MODE="smoke" ;;
    --status) exec bash "$SCRIPT_DIR/status.sh" ;;
    --stop)   exec bash "$SCRIPT_DIR/stop.sh" ;;
    -h|--help) usage; exit 0 ;;
    "") ;;
    *) echo "未知参数: $1（--smoke / --status / --stop，见 --help）" >&2; exit 1 ;;
esac

SUDO=""   # 调用处故意不加引号：为空时整词消失（引号式 "" 会按空命令名执行，rc=127）
ensure_root() { # ensure_root <需要 root 的动作描述>
    if [ "$(id -u)" -eq 0 ]; then return 0; fi
    if command -v sudo >/dev/null 2>&1; then SUDO="sudo"; return 0; fi
    echo "FATAL: $1 需要 root，且本机无 sudo。可用: su -c '$0 $*'" >&2
    exit 1
}

# ---------------- 1. 预检（fail-loud，全部通过才继续）----------------
source "$SCRIPT_DIR/sdc_common.sh"
ensure_bin   # BIN 缺失即 FATAL 退出（Task 3：仅真正执行 $BIN 的消费者显式调用）
ensure_dirs
command -v python3 >/dev/null 2>&1 || { echo "FATAL: 战役驱动需要 python3" >&2; exit 1; }
if [ "$MODE" = "run" ]; then
    command -v systemctl >/dev/null 2>&1 \
        || { echo "FATAL: run 模式需要 systemctl（systemd 机器）；冒烟验证可用 --smoke" >&2; exit 1; }
fi
echo "[preflight] sdcshield: $BIN"
# L0 冒烟门：zstd19 单线程回归（CLAUDE.md 规定的最小回归，约 2 秒）
if ! "$BIN" -e zstd19 -t 2000 -n 1 -Y -o /dev/null >/dev/null 2>&1; then
    echo "FATAL: L0 冒烟未过（zstd19 -t 2000 -n 1）——先修复构建再上线" >&2
    exit 1
fi
echo "[preflight] L0 冒烟: pass"

# ---------------- 2. 按需安装 systemd 单元（幂等）----------------
if [ "$MODE" = "run" ]; then
    if systemctl list-unit-files sdc-excite-reproduce.service 2>/dev/null \
            | grep -q '^sdc-excite-reproduce\.service'; then
        echo "[install] systemd 单元已就位，跳过"
    else
        ensure_root "安装 systemd 单元"
        echo "[install] systemd 单元缺失，执行 install.sh"
        $SUDO bash "$SCRIPT_DIR/install.sh"
    fi

    # ---------------- 3. 眼睛先于负载：起 sdc-monitor ----------------
    if $SUDO systemctl is-active --quiet sdc-monitor.service; then
        echo "[monitor] sdc-monitor 已活跃，跳过"
    else
        ensure_root "启动 sdc-monitor（联锁眼睛）"
        echo "[monitor] 启动 sdc-monitor（温度/内存/磁盘/SEL 联锁）"
        $SUDO systemctl start sdc-monitor.service
    fi
fi

# ---------------- 4. 冒烟门（战役 smoke，不过不上线）----------------
# 以当前用户跑（见头部"设计边界"）。全链路时长本机实测远超 NEW_BOARD_ONBOARDING.md
# 的 ~10 分钟估算（-T 预算只拦新用例，在飞用例须跑满 -t 才停）：门限 env 可覆盖，
# 默认 2400s——覆盖最慢 L5 变体（heatsoak 的 300s 预算 + 600s 单测上限）加余量。
SMOKE_TIMEOUT_S="${SMOKE_TIMEOUT_S:-2400}"
echo "[smoke] 战役冒烟（L1-L5 短时长单遍，L5 专项每次轮换一支分支，最长 $((SMOKE_TIMEOUT_S / 60)) 分钟）"
if ! timeout "$SMOKE_TIMEOUT_S" bash "$SCRIPT_DIR/sdc-excite-reproduce.sh" smoke; then
    echo "FATAL: 战役冒烟未过——拒绝上线 full（详见 $EXCITE_REPRODUCE_DIR/driver.log）" >&2
    exit 1
fi
echo "[smoke] pass"

if [ "$MODE" = "smoke" ]; then
    echo "冒烟模式完成（未启动 systemd 战役）。一键上线: sudo bash $0"
    exit 0
fi

# ---------------- 5. 上线战役（负载，断点续跑）----------------
if $SUDO systemctl is-active --quiet sdc-excite-reproduce.service; then
    echo "[campaign] sdc-excite-reproduce 已在运行（幂等跳过）"
else
    ensure_root "启动 sdc-excite-reproduce（7×24 战役）"
    echo "[campaign] 启动 sdc-excite-reproduce（L1-L5 状态机，断点续跑）"
    $SUDO systemctl start sdc-excite-reproduce.service
fi

# ---------------- 6. 状态摘要 + 指路 ----------------
sleep 3
bash "$SCRIPT_DIR/status.sh" || true
cat <<TIP

一键上线完成。后续:
  巡检         bash $SCRIPT_DIR/excite.sh --status
  停止(留断点)  sudo bash $SCRIPT_DIR/excite.sh --stop
  M2 事件栈    docs/sdc-excite-reproduce/operations-runbook.md §1（单板相关，按手册拉起）
  运维手册     docs/sdc-excite-reproduce/operations-runbook.md
TIP

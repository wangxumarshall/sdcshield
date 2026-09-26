#!/bin/bash
# m2_controller_baseline.sh — controller 消费基线预置（M2 T6 部署步骤，v5 §8）
#
# 为什么需要：sdc-eventd 首启把五源全部历史回填 spool/events.jsonl（事件档案，
# 设计内——真实历史源尾随产出，全史可 replay）；sdc-controller 首启 offset=0
# 会把整段历史喂进状态机。本机实测（2026-09-26）：193 条历史事件含
# 2×sdc_mismatch(red) + 25×interlock PAUSE/KILL(black)，green→red→black 终态，
# 并对陈旧事件分派 snapshot/verify 请求（历史若含 orange 离散断言还会经
# root-helper 真触发 perf burst）——陈旧事件驱动特权动作是安全事故。
#
# 部署基线（本脚本）：state=green + events.jsonl 消费 offset 预置到回填末尾，
# controller 只消费部署时刻之后的实时事件。events.jsonl 本身不删不改——
# 历史完整保留在事件档案里。
#
# offset 单位裁定：tail_file（T1 单一实现，eventd/controller/ring 共用）以
# 二进制模式读文件 + 原始字节上 rfind（58153793 起），offset 是**字节**偏移
# ——生产实证（2026-09-26）：164073B 档案全量消费 offset=164073==文件字节数。
# 本脚本必须复用 tail_file 本身取末偏移（同一实现同一单位），绝不自行按
# 字符数/行数算——文本模式字符偏移每轮缩水多字节开销，controller 会重读
# 已消费行 + 断行合并（58153793 修复前的重复消费缺陷）。
#
# 时机：eventd 完成首轮回填之后、controller 首次启动之前（install.sh 只 enable
# 不 start，即安装与首次 start 之间；eventd 启动后等 ≥1 轮 2s 轮询即可）。
# 幂等语义：state 文件已存在时拒绝（防覆盖既有进度）——确要重置基线用 --force
# （语义=基线重置到现在，controller 必须先停）。
# 用法: bash m2_controller_baseline.sh [--force] [数据根]   # 数据根缺省 ~/sdc-excite-reproduce
set -euo pipefail
FORCE=0
if [ "${1:-}" = "--force" ]; then FORCE=1; shift; fi
ROOT="${1:-${SDC_EXCITE_REPRODUCE_DIR:-$HOME/sdc-excite-reproduce}}"
SPOOL="$ROOT/spool"
EVENTS="$SPOOL/events.jsonl"
STATE="$SPOOL/controller_state.json"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"

if systemctl is-active --quiet sdc-controller.service 2>/dev/null; then
    echo "✗ sdc-controller 运行中——基线预置须在其停止时做（运行中重置会跳过未消费事件）" >&2
    exit 1
fi
if [ -f "$STATE" ] && [ "$FORCE" -ne 1 ]; then
    echo "✗ $STATE 已存在（controller 已有进度）——重置基线用 --force" >&2
    exit 1
fi
[ -f "$EVENTS" ] || { echo "✗ $EVENTS 不存在——eventd 尚未回填（先启动 sdc-eventd 并等 ≥1 轮）" >&2; exit 1; }

python3 - "$EVENTS" "$STATE" "$REPO" <<'EOF'
import json, os, sys
events, state, repo = sys.argv[1], sys.argv[2], sys.argv[3]
sys.path.insert(0, os.path.join(repo, "tools", "telemetry"))
from sdc_eventd import tail_file            # T1 单一实现：同一 offset 单位（字节）
lines, store = tail_file(events, {})        # 空起点全量消费 → 末偏移即回填末尾
off = store[str(events)]
st = {"state": "green", "offsets": {os.path.abspath(events): off}}
tmp = state + ".tmp"
with open(tmp, "w", encoding="utf-8") as f:
    json.dump(st, f, ensure_ascii=False, indent=1)
os.replace(tmp, state)                      # 原子写（controller_state 同款纪律）
print(f"[baseline] 消费全量 {len(lines)} 行取末偏移 offset={off} → {state} (state=green)")
EOF
echo "基线就绪——可启动 sdc-controller"

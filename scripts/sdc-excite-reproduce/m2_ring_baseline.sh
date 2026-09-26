#!/bin/bash
# m2_ring_baseline.sh — ring 消费基线预置（M2 终审 must-fix，配套 m2_controller_baseline.sh）
#
# 为什么需要：sdc-ring 若无基线直接 enable --now，首启 events.jsonl 消费
# offset=0 会重扫全部历史事件（本机 2026-09-26 实测 193 条，含 2×sdc_mismatch(red)
# + 25×interlock PAUSE/KILL(black)）——重扫护栏（frozen 集合 + ring_window/ 文件
# 存在性）只能拦"已有固化证据"的覆盖，拦不住首次固化：当前 120s 热窗对历史事件
# 是时间错位数据，冒充 [-60s,+60s] 固化窗口即取证证据污染（controller 账本记
# freeze_ring 而无人执行是账实不符；ring 常驻后证据必须真）。
#
# 部署基线（本脚本）：events.jsonl 消费 offset 预置到档案现末尾（复用 tail_file
# 单一实现，字节单位——58153793 后二进制读），ring 只消费基线之后的新事件；
# 同时预置 ring_frozen.json 空集。**只预置 offset，不固化任何历史事件**——
# 历史事件的窗口数据早已流逝，诚实做法是基线后新事件才固化（时间错位的
# "证据"比没有证据更糟）。events.jsonl 本身不删不改——历史完整保留在事件档案。
# 窗口无需预置：RingWindow 是进程内存态，sdc-ring 首轮自动回放两 CSV 文件尾
# ≤1MB 暖窗（重启/首启同路径）。
#
# 时机：eventd 完成首轮回填之后、sdc-ring 首次启动之前（install.sh 只 enable
# 不 start；与 m2_controller_baseline.sh 同窗口执行，各管各的状态文件）。
# 幂等语义：ring_offset.json 已存在时拒绝（防覆盖既有进度）——确要重置基线用
# --force（语义=基线重置到现在，sdc-ring 必须先停；既有 ring_frozen.json 固化
# 集不覆盖——护栏只增不减）。
# 用法: bash m2_ring_baseline.sh [--force] [数据根]   # 数据根缺省 ~/sdc-excite-reproduce
set -euo pipefail
FORCE=0
if [ "${1:-}" = "--force" ]; then FORCE=1; shift; fi
ROOT="${1:-${SDC_EXCITE_REPRODUCE_DIR:-$HOME/sdc-excite-reproduce}}"
SPOOL="$ROOT/spool"
EVENTS="$SPOOL/events.jsonl"
OFFSET="$SPOOL/ring_offset.json"
FROZEN="$SPOOL/ring_frozen.json"
REPO="$(cd "$(dirname "$0")/../.." && pwd)"

if systemctl is-active --quiet sdc-ring.service 2>/dev/null; then
    echo "✗ sdc-ring 运行中——基线预置须在其停止时做（运行中重置会跳过未消费事件）" >&2
    exit 1
fi
if [ -f "$OFFSET" ] && [ "$FORCE" -ne 1 ]; then
    echo "✗ $OFFSET 已存在（ring 已有进度）——重置基线用 --force" >&2
    exit 1
fi
[ -f "$EVENTS" ] || { echo "✗ $EVENTS 不存在——eventd 尚未回填（先启动 sdc-eventd 并等 ≥1 轮）" >&2; exit 1; }
if [ -f "$FROZEN" ] && [ ! -f "$OFFSET" ]; then
    echo "⚠ $FROZEN 已存在而 $OFFSET 不存在——ring 曾运行过，本次基线只前移 offset，固化集保留" >&2
fi

python3 - "$EVENTS" "$SPOOL" "$REPO" <<'EOF'
import json, os, sys
events, spool, repo = sys.argv[1], sys.argv[2], sys.argv[3]
sys.path.insert(0, os.path.join(repo, "tools", "telemetry"))
from sdc_eventd import tail_file            # T1 单一实现：同一 offset 单位（字节）
lines, store = tail_file(events, {})        # 空起点全量消费 → 末偏移即档案末尾
off = store[str(events)]
def atomic(path, obj):                      # 原子写（ring _atomic_json 同款纪律）
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(obj, f, ensure_ascii=False, indent=1)
    os.replace(tmp, path)
atomic(os.path.join(spool, "ring_offset.json"), {"events_offset": off})
frozen = os.path.join(spool, "ring_frozen.json")
had = os.path.exists(frozen)
if not had:                                # 既有固化集不覆盖（重扫护栏只增不减）
    atomic(frozen, [])
msg = "；ring_frozen.json 预置空集" if not had else "；既有 ring_frozen.json 保留"
print(f"[ring-baseline] 消费全量 {len(lines)} 行取末偏移 offset={off} → ring_offset.json{msg}（不固化历史事件）")
EOF
echo "基线就绪——可启动 sdc-ring"

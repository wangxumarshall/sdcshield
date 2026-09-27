#!/bin/bash
# d12_clock_jump.sh — 时间跳变（clock jump）演练（v5 §17.2 第 12 类）。
#
# 注入→观测链（sdc_time 真实五元时间契约——函数级，真锚点实采）：
#   ① 正常锚定基线：实采两个锚点（间隔 2s）→ 锚定隐含速率（Δcntvct/
#      Δrealtime）与名义 CNTFRQ 偏差 <1%（无跳变——真实时钟健康）；
#   ② 跳变注入：第二锚点 realtime_ns 阶跃 +600s（NTP step/ RTC 回拨同构，
#      cntvct 连续不受影响）→ 隐含速率塌缩（跨度虚增 300 倍）→ 残差
#      检出（隐含速率偏差 >50%——锚定残差检测口径）；
#   ③ 误差界不吞跳变：interpolate 对跳变锚点对的越界估计误差界 ≥ 跳变
#      跨度（600s——误差界诚实含整个虚增跨度，不假装包络精度）；
#      mapping_error_ns ≥0（真实残差非负哨兵口径）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d12_clock_jump

python3 - "$REPO" > "$DRILL_ROOT/d12.out" 2>&1 <<'PYEOF'
import sys, time
sys.path.insert(0, sys.argv[1] + "/tools/telemetry")
import sdc_time

def implied_rate_hz(a, b):
    """锚定隐含速率：Δcntvct / Δrealtime（秒）——名义上应 ≈ cntfrq_hz。"""
    return (b["cntvct"] - a["cntvct"]) / ((b["realtime_ns"] - a["realtime_ns"]) / 1e9)

def dev(a, b):
    return abs(implied_rate_hz(a, b) - a["cntfrq_hz"]) / a["cntfrq_hz"]

checks = []
def check(name, ok, detail):
    checks.append(ok)
    print(f"  ✓ {name}（{detail}）" if ok else f"  ✗ {name}（{detail}）")
    if not ok:
        raise SystemExit(1)

# ① 正常锚定基线（实采两锚点，间隔 2s）
a = sdc_time.anchor()
time.sleep(2)
b = sdc_time.anchor()
dev_ok = dev(a, b)
check("正常锚定隐含速率偏差 <1%", dev_ok < 0.01, f"dev={dev_ok:.6f}")
check("真实锚点 mapping_error_ns 非负", a["mapping_error_ns"] >= 0 and b["mapping_error_ns"] >= 0,
      f"a={a['mapping_error_ns']}ns b={b['mapping_error_ns']}ns")

# ② 跳变注入：第二锚点 realtime 阶跃 +600s（cntvct 连续）
jump = dict(b, realtime_ns=b["realtime_ns"] + 600 * 10**9)
dev_jump = dev(a, jump)
check("跳变锚点隐含速率偏差 >50%（残差检出）", dev_jump > 0.5, f"dev={dev_jump:.4f}")
check("跳变检出判据单调：跳变对偏差 ≫ 正常对偏差", dev_jump > 50 * dev_ok,
      f"{dev_jump:.4f} vs {dev_ok:.6f}")

# ③ 误差界不吞跳变（越界外推误差 ≥ 虚增跨度）
_, err_ok = sdc_time.interpolate([a, b], a["realtime_ns"] + 10**9)          # 正常对内插
_, err_jump = sdc_time.interpolate([a, jump], a["realtime_ns"] + 10**9)     # 跳变对
check("跳变锚点对插值误差界 ≥600s（虚增跨度全额入界）", err_jump >= 600 * 10**9,
      f"err={err_jump/1e9:.1f}s vs 正常对 err={err_ok/1e9:.3f}s")

print(f"ANCHOR_SPAN_OK={a['cntfrq_hz']}")
PYEOF
cat "$DRILL_ROOT/d12.out"

# 断言（外层复核）：五项检查全过 + 真实 CNTFRQ 回显（锚点实采证据）
D12OK=$(grep -c "^  ✓" "$DRILL_ROOT/d12.out")
[ "$D12OK" -eq 5 ] || fail "d12 内部检查未全过（${D12OK}/5）"
echo "  ✓ 五项时间锚定检查全过（正常偏差/残差非负/跳变检出/单调判据/误差界不吞）"
grep -q "ANCHOR_SPAN_OK=" "$DRILL_ROOT/d12.out" || fail "无 CNTFRQ 回显（锚点实采证据缺失）"
echo "  ✓ 真实锚点实采在案（$(grep ANCHOR_SPAN_OK "$DRILL_ROOT/d12.out")）"

drill_pass

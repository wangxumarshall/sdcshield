#!/usr/bin/env bash
# droop_mod 编译 + 功能自测。非 aarch64 → SKIP（exit 0）。
# 用法: bash tools/droop-excite/tests/test_droop_mod.sh
set -euo pipefail
cd "$(dirname "$0")/.."
ARCH="$(uname -m)"
if [ "$ARCH" != "aarch64" ]; then
    echo "SKIP: droop_mod is aarch64-only (host=$ARCH)"
    exit 0
fi
gcc -O2 -Wall -Wextra -Werror -std=c17 -pthread -lm -o droop_mod droop_mod.c
echo "== 1/5 self-test =="
./droop_mod --self-test
echo "== 2/5 calibrate =="
./droop_mod --calibrate --payload dep --off-mode stall > /tmp/dm_cal.json
cat /tmp/dm_cal.json
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_cal.json"))
assert d["cyc_per_iter_on"] > 0 and d["cyc_per_iter_off"] > 0, d
assert 1 < d["cyc_per_iter_on"] < 100, d      # dep 每 iter ~4-10 周期
print("calibration sane")
EOF
echo "== 3/5 高频门控（100MHz 目标，2s，2 核） =="
./droop_mod --cores 0-1 --threads 2 --duration 2 --freq-hz 100e6 --duty 0.5 \
    --json-summary /tmp/dm_gate.json
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_gate.json"))
assert d["threads"] == 2, d
assert d["checksum_failures"] == 0, d
f = d["freq_measured_hz_mean"]
assert 4e7 < f < 1.5e8, f      # 直射档=冒烟：下界 -60% 容差（on_iters=3 每周期固定开销
print("gate ok, f_meas=%.4g" % f)  # 使实测约 44MHz）；高频精度由 run_sweep 的 refine_iters 闭环（Task 3）；顶盖由 4/5 段证明
EOF
echo "== 4/5 最小 iters 顶盖（1/1 迭代，硬上限） =="
./droop_mod --cores 0 --duration 2 --on-iters 1 --off-iters 1 \
    --json-summary /tmp/dm_ceil.json
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_ceil.json"))
f = d["freq_measured_hz_mean"]
assert f >= 1.5e8, f     # 50-150MHz 频段可达的规格级断言：1/1 迭代为硬顶
print("ceiling ok, f_meas=%.4g" % f)  # （本机实测 ~228MHz，阈值留余量）
EOF
echo "== 5/5 低频门控（0.5Hz 目标）+ SIGTERM 路径 =="
./droop_mod --cores 0 --duration 10 --freq-hz 0.5 --duty 0.5 \
    --json-summary /tmp/dm_gate2.json &
P=$!
sleep 2
kill -TERM $P
wait $P
python3 - <<'EOF'
import json
d = json.load(open("/tmp/dm_gate2.json"))
assert d["stopped_by_signal"] == "true", d
assert d["checksum"] == "off", d   # 低频大相位校验自动关闭（防软件参考拖垮门控）
print("sigterm ok")
EOF
echo "ALL PASS"

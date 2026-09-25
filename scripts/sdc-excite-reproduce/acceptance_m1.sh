#!/bin/bash
# acceptance_m1.sh — M1 退出标准核对（v5 §14.1：24h 采集无关键丢样 + 质量字段可见 + 开销达标）
# 用法: bash acceptance_m1.sh <运行小时数，默认 24>   # 冒烟口径传 0.5，正式门传 24
# 六项门：① 三采集器 active ② 丢样<0.1% ③ percore 新鲜度 ④ PMU percent_covered 可见
#         ⑤ 运行时长（容差 2%）⑥ A/B 常态开销
# ⑥ 为 T2 裁定的双档协议：复测 median 下降 ≤3% PASS / 3%-8% WARN（受环境噪声限制，
# 需静默窗口复测——如实降级路径）/ >8% FAIL；输出复测 median±MAD；跑前查静默窗口
# （pgrep sdcshield 非 0 → 记警告不中断）。锚点与复测 t-ms 不同（10s vs 30s）→
# 按 loops/s 归一后比较，直接比 loop_total 不可比。
# 退出码：0 = PASS（含 WARN 时输出明确标注，WARN 项需静默窗口复测跟进）；1 = FAIL。
set -u
cd "$(dirname "$0")/../.."                              # 仓库根（ab_baseline 相对路径）
HOURS="${1:-24}"
ROOT="${SDC_EXCITE_REPRODUCE_DIR:-/home/sdc/sdc-excite-reproduce}"
MON="$ROOT/monitor"
FAIL=0; WARNS=0
need() { echo "✗ $1"; FAIL=1; }
ok()   { echo "✓ $1"; }
warn() { echo "⚠ $1"; WARNS=$((WARNS+1)); }
have() { [ -s "$1" ] || { need "缺 $1"; return 1; }; }

# 1) 三采集器 active
for c in percore pmu ras; do
    [ "$(systemctl is-active sdc-collector@$c)" = active ] && ok "sdc-collector@$c active" \
        || need "sdc-collector@$c 非 active"
done

# 2) 丢样：selfmon 里 samples_dropped 合计为 0（或占 samples_total <0.1%）
if have "$MON/collector_self.csv"; then
python3 - "$MON/collector_self.csv" <<'EOF' && ok "丢样 <0.1%" || need "丢样超标"
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
tot = sum(int(r["samples_total"]) for r in rows)
dropped = sum(int(r["samples_dropped"]) for r in rows)
sys.exit(0 if tot and dropped / tot < 0.001 else 1)
EOF
fi

# 3) 数据新鲜度：percore.csv 末行 ts 距今 < 2×周期（60s 周期 → 180s）
if have "$MON/percore.csv"; then
LAST=$(tail -1 "$MON/percore.csv" | cut -d, -f1)
if LAST_TS=$(date -d "$LAST" +%s 2>/dev/null); then
    AGE=$(( $(date +%s) - LAST_TS ))
    [ "$AGE" -lt 180 ] && ok "percore 新鲜 (${AGE}s)" || need "percore 停更 ${AGE}s"
else
    need "percore.csv 末行 ts 不可解析: '${LAST}'"
fi
fi

# 4) PMU 质量字段可见：pmu_core.csv 列头含 percent_covered（multiplex 诚实呈现）
if have "$MON/pmu_core.csv"; then
head -1 "$MON/pmu_core.csv" | grep -q percent_covered && ok "PMU 质量字段可见" \
    || need "pmu_core.csv 缺 percent_covered"
fi

# 5) 时长门：运行满 HOURS 小时（collector_self.csv 最早最晚 ts 差，容差 2%）
if have "$MON/collector_self.csv"; then
python3 - "$MON/collector_self.csv" "$HOURS" <<'EOF' && ok "运行 ≥${HOURS}h" || need "运行时长不足（或冒烟口径传 0.5 再验）"
import csv, sys, datetime
rows = list(csv.DictReader(open(sys.argv[1])))
t0 = datetime.datetime.strptime(rows[0]["ts"], "%Y-%m-%d %H:%M:%S")
t1 = datetime.datetime.strptime(rows[-1]["ts"], "%Y-%m-%d %H:%M:%S")
sys.exit(0 if (t1 - t0).total_seconds() >= float(sys.argv[2]) * 3600 * 0.98 else 1)
EOF
fi

# 6) A/B 常态开销复测（双档协议）——锚点=采集器全停基线（Task 2），本测=采集器常态运行
NS=$(pgrep -c sdcshield 2>/dev/null || true)
if [ -n "$NS" ] && [ "$NS" != "0" ]; then
    warn "静默窗口不净: pgrep sdcshield=$NS（共享机器噪声，如实记录不中断）"
fi
if python3 tools/telemetry/ab_baseline.py --runs 15 --t-ms 30000 --out /tmp/ab_m1.json; then
python3 - "$ROOT/monitor/ab_baseline.json" /tmp/ab_m1.json <<'EOF'
import json, statistics, sys
A, B = (json.load(open(p)) for p in sys.argv[1:3])
if A["config"]["n"] != B["config"]["n"]:
    print(f"A/B 配置不一致: 锚点 n={A['config']['n']} vs 复测 n={B['config']['n']}（不可比）")
    sys.exit(4)
def rates(doc):                                    # loop_total → loops/s（跨 t-ms 归一）
    t = doc["config"]["t_ms"] / 1000.0
    return [r["loop_total"] / t for r in doc["runs"]]
ra, rb = rates(A), rates(B)
ma, mb = statistics.median(ra), statistics.median(rb)
mada = statistics.median([abs(x - ma) for x in ra])
madb = statistics.median([abs(x - mb) for x in rb])
drop = (ma - mb) / ma * 100
print(f"锚点  median {ma:.2f} loops/s (MAD ±{mada:.2f} = {mada/ma*100:.1f}%, runs={len(ra)}, t_ms={A['config']['t_ms']})")
print(f"复测  median {mb:.2f} loops/s (MAD ±{madb:.2f} = {madb/mb*100:.1f}%, runs={len(rb)}, t_ms={B['config']['t_ms']})")
print(f"常态开销: median 下降 {drop:+.2f}%")
if drop <= 3.0:
    print("A/B 判定: PASS（下降 ≤3%）"); sys.exit(0)
if drop <= 8.0:
    print("A/B 判定: WARN（下降 3%-8%）——受环境噪声限制，需静默窗口复测"); sys.exit(3)
print("A/B 判定: FAIL（下降 >8%）——查采集器 CPU 占用"); sys.exit(4)
EOF
case $? in
    0) ok "A/B 常态开销 ≤3%（PASS）";;
    3) warn "A/B 常态开销 3%-8%（WARN）——受环境噪声限制，需静默窗口复测";;
    *) need "A/B 常态开销 >8% 或复测异常（FAIL）——查采集器 CPU 占用/锚点文件";;
esac
else
    need "A/B 复测执行失败（ab_baseline.py 非 0 退出）"
fi

if [ "$FAIL" = 0 ] && [ "$WARNS" = 0 ]; then
    echo "M1 验收 PASS"; exit 0
elif [ "$FAIL" = 0 ]; then
    echo "M1 验收 PASS（含 ${WARNS} 项 WARN——见上，WARN 项需静默窗口复测后闭环）"; exit 0
else
    echo "M1 验收 FAIL"; exit 1
fi

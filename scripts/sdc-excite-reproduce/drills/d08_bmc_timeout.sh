#!/bin/bash
# d08_bmc_timeout.sh — BMC 通道超时/失联演练（v5 §17.2 第 8 类）。
#
# 注入→观测链（隔离根内假 ipmitool——全程零真 BMC 接触）：
#   ① 注入：MON_SDR_FILE 指空文件（非空才用文件——空文件走 ipmitool 路径）
#      + PATH 前置假 ipmitool（模拟超时/失联：立即失败）→ monitor
#      MON_SELFTEST 一个完整采样周期；
#   ② 观测：monitor.csv bmc_ok 列=0（通道降级可见）+ OS 侧采样不断链
#      （disk_pct/oom_kill/mc_ce_total 等固定尾列照常——acpitz/meminfo/df
#      降级路径，v5 §6.7）+ 假 ipmitool 调用标记在案（证明零真 BMC 接触）；
#   ③ 事件分级：单周期降级不告警（bmc_fail=10 才告警——as-built 守卫）→
#      sdc_eventd --once 零新事件（降级不产生假事件/假联锁）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d08_bmc_timeout

# ---- 注入①：空 SDR 文件 + 假 ipmitool（PATH 前置——绝不打真 BMC） ----
FAKEBIN="$DRILL_ROOT/fakebin"
mkdir -p "$FAKEBIN"
: > "$DRILL_ROOT/empty_sdr.txt"   # 空文件：fetch_sdr 的 -s 判空 → 走 ipmitool 路径
cat > "$FAKEBIN/ipmitool" <<EOF
#!/bin/bash
# d08 假 ipmitool——模拟 BMC 超时/失联（立即失败，绝不触达真实设备）
echo "\$(date '+%F %T') invoked: \$*" >> "$DRILL_ROOT/fake_ipmitool_calls.log"
exit 1
EOF
chmod +x "$FAKEBIN/ipmitool"

env -u SDC_ROOT_PW MON_SELFTEST=1 MON_SDR_FILE="$DRILL_ROOT/empty_sdr.txt" \
    SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT" \
    PATH="$FAKEBIN:$PATH" \
    bash "$REPO/scripts/sdc-excite-reproduce/sdc_monitor.sh" \
    > "$DRILL_ROOT/monitor_run.out" 2>&1

# 断言②-a：全程零真 BMC 接触（假 ipmitool 被调用且留痕——PATH 前置生效）
assert_contains "$DRILL_ROOT/fake_ipmitool_calls.log" "sdr" \
    "假 ipmitool 被调用（标记在案）——零真 BMC 接触"

# 断言②-b：bmc_ok=0 列（通道降级可见）
BMC=$(python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
hdr = rows[0]
print(rows[1][hdr.index("bmc_ok")])' "$DRILL_ROOT/monitor/monitor.csv")
assert_eq "$BMC" "0" "monitor.csv bmc_ok 列=0（BMC 通道降级可见）"

# 断言②-c：OS 侧采样不断链（降级路径——df/vmstat/EDAC 固定尾列照常非空）
python3 -c '
import csv, sys
rows = list(csv.reader(open(sys.argv[1])))
hdr, row = rows[0], rows[1]
for col in ("disk_pct", "oom_kill", "pgmajfault"):
    assert row[hdr.index(col)] != "", f"OS 侧列 {col} 断链（空值）"
' "$DRILL_ROOT/monitor/monitor.csv" || fail "OS 侧采样断链（降级路径破坏）"
echo "  ✓ OS 侧采样不断链（disk_pct/oom_kill/pgmajfault 非空——降级不弃测）"

# ---- 断言③：单周期降级零新事件（bmc_fail=10 才告警——不产生假事件/假联锁） ----
OUT=$(run_eventd)
echo "$OUT" | grep -q "0 new events" || fail "BMC 单周期降级不应产生事件: $OUT"
echo "  ✓ 单周期降级零新事件（bmc_fail=10 告警线未到——降级入列不入流）"

drill_pass

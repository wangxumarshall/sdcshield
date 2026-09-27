#!/bin/bash
# d13_restore_fail.sh — restore 失败（读回不匹配）演练（v5 §17.2 第 13 类）。
#
# 注入→观测链（sdc_root_helper 真实 execute+审计路径——sysfs 注入点全 mock，
# 绝不真写 /sys：模块头"注入点（测试全 mock，绝不真 offline"纪律）：
#   ① 前置快照落位（spool/pre_state_online.txt=0-7 全集——restore 目标）；
#   ② 读回不匹配注入：mock _write_cpu_online 为 no-op、_read_cpu_attr("online")
#      恒返 0-3（写不生效=内核拒改/外部扰动同构）→ 真实 execute("cpu_online")
#      → status=readback_mismatch（读回 online 与期望不符——不谎报 done）；
#   ③ restore 失败：同 mock 下 execute("restore") → 目标 0-7 vs 读回 0-3 →
#      status=readback_mismatch（"restore 读回 ≠ 前置快照"）；
#   ④ 审计断言：root_helper_audit.jsonl 两行均 readback_mismatch、action/
#      reason/old/new/readback 字段齐——恢复失败可审计（人工收敛入口）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d13_restore_fail

# 前置快照（restore 目标=0-7 全集；hotplug 前置快照"已存在不覆盖"——
# 先落位使 cpu_online 的快照不覆盖本目标）
printf '0-7\n' > "$DRILL_ROOT/spool/pre_state_online.txt"

python3 - "$REPO" "$DRILL_ROOT" <<'PYEOF'
import sys
sys.path.insert(0, sys.argv[1] + "/tools/control")
import sdc_root_helper as rh

# sysfs 注入点全 mock（模块头纪律：绝不真写 /sys；possible 含演练 cpu 集）
rh._read_cpu_attr = lambda attr: {"possible": "0-7", "online": "0-3"}[attr]
rh._write_cpu_online = lambda cpu, up: None        # no-op：写不生效（读回不变）
rh._sdcshield_running = lambda: False              # 战役在跑——真实探测会拒，mock 隔离

root = sys.argv[2]
r1 = rh.execute("cpu_online", {"cpus": [5]}, root, action_id="d13hotplug1")
r2 = rh.execute("restore", {}, root, action_id="d13restore1")
assert r1["status"] == "readback_mismatch", r1
assert "读回 online" in r1["reason"] and "期望不符" in r1["reason"], r1
assert r2["status"] == "readback_mismatch", r2
assert "restore 读回" in r2["reason"] and "前置快照" in r2["reason"], r2
assert r2["old"] == "0-3" and r2["readback"] == "0-3", r2
for r in (r1, r2):
    print(f"  ✓ execute {r['action']} → status={r['status']}（reason={r['reason']}）")
PYEOF

# 断言：审计行落位（真实 _audit 路径写入隔离根 spool/）
AUDIT="$DRILL_ROOT/spool/root_helper_audit.jsonl"
jlassert "$AUDIT" \
    'len(d)==2 and all(r["status"]=="readback_mismatch" for r in d)
    and d[0]["action"]=="cpu_online" and d[1]["action"]=="restore"
    and all(r["old"] and r["new"] and r["readback"] for r in d)' \
    "审计两行均 readback_mismatch（cpu_online+restore——恢复失败可审计）"
assert_contains "$AUDIT" "d13hotplug1" "审计行含 hotplug action_id（可追溯）"
assert_contains "$AUDIT" "d13restore1" "审计行含 restore action_id（可追溯）"
assert_not_contains "$AUDIT" '"status": "done"' "无 done 谎报（读回不符绝不静默成功）"

drill_pass

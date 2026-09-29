#!/bin/bash
# d15_kdump_probe.sh — kdump 只读探针演练（M5 终审 Minor #2：v5 §17.2 "panic/
# kdump" 中 kdump 半项此前无演练——d06 覆盖 panic，本类补 kdump 就绪性只读核验）。
#
# 探针三项（全部只读——**绝不触发 kdump、绝不 panic、绝不写 /var/crash 内容**）：
#   ① kdumpctl status 输出含 "operational"：kdump 服务就绪；
#   ② /sys/kernel/kmsg 或 /proc/cmdline 含 crashkernel= ：崩溃内核内存预留就绪；
#   ③ /var/crash 存在且可写：转储落盘位就绪。
#
# 安全边界（as-built 关键裁定）：
#   - **真 kdumpctl 非只读**（status 需 root 且会写 /var/lock/kdump——本机实测
#     sdc 用户下即因写锁失败 "Permission denied / Create file lock failed"）——
#     故演练 PATH 前置**假 kdumpctl**（d08 假 ipmitool 同模式）：只回显 status
#     文本，绝不起/停 kdump、绝不写锁、零真服务接触；
#   - 探针路径经环境变量可重定向（KD_CMDLINE/KD_KMSG/KD_CRASH_DIR，缺省即真实
#     /proc/cmdline、/sys/kernel/kmsg、/var/crash）——演练用隔离根 fixture，
#     绝不写真实 /var/crash；
#   - 可写性判定用 `[ -w ]`（access(2) 只读系统调用，**不创建任何文件**）；
#   - 真实系统观测（读 /proc/cmdline、ls /var/crash）为纯读，演练前后对
#     /var/crash 内容快照比对（零接触实证）。
#
# 场景（合成——探针逻辑的真值断言；探针本体不带 KD_* 即对真实系统只读可用）：
#   healthy ：kdumpctl operational + cmdline 含 crashkernel= + 可写转储位 → 三 ✓
#   kmsg    ：cmdline 无 crashkernel 而 /sys/kernel/kmsg 有 → ② 命中 OR 第二支
#   degraded：kdumpctl not operational + 两处皆无 crashkernel + 转储位不可写
#             → 三 FAIL（负向断言：探针非 no-op，M5 §6 占位诚实同精神）
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d15_kdump_probe

FAKEBIN="$DRILL_ROOT/fakebin"
FIX="$DRILL_ROOT/fixtures"
EVID="$DRILL_ROOT/monitor"
mkdir -p "$FAKEBIN" "$FIX" "$EVID"

# ---- 假 kdumpctl（只读 status 回显；文本取自 KDUMP_PROBE_FAKE_STATUS） ----
cat > "$FAKEBIN/kdumpctl" <<'EOF'
#!/bin/bash
# d15 假 kdumpctl——仅支持只读 status（绝不 start/restart/reload、绝不写锁）
[ "${1:-}" = status ] || { echo "d15 假 kdumpctl：仅支持只读 status" >&2; exit 1; }
echo "kdump: Kdump is ${KDUMP_PROBE_FAKE_STATUS:-operational}"
EOF
chmod +x "$FAKEBIN/kdumpctl"
export PATH="$FAKEBIN:$PATH"   # 假 kdumpctl 前置（真机 kdumpctl 非只读，绝不调用）

# ---- 真机 /var/crash 只读快照（零接触实证基线） ----
crash_snapshot() {
    if [ -d /var/crash ]; then find /var/crash -mindepth 1 2>/dev/null | sort
    else echo "(absent)"; fi
}
PRE_CRASH="$(crash_snapshot)"

# ---- fixtures ----
echo "BOOT_IMAGE=/vmlinuz root=/dev/mapper/x ro crashkernel=1024M,high quiet" \
    > "$FIX/cmdline_ok"
echo "BOOT_IMAGE=/vmlinuz root=/dev/mapper/x ro quiet" > "$FIX/cmdline_nok"
echo "Kernel command line: ... crashkernel=1024M,high ..." > "$FIX/kmsg_ok"
mkdir -p "$FIX/crash_ok" "$FIX/crash_ro"
chmod 500 "$FIX/crash_ro"      # 不可写转储位（degraded 场景）

# ---------------------------------------------------------------------------
# 探针本体：三只读判定 → "CHECK <名>=PASS|FAIL" 逐行落盘；全过 rc=0，否则 rc=1
probe() {  # probe <输出文件> <kdumpctl 状态文本>
    local out="$1" st="$2" rc=0
    : > "$out"
    # ① kdumpctl status 含 "operational"（且非 "not operational"）
    local kc stx; kc="$(command -v kdumpctl || true)"
    stx="$(KDUMP_PROBE_FAKE_STATUS="$st" "$kc" status 2>/dev/null || true)"
    if [ -n "$stx" ] && echo "$stx" | grep -q "operational" \
       && ! echo "$stx" | grep -q "not operational"; then
        echo "CHECK kdumpctl_status=PASS" >> "$out"
    else
        echo "CHECK kdumpctl_status=FAIL" >> "$out"; rc=1
    fi
    # ② crashkernel= 预留（/sys/kernel/kmsg 或 /proc/cmdline——OR，kmsg 缺则回退）
    if grep -qs "crashkernel=" "${KD_KMSG:-/sys/kernel/kmsg}" 2>/dev/null \
       || grep -qs "crashkernel=" "${KD_CMDLINE:-/proc/cmdline}" 2>/dev/null; then
        echo "CHECK crashkernel_reserved=PASS" >> "$out"
    else
        echo "CHECK crashkernel_reserved=FAIL" >> "$out"; rc=1
    fi
    # ③ 转储位存在且可写（[ -w ]=access(2) 只读判定，绝不写入任何字节）
    local cd="${KD_CRASH_DIR:-/var/crash}"
    if [ -d "$cd" ] && [ -w "$cd" ]; then
        echo "CHECK crash_dir_writable=PASS" >> "$out"
    else
        echo "CHECK crash_dir_writable=FAIL" >> "$out"; rc=1
    fi
    return "$rc"
}

# ---- healthy：三 ✓（kmsg 路径不存在 → 实证 ② 回退 cmdline） ----
export KD_CMDLINE="$FIX/cmdline_ok" KD_KMSG="$FIX/kmsg_absent" KD_CRASH_DIR="$FIX/crash_ok"
probe "$EVID/kdump_probe_healthy.txt" operational \
    || fail "healthy 场景探针未全过（真值断言失败）"

# ---- kmsg：cmdline 无 crashkernel，kmsg 有 → ② 命中 OR 第二支 ----
export KD_CMDLINE="$FIX/cmdline_nok" KD_KMSG="$FIX/kmsg_ok"
probe "$EVID/kdump_probe_kmsg.txt" operational \
    || fail "kmsg 场景 ② 未命中（OR 第二支失效）"

# ---- degraded：三 FAIL（负向断言——探针非 no-op） ----
export KD_CMDLINE="$FIX/cmdline_nok" KD_KMSG="$FIX/kmsg_absent" KD_CRASH_DIR="$FIX/crash_ro"
if probe "$EVID/kdump_probe_degraded.txt" "not operational"; then
    fail "degraded 场景探针竟全过——探针是 no-op（占位诚实 Violation）"
fi
unset KD_CMDLINE KD_KMSG KD_CRASH_DIR

# ---- 断言 ----
assert_contains "$EVID/kdump_probe_healthy.txt" "CHECK kdumpctl_status=PASS" \
    "① 假 kdumpctl status operational → PASS"
assert_contains "$EVID/kdump_probe_healthy.txt" "CHECK crashkernel_reserved=PASS" \
    "② cmdline 含 crashkernel= → PASS（kmsg 缺失回退 cmdline）"
assert_contains "$EVID/kdump_probe_healthy.txt" "CHECK crash_dir_writable=PASS" \
    "③ 转储位存在且可写 → PASS"
assert_contains "$EVID/kdump_probe_kmsg.txt" "CHECK crashkernel_reserved=PASS" \
    "② OR 第二支：kmsg 含 crashkernel=（cmdline 无）→ PASS"
assert_eq "$(grep -c '=FAIL' "$EVID/kdump_probe_degraded.txt")" "3" \
    "degraded 三探针全 FAIL（负向：探针确为真判定）"

# ---- 真机零接触 + 只读观测证据 ----
POST_CRASH="$(crash_snapshot)"
assert_eq "$PRE_CRASH" "$POST_CRASH" \
    "真机 /var/crash 内容零变化（探针全只读——绝不写转储目录）"
{
    echo "== d15 真机只读观测（$(TS)；探针默认参数直接对真实系统可用）=="
    echo "-- /proc/cmdline crashkernel 预留："
    ck="$(grep -o 'crashkernel=[^ ]*' /proc/cmdline 2>/dev/null || true)"
    if [ -n "$ck" ]; then echo "  $ck"; else echo "  （缺——kdump 未配置）"; fi
    echo "-- /var/crash："
    ls -ld /var/crash 2>&1 | sed 's/^/  /'
    echo "ZERO_TOUCH /var/crash：演练前后内容快照一致（只读实证）"
} > "$EVID/real_system.txt"
assert_exists "$EVID/real_system.txt" "真机只读观测证据落盘"
assert_contains "$EVID/real_system.txt" "ZERO_TOUCH /var/crash" "零接触实证行在场"

drill_pass

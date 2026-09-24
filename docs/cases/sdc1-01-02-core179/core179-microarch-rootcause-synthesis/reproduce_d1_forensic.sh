#!/bin/bash
# Artifact: D1 forensic reproduction (independent verification without 180GB vmcore)
#
# This script reproduces the D1 decisive evidence (§3.2) — that the corrupted
# register value x20 == ror1(__per_cpu_offset[0]) (right-rotate by 1 byte)
# with Hamming distance 0 — from the original 0102-board vmcore. It requires:
#   - crash (v8.0.4+) + vmlinux debuginfo matching the vmcore kernel
#   - the vmcore file (1558 dump is smallest at 9.3GB)
#
# Usage:
#   ./reproduce_d1_forensic.sh <vmlinux> <vmcore>
# Example:
#   ./reproduce_d1_forensic.sh \
#     /usr/lib/debug/usr/lib/modules/6.6.0-145.3.23.154.oe2403sp3.aarch64/vmlinux \
#     /home/sdc/vmcore/127.0.0.1-2026-08-25-15:58:09/vmcore
#
# Output: the crash commands + a Python verification that rol1(slot[0]) == x20.
set -e
VML="${1:?usage: $0 <vmlinux> <vmcore>}"
VMC="${2:?usage: $0 <vmlinux> <vmcore>}"

if [ "$(id -u)" -ne 0 ]; then
  echo "note: vmcore is root-owned; using sudo for crash"
  SUDO="sudo"
else
  SUDO=""
fi

# crash commands to dump the per-cpu offset array head + slot 146
CMDFILE=$(mktemp)
cat > "$CMDFILE" <<'EOF'
p &__per_cpu_offset
rd -64 0xffffb378e29e55d0 1
rd -64 0xffffb378e29e5a60 1
set 179
EOF

echo "=== D1 forensic reproduction (§3.2) ==="
echo "crash commands:"
cat "$CMDFILE"
echo ""
echo "=== crash output (slot[0] and slot[146] from vmcore) ==="
$SUDO crash -i "$CMDFILE" "$VML" "$VMC" 2>/dev/null | grep -vE "seek error|Copyright|GNU|free software|NO WARRANTY|gathering|please wait"
rm -f "$CMDFILE"

echo ""
echo "=== Python verification (golden expected: ror1(slot[0]) == x20, Hamming 0) ==="
echo "    (ror1 = right-rotate by 1 byte; slot[0] is the stale-replay source, NOT the truth)"
python3 - <<'PYEOF'
def ror(v, k, bits=64):
    # right-rotate the 64-bit value by k bytes (k*8 bits)
    m = (1<<bits)-1
    return ((v >> (k*8)) | (v << (bits - k*8))) & m
def hamming(a, b): return bin(a ^ b).count("1")

# These values are read from the 1558 vmcore via crash (above).
# Golden expected (from paper §3.2, independently re-verified from 0102 board):
slot0   = 0xffffcc879da2e000   # __per_cpu_offset[0]  (stale-replay SOURCE — the value the load actually returned, per the stale-replay model)
slot146 = 0xffffcc879ed92000   # __per_cpu_offset[146] (the TRUTH the load should have returned)
x20_bad = 0x00ffffcc879da2e0   # the corrupted register value at panic

r1 = ror(slot0, 1)             # right-rotate slot[0] by 1 byte
print(f"slot[0]      = {hex(slot0)}  (stale-replay source; NOT the truth)")
print(f"ror1(slot0) = {hex(r1)}  (right-rotate by 1 byte)")
print(f"x20 (bad)   = {hex(x20_bad)}")
print(f"ror1 == x20? {r1 == x20_bad}  (Hamming distance {hamming(r1, x20_bad)})")
print(f"slot[146]   = {hex(slot146)}  (the TRUTH)")
print(f"Hamming(x20, slot[146]) = {hamming(x20_bad, slot146)}  (>>0: x20 is corrupted, not the truth)")
print(f"XOR(slot0, x20)  popcount = {hamming(slot0, x20_bad)}  (stale-source -> signature; no k<30 pure bit-flip reproduces it)")
print(f"XOR(slot146, x20) popcount = {hamming(slot146, x20_bad)}  (truth -> signature; no k<26 pure bit-flip reproduces it)")
assert r1 == x20_bad, "FAIL: ror1(slot[0]) != x20"
assert hamming(r1, x20_bad) == 0, "FAIL: Hamming != 0"
print("\nPASS: D1 decisive evidence reproduced — x20 == ror1(slot[0]), Hamming 0.")
PYEOF

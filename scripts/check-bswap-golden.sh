#!/bin/sh
# check-bswap-golden.sh — mechanical gate for the issue #374 paradigm.
#
# The movbe self-inverse-roundtrip fix (docs/superpowers/plans/
# 2026-11-01-issue374-movbe-roundtrip-golden.md) requires that the
# init-phase golden for the byte-swapped intermediate NEVER executes on
# the byte-swap datapath under test: a scalar `rev`/`rev16`/`rev32`
# instruction (what __builtin_bswap compiles to on aarch64) or a vector
# TBL/REV would make golden and actual share the faulted unit, reopening
# the symmetric-fault blind spot the fix exists to close. Hand-written
# shift/or reassembly is NOT safe either: GCC's bswap-idiom recognizer
# folds it back into a single `rev`. The identity-LUT lookup form blocks
# that folding, and this script pins the property against the built
# binary.
#
# For each requested symbol (a test's *_init function, or a golden helper)
# it disassembles the EXACT symbol extent and fails if any scalar
# byte-reverse instruction or any vector ALU instruction appears there.
# Pure data movement (ldr/str, mov) is allowed: it is not golden
# computation, and a load/store-path fault hits golden bytes and the
# run-side freshly computed value differently, so the miscompare still
# fires.
#
# Symbol names are C++-mangled in the symtab: the script resolves them by
# substring through nm. A symbol that cannot be resolved, or resolves to
# an empty disassembly, is a FAILURE, not a pass — an empty objdump
# output grep'd for byte-swap instructions is a hollow PASS (the exact
# trap check-scalar-golden.sh was written to prevent; hit in practice on
# 2026-11-01).
#
# Usage:
#   scripts/check-bswap-golden.sh <binary> <symbol-substring>...
# Example:
#   scripts/check-bswap-golden.sh builddir/sdcshield \
#       movbe_init movbe_dump_init movbe_v_init
# Exit: 0 = all symbols byte-swap-clean, 1 = any violation or resolution
# failure.

set -u

if [ $# -lt 2 ]; then
    echo "usage: $0 <binary> <symbol-substring>..." >&2
    exit 1
fi

BIN=$1; shift
[ -x "$BIN" ] || { echo "error: '$BIN' is not an executable" >&2; exit 1; }

# Scalar byte-reverse mnemonics (A64): rev/rev16/rev32 with w or x
# registers. Vector byte-permutes (rev/tbl on vN./zN.) are covered by VALU_RE.
BREV_RE='[[:space:]](rev|rev16|rev32)[[:space:]]+[wx][0-9]+[[:space:]]*,'
# Vector ALU mnemonics (A64 SIMD + SVE): op with a vN.<shape> or zN.<shape>
# operand. Covers NEON (v registers) AND SVE (z registers -- svtbl compiles
# to `tbl z0.b, ...`), plus the suffixed forms (rev32/rev64/zip1/uzp2/...)
# the unsuffixed-only alternation missed. Data movement (ldr/str, mov/ins/
# dup) is deliberately excluded.
VALU_RE='[[:space:]](add|sub|eor|and|orr|bic|orn|mul|mla|mls|neg|abs|shl|sshll2?|ushll2?|sri|sli|ext|trn1?|trn2?|zip1?|zip2?|uzp1?|uzp2?|tbl|tbx|rev16|rev32|rev64|revb|rev)[[:space:]]+[vz][0-9]+[.[]'

fail=0
for sub in "$@"; do
    # resolve mangled symbol(s) + size via nm; local (t) and global (T) both fine.
    # The movbe family names every per-TU init function identically (movbe_init),
    # so one substring can match MANY distinct static symbols at different
    # addresses — ALL of them are checked; a violation in any one is a failure.
    # nm -S fields: $1=addr $2=size $3=type $4=mangled-name
    nmatch=0
    for line in $(nm -S "$BIN" 2>/dev/null | awk -v s="$sub" '$4 ~ s && ($3 == "t" || $3 == "T") {print $1 ":" $2 ":" $4}'); do
        nmatch=$((nmatch + 1))
        addr=${line%%:*}
        rest=${line#*:}
        size=${rest%%:*}
        mangled=${rest#*:}
        end=$((0x$addr + 0x$size))
        out=$(objdump -d "$BIN" --start-address=0x$addr --stop-address=$end 2>/dev/null)
        insns=$(printf '%s\n' "$out" | grep -cE '^[[:space:]]+[0-9a-f]+:')
        brev=$(printf '%s\n' "$out" | grep -cE "$BREV_RE")
        valu=$(printf '%s\n' "$out" | grep -cE "$VALU_RE")
        if [ "$insns" -eq 0 ]; then
            echo "FAIL  $sub [$mangled @0x$addr]: empty disassembly (hollow-pass guard)"
            fail=1
        elif [ "$brev" -gt 0 ]; then
            echo "FAIL  $sub [$mangled @0x$addr]: $brev scalar byte-reverse instruction(s) in golden path:"
            printf '%s\n' "$out" | grep -E "$BREV_RE" | sed 's/^/        /' | head -5
            fail=1
        elif [ "$valu" -gt 0 ]; then
            echo "FAIL  $sub [$mangled @0x$addr]: $valu vector-ALU instruction(s) in golden path:"
            printf '%s\n' "$out" | grep -E "$VALU_RE" | sed 's/^/        /' | head -5
            fail=1
        else
            echo "PASS  $sub [$mangled @0x$addr]: $insns insns, 0 byte-reverse, 0 vector-ALU"
        fi
    done
    if [ "$nmatch" -eq 0 ]; then
        echo "FAIL  $sub: symbol not found in $BIN (wrong name, or inlined away?)"
        fail=1
    fi
done

exit $fail

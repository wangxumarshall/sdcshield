#!/bin/bash
# gem5-512-extract.sh — mechanically extract the per-test verdict summary of
# the gem5 SE 512-bit leg (SVE 全 VL 验证运动 Task 6, controller Ruling E
# discipline: raw logs stay out of git, the extracted summary goes in).
#
# Usage:   gem5-512-extract.sh <campaign-dir>
#
# Inputs (on disk, gitignored):
#   <dir>/gem5-512/batch.tsv     — one line per executed test:
#       <test>\trc=<n>\twall=<n>s\t<exit-line|<no-exit-line>>\t<sim-exit-line>
#       (written by gem5-512-batch.sh's --one mode, O_APPEND single-line printf)
#   <dir>/gem5-512/<test>.log    — raw gem5+sdcshield output of that test
#   <dir>/sve-test-list.txt      — the 190-test list (row order + coverage)
#
# Output (<dir>/gem5-512-verdicts.txt, committed):
#   identity header (gem5 md5, sdcshield version, command shape),
#   column header, then ONE row per listed test IN LIST ORDER:
#       <test> <rc> <wall-s> <exit-line> <tick> <verdict>
#   verdict classification (mechanical; process exit code is authoritative —
#   the overlay can suppress the exit: line and mislabel skips, so the line
#   is only corroboration):
#       pass     rc=0 (exit: line present as "exit: pass", or suppressed)
#       timeout  rc=124 (outer 3600s timeout)
#       fail     "exit: fail" or rc!=0 with an exit line
#       crash    rc!=0 without an exit line (gem5 fatal / signal)
#       ANOMALY  anything else (flagged for manual triage)
#
# The script refuses to trust a partial batch: missing rows abort loudly with
# the missing test names on stderr.

set -euo pipefail

DIR="${1:-.}"
LIST="$DIR/sve-test-list.txt"
BATCH="$DIR/gem5-512/batch.tsv"
OUT="$DIR/gem5-512-verdicts.txt"

[ -f "$LIST" ]  || { echo "ERROR: list not found: $LIST" >&2; exit 2; }
[ -f "$BATCH" ] || { echo "ERROR: batch.tsv not found: $BATCH (batch not run?)" >&2; exit 2; }

md5=$(md5sum /home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt | cut -d' ' -f1)
ver=$(/home/sdc/wangxu/sdcshield-main/builddir/sdcshield --version 2>/dev/null || echo unknown)

{
    echo "# gem5 SE 512-bit leg — per-test verdict summary (mechanical extraction)"
    echo "# gem5 binary: /home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt  md5=$md5"
    echo "#              (Task 5 final 9-stub build)"
    echo "# sdcshield:   $ver (12ccda540fd4 tree)"
    echo "# command:     run_sve.sh sdcshield -e <test> -t 500 -n 1 -f no --ignore-timeout"
    echo "#              <26 baked -O> [gem5-overrides.txt extras] --vl 4"
    echo "# verdict rule: process exit code is authoritative (nofork; per-test yaml"
    echo "#               blocks and sometimes the exit: line itself are suppressed by"
    echo "#               the shmem overlay — Task 5 Ruling H + Task 6 spurious-skip"
    echo "#               finding, triage-512.md). rc=0 = pass; the exit: line is"
    echo "#               corroboration when present; ticks feed the skip audit."
    echo "test	rc	wall-s	exit-line	tick	verdict"
    awk -F'\t' -v LIST="$LIST" -v BATCHARG="$BATCH" '
        BEGIN {
            n = 0
            while ((getline line < LIST) > 0) {
                sub(/\r$/, "", line)
                if (line == "" || line ~ /^#/) continue
                n++; order[n] = line
            }
            close(LIST)
            while ((getline line < BATCHARG) > 0) {
                split(line, f, "\t")
                if (f[1] == "") continue
                rc[f[1]] = f[2]; sub(/^rc=/, "", rc[f[1]])
                wall[f[1]] = f[3]; sub(/^wall=/, "", wall[f[1]]); sub(/s$/, "", wall[f[1]])
                exitl[f[1]] = f[4]
                sim[f[1]] = f[5]
                tick[f[1]] = ""
                if (sim[f[1]] ~ /@ tick /) {
                    t = sim[f[1]]
                    sub(/^.*@ tick /, "", t); sub(/ .*/, "", t)
                    tick[f[1]] = t
                }
            }
            close(BATCHARG)
            missing = 0
            for (i = 1; i <= n; i++) {
                t = order[i]
                if (!(t in rc)) {
                    printf "MISSING ROW: %s\n", t > "/dev/stderr"
                    missing++
                    continue
                }
                hasexit = (exitl[t] != "<no-exit-line>")
                e = exitl[t]; sub(/^exit: /, "", e)
                v = "ANOMALY"
                if (rc[t] == 0 && (e == "pass" || e == "<no-exit-line>")) v = "pass"
                else if (rc[t] == "124") v = "timeout"
                else if (hasexit && e == "fail") v = "fail"
                else if (!hasexit && rc[t] != 0) v = "crash"
                else if (rc[t] != 0) v = "fail"
                printf "%s\t%s\t%s\t%s\t%s\t%s\n", t, rc[t], wall[t], exitl[t], tick[t], v
            }
            exit(missing ? 1 : 0)
        }
    ' /dev/null
} > "$OUT"

rows=$(grep -c -v "^#" "$OUT"); rows=$((rows - 1))
expected=$(grep -c . "$LIST")
if [ "$rows" -ne "$expected" ]; then
    echo "ERROR: $rows data rows != $expected list tests in $OUT" >&2
    exit 2
fi
echo "OK: $OUT — $rows rows (expected $expected)" >&2
awk -F'\t' '!/^#/ && !/^test\t/ {c[$6]++} END {for (k in c) printf "  %-8s %d\n", k, c[k]}' "$OUT" >&2

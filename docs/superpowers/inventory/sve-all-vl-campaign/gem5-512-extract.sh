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
#   then a trailing skip-audit comment block (see below).
#   verdict classification (mechanical; process exit code is authoritative —
#   the overlay can suppress the exit: line and mislabel skips, so the line
#   is only corroboration):
#       pass     rc=0 with the exit line present as "exit: pass"
#       timeout  rc=124 (outer 3600s timeout)
#       fail     "exit: fail" or rc!=0 with an exit line
#       crash    rc!=0 without an exit line (gem5 fatal / signal)
#       ANOMALY  anything else (flagged for manual triage) — including rc=0
#                with the exit line SUPPRESSED: never auto-pass such a cell.
#                Without --fatal-skips an honest init-skip ALSO exits 0
#                (framework/sandstone.cpp: exit_code = EXIT_FAILURE only on
#                total_failures || (total_skips && fatal_skips)), and the
#                exit-line vocabulary is pass/fail/interrupted/invalid only
#                (framework/logging.cpp:587) — a skipped run still prints
#                "exit: pass" (native 128 leg, sve512_fmmla_arm). rc=0
#                therefore cannot MECHANICALLY exclude a skip; the appended
#                skip-audit block (tick floor, triage-512 §5.4) is the
#                required corroboration when reusing this extractor.
#
# The script refuses to trust a partial batch: missing rows abort loudly with
# the missing test names on stderr.

set -euo pipefail

DIR="${1:-.}"
LIST="$DIR/sve-test-list.txt"
BATCH="$DIR/gem5-512/batch.tsv"
OUT="$DIR/gem5-512-verdicts.txt"
IDY="$DIR/gem5-512/identity.txt"
# Env-overridable for off-host reuse (PR #307 #7): defaults are the
# 2026-10-29 campaign locations on the recording host.
G5=${G5:-/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt}
BIN=${BIN:-/home/sdc/wangxu/sdcshield-main/builddir/sdcshield}

[ -f "$LIST" ]  || { echo "ERROR: list not found: $LIST" >&2; exit 2; }
[ -f "$BATCH" ] || { echo "ERROR: batch.tsv not found: $BATCH (batch not run?)" >&2; exit 2; }

# Identity source (PR #307 #7): the data rows come from batch.tsv, produced
# by the binary that RAN the batch — not by whatever is on disk at extract
# time. Prefer the batch-time record written by gem5-512-batch.sh; only when
# it is absent (batches predating that record) recompute from the CURRENT
# disk binaries AND label the header as such, so a post-rebuild re-extraction
# can never silently stamp a new md5 over old data rows. Missing binaries no
# longer kill the script (set -euo pipefail used to make any off-host rerun
# die at the md5sum) — they degrade to "unavailable"/"unknown" plus the label.
if [ -f "$IDY" ]; then
    md5=$(awk -F'\t' '$1 == "gem5_md5" {print $2; exit}' "$IDY")
    ver=$(awk -F'\t' '$1 == "sdcshield_version" {print $2; exit}' "$IDY")
    idysrc="batch-time record gem5-512/identity.txt (written by gem5-512-batch.sh)"
else
    md5=$(md5sum "$G5" 2>/dev/null | cut -d' ' -f1 || true)
    ver=$("$BIN" --version 2>/dev/null || true)
    idysrc="RECOMPUTED AT EXTRACT TIME from the current disk binaries — no batch-time identity record exists; after a gem5 rebuild these values may NOT describe the binary that produced batch.tsv (verify before trusting)"
fi
: "${md5:=unavailable}" "${ver:=unknown}"

{
    echo "# gem5 SE 512-bit leg — per-test verdict summary (mechanical extraction)"
    echo "# gem5 binary: $G5  md5=$md5"
    echo "#              (Task 5 final 9-stub build)"
    echo "# sdcshield:   $ver (12ccda540fd4 tree)"
    echo "# identity source: $idysrc"
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
                if (rc[t] == 0 && e == "pass") v = "pass"
                else if (rc[t] == "124") v = "timeout"
                else if (hasexit && e == "fail") v = "fail"
                else if (!hasexit && rc[t] != 0) v = "crash"
                else if (rc[t] != 0) v = "fail"
                printf "%s\t%s\t%s\t%s\t%s\t%s\n", t, rc[t], wall[t], exitl[t], tick[t], v
            }
            exit(missing ? 1 : 0)
        }
    ' /dev/null
    echo "#"
    echo "# skip-audit (triage-512 §5.4 discipline): rc=0 cannot mechanically"
    echo "# exclude an honest init-skip — without --fatal-skips a skip also exits 0"
    echo "# and still prints \"exit: pass\" (framework/logging.cpp:587 vocabulary is"
    echo "# pass/fail/interrupted/invalid only; native 128 leg sve512_fmmla_arm). The"
    echo "# tick column is the audit handle: an honest init-skip stops at the"
    echo "# framework init floor (no test round — in the 2026-10-29 clean batch the"
    echo "# lowest real-run tick was 5.94e9, fma_sve). Lowest 10 rc=0 ticks:"
    awk -F'\t' '
        $2 == "rc=0" && $5 ~ /@ tick / {
            t = $5; sub(/^.*@ tick /, "", t); sub(/ .*/, "", t)
            print t, $1
        }' "$BATCH" | sort -n | head -10 | sed 's/^/#   /'
} > "$OUT"

rows=$(grep -c -v "^#" "$OUT"); rows=$((rows - 1))
expected=$(grep -c . "$LIST")
if [ "$rows" -ne "$expected" ]; then
    echo "ERROR: $rows data rows != $expected list tests in $OUT" >&2
    exit 2
fi
echo "OK: $OUT — $rows rows (expected $expected)" >&2
awk -F'\t' '!/^#/ && !/^test\t/ {c[$6]++} END {for (k in c) printf "  %-8s %d\n", k, c[k]}' "$OUT" >&2

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
#       [\tsrc=batch]
#       (written by gem5-512-batch.sh's --one mode, O_APPEND single-line
#       printf; the trailing src=batch stamp — absent in batches predating
#       2026-10-30 — feeds the provenance audit below)
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
#       pass     rc=0 with the exit line present as "exit: pass" AND a
#                parseable tick >= the init-skip floor (tick_floor below)
#       timeout  rc=124 (outer 3600s timeout)
#       fail     "exit: fail" or rc!=0 with an exit line
#       crash    rc!=0 without an exit line (gem5 fatal / signal)
#       ANOMALY  anything else (flagged for manual triage) — including rc=0
#                with the exit line SUPPRESSED, and rc=0+"exit: pass" whose
#                tick is MISSING (fail-closed, PR #307 #24: format drift or
#                a polluted log must not silently bypass the floor) or BELOW
#                the init-skip floor. Never auto-pass such
#                cells: without --fatal-skips an honest init-skip ALSO exits
#                0 (framework/sandstone.cpp: exit_code = EXIT_FAILURE only
#                on total_failures || (total_skips && fatal_skips)) and
#                still prints "exit: pass" (vocabulary pass/fail/interrupted/
#                invalid only, framework/logging.cpp:587). No log LINE
#                discriminates: per-test yaml blocks are suppressed by the
#                overlay (0 of 190 campaign logs carry a "result:" line) —
#                the tick floor is the only mechanical handle. LIMIT: a skip
#                whose test_init is heavier than the lightest real round can
#                tick ABOVE the floor; the appended skip-audit block (lowest
#                rc=0 ticks) is the required human corroboration.
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

# Tick-floor identity binding (PR #307 #34): tick_floor below was measured
# against THIS gem5 binary (the campaign's Task 5 final 9-stub build) — a
# different binary shifts both the honest-skip floor and the lightest-real
# reference (different startup/round costs), so an unanchored floor must not
# classify rc=0 rows. Fail loudly (fail-closed, consistent with the
# missing-tick rule); 'unavailable' (binary missing off-host) fails the
# binding too — judging with an unverifiable identity is exactly the
# laundering these guards exist to prevent.
floor_anchor_md5="3f9011f40dbe80ec1df478c62ddb0b06"
if [ "$md5" != "$floor_anchor_md5" ]; then
    echo "ERROR: tick_floor 5900000000 is anchored to gem5 md5 $floor_anchor_md5, but this extraction's identity resolved to '$md5' — re-measure the honest-skip vs lightest-real ticks with the current binary (see the tick_floor comment in the awk below) and re-anchor both the constant and this binding before classifying; refusing to judge rc=0 rows with an unanchored floor" >&2
    exit 2
fi

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
                sub(/\|.*/, "", line)   # list contract: optional "|time|default" suffix (parse_test_list_line)
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
            # init-skip tick floor (PR #307 #15; measured 2026-10-30 with
            # the exact batch command shape on the campaign host, gem5 md5
            # 3f9011f4...): an honest init-skip runs NO test round, so its
            # tick sits below the lightest real run. Measured honest skip
            # sve2_cross_precision_arm @512 (SVE2 gate closed in gem5) =
            # 5.786e9; lightest clean-batch real run fma_sve = 5.942e9;
            # floor 5.9e9 sits between them. Re-anchor this constant from a
            # fresh skip-vs-lightest-real measurement when reusing the
            # extractor on a different binary/config.
            tick_floor = 5900000000
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
                if (rc[t] == 0 && e == "pass") {
                    v = "pass"
                    # fail-closed (PR #307 #24): an EMPTY tick (sim line lost
                    # its "@ tick " anchor to format drift or a polluted log)
                    # must NOT bypass the floor — cells at the highest
                    # false-green risk get triage, not unconditional pass.
                    if (tick[t] == "")
                        v = "ANOMALY(missing-tick)"
                    else if (tick[t] + 0 < tick_floor)
                        v = "ANOMALY(init-skip)"
                }
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
    echo "# skip-audit (triage-512 §5.4 discipline + mechanical floor, PR #307 #15):"
    echo "# rc=0 alone cannot exclude an honest init-skip — without --fatal-skips a"
    echo "# skip also exits 0 and still prints \"exit: pass\" (framework/logging.cpp:587"
    echo "# vocabulary is pass/fail/interrupted/invalid only; native 128 leg"
    echo "# sve512_fmmla_arm), and per-test yaml blocks are suppressed by the overlay"
    echo "# (0 of 190 campaign logs carry a \"result:\" line). Rows whose rc=0 tick"
    echo "# is MISSING (fail-closed, PR #307 #24 — format drift / polluted log)"
    echo "# or falls below the floor are classified ANOMALY(missing-tick) /"
    echo "# ANOMALY(init-skip) above and stop matrix generation (matrix-gen.sh"
    echo "# FATALs on unknown verdicts)."
    echo "# Floor basis (measured 2026-10-30, batch command shape, gem5 md5"
    echo "# 3f9011f4...): honest skip sve2_cross_precision_arm @512 = 5.786e9;"
    echo "# lightest clean-batch real run fma_sve = 5.942e9; floor = 5.9e9."
    echo "# LIMIT: a skip with heavy test_init can tick ABOVE the floor — audit"
    echo "# the lowest rc=0 ticks below by hand when reusing this extractor:"
    # NOTE: do NOT use `head -10` here — head exits after 10 lines and sort
    # then intermittently takes SIGPIPE; with pipefail + set -e that kills
    # the script mid-block and leaves a TRUNCATED verdicts file (measured
    # 130/300 failures; self-caught during Round-4 verification). sed reads
    # ALL of its input, so the upstream never sees EPIPE.
    awk -F'\t' '
        $2 == "rc=0" && $5 ~ /@ tick / {
            t = $5; sub(/^.*@ tick /, "", t); sub(/ .*/, "", t)
            print t, $1
        }' "$BATCH" | sort -n | sed -n '1,10s/^/#   /p'
    # provenance audit (PR #307 #18/#25): three mechanical checks —
    #   (1) wall > 3660 s: exceeds the batch script's outer timeout (3600 s
    #       + 60 s slack) — NOT a script product;
    #   (2) rows lacking the src=batch stamp, in a PARTIALLY stamped
    #       batch.tsv: manually appended or legacy (fully-unstamped legacy
    #       batches are covered by the LIMIT note instead of row spam);
    #   (3) every listed test must have a non-empty per-test log on disk
    #       (a row that never ran through --one has no log).
    prov_wall=$(awk -F'\t' '{
            wall = $3; sub(/^wall=/, "", wall); sub(/s$/, "", wall)
            if (wall + 0 > 3660)
                print "#   " $1 " (wall=" wall " s, " $2 ") — exceeds the script outer timeout 3600"
        }' "$BATCH")
    nstamped=$(awk -F'\t' '$6 == "src=batch"' "$BATCH" | wc -l)
    ntotal=$(grep -c . "$BATCH")
    prov_unstamped=""
    if [ "$nstamped" -gt 0 ] && [ "$nstamped" -lt "$ntotal" ]; then
        prov_unstamped=$(awk -F'\t' '$6 != "src=batch" { print "#   " $1 " — no src=batch stamp (manual append or legacy row)" }' "$BATCH")
    fi
    nologs=$(awk '{ sub(/\r$/, ""); if ($0 == "" || $0 ~ /^#/) next; sub(/\|.*/, ""); print }' "$LIST" \
        | while IFS= read -r t; do
              [ -s "$DIR/gem5-512/$t.log" ] || echo "#   $t — no non-empty per-test log on disk"
          done)
    # All logs absent (in-repo regen without the batch workspace — Ruling E
    # keeps the big raw logs out of git; batch.tsv itself is committed since
    # PR #307 #30): one honest note instead of N identical flag lines.
    list_n=$(awk '{ sub(/\r$/, ""); if ($0 == "" || $0 ~ /^#/) next; sub(/\|.*/, ""); n++ } END { print n + 0 }' "$LIST")
    miss_n=$(printf '%s\n' "$nologs" | grep -c . || true)
    if [ "$miss_n" = "$list_n" ] && [ "$list_n" -gt 0 ]; then
        nologs="#   (no per-test logs in this checkout — raw logs stay out of git per Ruling E; run the extractor in the batch workspace for this cross-check)"
    fi
    if [ -n "$prov_wall" ] || [ -n "$prov_unstamped" ] || [ -n "$nologs" ]; then
        echo "#"
        echo "# provenance audit (PR #307 #18/#25): stamped rows $nstamped/$ntotal"
        echo "# (src=batch 6th field, added to gem5-512-batch.sh after this campaign;"
        echo "# 0 stamped = legacy batch). Flagged rows below; LIMIT (honest bound):"
        echo "# in a legacy batch the wall>3660 check is the only discriminator and it"
        echo "# is coincidental, not structural — a manually appended row with"
        echo "# wall <= 3600 s (e.g. a direct re-run with different -O overrides) is"
        echo "# mechanically indistinguishable from a script row; cross-check"
        echo "# triage-512 §4 (batch record) and the per-test logs before trusting:"
        if [ -n "$prov_wall" ]; then echo "$prov_wall"; fi
        if [ -n "$prov_unstamped" ]; then echo "$prov_unstamped"; fi
        if [ -n "$nologs" ]; then echo "$nologs"; fi
    fi
} > "$OUT"

rows=$(grep -c -v "^#" "$OUT"); rows=$((rows - 1))
# expected count uses the SAME line contract as the awk loader above (blank
# and ^# lines skipped) — grep -c . would count comment lines and false-fail
# the row check on a commented list (PR #307 #21).
expected=$(awk '{ sub(/\r$/, ""); if ($0 == "" || $0 ~ /^#/) next; n++ } END { print n + 0 }' "$LIST")
if [ "$rows" -ne "$expected" ]; then
    echo "ERROR: $rows data rows != $expected list tests in $OUT" >&2
    exit 2
fi
echo "OK: $OUT — $rows rows (expected $expected)" >&2
awk -F'\t' '!/^#/ && !/^test\t/ {c[$6]++} END {for (k in c) printf "  %-8s %d\n", k, c[k]}' "$OUT" >&2

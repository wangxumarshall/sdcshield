#!/bin/bash
# extract-summary.sh — mechanically summarize an sdcshield yaml run log into
# a per-test TSV (controller Ruling E, SVE 全 VL 验证运动 Task 2).
#
# Usage:   extract-summary.sh <run.log> <test-list.txt> <out.summary.txt>
#
# Output:  a comment header (the run's exact command line, binary version,
#          exit line), a column header, ONE row per listed test:
#              <test>\t<result>\t<iterations>\t<test-runtime-ms>
#          and a trailing comment line carrying the framework-appended
#          mce_check verdict (comment lines never count as data rows, so the
#          row-count check below is unaffected).
#
# Extraction rules (keyed on the yaml per-fracture block structure — the
# framework emits one "- test:" block per fracture, each with exactly one
# 2-space-indented "result:" and "test-runtime:" line; see
# framework/logging.cpp YamlLogger::print_result_line/print_fixed and
# framework/sandstone_run.cpp run_one_test):
#   - result     = the MOST SERIOUS "result:" of that test's blocks (worst-wins;
#                  per-thread/per-iteration sub-entries sit at deeper indentation
#                  and never match the 2-space anchor). Not last-wins: the
#                  framework retries a failed test (framework/sandstone_run.cpp
#                  run_one_test — default retest_count = 10, and it still
#                  returns Failed if ANY round failed), so an intermittent-SDC
#                  log reads pass…fail…retry-pass and last-wins would record
#                  "pass" for a test the log's own exit line calls fail.
#                  Severity follows the TestResult enum as logged:
#                  pass < skip < fail < crash < operating system error <
#                  timed out < interrupted < unknown (an unrecognized value
#                  outranks everything so it is never silently dropped; ties
#                  keep the first). "skip" outranks "pass" because a skip ends
#                  the fracture loop immediately (run_one_test "don't repeat
#                  skipped tests"), so a pass→skip sequence is a skip, matching
#                  the framework's final verdict.
#   - iterations = number of "- test:" fracture blocks for that test.
#   - test-runtime-ms = sum of the blocks' "test-runtime:" values (ms).
#   - rows are restricted to tests named in <test-list.txt> (the framework
#     appends mce_check after the last list test; it is not a list test and is
#     reported only as a trailing comment line, never as a data row).
#
# The script refuses to trust a partial log: it fails loudly if the log has no
# "exit:" line (run died mid-way), if the row count != list size, or if any
# listed test has no block in the log.

set -euo pipefail

LOG="$1"; LIST="$2"; OUT="$3"

[ -f "$LOG" ]  || { echo "ERROR: log not found: $LOG" >&2; exit 2; }
[ -f "$LIST" ] || { echo "ERROR: list not found: $LIST" >&2; exit 2; }

cmdline=$(grep -a -m1 "^command-line: " "$LOG" || true)
version=$(grep -a -m1 "^version: " "$LOG" || true)
exitline=$(grep -a "^exit: " "$LOG" | tail -1 || true)
if [ -z "$exitline" ]; then
    echo "ERROR: no 'exit:' line in $LOG — partial corpse, refusing to summarize" >&2
    exit 2
fi

{
    echo "# summary of: $LOG"
    echo "# $cmdline"
    echo "# $version"
    echo "# $exitline"
    echo "test	result	iterations	test-runtime-ms"
    awk -v LIST="$LIST" '
        BEGIN {
            n = 0
            while ((getline line < LIST) > 0) {
                sub(/\r$/, "", line)
                if (line == "" || line ~ /^#/) continue
                sub(/\|.*/, "", line)   # list contract: optional "|time|default" suffix (parse_test_list_line)
                n++; order[n] = line
            }
            close(LIST)
        }
        /^- test: / {
            name = $0; sub(/^- test: */, "", name)
            blocks[name]++
            cur = name
        }
        function rank(r) {
            if (r == "pass") return 0
            if (r == "skip") return 1
            if (r == "fail") return 2
            if (r == "crash") return 3
            if (r == "operating system error") return 4
            if (r == "timed out") return 5
            if (r == "interrupted") return 6
            return 9       # unknown result value: outranks everything, never dropped
        }
        /^  result: / {
            if (cur != "") {
                r = $0; sub(/^  result: */, "", r)
                if (!(cur in worst) || rank(r) > rank(worst[cur]))
                    worst[cur] = r
            }
        }
        /^  test-runtime: / {
            if (cur != "") { v = $0; sub(/^  test-runtime: */, "", v); rt[cur] += v + 0 }
        }
        END {
            for (i = 1; i <= n; i++) {
                t = order[i]
                printf "%s\t%s\t%d\t%.3f\n", t, (t in worst ? worst[t] : "MISSING"), blocks[t] + 0, rt[t] + 0
            }
            if ("mce_check" in worst)
                printf "# mce_check (framework-appended, not a list test): %s\n", worst["mce_check"]
        }
    ' "$LOG"
} > "$OUT"

rows=$(grep -c -v "^#" "$OUT")
rows=$((rows - 1))   # minus the column header
expected=$(awk '{ sub(/\r$/, ""); if ($0 == "" || $0 ~ /^#/) next; n++ } END { print n + 0 }' "$LIST")
   # counted the same way the awk loader above counts (blank and ^# lines
   # are skipped), so a commented list does not spuriously fail the check.

if [ "$rows" -ne "$expected" ]; then
    echo "ERROR: $rows data rows != $expected list tests in $OUT" >&2
    exit 2
fi
if grep -q "MISSING" "$OUT"; then
    echo "ERROR: listed test with no log block in $OUT:" >&2
    grep "MISSING" "$OUT" >&2
    exit 2
fi
echo "OK: $OUT — $rows rows (expected $expected)" >&2

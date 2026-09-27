#!/bin/bash
# d02_test_bug.sh — 测试 bug 样本（TEST_BUG）演练（v5 §17.2 第 2 类）。
#
# 注入→观测链（三段全真实路径）：
#   ① ledger 行 note 含竞态关键词（M0 分类单一事实源 _TEST_RACE_KEYS）→
#      sdc_eventd → sdc_mismatch 事件 classification.primary==test_race
#      （test bug 方向定案标注——绝不混入硬件 SDC 候选）；
#   ② 竞态定案事件目录（race_audit.json verdict=race_confirmed——三探针
#      检查单产物口径）→ sdc_reproducer gate 子命令 → 门禁 5 拒（gate 拒，
#      原始证据保留）；
#   ③ 毒丸队列记录（非法 JSON）→ sdc_reproducer --from-queue --once →
#      status=invalid 单列（原文截留入案，rc=1 诚实汇报）。
#
# 断言（≥2）：test_race 分类 / gate 拒 / invalid 单列（三段各≥1）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d02_test_bug

SEED="AES:d02bug$(printf '%054d' 0)"
EVDIR="$DRILL_ROOT/events/$(date +%Y%m%d-%H%M%S)-d02bug"
mkdir -p "$EVDIR"

# ---- 注入①：ledger 竞态关键词行 ----
echo "$(TS),d02_bug,rc=1,test=zstd19,seed=$SEED,复测全数复现（竞态 test_bug 样本 M5 演练）" \
    >> "$DRILL_ROOT/events/ledger.csv"
run_eventd

# 断言①：test_race 分类（M0 关键词单一事实源——绝不误标硬件 SDC）
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==1 and d[0]["event_type"]=="sdc_mismatch"
    and d[0]["classification"]["primary"]=="test_race"
    and d[0]["classification"]["status"]=="open"' \
    "sdc_mismatch 事件分类 primary=test_race（竞态关键词命中）"

# ---- 注入②：竞态定案事件目录（门禁输入——七项门禁第 5 项探针产物） ----
cat > "$EVDIR/stdout_summary.out" <<EOF
- test: zstd19
  result: fail
  fail: { cpu-mask: 'X', time-to-fail: 2.511, seed: '$SEED'}
EOF
cat > "$EVDIR/context.txt" <<EOF
cmd: /bin/true --cpuset=3 -e zstd19 -t 100s -o /tmp/d02.yaml
rc: 1  date: $(date -Is)  fail_seed: $SEED(usable=1)  failed_test: zstd19
EOF
cat > "$EVDIR/yaml_extract.txt" <<EOF
command-line: 'sdcshield --cpuset=3 -e zstd19 -t 100s'
- test: zstd19
  state: { seed: '$SEED', iteration: 5, retry: false }
  result: fail
  fail: { cpu-mask: 'X', time-to-fail: 2.511, seed: '$SEED'}
  threads:
  - thread: 3
    id: { logical:  3, package: 0, numa_node: 0, module: 0, core:  3, thread: 0 }
    state: failed
    time-to-fail: 2.511
    messages:
      data-miscompare:
        type:        uint32_t
        offset:      [ 44, 0 ]
        actual:      '0x000000ff'
        expected:    '0x000000f7'
        mask:        '0x00000008'
EOF
cat > "$EVDIR/retests.txt" <<EOF
retest1(targeted) rc=1 fail/crash=1
EOF
# 竞态三探针定案（v5 §10.1.5 检查单产物）：race_confirmed → 门禁 5 拒
printf '{"verdict": "race_confirmed", "probes": {"lockdep": "hold", "tsan": "race", "stress_loops": "diff"}, "note": "M5 演练合成定案"}\n' \
    > "$EVDIR/race_audit.json"

set +e
python3 "$REPO/tools/excite/sdc_reproducer.py" gate "$EVDIR" \
    > "$DRILL_ROOT/gate.out" 2>&1
GRC=$?
set -e
cat "$DRILL_ROOT/gate.out"

# 断言②：门禁拒（rc=1 + 门禁5 拒文案——test_bug 一等候选，原始证据保留）
assert_eq "$GRC" "1" "gate 子命令 rc=1（门禁未通过）"
assert_contains "$DRILL_ROOT/gate.out" "竞态审计定案" "gate 输出含竞态定案拒项"
assert_contains "$DRILL_ROOT/gate.out" "门禁5 拒" "gate 输出含 [门禁5 拒] 标号"
assert_not_contains "$DRILL_ROOT/gate.out" "门禁3 拒" "miscompare 重算自洽（ff^f7=08）——无门禁3 拒"
assert_not_contains "$DRILL_ROOT/gate.out" "门禁6 拒" "detecting-cpu=3 在 cpuset=3 内——无门禁6 拒"

# ---- 注入③：毒丸队列记录（非法 JSON）→ invalid 单列 ----
mkdir -p "$DRILL_ROOT/spool/repro_queue"
printf '{"event_dir": "%s", "test": "zstd19", "bad json\n' "$EVDIR" \
    > "$DRILL_ROOT/spool/repro_queue/0000000000000000001.json"
set +e
python3 "$REPO/tools/excite/sdc_reproducer.py" --from-queue --once \
    --data-root "$DRILL_ROOT" > "$DRILL_ROOT/consumer.out" 2>&1
QRC=$?
set -e
cat "$DRILL_ROOT/consumer.out"

# 断言③：invalid 单列（原文截留 + 队列清空 + rc=1 诚实汇报）
assert_eq "$QRC" "1" "毒丸消费 rc=1（invalid 诚实汇报）"
assert_contains "$DRILL_ROOT/consumer.out" "invalid" "消费输出含 invalid 行"
jassert "$DRILL_ROOT/spool/repro_done/0000000000000000001.json" \
    'd["status"]=="invalid" and "原文截留" in d["error"] and "bad json" in d["raw"]' \
    "done 记录 status=invalid 且原文截留在案"
assert_eq "$(ls "$DRILL_ROOT/spool/repro_queue" | wc -l)" "0" "毒丸消费后队列清空"

drill_pass

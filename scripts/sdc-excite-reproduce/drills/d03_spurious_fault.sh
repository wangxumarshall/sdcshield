#!/bin/bash
# d03_spurious_fault.sh — spurious fault 日志演练（v5 §17.2 第 3 类）。
#
# 注入→观测链：
#   ① 产出端口径核验：ras collector 真实函数 count_spurious 对合成 dmesg
#      文本计数（生产端 journal_watch.log SPURIOUS 行的单一事实源）；
#   ② journal_watch.log SPURIOUS 行（collector@ras 生产格式）→ sdc_eventd →
#      ras_keyword yellow 事件入流（source_line 溯源）；
#   ③ 控制器诚实不升级：ras_keyword yellow 无规则命中 → 零决策、终态 green
#      （spurious 是 canary 观测信号——不冒充硬件 SDC，不触发联锁）。
set -euo pipefail
source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
drill_init d03_spurious_fault

# ---- 断言①：产出端计数函数真实口径（count_spurious——journal SPURIOUS 行来源） ----
N=$(python3 -c '
import sys
sys.path.insert(0, "'"$REPO"'/tools/telemetry")
import sdc_collector_ras as ras
text = ("Aug 1 00:00:01 kernel: spurious translation fault on tso at 0x0000ff\n"
        "Aug 1 00:00:02 kernel: unrelated line\n"
        "Aug 1 00:00:03 kernel: SPURIOUS TRANSLATION FAULT on tso at 0x0000f7\n")
print(ras.count_spurious(text))
')
assert_eq "$N" "2" "count_spurious 对合成 dmesg 计数=2（大小写不敏感、噪声行不计）"

# ---- 注入②：journal_watch.log SPURIOUS 行（collector@ras 生产格式逐字） ----
echo "[$(TS)] SPURIOUS total=2 delta=1 source=dmesg（无 per-CPU 定位，M2 BPF 升级）" \
    >> "$DRILL_ROOT/monitor/journal_watch.log"
run_eventd
run_controller

# 断言②：ras_keyword yellow 事件入流（源行溯源保留）
jlassert "$DRILL_ROOT/spool/events.jsonl" \
    'len(d)==1 and d[0]["event_type"]=="ras_keyword" and d[0]["severity"]=="yellow"
    and "SPURIOUS" in d[0]["source_line"] and d[0]["source"]=="journal"' \
    "SPURIOUS 行 → ras_keyword yellow 事件（source_line 溯源）"

# 断言③：诚实不升级——零决策、终态 green（canary 观测不冒充硬件 SDC）
NDEC=0
[ -f "$DRILL_ROOT/spool/controller_ledger.jsonl" ] && NDEC=$(wc -l < "$DRILL_ROOT/spool/controller_ledger.jsonl")
assert_eq "$NDEC" "0" "控制器零决策（ras_keyword yellow 无规则命中——诚实入流不升级）"
jassert "$DRILL_ROOT/spool/controller_state.json" 'd["state"]=="green"' \
    "终态 green（spurious 不触发联锁/状态转换）"

drill_pass

#!/bin/bash
# drill_lib.sh — M5 故障演练脚本族公共库（v5 §17.2 十三类验收基准，Task 3）。
# 被 d01-d13 各演练脚本 source；零第三方依赖（bash + python3 stdlib）。
#
# 安全边界（战役+9 服务运行中——与 m2_drill/m3_drill 同模式）：
#   - 每类演练在独立隔离数据根 $DRILL_OUT/<类名>/root/ 上运行（events/monitor/
#     spool/cmd 四目录由 drill_init 全新建）——绝不注入真实 spool、绝不 kill
#     真实服务、绝不真断网/真满盘/真写 sysfs；
#   - DRILL_OUT 真实根形态守卫：目标目录已含 spool/+driver.log（真实战役根
#     as-built 形态）即拒绝——绝不缺省/误指到真实根；
#   - 演练产出的测试进程（d07 hang 进程/d09 collector 副本）一律 reg_pid
#     注册，trap EXIT 清理——不留孤儿；
#   - 需要 ipmitool 语义的演练（d08）用隔离根内假 ipmitool（PATH 前置），
#     绝不打真 BMC。
#
# 用法（各 dXX 脚本开头）：
#   source "$(dirname "${BASH_SOURCE[0]}")/drill_lib.sh"
#   drill_init <类名>          # 建隔离根 + trap 清理
#   ... 注入 + run_eventd/run_ring/run_controller + 断言 ...
#   drill_pass                 # "== <类名>: PASS ==" + exit 0
#
# 断言框架：assert_contains <file> <grep模式> <msg> / assert_not_contains /
#   assert_exists <path> <msg> / assert_eq <got> <want> <msg> /
#   jassert <json文件> <python布尔式(变量d)> <msg> / fail <msg>（exit 1）。
#
# 环境变量：
#   DRILL_OUT  演练产物根（默认 <drills>/out；pytest 用 --out/tmp_path 覆盖——
#              绝不写仓库内默认位）；DRILL_ONLY 供 drill_all 子集过滤。

# ---------------------------------------------------------------------------
# 路径与全局

DRILLS_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$DRILLS_DIR/../../.." && pwd)"
: "${DRILL_OUT:=$DRILLS_DIR/out}"
RULES="$REPO/configs/sdc-excite-reproduce/rules_m2.json"
COMMON="$REPO/scripts/sdc-excite-reproduce/sdc_common.sh"
DRILL_NAME=""
DRILL_ROOT=""
DRILL_PIDS=()

TS() { date '+%F %T'; }

fail() {  # fail <msg>：断言失败——退出码 1（drill_all 以退出码汇总）
    echo "✗ [$DRILL_NAME] $1" >&2
    exit 1
}

# ---------------------------------------------------------------------------
# 隔离根构造 + 清理

cleanup_drill() {  # kill 本次演练注册的全部测试进程（不留孤儿；幂等）
    local p
    for p in "${DRILL_PIDS[@]:-}"; do
        [ -n "$p" ] || continue
        kill -KILL "$p" 2>/dev/null || :
        wait "$p" 2>/dev/null || :
    done
}

reg_pid() {  # reg_pid <pid>：注册演练自有测试进程（d07/d09）——EXIT 必清
    DRILL_PIDS+=("$1")
}

drill_init() {  # drill_init <类名>：全新隔离根 + 真实根守卫 + trap 清理
    DRILL_NAME="$1"
    [ -n "$DRILL_NAME" ] || { echo "✗ drill_init 需要类名" >&2; exit 1; }
    # 真实根形态守卫：DRILL_OUT 已是 as-built 战役根（spool/+driver.log）即拒绝
    if [ -e "$DRILL_OUT/spool" ] && [ -e "$DRILL_OUT/driver.log" ]; then
        echo "✗ DRILL_OUT=$DRILL_OUT 形似真实战役根（含 spool/+driver.log）——" \
             "演练绝不指向真实根" >&2
        exit 1
    fi
    local out="$DRILL_OUT/$DRILL_NAME"
    rm -rf "$out"                       # 本类产物位（drill 专属 scratch，可重跑）
    DRILL_ROOT="$out/root"
    mkdir -p "$DRILL_ROOT"/events "$DRILL_ROOT"/monitor "$DRILL_ROOT"/spool "$DRILL_ROOT"/cmd
    trap cleanup_drill EXIT INT TERM
    echo "== $DRILL_NAME: 隔离根 $DRILL_ROOT =="
}

# ---------------------------------------------------------------------------
# m2/m3 工具调用（四守护统一 --once + 隔离根入口）

run_tool() {  # run_tool <repo内相对路径> [args...]：python3 tools/<rel> --once --data-root
    python3 "$REPO/tools/$1" --once --data-root "$DRILL_ROOT" "${@:2}"
}

run_eventd()     { run_tool telemetry/sdc_eventd.py; }
run_ring()       { run_tool telemetry/sdc_ring.py; }
run_controller() { run_tool control/sdc_controller.py --rules "$RULES"; }
run_helper()     { run_tool control/sdc_root_helper.py --mock-exec; }  # 仅测试旗标（m2_drill 同款）

# enqueue_repro：复现队列生产端（sdc_common.sh 生产路径——驱动 handle_failure
# 同款接线）。调用方需先 export SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT"。
enqueue_repro_drill() {  # enqueue_repro_drill <event_dir> [test] [seed]
    ( export SDC_EXCITE_REPRODUCE_DIR="$DRILL_ROOT"
      # shellcheck disable=SC1090
      source "$COMMON"
      enqueue_repro "$@" )
}

# ---------------------------------------------------------------------------
# 断言框架（每类至少 2 断言——注入→观测）

assert_exists() {  # assert_exists <path> <msg>
    [ -e "$1" ] || fail "${2:-缺少 $1}"
    echo "  ✓ ${2:-$1 存在}"
}

assert_contains() {  # assert_contains <file> <grep模式> <msg>
    [ -f "$1" ] || fail "${3:-断言失败：$1 不存在}"
    grep -q -- "$2" "$1" || fail "${3:-断言失败：$1 不含 '$2'}"
    echo "  ✓ ${3:-$1 含 '$2'}"
}

assert_not_contains() {  # assert_not_contains <file> <grep模式> <msg>
    [ -f "$1" ] || fail "${3:-断言失败：$1 不存在}"
    ! grep -q -- "$2" "$1" || fail "${3:-断言失败：$1 不应含 '$2'（却含）}"
    echo "  ✓ ${3:-$1 不含 '$2'}"
}

assert_eq() {  # assert_eq <got> <want> <msg>
    [ "$1" = "$2" ] || fail "${3:-断言失败：'$1' != '$2'}"
    echo "  ✓ ${3:-相等：$1}"
}

jassert() {  # jassert <json文件> <python布尔式(根对象为 d)> <msg>——单 JSON 文档
    python3 -c '
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    d = json.load(f)
# 多行表达式按行折叠（续行缩进对 eval 非法——行内字符串内空格不受影响）
expr = " ".join(l.strip() for l in sys.argv[2].splitlines() if l.strip())
assert eval(expr), f"{expr} 不成立: {json.dumps(d, ensure_ascii=False)[:400]}"
' "$1" "$2" || fail "${3:-JSON 断言失败：$1 ← $2}"
    echo "  ✓ ${3:-$1 ← $2}"
}

jlassert() {  # jlassert <jsonl文件> <python布尔式(根对象为 d=逐行对象列表)> <msg>
    python3 -c '
import json, sys
with open(sys.argv[1], encoding="utf-8") as f:
    d = [json.loads(l) for l in f if l.strip()]
expr = " ".join(l.strip() for l in sys.argv[2].splitlines() if l.strip())
assert eval(expr), f"{expr} 不成立: {json.dumps(d, ensure_ascii=False)[:400]}"
' "$1" "$2" || fail "${3:-JSONL 断言失败：$1 ← $2}"
    echo "  ✓ ${3:-$1 ← $2}"
}

drill_pass() {
    echo "== $DRILL_NAME: PASS =="
    exit 0
}

#!/bin/bash
# m3_drill.sh — M3 退出标准演练（v5 §14.1 M3 行：①人工/架构态注入事件能自动
# 到 R3——capsule 完整+可重放；②单核不复现条件不会被错误删除——CORE179 概率
# ddmin 语义）。Task 6 交付，人工/CI 驱动器。
#
# 演练链（隔离数据根全链，产出端/消费端均真实代码路径）：
#   合成 mismatch 事件目录（stdout_summary/yaml_extract/context/retests 四件
#   全齐，data-miscompare 重算自洽 ff^f7=08，detecting 核=实机 online 末核）
#   → enqueue_repro（sdc_common.sh 生产产出端——M2 enqueue_reproduction 落点，
#     驱动 handle_failure 同款）→ spool/repro_queue/<epoch-ns>.json
#   → sdc_reproducer --from-queue --once --data-root <隔离根>（消费端真实链：
#     from_event resolve 冻结 → 真实性门禁七项 → build_capsule → 概率复测×3）
#
# 安全边界（战役+9 服务运行中——绝不碰真实根/真实负载）：
#   - 数据根必须为全新目录：已含 spool/ 即拒绝（m2_drill 同守卫——真实根部署
#     后必含 spool/，天然防误跑）；无参/空参拒绝，绝不缺省到真实根。
#   - SDC_BIN 指向隔离根内假二进制（fake bin）：仅按 --cpuset 是否含 victim
#     锚点核定 rc（victim=1+fail 行 / aggressor 与健康核对照=0）——健康核对照
#     与 run_cycle 的发射路径全走 fake bin，绝不真跑 sdcshield（任务书"fake
#     bin 或 gate 结果注入"二选一：选 fake bin——消费端为 CLI 子进程，无法
#     注入 python mock，fake bin 是唯一能让子进程全链走真的方式）。
#   - capsule 门禁预检（内核/governor/online/拓扑指纹）**不 mock 不跳过**：
#     capsule 平台快照取自实机 sysfs 只读探针，同机 run.sh --check-only 天然
#     一致——退出标准①"可重放"的最强形态（同机重放预检真实通过）。
#   - MEMWATCH_POLL_S=1 / MEMGUARD_WAIT_S=10（演练时限界提速——护栏语义不
#     变）；前置拒：MemAvailable < 6GB（复测内存门门槛——内存紧张期演练的
#     试验必为 rc=250 invalid，先拒绝并明说，不做 7×60s 假等待）。
#
# 断言（全过 exit 0，任一失败 exit 1）：
#   A. 队列消费闭环：done 落位 status=processed + 复测 k=3/3（Wilson ci 同源
#      重算）+ 队列清空 + 门禁过（ok=true 无拒项，manifest.gate 同注入）
#   B. capsule 完整（v5 §10.2 目录逐项 23 文件）+ manifest 冻结口径（本机
#      内核 release / 受界 100s / 二进制 sha256 引用不复制）+ SHA256SUMS
#      全文件校验通过
#   C. capsule 可重放（退出标准①）：run.sh --check-only 真实通过（同机预检，
#      不 mock）+ 重放段冻结 victim 锚点核 / test / seed
#   D. CORE179 语义（退出标准②）：合成 run_trial（复现 = victim={锚点核} ∧
#      aggressor 含 cache ∧ 有 aggressor 核 ∧ 失败 test/seed 在集）→
#      probabilistic_ddmin 小预算 → min_set 保留非空 aggressor（"最小 victim
#      + 必要 aggressor 场"）+ necessary_aggressors 双因素标记 + 删空探测
#      REJECT 记录在树（崩塌有证据，非静默跳过）——绝不输出空 aggressor
#
# 用法: bash m3_drill.sh <隔离数据根>    # 无参/空参拒绝——绝不缺省到真实根
set -euo pipefail

REPO="$(cd "$(dirname "$0")/../.." && pwd)"
REPRODUCER="$REPO/tools/excite/sdc_reproducer.py"
COMMON="$REPO/scripts/sdc-excite-reproduce/sdc_common.sh"

if [ "$#" -ne 1 ]; then
    echo "用法: $0 <隔离数据根>（必填——无参拒绝，防误对真实根）" >&2
    exit 1
fi
ROOT="$1"
if [ -z "$ROOT" ]; then
    echo "✗ 数据根不能为空" >&2
    exit 1
fi
if [ -e "$ROOT/spool" ]; then
    echo "✗ $ROOT 已含 spool/——演练须用全新隔离数据根（防误对真实战役根/重跑污染）" >&2
    exit 1
fi

MA_KB=$(awk '/MemAvailable/{print $2}' /proc/meminfo)
if [ "$MA_KB" -lt 6291456 ]; then
    echo "✗ MemAvailable=${MA_KB}kB < 6GB（复测内存门门槛）——内存紧张期演练试验必为 rc=250 invalid，先拒绝（稍后重试）" >&2
    exit 1
fi

# 实机 online 集（只读探针）：victim 锚点核=末核，健康核对照=首核
eval "$(python3 - <<'PY'
def expand(s):
    out = []
    for p in s.strip().split(","):
        if "-" in p:
            a, b = p.split("-")
            out += list(range(int(a), int(b) + 1))
        elif p:
            out.append(int(p))
    return out
online = expand(open("/sys/devices/system/cpu/online").read())
print(f"ONLINE_N={len(online)}")
print(f"ANCHOR={online[-1]}")
print(f"CONTROL={online[0] if online[0] != online[-1] else online[1]}")
print("CPUSET_REQ=" + ",".join(str(c) for c in sorted(set(online[:2] + [online[-1]]))))
PY
)"
if [ "$ONLINE_N" -lt 2 ]; then
    echo "✗ online 核数 $ONLINE_N < 2——victim/aggressor 无法隔离，无法演练" >&2
    exit 1
fi

SEED="AES:m3drill$(printf '%057d' 0)"
MASK="$(printf '%*s' "$ANCHOR" '' | tr ' ' '.')X"   # cpu-mask 位号=锚点核（回退口径同锚点）
export SDC_EXCITE_REPRODUCE_DIR="$ROOT"             # enqueue_repro（产出端）数据根
export SDC_BIN="$ROOT/fake-sdcshield"               # resolve 冻结/门禁对照/复测发射全走 fake bin
export MEMWATCH_POLL_S=1 MEMGUARD_WAIT_S=10         # 演练时限界（护栏语义不变）

mkdir -p "$ROOT"/events "$ROOT"/spool/repro_queue

# ---- fake bin（隔离根内，绝不真跑 sdcshield） ----
cat > "$SDC_BIN" <<'FAKE'
#!/bin/bash
# m3_drill fake bin —— 演练专用假二进制：--cpuset 含 victim 锚点核 → 写
# fail YAML 退出码 1（victim 复现）；否则（aggressor / 健康核对照）→ 写
# 干净 YAML 退出码 0。绝不执行任何真实负载。
ANCHOR=__ANCHOR__
cpuset= out= prev=
for a in "$@"; do
    case "$a" in
        --cpuset=*) cpuset="${a#--cpuset=}" ;;
        -o) prev=o ;;
        *) if [ "$prev" = o ]; then out="$a"; prev=; fi ;;
    esac
done
[ -n "$out" ] || { echo "fake bin: 无 -o 输出路径" >&2; exit 99; }
hit=0
IFS=,
for c in $cpuset; do [ "$c" = "$ANCHOR" ] && hit=1; done
unset IFS
{
    echo "- test: zstd19"
    if [ "$hit" = 1 ]; then
        echo "  result: fail"
        echo "  fail: { cpu-mask: '__MASK__', time-to-fail: 2.511, seed: '__SEED__'}"
    else
        echo "  result: pass"
    fi
} > "$out"
[ "$hit" = 1 ] && exit 1
exit 0
FAKE
sed -i "s/__ANCHOR__/$ANCHOR/; s/__MASK__/$MASK/; s/__SEED__/$SEED/" "$SDC_BIN"
chmod +x "$SDC_BIN"

# ---- 合成 mismatch 事件目录（M2 handle_failure 产物口径，四件全齐） ----
EVDIR="$ROOT/events/$(date +%Y%m%d-%H%M%S)-m3drill"
mkdir -p "$EVDIR"
cat > "$EVDIR/stdout_summary.out" <<EOF
- test: zstd19
  result: fail
  fail: { cpu-mask: '$MASK', time-to-fail: 2.511, seed: '$SEED'}
EOF
cat > "$EVDIR/context.txt" <<EOF
cmd: $SDC_BIN --cpuset=$CPUSET_REQ -e zstd19 -t 300s -o /tmp/m3drill.yaml
rc: 1  date: $(date -Is)  fail_seed: $SEED(usable=1)  failed_test: zstd19
--- monitor 最近 20 行 ---
$(date '+%F %T'),79,50,38,30,35,,324,0.85,0.88
$(date '+%F %T'),80,51,39,30,35,,324,0.85,0.88
--- EDAC ---
0
--- mem ---
              total        used        free
Mem:          384          100          284
EOF
cat > "$EVDIR/yaml_extract.txt" <<EOF
command-line: 'sdcshield --cpuset=$CPUSET_REQ -e zstd19 -t 300s'
- test: zstd19
  state: { seed: '$SEED', iteration: 5, retry: false }
  result: fail
  fail: { cpu-mask: '$MASK', time-to-fail: 2.511, seed: '$SEED'}
  threads:
  - thread: $ANCHOR
    id: { logical:  $ANCHOR, package: 8442, numa_node: 2, module: 8544, core:  $ANCHOR, thread: 0, family: 72, model: 0xd01, stepping: 0 }
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
retest1(targeted) rc=0 fail/crash=0
retest2(targeted) rc=0 fail/crash=0
retest3(targeted) rc=0 fail/crash=0
EOF

# capabilities.env（resolve 冻结输入——NPROC 实机口径，防漂移校验同源）
printf '# m3_drill 合成（实机 online 计数）\nNPROC=%s\n' "$ONLINE_N" \
    > "$ROOT/capabilities.env"

echo "== m3_drill: 隔离根 $ROOT（victim 锚点核=$ANCHOR 对照核=$CONTROL online=$ONLINE_N）=="

# ---- 产出端：enqueue_repro（生产路径——驱动 handle_failure 同款接线） ----
# shellcheck disable=SC1090
source "$COMMON"
enqueue_repro "$EVDIR" zstd19 "$SEED"
QF=$(echo "$ROOT"/spool/repro_queue/*.json)
[ -f "$QF" ] || { echo "✗ 队列文件未落位 spool/repro_queue/" >&2; exit 1; }
grep -q '"event_dir"' "$QF" || { echo "✗ 队列记录缺 event_dir" >&2; exit 1; }

# ---- 消费端：--from-queue --once（CLI 子进程——与生产同入口） ----
set +e
python3 "$REPRODUCER" --from-queue --once --data-root "$ROOT" \
    > "$ROOT/consumer.out" 2>&1
CRC=$?
set -e
cat "$ROOT/consumer.out"
[ "$CRC" -eq 0 ] || { echo "✗ 消费端 rc=$CRC（预期 processed rc=0）" >&2; exit 1; }
grep -q "processed" "$ROOT/consumer.out" || { echo "✗ 消费输出无 processed 行" >&2; exit 1; }

echo "== 断言 =="
python3 - "$ROOT" "$REPO" "$(basename "$QF")" "$(basename "$EVDIR")" \
    "$ANCHOR" "$CONTROL" "$SEED" <<'PYEOF'
import glob, json, os, subprocess, sys

root, repo, qname, event_id = sys.argv[1:5]
anchor, control, seed = int(sys.argv[5]), int(sys.argv[6]), sys.argv[7]
sys.path.insert(0, os.path.join(repo, "tools", "excite"))
sys.path.insert(0, os.path.join(repo, "tools", "telemetry"))
import sdc_reproducer as sr
import sdc_reducer as red

def fail(msg):
    print(f"✗ {msg}", file=sys.stderr)
    sys.exit(1)

# A. 队列消费闭环：done 落位 + processed + 复测 3/3 + 门禁过 + 队列清空
ddir = os.path.join(root, "spool", "repro_done")
done = sorted(os.listdir(ddir)) if os.path.isdir(ddir) else []
if done != [qname]:
    fail(f"done 落位非预期: {done}（预期仅 [{qname}]）")
rec = json.load(open(os.path.join(ddir, qname), encoding="utf-8"))
if rec["status"] != "processed" or rec["event_id"] != event_id:
    fail(f"done 状态/事件非预期: {rec.get('status')} / {rec.get('event_id')}")
cyc = rec["repro"]
if (cyc["k"], cyc["n"]) != (3, 3) or cyc["ci"] != list(sr.wilson_ci(3, 3)):
    fail(f"概率复测非 3/3 或 ci 非同源 Wilson: {cyc}")
if rec["gate"]["ok"] is not True or any("拒]" in r for r in rec["gate"]["reasons"]):
    fail(f"门禁未过或含拒项: {rec['gate']}")
left = os.listdir(os.path.join(root, "spool", "repro_queue"))
if left:
    fail(f"队列未清空: {left}")
cap = rec["capsule_dir"]
if not os.path.isdir(cap):
    fail(f"capsule 目录缺失: {cap}")

# B. capsule 完整（v5 §10.2 目录逐项）+ manifest 冻结口径 + SHA256SUMS
REQUIRED = ["manifest.json", "resolved-profile.json", "topology.json",
            "platform.json", "binaries/sdcshield.sha256", "inputs/victim.json",
            "golden/README.txt", "event.json",
            "original/stdout.log", "original/result.yaml", "original/context.txt",
            "original/retests.txt", "original/stderr.log",
            "windows/monitor_tail.csv", "windows/NOTES.md",
            "scripts/run.sh", "scripts/verify.sh", "scripts/restore.sh",
            "reduction/graph.jsonl", "reduction/trials.jsonl",
            "diagnosis/hypotheses.json", "diagnosis/report.md", "SHA256SUMS"]
missing = [p for p in REQUIRED if not os.path.isfile(os.path.join(cap, p))]
if missing:
    fail(f"capsule 缺项: {missing}")
man = json.load(open(os.path.join(cap, "manifest.json"), encoding="utf-8"))
if man["event_id"] != event_id or man["gate"]["ok"] is not True:
    fail(f"manifest 事件/门禁非预期: {man.get('event_id')} / {man['gate']}")
if not str(man["binary"]["sha256"]).startswith("sha256:"):
    fail(f"manifest 二进制哈希非 sha256 引用口径: {man['binary']['sha256']}")
if man["platform"]["kernel_release"] != os.uname().release:
    fail("manifest 平台快照非本机——同机 --check-off 预检必拒（可重放前提破坏）")
if man["replay"]["duration_s"] != sr.QUEUE_EVENT_BUDGET_S // sr.QUEUE_TRIALS:
    fail(f"manifest replay 时长未受界（≤5min 预算 per-trial 截断）: "
         f"{man['replay']['duration_s']}")
runsh = os.path.join(cap, "scripts", "run.sh")
if not os.access(runsh, os.X_OK):
    fail("scripts/run.sh 不可执行")
r = subprocess.run(["sha256sum", "-c", "SHA256SUMS"], cwd=cap,
                   capture_output=True, text=True, timeout=60)
if r.returncode != 0:
    fail(f"SHA256SUMS 全文件校验失败: {r.stdout}{r.stderr}")

# C. capsule 可重放（退出标准①）：run.sh --check-only 同机真实通过（不 mock
#    ——平台快照=实机只读探针）+ 重放段冻结 victim 锚点核 / test / seed
r = subprocess.run(["bash", runsh, "--check-only"], cwd=cap,
                   capture_output=True, text=True, timeout=60)
if r.returncode != 0 or "预检通过" not in (r.stdout + r.stderr):
    fail(f"run.sh --check-only 未通过: rc={r.returncode} {r.stdout}{r.stderr}")
body = open(runsh, encoding="utf-8").read()
for token in (f"--cpuset={anchor} ", "-e zstd19", seed):
    if token not in body:
        fail(f"run.sh 重放段缺冻结复现条件 {token!r}")

# D. CORE179 语义（退出标准②）：合成 run_trial + 概率 ddmin 小预算——复现 =
#    victim 含锚点核 ∧ aggressor 含 cache ∧ 有 aggressor 核 ∧ 失败 test/seed
#    在集（最小集收缩到 victim={锚点核}；θ=0.5：合成世界复现率非 0 即 1，
#    中分即判；min_trials=2 禁单次定夺——CORE179 硬规则）
def repro(cfg):
    return (anchor in cfg["victim_cpus"]
            and "cache" in cfg["aggressor_families"]
            and cfg["aggressor_cpus"]
            and "zstd19" in cfg["tests"]
            and seed in cfg["seeds"])

factors = red.Factors(
    victim_cpus=[anchor, control],          # 第二 victim 候选（合成标签）
    aggressor_cpus=[control, control + 1],  # aggressor 核标签×2
    aggressor_families=["cache", "integer"],
    tests=["zstd19", "memcpy_rewr"],
    seeds=[seed, "LCG:0"])
out = red.probabilistic_ddmin(
    factors, repro,
    budget={"min_trials": 2, "max_trials": 40, "target_repro_rate": 0.5},
    rng_seed=179)
ms = out["min_set"]
if out["status"] != "minimized":
    fail(f"ddmin 未收敛: {out['status']}")
if ms["victim_cpus"] != [anchor] or ms["tests"] != ["zstd19"] or ms["seeds"] != [seed]:
    fail(f"min_set victim/test/seed 非预期: {ms}")
if ms["aggressor_families"] != ["cache"] or not ms["aggressor_cpus"]:
    fail(f"min_set aggressor 被删空或未收缩（CORE179：必须保留必要 aggressor）: {ms}")
if set(out["necessary_aggressors"]) != {"aggressor_cpus", "aggressor_families"}:
    fail(f"necessary_aggressors 非预期: {out['necessary_aggressors']}")
for fac in ("aggressor_cpus", "aggressor_families"):
    empt = [t for t in out["trials"]
            if t["factor"] == fac and t["config"][fac] == []]
    if not empt or not all(t["verdict"] == "REJECT" and t["applied"] is not True
                           for t in empt):
        fail(f"aggressor 因素 {fac} 删空探测无 REJECT 回退证据: {empt}")
if out["total_trials"] > 200:
    fail(f"小预算超界: {out['total_trials']}")
print(f"✓ A 队列消费闭环（processed 复测 k=3/3 门禁过）  "
      f"✓ B capsule 完整+SHA256SUMS  ✓ C run.sh --check-only 可重放  "
      f"✓ D CORE179 最小 victim+必要 aggressor（trials={out['total_trials']}）")
PYEOF

echo "== m3_drill: 全部断言通过（M3 退出标准演练 PASS）=="

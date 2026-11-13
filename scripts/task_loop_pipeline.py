#!/usr/bin/env python3
"""task_loop_pipeline.py — 夜间全自动流水线编排器(spec 2026-11-12-full-auto-pipeline)。

cron 0 23 启动: 窗口循环(每 PIPELINE_CYCLE_SEC 一轮) → 三泳道并行派发
(issue 修复/pr 检视/意见修复, 各 SLOT_KINDS 个槽) + audit 后台线 + merge 门
(引擎 PR 全门槛自动 squash merge; docs-only 快速通道) + 精确归因 revert。
09:00 软停: 不再派新 → 等在跑 ≤SOFT_STOP_GRACE_SEC → 击杀(不计 attempts,
条目回队列) → 清 worktree → 写夜间报告。设计细节一律见 spec,本文件不重述。
"""
# SPDX-License-Identifier: Apache-2.0
import collections
import fcntl
import os
import re
import shlex
import subprocess
import sys
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import task_loop as tl  # noqa: E402  (与 tl.pr_loop 同款命名空间引用,测试可 monkeypatch)
from task_loop import GhError, StateStore  # noqa: E402

PIPELINE_LOCK_NAME = "pipeline.lock"
NO_AUTO_MERGE_NAME = "NO_AUTO_MERGE"
SLOT_KINDS = {"issue_fix": 2, "pr_review": 2, "pr_repair": 2, "audit": 1}
PIPELINE_CYCLE_SEC = 300
SOFT_STOP_GRACE_SEC = 1800
REVIEW_AGENT_TIMEOUT_SEC = 5400
REVIEW_PER_HEAD_PER_NIGHT = 2
DOCS_QUIET_SEC = 300
MEMINFO_PATH = "/proc/meminfo"


def min_mem_gb() -> int:
    """内存闸阈值(GB)——env PIPELINE_MIN_MEM_GB 调用时读(测试可 setenv)。"""
    try:
        return int(os.environ.get("PIPELINE_MIN_MEM_GB", "8"))
    except ValueError:
        return 8


def _cgroup_mem_limit_mb() -> "int | None":
    """cgroup 内存限额(MB);无限制/不可读 → None(不参与取小)。"""
    for path in ("/sys/fs/cgroup/memory.max",           # cgroup v2 统一层级
                 "/sys/fs/cgroup/memory/memory.limit_in_bytes"):  # v1
        try:
            with open(path) as f:
                raw = f.read().strip()
        except OSError:
            continue
        if raw in ("", "max"):
            return None
        v = int(raw)
        if v > (1 << 40):          # v1 时常给 2^63 量级哨兵 = 无限制
            return None
        return v // (1024 * 1024)
    return None


def mem_available_mb() -> int:
    """MemAvailable(MB) 与 cgroup 限额取小——两者任一吃紧都算吃紧。"""
    avail = None
    try:
        with open(MEMINFO_PATH) as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    avail = int(line.split()[1]) // 1024
                    break
    except OSError:
        return 0
    if avail is None:
        return 0
    limit = _cgroup_mem_limit_mb()
    return min(avail, limit) if limit is not None else avail


def mem_gate_ok() -> bool:
    """派发前内存闸: MemAvailable ≥ 阈值才放行新 agent(宁慢勿爆,spec §7)。"""
    return mem_available_mb() >= min_mem_gb() * 1024


def boot_id() -> str:
    """本机启动标识——槽位记录启动时 boot_id,收割时对比即知 agent 是否死于重启。"""
    with open("/proc/sys/kernel/random/boot_id") as f:
        return f.read().strip()


def in_window(now: datetime, start_hour: int = 23, end_hour: int = 9) -> bool:
    """流水线窗口(本地时间): 23:00–09:00,跨午夜区间。"""
    h = now.hour
    return h >= start_hour or h < end_hour


def _now_local_iso():
    return datetime.now().astimezone().isoformat(timespec="seconds")


def _slots(state, kind):
    return state.setdefault("slots", {}).setdefault(kind, [])


def slot_get(state, kind, key):
    for s in _slots(state, kind):
        if s["key"] == key:
            return s
    return None


def slot_acquire(state, kind, key, pid, pgid, log, worktree,
                 started_iso, boot):
    """占用一槽;同 key 互斥(同条目/同 PR 至多 1 agent)、满槽拒绝。"""
    live = [s for s in _slots(state, kind) if tl.agent_alive(s["pid"])]
    if slot_get(state, kind, key) is not None:
        return False
    if len(live) >= SLOT_KINDS.get(kind, 0):
        return False
    _slots(state, kind).append(
        {"key": key, "pid": pid, "pgid": pgid, "log": log,
         "worktree": worktree, "started_at": started_iso, "boot": boot})
    return True


def slot_release(state, kind, key):
    s = _slots(state, kind)
    state["slots"][kind] = [x for x in s if x["key"] != key]


def free_slot_count(state, kind):
    """空槽数 = 槽上限 − 活 PID 占用数(死槽即时回收)。"""
    live = sum(1 for s in _slots(state, kind) if tl.agent_alive(s["pid"]))
    return max(0, SLOT_KINDS.get(kind, 0) - live)


def slot_janitor(state, now_iso):
    """清死槽并回收其 worktree;返回被清清单(boot 变化 → environmental=True,
    供启动对账做 attempts 豁免;软停/正常退场走 slot_release 不经此)。"""
    released = []
    for kind in SLOT_KINDS:
        alive = []
        for s in _slots(state, kind):
            if tl.agent_alive(s["pid"]):
                alive.append(s)
                continue
            released.append({"kind": kind, **s,
                             "environmental": s.get("boot") != boot_id()})
            if s.get("worktree"):
                tl.worktree_cleanup(s["worktree"])
        state["slots"][kind] = alive
    return released


def startup_reconcile(state):
    """pipeline 启动对账: 重启(boot_id 变)期间死亡的 agent 条目干净回队、
    不计 attempts(#611 attempts=4 假性升级教训);其余死槽只清槽,条目判档
    仍交 tl.reap_or_advance(单飞语义外的正常 dead/timeout 记账)。"""
    entries = None
    for info in slot_janitor(state, _now_local_iso()):
        if not info["environmental"]:
            continue
        if entries is None:
            # StateStore.entries 每次 from_dict 出新副本——只解析一次,
            # 推进在循环外统一回写,否则下一轮解析即丢。
            entries = tl.StateStore.entries(state)
        for e in entries:
            keys = (f"issue-{e.issue_number or e.run_id}",)
            if e.pr_number is not None:
                keys = keys + (f"pr-{e.pr_number}",)
            if info["key"] in keys and e.status in tl.AGENT_STATES:
                e.status = "PR_OPEN" if e.status == "BOT_FIX" else "QUEUED"
                e.agent_pid = None
                e.agent_started_at = None
                e.last_dispatched_head = None
                e.updated_at = _now_local_iso()
                print(f"[reconcile] {info['key']} 死于重启(boot 变化)——"
                      f"干净回队 {e.status},不计 attempts")
    if entries is not None:
        # Entry 是解析副本,推进须 to_dict 序列化回 state dict(与
        # tl.reap_or_advance 同款 write-back),否则下次 entries 拿回旧值。
        state["entries"] = [e.to_dict() for e in entries]


# ============ 泳道2 检视派发器(Task 6) ============


def reset_review_tries(state):
    """每晚边界: pipeline 启动时清零检视尝试计数——「每晚每 (pr,head) ≤2
    次」的"夜"以流水线启动对账为界(spec §4 泳道2 边界列)。"""
    state["review_tries"] = {}


def dispatch_review(state, pr_num, head):
    """泳道2 派发: 每晚每 (pr,head) ≤REVIEW_PER_HEAD_PER_NIGHT 次;满槽/内存
    不足返回 None。spawn 命令与修复 agent 同形(setsid+timeout),凭据经 agent.env。"""
    if not mem_gate_ok():
        return None
    tries = state.setdefault("review_tries", {})
    tk = f"{pr_num}:{head}"
    if tries.get(tk, 0) >= REVIEW_PER_HEAD_PER_NIGHT:
        print(f"[review] PR #{pr_num} head {head[:8]} 已试 {tries[tk]} 次/晚,"
              f"留待明晚")
        return None
    key = f"pr-{pr_num}"
    # R3(控制器裁定): 同 PR 重试先释放自身 key 的死槽——janitor 每轮才跑,
    # 不自清则同 key 互斥把「agent 已死、本可重派」的 PR 卡到下一轮
    # (释放只看本 key,不破坏「先 janitor 后派发」的全局清理序)。
    existing = slot_get(state, "pr_review", key)
    if existing is not None and not tl.agent_alive(existing["pid"]):
        slot_release(state, "pr_review", key)
    if free_slot_count(state, "pr_review") <= 0:
        return None
    ts = datetime.now().strftime("%Y%m%dT%H%M%S")
    log = os.path.join(tl._resolve_state_dir(), f"review-pr{pr_num}-{ts}.log")
    inner = (f"source {shlex.quote(os.path.join(tl._resolve_state_dir(), 'agent.env'))}"
             f" && timeout {REVIEW_AGENT_TIMEOUT_SEC} /usr/bin/python3 "
             f"{shlex.quote(os.path.join(tl._resolve_clone_dir(), 'scripts', 'pr_local_review.py'))}"
             f" --pr {pr_num}")
    cmd = f"nohup setsid bash -c {shlex.quote(inner)} >> {shlex.quote(log)} 2>&1 & echo $!"
    pid = tl.run_background(cmd, cwd=tl._resolve_clone_dir(), log=log)
    if not slot_acquire(state, "pr_review", key, pid, pid, log, None,
                        _now_local_iso(), boot_id()):
        # 占位失败只剩同轮并发竞态(同 key 早闸已拦常态)——killpg 整组击杀
        # 刚派生的 agent 不留孤儿(setsid 使 $! PID 即 pgid),且不烧每晚
        # 预算(tries 未增,击杀不是一次检视尝试)。
        try:
            os.killpg(pid, 9)
        except OSError:
            pass
        return None
    tries[tk] = tries.get(tk, 0) + 1
    return pid


# ============ 泳道3 merge 门(Task 7) ============

GateResult = collections.namedtuple("GateResult", "ok fast reasons")

# 必需 check 常量(裁定#1 校准,2026-11-13 对现役 PR #661/#660/#643/#642
# `gh pr view --json statusCheckRollup` 实测四 PR 完全一致): rollup 的
# CheckRun 实名是 lint / git-sanity / Build binary for CPU (Linux) /
# zizmor (workflow security)——分支保护 UI 建议集的写法「Security /
# zizmor」「Build binary … (GCC-arm64, -arm, unittests, selftests)」在
# rollup 不出现;checks_ok 按名字前缀匹配,常量写主干即可(矩阵后缀形态
# 由前缀覆盖;zizmor 的状态上下文裸名「zizmor」同被主干覆盖)。
REQUIRED_CHECKS_FULL = ["lint", "git-sanity",
                        "Build binary for CPU (Linux)", "zizmor"]
REQUIRED_CHECKS_DOCS = ["lint", "git-sanity"]
ENGINE_BRANCH_RE = re.compile(r"^fix/(issue-\d+|ci-\d+|revert-pr\d+)")
BASELINE_MERGED_DEPTH = 3


def docs_only(files):
    """docs-only 判定委托 tl._docs_only(T5 单一真源——同一判定同时喂
    max_rounds_for 轮次上限与本门快道,口径必须同源不漂移)。"""
    return tl._docs_only(files)


def wip_gated(title):
    """用户 WIP 闸(spec §5 docs 例外): WIP: 前缀 = 显式不同意当夜合入,
    去掉前缀即显式同意;只拦 docs 快道(引擎 PR 走 classify_state 五 bot)。"""
    return bool(title) and title.startswith("WIP:")


def baseline_reds(slug, workflow):
    """继承红基线(controller 裁定 2026-11-13,fix-round 1 Important#2 重写):
    最近 BASELINE_MERGED_DEPTH 个已合入 PR 的 rollup 中,同 workflowName 的
    红 job 名并集——贴 PR #426/#343「同基底已合入 PR 对照」先例的实操化。
    原 main-run 形态(--event schedule,push)对 PR CI workflow("Pull
    Request",lint/build 所在)结构性为空: 该 workflow 仅 pull_request 事件
    跑在 PR head,main 上无对应 run,继承红永不放行、特性死亡。拉不到/
    空 → 空集(不放行任何红,宁严勿松);单个基线 PR 拉取失败跳过不炸整链。
    PR 号经 str().split() 归一: gh --jq '.[].number' 多值输出多行裸串,
    单值经 gh_json 的 json.loads 得 int。"""
    try:
        nums = tl.gh_json_idem(["pr", "list", "--repo", slug, "--state",
                                "merged", "--limit", str(BASELINE_MERGED_DEPTH),
                                "--json", "number", "--jq", ".[].number"])
    except GhError:
        return set()
    jobs = set()
    for n in [x.strip() for x in str(nums or "").split() if x.strip()]:
        try:
            rollup = tl.gh_json_idem(
                ["pr", "view", n, "--repo", slug, "--json",
                 "statusCheckRollup", "--jq",
                 '.statusCheckRollup[] | [.workflowName, .name, .conclusion] | @tsv'])
        except GhError:
            continue
        for line in str(rollup or "").splitlines():
            parts = line.split("\t")
            if len(parts) < 3:
                continue
            if parts[0].strip() == workflow and \
                    parts[2].strip().lower() == "failure":
                jobs.add(parts[1].strip())
    return jobs


def checks_ok(pr, required, slug):
    """PR 的 check rollup: required 集(按名字前缀匹配,主干覆盖矩阵后缀)
    全绿 → True;红 job 按 rollup 行自带的 workflowName 分组对照已合入
    PR 基线(裁定#2 数据流: PRData 无 workflow_name 字段,红 job 自证所属
    workflow,evaluate_merge_gate 不传 pr.workflow_name;基线 = baseline_reds
    最近 BASELINE_MERGED_DEPTH 个已合入 PR 同 workflowName 的红 job 并集),
    全组 ⊆ 基线 → 继承红放行(spec §5 门槛 3)。结论按小写归一——实测
    rollup 结论是大写 SUCCESS/SKIPPED/FAILURE;SKIPPED 不算红(docs-only PR
    的重 job 常见)。
    """
    try:
        rollup = tl.gh_json_idem(
            ["pr", "view", str(getattr(pr, "pr_number", None)
                               or getattr(pr, "number", None)),
             "--repo", slug, "--json", "statusCheckRollup", "--jq",
             ".statusCheckRollup[] | [.name, .conclusion, .workflowName] | @tsv"])
    except GhError as exc:
        return False, [f"check rollup 拉取失败: {exc}"]
    conclusions = {}
    failed_by_wf = {}       # workflowName → 红 job 名集合(继承红分组单位)
    for line in (rollup or "").splitlines():   # gh 空输出 None → 零 check → 缺席拒
        if not line.strip():
            continue
        name, _, rest = line.partition("\t")
        concl, _, workflow = rest.partition("\t")
        name = name.strip()
        conclusions[name] = concl.strip()
        if concl.strip().lower() not in ("success", "skipped"):
            failed_by_wf.setdefault(workflow.strip(), set()).add(name)
    missing = [r for r in required
               if not any(n.startswith(r) for n in conclusions)]
    if missing:
        return False, [f"required check 缺席: {missing}"]
    if not failed_by_wf:
        return True, []
    evidence_ok, evidence_bad = [], []
    for workflow, jobs in sorted(failed_by_wf.items()):
        base = baseline_reds(slug, workflow)
        wf_label = workflow or "<无 workflow>"
        if jobs <= base:
            evidence_ok.append(
                f"[{wf_label}] {sorted(jobs)} ⊆ 已合入 PR 基线红 {sorted(base)}")
        else:
            evidence_bad.append(
                f"[{wf_label}] {sorted(jobs - base)} 不在已合入 PR 基线红 "
                f"{sorted(base)}")
    if evidence_bad:
        return False, [f"红 job 非基线继承: {'; '.join(evidence_bad)}"]
    return True, [f"继承红放行: {'; '.join(evidence_ok)}"]


def docs_ready(pr, now):
    """docs 快道就绪(门槛 2 的 docs 例外,spec §5 决策表 #9): 第 5 bot
    (local-claude)信号在 ∧ 静默窗 DOCS_QUIET_SEC(900→300)过 ∧ 零可行动
    意见(annotations 完整性/超容差意见同 classify_state 口径)——外部
    4 bot 事件驱动照跑,不等待。"""
    if pr.bot_done.get("local-claude") is None:
        return False
    if pr.annotations_fetch_failed or pr.pending_skew_opinions > 0 \
            or pr.opinion_count > 0:
        return False
    if pr.latest_signal is None:
        return False
    return (now - pr.latest_signal).total_seconds() >= DOCS_QUIET_SEC


def evaluate_merge_gate(state, entry, pr, files, now, no_auto_merge):
    """六门槛全查(spec §5),只判不执行(执行是 execute_merge,T8)。任一
    拒即 return,reasons 携带证据(审计评论/夜间报告用);熔断(PAUSED/
    NO_AUTO_MERGE)前置到入口——零 gh 成本,夜间熔断时省全部后续 gh
    (fix-round 1 Minor #4);entry=None 时仅 docs 快道可过(用户 docs PR,
    用户非 docs PR 结构性排除);no_auto_merge 由 T8 的 no_auto_merge_active()
    检测后传入(裁定#4,熔断存在 → 纯报告模式=灰度)。mergeable 经 getattr
    容错(真实 PRData 无该字段,T8 接线时经 gh pr view 补,缺字段视为未报
    冲突不炸门);fork 是 PRData 既有字段(fix-round 1 Critical #1 补拒)。"""
    fast = docs_only(files)
    # 门槛 5(前置): 熔断(PAUSED / NO_AUTO_MERGE——任一存在即转纯报告模式)
    if tl._paused():
        return GateResult(False, fast, ["PAUSED 生效"])
    if no_auto_merge:
        return GateResult(False, fast, ["NO_AUTO_MERGE 熔断(灰度: 只报告)"])
    # 门槛 1: 归属(引擎条目 ∧ fix/issue-N|fix/ci-N|fix/revert-prN 分支;
    # docs-only 例外: 不要求引擎归属,适用任意 PR)
    if entry is None:
        if not fast:
            return GateResult(False, False, ["非引擎 PR 且非 docs-only——不自动 merge"])
    else:
        if not ENGINE_BRANCH_RE.match(entry.fix_branch or ""):
            return GateResult(False, False, [f"非引擎分支: {entry.fix_branch}"])
    # 门槛 4: 状态(open ∧ 非 DRAFT ∧ 非 fork ∧ 无冲突;WIP 草稿闸只对
    # docs 快道——fork PR 不自动 merge,docs 快道亦不放行)
    if pr.state != "open" or pr.draft:
        return GateResult(False, fast, ["非 open/DRAFT"])
    if fast and wip_gated(pr.title):
        return GateResult(False, True, [f"WIP: 草稿闸({pr.title!r})"])
    if getattr(pr, "fork", False):
        return GateResult(False, fast, ["fork PR——不自动 merge"])
    if getattr(pr, "mergeable", None) is False:
        return GateResult(False, fast, ["与 main 冲突(mergeable=false)"])
    # 门槛 2: 检视(docs 快道 = 第5bot 唯一+300s;全门槛 = classify_state
    # 五 bot READY_TO_MERGE)
    if fast:
        if not docs_ready(pr, now):
            return GateResult(False, True, ["docs 快道未就绪(第5bot/静默窗/意见)"])
    else:
        st_class = tl.pr_loop.classify_state(pr, tl.pr_loop.DEFAULT_QUIET, now)
        if st_class != "READY_TO_MERGE":
            return GateResult(False, False, [f"classify={st_class}≠READY_TO_MERGE"])
    # 门槛 3: CI check(docs 只查快 job;红 job 继承红基线对照在 checks_ok 内)
    ok, why = checks_ok(pr, REQUIRED_CHECKS_DOCS if fast
                        else REQUIRED_CHECKS_FULL, tl.repo_slug())
    if not ok:
        return GateResult(False, fast, why)
    return GateResult(True, fast, why)      # why=继承红放行的证据,审计用


# ============ 泳道3 merge 执行 + 记账(Task 8) ============


def fill_mergeable(pr) -> None:
    """补 PRData 缺失的 mergeable 字段(公开接缝,裁定#1): 真实 PRData
    无该字段,调用方(T10 merge_sweep)过门前先过本函数,门的 mergeable
    False 拒臂才有数据。缺/None 才拉(有值不重复拉);GhError → 保持
    None 并 [warn];gh 空输出 None → 保持 None——门按 T7 过渡语义
    (None 不拒)放行,接线遗漏由 gate 测试锁形。PR 号经双形解析
    pr_number|number(与 execute_merge 同款,fix-round 1 Minor#1——
    同文件同一对象不得两种取号形态)。"""
    if getattr(pr, "mergeable", None) is not None:
        return
    slug = tl.repo_slug()
    num = getattr(pr, "pr_number", None) or getattr(pr, "number", None)
    try:
        v = tl.gh_json_idem(["pr", "view", str(num), "--repo", slug,
                              "--json", "mergeable", "--jq", ".mergeable"])
    except GhError as exc:
        print(f"[warn] PR #{num} mergeable 拉取失败"
              f"(保持 None,门按过渡语义不拒): {exc}")
        return
    if v is None:
        return
    pr.mergeable = v


def no_auto_merge_active() -> bool:
    """NO_AUTO_MERGE 熔断探测(STATE_DIR/NO_AUTO_MERGE 存在即 True): 纯
    存在性探查——幂等、零 gh、零状态变更。门只收 no_auto_merge: bool,
    由编排器调本函数传入(裁定#4: 熔断存在 → 门转纯报告模式=灰度)。"""
    return os.path.exists(
        os.path.join(tl._resolve_state_dir(), NO_AUTO_MERGE_NAME))


def execute_merge(state, entry, pr, gate, dry_run=False) -> "str | None":
    """过门后执行 squash merge: PR 评论贴门槛证据 → `gh pr merge --squash
    --delete-branch` → 回读新 main sha 记 entry.merged_at/merged_sha
    (revert 归因主数据,人工 merge 永不写)→ 引擎条目 DONE + issue 收尾
    评论/关闭(entry 为 None 的用户 docs PR 只 merge+评论)。merge 执行权
    在编排器代码,绝不在 LLM(spec §1 决策 7)。失败语义按不可逆点(merge
    本身)分界(fix-round 1 Important #1/3): 前侧评论/merge 步 GhError
    原样上抛——条目不置 DONE,下轮门复评;后侧两类例外都不上抛——sha
    回读失败/空降级为 merged_sha=None + [warn](已合入是事实,记账反映
    现实,崩溃不撤销 merge 只丢账),issue close 失败被 [warn] 吞(state
    为权威)。PR 号经双形解析 pr_number|number(真实 PRData 字段名是
    number,与 checks_ok/T5 _pr_number 同款契约)。"""
    slug = tl.repo_slug()
    num = getattr(pr, "pr_number", None) or getattr(pr, "number", None)
    evidence = "\n".join(f"- {r}" for r in gate.reasons) or "- 全门槛通过"
    body = (f"🤖 task-loop pipeline 自动 merge(spec 2026-11-12):\n"
            f"- 门槛证据:\n{evidence}\n"
            f"- 方式: squash + delete-branch;"
            f"{'docs-only 快速通道' if gate.fast else '全门槛'}")
    if dry_run:
        print(f"[dry-run] 将 merge PR #{num}: {body[:120]}…")
        return None
    tl.gh_json_idem(["pr", "comment", str(num), "--repo", slug,
                     "--body", body])
    tl.gh_json_idem(["pr", "merge", str(num), "--repo", slug,
                     "--squash", "--delete-branch"])
    raw = None
    try:
        raw = tl.gh_json_idem(
            ["pr", "view", str(num), "--repo", slug, "--json",
             "mergeCommit", "--jq", ".mergeCommit.oid"])
    except GhError as exc:
        print(f"[warn] PR #{num} mergeCommit 回读失败"
              f"——已合入但 merged_sha 缺: {exc}")
    merged_sha = (raw or "").strip() or None
    if merged_sha is None:
        print(f"[warn] PR #{num} 已 squash merge 但 sha 回读为空"
              f"——merged_sha=None(revert 归因不可见该 PR)")
    if entry is not None:
        entry.merged_at = _now_local_iso()
        entry.merged_sha = merged_sha
        entry.status = "DONE"
        entry.updated_at = _now_local_iso()
        if entry.issue_number is not None:
            tl.comment_on_issue(
                entry.issue_number,
                f"修复 PR #{num} 已由 pipeline 自动 squash merge"
                f"(main {(merged_sha or '?')[:12]})。本 issue 闭环。")
            try:
                tl.gh_json_idem(["issue", "close", str(entry.issue_number),
                                 "--repo", slug])
            except GhError as exc:
                print(f"[warn] issue 关闭失败(state 为权威): {exc}")
    print(f"[merge] PR #{num} 已自动合入 → {(merged_sha or '?')[:12]}")
    return merged_sha

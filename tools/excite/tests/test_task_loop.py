"""task_loop 统一引擎测试(PR-1b 起): schema v2 / 状态归一 / 目录迁移。"""
import sys, os, json
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "scripts"))
import task_loop as tl


def test_entry_from_dict_v1_normalized():
    """v1 state(旧状态名、无 source/kind/audit_hash)加载归一为 v2。"""
    d = {"workflow": "Sanitizer", "run_id": 123, "status": "DIAGNOSING",
         "issue_number": 9, "fix_branch": "fix/ci-123", "pr_number": None,
         "attempts": 1, "rounds": 0, "agent_pid": 42, "agent_started_at": None,
         "agent_log": None, "last_dispatched_head": None, "extra_runs": [],
         "created_at": "2026-10-01T00:00:00Z", "updated_at": "2026-10-01T00:00:00Z"}
    e = tl.Entry.from_dict(d)
    assert e.status == "DISPATCHED"
    assert e.source == "ci" and e.kind == "bug" and e.audit_hash is None


def test_entry_roundtrip_keeps_v2_fields():
    e = tl.Entry(workflow="", run_id=0, status="QUEUED", issue_number=7,
                 fix_branch="", pr_number=None, attempts=0, rounds=0,
                 agent_pid=None, agent_started_at=None, agent_log=None,
                 last_dispatched_head=None, extra_runs=[], created_at="t",
                 updated_at="t", source="session", kind="feature",
                 audit_hash=None)
    assert tl.Entry.from_dict(e.to_dict()) == e


# ============ Task 3: state 目录迁移 + env 双轨 ============

def test_state_dir_env_priority_and_default(tmp_path, monkeypatch):
    monkeypatch.delenv("TASKLOOP_STATE_DIR", raising=False)
    monkeypatch.delenv("CITRIAGE_STATE_DIR", raising=False)
    monkeypatch.setattr(tl, "DEFAULT_STATE_DIR", str(tmp_path / "new"))
    assert tl._resolve_state_dir() == str(tmp_path / "new")
    monkeypatch.setenv("CITRIAGE_STATE_DIR", str(tmp_path / "old"))
    assert tl._resolve_state_dir() == str(tmp_path / "old")
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "win"))
    assert tl._resolve_state_dir() == str(tmp_path / "win")


def test_migrate_legacy_state_dir_idempotent(tmp_path, monkeypatch):
    legacy = tmp_path / "legacy"; new = tmp_path / "new"
    legacy.mkdir(); new.mkdir()
    (legacy / "state.json").write_text('{"entries": [], "auth_failure_streak": 0}')
    (legacy / "agent.env").write_text("# env\n")
    monkeypatch.setattr(tl, "LEGACY_STATE_DIR", str(legacy))
    monkeypatch.setattr(tl, "DEFAULT_STATE_DIR", str(new))
    monkeypatch.delenv("TASKLOOP_STATE_DIR", raising=False)
    monkeypatch.delenv("CITRIAGE_STATE_DIR", raising=False)
    tl._migrate_legacy_state_dir()
    assert (new / "state.json").exists() and (new / "agent.env").exists()
    assert (legacy / "state.json").exists()          # 旧目录保留归档
    tl._migrate_legacy_state_dir()                    # 复跑零副作用
    assert (new / "state.json").read_text() == '{"entries": [], "auth_failure_streak": 0}'


def test_paused_checks_both_paths(tmp_path, monkeypatch):
    monkeypatch.setattr(tl, "LEGACY_STATE_DIR", str(tmp_path / "l"))
    monkeypatch.setattr(tl, "DEFAULT_STATE_DIR", str(tmp_path / "n"))
    (tmp_path / "l").mkdir(); (tmp_path / "l" / "PAUSED").touch()
    assert tl._paused() is True                       # 过渡期旧 PAUSED 也生效


# ============ Task 4: gh label 原语 + 幂等写重试 ============

class FakeGh:
    def __init__(self, fail_transient=0):
        self.calls = []; self._fail = fail_transient
    def gh_json(self, args, input_text=None):
        self.calls.append((tuple(args), input_text))
        if self._fail > 0:
            self._fail -= 1
            raise tl.GhError("gh api … 失败(exit 1): HTTP 500")
        if args[0] == "issue" and args[1] == "create":
            return "https://github.com/x/y/issues/42"
        return None

def test_gh_json_idem_retries_transient(tmp_path, monkeypatch):
    fake = FakeGh(fail_transient=2)
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    tl.gh_json_idem(["issue", "comment", "1", "--body", "x"])   # 3 次尝试内成功
    assert len(fake.calls) == 3

def test_gh_json_idem_no_retry_on_4xx(monkeypatch):
    fake = FakeGh(fail_transient=0)
    def always_4xx(args, input_text=None):
        fake.calls.append(tuple(args))
        raise tl.GhError("gh api … 失败(exit 1): HTTP 404")
    monkeypatch.setattr(tl, "gh_json", always_4xx)
    try:
        tl.gh_json_idem(["issue", "comment", "1", "--body", "x"])
        assert False, "4xx 必须首次即抛"
    except tl.GhError:
        assert len(fake.calls) == 1

def test_create_task_issue_labels_and_marker(monkeypatch):
    fake = FakeGh()
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    n = tl.create_task_issue("t", "body", ["agent-task", "agent-bug"])
    assert n == 42
    args, body = fake.calls[0]
    assert "--label" in args and "agent-task" in args


# ============ Task 5: intake_session CLI ============

def test_enqueue_bug_creates_labeled_issue(tmp_path, monkeypatch, capsys):
    fake = FakeGh()
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    bf = tmp_path / "b.md"; bf.write_text("正文")
    rc = tl.main(["enqueue-bug", "--title", "t", "--body-file", str(bf)])
    assert rc == 0
    args, body = fake.calls[0]
    assert "agent-task" in args and "agent-bug" in args and "bug" in args
    assert tl.UNTRUSTED_BANNER in body

def test_enqueue_bug_findings_file_does_not_read_stdin(tmp_path, monkeypatch,
                                                       capsys):
    """PR#317 检视#9: --findings-file 路径正文完全不用,不得先读 stdin——
    规范用法(dev-loop.md)恰是不带 --body-file 的裸调用,人工在终端执行
    会永久挂起等 stdin。"""
    class _Boom:
        def read(self, *a):
            raise AssertionError(
                "enqueue-bug --findings-file 不得读 stdin(人工终端会挂起)")
    monkeypatch.setattr(tl.sys, "stdin", _Boom())
    fake = FakeGh()
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    ff = tmp_path / "f.json"
    ff.write_text(json.dumps([
        {"path": "a.c", "line": 1, "severity": "Minor", "category": "正确性",
         "message": "m1", "evidence": "e1"},
        {"path": "b.c", "line": 2, "severity": "Minor", "category": "性能",
         "message": "m2", "evidence": "e2"}]))
    rc = tl.main(["enqueue-bug", "--findings-file", str(ff)])
    assert rc == 0
    created = [c for c in fake.calls if c[0][:2] == ("issue", "create")]
    assert len(created) == 2                       # 一条 finding 一个 issue
    bodies = [c[1] for c in created]
    assert any("m1" in b for b in bodies) and tl.UNTRUSTED_BANNER in bodies[0]

def test_enqueue_bug_findings_dedup_via_marker(tmp_path, monkeypatch, capsys):
    """PR#317 检视#16(轮2): session findings 批量路径幂等——每条 finding
    以 _audit_hash 归一化 hash 嵌 body marker(<!-- task-loop:finding:h -->),
    gh search 命中 open issue 即跳过;中途瞬态失败后重跑不重复建 issue
    (对照: ci 源有 RUN_MARKER_FMT、audit 源有双源查重,session 源原为裸奔)。"""
    class _Boom:
        def read(self, *a):
            raise AssertionError("findings 路径不得读 stdin")
    monkeypatch.setattr(tl.sys, "stdin", _Boom())
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    f1 = {"path": "a.c", "line": 1, "severity": "Minor", "category": "正确性",
          "message": "m1", "evidence": "e1"}
    f2 = {"path": "b.c", "line": 2, "severity": "Minor", "category": "性能",
          "message": "m2", "evidence": "e2"}
    h1, h2 = tl._audit_hash(f1), tl._audit_hash(f2)
    # 重跑场景: f2 的 marker 已命中既有 open issue(上一轮已建)
    hit_bodies = {h2: tl.FINDING_MARKER_FMT.format(hash=h2)}
    created = []
    def fake_gh(args, input_text=None):
        if args[:2] == ["search", "issues"]:
            q = args[-1]
            for h, marker in hit_bodies.items():
                if q == f"task-loop:finding:{h}":
                    return [{"number": 4242, "title": "t", "body": marker}]
            return []
        if args[:2] == ["issue", "create"]:
            created.append((args, input_text))
            return "https://github.com/x/y/issues/42"
        return None
    monkeypatch.setattr(tl, "gh_json", fake_gh)
    ff = tmp_path / "f.json"; ff.write_text(json.dumps([f1, f2]))
    assert tl.main(["enqueue-bug", "--findings-file", str(ff)]) == 0
    assert len(created) == 1, "marker 命中的 finding 不得重复建 issue"
    args, body = created[0]
    assert tl.FINDING_MARKER_FMT.format(hash=h1) in body, \
        "新建 issue 正文须嵌 finding marker(供重跑查重)"
    assert tl.UNTRUSTED_BANNER in body
    out = capsys.readouterr().out
    assert "已见过" in out
    # 完整重跑: 两条全部命中 → 零建
    hit_bodies[h1] = tl.FINDING_MARKER_FMT.format(hash=h1)
    created.clear()
    ff2 = tmp_path / "f2.json"; ff2.write_text(json.dumps([f1, f2]))
    assert tl.main(["enqueue-bug", "--findings-file", str(ff2)]) == 0
    assert created == []

def test_enqueue_bug_empty_findings_file_is_noop(tmp_path, monkeypatch, capsys):
    """PR#317 检视#9 附: 空 findings 数组 = 零问题待入队——零 issue、零
    stdin 读取(旧实现会落入单条路径读 stdin 挂起/建占位 issue)。"""
    class _Boom:
        def read(self, *a):
            raise AssertionError("空 findings 也不得读 stdin")
    monkeypatch.setattr(tl.sys, "stdin", _Boom())
    fake = FakeGh()
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    ff = tmp_path / "empty.json"; ff.write_text("[]")
    assert tl.main(["enqueue-bug", "--findings-file", str(ff)]) == 0
    assert [c for c in fake.calls if c[0][:2] == ("issue", "create")] == []


def test_enqueue_task_requires_approval_hint(capsys, tmp_path, monkeypatch):
    fake = FakeGh()
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    bf = tmp_path / "b.md"; bf.write_text("正文")
    rc = tl.main(["enqueue-task", "--title", "t", "--body-file", str(bf)])
    assert rc == 0
    out = capsys.readouterr().out
    assert "agent-approved" in out          # 提示人工确认门

def test_approve_adds_label(monkeypatch):
    fake = FakeGh()
    monkeypatch.setattr(tl, "gh_json", fake.gh_json)
    assert tl.main(["approve", "42"]) == 0
    assert fake.calls[0][0][:4] == ("issue", "edit", "42", "--repo")


# ============ Task 6: 队列对账 reconcile_issues + HUMAN_CLOSED ============

def _labels(*names):
    return [{"name": n} for n in names]

def test_issue_eligible_matrix():
    f = tl.issue_eligible
    assert f(["agent-task", "agent-bug"]) is True
    assert f(["agent-task", "agent-approved"]) is True
    assert f(["agent-task", "agent-bug", "agent-approved"]) is True
    assert f(["agent-task"]) is False                    # 既非 bug 也未确认
    assert f(["agent-task", "agent-feature"]) is False   # feature 未确认
    assert f(["agent-task", "agent-bug", "agent-wip"]) is False
    assert f(["agent-task", "agent-bug", "agent-escalated"]) is False

def test_reconcile_creates_manual_entry_and_respects_state(monkeypatch):
    state = {"entries": [], "auth_failure_streak": 0}
    monkeypatch.setattr(tl, "gh_json", lambda args, input_text=None: [
        {"number": 3, "title": "t", "labels": _labels("agent-task", "agent-feature", "agent-approved")},
        {"number": 5, "title": "t", "labels": _labels("agent-task", "agent-bug")},
    ] if args[0] == "issue" and args[1] == "list" else None)
    tl.reconcile_issues(state, dry_run=False)
    es = tl.StateStore.entries(state)
    assert [(e.issue_number, e.source, e.kind) for e in es] == [(3, "manual", "feature"), (5, "manual", "bug")]
    assert all(e.status == "QUEUED" for e in es)
    # 复跑: state 已跟踪 → 零新建
    tl.reconcile_issues(state, dry_run=False)
    assert len(tl.StateStore.entries(state)) == 2

def test_reconcile_human_closed_sweep(monkeypatch):
    state = {"entries": [dict(
        workflow="", run_id=0, status="QUEUED", issue_number=7, fix_branch="",
        pr_number=None, attempts=0, rounds=0, agent_pid=None,
        agent_started_at=None, agent_log=None, last_dispatched_head=None,
        extra_runs=[], created_at="t", updated_at="t",
        source="manual", kind="bug", audit_hash=None)], "auth_failure_streak": 0}
    def fake_gh(args, input_text=None):
        if args[:2] == ["issue", "list"]:
            return []                       # open 列表已无 #7(被人工关闭)
        if args[:2] == ["issue", "view"]:
            return {"state": "closed"}
        return None
    monkeypatch.setattr(tl, "gh_json", fake_gh)
    tl.reconcile_issues(state, dry_run=False)
    e = tl.StateStore.entries(state)[0]
    assert e.status == "HUMAN_CLOSED" and e.agent_pid is None


# ============ Task 7: 任务派发契约(分支命名/prompt/stdin runner/wip 标签) ============

def _task_entry(**kw):
    base = dict(workflow="", run_id=0, status="QUEUED", issue_number=33,
                fix_branch="", pr_number=None, attempts=0, rounds=0,
                agent_pid=None, agent_started_at=None, agent_log=None,
                last_dispatched_head=None, extra_runs=[], created_at="t",
                updated_at="t", source="session", kind="bug", audit_hash=None)
    base.update(kw)
    return tl.Entry(**base)

def test_default_fix_branch():
    assert tl._default_fix_branch(_task_entry(source="ci", run_id=123,
                                              issue_number=9)) == "fix/ci-123"
    assert tl._default_fix_branch(_task_entry()) == "fix/issue-33"
    assert tl._default_fix_branch(_task_entry(kind="feature")) == "feat/issue-33"

def test_task_prompt_contract():
    p = tl.build_task_prompt(_task_entry())
    assert "issue #33" in p or "#33" in p
    assert "fix/issue-33" in p
    assert "Closes #33" in p                       # PR body 契约
    assert tl.TASK_DONE_MARK in p and tl.TASK_ESCALATED_MARK in p
    assert tl.UNTRUSTED_BANNER in p
    assert "未复现不修" in p                       # bug 红线
    f = tl.build_task_prompt(_task_entry(kind="feature"))
    assert "偏离需求即 escalated" in f             # feature 红线

def test_check_agent_outcome_accepts_both_marks(tmp_path):
    log = tmp_path / "a.log"; log.write_text("x\ntask-loop: done fix/issue-1\n")
    e = _task_entry(status="DISPATCHED", agent_log=str(log))
    assert tl.check_agent_outcome(e) == "success"
    log.write_text("ci-triage: escalated 复现不出\n")
    assert tl.check_agent_outcome(e) == "escalated_reported"

def test_dispatch_writes_prompt_file_and_wip_label(tmp_path, monkeypatch):
    state = {"entries": [], "auth_failure_streak": 0}
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    labels = []
    monkeypatch.setattr(tl, "gh_label_add",
                        lambda n, lb: labels.append((n, lb)))
    captured = {}
    def fake_run_background(cmd, cwd, log):
        captured["cmd"] = cmd
        return 4321
    monkeypatch.setattr(tl, "run_background", fake_run_background)
    e = _task_entry()
    pid = tl.dispatch_agent(e, state)
    assert pid == 4321 and e.status == "DISPATCHED" and e.fix_branch == "fix/issue-33"
    assert labels == [(33, "agent-wip")]
    # stdin 化: 命令里不再出现被引用的超长 prompt,而是 < prompt 文件
    assert "< " in captured["cmd"] and "claude -p" in captured["cmd"]
    prompt_files = list(tmp_path.glob("prompt-*"))
    assert len(prompt_files) == 1 and "Closes #33" in prompt_files[0].read_text()


# ============ PR#317 检视#6: 派发记账即时落盘(跨进程崩溃窗口) ============

def test_scan_persists_dispatch_accounting_before_lane_b(tmp_path, monkeypatch):
    """检视#6(Critical): 任务派发扫派成 agent 后,state 须在进入车道 B 之前
    就落盘——原实现唯一落盘点在 finally,其间隔着整条车道 B(ensure_local_review
    的本地 claude 审查可达 CLAUDE_TIMEOUT=3600s);进程一旦死亡,nohup 孤儿
    agent 仍在克隆里干活而磁盘 state 无 PID → 下一 tick 单飞失明 + heal_clone
    reset --hard 踩活 agent 工作区(生产事故 9d6ba452 同类根因)。"""
    from types import SimpleNamespace
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    (tmp_path / "state.json").write_text(json.dumps(
        {"entries": [dict(workflow="", run_id=0, status="QUEUED", issue_number=5,
                          fix_branch="", pr_number=None, attempts=0, rounds=0,
                          agent_pid=None, agent_started_at=None, agent_log=None,
                          last_dispatched_head=None, extra_runs=[],
                          created_at="t", updated_at="t",
                          source="manual", kind="bug", audit_hash=None)],
         "auth_failure_streak": 0}))
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    monkeypatch.setattr(tl, "gh_label_add", lambda n, lb: None)
    monkeypatch.setattr(tl, "reconcile_issues", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tl, "heal_clone", lambda s, dry_run: None)
    # 车道 B 入口 = 崩溃窗口探针: 此刻磁盘上有什么,进程死亡后下一 tick
    # 就只能看到什么(finally 的 save 在崩溃场景不会执行)。
    disk_at_lane_b = {}
    def fake_lane_b(state, dry_run):
        disk_at_lane_b["state"] = tl.StateStore(str(tmp_path)).load()
    monkeypatch.setattr(tl, "lane_b_advance", fake_lane_b)
    def fake_dispatch(entry, state, dry_run=False, prompt_override=None,
                      increment_rounds=False):
        entry.status = "DISPATCHED"; entry.agent_pid = os.getpid()
        return os.getpid()                      # 确定性真活 PID(探活必真)
    monkeypatch.setattr(tl, "dispatch_agent", fake_dispatch)
    assert tl._run_scan_orchestration(dry_run=False) == 0
    e = disk_at_lane_b["state"]["entries"][0]
    assert e["status"] == "DISPATCHED", "派发状态未在车道 B 前落盘"
    assert e["agent_pid"] == os.getpid(), "agent_pid 未在车道 B 前落盘(单飞失明窗口)"


def test_lane_b_persists_dispatch_accounting_immediately(tmp_path, monkeypatch):
    """检视#6(车道 B 半边): bot 处置 agent 派成后即时落盘——本函数返回后
    编排层还要跑其余条目才到 finally,期间崩溃同样丢 PID。"""
    from types import SimpleNamespace
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_REPO", "selftest/local")
    pr = SimpleNamespace(number=88, head_sha="b" * 40, url="https://x/88",
                         bot_done={"local-claude": "t"})
    monkeypatch.setattr(tl.pr_loop, "fetch_pr", lambda slug, num: pr)
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda p, quiet, now: "READY_TO_FIX")
    monkeypatch.setattr(tl.pr_loop, "render_collect",
                        lambda p, slug, quiet: "# collect 导出(fake)\n")
    monkeypatch.setattr(tl.pr_loop, "ensure_local_review",
                        lambda num, sha: None)
    monkeypatch.setattr(tl, "gh_label_add", lambda n, lb: None)
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: os.getpid())
    entry = _task_entry(status="PR_OPEN", pr_number=88, rounds=0,
                        last_dispatched_head=None)
    state = {"entries": [entry.to_dict()], "auth_failure_streak": 0}
    tl.lane_b_advance(state, dry_run=False)     # 调用方不 save,直接看磁盘
    e = tl.StateStore(str(tmp_path)).load()["entries"][0]
    assert e["status"] == "BOT_FIX" and e["agent_pid"] == os.getpid(), \
        "车道 B 派发记账未即时落盘"
    assert e["rounds"] == 1 and e["last_dispatched_head"] == "b" * 40


# ============ PR#317 检视#11: paused 横幅与闸门同口径(双路径) ============

def test_scan_paused_banner_honors_legacy_dir(tmp_path, monkeypatch, capsys):
    """检视#11: 过渡期旧目录 PAUSED 同样生效(dispatch_agent 闸门走
    _paused() 新旧双路径,手册 §5.1),scan 编排的全局熔断横幅不得只查新
    目录单路径——否则旧目录 touch PAUSED 时派发全被拒却零横幅,运维遥测
    与实际闸门状态矛盾。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    # 检视#19(轮2)后 _paused() 的 env 守卫: 重定向 run 不查 legacy——
    # 本用例锚"默认目录 run 尊重 legacy 双路径",故把 DEFAULT 也指到
    # tmp(非重定向场景),夹具对齐新契约而非放松断言。
    monkeypatch.setattr(tl, "DEFAULT_STATE_DIR", str(tmp_path))
    legacy = tmp_path / "legacy"; legacy.mkdir()
    (legacy / "PAUSED").touch()
    monkeypatch.setattr(tl, "LEGACY_STATE_DIR", str(legacy))
    (tmp_path / "state.json").write_text(json.dumps(
        {"entries": [dict(workflow="", run_id=0, status="QUEUED", issue_number=5,
                          fix_branch="", pr_number=None, attempts=0, rounds=0,
                          agent_pid=None, agent_started_at=None, agent_log=None,
                          last_dispatched_head=None, extra_runs=[],
                          created_at="t", updated_at="t",
                          source="manual", kind="bug", audit_hash=None)],
         "auth_failure_streak": 0}))
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    monkeypatch.setattr(tl, "reconcile_issues", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tl, "heal_clone", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "lane_b_advance", lambda s, dry_run: None)
    def fake_dispatch(entry, state, dry_run=False, prompt_override=None,
                      increment_rounds=False):
        # 镜像真实 dispatch_agent 的 PAUSED 闸门契约(闸门本体在 dispatch_agent
        # 内,走 _paused() 双路径);本测试锚的是编排层横幅与闸门口径一致
        if tl._paused():
            return None
        dispatched.append(entry.issue_number)
        return None
    dispatched = []
    monkeypatch.setattr(tl, "dispatch_agent", fake_dispatch)
    assert tl._run_scan_orchestration(dry_run=False) == 0
    assert dispatched == []                        # PAUSED: 零派发
    out = capsys.readouterr().out
    assert "[paused] PAUSED 熔断生效——跳过一切派发" in out, "全局横幅未随旧目录 PAUSED 触发"
    assert "[paused] 车道 A 派发被 PAUSED 闸门拦下" in out


# ============ PR#317 检视#12+#13: 标签协议=派发时控制面 + 清扫去 N+1 ============

def test_scan_dispatch_gate_freezes_ineligible(tmp_path, monkeypatch, capsys):
    """检视#12: 资格判定不能只是入队时刻的一次性闸门——人工对已入队 issue
    补 agent-escalated(冻结)/摘 agent-task(离开 open 列表)后,任务派发扫
    与重派扫都必须复查当前标签;资格正常的照常派发。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    def _e(n, status):
        return dict(workflow="", run_id=0, status=status, issue_number=n,
                    fix_branch="", pr_number=None, attempts=0, rounds=0,
                    agent_pid=None, agent_started_at=None, agent_log=None,
                    last_dispatched_head=None, extra_runs=[],
                    created_at="t", updated_at="t",
                    source="manual", kind="bug", audit_hash=None)
    (tmp_path / "state.json").write_text(json.dumps(
        {"entries": [_e(5, "QUEUED"), _e(6, "QUEUED"), _e(7, "DISPATCHED")],
         "auth_failure_streak": 0}))
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    monkeypatch.setattr(tl, "gh_label_add", lambda n, lb: None)
    # 对账返回当前 open+agent-task 标签快照: #5 人工补 escalated;#6 资格
    # 正常;#7 被人工摘 agent-task(不在 open 列表)——re派扫同受闸。
    monkeypatch.setattr(tl, "reconcile_issues", lambda s, dry_run: {
        5: ["agent-task", "agent-bug", "agent-escalated"],
        6: ["agent-task", "agent-bug"]})
    monkeypatch.setattr(tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tl, "heal_clone", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "lane_b_advance", lambda s, dry_run: None)
    dispatched = []
    def fake_dispatch(entry, state, dry_run=False, prompt_override=None,
                      increment_rounds=False):
        dispatched.append(entry.issue_number)
        entry.status = "DISPATCHED"; entry.agent_pid = os.getpid()
        return os.getpid()
    monkeypatch.setattr(tl, "dispatch_agent", fake_dispatch)
    assert tl._run_scan_orchestration(dry_run=False) == 0
    assert dispatched == [6], "冻结条目不得派发,资格正常条目照常派发"
    out = capsys.readouterr().out
    assert "[gate] issue #5" in out and "[gate] issue #7" in out
    saved = tl.StateStore(str(tmp_path)).load()
    by_n = {e["issue_number"]: e for e in saved["entries"]}
    assert by_n[5]["status"] == "QUEUED" and by_n[5]["agent_pid"] is None
    assert by_n[7]["status"] == "DISPATCHED" and by_n[7]["agent_pid"] is None

def test_reconcile_human_closed_sweep_no_nplus1(monkeypatch):
    """检视#13: HUMAN_CLOSED 清扫复用对账拉取的 open 列表——仍在列表的
    QUEUED/DISPATCHED 条目零 gh issue view(旧实现每条每 tick 一次、各带
    45s 超时且全程持 scan.lock);只有离开列表的才复核一次。"""
    def _e(n, status):
        return dict(workflow="", run_id=0, status=status, issue_number=n,
                    fix_branch="", pr_number=None, attempts=0, rounds=0,
                    agent_pid=None, agent_started_at=None, agent_log=None,
                    last_dispatched_head=None, extra_runs=[],
                    created_at="t", updated_at="t",
                    source="manual", kind="bug", audit_hash=None)
    state = {"entries": [_e(3, "QUEUED"), _e(4, "QUEUED"), _e(5, "QUEUED"),
                         _e(9, "DISPATCHED")],
             "auth_failure_streak": 0}
    views = []
    def fake_gh(args, input_text=None):
        if args[:2] == ["issue", "list"]:
            # open+agent-task 列表: #3/#4/#5 仍在;#9 已关闭不在
            return [{"number": n, "title": "t",
                     "labels": [{"name": "agent-task"},
                                {"name": "agent-bug"}]}
                    for n in (3, 4, 5)]
        if args[:2] == ["issue", "view"]:
            views.append(args[2])
            return {"state": "closed"}
        return None
    monkeypatch.setattr(tl, "gh_json", fake_gh)
    open_labels = tl.reconcile_issues(state, dry_run=False)
    assert views == ["9"], "open 列表内的条目零 issue view(去 N+1)"
    by_n = {e.issue_number: e for e in tl.StateStore.entries(state)}
    assert by_n[9].status == "HUMAN_CLOSED"      # 离开列表 → 复核一次 → 接管
    assert all(by_n[n].status == "QUEUED" for n in (3, 4, 5))
    assert open_labels == {3: ["agent-task", "agent-bug"],
                           4: ["agent-task", "agent-bug"],
                           5: ["agent-task", "agent-bug"]}, \
        "对账返回 open 标签快照供派发扫复用(一次 gh 调用,不加 N+1)"


# ============ PR#317 检视#14: dry-run 零本地写(复跑无痕兑现) ============

def test_dry_run_scan_zero_local_writes(tmp_path, monkeypatch):
    """检视#14: dry-run「复跑无痕」= 零本地写——原实现在判定 dry 之前就
    makedirs 状态目录并 O_CREAT scan.lock,与自身 docstring 及手册矛盾,
    且在部署机上提前建目录会掩盖 cron 重定向目录缺失类问题在演练期暴露
    的机会(检视#8 的发现路径被堵)。锁保护的是 state 写,dry-run 不写
    state,无需互斥。"""
    state_dir = tmp_path / "never-created"
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(state_dir))
    monkeypatch.setattr(tl, "LEGACY_STATE_DIR", str(tmp_path / "legacy"))
    monkeypatch.setattr(tl, "reconcile_issues", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tl, "heal_clone", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "lane_b_advance", lambda s, dry_run: None)
    assert tl._run_scan_orchestration(dry_run=True) == 0
    assert not state_dir.exists(), "dry-run 不得创建状态目录/锁文件"

def test_dry_run_audit_zero_local_writes(tmp_path, monkeypatch):
    """检视#14(audit 半边): run_audit 的 audit.lock/makedirs 同理只在非
    dry 时建立。"""
    state_dir = tmp_path / "never-created"
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(state_dir))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    monkeypatch.setattr(tl, "LEGACY_STATE_DIR", str(tmp_path / "legacy"))
    monkeypatch.setattr(tl, "_git", lambda args, clone_dir=None:
                        "same" if args[0] in ("rev-parse", "rev-list")
                        else "")
    assert tl.run_audit(dry_run=True) == 0
    assert not state_dir.exists(), "dry-run audit 不得创建状态目录/锁文件"


# ============ Task 8: scan 编排接入 + status ============

def test_scan_dispatches_task_fifo_respecting_single_flight(tmp_path, monkeypatch):
    """两个 QUEUED 任务,单飞 → 只派 FIFO 首个(issue 号小者)。"""
    state = {"entries": [
        dict(workflow="", run_id=0, status="QUEUED", issue_number=9,
             fix_branch="", pr_number=None, attempts=0, rounds=0,
             agent_pid=None, agent_started_at=None, agent_log=None,
             last_dispatched_head=None, extra_runs=[], created_at="t",
             updated_at="t", source="session", kind="bug", audit_hash=None),
        dict(workflow="", run_id=0, status="QUEUED", issue_number=5,
             fix_branch="", pr_number=None, attempts=0, rounds=0,
             agent_pid=None, agent_started_at=None, agent_log=None,
             last_dispatched_head=None, extra_runs=[], created_at="t",
             updated_at="t", source="manual", kind="bug", audit_hash=None)],
        "auth_failure_streak": 0}
    # 编排从 StateStore 装 state(brief 的用例只构造了局部 dict,从未注入)
    # ——先落盘进 tmp;env 重定向让 StateStore/scan.lock/落盘全在 tmp,
    # 绝不碰真实默认目录(夹具真设置,非注释括注)。
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    (tmp_path / "state.json").write_text(json.dumps(state))
    # 引擎缝全部离线(真实 scan 触网): 标签确保(Ruling: 非 dry 编排会真调
    # ensure_agent_labels → 真实 gh label 写,必须打桩)/派发/对账/扫描/
    # 自愈/车道 B。
    monkeypatch.setattr(tl, "ensure_agent_labels", lambda: None)
    monkeypatch.setattr(tl, "gh_label_add", lambda n, lb: None)
    dispatched = []
    def fake_dispatch(entry, state, dry_run=False, prompt_override=None,
                      increment_rounds=False):
        # Ruling 5: fake 自身镜像真实单飞契约(单飞属 dispatch_agent,不属
        # 编排层),且派成的 agent 用 os.getpid()(确定性真活 PID,探活必真;
        # 伪 PID 如 99 探活不确定,ci_triage 用例 12 同款手法)。
        if tl.has_running_agent(state):
            return None
        dispatched.append(entry.issue_number)
        entry.status = "DISPATCHED"; entry.agent_pid = os.getpid()
        return os.getpid()
    monkeypatch.setattr(tl, "dispatch_agent", fake_dispatch)
    monkeypatch.setattr(tl, "reconcile_issues", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tl, "heal_clone", lambda s, dry_run: None)
    monkeypatch.setattr(tl, "lane_b_advance", lambda s, dry_run: None)
    rc = tl._run_scan_orchestration(dry_run=False)
    assert rc == 0 and dispatched == [5]        # FIFO issue 号,单飞拦下第二个


def test_cmd_status_prints_queue_and_entries(tmp_path, monkeypatch, capsys):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    (tmp_path / "state.json").write_text(json.dumps({
        "entries": [dict(workflow="Sanitizer", run_id=1, status="PR_OPEN",
                         issue_number=9, fix_branch="fix/ci-1", pr_number=3,
                         attempts=1, rounds=0, agent_pid=None,
                         agent_started_at=None, agent_log=None,
                         last_dispatched_head=None, extra_runs=[],
                         created_at="t", updated_at="t", source="ci",
                         kind="bug", audit_hash=None)],
        "auth_failure_streak": 0}))
    monkeypatch.setattr(tl, "list_open_agent_issues", lambda: [])
    assert tl.main(["status"]) == 0
    out = capsys.readouterr().out
    assert "PR_OPEN" in out and "#9" in out


# ============ Task 9: audit 无人值守 main 增量对抗审查 ============

def test_audit_hash_stable_and_normalized():
    a = {"path": "a.c", "line": 1, "category": "正确性",
         "message": " x ", "evidence": "e1"}
    b = {"path": "a.c", "line": 1, "category": "正确性",
         "message": "x", "evidence": "DIFFERENT"}     # evidence 不进 hash
    assert tl._audit_hash(a) == tl._audit_hash(b)
    c = dict(a, line=2)
    assert tl._audit_hash(a) != tl._audit_hash(c)

def test_run_audit_dedups_and_advances_cursor(tmp_path, monkeypatch):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    state = {"entries": [], "auth_failure_streak": 0,
             "audit_cursor": {"sha": "base0000000000000000000000000000000000000000",
                              "at": "2026-10-29T00:00:00Z"}}
    store = tl.StateStore(str(tmp_path)); store.save(state)
    git_out = {"fetch": "", "rev-parse": "head1111111111111111111111111111111111111111",
               "log": "abc1234 commit one\ndef5678 commit two"}
    monkeypatch.setattr(tl, "_git", lambda args, clone_dir=None:
                        git_out.get(args[0], ""))
    monkeypatch.setattr(tl, "_git_diff", lambda base, head: "+diff body")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_add", lambda sha: "/tmp/wt")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_remove", lambda p: None)
    monkeypatch.setattr(tl.pr_local_review, "_run_claude",
                        lambda cmd, cwd, prompt: json.dumps([
                            {"path": "a.c", "line": 3, "severity": "Important",
                             "category": "正确性", "message": "m",
                             "evidence": "e"},
                            {"path": "a.c", "line": 3, "severity": "Important",
                             "category": "正确性", "message": "m",
                             "evidence": "e"}]))          # 同 finding ×2 → 去重为 1
    created = []
    monkeypatch.setattr(tl, "create_task_issue",
                        lambda title, body, labels:
                        created.append(body) or 42)
    monkeypatch.setattr(tl, "_search_audit_issue", lambda h: None)
    rc = tl.run_audit(dry_run=False)
    assert rc == 0 and len(created) == 1
    assert tl.AUDIT_MARKER_FMT.format(hash=tl._audit_hash(
        {"path": "a.c", "line": 3, "category": "正确性", "message": "m"})) in created[0]
    s2 = store.load()
    assert s2["audit_cursor"]["sha"].startswith("head1111")   # cursor 推进
    assert len(s2["audit_seen"]) == 1

def test_run_audit_no_new_commits_early_exit(tmp_path, monkeypatch):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    state = {"entries": [], "auth_failure_streak": 0,
             "audit_cursor": {"sha": "same", "at": "t"}}
    monkeypatch.setattr(tl, "_git", lambda args, clone_dir=None:
                        "same" if args[0] == "rev-parse" else "")
    assert tl.run_audit(dry_run=False) == 0    # 无新提交,零 claude 调用


# ============ PR#317 检视#7: 空输出/schema 漂移 ≠ 零 findings 成功审查 ============

def _audit_env(tmp_path, monkeypatch, claude_output):
    """run_audit 离线环境: git/claude/worktree 缝全 fake,claude 输出可注入。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    state = {"entries": [], "auth_failure_streak": 2,
             "audit_cursor": {"sha": "base0000000000000000000000000000000000000000",
                              "at": "2026-10-29T00:00:00Z"}}
    store = tl.StateStore(str(tmp_path)); store.save(state)
    git_out = {"fetch": "", "rev-parse": "head1111111111111111111111111111111111111111",
               "log": "abc1234 commit one"}
    monkeypatch.setattr(tl, "_git", lambda args, clone_dir=None:
                        git_out.get(args[0], ""))
    monkeypatch.setattr(tl, "_git_diff", lambda base, head: "+diff body")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_add", lambda sha: "/tmp/wt")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_remove", lambda p: None)
    monkeypatch.setattr(tl.pr_local_review, "_run_claude",
                        lambda cmd, cwd, prompt: claude_output)
    created = []
    monkeypatch.setattr(tl, "create_task_issue",
                        lambda title, body, labels: created.append(body) or 42)
    monkeypatch.setattr(tl, "_search_audit_issue", lambda h: None)
    return store, created

def test_audit_empty_output_does_not_advance_cursor(tmp_path, monkeypatch):
    """检视#7: claude 空输出被当「零 findings 成功」→ cursor 永久推进 +
    streak 被空跑清零。修复后: 空输出 = 审查失败(rc 1),cursor 不动、
    streak 不清零、零 issue,下轮重审同区间。"""
    store, created = _audit_env(tmp_path, monkeypatch, claude_output="")
    assert tl.run_audit(dry_run=False) == 1
    s2 = store.load()
    assert s2["audit_cursor"]["sha"].startswith("base0000")   # cursor 未推进
    assert s2["auth_failure_streak"] == 2                     # 未被空跑清零
    assert created == [] and s2.get("audit_seen") is None     # 零入库

def test_audit_all_malformed_output_does_not_advance_cursor(tmp_path, monkeypatch):
    """检视#7(schema 漂移半边): 输出是 JSON 数组但每条都缺 message——
    _parse_findings 逐条静默跳过后同样不得被当零 findings 成功。"""
    store, created = _audit_env(
        tmp_path, monkeypatch,
        claude_output=json.dumps([{"summary": "x"}, {"note": "y"}]))
    assert tl.run_audit(dry_run=False) == 1
    s2 = store.load()
    assert s2["audit_cursor"]["sha"].startswith("base0000")
    assert s2["auth_failure_streak"] == 2
    assert created == []

def test_audit_genuine_zero_findings_advances_cursor(tmp_path, monkeypatch):
    """检视#7 反向锚: 真·零 findings(claude 输出空 JSON 数组)仍是成功
    审查——cursor 推进、streak 归零不被误伤。"""
    store, created = _audit_env(tmp_path, monkeypatch, claude_output="[]")
    assert tl.run_audit(dry_run=False) == 0
    s2 = store.load()
    assert s2["audit_cursor"]["sha"].startswith("head1111")
    assert s2["auth_failure_streak"] == 0
    assert created == []


# ============ Task 9 fix round 1: Ruling 11 跨锁覆写 ============

def test_audit_does_not_clobber_concurrent_scan_state(tmp_path, monkeypatch):
    """Ruling 11: audit 不得覆写并发 scan 写入的 entries——state 写入只合
    并 audit 自有键(audit_cursor/audit_seen)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    state = {"entries": [], "auth_failure_streak": 0,
             "audit_cursor": {"sha": "base0000000000000000000000000000000000000000",
                              "at": "2026-10-29T00:00:00Z"}}
    store = tl.StateStore(str(tmp_path)); store.save(state)
    # scan 在 audit load 之后写入的推进(派发记账: run 1 → DISPATCHED + 活 PID)
    scan_entry = dict(workflow="Sanitizer", run_id=1, status="DISPATCHED",
                      issue_number=9, fix_branch="fix/ci-1", pr_number=None,
                      attempts=1, rounds=0, agent_pid=123,
                      agent_started_at=None, agent_log=None,
                      last_dispatched_head=None, extra_runs=[],
                      created_at="t", updated_at="t", source="ci",
                      kind="bug", audit_hash=None)
    git_out = {"fetch": "", "rev-parse": "head1111111111111111111111111111111111111111",
               "log": "abc1234 commit one"}
    monkeypatch.setattr(tl, "_git", lambda args, clone_dir=None:
                        git_out.get(args[0], ""))
    monkeypatch.setattr(tl, "_git_diff", lambda base, head: "+diff body")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_add", lambda sha: "/tmp/wt")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_remove", lambda p: None)
    def fake_run_claude(cmd, cwd, prompt):
        # claude 运行(分钟级)期间,并发 scan tick(持 scan.lock)落盘了
        # 派发记账——audit 收尾若整份覆写启动时快照,这条 entries 会丢
        disk = tl.StateStore(str(tmp_path)).load()
        disk["entries"] = [scan_entry]
        tl.StateStore(str(tmp_path)).save(disk)
        return json.dumps([
            {"path": "a.c", "line": 3, "severity": "Important",
             "category": "正确性", "message": "m", "evidence": "e"}])
    monkeypatch.setattr(tl.pr_local_review, "_run_claude", fake_run_claude)
    created = []
    monkeypatch.setattr(tl, "create_task_issue",
                        lambda title, body, labels:
                        created.append(body) or 42)
    monkeypatch.setattr(tl, "_search_audit_issue", lambda h: None)
    rc = tl.run_audit(dry_run=False)
    assert rc == 0 and len(created) == 1
    s2 = store.load()
    # 并发 scan 的派发记账幸存(entries 不被 audit 启动时的空快照回滚)
    assert any(e["run_id"] == 1 and e["status"] == "DISPATCHED"
               and e["agent_pid"] == 123 for e in s2["entries"])
    # audit 自有键照常推进
    assert s2["audit_cursor"]["sha"].startswith("head1111")
    assert len(s2["audit_seen"]) == 1

def test_audit_auth_fail_increments_fresh_disk_streak(tmp_path, monkeypatch):
    """Ruling 11: streak 增量对锁内 fresh 磁盘值做,不吃启动时快照。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    state = {"entries": [], "auth_failure_streak": 0,
             "audit_cursor": {"sha": "base0000000000000000000000000000000000000000",
                              "at": "2026-10-29T00:00:00Z"}}
    store = tl.StateStore(str(tmp_path)); store.save(state)
    git_out = {"fetch": "", "rev-parse": "head1111111111111111111111111111111111111111",
               "log": "abc1234 commit one"}
    monkeypatch.setattr(tl, "_git", lambda args, clone_dir=None:
                        git_out.get(args[0], ""))
    monkeypatch.setattr(tl, "_git_diff", lambda base, head: "+diff body")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_add", lambda sha: "/tmp/wt")
    monkeypatch.setattr(tl.pr_local_review, "_worktree_remove", lambda p: None)
    def fake_run_claude_auth_fail(cmd, cwd, prompt):
        # claude 运行期间并发 scan 把磁盘 streak 推到 2(如另车道 auth_fail
        # 计数)——audit 的 +1 必须对这个 fresh 值做,而非启动时快照的 0
        disk = tl.StateStore(str(tmp_path)).load()
        disk["auth_failure_streak"] = 2
        tl.StateStore(str(tmp_path)).save(disk)
        raise tl.GhError("claude 检视失败(exit 1): API Error 403")
    monkeypatch.setattr(tl.pr_local_review, "_run_claude",
                        fake_run_claude_auth_fail)
    # 子阈值分支: limit 抬到 5 使 3 < 5(实际常量 3 的边界行为见下半段)
    monkeypatch.setattr(tl, "AUTH_FAILURE_STREAK_LIMIT", 5)
    assert tl.run_audit(dry_run=False) == 1
    s2 = store.load()
    assert s2["auth_failure_streak"] == 3      # fresh 2+1,而非快照 0+1
    assert not (tmp_path / "PAUSED").exists()  # 3 < 5: 未达熔断
    # 真实常量边界: limit=3 时同样的 fresh 2+1=3 >= 3 → PAUSED 熔断落地
    monkeypatch.setattr(tl, "AUTH_FAILURE_STREAK_LIMIT", 3)
    assert tl.run_audit(dry_run=False) == 1
    s3 = store.load()
    assert s3["auth_failure_streak"] == 3      # 再次 fresh 2+1
    assert (tmp_path / "PAUSED").exists()


# ============ 终审修复: HUMAN_CLOSED 是终态(同根因收敛不得误判活跃) ============

def test_human_closed_ci_entry_not_active_for_convergence(tmp_path, monkeypatch):
    """终审 Important#1: HUMAN_CLOSED 是终态——同 workflow 新红 run 不得收敛
    进它(不在已关闭 issue 上评论、吞失败),而应建新 issue 开新条目。"""
    state = {"entries": [dict(
        workflow="Sanitizer", run_id=1, status="HUMAN_CLOSED", issue_number=7,
        fix_branch="", pr_number=None, attempts=0, rounds=0, agent_pid=None,
        agent_started_at=None, agent_log=None, last_dispatched_head=None,
        extra_runs=[], created_at="t", updated_at="t", source="ci",
        kind="bug", audit_hash=None)], "auth_failure_streak": 0}
    created, comments = [], []
    monkeypatch.setattr(tl, "create_issue", lambda run: created.append(run) or 9)
    monkeypatch.setattr(tl, "comment_on_issue",
                        lambda n, body: comments.append((n, body)))
    monkeypatch.setattr(tl, "find_issue_by_run", lambda run_id: None)
    monkeypatch.setattr(tl, "find_closed_marker_issue", lambda run_id: None)
    run2 = {"databaseId": 2, "workflowName": "Sanitizer", "event": "schedule",
            "conclusion": "failure", "headBranch": "main", "headSha": "x",
            "createdAt": "2026-10-29T00:00:00Z", "url": "u"}
    tl.triage_new_red_runs(state, [run2], dry_run=False)
    assert len(created) == 1            # 新红 run 开新 issue(未被死条目吞掉)
    assert comments == []               # 零评论——不在已关闭 issue 上碰
    assert state["entries"][0]["extra_runs"] == []   # 死条目未被挂 extra_runs
    assert state["entries"][1]["run_id"] == 2 and state["entries"][1]["status"] == "QUEUED"


def test_paused_env_redirect_ignores_legacy_dir(tmp_path, monkeypatch):
    """PR#317 检视#19(轮2): env 重定向的 run 不检查真实 legacy 目录——与
    _migrate_legacy_state_dir 的 env 守卫同口径,多实例互不误拦。"""
    legacy = tmp_path / "legacy"; legacy.mkdir()
    (legacy / "PAUSED").touch()
    monkeypatch.setattr(tl, "DEFAULT_STATE_DIR", str(tmp_path / "inst-a"))
    monkeypatch.setattr(tl, "LEGACY_STATE_DIR", str(legacy))
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "inst-b"))
    assert tl._paused() is False          # 重定向实例: legacy PAUSED 不拦
    monkeypatch.delenv("TASKLOOP_STATE_DIR", raising=False)
    monkeypatch.delenv("CITRIAGE_STATE_DIR", raising=False)
    assert tl._paused() is True           # 默认目录 run: legacy 仍生效(过渡期)


def test_auth_fail_pattern_no_false_positive_on_bare_403():
    """PR#317 检视#23(轮2): 认证判定收紧——裸 '403'(行号/URL 片段)不再
    误判入熔断 streak;API Error/Not logged in/HTTP 403|401 照常命中。"""
    assert tl.AUTH_FAIL_PATTERN.search("API Error: 403 Not logged in")
    assert tl.AUTH_FAIL_PATTERN.search("HTTP 403 Forbidden")
    assert tl.AUTH_FAIL_PATTERN.search("HTTP 401")
    assert not tl.AUTH_FAIL_PATTERN.search("  at foo.c:403 in bar()")
    assert not tl.AUTH_FAIL_PATTERN.search("see line 403 of the config")


def test_agent_env_path_dead_constant_removed():
    """PR#317 检视#22(轮2): AGENT_ENV_PATH 导入期快照死代码已删——防止
    重新引入第二个可分叉真相源(build_claude_command 用 agent_env_path())。"""
    import pr_local_review as plr
    assert not hasattr(plr, "AGENT_ENV_PATH")


# ===== PR#317 轮 3 检视修复 =====

def test_ensure_agent_labels_uses_json_flag(monkeypatch):
    """检视#25(轮3,Critical): gh label list 必须带 --json——缺省输出
    三列文本表格,gh_json 原样返回 str,逐字符迭代 l["name"] 抛
    TypeError 且编排层不捕,首个非 dry scan tick 必崩(真机实证)。"""
    calls = []
    def fake_gh_json(args, input_text=None):
        calls.append(tuple(args))
        if args[0] == "label" and args[1] == "list":
            return [{"name": n} for n in
                    ("agent-task", "agent-bug", "agent-feature",
                     "agent-approved", "agent-wip", "agent-escalated")]
        return None
    monkeypatch.setattr(tl, "gh_json", fake_gh_json)
    tl.ensure_agent_labels()
    label_list = [c for c in calls if c[:2] == ("label", "list")]
    assert len(label_list) == 1
    assert "--json" in label_list[0] and "name" in label_list[0]
    assert not any(c[:2] == ("label", "create") for c in calls)


def test_auth_fail_below_threshold_removes_wip_label(monkeypatch, tmp_path):
    """检视#26(轮3,Critical): auth_fail 未达熔断阈值的重派等待路径必须
    摘 agent-wip——残留 wip 让下一 tick 的派发闸把 DISPATCHED+无 PID
    条目判'不具资格'永久冻结,一次凭据失效即死锁。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    removed = []
    monkeypatch.setattr(tl, "gh_label_remove",
                        lambda n, lb: removed.append((n, lb)))
    monkeypatch.setattr(tl, "check_agent_outcome", lambda e: "auth_fail")
    monkeypatch.setattr(tl, "_kill_agent_if_alive",
                        lambda e: setattr(e, "agent_pid", None))
    state = {"entries": [dict(
        workflow="Sanitizer", run_id=7, status="DISPATCHED", issue_number=42,
        fix_branch="fix/ci-7", pr_number=None, attempts=1, rounds=0,
        agent_pid=123, agent_started_at=None, agent_log=None,
        last_dispatched_head=None, extra_runs=[], created_at="t",
        updated_at="t", source="ci", kind="bug", audit_hash=None)],
        "auth_failure_streak": 0}
    tl.reap_or_advance(state, dry_run=False)
    assert state["auth_failure_streak"] == 1           # 未达 3,不熔断
    assert removed == [(42, "agent-wip")]              # 关键: wip 已摘
    e = tl.StateStore.entries(state)[0]
    assert e.status == "DISPATCHED" and e.agent_pid is None


def _entry(**kw):
    base = dict(workflow="", run_id=0, status="QUEUED", issue_number=9,
                fix_branch="", pr_number=None, attempts=0, rounds=0,
                agent_pid=None, agent_started_at=None, agent_log=None,
                last_dispatched_head=None, extra_runs=[], created_at="t",
                updated_at="t", source="manual", kind="bug", audit_hash=None)
    base.update(kw)
    return tl.Entry(**base)


def test_dispatch_gate_ci_source_exempt_from_membership_freeze():
    """检视#27(轮3): v1 迁移的 ci 条目 issue 无 agent-task 标签(旧
    create_issue 不带 --label)永不在 open_labels 快照——ci 源豁免成员
    资格冻结(冻结面=关闭 issue/PAUSED);task 源维持冻结语义。"""
    ci = _entry(source="ci", status="DISPATCHED", issue_number=9)
    manual = _entry(source="manual", issue_number=8)
    open_labels = {}                       # 两 issue 均不在快照
    assert tl._dispatch_gate_ok(ci, open_labels) is True
    assert tl._dispatch_gate_ok(manual, open_labels) is False


def test_kill_agent_killpg_for_leader_single_kill_fallback(monkeypatch):
    """检视#28(轮3): 组长形态(setsid 派发)整组击杀 bash→timeout→claude;
    非组长(历史形态/直起进程)回退单杀——防 killpg 误伤调用方自己的组。"""
    killed = []
    monkeypatch.setattr(tl.os, "getpgid", lambda pid: pid)     # 组长
    monkeypatch.setattr(tl.os, "killpg",
                        lambda pgid, sig: killed.append(("pg", pgid)))
    monkeypatch.setattr(tl.os, "kill",
                        lambda pid, sig: killed.append(("pid", pid)))
    e = _entry(status="DISPATCHED", agent_pid=777)
    monkeypatch.setattr(tl, "agent_alive", lambda pid: True)
    tl._kill_agent_if_alive(e)
    assert killed == [("pg", 777)] and e.agent_pid is None
    killed.clear()
    e.agent_pid = 778
    monkeypatch.setattr(tl.os, "getpgid", lambda pid: 4321)    # 非组长
    tl._kill_agent_if_alive(e)
    assert killed == [("pid", 778)] and e.agent_pid is None


def test_dispatch_command_uses_setsid(tmp_path, monkeypatch):
    """检视#28(轮3): 派发命令内层 bash 经 setsid 成为会话/组长——$!
    记录的 PID 即 pgid,killpg 可整树击杀(claude 不再孤儿化)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "clone"))
    monkeypatch.setattr(tl, "gh_label_add", lambda n, lb: None)
    captured = {}
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: captured.update(cmd=cmd) or 99)
    tl.dispatch_agent(_entry(), {"entries": [], "auth_failure_streak": 0})
    assert "setsid" in captured["cmd"] and "claude -p" in captured["cmd"]


def test_audit_dead_base_revision_self_heals(tmp_path, monkeypatch, capsys):
    """检视#29(轮3): cursor 的 base 因 prune/改写不在 origin/main 历史
    → merge-base 失败时回退 24h 窗口重derive,不崩溃循环;git 层其它
    RuntimeError 优雅 return 1 不裸 traceback。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    store = tl.StateStore(str(tmp_path))
    store.save({"entries": [], "auth_failure_streak": 0,
                "audit_cursor": {"sha": "deadbeef" + "0" * 33, "at": "t"}})
    git_map = {"rev-parse": "head1111111111111111111111111111111111111111",
               "rev-list": "base9990000000000000000000000000000000000000",
               "log": "abc1234 x", "diff": "+d"}
    def fake_git(args, clone_dir=None):
        if args[0] == "merge-base":
            raise RuntimeError("git merge-base … 失败(exit 128): fatal")
        return git_map.get(args[0], "")
    monkeypatch.setattr(tl, "_git", fake_git)
    monkeypatch.setattr(tl, "_git_diff", lambda b, h: "+diff")
    assert tl.run_audit(dry_run=True) == 0
    out = capsys.readouterr().out
    assert "回退 24h 窗口" in out and "base999000"[:8] in out
    # 其它 git 失败(fetch 阶段)→ 优雅退出
    def boom(args, clone_dir=None):
        raise RuntimeError("git fetch … 失败(exit 128)")
    monkeypatch.setattr(tl, "_git", boom)
    assert tl.run_audit(dry_run=True) == 1


# ===== PR#317 轮 4 检视修复 =====

def test_timeout_outcome_kills_live_agent_before_requeue(monkeypatch, tmp_path):
    """检视#33(轮4): timeout 判档先于探活,PID 可能仍活——重派路径清引用
    前必须击杀(孤儿化+单飞失明+heal 踩活工作区);dead 结局击杀幂等无害。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    killed = []
    monkeypatch.setattr(tl, "check_agent_outcome", lambda e: "timeout")
    monkeypatch.setattr(tl, "_kill_agent_if_alive",
                        lambda e: (killed.append(e.agent_pid),
                                   setattr(e, "agent_pid", None)))
    state = {"entries": [dict(
        workflow="Sanitizer", run_id=3, status="DISPATCHED", issue_number=11,
        fix_branch="fix/ci-3", pr_number=None, attempts=1, rounds=0,
        agent_pid=555, agent_started_at=None, agent_log=None,
        last_dispatched_head=None, extra_runs=[], created_at="t",
        updated_at="t", source="ci", kind="bug", audit_hash=None)],
        "auth_failure_streak": 0}
    tl.reap_or_advance(state, dry_run=False)
    assert killed == [555]                    # timeout 活 PID 被击杀
    e = tl.StateStore.entries(state)[0]
    assert e.attempts == 2 and e.agent_pid is None   # 走重派记账


def test_audit_search_finding_issue_checks_closed_too(monkeypatch):
    """检视#35(轮4): audit 幂等查重双态搜索——创建轮中途崩溃+人工驳回
    关闭的组合窗口下,不得重建人工已驳回的 issue。"""
    states_queried = []
    def fake_gh(args, input_text=None):
        if tuple(args[:2]) == ("search", "issues"):
            states_queried.append(args[args.index("--state") + 1])
            if args[args.index("--state") + 1] == "closed":
                return [{"number": 77, "body": "x "
                         + tl.AUDIT_MARKER_FMT.format(hash="ab" * 8) + " y"}]
            return []
        return None
    monkeypatch.setattr(tl, "gh_json", fake_gh)
    assert tl._search_audit_issue("ab" * 8) == 77
    assert states_queried == ["open", "closed"]     # 双态都查了


def test_audit_non_dry_ensures_labels(monkeypatch, tmp_path):
    """检视#36(轮4): audit 单独存活的部署(scan 停用)下,建 finding issue
    前须幂等确保 agent-* 标签在位——否则每个 create 都 GhError,cursor
    永不推进,覆盖面静默归零。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path))
    called = []
    monkeypatch.setattr(tl, "ensure_agent_labels",
                        lambda: called.append(1))
    monkeypatch.setattr(tl, "_with_scan_lock", lambda fn: called.append("m"))
    monkeypatch.setattr(tl, "_paused", lambda: True)   # PAUSED 早退前已确保
    assert tl.run_audit(dry_run=False) == 0
    assert called == ["m", 1]                # 迁移后、PAUSED 闸前已确保标签

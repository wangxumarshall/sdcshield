"""task_loop 统一引擎测试(PR-1b 起): schema v2 / 状态归一 / 目录迁移。"""
import sys, os
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

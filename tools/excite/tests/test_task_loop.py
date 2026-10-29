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

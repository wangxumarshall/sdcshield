"""task_loop_pipeline 测试: 槽位常量 / 窗口判定 / 内存闸 / boot_id /
槽位账本(租约/janitor/启动对账)。"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "scripts"))
import task_loop_pipeline as tp
import task_loop as tl
from datetime import datetime


def test_slot_kinds_max():
    assert tp.SLOT_KINDS == {"issue_fix": 2, "pr_review": 2,
                             "pr_repair": 2, "audit": 1}


def test_in_window_cross_midnight():
    assert tp.in_window(datetime(2026, 11, 12, 23, 0))
    assert tp.in_window(datetime(2026, 11, 13, 3, 0))
    assert tp.in_window(datetime(2026, 11, 13, 8, 59))
    assert not tp.in_window(datetime(2026, 11, 13, 9, 0))
    assert not tp.in_window(datetime(2026, 11, 12, 12, 0))


def test_min_mem_gb_env(monkeypatch):
    monkeypatch.setenv("PIPELINE_MIN_MEM_GB", "12")
    assert tp.min_mem_gb() == 12
    monkeypatch.delenv("PIPELINE_MIN_MEM_GB")
    assert tp.min_mem_gb() == 8


def test_mem_available_reads_proc_meminfo(tmp_path, monkeypatch):
    fake = tmp_path / "meminfo"
    fake.write_text("MemTotal:      29000000 kB\nMemAvailable:   9000000 kB\n")
    monkeypatch.setattr(tp, "MEMINFO_PATH", str(fake))
    monkeypatch.setattr(tp, "_cgroup_mem_limit_mb", lambda: None)
    # MemAvailable 按整 MB 取整: 9000000 kB // 1024 = 8789 MB
    assert tp.mem_available_mb() == 8789


def test_mem_gate_ok_threshold(monkeypatch):
    monkeypatch.setattr(tp, "mem_available_mb", lambda: 4096)
    assert tp.mem_gate_ok() is False
    monkeypatch.setattr(tp, "mem_available_mb", lambda: 8192)
    assert tp.mem_gate_ok() is True


def test_boot_id_shape():
    # 真实 /proc 可读；形如 36 字符 hex-uuid
    v = tp.boot_id()
    assert len(v) == 36 and "-" in v


def test_entry_new_fields_roundtrip_and_legacy():
    from task_loop import Entry
    base = dict(workflow="Sanitizer", run_id=1, status="DONE", issue_number=2,
                fix_branch="fix/issue-2", pr_number=3, attempts=1, rounds=0,
                agent_pid=None, agent_started_at=None, agent_log=None,
                last_dispatched_head=None, extra_runs=[], created_at="t",
                updated_at="t", source="audit", kind="bug", audit_hash=None)
    e = Entry(**base, lane="issue_fix", worktree="/w", merged_at="m",
              merged_sha="ab")
    d = e.to_dict()
    assert d["lane"] == "issue_fix" and d["merged_sha"] == "ab"
    # 旧 state(无 4 新键)加载容错: lane="" 其余 None
    legacy = Entry.from_dict(base)
    assert legacy.lane == "" and legacy.worktree is None
    assert legacy.merged_at is None and legacy.merged_sha is None
    # 往返
    assert Entry.from_dict(d).to_dict() == d


# ============ 槽位账本(Task 3) ============


def _mkstate():
    return {"entries": [], "auth_failure_streak": 0}


def test_slot_acquire_respects_kind_max_and_dup_key(monkeypatch):
    # 探活 monkeypatch 钉死: 低号 PID 在真机是 root 内核线程(ksoftirqd/
    # kworker),os.kill → EPERM → agent_alive 判活,宿主相关必假活。
    monkeypatch.setattr(tl, "agent_alive", lambda pid: True)
    st = _mkstate()
    assert tp.slot_acquire(st, "issue_fix", "issue-1", 101, 100, "/l", "/w",
                           "t", "b") is True
    assert tp.slot_acquire(st, "issue_fix", "issue-1", 102, 100, "/l", "/w",
                           "t", "b") is False          # 同 key 互斥
    assert tp.slot_acquire(st, "issue_fix", "issue-2", 103, 100, "/l", "/w",
                           "t", "b") is True
    assert tp.slot_acquire(st, "issue_fix", "issue-3", 104, 100, "/l", "/w",
                           "t", "b") is False          # 超 2 槽


def test_slot_release_and_free_count(monkeypatch):
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    tp.slot_acquire(st, "pr_review", "pr-9", 201, 200, "/l", None, "t", "b")
    assert tp.free_slot_count(st, "pr_review") == 2   # 死 PID → janitor 清 → 空槽
    tp.slot_release(st, "pr_review", "pr-9")
    assert tp.slot_get(st, "pr_review", "pr-9") is None


def test_slot_janitor_marks_environmental(monkeypatch):
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    tp.slot_acquire(st, "issue_fix", "issue-1", 999999, 0, "/l", "/w",
                    "t", "old-boot")
    monkeypatch.setattr(tp, "boot_id", lambda: "new-boot")
    gone = tp.slot_janitor(st, "now")
    assert len(gone) == 1 and gone[0]["environmental"] is True


def test_startup_reconcile_requeues_without_attempt(monkeypatch):
    st = _mkstate()
    st["entries"] = [dict(workflow="w", run_id=1, status="DISPATCHED",
                          issue_number=5, fix_branch="fix/issue-5",
                          pr_number=None, attempts=3, rounds=0, agent_pid=111,
                          agent_started_at="t", agent_log="/l",
                          last_dispatched_head="abc", extra_runs=[],
                          created_at="t", updated_at="t", source="audit",
                          kind="bug", audit_hash=None)]
    tp.slot_acquire(st, "issue_fix", "issue-5", 111, 0, "/l", "/w",
                    "t", "old-boot")
    monkeypatch.setattr(tp, "boot_id", lambda: "new-boot")
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    tp.startup_reconcile(st)
    e = tl.StateStore.entries(st)[0]
    assert e.status == "QUEUED" and e.agent_pid is None
    assert e.attempts == 3          # 未 +1: 环境性死亡豁免
    assert tp.slot_get(st, "issue_fix", "issue-5") is None

"""task_loop_pipeline 骨架测试: 槽位常量 / 窗口判定 / 内存闸 / boot_id。"""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "scripts"))
import task_loop_pipeline as tp
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

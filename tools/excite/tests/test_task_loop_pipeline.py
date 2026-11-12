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

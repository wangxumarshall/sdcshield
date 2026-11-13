# tools/excite/tests/test_task_loop_nodepool.py
import subprocess
import sys, os
import pytest
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "..", "..", "..", "scripts"))
import task_loop_nodepool as np


def test_registry_shape_and_exclusions():
    ids = [n["host_id"] for n in np.BOARD_NODES]
    assert ids == ["0102", "0101", "0201"]          # spec §1 决策 3 顺序
    for n in np.BOARD_NODES:
        assert set(n) == {"host_id", "ssh", "clone_dir", "cores", "slots"}
        assert n["clone_dir"] == "/home/sdc/wangxu/sdcshield-autoci"
        assert n["slots"] == {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}
        assert "@" in n["ssh"] and n["ssh"].endswith(":22")
    assert "0104" not in ids and "0202" not in ids and "0203" not in ids
    assert np.LOCAL_NODE["host_id"] == "local"
    assert np.node_by_host("0102")["cores"] == 192
    assert np.node_by_host("local") is np.LOCAL_NODE


def test_run_ssh_argv_and_success(monkeypatch):
    calls = []
    monkeypatch.setattr(np.subprocess, "run",
                        lambda a, **k: calls.append(a) or type(
                            "R", (), {"returncode": 0, "stdout": b"OK\n"})())
    out = np.run_ssh("0102", "uptime")
    assert out == "OK"
    a = calls[0]
    assert a[0] == "ssh" and a[1] == "-o" and a[2] == "ConnectTimeout=8"
    assert a[3] == "-o" and "StrictHostKeyChecking=no" in a[4]
    assert "sdc@172.168.160.42" in a


def test_run_ssh_failure_raises_nodeerror(monkeypatch):
    monkeypatch.setattr(np.subprocess, "run",
                        lambda a, **k: type("R", (), {
                            "returncode": 255, "stdout": b"",
                            "stderr": b"Connection refused"})())
    try:
        np.run_ssh("0101", "uptime")
        assert False, "应抛 NodeError"
    except np.NodeError as exc:
        assert "0101" in str(exc) and "255" in str(exc)


def test_run_ssh_timeout_becomes_nodeerror(monkeypatch):
    """网络静默黑洞: subprocess 超时必须归一为 NodeError,不得穿透降级契约。"""
    def boom(argv, **kwargs):
        raise subprocess.TimeoutExpired("ssh", 55)
    monkeypatch.setattr(np.subprocess, "run", boom)
    with pytest.raises(np.NodeError) as excinfo:
        np.run_ssh("0201", "sleep 999", timeout=45)
    assert "0201" in str(excinfo.value) and "55" in str(excinfo.value)


# probe_node 的三条测试桩: probe 以 timeout=12 关键字调用 run_ssh(单次往返),
# 桩函数加 **kw 对齐该签名。
def test_probe_remote_parses_combined_output(monkeypatch):
    out = "\n".join([
        "192",                                   # nproc
        "0.92, 0.48, 0.35",                      # /proc/loadavg
        "MemAvailable:  651304200 kB",           # grep MemAvailable
        "2026-01-02 03:04:05",                   # uptime -s
        "inactive",                              # systemctl is-active
    ])
    monkeypatch.setattr(np, "run_ssh", lambda h, cmd, t=45, **kw: out)
    p = np.probe_node("0102")
    # 651304200 kB // 1024 = 636039 MB(计划草稿写 636043 为笔误: 无换算口径
    # 能从 651304200 kB 得出 636043, 该值对应 651308032 kB)。
    assert p == {"reachable": True, "mem_avail_mb": 636039, "load15": 0.35,
                 "cores": 192, "excite_active": False,
                 "boot_time": "2026-01-02 03:04:05"}


def test_probe_unreachable(monkeypatch):
    def boom(h, cmd, t=45, **kw):
        raise np.NodeError("节点 0101 命令失败(rc=255)")
    monkeypatch.setattr(np, "run_ssh", boom)
    p = np.probe_node("0101")
    assert p["reachable"] is False and p["mem_avail_mb"] == 0


def test_probe_local_reads_proc(monkeypatch, tmp_path):
    fake = tmp_path / "meminfo"
    fake.write_text("MemAvailable:  1048576 kB\n")
    monkeypatch.setattr(np, "MEMINFO_PATH", str(fake))
    p = np.probe_node("local")
    assert p["reachable"] is True and p["mem_avail_mb"] == 1024
    assert p["excite_active"] is False and p["boot_time"] is None

# tools/excite/tests/test_task_loop_nodepool.py
import sys, os
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

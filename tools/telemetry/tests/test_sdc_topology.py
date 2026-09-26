import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_topology

def test_snapshot_invariants():
    t = sdc_topology.snapshot()
    assert set(t["online"]) <= set(t["possible"])
    assert set(t["possible"]) == set(t["online"]) | set(t["offline"])
    for _, info in t["cpus"].items():
        assert isinstance(info["online"], bool)
        assert isinstance(info["cluster_id"], int)   # PPTT 伪影板：值可能很大，只断言类型
    assert len(t["nodes"]) >= 1

def test_run_snapshot_cli():
    import json, subprocess
    out = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "..",
        "sdc_topology.py")], capture_output=True, text=True, check=True).stdout
    t = json.loads(out)
    assert t["possible"] == sorted(t["possible"])

def test_has_memory_matches_memtotal():
    import glob, os
    t = sdc_topology.snapshot()
    for n in glob.glob("/sys/devices/system/node/node*"):
        total = sdc_topology._memtotal_kb(n)
        nid = os.path.basename(n)[4:]
        assert t["nodes"][nid]["has_memory"] == (total > 0), nid

def test_has_memory_uses_regex_independent_of_impl(tmp_path):
    # 前缀变体免疫：写真实 meminfo 文件直接验证正则口径（_memtotal_kb 契约是目录路径）
    import sdc_topology as st
    (tmp_path / "meminfo").write_text("Node 0 MemTotal:       264123904 kB\n")
    assert st._memtotal_kb(str(tmp_path)) == 264123904
    (tmp_path / "meminfo").write_text("MemTotal:       0 kB\n")
    assert st._memtotal_kb(str(tmp_path)) == 0
    (tmp_path / "meminfo").write_text("Node 3 MemTotal:  1024 kB\n")
    assert st._memtotal_kb(str(tmp_path)) == 1024

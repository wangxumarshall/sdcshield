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
        total = 0
        try:
            # 行格式: "Node <id> MemTotal:  <kB> kB" → 值在 split()[3]
            for line in open(f"{n}/meminfo"):
                if "MemTotal:" in line:
                    total = int(line.split()[3]); break
        except OSError:
            pass
        nid = os.path.basename(n)[4:]
        assert t["nodes"][nid]["has_memory"] == (total > 0), nid

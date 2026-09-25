#!/usr/bin/env python3
"""sdc_topology.py — 拓扑快照（v5 §4.3）。不假设 package/cluster 连续或从 0 起（PPTT 伪影）。"""
import glob, json, os

_SYS_CPU = "/sys/devices/system/cpu"

def _read(path, default=None):
    try:
        return open(path).read().strip()
    except OSError:
        return default

def _expand(cpulist):
    out = []
    for part in (cpulist or "").split(","):
        if not part: continue
        if "-" in part:
            lo, hi = part.split("-"); out.extend(range(int(lo), int(hi) + 1))
        else:
            out.append(int(part))
    return out

def _node_has_memory(node_dir):
    # 行格式（内核 ABI，drivers/base/node.c）: "Node <id> MemTotal:  <kB> kB"
    # 故用 "MemTotal:" in line 而非 startswith；值在 split()[3]（[1] 是节点号）。
    try:
        for line in open(f"{node_dir}/meminfo"):
            if "MemTotal:" in line:
                return int(line.split()[3]) > 0
    except OSError:
        pass
    return False

def snapshot():
    possible = _expand(_read(f"{_SYS_CPU}/possible"))
    online = _expand(_read(f"{_SYS_CPU}/online"))
    cpus = {}
    for c in possible:
        base = f"{_SYS_CPU}/cpu{c}"
        cpus[str(c)] = {
            "online": c in online,
            "package": int(_read(f"{base}/topology/physical_package_id", -1)),
            "core_id": int(_read(f"{base}/topology/core_id", -1)),
            "cluster_id": int(_read(f"{base}/topology/cluster_id", -1)),
            "die_id": int(_read(f"{base}/topology/die_id", -1)),
        }
    nodes = {}
    for n in sorted(glob.glob("/sys/devices/system/node/node*"),
                    key=lambda p: int(os.path.basename(p)[4:])):
        nid = os.path.basename(n)[4:]
        nodes[nid] = {"cpulist": _read(f"{n}/cpulist", ""),
                      "has_memory": _node_has_memory(n)}
    return {"possible": possible,
            "online": online,
            "offline": sorted(set(possible) - set(online)),
            "isolated": _expand(_read(f"{_SYS_CPU}/isolated")),
            "cpus": cpus, "nodes": nodes}

if __name__ == "__main__":
    print(json.dumps(snapshot(), indent=1))

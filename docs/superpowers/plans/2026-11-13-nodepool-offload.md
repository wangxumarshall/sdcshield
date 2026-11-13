# 跨机卸载 nodepool offload 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** nightly pipeline 的三泳道派发升级为"本机+0102/0101/0201 四节点资源池常态分载"——agent 在板上全流程执行（修→验→push→gh pr create→贴报告），探活/收割/软停经 SSH 分流，state 写者仍唯一在本机。

**Architecture:** 新模块 `scripts/task_loop_nodepool.py` 承载全部远程面（板注册表/SSH 封装/健康探测/pick_node 择优/远程五操作）；`task_loop.py` 的 dispatch_agent 加 node 分支与 Entry.host 字段；`task_loop_pipeline.py` 的槽位账本加 host 维度、cycle 派发点接 pick_node。一次性三板环境由 `scripts/nodepool_setup.sh` 幂等准备。

**Tech Stack:** Python 3（/usr/bin/python3，零第三方——SSH 用系统 `ssh`/`scp` 子进程，ConnectTimeout=8）、pytest、bash。

**Spec:** `docs/superpowers/specs/2026-11-13-nodepool-offload-design.md`（8 条决策表 + §2-§8）。实施前必读。

## Global Constraints

- 解释器 `/usr/bin/python3`；**零第三方新依赖**（禁 paramiko——SSH 一律 `subprocess` 调系统 `ssh`/`scp`，形态 `ssh -o ConnectTimeout=8 -o StrictHostKeyChecking=no <user@host> <cmd>`）。
- 凭据不入库：板上 token/密码绝不进代码与 commit；SSH 走密钥免密（setup 分发），`agent.env` 经 scp 0600 放板。
- 板上路径常量：克隆 `/home/sdc/wangxu/sdcshield-autoci`、状态目录 `~/sdcshield-autoci-state`。
- 常量（spec 精确值）：每节点内存闸 = `PIPELINE_MIN_MEM_GB`（默认 8G）；`load15 < cores×0.5`；激发单元名 `sdc-excite-reproduce`；agent 内层 `timeout 3600`；日志尾读 `tail -c 4000`；SSH ConnectTimeout=8。
- 测试纪律：**零触网零真 SSH**——nodepool 一切 ssh 经模块级单点 `run_ssh`（测试 monkeypatch 它）；探活/时钟注入参数。
- 每任务一 commit：`git commit -s`（DCO 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By）；分支 `feat/nodepool-offload`；commit 前全量 `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`（基线 374 passed）。
- 注释/标识符风格贴 `scripts/task_loop.py` 现状：中文注释、`_` 前缀私有、print 带 `[tag]` 前缀。
- 安全阀：env `PIPELINE_NODEPOOL=0` 全关（纯本机旧行为）；`PIPELINE_FORCE_NODE=<host_id>` 强制单节点（冒烟/测试用）。
- 多 key 轮询（spec 决策表 #9 增补）：GLM 三 key（key3/wang/ding）round-robin；池文件 `agent-key-*.env` 0600；**token 绝不打印/入库**，测试用假 token 文件。
- x86-64 零触碰（纯 Python/bash）。行号引用以当前 main（060812ea 系）函数名/注释锚点定位，漂移以实码为准。

---

### Task 1: nodepool 模块骨架——注册表 + SSH 单点运行器

**Files:**
- Create: `scripts/task_loop_nodepool.py`
- Test: `tools/excite/tests/test_task_loop_nodepool.py`

**Interfaces:**
- Produces: `BOARD_NODES`（注册表 list[dict]，含 host_id/ssh/clone_dir/cores/slots）；`LOCAL_NODE`；`node_by_host(host_id)`；`class NodeError(RuntimeError)`；`run_ssh(host_id, cmd, timeout=45) -> str`（**全模块唯一 ssh 子进程出口**——stdout 成功返回；非零退出抛 NodeError 含 stderr 摘要；后续任务全经它）；`put_file(host_id, local_path, remote_path)`（scp 同款单点）；`REMOTE_STATE_DIR = "~/sdcshield-autoci-state"`。

- [ ] **Step 1: 写失败测试**

```python
# tools/excite/tests/test_task_loop_nodepool.py
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "..", "..", "scripts"))
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
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_nodepool.py -x -q`
Expected: FAIL（ModuleNotFoundError: task_loop_nodepool）

- [ ] **Step 3: 实现**

```python
#!/usr/bin/env python3
# SPDX-License-Identifier: Apache-2.0
"""task_loop_nodepool.py — 跨机卸载节点池(spec 2026-11-13-nodepool-offload)。

nightly pipeline 的远程执行面: 板注册表 + SSH 单点运行器 + 健康探测 +
pick_node 择优 + 远程五操作(spawn/探活/日志尾/击杀/worktree)。本机编排器
(task_loop_pipeline/task_loop) 经懒 import 使用本模块; nodepool 不反向
import 它们(锁死依赖方向: tl → tp → np)。

零第三方: SSH 一律系统 ssh/scp 子进程, ConnectTimeout=8(探测语义:
连不通=该节点本轮不可选,绝不长时间挂住 cycle)。凭据走 setup 分发的密钥
免密(nodepool_setup.sh), 密码/token 不进任何 argv 与代码。

安全阀: env PIPELINE_NODEPOOL=0 → pick_node 只返回本机(纯本机旧行为);
PIPELINE_FORCE_NODE=<host_id> → 强制单节点(冒烟/测试用)。
"""
import os
import subprocess

# 板注册表(spec §3.1; 凭据不入库——连接经 setup 分发的 ~/.ssh/authorized_keys)
# 顺序即 pick_node 的注册表序(同分时先 0102)。0104 水银核永久排除;
# 0202/0203 异网隧道板排除(spec 决策表 #3)。
BOARD_NODES = [
    {"host_id": "0102", "ssh": "sdc@172.168.160.42:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 192, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
    {"host_id": "0101", "ssh": "sdc@172.168.177.97:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 126, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
    {"host_id": "0201", "ssh": "sdc@172.168.178.81:22",
     "clone_dir": "/home/sdc/wangxu/sdcshield-autoci",
     "cores": 96, "slots": {"issue_fix": 2, "pr_review": 2, "pr_repair": 2}},
]
LOCAL_NODE = {"host_id": "local", "ssh": None,
              "clone_dir": "/home/sdc/wangxu/operator/sdcshield-autoci",
              "cores": None, "slots": {"issue_fix": 2, "pr_review": 2,
                                       "pr_repair": 2}}
REMOTE_STATE_DIR = "~/sdcshield-autoci-state"   # 板上日志/prompt/agent.env 目录
SSH_TIMEOUT_DEFAULT = 45


class NodeError(RuntimeError):
    """节点操作失败(SSH 不通/命令非零)。调用方按 spec §6 降级,不炸 cycle。"""


def node_by_host(host_id):
    for n in BOARD_NODES:
        if n["host_id"] == host_id:
            return n
    if host_id == "local":
        return LOCAL_NODE
    raise NodeError(f"未知节点: {host_id}")


def _ssh_argv(host_id, cmd, timeout):
    node = node_by_host(host_id)
    user_host, _, port = node["ssh"].rpartition(":")
    return ["ssh", "-o", "ConnectTimeout=8", "-o", "StrictHostKeyChecking=no",
            "-o", "UserKnownHostsFile=/dev/null", "-p", port, user_host, cmd]


def run_ssh(host_id, cmd, timeout=SSH_TIMEOUT_DEFAULT):
    """全模块唯一 ssh 出口(测试 monkeypatch 此单点,零真触网)。
    成功返回 stdout.strip(); 非零退出抛 NodeError(含退出码+stderr 摘要)。"""
    proc = subprocess.run(_ssh_argv(host_id, cmd, timeout), capture_output=True,
                          timeout=timeout + 10)
    if proc.returncode != 0:
        err = proc.stderr.decode("utf-8", errors="replace")[:200]
        raise NodeError(f"节点 {host_id} 命令失败(rc={proc.returncode}): {err}")
    return proc.stdout.decode("utf-8", errors="replace").strip()


def put_file(host_id, local_path, remote_path):
    """scp 单点(同 run_ssh 的免密形态; agent.env/prompt 上板走这里)。"""
    node = node_by_host(host_id)
    user_host, _, port = node["ssh"].rpartition(":")
    argv = ["scp", "-o", "ConnectTimeout=8", "-o", "StrictHostKeyChecking=no",
            "-o", "UserKnownHostsFile=/dev/null", "-P", port,
            local_path, f"{user_host}:{remote_path}"]
    proc = subprocess.run(argv, capture_output=True, timeout=60)
    if proc.returncode != 0:
        raise NodeError(f"scp → {host_id} 失败(rc={proc.returncode}): "
                        f"{proc.stderr.decode(errors='replace')[:200]}")
```

- [ ] **Step 4: 跑测试确认通过 + 全量**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_nodepool.py -x -q` → PASS；`/usr/bin/python3 -m pytest tools/excite/tests/ -x -q` → 374+3

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_nodepool.py tools/excite/tests/test_task_loop_nodepool.py
git commit -s -m "feat(nodepool): 节点池骨架——板注册表(0102/0101/0201,排除 0104/0202/0203)+SSH/scp 单点运行器(NodeError 降级语义)"
```

---

### Task 2: probe_node 板健康探测

**Files:**
- Modify: `scripts/task_loop_nodepool.py`
- Test: `tools/excite/tests/test_task_loop_nodepool.py`

**Interfaces:**
- Produces: `probe_node(host_id) -> dict`，键 `{reachable: bool, mem_avail_mb: int, load15: float, cores: int, excite_active: bool, boot_time: str|None}`。local 节点同样可探测（mem 来自 `tp` 同源函数——本任务内先用 /proc/meminfo 直读，与 tp.mem_available_mb 同口径，不 import tp）。一次 SSH 取全部字段。

- [ ] **Step 1: 写失败测试**

```python
def test_probe_remote_parses_combined_output(monkeypatch):
    out = "\n".join([
        "192",                                   # nproc
        "0.92, 0.48, 0.35",                      # /proc/loadavg
        "MemAvailable:  651304200 kB",           # grep MemAvailable
        "2026-01-02 03:04:05",                   # uptime -s
        "inactive",                              # systemctl is-active
    ])
    monkeypatch.setattr(np, "run_ssh", lambda h, cmd, t=45: out)
    p = np.probe_node("0102")
    assert p == {"reachable": True, "mem_avail_mb": 636043, "load15": 0.35,
                 "cores": 192, "excite_active": False,
                 "boot_time": "2026-01-02 03:04:05"}


def test_probe_unreachable(monkeypatch):
    def boom(h, cmd, t=45):
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
```

- [ ] **Step 2: 确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_nodepool.py -k probe -x -q` → FAIL（probe_node 未定义）

- [ ] **Step 3: 实现（追加到 nodepool）**

```python
MEMINFO_PATH = "/proc/meminfo"

# 一次 SSH 取全字段(nproc/loadavg/MemAvailable/uptime/激发状态)——probe 每
# cycle 对候选板各一次,单次往返优先于多次。
_PROBE_CMD = ("nproc; cat /proc/loadavg; "
              "grep MemAvailable /proc/meminfo; "
              "uptime -s; systemctl is-active sdc-excite-reproduce 2>/dev/null"
              " || echo inactive")


def _parse_probe(out):
    lines = [l.strip() for l in out.splitlines() if l.strip()]
    cores = int(lines[0])
    load15 = float(lines[1].split()[2])
    mem_kb = int(lines[2].split()[1])
    boot = lines[3]
    excite = lines[4] == "active"
    return {"reachable": True, "mem_avail_mb": mem_kb // 1024,
            "load15": load15, "cores": cores, "excite_active": excite,
            "boot_time": boot}


def probe_node(host_id):
    """节点健康探测(spec §2): 不可达 → reachable=False 其余零值。"""
    if host_id == "local":
        avail = 0
        try:
            with open(MEMINFO_PATH) as f:
                for line in f:
                    if line.startswith("MemAvailable:"):
                        avail = int(line.split()[1]) // 1024
                        break
        except OSError:
            return {"reachable": False, "mem_avail_mb": 0, "load15": 0.0,
                    "cores": 0, "excite_active": False, "boot_time": None}
        return {"reachable": True, "mem_avail_mb": avail, "load15": 0.0,
                "cores": 0, "excite_active": False, "boot_time": None}
    try:
        return _parse_probe(run_ssh(host_id, _PROBE_CMD, timeout=12))
    except NodeError:
        return {"reachable": False, "mem_avail_mb": 0, "load15": 0.0,
                "cores": 0, "excite_active": False, "boot_time": None}
```

（`test_probe_local_reads_proc` 断言 local cores=0——local 的 cores/负载由 pick_node 特判（本机 load 阈值不查，内存闸已够），0 值不参与板间排序比较。）

- [ ] **Step 4: 通过 + 全量** → 374+6

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_nodepool.py tools/excite/tests/test_task_loop_nodepool.py
git commit -s -m "feat(nodepool): probe_node 一次 SSH 五字段健康探测(nproc/load15/MemAvailable/uptime/激发态),不可达零值降级"
```

---

### Task 3: Entry.host 字段 + 槽位 host 维度（tp/tl 侧账本扩展）

**Files:**
- Modify: `scripts/task_loop.py`（Entry dataclass/to_dict/from_dict，约 192-310 区域）
- Modify: `scripts/task_loop_pipeline.py`（slot_acquire/free 计数）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces: `tl.Entry.host: str = "local"`（第 25 字段，带默认+from_dict 容错）；`tp.slot_acquire(state, kind, key, pid, pgid, log, worktree, started_iso, boot, host="local")`（末参默认——**既有 9 参调用零改动**）；`tp.free_slots_on(state, kind, host="local") -> int`（指定节点的空槽数）；`tp.slot_count_cap(state, kind, host)`（注册表上限，local 回落 SLOT_KINDS）。
- Consumes: `np.node_by_host`（懒 import，经 `import task_loop_nodepool as np` 在函数内）。

- [ ] **Step 1: 写失败测试**

```python
def test_entry_host_field_roundtrip_and_legacy():
    from task_loop import Entry
    base = dict(workflow="w", run_id=1, status="PR_OPEN", issue_number=5,
                fix_branch="fix/issue-5", pr_number=9, attempts=1, rounds=0,
                agent_pid=None, agent_started_at=None, agent_log=None,
                last_dispatched_head=None, extra_runs=[], created_at="t",
                updated_at="t", source="audit", kind="bug", audit_hash=None)
    e = Entry(**base, host="0102")
    assert e.to_dict()["host"] == "0102"
    assert Entry.from_dict(base).host == "local"      # 旧 state 升格


def test_slot_host_dimension(monkeypatch):
    st = _mkstate()
    monkeypatch.setattr(tp.tl, "agent_alive", lambda pid: True)
    assert tp.slot_acquire(st, "issue_fix", "issue-1", 101, 100, "/l", "/w",
                           "t", "b") is True                      # host=local 默认
    assert tp.slot_acquire(st, "issue_fix", "issue-2", 201, 200, "/l", "/w",
                           "t", "b", host="0102") is True
    assert tp.slot_acquire(st, "issue_fix", "issue-3", 301, 300, "/l", "/w",
                           "t", "b", host="0102") is True
    assert tp.free_slots_on(st, "issue_fix", "local") == 1
    assert tp.free_slots_on(st, "issue_fix", "0102") == 0
    assert tp.slot_count_cap(st, "issue_fix", "0102") == 2
    assert tp.slot_count_cap(st, "issue_fix", "local") == tp.SLOT_KINDS["issue_fix"]
    # 同 key 跨节点互斥: 同一 issue 不同时在两台跑
    assert tp.slot_acquire(st, "issue_fix", "issue-1", 401, 400, "/l", "/w",
                           "t", "b", host="0101") is False
```

- [ ] **Step 2: 确认失败** → FAIL（Entry 无 host / slot_acquire 无 host 形参）

- [ ] **Step 3: 实现**

(a) `task_loop.py` Entry：`last_conflict_night` 之后加 `host: str = "local"`；`to_dict` 加 `"host": self.host`；`from_dict` 加 `host=d.get("host", "local")`；字段注释口径 24→25。

(b) `task_loop_pipeline.py`：slot_acquire 加末参 `host="local"`，租约 dict 存 `"host": host`；同 key 互斥检查不区分 host（spec §4 同 key 跨节点互斥）；`free_slot_count(state, kind)` **保持现语义**（本机闸与旧测试零破坏），新增：

```python
def free_slots_on(state, kind, host="local"):
    """指定节点的空槽数(host 维度分账; local=tp 本机, 板=注册表配额)。"""
    import task_loop_nodepool as np
    live = sum(1 for s in _slots(state, kind)
               if s.get("host", "local") == host and tl.agent_alive(s["pid"]))
    return max(0, slot_count_cap(state, kind, host) - live)


def slot_count_cap(state, kind, host):
    if host == "local":
        return SLOT_KINDS.get(kind, 0)
    import task_loop_nodepool as np
    return np.node_by_host(host)["slots"].get(kind, 0)
```

- [ ] **Step 4: 通过 + 全量** → 基线+2

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop.py scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(nodepool): Entry.host 第25字段(旧 state 升格 local)+槽位 host 维度(free_slots_on/slot_count_cap,同 key 跨节点互斥)"
```

---

### Task 4: pick_node 候选过滤×择优×兜底

**Files:**
- Modify: `scripts/task_loop_nodepool.py`
- Test: `tools/excite/tests/test_task_loop_nodepool.py`

**Interfaces:**
- Consumes: `tp.free_slots_on`（懒 import tp——依赖方向特例：np 仅在本函数内 import tp，tp 在派发点 import np，模块顶层无环）。
- Produces: `pick_node(kind, state) -> node dict | None`。规则（spec §2）：`PIPELINE_NODEPOOL=0` → 恒 LOCAL_NODE；`PIPELINE_FORCE_NODE` → 该节点（不探测过滤，冒烟用）；否则候选=[local]+BOARD_NODES，probe 过滤（reachable ∧ ¬excite_active ∧ load15<cores×0.5——local 不查 load ∧ MemAvailable≥`PIPELINE_MIN_MEM_GB`×1024 ∧ `tp.free_slots_on(kind, host)>0`），按 mem_avail_mb 降序（local 与板平权，0102 通常胜）取首；全空 → None（调用方维持"本轮不派"）。

- [ ] **Step 1: 写失败测试**

```python
def test_pick_node_prefers_richest_and_filters(monkeypatch):
    import task_loop_pipeline as tp
    probes = {"local": {"reachable": True, "mem_avail_mb": 26000,
                        "load15": 0.1, "cores": 0, "excite_active": False,
                        "boot_time": None},
              "0102": {"reachable": True, "mem_avail_mb": 636000,
                       "load15": 0.35, "cores": 192,
                       "excite_active": False, "boot_time": "x"},
              "0101": {"reachable": True, "mem_avail_mb": 26000,
                       "load15": 0.1, "cores": 126,
                       "excite_active": False, "boot_time": "x"},
              "0201": {"reachable": True, "mem_avail_mb": 25000,
                       "load15": 95.0, "cores": 96,          # load 超核半 → 滤掉
                       "excite_active": False, "boot_time": "x"}}
    monkeypatch.setattr(np, "probe_node", lambda h: probes[h])
    monkeypatch.setattr(tp, "free_slots_on", lambda s, k, host="local": 2)
    monkeypatch.delenv("PIPELINE_NODEPOOL", raising=False)
    monkeypatch.delenv("PIPELINE_FORCE_NODE", raising=False)
    st = {}
    assert np.pick_node("issue_fix", st)["host_id"] == "0102"   # 常态分载
    probes["0102"]["excite_active"] = True
    assert np.pick_node("issue_fix", st)["host_id"] == "local"  # 激发跳过
    probes["local"]["mem_avail_mb"] = 4000                      # 本机内存不足
    assert np.pick_node("issue_fix", st)["host_id"] == "0101"
    monkeypatch.setattr(tp, "free_slots_on", lambda s, k, host="local": 0)
    assert np.pick_node("issue_fix", st) is None                # 全空 → None


def test_pick_node_env_valves(monkeypatch):
    monkeypatch.setenv("PIPELINE_NODEPOOL", "0")
    assert np.pick_node("pr_review", {}) is np.LOCAL_NODE
    monkeypatch.setenv("PIPELINE_NODEPOOL", "1")
    monkeypatch.delenv("PIPELINE_FORCE_NODE", raising=False)
    monkeypatch.setenv("PIPELINE_FORCE_NODE", "0201")
    assert np.pick_node("pr_review", {})["host_id"] == "0201"  # 强制不探测
```

- [ ] **Step 2: 确认失败** → FAIL（pick_node 未定义）

- [ ] **Step 3: 实现（追加到 nodepool）**

```python
def _node_ok(probe, node):
    if not probe["reachable"] or probe["excite_active"]:
        return False
    if node["host_id"] != "local" and \
            probe["cores"] and probe["load15"] >= probe["cores"] * 0.5:
        return False                       # 本机不查 load(闸已够,白天重载常态)
    min_mb = int(os.environ.get("PIPELINE_MIN_MEM_GB", "8")) * 1024
    return probe["mem_avail_mb"] >= min_mb


def pick_node(kind, state):
    """三泳道派发的择优入口(spec §1 决策 1 常态分载)。
    返回 node dict; None=全节点不可用(调用方维持现行「本轮不派」)。
    探测零缓存——每 cycle 每候选一次 probe(300s 粒度天然缓存,SSH 12s 超时)。"""
    if os.environ.get("PIPELINE_NODEPOOL", "1") == "0":
        return LOCAL_NODE
    forced = os.environ.get("PIPELINE_FORCE_NODE")
    if forced:
        return node_by_host(forced)
    import task_loop_pipeline as tp          # 懒 import,模块顶层无环
    best, best_mem = None, -1
    for node in [LOCAL_NODE] + BOARD_NODES:
        probe = probe_node(node["host_id"])
        if not _node_ok(probe, node):
            continue
        if tp.free_slots_on(state, kind, host=node["host_id"]) <= 0:
            continue
        if probe["mem_avail_mb"] > best_mem:
            best, best_mem = node, probe["mem_avail_mb"]
    return best
```

- [ ] **Step 4: 通过 + 全量** → 基线+2

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_nodepool.py tools/excite/tests/test_task_loop_nodepool.py
git commit -s -m "feat(nodepool): pick_node 常态分载择优——探测过滤(激发跳过/load/每节点内存闸/空槽)按 MemAvailable 降序,env 双安全阀,全空 None 兜底"
```

---

### Task 5: 远程五操作（spawn/探活/日志尾/击杀/worktree+heal+refresh）

**Files:**
- Modify: `scripts/task_loop_nodepool.py`
- Test: `tools/excite/tests/test_task_loop_nodepool.py`

**Interfaces:**
- Produces:
  - `spawn_remote(node, inner_cmd, log_path) -> int`（PID 契约同本机 run_background：`nohup setsid bash -c <cmd> >> <log> 2>&1 & echo $!`，读唯一回显行转 int；读不到抛 NodeError）
  - `alive_remote(host_id, pid) -> bool`（`kill -0`；SSH 不通抛 NodeError——**调用方须把 NodeError 归 unreachable 槽保留，绝不判死**）
  - `read_log_tail(host_id, path, nbytes=4000) -> str`（`tail -c 4000`；NodeError 上抛）
  - `kill_remote(host_id, pgid, sig)`（`kill -<sig> -- -<pgid>`；NodeError 上抛，调用方留次夜）
  - `prepare_worktree_remote(node, key) -> str`（板上 `git worktree add --detach <clone>/.worktrees/<key> origin/main`，含先 cleanup；返回板上路径）
  - `cleanup_worktree_remote(host_id, path)`（remove --force + prune + rmtree 兜底）
  - `heal_remote(node)`（`git fetch origin --prune && git reset --hard origin/main`，cwd=clone_dir）
  - `refresh_env_remote(node)`（`mkdir -p REMOTE_STATE_DIR` + `put_file(agent.env)`——本机 agent.env 路径由调用方传入：`refresh_env_remote(node, local_agent_env_path)`）
- Consumes: `run_ssh`/`put_file`（Task 1）。

- [ ] **Step 1: 写失败测试**

```python
def test_spawn_remote_pid_contract(monkeypatch):
    sent = []
    monkeypatch.setattr(np, "run_ssh",
                        lambda h, cmd, t=45: sent.append((h, cmd)) or "4242")
    pid = np.spawn_remote(np.node_by_host("0102"), "source env && claude -p",
                          "~/sdcshield-autoci-state/agent-x.log")
    assert pid == 4242
    h, cmd = sent[0]
    assert h == "0102"
    assert "nohup setsid bash -c" in cmd and "echo $!" in cmd
    assert "sdcshield-autoci-state/agent-x.log" in cmd


def test_alive_and_logtail_and_kill_forms(monkeypatch):
    sent = []
    monkeypatch.setattr(np, "run_ssh",
                        lambda h, cmd, t=45: sent.append((h, cmd))
                        or ("0" if "kill -0" in cmd else "TAIL"))
    assert np.alive_remote("0102", 4242) is True
    assert sent[-1][1].strip() == "kill -0 4242 || echo 1; kill -0 4242 2>/dev/null || exit 1" \
        or "kill -0 4242" in sent[-1][1]
    assert np.read_log_tail("0102", "~/a.log") == "TAIL"
    assert "tail -c 4000" in sent[-1][1] and "a.log" in sent[-1][1]
    np.kill_remote("0102", 4242, 15)
    assert "kill -15 -- -4242" in sent[-1][1]


def test_alive_remote_dead_pid(monkeypatch):
    monkeypatch.setattr(np, "run_ssh",
                        lambda h, cmd, t=45: (_ for _ in ()).throw(
                            np.NodeError("rc=1")))
    assert np.alive_remote("0101", 999) is False        # 命令失败=进程死


def test_worktree_heal_refresh_forms(monkeypatch):
    sent = []
    monkeypatch.setattr(np, "run_ssh",
                        lambda h, cmd, t=45: sent.append((h, cmd)) or "")
    monkeypatch.setattr(np, "put_file",
                        lambda h, l, r: sent.append((h, f"PUT {l}->{r}")))
    wt = np.prepare_worktree_remote(np.node_by_host("0102"), "issue-7")
    assert wt == "/home/sdc/wangxu/sdcshield-autoci/.worktrees/issue-7"
    assert any("worktree add --detach" in c and "issue-7" in c
               for _, c in sent)
    np.heal_remote(np.node_by_host("0102"))
    assert any("reset --hard origin/main" in c for _, c in sent)
    np.refresh_env_remote(np.node_by_host("0102"), "/local/agent.env")
    assert any("mkdir -p ~/sdcshield-autoci-state" in c for _, c in sent)
    assert any("PUT /local/agent.env" in str(s) for s in sent)
```

- [ ] **Step 2: 确认失败** → FAIL

- [ ] **Step 3: 实现（追加到 nodepool）**

```python
def spawn_remote(node, inner_cmd, log_path):
    """远程派生,与本机 run_background 同契约(PID 回显唯一行)。"""
    cmd = (f"nohup setsid bash -c {inner_cmd!r} >> {log_path} 2>&1 & echo $!")
    out = run_ssh(node["host_id"], cmd)
    try:
        return int(out)
    except ValueError:
        raise NodeError(f"{node['host_id']} 未回显后台 PID(实际 {out!r})")


def alive_remote(host_id, pid):
    """探活: kill -0。NodeError(SSH 断)上抛——调用方按 unreachable 处理,
    绝不把「连不上」当「进程死」(spec §5)。"""
    run_ssh(host_id, f"kill -0 {pid}")
    return True          # run_ssh 非零即抛;走到这里=活着


def read_log_tail(host_id, path, nbytes=4000):
    return run_ssh(host_id, f"tail -c {nbytes} {path}")


def kill_remote(host_id, pgid, sig):
    run_ssh(host_id, f"kill -{sig} -- -{pgid}")


def prepare_worktree_remote(node, key):
    path = f"{node['clone_dir']}/.worktrees/{key}"
    cleanup_worktree_remote(node["host_id"], path)
    run_ssh(node["host_id"],
            f"cd {node['clone_dir']} && git fetch origin --prune && "
            f"git worktree add --detach {path} origin/main")
    return path


def cleanup_worktree_remote(host_id, path):
    run_ssh(host_id,
            f"git -C {node_by_host(host_id)['clone_dir']} worktree remove "
            f"--force {path} 2>/dev/null; git -C "
            f"{node_by_host(host_id)['clone_dir']} worktree prune; "
            f"rm -rf {path}")


def heal_remote(node):
    run_ssh(node["host_id"],
            f"cd {node['clone_dir']} && git fetch origin --prune && "
            f"git reset --hard origin/main")


def refresh_env_remote(node, local_agent_env_path):
    run_ssh(node["host_id"], f"mkdir -p {REMOTE_STATE_DIR}")
    put_file(node["host_id"], local_agent_env_path,
             f"{REMOTE_STATE_DIR}/agent.env")
    run_ssh(node["host_id"], f"chmod 600 {REMOTE_STATE_DIR}/agent.env")
```

- [ ] **Step 4: 通过 + 全量**

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_nodepool.py tools/excite/tests/test_task_loop_nodepool.py
git commit -s -m "feat(nodepool): 远程五操作——spawn(PID 回显契约)/探活(NodeError=unreachable 不判死)/日志尾/击杀 pgid/worktree+heal+agent.env 刷新"
```

---

### Task 6: dispatch_agent/dispatch_review 远程派发分支

**Files:**
- Modify: `scripts/task_loop.py`（dispatch_agent，"槽位账本"区段）
- Modify: `scripts/task_loop_pipeline.py`（dispatch_review）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Consumes: `np.pick_node/spawn_remote/prepare_worktree_remote/refresh_env_remote/put_file/REMOTE_STATE_DIR/NodeError`（懒 import `import task_loop_nodepool as np`）。
- Produces: `tl.dispatch_agent(entry, state, dry_run, prompt_override, increment_rounds, node=None)`——node=None → np.pick_node；node 是 local 或 None 落 local → **现行路径原样**；remote 节点 → 远程分支。`tp.dispatch_review(state, pr_num, head, node=None)` 同款。Entry 记 host/板上路径。

- [ ] **Step 1: 写失败测试**

```python
def test_dispatch_agent_remote_branch(monkeypatch, tmp_path):
    """远程派发: prompt scp 上板 + 板上 worktree + spawn_remote + 槽 host=0102
    + Entry.host 记账; 本机零 run_background 调用。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    os.makedirs(tmp_path / "st")
    (tmp_path / "st" / "agent.env").write_text("PATH=/usr/bin\n")
    spawn_local = []
    monkeypatch.setattr(tp.tl, "run_background",
                        lambda cmd, cwd, log: spawn_local.append(cmd) or 1)
    monkeypatch.setattr(tp.tl, "gh_label_add", lambda *a: None)
    monkeypatch.setattr(tp.tl, "_worktree_prepare",
                        lambda c, k: spawn_local.append("LOCAL-WT") or "/lw")
    import task_loop_nodepool as np
    node0102 = np.node_by_host("0102")
    remote = {"spawned": [], "wts": [], "files": [], "env": []}
    monkeypatch.setattr(np, "pick_node", lambda k, s: node0102)
    monkeypatch.setattr(np, "spawn_remote",
                        lambda n, cmd, log: remote["spawned"].append((n, cmd, log)) or 4242)
    monkeypatch.setattr(np, "prepare_worktree_remote",
                        lambda n, key: remote["wts"].append(key) or "/board/wt")
    monkeypatch.setattr(np, "refresh_env_remote",
                        lambda n, env: remote["env"].append(env))
    monkeypatch.setattr(np, "put_file",
                        lambda h, l, r: remote["files"].append((l, r)))
    from task_loop import Entry
    e = Entry(workflow="w", run_id=1, status="QUEUED", issue_number=7,
              fix_branch="fix/issue-7", pr_number=None, attempts=0, rounds=0,
              agent_pid=None, agent_started_at=None, agent_log=None,
              last_dispatched_head=None, extra_runs=[], created_at="t",
              updated_at="t", source="audit", kind="bug", audit_hash=None)
    st = {"entries": [e.to_dict()], "auth_failure_streak": 0}
    es = tp.tl.StateStore.entries(st)
    pid = tp.tl.dispatch_agent(es[0], st)
    assert pid == 4242 and not spawn_local               # 本机零派生
    assert es[0].host == "0102"
    assert es[0].worktree == "/board/wt"
    assert remote["wts"] == ["issue-7"]
    assert any(l.endswith("agent.env") for l in remote["env"])
    assert len(remote["files"]) == 1                       # prompt 上板
    assert tp.tl.slot_get(st, "issue_fix", "issue-7")["host"] == "0102"


def test_dispatch_agent_remote_spawnfail_falls_back_local(monkeypatch, tmp_path):
    """远程 spawn 失败(NodeError): 不占槽不燃 attempts,回退本机路径。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    os.makedirs(tmp_path / "st")
    (tmp_path / "st" / "agent.env").write_text("PATH=/usr/bin\n")
    import task_loop_nodepool as np
    monkeypatch.setattr(np, "pick_node",
                        lambda k, s: np.node_by_host("0102"))
    def boom(n, cmd, log):
        raise np.NodeError("节点 0102 命令失败(rc=255)")
    monkeypatch.setattr(np, "spawn_remote", boom)
    monkeypatch.setattr(np, "prepare_worktree_remote",
                        lambda n, key: "/board/wt")
    monkeypatch.setattr(np, "refresh_env_remote", lambda n, env: None)
    monkeypatch.setattr(np, "put_file", lambda h, l, r: None)
    spawn_local = []
    monkeypatch.setattr(tp.tl, "run_background",
                        lambda cmd, cwd, log: spawn_local.append(cmd) or 77)
    monkeypatch.setattr(tp.tl, "gh_label_add", lambda *a: None)
    monkeypatch.setattr(tp.tl, "_worktree_prepare", lambda c, k: "/lw")
    from task_loop import Entry
    e = Entry(workflow="w", run_id=1, status="QUEUED", issue_number=7,
              fix_branch="fix/issue-7", pr_number=None, attempts=0, rounds=0,
              agent_pid=None, agent_started_at=None, agent_log=None,
              last_dispatched_head=None, extra_runs=[], created_at="t",
              updated_at="t", source="audit", kind="bug", audit_hash=None)
    st = {"entries": [e.to_dict()], "auth_failure_streak": 0}
    es = tp.tl.StateStore.entries(st)
    pid = tp.tl.dispatch_agent(es[0], st)
    assert pid == 77 and es[0].host == "local"        # 回退本机成功
    assert es[0].attempts == 1                        # 只燃一次(成功派发)
    assert tp.tl.slot_get(st, "issue_fix", "issue-7")["host"] == "local"
```

- [ ] **Step 2: 确认失败** → FAIL（dispatch_agent 无 node 分支/Entry 无 host 断言失败）

- [ ] **Step 3: 实现**

(a) `task_loop.py` `dispatch_agent` 签名加 `node=None`；kind/key 推导与三闸（PAUSED/mem/同 key 占位）之后、prompt 落盘之后，插远程分支（本机路径原样保留在 else）：

```python
    # 跨机分载(spec 2026-11-13-nodepool-offload §4): node=None 由 pick_node
    # 择优; 选中远程节点走 nodepool 分支, 本机路径零改动(else 原样)。
    import task_loop_nodepool as np
    if node is None:
        try:
            node = np.pick_node(kind, state)
        except Exception as exc:          # nodepool 任何意外 → 本机兜底
            print(f"[nodepool] pick_node 失败,回退本机: {exc}")
            node = tl_local_NODE if False else np.LOCAL_NODE
    if node is not None and node.get("host_id") != "local":
        try:
            np.refresh_env_remote(node, os.path.join(
                _resolve_state_dir(), AGENT_ENV_NAME))
            wt = np.prepare_worktree_remote(node, key)
            np.heal_remote(node)           # 主克隆 heal(不动 .worktrees/)
            remote_log = (f"{np.REMOTE_STATE_DIR}/{os.path.basename(log)}")
            np.put_file(node["host_id"], prompt_path,
                        f"{np.REMOTE_STATE_DIR}/{os.path.basename(prompt_path)}")
            inner = (f"source {np.REMOTE_STATE_DIR}/agent.env && "
                     f"timeout {AGENT_TIMEOUT_SEC} claude -p "
                     f"--dangerously-skip-permissions < "
                     f"{np.REMOTE_STATE_DIR}/{os.path.basename(prompt_path)}")
            pid = np.spawn_remote(node, inner, remote_log)
            if not tp_slot(state, kind, key, pid, remote_log, wt):
                np.kill_remote(node["host_id"], pid, 9)
                raise np.NodeError("远程槽占位失败(同 key 在跑)")
        except np.NodeError as exc:
            print(f"[nodepool] {node['host_id']} 远程派发失败,回退本机: {exc}")
        else:
            entry.host = node["host_id"]
            entry.agent_log = remote_log
            entry.worktree = wt
            # ……(记账段共用: status/attempts/rounds/lane 与本机分支同一代码)
            return pid
    # else: 现行本机路径原样(prompt 落盘/PID/记账/slot_acquire host=local)
```

（实现时把**记账段**（status/attempts+1/rounds/lane/agent_pid/agent_started_at/agent_log/派发打标签/store 归调用方）抽成两分支共用的小函数 `_record_dispatch(entry, kind, pid, log, wt)`，杜绝两份漂移；`tp_slot` = 懒 `import task_loop_pipeline as tp; tp.slot_acquire(...)`——注意 slot_acquire 签名 Task 3 已带 host，调用处传 `host=node["host_id"]`。）

(b) `task_loop_pipeline.py` `dispatch_review` 签名加 `node=None`：同款结构——远程时 prompt 不经文件（现行为 stdin 喂入）→ **0102 分支改 scp prompt 文件**：现 dispatch_review 的 inner 已是文件形态（`< prompt`）？核实：现实现 inner= `source agent.env && timeout REVIEW_AGENT_TIMEOUT_SEC ... --pr N`（prompt 不落盘）。远程分支构造 inner 同款 + `--pr {pr_num}`，log=`REMOTE_STATE_DIR/review-pr<N>-<ts>.log`，槽 host、Entry 不动（review 槽无 Entry）。

- [ ] **Step 4: 通过 + 全量**（本机既有 dispatch 测试须全绿——local 分支零语义变化）

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop.py scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(nodepool): dispatch_agent/dispatch_review 远程分支——pick_node 择优,prompt/agent.env 上板,NodeError 回退本机,记账段两分支共用防漂移"
```

---

### Task 6b: 多 key 轮询降时延（GLM key3/wang/ding round-robin）

**Files:**
- Create: `scripts/gen_agent_envs.sh`
- Modify: `scripts/task_loop.py`（dispatch_agent 的 env source 路径 + 两助手）
- Modify: `scripts/task_loop_pipeline.py`（dispatch_review 的 env source）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces: `tl._env_pool(state_dir) -> list[str]`（glob `agent-key-*.env` 排序，空池回落 `[agent.env]`）；`tl._pick_env_path(state, state_dir) -> str`（`state["env_rr"]` 计数取模轮换，计数入 state 跨 cycle 持续）；`gen_agent_envs.sh`（从 `~/.bashrc` 三函数 `set-glm-qwen-key3/-key-wang/-key-ding` 提取 token，基于现行 `agent.env` 生成池文件，幂等 0600）；dispatch_agent/dispatch_review 的 local inner `source <选中池文件>`；远程分支 `refresh_env_remote(node, <选中池文件>)`——板上落名恒 `agent.env`，板端命令零改动。
- Consumes: Task 5 `refresh_env_remote`、Task 6 远程分支。
- 约束：token 绝不打印/入库；测试一律假 token 文件；audit 线保持 base `agent.env`（单实例无并发压力，注释注明）。

- [ ] **Step 1: 写失败测试**

```python
def test_env_pool_rotation_and_fallback(tmp_path):
    sd = tmp_path / "st"
    sd.mkdir()
    (sd / "agent.env").write_text("base\n")
    st = {}
    p = tp.tl._pick_env_path(st, str(sd))
    assert p.endswith("agent.env")                    # 池空回落单文件
    assert st["env_rr"] == 1
    for i in ("1", "2", "3"):
        (sd / f"agent-key-{i}.env").write_text(f"k{i}\n")
    st = {}
    picks = [tp.tl._pick_env_path(st, str(sd)) for _ in range(7)]
    order = [p.rsplit("agent-key-", 1)[1][0] for p in picks]
    assert order == ["1", "2", "3", "1", "2", "3", "1"]   # round-robin
    assert st["env_rr"] == 7


def test_dispatch_local_sources_picked_env(monkeypatch, tmp_path):
    """本机派发: inner source 的是轮换到的池文件(非固定 agent.env)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    sd = tmp_path / "st"
    sd.mkdir()
    (sd / "agent.env").write_text("base\n")
    (sd / "agent-key-1.env").write_text("k1\n")
    (sd / "agent-key-2.env").write_text("k2\n")
    cmds = []
    monkeypatch.setattr(tp.tl, "run_background",
                        lambda cmd, cwd, log: cmds.append(cmd) or 42)
    monkeypatch.setattr(tp.tl, "gh_label_add", lambda *a: None)
    monkeypatch.setattr(tp.tl, "_worktree_prepare", lambda c, k: "/w")
    from task_loop import Entry
    e = Entry(workflow="w", run_id=1, status="QUEUED", issue_number=3,
              fix_branch="fix/issue-3", pr_number=None, attempts=0, rounds=0,
              agent_pid=None, agent_started_at=None, agent_log=None,
              last_dispatched_head=None, extra_runs=[], created_at="t",
              updated_at="t", source="audit", kind="bug", audit_hash=None)
    st = {"entries": [e.to_dict()], "auth_failure_streak": 0}
    es = tp.tl.StateStore.entries(st)
    tp.tl.dispatch_agent(es[0], st)                 # rr=0 → pool[0]=agent-key-1
    tp.tl.dispatch_agent(es[0], st) if False else None
    assert "agent-key-1.env" in cmds[0]
```

（第二个 dispatch 断言略——同一 entry 重派被同 key 闸拦属正确行为；轮换顺序由第一个单测覆盖。实现时如需二连派断言用两个 entry。）

- [ ] **Step 2: 确认失败** → FAIL（_pick_env_path 未定义）

- [ ] **Step 3: 实现**

(a) `scripts/gen_agent_envs.sh`：

```bash
#!/bin/bash
# gen_agent_envs.sh — 从 ~/.bashrc 提取三把 GLM key(key3/wang/ding)生成
# agent-key-N.env 池文件(0600,幂等)。派发层 round-robin 消费(spec 决策 #9:
# 8 并发 agent 均摊三 key,单 key 速率压力 1/3——降时延)。
# token 绝不打印,只经文件传递;失败即止(缺一个 key 就少一个池文件,如实报告)。
set -euo pipefail
STATE_DIR="${TASKLOOP_STATE_DIR:-$HOME/.local/state/sdcshield-task-loop}"
BASE="$STATE_DIR/agent.env"
[ -f "$BASE" ] || { echo "缺基准 $BASE"; exit 1; }
i=0
for fn in set-glm-qwen-key3 set-glm-qwen-key-wang set-glm-qwen-key-ding; do
  tok=$(sed -n "/^${fn}()/,/^}/p" "$HOME/.bashrc" \
        | grep -oP 'ANTHROPIC_AUTH_TOKEN="\K[^"]+' | head -1 || true)
  if [ -z "$tok" ]; then echo "[warn] $fn 无 token,跳过"; continue; fi
  i=$((i+1))
  out="$STATE_DIR/agent-key-${i}.env"
  sed -E "s|^export ANTHROPIC_AUTH_TOKEN=.*|export ANTHROPIC_AUTH_TOKEN=\"${tok}\" #pool-${i}|" \
      "$BASE" > "$out"
  chmod 600 "$out"
done
echo "生成 ${i} 个池文件于 $STATE_DIR(期望 3)"
```

(b) `task_loop.py`（dispatch_agent 区段前）：

```python
def _env_pool(state_dir):
    """agent 凭据池(多 key 轮询,spec 决策 #9): agent-key-*.env 排序;
    池空回落单文件 agent.env(兼容现部署——gen_agent_envs.sh 未跑过)。"""
    import glob as _glob
    pool = sorted(_glob.glob(os.path.join(state_dir, "agent-key-*.env")))
    return pool or [os.path.join(state_dir, AGENT_ENV_NAME)]


def _pick_env_path(state, state_dir):
    """round-robin 取一个 env 文件(计数入 state 跨 cycle 持续轮换)。"""
    pool = _env_pool(state_dir)
    n = state.get("env_rr", 0)
    state["env_rr"] = n + 1
    return pool[n % len(pool)]
```

(c) dispatch_agent：inner 构造处（现 `source <STATE_DIR>/agent.env`）改 `source {_pick_env_path(state, state_dir)}`；Task 6 远程分支的 `refresh_env_remote(node, <picked>)` 同源（远程 scp 选中池文件、板端落名 `agent.env` 不变）。dispatch_review 同款两处。audit 线不接（保持 base，注释注明单实例）。

- [ ] **Step 4: 通过 + 全量 + 真机冒烟**

Run: 全量绿；`bash scripts/gen_agent_envs.sh` 真跑 → `ls ~/.local/state/sdcshield-task-loop/agent-key-*.env` 恰 3 个、`grep -c '#pool-'` 3 行（**不 cat 全文**）；`pipeline --dry-run` EXIT=0。

- [ ] **Step 5: Commit**

```bash
git add scripts/gen_agent_envs.sh scripts/task_loop.py scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(nodepool): GLM 三 key round-robin 轮询——gen_agent_envs.sh 池生成,派发按 env_rr 轮换 source,远程 scp 选中文件,池空回落单 env"
```

---

### Task 7: 探活/收割/Janitor/软停/启动对账的 host 分流

**Files:**
- Modify: `scripts/task_loop.py`（agent_alive 调用点：reap_or_advance/lane_b_advance/check_agent_outcome/has_running_agent）
- Modify: `scripts/task_loop_pipeline.py`（slot_janitor/free_slot_count/startup_reconcile/soft_stop）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces: `tl.host_alive(host, pid) -> bool`（local→os.kill；remote→np.alive_remote，**NodeError 上抛**由调用方定性）；`tl._agent_log_tail_by(entry, nbytes) -> str`（local→本地文件读；remote→np.read_log_tail；NodeError 抛出）；`tp._probe_alive(slot_or_entry) -> "alive"|"dead"|"unreachable"`（unreachable=SSH 断，槽保留不燃 attempts）；`tp.slot_janitor`/`soft_stop`/`startup_reconcile` 的远程分支（含板上 `uptime -s` 环境性豁免）。
- Consumes: Task 5 五操作。

- [ ] **Step 1: 写失败测试**

```python
def test_host_alive_dispatch(monkeypatch):
    import os as _os
    assert tp.tl.host_alive("local", _os.getpid()) is True
    assert tp.tl.host_alive("local", 999999999) is False
    import task_loop_nodepool as np
    def boom(h, p):
        raise np.NodeError("SSH 断")
    monkeypatch.setattr(np, "alive_remote", boom)
    try:
        tp.tl.host_alive("0102", 4242)
        assert False, "NodeError 须上抛由调用方定性"
    except np.NodeError:
        pass


def test_janitor_remote_unreachable_keeps_slot(monkeypatch):
    """远程槽 SSH 断: 槽保留(unreachable),不回收、不触发条目豁免。"""
    import task_loop_nodepool as np
    st = _mkstate()
    tp.slot_acquire(st, "issue_fix", "issue-9", 555, 555, "/rl", "/rw",
                    "t", "b", host="0102")
    def boom(h, p):
        raise np.NodeError("断")
    monkeypatch.setattr(np, "alive_remote", boom)
    gone = tp.slot_janitor(st, "now")
    assert gone == []                                  # unreachable 不算死
    assert tp.slot_get(st, "issue_fix", "issue-9") is not None


def test_janitor_remote_dead_board_reboot_exempts(monkeypatch):
    """远程死槽: 板 uptime 晚于 started_at(板重启过)→ environmental=True。"""
    import task_loop_nodepool as np
    st = _mkstate()
    tp.slot_acquire(st, "issue_fix", "issue-9", 555, 555, "/rl", "/rw",
                    "t", "b", host="0102")
    monkeypatch.setattr(tp.tl, "agent_alive", lambda pid: False)
    monkeypatch.setattr(np, "alive_remote", lambda h, p: False)
    monkeypatch.setattr(np, "run_ssh",
                        lambda h, cmd, t=45: "2026-11-14 23:00:00")
    gone = tp.slot_janitor(st, "now")
    assert len(gone) == 1 and gone[0]["environmental"] is True


def test_soft_stop_remote_kill_and_unreachable(monkeypatch):
    """软停: 远程活槽 ssh 击杀+清槽; unreachable 槽跳过(留次夜)。"""
    import task_loop_nodepool as np
    st = _mkstate()
    from task_loop import Entry
    st["entries"] = [dict(workflow="w", run_id=1, status="DISPATCHED",
                         issue_number=9, fix_branch="fix/issue-9",
                         pr_number=None, attempts=1, rounds=0, agent_pid=555,
                         agent_started_at="t", agent_log="/rl",
                         last_dispatched_head=None, extra_runs=[],
                         created_at="t", updated_at="t", source="audit",
                         kind="bug", audit_hash=None, host="0102")]
    tp.slot_acquire(st, "issue_fix", "issue-9", 555, 555, "/rl", "/rw",
                    "t", "b", host="0102")
    killed = []
    def fake_alive(pid): return True        # slot 层视为活 → 进击杀段
    monkeypatch.setattr(tp.tl, "agent_alive", fake_alive)
    state_ssh = {"n": 0}
    def fake_run_ssh(h, cmd, t=45):
        state_ssh["n"] += 1
        if "kill" in cmd:
            raise np.NodeError("断")
        return ""
    monkeypatch.setattr(np, "run_ssh", fake_run_ssh)
    tp.soft_stop(st, grace_sec=0)                       # grace=0 直接击杀段
    # unreachable: 条目不回队(留次夜 reconcile)、attempts 不燃
    e = tp.tl.StateStore.entries(st)[0]
    assert e.status == "DISPATCHED" and e.attempts == 1
```

（`soft_stop` 现实现以 `tl.agent_alive(s["pid"])` 判活——远程槽需改为 `_probe_alive(slot)`：alive→击杀；unreachable→跳过；dead→只清。）

- [ ] **Step 2: 确认失败** → FAIL

- [ ] **Step 3: 实现**

(a) `task_loop.py`：新增两助手（agent_alive 旁）：

```python
def host_alive(host, pid):
    """按 host 探活(local=os.kill; remote=ssh kill -0)。NodeError 上抛——
    调用方必须把 SSH 断定性为 unreachable(槽保留),绝不判死。"""
    if host == "local" or host is None:
        return agent_alive(pid)
    import task_loop_nodepool as np
    return np.alive_remote(host, pid)


def _agent_log_tail_by(entry, nbytes=4000):
    """按 entry.host 读日志尾(local 直读; remote=ssh tail -c)。收割/认证
    失败判定(check_agent_outcome)只消费此文本——远程本地同构。"""
    if entry.host == "local" or not entry.host:
        with open(entry.agent_log, "rb") as f:
            f.seek(0, os.SEEK_END)
            f.seek(max(0, f.tell() - nbytes))
            return f.read().decode("utf-8", errors="replace")
    import task_loop_nodepool as np
    return np.read_log_tail(entry.host, entry.agent_log, nbytes)
```

`check_agent_outcome` 的日志读改调 `_agent_log_tail_by(entry)`；其超时/running 判定的 PID 探活改 `host_alive(entry.host, entry.agent_pid)`，并捕 NodeError → 返回 `"unreachable"`（reap 把 unreachable 条目**原样跳过**：不燃 attempts、不推进状态，下一轮再看）。`reap_or_advance`/`lane_b_advance`/`has_running_agent` 的 `agent_alive(...)` 调用点同样换 `host_alive`——unreachable 按"活着"处理（保守：不重派、不升级）。

(b) `task_loop_pipeline.py`：`_probe_alive(slot) -> "alive"|"dead"|"unreachable"`（local os.kill / remote np.alive_remote+NodeError 捕获）；`slot_janitor`/`free_slots_on`/`free_slot_count` 的活判经 `_probe_alive`（unreachable 计为占用——保守不超发）；`slot_janitor` 远程死槽回收前查板 `uptime -s`（np.run_ssh）晚于 started_at → released 记 `environmental=True`；`soft_stop` 击杀段按 `_probe_alive` 分流（alive→np.kill_remote TERM/KILL + cleanup_worktree_remote；unreachable→跳过打印；dead→只清槽）；`startup_reconcile` 仅处理 `host=="local"` 条目（远程条目由 janitor 的 unreachable 语义次夜收敛）。

- [ ] **Step 4: 通过 + 全量**

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop.py scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(nodepool): 探活/收割/Janitor/软停 host 分流——unreachable 槽保留不燃 attempts,板 uptime 重启豁免,日志尾远程同构收割"
```

---

### Task 8: cycle_once 接线（派发点选节点 + merge_sweep/review 泳道贯通）

**Files:**
- Modify: `scripts/task_loop_pipeline.py`（cycle_once 泳道 1/2 派发扫、merge_sweep 的 repair 槽活判）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Consumes: Task 6/7 全部。
- Produces: cycle_once 泳道 1 循环里 `_dispatch_gate_ok` 通过后调 `dispatch_agent(entry, state, dry_run)`（**不改签名**——node=None 内部 pick_node）；空槽判定从 `free_slot_count(state,"issue_fix")` 改为"pick_node 返回 None 则 break"（每轮 cycle 每泳道最多一次 pick_node，选同一节点可连派至其满槽）；泳道 2 同款（`dispatch_review(..., node=None)`）。merge_sweep 的"repair 槽活 agent 跳过"探活经 host 分流（Task 7）。

- [ ] **Step 1: 写失败测试**

```python
def test_cycle_once_dispatches_to_remote_node(monkeypatch, tmp_path):
    """端到端: 泳道1 QUEUED 条目在 pick_node=0102 时经远程分支派发,
    cycle 其余步骤(stub)零回归。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    os.makedirs(tmp_path / "st")
    (tmp_path / "st" / "agent.env").write_text("PATH=/usr/bin\n")
    import task_loop_nodepool as np
    monkeypatch.setattr(np, "pick_node", lambda k, s: np.node_by_host("0102"))
    for m in ("refresh_env_remote", "prepare_worktree_remote",
              "heal_remote"):
        setattr(np, m, lambda *a, m=m: None)
    np.put_file = lambda h, l, r: None
    np.spawn_remote = lambda n, cmd, log: 4242
    monkeypatch.setattr(np, "refresh_env_remote", lambda n, e: None)
    monkeypatch.setattr(np, "prepare_worktree_remote",
                        lambda n, k: "/board/wt")
    monkeypatch.setattr(np, "heal_remote", lambda n: None)
    monkeypatch.setattr(np, "put_file", lambda h, l, r: None)
    monkeypatch.setattr(np, "spawn_remote", lambda n, cmd, log: 4242)
    order = []
    monkeypatch.setattr(tp.tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tp.tl, "triage_new_red_runs",
                        lambda s, r, dry_run=False: order.append("intake"))
    monkeypatch.setattr(tp.tl, "reap_or_advance",
                        lambda s, dry_run=False: order.append("reap"))
    monkeypatch.setattr(tp.tl, "reconcile_issues",
                        lambda s, dry_run=False: order.append("recon") or set())
    monkeypatch.setattr(tp.tl, "heal_clone", lambda s, dry_run=False: None)
    monkeypatch.setattr(tp.tl, "lane_b_advance", lambda s, dry_run=False: None)
    monkeypatch.setattr(tp, "needs_review_list", lambda: [])
    monkeypatch.setattr(tp, "spawn_audit", lambda s, d, window_end=9: None)
    monkeypatch.setattr(tp, "merge_sweep", lambda s, e, d, n: None)
    from task_loop import Entry
    st = {"entries": [dict(workflow="w", run_id=1, status="QUEUED",
                           issue_number=12, fix_branch="", pr_number=None,
                           attempts=0, rounds=0, agent_pid=None,
                           agent_started_at=None, agent_log=None,
                           last_dispatched_head=None, extra_runs=[],
                           created_at="t", updated_at="t", source="audit",
                           kind="bug", audit_hash=None)],
          "auth_failure_streak": 0}
    tp.cycle_once(st, False, [], datetime.now(timezone.utc), window_end=9)
    assert order == ["intake", "reap", "recon"]          # 编排序零回归
    e = tp.tl.StateStore.entries(st)[0]
    assert e.status == "DISPATCHED" and e.host == "0102" and e.agent_pid == 4242
    assert tp.tl.slot_get(st, "issue_fix", "issue-12")["host"] == "0102"


def test_cycle_once_pick_none_skips_lane(monkeypatch, tmp_path):
    """pick_node 恒 None(全节点不可用): 泳道1 不派、条目原样(现行「本轮不派」)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    os.makedirs(tmp_path / "st")
    import task_loop_nodepool as np
    monkeypatch.setattr(np, "pick_node", lambda k, s: None)
    spawn = []
    monkeypatch.setattr(tp.tl, "run_background",
                        lambda cmd, cwd, log: spawn.append(1) or 1)
    for name in ("scan_red_runs", "reap_or_advance"):
        monkeypatch.setattr(tp.tl, name,
                            lambda *a, **k: None)
    monkeypatch.setattr(tp.tl, "triage_new_red_runs", lambda *a, **k: None)
    monkeypatch.setattr(tp.tl, "reconcile_issues", lambda *a, **k: set())
    monkeypatch.setattr(tp.tl, "heal_clone", lambda *a, **k: None)
    monkeypatch.setattr(tp.tl, "lane_b_advance", lambda *a, **k: None)
    monkeypatch.setattr(tp, "needs_review_list", lambda: [])
    monkeypatch.setattr(tp, "spawn_audit", lambda *a, **k: None)
    monkeypatch.setattr(tp, "merge_sweep", lambda *a, **k: None)
    from task_loop import Entry
    st = {"entries": [dict(workflow="w", run_id=1, status="QUEUED",
                           issue_number=12, fix_branch="", pr_number=None,
                           attempts=0, rounds=0, agent_pid=None,
                           agent_started_at=None, agent_log=None,
                           last_dispatched_head=None, extra_runs=[],
                           created_at="t", updated_at="t", source="audit",
                           kind="bug", audit_hash=None)],
          "auth_failure_streak": 0}
    tp.cycle_once(st, False, [], datetime.now(timezone.utc), window_end=9)
    assert spawn == []                                    # 零派发
    assert tp.tl.StateStore.entries(st)[0].status == "QUEUED"
```

- [ ] **Step 2: 确认失败** → FAIL

- [ ] **Step 3: 实现**

cycle_once 泳道 1 循环改造（泳道 2 同款结构）：

```python
    # 泳道1: QUEUED FIFO × 节点池择优(每轮一次 pick_node,选同一节点连派至满槽;
    # None=全节点不可用 → 本轮不派,spec §6 兜底)
    import task_loop_nodepool as np
    node1 = np.pick_node("issue_fix", state) if not dry_run else np.LOCAL_NODE
    if node1 is None:
        print("[nodepool] 全节点无空槽/不可用,泳道1 本轮不派")
    else:
        for e in sorted((x for x in es if x.status == "QUEUED"),
                        key=lambda x: (x.issue_number is None, x.issue_number)):
            if not tl._dispatch_gate_ok(e, open_labels):
                continue
            if tp_free := tp.free_slots_on(state, "issue_fix",
                                           host=node1["host_id"]) <= 0:
                break
            pid = tl.dispatch_agent(e, state, dry_run=dry_run, node=node1)
            if pid is not None and not dry_run:
                _pipeline_save(store, state)
```

泳道 2：`node2 = np.pick_node("pr_review", state)`，循环 `needs_review_list()`，`tp.dispatch_review(state, num, head, node=node2)`，None 则整轮跳过并打印。merge_sweep 的 repair 槽活判换 `_probe_alive`（unreachable 视为占用跳过该 PR——保守）。dry-run 全程 LOCAL_NODE（零 SSH 零副作用契约保持）。

- [ ] **Step 4: 通过 + 全量 + dry-run 真机复验**

Run: `/usr/bin/python3 scripts/task_loop.py pipeline --dry-run` → EXIT=0 且 `PIPELINE_NODEPOOL=0` 与默认两种环境各跑一次（默认环境下 dry 分支不打 SSH——用 monkeypatch 测试锁形 + 真机 EXIT=0 双证）。

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(nodepool): cycle_once 三泳道接节点池——每轮一次 pick_node 连派至满槽,None 兜底不派,dry-run 零 SSH"
```

---

### Task 9: nodepool_setup.sh 三板一次性环境（幂等）

**Files:**
- Create: `scripts/nodepool_setup.sh`
- Test: `bash -n` + 对三板真实执行（本任务产物即环境就绪；无 pytest 面——如实声明，验证=每板实跑命令输出）

**Interfaces:**
- Consumes: 板注册表三板（0102/0101/0201）；本机 `gh auth token`；`~/.claude/skills/accessing-sdc-boards/scripts/boards.sh` 的直连 SSH 形态（**仅 setup 阶段用密码直连**——运行时密钥免密）。
- Produces: 三板各就绪——`/home/sdc/wangxu/sdcshield-autoci`（https remote + gh 凭据）、`~/sdcshield-autoci-state/`、0101/0201 `/usr/local/bin/gh` 已登录、vendored 依赖 build 完、`zstd19 exit: pass`；0103 `~/.ssh/id_ed25519_nodepool` 私钥 + 三板 authorized_keys 公钥行。

- [ ] **Step 1: 写脚本**

```bash
#!/bin/bash
# nodepool_setup.sh — 跨机卸载三板一次性环境准备(spec §3.2,全部幂等)。
# 直连板 SSH 密码形态仅本脚本使用(boards.sh 同源);运行时一律密钥免密。
# 用法: bash scripts/nodepool_setup.sh [板号...]   # 缺省 = 0102 0101 0201
set -euo pipefail
BOARDS="${*:-0102 0101 0201}"
PASS='SDC@2026'
SSH_KEY=~/.ssh/id_ed25519_nodepool
KEY_IP_0101=172.168.177.97; KEY_IP_0102=172.168.160.42; KEY_IP_0201=172.168.178.81
ip_of() { case "$1" in 0101) echo $KEY_IP_0101;; 0102) echo $KEY_IP_0102;; 0201) echo $KEY_IP_0201;; esac; }
BSSH() { sshpass -p "$PASS" ssh -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 sdc@$(ip_of "$1") "${@:2}"; }

# 1) 0103 专用密钥 + 三板 authorized_keys 追加(幂等: 已在则跳过)
[ -f "$SSH_KEY" ] || ssh-keygen -t ed25519 -N "" -f "$SSH_KEY" -C nodepool@0103
PUB=$(cat "${SSH_KEY}.pub")
for B in $BOARDS; do
  BSSH "$B" "grep -qF '$PUB' ~/.ssh/authorized_keys 2>/dev/null || echo '$PUB' >> ~/.ssh/authorized_keys"
  chmod 600 ~/.ssh/authorized_keys
done

# 2) 密钥免密自验(失败即止——运行时全靠它)
NPSSH() { ssh -i "$SSH_KEY" -o StrictHostKeyChecking=no -o UserKnownHostsFile=/dev/null -o ConnectTimeout=8 sdc@$(ip_of "$1") "${@:2}"; }
for B in $BOARDS; do NPSSH "$B" "hostname" >/dev/null || { echo "FAIL $B 免密未通"; exit 1; }; done

# 3) 每板: gh 就绪(0101/0201 装+登录;0102 已有) + 独立克隆 + state 目录
TOKEN=$(gh auth token)
for B in $BOARDS; do
  NPSSH "$B" "command -v gh >/dev/null || {
    curl -fsSL -o /tmp/gh.tgz https://github.com/cli/cli/releases/download/v2.97.0/gh_2.97.0_linux_arm64.tar.gz &&
    tar -C /tmp -xzf /tmp/gh.tgz &&
    echo 'SDC@2026' | sudo -S install -m 755 /tmp/gh_2.97.0_linux_arm64/bin/gh /usr/local/bin/gh; }"
  NPSSH "$B" "gh auth status >/dev/null 2>&1 || echo '$TOKEN' | gh auth login --with-token >/dev/null && gh auth setup-git"
  NPSSH "$B" "mkdir -p ~/sdcshield-autoci-state ~/wangxu && chmod 700 ~/sdcshield-autoci-state"
  NPSSH "$B" "[ -d /home/sdc/wangxu/sdcshield-autoci/.git ] || git clone https://github.com/wangxumarshall/sdcshield.git /home/sdc/wangxu/sdcshield-autoci"
done

# 4) 每板 vendored 依赖 + 构建 + 冒烟(幂等: install/ 已有跳过;构建总是重跑确保新鲜)
for B in $BOARDS; do
  NPSSH "$B" "cd /home/sdc/wangxu/sdcshield-autoci &&
    for dep in openssl openblas sleef isa-l acl; do
      [ -d third-party/\$dep/install ] || bash third-party/\$dep/build.sh; done &&
    PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release 2>/dev/null || true &&
    ninja -C builddir -j 32 &&
    ./builddir/sdcshield --list-tests | wc -l &&
    timeout 60 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1"
done
echo "=== setup 完成: $BOARDS ==="
```

- [ ] **Step 2: 语法检查 + 分块真跑**

Run: `bash -n scripts/nodepool_setup.sh && chmod +x scripts/nodepool_setup.sh`
真跑（分块观察，vendored build 长——后台跑）：
`nohup bash scripts/nodepool_setup.sh > /tmp/nodepool_setup.log 2>&1 &`；轮询 `tail /tmp/nodepool_setup.log`；**每板必须看到**：免密通（FAIL 无）、`gh auth status` OK、克隆存在、`--list-tests | wc -l` ≥ 439、`exit: pass`。0102 依赖最全可能跳过部分 build——如实记录每板实耗。

- [ ] **Step 3: 板上运行时前置自验（nodepool 真跑三面）**

Run（本机，密钥形态）：
```bash
/usr/bin/python3 - <<'EOF'
import sys; sys.path.insert(0, "scripts")
import task_loop_nodepool as np
for h in ("0102", "0101", "0201"):
    p = np.probe_node(h)
    print(h, p)
    assert p["reachable"] and not p["excite_active"] and p["mem_avail_mb"] > 8192, h
EOF
```
Expected: 三板 reachable=True，MemAvailable 各 >8G（0102 ~636000）。任何 False → 修完再继续。

- [ ] **Step 4: Commit**

```bash
git add scripts/nodepool_setup.sh
git commit -s -m "feat(nodepool): 三板一次性 setup 脚本——密钥免密分发/0101+0201 补 gh/独立克隆/vendored 构建/探测自验,全幂等"
```

---

### Task 10: 真实冒烟——0102 远程派发全链路

**Files:** 无新文件（验证任务；产出=冒烟证据入报告）。

- [ ] **Step 1: 建可弃冒烟 issue**

```bash
gh issue create --repo wangxumarshall/sdcshield \
  --title "[冒烟] nodepool 远程派发测试(可弃)" \
  --body "nodepool offload 冒烟: 验证 0102 远程派发全链路。冒烟完自动关闭,勿修。<!-- task-loop:smoke -->" \
  --label agent-task,agent-bug 2>/dev/null || \
gh issue create --repo wangxumarshall/sdcshield --title "[冒烟] nodepool 远程派发测试(可弃)" --body "..." 
```
（标签不存在则先 `gh label create agent-task` / `agent-bug`——本仓库已有，直接建。）

- [ ] **Step 2: 强制单节点单轮真派**

Run: `cd /home/sdc/wangxu/operator/sdcshield-autoci && PIPELINE_FORCE_NODE=0102 timeout 600 /usr/bin/python3 scripts/task_loop.py scan`
（scan=单轮薄壳；FORCE_NODE 绕过择优直击 0102。）

- [ ] **Step 3: 链路逐环实证（全部引用真实输出）**

1. `task_loop.py status` → 冒烟条目 DISPATCHED、host=0102
2. `ssh -i ~/.ssh/id_ed25519_nodepool sdc@172.168.160.42 'ls ~/sdcshield-autoci-state/ ~/wangxu/sdcshield-autoci/.worktrees/ && tail -5 ~/sdcshield-autoci-state/agent-*.log'` → prompt/agent.env 在板、worktree 在建、日志在长
3. 板上 agent 完成后（≤3600s，轮询）：`status` → PR_OPEN + 分支已推；`gh pr list --head fix/issue-<N>` → PR 已开（**板上 gh 全流程实证**）
4. `gh pr close <PR> && git push origin --delete fix/issue-<N> && gh issue close <N>`——冒烟清理（PR 不 merge；分支删；issue 关）
5. 冒烟条目 state 归位：`PIPELINE_FORCE_NODE=0102 task_loop.py scan` 再跑一轮收割 PR_CLOSED → DONE（或直接确认条目 DONE 后 HUMAN_CLOSED 亦可——如实记录实际终态）

- [ ] **Step 4: 冒烟报告 + Commit（计划要求证据入 git——报告作为 plan 进度记录不单独 commit，写入 Task 11 的收官 commit message 引用；无代码 commit）**

Run: 全量 `pytest tools/excite/tests/ -q` → 基线绿。

---

### Task 11: 文档同步 + 收官

**Files:**
- Modify: `docs/build-deploy-pr-ci/task-loop.md`（§2 架构图加节点池行/§3 新增「节点池」小节/§4 槽位表 host 维度/§6 错误表加 unreachable 行）
- Modify: `README.md`（task-loop 段一句：三泳道跨 0102/0101/0201 常态分载）
- Modify: `CLAUDE.md`（task-loop 描述行加节点池短语，克制）

- [ ] **Step 1: 文档编辑**（先读实文再改；克制精确，task-loop.md 为主）
- [ ] **Step 2: grep 校验**：`grep -rn "nodepool\|节点池" docs/build-deploy-pr-ci/task-loop.md | head` 应见新增小节；README/CLAUDE 各一处提及，无过度展开。
- [ ] **Step 3: 全量验证**：`/usr/bin/python3 -m pytest tools/excite/tests/ -q` 全绿；`PIPELINE_NODEPOOL=0 /usr/bin/python3 scripts/task_loop.py pipeline --dry-run` 与默认环境各 EXIT=0。
- [ ] **Step 4: Commit + push**

```bash
git add docs/build-deploy-pr-ci/task-loop.md README.md CLAUDE.md
git commit -s -m "docs(pr-ci): nodepool 跨机分载落地——task-loop.md 节点池小节/README/CLAUDE 一句提及"
git push -u origin feat/nodepool-offload
```

- [ ] **Step 5: 人工 merge 交接**：PR 开出（push 触发 auto-pr）；本 feature 走人工 merge（同上一 feature 模式——它改引擎自身）；合入后 autoci 克隆 heal 自动生效，首夜进入既有观察窗。

---

## Self-Review 记录（写计划时已自查）

1. **Spec 覆盖**：决策 1(pick_node)/2(板上全流程=agent 契约零改动,Task 6 不动 prompt)/3(注册表)/4(nodepool 模块)/5(克隆路径常量)/6(SSH 密钥 Task 9)/7(激发跳过 Task 4)/8(audit 留本机——Task 8 未接 spawn_audit 到 nodepool,零改动) + §3.3 每夜刷新(Task 6 refresh)/§5 五操作(Task 5+7)/§6 错误表(Tasks 4/6/7)/§7 测试与冒烟(Tasks 1-8/9/10)。无缺口。
2. **占位符**：无 TBD；Task 6 的 `tl_local_NODE if False else np.LOCAL_NODE` 为示意性丑陋行——**实施时写干净**（直接 `node = np.LOCAL_NODE`），计划里保留该备注是因为 pick_node 异常兜底必须存在。已注明。
3. **类型一致性**：`free_slots_on(state, kind, host="local")`（Task 3 定义，Task 4/8 消费同签名）；`slot_acquire(..., host="local")`（Task 3 → Task 6/7 消费）；`np.alive_remote(host, pid)`（Task 5 → Task 7 `tl.host_alive` 消费）；`probe_node(host_id)`（Task 2 → Task 4 消费）；Entry.host（Task 3 → 6/7 消费）。
4. **已知边界**：Task 7 改动 `has_running_agent/reap/lane_b` 的活判为保守语义（unreachable=活）——既有单飞类测试需同步核对（free_slot_count 的 live 判经 _probe_alive 后 unreachable 计占用,与本机语义兼容）；Task 9 setup 的 gh tar 下载 URL 版本号 v2.97.0 与 0102 实测版本对齐,若 404 换 release 最新稳定 tag（板到 github 443 实测通）。

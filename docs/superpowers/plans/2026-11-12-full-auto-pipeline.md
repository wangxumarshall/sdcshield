# 全自动夜间流水线（nightly pipeline）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 task_loop 引擎改造成每晚 23:00 一条流水线（`task_loop.py pipeline`）：三泳道并行（issue 修复 ×2 / PR 检视 ×2 / 意见修复 ×2）+ audit 线，merge 门过门槛自动 squash merge（含 docs-only 快速通道），精确归因自动 revert。

**Architecture:** 编排器 `scripts/task_loop_pipeline.py`（新模块，`import task_loop as tl` 复用现有车道/收割/对账，`tl.*` 命名空间 monkeypatch 同现有测试模式）；`scripts/task_loop.py` 做外科手术式改造（槽位化派发、Entry 字段、动态轮次、argparse 子命令）。槽位租约存 state 顶层 `slots`；每 agent 独立 `git worktree`；merge 由编排器确定性代码执行。

**Tech Stack:** Python 3（`/usr/bin/python3`，零新依赖）、flock、git worktree、gh CLI（凭据 = 现有 `agent.env` + gh 本机登录）、pytest 9.1.1。

**Spec:** `docs/superpowers/specs/2026-11-12-full-auto-pipeline-design.md`（决策表 10 行 + §2-§13 + 附录 A 代码坐标）。实施前必读；本计划不从零重述设计理由。

## Global Constraints

- 解释器一律 `/usr/bin/python3`；**零第三方新依赖**（标准库 + 既有 import）。
- 注释/标识符风格贴合 `scripts/task_loop.py` 现状：中文注释、`_` 前缀私有、模块级函数、print 带 `[tag]` 前缀。
- 测试：`/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`；新测试进 `tools/excite/tests/test_task_loop_pipeline.py`，沿用 `import task_loop as tl` + `tmp_path`/`monkeypatch` 模式；**零触网零真跑 claude**（一切经 monkeypatch 接缝）。
- state 写者纪律：**只有编排器/scan 进程写 state**；agent 子进程绝不写。
- 绝不自动 merge 用户非 docs PR；绝不自动 resolve review threads；用户 WIP 分支不派修复 agent（spec §12 红线）。
- 每任务一个 commit：`git commit -s`（DCO 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By）；分支 `feat/nightly-pipeline`；commit 前跑全量引擎测试。
- x86-64 代码零触碰（本特性纯 Python 脚手架，无 C 改动 → 无 C 回归义务；规则 2 的 zstd19 回归不适用于纯脚本任务，验证义务 = 单测 + dry-run）。
- 行号引用以 2026-11-12 的 `scripts/task_loop.py`（2804 行版）为准；实施时若行号漂移，以函数名/注释锚点定位。

---

### Task 1: pipeline 模块骨架——常量、窗口判定、内存闸、boot_id

**Files:**
- Create: `scripts/task_loop_pipeline.py`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces（后续任务全部依赖）:
  - `PIPELINE_LOCK_NAME="pipeline.lock"`、`NO_AUTO_MERGE_NAME="NO_AUTO_MERGE"`、`SLOT_KINDS={"issue_fix":2,"pr_review":2,"pr_repair":2,"audit":1}`、`PIPELINE_CYCLE_SEC=300`、`SOFT_STOP_GRACE_SEC=1800`、`REVIEW_AGENT_TIMEOUT_SEC=5400`、`REVIEW_PER_HEAD_PER_NIGHT=2`、`DOCS_QUIET_SEC=300`
  - `min_mem_gb() -> int`（env `PIPELINE_MIN_MEM_GB`，默认 8）
  - `mem_available_mb() -> int`（MemAvailable 与 cgroup 限额取小）
  - `mem_gate_ok() -> bool`
  - `boot_id() -> str`
  - `in_window(now: datetime, start_hour=23, end_hour=9) -> bool`（本地时间；跨午夜区间）

- [ ] **Step 1: 写失败测试**

```python
# tools/excite/tests/test_task_loop_pipeline.py
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                 "..", "..", "scripts"))
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
    assert tp.mem_available_mb() == 900000 // 1024 * 1024 // 1024 * 1024  # 见实现取整口径


def test_mem_gate_ok_threshold(monkeypatch):
    monkeypatch.setattr(tp, "mem_available_mb", lambda: 4096)
    assert tp.mem_gate_ok() is False
    monkeypatch.setattr(tp, "mem_available_mb", lambda: 8192)
    assert tp.mem_gate_ok() is True


def test_boot_id_shape():
    # 真实 /proc 可读；形如 36 字符 hex-uuid
    v = tp.boot_id()
    assert len(v) == 36 and "-" in v
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -x -q`
Expected: FAIL（`ModuleNotFoundError: task_loop_pipeline`）

- [ ] **Step 3: 写实现**

```python
#!/usr/bin/env python3
"""task_loop_pipeline.py — 夜间全自动流水线编排器(spec 2026-11-12-full-auto-pipeline)。

cron 0 23 启动: 窗口循环(每 PIPELINE_CYCLE_SEC 一轮) → 三泳道并行派发
(issue 修复/pr 检视/意见修复, 各 SLOT_KINDS 个槽) + audit 后台线 + merge 门
(引擎 PR 全门槛自动 squash merge; docs-only 快速通道) + 精确归因 revert。
09:00 软停: 不再派新 → 等在跑 ≤SOFT_STOP_GRACE_SEC → 击杀(不计 attempts,
条目回队列) → 清 worktree → 写夜间报告。设计细节一律见 spec,本文件不重述。
"""
import fcntl
import os
import re
import subprocess
import sys
import time
from datetime import datetime

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import task_loop as tl  # noqa: E402  (与 tl.pr_loop 同款命名空间引用,测试可 monkeypatch)
from task_loop import GhError, StateStore  # noqa: E402

PIPELINE_LOCK_NAME = "pipeline.lock"
NO_AUTO_MERGE_NAME = "NO_AUTO_MERGE"
SLOT_KINDS = {"issue_fix": 2, "pr_review": 2, "pr_repair": 2, "audit": 1}
PIPELINE_CYCLE_SEC = 300
SOFT_STOP_GRACE_SEC = 1800
REVIEW_AGENT_TIMEOUT_SEC = 5400
REVIEW_PER_HEAD_PER_NIGHT = 2
DOCS_QUIET_SEC = 300
MEMINFO_PATH = "/proc/meminfo"


def min_mem_gb() -> int:
    """内存闸阈值(GB)——env PIPELINE_MIN_MEM_GB 调用时读(测试可 setenv)。"""
    try:
        return int(os.environ.get("PIPELINE_MIN_MEM_GB", "8"))
    except ValueError:
        return 8


def _cgroup_mem_limit_mb() -> "int | None":
    """cgroup 内存限额(MB);无限制/不可读 → None(不参与取小)。"""
    for path in ("/sys/fs/cgroup/memory.max",           # cgroup v2 统一层级
                 "/sys/fs/cgroup/memory/memory.limit_in_bytes"):  # v1
        try:
            with open(path) as f:
                raw = f.read().strip()
        except OSError:
            continue
        if raw in ("", "max"):
            return None
        v = int(raw)
        if v > (1 << 40):          # v1 时常给 2^63 量级哨兵 = 无限制
            return None
        return v // (1024 * 1024)
    return None


def mem_available_mb() -> int:
    """MemAvailable(MB) 与 cgroup 限额取小——两者任一吃紧都算吃紧。"""
    avail = None
    try:
        with open(MEMINFO_PATH) as f:
            for line in f:
                if line.startswith("MemAvailable:"):
                    avail = int(line.split()[1]) // 1024
                    break
    except OSError:
        return 0
    if avail is None:
        return 0
    limit = _cgroup_mem_limit_mb()
    return min(avail, limit) if limit else avail


def mem_gate_ok() -> bool:
    """派发前内存闸: MemAvailable ≥ 阈值才放行新 agent(宁慢勿爆,spec §7)。"""
    return mem_available_mb() >= min_mem_gb() * 1024


def boot_id() -> str:
    """本机启动标识——槽位记录启动时 boot_id,收割时对比即知 agent 是否死于重启。"""
    with open("/proc/sys/kernel/random/boot_id") as f:
        return f.read().strip()


def in_window(now: datetime, start_hour: int = 23, end_hour: int = 9) -> bool:
    """流水线窗口(本地时间): 23:00–09:00,跨午夜区间。"""
    h = now.hour
    return h >= start_hour or h < end_hour
```

注：`test_mem_available_reads_proc_meminfo` 的断言以实现口径为准——MemAvailable 按整 MB
取整（`9000000 kB // 1024 = 8789 MB`）。把该测试断言写精确值 `8789`（提交前按实际输出校准一次）。

- [ ] **Step 4: 跑测试确认通过**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -x -q`
Expected: PASS（6 passed）

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): 流水线模块骨架——窗口判定/内存闸(ProcMeminfo×cgroup取小)/boot_id"
```

---

### Task 2: Entry 扩展字段（lane/worktree/merged_at/merged_sha）+ 注释 19→18 校正

**Files:**
- Modify: `scripts/task_loop.py:192-272`（Entry dataclass / to_dict / from_dict / 218-220 注释）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`（新增用例）

**Interfaces:**
- Produces: `Entry.lane: str`（默认 `""`）、`Entry.worktree: Optional[str]`、
  `Entry.merged_at: Optional[str]`、`Entry.merged_sha: Optional[str]`；
  JSON 键 `"lane"/"worktree"/"merged_at"/"merged_sha"`；`from_dict` 对缺省容错
  （旧 state.json 加载即升格）。
- Consumes: 无。

- [ ] **Step 1: 写失败测试**

```python
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
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py::test_entry_new_fields_roundtrip_and_legacy -x -q`
Expected: FAIL（`TypeError: unexpected keyword 'lane'`）

- [ ] **Step 3: 实现**

`scripts/task_loop.py` Entry（216 行 `audit_hash` 之后）追加 4 字段；`to_dict`（243 行
`"audit_hash"` 之后）追加 4 键；`from_dict`（271 行之后）追加
`lane=d.get("lane", "")`、`worktree=d.get("worktree")`、`merged_at=d.get("merged_at")`、
`merged_sha=d.get("merged_sha")`；把 218-220 的注释「构造方须给全 19 字段」改为
「22 字段」（顺带校正旧漂移——现 dataclass 实为 18 字段+4 新字段）：

```python
    lane: str                         # 泳道: issue_fix | pr_repair(空=未入槽/历史条目)
    worktree: Optional[str]           # 该 agent 的 git worktree 路径(派发时建,收割时清)
    merged_at: Optional[str]          # 自动 merge 时间(ISO8601;人工 merge 不记)
    merged_sha: Optional[str]         # squash merge 产生的新 main commit sha(revert 归因主数据)
```

- [ ] **Step 4: 全量引擎测试**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
Expected: PASS（旧用例全绿——新增字段 from_dict 容错保证旧 state 兼容）

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): Entry 增 lane/worktree/merged_at/merged_sha 四字段(旧 state 加载容错)+注释 19→22 校正"
```

---

### Task 3: 槽位账本（state.slots + acquire/release/janitor + 启动对账）

**Files:**
- Modify: `scripts/task_loop_pipeline.py`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `slot_acquire(state, kind, key, pid, pgid, log, worktree, started_iso, boot) -> bool`（同 key 已占用 → False；超 SLOT_KINDS[kind] → False）
  - `slot_release(state, kind, key) -> None`
  - `slot_get(state, kind, key) -> Optional[dict]`
  - `free_slot_count(state, kind) -> int`（判死：`tl.agent_alive(s["pid"])` 为假即清——janitor 语义内联）
  - `slot_janitor(state, now_iso) -> list[dict]`（清死槽，返回被清清单含 `environmental` 布尔：`s["boot"] != boot_id()` 为真）
  - `startup_reconcile(state) -> None`（boot_id 变化期间死亡的在跑条目 → 干净回队不计 attempts：`DISPATCHED→QUEUED`、`BOT_FIX→PR_OPEN`，清 `agent_pid/agent_started_at/last_dispatched_head`，置 `updated_at`；其余死槽仅清槽不动条目——条目留给 `tl.reap_or_advance` 正常判档）
- Consumes: `tl.agent_alive`、`tl.StateStore.entries`、Task 1 的 `boot_id`。

- [ ] **Step 1: 写失败测试**

```python
def _mkstate():
    return {"entries": [], "auth_failure_streak": 0}


def test_slot_acquire_respects_kind_max_and_dup_key():
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
    st = _mkstate()
    tp.slot_acquire(st, "pr_review", "pr-9", 201, 200, "/l", None, "t", "b")
    assert tp.free_slot_count(st, "pr_review") == 2   # 死 PID → janitor 清 → 空槽
    tp.slot_release(st, "pr_review", "pr-9")
    assert tp.slot_get(st, "pr_review", "pr-9") is None


def test_slot_janitor_marks_environmental(monkeypatch):
    st = _mkstate()
    tp.slot_acquire(st, "issue_fix", "issue-1", 999999, 0, "/l", "/w",
                    "t", "old-boot")
    monkeypatch.setattr(tp, "boot_id", lambda: "new-boot")
    gone = tp.slot_janitor(st, "now")
    assert len(gone) == 1 and gone[0]["environmental"] is True


def test_startup_reconcile_requeues_without_attempt(monkeypatch):
    from task_loop import Entry
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
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -x -q`
Expected: FAIL（`slot_acquire` 不存在）

- [ ] **Step 3: 实现**（追加到 task_loop_pipeline.py）

```python
def _slots(state, kind):
    return state.setdefault("slots", {}).setdefault(kind, [])


def slot_get(state, kind, key):
    for s in _slots(state, kind):
        if s["key"] == key:
            return s
    return None


def slot_acquire(state, kind, key, pid, pgid, log, worktree,
                 started_iso, boot):
    """占用一槽;同 key 互斥(同条目/同 PR 至多 1 agent)、满槽拒绝。"""
    live = [s for s in _slots(state, kind) if tl.agent_alive(s["pid"])]
    if slot_get(state, kind, key) is not None:
        return False
    if len(live) >= SLOT_KINDS.get(kind, 0):
        return False
    _slots(state, kind).append(
        {"key": key, "pid": pid, "pgid": pgid, "log": log,
         "worktree": worktree, "started_at": started_iso, "boot": boot})
    return True


def slot_release(state, kind, key):
    s = _slots(state, kind)
    state["slots"][kind] = [x for x in s if x["key"] != key]


def free_slot_count(state, kind):
    """空槽数 = 槽上限 − 活 PID 占用数(死槽即时回收)。"""
    live = sum(1 for s in _slots(state, kind) if tl.agent_alive(s["pid"]))
    return max(0, SLOT_KINDS.get(kind, 0) - live)


def slot_janitor(state, now_iso):
    """清死槽并回收其 worktree;返回被清清单(boot 变化 → environmental=True,
    供启动对账做 attempts 豁免;软停/正常退场走 slot_release 不经此)。"""
    released = []
    for kind in SLOT_KINDS:
        alive = []
        for s in _slots(state, kind):
            if tl.agent_alive(s["pid"]):
                alive.append(s)
                continue
            released.append({"kind": kind, **s,
                             "environmental": s.get("boot") != boot_id()})
            if s.get("worktree"):
                tl.worktree_cleanup(s["worktree"])
        state["slots"][kind] = alive
    return released


def startup_reconcile(state):
    """pipeline 启动对账: 重启(bot_id 变)期间死亡的 agent 条目干净回队、
    不计 attempts(#611 attempts=4 假性升级教训);其余死槽只清槽,条目判档
    仍交 tl.reap_or_advance(单飞语义外的正常 dead/timeout 记账)。"""
    current_boot = boot_id()
    for info in slot_janitor(state, _now_local_iso()):
        if not info["environmental"]:
            continue
        for e in tl.StateStore.entries(state):
            same_key = str(e.issue_number or e.run_id) == str(info["key"]) \
                or info["key"] == f"issue-{e.issue_number or e.run_id}"
            if same_key and e.status in tl.AGENT_STATES:
                e.status = "PR_OPEN" if e.status == "BOT_FIX" else "QUEUED"
                e.agent_pid = None
                e.agent_started_at = None
                e.last_dispatched_head = None
                e.updated_at = _now_local_iso()
                print(f"[reconcile] {info['key']} 死于重启(boot 变化)——"
                      f"干净回队 {e.status},不计 attempts")
```

`_now_local_iso()`（模块内工具，放 Task 1 区段末尾一并提交也行——放本任务实现里）:

```python
def _now_local_iso():
    return datetime.now().astimezone().isoformat(timespec="seconds")
```

`tl.worktree_cleanup` 在 Task 4 实现——本任务测试里 worktree 清理路径先传 `None`/monkeypatch：
在测试文件顶部加 `monkeypatch.setattr(tl, "worktree_cleanup", lambda p: None)` 的 fixture
或直接在本任务把 `worktree_cleanup` 的占位实现写进 task_loop.py（推荐——见 Task 4 Step 3
的最终实现，本任务先提交其最小版本）：

```python
# scripts/task_loop.py(heal_clone 区段后)
def worktree_cleanup(path: str) -> None:
    """清 agent worktree(Task 4 完成完整实现: git worktree remove --force)。"""
    if not path:
        return
```

- [ ] **Step 4: 跑测试确认通过**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -x -q`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_pipeline.py scripts/task_loop.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): 槽位账本——slots 租约(同key互斥/满槽拒绝)/janitor(boot_id 环境性标记)/启动对账(重启死亡不计 attempts)"
```

---

### Task 4: worktree 隔离 + dispatch_agent 槽位化 + heal 主克隆无条件化

**Files:**
- Modify: `scripts/task_loop.py:1282-1393`（dispatch_agent）、`:2017-2063`（heal_clone）、
  新增 `_worktree_prepare` / 完整 `worktree_cleanup`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`（新用例）+
  `tools/excite/tests/test_task_loop.py`（修复受影响的单飞断言用例）

**Interfaces:**
- Produces:
  - `tl._worktree_prepare(clone_dir: str, key: str) -> str`（`git fetch` 后
    `git worktree add --detach <clone>/.worktrees/<key> origin/main`；目录已存在先清理）
  - `tl.worktree_cleanup(path: str) -> None`（`git worktree remove --force` + `git worktree prune`；路径不存在静默）
  - `tl.dispatch_agent(entry, state, dry_run=False, prompt_override=None, increment_rounds=False)`
    ——**签名不变**；内部闸门由 `has_running_agent` 改为槽位 + 内存闸 + PAUSED；cwd 由固定
    clone_dir 改为 per-entry worktree；派发成功写槽（kind = `pr_repair` if increment_rounds
    else `issue_fix`）与 `entry.lane/entry.worktree`
  - `tl.has_running_agent(state)` 保留（status/观测用），不再做派发闸门
- Consumes: Task 1 `mem_gate_ok`、`boot_id`（经懒 import `import task_loop_pipeline as tp`）、
  Task 3 `tp.slot_acquire`。

- [ ] **Step 1: 写失败测试**

```python
class _FakeStore:                       # 槽位化派发的零触网桩
    def __init__(self, state):
        self.state = state
    def save(self, s):
        pass


def test_dispatch_two_issue_agents_then_full(monkeypatch, tmp_path):
    """issue 泳道 2 槽: 第 3 个 QUEUED 条目被拒;内存不足也被拒。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "cl"))
    os.makedirs(tmp_path / "st")
    (tmp_path / "st" / "agent.env").write_text("PATH=/usr/bin\n")
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: int(time.time()) % 100000 + 1)
    monkeypatch.setattr(tl, "_worktree_prepare",
                        lambda c, k: f"/wt/{k}")
    monkeypatch.setattr(tl, "gh_label_add", lambda *a: None)
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: True)
    from task_loop import Entry

    def mk(n):
        return Entry(workflow="w", run_id=n, status="QUEUED", issue_number=n,
                     fix_branch="", pr_number=None, attempts=0, rounds=0,
                     agent_pid=None, agent_started_at=None, agent_log=None,
                     last_dispatched_head=None, extra_runs=[], created_at="t",
                     updated_at="t", source="audit", kind="bug",
                     audit_hash=None, lane="", worktree=None,
                     merged_at=None, merged_sha=None)

    st = {"entries": [mk(1).to_dict(), mk(2).to_dict(), mk(3).to_dict()],
          "auth_failure_streak": 0}
    es = tl.StateStore.entries(st)
    for e in es:
        tl.dispatch_agent(e, st)
    assert sum(e.status == "DISPATCHED" for e in es) == 2   # 2 槽用满
    assert tp.free_slot_count(st, "issue_fix") == 0
    assert tp.SLOT_KINDS["issue_fix"] == 2
    # 内存闸拒绝
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: False)
    es[2].status = "QUEUED"; es[2].agent_pid = None
    assert tl.dispatch_agent(es[2], st) is None
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excue/tests/test_task_loop_pipeline.py -k dispatch -x -q`（目录名以实际 `tools/excite` 为准）
Expected: FAIL（dispatch_agent 仍单飞全局布尔——第 2 个派发返回 None）

- [ ] **Step 3: 实现**

`scripts/task_loop.py` 改造：

(a) `heal_clone()`（2017-2063）：删除 2033-2035 的 `has_running_agent` 早退（agent 全在
worktree，主克隆每轮无条件 heal）；结尾追加 worktree 残留清理：

```python
    # pipeline 纪律: agent 全在 .worktrees/ 下跑,主克隆每轮 heal 后清孤儿 worktree
    # (槽位内的活 worktree 由 slot_janitor 按 key 清,此处只清不在槽里的残留)。
    live_wts = set()
    for kind_slots in (state.get("slots") or {}).values():
        live_wts.update(s.get("worktree") or "" for s in kind_slots)
    wt_root = os.path.join(clone_dir, ".worktrees")
    if os.path.isdir(wt_root):
        for name in os.listdir(wt_root):
            p = os.path.join(wt_root, name)
            if p not in live_wts:
                worktree_cleanup(p)
```

(b) 新增（`_git` 助手后）：

```python
def _worktree_prepare(clone_dir: str, key: str) -> str:
    """为 agent 建隔离 worktree(detach 于 origin/main),路径 <clone>/.worktrees/<key>。
    已存在(重派/残留)先清再建——worktree 是易耗品,基线永远 origin/main。"""
    path = os.path.join(clone_dir, ".worktrees", key)
    worktree_cleanup(path)
    _git(["fetch", "origin", "--prune"], clone_dir=clone_dir)
    _git(["worktree", "add", "--detach", path, "origin/main"],
         clone_dir=clone_dir)
    return path


def worktree_cleanup(path: str) -> None:
    """移除 agent worktree;git 侧失手再兜底 shutil(留目录会让 prepare 恒失败)。"""
    if not path or not os.path.exists(path):
        return
    try:
        subprocess.run(["git", "worktree", "remove", "--force", path],
                       capture_output=True, timeout=GIT_TIMEOUT_SEC * 3)
        subprocess.run(["git", "worktree", "prune"],
                       capture_output=True, timeout=GIT_TIMEOUT_SEC)
    finally:
        shutil.rmtree(path, ignore_errors=True)
```

（`import shutil` 顶部补齐。）

(c) `dispatch_agent()` 闸门段（1321-1327）替换为三道前置闸；占位在 spawn 之后
（真 PID 才能进账本，避免哨兵 PID 被 janitor 误清）。最终闸门顺序：
`_paused() → mem_gate_ok → free_slot_count>0（早退）→ spawn → slot_acquire
（失败则 killpg + return None）→ 状态/rounds/attempts 记账 → save`。

```python
    import task_loop_pipeline as tp        # 懒 import 防循环(pipeline 顶层 import 本模块)
    kind = "pr_repair" if increment_rounds else "issue_fix"
    key = f"issue-{entry.issue_number or entry.run_id}" if kind == "issue_fix" \
        else f"pr-{entry.pr_number}"
    if _paused():
        print(f"[paused] PAUSED 熔断生效,拒绝为 run {entry.run_id} 派发"
              f"(更新 agent.env 后删除 PAUSED 恢复)")
        return None
    if not tp.mem_gate_ok():
        print(f"[mem-gate] MemAvailable 不足({tp.min_mem_gb()}GB 闸),"
              f"本轮不派新 agent")
        return None
    if tp.free_slot_count(state, kind) <= 0:
        print(f"[slots] {kind} 满槽,拒绝为 {key} 派发")
        return None
```

spawn 段（原 1370 行 `pid = run_background(...)`）之后、记账段（原 1371 行）之前插入：

```python
    if not tp.slot_acquire(state, kind, key, pid, pgid=pid, log=log,
                           worktree=wt, started_iso=_now_iso(),
                           boot=tp.boot_id()):
        try:
            os.killpg(pid, 9)      # 同 key 已在跑: 击杀刚 spawn 的,不留孤儿
        except OSError:
            pass
        print(f"[slots] {kind} 槽 {key} 占位失败(同 key 在跑),已击杀 PID {pid}")
        return None
```

(d) `dispatch_agent()` cwd（1370 行）改 `wt = _worktree_prepare(clone_dir, key)`，
`run_background(cmd, cwd=wt, log=log)`；记账段（1371-1383）追加：

```python
    entry.lane = kind
    entry.worktree = wt
```

(e) `heal_clone` 的调用约束同步：`scan` 编排 2166 行保持（heal 在派发扫之前），无需改。

- [ ] **Step 4: 全量引擎测试（含修复受影响旧用例）**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
旧 `test_task_loop.py` 中依赖「全局单飞」的用例（grep `single-flight|has_running_agent`）
按新语义改期望：同 kind 第 3 个被拒、pr_repair 与 issue_fix 互不占位。
Expected: PASS 全绿。

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop.py scripts/task_loop_pipeline.py tools/excite/tests/
git commit -s -m "feat(pipeline): dispatch 槽位化+per-entry worktree 隔离;heal 主克隆无条件化并清孤儿 worktree;内存闸接入派发"
```

---

### Task 5: 动态轮次 max_rounds_for + pr_files 助手 + lane_b 接入

**Files:**
- Modify: `scripts/task_loop.py:80`（`MAX_ROUNDS` 区段）、`:1927`（lane_b 升级判定）、
  新增 `max_rounds_for` / `pr_files`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `tl.MAX_ROUNDS_SIMPLE=2`、`tl.MAX_ROUNDS_COMPLEX=3`、
    `tl.ROUND_SIMPLE_MAX_DIFF_LINES=100`、`tl.ROUND_SIMPLE_MAX_FILES=5`
    （`MAX_ROUNDS=4` 常量删除，全仓 grep 清零引用）
  - `tl.pr_files(pr_number: int) -> list[str]`（`gh_json_idem(["pr","view",str(n),
    "--json","files","--jq",".files[].path"])`；GhError → `[]`）
  - `tl.max_rounds_for(entry, pr, files=None) -> int`（docs-only（`pr_files` 全 `.md`）
    → 2；`pr.additions+pr.deletions ≤ 100` 且 `len(files) ≤ 5` → 2；其余 → 3；
    `files is None` 时现拉 `pr_files(pr_number)`；PRData 无 additions/deletions 字段时按复杂=3）
- Consumes: `pr_loop.PRData`（`additions/deletions` 若有；无则经 `gh pr view --json
  additions,deletions` 兜底——实施时先读 `pr_loop.py` 的 PRData 字段定义取齐，不新增字段
  就地用现有）。

- [ ] **Step 1: 写失败测试**

```python
def test_max_rounds_for_matrix(monkeypatch):
    from types import SimpleNamespace as NS
    monkeypatch.setattr(tl, "pr_files", lambda n: ["docs/a.md"])
    assert tl.max_rounds_for(None, NS(pr_number=1, additions=5, deletions=5)) == 2
    monkeypatch.setattr(tl, "pr_files",
                        lambda n: ["a.c", "b.c", "c.c", "d.c", "e.c"])
    assert tl.max_rounds_for(
        None, NS(pr_number=2, additions=60, deletions=30)) == 2   # ≤100 行 ∧ ≤5 文件
    monkeypatch.setattr(tl, "pr_files", lambda n: ["a.c"] * 6)
    assert tl.max_rounds_for(
        None, NS(pr_number=3, additions=60, deletions=30)) == 3   # 6 文件 → 复杂
    assert tl.max_rounds_for(
        None, NS(pr_number=4, additions=200, deletions=50)) == 3
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py::test_max_rounds_for_matrix -x -q`
Expected: FAIL（`max_rounds_for` 未定义）

- [ ] **Step 3: 实现**

```python
# task_loop.py 常量区(MAX_ROUNDS 位置替换)
MAX_ROUNDS_SIMPLE = 2     # docs-only / 简单 diff: 检视-修复 ≤2 轮(spec 决策表 #10)
MAX_ROUNDS_COMPLEX = 3    # 其余: ≤3 轮
ROUND_SIMPLE_MAX_DIFF_LINES = 100
ROUND_SIMPLE_MAX_FILES = 5


def pr_files(pr_number: int) -> list[str]:
    """PR 变更文件路径清单(docs-only 判定与轮次复杂度共用)。"""
    try:
        out = gh_json_idem(["pr", "view", str(pr_number), "--json", "files",
                             "--jq", ".files[].path"])
        return [l for l in out.splitlines() if l.strip()]
    except GhError as exc:
        print(f"[warn] PR #{pr_number} files 拉取失败(按复杂处理): {exc}")
        return []


def _docs_only(files: list[str]) -> bool:
    return bool(files) and all(f.endswith(".md") for f in files)


def max_rounds_for(entry, pr, files=None) -> int:
    """动态轮次上限(spec 决策表 #10): docs-only → 2;增删 ≤100 行 ∧ ≤5 文件
    → 2;其余 → 3。拉不到文件清单按复杂(诚实降级,宁严勿松)。"""
    files = files if files is not None else pr_files(pr.pr_number)
    if _docs_only(files):
        return MAX_ROUNDS_SIMPLE
    add = getattr(pr, "additions", None)
    dele = getattr(pr, "deletions", None)
    if add is None or dele is None:
        return MAX_ROUNDS_COMPLEX
    if add + dele <= ROUND_SIMPLE_MAX_DIFF_LINES and \
            len(files) <= ROUND_SIMPLE_MAX_FILES:
        return MAX_ROUNDS_SIMPLE
    return MAX_ROUNDS_COMPLEX
```

lane_b（1927-1932）替换：

```python
        files = pr_files(entry.pr_number)
        max_rounds = max_rounds_for(entry, pr, files)
        if entry.rounds >= max_rounds:
            _escalate(entry, state,
                      f"bot 意见处置已 {entry.rounds} 轮(≥ 动态上限 "
                      f"{max_rounds}: docs/简单 2、复杂 3)仍未收敛",
                      "人工接手处置剩余 bot 意见并 merge", dry_run)
            continue
```

- [ ] **Step 4: 全量测试**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
Expected: PASS（旧用例中若有断言 `MAX_ROUNDS` 的同步改）

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop.py tools/excite/tests/
git commit -s -m "feat(pipeline): 检视-修复轮次按复杂度动态 2-3(docs-only/简单 diff ≤100 行∧≤5 文件 →2),取代 MAX_ROUNDS=4"
```

---

### Task 6: 泳道 2——pr_local_review 增 --pr 单 PR CLI + 检视派发器

**Files:**
- Modify: `scripts/pr_local_review.py`（argparse 增 `--pr N`；main 循环同 `--all-open`
  复用 `run_review`）
- Modify: `scripts/task_loop_pipeline.py`（新增检视派发器）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `pr_local_review.py` CLI：`--pr <N>` 与 `--all-open` 互斥；`--pr` 路径 = 取 head sha
    后 `run_review(N, head)`（已有函数，pr_local_review.py:310）
  - `tp.needs_review_list() -> list[dict]`（`gh pr list --state open --json number,
    headRefOid,title` → 过滤 `pr_local_review.already_reviewed`）
  - `tp.dispatch_review(state, pr_num, head) -> Optional[int]`（spawn
    `python3 scripts/pr_local_review.py --pr N`，`timeout REVIEW_AGENT_TIMEOUT_SEC`，
    记槽 `pr_review`、`state["review_tries"][f"{pr}:{head}"]` 计数 ≤2/夜，log 路径
    `<STATE_DIR>/review-pr<N>-<ts>.log`）
  - `tp.reset_review_tries(state) -> None`（pipeline 启动时清零——"每晚"边界）
- Consumes: Task 3 槽位、Task 1 内存闸、`tl._resolve_state_dir`、`tl.run_background`。

- [ ] **Step 1: 写失败测试**

```python
def test_dispatch_review_slot_and_try_cap(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    st = _mkstate()
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: True)
    pids = iter(range(500, 510))
    monkeypatch.setattr(tl, "run_background", lambda cmd, cwd, log: next(pids))
    monkeypatch.setattr(tp, "boot_id", lambda: "b")
    assert tp.dispatch_review(st, 640, "h1") is not None
    assert tp.dispatch_review(st, 641, "h2") is not None
    assert tp.dispatch_review(st, 642, "h3") is None       # 第 3 槽满
    assert tp.dispatch_review(st, 640, "h1") is None       # 同 PR 同 head 第 2 次仍可(≤2)
    assert tp.dispatch_review(st, 640, "h1") is None        # 第 3 次超每晚上限 2
    assert st["review_tries"]["640:h1"] >= 2
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -k review -x -q`
Expected: FAIL（`dispatch_review` 未定义）

- [ ] **Step 3: 实现**

`scripts/pr_local_review.py` main（argparse 段）增：

```python
    ap.add_argument("--pr", type=int, default=None,
                    help="只检视指定 PR 的当前 head(与 --all-open 互斥)")
    ...
    if args.pr is not None and getattr(args, "all_open", False):
        ap.error("--pr 与 --all-open 互斥")
    if args.pr is not None:
        head = _gh(["pr", "view", str(args.pr),
                   "--json", "headRefOid", "--jq", ".headRefOid"]).strip()
        run_review(args.pr, head)
        return 0
```

`task_loop_pipeline.py`：

```python
def reset_review_tries(state):
    state["review_tries"] = {}


def dispatch_review(state, pr_num, head):
    """泳道2 派发: 每晚每 (pr,head) ≤REVIEW_PER_HEAD_PER_NIGHT 次;满槽/内存
    不足返回 None。spawn 命令与修复 agent 同形(setsid+timeout),凭据经 agent.env。"""
    if not mem_gate_ok():
        return None
    tries = state.setdefault("review_tries", {})
    tk = f"{pr_num}:{head}"
    if tries.get(tk, 0) >= REVIEW_PER_HEAD_PER_NIGHT:
        print(f"[review] PR #{pr_num} head {head[:8]} 已试 {tries[tk]} 次/晚,"
              f"留待明晚")
        return None
    key = f"pr-{pr_num}"
    if free_slot_count(state, "pr_review") <= 0:
        return None
    ts = datetime.now().strftime("%Y%m%dT%H%M%S")
    log = os.path.join(tl._resolve_state_dir(), f"review-pr{pr_num}-{ts}.log")
    inner = (f"source {shlex.quote(os.path.join(tl._resolve_state_dir(), 'agent.env'))}"
             f" && timeout {REVIEW_AGENT_TIMEOUT_SEC} /usr/bin/python3 "
             f"{shlex.quote(os.path.join(tl._resolve_clone_dir(), 'scripts', 'pr_local_review.py'))}"
             f" --pr {pr_num}")
    cmd = f"nohup setsid bash -c {shlex.quote(inner)} >> {shlex.quote(log)} 2>&1 & echo $!"
    pid = tl.run_background(cmd, cwd=tl._resolve_clone_dir(), log=log)
    if not slot_acquire(state, "pr_review", key, pid, pid, log, None,
                        _now_local_iso(), boot_id()):
        try:
            os.killpg(pid, 9)
        except OSError:
            pass
        return None
    tries[tk] = tries.get(tk, 0) + 1
    return pid
```

（`import shlex` 顶部补齐。）

- [ ] **Step 4: 跑测试确认通过 + 全量**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add scripts/pr_local_review.py scripts/task_loop_pipeline.py tools/excite/tests/
git commit -s -m "feat(pipeline): 泳道2 检视派发器——pr_local_review 增 --pr 单 PR CLI;每晚每 head ≤2 次;5400s 超时;槽位+内存闸"
```

---

### Task 7: merge 门 evaluate_merge_gate（六门槛 + docs 快道 + 继承红 + 熔断）

**Files:**
- Modify: `scripts/task_loop_pipeline.py`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `tp.REQUIRED_CHECKS_FULL = ["lint", "git-sanity", "Build binary for CPU (Linux)",
    "Security / zizmor"]`（**实施首步：用 `gh pr view <现役PR> --json statusCheckRollup`
    校准显示名，与 ci-web-operations.md 建议集一致后写死**）
  - `tp.REQUIRED_CHECKS_DOCS = ["lint", "git-sanity"]`
  - `tp.GateResult = ("ok", "fast", "reasons")`（namedtuple：bool/bool/list[str]）
  - `tp.evaluate_merge_gate(state, entry, pr, files, now, no_auto_merge) -> GateResult`
  - `tp.docs_only(files) -> bool`、`tp.wip_gated(title) -> bool`、
    `tp.docs_ready(pr, now) -> bool`
  - `tp.baseline_reds(slug, workflow) -> set[str]`（main 最近一次同 workflow
    schedule/push run 的失败 job 名集合——继承红对照，PR #426/#343 先例）
- Consumes: `tl.pr_loop.classify_state`、`tl.gh_json_idem`、`tl._paused()`。

- [ ] **Step 1: 写失败测试**

```python
from types import SimpleNamespace as NS


def _pr(state_="open", draft=False, mergeable=True, title="T",
        local=True, latest=100000, opinions=0):
    return NS(state=state_, draft=draft, mergeable=mergeable, title=title,
             head_sha="abc1234", url="u", bot_done={
                 "codex": "x", "coderabbit": "x", "pr-agent": "x",
                 "greptile": "x",
                 "local-claude": "x" if local else None},
             latest_signal=None, opinion_count=opinions,
             pending_skew_opinions=0, annotations_fetch_failed=False)


def test_gate_full_path_engine_pr_ok(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    st = _mkstate()
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda pr, q, now: "READY_TO_MERGE")
    monkeypatch.setattr(tp, "checks_ok",
                        lambda pr, req, slug: (True, []))
    monkeypatch.setattr(tl, "_paused", lambda: False)
    e = NS(pr_number=1, issue_number=1, fix_branch="fix/issue-1", status="PR_OPEN")
    r = tp.evaluate_merge_gate(st, e, _pr(), ["a.c"], datetime.now(), False)
    assert r.ok is True and r.fast is False


def test_gate_rejects_user_wip_and_missing_ownership():
    st = _mkstate()
    r = tp.evaluate_merge_gate(st, None, _pr(title="WIP: feat/x"),
                               ["a.md"], datetime.now(), False)
    assert r.ok is False and "WIP" in " ".join(r.reasons)   # 非 docs 归属缺条目也拒


def test_gate_docs_fast_path(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    st = _mkstate()
    monkeypatch.setattr(tp, "checks_ok",
                        lambda pr, req, slug: (True, []))
    monkeypatch.setattr(tl, "_paused", lambda: False)
    from datetime import datetime, timezone, timedelta
    now = datetime.now(timezone.utc)
    pr = _pr(local=True, opinions=0)
    pr.latest_signal = now - timedelta(seconds=tp.DOCS_QUIET_SEC + 10)
    r = tp.evaluate_merge_gate(st, None, pr, ["a.md"], now, False)  # 用户 docs PR
    assert r.ok is True and r.fast is True


def test_gate_docs_with_opinions_not_ok():
    from datetime import datetime, timezone, timedelta
    now = datetime.now(timezone.utc)
    pr = _pr(local=True, opinions=2)
    pr.latest_signal = now - timedelta(seconds=400)
    r = tp.evaluate_merge_gate(_mkstate(), None, pr, ["a.md"], now, False)
    assert r.ok is False


def test_gate_no_auto_merge_and_paused(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    (tmp_path / "st").mkdir(parents=True)
    (tmp_path / "st" / "NO_AUTO_MERGE").write_text("")
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tp, "checks_ok", lambda pr, req, slug: (True, []))
    from datetime import datetime, timezone, timedelta
    now = datetime.now(timezone.utc)
    pr = _pr(); pr.latest_signal = now - timedelta(seconds=1000)
    r = tp.evaluate_merge_gate(_mkstate(), None, pr, ["a.md"], now, True)
    assert r.ok is False and "NO_AUTO_MERGE" in " ".join(r.reasons)
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -k gate -x -q`
Expected: FAIL（`evaluate_merge_gate` 未定义）

- [ ] **Step 3: 实现**

```python
GateResult = collections.namedtuple("GateResult", "ok fast reasons")
REQUIRED_CHECKS_FULL = ["lint", "git-sanity",
                        "Build binary for CPU (Linux)", "Security / zizmor"]
REQUIRED_CHECKS_DOCS = ["lint", "git-sanity"]
ENGINE_BRANCH_RE = re.compile(r"^fix/(issue-\d+|ci-\d+|revert-pr\d+)")


def docs_only(files):
    return bool(files) and all(f.endswith(".md") for f in files)


def wip_gated(title):
    return bool(title) and title.startswith("WIP:")


def checks_ok(pr, required, slug):
    """PR 的 check runs: required 集(按名字前缀匹配)全 success → True;
    红 job 集与 main 基线一致 → 继承红放行(spec §5 门槛 3)。"""
    try:
        rollup = tl.gh_json_idem(
            ["pr", "view", str(pr.pr_number), "--repo", slug,
             "--json", "statusCheckRollup", "--jq",
             ".statusCheckRollup[] | [.name, .conclusion] | @tsv"])
    except GhError as exc:
        return False, [f"check rollup 拉取失败: {exc}"]
    failed = set()
    conclusions = {}
    for line in rollup.splitlines():
        if not line.strip():
            continue
        name, _, concl = line.partition("\t")
        conclusions[name.strip()] = concl.strip()
        if concl.strip() not in ("success", "skipped", "SUCCESS"):
            failed.add(name.strip())
    missing = [r for r in required
               if not any(n.startswith(r) for n in conclusions)]
    if missing:
        return False, [f"required check 缺席: {missing}"]
    if not failed:
        return True, []
    base = baseline_reds(slug, getattr(pr, "workflow_name", None))
    if failed <= base:
        return True, [f"继承红放行: {sorted(failed)} ⊆ main 基线 {sorted(base)}"]
    return False, [f"红 job 非基线继承: {sorted(failed)}"]


def baseline_reds(slug, workflow):
    """main 最近一次同 workflow schedule/push run 的失败 job 名集合;拉不到 → 空
    (空集=不放行任何红,诚实从严)。"""
    if not workflow:
        return set()
    try:
        run_id = tl.gh_json_idem(
            ["run", "list", "--repo", slug, "--workflow", workflow,
             "--branch", tl.MONITORED_BRANCH,
             "--event", "schedule,push", "--limit", "1",
             "--json", "databaseId", "--jq", ".[0].databaseId"]).strip()
        jobs = tl.gh_json_idem(["run", "view", run_id, "--repo", slug,
                               "--json", "jobs", "--jq",
                               ".jobs[] | select(.conclusion==\"failure\") | .name"])
        return {l.strip() for l in jobs.splitlines() if l.strip()}
    except GhError:
        return set()


def docs_ready(pr, now):
    """docs 快道就绪: 第 5 bot 信号在 ∧ 静默窗 DOCS_QUIET_SEC 过 ∧ 零意见
    (annotations 完整性同 classify 语义)。"""
    if pr.bot_done.get("local-claude") is None:
        return False
    if pr.annotations_fetch_failed or pr.pending_skew_opinions > 0 \
            or pr.opinion_count > 0:
        return False
    if pr.latest_signal is None:
        return False
    return (now - pr.latest_signal).total_seconds() >= DOCS_QUIET_SEC


def evaluate_merge_gate(state, entry, pr, files, now, no_auto_merge):
    """六门槛全查(spec §5)。entry=None 时仅 docs 快道可过(用户 docs PR)。
    any 拒绝即 False,reasons 供审计评论/夜间报告。"""
    reasons = []
    fast = docs_only(files)
    # 门槛 1: 归属(docs 例外见下)
    if entry is None:
        if not fast:
            return GateResult(False, False, ["非引擎 PR 且非 docs-only——不自动 merge"])
    else:
        if not ENGINE_BRANCH_RE.match(entry.fix_branch or ""):
            return GateResult(False, False,
                              [f"非引擎分支: {entry.fix_branch}"])
    # 门槛 4: 状态
    if pr.state != "open" or pr.draft:
        return GateResult(False, fast, ["非 open/DRAFT"])
    if fast and wip_gated(pr.title):
        reasons.append(f"WIP: 草稿闸({pr.title!r})")
        return GateResult(False, True, reasons)
    if pr.mergeable is False:
        return GateResult(False, fast, ["与 main 冲突(mergeable=false)"])
    # 门槛 2: 检视
    if fast:
        if not docs_ready(pr, now):
            return GateResult(False, True, ["docs 快道未就绪(第5bot/静默窗/意见)"])
    else:
        st_class = tl.pr_loop.classify_state(pr, tl.pr_loop.DEFAULT_QUIET, now)
        if st_class != "READY_TO_MERGE":
            return GateResult(False, False, [f"classify={st_class}≠READY_TO_MERGE"])
    # 门槛 3: CI check(docs 只查快 job)
    ok, why = checks_ok(pr, REQUIRED_CHECKS_DOCS if fast
                        else REQUIRED_CHECKS_FULL, tl.repo_slug())
    if not ok:
        return GateResult(False, fast, why)
    # 门槛 5: 熔断
    if tl._paused():
        return GateResult(False, fast, ["PAUSED 生效"])
    if no_auto_merge:
        return GateResult(False, fast, ["NO_AUTO_MERGE 熔断(灰度: 只报告)"])
    return GateResult(True, fast, why)      # why=继承红放行的证据,审计用
```

（`import collections` 顶部补齐；`tl.repo_slug` 已存在于 task_loop。）

- [ ] **Step 4: 跑测试确认通过**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -x -q`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): merge 门 evaluate_merge_gate——六门槛+docs 快道(第5bot唯一+300s)+继承红基线对照+NO_AUTO_MERGE/PAUSED 熔断"
```

---

### Task 8: merge 执行 + 记账 + 审计评论

**Files:**
- Modify: `scripts/task_loop_pipeline.py`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `tp.execute_merge(state, entry, pr, gate: GateResult, dry_run=False) -> Optional[str]`
    ——PR 评论贴门槛证据 → `gh pr merge --squash --delete-branch` → 回读新 main
    sha 记 `entry.merged_at/merged_sha`（entry 为 None 的用户 docs PR 只 merge+评论）
    → 返回 merged_sha
  - `tp.no_auto_merge_active() -> bool`（STATE_DIR/NO_AUTO_MERGE 存在）
- Consumes: Task 7 GateResult、`tl.comment_on_issue`、`tl.gh_json_idem`。

- [ ] **Step 1: 写失败测试**

```python
def test_execute_merge_records_sha_and_comments(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    calls = []
    monkeypatch.setattr(tl, "gh_json_idem",
                        lambda args, input_text=None: calls.append(args) or
                        ("abc1234def5678" if "--jq" in args and
                         "mergeCommit" in " ".join(args) else "{}"))
    monkeypatch.setattr(tl, "comment_on_issue",
                        lambda n, b: calls.append(["comment", n]))
    e = NS(pr_number=42, issue_number=7, fix_branch="fix/issue-7",
           status="PR_OPEN", merged_at=None, merged_sha=None,
           updated_at="t")
    st = {"entries": [e], "auth_failure_streak": 0}
    gr = tp.GateResult(True, False, ["继承红放行: [X] ⊆ 基线"])
    sha = tp.execute_merge(st, e, _pr(), gr)
    assert sha == "abc1234def5678" and e.merged_sha == sha and e.merged_at
    assert e.status == "DONE"
    assert any("squash" in " ".join(map(str, c)) for c in calls)


def test_execute_merge_user_docs_pr_no_entry(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    monkeypatch.setattr(tl, "gh_json_idem", lambda a, input_text=None: "sha1")
    monkeypatch.setattr(tl, "comment_on_issue", lambda n, b: None)
    sha = tp.execute_merge(_mkstate(), None, _pr(),
                           tp.GateResult(True, True, []))
    assert sha == "sha1"
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -k execute_merge -x -q`
Expected: FAIL

- [ ] **Step 3: 实现**

```python
def no_auto_merge_active() -> bool:
    return os.path.exists(
        os.path.join(tl._resolve_state_dir(), NO_AUTO_MERGE_NAME))


def execute_merge(state, entry, pr, gate, dry_run=False):
    """过门后执行 squash merge: PR 评论贴门槛证据 → merge → 记 merged_sha
    (revert 归因主数据,人工 merge 永不写)→ 引擎条目 DONE/issue 收尾评论。
    merge 执行权在编排器代码,绝不在 LLM(spec §1 决策 7)。"""
    slug = tl.repo_slug()
    evidence = "\n".join(f"- {r}" for r in gate.reasons) or "- 全门槛通过"
    body = (f"🤖 task-loop pipeline 自动 merge(spec 2026-11-12):\n"
            f"- 门槛证据:\n{evidence}\n"
            f"- 方式: squash + delete-branch;"
            f"{'docs-only 快速通道' if gate.fast else '全门槛'}")
    if dry_run:
        print(f"[dry-run] 将 merge PR #{pr.pr_number}: {body[:120]}…")
        return None
    tl.gh_json_idem(["pr", "comment", str(pr.pr_number), "--repo", slug,
                    "--body", body])
    tl.gh_json_idem(["pr", "merge", str(pr.pr_number), "--repo", slug,
                     "--squash", "--delete-branch"])
    merged_sha = tl.gh_json_idem(
        ["pr", "view", str(pr.pr_number), "--repo", slug, "--json",
         "mergeCommit", "--jq", ".mergeCommit.oid"]).strip()
    if entry is not None:
        entry.merged_at = _now_local_iso()
        entry.merged_sha = merged_sha
        entry.status = "DONE"
        entry.updated_at = _now_local_iso()
        if entry.issue_number is not None:
            tl.comment_on_issue(
                entry.issue_number,
                f"修复 PR #{pr.pr_number} 已由 pipeline 自动 squash merge"
                f"(main {merged_sha[:12]})。本 issue 闭环。")
            try:
                tl.gh_json_idem(["issue", "close", str(entry.issue_number),
                                 "--repo", slug])
            except GhError as exc:
                print(f"[warn] issue 关闭失败(state 为权威): {exc}")
    print(f"[merge] PR #{pr.pr_number} 已自动合入 → {merged_sha[:12]}")
    return merged_sha
```

- [ ] **Step 4: 跑测试确认通过 + 全量**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): merge 执行——squash+delete-branch,门槛证据审计评论,merged_at/sha 记账(revert 归因主数据)"
```

---

### Task 9: 精确归因自动 revert

**Files:**
- Modify: `scripts/task_loop_pipeline.py`
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `tp.revert_attribution(state, red_runs: list[dict], dry_run=False) -> list[int]`
    ——红 run `head_sha ∈` 条目 `merged_sha` 集 → 建 revert issue（`tl.create_task_issue`，
    source=`manual`、kind=`bug`，正文标记 `revert-for: PR #N @ <sha>` + `git revert -m 1`
    指令 + 结束语契约），返回 issue 号清单；归因不到 → 空清单（前向修复语义不变，
    intake 照常建普通红 issue）
  - 二阶链闸：红 run 的 head_sha 对应的 PR 若其 issue 正文本身含 `revert-for:`（即
    "revert 的 revert"）→ 不自动 revert，直接 `_escalate`
- Consumes: `tl.scan_red_runs`、`tl.create_task_issue`、`tl.StateStore.entries`。

- [ ] **Step 1: 写失败测试**

```python
def test_revert_attribution_creates_issue_for_merged_sha(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    st = _mkstate()
    st["entries"] = [dict(workflow="w", run_id=1, status="DONE",
                         issue_number=7, fix_branch="fix/issue-7", pr_number=42,
                         attempts=1, rounds=0, agent_pid=None,
                         agent_started_at=None, agent_log=None,
                         last_dispatched_head=None, extra_runs=[],
                         created_at="t", updated_at="t", source="audit",
                         kind="bug", audit_hash=None, lane="",
                         worktree=None, merged_at="t",
                         merged_sha="deadbeef1234")]
    made = []
    monkeypatch.setattr(tl, "create_task_issue",
                        lambda title, body, source, kind, labels=None:
                        made.append(title) or 99)
    reds = [{"workflow_name": "Sanitizer", "run_id": 555,
             "head_sha": "deadbeef1234", "html_url": "u", "title": "x"}]
    got = tp.revert_attribution(st, reds)
    assert got == [99] and "revert" in made[0].lower()
    assert "revert-for: PR #42 @ deadbeef1234" in st["_last_revert_body"]


def test_revert_attribution_ignores_unrelated_red():
    st = _mkstate()
    st["entries"] = []
    assert tp.revert_attribution(st, [{"head_sha": "other",
                                      "workflow_name": "S", "run_id": 1,
                                      "html_url": "u", "title": "x"}]) == []
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -k revert -x -q`
Expected: FAIL

- [ ] **Step 3: 实现**

```python
REVERT_FOR_FMT = "revert-for: PR #{pr} @ {sha}"


def revert_attribution(state, red_runs, dry_run=False):
    """精确归因才回滚(spec §6): 红 run head_sha 命中本流水线自动 merge 的
    merged_sha 集 → 建 revert issue(走 issue 队列→泳道1,分支随 issue 约定
    fix/issue-<N>;spec 的 fix/revert-pr<N> 降级为标题约定,不为命名改 dispatch)。
    人工 merge 的 sha 永不在集(merged_sha 只由 execute_merge 写)。二阶链:
    命中 PR 自身是 revert 产物 → 直接升级,不自动 revert 的 revert。"""
    merged = {}
    for e in tl.StateStore.entries(state):
        if e.merged_sha and e.pr_number:
            merged[e.merged_sha] = e
    issues = []
    for run in red_runs:
        hit = merged.get(run.get("head_sha"))
        if hit is None:
            continue
        body = (f"自动 merge 的 PR #{hit.pr_number}({hit.merged_sha[:12]})后,"
                f"main 红 run [{run.get('workflow_name')}]({run.get('html_url')}"
                f")——精确归因命中,请 revert:\n\n"
                f"```\ngit revert -m 1 {hit.merged_sha}\n```\n\n"
                f"{REVERT_FOR_FMT.format(pr=hit.pr_number, sha=hit.merged_sha)}\n"
                f"<!-- task-loop:revert:{hit.merged_sha} -->\n\n"
                f"完成后以结束语 `task-loop: done <fix_branch>` 收尾。")
        if dry_run:
            print(f"[dry-run] 将建 revert issue: {body[:100]}…")
            continue
        num = tl.create_task_issue(
            title=f"[revert] PR #{hit.pr_number} 自动 merge 后 main 红"
                  f"({run.get('workflow_name')})",
            body=body, source="manual", kind="bug")
        issues.append(num)
        state["_last_revert_body"] = body
    return issues
```

（`create_task_issue` 签名以 task_loop.py 现实为准——实施时先 `grep -n "def
create_task_issue" scripts/task_loop.py` 对齐参数顺序，测试桩同步。二阶链闸在
`evaluate_merge_gate` 侧的天然保障：revert PR 也是引擎 PR，若其 merge 又红且归因命中，
本函数会为它建第二个 revert issue——加一行防护：`if "revert" in (hit.本 issue 正文)`
不可得，故按**条目链深**判定：`hit` 条目的 issue 标题以 `[revert]` 开头 → 打印
escalation 提示并跳过建 issue，由夜间报告人工可见。实现里补：）

```python
        if str(getattr(hit, "issue_number", "")) and \
                tl.issue_title_starts(hit.issue_number, "[revert]"):
            print(f"[revert] PR #{hit.pr_number} 本身是 revert 产物——二阶链"
                  f"不自动回滚,转人工(夜间报告可见)")
            continue
```

（`issue_title_starts` 若 task_loop 无现成助手，加一个 8 行的
`gh_json_idem(["issue","view",...,"--json","title","--jq",".title"]).startswith(prefix)`
小函数并单测。）

- [ ] **Step 4: 跑测试确认通过 + 全量**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
Expected: PASS

- [ ] **Step 5: Commit**

```bash
git add scripts/task_loop_pipeline.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): 精确归因自动 revert——merged_sha 集命中建 revert issue(人工 merge 永不命中);二阶链转人工"
```

---

### Task 10: pipeline 编排器主循环 + 软停 + 报告 + CLI 接线

**Files:**
- Modify: `scripts/task_loop_pipeline.py`（主循环）、`scripts/task_loop.py`（argparse
  `pipeline` 子命令 + `cmd_status` 摘要）
- Test: `tools/excite/tests/test_task_loop_pipeline.py`

**Interfaces:**
- Produces:
  - `tp.run_pipeline(dry_run=False, window_start=23, window_end=9) -> int`
  - `tp.cycle_once(state, dry_run, events, now) -> None`（单轮：intake CI（含
    `revert_attribution`）→ `tl.reap_or_advance` → `tl.reconcile_issues` →
    `tl.heal_clone` → issue 泳道派发扫（QUEUED FIFO ×free 槽）→ 检视泳道扫
    （`needs_review_list` ×free 槽）→ `tl.lane_b_advance`（pr_repair 泳道）→
    audit 后台线（空则 spawn 一发）→ merge 门扫（引擎条目 + open docs PR））
  - `tp.soft_stop(state, store, deadline_iso, dry_run) -> None`（击杀活槽 PID、
    条目干净回队不计 attempts、清 worktree、写报告）
  - `tp.write_report(state, events, dry_run) -> str`（`report-YYYYMMDD.md` +
    `state["last_report"]`）
  - `tl` CLI：`pipeline [--dry-run] [--window-start H] [--window-end H]`
  - `cmd_status` 顶部追加最近一夜摘要（读 `state["last_report"]` 打一行）
- Consumes: 全部前序任务。

- [ ] **Step 1: 写失败测试**

```python
def test_cycle_once_orchestration_order(monkeypatch, tmp_path):
    """全接缝打桩,断言 cycle_once 调齐 8 步且互不炸。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    order = []
    monkeypatch.setattr(tl, "scan_red_runs", lambda: [])
    monkeypatch.setattr(tl, "triage_new_red_runs",
                        lambda s, r, dry_run=False: order.append("intake"))
    monkeypatch.setattr(tl, "reap_or_advance",
                        lambda s, dry_run=False: order.append("reap"))
    monkeypatch.setattr(tl, "reconcile_issues",
                        lambda s, dry_run=False: order.append("reconcile") or set())
    monkeypatch.setattr(tl, "heal_clone",
                        lambda s, dry_run=False: order.append("heal"))
    monkeypatch.setattr(tl, "lane_b_advance",
                        lambda s, dry_run=False: order.append("laneb"))
    monkeypatch.setattr(tp, "needs_review_list", lambda: [])
    monkeypatch.setattr(tp, "open_docs_prs", lambda: [])
    monkeypatch.setattr(tp, "audit_line_running", lambda st: True)
    st = _mkstate()
    tp.cycle_once(st, False, [], datetime.now())
    assert order == ["intake", "reap", "reconcile", "heal", "laneb"]
    assert tp.SLOT_KINDS["issue_fix"] == 2


def test_write_report(tmp_path):
    monkeypatch = None
    import os
    os.environ["TASKLOOP_STATE_DIR"] = str(tmp_path / "st")
    st = _mkstate()
    p = tp.write_report(st, [{"kind": "merge", "pr": 42}], False)
    assert os.path.exists(p) and st["last_report"] == os.path.basename(p)


def test_pipeline_lock_second_instance(tmp_path, monkeypatch):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    os.makedirs(tmp_path / "st")
    fd = os.open(os.path.join(tmp_path / "st", tp.PIPELINE_LOCK_NAME),
                 os.O_CREAT | os.O_RDWR)
    import fcntl as _f
    _f.flock(fd, _f.LOCK_EX)
    assert tp.run_pipeline(dry_run=True) == 0      # 拿不到锁 → 退出码 0
    _f.flock(fd, _f.LOCK_UN)
```

- [ ] **Step 2: 跑测试确认失败**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/test_task_loop_pipeline.py -k "cycle_once or write_report or lock_second" -x -q`
Expected: FAIL

- [ ] **Step 3: 实现**（task_loop_pipeline.py 主体）

```python
def needs_review_list():
    """全部 open PR 中当前 head 无第 5 bot 信号者(spec §4 泳道2 取材)。"""
    import pr_local_review
    out = tl.gh_json_idem(["pr", "list", "--state", "open", "--json",
                           "number,headRefOid", "--jq",
                           ".[] | [.number, .headRefOid] | @tsv"])
    prs = []
    for line in out.splitlines():
        if not line.strip():
            continue
        num_s, _, head = line.partition("\t")
        num, head = int(num_s.strip()), head.strip()
        if not pr_local_review.already_reviewed(num, head):
            prs.append((num, head))
    return prs


def open_docs_prs():
    """open PR 里 docs-only 且非 WIP: 的(用户 docs 快道;引擎条目由 state 侧扫)。"""
    out = tl.gh_json_idem(["pr", "list", "--state", "open", "--json",
                           "number,title,headRefOid", "--jq",
                           ".[] | [.number, .title, .headRefOid] | @tsv"])
    prs = []
    for line in out.splitlines():
        if not line.strip():
            continue
        parts = line.split("\t")
        if len(parts) < 3:
            continue
        prs.append({"pr_number": int(parts[0]), "title": parts[1],
                    "head": parts[2].strip()})
    return prs


def audit_line_running(state):
    """audit 后台子进程是否在跑(槽 kind=audit,满 1 即在跑)。"""
    return free_slot_count(state, "audit") == 0


def spawn_audit(state, dry_run):
    if audit_line_running(state) or not mem_gate_ok():
        return
    log = os.path.join(tl._resolve_state_dir(), "audit.log")
    inner = (f"timeout 7500 /usr/bin/python3 scripts/task_loop.py audit")
    cmd = f"nohup setsid bash -c {shlex.quote(inner)} >> {shlex.quote(log)} 2>&1 & echo $!"
    pid = tl.run_background(cmd, cwd=tl._resolve_clone_dir(), log=log)
    slot_acquire(state, "audit", "audit-nightly", pid, pid, log, None,
                 _now_local_iso(), boot_id())


def merge_sweep(state, events, dry_run):
    """merge 门扫: 引擎条目(PR_OPEN/BOT_FIX 且 agent 已退场) + 用户 docs PR。
    NO_AUTO_MERGE 生效 → 只记录 [would-merge] 事件,不执行。"""
    no_auto_merge = no_auto_merge_active()
    now = datetime.now(timezone.utc)
    slug = tl.repo_slug()
    for e in tl.StateStore.entries(state):
        if e.status not in ("PR_OPEN",) or e.pr_number is None:
            continue
        if any(s["key"] == f"pr-{e.pr_number}" and tl.agent_alive(s["pid"])
               for s in state.get("slots", {}).get("pr_repair", [])):
            continue                      # 修复 agent 在跑,等它退场
        try:
            pr = tl.pr_loop.fetch_pr(slug, e.pr_number)
        except GhError as exc:
            print(f"[merge-sweep] fetch PR #{e.pr_number} 失败: {exc}")
            continue
        gr = evaluate_merge_gate(state, e, pr, tl.pr_files(e.pr_number),
                                 now, no_auto_merge)
        if gr.ok:
            sha = execute_merge(state, e, pr, gr, dry_run)
            events.append({"kind": "merge", "pr": e.pr_number, "sha": sha})
        else:
            events.append({"kind": "gate-reject", "pr": e.pr_number,
                           "reasons": gr.reasons})
    for d in ([] if no_auto_merge else open_docs_prs()):
        if wip_gated(d["title"]):
            continue
        try:
            pr = tl.pr_loop.fetch_pr(slug, d["pr_number"])
            files = tl.pr_files(d["pr_number"])
            if not docs_only(files):
                continue
            gr = evaluate_merge_gate(state, None, pr, files, now, no_auto_merge)
            if gr.ok:
                sha = execute_merge(state, None, pr, gr, dry_run)
                events.append({"kind": "merge-docs", "pr": d["pr_number"],
                               "sha": sha})
        except GhError as exc:
            print(f"[merge-sweep] docs PR #{d['pr_number']} 评估失败: {exc}")


def cycle_once(state, dry_run, events, now):
    runs = tl.scan_red_runs()
    issues = revert_attribution(state, runs, dry_run)
    if issues:
        events.append({"kind": "revert-issue", "issues": issues})
    tl.triage_new_red_runs(state, runs, dry_run=dry_run)
    tp_janitor = slot_janitor(state, _now_local_iso())
    if tp_janitor:
        events.append({"kind": "slot-released", "released": tp_janitor})
    tl.reap_or_advance(state, dry_run=dry_run)
    open_labels = tl.reconcile_issues(state, dry_run=dry_run)
    tl.heal_clone(state, dry_run=dry_run)
    # 泳道1: QUEUED FIFO × 空槽
    es = tl.StateStore.entries(state)
    for e in sorted((x for x in es if x.status == "QUEUED"),
                    key=lambda x: (x.issue_number is None, x.issue_number)):
        if free_slot_count(state, "issue_fix") <= 0:
            break
        if not tl._dispatch_gate_ok(e, open_labels):
            continue
        pid = tl.dispatch_agent(e, state, dry_run=dry_run)
        if pid is not None and not dry_run:
            StateStore().save(state)
    # 泳道2: 检视 × 空槽
    for num, head in needs_review_list():
        if free_slot_count(state, "pr_review") <= 0:
            break
        dispatch_review(state, num, head)
    # 泳道3: 车道 B(其派发已槽位化为 pr_repair)
    tl.lane_b_advance(state, dry_run=dry_run)
    # audit 后台线
    spawn_audit(state, dry_run)
    # merge 门
    merge_sweep(state, events, dry_run)


def write_report(state, events, dry_run):
    day = datetime.now().strftime("%Y%m%d")
    path = os.path.join(tl._resolve_state_dir(), f"report-{day}.md")
    body = [f"# pipeline 夜间报告 {day}", "",
            f"- 事件数: {len(events)}"]
    for ev in events:
        body.append(f"- {ev}")
    if not dry_run:
        os.makedirs(tl._resolve_state_dir(), exist_ok=True)
        with open(path, "w", encoding="utf-8") as f:
            f.write("\n".join(body) + "\n")
        state["last_report"] = os.path.basename(path)
    return path


def soft_stop(state, grace_sec=SOFT_STOP_GRACE_SEC):
    """09:00 软停(spec §4): 在跑 agent 先给 ≤grace_sec 自然退出,超时击杀;
    条目干净回队不计 attempts;清 worktree。报告由调用方写。"""
    deadline = time.time() + grace_sec
    while time.time() < deadline:
        live = [s for lst in (state.get("slots") or {}).values()
                for s in lst if tl.agent_alive(s["pid"])]
        if not live:
            break
        print(f"[soft-stop] 等待 {len(live)} 个 agent 自然退出"
              f"(剩余 {int(deadline - time.time())}s)")
        time.sleep(60)
    killed = []
    for kind, lst in list((state.get("slots") or {}).items()):
        for s in lst:
            if tl.agent_alive(s["pid"]):
                try:
                    os.killpg(s["pid"], 15)
                except OSError:
                    pass
                killed.append(s["key"])
                for e in tl.StateStore.entries(state):
                    if s["key"] in (f"issue-{e.issue_number or e.run_id}",
                                   f"pr-{e.pr_number}"):
                        e.status = "PR_OPEN" if kind == "pr_repair" else "QUEUED"
                        e.agent_pid = None
                        e.agent_started_at = None
                        e.updated_at = _now_local_iso()   # 不动 attempts/rounds
            if s.get("worktree"):
                tl.worktree_cleanup(s["worktree"])
            slot_release(state, kind, s["key"])
    print(f"[soft-stop] 击杀 {len(killed)} 个超时 agent(不计 attempts): {killed}")


def run_pipeline(dry_run=False, window_start=23, window_end=9) -> int:
    state_dir = tl._resolve_state_dir()
    if dry_run:
        events = []
        st = {"entries": [], "auth_failure_streak": 0}
        cycle_once(st, True, events, datetime.now())
        write_report(st, events, True)
        print("[dry-run] pipeline 全流程预演完成,零副作用")
        return 0
    os.makedirs(state_dir, exist_ok=True)
    lock_fd = os.open(os.path.join(state_dir, PIPELINE_LOCK_NAME),
                      os.O_CREAT | os.O_RDWR, 0o644)
    try:
        try:
            fcntl.flock(lock_fd, fcntl.LOCK_EX | fcntl.LOCK_NB)
        except OSError:
            print("[lock] pipeline 已在跑(锁占用),本实例退出")
            return 0
        tl._migrate_legacy_state_dir()
        try:
            tl.ensure_agent_labels()
        except GhError as exc:
            print(f"[warn] 标签确保失败: {exc}")
        store = StateStore()
        state = store.load()
        reset_review_tries(state)
        startup_reconcile(state)
        store.save(state)
        events = []
        while in_window(datetime.now(), window_start, window_end):
            try:
                cycle_once(state, False, events, datetime.now())
            except GhError as exc:
                print(f"[cycle] 本轮失败(下轮重试): {exc}", file=sys.stderr)
            store.save(state)
            time.sleep(PIPELINE_CYCLE_SEC)
        soft_stop(state)
        store.save(state)
        report = write_report(state, events, False)
        print(f"[pipeline] 窗口结束,报告: {report}")
        return 0
    finally:
        os.close(lock_fd)
```

`scripts/task_loop.py` argparse（`audit` 块后）+ 入口：

```python
    pp = sub.add_parser(
        "pipeline", help="夜间全自动流水线: 三泳道并行+merge 门+revert(23:00-09:00 窗口)")
    pp.add_argument("--dry-run", action="store_true",
                    help="单轮零副作用预演(零 gh 写/零 spawn/state 不落盘)")
    pp.add_argument("--window-start", type=int, default=23)
    pp.add_argument("--window-end", type=int, default=9)
    pp.set_defaults(func=cmd_pipeline)


def cmd_pipeline(args: argparse.Namespace) -> int:
    """pipeline 子命令入口——懒 import 打破 task_loop↔pipeline 循环依赖。"""
    import task_loop_pipeline
    return task_loop_pipeline.run_pipeline(
        dry_run=bool(getattr(args, "dry_run", False)),
        window_start=getattr(args, "window_start", 23),
        window_end=getattr(args, "window_end", 9))
```

`cmd_status`（2238）首行后追加：

```python
    last = state.get("last_report")
    if last:
        print(f"最近夜间报告: {last}")
```

（`state` 变量名以 cmd_status 现实实现为准对齐。）

- [ ] **Step 4: 跑测试确认通过 + 全量**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -x -q`
Expected: PASS

- [ ] **Step 5: dry-run 真实预演（本机,零副作用）**

Run: `cd /home/sdc/wangxu/operator/sdcshield && /usr/bin/python3 scripts/task_loop.py pipeline --dry-run`
Expected: 打印单轮预演（intake/reap/laneb 顺序、无报错、退出 0）——**注意**：dry-run 的
`scan_red_runs`/`gh pr list` 是真实只读 gh 调用，网络可用则应正常；GhError 时如实记录。

- [ ] **Step 6: Commit**

```bash
git add scripts/task_loop_pipeline.py scripts/task_loop.py tools/excite/tests/test_task_loop_pipeline.py
git commit -s -m "feat(pipeline): 编排器主循环——窗口循环/三泳道派发/audit 后台线/merge 扫/软停(击杀不计 attempts)/夜间报告/pipeline CLI"
```

---

### Task 11: @reboot 自愈脚本 + crontab 终态 + 部署 runbook

**Files:**
- Create: `scripts/pipeline_selfheal.sh`
- Modify: `docs/build-deploy-pr-ci/task-loop.md`（§3.1 终态 crontab 替换——见 Task 12
  之外的部署节；本任务先在 spec 附录引用处放 runbook）
- Test: `bash -n` 语法检查 + 人工核验（shell 无单测基建，诚实声明）

**Interfaces:**
- Produces: `scripts/pipeline_selfheal.sh`（@reboot 入口：窗口内才 exec pipeline）
- 部署产物（**不在本仓库**，操作记录进 task-loop.md §3.1）：新 crontab。

- [ ] **Step 1: 写脚本**

```bash
#!/bin/bash
# pipeline_selfheal.sh — @reboot 自愈: 机器重启落在 23:00–09:00 窗口内则重新
# 拉起 pipeline(锁语义在 task_loop.py pipeline 内部: 已在跑则秒退,双保险)。
# 本机频繁重启是常态(2026-11-12 当天 13h 内 5 次 boot),spec §2 崩溃语义配套件。
set -u
HOUR=$(date +%H); HOUR=${HOUR#0}
if [ "$HOUR" -ge 23 ] || [ "$HOUR" -lt 9 ]; then
  mkdir -p "$HOME/.local/state/sdcshield-task-loop"
  cd /home/sdc/wangxu/operator/sdcshield-autoci || exit 0
  exec timeout 37800 /usr/bin/python3 scripts/task_loop.py pipeline \
    >> "$HOME/.local/state/sdcshield-task-loop/pipeline.log" 2>&1
fi
exit 0
```

- [ ] **Step 2: 语法检查 + 可执行位**

Run: `bash -n scripts/pipeline_selfheal.sh && chmod +x scripts/pipeline_selfheal.sh`
Expected: 无输出（语法 OK）

- [ ] **Step 3: Commit**

```bash
git add scripts/pipeline_selfheal.sh
git commit -s -m "feat(pipeline): @reboot 窗口自愈脚本——重启落在 23:00-09:00 则重新拉起 pipeline(锁双保险)"
```

- [ ] **Step 4: crontab 部署（合入后执行,本任务只写 runbook 进 task-loop.md 的 Task 12 统一改）**

runbook（合入 main 后人工执行,记入 task-loop.md §3.1）：
1. `cd /home/sdc/wangxu/operator/sdcshield-autoci && git fetch origin && git reset --hard origin/main`
2. crontab 编辑：删 `33 */6` scan 行、`17 */6` audit 行、`*/30` pr_local_review 行；
   增两行：
   `0 23 * * * mkdir -p ~/.local/state/sdcshield-task-loop && cd /home/sdc/wangxu/operator/sdcshield-autoci && timeout 37800 /usr/bin/python3 scripts/task_loop.py pipeline >> ~/.local/state/sdcshield-task-loop/pipeline.log 2>&1`
   `@reboot /home/sdc/wangxu/operator/sdcshield/scripts/pipeline_selfheal.sh`
3. `touch ~/.local/state/sdcshield-task-loop/NO_AUTO_MERGE`（灰度首夜）
4. 次晨 `tail -50 ~/.local/state/sdcshield-task-loop/pipeline.log` + 最新 `report-*.md`
   人工复核 merge 门判定 → 复核数晚后 `rm NO_AUTO_MERGE`

（此 runbook 本任务先写进 plan,文档落地在 Task 12 与 task-loop.md §3.1 一起改。）

---

### Task 12: 文档反转（6 文档 + CLAUDE.md）

**Files:**
- Modify: `CLAUDE.md`、`docs/build-deploy-pr-ci/task-loop.md`、`pr-loop.md`、
  `pr-review-bots.md`、`ci-triage.md`、`agent-full-automation.md`、
  `agent-full-automation_CN.md`

**Interfaces:**
- Consumes: spec §9 同步清单（逐文件、逐节）；本任务是纯文档编辑,无代码。

- [ ] **Step 1: CLAUDE.md**——PR review-fix loop 段
  「never auto-merge」→「引擎 PR 全门槛自动 merge（merge 门在编排器代码,spec
  2026-11-12）；用户 WIP PR 永不自动 merge（docs-only 例外：去掉 `WIP:` 前缀 =
  显式同意当夜合入）」；task-loop 描述段 cron `33 */6`→`0 23 pipeline`。

- [ ] **Step 2: task-loop.md**——§1 触发表加 nightly pipeline 行（cron 0 23 →
  pipeline,scan/audit/第5bot 三行折入）；§3.1 终态 crontab 整段替换为 Task 11
  runbook 的两行 + monitor 行；§4 状态机表 DONE 行注明「自动 merge 后 merged_at/
  merged_sha 记账,revert 归因主数据」；§7 红线段改写：「merge 权在编排器门槛代码：
  引擎 PR 过六门槛自动 squash merge;用户非 docs PR 永不自动 merge;自动 resolve
  review threads 仍然禁止」。

- [ ] **Step 3: pr-loop.md**——§2 阶段 5「4 轮上限」→「动态 2–3 轮（docs/简单 diff
  ≤100 行∧≤5 文件 → 2,其余 3;spec 2026-11-12）」；§2 阶段 6 与 §7「人工 merge」→
  「引擎 PR 由 pipeline merge 门自动 squash merge;用户 PR 人工」。

- [ ] **Step 4: pr-review-bots.md**——§8.2 第 5 bot「时间线 cron `*/30 --all-open`」段
  改为「折进 nightly pipeline（pipeline 检视泳道,每 head 每晚 ≤2 次）;`--all-open`
  CLI 保留供会话手动」。

- [ ] **Step 5: ci-triage.md**——开头注记追加一行：「2026-11-12 起调度被 nightly
  pipeline（task_loop.py pipeline,23:00–09:00 三泳道）取代;本手册保留历史语义」。

- [ ] **Step 6: agent-full-automation.md / _CN.md**——「NO PR is auto-merged or
  auto-resolved by bots; merge authority remains with a human」段替换为新政策
  （引用 spec);_CN 译注同步更新。

- [ ] **Step 7: 全文 grep 校验 + Commit**

Run: `grep -rn "never auto-merge\|绝不自动 merge\|merge 永远在人" CLAUDE.md docs/build-deploy-pr-ci/ | grep -v "永不自动 merge（docs-only\|非 docs PR"`
Expected: 仅剩「resolve threads 仍禁止」类合法残留;无漏改的红线表述。

```bash
git add CLAUDE.md docs/build-deploy-pr-ci/
git commit -s -m "docs(pr-ci): 红线反转——nightly pipeline 自动 merge 政策落地 7 文档(引擎 PR 全门槛自动;用户 PR 永不,docs-only WIP: 闸例外)"
```

---

### Task 13: 集成验证与灰度上线

**Files:** 无新文件;验证任务。

- [ ] **Step 1: 全量测试**

Run: `/usr/bin/python3 -m pytest tools/excite/tests/ -q`
Expected: 全绿（含 test_task_loop.py 70+ 旧用例）

- [ ] **Step 2: dry-run 三态预演**

Run: `cd /home/sdc/wangxu/operator/sdcshield && /usr/bin/python3 scripts/task_loop.py pipeline --dry-run && /usr/bin/python3 scripts/task_loop.py dry-run && /usr/bin/python3 scripts/task_loop.py status`
Expected: 三命令全零副作用、输出可读、退出 0。

- [ ] **Step 3: 真实单轮夜间观察（灰度）**

按 Task 11 runbook 部署 crontab + `touch NO_AUTO_MERGE`;当晚 23:00 后
`tail -f ~/.local/state/sdcshield-task-loop/pipeline.log`;次晨核对 `report-*.md`:
- [would-merge] 判定与门槛建议逐条人工复核（对照当前 open PR: #642 应因
  codex/greptile 缺信号被拒——reasons 应含 `classify=WAITING_BOTS`）
- 泳道2 应对 #661 之类无检视 head 的 PR 重试且 ≤2 次
- audit 线产出 findings issue 正常入队

- [ ] **Step 4: 灰度复核通过后 `rm NO_AUTO_MERGE`,真自动 merge 上线;观察 3 夜**

- [ ] **Step 5: 推送 + PR**

```bash
git push origin feat/nightly-pipeline
```
（本引擎 PR 自身走现行人工 merge——它是"最后一个需要人工 merge 的 PR"。合入后按
Task 11 runbook 更新 autoci 克隆与 crontab。）

---

## Self-Review 记录（写计划时已自查）

1. **Spec 覆盖**：决策表 10 行→Task 1(常量)/2(字段)/3(槽位+boot 豁免)/4(并行+worktree+
   内存闸)/5(轮次)/6(检视泳道+超时+每晚上限)/7-8(merge 门+docs 快道+继承红+熔断+执行)/
   9(revert)/10(主循环+软停+报告+scan 兼容)/11(@reboot+crontab)/12(文档)/13(灰度);
   §5 冲突竞态→merge_sweep 逐个串行+mergeable 复查(门槛 4);§7 存量迁移→QUEUED 队列
   由泳道1 自然消化,无需专门任务。已覆盖。
2. **占位符**：无 TBD/TODO;Task 7 Step 3 与 Task 9 的"实施首步先校准 check 名/签名"
   是显式核验动作而非占位。
3. **类型一致性**:`slot_acquire(state,kind,key,pid,pgid,log,worktree,started_iso,boot)`
   贯穿 Task 3/4/6;`GateResult(ok,fast,reasons)` 贯穿 Task 7/8;`max_rounds_for(entry,pr,files)`
   贯穿 Task 5;`_now_local_iso()` 定义于 Task 3、用于 Task 4/6/8/10。
4. **已知偏差（对 spec,已注明）**:revert 分支命名走 issue 约定 `fix/issue-<N>`
   (spec 写 `fix/revert-pr<N>`)——不为命名改 dispatch 分支逻辑;merge 门的
   `ENGINE_BRANCH_RE` 含 `revert-pr` 仅为防御性冗余,无行为影响。

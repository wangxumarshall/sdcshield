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

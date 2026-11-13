"""task_loop_pipeline 测试: 槽位常量 / 窗口判定 / 内存闸 / boot_id /
槽位账本(租约/janitor/启动对账)/ 槽位化派发 + worktree 隔离(Task 4)。"""
import sys, os, shutil, subprocess
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "..", "..", "scripts"))
import task_loop_pipeline as tp
import task_loop as tl
from types import SimpleNamespace as NS
from datetime import datetime, timedelta, timezone


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


# ============ 槽位账本(Task 3) ============


def _mkstate():
    return {"entries": [], "auth_failure_streak": 0}


def test_slot_acquire_respects_kind_max_and_dup_key(monkeypatch):
    # 探活 monkeypatch 钉死: 低号 PID 在真机是 root 内核线程(ksoftirqd/
    # kworker),os.kill → EPERM → agent_alive 判活,宿主相关必假活。
    monkeypatch.setattr(tl, "agent_alive", lambda pid: True)
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
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    tp.slot_acquire(st, "pr_review", "pr-9", 201, 200, "/l", None, "t", "b")
    assert tp.free_slot_count(st, "pr_review") == 2   # 死 PID → janitor 清 → 空槽
    tp.slot_release(st, "pr_review", "pr-9")
    assert tp.slot_get(st, "pr_review", "pr-9") is None


def test_slot_janitor_marks_environmental(monkeypatch):
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    tp.slot_acquire(st, "issue_fix", "issue-1", 999999, 0, "/l", "/w",
                    "t", "old-boot")
    monkeypatch.setattr(tp, "boot_id", lambda: "new-boot")
    gone = tp.slot_janitor(st, "now")
    assert len(gone) == 1 and gone[0]["environmental"] is True


def test_startup_reconcile_requeues_without_attempt(monkeypatch):
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


# ============ T3 遗留三臂补测(控制器裁定#3,同 commit) ============


def _entry(**kw):
    d = dict(workflow="w", run_id=1, status="DISPATCHED", issue_number=5,
             fix_branch="fix/issue-5", pr_number=None, attempts=2, rounds=0,
             agent_pid=111, agent_started_at="t", agent_log="/l",
             last_dispatched_head=None, extra_runs=[], created_at="t",
             updated_at="t", source="audit", kind="bug", audit_hash=None)
    d.update(kw)
    return d


def test_startup_reconcile_bot_fix_to_pr_open(monkeypatch):
    """臂①: boot 变化的死槽撞 BOT_FIX 条目(issue 键)——status 归
    PR_OPEN,attempts 不动。"""
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    st["entries"] = [_entry(status="BOT_FIX", pr_number=88)]
    tp.slot_acquire(st, "issue_fix", "issue-5", 111, 0, "/l", "/w",
                    "t", "old-boot")
    monkeypatch.setattr(tp, "boot_id", lambda: "new-boot")
    tp.startup_reconcile(st)
    e = tl.StateStore.entries(st)[0]
    assert e.status == "PR_OPEN" and e.attempts == 2
    assert tp.slot_get(st, "issue_fix", "issue-5") is None


def test_startup_reconcile_matches_pr_key(monkeypatch):
    """臂②: 槽 key = f"pr-{pr_number}"(车道 B 派发形态)同样命中条目
    (非 BOT_FIX → QUEUED)。"""
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    st["entries"] = [_entry(pr_number=88)]
    tp.slot_acquire(st, "pr_repair", "pr-88", 222, 0, "/l", "/w",
                    "t", "old-boot")
    monkeypatch.setattr(tp, "boot_id", lambda: "new-boot")
    tp.startup_reconcile(st)
    e = tl.StateStore.entries(st)[0]
    assert e.status == "QUEUED" and e.agent_pid is None
    assert tp.slot_get(st, "pr_repair", "pr-88") is None


def test_startup_reconcile_nonenvironmental_frees_slot_only(monkeypatch):
    """臂③: 非 environmental 死槽(boot 未变)只清槽,条目不动——判档仍归
    tl.reap_or_advance 正常记账。"""
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)
    st = _mkstate()
    st["entries"] = [_entry()]
    tp.slot_acquire(st, "issue_fix", "issue-5", 111, 0, "/l", "/w",
                    "t", tp.boot_id())          # 当前 boot: 非 environmental
    tp.startup_reconcile(st)
    assert tp.slot_get(st, "issue_fix", "issue-5") is None
    e = tl.StateStore.entries(st)[0]
    assert e.status == "DISPATCHED" and e.agent_pid == 111
    assert e.attempts == 2


# ============ Task 4: 槽位化派发 + worktree 隔离 ============


def _patch_dispatch_offline(monkeypatch, tmp_path):
    """dispatch_agent 离线夹具: state/clone 目录重定向 + 凭据 + 引擎缝全打桩。
    探活钉死 True(裁定#2): 槽位一旦占用保持占用;裸真探活不确定,且本机低号
    PID 是内核线程(EPERM 判活)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "cl"))
    os.makedirs(tmp_path / "st")
    (tmp_path / "st" / "agent.env").write_text("PATH=/usr/bin\n")
    monkeypatch.setattr(tl, "agent_alive", lambda pid: True)
    monkeypatch.setattr(tl, "_worktree_prepare",
                        lambda clone_dir, key: f"/wt/{key}")
    monkeypatch.setattr(tl, "gh_label_add", lambda *a: None)
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: True)


def test_dispatch_two_issue_agents_then_full(monkeypatch, tmp_path):
    """issue 泳道 2 槽: 第 3 个 QUEUED 条目被拒;内存不足也被拒。"""
    _patch_dispatch_offline(monkeypatch, tmp_path)
    # 桩 PID 固定小整数(裁定#2): brief 原文的 time 派生 PID 可能撞本机
    # 真实活 PID(低号多为内核线程),探活不确定会让账本时占时空。
    pids = iter((4242, 4243, 4244))
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: next(pids))

    def mk(n):
        return tl.Entry(workflow="w", run_id=n, status="QUEUED", issue_number=n,
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
    # 派发即记账: lane/worktree 落条目,worktree 进槽位账本
    assert es[0].lane == "issue_fix" and es[0].worktree == "/wt/issue-1"
    assert tp.slot_get(st, "issue_fix", "issue-1")["worktree"] == "/wt/issue-1"
    # 内存闸拒绝(第 3 条目在满槽闸已被拒、零记账,无需重置字段——控制器
    # 裁定#3: 原稿两行赋值是无效死行,已删)
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: False)
    assert tl.dispatch_agent(es[2], st) is None


def test_dispatch_pr_repair_uses_own_slots(monkeypatch, tmp_path):
    """pr_repair 与 issue_fix 互不占位: issue 2 槽用满后,pr 泳道照常派,
    且 rounds 记账在 pr 泳道(lane B 契约)。"""
    _patch_dispatch_offline(monkeypatch, tmp_path)
    pids = iter((4242, 4243, 4244))
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: next(pids))

    def mk(n, pr=None):
        return tl.Entry(workflow="w", run_id=n, status="QUEUED", issue_number=n,
                        fix_branch="", pr_number=pr, attempts=0, rounds=0,
                        agent_pid=None, agent_started_at=None, agent_log=None,
                        last_dispatched_head=None, extra_runs=[], created_at="t",
                        updated_at="t", source="audit", kind="bug",
                        audit_hash=None)

    st = {"entries": [mk(1).to_dict(), mk(2).to_dict(),
                      mk(3, pr=88).to_dict()],
          "auth_failure_streak": 0}
    es = tl.StateStore.entries(st)
    tl.dispatch_agent(es[0], st)
    tl.dispatch_agent(es[1], st)
    assert tp.free_slot_count(st, "issue_fix") == 0
    assert tl.dispatch_agent(es[2], st, increment_rounds=True) == 4244
    assert es[2].status == "BOT_FIX" and es[2].rounds == 1
    assert es[2].lane == "pr_repair" and es[2].worktree == "/wt/pr-88"
    assert tp.free_slot_count(st, "pr_repair") == 1
    assert tp.free_slot_count(st, "issue_fix") == 0   # pr 占用不影响 issue


def test_dispatch_same_key_second_refused_zero_spawn(monkeypatch, tmp_path):
    """同 key 二次派发: Task 5(控制器裁定#2)起前置早退——槽位账本已有
    同 key 记录(不判活)即直接拒派,零 spawn、零 killpg,条目记账保持
    首派(不重置 PID/状态/attempts)。旧「spawn 后 acquire 拒再 killpg
    击杀」的过渡期 churn 已消;slot_acquire 的击杀网仅剩并发竞态兜底。"""
    _patch_dispatch_offline(monkeypatch, tmp_path)
    pids = iter((4242, 4243))
    spawned = []
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: spawned.append(1) or next(pids))
    killed = []
    monkeypatch.setattr(os, "killpg",
                        lambda pid, sig: killed.append((pid, sig)))
    st = {"entries": [], "auth_failure_streak": 0}
    e = tl.StateStore.entries({"entries": [_entry()], "auth_failure_streak": 0})[0]
    assert tl.dispatch_agent(e, st) == 4242
    assert tl.dispatch_agent(e, st) is None        # 同 key issue-5 → 早退
    assert spawned == [1]                          # 第二次零 spawn
    assert killed == []                            # 无 killpg(不再 spawn-then-kill)
    assert e.agent_pid == 4242 and e.status == "DISPATCHED"
    assert e.attempts == 3 and e.lane == "issue_fix"
    assert len(st["slots"]["issue_fix"]) == 1


def test_dispatch_worktree_prepare_failure_refuses(monkeypatch, tmp_path):
    """worktree 准备失败(git 瞬时故障: fetch 网络抖动等)→ 拒派不炸 tick:
    返回 None、条目原状零记账、零 spawn——与 heal_clone 同款「本条拒绝、
    下轮重试」错误策略。"""
    _patch_dispatch_offline(monkeypatch, tmp_path)
    def boom(clone_dir, key):
        raise RuntimeError("git fetch origin --prune… 失败(exit 128): 网络抖动")
    monkeypatch.setattr(tl, "_worktree_prepare", boom)
    spawned = []
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: spawned.append(1) or 4242)
    e = tl.StateStore.entries({"entries": [_entry()],
                               "auth_failure_streak": 0})[0]
    assert tl.dispatch_agent(e, {"entries": [], "auth_failure_streak": 0}) is None
    assert e.status == "DISPATCHED" and e.attempts == 2
    assert spawned == []


def test_worktree_prepare_cleanup_roundtrip(tmp_path):
    """真实 git 集成: prepare 建于 origin/main、可重复 prepare(清残留);
    rmtree 兜底删目录后留 .git/worktrees 注册——cleanup 不 prune 该残留时
    同 key 重建必炸(exit 128「丢失但已注册」,2026-11-12 /tmp 实测)。"""
    bare = tmp_path / "origin.git"
    subprocess.run(["git", "init", "--bare", "-q", "-b", "main", str(bare)],
                   check=True)
    seed = tmp_path / "seed"
    subprocess.run(["git", "init", "-q", "-b", "main", str(seed)], check=True)
    (seed / "f.txt").write_text("x")
    subprocess.run(["git", "-C", str(seed), "add", "."], check=True)
    subprocess.run(["git", "-C", str(seed), "-c", "user.email=t@t",
                    "-c", "user.name=t", "commit", "-qm", "init"], check=True)
    subprocess.run(["git", "-C", str(seed), "push", "-q", str(bare), "main:main"],
                   check=True)
    clone = tmp_path / "clone"
    subprocess.run(["git", "clone", "-q", str(bare), str(clone)], check=True)
    wt = tl._worktree_prepare(str(clone), "issue-1")
    assert os.path.isfile(os.path.join(wt, "f.txt"))
    # 覆盖既有 worktree(重派残留): prepare 幂等
    assert tl._worktree_prepare(str(clone), "issue-1") == wt
    # rmtree 兜底现场(目录没了、注册残留): prepare 仍须能重建
    shutil.rmtree(wt)
    wt = tl._worktree_prepare(str(clone), "issue-1")
    assert os.path.isdir(wt)
    tl.worktree_cleanup(wt)
    assert not os.path.exists(wt)
    assert not os.listdir(os.path.join(clone, ".worktrees"))
    # 注册已 prune: 同 key 可再建
    assert os.path.isdir(tl._worktree_prepare(str(clone), "issue-1"))


# ============ Task 5: 动态轮次 + reap 释放槽位 + 同 key 占位早退 ============


def test_dynamic_rounds_constants():
    """动态轮次常量锚(spec 决策表 #10,取代固定 MAX_ROUNDS=4):
    docs-only/简单 diff → 2,其余 → 3;简单 diff 门槛 100 行 ∧ 5 文件。"""
    assert tl.MAX_ROUNDS_SIMPLE == 2
    assert tl.MAX_ROUNDS_COMPLEX == 3
    assert tl.ROUND_SIMPLE_MAX_DIFF_LINES == 100
    assert tl.ROUND_SIMPLE_MAX_FILES == 5


def test_max_rounds_for_matrix(monkeypatch):
    from types import SimpleNamespace as NS
    monkeypatch.setattr(tl, "pr_files", lambda n: ["docs/a.md"])
    assert tl.max_rounds_for(None, NS(pr_number=1, additions=5, deletions=5)) == 2
    # Minor B(fix-round 1): docs 臂优先于复杂度——docs-only 即使增删
    # 250 行(≥100)仍按 docs-only 上限 2
    assert tl.max_rounds_for(
        None, NS(pr_number=1, additions=200, deletions=50)) == 2
    monkeypatch.setattr(tl, "pr_files",
                        lambda n: ["a.c", "b.c", "c.c", "d.c", "e.c"])
    assert tl.max_rounds_for(
        None, NS(pr_number=2, additions=60, deletions=30)) == 2   # ≤100 行 ∧ ≤5 文件
    monkeypatch.setattr(tl, "pr_files", lambda n: ["a.c"] * 6)
    assert tl.max_rounds_for(
        None, NS(pr_number=3, additions=60, deletions=30)) == 3   # 6 文件 → 复杂
    assert tl.max_rounds_for(
        None, NS(pr_number=4, additions=200, deletions=50)) == 3
    # files 显式给出时不现拉(pr_files 不被调用也能判)
    monkeypatch.setattr(tl, "pr_files", lambda n: (_ for _ in ()).throw(
        AssertionError("files 已显式给出,不应再拉")))
    assert tl.max_rounds_for(
        None, NS(pr_number=6, additions=5, deletions=5),
        files=["docs/x.md"]) == 2


def test_max_rounds_for_additions_deletions_gh_fallback(monkeypatch):
    """Important#1(fix-round 1): PRData 无 additions/deletions 字段(现行
    pr_loop.PRData 即此形态,开工前已 grep 核实)→ 经 gh pr view --json
    additions,deletions 兜底(brief Consumes 明文)——不兜底则简单 diff 臂
    生产不可达。兜底拉取失败(GhError)按复杂=3(宁严勿松)。"""
    from types import SimpleNamespace as NS
    monkeypatch.setattr(tl, "pr_files", lambda n: ["a.c"])
    # 兜底拉到 40+30=70 ≤100 行 ∧ 1 文件 → 简单=2(gh_json_idem 经 gh_json
    # 的 json.loads 成 dict——桩镜像该契约,非裸 JSON 字符串)
    monkeypatch.setattr(tl, "gh_json_idem",
                        lambda args, input_text=None:
                        {"additions": 40, "deletions": 30})
    assert tl.max_rounds_for(None, NS(pr_number=7)) == 2
    # 兜底拉取失败 → 复杂=3
    def boom(args, input_text=None):
        raise tl.GhError("gh pr view … 失败(exit 1): HTTP 404")
    monkeypatch.setattr(tl, "gh_json_idem", boom)
    assert tl.max_rounds_for(None, NS(pr_number=8)) == 3
    # gh 空输出(gh_json 退出码 0 无输出 → None)→ or {} 防空,0+0 ≤100
    # ∧ 1 文件 → 简单=2(空 PR 无增删,0 行本就是最小简单 diff)
    monkeypatch.setattr(tl, "gh_json_idem", lambda args, input_text=None: None)
    assert tl.max_rounds_for(None, NS(pr_number=9)) == 2
    # PR 号双形解析: 真实 PRData 字段名是 number(无 pr_number)——审查者
    # 片段的 pr.pr_number 对它 AttributeError,离线 lane_b 用例当场逮到
    monkeypatch.setattr(tl, "gh_json_idem",
                        lambda args, input_text=None:
                        {"additions": 40, "deletions": 30})
    assert tl.max_rounds_for(None, NS(number=12)) == 2


def test_max_rounds_for_empty_files_strict_complex(monkeypatch):
    """Minor A(fix-round 1): files 拉取失败(空清单)即使增删字段齐全也按
    复杂=3——空表会冒充「≤5 文件」误走简单臂,与「宁严勿松」相悖。"""
    from types import SimpleNamespace as NS
    assert tl.max_rounds_for(
        None, NS(pr_number=10, additions=60, deletions=30), files=[]) == 3
    monkeypatch.setattr(tl, "pr_files", lambda n: [])
    assert tl.max_rounds_for(
        None, NS(pr_number=11, additions=60, deletions=30)) == 3


def test_pr_files_parses_lines_and_degrades_on_gherror(monkeypatch):
    """pr_files: gh_json_idem 输出按行拆(去空行);GhError → [](按复杂
    处理的诚实降级,绝不炸车道 B);gh 空输出(gh_json 退出码 0 无输出
    → None,空 diff PR 的 .files[].path 即此形态)→ [](Important#2,
    fix-round 1——旧实现 out.splitlines() 裸崩 AttributeError)。"""
    monkeypatch.setattr(tl, "gh_json_idem",
                        lambda args, input_text=None:
                        "docs/a.md\nsrc/b.c\n\n")
    assert tl.pr_files(88) == ["docs/a.md", "src/b.c"]

    def boom(args, input_text=None):
        raise tl.GhError("gh pr view … 失败(exit 1): HTTP 404")
    monkeypatch.setattr(tl, "gh_json_idem", boom)
    assert tl.pr_files(88) == []

    # Important#2: gh 空输出 → gh_json 返回 None → pr_files 不得裸崩
    monkeypatch.setattr(tl, "gh_json_idem", lambda args, input_text=None: None)
    assert tl.pr_files(88) == []


def test_docs_only_requires_all_md_and_nonempty():
    assert tl._docs_only(["a.md", "docs/b.md"]) is True
    assert tl._docs_only([]) is False            # 拉不到清单不冒充 docs-only
    assert tl._docs_only(["a.md", "b.c"]) is False


def test_reap_releases_issue_slot_on_exit(monkeypatch):
    """控制器裁定#1(车道 A 臂): agent 退场判档后、归还/重派之前释放
    issue_fix 槽位 + entry.worktree 清引用;attempts 记账语义不动。"""
    monkeypatch.setattr(tl, "check_agent_outcome", lambda e: "dead")
    monkeypatch.setattr(tl, "gh_label_remove", lambda n, lb: None)
    st = _mkstate()
    st["entries"] = [_entry(attempts=0, agent_pid=999999901,   # 必死伪 PID
                            worktree="/wt/issue-5")]
    tp.slot_acquire(st, "issue_fix", "issue-5", 999999901, 0, "/l",
                    "/wt/issue-5", "t", tp.boot_id())
    tl.reap_or_advance(st, dry_run=False)
    e = tl.StateStore.entries(st)[0]
    assert e.attempts == 1 and e.agent_pid is None   # dead → +1 重派语义不动
    assert e.worktree is None                        # 条目侧清引用
    assert tp.slot_get(st, "issue_fix", "issue-5") is None   # 槽已释放
    # 幂等: 无槽 state(过渡期 Task 4 前条目)reap 零炸、照常记账
    st2 = _mkstate()
    st2["entries"] = [_entry(attempts=0, agent_pid=999999901)]
    tl.reap_or_advance(st2, dry_run=False)
    assert tl.StateStore.entries(st2)[0].attempts == 1


def test_reap_releases_pr_repair_slot_on_bot_fix_exit(monkeypatch):
    """控制器裁定#1(车道 B 臂): BOT_FIX agent auth_fail 退场(未达熔断
    阈值)→ 归还 PR_OPEN 的同时释放 pr_repair 槽位;rounds 记账不动。"""
    monkeypatch.setattr(tl, "check_agent_outcome", lambda e: "auth_fail")
    st = _mkstate()
    st["entries"] = [_entry(status="BOT_FIX", pr_number=88, rounds=2,
                            agent_pid=999999902, worktree="/wt/pr-88")]
    tp.slot_acquire(st, "pr_repair", "pr-88", 999999902, 0, "/l",
                    "/wt/pr-88", "t", tp.boot_id())
    tl.reap_or_advance(st, dry_run=False)
    e = tl.StateStore.entries(st)[0]
    assert e.status == "PR_OPEN" and e.rounds == 2   # 车道 B 记账语义不动
    assert e.agent_pid is None and e.worktree is None
    assert tp.slot_get(st, "pr_repair", "pr-88") is None


def test_dispatch_same_key_dead_slot_zero_spawn(monkeypatch, tmp_path,
                                                capsys):
    """控制器裁定#2 配套: 死槽占 key(未清死槽,过渡期形态)→ dispatch
    前置早退返回 None、零 spawn(消 spawn-then-killpg churn);早退按 key
    精确命中,不拦其他条目。"""
    _patch_dispatch_offline(monkeypatch, tmp_path)
    monkeypatch.setattr(tl, "agent_alive", lambda pid: False)  # 槽内 PID 已死
    spawned = []
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: spawned.append(1) or 4242)
    st = _mkstate()
    tp.slot_acquire(st, "issue_fix", "issue-5", 999999901, 0, "/l",
                    "/wt/issue-5", "t", tp.boot_id())
    e = tl.StateStore.entries({"entries": [_entry()], "auth_failure_streak": 0})[0]
    assert tl.dispatch_agent(e, st) is None          # 死槽占 key → 早退
    assert spawned == []                            # 零 spawn
    assert "已占用" in capsys.readouterr().out
    assert e.attempts == 2 and e.status == "DISPATCHED"   # 零记账
    # key 精确: 其他条目(issue-6)不受死槽影响,照常派发
    e6 = tl.StateStore.entries({"entries": [_entry(issue_number=6)],
                                "auth_failure_streak": 0})[0]
    assert tl.dispatch_agent(e6, st) == 4242
    assert spawned == [1]


# ============ Task 6: 泳道2 检视派发器 + --pr 单 PR CLI ============


def _patch_review_offline(monkeypatch, tmp_path, alive=True):
    """dispatch_review 离线夹具(控制器裁定 R3): 探活一律 monkeypatch
    tl.agent_alive,勿裸真 PID——本机低号 PID 是内核线程,agent_alive 对
    EPERM 判活,宿主相关必假活。state 目录重定向(命令串/log 纯字符串
    构造,零真实落盘);run_background 桩返回固定小整数 PID 序列(4242…)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    monkeypatch.setattr(tl, "agent_alive", lambda pid: alive)
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: True)
    monkeypatch.setattr(tp, "boot_id", lambda: "b")
    pids = iter((4242, 4243, 4244, 4245))
    monkeypatch.setattr(tl, "run_background",
                        lambda cmd, cwd, log: next(pids))


def test_dispatch_review_per_night_cap_with_self_release(monkeypatch, tmp_path):
    """R3 死槽自释放语义: agent 全死(探活恒 False)→ 同 (pr,head) 重试先
    释放自身 key 死槽,第 1、2 次可派(tries=1,2);第 3 次撞每晚上限
    REVIEW_PER_HEAD_PER_NIGHT=2 拒,且拒绝不增计数。"""
    _patch_review_offline(monkeypatch, tmp_path, alive=False)
    st = _mkstate()
    assert tp.dispatch_review(st, 640, "h1") == 4242        # tries=1
    assert st["review_tries"]["640:h1"] == 1                 # 键 f"{pr}:{head}"
    assert tp.dispatch_review(st, 640, "h1") == 4243        # 死槽自释放, tries=2
    assert st["review_tries"]["640:h1"] == 2
    assert tp.dispatch_review(st, 640, "h1") is None        # ≥2 → 每晚拒
    assert st["review_tries"]["640:h1"] == 2                # 拒绝不增计数
    # 内存闸拒: 闸在计数/槽位之前,零 spawn、零记账
    monkeypatch.setattr(tp, "mem_gate_ok", lambda: False)
    assert tp.dispatch_review(st, 640, "h9") is None
    assert "640:h9" not in st["review_tries"]


def test_dispatch_review_live_same_key_and_full_slots(monkeypatch, tmp_path):
    """agent 全活: 同 key 活占位时重派 → spawn 后 acquire 同 key 互斥拒 →
    killpg 整组击杀兜底,不烧每晚预算;两槽活满后第 3 个不同 PR 满槽拒
    (零 spawn、零计数)。"""
    _patch_review_offline(monkeypatch, tmp_path, alive=True)
    killed = []
    monkeypatch.setattr(os, "killpg",
                        lambda pid, sig: killed.append((pid, sig)))
    st = _mkstate()
    assert tp.dispatch_review(st, 640, "h1") == 4242
    assert tp.dispatch_review(st, 640, "h1") is None        # 同 key 互斥
    assert killed == [(4243, 9)]                             # 击杀刚 spawn 的
    assert st["review_tries"]["640:h1"] == 1                # 击杀不烧预算
    assert tp.dispatch_review(st, 641, "h2") == 4244
    assert tp.dispatch_review(st, 642, "h3") is None          # 满槽拒
    assert "642:h3" not in st["review_tries"]


def test_dispatch_review_acquire_fail_killpg_fallback(monkeypatch, tmp_path):
    """acquire 失败(killpg 兜底): monkeypatch slot_acquire → False,断言
    os.killpg 被调(打桩记录)且返回 None、tries 不增(击杀不烧预算)。"""
    _patch_review_offline(monkeypatch, tmp_path, alive=True)
    monkeypatch.setattr(tp, "slot_acquire", lambda *a, **kw: False)
    killed = []
    monkeypatch.setattr(os, "killpg",
                        lambda pid, sig: killed.append((pid, sig)))
    st = _mkstate()
    assert tp.dispatch_review(st, 640, "h1") is None
    assert killed == [(4242, 9)]
    assert st["review_tries"] == {}


def test_reset_review_tries_nightly_boundary(monkeypatch, tmp_path):
    """每晚边界: reset_review_tries(pipeline 启动时清零)后同 (pr,head)
    立即可再派,计数从 1 起步。"""
    _patch_review_offline(monkeypatch, tmp_path, alive=False)
    st = _mkstate()
    assert tp.dispatch_review(st, 640, "h1") == 4242
    assert tp.dispatch_review(st, 640, "h1") == 4243
    assert tp.dispatch_review(st, 640, "h1") is None          # 每晚上限 2
    tp.reset_review_tries(st)
    assert st["review_tries"] == {}
    assert tp.dispatch_review(st, 640, "h1") == 4244          # 新夜可再派
    assert st["review_tries"]["640:h1"] == 1


def test_dispatch_review_spawn_command_shape(monkeypatch, tmp_path):
    """派发契约: spawn `--pr <N>`(timeout 5400s / agent.env 凭据 /
    setsid+`echo $!` 回显形态)、log = <STATE_DIR>/review-pr<N>-<ts>.log、
    cwd = 克隆目录;常量锚 REVIEW_AGENT_TIMEOUT_SEC=5400 /
    REVIEW_PER_HEAD_PER_NIGHT=2。"""
    _patch_review_offline(monkeypatch, tmp_path, alive=True)
    monkeypatch.setenv("TASKLOOP_CLONE_DIR", str(tmp_path / "cl"))
    seen = {}
    monkeypatch.setattr(
        tl, "run_background",
        lambda cmd, cwd, log: seen.update(cmd=cmd, cwd=cwd, log=log) or 4242)
    st = _mkstate()
    assert tp.dispatch_review(st, 640, "h1") == 4242
    assert tp.REVIEW_AGENT_TIMEOUT_SEC == 5400
    assert tp.REVIEW_PER_HEAD_PER_NIGHT == 2
    assert "--pr 640" in seen["cmd"]
    assert "timeout 5400 /usr/bin/python3" in seen["cmd"]
    assert "pr_local_review.py" in seen["cmd"]
    assert "agent.env" in seen["cmd"]
    assert "setsid" in seen["cmd"] and "echo $!" in seen["cmd"]
    assert seen["cwd"] == str(tmp_path / "cl")
    assert seen["log"].startswith(str(tmp_path / "st"))
    assert "review-pr640-" in seen["log"]


def test_pr_local_review_cli_pr_flag_and_mutex(monkeypatch, capsys):
    """--pr 单 PR CLI: 与 --all-open 互斥(argparse error → SystemExit 2,
    任一顺序);--pr 与位置参数 <PR> 共用单 PR 路径 review_one_pr(取 head
    sha → run_review → publish_review,spec §4 泳道2「发布检视」);无参
    用法错误 exit 2。"""
    import pr_local_review as plr
    for argv in (["--pr", "640", "--all-open"],
                 ["--all-open", "--pr", "640"]):
        try:
            plr.main(argv)
            code = None
        except SystemExit as exc:
            code = exc.code
        assert code == 2
    assert "互斥" in capsys.readouterr().err
    calls = []
    monkeypatch.setattr(plr, "review_one_pr", lambda n: calls.append(n))
    assert plr.main(["--pr", "640"]) == 0
    assert plr.main(["641"]) == 0                    # 位置参数同路径
    assert calls == [640, 641]
    assert plr.main([]) == 2                          # 无参用法错误
    assert calls == [640, 641]
    assert "需指定" in capsys.readouterr().err


# ============ Task 7: merge 门 evaluate_merge_gate ============


def _pr(state_="open", draft=False, mergeable=True, title="T",
        local=True, latest=100000, opinions=0):
    """merge 门 PR 替身(控制器裁定#3): SimpleNamespace 形状与真实 PRData
    关键字段吻合(bot_done/latest_signal/opinion_count/pending_skew_opinions/
    annotations_fetch_failed/draft/mergeable/state/title/head_sha);PR 号
    双形 pr_number+number(真实 PRData 字段名是 number,T5 _pr_number 同款
    双形契约);latest=最新信号距 now 秒数(默认远超静默窗),None →
    latest_signal=None(静默窗未起步臂)。"""
    return NS(state=state_, draft=draft, mergeable=mergeable, title=title,
              head_sha="abc1234", url="u", pr_number=1, number=1, bot_done={
                  "codex": "x", "coderabbit": "x", "pr-agent": "x",
                  "greptile": "x",
                  "local-claude": "x" if local else None},
              latest_signal=None if latest is None else
              datetime.now(timezone.utc) - timedelta(seconds=latest),
              opinion_count=opinions, pending_skew_opinions=0,
              annotations_fetch_failed=False)


def _gate_entry(fix_branch="fix/issue-1", pr_number=1, issue_number=1):
    return NS(pr_number=pr_number, issue_number=issue_number,
              fix_branch=fix_branch, status="PR_OPEN")


def _gh_stub(rollup_rows, baselines=None):
    """tl.gh_json_idem 离线桩(零触网): pr view statusCheckRollup → 三列
    TSV(name\tconclusion\tworkflowName——rollup 实测形态,裁定#2 数据流);
    run list → 基线 run id(真 gh `--jq '.[0].databaseId]'` 输出裸数字,
    gh_json json.loads 得 int;空清单真 gh 输出 null → None 契约);
    run view → 该基线 run 的红 job 行。baselines: {workflowName:
    (run_id, "红 job 名多行串")}。"""
    baselines = baselines or {}

    def fake(args, input_text=None):
        if "statusCheckRollup" in args:
            return "".join(f"{n}\t{c}\t{w}\n" for n, c, w in rollup_rows)
        if args[:2] == ["run", "list"]:
            wf = args[args.index("--workflow") + 1]
            return baselines[wf][0] if wf in baselines else None
        if args[:2] == ["run", "view"]:
            rid = str(args[2])
            for wf, (run_id, jobs) in baselines.items():
                if str(run_id) == rid:
                    return jobs
            raise tl.GhError(f"gh run view {rid}… 失败(exit 1)")
        raise AssertionError(f"意外 gh 调用: {args}")
    return fake


def test_required_checks_and_engine_branch_re_calibration():
    """常量锚(裁定#1 校准,2026-11-13 对现役 PR #661/#660/#643/#642
    `gh pr view --json statusCheckRollup` 实测四 PR 完全一致): rollup 的
    CheckRun 实名是 lint / git-sanity / Build binary for CPU (Linux) /
    zizmor (workflow security)——分支保护 UI 建议集写法「Security / zizmor」
    在 rollup 不出现;checks_ok 按前缀匹配,常量写主干(矩阵后缀形态由
    前缀覆盖,zizmor 状态上下文裸名「zizmor」同被主干覆盖)。"""
    assert tp.REQUIRED_CHECKS_FULL == ["lint", "git-sanity",
                                       "Build binary for CPU (Linux)", "zizmor"]
    assert tp.REQUIRED_CHECKS_DOCS == ["lint", "git-sanity"]
    assert tp.DOCS_QUIET_SEC == 300
    for branch in ("fix/issue-1", "fix/issue-123", "fix/ci-45",
                   "fix/revert-pr67", "fix/issue-1-more"):
        # 前缀匹配语义(brief 原文 re.match 无 $ 锚,spec glob fix/issue-*):
        # fix/issue-1-more 属 fix/issue-* 形,应过
        assert tp.ENGINE_BRANCH_RE.match(branch), branch
    for branch in ("fix/user-thing", "user-branch", "fix/issue-abc",
                   "", "fix/revert-42"):
        assert not tp.ENGINE_BRANCH_RE.match(branch), branch


def test_wip_gated_and_docs_only_shapes():
    assert tp.wip_gated("WIP: feat/x") is True
    assert tp.wip_gated("feat: 正常标题") is False
    assert tp.wip_gated("") is False
    assert tp.docs_only(["a.md", "docs/b.md"]) is True
    assert tp.docs_only([]) is False            # 空清单不冒充 docs-only
    assert tp.docs_only(["a.md", "b.c"]) is False
    # 与 tl._docs_only 单一真源一致(T5 已锚;同一判定喂轮次上限与快道)
    for files in (["a.md"], [], ["a.md", "b.c"], ["x.md", "y.md"]):
        assert tp.docs_only(files) == tl._docs_only(files)


def test_docs_ready_matrix():
    """docs 快道门槛 2 放宽四要素: 第5bot 唯一 + DOCS_QUIET_SEC=300s 静默 +
    零意见(含 pending 超容差/annotations 完整性,同 classify 口径)。"""
    now = datetime.now(timezone.utc)
    assert tp.docs_ready(_pr(latest=tp.DOCS_QUIET_SEC + 1), now) is True
    pr = _pr(latest=None)
    pr.latest_signal = now - timedelta(seconds=tp.DOCS_QUIET_SEC)
    assert tp.docs_ready(pr, now) is True          # 边界 ≥(恰 300s 过)
    pr = _pr(latest=None)
    pr.latest_signal = now - timedelta(seconds=tp.DOCS_QUIET_SEC - 1)
    assert tp.docs_ready(pr, now) is False         # 差 1s 未过窗
    assert tp.docs_ready(_pr(latest=None), now) is False      # 无信号
    assert tp.docs_ready(_pr(local=False), now) is False     # 第5bot 缺
    assert tp.docs_ready(_pr(opinions=1), now) is False      # 有可行动意见
    pr = _pr(); pr.pending_skew_opinions = 1
    assert tp.docs_ready(pr, now) is False                    # 超容差意见
    pr = _pr(); pr.annotations_fetch_failed = True
    assert tp.docs_ready(pr, now) is False                    # annotations 不完整


def test_baseline_reds_gh_shapes_and_degradation(monkeypatch):
    """baseline_reds(裁定#2: gh 调用形态保持 brief 原文): run list
    --workflow X --branch main --event schedule,push --limit 1 → run view
    取失败 job 名;空 workflow → 空集零 gh;GhError/gh 空输出 → 空集
    (空集=不放行任何红,诚实从严);run id 真实形态是 int(gh_json 对裸数字
    json.loads)——str() 归一防 int.strip() 崩。"""
    rec = {}

    def fake(args, input_text=None):
        if args[:2] == ["run", "list"]:
            rec["list"] = args
            return 77
        rec.setdefault("view", []).append(args)
        return "lint\nbuild-arm\n"
    monkeypatch.setattr(tl, "gh_json_idem", fake)
    assert tp.baseline_reds("o/r", "Pull Request") == {"lint", "build-arm"}
    assert rec["list"][rec["list"].index("--workflow") + 1] == "Pull Request"
    assert rec["list"][rec["list"].index("--branch") + 1] == tl.MONITORED_BRANCH
    assert rec["list"][rec["list"].index("--event") + 1] == "schedule,push"
    assert rec["list"][rec["list"].index("--limit") + 1] == "1"
    assert "77" in rec["view"][0]
    # 空 workflow → 空集且零 gh 调用(rollup 状态上下文无从对照即不放行)
    called = []
    monkeypatch.setattr(tl, "gh_json_idem",
                        lambda a, input_text=None: called.append(1) or 77)
    assert tp.baseline_reds("o/r", "") == set()
    assert tp.baseline_reds("o/r", None) == set()
    assert called == []
    # run list 失败 / run view 失败 → 空集
    def boom_list(args, input_text=None):
        if args[:2] == ["run", "list"]:
            raise tl.GhError("gh run list … 失败(exit 1): HTTP 502")
        return "x"
    monkeypatch.setattr(tl, "gh_json_idem", boom_list)
    assert tp.baseline_reds("o/r", "Pull Request") == set()

    def boom_view(args, input_text=None):
        if args[:2] == ["run", "view"]:
            raise tl.GhError("gh run view … 失败(exit 1): HTTP 502")
        return 77
    monkeypatch.setattr(tl, "gh_json_idem", boom_view)
    assert tp.baseline_reds("o/r", "Pull Request") == set()
    # gh 空输出(run list 无匹配 → null → None)不炸 → 空集
    monkeypatch.setattr(tl, "gh_json_idem", lambda a, input_text=None: None)
    assert tp.baseline_reds("o/r", "Pull Request") == set()


def test_checks_ok_green_prefix_and_case_forms(monkeypatch):
    """全绿 rollup → (True, []);前缀匹配覆盖矩阵后缀形态;结论大小写归一
    (实测 rollup 结论大写 SUCCESS/SKIPPED/FAILURE——brief 的
    (success, skipped, SUCCESS) 漏大写 SKIPPED,校准修正为小写归一);
    SKIPPED 不算红(docs-only 重 job 常见形态)。"""
    rows = [("lint", "SUCCESS", "Pull Request"),
            ("git-sanity", "success", "Pull Request"),   # 小写亦绿
            ("Build binary for CPU (Linux) (GCC-arm64, -arm, unittests, "
             "selftests)", "SKIPPED", "Pull Request"),    # 矩阵后缀 + skip
            ("zizmor (workflow security)", "SUCCESS", "Security"),
            ("Multi-version (${{ matrix.series }})", "SKIPPED", "Pull Request"),
            ("CodeRabbit", "SKIPPED", "")]                # 状态上下文空 workflow
    monkeypatch.setattr(tl, "gh_json_idem", _gh_stub(rows))
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_FULL, "o/r")
    assert ok is True and why == []


def test_checks_ok_required_missing_and_gherror(monkeypatch):
    rows = [("lint", "SUCCESS", "Pull Request"),
            ("git-sanity", "SUCCESS", "Pull Request")]   # Build binary/zizmor 缺席
    monkeypatch.setattr(tl, "gh_json_idem", _gh_stub(rows))
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_FULL, "o/r")
    assert ok is False and "required check 缺席" in why[0]
    # docs 集(lint+git-sanity)在 → True
    ok, _ = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_DOCS, "o/r")
    assert ok is True
    # rollup 拉取失败 → False + 证据
    def boom(args, input_text=None):
        raise tl.GhError("gh pr view … 失败(exit 1): HTTP 502")
    monkeypatch.setattr(tl, "gh_json_idem", boom)
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_DOCS, "o/r")
    assert ok is False and "check rollup 拉取失败" in why[0]
    # gh 空输出(gh_json None 契约)不炸 → 零 check → 缺席拒绝
    monkeypatch.setattr(tl, "gh_json_idem", lambda a, input_text=None: None)
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_DOCS, "o/r")
    assert ok is False and "required check 缺席" in why[0]


def test_checks_ok_inherited_red_by_workflow_groups(monkeypatch):
    """继承红(裁定#2 数据流): 红 job 的 workflowName 从 rollup 行收集,
    按 workflow 分组对照 main 基线——全组 ⊆ 基线 → 放行并携带证据;
    任一组有非基线红 → 拒;空 workflowName(状态上下文)基线空集 → 拒。"""
    rows = [("lint", "FAILURE", "Pull Request"),
            ("git-sanity", "SUCCESS", "Pull Request"),
            ("Build binary for CPU (Linux)", "SUCCESS", "Pull Request"),
            ("zizmor (workflow security)", "SUCCESS", "Security"),
            ("spell-check", "FAILURE", "Docs Lint")]
    base = {"Pull Request": (101, "lint\n"),   # main 同 workflow 基线红
            "Docs Lint": (102, "")}             # 基线无 spell-check
    monkeypatch.setattr(tl, "gh_json_idem", _gh_stub(rows, base))
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_FULL, "o/r")
    assert ok is False and "非基线继承" in why[0] and "spell-check" in why[0]
    # 去掉 Docs Lint 外来红 → lint 红继承自基线 → 放行 + 证据
    monkeypatch.setattr(tl, "gh_json_idem", _gh_stub(rows[:4], base))
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_FULL, "o/r")
    assert ok is True and "继承红放行" in why[0] and "lint" in why[0]
    # 空 workflowName 的红(状态上下文)→ 无从对照 → 拒
    rows3 = [("lint", "SUCCESS", "Pull Request"),
             ("git-sanity", "SUCCESS", "Pull Request"),
             ("Build binary for CPU (Linux)", "SUCCESS", "Pull Request"),
             ("zizmor (workflow security)", "SUCCESS", "Security"),
             ("CodeRabbit", "FAILURE", "")]
    monkeypatch.setattr(tl, "gh_json_idem", _gh_stub(rows3, base))
    ok, why = tp.checks_ok(_pr(), tp.REQUIRED_CHECKS_FULL, "o/r")
    assert ok is False and "非基线继承" in why[0] and "CodeRabbit" in why[0]


def test_gate_full_path_engine_pr_ok(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    st = _mkstate()
    seen = {}
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda pr, q, now: "READY_TO_MERGE")
    monkeypatch.setattr(tp, "checks_ok",
                        lambda pr, req, slug: seen.update(req=req, slug=slug)
                        or (True, []))
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tl, "repo_slug", lambda: "o/r")
    e = _gate_entry()
    r = tp.evaluate_merge_gate(st, e, _pr(), ["a.c"],
                              datetime.now(timezone.utc), False)
    assert r.ok is True and r.fast is False
    assert seen["req"] == tp.REQUIRED_CHECKS_FULL   # 全门槛查 FULL 集
    assert seen["slug"] == "o/r"
    assert r.reasons == []


def test_gate_rejects_user_wip_and_missing_ownership():
    st = _mkstate()
    r = tp.evaluate_merge_gate(st, None, _pr(title="WIP: feat/x"),
                               ["a.md"], datetime.now(timezone.utc), False)
    assert r.ok is False and "WIP" in " ".join(r.reasons)   # 草稿闸
    # 非 docs 归属缺条目也拒(brief 注): 用户代码 PR 无引擎条目 → 门槛 1 拒
    r2 = tp.evaluate_merge_gate(st, None, _pr(), ["a.c"],
                                datetime.now(timezone.utc), False)
    assert r2.ok is False and "非引擎 PR" in " ".join(r2.reasons)


def test_gate_rejects_non_engine_branch():
    """门槛 1 引擎分支形: fix/issue-N|fix/ci-N|fix/revert-prN 之外拒
    (用户 WIP 分支结构性排除,无白名单)。"""
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(fix_branch="fix/user-x"),
                               _pr(), ["a.c"], datetime.now(timezone.utc), False)
    assert r.ok is False and "非引擎分支" in " ".join(r.reasons)


def test_gate_rejects_closed_draft_conflict():
    """门槛 4 状态三拒臂: 非 open / DRAFT / mergeable=false。"""
    now = datetime.now(timezone.utc)
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(),
                               _pr(state_="closed"), ["a.c"], now, False)
    assert r.ok is False and "非 open/DRAFT" in " ".join(r.reasons)
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(),
                               _pr(draft=True), ["a.c"], now, False)
    assert r.ok is False and "非 open/DRAFT" in " ".join(r.reasons)
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(),
                               _pr(mergeable=False), ["a.c"], now, False)
    assert r.ok is False and "冲突" in " ".join(r.reasons)


def test_gate_docs_fast_path(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    st = _mkstate()
    seen = {}
    monkeypatch.setattr(tp, "checks_ok",
                        lambda pr, req, slug: seen.update(req=req) or (True, []))
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tl, "repo_slug", lambda: "o/r")
    # 快道不查外部四 bot(门槛 2 放宽: 第5bot 唯一)——classify 一触即炸
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda pr, q, now: (_ for _ in ()).throw(
                            AssertionError("docs 快道不应走 classify_state")))
    now = datetime.now(timezone.utc)
    pr = _pr(local=True, opinions=0)
    pr.latest_signal = now - timedelta(seconds=tp.DOCS_QUIET_SEC + 10)
    r = tp.evaluate_merge_gate(st, None, pr, ["a.md"], now, False)  # 用户 docs PR
    assert r.ok is True and r.fast is True
    assert seen["req"] == tp.REQUIRED_CHECKS_DOCS   # 快道只查快 job


def test_gate_docs_with_opinions_not_ok():
    now = datetime.now(timezone.utc)
    pr = _pr(local=True, opinions=2)
    pr.latest_signal = now - timedelta(seconds=400)
    r = tp.evaluate_merge_gate(_mkstate(), None, pr, ["a.md"], now, False)
    assert r.ok is False
    assert "docs 快道未就绪" in " ".join(r.reasons)


def test_gate_engine_entry_docs_files_take_fast_path(monkeypatch, tmp_path):
    """引擎条目的 docs-only PR 同样走快道(门槛 1/2/3 docs 例外适用任意
    PR,spec §5): 引擎分支仍须匹配,检视只等第5bot+300s,CI 只查快 job。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    seen = {}
    monkeypatch.setattr(tp, "checks_ok",
                        lambda pr, req, slug: seen.update(req=req) or (True, []))
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tl, "repo_slug", lambda: "o/r")
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda pr, q, now: (_ for _ in ()).throw(
                            AssertionError("docs 快道不应走 classify_state")))
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(), _pr(), ["a.md"],
                               datetime.now(timezone.utc), False)
    assert r.ok is True and r.fast is True
    assert seen["req"] == tp.REQUIRED_CHECKS_DOCS


def test_gate_engine_classify_not_ready_rejects(monkeypatch, tmp_path):
    """全门槛路径检视判定委托 pr_loop.classify_state(五 bot/静默/意见),
    DEFAULT_QUIET 口径;≠READY_TO_MERGE 即拒。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    seen = {}

    def cls(pr, quiet, now_arg):
        seen["quiet"] = quiet
        return "READY_TO_FIX"
    monkeypatch.setattr(tl.pr_loop, "classify_state", cls)
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(), _pr(), ["a.c"],
                               datetime.now(timezone.utc), False)
    assert r.ok is False and "READY_TO_FIX" in " ".join(r.reasons)
    assert seen["quiet"] == tl.pr_loop.DEFAULT_QUIET


def test_gate_rejects_when_checks_fail_and_when_paused(monkeypatch, tmp_path):
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda pr, q, now: "READY_TO_MERGE")
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tl, "repo_slug", lambda: "o/r")
    now = datetime.now(timezone.utc)
    monkeypatch.setattr(tp, "checks_ok",
                        lambda pr, req, slug: (False, ["required check 缺席: ['zizmor']"]))
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(), _pr(), ["a.c"],
                               now, False)
    assert r.ok is False and "required check 缺席" in " ".join(r.reasons)
    # 门槛 5: PAUSED 熔断(引擎与 docs 快道同样受熔断)
    monkeypatch.setattr(tp, "checks_ok", lambda pr, req, slug: (True, []))
    monkeypatch.setattr(tl, "_paused", lambda: True)
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(), _pr(), ["a.c"],
                               now, False)
    assert r.ok is False and "PAUSED" in " ".join(r.reasons)
    r = tp.evaluate_merge_gate(_mkstate(), None, _pr(latest=1000), ["a.md"],
                               now, False)
    assert r.ok is False and "PAUSED" in " ".join(r.reasons)


def test_gate_engine_inherited_red_passes_with_evidence(monkeypatch, tmp_path):
    """全门槛 + 继承红放行(rollup workflowName 数据流,裁定#2): rollup
    lint 红 ⊆ main 同 workflow 基线红 → 门过且 reasons 携带证据行
    (spec §5 门槛 6 审计评论的输入;真实 PR #661 现况即 lint 继承红)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    monkeypatch.setattr(tl.pr_loop, "classify_state",
                        lambda pr, q, now: "READY_TO_MERGE")
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tl, "repo_slug", lambda: "o/r")
    rows = [("lint", "FAILURE", "Pull Request"),
            ("git-sanity", "SUCCESS", "Pull Request"),
            ("Build binary for CPU (Linux)", "SUCCESS", "Pull Request"),
            ("zizmor (workflow security)", "SUCCESS", "Security")]
    monkeypatch.setattr(tl, "gh_json_idem",
                        _gh_stub(rows, {"Pull Request": (101, "lint\n")}))
    r = tp.evaluate_merge_gate(_mkstate(), _gate_entry(), _pr(), ["a.c"],
                               datetime.now(timezone.utc), False)
    assert r.ok is True and r.fast is False
    assert any("继承红放行" in x and "lint" in x for x in r.reasons)


def test_gate_no_auto_merge_and_paused(monkeypatch, tmp_path):
    """门槛 5 熔断参数臂(裁定#4: NO_AUTO_MERGE 文件的检测函数
    no_auto_merge_active 属 T8,门只收 no_auto_merge: bool——文件落盘仅为
    T8 接线形态示例);存在 → 门转纯报告模式(ok=False,灰度)。"""
    monkeypatch.setenv("TASKLOOP_STATE_DIR", str(tmp_path / "st"))
    (tmp_path / "st").mkdir(parents=True)
    (tmp_path / "st" / "NO_AUTO_MERGE").write_text("")
    monkeypatch.setattr(tl, "_paused", lambda: False)
    monkeypatch.setattr(tp, "checks_ok", lambda pr, req, slug: (True, []))
    monkeypatch.setattr(tl, "repo_slug", lambda: "o/r")
    now = datetime.now(timezone.utc)
    pr = _pr(); pr.latest_signal = now - timedelta(seconds=1000)
    r = tp.evaluate_merge_gate(_mkstate(), None, pr, ["a.md"], now, True)
    assert r.ok is False and "NO_AUTO_MERGE" in " ".join(r.reasons)

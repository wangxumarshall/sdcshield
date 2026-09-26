import os, shlex, shutil, subprocess, time
import pytest

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")
DRIVER = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc-excite-reproduce.sh")

# Task 5：复测内存护栏 + control 孤儿清理。
# 背景（RCA 附3）：cold_c4 事件的 wholecmd 重放（900s×3）在内存紧张期把机器推进 OOM，
# 且泄漏的复测命令留下 comm=control 的孤儿切片（framework/sandstone_run.cpp:1242
# prctl 改名——pkill -x sdcshield 够不着）。
# 护栏只针对复测（v5 §8.6 分工：主负载的内存防线是 monitor 联锁）。
#
# ======================= 红线（评审 round 2，2026-09-26）=======================
# 战役运行期本文件测试必须经 SDC_ORPHAN_PIDS 沙盒——orphan_sandbox fixture 统一注入，
# 禁止绕过。cleanup_orphans 的生产路径是 pkill -KILL -x control -u <user>，与运行中
# 战役的切片（comm=control 同用户）不可区分——无沙盒时期实跑已污染战役（125726 阶段
# .out 19 条 code:9 'Killed' 假 crash + events/20260926-131429 复测事件）。凡新增触及
# retest_guarded 击杀路径 / cleanup_orphans 的测试，一律在本 fixture 覆盖下运行。
# =============================================================================


def _spawn_control(tmp_path, sub):
    # 派生 comm=control 的 setsid 孤儿孙进程（框架切片形态：prctl 改名 + signals.cpp
    # setsid 自成组）。pid 由进程自报经文件——setsid 可能 fork，$! 不可靠（实测踩坑）。
    d = tmp_path / sub
    d.mkdir(exist_ok=True)
    ctrl = d / "control"
    shutil.copy("/bin/sleep", ctrl); ctrl.chmod(0o755)
    pidf = d / "pid"
    # >/dev/null 2>&1：幸存者不得持有 capture 管道写端（否则 subprocess.run 等 EOF 挂起）
    subprocess.run(["bash", "-c",
                    f"setsid bash -c 'echo $$ > {pidf}; exec {ctrl} 300' >/dev/null 2>&1 &"],
                   capture_output=True, timeout=10)
    for _ in range(40):                    # 等自报 pid（≤2s）
        if pidf.exists():
            try:
                return int(pidf.read_text().strip())
            except ValueError:
                pass
        time.sleep(0.05)
    raise AssertionError("control 沙盒目标未启动")


@pytest.fixture(autouse=True)
def orphan_sandbox(tmp_path):
    # SDC_ORPHAN_PIDS 沙盒（评审 round 2）：注册测试自建 control 孙进程 pid，
    # cleanup_orphans 走 pid 表杀（战役零影响）；生产 pkill 路径由源码断言覆盖。
    pid = _spawn_control(tmp_path, "sandbox")
    old = os.environ.get("SDC_ORPHAN_PIDS")
    os.environ["SDC_ORPHAN_PIDS"] = str(pid)
    yield pid
    subprocess.run(["kill", "-KILL", str(pid)], capture_output=True)   # 自清
    if old is None:
        os.environ.pop("SDC_ORPHAN_PIDS", None)
    else:
        os.environ["SDC_ORPHAN_PIDS"] = old


def bash(body, timeout=30):
    return subprocess.run(["bash", "-c", f"source '{COMMON}'\n{body}"],
                          capture_output=True, text=True, timeout=timeout)


def _fake_bin(tmp_path, body):
    p = tmp_path / "fakebin"
    p.write_text("#!/bin/bash\n" + body + "\n")
    p.chmod(0o755)
    return p


# ---------------- mem_below 纯函数（brief Step 1 原样）----------------

def test_mem_below():
    assert bash("mem_below 100 200 && echo Y").stdout.strip().endswith("Y")
    assert bash("mem_below 300 200 || echo N").stdout.strip().endswith("N")
    assert bash("mem_below '' 200 || echo N").stdout.strip().endswith("N")   # 空值=不拦
    assert bash("mem_below abc 200 || echo N").stdout.strip().endswith("N")  # 非数值=不拦（读数异常不得拖死复测）


# ---------------- cleanup_orphans（不能真跑 pkill → 源码级断言）----------------

def test_orphan_cleanup_targets_own_user_only():
    # cleanup_orphans 的 pkill 必须限定 -u 当前用户（不能误杀他人 control 进程）
    src = open(COMMON).read()
    assert 'pkill -KILL -x control -u' in src
    assert "cleanup_orphans()" in src            # 且确实作为函数存在（调用点见下）
    assert "SDC_ORPHAN_PIDS" in src              # 测试沙盒分支必须存在（评审 round 2 红线）


def test_orphan_sandbox_spares_bystanders(tmp_path, orphan_sandbox):
    # 沙盒安全证明：SDC_ORPHAN_PIDS 设置 → cleanup_orphans 只杀注册 pid；
    # 未注册的同名 control 旁观者必须幸存——证明生产 pkill 路径未走（战役切片零影响）
    witness = _spawn_control(tmp_path, "witness")
    try:
        assert bash("cleanup_orphans").returncode == 0
        time.sleep(0.2)
        r = subprocess.run(["kill", "-0", str(orphan_sandbox)], capture_output=True)
        assert r.returncode != 0, "注册 pid 未被 cleanup_orphans 收掉——沙盒击杀失效"
        r = subprocess.run(["kill", "-0", str(witness)], capture_output=True)
        assert r.returncode == 0, "未注册 control 旁观者被杀——沙盒失效，生产 pkill 会误伤战役！"
    finally:
        subprocess.run(["kill", "-KILL", str(witness)], capture_output=True)


def test_cleanup_orphans_call_sites():
    # 三个调用点：handle_failure 复测前 / run_sdc 阶段超时 kill 路径 / EXIT trap 清理段
    lines = open(DRIVER).read().splitlines()
    idx = [i for i, l in enumerate(lines) if "cleanup_orphans" in l and "pkill" not in l]
    hf = next(i for i, l in enumerate(lines) if l.startswith("handle_failure()"))
    retest = next(i for i, l in enumerate(lines) if "for i in 1 2 3" in l)
    killed = next(i for i, l in enumerate(lines) if "被阶段边界终止" in l)
    cleanup_fn = next(i for i, l in enumerate(lines) if l.startswith("cleanup()"))
    trap = next(i for i, l in enumerate(lines) if l.startswith("trap cleanup EXIT"))
    assert any(hf < i < retest for i in idx), "复测前必须先清 control 孤儿"
    assert any(abs(i - killed) <= 2 for i in idx), "阶段兜底 kill 后必须收残"
    assert any(cleanup_fn < i < trap for i in idx), "EXIT trap 清理段必须兜底"


# ---------------- retest_guarded 组合逻辑（mock：_cur_memavail_kb 覆写 + 假 BIN）----------------

HEALTHY = 104857600   # 100 GB（kB）
LOW_GATE = 3145728    # 3 GB：低于 6GB 前置门
LOW_WATCH = 2097152   # 2 GB：低于 2.5GB 看门狗线


def test_retest_guarded_runs_when_mem_healthy(tmp_path):
    # 内存充足：直接运行，rc 透传，不写任何护栏行
    rtxt = tmp_path / "retests.txt"; rtxt.write_text("")
    binp = _fake_bin(tmp_path, "exit 7")
    p = bash(f'_cur_memavail_kb() {{ echo {HEALTHY}; }}\n'
             f'MEMGUARD_WAIT_S=0.2 MEMWATCH_POLL_S=0.2 '
             f'retest_guarded retest1 "{rtxt}" 30s "{binp}" -e x; echo rc=$?')
    assert "rc=7" in p.stdout, p.stdout + p.stderr
    assert "skipped" not in rtxt.read_text() and "killed" not in rtxt.read_text()


def test_retest_guarded_skips_when_mem_low(tmp_path):
    # 前置门：低内存 → 等 MEMGUARD_WAIT_S 重取仍低 → 记 skipped、返回 250、复测不得启动
    rtxt = tmp_path / "retests.txt"; rtxt.write_text("")
    marker = tmp_path / "ran"
    binp = _fake_bin(tmp_path, f"touch {marker}; exit 0")
    p = bash(f'_cur_memavail_kb() {{ echo {LOW_GATE}; }}\n'
             f'MEMGUARD_WAIT_S=0.2 retest_guarded retest1 "{rtxt}" 30s "{binp}"; echo rc=$?',
             timeout=20)
    assert "rc=250" in p.stdout, p.stdout + p.stderr
    assert f"retest1 skipped(memguard ma={LOW_GATE}kB)" in rtxt.read_text()
    assert not marker.exists(), "内存门拦截时复测进程不得启动"


def _seq_memavail_mock(tmp_path, values):
    # 逐次返回 values（超出序列钳位末值）。计数必须落文件：ma=$(_cur_memavail_kb) 是
    # 命令替换子 shell，shell 变量计数在两次调用间丢失（实测踩坑——永远只返回第一个值）。
    seqf = tmp_path / "memseq"
    seqf.write_text("\n".join(str(v) for v in values) + "\n")
    cnt = tmp_path / "calls"
    return ('_cur_memavail_kb() { '
            'n=$(cat "' + str(cnt) + '" 2>/dev/null || echo 0); n=$((n+1)); echo "$n" > "' + str(cnt) + '"; '
            'awk -v n="$n" \'NR<=n{v=$1} END{print v}\' "' + str(seqf) + '"; }')


def test_retest_guarded_waits_and_recovers(tmp_path):
    # 前置门的等待路径：先低后高（≤60s 内恢复）→ 照常运行，不记 skipped
    rtxt = tmp_path / "retests.txt"; rtxt.write_text("")
    binp = _fake_bin(tmp_path, "exit 0")
    p = bash(_seq_memavail_mock(tmp_path, [LOW_GATE, HEALTHY]) + "\n"
             f'MEMGUARD_WAIT_S=0.2 MEMWATCH_POLL_S=0.2 '
             f'retest_guarded retest1 "{rtxt}" 30s "{binp}"; echo rc=$?',
             timeout=20)
    assert "rc=0" in p.stdout, p.stdout + p.stderr
    assert "skipped" not in rtxt.read_text()


def test_retest_guarded_watchdog_kills_whole_tree(tmp_path, orphan_sandbox):
    # 看门狗：运行期跌破 2.5GB → KILL 复测进程组 + cleanup_orphans 收 control 切片。
    # 真实形态（2026-09-26 真机实测 pid/pgid 链）：timeout(pgid=自pid) → sdcshield
    # main(pgid=timeout pid，组杀可及) → control 切片(pgid=sess=自pid——
    # framework/sysdeps/unix/signals.cpp:63 signals_init_child setsid 自成会话，
    # 组杀够不着)。生产语义=击杀序列后 cleanup_orphans 以 pkill -x control 收切片；
    # 测试在 SDC_ORPHAN_PIDS 沙盒下验证该语义：注册 pid 被收（cleanup_orphans 已调用）、
    # 未注册的 setsid 孙进程幸存（既证组杀对 setsid 逃逸无效=cleanup_orphans 的存在
    # 理由，又证沙盒不伤旁观——生产 pkill 路径由源码断言+沙盒语义测试双层覆盖）。
    rtxt = tmp_path / "retests.txt"; rtxt.write_text("")
    sleeper = tmp_path / "sleeper.pid"
    gc = tmp_path / "gc.pid"
    ctrl = tmp_path / "control"      # comm=control：模拟框架切片的 prctl 改名
    shutil.copy("/bin/sleep", ctrl); ctrl.chmod(0o755)
    binp = _fake_bin(tmp_path, f"sleep 30 &\necho $! > {sleeper}\n"
                               f"setsid bash -c 'echo $$ > {gc}; exec {ctrl} 300' >/dev/null 2>&1 &\nwait")
    p = bash(_seq_memavail_mock(tmp_path, [HEALTHY, LOW_WATCH]) + "\n"
             f'MEMGUARD_WAIT_S=0.2 MEMWATCH_POLL_S=0.3 '
             f'retest_guarded retest1 "{rtxt}" 60s "{binp}"; echo rc=$?',
             timeout=30)
    assert "rc=137" in p.stdout, p.stdout + p.stderr
    assert f"retest1 killed(memwatch ma={LOW_WATCH}kB)" in rtxt.read_text()
    time.sleep(0.3)   # KILL 投递到生效的微小时滞
    spid = int(sleeper.read_text().strip())
    gpid = int(gc.read_text().strip())
    try:
        r = subprocess.run(["kill", "-0", str(spid)], capture_output=True)
        assert r.returncode != 0, f"看门狗后 sleeper(pid={spid}) 仍存活——组杀未生效"
        # setsid 孙进程（切片形态，未注册）：组杀够不着 + 沙盒不杀旁观 → 必须幸存
        r = subprocess.run(["kill", "-0", str(gpid)], capture_output=True)
        assert r.returncode == 0, f"未注册 setsid 孙进程(pid={gpid}) 被杀——沙盒泄漏或组杀越界"
        # 注册目标（切片替身）：cleanup_orphans 在击杀序列中被调用并收掉
        r = subprocess.run(["kill", "-0", str(orphan_sandbox)], capture_output=True)
        assert r.returncode != 0, "注册 control pid 未被收——cleanup_orphans 未在击杀序列调用"
    finally:
        subprocess.run(["kill", "-KILL", str(spid), str(gpid)], capture_output=True)  # 测试自清


def test_retest_guarded_no_false_positive_when_exits_during_judge(tmp_path):
    # 竞态消除（评审 Minor 1）：ma 读取期间复测进程自然退出 → 击杀扑空，
    # 不得记 killed(memwatch)（假阳性——它并非死于看门狗）。
    # 构造：mock 第 2 次调用（看门狗判定取数时）先 KILL fakebin、留 0.6s 让
    # timeout 退出并被 bash 收尸（实证：前台命令等待期间 bash 收尸后台作业），
    # 再返回低内存值——此时击杀扑空（pid 已收尸），修复前会误记 killed。
    rtxt = tmp_path / "retests.txt"; rtxt.write_text("")
    fbp = tmp_path / "fb.pid"
    binp = _fake_bin(tmp_path, f"echo $$ > {fbp}\nsleep 60")
    cnt = tmp_path / "calls"
    mock = ('_cur_memavail_kb() { n=$(cat "' + str(cnt) + '" 2>/dev/null || echo 0); '
            'n=$((n+1)); echo "$n" > "' + str(cnt) + '"; '
            'if [ "$n" -ge 2 ]; then kill -KILL "$(cat ' + str(fbp) + ')" 2>/dev/null; '
            'sleep 0.6; echo ' + str(LOW_WATCH) + '; '
            'else echo ' + str(HEALTHY) + '; fi; }')
    p = bash(mock + "\n"
             f'MEMGUARD_WAIT_S=0.2 MEMWATCH_POLL_S=0.3 '
             f'retest_guarded retest1 "{rtxt}" 60s "{binp}"; echo rc=$?',
             timeout=30)
    assert "rc=137" in p.stdout, p.stdout + p.stderr   # fakebin 被测试所杀 → timeout 汇报 137
    assert "killed" not in rtxt.read_text(), "自然退出被误记 killed(memwatch)——假阳性"


# ---------------- 驱动接线与边界守护 ----------------

def test_retest_guarded_wired_in_handle_failure():
    src = open(DRIVER).read()
    # 两处复测循环都必须走 retest_guarded（定向 + 整命令回退）
    assert src.count('retest_guarded "retest$i"') == 2
    # 旧的裸 timeout 复测行已灭（包装层移入 retest_guarded）
    assert "timeout --signal=TERM --kill-after=60s 150s" not in src
    assert "timeout --signal=TERM --kill-after=60s 900s" not in src
    # 250=内存门拦截 → continue（跳过该轮，不补写 rc 行——skipped 行由护栏记）
    assert '[ "$rcf" -eq 250 ] && continue' in src


def test_no_mem_checks_in_main_phase_paths():
    # 自审守护（v5 §8.6）：主阶段路径零内存检查——mem_below/_cur_memavail_kb/retest_guarded
    # 不得出现在驱动主阶段代码；retest_guarded 的两处调用必须都在 handle_failure 函数体内
    lines = open(DRIVER).read().splitlines()
    assert not any("mem_below" in l or "_cur_memavail_kb" in l for l in lines)
    hf = next(i for i, l in enumerate(lines) if l.startswith("handle_failure()"))
    end = next(i for i, l in enumerate(lines) if "stress-ng 补充层" in l)
    calls = [i for i, l in enumerate(lines) if "retest_guarded" in l]
    assert calls and all(hf < i < end for i in calls), "retest_guarded 只允许在 handle_failure 内"

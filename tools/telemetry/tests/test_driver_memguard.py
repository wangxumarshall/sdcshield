import os, shlex, subprocess

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")
DRIVER = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc-excite-reproduce.sh")

# Task 5：复测内存护栏 + control 孤儿清理。
# 背景（RCA 附3）：cold_c4 事件的 wholecmd 重放（900s×3）在内存紧张期把机器推进 OOM，
# 且泄漏的复测命令留下 comm=control 的孤儿切片（framework/sandstone_run.cpp:1242
# prctl 改名——pkill -x sdcshield 够不着）。
# 护栏只针对复测（v5 §8.6 分工：主负载的内存防线是 monitor 联锁）。


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


def test_retest_guarded_watchdog_kills_whole_tree(tmp_path):
    # 看门狗：运行期跌破 2.5GB → KILL 复测进程组（含 timeout 的子树，不留 control 孤儿）
    # 2026-09-26 实证：本机 coreutils timeout 自成 pgid（整树 pgid=timeout pid），
    # kill -KILL $rpid + kill -KILL -- -$rpid 一击整树；只杀 timeout 会孤儿化命令树。
    rtxt = tmp_path / "retests.txt"; rtxt.write_text("")
    sleeper = tmp_path / "sleeper.pid"
    binp = _fake_bin(tmp_path, f"sleep 30 &\necho $! > {sleeper}\nwait")
    p = bash(_seq_memavail_mock(tmp_path, [HEALTHY, LOW_WATCH]) + "\n"
             f'MEMGUARD_WAIT_S=0.2 MEMWATCH_POLL_S=0.3 '
             f'retest_guarded retest1 "{rtxt}" 60s "{binp}"; echo rc=$?',
             timeout=30)
    assert "rc=137" in p.stdout, p.stdout + p.stderr
    assert f"retest1 killed(memwatch ma={LOW_WATCH}kB)" in rtxt.read_text()
    spid = int(sleeper.read_text().strip())
    try:
        r = subprocess.run(["kill", "-0", str(spid)], capture_output=True)
        assert r.returncode != 0, f"看门狗后 sleeper(pid={spid}) 仍存活——孤儿未清"
    finally:
        subprocess.run(["kill", "-KILL", str(spid)], capture_output=True)  # 测试自清


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

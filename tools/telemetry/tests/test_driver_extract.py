import os, shlex, subprocess

COMMON = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc_common.sh")
DRIVER = os.path.join(os.path.dirname(__file__), "..", "..", "..",
                      "scripts", "sdc-excite-reproduce", "sdc-excite-reproduce.sh")

# 真实形态：2026-09-25 cold_c4 事件 stdout_summary.out
# （~/sdc-excite-reproduce/events/20260925-082233-cold_c4-rc137/，逐字摘录其头部）
# RCA 附3：该事件 result 为 crash（非 fail）→ 旧提取只认 'result: *fail' → 提取为空
# → handle_failure 落入 wholecmd 重放（900s×3，重放泄漏命令），而非 120s 定向复测。
CRASH_OUT = """command-line: 'sdcshield -T 1800s -t 60s --strict-runtime -e memcpy_rewr,cachebounce,... -o /home/sdc/sdc-campaign/logs/20260925/082136-cold_c4.yaml'
version: sdcshield-6c76ea63a7a9-dirty
tests:
- test: memcpy_rewr
  result: crash
  result-details: { crashed: true, core-dump: false, code: 9, reason: 'Killed' }
  fail: { cpu-mask: null, time-to-fail: null, seed: 'AES:87cbf86cb7f2e560cdde91853478f78278340793480d1a9f32216e7acb87087d' }
  threads:
  - thread: main
    runtime: 325.767
    resource-usage: { utime: 5596.368, stime: 1228.443, cpuavg: 2095.0, maxrss: 1248684, majflt: 0, minflt: 230864 }
- test: cachebounce
  result: crash
  result-details: { crashed: true, core-dump: false, code: 9, reason: 'Killed' }
  fail: { cpu-mask: null, time-to-fail: null, seed: 'AES:be916f056e8280c64c87439b8f7f1b8b78340793480d1a9f32216e7acb87087d' }
  threads:
  - thread: main
    runtime: 87.842
"""

PASS_OUT = """tests:
- test: zstd19
  result: pass
  threads:
  - thread: main
    runtime: 10184.000
"""


def _bash(body):
    return subprocess.run(["bash", "-c", f"source '{COMMON}'\n{body}"],
                          capture_output=True, text=True, timeout=15)


def test_crash_test_extracted(tmp_path):
    # 修复判据（RCA 附3）：crash 事件的测试名与 AES 种子都必须进入定向复测路径
    out = tmp_path / "cold_c4.out"
    out.write_text(CRASH_OUT)

    t = _bash(f'extract_failed_test "{out}"').stdout.strip()
    assert t == "memcpy_rewr"                     # 首事件，非 cachebounce

    seed = _bash(f'extract_fail_seed "{out}"').stdout.strip()
    assert seed == "AES:87cbf86cb7f2e560cdde91853478f78278340793480d1a9f32216e7acb87087d"

    # pass 文件不产出事件（不伪造）
    p = tmp_path / "pass.out"
    p.write_text(PASS_OUT)
    assert _bash(f'extract_failed_test "{p}"').stdout.strip() == ""

    # 驱动侧接线：handle_failure 必须走上面两个函数，且不得残留只认 fail 的提取器
    # （否则测试绿而驱动仍坏——RCA 根因正是驱动内联正则只匹配 fail）
    src = open(DRIVER).read()
    assert 'extract_failed_test "$outsum"' in src
    assert 'extract_fail_seed "$outsum"' in src
    assert "/^  result: *fail/{print t; exit}" not in src          # 旧内联 awk 提取器已删
    assert "grep -m1 -B3 'result: *fail'" not in src               # 旧 grep 回退已删


def _run_src_cmd(cmd, fixture, var):
    # 从驱动源码逐字提取的命令/管道对 fixture 实跑——防"正则改了但漏 -E"这类
    # 源码级失效（BRE 下 '(fail|crash)' 是字面量，fail/crash 全匹配不到）
    r = subprocess.run(["bash", "-c", f"{var}={shlex.quote(str(fixture))}; {cmd} && echo HIT"],
                       capture_output=True, text=True, timeout=15)
    return r.stdout


def test_spectrum_sweep_detection_accepts_crash(tmp_path):
    # :254 同类修复（Task 4 同模式）：phase_l2 谱系扫档的失败检测只认 fail →
    # spectrum 产物的 crash 事件漏进取证流水线
    src = open(DRIVER).read()
    line = next(l.strip() for l in src.splitlines()
                if "grep" in l and "result:" in l and '"$f"' in l)
    cmd = line[len("if "):line.index("; then")]
    crash = tmp_path / "spec_crash.yaml"; crash.write_text("tests:\n- test: x\n  result: crash\n")
    ok = tmp_path / "spec_ok.yaml"; ok.write_text("tests:\n- test: x\n  result: pass\n")
    assert "HIT" in _run_src_cmd(cmd, crash, "f"), f"crash 行未触发谱系检测: {line}"
    assert "HIT" not in _run_src_cmd(cmd, ok, "f")


def test_yaml_fallback_extraction_accepts_crash(tmp_path):
    # :146 修复（Task 4 遗留，2026-09-26 实证发现）：回退提取的正则加了 (fail|crash)
    # 却漏 -E——BRE 下为字面量，fail/crash 都提取不到（.out 摘要缺失时回退路径静默失效）
    src = open(DRIVER).read()
    line = next(l.strip() for l in src.splitlines() if "grep -m1 -B3" in l)
    pipe = line[line.index("LC_ALL=C grep"):line.rindex(")")]
    y = tmp_path / "ev.yaml"
    y.write_text("- test: memcpy_rewr\n  result: crash\n  result-details: {code: 9}\n")
    r = subprocess.run(["bash", "-c",
                        f'yaml={shlex.quote(str(y))}; out=$({pipe}); echo "[$out]"'],
                       capture_output=True, text=True, timeout=15)
    assert "[memcpy_rewr]" in r.stdout, f"crash 事件回退提取失败: {line}"

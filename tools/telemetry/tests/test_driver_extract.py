import os, subprocess

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

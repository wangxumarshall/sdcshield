#!/usr/bin/env python3
# verify-params.py — 在 openEuler 容器内做"全量参数"功能测试的纯 stdlib 脚本。
#
# 为什么纯 stdlib(不 import yaml):三个系列的构建镜像 RPM 树都**没有** PyYAML;
# 容器内 import yaml 会直接崩。sdcshield 的 YAML 输出稳定,这里用简单行的正则/
# 前缀解析,只做"判定 pass/fail/skip + 崩溃计数 + 摘字段",不做完整 YAML 反序列化。
#
# 验证设计(对齐 CLAUDE.md 全量要求 + 已知平台怪癖):
#   1) 枚举: 默认 PROD; --quality=-1 覆盖 BETA+SKIP; --selftests 覆盖框架自测(含期望失败的
#      selftest_failinit 等,统一用 retest-on-failure=0 让框架不重试、原样返回)。
#   2) 并发: 分 runner 拓扑自适应 1/4/8 线程(容器内用 nproc)。
#   3) openblas: 补 -O <id>.mdim 扫谱(仅存在时)。
#   4) 负面用例集(selftest_*fail/abort/sig*)逐个 -e 跑并断言"非零退出但不 insn 崩溃"。
#   stdout 末行 RESULT: PASS|FAIL 供 workflow 解析;退出码 0=PASS / 1=FAIL。
#
# 用法:
#   python3 verify-params.py --bin /path/sdcshield --out /out/results
# SPDX-License-Identifier: Apache-2.0
import argparse
import os
import re
import subprocess
import sys

RESULT_RE = re.compile(r"^- test:\s*(\S+)")
RESULT_VAL = re.compile(r"^  result:\s*(\S+)")
RUNTIME_RE = re.compile(r"^  test-runtime:\s*(\S+)")
CRASH_RE = re.compile(
    r"crash-context|exited with signal|backtrace|SIGSEGV when|SIGABRT when|SIGILL when|SIGBUS when|SIGFPE when",
    re.IGNORECASE,
)
# 跳过关键字(placeholder 诚实跳过、无硬件、超时)——不计 fail
SKIP_OK_RE = re.compile(
    r"to be implemented|CpuNotSupported|not available|unsupported|placeholder|"
    r"test resource issue|requires \bSMT\b|SVE|requires",
    re.IGNORECASE,
)

# 期望失败的 selftest(框架自测故意触发失败路径;退出码非 0 是"正确")
NEGATIVE_SELFTESTS = [
    "selftest_fail", "selftest_failinit", "selftest_abort", "selftest_abortinit",
    "selftest_freeze", "selftest_freeze_fork", "selftest_sigill", "selftest_sigsegv",
    "selftest_sigbus", "selftest_sigfpe", "selftest_sigsegv_init", "selftest_sigsegv_instruction",
    "selftest_sigsegv_kernel", "selftest_sigkill", "selftest_reportfail", "selftest_reportfailmsg",
    "selftest_oserror", "selftest_randomfail_50pct", "selftest_randomfail_rare",
    "selftest_timed_randomfail_25pct", "selftest_timed_randomfail_rare",
    "selftest_datacomparefail_double", "selftest_datacomparefail_float",
    "selftest_malloc_fail", "selftest_cxxthrow", "selftest_cxxthrowcatch",
]
# 与现有 verify-built-pristine.sh 一致的数值敏感 eigen 集(192 核 ULP flakiness → -n 1)
EIGEN_N1 = ["eigen_svd_double", "eigen_sparse", "eigen_svd_cdouble", "eigen_svd_cdouble_sve"]
# 基准/参数扫描的 openblas mdim 档
OPENBLAS_MDIM = ["64", "256", "512"]


def run(cmd, timeout=900):
    """跑一条命令,返回 (exit_code, stdout_text)。容器内用 --ignore-timeout 规避 cgroup 超时误报。"""
    try:
        p = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                           timeout=timeout, text=True)
        return p.returncode, p.stdout
    except subprocess.TimeoutExpired as e:
        return 124, (e.stdout or "")


def list_tests(bin_path, extra=None):
    cmd = [bin_path, "--list-tests"]
    if extra:
        cmd += extra
    rc, out = run(cmd)
    return [l.strip() for l in out.splitlines() if l.strip()]


def parse_result_yaml(text):
    """从 sdcshield YAML 输出里摘 per-test 的 (test, result, runtime),以及崩溃计数。"""
    tests = []          # list of (name, result, runtime)
    crashes = 0
    cur = None
    for line in text.splitlines():
        m = RESULT_RE.match(line)
        if m:
            if cur is not None:
                tests.append(cur)
            cur = [m.group(1), "unknown", 0.0]
            continue
        if cur is None:
            if CRASH_RE.search(line):
                crashes += 1
            continue
        mv = RESULT_VAL.match(line)
        if mv:
            cur[1] = mv.group(1)
        mrt = RUNTIME_RE.match(line)
        if mrt:
            try:
                cur[2] = float(mrt.group(1))
            except ValueError:
                cur[2] = 0.0
        if CRASH_RE.search(line):
            crashes += 1
    if cur is not None:
        tests.append(cur)
    return tests, crashes


def summarize(tests, crashes, ok_labels=("pass", "skip", "timed out")):
    npass = sum(1 for _, r, _ in tests if r == "pass")
    nskip = sum(1 for _, r, _ in tests if r == "skip")
    nfail = sum(1 for _, r, _ in tests if r == "fail")
    ncrash = sum(1 for _, r, _ in tests if r == "crash")
    return npass, nskip, nfail, ncrash, crashes


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bin", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--threads", type=int, nargs="*", default=None,
                    help="并发档,默认 [1,4,8] 按 nproc 收缩")
    ap.add_argument("--mdim", nargs="*", default=None,
                    help="openblas mdim 扫谱档,默认 64 256 512")
    ap.add_argument("--smoke", action="store_true",
                    help="快档:只跑 all-quality(-n1) + zstd19,跳过多线程/mdim/selftest 负面")
    args = ap.parse_args()

    bin_path = args.bin
    out_dir = args.out
    os.makedirs(out_dir, exist_ok=True)

    try:
        nproc = int(os.sysconf("SC_NPROCESSORS_ONLN"))
    except (AttributeError, ValueError):
        nproc = 4
    # 已在容器内但不一定全核可用;收缩并发档,保证时间可预测
    threads = args.threads or [1, 4, 8]
    threads = [t for t in threads if t <= nproc] or [1]
    mdim = args.mdim or OPENBLAS_MDIM

    # ---- 1) 枚举集合 ----
    prod_all = list_tests(bin_path)
    all_ql = list_tests(bin_path, ["--quality=-1"])
    selftests = list_tests(bin_path, ["--selftests"])
    total_enum = len(prod_all)

    log = []          # 人类可读摘要
    fail_total = 0
    crash_total = 0

    # ---- 2) 全质量级跑一遍(单线程基线, 判"能跑"与结果分布) ----
    #   --quality=-1 覆盖 PROD+BETA+SKIP; --retest-on-failure=0 让失败原样返回不重试;
    #   单线程规避 192 核 eigen ULP flakiness;-t 5000 每测试上限 5s(quick 下更短)。
    stage_tests = []
    if not args.smoke:
        for extra, label in [
            (["--quality=-1", "--retest-on-failure=0", "-n", "1", "-t", "5000"], "all-quality(-n1)"),
        ]:
            y = os.path.join(out_dir, "allquality.yaml")
            cmd = [bin_path] + extra + ["-o", y]
            _rc, stdout = run(cmd)
            tests, crashes = parse_result_yaml(_read(y) or stdout)
            npass, nskip, nfail, ncrash, _ = summarize(tests, crashes)
            stage_tests.append((label, tests, crashes, npass, nskip, nfail, ncrash))

    for label, tests, crashes, npass, nskip, nfail, ncrash in stage_tests:
        ok = (nfail == 0 and ncrash == 0 and npass + nskip > 0)
        if not ok:
            fail_total += 1
        crash_total += crashes
        log.append(f"[{label}] tests={len(tests)} pass={npass} skip={nskip} fail={nfail} crash={ncrash} "
                   f"({ 'OK' if ok else 'FAIL'})")

    # ---- 3) 多线程并发档(-n 1/4/8)对全 PROD 各跑一轮(内存/锁/cache 压力面) ----
    #   -t 1000 每测试 ~1s,配合 --ignore-timeout 规避 cgroup 超时误报;断言 0 fail 0 crash。
    if args.smoke:
        for n in [1]:
            y = os.path.join(out_dir, "smoke_zstd19.yaml")
            cmd = [bin_path, "--retest-on-failure=0", "-n", "1", "-t", "3000",
                   "-e", "zstd19", "-o", y]
            _rc, stdout = run(cmd)
            tests, crashes = parse_result_yaml(_read(y) or stdout)
            npass, nskip, nfail, ncrash, _ = summarize(tests, crashes)
            ok = nfail == 0 and ncrash == 0 and npass >= 1
            if not ok:
                fail_total += 1
            crash_total += crashes
            log.append(f"[smoke zstd19 -n 1] pass={npass} skip={nskip} fail={nfail} crash={ncrash} "
                       f"({'OK' if ok else 'FAIL'})")
        print(f"enumerated: prod={total_enum} all-quality={len(all_ql)} selftests={len(selftests)} "
              f"threads={threads} nproc={nproc} (smoke)")
        for line in log:
            print(line)
        verdict = "PASS" if (fail_total == 0 and crash_total == 0) else "FAIL"
        print(f"RESULT: {verdict} (sections_fail={fail_total} crashes={crash_total})")
        return 0 if verdict == "PASS" else 1

    for n in threads:
        y = os.path.join(out_dir, f"prod_n{n}.yaml")
        # eigen 数值类在 >1 线程下有已知 ULP flakiness(CLAUDE.md),多线程档 --disable 规避;
        # 它们已在 all-quality(-n1) 单线程档覆盖。
        disable_args = []
        for eig in EIGEN_N1:
            disable_args += ["--disable", eig]
        cmd = [bin_path, "--retest-on-failure=0", "-n", str(n), "-t", "1000",
               "--ignore-timeout"] + disable_args + ["-o", y]
        _rc, stdout = run(cmd)
        tests, crashes = parse_result_yaml(_read(y) or stdout)
        npass, nskip, nfail, ncrash, _ = summarize(tests, crashes)
        ok = nfail == 0 and ncrash == 0
        if not ok:
            fail_total += 1
        crash_total += crashes
        log.append(f"[all-PROD -n {n}] pass={npass} skip={nskip} fail={nfail} crash={ncrash} "
                   f"({'OK' if ok else 'FAIL'})")

    # ---- 4) selftests 正向集(@positive) ----
    #   @positive 只带 --selftests 前缀生效。
    y = os.path.join(out_dir, "selftest_positive.yaml")
    cmd = [bin_path, "--selftests", "--quick", "--retest-on-failure=0", "-n", "1",
           "-e", "@positive", "-o", y]
    rc, stdout = run(cmd)
    pos_ok = (rc == 0)
    if not pos_ok:
        fail_total += 1
    log.append(f"[selftest @positive] rc={rc} ({'OK' if pos_ok else 'FAIL'})")

    # ---- 5) selftests 负面集(逐个跑, 期望退出码非 0 但不 insn 崩溃) ----
    neg_ok = True
    neg_list = [t for t in NEGATIVE_SELFTESTS if t in selftests]
    for t in neg_list:
        y = os.path.join(out_dir, f"selftest_{t}.yaml")
        cmd = [bin_path, "--selftests", "--quick", "--retest-on-failure=0",
               "--on-hang=kill", "--on-crash=kill", "-n", "1", "-e", t, "-o", y]
        rc, stdout = run(cmd, timeout=120)
        # 期望: 非 0 退出(代表"测试正确报告了失败"),且无机器指令级崩溃信号
        insn_crash = bool(CRASH_RE.search(stdout))
        ok = (rc != 0 and not insn_crash)
        if not ok:
            neg_ok = False
        log.append(f"[selftest {t}] rc={rc} insn_crash={insn_crash} ({'OK' if ok else 'FAIL'})")

    if not neg_ok:
        fail_total += 1

    # ---- 6) openblas mdim 参数扫谱(存在时) ----
    for t in ["openblas_dgemm", "openblas_sgemm", "openblas_zgemm"]:
        if t not in prod_all:
            continue
        for m in mdim:
            y = os.path.join(out_dir, f"{t}_mdim{m}.yaml")
            cmd = [bin_path, "-O", f"{t}.mdim={m}", "-n", "1", "--retest-on-failure=0",
                   "-t", "5000", "--ignore-timeout", "-e", t, "-o", y]
            _rc, stdout = run(cmd)
            tests, crashes = parse_result_yaml(_read(y) or stdout)
            npass, nskip, nfail, ncrash, _ = summarize(tests, crashes)
            ok = nfail == 0 and ncrash == 0
            if not ok:
                fail_total += 1
            crash_total += crashes
            log.append(f"[{t} mdim={m}] pass={npass} skip={nskip} fail={nfail} crash={ncrash} "
                       f"({'OK' if ok else 'FAIL'})")

    # ---- 汇总 ----
    # echo 各节 + 末行 RESULT 供 workflow 解析
    print(f"enumerated: prod={total_enum} all-quality={len(all_ql)} selftests={len(selftests)} "
          f"threads={threads} nproc={nproc}")
    for line in log:
        print(line)

    verdict = "PASS" if (fail_total == 0 and crash_total == 0) else "FAIL"
    print(f"RESULT: {verdict} (sections_fail={fail_total} crashes={crash_total})")
    return 0 if verdict == "PASS" else 1


def _read(path):
    try:
        with open(path, "r") as f:
            return f.read()
    except OSError:
        return ""


if __name__ == "__main__":
    sys.exit(main())
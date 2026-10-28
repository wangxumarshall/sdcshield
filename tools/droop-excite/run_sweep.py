#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""droop 激发战役驱动（0103 专用；root 运行——ipmitool/SCHED_FIFO 需要）。

Spec: docs/superpowers/specs/2026-10-27-droop-resonance-excitation-design.md
与 sdcshield 解耦：仅经 CLI 调用 builddir/sdcshield 二进制（--repo 指定）；
无 sdcshield 时纯调制步骤仍可运行。

用法（阶段编排见文末 main）:
    sudo python3 run_sweep.py --phase p0 --data-root /home/sdc/droop-excite
    python3  run_sweep.py --phase p2 --dry-run
"""
import csv
import glob
import json
import os
import re
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))

# ---- 0103 拓扑（2026-10-27 实测固化；assert_topology 运行时校验，漂移即拒绝） ----
NCPUS = 128
RESERVED_CPUS = list(range(120, 128))       # OS/驱动/记录器/会话
SOCKET_A_CPUS = list(range(0, 64))          # physical_package_id=36
SOCKET_B_CPUS = list(range(64, 128))        # physical_package_id=8442

DEFAULT_THRESHOLDS = {
    "cpu_temp_abort": 95.0,        # ℃（BMC 临界 105）
    "nvddavs_min": 0.80,           # V（BMC 下限 0.770）
    "psu_total_w_abort": 1300.0,   # W（P0 实测满载后收紧，写 thresholds.json）
}

EIGEN_TESTS_DEFAULT = ("eigen_svd_double,eigen_sparse,"
                       "eigen_gemm_double_dynamic_square")
EIGEN_TESTS_FULL = ("eigen_svd_double,eigen_svd_double2,eigen_sparse,"
                    "eigen_gemm_double_dynamic_square,"
                    "eigen_gemm_cdouble_dynamic_square,"
                    "eigen_gemm_float_dynamic_square,eigen_gemm_double14,"
                    "eigen_svd,eigen_svd_cdouble,eigen_svd_cdouble_noavx512,"
                    "eigen_svd_fvectors")


def probe_topology(sysfs_root="/sys"):
    """读 sysfs：{"ncores":N,"sockets":{pkg_id:[cpus]},"policies":{name:[cpus]}}。"""
    pat = re.compile(r"cpu(\d+)$")
    sockets, cpus = {}, []
    for d in sorted(glob.glob(os.path.join(sysfs_root,
                                           "devices/system/cpu/cpu[0-9]*")),
                    key=lambda p: int(pat.search(p).group(1))):
        c = int(pat.search(d).group(1))
        try:
            with open(os.path.join(d, "topology/physical_package_id")) as f:
                pkg = f.read().strip()
        except OSError:
            pkg = "?"
        sockets.setdefault(pkg, []).append(c)
        cpus.append(c)
    policies = {}
    for d in sorted(glob.glob(os.path.join(sysfs_root,
                                           "devices/system/cpu/cpufreq/policy*"))):
        try:
            with open(os.path.join(d, "related_cpus")) as f:
                policies[os.path.basename(d)] = [int(x) for x in f.read().split()]
        except OSError:
            pass
    return {"ncores": len(cpus), "sockets": sockets, "policies": policies}


def assert_topology(sysfs_root="/sys"):
    topo = probe_topology(sysfs_root)
    if topo["ncores"] != NCPUS or len(topo["sockets"]) != 2:
        raise RuntimeError(f"0103 拓扑漂移，拒绝运行: {topo}")
    vals = {frozenset(v) for v in topo["sockets"].values()}
    want = {frozenset(SOCKET_A_CPUS), frozenset(SOCKET_B_CPUS)}
    if vals != want:
        raise RuntimeError(f"0103 socket 划分与固化值不符: {topo}")
    return topo


def mod_cores(m, placement):
    """调制器核心列表。same=socketA 头部；cross=两 socket 各半。"""
    if placement == "same":
        return SOCKET_A_CPUS[:m]
    if placement == "cross":
        return SOCKET_A_CPUS[: m // 2] + SOCKET_B_CPUS[: m - m // 2]
    raise ValueError(placement)


def eigen_cores(mod):
    used = set(RESERVED_CPUS) | set(mod)
    return [c for c in range(NCPUS) if c not in used]


def fmt_cpulist(cs):
    return ",".join(str(c) for c in sorted(cs))


def build_mod_cmd(step, paths):
    """step["mod"]: "droop_mod" 或 "power_virus_dit"（P0 零代码冒烟用现成测试）。"""
    if step.get("mod") is None:
        return None
    mc = fmt_cpulist(step["mod_cores"])
    if step["mod"] == "power_virus_dit":
        return ["taskset", "-c", mc, paths["sdcshield"],
                "-e", "power_virus_dit", "-t", f"{step['duration']}s",
                "--strict-runtime"]
    # SDD 裁定 21：--cores 必须显式传给 droop_mod——它启动时会用自己的
    # 默认亲和（全部在线核）sched_setaffinity，覆盖外部 taskset 限制
    # （P1 实测 128 线程铺满全核致 RT 节流、遥测致盲）。taskset 保留作冗余。
    cmd = ["taskset", "-c", mc, paths["droop_mod"], "--cores", mc]
    if step.get("on_iters") is not None:
        cmd += ["--on-iters", str(step["on_iters"]),
                "--off-iters", str(step["off_iters"])]
    else:
        cmd += ["--freq-hz", repr(step["freq_hz"]),
                "--duty", repr(step["duty"])]
    cmd += ["--payload", step.get("payload", "dep"),
            "--off-mode", step.get("off_mode", "stall"),
            "--duration", str(step["duration"]),
            "--resync", str(step.get("resync", 1024)),
            "--json-summary", os.path.join(step["dir"], "mod_summary.json")]
    return cmd


def build_eigen_cmd(step, paths):
    if not step.get("eigen_cores"):
        return None
    return ["taskset", "-c", fmt_cpulist(step["eigen_cores"]), paths["sdcshield"],
            "-e", step.get("eigen_tests", EIGEN_TESTS_DEFAULT),
            "-t", "60s", "-T", f"{step['duration']}s", "--strict-runtime"]


def evaluate_guardrails(sample, thresholds):
    """单 BMC 样本 → None(安全) 或 停机原因字符串。"""
    def num(key):
        try:
            return float(sample.get(key, "") or "")
        except ValueError:
            return None
    for key in ("CPU1 Core Rem", "CPU2 Core Rem"):
        v = num(key)
        if v is not None and v >= thresholds["cpu_temp_abort"]:
            return f"guardrail: {key}={v:.1f}C >= {thresholds['cpu_temp_abort']}C"
    for key in ("CPU1 N_VDDAVS", "CPU2 N_VDDAVS"):
        v = num(key)
        if v is not None and v <= thresholds["nvddavs_min"]:
            return f"guardrail: {key}={v:.3f}V <= {thresholds['nvddavs_min']}V"
    p1, p2 = num("Power"), num("Power2")
    if p1 is not None and p2 is not None \
            and p1 + p2 >= thresholds["psu_total_w_abort"]:
        return f"guardrail: PSU={p1 + p2:.0f}W >= {thresholds['psu_total_w_abort']}W"
    return None


def last_csv_row(path):
    try:
        with open(path) as f:
            rows = list(csv.DictReader(f))
        return rows[-1] if rows else None
    except OSError:
        return None


def sel_count():
    r = subprocess.run(["ipmitool", "sel", "list"], capture_output=True,
                       text=True)
    return len(r.stdout.splitlines())


def ras_error_count(text):
    return len(re.findall(r"Machine check|EDAC|Hardware Error|mce:", text, re.I))


def dmesg_text():
    r = subprocess.run(["dmesg"], capture_output=True, text=True)
    return r.stdout


def refine_iters(step, paths):
    """闭环频率修正：mod-only 探测后按实测频率缩放 iters。
    覆盖规则（SDD 裁定 9）：探测需 ≥2 个门控周期才能测频，探测时长上限
    30s——覆盖 P1 全部低频点（0.1Hz→20s 探测）；更低的频率直接跑，实测
    频率由摘要如实报告。动因：T2 修正轮 2 发现代码布局会使 stall 的
    独立校准（2.0 cyc/iter）偏离门内实际（~1.0），低频直射的 off 相位
    因此减半、duty 轴失真——闭环探测度量的是门内真实节拍，不受布局影响。
    探测失败（无法执行/摘要缺失或损坏）→ 写 probe/probe_failed.txt 后
    放弃修正，按 freq 直射（实测频率由最终摘要如实报告）。"""
    if step.get("on_iters") is not None or not step.get("freq_hz"):
        return
    two_periods = 2.0 / step["freq_hz"]
    if two_periods > 30.0:
        return
    probe = dict(step)
    probe["duration"] = max(3.0, two_periods)
    probe["dir"] = os.path.join(step["dir"], "probe")
    os.makedirs(probe["dir"], exist_ok=True)
    try:
        cmd = build_mod_cmd(probe, paths)
        with open(os.path.join(probe["dir"], "mod.log"), "w") as lf:
            subprocess.run(cmd, stdout=lf, stderr=subprocess.STDOUT, check=False)
        with open(os.path.join(probe["dir"], "mod_summary.json")) as f:
            s = json.load(f)
        fm = s["freq_measured_hz_mean"]
        scale = fm / step["freq_hz"]          # 实测偏低 → iters 缩小
        on = max(1, int(s["on_iters"] * scale))
        off = max(0, int(s["off_iters"] * scale))
    except (OSError, json.JSONDecodeError, KeyError) as e:
        with open(os.path.join(probe["dir"], "probe_failed.txt"), "w") as f:
            f.write(f"{type(e).__name__}: {e}\n")
        return                              # 放弃修正：freq 直射
    step["on_iters"], step["off_iters"] = on, off


def run_step(step, paths, thresholds, pre_secs=15.0, post_secs=15.0,
             poll_interval=3.0):
    """执行一个战役步骤。pre 15s 空载采样 → dwell（eigen+mod）→ post 15s →
    summary.json 落盘。返回 step dict。异常路径由 finally 兜底终止
    bmc_logger（root 记录器不遗留）。"""
    t = {"pre_start": time.time()}
    step["dir"] = os.path.join(paths["run_dir"], "steps", step["id"])
    os.makedirs(step["dir"], exist_ok=True)
    bmc_csv = os.path.join(step["dir"], "bmc.csv")
    sel0, dm0 = sel_count(), dmesg_text()
    logger = subprocess.Popen(
        [sys.executable, os.path.join(HERE, "bmc_logger.py"),
         "--out", bmc_csv, "--interval", "3"],
        stdout=subprocess.DEVNULL)
    try:
        refine_iters(step, paths)               # 高频：mod-only 探测修正（3s）
        time.sleep(pre_secs)
        t["dwell_start"] = time.time()
        mod_cmd = build_mod_cmd(step, paths)
        eig_cmd = build_eigen_cmd(step, paths)
        with open(os.path.join(step["dir"], "commands.json"), "w") as f:
            json.dump({"mod": mod_cmd, "eigen": eig_cmd}, f, indent=1)
        procs = {}
        if eig_cmd:
            procs["eigen"] = subprocess.Popen(
                eig_cmd,
                stdout=open(os.path.join(step["dir"], "eigen.log"), "w"),
                stderr=subprocess.STDOUT)
        if mod_cmd:
            procs["mod"] = subprocess.Popen(
                mod_cmd,
                stdout=open(os.path.join(step["dir"], "mod.log"), "w"),
                stderr=subprocess.STDOUT)
        abort_reason = None
        if not procs:
            time.sleep(step["duration"])        # 纯采样步骤（空载基线）
        else:
            deadline = time.time() + step["duration"] + 60.0
            while time.time() < deadline:
                if all(p.poll() is not None for p in procs.values()):
                    break
                time.sleep(poll_interval)
                sample = last_csv_row(bmc_csv)
                if sample:
                    abort_reason = evaluate_guardrails(sample, thresholds)
                    if abort_reason:
                        for p in procs.values():
                            p.terminate()
                        break
        for name, p in procs.items():
            try:
                step[f"{name}_rc"] = p.wait(timeout=60)
            except subprocess.TimeoutExpired:
                p.kill()                        # 超时不撒手：强杀后收割
                step[f"{name}_rc"] = p.wait()
        t["dwell_end"] = time.time()
        time.sleep(post_secs)
        t["post_end"] = time.time()
        sel1, dm1 = sel_count(), dmesg_text()
        step["sel_delta"] = sel1 - sel0
        step["ras_delta"] = ras_error_count(dm1) - ras_error_count(dm0)
        with open(os.path.join(step["dir"], "dmesg_tail.txt"), "w") as f:
            f.write(dm1[-8000:])
        step["timeline"] = t
        step["aborted"] = abort_reason
        step["sdc_suspect"] = bool(
            step.get("mod_rc") == 2
            or (step.get("eigen_rc") not in (0, None))
            or step["ras_delta"] > 0)
        with open(os.path.join(step["dir"], "summary.json"), "w") as f:
            json.dump(step, f, indent=1, default=str)
    finally:
        try:
            logger.terminate()
            logger.wait(timeout=15)
        except subprocess.TimeoutExpired:
            logger.kill()                       # 记录器僵死：强杀后收割
            logger.wait()
    return step


# ---------------- 阶段步生成 ----------------
def steps_p0(duration=300):
    m24 = mod_cores(24, "same")
    return [
        {"id": "p0_1_idle_baseline", "mod": None, "mod_cores": [],
         "eigen_cores": [], "duration": 60, "note": "空载基线"},
        {"id": "p0_2_eigen_full", "mod": None, "mod_cores": [],
         "eigen_cores": eigen_cores([]), "duration": duration,
         "note": "eigen 满载基线+热轨迹（功率护栏收紧数据）"},
        {"id": "p0_3_pvdit_full", "mod": "power_virus_dit",
         "mod_cores": eigen_cores([]), "eigen_cores": [],
         "duration": duration, "note": "现有 di/dt 载荷对照（~数百kHz 点）"},
        {"id": "p0_4_eigen_plus_pvdit", "mod": "power_virus_dit",
         "mod_cores": m24, "eigen_cores": eigen_cores(m24),
         "duration": duration, "note": "分核 co-run 冒烟"},
    ]


def steps_p1(duration=90, payloads=("dep", "ilp2")):
    m = mod_cores(24, "same")
    steps = []
    for pl in payloads:      # 载荷裁决：100% duty 满载功率对比（无 eigen，隔离测量）
        steps.append({"id": f"p1_payload_{pl}", "mod": "droop_mod",
                      "payload": pl, "freq_hz": 1.0, "duty": 1.0,
                      "mod_cores": m, "eigen_cores": [], "duration": 120,
                      "note": "payload 裁决（100% duty 功率对比）"})
    for f in (0.1, 0.25, 0.5, 1.0):
        for duty in (0.25, 0.5, 0.75):
            for pl in payloads:
                # SDD 裁定 18（P0 证据）：eigen co-run 的 dwell 内 p2p 噪声
                # 实测 156W（测试轮换/标定相位），24 核调制器摆幅 ~29W 会被
                # 淹没——ΔI 标定必须 mod-only；co-run 行为由 P3/P4 的
                # V_sag/功能证据覆盖。
                steps.append({"id": f"p1_f{f:g}_d{duty}_{pl}",
                              "mod": "droop_mod", "payload": pl,
                              "freq_hz": f, "duty": duty, "mod_cores": m,
                              "eigen_cores": [],
                              "duration": duration,
                              "note": "低频标定（mod-only，裁定 18）："
                                      "BMC 电平差 → ΔI"})
    return steps


def steps_p2(duration=120):
    m = mod_cores(24, "same")
    return [{"id": f"p2_f{f:g}", "mod": "droop_mod", "payload": "dep",
             "freq_hz": f, "duty": 0.5, "mod_cores": m,
             "eigen_cores": eigen_cores(m), "duration": duration,
             "note": "中频过渡"} for f in (1e3, 1e4, 1e5, 1e6, 1e7)]


def steps_p3(payload="dep", duration=120):
    m = mod_cores(24, "same")
    freqs = [1e6, 2e6, 5e6, 1e7, 2e7, 5e7, 7.5e7, 1e8, 1.25e8, 1.5e8,
             1.75e8, 2e8]
    dense = {2e7, 5e7, 1e8, 1.5e8}
    steps = []
    for f in freqs:
        for d in ((0.3, 0.5, 0.7) if f in dense else (0.5,)):
            steps.append({"id": f"p3_f{f:g}_d{d}", "mod": "droop_mod",
                          "payload": payload, "freq_hz": f, "duty": d,
                          "mod_cores": m, "eigen_cores": eigen_cores(m),
                          "duration": duration, "note": "目标频段扫频"})
    return steps


def steps_p3b(freq, duty, payload="dep", duration=120):
    steps = []
    for place in ("same", "cross"):
        for mcount in (8, 16, 32, 64):
            m = mod_cores(mcount, place)
            steps.append({"id": f"p3b_m{mcount}_{place}",
                          "mod": "droop_mod", "payload": payload,
                          "freq_hz": freq, "duty": duty,
                          "mod_cores": m, "eigen_cores": eigen_cores(m),
                          "duration": duration, "note": "M/放置扫描"})
    return steps


def steps_p4(freq, duty, payload, duration=2400, placement="same", m=24):
    mcores = mod_cores(m, placement)
    return [{"id": "p4_best", "mod": "droop_mod", "payload": payload,
             "freq_hz": freq, "duty": duty, "mod_cores": mcores,
             "eigen_cores": eigen_cores(mcores), "duration": duration,
             "eigen_tests": EIGEN_TESTS_FULL,
             "note": "最优配置强化驻留：全家族 eigen + 最强调制（40min）"}]


# ---------------- 心跳 ----------------
def start_heartbeat(data_root, phase):
    import threading
    state = {"phase": phase, "step": "", "n": 0}
    stop = threading.Event()
    hb = os.path.join(data_root, "heartbeat.json")

    def loop():
        while not stop.wait(10.0):
            state["n"] += 1
            state["ts"] = time.time()
            try:
                with open(hb, "w") as f:
                    json.dump(state, f)
            except OSError:
                pass
    th = threading.Thread(target=loop, daemon=True)
    th.start()
    return stop, state


# ---------------- CLI ----------------
def main(argv=None):
    import argparse
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--phase", required=True,
                    choices=["p0", "p1", "p2", "p3", "p3b", "p4"])
    ap.add_argument("--data-root", default="/home/sdc/droop-excite")
    ap.add_argument("--repo",
                    default="/home/sdc/wangxu/operator/sdcshield")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--step-duration", type=float, default=None)
    ap.add_argument("--payload", default="dep",
                    help="p3/p3b/p4 的调制载荷（P1 裁决结果）")
    ap.add_argument("--freq", type=float, default=None,
                    help="p3b/p4 的频率（P3/P3b 最优结果）")
    ap.add_argument("--duty", type=float, default=None,
                    help="p3b/p4 的占空比")
    args = ap.parse_args(argv)

    if not args.dry_run:
        assert_topology()
    run_id = time.strftime("%Y%m%dT%H%M%S")
    run_dir = os.path.join(args.data_root, f"{args.phase}_{run_id}")
    paths = {"droop_mod": os.path.join(HERE, "droop_mod"),
             "sdcshield": os.path.join(args.repo, "builddir", "sdcshield"),
             "run_dir": run_dir}

    if args.phase == "p0":
        steps = steps_p0()
    elif args.phase == "p1":
        steps = steps_p1()
    elif args.phase == "p2":
        steps = steps_p2()
    elif args.phase == "p3":
        steps = steps_p3(payload=args.payload)
    elif args.phase == "p3b":
        if args.freq is None or args.duty is None:
            sys.exit("p3b 需要 --freq/--duty（来自 p3 最优）")
        steps = steps_p3b(args.freq, args.duty, payload=args.payload)
    else:
        if None in (args.freq, args.duty):
            sys.exit("p4 需要 --freq/--duty/--payload（来自 p3/p3b 最优）")
        steps = steps_p4(args.freq, args.duty, args.payload)
    if args.step_duration:
        for s in steps:
            s["duration"] = args.step_duration

    if args.dry_run:
        for s in steps:
            s["dir"] = "(dry)"
            print(json.dumps(
                {"id": s["id"],
                 "mod": build_mod_cmd(s, paths) if s.get("mod") else None,
                 "eigen": build_eigen_cmd(s, paths)
                 if s.get("eigen_cores") else None},
                ensure_ascii=False))
        return 0

    if os.geteuid() != 0:
        sys.exit("需要 root（ipmitool / SCHED_FIFO）")
    if any(s.get("mod") == "droop_mod" for s in steps) \
            and not os.path.exists(paths["droop_mod"]):
        sys.exit(f"missing {paths['droop_mod']}（先编译：见 README）")
    if any(s.get("eigen_cores") or s.get("mod") == "power_virus_dit"
           for s in steps) \
            and not os.path.exists(paths["sdcshield"]):
        sys.exit(f"missing {paths['sdcshield']}（--repo 指定仓库）")
    os.makedirs(run_dir, exist_ok=True)

    thresholds = dict(DEFAULT_THRESHOLDS)
    th_path = os.path.join(args.data_root, "thresholds.json")
    if os.path.exists(th_path):      # P0 之后可收紧持久化
        with open(th_path) as f:
            thresholds.update(json.load(f))
    hb_stop, hb_state = start_heartbeat(args.data_root, args.phase)
    try:
        for s in steps:
            hb_state["step"] = s["id"]
            print(f"=== {s['id']} ({s.get('note', '')}) ===", flush=True)
            run_step(s, paths, thresholds)
            if s.get("sdc_suspect"):
                print("SDC 疑似事件 —— 停机取证（spec D4）。证据目录：",
                      s["dir"], flush=True)
                break
            if s.get("aborted"):
                print("护栏触发，中止当步：", s["aborted"], flush=True)
    finally:
        hb_stop.set()
    print("run_dir:", run_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())

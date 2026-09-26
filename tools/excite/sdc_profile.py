#!/usr/bin/env python3
"""sdc_profile.py — 压力 profile：定义/resolve 冻结/受界执行器（v5 §7.5，M3 Task 1）。stdlib only。

零第三方依赖裁定（M3 计划 Tech Stack）：profile 文件用 JSON——v5 §7.5 YAML 示例的
语义等价物（字段逐项对应），避免 yaml 依赖；JSON 无注释语法，"_" 前缀键作注释
载体（校验忽略，见 configs/sdc-excite-reproduce/profiles/victim_candidate_v1.json 注释头）。

三层：
  load_profile(path)/load_profile_dict(d)   JSON profile 校验（v5 §7.5 字段逐项）；
  resolve(profile, capabilities, topo)       resolved-profile 冻结：CPU 列表按拓扑
      实际展开（all_except_victim = online − victim；**禁止 p0 假定编号**——一律
      sysfs 口径的 topology_snapshot，PPTT 伪影板 package/cluster id 不连续）、
      capabilities/topology 快照、二进制 sha256、family→测试映射、seed 等所有
      默认值——同一 profile 名在不同机器/时点不可静默漂移（v5 §7.5 末段）；
      axes（四轴中 M3 可执行三轴：cpu_hotplug/governor_experiment/load_shaping）
      对照 capabilities 拒绝缺口——unknown 同样拒绝（能力探测诚实，v5 §7.4）；
  ProfileRunner(resolved, data_root).run_once()   一轮 victim+aggressor 编排：
      victim 先起 → aggressors 后起（与 victim 并发扰动）→ 全部 _await（超时 =
      limits.duration_s + 120）→ cleanup_orphans → 返回
      {"rc","launched","victim_rc","aggressor_rcs","started_at","ended_at"}。

发射约定对齐 sdc-excite-reproduce.sh：`sdcshield --cpuset=<逗号集> -e <tests>
-t <duration>s [-s <seed>] [-n <max_threads>] -o <yaml>`。真实 _launch 以
sdc_common.sh 的 retest_guarded 包裹（内存护栏复用驱动语义：前置门 6GB /
看门狗 2.5GB / rc=250=拦截），并遵守 env -u SDC_ROOT_PW 纪律（M3 Global
Constraints）。

战役并行安全（红线）：run_once 末尾的 cleanup_orphans 以 SDC_ORPHAN_PIDS
沙盒限定只杀本 run 自己拉起的进程——绝不落入 pkill 全局路径（2026-09-26 实测
教训：comm=control 与战役运行中切片不可区分）；孤儿 control 切片的兜底由驱动
侧阶段边界 cleanup_orphans 负责。

测试注入点：_launch/_await/_tests_for_family/_cleanup_orphans——单测全部
mock，绝不真跑 sdcshield。
"""
import argparse, copy, hashlib, json, os, shlex, signal, subprocess, sys, time

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "telemetry"))
import sdc_topology                      # 实时拓扑快照（M1 资产，未重写）

REPO_DIR = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                        "..", ".."))
SDC_COMMON_SH = os.path.join(REPO_DIR, "scripts", "sdc-excite-reproduce",
                             "sdc_common.sh")

REQUIRED_TOP = ("profile_id", "victim", "aggressors", "environment", "monitoring",
                "safety_policy", "limits")           # v5 §7.5 逐项
SEED_POLICIES = ("fixed", "fixed_then_sweep")
AGGRESSOR_TOPOLOGIES = ("all_except_victim", "explicit")
AXES_KEYS = ("cpu_hotplug", "governor_experiment", "load_shaping")
AWAIT_GRACE_S = 120          # _await 超时 = duration + 120（retest_guarded 的
                             # TERM→KILL 序列在 duration+30+60 内收完，此为外层兜底）

# aggressor family → 测试名默认表。测试名取自本机 --list-tests 实测名单
# （2026-09-26，422 用例）；映射可在 run 期被 ProfileRunner._tests_for_family
# 覆写（测试注入点/换板适配——resolve 冻结的即本表快照）。
DEFAULT_FAMILY_TESTS = {
    "cache":   ["cachebounce", "cache_stress_aggressor"],
    "integer": ["adcx_arm"],
    "stream":  ["memcpy_l1d_size"],
    "neon":    ["sleef_neon"],
}

_HASH_CACHE = {}             # (path, st_mtime_ns, st_size) -> sha256（277MB 二进制
                             # 重复 resolve 不重复读盘）


def _now():
    return time.strftime("%F %T")


# ---------------------------------------------------------------------------
# profile 定义（v5 §7.5）

def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


def _int_list(v, what):
    _require(isinstance(v, list) and v and
             all(isinstance(x, int) and not isinstance(x, bool) for x in v),
             f"{what} 必须为非空 int 列表: {v!r}")
    return list(v)


def _str_list(v, what):
    _require(isinstance(v, list) and v and
             all(isinstance(x, str) and x for x in v),
             f"{what} 必须为非空字符串列表: {v!r}")
    return list(v)


def load_profile_dict(d):
    """校验并归一化 profile dict（ "_" 前缀键=注释载体，忽略）。配置错误必须
    响亮失败（ValueError 带字段名），不静默降级。返回归一化副本（不改动入参）。"""
    _require(isinstance(d, dict), f"profile 必须为 JSON 对象: {type(d).__name__}")
    where = f"profile {d.get('profile_id', '?')!r}: "
    for k in REQUIRED_TOP:
        _require(k in d, where + f"缺必填键 {k}（v5 §7.5）")
    out = {k: v for k, v in d.items() if not k.startswith("_")}
    if "schema_version" in out:
        _require(out["schema_version"] == 1,
                 where + f"schema_version != 1: {out['schema_version']!r}")
    _require(isinstance(out["profile_id"], str) and out["profile_id"],
             where + "profile_id 必须为非空字符串")

    v = out["victim"]
    _require(isinstance(v, dict), where + "victim 必须为对象")
    vic = dict(v)
    vic["cpus"] = _int_list(vic.get("cpus"), where + "victim.cpus")
    vic["tests"] = _str_list(vic.get("tests"), where + "victim.tests")
    _require(vic.get("seed_policy") in SEED_POLICIES,
             where + f"victim.seed_policy 非法: {vic.get('seed_policy')!r}"
                     f"（合法: {SEED_POLICIES}）")
    if "seed" in vic:
        _require(isinstance(vic["seed"], str) and vic["seed"],
                 where + "victim.seed 必须为非空字符串")
    vic["iterations_per_seed"] = vic.get("iterations_per_seed", 1)
    _require(isinstance(vic["iterations_per_seed"], int)
             and not isinstance(vic["iterations_per_seed"], bool)
             and vic["iterations_per_seed"] > 0,
             where + "victim.iterations_per_seed 必须为正整数")
    out["victim"] = vic

    a = out["aggressors"]
    _require(isinstance(a, dict), where + "aggressors 必须为对象")
    agg = dict(a)
    _require(agg.get("topology") in AGGRESSOR_TOPOLOGIES,
             where + f"aggressors.topology 非法: {agg.get('topology')!r}"
                     f"（合法: {AGGRESSOR_TOPOLOGIES}）")
    agg["families"] = _str_list(agg.get("families"), where + "aggressors.families")
    agg["duty_cycle"] = agg.get("duty_cycle", 1.0)
    _require(isinstance(agg["duty_cycle"], (int, float))
             and 0 < agg["duty_cycle"] <= 1,
             where + f"aggressors.duty_cycle 必须为 (0,1]: {agg['duty_cycle']!r}")
    if agg.get("max_threads") is not None:
        _require(isinstance(agg["max_threads"], int)
                 and not isinstance(agg["max_threads"], bool)
                 and agg["max_threads"] > 0,
                 where + "aggressors.max_threads 必须为正整数")
    if agg["topology"] == "explicit":
        agg["cpus"] = _int_list(agg.get("cpus"), where + "aggressors.cpus（explicit 拓扑必填）")
    out["aggressors"] = agg

    e = out["environment"]
    _require(isinstance(e, dict), where + "environment 必须为对象")
    env = dict(e)
    _require(env.get("governor") == "performance",
             where + f"environment.governor 只接受 performance（M3 安全不变式："
                     f"实验性 governor 切换需独立授权，v5 §7.4.1/T2 裁定），"
                     f"实测 {env.get('governor')!r}")
    out["environment"] = env

    m = out["monitoring"]
    _require(isinstance(m, dict), where + "monitoring 必须为对象")
    mon = dict(m)
    mon["pmu_groups"] = _str_list(mon.get("pmu_groups"), where + "monitoring.pmu_groups")
    _require(isinstance(mon.get("steady_period_ms"), int)
             and not isinstance(mon.get("steady_period_ms"), bool)
             and mon["steady_period_ms"] > 0,
             where + "monitoring.steady_period_ms 必须为正整数")
    out["monitoring"] = mon

    _require(isinstance(out["safety_policy"], str) and out["safety_policy"],
             where + "safety_policy 必须为非空字符串")

    l = out["limits"]
    _require(isinstance(l, dict), where + "limits 必须为对象")
    lim = dict(l)
    for k in ("duration_s", "max_failures"):
        _require(isinstance(lim.get(k), int) and not isinstance(lim.get(k), bool)
                 and lim[k] > 0, where + f"limits.{k} 必须为正整数")
    out["limits"] = lim

    axes = out.get("axes", {})
    _require(isinstance(axes, dict), where + "axes 必须为对象")
    norm = {}
    for k, spec in axes.items():
        _require(k in AXES_KEYS,
                 where + f"axes 未知轴 {k!r}（合法: {AXES_KEYS}）")
        if isinstance(spec, bool):
            norm[k] = {"enabled": spec}
        else:
            _require(isinstance(spec, dict), where + f"axes.{k} 必须为对象或 bool")
            spec = dict(spec)
            spec["enabled"] = bool(spec.get("enabled", False))
            norm[k] = spec
    out["axes"] = norm
    return out


def load_profile(path):
    """从 JSON 文件载入并校验（load_profile_dict 的文件形态）。"""
    with open(path, encoding="utf-8") as f:
        return load_profile_dict(json.load(f))


def load_capabilities(path):
    """capabilities.env（collect_inventory v2 产）→ str dict。
    行内注释（` # ...`）与引号（MEM_NODES="1 3"）剥离；"#"-开头行跳过。"""
    caps = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            line = line.strip()
            if not line or line.startswith("#") or "=" not in line:
                continue
            k, v = line.split("=", 1)
            caps[k.strip()] = v.split("#", 1)[0].strip().strip('"')
    return caps


def binary_sha256(path):
    """sha256(builddir/sdcshield)——resolved 冻结的二进制身份。按
    (path, mtime, size) 记忆化：277MB 二进制重复 resolve 不重复读盘。"""
    st = os.stat(path)
    key = (path, st.st_mtime_ns, st.st_size)
    if key not in _HASH_CACHE:
        h = hashlib.sha256()
        with open(path, "rb") as f:
            for chunk in iter(lambda: f.read(1 << 16), b""):
                h.update(chunk)
        _HASH_CACHE[key] = h.hexdigest()
    return _HASH_CACHE[key]


def _require_cap(caps, key, what):
    v = caps.get(key)
    _require(v == "yes",
             f"能力缺口: {what} 需 {key}=yes，实测 {v!r}"
             f"（unknown=未探测，同样拒绝——能力探测诚实，v5 §7.4）")


def _derive_seed(profile_id):
    """seed_policy=fixed 且未显式给 seed 时的确定性派生（跨进程/跨机器稳定：
    PYTHONHASHSEED 不参与）。LCG: 前缀 = 引擎名:状态 的 -s 语法。"""
    h = hashlib.sha256(("sdc-profile:" + profile_id).encode()).hexdigest()
    return "LCG:" + str(int(h[:12], 16))


# ---------------------------------------------------------------------------
# resolve：resolved-profile 冻结（v5 §7.5 末段）

def resolve(profile, capabilities, topology_snapshot):
    """profile + capabilities + topology → 不可变 resolved-profile dict。

    冻结：CPU 列表实际化（all_except_victim 按 online 展开 / explicit 校验）、
    capabilities+topology 快照、二进制哈希、family→测试映射、victim seed、
    所有默认值。拒绝：victim ⊄ online、explicit 与 victim 重叠或 ⊄ online、
    axes 能力缺口、NPROC 与 online 数不一致（capabilities.env 过期防漂移）。
    """
    p = load_profile_dict(profile)                      # 结构校验（含 axes 归一化）
    _require(isinstance(capabilities, dict),
             "capabilities 必须为 dict（load_capabilities 产物）")
    _require(isinstance(topology_snapshot, dict)
             and isinstance(topology_snapshot.get("online"), list),
             "topology_snapshot 缺 online 列表（sdc_topology.snapshot 口径）")
    online = topology_snapshot["online"]
    where = f"resolve {p['profile_id']!r}: "

    victim_cpus = sorted(p["victim"]["cpus"])
    bad = [c for c in victim_cpus if c not in online]
    _require(not bad, where + f"victim CPU {bad} 不在 online 集（topology 实测 "
                              f"{len(online)} 核）——victim 必须落在当前 online CPU 内")

    if "NPROC" in capabilities:
        try:
            n = int(capabilities["NPROC"])
        except ValueError:
            n = None
        _require(n is None or n == len(online),
                 where + f"capabilities NPROC={capabilities['NPROC']} 与 online 数 "
                         f"{len(online)} 不一致——capabilities.env 过期？"
                         f"重跑 collect_inventory.sh 后再 resolve")

    for name, spec in p["axes"].items():                # 四轴能力缺口（诚实拒绝）
        if not spec["enabled"]:
            continue
        if name == "cpu_hotplug":
            _require_cap(capabilities, "CPU_ONLINE_WRITABLE", "axes.cpu_hotplug")
        elif name == "governor_experiment":
            _require_cap(capabilities, "GOVERNOR_WRITABLE", "axes.governor_experiment")
            _require_cap(capabilities, "HAS_CPUFREQ", "axes.governor_experiment")

    unknown = [f for f in p["aggressors"]["families"]
               if f not in DEFAULT_FAMILY_TESTS]
    _require(not unknown,
             where + f"未知 aggressor family: {unknown}"
                     f"（默认表: {sorted(DEFAULT_FAMILY_TESTS)}；"
                     f"覆写 ProfileRunner._tests_for_family 可注入）")

    topo = p["aggressors"]["topology"]
    if topo == "all_except_victim":
        vset = set(victim_cpus)
        aggressor_cpus = [c for c in online if c not in vset]
    else:                                               # explicit
        aggressor_cpus = sorted(p["aggressors"]["cpus"])
        off = [c for c in aggressor_cpus if c not in online]
        _require(not off, where + f"explicit aggressor CPU {off} 不在 online 集")
        ov = [c for c in aggressor_cpus if c in set(victim_cpus)]
        _require(not ov, where + f"explicit aggressor 集 {aggressor_cpus} 与 victim "
                                 f"{victim_cpus} 重叠——victim/aggressor 必须隔离")
    _require(aggressor_cpus, where + "aggressor 集为空（online 全被 victim 占据？）")

    bin_path = os.environ.get("SDC_BIN",
                              os.path.join(REPO_DIR, "builddir", "sdcshield"))
    _require(os.path.isfile(bin_path),
             where + f"sdcshield 二进制不存在: {bin_path}（SDC_BIN 可覆盖；"
                     f"先完成构建——resolved 必须冻结真实二进制哈希）")

    seed = p["victim"].get("seed") or _derive_seed(p["profile_id"])
    return {
        "schema_version": 1,
        "profile_id": p["profile_id"],
        "resolved_at": _now(),
        "victim_cpus": victim_cpus,
        "victim_tests": list(p["victim"]["tests"]),
        "victim_seed_policy": p["victim"]["seed_policy"],
        "victim_seed": seed,
        "iterations_per_seed": p["victim"]["iterations_per_seed"],
        "aggressor_topology": topo,
        "aggressor_cpus": aggressor_cpus,
        "aggressor_families": list(p["aggressors"]["families"]),
        "aggressor_family_tests": {f: list(DEFAULT_FAMILY_TESTS[f])
                                   for f in p["aggressors"]["families"]},
        "aggressor_duty_cycle": p["aggressors"]["duty_cycle"],
        "aggressor_max_threads": p["aggressors"].get("max_threads"),
        "environment": dict(p["environment"]),
        "monitoring": {**p["monitoring"],
                       "pmu_capabilities": {k: v for k, v in capabilities.items()
                                            if k.startswith("PMU_")}},
        "safety_policy": p["safety_policy"],
        "limits": dict(p["limits"]),
        "axes": copy.deepcopy(p["axes"]),
        "capabilities_snapshot": dict(capabilities),
        "topology_snapshot": copy.deepcopy(topology_snapshot),
        "binary_path": bin_path,
        "binary_hash": "sha256:" + binary_sha256(bin_path),
    }


# ---------------------------------------------------------------------------
# 执行器：victim + aggressor 一轮编排（受界，测试全 mock）

class ProfileRunner:
    """run_once：victim 先起、aggressors 后起并发、全部 _await、收残。
    _launch/_await/_tests_for_family/_cleanup_orphans 为注入点（单测 mock）。"""

    def __init__(self, resolved, data_root):
        self.resolved = resolved
        self.data_root = data_root
        self.bin = resolved["binary_path"]
        self._seq = 0

    # ---- 注入点 ----

    def _tests_for_family(self, family):
        """family → 测试名（默认读 resolved 冻结的 DEFAULT_FAMILY_TESTS 快照；
        换板/测试注入可覆写）。"""
        tests = self.resolved["aggressor_family_tests"].get(family)
        if not tests:
            raise ValueError(f"无 family 映射: {family!r}（覆写本方法注入）")
        return list(tests)

    def _launch(self, role, cpus, test, extra):
        """真实发射：retest_guarded 包裹的 sdcshield 子进程（bash wrapper）。
        返回 Popen——_await 等 wrapper rc（250=内存门拦截，同驱动语义）。
        env -u SDC_ROOT_PW 纪律；start_new_session 使 killpg 可收整树。
        """
        duration = int(self.resolved["limits"]["duration_s"])
        runs = os.path.join(self.data_root, "runs")
        os.makedirs(runs, exist_ok=True)
        self._seq += 1
        tag = f"{time.strftime('%Y%m%d-%H%M%S')}-{self._seq}-{role}"
        yaml_path, out_path = os.path.join(runs, tag + ".yaml"), os.path.join(runs, tag + ".out")
        rtxt = os.path.join(runs, tag + ".retests.txt")
        cmd = [self.bin, "--cpuset=" + ",".join(str(c) for c in cpus),
               "-e", test, *extra, "-o", yaml_path]
        inner = " ".join(shlex.quote(x) for x in cmd)
        guard_s = duration + 30            # timeout TERM→(60s)→KILL 在 duration+90 收完，
                                          # _await 的 duration+120 是外层兜底
        script = (f"source {shlex.quote(SDC_COMMON_SH)}; "
                  f"retest_guarded {shlex.quote(tag)} {shlex.quote(rtxt)} "
                  f"{guard_s}s {inner}")
        env = {k: v for k, v in os.environ.items() if k != "SDC_ROOT_PW"}
        out = open(out_path, "wb")
        try:
            return subprocess.Popen(["bash", "-c", script], stdout=out,
                                    stderr=subprocess.STDOUT, env=env,
                                    start_new_session=True)
        finally:
            out.close()

    def _await(self, pid, timeout):
        """等 wrapper 结束；超时 KILL 其进程组（sdcshield 对 TERM 无响应前科
        ——RCA 附3，直接 KILL 不做 TERM 阶梯）；负 rc 归一 128+sig（137 语义）。"""
        try:
            return pid.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            try:
                os.killpg(pid.pid, signal.SIGKILL)
            except (ProcessLookupError, PermissionError):
                pass
            rc = pid.wait()
            return rc if rc >= 0 else 128 - rc

    def _cleanup_orphans(self, pids):
        """收尾收残：source sdc_common.sh 调 cleanup_orphans，但以
        SDC_ORPHAN_PIDS 沙盒限定只杀本 run 拉起的 pid（战役并行红线：全局
        pkill 会误杀战役 control 切片；孤儿 control 切片兜底在驱动侧阶段边界）。
        空表不落地 bash——SDC_ORPHAN_PIDS 空串会退化为全局 pkill。"""
        plist = ",".join(str(p.pid if isinstance(p, subprocess.Popen) else p)
                         for p in pids if p)
        if not plist:
            return
        subprocess.run(
            ["bash", "-c", f"source {shlex.quote(SDC_COMMON_SH)}; cleanup_orphans"],
            env=dict(os.environ, SDC_ORPHAN_PIDS=plist),
            stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)

    # ---- 编排 ----

    def run_once(self):
        """一轮：victim 先起 → aggressors 后起（并发扰动）→ 全部 _await →
        收残。返回 {"rc","launched","victim_rc","aggressor_rcs",
        "started_at","ended_at"}；rc=首个非零（victim 优先）。"""
        r = self.resolved
        duration = int(r["limits"]["duration_s"])
        await_to = duration + AWAIT_GRACE_S
        started = _now()
        tracked, aggressor_procs = [], []
        try:
            extra = ["-t", f"{duration}s"]
            if r["victim_seed"]:
                extra += ["-s", r["victim_seed"]]
            tracked.append(self._launch("victim", r["victim_cpus"],
                                        ",".join(r["victim_tests"]), extra))
            mt = r.get("aggressor_max_threads")
            n_threads = max(1, mt // len(r["aggressor_families"])) if mt else None
            for fam in r["aggressor_families"]:
                extra = ["-t", f"{duration}s"]
                if n_threads:                # 受界纪律：aggressor 总并发 ≤max_threads
                    extra += ["-n", str(n_threads)]
                tracked.append(self._launch("aggressor", r["aggressor_cpus"],
                                            ",".join(self._tests_for_family(fam)),
                                            extra))
                aggressor_procs.append((fam, tracked[-1]))
            victim_rc = self._await(tracked[0], await_to)
            aggressor_rcs = {fam: self._await(pid, await_to)
                             for fam, pid in aggressor_procs}
        finally:
            self._cleanup_orphans(tracked)   # 异常路径也收残
        rc = victim_rc if victim_rc != 0 else \
            next((v for v in aggressor_rcs.values() if v != 0), 0)
        return {"rc": rc, "launched": len(tracked), "victim_rc": victim_rc,
                "aggressor_rcs": aggressor_rcs, "started_at": started,
                "ended_at": _now()}


# ---------------------------------------------------------------------------

def _load_topology(path):
    if path:
        with open(path, encoding="utf-8") as f:
            return json.load(f)
    return sdc_topology.snapshot()           # sysfs 实时（只读探针）


def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="sdc_profile.py",
        description="sdc_profile——压力 profile 定义/resolve 冻结/受界执行器"
                    "（v5 §7.5，M3）")
    sub = ap.add_subparsers(dest="cmd", required=True)
    rp = sub.add_parser("run", help="resolve 并执行一轮 victim+aggressor"
                                    "（--dry-run 只产 resolved 不执行）")
    rp.add_argument("profile", help="profile JSON 路径")
    rp.add_argument("--dry-run", action="store_true",
                    help="只打印 resolved-profile JSON 到 stdout，不执行")
    rp.add_argument("--data-root", default=None,
                    help="数据根（默认 $SDC_EXCITE_REPRODUCE_DIR/"
                         "$SDC_CAMPAIGN_DIR/~/sdc-excite-reproduce）")
    rp.add_argument("--capabilities", default=None,
                    help="capabilities.env 路径（默认 <data-root>/capabilities.env）")
    rp.add_argument("--topology", default=None,
                    help="topology JSON 路径（默认 sysfs 实时快照）")
    a = ap.parse_args(argv)

    root = a.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") or \
        os.environ.get("SDC_CAMPAIGN_DIR") or os.path.expanduser("~/sdc-excite-reproduce")
    caps_path = a.capabilities or os.path.join(root, "capabilities.env")
    if not os.path.isfile(caps_path):
        print(f"sdc_profile: capabilities 文件不存在: {caps_path}"
              f"（先跑 collect_inventory.sh 或 --capabilities 指定）", file=sys.stderr)
        return 2
    try:
        resolved = resolve(load_profile(a.profile),
                           load_capabilities(caps_path), _load_topology(a.topology))
    except ValueError as e:
        print(f"sdc_profile: {e}", file=sys.stderr)
        return 2
    if a.dry_run:
        print(json.dumps(resolved, ensure_ascii=False, indent=1))
        return 0
    result = ProfileRunner(resolved, root).run_once()
    print(json.dumps(result, ensure_ascii=False, indent=1))
    return 0 if result["rc"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

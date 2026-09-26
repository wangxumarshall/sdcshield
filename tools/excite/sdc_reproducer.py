#!/usr/bin/env python3
"""sdc_reproducer.py — victim/aggressor 复现 runner + 真实性门禁 + 复现胶囊
（v5 §10.1-10.2，M3 Task 3）。stdlib only。

组成：
  wilson_ci(k, n)                 Wilson 95% 置信区间（纯函数；k=0 下界、k=n
                                  上界为数学精确值 0/1——FP 噪声显式截断）
  parse_event(event_dir)          M2 事件目录（handle_failure 产物口径）→
                                  结构化摘要：rc/cmd/test/seed/cpuset 请求/
                                  detecting 核/fail 行数/data-miscompare 块
                                  （重算 xor/lane/popcount）/复测行
  authenticity_gate(dir, ctx)     v5 §10.1 七项门禁 → (ok, reasons)。
                                  拒（ok=False）：证据缺失/自相矛盾/健康核对照
                                  复现/竞态定案/affinity 错位/sham 亦复现；
                                  警告（不拒）：对照未执行/未审计/无锚点——
                                  如实入 reasons 交上层裁量
  build_capsule(ev, resolved, out) v5 §10.2 复现胶囊：目录逐项 + SHA256SUMS
                                  全文件 + scripts/run.sh 生成（manifest 预检：
                                  内核 release/governor/online 集/拓扑指纹/
                                  BMC 热状态允许区间 ["ambient","+10C"] 形式）；
                                  **binaries/ 只放 sha256+路径引用不复制二进制**
                                  （500MB 级——run.sh 以内容哈希核验重放机
                                  本机二进制，$SDC_BIN 可覆盖路径）
  Reproducer(data_root, event_dir)
      .from_event() -> resolved   事件 test/seed → 最小 victim 单测 profile
                                  （复现条件建模 R 的最小起点，v5 §10.4）
      .run_cycle(profile, n)      n 次受界运行数失败 → {k, n, ci}（Wilson）
  consume_queue(data_root, once)  M3 T5 队列消费：扫 spool/repro_queue/（驱动
                                  enqueue_repro 产，M2 enqueue_reproduction
                                  落点）→ 逐事件 from_event→门禁→（过）capsule
                                  + 概率复测 QUEUE_TRIALS 次（单事件总预算
                                  QUEUE_EVENT_BUDGET_S ≤5min，per-trial 截断）
                                  → 结果行 spool/repro_done/<队列文件名> +
                                  删队列；CLI: --from-queue [--once]
                                  [--data-root DIR]（error/invalid 条目 rc=1
                                  诚实汇报；gate 拒/already_done 同样入 done
                                  不重试）

门禁七项对照（v5 §10.1 ↔ M3 简化裁定）：
  1 原始证据保存（stdout_summary/yaml_extract/context 三件齐）；
  2 完整迭代内（rc∈{124,137,143} 且 YAML 无 fail/crash 行=阶段边界拒——与驱动
    run_sdc 判据同口径；YAML 有 fail 行=框架已记录完整失败迭代，rc=137 收尾
    KILL 不改判——mesh 事件实证形态）；
  3 offset-lane-mask 重算（data-miscompare 块：actual^expected==mask、lane
    重算、popcount——取证链自洽性）；
  4 健康核对照（M3 简化：同二进制同 seed 在 cpuset 对照核跑一遍 rc+result
    比对——注入点 _control_run，测试 mock）；
  5 竞态审计（事件目录无竞态三探针产物→警告"未审计"不拒；检查单引用
    docs/superpowers/output/2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-
    plan-output.md 附3/事件#1——mesh 竞态假阳性定案方法学；race_audit.json
    verdict=race_confirmed→拒，test_bug 方向）；
  6 实际 CPU 确认（cpuset 请求 vs detecting-cpu：id{logical}/state:failed
    线程记录与 detecting-cpu 行，越界=affinity 未遵守/证据错位→拒）；
  7 复测与 sham（retests.txt：rc=250 内存门拦截=无效试验；sham_result 亦
    复现→拒——复现信号非特异；未执行→警告）。

编排约束（T2 评审移交②）：run_cycle 不编排任何轴——profile 启用轴即
ValueError（cpu_hotplug 的离线动作只允许在 victim/aggressor 全部退出后的
收尾窗口，M3 未实现该编排；静默丢弃轴=谎报复现条件）。

JSON 裁定（M3 计划 Tech Stack，同 T1 profile）：v5 §10.2 的 *.yaml/*.parquet
命名以语义等价 JSON/JSONL 落地（manifest.json/hypotheses.json/trials.jsonl），
零第三方依赖。

注入点（测试全 mock，绝不真跑 sdcshield——战役+9 服务运行中）：
  _control_run / Reproducer._run_once / _platform_probe。
"""
import argparse, glob, hashlib, json, os, re, shlex, shutil, sys, tempfile, time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "telemetry"))
import sdc_profile                      # T1——resolve/ProfileRunner 复用
import sdc_topology                     # M1 资产——from_event 默认实时拓扑

Z95 = 1.959963984540054                 # 95% 双侧正态分位
STAGE_BOUNDARY_RCS = (124, 137, 143)    # timeout/SIGKILL/SIGTERM（驱动口径）
MEMGUARD_RC = 250                       # retest_guarded 内存门拦截
CONTROL_TIMEOUT_S = 120                 # 健康核对照时长（驱动定向复测同款）
MAX_INVALID_TRIALS = 3                  # run_cycle 对 250 拦截的有界重试
EVENT_EVIDENCE_FILES = ("stdout_summary.out", "yaml_extract.txt", "context.txt")

# data-miscompare 块 type → 字节宽（logging.cpp SandstoneDataDetails::type_name
# 实名表；未知类型只做 xor/mask 重算，lane 检查跳过）
TYPE_SIZES = {
    "uint8_t": 1, "int8_t": 1, "HFloat8": 1, "BFloat8": 1,
    "uint16_t": 2, "int16_t": 2, "_Float16": 2, "_BFloat16": 2,
    "uint32_t": 4, "int32_t": 4, "float": 4,
    "uint64_t": 8, "int64_t": 8, "double": 8, "_Float64x": 8,
    "uint128_t": 16, "int128_t": 16, "_Float128": 16,
}

RACE_CHECKLIST_REF = ("docs/superpowers/output/"
                      "2026-09-23-2102312YVY10M6000038-sdc-7x24-stress-plan-"
                      "output.md（附3/事件#1 mesh 竞态假阳性定案方法学）")


def _now():
    return time.strftime("%F %T")


def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


# ---------------------------------------------------------------------------
# Wilson 95% 置信区间（纯函数）

def wilson_ci(k, n):
    """Wilson score 95% 区间 → (lo, hi)。

    k=0 下界与 k=n 上界为数学精确值 0/1（p̂=0 时 center==half、p̂=1 时
    lo=1/(1+z²/n) 的闭式退化端点），显式特判——消除公式浮点噪声
    （center-half 在 p̂=0 时为 ±1ulp 而非精确 0）。
    """
    _require(isinstance(k, int) and not isinstance(k, bool)
             and isinstance(n, int) and not isinstance(n, bool),
             f"wilson_ci 参数必须为 int: k={k!r} n={n!r}")
    _require(n > 0, f"wilson_ci n 必须 > 0: {n!r}")
    _require(0 <= k <= n, f"wilson_ci 需要 0 ≤ k ≤ n: k={k} n={n}")
    p = k / n
    denom = 1 + Z95 * Z95 / n
    center = (p + Z95 * Z95 / (2 * n)) / denom
    half = Z95 * (p * (1 - p) / n + Z95 * Z95 / (4 * n * n)) ** 0.5 / denom
    lo = 0.0 if k == 0 else max(0.0, center - half)
    hi = 1.0 if k == n else min(1.0, center + half)
    return (lo, hi)


# ---------------------------------------------------------------------------
# 事件目录解析（M2 handle_failure 产物口径）

def _extract_test_seed(text):
    """stdout_summary / yaml 同构提取（驱动 extract_failed_test/
    extract_fail_seed 语义的推广：fail|crash 通吃，seed 接受任意引擎——
    事件原种子即复现条件，不止 AES）。返回 (test, seed)。"""
    test = seed = None
    cur = None
    for line in text.splitlines():
        m = re.match(r"^-\s*test:\s+(\S+)\s*$", line)
        if m:
            cur = m.group(1)
            continue
        if test is None and cur and re.match(r"^\s{2}result:\s*(fail|crash)\b", line):
            test = cur
            continue
        if seed is None and re.match(r"^\s{2}fail:\s*\{", line):
            ms = re.search(r"seed:\s*'([^']+)'", line)
            if ms and ms.group(1):
                seed = ms.group(1)
    return test, seed


def _parse_miscompares(text):
    """data-miscompare 块（logging.cpp logging_format_data 实格式）→
    结构化列表；每块重算 xor=actual^expected、popcount、lane（首个差异
    字节的存储序偏移——hex 串为 big-endian 显示，存储 byte i 对应 hex 对
    size-1-i）。consistent=None 表示 actual/expected/mask 为 null
    （memcmp_offset 未定位差异）无从重算。"""
    blocks, cur = [], None
    for line in text.splitlines():
        if "data-miscompare:" in line:
            if cur:
                blocks.append(cur)
            cur = {"type": None, "offset": None, "actual": None,
                   "expected": None, "mask": None}
            continue
        if cur is None:
            continue
        if re.match(r"^-\s*test:", line):                # 块外（下一测试条目）
            blocks.append(cur)
            cur = None
            continue
        m = re.match(r"^\s*type:\s*(\S+)", line)
        if m:
            cur["type"] = m.group(1)
        m = re.match(r"^\s*offset:\s*\[\s*(-?\d+),\s*(-?\d+)\s*\]", line)
        if m:
            cur["offset"] = [int(m.group(1)), int(m.group(2))]
        for key in ("actual", "expected", "mask"):
            m = re.match(rf"^\s*{key}:\s*'([^']*)'", line)
            if m:
                cur[key] = m.group(1)
    if cur:
        blocks.append(cur)

    out = []
    for b in blocks:
        rec = dict(b)

        def _hex(v):
            if v is None or v == "null":
                return None
            try:
                return int(v, 16)
            except ValueError:
                return None

        a, e, mk = _hex(b["actual"]), _hex(b["expected"]), _hex(b["mask"])
        rec["actual"], rec["expected"], rec["mask"] = a, e, mk
        rec["recomputed_xor"] = rec["popcount"] = rec["lane"] = None
        if a is None or e is None or mk is None:
            rec["consistent"] = None
            out.append(rec)
            continue
        xor = a ^ e
        rec["recomputed_xor"] = xor
        rec["popcount"] = bin(xor).count("1")
        consistent = xor == mk
        size = TYPE_SIZES.get(b["type"])
        ha = (b["actual"] or "").removeprefix("0x")
        he = (b["expected"] or "").removeprefix("0x")
        if size and len(ha) == len(he) == size * 2:
            ba, be = bytes.fromhex(ha), bytes.fromhex(he)
            for i in range(size):
                if ba[size - 1 - i] != be[size - 1 - i]:
                    rec["lane"] = i
                    break
            if rec["lane"] is not None and b["offset"] is not None:
                consistent = consistent and rec["lane"] == b["offset"][1]
        rec["consistent"] = consistent
        out.append(rec)
    return out


def parse_event(event_dir):
    """事件目录 → 结构化摘要 dict（字段见模块头）。缺文件容错（门禁1 裁量），
    绝不因个别文件缺失抛异常。"""
    def _read(name):
        try:
            with open(os.path.join(event_dir, name),
                      encoding="utf-8", errors="replace") as f:
                return f.read()
        except OSError:
            return ""

    stdout, ctx, yml, retxt = (_read("stdout_summary.out"),
                               _read("context.txt"),
                               _read("yaml_extract.txt"),
                               _read("retests.txt"))
    ev = {"dir": os.path.abspath(event_dir),
          "event_id": os.path.basename(os.path.normpath(event_dir)),
          "files": sorted(os.listdir(event_dir))
          if os.path.isdir(event_dir) else [],
          "miscompares": _parse_miscompares(yml)}

    m = re.search(r"^rc:\s*(-?\d+)", ctx, re.M)
    ev["rc"] = int(m.group(1)) if m else None
    m = re.search(r"^cmd:\s+(.+)$", ctx, re.M)
    cmd = m.group(1).strip() if m else None
    ev["cmd"] = cmd
    ev["binary"] = cmd.split()[0] if cmd else None
    cpuset = None
    if cmd:
        m = re.search(r"--cpuset[= ]([0-9][0-9,]*)", cmd)
        if m:
            cpuset = [int(x) for x in m.group(1).split(",")]
    ev["cpuset"] = cpuset

    # test/seed：stdout 摘要（驱动首选）→ yaml fail 行 → context 记录
    test, seed = _extract_test_seed(stdout)
    if test is None or seed is None:
        t2, s2 = _extract_test_seed(yml)
        test, seed = test or t2, seed or s2
    m = re.search(r"failed_test:\s*(\S+)", ctx)
    if test is None and m and m.group(1) != "?":
        test = m.group(1)
    m = re.search(r"fail_seed:\s*(\S+?)\(usable=(\S*)\)", ctx)
    if m:
        ev["seed_usable"] = m.group(2) == "1"
        if seed is None and m.group(1) != "none":
            seed = m.group(1)
    else:
        ev["seed_usable"] = None
    ev["test"], ev["seed"] = test, seed

    ev["fail_lines"] = sum(1 for line in yml.splitlines()
                           if re.match(r"^\s*result:\s*(fail|crash)\b", line))

    # detecting 核：失败线程 id{logical}（CPU 设备失败时 id 行即 failing_cpu，
    # logging.cpp print_thread_header）+ detecting-cpu 行（他型设备）
    detecting, cur_id = [], None
    for line in yml.splitlines():
        m = re.match(r"^\s*-\s*thread:", line)
        if m:
            cur_id = None
            continue
        m = re.match(r"^\s*detecting-cpu:\s*\{\s*logical:\s*(\d+)", line)
        if m:
            detecting.append(int(m.group(1)))
            continue
        m = re.match(r"^\s*id:\s*\{\s*logical:\s*(\d+)", line)
        if m:
            cur_id = int(m.group(1))
            continue
        if cur_id is not None and re.match(r"^\s*state:\s*failed\b", line):
            detecting.append(cur_id)
            cur_id = None
    if not detecting:
        # 回退：fail 块 cpu-mask（'.'=未失败 'X'=全失败 hex=部分；位号=package
        # 内 core 序号——仅单 package 扁平板（本板 as-built）等同逻辑 CPU，
        # 多 package 板此回退跳过不猜）
        for line in yml.splitlines() + stdout.splitlines():
            m = re.search(r"cpu-mask:\s*'([^']*)'", line)
            if m and m.group(1) and m.group(1) != "null" and ":" not in m.group(1):
                detecting = [i for i, ch in enumerate(m.group(1))
                             if ch not in "._"]
                break
    ev["detecting_cpus"] = sorted(set(detecting))

    valid = reproduced = 0
    for line in retxt.splitlines():
        m = re.match(r"^retest\d+\((targeted|wholecmd)\)"
                     r" rc=(-?\d+) fail/crash=(\d*)", line)
        if not m:
            continue                          # skipped(memguard) 行等
        if int(m.group(2)) == MEMGUARD_RC:
            continue                          # 内存门拦截——非有效试验
        valid += 1
        if m.group(3) and int(m.group(3)) > 0:
            reproduced += 1
    ev["retests"] = {"valid": valid, "reproduced": reproduced}
    return ev


# ---------------------------------------------------------------------------
# 真实性门禁（v5 §10.1 七项）

def _control_run(binary, test, seed, cpus, timeout_s=CONTROL_TIMEOUT_S):
    """健康核对照（真实实现；测试 mock 本函数——战役红线）。

    同二进制同 seed 在 cpuset 对照核受界跑一遍（复用 T1 ProfileRunner 的
    retest_guarded 内存护栏发射约定），返回 {"rc", "fail_count"}。
    fail_count = 本轮 yaml 的 result: fail/crash 行数。
    """
    root = tempfile.mkdtemp(prefix="sdc-control-run-")
    try:
        runner = sdc_profile.ProfileRunner(
            {"binary_path": binary, "limits": {"duration_s": timeout_s}}, root)
        extra = ["-t", f"{timeout_s}s"]
        if seed:
            extra += ["-s", str(seed)]
        proc = runner._launch("control", list(cpus), test, extra)
        try:
            rc = runner._await(proc, timeout_s + sdc_profile.AWAIT_GRACE_S)
        finally:
            runner._cleanup_orphans([proc])
        fail_count = 0
        for y in glob.glob(os.path.join(root, "runs", "*.yaml")):
            try:
                with open(y, encoding="utf-8", errors="replace") as f:
                    fail_count += sum(1 for line in f
                                      if re.match(r"^\s*result:\s*(fail|crash)\b",
                                                  line))
            except OSError:
                pass
        return {"rc": rc, "fail_count": fail_count}
    finally:
        shutil.rmtree(root, ignore_errors=True)


def authenticity_gate(event_dir, capsule_ctx):
    """v5 §10.1 七项门禁 → (ok, reasons)。ok=无拒项；reasons 含拒+警（文案
    带 [门禁N 拒/警告] 前缀可判别）。capsule_ctx 键（全可选）：
    binary/control_cpus/control_timeout_s/control_result{rc,fail_count}/
    sham_result{k,n}。"""
    ctx = capsule_ctx or {}
    ev = parse_event(event_dir)
    rejects, warns = [], []

    # 1 原始证据保存
    missing = [n for n in EVENT_EVIDENCE_FILES
               if not os.path.isfile(os.path.join(event_dir, n))]
    if missing:
        rejects.append(f"[门禁1 拒] 原始证据缺失: {missing}（v5 §10.1.1——"
                       "原始 stdout/YAML/context 必须保全，非解析摘要）")

    # 2 完整迭代内
    rc, fl = ev["rc"], ev["fail_lines"]
    if rc in STAGE_BOUNDARY_RCS and fl == 0:
        rejects.append(f"[门禁2 拒] 失败不在完整迭代内: rc={rc} 为阶段边界终止"
                       "（124=timeout/137=SIGKILL/143=SIGTERM）且 YAML 无"
                       " fail/crash 行——非完整迭代事件（v5 §10.1.2，invalid）")
    elif fl == 0:
        warns.append("[门禁2 警告] YAML 无 fail/crash 行（进程级失败）——"
                     "完整迭代性未核验，复现口径以 rc≠0 计（v5 §10.1.2，不拒）")

    # 3 offset-lane-mask 重算
    if ev["miscompares"]:
        bad = [m for m in ev["miscompares"] if m["consistent"] is False]
        if bad:
            b = bad[0]
            rejects.append(
                f"[门禁3 拒] 取证数据自相矛盾: mask 重算不符——"
                f"actual^expected=0x{b['recomputed_xor']:x} != 记录 mask="
                f"0x{b['mask']:x}（{b['type']} @offset {b['offset']}，"
                "v5 §10.1.3 offset/lane/mask 重算）")
        nulls = [m for m in ev["miscompares"] if m["consistent"] is None]
        if nulls:
            warns.append(f"[门禁3 警告] {len(nulls)} 个 data-miscompare 块 "
                         "actual/expected/mask 为 null（memcmp_offset 未定位"
                         "差异）——无从重算（v5 §10.1.3，不拒）")
    elif fl:
        warns.append("[门禁3 警告] 无 data-miscompare 块（crash/非 memcmp 类"
                     "失败）——offset/lane/mask 无从重算（v5 §10.1.3，不拒）")

    # 4 健康核对照（M3 简化：同二进制同 seed 对照核 rc+result 比对）
    ctrl = ctx.get("control_result")
    if ctrl is None:
        binary = ctx.get("binary") or ev["binary"]
        cpus = ctx.get("control_cpus")
        if not (ev["test"] and binary and cpus):
            warns.append("[门禁4 警告] 健康核对照未执行（缺 test/binary/"
                         "control_cpus 之一）——确定性测试缺陷未排除"
                         "（v5 §10.1.4，不拒）")
        else:
            try:
                ctrl = _control_run(binary, ev["test"], ev["seed"], cpus,
                                    ctx.get("control_timeout_s",
                                            CONTROL_TIMEOUT_S))
            except Exception as exc:            # 发射异常如实入案不吞
                warns.append(f"[门禁4 警告] 健康核对照执行异常: {exc!r}"
                             "（不拒，人工核查）")
                ctrl = None
    if ctrl is not None:
        if ctrl.get("rc") == 0 and ctrl.get("fail_count", 0) == 0:
            pass                                # 通过——无问题行
        else:
            rejects.append(f"[门禁4 拒] 健康核对照同样失败: rc={ctrl.get('rc')}"
                           f" fail/crash={ctrl.get('fail_count')}——确定性测试"
                           "缺陷候选（test_bug 方向，v5 §10.1.4）")

    # 5 竞态审计（三探针检查单引用；无产物=未审计警告不拒）
    ra_path = os.path.join(event_dir, "race_audit.json")
    if os.path.isfile(ra_path):
        try:
            with open(ra_path, encoding="utf-8") as f:
                verdict = json.load(f).get("verdict")
        except (OSError, ValueError):
            verdict = None
        if verdict == "race_confirmed":
            rejects.append("[门禁5 拒] 竞态审计定案: 测试内部竞态"
                           "（race_confirmed）——test_bug 方向（v5 §10.1.5，"
                           "原始证据保留）")
        elif verdict is None:
            warns.append("[门禁5 警告] race_audit.json 不可解析——竞态审计"
                         "结果未知（v5 §10.1.5，不拒）")
    else:
        warns.append("[门禁5 警告] 竞态未审计: 事件目录无竞态三探针产物——"
                     f"检查单引用 {RACE_CHECKLIST_REF}"
                     "（test bug 一等候选，v5 §10.1.5，不拒）")

    # 6 实际 CPU 确认（cpuset 请求 vs detecting-cpu）
    det = ev["detecting_cpus"]
    if not det:
        warns.append("[门禁6 警告] 无 detecting-cpu/失败线程记录——实际 CPU"
                     " 无法核验（v5 §10.1.6，不拒）")
    elif ev["cpuset"] is not None:
        outside = [c for c in det if c not in set(ev["cpuset"])]
        if outside:
            rejects.append(f"[门禁6 拒] 实际 CPU 不符: detecting-cpu {outside} "
                           f"不在请求 cpuset {ev['cpuset']} 内——affinity 未"
                           "遵守或证据错位（v5 §10.1.6）")

    # 7 复测与 sham
    rt = ev["retests"]
    if rt["valid"] == 0:
        warns.append("[门禁7 警告] 无有效复测记录（retests.txt 缺失或全为"
                     "内存门拦截）——复现概率未估计（v5 §10.1.7，不拒）")
    elif rt["reproduced"] == rt["valid"]:
        warns.append(f"[门禁7 警告] 复测全数复现（k={rt['reproduced']}/"
                     f"n={rt['valid']}）——确定性缺陷候选，须健康核对照裁定"
                     "（v5 §10.1.7，不拒）")
    sham = ctx.get("sham_result")
    if sham is None:
        warns.append("[门禁7 警告] sham-control 未执行（v5 §10.1.7，不拒）")
    elif sham.get("k", 0) > 0:
        rejects.append(f"[门禁7 拒] sham-control 亦复现（k={sham.get('k')}/"
                       f"n={sham.get('n')}）——复现信号非特异（v5 §10.1.7）")

    return (not rejects, rejects + warns)


# ---------------------------------------------------------------------------
# 复现胶囊（v5 §10.2）

def topology_fingerprint(topo):
    """拓扑指纹（run.sh 预检口径）：sha256 over online 集 + 每 CPU
    (package,cluster,core,die) + NUMA cpulist 的规范化文本。

    **与 run.sh 内嵌预检片段同源**（本模块是 run.sh 的生成器——两处规范化
    由 test_capsule_run_sh_check_only_passes_on_live_machine 端到端守卫，
    漂移即红）。
    """
    parts = ["online=" + ",".join(str(c) for c in sorted(topo["online"]))]
    for c in sorted(topo["cpus"], key=int):
        e = topo["cpus"][c]
        parts.append("cpu%s:p%s,cl%s,co%s,d%s"
                     % (c, e["package"], e["cluster_id"], e["core_id"],
                        e["die_id"]))
    for nid in sorted(topo["nodes"], key=int):
        parts.append("node%s:%s" % (nid, topo["nodes"][nid]["cpulist"]))
    return hashlib.sha256(";".join(parts).encode()).hexdigest()


def _cpu_identity():
    """ARM64 /proc/cpuinfo 首处理器块身份（x86 无这些键则 None）。"""
    ident = {"implementer": None, "part": None, "revision": None}
    try:
        with open("/proc/cpuinfo", encoding="utf-8", errors="replace") as f:
            for line in f:
                m = re.match(r"CPU (implementer|part|revision)\s*:\s*(\S+)", line)
                if m:
                    ident[m.group(1)] = m.group(2)
    except OSError:
        pass
    return ident


def _platform_probe(topology_snapshot):
    """平台快照（capsule platform.json / run.sh 预检基准）——只读探针，不触
    特权、不起子进程。governor：有 cpufreq 读 cpu0 实值；无 cpufreq（本板
    Kunpeng 920 实况）记 performance 不变式。BMC/热状态以允许区间表示
    （v5 §10.2：无法精确复原的不假装确定）——thermal_ambient_c=None 即
    无数值基线，run.sh 预检只提示不拒。注入点：测试 mock。"""
    u = os.uname()
    gov_path = "/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor"
    try:
        with open(gov_path, encoding="utf-8") as f:
            governor = f.read().strip()
        has_cpufreq = True
    except OSError:
        governor, has_cpufreq = "performance", False
    return {
        "kernel_release": u.release,
        "kernel_version": u.version,
        "machine": u.machine,
        "cpu_identity": _cpu_identity(),
        "pagesize": os.sysconf("SC_PAGE_SIZE"),
        "governor": governor,
        "has_cpufreq": has_cpufreq,
        "online": sorted(topology_snapshot["online"]),
        "thermal_band": ["ambient", "+10C"],
        "thermal_ambient_c": None,
        "thermal_tolerance_c": 10,
    }


# run.sh 内嵌预检（python3 heredoc）——与 topology_fingerprint 同源规范化
_RUN_SH_PRECHECK = r'''
import glob, hashlib, json, os, platform, re, subprocess, sys
mf = json.load(open(sys.argv[1], encoding="utf-8"))
plat = mf["platform"]
fails, notes = [], []

def rd(p):
    try:
        return open(p).read().strip()
    except OSError:
        return ""

# 1. 内核 release（v5 §10.2：一致才跑）
rel = platform.release()
if rel != plat["kernel_release"]:
    fails.append("内核 release 不符: 实测 %s != manifest %s"
                 % (rel, plat["kernel_release"]))

# 2. governor（M3 安全不变式 performance；无 cpufreq 板卡提示不拒）
gov = rd("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor")
if gov:
    if gov != plat["governor"]:
        fails.append("governor 不符: 实测 %s != manifest %s" % (gov, plat["governor"]))
else:
    notes.append("无 cpufreq——governor 不可调（manifest 不变式 %s 不受影响）"
                 % plat["governor"])

def expand(s):
    out = set()
    for part in (s or "").split(","):
        if not part:
            continue
        if "-" in part:
            lo, hi = part.split("-")
            out.update(range(int(lo), int(hi) + 1))
        else:
            out.add(int(part))
    return out

online = expand(rd("/sys/devices/system/cpu/online"))

# 3. online 集
if online != set(plat["online"]):
    fails.append("online 集不符: 实测 %s != manifest %s"
                 % (sorted(online), plat["online"]))

# 4. 拓扑指纹（与 sdc_reproducer.topology_fingerprint 同源规范化——生成器与本
#    片段由 live 端到端测试共同守卫）
possible = sorted(expand(rd("/sys/devices/system/cpu/possible")))
parts = ["online=" + ",".join(str(c) for c in sorted(online))]
for c in possible:
    base = "/sys/devices/system/cpu/cpu%d" % c
    def iv(p):
        v = rd(p)
        try:
            return int(v)
        except ValueError:
            return -1
    parts.append("cpu%d:p%d,cl%d,co%d,d%d" % (
        c, iv(base + "/topology/physical_package_id"),
        iv(base + "/topology/cluster_id"), iv(base + "/topology/core_id"),
        iv(base + "/topology/die_id")))
for n in sorted(glob.glob("/sys/devices/system/node/node*"),
                key=lambda p: int(os.path.basename(p)[4:])):
    parts.append("node%s:%s" % (os.path.basename(n)[4:], rd(n + "/cpulist")))
fp = hashlib.sha256(";".join(parts).encode()).hexdigest()
if fp != mf["topology_hash"]:
    fails.append("拓扑指纹不符: 实测 %s != manifest %s" % (fp, mf["topology_hash"]))

# 5. BMC/热状态允许区间（v5 §10.2：无法精确复原的以区间表示，不假装确定）
band = plat.get("thermal_band") or []
amb = plat.get("thermal_ambient_c")
if amb is None:
    notes.append("BMC/热状态未核验（无数值基线）——按允许区间 %s 记录" % band)
else:
    try:
        out = subprocess.run(["ipmitool", "sdr", "type", "Temperature"],
                             capture_output=True, text=True,
                             timeout=30).stdout
        temps = [float(t) for t in re.findall(r"(\d+) degrees C", out)]
        tol = float(plat.get("thermal_tolerance_c", 10))
        if not temps:
            notes.append("BMC 温度不可读——允许区间未核验（不拒）")
        elif max(temps) > float(amb) + tol:
            fails.append("热状态超允许区间: 实测峰值 %.1fC > %s+%sC"
                         % (max(temps), amb, tol))
        else:
            notes.append("热状态在允许区间内（峰值 %.1fC）" % max(temps))
    except (OSError, subprocess.SubprocessError):
        notes.append("ipmitool 不可用——热状态允许区间未核验（不拒）")

for n in notes:
    print("run.sh 预检注记: " + n, file=sys.stderr)
if fails:
    for f in fails:
        print("run.sh 预检拒绝: " + f, file=sys.stderr)
    sys.exit(1)
print("run.sh 预检通过: 内核/governor/online/拓扑指纹一致", file=sys.stderr)
'''

_RUN_SH_TEMPLATE = """#!/usr/bin/env bash
# repro capsule run.sh —— tools/excite/sdc_reproducer.py build_capsule 自动生成
# （v5 §10.2）。语义：manifest 预检（内核 release/governor/online 集/拓扑
# 指纹/BMC 热状态允许区间）全过 → 二进制哈希核验（binaries/ 引用不复制）→
# 受界重放（victim 先起、aggressor 并发——ProfileRunner.run_once 同序）。
# --check-only：只做预检与哈希核验，不启动任何负载。
set -u
CAPSULE_DIR="$(cd "$(dirname "$0")/.." && pwd)"
MANIFEST="$CAPSULE_DIR/manifest.json"
[ -f "$MANIFEST" ] || { echo "run.sh: 缺 $MANIFEST"; exit 4; }

python3 - "$MANIFEST" <<'PYEOF'
@PRECHECK@PYEOF
[ $? -ne 0 ] && exit 2

BIN="${SDC_BIN:-@BIN_PATH@}"
HASH="$(sha256sum "$BIN" 2>/dev/null | cut -d' ' -f1)"
[ -n "$HASH" ] && [ "$HASH" = "@BIN_SHA256@" ] || {
  echo "run.sh: 二进制哈希不符: $BIN ($HASH) != @BIN_SHA256@（capsule 引用不复制；SDC_BIN 可指向一致二进制）"
  exit 3
}

if [ "${1:-}" = "--check-only" ]; then
  echo "run.sh: 预检通过（--check-only，未启动负载）"
  exit 0
fi

@REPLAY@
exit $RC
"""


def _launch_line(bin_var, tag, cpus, test, extra):
    toks = [bin_var, "--cpuset=" + ",".join(str(c) for c in cpus),
            "-e", test, *extra, "-o", f'"$CAPSULE_DIR/replay/{tag}.yaml"']
    return " ".join(shlex.quote(t) if not t.startswith('"') else t for t in toks) + " &"


def _replay_block(resolved):
    """run.sh 重放段：victim 先起、aggressor 按族并发、wait 全收、
    rc 归并（victim 优先——run_once 同语义）。"""
    dur = int(resolved["limits"]["duration_s"])
    fams = list(resolved.get("aggressor_families") or [])
    mt = resolved.get("aggressor_max_threads")
    n_threads = max(1, mt // len(fams)) if (mt and fams) else None
    lines = ['mkdir -p "$CAPSULE_DIR/replay"']
    extra = ["-t", f"{dur}s"]
    if resolved.get("victim_seed"):
        extra += ["-s", str(resolved["victim_seed"])]
    lines.append(_launch_line('"$BIN"', "victim", resolved["victim_cpus"],
                              ",".join(resolved["victim_tests"]), extra))
    lines.append("VPID=$!")
    for fam in fams:
        tests = ",".join(resolved.get("aggressor_family_tests", {}).get(fam, []))
        extra = ["-t", f"{dur}s"] + (["-n", str(n_threads)] if n_threads else [])
        var = re.sub(r"\W", "_", fam)
        lines.append(_launch_line('"$BIN"', f"aggressor-{fam}",
                                  resolved["aggressor_cpus"], tests, extra))
        lines.append(f"PID_{var}=$!")
    lines += ['RC=0', 'wait "$VPID"; VRC=$?', '[ "$VRC" -ne 0 ] && RC=$VRC',
              f'echo "replay: victim_rc=$VRC"']
    for fam in fams:
        var = re.sub(r"\W", "_", fam)
        lines += [f'wait "$PID_{var}"; ARC=$?',
                  f'[ "$RC" -eq 0 ] && [ "$ARC" -ne 0 ] && RC=$ARC',
                  f'echo "replay: aggressor({fam})_rc=$ARC"']
    return "\n".join(lines)


def _write_sha256sums(out_dir):
    """capsule 全文件 SHA256SUMS（根目录，两空格格式；不含自身）。"""
    entries = []
    for base, _dirs, files in os.walk(out_dir):
        for name in files:
            p = os.path.join(base, name)
            rel = os.path.relpath(p, out_dir).replace(os.sep, "/")
            if rel == "SHA256SUMS":
                continue
            h = hashlib.sha256()
            with open(p, "rb") as f:
                for chunk in iter(lambda: f.read(1 << 16), b""):
                    h.update(chunk)
            entries.append(f"{h.hexdigest()}  {rel}")
    with open(os.path.join(out_dir, "SHA256SUMS"), "w",
              encoding="utf-8") as f:
        f.write("\n".join(sorted(entries)) + "\n")


def build_capsule(event_dir, resolved_profile, out_dir, gate_results=None):
    """v5 §10.2 复现胶囊打包 → manifest dict。

    binaries/ 只放 sha256+路径引用（500MB 级二进制不复制）；run.sh 以内容
    哈希核验重放机本机二进制（$SDC_BIN 覆盖路径）。JSON 裁定：v5 的
    manifest.yaml/hypotheses.yaml/trials.parquet 以 manifest.json/
    hypotheses.json/trials.jsonl 语义等价落地（零第三方依赖）。
    """
    _require(isinstance(resolved_profile, dict) and
             isinstance(resolved_profile.get("topology_snapshot"), dict) and
             isinstance(resolved_profile["topology_snapshot"].get("online"),
                        list), "resolved_profile 缺 topology_snapshot（resolve 产物口径）")
    for k in ("binary_path", "binary_hash", "victim_cpus", "victim_tests",
              "limits"):
        _require(k in resolved_profile, f"resolved_profile 缺 {k}")
    ev = parse_event(event_dir)
    topo = resolved_profile["topology_snapshot"]
    platform = _platform_probe(topo)              # 注入点：测试 mock
    os.makedirs(out_dir, exist_ok=True)
    for sub in ("binaries", "inputs", "golden", "original", "windows",
                "scripts", "reduction", "diagnosis"):
        os.makedirs(os.path.join(out_dir, sub), exist_ok=True)

    def _w(rel, text):
        with open(os.path.join(out_dir, rel), "w", encoding="utf-8") as f:
            f.write(text)

    def _cp(src_name, rel, note_if_missing=None):
        src = os.path.join(event_dir, src_name)
        if os.path.isfile(src):
            shutil.copyfile(src, os.path.join(out_dir, rel))
            return rel
        if note_if_missing:
            _w(rel, note_if_missing)
        return None

    bin_hash_hex = str(resolved_profile["binary_hash"]).split(":")[-1]
    manifest = {
        "schema_version": 1,
        "event_id": ev["event_id"],
        "created_at": _now(),
        "generator": "tools/excite/sdc_reproducer.py build_capsule",
        "source_event_dir": ev["dir"],
        "profile_id": resolved_profile.get("profile_id"),
        "binary": {
            "path": resolved_profile["binary_path"],
            "sha256": resolved_profile["binary_hash"],
            "note": "引用不复制（500MB 级）——run.sh 以内容哈希核验重放机"
                    "本机二进制，$SDC_BIN 可覆盖路径（binaries/sdcshield.sha256）",
        },
        "platform": platform,
        "topology_hash": topology_fingerprint(topo),
        "thermal_band": platform["thermal_band"],
        "victim": {"cpus": resolved_profile["victim_cpus"],
                   "tests": resolved_profile["victim_tests"],
                   "seed": resolved_profile.get("victim_seed"),
                   "seed_policy": resolved_profile.get("victim_seed_policy")},
        "replay": {"duration_s": resolved_profile["limits"]["duration_s"],
                   "aggressor_families": resolved_profile.get("aggressor_families"),
                   "order": "victim 先起、aggressor 并发（ProfileRunner.run_once 同序）"},
        "gate": gate_results if gate_results is not None else
        {"note": "authenticity_gate 由调用方（T5 队列消费）在打包前执行并注入"},
    }

    # original/：原始证据三件套复制——**先于 manifest 落盘**（M3 移交一行修：
    # original_evidence 键须在 _w(manifest.json) 前赋值，否则盘上 manifest
    # 缺该键——返回值有而文件无=证据链断言对不上）
    copied = []
    if _cp("stdout_summary.out", "original/stdout.log"):
        copied.append("original/stdout.log")
    if _cp("yaml_extract.txt", "original/result.yaml"):
        copied.append("original/result.yaml")
    if _cp("context.txt", "original/context.txt"):
        copied.append("original/context.txt")
    if _cp("retests.txt", "original/retests.txt"):
        copied.append("original/retests.txt")
    _w("original/stderr.log",
       "# 驱动以 2>&1 合并采集——stderr 无独立原件，全量见 stdout.log"
       "（as-built 口径，scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh）\n")
    manifest["original_evidence"] = copied

    # ---- 逐项落盘（v5 §10.2 目录结构） ----
    _w("manifest.json", json.dumps(manifest, ensure_ascii=False, indent=1))
    _w("resolved-profile.json",
       json.dumps(resolved_profile, ensure_ascii=False, indent=1))
    _w("topology.json", json.dumps(topo, ensure_ascii=False, indent=1))
    _w("platform.json", json.dumps(platform, ensure_ascii=False, indent=1))
    _w("binaries/sdcshield.sha256",
       f"{bin_hash_hex}  {resolved_profile['binary_path']}\n"
       "# 引用不复制（500MB 级）：capsule 记录内容哈希；重放机校验本机二进制"
       "一致后使用，SDC_BIN 可覆盖路径\n")
    _w("inputs/victim.json",
       json.dumps({"test": ev["test"], "seed": ev["seed"],
                   "seed_policy": "fixed",
                   "note": "事件失败 test/seed 冻结（from_event 同口径）——"
                           "复现条件建模 R 的 victim 输入"},
                  ensure_ascii=False, indent=1))
    _w("golden/README.txt",
       "golden 期望值内置于 sdcshield 二进制（框架 memcmp 自校验）——无外部"
       "golden 文件。本目录为 v5 §10.2 结构占位；§10.5 最小检测用例（毒药数据"
       "组合锁定）产出后落位于此。\n")
    _w("event.json", json.dumps(ev, ensure_ascii=False, indent=1))

    # windows/：context.txt 内嵌 monitor 尾样提取 + 不可得项如实注记
    mon_lines, in_mon = [], False
    for line in (open(os.path.join(event_dir, "context.txt"),
                      encoding="utf-8", errors="replace").read()
                 if os.path.isfile(os.path.join(event_dir, "context.txt"))
                 else "").splitlines():
        if line.startswith("--- monitor"):
            in_mon = True
            continue
        if in_mon and line.startswith("---"):
            break
        if in_mon:
            mon_lines.append(line)
    _w("windows/monitor_tail.csv",
       "\n".join(mon_lines) + ("\n" if mon_lines else ""))
    _w("windows/NOTES.md",
       "windows/ 取证窗口 as-built 口径：monitor_tail.csv 提取自 context.txt"
       "（事件时刻前 20 行采样）；kernel.log/sel.txt 由 root 快照通道"
       "（snapshot.request→dmesg/SEL/SDR）另行归档——事件目录无独立原件，"
       "不伪造占位（v5 §10.2 结构项以本注记如实交代）。\n")

    # scripts/：run.sh（预检+哈希核验+重放）/ verify.sh / restore.sh
    run_sh = (_RUN_SH_TEMPLATE
              .replace("@PRECHECK@", _RUN_SH_PRECHECK + "\n")
              .replace("@BIN_PATH@", resolved_profile["binary_path"])
              .replace("@BIN_SHA256@", bin_hash_hex)
              .replace("@REPLAY@", _replay_block(resolved_profile)))
    _w("scripts/run.sh", run_sh)
    os.chmod(os.path.join(out_dir, "scripts", "run.sh"), 0o755)
    _w("scripts/verify.sh",
       '#!/usr/bin/env bash\n'
       '# verify.sh —— 重放结果核验：replay/*.yaml 出现 result: fail/crash 行\n'
       '# → 判复现（exit 0）；全零 → 未复现（exit 1）；无产物 → exit 2。\n'
       'set -u\n'
       'CAPSULE_DIR="$(cd "$(dirname "$0")/.." && pwd)"\n'
       'fails=0\n'
       'found=0\n'
       'for y in "$CAPSULE_DIR"/replay/*.yaml; do\n'
       '  [ -f "$y" ] || continue\n'
       '  found=1\n'
       '  n=$(grep -Ec "result: *(fail|crash)" "$y" 2>/dev/null || true)\n'
       '  fails=$((fails + ${n:-0}))\n'
       'done\n'
       '[ "$found" -eq 1 ] || { echo "verify: 无重放产物（先跑 run.sh）"; exit 2; }\n'
       'if [ "$fails" -gt 0 ]; then\n'
       '  echo "verify: 复现（fail/crash 行 x$fails）"\n'
       '  exit 0\n'
       'fi\n'
       'echo "verify: 未复现（0 fail/crash 行）"\n'
       'exit 1\n')
    os.chmod(os.path.join(out_dir, "scripts", "verify.sh"), 0o755)
    online_csv = ",".join(str(c) for c in platform["online"])
    _w("scripts/restore.sh",
       '#!/usr/bin/env bash\n'
       '# restore.sh —— 重放后收敛（需 root；best-effort）：CPU online 集回\n'
       f'# manifest 基线（{online_csv}）。governor 恒 performance 不变式\n'
       '# （M3 安全不变式）——无需回切动作。\n'
       'set -u\n'
       f'for c in {online_csv}; do\n'
       '  if [ -e "/sys/devices/system/cpu/cpu$c/online" ]; then\n'
       '    echo 1 > "/sys/devices/system/cpu/cpu$c/online" 2>/dev/null || true\n'
       '  fi\n'
       'done\n'
       'echo "restore: online 集已按 manifest 基线收敛（best-effort）"\n')
    os.chmod(os.path.join(out_dir, "scripts", "restore.sh"), 0o755)

    # reduction/ + diagnosis/（T4/M4 产出的落点——结构占位如实注明）
    _w("reduction/graph.jsonl",
       '{"_comment": "概率 ddmin 搜索树（v5 §10.3/T4 sdc_reducer）逐行落点——'
       'capsule 冻结复现条件起点"}\n')
    _w("reduction/trials.jsonl",
       '{"_comment": "序贯试验记录（run_cycle/概率 ddmin 落点；v5 §10.2 '
       'trials.parquet 的 stdlib JSONL 语义等价——零第三方依赖裁定）"}\n')
    _w("diagnosis/hypotheses.json",
       json.dumps({"schema_version": 1, "hypotheses": [],
                   "note": "M4 假设矩阵（v5 §11）产出落点——结构占位"},
                  ensure_ascii=False, indent=1))
    gate_txt = (json.dumps(gate_results, ensure_ascii=False, indent=1)
                if gate_results is not None
                else "见调用方审计（authenticity_gate 于打包前执行——T5 队列消费）")
    _w("diagnosis/report.md",
       f"# 复现胶囊诊断报告 — {ev['event_id']}\n\n"
       f"- 事件: {ev['event_id']}（源目录 {ev['dir']}）\n"
       f"- victim: test={ev['test']} seed={ev['seed']} cpus="
       f"{resolved_profile['victim_cpus']}\n"
       f"- 平台: kernel {platform['kernel_release']}, governor "
       f"{platform['governor']}"
       f"（has_cpufreq={platform['has_cpufreq']}）, online "
       f"{len(platform['online'])} cpus, 拓扑指纹 "
       f"{manifest['topology_hash'][:12]}…\n"
       f"- 二进制: {resolved_profile['binary_path']}（"
       f"{resolved_profile['binary_hash'][:22]}…——引用不复制）\n"
       f"- 热状态允许区间: {platform['thermal_band']}\n"
       f"- 门禁: {gate_txt}\n"
       f"- 证据: {', '.join(copied) or '（无——门禁1 应已拒）'}\n\n"
       "## 复现概率（run_cycle）\n\n"
       "由 Reproducer.run_cycle / T4 概率 ddmin 填充——capsule 只冻结条件。\n")

    _write_sha256sums(out_dir)
    return manifest


# ---------------------------------------------------------------------------
# Reproducer：from_event + run_cycle

class Reproducer:
    """事件驱动的复现器。from_event 构造最小 victim 单测 resolved profile；
    run_cycle 做 n 次受界运行的概率复测。注入点：_run_once（测试 FakeReproducer
    覆写——绝不真跑 sdcshield）。"""

    def __init__(self, data_root, event_dir):
        self.data_root = data_root
        self.event_dir = event_dir
        self.event = parse_event(event_dir)

    def from_event(self, capabilities=None, topology_snapshot=None,
                   aggressor_families=("cache",)):
        """事件 test/seed → 最小 victim 单测 resolved profile（复现条件建模
        R 的最小起点，v5 §10.4）。

        victim = 失败 test+seed 锚定 detecting-cpu（无锚点/无种子→响亮拒绝：
        wholecmd 重放语义不属 from_event——诚实优先，不静默换条件）；aggressor
        = 单 cache 族最小结构化干扰（原事件的全机负载为上界，概率 ddmin（T4）
        在两者之间收缩）。duration=120s 与驱动定向复测同款受界纪律。
        capabilities/topology 缺省从 data_root/capabilities.env + 实时 sysfs
        快照取（resolve 冻结口径，sdc_profile.resolve 单一权威）。"""
        ev = self.event
        _require(ev["test"], "事件未提取失败测试名——无法构造 victim profile")
        _require(ev["seed"], "事件无失败种子记录——无法定向复现"
                             "（wholecmd 重放不属 from_event）")
        if capabilities is None:
            caps_path = os.path.join(self.data_root, "capabilities.env")
            _require(os.path.isfile(caps_path),
                     f"缺 capabilities 且未显式给定: {caps_path}"
                     f"（先跑 collect_inventory.sh 或传 capabilities 参数）")
            capabilities = sdc_profile.load_capabilities(caps_path)
        if topology_snapshot is None:
            topology_snapshot = sdc_topology.snapshot()
        online = topology_snapshot["online"]
        anchor = [c for c in ev["detecting_cpus"] if c in set(online)]
        _require(anchor, "事件无失败核锚点（detecting-cpu/cpu-mask 均缺或不在"
                         " online 集）——先人工定核再复现")
        profile = {
            "schema_version": 1,
            "profile_id": f"repro-{ev['event_id']}",
            "victim": {"cpus": [anchor[0]], "tests": [ev["test"]],
                       "seed_policy": "fixed", "seed": ev["seed"],
                       "iterations_per_seed": 1},
            "aggressors": {"topology": "all_except_victim",
                           "families": list(aggressor_families),
                           "duty_cycle": 1.0, "max_threads": 16},
            "environment": {"governor": "performance"},
            "monitoring": {"pmu_groups": ["core_base"], "steady_period_ms": 1000},
            "safety_policy": "lab_default_v1",
            "limits": {"duration_s": 120, "max_failures": 3},
        }
        return sdc_profile.resolve(profile, capabilities, topology_snapshot)

    def _run_once(self, profile):
        """一次受界运行（真实实现；测试注入点）。返回 run_once 结果 +
        fail_count（本轮新产 yaml 的 result: fail/crash 行数——以文件集差分
        精确归属本轮，避免秒级 mtime 粒度的双计）。"""
        runs = os.path.join(self.data_root, "runs")
        pat = os.path.join(runs, "*.yaml")
        before = set(glob.glob(pat))
        runner = sdc_profile.ProfileRunner(profile, self.data_root)
        result = runner.run_once()
        fail_count = 0
        for y in set(glob.glob(pat)) - before:
            try:
                with open(y, encoding="utf-8", errors="replace") as f:
                    fail_count += sum(1 for line in f
                                      if re.match(r"^\s*result:\s*(fail|crash)\b",
                                                  line))
            except OSError:
                pass
        return {**result, "fail_count": fail_count}

    def run_cycle(self, profile, n_trials):
        """n 次受界运行 → {"k","n","ci","attempts","invalid"}。

        失败口径：fail/crash 行>0 或 rc≠0；rc=250（内存门拦截）= 无效试验
        不计入 n（有界重试 MAX_INVALID_TRIALS，恒收集不到 n 个有效试验时
        如实以已收集数返回，n=0 → 真空区间 (0.0, 1.0)）。
        轴编排约束（T2 移交②）：本方法不执行任何轴——profile 启用轴即
        ValueError（cpu_hotplug 的离线动作只允许在 victim/aggressor 全部
        退出后的收尾窗口，M3 未实现该编排；静默丢弃轴=谎报复现条件）。"""
        _require(isinstance(n_trials, int) and not isinstance(n_trials, bool)
                 and n_trials > 0, f"n_trials 必须为正整数: {n_trials!r}")
        enabled = []
        for name, spec in ((profile or {}).get("axes") or {}).items():
            en = spec.get("enabled") if isinstance(spec, dict) else bool(spec)
            if en:
                enabled.append(name)
        if enabled:
            raise ValueError(
                f"run_cycle 不编排轴（启用: {enabled}）——M3 裁定：轴编排"
                "未接入 run_cycle（cpu_hotplug 的离线动作只允许在 "
                "victim/aggressor 全部退出后的收尾窗口，T2 移交②），"
                "拒绝而非静默丢弃复现条件")
        k = n = invalid = attempts = 0
        ceiling = n_trials + MAX_INVALID_TRIALS
        while n < n_trials and attempts < ceiling:
            attempts += 1
            r = self._run_once(profile)
            if r.get("rc") == MEMGUARD_RC:
                invalid += 1
                continue
            n += 1
            if r.get("rc") != 0 or r.get("fail_count", 0) > 0:
                k += 1
        ci = wilson_ci(k, n) if n else (0.0, 1.0)
        return {"k": k, "n": n, "ci": ci, "attempts": attempts,
                "invalid": invalid}


# ---------------------------------------------------------------------------
# 复现队列消费（M3 T5：M2 enqueue_reproduction → repro_queue → 闭环）

QUEUE_EVENT_BUDGET_S = 300          # 单事件概率复测总预算（≤5 min，受界纪律）
QUEUE_TRIALS = 3                    # 复测次数（初判；v5 §10.1.7 概率口径）
QUEUE_SUBDIR = "repro_queue"        # 生产端：驱动 enqueue_repro（sdc_common.sh）
DONE_SUBDIR = "repro_done"
CAPSULE_SUBDIR = "repro_capsules"


def _queue_sort_key(name):
    """ts 序以文件名为准：epoch-ns 数值序优先；非数值名（异常形态）字典序殿后。"""
    stem = name[:-len(".json")]
    return (0, int(stem), "") if stem.isdigit() else (1, 0, stem)


def _finish_queue_entry(rec, done_path, qpath):
    """done 先落（tmp+os.replace 原子写）再删队列文件——两步间崩溃 → 下轮
    already_done 幂等收敛（复测是有界负载，不得无理由重跑）。"""
    tmp = done_path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        json.dump(rec, f, ensure_ascii=False, indent=1)
    os.replace(tmp, done_path)
    os.unlink(qpath)


def _consume_queue_entry(data_root, qpath, name, done_path,
                         capabilities, topology_snapshot):
    """单个队列条目 → 结果行（写 done + 删队列；already_done 只删队列）。

    状态：processed / gate_rejected / invalid（毒丸 JSON，原文截留入案）/
    error（from_event 响亮拒绝等）/ already_done（崩溃恢复，不重跑）。
    gate 拒亦移 done（拒因入案）——不重试（消费端一次定案，重试属人工裁量）。
    """
    stem = name[:-len(".json")]
    rec = {"id": stem, "status": None, "event_id": None, "ts": _now()}
    with open(qpath, encoding="utf-8", errors="replace") as f:
        raw = f.read()
    try:
        entry = json.loads(raw)
        if not isinstance(entry, dict) or not entry.get("event_dir"):
            raise ValueError(f"记录非 JSON 对象或缺 event_dir: {raw!r}")
    except ValueError as e:              # 毒丸 JSON（bash printf 转义残留形态等）
        rec.update(status="invalid",
                   error=f"队列记录非法 JSON: {e}——原文截留: {raw}")
        rec["raw"] = raw
        _finish_queue_entry(rec, done_path, qpath)
        return rec

    event_dir = entry["event_dir"]
    rec["event_dir"] = event_dir
    rec["event_id"] = os.path.basename(os.path.normpath(event_dir))
    if os.path.exists(done_path):        # 崩溃恢复：done 已在，只清队列不重跑
        rec["status"] = "already_done"
        os.unlink(qpath)
        return rec
    try:
        rep = Reproducer(data_root, event_dir)
        resolved = rep.from_event(capabilities, topology_snapshot)
        ctx = {"binary": resolved["binary_path"]}
        det = set(rep.event["detecting_cpus"])
        healthy = [c for c in resolved["topology_snapshot"]["online"]
                   if c not in det]
        if healthy:                      # 健康核对照（门禁4 真实发射，受界 120s）
            ctx["control_cpus"] = healthy[:1]
        ok, reasons = authenticity_gate(event_dir, ctx)
        rec["gate"] = {"ok": ok, "reasons": reasons}
        if not ok:
            rec["status"] = "gate_rejected"
        else:
            per_trial = QUEUE_EVENT_BUDGET_S // QUEUE_TRIALS
            if resolved["limits"]["duration_s"] > per_trial:
                resolved["limits"]["duration_s"] = per_trial   # ≤5min 预算截断
            capsule_dir = os.path.join(data_root, "spool", CAPSULE_SUBDIR,
                                       rec["event_id"])
            build_capsule(event_dir, resolved, capsule_dir,
                          gate_results=rec["gate"])   # 门禁结果注入 manifest
            cycle = rep.run_cycle(resolved, QUEUE_TRIALS)
            rec["status"] = "processed"
            rec["capsule_dir"] = capsule_dir
            rec["repro"] = {**cycle, "ci": list(cycle["ci"])}
    except ValueError as e:              # from_event 响亮拒绝（无种子/无锚点等）
        rec["status"] = "error"
        rec["error"] = str(e)
    except Exception as e:               # 意外异常同样入案隔离——不炸队列扫描
        rec["status"] = "error"
        rec["error"] = f"{type(e).__name__}: {e}"
    _finish_queue_entry(rec, done_path, qpath)
    return rec


def consume_queue(data_root, once=False, capabilities=None,
                  topology_snapshot=None):
    """扫 spool/repro_queue/（epoch-ns 文件名数值序）→ 逐事件
    from_event→真实性门禁→（过）build_capsule + 概率复测 QUEUE_TRIALS 次
    （单事件总预算 QUEUE_EVENT_BUDGET_S，超出按 per-trial 截断）→ 结果行写
    spool/repro_done/<队列文件名> → 删队列文件。返回结果行列表（CLI 汇报用）。

    capabilities/topology 缺省由 from_event 从 data_root/capabilities.env +
    实时 sysfs 快照取（resolve 冻结单一权威）。注入点同 Reproducer._run_once /
    _control_run（测试全 mock——战役运行中绝不真发射负载）。
    """
    qdir = os.path.join(data_root, "spool", QUEUE_SUBDIR)
    ddir = os.path.join(data_root, "spool", DONE_SUBDIR)
    os.makedirs(ddir, exist_ok=True)
    names = []
    if os.path.isdir(qdir):
        names = sorted((n for n in os.listdir(qdir) if n.endswith(".json")),
                       key=_queue_sort_key)
    results = []
    for name in names:
        results.append(_consume_queue_entry(
            data_root, os.path.join(qdir, name), name,
            os.path.join(ddir, name), capabilities, topology_snapshot))
        if once:                         # --once：处理一个即退（轮询单步）
            break
    return results


# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        prog="sdc_reproducer.py",
        description="sdc_reproducer——复现 runner + 真实性门禁 + 复现胶囊"
                    "（v5 §10.1-10.2，M3）")
    # --from-queue 为模式旗标而非子命令（子命令名不得带 -- 前缀）；与
    # gate/capsule 子命令互斥，裸调用响亮报错
    ap.add_argument("--from-queue", action="store_true",
                    help="消费 spool/repro_queue（M2 enqueue_reproduction 落点，"
                         "M3 T5）；与子命令 gate/capsule 互斥")
    ap.add_argument("--once", action="store_true",
                    help="（--from-queue）处理一个事件即退（轮询模式单步）")
    ap.add_argument("--data-root", default=None,
                    help="（--from-queue）数据根（默认 $SDC_EXCITE_REPRODUCE_DIR"
                         "/$SDC_CAMPAIGN_DIR/~/sdc-excite-reproduce）")
    sub = ap.add_subparsers(dest="cmd")
    gp = sub.add_parser("gate", help="对事件目录跑七项真实性门禁")
    gp.add_argument("event_dir")
    gp.add_argument("--control-cpus", default=None,
                    help="健康核对照 cpuset（逗号集；缺省不跑对照→警告）")
    gp.add_argument("--binary", default=None, help="对照用二进制（缺省取事件 cmd）")
    gp.add_argument("--sham-k", type=int, default=None,
                    help="sham-control 复现数（与 --sham-n 成对）")
    gp.add_argument("--sham-n", type=int, default=None)
    cp = sub.add_parser("capsule", help="打包复现胶囊（v5 §10.2）")
    cp.add_argument("event_dir")
    cp.add_argument("out_dir")
    cp.add_argument("--resolved", required=True,
                    help="resolved-profile JSON（sdc_profile.py run --dry-run 产）")
    a = ap.parse_args(argv)

    if a.from_queue and a.cmd:
        ap.error("--from-queue 与子命令 gate/capsule 互斥")

    if a.from_queue:
        root = a.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") \
            or os.environ.get("SDC_CAMPAIGN_DIR") \
            or os.path.expanduser("~/sdc-excite-reproduce")
        results = consume_queue(root, once=a.once)
        if not results:
            print(f"队列为空（{os.path.join(root, 'spool', QUEUE_SUBDIR)} "
                  "无待处理事件）")
            return 0
        bad = 0
        for r in results:
            if r["status"] == "processed":
                cyc = r["repro"]
                print(f"[{r['id']}] processed: event={r['event_id']} "
                      f"复测 k={cyc['k']}/{cyc['n']} "
                      f"ci=({cyc['ci'][0]:.4f},{cyc['ci'][1]:.4f}) "
                      f"capsule={r['capsule_dir']}")
            elif r["status"] == "gate_rejected":
                print(f"[{r['id']}] gate_rejected: event={r['event_id']}（不重试）")
                for reason in r["gate"]["reasons"]:
                    print("  " + reason)
            elif r["status"] == "already_done":
                print(f"[{r['id']}] already_done: event={r['event_id']}"
                      "（done 已存在，只清队列不重跑）")
            else:                        # invalid / error——rc=1 诚实汇报
                bad += 1
                where = f" event={r['event_id']}" if r.get("event_id") else ""
                print(f"[{r['id']}] {r['status']}{where}: {r['error']}")
        return 1 if bad else 0

    if a.cmd == "gate":
        ctx = {}
        if a.control_cpus:
            ctx["control_cpus"] = [int(x) for x in a.control_cpus.split(",")]
        if a.binary:
            ctx["binary"] = a.binary
        if a.sham_n is not None:
            ctx["sham_result"] = {"k": a.sham_k or 0, "n": a.sham_n}
        ok, reasons = authenticity_gate(a.event_dir, ctx)
        for r in reasons:
            print(r)
        print(f"authenticity_gate: {'通过（含警告）' if ok else '未通过'} "
              f"（{len(reasons)} 条问题行）")
        return 0 if ok else 1

    if a.cmd != "capsule":
        ap.error("需指定 --from-queue 或子命令 gate/capsule")

    with open(a.resolved, encoding="utf-8") as f:
        resolved = json.load(f)
    manifest = build_capsule(a.event_dir, resolved, a.out_dir)
    print(json.dumps({k: manifest[k] for k in
                      ("event_id", "profile_id", "topology_hash",
                       "thermal_band")}, ensure_ascii=False, indent=1))
    print(f"capsule: {os.path.abspath(a.out_dir)}（SHA256SUMS 全文件校验就绪）")
    return 0


if __name__ == "__main__":
    sys.exit(main())

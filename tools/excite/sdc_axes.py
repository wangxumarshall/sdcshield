#!/usr/bin/env python3
"""sdc_axes.py — 四轴策略执行器（v5 §7.4，M3 Task 2）。stdlib only。

三类 axis_spec（对齐 v5 §7.4 四轴中 M3 可执行的三轴——电压骤变 §7.4.2 为 R3
默认禁用，M3 不实现）：

  ① governor_experiment  {"from": "performance", "to": "powersave", "hold_s": 60}
     **M3 裁定（期望拒绝是显式行为不是缺陷）**：governor 走 root monitor 现有
     代理（cmd/governor.request，原始字符串协议——cat 后与 "performance" 等值
     比较），代理只接受 performance（用户指令 2026-09-24：全核恒 performance）。
     root-helper allowlist 无 governor 动作（M2 范围裁定）。故本轴实现为：
     物理能力门（GOVERNOR_WRITABLE/HAS_CPUFREQ=yes，与 sdc_profile.resolve
     同键——单一判据）通过后**写非 performance 请求 + 等待 `.rejected.*` 终态
     + 返回 capability_gap_rejected + 审计入案**——能力缺口如实暴露，不静默
     跳过也不谎报成功。实验性 governor 切换延后至用户显式授权扩展 allowlist
     （v5 §7.4.1 R1 实验档需独立授权）。
  ② load_shaping  {"pattern": "burst", "duty": 0.5, "period_s": 20,
     "duration_s": 300}——_make_loadplan() 纯函数产出时间段表
     [{"t_on_s":..,"t_off_s":..}, ...]，执行编排逐段经注入点
     _aggressor_burst(t_on, t_off)（默认实现复用 sdc_profile.ProfileRunner 的
     受界发射：power_virus_dit 专用 di/dt 载荷、-t 整秒自停、-n ≤16 并发上限、
     retest_guarded 内存护栏；spec.cpus 必填——M3 纪律拒绝全核新负载）。
  ③ cpu_hotplug  {"op": "offline", "cpus": [96,97], "then": "online",
     "hold_s": 30}——写 cmd/cpu_hotplug.request（helper JSON schema）→ 本地
     预校验（直接复用 sdc_root_helper.validate——请求 schema 单一权威，M2
     helper 侧同判据再验）→ 等 `.done`/`.rejected`/`.expired` 终态。then 语义：
     前置 done → hold → 反向操作；前置 timeout（状态未知）→ 跳过 hold 直接
     尝试收敛（恢复语义）；前置明确未执行（rejected/expired/error）→ then
     skipped 入案。run 边界（v5 §7.4.4）：helper 侧拒绝对运行中 sdcshield/
     stress-ng 的 cpu_offline——本机战役运行中该拒绝是 as-built 真实语义。

文件协议终态两代命名统一收（_wait_request 扫描）：
  helper 通道  <req>.request → <req>.request.<status>.<epoch>（os.replace 改名）
  monitor 代理 governor.request → governor.<status>.<epoch>（mv 改名）
终态归属判据 = **inode 同一性**：两类消费者的终态化都是同目录 rename（monitor
sdc_monitor.sh mv / helper _mv os.replace——inode 不变），而每次写请求是
tmp+replace 全新 inode。故"终态文件的 inode == 本次请求的 inode"精确等价于
"这是我发的请求的终态"——上一轮的终态（旧 inode）、同秒陈旧终态、并发写者的
请求一律天然排除。不依赖文件时间戳比较（实测教训：fs ctime 是粗粒度时钟
——本机滞后 ~1.7ms——与 time.time_ns() 精细时钟比较恒假阴性；秒级 epoch
下限又漏同秒窗口，inode 是唯一无窗口判据）。expires_at 与等待超时对齐
（-2s helper 轮询余量）：执行器放弃的请求 helper 侧到期拒绝，绝不迟到执行。

注入点（测试全 mock，绝不真 hotplug/真 governor/真负载——战役+9 服务运行中）：
_wait_request / _read_online / _hold / _aggressor_burst。
审计：每个相位结果一行入 spool/axes_audit.jsonl（append-only，含被拒/跳过/
超时——被拒绝的尝试也是安全事件），结果 dict 回填 "audit" 引用。
"""
import glob, json, math, os, re, sys, time, uuid
from datetime import datetime, timedelta, timezone

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                "..", "control"))
import sdc_root_helper                      # M2 helper——validate/_parse_cpu_list 复用
import sdc_profile                          # T1——默认 _aggressor_burst 的受界发射复用

AXES = ("governor_experiment", "load_shaping", "cpu_hotplug")
AXES_AUDIT_FILE = "axes_audit.jsonl"
PRE_STATE_FILE = sdc_root_helper.PRE_STATE_FILE   # hotplug 前置快照（restore 读回）

GOVERNOR_INVARIANT = "performance"          # M3 安全不变式：恒 performance
GOVERNOR_WAIT_S = 120                       # monitor 轮询 ≤60s + 余量
HOTPLUG_WAIT_S = 120                        # helper 轮询 2s——余量充足
RESTORE_WAIT_S = 120
EXPIRY_SLACK_S = 2                          # expires_at = 提交 + 等待 - 2（helper
                                            # 轮询余量：终态化先于我方超时落地）
WAIT_POLL_S = 0.05

DEFAULT_AGGRESSOR_TEST = "power_virus_dit"  # v5 §7.4.3 专用 di/dt 载荷（--list-tests 实名）
DEFAULT_AGGRESSOR_THREADS = 16              # M3 受界纪律：aggressor 总并发 ≤16
MAX_AGGRESSOR_THREADS = 16

# 终态文件名尾缀：.done/.rejected/.expired/.error/.readback_mismatch.<epoch>
# （helper _mv 的 status 集 + monitor governor 代理的 done/rejected）
TERMINAL_RE = re.compile(
    r"\.(done|rejected|expired|error|readback_mismatch)\.(\d+)$")


def _now():
    return time.strftime("%F %T")


def _require(cond, msg):
    if not cond:
        raise ValueError(msg)


def _num(v, what, low, high=None):
    """数值校验（bool 不算数）：low < v ≤ high（high=None 无上界）。"""
    _require(isinstance(v, (int, float)) and not isinstance(v, bool) and v > low
             and (high is None or v <= high),
             f"{what} 必须为 > {low}" + (f" 且 ≤ {high}" if high is not None else "")
             + f" 的数值: {v!r}")
    return v


def _int_at_least(v, what, low):
    _require(isinstance(v, int) and not isinstance(v, bool) and v >= low,
             f"{what} 必须为 ≥ {low} 的整数: {v!r}")
    return v


def _cpu_list(v, what):
    _require(isinstance(v, list) and v and
             all(isinstance(c, int) and not isinstance(c, bool) for c in v),
             f"{what} 必须为非空 int 列表: {v!r}")
    return list(v)


# ---------------------------------------------------------------------------
# 负载整形计划（纯函数——v5 §7.4.3：功耗瞬变作为负载整形的结果产生）

def _make_loadplan(pattern, duty, period_s, duration_s):
    """burst 计划：时间段表 [{"t_on_s", "t_off_s"}, ...]（ms 圆整，消除浮点噪声）。

    每周期 on=duty*period / off=(1-duty)*period；整周期数 floor(duration/period)，
    余量按同占空比成尾段（duration < period → 单段）。边界：duty=1.0 → t_off=0
    （恒满载）；duty=0.05 → 1s on / 19s off（period=20）。未知 pattern / 越界
    参数 → ValueError（配置错误响亮失败）。秒值圆整到 1ms——on+off 与
    period/余量的偏差 ≤0.001s。
    """
    _require(pattern == "burst",
             f"pattern 必须为 'burst'（M3 仅实现 burst 阶跃，v5 §7.4.3）: {pattern!r}")
    _num(duty, "duty", 0, 1)
    _num(period_s, "period_s", 0)
    _num(duration_s, "duration_s", 0)
    plan = []
    n_full = int(duration_s // period_s)
    for _ in range(n_full):
        t_on = round(duty * period_s, 3)
        plan.append({"t_on_s": t_on, "t_off_s": round(period_s - t_on, 3)})
    rest = duration_s - n_full * period_s
    if rest > 1e-9:                          # 余量尾段：同占空比（诚实保形，不截断）
        t_on = round(duty * rest, 3)
        plan.append({"t_on_s": t_on, "t_off_s": round(rest - duty * rest, 3)})
    return plan                              # duration_s > 0 已验 → 恒 ≥1 段


# ---------------------------------------------------------------------------
# 四轴执行器

class AxesExecutor:
    """execute(axis_spec) -> list[结果 dict]（每相位一条，全量入 axes 审计）；
    restore_all() -> list[结果]（restore.request + 读回断言）。

    注入点：_wait_request（文件协议等待）/ _read_online（sysfs 读回）/
    _hold（相位间保持）/ _aggressor_burst（负载段执行）——单测全 mock。
    """

    def __init__(self, data_root, capabilities):
        _require(isinstance(capabilities, dict),
                 f"capabilities 必须为 dict（load_capabilities 产物）: "
                 f"{type(capabilities).__name__}")
        self.data_root = data_root
        self.capabilities = capabilities
        self.cmd_dir = os.path.join(data_root, "cmd")
        os.makedirs(self.cmd_dir, exist_ok=True)
        os.makedirs(os.path.join(data_root, "spool"), exist_ok=True)
        self._submit_ident = None            # 最近一次请求提交的 (dev, ino)——
                                              # _wait_request 终态归属判据（见模块头）
        self._active_load_spec = None        # load_shaping 执行期spec（默认 burst 读）

    # ---- 注入点 ----

    def _wait_request(self, cmd_dir, prefix, action_id, timeout):
        """等请求终态：扫 <stem>.* 的 `.<status>.<epoch>` 尾缀，返回 (status, path)。
        超时 → ("timeout", None)。终态归属 = inode 同一性（终态文件的
        (dev, ino) == _write_request 记录的本次请求 inode——rename 终态化保
        inode，见模块头）：旧轮终态/同秒陈旧终态/并发写者的请求天然排除。
        无 _submit_ident（未先 _write_request）→ 恒 timeout（安全默认）。"""
        stem = f"{prefix}-{action_id}" if action_id else prefix
        deadline = time.monotonic() + timeout
        while True:
            best = None
            for p in glob.glob(os.path.join(cmd_dir, stem + ".*")):
                m = TERMINAL_RE.search(os.path.basename(p))
                if not m:
                    continue                  # 悬挂请求/tmp——非终态
                try:
                    st = os.stat(p)
                except OSError:
                    continue                  # 竞态消失——下轮再看
                if self._submit_ident is None or \
                        (st.st_dev, st.st_ino) != self._submit_ident:
                    continue                  # 不是本次请求的 inode——陈旧/他人终态
                key = (int(m.group(2)), st.st_ctime_ns)
                if best is None or key > best[0]:
                    best = (key, m.group(1), p)
            if best:
                return best[1], best[2]
            if time.monotonic() >= deadline:
                return "timeout", None
            time.sleep(WAIT_POLL_S)

    def _read_online(self):
        """读回 /sys/devices/system/cpu/online（注入点：测试 mock）。"""
        with open(os.path.join(sdc_root_helper.SYSFS_CPU, "online")) as f:
            return f.read().strip()

    def _hold(self, seconds):
        """相位间保持（注入点）。"""
        time.sleep(seconds)

    def _aggressor_burst(self, t_on, t_off):
        """默认实现：单段受界 burst——复用 sdc_profile.ProfileRunner 的发射约定
        （retest_guarded 内存护栏 + env -u SDC_ROOT_PW + killpg 可收整树）。
        t_on 向上取整整秒（sdcshield -t 不接受小数，0.5s 实测 invalid time）；
        -n ≤16（M3 受界纪律）；t_off 空转 hold。返回 wrapper rc。
        ProfileRunner 仅消费 resolved 的 binary_path/limits.duration_s 两键
        （对 T1 源码核实——run_once 不走，无完整 resolved 需求）。"""
        spec = self._active_load_spec or {}
        cpus = spec.get("cpus")
        _require(cpus, "load_shaping 执行需 spec.cpus（M3 受界纪律：拒绝全核"
                       "新负载——显式给定限界核集）")
        test = spec.get("test") or DEFAULT_AGGRESSOR_TEST
        t_on_i = max(1, math.ceil(t_on))
        mt = spec.get("max_threads", DEFAULT_AGGRESSOR_THREADS)
        runner = sdc_profile.ProfileRunner(
            {"binary_path": self._bin_path(), "limits": {"duration_s": t_on_i}},
            self.data_root)
        proc = runner._launch("aggressor", list(cpus), test,
                              ["-t", f"{t_on_i}s", "-n", str(mt)])
        try:
            rc = runner._await(proc, t_on_i + sdc_profile.AWAIT_GRACE_S)
        finally:
            runner._cleanup_orphans([proc])   # 异常路径也收残（沙盒限本 run pid）
        if t_off and t_off > 0:
            self._hold(t_off)
        return rc

    def _bin_path(self):
        return os.environ.get("SDC_BIN") or os.path.join(sdc_profile.REPO_DIR,
                                                         "builddir", "sdcshield")

    # ---- 内部：请求写入 + 审计 ----

    def _write_request(self, name, text):
        """原子写请求（tmp + os.replace——消费者不读半截）+ 记录请求 inode
        （_wait_request 终态归属判据：rename 终态化保 inode，见模块头）。
        inode 取自 replace **之前**的 tmp：rename 保 inode，tmp 的 inode 即
        提交后 path 的 inode——关闭 replace→stat(path) 之间并发写者换掉
        path 的竞态窗口（旧实现会误领并发写者的 inode 为本方请求）。"""
        path = os.path.join(self.cmd_dir, name)
        tmp = path + ".tmp"
        with open(tmp, "w", encoding="utf-8") as f:
            f.write(text)
        st = os.stat(tmp)
        os.replace(tmp, path)
        self._submit_ident = (st.st_dev, st.st_ino)

    def _expires_at(self, wait_s):
        """请求期限 = 提交 + 等待 - 2s（helper 轮询余量）：我方超时放弃的请求
        helper 侧到期拒绝——绝不迟到执行（半执行窗口 ≤ 轮询间隔）。"""
        return (datetime.now(timezone.utc)
                + timedelta(seconds=max(5, wait_s - EXPIRY_SLACK_S))).isoformat()

    def _record(self, result):
        """结果 → axes 审计行（append-only；被拒/跳过/超时同样入案）+ 回填
        result["audit"] 引用（可追溯）。"""
        line = {"ts": _now(), **result}
        with open(os.path.join(self.data_root, "spool", AXES_AUDIT_FILE),
                  "a", encoding="utf-8") as f:
            f.write(json.dumps(line, ensure_ascii=False) + "\n")
        result["audit"] = dict(line)
        return result

    def _cap_gap(self, axis, keys, extra=None):
        """能力门：任一键非 yes（unknown 同拒——诚实）→ 返回缺口结果 dict
        （已入审计）；全部就绪 → None（放行）。"""
        missing = [f"{k}={self.capabilities.get(k)!r}" for k in keys
                   if self.capabilities.get(k) != "yes"]
        if not missing:
            return None
        return self._record({
            "axis": axis, "phase": "capability_gate", "status": "capability_gap",
            "reason": "能力缺口: " + " ".join(missing)
                      + "（unknown=未探测同样拒绝——能力探测诚实，v5 §7.4；"
                        "物理不可用不写请求）",
            **(extra or {})})

    # ---- 轴分发 ----

    def execute(self, axis_spec):
        """执行一个 axis_spec → 每相位一条结果（全量入审计）。配置错误
        ValueError 响亮失败；执行期政策性结果（拒绝/缺口/超时）如实入案。"""
        _require(isinstance(axis_spec, dict),
                 f"axis_spec 必须为 dict: {type(axis_spec).__name__}")
        axis = axis_spec.get("axis")
        if axis == "governor_experiment":
            return self._execute_governor(axis_spec)
        if axis == "load_shaping":
            return self._execute_load_shaping(axis_spec)
        if axis == "cpu_hotplug":
            return self._execute_hotplug(axis_spec)
        raise ValueError(f"未知轴 {axis!r}（M3 可执行轴: {AXES}；"
                         "电压骤变 v5 §7.4.2 R3 默认禁用不在 M3 范围）")

    # ① governor 实验档：写请求 + 期望拒绝 + 审计（M3 裁定，见模块头）
    def _execute_governor(self, spec):
        frm, to = spec.get("from"), spec.get("to")
        _require(isinstance(frm, str) and frm, "governor_experiment 缺 from（str）")
        _require(frm == GOVERNOR_INVARIANT,
                 f"governor 实验档起点必须为 {GOVERNOR_INVARIANT!r}（M3 安全"
                 f"不变式：环境恒 performance——非 performance 起点意味着不变式"
                 f"已被破，拒绝在此基线上实验）: {frm!r}")
        _require(isinstance(to, str) and to and "\n" not in to,
                 f"to 必须为单行非空 str: {to!r}")
        _require(not sdc_root_helper._metachar_hit(to),
                 f"to 含 shell 元字符，拒绝: {to!r}")
        if "hold_s" in spec:
            _int_at_least(spec["hold_s"], "hold_s", 0)
        # hold_s 在 M3 恒不被消费：非 performance 恒被拒（无从保持）、performance
        # →performance 为幂等空操作——真实"切换-保持-回切"属 v5 §7.4.1 实验档，
        # 待独立授权扩展 allowlist 后启用；此处仅校验+入审计（诚实记录未执行）。
        gap = self._cap_gap("governor_experiment",
                            ("GOVERNOR_WRITABLE", "HAS_CPUFREQ"),
                            extra={"from": frm, "to": to, "hold_s": spec.get("hold_s")})
        if gap:
            return [gap]

        # 原始字符串协议（monitor 代理 cat 后与 performance 等值比较）
        self._write_request("governor.request", to + "\n")
        status, term = self._wait_request(self.cmd_dir, "governor", None,
                                          GOVERNOR_WAIT_S)
        if status == "rejected":
            final = "capability_gap_rejected"   # 期望行为：政策性缺口如实暴露
            reason = ("monitor 代理拒绝非 performance 请求（M3 安全不变式：恒 "
                      "performance）——实验性 governor 切换延后至用户显式授权"
                      "扩展 allowlist（v5 §7.4.1 R1 实验档需独立授权）")
        elif status == "done":
            if to == GOVERNOR_INVARIANT:
                final, reason = "done", "幂等回不变式（performance→performance）"
            else:
                final = "unexpected_done"        # 代理竟接受非 performance——不变式被破
                reason = (f"monitor 代理接受了非 performance 请求 {to!r}——安全"
                          "不变式被破，人工核查 governor 实态（不谎报成功）")
        else:
            final, reason = status, f"请求终态 {status}（monitor 代理未见响应？）"
        return [self._record({
            "axis": "governor_experiment", "phase": "switch", "status": final,
            "reason": reason, "from": frm, "to": to,
            "hold_s": spec.get("hold_s"), "request": "governor.request",
            "terminal": os.path.basename(term) if term else None})]

    # ② 负载整形：计划纯函数 + 逐段编排（注入点 _aggressor_burst）
    def _execute_load_shaping(self, spec):
        pattern = spec.get("pattern")
        duty = spec.get("duty")
        period_s = spec.get("period_s")
        duration_s = spec.get("duration_s")
        plan = _make_loadplan(pattern, duty, period_s, duration_s)  # 校验内含
        if "cpus" in spec:
            _cpu_list(spec["cpus"], "load_shaping.cpus")
        if "test" in spec:
            _require(isinstance(spec["test"], str) and spec["test"],
                     f"load_shaping.test 必须为非空 str: {spec['test']!r}")
        if "max_threads" in spec:
            _int_at_least(spec["max_threads"], "load_shaping.max_threads", 1)
            _require(spec["max_threads"] <= MAX_AGGRESSOR_THREADS,
                     f"load_shaping.max_threads 必须 ≤ {MAX_AGGRESSOR_THREADS}"
                     f"（M3 受界纪律：aggressor 总并发 ≤16 线程）: "
                     f"{spec['max_threads']!r}")
        self._active_load_spec = spec
        bursts, error = [], None
        try:
            for seg in plan:
                rc = self._aggressor_burst(seg["t_on_s"], seg["t_off_s"])
                bursts.append({**seg, "rc": rc})
        except ValueError:
            raise                              # 配置错误（如缺 cpus）响亮上抛
        except Exception as e:                 # 发射器异常——入案不吞
            error = repr(e)
        if error is not None:
            status = "error"
        elif all(b["rc"] == 0 for b in bursts):
            status = "done"
        else:
            status = "failed"                  # 段负载 rc 非零——不谎报 done
        return [self._record({
            "axis": "load_shaping", "phase": "shape", "status": status,
            "reason": error, "pattern": pattern, "duty": duty,
            "period_s": period_s, "duration_s": duration_s,
            "plan": plan, "bursts": bursts})]

    # ③ CPU 核上下线：helper 文件协议 + 本地预校验（validate 单一权威复用）
    def _hotplug_phase(self, op, cpus):
        action = "cpu_" + op                   # offline → cpu_offline
        ok, why = sdc_root_helper.validate(action, {"cpus": list(cpus)})
        if not ok:
            return self._record({
                "axis": "cpu_hotplug", "phase": op, "status": "rejected",
                "cpus": list(cpus), "reason": why,
                "note": "本地预校验拒绝（helper 侧同判据再验）——不写请求"})
        req = {"schema_version": "1", "action_id": uuid.uuid4().hex,
               "action": action, "parameters": {"cpus": list(cpus)},
               "expires_at": self._expires_at(HOTPLUG_WAIT_S),
               "caller_pid": os.getpid()}
        self._write_request("cpu_hotplug.request",
                            json.dumps(req, ensure_ascii=False))
        status, term = self._wait_request(self.cmd_dir, "cpu_hotplug", None,
                                          HOTPLUG_WAIT_S)
        return self._record({
            "axis": "cpu_hotplug", "phase": op, "status": status,
            "cpus": list(cpus), "action_id": req["action_id"],
            "request": "cpu_hotplug.request",
            "expires_at": req["expires_at"],
            "terminal": os.path.basename(term) if term else None})

    def _execute_hotplug(self, spec):
        op = spec.get("op")
        _require(op in ("offline", "online"),
                 f"op 必须为 offline|online: {op!r}")
        cpus = _cpu_list(spec.get("cpus"), "cpu_hotplug.cpus")
        then = spec.get("then")
        _require(then in (None, "online", "offline"),
                 f"then 必须为 online|offline 或缺省: {then!r}")
        _require(then != op, f"then == op（{then!r}）无意义——去掉 then 或取反向")
        hold_s = spec.get("hold_s", 0)
        _int_at_least(hold_s, "hold_s", 0)
        gap = self._cap_gap("cpu_hotplug", ("CPU_ONLINE_WRITABLE",),
                            extra={"op": op, "cpus": list(cpus)})
        if gap:
            return [gap]

        results = [self._hotplug_phase(op, cpus)]
        if not then:
            return results
        if results[0]["status"] == "done":
            if hold_s > 0:
                self._hold(hold_s)             # 真实 offline 后保持——再反向
            results.append(self._hotplug_phase(then, cpus))
        elif results[0]["status"] == "timeout":
            # 状态未知 → 跳过 hold 直接尝试反向收敛（恢复语义：终态向 spec 意图）
            results.append(self._hotplug_phase(then, cpus))
        else:                                  # rejected/expired/error——明确未执行
            results.append(self._record({
                "axis": "cpu_hotplug", "phase": then, "status": "skipped",
                "cpus": list(cpus),
                "reason": f"前置 {op} {results[0]['status']}——未执行/未变更，"
                          "then 跳过（入案不静默）"}))
        return results

    # ---- 恢复：restore.request + 读回断言 ----

    def restore_all(self):
        """restore.request（helper 收敛动作，恒最后处理）→ 等 done → 读回
        /sys online 与前置快照 spool/pre_state_online.txt 断言相等。"""
        snap_path = os.path.join(self.data_root, "spool", PRE_STATE_FILE)
        if not os.path.exists(snap_path):
            return [self._record({
                "axis": "restore_all", "phase": "restore", "status": "no_snapshot",
                "reason": f"无前置快照 spool/{PRE_STATE_FILE}——从未 hotplug，"
                          "无从恢复（helper 侧同判据）——不写请求"})]
        with open(snap_path, encoding="utf-8") as f:
            snapshot = f.read().strip()
        req = {"schema_version": "1", "action_id": uuid.uuid4().hex,
               "action": "restore", "parameters": {},
               "expires_at": self._expires_at(RESTORE_WAIT_S),
               "caller_pid": os.getpid()}
        self._write_request("restore.request",
                            json.dumps(req, ensure_ascii=False))
        status, term = self._wait_request(self.cmd_dir, "restore", None,
                                          RESTORE_WAIT_S)
        readback = None
        if status == "done":
            readback = self._read_online()     # 注入点：独立读回断言（纵深防御
                                              # ——helper 内部已读回，此处再核）
            status = ("done" if sdc_root_helper._parse_cpu_list(snapshot)
                      == sdc_root_helper._parse_cpu_list(readback)
                      else "readback_mismatch")
        return [self._record({
            "axis": "restore_all", "phase": "restore", "status": status,
            "snapshot": snapshot, "readback": readback,
            "action_id": req["action_id"], "request": "restore.request",
            "terminal": os.path.basename(term) if term else None})]

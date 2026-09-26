#!/usr/bin/env python3
"""sdc_root_helper.py — allowlist 特权动作执行器（v5 §13.2/§13.3，M2 Task 4）。stdlib only。

root 服务（systemd sdc-root-helper.service，T6 部署）：扫 cmd/ 三类请求文件 →
校验 → 执行 → 审计 + 请求终态化。控制器（用户态，无 root）只写请求——特权动作
全部收敛到本 helper（§13.2 进程与权限边界）。

请求通道（文件名即通道——请求文件塞不进通道外的动作）：
  cmd/burst-<action_id>.request → perf_burst（多槽：T3 评审裁定，同轮多触发
                                   各占一文件，后写不覆盖先写）
  cmd/cpu_hotplug.request       → cpu_offline | cpu_online（单槽：低频人工/系统动作）
  cmd/restore.request           → restore（单槽；恒最后处理——收敛动作）
  cmd/snapshot.request          → 沿用 root monitor 现有代理，本 helper 不重复实现

安全防线（纵深——不拼 shell 是根，层层设防）：
  1. ALLOWED 固定枚举；子进程全部 list-args subprocess（无 shell=True）；
  2. validate 参数范围校验：perf_burst duration_s∈[1,300]、cpus ⊆ possible
     （空列表=perf -a 全核语义，T3 裁定）、output_dir 必须在数据根内
     （realpath+commonpath 防路径逃逸）；cpu_offline cpus ⊆ possible 且非 CPU0
     且当前无 sdcshield/stress-ng 进程（run 边界 §7.4.4——stress-ng 是补层激励
     源同列，拒绝面只增不减）；cpu_online cpus ⊆ possible；restore 无参数；
  3. shell 元字符防线：params 全部字符串叶子（键与值递归）含 ;|&$()`"' 等即
     拒绝——等价于"序列化后扫描"（JSON 序列化只添结构字符，值中元字符必现于
     字符面），直接扫字符面可免 JSON 语法引号假阳性；
  4. nonce/幂等（§13.3）：action_id 已执行过即拒绝重放（spool/root_helper_seen.json
     持久化 + 原子写；执行前先烧录——半执行的 action 不被重放）；
  5. expires_at 期限：已过期 mv .expired.<ts> 不执行（审计一行）；缺省不设期限；
  6. hotplug 前置快照 spool/pre_state_online.txt（首次改写前落原 online 集，
     已存在不覆盖——restore 恒回最初全集）；执行后读回 /sys online 校验，
     不符 → readback_mismatch（诚实可见，不谎报 done）；
  7. 审计 spool/root_helper_audit.jsonl：每请求一行——含 rejected/expired/error
     （被拒绝的尝试也是安全事件），字段 ts/action/action_id/status/reason/
     caller_uid/caller_user/caller_pid/params/old/new/readback。

注入点（测试全 mock，绝不真 offline/真 perf/真写 sysfs 对真机）：
_sdcshield_running / _run_perf_burst / _write_cpu_online / _read_cpu_attr。
--once 跑一个完整轮次即退出（测试/演练）；守护 2s 轮询、SIGTERM 优雅退出
（分片睡 ≤1s——M1 教训：PEP 475 会续睡剩余时长）。
"""
import argparse, glob, json, os, pwd, re, signal, subprocess, sys, time, uuid
from datetime import datetime, timezone

POLL_INTERVAL_S = 2.0
ALLOWED = {"perf_burst", "cpu_online", "cpu_offline", "restore"}
MAX_BURST_DURATION_S = 300
AUDIT_FILE = "root_helper_audit.jsonl"
SEEN_FILE = "root_helper_seen.json"          # nonce 已烧录 action_id（原子写）
PRE_STATE_FILE = "pre_state_online.txt"      # hotplug 前 online 集（restore 读回）
SYSFS_CPU = "/sys/devices/system/cpu"
ACTION_ID_RE = re.compile(r"^[A-Za-z0-9_-]{1,128}$")   # 无 / 无 .. 无元字符——perf 输出文件名安全
_SHELL_META = ";|&$()`\"'"
# 通道 → 允许的动作（处理顺序即元组序：restore 恒最后——收敛动作）
_CHANNELS = (
    ("burst-*.request", ("perf_burst",)),
    ("cpu_hotplug.request", ("cpu_offline", "cpu_online")),
    ("restore.request", ("restore",)),
)


def _now():
    return time.strftime("%F %T")


# ---------------------------------------------------------------------------
# sysfs / 进程探测（_read_cpu_attr/_write_cpu_online/_sdcshield_running 为注入点）

def _read_cpu_attr(attr):
    """读 /sys/devices/system/cpu/<attr>（possible/online/offline）→ 去尾换行原文。"""
    with open(f"{SYSFS_CPU}/{attr}") as f:
        return f.read().strip()


def _parse_cpu_list(s):
    """'0-3,8' → {0,1,2,3,8}；'' → set()。"""
    out = set()
    for part in s.split(","):
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-", 1)
            out.update(range(int(a), int(b) + 1))
        else:
            out.add(int(part))
    return out


def _write_cpu_online(cpu, up):
    """写 /sys/devices/system/cpu/cpuN/online（注入点：测试 mock）。生产路径真写。"""
    with open(f"{SYSFS_CPU}/cpu{cpu}/online", "w") as f:
        f.write("1" if up else "0")


def _sdcshield_running():
    """本机是否有运行中的 sdcshield/stress-ng 进程（cpu_offline 前置，§7.4.4）。
    纯 /proc 扫描（无子进程）；读不到的进程（消失/权限）跳过。"""
    for pid in os.listdir("/proc"):
        if not pid.isdigit():
            continue
        try:
            with open(f"/proc/{pid}/comm", errors="replace") as f:
                comm = f.read().strip()
        except OSError:
            continue
        if "sdcshield" in comm or "stress-ng" in comm:
            return True
    return False


def _run_perf_burst(params, output_path):
    """perf stat 全核 100ms 间隔计数 → CSV（注入点：测试 mock）。返回 'exit=N'。

    cpus 参数不进 perf 命令：-a 即全核（T3 裁定，cpus=[] 全核语义），cpus 是
    事后分析的疑点核标记（记审计）。list-args subprocess，无 shell。
    """
    dur = int(params["duration_s"])
    cmd = ["perf", "stat", "-x", ",", "-a", "--per-core", "-I", "100",
           "-e", "cycles,instructions", "--", "sleep", str(dur)]
    with open(output_path, "w") as f:                 # -x CSV 落文件（stderr 一并收）
        p = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT,
                           timeout=dur + 60)
    return f"exit={p.returncode}"


# ---------------------------------------------------------------------------
# 校验（allowlist + 参数范围 + 元字符）

def _metachar_hit(value):
    """递归扫字符串叶子（键+值）——含 shell 元字符即真。int/float/bool/None 无字符面。"""
    if isinstance(value, str):
        return any(c in value for c in _SHELL_META)
    if isinstance(value, dict):
        return any(_metachar_hit(k) or _metachar_hit(v) for k, v in value.items())
    if isinstance(value, (list, tuple)):
        return any(_metachar_hit(v) for v in value)
    return False


def _check_cpus(cpus, possible, allow_empty):
    """cpus 校验：list[int]（bool 不算 int）且 ⊆ possible。"""
    if not isinstance(cpus, list):
        return False, f"cpus 必须为 list: {cpus!r}"
    if not cpus and not allow_empty:
        return False, "cpus 不能为空（hotplug 空目标无意义）"
    for c in cpus:
        if isinstance(c, bool) or not isinstance(c, int):
            return False, f"cpus 元素必须为 int: {c!r}"
        if c not in possible:
            return False, f"cpu {c} 超 possible（{sorted(possible)[-1]} 为上界）"
    return True, None


def validate(action, params, data_root=None):
    """allowlist + 参数范围校验 → (ok, reason)；ok 时 reason 为 None。

    data_root 提供时（HelperLoop 恒传）追加需数据根上下文的校验：
    perf_burst.output_dir 数据根内包含（防路径逃逸）、restore 前置快照存在。
    cpu_offline 顺序：cpus 边界（CPU0/possible）先于 sdcshield 检查——CPU0
    拒绝原因不被 run 边界遮蔽（战役运行中真机恒有 sdcshield 进程）。
    """
    if action not in ALLOWED:
        return False, f"动作不在 allowlist {sorted(ALLOWED)}: {action!r}"
    if params is None:
        params = {}
    if not isinstance(params, dict):
        return False, f"parameters 必须为 dict: {type(params).__name__}"
    if _metachar_hit(params):
        return False, ("参数含 shell 元字符（;|&$()`\\\"' 等），拒绝——"
                       "虽不拼 shell，纵深防御（v5 §13.2）")

    if action == "restore":
        if params:
            return False, f"restore 不接受参数: {params!r}"
        if data_root is not None and not os.path.exists(
                os.path.join(data_root, "spool", PRE_STATE_FILE)):
            return False, f"无前置快照 spool/{PRE_STATE_FILE}，无从恢复"
        return True, None

    if action == "perf_burst":
        dur = params.get("duration_s")
        if (isinstance(dur, bool) or not isinstance(dur, int)
                or not 1 <= dur <= MAX_BURST_DURATION_S):
            return False, f"duration_s 必须为 1..{MAX_BURST_DURATION_S} 的整数: {dur!r}"
        ok, why = _check_cpus(params.get("cpus"), _possible(), allow_empty=True)
        if not ok:
            return False, why
        out_dir = params.get("output_dir")
        if out_dir is not None:
            if not isinstance(out_dir, str) or not out_dir:
                return False, f"output_dir 必须为非空 str: {out_dir!r}"
            if data_root is not None and not _inside_root(out_dir, data_root):
                return False, f"output_dir 必须在数据根内（防路径逃逸）: {out_dir!r}"
        return True, None

    # cpu_offline / cpu_online
    ok, why = _check_cpus(params.get("cpus"), _possible(), allow_empty=False)
    if not ok:
        return False, why
    if action == "cpu_offline":
        if 0 in params["cpus"]:
            return False, "CPU0 拒绝下线（cpu_offline 边界）"
        if _sdcshield_running():
            return False, ("当前有 sdcshield/stress-ng 进程在运行，cpu_offline 拒绝"
                           "——run 边界约束（v5 §7.4.4）")
    return True, None


def _possible():
    return _parse_cpu_list(_read_cpu_attr("possible"))


def _inside_root(path, data_root):
    """realpath 包含校验（防 ../ 与符号链接逃逸）。"""
    real_p, real_root = os.path.realpath(path), os.path.realpath(data_root)
    try:
        return os.path.commonpath([real_p, real_root]) == real_root
    except ValueError:                                 # 混合绝对/相对等
        return False


# ---------------------------------------------------------------------------
# 执行 + 审计

def _result_skeleton(action, action_id, caller, request_name):
    return {"ts": _now(), "action": action, "action_id": action_id,
            "status": None, "reason": None, "request": request_name,
            "caller_pid": (caller or {}).get("pid"),
            "caller_uid": (caller or {}).get("uid"),
            "caller_user": (caller or {}).get("user"),
            "params": None, "old": None, "new": None, "readback": None}


def _audit(data_root, res):
    """审计 append（执行/拒绝/过期/错误一行不漏——被拒绝的尝试也是安全事件）。"""
    spool = os.path.join(data_root, "spool")
    os.makedirs(spool, exist_ok=True)
    with open(os.path.join(spool, AUDIT_FILE), "a", encoding="utf-8") as f:
        f.write(json.dumps(res, ensure_ascii=False) + "\n")


def _snapshot_pre_state(data_root, online_raw):
    """首次 hotplug 前落原 online 集（已存在不覆盖——restore 恒回最初全集）。"""
    path = os.path.join(data_root, "spool", PRE_STATE_FILE)
    if os.path.exists(path):
        return
    os.makedirs(os.path.dirname(path), exist_ok=True)
    tmp = path + ".tmp"
    with open(tmp, "w", encoding="utf-8") as f:
        f.write(online_raw + "\n")
    os.replace(tmp, path)                              # 原子写（T2/T3 教训）


def execute(action, params, data_root, action_id=None, caller=None,
            request_name=None):
    """执行一个 allowlist 动作 + 前后状态审计 → 结果 dict（即审计行形状）。

    幂等/期限由 HelperLoop 的 nonce/过期层把守；本函数自戴防线：再 validate
    （直接调用方（M3）不可信赖）、action_id 文件名安全（perf 输出路径拼接）、
    异常不外抛（status=error 入审计——7×24 守护不因单个坏请求死）。
    """
    if params is None:
        params = {}
    if action_id is None:
        action_id = uuid.uuid4().hex                   # 直接调用的现发 nonce
    res = _result_skeleton(action, action_id, caller, request_name)
    res["params"] = params
    if not ACTION_ID_RE.match(action_id):
        res["status"], res["reason"] = "rejected", f"action_id 非法（须 ^[A-Za-z0-9_-]+$）: {action_id!r}"
        _audit(data_root, res)
        return res
    ok, why = validate(action, params, data_root)
    if not ok:
        res["status"], res["reason"] = "rejected", why
        _audit(data_root, res)
        return res
    try:
        if action == "perf_burst":
            out_dir = params.get("output_dir") or os.path.join(data_root, "spool", "bursts")
            os.makedirs(out_dir, exist_ok=True)
            out_path = os.path.join(out_dir, f"{action_id}.csv")
            ret = _run_perf_burst(params, out_path)
            res["status"] = "done"
            res["readback"] = {"output": out_path, "perf_return": ret}
        elif action in ("cpu_offline", "cpu_online"):
            up = action == "cpu_online"
            old = _read_cpu_attr("online")
            _snapshot_pre_state(data_root, old)
            for cpu in sorted(set(params["cpus"])):
                _write_cpu_online(cpu, up)
            new = _read_cpu_attr("online")
            res.update(old=old, new=new, readback=new)
            want = (_parse_cpu_list(old) | set(params["cpus"])) if up \
                else (_parse_cpu_list(old) - set(params["cpus"]))
            if _parse_cpu_list(new) == want:
                res["status"] = "done"
            else:                                      # 内核拒下线/外部扰动——不谎报
                res["status"] = "readback_mismatch"
                res["reason"] = f"读回 online={new!r} 与期望不符"
        else:                                          # restore
            with open(os.path.join(data_root, "spool", PRE_STATE_FILE)) as f:
                target_raw = f.read().strip()
            target = _parse_cpu_list(target_raw)
            old = _read_cpu_attr("online")
            current = _parse_cpu_list(old)
            for cpu in sorted(current - target):
                _write_cpu_online(cpu, False)
            for cpu in sorted(target - current):
                _write_cpu_online(cpu, True)
            new = _read_cpu_attr("online")
            res.update(old=old, new=new, readback=new)
            if _parse_cpu_list(new) == target:
                res["status"] = "done"
            else:
                res["status"] = "readback_mismatch"
                res["reason"] = f"restore 读回 {new!r} ≠ 前置快照 {target_raw!r}"
    except Exception as e:
        res["status"], res["reason"] = "error", repr(e)
    _audit(data_root, res)
    return res


# ---------------------------------------------------------------------------
# 请求循环

class HelperLoop:
    """一轮 poll_once = 扫三类请求 → 期限/通道/校验/nonce → execute → 审计 + mv 终态。"""

    def __init__(self, data_root, cmd_dir):
        self.data_root, self.cmd_dir = data_root, cmd_dir
        os.makedirs(os.path.join(data_root, "spool"), exist_ok=True)
        os.makedirs(cmd_dir, exist_ok=True)
        self.seen = self._load_seen()

    def _sp(self, name):
        return os.path.join(self.data_root, "spool", name)

    def _load_seen(self):
        try:
            with open(self._sp(SEEN_FILE), encoding="utf-8") as f:
                v = json.load(f)
            return set(v) if isinstance(v, list) else set()
        except (OSError, json.JSONDecodeError):
            return set()                                # 首启/残缺：视作空（最坏=重复执行一次）

    def _save_seen(self):
        tmp = self._sp(SEEN_FILE + ".tmp")
        with open(tmp, "w", encoding="utf-8") as f:
            json.dump(sorted(self.seen), f)
        os.replace(tmp, self._sp(SEEN_FILE))            # 原子写

    def _mv(self, path, status):
        """请求 → 终态 x.<status>.<epoch>（monitor governor.done.$(date +%s) 同例）。"""
        os.replace(path, f"{path}.{status}.{int(time.time())}")

    def _caller_identity(self, path):
        """文件协议的调用者身份 = 请求文件属主（controller 现不写 caller_pid 字段 →
        审计记 null；M3 请求协议可补）。"""
        try:
            uid = os.stat(path).st_uid
        except OSError:
            return {"uid": None, "user": None}
        try:
            user = pwd.getpwuid(uid).pw_name
        except KeyError:
            user = str(uid)
        return {"uid": uid, "user": user}

    def poll_once(self):
        """处理当前全部请求（burst 多槽 sorted → hotplug → restore 恒最后）。
        返回审计行列表（含 rejected/expired/error——每请求必有一行）。"""
        jobs = []
        for pattern, channel_actions in _CHANNELS:
            for p in sorted(glob.glob(os.path.join(self.cmd_dir, pattern))):
                jobs.append((p, channel_actions))
        return [self._process(path, channel_actions) for path, channel_actions in jobs]

    def _process(self, path, channel_actions):
        name = os.path.basename(path)
        res = _result_skeleton(None, None, None, name)
        try:
            # 1. 读 + 解析（坏请求也是安全事件——拒绝入审计，不静默删）
            try:
                with open(path, encoding="utf-8") as f:
                    req = json.load(f)
                if not isinstance(req, dict):
                    raise ValueError(f"请求体非 JSON object: {type(req).__name__}")
            except (OSError, ValueError, json.JSONDecodeError) as e:
                res["status"], res["reason"] = "rejected", f"请求不可读/不可解析: {e!r}"
                return self._finish(path, res, "rejected")
            res["action"], res["action_id"] = req.get("action"), req.get("action_id")
            res["params"], res["caller_pid"] = req.get("parameters"), req.get("caller_pid")
            res["caller_uid"], res["caller_user"] = (lambda i: (i["uid"], i["user"]))(
                self._caller_identity(path))
            # 2. 信封
            if req.get("schema_version") != "1":
                res["status"], res["reason"] = "rejected", "schema_version != '1'"
                return self._finish(path, res, "rejected")
            aid = res["action_id"]
            if not isinstance(aid, str) or not ACTION_ID_RE.match(aid):
                res["status"], res["reason"] = "rejected", f"action_id 非法: {aid!r}"
                return self._finish(path, res, "rejected")
            # 3. 期限：已过 → 不执行（传入裁定：mv .expired + 审计一行）
            exp = req.get("expires_at")
            if exp is not None:
                try:
                    when = datetime.fromisoformat(exp)
                except (TypeError, ValueError):
                    res["status"], res["reason"] = "rejected", f"expires_at 不可解析: {exp!r}"
                    return self._finish(path, res, "rejected")
                if when.tzinfo is None:
                    when = when.replace(tzinfo=timezone.utc)
                if when < datetime.now(timezone.utc):
                    res["status"], res["reason"] = "expired", f"expires_at 已过: {exp}"
                    return self._finish(path, res, "expired")
            # 4. 通道一致性：文件名即通道
            if res["action"] not in channel_actions:
                res["status"], res["reason"] = "rejected", \
                    f"动作 {res['action']!r} 不在通道 {name} 允许集 {list(channel_actions)}"
                return self._finish(path, res, "rejected")
            # 5. 参数校验
            ok, why = validate(res["action"], res["params"], self.data_root)
            if not ok:
                res["status"], res["reason"] = "rejected", why
                return self._finish(path, res, "rejected")
            # 6. nonce：先烧录后执行——半执行的 action 不被重放（§13.3）
            if aid in self.seen:
                res["status"], res["reason"] = "rejected", \
                    f"重复 action_id（nonce 幂等护栏，v5 §13.3）: {aid}"
                return self._finish(path, res, "rejected")
            self.seen.add(aid)
            self._save_seen()
            # 7. execute（内部已审计；异常自兜底 status=error）
            r = execute(res["action"], res["params"], self.data_root, action_id=aid,
                        caller={"pid": res["caller_pid"], "uid": res["caller_uid"],
                                "user": res["caller_user"]},
                        request_name=name)
            self._mv(path, r["status"])
            return r
        except Exception as e:                          # 单请求级兜底：守护不死
            res["status"], res["reason"] = "error", repr(e)
            try:
                self._mv(path, "error")
            except OSError:
                pass
            _audit(self.data_root, res)
            return res

    def _finish(self, path, res, status):
        """HelperLoop 侧拒绝/过期收尾：审计 + mv 终态。"""
        _audit(self.data_root, res)
        self._mv(path, status)
        return res


# ---------------------------------------------------------------------------

def main(argv=None):
    ap = argparse.ArgumentParser(
        description="sdc-root-helper：allowlist 特权动作执行器（v5 §13.2，M2）")
    ap.add_argument("--once", action="store_true",
                    help="跑一个完整轮次后退出（测试/演练模式）")
    ap.add_argument("--data-root", default=None,
                    help="数据根（默认 $SDC_EXCITE_REPRODUCE_DIR/$SDC_CAMPAIGN_DIR"
                         "/~/sdc-excite-reproduce）")
    a = ap.parse_args(argv)
    root = a.data_root or os.environ.get("SDC_EXCITE_REPRODUCE_DIR") or \
        os.environ.get("SDC_CAMPAIGN_DIR") or os.path.expanduser("~/sdc-excite-reproduce")
    loop = HelperLoop(root, os.path.join(root, "cmd"))
    if a.once:
        res = loop.poll_once()
        parts = " ".join(f"{r['action']}:{r['status']}" for r in res)
        print(f"[root-helper] once: {len(res)} requests" + (f" ({parts})" if parts else ""))
        return 0
    stop = [False]
    def _term(signum, frame):
        stop[0] = True
    signal.signal(signal.SIGTERM, _term)
    signal.signal(signal.SIGINT, _term)
    while not stop[0]:
        loop.poll_once()
        end = time.monotonic() + POLL_INTERVAL_S       # 分片睡 ≤1s：SIGTERM 后 ≤1s 内退出
        while not stop[0] and time.monotonic() < end:
            time.sleep(min(1.0, end - time.monotonic()))
    return 0


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""collector@pmu — 核+uncore PMU 计数模式采集（v5 §6.2/§6.7 矩阵 #3/#4）。

设计：两个持久 `perf stat -x, -a -I<ms>` 子进程（计数模式、非采样——v5 §6.8 裁定）；
核表 --per-core + 组轮换（core_base/core_memory/core_path 各 ≤PMU_CORE_COUNTERS 事件）；
uncore 设备限定（hisi_sccl*_{l3c,hha,ddrc}，事件名经 sysfs events/ 发现式枚举）；
percent<80 → 该窗口 multiplex_degraded（宽表 percent_covered 列保留可见——诚实呈现）。

相对计划的实证修正（2026-09-25 Kunpeng 920 / perf 6.6 root 实测）：
1. perf stat 的 -x, 行输出走 **stderr**（stdout=PIPE+stderr=DEVNULL 实测 0 行）→
   stderr=subprocess.STDOUT 合并进所读管道；
2. raw 事件在输出中打印为 rXX 原样（非菜单名）→ 解析后经 raw→菜单名映射落列；
3. 组轮换后事件集合随组变化 → 列头取轮换全组菜单**并集**（固定宽表 schema，
   非本组列留空）——否则 10min 轮换后行列错位（同 percore 列头防御先例）；
4. 读子进程用 select+os.read 直读 fd、跨周期字节缓冲装配行：select 看不见
   TextIO 用户态缓冲，select+readline 实测 3 周期丢 11/256 核行 + 1/64 uncore 行；
   且轮换前先收完旧进程在途行——先 kill 再读会丢旧进程最后一个完整窗口；
5. 子进程 EOF/读循环超时均 break（select 对 EOF 恒就绪，continue 会忙转）；
6. run() 退出（max_cycles/SIGTERM）后 terminate+wait 子进程，不留孤儿 perf。
"""
import glob, os, select, subprocess, time
from sdc_collector import SdcCollector, register

MUX_THRESHOLD = 80.0
ROTATE_S = 600            # 组轮换周期（v5 §6.2：超预算分组 10min 轮换）
RAW = {  # v5 §6.2 六组菜单（TSV110 raw 码；分组即能力锚点）
    "core_base":   {"cpu_cycles": 0x11, "inst_retired": 0x08, "inst_spec": 0x1b,
                    "exe_stall_cycle": 0x7001, "stall_frontend": 0x23, "stall_backend": 0x24},
    "core_memory": {"mem_stall_anyload": 0x7004, "mem_stall_l1miss": 0x7006,
                    "mem_stall_l2miss": 0x7007, "l1d_cache_refill_rd": 0x42,
                    "l2d_cache_refill_rd": 0x52, "ll_cache_miss_rd": 0x37},
    "core_path":   {"dtlb_walk": 0x34, "l1d_tlb_refill_rd": 0x4c, "remote_access": 0x31,
                    "memory_error": 0x1a, "br_mis_pred": 0x10},
    "l3c":  {"back_invalid": 0x29, "retry_ring": 0x41, "retry_cpu": 0x40, "prefetch_drop": 0x42},
    "hha":  {"rx_outer": 0x01, "rx_sccl": 0x02, "tx_snp_num": 0x33},
    "ddrc": {"flux_rd": 0x01, "flux_wr": 0x00, "rnk_chg": 0x06, "rw_chg": 0x07},
}
GROUPS = RAW
CORE_ROTATION = ["core_base", "core_memory", "core_path"]
UNCORE_ROTATION = ["l3c", "hha", "ddrc"]
# 宽表列 = 轮换全组事件并集（固定 schema；group 列标示当前组，非本组列留空）
CORE_COLUMNS = ["cycles"] + [k for g in CORE_ROTATION for k in RAW[g]]
UNCORE_COLUMNS = [k for g in UNCORE_ROTATION for k in RAW[g]]

def is_degraded(percent):
    return percent < MUX_THRESHOLD

def parse_percore(line):
    f = line.rstrip("\n").split(",")
    return f[0], f[1], int(f[3]), f[5], float(f[7])

def parse_plain(line):
    f = line.rstrip("\n").split(",")
    return f[0], int(f[1]), f[3], float(f[5])

def _uncore_events(group):
    """发现式：枚举 hisi_sccl*_<group> 设备的 events/ 目录，命中菜单键则取该 raw。"""
    evs = []
    for dev in sorted(glob.glob(f"/sys/bus/event_source/devices/hisi_sccl*_{group}*")):
        name = os.path.basename(dev)
        for ev, code in RAW[group].items():
            if os.path.exists(os.path.join(dev, "events", ev)):
                evs.append(f"{name}/{ev}/")
    return evs

def _drain_lines(fd, buf):
    """select+os.read 直读 fd（绕开 TextIO 缓冲——select 看不见它，readline 版实测丢行）。
    返回 (完整行列表, 残余字节缓冲)。残余（半行）跨周期携带，不丢。"""
    while True:
        r, _, _ = select.select([fd], [], [], 0.2)
        if not r:
            break
        chunk = os.read(fd, 65536)
        if not chunk:                     # EOF：子进程退出
            break
        buf += chunk
    lines = []
    while b"\n" in buf:
        line, _, buf = buf.partition(b"\n")
        lines.append(line.decode("utf-8", "replace"))
    return lines, buf

def _kill(p):
    """terminate+wait：退出与轮换都不留孤儿 perf / 僵尸。"""
    if not p:
        return
    p.terminate()
    try:
        p.wait(timeout=2)
    except subprocess.TimeoutExpired:
        p.kill()

@register
class PmuCollector(SdcCollector):
    NAME = "pmu"
    DEFAULT_PERIOD_S = 20.0
    def __init__(self, **kw):
        super().__init__(**kw)
        try:
            self.budget = int(os.environ.get("PMU_CORE_COUNTERS", "6"))
        except ValueError:                    # capabilities.env 可能尚为 "unknown"（Task 7 刷新）
            self.budget = 6
        self._core_group = ""
        self._uncore_group = ""
        self._p_core = self._p_uncore = None
        self._core_fd = self._uncore_fd = None
        self._core_buf = self._uncore_buf = b""
        self._core_evname = {}
        self._rotate_at = 0.0
        self._core_hdr_written = self._uncore_hdr_written = False
        self._core_out = open(os.path.join(os.path.dirname(self.csv_path), "pmu_core.csv"), "a")
        self._uncore_out = open(os.path.join(os.path.dirname(self.csv_path), "pmu_uncore.csv"), "a")
    def _spawn(self):
        self._core_group = CORE_ROTATION[0] if not self._core_group else \
            CORE_ROTATION[(CORE_ROTATION.index(self._core_group) + 1) % len(CORE_ROTATION)]
        self._uncore_group = UNCORE_ROTATION[0] if not self._uncore_group else \
            UNCORE_ROTATION[(UNCORE_ROTATION.index(self._uncore_group) + 1) % len(UNCORE_ROTATION)]
        _kill(self._p_core)
        _kill(self._p_uncore)
        self._core_buf = self._uncore_buf = b""   # 旧进程残余半行弃置（≤1 行）
        raw_items = list(RAW[self._core_group].items())[:self.budget - 1]
        self._core_evname = {f"r{c:x}": n for n, c in raw_items}   # perf 打印 rXX → 菜单名
        core_evs = ["cycles"] + [f"r{c:x}" for _, c in raw_items]
        uncore_evs = _uncore_events(self._uncore_group)
        # perf -x, 行输出走 stderr（实测）→ 合并进所读管道；不用 text 模式（_drain_lines 直读 fd）
        self._p_core = subprocess.Popen(
            ["perf", "stat", "-x,", "-a", "--per-core", f"-I{int(self.period_s*1000)}",
             "-e", ",".join(core_evs)], stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        self._core_fd = self._p_core.stdout.fileno()
        self._p_uncore = None
        self._uncore_fd = None
        if uncore_evs:
            args = ["perf", "stat", "-x,", "-a", f"-I{int(self.period_s*1000)}",
                    "-e", ",".join(uncore_evs)]
            self._p_uncore = subprocess.Popen(args, stdout=subprocess.PIPE,
                                              stderr=subprocess.STDOUT)
            self._uncore_fd = self._p_uncore.stdout.fileno()
        self._rotate_at = time.monotonic() + ROTATE_S
    def _drain_write(self):
        """收净两路在途行并落盘（核表 per-core 行形，uncore plain 行形）。"""
        nrows = 0
        if not self._core_hdr_written:
            self._core_out.write(",".join(["ts", "group", "core", "percent_covered"]
                                          + CORE_COLUMNS) + "\n")
            self._core_hdr_written = True
        lines, self._core_buf = _drain_lines(self._core_fd, self._core_buf)
        acc = {}
        for line in lines:
            if line.startswith("#") or "," not in line:
                continue
            try:
                ts, core, val, ev, pct = parse_percore(line)
            except (IndexError, ValueError):
                continue
            ev = self._core_evname.get(ev, ev)   # rXX → 菜单名
            acc.setdefault((ts, core), {"pct": pct})[ev] = val
        for (ts, core), d in sorted(acc.items()):
            self._core_out.write(",".join([self.now_iso(), self._core_group, core,
                                           f"{d['pct']}"] + [str(d.get(e, "")) for e in CORE_COLUMNS]) + "\n")
        nrows += len(acc)
        # uncore 同构（plain 行形）；事件键取设备名后的短名，列头为并集固定 schema
        uacc = {}
        if self._p_uncore is not None:
            ulines, self._uncore_buf = _drain_lines(self._uncore_fd, self._uncore_buf)
            for line in ulines:
                if line.startswith("#") or "," not in line:
                    continue
                try:
                    ts, val, ev, pct = parse_plain(line)
                except (IndexError, ValueError):
                    continue
                parts = ev.split("/")
                if len(parts) < 2:
                    continue
                uacc.setdefault((ts, parts[0]), {"pct": pct})[parts[1]] = val
            if not self._uncore_hdr_written and uacc:
                self._uncore_out.write(",".join(["ts", "group", "device", "percent_covered"]
                                                + UNCORE_COLUMNS) + "\n")
                self._uncore_hdr_written = True
            for (ts, dev), d in sorted(uacc.items()):
                self._uncore_out.write(",".join([self.now_iso(), self._uncore_group, dev,
                                                 f"{d['pct']}"] + [str(d.get(e, "")) for e in UNCORE_COLUMNS]) + "\n")
            nrows += len(uacc)
        self.samples_total += nrows               # 直写专用文件；基类 samples_total 记行数
        self._core_out.flush(); self._uncore_out.flush()
    def collect_once(self):
        if self._p_core is None or time.monotonic() >= self._rotate_at:
            if self._p_core is not None:
                self._drain_write()               # 轮换前收完旧进程在途行——不丢最后一个窗口
            self._spawn()
            time.sleep(self.period_s + 0.5)       # 等新进程首个 interval 行
        self._drain_write()
        return []                                 # 数据直写专用文件，不走基类 CSV
    def run(self):
        try:
            super().run()
        finally:                                  # max_cycles/SIGTERM 退出后不留孤儿 perf
            _kill(self._p_core)
            _kill(self._p_uncore)

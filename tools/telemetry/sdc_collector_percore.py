#!/usr/bin/env python3
"""collector@percore — 逐核占用率 + 实测频率 + 频点驻留直方图（v5 §6.7 矩阵 #1/#7/#8）。

csv: monitor/percore.csv  ts + util_cpuN_pct×N + freq_cpuN_khz×N
驻留: 内存 100MHz 桶累计，每 10 个周期（或 RESIDENCY_EVERY_S）快照追加 freq_residency.log
频率口径: cpuinfo_cur_freq（实测值，root）优先，降级 scaling_cur_freq（请求值），再降级列空
"""
import glob, os, sys, time
from sdc_collector import SdcCollector, register

SYS_CPU = "/sys/devices/system/cpu"
RESIDENCY_EVERY_S = 600

def _read(path):
    try:
        return open(path).read().strip()
    except OSError:
        return None

def _cpus():
    out = []
    for part in (_read(f"{SYS_CPU}/possible") or "").split(","):
        if not part:
            continue
        if "-" in part:
            a, b = part.split("-"); out.extend(range(int(a), int(b) + 1))
        else:
            out.append(int(part))
    return out

def _stat_ticks(text):
    """返回 {cpu_id: [各列 tick]}，仅逐核行（cpuN）。"""
    out = {}
    for line in text.splitlines():
        if line.startswith("cpu") and line[3:4].isdigit():
            parts = line.split()
            out[int(parts[0][3:])] = [int(x) for x in parts[1:]]
    return out

def util_pct(stat_a, stat_b):
    """两拍 /proc/stat → 每核 busy%（(total-idle)/total），核序按 possible 升序。"""
    ta, tb = _stat_ticks(stat_a), _stat_ticks(stat_b)
    out = []
    for c in sorted(tb):
        a, b = ta.get(c), tb[c]
        if not a:
            out.append(-1.0); continue
        d = [y - x for x, y in zip(a, b)]
        total = sum(d)
        idle = d[3] + (d[4] if len(d) > 4 else 0)      # idle + iowait
        out.append(round(100.0 * (total - idle) / total, 1) if total > 0 else 0.0)
    return out

def freq_bucket_khz(khz):
    return int(khz) // 100000 * 100000                 # 100MHz 桶

def read_freq(cpus):
    vals = []
    for c in cpus:
        base = f"{SYS_CPU}/cpu{c}/cpufreq"
        v = _read(f"{base}/cpuinfo_cur_freq") or _read(f"{base}/scaling_cur_freq")
        vals.append(int(v) if v else "")
    return vals

@register
class PerCoreCollector(SdcCollector):
    NAME = "percore"
    DEFAULT_PERIOD_S = float(os.environ.get("PERCORE_PERIOD_S", "60"))
    def __init__(self, **kw):
        super().__init__(**kw)
        self.cpus = _cpus()
        self.HEADER = (["ts"] + [f"util_cpu{c}_pct" for c in self.cpus]
                       + [f"freq_cpu{c}_khz" for c in self.cpus])
        self._init_csv()
        self._last_stat = open("/proc/stat").read()
        self._buckets = {c: {} for c in self.cpus}
        self._last_snap = time.monotonic()
        # 列头防御：CSV 已存在且列头与本次核数不符时，追加会串列——首轮即拒绝启动
        _hdr = open(self.csv_path).readline().rstrip("\n") if os.path.getsize(self.csv_path) else ""
        if _hdr and _hdr != ",".join(self.HEADER):
            print(f"[{self.NAME}] CSV 列头不一致（核数变化？）: {self.csv_path}", file=sys.stderr, flush=True); sys.exit(1)
    def collect_once(self):
        cur = open("/proc/stat").read()
        utils = util_pct(self._last_stat, cur)
        self._last_stat = cur
        freqs = read_freq(self.cpus)
        for c, f in zip(self.cpus, freqs):
            if f != "":
                b = self._buckets[c]
                b[freq_bucket_khz(f)] = b.get(freq_bucket_khz(f), 0) + 1
        rows = [[self.now_iso()] + [f"{u}" if u >= 0 else "" for u in utils]
                + [f"{f}" if f != "" else "" for f in freqs]]
        if time.monotonic() - self._last_snap >= RESIDENCY_EVERY_S:
            with open(os.path.join(os.path.dirname(self.csv_path), "freq_residency.log"), "a") as f:
                f.write(f"# {self.now_iso()} 100MHz 桶累计（自启动）\n")
                for c in self.cpus:
                    if self._buckets[c]:
                        f.write(f"cpu{c}: " + " ".join(f"{k//1000}MHzx{v}"
                                for k, v in sorted(self._buckets[c].items())) + "\n")
            self._last_snap = time.monotonic()
        return rows

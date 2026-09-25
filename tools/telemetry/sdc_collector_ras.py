#!/usr/bin/env python3
"""collector@ras — EDAC/journal 流/spurious canary/rasdaemon 监护/BERT dump（v5 §6.6/§6.7 #9）。

M1 口径（诚实边界）：spurious canary 为 dmesg/journal 轮询计数版（source=dmesg，
无 per-CPU 定位——BPF tracepoint 版属 M2，v5 §6.5 的降级路径）；rasdaemon 监护 10min 一次。
"""
import datetime, glob, os, re, shutil, subprocess, sys
from sdc_collector import SdcCollector, register

RAS_KEYWORDS = re.compile(
    r"error|fail|ras|edac|mce|hwpoison|throttle|thermal|panic|oops|segfault"
    r"|page fault|guard page|hardware|spurious", re.IGNORECASE)
RAS_NOISE = re.compile(r"hns3|rcu.*stall|rcu: INFO", re.IGNORECASE)
SPURIOUS_RE = re.compile(r"spurious translation fault", re.IGNORECASE)

def ras_keyword(line):
    return bool(RAS_KEYWORDS.search(line)) and not RAS_NOISE.search(line)

def count_spurious(text):
    return len(SPURIOUS_RE.findall(text))

def _read_int(path):
    try:
        return int(open(path).read().strip())
    except (OSError, ValueError):
        return -1

@register
class RasCollector(SdcCollector):
    NAME = "ras"
    DEFAULT_PERIOD_S = 60.0
    RASDAEMON_CHECK_EVERY_S = 600
    def __init__(self, **kw):
        super().__init__(**kw)
        self.HEADER = ["ts", "mc", "ce", "ue"]
        # v5 §6.6 产物名为 ras_edac.csv（基类默认按 NAME 派生成 ras.csv，此处显式对齐计划接口）
        self.csv_path = os.path.join(os.path.dirname(self.csv_path), "ras_edac.csv")
        self._init_csv()
        self._last_since = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self._journal = open(os.path.join(os.path.dirname(self.csv_path), "journal_watch.log"), "a")
        self._last_daemon = 0.0
        bert = "/sys/firmware/acpi/tables/BERT"
        inv = os.path.join(os.path.dirname(os.path.dirname(self.csv_path)), "inventory")
        if os.path.exists(bert):
            os.makedirs(inv, exist_ok=True)
            try:
                shutil.copy(bert, os.path.join(inv, "BERT.bin"))
            except OSError as e:
                print(f"[ras] BERT dump 失败: {e}", file=sys.stderr)
        self._spurious_total = 0
    def collect_once(self):
        rows = []
        for mc in sorted(glob.glob("/sys/devices/system/edac/mc/mc*"),
                         key=lambda p: int(os.path.basename(p)[2:])):
            name = os.path.basename(mc)
            ce, ue = _read_int(f"{mc}/ce_count"), _read_int(f"{mc}/ue_count")
            if ce < 0 and ue < 0:
                continue                                # 无 EDAC 或不可读——列空降级
            rows.append([self.now_iso(), name, str(ce), str(ue)])
            if ue > 0:
                self._journal.write(f"[{self.now_iso()}] UE>0 {name} ce={ce} ue={ue} 告警\n")
        out = subprocess.run(["journalctl", "-k", "--since", self._last_since, "--no-pager"],
                             capture_output=True, text=True).stdout
        self._last_since = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        spurious = count_spurious(out)
        if spurious:
            self._spurious_total += spurious
            self._journal.write(f"[{self.now_iso()}] SPURIOUS total={self._spurious_total} "
                                f"delta={spurious} source=dmesg（无 per-CPU 定位，M2 BPF 升级）\n")
        for line in out.splitlines():
            if ras_keyword(line):
                self._journal.write(f"[{self.now_iso()}] RAS {line[:300]}\n")
        self._journal.flush()
        import time as _t
        if _t.monotonic() - self._last_daemon >= self.RASDAEMON_CHECK_EVERY_S:
            st = subprocess.run(["systemctl", "is-active", "rasdaemon"],
                                capture_output=True, text=True).stdout.strip()
            if st != "active":
                self._journal.write(f"[{self.now_iso()}] 告警: rasdaemon is-active={st}\n")
            self._last_daemon = _t.monotonic()
        return rows

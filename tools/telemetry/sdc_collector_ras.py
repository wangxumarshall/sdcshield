#!/usr/bin/env python3
"""collector@ras — EDAC/journal 流/spurious canary/rasdaemon 监护/BERT dump（v5 §6.6/§6.7 #9）。

M1 口径（诚实边界）：spurious canary 为 dmesg/journal 轮询计数版（source=dmesg，
无 per-CPU 定位——BPF tracepoint 版属 M2，v5 §6.5 的降级路径）；rasdaemon 监护 10min 一次。

per-DIMM 宽稀疏列设计（v5 12 维矩阵 #9 路由项）：HEADER 除 ts/mc/ce/ue 外，
为 __init__ 时发现式枚举到的每个 EDAC dimm 追加 dimm_mc<N>_<idx>_ce/_ue 两列
（按 mc 号、dimm 号数值排序）；每行（每 mc 一行）只填本 mc 名下 dimm 的当前值，
其余 dimm 列与文件缺失的计数列留空——列集在进程生命周期内固定，dimm 热插拔
需重启采集器才会进列集；重启时 __init__ 比对既有文件列头（percore 模式），
列集漂移 → stderr 一行 + exit 1 拒绝拼接，绝不静默错位续写。
"""
import datetime, glob, os, re, shutil, subprocess, sys
from sdc_collector import SdcCollector, register

EDAC_ROOT = "/sys/devices/system/edac"   # 模块常量：测试可 monkeypatch 到假树

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

def _discover_dimms():
    """发现式枚举 EDAC mc*/dimm*/（mc 号与 dimm 号均数值排序）。返回 [(mc_name, dimm_idx)]。"""
    out = []
    for mc in sorted(glob.glob(os.path.join(EDAC_ROOT, "mc", "mc*")),
                     key=lambda p: int(os.path.basename(p)[2:])):
        for dimm in sorted(glob.glob(os.path.join(mc, "dimm*")),
                           key=lambda p: int(os.path.basename(p)[4:])):
            out.append((os.path.basename(mc), int(os.path.basename(dimm)[4:])))
    return out

@register
class RasCollector(SdcCollector):
    NAME = "ras"
    DEFAULT_PERIOD_S = 60.0
    RASDAEMON_CHECK_EVERY_S = 600
    def __init__(self, **kw):
        super().__init__(**kw)
        self._dimms = _discover_dimms()
        self.HEADER = (["ts", "mc", "ce", "ue"]
                       + [f"dimm_{mc}_{idx}_{c}" for mc, idx in self._dimms for c in ("ce", "ue")])
        # v5 §6.6 产物名为 ras_edac.csv（基类默认按 NAME 派生成 ras.csv，此处显式对齐计划接口）
        self.csv_path = os.path.join(os.path.dirname(self.csv_path), "ras_edac.csv")
        self._init_csv()
        # 重启列头防御（I-1，percore 模式移植）：既有文件列头与本次列集不符（dimm 拓扑
        # 跨重启漂移）时，追加会静默错位拼进旧列头——首轮即拒绝启动
        _hdr = open(self.csv_path).readline().rstrip("\n") if os.path.getsize(self.csv_path) else ""
        if _hdr and _hdr != ",".join(self.HEADER):
            print(f"[{self.NAME}] CSV 列头不一致（EDAC dimm 拓扑漂移？）: {self.csv_path}\n"
                  f"  已有: {_hdr}\n  待写: {','.join(self.HEADER)}", file=sys.stderr, flush=True)
            sys.exit(1)
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
        for mc in sorted(glob.glob(os.path.join(EDAC_ROOT, "mc", "mc*")),
                         key=lambda p: int(os.path.basename(p)[2:])):
            name = os.path.basename(mc)
            ce, ue = _read_int(f"{mc}/ce_count"), _read_int(f"{mc}/ue_count")
            if ce < 0 and ue < 0:
                continue                                # 无 EDAC 或不可读——列空降级
            row = [self.now_iso(), name, str(ce), str(ue)]
            for dmc, didx in self._dimms:               # 宽稀疏：本 mc 名下 dimm 填值，其余留空
                for cnt in ("dimm_ce_count", "dimm_ue_count"):
                    v = _read_int(f"{EDAC_ROOT}/mc/{dmc}/dimm{didx}/{cnt}") if dmc == name else -1
                    row.append("" if v < 0 else str(v))
            rows.append(row)
            if ue > 0:
                self._journal.write(f"[{self.now_iso()}] UE>0 {name} ce={ce} ue={ue} 告警\n")
        p = subprocess.run(["journalctl", "-k", "--since", self._last_since, "--no-pager"],
                           capture_output=True, text=True)
        if p.returncode != 0:
            # journalctl 失败（如权限/容量）：告警且不推进 _last_since——下周期重读同窗口，不丢流
            self._journal.write(f"[{self.now_iso()}] 告警: journalctl rc={p.returncode} "
                                f"since={self._last_since}（不推进 since，下周期重试）\n")
        else:
            self._last_since = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
            spurious = count_spurious(p.stdout)
            if spurious:
                self._spurious_total += spurious
                self._journal.write(f"[{self.now_iso()}] SPURIOUS total={self._spurious_total} "
                                    f"delta={spurious} source=dmesg（无 per-CPU 定位，M2 BPF 升级）\n")
            for line in p.stdout.splitlines():
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

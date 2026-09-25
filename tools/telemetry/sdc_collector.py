#!/usr/bin/env python3
"""sdc_collector.py — sdc-excite-reproduce 采集器族入口（v5 §3.1 collector@{percore,pmu,ras}）。

用法: python3 sdc_collector.py <percore|pmu|ras> [--period-s N]
自监控（v5 §6.8）: 每周期向 monitor/collector_self.csv 写一行——samples_total/dropped/
period/loop_duration/last_success/rss；drop counter 是丢失率 <0.1% 验收的依据。
"""
import argparse, csv, datetime, os, resource, signal, sys, time

DATA_ROOT = os.environ.get("SDC_EXCITE_REPRODUCE_DIR",
                           os.environ.get("SDC_CAMPAIGN_DIR", os.path.expanduser("~/sdc-excite-reproduce")))

class SdcCollector:
    NAME, HEADER = "base", []
    def __init__(self, period_s, out_dir=None, selfmon_path=None, max_cycles=None):
        self.period_s = period_s
        out = out_dir or os.path.join(DATA_ROOT, "monitor")
        os.makedirs(out, exist_ok=True)
        self.csv_path = os.path.join(out, f"{self.NAME}.csv")
        self.selfmon_path = selfmon_path or os.path.join(out, "collector_self.csv")
        self.max_cycles = max_cycles
        self.samples_total = 0
        self.samples_dropped = 0
        self._stop = False
        self._init_csv()
    def _init_csv(self):
        if self.HEADER and not os.path.exists(self.csv_path):
            with open(self.csv_path, "w", newline="") as f:
                csv.writer(f).writerow(self.HEADER)
    @staticmethod
    def now_iso():
        return datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    def collect_once(self):
        raise NotImplementedError
    def _selfmon(self, loop_s, ok):
        new = not os.path.exists(self.selfmon_path)
        with open(self.selfmon_path, "a", newline="") as f:
            w = csv.writer(f)
            if new:
                w.writerow(["ts", "collector", "samples_total", "samples_dropped",
                            "period_s", "loop_duration_s", "last_success_ts", "rss_kb"])
            w.writerow([self.now_iso(), self.NAME, self.samples_total, self.samples_dropped,
                        self.period_s, f"{loop_s:.3f}", self.now_iso() if ok else "",
                        resource.getrusage(resource.RUSAGE_SELF).ru_maxrss])
    def run(self):
        def _term(signum, frame):
            self._stop = True
        signal.signal(signal.SIGTERM, _term)
        signal.signal(signal.SIGINT, _term)
        cycles = 0
        while not self._stop and (self.max_cycles is None or cycles < self.max_cycles):
            t0 = time.monotonic()
            try:
                rows = self.collect_once()
                with open(self.csv_path, "a", newline="") as f:
                    csv.writer(f).writerows(rows)
                self.samples_total += len(rows)
                ok = True
            except Exception as e:                      # 采集失败≠退出：计数后下周期重试
                self.samples_dropped += 1
                print(f"[{self.NAME}] collect failed: {e}", file=sys.stderr, flush=True)
                ok = False
            self._selfmon(time.monotonic() - t0, ok)
            cycles += 1
            # 分片睡：SIGTERM 后 ≤1s 内退出（PEP 475 会续睡剩余时长，长周期会撞模板 TimeoutStopSec=30 被 SIGKILL）
            remain = self.period_s - (time.monotonic() - t0)
            while remain > 0 and not self._stop:
                time.sleep(min(1.0, remain))
                remain = self.period_s - (time.monotonic() - t0)

_REGISTRY = {}
def register(cls):
    _REGISTRY[cls.NAME] = cls
    return cls

def main(argv=None):
    for m in ("sdc_collector_percore", "sdc_collector_pmu", "sdc_collector_ras"):
        try:
            __import__(m)                                 # 触发 @register；未交付的子模块跳过
        except ImportError:
            pass
    ap = argparse.ArgumentParser()
    ap.add_argument("name", choices=sorted(_REGISTRY))
    ap.add_argument("--period-s", type=float, default=None)
    a = ap.parse_args(argv)
    cls = _REGISTRY[a.name]
    period = a.period_s or cls.DEFAULT_PERIOD_S
    cls(period_s=period).run()

if __name__ == "__main__":
    # __main__ 双导入陷阱（C1）：直接 main() 时子模块 from sdc_collector import register
    # 会导入第二份模块副本，注册表写进副本、分派读原表恒空。委托给 canonical 模块：
    from sdc_collector import main as _main
    _main(sys.argv[1:])

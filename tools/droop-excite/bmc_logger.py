#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""BMC 传感器轮询记录器（in-band ipmitool → CSV）。

0103 (iBMC 6.70) 实测传感器名（tests/fixtures/sensor_list_0103.txt）。
节奏保持 >= interval_min 秒（实测 <3s 会把 BMC 采样打超时）。
用法: sudo python3 bmc_logger.py --out /path/bmc.csv [--interval 3] [--max-samples N]
停止: SIGTERM/SIGINT（优雅退出）。与 sdcshield 解耦，可独立运行。
"""
import argparse
import csv
import signal
import subprocess
import sys
import time

SENSORS = [
    "CPU1 N_VDDAVS", "CPU2 N_VDDAVS", "CPU1 VDDAVS", "CPU2 VDDAVS",
    "Power", "Power2", "PS2 IOut", "CPU1 Core Rem", "CPU2 Core Rem",
]
CSV_COLUMNS = ["ts"] + SENSORS


def parse_sensor_list(text):
    """ipmitool sensor list 输出 → {传感器名: 读数}。'na'/空读数跳过。"""
    out = {}
    for line in text.splitlines():
        parts = [p.strip() for p in line.split("|")]
        if len(parts) >= 2 and parts[0]:
            if parts[1] and parts[1] != "na":
                out[parts[0]] = parts[1]
    return out


def read_once(timeout=30):
    """单次全量读取（in-band，需 root）。失败抛 CalledProcessError/TimeoutExpired。"""
    r = subprocess.run(["ipmitool", "sensor", "list"], capture_output=True,
                       text=True, timeout=timeout)
    r.check_returncode()
    return parse_sensor_list(r.stdout)


def poll_loop(out_path, interval_min=3.0, max_samples=None):
    """轮询主循环。返回写入样本数。SIGTERM/SIGINT 停止。"""
    stop = {"flag": False}

    def _stop(signum, frame):
        stop["flag"] = True

    signal.signal(signal.SIGTERM, _stop)
    signal.signal(signal.SIGINT, _stop)
    n = 0
    with open(out_path, "w", newline="") as f:
        w = csv.writer(f)
        w.writerow(CSV_COLUMNS)
        f.flush()
        while not stop["flag"]:
            t0 = time.monotonic()
            try:
                vals = read_once()
                w.writerow([f"{time.time():.3f}"] + [vals.get(s, "") for s in SENSORS])
            except Exception as e:  # BMC 偶发超时不致命：记录后继续
                w.writerow([f"{time.time():.3f}"] + [f"ERR:{e}"] * len(SENSORS))
            f.flush()
            n += 1
            if max_samples is not None and n >= max_samples:
                break
            time.sleep(max(0.0, interval_min - (time.monotonic() - t0)))
    return n


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out", required=True, help="输出 CSV 路径")
    ap.add_argument("--interval", type=float, default=3.0,
                    help="最小轮询间隔秒（默认 3.0，勿低于 3）")
    ap.add_argument("--max-samples", type=int, default=None)
    args = ap.parse_args(argv)
    if args.interval < 3.0:
        print("WARNING: interval < 3s 可能打超时 BMC（实测教训）", file=sys.stderr)
    n = poll_loop(args.out, args.interval, args.max_samples)
    print(f"bmc_logger: wrote {n} samples to {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())

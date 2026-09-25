#!/usr/bin/env python3
"""ab_baseline.py — A/B 开销基线锚点（v5 §6.8）：同命令 N 次取 loop-count 中位数。

用法: python3 ab_baseline.py --runs 5 --out /home/sdc/sdc-excite-reproduce/monitor/ab_baseline.json
对照纪律：采集器全部停止时取"无监控"锚点；采集器运行时复测，中位数下降 ≤3% 达标。
注意：共享机器（clangd/llama-server 等常驻）噪声存在——两次测量取相同时段与相同 -n。
"""
import argparse, json, os, re, statistics, subprocess, sys, tempfile

def parse_loop_total(yaml_text):
    return sum(int(m) for m in re.findall(r"loop-count:\s*(\d+)", yaml_text))

def run_once(sdc_bin, t_ms, n_threads):
    with tempfile.NamedTemporaryFile(suffix=".yaml", delete=False) as f:
        out = f.name
    try:
        # -vvv 必须：per-thread loop-count 仅在 verbosity>2 时输出（framework/logging.cpp），
        # 缺省/-v 实测 0 条 → loop_total 恒 0（2026-09-25 本机实测）
        subprocess.run([sdc_bin, "-e", "zstd19", "-t", str(t_ms), "-n", str(n_threads),
                        "-vvv", "-o", out], check=True, capture_output=True, timeout=t_ms / 1000 * 5 + 60)
        total = parse_loop_total(open(out).read())
        if total <= 0:
            raise RuntimeError("loop_total=0——-vvv 输出无 loop-count？锚点不可用（防假 PASS）")
        return total
    finally:
        os.unlink(out)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--runs", type=int, default=5)
    ap.add_argument("--t-ms", type=int, default=10000)
    ap.add_argument("--n", type=int, default=32)
    ap.add_argument("--sdc-bin", default=os.path.join(os.path.dirname(os.path.abspath(__file__)),
                                                      "../../builddir/sdcshield"))
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    runs = [{"loop_total": run_once(a.sdc_bin, a.t_ms, a.n)} for _ in range(a.runs)]
    doc = {"config": {"t_ms": a.t_ms, "n": a.n, "runs": a.runs},
           "runs": runs, "median_loop_total": statistics.median(r["loop_total"] for r in runs)}
    os.makedirs(os.path.dirname(a.out), exist_ok=True)
    json.dump(doc, open(a.out, "w"), indent=1)
    print(json.dumps(doc))

if __name__ == "__main__":
    main()

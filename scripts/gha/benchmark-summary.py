#!/usr/bin/env python3
# benchmark-summary.py — 汇总 report 作业下载的 15 个 benchmark.tsv 成跨 OS 对比表。
#
# 输入:all-results/verify-<series>-<sp>/benchmark/benchmark.tsv(actions/download-artifact
#   未 merge 时每镜像一个子目录)。
# 输出:stdout 一张 markdown 表,列 = 测试名,行 = 各镜像 wall_seconds(固定 loop 数)。
# 纯 stdlib,无任何第三方依赖。
#
# SPDX-License-Identifier: Apache-2.0
import glob
import os
import sys

ARTIFACT_GLOB = "all-results/verify-*/benchmark/benchmark.tsv"


def main():
    rows = {}
    tests = []
    found = 0
    for f in sorted(glob.glob(ARTIFACT_GLOB)):
        # f 形如 all-results/verify-24.03-LTS-SP3/benchmark/benchmark.tsv
        img = f.split(os.sep)[1]
        if not img.startswith("verify-"):
            continue
        img = img[len("verify-"):]  # 24.03-LTS-SP3
        rows.setdefault(img, {})
        found += 1
        with open(f, "r") as fh:
            for line in fh:
                line = line.rstrip("\n")
                if not line or line.startswith("test"):
                    continue
                parts = line.split("\t")
                if len(parts) < 2:
                    continue
                t, rt = parts[0], parts[1]
                rows[img][t] = rt
                if t not in tests:
                    tests.append(t)

    if not rows:
        print("No benchmark.tsv artifacts found.", file=sys.stderr)
        return 1

    imgs = sorted(rows.keys())
    print("## Multi-OS benchmark (固定 loop 数,单位 wall 秒;n/a = 该 OS 无此测试)")
    print("| test | " + " | ".join(imgs) + " |")
    print("|" + "---|" * (len(imgs) + 1))
    for t in tests:
        cells = [rows.get(i, {}).get(t, "n/a") for i in imgs]
        print("| " + t + " | " + " | ".join(cells) + " |")
    return 0


if __name__ == "__main__":
    sys.exit(main())
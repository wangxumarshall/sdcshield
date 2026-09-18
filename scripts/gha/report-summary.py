#!/usr/bin/env python3
# report-summary.py — 汇总 report 作业下载的 15 份 allquality.yaml,生成「用例 × 版本」结果矩阵。
#
# 输入:all-results/verify-<series>-<sp>/results/allquality.yaml(actions/download-artifact
#   未 merge 时每镜像一个子目录)。
# 输出:stdout 一张 markdown 表:
#     行 = 测试用例(全量,按 15 版本求并集),列 = 15 个 OS 版本。
#     每格 = <RESULT>[<耗时>s],如 PASS[1.23s] / FAIL[0.10s] / SKIP[0.00s] / TIMEOUT[60.0s] / CRASH[0.0s]。
#     某用例在某个版本不存在(该版本未编译出该测试)→ 空(不渲染"n/a",留空更醒目)。
#   纯 stdlib,逐行流式解析 YAML(每份 ~1.4MB,15 份 ~20MB,不整文件入内存)。
#
# 结果态映射(sdcshield YAML `result:` 字段 → 表格 token):
#   pass                      → PASS
#   fail                      → FAIL
#   crash                     → CRASH
#   skip                      → SKIP
#   timed out                 → TIMEOUT
#   interrupted               → INTERRUPTED
#   invalid                   → INVALID
#   operating system error    → OSERR
#
# SPDX-License-Identifier: Apache-2.0
import glob
import os
import re
import sys

ARTIFACT_GLOB = "all-results/verify-*/results/allquality.yaml"

RESULT_TOKEN = {
    "pass": "PASS",
    "fail": "FAIL",
    "crash": "CRASH",
    "skip": "SKIP",
    "timed out": "TIMEOUT",
    "interrupted": "INTERRUPTED",
    "invalid": "INVALID",
    "operating system error": "OSERR",
}

TEST_RE = re.compile(r"^- test:\s*(\S+)")
RESULT_RE = re.compile(r"^  result:\s*(\S.*)$")
RUNTIME_RE = re.compile(r"^  test-runtime:\s*([0-9.]+)")


def parse_allquality(path):
    """流式解析一份 allquality.yaml → dict {test_name: (result_token, runtime_seconds)}。
    同用例多次出现(分片/fracture 已被 --max-test-loop-count 3 抑制,但保险起见取
    最后一次出现的记录,即最终态)。"""
    tests = {}
    cur_name = None
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            line = line.rstrip("\n")
            m = TEST_RE.match(line)
            if m:
                cur_name = m.group(1)
                continue
            if cur_name is None:
                continue
            mr = RESULT_RE.match(line)
            if mr:
                raw = mr.group(1).strip()
                token = RESULT_TOKEN.get(raw, raw.upper())
                tests[cur_name] = [token, tests.get(cur_name, [None, 0.0])[1]]
                continue
            mrt = RUNTIME_RE.match(line)
            if mrt:
                try:
                    rt = float(mrt.group(1))
                except ValueError:
                    rt = 0.0
                if cur_name in tests:
                    tests[cur_name][1] = rt
                else:
                    tests[cur_name] = [None, rt]
    # 清掉只有 runtime 没有 result 的残缺记录(result=None)
    return {k: v for k, v in tests.items() if v[0] is not None}


def main():
    per_image = {}       # image -> {test: (token, runtime)}
    test_order = []      # 保序去重的用例并集
    seen_tests = set()

    for f in sorted(glob.glob(ARTIFACT_GLOB)):
        # f 形如 all-results/verify-24.03-LTS-SP3/results/allquality.yaml
        parts = f.split(os.sep)
        if len(parts) < 3:
            continue
        img_dir = parts[1]
        if not img_dir.startswith("verify-"):
            continue
        img = img_dir[len("verify-"):]     # 24.03-LTS-SP3
        tests = parse_allquality(f)
        if not tests:
            print(f"warning: {f} 无有效测试记录", file=sys.stderr)
            continue
        per_image[img] = tests
        for t in tests:
            if t not in seen_tests:
                seen_tests.add(t)
                test_order.append(t)

    if not per_image:
        print("No allquality.yaml artifacts found.", file=sys.stderr)
        return 1

    imgs = sorted(per_image.keys())
    n_imgs = len(imgs)
    n_tests = len(test_order)

    # 汇总统计
    summary = {tok: 0 for tok in RESULT_TOKEN.values()}
    summary["n/a"] = 0

    def fmt_cell(t):
        rec = per_image.get(t)
        # rec: img -> (token, runtime)
        vals = []
        for img in imgs:
            r = per_image[img].get(t)
            if r is None:
                vals.append("")            # 该版本未编译出该用例
                summary["n/a"] += 1
            else:
                token, rt = r
                vals.append(f"{token}[{rt:.2f}s]")
                summary[token] = summary.get(token, 0) + 1
        return vals

    print("## 全量用例 × 全版本测试结果矩阵")
    print(f"(共 {n_tests} 个用例 × {n_imgs} 个 OS 版本;PASS=执行正确,FAIL=报错,"
          f"SKIP=跳过,TIMEOUT=超时,CRASH=崩溃,OSERR=系统错误,空=该版本无此用例)")
    print()
    print("| 用例 | " + " | ".join(imgs) + " |")
    print("|" + "---|" * (n_imgs + 1))
    for t in test_order:
        print("| " + t + " | " + " | ".join(fmt_cell(t)) + " |")

    # 尾部统计:每个结果态的单元格计数
    print()
    print("## 结果态统计(单元格计数)")
    print("| 结果态 | 计数 |")
    print("|---|---|")
    for tok in sorted(summary, key=lambda k: -summary[k]):
        print(f"| {tok} | {summary[tok]} |")
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except BrokenPipeError:
        # 输出被下游(如 head/tee)提前关闭管道;静默退出,避免误报非零退出码。
        try:
            devnull = os.open(os.devnull, os.O_WRONLY)
            os.dup2(devnull, sys.stdout.fileno())
        except OSError:
            pass
        sys.exit(0)

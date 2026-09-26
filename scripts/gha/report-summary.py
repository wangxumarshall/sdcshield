#!/usr/bin/env python3
# report-summary.py — 汇总 report 作业下载的 15 份 allquality.yaml,生成「用例 × 版本」结果矩阵。
#
# 输入:all-results/verify-<series>-<sp>/results/allquality.yaml(actions/download-artifact
#   未 merge 时每镜像一个子目录)。
# 可选 --previous DIR:上次成功全量运行的 artifact 根目录(同样内含
#   verify-*/results/allquality.yaml)。给定且解析出基线时,每格耗时附相对基线的增量:
#     <RESULT>[<耗时>s (±Δs)],Δ = 当前耗时 - 基线耗时(增加为正)。
#   仅对比耗时:结果态(PASS/FAIL/...)本身不参与对比,FAIL 格同样显示耗时 Δ。
#   当前有而基线无的用例(基线版本未编译出该测试)维持 <RESULT>[<耗时>s] 不显示 Δ;
#   基线有而当前无的用例不产生行(行集合以当前运行的并集为准)。
#   DIR 内无 allquality.yaml(如只有 smoke 产物,或 run 已过 artifact retention)→
#   视同基线缺失:打印说明行,不对比,不报错。
# 输出:stdout 一张 markdown 表:
#     行 = 测试用例(全量,按 15 版本求并集),列 = 15 个 OS 版本。
#     每格 = <RESULT>[<耗时>s],如 PASS[1.23s] / FAIL[0.10s] / SKIP[0.00s] / TIMEOUT[60.0s] / CRASH[0.0s]。
#     某用例在某个版本不存在(该版本未编译出该测试)→ 空(不渲染"n/a",留空更醒目)。
#   不带 --previous 时输出与历史版本逐字节一致(pr.yaml 的 quick summary 无参复用本脚本)。
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
import argparse
import glob
import os
import re
import sys

ARTIFACT_ROOT = "all-results"   # 当前运行 artifact 根(actions/download-artifact 落盘处)

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


def scan_artifacts(root):
    """扫描 <root>/verify-*/results/allquality.yaml → (per_image, test_order)。

    per_image: {镜像名: {用例: [token, runtime]}};test_order 为按 sorted glob 序
    保序去重的用例并集(基线扫描只需要 per_image,其 test_order 丢弃)。镜像名取
    路径相对 root 的第一段并剥 verify- 前缀(root 为绝对路径时同样正确)。"""
    per_image = {}
    test_order = []
    seen_tests = set()
    for f in sorted(glob.glob(os.path.join(root, "verify-*", "results", "allquality.yaml"))):
        # f 形如 <root>/verify-24.03-LTS-SP3/results/allquality.yaml
        img_dir = os.path.relpath(f, root).split(os.sep)[0]
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
    return per_image, test_order


def main():
    ap = argparse.ArgumentParser(
        description="汇总 multi-os-verify 各镜像 allquality.yaml 为「用例 × 版本」结果矩阵")
    ap.add_argument("--previous", metavar="DIR", default=None,
                    help="上次成功全量运行的 artifact 根目录(内含 verify-*/results/"
                         "allquality.yaml);给定后每格耗时附相对基线的增量 Δ(仅对比耗时)")
    args = ap.parse_args()

    per_image, test_order = scan_artifacts(ARTIFACT_ROOT)

    if not per_image:
        print("No allquality.yaml artifacts found.", file=sys.stderr)
        return 1

    # ---- 可选基线:上次成功全量运行。目录内无 allquality.yaml(全 smoke 产物或已过
    # retention)→ 基线缺失,后续只打印说明行、不对比,不算错误(基线是增强信息)。----
    per_image_prev = {}
    if args.previous is not None:
        per_image_prev, _ = scan_artifacts(args.previous)

    imgs = sorted(per_image.keys())
    n_imgs = len(imgs)
    n_tests = len(test_order)

    # 汇总统计
    summary = {tok: 0 for tok in RESULT_TOKEN.values()}
    summary["n/a"] = 0

    def fmt_cell(t):
        # rec: img -> (token, runtime)
        vals = []
        for img in imgs:
            r = per_image[img].get(t)
            if r is None:
                vals.append("")            # 该版本未编译出该用例
                summary["n/a"] += 1
                continue
            token, rt = r
            summary[token] = summary.get(token, 0) + 1
            prev = per_image_prev.get(img, {}).get(t) if per_image_prev else None
            if prev is not None:
                # 当前与基线同版本同用例都有耗时 → 附增量(增加为正)
                delta = rt - prev[1]
                vals.append(f"{token}[{rt:.2f}s ({delta:+.2f}s)]")
            else:
                # 基线无该用例(或无基线)→ 维持旧格式
                vals.append(f"{token}[{rt:.2f}s]")
        return vals

    print("## 全量用例 × 全版本测试结果矩阵")
    print(f"(共 {n_tests} 个用例 × {n_imgs} 个 OS 版本;PASS=执行正确,FAIL=报错,"
          f"SKIP=跳过,TIMEOUT=超时,CRASH=崩溃,OSERR=系统错误,空=该版本无此用例)")
    if args.previous is not None:
        if per_image_prev:
            print("(Δ 为相对上次成功全量运行的耗时变化,增加为正,仅对比耗时;"
                  "基线无该用例时不显示 Δ)")
        else:
            print(f"(无基线:{args.previous} 内未找到 allquality.yaml —— 7 天内可能无"
                  f"成功的全量运行 artifact,本次不对比耗时)")
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

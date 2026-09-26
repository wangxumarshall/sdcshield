# sdc-excite-reproduce M1（可观测性·采集器族）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 方案 M1 的采集器族——`sdc-collector@{percore,pmu,ras}` 三个 root 采集器（systemd 模板化、自监控、计数模式 PMU 组轮换）、M0 移交修缮与能力探测补全、A/B 开销基线，达成"24 小时采集无关键丢样 + 质量字段可见 + 常态开销达标"的 M1 退出标准。

**Architecture:** 三个独立 systemd 模板实例采集器（Python stdlib，单入口 `sdc_collector.py <name>` 分派），各自落 CSV 到数据根 `monitor/`；PMU 采集器以持久 `perf stat -x, -a` 子进程计数模式读出（无采样中断）；全部采集器每周期写自监控行（drop counter/last_success/rss）。复用 M0 的 `sdc_event.py`/`sdc_time.py` 契约。

**Tech Stack:** Python 3.11 stdlib（subprocess/json/csv/time/re）、bash（install.sh/systemd 模板）、Linux perf 6.6（`-x, -I` 计数模式）、systemd 模板单元（root）。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5；本计划实现其 §14.1 M1 行 + §14.2 补丁单元 5/6/7 + M0 终审移交清单）。范围裁定与偏差见文末。

## Global Constraints

- **一补丁一单元**：每 Task 一 commit，自验证后 `git commit -s`（尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容、无 Co-Authored-By）；分支 `feat/sdc-excite-reproduce-m1`（从 main 建，main 现已含全部 M0）；`git branch --show-current` 核实后才 commit；**绝不提交/推送 main**——集成经 PR（`GITHUB_PERSONAL_ACCESS_TOKEN` 在环境中可用 API 建 PR，合并由用户执行）。
- **零第三方 Python 依赖**（stdlib only，机器离线）。
- **性能口径**（v5 §6.8）：计数模式非采样；常态采集吞吐下降中位数 ≤3%；关键事件丢失率 <0.1% 且必须有 drop counter。
- **perf 访问**：`-a` 系统级与 uncore 需 root（paranoid=2 实证）——采集器全部经 root systemd 单元运行；本地测试用非 `-a` 或降级路径。
- **root 通道纪律**：root 操作经 `/tmp/sdc_inv_stage/su_run.py`（密码内置该临时文件，重启自清）；**子代理做"必须非 root"的验证一律 `env -u SDC_ROOT_PW`**（环境含该变量会被继承——M0 Task 6 实录）。
- **perf `-x,` 输出格式（2026-09-25 本机实测，perf 6.6）**：普通行 `ts,value,unit,event,time_enabled_ns,percent,,comment`；`--per-core` 行 `ts,core_id,agg_count,value,unit,event,time_enabled_ns,percent,,comment`（如 `0.500525610,S36-D0-C0,1,7976565,,cycles,502963210,100.00,,`）。`percent < 80` ⇒ 该窗口 `multiplex_degraded`。
- **x86-64 零改动**：不碰 `framework/`、`tests/`、根 `meson.build`；`tools/telemetry/` 与 `scripts/sdc-excite-reproduce/` 为唯一落点。
- **术语**：全部 sdc-excite-reproduce，不新引入 campaign。
- **战役保持停止**：本计划不启动战役（采集器独立于战役运行）；是否重启战役在计划外（用户决策，前置 = RCA 已定性）。
- **诚实纪律**：验证步骤引用真实命令输出；能力探测输出什么记什么，不得改逻辑迁就预期。

---

### Task 1: M0 移交修缮（tools/telemetry 五处小修）

**Files:**
- Modify: `tools/telemetry/sdc_event.py`（CLI 坏 JSON 优雅报错）
- Modify: `tools/telemetry/sdc_legacy_adapter.py`（幂等内容派生 ID + `_ts_to_ns` 精度 + 死代码）
- Modify: `tools/telemetry/sdc_topology.py`（`_node_has_memory` 改正则，解除与测试共享 `split()[3]` 的盲区）
- Modify: `tools/telemetry/sdc_time.py`（`interpolate` 三处边界）
- Test: `tools/telemetry/tests/test_sdc_event.py`、`test_sdc_legacy_adapter.py`、`test_sdc_topology.py`、`test_sdc_time.py`（各追加用例）

**Interfaces:**
- Consumes: M0 交付的四个模块（本仓 main 已含）
- Produces: `sdc_event` CLI 坏输入退出码 1 + stderr 单行报错（无 traceback）；`convert_ledger` 的 `event_id = "legacy-" + sha1(行内容)[:12]`（同一行两次转换 ID 相同）；`interpolate` 对 `span==0` 抛 `ValueError`（不再 ZeroDivisionError）、越界样本误差含外推距离

- [ ] **Step 1: 写失败测试（四个文件各追加用例）**

`test_sdc_event.py` 追加：

```python
def test_cli_bad_json_graceful(capsys=None):
    import subprocess, sys, os
    p = subprocess.run([sys.executable, os.path.join(os.path.dirname(__file__), "..",
        "sdc_event.py"), "validate-jsonl"], input="{oops\n", capture_output=True, text=True)
    assert p.returncode == 1
    assert "INVALID" in p.stderr and "Traceback" not in p.stderr
```

`test_sdc_legacy_adapter.py` 追加：

```python
def test_event_id_content_derived_stable():
    evs = ada.convert_ledger(os.path.join(FIX, "ledger.csv"), "c0")
    evs2 = ada.convert_ledger(os.path.join(FIX, "ledger.csv"), "c0")
    assert [e["event_id"] for e in evs] == [e["event_id"] for e in evs2]
    assert all(e["event_id"].startswith("legacy-") and len(e["event_id"]) == 19 for e in evs)
```

`test_sdc_topology.py` 追加（替代原 `test_has_memory_matches_memtotal` 的共享解析）：

```python
def test_has_memory_uses_regex_independent_of_impl():
    # 前缀变体免疫：构造伪 meminfo 行直接验证正则口径（不依赖实现的 split 索引）
    import sdc_topology as st
    class FakeN:  # 伪 node 目录对象
        def __init__(self, text): self.text = text
    assert st._memtotal_kb(FakeN("Node 0 MemTotal:       264123904 kB")) == 264123904
    assert st._memtotal_kb(FakeN("MemTotal:       0 kB")) == 0
    assert st._memtotal_kb(FakeN("Node 3 MemTotal:  1024 kB")) == 1024
```

`test_sdc_time.py` 追加：

```python
def test_interpolate_zero_span_raises():
    import pytest
    a = {"realtime_ns": 100, "cntvct": 5, "mapping_error_ns": 0}
    with pytest.raises(ValueError):
        sdc_time.interpolate([a, dict(a)], 100)

def test_interpolate_extrapolation_error_grows():
    a = {"realtime_ns": 1000, "cntvct": 100, "mapping_error_ns": 0}
    b = {"realtime_ns": 2000, "cntvct": 300, "mapping_error_ns": 0}
    _, err_in = sdc_time.interpolate([a, b], 1500)
    _, err_out = sdc_time.interpolate([a, b], 500)   # 越界外推
    assert err_out > err_in >= 0
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/ -q
```
Expected: 新增 5 用例中 4 个 FAIL（优雅报错/ID 稳定/zero_span/外推误差），`_memtotal_kb` 用例 FAIL（函数不存在）。

- [ ] **Step 3: 实现五处修改**

`sdc_event.py`：`__main__` 块整体替换为：

```python
if __name__ == "__main__":
    mode = sys.argv[1] if len(sys.argv) > 1 else "validate"
    if mode == "validate":
        data = [json.load(sys.stdin)]
    else:
        data = []
        for line in sys.stdin:
            if not line.strip():
                continue
            try:
                data.append(json.loads(line))
            except json.JSONDecodeError as e:
                print(f"INVALID json line {e.lineno}: {e.msg}", file=sys.stderr)
                sys.exit(1)
    bad = 0
    for ev in data:
        errs = validate_event(ev)
        if errs: bad += 1; print(f"INVALID {ev.get('event_id')}: {errs}", file=sys.stderr)
    sys.exit(1 if bad else 0)
```

`sdc_legacy_adapter.py`：`_ts_to_ns` 改 `return int(time.mktime(time.strptime(ts, "%Y-%m-%d %H:%M:%S"))) * 10**9`；`convert_ledger` 循环里 `event_id=f"legacy-{i:04d}"` 改为内容派生：

```python
        rid = hashlib.sha1(("|".join([row["ts"], row["label"], row.get("rc", ""),
                                       row.get("test", ""), row.get("seed", ""),
                                       row.get("note", "")])).encode()).hexdigest()[:12]
        ev = sdc_event.make_event(
            event_id=f"legacy-{rid}", event_type=etype,
```

（文件头 import 加 `hashlib`；删除 `rc` 的 `.replace("rc=", "")` 死代码与 `for i, ... enumerate` 的未用 `i`——改 `for row in parse_ledger(path):`。）

`sdc_topology.py`：新增并替换：

```python
import re as _re

def _memtotal_kb(node_dir):
    """正则口径：对前缀变体（Node <id> MemTotal: / MemTotal:）免疫。"""
    try:
        for line in open(f"{node_dir}/meminfo"):
            m = _re.search(r"MemTotal:\s+(\d+)", line)
            if m:
                return int(m.group(1))
    except OSError:
        pass
    return 0

def _node_has_memory(node_dir):
    return _memtotal_kb(node_dir) > 0
```

（`test_has_memory_matches_memtotal` 既有测试的解析同步改用 `sdc_topology._memtotal_kb`。）

`sdc_time.py`：`interpolate` 整体替换：

```python
def interpolate(anchors, realtime_ns):
    """最近两锚点线性插值 → (cntvct 估计, 误差界 ns)。anchors 按 realtime 升序、≥2 个。"""
    if len(anchors) < 2:
        raise ValueError("需要 ≥2 个锚点")
    a, b = anchors[-2], anchors[-1]
    span = b["realtime_ns"] - a["realtime_ns"]
    if span <= 0:
        raise ValueError("锚点 realtime 无跨度（span<=0）")
    est = a["cntvct"] + (realtime_ns - a["realtime_ns"]) / span * (b["cntvct"] - a["cntvct"])
    err = max(a.get("mapping_error_ns", 0), b.get("mapping_error_ns", 0)) + span
    if realtime_ns < a["realtime_ns"] or realtime_ns > b["realtime_ns"]:
        # 越界外推：误差加上外推距离（对称计），且不假装包络精度
        err += abs(realtime_ns - (a["realtime_ns"] if realtime_ns < a["realtime_ns"]
                                  else b["realtime_ns"]))
    return int(est), err
```

- [ ] **Step 4: 全套测试通过**

```bash
python3 -m pytest tools/telemetry/tests/ -q
```
Expected: 21 passed（原 16 + 新 5）。

- [ ] **Step 5: Commit**

```bash
git add tools/telemetry/
git commit -s -m "tools: M0 终审移交修缮——CLI 优雅报错/适配器幂等 ID/topology 正则/interpolate 边界

Signed-off-by: wangxu <wangxumarshall@qq.com>"
git push -u origin feat/sdc-excite-reproduce-m1
```

---

### Task 2: capabilities v2.1 探测补全 + A/B 开销基线锚点

**Files:**
- Modify: `scripts/sdc-excite-reproduce/collect_inventory.sh`（第 16 节增补：scaling_driver、per-PMU 计数器预算）
- Create: `tools/telemetry/ab_baseline.py`
- Test: `tools/telemetry/tests/test_ab_baseline.py`

**Interfaces:**
- Consumes: `collect_inventory.sh` 既有第 16 节结构（`$OUT`/`$ROOT_MODE`）
- Produces: capabilities.env 新键 `CPUFREQ_DRIVER`（真驱动名，如 cppc_cpufreq）、`PMU_CORE_COUNTERS=<int>`（无复用容量实测值）、`AB_BASELINE_FILE=<路径>`；`ab_baseline.py` 用法 `ab_baseline.py --runs N --out FILE`，输出 JSON `{"runs": [{"loop_total": N, "wall_s": F}, ...], "median_loop_total": N}`（M1 Task 7 复测对照）

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_ab_baseline.py`：

```python
import os, sys, json
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import ab_baseline

FAKE_YAML = """tests:
- test: zstd19
  result: pass
  threads:
  - thread: main
  - thread: 0
    loop-count: 1200
  - thread: 1
    loop-count: 1180
"""

def test_parse_loop_total():
    assert ab_baseline.parse_loop_total(FAKE_YAML) == 2380

def test_parse_loop_total_missing_is_zero():
    assert ab_baseline.parse_loop_total("tests:\n- test: zstd19\n  result: pass\n") == 0
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_ab_baseline.py -q
```
Expected: FAIL（模块不存在）。

- [ ] **Step 3: 实现 ab_baseline.py + collect_inventory 增补**

`tools/telemetry/ab_baseline.py`：

```python
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
        subprocess.run([sdc_bin, "-e", "zstd19", "-t", str(t_ms), "-n", str(n_threads),
                        "-o", out], check=True, capture_output=True, timeout=t_ms / 1000 * 5 + 60)
        return parse_loop_total(open(out).read())
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
```

`collect_inventory.sh` 第 16 节内增补三行（插在 `GOVERNOR=` 行之后）：

```bash
  echo "CPUFREQ_DRIVER=$(cat "$cpu0f/scaling_driver" 2>/dev/null || echo none)"
```

并在该节末尾（`cp "$CAP" ...` 之前）追加 per-PMU 计数器预算探测（root 通道，M0 终审判定的 spec §14.2 单元 3 缺口）：

```bash
# per-PMU 计数器预算探测：递增事件数直至出现 multiplex（time 百分比 <100）
# 列位依据（Global Constraints 实测格式）：普通 -a 行 percent 在 $6；--per-core 行在 $8——勿混用
probe_pmu_budget() { # $1=perf 前缀（固定 "-a"），$2=事件名（重复挂 N 份占计数器）
    local n evs pct
    for n in 2 3 4 5 6 7 8; do
        evs=$(seq 1 $n | sed "s/.*/$2/" | paste -sd,)
        pct=$(eval perf stat -x, $1 -e "$evs" -- true 2>&1 \
              | awk -F, '{gsub(/ /,"",$6); if ($6 ~ /^[0-9.]+$/ && $6+0 < 100) {print $6; exit}}')
        if [ -n "$pct" ]; then echo $((n-1)); return; fi
    done
    echo 8
}
CORE_BUDGET=$(probe_pmu_budget "-a" "cycles")
if [ "$ROOT_MODE" != none ]; then
    echo "PMU_CORE_COUNTERS=$CORE_BUDGET  # 无复用容量实测（递增事件至首次 multiplex）" >> "$CAP"
else
    echo "PMU_CORE_COUNTERS=unknown  # 需 root（perf -a）" >> "$CAP"
fi
echo "AB_BASELINE_FILE=$HOME/sdc-excite-reproduce/monitor/ab_baseline.json" >> "$CAP"
```

- [ ] **Step 4: 测试 + 实测验证**

```bash
python3 -m pytest tools/telemetry/tests/test_ab_baseline.py -q   # 2 passed
bash -n scripts/sdc-excite-reproduce/collect_inventory.sh
# 取 A/B 锚点（采集器未部署=无监控态；-n 32 留共享机器余量）
python3 tools/telemetry/ab_baseline.py --runs 5 --out /home/sdc/sdc-excite-reproduce/monitor/ab_baseline.json
# 探测验证（root 通道）
timeout 120 python3 /tmp/sdc_inv_stage/su_run.py "bash /home/sdc/wangxu/sdcshield/scripts/sdc-excite-reproduce/collect_inventory.sh /tmp/inv-m1t2" 2>&1 | tr -d '\r' | sed '1{/^密码：* *$/d}' | tail -3
grep -E "CPUFREQ_DRIVER|PMU_CORE_COUNTERS" /tmp/inv-m1t2/*/16_capabilities.env
```
Expected: pytest 2 passed；`CPUFREQ_DRIVER=cppc_cpufreq`（本机事实，如不符如实报告）；`PMU_CORE_COUNTERS=5` 或 6（armv8_pmuv3 实测值，如实记录）；ab_baseline.json 生成且 median 为正整数（真实数值进报告）。

- [ ] **Step 5: Commit**

```bash
git add tools/telemetry/ab_baseline.py tools/telemetry/tests/test_ab_baseline.py scripts/sdc-excite-reproduce/collect_inventory.sh
git commit -s -m "tools: capabilities v2.1（scaling_driver + per-PMU 预算探测）+ A/B 开销基线锚点

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 3: sdc-collector@.service 模板 + 采集器框架

**Files:**
- Create: `scripts/sdc-excite-reproduce/systemd/sdc-collector@.service.in`
- Create: `tools/telemetry/sdc_collector.py`（入口分派 + 公共基类 + 自监控）
- Modify: `scripts/sdc-excite-reproduce/install.sh`（模板安装）
- Test: `tools/telemetry/tests/test_sdc_collector.py`

**Interfaces:**
- Consumes: 数据根 `${SDC_EXCITE_REPRODUCE_DIR:-/home/sdc/sdc-excite-reproduce}`；capabilities.env（节奏与预算键，缺失有默认）
- Produces: `class SdcCollector`（子类实现 `collect_once() -> list[str]`（CSV 行）与 `NAME`/`HEADER`；基类管周期循环、CSV append、自监控行、SIGTERM 优雅退出）；自监控文件 `monitor/collector_self.csv`（列：`ts,collector,samples_total,samples_dropped,period_s,loop_duration_s,last_success_ts,rss_kb`）；入口 `python3 sdc_collector.py <percore|pmu|ras> [--period-s N]`；Task 4-6 的三个子类经 `sdc_collector.register()` 注册

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_sdc_collector.py`：

```python
import os, sys, csv
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector as sc

class EchoCollector(sc.SdcCollector):
    NAME, HEADER = "echo", ["ts", "value"]
    def collect_once(self):
        return [[self.now_iso(), "42"]]

def test_base_class_loop_and_selfmon(tmp_path):
    c = EchoCollector(period_s=0.2, out_dir=str(tmp_path), selfmon_path=str(tmp_path / "self.csv"),
                      max_cycles=2)
    c.run()
    rows = list(csv.reader(open(tmp_path / "echo.csv")))
    assert rows[0] == ["ts", "value"] and len(rows) == 3          # header + 2 行
    selfmon = list(csv.DictReader(open(tmp_path / "self.csv")))
    assert len(selfmon) == 2
    assert selfmon[-1]["samples_total"] == "2" and selfmon[-1]["samples_dropped"] == "0"

def test_dispatch_unknown_raises():
    import pytest
    with pytest.raises(SystemExit):
        sc.main(["nosuch"])
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector.py -q
```
Expected: FAIL（模块不存在）。

- [ ] **Step 3: 实现框架 + 模板 + install.sh**

`tools/telemetry/sdc_collector.py`：

```python
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
            time.sleep(max(0.0, self.period_s - (time.monotonic() - t0)))

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
    main(sys.argv[1:])
```

`scripts/sdc-excite-reproduce/systemd/sdc-collector@.service.in`：

```ini
[Unit]
Description=SDCShield sdc-excite-reproduce collector (%i)
StartLimitIntervalSec=0

[Service]
Type=simple
ExecStart=/usr/bin/python3 @REPO@/tools/telemetry/sdc_collector.py %i
Environment=SDC_EXCITE_REPRODUCE_DIR=@CAMPAIGN_DIR@
Restart=always
RestartSec=10
TimeoutStopSec=30
KillMode=control-group

[Install]
WantedBy=multi-user.target
```

`install.sh`：在 `sed_inplace ... sdc-monitor.service.in` 行后追加：

```bash
sed_inplace "$HERE/systemd/sdc-collector@.service.in" /etc/systemd/system/sdc-collector@.service
```

并更新结尾 echo 清单加一行 `  采集器模板: /etc/systemd/system/sdc-collector@.service（systemctl start sdc-collector@percore 等）`。

- [ ] **Step 4: 测试通过（临时桩注册——此时三个子模块还不存在，入口 main 会 ImportError；本步只测基类）**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector.py -q
```
Expected: 2 passed。`main()` 的子模块 import 为 try/except——Task 3 时只有基类也能跑（`main(["nosuch"])` 经 argparse choices 报 SystemExit）；Task 4-6 各交付后 `_REGISTRY` 自动长全。

- [ ] **Step 5: Commit**

```bash
git add tools/telemetry/sdc_collector.py tools/telemetry/tests/test_sdc_collector.py scripts/sdc-excite-reproduce/systemd/sdc-collector@.service.in scripts/sdc-excite-reproduce/install.sh
git commit -s -m "tools: sdc-collector@.service systemd 模板 + 采集器公共框架（自监控/drop counter/优雅退出）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 4: collector@percore（逐核占用/实测频率/驻留直方图，补丁单元 5）

**Files:**
- Create: `tools/telemetry/sdc_collector_percore.py`
- Test: `tools/telemetry/tests/test_sdc_collector_percore.py`

**Interfaces:**
- Consumes: Task 3 `SdcCollector`/`register`；`/proc/stat`、`/sys/devices/system/cpu/cpu*/cpufreq/cpuinfo_cur_freq`（root；缺失降级 `scaling_cur_freq`，再缺失列空——v5 附录 B 81 机 declared-absent 口径）
- Produces: `monitor/percore.csv`（`ts` + `util_cpu<N>_pct` ×N + `freq_cpu<N>_khz` ×N，N=possible 核数）；`monitor/freq_residency.log`（10min 每核 100MHz 桶快照 + 日累计，v5 12 维矩阵 #7/#8）

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_sdc_collector_percore.py`：

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector_percore as pc

STAT_A = "cpu0 100 0 100 1000 0 0 0 0 0\n"
STAT_B = "cpu0 100 0 100 1500 0 0 0 0 0\n"   # idle +500 拍 = 5s 全闲 → util 0%
STAT_C = "cpu0 400 0 100 1500 0 0 0 0 0\n"   # user +300 → util 300/(300+500)=37.5%

def test_util_delta():
    assert pc.util_pct(STAT_A, STAT_B) == [0.0]          # idle +500 → 全闲 0%
    u = pc.util_pct(STAT_A, STAT_C)                       # user 100→400(+300), idle 1000→1500(+500)
    assert abs(u[0] - 37.5) < 0.1                         # busy 300 / total 800

def test_bucket_100mhz():
    assert pc.freq_bucket_khz(2599999) == 2500000
    assert pc.freq_bucket_khz(2600000) == 2600000
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector_percore.py -q
```
Expected: FAIL（模块不存在）。

- [ ] **Step 3: 实现**

`tools/telemetry/sdc_collector_percore.py`：

```python
#!/usr/bin/env python3
"""collector@percore — 逐核占用率 + 实测频率 + 频点驻留直方图（v5 §6.7 矩阵 #1/#7/#8）。

csv: monitor/percore.csv  ts + util_cpuN_pct×N + freq_cpuN_khz×N
驻留: 内存 100MHz 桶累计，每 10 个周期（或 RESIDENCY_EVERY_S）快照追加 freq_residency.log
频率口径: cpuinfo_cur_freq（实测值，root）优先，降级 scaling_cur_freq（请求值），再降级列空
"""
import glob, os, time
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
```

（`SdcCollector.__init__` 需允许子类后设 HEADER 再 `_init_csv()`——Task 3 框架已满足：`_init_csv` 是幂等方法且 HEADER 为类属性可实例后覆写。若 CSV 已存在且列头不同，追加前须校验列头一致：`collect_once` 首轮比对首行，不一致则写 stderr 并 exit 1——把这 3 行防御加进 `__init__` 尾部。）

- [ ] **Step 4: 测试 + 实跑验证**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector_percore.py -q   # 2 passed
# 实跑 3 周期（非 root：频率降级 scaling_cur_freq；root 才有实测值）
env -u SDC_ROOT_PW python3 - <<'EOF'
import sys; sys.path.insert(0, "tools/telemetry")
import sdc_collector_percore as pc
c = pc.PerCoreCollector(period_s=1, out_dir="/tmp/percore-test", selfmon_path="/tmp/percore-test/self.csv", max_cycles=3)
c.run()
EOF
head -2 /tmp/percore-test/percore.csv | cut -c1-160; wc -l /tmp/percore-test/percore.csv
```
Expected: 4 行（header+3）；ts + 128 util 列 + 128 freq 列；util 数值 0-100；freq 非 root 时为 scaling 值或空（如实记录）。

- [ ] **Step 5: Commit**

```bash
git add tools/telemetry/sdc_collector_percore.py tools/telemetry/tests/test_sdc_collector_percore.py
git commit -s -m "tools: collector@percore — 逐核占用/实测频率/驻留直方图（v5 补丁单元 5）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 5: collector@pmu（perf 计数模式持久进程 + 组轮换，补丁单元 6）

**Files:**
- Create: `tools/telemetry/sdc_collector_pmu.py`
- Test: `tools/telemetry/tests/test_sdc_collector_pmu.py`

**Interfaces:**
- Consumes: Task 3 框架；capabilities.env 键 `PMU_CORE_COUNTERS`（默认 6）；perf `-x, -a` root（systemd 单元内）
- Produces: `monitor/pmu_core.csv`（宽表：`ts,group,core_id,percent_covered` + 每事件列；行形=每 interval 每 core 一行）；`monitor/pmu_uncore.csv`（`ts,group,device,percent_covered` + 事件列）；`GROUPS` 常量（v5 §6.2 六组菜单，raw 码即能力锚点）——M1 核表用 `core_base/core_memory/core_path` 轮换、uncore 用 `l3c/hha/ddrc` 轮换；`multiplex` 判定函数 `is_degraded(percent) -> bool`（<80.0）

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_sdc_collector_pmu.py`：

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector_pmu as pm

PERCORE_LINE = "0.500525610,S36-D0-C0,1,7976565,,cycles,502963210,100.00,,"
PERCORE_MUX = "1.500525610,S36-D0-C1,1,1234,,cycles,502963210,42.00,,"
PLAIN_LINE = "0.500525610,287719,,hisi_sccl1_ddrc0/flux_rd/,693440,100.00,,"

def test_parse_percore_line():
    ts, core, val, ev, pct = pm.parse_percore(PERCORE_LINE)
    assert (ts, core, val, ev, pct) == ("0.500525610", "S36-D0-C0", 7976565, "cycles", 100.0)

def test_is_degraded():
    assert pm.is_degraded(100.0) is False and pm.is_degraded(42.0) is True

def test_parse_plain_line():
    ts, val, ev, pct = pm.parse_plain(PLAIN_LINE)
    assert ev == "hisi_sccl1_ddrc0/flux_rd/" and pct == 100.0

def test_groups_shape():
    assert set(pm.GROUPS["core_base"]) <= {"cpu_cycles", "inst_retired", "inst_spec",
        "exe_stall_cycle", "stall_frontend", "stall_backend"}
    assert len(pm.GROUPS["l3c"]) == 4
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector_pmu.py -q
```
Expected: FAIL（模块不存在）。

- [ ] **Step 3: 实现**

`tools/telemetry/sdc_collector_pmu.py`：

```python
#!/usr/bin/env python3
"""collector@pmu — 核+uncore PMU 计数模式采集（v5 §6.2/§6.7 矩阵 #3/#4）。

设计：两个持久 `perf stat -x, -a -I<ms>` 子进程（计数模式、非采样——v5 §6.8 裁定）；
核表 --per-core + 组轮换（core_base/core_memory/core_path 各 ≤PMU_CORE_COUNTERS 事件）；
uncore 设备限定（hisi_sccl*_{l3c,hha,ddrc}，事件名经 sysfs events/ 发现式枚举）；
percent<80 → 该窗口 multiplex_degraded（宽表 percent_covered 列保留可见——诚实呈现）。
"""
import glob, os, re, subprocess, time
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

@register
class PmuCollector(SdcCollector):
    NAME = "pmu"
    DEFAULT_PERIOD_S = 20.0
    def __init__(self, **kw):
        super().__init__(**kw)
        self.budget = int(os.environ.get("PMU_CORE_COUNTERS", "6"))
        self._core_group = ""
        self._uncore_group = ""
        self._p_core = self._p_uncore = None
        self._rotate_at = 0.0
        self._core_hdr_written = self._uncore_hdr_written = False
        self._core_out = open(os.path.join(os.path.dirname(self.csv_path), "pmu_core.csv"), "a")
        self._uncore_out = open(os.path.join(os.path.dirname(self.csv_path), "pmu_uncore.csv"), "a")
    def _spawn(self):
        self._core_group = CORE_ROTATION[0] if not self._core_group else \
            CORE_ROTATION[(CORE_ROTATION.index(self._core_group) + 1) % len(CORE_ROTATION)]
        self._uncore_group = UNCORE_ROTATION[0] if not self._uncore_group else \
            UNCORE_ROTATION[(UNCORE_ROTATION.index(self._uncore_group) + 1) % len(UNCORE_ROTATION)]
        if self._p_core:
            self._p_core.terminate()
        if self._p_uncore:
            self._p_uncore.terminate()
        core_evs = ["cycles"] + [f"r{c:x}" for _, c in
                                 list(RAW[self._core_group].items())[:self.budget - 1]]
        uncore_evs = _uncore_events(self._uncore_group)
        self._p_core = subprocess.Popen(
            ["perf", "stat", "-x,", "-a", "--per-core", f"-I{int(self.period_s*1000)}",
             "-e", ",".join(core_evs)], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True)
        args = ["perf", "stat", "-x,", "-a", f"-I{int(self.period_s*1000)}"]
        if uncore_evs:
            args += ["-e", ",".join(uncore_evs)]
            self._p_uncore = subprocess.Popen(args, stdout=subprocess.PIPE,
                                              stderr=subprocess.DEVNULL, text=True)
        self._rotate_at = time.monotonic() + ROTATE_S
    def collect_once(self):
        if time.monotonic() >= self._rotate_at or self._p_core is None:
            self._spawn()
            time.sleep(self.period_s + 0.5)      # 等首个 interval 行
        rows = []
        core_events = ["cycles"] + list(RAW[self._core_group].keys())[:self.budget - 1]
        if not self._core_hdr_written:
            self._core_out.write(",".join(["ts", "group", "core", "percent_covered"]
                                          + core_events) + "\n")
            self._core_hdr_written = True
        # 非阻塞读已有输出（子进程持续产出；本方法每 period 被调一次）
        import select
        acc = {}
        while True:
            r, _, _ = select.select([self._p_core.stdout], [], [], 0.2)
            if not r:
                break
            line = self._p_core.stdout.readline()
            if not line:
                break
            if line.startswith("#") or "," not in line:
                continue
            try:
                ts, core, val, ev, pct = parse_percore(line)
            except (IndexError, ValueError):
                continue
            acc.setdefault((ts, core), {"pct": pct})[ev] = val
        for (ts, core), d in sorted(acc.items()):
            self._core_out.write(",".join([self.now_iso(), self._core_group, core,
                                           f"{d['pct']}"] + [str(d.get(e, "")) for e in core_events]) + "\n")
        # uncore 同构（plain 行形）
        if self._p_uncore:
            uacc = {}
            while True:
                r, _, _ = select.select([self._p_uncore.stdout], [], [], 0.2)
                if not r:
                    break
                line = self._p_uncore.stdout.readline()
                if not line or line.startswith("#"):
                    continue
                try:
                    ts, val, ev, pct = parse_plain(line)
                except (IndexError, ValueError):
                    continue
                dev = ev.split("/")[0]
                uacc.setdefault((ts, dev), {"pct": pct})[ev] = val
            devs = sorted({k[1] for k in uacc}) or _uncore_events(self._uncore_group)
            if not self._uncore_hdr_written and uacc:
                evnames = list(next(iter(uacc.values())).keys())
                self._uncore_out.write(",".join(["ts", "group", "device", "percent_covered"]
                                                + evnames) + "\n")
                self._uncore_hdr_written = True
            for (ts, dev), d in sorted(uacc.items()):
                self._uncore_out.write(",".join([self.now_iso(), self._uncore_group, dev,
                                                 f"{d['pct']}"] + [str(v) for k, v in d.items()
                                                 if k != "pct"]) + "\n")
        self._core_out.flush(); self._uncore_out.flush()
        return []                                   # 数据直写专用文件，不走基类 CSV
```

（核事件列表在 `_spawn` 内联构造：`["cycles"] + [f"r{c:x}" for _, c in list(RAW[组].items())[:预算-1]]`——cycles 常驻 + 预算内 raw 事件。）

- [ ] **Step 4: 测试 + root 实跑验证**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector_pmu.py -q        # 4 passed
# root 实跑 3 周期 ×5s（短周期验证管线，非生产 20s）
timeout 300 python3 /tmp/sdc_inv_stage/su_run.py "cd /home/sdc/wangxu/sdcshield/tools/telemetry && SDC_EXCITE_REPRODUCE_DIR=/tmp/pmu-test python3 -c \"
import sys; sys.path.insert(0, '.')
import sdc_collector_pmu as pm
c = pm.PmuCollector(period_s=5, out_dir='/tmp/pmu-test/monitor', selfmon_path='/tmp/pmu-test/self.csv', max_cycles=3)
c.run()\"" 2>&1 | tr -d '\r' | sed '1{/^密码：* *$/d}' | tail -3
head -3 /tmp/pmu-test/monitor/pmu_core.csv; echo ---; head -3 /tmp/pmu-test/monitor/pmu_uncore.csv; cat /tmp/pmu-test/self.csv
```
Expected: pmu_core.csv 有 header+数据行（S*-D*-C* 核号、percent_covered=100.00、cycles 等列有值）；pmu_uncore.csv 有 hisi_sccl* 设备行；self.csv 3 周期 samples_total≥1（本采集器直写不走基类行——samples_total 反映周期数即可，如实记录）；**若 percent_covered <80 出现在 3 事件组——如实记录（说明预算探测与实际有出入，调 `_spawn` 的事件数）**。

- [ ] **Step 5: Commit**

```bash
git add tools/telemetry/sdc_collector_pmu.py tools/telemetry/tests/test_sdc_collector_pmu.py
git commit -s -m "tools: collector@pmu — perf 计数模式持久进程 + 核/uncore 组轮换 + multiplex 可见（v5 补丁单元 6）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 6: collector@ras（EDAC/journal 流/spurious canary/rasdaemon 监护/BERT，补丁单元 7 本机部分）

**Files:**
- Create: `tools/telemetry/sdc_collector_ras.py`
- Test: `tools/telemetry/tests/test_sdc_collector_ras.py`

**Interfaces:**
- Consumes: Task 3 框架；EDAC sysfs（发现式 `mc*/{ce,ue}_count` 与 `mc*/dimm*/`）、`journalctl -k --since`、`systemctl is-active rasdaemon`、`/sys/firmware/acpi/tables/BERT`
- Produces: `monitor/ras_edac.csv`（`ts,mc,ce,ue` + 发现式 `dimm_<mc>_<idx>` 列）；`monitor/journal_watch.log`（RAS 关键字流 + spurious canary 计数行 `SPURIOUS total=<n> source=dmesg`——per-CPU 版属 M2 BPF 升级，如实标注）；rasdaemon 掉线告警行；数据根 `inventory/BERT.bin`（启动一次）

- [ ] **Step 1: 写失败测试**

`tools/telemetry/tests/test_sdc_collector_ras.py`：

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_collector_ras as ras

def test_keywords_filter():
    line_bad = "hns3 0000:7d:00.0: ... error"
    line_hit = "EDAC MC0: 1 Corrected error"
    line_stall = "rcu: INFO: rcu_sched self-detected stall on CPU"
    assert ras.ras_keyword(line_bad) is False          # hns3 噪声排除
    assert ras.ras_keyword(line_stall) is False        # rcu-stall 噪声排除（专属清单另计）
    assert ras.ras_keyword(line_hit) is True

def test_spurious_count():
    text = "a spurious translation fault b\nnothing\nspurious translation fault c\n"
    assert ras.count_spurious(text) == 2
```

- [ ] **Step 2: 运行确认失败**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector_ras.py -q
```
Expected: FAIL（模块不存在）。

- [ ] **Step 3: 实现**

`tools/telemetry/sdc_collector_ras.py`：

```python
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
```

（`import sys` 由文件头 `import ... sys` 引入——BERT 失败走 except 打 stderr 一行后继续，不中断采集。）

- [ ] **Step 4: 测试 + 实跑验证**

```bash
python3 -m pytest tools/telemetry/tests/test_sdc_collector_ras.py -q      # 2 passed
env -u SDC_ROOT_PW python3 - <<'EOF'
import sys; sys.path.insert(0, "tools/telemetry")
import sdc_collector_ras as ras
c = ras.RasCollector(period_s=1, out_dir="/tmp/ras-test", selfmon_path="/tmp/ras-test/self.csv", max_cycles=2)
c.run()
EOF
cat /tmp/ras-test/ras_edac.csv; wc -l /tmp/ras-test/journal_watch.log; cat /tmp/ras-test/self.csv
```
Expected: ras_edac.csv 含本机 mc0-mcN 行（EDAC 存在则 ce/ue 数值；无则仅 header——如实记录）；self.csv 2 周期 samples_dropped=0；journal_watch.log 至少 0 行（无关键字为正常）。

- [ ] **Step 5: Commit（全套重跑）**

```bash
python3 -m pytest tools/telemetry/tests/ -q           # 期望全绿（基类+三采集器+M0 全部）
git add tools/telemetry/sdc_collector_ras.py tools/telemetry/tests/
git commit -s -m "tools: collector@ras — EDAC/journal 流/spurious canary(dmesg 版)/rasdaemon 监护/BERT dump（v5 补丁单元 7 本机部分）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
```

---

### Task 7: 部署、冒烟与 M1 验收门

**Files:**
- Modify: `scripts/sdc-excite-reproduce/NEW_BOARD_ONBOARDING.md`（采集器部署/验收段落）
- Create: `scripts/sdc-excite-reproduce/acceptance_m1.sh`

**Interfaces:**
- Consumes: Task 3-6 全部交付；Task 2 的 ab_baseline.json 锚点与 PMU_CORE_COUNTERS；root 通道（install.sh + systemctl）
- Produces: 已部署 `sdc-collector@{percore,pmu,ras}`（enabled+active）；`acceptance_m1.sh`（丢样核对 + 质量字段核对 + A/B 复测，退出码 0/1）；M1 退出标准判定记录

- [ ] **Step 1: 写验收脚本**

`scripts/sdc-excite-reproduce/acceptance_m1.sh`：

```bash
#!/bin/bash
# acceptance_m1.sh — M1 退出标准核对（v5 §14.1：24h 采集无关键丢样 + 质量字段可见 + 开销达标）
# 用法: bash acceptance_m1.sh <运行小时数，默认 24>     # root 跑（读 systemd 与 root 文件）
set -u
cd "$(dirname "$0")/../.."                              # 仓库根（ab_baseline 相对路径）
HOURS="${1:-24}"
ROOT="${SDC_EXCITE_REPRODUCE_DIR:-/home/sdc/sdc-excite-reproduce}"
MON="$ROOT/monitor"
FAIL=0
need() { echo "✗ $1"; FAIL=1; }
ok()   { echo "✓ $1"; }

# 1) 三采集器 active
for c in percore pmu ras; do
    [ "$(systemctl is-active sdc-collector@$c)" = active ] && ok "sdc-collector@$c active" \
        || need "sdc-collector@$c 非 active"
done

# 2) 丢样：selfmon 里 samples_dropped 合计为 0（或占 samples_total <0.1%）
python3 - "$MON/collector_self.csv" <<'EOF' && ok "丢样 <0.1%" || need "丢样超标"
import csv, sys
rows = list(csv.DictReader(open(sys.argv[1])))
tot = sum(int(r["samples_total"]) for r in rows)
dropped = sum(int(r["samples_dropped"]) for r in rows)
sys.exit(0 if tot and dropped / tot < 0.001 else 1)
EOF

# 3) 数据新鲜度：percore.csv 末行 ts 距今 < 2×周期
LAST=$(tail -1 "$MON/percore.csv" | cut -d, -f1)
AGE=$(( $(date +%s) - $(date -d "$LAST" +%s) ))
[ "$AGE" -lt 180 ] && ok "percore 新鲜 (${AGE}s)" || need "percore 停更 ${AGE}s"

# 4) PMU 质量字段可见：pmu_core.csv 有 percent_covered 列且含 <100 样本不丢失（multiplex 诚实呈现）
head -1 "$MON/pmu_core.csv" | grep -q percent_covered && ok "PMU 质量字段可见" \
    || need "pmu_core.csv 缺 percent_covered"

# 5) 时长门：运行满 HOURS 小时（用 collector_self.csv 最早最晚 ts 差）
python3 - "$MON/collector_self.csv" "$HOURS" <<'EOF' && ok "运行 ≥${HOURS}h" || need "运行时长不足（或去掉 --smoke 再验）"
import csv, sys, datetime
rows = list(csv.DictReader(open(sys.argv[1])))
t0 = datetime.datetime.strptime(rows[0]["ts"], "%Y-%m-%d %H:%M:%S")
t1 = datetime.datetime.strptime(rows[-1]["ts"], "%Y-%m-%d %H:%M:%S")
sys.exit(0 if (t1 - t0).total_seconds() >= float(sys.argv[2]) * 3600 * 0.98 else 1)
EOF

# 6) A/B 开销：复测中位数相对锚点下降 ≤3%
python3 tools/telemetry/ab_baseline.py --runs 5 --out /tmp/ab_m1.json
python3 - "$ROOT/monitor/ab_baseline.json" /tmp/ab_m1.json <<'EOF' && ok "常态开销 ≤3%" || need "开销 >3%——查采集器 CPU 占用"
import json, sys
a = json.load(open(sys.argv[1]))["median_loop_total"]
b = json.load(open(sys.argv[2]))["median_loop_total"]
sys.exit(0 if a and (a - b) / a <= 0.03 else 1)
EOF

[ "$FAIL" = 0 ] && echo "M1 验收 PASS" || echo "M1 验收 FAIL"
exit $FAIL
```

- [ ] **Step 2: root 部署 + 冒烟（30 分钟）**

```bash
bash scripts/sdc-excite-reproduce/acceptance_m1.sh 0.001 || true   # 部署前应全 FAIL（红）
# root 部署（经 su_run.py 通道；enable+start 三采集器）
timeout 300 python3 /tmp/sdc_inv_stage/su_run.py "bash /home/sdc/wangxu/sdcshield/scripts/sdc-excite-reproduce/install.sh /home/sdc/sdc-excite-reproduce && systemctl daemon-reload && systemctl enable --now sdc-collector@percore sdc-collector@pmu sdc-collector@ras && systemctl is-active sdc-collector@percore sdc-collector@pmu sdc-collector@ras" 2>&1 | tr -d '\r' | sed '1{/^密码：* *$/d}' | tail -5
sleep 1800   # 30 分钟冒烟
bash scripts/sdc-excite-reproduce/acceptance_m1.sh 0.5            # 0.5h 门（时长项按冒烟口径）
```
Expected: 部署输出 active×3；0.5h 门除"时长"项按冒烟口径外全 ✓（真实输出引用进报告；**PMU percent_covered<100 的组如实记录并对应调事件数**）。

- [ ] **Step 3: 24 小时验收（长跑，用户可顺带决策战役重启）**

```bash
sleep 86400 && bash scripts/sdc-excite-reproduce/acceptance_m1.sh 24
```
Expected: 6 项全 ✓（真实输出进报告——这是 M1 退出标准的最终判据）。

- [ ] **Step 4: 文档同步 + Commit**

`NEW_BOARD_ONBOARDING.md` 追加"采集器部署与验收"段（install.sh → enable --now 三采集器 → acceptance_m1.sh 门）。

```bash
git add scripts/sdc-excite-reproduce/acceptance_m1.sh scripts/sdc-excite-reproduce/NEW_BOARD_ONBOARDING.md
git commit -s -m "scripts: M1 验收门（丢样/新鲜度/质量字段/时长/开销）+ 部署文档（v5 M1 退出标准）

Signed-off-by: wangxu <wangxumarshall@qq.com>"
git push
```

- [ ] **Step 5: 集成 PR（API，合并留给用户）**

```bash
python3 - <<'EOF'
import json, os, urllib.request
TOKEN = os.environ['GITHUB_PERSONAL_ACCESS_TOKEN']
body = ("## M1 范围\n补丁单元 5/6/7（collector@percore/pmu/ras + systemd 模板 + 自监控）"
        "+ M0 终审移交修缮 + capabilities v2.1（scaling_driver/per-PMU 预算/A-B 锚点）+ acceptance_m1 验收门。\n\n"
        "## 验收证据\nTask 7 各步真实输出（部署 active×3、0.5h 冒烟、24h 门 6 项）见执行报告。\n\n"
        "## 偏差\nmonitor v3→M1b；120s ring/BPF canary→M2；81 机→M5——计划文末偏差表。\n\n"
        "🤖 Generated with [Claude Code](https://claude.com/claude-code)")
req = urllib.request.Request("https://api.github.com/repos/wangxumarshall/sdcshield/pulls",
    method="POST", data=json.dumps({"title": "feat: sdc-excite-reproduce M1 采集器族（percore/pmu/ras + systemd 模板 + M0 修缮 + 验收门）",
    "head": "feat/sdc-excite-reproduce-m1", "base": "main", "body": body}).encode(),
    headers={"Authorization": f"Bearer {TOKEN}", "Accept": "application/vnd.github+json"})
print(urllib.request.urlopen(req).status)
EOF
```

---

## 与 v5 M1 交付物的偏差（诚实记录）

| v5 M1 交付物 | 本计划 | 偏差理由 |
|---|---|---|
| monitor v3（补丁单元 4：BMC 单轮询/发现式列集/离散态 diff/UE 告警/看门狗） | **未列入 → M1b 独立计划** | 需对 17KB 在役 `sdc_monitor.sh` 做精确增量改造（安全联锁不动），本计划聚焦新建采集器族；单元 4 与采集器族无耦合，可独立交付 |
| 120 秒 ring + 事件触发 burst | **未列入 → M2**（随 sdc-eventd 事件管线） | ring/burst 的消费者（eventd/burst 调度）属 M2 规则闭环；M1 采集器为同步 append（丢样由 drop counter 度量），先立"无丢样"基线 |
| spurious canary BPF 版 | dmesg/journal 计数版（source=dmesg 标注） | v5 §6.5 降级路径；BPF tracepoint 挂载需 BTF/kallsyms 能力探测，随 M2 root-helper 一起做 |
| 81 机 rasdaemon 启用 | 不在 | 81 机入役属 M5（补丁单元 13） |
| 1 秒带内采集（sdc-monitor-fast 目标态） | percore 默认 60s、`PERCORE_PERIOD_S` 可调至 1s；提速以 A/B 开销达标为解锁条件 | v5 §6.3 as-built 台阶（60s→1s 需开销验证）；A/B 锚点+复测已内建（Task 2/7） |

## M0 终审移交清单对照（memory: sdc-excite-reproduce-m1-carryovers）

| 移交项 | 本计划落点 |
|---|---|
| per-PMU 计数器预算探测 | Task 2（probe_pmu_budget + PMU_CORE_COUNTERS） |
| A/B 开销基线 | Task 2 锚点 + Task 7 复测门 |
| CPUFREQ_DRIVER 改 scaling_driver | Task 2 |
| split()[3] 共享盲区 → 正则 | Task 1（sdc_topology + 测试独立正则口径） |
| interpolate 三处边界 | Task 1 |
| CNTVCT 进程内读取 | **不列**（M1 无紧映射消费者——锚点级 mapping_error 已够；M2 eventd 需要时做） |
| 适配器幂等 ID / _ts_to_ns / 死代码 | Task 1 |
| CPU_ONLINE_WRITABLE root 语义 | **不列**（随 M2 root-helper allowlist 设计一并做） |
| CLI 优雅报错 | Task 1 |
| profile schema | M3（v5 偏差表已记） |

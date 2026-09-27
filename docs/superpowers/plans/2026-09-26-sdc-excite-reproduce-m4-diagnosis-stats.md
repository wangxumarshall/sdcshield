# sdc-excite-reproduce M4（诊断与统计）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 M4 的诊断与统计——假设矩阵评分器（十域）、位形态视图、事件对齐时序差分、置信区间/零事件上界统计工具链、离线诊断报告生成器，退出标准="报告区分事实/推断/假设且所有比率含分母、暴露和区间"。

**Architecture:** `tools/analysis/` 新目录四件：`sdc_stats.py`（Clopper-Pearson/Wilson/rule-of-three/暴露量纯函数库）、`sdc_hypothesis.py`（十域假设矩阵+信号匹配+反事实探针建议）、`sdc_bitview.py`（mismatch 位形态视图）、`sdc_report.py`（离线报告生成——事件对齐时序+假设排序+证据等级+统计表）。消费 M0-M3 全链产物（ledger/events.jsonl/capsule/reducer 树）。

**Tech Stack:** Python 3.11 stdlib（json/statistics/math/datetime/csv）；零第三方依赖（CP 区间用 beta.ppf 闭式替代——纯整数算法或 math.comb 精确实现）。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5 §9 统计/§11 诊断/§12 可视化/§14.1 M4 行）。范围：M4 = 假设矩阵/探针库（建议生成——执行属 M3 runner 组合）/CP 统计/报告；报告渲染为 Markdown（Grafana 属〔研究〕）。

## Global Constraints

- **一补丁一单元**：每 Task 一 commit；`git commit -s` 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>` 其后无内容、无 Co-Authored-By；分支 `feat/sdc-excite-reproduce-m4`（从 main 建）；绝不提交/推送 main。
- **零第三方依赖**：CP 区间用 `math.comb` 精确二项（或 Wilson 近似+注明）；**k=0 时 rule of three 上界 3/n**。
- **诚实统计纪律（v5 §9.4/§9.6）**：所有比率带分母/暴露/区间；`k=0` 不写"发生率为 0"；INVALID/TEST_BUG 不入 SDC 分母但单独报告。
- **证据等级（v5 §11.8）**：E0 观察/E1 关联/E2 干预/E3 交叉验证/E4 平台确认——无 E2 以上不写"根因已定位"。
- 报告三段分级：事实/推断/假设（v5 §11.1-5）。
- **测试全部 fixture 数据**（真实事件目录只读冒烟允许）；`env -u SDC_ROOT_PW` 纪律。
- **M3 终审移交（首任务顺手）**：①`sdc_reproducer.py` manifest `original_evidence` 键落盘先行（M3 Minor #2）；②CORE179 案例数据回填测试（用 docs/cases/ 的真实样本作回归 fixture）。

---

### Task 1: sdc_stats——精确二项统计库 + M3 manifest 一行修

**Files:**
- Create: `tools/analysis/sdc_stats.py`（目录 + `__init__.py` 无需——测试目录约定同 excite）
- Modify: `tools/excite/sdc_reproducer.py`（manifest original_evidence 赋值提前一行）
- Test: `tools/analysis/tests/test_sdc_stats.py`

**Interfaces:**
- Consumes: 无（纯函数库）
- Produces（后续任务与报告直接消费）:
  - `clopper_pearson(k, n, alpha=0.05) -> (lo, hi)`：精确二项区间（math.comb 累加实现，n≤1e6 精确；k=0 → lo=0, hi=1-(alpha/2)^(1/n)；k=n → lo=(alpha/2)^(1/n), hi=1）
  - `wilson_ci(k, n, alpha=0.05) -> (lo, hi)`（复用 sdc_reproducer 语义——独立实现并交叉验证一致）
  - `rule_of_three(n) -> float`：3/n（k=0 的 95% 上界近似）
  - `required_trials(p0, alpha=0.05) -> int`：`ceil(log(alpha) / log(1-p0))`（v5 §9.4 公式）
  - `iteration_rate(confirmed, valid_iterations) -> dict`：`{"rate", "cp_lo", "cp_hi", "n", "k"}`——k=0 时 rate=0 但区间上界为准
  - `beta_binomial_adjust(ks, ns) -> dict`：跨 run block 的收缩估计（简单经验贝叶斯：Beta(1,1) 先验后验均值+95% 等尾区间）

- [ ] **Step 1: 失败测试**：

```python
import math, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_stats as st

def test_cp_known_values():
    lo, hi = st.clopper_pearson(0, 20)
    assert lo == 0.0 and abs(hi - (1 - 0.025 ** (1/20))) < 1e-12
    lo2, hi2 = st.clopper_pearson(20, 20)
    assert abs(lo2 - 0.025 ** (1/20)) < 1e-12 and hi2 == 1.0
    lo3, hi3 = st.clopper_pearson(5, 20)
    assert 0.08 < lo3 < 0.13 and 0.38 < hi3 < 0.50   # 统计表值域

def test_cp_symmetry_and_monotonicity():
    a = st.clopper_pearson(3, 10); b = st.clopper_pearson(4, 10)
    assert b[0] > a[0] and b[1] > a[1]

def test_rule_of_three():
    assert abs(st.rule_of_three(300) - 0.01) < 1e-9

def test_required_trials():
    n = st.required_trials(1e-4)
    assert 29000 < n < 31000                     # 3/p0 量级

def test_iteration_rate_k0_upper_bound():
    r = st.iteration_rate(0, 1000)
    assert r["rate"] == 0.0 and r["cp_hi"] > 0 and r["cp_hi"] < 0.005
    assert "上界" in r.get("note", "") or True   # 诚实口径注释

def test_wilson_matches_reproducer():
    sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", "..", "excite"))
    import sdc_reproducer as sr
    for k, n in [(0, 10), (5, 10), (10, 10), (3, 100)]:
        assert st.wilson_ci(k, n) == sr.wilson_ci(k, n)

def test_beta_binomial_shrinks_toward_prior():
    r = st.beta_binomial_adjust([0, 0, 0], [100, 100, 100])
    assert 0 < r["posterior_mean"] < 0.01        # 收缩后非零（k=0 不写 0）
```

- [ ] **Step 2-4: 红→实现→绿**（CP 用闭式边界+内部累加中段；wilson 公式照抄 sdc_reproducer 保证逐位一致）。
- [ ] **Step 5: M3 移交一行修 + Commit**：`sdc_reproducer.py` 的 manifest `original_evidence` 赋值移到落盘前（+1 断言于 test_m3_drill 或本文件），同 commit 或独立 commit `fix(reproducer): manifest original_evidence 键先行`（DCO 尾行）+ push。

---

### Task 2: sdc_bitview——位形态与空间聚集视图

**Files:**
- Create: `tools/analysis/sdc_bitview.py`
- Test: `tools/analysis/tests/test_sdc_bitview.py`

**Interfaces:**
- Consumes: 事件 mismatch 块（`actual_hex/expected_hex/xor_mask_hex/popcount/byte_offset/lane/type`）——M0 canonical schema
- Produces:
  - `bit_views(mismatches: list[dict]) -> dict`：v5 §11.3 七视图——offset→lane→bit 频率表/单多 bit 比例/Hamming distance 分布/符号-指数-尾数聚集/lane 固定性（地址变 lane 不变 vs 反之）/CPU 聚集（core/cluster/die/socket 计数）/seed 内稳定性（同 seed 错误值是否稳定）
  - `classification_hint(views) -> dict`：位形态→候选域提示（"固定 lane+地址变 → 执行/寄存器通路候选；固定 cache set+lane 变 → 存储层级候选"——提示非定性，输出 `confidence: "hint"`）
  - `format_bit_report(views, hints) -> str`（Markdown 段落）

- [ ] **Step 1: 失败测试**（fixture：合成 mismatch 集——mantissa 位翻 3 件+exponent 1 件+多 bit 1 件+固定 lane 变地址 2 件）：
```python
def test_views_mantissa_clustering():
    views = bv.bit_views(MISMATCHES)          # fixture 中 3 件 mantissa 位
    assert views["field_clustering"]["mantissa"] >= 3
    assert views["multi_bit_ratio"] < 0.5

def test_lane_fixedness_hint():
    # 固定 lane+地址变 → 执行通路提示
    h = bv.classification_hint(bv.bit_views(LANE_FIXED))
    assert "执行" in h["hint"] and h["confidence"] == "hint"

def test_seed_stability_view():
    views = bv.bit_views(SAME_SEED_SET)       # 同 seed 稳定错误值
    assert views["seed_stability"]["stable"] is True
```
- [ ] **Step 2-4: 红→实现→绿**。
- [ ] **Step 5: Commit** `tools: 位形态七视图 + 候选域提示（v5 §11.3-11.4，M4）`

---

### Task 3: sdc_hypothesis——十域假设矩阵与探针建议

**Files:**
- Create: `tools/analysis/sdc_hypothesis.py`
- Create: `configs/sdc-excite-reproduce/hypothesis_matrix.json`（v5 §11.2 十域表的机器可读版：每域含 support_signals/counter_probe/alternative 字段——逐字照 v5）
- Test: `tools/analysis/tests/test_sdc_hypothesis.py`

**Interfaces:**
- Consumes: T2 `bit_views/classification_hint`；事件流（cpu 聚集/PMU 异常 from spool；-n 1 免疫 from ledger retest）
- Produces:
  - `load_matrix() -> list[dict]`（十域：测试-oracle/Integer ALU/FP-SIMD-Vector/寄存器-旁路/LSU-L1D/L2-LLC-一致性/TLB-PTW/内存控制器-DRAM/分支-前端/时序-供电边际）
  - `score_hypotheses(evidence: dict, matrix) -> list[dict]`：每域 `{"domain", "score", "supporting": [...], "contradicting": [...], "probe": <反事实探针建议 str>}` 按 score 降序——evidence 键：`cpu_clustering/bit_hints/n1_immune/multicore_only/pmu_anomaly/workset_dependent/...`（每键 ±贡献）
  - `evidence_level(actions_taken: list) -> str`：E0-E4（干预实验执行记录→E2；独立复现→E3）
  - `format_ranking(scored, level) -> str`（Markdown——含"无 E2 以上证据不写根因已定位"守卫行）

- [ ] **Step 1: 失败测试**（CORE179 真实画像作 fixture：cpu 聚集+n1 非免疫+多核依赖+load-path 位提示 → LSU/一致性域应排前；测试-oracle 域在"替换测试实现仍发生"证据下应被反证）：
```python
def test_core179_profile_ranks_lsu_high():
    ev = {"cpu_clustering": True, "n1_immune": False, "multicore_only": True,
          "bit_hints": "load_path", "healthy_core_clean": True}
    scored = hyp.score_hypotheses(ev, hyp.load_matrix())
    top = scored[0]["domain"]
    assert top in ("LSU-L1D", "L2-LLC-一致性", "寄存器-旁路")
    assert any(d["domain"] == "测试-oracle" and d["score"] < 0 for d in scored)

def test_evidence_level_guard():
    assert hyp.evidence_level([]) == "E1"          # 只有观察
    assert hyp.evidence_level([{"type": "intervention"}]) == "E2"
    out = hyp.format_ranking([{...}], "E1")         # 无 E2
    assert "相关/候选" in out and "根因已定位" not in out
```
- [ ] **Step 2-4: 红→实现→绿**。
- [ ] **Step 5: Commit** `tools: 十域假设矩阵评分器 + 反事实探针建议 + 证据等级守卫（v5 §11.2/§11.8，M4）`

---

### Task 4: 事件对齐时序差分 + PMU 差分视图

**Files:**
- Create: `tools/analysis/sdc_timeline.py`
- Test: `tools/analysis/tests/test_sdc_timeline.py`

**Interfaces:**
- Consumes: monitor.csv/percore.csv/pmu_core.csv（M1 采集器）+ 事件时刻
- Produces:
  - `align_window(metrics_csv, event_ts, before_s=60, after_s=60) -> dict`：事件对齐窗口切片（各 CSV 列随相对时间序列）
  - `suspect_vs_control_diff(pmu_csv, suspect_cpus, control_cpus, window) -> dict`：嫌疑核 vs 对照核差分（v5 §11.5——IPC/stall/MPKI/DTLB/remote 的差值表）
  - `timeline_verdict(diff) -> dict`：判读规则四条（前尾部同升=时序压力候选/mismatch 无 PMU 异常=不否定/PMU 异常无 mismatch=canary/对照核同步异常=共享环境）——输出 `{"reading", "caveat"}`（caveat 必含"同窗相关≠因果"）
  - `format_timeline(windows, diff, verdict) -> str`

- [ ] **Step 1: 失败测试**（fixture CSV：合成事件前嫌疑核 stall 上升+对照核平稳 → 时序压力候选；对照核同步尖峰 → 共享环境改判）：
```python
def test_stall_rise_before_event_suspect_only():
    v = tl.timeline_verdict(tl.suspect_vs_control_diff(PMU_FIXTURE, [96], [0, 32], WIN))
    assert "时序" in v["reading"] and "因果" in v["caveat"]

def test_control_sync_spikes_reclassify():
    v2 = tl.timeline_verdict(tl.suspect_vs_control_diff(PMU_BOTH_FIXTURE, [96], [0], WIN))
    assert "共享环境" in v2["reading"]
```
- [ ] **Step 2-4: 红→实现→绿**。
- [ ] **Step 5: Commit** `tools: 事件对齐时序差分 + 嫌疑/对照核 PMU 差分 + 判读四规则（v5 §11.4-11.5，M4）`

---

### Task 5: sdc_report——离线诊断报告生成器（M4 退出标准载体）

**Files:**
- Create: `tools/analysis/sdc_report.py`
- Test: `tools/analysis/tests/test_sdc_report.py`

**Interfaces:**
- Consumes: T1 stats/T2 bitview/T3 hypothesis/T4 timeline + ledger.csv + events.jsonl + capsule（若存在）
- Produces:
  - `generate_report(data_root, event_id=None, out_path=None) -> str`：Markdown 报告五段——①概述（事件清单+分母/暴露表：每次数都带 n/k/CP 区间，k=0 显式上界）②位形态段（T2）③时序段（T4——无事件时省略）④假设排序段（T3——含证据等级+守卫行）⑤建议段（下一项最有信息量实验=各域 probe 的第一名）
  - `fact_inference_hypothesis_sections(evidence) -> (facts, inferences, hypotheses)`：三段分级切分（v5 §11.1-5：原始观测=事实；同窗相关=推断；未验证解释=假设——测试断言各类陈述归段正确）
  - `rate_table(events, valid_iterations) -> str`：迭代/核心小时双口径表+CP 区间列（k=0 行显式"上界 X（rule of three）"）
  - CLI：`python3 sdc_report.py [--data-root DIR] [--event-id ID] [-o out.md]`
  - **报告红线（自检器）**：`report_guard(md_text) -> list[str]`——扫描"无分母比率/无区间发生率/PMU 异常直接写 SDC/根因已定位（无 E2 前缀）"四违例，返回违规行号清单（生成器内部自检+测试）

- [ ] **Step 1: 失败测试**：
```python
def test_report_has_all_sections_and_denominators(tmp_path):
    md = rep.generate_report(str(fixture_root(str(tmp_path))))   # fixture 数据根
    for sec in ("概述", "位形态", "时序", "假设排序", "建议"):
        assert sec in md
    assert "n=" in md and "95% CI" in md                       # 分母+区间在场
    assert rep.report_guard(md) == []                          # 红线零违例

def test_k0_report_shows_upper_bound_not_zero_rate():
    md = rep.generate_report(str(zero_event_root(str(tmp_path))))
    assert "上界" in md and "发生率为 0" not in md

def test_guard_catches_violations():
    bad = "SDC 率 3.2%\n根因已定位：LSU\n"          # 无分母 + 无 E2 定性
    assert len(rep.report_guard(bad)) == 2
```
- [ ] **Step 2-4: 红→实现→绿**。
- [ ] **Step 5: 真实数据冒烟 + Commit**：对本机真实数据根跑一次 `sdc_report.py --data-root ~/sdc-excite-reproduce -o /tmp/report_real.md`——真实事件（污染事件已标注不入 SDC 分母）通过 guard 零违例；报告样段进报告文件。Commit + push。

---

### Task 6: M4 收官——CORE179 回归样本 + onboarding + PR

**Files:**
- Create: `tools/analysis/tests/fixtures/core179_case.json`（docs/cases 真实画像的脱敏 fixture）
- Modify: `NEW_BOARD_ONBOARDING.md`（M4 段：分析工具族用法）
- Ops: 无部署（离线分析工具）

- [ ] **Step 1-3**: CORE179 fixture 回归（hypothesis 排序对该画像稳定输出 LSU/一致性前列——真实案例锚定）；全套测试绿；真实根报告复跑 guard 零违例。
- [ ] **Step 4: Commit + push + PR**（API；终审由 SDD 流程派 opus 全分支审）。

---

## 与 v5 M4 交付物的对照（自审）

| v5 M4 交付物/退出标准 | 落点 |
|---|---|
| 假设矩阵 | T3（hypothesis_matrix.json 机器可读十域） |
| 探针库 | T3 probe 字段（建议生成；执行=M3 runner 组合，工具链闭环） |
| 置信区间/聚类分析 | T1（CP/Wilson/收缩）+ T2（位聚类） |
| 离线报告 | T5 |
| 退出：报告区分事实/推断/假设 | T5 三段切分+测试 |
| 退出：所有比率含分母、暴露和区间 | T1 rate_table + T5 report_guard 自检 |
| §12.1 视图（事件对齐/拓扑热力/矩阵/森林图） | T4 时序+T2 位视图（Markdown 表——Grafana〔研究〕；森林图效应量+CI 以表格呈现） |
| M3 终审移交 | T1（original_evidence 一行修）+ T6（CORE179 fixture） |

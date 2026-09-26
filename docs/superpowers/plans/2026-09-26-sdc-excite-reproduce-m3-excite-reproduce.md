# sdc-excite-reproduce M3（主动激发与复现）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 落地 v5 M3 的主动激发与复现——profile 矩阵（resolved-profile）、四轴策略执行器（governor 实验档/负载整形/上下线）、victim/aggressor 复现 runner、真实性门禁与复现胶囊（repro capsule）、概率 ddmin reducer，以"注入事件能自动到 R3 且单核不复现条件不被误删"为退出标准。

**Architecture:** `tools/excite/` 新目录三件：`sdc_profile.py`（profile YAML→resolved 快照 + 驱动器执行）、`sdc_reproducer.py`（victim/aggressor runner + 胶囊打包 + 真实性门禁）、`sdc_reducer.py`（概率 ddmin + 搜索树）。复用 M0/M1/M2 全部资产：`sdc_event` 契约、`sdc_root_helper`（hotplug/governor 动作）、`retest_guarded`/`cleanup_orphans`（driver 加固）、`capabilities.env`（四轴能力）。driver 侧消费 M2 预留的 `enqueue_reproduction` 动作行。

**Tech Stack:** Python 3.11 stdlib（subprocess/json/shutil/hashlib/statistics）、bash（驱动集成段）、YAML 仅 profile 输入侧手写极简解析器（**零第三方依赖**——profile 用受限 YAML 子集：二层映射+列表，~40 行解析器或改用 JSON profile——**裁定：profile 文件用 JSON**（`.json`），v5 §7.5 的 YAML 示例语义照搬、语法换 JSON，避免依赖）。

**Spec:** `docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md`（v5 §7 激发/§10 复现/§14.1 M3 行 + §14.2 单元 15-16）。范围：M3 = 概率 reducer + capsule + 四轴执行器 + victim/aggressor runner；架构态注入与 ML 属〔研究〕不含；CORE179 历史样本作回归样本。

## Global Constraints

- **一补丁一单元**：每 Task 一 commit；`git commit -s` 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>` 其后无任何内容、无 Co-Authored-By；分支 `feat/sdc-excite-reproduce-m3`（从 main 建）；绝不提交/推送 main。
- **战役+9 服务运行中**：M3 的激发/复现实验全部**受界**（v5 §7.4 与战役兼容约束）：单次 ≤5min、内存护栏复用 `retest_guarded`、**不做全核新负载**（victim/aggressor 实验用 `--cpuset` 限界核集，aggressor 总并发 ≤16 线程）、hotplug 经 root-helper run 边界。**测试全部 mock/fixture，绝不真跑激发/真 hotplug/真 governor 切换**（能力探测类只读探针除外）。
- **零第三方依赖**：profile 文件用 JSON（v5 §7.5 YAML 示例的语义等价物，见工具头注释说明）。
- **CORE179 硬规则（v5 §10.3/§10.4）**：禁确定性 ddmin——reducer 全概率口径（k/n+区间+ACCEPT/REJECT/INCONCLUSIVE）；单核模式不复现时输出"最小 victim+最小必要 aggressor 场"不强迫单线程样例。
- **capsule 诚实（v5 §10.2）**：`run.sh` 运行前校验 manifest（内核/governor/online 集/拓扑）；无法精确复原的 BMC/热状态以允许区间表示。
- 每实验 manifest 预定义停止规则（v5 §9.8）；`env -u SDC_ROOT_PW` 纪律。

---

### Task 1: sdc_profile——profile 定义、resolve 与执行器

**Files:**
- Create: `tools/excite/sdc_profile.py`（目录 `tools/excite/` 新建 + `.gitignore` 空除）
- Create: `configs/sdc-excite-reproduce/profiles/victim_candidate_v1.json`
- Test: `tools/excite/tests/test_sdc_profile.py`

**Interfaces:**
- Consumes: `capabilities.env` 键（NPROC/GOVERNOR_WRITABLE/CPU_ONLINE_WRITABLE/HAS_CPUFREQ 等）；`sdc-excite-reproduce.sh` 的 bin/flags 约定
- Produces:
  - `load_profile(path) -> dict`（JSON 校验：必填 `profile_id/victim/tests/aggressors/environment/monitoring/safety_policy/limits`——v5 §7.5 字段逐项）
  - `resolve(profile, capabilities, topology_snapshot) -> dict`：resolved-profile（CPU 列表实际化——`all_except_victim` 按 topology 展开、PMU 实例/sensor 名/二进制哈希/所有默认值冻结；**禁止 `p0` 假定编号**——用 sysfs cpulist）
  - `ProfileRunner(resolved, data_root).run_once() -> dict(result)`：执行一轮 victim+aggressor（subprocess `sdcshield -e ... --cpuset=...`，victim 先起、aggressors 并发起、`retest_guarded` 包裹、结束收 `cleanup_orphans`）——**测试全 mock**（`_launch`/`_await` 注入点）
  - CLI：`python3 sdc_profile.py run <profile.json> [--dry-run]`（--dry-run 只产 resolved 不执行）

- [ ] **Step 1: 失败测试**：

```python
import json, os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_profile as sp

CAPS = {"NPROC": "128", "GOVERNOR_WRITABLE": "no", "CPU_ONLINE_WRITABLE": "yes",
        "HAS_CPUFREQ": "yes", "PMU_CORE_COUNTERS": "12"}
TOPO = {"possible": list(range(128)), "online": list(range(128)),
        "nodes": {"1": [32, 33], "3": [96, 97]}}   # 精简拓扑 fixture

PROFILE = {
    "profile_id": "victim_test_v1",
    "victim": {"cpus": [96], "tests": ["openblas_dgemm"], "seed_policy": "fixed",
               "iterations_per_seed": 100},
    "aggressors": {"topology": "all_except_victim", "families": ["cache", "integer"],
                   "duty_cycle": 1.0},
    "environment": {"governor": "performance", "numa_policy": "recorded_default"},
    "monitoring": {"pmu_groups": ["core_base"], "steady_period_ms": 1000},
    "safety_policy": "lab_default_v1", "limits": {"duration_s": 300, "max_failures": 3}}

def test_load_profile_validates_required():
    p = sp.load_profile_dict(PROFILE)
    assert p["profile_id"] == "victim_test_v1"
    import pytest
    bad = dict(PROFILE); del bad["victim"]
    with pytest.raises(ValueError, match="victim"):
        sp.load_profile_dict(bad)

def test_resolve_expands_all_except_victim():
    r = sp.resolve(PROFILE, CAPS, TOPO)
    assert 96 not in r["aggressor_cpus"] and 97 in r["aggressor_cpus"]
    assert r["binary_hash"] and r["capabilities_snapshot"] == CAPS
    assert r["topology_snapshot"]["online"] == TOPO["online"]

def test_resolve_rejects_capability_gap():
    bad_caps = dict(CAPS, CPU_ONLINE_WRITABLE="no")
    prof = dict(PROFILE); prof["victim"] = dict(PROFILE["victim"], cpus=[96])
    prof["axes"] = {"cpu_hotplug": {"enabled": True}}
    import pytest
    with pytest.raises(ValueError, match="CPU_ONLINE"):
        sp.resolve(prof, bad_caps, TOPO)

def test_runner_orchestration_mock(tmp_path):
    r = sp.resolve(PROFILE, CAPS, TOPO)
    launched = []
    class FakeRunner(sp.ProfileRunner):
        def _launch(self, role, cpus, test, extra):
            launched.append((role, tuple(cpus), test))
            return f"pid-{role}"
        def _await(self, pid, timeout): return 0
    res = FakeRunner(r, str(tmp_path)).run_once()
    roles = [l[0] for l in launched]
    assert roles[0] == "victim" and "aggressor" in roles
    assert res["rc"] == 0 and res["launched"] == len(launched)
```

- [ ] **Step 2-4: 红→实现→绿**。要点：`aggressor_cpus = [c for c in online if c not in victim_cpus]`（victim ⊄ online 时报错）；`families`→测试名映射查 `--list-tests` 缓存（`_tests_for_family()` 注入点，测试 mock）；四轴 `axes` 键（可选）：`cpu_hotplug/governor_experiment/load_shaping`——resolve 时对照 capabilities 拒绝缺口（能力探测诚实）；`binary_hash`=sha256(builddir/sdcshield)。
- [ ] **Step 5: Commit** `tools: sdc_profile——profile 定义/resolve 冻结/受界执行器（v5 §7.5，M3）`

---

### Task 2: 四轴策略执行器（governor 实验档/负载整形/hotplug 编排）

**Files:**
- Create: `tools/excite/sdc_axes.py`
- Test: `tools/excite/tests/test_sdc_axes.py`

**Interfaces:**
- Consumes: `sdc_root_helper` 的请求文件协议（`cmd/burst-<id>.request`/`cpu_hotplug.request`/`restore.request`——T4 交付）；capabilities
- Produces:
  - ` AxesExecutor(data_root, capabilities)`：`.execute(axis_spec) -> list[result_dict]`，axis_spec 三类（对齐 v5 §7.4）：
    ①`{"axis": "governor_experiment", "from": "performance", "to": "powersave", "hold_s": 60}`——写 `cmd/governor.request`？**不**——root-helper 的 allowlist 无 governor 动作（M2 范围裁定 governor 走 monitor 现有代理只接受 performance）。**M3 裁定：governor 实验档经 cmd/governor.request 仍只允许 performance（安全不变式），实验性 governor 切换延后至用户显式授权扩展 allowlist**——本轴实现为：写请求 + 期望拒绝 + 审计记录（"能力缺口如实暴露"），文档标注 v5 §7.4.1 的实验档需独立授权；
    ②`{"axis": "load_shaping", "pattern": "burst", "duty": 0.5, "period_s": 20, "duration_s": 300}`——生成负载阶跃脚本计划（`_make_loadplan()` 纯函数产出时间段表 [{"t_on_s":..,"t_off_s":..}]，执行经 ProfileRunner 的 aggressor 启停——测试只验计划纯函数与编排调用序列）；
    ③`{"axis": "cpu_hotplug", "op": "offline", "cpus": [96, 97], "then": "online", "hold_s": 30}`——写 `cmd/cpu_hotplug.request`（helper schema）+ 等 `.done`/`.rejected`（`_wait_request()` 注入点）；**测试全 mock 文件协议**。
  - `.restore_all() -> list[result]`：`restore.request` + 读回断言
- 测试：计划纯函数（burst 占空比表/边界）+ 文件协议 mock（写请求→模拟 done/rejected/过期）+ governor 拒绝语义 + hotplug 请求 schema 与 helper validate 兼容（import sdc_root_helper.validate 交叉验证）

- [ ] **Step 1-4: 红→实现→绿**（governor 轴的"期望拒绝"是显式行为不是缺陷——测试断言拒绝+审计在案）。
- [ ] **Step 5: Commit** `tools: 四轴策略执行器——负载整形计划/hotplug 编排/governor 实验档能力缺口暴露（v5 §7.4，M3）`

---

### Task 3: victim/aggressor 复现 runner + 真实性门禁 + 复现胶囊

**Files:**
- Create: `tools/excite/sdc_reproducer.py`
- Test: `tools/excite/tests/test_sdc_reproducer.py`

**Interfaces:**
- Consumes: T1 ProfileRunner；v5 §10.1 七步门禁清单；M2 事件（RED 触发源）
- Produces:
  - `authenticity_gate(event_dir, capsule_ctx) -> (ok, [reasons])`：v5 §10.1 七项（原始证据保存/完整迭代内/offset-lane-mask 重算/健康机对照【M3 简化为健康核对照：同二进制同 seed 在 cpuset 对照核跑一遍 rc+result 比对】/竞态审计【事件目录竞态三探针检查单引用】/实际 CPU 确认【cpuset 请求 vs detecting-cpu】/复测与 sham）
  - `build_capsule(event_dir, resolved_profile, out_dir) -> dict(manifest)`：v5 §10.2 目录结构（manifest/resolved-profile/topology/platform/binaries/inputs/golden/event/original/windows/scripts/reduction/diagnosis）；`scripts/run.sh` 生成（manifest 预检：内核 release/governor/online 集/拓扑一致才跑，BMC 热状态允许区间）；SHA256SUMS
  - `Reproducer(data_root, event_dir)`：`.from_event() -> resolved_profile`（事件 test/seed → victim 单测 profile 构造——复现条件建模 R 的最小起点）；`.run_cycle(profile, n_trials) -> {k, n, ci}`（复测概率估计：n 次受界运行数失败次数，Wilson 95% 区间——stdlib 实现 `wilson_ci(k, n)`）
  - `wilson_ci(k, n) -> (lo, hi)`（纯函数）

- [ ] **Step 1: 失败测试**（fixture：合成事件目录含 stdout_summary/yaml_extract/context；capsule 目录逐项断言；gate 七项各有正反用例——健康核对照用 mock `_control_run()`）：

```python
import os, sys, math
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_reproducer as sr

def test_wilson_ci_bounds():
    lo, hi = sr.wilson_ci(0, 20)
    assert lo == 0.0 and hi < 0.20        # rule of three 量级
    lo2, hi2 = sr.wilson_ci(20, 20)
    assert lo2 > 0.84                     # 全成功下界

def test_authenticity_gate_catches_stage_boundary(tmp_path):
    ev = tmp_path / "ev"; ev.mkdir()
    (ev / "stdout_summary.out").write_text(
        "- test: zstd19\n  result: fail\n")
    (ev / "context.txt").write_text("rc: 143\n")     # 阶段边界 SIGTERM
    ok, reasons = sr.authenticity_gate(str(ev), {})
    assert not ok and any("完整迭代" in r for r in reasons)
```

- [ ] **Step 2-4: 红→实现→绿**。
- [ ] **Step 5: Commit** `tools: 复现 runner + 真实性门禁七项 + 复现胶囊打包（v5 §10.1-10.2，M3）`

---

### Task 4: 概率 ddmin reducer + 搜索树

**Files:**
- Create: `tools/excite/sdc_reducer.py`
- Test: `tools/excite/tests/test_sdc_reducer.py`

**Interfaces:**
- Consumes: T3 `wilson_ci`/Reproducer.run_cycle（注入点 `_run_trial(config) -> bool`——测试 mock 决定论）
- Produces:
  - `probabilistic_ddmin(factors, run_trial, budget) -> {"min_set", "tree", "trials", "verdicts"}`：v5 §10.3——因素集（victim_cpus/aggressor_groups/tests/seeds/axes）按序删减，每候选配置 `min_trials`（默认 20）到 `max_trials`（200）序贯试验；ACCEPT（复现率下界 ≥ 阈值或非劣）/ REJECT（足够证据低于）/ INCONCLUSIVE（预算尽）；**CORE179 硬规则**：`aggressor` 因素组删空时若复现率崩塌 → 输出"最小 victim+必要 aggressor 场"（不强迫单线程）
  - `trial_record(config, k, n, ci, verdict) -> dict`（搜索树节点——jsonl 落盘）
  - 因素模型：`Factors` 类（`{"victim_cpus": [...], "aggressor_cpus": [...], "aggressor_families": [...], "tests": [...], "seeds": [...]}`，`.reduce(factor, subset) -> Factors`）
- 测试：合成 `run_trial`（概率 p(config) 由测试指定——如复现需 victim={96} ∧ aggressor 含 cache 族 ∧ seed s3）：断言收敛到最小集、INCONCLUSIVE 路径、预算上限、搜索树完整、单核删空时 aggressor 恢复语义

- [ ] **Step 1: 失败测试**（核心用例——"依赖 aggressor 的复现"）：

```python
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
import sdc_reducer as red

def true_repro(config):     # 复现条件：victim=96 ∧ aggressor 含 cache ∧ seed in {s3}
    return (96 in config["victim_cpus"] and "cache" in config["aggressor_families"]
            and "s3" in config["seeds"])

def test_ddmin_finds_min_set_with_aggressor_dependency():
    factors = red.Factors(victim_cpus=[96, 97], aggressor_cpus=list(range(120)),
                          aggressor_families=["cache", "integer", "stream"],
                          tests=["openblas_dgemm"], seeds=["s1", "s3"])
    out = red.probabilistic_ddmin(
        factors, lambda cfg: true_repro(cfg), budget={"min_trials": 5, "max_trials": 30})
    assert out["min_set"]["victim_cpus"] == [96]
    assert out["min_set"]["aggressor_families"] == ["cache"]
    assert out["min_set"]["seeds"] == ["s3"]
    assert all("cache" not in t["config"]["aggressor_families"] or t["verdict"] != "REJECT"
               for t in out["trials"] if t["config"]["aggressor_families"] == [])  # 删空组恢复语义

def test_ddmin_budget_exhaustion_inconclusive():
    calls = {"n": 0}
    def flaky(cfg):
        calls["n"] += 1
        return calls["n"] % 2 == 0      # 50% 恒振荡永不收敛
    factors = red.Factors(victim_cpus=[1], aggressor_cpus=[2], aggressor_families=["x"],
                          tests=["t"], seeds=["s"])
    out = red.probabilistic_ddmin(factors, flaky, budget={"min_trials": 3, "max_trials": 6},
                                  max_total_trials=12)
    assert out["verdicts"][-1] in ("INCONCLUSIVE",) or out["trials"][-1]["verdict"] == "INCONCLUSIVE"
```

- [ ] **Step 2-4: 红→实现→绿**（序贯试验：先 min_trials 定初判，区间含阈值则加样至分辨或 max_trials）。
- [ ] **Step 5: Commit** `tools: 概率 ddmin reducer + 搜索树（CORE179 硬规则：禁确定性删减，v5 §10.3，M3）`

---

### Task 5: 驱动集成——enqueue_reproduction 消费 + M2 动作接线

**Files:**
- Modify: `scripts/sdc-excite-reproduce/sdc-excite-reproduce.sh`（handle_failure 尾部 + 分类协议后钩子）
- Test: `tools/excite/tests/test_driver_repro_integration.py`

**Interfaces:**
- Consumes: T3 Reproducer；M2 controller 的 `enqueue_reproduction` 动作（账本行）与 `cmd/verify.request`（T5 M2 移交的消费者）
- Produces:
  - driver 的 `handle_failure` 在台账行后：写 `spool/repro_queue/<event_id>.json`（`{"event_dir", "test", "seed", "queued_at"}`）——M2 `enqueue_reproduction` 动作行的落点（controller 账本已有动作名，本任务给队列文件消费端）
  - `sdc_reproducer.py --from-queue [--once]`：扫 `spool/repro_queue/` → 逐事件 from_event→gate→（gate 过）`build_capsule` + 概率复测 3 次定初判（受界）→ 移到 `spool/repro_done/<event_id>.json`（结果行）——**真实复现循环受 budget 限界（单事件 ≤5 min）**
  - driver 的 `cmd/verify.request` 消费：monitor v3 联锁 PAUSE 时 controller 写的 verify.request（M2 T5）→ 本任务给最小消费者（root-helper 扩 or driver 轮询——**裁定：driver 轮询消费**：见 verify.request → 记台账一行"interlock verify 触发" + touch snapshot.request）——闭环 M2 移交项
- 测试：mock 驱动函数 bash 子进程级（fixture 事件目录 + 队列文件 → --once → capsule 生成 + done 移动 + 台账行）；verify.request 消费用隔离 cmd/ 目录

- [ ] **Step 1-4: 红→实现→绿**。
- [ ] **Step 5: Commit** `scripts+tools: 复现队列消费 + verify.request 闭环（M2 enqueue_reproduction 落地，M3）`

---

### Task 6: M3 退出标准演练 + 部署收尾

**Files:**
- Create: `scripts/sdc-excite-reproduce/m3_drill.sh`
- Modify: `NEW_BOARD_ONBOARDING.md`（M3 段：profile/excite 工具/repro 队列）
- Ops: 无新守护（reproducer 按需 CLI / cron 可选——**裁定：不部署常驻**，事件驱动由驱动钩子+手动/巡检触发）

**Interfaces:**
- Produces: m3_drill.sh（隔离数据根全链：注入合成 mismatch 事件目录 → repro_queue → reproducer --once（gate 过）→ capsule 目录断言（manifest/run.sh/SHA256SUMS）→ 概率复测 mock 3 次 → done；CORE179 场景断言：aggressor 删空复现率崩塌时输出"最小 victim+必要 aggressor"而非空 aggressor——合成 run_trial mock）——**退出标准映射**：①"注入事件自动到 R3"=capsule 完整+可重放（run.sh --dry-run 校验过）②"单核不复现条件不被误删"=CORE179 断言
- 部署验证：真实根无新服务；驱动钩子代码在 repo（下次驱动重启生效——记录于 onboarding）；全套测试绿；`git diff main --stat -- framework/ tests/ meson.build` 空

- [ ] **Step 1-3: 演练脚本+断言+文档**；**Step 4: Commit + push + PR（API）+ 终审准备**

---

## 与 v5 M3 交付物的对照（自审）

| v5 M3 交付物/退出标准 | 落点 |
|---|---|
| profile 矩阵（resolved-profile） | T1（JSON 裁定） |
| 四轴策略执行器（governor 实验/负载整形/上下线） | T2（governor=能力缺口诚实暴露，v5 §7.4.1 实验档需独立授权） |
| victim/aggressor runner | T1 ProfileRunner + T3 |
| repro capsule | T3 |
| 真实性门禁 | T3（七项，健康机对照简化为健康核对照——机器数=1 的现实） |
| 概率 reducer | T4 |
| 退出：注入事件自动到 R3 | T6（capsule 完整+可重放） |
| 退出：单核不复现不被误删 | T4 CORE179 硬规则 + T6 断言 |
| M2 移交：enqueue_reproduction/verify.request | T5 |

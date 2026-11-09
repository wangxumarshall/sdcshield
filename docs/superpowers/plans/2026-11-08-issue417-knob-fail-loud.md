# Issue #417: knob-from-worker fail-loud 三缺口修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development
> (recommended) or superpowers:executing-plans to implement this plan task-by-task.
> Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把「test knob 只能在 test_init() 读」这一契约从「debug 构建才检查」升级为
所有构建等级都强制、有负路径 selftest 锁定、且 sizing 缓存层不再静默回退。

**Architecture:** 三处框架层加固 + 两处现行违例迁移：
1. `framework/logging.cpp` 的 `thread_num != -1` 检查移出 `#ifndef NDEBUG`，
   并前置到 `no_output` 短路之前（release 的 `b_ndebug=if-release` 不再编译掉它）；
2. `framework/selftest.cpp` 新增负路径 selftest（run 里读 knob，期望 SIGABRT），
   接入 `bats/sanity-check/20-selftests.bats` 与 `scripts/gha/verify-params.py`；
3. `framework/sdc_stress_config.h::sdc_knob_int_clamped()` worker 线程 cache miss
   从「静默返回 def」改为 log_error + abort；
4. 现行违例：`low_load_cache_pressure{,_neon}` 在 run() 直连 getter → 迁入 init 缓存
   （不迁的话缺口 1 落地即把这两个 PROD 用例打成 crash 回归）。

**Tech Stack:** C++20 framework、meson（release + debugoptimized）、bats、Python stdlib。

**Spec:** GitHub issue #417。现状核查与考古证据见
`docs/superpowers/plans/2026-11-01-issue183-random-access-sweep-undisable.md`。

## Global Constraints

- 永不 push main；`git commit -s`，尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`。
- x86-64 非回归：检查本身双架构等价；正常路径（init 内读取）只多一次 TLS 比较。
- 每 commit 一个逻辑单元；release 与 debugoptimized 双档零新警告。
- 本机验证：`-e zstd19 -t 2000 -n 1` → `exit: pass`；负路径 selftest 真实
  abort 输出；低载/正路径用例回归。

## 现状核查（2026-11-08 实测）

- issue 称「64 处 getter 调用点全部在 *_init」**不完全准确**：全仓扫描
  （逐函数花括号配对）发现 `low_load_cache_pressure.cpp:90` 与
  `low_load_cache_pressure_neon.cpp:73` 在 `*_run` 里直接调用
  `get_testspecific_knob_value_uint()`——release 下静默，debug 下 abort。
  本计划把它们一并迁入 init。
- 其余 run 路径的 `sdc_knob_int_clamped()` 调用（gather ×10、openssl sha/sha3）
  均已在 init 顶部显式预热缓存，cache-miss abort 不会误伤。

## Tasks

- [ ] **Task 1: 检查无条件化 + 现行违例迁移**
  - `framework/logging.cpp`：thread 检查移出 NDEBUG、置于 no_output 短路之前。
  - `low_load_cache_pressure{,_neon}.cpp`：`duty_us` 在 init 读取+clamp 并存入
    `LLP{,N}Data`，run 只读缓存。
  - 验证：release 构建；`-e low_load_cache_pressure*` pass；负路径 selftest 未加前先用
    临时构造无法验证——以 Task 2 的 selftest 在 release 下真实 abort 作为实证。

- [ ] **Task 2: 负路径 selftest + 接入 sanity/verify-params**
  - `framework/selftest.cpp`：`selftest_knob_from_run`（group_negative，run 读 knob）；
    若检查失效则 log_warning + 返回 EXIT_SUCCESS（使 rc==0，负向断言必红）。
  - `bats/sanity-check/20-selftests.bats`：加入 TAP crashtests 列表 + 独立
    `selftest_crash_common` 用例（SIGABRT / Aborted）。
  - `scripts/gha/verify-params.py`：加入 `NEGATIVE_SELFTESTS`。

- [ ] **Task 3: sdc_knob_int_clamped worker cache-miss fail-loud**
  - `framework/sdc_stress_config.h`：miss 时 `log_error` + `abort()`；更新函数头注释。
  - 验证：gather/openssl sha 用例 pass（预热路径未受影响）。

- [ ] **Task 4: 双档构建 + 回归 + 文档同步**
  - release 与 debugoptimized（`builddir-asan` 为 debugoptimized+ASAN，可作
    NDEBUG 未定义档的编译验证）零新警告。
  - `scripts/gha/verify-params.py` 的负路径集合逐项跑（或等价命令）。
  - 更新 `docs/architecture.md`：新增 test-knob 契约小节。

## Verification Matrix

| 行为 | 命令 | 期望 |
|---|---|---|
| release 检查生效 | 负路径 selftest（release 二进制） | SIGABRT / rc=1 / `reason: Aborted` |
| 正路径 | `--selftests -e selftest_logs_options` | pass |
| 现行违例迁移 | `-e low_load_cache_pressure -t 3000 -n 1` | pass |
| sizing 预热 | `-e gatherscatterpd -t 2000 -n 1` | pass |
| 回归 | `-e zstd19 -t 2000 -n 1` | `exit: pass` |
| 无警告 | `./scripts/build.sh`（release）；`ninja -C builddir-asan` | 零新警告 |

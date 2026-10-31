# Issue #365: arm64_sdc_sve per-thread 全尺寸缓冲 sizing 修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `arm64_sdc_sve` 的自适应 sizing 按真实消费者份数(`1 + thread_count()`)上报,消除"每线程一份全尺寸副本"的 n 倍 OOM 放大。

**Architecture:** 只改 `tests/cpu/sve/mem/arm64_sdc_sve.cpp` 的 `sdc_sve_buffer_bytes()`:自适应分支的 `sdc_target_workset_bytes(600, 2, nullptr)` 改为 `sdc_target_workset_bytes(600, 1 + thread_count(), nullptr, "arm64_sdc_sve", "buffer_bytes")`,并同步注释;docs 在 `SDC_STRESS_CONFIG_CN.md` 5.2 节补 consumers 用法一句。校验链(init golden 三签名 → run 每线程全量 memcpy + 全量三签名比较)一字不改。

**Tech Stack:** C++20 / sdcshield 框架(sdc_stress_config.h sizing 体系)/ meson + ninja。

**Spec:** GitHub issue #365(+ issue #360 审计项 B1 的实测证据)。

## 选型论证(修法 A:`consumers = 1 + thread_count()`;否决修法 B:run 副本切分)

1. **SDC 检测语义不变(决定性)**:golden 是 init 对共享 `data->test_buffer` 全量算的三个签名(arm64_sdc_sve.cpp:119-120);run 每线程 `memcpy` 全量到自己的副本(:179)后对副本全量算三签名与 golden 逐字节比(:180-193)。修法 A 下这条链一字不改——每线程仍校验一份完整缓冲。修法 B(每线程只拷 `buffer_bytes/n`)要么所有线程校验同一段(覆盖面缩 1/n),要么按 thread-id 分段——而段级 CRC 无法与全量 golden 比较,必须重构 init 为 per-segment golden 表,patch 面和语义面都远超"修 sizing"。
2. **consumers 语义如实上报**:框架定义 consumers = "同时跑的大内存用例数"(framework/sdc_stress_config.h:1276-1278),且函数内部自动乘并发 slice 数(:1314-1318)——传 per-slice 真实份数 `1 + thread_count()`(`thread_count()` 在 fork 后子进程里返回本 slice 线程数,framework/sandstone.h:657-659)正是该参数的设计用法。mesh_upi_sve_write_only 的 `/threads` 变体是"无共享缓冲、无 golden 链"结构下的等价物,不适用本测试。
3. **压力意图保持**:body 关键路径是表查 CRC(数百 MB/s,arm64_sdc_sve.cpp:156-166 注释实测),不是带宽;每份缩小到 `预算/(1+n)` 而份数 ×n → 全核每 body 总扫描量 ≈ 恒定 36.6 GiB(600‰ 预算),总量预算语义不变;总 memcpy 触页量 `n/(n+1) × 预算` 随 n 微增。
4. **单线程行为零变化**:`1 + 1 = 2` = 旧值,`-n 1` 的每份尺寸与日志与修复前完全一致。
5. **init 峰值同样覆盖**:init 期间共享 1 + 自检 probe 1 = 2 份(probe 在 init 内 free,:123-128)≤ run 期间 `1 + n` 份(n≥1)。

## Global Constraints

- worktree 分支 `worktree-agent-a584b20c16c605f31`,绝不 push main;`git commit -s`,最后一行且仅此一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,禁止 Co-Authored-By。
- 构建:`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `SDC_BUILD_JOBS=20 ./scripts/build.sh`(4 并发 agent 禁满核)零新警告。
- **不许把本机推进 swap 抖动**:修复前绝不跑自适应 `-n 8`(issue 实测 146 GiB 提交需求);修复前对照只用 `-O buffer_bytes=134217728` 限流。
- 本机 SVE-256,VL 门 `arm64_require_sve_vl(test, 128, 1024)` 在范围内 → 本机原生验证,无需 gem5-fi。
- 测试为 ARM64 专属(整文件 `#if defined(__aarch64__)`),x86 非回归 = 目检改动均在 `#if` 块内。

---

### Task 1: 基线构建 + 修复前限流对照

**Files:** 无代码改动;产出基线数据。

- [ ] **Step 1:** `cd` worktree;`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release && SDC_BUILD_JOBS=20 ./scripts/build.sh` — 零警告,产出修复前二进制。
- [ ] **Step 2:** 记录本机物理内存口径:`grep MemTotal /proc/meminfo`(预算 = MemTotal × 600‰;issue 口径 61 GiB)。
- [ ] **Step 3:** 修复前限流对照:`/usr/bin/time -v ./builddir/sdcshield --quality=-1 -e arm64_sdc_sve -O arm64_sdc_sve.buffer_bytes=134217728 -n 8 -t 30 2>&1 | grep -E "压力映射|exit|Maximum resident"` — 预期:knob bypass sizing(无自适应日志),maxrss ≈ 9 × 128 MiB ≈ 1.2 GiB,exit: pass。此为"显式配置路径"的修复前基线。

### Task 2: sizing patch

**Files:**
- Modify: `tests/cpu/sve/mem/arm64_sdc_sve.cpp:31-47`(sizing 函数 + 注释)
- Modify: `tests/cpu/sve/mem/arm64_sdc_sve.cpp:156-177`(TEST_LOOP 注释中"separate sizing concern, tracked in its own issue"句)

**Interfaces:**
- Consumes: `int thread_count()`(framework/sandstone.h:653,子进程内返回本 slice 线程数);`uint64_t sdc_target_workset_bytes(int permille, int consumers, const char *env_override, const char *test_id = nullptr, const char *knob = nullptr)`(framework/sdc_stress_config.h:1280)。
- Produces: `static size_t sdc_sve_buffer_bytes(struct test *)` 签名不变;行为变化仅在自适应分支。

- [ ] **Step 1:** `sdc_sve_buffer_bytes()` 自适应分支改为:

```c
static size_t sdc_sve_buffer_bytes(struct test *test)
{
    int64_t v = sdc_knob_int_clamped(test, "buffer_bytes", 0, 64 << 10,
                                     1ll << 40);
    if (v == 0)
        sdc_env_knob_int_clamped("arm64_sdc_sve", "buffer_bytes", &v,
                                 64 << 10, 1ll << 40);
    if (v > 0)
        return (size_t)v;
    /* consumers = 1 份 init 共享检测缓冲 + 每线程 1 份 run 副本
     * (per-slice 口径; sdc_target_workset_bytes 内部再乘并发 slice
     * 数)。修复 #365: 旧值 2 只在单线程成立 — n 线程实际提交需求
     * = (1+n) × 份大小, 128 线程外推 2.3 TiB 必 OOM。 */
    int threads = thread_count();
    if (threads < 1)
        threads = 1;
    return (size_t)sdc_target_workset_bytes(600, 1 + threads, nullptr,
                                            "arm64_sdc_sve", "buffer_bytes");
}
```

同步更新函数头注释(31-36 行)的优先级链第 3 条描述。

- [ ] **Step 2:** TEST_LOOP 注释 156-177 行:更新 "(The per-thread full-size buffer copy is a separate sizing concern, tracked in its own issue.)" 为已修引用(#365,consumers=1+threads),及 162-163 行 "600 permille of RAM across 2 consumers" 措辞。
- [ ] **Step 3:** `SDC_BUILD_JOBS=20 ./scripts/build.sh` 零新警告。

### Task 3: 端到端验证(全部 100% real,引用实际输出)

- [ ] **Step 1:** 限流回归(knob bypass 路径):`/usr/bin/time -v ./builddir/sdcshield --quality=-1 -e arm64_sdc_sve -O arm64_sdc_sve.buffer_bytes=134217728 -n 8 -t 30` — 与 Task 1 Step 3 基线行为一致,maxrss 同量级,exit: pass。
- [ ] **Step 2:** 自适应 `-n 8 -t 30`(放大消除的主证据):预期 `[压力映射] arm64_sdc_sve.buffer_bytes = ... (来源: 工作集自适应 600‰÷9)` ≈ 4.07 GiB/份(61 GiB 机);maxrss ≈ 36.6 GiB(9 份合计),无 swap 抖动,exit: pass。修复前同命令 = 146+ GiB 提交需求(issue #365 实测,不复刻)。
- [ ] **Step 3:** 自适应 `-n 32 -t 30`:预期 `600‰÷33` ≈ 1.11 GiB/份,maxrss ≈ 36.6 GiB,exit: pass。
- [ ] **Step 4:** 自适应 `-n 1 -t 30`(单线程零变化证据):预期 `600‰÷2` = 18.3 GiB/份,与修复前完全一致(修复前 -n 1 consumers 亦为 2);init 慢(18.3 GiB × memset + 2×CRC),timeout 放宽到 600 s;maxrss ≈ 36.6 GiB,exit: pass。
- [ ] **Step 5:** 每次大内存跑前查 `MemAvailable`;若 < 42 GiB,先等余量再跑(36.6 GiB 驻留需求)。

### Task 4: 回归 + docs 同步

- [ ] **Step 1:** 回归:`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- [ ] **Step 2:** x86 目检:确认全部改动在 `#if defined(__aarch64__)` 块内(该测试文件 x86 路径零改动)。
- [ ] **Step 3:** docs:`docs/SDC_STRESS_CONFIG_CN.md` 5.2 节补一句 consumers 用法:"'init 共享 1 份 + run 每线程 1 份副本'结构(如 arm64_sdc_sve)须报 `1 + thread_count()`(issue #365:报常数 2 会使 n 线程提交需求放大 n 倍)"。

### Task 5: 提交、PR、检视、合入

- [ ] **Step 1:** `git branch --show-current`(确认 feature 分支)→ `git add` 测试文件 + docs → `git commit -s`(单 commit,DCO 尾行,无 Co-Authored-By)。
- [ ] **Step 2:** push feature 分支 → `gh pr create --base main`(正文含选型论证与实测证据、`Fixes #365`)。
- [ ] **Step 3:** 派 `feature-dev:code-reviewer` 检视,批判性处置 findings(有理则修,无理则据)。
- [ ] **Step 4:** `gh pr merge <PR> --merge` → issue 自动关(`Fixes #365`)→ `gh issue view 365` 确认 closed。

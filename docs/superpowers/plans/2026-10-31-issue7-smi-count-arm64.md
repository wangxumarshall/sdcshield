# ARM64 smi_count 移植（issue #7）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `smi_count` 从 ARM64 诚实占位 SKIP 升级为真实测试：每 CPU 一个 worker 连续读 `CNTVCT_EL0`，统计相邻读间隔超过阈值的"被偷走时间"毛刺（次数/最大值/累计），作为解释其它测试时序抖动的诊断辅助。

**Architecture:** 三层判定（内核数据源不存在 → 方案二 PMU 不等价 → 方案一软件探针是唯一可行路径，但语义降级必须诚实标注）。实现完全落在 `tests/common/smi_count/smi_count.cpp` 的 `#elif defined(__aarch64__)` 新分支：init 做 SIGILL 守卫的能力探测，run 在标准 `TEST_LOOP` 窗口内逐 CPU busy 探测，非零才 `log_platform_message` 报告（x86 同构）。不改框架、不改 meson、x86 零触碰。

**Tech Stack:** C++20（测试文件现有风格）、AArch64 系统寄存器 inline asm（`mrs cntvct_el0/cntfrq_el0`，树内先例 `tests/cpu/virtualization/vmx_vmexit_from_cr8.cpp:7-9`）、POSIX `sigaction`/`sigsetjmp` 同步陷阱探测。

**Spec:** GitHub issue #7（三方案原文）+ 本文档"判定依据"节（两轮实测数据，是阈值与文档措辞的唯一根据）。

## Global Constraints

- x86-64 零改动：所有代码变更在 `#elif defined(__aarch64__)` 分支内；`git diff` 必须可逐行核验。
- `#else`（x86 与 ARM64 之外的架构）保留原诚实占位 SKIP，reason 逐字不变。
- DCO：`git commit -s`，最后一行且仅此一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，禁止 Co-Authored-By；只 push feature 分支，永不 push main。
- 构建零新警告：`./scripts/build.sh`（`SDC_BUILD_JOBS=20`，4 并发 agent 禁满核）。
- 回归不过不 commit：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
- 本机为 HIP09 128 核（SVE1、裸机），探针在本机可原生验证——CLAUDE.md 的 gem5-fi 替代验证条款不适用（能力本机具备）。
- 探针语义边界必须同时写进：代码注释、README、issue 关闭评论（永不虚报"这是固件计数器"）。

## 判定依据（两轮实测，2026-10-31，HIP09 128 核裸机，openEuler 6.6 内核）

### 1. 裸机 ARM64 无内核可见的 SMI_COUNT 等价物（方案 B 判死）

| 数据源 | 实测结果 |
|---|---|
| `/proc/stat` steal 列 | 自启动 2.57 天恒 0（paravirt 机制在、裸机无 provider） |
| `/proc/interrupts` | grep `el3\|smc\|firmware\|sdei\|secure` 零匹配；Group 0（EL3）中断由 TF-A 处理，结构性不进内核 |
| PMU `exc_taken`(0x09) | 对 EL0→EL1 的 SVC 精确 1:1 计数；内核态（SMC/HVC→EL3）计数需 root（`perf_event_paranoid=2`），SMC 是否计入未验证 |
| SDEI v1.0 | 固件有 ACPI SDEI 表，但事件计数只在 root-only debugfs |
| stolen-time sysctl/sysfs | `/proc/sys` 递归 grep 零匹配；`systemd-detect-virt`=none 确认裸机 |
| cpuidle | `/sys/devices/system/cpu/cpu0/cpuidle/` 不存在 → 稳态内核→EL3 SMC 流量 ≈ 0 |

方案三（改 TF-A 固件加 SMCCC OEM 函数）超出本仓库边界（需固件协同），不在本卡范围。

### 2. CNTVCT 毛刺分布（决定阈值与语义边界声明）

独立探针（128 线程各钉一核 busy 读 CNTVCT_EL0，10s，loadavg≈2.5 有其它 agent 活动）：

| 毛刺区间 | 总次数 | 次/cpu/10s | 主导来源 |
|---|---|---|---|
| ≤1µs | 23,952 | 187 | 普通 IRQ |
| ≤20µs | 125,236 | 978 | tick/IRQ 处理 |
| ≤100µs | 2,029 | 15.9 | 长中断 |
| 200µs–2ms | ~1,900 | ~19.6 | 谷区（固件级事件会落这里） |
| 2–5ms | 15,972 | 124.8 | **CFS 抢占其它租户任务（主导）** |
| >20ms | 0 | 0 | — |

最大毛刺 16.7ms（cpu62）；累计被偷 ~5.4%/CPU。读回环 ~25.4ns/次（39.38M 次/s/cpu）。CNTFRQ=100MHz；CNTVCT_EL0 用户态可读、CNTPCT_EL0 在 EL0 触 SIGILL。

**结论**：多租户机器上 >1ms 毛刺被调度抢占主导——探针测的是"时序噪声地图"不是"固件事件数"。阈值 100µs（远高于 tick/IRQ 的 1–20µs）计为毛刺，1ms 额外计为严重；两者都是分布分桶，诊断价值在数字本身。

## File Structure

- Modify: `tests/common/smi_count/smi_count.cpp`（唯一代码变更：`#else` 拆成 `#elif defined(__aarch64__)` 真实现 + `#else` 原占位）
- Modify: `README.md`（质量分级表 5→4/489→490、SKIP 清单、占位表删行、新增语义边界段）
- Modify: `docs/architecture.md`（占位示例句、InterruptMonitor 节 ARM64 路径说明）
- Modify: `docs/research/progress-log.md`（追加本卡条目）
- Create: 本计划文件
- 不改：`framework/`（InterruptMonitor 是"计数器采样"API，探针是"连续测量"，不适用）、`tests/common/meson.build`（文件已在编译清单）

---

### Task 1: smi_count.cpp ARM64 探针实现

**Files:**
- Modify: `tests/common/smi_count/smi_count.cpp:73-132`（整个 `#else` 段）

**Interfaces:**
- Consumes: `sandstone_p.h`（已 include）：`TEST_LOOP`、`log_skip`、`log_platform_message`、`device_info[]`、`DECLARE_TEST`；`SANDSTONE_LOG_INFO` 字面量前缀。
- Produces: 测试 id `smi_count` 在 ARM64 上为 PROD 级真实实现（供 README/docs/issue 收尾引用）。

- [ ] **Step 1: 记录改动前基线（TDD 的"红灯"）**

```bash
./builddir/sdcshield --quality=-1 -e smi_count -t 2000 2>&1 | grep -E "skip-reason|result:|exit:"
```
Expected: `result: skip`，skip-reason 为 `to be implemented (placeholder): SMI counting requires a per-CPU firmware/RAS-interrupt counter not available on this architecture`；`--list-tests` 不含 smi_count（SKIP 级不在默认集）。（若 builddir 尚未配置，先做 Task 2 Step 1 的构建。）

- [ ] **Step 2: 写入 ARM64 实现**

把 `#else // !__x86_64__` 起的整段（注释+四个函数+DECLARE_TEST）替换为下面三个分支结构。x86 段（`#if defined(__x86_64__)` 到 `#else` 之前）一字不动：

```cpp
#elif defined(__aarch64__)

/*
 * ARM64 port of smi_count (issue #7): a per-CPU "stolen time" probe.
 *
 * x86-64 counts SMM entries via the IA32_SMI_COUNT MSR (0x34): a hardware
 * counter, maintained by firmware, that the OS samples at two points in
 * time. ARM64 has no equivalent: on bare metal there is no kernel-visible
 * firmware-preemption counter (measured 2026-10-31 on a HiSilicon HIP09
 * 128-core host: /proc/stat steal is identically 0 since boot, EL3-owned
 * Group-0 interrupts never reach /proc/interrupts, PMU EXC_TAKEN counts
 * EL0->EL1 exceptions only and needs root for kernel-mode counting, SDEI
 * event counts live in root-only debugfs). The only consequence of "the
 * CPU was taken away from us" that an unprivileged per-CPU thread can
 * measure is a gap between two back-to-back reads of the architectural
 * counter CNTVCT_EL0 (user-space readable, ~25 ns per read+compare loop
 * iteration on the reference host).
 *
 * SEMANTIC BOUNDARY (deliberate): unlike MSR 0x34 this probe CANNOT
 * attribute a gap to its cause. A gap > threshold may be firmware
 * (SMC/EL3, RAS), a hypervisor steal (in a guest), an interrupt storm, or
 * plain kernel scheduling of other tasks. On a busy multi-tenant host the
 * millisecond-scale gaps are dominated by CFS preemption (measured: 2-5 ms
 * bucket, ~12 events/cpu/s under loadavg ~2.5). Treat the output as a
 * per-CPU timing-noise map that helps explain jitter seen by other tests,
 * NOT as a firmware-event count. This test never fails; it only reports.
 *
 * Observation window: the test's own run window (default 1 s, -t to
 * extend). The x86 version samples a global hardware counter from preinit
 * (application start) to run, i.e. it spans the whole sandstone run; a
 * software probe can only observe while it executes (a monitor thread
 * started in preinit would not survive the fork into the test child), so
 * the ARM64 window is narrower by construction.
 *
 * Thresholds: 100 us counts as a gap (well above timer-tick/IRQ handler
 * cost, 1-20 us measured), 1 ms is additionally tallied as severe. Both
 * are distribution buckets, not firmware attributions.
 */

#include <cinttypes>
#include <csetjmp>
#include <csignal>
#include <cstdint>

namespace {
/* Direct system-register reads, same pattern as
 * tests/cpu/virtualization/vmx_vmexit_from_cr8.cpp. */
static inline uint64_t read_cntvct_el0()
{
    uint64_t val;
    __asm__ volatile("mrs %0, cntvct_el0" : "=r"(val));
    return val;
}

static inline uint64_t read_cntfrq_el0()
{
    uint64_t val;
    __asm__ volatile("mrs %0, cntfrq_el0" : "=r"(val));
    return val;
}

/* A gap above this many microseconds counts as a timing anomaly. */
constexpr unsigned long GapThresholdUS = 100;
/* Gaps above this are additionally tallied as severe. */
constexpr unsigned long GapSevereUS = 1000;
/* Reads per TEST_LOOP iteration: 2^18 back-to-back reads take ~6.5 ms on
 * the reference host (25 ns/read), which keeps one framework iteration
 * above the 1 ms minimum enforced by --test-tests (debug builds) while
 * bounding the deadline-check overshoot to a single burst. */
constexpr int GapBurstReads = 1 << 18;

/* ticks -> microseconds; the split multiplication keeps both terms well
 * below 2^64 for any realistic window. */
static uint64_t ticks_to_us(uint64_t ticks, uint64_t freq)
{
    return (ticks / freq) * 1000000 + (ticks % freq) * 1000000 / freq;
}

/* Capability probe: CNTVCT_EL0/CNTFRQ_EL0 are EL0-readable unless this
 * kernel cleared CNTKCTL_EL1.EL0VCTEN; if they trap we take a SIGILL.
 * Recover via sigsetjmp/siglongjmp (the classic synchronous-trap probe
 * idiom: the trap is delivered on this same thread at a known instruction)
 * and skip honestly instead of crashing. The handler is installed before
 * the worker threads exist and restored immediately, so the framework's
 * own crash handler (child_debug) is preserved. */
static sigjmp_buf cntvct_probe_jmp;

static void cntvct_probe_sigill(int)
{
    siglongjmp(cntvct_probe_jmp, 1);
}

static int smi_count_init(struct test *test)
{
    (void) test;

    struct sigaction sa = {};
    struct sigaction old;
    sa.sa_handler = cntvct_probe_sigill;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGILL, &sa, &old) != 0) {
        log_skip(OSResourceIssueSkipCategory,
                 "could not install SIGILL probe handler to test for "
                 "CNTVCT_EL0 EL0 readability");
        return EXIT_SKIP;
    }

    bool readable = false;
    uint64_t freq = 0;
    if (sigsetjmp(cntvct_probe_jmp, 1) == 0) {
        (void) read_cntvct_el0();
        freq = read_cntfrq_el0();
        readable = freq != 0;       // defensive: avoid div-by-zero below
    }
    sigaction(SIGILL, &old, nullptr);

    if (!readable) {
        log_skip(RuntimeSkipCategory,
                 "CNTVCT_EL0/CNTFRQ_EL0 trap at EL0 (CNTKCTL_EL1.EL0VCTEN "
                 "cleared by this kernel); cannot probe stolen-time gaps");
        return EXIT_SKIP;
    }
    return EXIT_SUCCESS;
}

static int smi_count_run(struct test *test, int thread)
{
    const uint64_t freq = read_cntfrq_el0();        // verified non-zero in init
    const uint64_t threshold_ticks = freq * GapThresholdUS / 1000000;
    const uint64_t severe_ticks = freq * GapSevereUS / 1000000;

    uint64_t gaps = 0;
    uint64_t severe = 0;
    uint64_t max_delta = 0;
    uint64_t stolen_ticks = 0;

    uint64_t previous = read_cntvct_el0();
    TEST_LOOP(test, 1) {
        for (int i = 0; i < GapBurstReads; ++i) {
            uint64_t now = read_cntvct_el0();
            uint64_t delta = now - previous;
            previous = now;
            if (delta > threshold_ticks) {
                ++gaps;
                stolen_ticks += delta;
                if (delta > max_delta)
                    max_delta = delta;
                if (delta > severe_ticks)
                    ++severe;
            }
        }
    }

    if (gaps != 0) {
        int real_cpu_number = device_info[thread].cpu_number;
        log_platform_message(SANDSTONE_LOG_INFO
                             "timing gap events detected: %" PRIu64 " gaps > %luus "
                             "(%" PRIu64 " > %luus), max %" PRIu64 "us, total stolen "
                             "%" PRIu64 "us on thread %d cpu_number %d\n",
                             gaps, GapThresholdUS, severe, GapSevereUS,
                             ticks_to_us(max_delta, freq),
                             ticks_to_us(stolen_ticks, freq),
                             thread, real_cpu_number);
    }
    return EXIT_SUCCESS;
}
} // end anonymous namespace

DECLARE_TEST(smi_count, "Counts stolen-time gap events (ARM64 SMI analog)")
    .test_init = smi_count_init,
    .test_run = smi_count_run,
    .desired_duration = 0,           /* default window (1 s, -t overrides) */
    .fracture_loop_count = -1,       /* never fracture: stats must span the
                                        whole window, a fracture restarts run() */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

#else // other architectures: honest placeholder

/*
 * On x86-64 this test reads the IA32_SMI_COUNT MSR (0x34) via
 * /dev/cpu/<n>/msr. On aarch64 it is a real CNTVCT_EL0 read-gap probe (see
 * above). Other architectures keep the honest placeholder below: no
 * portable per-CPU firmware/RAS-interrupt counter is available, so the
 * test stays listed and schedulable but reports a clean skip ("to be
 * implemented (placeholder)") so the result is marked ignored rather than
 * spuriously passing.
 */

static int smi_count_preinit(struct test *test)
{
    (void) test;
    return EXIT_SUCCESS;
}

static int smi_count_init(struct test *test)
{
    (void) test;
    log_skip(TestResourceIssueSkipCategory,
             "to be implemented (placeholder): SMI counting requires a "
             "per-CPU firmware/RAS-interrupt counter not available on this "
             "architecture");
    return EXIT_SKIP;
}

static int smi_count_run(struct test *test, int thread)
{
    (void) test;
    (void) thread;
    __builtin_unreachable();    // init reports EXIT_SKIP, run never executes
}

static int smi_count_cleanup(struct test *test)
{
    (void) test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(smi_count, "Counts SMI events")
    .test_preinit = smi_count_preinit,
    .test_init = smi_count_init,
    .test_run = smi_count_run,
    .test_cleanup = smi_count_cleanup,
    .desired_duration = -1,
    .fracture_loop_count = -1,
    .quality_level = TEST_QUALITY_SKIP,
END_DECLARE_TEST

#endif // __x86_64__ / __aarch64__ / other
```

实现要点（执行者必须理解，不可"优化"掉）：
- `TEST_LOOP(test, 1)` + 内层 2^18 读：迭代时长 ~6.5ms，同时满足 `--test-tests`（Debug 构建）的 MinimumLoopDuration=1ms（sandstone_run.cpp:570）与 deadline 检查粒度；`prev` 声明在 TEST_LOOP 之外，跨 burst 连续测量，无窗口死角。
- `.fracture_loop_count = -1` 是硬要求：自动 fracture（=0）每 40 次内循环重启 run()（sandstone_run.cpp:1935-1938），统计会清零。
- `desired_duration = 0`（默认 1s 窗口）而非 x86 的 -1（runs once）——探针的价值就是窗口本身。
- 100µs 阈值换算 `freq * 100 / 1000000`：u64 无溢出（1e8×100=1e10）。
- 报告格式对齐 x86 版（`log_platform_message` + `SANDSTONE_LOG_INFO` + `on thread %d cpu_number %d`，仅在非零时输出）。
- SIGILL 守卫覆盖 `cntvct` 与 `cntfrq` 两个读；`sigsetjmp(buf, 1)` 的 savemask 保证跳回后 SIGILL 解除阻塞。

- [ ] **Step 3: 编译并确认零警告**

```bash
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release 2>&1 | tail -3
SDC_BUILD_JOBS=20 ./scripts/build.sh 2>&1 | grep -iE "warning|error" | grep -v "third-party" || echo CLEAN
```
Expected: `CLEAN`（无本仓新警告）。

- [ ] **Step 4: 行为验证（单核）**

```bash
./builddir/sdcshield -e smi_count -t 5000 -n 1 2>&1 | grep -E "result:|exit:|timing gap|smi_count"
```
Expected: `result: pass`、`exit: pass`；在 loadavg>0 的本机上大概率有 1-3 条 `timing gap events detected`（若无也合法——本机恰无毛刺）。`--list-tests` 现在应含 smi_count（PROD 入默认集）。

- [ ] **Step 5: 行为验证（128 核全核，噪声水平记录）**

```bash
./builddir/sdcshield -e smi_count -t 10000 2>&1 | tee /tmp/smi_count_fullcore.log | grep -cE "timing gap"
grep -oE "gaps > 100us \([0-9]+ > 1000us\), max [0-9]+us" /tmp/smi_count_fullcore.log | sort | uniq -c | sort -rn | head
```
Expected: pass；统计各线程毛刺分布，如实摘录进 PR 描述与 issue 收尾（这是"全核 busy 下探针噪声水平"的实证记录）。

- [ ] **Step 6: 回归 + x86 目检**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | grep -E "result:|exit:"
git diff --stat
git diff framework/ | wc -l   # must be 0
git diff tests/common/smi_count/smi_count.cpp | grep -E "^[-+]" | grep -vE "^[-+]{3}" | head -20  # 逐行确认全部落在非 x86 分支
```
Expected: zstd19 `exit: pass`；framework/ 零 diff；smi_count.cpp 的删改行全部位于原 `#else` 段（x86 段 `#if defined(__x86_64__)` 至 `#else` 之间零触碰）。

### Task 2: 文档同步（rule 7）

**Files:**
- Modify: `README.md:405-421` 区域
- Modify: `docs/architecture.md:54-57` 与 `:158-162`
- Modify: `docs/research/progress-log.md`（文末追加）

**Interfaces:**
- Consumes: Task 1 的实测输出（--list-tests 计数、全核毛刺分布）。
- Produces: 与代码 100% 一致的文档口径。

- [ ] **Step 1: README.md 质量分级表与清单**

1. 分级表：SKIP 行 `5` → `4`，PROD 行 `489` → `490`（合计 500 不变）；表下注的核对日期改为 2026-10-31 并注明 +1（smi_count 探针）。
2. "SVE1 主机实测 479 PROD" → 用本机复测值（预期 480，以 `--list-tests | grep -c` 实测为准）；"无 SVE 鲲鹏 920 为 297" → 298 并标注"同 +1 推算"（无 SVE 主机不在本机，不可实测——如实标注推算性质）。
3. SKIP 清单行删去 `smi_count`（保留 4 个 eigen_svd_jacobi）。
4. 占位表删去 `smi_count` 行（保留 ist 两行）。
5. 在 mce_check 说明段之后新增：

> **`smi_count`（ARM64）**：真实的 CNTVCT_EL0 时序毛刺探针——每个 CPU 一个 worker 在测试窗口内连续读架构计数器，统计相邻读间隔 >100µs 的"被偷走时间"事件（次数 / >1ms 严重数 / 最大值 / 累计），非零才经 `log_platform_message` 报告，永不失败。**语义边界**：无法区分毛刺来源（固件 SMC/EL3、虚拟化偷取、中断风暴、内核调度抢占——多租户负载下 2–5ms 桶由 CFS 抢占主导），与 x86 的 MSR 0x34（仅计 SMM、覆盖整个运行期）语义不同；ARM64 版观察窗口是本测试自身运行窗口（默认 1s，`-t` 可延长）。裸机上不存在内核可见的固件抢占计数器（steal 恒 0、EL3 中断不进 /proc/interrupts），这是可用的最接近等价物，定位是解释其它测试时序抖动的诊断辅助。x86-64 版本仍走 MSR 0x34 路径（零改动）。

- [ ] **Step 2: docs/architecture.md**

1. `:54-57` 占位规则示例：`Examples: tests/cpu/ist/ist.c, tests/common/smi_count/smi_count.cpp.` → `Example: tests/cpu/ist/ist.c. (tests/common/smi_count/smi_count.cpp was one until the ARM64 CNTVCT_EL0 gap probe landed — 2026-10-31, issue #7.)`
2. `:158-162` InterruptMonitor 节：`count_smi_events() uses read_msr (x86-only MSR 0x34 — no ARM equivalent, returns nullopt).` 后追加一句：`The ARM64 smi_count test therefore bypasses InterruptMonitor entirely — it is a CNTVCT_EL0 read-gap probe implemented inside the test, because no sampleable firmware-preemption counter exists on bare-metal ARM64.`

- [ ] **Step 3: progress-log 追加**

文末按现有格式追加一节：决策（A：软件探针）+ 判死依据（上表两轮实测）+ 实现位置 + 验证摘录（单核/全核输出、zstd19 回归、x86 零 diff）。

- [ ] **Step 4: 核对文档与代码一致性**

重读 README/architecture 修改段，确认：计数与 `--list-tests` 实测一致；语义边界措辞与代码注释同源；无"固件计数器"之类的虚报。

### Task 3: commit、PR、检视、收尾

- [ ] **Step 1: 提交（单 commit = 一个移植单元：计划+代码+文档）**

```bash
git branch --show-current   # 确认在 worktree 分支
git checkout -b feat/issue7-smi-count-arm64
git add docs/superpowers/plans/2026-10-31-issue7-smi-count-arm64.md \
        tests/common/smi_count/smi_count.cpp README.md \
        docs/architecture.md docs/research/progress-log.md
git commit -s -m "feat(smi_count): ARM64 CNTVCT_EL0 stolen-time gap probe (issue #7)

Replace the honest placeholder SKIP with a real per-CPU probe: each worker
busy-reads CNTVCT_EL0 for the test window and reports gaps >100us
(count / >1ms severe / max / total stolen) via log_platform_message.
Semantic boundary is documented in-code and in README: unlike x86 MSR
0x34 (SMM-only, whole-run), the probe cannot attribute gaps (firmware /
hypervisor / IRQ / CFS preemption) and observes only its own window.
Bare-metal ARM64 exposes no kernel-visible firmware-preemption counter
(measured: /proc/stat steal identically 0, EL3 Group-0 IRQs never in
/proc/interrupts, PMU EXC_TAKEN EL0-only without root, SDEI in root-only
debugfs) - this is the closest honest equivalent, a timing-noise map to
explain jitter in other tests. Never fails; PROD (x86 parity).
x86-64 path untouched; other-arch placeholder kept verbatim."
git log -1 --format=%B | tail -2   # DCO 行校验：Signed-off-by 且无 Co-Authored-By
```

- [ ] **Step 2: push + PR**

```bash
git push -u origin feat/issue7-smi-count-arm64
gh pr create --title "feat(smi_count): ARM64 CNTVCT_EL0 stolen-time gap probe (issue #7)" \
  --body "Fixes #7 ..."   # 正文含：判定依据表、语义边界、验证输出摘录（单核+全核+回归）
```

- [ ] **Step 3: code-reviewer 检视 + 处置 + 人工 merge 提醒**

按 pr-loop 流程派 code-reviewer；对抗性吸收意见（尤其关注：阈值论证、fracture=-1 理由、SIGILL 守卫正确性、x86 零改动核验）；修复轮次后提醒人工 merge，不自动 merge。

- [ ] **Step 4: merge 后 issue #7 收尾评论**

内容：决策 A 及理由（两轮实测判死 B/C 的证据链）、语义边界重申（对 issue 方案一的原措辞做一处修正：探针无法区分固件与调度，是"时序噪声地图"不是"SMI 等价计数器"）、PR 链接、全核噪声实测摘录。

## Self-Review 结论

- 覆盖：issue #7 三方案均有处置（一=实现，二=判死依据，三=边界外声明）；CLAUDE.md 规则 1/2/3/5/7 均有对应任务。
- 无占位符：所有代码/命令/文档措辞已给出全文。
- 类型一致：`read_cntvct_el0/read_cntfrq_el0/ticks_to_us` 定义与调用一致；`GapThresholdUS/GapSevereUS/GapBurstReads` 常量名一致。
- 风险点已前置：fracture 重启清零（用 -1 规避）、--test-tests 迭代下限（burst 6.5ms）、SIGILL 与 child_debug 共存（save/restore）、uint64 溢出（拆分乘法）、-t 极小值时的 deadline 超限（<30ms 的 -t 在 Debug --test-tests 模式可能被标记 overtime——文档化接受，默认 -t 1000ms）。

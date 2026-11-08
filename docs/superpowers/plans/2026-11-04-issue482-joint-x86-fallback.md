# Issue #482: sdc_joint_dim_slice x86 fallback 补齐(四源预算 + 时间校准)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 解除 `sdc_joint_dim_slice` x86 `#else` fallback 的 armed trap——补齐四源内存预算与 `solve_once` 时间校准,使未来跨 arch 调用方拆 guard 迁移时 x86-64 与 ARM 同受「时间×内存双上限」契约,而非静默失去 2000ms 级单解时间上限与 MemAvailable/cgroup/commit 三源预算。

**Architecture:** fallback 保持「全局口径」(无 per-node/AR/VmHWM 反馈——x86 无 `sdc_numa.h` 基础设施),但能力面与 ARM 分支同构:内存侧 `sdc_budget_bytes(permille)`(arch-independent,同文件 `#if` 之外,四源取 min),时间侧 `sdc_calibrated_dim` 幂律拟合求 `n_run`(AR=1 诚实退化:`run_budget_ms` 语义不变 = 全核单迭代(2 解)预算,单线程探针口径与调用方旧链一致)。调用方 guard 全部不动(rule 5:现行 x86 行为 byte-identical;拆 guard 是未来迁移的决策,不在本 issue 范围)。

**Tech Stack:** C++20 header-only 层(`framework/sdc_stress_config.h`),`-U__aarch64__` 预处理激活验证 harness(g++ aarch64 后端编译+执行 fallback 的 arch-independent 逻辑)。

**Spec:** issue #482(audit:cef3087a92cd01ff)+ `docs/superpowers/plans/2026-10-27-eigen-p0-p3-fix.md` Task 8/9(迁移终态:两步链 → 一次 `sdc_joint_dim_slice`)+ `docs/architecture.md` "Eigen sizing" 节。

## Global Constraints

- rule 1(一个补丁一个单元):本 plan 全部改动一个 commit(同一缺陷的代码+失实注释+文档面)。
- rule 2(自验证 100% 真):验证引用真实命令输出;本机无 x86-64 工具链/sysroot(实测 `clang++ --target=x86_64-linux-gnu` 败于 aarch64 glibc 头的 `__neon_vector_type__`/SVE 类型,见 Task 3 Step 2),真 x86 硬件验证不可行——边界如实声明,以 `-U__aarch64__` 的真实编译+执行替代(fallback 逻辑 100% arch-independent:四源预算读 `/proc`/`/sys`,时间校准纯 C++ 数学,无 intrinsic)。
- rule 3(DCO):`git commit -s`,最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,其后无内容。
- rule 5(x86-64 untouched):Intel 原代码零触碰;`sdc_stress_config.h` 是 SDCShield 自有框架层;**所有调用方的 `#if defined(__aarch64__)` guard 与 `#else` 旧链原样保留**——当前 x86 无任何调用方经 fallback(调用点全部 guard),本修复不改变任何现行 x86 行为。
- 零新警告(`./scripts/build.sh`)。

## 根因(已调查,证据链)

`#else` fallback 写于 Task 4/5/6 时代(当时调用方全部是 ARM-only TU,x86 上不编译,fallback 无害)。Task 8/9 后调用方变为**跨 arch TU**(SVD 模板 10 用户 / eigen_sparse / GEMM 7 文件),防御从「TU 不编译」降级为「每调用方手写 guard + `#else` 旧链」;fallback 声称的契约——「时间校准维持调用方既有的 sdc_calibrated_dim 链」(`sdc_stress_config.h` #else 注释原文)——从此只能靠调用方自觉兑现。plan Task 8/9 的终态(「一次 sdc_joint_dim_slice」替换两步链)要求拆 guard,届时 x86 静默失去:① `solve_once` 被 `(void)` 掉(eigen_sparse 注释自述的「自适应档满核 600s SIGQUIT」故障类复活);② 预算只剩物理×‰,丢 MemAvailable×0.9 / cgroup v1+v2 / CommitLimit−Committed_AS×0.9 三源("job oom" 实测前科,issue #310)。

**复现记录(2026-11-04,本机 aarch64,`-U__aarch64__` 激活 x86 fallback,未修改代码):**

```
$ ./spike x 900
[harness] phys=31.0GiB budget_bytes(500‰)=15.5GiB MemAvail=27.7GiB
W> [联合求解] harness_probe.mdim: n_mem=10769 → 8192 (全局口径 27.9GiB; rule 5: x86 原路径, 无 per-node/AR/时间外推)
[harness] result n=8192 probe_calls=0
```

- `probe_calls=0`:调用方传入真实 `solve_once` 与 `run_budget_ms=2000`,fallback 一次都没调——时间校准被静默丢弃,n=8192 无任何时间上限(幂律 t(n)=1ms·(n/64)³ 下 8192 档单解 ≈ 33 分钟,即「600s SIGQUIT」故障类)。
- 「全局口径 27.9GiB」= 纯物理 31.0GiB×900‰;四源 `sdc_budget_bytes(900)` 本机实测 = min(27.9, 27.7×0.9=24.9, …) = 24.9GiB(MemAvailable 源绑定)——超分 ~12%。
- harness 完整源码见 Task 3(进 plan 文档持久化,不进仓库可执行面)。

## 调用方盘点(修复涉及面)

真正调用 `sdc_joint_dim_slice` 的调用方分两类(全部不触 x86 fallback):
1. **ARM-only TU**(meson `tests_set_sve` 等构建块,x86 不编译):`gemm_double_max_arm`、`svd_cdouble_sve`、`eigen_svd_bidiag_sve`(+各自 body)。
2. **跨 arch TU 但调用点 `#if defined(__aarch64__) && defined(__linux__)` guard**(x86 编译、走 `#else` 旧链):`tests/cpu/eigen_svd/sandstone_eigen_common.h`(:219,SVD 模板 10 用户)、`tests/cpu/eigen_sparse/eigen_sparse.cpp`(:167)、GEMM 7 文件(`gemm_double_dynamic_square.cpp:109` 等)。

---

### Task 1: 补齐 x86 fallback(核心修复)

**Files:**
- Modify: `framework/sdc_stress_config.h:1160-1185`(`sdc_joint_dim_slice` 的 `#else` 分支)

**Interfaces:**
- Consumes: `sdc_budget_bytes(int permille)`(同文件 :296,arch-independent)、`sdc_calibrated_dim(int lo, int hi, long max_single_ms, const char *test_id, const char *knob, double k_floor, F solve_once, struct sdc_fit *fit)`(同文件 :695,template,arch-independent,已被 x86 `#else` 调用方链使用即 x86-compile-clean)、`struct sdc_fit { double a_ms; double p; }`。
- Produces: 同名函数同签名;`#else` 分支新行为——`n = min(n_mem, n_run, hi)` + lo 兜底,`n_run` 来自 `sdc_calibrated_dim` 的 fit 外推(AR=1)。

- [ ] **Step 1: 替换 #else 分支为完整实现**

用下面内容整体替换 `#else ... #endif`(:1160-1185,从 `/* x86-64 fallback(rule 5: 行为不变): 仅 n_mem 全局口径` 到 `return n;` 与 `#endif` 之间):

```c
#else
    /* x86-64 fallback(#482): 四源全局预算 + 时间校准——能力面与 ARM
     * 分支同构, 减去 per-node/AR/VmHWM 反馈(x86 无 sdc_numa.h 基础设施):
     *   - 内存: sdc_budget_bytes(四源取 min) 按全机线程×c 分摊 + slices×
     *     shared。旧口径仅物理×‰——批处理/容器限额下超分("job oom"
     *     实测前科, svd_cdouble_sve.cpp 注释; #310 的四源函数本就
     *     arch-independent, 读不到的源自动跳过, 退化回物理×‰)。
     *   - 时间: solve_once 非空且 run_budget_ms>0 时经 sdc_calibrated_dim
     *     幂律拟合求 n_run(AR=1: x86 无 AR 表, 与调用方旧链同为单线程
     *     探针口径; run_budget_ms 语义与 ARM 一致 = 全核单迭代(2 解)
     *     预算)。旧 fallback `(void)solve_once` 静默丢弃时间校准——
     *     跨 arch 调用方拆 guard 迁移时会失去单解时间上限,
     *     eigen_sparse 注释自述的『自适应档满核 600s SIGQUIT』故障类
     *     随之复活(#482 的 armed trap), 本补丁解除之。
     * 行为边界(如实): 当前 x86 无调用方经此分支(调用点均 aarch64
     *     guard, 见各调用方 #else 注释), 本补丁不改变任何现行 x86
     *     行为; 改变的是未来迁移者得到的契约。rule 5 依归: Intel 原代码
     *     零触碰, 调用方 #else 旧链原样保留。 */
    int total = sdc_total_threads();
    if (total < 1)
        total = 1;
    int cur = thread_count();
    int slices = (cur > 0 && total > cur) ? (total + cur - 1) / cur : 1;
    double budget = (double)sdc_budget_bytes(permille);
    double avail = budget - a->shared_bytes_per_slice * (double)slices;
    int n_mem = (c_bytes > 0.0 && avail > 0.0)
            ? (int)std::sqrt(avail / ((double)total * c_bytes)) : 0;
    int n_run = 0;
    if (solve_once && a->run_budget_ms > 0 && n_mem > a->lo) {
        /* 与 ARM 分支同构: 只取 fit(两点差分已扣固定开销 b), 返回值
         * 弃用(其内向外推是未扣 b 的保守口径); k_floor=3.0 与 ARM 分支
         * 同值(现役/规划消费方复杂度下限, sdc_joint_args 不带此参的
         * 签名契约); 探针上限 = knob hi(同 ARM: 差分基线拉得越开,
         * 固定开销扣除对计时噪声越稳)。AR=1 → per_solve=run_budget/2。 */
        long per_solve = (long)((double)a->run_budget_ms / 2.0);
        if (per_solve < 1)
            per_solve = 1;
        struct sdc_fit fit;
        sdc_calibrated_dim(a->lo, a->hi, per_solve,
                           test ? test->id : "(global)", knob, 3.0,
                           solve_once, &fit);
        if (fit.a_ms > 0.0 && fit.p > 1.0 && std::isfinite(fit.a_ms)) {
            double nr = std::pow((double)a->run_budget_ms
                                 / (2.0 * fit.a_ms), 1.0 / fit.p);
            if (nr >= 2147483647.0)
                n_run = 2147483647;        /* 防溢出截断 */
            else if (nr > 0.0)
                n_run = (int)nr;
        }
    }
    int n = n_mem;
    if (n_run > 0 && n_run < n)
        n = n_run;
    if (a->hi < n)
        n = a->hi;
    if (n < a->lo)
        n = a->lo;
    log_warning("[联合求解] %s.%s: n_mem=%d n_run=%d → %d (全局四源口径 %.1fGiB; "
                "x86: 无 per-node/AR/反馈)",
                test ? test->id : "(global)", knob ? knob : "?",
                n_mem, n_run, n, budget / 1073741824.0);
    return n;
#endif
```

注意:`n_mem > a->lo` 的 gate 与 ARM 分支(:1117)一致——内存下限都不够时时间校准无意义,直接走 lo 兜底。`solve_once == NULL || run_budget_ms == 0` 时 n_run=0,`min(n_mem, n_run)` 退化为纯内存口径(头文件既有文档契约「solve_once 可为 NULL(不校时间, 只算 n_mem); run_budget_ms=0 同理」——修复后该契约对 x86 也成立)。

- [ ] **Step 2: harness 复跑(修复后行为验证 + 手工核算)**

见 Task 3(验证 harness 与预期数值)。

---

### Task 2: 同步失实注释(rule 7 / 诚实性:注释不得说谎)

**Files:**
- Modify: `tests/cpu/eigen_svd/sandstone_eigen_common.h:230-239`(`#else` 注释)
- Modify: `tests/cpu/eigen_sparse/eigen_sparse.cpp:181-184`(`#else` 注释)
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square.cpp:97-98`(`#else` 注释)
- Modify: `docs/architecture.md:276-283`(fallback 描述段)

**Interfaces:** 无代码接口;纯注释/文档措辞,指向 Task 1 的新契约。

- [ ] **Step 1: 三处调用方注释更新**

三处的共同失实陈述是「sdc_joint_dim_slice 的 x86 分支忽略 solve_once/run_budget_ms」。修复后仍成立的不迁移理由是:**数值口径会变**(RSS 表/四源全局预算 vs 三元组物理×‰;`run_budget_ms` 口径 vs 2000ms 单解上限;SVD/sparse 的 per-family run_budget 是按 ARM 宿主 AR 校准的值)——rule 5 的 byte-identical 仍要求 guard。各处改法:

`sandstone_eigen_common.h`(:231-239,英文注释)——把
`VERBATIM on non-aarch64 — sdc_joint_dim_slice's x86 branch ignores solve_once/run_budget_ms (its time calibration is the CALLER's chain by contract), so routing x86 through the joint solver would silently drop the 2 s single-solve cap and swap the legacy memory triple for the ARM-measured RSS table.`
改为
`VERBATIM on non-aarch64 — the joint solver's x86 branch now honors solve_once (#482: four-source global budget + power-law time cap), but routing x86 through it would still swap the legacy memory triple for the ARM-measured RSS table, the global four-source budget for the physical-only one, and the 2 s single-solve cap for the run_budget_ms semantics (calibrated on ARM hosts' AR) — a sizing change rule 5 forbids.`

`eigen_sparse.cpp`(:181-184,中文注释)——把
`sdc_joint_dim_slice 的 x86 分支忽略 solve_once/run_budget_ms, 无条件迁移会静默丢掉 2s 单解时间校准并把内存三元组换成 ARM 实测的 RSS 表。`
改为
`sdc_joint_dim_slice 的 x86 分支已补齐时间校准与四源预算(#482), 但迁移仍会把内存三元组换成 ARM 实测的 RSS 表、把 2s 单解上限换成按 ARM 宿主 AR 校准的 run_budget_ms 口径——sizing 数值变化, rule 5 保持旧链。`

`gemm_double_dynamic_square.cpp`(:97-98,中文注释)——把
`x86-64(rule 5): #else 保留旧链原样——joint 的 x86 分支按契约忽略 solve_once/run_budget_ms(时间校准是调用方链的职责)。`
改为
`x86-64(rule 5): #else 保留旧链原样——joint 的 x86 分支已补齐时间校准与四源预算(#482), 但迁移仍会把内存三元组换成 ARM 实测的 RSS 表/四源全局预算, sizing 数值变化, rule 5 保持旧链。`

- [ ] **Step 2: architecture.md :276-283 段更新**

把 `The `#else` branch inside `sdc_joint_dim_slice` itself is the defensive definition of the same legacy semantics; no live x86 caller reaches it today (all call sites are `#if defined(__aarch64__) && defined(__linux__)`-gated), it exists so a future cross-arch caller gets defined, loudly-logged behavior instead of a compile error.`
改为
`The `#else` branch inside `sdc_joint_dim_slice` itself is the global-arch fallback (#482): four-source global budget + power-law time cap via `sdc_calibrated_dim` (AR=1, no per-node/AR/VmHWM feedback — x86 has no sdc_numa.h). No live x86 caller reaches it today (all call sites are `#if defined(__aarch64__) && defined(__linux__)`-gated); it exists so a future cross-arch caller that drops its guard gets the same memory×time dual cap as ARM instead of silently losing the single-solve time bound. Migrating an x86 caller still changes its sizing numbers (RSS table / four-source budget / run_budget_ms semantics), which rule 5 keeps to the callers' own `#else` chains.`

---

### Task 3: 验证(rule 2 全链)

**Files:**
- Test: `/tmp/spike482/`(一次性 harness,不进仓库;源码持久化于本 plan 供复现)

**Interfaces:**
- Consumes: Task 1 的修改;`-U__aarch64__` 预处理技巧(g++ 对内建宏的 `-U` 在预处理层生效,fallback 代码无 arch 指令,aarch64 后端编译/执行等价)。

- [ ] **Step 1: harness 源码(完整,供任何人复现)**

`/tmp/spike482/stub/sandstone.h`:
```c
/* Minimal stub of framework/sandstone.h for the #482 x86-fallback
 * verification harness ONLY (not part of the repo). Signatures copied
 * verbatim from the real header so the included sdc_stress_config.h
 * compiles against the true API surface. */
#ifndef STUB_SANDSTONE_H
#define STUB_SANDSTONE_H
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdarg.h>

struct test { const char *id; };

extern __thread int thread_num;
extern void log_message(int thread_num, const char *msg, ...);
int thread_count(void);
int total_thread_count(void);
bool quick_run(void);

#define SANDSTONE_LOG_ERROR     "E> "
#define SANDSTONE_LOG_WARNING   "W> "
#define log_error(...)          log_message(thread_num, SANDSTONE_LOG_ERROR __VA_ARGS__)
#define log_warning(...)        log_message(thread_num, SANDSTONE_LOG_WARNING __VA_ARGS__)
#define log_info(...)           log_message(thread_num, "I> " __VA_ARGS__)
#endif
```

`/tmp/spike482/stub/test_knobs.h`:
```c
/* Minimal stub of framework/test_knobs.h (#482 harness only). */
#ifndef STUB_TEST_KNOBS_H
#define STUB_TEST_KNOBS_H
#include <stdint.h>
#include <stdbool.h>
struct test;
int64_t get_testspecific_knob_value_int(const struct test *test, const char *key,
                                         int64_t value_if_not_present);
bool set_knob_from_key_value_string(const char *key_value_pair);
#endif
```

`/tmp/spike482/spike.cpp`:
```cpp
/* #482 复现/验证 harness —— 不进仓库。
 * -U__aarch64__ 在预处理层把 sdc_stress_config.h 的
 * #if defined(__aarch64__) && defined(__linux__) 判否, 编译并执行
 * sdc_joint_dim_slice 的 x86 #else fallback 分支。fallback 代码
 * 全部为 arch-independent API(四源预算读 /proc 与 /sys, 时间校准为
 * 纯 C++ 数学), 在本机 aarch64 上执行的结果对 x86-64 Linux 有效。 */
#include <sandstone.h>
#include "sdc_stress_config.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>

/* ---- framework 符号的 harness 实现 ---- */
__thread int thread_num = -1;
static int g_thread_count = 8, g_total_threads = 8;
static bool g_quick = false;
void log_message(int, const char *msg, ...)
{
    va_list ap; va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    fputc('\n', stderr);
    va_end(ap);
}
int thread_count(void) { return g_thread_count; }
int total_thread_count(void) { return g_total_threads; }
bool quick_run(void) { return g_quick; }
int64_t get_testspecific_knob_value_int(const struct test *, const char *,
                                        int64_t d) { return d; }
bool set_knob_from_key_value_string(const char *) { return true; }

/* ---- 可控计时探针: 计数 + 每档目标耗时 ---- */
static int g_probe_calls = 0;
static int probe(int n)
{
    ++g_probe_calls;
    /* 幂律 t(n) = 1ms·(n/64)^3: 64→1ms, 128→8ms, 256→64ms, 512→512ms,
     * 1024→4096ms; k=log2(4096/512)=3.0 恰落 k_floor。 */
    double ms = 1.0 * (double)n * n * n / (64.0 * 64.0 * 64.0);
    auto t0 = std::chrono::steady_clock::now();
    while (std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - t0).count() < ms) {
    }
    return 0;
}

int main(int argc, char **argv)
{
    if (argc > 1 && argv[1][0] == 'q') g_quick = true;
    struct test t = { "harness_probe" };
    struct sdc_joint_args ja = {};
    ja.lo = 64; ja.hi = 8192;
    ja.fam = SDC_RSS_GEMM_F64;          /* 表值 0.0315 KB/n²/线程 */
    ja.bytes_per_elem = 8; ja.matrices_per_thread = 4; ja.rss_ratio = 6.0;
    ja.shared_bytes_per_slice = 0;
    ja.mem_fraction_permille = (argc > 2) ? atoi(argv[2]) : 500;
    ja.ar_fam = SDC_FAM_MEM_BOUND;
    ja.run_budget_ms = 2000;            /* 与调用方旧链同量级的时间预算 */

    fprintf(stderr, "[harness] phys=%.1fGiB budget_bytes(%d‰)=%.1fGiB "
            "MemAvail=%.1fGiB\n",
            (double)sdc_phys_mem_bytes() / 1073741824.0,
            ja.mem_fraction_permille,
            (double)sdc_budget_bytes(ja.mem_fraction_permille) / 1073741824.0,
            (double)sdc_memavailable_bytes() / 1073741824.0);
    int n = sdc_joint_dim_slice(&t, "mdim", &ja, probe);
    fprintf(stderr, "[harness] result n=%d probe_calls=%d\n", n, g_probe_calls);
    return 0;
}
```

编译:`g++ -std=c++20 -U__aarch64__ -I/tmp/spike482/stub -I<repo>/framework -Wall -Wextra -o spike spike.cpp -pthread`
(允许 harness 自身 stub 的 `unused parameter` 一类噪音;**被测头文件的 fallback 代码必须零警告**——若 fallback 代码报警告即失败。)

- [ ] **Step 2: 修复后运行 + 手工核算预期**

Run: `./spike x 500`(本机 31GiB 物理、8 线程口径)
Expected(手工核算):
- 四源:500‰ → 物理×‰ = 15.5GiB;MemAvailable×0.9 = ~24.9GiB > 15.5 → 物理源绑定,budget = 15.5GiB(与修复前数值相同的本机巧合,但来源是四源 min);permille=900 时 budget 必须是 24.9GiB(MemAvailable 绑定)而修复前是 27.9GiB——**预算差必须显现**。
- 时间校准(per_solve=1000ms):probe 档自适应 64(1ms)→128(8ms)→256(64ms)→512(512ms≥100ms)停;mid=1024(4096ms);k=3.0;外推 dim=512×(500/512)^(1/3)≈507;验证探针 probe(507)≈497ms ≤ 1000ms 不收缩;fit.a = (4096−512)/(1024³−512³) ≈ 3.8147e-6 ms;n_run = (2000/(2×3.8147e-6))^(1/3) ≈ 640。
- **n = min(n_mem≈8026, n_run≈640, hi=8192) ≈ 640,probe_calls ≈ 5-6(>0)**。n 显著小于纯内存档 = 时间上限生效的直接证据;`[时间校准]` 与 `[联合求解] ... n_run=640` 日志行出现。
- 回归对照:`./spike x 900` → budget=24.9GiB(MemAvailable 钳制,修复前 27.9)。

- [ ] **Step 3: 全量构建零新警告**

Run: `./scripts/build.sh`
Expected: 与修改前相同的零警告全量构建(aarch64 主路径不受影响——fallback 在本机不编译,构建验证的是「没改坏别处」)。

- [ ] **Step 4: 回归**

Run:
```
./builddir/sdcshield -e zstd19 -t 2000 -n 1   # 标准(rule 2c)
./builddir/sdcshield -e eigen_sparse -t 2000 -n 1   # joint 调用方抽测(ARM 分支)
./builddir/sdcshield -e eigen_svd_double -t 2000 -n 1  # SVD 模板族(经 sandstone_eigen_common.h)
```
Expected: 全部 `exit: pass`;`[联合求解]` 日志的 n_mem/n_run/AR 行与修改前同构(ARM 分支代码零改动,抽测防意外)。

- [ ] **Step 5: x86 非回归边界(如实声明)**

本机无 x86-64 工具链/sysroot:实测 `clang++ --target=x86_64-linux-gnu` 在 include aarch64 glibc 头时报 `__neon_vector_type__`/`__SVFloat32_t` 不支持——真 x86 编译无法在本机完成。x86 侧验证 = (a) `-U__aarch64__` 的真实编译+执行(Step 1-2,arch-independent 逻辑);(b) 代码检视:fallback 无 intrinsic/无 arch 头依赖;(c) 现行 x86 行为零变化的论证(调用点全部 guard,PR 报告引用 guard 位置清单)。CI 若有 x86 构建将以真 x86 编译覆盖 (a) 的语法面。

---

### Task 4: 提交、PR、报告

- [ ] **Step 1:** `git checkout -B fix/issue-482 origin/main`(单树纪律;工作契约)
- [ ] **Step 2:** `git add framework/sdc_stress_config.h tests/cpu/eigen_svd/sandstone_eigen_common.h tests/cpu/eigen_sparse/eigen_sparse.cpp tests/cpu/eigen_gemm/gemm_double_dynamic_square.cpp docs/architecture.md docs/superpowers/plans/2026-11-04-issue482-joint-x86-fallback.md`
- [ ] **Step 3:** `git commit -s -m "fix(sizing): sdc_joint_dim_slice x86 fallback 补齐四源预算与时间校准 (#482)

...正文:根因(armed trap)、改动点、验证证据摘要..."
最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`
- [ ] **Step 4:** `git push origin fix/issue-482`
- [ ] **Step 5:** `gh pr create --base main --head fix/issue-482 --title "fix: sdc_joint_dim_slice x86 fallback 补齐四源预算与时间校准 (#482)" --body "Closes #482 + 实现/修复报告(根因、改动点、验证证据、验证边界)"`(auto-pr.yml 抢先时兜底 `gh pr edit fix/issue-482`)
- [ ] **Step 6:** 报告贴 issue #482(根因/方案、改动、验证证据含真实输出、PR 链接);`git checkout main && git status` 须 clean

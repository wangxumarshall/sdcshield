# CLAUDE.md 分层瘦身重构 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将 CLAUDE.md（191 行）重构为分层结构：铁律+命令+怪癖+按任务触发的 docs 导航图（≤115 行），深度架构内容迁入新文件 `docs/architecture.md`（~100 行），全部数字/路径/行号断言以 2026-09-24 实测为准。

**Architecture:** 两文件两提交的纯文档重构（零代码改动、零 x86 影响）。CLAUDE.md 承载 agent 每会话必需的内容（硬规则、核心命令、平台怪癖、docs 导航）；深水区内容（fork 生命周期、feature 检测管线、sysdeps 分层、RAS、RNG）原样迁入 `docs/architecture.md` 并由 CLAUDE.md 指针引用。

**Tech Stack:** Markdown / git（DCO 签核、feature 分支、自动推送）。

**Spec:** 2026-09-24 会话内批准的设计（bounded 路径，无独立 spec 文件）。设计要点：分层瘦身哲学；英文正文+中文铁律；导航图按任务触发器组织（非目录树）；CPU179 双根因叙事必须声明引用线系。

## Global Constraints

- 全程在 feature 分支 `docs/claude-md-layered-refactor` 上工作，**绝不 push main**（当前在 main @ b85da2ed，Task 1 首步切分支）。
- 每个提交必须 `git commit -s`，尾签 `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`，其后无任何内容（无 Co-Authored-By）。
- `git add` 只加明确列出的文件——**绝不 `git add -A` / `git add .`**：工作区有范围外未跟踪文件 `docs/sdc-excite-reproduce/sdc-reproduce.md`，不得入库。
- 用户未提交的 CLAUDE.md `### Always` 段（git diff 所示 5 行中文规则）必须**逐字保留**进新 CLAUDE.md，随本次重构一并提交。
- 本计划所有"Expected"值均为 2026-09-24 在本机（main @ b85da2ed，builddir 新鲜构建）实测所得；执行时命令输出若不符，以实测为准修正文档内容并在此计划中勾选时注明。
- 新 CLAUDE.md 行数验收：`wc -l` ≤ 115（目标 ~105）。
- 零代码改动：`git diff --stat` 中不得出现 framework/ tests/ third-party/ 下任何文件。

---

### Task 1: 创建分支并提交本计划

**Files:**
- Create: （无——本计划文件已在磁盘上）
- Modify: 无

**Interfaces:**
- Produces: feature 分支 `docs/claude-md-layered-refactor`（基于 main @ b85da2ed）；计划文件入库。

- [ ] **Step 1: 创建并切换到 feature 分支**

```bash
git checkout -b docs/claude-md-layered-refactor
```

Run 后确认输出含 `Switched to a new branch 'docs/claude-md-layered-refactor'`。工作区的 `M CLAUDE.md` 会随分支带过来——这是预期的（Task 3 吸收）。

- [ ] **Step 2: 提交计划文件（仅此一个文件）**

```bash
git add docs/superpowers/plans/2026-09-24-claude-md-layered-refactor.md
git commit -s -m "docs(plans): CLAUDE.md layered refactor plan — 2 commits, fact-verified"
```

- [ ] **Step 3: 验证提交与工作区边界**

Run: `git log -1 --format='%h %s%n%(trailers:only)'`
Expected: 新提交哈希 + 上述标题 + `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`（最后一行）。
Run: `git status --short`
Expected: 仅 ` M CLAUDE.md` 与 `?? docs/sdc-excite-reproduce/sdc-reproduce.md`（与开工时一致）。

- [ ] **Step 4: 推送分支**

```bash
git push -u origin docs/claude-md-layered-refactor
```

---

### Task 2: 新增 `docs/architecture.md`（深度架构承接文件）

**Files:**
- Create: `docs/architecture.md`

**Interfaces:**
- Consumes: 现 CLAUDE.md "Architecture" 段全部事实（已逐条实测：`sandstone.cpp:546/559` 守卫、`child_run` 在 `framework/sandstone_run.cpp:1232`、libc 覆盖块在 `framework/random.cpp:781-883`、全部引用路径存在）。
- Produces: `docs/architecture.md`——Task 3 的 CLAUDE.md 导航图与 Architecture 段指向它。

- [ ] **Step 1: 写入完整内容**

写入 `docs/architecture.md`（内容如下，行号引用均为 2026-09-24 实测）：

````markdown
# SDCShield Architecture Deep-Dive

> Companion to `CLAUDE.md` — read this **before touching `framework/`**.
> All file/line references verified against main @ b85da2ed on 2026-09-24.

## Test lifecycle (fork model)

Each test runs in a **forked child process**:

```
parent: run_one_test_inner (framework/sandstone_run.cpp)
  → child_run (framework/sandstone_run.cpp:1232)
      → test_init                     (child main thread)
      → per-CPU worker threads        (test_run via test_run_wrapper_function)
      → test_cleanup
```

The parent collects results via `forkfd`. A test that crashes crashes the
*child*, which reports back through `CrashContext`
(`framework/sysdeps/unix/child_debug.cpp`).

Fork modes (`SandstoneApplication::ForkMode`): `no_fork`, `fork_each_test`
(default for non-debug builds), `exec_each_test` (default for debug builds).

`test_preinit` runs **once in the parent** and is nulled after first run, so it
does not re-run across iterations — use `test_init` for per-iteration skipping.

## Test declaration & the "tests" section

Tests are `struct test` instances placed in a special ELF section
(`section("tests")`) via the `DECLARE_TEST` macro; the framework iterates
`__start_tests`..`__stop_tests` to find them. `DECLARE_MANUAL_TEST` omits the
section attribute — used for special/injected tests like `mce_check`, which is
always appended last regardless of selection.

`struct test` fields: `test_preinit` / `test_init` / `test_run` /
`test_cleanup` (function pointers; NULL is allowed and skipped — **except
`test_run`, which must be non-NULL**), `minimum_cpu` (feature gate),
`quality_level`, `groups`, `flags`.

## Quality levels

`TEST_QUALITY_SKIP`(-1) < `BETA`(0) < `PROD`(2, default `--quality=2`). A test
runs when `quality_level >= requested_quality`. The SKIP-skip guard
(`framework/sandstone.cpp:546` and `:559`) drops SKIP-level tests unless
`requested_quality < 0` — so a SKIP-level test with a NULL `test_run` will
**crash** under `--quality=-1` unless it self-skips in init (see `mce_check`).

## Cross-architecture portability model

The x86-64 implementation is the reference; ARM64 is a parallel port. Gating is
`#ifdef __x86_64__` with ARM64 `#elif`/stubs. Rules:

- Placeholder tests for unimplemented-on-ARM features **must skip** with reason
  `"to be implemented (placeholder)"` (`EXIT_SKIP` from `test_init`, never
  `EXIT_SUCCESS`). Examples: `tests/cpu/ist/ist.c`,
  `tests/common/smi_count/smi_count.cpp`.
- Real features get real implementations; truly-absent hardware (ARM has no
  CPU microcode/PPIN/MSR) stays a documented stub, not faked.
  `tests/common/mce_check/mce_check.cpp` is a *real* EDAC-backed test on ARM64.

## CPU feature detection (generated pipeline)

```
framework/device/cpu/simd.conf      (x86 keys: CPUID leaves)  ─┐
framework/device/cpu/simd-arm.conf  (ARM keys: HWCAP bits)   ─┴→ {x86simd,armsimd}_generate.pl
                                                              (Perl, at meson-configure time)
                                                              → builddir/.../cpu_features.h
                                                              (GENERATED — do not edit)
```

The generated header emits `device_features_t` (128-bit), `cpu_feature_*`
constants, `cpu_*` arch macros, `features_string`/`features_indices` tables,
and `x86_locators`/`x86_architectures` tables. x86 keys on CPUID leaves; ARM
keys on `getauxval(AT_HWCAP/HWCAP2)`. **Both generators emit identical symbol
names**, so `cpu_device.cpp` / `cpuid_internal.h` work unchanged across
arches. `detect_cpu()` (`cpuid_internal.h`) fills the global `device_features`;
`device_has_feature(cpu_feature_X)` / `minimum_cpu` gating consume it;
`dump_device_info()` (`cpu_device.cpp`) and `device_features_to_string()`
consume the generated tables.

## Sysdeps layering

```
framework/sysdeps/<os>/                 linux | darwin | freebsd | windows
framework/sysdeps/unix/                 shared unix (incl. child_debug.cpp)
framework/device/cpu/sysdeps/<os>/      CPU-specific
framework/sysdeps/generic/              non-x86 fallbacks (e.g. kvm.c skip stub)
```

Arch gating is **inside files** (`#ifdef __x86_64__` / `__aarch64__`) and
**inside meson** (`host_machine.cpu_family()`): e.g. `msr.c` is x86-only,
`interrupt_monitor.cpp` is x86+aarch64.

## InterruptMonitor / RAS

`framework/interrupt_monitor.hpp` + `sysdeps/linux/interrupt_monitor.cpp`.
`InterruptMonitorWorks` is `true` on linux x86-64 + aarch64. x86 counts MCE/TRM
lines from `/proc/interrupts`; aarch64 counts EDAC `ce_count` + `ue_count`
(controller-wide, placed at index 0). `count_smi_events()` uses `read_msr`
(x86-only MSR 0x34 — no ARM equivalent, returns nullopt). `mce_check` is a
special always-inserted test.

## Tests directory layout

- `tests/common/` — arch-agnostic (mce_check, smi_count)
- `tests/cpu/` — compute tests (eigen_*, zlib, zstd, ifs, ist, openssl,
  openblas_gemm, sleef, pocketfft, isa-l, mesh_*)
- `tests/{gpu,idxd}/` — only built for `-Ddevice_type=gpu/idxd`
- `tests/examples/` — never built; reference only

`tests/cpu/meson.build` uses meson **sourcesets**: `tests_set_base` (all
arches), `tests_set_hsw`/`tests_set_skx` (x86 AVX2/AVX512, compiled with
`-DEigen=EigenAVX2`/`EigenAVX512` namespace renames so multiple SIMD backends
link without symbol clash), `tests_set_sve` (aarch64 SVE, `-DEigen=EigenSVE`).

## RNG

`framework/random.cpp`. Engines: Constant, LCG, AES (auto-picked when
`haveAes()`; list via `-s help`). The AES engine uses `#pragma GCC target`
per-arch: x86 `_mm_aesenc_si128`, aarch64 `vaesmcq_u8(vaeseq_u8(...))` via
`+crypto`. RNG state is per-thread (`thread_rng` union, 64-byte aligned). The
framework **overrides libc seeding functions** (`framework/random.cpp:781-883`
`extern "C"` block — `srand` aborts, etc.).

## Eigen 5.0 SVE double packets (vendored)

The vendored Eigen 5.0 SVE backend carries `double`/`complex<double>` packets
(`PacketXd` in `arch/SVE/PacketMath.h`, `PacketXcd` + `arch/SVE/Complex.h`),
making `eigen_svd_cdouble_sve` run vectorized SVD (~0.7 s at M_DIM 300 vs
>10 min scalar). **Runtime-VL rule**: size-specific SVE code requires the
process vector length to equal the compile-time `-msve-vector-bits`; this host
is VL=256 while `tests_sve` compiles at 128, so the SVD SVE test must run via
the `prctl(PR_SVE_SET_VL, 16)` launcher in `scripts/eigen-sve-double/`.
`sleef_sve` compiles at 128 by design.
````

- [ ] **Step 2: 验证文中全部路径/行号引用**

Run:
```bash
ls framework/sandstone_run.cpp framework/sandstone.cpp framework/sysdeps/unix/child_debug.cpp \
   framework/device/cpu/simd.conf framework/device/cpu/simd-arm.conf \
   tests/cpu/ist/ist.c tests/common/smi_count/smi_count.cpp tests/common/mce_check/mce_check.cpp \
   framework/interrupt_monitor.hpp framework/sysdeps/linux/interrupt_monitor.cpp \
   third-party/eigen5/Eigen/src/Core/arch/SVE/PacketMath.h scripts/eigen-sve-double/
```
Expected: 全部存在，无 "No such file"。

Run: `grep -c "child_run" framework/sandstone_run.cpp`
Expected: ≥3（含 :1232 定义）。

Run: `sed -n '546p;559p' framework/sandstone.cpp`
Expected: 两行均含 `quality_level < 0 && sApp->requested_quality >= 0`。

- [ ] **Step 3: 提交并推送**

```bash
git add docs/architecture.md
git commit -s -m "docs: add architecture deep-dive (moved from CLAUDE.md, refs verified 2026-09-24)

Moves fork lifecycle, tests ELF section, quality gates, cross-arch portability
model, feature-detection generator pipeline, sysdeps layering, RAS and RNG
out of CLAUDE.md so the agent-facing file stays lean. All file/line refs
re-verified against main @ b85da2ed."
git push
```

Run: `git log -1 --format='%(trailers:only)'`
Expected: `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`。

---

### Task 3: 重写 CLAUDE.md（分层瘦身 + docs 导航图）

**Files:**
- Modify: `CLAUDE.md`（全文重写；吸收工作区未提交的 `### Always` 段）

**Interfaces:**
- Consumes: Task 2 的 `docs/architecture.md`（导航图与 Architecture 段指向它）；实测事实卡（422 tests / 70 ipsec / zstd19 pass / RNG 三引擎）。
- Produces: ≤115 行的新 CLAUDE.md。

- [ ] **Step 1: 写入新内容（整文件替换）**

写入 `CLAUDE.md`（内容如下；若最终行数 >115，压缩注释与空行，**不得删减铁律内容**）：

````markdown
# CLAUDE.md

Guidance for Claude Code in this repository. Deep-dives live under `docs/` —
the Documentation map below routes you; consult it before guessing.

## What this is

SDCShield is a CPU/system defect-detection tool: tests stress compute units
(FMA, SIMD, compression, SVD, crypto) and byte-compare against golden values,
to catch **silent data corruption (SDC)** — computations that don't crash but
produce wrong bits. Forked from Intel's OpenDCDiag; ported to ARM64 (Kunpeng
920 / generic ARMv8.1+); x86-64 is the untouched reference architecture.

## Non-negotiable rules

1. **One patch per unit** — each feature/bug/porting point is its own commit;
   never bundle. Work multi-item lists sequentially: verify → commit → push,
   then next.
2. **Self-verify before commit, 100% real** — (a) `ninja -C builddir` clean,
   zero new warnings; (b) run the affected behavior and quote real output;
   (c) regression: `-e zstd19 -t 2000 -n 1` → `exit: pass`; (d) x86
   non-regression by inspection (changes under `#elif __aarch64__` or per-arch
   meson guards). No "should work" claims — failing verification means fix and
   re-verify, never commit.
3. **DCO + auto-push** — `git commit -s` (ends with `Signed-off-by:
   wangxumarshall <wangxumarshall@qq.com>`, nothing after, no Co-Authored-By).
   Push to the remote feature branch after each verified commit; never push to
   `main`. Before committing, run `git branch --show-current` (sessions can
   switch branches mid-flight).
4. **Plan-driven** — anything beyond a single obvious line requires the
   `superpowers:writing-plans` skill first, plan saved under
   `docs/superpowers/plans/YYYY-MM-DD-<feature>.md`. No plan, no code.
5. **x86-64 untouched** — the ARM64 port is additive (`#elif __aarch64__`,
   per-arch meson guards); widen `#ifdef __x86_64__` to include `__aarch64__`
   only where genuinely shared.
6. **Placeholder honesty** — unimplemented-on-ARM features must skip with
   reason `"to be implemented (placeholder): <what's missing>"` (`EXIT_SKIP`
   from `test_init`, never `EXIT_SUCCESS`). A no-op test reporting pass is a
   bug. `mce_check` is a real EDAC-backed test on ARM64 and must pass.
7. **Docs sync** — 大颗粒度修改必须同步更新 README.md 与 docs/ 下对应文档，
   确保 100% 准确。

## Always

- Reply in Chinese.
- Be concise.
- State result before cause when practical.
- 必须诚实、不能说谎、必须100%服从事实、所有工作和结果必须基于事实并且经过严格的逻辑推理或实证，永远尊重事实、永远真诚。
- 所有回答必须通俗易懂却不失深刻，必须言简意赅，深刻准确。

## Build & run

```console
# ARM64 needs Eigen 5.0.0+ (vendored; system Eigen 3.3.x breaks on GCC 12+):
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
ninja -C builddir
# Fresh clone: run the vendored build.sh scripts first (openssl/openblas/
# sleef/isa-l/acl — see README). Missing install/ dirs degrade gracefully:
# meson prints a message and those tests aren't built.

./builddir/sdcshield --list-tests             # 422 tests at default PROD quality
./builddir/sdcshield -e zstd19 -t 5000        # one test, 5 s, all CPUs
./builddir/sdcshield -e zstd19 -t 5000 -n 1   # single-threaded (deterministic)
./builddir/sdcshield --quality=-1 -e <test>   # include SKIP-level tests
./builddir/sdcshield --selftests ...          # framework self-tests
./builddir/sdcshield --dump-cpu-info          # detected CPU/features/topology
./builddir/sdcshield -s help                  # RNG engines: Constant/LCG/AES
./builddir/sdcshield --on-crash=context -e selftest_sigsegv -vv
```

After changing meson sources/options: `meson setup --reconfigure builddir ...`
then `ninja`. Verbose: `-v` / `-vv`. Logging: `-Dlogging_format=yaml|tap|
no_output`. Full CLI reference (incl. undocumented options):
`docs/research/usage.md`.

## Writing tests

Full guide: `docs/misc/writing_tests.md`. Registration:
`docs/research/adding-tests.md`. Style rules (C++20/C17, GCC-10, no warnings,
naming, test-dir layout): `docs/misc/coding_style_guide.md`. Minimal pattern:

```c
#include "sandstone.h"
static int my_init(struct test *test) { /* set up golden data */ return EXIT_SUCCESS; }
static int my_run(struct test *test, int cpu) {
    TEST_LOOP(test, N) { /* compute, memcmp_or_fail(actual, golden, len, "...") */ }
    return EXIT_SUCCESS;
}
DECLARE_TEST(my_test, "description")
    .test_init = my_init, .test_run = my_run, .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
```

- `EXIT_SKIP` (-255) from init/run → skipped (log the reason first via
  `log_skip(category, "reason")`).
- `report_fail()` / `memcmp_or_fail()` abort the thread.
- `TEST_LOOP` is the standard timed loop; `test_time_condition()` checks budget.
- Golden comparison is **byte-identical `memcmp`** — FP must be reproducible
  (the framework quiets SNaN uniformly in `sandstone_data.cpp` so FP16/BF16 NaN
  bit patterns match cross-arch).

## Architecture (mental model)

Each test runs in a forked child process; the child's `test_init` spawns
per-CPU worker threads calling `test_run`; the parent collects results via
`forkfd`. Tests are `struct test` instances in a special ELF `tests` section;
quality levels gate what runs. **Before touching `framework/`, read
`docs/architecture.md`** (fork lifecycle, feature-detection generator pipeline,
sysdeps layering, InterruptMonitor/RAS, RNG).

## Known platform quirks (Kunpeng 920, openEuler 24.03 SP3)

- `sysfs cluster_id` increases monotonically (138→654→1170…) and
  `physical_package_id` is large — firmware/ACPI-PPTT artifact, read as-is.
- No `cpufreq` → `--vary-frequency` / `--vary-uncore-frequency` skip
  gracefully.
- No `thermal_zone*` CPU zones → thermal throttle is a no-op here.
- `eigen_svd_double` / `eigen_sparse` fail sporadically under full-system
  multithreading (192 CPUs; ULP-level diffs vs strict memcmp) — `-n 1` always
  passes. Eigen/large-core-count limitation, not a port defect.
- SVE runtime-VL rule: host is VL=256, `tests_sve` compiles at 128 → the eigen
  SVD SVE test needs the VL-pinning launcher in `scripts/eigen-sve-double/`;
  `sleef_sve` stays at 128 by design.

## Documentation map — consult before guessing

- **Framework internals**: `docs/architecture.md` (fork model, quality gates,
  feature detection, sysdeps, RAS, RNG) — read before touching `framework/`.
- **Writing tests / code style**: `docs/misc/writing_tests.md`,
  `docs/research/adding-tests.md`, `docs/misc/coding_style_guide.md`.
- **CLI beyond this file**: `docs/research/usage.md` (all options).
- **Build / deploy / multi-OS**: README (quick start, 15-version build);
  `docs/build-deploy/` (design + usermanual + offline deps).
- **Session recovery** (machine may die anytime; persist conclusions to disk):
  `docs/research/progress-log.md`; fact card in `docs/research/README.md`.
- **SDC case history**: `docs/cases/` — CPU179 has TWO competing root-cause
  lineages (OoO register-liveness vs LSU load-return); state which you cite.
  Also CPU122 (mercurial core), cn23154 (P0→P6 core localization), hpc
  (counter-example methodology). Excitation/reproduction methodology:
  `docs/sdc-excite-reproduce/`.
- **Microarch / hardware reference**: `docs/cpu/arm64-microarchtecture-internals/`
  (canonical start; Ch.9 SDC-sensitive units, Ch.10 CPU179 case),
  `docs/cpu/arm64/kungpeng/` (measured 920 + PMU events),
  `docs/cpu/arm64/armv8-isa/` (per-instruction), `neoverse-*-trm/`;
  gem5+CHAOS fault injection: `docs/gem5-doc/`.
- **Literature rationale** (why these workloads/libs — 31-paper synthesis):
  `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`; fault-injection experiment plan:
  `docs/paper/SDC_FAULT_INJECTION_EXPERIMENT_PLAN_CN.md`.
- **Theory**: `docs/hypothesis/ARM64-SDC-uArch.md`. **Output schema**:
  `docs/sdcshield-cpu.schema.json`. **Provenance/compliance**:
  `docs/misc/OPEN_SOURCE_PROVENANCE.md`.
- **Process artifacts**: `docs/superpowers/` (plans/, specs/, inventory/).
````

- [ ] **Step 2: 行数与内容验收**

Run: `wc -l CLAUDE.md`
Expected: ≤ 115（目标 ~105；超出则压缩注释/空行，铁律内容不得删）。

Run: `grep -c "Signed-off-by: wangxumarshall" CLAUDE.md && grep -c "所有回答必须通俗易懂" CLAUDE.md`
Expected: 1 和 1（`### Always` 段逐字保留）。

Run: `grep -c "docs/architecture.md" CLAUDE.md`
Expected: ≥2（Architecture 段 + 导航图）。

- [ ] **Step 3: 命令与数字断言实测复验**

Run: `./builddir/sdcshield --list-tests | wc -l`
Expected: `422`（与文中一致；若变，改文中数字为本命令实测值）。

Run: `./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1`
Expected: `exit: pass`。

Run: `./builddir/sdcshield -s help | tr '\n' ' '`
Expected: `Constant LCG AES`。

Run: `ls docs/architecture.md docs/misc/writing_tests.md docs/research/usage.md docs/research/adding-tests.md docs/research/progress-log.md docs/misc/coding_style_guide.md docs/misc/OPEN_SOURCE_PROVENANCE.md docs/build-deploy/ docs/sdc-excite-reproduce/ docs/gem5-doc/ docs/hypothesis/ARM64-SDC-uArch.md docs/sdcshield-cpu.schema.json docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md docs/paper/SDC_FAULT_INJECTION_EXPERIMENT_PLAN_CN.md scripts/eigen-sve-double/ > /dev/null && echo ALL_EXIST`
Expected: `ALL_EXIST`。

- [ ] **Step 4: 提交（吸收未提交的 ### Always 改动）并推送**

```bash
git branch --show-current   # 必须仍是 docs/claude-md-layered-refactor
git add CLAUDE.md
git commit -s -m "docs: refactor CLAUDE.md into layered rules + navigation map

191 → ~105 lines. Hard rules (DCO, one-patch-per-unit, self-verification,
x86-untouched, placeholder honesty) front-loaded; deep architecture moved to
docs/architecture.md; new task-triggered Documentation map over the full
docs/ tree (9 knowledge domains). Stale facts refreshed with 2026-09-24
measurements (278→422 PROD tests, 46→70 ipsec) and the writing_tests.md path
drift fixed (docs/ → docs/misc/). Absorbs the pending '### Always' section."
git push
```

Run: `git log -1 --format='%(trailers:only)' && git status --short`
Expected: `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`；status 仅剩 `?? docs/sdc-excite-reproduce/sdc-reproduce.md`。

---

## Self-Review 记录

- **Spec 覆盖**：设计三要素（分层瘦身 / 英文正文+中文铁律 / 任务触发式导航图）分别落于 Task 3 正文、`### Always` 段、Documentation map 段；事实重测落于各 Step 3；2 提交结构 = Task 2 + Task 3（Task 1 为计划入库，仓库先例 `docs(plans):` 独立提交）。✓
- **占位符扫描**：两份文件全文嵌入计划，无 TBD/TODO。✓
- **一致性**：Task 2 产出的 `docs/architecture.md` 与 Task 3 引用路径一致；实测值（422/70/pass/三引擎/546/559/1232/781-883）在两处出现处一致。✓

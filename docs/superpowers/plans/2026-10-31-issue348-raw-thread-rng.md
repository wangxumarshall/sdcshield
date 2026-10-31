# Issue #348: framework RNG raw-thread 槽位混叠修复 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除裸 `std::thread` 调用框架 RNG 时全体混叠 worker-0 槽（`rngs[1]`）的数据竞争（issue #348），框架线程 RNG 流 bit 级不变。

**Architecture:** `framework/random.cpp` 内为每线程缓存一个引擎指针 TLS（`tls_engine`）；框架线程在既有初始化路径（`random_init_global` / `random_init_thread` / `thread_num==-1` 首用回退）绑定到与今天完全相同的槽位；从未走框架初始化的线程（裸 `std::thread`，`thread_num` TLS 零初始化为 0）惰性播种一个私有 `thread_local` 引擎，mixin 来自进程级原子计数器 ^ 固定盐，播种素材取自当前全局引擎状态（保持 `-s` 种子依赖性）。

**Tech Stack:** C++20 / meson / ninja；ThreadSanitizer（`-Db_sanitize=thread`）作竞争消除实证。

**Spec:** GitHub issue #348（正文见 `gh issue view 348`）；RNG 设计契约见 `docs/architecture.md` "RNG" 章节（本次不破坏任何既有契约，仅补一条新契约：裸线程得到惰性私有引擎）。

## Global Constraints

- **x86-64 非回归是硬约束**（CLAUDE.md rule 5）：`framework/random.cpp` 两架构共享；本 patch 为纯 C++ TLS 逻辑，无任何 arch 专用代码改动；框架线程（main / init 辅助线程 / worker）绑定槽位与今天逐一相同 → 流 bit 级不变（Task 3 实证）。
- **RNG 语义契约保持**：每线程独立引擎、`-s <engine>:<seed>` 对框架线程可重放、`random_advance_seed` 行为（仅在测试间推进全局槽）不变。
- **构建**：`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `SDC_BUILD_JOBS=20 ./scripts/build.sh`，零新警告（4 并发 agent 禁满核）。
- **提交**：`git commit -s`，最后一行且仅此一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`；push feature 分支，绝不 push main。
- **禁止虚报**：所有验证引用真实命令输出；TSan 不可行则如实记录（本机为 aarch64，预期可行）。

## 选型论证（issue 列出的三个方向）

**结论：选 A（惰性初始化的 `thread_local` 引擎缓冲，不依赖 thread_num 编号），实现为"每线程引擎指针 TLS + 裸线程惰性私有引擎"，全部改动收敛在 `framework/random.cpp`，零新 API、零调用方改动。**

- **A（选定）**：
  - 根治：任何未来裸线程用户自动获得私有引擎，无需记得调用任何 API；
  - 改动面：单文件（framework/random.cpp）+ 文档；热路径从「TLS 读 `thread_num` + assert + `sApp→random_engine→per_thread` 三级依赖链」变为「单次 TLS 指针读 + 预测分支」，不劣反优；
  - x86 非回归：框架线程绑定到与现在相同的槽（bit 级同流）；裸线程在 x86 树内无 RNG 用户（cache_stress_aggressor 的 victim 是 `#ifdef __aarch64__`），行为零变化；同时修掉 x86 未来的同型雷；
  - 可重放性：裸线程流 = f(全局状态, 盐^计数器)，仍依赖 `-s` 种子；残余限制（诚实声明）：计数器→线程的分配顺序取决于首次 RNG 调度的交错，逐裸线程跨运行重放无法保证（需测试侧显式协作播种才可根除，超出本 issue 范围，见 docs 更新）。
- **B（`random_init_thread()` 裸线程安全变体，否决）**：需要新公共 API + 每个调用方显式 opt-in——下一个忘调用的裸线程用户踩回同一 UB；且为达成"零调用方改动"仍需 A 的 TLS 逃逸口，等于 A + 冗余面。
- **C（文档化禁止 + 测试侧本地 PRNG，否决）**：把缺陷留在框架；cache_stress_aggressor 要自带 PRNG（重复造轮子、丢失 `-s` 种子联动）；且无法保护未来用户。issue 本身将其定位为"供讨论"的保守项，实测触发点存在（TSan 可证），无理由不动框架。

**证据链（file:line）**：
- `framework/sandstone_run.cpp:86` `thread_local int thread_num = 0;` — 裸线程 TLS 零初始化为 0（缺陷根因）；
- `framework/random.cpp:128-131` `thread_local_rng()` → `rng_for_thread(0)` → `&rngs[1]`；
- 框架线程正规路径：worker `thread_runner` → `random_init_thread`（sandstone_run.cpp:792）；并行/顺序两条调度路径都过它（:792 唯一调用点；sequential 循环 :885-890 每 t 重调）；main 线程 `thread_num = -1`（sandstone.cpp:1023）；test-init 辅助线程 `thread_num = -1`（sandstone_run.cpp:1363，且 `pthread_join` 于 worker 诞生之前 :1383-1385）；
- 播种安全性前提：全局槽 `rngs[0]` 仅在测试间被写（`random_advance_seed` 调用点 sandstone_run.cpp:1982/2029，均在 worker join 之后）与 init 期（random_init_global，无线程存活）——测试运行期间惰性播种对 `rngs[0]` 的读无并发写者；
- 唯一树内裸线程 RNG 用户：`tests/cpu/cache/cache_stress_aggressor.cpp:51-54`（victim_thread 4× `frandomf_scale`）。`memcpy_sem.cpp`（RNG 在 worker 的 run 循环里调，裸线程只 memcpy）与 `fisttp_arm.cpp`（`std::thread::hardware_concurrency()` 而非线程创建）**不是**触发点——修正了我最初的误判；
- 值域安全（issue 已核验）：`random_template<float>`（random.cpp:670-687）mantissa 掩码保证返回 ∈ [0, scale)，无假阳性——本修复是 UB/质量/重放修复，不是正确性修复。

## File Structure

- Modify: `framework/random.cpp` — 全部代码改动（TLS 绑定 + 惰性裸线程引擎 + 两个 init 函数各加一行绑定 + `<atomic>` include）。
- Modify: `docs/architecture.md` — RNG 章节补裸线程契约一段（rule 7 文档同步）。
- Modify: `tests/cpu/cache/cache_stress_aggressor.cpp` — victim 注释更新（"框架 RNG 按线程独立流"从 wishful 变为事实，引用 #348）。
- Create: 本 plan 文件。
- 不改：`framework/sandstone.h`（无新 API）、`framework/sandstone_run.cpp`、任何调用方。

---

### Task 1: 基线证据（pristine main 上，patch 前）

**Files:** 无代码改动；产物为基线记录。

- [x] **Step 1.1: 基线构建**

```bash
cd <worktree>
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
SDC_BUILD_JOBS=20 ./scripts/build.sh
```

预期：构建成功。记录任何既有警告（作为"零**新**警告"的比对基线）。

- [x] **Step 1.2: `--list-tests` 基线计数**

```bash
./builddir/sdcshield --list-tests | wc -l
```

记录 N_baseline（本机 SVE-256，预期 ~478 量级，以实测为准）。

- [x] **Step 1.3: 框架线程 RNG 流基线（selftest 直印 RNG 值）**

`selftest_logs_random_init`（init 辅助线程，thread_num=-1）与 `selftest_logs_random`（worker，thread_num=0..N）直接 `log_info("%u %u %u %u", r1..r4)` 打印 `random()` 原始值——是框架线程流 bit 级比对的最直接探针。

```bash
./builddir/sdcshield --quality=-1 -s LCG:1234 -e selftest_logs_random_init -t 100 -n 1 > /tmp/base_init.log 2>&1
./builddir/sdcshield --quality=-1 -s LCG:1234 -e selftest_logs_random -t 100 -n 4 > /tmp/base_run.log 2>&1
./builddir/sdcshield --quality=-1 -s LCG:1234 -e selftest_logs_random -t 100 -n 4 > /tmp/base_run2.log 2>&1
grep -E "^[0-9]+ [0-9]+ [0-9]+ [0-9]+$" /tmp/base_init.log /tmp/base_run.log /tmp/base_run2.log
```

预期：base_run.log 与 base_run2.log 的 RNG 值行完全一致（同构建同 seed 自确定性成立——这是后续跨构建比对有效的前提）。若 log_info 行带前缀，按实际格式 grep。记录全部值。

- [x] **Step 1.4: 随机性敏感测试基线（zstd19 / random_access_sweep / cache_stress_aggressor）**

```bash
./builddir/sdcshield -s LCG:1234 -e zstd19 -t 3000 -n 1 > /tmp/base_zstd.log 2>&1; echo "exit=$?"
./builddir/sdcshield -s LCG:1234 -e random_access_sweep -t 3000 -n 1 > /tmp/base_ras.log 2>&1; echo "exit=$?"
./builddir/sdcshield -s LCG:1234 -e cache_stress_aggressor -t 15 -n 4 > /tmp/base_csa.log 2>&1; echo "exit=$?"
```

预期：三者 exit=0（pass）。记录各自的 pass 行与任何数据依赖输出。注：cache_stress_aggressor 的 victim 取值路径修复后**预期改变**（见 Task 3 Step 3.4），此处只记录框架侧行为与 pass 状态。

- [x] **Step 1.5: TSan 基线——证明竞争存在（"失败的测试"）**

```bash
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir-tsan -Db_sanitize=thread --buildtype=debug
SDC_BUILD_JOBS=20 ./scripts/build.sh -C builddir-tsan
./builddir-tsan/sdcshield -s LCG:1234 -e cache_stress_aggressor -t 30 -n 4 > /tmp/base_tsan.log 2>&1; echo "exit=$?"
grep -c "WARNING: ThreadSanitizer" /tmp/base_tsan.log
grep -A6 "WARNING: ThreadSanitizer: data race" /tmp/base_tsan.log | head -30
```

预期：TSan 报 `rngs[1]`（`sApp->random_engine->per_thread` 向量 data+4 字节偏移附近）的 write-write race（多 victim 对同一 LCG 状态字非原子 RMW）。若 TSan 构建不可行（链接/运行环境问题），如实记录并依赖 Step 3.5 的替代证据（全核长跑 pass + 逻辑论证），不虚报。

### Task 2: 修复 patch（framework/random.cpp）

**Files:**
- Modify: `framework/random.cpp:9-14`（include `<atomic>`）、`:117-131`（TLS 绑定 + 惰性裸线程引擎 + 新 `thread_local_rng`）、`:579-644`（`random_init_global` 尾部加绑定）、`:664-668`（`random_init_thread` 加绑定）。

**Interfaces:**
- Consumes: `rng_for_thread(int)`（既有）、`sApp->random_engine->seedThread(thread_rng*, uint32_t)`（既有虚接口）、`thread_num`（extern TLS，只读）。
- Produces: `thread_local_rng()` 新语义（对外签名不变——所有 RNG 入口 `random32/64/128`、`frandom*_scale`、`random()`、`memset_random` 等行为自动更新）；无新公共符号。

- [x] **Step 2.1: include 区加 `<atomic>`**

在 `framework/random.cpp:9`（`#include <algorithm>`）后按字母序插入：

```cpp
#include <atomic>
```

- [x] **Step 2.2: 替换 :117-131 的 `thread_local_rng` 区块**

将

```cpp
namespace {
static thread_rng *rng_for_thread(int thread_num)
{
    assert(thread_num < thread_count());
    assert(thread_num >= -1);

    auto &rngs = sApp->random_engine->per_thread;
    assert(rngs.size());
    return &rngs[thread_num + 1];
}

static thread_rng *thread_local_rng()
{
    return rng_for_thread(thread_num);
}
```

替换为：

```cpp
namespace {
static thread_rng *rng_for_thread(int thread_num)
{
    assert(thread_num < thread_count());
    assert(thread_num >= -1);

    auto &rngs = sApp->random_engine->per_thread;
    assert(rngs.size());
    return &rngs[thread_num + 1];
}

// -- per-thread engine binding (issue #348) --
// Every thread caches its engine slot in tls_engine. Framework threads run
// one of the framework's init paths before any RNG use:
//   - main thread / re-initing child: random_init_global() binds below;
//   - per-CPU workers, parallel AND sequential scheduling alike:
//     random_init_thread() (sandstone_run.cpp) binds the slot;
//   - the test-init aux thread runs with thread_num == -1
//     (sandstone_run.cpp), which thread_local_rng() resolves to the global
//     engine slot, as it always did.
// Threads spawned by test code with a raw std::thread run NONE of those:
// their thread_num TLS is zero-initialized to 0, which used to alias
// framework worker 0's slot (rngs[1]) -- up to thread_count() such threads
// doing non-atomic read-modify-write on one engine state = a data race
// (C++ UB, issue #348). Those threads now get a private, lazily-seeded
// engine instead.
static thread_local thread_rng *tls_engine = nullptr;

// engine storage for raw (non-framework) threads, seeded on first RNG use
static thread_local thread_rng tls_raw_engine_storage;

// salt so a raw thread's mixin never equals a worker's mixin_from_device_info
// scramble() for plausible topologies (a collision would only duplicate one
// stream, never corrupt one)
constexpr uint32_t kRawThreadMixinSalt = UINT32_C(0x52617721);    // "Raw!"

// hands each raw thread a distinct mixin
static std::atomic<uint32_t> raw_thread_mixin_counter{0};

static thread_rng *raw_thread_engine()
{
    // Seed from the global engine's CURRENT state so the stream still depends
    // on `-s <engine>:<seed>`. The global slot is only written between test
    // executions (random_advance_seed, sandstone_run.cpp, runs after all
    // worker threads joined) and during random_init_global itself (no test
    // threads exist yet) -- reads here are race-free by that schedule.
    uint32_t mixin = kRawThreadMixinSalt
            ^ raw_thread_mixin_counter.fetch_add(1, std::memory_order_relaxed);
    sApp->random_engine->seedThread(&tls_raw_engine_storage, mixin);
    tls_engine = &tls_raw_engine_storage;
    return tls_engine;
}

static thread_rng *thread_local_rng()
{
    if (thread_rng *r = tls_engine)
        return r;
    if (thread_num == -1) {
        // main thread and test-init aux thread: the global engine slot,
        // exactly as before (thread_num is set to -1 before any RNG use)
        tls_engine = rng_for_thread(-1);
        return tls_engine;
    }
    // unbound and not a -1 thread: a raw thread spawned by test code
    // (framework workers bind in random_init_thread before any test code
    // runs on them)
    return raw_thread_engine();
}
```

- [x] **Step 2.3: `random_init_global` 尾部绑定（:644 `}` 前）**

在函数末尾（`seedGlobalEngine(sseq);` 所在 if/else 块之后、闭合 `}` 之前）加：

```cpp
    // (re)bind the calling (main / re-initing child) thread to the fresh
    // global engine slot: a child that re-runs this must not keep a cached
    // pointer into the previous engine's freed per_thread vector (issue #348)
    tls_engine = rng_for_thread(-1);
```

- [x] **Step 2.4: `random_init_thread` 绑定（:664-668）**

将

```cpp
void random_init_thread(int thread_num)
{
    // nothing should be modifying the global engine now
    sApp->random_engine->seedThread(rng_for_thread(thread_num), mixin_from_device_info(thread_num));
}
```

改为

```cpp
void random_init_thread(int thread_num)
{
    // nothing should be modifying the global engine now
    sApp->random_engine->seedThread(rng_for_thread(thread_num), mixin_from_device_info(thread_num));
    // bind this worker's engine slot: test code running on this thread --
    // and any raw std::thread it spawns -- can never alias another
    // thread's slot (issue #348)
    tls_engine = rng_for_thread(thread_num);
}
```

- [x] **Step 2.5: 编译检查（快速反馈，完整构建在 Task 3）**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh 2>&1 | tail -5
```

预期：构建成功，零新警告（对照 Task 1 Step 1.1 记录）。

### Task 3: 端到端验证 100% real

- [x] **Step 3.1: 干净重建**

```bash
rm -rf builddir && PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
SDC_BUILD_JOBS=20 ./scripts/build.sh 2>&1 | tee /tmp/post_build.log
grep -ci "warning" /tmp/post_build.log
```

预期：成功；警告数 ≤ 基线（零新警告）。

- [x] **Step 3.2: `--list-tests` 计数一致**

```bash
./builddir/sdcshield --list-tests | wc -l
```

预期：== N_baseline。

- [x] **Step 3.3: 框架线程 RNG 流 bit 级一致（核心非回归证据）**

```bash
./builddir/sdcshield --quality=-1 -s LCG:1234 -e selftest_logs_random_init -t 100 -n 1 > /tmp/post_init.log 2>&1
./builddir/sdcshield --quality=-1 -s LCG:1234 -e selftest_logs_random -t 100 -n 4 > /tmp/post_run.log 2>&1
diff <(grep -E "[0-9]+ [0-9]+ [0-9]+ [0-9]+" /tmp/base_init.log) <(grep -E "[0-9]+ [0-9]+ [0-9]+ [0-9]+" /tmp/post_init.log)
diff <(grep -E "[0-9]+ [0-9]+ [0-9]+ [0-9]+" /tmp/base_run.log) <(grep -E "[0-9]+ [0-9]+ [0-9]+ [0-9]+" /tmp/post_run.log)
```

预期：两个 diff 均为空——init 线程（-1 槽）与 worker（1..N 槽）的 RNG 流修复前后 bit 级一致（`-s` 可重放性未破坏的直接证据）。

- [x] **Step 3.4: 随机性敏感测试非回归**

```bash
./builddir/sdcshield -s LCG:1234 -e zstd19 -t 3000 -n 1 > /tmp/post_zstd.log 2>&1; echo "exit=$?"
./builddir/sdcshield -s LCG:1234 -e random_access_sweep -t 3000 -n 1 > /tmp/post_ras.log 2>&1; echo "exit=$?"
diff /tmp/base_zstd.log /tmp/post_zstd.log && diff /tmp/base_ras.log /tmp/post_ras.log
```

预期：exit=0；zstd19/random_access_sweep 日志与基线一致（若日志含时间戳等非确定行，diff 其可稳定子集并在汇报中如实说明比对口径）。cache_stress_aggressor：pass 状态保持，victim 取值路径变化是**预期内**（victim 从共享竞争态变为独立惰性引擎流），如实分开记录。

- [x] **Step 3.5: 竞争消除实证 A——TSan**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh -C builddir-tsan
./builddir-tsan/sdcshield -s LCG:1234 -e cache_stress_aggressor -t 30 -n 4 > /tmp/post_tsan.log 2>&1; echo "exit=$?"
grep -c "WARNING: ThreadSanitizer" /tmp/post_tsan.log
```

预期：exit=0，`WARNING: ThreadSanitizer` 计数 0（基线 Step 1.5 的 race 报告消失）。

- [x] **Step 3.6: 竞争消除实证 B——全核长跑**

```bash
./builddir/sdcshield -s LCG:1234 -e cache_stress_aggressor -t 180 > /tmp/post_csa_full.log 2>&1; echo "exit=$?"
grep -i "fail\|mismatch" /tmp/post_csa_full.log | head
```

预期：exit=0（pass），零 mismatch（128 victim 各自独立引擎 + 独立 victim 槽）。

- [x] **Step 3.7: 标准回归（CLAUDE.md rule 2c）**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -3
```

预期：`exit: pass`。

- [x] **Step 3.8: x86-64 非回归（inspection，rule 2d）**

逐条核对并在 PR 正文论证：改动纯 C++ TLS 逻辑，无 `#ifarch`；框架线程槽位映射不变；x86 树内无裸线程 RNG 用户（cache_stress_aggressor victim 在 `#ifdef __aarch64__` 内）；AES/LCG/Constant 引擎代码零改动；`thread_num` 声明与初始化零改动。

### Task 4: 文档同步 + 提交 + PR + 检视 + merge

- [x] **Step 4.1: docs/architecture.md RNG 章节补契约**

在 "RNG" 段（`framework/random.cpp:781-883` 描述句之后）补：

> Threads spawned by test code with a raw `std::thread` never run the
> framework's init paths; their `thread_num` TLS is zero-initialized to 0
> which used to alias worker 0's slot (`rngs[1]`) — a data race when several
> such threads call the RNG (issue #348). Since 2026-10-31 they get a
> private, lazily-seeded `thread_local` engine instead (mixin = fixed salt ^
> process-wide counter, seeded from the global engine state, so streams still
> depend on `-s`). Caveat: which raw thread gets which counter value depends
> on scheduling, so per-raw-thread replay across runs is not guaranteed;
> framework-thread streams remain bit-exact under `-s`.

- [x] **Step 4.2: cache_stress_aggressor.cpp victim 注释更新**

`victim_thread` 注释（:47-48）"框架 RNG 按线程独立流" 补引用：

```cpp
    // 每迭代重掷随机浮点数向量（框架 RNG 按线程独立流——含裸 std::thread：
    // framework/random.cpp 为未走框架初始化的线程惰性播种私有引擎，issue #348；
    // frandomf_scale 返回 [0, scale)，取 [-1, 1) 区间做跨符号值）
```

- [x] **Step 4.3: 提交**

```bash
git branch --show-current        # 必须是 worktree feature 分支，绝不在 main
git add framework/random.cpp docs/architecture.md tests/cpu/cache/cache_stress_aggressor.cpp docs/superpowers/plans/2026-10-31-issue348-raw-thread-rng.md
git commit -s -m "fix(framework): raw std::thread RNG 用户改用惰性私有引擎，消除对 worker-0 槽的竞争

裸 std::thread 的 thread_num TLS 零初始化为 0，thread_local_rng() 落到
rngs[1]（框架 worker 0 的槽）：cache_stress_aggressor 全核跑时最多 128 个
victim 对同一引擎状态并发非原子 RMW = C++ UB（issue #348）。取值域安全
（mantissa 掩码）不产生假阳性，但随机性退化且 -s 对 victim 不可重放。

修复：每线程引擎指针 TLS（tls_engine）。框架线程在既有 init 路径绑定到与
之前完全相同的槽（random_init_global / random_init_thread / thread_num==-1
首用回退）——框架线程 RNG 流 bit 级不变（selftest_logs_random* 同 seed
修复前后 diff 为空实证）；未绑定线程（裸 std::thread）惰性播种私有
thread_local 引擎，mixin = 固定盐 ^ 进程级原子计数器，播种素材取当前全局
引擎状态（-s 依赖性保持）。x86-64 共享此代码且树内无裸线程 RNG 用户，
行为零变化。

验证：TSan 基线报 rngs[1] write-write race → 修复后 0 报告；
cache_stress_aggressor 全核 180s pass；zstd19 -t 2000 -n 1 exit: pass；
--list-tests 计数与基线一致。

Fixes #348"
git push -u origin worktree-agent-a6aee994d5746865f    # 以实际分支名为准
```

- [ ] **Step 4.4: PR + code-reviewer 检视循环**

```bash
gh pr create --base main --title "fix(framework): raw std::thread RNG 用户改用惰性私有引擎 (issue #348)" --body <正文：选型论证 + 验证证据 + Fixes #348>
```

Agent 工具派 `feature-dev:code-reviewer` 批判性检视（重点攻击：并发正确性、TLS 生命周期/fork 交互、x86 语义、热路径性能）。blocking 发现 → 修复 → push → 循环至无 blocking。

- [ ] **Step 4.5: merge + issue 收尾**

```bash
gh pr merge <PR号> --merge
gh issue view 348   # 确认 Fixes #348 自动关闭
```

## Self-Review

- Spec 覆盖：issue 的三个修复方向已评估并论证选型（上）；"关键位置"四个文件全部核对；影响四条（UB/域安全/质量/重放）逐条对应（UB→TSan 证据；域安全→不变式保持；质量→独立流；重放→框架线程 bit 级 + 裸线程诚实边界）。
- 占位符扫描：无 TBD/TODO；所有代码步骤含完整代码。
- 类型一致性：`tls_engine`（`thread_rng*`）、`raw_thread_engine()`（返回 `thread_rng*`）、`kRawThreadMixinSalt`（`uint32_t`）在 Task 2/3/4 引用一致；`thread_local_rng()` 签名不变。
- 残余风险与诚实边界：裸线程逐线程跨运行重放不保证（已声明）；TSan 若环境不可行按 Step 1.5 预案如实记录。

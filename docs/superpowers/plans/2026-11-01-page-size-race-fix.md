# 2026-11-01 sandstone_page_size() 竞态与 -1 永久缓存修复（issue #399）

**Goal:** 消除 `sandstone_page_size()` 两份实现中的无同步 static 惰性缓存——
并发首调 data race（形式 UB）与 sysconf 失败值 -1 永久缓存（转型
4294967295 喂给 ROUND_UP_TO 位掩码）。

**Architecture:** 去掉全部缓存状态，改为每次调用 `sysconf(_SC_PAGESIZE)`；
非正值（POSIX 允许的 -1）回退历史缺省 4096，绝不把 `(unsigned)-1` 喂进掩码。
无状态 = 竞态按构造消除，无初始化顺序依赖（对比「启动期常量」方案：malloc
interposition 的 `pvalloc` 理论上可于 main 之前被静态初始化器调用，全局常量
届时未初始化，方案劣于 per-call）。

**Tech Stack:** C17/C++20（两份头文件必须同时保持 C 兼容——5 个 C TU 包含
sandstone_p.h，排除 C++ magic static 方案）；clang-17 `-fsanitize=thread`
（GCC 12 的 libtsan 运行库本机缺失）；复现器为 /tmp 一次性程序，不入库。

**Spec:** issue #399（audit：framework/sandstone_p.h:88 与
framework/sandstone_test_utils.h:29-35 两份可漂移副本）。

## Global Constraints

- 一个补丁一个单元：commit 1 = 计划文档 + 旧计划更正注记（docs）；commit 2 =
  代码修复 + 单测。`git commit -s`，DCO 最后一行。
- x86-64 非回归：改动为架构中立 framework 代码，4K 页宿主上
  `sysconf(_SC_PAGESIZE)` 前后都返回 4096，行为逐位等价（检视声明）。
- 构建零新警告；unittests 全过；回归 `-e zstd19 -t 2000 -n 1` → exit: pass。
- Windows 不在范围（沿袭 2026-10-28 计划的用户裁定：只考虑 Linux）。

## 根因（Phase 1 结论）

1f06467e（2026-10-28）引入的实现：
```c
static long cached = 0;
if (!cached) cached = sysconf(_SC_PAGESIZE);
return (unsigned)cached;
```
- **data race**：多线程首调并发写非原子 `cached`，C++11 [intro.races] /
  C11 5.1.2.4 下即 UB，与「所有写入者写同值」无关——旧计划
  「竞态良性（所有写入者写同值）」论断不成立，须更正。实际暴露面：主二进制
  在 main 线程 init_shmem()（sandstone.cpp:315 起）即完成首调，fork 子进程
  继承已初始化缓存，当前无活调度能踩中；但 `pvalloc` interposition 可被
  任意线程任意时刻调用，任何未来早于 main 首调的调用点（静态初始化器、库
  ctor 线程）都会实际竞态。unittests 二进制（test_utils 副本）当前单线程
  调用，同样只是「暂未踩中」。
- **-1 永久缓存**：sysconf 失败返回 -1 时 `!cached` 为假 → 永久缓存 →
  `(unsigned)-1` = 4294967295 → `ROUND_UP_TO(v, 0xFFFFFFFF)` 的掩码
  `~(n-1)` = 1，对齐结果退化为奇偶位垃圾。glibc（读 dl_pagesize）与
  musl（编译期常量）上 `_SC_PAGESIZE` 实际不会失败，故为健壮性缺陷；
  回退 4096 = 1f06467e 之前的历史行为，严格不劣于旧代码。

## 开销论证（per-call sysconf）

全部调用点均为冷路径：shmem 初始化（sandstone.cpp）、线程栈 GuardSize
（sandstone_thread.cpp，每线程属性构造一次）、日志 mmap（logging.cpp，
每日志文件一次）、`pvalloc` interposition（遗留稀有 API）、
ManagedCodeBuffer::allocate（单测）。无热循环调用点（grep
ROUND_UP_TO_PAGE 全量核对，tests/ 零命中）。glibc `sysconf(_SC_PAGESIZE)`
= `__getpagesize()` = 读 `GLRO(dl_pagesize)`，无 syscall；issue 本身认定
「vdso 级开销」。

---

### Task 1: 复现器（先失败，/tmp 一次性，不入库）

**Files:**
- Create: `/tmp/repro_race.cpp`（旧实现形态 + N+1 线程双屏障循环）
- Create: `/tmp/repro_neg1.c`（interpose sysconf，armed 门控失败一次）

**Step 1: 写 data race 复现器并跑 TSAN，验证旧实现报 race**

```cpp
// /tmp/repro_race.cpp —— issue #399 复现：无同步 static 惰性缓存
#include <pthread.h>
#include <unistd.h>
#include <stdio.h>

static long cached = 0;   /* 缺陷本体：无同步缓存（文件域以便轮次间复位） */

static inline unsigned sandstone_page_size_old(void)
{
    if (!cached)
        cached = sysconf(_SC_PAGESIZE);
    return (unsigned)cached;
}

enum { NTHREADS = 8, ROUNDS = 2000 };
static pthread_barrier_t start_barrier, end_barrier;

static void *worker(void *)
{
    for (int r = 0; r < ROUNDS; ++r) {
        pthread_barrier_wait(&start_barrier);
        (void)sandstone_page_size_old();
        pthread_barrier_wait(&end_barrier);
    }
    return nullptr;
}

int main()
{
    pthread_barrier_init(&start_barrier, nullptr, NTHREADS + 1);
    pthread_barrier_init(&end_barrier, nullptr, NTHREADS + 1);
    pthread_t t[NTHREADS];
    for (int i = 0; i < NTHREADS; ++i)
        pthread_create(&t[i], nullptr, worker, nullptr);
    for (int r = 0; r < ROUNDS; ++r) {
        cached = 0;                       /* 单线程复位，端屏障保证有序 */
        pthread_barrier_wait(&start_barrier);
        (void)sandstone_page_size_old();  /* 与 8 个 worker 并发首调 */
        pthread_barrier_wait(&end_barrier);
    }
    for (int i = 0; i < NTHREADS; ++i)
        pthread_join(t[i], nullptr);
    printf("done, cached=%ld\n", cached);
    return 0;
}
```

Run: `clang++ -O2 -fsanitize=thread -pthread /tmp/repro_race.cpp -o /tmp/repro_race && TSAN_OPTIONS=halt_on_error=1 /tmp/repro_race`
Expected（修复前）: stderr 含 `WARNING: ThreadSanitizer: data race`，
racy write/read 于 `cached`，进程非零退出。

**Step 2: 写 -1 永久缓存复现器，验证旧实现返回 4294967295 且不恢复**

```c
/* /tmp/repro_neg1.c —— issue #399 复现：sysconf 失败值 -1 永久缓存 */
#include <unistd.h>
#include <stdio.h>

static int armed = 0;
long sysconf(int name)            /* interpose：仅 armed 时失败 */
{
    if (name == _SC_PAGESIZE && armed)
        return -1;
    return 4096;
}

static inline unsigned sandstone_page_size_old(void)
{
    static long cached = 0;
    if (!cached)
        cached = sysconf(_SC_PAGESIZE);
    return (unsigned)cached;
}

int main(void)
{
    armed = 1;
    unsigned first = sandstone_page_size_old();  /* sysconf 失败 -> 缓存 -1 */
    armed = 0;
    unsigned second = sandstone_page_size_old(); /* sysconf 已恢复 4096 */
    printf("first=%u second=%u\n", first, second);
    return 0;
}
```

Run: `gcc -O2 /tmp/repro_neg1.c -o /tmp/repro_neg1 && /tmp/repro_neg1`
Expected（修复前）: `first=4294967295 second=4294967295`（失败值永久缓存）。

### Task 2: 计划文档 + 旧计划更正（commit 1，docs）

**Files:**
- Create: 本文档
- Modify: `docs/superpowers/plans/2026-10-28-page-size-guard-fix.md`
  「安全性论证」节「竞态良性（所有写入者写同值）」处加 2026-11-01 更正
  注记，指向本文档与 issue #399。

**Step:** 写入两文档后 `git add docs/ && git commit -s`，标题
`docs(plans): sandstone_page_size 竞态修复计划 + 旧计划「竞态良性」更正 (issue #399)`。

### Task 3: 代码修复 + 单测（commit 2）

**Files:**
- Modify: `framework/sandstone_p.h:75-95`（实现 + 头注释）
- Modify: `framework/sandstone_test_utils.h:29-35`（同款修复，保持该头
  C++ 风格；与 sandstone_p.h 的复制关系是单测隔离的既有设计，本修复不
  去重——去重属结构重构，非本缺陷单元）
- Modify: `framework/unit-tests/sandstone_test_utils_tests.cpp`
  （x86-64 守卫之外补 PageSize 单测，履行该头「入内必单测」契约）

**Step 1: sandstone_p.h 新实现（C/C++ 双兼容）**

```c
/*
 * Page size used for mmap/mprotect alignment.  MUST match the kernel's actual
 * page size, otherwise mprotect() will round to the kernel's page boundary and
 * may make adjacent data read-only (causing SIGSEGV on kernels with >4 KB
 * pages, e.g. ARM64 16 KB / 64 KB configurations).  Resolved per call via
 * sysconf(_SC_PAGESIZE), with no function-level static cache: concurrent
 * first calls cannot race (issue #399) and a sysconf failure is never
 * permanently cached.  All call sites are cold paths, so the per-call cost
 * (glibc reads the dynamic loader's dl_pagesize, no syscall) is irrelevant.
 */
#ifdef __cplusplus
extern "C++"
{
#endif
static inline unsigned sandstone_page_size(void)
{
    long pg = sysconf(_SC_PAGESIZE);
    /* POSIX permits -1; on Linux it cannot happen (glibc: dl_pagesize load,
     * musl: build-time constant).  Fall back to the historical 4 KB default
     * rather than feeding (unsigned)-1 into the ROUND_UP_TO bitmask. */
    return pg > 0 ? (unsigned)pg : 4096;
}
#ifdef __cplusplus
}
#endif
```

**Step 2: sandstone_test_utils.h 同款（保持 `unsigned(pg)` 风格）**

```c
static inline unsigned sandstone_page_size()
{
    long pg = sysconf(_SC_PAGESIZE);
    /* No static cache: race-free by construction and a sysconf failure is
     * not permanently cached (issue #399).  POSIX permits -1; on Linux it
     * cannot happen, so the 4096 fallback just keeps the historical default
     * instead of feeding (unsigned)-1 into the ROUND_UP_TO bitmask. */
    return pg > 0 ? unsigned(pg) : 4096;
}
```

**Step 3: 单测（置于 `#endif // __x86_64__` 之后、WeightedPicker 段之前）**

```cpp
// ======================================================================
// Page Size Tests
// ======================================================================
TEST(PageSizeTest, ReturnsPositivePowerOfTwo)
{
    /* Host-dependent (ARM64: 4K/16K/64K), so assert the invariants that
     * ROUND_UP_TO_PAGE relies on rather than a specific value. */
    unsigned pg = sandstone_page_size();
    EXPECT_GT(pg, 0u);
    EXPECT_EQ(pg & (pg - 1), 0u) << "page size must be a power of two";
}
```

**Step 4: 复现器转绿**

- `/tmp/repro_race.cpp` 函数体替换为新实现（去掉 `cached` 与复位行），
  同命令重跑 → 无 TSAN 报告、`done` 正常退出 0。
- `/tmp/repro_neg1.c` 同替换 → `first=4096 second=4096`（回退生效、无
  永久缓存）。

**Step 5: 构建 + 单测 + 回归（rule 2）**

- `./scripts/build.sh`（既有 builddir）→ 零新警告（修复前先建基线）。
- 跑 unittests 二进制 → 含新 PageSize 用例全过。
- `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`；
  `-e mce_check` → pass（每跑必经 shmem 初始化与线程栈 GuardSize，即
  受影响行为的实跑证据）。
- x86-64 非回归：检视声明（架构中立，4K 页行为等价）。

**Step 6: 提交** `git commit -s`，标题
`fix(framework): sandstone_page_size 去 static 缓存——并发竞态与 sysconf -1 永久缓存修复 (issue #399)`。

### Task 4: 推送 / PR / 报告

push `fix/issue-399` → `gh pr create --base main`（正文含 `Closes #399` +
根因/改动/验证证据）→ 报告贴 issue #399 → 工作区回干净 main。

## 自审

- 覆盖 issue 三要素：竞态（Task 1/3）、-1 永久缓存（Task 1/3）、两副本
  漂移（Task 3 同步修两份；去重裁定为超出本单元，报告说明）。
- 无占位符；类型一致（`unsigned` 返回、`long pg` 中间量两份一致）。
- 验证边界诚实：本机 4K 页，>4K 页行为无本机可执行路径（沿袭 1f06467e
  的检视论证边界）；data race 的「修复后无报告」由 TSAN 复现器实证。

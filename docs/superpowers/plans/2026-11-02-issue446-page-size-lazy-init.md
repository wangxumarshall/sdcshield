# Issue #446 修复计划:`sandstone_page_size()` lazy-init 数据竞争 + sysconf 失败值永久缓存

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 `sandstone_page_size()`(两处孪生副本)lazy-init 的无同步数据竞争(标准 UB),并使 `sysconf(_SC_PAGESIZE)` 失败值(-1)不再被永久缓存成 `(unsigned)-1` 参与 `ROUND_UP_TO` 位运算,失败路径 fail-loudly(带诊断 + abort)。

**Architecture:** 原子暂存式 memoization:`__atomic_load_n/__ATOMIC_ACQUIRE` 读缓存 → miss 时本地变量调 `sysconf` → 校验(>0 且 2 的幂)→ 校验失败走 raw-fd 诊断 + abort(仿 `sysdeps/linux/malloc.cpp` `null_pointer_consumption` 先例,此时 logging 未必已初始化)→ 成功才 `__atomic_store_n/__ATOMIC_RELEASE` 写回。并发首调可能各自调用 `sysconf` 并写回**同值**——原子写 + 幂等初始化,标准意义上的竞态消除;每次返回值都经过本线程校验。两处孪生副本同步修改(不去重——`sandstone_test_utils.h:25-27` 明示复制是刻意决策:避免单测拖入 sandstone_p.h 依赖)。

**Tech Stack:** C/C++ 共通层(纯 GCC/Clang 内建 `__atomic_*`,仓库先例 `sandstone_run.cpp:1248`;`sandstone_p.h` 被 4 个真实 `.c` 包含,禁用 C++-only 手法);gtest 单测。

**Spec:** issue #446(audit:无同步竞态 + sysconf 失败值 -1 永久缓存,`framework/sandstone_p.h:86-92` 与 `framework/sandstone_test_utils.h:29-35` 同款)。

## 已复现证据(修复前提,2026-11-02 实测)

- (a) 竞态:TSan 复现器(含 `framework/sandstone_test_utils.h` 原文,8 线程并发首调),
  `clang++ -fsanitize=thread` 运行:
  `WARNING: ThreadSanitizer: data race` — `Read of size 8 ... sandstone_test_utils.h:32`(reader)/`Previous write ... :33`(writer,`cached = sysconf(...)`)。
- (b) 失败值缓存:`LD_PRELOAD` 假 `sysconf`(仅 `_SC_PAGESIZE` 返回 -1,其余 `dlsym(RTLD_NEXT)` 转发),
  跑真实 `builddir/sdcshield --list-tests` →
  `internal error: could not map the shared memory file to memory: Invalid argument`,真实 exit=73,
  崩点 `sandstone.cpp:350`(shmem mmap;`ROUND_UP_TO_PAGE` 退化为 `&1` → 尺寸 0 → EINVAL)——
  报错完全无页大小/sysconf 诊断 = issue 所述「静默灾难」。

## 全局约束

- 一个补丁一个单元:本计划 = 单一缺陷(issue #446)单一 commit,两处孪生副本同修属同一单元。
- `sandstone_p.h` 的修改必须同时合法于 C 与 C++(被 `sysdeps/linux/msr.c`、`sysdeps/unix/tmpfile.c`、`sysdeps/generic/{physicaladdress,memfpt}.c` 包含)。
- `sandstone_test_utils.h` 是 clean file:改动必须配单测(`framework/unit-tests/sandstone_test_utils_tests.cpp`)。
- 构建零新警告(`./scripts/build.sh`,unittests 目标也吃 `default_cpp_warn`)。
- 不改 x86-64 专属路径;两处头文件本为跨架构共享代码(无 `#ifdef __x86_64__`),同构修复对两架构等价生效,x86 非回归 = 好路径行为逐位不变(原子暂存 + 校验在成功路径上不改变返回值)。
- 不去重两副本、不重构 `ManagedCodeBuffer` 等(超范围)。

---

### Task 1: 修复两处孪生副本

**Files:**
- Modify: `framework/sandstone_p.h:71-96`
- Modify: `framework/sandstone_test_utils.h:25-36`

**Interfaces:**
- Produces: `static inline bool sandstone_is_valid_page_size(long page_size)`(纯谓词:正且 2 的幂);
  `static __attribute__((unused, noinline, noreturn)) void sandstone_page_size_fatal(long page_size)`(stderr 诊断 + abort,纯 C89);
  `sandstone_page_size()` 签名不变(`static inline unsigned (void)`),返回值语义不变(合法页大小)。

**与 PR #453(issue #444)的组合性**:#453 给同一函数加 `_WIN32` 分支(getpagesize)、POSIX 臂逐字保留(即保留本 issue 待修的竞态体)。本修复重写 POSIX 臂;两 PR 文本冲突、语义干净,后合入者 rebase(rebase 时 #453 的「POSIX 臂逐字不变」表述需同步更新)。本修复的 fatal 助手为纯 C89(stdio/stdlib),对 MinGW 零新增 POSIX 依赖,不扩大 #444 的守卫面。

- [x] **Step 1: 改 `framework/sandstone_p.h`**(C/C++ 双合法;放在现有 page-size 注释块内、`ROUND_UP_TO_PAGE` 宏之前)

```c
/* align to a power of 2 */
#define ROUND_UP_TO(value, n)       (((value) + (n) - 1) & (~((n) - 1)))
#include <unistd.h>
#include <stdio.h>      // fprintf(stderr, ...) —— 新增(其余不变)
#include <stdlib.h>     // abort() —— 新增

/*
 * Page size used for mmap/mprotect alignment.  MUST match the kernel's actual
 * page size, ... (原注释保留,追加一句)Resolve once via
 * sysconf(_SC_PAGESIZE) and cache it.
 */
static inline bool sandstone_is_valid_page_size(long page_size)
{
    return page_size > 0 && (page_size & (page_size - 1)) == 0;
}

/* There is no safe fallback page size (a hardcoded one breaks kernels with
 * bigger pages, see the comment above) and logging may not be initialized
 * yet, so write the diagnosis to stderr directly and abort — like
 * malloc.cpp's null_pointer_consumption().  Fail loudly, never cache garbage.
 * 纯 C89（stdio/stdlib）——对 MinGW（issue #444 正在修的平台）零新增
 * POSIX 依赖；不用 raw write(STDERR_FILENO)，避免扩大 #444 的守卫面。 */
static __attribute__((unused, noinline, noreturn)) void sandstone_page_size_fatal(long page_size)
{
    fprintf(stderr, "internal error: unusable page size from sysconf(_SC_PAGESIZE): 0x%lx\n",
            (unsigned long)page_size);
    abort();
}

static inline unsigned sandstone_page_size(void)
{
    static long cached = 0;
    long page_size = __atomic_load_n(&cached, __ATOMIC_ACQUIRE);
    if (__builtin_expect(!page_size, false)) {
        page_size = sysconf(_SC_PAGESIZE);
        if (__builtin_expect(!sandstone_is_valid_page_size(page_size), false))
            sandstone_page_size_fatal(page_size);
        __atomic_store_n(&cached, page_size, __ATOMIC_RELEASE);
    }
    return (unsigned)page_size;
}
```

- [x] **Step 2: `framework/sandstone_test_utils.h` 同款修改**(加 `<stdio.h>` `<stdlib.h>` 两个 include;函数体与 p.h 逐字一致,保持孪生)

- [x] **Step 3: 构建**(实测:`ninja -C builddir -t clean` 全量 624 目标 + `./scripts/build.sh` + `ninja unittests` 21 目标,exit 0,grep warning 计 0)

### Task 2: 单测(clean file 要求)

**Files:**
- Modify: `framework/unit-tests/sandstone_test_utils_tests.cpp`(文件末尾,`#ifdef __x86_64__` 之外的公共区)

**Interfaces:**
- Consumes: Task 1 的 `sandstone_is_valid_page_size` / `sandstone_page_size` / `sandstone_page_size_fatal`(static于本 TU,直接 include 头文件可见)。

- [x] **Step 1: 新增 gtest 测试**(值正确性、谓词纯逻辑、并发一致性、fail-loudly 死亡测试;用 `std::thread`,unittests 已链 `pthread_dep`)

```cpp
TEST(PageSize, ReturnsPositivePowerOfTwoMatchingSysconf) {
    unsigned ps = sandstone_page_size();
    EXPECT_GT(ps, 0u);
    EXPECT_EQ(ps & (ps - 1), 0u);
    EXPECT_EQ(ps, static_cast<unsigned>(sysconf(_SC_PAGESIZE)));
}
TEST(PageSize, ValidPageSizesAccepted) {
    EXPECT_TRUE(sandstone_is_valid_page_size(1));
    EXPECT_TRUE(sandstone_is_valid_page_size(4096));
    EXPECT_TRUE(sandstone_is_valid_page_size(65536));
}
TEST(PageSize, InvalidPageSizesRejected) {
    EXPECT_FALSE(sandstone_is_valid_page_size(0));
    EXPECT_FALSE(sandstone_is_valid_page_size(-1));           // sysconf 失败值
    EXPECT_FALSE(sandstone_is_valid_page_size(4097));          // 非 2 的幂
    EXPECT_FALSE(sandstone_is_valid_page_size(0xFFFFFFFF));   // (unsigned)-1
}
TEST(PageSize, FatalOnUnusablePageSize) {
    EXPECT_DEATH(sandstone_page_size_fatal(-1), "unusable page size");
}
TEST(PageSize, ConcurrentCallsAgree) {
    std::vector<std::thread> threads;
    std::array<unsigned, 8> results{};
    for (auto &r : results) threads.emplace_back([&r]{ r = sandstone_page_size(); });
    for (auto &t : threads) t.join();
    for (unsigned r : results) { EXPECT_EQ(r, results[0]); EXPECT_GT(r, 0u); }
}
```
(需 `#include <thread>` `<array>`;文件已含 `<vector>`。)

- [x] **Step 2: 跑单测**(实测:`PageSize.*` 5/5 PASSED 含死亡测试;CI 原样过滤器 `--gtest_filter=-LinuxThermalFixture.CurrentMachine_TestUsingSingletomTemperaturesOnCurrentMachine` → 109 tests PASSED,exit 0;本机 thermal CurrentMachine 用例因缺 `thermal_zone*` 既有失败,CI 同款过滤已排除)

### Task 3: 复现器回归验证(修复后必须全部翻转)

- [x] TSan 复现器重编重跑(clang++ 17 -fsanitize=thread,指向工作区头文件)→ **无** data race 报告,干净输出 `page size = 4096`,exit 0
- [x] `LD_PRELOAD` 假 sysconf 跑 `builddir/sdcshield --list-tests`(实测)→ 输出 `internal error: unusable page size from sysconf(_SC_PAGESIZE): 0xffffffffffffffff` + abort(真实 exit=134),不再是修复前的误导性 `could not map the shared memory file to memory: Invalid argument`(真实 exit=73)
- [x] 回归:`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`(实测)
- [x] 快速健全(实测):`--list-tests` 修复前 387 = 修复后 387(stash A/B 同机对照);CI selftest 判定循环复刻——positive 11/11 exit 0,failinit/logerror 预期 rc=1

### Task 4: 提交/推送/PR/报告

- [x] `git checkout -B fix/issue-446 origin/main`;`git commit -s`(Signed-off-by 最后一行)
- [x] push `fix/issue-446`;`gh pr create --base main --head fix/issue-446 --title "fix: sandstone_page_size() lazy-init 数据竞争与 sysconf 失败值永久缓存 (#446)"(含 Closes #446 + 报告);被 auto-pr 抢先则 `gh pr edit fix/issue-446`
- [ ] 报告(方案/根因、改动、验证证据、PR 链接)贴 issue #446;结束回 main 干净态

## Self-Review

- Spec 覆盖:(a) 竞态 → 原子暂存;(b) -1 缓存 → 校验前不落缓存 + fail-loudly。issue 两点全覆盖,无多余改动。
- 无占位符:所有代码块完整可落地。
- 命名一致:`sandstone_is_valid_page_size`/`sandstone_page_size_fatal` 在 Task 1 定义、Task 2 使用,签名一致。
- 已知边界:死亡测试的 stderr 正则只取纯文本子串(无正则元字符);fatal 助手纯 C89(fprintf/abort,stdio/stdlib),不新增 POSIX 依赖;`unused` 属性防 C TU 未引用告警。验证边界如实声明:bats sanity-check(3744 行)本机无 bats,由 CI 侧运行;gem5-fi 不适用——本缺陷与 VL/SVL/ISA 档位无关,全部受影响行为均已在真机实证(无芯片能力缺失路径)。

# sandstone_page_size() 去重（issue #451）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `sandstone_page_size()` / `ROUND_UP_TO` / `ROUND_UP_TO_PAGE` 收敛到唯一的轻量权威头 `framework/sandstone_page.h`，`sandstone_p.h` 与 `sandstone_test_utils.h` 改为包含它，消除无护栏的函数双实现。

**Architecture:** 提交 1f06467e 页大小运行时化时把 `sandstone_page_size()` 函数体在两个头各复制一份（原先只是文本相同的宏复制，编译器对宏文本漂移会告警；函数化后两份各自演化零护栏）。`sandstone_test_utils.h:25-27` 头注释本身写明了既定方向：把这类简单宏从 sandstone_p.h 抽出以打破单测依赖。方案即落实该方向：新建零依赖（仅 `<unistd.h>`）、C/C++ 双兼容的最小头承载唯一实现；两头原位替换为 `#include "sandstone_page.h"`。展开后与现状 100% 文本等价（宏、函数体、`extern "C++"` 包裹逐字保留 sandstone_p.h 版本），行为零变化。

**Tech Stack:** C17/C++20（GCC-10）、Meson、GoogleTest（unittests target）。

**Spec:** issue #451（audit 报告：framework/sandstone_p.h:86-92 与 framework/sandstone_test_utils.h:29-35 双份 `static inline unsigned sandstone_page_size()`，单测验证影子副本，框架 16 个使用点走 sandstone_p.h 份，演化漂移无护栏）。

## Global Constraints

- 一个补丁一个单元；`git commit -s`（DCO 最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`）；只推 `fix/issue-451`。
- 构建零新警告（`./scripts/build.sh`）；受影响行为真实运行引用输出；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`；x86-64 非回归如实论证。
- C/C++ 双兼容：5 个 C TU（sysdeps/{generic/memfpt.c, generic/physicaladdress.c, unix/tmpfile.c, linux/msr.c, windows/tmpfile.c}）包含 sandstone_p.h。
- 不触碰 `__x86_64__` 专属路径；改动为纯结构重构。
- meson 无需改动：仓库不注册普通头文件（sandstone_p.h/sandstone_test_utils.h 均不在任何 meson 列表），`""` 包含走同目录查找。

---

### Task 1: 复现——实证「零护栏 + 单测验证影子副本」

**Files:**
- 仅临时破坏 `framework/sandstone_p.h`（实验后 `git checkout -- framework/sandstone_p.h` 还原，不提交）

**Interfaces:**
- Consumes: 现状两份函数体。
- Produces: 缺陷实证记录（进 issue 报告与提交说明）：破坏框架份后构建+单测全绿 = 单测只能验证 test_utils 份。

- [ ] **Step 1: 文本比对确认两份已经文本漂移**

`sandstone_p.h:91` 为 `return (unsigned)cached;`，`sandstone_test_utils.h:34` 为 `return unsigned(cached);`（C 风格 cast vs C++ 函数式 cast）——「静默文本漂移」已发生，尚无语义分叉。

- [ ] **Step 2: 破坏实验 A——只破坏 sandstone_p.h 份**

把 `framework/sandstone_p.h` 中 `return (unsigned)cached;` 临时改为 `return (unsigned)cached * 2;`，然后：

```bash
./scripts/build.sh 2>&1 | tail -3        # 预期:构建成功,零告警(无任何护栏)
ninja -C builddir unittests 2>&1 | tail -3 && ./builddir/unittests 2>&1 | tail -3
# 预期:unittests 编译通过且全部 PASS——单测验证的是 test_utils 影子副本,框架份被破坏也全绿
```

- [ ] **Step 3: 还原**

```bash
git checkout -- framework/sandstone_p.h && git diff --stat   # 预期:空
```

记录三段真实输出进 issue 报告。ARM64 主机上 `CodeBufferTestFixture` 在 `#ifdef __x86_64__` 内编译出局，破坏 test_utils 份的运行时单测本机不可达——如实声明该验证边界（反向实验由 Task 3 的全量构建覆盖：改坏唯一实现必然被编译/单测捕获）。

### Task 2: 新建权威头 + 两头替换 + 单测护栏加固

**Files:**
- Create: `framework/sandstone_page.h`
- Modify: `framework/sandstone_p.h:71-96`（删本地三件套，原位换 include）
- Modify: `framework/sandstone_test_utils.h:20-36`（删本地三件套与孤立 include，换 include）
- Modify: `framework/unit-tests/sandstone_test_utils_tests.cpp`（新增 page-size 行为测试，全平台编译）

**Interfaces:**
- Consumes: 无（首个任务）。
- Produces: `framework/sandstone_page.h` 对外提供 `ROUND_UP_TO(value, n)`、`static inline unsigned sandstone_page_size(void)`、`ROUND_UP_TO_PAGE(value)`，与现状同名同语义；`sandstone_p.h` / `sandstone_test_utils.h` 经包含继续向下游提供同名宏与函数。

- [ ] **Step 1: 写失败/护栏测试（先于实现提交到工作区）**

在 `framework/unit-tests/sandstone_test_utils_tests.cpp` 的 `#endif // __x86_64__` 之后、WeightedPicker 段之前插入：

```cpp
// ======================================================================
// Page size tests -- the single canonical implementation lives in
// sandstone_page.h and reaches this file via sandstone_test_utils.h.
// These lock the contract the framework's 16 ROUND_UP_TO_PAGE users
// (mmap/mprotect alignment) rely on.  Runs on all architectures.
// ======================================================================
TEST(PageSizeTest, MatchesSysconf)
{
    EXPECT_EQ(sandstone_page_size(), static_cast<unsigned>(sysconf(_SC_PAGESIZE)));
}

TEST(PageSizeTest, IsPowerOfTwo)
{
    unsigned ps = sandstone_page_size();
    EXPECT_NE(ps, 0u);
    EXPECT_EQ(ps & (ps - 1), 0u);
}

TEST(PageSizeTest, RoundUpToPageRoundsUpToNextPage)
{
    unsigned ps = sandstone_page_size();
    EXPECT_EQ(ROUND_UP_TO_PAGE(1), ps);           // sub-page rounds up to one page
    EXPECT_EQ(ROUND_UP_TO_PAGE(ps), ps);         // page-aligned value is unchanged
    EXPECT_EQ(ROUND_UP_TO_PAGE(ps + 1), 2 * ps); // first byte of page 2
}
```

注：这是结构修复而非行为变更——测试在现状（影子副本逻辑本就正确）与修复后均应通过；其作用是把「唯一实现」的行为契约锁进单测，使未来唯一副本被改坏时本机（ARM64）可捕获。如实记录这一点，不宣称 TDD 红灯。

- [ ] **Step 2: 新建 `framework/sandstone_page.h`（逐字保留 sandstone_p.h 版函数体）**

```c
/*
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Minimal page-size helpers -- the single canonical home of ROUND_UP_TO,
 * sandstone_page_size() and ROUND_UP_TO_PAGE.
 *
 * Included by both sandstone_p.h (framework proper) and
 * sandstone_test_utils.h (unit-tested constructs), so the implementation
 * the unit tests exercise is byte-for-byte the one the framework's
 * mmap/mprotect users run.  Keep this header usable from C and C++ and
 * free of any include other than <unistd.h>.
 */

#ifndef INC_SANDSTONE_PAGE_H
#define INC_SANDSTONE_PAGE_H

#include <unistd.h>

/* align to a power of 2 */
#define ROUND_UP_TO(value, n)       (((value) + (n) - 1) & (~((n) - 1)))

/*
 * Page size used for mmap/mprotect alignment.  MUST match the kernel's actual
 * page size, otherwise mprotect() will round to the kernel's page boundary and
 * may make adjacent data read-only (causing SIGSEGV on kernels with >4 KB
 * pages, e.g. ARM64 16 KB / 64 KB configurations).  Resolve once via
 * sysconf(_SC_PAGESIZE) and cache it.
 */
#ifdef __cplusplus
extern "C++"
{
#endif
static inline unsigned sandstone_page_size(void)
{
    static long cached = 0;
    if (!cached)
        cached = sysconf(_SC_PAGESIZE);
    return (unsigned)cached;
}
#ifdef __cplusplus
}
#endif

#define ROUND_UP_TO_PAGE(value)     ROUND_UP_TO(value, sandstone_page_size())

#endif /* INC_SANDSTONE_PAGE_H */
```

- [ ] **Step 3: `framework/sandstone_p.h` 删本地三件套，原位换 include**

把 `sandstone_p.h:71-96`（`/* align to a power of 2 */` 注释、`ROUND_UP_TO` 宏、`#include <unistd.h>`、页大小说明注释、`extern "C++"` 函数、`ROUND_UP_TO_PAGE` 宏）整体替换为：

```c
/* Page-size helpers (ROUND_UP_TO / sandstone_page_size / ROUND_UP_TO_PAGE):
 * shared single implementation, see sandstone_page.h.  Included inside the
 * extern "C" block is safe -- the header is self-contained and gives the
 * helper C++ linkage via its own extern "C++" wrapper. */
#include "sandstone_page.h"
```

- [ ] **Step 4: `framework/sandstone_test_utils.h` 删本地三件套，换 include**

把 `sandstone_test_utils.h` 中 `#include <unistd.h>`（21 行，sysconf 的孤立依赖）与 25-36 行（copy 注释、`ROUND_UP_TO`、`sandstone_page_size`、`ROUND_UP_TO_PAGE`）替换为：

```cpp
#include "sandstone_page.h"
```

同目录 `""` 包含即可命中，无需 meson 改动（仓库不注册普通头）。

- [ ] **Step 5: 全量构建 + 单测**

```bash
./scripts/build.sh 2>&1 | tee /tmp/build451.log | tail -3
grep -iE "warning|error" /tmp/build451.log | grep -v "expected-memory" || echo "no new warnings"
ninja -C builddir unittests && ./builddir/unittests 2>&1 | tail -5
```

预期：构建零警告；unittests 全 PASS（含新增 3 个 PageSizeTest）。

### Task 3: 行为验证（CLAUDE.md 规则 2）与提交

**Files:**
- 提交 `framework/sandstone_page.h`、`framework/sandstone_p.h`、`framework/sandstone_test_utils.h`、`framework/unit-tests/sandstone_test_utils_tests.cpp`（单补丁单单元）

**Interfaces:**
- Consumes: Task 2 的唯一实现。
- Produces: `fix/issue-451` 分支上的单个 DCO 提交。

- [ ] **Step 1: 回归——受影响行为（mmap/mprotect 页对齐 + 线程栈 guard）**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1; echo "exit: $?"
# 契约:exit: pass
./builddir/sdcshield -e mce_check -n 1; echo "exit: $?" 2>/dev/null
# 1f06467e 同路径组合,作为受影响行为旁证
./builddir/sdcshield -e zstd19 -t 2000; echo "exit: $?"
# 全核(128 线程)动态 guard 栈分配实跑(线程栈 guard 走 sandstone_page_size)
```

- [ ] **Step 2: x86-64 非回归论证（如实声明）**

纯头文件结构重构：宏与函数体逐字保留、无任何 `__x86_64__` 专属分支被触碰；x86-64 TU 与 ARM64 TU 经同一包含链取得同一实现，4K 页主机行为 no-op。x86-64 为检视论证（本机 ARM64 无 x86 运行环境），不宣称实跑。

- [ ] **Step 3: 文档同步检查**

改动为 framework 内部头结构修复，不改变用户可见行为/CLI/测试集 —— README 与 docs/ 无对应条目需更新（非大颗粒度修改）。如 grep 发现文档提及该双实现或 ROUND_UP_TO 定义位置则一并更正。

- [ ] **Step 4: 提交**

```bash
git branch --show-current   # 须为 fix/issue-451
git add framework/sandstone_page.h framework/sandstone_p.h \
        framework/sandstone_test_utils.h \
        framework/unit-tests/sandstone_test_utils_tests.cpp \
        docs/superpowers/plans/2026-11-02-page-size-dedup.md
git commit -s -m "fix(framework): sandstone_page_size() 双实现收敛至唯一权威头 sandstone_page.h (#451)"
```

提交说明正文含：根因（1f06467e 函数体双复制，宏护栏失效）、复现证据（破坏实验 A 输出）、改动点、验证输出摘录。

- [ ] **Step 5: 推分支、开 PR、回帖 issue #451**

```bash
git push -u origin fix/issue-451
gh pr create --base main --head fix/issue-451 \
  --title "fix: sandstone_page_size() 收敛唯一实现 sandstone_page.h (#451)" \
  --body "<Closes #451 + 根因/改动/验证证据>"
# 兜底:auto-pr.yml 抢先则 gh pr edit fix/issue-451 --title … --body …
gh issue comment 451 --body "<方案/根因、改动、验证证据、PR 链接>"
git checkout main && git status   # 须 clean,回 main
```

## Self-Review

- 覆盖：issue 三要素（双实现、单测影子副本、16 使用点）分别由 Task 2 唯一头、Task 1 破坏实验、Task 3 回归覆盖。✓
- 占位符：无 TBD；所有代码块完整。✓
- 类型一致：`static inline unsigned sandstone_page_size(void)` 三处签名一致；`ROUND_UP_TO_PAGE` 语义不变。✓
- 风险点已核：5 个 C TU 包含 sandstone_p.h（新头 C 兼容）；meson 不注册普通头（无需改构建）；`ROUND_UP_TO` 非 PAGE 用法 3 处均经 sandstone_p.h 取得；删除两处孤立 `<unistd.h>` include 由全量构建兜底验证。

# 2026-11-01 线程栈 guard 吞错响亮化 + 机制勘误（issue #396）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `framework/sandstone_thread.cpp` 三处
`IGNORE_RETVAL(mprotect/madvise)` 静默吞错改为响亮 abort，并就地更正两处与
Linux 语义相反的「mprotect 向下取整」机制陈述（2026-10-28 计划文档 + 本文件
代码注释）。

**Architecture:** 失败处理沿用框架既有「`fprintf(stderr, ...)` + `abort()`」
致命内部错误惯例（sandstone_run.cpp:1073、logging.cpp:1480 同款）；errno
渲染用现成线程安全 `strerror_for_mmap()`（sandstone.h:533 声明，
selftest.cpp:1271 已有 "mprotect failed: %s" 同型先例）。勘误只改陈述，
不动已被 1f06467e 修对的代码几何。

**Spec:** issue #396（审计正文，2026-11-01）+ 本文档。

## Global Constraints

- 分支 `fix/issue-396`（从 `origin/main` 起），绝不推 main、绝不 merge。
- 一补丁一单元：计划文档 / 代码 / 旧计划勘误 = 3 个独立提交，`git
  commit -s`（DCO 最后一行，无 Co-Authored-By）。
- x86-64 非回归：改动全部位于 `#ifndef _WIN32` 的架构中立 framework
  代码；Windows 构建不受影响（helper 同置于 `#ifndef _WIN32`）。
- 大颗粒度文档同步：本计划 + 2026-10-28 计划勘误一并入 PR。

## 背景：审计论断逐条实证（2026-11-01，kernel 6.6.0-159.4.3.154，
aarch64，`getconf PAGESIZE` = 4096）

审计称：Linux mprotect(2) 对非页对齐 addr 返回 EINVAL（非向下取整），
真实故障链是 EINVAL 被 IGNORE_RETVAL 吞掉 → 栈滞留 PROT_NONE →
pthread_create explicit_bzero SIGSEGV；MADV 路径则是 guard 安装失败被吞、
线程间栈无 guard。以下为 /tmp/mprotect_semantics.c 实测输出（真机）：

```
Q1 mprotect(m+pg+pg/2, 1pg, RW) = -1 errno=Invalid argument
Q1 RESULT: FAILED -> kernel rejects misaligned start; no round-down
Q2 mprotect(m+pg, 2.5pg, PROT_NONE) = 0
Q2 write@intended_end+1(inside its page): SIGSEGV; write@next_page: ok
Q2 RESULT: end rounded UP (whole tail page protected)
Q3 old-code chain: mprotect(misaligned,RW)=-1 (swallowed), write into
   still-PROT_NONE stack region: SIGSEGV (silent — no prior error message)
Q4 madvise(m+pg+pg/2, MADV_DONTNEED) = -1 errno=Invalid argument
```

- **Q1**：非页对齐起点 → **EINVAL，无向下取整**——审计核心论断成立；
  计划文档 :10「mprotect 把可写区起点向下取整、越过 guard」与
  thread.cpp:23 注释同误。
- **Q2**：mprotect 对 **end** 向上取整（尾部整页并入保护范围）——计划
  文档 :7-8 关于 `ROUND_UP_TO_PAGE` 的第一半陈述（长度方向「取整把相邻
  数据页连带改只读」）方向正确，且 sandstone_p.h:77 注释说的是 end 取整，
  **不在勘误范围**。
- **Q3**：完整复现旧代码故障链——吞掉 EINVAL 后对滞留 PROT_NONE 区的
  写入无声 SIGSEGV。
- **Q4**：madvise 非页对齐起点同样 EINVAL——MADV 路径 :75 吞错属实
  （guard 安装失败 → 线程间栈无保护）。
- **几何算术**：`THREAD_STACK_SIZE = 8192*1024`（8 MiB）= 64K×128 =
  16K×512 → guard = max(8192, page) 时全部 mprotect/madvise 地址
  `mmap_base + N*(guard+TSS) [+ guard]` 页对齐。**修复有效靠的是对齐
  几何，不是任何「取整」行为**——审计的根因归位成立。

## Task 1: 代码修复（framework/sandstone_thread.cpp）

**Files:**
- Modify: `framework/sandstone_thread.cpp:20-26`（注释）、`:53-97`
  （POSIX allocate_stack_block + helper）

**Interfaces:**
- Consumes: `strerror_for_mmap()`（sandstone.h:533，线程安全 errno 渲染）、
  `sandstone_page_size()`（sandstone_p.h:86）、`program_invocation_name`
  （sandstone_p.h:98）、`SandstoneTestThreadAttributes::GuardSize()`。
- Produces: 文件内不可见链接 `[[noreturn]] static void
  thread_stack_setup_failed(const char *what, const void *addr)`
  （unnamed namespace 内、`#ifndef _WIN32` 守卫——Windows 构建不定义，
  无未用警告）。

- [ ] **Step 1.1: 注释机制勘误**——:20-26 的「round the writable region
  start address DOWN past the guard」改为真实机制：Linux 对非页对齐
  mprotect/madvise 地址返回 EINVAL（不取整），几何失配会令栈滞留
  PROT_NONE（mprotect 路径）或线程间无 guard（madvise 路径），故障链
  引 issue #396。

```cpp
    /*
     * Guard region between per-thread stacks.  MUST be at least as large as
     * the kernel page size and, like THREAD_STACK_SIZE, be a multiple of it:
     * every mprotect()/madvise() address below is mmap_base + N * (guard +
     * THREAD_STACK_SIZE) [+ guard], which stays page-aligned only while that
     * holds.  Linux REJECTS a non-page-aligned address with EINVAL -- it
     * does NOT round it down -- so a geometry mismatch leaves the stacks
     * stuck PROT_NONE (mprotect path) or without guards between them
     * (madvise path), crashing the next pthread_create() inside its
     * explicit_bzero.  A swallowed EINVAL used to hide this until issue
     * #396; the setup calls below now abort() loudly on failure.
     */
```

- [ ] **Step 1.2: helper + 三处调用点改判**——:70（MADV 全区 RW）、
  :75（MADV guard 安装）、:84（mprotect 路径 RW）三处
  `IGNORE_RETVAL(...)` 改为 `!= 0 → thread_stack_setup_failed(...)`；
  :66 特性探测 madvise **保持不判**（失败=内核无 MADV_GUARD_INSTALL，
  属设计内回退，非错误）。errno 必须在 fprintf 之前捕获（fprintf 可改
  errno）。

```cpp
#ifndef _WIN32
/*
 * Thread-stack guard setup must not fail silently: Linux rejects a
 * non-page-aligned mprotect()/madvise() address with EINVAL (it does not
 * round it down), so a swallowed failure leaves the stacks PROT_NONE or
 * guard-less and the next pthread_create() crashes with no prior message
 * (issue #396).  Report and abort instead.
 */
[[noreturn]] static void thread_stack_setup_failed(const char *what, const void *addr)
{
    const char *err = strerror_for_mmap();    // capture errno before fprintf
    fprintf(stderr, "%s: %s(%p) failed: %s (page size %u, guard %zu)\n",
            program_invocation_name, what, addr, err,
            sandstone_page_size(), SandstoneTestThreadAttributes::GuardSize());
    abort();
}
#endif
```

调用点（POSIX allocate_stack_block 内）：

```cpp
        if (mprotect(map, size, PROT_READ | PROT_WRITE) != 0)
            thread_stack_setup_failed("mprotect", map);
        ...
            if (madvise(ptr, guard, MADV_GUARD_INSTALL) != 0)
                thread_stack_setup_failed("madvise(MADV_GUARD_INSTALL)", ptr);
        ...
        if (mprotect(ptr, THREAD_STACK_SIZE, PROT_READ | PROT_WRITE) != 0)
            thread_stack_setup_failed("mprotect", ptr);
```

- [ ] **Step 1.3: 构建**——`./scripts/build.sh` 干净构建，零新警告。
- [ ] **Step 1.4: 受影响行为实跑**——任何多线程测试正常起线程即证 guard
  几何未破坏（引用真实输出）；回归 `-e zstd19 -t 2000 -n 1` →
  `exit: pass`；`-e mce_check` → pass。
- [ ] **Step 1.5: 失败路径 A/B 实验**（一次性，不入库）——guard 破坏 hack
  （`GuardSize()` 返回 `8192+2048`，非 4K 整倍数）在一次性构建目录
  `builddir-396exp` 实跑：pre-fix（main 的 thread.cpp + hack）预期
  无声 SIGSEGV；post-fix（本分支 + hack）预期 stderr 响亮报错 + abort。
  完毕 `git checkout fix/issue-396 -- framework/sandstone_thread.cpp`
  还源、删 `builddir-396exp`、`git status` 须 clean。
- [ ] **Step 1.6: 提交**——`fix(framework): 线程栈 guard mprotect/madvise
  吞错改响亮 abort (issue #396)`，`git commit -s`。

## Task 2: 旧计划文档勘误（docs/superpowers/plans/2026-10-28-page-size-guard-fix.md）

**Files:**
- Modify: `docs/superpowers/plans/2026-10-28-page-size-guard-fix.md:10`、
  `:62`（就地更正）+ 文末新增勘误节。

- [ ] **Step 2.1: :10 更正**——「mprotect 把可写区起点向下取整、越过
  guard」→「guard+THREAD_STACK_SIZE 步进使 mprotect 起点非页对齐，Linux
  返回 EINVAL（不取整），被 IGNORE_RETVAL 静默吞掉 → 栈滞留
  PROT_NONE → pthread_create explicit_bzero SIGSEGV」。
- [ ] **Step 2.2: :62 更正**——「检视论证（内核 mprotect 向下取整语义）」
  →「检视论证（对齐几何算术：guard=max(8192,page)、
  THREAD_STACK_SIZE=8192*1024 为 64K 整倍数 → mprotect/madvise 地址
  全页对齐；非对齐 addr → EINVAL 实证见勘误节）」。
- [ ] **Step 2.3: 文末勘误节**——引 issue #396：引原文错句、给实证（Q1）
  与真实故障链、说明修复有效的真实原因（几何），并注明 1f06467e 提交
  信息同误（历史不改写）；吞错路径已由本 issue 修复消除。
- [ ] **Step 2.4: 提交**——`docs(plans): 2026-10-28 页大小修复计划勘误
  (issue #396)`，`git commit -s`。

## 验证边界（诚实声明）

- 本机 4K 页 → `MADV_GUARD_INSTALL` 路径仅编译级验证：内核 6.6 <
  6.13，`madvise(MADV_GUARD_INSTALL)` 实测返回 EINVAL（探针
  /tmp/madv_probe.c），运行时走 mprotect 路径。mprotect 路径真机全绿。
- >8K 页内核故障本机不可复现 → 以「故意破坏几何」在同一路径实证
  （等效的通用几何失配故障，非页对齐 → EINVAL 由 Q1 直接证明）。
- x86-64：检视论证——中立 framework 代码，4K 页 x86-64 上
  `GuardSize() = 8192` 与原值逐位相同、几何不变；失败时的行为差异
  分路径，不得统一压扁为「无声 SIGSEGV」：mprotect 失败（mprotect
  路径逐线程置 RW、MADV 路径全区置 RW）旧为吞错 → 栈滞留
  PROT_NONE → 后续 pthread_create 无声 SIGSEGV（Q3 实测）；MADV
  循环内 madvise(MADV_GUARD_INSTALL) 失败旧为吞错 → 无声降级
  继续跑（线程间缺 guard、无 SIGSEGV——issue #396 谴责的盲区形态，
  6.13+ MADV 内核上 x86-64 同样存在该子路径）——两者现均为响亮
  abort（strictly better）。
- Windows：helper 与三处调用点全在 `#ifndef _WIN32`，构建不受影响。

## 交付

- 分支 `fix/issue-396`（`git checkout -B fix/issue-396 origin/main`）：
  - commit 1：本计划文档；
  - commit 2：thread.cpp 代码修复（Task 1）；
  - commit 3：2026-10-28 计划勘误（Task 2）。
- push origin（feature 分支，绝不 main）；`gh pr create --base main
  --head fix/issue-396`（Closes #396 + 报告）；报告贴 issue #396。

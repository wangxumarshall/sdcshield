# issue #495 审计残余缺口闭合(ROUND_UP_TO 直接单测 + 2026-10-28 plan 勘误)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 闭合 issue #495 审计的两个残余缺口——`sandstone_page.h` 中 `ROUND_UP_TO` 唯一缺直接单测的接口,与 2026-10-28 plan 的虚标验证声明未勘误。

**Architecture:** 纯测试与文档补齐,零运行时代码改动。单测加入既有的 `framework/unit-tests/sandstone_test_utils_tests.cpp` 页大小契约区;勘误按该 plan 文末 2026-11-01(issue #396)勘误节的既有先例追加,不改写历史原文。

**Tech Stack:** C++20 / Google Test(meson `unittests` native 目标)/ Markdown 文档。

**Spec:** issue #495(`gh issue view 495`)——审计指出:向 clean-file 头新增 `sandstone_page_size()` 未加单测,且 plan 2026-10-28-page-size-guard-fix.md 验证节声称「单测覆盖改动头」与事实不符。

## 事实基线(2026-11-06 考古,main @ aeb2c1ea)

- 2026-10-28 `1f06467e` 新增 `sandstone_page_size()` 无单测(plan 验证声明虚标)。
- 2026-11-03 16:29 issue #495 创建(当时 grep 单测文件零匹配属实)。
- 2026-11-04 PR #455(`36ac4f12`,issue #446)+ PR #470(`41c403fa`,issue #451)先后补 43+27 行单测并把实现收敛到权威头 `framework/sandstone_page.h`。
- **当前 main 覆盖盘点**(`sandstone_page.h` 五个公开接口):`sandstone_page_size`/`sandstone_is_valid_page_size`/`sandstone_page_size_fatal`/`ROUND_UP_TO_PAGE` 已有直接单测;**`ROUND_UP_TO(value, n)` 零直接单测**(仅经 `ROUND_UP_TO_PAGE` 以 n=页大小间接覆盖),而仓库有 3 个非页使用点:`framework/sandstone.cpp:331`(alignof)、`framework/sysdeps/unix/resource.cpp:22`(256)、`framework/sysdeps/unix/child_debug.cpp:479`(1024U)。
- issue #495 指控的 plan 虚标文本(:58-59)仍在,未按 :78 先例勘误。

## Global Constraints

- 分支 `fix/issue-495`(自 origin/main 起),绝不 push main;绝不 merge;绝不 resolve issue/PR。
- One patch per unit:每个 commit 一个单元,`git commit -s`,DCO 最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,无 Co-Authored-By。
- 构建 `./scripts/build.sh` 零新警告;单测二进制全绿;回归 `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- x86-64 非回归:改动为纯单测新增 + 纯文档,sdcshield 二进制零字节改动(检视论证);单测代码架构中立(无 `#ifdef`,参照既有 PageSizeTest 区,区别于 `__x86_64__` 守卫的 CodeBuffer 区)。
- 代码风格随 `framework/unit-tests/sandstone_test_utils_tests.cpp` 现状(C++20、4 空格缩进、TEST 宏)。
- 本任务不涉及芯片能力缺失(SVE/SME/VL 档位),单测在本机真实运行,无 gem5-fi 义务。

---

### Task 1: 本计划文档入库

**Files:**
- Create: `docs/superpowers/plans/2026-11-06-issue495-page-size-unit-test-gap.md`(本文件)

**Interfaces:**
- Consumes: 无。
- Produces: 计划文档(2026-10-28 plan 交付先例:commit 1 = 计划文档)。

- [x] **Step 1: 写入本计划文档**(已完成——本文件)。

- [x] **Step 2: Commit**

```bash
git checkout -B fix/issue-495 origin/main
git add docs/superpowers/plans/2026-11-06-issue495-page-size-unit-test-gap.md
git commit -s -m "docs(plan): issue #495 审计残余缺口闭合计划——ROUND_UP_TO 直接单测 + 2026-10-28 plan 勘误 (#495)"
```

### Task 2: ROUND_UP_TO 直接单测(characterization,锁定既有宏语义)

**Files:**
- Modify: `framework/unit-tests/sandstone_test_utils_tests.cpp`(在 `TEST(PageSizeTest, RoundUpToPageRoundsUpToNextPage)` 块之后、:96 WeightedPicker 分区注释之前插入)

**Interfaces:**
- Consumes: `ROUND_UP_TO(value, n)` 宏(`framework/sandstone_page.h:29`,经 `sandstone_test_utils.h` → `sandstone_page.h` 可见,无需新 include)。
- Produces: `TEST(PageSizeTest, RoundUpToAlignsToAnyPowerOfTwo)`——无下游依赖。

**说明(诚实边界):** 这是 characterization test——被测宏自上游 Initial Commit 起已存在,测试预期直接 PASS;「失败先行」的 TDD 形态不适用,覆盖缺口的证明是 grep 事实(单测文件中 `ROUND_UP_TO(` 直接用例零匹配),已在事实基线引用。不测 n 非 2 的幂的行为(宏文档只承诺 power of 2)。

- [ ] **Step 1: 插入测试**

```cpp
TEST(PageSizeTest, RoundUpToAlignsToAnyPowerOfTwo)
{
    // ROUND_UP_TO is the general power-of-two alignment macro ROUND_UP_TO_PAGE
    // is built on; the framework also uses it with non-page alignments
    // (alignof in sandstone.cpp, 256 in resource.cpp, 1024U in child_debug.cpp).
    EXPECT_EQ(ROUND_UP_TO(0, 64), 0);            // zero stays zero
    EXPECT_EQ(ROUND_UP_TO(1, 64), 64);           // sub-align rounds up
    EXPECT_EQ(ROUND_UP_TO(64, 64), 64);          // aligned value unchanged
    EXPECT_EQ(ROUND_UP_TO(65, 64), 128);         // first byte past the boundary
    EXPECT_EQ(ROUND_UP_TO(255, 256), 256);
    EXPECT_EQ(ROUND_UP_TO(1024, 1024), 1024);
    EXPECT_EQ(ROUND_UP_TO(1025, 1024), 2048);
    // size_t operands, as at the alignof() call site (sandstone.cpp)
    EXPECT_EQ(ROUND_UP_TO(static_cast<size_t>(16385), static_cast<size_t>(4096)),
              static_cast<size_t>(20480));
}
```

- [ ] **Step 2: 构建单测目标并运行**

```bash
./scripts/build.sh unittests
./builddir/unittests --gtest_filter='PageSizeTest.*:PageSize.*'
```

Expected: 编译零警告;`RoundUpToAlignsToAnyPowerOfTwo` PASS,既有页大小用例全 PASS。

- [ ] **Step 3: 全量构建零新警告**

```bash
./scripts/build.sh
```

Expected: 与基线相比零新警告。

- [ ] **Step 4: 回归**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1
```

Expected: `exit: pass`(单测为 native 目标,不影响 sdcshield 二进制——此步为惯例回归门)。

- [ ] **Step 5: Commit**

```bash
git add framework/unit-tests/sandstone_test_utils_tests.cpp
git commit -s -m "tests(framework): ROUND_UP_TO 任意 2 的幂对齐直接单测——闭合 sandstone_page.h 全接口覆盖 (#495)"
```

### Task 3: 2026-10-28 plan 虚标验证声明勘误

**Files:**
- Modify: `docs/superpowers/plans/2026-10-28-page-size-guard-fix.md`(文末追加勘误节,不改写 :58-59 历史原文)

**Interfaces:**
- Consumes: :78 既有勘误节格式先例(issue #396)。
- Produces: 无下游依赖。

- [ ] **Step 1: 文末追加勘误节**

```markdown
## 勘误(2026-11-06,issue #495 审计)

本文件 :58-59 声称「运行单测二进制(sandstone_test_utils_tests.cpp 覆盖改动
头)」——验证声明虚标:1f06467e 当时未向该单测文件添加任何用例,新函数
sandstone_page_size()(含缓存与 sysconf 失败路径)零覆盖;既有
ManagedCodeBuffer/WeightedPicker 测试只是碰巧 include 改动头,不构成对新增
内容的覆盖。这违反了 sandstone_test_utils.h :11-13 的 clean-file 硬规则
(「Please do not add anything to this file unless it is unit tested」)。

事实补齐(均晚于 issue #495 的 2026-11-03 审计时点):

- issue #446 修复(PR #455,36ac4f12,2026-11-04):补
  sandstone_is_valid_page_size / sandstone_page_size_fatal /
  sandstone_page_size 并发单测共 43 行;
- issue #451 修复(PR #470,41c403fa,2026-11-04):双实现收敛到权威头
  sandstone_page.h,补 ROUND_UP_TO_PAGE 契约单测 27 行;
- issue #495 修复:补 ROUND_UP_TO 任意 2 的幂对齐的直接单测,
  sandstone_page.h 全部五个公开接口至此均有直接单测。
```

- [ ] **Step 2: 无构建影响声明**(纯 Markdown,不进构建)。

- [ ] **Step 3: Commit**

```bash
git add docs/superpowers/plans/2026-10-28-page-size-guard-fix.md
git commit -s -m "docs(plan): 2026-10-28 页大小 plan 验证声明虚标勘误——单测实际由 #455/#470 补齐 (#495)"
```

### Task 4: 交付(不产生新 commit)

- [ ] push `fix/issue-495` → `gh pr create --base main --head fix/issue-495`(title `fix: ... (#495)`,body 含 `Closes #495` + 根因/改动/验证证据);若 auto-pr.yml 抢先则 `gh pr edit` 兜底。
- [ ] issue #495 贴最终报告(事实链、残余缺口处置、验证证据、PR 链接)——只回帖,不 resolve。
- [ ] 工作区回干净的 main。

## Self-Review

- **Spec 覆盖**:issue #495 两项指控——(1) 零单测 → Task 2(残余唯一缺口 ROUND_UP_TO)+ 事实基线说明主体已由 #455/#470 补;(2) plan 虚标 → Task 3 勘误。无遗漏。
- **Placeholder 扫描**:所有代码/文本步骤均为完整内容,无 TBD。
- **类型一致性**:测试仅用 `ROUND_UP_TO` 宏与 int/size_t 字面量,`static_cast<size_t>` 与文件内 :78 `static_cast<unsigned>` 风格一致;无跨任务接口。

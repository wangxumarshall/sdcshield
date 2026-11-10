# sandstone_p.h 链接块内 include 迁移（issue #590）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `#include "sandstone_page.h"` 从 `framework/sandstone_p.h` 的 `extern "C" {` 链接说明块内部（:81，夹在 EINTR_LOOP 宏与 `program_invocation_name` 声明之间）移到文件顶部 include 区（`#include <unistd.h>` 之后），消除「在链接说明块内包含（经一层间接引入的）系统头」这一反模式。

**Architecture:** issue #590 的原始行号（:57 `extern "C" {` / :73 `#include <unistd.h>`）精确匹配 1f06467e（2026-10-28）快照，其直接诉求已由 PR #407（merged c0abd4d3）完成——`<unistd.h>` 已在顶部 include 区（:26）。但 PR #470（merged 41c403fa）收敛页大小助手到 `sandstone_page.h` 时，把对它的 include 原位（链接块内）留下，且该头自身第 24-26 行无条件包含 `<unistd.h>/<stdio.h>/<stdlib.h>` 三个系统头——反模式经一层间接存活：在 C++ TU 中，`<stdlib.h>` 在 C++ 模式解析到 **C++ 标准库头** `/usr/include/c++/12/stdlib.h`，其实测首次包含正落在 `extern "C" {` 内（本机预处理实证，见 Task 1 Step 1），`using std::abort; … using std::malloc;` 等 32 条 using 声明与 `<cstdlib>` 的再包含全部处于 C 链接上下文。修复即按 issue 自述方案：include 移到顶部 include 区。移动后 `extern "C"` 块内不再有任何 include，`sandstone_page.h` 自带的 `extern "C++"` wrapper 降级为纯防御性自包含措施（保留不动——它是被抽出头自身的语境无关契约，不是 sandstone_p.h 的补丁结构）。

**Tech Stack:** C17/C++20（GCC-10 基线，本机 GCC-12）、Meson、GoogleTest（unittests target）。

**Spec:** issue #590（audit 报告，Minor/正确性：`#include <unistd.h>` 落在 `extern "C" {` 块内部且夹在宏定义序列中间，链接说明块内包含系统头是公认反模式；「把 include 移到文件顶部 include 区（:18-20 一带）即可同时消除这两个补丁式结构」）。

## Global Constraints

- 一个补丁一个单元；`git commit -s`（DCO 最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`）；只推 `fix/issue-590`，绝不推 main。
- 构建零新警告（`./scripts/build.sh`）；受影响行为真实运行引用输出；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`；x86-64 非回归如实论证。
- C/C++ 双兼容：sandstone_p.h 被 5 个 C TU 包含（sysdeps/generic/memfpt.c、generic/physicaladdress.c、unix/tmpfile.c、linux/msr.c、windows/tmpfile.c），新位置（:28 `#ifdef __cplusplus` 之前）仍在语言公共区，两语言均获得该头，与现状一致。
- 不触碰 `__x86_64__` 专属路径；改动为纯 include 位置迁移 + 注释更新，`sandstone_page.h` 本体零改动。
- meson 无需改动：仓库不注册普通头文件，`""` 包含走同目录查找。
- 本机无 MinGW/MSVC，Windows 路径按检查论证（与 PR #407 声明的 mock 边界一致）；gem5-fi 规则不适用（改动与芯片能力无关，无 SVE/SME/VL 层级问题，原生可全验证）。

---

### Task 1: 迁移 include 出链接说明块

**Files:**
- Modify: `framework/sandstone_p.h:26-27`（新增 include + 注释）
- Modify: `framework/sandstone_p.h:77-81`（删除原注释 + include）
- Create: `docs/superpowers/plans/2026-11-09-issue590-page-include-linkage.md`（本计划，随补丁同 commit——#470 先例）

**Interfaces:**
- Consumes: `framework/sandstone_page.h` 的既有契约（自包含、C/C++ 双兼容、自带 `extern "C++"` wrapper）。
- Produces: `ROUND_UP_TO` / `sandstone_page_size()` / `ROUND_UP_TO_PAGE` 在 sandstone_p.h 的定义点从「:81（extern "C" 块内）」提前到「:27 附近（顶部 include 区）」——仅可见性位置变化，无符号/宏语义变化；`extern "C"` 块（:63 起）内不再包含任何 `#include`。

- [x] **Step 1: 复现——预处理实证系统头首次包含落在链接块内**

对最小 TU（首包含即 sandstone_p.h）与真实 TU（framework/sandstone.cpp，经 compile_commands.json 的真实编译旗标）分别 `g++ -E` 预处理，定位 `extern "C" {`（源自 sandstone_p.h:63）与 `/usr/include/c++/12/stdlib.h` 首次包含标记的相对位置。两者均显示：

```console
# 63 "../framework/sandstone_p.h"
extern "C" {
# 81 "../framework/sandstone_p.h"
# 1 "../framework/sandstone_page.h" 1
# 26 "../framework/sandstone_page.h"
# 1 "/usr/include/c++/12/stdlib.h" 1 3     ← C++ 标准库头首次包含，位于 extern "C" 之内
...
using std::abort;
using std::atexit;
...
```

（`<unistd.h>` 与 `<stdio.h>` 因 sandstone_p.h:26 / sandstone.h:10 的更早包含被 include guard 挡住，未在块内首次展开；`<stdlib.h>` 的 C++ 包装头 guard 不同（`_GLIBCXX_STDLIB_H` 系），此前只被 glibc 的 `/usr/include/stdlib.h`（经 libstdc++ 头链，输出行 25728）先行挡过 C 侧，C++ 包装头首次出现即落在链接块内。）

- [x] **Step 2: 编辑——include 移至顶部 include 区，旧位置删除**

`framework/sandstone_p.h` 第 26 行 `#include <unistd.h>` 之后、第 28 行 `#ifdef __cplusplus` 之前插入：

```c
/* Page-size helpers (ROUND_UP_TO / sandstone_page_size / ROUND_UP_TO_PAGE):
 * shared single implementation, see sandstone_page.h.  Keep this include
 * OUTSIDE the extern "C" block below -- the header's own #include <stdlib.h>
 * resolves (in C++) to the C++ <stdlib.h> wrapper, whose first inclusion
 * must not sit inside a linkage specification (issue #590). */
#include "sandstone_page.h"
```

并整体删除 :77-81（原注释 4 行 + include 1 行）：

```c
/* Page-size helpers (ROUND_UP_TO / sandstone_page_size / ROUND_UP_TO_PAGE):
 * shared single implementation, see sandstone_page.h.  Included inside the
 * extern "C" block is safe -- the header is self-contained and gives the
 * helper C++ linkage via its own extern "C++" wrapper. */
#include "sandstone_page.h"
```

`sandstone_page.h` 不动（其 `extern "C++"` wrapper 保留为该自包含头的防御性契约；sandstone_test_utils.h 的常规语境包含不受影响）。

- [x] **Step 3: 探针复核——链接块内零 include**

重跑 Step 1 的两条预处理命令。判定标准（两条都要满足）：
1. `# 1 "/usr/include/c++/12/stdlib.h"` 标记出现位置在 `extern "C" {`（sandstone_p.h:63 展开处）**之前**；
2. `extern "C" {` 与其配对 `}`（sandstone_p.h:113 展开处）之间不再出现任何 `# 1 "..."` include 行标记。

- [x] **Step 4: 构建——零新警告**

```bash
./scripts/build.sh
./scripts/build.sh unittests
```

预期：均成功，无任何新警告（对照：改动前基线同样零警告）。

- [x] **Step 5: 回归——受影响行为 + 契约回归**

```bash
./builddir/unittests                                    # 页大小契约单测（ROUND_UP_TO_PAGE/sandstone_page_size 全绿）
./builddir/sdcshield -e zstd19 -t 2000 -n 1             # 契约回归 → exit: pass
```

（`sandstone_page_size()` 在每次 sdcshield 运行的共享内存布局计算中实际执行——`sandstone.cpp:315/333/337` 的 `ROUND_UP_TO_PAGE`，故任一 `-e` 运行即覆盖框架侧行为。）

- [x] **Step 6: x86-64 非回归论证（按检查）**

改动位于 arch 无关头、语言公共区（`#ifdef __cplusplus` 之前），不含任何 `__aarch64__`/`__x86_64__` 条件；include 内容与条件完全不变，仅位置提前。x86-64 与 ARM64 走同一展开：glibc/mingw/darwin/freebsd 的 C 头均以 `__BEGIN_DECLS`/`__END_DECLS` 自包裹，故块内/块外首次包含的实际链接均为 C 链接，声明效果零变化；`sandstone_is_valid_page_size`/`sandstone_page_size_fatal`（static，内部链接）从 C 链接类型变为 C++ 链接类型仅是类型系统纯化，无符号表/调用约定影响（Itanium C++ ABI 下二者表示相同）。纯声明式论证，无 x86-64 行为变化。

- [ ] **Step 7: 提交（一个单元，含本计划文档）**

```bash
git checkout -B fix/issue-590 origin/main
git add framework/sandstone_p.h docs/superpowers/plans/2026-11-09-issue590-page-include-linkage.md
git commit -s   # 标题: fix(framework): sandstone_p.h 把 sandstone_page.h include 迁出 extern "C" 链接块 (issue #590)
git push origin fix/issue-590
```

随后开 PR（`--base main --head fix/issue-590`，正文含 `Closes #590` + 根因/改动/验证证据），并把报告贴回 issue #590。

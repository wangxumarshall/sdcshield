# 2026-11-02 issue #444:Windows 编译路径页大小守卫（sandstone_p.h 裸 unistd.h/sysconf 回归）

## 背景

issue #444（audit）：`framework/sandstone_p.h:73` 裸 `#include <unistd.h>` 无
`_WIN32` 守卫，系统性破坏 Windows 编译路径——`sysdeps/windows/aligned_alloc.cpp:12`
包含 sandstone_p.h 且 `:59` 的 `pvalloc` 直接使用 `ROUND_UP_TO_PAGE`；该头
`:42-48` 本有 `_WIN32 struct rusage` 兼容块（上游明确支持 Windows 包含此头）。
CI（multi-os-verify 仅 15 个 openEuler SP）不覆盖 Windows，破坏是静默的。

## 根因实证（2026-11-02，本机交叉复现，未复现不修 → 已复现）

- 引入提交：`1f06467e`（2026-10-28，页大小运行时化）。该提交把
  `sandstone_page_size()`（`sysconf(_SC_PAGESIZE)`）与裸 `#include <unistd.h>`
  引入 `sandstone_p.h:73/86-92`；孪生副本 `sandstone_test_utils.h`（`+<unistd.h>`
  include + `sysconf` 版 `sandstone_page_size()`）同病。计划文档
  2026-10-28-page-size-guard-fix.md 自认：裸加 `<unistd.h>` 会使 Windows 编译
  失败——按当时用户裁定 Windows 不在范围。本轮补上该已知缺口。
- 受支持的 Windows 工具链 = MinGW-w64：meson Windows 路径全部标志是 MinGW 专属
  （`-Wa,-mbig-obj`、`-mconsole`、`-Wl,--stack`、`__USE_MINGW_ANSI_STDIO`、
  `-include win32_stdlib.h`）；POSIX 垫片基于 `#include_next`；
  `aligned_alloc.cpp` 用 `__builtin_mul_overflow`——MSVC 从来不可建，
  "无 unistd.h 的工具链"（MSVC 类）不是本仓资产。
- mingw-w64 v9 头文件集（本机 `dnf download mingw64-headers` 9.0.0-6 解包）
  整树 grep：`sysconf` 无 C 声明（唯一出现于 libstdc++ experimental/socket），
  `_SC_PAGESIZE` 完全不存在。而 `#include <unistd.h>` 本身经
  `sysdeps/windows/unistd.h` 垫片（`#include_next`）解析成功。
- 真实 TU 复现（修复前）：`framework/sysdeps/windows/aligned_alloc.cpp` 用
  x86_64-w64-mingw32-g++ 11.2.1（`dnf download mingw64-gcc-c++` 等解包到
  /tmp 前缀，免 root）+ meson 同构 Windows 链（`-include win32_stdlib.h`、
  sysdeps/windows 在 include 路径、同款 `-D` 宏、`-std=gnu++23`，
  `-fsyntax-only`）→ 恰好两错，全在 `sandstone_p.h:90`：
  `'sysconf' was not declared` 与 `'_SC_PAGESIZE' was not declared`。
  （宿主生成的 sandstone_config.h 的 `__fp16` 噪声已按 x86_64-mingw GCC11
  忠实配置消除：meson `has_type` 探测 `_Float16`/`__fp16` 均无 →
  `#undef SANDSTONE_FP16_TYPE`。）
- 结论：审计结论（系统性破坏 Windows 编译路径）正确；机制精化一层——破坏点
  不是 include 行本身（垫片链可解析），是 `sysconf`/`_SC_PAGESIZE` 在 mingw-w64
  头集中不存在，`sandstone_page_size()` 双错必炸于每个含 sandstone_p.h 的
  Windows TU。

## 改动（一个工作单元：恢复 1f06467e 打破的 Windows 编译路径）

1. `framework/sandstone_p.h` — `sandstone_page_size()` 加 `_WIN32` 分支：
   `getpagesize()`（`sysdeps/windows/unistd.c` 的上游垫片实现，`GetSystemInfo`，
   已随每个 Windows 构建链接）；POSIX 分支逐字不变；include 补双分支注释。
2. `framework/sandstone_test_utils.h` — 同款分支（孪生副本，同一回归单元
   1f06467e）。

选型依据：`getpagesize()`（动态；issue 建议的"用系统调用"选项；上游 Windows
路径唯一页大小 API）优于硬编码 4096（违背 1f06467e 运行时化精神；零售 Windows
全架构虽为 4K，仍取系统报告值）。

不守卫 include 本身的依据：Windows 分支（`getpagesize()`）恰需该 include 解析到
`sysdeps/windows` 垫片取得声明；实证表明受支持工具链上 include 解析成功；
MSVC 类工具链从来不可建（include_next 垫片 / GNU builtins），按 `_WIN32`
守卫 include 反而在两个分支都需要它。

## 验证（rule 2）

1. 复现（修复前）：上述交叉编译 TU 真实输出（两错，`sandstone_p.h:90`）。
2. 修复后同编译：零错误（exit 0）。
3. Linux：`./scripts/build.sh` 零新警告；unittests（含
   sandstone_test_utils_tests，覆盖孪生头 POSIX 分支）pass。
4. 回归：`-e zstd19 -t 2000 -n 1` → `exit: pass`（`ROUND_UP_TO_PAGE` 在
   sandstone.cpp shmem 布局与线程栈 guard 上被任何测试运行覆盖）。
5. x86-64 非回归：检视——POSIX 分支逐字未动；Windows 分支仅 `_WIN32` 可达；
   框架中立代码，行为逐位等价。

## 交付

- 分支 `fix/issue-444`：commit 1 本计划文档；commit 2 代码改动
  （`git commit -s`，DCO 最后一行）。push origin（feature 分支，绝不 main），
  PR `Closes #444`，报告贴回 issue #444。

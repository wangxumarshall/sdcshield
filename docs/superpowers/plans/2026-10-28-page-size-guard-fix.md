# 2026-10-28 页大小自适应修复（ROUND_UP_TO_PAGE + 线程栈 GuardSize）

## 背景

用户提供的补丁（本会话原文），修复 >4 KB 页内核（ARM64 16K/64K 页配置）上两个缺陷：

1. `ROUND_UP_TO_PAGE` 硬编码 `4096U`：mprotect() 按内核实际页边界取整，
   4K 假设在 >4K 页内核上会把相邻数据页连带改为只读 → SIGSEGV。
2. `SandstoneTestThreadAttributes::GuardSize` 硬编码 `8192`：>8K 页内核上
   guard+THREAD_STACK_SIZE 步进使 mprotect 起点不再页对齐，Linux 对非页
   对齐 addr 返回 EINVAL（不取整），被 IGNORE_RETVAL 静默吞掉 → 栈滞留
   PROT_NONE → pthread_create 的 explicit_bzero SIGSEGV（MADV 路径则是
   guard 安装 madvise 失败被吞、线程间栈无保护；见文末勘误）。

验证机页大小 = 4096（`getconf PAGESIZE` 实测）→ 本机行为 no-op；
>4K 页修复效果只能检视论证（诚实边界，见"验证"节）。

## 改动（3 文件，一个工作单元）

1. `framework/sandstone_p.h` — 新增 `sandstone_page_size()`
   （`sysconf(_SC_PAGESIZE)` 结果缓存于函数级 static）；`ROUND_UP_TO_PAGE`
   改用该运行时页大小。
2. `framework/sandstone_test_utils.h` — 同款函数（C++ 形式），该头是
   sandstone_p.h 宏的单元测试副本。
3. `framework/sandstone_thread.cpp` — `GuardSize` 从 `static constexpr
   size_t = 8192` 改为静态函数 `max(8192, page_size)`；
   `allocate_stack_block()` / `update_with_stack_for()` 内所有使用点改用
   运行时 guard 值。

## 用户裁定的实现边界

1. **只考虑 Linux**（用户指令 2026-10-28）：<unistd.h>/sysconf 直接落地，
    不加 `_WIN32` 守卫。已知事实（如实记录，非设计变更）：
    `sysdeps/windows/aligned_alloc.cpp` 包含 sandstone_p.h 且使用
    ROUND_UP_TO_PAGE，裸加 `<unistd.h>` 会使 Windows 编译失败——按用户
    裁定 Windows 构建不在本次范围。
2. **粘贴损伤重建**：用户 diff 中 `•` 代替缩进、`allocate_stack_block()`
   出现新旧行交叠，按真实源码语义重建（用户提供的 hunk 语义为准）。

## 安全性论证

- `ROUND_UP_TO_PAGE` 全部 16 个使用点（sandstone.cpp / logging.cpp /
  sandstone_run.cpp / sysdeps/{linux,windows,darwin,freebsd}）均为运行时
  上下文（mmap 尺寸、指针算术、运行期 assert），无数组尺寸/静态断言等
  常量表达式场景 → 宏改为运行时函数调用安全。
- `static inline` + 函数级 static 缓存：每 TU 一份缓存；竞态良性（所有
  写入者写同值）；sysconf 线程安全；`_SC_PAGESIZE` 在 Linux/macOS/
  FreeBSD 均存在。
- x86-64：改动为架构中立 framework 代码，4K 页 x86-64（全部主流配置）
  行为逐位等价；Windows 不在范围（用户裁定，只考虑 Linux）。

## 验证（rule 2，本机 4K 页）

1. 独立构建目录（`builddir-pagefix`，避开在跑的 SVE 基线 builddir）：
   `PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir-pagefix
   --buildtype=release && ./scripts/build.sh -C builddir-pagefix` —
   零新警告。
2. `./scripts/build.sh -C builddir-pagefix unittests` 并运行单测二进制
   （sandstone_test_utils_tests.cpp 覆盖改动头）。
3. 回归：`builddir-pagefix/sdcshield -e zstd19 -t 2000 -n 1` →
   `exit: pass`；`-e mce_check` → pass。
4. 受影响行为实跑：线程栈 guard 几何 = 任何多线程测试正常起线程即证
   未破坏（引用真实输出）；页大小 = `getconf PAGESIZE` = 4096，本机
   no-op 如实声明；>4K 页修复为检视论证（对齐几何算术：guard =
   max(8192, page)、THREAD_STACK_SIZE = 8192*1024 为 64K 整倍数 → 全部
   mprotect/madvise 地址页对齐；非对齐 addr → EINVAL 的内核语义实证见
   文末勘误），无本机可执行路径——验证边界诚实声明。
5. x86-64 非回归：检视（中立代码，4K 页行为等价；Windows 不在范围）。

## 交付

- 分支 `fix/page-size-guard-2026-10-28`：
  - commit 1：本计划文档（docs 同步）；
  - commit 2：代码改动（`git commit -s`，DCO `Signed-off-by: wangxu
    <wangxumarshall@qq.com>`，无 Co-Authored-By）。
- push origin（feature 分支，绝不 main）。

## 勘误（2026-11-01，issue #396 审计）

本文件 :10/:62 原文（及 1f06467e 提交信息，历史不改写）把 >8K 页内核上的
故障机制写成「mprotect 把可写区起点向下取整、越过 guard」——与 Linux
语义相反。实测（kernel 6.6 / aarch64 / 4K 页真机，issue #396 审计小程序
四问）：

- mprotect 非页对齐起点 → **-1 EINVAL，无任何取整**（mprotect(2)：
  EINVAL — addr is not a multiple of the system page size）；
- mprotect 对 **end** 向上取整（尾部整页并入保护范围）——本文 :7-8 关于
  `ROUND_UP_TO_PAGE` 的第一半陈述方向正确，不在勘误范围；
- madvise 非页对齐起点同样 EINVAL。

真实故障链：EINVAL 被 `sandstone_thread.cpp` 的
`IGNORE_RETVAL(mprotect(...))` 吞掉 → 栈滞留 PROT_NONE → pthread_create
explicit_bzero 无声 SIGSEGV（MADV 路径则是 guard 安装失败被吞、线程间栈
无 guard 保护）。A/B 实验实证（GuardSize 破坏 hack=10240 非页整倍数）：
pre-fix 无声 SIGSEGV（SEGV_ACCERR，libc `str q0` 清栈现场）；post-fix
响亮报错 + abort。

修复之所以正确，靠的是对齐几何算术——`THREAD_STACK_SIZE = 8192*1024`
（64K 的 128 倍）使 guard = max(8192, page) 时全部 mprotect/madvise 地址
自然页对齐——而非任何「取整」行为。该静默吞错路径未随 1f06467e 消除，
已由 issue #396 修复（3fbfecc5，三处吞错改响亮 abort，机制注释同步勘误）。

## 勘误（2026-11-06，issue #495 审计）

本文件 :58-59 声称「运行单测二进制（sandstone_test_utils_tests.cpp 覆盖改动
头）」——验证声明虚标：1f06467e 当时未向该单测文件添加任何用例，新函数
sandstone_page_size()（含缓存与 sysconf 失败路径）零覆盖；既有
ManagedCodeBuffer/WeightedPicker 测试只是碰巧 include 改动头，不构成对新增
内容的覆盖。这违反了 sandstone_test_utils.h :11-13 的 clean-file 硬规则
（「Please do not add anything to this file unless it is unit tested」）。

事实补齐（均晚于 issue #495 的 2026-11-03 审计时点）：

- issue #446 修复（PR #455，36ac4f12，2026-11-04）：补
  sandstone_is_valid_page_size / sandstone_page_size_fatal /
  sandstone_page_size 并发单测共 43 行；
- issue #451 修复（PR #470，41c403fa，2026-11-04）：双实现收敛到权威头
  sandstone_page.h，补 ROUND_UP_TO_PAGE 契约单测 27 行；
- issue #495 修复：补 ROUND_UP_TO 任意 2 的幂对齐的直接单测，
  sandstone_page.h 全部五个公开接口至此均有直接单测。

# 2026-11-01 sandstone_p.h 页大小函数 Windows 守卫修复(issue #397)

## 背景(根因 + 复现)

`1f06467e`(2026-10-28,fix/page-size-guard-2026-10-28)在
`framework/sandstone_p.h:73` 无条件 `#include <unistd.h>`(位于 :57 的
`extern "C" {` 块内),且新函数 `sandstone_page_size()` 调用
`sysconf(_SC_PAGESIZE)`。当时计划文档按用户裁定「只考虑 Linux」,只记录了
`sysdeps/windows/aligned_alloc.cpp` 一处受影响,且未在代码中留下任何警示
或守卫。

实测受影响面(比计划文档承认的更大):包含 sandstone_p.h 的 **全部
Windows 编译单元**——13 个 `framework/*.cpp`(Floats/logging/random/
sandstone/sandstone_chrono/sandstone_opts/sandstone_run/sandstone_test_groups/
sandstone_tests/sandstone_thread/sandstone_ulog/selftest/test_knobs,
meson 无平台排除)+ 3 个 `framework/sysdeps/windows/{tmpfile.c:7,
child_debug.cpp:8, aligned_alloc.cpp:12}` = **16 个 TU**。

失败机制(上游 Windows 构建口径 = MinGW,由 meson.build:141-160 的
`__USE_MINGW_ANSI_STDIO`/`-mconsole`/`-include` 等 GCC-only 参数证明):

1. **MinGW 路径**(上游口径):mingw-w64 的 `unistd.h`(master,实测拉取,
   89 行)声明集**不含** `sysconf`/`_SC_PAGESIZE`,且 mingw-w64 全仓库树
   无 sysconf 实现(git tree recursive grep 零命中)→
   `'sysconf' was not declared` / `'_SC_PAGESIZE' was not declared`。
2. **MSVC 路径**(非本项目支持口径):`<unistd.h>` 本身不存在,更早失败。
3. `#include` 位于 `extern "C"` 块内:glibc(`__BEGIN_DECLS`)与 mingw
   (`_CRT_BEGIN_C_HEADER`)均自带嵌套自保护,当前恰好无害,但属「靠
   头文件自保护的巧合安全」,顺带移出。

本仓库 CI 无 Windows job(multi-os-verify.yml 全部 runs-on: ubuntu),
故破坏未被发现。

**本机复现**(aarch64 Linux,无 MinGW 交叉链):用忠实于 mingw-w64 master
`unistd.h` 的 mock(同 include guard、同 `_CRT_BEGIN_C_HEADER` 自保护、
同声明集——特别是无 sysconf/_SC_PAGESIZE),编译 sandstone_p.h :57-96
的最小提取体(Windows C++ TU 所见形态):

```console
$ g++ -c -I mingw-mock -o repro.o repro.cpp
repro.cpp:19:26: 错误:'_SC_PAGESIZE'在此作用域中尚未声明
repro.cpp:19:18: 错误:'sysconf'在此作用域中尚未声明
EXIT=1
```

(诚实边界:mock 是真实 mingw 头的忠实子集而非完整 MinGW 工具链;
sysconf 缺失这一核心变量与真实头文件一致,已引用真实文件内容佐证。)

## 改动(1 文件,一个工作单元)

`framework/sandstone_p.h`:

1. 顶部 include 区(`#include <limits.h>` 之后)加平台分支,
   把 `#include <unistd.h>` 从 :73(extern "C" 区内)移出到此:

   ```c
   #ifdef _WIN32
   #  define WIN32_LEAN_AND_MEAN
   #  include <windows.h>     /* GetSystemInfo() for sandstone_page_size() */
   #else
   #  include <unistd.h>      /* sysconf(_SC_PAGESIZE) for sandstone_page_size() */
   #endif
   ```

2. `sandstone_page_size()` 加 `_WIN32` 分支(GetSystemInfo 回退,
   与 Linux 侧同样缓存一次):

   ```c
   static inline unsigned sandstone_page_size(void)
   {
   #ifdef _WIN32
       static unsigned long cached = 0;
       if (!cached) {
           SYSTEM_INFO info;
           GetSystemInfo(&info);
           cached = info.dwPageSize;
       }
       return (unsigned)cached;
   #else
       static long cached = 0;
       if (!cached)
           cached = sysconf(_SC_PAGESIZE);
       return (unsigned)cached;
   #endif
   }
   ```

3. 函数前注释同步双平台口径(sysconf → sysconf/GetSystemInfo)。

签名依据(实测拉取 mingw-w64 master `sysinfoapi.h:18-34,70`):
`SYSTEM_INFO.dwPageSize` 为 `DWORD`;`VOID WINAPI GetSystemInfo(LPSYSTEM_INFO)`。

## 设计依据(逐项)

- **`WIN32_LEAN_AND_MEAN`**:仓库既有防御先例(random.cpp:33);全仓库
  (framework/ + tests/,third-party 除外)对 LEAN 排除头(rpc/shellapi/
  winsock/mmsystem/wincrypt…)零依赖(grep 实测);避免把 winsock.h 拉进
  全部 16 个 TU(mingw windows.h 无 LEAN 时会包含 winsock.h,
  windows.h:91-93 实测)。
- **min/max 宏污染**:mingw-w64 `minwindef.h:170` 的 `#ifndef __cplusplus`
  守卫使 C++ TU 从不获得 min/max 宏(实证),sandstone_opts.cpp:701 等
  `std::min` 用法安全;唯一 C TU(tmpfile.c)现状本就 include windows.h,
  宏环境不变。MSVC 口径不受支持(meson GCC-only 参数),不构成约束。
- **`extern "C++"` 逃逸**:保留(现状已验证形态,static 函数链接说明本就
  无效,保留零风险)。
- **不移 windows.h 相关内容到 sysdeps**:Windows 上 `ROUND_UP_TO_PAGE`
  唯一使用点 aligned_alloc.cpp:59 `pvalloc()` 与 16 个 TU 的头内联需求
  要求函数自足于头文件;拆分会引入 Linux/Windows 结构不对称,超出最小改动。
- **不动 `framework/sandstone_test_utils.h`**:单测专用副本,上游本来就
  `#include <sys/mman.h>`(:20,Windows 从来不可编译),非本 commit 引入,
  不在本 issue 范围(如实声明,不顺手扩界)。

## 安全性论证

- **Linux/macOS/FreeBSD 行为零变化**:`#else` 分支代码与现状逐字相同
  (sysconf 路径);仅 `#include <unistd.h>` 位置从 extern "C" 区内移到
  文件顶部——glibc/musl 的 unistd.h 自足,提前包含改变不了声明集;
  构建零新警告验证兜底。
- **Windows 语义**:`dwPageSize` 是该系统页大小(x86/x64 = 4 KB,
  ARM64 WoA 24H2 可为 16 KB),与 pvalloc 的「round up to the actual
  page size」语义一致;GetSystemInfo 是 core API,MinGW/MSVC 均有。
- **竞态**:与现状同构(函数级 static + 写同值,良性)。

## 验证(rule 2)

1. `./scripts/build.sh` 全量重编:零新警告。
2. **修复后 mock 重跑**(同一 mingw mock 环境 + `-D_WIN32` + 忠实
   `windows.h` mock,按 sysinfoapi.h 真实签名):预修复错误消失,
   编译通过并正确返回 `dwPageSize`——引用真实输出。
3. 回归:`-e zstd19 -t 2000 -n 1` → `exit: pass`;`-e mce_check` →
   pass(标准口径);线程栈 guard 实跑(多线程全核)→ pass。
4. x86-64 非回归:改动为架构中立 framework 代码,Linux 路径逐字不变
   (检视 + 构建验证);纯守卫新增,如实声明。
5. **Windows 真实工具链验证不可得**(本机无 MinGW/MSVC,CI 无 Windows
   job):以上述 mock 编译 + mingw-w64 真实头文件拉取检视为验证边界,
   如实声明,不以「应该能过」替代。

## 交付

- 分支 `fix/issue-397`(从 origin/main):
  - commit 1:本计划文档(docs 同步);
  - commit 2:代码修复(`git commit -s`,DCO 末行 Signed-off-by,
    无 Co-Authored-By)。
- push origin feature 分支(绝不 main);PR `--base main`,正文含
  `Closes #397` + 修复报告;报告贴回 issue #397。

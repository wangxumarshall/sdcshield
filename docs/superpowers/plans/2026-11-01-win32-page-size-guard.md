# 2026-11-01 sandstone_p.h 页大小函数 Windows 守卫修复(issue #397)

## 背景(根因 + 复现)

`1f06467e`(2026-10-28,fix/page-size-guard-2026-10-28)在
`framework/sandstone_p.h:73` 无条件 `#include <unistd.h>`(位于 :57 的
`extern "C" {` 块内),且新函数 `sandstone_page_size()` 调用
`sysconf(_SC_PAGESIZE)`。当时计划文档按用户裁定「只考虑 Linux」,只记录了
`sysdeps/windows/aligned_alloc.cpp` 一处受影响,且未在代码中留下任何警示
或守卫。

实测受影响面(R1 检视勘误 + R2 处置 worker 独立复核后的口径):包含
sandstone_p.h 的 **全部 Windows 编译单元**(默认构建口径:target=windows、
device_type=cpu 缺省、selftests 缺省 true)——13 个 `framework/*.cpp`
(Floats/logging/random/sandstone/sandstone_chrono/sandstone_opts/
sandstone_run/sandstone_test_groups/sandstone_tests/sandstone_thread/
sandstone_ulog/selftest/test_knobs;selftest 经 `-Dselftests` 缺省 true
编入,其余在 framework/meson.build:192 起的无条件 framework_files 列表)
+ 3 个 `framework/device/cpu/{cpu_device.cpp,topology.cpp,logging_cpu.cpp}`
(framework/meson.build:95 `subdir(devicedeps_dir)` 无平台条件,
device/cpu/meson.build:30-34 无条件加入 framework_files)+ 3 个
`framework/sysdeps/windows/{tmpfile.c:7,child_debug.cpp:8,
aligned_alloc.cpp:12}` + 2 个经 sysdeps/windows/meson.build 的
`'../generic/memfpt.c'`、`'../generic/physicaladdress.c'` 条目编入
Windows 构建的 `framework/sysdeps/generic/` C TU(两文件均在 :6 直接
include;generic/kvm.c 同列表编入但不 include 本头)+ 1 个
`tests/common/mce_check/mce_check.cpp:29`(tests/common/meson.build:8
无条件编译;紧邻的 smi_count 反而有 `if target_machine.system() ==
'linux'` 守卫)+ 3 个 `tests/cpu/memory/memcpy_{l1d_size,l2_cache_size,
l3_cache_size}.cpp`(各 :2 直接 include,tests/cpu/meson.build:79-81 位于
无 when: 条件的 `tests_set_base.add(files(...))` 列表)= **直接 25 个 TU**;
另加 1 个间接 TU `framework/device/cpu/premain.cpp`(经 cpuid_internal.h:18
拉入 sandstone_p.h,cpuid_internal.h 的唯一非直接包含消费者)= **合计
26 个 TU**。排除项口径:smi_count(linux target 守卫)、event_monitor 与
device/gpu/idxd(device_type 守卫)、sysdeps/{darwin,freebsd,linux,unix}
(system 守卫);host aarch64 门控的测试族(cache_stress_civac 等)仅在注释
提及本头、无 include,不在枚举内。

**「全部恢复可编译」不成立(R2 检视 Critical 证伪,worker 独立复核
属实)**:上述 26 个 TU 中至少 4 个另有**独立于 sandstone_p.h、早于
1f06467e 就存在**的 Windows 阻断——`framework/sdc_stress_config.h:85-86`
(`sysconf(_SC_PHYS_PAGES)`/`sysconf(_SC_PAGESIZE)`,该处无任何平台守卫,
:58-60 的 `__aarch64__ && __linux__` 门在 :60 已闭合;引入于 148389c6
2026-10-15)与 :327(`sysconf(_SC_NPROCESSORS_ONLN)`),经
`sandstone_opts.cpp:9` `#include "sdc_stress_config.h"` 编入 Windows
构建(framework/meson.build:202 无条件 framework_files);以及
memcpy_{l1d,l2,l3}_size.cpp 的未守卫 `sysconf(_SC_LEVEL{1_DCACHE,2_CACHE,
3_CACHE}_SIZE)`(memcpy_l1d_size.cpp:20 / memcpy_l2_cache_size.cpp:19 /
memcpy_l3_cache_size.cpp:22;引入于 40f23fee 2026-09-20 / 6bedc73e
2026-08-21)。sysdeps/windows shim 头(sysdeps/windows/unistd.h、
sys/mman.h 等)均无任何 `_SC_*` 定义(grep 实测)。**本 PR 解除的是
1f06467e 在 sandstone_p.h 引入的这一单点阻断(issue #397 的审计范围);
存量阻断的修复需要 Windows API 移植(GlobalMemoryStatusEx /
GetLogicalProcessorInformation 等),各为一个独立单元且超出本 issue
范围,建议单开 issue**。修复后这些 TU 在 Windows 上仍会因存量 sysconf
直调编译失败——这是如实声明的验证边界,不是本 PR 能承诺恢复的事实。

失败机制(上游 Windows 构建口径 = MinGW,由 meson.build:141-160 的
`__USE_MINGW_ANSI_STDIO`/`-mconsole`/`-include` 等 GCC-only 参数证明;
R2 勘误后的准确解析链):

1. **include 解析链(R2 勘误)**:framework_incdir(framework/meson.build:
   76-86)把 `sysdeps/windows` 前置于系统路径,Windows 上 sandstone_p.h 的
   `#include <unistd.h>` **先解析到仓库自带 shim**
   `sysdeps/windows/unistd.h`(`#include_next <unistd.h>` 再链到 mingw-w64
   的 unistd.h)——shim 与 mingw 头**两者均不声明 `sysconf`,也不定义
   `_SC_PAGESIZE`**(shim 头 grep 实测无任何 `_SC_*`;mingw-w64 master
   unistd.h 实测拉取声明集无 sysconf)→ `'sysconf' was not declared` /
   `'_SC_PAGESIZE' was not declared`。初版文档只描述了 mingw 头一侧,
   漏了 shim 这一层——但两层结论一致(都无 sysconf),复现结论不变。
2. **MSVC 路径**(非本项目支持口径):`<unistd.h>` 经 shim 解析不缺,
   但 sysconf/_SC_PAGESIZE 同样缺失,仍在 sandstone_p.h 处失败。
3. `#include` 位于 `extern "C"` 块内:glibc(`__BEGIN_DECLS`)与 mingw
   (`_CRT_BEGIN_C_HEADER`)均自带嵌套自保护,当前恰好无害,但属「靠
   头文件自保护的巧合安全」,顺带移出。

本仓库 CI 无 Windows job(multi-os-verify.yml 全部 runs-on: ubuntu),
故破坏未被发现。

## 改动(R2 后最终口径:2 个代码文件,两个工作单元)

`framework/sandstone_p.h`(一个工作单元,R2 设计修正后最终形态):

1. 顶部 include 区(`#include <limits.h>` 之后)无条件:

   ```c
   /* unistd.h: Windows 下经 framework_incdir 解析到仓库自带 sysdeps/windows
    * shim(getpagesize 声明,#include_next 链到 mingw 的 unistd.h——两者均无
    * sysconf);其余平台为系统 unistd.h(sysconf)。sandstone_page_size() 的
    * 两个实现都由它供给,不再引入 windows.h(零宏面注入)。 */
   #include <unistd.h>
   ```

   (R1 曾采用 `#ifdef _WIN32 [WIN32_LEAN_AND_MEAN/NOMINMAX 守卫 +
   windows.h] #else [unistd.h] #endif`;R2 检视指出仓库既有
   `sysdeps/windows/unistd.h:26` `int getpagesize(void);` shim
   (unistd.c 以 GetSystemInfo().dwPageSize 实现),改用之即可零宏面注入,
   宏守卫层随之撤销。)

2. `sandstone_page_size()` 的 `_WIN32` 分支改调 `getpagesize()`:

   ```c
   static inline unsigned sandstone_page_size(void)
   {
   #ifdef _WIN32
       static int cached = 0;
       if (!cached)
           cached = getpagesize();     /* sysdeps/windows/unistd.h shim */
       return (unsigned)cached;
   #else
       static long cached = 0;
       if (!cached)
           cached = sysconf(_SC_PAGESIZE);
       return (unsigned)cached;
   #endif
   }
   ```

3. 函数前注释同步双平台口径(sysconf → sysconf/getpagesize)。

`framework/sysdeps/windows/aligned_alloc.cpp`(另一个工作单元,R1 检视
采纳):`valloc` 的对齐从硬编码 4096 改为 `sandstone_page_size()`——
16K 页系统(WoA 24H2)上 pvalloc 此前会返回 4096 对齐而非页对齐内存;
linux/darwin 侧 valloc 分别经 `__libc_valloc`/`malloc_zone_valloc`
保证真页对齐,Windows 侧原是三平台唯一 >4K 页语义错误实现(两函数均无
in-repo 调用者,潜伏缺陷修复)。

## 设计依据(逐项)

- **`getpagesize()` shim 而非直接 GetSystemInfo(R2 设计修正,采纳)**:
  仓库既有 `sysdeps/windows/unistd.h:26` 声明 `int getpagesize(void);`,
  `unistd.c` 以 `GetSystemInfo().dwPageSize` 实现——同一数值来源,上游
  Intel 既有 plumbing,Windows 全部框架代码的 POSIX shim 机制(pipe/
  pread/pwrite 同文件)本就如此工作。优点:(a) `#include <unistd.h>`
  保持无条件单一形态,Windows 经 framework_incdir 解析到 shim,Linux/
  darwin/freebsd 走系统头,**Linux 路径逐字不变**;(b) **零
  windows.h 宏面注入**——R1 方案把 windows.h(及其 LEAN/NOMINMAX 宏
  卫生问题、winsock 拉入风险、DELETE/ERROR/near/far 等 token 宏面)
  注入全部 26 个 TU,催生了一整套宏守卫 + token 扫描验证链,R2 检视
  证明该扫描连「全量」都称不上(漏 TRUE/FALSE/TRANSPARENT/OPAQUE/
  CONST/IN/OUT);getpagesize 方案从根上消除该风险类,无需任何宏守卫。
- **min/max 宏污染、WIN32_LEAN_AND_MEAN、NOMINMAX**:R2 后**全部不再
  适用**——sandstone_p.h 不再定义任何宏、不再包含 windows.h。R1 为
  windows.h 方案加的 `#ifndef` 守卫层(7cf2294b)随设计修正一并撤销。
  仓库内仍自带 windows.h 的 TU(sysdeps/windows/*、sandstone.cpp:35、
  logging.cpp 自带 `#ifdef _WIN32` include 区等)维持其原有形态,不受
  本头影响(grep 实测:使用 IN/OUT/TRUE/FALSE/HANDLE 等 Windows 符号的
  TU 全部自带 windows.h include——sandstone.cpp:35、logging.cpp:74
  区、cpu_affinity.cpp 等;无任何 TU 依赖 sandstone_p.h 的传递注入,
  1f06467e 之前的基线结构亦如此)。
- **`extern "C++"` 逃逸**:保留(现状已验证形态,static 函数链接说明本就
  无效,保留零风险)。
- **不移内容到 sysdeps**:Windows 侧 `getpagesize` 本就来自 sysdeps
  (复用既有 shim),头内联的硬需求不变——grep 实测 Windows 编译 TU 中
  `ROUND_UP_TO_PAGE` 共 **9 个使用点**:sandstone.cpp:315/333/337/376/
  378/410(init_shmem/commit_shmem/attach_shmem 的共享内存布局,经
  sysdeps/windows/mman.c 的 VirtualAlloc/VirtualProtect shim 真实运行)、
  logging.cpp:368/379(LogMessagesFile 的 mmap/munmap)、sandstone_run.cpp:
  865(protect_shmem 断言)。多个 Windows TU 直接消费
  `sandstone_page_size()`/`ROUND_UP_TO_PAGE`,头内联是硬需求;shmem
  布局消费 dwPageSize 恰是修复的正确性收益(4K 页系统数值不变;16K 页
  WoA 上布局取整到真实页而非硬编码 4096)。
- **不动 `framework/sandstone_test_utils.h`(R2 归因勘误 + R3 引入史
  勘误后重述)**:该头是单测专用副本(刻意脱离 sandstone_p.h 依赖,自注
  「copied from sandstone_p.h because sandstone_p.h creates too many
  dependencies」)。初版排除理由「上游本来就 `#include <sys/mman.h>`
  (:20,Windows 从来不可编译)」**是错误归因(R2 检视证伪,worker 复核
  属实)**:仓库自带 `sysdeps/windows/sys/mman.h`(PROT_*/MAP_*/
  mmap/mprotect 全套声明),framework_incdir 前置使 `<sys/mman.h>` 在
  Windows 解析到该 shim(logging.cpp:39 的 angle include 即同一机制)
  ——mman.h 不构成阻断;真阻断点是 :33 的 `sysconf(_SC_PAGESIZE)`。
  **R3 勘误(引入史)**:`git show 1f06467e -- framework/
  sandstone_test_utils.h` 实证该 commit 恰是本回归的引入点之一——加
  :21 `#include <unistd.h>` 并把 `ROUND_UP_TO(value, 4096U)` 改为
  sysconf 版函数;即修复前(硬编码 4096)该文件 Windows 可编译,其
  sysconf 是 **1f06467e 引入的回归点**(非存量)。仍排除修复的真实
  依据(R2/R3 复核后口径):(a) unittests 目标事实上 Linux-only——
  `build_by_default: false` + `native: true`(不进默认构建口径)且
  meson.build:348-369 **无条件编入 `sysdeps/linux/malloc.cpp`**,单修
  test_utils.h:33 救不了该目标的 Windows 可编译性;(b) 修法与
  sandstone_p.h 相同(getpagesize 镜像分支),属独立工作单元,
  建议 unittests 目标 Windows 化单开 issue 一并处理。

## 安全性论证

- **Linux/macOS/FreeBSD 行为零变化**:`#else` 分支代码与现状逐字相同
  (sysconf 路径);`#include <unistd.h>` 在非 Windows 平台解析到系统头,
  与 d4f711a0 后已验证形态一致(glibc/musl 的 unistd.h 自足);构建零新
  警告验证兜底。
- **Windows 语义**:`getpagesize()` shim 的实现即
  `GetSystemInfo().dwPageSize`(sysdeps/windows/unistd.c),与 R1 方案
  同一数值来源、同一「该系统页大小」语义(x86/x64 = 4 KB,ARM64 WoA
  24H2 可为 16 KB),但经上游既有 plumbing;缓存一次的行为保留
  (函数级 static,与两侧现状同构)。
- **竞态**:与现状同构(函数级 static + 写同值,良性)。

## 已知缺陷与遗留(R3 检视披露,超出本 issue 范围,建议单开 issue)

- **ROUND_UP_TO_PAGE 对 ≥4GiB 64 位操作数回绕(存量,本 PR 未加重)**:
  `sandstone_page_size()` 返回 `unsigned`(32 位契约),`ROUND_UP_TO` 的
  `~((n)-1)` 掩码在 unsigned 上求补、64 位 AND 中零扩展——worker 实测
  (逐字宏 + unsigned 供值编译运行):`ROUND_UP_TO_PAGE(4GiB+123) =
  16384`(期望 0x100004000)、`ROUND_UP_TO_PAGE(8GiB+5) = 16384`(期望
  0x200000000)。无钳制活调用点:logging.cpp:368 的 mmap 长度(st.st_size
  为 64 位,>4GiB 消息文件时映射截断而读取仍按原大小遍历→越界 SIGSEGV)。
  1f06467e 之前的 `ROUND_UP_TO(value, 4096U)` 同为 32 位掩码——纯存量;
  修复需改跨平台共享契约(unsigned → size_t,影响 9+ 个 Linux 活调用
  点),属独立工作单元与独立验证,不在本 PR 顺手改,建议单开 issue。
- **CI Windows 编译防线(R1-10 遗留,R3 补充事实)**:仓库根自带
  `meson-cross-win32.ini`([binaries] 预期 /usr/bin/x86_64-w64-mingw32-gcc
  + wine exe_wrapper)却无任何 CI job 消费(multi-os-verify.yml 全部
  ubuntu)。防线此刻建立即恒红——上述 4 个存量 sysconf 阻断 TU 修复前
  Windows 构建本就失败;防线必须与存量阻断修复一体规划,建议单开 issue。

## 验证(rule 2;R2 后口径)

1. `./scripts/build.sh` 重编:零新警告(R2 修正后增量重编全绿)。
2. **mock 重跑(R2 后:按真实解析链建模,比 R1 的纯函数体 mock 更强)**:
   `-D_WIN32` 下 `-I` 指向**仓库真实的 sysdeps/windows**(真 shim:
   `#include_next` + `getpagesize` 声明,非转写)+ 第二 -I 放 mock
   mingw unistd.h(忠实 mingw-w64 master 声明集,无 sysconf),受验
   include 区与函数从修复后 sandstone_p.h 逐字 sed 提取——C++/C 双
   模式 `-Wall -Wextra -Werror` 编译通过,运行返回 mock 页大小
   16384;对照:1f06467e 的 sysconf-only 版本在同一真实解析链下复现
   `'sysconf' was not declared`。valloc/pvalloc(修复后逐字提取)在
   16K 页 mock 下 `valloc(12345)`/`pvalloc(12345)` 返回地址
   mod 16384 = 0(页对齐);旧版硬编码 4096 对照返回 mod 16384 = 8192
   (缺陷实证)。默认(无 -D_WIN32)模式在 Linux 实跑返回真实页大小。
   **mock 的局限(如实声明)**:仍是提取体 + mock 头链,不是 26 个受影响
   TU 的本体编译,更不能发现 TU 本体的存量 sysconf 直调(见上文
   「全部恢复可编译不成立」)——这正是 R2 Critical 指出的验证结构
   盲区,已用全仓 grep + git blame 静态补上(4 个存量阻断 TU 全部
   定位并如实声明)。
3. 回归:`-e zstd19 -t 2000 -n 1` → `exit: pass`;`-e mce_check
   -t 2000 -n 1` → `exit: pass`。
4. x86-64 非回归:改动为架构中立 framework 代码,Linux `#else` 分支
   逐字不变(检视 + 构建验证);`#ifdef _WIN32` 分支仅 Windows 编译。
5. **Windows 真实工具链验证不可得**(本机无 MinGW/MSVC——which 三种
   mingw-gcc + clang-cl 全部未命中;CI 无 Windows job;gem5-fi 为
   ARM64 Linux 全系统仿真、无法仿真 Windows 用户态 API):以上述
   真实解析链 mock + shim/unix 头逐字核读 + 全仓 grep + git blame 为
   验证边界,如实声明,不以「应该能过」替代。
6. **宏 token 扫描(R1 遗留验证,R2 后不再承载验证职责,如实记录)**:
   R1 时代针对「windows.h 注入 26 TU」的 token 扫描集
   `\b(DELETE|OPTIONAL|IGNORE|ERROR|NEAR|FAR|near|far)\b` + 裸
   `min(|max(` 曾以「全量」名义呈报——R2 检视指出不全量(漏
   TRUE/FALSE/TRANSPARENT/OPAQUE/CONST/IN/OUT),worker 以扩展集复扫
   `framework/ + tests/`:代码位命中仅 sandstone.cpp:764(FALSE/
   CreateEventA,自带 windows.h)、cpu_affinity.cpp:90(sysdeps/
   windows,自带)、logging.cpp:86(IN OUT,自带 windows.h),其余
   全为字符串字面量与注释(forkfd/sysexits 的 MIT/BSD 许可文本),
   **零实撞**——结论侥幸成立,但 R2 改用 getpagesize shim
   后 sandstone_p.h 不再注入任何宏面,该扫描不再是本 PR 的验证依赖。

## 交付

- 分支 `fix/issue-397`(从 origin/main):
  - commit:本计划文档(docs 同步);
  - commit:代码修复(`git commit -s`,DCO 末行 Signed-off-by,
    无 Co-Authored-By);
  - R1 处置:宏守卫 + valloc + 文档勘误三单元;
  - R2 处置:getpagesh 设计修正(撤销宏守卫层)+ 文档 R2 勘误两单元。
- push origin feature 分支(绝不 main);PR `--base main`,正文含
  `Closes #397` + 修复报告;报告贴回 issue #397。

## 检视修正(PR #407 review R1,2026-11-01)

第 5 bot(local-claude)对 head d4f711a0 的 6 条意见 + pr-agent 1 条,
逐条批判性处置后采纳 5 条(代码 2:valloc 对齐、宏卫生;文档 3:受影响面
勘误、9 使用点勘误、mock 局限如实化)、变通 2 条(pr-agent「需人工进一步
验证真实 Windows 构建」与 local-claude 工具链验证意见的真实边界——本机
无 mingw/MSVC、gem5-fi 无法仿真 Windows 用户态,如实声明;CI Windows
防线遗留人工决策):

1. **valloc/pvalloc 对齐**(采纳,代码):valloc 硬编码 4096 对齐改为
   `sandstone_page_size()`(855b308b)。
2. **宏卫生**(采纳,代码):`WIN32_LEAN_AND_MEAN` 加 `#ifndef` 守卫 +
   追加 `NOMINMAX`(7cf2294b)。**R2 注:随设计修正(见下)整体撤销,
   该方案被证伪为过度设计。**
3. **文档勘误**(采纳,文档):受影响面 16 → 26 个 TU(R2 再勘误:
   R1 勘误初稿的 21 仍漏 generic 2 + memcpy 3);9 使用点勘误;
   「唯一含本头的 C TU tmpfile.c」勘误为 3 个 C TU;tmpfile.c 的
   windows.h 由非 lean 变 lean 核对;mock 验证声称如实化。
4. **宏 token 全量扫描**(新增验证):全仓零实撞(R2 注:R1 的「全量」
   名不副实,已扩展集复扫,仍零实撞)。
5. **真实 Windows 工具链编译验证**(变通):验证边界如实声明。
6. **CI Windows 编译防线**(变通,遗留):建议人工决策单开 issue。

## 检视修正(PR #407 review R2,2026-11-02,head ad208e0e)

第 5 bot 对 head ad208e0e 的 6 条意见(1 Critical + 1 Important 行内
finding + 4 Minor 行级),逐条批判性处置:

1. **「26 TU 全部恢复可编译」被证伪**(Critical,**采纳——文档口径
   修正 + 范围裁定**):worker 独立复核属实——sdc_stress_config.h:85-86/
   :327(148389c6,2026-10-15,早于 1f06467e)与 memcpy_{l1d,l2,l3}
   的未守卫 sysconf(40f23fee/6bedc73e,2026-09-20/08-21)构成 4 个 TU
   的独立存量阻断。处置:文档改为「解除 sandstone_p.h 单点阻断 +
   如实声明存量阻断(含引入时间线与建议单开 issue)」,不扩 PR 范围
   (修复存量阻断 = Windows API 移植,各为独立单元,且 mock 边界内
   不可验证);mock 的验证结构盲区(提取体覆盖不了 TU 本体)在验证节
   如实化,静态 grep + git blame 补位。
2. **test_utils.h 排除理由证伪**(Important,**采纳——归因修正,仍
   排除但换真理由**):sys/mman.h 在 Windows 解析到仓库 shim(framework_
   incdir 前置),真阻断是 :33 的 sysconf。但 unittests 目标
   `build_by_default: false`/`native: true` 且无条件编入
   sysdeps/linux/malloc.cpp——单修 test_utils.h:33 救不了该目标的
   Windows 可编译性;独立副本 + 独立单元,建议随存量阻断单开 issue。
   (reviewer「镜像加 5 行即修」对文件本身正确,对本 PR 目标无增益。)
3. **token 扫描不全量**(Minor,**采纳**):扩展集
   TRUE/FALSE/TRANSPARENT/OPAQUE/CONST/IN/OUT 复扫,零实撞(3 个
   代码位命中均自带 windows.h);文档措辞修正 + 随设计修正降级为
   历史记录(不再承载验证职责)。
4. **注释死指引**(Minor,**采纳——随设计修正消除**):「自行
   #define」半句确为死指引(windows.h include guard 使 #ifndef 后无法
   再得非 lean);该注释随 windows.h 一并移除,问题不复存在。
5. **重造 getpagesize 轮子**(Minor,**采纳——设计修正**):改用
   sysdeps/windows/unistd.h 的既有 getpagesize shim,保持无条件
   `#include <unistd.h>`,零宏面注入;撤销 R1 的宏守卫层(7cf2294b)
   与 d4f711a0 的 windows.h 注入。安全性核查:全仓 Windows 符号使用
   者(sandstone.cpp:35、logging.cpp、sysdeps/windows/*)全部自带
   windows.h,无 TU 依赖传递注入(1f06467e 之前的基线结构亦如此),
   移除注入不产生新破坏。
6. **文档「改动(1 文件)」过时**(Minor,**采纳**):改动节改为
   R2 后最终口径(2 个代码文件,两个工作单元)。

## 检视修正(PR #407 review R3,2026-11-02,head d521d0bf)

第 5 bot 对 head d521d0bf 的 4 条 Minor 行级意见(无 Critical/Important
finding)+ 并行补审 review 对 head ad208e0e 的 3 条行级意见中仍适用于
当前代码的 2 条,逐条处置:采纳 3、变通 3:

1. **32 位页大小契约对 ≥4GiB 操作数回绕**(Minor,内存,**变通·遗留**):
   worker 实测确认(见「已知缺陷与遗留」节,本机编译逐字宏复现
   ROUND_UP_TO_PAGE(4GiB+123)=16384);存量缺陷(1f06467e 之前同为
   32 位掩码,本 PR 未加重),修复需改跨平台共享契约(unsigned →
   size_t),超出本 issue 范围——记录 + 建议单开 issue,不顺手改。
2. **PR 标题「恢复 Windows 可编译性」过度声明**(Minor,**采纳**):
   PR 标题改为「解除 Windows 编译单点阻断」口径,与本文档一致。
3. **PR 正文停留在 d4f711a0 幽灵设计**(Minor,**采纳**):PR 正文按
   R2 后最终形态重写(getpagesize shim、2 个代码文件、26 TU 诚实口径、
   最终验证证据、完整提交清单)。
4. **meson-cross-win32.ini 无 CI job 消费**(Minor,**变通·遗留**):
   cross 文件存在属实(worker 复核仓库根,预期 /usr/bin/x86_64-w64-
   mingw32-gcc + wine);防线此刻建立即恒红(存量阻断修复前 Windows
   构建本就失败),须与存量阻断修复一体规划,建议单开 issue(见
   「已知缺陷与遗留」节)。
5. **(并行补审,仍适用)test_utils.h 引入史归因再勘误**(Important,
   **采纳**):git 实证 1f06467e 恰是 test_utils.h sysconf 的引入点
   (加 :21 unistd.h + 4096U→sysconf 函数)——它是本 issue 回归点
   之一、非存量;排除修复的真实理由修正为「unittests 目标事实上
   Linux-only」(见设计依据节勘误后口径)。
6. **(并行补审,仍适用)非原子惰性初始化 pattern**(Minor,并发,
   **变通**):两侧分支沿用 `static cached; if (!cached)` 形态——C/C++
   TU 共存的公共分母(magic static 仅 C++11 可用,3 个 C TU 不可用)、
   与 sysconf 侧 1f06467e 以来形态同构、写值恒同、Windows 侧 9 个
   使用点全为主线程上下文,形式竞争实践良性,维持现状。

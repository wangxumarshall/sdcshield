# issue #589: sandstone_page_size() `_WIN32` 分支残余数据竞争修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 `sandstone_page_size()` 在**所有**平台分支上满足 issue #589 的不变式——函数级 static 缓存的懒初始化无数据竞争(C++11 语义)、失败/非法值绝不入缓存——方法是修复 #455 修复时自己引入、至今仍持有同款缺陷的 `_WIN32` 分支。

**Architecture:** issue #589 指控的 POSIX 路径缺陷已在 main 修复(PR #455/36ac4f12,issue #446;收敛进唯一权威头 PR #470/41c403fa,issue #451)。残余增量:`framework/sandstone_page.h` 的 `#ifdef _WIN32` 分支(`static int cached = 0; if (!cached) cached = getpagesize();`)仍是无同步读-改-写(常量初始化 → 无 C++ magic-static 线程保护),与 #589/#446 指控的模式逐字同款;#470 检视已点名「双重标准」但从未修。修复:把函数体统一为单一原子懒初始化骨架,`#ifdef _WIN32` 只剩「取值调用 + 诊断来源标签」两行,POSIX 语义与 abort 消息字节不变。

**Tech Stack:** C/C++ 双兼容头(GNU 扩展:`__atomic_load_n`/`__atomic_store_n`/`__builtin_expect`,mingw GCC 同样支持——meson.build:102 确认 Windows 构建用 `cpp.get_id() == 'gcc'`);单测 gtest;复现用 clang++ 17 `-fsanitize=thread`(本机 GCC 12 的 libtsan 运行库缺失,clang 自带运行时可用)。

**Spec:** issue #589(及其前史 #446 → PR #455、#451 → PR #470);仓库规则 CLAUDE.md / AGENTS.md。

## Global Constraints

- 一个补丁一个单元:本计划全部改动 = `framework/sandstone_page.h` 一处函数体统一 + `framework/unit-tests/sandstone_test_utils_tests.cpp` 一行协同改动(fatal 加参后的调用点),**单个 commit**。
- POSIX 路径行为**字节不变**:原子序(ACQUIRE/RELEASE)、校验、abort 消息文本 `internal error: unusable page size from sysconf(_SC_PAGESIZE): 0x%lx` 逐字保留。
- 头文件契约:C(gnu17)与 C++(gnu++23)双兼容(镜像 meson.build:11-12 的 `c_std=gnu17`/`cpp_std=gnu++23`),除 `<unistd.h>/<stdio.h>/<stdlib.h>` 外零新 include。
- 验证 100% 真实:TSan 复现先行(未复现不修),构建零新警告(对照 pristine main 基线),unittests 全绿,`-e zstd19 -t 2000 -n 1` 与全核 `-e zstd19 -t 2000` 回归 `exit: pass`。
- 验证边界如实声明:本机(ARM64 Linux)与 CI(multi-os-verify.yml 仅 Linux)均无 Windows 工具链/runner;`_WIN32` 分支以「Linux 上 `-D_WIN32` 编译并实跑(glibc `getpagesize()` 顶替 Windows shim 的 `GetSystemInfo` 后端,同签名、同无失败路径)+ TSan 复现→清洁 + 检视」验证。gem5-fi 条款不适用(这是 OS 工具链缺口,非芯片能力缺口——该条款限定 HWCAP/HWCAP2/VL/SVL/ISA 代际)。
- 复现/对照实验放 `/tmp/issue589/`,**不进仓库**(#470「破坏实验已还原未随补丁提交」先例)。
- git 纪律:从 `origin/main` 起 `git checkout -B fix/issue-589 origin/main`,只在此分支提交,`git commit -s`(DCO 最后一行),绝不推 main、绝不 merge、绝不 resolve issue;结束时工作区回到干净 main。

---

### Task 1: 基线构建 + TSan 复现残余缺陷(RED)

**Files:**
- Create: `/tmp/issue589/repro.cpp`(仓库外实验件)
- 无仓库内改动

**Interfaces:**
- Consumes: `framework/sandstone_page.h`(pristine main 版)
- Produces: pristine-main 基线构建警告清单 `/tmp/issue589/baseline-build.log`;TSan 复现输出(残余缺陷的 RED 证据)

- [ ] **Step 1: 建分支并确认干净起点**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
git checkout -B fix/issue-589 origin/main
git status --short        # 期望: 空
```

- [ ] **Step 2: pristine main 基线全量构建,捕获警告基线**

```bash
./scripts/build.sh 2>&1 | tee /tmp/issue589/baseline-build.log
grep -ci "warning" /tmp/issue589/baseline-build.log   # 记下基线数(供 Task 3 对照)
```

期望:构建成功;记下警告计数(若非零,均为既有警告,与本任务无关,如实记录)。

- [ ] **Step 3: 写 TSan 复现 harness(仓库外)**

`/tmp/issue589/repro.cpp`:

```cpp
/* issue #589 residual repro: _WIN32-branch lazy-init data race.
 * NOT part of the repo (#470 experiment precedent). Compile with -D_WIN32
 * against framework/sandstone_page.h; glibc's getpagesize() stands in for
 * the sysdeps/windows shim (same signature, same no-failure semantics). */
#include "sandstone_page.h"

#include <atomic>
#include <cstdio>
#include <thread>
#include <vector>

int main(int argc, char **argv)
{
    const int nthreads = argc > 1 ? atoi(argv[1]) : 8;
    std::atomic<int> gate{0};
    std::vector<unsigned> results(nthreads, 0);
    std::vector<std::thread> threads;
    for (int i = 0; i < nthreads; ++i)
        threads.emplace_back([&, i] {
            while (gate.load(std::memory_order_acquire) == 0)
                std::this_thread::yield();
            unsigned v = 0;
            for (int k = 0; k < 1000; ++k)
                v = sandstone_page_size();   /* hammer the lazy-init */
            results[i] = v;
        });
    gate.store(1, std::memory_order_release);  /* release all at once */
    for (auto &t : threads)
        t.join();

    bool ok = results[0] > 0;
    for (unsigned r : results)
        ok = ok && (r == results[0]);
    printf("threads=%d page_size=0x%x all_agree=%d\n",
           nthreads, results[0], (int)ok);
    return ok ? 0 : 1;
}
```

- [ ] **Step 4: 编译并运行 pre-fix `_WIN32` 复现(期望 TSan 报数据竞争)**

```bash
cd /tmp/issue589
clang++ -std=gnu++23 -g -fsanitize=thread -D_WIN32 \
    -I/home/sdc/wangxu/operator/sdcshield-autoci/framework \
    repro.cpp -o repro_win32_prefix
./repro_win32_prefix 8 2>&1 | tee prefix_win32.log
grep -c "WARNING: ThreadSanitizer: data race" prefix_win32.log
```

期望:stderr 含 `WARNING: ThreadSanitizer: data race`,竞争对象为 `sandstone_page_size` 内的 `cached`(Write of size 4 / Read of size 4,先前 main 的 `static int cached`)。**这是残余缺陷的复现(RED)。**

- [ ] **Step 5: 对照一——pre-fix POSIX 分支(期望清洁,证明 harness 不误报且 main 的 POSIX 路径确已修复)**

```bash
clang++ -std=gnu++23 -g -fsanitize=thread \
    -I/home/sdc/wangxu/operator/sdcshield-autoci/framework \
    repro.cpp -o repro_posix_prefix
./repro_posix_prefix 8 2>&1 | tee prefix_posix.log
grep -c "WARNING: ThreadSanitizer: data race" prefix_posix.log   # 期望 0
```

期望:无 TSan 报告,输出 `all_agree=1`,退出码 0。

### Task 2: 统一原子懒初始化 + 协同单测改动(GREEN)

**Files:**
- Modify: `framework/sandstone_page.h:48-53`(fatal 加 `source` 参数)与 `:59-77`(函数体统一)
- Modify: `framework/unit-tests/sandstone_test_utils_tests.cpp:200-203`(fatal 调用点协同加参)

**Interfaces:**
- Consumes: Task 1 的 harness 与基线
- Produces: `sandstone_page_size_fatal(long page_size, const char *source)`(新签名,POSIX 消息文本字节不变);`sandstone_page_size()` 统一原子懒初始化体(对外签名/语义不变)

- [ ] **Step 1: 改 `framework/sandstone_page.h` 的 fatal 与函数体**

`sandstone_page_size_fatal` 整体替换为(注意:POSIX 输出文本与旧版逐字节相同):

```c
static __attribute__((unused, noinline, noreturn)) void sandstone_page_size_fatal(long page_size, const char *source)
{
    fprintf(stderr, "internal error: unusable page size from %s: 0x%lx\n",
            source, (unsigned long)page_size);
    abort();
}
```

`sandstone_page_size()` 函数体整体替换为:

```c
static inline unsigned sandstone_page_size(void)
{
    static long cached = 0;
    long page_size = __atomic_load_n(&cached, __ATOMIC_ACQUIRE);
    if (__builtin_expect(!page_size, false)) {
        const char *source;
#ifdef _WIN32
        source = "getpagesize()";             /* sysdeps/windows/unistd.h shim */
        page_size = getpagesize();
#else
        source = "sysconf(_SC_PAGESIZE)";
        page_size = sysconf(_SC_PAGESIZE);
#endif
        if (__builtin_expect(!sandstone_is_valid_page_size(page_size), false))
            sandstone_page_size_fatal(page_size, source);
        __atomic_store_n(&cached, page_size, __ATOMIC_RELEASE);
    }
    return (unsigned)page_size;
}
```

并在函数前的块注释(`mprotect() will round ...` 段)末尾追加一句:

```c
 * The cache is an atomic lazy-init: per-CPU worker threads race on the
 * first call (issues #446/#589), so loads/stores go through __atomic_
 * builtins on BOTH branches and neither caches an unvalidated value.
```

- [ ] **Step 2: 协同改单测调用点**(`framework/unit-tests/sandstone_test_utils_tests.cpp:201`)

```cpp
TEST(PageSize, FatalOnUnusablePageSize) {
    EXPECT_DEATH(sandstone_page_size_fatal(-1, "sysconf(_SC_PAGESIZE)"), "unusable page size");
}
```

- [ ] **Step 3: C/C++ 双兼容语法验证(±_WIN32,镜像 meson std 档)**

```bash
cd /home/sdc/wangxu/operator/sdcshield-autoci
printf '#include <stdbool.h>\n#include "sandstone_page.h"\nint main(void){return sandstone_page_size()==0;}\n' > /tmp/issue589/compat.c
printf '#include "sandstone_page.h"\nint main(){return sandstone_page_size()==0;}\n' > /tmp/issue589/compat.cpp
gcc -std=gnu17 -fsyntax-only -I framework /tmp/issue589/compat.c             # C, POSIX
gcc -std=gnu17 -D_WIN32 -fsyntax-only -I framework /tmp/issue589/compat.c    # C, Windows 分支
g++ -std=gnu++23 -fsyntax-only -I framework /tmp/issue589/compat.cpp         # C++, POSIX
g++ -std=gnu++23 -D_WIN32 -fsyntax-only -I framework /tmp/issue589/compat.cpp # C++, Windows 分支
```

(注:`<stdbool.h>` 预置是模拟真实 C TU 经 `sandstone.h` 的传递包含——头文件 C 自足性缺口是 #470 检视已记录的既有独立缺陷,不属本单元。)期望:四种组合全部零输出零错误。

- [ ] **Step 4: post-fix TSan 复验 `_WIN32` 分支(期望清洁)**

```bash
cd /tmp/issue589
clang++ -std=gnu++23 -g -fsanitize=thread -D_WIN32 \
    -I/home/sdc/wangxu/operator/sdcshield-autoci/framework \
    repro.cpp -o repro_win32_postfix
./repro_win32_postfix 8 2>&1 | tee postfix_win32.log
grep -c "WARNING: ThreadSanitizer: data race" postfix_win32.log   # 期望 0
```

期望:无 TSan 报告,`all_agree=1`,退出码 0——Windows 编译分支的原子懒初始化逻辑在 Linux 上实跑(glibc `getpagesize()` 顶替 shim)。

- [ ] **Step 5: post-fix TSan 复验 POSIX 分支(期望清洁,语义未变)**

```bash
clang++ -std=gnu++23 -g -fsanitize=thread \
    -I/home/sdc/wangxu/operator/sdcshield-autoci/framework \
    repro.cpp -o repro_posix_postfix
./repro_posix_postfix 8 2>&1 | tee postfix_posix.log
grep -c "WARNING: ThreadSanitizer: data race" postfix_posix.log   # 期望 0
```

### Task 3: 全量验证

**Files:** 无新改动(纯验证)

**Interfaces:**
- Consumes: Task 2 的改动
- Produces: 验证证据(构建/单测/回归,全部真实输出)

- [ ] **Step 1: 全量重构建,零新警告对照**

```bash
./scripts/build.sh 2>&1 | tee /tmp/issue589/fix-build.log
grep -i "warning" /tmp/issue589/fix-build.log | grep -vf <(grep -i "warning" /tmp/issue589/baseline-build.log) || echo "NO-NEW-WARNINGS"
```

期望:`NO-NEW-WARNINGS`(或差异为空)。

- [ ] **Step 2: unittests 全绿**

```bash
U=$(find builddir -type f -name unittests | head -1)
"$U" --gtest_filter=-LinuxThermalFixture.* 2>&1 | tail -5
```

期望:全部 PASS(`LinuxThermalFixture.*` 为 #470 已记录的既有环境性失败,filter 出局;含改动后的 `PageSize.FatalOnUnusablePageSize`)。

- [ ] **Step 3: 回归——单线程与全核**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -3
./builddir/sdcshield -e zstd19 -t 2000 2>&1 | tail -3
```

期望:两次均 `exit: pass`。全核档兼作修复后原子懒初始化的多线程实跑(每 worker 线程栈 guard 经 `ROUND_UP_TO_PAGE` 并发首调)。

- [ ] **Step 4: x86-64/Windows 非回归边界(如实声明)**

本机与 CI 均无 Windows runner、无 mingw 交叉编译器;非回归依据 = Task 2 Step 3/4/5(四种编译组合 + `_WIN32` 分支 Linux 实跑 + TSan 前后对照)+ 检视(meson.build:102 Windows=GCC,`__atomic` 内建可用;`static long` 在 LLP64 Windows 为 32 位,页大小域足够)。gem5-fi 不适用(OS 工具链缺口非芯片能力缺口)。PR 正文照此声明。

### Task 4: 提交、推送、开 PR、贴报告

- [ ] **Step 1: 单 commit 提交(DCO)**

```bash
git add framework/sandstone_page.h framework/unit-tests/sandstone_test_utils_tests.cpp \
        docs/superpowers/plans/2026-11-09-issue589-win32-page-size-lazy-init.md
git branch --show-current          # 必须是 fix/issue-589
git commit -s                       # 正文:根因(滞后重复+残余增量)、改动、验证证据;
                                    # 末行 Signed-off-by: wangxu <wangxumarshall@qq.com>,其后无内容
```

- [ ] **Step 2: 推分支 + 开 PR(绝不推 main)**

```bash
git push -u origin fix/issue-589
gh pr create --base main --head fix/issue-589 \
  --title "fix: sandstone_page_size() _WIN32 分支原子化——补齐 #446 修复遗留的双重标准 (#589)" \
  --body "Closes #589 + 完整报告(根因/改动/验证证据/边界)"
```

若 auto-pr.yml 抢先:`gh pr edit fix/issue-589 --title … --body …` 兜底(必须含 Closes #589)。

- [ ] **Step 3: 报告贴 issue #589**(POSIX 已修证据 + 残余增量修复 + 验证证据 + PR 链接),不 resolve。

- [ ] **Step 4: 收尾纪律**

```bash
git checkout main && git status   # 期望 clean
```

---

## Self-Review

- **Spec 覆盖**:#589 两项指控——(a) 数据竞争:POSIX 已修(#455)+ 本计划修 `_WIN32` 残余;(b) 失败值永久缓存:POSIX 已修 + `_WIN32` 分支统一校验(该分支 shim 无失败路径,校验为统一不变式的纵深防御,如实声明)。引用位置 `sandstone_test_utils.h:29-35` 的副本已被 #470 消灭(include 唯一权威头)。✔
- **占位符扫描**:无 TBD/TODO;所有代码块完整。✔
- **类型/签名一致性**:`sandstone_page_size_fatal(long, const char*)` 在 Task 2 Step 1/2 与单测调用点一致;`static long cached` 统一(旧 `_WIN32` 为 `static int`, LLP64 同宽)。✔
- **不属本单元的邻接发现**(报告提及、不动手):#470 检视的 stdbool C 自足缺口、`ROUND_UP_TO_PAGE` ≥4GB 截断(Important)——均为独立缺陷,遵循一补丁一单元。

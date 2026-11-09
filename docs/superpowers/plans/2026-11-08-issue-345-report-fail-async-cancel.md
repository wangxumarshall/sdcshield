# Plan: 修复 issue #345 — report_fail 的 PTHREAD_CANCEL_ASYNCHRONOUS 自取消把 fail 退化成 crash

日期：2026-11-08 · 分支：fix/issue-345 · 载体：无头 fleet agent（本机：鲲鹏 920 无 SVE, openEuler, glibc 2.38 aarch64）

## 问题（已核实）

`report_fail_common()`（framework/sandstone_run.cpp:189-202，上游 OpenDCDiag 原始代码，
`git log -L` 证实随 "FW: decouple test run semantics" 导入，非本移植引入）：

```c
pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, nullptr);
pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, nullptr);
pthread_cancel(pthread_self());
```

worker 失败时以**异步取消**终止自身。glibc 的取消/线程退出走 `_Unwind_ForcedUnwind`
强制栈展开（会执行 C++ 作用域析构——`thread_runner` 的 scopeExit cleanup 正依赖此，
实证见下）。ASYNC 类型的取消信号可在**任意指令边界**触发该展开；信号落在 syscall 桩
附近时展开器对被中断帧的 CFI/寄存器恢复不精确，cleanup lambda 以坏基址读
`sApp->shmem->cfg` → SIGSEGV（issue #345 在 SVE 主机实测：RIP 反复落 cleanup lambda、
FAR 恒定且不在任何 maps、-vv 下不出现、mmap 后紧接 report_fail 几乎必现、
pristine 703752ae 上 3/3 run × 11 块 crash）。后果：fail 退化成 crash，triage 语义被污染。

## 本机实证（glibc 2.38 aarch64，2026-11-08）

1. **独立复现器**（/tmp/repro345/repro.cpp，模拟 report_fail_common + thread_runner
   scopeExit 结构）：ASYNC 自取消与 pthread_exit 均 100% 执行 cleanup 析构
   （各 ≥2000 次线程迭代，0 崩溃）——证实 glibc 取消路径执行 C++ 析构
   （上游注释 "does call the cleanup handlers" 成立），也证实修复后语义保持。
2. **反汇编** `/usr/lib64/libc.so.6` 的 `pthread_exit`：存 result → 原子置
   cancelhandling EXITING 位(#0x10) → 调 `__pthread_unwind(&cleanup_jmp_buf)`——
   即 `__do_cancel`。**pthread_exit 与 pthread_cancel 共享同一强制展开机制**，
   但从定义良好的调用点发起，零信号。
3. **框架级毒化负控**（memcpy0 临时毒化，PR #336 同型：2 页 mmap 凿洞 +
   re-loop 顶部 MAP_FIXED 回填 + 紧接 report_fail_msg，未入库）：本机
   `-e memcpy0 -t 1000 -n 1` × 5 run + 全核 × 2 run，每次 run 恰 11 个
   `result: fail` 块（框架对失败测试固定重跑 11 次 = 原 issue「每 run 11 块」的
   口径来源），**0 crash**。crash 窗口为 SVE 主机时序特有，本机不可复现
   ——这是如实声明的验证边界；修复正确性不依赖本机复现 crash，而依赖
   「按构造消除任意边界展开」（POSIX/glibc 语义保证）+ 上述功能等价实证。

## 修复方案

**`pthread_exit(nullptr)` 替换三行取消舞步**（Linux 分支；`_WIN32` 的 `_endthread()`
不动）。论证：

- 语义等价：glibc 中 pthread_exit ≡ `__do_cancel`（同一下游展开机制，同跑 cleanup
  handlers 与 C++ 析构、TSD 析构；反汇编证实）。`thread_runner` cleanup lambda、
  `SandstoneTestThread::start` 的 clear_tid、`run_threads_sequentially` 剩余 CPU 跳过
  等既有语义全部保持。
- 危险类按构造消除：不再有取消信号、不再有 ASYNC 类型、不再触碰取消状态——
  不存在「任意指令边界发起展开」的窗口。
- 附带修正（同一直触发的行为差异，fleet 复核后按实证改口径——内部语义
  清理，非用户可见缺陷修复）：test_init 在 aux 线程内经 report_fail 失败时，
  旧路径 `pthread_join` 得 `PTHREAD_CANCELED(-1)`，与「负返回值=SKIP/-errno」
  约定冲突 → run_one_test_inner 判 Skip、child 以 255 退出（strace 实证：
  11 个 child 全部 `exit_group(-1)`）、且确实调用了 log_skip("Unexpected OS
  error")。**但**该误分类不出现在最终输出：logging_print_results 的
  AbstractLogger 线程扫描见 main 线程 fail 标记（report_fail_common 首行
  logging_mark_thread_failed(-1) 所置）把 testResult 救回 Failed（retest
  循环因此照常重试出 11 块），而 "Unexpected OS error" 消息被
  `skipInMainThread` 门（logging.cpp SkipMessages 分支）压掉、永不打印。
  新路径 join 得 nullptr → ret=0 → 直接落 `has_failed()` 判 Failed、child
  以 1 退出。用户可观察差异仅两处：child exit code 255→1（forkfd 内部，
  不可见）与 `time-to-fail` 数值→null（旧值来自被救回路径的线程扫描，
  null 与 init 返回码失败路径 ret>0 的既有行为一致，非回归）。
- x86-64 非回归（规则 5）：改动点无架构分支（`#ifdef _WIN32` 之外双架构共享同一
  行）；glibc x86-64 的 pthread_exit/cancel 同源同机制；取消语义变化对双架构等价
  ——线程仍终止、展开仍跑析构，仅发起点从任意边界变为定义点。属「真正共享」的
  路径，不涉及 `#ifdef __x86_64__` 扩张。
- 同步更新 `run_threads_sequentially` 处提及 pthread_cancel 的过时注释（其描述的
  正是被改函数）。

不采用的备选（记录理由）：
- DEFERRED + `pthread_testcancel()`：同样可行（展开发起点也变为定义点），但保留了
  取消信号与取消状态变更两个多余活动件；glibc 下游机制与 pthread_exit 完全相同，
  无增益。issue #345 给出的另一支「显式 pthread_exit」在 glibc 上正是本方案。
- 纯 `pthread_exit` 之外的「置标志 + 正常返回」：要求 report_fail 可返回，破坏全部
  ~680 个测试依赖的 noreturn 契约（sandstone_run.cpp:752-753 注释即此契约），
  改动面不可接受。

## 验证清单（100% 真实输出）

1. 构建：`./scripts/build.sh` 零新警告（基线已验 0 警告）。
2. 毒化负控（临时补丁不入库）：修复前（pristine+毒化）本机 `result: fail`（11 块），
   修复后（fix+毒化）仍 `result: fail`（11 块）且 -vv 下 per-thread 状态等价
   ——「fail 不因修复退化/变化」。
3. 正常 fail 路径：`--test-the-test` 自测（若本机可用）或既有失败语义用例。
4. 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
5. x86-64：检视论证（无架构分支 + glibc 同源机制），本机无法跑 x86-64，如实声明。

## 验证实录（2026-11-08，本机鲲鹏 920 无 SVE / glibc 2.38 aarch64）

毒化载体 memcpy0（临时补丁，验证后已 `git checkout` 回退，不入库）；毒化形状 =
PR #336 同型（2 页 mmap 凿洞 + re-loop 顶部 MAP_FIXED 回填 + 紧接
`report_fail_msg`）。框架对失败测试固定重跑 11 次 → 每 run 11 个结果块
（= issue #345「每 run 11 块」的口径：SVE 主机 11 块全 crash，本机 11 块全 fail）。

| 场景（-e memcpy0） | 修复前 | 修复后 |
|---|---|---|
| 并行调度 `-t 1000 -n 1`，mmap+fail ×5 run | 11×`result: fail`，0 crash（每 run） | 11×`result: fail`，0 crash |
| 并行调度 `-n 1`，纯 fail（无 mmap） | 11×fail，0 crash | 11×fail，0 crash |
| 并行调度 `-n 1 -vv`，mmap+fail | `state: failed`×11 | `state: failed`×11；归一化
  （剔除 time-to-fail/test-runtime/seed/log_file）后 diff 仅剩时间数值，结构逐行一致 |
| 顺序调度（临时 `.flags = test_schedule_sequential`）`-n 2` | 11×fail，0 crash，仅 `- thread: 0`
  （thread 1 未跑——失败跳出循环语义保持） | 11×fail，0 crash，仅 `- thread: 0` |
| 全核（64 线程）顺序调度 `-t 3000` | 11×fail，0 crash | 11×fail，0 crash，仅 `- thread: 0` |

- 消息落盘：每块均含 `E> Failed at ../tests/cpu/memory/memcpy0.cpp:46: POISON
  issue-345: ...`（检出不丢失，与 issue 描述一致）。
- 独立复现器（/tmp/repro345/repro.cpp）：ASYNC 自取消 / DEFERRED+testcancel /
  pthread_exit 三模式各 ≥50-2000 次线程迭代，cleanup 析构 100% 执行、0 崩溃。
- 构建：`./scripts/build.sh` 修复前后均 0 警告（含毒化/回退全部中间态）。
- 回归（毒化回退后、最终二进制）：`-e zstd19 -t 2000 -n 1` → `exit: pass`；
  `-e mce_check -t 2000 -n 1` → `exit: pass`（顺序调度 + 必须通过项）；
  `-e memcpy0 -t 2000 -n 1` → `exit: pass`。
- x86-64 非回归（检视论证）：改动点在 `#ifdef _WIN32` 之外无架构分支，双架构
  同一行代码；glibc x86-64 的 `pthread_exit` 与 cancel 同源共享 `__do_cancel`
  强制展开（本修复不引入任何架构差异）；x86-64 构建未在本机执行（无 x86-64
  环境），如实声明——但该路径不依赖任何 aarch64 专有语义。

## 验证边界（如实）

- SVE 主机上的 crash 本身无法在本机复现（77 次取消事件 0 crash）——修复「消灭
  crash」的直接证据来自机制构造（ASYNC 任意边界展开窗口被整体移除）+ issue #345
  的 SVE 主机证据，非本机复现-消除对照。
- gem5-fi 不适用：本缺陷是 glibc 信号/强制展开语义，gem5 SE 无同源 pthread 取消
  机制，仿真无意义；受影响行为（report_fail→线程终止→verdict）在本机可完整真机
  运行，无能力缺失。

## Fleet 复核轮（2026-11-09，localhost0102 无 SVE 920 / glibc 2.38 aarch64，独立复验）

PR #582 fleet 检视-验证 agent 的独立复验（不复用原作者工件；自有复现器
/tmp/repro345/repro2.cpp + 自有毒化形状）：

1. **微实证（2000 线程迭代 × 3 模式 × mmap 扰动开/关）**：async 自取消 /
   pthread_exit / DEFERRED+testcancel 三模式 cleanup 析构全部 100% 执行、
   0 崩溃；join retval 语义：取消类=PTHREAD_CANCELED、pthread_exit=nullptr。
2. **反汇编主张复核**：`objdump -d /usr/lib64/libc.so.6` pthread_exit——
   存 result → ldxr/stxr 原子置 cancelhandling 位 #0x10 → 调 unwinder 强制
   展开，与计划文档记载一致。
3. **评论勘误（本轮 fix）**：实测 glibc 2.38 aarch64 上 DISABLE 创建者的
   新线程取消状态为 **ENABLE/DEFERRED（POSIX 默认，无继承）**——原注释
   "workers inherit PTHREAD_CANCEL_DISABLE" 前提为假，已改述为
   「pthread_exit 与取消状态无关 + glibc 不继承创建者状态」。（不影响修复
   正确性：旧代码本就显式 ENABLE，新代码 pthread_exit 不看取消状态。）
4. **框架级毒化负控（fix 后）**：memcpy0 run 路径毒化（2 页 mmap 凿洞 +
   re-loop 顶 MAP_FIXED 回填 + 紧接 report_fail_msg）——`-t 1000 -n 1` × 5
   run、全核 `-t 3000` × 2 run：每 run 恰 11 块 `result: fail`、0 crash、
   消息 11/11 落盘；`-vv` 11×`state: failed`；临时 `test_schedule_sequential`
   + `-n 2`：11×fail、0 crash、仅 `- thread: 0`（失败跳出循环语义保持）。
5. **旧代码同型对照（pristine main + 同毒化）**：run 路径 ×3 + 全核 ×1 同为
   11×fail、0 crash——印证 crash 窗口为 SVE 主机时序特有、本机不可复现；
   init 路径毒化：strace 实证 11 child 全 `exit_group(-1)`（=EXIT_SKIP 误分类
   真实存在于 run_one_test_inner 层），但最终 YAML 仍 `result: fail`（被
   AbstractLogger 线程扫描救回）、无 "Unexpected OS error" 输出（被
   skipInMainThread 门压掉）——据此改写上方「附带修正」口径。
6. **回归（干净树重建后）**：`-e zstd19 -t 2000 -n 1`、`-e mce_check -t 2000
   -n 1`、`-e memcpy0 -t 2000 -n 1` 全部 `exit: pass`；build.sh 零新警告。
7. **x86-64 非回归**：检视论证同前（无架构分支、glibc 双架构同源 __do_cancel）；
   本机（localhost0102，非 SVE 主机 0104）无 SVE 载体，SVE 主机毒化对照未做，
   如实声明。

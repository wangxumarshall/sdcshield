# memcpy1_neon stderr 崩溃修复 — 计划

**Goal:** 修复 2026-11-11 战役发现的 rc=139 崩溃链（266 测试战役唯一失败）：
`memcpy1_neon` 逐迭代 `fprintf(stderr)` → GB 级 stderr memfd → 框架
`print_child_stderr_common` 打印路径 SIGSEGV（父进程，测试本体已 pass）。

**Root cause（两层，2026-11-11 12:54 实证）:**
1. `tests/cpu/neon_memcpy_cache/memcpy1_neon.cpp:46` — 无条件逐迭代
   `fprintf(stderr, "…Iteration %lu, src first 8 bytes…")`（2026-11-02 移植
   调试遗留；同族 memcpy0_neon 无此打印）。三重危害：GB 级 stderr、
   I/O 拖垮激发强度（实测 avg-freq 23.7MHz）、触发框架缺陷。
2. `framework/logging.cpp` `print_child_stderr_common`(:1687)/
   `LogMessagesFile`(:357) — 崩溃现场 `size=4294261107`（≈4GB）但映射在
   data+58KB 处即结束 → `print_content_indented`(:1511) 的
   `strnchr(line,'\n',end-line)` memchr 扫出界。映射与 size 的背离机制
   待用 12:54 coredump（/tmp/core_m1n.dump）`info proc mappings` 定案
   （疑 memfd 动态增长与构造期 mmap 长度在 GB 尺度不一致）。

**Fix（两单元两提交）:**
- 单元 1（测试）: 删 fprintf/snprintf/iteration 计数与 `<cstdio>`
  （失败上报已由 `report_fail_msg` 正规承担）。
- 单元 2（框架）: `print_child_stderr_common` 对超限 stderr 安全转储——
  钳制上限（1MB）+ 超限走 pread 读取（绕开 mmap 长度语义不确定区）+
  截断注记（真实字节数）。机制定案后评估是否可最小化。

**Verification (rule 2):**
- 机制定案: gdb 既有 coredump（帧 4 的 `f.size/f.base` + `info proc mappings`）
- clean build 零新警告（builddir3，新分支 off main 4d2031e1）
- 框架钳制验证: 本地 scratch spam 测试（不入库，验证后移除+增量重链）
- `-e memcpy1_neon -t 300000` 全核 → pass + stderr 体积正常（对照 12:54 崩溃）
- 回归 `-e zstd19 -t 2000 -n 1` → exit: pass
- x86-64: logging.cpp 为共享框架代码，stderr < 上限时行为零变化（by
  inspection）；修复对 x86 同为保护

**Branch:** `fix/memcpy1-neon-stderr-20261112` off main；可靠后推送 GitHub。
**不做（另开 issue 跟踪）:** 子进程 stderr 无上限捕获的 RAM 占用
（memfd 满速全核可至数百 GB——本机 381G 有 OOM 风险，需捕获期钳制）。

**验证结果（2026-11-12 实录）:**
- 宏：独立 C 探针（本机 64KB 页）新宏 ROUND_UP_TO_PAGE(4GiB+1)=4295032832 ✓ / 旧宏 65536 ✗
- 端到端：scratch 载体（fracture_loop_count=-1 单回合）3min spam 累积 **356GB** stderr memfd，
  -o 日志恰 1,051,610 字节 + "356457130690 bytes total" 截断注记，exit: pass 无崩溃
- zstd19 回归 pass；memcpy1_neon 30s sanity pass、日志 1833 字节零打印
- 过程发现：fracture 多回合下每回合转储天然 <1MB（2s 内 1463 微回合 × 157KB）——钳制不触发
  但累计总量无界；-v 下子进程 stderr 直通继承 fd（非 memfd 路径）
- 遗留 issue：memfd 捕获期无上限 RAM 占用（实测单回合 3min ~350GB）
- 单测：本机 gtest 运行库缺失（rootfs 仅悬空软链），RoundUpToPagePast4GiB 由 CI 运行，
  本地以独立探针等效验证

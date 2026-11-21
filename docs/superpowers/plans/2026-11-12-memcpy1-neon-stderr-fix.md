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
（memfd 满速全核可至数百 GB——本机 381G 有 OOM 风险，需捕获期钳制；
跟踪 issue 已于 2026-11-18 建立：**#843**）。

**验证结果（2026-11-12 实录）:**
- 宏：独立 C 探针（本机 64KB 页）新宏 ROUND_UP_TO_PAGE(4GiB+1)=4295032832 ✓ / 旧宏 65536 ✗
- 端到端：scratch 载体（fracture_loop_count=-1 单回合）3min spam 累积 **356GB** stderr memfd，
  -o 日志恰 1,051,610 字节 + "356457130690 bytes total" 截断注记，exit: pass 无崩溃
- zstd19 回归 pass；memcpy1_neon 30s sanity pass、日志 1833 字节零打印
- 过程发现：fracture 多回合下每回合转储天然 <1MB（2s 内 1463 微回合 × 157KB）——钳制不触发
  但累计总量无界；-v 下子进程 stderr 直通继承 fd（非 memfd 路径）
- 遗留 issue：memfd 捕获期无上限 RAM 占用（实测单回合 3min ~350GB）→ **#843**
- 单测：本机 gtest 运行库缺失（rootfs 仅悬空软链），RoundUpToPagePast4GiB 由 CI 运行，
  本地以独立探针等效验证

**复核附录（2026-11-18，pr-issue-triage triage 实录，GCC 10.3.1 / openEuler 22.03 主机）:**
- pread 重写路径端到端（scratch 载体 1.2MB stderr，验证后已移除）：`--full-log` 下失败/通过均
  转储前 1 MiB + `[stderr truncated: 1228800 bytes total, first 1048576 bytes shown]` 注记，
  父进程存活；compact 默认模式下失败测试同样转储带注记，pass 折叠不转储（main 新增
  compact 日志的既有语义，非本 PR 行为）。
- 机制定案披露：12:54 coredump（/tmp/core_m1n.dump）已不存在，「映射与 size 背离」永久无法
  从该证据定案。两支候选现场如实并存：(a) 本计划记录的 4294261107（<2^32）+ 映射止于 +58KB——
  探针实证旧 32 位掩码**不**截断该值（取整为 4 GiB），宏截断解释不了这支现场；(b) >4GiB →
  低 32 位（4.14 GiB → 143.4 MiB）——宏截断自洽现场。转储路径已改 pread 头读取（不再 mmap），
  两支现场下均健壮（EOF 容忍 + 截断注记如实报告实读字节数）。三处代码注释
  （sandstone_page.h / logging.cpp / 单测）已去「observed 定案」口吻。
- meson native 池注入 A/B 实证（本机 GCC 10.3.1 无 <barrier>，探测块触发）：
  修复在——`ninja unittests` 构建零告警通过，`RoundUpToPagePast4GiB` PASS；
  修复摘除——`fatal error: barrier: No such file or directory`（topology_cpu.h:16），
  2026-11-12 的「observed」声称就此以当日 A/B 复现坐实。
- 回归：`-e memcpy1_neon -t 30000`（全核与 -n 1）exit: pass、日志 975 字节零打印；
  `-e zstd19 -t 2000 -n 1` exit: pass。
- 残留特性（非缺陷、关联 #843 类）：转储钳制按 fracturing 回合计，每回合 1 MiB 上限，
  回合数无界则日志总量无界（对比修复前每回合可 GB 级 memfd 已收 1000×）；实测 2s 内
  328 微回合 × 1 MiB ≈ 344 MB 日志（scratch 载体极端形态）。

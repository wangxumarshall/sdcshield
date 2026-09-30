# Plan: 狩猎 v3 两 SDC fail 审计修复（P1）+ movbe SLF 多宽度扩展测试（P2）

- Date: 2026-09-27 | Branch: `feat/sve-port-avx53` | Status: approved
- P2 scope changed by user instruction 2026-09-27: `movbe_dump_probe_c_sve` **原样保留**（122 触发配方 + 证据链，一个字节不动），改进与扩展全部放进**新建测试**。

## Background（审计结论，已核实）

狩猎 v3（2026-09-24→09-27，67h，全核 116 项，122 封顶 1.45 GHz）产生两个 fail：

1. **movbe_dump_probe_c_sve → CPU 122，真 SDC 成立**。本机（VL=256）无逻辑缺陷：`priv_input` 线程私有、golden（`data->input`）init 后只读、svtbl [3,2,1,0] 对合。3/3 次尝试 128 线程仅 bit122 单点命中 + 第 3 次 actual=0x995B001D 恰为 bswap(golden)=一次交换签名（store→load 转发失效）+ 1、2 次多位垃圾。SIGSEGV 在子线程失败清理路径 `thread_runner`（sandstone_run.cpp:753，`sApp->shmem` 为垃圾值 0x7e4f9400064），与 insert_extract 的父进程 libc SIGSEGV 不同源，非框架通病。
2. **insert_extract_sve → CPU 7，假阳性（软件竞态），FAIL 撤销**。`tests/cpu/sve/vector2/insert_extract_sve.cpp:91-93` store/load 子检查读写 `data->mem_buf`，`test->data` 单实例被全部 128 线程共享零同步 → 线程间覆写误报。决定性日志：`Iter 25, lane=1, insert_val=-30.79, extracted=-30.79, result=FAIL` —— 无竞态的 insert/extract 子检查（线程私有栈）干净，失败恰在有竞态的共享缓冲检查上。竞态自原版 NEON `tests/cpu/vector/insert_extract.cpp` 继承。次要缺陷：`std::mt19937(std::random_device{}())` 绕过框架 RNG（不可复现）、每迭代 `fprintf`+ANSI 色+`fflush`（~1000× 减速、4GB 日志）、non-aarch64 路径裸 `EXIT_SKIP` 无 `log_skip`（违反 placeholder 诚实规则）。

movbe_dump_probe_c_sve 审计发现的**潜在缺陷**（本机未触发，与 122 失败无关）：(a) `in_bytes[64]/out_bytes[64]/restored[16]` 按 16 元素定容而 `svcntw()` 可达 20（VL=640）以上 → **首个溢出 VL=640**（VL=512 时 svcntw()=16 恰好装满；本机 VL=256→lanes=8 安全）；(b) `data->swapped` 64KB 死分配。→ 由 P2 新测试修正（不动原文件）。（注：阈值 640 为 Task 2 评审纠正，初稿误写 1024。）

覆盖面查重：`mem/partial_store_forwarding_sve`（1/2/4/8B 部分宽度 store+load 命中 store-buffer）与 `mem/lsu_store_forward_sve`（LSU 全转发场景）已覆盖标量宽度失配；movbe/core179 家族全部为 bswap32 → 新测试的非重复定位 = **多元素宽度（16/32/64-bit）bswap 往返过 SLF 路径**。

## Global Constraints（对每个 unit 绑定）

1. One patch per unit：P1、P2 各一个 commit，各自完成全部验证后才 commit。
2. **红线**：`tests/cpu/sve/core179/` 现有文件一个字节不改（P2 只新建文件 + 在 meson.build 增加一行注册）。
3. 自验证 100% 真实命令、引用真实输出；任何一步失败不得 commit（CLAUDE.md）。
4. x86-64 非回归：所有改动位于 `tests/cpu/sve/`（meson 里 `if host_machine.cpu_family() == 'aarch64'` 门控内），x86 构建零影响。
5. 每个 unit 验证通过后自动 push 到 `feat/sve-port-avx53`（CLAUDE.md 常设授权），绝不 push main。
6. Commit message 不得以 `Co-Authored-By: Claude` 结尾。
7. 122 协议：本计划的验证一律排除 122（`-n 1` 或 `--cpuset='!122'`）；任何含 122 的运行需用户明示裁决。
8. 框架 API 事实（已核实）：`random32()`（sandstone.h:571）、`frandomf_scale(float scale)` 返回 [0,scale) 正浮点（:582）、`report_fail_msg` 为 `__attribute__((noreturn))`（:458，调用后无需 return）、框架 RNG 为每线程流（线程安全且 `-s` 可复现）。

## Task P1: 修复 insert_extract_sve 竞态与可复现性

File: `tests/cpu/sve/vector2/insert_extract_sve.cpp`（单文件，唯一 commit）

保留（语义不变）：测试名/描述、`.groups = DECLARE_TEST_GROUPS(&group_math)`、`TEST_QUALITY_PROD`、`IE_LANES 4`、init 的 HWCAP_SVE 门控（含 log_skip 文案）、svsel 单 lane 谓词插入机制、svld1_f32+`svcmpeq_f32`+`svcntp_b32` 的 store/load 一致性检查、`do { ... } while (test_time_condition(test));` 循环形状。

修改（全部）：
1. **竞态修复**：store/load 子检查改用函数局部线程私有栈缓冲 `float sl_buf[IE_LANES];`；删除 `struct test_data_sve`、`test->data` 机制（init 只留 HWCAP 门控；`test_cleanup` 字段整个删除——NULL 合法）。`svst1_f32(pg, sl_buf, orig_vec)` 与 `svld1_f32(pg, sl_buf)` 之间插入 `__asm__ volatile("" ::: "memory");` 阻断编译器消除真实 reload（先例：movbe_dump_probe_c_sve.cpp:103）。
2. **RNG 换框架每线程流**：删 `std::mt19937`/`std::uniform_real_distribution`；数值 = `frandomf_scale(200.0f) - 100.0f`（[-100,100)，与原 dist 范围一致）；lane = `(int)(random32() % IE_LANES)`。种子从此可复现。
3. **删每迭代日志**：删 `static std::atomic<uint64_t> iter`、`fprintf`/`fflush`/ANSI 色码。失败时 dump-on-failure：`log_error` 打印 lane、insert_val、extracted、orig_vals[4]、new_vals[4]（区分两类子检查失败），随后 `report_fail_msg("insert_extract_sve: insert/extract or store/load consistency failure")`（noreturn，无需 return）。
4. **placeholder 诚实**：non-aarch64 run 桩补 `log_skip(CpuNotSupportedSkipCategory, "to be implemented (placeholder): ARM SVE required for insert_extract_sve")` + `return EXIT_SKIP`。
5. **清理**：删多余 include（`<random>`、`<atomic>`、`<cstdio>`、`<cstring>`——新代码不再用 memset/memcpy），保留 `<cstdint>`；删 init 里的死代码 `std::mt19937 rng(...); (void)rng;`。

验证（逐条真实输出）：
- `ninja -C builddir` 零新告警/错误
- `./builddir/sdcshield -e insert_extract_sve -t 5000 -n 1` → pass（单线程基线）
- `./builddir/sdcshield -e insert_extract_sve -t 60000 --cpuset='!122'` → 127 线程 60s 全 pass（竞态门：旧代码此场景必 fail）
- `./builddir/sdcshield -e zstd19 -t 5000 -n 1` → pass（回归）
- x86 非回归：该文件仅在 aarch64 门控的 `sve_avx53_sources` 中注册（tests/cpu/sve/meson.build:116），diff 无共享路径改动

## Task P2: 新建 movbe_slf_sweep_sve（多元素宽度 SLF 往返探针）

Files: 新建 `tests/cpu/sve/core179/movbe_slf_sweep_sve.cpp` + `tests/cpu/sve/meson.build` 在 core179 块（:145-165）末尾追加一行 `'core179/movbe_slf_sweep_sve.cpp',`（唯一 commit；现有探针文件零改动）

设计（继承 probe-c 的 SLF 语义 + 三项改进）：
1. **SLF 核心语义继承**（movbe_dump_probe_c_sve 的触发配方）：每线程私有缓冲 → svtbl 字节重排 → `svst1` 存回原位置（直打堆无栈中转）→ `__asm__ volatile("" ::: "memory")` → `svld1` 立即重读同一地址（真实 store→load 转发）→ svtbl 还原 → 与 golden（init 后只读的 `data->input`）逐位比对。dump-on-failure：index/width/golden/actual/xor + 字节分解，随后 `report_fail_msg`（noreturn）。
2. **改进 A — 多元素宽度**：外层每遍在三种元素宽度间轮换扫描：16-bit（每元素 [1,0]）、32-bit（[3,2,1,0]）、64-bit（[7,6,5,4,3,2,1,0]）。统一机制：元素宽 W 字节、字节位 p 的索引表项 = `(p / W) * W + (W - 1 - (p % W))`（元素内字节反转，对合）。init 时对三种宽度各用 N 个随机向量对照 `__builtin_bswap16/32/64` 自检（bit-exact，任一不符即 init fail），沿用家族 50k 随机向量验证的先例。
3. **改进 B — VL 可移植**：所有栈缓冲按 SVE 架构最大 VL=2048bit=256 字节定容（`uint8_t buf[256]` 级别），运行时按 `svcntb()`/`svcntw()`/`svcntd()` 对应宽度的 lanes 分批 + `svwhilelt` 谓词处理尾部——封死原探针 VL≥1024 的栈溢出缺陷，任何 VL 下行为正确。
4. **改进 C — 无死分配**：不引入 `swapped` 之类的未用缓冲；`test->data` 只含必要字段；cleanup 释放全部分配。
5. 数据：init 用 `random32()` 填充 64KB 缓冲（16384 u32 / 8192 u64，按字节扫描与宽度解耦）；每线程 `thread_local` 私有副本，每遍扫描前 memcpy 重置（store-back 会污染，同 probe-c:86-87）。RNG 全部走框架流，`-s` 可复现。
6. 门控与声明：init 首查 HWCAP_SVE，无 SVE 则 `log_skip(..., "to be implemented (placeholder): ARM SVE required for movbe_slf_sweep_sve")` + `EXIT_SKIP`；non-aarch64 run 桩同样 log_skip；`DECLARE_TEST(movbe_slf_sweep_sve, ...)` 无 `.groups`（与 core179 家族一致）、`TEST_QUALITY_PROD`；注释块 `@test @b` 风格与家族一致并注明“extends movbe_dump_probe_c_sve: multi-width (16/32/64) SLF round-trip, VL-portable, no dead allocations”。

验证（逐条真实输出）：
- `meson setup --reconfigure builddir`（meson.build 改动）+ `ninja -C builddir` 零新告警
- `./builddir/sdcshield --list-tests | grep movbe_slf_sweep_sve` → 出现于 PROD 列表
- `./builddir/sdcshield -e movbe_slf_sweep_sve -t 5000 -n 1` → pass
- `./builddir/sdcshield -e movbe_slf_sweep_sve -t 60000 --cpuset='!122'` → 127 线程 60s 全 pass
- `./builddir/sdcshield -e zstd19 -t 5000 -n 1` → pass（回归）
- x86 非回归：新文件 + meson 行均在 aarch64 门控内
- 122 两阶段验证（离线 122 → 基线 → 在线 122）：**待用户裁决，本 unit 不执行**

## Deferred（待用户裁决，本轮不执行）

- P3: 原版 NEON `tests/cpu/vector/insert_extract.cpp` 同源竞态修复
- P4: `tests/cpu/sve` 其余测试共享可写 `test->data` 模式清查（只读 grep 清查）；家族级分配加固（priv 的 aligned_alloc / malloc 空指针检查，probe-c:51/:82 同款继承模式）也归入此类
- movbe_slf_sweep_sve 含 122 的两阶段验证。**解读警示（最终评审 Minor #4）**：sweep 是相关配方而非 probe-c 的位等价触发器——probe-c 在 svst1 前用标量 memcpy 将原始字节写回同地址（存储缓冲预置），且首次向量加载经过栈中转；sweep 是直连堆加载→svtbl→svst1→屏障→svld1。**"sweep 在 122 上 pass 而 probe-c fail" 不构成对 122 定性的反驳**；两阶段验证时应同场同跑两者，配对结果才有信息量
- 4 枚 coredump 的 gdb 深挖（movbe SIGSEGV 指针污染定性）

# Issue #318: cache_stress_aggressor ARM64 移植性修复 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 cache_stress_aggressor 的 ARM64 缓存压力原语换成真正的 `dc civac`（x86 clflush 等价物），并用 asm 边界恢复被编译器架空的 victim 回读校验，使测试真正产生数据缓存逐出压力且校验可信。

**Architecture:** 单文件单 unit 修复。aggressor 的 `__builtin___clear_cache`（I-cache 维护原语，emit `dc cvau`+`ic ivau`，不失效 L1 行）换成仓内已有四处先例的 `dc civac + dsb sy` 内联汇编；victim 的 `vld1q_f32` 回读因 -O3 store-to-load forwarding 被编译器消除（实证 `fcmeq v1,v1` 恒真），按 neon_rot_2src.cpp:47 同一修法改为 asm `ldr %q0`。同步修正两处实验笔记文档中的过时/错误表述。

**Tech Stack:** C++20 / ARM64 内联汇编 / GCC 12.3.1 -O3 / meson+ninja。

**Spec:** GitHub issue #318（正文见 `gh issue view 318`）；根因分析见本 plan 的「根因结论」。

## 根因结论（批判性分析，全部有实证）

issue 报告的方向正确（原语选错、测试在 ARM64 上不可信），但机制归因需按证据修正：

1. **原语选错 — 成立（issue 核心主张）**。`tests/cpu/cache/cache_stress_aggressor.cpp:26-33` 的 `clflush_arm()` 用 `__builtin___clear_cache` 充当 x86 `clflush` 替代。反汇编实证（本机 GCC 12.3.1 -O3，aggressor_thread 循环体）：`bl __clear_cache`（libgcc 调用，内部为逐行 `dc cvau` + `dsb ish` + `ic ivau` + `dsb ish` + `isb`）+ `dmb ish`。注意 issue 说 "GCC 会 emit dc civac" 不准确——实际是 **cvau**（clean 到统一点，**不失效** L1 行）。后果：victim 的 L1 副本永不被逐出，load 永远命中 L1，测试名义上的 "aggressive CLFLUSH" 数据面逐出压力**从未产生**；`ic ivau` 部分是对数据线做 I-cache 维护，纯属无关副流量。D-tests.md:114 自承此为"存疑的近似"。
2. **issue 声称的 race 机制 — 在当前 main 不成立**。victim 是**单写者**（93e4aa30，2026-10-17，per-worker `thread_local` slot）：同线程同地址 store→load 在 AArch64 上按程序序自我可见（架构保证），缓存维护指令（cvau/civac）不丢数据（clean 先写回再失效）。aggressor 只做维护不做数据写。故当前代码结构上不可能产生 act≠ref。
3. **决定性证据 — 当前 release 构建的校验已被编译器架空（本分析新发现）**。victim_thread 反汇编：`fcmeq v0.4s, v1.4s, v1.4s`（寄存器自比，恒真）——`vld1q_f32` 被 store-to-load forwarding 消除，循环内**没有任何从 slot 的 load**（只有 `str q1, [x20]`）。当前二进制在校验路径上不可能失败 = **假阴性**：什么都检不出。neon_rot_2src.cpp:40-43 注释记录过同一问题与修法。
4. **g002 lane 互换日志的归属**。`act=[ref_lo, other_hi]` 是**两个写者**在同一槽上拆分的 128-bit store 交错（非 single-copy atomic 的两半错叠）的签名，即 2f4a6ec9..93e4aa30^ 之间共享 `victim_value` 时代的二进制（2026-10-16 战役事件 #001 同款）；且 g002 二进制的回读未被 forward（非本仓标准 -O3 构建），方能打印出该 mismatch。当前 main 的标准 release 构建无法复现（第 3 条）。
5. **顺带发现（不在本 unit 修，另行跟进）**：victim 是裸 `std::thread`，其 TLS `thread_num` 零初始化为 0（framework/sandstone.h:642），`frandomf_scale` → `rng_for_thread(0)` → 所有 victim 线程并发竞争 framework 0 号线程的 RNG 槽 `rngs[1]`（framework/random.cpp:118-131）——潜在数据竞争（只影响取值质量，不影响 slot 单写者校验健全性）。

**修复后为什么不需要 victim 侧内存屏障（issue 菜单之一，拒绝理由）**：校验健全性只依赖同线程同地址序（架构保证，与弱内存模型无关——弱序只影响跨线程观察）+ 缓存维护不丢数据。加 `dmb ish` 不改变任何语义，只降吞吐，属 cargo-cult。issue 观察到的失败模式（lane 互换）需要第二个写者，已由 93e4aa30 消除。

## Global Constraints

- 一个 patch 一个 unit：本 plan 全部改动 = 一个 commit（代码 + 文档同步）。
- x86-64 untouched：所有代码改动在 `#ifdef __aarch64__` 内；x86 路径（EXIT_SKIP）不动。
- 零新增警告：`SDC_BUILD_JOBS=20 ./scripts/build.sh`（共享服务器，禁满核）。
- 提交 `git commit -s`，最后一行且仅一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，禁止 Co-Authored-By；push feature 分支，绝不 push main。
- `-t` 单位是**毫秒**：`-t 180000` = 180 秒（CLAUDE.md 写明 `-t 5000` = 5 s）。
- 本机（SVE1 256-bit Kunpeng 128 核）可完整验证该测试，无需 gem5-fi（无 SVE/SME tier 依赖）。

---

### Task 1: 代码修复（clflush_arm → dc civac；victim 回读 → asm 边界）

**Files:**
- Modify: `tests/cpu/cache/cache_stress_aggressor.cpp:26-33`（clflush_arm 实现+注释）
- Modify: `tests/cpu/cache/cache_stress_aggressor.cpp:53-56`（victim 回读）
- Modify: `tests/cpu/cache/cache_stress_aggressor.cpp:147`（DECLARE 描述串）

**Interfaces:**
- Produces: `static inline void clflush_arm(void *ptr)` 签名不变，body 变为 `dc civac + dsb sy` 内联汇编；victim 循环产出真实的 str q→ldr q 内存往返。

- [ ] **Step 1: 替换 clflush_arm**

将：

```cpp
// ARM 缓存行冲刷（将指定地址的缓存行写回并无效）
static inline void clflush_arm(void *ptr) {
    // 使用内联汇编执行 DC CIVAC（数据缓存清空并无效）
    // 注意：需要特权级别，若在用户空间可能无效，需通过 __clear_cache 等
    // 这里使用 __builtin___clear_cache 作为通用替代（会清空指令缓存，但对数据也有效）
    __builtin___clear_cache((char*)ptr, (char*)ptr + CACHE_LINE_SIZE);
    // 内存屏障确保冲刷完成
    __sync_synchronize();
}
```

替换为（注释逐条纠正旧注释的两个事实错误——"需要特权级别" 与 "对数据也有效"）：

```cpp
// x86 clflush 的 ARM64 等价原语：dc civac（按虚拟地址 clean & invalidate 到
// 点一致性 PoC）——脏行先写回再失效，下次访问强制从 L2/L3 重取，产生真实
// 的数据缓存逐出压力。按 VA 的缓存维护指令 EL0 用户态可直接执行（无需特
// 权；仓内先例 l2c_cross_cache_line_arm.cpp:165、movbe_l1d_vs_l2_sve.cpp:192
// 均为用户态执行）。地址须缓存行对齐（Kunpeng 920 上非对齐 dc civac 触发
// BUS_ADRALN，见 lsu_store_forward_sve.cpp 注释）；本测试 slot 已 alignas(64)。
// dsb sy 确保维护指令完成后才进下一轮（覆盖旧实现的 __sync_synchronize）。
//
// 被替换的 __builtin___clear_cache 是 I-cache 维护原语（JIT/自修改代码用，
// 逐行 dc cvau + ic ivau）：cvau 不失效 L1 行，数据面零逐出压力（issue #318）。
static inline void clflush_arm(void *ptr) {
    __asm__ volatile("dc civac, %0\n\tdsb sy\n\t" : : "r"(ptr) : "memory");
}
```

- [ ] **Step 2: victim 回读改为 asm 边界**

将（`cache_stress_aggressor.cpp:52-56`）：

```cpp
        // 写入本线程对私有 victim 槽（独占缓存行）
        vst1q_f32((float*)slot, val);

        // 立即读取
        float32x4_t actual = vld1q_f32((float*)slot);
```

替换为：

```cpp
        // 写入本线程对私有 victim 槽（独占缓存行）
        vst1q_f32((float*)slot, val);

        // 立即读回。必须用 asm 边界：-O3 下编译器对 intrinsic 做
        // store-to-load forwarding，把回读消除成寄存器自比（objdump 实证
        // fcmeq v1.4s, v1.4s 恒真，循环内无任何从 slot 的 load）——校验退
        // 化为空转。neon_rot_2src.cpp 同一问题同一修法（asm 边界保证
        // str q → ldr q 背靠背落到二进制里）。
        float32x4_t actual;
        __asm__ volatile("ldr %q0, [%1]" : "=w"(actual) : "r"(slot) : "memory");
```

比较逻辑 `vceqq_f32(val, actual)` 不动——现在 actual 是真实内存回读值。

- [ ] **Step 3: DECLARE 描述串如实命名原语**

`cache_stress_aggressor.cpp:147`：

```cpp
DECLARE_TEST(cache_stress_aggressor, "Stress cache with aggressive CLFLUSH (NEON)")
```

改为：

```cpp
DECLARE_TEST(cache_stress_aggressor, "Stress cache with aggressive line invalidation (NEON, DC CIVAC)")
```

### Task 2: 构建 + 反汇编验证（修复前后对照）

**Files:**
- Build: `builddir/`
- Inspect: `builddir/tests/cpu/libtests_base.a.p/cache_cache_stress_aggressor.cpp.o`

- [ ] **Step 1: 构建**

Run: `SDC_BUILD_JOBS=20 ./scripts/build.sh`
Expected: 全绿，零新增警告。

- [ ] **Step 2: 反汇编断言（这是本修复的"测试"，修复前为 FAIL）**

Run:
```bash
objdump -d builddir/tests/cpu/libtests_base.a.p/cache_cache_stress_aggressor.cpp.o --no-show-raw-insn | awk '/aggressor_thread/{f=1} f'
```
Expected（aggressor 循环体）：出现 `dc civac, x..` 与 `dsb sy`，不再有 `bl <__clear_cache>`。

Run:
```bash
objdump -d builddir/tests/cpu/libtests_base.a.p/cache_cache_stress_aggressor.cpp.o --no-show-raw-insn | awk '/victim_thread/{f=1} f'
```
Expected（victim 循环体）：`str q.., [x..]` 之后紧跟 `ldr q.., [x..]`，且 `fcmeq` 的两个源操作数是**两个不同的寄存器**（修复前是 `fcmeq v0.4s, v1.4s, v1.4s` 同寄存器自比、无 ldr）。

### Task 3: 端到端运行验证

- [ ] **Step 1: 全核 180 秒（g002 同款配置）**

Run: `./builddir/sdcshield -e cache_stress_aggressor -t 180000`
Expected: `exit: pass`，stderr 无 mismatch dump。（修复前同命令已在取证中，预期同样 pass——因校验被架空不可能失败；修复后 pass 才是"有真实压力下仍健全"的证明。）

- [ ] **Step 2: 多 seed 置信**

Run: `./builddir/sdcshield -e cache_stress_aggressor -t 180000 -s 2`、`-s 5`、以及 `-t 60000 -n 8 -s 3`
Expected: 全部 `exit: pass`。

- [ ] **Step 3: 回归**

Run: `./builddir/sdcshield -e zstd19 -t 2000 -n 1`
Expected: `exit: pass`。

- [ ] **Step 4: x86 非回归目检**

确认 diff 全部位于 `#ifdef __aarch64__` 内；x86 路径（init/run 的 `#else` SKIP 分支）无改动。

### Task 4: 文档同步（issue 引用的两处实验笔记）

**Files:**
- Modify: `docs/paper/ref-paper-experiment-note/D-tests.md:114`
- Modify: `docs/paper/ref-paper-experiment-note/experiments/E11-ITHICA.md:323-325`

- [ ] **Step 1: D-tests.md 的"无 cache flush 数据面 helper"条目更新**

把「`__clear_cache` 是 I-cache 语义(cache_stress_aggressor.cpp:24-28 注释自承"对数据也有效"是存疑的近似);dc civac 只在 l2c_cross_cache_line_arm.cpp:163 内联 asm 局部使用」更新为：cache_stress_aggressor 已在 issue #318 修复中弃用 `__clear_cache` 改用 `dc civac` 内联 asm；dc civac 局部使用点列举（l2c_cross_cache_line_arm.cpp:163、movbe_l1d_vs_l2_sve.cpp:192、cache_stress_aggressor.cpp）；"未成公共 helper"现状保留。

- [ ] **Step 2: E11-ITHICA.md 事实纠错**

原文「`dc civac`(cache invalidate to PoC, 需内核或特权;用户态不可直接执行)」是**事实错误**（按 VA 的缓存维护指令 EL0 可执行，仓内多个测试即用户态执行；该错误表述正是当初误导移植选 `__clear_cache` 的源头）。改为如实表述并注明此纠错关联 issue #318。`__clear_cache` "只对 I-cache 保证"的表述本来就对，保留。

### Task 5: 提交、push、PR

- [ ] **Step 1: 确认分支**

Run: `git branch --show-current` → 若非 feature 分支则 `git checkout -b fix/issue318-cache-stress-aggressor-arm64`。

- [ ] **Step 2: 提交（单 commit，含代码+文档）**

```bash
git add tests/cpu/cache/cache_stress_aggressor.cpp \
        docs/paper/ref-paper-experiment-note/D-tests.md \
        docs/paper/ref-paper-experiment-note/experiments/E11-ITHICA.md
git commit -s  # 信息见下
```

commit message 主题：`test(cache): fix cache_stress_aggressor ARM64 portability — DC CIVAC + real reload verify (issue #318)`，正文含根因两条 + 反汇编证据 + 验证输出。

- [ ] **Step 3: push + PR**

```bash
git push -u origin fix/issue318-cache-stress-aggressor-arm64
gh pr create --base main  # 正文含根因、修法、验证证据原文、Fixes #318
```

- [ ] **Step 4: 检视循环**

派 feature-dev:code-reviewer 子 agent 批判检视；blocking findings → 修复 → push；循环至无 blocking。

- [ ] **Step 5: merge + issue 收尾**

```bash
gh pr merge <PR号> --merge
```
`Fixes #318` 自动关闭；如未关则 `gh issue close 318 --comment ...`。另开 follow-up issue 记录 RNG 槽竞争（victim 裸线程 thread_num=0 → 全体竞争 rngs[1]）。

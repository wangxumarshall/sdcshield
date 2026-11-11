# neon_rot_ldp 实施计划（fp64_rowmajor ko → sandstone 用例）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `tools/fp64_rowmajor_ko/fp64_rowmajor_ldp_stp.c`（内核模块形态的 fp64 row-major 旋转测试，访存已换 `ldp x,x`/`stp x,x`）改造成本树 sandstone 用例 `tests/cpu/misc/neon_rot_ldp.cpp` 并验证。

**Architecture:** 单文件新增 + meson aarch64 块内注册一行。骨架 = 本树 `neon_rot_ldr_at_top_rowmajor.cpp`（升/降序交替 sweep、top 丢弃 load、双 store、memcmp 冷路径），数据通路 = ko 的 pair 形态（`ldp xA,xB`/`stp xA,xB`，16B/条，lo/hi ↔ `v.d[0]/v.d[1]` 小端一一对应）+ `fp64_rot`（fadd/fsub/fmul/fmax，slot = i%4）。黄金 = **同路径 NEON 重算**（Task 1 探针实证裁决：C `fmax` 是 maxNum 语义与 FMAX 的 NaN 传播语义架构级不同；本机鲲鹏 920 的标量 FP 管道与 NEON 向量管道在双 NaN 输入的 payload 选择上分歧——双 NaN lane 无位等标量基准，任何标量黄金 = 舰队级假阳性；同路径 NEON 黄金构造上位可复现，与 ko `fp64_golden_init` 及主树 fp64 父测试约定一致；持续性 NEON 数据通路损坏由整数兄弟用例的 issue #373 标量黄金覆盖）。失败报告 = ko 的冷路径 A/B/T/G/E/X 紧凑消息（noinline 助手 + `report_fail_msg`，保留 "miscompare" grep 关键字）。

**Tech Stack:** C++20 / aarch64 内联 asm（ko 逐字移植）/ meson ARM64-only 块 / GCC 12（本机构建器）。

**Spec:** 用户指令（本会话 2026-11-11）+ CLAUDE.md 规则 1–7。ko 语义基准：`tools/fp64_rowmajor_ko/{fp64_rowmajor_ldp_stp.c,verify.sh,verify.log}`（2026-11-10 验证 16/16 PASS：内核 ldp/stp ≡ 用户态 ldr q/str q，六缓冲 FNV-1a 哈希逐位一致）；本树家族骨架：`tests/cpu/misc/neon_rot_ldr_at_top_rowmajor.cpp`；fp64 父参考（主树，同路径黄金——仅作报告格式参考）：`/home/sdc/wangxu/sdcshield/tests/cpu/misc/neon_rot_ldr_at_top_rowmajor_fp64_dump.cpp`。

## Global Constraints

- 一个单元一个补丁：新用例 + meson 注册 + 本计划文件 = 1 个 commit（repo 惯例 plans 与代码同 commit）。
- 自验 100% 真实：(a) `./scripts/build.sh` 零新警告；(b) 实跑引用真实输出；(c) 回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`；(d) x86 非回归 = meson 改动仅落在 `if host_machine.cpu_family() == 'aarch64'` 块内（逐行检查）。NEON 是 AArch64 基线 ISA、本机可跑 → 不触发 gem5-fi 条款。
- DCO：`git commit -s`，末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By；当前在 main → 先切 feature 分支（绝不推 main）。
- 文档同步：单测新增、家族已描述 → 非大颗粒度，README 不动（沿 2026-11-10 计划同判例）；文件头 docstring + DECLARE_TEST 描述串必写；计数变化（本机 PROD 439→440）在提交信息与最终回复中如实声明，README 时序链留 merge 期更新。
- 测量口径：实跑统一本机；单线程 `-n 1` 与全核都要过。

## Review Focus

1. **黄金相位/lane 映射错位**——run 与黄金 init 的向量 i 都必须用 `fp64_rot(i%4)`、同一对源向量；ldp 的 lo/hi 必须落 lane0/lane1（小端）。黄金 init 若用与 run 不同的 slot 映射或助手形态，memcmp 立即假报。→ Task 2 黄金循环与 run 逐行同构（同一 `fp64_rot`、同一 pair 助手）+ Task 3 实跑 memcmp 全绿。
2. **标量黄金在本机不可复现**（Task 1 已实证，非假设）：C `fmax` 为 maxNum 语义，与 `vmaxq_f64`（FMAX，NaN 传播）架构级不同；双 NaN 输入下标量管道与向量管道 payload 分歧（鲲鹏 920 实测），无双 NaN 位等标量基准。→ 裁决：同路径 NEON 黄金（ko/主树 fp64 父测试约定）；探针输出记录在案。
3. **pair 指令被编译器折叠或数据通路残留 `ldr q`/`str q`**（黄金/init 若误用 NEON 也会引入）。→ volatile asm 锁指令形态 + Task 3 objdump 数据通路点验（ko verify.sh 同款门禁：run 函数 ldr q=0、str q=0、ldp x>0、stp x>0）。
4. **降序 sweep 越界/漏访**——`i = ascending ? n : (COUNT-1-n)` 必须恰好覆盖 [0,COUNT) 全下标。→ 与家族逐行同构 + Task 3 实跑（黄金 memcmp 每 sweep 全量核对，漏访必报）。
5. **分配失败伪装成 SDC**——init 侧 `aligned_alloc` NULL 不得裸解引用。→ init NULL → `log_skip(RuntimeSkipCategory,...)` + `EXIT_SKIP`（ldr_rand 判例）；run 侧 temp/dst NULL → `report_fail_msg`（诚实失败，绝不 segv）。另：GCC 新警告（switch 全覆盖、`(void)unused`）由零警告门禁兜住。

---

### Task 1: 黄金语义探针（先测后码）

**Files:**
- Create: `/tmp/neon_rot_ldp_probe.c`（探针，不入库）
- Produces: 探针输出记录进最终回复；黄金 slot-3 形态决定（C `fmax` 或 asm `fmax d`）

**Interfaces:**
- Produces: Task 2 黄金循环 slot 3 的写法依据。探针同时验证 slot 0/1/2 的标量 `+,-,*` 与 `vaddq/vsubq/vmulq` 位等（IEEE 正确舍中操作，理论保证，实证兜底）。

- [ ] **Step 1: 写探针**

`/tmp/neon_rot_ldp_probe.c`：xorshift64* 生成随机 64 位对 + **刻意注入的角落**（±SNaN、±QNaN、±Inf、±0、denormal 与正负有限值成对交叉），对每对 (a,b) 比较 `a+b` vs `vaddq_f64`、`a-b` vs `vsubq_f64`、`a*b` vs `vmulq_f64`、`fmax(a,b)` vs `vmaxq_f64` 的逐位结果（memcpy 取位），分别统计失配数。≥1e7 随机对 + 全部注入角落。

- [ ] **Step 2: 编译运行**

Run: `gcc -O2 -Wall -Wextra -o /tmp/neon_rot_ldp_probe /tmp/neon_rot_ldp_probe.c && /tmp/neon_rot_ldp_probe`
Expected: 4 项失配全 0 → slot 3 用纯 C `fmax`；若 fmax 失配 >0 → slot 3 改用内联 asm `fmax %0,%1,%2`（约束 `"=w"(r) "w"(a) "w"(b)`），并把失配样例记录进 docstring。add/sub/mul 任一失配 >0 → 停下重查（与理论冲突，须解释后才能继续）。

### Task 2: 新用例 neon_rot_ldp.cpp + meson 注册

**Files:**
- Create: `tests/cpu/misc/neon_rot_ldp.cpp`
- Modify: `tests/cpu/meson.build`（aarch64 块 `'misc/neon_rot_ldr_rand_thread.cpp',` 之后插 `'misc/neon_rot_ldp.cpp',`）

**Interfaces:**
- Consumes: Task 1 裁决——黄金 = 同路径 NEON 重算（与 run 同 `fp64_rot`、同 pair 助手；ko `fp64_golden_init` 形态）。
- Produces: 测试 id `neon_rot_ldp`（PROD）；`static inline uint64x2_t load_pair_vec(const void*)` / `static inline void store_pair_vec(void*, uint64x2_t)`（ko 逐字 C++ 化，`__asm__ volatile ("ldp %0, %1, [%2]" : "=r"(lo), "=r"(hi) : "r"(addr) : "memory")` + `vcombine_u64(vcreate_u64(lo), vcreate_u64(hi))`；store 侧 `vgetq_lane_u64` 取 lo/hi 后 `stp %0, %1, [%2]`）；`static inline uint64x2_t fp64_rot(int slot, uint64x2_t a, uint64x2_t b)`（ko 逐字）；`__attribute__((noinline)) static void neon_rot_ldp_miscompare_report(...)`。

- [ ] **Step 1: 文件骨架（docstring + 前置）**

按家族格式：SPDX/copyright 头 + `@test @b neon_rot_ldp` @parblock docstring（lineage：ko 形态、verify.log 16/16；pair 替换与小端 lane 映射；骨架五要素；黄金 = Task 1 裁决的同路径 NEON 重算 + 探针证据；动机：pair 编码单指令 16B、LSQ/store buffer 单指令占用、FP64 消费端）。`#define NEON_ROT_LDP_COUNT 1024`；struct `NeonRotLdpData{srcA,srcB,scratch,expected}`（家族模式，成员在 `#if __aarch64__` 外——文件仅入 aarch64 构建面）。

- [ ] **Step 2: pair 助手 + fp64_rot + 冷路径报告助手**

ko 逐字移植（u64→uint64_t、snprintf %llx→%016"PRIx64" 或 C++ 风格 static_cast + %016lx 沿家族）。报告助手：bad[8]/total 扫描 → `char msg[900]` 拼接 `"neon_rot_ldp data miscompare: cpu=%d sweep=%lu nbad=%d bad=..."` + 前 2 个坏元素的 A/B/T/G/E/X（`sched_getcpu()` 取核号——fp64_dump 判例：不把 run 的 cpu 参数留活，避免热循环寄存器分配漂移）→ `report_fail_msg("%s", msg)`。

- [ ] **Step 3: init/run/cleanup + DECLARE_TEST**

init：4×`aligned_alloc(64, ...)`（NULL → `log_skip(RuntimeSkipCategory, ...)` + `EXIT_SKIP`）；3×`memset_random`；黄金（Task 1 裁决：同路径 NEON 重算，ko `fp64_golden_init` 形态）：`for i ∈ [0,COUNT): store_pair_vec(&expected[i], fp64_rot(i%4, load_pair_vec(&srcA[i]), load_pair_vec(&srcB[i])))`——与 run 同 slot 映射、同助手、同 `fp64_rot`。docstring 诚实记录：标量黄金为何不可行（探针证据）+ 本用例不覆盖的故障类由整数兄弟用例覆盖。run：per-thread `temp/dst` `aligned_alloc`（NULL → `report_fail_msg`）；`TEST_LOOP(test, 1 << 13)`：升/降序交替、每迭代 top 丢弃 `load_pair_vec(scratch[i])`（`(void)` 压制）→ 2 源 pair load → `fp64_rot(i%4)` → `store_pair_vec(temp[i])` + `store_pair_vec(dst[i])`；每 sweep 后 `memcmp(dst, expected)`，失败只进 noinline 报告助手。cleanup：4×free + `free(data)`。`DECLARE_TEST(neon_rot_ldp, "<家族风格描述串：ldp/stp pair 形态 + fp64 旋转 + ko 背书>")`，`.quality_level = TEST_QUALITY_PROD`。

- [ ] **Step 4: meson 注册**

`tests/cpu/meson.build` aarch64 块内 `'misc/neon_rot_ldr_rand_thread.cpp',` 行后插 `'misc/neon_rot_ldp.cpp',`（块内注释按需补一行来源说明）。

### Task 3: 验证门禁（CLAUDE.md 规则 2 四项 + 指令/计数点验）

**Files:**
- 无改动；验证 Task 2 产物。产物：objdump 摘录、实跑输出、计数输出，全部进最终回复。

- [ ] **Step 1: 干净构建零警告**

Run: `./scripts/build.sh 2>&1 | grep -iE "warn|error" `（构建须成功）
Expected: 空输出。

- [ ] **Step 2: objdump 点验 pair 形态（ko verify.sh 同款门禁）**

Run: `objdump -d builddir/sdcshield | awk '/<neon_rot_ldp_run>:/,/ret/'` 逐条点验数据通路：每内层迭代 3×`ldp x` + 2×`stp x`，无 `ldr q`/`str q` 残留；黄金循环同理无向量访存（标量黄金）。
Expected: 清单引用进最终回复。

- [ ] **Step 3: 实跑（单线程 + 全核 + 多 seed）**

Run: `./builddir/sdcshield -e neon_rot_ldp -t 5000 -n 1`；再 `-t 5000`（全核）；再 `-s 2 -t 5000 -n 1`（换 seed 复核黄金可复现性）。
Expected: 三次 `exit: pass`，引用真实输出。

- [ ] **Step 4: 计数 + 回归 + x86 非回归**

Run: `./builddir/sdcshield --list-tests | grep -c .`（前后对照，PROD 439→440）与 `grep neon_rot_ldp`；`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`；`git diff main -- tests/cpu/meson.build` 逐行确认改动仅在 aarch64 块内（x86 构建面零接触）。
Expected: 全过。

### Task 4: 提交推送

- [ ] **Step 1: 分支 + 提交**

`git branch --show-current`（须已在 feature 分支 `feat/neon-rot-ldp`，从 main 新建）；`git add tests/cpu/misc/neon_rot_ldp.cpp tests/cpu/meson.build docs/superpowers/plans/2026-11-11-neon-rot-ldp.md`；`git commit -s`（描述含：ko 背书、探针结论、四项验证证据、PROD 439→440 计数声明）。

- [ ] **Step 2: 推送**

`git push -u origin feat/neon-rot-ldp`（绝不推 main）。

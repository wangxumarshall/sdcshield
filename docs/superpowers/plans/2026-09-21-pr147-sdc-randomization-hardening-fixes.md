# PR #147 SDC 随机化加固 — 审查修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 PR #147 对抗性审查确认的缺陷（见 `2026-09-21-pr147-adversarial-review-findings.md`），在不降低已获得的瞬态 SDC 检出率的前提下，消除新引入的同义反复 golden、曝光并修复 mesh 家族的既有空转 pass、兑现 commit 声明中未完成的 stderr 清理。

**Architecture:** 四条主线：(1) G1′ 三个 GMP 测试换独立标量 golden（扩展 PR #146 修复计划的任务 1）；(2) G2′/G3′ mesh 6 文件屏障协议重设计（sense-reversal），使校验真正运行；(3) G4′/G5′ 兑现 lock 剩余 18 文件与 kreg/mesh 家族的 stderr 清理；(4) G6′/G7′ svd_cdouble_sve 死 init 清理、kreg4 常量 lane、注释与 README 对齐。所有修复在 `pr-147` 分支追加 commit。

**Tech Stack:** C++23（tests），框架 RNG，GMP（`mpz_*` + `__int128` 标量 golden），`std::atomic` sense-reversal barrier，meson/ninja，clangd 17 LSP。

## Global Constraints

- 分支：`pr-147`（已 checkout，HEAD = 7e40e3f），追加 commit，不改历史
- 每个 commit：`git commit -s`（DCO：`Signed-off-by: wangxumarshall <wangxumarshall@qq.com>` 结尾，无 Co-Authored-By）
- builddir 配置沿用：`-Dssl_link_type=static` + ACL vendored（勿重配）
- 提交前强制自验证：`ninja -C builddir` 零新告警；受影响测试实跑 `exit: pass`；`-e zstd19 -t 2000 -n 1` 回归；x86-64 非回归检查
- LSP：改动文件过 `clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/<file>`；kreg4 修复后其 8 处 `constant_integer_arg_type` 诊断必须消失
- 故障注入自测是 G1′/G2′ 的必要验收（对称注入必须 fail）
- 本计划与 PR #146 修复计划（`2026-09-20-pr146-sdc-randomization-hardening-fixes.md`）互补：两 PR 若先后合并，G1′ 覆盖 146-任务1 的扩展范围（bigint_mulx_arm + gmp_bignum + gmp_bigadd 三个文件统一处理）

---

### Task 1 (G1′): 三个 GMP 测试独立标量 golden（bigint_mulx_arm + gmp_bignum + gmp_bigadd）

**Files:**
- Modify: `tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp:30-115`（若 PR #146 修复已做则跳过此文件）
- Modify: `tests/cpu/arithmetic_arm/gmp_bignum.cpp:74-120`
- Modify: `tests/cpu/arithmetic_arm/gmp_bigadd.cpp:71-115`
- 参照（正确模式）: `tests/cpu/arithmetic/adcx.cpp:30-37`

**Interfaces:**
- Produces（gmp_bignum.cpp 内 static）:
  `static void schoolbook_mul_golden(const uint64_t *a, const uint64_t *b, uint64_t *out, size_t nlimbs)` — `unsigned __int128` schoolbook，nlimbs=64（4096 位）
- Produces（gmp_bigadd.cpp 内 static）:
  `static void scalar_add_golden(const uint64_t *a, const uint64_t *b, uint64_t *out, size_t nlimbs)` — `__int128` 进位链
- 已实证（2026-09-21 审查中验证）：mul golden 与 GMP 在 200 组随机 4096×4096 一致；add golden 与 GMP 在 1000 组随机 4096 位加法一致

- [x] **Step 1: gmp_bignum.cpp — golden 换 schoolbook**

在 `fill_random_bytes` 后新增：

```c
/* H13'' (PR #147 review): independent scalar golden — __int128 schoolbook
 * multiply, a different code path from GMP's mpn/UMULL kernels. The previous
 * PR #147 change computed golden with the SAME mpz_mul call as the DUT (pure
 * x==x; symmetric fault-injection passes — verified by experiment). Verified
 * bit-identical to GMP on 200 random 4096x4096 inputs. */
static void schoolbook_mul_golden(const uint64_t *a, const uint64_t *b,
                                  uint64_t *out, size_t nlimbs)
{
    std::vector<uint64_t> acc(2 * nlimbs, 0);
    for (size_t i = 0; i < nlimbs; ++i) {
        unsigned __int128 carry = 0;
        for (size_t j = 0; j < nlimbs; ++j) {
            unsigned __int128 cur = (unsigned __int128)a[i] * b[j] + acc[i + j] + carry;
            acc[i + j] = (uint64_t)cur;
            carry = cur >> 64;
        }
        acc[i + nlimbs] += (uint64_t)carry;
    }
    memcpy(out, acc.data(), nlimbs * sizeof(uint64_t));
}
```

run 循环内，golden 改为（操作数先转 limb 数组；`mpz_export` 方向注意：本测试
`mpz_import(a, GMP_BYTELEN, 1, 1, 1, 0, buf)` 是**大端字节序**导入 —
schoolbook 需要 limb 视图。两种实现路径：

```c
        fill_random_bytes(buf, GMP_BYTELEN);
        mpz_import(a, GMP_BYTELEN, 1, 1, 1, 0, buf);
        fill_random_bytes(buf, GMP_BYTELEN);
        mpz_import(b, GMP_BYTELEN, 1, 1, 1, 0, buf);

        /* 独立标量 golden：从 mpz 导出 limb（GMP 内部小端 limb 序），
         * schoolbook 乘出低 4096 位 */
        size_t na = 0, nb = 0;
        std::vector<uint64_t> la(GMP_BYTELENLIMBS(1), 0), lb(GMP_BYTELENLIMBS(1), 0);
        mpz_export(la.data(), &na, -1, sizeof(mp_limb_t), 0, 0, a);
        mpz_export(lb.data(), &nb, -1, sizeof(mp_limb_t), 0, 0, b);
        std::vector<uint64_t> gold(GMP_BYTELENLIMBS(1), 0);
        schoolbook_mul_golden(la.data(), lb.data(), gold.data(), GMP_BYTELENLIMBS(1));
        const size_t gsize = GMP_BYTELENLIMBS(1);

        /* 受测路径：GMP mpz_mul */
        mpz_mul(result, a, b);
        size_t rsize = mpz_size(result);
        if (rsize != gsize) {
            mpz_clears(a, b, golden, result, NULL);
            report_fail_msg("GMP mul produced unexpected size %zu vs %zu", rsize, gsize);
        }
        for (size_t i = 0; i < rsize; ++i) {
            mp_limb_t got = mpz_getlimbn(result, i);
            mp_limb_t want = gold[i];
            if (got != want) { ... 原失败路径，want 换 gold[i] ... }
        }
```

（`mpz_t golden` 变量与其 init/clear 可一并删除；注意保留失败路径的
`mpz_clears` 清单同步。）

**注意 mpz_import 的端序**：本文件用 `order=1`（大端）按**字节**导入 buf；
而 limb 视图用 `mpz_export(..., -1, sizeof(mp_limb_t), ...)` 导出 — 两个方向
都经过 mpz_t，值语义一致，schoolbook 拿到的就是同一数值的 limb 表示。已在上面的
200 组验证程序中用同样方式验证。

- [x] **Step 2: gmp_bigadd.cpp — golden 换标量进位链**

```c
/* H13'' (PR #147 review): independent scalar add golden (__int128 carry
 * chain). Verified bit-identical to GMP on 1000 random 4096-bit adds. */
static void scalar_add_golden(const uint64_t *a, const uint64_t *b,
                              uint64_t *out, size_t nlimbs)
{
    unsigned __int128 carry = 0;
    for (size_t i = 0; i < nlimbs; ++i) {
        unsigned __int128 s = (unsigned __int128)a[i] + b[i] + carry;
        out[i] = (uint64_t)s;
        carry = s >> 64;
    }
}
```

run 循环同 Step 1 的模式（la/lb/gold + `mpz_add(result, a, b)` 作 DUT）。

- [x] **Step 3: bigint_mulx_arm.cpp（若 PR #146 修复未先行）**

按 PR #146 修复计划任务 1 执行（`schoolbook_mul_golden` 8-limb 版本已在
146 计划中给出并验证）。三个文件若在同一批次修复，用一个 commit。

- [x] **Step 4: 故障注入自测（决定性验收，每文件）**

对每个文件：在 DUT 的 `mpz_mul`/`mpz_add` 调用后注入
`{ mpz_t one; mpz_init_set_ui(one, 1); mpz_xor(result, result, one); mpz_clear(one); }`
→ 期望 `exit: fail`（golden 来自独立标量路径，注入必被抓）。
再对 golden 路径对称注入（schoolbook 输出 `gold[0] ^= 1;`）→ 期望 `exit: fail`。
（PR #147 当前版本对称注入 pass — 已实证；修复后两个方向都必须 fail。）
全部验证后撤销注入。

- [x] **Step 5: 验证 + LSP + 回归**

```bash
ninja -C builddir    # 零新告警
for f in tests/cpu/arithmetic_arm/gmp_bignum.cpp tests/cpu/arithmetic_arm/gmp_bigadd.cpp; do
  clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/$f 2>&1 | grep -oP "All checks completed.*"
done
timeout 60 ./builddir/sdcshield -e gmp_bignum -e gmp_bigadd -t 4000 -n 1 2>&1 | tail -1   # exit: pass
timeout 90 ./builddir/sdcshield -e gmp_bignum -e gmp_bigadd -t 8000 2>&1 | tail -1          # all-core: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                        # exit: pass
```

- [x] **Step 6: Commit**

```bash
git add tests/cpu/arithmetic_arm/
git commit -s -m "fix(gmp): independent __int128 scalar goldens (were same-function tautology)

gmp_bignum/gmp_bigadd computed golden with the SAME mpz_mul/mpz_add call
as the DUT path (same operands, same iteration, same thread) — the exact
defect class the PR's own crc commit (019d82f) documented as 'audit D11:
a deterministic CRC-unit defect produces identical wrong values and
passes forever'. Symmetric fault-injection (corrupting both computes
identically) passes the current code; verified by experiment 2026-09-21.

Replace with independent scalar goldens: schoolbook multiply / carry-chain
add over __int128 limbs (mirroring adcx.cpp's __int128-golden pattern),
verified bit-identical to GMP on 200 random 4096x4096 multiplies and 1000
random 4096-bit adds. DUT paths remain GMP mpz_mul/mpz_add (mpn/UMULL —
the silicon path under stress). Fault-injection in either direction now
fails the test (verified). Also covers bigint_mulx_arm if not already
fixed by the PR #146 follow-up.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 2 (G2′+G3′): mesh 6 文件屏障协议重设计（sense-reversal）

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_avx2_symm_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_sse_symm_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_avx2_asymm_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_avx_sym.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_sse_asymm_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_sse_sym.cpp`

**背景（审查实证）**：这 6 个文件的 phase-1 屏障
`while ((read_done != 0 || iter == current_iter) && time)` 从第一轮起死锁
（`read_done` 从未被置 1，`iter` 只在屏障之后的 phase-3 推进），仅靠超时
break 解除 → Σ==golden 校验与 store/reload 比较从未执行 → **空转 pass**。
pre-existing（098d4aa 移植引入），PR #147 未修复也未曝光（commit 声称
"checks are kept" 对这 6 文件不成立）。单改 init（read_done=1）只解第一轮，
第二轮同样死锁（已实验证明）— 必须换协议。

**Interfaces:**
- Produces（每文件内 static，6 文件同构）:
  `static void mesh_barrier(ThreadData *td, uint32_t total_threads, uint32_t my_epoch, struct test *test)`
  — sense-reversal：`arrived.fetch_add(1)+1 == total` 时 `epoch.store(my_epoch+1)`，
  否则自旋等 `epoch != my_epoch`（带 `test_time_condition` 逃生）
- ThreadData 增 `std::atomic<uint32_t> arrived; std::atomic<uint32_t> epoch;`
  （替代 `iter`/`read_done`/`round_done` 三个字段；`num_threads` 保留）

- [ ] **Step 1: 重写屏障与轮次逻辑（以 mesh_upi_avx2_symm_int.cpp 为模板）**

struct 改动：

```c
struct ThreadData {
    alignas(64) int32_t *data;              // 共享数据数组
    std::atomic<uint64_t> global_sum;       // 累加和
    std::atomic<uint32_t> next_block;       // 块分配计数器
    std::atomic<uint32_t> allocated_blocks;// 已完成块计数
    std::atomic<uint32_t> thread_idx;       // 线程 ID 分配
    std::atomic<uint32_t> num_threads;
    /* sense-reversal barrier (G4' fix): the original iter/read_done/round_done
     * protocol deadlocked from round 1 (read_done never set 1; iter only
     * advanced after a completed round) — the sum check NEVER executed and
     * the test passed vacuously (verified: 0 bytes of per-round stderr over
     * many runs; pre-existing since the 098d4aa port). */
    std::atomic<uint32_t> arrived;
    std::atomic<uint32_t> epoch;
};
```

init 增加：
```c
    td->arrived.store(0, std::memory_order_relaxed);
    td->epoch.store(0, std::memory_order_relaxed);
```

run 的循环骨架（替换原 phase-1 屏障 + phase-3 轮次推进；phase-2 的校验体不动）：

```c
    uint32_t my_epoch = 0;
    do {
        /* ---------- 阶段 1：所有线程共同填充数据（屏障隔轮） ---------- */
        /* 重置本轮状态（幂等 store，多线程同值写 benign） */
        td->global_sum.store(0, std::memory_order_seq_cst);
        td->next_block.store(0, std::memory_order_seq_cst);
        td->allocated_blocks.store(0, std::memory_order_seq_cst);

        /* 所有线程竞争分配块并写入随机数据（原逻辑不动：vals/dist/NEON store/
         * global_sum.fetch_add/allocated_blocks.fetch_add） */
        ...原 phase-1 填充循环原样保留...

        /* 等待所有线程完成写入 */
        while (td->allocated_blocks.load(std::memory_order_seq_cst) < NUM_BLOCKS &&
               test_time_condition(test)) {
            __asm__ volatile("yield");
        }
        if (!test_time_condition(test)) break;

        __sync_synchronize();

        /* ---------- 阶段 2：所有线程读取完整数组并验证（原逻辑不动） ---------- */
        ...原 phase-2 读+校验+（可选）fprintf 原样保留...

        /* ---------- 阶段 3：sense-reversal 屏障进入下一轮 ---------- */
        uint32_t done = td->arrived.fetch_add(1, std::memory_order_acq_rel) + 1;
        if (done == total_threads) {
            td->arrived.store(0, std::memory_order_relaxed);
            td->epoch.store(my_epoch + 1, std::memory_order_release);
        } else {
            while (td->epoch.load(std::memory_order_acquire) == my_epoch &&
                   test_time_condition(test)) {
                __asm__ volatile("yield");
            }
            if (!test_time_condition(test)) break;
        }
        ++my_epoch;
    } while (test_time_condition(test));
```

**注意**：阶段 1 开头的状态重置移到了屏障**之后**（上一轮全部线程离开阶段 2
才能重置 `global_sum`/`next_block`），这正是原协议想做而没做对的轮次隔离。
6 个文件的 NUM_BLOCKS/填充/读取体各自不同 — 只统一屏障骨架，工作体逐文件保留。

- [ ] **Step 2: 验证校验真正运行（stderr 证据 + 故障注入）**

```bash
ninja -C builddir
# 修复前：-f no 直连 stderr 多轮 0 字节（校验从未跑）；修复后必须有每轮输出
timeout 20 ./builddir/sdcshield -f no -e mesh_upi_avx2_symm_int -t 3000 -n 2 2>/tmp/mesh_fixed.txt
grep -c "read_sum" /tmp/mesh_fixed.txt    # 期望 > 0（每轮每线程 1 行）
```

故障注入（证明 Σ 校验现在有效）：临时把阶段 2 的
`bool passed = (read_sum == expected_sum) && consistent;` 改为
`bool passed = (read_sum == expected_sum + 1) && consistent;` → 期望 `exit: fail`
（修复前这个注入永远不触发 — 校验不可达）。验证后撤销。

- [ ] **Step 3: 6 文件全量验证**

```bash
ninja -C builddir
for t in mesh_upi_avx2_symm_int mesh_upi_sse_symm_int mesh_upi_avx2_asymm_int mesh_upi_avx_sym mesh_upi_sse_asymm_int mesh_upi_sse_sym; do
  timeout 30 ./builddir/sdcshield -e $t -t 3000 -n 2 2>&1 | tail -1   # 每个 exit: pass
done
timeout 120 ./builddir/sdcshield -e mesh_upi_avx2_symm_int -e mesh_upi_sse_sym -t 5000 2>&1 | tail -1  # all-core 抽样
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1
```

- [ ] **Step 4: LSP + Commit**

```bash
for f in tests/cpu/mesh/mesh_upi_avx2_symm_int.cpp tests/cpu/mesh/mesh_upi_sse_sym.cpp; do
  clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/$f 2>&1 | grep -oP "All checks completed.*"
done
git add tests/cpu/mesh/
git commit -s -m "fix(mesh): replace deadlocked round barrier with sense-reversal (6 tests actually verify now)

The symm/asymm 6-file family's phase-1 barrier
  while ((read_done != 0 || iter == current_iter) && time)
deadlocked from round 1: read_done is never set to 1 anywhere and iter
only advances in phase 3 AFTER the barrier — no thread can pass, the
loop spins until the time budget expires, and the run returns
EXIT_SUCCESS. The sum check and the store/reload compare NEVER executed
(verified: 0 bytes of per-round stderr over many -f no runs; an
experimental init-only fix still deadlocks at round 2). Pre-existing
since the 098d4aa port; the GHA workflow's --disable mesh_upi* was
masking it as 'needs multi-NUMA silicon'.

Replace the three-field protocol (iter/read_done/round_done) with a
sense-reversal barrier (arrived/epoch): round N's state reset happens
only after all threads left round N-1's verification. The fill/read/
verify bodies are unchanged. Verified: per-round stderr output now
present (was 0); fault-injecting a sum mismatch now fails the test
(was unreachable); all 6 tests -t 3000 -n 2 pass; zstd19 regression
pass; x86-64: NEON under #ifdef, x86 paths untouched.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 3 (G5′): 兑现 lock 家族 stderr 清理（剩余 18 文件）

**Files:**
- Modify: `tests/cpu/lock/lockgen.cpp`、`lockless_cmpxchg16b.cpp`、`lockless_cmpxchg8b.cpp`、`locks_ccb.cpp`、`locks_ccb_xch_only.cpp`、`locks.cpp`、`locks_xch_only.cpp`
- Modify: `tests/cpu/spinlock/spinlock_crosses_cacheline.cpp`、`spinlock_rmw_cmpxchg.cpp`、`spinlock_rmw_cmpxchg_pause.cpp`、`spinlock_rmw_xchg.cpp`、`spinlock_rmw_xchg_pause.cpp`、`spinlock_same_core.cpp`、`spinlock_stress_bts.cpp`、`spinlock_stress_cmpxchg16b.cpp`、`spinlock_stress_cmpxchg.cpp`、`spinlock_unaligned.cpp`、`spinlock_with_hle.cpp`

（18 文件清单来自审查的逐文件比对；commit c805065 只清了 8 个。）

**Interfaces:**
- 无新接口；机械删除 run 循环内 ungated 的 `fprintf(stderr, "...PASS...")` + `fflush`
- 保留 finish 阶段的汇总 fprintf（每 fracture 一次，设计内）
- fail 路径若需要上下文，按 `spinlock_stress.cpp` 已修文件的样式放进 `if (!passed)` 或 report_fail_msg 参数

- [ ] **Step 1: 逐文件删除 in-loop PASS fprintf**

模式（以 locks.cpp:73-75 为例）：

删除：
```c
        // 输出本次的输入（线程、增量）和结果（锁操作成功，PASS）
        fprintf(stderr, "locks: Thread %d, inc=%lu, result=%sPASS%s\n",
                id, inc, GREEN, RESET);
        fflush(stderr);
```

（`GREEN/RESET` 宏若因此无用户则一并删；finish 汇总输出里的宏保留。）

其余 17 文件同构 — 找各自 run 循环内的 `fprintf(stderr, ... Thread %d ... PASS ...)` +
`fflush` 删除。**逐文件确认**删的是 in-loop ungated 行，不动 finish 汇总。

- [ ] **Step 2: 批量核查清零**

```bash
for f in tests/cpu/lock/*.cpp tests/cpu/spinlock/*.cpp; do
  awk -v fn="$f" '/do \{/,/while \(test_time_condition/ { if (/if \(!passed\)/) gated=1; if (/fprintf/ && !gated) { print "STILL-FLOODS: " fn; exit } }' "$f"
done
# 期望：无输出
```

- [ ] **Step 3: 验证**

```bash
ninja -C builddir
timeout 30 ./builddir/sdcshield -e locks -t 3000 -n 4 -v 2>/dev/null | wc -c   # 显著下降（was 742KB；只剩 finish 汇总）
timeout 30 ./builddir/sdcshield -e locks -t 3000 -n 4 -v 2>/dev/null | grep -c "Thread.*PASS"  # 0（was 10080）
timeout 250 ./builddir/sdcshield -e 'lock*' -e 'spinlock_*' -t 3000 -n 4 2>&1 | tail -1            # exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                                # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/lock/ tests/cpu/spinlock/
git commit -s -m "fix(lock): complete the per-iteration stderr cleanup c805065 claimed (18 remaining files)

c805065's message said 'the per-iteration stderr spam is removed' for
all 28 lock/spinlock tests, but only 8 of the touched files actually
lost their in-loop PASS line — 18 kept flooding (measured: locks
-t 3000 -n 4 -v produced 10,080 Thread-PASS lines / 742KB of YAML).
Remove the remaining in-loop fprintf+fflush pairs; the finish-phase
counter summary (one line per fracture) is kept by design.

Verified: 0 ungated in-loop fprintf across lock/ and spinlock/ (awk
sweep); locks -v output 742KB -> summary-only; full lock+spinlock
family -t 3000 -n 4 exit:pass; zstd19 regression pass.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 4 (G6′a): kreg1-9 stderr 清理 + kreg4 常量 lane 修复

**Files:**
- Modify: `tests/cpu/vector/kreg1.cpp` … `kreg9.cpp`（9 文件）

**Interfaces:**
- kreg4 修复 Produces：两处 4-迭代 lane 提取循环展开为常量 lane（对齐 insert_extract.cpp 的既有样式）
- 所有 9 文件：in-loop fprintf 移入 `if (!passed)` 或改 report_fail_msg 参数

- [ ] **Step 1: kreg4.cpp 常量 lane 展开**

第 56-76 行（VPTESTM 掩码提取）替换为：

```c
        // 提取每个 lane 的最低有效位（0 或 1）组合成 16 位掩码
        // （vgetq_lane_u32 要求编译期常量 lane — 循环展开，ACLE 规范）
        uint16_t hw_mask_m = 0;
        {
            const uint32_t v0_[] = { vgetq_lane_u32(m0, 0), vgetq_lane_u32(m0, 1),
                                     vgetq_lane_u32(m0, 2), vgetq_lane_u32(m0, 3) };
            const uint32_t v1_[] = { vgetq_lane_u32(m1, 0), vgetq_lane_u32(m1, 1),
                                     vgetq_lane_u32(m1, 1), vgetq_lane_u32(m1, 3) };
            const uint32_t v2_[] = { vgetq_lane_u32(m2, 0), vgetq_lane_u32(m2, 1),
                                     vgetq_lane_u32(m2, 2), vgetq_lane_u32(m2, 3) };
            const uint32_t v3_[] = { vgetq_lane_u32(m3, 0), vgetq_lane_u32(m3, 1),
                                     vgetq_lane_u32(m3, 2), vgetq_lane_u32(m3, 3) };
            for (int i = 0; i < 4; ++i) {
                if (v0_[i] & 1) hw_mask_m |= (1 << i);
                if (v1_[i] & 1) hw_mask_m |= (1 << (i + 4));
                if (v2_[i] & 1) hw_mask_m |= (1 << (i + 8));
                if (v3_[i] & 1) hw_mask_m |= (1 << (i + 12));
            }
        }
```

（注意上面 `v1_` 数组第 3 个元素笔误示范 — 实施时四个数组都按 0,1,2,3 写。）
第 84-102 行（VPTESTNM）同构展开。

- [ ] **Step 2: 9 文件 in-loop fprintf 移入 fail 分支**

模式（对齐 5998967 在 fma 家族的做法）：把 `fprintf` 组与 `color/result_str`
局部变量剪切进 `if (!passed) { ... report_fail_msg(...); }` 内。kreg8（14 行）
与 kreg7（7 行）输出最长 — 全部进 fail 分支，信息不丢。

- [ ] **Step 3: 验证（含 LSP 诊断清零）**

```bash
ninja -C builddir
clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/tests/cpu/vector/kreg4.cpp 2>&1 | grep "constant_integer_arg_type" | wc -l
# 期望 0（was 8）
timeout 60 ./builddir/sdcshield -e 'kreg*' -t 2500 -n 1 2>&1 | tail -1    # exit: pass
timeout 30 ./builddir/sdcshield -e kreg4 -t 2000 -n 1 -v 2>/dev/null | wc -c   # < 15KB（was 642KB）
timeout 60 ./builddir/sdcshield -e kreg4 -e kreg7 -t 6000 2>&1 | tail -1   # all-core: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1      # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/vector/
git commit -s -m "fix(kreg): constant lane indices in kreg4 (LSP-diagnosed) + move kreg1-9 stderr to fail path

clangd flags 8 'argument to __builtin_neon_vgetq_lane_i32 must be a
constant integer' errors in kreg4 (loop-variable lane index) — the same
ACLE violation 0445a21 fixed in insert_extract; GCC -O3 happens to
fully unroll the 4-trip loop into constant lanes (objdump-verified), so
runtime behavior was correct, but any non-unrolling build turns it into
a hard error. Unroll to compile-time lanes. Also move the kreg1-9
per-iteration stderr dumps (4-14 lines/iter; kreg4 -t 2000 -v was
642KB of YAML) into the failure branch, matching the fma family's
post-5998967 pattern.

Verified: kreg4 clangd constant_integer_arg_type diagnostics 8 -> 0;
kreg family -t 2500 -n 1 and kreg4/kreg7 all-core exit:pass; kreg4 -v
output 642KB -> <15KB; zstd19 regression pass.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 5 (G6′b): mesh 运行家族 stderr 清理（read_only/write_only/distrib/read_L3）

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_avx2_read_only_int.cpp`、`mesh_upi_sse_read_only_int.cpp`、`mesh_upi_avx2_read_L3_int.cpp`、`mesh_upi_sse_read_L3_int.cpp`、`mesh_upi_avx512_asymm_read_only_int.cpp`、`mesh_upi_sse_read_only_int.cpp`、`mesh_upi_sse_write_only_int.cpp`、`mesh_upi_avx512_asymm_write_only_int.cpp`、`mesh_upi_avx512_asymm_write_only_int.cpp`、`mesh_upi_{avx2,avx512,sse}_asymm_distrib{,_write}_int.cpp`（运行且 flood 的清单以 Step 2 的 awk 扫描为准）

**Interfaces:**
- 模式与 Task 3/4 相同：in-loop fprintf 进 `if (!passed)`；Task 2 修复后的 symm/asymm 6 文件每轮 1 行的轮次汇总**保留**（那是设计内的 round summary）

- [ ] **Step 1: 逐文件移动 fprintf 进 fail 分支**（对照审查中确认的 `mesh_upi_sse_read_only_int -t 3000 -n 2 -v` → 1,720 行 / 329KB 的 flood）

- [ ] **Step 2: 批量核查**

```bash
for f in tests/cpu/mesh/*.cpp; do
  awk -v fn="$f" '/do \{/,/while \(test_time_condition/ { if (/if \(!passed\)/) gated=1; if (/fprintf/ && !gated) { print "STILL-FLOODS: " fn; exit } }' "$f"
done
# 期望：无输出（Task 2 的 6 文件轮次汇总若在 passed 路径打印，可豁免为每轮 1 行 — 在 commit message 里注明豁免清单）
```

- [ ] **Step 3: 验证 + Commit**

```bash
ninja -C builddir
timeout 30 ./builddir/sdcshield -e mesh_upi_sse_read_only_int -t 3000 -n 2 -v 2>/dev/null | wc -c  # < 20KB（was 329KB）
timeout 300 ./builddir/sdcshield -e 'mesh_*' -t 2000 -n 2 2>&1 | tail -1                            # exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1
git add tests/cpu/mesh/
git commit -s -m "fix(mesh): move per-iteration stderr to fail path (read_only/write_only/distrib families)

mesh_upi_sse_read_only_int -t 3000 -n 2 -v produced 1,720 Thread-PASS
lines / 329KB of YAML — the same flood class c805065/5998967 removed
from lock/fma but missed here. Round-summary lines in the Task-2-fixed
symm/asymm family (one line per round per thread) are kept by design.

Verified: -v output 329KB -> <20KB; mesh family -t 2000 -n 2 exit:pass;
zstd19 regression pass.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 6 (G7′): svd_cdouble_sve 死 init 清理 + 注释对齐 + README 计数

**Files:**
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp:168-176`
- Modify: `tests/cpu/eigen_svd/sandstone_eigen_common.h:62-76`（其余 9 个 compile-time-Dim 用户的 init golden）
- Modify: `tests/cpu/crc/crc32.cpp:46`、`crc32_fixed.cpp`、`crc32_fixed_shuffled.cpp`（"CRC-32C" 注释与 `crc32c_software` 命名）
- Modify: `tests/cpu/arm64/sve512_f64_chain_arm.cpp:172`（指数带注释）
- Modify: `tests/cpu/arm64/lsu_store_forward_arm.cpp:133`（死变量 `golden_cycle`）
- Modify: `README.md:283`（328 → 实测值）

**Interfaces:**
- `svd_cdouble_sve.cpp` 的 `sve_probe_and_init`：删除 `d->orig_matrix = Mat::Random(...)` + `calculate_once(...)` 两行（run 已完全自带 prime），`desired_duration` 模型注释从 "两分解" 改为 "prime+DUT 两分解"（init 不再算）
- `sandstone_eigen_common.h` 的 `init()`：`if constexpr (!kRuntimeDim)` 分支里的 `Mat::Random` + `calculate_once` 同样成为死代码 — 删除并留注释（run 忽略 init 矩阵）

- [ ] **Step 1: 删除死 init 计算**

svd_cdouble_sve.cpp（第 168-176 行区域）：

```c
    /* Allocate the test data (the runtime-dimension template's run()
     * primes its own per-thread matrices; the init-time golden BDCSVD
     * became dead computation once run() stopped reading eigen_test_data's
     * matrices — removed. Timing note: desired_duration now covers exactly
     * the two run-side decompositions (prime + DUT). */
    auto d = new eigen_svd_cdouble_sve_test::eigen_test_data;
    d->dim = g_dim;
    test->data = d;
    return EXIT_SUCCESS;
```

sandstone_eigen_common.h init()：

```c
    static int init(struct test *test)
    {
        auto d = new eigen_test_data;
        /* H11'' (PR #147 review): run() primes per-thread matrices itself;
         * the init-time Mat::Random + golden SVD became dead computation
         * (never compared) once the re-roll landed — removed. */
        test->data = d;
        return EXIT_SUCCESS;
    }
```

同步更新 svd_cdouble_sve.cpp 头部 "two complete decompositions" 的时序注释
（模型不变：run 侧仍是 prime+DUT 两次；只是明确 init 不再贡献第三次）。

- [ ] **Step 2: 小注释/命名对齐**

- crc32 三件套：第 46 行等 "（CRC-32C）" 注释改 "（IEEE 802.3 CRC-32）"；`crc32c_software` 更名 `crc32_ieee_software`（三文件同步，与 H15' 注释一致）
- sve512_f64_chain_arm.cpp:172 注释改为 "uniform exponent 0x3FE..0x3FF covers [0.5, 2)"
- lsu_store_forward_arm.cpp:133 删除死变量 `static uint64_t golden_cycle = 0;`

- [ ] **Step 3: README 计数复核**

```bash
./builddir/sdcshield --list-tests 2>/dev/null | wc -l   # 实测值（本机 329）
# README.md:283 的 328 改为该实测值；若与 329 不符，查明差异源（如 ACL 探测）后写实际数字
```

- [ ] **Step 4: 验证 + Commit**

```bash
ninja -C builddir
timeout 120 ./builddir/sdcshield -e eigen_svd -e eigen_svd_double -e eigen_svd_cdouble -e eigen_svd_fvectors -e eigen_svd_cdouble_noavx512 -e eigen_svd_double2 -t 8000 -n 1 2>&1 | tail -1  # exit: pass
timeout 60 ./builddir/sdcshield -e crc32 -e crc32_fixed -e crc32_fixed_shuffled -t 2500 -n 1 2>&1 | tail -1   # exit: pass
timeout 60 ./builddir/sdcshield -e lsu_store_forward_arm -e sve512_f64_chain_arm -t 3000 -n 1 2>&1 | tail -1   # exit: pass（sve512 在本机 skip 或 pass 均可）
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1
git add -u
git commit -s -m "fix(eigen,crc,misc): remove dead init goldens, align comments/names with verified behavior

svd_cdouble_sve's init computed a full BDCSVD (~243s at auto-size
N=4400) that the re-rolled run() never reads — pure dead computation
that also pushed the wall clock 50% past the two-decomposition timing
model. Same for the template init's Mat::Random + golden SVD in the
nine compile-time-dim users. Remove both; run() primes its own
per-thread matrices. Also: crc32 family's stale 'CRC-32C' comments and
misleading crc32c_software name (the probe proved IEEE 802.3),
sve512_f64_chain's over-wide exponent-band comment, the dead
golden_cycle static in lsu_store_forward_arm, and the README test
count (328 claimed, 329 measured).

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 7: 收尾 — 全量回归 + LSP 抽查 + 推送

- [ ] **Step 1: 全量功能回归**

```bash
ninja -C builddir
timeout 60 ./builddir/sdcshield -e gmp_bignum -e gmp_bigadd -e bigint_mulx_arm -t 4000 -n 1 2>&1 | tail -1
timeout 120 ./builddir/sdcshield -e mesh_upi_avx2_symm_int -e mesh_upi_sse_sym -e mesh_upi_avx2_asymm_int -e mesh_upi_avx_sym -e mesh_upi_sse_asymm_int -e mesh_upi_sse_sym -t 3000 -n 2 2>&1 | tail -1
timeout 300 ./builddir/sdcshield -e 'mesh_*' -t 2000 -n 2 2>&1 | tail -1
timeout 250 ./builddir/sdcshield -e 'lock*' -e 'spinlock_*' -t 3000 -n 4 2>&1 | tail -1
timeout 60 ./builddir/sdcshield -e 'kreg*' -t 2500 -n 1 2>&1 | tail -1
timeout 120 ./builddir/sdcshield -e eigen_svd -e eigen_svd_double -e eigen_gemm_double_dynamic_square -e eigen_sparse -t 6000 -n 1 2>&1 | tail -1
timeout 60 ./builddir/sdcshield -e crc32 -e crc32_fixed -e crc32_fixed_shuffled -e isal_crc_ieee -t 2500 -n 1 2>&1 | tail -1
timeout 60 ./builddir/sdcshield -e atomic_simd_128 -e atomic_simd_256 -e atomic_simd_512 -t 4000 -n 1 2>&1 | tail -1
timeout 200 ./builddir/sdcshield -e 'sve512*' -e lsu_store_forward_arm -t 2000 -n 1 2>&1 | tail -1
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1
# 全部期望 exit: pass
```

- [ ] **Step 2: LSP 抽查（本计划触碰文件）**

```bash
for f in tests/cpu/arithmetic_arm/gmp_bignum.cpp tests/cpu/arithmetic_arm/gmp_bigadd.cpp \
         tests/cpu/mesh/mesh_upi_avx2_symm_int.cpp tests/cpu/lock/locks.cpp \
         tests/cpu/vector/kreg4.cpp tests/cpu/eigen_svd/svd_cdouble_sve.cpp; do
  echo "== $f"; clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/$f 2>&1 | grep -c "constant_integer_arg_type\|error:"
done
# 期望全 0（tweak-only 噪音除外）
```

- [ ] **Step 3: x86-64 非回归审查（inspection）**

```bash
git diff 7e40e3f..HEAD --stat
# 逐文件确认：gmp/mesh/lock/kreg/eigen/crc 改动在 arch-neutral 共享逻辑或 #ifdef __aarch64__ 门内；
# sense-reversal 屏障替换的 std::atomic 逻辑两架构通用（与原协议同为 arch-neutral）
```

- [ ] **Step 4: 推送**

```bash
git push origin pr-147:<PR-147 的 head 分支名>
# （本机 gh 未认证 — head 分支名以 GitHub PR 页面为准；推送后请用户在 GitHub 确认 PR 更新）
```

- [ ] **Step 5: 更新 findings 文档**

在 `2026-09-21-pr147-adversarial-review-findings.md` 末尾追加"修复落地记录"：
每个 G# 对应的 commit hash + 验证输出摘录。

---

### Task 8（执行中追加）: mesh asymm 剩余 5 文件 fracture 预算饥饿修复

> 由 Task 2/Task 5 审查发现追加（2026-09-22）：avx512_asymm_int、avx2_asymm_write、
> avx2_asymm_distrib_write、avx512_asymm_distrib、sse_asymm_distrib 的读端在 -n 2 下
> 实测 0 行输出（99 writer fills / 0 reader rounds 同类症状）— 读等待自旋每迭代消耗
> test_time_condition 的 fracture 预算（auto-fracture cap 40），预算耗尽时读者从未
> 进入验证阶段 → 疑似 vacuous pass（与 Task 2 修复的 asymm 对同一根因）。

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_avx512_asymm_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_avx2_asymm_write_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_avx2_asymm_distrib_write_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_avx512_asymm_distrib_int.cpp`
- Modify: `tests/cpu/mesh/mesh_upi_sse_asymm_distrib_int.cpp`
- 同批并入（Task 6 审查 Minor，同类）：4 个 avx512 mesh 文件的未用 `#include <random>`；
  `tests/cpu/sve/mesh/mesh_upi_sve_wide_*` 5 文件的 block_ok 空检查（同 87ebc4b 的修法）；
  `tests/cpu/arm64/l2c_cross_cache_line_arm.cpp:234` 死 `static uint64_t golden_cycle`

**方案**：按 Task 2 已验证的 1024-spin 节流模式（等待条件每 spin 检查、ttc 预算
检查每 1024 spin 一次 + out_of_budget 逃生）替换这些文件读等待里的裸 per-iteration
`test_time_condition` 自旋。同批把 5 文件的 per-round fprintf 移入 fail 分支
（与 Task 5 家族统一；Task 2 六件的每轮汇总豁免不变）。

**验收**：
- 修复后每文件 `-f no -t 3000 -n 2` 直连 stderr 出现读端验证输出（>0 行）或
  通过注入实验证明验证可达（sum 注入 → exit: fail）
- `mesh_*` 全家族 `-t 2000 -n 2` exit: pass；zstd19 回归
- 一个 commit；DCO；x86-64 检查

**完成记录（2026-09-22，commit 8502e49）**：逐文件运行时判定后假设对 3 文件成立
（avx2_asymm_write / avx2_asymm_distrib_write / sse_asymm_distrib：-o 日志通道实测
0 行读端验证）、对 2 文件不成立（avx512_asymm_int 375 行、avx512_asymm_distrib_int
374 行 —— 已在验证，按"不改动能工作的"跳过节流，仅做 fprintf 迁移）。注意：裸终端
stderr 恒为 0（框架把 child stderr dup2 进 memfd 再写入 -o 日志），行数计数必须用
-o 日志或 -v 通道。三个饥饿文件：读者首等桥接实测 2.7~6.7ms 线程启动滞后（自旋
~1.7ns/次，1024 档仅 ~70µs）→ 该等待档距取 131072，其余等待保持 1024；sse 文件写
者另有两处缺陷（轮次屏障误置于逐块循环内致单轮最多填 1 块；每块烧 2 次 ttc 致
256 块一轮需 ~513 次预算）→ 按 2731971 写者结构重构（屏障前置 + 无守卫有限填充）。
修后行数 6595/6592/377。5 文件全部注入实验 exit:fail（证据行触发）→ 还原 pass。
Minor：(a) 实查 4 个点名文件中 avx512_sym 并无 `<random>`（审查信息过时），其余
3 个 + 5 个目标文件共 8 处删除（预处理级核实仅注释提及）；(b) SVE wide 5 文件
block_ok 按 87ebc4b 接入失败判定（本机无 SVE，运行时不可验证，build+LSP 验证）；
(c) l2c 死 static 已删。ninja 零新增警告（1 个预存 block_ok 警告经 stash 对照证
实）；家族 39 测试 pass；zstd19/l2c pass；LSP 0 error；x86-64 机械核查全部 hunk
在 aarch64 guard 内。详见 task-8-report.md。

---

## 与 PR #146 修复计划的协调

两份计划独立可执行，但有交叉：

1. **Task 1（G1′）⊇ 146-计划-任务 1**：bigint_mulx_arm 在两边都出现。执行顺序
   谁先做谁覆盖，后者跳过该文件；commit message 里注明覆盖关系。
2. **146-计划-任务 4/5（fma stderr + RNG）已被 PR #147 的 5998967 完成** —
   若 146 修复计划尚未执行，其任务 4/5 可直接划掉。
3. **146-计划-任务 2/3（openssl/ipsec staggered golden）在 PR #147 中未做** —
   仍然有效，且其 F1 分析同样适用于本 PR 未触碰的 openssl/ipsec 家族。
4. 两 PR 若都合并，G1′ 的三文件统一处理避免重复劳动。

## Self-Review 记录

- **覆盖核对**：F1′→Task 1；F2′+F3′→Task 2；F5′(部分)→Task 3；F5′(kreg4)+F6′(kreg)→Task 4；F6′(mesh)→Task 5；F4′+F7′+F8′→Task 6；收尾→Task 7。
- **占位符扫描**：无 TBD/TODO；两个 golden 实现已在本机独立验证（200/1000 组）；屏障设计经过 init-only 失败实验后修正为 sense-reversal。
- **类型一致性**：`schoolbook_mul_golden(const uint64_t*, const uint64_t*, uint64_t*, size_t)` 与 `scalar_add_golden(同签名)` 各自定义于自己文件；`mesh_barrier` 为各文件内 static（6 文件同构复制，不跨文件链接）；Task 6 的 init 改动只删代码不添接口。
- **执行顺序**：Task 1（检出率核心）→ Task 2（空转 pass）→ Task 3/4/5（stderr，可并行）→ Task 6 → Task 7。

# Issue #373 第一棒：neon_rot 族同码 golden → 标量独立 golden

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 `tests/cpu/misc/neon_rot_*.cpp`（38 个转换目标）的 init golden 从"与被测 hot loop 同一组 NEON 指令计算"改为"纯 C 标量独立复算"，使 NEON 数据通路的持久单元故障（stuck lane / 确定性算错 / mercurial core）从"零检出"变为"必检出"，并立范式供 issue #373 剩余族（sve_rot / arm0102 / neon_test / neon_add_sve / 15 个 sve VLA 变体）照抄。

**Architecture:** 只换 golden 的来源——init 的 golden 循环从向量 intrinsic 换成标量整数运算（经 `reinterpret_cast` 逐元素读写同一缓冲），run 侧压力骨架（TEST_LOOP、迭代结构、asm ldr/str、指令混合、缓冲尺寸）、冷路径 dump/classifier、失败闸门（memcmp）全部零改动。expected 缓冲类型保持 `uint64x2_t*` 等原类型（字节布局不变），使 dump 变体的 `fwrite(data->expected, ...)` 无需触碰。

**Tech Stack:** C++20 / GCC（aarch64, arm_neon.h intrinsics + inline asm）、meson/ninja、sdcshield 框架（TEST_LOOP / memset_random / report_fail_msg）。

**Spec:** GitHub issue #373（`gh issue view 373`）——缺口分析、族清单、树内 3 个标量先例（`tests/cpu/sve/case/neon_rot_2src_sve.cpp:105-112`、`neon_rot_store_only_sve.cpp:99-106`、`neon_rot_u8x16_sve.cpp:119-126`）与验收方向。本计划是 #334 母卡下 #373 的第一棒，只做 misc/neon_rot 族。

## Global Constraints

- CLAUDE.md 全部规则生效；golden 比对语义是**字节一致 memcmp**（本计划不改变任何比对代码）。
- **x86-64 untouched**：目标文件全部在 `tests/cpu/meson.build` 的 `if host_machine.cpu_family() == 'aarch64'` 块内注册（2026-10-31 bulk registration），x86 构建不编译它们；改动只减少 init 里的 arch 相关代码，x86 非回归靠此事实目检。
- **防同源回归**：标量 golden 循环不得被编译器自动向量化回 NEON/SVE——switch 相位分派是天然屏障，但必须以 objdump 机械验证（见 V4）每个 init 符号内无向量 ALU 指令。
- golden 在 `test_init`（主线程）一次算好；随机输入在 init 由 `memset_random` 冻结后 run 只读——标量与向量读同一输入字节。
- 构建零新警告：`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `SDC_BUILD_JOBS=20 ./scripts/build.sh`（并发 agent 禁满核）。
- 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- DCO：`git commit -s`，尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By；只 push feature 分支，**绝不 push main**。
- 提交粒度：模板族机械迁移按"波次"提交（先例：2026-10-31 一次提交 71 个文件）；每波提交前该波所有测试逐个验证通过。

---

## 范式（本计划的核心产出——后来者的模板）

### 零检出机理（为什么同码 golden 守不住主靶）

同码 golden = init 阶段 expected 用**与 run hot loop 完全相同的向量指令、同一 NEON 数据通路**算出（源码自注释承认：`neon_rot_u32x4.cpp` "golden: same rotating composition, computed on the same NEON units"）。此时：

- **瞬态 SDC**（init 之后发生的翻转）：只污染 run 侧 → expected≠actual → 可检出。这是现有语义。
- **持久单元故障**（stuck lane、NEON/SVE 数据通路确定性算错、mercurial core）：init 与 run 走同一通路 → 同一确定性错误进入两侧 → memcmp 字节一致 → **假 PASS，零检出**。NEON/SVE 数据通路是本仓库 SDC 案例史的第一现场，这批族恰好只有"瞬态可检"语义。

标量独立 golden 把 expected 的计算移到**与 NEON 单元物理独立的标量整数通路**（纯 C 的 `+ - ^ & | *`，uintN_t 模 2^N 位精确运算）。向量单元持久故障只污染 actual → expected≠actual → FAIL。检出语义从"瞬态可检"升级为"瞬态+持久可检"。

### 规则

1. **只换 golden 来源**：run 压力骨架、冷路径、闸门（memcmp）零改动。
2. **init 一次算好**：golden 在 `test_init` 算完存 expected；绝不在 run 里与向量同循环混合计算。唯一例外是 tlbstress（窗口滑动结构要求逐窗口重算 golden）——其 golden 是独立的 noinline 函数，标量化后同样独立于 NEON 通路，且 noinline 屏障（防 GCC 融合热循环删 store）必须保留。
3. **输入确定性**：`memset_random` 在 init 冻结；run 不改输入（既有事实，本计划不触碰）。
4. **位精确**：`vaddq_uN`↔uintN_t 加（模 2^N wrap）、`vsubq`↔减、`veorq`↔`^`、`vandq`↔`&`、`vorrq`↔`|`、`vbicq(a,b)`↔`a & ~b`、`vornq(a,b)`↔`a | ~b`、`vmulq_s32`↔uint32 乘（低 32 位与符号无关）、`vmla` 类↔`a*b+c`。ROR 类（后续 sve_rot 族会遇到）：`(x >> n) | (x << (w - n))`。
5. **防自动向量化**：标量 golden 循环带 switch 相位分派（GCC 向量化器不穿过 switch）；每波构建后 objdump 验证 init 符号无向量 ALU 指令（见 V4）。若验证失败，加 `#pragma GCC optimize("no-tree-vectorize")` 再验。
6. **类型最小差**：expected 缓冲保持原向量类型（`uint64x2_t*` 等），经 `reinterpret_cast<uintN_t*>` 逐元素填标量值——字节布局与 dump 路径零变化。
7. **失败闸门必须是标量 memcmp**：misc 族现状已满足（`vec_eq` 等 NEON 比较只是闸门触发后的冷路径诊断分类器，不是闸门）。后来者迁移其它族时必须检查此点：若闸门本身是向量比较，持久比较单元故障可造成假 PASS，须把闸门换成 memcmp、向量比较降级为诊断。

### 模板代码（按形态分类；文件映射见任务表）

**T-A：并行双源 uint64x2_t，相位 = 向量索引 % 4，{add,eor,and,orr}**（最常见）

```c
    /* golden (issue #373): scalar independent recomputation — pure C integer
     * ops on the scalar pipeline, never the NEON units under test. A persistent
     * NEON datapath fault (stuck lane / unit computing deterministic wrong
     * bits) can no longer corrupt golden and actual identically; it now
     * surfaces as a golden-vs-actual byte difference at the memcmp. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * COUNT_MACRO; e++) {
            switch ((e / 2) % 4) {   /* op phase: vector index % 4 */
                case 0: ex[e] = sa[e] + sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & sb[e]; break;
                case 3: ex[e] = sa[e] | sb[e]; break;
            }
        }
    }
```

（COUNT_MACRO 为该文件的元素数宏；e 是标量元素下标，2 个/向量，lane k = e%2。）

**T-A1：单源 + 常量第二操作数**（neon_rot_1src，常量 `0x5A5A5A5A5A5A5A5AULL`）——同 T-A 但 `sb[e]` 换常量：
```c
                case 0: ex[e] = sa[e] + 0x5A5A5A5A5A5A5A5AULL; break;
                case 1: ex[e] = sa[e] ^ 0x5A5A5A5A5A5A5A5AULL; break;
                case 2: ex[e] = sa[e] & 0x5A5A5A5A5A5A5A5AULL; break;
                case 3: ex[e] = sa[e] | 0x5A5A5A5A5A5A5A5AULL; break;
```

**T-A32：uint32x4_t**（neon_rot_u32x4）：元素数 `4 * COUNT_MACRO`，相位 `(e / 4) % 4`，`sa/sb/ex` 为 `uint32_t*`，运算符同 T-A（uint32_t 模 2^32 wrap 位精确）。

**T-A8：uint8x16_t**（neon_rot_u8x16）：元素数 `16 * COUNT_MACRO`，相位 `(e / 16) % 4`，`uint8_t*`；case 0 写 `ex[e] = static_cast<uint8_t>(sa[e] + sb[e]);`（回窄即模 256 wrap），其余 `^ & |` 同型。

**T-SUB：{vsubq,veorq,vbicq,vornq}_u64**（neon_rot_ldr_at_top_sub）：同 T-A 骨架，case 体换：
```c
                case 0: ex[e] = sa[e] - sb[e]; break;
                case 1: ex[e] = sa[e] ^ sb[e]; break;
                case 2: ex[e] = sa[e] & ~sb[e]; break;
                case 3: ex[e] = sa[e] | ~sb[e]; break;
```

**T-OD：3 路交织源**（neon_rot_ldr_at_top_offdiag，a=`interleaved[3i+1]`、b=`interleaved[3i+2]`）：
```c
        const uint64_t *s  = reinterpret_cast<const uint64_t *>(data->interleaved);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * COUNT_MACRO; e++) {
            const int i = e / 2, lane = e % 2;
            const uint64_t a = s[2 * (3 * i + 1) + lane];
            const uint64_t b = s[2 * (3 * i + 2) + lane];
            switch (i % 4) {
                case 0: ex[e] = a + b; break;
                case 1: ex[e] = a ^ b; break;
                case 2: ex[e] = a & b; break;
                case 3: ex[e] = a | b; break;
            }
        }
```

**T-K5：双源交织单缓冲**（k5far、k5inter_rand，a=`src[2i]`、b=`src[2i+1]`）：
```c
        const uint64_t *s  = reinterpret_cast<const uint64_t *>(data->src);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * COUNT_MACRO; e++) {
            const int i = e / 2, lane = e % 2;
            const uint64_t a = s[2 * (2 * i)     + lane];
            const uint64_t b = s[2 * (2 * i + 1) + lane];
            switch (i % 4) {
                case 0: ex[e] = a + b; break;
                case 1: ex[e] = a ^ b; break;
                case 2: ex[e] = a & b; break;
                case 3: ex[e] = a | b; break;
            }
        }
```

**T-PF：置换访问序 + 按位置相位**（neon_rot_ldr_at_top_prefetch，`i=(pos*65)%COUNT`、相位 `pos%4`）：
```c
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t       *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int pos = 0; pos < COUNT_MACRO; pos++) {
            const int i = (pos * 65) % COUNT_MACRO;
            for (int lane = 0; lane < 2; lane++) {
                const int e = 2 * i + lane;
                switch (pos % 4) {   /* 相位跟访问位置，与 run 一致 */
                    case 0: ex[e] = sa[e] + sb[e]; break;
                    case 1: ex[e] = sa[e] ^ sb[e]; break;
                    case 2: ex[e] = sa[e] & sb[e]; break;
                    case 3: ex[e] = sa[e] | sb[e]; break;
                }
            }
        }
```

**T-SR：srcref + 错位一格 expected**（neon_rot_stale_reload；expected[0]=srcref[0]，expected[i]=srcref[i-1]，i 为向量下标）：
```c
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t *rf = reinterpret_cast<uint64_t *>(data->srcref);
        uint64_t *ex = reinterpret_cast<uint64_t *>(data->expected);
        for (int e = 0; e < 2 * COUNT_MACRO; e++) {
            switch ((e / 2) % 4) {
                case 0: rf[e] = sa[e] + sb[e]; break;
                case 1: rf[e] = sa[e] ^ sb[e]; break;
                case 2: rf[e] = sa[e] & sb[e]; break;
                case 3: rf[e] = sa[e] | sb[e]; break;
            }
        }
        /* dst golden: rotated by one vector (= 2 scalar elements);
         * dst[0] = preload (= srcref[0], consistent with temp_buf[0] prefill) */
        ex[0] = rf[0];
        ex[1] = rf[1];
        for (int e = 2; e < 2 * COUNT_MACRO; e++)
            ex[e] = rf[e - 2];
```

**T-MLA：收窄 + 乘加族**（neon_rot_ldr_at_top_mla，uint32x4_t；init 先把源**原地收窄**到 0x0000ffff 再算 golden，run 读收窄后的源）：
```c
    /* golden (issue #373): scalar independent recomputation — narrowing +
     * mul/add/sub/eor in pure C (uint32 arithmetic is bit-exact with the s32
     * vector ops: products of 15-bit-masked lanes never overflow s32 anyway). */
    {
        uint32_t *sa = reinterpret_cast<uint32_t *>(data->srcA);
        uint32_t *sb = reinterpret_cast<uint32_t *>(data->srcB);
        uint32_t *ex = reinterpret_cast<uint32_t *>(data->expected);
        for (int e = 0; e < 4 * COUNT_MACRO; e++) {
            const uint32_t a = sa[e] & 0x0000ffffu;   /* narrow_for_mul, scalar */
            const uint32_t b = sb[e] & 0x0000ffffu;
            sa[e] = a;   /* persist narrowed sources; run reads the same bytes */
            sb[e] = b;
            switch ((e / 4) % 4) {
                case 0: ex[e] = a * b;     break;
                case 1: ex[e] = a * b + a; break;
                case 2: ex[e] = a * b - b; break;
                case 3: ex[e] = a ^ b;     break;
            }
        }
    }
```
（原 init 里 `narrow_for_mul` intrinsic 与 `vreinterpretq_s32_u32` 全部不再出现在 golden 路径；`narrow_for_mul` 函数若再无调用点则删除。）

**T-K2：4 类 XOR 折叠校验和 golden**（neon_rot_ldr_at_top_rowmajor_k2pureload；golden 是 `golden_sum[4]` 而非逐元素 expected，XOR 折叠与顺序无关，故 run 的 asc/desc 扫描与 init 逐序折叠结果一致）：
```c
    /* golden checksums (issue #373): scalar independent fold — the same
     * per-op-class XOR accumulation the hot loop performs, computed with
     * pure C integer ops so a persistent NEON fault cannot corrupt both
     * sides identically. */
    {
        const uint64_t *sa = reinterpret_cast<const uint64_t *>(data->srcA);
        const uint64_t *sb = reinterpret_cast<const uint64_t *>(data->srcB);
        uint64_t *gs = reinterpret_cast<uint64_t *>(data->golden_sum);  /* uint64x2_t[4] → 8 lanes */
        for (int k = 0; k < 8; k++) gs[k] = 0;
        for (int i = 0; i < COUNT_MACRO; i++) {
            const uint64_t a0 = sa[2 * i], a1 = sa[2 * i + 1];
            const uint64_t b0 = sb[2 * i], b1 = sb[2 * i + 1];
            uint64_t r0, r1;
            switch (i % 4) {
                case 0:  r0 = a0 + b0; r1 = a1 + b1; break;
                case 1:  r0 = a0 ^ b0; r1 = a1 ^ b1; break;
                case 2:  r0 = a0 & b0; r1 = a1 & b1; break;
                default: r0 = a0 | b0; r1 = a1 | b1; break;
            }
            gs[2 * (i % 4)]     ^= r0;
            gs[2 * (i % 4) + 1] ^= r1;
        }
    }
```

**T-TLB：run 内逐窗口 golden helper**（neon_rot_ldr_at_top_tlbstress；只改 `tlbstress_compute_golden` 函数体，签名与 noinline 不动）：
```c
__attribute__((noinline))
static void tlbstress_compute_golden(uint64x2_t *golden, const uint64x2_t *srcA,
                                     const uint64x2_t *srcB, int n) {
    /* issue #373: scalar independent golden — pure C integer ops on the
     * scalar pipeline, never the NEON units under test, so a persistent NEON
     * datapath fault cannot corrupt golden and actual identically.
     * noinline stays: it is the barrier that keeps GCC from fusing this
     * loop with the hot sweep and deleting the dst stores (2026-09-09). */
    const uint64_t *sa = reinterpret_cast<const uint64_t *>(srcA);
    const uint64_t *sb = reinterpret_cast<const uint64_t *>(srcB);
    uint64_t       *ex = reinterpret_cast<uint64_t *>(golden);
    for (int e = 0; e < 2 * n; e++) {
        switch ((e / 2) % 4) {
            case 0: ex[e] = sa[e] + sb[e]; break;
            case 1: ex[e] = sa[e] ^ sb[e]; break;
            case 2: ex[e] = sa[e] & sb[e]; break;
            case 3: ex[e] = sa[e] | sb[e]; break;
        }
    }
}
```

**T-K7：k7p_init_common 内的公共 golden 循环**（neon_rot_ldr_at_top_rowmajor_k7pattern，一次改动覆盖 4 个测试格 zero/one/rand_gps/hiham）——T-A 原样应用于 `k7p_init_common` 的 golden 循环（pattern fill 的 `store_vec/vcombine/vcreate` 是输入生成，不是 golden 计算，不动；输入生成若被存储通路故障污染，golden 与 run 读到的是同一污染输入，任何输出比对 golden 都无法检出——这超出本 issue 范式，如实在 PR 说明）。

### 文件 → 模板映射（38 个转换目标 + 1 个不改）

| 波次 | 文件 | 模板 | 原 golden 行号 |
|---|---|---|---|
| W1 | neon_rot_ldr_at_top.cpp | T-A | 67-75 |
| W1 | neon_rot_1src.cpp | T-A1 | 56-64 |
| W1 | neon_rot_u32x4.cpp | T-A32 | 62-70 |
| W1 | neon_rot_u8x16.cpp | T-A8 | 64-72 |
| W1 | neon_rot_ldr_at_top_sub.cpp | T-SUB | 65-73 |
| W1 | neon_rot_store_only.cpp | T-A | 63-68 |
| W1 | neon_rot_fwd_probe.cpp | T-A | 68-73 |
| W1 | neon_rot_nop_gap.cpp | T-A | 64-69 |
| W1 | neon_rot_ldr_at_top_str3.cpp | T-A | 72-77 |
| W1 | neon_rot_ldr_at_top_str4.cpp | T-A | 66-71 |
| W2 | neon_rot_ldr_at_top_ldr4.cpp | T-A | 77-82 |
| W2 | neon_rot_ldr_at_top_ldr5.cpp | T-A | 81-86 |
| W2 | neon_rot_ldr_at_top_lsqfill.cpp | T-A | 78-83 |
| W2 | neon_rot_ldr_at_top_lsqfill_v2.cpp | T-A | 113-118 |
| W2 | neon_rot_ldr_at_top_offdiag.cpp | T-OD | 68-78 |
| W2 | neon_rot_ldr_at_top_prefetch.cpp | T-PF | 80-91 |
| W2 | neon_rot_ldr_at_top_priv.cpp | T-A | 63-68 |
| W2 | neon_rot_ldr_at_top_rfport.cpp | T-A | 86-91 |
| W2 | neon_rot_ldr_at_top_rfport_v2.cpp | T-A | 111-116 |
| W2 | neon_rot_ldr_at_top_robfill.cpp | T-A | 114-119 |
| W2 | neon_rot_ldr_at_top_robfill_np.cpp | T-A | 73-78 |
| W2 | neon_rot_ldr_at_top_mla.cpp | T-MLA | 74-89 |
| W3 | neon_rot_ldr_at_top_rowmajor.cpp | T-A | 74-79 |
| W3 | neon_rot_ldr_at_top_rowmajor_dualcmp.cpp | T-A | 96-101 |
| W3 | neon_rot_ldr_at_top_rowmajor_dump.cpp | T-A | 110-115 |
| W3 | neon_rot_ldr_at_top_rowmajor_gps.cpp | T-A | 141-146 |
| W3 | neon_rot_ldr_at_top_rowmajor_k2pureload.cpp | T-K2 | 125-142 |
| W3 | neon_rot_ldr_at_top_rowmajor_k3res.cpp | T-A | 111-116 |
| W3 | neon_rot_ldr_at_top_rowmajor_k4bgap.cpp | T-A | 102-107 |
| W3 | neon_rot_ldr_at_top_rowmajor_k4gap.cpp | T-A | 111-116 |
| W3 | neon_rot_ldr_at_top_rowmajor_k5far.cpp | T-K5 | 132-137 |
| W3 | neon_rot_ldr_at_top_rowmajor_k5inter_rand.cpp | T-K5 | 67-76 |
| W3 | neon_rot_ldr_at_top_rowmajor_k7pattern.cpp | T-K7 | 139-146 |
| W3 | neon_rot_ldr_at_top_rowmajor_str3_checkall.cpp | T-A | 212-217 |
| W3 | neon_rot_ldr_at_top_str4_checkall.cpp | T-A | 225-230 |
| W3 | neon_rot_ldr_at_top_dump.cpp | T-A | 105-110 |
| W3 | neon_rot_stale_reload.cpp | T-SR | 64-77 |
| W4 | neon_rot_ldr_at_top_tlbstress.cpp | T-TLB | 93-104 |
| 不改 | neon_rot_ldr_at_top_rowmajor_scalar64.cpp | — | 已是全标量（golden 与 run 都是纯 C 标量）。它是"标量-对-标量同码"，属另一层缺口（标量 ALU 持久故障同样双侧同错）；本范式（golden 独立于被测通路）对它需要一个非标量 golden，方向相反，超出本棒范围——在 PR 与 issue 评论中如实标注，留给后续棒决策。 |

行号是 2026-11-01 main 快照值，执行时以实际文件内容为准（先 Read 窗口再 Edit）。

### 验证协议（每个转换的测试都必须过，缺一不可）

- **V1 单测**：`./builddir/sdcshield -e <test> -t 10 -n 4` → 输出含 `exit: pass`。
- **V2 同 seed 双跑**：`./builddir/sdcshield -e <test> -t 10 -n 4 -s 'AES:issue373'` 连跑两次均 pass（若 `-s` 语法报错，退化为背靠背双跑并记录；golden/actual 结构上读同一输入字节，确定性不依赖 RNG）。
- **V3 波次构建**：`SDC_BUILD_JOBS=20 ./scripts/build.sh` 零新警告。
- **V4 objdump 防向量化门**：对该波每个测试的 init 符号（tlbstress 为 `tlbstress_compute_golden`）反汇编，向量 ALU 指令（add/sub/eor/and/orr/bic/orn/mul/mla 等带 `vN.` 操作数）必须零命中（mov/str q 的 fill 与纯数据搬运指令允许——搬运不是 golden 计算，load/store 通路故障对 golden 搬运与 run 侧重算的影响不同构，miscompare 仍会触发）。**必须用 `scripts/check-scalar-golden.sh <binary> <symbol>...`**：它自行解析 C++ mangled 符号名并按精确地址范围反汇编，符号解析失败或空反汇编记 FAIL。陷阱记录（2026-11-01 实测踩中）：直接 `objdump --disassemble=<未修饰名>` 对 C++ static 函数返回**空**反汇编，grep 零命中是**假阴性 PASS**；k7p_init_common 会被内联进 4 个 wrapper（查 wrapper 符号）。
- **V5 注入实验**（一次性，W1 后做）：见 Task 6。
- **V6 回归**：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- **V7 x86 目检**：所有改动文件在 meson aarch64-only 块内；diff 只含 init golden 循环（减少 arch 代码）→ x86 非回归成立。

---

### Task 1: 分支与基线

**Files:** 无代码改动；git 操作。

- [x] 确认 worktree 分支从 origin/main 最新分出（已是）。
- [x] `git checkout -b feat/issue373-scalar-golden-neon-rot`。
- [x] 基线构建 + 基线单测抽样（neon_rot_ldr_at_top 转换前 pass 一次，确认起点干净）。
- [x] 提交本计划文档：`git add docs/superpowers/plans/2026-11-01-issue373-scalar-golden-neon-rot.md && git commit -s -m "docs(plan): issue #373 第一棒——neon_rot 族标量独立 golden 范式与迁移计划"`。

### Task 2: Wave 1 —— 范式立桩（10 文件，T-A/A1/A32/A8/SUB）

**Files:** 见映射表 W1 行。**Interfaces:** 无（纯测试内部改动）。

- [x] 逐文件：Read init 窗口 → 按模板替换 golden 循环（保留原注释语义改写为 issue #373 注释）→ Edit。
- [x] V3 构建（零新警告）。
- [x] V1+V2：10 个测试逐个 `-t 10 -n 4` pass + 同 seed 双跑稳定。
- [x] V4：objdump 门过 10 个 init 符号。
- [x] Commit（-s）：`fix(tests): neon_rot W1 同码 golden→标量独立 golden（issue #373 范式立桩）`。

### Task 3: Wave 2 —— ldr_at_top 探针变体（12 文件）

同 Task 2 流程（模板 T-A ×10、T-OD、T-PF、T-MLA）。Commit：`fix(tests): neon_rot W2 ldr_at_top 变体族同码 golden→标量独立 golden`。

### Task 4: Wave 3 —— rowmajor/dump/checkall 族（15 文件）

同 Task 2 流程（T-A ×11、T-K5 ×2、T-K2、T-K7、T-SR）。k7pattern 覆盖 4 个测试格，4 格都要 V1/V2。Commit：`fix(tests): neon_rot W3 rowmajor/dump 族同码 golden→标量独立 golden`。

### Task 5: Wave 4 —— tlbstress（run 内 golden helper）

T-TLB：只改 `tlbstress_compute_golden` 函数体。V1/V2/V4（objdump 查 `tlbstress_compute_golden` 符号）。Commit：`fix(tests): neon_rot tlbstress 逐窗口 golden 改标量独立复算`。

### Task 6: 注入实验（检出能力的直接证据，不进 commit）

**Files:** 临时 patch `tests/cpu/misc/neon_rot_ldr_at_top.cpp`，实验后 `git checkout --` 还原。

- [x] **A 步（旧 golden 零检出复现）**：在**未转换**的源上（Task 2 之前先做此步，或 `git stash`/临时还原），把 init 与 run 的 case 0 同时改为 `veorq_u64(vaddq_u64(a, b), vdupq_n_u64(1ULL << 41))`（模拟 NEON 加法单元持久故障：两条路径同错一位）→ 增量构建 → `-e neon_rot_ldr_at_top -t 10 -n 4` → **预期 pass**（持久故障被同码 golden 吞掉——零检出实证）。记录完整输出。
- [x] **B 步（新 golden 检出证明)**：在已转换源上只给 run 的 case 0 注入同一位错 → 增量构建 → 同命令 → **预期 FAIL，报 `neon_rot_ldr_at_top data miscompare`**。记录完整输出。
- [x] 还原 patch，重建，确认 `-e neon_rot_ldr_at_top -t 10 -n 4` 回到 pass。
- [x] 边界诚实声明：源级注入等效模拟单元故障（任务卡认可）；gem5-fi CHAOS 真·单 lane 持久注入演示留给后续棒（issue 验收方向的原话），在 PR 正文注明。

### Task 7: 回归、文档同步、收尾

- [x] V6 回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`（实测：rebase 前后各一次均 pass）。
- [x] V3 最终全量构建零新警告；V7 x86 目检记录（39 文件全在 aarch64-only meson 块；全树干净重建仅 2 条存量警告在 tests/cpu/mesh/mesh_upi_{avx,sse}_sym.cpp，origin/main #308/#331 引入，不在本 diff）。
- [x] 文档同步（CLAUDE.md 规则 7）：`docs/coding/writing_tests.md` 增补"golden 必须独立于被测数据通路（issue #373 范式）"条目，指向本计划为模板；README 无 golden 语义表述，不动。
- [x] Commit：`docs(tests): golden 独立性范式写入 writing_tests.md（issue #373）`。
- [x] push feature 分支；开 PR #412：正文含范式说明 + 完成清单 + 注入实验输出 + 剩余清单 + `Refs #373`（**不写 Fixes，issue 不关闭**）。
- [x] code-review（/code-review high：4 角度并行 + 实证复核）→ 全部发现处置（见下）→ merge。
- [x] 向编排者汇报：完成度、范式要点、验证证据、PR 链接、剩余清单。

**检视处置记录（2026-11-01）**：零功能性缺陷；采纳 6 项加固——(1) mla 注释的"乘积不溢出 s32"说法错误（0xffff×0xffff=0xFFFE0001 会进符号位，位精确的真实理由是 uint32 模 2^32 == s32 低 32 位），三处注释改正并警示 int32_t 改写会是 UB；(2) stale_reload 注释补边界（旋转搬运被 GCC 向量化为 ldr/str q 属数据搬运非 golden 计算，检出语义不受影响）；(3) 新增 `scripts/check-scalar-golden.sh` 机械门（mangled 符号解析 + 空反汇编记 FAIL，堵住 V4 假阴性 PASS 陷阱；负向对照：run 符号被抓出 4 条向量 ALU；正向：43/43 符号 PASS）；(4) writing_tests.md 的族 glob 限定排除 scalar64；(5) 剩余清单补 `tests/cpu/misc/core179_kernel_sdc_repro.cpp` 与 `tests/cpu/arm-0102/arm0102_kreg_mask.cpp`（非 sve 变体，原清单漏计）；(6) 先例文件 neon_rot_u8x16_sve.cpp 过时注释修正（与 2src_sve 同类）。

## Self-Review

- 覆盖检查：issue 的 misc/neon_rot 族 39 文件 = 38 转换（W1-W4 全列出）+ 1 不改（scalar64，理由与去向已写）；范式规则/先例引用/注入实验/验收边界（gem5-fi 留待）均有着落；"run 侧比对代码不变"与"闸门必须标量"的张力已用事实解决（misc 族闸门本就是 memcmp）。
- 占位符扫描：无 TBD/TODO；每模板给全码；每文件给行号与模板名。
- 类型一致性：模板统一 `reinterpret_cast<uintN_t*>`、`ex/sa/sb/gs/rf` 命名、`COUNT_MACRO` 占位符在映射表语境下指各文件宏（执行者 Read 后代入实际宏名）。

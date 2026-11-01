# Issue #374：movbe 自逆往返中间值 golden + L1/L0 恒真族独立 golden + sched_storm 语义分级

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 movbe 族 44 个测试"存而不比"的 swapped 中间值升级为对照**独立标量 golden** 的逐字节校验（关闭 `bswap∘bswap==x` 自逆恒等式对对称持久 REV/TBL 故障的盲区），并修复缺口 B 四族（partial_store_forwarding 副本互比、memcpy_rewr 链式互比、cachebounce 纯计数、sched_storm×4 零校验挂 PROD）。

**Architecture:** 热路径**零改动**——这些测试是 core-179 SDC 的已确认触发器，树内多处注释（movbe_dump.cpp:61-76、movbe_l1d_variants.cpp:43-51、movbe_str_multiload.cpp:45-48）实证过热循环任何指令调度变化会灭活触发。因此 golden 在 `test_init` 一次算好（256 字节恒等 LUT 查表重排，LUT 的内存载入阻断 GCC bswap 惯用式识别把它融合回 `rev` 指令），校验放在 `TEST_LOOP` 退出后的一次全量 sweep：每个元素每次迭代写入的 swapped 值只依赖共享只读 input，终态确定性成立，持久错排必被逐字节抓到。

**Tech Stack:** C++20 / GCC（aarch64；`__builtin_bswap32`→`rev`、SVE `svtbl_u8`）、meson/ninja、sdcshield 框架（TEST_LOOP / random32 / log_error / report_fail_msg(noreturn)）。

**Spec:** GitHub issue #374（`gh issue view 374`）；母卡 #334；姊妹先例 PR #412（issue #373 第一棒，范式模板 `docs/superpowers/plans/2026-11-01-issue373-scalar-golden-neon-rot.md` + 机械门 `scripts/check-scalar-golden.sh` + `docs/coding/writing_tests.md` golden 独立性条目）。

## Global Constraints

- CLAUDE.md 全部规则生效；**绝不 push main**；`git commit -s` 尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By。
- **热循环零改动**：所有 run 侧 `TEST_LOOP` 循环体内的指令序列不动（asm 块天然钉死；C 变体用 objdump 前后对照证明仅追加 sweep）。`movbe_dump_golden_sve` / `movbe_l1d_vs_l2_sve` 冷路径"复核基准用被测 reload(in2)"是有注释的刻意设计，**不碰**（issue 原文要求）。
- **golden 独立于被测通路**：expected_swapped 用恒等 LUT 查表重排计算——手写移位/或重排会被 GCC `tree-ssa-math-opts` 的 bswap 惯用式识别折叠回 `rev`（被测指令本身），LUT 的内存载入使该折叠不可能；机械门 `scripts/check-bswap-golden.sh` 钉住（init 符号内零 `rev/rev16/rev32` 标量指令 + 零向量 ALU + 空反汇编记 FAIL）。
- **x86-64 untouched**：movbe 33 文件、sched_storm×4、memcpy_rewr 全部注册在 aarch64-only meson 块（2026-10-31 已核）；partial_store_forwarding 与 cachebounce 是跨架构文件，只做两侧同享的纯 C 可移植增强，不引入 arch 分叉。
- 构建零新警告：`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `SDC_BUILD_JOBS=20 ./scripts/build.sh`。
- 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- 提交粒度按波次；每波提交前该波所有测试逐个 V1 验证通过。
- gem5-fi 边界：本卡改动是**校验语义升级**（init golden + post-loop sweep + 降级），不改变任何 SKIP/run 路径的 ISA 依赖；本机（SVE-256）可直接运行全部 ARM64+SVE 变体做 V1。注入实验按任务卡以"等效单元故障源级注入"演示（issue 验收方向原文），gem5-fi CHAOS 真·单 lane 持久注入属后续棒。

---

## 范式（本计划核心产出）

### 零检出机理（自逆恒等式的对称故障盲区）

现状校验：`val = input[i]; val = bswap(val); swapped[i] = val; val = bswap(val); if (val != input[i]) fail`。
编译器把第二个 bswap 折叠为 `input[i]` 的 reload（store 可能 alias input），实际比对是 **reload vs 首读**——瞬态翻转与加载/存储路径损坏**可检出**（这是测试的既有语义，保留）。但 swapped 中间值是**写死数据**：REV/TBL 单元固定错排 P（P 为对合时 P∘P=id）两次交换同时错、互相抵消，恒等式照样成立——**持久对称故障零检出**。SVE 版 `svtbl∘svtbl` 同理（错排索引表自乘复原）。

### 修法三件套（照抄 #412 防同源结构）

1. **init 一次算好**：`expected_swapped[i] = lut_bswap32(input[i])`（SVE 版同一标量函数复算 `svtbl` 的字节重排结果——字节层面与 VL 无关）。
2. **相位阻断自动向量化/惯用式识别**：LUT 查表载入天然阻断 `rev` 融合；机械门逐符号反汇编验证。
3. **TEST_LOOP 后全量 sweep**：终态确定性论证——input 共享只读、每次迭代对元素 i 写入同一个 `bswap(input[i])`，故 sweep 时 `swapped[i]` 必等于 golden；持久错排 → 全部元素错 → 逐字节 FAIL。多 worker 并发写同值 + 并发 sweep：按值同一性无竞争。

### 诚实边界（写入 PR）

- **mercurial（偶发）REV 故障**：post-loop sweep 只覆盖最后一个内层迭代写入的终态，早先迭代的偶发错排在后续覆写中丢失——本修法对此从 0% 提升到"末迭代覆盖"，非 100%。逐迭代校验需要动热循环（加 ldr+cmp+branch ≈ +2 指令/元素且插入点破坏 str→ldr 邻接），会灭活 core-179 触发，不做。既有 reload-vs-首读比对不受影响。
- **probe_a（arm64+sve）不修**：该变体的孤立变量就是"无 store/纯寄存器"，任何状态外泄都会摧毁实验设计；诚实标注描述串。
- **probe_c（arm64+sve）合法集校验**：同地址 SLF 设计使迭代奇偶不可后知，终态 ∈ {seed, bswap(seed)} 双合法态校验——非对合持久错排 100% 抓、对合错排仅奇偶为奇时抓（约半数 worker）。
- **v_mul/v_add 的 mul/add 载荷 golden 是同单元标量复算**（无廉价独立平方/加法路径）；这两个变体不含字节交换单元，issue 目标故障类（REV/TBL 错排）不适用于它们，其 reload 路径检测本就独立（首读寄存器基准）。
- **cachebounce / partial_store_forwarding 是跨架构文件**：改动为纯 C 可移植代码，x86 构建同样获得更强校验（非回归，PR 中如实声明）。

### 模板

**T-LUT（每文件独立的 golden 三函数，static 防 ODR）**

```c
/* issue #374: identity LUT for the scalar-independent byte-swap golden.
 * The table loads block GCC's bswap-idiom recognizer from folding the
 * shift/or reassembly back into a REV instruction -- the exact unit under
 * test -- so a persistent REV mispermutation can never corrupt golden and
 * actual identically. Pinned by scripts/check-bswap-golden.sh (zero
 * rev/rev16/rev32 and zero vector-ALU inside the init symbol). */
static uint8_t g_bswap_lut[256];
static void movbe_golden_lut_init(void)
{
    for (int b = 0; b < 256; ++b)
        g_bswap_lut[b] = (uint8_t)b;
}
static uint32_t movbe_golden_bswap32(uint32_t x)
{
    return ((uint32_t)g_bswap_lut[x & 0xff] << 24) |
           ((uint32_t)g_bswap_lut[(x >> 8) & 0xff] << 16) |
           ((uint32_t)g_bswap_lut[(x >> 16) & 0xff] << 8) |
           (uint32_t)g_bswap_lut[x >> 24];
}
```

**T-C（C 往返族：struct +1 字段，init +golden 填充，run +post-loop sweep）**

struct 加 `uint32_t *expected_swapped;`；init 在 random32 填充后：

```c
    movbe_golden_lut_init();
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i)
        data->expected_swapped[i] = movbe_golden_bswap32(data->input[i]);
```

run 在 TEST_LOOP 闭括号后、`return EXIT_SUCCESS;` 前：

```c
    /* issue #374: post-loop verification of the stored intermediate. The hot
     * loop above is unchanged (the core-179 str->ldr trigger window is
     * untouched); every iteration stores the same bswap32(input[i]) to
     * swapped[i], so the final content must equal the init-time LUT golden.
     * A persistent REV mispermutation (symmetric fault: the wrong permutation
     * is applied twice and cancels in the round-trip identity) is caught here
     * byte-exactly. */
    for (size_t i = 0; i < MOVBE_BUFFER_SIZE; ++i) {
        if (data->swapped[i] != data->expected_swapped[i]) {
            log_error("MovBE: swapped-intermediate miscompare at index %u: "
                      "input=0x%08X golden=0x%08X actual=0x%08X xor=0x%08X",
                      (unsigned)i, data->input[i], data->expected_swapped[i],
                      data->swapped[i], data->expected_swapped[i] ^ data->swapped[i]);
            report_fail_msg("MovBE: swapped buffer != independent golden at index %u",
                            (unsigned)i);
        }
    }
```

cleanup 加 `free(data->expected_swapped);`。

**T-C-SLOT（f/g1/g2/h：存储索引带掩码/步进）**：sweep 改为按槽位遍历，期望值取"该槽最后写入者"的 golden。`i & 15`（f）：slot s ← 元素 `SIZE-16+s`；`(i&15)*scale`（g1: scale=65536, g2: scale=32）：slot k ← 元素 `SIZE-16+k`；`i & mask`（h, mask+1 整除 SIZE）：slot s ← 元素 `SIZE-(mask+1)+s`。h 的 mask 运行期读全局 `g_probe_h_mask`（与 store 同源同值）。

**T-C-SINK（probe_d：全局单值 sink）**：sweep 单点 `probe_d_sink == expected_swapped[SIZE-1]`（所有线程写同值，终态确定）。

**T-C-LEGALSET（probe_c：同地址 SLF 奇偶不可知）**：终态 `tls.input[i]` ∈ {`data->input[i]`, `expected_swapped[i]`}，否则 FAIL；注释写明非对合 100%/对合仅奇偶奇数时抓的边界。

**T-ASM（movbe_l1d_variants ×10 + str_multiload）**：struct/init/cleanup 同 T-C（`movbe_v_data`）；10 个 run 各自在 TEST_LOOP 后调用共享 sweep helper（rev 系 8 个用 `expected_swapped`；v_mul/v_add 内联 6 行 sweep，期望 `input[i]*input[i]` / `input[i]+input[i]` 标量复算，边界见上）。asm 块零触碰。

**T-SVE（svtbl 族标准 12 文件）**：同 T-C，sweep 为纯标量循环（`data->swapped[i] != expected_swapped[i]`，与 VL 无关）；可配 count 的文件 sweep 上界用 `data->count`。`movbe_dump_golden_sve` / `movbe_l1d_vs_l2_sve` 的冷路径与 asm reload helper **零触碰**，只加 init golden + post-loop sweep。

**T-SVE-SLOT（f/g1/g2/h_sve）**：slot k ← 最后一个 `base/lanes ≡ k (mod 16)`（或 `base/128 % h_lines`）的向量；期望 = 该向量元素集的 LUT golden 逐字节比对（probe_f_sve 的 `svdup_u32(first)` 单值槽按文件实读后定模板）。

**T-SLF（slf_sweep_sve）**：实读确认三宽度遍是否都在 TEST_LOOP 体内、priv 是否每遍前 memcpy 复位；终态 = 最后宽度（8B）置换(golden)，sweep 用 LUT 逐字节复算该置换比对。既有 init 自检与 per-iteration restore-vs-golden 比对零触碰。

**T-PSF（partial_store_forwarding）**：读侧独立 golden——load 范围的**store 前**内容先捕获（`pre = read_mem(real_buf+l_off,l_sz)`，在 store 之前，不进入 store→load 转发窗口），store 后期望值 = 寄存器拼接 `splice(pre, s_val)`（重叠字节取 s_val，其余取 pre）；`real_val != expected` FAIL。保留双写刺激与 shadow 差异校验（次级）。`_sve` 姊妹文件实读后套同型。

**T-MEMCPY（memcpy_rewr）**：m_tmp 填充从逐字节 `i ^ myget_cycles()` 改为块级常量 `c = myget_cycles()` 一次读出（`m_tmp_p[i] = (unsigned char)((uint64_t)i ^ c)`），`func_data_make` 在同一 mutex 区写 `st->golden_cycles = c`；init 对 `gl_tester.b1` 做一致的 `i ^ C0` 初始化（C0=0 记入 golden_cycles）；消费端在 mutex 内 snapshot C，`func_struct_test2` 增加算术 golden 校验 `b1[i] == (unsigned char)((uint64_t)i ^ C_snap)`（覆盖 m_tmp→gl_tester.b1→b1 全传输链）。b2..b5 互比保留。

**T-BOUNCE（cachebounce）**：SharedData 加 per-CPU 寄存器计数落点 `local_adds[device_count()]`（aligned_alloc，init 清零）；run 里 `uint64_t local = 0` 随循环 `++local`，TEST 循环退出后一次写入 `data->local_adds[cpu]`；finish 求和 Σlocals 并与 counter、total_adds 双比对——对称丢更新/整迭代丢弃（两原子同丢，现有恒等式盲）→ Σlocals ≠ counter FAIL。热循环仅 +1 add/迭代（寄存器）。

**T-STORM（sched_storm×4）**：`quality_level` PROD→BETA + 描述串已如实（"NOT an SDC probe"/"detector is external"），代码注释引用 #374。理由：四测试为内核行为刺激/负对照，无 SDC 检出语义，不应混入 PROD 默认集（#334 分级）；BETA 保留 `--beta` 可达性供 core-179 testbed。

### 文件 → 模板映射（44 movbe + 缺口 B）

| 波次 | 文件 | 模板 | 测试数 |
|---|---|---|---|
| W2a | tests/cpu/arm64/movbe.cpp | T-C | 1 |
| W2a | tests/cpu/arm64/movbe_dump.cpp | T-C | 1 |
| W2a | tests/cpu/arm64/movbe_dump_probe_b.cpp | T-C | 1 |
| W2a | tests/cpu/arm64/movbe_dump_probe_d.cpp | T-C-SINK | 1 |
| W2a | tests/cpu/arm64/movbe_dump_probe_e.cpp | T-C | 1 |
| W2a | tests/cpu/arm64/movbe_dump_probe_x.cpp | T-C | 1 |
| W2a | tests/cpu/arm64/movbe_dump_probe_xn.cpp | T-C | 1 |
| W2a | tests/cpu/misc/movbe_dump_golden.cpp | T-C | 1 |
| W2a | tests/cpu/misc/movbe_l1d_vs_l2.cpp | T-C（2 run 共用 init/cleanup） | 2 |
| W2b | tests/cpu/arm64/movbe_dump_probe_f.cpp | T-C-SLOT | 1 |
| W2b | tests/cpu/arm64/movbe_dump_probe_g1.cpp | T-C-SLOT | 1 |
| W2b | tests/cpu/arm64/movbe_dump_probe_g2.cpp | T-C-SLOT | 1 |
| W2b | tests/cpu/arm64/movbe_dump_probe_h.cpp | T-C-SLOT | 1 |
| W2c | tests/cpu/arm64/movbe_dump_probe_c.cpp | T-C-LEGALSET | 1 |
| W2c | tests/cpu/arm64/movbe_dump_probe_a.cpp | 标注（不修，边界写描述+注释） | 1 |
| W3 | tests/cpu/misc/movbe_l1d_variants.cpp | T-ASM | 10 |
| W3 | tests/cpu/misc/movbe_str_multiload.cpp | T-ASM | 1 |
| W4a | tests/cpu/sve/case/movbe_sve.cpp | T-SVE | 1 |
| W4a | tests/cpu/sve/case/movbe_dump_sve.cpp | T-SVE | 1 |
| W4a | tests/cpu/sve/case/movbe_dump_golden_sve.cpp | T-SVE（冷路径不碰） | 1 |
| W4a | tests/cpu/sve/case/movbe_l1d_vs_l2_sve.cpp | T-SVE（冷路径不碰） | 2 |
| W4a | tests/cpu/sve/case/movbe_dump_probe_b/d/e/x/xn_sve.cpp | T-SVE | 5 |
| W4b | tests/cpu/sve/case/movbe_dump_probe_f/g1/g2/h_sve.cpp | T-SVE-SLOT | 4 |
| W4b | tests/cpu/sve/case/movbe_dump_probe_c_sve.cpp | T-C-LEGALSET | 1 |
| W4b | tests/cpu/sve/case/movbe_dump_probe_a_sve.cpp | 标注（不修） | 1 |
| W4b | tests/cpu/sve/case/movbe_slf_sweep_sve.cpp | T-SLF | 1 |
| W5 | tests/cpu/misc/partial_store_forwarding.cpp | T-PSF | 1 |
| W5 | tests/cpu/sve/mem/partial_store_forwarding_sve.cpp | T-PSF（实读定型） | 1 |
| W5 | tests/cpu/memory/memcpy_rewr.cpp | T-MEMCPY | 1 |
| W5 | tests/cpu/cache/cachebounce.cpp | T-BOUNCE | 1 |
| W5 | tests/cpu/misc/sched_storm_{baseline,futex,hybrid,thread}.cpp | T-STORM | 4 |

注：`tests/cpu/cache/cache_stress_civac.cpp` 与 movbe 同病（golden=input[i] 重读 + 双 bswap 自逆），issue #374 未列入本卡清单，作为发现写入 PR 移交清单。

### 验证协议（每个改动的测试必须全过）

- **V1 单测**：`./builddir/sdcshield -e <test> -t 10 -n 4` → `exit: pass`。
- **V2 同 seed 双跑**：同命令背靠背双跑均 pass（golden/结构读同一 input 字节，确定性不依赖 RNG）。
- **V3 波次构建**：`SDC_BUILD_JOBS=20 ./scripts/build.sh` 零新警告。
- **V4 机械门**：`scripts/check-bswap-golden.sh builddir/sdcshield <init-symbol>...`——golden 符号内零 `rev/rev16/rev32`（w/x 寄存器）+ 零向量 ALU + 空反汇编 FAIL；负向对照：run 符号必须被抓出 `rev`（证明门有效）。
- **V4b 热循环不变证明**（C 变体）：改动前后 run 符号 objdump 对照，热循环体（含 str→ldr 邻接）指令序列一致，差异仅为尾部追加 sweep（寄存器保存序差异允许但须逐条列出确认不触循环体）。asm 变体：asm 块字面即不变。
- **V5 注入实验**（每子族一次，不进 commit）：(a) C 族——movbe_dump 把第一个 `__builtin_bswap32` 换成 `__builtin_bswap16`×2（对合错排 REV16 语义）→ 旧代码 pass（盲区实证）+ 新 sweep FAIL；(b) asm 族——l1d_variants 的 asm `rev` 改 `rev16` → 同上；(c) SVE 族——svtbl 索引表 `[3,2,1,0]` 改 `[2,3,0,1]`（对合错排，即 issue 验收原文的"TBL 索引表固定错一位"）→ 同上。还原后重建 pass。
- **V6 回归**：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
- **V7 x86 目检**：全部 movbe/sched_storm/memcpy_rewr 改动文件在 aarch64-only meson 块；partial_store_forwarding/cachebounce 跨架构但为两侧同享纯 C 增强。

---

### Task 1: 分支与基线

**Files:** 无代码改动。

- [x] `git checkout -b feat/issue374-movbe-roundtrip-golden`。
- [x] 基线构建（零警告确认）+ `movbe_dump`、`movbe_l1d_v_rev`、`movbe_sve` 各跑一次 `-t 10 -n 4` pass 确认起点干净。
- [x] 基线 run/init 符号 objdump 快照存 /tmp（V4b 对照用）。
- [x] 提交本计划：`git add docs/superpowers/plans/2026-11-01-issue374-movbe-roundtrip-golden.md && git commit -s -m "docs(plan): issue #374 movbe 中间值 golden 与恒真族修复计划"`。

### Task 2: 机械门 scripts/check-bswap-golden.sh

**Files:** Create `scripts/check-bswap-golden.sh`（chmod +x）。

- [x] 照抄 check-scalar-golden.sh 骨架（nm mangled 解析 + 精确地址范围 + 空反汇编 FAIL），检查集 = 标量 `rev/rev16/rev32`（`[wx]` 操作数）∪ 向量 ALU（复用 VALU_RE）。
- [x] 负向对照：对基线二进制跑 `movbe_dump_run` 符号 → 必须 FAIL（含 rev）；`movbe_dump_init` → PASS（现状 init 无 rev）。记录输出。
- [x] Commit：`feat(scripts): check-bswap-golden.sh 机械门——golden 符号零 rev/零向量 ALU（issue #374）`。

### Task 3: W2a —— C 往返族标准 10 文件

**Files:** 见映射表 W2a。逐文件：Read → struct 加字段 → init 加 LUT+golden 填充 → run 加 sweep → cleanup 加 free → Edit。

- [x] 10 文件全部改造（模板 T-C ×9、T-C-SINK ×1）。
- [x] V3 构建；V1+V2 逐测试；V4 机械门过 10 个 init 符号；V4b 抽 3 个文件做热循环对照。
- [x] Commit：`fix(tests): movbe C 族 swapped 中间值→独立 LUT golden + post-loop sweep（issue #374 W2a）`。

### Task 4: W2b —— slot 族 4 文件

同 Task 3 流程（T-C-SLOT；h 读运行期 mask）。

- [x] 4 文件改造 + V1/V2/V4；V4b 抽 1 个对照。
- [x] Commit：`fix(tests): movbe slot 探针族 swapped 终态→按槽位 golden 校验（issue #374 W2b）`。

### Task 5: W2c —— probe_c 合法集 + probe_a 标注

- [x] probe_c：终态双合法态校验 + 边界注释；probe_a：描述串与注释如实标注"无状态外泄变体，中间值校验结构性不可行（issue #374 边界）"。
- [x] V1/V2（probe_a 仅 V1——无行为变化）；Commit：`fix(tests): movbe probe_c 终态合法集校验 + probe_a 零检出边界标注（issue #374 W2c）`。

### Task 6: W3 —— asm 族 2 文件 11 测试

- [x] movbe_l1d_variants：init/cleanup 扩展 + 共享 sweep helper + 10 个 run 尾部各加调用（v_mul/v_add 内联期望）。
- [x] movbe_str_multiload：T-C（asm 零触碰；既有 in-loop expected 不动，仅加 sweep）。
- [x] V1/V2 ×11、V4（movbe_v_init、movbe_init 符号）；Commit：`fix(tests): movbe asm 族 swapped 中间值→独立 golden（issue #374 W3）`。

### Task 7: 注入实验 V5（C/asm/SVE 三子族，临时 patch 不进 commit）

- [x] 按 V5 (a)(b)(c) 执行：旧代码盲区 pass 实证 → 新校验 FAIL 证明 → 还原重建 pass。记录完整输出。
- [x] SVE 注入在 W4 完成后补做（顺序调整：(a)(b) 此时做，(c) 在 Task 9 后）。

### Task 8: W4a —— SVE 标准 9 文件 11 测试

- [x] 逐文件 T-SVE 改造；dump_golden_sve/l1d_vs_l2_sve 冷路径零触碰确认（diff 检查）。
- [x] V1/V2/V4（sve init 符号）；Commit：`fix(tests): movbe sve 族 swapped 中间值→标量 LUT golden（issue #374 W4a）`。

### Task 9: W4b —— SVE slot/特殊 7 文件 7 测试 + SVE 注入

- [x] f/g1/g2/h_sve（实读定模板）、probe_c_sve（合法集）、probe_a_sve（标注）、slf_sweep_sve（实读定终态置换）。
- [x] V5(c) SVE 索引表错排注入。
- [x] V1/V2/V4；Commit：`fix(tests): movbe sve slot/SLF 族中间值校验 + 注入证明（issue #374 W4b）`。

### Task 10: W5 —— 缺口 B 8 文件 9 测试

- [x] partial_store_forwarding（+实读 _sve 套同型）、memcpy_rewr、cachebounce、sched_storm×4。
- [x] V1/V2（sched_storm BETA 后 `-e` 显式点名仍可跑，验证 quality 过滤不拦显式请求）；memcpy_rewr 按 `-n ≥2` 跑（要求 ≥2 线程）；Commit：`fix(tests): 恒真族独立 golden——PSF 拼接 golden/memcpy 算术 golden/cachebounce 寄存器计数/sched_storm 降 BETA（issue #374 W5）`。

### Task 11: 文档同步 + 回归 + 收尾

- [x] `docs/coding/writing_tests.md` golden 独立性条目增补"自逆往返中间值必须校验"与 LUT 防融合要点（引用本计划）。
- [x] V6 回归；V7 x86 目检记录；最终全量构建零新警告。
- [x] Commit：`docs(tests): 自逆往返中间值校验范式写入 writing_tests.md（issue #374）`。

### Task 12: PR 与闭环

- [x] push feature 分支；PR 正文：范式说明、完成清单、注入实验输出、诚实边界、移交清单（cache_stress_civac 同病、probe_a 结构边界、mercurial 覆盖边界、gem5-fi 后续棒）、`Refs #374`（不写 Fixes）。
- [x] /code-review high → 处置全部发现 → merge；issue #374 留移交评论。

## Self-Review

- 覆盖检查：issue 缺口 A 44 测试全数入映射表（42 修复 + probe_a×2 标注边界）；缺口 B 四族各有模板；两处"不要动"（dump_golden_sve/l1d_vs_l2_sve 冷路径）已在约束与模板双处声明；注入实验覆盖三子族。
- 占位符扫描：T-SVE-SLOT/T-SLF/T-PSF(_sve) 标注"实读后定"是执行期动作（Edit 前必须 Read），非占位符——模板主体已给全。
- 类型一致性：`expected_swapped`/`movbe_golden_bswap32`/`g_bswap_lut`/`movbe_golden_lut_init` 全计划统一命名；sweep 上界 MOVBE_BUFFER_SIZE vs data->count 的分属（可配 count 的文件用 count）已在模板写明。

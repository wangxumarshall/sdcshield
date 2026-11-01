# Issue #376：链终态-only → 分段中间检查点（~18 测试，gem5-fi 注入验收）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [x]`) syntax for tracking.

**Goal:** 把 sve512_*/power_virus 族中"只在链终点比对终态"的累积型链测试改为**分段中间检查点**——链切成 K 段、每段边界把累积器溢出到对照缓冲与标量独立 golden 字节比对，消除"中途 bit 翻转被后续加法/舍入吸收"的漏检盲区，并用 gem5-fi CHAOS 单 bit 注入验收。

**Architecture:** 只增加检查点，不改链本身——热循环内仅在段边界加一次 `svst1`（累积器寄存器跨 store 连续，FMA 依赖链不断）；golden 在同一趟标量复算里顺带记录各段边界状态（零额外代价）；段表在链外（epilogue）比对。既有终态比对被最后一段检查点包含（语义只增不减）。gem5-fi 侧新建同源双内核 `chain_ckpt_dual.c`（final-only / checkpoint 两种模式），同一注入分别演示"旧语义吞掉、新语义抓到"。

**Tech Stack:** C++20 / GCC（aarch64, arm_sve.h / arm_neon.h）、meson/ninja、sdcshield 框架（TEST_LOOP / log_warning / report_fail_msg）、gem5-fi CHAOS（CHAOSFPU/CHAOSPhysReg bit_flip）。

**Spec:** GitHub issue #376（`gh issue view 376`）。母卡 #334；姊妹先例：PR #412（标量独立 golden 范式 + `scripts/check-scalar-golden.sh`）、PR #416（gem5-fi 四档证据链台账组织）。#334 拆卡中本卡只管"链终态-only → 分段检查点"这一层。

## Global Constraints

- CLAUDE.md 全部规则生效；比对语义保持**字节一致**（本计划只增加比对点，不改比对方式；f64_special 的类别比对、stencil B 的容差比对按原语义分段化）。
- **x86-64 untouched**：全部目标文件在 `tests/cpu/meson.build` 的 aarch64 块内（目检确认）；power_virus_dit.cpp 虽有 `#else` SKIP 分支，改动只落在 `#ifdef __aarch64__` 内。
- 防超时红线（#360/#349/#326）：段边界 store 每 64 步一次（512 步链 = 8 次，指令占比 1.6%）；`-t 2000` 不得超时；每个测试改动前后 `-t 2000 -n 1` 墙钟对比记录。
- 构建零新警告：`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `SDC_BUILD_JOBS=20 ./scripts/build.sh`（并发 agent 禁满核）。
- 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- DCO：`git commit -s`，尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By；只 push feature 分支 `feat/issue376-chain-checkpoints`，**绝不 push main**。
- 提交粒度：一波一提交（CLAUDE.md 规则 1 的"模板族机械迁移按波次"先例）；每波提交前该波全部测试逐个验证通过。
- gem5-fi：`/home/sdc/wangxu/gem5-fi`，`CHAOS/gem5/build/ARM/gem5.opt`（md5 `3f9011f40dbe80ec1df478c62ddb0b06`），SE 模式 `configs/se/arm_chaos.py`；`SDCSHIELD_FAILS` 行为准（gem5 SE 不可靠传播 guest exit code）。

---

## 机理与分段粒度论证（本计划的核心依据，实证于 2026-11-01）

### 吸收的算术机理

FMA 累积链 `acc = fma(m, a, acc)` 中，累积器 mantissa 位 j 的单 bit 翻转 δ = 2^j·ulp(acc)。舍入是周期函数：δ 为当前 ulp 整数倍时 `round(x+δ) = round(x)+δ`——翻转**确定性传播**，直到累积幅度增长 2^j（跨 j 个 binade）使 δ 变 sub-ulp；此后每步舍入以 1−δ/ulp 概率吸收它，几步内磨掉。指数位/符号位翻转 δ 巨大，终态就能抓到。**终态-only 的盲区 = 翻转点之后幅度增长 ≥ 2^j 的那类翻转**——链越长幅度增长越多、盲区越大（与 issue 的"链越长盲区越大"一致）。

### 主机实证（/tmp scratch 标量模拟，sve512_f32_chain 语义：512 步 fmaf 链、|x|≤2 有限带、seed [1,2)，lane 0，30 seeds × 90 个 (step,bit) 格）

| 粒度 | 全格检出率（j=0..20） | 盲区位检出率（j≤3） | 热循环内 store 开销 |
|---|---|---|---|
| 终态-only（现状） | 81.7% | **59.9%** | 0 |
| SEG=128（4 段） | 87.9% | 73.2% | 0.8% |
| **SEG=64（8 段）** | **90.8%** | **79.4%（1.33x）** | **1.6%** |
| SEG=32（16 段） | 92.9% | 84.1%（1.40x） | 3.1% |
| SEG=16（32 段） | 94.3% | 87.1%（1.45x） | 6.3% |

典型格（30 seeds）：bit0@step32 终态 4/30 → SEG64 14/30；bit1@step96 终态 15/30 → SEG64 26/30；bit0@step96 终态 9/30 → SEG64 24/30。

**决策：SEG_LEN=64**（拐点：32→16 只再赚 3.0pp 但开销翻倍；64→32 赚 4.7pp、开销 1.6%→3.1%）。2048 步 burst 用 SEG_LEN=64（32 个检查点）；64 步 special 链用 SEG_LEN=16（4 个检查点）。

**诚实的检测边界**：段内被磨掉的翻转（残差盲区，SEG64 下盲区位仍漏 20.6%）与"翻转时已 sub-ulp"的位（本族生成器不会产生——mantissa 位 j≥0 均 ≥1 ulp）仍漏检。检查点消除的是"增长磨掉"类盲区，不是全部吸收。f32 链可同时评估整数操作数化（issue 修法方向 3），但会改变压力形态（随机有限带 → 整数格点），本棒不动，留给后续棒决策。

### 为什么段边界 store 不稀释压力语义

- FMA 依赖链不断：`acc` 寄存器跨 `svst1` 连续流向下一步 `svmla`，段边界只是旁路观察（store 读 acc，不写）。
- 指令占比 1.6%（8 store / 512 FMA），store 走 LSU pipe 不占 FMA pipe；di/dt burst 语义（power_virus）的电流摆动来自 FMA 密度，1.6% 不改变形态。
- golden 侧零额外代价：同一趟 512 步标量复算顺带记录 8 个边界（8 次 4 字节 store/lane）。
- 段表比对在链外（epilogue），不进热循环。

### 检查点的位一致性论证

golden（逐操作 `std::fma`）与 HW（`svmla`/`vfma`）都是 IEEE 单舍入融合乘加、同操作数同序——既有终态字节一致已证明整链逐操作一致，中间边界状态按同一论证逐点一致（PR #412 范式：golden 独立于被测向量通路）。f64_special 的 NaN 中间态按既有类别比对语义处理（两侧 NaN 类别相等即可，IMP-DEF payload 不比）。

---

## 范式模板代码

### T-CHK32（f32 链，sve512_f32_chain_arm 为例）

```c
// Segmented intermediate checkpoints (issue #376): the chain state is
// byte-compared at SEG_LEN-step boundaries, not only at chain end. A mid-
// chain accumulator bit flip (delta = 2^j ulp) propagates deterministically
// until the accumulated magnitude grows 2^j past it, then gets ground away
// by rounding — final-only comparison is blind to that class (measured:
// low-bit mid-chain flips caught only 59.9% at chain end vs 79.4% with
// 64-step checkpoints). The boundary spill does NOT break the FMA
// dependency chain (acc flows on in the same register); it is 8 stores
// per 512 FMAs (1.6%).
static constexpr size_t SEG_LEN   = 64;
static constexpr size_t SEG_COUNT = CHAIN_STEPS / SEG_LEN;

static void f32_chain_golden(const SveF32ChainData *d, uint32_t *seg_out)
{
    const size_t lanes = d->vl_w;
    for (size_t lane = 0; lane < lanes; ++lane) {
        uint32_t accw = d->seeds_f32[lane];
        float acc;
        memcpy(&acc, &accw, 4);
        for (size_t i = 0; i < CHAIN_STEPS; ++i) {
            float m, a;
            memcpy(&m, &d->m_f32[i * lanes + lane], 4);
            memcpy(&a, &d->a_f32[i * lanes + lane], 4);
            acc = std::fma(m, a, acc);
            if ((i + 1) % SEG_LEN == 0)
                memcpy(&seg_out[((i + 1) / SEG_LEN - 1) * lanes + lane],
                       &acc, 4);
        }
    }
}

static void f32_chain_hw(const SveF32ChainData *d, uint32_t *seg_out)
{
    const size_t lanes = d->vl_w;
    const svbool_t pg = svptrue_b32();
    svfloat32_t acc = svld1_f32(pg, reinterpret_cast<const float *>(d->seeds_f32.data()));
    for (size_t i = 0; i < CHAIN_STEPS; ++i) {
        svfloat32_t m = svld1_f32(pg, reinterpret_cast<const float *>(&d->m_f32[i * lanes]));
        svfloat32_t a = svld1_f32(pg, reinterpret_cast<const float *>(&d->a_f32[i * lanes]));
        acc = svmla_f32_x(pg, acc, m, a);
        if ((i + 1) % SEG_LEN == 0)   /* issue #376 checkpoint spill */
            svst1_f32(pg, reinterpret_cast<float *>(
                          &seg_out[((i + 1) / SEG_LEN - 1) * lanes]), acc);
    }
}
```

run 侧（终态比对被最后一段包含，旧 `gold32/hw` 单向量换成段表）：

```c
    std::vector<uint32_t> gold_seg(SEG_COUNT * d->vl_w);
    std::vector<uint32_t> hw_seg(SEG_COUNT * d->vl_w);
    f32_chain_golden(d, gold_seg.data());

    do {
        bool all_passed = true;
        f32_chain_hw(d, hw_seg.data());
        for (size_t seg = 0; seg < SEG_COUNT; ++seg) {
            for (size_t lane = 0; lane < d->vl_w; ++lane) {
                const size_t idx = seg * d->vl_w + lane;
                if (hw_seg[idx] != gold_seg[idx]) {
                    log_warning("sve512_f32_chain: seg %zu (after step %zu)"
                                " lane %zu golden=0x%08" PRIx32
                                " actual=0x%08" PRIx32 " xor=0x%08" PRIx32,
                                seg, (seg + 1) * SEG_LEN, lane,
                                gold_seg[idx], hw_seg[idx],
                                gold_seg[idx] ^ hw_seg[idx]);
                    all_passed = false;
                }
            }
        }
        if (!all_passed) {
            report_fail_msg("sve512_f32_chain_arm: SVE f32 FMLA chain SDC "
                            "detected (segmented checkpoint)");
            return EXIT_FAILURE;
        }
    } while (test_time_condition(test));
```

（段序 0..K-1 迭代 = 报最早失败段，诊断友好；`report_f32_lane_mismatch` helper 若无其他调用点则删除或改造为带 seg 参数。）

**T-CHK64**：f64 版同型（`uint64_t`/`svfloat64_t`/`svld1_f64`/`8`）。
**T-CHKSVD**：T-CHK32/64 的 per-block 版——golden/hw 加 `row, col` 参数与 `block_slab_base` 寻址不变，段表 `SEG_COUNT * lanes` 尺寸在块循环外分配一次、每块重算重比。
**T-CHKCAT**：f64_special 的类别比对版——段边界记录后，逐段用既有 `f64_lane_ok`（NaN/Inf 类别容差）比对，不用字节比对（中间 NaN 态的 payload 是 IMP-DEF）。
**T-PVNEON**：power_virus_dit 的 NEON 版——`burst_fma_chain` 改为 `burst_fma_chain_seg(int n, uint32_t (*seg_out)[4])`，段边界 `vst1q_u32(seg_out[seg], vreinterpretq_u32_f32(acc))`；reference 同步记录段 golden（`burst_fma_reference_seg`）；比对逐段 4 lane。
**T-PVSVE**：power_virus_dit_sve_arm——burst 循环内 `k % SEG_LEN == 63` 时 `svst1_f64`；golden 的 per-lane 循环同步记录；段表 `32 * vl_d`。
**T-FCMLA**：sve512_fcmla_arm——golden 在 init 算（4 rotations × 512 步），init 循环内每 64 步记录段表；HW 链每 64 步 spill；run 逐 rotation 逐段比对。

## 文件 → 波次映射（13 改 + 4 不改）

| 波次 | 文件 | 模板 | 链长×类型 | 风险 |
|---|---|---|---|---|
| W1 | tests/cpu/arm64/sve512_f32_chain_arm.cpp | T-CHK32 | 512 步 f32 累积 | 最高 |
| W1 | tests/cpu/arm64/sve512_f32_chain_svd.cpp | T-CHK32+T-CHKSVD | 512×361 块 f32 | 最高 |
| W1 | tests/cpu/arm64/sve512_fma_arm.cpp | T-CHK32+T-CHK64+T-CHKCAT | f64 512 / special 64 / f32 512 三链 | 最高 |
| W2 | tests/cpu/arm64/sve512_f64_chain_arm.cpp | T-CHK64 | 512 步 f64 | 中 |
| W2 | tests/cpu/arm64/sve512_f64_chain_svd.cpp | T-CHK64+T-CHKSVD | 512×361 块 f64 | 中 |
| W2 | tests/cpu/arm64/sve512_fcmla_arm.cpp | T-FCMLA | 512 步×4 rot f64 | 中 |
| W2 | tests/cpu/arm64/power_virus_dit_sve_arm.cpp | T-PVSVE | 2048 步 f64 纯增长 | 中 |
| W2 | tests/cpu/arm64/power_virus_dit.cpp | T-PVNEON | 2048 步 f32 纯增长（晚期低位吸收） | 中 |
| W3 | tests/cpu/arm64/sve512_f64_special_arm.cpp | T-CHKCAT | 64 步类别比对 | 中高（掩码类） |
| W3 | tests/cpu/arm64/sve512_f64_special_svd.cpp | T-CHKCAT+T-CHKSVD | 64×361 块 | 中高 |
| W3 | tests/cpu/arm64/sve512_stencil_axis_arm.cpp | 读后定（shape B 容差链） | 12-FMA×N | 中高（容差吸收） |
| W3 | tests/cpu/arm64/sve512_nt_reload_arm.cpp | 读后定（精确算术） | 64 步递归 | 低 |
| W3 | tests/cpu/arm64/sve512_fmmla_arm.cpp | 读后定（精确整数） | 256 步×2 tile | 低（issue 点名，顺带做） |
| 不改 | sve512_gather_scatter_arm/_svd.cpp | 已逐轮比对 | — | — |
| 不改 | sve512_xlswave_ctx_arm.cpp | 已逐 sweep 比对 | — | — |
| 不改 | sve512_pred_ops_arm.cpp | 已逐段/逐迭代比对 | — | — |

精确算术链（fmmla 整数操作数、nt_reload 1.5 倍数、stencil A 1/8 格点）无舍入吸收窗（中间值精确可表示、翻转传播到终态）——检查点对其只有"更早失败定位"价值、无检出率增益；W3 实施时逐个读链确认该论断成立再动手，若成立则做统一性分段（低成本）并在 PR 如实标注检出率无增益，若链中存在非精确段（如 stencil B）则该段真有增益。

行号是 2026-11-01 main 快照值，执行时以实际文件内容为准（先 Read 窗口再 Edit）。

## 验证协议（每个改动的测试都必须过，缺一不可）

- **V1 单测**：`./builddir/sdcshield -e <test> -t 10 -n 4` → `exit: pass`。
- **V2 双跑稳定**：同命令背靠背连跑两次均 pass（golden/actual 结构上读同一输入、确定性）。
- **V3 波次构建**：`SDC_BUILD_JOBS=20 ./scripts/build.sh` 零新警告。
- **V5 注入实验**（见 Task 6/7）：host A/B + gem5-fi。
- **V6 回归**：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- **V6b 防超时**：每测试 `-t 2000 -n 1` 改动前后墙钟对比（#360 红线）。
- **V7 x86 目检**：改动文件全在 meson aarch64 块 / `#ifdef __aarch64__` 内。

---

### Task 1: 基线与计划提交

**Files:** 无代码改动；git 操作。

- [x] 确认 worktree 分支从 origin/main 最新分出；`git checkout -b feat/issue376-chain-checkpoints`。
- [x] 基线构建 + 抽样：改动前 `./builddir/sdcshield -e sve512_f32_chain_arm -t 10 -n 4` pass 一次，记录 `-t 2000 -n 1` 墙钟基线（W1/W2 全部目标测试）。
- [x] 提交本计划：`git add docs/superpowers/plans/2026-11-01-issue376-chain-checkpoints.md && git commit -s -m "docs(plan): issue #376 链终态-only→分段中间检查点——吸收机理实证与迁移计划"`。

### Task 2: Wave 1 —— f32 链 + fma_arm 三链（最高风险）

**Files:** 映射表 W1 三文件。**Interfaces:** 无（测试内部改动）。

- [x] `sve512_f32_chain_arm.cpp`：按 T-CHK32 全文替换 golden/hw/比对；docblock 补检查点语义段。
- [x] `sve512_f32_chain_svd.cpp`：T-CHK32 + 段表在块循环外分配（每块重算重比）。
- [x] `sve512_fma_arm.cpp`：读全文后把 T-CHK64（f64 链）、T-CHKCAT（special 链，SEG_LEN=16）、T-CHK32（f32 链）应用到三个链工作负荷；gather/scatter 负荷零改动。
- [x] V3 构建（零新警告）→ V1+V2 逐测试 → V6b 墙钟对比。
- [x] Commit（-s）：`fix(tests): W1 f32/fma 链终态-only→分段中间检查点（issue #376）`。

### Task 3: Wave 2 —— f64 链 + fcmla + power_virus

**Files:** 映射表 W2 五文件。

- [x] `sve512_f64_chain_arm.cpp` / `sve512_f64_chain_svd.cpp`：T-CHK64（+SVD 版）。
- [x] `sve512_fcmla_arm.cpp`：T-FCMLA（init golden 段表 + HW spill + 逐 rotation 逐段比对）。
- [x] `power_virus_dit_sve_arm.cpp`：T-PVSVE（SEG_LEN=64 → 32 段）。
- [x] `power_virus_dit.cpp`：T-PVNEON（SEG_LEN=64 → 32 段；burst/stall duty cycle 不变的注释论证写进代码）。
- [x] V3 → V1+V2 逐测试 → V6b。
- [x] Commit（-s）：`fix(tests): W2 f64/fcmla/power_virus 链终态-only→分段中间检查点`。

### Task 4: Wave 3 —— 类别/容差/精确链

**Files:** 映射表 W3 五文件。

- [x] `sve512_f64_special_arm.cpp` / `_svd.cpp`：T-CHKCAT（类别比对逐段；SEG_LEN=16）。
- [x] `sve512_stencil_axis_arm.cpp`：读链后定案——shape B 容差链分段（同容差逐段比对），shape A 若确认精确则统一性分段+如实标注。
- [x] `sve512_nt_reload_arm.cpp` / `sve512_fmmla_arm.cpp`：读链确认精确算术论断；成立则统一性分段（fmmla 为 issue 点名文件，顺带做）+ 标注无检出率增益；不成立（发现非精确吸收窗）则按增益语义分段。
- [x] V3 → V1+V2 → V6b。
- [x] Commit（-s）：`fix(tests): W3 special/stencil/nt_reload/fmmla 检查点补全（issue #376）`。

### Task 5: host 注入实验（A/B 检出对比，不进 commit）

**Files:** 临时 patch W1 已改文件（`git stash`/手工还原）。

- [x] **A 步（终态-only 吞掉复现）**：在**未分段**的源上（临时还原或 git stash），给 `f32_chain_hw` 注入：step 64 处对 acc XOR `1u << 1`（lane 0 定向用 sveor_m 或全 lane XOR 后看 lane 0）——由 /tmp 模拟选一个 30 seeds 大多数吸收的 (k,j) 组合 → 构建 → `-e sve512_f32_chain_arm -t 10 -n 4` → **预期 pass**（吸收实证）。记录完整输出。
- [x] **B 步（分段抓到证明）**：在已分段源上给 HW 链同一位置注入同一位错 → 构建 → 同命令 → **预期 FAIL，报 seg 1 (after step 128) 的 golden/actual/xor**。记录完整输出。
- [x] 还原 patch、重建、确认 pass。
- [x] 边界声明：源级注入等效模拟中途翻转（任务卡认可）；真·故障注入见 Task 6。

### Task 6: gem5-fi 双内核与注入验收（issue 硬性要求）

**Files:**
- Create: `/home/sdc/wangxu/gem5-fi/workloads/sdcshield/chain_ckpt_dual.c`（仓库外，gem5-fi 树）
- Modify: `/home/sdc/wangxu/gem5-fi/workloads/sdcshield/Makefile`（加 target）
- Create: `docs/superpowers/inventory/issue376-chain-checkpoints/`（台账：命令、日志、verdicts）

- [x] 双内核：镜像 sve512_f32_chain 语义（fixed 确定性操作数、标量 fmaf golden、SVE svmla 链、512 步×8 重复给注入器窗口），argv `mode final|ckpt`；输出契约 `SDCSHIELD_FAILS=<n>` + 16-hex FNV checksum（终态）。ckpt 模式 SEG_LEN=64。
- [x] 基线：两 mode 各跑 `--sve_vl 2`（256-bit，与主机同档）+ `--sve_vl 4`（512-bit）→ `SDCSHIELD_FAILS=0` + 两 mode checksum 一致。
- [x] 注入：`--chaos_fpu --fault_type bit_flip --fault_mask <低位 mask> --first_clock <校准进链窗口> --max_faults 1 --probability 1.0`（或 CHAOSPhysReg vector 定向）；`fault_injections.log`/`stats.txt` 确认 numFaultsInjected=1。
- [x] 验收矩阵（至少各 1 组，引用真实输出）：mode=final 注入跑 → `SDCSHIELD_FAILS=0` 且 checksum==基线（吸收：终态字节不变，故障完全不可见）；mode=ckpt 同参数注入跑 → `SDCSHIELD_FAILS>0`（分段检出）。
- [x] 若首次 first_clock/mask 组合未落进链窗口或未被吸收：如实记录校准过程（尝试过的参数、fault_injections.log 证据），不做选择性隐瞒；若某参数下 final 也检出，如实分类（该位属于传播类，非吸收类）。
- [x] 台账 + PR 引用（注入位、first_clock、检出输出、SDCSHIELD_FAILS）。

### Task 7: 回归、文档同步、收尾

- [x] V6 回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`。
- [x] V3 最终全量构建零新警告；V7 x86 目检记录。
- [x] 文档同步（rule 7）：`docs/coding/writing_tests.md` 增补"长链必须分段中间检查点（issue #376 范式）"条目（吸收机理一段 + SEG_LEN=64 缺省 + 指向本计划）；README 无此层表述不动；CLAUDE.md 已有 FMMLA 精确操作数条目不动。
- [x] Commit（-s）：`docs(tests): 链分段检查点范式写入 writing_tests.md（issue #376）`。
- [x] push feature 分支；开 PR：正文含机理+粒度数据+注入实验输出+gem5 验收矩阵+剩余清单（若有）+`Refs #376`（**不写 Fixes，未做完不关 issue**）。
- [x] code-review（/code-review high）→ 处置全部发现 → merge。
- [x] issue 留移交评论（完成范围、证据链接、剩余清单）。

## Self-Review

- 覆盖检查：issue 点名的三个文件（f32_chain_arm 248-253 / fmmla_arm 322-331 / power_virus_dit 155-186）分别在 W1/W3/W2；~18 测试 = 13 改 + 4 已有中间检查（如实列出不改理由）；修法方向 1（周期检查点）= 本计划主体、方向 2（粒度权衡）= 实证表、方向 3（整数操作数化）= 明确留后续棒；验收方向（gem5 注入演示 + -t 2000 不超时）= Task 6 + V6b。
- 占位符扫描：无 TBD/TODO；模板给全码；W3 两个"读后定"文件给出了决策规则与两条分支的具体做法（不是占位——是显式决策点）。
- 类型一致性：模板统一 `SEG_LEN/SEG_COUNT/seg_out[idx]` 命名；`gold_seg/hw_seg` 尺寸 `SEG_COUNT * lanes`；fcmla 的 init 段表与 run 比对索引同构。

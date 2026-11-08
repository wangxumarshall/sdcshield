# Issue #373 第二棒：sve_rot / VLA 变体 / arm0102_sve / 散件 同码 golden → 标量独立 golden

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把第一棒（PR #412，neon_rot 39 文件）之外的 issue #373 剩余同码 golden 族全部迁移到"标量独立 golden"范式，使 SVE/NEON 数据通路的持久单元故障从"零检出"变为"必检出"；扩展 `scripts/check-scalar-golden.sh` 机械门覆盖 SVE 指令形态；对 scalar64 标量-对-标量缺口给出处置裁决。

**Architecture:** 只换 golden 的来源——init 的 golden 循环从 SVE/NEON intrinsic 换成纯 C 标量运算（经 `reinterpret_cast` 或直接的 `uint64_t*`/`uint32_t*` 逐元素读写同一缓冲），run 侧压力骨架（TEST_LOOP、迭代结构、asm ldr/str、pg4 子块、指令混合、缓冲尺寸）、冷路径 dump/classifier、失败闸门（memcmp/memcmp_or_fail）全部零改动。expected 缓冲类型与字节布局不变。

**Spec:** GitHub issue #373 + 第一棒移交评论（2026-11-01）+ 范式模板 `docs/superpowers/plans/2026-11-01-issue373-scalar-golden-neon-rot.md`。本计划是该范式的第二棒（剩余清单）。

## 本机验证基线（2026-11-08 实测，`--dump-cpu-info`）

- CPU: cortex x3b（ARMv9），**SVE 有，VL=256 bit（32 B）**；SVE2/SME/SME2 无。
- CLAUDE.md 平台怪癖节"本机 no-SVE 920"为旧机记录，该节自声明"binds no machine"；以本机实测为准（SVE 测试全部真机可跑，SVE2/SME 相关 SKIP 属预期——本棒目标族无 SVE2/SME 依赖）。
- 标量 golden 本身不含任何 SVE 指令、与 VL 无关（逐元素纯 C），128/512/1024 档正确性由构造保证（run 骨架零改动、tier 行为既有事实）；本机 256 档真机验证。

## 范围核实（对 2026-11-08 main = aeb2c1ea 的实测勘误）

第一棒移交清单基于 2026-11-01 树况；#472 迁移（2026-11-03）后有两处过时：

1. `tests/cpu/arm-0102/arm0102_kreg_mask.cpp`（非 sve）**已随整个 arm-0102/ 目录删除**（ff74f226 "delete old"），现行等价物即 `sve/kreg/` 5 个 `_sve` 文件——移交清单该行与 arm0102_sve 行合并为同一目标。
2. sve_rot 实为 **5 文件 8 测试**（移交评论说"2 文件 5 测试"，少计）：`sve_rot_ldr_at_top.cpp`、`_rowmajor.cpp`、`_rowmajor_k3res.cpp`、`_rowmajor_k5inter_rand.cpp` 各 1 测试 + `_rowmajor_k7pattern.cpp` 4 测试（k7p_zero/one/rand/hiham 共用 `sve_k7p_init_common`）。
3. **新发现 PR #412 漏网**（全树 marker 扫描）：`tests/cpu/misc/neon_rot_ldr_rand.cpp`、`neon_rot_ldr_rand_thread.cpp`——同为 `vaddq/veorq/vandq/vorrq_u64` 同码 golden（W1-W4 映射表未列），属 issue 原范围 `misc/neon_rot_*` 族，本棒补迁。
4. `tests/cpu/sve/mem/agu_stress_2src_sve.cpp`：golden 已是标量，仅注释"computed on the same SVE units"陈旧——只改注释。
5. kreg2/3/5/6/8/9、vector2/kreg1/4/7、sve/mem 其余：golden 已独立（实测扫描），不在范围。

### 最终目标清单（28 文件 / 39 测试 + 1 注释 + 1 裁决 + 1 脚本 + 1 文档）

| 波次 | 文件 | 测试数 | 模板 |
|---|---|---|---|
| W1 | sve/sve_rot_ldr_at_top.cpp | 1 | T-SR64（每槽相位 i%4，u64 逐元素，VLA 字节缓冲） |
| W1 | sve/sve_rot_ldr_at_top_rowmajor.cpp | 1 | T-SR64 |
| W1 | sve/sve_rot_ldr_at_top_rowmajor_k3res.cpp | 1 | T-SR64（GPS 标签填充是输入生成，不动） |
| W1 | sve/sve_rot_ldr_at_top_rowmajor_k5inter_rand.cpp | 1 | T-SR64-K5（源对 slot 2i/2i+1） |
| W1 | sve/sve_rot_ldr_at_top_rowmajor_k7pattern.cpp | 4 | T-SR64（k7p_init_common 一处改 4 格；模式填充是输入生成，不动） |
| W2 | sve/case/neon_rot_ldr_at_top_sve.cpp | 1 | T-VA（per-slot 相位 (e/lanes)%4） |
| W2 | sve/case/…_rowmajor_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_sub_sve.cpp | 1 | T-VA-SUB（-/^/&~/'|~） |
| W2 | sve/case/…_mla_sve.cpp | 1 | T-VA-MLA（0x7fff 收窄持久化 + ×/×+a/b−×/^，u32） |
| W2 | sve/case/…_offdiag_sve.cpp | 1 | T-VA-OD（源对 [3i+1]/[3i+2]） |
| W2 | sve/case/…_str3_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_ldr4_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_rowmajor_dump_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_rowmajor_gps_sve.cpp | 1 | T-VA（GPS 标签填充是输入生成，不动） |
| W2 | sve/case/…_rowmajor_dualcmp_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_rowmajor_str3_checkall_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_rowmajor_k5far_sve.cpp | 1 | T-VA-K5（源对 [2i]/[2i+1]） |
| W2 | sve/case/…_rowmajor_k5inter_rand_sve.cpp | 1 | T-VA-K5 |
| W2 | sve/case/…_rowmajor_k3res_sve.cpp | 1 | T-VA |
| W2 | sve/case/…_rowmajor_k7pattern_sve.cpp | 4 | T-VA（k7p_sve_init_common 一处改 4 格） |
| W3 | sve/kreg/arm0102_kreg_logic_sve.cpp | 1 | T-1102（相位 (i/4)%4；&/'|/^/&~） |
| W3 | sve/kreg/arm0102_kreg_mask_sve.cpp | 1 | T-1102-CMP（==、(int32)>、==、(a&b)!=0 → 0/0xFFFFFFFF） |
| W3 | sve/kreg/arm0102_kreg_not_sve.cpp | 1 | T-1102（~(a&b)/~(a'|b)/~(a^b)/~(a&~b)） |
| W3 | sve/kreg/arm0102_kreg_select_sve.cpp | 1 | T-1102-BSL（bsl(a,b,c)=(a&b)'|(~a&c) 四变体标量式） |
| W3 | sve/kreg/arm0102_fma_f32_sve.cpp | 1 | T-1102-FMA（fmaf 精确复制式，见下） |
| W4 | misc/core179_kernel_sdc_repro.cpp | 1（双骨架） | T-A + T-A32-CMP（NEON u64x2 / u32x4） |
| W4 | arm64/neon_test.cpp | 1 | T-ADD（无相位→no-tree-vectorize 门） |
| W4 | sve/fma2/neon_add_sve.cpp | 1 | T-ADD |
| W4 | misc/neon_rot_ldr_rand.cpp | 1 | T-A（u64x2，相位 (e/2)%4） |
| W4 | misc/neon_rot_ldr_rand_thread.cpp | 1 | T-A |
| W4 | sve/mem/agu_stress_2src_sve.cpp | — | 注释修正（golden 已标量） |
| W5 | scalar64（misc/neon_rot_ldr_at_top_rowmajor_scalar64.cpp） | — | **裁决：声明不改**（依据见下） |
| W5 | scripts/check-scalar-golden.sh | — | VALU_RE 扩 SVE z/p 形态 |
| W5 | docs/coding/writing_tests.md | — | migrated-family 列表补本棒族 |

## 关键技术决策

### D1: SVE VLA 的标量 golden 与相位语义

- sve_rot 族（W1）：缓冲是 `uint8_t*`，几何 = vl_slots × vl_bytes；标量 golden 以 `uint64_t*` reinterpret 逐元素填 expected，**相位 = 槽序号 i%4**（与 run 的 rot_alu(i,…) 一致），每槽 lanes = vl_bytes/8 个元素共享相位。
- case 族（W2）：缓冲已是 `uint64_t*`，elems = slots×lanes；标量 golden 相位 = `(e / lanes) % 4`（e 为元素下标）。
- 相位必须逐文件从 run 侧核对（2src_sve 先例是 per-element 相位、at_top 族是 per-slot 相位——不同骨架不同语义）。

### D2: arm0102 族相位

run/init 的 pg4 子块结构里相位 = (base+sub)/SUBLANES（4 元素组全局序号）；base 是 lanes 的倍数、lanes 是 4 的倍数（VL≥128B? 实为 VL≥16B 且为 16 的倍数 → lanes=svl/4 是 4 的倍数）→ 元素 i 的相位 = **(i/4)%4**，与 base/sub 分解无关。标量 golden 用平铺循环。

### D3: fma_f32 的位精确标量式（关键）

`svmla_f32_x(pg, c, a, b) = c + a*b`（单次舍入）。标量等价（全部位精确、含 ±0 符号位）：

| 相位 | SVE 序列 | 标量精确复制式 |
|---|---|---|
| 0 fma | svmla(c,a,b) | `fmaf(a, b, c)` |
| 1 fms | svmsb(c,a,b) = c−a*b | `fmaf(-a, b, c)` |
| 2 fnma | svneg(svmsb(svneg(c),a,b)) | `-fmaf(-a, b, -c)` |
| 3 fnms | svneg(svmla(svneg(c),a,b)) | `-fmaf(a, b, -c)` |

证明要点：`fmaf(−a,b,x)` ≡ `x − a*b` 逐位（积取负精确；加法与减法同为单次舍入的精确和）；取负精确；round-to-nearest 对称（含零符号）。**不得**把 2/3 化简成与 0/1 相同的直接式——c=−0 或 a·b=±0 的零符号位分歧（例：a=+0,b>0,c=−0 时 fma=+0 而嵌套 neg 序列=−0）。输入域 `frandomf_scale(100.0f) ∈ [0,100)` 无 NaN/−0，但公式按全输入域位精确构造，不依赖分布。fmaf 编译为标量 `fmadd s`（标量 FP 管线，与 SVE `fmla z` 不同指令不同通路——stuck SVE lane 只污染向量侧）。无 -ffast-math（buildtype=release 仅 -O3；fmaf 语义强制融合）。

### D4: 无相位纯加法 golden 的防向量化（neon_add / neon_add_sve）

`g[i]=a[i]+b[i]` 是 -O3 自动向量化的标准 idiom——golden 会被静默编回 NEON/SVE（同码回归）。处置：golden 循环放独立 `static` 函数 + `#pragma GCC optimize("no-tree-vectorize")`（第一棒计划 rule 5 预案的启用），并以 check-scalar-golden.sh 验证。

### D5: scalar64 裁决——声明不改

`neon_rot_ldr_at_top_rowmajor_scalar64` 的被测通路是**标量 ALU 本身**，golden 亦标量（标量-对-标量同码）。范式修法（独立通路 golden）在此方向反转：需要 NEON 计算 golden。不改的依据：
1. 反向修法需把 golden 重构为相位跨距配对（同相位元素相距 4，NEON 向量 golden 须 vcombine(i,i+4) 配对）——全新机械，无树内先例，且 check-scalar-golden.sh 门断言的是相反性质（golden 内零向量 ALU），无法复用；
2. 反向修法使 NEON 持久故障 FAIL 一个名字声明 scalar chassis 的测试——在 core-179 归因战役的仪器上制造跨归因噪声（尽管套件级差分可消歧）；
3. 该测试的既有角色是速率对照底盘（瞬态 SDC 速率测量），持久故障检出升级与该测量正交；
4. 缺口真实存在，如实声明：持久标量 ALU 故障对本测试零检出。留后续棒/所有者决策。

（writing_tests.md 已有边界声明："except rowmajor_scalar64 — already fully scalar, a separate scalar-vs-scalar same-code gap"，本棒维持并补全论证。）

### D6: check-scalar-golden.sh SVE 扩展

现行 VALU_RE 只匹配 NEON `vN.` 操作数——SVE `add z0.d,…`、`fmla z0.s, p0/m,…`、`cmpeq p0.s,…` 均漏检。扩展：助记符集补 SVE ALU（fmla/fmsb/fnmadd 系、neg/not/sel、cmp 系），操作数形态补 `z[0-9]+\.` 与 `p[0-9]+\.`（p0/m 形态由 `p[0-9]+[./]` 覆盖首操作数）。允许的例外保持：数据搬运（ldr/str、ld1/st1）、标量 FP `fmadd s`（D3 的独立通路）、rdvl/mrs。负向对照：转换前的 SVE init 符号必须被抓出（否则扩展无效）；正向：转换后全 PASS + NEON 族已覆盖符号不回归。

## Global Constraints

- CLAUDE.md 全部规则生效；字节一致 memcmp 闸门不动。
- x86-64 untouched：全部目标文件都在 aarch64-only meson 块（arm64/、sve/、misc/ 的 aarch64 guard）——x86 构建不编译它们；diff 只减少 init 的 arch 相关代码。
- 防同源回归：标量 golden 经 switch 相位分派（天然屏障）或 D4 pragma；每波构建后 `scripts/check-scalar-golden.sh` 过该波全部 init 符号（扩展版）。
- 构建零新警告：`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release`（已存在则 --reconfigure）→ `SDC_BUILD_JOBS=20 ./scripts/build.sh`（并发 agent 禁满核）。
- 回归：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- DCO：`git commit -s`，尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`（CLAUDE.md 规则 3 口径，与树内全部已合并提交一致），无 Co-Authored-By；只 push fix/issue-373，绝不 push main。
- 提交粒度：波次=逻辑单元（W1 sve_rot 族、W2 VLA 族、W3 arm0102 族、W4 散件、W5 工具+文档）；PR 论证同族批量=一个单元。

## 验证协议（每个转换测试必须过）

- **V1 单测**：`./builddir/sdcshield -e <test> -t 10 -n 4` → `exit: pass`（本机 SVE-256 真机）。
- **V2 双跑稳定**：同命令连跑两次均 pass。
- **V3 波次构建**：`SDC_BUILD_JOBS=20 ./scripts/build.sh` 零新警告。
- **V4 机械门**：`scripts/check-scalar-golden.sh builddir/sdcshield <该波全部 init 符号>`（k7pattern 查 4 个 wrapper 符号）→ 全 PASS。
- **V5 注入实证**（一次性，W1 后）：毒化 `rot_alu` case 0（`sveor(svadd(a,b), dup(1<<41))`，等效 SVE 加法单元持久单错位）：旧码（origin/main）→ `exit: pass`（零检出）；新码 → `exit: fail`（sve_rot_ldr_at_top data miscompare）。临时补丁不入库。边界诚实声明：源级注入等效模拟（任务卡认可）；gem5-fi CHAOS 真·单 lane 注入保持开放（与第一棒相同的验收边界）。
- **V6 回归**：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
- **V7 x86 目检**：全部改动文件 aarch64-only 注册；diff 只含 init golden（减少 arch 代码）。

## 任务清单

### Task 1: 分支与计划 ✅
- [x] `git checkout -b fix/issue-373`（自 aeb2c1ea）。
- [x] 本计划文档 commit（`docs(plan): issue #373 第二棒…`）。

### Task 2: W1 sve_rot 族（5 文件 8 测试）
- [ ] 逐文件 Read init 窗口 → T-SR64 替换 golden 循环（k7pattern 改 `sve_k7p_init_common`）。
- [ ] V3 / V1×8 / V4（init 符号 ×5 + k7p wrapper ×4）。
- [ ] Commit：`fix(tests): sve_rot 族同码 golden→标量独立 golden (issue #373 W1)`。

### Task 3: W2 VLA case 族（15 文件 18 测试）
- [ ] 同流程（模板 T-VA 族；mla 收窄持久化保留；offdiag/k5 源对下标换算）。
- [ ] V3 / V1×18 / V4。
- [ ] Commit：`fix(tests): neon_rot SVE VLA 变体族同码 golden→标量独立 golden (issue #373 W2)`。

### Task 4: W3 arm0102 族（5 文件 5 测试）
- [ ] kreg 4 文件平铺标量；fma_f32 用 D3 表。
- [ ] V3 / V1×5 / V4（fma init 符号允许标量 fmadd，禁 z 形态）。
- [ ] Commit：`fix(tests): arm0102 SVE 族同码 golden→标量独立 golden——fma 用 fmaf 位精确式 (issue #373 W3)`。

### Task 5: W4 散件（5 文件 + 1 注释）
- [ ] core179 双骨架、neon_add×2（D4 pragma）、ldr_rand×2、agu 注释。
- [ ] V3 / V1×5 / V4。
- [ ] Commit：`fix(tests): core179/neon_add/ldr_rand 散件同码 golden→标量独立 golden (issue #373 W4)`。

### Task 6: W5 工具与文档
- [ ] check-scalar-golden.sh 扩 SVE（D6）+ 负向/正向对照实录。
- [ ] writing_tests.md 族清单更新 + scalar64 裁决补注。
- [ ] Commit：`fix(scripts): check-scalar-golden 门扩 SVE 指令形态 + 文档族清单 (issue #373 W5)`。

### Task 7: 注入实证（W6）
- [ ] A/B 对比实录（V5）；还原后重跑 pass。

### Task 8: 收尾（W7）
- [ ] V6 zstd19 回归；V3 最终全量构建零新警告；V7 x86 目检记录。
- [ ] push fix/issue-373；开 PR（正文含验证实录，`Closes #373`）；issue #373 处置评论。

## Self-Review

- 覆盖：第一棒移交清单全项有着落（含两处过时勘误：arm-0102 非 sve 已删、sve_rot 计数修正）+ 2 个新发现漏网 + scalar64 裁决。
- 位精确：D3 的 FMA 表是本棒唯一非显然位精确点，已给全输入域证明（含零符号位）；其余为模 2^N 整数运算。
- 门禁：V4 对 SVE 形态的盲区以 D6 扩展补上，负向对照防止"扩了但不抓"。

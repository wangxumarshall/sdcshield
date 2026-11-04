# issue #445: acl_gemm_sve 迭代内截止检查（行块化 body）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 issue #445 定性的产品级缺陷——`acl_gemm_sve` 默认参数（600‰ 内存自适应 dim + 300 s 超时耐心地板）在大内存 SVE 主机上必然 `exit invalid(2)`（非 SKIP 非 pass）——采用 enumeration.md §5.3 杠杆 (c) 之「迭代内截止检查」：把单体 O(dim³) body 切成 16 行行块，使 -t 截止线在任意 dim ≤ 4096、任意主机上都能在耐心地板内兑现。

**Architecture:** 只改 `tests/cpu/sve/arithmetic/acl_gemm_sve.cpp` 的 `test_run`：body 从"整个矩阵一趟 GEMM + 全量 memcmp"改为"16 行行块 GEMM + 块内 memcmp"，行块游标跨 body 环回推进；`TEST_LOOP` 粒度 64 → 4（#360 家族标准）。几何模型（600‰, clamp [64,4096]）、knob 链、golden 计算、svmla 行内数学**一字不动**——位级输出不变，内存压力覆盖不变。行独立（C 的第 i 行只依赖 A 的第 i 行与整个 B）保证块内比较与整趟比较语义等价。

**Tech Stack:** C++20 / SVE1 intrinsics（arm_sve.h）/ sdcshield TEST_LOOP 框架宏 / gem5 CHAOS SE（AtomicSimpleCPU，se_sve.py，`--vl {1,2,4,8}`）。

**Spec:** issue #445（审计：Task 2 把该缺陷误定性为"环境类"，短路了修复循环）；机制链与修复杠杆枚举：`docs/superpowers/inventory/sve-all-vl-campaign/enumeration.md:164-194`（§5.3 + (c)）；campaign 实测证据：`docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md:77-119`（§4 表 #8-#25 及 acl_gemm_sve 的 dim=512 有界化）。

## Global Constraints

- CLAUDE.md 规则 2：本机（Kunpeng 920 0xd01，**无 SVE**）只能验证 SKIP 路径与构建——**不足以**作为功能性改动的验证；功能验证必须在 gem5-fi（CHAOS/SE）于受影响 VL 档（128/256/512/1024-bit 门内取档）跑真二进制并引用真实输出（tier、exit、Simulated exit code）。
- CLAUDE.md 规则 1（一补丁一单元）、规则 3（`git commit -s`，DCO 最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容；绝不推 main）。
- CLAUDE.md 规则 5（x86-64 不动）：`tests/cpu/sve/meson.build:22` 整目录仅在 `host_machine.cpu_family() == 'aarch64'` 下进入构建——零 x86 影响 by construction。
- CLAUDE.md 规则 6（占位诚实）与本计划无关（无占位）。
- 未复现不修的处置：缺陷在 SVE-256 campaign 主机上有仓库内真实验证记录（triage-256.md §4 表、enumeration.md §5.3 两次独立复现 + 全核复现）；本 CI 主机无 SVE 无法原生复现，机制链逐环 file:line 已核（见 Task 1 注释引用）。
- gem5 本机身份：`/home/sdc/wangxu/operator/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt`，md5 `41ffcc66a4facc74b1c54cf1f7edbe3a`（**与** campaign 录制主机的 `3f9011f40dbe80ec1df478c62ddb0b06` **不同**——不同主机的不同构建，证据中如实记录）。

## 范围裁定（审计要求的同类全量 disposition）

25 个 campaign 超时格在**当前 main** 的状态（file:line 逐一核实）：

| 格 | 状态 | 证据 |
|---|---|---|
| #1-2 agu_stress_2src_sve / store_only_sve | 已修（粒度 1<<13→4） | `ae6399c7`（issue #349），两文件 TEST_LOOP(test, 4) |
| #3-7 arm0102_fma_f32 + kreg_{logic,mask,not,select} | 已修 | `97c88275`（issue #360），TEST_LOOP(test, 4) |
| #8 eigen_svd_bidiag_sve | 已修（联合求解） | `eigen_svd_bidiag_sve.cpp:459` run_budget_ms=200000 |
| #9 eigen_svd_cdouble_sve | 已修（联合求解） | `svd_cdouble_sve.cpp:273` run_budget_ms=140000 |
| #10 gmp_bigadd_sve | 已修 | `97c88275`，TEST_LOOP(test, 4) + 机制注释 |
| #11-14 ipsec wide/96 变体 ×4 | 已修 | `97c88275` 覆盖 7 个 ipsec 文件 |
| #15 mem_disambig_alu_sve | 已修（尺寸放大 + THP） | issue #366 CLOSED；`mem_disambig_alu_sve.cpp:76-83` 预算÷threads |
| #16 mesh_upi_sve_sym | 单独跟踪 | issue #357 OPEN（文件内 :46-60 边界声明） |
| #17 movmskpspd_sve | 已修 | `97c88275` |
| #18-25 sve_rot_ldr_at_top 族 ×8 | 已修 | `97c88275` 覆盖 7 个 sve_rot 文件（k7pattern 含 4 ID） |
| （表外）acl_gemm_sve | **未修——本计划目标** | enumeration.md §5.3 点名；TEST_LOOP(64) + 单体 O(dim³) body + 纯 600‰ 内存模型，全仓唯一残留 |

同类残余边界（如实声明、不隐改）：openblas_gemm 族（lu/dgemm/sgemm/cgemm/zgemm）为 850‰ + TEST_LOOP(1) + BLAS 核 body（compute-bound，campaign 级主机秒级）；其 body 在**超大内存**主机（≥TB 级）上理论上可复现同型相交，超出本 issue 证据基线（61-62 GiB campaign 主机 / 30 GiB 本机），不在本补丁动。

## 截止线裕度论证（修复有效性算术）

- campaign 实测（61 GiB SVE-256 主机）：dim=3113 全核单趟 > 300 s（被杀时趟未完成，utime 224-230 s @ -n 1 dim=4096）→ 单行成本 row(3113, 全核) > 300/3113 ≈ 0.096 s，row(4096, 单线程) ≈ 224/4096 ≈ 0.055 s。
- 修复后检查点间隔 = 4 块 × 16 行 = 64 行 ≤ 64 × row。dim=4096 最坏档：64 × 0.055 × AR。要击穿 300 s 耐心地板需 AR > 85×（全核争抢慢化实测口径 3-25×，见 `sdc_stress_config.h:660-674` 注释）。**裕度 ≥ 3-25×，结构上不可能再撞地板。**
- 修复前（N=64、单体 body）：检查点间隔 = 64 × 整趟 > 64 × 300 s——任何主机都到不了第一个检查点。

---

### Task 1: 行块化 `acl_gemm_sve_run` + 粒度 64→4

**Files:**
- Modify: `tests/cpu/sve/arithmetic/acl_gemm_sve.cpp:104-166`（仅 run 函数与 body 注释）

**Interfaces:**
- Consumes: `TEST_LOOP(test, N)`（`framework/sandstone.h:144-148`，N=每 N 个 body 查一次 -t 截止线）；`sdc_auto_square_dim`（init 几何，不动）。
- Produces: 行块 body 的 `acl_gemm_sve_run`；无新导出符号；DECLARE_TEST 结构不变。

- [x] **Step 1: 修改 run 函数（整函数替换，行内数学逐字保留）**

```cpp
#if defined(__aarch64__)
static int acl_gemm_sve_run(struct test *test, int cpu)
{
    (void)cpu;
    auto *d = static_cast<acl_gemm_sve_data *>(test->data);
    const int N = d->n;
    std::vector<float> C((size_t)N * N);
    const int lanes = svcntw();
    /* B 行转置缓冲: n 可大于 64, 栈上定长放不下 -> 堆上一次性分配,
     * run 只调一次, 循环外统一释放。 */
    float *brow = (float *)aligned_alloc(64, (size_t)N * sizeof(float));
    if (!brow) {
        report_fail_msg("acl_gemm_sve: brow alloc failed");
        return EXIT_FAILURE;
    }

    /* 迭代内截止检查 (issue #445; enumeration.md §5.3 杠杆 (c)):
     * 自适应 dim 是纯内存模型 (600‰ RAM, 见 init), 而一次完整 GEMM 是
     * O(dim³) —— 大内存 SVE 主机上 dim=3113 时单体 body > 300 s
     * (campaign 实测 SIGQUIT timed out / exit invalid(2),
     * triage-256 §4), 而 TEST_LOOP 只在 body 之间查 -t 截止线
     * (framework/sandstone.h), 单体 body 永远到不了检查点, 撞
     * test_timeout() 的 300 s 耐心地板 (framework/sandstone.cpp)。
     * 解法: body 从"整个矩阵"缩为 16 行的行块, 行块游标跨 body 环回
     * 推进; 每块算完立即与 golden 做块内 memcmp —— C 的第 i 行只依赖
     * A 的第 i 行与整个 B (行独立), 块内比较与整趟比较逐字节同义,
     * svmla 行内数学逐字未动, 位级输出不变。N=4 (#360 家族标准粒度)
     * 使截止线间隔 = 4 块 = 64 行 = 整趟的 64/N: dim≤4096 时每块
     * 秒级(全核), 检查点恒在耐心地板内, -t 在任意主机/任意 dim 都能
     * 兑现。单趟总指令流不变(只是按 16 行切片), 600‰ 内存几何不动。 */
    constexpr int kRowBlock = 16;
    int next_row = 0;
    TEST_LOOP(test, 4) {
        const int ibase = next_row;
        int iend = ibase + kRowBlock;
        if (iend > N)
            iend = N;
        for (int i = ibase; i < iend; ++i) {
            for (size_t jbase = 0; jbase < (size_t)N; jbase += lanes) {
                int n = (N - (int)jbase < lanes) ? N - (int)jbase : lanes;
                svbool_t pg = svwhilelt_b32((uint64_t)0, (uint64_t)n);
                svfloat32_t acc = svdup_f32(0.0f);
                for (int k = 0; k < N; ++k) {
                    /* B 行 k 的 jbase..jbase+n 列 → 向量 (谓词控) */
                    for (int c = 0; c < n; ++c) brow[c] = d->B[(size_t)k * N + jbase + c];
                    svfloat32_t vb = svld1_f32(pg, brow);
                    /* svmla = FMLA fused 乘加(单次舍入), 与 golden 的
                     * fmaf 舍入序一致, 有限值输入下位级一致 */
                    acc = svmla_f32_x(pg, acc,
                              svdup_f32(d->A[(size_t)i * N + k]), vb);
                }
                svst1_f32(pg, C.data() + (size_t)i * N + jbase, acc);
            }
        }
        next_row = (iend >= N) ? 0 : iend;
        const size_t block_elems = (size_t)(iend - ibase) * (size_t)N;
        const float *g = d->golden.data() + (size_t)ibase * N;
        float *cbase = C.data() + (size_t)ibase * N;
        if (memcmp(cbase, g, block_elems * sizeof(float)) != 0) {
            /* cold path: locate mismatches, record bit-level evidence (bit pattern + float) */
            {
                size_t _dump_bad = 0;
                int _dump_shown = 0;
                for (size_t _dump_j = 0; _dump_j < block_elems; ++_dump_j) {
                    uint32_t _dump_g, _dump_a;
                    memcpy(&_dump_g, &g[_dump_j], 4);
                    memcpy(&_dump_a, &cbase[_dump_j], 4);
                    if (_dump_g != _dump_a) {
                        ++_dump_bad;
                        if (_dump_shown < 8) {
                            log_error("acl_gemm_sve: mismatch at index %zu: golden=0x%08x (%g) actual=0x%08x (%g) xor=0x%08x",
                                      (size_t)ibase * N + _dump_j, _dump_g,
                                      g[_dump_j], _dump_a, cbase[_dump_j],
                                      _dump_g ^ _dump_a);
                            ++_dump_shown;
                        }
                    }
                }
                log_error("acl_gemm_sve: total mismatched elements: %zu / %d",
                          _dump_bad, (int)block_elems);
            }
            free(brow); /* heap brow: 释放后再 report_fail_msg (noreturn) */
            report_fail_msg("acl_gemm_sve: GEMM output miscompare (SVE svmla path)");
        }
    }
    free(brow);
    return EXIT_SUCCESS;
}
#else
```

（`#else` 分支、`test_cleanup`、`DECLARE_TEST` 逐字不动。）

- [x] **Step 2: 构建零新警告**

Run: `./scripts/build.sh 2>&1 | grep -iE "warning|error" ; echo "grep-rc=$?"`
Expected: 无 acl_gemm_sve 相关 warning/error（grep rc=1 为无匹配；若有任何新告警即失败，修完再继续）。

- [x] **Step 3: 本机 SKIP 门路径不变（诚实边界：非功能验证）**

Run: `./builddir/sdcshield -e acl_gemm_sve -t 2000 -n 1 2>&1 | grep -E "result:|skip-reason|exit:"`
Expected: `result: skip` / `skip-reason: 'test compiled with sve'` / `exit: pass`（框架编译期特性门，与修复前一致——本机无 SVE，功能验证由 Task 2 的 gem5-fi 承担）。

- [x] **Step 4: 框架回归**

Run: `./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -3`
Expected: `exit: pass`。

- [x] **Step 5: x86-64 非回归（by construction 检查）**

Run: `sed -n '22p' tests/cpu/sve/meson.build`
Expected: `if host_machine.cpu_family() == 'aarch64'`（整目录仅 aarch64 进入构建；本补丁只动该目录下单个文件——零 x86 影响）。

### Task 2: gem5-fi CHAOS/SE 档位验证（rule 2 功能验证主体）

**Files:**
- 无仓库文件改动（证据收集；产物落 `/tmp/issue445-gem5/`，不入库）。

**Interfaces:**
- Consumes: 本机 `gem5.opt`（md5 41ffcc66a4facc74b1c54cf1f7edbe3a，与 campaign 录制主机的 3f9011f4 不同构建——如实记录）；`configs/se/arm_chaos.py`（`--cmd/--sve_vl/--workload_args/--cpu O3`）；`acl_gemm_sve_dual.c`（PR #242 `sme_za_tileview_dual` 同款 same-source dual kernel：镜像补丁后的 body 逐字 svmla 行数学 + 16 行行块游标 + 块内 memcmp + fmaf golden，modes fixed/random/poison×负控）。
- Produces: 每 VL 档 × 每 mode × 每 dim 的真实运行记录（`SDCSHIELD_FAILS=0`、16-hex 校验和、`Simulated exit code`），进 Task 3 的 PR 正文与 issue 报告。

**执行边界（如实记录）**：
1. **真二进制路线在本机不可行**：campaign 的 512 腿用"Task 5 终版 9-stub 链"gem5（录制主机 md5 3f9011f4）跑真 sdcshield；本机 gem5-opt（41ffcc66）与检出均**无该 stub 链**（se_workload.cc 仅 vendor 导入 3e739f0f），真二进制死于 `fatal: syscall prctl (#167) unimplemented`（实测 4 档全灭——框架 2026-10-27 VL pin 用 prctl）。重建 stub 链超出本任务预算且本机 gem5-fi 检出处于他人 WIP 分支（feat/pr294-r5-dual，有未提交改动）。
2. **载体改用 CLAUDE.md rule 2 点名的 same-source dual kernel**（"same-source dual kernel under workloads/sdcshield/"；PR #242 先例即测试代码修复用 dual kernel 四档验证）。为遵守任务红线（cwd 外只读、gem5-fi 检出有他人未提交 WIP），kernel 落 `/tmp/issue445-gem5/acl_gemm_sve_dual.c` 零接触 gem5-fi 仓库，源码随 PR/issue 报告附上；其 Makefile 全局旗标（clang -static -O2 -march=armv9-a+sve+sme -Wall -Wextra）逐字沿用。
3. gem5 验证的是**改动后的 body 代码路径**（行块切分/游标环回/尾块 clamp/块内 memcmp/位级一致）；自适应 dim 的截止线裕度由"截止线裕度论证"节的算术承担（gem5 模拟不了 dim>256 的趟，campaign matrix.md 注记：dim=512/256 在 gem5 中 WALL 超时）。

- [x] **Step 1: 编译 dual kernel（零警告）**

```bash
cd /tmp/issue445-gem5 && clang -static -O2 -march=armv9-a+sve+sme -Wall -Wextra \
  -o acl_gemm_sve_dual acl_gemm_sve_dual.c -lm
```

- [x] **Step 2: 四档矩阵（--cpu O3，家族约定；并发 8）**

正例 16 = {fixed,random} × dim{128,100} × --sve_vl{1,2,4,8}；负控 4 = poison × dim128 × 四档。
dim=128 = 8 满块/趟；dim=100 = 6 满块 + 4 行尾块（exercises `iend = min(ibase+16, N)` 的尾块 clamp）。
Run: `/tmp/issue445-gem5/run_matrix.sh`（产物 `matrix.tsv`：`<tag> rc=<n> fails=<n> cs=<16hex> Simulated exit code: <n>`）

- [x] **Step 3: 断言（三要素 + 跨档一致 + 负控检出）**

Expected：
- 16 正例全 `rc=0`、`fails=0`、`Simulated exit code: 0`；
- 同 mode/seed/dim 下**四档校验和逐字相同**（svmla 的 k 累加链按 k 顺序、lane 只划分 j 空间——C 与 VL 无关且恒等于 fmaf golden，这正是被测位级契约）；
- 4 个 poison 负控全 `rc=1`、`fails>0`（证明块内 memcmp 接对了行）。
任一断言不成立 ⇒ 停下回 Phase 1 重分析，不得带病提交。

### Task 3: 提交、推送、开 PR、贴报告

**Files:**
- 无代码文件（git 操作 + GitHub 报告）。

- [ ] **Step 1: 确认分支纪律**

Run: `git status`（须 clean，除计划文档勾选更新）；`git branch --show-current` → `fix/issue-445`（从 `origin/main` 起：`git checkout -B fix/issue-445 origin/main` 已在会话首完成；全程只在此分支提交）。

- [ ] **Step 2: 提交（一个单元一个补丁，DCO 最后一行）**

```bash
git add tests/cpu/sve/arithmetic/acl_gemm_sve.cpp docs/superpowers/plans/2026-11-02-issue445-acl-gemm-sve-deadline.md
git commit -s -m "fix(sve): acl_gemm_sve 行块化 body 兑现 -t 截止线——大内存主机默认参数不再必然 exit invalid (#445)"
```

提交信息正文须含：根因（600‰ 内存模型 × O(dim³) 单体 body × 300 s 耐心地板 × TEST_LOOP 只在 body 间查截止线）、改动点（行块 + N=4）、验证证据（构建零新警告 / 本机 SKIP 门不变 / zstd19 回归 pass / gem5-fi 四档真实输出 + tick 对比 / x86 by construction）。

- [ ] **Step 3: 推送（绝不推 main）**

Run: `git push -u origin fix/issue-445`

- [ ] **Step 4: 开 PR（auto-pr 抢先则改写）**

```bash
gh pr create --base main --head fix/issue-445 \
  --title "fix: acl_gemm_sve 行块化 body 兑现 -t 截止线，默认参数不再必然超时 (#445)" \
  --body "Closes #445 + 实现报告（根因/范围 disposition/改动/验证证据）"
```
若报"分支已有 open PR"（auto-pr.yml 抢先）：`gh pr edit fix/issue-445 --title … --body …`（正文必含 `Closes #445`）。

- [ ] **Step 5: 报告贴 issue #445**

正文：缺陷定性与根因（代码选择相交：内存模型 × 超时地板，引用 file:line）、25 格 disposition 表（已修/单独跟踪/本补丁）、改动、验证证据（含 gem5 四档输出与本机诚实边界）、PR 链接。绝不标记 resolved/closed。

- [ ] **Step 6: 工作区复位**

Run: `git checkout main && git status` → 须 clean（不留游离改动）。

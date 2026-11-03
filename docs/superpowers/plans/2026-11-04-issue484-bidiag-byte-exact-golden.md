# Issue #484: eigen_svd_bidiag_sve 字节级跨核 golden 通道 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭 issue #484 指出的 SDC 检出盲区——`eigen_svd_bidiag_sve` 的全部三条校验通道(重掷轮容差 :602、双跑自一致 memcmp :611、回放轮容差 :589)都无法捕获「确定性 + 幅度 < kTolRel×‖B‖_F」的位错;增加第四通道:init 控制线程以同一 SVE 内核对冻结输入算字节级 golden,回放轮 plain memcmp。

**Architecture:** 只增不改——三条既有通道与 DUT 内核零改动;`bidiag_data` 死字段 `work`(仅 init 置 nullptr,run/cleanup 均不触碰)复用为 `golden_sve`;init 在标量 golden 之后、worker 启动之前以 `bidiagonalize_sve`(init 本地 gofs/hv/hw scratch)对 `d->orig` 副本再算一份;run 的冻结回放分支在既有容差比对之后追加 `memcmp(work, d->golden_sve, bytes)`。跨核性来自框架事实:控制线程在 `test_init` 前被钉到芯片最大 VL(CLAUDE.md 2026-10-27 spec policy,`framework/device/cpu/arm64/arm64_spec_prepare.h:38-46`),worker 各自重钉并验证读回——init 与 worker 的 `svcntd()` 恒一致,gofs 表尺寸/内容一致;同芯片同核设计 → `svaddv` 树形归约次序一致 → 字节相等。

**Tech Stack:** C++20(aarch64 SVE ACLE `arm_sve.h`)、meson/ninja、sdcshield 框架(TEST_LOOP/report_fail_msg/log_skip)、gem5 CHAOS SE(`--sve_vl {1,2,4,8}` = 128/256/512/1024-bit)、clang 17 静态双 kernel。

**Spec:** GitHub issue #484(`gh issue view 484`)——审计 finding:golden 比对是范数聚合容差(kTolRel=1e-9 × Frobenius)而非字节级 memcmp;确定性低幅度位错同时逃过容差通道与双跑自一致通道。仓库规则依据:AGENTS.md 检视重点 3(「任何容差/近似比较 = SDC 盲区」「禁止跨 lane 聚合校验」)、CLAUDE.md「Golden comparison is byte-identical memcmp」、docs/coding/writing_tests.md SVE/SME policy(issue #373 范式及其本计划新增的 FP 树归约例外段)。

## Global Constraints

- CLAUDE.md 全部规则生效;**一个补丁一个单元**:本计划拆 3 个提交——docs(plan) / fix(test+writing_tests.md) / docs(campaign)(inventory 证据链)。
- **x86-64 untouched**:全部改动在 `tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp` 的 `#ifdef __aarch64__` 分支与 `docs/coding/writing_tests.md`;`#else` 占位路径零改动,x86 非回归靠此事实目检(纯 aarch64 分支内新增)。
- **本机无 SVE**(`--dump-cpu-info` 实测 "SVE not present"):受影响行为按 CLAUDE.md gem5-fi 强制规则必须在 gem5-fi 仿真验证并引用真实输出;本机只验 SKIP 路径 + 构建零警告 + zstd19 回归。本机 gem5 设施为 `/home/sdc/wangxu/gem5-fi-fuzz`(CLAUDE.md 的 `/home/sdc/wangxu/gem5-fi` 是他机路径,多机记录条款:换机先探测)——**只读使用**(执行其 gem5.opt 与 configs,不在该仓写任何文件),harness 源码与产物落在**本仓** inventory 目录。
- 构建:`PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release`(已配置则免)+ `SDC_BUILD_JOBS=20 ./scripts/build.sh`,零新警告。
- DCO:`git commit -s`,尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,无 Co-Authored-By;只 push `fix/issue-484`,绝不 push main。
- gem5 证据落盘:`docs/superpowers/inventory/issue484-bidiag-byte-golden/`(gem5-logs/ + verdicts.txt + 双 kernel 源),先例 `issue410-eigen-complex-1024/`。

---

### Task 1: 同源 dual-kernel gem5 harness——盲区复现(红)

**Files:**
- Create(本仓 inventory,Task 6 提交): `docs/superpowers/inventory/issue484-bidiag-byte-golden/eigen_svd_bidiag_dual.c`
- 构建产物(scratch,不提交): `/tmp/issue484/eigen_svd_bidiag_dual`

**Interfaces:**
- 逐字提取(verbatim,只做列明的检测中立适配):`bidiagonalize_ref`、`bidiagonalize_sve`、`random_pm_one` 的值域形状、`bidiag_compare_or_fail` 的判定逻辑、run 的回放/重掷/双跑结构——提取源 = 当前 main(418e4e09)的 `tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp`。
- 检测中立适配(逐项列于 harness 头注释,先例 fpu_special_sve_dual.c):框架 `random64()` → splitmix64 定种子;`report_fail_msg` → `g_fails++` + fprintf(语义:计数不中止);TLS → 局部;`arm64_require_sve_vl` 门控 → gem5 `--sve_vl` 参数天然满足。
- argv 协议:`<mode old|new> <fault none|tiny> [n=64] [iters=8] [seed=1]`;输出协议:`MODE/FAULT/VL/n` 行、逐轮 `max_diff/tol` 诊断行、`SDCSHIELD_FAILS=<uint>` 行、FNV-1a checksum 行;exit = fails?1:0。
- 故障模型(`fault=tiny`,模拟持久性 SVE 数据通路低位错——确定性、仅污染 SVE 内核输出):`dut_kernel(out)` = `bidiagonalize_sve(out,...)` 后对对角元 `out[(n/2)*(n+1)]` 执行 `bits ^= 1ULL`(尾数 LSB,~1 ULP×O(√n) 元素 ≈ 1e-15 ≪ tol≈3.7e-8@n=64);work/work2/回放 work 三处 DUT 输出均注入;`golden`/`l_golden`/`golden_sve`(`bidiagonalize_ref` 或 init 角色内核输出)不注入。`mode=old` = 仅旧三通道;`mode=new` = 旧三通道 + 回放轮 `memcmp(work, golden_sve, bytes)`。

- [x] **Step 1: 写 harness**(结构:fill orig → golden=ref(orig) → [new] golden_sve=kernel(orig) → 8 轮 iter 奇偶交替:奇=回放(orig→work→dut_kernel→容差比对→[new] memcmp golden_sve),偶=重掷(首轮强制重掷 l_orig/l_golden→work→dut_kernel→容差比对→work2→dut_kernel→memcmp(work,work2));全部 alloc 经 round_up_64)
- [x] **Step 2: 构建**:`clang -static -O2 -march=armv9-a+sve -Wall -Wextra -o /tmp/issue484/eigen_svd_bidiag_dual <src> -lm`,零警告
- [x] **Step 3: 跑盲区复现(红)**:`gem5.opt configs/se/arm_chaos.py --cmd /tmp/issue484/eigen_svd_bidiag_dual --sve_vl 4 --workload_args "old tiny 64 8 1"`(cwd=/tmp/issue484/run-old-tiny),期望 `SDCSHIELD_FAILS=0` 且诊断行显示 `max_diff≈1e-15 ≤ tol≈3.7e-8`——**盲区复现即红**:确定性 1-ULP 位错逃过全部旧通道
- [x] **Step 4: 跑健康基线**:`old none` 与 `new none`(VL=4)均期望 `SDCSHIELD_FAILS=0`;`new none` 同时验证「同上下文两次内核调用字节一致」(golden_sve 与回放 work memcmp 通过 = 内核确定性,通道无假阳)

### Task 2: 测试文件修复(green 的仓库侧实现)

**Files:**
- Modify: `tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp`(struct :110-115、init :463-496、init golden 段 :498-514、run 回放分支 :582-589、cleanup :628-637、文件头 docblock、DECLARE_TEST :665-669)

**Interfaces:**
- `bidiag_data.work`(死字段)→ `golden_sve`;init 新增 SVE-golden 计算段;run 回放分支追加 memcmp;`bidiagonalize_sve`/`bidiagonalize_ref`/双跑/重掷结构零改动。

- [x] **Step 1: struct 字段复用**——`double *work; /* 保留字段(未用...) */` → `double *golden_sve; /* init 控制线程以 SVE 内核算的冻结输入字节级期望(回放轮 memcmp,issue #484) */`,删除 init 中 `d->work = nullptr;` 行
- [x] **Step 2: init 分配三缓冲**(初始 + T10 降档重试两处):`d->golden_sve = (double *)aligned_alloc(64, bytes);`,NULL 检查与 free 补齐 `golden_sve`;`shared_bytes_per_slice` 注释 16n²→24n²、≤67MB→≤100MB、<0.2%→<0.3%
- [x] **Step 3: init 计算 SVE golden**(标量 golden + 有限性金丝雀之后):

```c
    /* 跨核字节级 golden (issue #484): 控制线程在 worker 启动前以与
     * worker 完全相同的 SVE 内核对同一冻结输入再算一份字节级期望。
     * 第四通道设计论证(为何不照抄 #373 标量 golden 范式、残留盲区
     * 边界)见文件头 "Verification design" issue #484 段。 */
    memcpy(d->golden_sve, d->orig, bytes);
    {
        double *hv = (double *)aligned_alloc(64, round_up_64((size_t)d->n * sizeof(double)));
        double *hw = (double *)aligned_alloc(64, round_up_64((size_t)d->n * sizeof(double)));
        uint64_t *gofs = (uint64_t *)aligned_alloc(64, round_up_64(svcntd() * sizeof(uint64_t)));
        if (!hv || !hw || !gofs) {
            free(hv); free(hw); free(gofs);
            free(d->orig); free(d->golden); free(d->golden_sve);
            delete d;
            log_skip(RuntimeSkipCategory, "memory: %s mdim=%d (SVE golden scratch)",
                     test->id, d->n);
            return EXIT_SKIP;
        }
        for (uint64_t l = 0; l < svcntd(); ++l)
            gofs[l] = l * (uint64_t)d->n * sizeof(double);
        bidiagonalize_sve(d->golden_sve, d->n, hv, hw, gofs);
        free(hv); free(hw); free(gofs);
    }
```

- [x] **Step 4: run 回放分支追加字节比对**(`bidiag_compare_or_fail(work, d->golden, n, iter);` 之后):

```c
            /* 字节级跨核通道 (issue #484): 确定性低幅度位错(< kTolRel×‖B‖_F,
             * 双跑逐字节一致)逃过上面容差与双跑 memcmp;init 控制线程(异核)
             * 以同一 SVE 内核算的 golden_sve 是其字节级期望——任意幅度的确定性
             * 偏差在此必失配。闸门保持 plain memcmp(#373: 向量比对闸门会为
             * 持久比对单元故障重开盲区)。 */
            if (memcmp(work, d->golden_sve, bytes) != 0) {
                for (int i = 0; i < n * n; ++i) {
                    if (memcmp(&work[i], &d->golden_sve[i], sizeof(double)) != 0) {
                        report_fail_msg("bidiagonalization deviates byte-exactly "
                                "from the init-thread SVE golden at (%d, %d): "
                                "worker %.17g vs init %.17g (iteration %ld) - "
                                "deterministic low-amplitude deviation "
                                "(issue #484 channel)",
                                i / n, i % n, work[i], d->golden_sve[i], iter);
                        break;
                    }
                }
            }
```

- [x] **Step 5: cleanup 释放** `free(d->golden_sve);`
- [x] **Step 6: 文件头 docblock** 增 issue #484 段(第四通道机理、#373 例外论证:svaddv 归约次序 impl-defined 无标量位精确等价、移除 svaddv=切除受压单元、跨核代替跨实现、保留标量容差通道作算法门(B1-015 证据:内核缺陷 ~1e-1 发散)、残留边界(a) -n 1 恰与控制线程同核 (b) 芯片级同错——两者均严格窄于修复前盲区);gem5 证据句留 Task 3 跑完后回填真实数字
- [x] **Step 7: DECLARE_TEST 描述串** 追加 ` + init-thread SVE-kernel byte-exact replay golden (issue #484)`

### Task 3: gem5 修复生效验证(green)+ 四档 sweep

**Files:**
- Test: `/tmp/issue484/eigen_svd_bidiag_dual`(harness 的 new 模式与修复后测试同源;若 Task 2 后提取源需同步,以修复后文件为准重提内核/比对段——内核本身零改动,仅通道集变化)
- 产物: `docs/superpowers/inventory/issue484-bidiag-byte-golden/gem5-logs/*.log`

- [x] **Step 1: 2×2 矩阵补全**:`new tiny`(VL=4)期望 `SDCSHIELD_FAILS=1` 且字节失配消息定位 (n/2, n/2)——修复捕获;与 Task 1 的 `old tiny`=0 对照成对
- [x] **Step 2: VL bracket**:`{old,new}×{none,tiny}` 在 `--sve_vl 1`(128-bit)复跑,期望与 VL=4 同结论
- [x] **Step 3: 四档 sweep(修复后功能验证)**:`new none` 于 `--sve_vl {1,2,4,8}`(n=64)全部期望 `SDCSHIELD_FAILS=0`;n=68(非整 VL 倍形状)于 `--sve_vl 4` 期望 `SDCSHIELD_FAILS=0`
- [x] **Step 4: 回填文件头 gem5 证据句**(档位、checksum、SDCSHIELD_FAILS=0、2×2 矩阵结论;诚实边界:SE 单上下文不能物理跨核,跨核性依赖内核确定性(双调用字节一致已验)+ 同芯片同核设计论证)

### Task 4: 文档同步(writing_tests.md 例外段)

**Files:**
- Modify: `docs/coding/writing_tests.md`(SVE/SME policy 节,"Golden must be independent of the datapath under test" 条目之后)

- [x] **Step 1: 增 FP 树归约例外段**:svaddv 类水平归约次序架构 impl-defined → 无标量位精确等价;为可仿真而移除 DUT 内的归约 = 切除受压单元,禁止;此时字节级 golden 由 `test_init` 控制线程以**同一向量内核**预计算、冻结回放轮跨核 plain memcmp,独立标量参考 + 容差保留为算法门;残留(同核单 worker 巧合、芯片级同错)须在测试 docblock 披露;先例 `tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp`(issue #484)
- [x] **Step 2: 核对 README 无需改动**:README 三处 bidiag 条目(族表 :382、knob 表 :490、Eigen 分派注 :503)均不描述校验通道——本修改不触碰其事实

### Task 5: 构建与本机验证

- [x] **Step 1: `SDC_BUILD_JOBS=20 ./scripts/build.sh`** 零新警告(受影响 TU `tests/cpu/sve/eigen/` 重编通过)
- [x] **Step 2: 本机 SKIP 路径**:`./builddir/sdcshield -e eigen_svd_bidiag_sve -t 2000 -n 1` → 无 SVE 能力诚实 SKIP(引用真实输出;此为本机可达边界,功能性验证由 Task 3 gem5 承担)
- [x] **Step 3: 回归**:`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`
- [x] **Step 4: x86-64 非回归目检声明**:改动全在 `#ifdef __aarch64__` 分支,`#else` 占位与共享代码零触碰(纯新增)

### Task 6: 提交拆分 + push + PR + issue 报告

- [x] **Step 1**: commit A `docs(plan): issue #484 bidiag 字节级跨核 golden 通道修复计划`(本文件)
- [x] **Step 2**: commit B `fix(sve): eigen_svd_bidiag_sve 增加 init 线程 SVE 内核跨核字节级 golden 通道 (#484)`(test 文件 + writing_tests.md;引用真实验证输出于 commit body)
- [x] **Step 3**: commit C `docs(campaign): issue #484 gem5 证据链`(inventory: dual 源 + gem5-logs + verdicts.txt)
- [x] **Step 4**: push `fix/issue-484`;`gh pr create --base main --head fix/issue-484 --title "fix: bidiag 回放轮增加 init 线程 SVE 内核跨核字节级 golden 通道,关闭确定性低幅度 SDC 盲区 (#484)"`(body 含 Closes #484 + 根因/改动/验证证据;auto-pr.yml 抢先则 `gh pr edit` 兜底)
- [x] **Step 5**: 报告贴 issue #484(根因、四通道设计、#373 例外论证、残留边界、gem5 2×2 矩阵与四档证据、PR 链接);`git checkout main && git status` clean

## Self-Review

- **Spec 覆盖**:issue 的两个指控——「容差而非 memcmp」→ 第四通道 plain memcmp(回放轮主 SDC 闸);「范数聚合掩蔽单 lane 小错」→ 字节级逐元素 memcmp 捕获任意幅度单元素偏差,容差通道降级为算法门并保留(B1-015 强度不回退)。✓
- **无占位符**:Task 2 全部代码块完整;Task 1 harness 结构 + 适配契约 + argv/输出协议完整,源码逐字提取规则明确(源文件与函数名给定)。✓
- **类型一致**:`golden_sve` 命名在 struct/init/run/cleanup/harness 诸任务一致;`round_up_64`/`aligned_alloc` 契约沿用。✓

# Issue #488: 显式 -O mdim 在 init bad_alloc 时静默降档 → 资源性 SKIP 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用户显式指定的 `-O <test>.mdim=N`（或 env 通道）在 init 期 `std::bad_alloc` 时不再被静默降档到 16 继续跑并报 PASS，改为资源性 `EXIT_SKIP`（诚实跳过）；自适应 mdim 的现有降档重试行为保持不变。

**Architecture:** T10 已把 7 个 eigen_gemm 用例的 init 降档块收敛进 `sandstone_gemm_common.h::sdc_gemm_init_shared`（单点）；但该 helper 只拿到 mdim 回调的最终 int，无法区分「用户显式」与「自适应」（全族 knob key 统一 `"mdim"` 但 clamp 分 4096/8192 两档、env 通道三四种形态，共享头内重查不可行）。修复采用**出参传递显式性**：mdim 回调签名加 `bool *explicit_dim`（共享头预置 false，回调在值来自 knob/env 显式通道时置 true），catch(bad_alloc) 头部分流——显式 → `log_skip` + `EXIT_SKIP`；否则走原降档路径（逐行不动）。

**Tech Stack:** C++20（`-fno-exceptions` 不适用——本路径本就用异常）、Eigen 5（vendored）、meson/ninja、bash ulimit -v 构造 bad_alloc。

**Spec:** issue #488（GitHub）；前置批判性分析见 fleet 任务书。核心契约冲突：T11「显式值用户自担、旁路一切自动」（`framework/sdc_stress_config.h` T11 块）与 CLAUDE.md rule 6「no-op test reporting pass is a bug」。

## Global Constraints

- 一个补丁一个逻辑单元（CLAUDE.md rule 1）：签名改动 + 7 文件适配 + 分流逻辑 = 同一 commit（拆开编译不过），`git commit -s`，尾行 `Signed-off-by: wangxumarshall <wangxumarshall@qq.com>`。
- x86-64 非回归（rule 5 / AGENTS.md rule 2）：`sdc_gemm_init_shared` 是 2026 年自研共享头（issue #440 收敛，非 Intel 原码），跨架构无守卫是既成事实；本修只动 catch(bad_alloc) 错误路径的**结果**（降档假 PASS → SKIP），正常路径与自适应降档路径字节不变，两架构一致受益——属修 bug 非改 x86 语义，安全论证记录于 PR。
- 只推 `fix/issue-488`，绝不 push main，绝不 merge/close（红线 1/2）。
- 零新警告构建（rule 2a）；验证 100% 真实引用输出（rule 2b）。
- SKIP 语义对齐框架先例：`log_skip(RuntimeSkipCategory, "memory: ...")`（worker 期 bad_alloc 同款），非 rule 6 的 placeholder 文案（那是给未实现功能的）。

## Review Focus

1. **显式 mdim + init bad_alloc** → 必须 SKIP 且理由可见（本修复本体；测试：ulimit + `-O mdim=4096`）。
2. **自适应 mdim + init bad_alloc** → 降档重试路径不得误伤（测试：ulimit 不带 -O，仍见 `[init 降级]` 且 PASS）。
3. **mdim 求解期（探针）bad_alloc** → `explicit_dim` 预置 false，探针异常走原 `catch(...)` 退守 16，不误判为显式（代码结构保证：探针路径不触写出参）。
4. **dump 变体冻结回放一致性**：SKIP 路径不产矩阵、run 不执行，`d->dim` 不再被覆写——降档路径的同档一致性行为不变（代码论证）。
5. **显式 mdim 正常（无 bad_alloc）** → 行为字节不变（测试：无 ulimit + `-O mdim=256` 短跑 PASS，无新日志形态）。

---

### Task 1: 修复前基线——复现 issue 行为

**Files:**
- 无代码改动（干净 main @ aeb2c1ea 上运行）

**Interfaces:**
- Consumes: `./builddir/sdcshield`（main 构建产物）
- Produces: before 行为实录（PR 验证段引用）

- [x] **Step 1: 构建 main 基线**

```bash
cd /home/wangxu/sdcshield
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release 2>&1 | tail -3
./scripts/build.sh 2>&1 | tail -5
```
Expected: 构建成功。若 builddir 已存在则 `meson setup --reconfigure builddir`。

- [x] **Step 2: ulimit 探测——复现显式 mdim 静默降档假 PASS**

```bash
(ulimit -v 262144; ./builddir/sdcshield -O eigen_gemm_double14.mdim=4096 \
   -e eigen_gemm_double14 -t 1000 -n 1 2>&1 | grep -E "init 降级|压力映射|eigen_gemm_double14|exit" | head -8)
```
Expected（修复前）: 出现 `[init 降级] ... mdim=4096 → 降档 mdim=16`，测试 **PASS**（issue #488 的假通过）。ulimit 值需按本机实测调（4096²×8B×3 矩阵 = 384MB，探针抬升档 ~96MB；目标卡在中间）——若 init 未炸（PASS 且无降级日志）则调低，若框架自身起不来则调高。

- [x] **Step 3: ulimit 探测——确认自适应降档路径可触发**

```bash
(ulimit -v <Step2 定值>; ./builddir/sdcshield -e eigen_gemm_double14 -t 1000 -n 1 2>&1 | grep -E "联合求解|init 降级|exit" | head -8)
```
Expected（修复前）: `[联合求解]` 行（n_mem/n_run）后见 `[init 降级] ... → 降档 mdim=16`，PASS。若探针先炸（`[联合求解] ... 探针异常 → 退守下限`）则微调 ulimit 让 2048 档探针通过；实在卡不出则在 PR 如实声明该验证边界（自适应降档不变由代码结构论证 + Task 4 Step 2 正常自适应跑佐证）。

### Task 2: 修复——共享头 catch 分流 + 7 文件出参适配

**Files:**
- Modify: `tests/cpu/eigen_gemm/sandstone_gemm_common.h:51-82`（签名 + 分流 + 契约注释）
- Modify: `tests/cpu/eigen_gemm/double14.cpp:52`（gemm_mdim 签名 + 显式分支）
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square.cpp:59`
- Modify: `tests/cpu/eigen_gemm/gemm_float_dynamic_square.cpp:56`
- Modify: `tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square.cpp:58`
- Modify: `tests/cpu/eigen_gemm/gemm_double_dynamic_square_dump.cpp:67`（gemm_k_dim）
- Modify: `tests/cpu/eigen_gemm/gemm_float_dynamic_square_dump.cpp:67`（gemm_k_dim）
- Modify: `tests/cpu/eigen_gemm/gemm_cdouble_dynamic_square_dump.cpp:61`

**Interfaces:**
- Produces: `sdc_gemm_init_shared(test, d, int (*mdim)(struct test *, bool *))`——出参契约：**共享头调用前预置 `*explicit_dim = false`；mdim 仅在返回值来自用户显式通道（-O/配置文件 knob 或 env）时置 true**；mdim 抛异常时出参保持 false（共享头catch按自适应处理）。7 个 mdim/gemm_k_dim 全部改签名，编译器保证无遗漏。

- [x] **Step 1: 共享头——签名与分流**

`sdc_gemm_init_shared` 改为（关键 diff，保留其余原样）：

```cpp
template <typename Data>
static inline int sdc_gemm_init_shared(struct test *test, Data *d,
                                        int (*mdim)(struct test *, bool *))
{
    using Mat = decltype(d->lhs);
    int dim = 0;
    bool explicit_dim = false;   /* mdim 仅在显式通道(knob/env)置 true */
    try {
        dim = mdim(test, &explicit_dim);   /* knob > env > 自适应, 只解一次 */
        d->dim = dim;
        d->lhs = Mat::Random(dim, dim);
        d->rhs = Mat::Random(dim, dim);
        d->prod = d->lhs * d->rhs;
    } catch (const std::bad_alloc &) {
        if (explicit_dim) {
            /* issue #488: 用户显式 -O/env 指定的 mdim 不可降档——T11 契约
             * 「显式值用户自担、旁路一切自动」, 静默 4096→16 把压力砍到
             * 1/65536 还报 PASS 属假通过(rule 6), 资源不足诚实 SKIP。 */
            log_skip(RuntimeSkipCategory,
                     "memory: %s mdim=%d (user-explicit, not downgradable)",
                     test->id, dim);
            return EXIT_SKIP;
        }
        constexpr int kInitRetryLo = 16;
        /* ...原降档重试块逐行不动... */
```

头注释同步补分流契约（显式→SKIP / 自适应→降档）。

- [x] **Step 2: 7 个文件——签名 + 显式分支**

每个文件的 mdim/gemm_k_dim 签名加 `bool *is_explicit`，显式返回分支置 true。6 个 `if (v > 0) return` 型文件（double14 / double / float / cdouble square 及 double / float dump）：

```cpp
static int gemm_mdim(struct test *test, bool *is_explicit)
{
    ...
    if (v > 0) {
        *is_explicit = true;     /* knob/env 显式通道 */
        return (int)v;
    }
    ...
```

cdouble_dump（`if (v <= 0)` 包自适应型）：

```cpp
static int gemm_mdim(struct test *test, bool *is_explicit)
{
    int64_t v = sdc_knob_int_clamped(test, "mdim", 0, 16, 4096);
    if (v <= 0) {
        ...原自适应块不动...
    } else {
        *is_explicit = true;     /* knob 显式通道 */
    }
    return (int)v;
}
```

dump 两文件的 `GEMM_K_DIM` env 通道也属显式（v > 0 即显式来源，同分支覆盖）。

- [x] **Step 3: 增量重建，零新警告**

```bash
./scripts/build.sh 2>&1 | tee /tmp/build488.log | tail -5
grep -iE "warning|error" /tmp/build488.log | grep -v "thrid-party\|third_party" | head
```
Expected: 编译成功（签名不匹配会硬报错），无新 warning。

### Task 3: 行为验证（after）

**Files:** 无代码改动。

- [x] **Step 1: 显式 mdim + bad_alloc → SKIP**

```bash
(ulimit -v <Task1 定值>; ./builddir/sdcshield -O eigen_gemm_double14.mdim=4096 \
   -e eigen_gemm_double14 -t 1000 -n 1 2>&1 | grep -E "user-explicit|skip|exit|eigen_gemm_double14" | head -6)
```
Expected: `memory: eigen_gemm_double14 mdim=4096 (user-explicit, not downgradable)` + 测试 **SKIP**，总行含 skipped: 1。无 `[init 降级]`。

- [x] **Step 2: 自适应 + bad_alloc → 降档不变**

```bash
(ulimit -v <同值>; ./builddir/sdcshield -e eigen_gemm_double14 -t 1000 -n 1 2>&1 | grep -E "init 降级|exit" | head -4)
```
Expected: 与 Task 1 Step 3 修复前输出一致（`[init 降级] → mdim=16` + PASS）。

- [x] **Step 3: 显式 mdim 正常路径不回归**

```bash
./builddir/sdcshield -O eigen_gemm_double14.mdim=256 -e eigen_gemm_double14 -t 2000 -n 1 2>&1 | tail -3
```
Expected: PASS（显式小尺寸照常执行）。

- [x] **Step 4: 自适应正常路径不回归（无资源约束）**

```bash
./builddir/sdcshield -e eigen_gemm_double14 -t 2000 -n 1 2>&1 | grep -E "联合求解|exit" | tail -3
```
Expected: `[联合求解]` 正常求解 + PASS。

- [x] **Step 5: dump 变体抽查（显式 SKIP 同样生效）**

```bash
(ulimit -v <同值或更低——dump 是 double 同尺寸>; ./builddir/sdcshield -O eigen_gemm_double_dynamic_square_dump.mdim=4096 \
   -e eigen_gemm_double_dynamic_square_dump -t 1000 -n 1 2>&1 | grep -E "user-explicit|exit" | head -4)
```
Expected: SKIP 同款理由。

### Task 4: 回归 + 提交 + PR + issue 处置评论

- [x] **Step 1: zstd19 回归**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -2
```
Expected: `exit: pass`。

- [x] **Step 2: x86-64 非回归（by inspection, rule 2d）**

检查 diff：改动仅 `#if` 守卫之外的共享错误路径与各文件 mdim（x86 侧同样编译的代码）。论证：正常路径与自适应降档路径字节不变；显式+bad_alloc 从假 PASS 变 SKIP 是诚实性修复，两架构一致正确；无 `#ifdef __x86_64__` 改动。记录于 PR body。

- [x] **Step 3: 提交 + push + PR + issue 评论**

```bash
git checkout -b fix/issue-488
git add tests/cpu/eigen_gemm/
git branch --show-current   # 必须是 fix/issue-488
git commit -s -m "fix(gemm): 显式 -O mdim 在 init bad_alloc 时资源性 SKIP, 不再静默降档假 PASS (#488)"
git push origin fix/issue-488
```
PR title: `fix: 显式 -O mdim 在 init bad_alloc 时资源性 SKIP 而非静默降档 (#488)`；body 含问题分析/修复方案/before-after 验证实录 + `Closes #488` + 🤖 尾注。API 建 PR 与 issue 评论（`.fleet/env`）。

- [x] **Step 4: progress-log 追加一条**（session recovery 契约），与代码同一纪律（可并入 Step 3 提交或单独说明——按「一个补丁一个单元」，代码 PR 不夹带，log 追加仅在报告完成后视仓库惯例决定；若夹带会违反单元性则只在 issue 评论留档）。

## Self-Review

- Spec 覆盖：issue 三要点（显式直取旁路 / catch 覆写降档 / 同模式多文件）分别由 Task 2 出参契约、Task 2 Step 1 分流、Task 2 Step 2 七文件全覆盖；bidiag 文件已退役（核实过 `ls` 无此文件），范围正确。
- 占位符：无 TBD；ulimit 具体值标注「按实测调」并给出调参方向与兜底声明——这是实测参数而非设计缺口。
- 类型一致：`int (*)(struct test *, bool *)` 在共享头与 7 文件间一致；`log_skip(RuntimeSkipCategory, ...)` 与框架先例一致。
- Review Focus 五条均有对应用户可见验证或代码论证（见各 Task）。

---

## 执行记录（2026-11-08，实测）

**方案偏差（重要）**：原计划用 `ulimit -v` 构造 bad_alloc。实测发现生产构建链接
`framework/sysdeps/linux/malloc.cpp` 的链接级 malloc 覆盖（Intel 原版
`null_pointer_consumption`）：分配失败直接 `raise(SIGKILL)`（模拟 OOM killer），
**bad_alloc 在生产构建不可达**——ulimit 下显式 mdim=4096 表现为 crash（code 9，
strace 实证 `tgkill` 自杀、3 次 128MB mmap ENOMEM 后被拦），降档假 PASS 不发生。
改用 **ASAN 构建**（`builddir-asan -Db_sanitize=address`，插桩被 `__SANITIZE_*`
排除）+ `ASAN_OPTIONS=allocator_may_return_null=1:max_allocation_size_mb=N` 构造
单次分配超限 → operator new 抛 bad_alloc → catch 真实可达。issue 描述的
「静默降档假 PASS」在 ASAN 构建下完整复现。

**验证矩阵（全部真实输出，详见 PR #fix-488）**：

| 场景 | 构建 | 修复前 | 修复后 |
|---|---|---|---|
| 显式 mdim=4096 + init bad_alloc | ASAN cap=64MB | `result: pass` + `[init 降级] mdim=4096 → 降档 mdim=16 重试一次`（假 PASS，issue 复现） | `result: skip`，`skip-reason: 'memory: eigen_gemm_double14 mdim=4096 (user-explicit, not downgradable)'` |
| 自适应 mdim=1093 + init bad_alloc | ASAN cap=8MB | （同模式降档） | `[init 降级] ... mdim=1093 → 降档 mdim=16` + `result: pass`（原路径不变） |
| 自适应正常求解 | ASAN cap=32MB | PASS | PASS（`[联合求解] n_mem=19451 n_run=1094 → 1094`） |
| 显式 mdim=256 正常 | ASAN | PASS | PASS |
| dump 变体显式 mdim=4096 + bad_alloc | ASAN cap=64MB | （同模式） | `result: skip`（同款 skip-reason） |
| zstd19 回归 | 生产 | — | `exit: pass` |
| 显式 mdim=4096 内存不足 | 生产 ulimit 256MB | crash（code 9 Killed） | crash（code 9 Killed，不变——插桩使 bad_alloc 不可达，x86/生产零行为差） |

构建：生产 + ASAN 双 builddir 增量重建均零新警告。

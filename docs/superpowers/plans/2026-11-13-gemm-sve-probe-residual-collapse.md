# GEMM SVE body 探针残余副本收口(issue #615 处置)Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 3 份 SVE GEMM payload body(`gemm_{f64,f32,c128}_sve_body.cpp`)中逐字复制的 splitmix sizing 探针收口为一行转发共享模板 `gemm_solve_probe.h`(gemm_double_max_arm_body.cpp 既有范式),消灭 issue #615 指控的谱系契约漂移面在现役 main 上的最后残存副本。

**Architecture:** 三步走:先用双 kernel 等价 harness(逐字旧副本 vs 共享模板,byte-identical 3 通道)证明收口前提成立;再对 3 个 body 做最小文本替换(include + 头注释 + 探针体→转发);最后 gem5-fi SE 模式按 128/256/512/1024 四档真跑双 kernel 取证(本机无 SVE,真机必 SKIP,CLAUDE.md gem5-fi 规则全额适用)。

**Tech Stack:** sdcshield(meson/ninja, GCC, aarch64, vendored Eigen 5 / `third-party/eigen5`)、gem5 SE(`scripts/eigen-sve-double/gem5/run_sve.sh` + `se_sve.py`,G5 解析为 `/home/sdc/wangxu/gem5-fi-fuzz/build/ARM/gem5.opt`)、bash。

**Spec:** issue #615 正文(审计指控)+ 本计划「处置判定」节(需求裁定)+ 先例契约:`eigen_splitmix_lineage.h` 漂移审计指引(检视 #392-R6)、`gemm_solve_probe.h` 头注释(检视 #392-R3/R5/R9)、`gemm_double_max_arm_body.cpp:144-147`(转发范式)、issue #410 verdicts(双 kernel 协议)。

## 处置判定(audit triage,需求裁定)

issue #615 = GEMM 探针/复制缺陷谱系**第 6 次上报**(#389/#440 → #539/#546 → #573 → #615 → #667)。两项指控分别处置:

**指控 1「gemm_solve_once sizing 探针(~30 行 splitmix 填充)在 7 个 GEMM TU 逐字复制」——属实但已于 2026-11-04 修复,当前 main 零残留,不重做。**
- 锚点快照:`623e5a6c`(2026-11-04 18:55:28,PR #392 合入前 21 秒的 main)——`git show 623e5a6c:tests/cpu/eigen_gemm/double14.cpp | sed -n '40p'` 命中全量探针副本起点,与 #573 同一审计快照。
- 修复链:PR #392(`e95013c5`,正文 Closes #389)把 8 份探针(7 个 T9 TU + T6 源头 `gemm_double_max_arm_body.cpp`)收口至 `tests/cpu/eigen_gemm/gemm_solve_probe.h`;PR #452(`3fb18cc5`,Closes #440)把 init 降档块收敛至 `sandstone_gemm_common.h`。两者 `git merge-base --is-ancestor` 均在 origin/main。
- 当前 main 实测:7 个 TU 的 `gemm_solve_once` 各只剩 4 行转发 wrapper(`awk '/static int gemm_solve_once/,/^}/' | wc -l` = 4 ×7);全仓 grep `0x9E3779B97F4A7C15` 在 7 个审计 TU 命中 0 条。

**指控 2「sdc_joint_args 参数块在 7 个 TU 仅 fam/clamp 不同,未沿用 SVD calibrated_auto_dim 先例」——是 PR #452 显式裁定的设计边界,不重开。**
- `sandstone_gemm_common.h:19` 明文:「不在共享范围(如实边界): gemm_mdim 的 knob/env/clamp/fam 是各文件真参数」——该裁定经四 bot 检视 + 人工 merge。
- SVD 先例可参数化恰恰因为其 aarch64/x86 两分支共用同组参数;GEMM 族两分支参数刻意不同(如 double14 aarch64 6 矩阵/1.0 legacy 三元组 vs x86 4 矩阵/6.0,rule 5 禁改 x86)。SVD 式一刀切对锚点文件本身不适用。若人工希望重开该边界,属独立决策,本 fix 不代办。

**残存缺陷(本 PR 修复对象,PR #542「主体已修但补残余」先例):**
`gemm_f64_sve_body.cpp:115-124`、`gemm_f32_sve_body.cpp:115-124`、`gemm_c128_sve_body.cpp:123-139` 各含一份**手写 splitmix 魔法数全量探针副本**(0x9E3779B97F4A7C15/0xC2B2AE3D27D4EB4F/0xD6E8FEB86659FD93 内联,`h % 2000u / 1000.0 - 1.0` 值域,未引用共享头):
- 引入时间:`4a6c7acf`(2026-11-03 17:57,feat/eigen-sve-unification)——**先于** PR #392 合并(2026-11-04 18:55),PR #392 的「8 份」收口清单在分支基线上不含这 3 份,合并时留在原地。
- 是**活体 sizing 探针**:`gemm_double_dynamic_square_sve.cpp:69` 等 dispatcher 以 `sdc_joint_dim_slice(test, "mdim", &ja, gemm_f64_solve_once)` 逐档消费(`pfx##_solve_once` 宏按 pinned VL 分派到 `<pfx>_vl<N>::solve_once`),f64 body 同时服务 `eigen_gemm_double_dynamic_square_sve` 与 `eigen_gemm_double14_sve`。受影响测试 id 共 4 个。
- 正是 `eigen_splitmix_lineage.h` 漂移审计指引(检视 #392-R6)定义的有效审计对象:「两家族代码路径中新增的未引用本头的逐字副本」。issue #615 的核心风险陈述(「谱系契约『改值域/结构必须与 fill_rand 同步演化』分散多处,一处演化漏其余会静默破坏探针计时口径」)在这 3 份副本上真实成立。

## Global Constraints

- 分支:`git checkout -B fix/issue-615 origin/main`(基线 3631a7d4);**绝不提交/推送 main**;全程只在此分支。
- DCO:`git commit -s`,末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,无 Co-Authored-By;提交前 `git branch --show-current`。
- 一个补丁一个单元:计划落盘 / 等价 harness / 探针收口 / 勾选+证据,四个独立 commit,先验证后 commit,逐个 push 到 fix/issue-615。
- rule 2 验证 100% 真:构建零新警告;双 kernel byte-identical 引用真实输出;回归 `-e zstd19 -t 2000 -n 1` exit: pass;x86-64 非回归按检视声明(3 个 body 是 `if host_machine.cpu_family() == 'aarch64'` meson 门控专属 TU,x86 从不编译,纯 aarch64 内部重构;`gemm_solve_probe.h` 只改注释零逻辑)。
- **gem5-fi 规则全额适用**:本机无 SVE(`grep -o "sve" /proc/cpuinfo` 空、`LD_SHOW_AUXV=1 /bin/true | grep HWCAP_SVE` 空,128 核)→ 4 个受影响测试真机必 SKIP,「SKIP 理由不变」不构成功能性验证;四档证据 = 双 kernel 在 gem5 SE 真跑(档位 echo、FNV checksum、Simulated exit code: 0)。gem5 身份记录:本机 G5 解析 md5 `5101f2a811fa5cfc17c6e52d51c39f18`(gem5-fi-fuzz 副本,9月8日构建)≠ campaign 的 `3f9011f40dbe80ec1df478c62ddb0b06`——如实披露;双 kernel 不依赖 prctl stub(`se_sve.py` 直接设 `ArmISA.sve_vl_se`),功能性验证不受 stub 集差异影响。本机 gem5 SE 预检已通过(trivial binary `--vl 1` → `Simulated exit code: 0 @ tick 82465000`)。
- 原始 gem5 运行日志**不入库**(campaign 裁定 E):证据以 PR 正文/issue 报告中引用的关键行呈现;双 kernel 源码入库(scripts/eigen-sve-double/ 既有先例)。
- 文档同步:本次为测试内部等价重构,无测试数/行为/CLI 变化,README 与 docs/ 无需更新(如实声明)。
- 结束时工作区回到干净 main(`git checkout main && git status` clean)。

---

### Task 1: 建分支、落盘本计划

**Files:**
- Create: `docs/superpowers/plans/2026-11-13-gemm-sve-probe-residual-collapse.md`(本文件)

**Steps:**

- [ ] **Step 1: 建分支**(执行会话以 origin/main 为基)

```bash
git checkout -B fix/issue-615 origin/main
git branch --show-current   # 期望: fix/issue-615
```

- [ ] **Step 2: 本计划写入该路径并 commit**

```bash
git add docs/superpowers/plans/2026-11-13-gemm-sve-probe-residual-collapse.md
git commit -s -m "docs(plan): issue #615 处置计划——3 份 SVE GEMM body 探针残余副本收口

指控 1 已由 PR #392/#452 于 2026-11-04 修复(锚点 623e5a6c 快照,当前 main
7 个 TU 仅剩转发 wrapper);指控 2 为 PR #452 显式裁定的真参数边界。残存缺口:
gemm_{f64,f32,c128}_sve_body.cpp 三份逐字 splitmix 探针(4a6c7acf 引入,先于
#392 合并漏入收口清单,dispatcher 逐档活体消费)——收口至 gemm_solve_probe.h,
#542 先例「主体已修但补残余」。"
```

- [ ] **Step 3: push**

```bash
git push -u origin fix/issue-615
```

---

### Task 2: 谱系等价双 kernel(native 腿先行验证收口前提)

**Files:**
- Create: `scripts/eigen-sve-double/test_gemm_probe_lineage.cpp`

**Interfaces:**
- Consumes: `tests/cpu/eigen_gemm/gemm_solve_probe.h`(`gemm_probe::fill_pair<Mat>`/`gemm_probe::solve_once<Mat>`,相对 include `../tests/cpu/eigen_gemm/gemm_solve_probe.h`)。
- Produces: 可执行双 kernel,退出码 0 = 全部 (变体×尺寸×a/b/c) byte-identical;stdout 每行含 FNV-1a checksum 供四档取证引用。

**Steps:**

- [ ] **Step 1: 写入双 kernel 源(完整内容如下,含逐字旧副本三变体)**

```cpp
// GEMM sizing-probe lineage equivalence (issue #615 residual sweep).
//
// Proves byte-identical equivalence between the pre-collapse VERBATIM inline
// splitmix probes (as carried by gemm_{f64,f32,c128}_sve_body.cpp on
// origin/main 3631a7d4) and the shared single-source template
// tests/cpu/eigen_gemm/gemm_solve_probe.h::gemm_probe (PR #392 collapse).
// The same three variants are what the 7 non-SVE T9 TUs carried pre-#392
// (PR #452 triage: 3 md5 clusters), so this re-proves the T9 collapse
// equivalence against the CURRENT template — the precondition the SVE-body
// forwarding fix relies on.
//
// Channels (issue #410 dual-kernel protocol):
//   A — old vs new byte-identical a/b/c per (variant, n): old fill == verbatim
//       loop, new fill == gemm_probe::fill_pair, then the SAME `c = a * b`;
//       every element finite (catches NaN-payload divergence);
//   B — FNV-1a 64-bit checksums printed per matrix for log quoting;
//   C — VL identity echo: prints the runtime SVE vector length so every
//       tier run self-certifies its tier.
//
// Build & run (native non-SVE host → NEON/scalar packets):
//   g++ -O2 -std=c++17 -I third-party/eigen5 \
//       scripts/eigen-sve-double/test_gemm_probe_lineage.cpp \
//       -o /tmp/gemm_probe_lineage_native && /tmp/gemm_probe_lineage_native
// Build per SVE tier (README recipe, scripts/eigen-sve-double/gem5/):
//   g++ -O2 -std=c++17 -I third-party/eigen5 -march=armv8.2-a+sve \
//       -msve-vector-bits=<128|256|512|1024> -DEIGEN_ARM64_USE_SVE \
//       scripts/eigen-sve-double/test_gemm_probe_lineage.cpp \
//       -o /tmp/gem5_bins/gemm_probe_lineage_<tier>
// Run tier in gem5 SE (run_sve.sh echoes the resolved gem5 md5 — record it):
//   scripts/eigen-sve-double/gem5/run_sve.sh \
//       /tmp/gem5_bins/gemm_probe_lineage_<tier> --vl <1|2|4|8>
#include <Eigen/Core>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <complex>
#include <vector>

#include "../tests/cpu/eigen_gemm/gemm_solve_probe.h"

using namespace Eigen;

typedef Matrix<double, Dynamic, Dynamic> MatD;
typedef Matrix<float, Dynamic, Dynamic> MatF;
typedef Matrix<std::complex<double>, Dynamic, Dynamic> MatC;

#if defined(__ARM_FEATURE_SVE)
#include <arm_sve.h>
#endif

static int failures = 0;
static int triplets = 0;

/* ---- channel B: FNV-1a 64-bit ---- */
static std::uint64_t fnv1a(const void *p, std::size_t bytes)
{
    const unsigned char *q = (const unsigned char *)p;
    std::uint64_t h = 1469598103934665603ull;
    for (std::size_t i = 0; i < bytes; ++i) {
        h ^= q[i];
        h *= 1099511628211ull;
    }
    return h;
}

/* ---- channel A "old" leg: VERBATIM fill loops (loop bodies kept
 * character-identical to origin/main 3631a7d4; only the enclosing signature
 * adapted to expose the matrices for comparison):
 *   f64 : gemm_f64_sve_body.cpp:115-124 (scalar double stream)
 *   f32 : gemm_f32_sve_body.cpp:115-124 (scalar double stream, f32 store
 *         via double intermediate — 原用例同款转型)
 *   c128: gemm_c128_sve_body.cpp:123-139 (complex re/im 四流) */
static void old_solve_f64(int dim, MatD &a, MatD &b, MatD &c)
{
    a.resize(dim, dim); b.resize(dim, dim);
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (double)(h % 2000u) / 1000.0 - 1.0;
            b(r, col) = (double)(h2 % 2000u) / 1000.0 - 1.0;
        }
    c = a * b;
}

static void old_solve_f32(int dim, MatF &a, MatF &b, MatF &c)
{
    a.resize(dim, dim); b.resize(dim, dim);
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            a(r, col) = (float)((double)(h % 2000u) / 1000.0 - 1.0);
            b(r, col) = (float)((double)(h2 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
}

static void old_solve_c128(int dim, MatC &a, MatC &b, MatC &c)
{
    a.resize(dim, dim); b.resize(dim, dim);
    for (int r = 0; r < dim; ++r)
        for (int col = 0; col < dim; ++col) {
            std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                            ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
            h ^= h >> 32;
            h *= 0xD6E8FEB86659FD93ull;
            h ^= h >> 32;
            std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h2 ^= h2 >> 32;
            std::uint64_t h3 = h2 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h3 ^= h3 >> 32;
            std::uint64_t h4 = h3 * 0xD6E8FEB86659FD93ull
                             + 0x9E3779B97F4A7C15ull;
            h4 ^= h4 >> 32;
            a(r, col) = std::complex<double>(
                (double)(h % 2000u) / 1000.0 - 1.0,
                (double)(h2 % 2000u) / 1000.0 - 1.0);
            b(r, col) = std::complex<double>(
                (double)(h3 % 2000u) / 1000.0 - 1.0,
                (double)(h4 % 2000u) / 1000.0 - 1.0);
        }
    c = a * b;
}

template <typename Mat>
static std::size_t matrix_bytes(const Mat &m)
{
    return (std::size_t)m.rows() * m.cols() * sizeof(typename Mat::Scalar);
}

template <typename Mat>
static bool bytes_equal(const Mat &x, const Mat &y)
{
    return x.rows() == y.rows() && x.cols() == y.cols()
        && std::memcmp(x.data(), y.data(), matrix_bytes(x)) == 0;
}

template <typename Mat>
static std::uint64_t matrix_fnv(const Mat &m)
{
    return fnv1a(m.data(), matrix_bytes(m));
}

/* channel A finiteness leg: all elements finite ([-1,1) by construction). */
static bool all_finite(const MatD &m)
{
    for (int i = 0; i < m.size(); ++i)
        if (!std::isfinite(m.data()[i])) return false;
    return true;
}
static bool all_finite(const MatF &m)
{
    for (int i = 0; i < m.size(); ++i)
        if (!std::isfinite(m.data()[i])) return false;
    return true;
}
static bool all_finite(const MatC &m)
{
    for (int i = 0; i < m.size(); ++i) {
        if (!std::isfinite(m.data()[i].real())) return false;
        if (!std::isfinite(m.data()[i].imag())) return false;
    }
    return true;
}

/* One (variant, n) comparison. The "new" leg reproduces the exact 3-line
 * body of gemm_probe::solve_once<Mat> (the template builds its matrices
 * internally and discards them; its body is literally construct →
 * fill_pair → multiply). */
template <typename Mat, typename OldFn>
static void compare(const char *name, OldFn old_solve, int n)
{
    Mat ao, bo, co, an(n, n), bn(n, n), cn;
    old_solve(n, ao, bo, co);
    gemm_probe::fill_pair(an, bn);
    cn = an * bn;

    triplets++;
    const bool ok_ab = bytes_equal(ao, an) && bytes_equal(bo, bn);
    const bool ok_c = bytes_equal(co, cn);
    const bool ok_fin = all_finite(co) && all_finite(cn);
    const bool ok = ok_ab && ok_c && ok_fin;
    if (!ok) failures++;
    std::printf("%s %-5s n=%-3d a=%016llx b=%016llx c=%016llx %s%s%s\n",
                ok ? "ok" : "FAIL", name, n,
                (unsigned long long)matrix_fnv(an),
                (unsigned long long)matrix_fnv(bn),
                (unsigned long long)matrix_fnv(cn),
                ok_ab ? "" : " [a/b MISMATCH]",
                ok_c ? "" : " [c MISMATCH]",
                ok_fin ? "" : " [NON-FINITE]");
}

int main(void)
{
#if defined(__ARM_FEATURE_SVE)
    std::printf("VL identity: svcntb=%llu bytes (%llu-bit tier)\n",
                (unsigned long long)svcntb(),
                (unsigned long long)svcntb() * 8);
#else
    std::printf("VL identity: non-SVE build (NEON/scalar packets) — host leg\n");
#endif
    for (int n : {1, 2, 3, 5, 16, 17, 33}) {
        compare<MatD>("f64", old_solve_f64, n);
        compare<MatF>("f32", old_solve_f32, n);
        compare<MatC>("c128", old_solve_c128, n);
    }
    std::printf("RESULT: %s (%d/%d (variant,n) triplets byte-identical)\n",
                failures == 0 ? "ALL PASS" : "MISMATCH", triplets - failures,
                triplets);
    return failures == 0 ? 0 : 1;
}
```

- [ ] **Step 2: native 编译并运行(NEON/scalar 腿,收口前提先行验证)**

```bash
g++ -O2 -std=c++17 -I third-party/eigen5 \
    scripts/eigen-sve-double/test_gemm_probe_lineage.cpp \
    -o /tmp/gemm_probe_lineage_native
/tmp/gemm_probe_lineage_native; echo "exit=$?"
```

Expected:`RESULT: ALL PASS (21/21 …)`,`exit=0`。若 FAIL:收口前提不成立,停止、报告 escalated,不得猜测式收口。

- [ ] **Step 3: commit harness**

```bash
git add scripts/eigen-sve-double/test_gemm_probe_lineage.cpp
git commit -s -m "test(scripts): GEMM 探针谱系等价双 kernel——逐字旧副本 vs 共享模板

三变体(f64/f32/c128)×7 尺寸 ×(a,b,c) byte-identical 三通道(#410 协议:
A byte 比对+有限性 / B FNV-1a / C VL echo)。旧腿 = 收口前逐字副本
(3631a7d4),新腿 = gemm_solve_probe.h 共享模板。native 腿 21/21 ALL PASS;
SVE 四档证据见 issue #615 处置报告(gem5 SE,本机无 SVE)。"
git push
```

---

### Task 3: 三 body 探针收口 + probe 头注释增补(修复单元)

**Files:**
- Modify: `tests/cpu/eigen_gemm/gemm_f64_sve_body.cpp`(include 16 行后、头注释 8-10 行、探针体 109-125 行)
- Modify: `tests/cpu/eigen_gemm/gemm_f32_sve_body.cpp`(include 15 行后、头注释 8-10 行、探针体 108-126 行)
- Modify: `tests/cpu/eigen_gemm/gemm_c128_sve_body.cpp`(include 17 行后、头注释 11-13 行、探针体 116-141 行)
- Modify: `tests/cpu/eigen_gemm/gemm_solve_probe.h`(头注释「措辞勘误(检视 #392-R9)」段后增补使用方谱系)

**Interfaces:**
- Consumes: `gemm_probe::solve_once<Mat>`(签名 `template <typename Mat> static int solve_once(int n)`)。
- Produces: 每个 body 仍导出 `EIGEN_SVE_BODY_NS::<pfx>_vl<N>::solve_once(int)` 同名同签名符号(dispatcher 的 `pfx##_solve_once` 宏分派零改动)。

**Steps:**

- [ ] **Step 1: 基线构建 + 抓基线 objdump**(修改前,取证旧指令序列)

```bash
ls builddir >/dev/null 2>&1 || PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
./scripts/build.sh 2>&1 | tail -20   # 基线:记录警告数(期望预存量数,零新增基准)
mkdir -p /tmp/objdump_old
for P in gemm_f64 gemm_f32 gemm_c128; do
  for T in 128 256 512 1024; do
    L=$(ls builddir/tests/cpu/libtests_eigen_${P}_vl${T}.a 2>/dev/null || \
        find builddir -name "libtests_eigen_${P}_vl${T}.a" | head -1)
    [ -n "$L" ] && objdump -d "$L" > /tmp/objdump_old/${P}_vl${T}.txt
  done
done
ls /tmp/objdump_old/ | wc -l   # 期望 12
```

(库名/路径若与预期不符,以 `find builddir -name "*eigen_gemm*vl*"` 实测为准,如实记录。)

- [ ] **Step 2: 编辑 `gemm_f64_sve_body.cpp`** — 三处 Edit:

Edit 2a(include 増补):
```
old: #include <sandstone.h>
     #include "sdc_stress_config.h"
     (blank)
     #include <Eigen/Core>
new: #include <sandstone.h>
     #include "sdc_stress_config.h"
     #include "gemm_solve_probe.h"
     (blank)
     #include <Eigen/Core>
```

Edit 2b(头注释,` * memcmp_or_fail;T10 bad_alloc 窄 catch 边界契约);solve_once 谱系` 起的 3 行):
```
old:  * memcmp_or_fail;T10 bad_alloc 窄 catch 边界契约);solve_once 谱系
      * 契约(检视 #51)同 gemm_double_dynamic_square.cpp 的 gemm_solve_once
      * (splitmix 确定性哈希,与 fill_rand 同分布)。服务
new:  * memcmp_or_fail;T10 bad_alloc 窄 catch 边界契约);solve_once 一行
      * 转发共享模板 gemm_solve_probe.h(#392 收口;谱系魔数/值域单源
      * eigen_splitmix_lineage.h,契约(检视 #51)不再依赖本 TU 手工
      * 镜像)。服务
```

Edit 2c(探针体整体替换,函数体 109-125 行):
```
old: int solve_once(int dim)
     {
         /* splitmix 同谱系探针(镜像 gemm_double_dynamic_square.cpp 的
          * gemm_solve_once):与 fill_rand 同分布、不依赖 per-thread RNG。 */
         Mat a(dim, dim), b(dim, dim), c;
         for (int r = 0; r < dim; ++r)
             for (int col = 0; col < dim; ++col) {
                 std::uint64_t h = (std::uint64_t)r * 0x9E3779B97F4A7C15ull
                                 ^ (std::uint64_t)col * 0xC2B2AE3D27D4EB4Full;
                 h ^= h >> 32;
                 h *= 0xD6E8FEB86659FD93ull;
                 h ^= h >> 32;
                 std::uint64_t h2 = h * 0xD6E8FEB86659FD93ull
                                  + 0x9E3779B97F4A7C15ull;
                 h2 ^= h2 >> 32;
                 a(r, col) = (double)(h % 2000u) / 1000.0 - 1.0;
                 b(r, col) = (double)(h2 % 2000u) / 1000.0 - 1.0;
             }
         c = a * b;
         return 0;
     }
new: int solve_once(int dim)
     {
         /* 联合求解 sizing 探针: 一行转发共享模板, 同
          * gemm_double_max_arm_body.cpp 的转发范式(#392 收口口径)——
          * splitmix 填充谱系魔数/值域单源 eigen_splitmix_lineage.h,
          * 谱系契约(检视 #51)不再依赖本 TU 手工镜像保持同步。 */
         return gemm_probe::solve_once<Mat>(dim);
     }
```

- [ ] **Step 3: 编辑 `gemm_f32_sve_body.cpp`** — 同型三处(include 15-17 行、头注释 8-11 行含 f64 中间量措辞、探针体 108-126 行,old 文本以 f32 变体逐字为准:注释为「f32 赋值转型经 f64 中间量(原用例同款转型)」,填充两行为 `(float)((double)(h % 2000u) / 1000.0 - 1.0)`)。

- [ ] **Step 4: 编辑 `gemm_c128_sve_body.cpp`** — 同型三处(include 17-19 行、头注释 11-13 行(re/im 四流措辞)、探针体 116-141 行,old 文本以 c128 变体逐字为准:h3/h4 四流 + 两个 std::complex<double> 构造)。

- [ ] **Step 5: `gemm_solve_probe.h` 头注释增补**(在「措辞勘误(检视 #392-R9)」段末尾、` */` 之前追加):
```
new(追加段):
 * 使用方谱系增补(issue #615, 2026-11-13): 本头消费方现为 7 个 T9
 * wrapper + 4 个 per-tier SVE body——gemm_double_max_arm_body 之外,
 * gemm_{f64,f32,c128}_sve_body 三份逐字 splitmix 探针副本(4a6c7acf
 * 引入, 先于 PR #392 合并, 漏入当时收口清单)一并收编为一行转发;
 * SVE body 的 meson 门控同 eigen_sve_body_specs 表(仅
 * host cpu_family=='aarch64', 无 OS 谓词), 一行转发无 #else 回退,
 * 「守门不附 __linux__」的理据由 4 个 body 共同承载。
```

- [ ] **Step 6: 构建 + 零新警告 + 符号 + objdump 新旧对比**

```bash
./scripts/build.sh 2>&1 | tail -20        # 期望: 成功, 无新警告(对照 Step 1 基线)
mkdir -p /tmp/objdump_new
for P in gemm_f64 gemm_f32 gemm_c128; do
  for T in 128 256 512 1024; do
    L=$(find builddir -name "libtests_eigen_${P}_vl${T}.a" | head -1)
    objdump -d "$L" > /tmp/objdump_new/${P}_vl${T}.txt
    nm "$L" | grep -c "solve_once"      # 期望 ≥1(符号在位)
  done
done
for f in /tmp/objdump_new/*.txt; do
  diff -q /tmp/objdump_old/$(basename $f) $f >/dev/null 2>&1 \
    && echo "IDENTICAL $(basename $f)" \
    || echo "DIFFERS $(basename $f) —— 记录 diff 摘要(预期仅寄存器分配差, 如 #392 先例)"
done
```

- [ ] **Step 7: 受影响行为 + 回归 + SKIP 路径(本机真跑)**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -3   # 期望 exit: pass
for T in eigen_gemm_double_dynamic_square_sve eigen_gemm_double14_sve \
         eigen_gemm_float_dynamic_square_sve eigen_gemm_cdouble_dynamic_square_sve; do
  ./builddir/sdcshield -e $T -t 2000 -n 1 2>&1 | grep -i "skip" | head -2
done
# 期望: 4 个测试 SKIP 且理由与修改前逐字一致(无 SVE 硬件;SKIP 是诚实路径,
# 真实档位证据在 Task 4 gem5)。修改前先跑一遍留存 SKIP 原文以对比。
```

- [ ] **Step 8: x86-64 非回归(按检视声明)**:3 个 body 均为 `if host_machine.cpu_family() == 'aarch64'` meson 门控专属 TU(x86-64 从不编译);`gemm_solve_probe.h` 仅注释增补零逻辑变化;harness 是新增验证工具。结论:纯 aarch64 内部等价重构,x86-64 零触及(如实声明,非验证豁免)。

- [ ] **Step 9: commit 修复单元**

```bash
git add tests/cpu/eigen_gemm/gemm_f64_sve_body.cpp \
        tests/cpu/eigen_gemm/gemm_f32_sve_body.cpp \
        tests/cpu/eigen_gemm/gemm_c128_sve_body.cpp \
        tests/cpu/eigen_gemm/gemm_solve_probe.h
git commit -s -m "refactor(eigen-gemm): 3 份 SVE body sizing 探针逐字副本收口至 gemm_solve_probe.h (issue #615)

gemm_{f64,f32,c128}_sve_body.cpp 各含一份手写 splitmix 魔法数探针副本
(4a6c7acf 引入, 先于 PR #392 合并, 漏入当时收口清单; dispatcher 经
sdc_joint_dim_slice 逐档活体消费)。按 gemm_double_max_arm_body.cpp 的
一行转发范式收编, 谱系魔数/值域回归单源 eigen_splitmix_lineage.h——
issue #615 指控的『一处演化漏其余』漂移面在现役 main 就此清零。

验证: 双 kernel byte-identical 21/21(f64/f32/c128 × n∈{1,2,3,5,16,17,33}
× a/b/c, native NEON 腿);SVE 四档 gem5 SE 见后续证据;构建零新警告;
zstd19 -t 2000 -n 1 exit: pass;4 受影响测试本机 SKIP 理由逐字不变;
x86-64 零触及(aarch64 专属 TU)。"
git push
```

---

### Task 4: gem5-fi 四档真跑 + 勾选 + 证据 commit

**Files:**
- Modify: `docs/superpowers/plans/2026-11-13-gemm-sve-probe-residual-collapse.md`(勾选本计划复选框)

**Interfaces:**
- Consumes: Task 2 harness 的四档编译产物;`scripts/eigen-sve-double/gem5/run_sve.sh`(自记 G5 md5)。

**Steps:**

- [ ] **Step 1: 四档编译 harness**

```bash
mkdir -p /tmp/gem5_bins
for T in 128 256 512 1024; do
  g++ -O2 -std=c++17 -I third-party/eigen5 -march=armv8.2-a+sve \
      -msve-vector-bits=$T -DEIGEN_ARM64_USE_SVE \
      scripts/eigen-sve-double/test_gemm_probe_lineage.cpp \
      -o /tmp/gem5_bins/gemm_probe_lineage_$T
done
ls /tmp/gem5_bins/    # 期望 4 个二进制
```

- [ ] **Step 2: gem5 SE 逐档真跑**

```bash
for QW in 1 2 4 8; do
  T=$((QW * 128))
  scripts/eigen-sve-double/gem5/run_sve.sh /tmp/gem5_bins/gemm_probe_lineage_$T --vl $QW \
    2>&1 | tee /tmp/gem5_lineage_vl$T.log | grep -E "VL identity|RESULT|Simulated exit"
done
```

Expected(每档):`VL identity: svcntb=<16/32/64/128> bytes (<128/256/512/1024>-bit tier)`、`RESULT: ALL PASS (21/21 …)`、`Simulated exit code: 0`。run_sve.sh 会回显 `G5=… md5=5101f2a8…`——引用时保留。

- [ ] **Step 3: 勾选本计划全部复选框 + commit**

```bash
# 将本文件中所有 "- [ ]" 改为 "- [x]"
git add docs/superpowers/plans/2026-11-13-gemm-sve-probe-residual-collapse.md
git commit -s -m "docs(plan): 勾选完成——#615 残余收口验证全绿

gem5 SE 四档(VL 128/256/512/1024)双 kernel ALL PASS 21/21 ×4,
Simulated exit code: 0 ×4;native NEON 腿 ALL PASS;构建零新警告;
zstd19 回归 pass;4 受影响测试 SKIP 理由逐字不变。"
git push
```

---

### Task 5: 开 PR + issue #615 处置报告(无 commit)

**Steps:**

- [ ] **Step 1: 开 PR**(若 auto-pr.yml 抢先导致 create 失败,按契约兜底 `gh pr edit fix/issue-615 --title … --body …`)

```bash
gh pr create --base main --head fix/issue-615 \
  --title "fix: 3 份 SVE GEMM body 逐字探针副本收口至 gemm_solve_probe.h (#615)" \
  --body "<Closes #615 + 完整处置报告: 处置判定(指控1已修/指控2边界/残存缺口根因)、
改动点、验证证据(native 21/21 + gem5 四档输出引用 + 构建/回归/SKIP/x86 声明)>"
```

- [ ] **Step 2: issue #615 贴处置报告**(内容 = PR 正文同源 + 本处置判定细节与时间线;不 resolve,关闭随人工 merge)。

- [ ] **Step 3: 工作区收尾**

```bash
git checkout main && git status    # 期望 clean
```

---

## Self-Review 结论(计划自检)

1. **Spec 覆盖**: issue 两项指控均有处置(指控 1 不重做 + 残余 3 副本修复;指控 2 边界裁定不重开,报告如实说明);gem5-fi 规则四档覆盖;rule 2 四要素(构建/行为/回归/x86)全落位。
2. **占位符扫描**: 无 TBD/“类似上文”——三 body 的 Edit old 文本全部以仓库实测逐字文本给出(f32/c128 变体注明以其自身逐字为准,执行者 Read 后逐字替换,与 Edit 工具语义一致)。
3. **类型一致性**: `gemm_probe::solve_once<Mat>`/`fill_pair<Mat>` 签名、`pfx##_vl<N>::solve_once(int)` 符号契约、harness MatD/MatF/MatC 与 body typedef 一致。

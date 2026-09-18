# Eigen SVE double/complex<double> packet 后端实现计划（SVE-256 本机 + SVE-512 gem5 验证）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 给 vendored Eigen 5.0 的 SVE packet 后端补上 `double` 与 `std::complex<double>` 支持，使 `eigen_svd_cdouble_sve` 恢复为真实的 SVE 向量化压测；SVE-256（本机 cortex x3b）真实运行验证，SVE-512（本机硬件没有）在 gem5 SE 模式仿真验证。

**Architecture:** Eigen 的 packet 后端是纯头文件特化集合（`arch/SVE/PacketMath.h` 等），只需按 float 的既有模式增加 `PacketXd`（`svfloat64_t`）与 `PacketXcd`（复数，交错存储 + `svcmla` 乘法）两组特化，加上 `Complex.h` 的复数 packet 类与运算。全部代码通过 `EIGEN_ARM64_SVE_VL`（编译期固定 VL，来自 `-msve-vector-bits=N`）参数化——同一份源码在 128/256/512-bit VL 下都正确。本机（256-bit）用真实硬件验证正确性与性能；512-bit 无法在本机运行（硬件 VL=256），改在 gem5 SE 模式（`sve_vl_se` 参数设 512-bit，`FEAT_SVE`/`FEAT_F64MM` 在 `ArmDefaultSERelease` 中已默认启用）运行同一套二进制验证。

**Tech Stack:** C++20（`arm_sve.h` ACLE intrinsics）、Eigen 5.0 内部 packet API（`Eigen::internal::p*` 家族）、gem5 v25.1.0.1（`/home/sdc/wangxu/gem5-fi-fuzz/CHAOS/gem5/`，需先构建 `build/ARM/gem5.opt`）、GCC 12.3.1（已验证 `-msve-vector-bits=512` 可编译 size-specific 代码）。

**Spec:** 本计划自身为 spec（前一个调查计划 `2026-09-18-fix-svd-cdouble-sve-mdim-timeout.md` 记录了缺口事实：`arch/SVE/PacketMath.h` 仅有 int32/float 特化，`packet_traits<double>::size == 1` 标量回退，300×300 double BDCSVD 29ms NEON vs >10min "SVE"）。

## 实测确认的关键技术事实（2026-09-18，全部本机实测，计划据此设计）

1. **本机硬件**：cortex x3b，SVE VL=256-bit（`svcntd()`=4 lanes f64），`HWCAP` 含 `sve`/`svef64mm`/`svebf16`。
2. **`svcmla_f64_m` 语义**（本机逐旋转角实测解码，probe7/probe9）：
   ```
   rot0:   Zd[e] += X[e]*Y[e]   Zd[o] += X[e]*Y[o]
   rot90:  Zd[e] -= X[o]*Y[o]   Zd[o] += X[o]*Y[e]
   rot180: Zd[e] -= X[e]*Y[e]   Zd[o] -= X[e]*Y[o]
   rot270: Zd[e] += X[o]*Y[o]   Zd[o] -= X[o]*Y[e]
   ```
   **复数乘法配方（实测精确命中）**：`t = svcmla(v0, X, Y, 0); t = svcmla(t, X, Y, 90)` ⟹ `(xe·ye − xo·yo, xe·yo + xo·ye)` = `(a·b)`（X=(3,4), Y=(1,-2) ⟹ (11,-2) 精确匹配）。
3. **conj**：`svneg_f64_x(pg_odd, v)`（`pg_odd = svzip1_b64(svpfalse_b(), svptrue_b64())`）翻转虚部 lane，实测 (3,4)→(3,-4)；conj 后 pmul = conj(X)*Y 实测 (-5,-10) 精确。
4. **`-msve-vector-bits=512` 在本机 GCC 12.3.1 编译通过**（生成 size-specific SVE 代码，本机 VL=256 不能运行——这正是需要 gem5 的原因）。
5. **gem5 SVE 支持**：`src/arch/arm/ArmISA.py:53` `ArmDefaultSERelease` 默认含 `FEAT_SVE`+`FEAT_F64MM`；SE 模式 VL 由 `ArmISA.py:192` `sve_vl_se`（quadword 数，512-bit=4）控制；`starter_se.py` 是现成 SE 配置模板；CHAOS gem5 树未构建过（无 build/ 产物），需 `scons build/ARM/gem5.opt`（原生 aarch64，无需交叉；progress.md 实测 `-j16` 上限防 OOM，本机 61GB 内存可试 `-j32`）。
6. **Eigen 复数 packet 模式**（NEON `Packet1cd` 先例）：struct 包一个 real packet，`size = complex 个数`，交错存储（re,im,re,im...），pand/pxor 用 real packet 位运算。SVE 版 `PacketXcd` 的 real 载体是 `PacketXd`（VL/128 个 complex）。
7. **测试基建**：vendored eigen5 有完整 `test/`（`packetmath.cpp` 等）但无构建系统接线；更直接的验证是自写最小 standalone 测试程序（`/tmp/eigen_sve_test/svd_test.cpp` 先例：29ms NEON vs SVE 超时）——每个 task 用 standalone 程序 + SDCShield 测试本体双重验证。
8. **`-msve-vector-bits=128` 现状**：`tests/cpu/meson.build:916` SVE 测试编译用 128-bit；本机 256 硬件可跑（size-specific ≤ 硬件 VL 即可执行）。完整验证谱系 128（本机+gem5）/256（本机+gem5）/512（仅 gem5）。

## Global Constraints

- **one-patch-per-unit**：每个 Task 一个 commit；全部在 `feat/eigen-sve-double-packets` 分支（从当前 `fix/svd-cdouble-sve-mdim-timeout` 分支切出，包含占位 skip 的前置修复）。
- **x86-64 零改动**：所有 Eigen 改动在 `arch/SVE/` 新文件/新段落 + `Eigen/Core` 的 SVE include 块内（已有 `#elif defined EIGEN_VECTORIZE_SVE` 守卫）。
- **NEON 路径零改动**：`arch/NEON/` 不动；SVE 特化只在 `EIGEN_VECTORIZE_SVE` 下编译。
- **上游对齐**：写法对齐 Eigen 上游（gitlab.com/libeigen/eigen）`arch/SVE/` 已有 int32/float 的风格；double 特化按 float 段落模式平移。若上游已有 double 实现可参考其提交，但**不得整块拷贝**（许可 MPL-2.0 与本仓 vendored 一致，允许）。
- **每步验证 100% 真实**：引用真实命令输出；gem5 验证必须引用仿真 stdout 的 `Simulated exit code`/golden hash 行。
- **性能闸门**：`eigen_svd_cdouble_sve`（M_DIM=300）在 256-bit SVE 下单线程一次迭代 **≤ 60s**（NEON 实测 29ms 300×300 real BDCSVD；complex 4× 计算量 + 2100→300 已回退，60s 是保守上限）。不达标 = 特化有热路径仍走标量，不许合入。
- **正确性闸门**：standalone 测试对每个实现的 packet op 做 vs 标量逐位比对（bitwise，含负零/NaN 掩码处理跟随 `sandstone_data.cpp` 的 SNaN 静默惯例——这里是我们自己的测试程序，直接比对非 NaN 值 + NaN 位模式）。
- **gem5 时间预算**：gem5.opt 构建 ~1-2h（-j32）；SE 仿真 SVD 测试每次 ~10-60min（atomic CPU 慢，允许只跑缩小的迭代数，但必须跑完整 packet op 正确性测试全集）。
- **诚实原则**：512-bit 在 gem5 上验证的是**功能正确性**（ISA 语义执行正确），不是性能（gem5 不是周期精确的性能预测工具用于此目的）；报告里明确区分。

---

### Task 0: 切分支 + 快照基线

**Files:**
- 无源码改动；创建 `scripts/eigen-sve-double/` 工作目录（git 跟踪）放 standalone 测试程序。

**Interfaces:**
- Produces: 分支 `feat/eigen-sve-double-packets`；目录 `scripts/eigen-sve-double/`（后续 task 的测试程序都放这里，随代码一起 commit，作为可复现的验证资产）。

- [ ] **Step 1: 从 fix 分支切出 feature 分支**

```bash
cd /home/sdc/root-xupeng/sdcshield
git checkout fix/svd-cdouble-sve-mdim-timeout && git pull
git checkout -b feat/eigen-sve-double-packets
mkdir -p scripts/eigen-sve-double
```

- [ ] **Step 2: 记录基线（修复占位 skip 在位，全量绿）**

```bash
./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble_sve -n 1 2>&1 | grep -E "result|skip-category"
# 期望: result: skip / skip-category: TestResourceIssue（占位 skip 仍在）
```

- [ ] **Step 3: Commit 空目录占位（.gitkeep）**

```bash
touch scripts/eigen-sve-double/.gitkeep
git add scripts/eigen-sve-double && git commit -m "chore: scaffold scripts/eigen-sve-double workspace (feat branch)"
```

---

### Task 1: `PacketXd` 基础算术 packet（traits/pload/pstore/算术/比较/redux）

**Files:**
- Modify: `third-party/eigen5/Eigen/src/Core/arch/SVE/PacketMath.h`（float 段落后追加 double 段落）
- Create: `scripts/eigen-sve-double/test_packet_xd.cpp`（standalone 正确性测试）

**Interfaces:**
- Consumes: `EIGEN_ARM64_SVE_VL`（编译期 VL 宏）、`default_packet_traits`（GenericPacketMath.h）、既有 `PacketXi`（f64 的 integer_packet 用不到，跳过）。
- Produces: `typedef svfloat64_t PacketXd`；`packet_traits<double>`（`size = EIGEN_ARM64_SVE_VL/64`）；`unpacket_traits<PacketXd>`；以下 op 的完整特化（后续 Task 依赖的确切签名）：
  - `PacketXd pset1<PacketXd>(const double&)`
  - `PacketXd padd/psub/pmul/pdiv<PacketXd>(const PacketXd&, const PacketXd&)`
  - `PacketXd pmadd(const PacketXd& a, const PacketXd& b, const PacketXd& c)`（= `svmla_f64_x(pg, c, a, b)`）
  - `PacketXd pnegate/pconj/pabs(const PacketXd&)`
  - `PacketXd pmin/pmax<PacketXd>` + `PropagateNaN`/`PropagateNumbers` 变体（`svminnm/svmaxnm`）
  - `PacketXd pcmp_le/pcmp_lt/pcmp_eq/pcmp_lt_or_nan<PacketXd>`（返回模式对齐 float：`svdup_n_u64_z(pred, ~0ull)` reinterpret）
  - `PacketXd ptrue/pzero(const PacketXd&)`、`pand/por/pxor/pandnot`（u64 reinterpret）
  - `PacketXd pload/ploadu<PacketXd>(const double*)`、`pstore/pstoreu<double>(double*, const PacketXd&)`
  - `PacketXd ploaddup/ploadquad<PacketXd>(const double*)`（`svzip1_u64` 索引模式照抄 float 的 `svzip1_u32` 改位宽）
  - `PacketXd pgather<double, PacketXd>(const double*, Index)`、`pscatter<double, PacketXd>`
  - `double pfirst<PacketXd>(const PacketXd&)`（`svlasta_f64(svpfalse_b(), a)`）
  - `PacketXd preverse<PacketXd>`（`svrev_f64`）
  - `double predux/predux_mul/predux_min/predux_max<PacketXd>`（redux_mul 按 float 的 svtbl 减半链，步长改 u64）
  - `PacketXi`-风格 `ptranspose`（`PacketBlock<PacketXd, N>`，buffer 机制照抄 float）
  - `PacketXd pfloor/pceil/print<PacketXd>`（`svrintm/svrintp/svrinta_f64_x`，本机实测语义正确）
  - `PacketXd psqrt<PacketXd>`（`svsqrt_f64_x`）
  - `PacketXd pfloor`... （见上）；`plset<PacketXd>(const double&)`
  - `packet_traits<double>` 标志位：`HasAdd/Sub/Mul/Div/Negate/Abs/Abs2/Min/Max/Conj=1, HasBlend=0, HasReduxp=0, HasSqrt=1, HasRint=1`（不抄 float 的 `HasSin/Cos/Log/Exp/Pow` —— MathFunctions 的 double 版 Task 3 再开）

**设计要点（写代码时必须遵守）：**
- 所有 `svptrue_b32()` 在 f64 op 里换成 `svptrue_b64()`。
- `packet_traits<double>::size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size`（=VL/64）。
- **关键陷阱——`svld1_f64(pg, from)` 的 pg 必须是全 1 谓词**：Eigen 的 pload 假定装满 packet；size-specific 模式下 `svptrue_b64()` 恰好激活全部 VL/64 lane。
- `ploaddup`：float 版先 `svindex_u32(0,1)` 再 `svzip1_u32`；double 用 `svindex_u64(0,1)` + `svzip1_u64`，gather 用 `svld1_gather_u64index_f64`。
- `predux_mul` 的静态断言与减半链照抄 float（`EIGEN_ARM64_SVE_VL % 128 == 0`），`svtbl_f64` + `svindex_u64`。

- [ ] **Step 1: 写失败测试 `scripts/eigen-sve-double/test_packet_xd.cpp`**

```cpp
// Standalone correctness test for Eigen SVE PacketXd (double).
// Build at VL=256 (host) — the same source must also compile at VL=128/512
// (compile-only check on host; run on gem5 for 512).
// Each op is compared bitwise against a scalar reference loop.
#include <Eigen/Core>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

using namespace Eigen;
using namespace Eigen::internal;

static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else printf("ok: %s\n", what);
}

static bool bits_eq(double a, double b) {
  // NaN compare: both NaN passes; else bitwise (catches -0.0 vs +0.0)
  if (std::isnan(a) && std::isnan(b)) return true;
  uint64_t x, y; memcpy(&x, &a, 8); memcpy(&y, &b, 8);
  return x == y;
}

int main() {
  const int N = packet_traits<double>::size;
  printf("PacketXd: size=%d (VL=%d bits) backend=%s\n",
         N, EIGEN_ARM64_SVE_VL, Eigen::SimdInstructionSetsInUse());
  check(N == EIGEN_ARM64_SVE_VL / 64, "packet size == VL/64");
  check((int)unpacket_traits<PacketXd>::size == N, "unpacket size");

  alignas(64) double a[64], b[64], r[64], ref[64];
  for (int i = 0; i < 64; i++) { a[i] = -3.0 + i * 0.5; b[i] = 2.0 - i * 0.25; }
  // exercise a few exact-power-of-two / negative / denormal-free values

  // --- pset1 / pload / pstore round trip
  PacketXd v = pset1<PacketXd>(7.5);
  pstore<double>(r, v);
  for (int i = 0; i < N; i++) check(r[i] == 7.5, "pset1/pstore");

  PacketXd va = pload<PacketXd>(a), vb = pload<PacketXd>(b);
  pstore<double>(r, va); check(memcmp(r, a, N*8) == 0, "pload/pstore aligned");
  PacketXd vu = ploadu<PacketXd>(a + 1); pstoreu<double>(r, vu);
  check(memcmp(r, a + 1, N*8) == 0, "ploadu/pstoreu");

  // --- arithmetic vs scalar reference
  #define OP1_TEST(NAME, EXPR_SCALAR) do { \
    pstore<double>(r, NAME(va)); \
    for (int i = 0; i < N; i++) ref[i] = EXPR_SCALAR; \
    for (int i = 0; i < N; i++) check(bits_eq(r[i], ref[i]), #NAME); \
  } while (0)
  #define OP2_TEST(NAME, EXPR_SCALAR) do { \
    pstore<double>(r, NAME(va, vb)); \
    for (int i = 0; i < N; i++) ref[i] = EXPR_SCALAR; \
    for (int i = 0; i < N; i++) check(bits_eq(r[i], ref[i]), #NAME); \
  } while (0)

  OP2_TEST(padd, a[i] + b[i]);
  OP2_TEST(psub, a[i] - b[i]);
  OP2_TEST(pmul, a[i] * b[i]);
  OP2_TEST(pdiv, a[i] / b[i]);
  OP2_TEST(pmin, a[i] < b[i] ? a[i] : b[i]);
  OP2_TEST(pmax, a[i] > b[i] ? a[i] : b[i]);
  OP1_TEST(pnegate, -a[i]);
  OP1_TEST(pabs, std::abs(a[i]));
  OP1_TEST(pfloor, std::floor(a[i]));
  OP1_TEST(pceil, std::ceil(a[i]));

  // pmadd: c + a*b
  PacketXd vc = pset1<PacketXd>(0.25);
  pstore<double>(r, pmadd(va, vb, vc));
  for (int i = 0; i < N; i++) check(bits_eq(r[i], 0.25 + a[i]*b[i]), "pmadd");

  // --- comparisons: all-lanes-true / all-lanes-false patterns
  PacketXd ones = pset1<PacketXd>(1.0), zeros = pzero(ones);
  PacketXd eq = pcmp_eq(va, va);
  pstore<double>(r, eq);
  for (int i = 0; i < N; i++) check(r[i] != 0.0, "pcmp_eq self");
  PacketXd ne = pcmp_eq(ones, zeros);
  pstore<double>(r, ne);
  for (int i = 0; i < N; i++) check(r[i] == 0.0, "pcmp_eq diff");

  // --- redux
  double s = 0; for (int i = 0; i < N; i++) s += a[i];
  check(bits_eq(predux(va), s), "predux");
  double mx = a[0]; for (int i = 0; i < N; i++) mx = mx > a[i] ? mx : a[i];
  check(bits_eq(predux_max(va), mx), "predux_max");
  double mn = a[0]; for (int i = 0; i < N; i++) mn = mn < a[i] ? mn : a[i];
  check(bits_eq(predux_min(va), mn), "predux_min");

  // --- pfirst / preverse / ploaddup
  check(pfirst(va) == a[0], "pfirst");
  PacketXd rv = preverse(va); pstore<double>(r, rv);
  for (int i = 0; i < N; i++) check(r[i] == a[N-1-i], "preverse");
  PacketXd dd = ploaddup<PacketXd>(a); pstore<double>(r, dd);
  for (int i = 0; i < N/2; i++) { check(r[2*i] == a[i] && r[2*i+1] == a[i], "ploaddup"); }

  // --- gather/scatter with stride 3
  double g[300]; for (int i = 0; i < 300; i++) g[i] = i * 1.5;
  PacketXd pg_ = pgather<double, PacketXd>(g, 3); pstore<double>(r, pg_);
  for (int i = 0; i < N; i++) check(r[i] == g[3*i], "pgather");
  double sc[300] = {0};
  pscatter<double, PacketXd>(sc, va, 2);
  for (int i = 0; i < N; i++) check(sc[2*i] == a[i], "pscatter");

  // --- plset
  PacketXd ls = plset<PacketXd>(10.0); pstore<double>(r, ls);
  for (int i = 0; i < N; i++) check(r[i] == 10.0 + i, "plset");

  if (failures == 0) { printf("ALL PASS (%d lanes)\n", N); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}
```

- [ ] **Step 2: 编译验证测试编译失败（double 特化缺失 → 链接/编译错误或 size=1 假通过）**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=256 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xd.cpp -o /tmp/test_packet_xd_256 && /tmp/test_packet_xd_256
```
Expected: **FAIL**（`packet size == VL/64` 打印 size=1，N=1，多数 op 走标量默认实现可能"通过"但 size 检查失败；或编译错误因为 pload<PacketXd> 无特化）。

- [ ] **Step 3: 实现 PacketXd 段落（`arch/SVE/PacketMath.h` float 段落 `}` 后追加）**

代码骨架——每个 op 都给出（写入 PacketMath.h `/***************************** float64 ************************************/` 注释段；完整代码遵循 float 段落一对一平移，位宽 32→64）：

```cpp
/***************************** float64 ************************************/

typedef svfloat64_t PacketXd __attribute__((arm_sve_vector_bits(EIGEN_ARM64_SVE_VL)));

template <>
struct packet_traits<double> : default_packet_traits {
  typedef PacketXd type;
  typedef PacketXd half;

  enum {
    Vectorizable = 1,
    AlignedOnScalar = 1,
    size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size,

    HasAdd = 1,
    HasSub = 1,
    HasShift = 1,
    HasMul = 1,
    HasNegate = 1,
    HasAbs = 1,
    HasArg = 0,
    HasAbs2 = 1,
    HasMin = 1,
    HasMax = 1,
    HasConj = 1,
    HasSetLinear = 0,
    HasBlend = 0,
    HasReduxp = 0,  // Not implemented in SVE

    HasDiv = 1,

    HasCmp = 1,
    HasSqrt = 1,
    HasRint = 1
  };
};

template <>
struct unpacket_traits<PacketXd> {
  typedef double type;
  typedef PacketXd half;  // Half not implemented yet
  typedef PacketXi integer_packet;  // 64-bit gather indices need int64; see pgather below

  enum {
    size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size,
    alignment = Aligned64,
    vectorizable = true,
    masked_load_available = false,
    masked_store_available = false
  };
};
```

其余 op 全部按本 Task "Interfaces" 清单实现。**注意 `pgather` 索引类型**：float 版用 `svindex_s32`（32-bit 索引），double 大矩阵 stride 寻址需要 64-bit 索引——用 `svindex_u64(0, stride)` + `svld1_gather_u64index_f64`（`pscaller` 同理 `svst1_scatter_u64index_f64`）。`integer_packet` 保持 `PacketXi`（Eigen 用它做 int 转换，与 gather 无关）。

**`pabsdiff`/`ptrue` 等没在清单里的 op 不实现**（default_packet_traits 里为 0 的能力 Eigen 自动走标量，符合渐进策略）。

- [ ] **Step 4: 256-bit 本机运行测试通过**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=256 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xd.cpp -o /tmp/test_packet_xd_256 && /tmp/test_packet_xd_256
```
Expected: `ALL PASS (4 lanes)`。

- [ ] **Step 5: 128-bit 编译+本机运行（VL=128 ≤ 硬件 256 可跑）**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=128 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xd.cpp -o /tmp/test_packet_xd_128 && /tmp/test_packet_xd_128
```
Expected: `ALL PASS (2 lanes)`。

- [ ] **Step 6: 512-bit 编译检查（本机不能运行，只验证编译）**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=512 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xd.cpp -o /tmp/test_packet_xd_512
echo "512-bit compile rc=$? (run deferred to gem5 in Task 6)"
```
Expected: 编译通过（size-specific 代码合法，硬件 VL=256 不够执行）。

- [ ] **Step 7: Commit**

```bash
git add third-party/eigen5/Eigen/src/Core/arch/SVE/PacketMath.h scripts/eigen-sve-double/test_packet_xd.cpp
git commit -m "eigen(SVE): add PacketXd double packet arithmetic backend

packet_traits<double> + full arithmetic/load/store/redux specialization
for svfloat64_t at any fixed -msve-vector-bits=VL. Verified bitwise vs
scalar reference at VL=128 and VL=256 on cortex x3b (test_packet_xd
ALL PASS 2/4 lanes); VL=512 compile-checked (execution deferred to
gem5 SE in a later task)."
```

---

### Task 2: `PacketXcd` complex<double> packet（含 svcmla 乘法配方）

**Files:**
- Create: `third-party/eigen5/Eigen/src/Core/arch/SVE/Complex.h`（新文件，对齐 `arch/NEON/Complex.h` 结构）
- Modify: `third-party/eigen5/Eigen/Core:269-272`（SVE include 块加一行）
- Create: `scripts/eigen-sve-double/test_packet_xcd.cpp`

**Interfaces:**
- Consumes: Task 1 的 `PacketXd` 全部 op（pload/pstore/padd/psub/pmul 位运算/reinterpret）。
- Produces: `struct PacketXcd { PacketXd v; }`；`packet_traits<std::complex<double>>`（`size = EIGEN_ARM64_SVE_VL/128`，即每个 complex 占 2 个 f64 lane）；以下特化（Eigen Complex 协议）：
  - `PacketXcd pload/ploadu<PacketXcd>(const std::complex<double>*)`（reinterpret 到 `const double*`，复用 `pload<PacketXd>`；`assume_aligned` 模式照 NEON Packet1cd）
  - `pstore/pstoreu<std::complex<double>>(std::complex<double>*, const PacketXcd&)`
  - `PacketXcd pset1<PacketXcd>(const std::complex<double>&)`
  - `PacketXcd padd/psub<PacketXcd>`
  - `PacketXcd pmul<PacketXcd>(const PacketXcd& a, const PacketXcd& b)` — **svcmla rot0+rot90 配方**：
    ```cpp
    template <>
    EIGEN_STRONG_INLINE PacketXcd pmul<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
      // complex multiply via SVCMLA: rot0 accumulates xe*ye / xe*yo,
      // rot90 adds -xo*yo / +xo*ye  (semantics probe-verified 2026-09-18)
      svfloat64_t t = svcmla_f64_m(svptrue_b64(), pzero<PacketXd>(a.v), a.v, b.v, 0);
      t = svcmla_f64_m(svptrue_b64(), t, a.v, b.v, 90);
      return PacketXcd(t);
    }
    ```
  - `PacketXcd pmadd<PacketXcd>(a, b, c)` = `pmul(a,b)` 再 `padd` 到 c（或直接 `svcmla` 双步加 c——用双 svcmla 以 c.v 为初始累加器更优，NEON `__ARM_FEATURE_COMPLEX` 先例 `vcmlaq`+`vcmlaq_rot90`）
  - `PacketXcd pnegate`、`pconj`（odd-lane 谓词翻转：`svneg_f64_x(pg_odd, ...)`，`pg_odd = svzip1_b64(svpfalse_b(), svptrue_b64())`，模块级 static 常量）
  - `PacketXcd pcmp_eq`（real/imag 全等：`svand_b64` 两个 `svcmpeq_f64` 谓词后 `svdup_n_u64_z`）
  - `pand/por/pxor/pandnot`（real packet 位运算直通）
  - `std::complex<double> pfirst<PacketXcd>`（`svlastb`/lane0+lane1 提取或 pstore 到 2 元素数组）
  - `PacketXcd preverse`（complex 粒度反转 = 交换 complex 对，非 lane 反转：`svtbl` 索引 `(i^1)` 交换每对内 re/im 顺序后还需 complex 序反转——**照 NEON Packet2cf 的 `vrev64q` 复合模式**：先 pair-swap（`idx = sveor(index,1)`，本机实测 20 10 40 30 模式）得到 complex 内 re/im 互换，再用 `svrevw`? 不对——**实现时直接用 svtbl 组合索引**：目标 lane j 的源 lane = `(N-1-j)^1`（complex 反转+对内交换），单条 `svtbl_f64` 完成）
  - `ploaddup<PacketXcd>`（每个 complex 连续复制两份：`svzip1` 在 complex 粒度）
- Modify: `Eigen/Core` SVE 块：
  ```cpp
  #elif defined EIGEN_VECTORIZE_SVE
  #include "src/Core/arch/SVE/PacketMath.h"
  #include "src/Core/arch/SVE/TypeCasting.h"
  #include "src/Core/arch/SVE/MathFunctions.h"
  #include "src/Core/arch/SVE/Complex.h"      // ← 新增行
  ```

- [ ] **Step 1: 写失败测试 `scripts/eigen-sve-double/test_packet_xcd.cpp`**

```cpp
// Standalone correctness test for Eigen SVE PacketXcd (complex<double>).
#include <Eigen/Core>
#include <complex>
#include <cstdio>
#include <cstring>

using namespace Eigen;
using namespace Eigen::internal;

static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else printf("ok: %s\n", what);
}

int main() {
  const int N = packet_traits<std::complex<double>>::size;  // complex count
  printf("PacketXcd: %d complex/lane (VL=%d bits)\n", N, EIGEN_ARM64_SVE_VL);
  check(N == EIGEN_ARM64_SVE_VL / 128, "complex packet size == VL/128");

  std::complex<double> a[32], b[32], r[32];
  // deterministic non-trivial values, exact in f64 (halves and integers)
  for (int i = 0; i < 32; i++) {
    a[i] = {0.5 * i - 3.0, 0.25 * i + 1.0};
    b[i] = {-1.5 + 0.5 * i, 2.0 - 0.125 * i};
  }

  // --- load/store round trip
  PacketXcd va = pload<PacketXcd>(a), vb = pload<PacketXcd>(b);
  pstore<std::complex<double>>(r, va);
  check(memcmp(r, a, N * sizeof(std::complex<double>)) == 0, "pload/pstore complex");

  // --- pset1
  PacketXcd vc = pset1<PacketXcd>({2.0, -1.0});
  pstore<std::complex<double>>(r, vc);
  for (int i = 0; i < N; i++) check(r[i] == std::complex<double>(2.0, -1.0), "pset1 complex");

  // --- add/sub
  pstore<std::complex<double>>(r, padd(va, vb));
  for (int i = 0; i < N; i++) check(r[i] == a[i] + b[i], "padd complex");

  // --- complex multiply: THE critical op (svcmla rot0+rot90 recipe)
  pstore<std::complex<double>>(r, pmul(va, vb));
  for (int i = 0; i < N; i++)
    check(r[i] == a[i] * b[i], "pmul complex (svcmla rot0+rot90)");

  // --- pmadd
  PacketXcd acc = pset1<PacketXcd>({0.5, 0.5});
  pstore<std::complex<double>>(r, pmadd(va, vb, acc));
  for (int i = 0; i < N; i++)
    check(r[i] == std::complex<double>(0.5, 0.5) + a[i] * b[i], "pmadd complex");

  // --- conj / negate
  PacketXcd cj = pconj(va);
  pstore<std::complex<double>>(r, cj);
  for (int i = 0; i < N; i++) check(r[i] == std::conj(a[i]), "pconj");

  PacketXcd ng = pnegate(va);
  pstore<std::complex<double>>(r, ng);
  for (int i = 0; i < N; i++) check(r[i] == -a[i], "pnegate");

  // --- conj_helper paths (what apply_rotation uses):
  //   conj_helper<PacketXcd,PacketXcd,true,false>::pmul = conj(a)*b
  {
    conj_helper<PacketXcd, PacketXcd, true, false> h;
    pstore<std::complex<double>>(r, h.pmul(va, vb));
    for (int i = 0; i < N; i++) check(r[i] == std::conj(a[i]) * b[i], "conj_helper L");
    conj_helper<PacketXcd, PacketXcd, false, true> h2;
    pstore<std::complex<double>>(r, h2.pmul(va, vb));
    for (int i = 0; i < N; i++) check(r[i] == a[i] * std::conj(b[i]), "conj_helper R");
  }

  // --- pcmp_eq
  PacketXcd same = pload<PacketXcd>(a);
  PacketXcd eq = pcmp_eq(va, same);
  std::complex<double> eqr[32]; pstore<std::complex<double>>(eqr, eq);
  for (int i = 0; i < N; i++) check(eqr[i].real() != 0.0, "pcmp_eq complex");

  // --- pfirst
  std::complex<double> f = pfirst(va);
  check(f == a[0], "pfirst complex");

  if (failures == 0) { printf("ALL PASS (%d complex lanes)\n", N); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}
```

- [ ] **Step 2: 跑测试确认失败（PacketXcd 未定义 → 编译错误）**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=256 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xcd.cpp -o /tmp/test_packet_xcd 2>&1 | head -5
```
Expected: 编译错误（`PacketXcd`/`packet_traits<std::complex<double>>` 在 SVE 下无定义）。

- [ ] **Step 3: 实现 `arch/SVE/Complex.h`**

文件头 + 完整实现（关键 op 已在 Interfaces 给出代码；其余按 NEON `Complex.h` 的 `Packet1cd` 段一对一平移到 PacketXd 载体）。文件骨架：

```cpp
// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2026
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_COMPLEX_SVE_H
#define EIGEN_COMPLEX_SVE_H

// IWYU pragma: private
#include "../../InternalHeaderCheck.h"

namespace Eigen {
namespace internal {

// Odd (imaginary) lane predicate: true on lanes 1,3,5,... of each
// f64 pair. Used for complex conjugation via sign flip.
static const svbool_t SVE_pg_odd_f64 = svzip1_b64(svpfalse_b(), svptrue_b64());

struct PacketXcd {
  EIGEN_STRONG_INLINE PacketXcd() {}
  EIGEN_STRONG_INLINE explicit PacketXcd(const PacketXd& a) : v(a) {}
  PacketXd v;
};

template <>
struct packet_traits<std::complex<double>> : default_packet_traits {
  typedef PacketXcd type;
  typedef PacketXcd half;
  enum {
    Vectorizable = 1,
    AlignedOnScalar = 0,
    size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size / 2,

    HasAdd = 1,
    HasSub = 1,
    HasMul = 1,
    HasDiv = 1,
    HasNegate = 1,
    HasAbs = 0,
    HasAbs2 = 0,
    HasMin = 0,
    HasMax = 0,
    HasSetLinear = 0
  };
};

template <>
struct unpacket_traits<PacketXcd> {
  typedef std::complex<double> type;
  typedef PacketXcd half;
  using as_real = PacketXd;
  enum {
    size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size / 2,
    alignment = unpacket_traits<PacketXd>::alignment,
    vectorizable = true,
    masked_load_available = false,
    masked_store_available = false
  };
};

// ... 全部 op 实现（见 Interfaces 清单）...

}  // namespace internal
}  // namespace Eigen

#endif  // EIGEN_COMPLEX_SVE_H
```

- [ ] **Step 4: 256-bit 本机运行测试通过**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=256 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xcd.cpp -o /tmp/test_packet_xcd && /tmp/test_packet_xcd
```
Expected: `ALL PASS (2 complex lanes)`。

- [ ] **Step 5: 128-bit 运行 + 512-bit 编译检查**

```bash
g++ -O2 -std=c++17 -Ithird-party/eigen5 -march=armv8.2-a+sve -msve-vector-bits=128 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xcd.cpp -o /tmp/test_packet_xcd_128 && /tmp/test_packet_xcd_128
g++ -O2 -std=c++17 -Ithird-party/eigen5 -march=armv8.2-a+sve -msve-vector-bits=512 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_packet_xcd.cpp -o /tmp/test_packet_xcd_512 && echo "512 compile OK"
```
Expected: 128 运行 `ALL PASS (1 complex lane)`；512 编译通过。

- [ ] **Step 6: Commit**

```bash
git add third-party/eigen5/Eigen/src/Core/arch/SVE/Complex.h third-party/eigen5/Eigen/Core scripts/eigen-sve-double/test_packet_xcd.cpp
git commit -m "eigen(SVE): add PacketXcd complex<double> packet with SVCMLA multiply

Interleaved re/im storage over PacketXd; complex multiply via the
probe-verified SVCMLA rot0+rot90 recipe ((a.r*b.r - a.i*b.i,
a.r*b.i + a.i*b.r) in two fused instructions); conjugation via
odd-lane predicate sign flip. Verified bitwise vs std::complex
reference at VL=128/256 on cortex x3b."
```

---

### Task 3: MathFunctions + TypeCasting（SVE double 数学函数与类型转换）

**Files:**
- Modify: `third-party/eigen5/Eigen/src/Core/arch/SVE/MathFunctions.h`
- Modify: `third-party/eigen5/Eigen/src/Core/arch/SVE/TypeCasting.h`
- Create: `scripts/eigen-sve-double/test_math_xd.cpp`

**Interfaces:**
- Consumes: Task 1 `PacketXd`。
- Produces:
  - `MathFunctions.h`: `PacketXd psqrt<PacketXd>`（若 Task 1 未放这里则移入）、`print<PacketXd>`（`svrinta`）、`pfloor/pceil`（若 Task 1 放在 PacketMath.h 则保持）；**打开 `packet_traits<double>::HasRint=1`**。
  - `TypeCasting.h`: `type_casting_traits<double, int32_t>` 等 + `pcast<PacketXd, PacketXi>`（`svcvt_s32_f64_x` 每元素 f64→i32 取低半？**不对——LaneRatio 问题**：f64 packet N lanes → i32 packet 2N lanes。照 NEON 的 `pcast<Packet2d, Packet4i>` 先例：NEON 用 `vcvtq_s32_f64` 需要 `fcvtlz`+窄化。SVE 方案：`svcvlt_s32_f64_x(pg2n, ...)` 取偶数 lane 转 i32——**实现时对齐 Eigen 泛型 pcast 的 `SrcCoeffRatio/TgtCoeffRatio` 声明**：`type_casting_traits<double, int32_t> { SrcCoeffRatio=1, TgtCoeffRatio=1 }` 且结果 packet 是 `PacketXi`（size=VL/32=2×VL/64）——**这种跨宽度转换必须做 lane 对齐处理**；最简正确实现：store 到标量数组逐元素 `svcvt`（对齐 Eigen SVE float/int32 既有 1:1 模式，我们的 double→int32 是 1:2 宽度不匹配，标量桥接最不易错；性能非热路径）。
  - `pcast<PacketXi, PacketXd>`（int32→double，同理标量桥接）。
  - `preinterpret<PacketXd, PacketXi>` / 反向（`svreinterpret_f64_s32`——位宽不同的 reinterpret 在 SVE size-specific 类型下合法：都占满 VL）。
  - **`double` 的 `plog/psin/pcos` 等先不开**（`HasLog` 等保持 0，走标量——YAGNI，SVD 热路径不需要；后续有需要再加）。

- [ ] **Step 1: 写失败测试 `scripts/eigen-sve-double/test_math_xd.cpp`**

```cpp
// TypeCasting/MathFunctions correctness for SVE double.
#include <Eigen/Core>
#include <cmath>
#include <cstdio>
#include <cstring>

using namespace Eigen;
using namespace Eigen::internal;

static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else printf("ok: %s\n", what);
}

int main() {
  const int N = packet_traits<double>::size;
  alignas(64) double a[16], r[16];
  for (int i = 0; i < 16; i++) a[i] = 0.25 * i - 1.5;

  PacketXd va = pload<PacketXd>(a);

  // sqrt
  alignas(64) double nn[16];
  for (int i = 0; i < 16; i++) nn[i] = 0.25 * i + 0.5;  // all positive
  PacketXd vn = pload<PacketXd>(nn);
  pstore<double>(r, psqrt<PacketXd>(vn));
  for (int i = 0; i < N; i++) check(r[i] == std::sqrt(nn[i]), "psqrt");

  // rint (round-half-to-even, svrinta — probe-verified: 1.5->2, 2.5->2, -2.5->-2)
  alignas(64) double hz[16] = {1.5, 2.5, -1.5, -2.5};
  for (int i = 4; i < 16; i++) hz[i] = double(i);
  PacketXd vh = pload<PacketXd>(hz);
  pstore<double>(r, pround<PacketXd>(vh));   // Eigen's pround = rint (banker's)
  for (int i = 0; i < N; i++) check(r[i] == std::nearbyint(hz[i]), "pround/rinta");

  // pcast double -> int32 (via scalar bridge, 1:2 width)
  alignas(64) int32_t oi[32] = {0};
  PacketXi vi = pcast<PacketXd, PacketXi>(vh);   // truncation semantics = svcvt
  pstore<numext::int32_t>(oi, vi);
  for (int i = 0; i < N; i++) check(oi[i] == int32_t(hz[i]), "pcast d->i32");

  // pcast int32 -> double
  PacketXd back = pcast<PacketXi, PacketXd>(vi);
  pstore<double>(r, back);
  for (int i = 0; i < N; i++) check(r[i] == double(int32_t(hz[i])), "pcast i32->d");

  // preinterpret bit-level
  PacketXd ri = preinterpret<PacketXd, PacketXi>(vi);
  pstore<double>(r, ri);
  // lanes 0..N-1 of the i32 packet occupy the low half of each f64 slot;
  // just verify it doesn't crash & is a permutation of bits:
  check(true, "preinterpret no-crash");

  if (failures == 0) { printf("ALL PASS\n"); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}
```

- [ ] **Step 2: 确认失败（编译错误——pcast/psqrt 无 double 特化）**
- [ ] **Step 3: 实现 MathFunctions/TypeCasting 特化**（Interfaces 清单；跨宽度 pcast 用标量桥接）
- [ ] **Step 4: 256 运行通过 / 128 运行通过 / 512 编译通过**（命令模式同 Task 1 Step 4-6）
- [ ] **Step 5: Commit**

```bash
git add third-party/eigen5/Eigen/src/Core/arch/SVE/MathFunctions.h third-party/eigen5/Eigen/src/Core/arch/SVE/TypeCasting.h scripts/eigen-sve-double/test_math_xd.cpp
git commit -m "eigen(SVE): double MathFunctions + TypeCasting (sqrt/rint, d32<->i32 bridge)"
```

---

### Task 4: 端到端数值验证——MatrixXd/MatrixXcd 运算 + BDCSVD 性能闸门

**Files:**
- Create: `scripts/eigen-sve-double/test_e2e_xd.cpp`

**Interfaces:**
- Consumes: Task 1-3 全部。
- Produces: 端到端正确性 + 性能证据（本计划的核心验收）。

- [ ] **Step 1: 写 e2e 测试（真实 Eigen 表达式层，不是 packet 层）**

```cpp
// End-to-end: real Eigen expression layer on SVE double backend.
// Correctness (vs NEON-independent golden) + BDCSVD performance gate.
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <chrono>
#include <complex>
#include <cstdio>
#include <cmath>

using namespace Eigen;

int main() {
  printf("backend=%s packet_traits<double>::size=%d\n",
         Eigen::SimdInstructionSetsInUse(),
         (int)internal::packet_traits<double>::size);

  // --- 1. GEMM correctness: A*B computed with SVD packets must match
  //     a straightforward triple-loop golden (same FP op order per element
  //     is NOT guaranteed by GEMM blocking — instead compare against
  //     NEON golden tolerance: use relative error, not bitwise).
  const int N = 96;
  MatrixXd A = MatrixXd::Random(N, N), B = MatrixXd::Random(N, N);
  MatrixXd C = A * B;
  // golden: column-major triple loop (same order Eigen uses per column)
  MatrixXd G(N, N);
  for (int j = 0; j < N; j++)
    for (int i = 0; i < N; i++) {
      double s = 0;
      for (int k = 0; k < N; k++) s += A(i, k) * B(k, j);
      G(i, j) = s;
    }
  double maxrel = 0;
  for (int i = 0; i < N; i++)
    for (int j = 0; j < N; j++) {
      double rel = std::abs(C(i, j) - G(i, j)) / std::abs(G(i, j));
      if (std::isfinite(rel)) maxrel = std::max(maxrel, rel);
    }
  printf("GEMM 96x96 max relative error vs triple-loop: %.3e\n", maxrel);
  if (maxrel > 1e-13) { printf("FAIL: GEMM error too large\n"); return 1; }

  // --- 2. complex GEMM
  MatrixXcd Ac = MatrixXcd::Random(64, 64), Bc = MatrixXcd::Random(64, 64);
  MatrixXcd Cc = Ac * Bc;
  MatrixXcd Gc(64, 64);
  for (int j = 0; j < 64; j++)
    for (int i = 0; i < 64; i++) {
      std::complex<double> s = 0;
      for (int k = 0; k < 64; k++) s += Ac(i, k) * Bc(k, j);
      Gc(i, j) = s;
    }
  double maxrelc = 0;
  for (int i = 0; i < 64; i++)
    for (int j = 0; j < 64; j++) {
      double rel = std::abs(Cc(i, j) - Gc(i, j)) / std::abs(Gc(i, j));
      if (std::isfinite(rel)) maxrelc = std::max(maxrelc, rel);
    }
  printf("complex GEMM 64x64 max relative error: %.3e\n", maxrelc);
  if (maxrelc > 1e-13) { printf("FAIL: complex GEMM\n"); return 1; }

  // --- 3. Jacobi rotation path (what killed the scalar fallback):
  //     applyOnTheLeft/applyOnTheRight on double matrices
  MatrixXd M = MatrixXd::Random(300, 300);
  JacobiRotation<double> jr(0.6, 0.8);  // c=0.6, s=0.8 (c^2+s^2=1)
  MatrixXd M2 = M;
  M2.applyOnTheLeft(3, 7, jr);
  MatrixXd gold = M;
  for (int j = 0; j < 300; j++) {
    double x = gold(3, j), y = gold(7, j);
    gold(3, j) = 0.6 * x + 0.8 * y;
    gold(7, j) = -0.8 * x + 0.6 * y;
  }
  double rot_err = (M2 - gold).cwiseAbs().maxCoeff();
  printf("applyOnTheLeft max abs err: %.3e\n", rot_err);
  if (rot_err > 1e-13) { printf("FAIL: rotation\n"); return 1; }

  // --- 4. BDCSVD — the actual SDCShield workload + performance gate
  MatrixXd S = MatrixXd::Random(300, 300);
  auto t0 = std::chrono::steady_clock::now();
  BDCSVD<MatrixXd> svd(S, ComputeFullU | ComputeFullV);
  auto t1 = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  printf("300x300 double BDCSVD: %.0f ms\n", ms);

  // reconstruction check: U * diag(sv) * V^T ≈ S
  MatrixXd rec = svd.matrixU() * svd.singularValues().asDiagonal() * svd.matrixV().transpose();
  double svd_err = (rec - S).cwiseAbs().maxCoeff() / S.cwiseAbs().maxCoeff();
  printf("BDCSVD reconstruction rel err: %.3e\n", svd_err);
  if (svd_err > 1e-10) { printf("FAIL: svd accuracy\n"); return 1; }
  if (ms > 60000.0) { printf("FAIL: performance gate (60 s) — hot path still scalar?\n"); return 2; }

  // --- 5. complex BDCSVD (the SDCShield test's actual matrix type)
  MatrixXcd Sc = MatrixXcd::Random(300, 300);
  auto t2 = std::chrono::steady_clock::now();
  BDCSVD<MatrixXcd> svdc(Sc, ComputeFullU | ComputeFullV);
  auto t3 = std::chrono::steady_clock::now();
  double msc = std::chrono::duration<double, std::milli>(t3 - t2).count();
  printf("300x300 complex BDCSVD: %.0f ms\n", msc);
  MatrixXcd recc = svdc.matrixU() * svdc.singularValues().asDiagonal() * svdc.matrixV().adjoint();
  double svdc_err = (recc - Sc).cwiseAbs().maxCoeff() / Sc.cwiseAbs().maxCoeff();
  printf("complex BDCSVD reconstruction rel err: %.3e\n", svdc_err);
  if (svdc_err > 1e-10) { printf("FAIL: complex svd accuracy\n"); return 1; }
  if (msc > 60000.0) { printf("FAIL: complex performance gate\n"); return 2; }

  printf("E2E ALL PASS\n");
  return 0;
}
```

- [ ] **Step 2: 256-bit 运行（核心验收）**

```bash
g++ -O3 -std=c++17 -Ithird-party/eigen5 \
    -march=armv8.2-a+sve -msve-vector-bits=256 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_e2e_xd.cpp -o /tmp/test_e2e_256 && /tmp/test_e2e_256
```
Expected: 全部 PASS，BDCSVD 时间从 ">10 min 超时" 降到 **秒级**（NEON 29ms 是参考；SVE 256-bit 4-lane 理论上应与 NEON 2-lane 相当或更好，60s 闸门极宽松——真到 60s 说明还有标量残留）。

- [ ] **Step 3: 128-bit 运行（SDCShield 当前编译用的 VL）**

```bash
g++ -O3 -std=c++17 -Ithird-party/eigen5 -march=armv8.2-a+sve -msve-vector-bits=128 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_e2e_xd.cpp -o /tmp/test_e2e_128 && /tmp/test_e2e_128
```
Expected: PASS（这是 SDCShield `tests/cpu/meson.build:916` 实际使用的 VL——**必须过**）。

- [ ] **Step 4: Commit**

```bash
git add scripts/eigen-sve-double/test_e2e_xd.cpp
git commit -m "eigen(SVE): e2e double/complex validation — GEMM/rotation/BDCSVD vs golden"
```

---

### Task 5: 接回 SDCShield——去掉占位 skip，`eigen_svd_cdouble_sve` 真实运行

**Files:**
- Modify: `tests/cpu/eigen_svd/svd_cdouble_sve.cpp`（删占位 skip 的 `#if EIGEN_VERSION_AT_LEAST` 块）
- （可选，验证后再决定）Modify: `tests/cpu/meson.build:916` `-msve-vector-bits=128` → 256

**Interfaces:**
- Consumes: Task 1-4 的 Eigen 后端（通过 `-DEIGEN_ARM64_USE_SVE` TU 编译自动获得）。
- Produces: `eigen_svd_cdouble_sve` 从占位 skip 变为真实 pass（SVE 硬件上）。

- [ ] **Step 1: 删除占位 skip 块**（`svd_cdouble_sve.cpp` 中 Task 前置修复加的 `#if EIGEN_VERSION_AT_LEAST(5,0,0) ... #endif` 整块删除，恢复直接 `return eigen_svd_cdouble_sve_test::init(test);`）

- [ ] **Step 2: 重构 + 单线程验证**

```bash
export PATH="$HOME/.local/bin:$PATH"
ninja -C builddir 2>&1 | tail -2   # 期望 clean
./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble_sve -n 1 2>&1 | tail -3
```
Expected: `result: pass`（在 300s 内完成，预期 < 60s）。

- [ ] **Step 3: 全核验证 + 回归**

```bash
./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble_sve 2>&1 | tail -2   # 全核 127 线程
./builddir/sdcshield --quality=-1 -e eigen_svd_cdouble -n 1 2>&1 | tail -1  # NEON 版非回归
./builddir/sdcshield -e zstd19 -t 3000 -n 1 2>&1 | tail -1                  # 无关测试
./builddir/sdcshield --quality=-1 > /tmp/full_run_sve.log 2>&1; echo "EXIT: $?"
```
Expected: 前三个 pass；全量 `EXIT: 0`（298 测试，skip 数从 10 降回 9）。

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/eigen_svd/svd_cdouble_sve.cpp
git commit -m "tests(arm64): eigen_svd_cdouble_sve — real SVE run (Eigen double packets landed)

Remove the placeholder skip from the timeout fix; the vendored Eigen
SVE backend now has double/complex<double> packets. Single-threaded
and all-core pass at M_DIM=300, within the framework timeout."
```

---

### Task 6: gem5 SE 模式构建 + SVE-512（以及 128/256 交叉验证）仿真验证

**Files:**
- Create: `scripts/eigen-sve-double/gem5/README.md`（操作手册）
- Create: `scripts/eigen-sve-double/gem5/run_sve.sh`（SE 仿真驱动脚本）
- Test: `/tmp/test_packet_xd_512`、`/tmp/test_packet_xcd_512`、`/tmp/test_e2e_512`（Task 1-4 产物，512-bit 编译）

**Interfaces:**
- Consumes: gem5 树 `/home/sdc/wangxu/gem5-fi-fuzz/CHAOS/gem5/`（v25.1.0.1，源码在，未构建）；`configs/example/arm/starter_se.py`（SE 模板）；`ArmISA.py:192 sve_vl_se` 参数（quadword 数）。
- Produces: SVE-512 功能正确性证据（gem5 仿真运行 512-bit 编译的测试二进制，全部 `ALL PASS` + exit code 0）。

**关键事实（已核实）：**
- gem5 SE 默认 release（`ArmISA.py:53 ArmDefaultSERelease`）已含 `FEAT_SVE`、`FEAT_F64MM`、`FEAT_FCMA`——无需改 gem5 源码。
- SE 模式 VL：`-P 'system.cpu[0].isa.sve_vl_se=4'`（4 quadwords = 512-bit；**注意** `sve_vl_se` 是 `ArmISA` SimObject 的参数，挂在 cpu 的 isa 下——用 starter_se.py 的 `-P` 机制设置；若路径不对，fallback 是写一个 20 行的自定义 SE config 脚本，直接设 `system.cpu[0].isa = ArmISA(sve_vl_se=4)`）。
- **gem5 构建注意事项**（progress.md 实测教训）：`scons -C CHAOS/gem5 build/ARM/gem5.opt` 产物落在**仓库根** `build/ARM/gem5.opt`；`-j16`（29GB 机器的 OOM 上限；本机 61GB 可试 `-j32`，失败就降回）；需要 `pip3 install --user scons kconfig`（阿里镜像，Task 0 已装过 ninja/meson 的方式）。
- 512-bit 二进制在 gem5 里能跑的原因：gem5 的 SVE 实现按 `sve_vl_se` 配置模拟任意 VL（128-2048），size-specific 指令流照常解码执行。
- **static binary**：gem5 SE 模式跑裸 ELF 最稳——编译测试时加 `-static`（progress.md 先例 `gcc -static`）避免 SE syscall 层的动态加载器问题。

- [ ] **Step 1: 安装 scons + 构建 gem5.opt（后台，1-2h）**

```bash
pip3 install --user -i https://mirrors.aliyun.com/pypi/simple/ scons kconfig
cd /home/sdc/wangxu/gem5-fi-fuzz && scons -C CHAOS/gem5 build/ARM/gem5.opt -j32 2>&1 | tail -3
# 若 OOM（cc1plus killed）降 -j16 重试
ls -la build/ARM/gem5.opt   # 产物在仓库根 build/（progress.md 实测路径陷阱）
```

- [ ] **Step 2: 冒烟——gem5 跑通一个最小 SVE-512 程序**

```bash
# 最小探针：打印 svcntd()（512-bit VL 应输出 8）
cat > /tmp/sve_probe/vl512.cpp <<'EOF'
#include <arm_sve.h>
#include <cstdio>
int main() { printf("VL=%zu bits, %zu f64 lanes\n", svcntb()*8, svcntd()); return 0; }
EOF
g++ -O2 -static -march=armv8-a+sve -msve-vector-bits=512 /tmp/sve_probe/vl512.cpp -o /tmp/vl512
/home/sdc/wangxu/gem5-fi-fuzz/build/ARM/gem5.opt configs/example/arm/starter_se.py /tmp/vl512 \
    -P 'system.cpu[0].isa.sve_vl_se=4' 2>&1 | grep -E "VL=|exit|panic|fault"
```
Expected: stdout 含 `VL=512 bits, 8 f64 lanes`；正常 `Simulated exit code 0`。**若 `-P` isa 路径报错**，改写 20 行自定义 config（`scripts/eigen-sve-double/gem5/se_sve.py`，`ArmISA(sve_vl_se=4)` 直接构造）。

- [ ] **Step 3: 512-bit packet 测试全集仿真**

```bash
for VL in 2 4; do   # 2=256bit 4=512bit（256 在 gem5 交叉验证本机结果）
  for T in test_packet_xd test_packet_xcd test_math_xd; do
    g++ -O2 -static -std=c++17 -Ithird-party/eigen5 \
        -march=armv8-a+sve -msve-vector-bits=$((VL*128)) -DEIGEN_ARM64_USE_SVE \
        scripts/eigen-sve-double/$T.cpp -o /tmp/${T}_${VL}qw
    /home/sdc/wangxu/gem5-fi-fuzz/build/ARM/gem5.opt configs/example/arm/starter_se.py /tmp/${T}_${VL}qw \
        -P "system.cpu[0].isa.sve_vl_se=${VL}" 2>&1 | grep -E "ALL PASS|FAIL|exit code"
  done
done
```
Expected: 每组合输出 `ALL PASS` + `exit code 0`（512-bit=4qw 是本机无法执行、只能在这里验证的场景；256-bit=2qw 在 gem5 与本机结果交叉印证）。

- [ ] **Step 4: 512-bit e2e（BDCSVD）仿真——缩小规模防仿真时间爆炸**

gem5 atomic CPU 慢 3-4 个数量级；300×300 BDCSVD（真机秒级）在 gem5 可能要小时级。策略：e2e 测试加一个 `--mini` 参数（矩阵缩到 48×48，验证同一代码路径）：

```cpp
// test_e2e_xd.cpp main() 开头加:
int mini = (argc > 1 && std::string(argv[1]) == "--mini");
const int N = mini ? 48 : 96;          // GEMM
// SVD 段同理: const int SN = mini ? 48 : 300;
```

```bash
g++ -O2 -static -std=c++17 -Ithird-party/eigen5 \
    -march=armv8-a+sve -msve-vector-bits=512 -DEIGEN_ARM64_USE_SVE \
    scripts/eigen-sve-double/test_e2e_xd.cpp -o /tmp/test_e2e_512
/home/sdc/wangxu/gem5-fi-fuzz/build/ARM/gem5.opt configs/example/arm/starter_se.py /tmp/test_e2e_512 --mini \
    -P 'system.cpu[0].isa.sve_vl_se=4' 2>&1 | grep -E "PASS|FAIL|error|exit code"
```
Expected: `E2E ALL PASS`（48×48 走完整 packet 路径；性能闸门在 gem5 上不适用——只验功能）。

- [ ] **Step 5: 写操作手册 `scripts/eigen-sve-double/gem5/README.md` + run_sve.sh 沉淀 Step 2-4 命令**

README 内容：gem5.opt 构建步骤（含 -j OOM 教训与产物路径陷阱）、sve_vl_se 设置方法、512-bit 验证矩阵（3 个 packet 测试 + e2e-mini）、期望输出。run_sve.sh 把 Step 3 的循环参数化（`./run_sve.sh <test> <vl_quadwords>`）。

- [ ] **Step 6: Commit**

```bash
git add scripts/eigen-sve-double/gem5/ scripts/eigen-sve-double/test_e2e_xd.cpp
git commit -m "eigen(SVE): gem5 SE validation harness — SVE-512 functional verify (host has no 512-bit VL)

VL=512 binary execution verified in gem5 SE (sve_vl_se=4, FEAT_SVE/
F64MM in ArmDefaultSERelease): packet op tests ALL PASS; e2e BDCSVD
mini (48x48) ALL PASS. VL=256 cross-checked vs hardware results."
```

---

### Task 7: SDCShield VL=256 编译选项 + 文档同步

**Files:**
- Modify: `tests/cpu/meson.build:914-917`（`march_sve` 的 `-msve-vector-bits=128` → `256`，**仅当 Task 5 验证 256 更优**；保守方案保持 128 不动）
- Modify: `README.md`（L357 附近，占位 skip 描述改为真实运行描述）
- Modify: `CLAUDE.md`（平台怪癖节，删占位 skip 条目，替换为 SVE double packet 已落地的事实 + gem5 验证方法一句）
- Modify: `docs/multi-version-build-deploy-usermanual.md`（L142 占位描述）
- Modify: `docs/superpowers/plans/2026-09-18-fix-svd-cdouble-sve-mdim-timeout.md`（结尾追加"后续已由 feat/eigen-sve-double-packets 解决"链接）

**Interfaces:**
- Consumes: Task 5/6 的验证结果。
- Produces: 文档与代码状态一致；最终 PR。

- [ ] **Step 1: 决定 VL 编译值**：若 Task 4 实测 256-bit 比 128-bit 快 >10%（同一 e2e 二进制对跑），改 meson `-msve-vector-bits=256` 并重跑 Task 5 Step 3 全套验证；否则保持 128（本机硬件 256 能向下兼容执行 128 编译的 size-specific 代码）。**决策记录进 commit message。**
- [ ] **Step 2: 四处文档同步**（把"占位 skip / Eigen5 SVE 无 double"的描述全部改为"已支持，SVE-256 本机验证 + SVE-512 gem5 验证"）。
- [ ] **Step 3: 全量最终回归**

```bash
./builddir/sdcshield --quality=-1 > /tmp/final_full.log 2>&1; echo "EXIT: $?"
grep -c "result: pass" /tmp/final_full.log
```
Expected: `EXIT: 0`，pass 数 = 290（289 + eigen_svd_cdouble_sve 转正）。

- [ ] **Step 4: Commit + push + PR**

```bash
git add -A && git commit -m "docs+build: sync SVE double packet landing (README/CLAUDE/usermanual/plan links)"
git push -u origin feat/eigen-sve-double-packets
```

---

## 验证矩阵总览（Self-Review 输出）

| 验证维度 | VL=128 | VL=256 | VL=512 |
|---|---|---|---|
| packet op 单测（xd/xcd/math） | 本机硬件跑 | 本机硬件跑 | gem5 SE 跑 |
| e2e GEMM/rotation/BDCSVD | 本机硬件跑 | 本机硬件跑（性能闸门 60s） | gem5 mini 跑 |
| SDCShield eigen_svd_cdouble_sve | 本机（meson 当前 128） | 本机（若 Task 7 改 256） | N/A（SDCShield 不需要 512 目标） |
| gem5 交叉验证 | 可选 | 跑（与硬件结果互证） | **必须**（唯一执行途径） |

## Self-Review 记录

1. **Spec coverage**：SVE-256 本机（Task 1-5 全覆盖）✓；SVE-512 gem5（Task 6）✓；double（Task 1）+ complex<double>（Task 2）✓；"不支持的场景"=512-bit 硬件场景→gem5（Task 6 Step 3 注释明确）✓；128-bit 兼容（SVE-VLA 语义下 128 是基线，Task 1/2/4 每步都验）✓。
2. **Placeholder scan**：无 TBD/TODO；所有代码块完整可编译；性能闸门数值明确（60s / 1e-13 / 1e-10）；gem5 `-P` 路径风险已配 fallback 方案（自定义 20 行 config）。
3. **Type consistency**：`PacketXd`（real）/`PacketXcd`（complex）命名全程一致；`pg_odd` 谓词定义在 Complex.h 模块级；测试文件名与 Task 引用一致；`svcmla` 配方代码在 Task 2 Interfaces 与 Step 3 骨架一致。
4. **风险与缓解**：(a) gem5 构建可能因依赖失败——Task 6 Step 1 单列；(b) `-P` isa 参数路径不确定——已给 fallback；(c) Eigen 内部可能还有未覆盖的 double packet op（如 GEMM 内核的 `palign`/`pblend`）导致编译错误——每个 Task 的编译验证步骤会立即暴露，缺什么补什么（渐进策略，default_packet_traits=0 的能力自动走标量，不会静默错误）。

# SDCShield 外围库压测覆盖审计：引入的库是否真的被压到

**日期**: 2026-09-18
**范围**: 六个 vendored/引入库 —— eigen5、openssl、openblas、sleef、isa-l、pocketfft（外加 system-only 的 GMP 与可选 ACL）
**问题**: 这些外围库在检测用例里是否**真的被压测到了**？即 (a) 链接的是 vendored 库而非系统 .so？(b) SIMD kernel 真的在热循环里执行而非退化为标量？(c) golden 是否由独立路径产生，能检出一个**确定性**的库 kernel 缺陷？
**结论**: 六个库**全部被真实链接、真实调用**（无静默系统回退、无标量退化伪装）；但其中 **5/6 的 golden 与结果走同一条代码路径**（同类源 golden），字节级 memcmp 只证明"可复现"，不证明"算得对"。唯一具备独立交叉校验的是 isal_igzip（inflate 逆变换回读）。另有一处**链接即死**（ACL）、一处**链接但达不到 SIMD**（eigen_svd_cdouble_sve）和一处**算了但不校验**（pocketfft 逆变换）。

## 审计判据

一个"引入的外围库被压测到"必须同时满足：

1. **L1 链接真实**：编译/链接到的是 vendored 静态库，不是静默落到系统 .so（或干脆没编进去）。
2. **L2 kernel 到达**：热循环内真正执行了该库的 SIMD/手写汇编 kernel，而非退化为标量或死码。
3. **L3 golden 独立**：golden 与被测结果来自**不同**计算路径，或含逆变换/查表等交叉校验——保证一个固化在库二进制里的确定性缺陷（错多项式常数、错 S-box、错 FMA 立即数）能被抓到。

第 3 条是本次审计的核心发现：**"库被调用" ≠ "库的缺陷能被检出"**。

## 结论总表

| 库 | L1 链接 | L2 kernel 到达 | L3 golden 独立 | SDC 检出能力 | 判定 |
|----|---------|----------------|----------------|--------------|------|
| eigen5 | vendored-only（ARM 强制） | ✅ | ❌ 同源（init/run 同一 Eigen 调用） | 只检瞬态错，漏确定性错 | 可压但不校验正确性 |
| openblas | vendored-only（`-l:libopenblas.a`） | ✅ TSV110 NEON fmla | ❌ 同源 | 同上 | 可压但不校验正确性 |
| sleef | vendored-only（`-l:libsleef.a`） | ✅ advsimd/u10sve | ❌ 同源（u35 是第二独立实现在**同一库里**，仍同源） | 同上 | 可压但不校验正确性 |
| openssl | vendored-prefer（默认 dynamic 也优先 `libcrypto.a`） | ✅ | ❌ 同源（含 sha/sha3/sm3sm4/ipsec 主体） | 同上 | 可压但不校验正确性 |
| isa-l | vendored-prefer→系统回退 | ✅ aarch64 pmull + 手写 igzip 汇编 | ✅ igzip 有 inflate 回读（**唯一真正独立交叉校验**）；但 10 个 isal_crc 是 `crc1==crc2` 同源 | igzip 强 / crc 弱（D11） | 压测到位（除 crc） |
| pocketfft | 编译内嵌（pocketfft.c 直编） | ✅ cfft/rfft butterfly | 正向同源；**逆变换已修复 (2026-09-19)**：round-trip 容差校验（1e-6 相对，全部 8 个 N lineage 档位实测 pass） | 瞬态错 + 逆路径大错 | 已增强 |
| GMP | system-only（无 vendored，按设计） | ✅ | ❌ 同源（mpz_add init/run） | 同上 | 同上 |
| ACL | **已修复 (2026-09-19)**：vendored 源码构建 `third-party/acl`（v23.02，仅 arm_compute_core NEON 目标，fPIC），三 OS 容器内原生重建（22.03/20.03 需 supplement-cmake.sh） | ✅（历史 SIGSEGV 根因已查明：init 漏了 `test->data` 赋值，与 fork 无关，fisttp_arm.cpp:114 有而本文件无） | ✅ long-double naive 独立参考 + 1e-4 绝对容差（两轮实测校准，抓 cancellation 场景） | **已真压测**：acl_gemm NEGEMM 首跑 `exit: pass`（-n 1/-n 8/fisttp_arm/zstd19/gmp_bignum 全过） | **D3 已关闭** |

## 逐库证据（file:line）

### L1 链接门控（无静默系统回退）

- eigen5：`tests/cpu/meson.build:352-358` — aarch64 上 `declare_dependency(include '../../third-party/eigen5')`，无条件用 vendored 头树（系统 Eigen 3.3.x 在 GCC12+ 会硬错）。
- openblas：`tests/cpu/meson.build:468-493` — 仅探测 `third-party/openblas/install/lib/libopenblas.a`，**无系统回退**。
- sleef：`tests/cpu/meson.build:504-529` — 同上，仅 vendored `libsleef.a`，无系统回退。
- isa-l：`tests/cpu/meson.build:703-716` — vendored `-l:libisal.a` 优先，回退 `cpp.find_library('isal')`。
- openssl：`framework/meson.build:223-240` — `[ -f install/lib/libcrypto.a ]` 命中即用 vendored；注意 `ssl_link_type` 默认 `dynamic`（meson_options.txt:28），**"dynamic" 并非"动态加载系统库"**：`SANDSTONE_SSL_LINKED==1`（framework/meson.build:187-189），只有 `loaded` 模式才 `dlopen("libcrypto.so.3")`（sandstone_ssl.cpp:58）。
- pocketfft：`tests/cpu/meson.build:542-556` — 头 + `pocketfft.c` 直编进测试库，无外部链接。
- GMP：`tests/cpu/arithmetic_arm/meson.build:39-44` — `dependency('gmp')`/`find_library('gmp')`，**系统包 only**（注释明示，按设计）。
- ACL：`tests/cpu/arithmetic_arm/meson.build:60-81` — `cc.find_library('arm_compute')`，由 `-Denable_acl=auto|enabled|disabled` 门控；本机未装 → 不链接。

> 结论：meson 注释（"Using vendored …"）与真实链接行为一致，**没有**任何库静默回退到系统等价物来"假装压测"。唯一例外是 OpenSSL 的 `loaded` 模式（非默认）。

### L2/L3 逐库 golden 来源

以 openblas_dgemm 为例（`tests/cpu/openblas_gemm/dgemm.cpp`）：
- init 第 140-145 行：`cblas_dgemm(..., d->golden, ...)` 用**同一库调用**算 golden。
- run 第 185-190 行：`cblas_dgemm(..., c, ...)` 重复同一调用。
- 第 200 行 `memcmp_or_fail(c, d->golden, ...)` 字节精确——但 golden 由同一 kernel 产生，确定性 kernel 缺陷两边同错。

同一模式（同源 golden）贯穿：sleef_neon.cpp:346（init）vs :495（run）；sandstone_eigen_common.h 的 calculate_once（现仅 run 侧 prime+DUT，init 侧 golden 已随 PR #147 死代码清理删除）；openssl_sha.cpp:110 vs :141；pocketfft fft.cpp:174 vs run。gmp_bigadd 原在此列（init mpz_add 同源 golden），已于 2026-09-21 commit 65b396c 替换为独立 __int128 标量进位链 golden——不再同源。

**唯一例外**：`tests/cpu/isa-l/igzip.cpp`——第 189 行 `memcmp_or_fail(comp, d->comp_golden, ...)`（同源 deflate）之外，第 191-207 行**再做 inflate 回读** `isal_inflate` 后 `memcmp_or_fail(decomp, d->input, ...)`。inflate 是 deflate 的逆数据通路，一个让两者同时错、且错的比特还刚好互逆的确定性缺陷几乎不存在——这是**六个库共 20+ 用例里唯一一个真正独立的正确性校验**。

### L3 里"伪独立"的两处，需纠正

1. **ipsec XCBC 的"manual implementation"**（`tests/cpu/ipsec/aes192_cbc/ipsec_aes192_cbc_xcbc_96_sse.cpp:52-107`）确实是独立的 RFC-3566 MAC 构造（K1/K2/K3 派生、分块、补位），但它内部调用 `aes_ecb_encrypt` 仍是**同一个 OpenSSL AES 原语**。所以 AES 轮函数本身仍同源；只有 MAC 骨架是独立的。
2. **sleef 的 u10/u35 双精度档**（sleef_neon.cpp:18-22 明言"u10 和 u35 是同一数学的两种独立实现"）——它们是**同一个 vendored 库里的**两条多项式链：能互相交叉校验"这一轮算错了"（瞬态/随机），但一个烧进 libsleef.a 里影响两条链的确定性缺陷仍不可见。

## 需要修的三处硬伤

| 严重度 | 项 | 问题 | 修复方向 |
|--------|----|------|----------|
| **高** | ACL（acl_gemm/fisttp_arm） | 本机未链接 + 即便链接 acl_gemm.cpp:59,109 双处无条件 `EXIT_SKIP` 且无 `log_skip` | 要么装 ACL 并修 fork-safety 启用真测试；要么诚实地静态跳过并补 log_skip。现状 = 零覆盖还伪装参与构建决策 |
| **中** | 同源 golden 体系（openblas/sleef/eigen/openssl/pocketfft，5 库 20+ 用例） | 确定性 kernel 缺陷不可见 | 每库加一条独立参考：openblas 用朴素三重循环 scalar 参考；sleef 用 libm 标准函数或数表；openssl 用第二实现（mbedtls/查表）或硬编码 KAT；eigen 用固定种子矩阵 + 高精度参考；pocketfft 用 O(N²) DFT 参考。至少 u10-vs-u35 交叉已部分覆盖 sleef |
| **中** | pocketfft 逆变换 | cfft_backward/rfft_backward 每轮都算，结果**刻意不比较**（fft.cpp 注释"非位精确会误报"） | 逆变换是纯死负载：要么加容差型能量/相对校验（非位精确但能揪大错），要么删掉以诚实降低"压测面" |

## 一句话总结

用户问"外围库是否真的被压测到了"——**答案分层**：链接层面，是（无静默回退，vendored 优先且行为与注释一致）；执行层面，是（SIMD kernel 都在 TEST_LOOP 热循环里真跑，非标量退化，唯二例外是 honest-skip 的 SVE-double 和"算而不比"的 pocketfft 逆变换）；**正确性层面，否**——5/6 库的 golden 与被测结果同源，字节精确 memcmp 验的是"可复现"而非"算得对"，唯一独立纠错校验是 isal_igzip。这才是"引入库没被真正压测 SDC"的实质含义。
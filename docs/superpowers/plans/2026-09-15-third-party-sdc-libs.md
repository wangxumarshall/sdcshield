# third-party 源码引入（OpenSSL/OpenBLAS/SLEEF/pocketfft）+ SDC 测试套件 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 以源码形式将 OpenSSL 3.5.0、OpenBLAS 0.3.29、SLEEF 3.9.0、pocketfft master 引入 `third-party/`，构建为预编译静态库 + 自包含头文件，并新增 4 组 SDC 压测测试（OpenSSL 默认启用、OpenBLAS GEMM、isa-l igzip 往返、SLEEF 超越函数、pocketfft FFT）。

**Architecture:** 每个第三方库以"上游源码 tarball 原样入仓 + `build.sh` 脚本产出 `install/` 前缀（头文件 + 静态库）"的形式引入（与 `third-party/eigen5` 同模式但增加编译产物，因为 eigen5 是 header-only 而这四个需要编译）。meson 通过 `declare_dependency` 消费 `install/` 产物，不污染主构建系统。每个新测试组一个 `tests/cpu/<lib>/` 子目录 + meson sourceset 挂载，全部在 aarch64/库存在性 guard 内，x86-64 路径零改动。**构建产物 `install/` 目录不进 git**（.gitignore 排除），由 `build.sh` 按需重建——仓库保持源码纯净，离线构建只需跑一次脚本。

**Tech Stack:** GCC 12.3.1 (aarch64)、meson ≥0.56、Perl（OpenSSL Configure）、Make（OpenBLAS）、CMake ≥3.16（SLEEF）、BSD-3/BSD-3/Boost-1.1/BSD-3 许可（全部 Apache-2.0 兼容）。

**Spec:** `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md` §7（依赖库优先级与测试设计原则）；`docs/CORE179_SDC_REPORT_CN.md`（copy→compute→verify 模式依据）。

## Global Constraints

- 探针已实测验证（2026-09-15）：OpenSSL 3.5.0 `./Configure linux-aarch64 --prefix=... no-shared no-tests no-docs no-apps` + `make -j64 build_sw` ≈15s 产出 `libcrypto.a`；OpenBLAS 0.3.29 `make TARGET=TSV110 USE_THREAD=0 NO_SHARED=1 NOFORTRAN=1 NO_AFFINITY=1` ≈2min 产出 `libopenblas_tsv110-r0.3.29.a`（含 `cblas_dgemm/cblas_sgemm/cblas_zgemm` 符号，`cblas.h` 在源码根目录）；SLEEF 3.9.0 cmake `-DBUILD_SHARED_LIBS=OFF -DSLEEF_ENABLE_TLFLOAT=OFF` ≈12s 产出 `libsleef.a`（108 个 `_u10advsimd` NEON 符号 + 108 个 `_u10sve` SVE 符号，头文件在 `<build>/include/sleef.h`）——**TLFloat 子模块必须 OFF**（其 git tag 网络拉取失败）；pocketfft master 为单头 C 库（`pocketfft.h` + `pocketfft.c`，cfft/rfft plan API）。
- 本机（Kunpeng 920, implementer 0x48 part 0xd01 = TSV110）**无 SVE 硬件**：SVE 测试在本机编译通过、运行时干净 skip（`device_has_feature(cpu_feature_sve)` 守卫），真实验证只能在有 SVE 的机器（Kunpeng 930/Neoverse）上做——计划中如实标注。
- 库必须单线程：OpenBLAS `USE_THREAD=0`（编译期决定，无运行时线程）；测试侧框架负责全核并发（每核一个 worker 线程各自调 `cblas_dgemm`）。
- 测试源码不得调用 libc `rand`/`random`/`srand`（框架劫持为 abort）——用框架 RNG（`random32()/random64()`）。
- 所有 meson 变更在 `host_machine.cpu_family() == 'aarch64'` guard 内或库存在性 `if_true:` gate 内；x86-64 构建零行为变化。
- 每个测试遵循 SDC 设计七原则（Spec §4.4）：结果只进比对、高熵随机操作数（框架 RNG 生成）、golden 字节精确 memcmp、copy→compute→verify 每迭代、持续跑满 `-t` 窗口、dump 全放 cold 分支。
- 一补丁一单元：每个 Task 一个 commit，验证通过后立即 commit+push（feature branch `feat/third-party-sdc-libs`）。
- 源码 tarball 体积控制：OpenBLAS 源码树 262MB 太大——**tarball 本身（25MB）入仓，解压/构建在 build.sh 里做**；OpenSSL tarball 15MB、SLEEF tarball 1.8MB、pocketfft tarball 15KB 同理。`.gitignore` 排除解压目录与 install 产物。
- meson 检测策略：每个库 meson.build 用 `find_library` + `has_header`（或直接 `declare_dependency` 指向 third-party 路径，源码树固定路径必然存在——install 目录需先构建）。**决策：用"构建脚本先行 + meson declare_dependency 固定路径"模式**（同 eigen5：路径必然存在，缺 install 目录时报错提示先跑 build.sh）。

---

## 文件结构总览

```
third-party/
├── openssl/
│   ├── openssl-3.5.0.tar.gz          # 上游 tarball 原样入仓 (~15MB)
│   ├── build.sh                      # 解压→Configure→make→install 到 ./install/
│   └── .gitignore                    # 排除 openssl-3.5.0/ 解压目录与 install/
├── openblas/
│   ├── OpenBLAS-0.3.29.tar.gz        # ~25MB
│   ├── build.sh                      # 解压→make TARGET=TSV110→安装头+库到 install/
│   └── .gitignore
├── sleef/
│   ├── sleef-3.9.0.tar.gz            # ~1.8MB
│   ├── build.sh                      # cmake→make→install 到 install/
│   └── .gitignore
├── pocketfft/
│   ├── pocketfft-master.tar.gz       # ~15KB
│   └── .gitignore                    # 无需 build.sh（header + 单 .c 直接编进测试库）
tests/cpu/
├── openblas_gemm/
│   ├── dgemm.cpp                     # Task: openblas_dgemm
│   ├── sgemm.cpp                     # openblas_sgemm
│   └── zgemm.cpp                     # openblas_zgemm
├── sleef/
│   ├── sleef_neon.cpp                # sleef_neon（NEON 超越函数，本机可跑）
│   └── sleef_sve.cpp                 # sleef_sve（SVE，编译过/运行时 skip）
├── pocketfft/
│   └── fft.cpp                       # pocketfft_fft（实+复 FFT 往返）
└── isa-l/igzip.cpp                   # isal_igzip（压缩往返，挂到现有 crc/isa-l 构建块）
docs/superpowers/plans/2026-09-15-third-party-sdc-libs.md   # 本计划
README.md                              # 各 Task 同步用例计数
docs/offline-build-dependencies.md    # P0 更新依赖章节
```

OpenSSL 测试复用现有 `tests/cpu/openssl/openssl_sha.cpp` + `tests/cpu/ipsec/`（框架已有的 SSL 管线），P0 只改构建默认值，无新测试文件。

---

### Task 1: 引入 OpenSSL 3.5.0 源码 + install 脚本（P0 第一步）

**Files:**
- Create: `third-party/openssl/openssl-3.5.0.tar.gz`（从 /tmp/dep-probe/openssl.tar.gz 拷入，即 codeload github openssl/openssl tag openssl-3.5.0）
- Create: `third-party/openssl/build.sh`
- Create: `third-party/openssl/.gitignore`

**Interfaces:**
- Produces: `third-party/openssl/install/lib/libcrypto.a`、`third-party/openssl/install/include/openssl/*.h`（后续 meson `declare_dependency` 消费的固定路径）；`install/include/openssl/configuration.h` 等配置头。

- [x] **Step 1: 拷入 tarball 并写 .gitignore**

```bash
mkdir -p third-party/openssl
cp /tmp/dep-probe/openssl.tar.gz third-party/openssl/openssl-3.5.0.tar.gz
cat > third-party/openssl/.gitignore <<'EOF'
openssl-3.5.0/
install/
EOF
```

- [x] **Step 2: 写 build.sh**

```bash
cat > third-party/openssl/build.sh <<'EOF'
#!/bin/bash
# Build OpenSSL 3.5.0 as a static libcrypto for SDCShield (aarch64).
# Produces: install/lib/libcrypto.a + install/include/openssl/
# Rationale: no-apps/no-tests trims the build to just the library we link;
# no-shared gives a self-contained static archive (no runtime .so dependency).
# Probe-verified 2026-09-15: configure ~15s + build ~15s at -j64 on Kunpeng 920.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libcrypto.a ] && { echo "install/ already present"; exit 0; }
tar xzf openssl-3.5.0.tar.gz
cd openssl-3.5.0
./Configure linux-aarch64 --prefix="$PWD/../install" \
    no-shared no-tests no-docs no-apps no-legacy --release
make -j"$JOBS" build_sw
make install_sw
echo "OK: $(ls -la ../install/lib/libcrypto.a)"
EOF
chmod +x third-party/openssl/build.sh
```

注：`no-legacy` 减体积（DEPRECATED 引擎/rmd160 等，测试不用）；如后续 ipsec 测试需要 legacy provider 再去掉（留注释）。若 `no-legacy` 导致 Configure 失败（探针未测此项），去掉该 flag 重试——**先在探针目录验证**：

```bash
cd /tmp/dep-probe/probe/openssl-openssl-3.5.0 && make clean 2>/dev/null; ./Configure linux-aarch64 --prefix=/tmp/dep-probe/ossl-2 no-shared no-tests no-docs no-apps no-legacy --release && make -j64 build_sw >/dev/null 2>&1 && echo NOLEGACY_OK || echo NOLEGACY_FAIL
```

- [x] **Step 3: 实跑 build.sh 验证**

```bash
./third-party/openssl/build.sh
```
预期输出：`OK: ... install/lib/libcrypto.a`（~6-8MB）。验证头文件与版本：

```bash
ls third-party/openssl/install/include/openssl/ | head
third-party/openssl/install/lib/libcrypto.a 存在
strings third-party/openssl/install/lib/libcrypto.a | grep -m1 "OpenSSL 3.5.0"
```

- [x] **Step 4: 验证与框架链接兼容（最小 smoke：系统 LD 下能否 nm 到符号）**

```bash
nm third-party/openssl/install/lib/libcrypto.a | grep -m2 "T EVP_DigestInit_ex\|T SHA256_Update"
```
预期：两个符号都出现（`openssl_sha.cpp` 经 `s_EVP_*` 间接调用的底层就在这）。

- [x] **Step 5: Commit**

```bash
git checkout -b feat/third-party-sdc-libs   # 若分支已存在则跳过
git add third-party/openssl/
git commit -m "third-party: vendor OpenSSL 3.5.0 source + static build script

Introduces third-party/openssl/ with the upstream 3.5.0 tarball and a
build.sh producing a self-contained static libcrypto.a (no-shared,
no-apps/no-tests/no-docs trimmed) under install/. Part of the SDC
dependency hardening plan (docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md §7):
hash/crypto kernels are the literature-confirmed lowest-masking SDC
probe payloads (ITHICA: OpenSSL second-highest fleet detections)."
```

---

### Task 2: meson 切换到 vendored OpenSSL 并默认启用（P0 第二步）

**Files:**
- Modify: `framework/meson.build:212-233`（SSL 查找块）
- Modify: `meson_options.txt:ssl_link_type` 默认值 `'none'` → `'dynamic'`
- Modify: `CLAUDE.md`（构建说明段：ssl 已默认启用、vendored 源码位置）
- Modify: `docs/offline-build-dependencies.md`（§1 层表与 §2.2 可选包：openssl 改为"源码自带"）

**Interfaces:**
- Consumes: Task 1 的 `third-party/openssl/install/{lib/libcrypto.a,include/}`。
- Produces: meson 变量 `crypto_dep`（含 include + link 指向 vendored libcrypto.a），`SANDSTONE_SSL_BUILD=1` 默认生效；`tests/cpu/openssl/openssl_sha.cpp` + 全部 ipsec 测试默认构建。

- [x] **Step 1: 修改 framework/meson.build 的 libcrypto 查找逻辑**

将现有块（`if framework_config.get('SANDSTONE_SSL_BUILD') == 1 ... dependency('libcrypto', ...)`）替换为：优先 vendored、回退系统、都无则禁用的三级查找。保持 `loaded`（dlopen）模式语义不变：

```meson
if framework_config.get('SANDSTONE_SSL_BUILD') == 1
  # Prefer the vendored static libcrypto under third-party/openssl/install
  # (built by third-party/openssl/build.sh); fall back to the system package.
  openssl_install = meson.current_source_dir() / '..' / 'third-party' / 'openssl' / 'install'
  if run_command('test', '-f', openssl_install / 'lib' / 'libcrypto.a', check: false).returncode() == 0
    crypto_dep = declare_dependency(
        include_directories: include_directories(openssl_install / 'include', is_system: true),
        link_args: ['-L' + openssl_install / 'lib', '-l:libcrypto.a'],
    )
    message('Using vendored OpenSSL (third-party/openssl/install)')
  else
    crypto_dep = dependency('libcrypto',
                          version: '>= 3.0',
                          required: false,
                          static: get_option('ssl_link_type') == 'static')
    if not crypto_dep.found()
        # If we cannot find libcrypto, disable SSL build
        framework_config.set10('SANDSTONE_SSL_BUILD', 0)
        message('No vendored or system libcrypto found; SSL tests disabled. Run third-party/openssl/build.sh to enable.')
    endif
  endif
  if crypto_dep.found()
      framework_files += ['sandstone_ssl.cpp', 'sandstone_ssl_rand.cpp']
      if framework_config.get('SANDSTONE_SSL_LINKED') == 0
          # dlopen()-only mode: compile args only, no link
          crypto_dep = crypto_dep.partial_dependency(
              compile_args: true,
              includes: true,
              link_args: false,
              links: false,
          )
      endif
  endif
endif
```

- [x] **Step 2: meson_options.txt 默认值改 dynamic**

```
option('ssl_link_type', type: 'combo', choices : ['none', 'dynamic', 'loaded', 'static'], value: 'dynamic',
```
（保留描述，仅改 value。`dynamic` 此处语义是"链接库"，vendored 场景实际是静态归档链接进二进制——自包含。）

- [x] **Step 3: 全量 reconfigure + 构建验证**

```bash
PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release
ninja -C builddir
```
预期：0 error 0 新 warning；meson 输出含 `Using vendored OpenSSL (third-party/openssl/install)`。

- [x] **Step 4: 功能验证——SSL 测试默认出现且通过**

```bash
./builddir/sdcshield --list-tests | grep -c ipsec        # 预期 ~40+（ipsec 系列全量）
./builddir/sdcshield --list-tests | grep openssl_sha     # 预期 1 行
./builddir/sdcshield -e openssl_sha -t 3000 -n 1        # 预期 exit: pass
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse -t 3000 -n 1   # 预期 pass（OpenSSL 后端分发）
```

- [x] **Step 5: 回归 + 自包含验证**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1             # 预期 exit: pass
ldd builddir/sdcshield | grep -c crypto                 # 预期 0（静态归档，无 libcrypto.so 运行时依赖）
nm builddir/sdcshield | grep -m1 "T SHA256_Update"      # 预期命中（静态链接进二进制）
```

- [x] **Step 6: 文档同步（CLAUDE.md + offline-build-dependencies.md）**

CLAUDE.md 构建段追加一行：
```
# OpenSSL is vendored (third-party/openssl) and enabled by default since
# 2026-09-15; run ./third-party/openssl/build.sh once before first build.
# If the install dir is absent, meson falls back to system libcrypto (or
# disables SSL tests with a message).
```
docs/offline-build-dependencies.md：§1 层表 C 行 openssl 改为"**源码自带**（third-party/openssl，build.sh 一次构建）"；§2.2 可选包表中 `openssl-devel` 行改为说明 vendored 默认启用、系统包仅作回退。

- [x] **Step 7: Commit**

```bash
git add framework/meson.build meson_options.txt CLAUDE.md docs/offline-build-dependencies.md
git commit -m "build: default-enable SSL tests via vendored OpenSSL

ssl_link_type default none->dynamic; framework libcrypto lookup now
prefers the vendored static archive under third-party/openssl/install
(meson message on fallback/disable). openssl_sha + ipsec suite build
by default; binary stays self-contained (no runtime libcrypto.so dep)."
```

---

### Task 3: 引入 OpenBLAS 0.3.29 源码 + install 脚本

**Files:**
- Create: `third-party/openblas/OpenBLAS-0.3.29.tar.gz`（/tmp/dep-probe/openblas-full.tgz）
- Create: `third-party/openblas/build.sh`
- Create: `third-party/openblas/.gitignore`

**Interfaces:**
- Produces: `third-party/openblas/install/lib/libopenblas_tsv110-r0.3.29.a`（符号链接 `libopenblas.a` 指向它）、`install/include/{cblas.h,openblas_config.h}`。

- [x] **Step 1: 拷入 tarball + .gitignore**

```bash
mkdir -p third-party/openblas
cp /tmp/dep-probe/openblas-full.tgz third-party/openblas/OpenBLAS-0.3.29.tar.gz
cat > third-party/openblas/.gitignore <<'EOF'
OpenBLAS-0.3.29/
install/
EOF
```

- [x] **Step 2: 写 build.sh**

```bash
cat > third-party/openblas/build.sh <<'EOF'
#!/bin/bash
# Build OpenBLAS 0.3.29 for SDCShield SDC stress testing (aarch64).
# TARGET=TSV110: HiSilicon TaiShan v110 microarchitecture (Kunpeng 920,
# implementer 0x48 part 0xd01). USE_THREAD=0: single-threaded on purpose —
# the SDCShield framework provides one worker thread per core; library-internal
# threading would only add nondeterministic reduction order (false positives).
# NOFORTRAN=1: no Fortran compiler on the host; C LAPACK is included, BLAS
# F77-mangled symbols (dgemm_ etc.) are still exported.
# Probe-verified 2026-09-15: ~2min at -j64, cblas_{s,d,z}gemm present.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libopenblas.a ] && { echo "install/ already present"; exit 0; }
tar xzf OpenBLAS-0.3.29.tar.gz
cd OpenBLAS-0.3.29
make -j"$JOBS" TARGET=TSV110 USE_THREAD=0 NO_SHARED=1 NOFORTRAN=1 NO_AFFINITY=1
# install targets: headers (cblas.h + generated openblas_config.h) + static lib
make PREFIX="$PWD/../install" install
cd ..
# canonical name for meson
[ -f install/lib/libopenblas_tsv110-r0.3.29.a ] && \
  ln -sf libopenblas_tsv110-r0.3.29.a install/lib/libopenblas.a
echo "OK: $(ls -la install/lib/libopenblas.a)"
EOF
chmod +x third-party/openblas/build.sh
```

- [x] **Step 3: 实跑验证**

```bash
./third-party/openblas/build.sh
```
预期：`OK: ... install/lib/libopenblas.a`（~24MB）。检查头文件与符号：

```bash
ls third-party/openblas/install/include/        # cblas.h openblas_config.h
nm third-party/openblas/install/lib/libopenblas.a | grep -cE "T (cblas_dgemm|cblas_sgemm|cblas_zgemm|dgemm_|sgemm_|zgemm_)$"   # 预期 6
```

- [x] **Step 4: 确认单线程性（无 pthread 依赖）**

```bash
nm third-party/openblas/install/lib/libopenblas.a | grep -c "pthread_create"   # 预期 0（USE_THREAD=0）
ldd 依赖检查：静态库无运行时依赖，跳过
```

- [x] **Step 5: Commit**

```bash
git add third-party/openblas/
git commit -m "third-party: vendor OpenBLAS 0.3.29 source + TSV110 static build script

Single-threaded (USE_THREAD=0) static build for the HiSilicon TSV110
core (Kunpeng 920). GEMM micro-kernels are hand-written NEON fmla
sequences — the literature-confirmed #1 SDC source (SEVI: >92% of
vector SDC incidents are FMA). Framework provides per-core workers."
```

---

### Task 4: openblas_dgemm 测试（P1 第一变体）

**Files:**
- Create: `tests/cpu/openblas_gemm/dgemm.cpp`
- Modify: `tests/cpu/meson.build`（新增 openblas 块，位于 zlib 块之后）

**Interfaces:**
- Consumes: Task 3 的 `third-party/openblas/install/`；框架 `TEST_LOOP`（`sandstone.h`）、`random64()`（框架 RNG，替代 rand）、`memcmp_or_fail`。
- Produces: 测试 ID `openblas_dgemm`；meson 变量 `openblas_dep`（后续 sgemm/zgemm 复用）；目录 `tests/cpu/openblas_gemm/`。

- [x] **Step 1: 写 meson 依赖块（tests/cpu/meson.build，zlib 块后插入）**

```meson
# OpenBLAS GEMM tests (vendored under third-party/openblas, built by its
# build.sh). Hand-written NEON fmla micro-kernels: the highest-density
# vector-FMA payload available, targeting the literature-confirmed #1 SDC
# source. Single-threaded library (USE_THREAD=0): the framework provides
# one worker per core. Copy->compute->verify per iteration (double14 pattern,
# docs/CORE179_SDC_REPORT_CN.md: the most frequent trigger structure).
openblas_install = meson.current_source_dir() / '../../third-party/openblas/install'
if host_machine.cpu_family() == 'aarch64' and run_command('test', '-f',
        openblas_install / 'lib/libopenblas.a', check: false).returncode() == 0
    openblas_dep = declare_dependency(
        include_directories: include_directories(openblas_install / 'include', is_system: true),
        link_args: ['-L' + openblas_install / 'lib', '-l:libopenblas.a'],
    )
    tests_set_base.add(
        when : openblas_dep,
        if_true : files(
            'openblas_gemm/dgemm.cpp',
        )
    )
else
    openblas_dep = disabler()
endif
```

注：`-l:libopenblas.a` 精确指定文件名（avoid 撞系统 libopenblas）。`tests_base_a` 的 `dependencies:` 列表追加 `openblas_dep`（找到现有 `dependencies: [tests_config_base.dependencies(), boost_dep, atomic_lib, isal_lib]` 行追加）。

- [x] **Step 2: 写 dgemm.cpp（完整代码）**

```cpp
/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b openblas_dgemm
 * @parblock
 * Stress test for the vector FMA units using OpenBLAS's hand-written NEON
 * dgemm micro-kernel (TSV110). Literature identifies vector FMA as the #1
 * SDC source (SEVI ASPLOS'26: >92% of vector SDC incidents; Veritas HPCA'25:
 * vector units orders of magnitude above scalar). Each iteration copies the
 * inputs, computes C = A*B, and byte-compares against the golden product
 * from init (copy->compute->verify, the most frequent CORE179 trigger
 * structure). A third scheduling sample beside Eigen/ACL GEMM.
 * @endparblock
 */

#include <sandstone.h>

#include <cblas.h>

#include <string.h>
#include <stdlib.h>

#define M_DIM 256

namespace {
struct gemm_test_data {
    double *a;
    double *b;
    double *golden;      /* C = A*B computed once in init */
    double *a_copy;
    double *b_copy;
    double *c;
};
}

#define CAST(_x) static_cast<struct gemm_test_data *>(_x)

static int openblas_dgemm_init(struct test *test) {
    auto d = new(gemm_test_data);
    test->data = d;
    size_t n2 = M_DIM * M_DIM;
    d->a      = (double *)malloc(n2 * sizeof(double));
    d->b      = (double *)malloc(n2 * sizeof(double));
    d->golden = (double *)malloc(n2 * sizeof(double));
    d->a_copy = (double *)malloc(n2 * sizeof(double));
    d->b_copy = (double *)malloc(n2 * sizeof(double));
    d->c      = (double *)malloc(n2 * sizeof(double));
    if (!d->a || !d->b || !d->golden || !d->a_copy || !d->b_copy || !d->c) {
        report_fail_msg("OOM allocating %zu bytes", n2 * sizeof(double) * 6);
    }
    /* high-entropy random operands (framework RNG; libc rand is trapped) */
    for (size_t i = 0; i < n2; ++i) {
        uint64_t r = random64();
        double x;
        memcpy(&x, &r, sizeof(x));
        /* keep magnitudes in a safe range to avoid inf/nan pollution */
        d->a[i] = (x / 1.0e30) * 1.0e-1;
        r = random64();
        memcpy(&x, &r, sizeof(x));
        d->b[i] = (x / 1.0e30) * 1.0e-1;
    }
    cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                M_DIM, M_DIM, M_DIM,
                1.0, d->a, M_DIM, d->b, M_DIM,
                0.0, d->golden, M_DIM);
    return EXIT_SUCCESS;
}

static int openblas_dgemm_run(struct test *test, int cpu) {
    auto d = CAST(test->data);
    TEST_LOOP(test, i) {
        /* copy-in (dirties cache lines, then reloads them in the kernel) */
        memcpy(d->a_copy, d->a, M_DIM * M_DIM * sizeof(double));
        memcpy(d->b_copy, d->b, M_DIM * M_DIM * sizeof(double));

        cblas_dgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
                    M_DIM, M_DIM, M_DIM,
                    1.0, d->a_copy, M_DIM, d->b_copy, M_DIM,
                    0.0, d->c, M_DIM);

        /* verify-out: byte-exact golden compare of inputs and product */
        if (memcmp(d->a_copy, d->a, M_DIM * M_DIM * sizeof(double)) != 0) {
            report_fail_msg("input A corrupted after GEMM (iteration %ld)", (long)i);
        }
        if (memcmp(d->b_copy, d->b, M_DIM * M_DIM * sizeof(double)) != 0) {
            report_fail_msg("input B corrupted after GEMM (iteration %ld)", (long)i);
        }
        memcmp_or_fail(d->c, d->golden, M_DIM * M_DIM * sizeof(double));
    }
    return EXIT_SUCCESS;
}

static int openblas_dgemm_cleanup(struct test *test) {
    auto d = CAST(test->data);
    free(d->a); free(d->b); free(d->golden);
    free(d->a_copy); free(d->b_copy); free(d->c);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(openblas_dgemm, "OpenBLAS DGEMM (NEON FMA micro-kernel, copy/compute/verify per iteration)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = openblas_dgemm_init,
  .test_run = openblas_dgemm_run,
  .test_cleanup = openblas_dgemm_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
```

注：`TEST_LOOP(test, i)` 是框架标准定时循环（内部 `test_time_condition`）；如果 sandstone.h 的 TEST_LOOP 签名不带迭代变量，改用 `do { ... } while (test_time_condition(test));`（参照 double14.cpp 模式）。**写代码前先 grep 确认**：`grep -n "define TEST_LOOP" framework/sandstone.h`。

- [x] **Step 3: 构建验证**

```bash
ninja -C builddir
```
预期：0 error 0 新 warning。

- [x] **Step 4: 功能验证**

```bash
./builddir/sdcshield --list-tests | grep openblas_dgemm    # 1 行
./builddir/sdcshield -e openblas_dgemm -t 5000 -n 1       # exit: pass
./builddir/sdcshield -e openblas_dgemm -t 5000            # 全核, exit: pass
```

- [x] **Step 5: 确定性验证（逐位复现门槛）**

```bash
./builddir/sdcshield -e openblas_dgemm -t 5000 -n 1 -s LCG:12345   # pass
./builddir/sdcshield -e openblas_dgemm -t 5000 -n 1 -s LCG:12345   # 再跑一次, pass（同 seed 同结果）
```
（若同 seed 复跑 fail，则存在非确定源，必须先修复——通常是 NaN/Inf 污染或未初始化内存。）

- [x] **Step 6: 回归**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1               # exit: pass
./builddir/sdcshield -e eigen_gemm_double_dynamic_square -t 5000 -n 1   # exit: pass（eigen 不受影响）
```

- [x] **Step 7: Commit**

```bash
git add tests/cpu/openblas_gemm/dgemm.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openblas_dgemm — NEON FMA GEMM SDC stress

Copy->compute->verify per iteration against an init-time golden product;
high-entropy operands from the framework RNG; byte-exact memcmp. The
third GEMM instruction-scheduling sample beside Eigen/ACL (CORE179 probe
H/X: instruction scheduling phase diversity is detection rate)."
```

---

### Task 5: openblas_sgemm 测试

**Files:**
- Create: `tests/cpu/openblas_gemm/sgemm.cpp`
- Modify: `tests/cpu/meson.build`（openblas 块 files 列表加一行）

**Interfaces:**
- Consumes: Task 4 的 `openblas_dep` 与 meson 块（仅往 `if_true: files(...)` 加 `'openblas_gemm/sgemm.cpp'`）。
- Produces: 测试 ID `openblas_sgemm`。

- [x] **Step 1: 写 sgemm.cpp**

与 Task 4 的 dgemm.cpp 结构完全一致，差异点：
- `struct` 名 `sgemm_test_data`、函数前缀 `openblas_sgemm_`；
- 缓冲类型 `float`；
- 缩放因子段：`d->a[i] = (x / 1.0e38f) * 1.0e-1f;`（float 安全域）；
- 调用 `cblas_sgemm(...)`；
- `DECLARE_TEST(openblas_sgemm, "OpenBLAS SGEMM (NEON FMA micro-kernel, single precision, copy/compute/verify per iteration)")`。

（完整代码同 Task 4 Step 2 模板逐行替换类型与函数名，不省略。）

- [x] **Step 2: meson files 列表追加**

在 Task 4 建立的 `if_true : files('openblas_gemm/dgemm.cpp')` 里加 `'openblas_gemm/sgemm.cpp',`。

- [x] **Step 3: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep openblas_sgemm
./builddir/sdcshield -e openblas_sgemm -t 5000 -n 1       # exit: pass
./builddir/sdcshield -e openblas_sgemm -t 5000            # 全核 pass
```

- [x] **Step 4: 回归 + Commit**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1               # exit: pass
git add tests/cpu/openblas_gemm/sgemm.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openblas_sgemm — single-precision NEON FMA GEMM SDC stress"
```

---

### Task 6: openblas_zgemm 测试（复数 = cdouble 覆盖）

**Files:**
- Create: `tests/cpu/openblas_gemm/zgemm.cpp`
- Modify: `tests/cpu/meson.build`（files 列表加 zgemm）

**Interfaces:**
- Consumes: Task 4 的 `openblas_dep`。
- Produces: 测试 ID `openblas_zgemm`。

- [x] **Step 1: 写 zgemm.cpp**

结构同 dgemm.cpp，差异：
- 用 `cblas_zgemm` + `void *alpha=&ONE, *beta=&ZERO`（zgemm 的标量是 `const void*`，用 `const double ONE[2] = {1.0, 0.0}, ZERO[2] = {0.0, 0.0};`）；
- 数据为 interleaved double 对（re,im），`n2 = 2*M_DIM*M_DIM` 个 double；
- 随机初始化循环一次填 re、一次填 im（各从独立 `random64()` 缩放）；
- `DECLARE_TEST(openblas_zgemm, "OpenBLAS ZGEMM (complex double NEON FMA, copy/compute/verify per iteration)")`。

cblas_zgemm 调用形态（精确）：
```cpp
static const cblas_complex_double ONE  = {1.0, 0.0};   /* cblas.h 提供 cblas_complex_double */
static const cblas_complex_double ZERO = {0.0, 0.0};
cblas_zgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans,
            M_DIM, M_DIM, M_DIM,
            &ONE, d->a, M_DIM, d->b, M_DIM,
            &ZERO, d->golden, M_DIM);
```

- [x] **Step 2: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield -e openblas_zgemm -t 5000 -n 1     # exit: pass
./builddir/sdcshield -e openblas_zgemm -t 5000          # 全核 pass
```

- [x] **Step 3: 回归 + Commit**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1
git add tests/cpu/openblas_gemm/zgemm.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openblas_zgemm — complex-double NEON FMA GEMM SDC stress

Complex GEMM decomposes into 4 real GEMMs (CORE179 §3.7: cdouble is the
lowest-rate but confirmed-triggering eigen path; this widens complex-path
coverage with a different scheduling sample)."
```

---

### Task 7: 引入 SLEEF 3.9.0 + sleef_neon 测试（可本机验证）

**Files:**
- Create: `third-party/sleef/sleef-3.9.0.tar.gz`（/tmp/dep-probe/sleef.tgz）
- Create: `third-party/sleef/build.sh`
- Create: `third-party/sleef/.gitignore`
- Create: `tests/cpu/sleef/sleef_neon.cpp`
- Modify: `tests/cpu/meson.build`

**Interfaces:**
- Produces: `third-party/sleef/install/{lib/libsleef.a,include/sleef.h}`；测试 ID `sleef_neon`；meson 变量 `sleef_dep`（Task 8 复用）。

- [x] **Step 1: tarball + .gitignore + build.sh**

```bash
mkdir -p third-party/sleef
cp /tmp/dep-probe/sleef.tgz third-party/sleef/sleef-3.9.0.tar.gz
cat > third-party/sleef/.gitignore <<'EOF'
sleef-3.9.0/
build/
install/
EOF
cat > third-party/sleef/build.sh <<'EOF'
#!/bin/bash
# Build SLEEF 3.9.0 (vectorized transcendental functions) for SDCShield.
# SLEEF_ENABLE_TLFLOAT=OFF is mandatory: the TLFloat submodule's pinned git
# tag is not fetchable offline (quad-precision support is not needed here).
# Probe-verified 2026-09-15: ~12s at -j64; libsleef.a exports 108 _u10advsimd
# (NEON) + 108 _u10sve (SVE) function families.
set -euo pipefail
cd "$(dirname "$0")"
JOBS=${JOBS:-$(nproc)}
[ -f install/lib/libsleef.a ] && { echo "install/ already present"; exit 0; }
tar xzf sleef-3.9.0.tar.gz
cmake -S sleef-3.9.0 -B build \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DSLEEF_ENABLE_TLFLOAT=OFF \
    -DCMAKE_INSTALL_PREFIX="$PWD/install" \
    -DSLEEF_ENABLE_NUMA=OFF
cmake --build build -j"$JOBS"
cmake --install build
echo "OK: $(ls -la install/lib/libsleef.a)"
EOF
chmod +x third-party/sleef/build.sh
./third-party/sleef/build.sh
```
预期：`OK: ... install/lib/libsleef.a`；`grep -c "Sleef_sind2_u10advsimd" <(nm install/lib/libsleef.a)` ≥ 1。

- [x] **Step 2: meson 块（tests/cpu/meson.build，openblas 块后）**

```meson
# SLEEF vectorized transcendental functions (vendored third-party/sleef).
# Polynomial FMA chains over NEON/SVE wide vectors; pure functions make a
# perfect golden-compare payload. Covers the newest-instruction-generation
# risk area (PinDrop Obs12: instructions fail most in their first arch gen).
sleef_install = meson.current_source_dir() / '../../third-party/sleef/install'
if host_machine.cpu_family() == 'aarch64' and run_command('test', '-f',
        sleef_install / 'lib/libsleef.a', check: false).returncode() == 0
    sleef_dep = declare_dependency(
        include_directories: include_directories(sleef_install / 'include', is_system: true),
        link_args: ['-L' + sleef_install / 'lib', '-l:libsleef.a'],
    )
    tests_set_base.add(
        when : sleef_dep,
        if_true : files(
            'sleef/sleef_neon.cpp',
        )
    )
else
    sleef_dep = disabler()
endif
```
`tests_base_a` 的 `dependencies:` 追加 `sleef_dep`。

- [x] **Step 3: 写 sleef_neon.cpp（完整代码）**

```cpp
/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sleef_neon
 * @parblock
 * Stress test for NEON data paths via SLEEF's vectorized transcendental
 * functions (sin/cos/exp/log, double & float, u10/u35 accuracy variants).
 * Long polynomial FMA dependency chains; results only feed a byte-exact
 * golden compare (data-flow payload: faults surface as wrong values, not
 * crashes — From Gates to SDCs DATE'25). Pure functions => deterministic
 * golden values computed once in init.
 * @endparblock
 */

#include <sandstone.h>

#include <sleef.h>

#include <arm_neon.h>
#include <string.h>
#include <stdlib.h>

#define ELEMS 1024   /* lanes: 512 float64x2 + 512 float32x4 (mixed widths) */

namespace {
struct sleef_test_data {
    /* inputs */
    double *xd;  float *xf;
    /* golden outputs */
    double *gd_sin, *gd_cos, *gd_exp, *gd_log;
    float  *gf_sin, *gf_cos, *gf_exp, *gf_log;
    /* scratch */
    double *od;  float *of;
};
}

#define CAST(_x) static_cast<struct sleef_test_data *>(_x)

static int sleef_neon_init(struct test *test) {
    auto d = new(sleef_test_data);
    test->data = d;
    d->xd = (double *)malloc(ELEMS * sizeof(double));
    d->xf = (float  *)malloc(ELEMS * sizeof(float));
    d->gd_sin = (double *)malloc(ELEMS * sizeof(double));
    d->gd_cos = (double *)malloc(ELEMS * sizeof(double));
    d->gd_exp = (double *)malloc(ELEMS * sizeof(double));
    d->gd_log = (double *)malloc(ELEMS * sizeof(double));
    d->gf_sin = (float  *)malloc(ELEMS * sizeof(float));
    d->gf_cos = (float  *)malloc(ELEMS * sizeof(float));
    d->gf_exp = (float  *)malloc(ELEMS * sizeof(float));
    d->gf_log = (float  *)malloc(ELEMS * sizeof(float));
    d->od = (double *)malloc(ELEMS * sizeof(double));
    d->of = (float  *)malloc(ELEMS * sizeof(float));
    if (!d->xd || !d->xf || !d->gd_sin || !d->od || !d->of) {
        report_fail_msg("OOM in sleef_neon init");
    }
    /* Inputs in the functions' principal domains (SLEEF u10/u35 guarantee
     * max 1.0/3.5 ULP error inside them; outside, results may differ
     * between builds but stay deterministic for a fixed binary):
     *   sin/cos: [-pi, pi] mapped from full-range random
     *   exp    : [-20, 20]
     *   log    : (0, huge] via positive random (uniform mantissa)
     */
    for (int i = 0; i < ELEMS; ++i) {
        uint64_t r = random64();
        double u = (double)(r >> 11) / 9007199254740992.0;  /* [0,1) */
        d->xd[i] = (u * 2.0 - 1.0) * 3.141592653589793;     /* [-pi,pi] */
        uint64_t r2 = random64();
        double v = (double)(r2 >> 11) / 9007199254740992.0;
        d->xf[i] = (float)((v * 2.0f - 1.0f) * 3.14159265358979f);
    }
    /* golden via the same SLEEF kernels, once */
    for (int i = 0; i < ELEMS; i += 2) {
        float64x2_t x2 = vld1q_f64(&d->xd[i]);
        vst1q_f64(&d->gd_sin[i], Sleef_sind2_u10advsimd(x2));
        vst1q_f64(&d->gd_cos[i], Sleef_cosd2_u10advsimd(x2));
    }
    for (int i = 0; i < ELEMS; ++i) {
        uint64_t r = random64();
        double u = (double)(r >> 11) / 9007199254740992.0;
        d->xd[i] = (u * 2.0 - 1.0) * 20.0;                  /* [-20,20] */
    }
    for (int i = 0; i < ELEMS; i += 2) {
        float64x2_t x2 = vld1q_f64(&d->xd[i]);
        vst1q_f64(&d->gd_exp[i], Sleef_expd2_u10advsimd(x2));
    }
    for (int i = 0; i < ELEMS; ++i) {
        uint64_t r = random64();
        double u = (double)(r >> 11) / 9007199254740992.0 + 1e-300;
        d->xd[i] = u * 1.0e10;                              /* positive */
    }
    for (int i = 0; i < ELEMS; i += 2) {
        float64x2_t x2 = vld1q_f64(&d->xd[i]);
        vst1q_f64(&d->gd_log[i], Sleef_logd2_u10advsimd(x2));
    }
    /* float golden (float32x4) */
    for (int i = 0; i < ELEMS; i += 4) {
        float32x4_t x4 = vld1q_f32(&d->xf[i]);
        vst1q_f32(&d->gf_sin[i], Sleef_sinf4_u10advsimd(x4));
        vst1q_f32(&d->gf_cos[i], Sleef_cosf4_u10advsimd(x4));
    }
    for (int i = 0; i < ELEMS; ++i) {
        uint64_t r = random64();
        float u = (float)((double)(r >> 11) / 9007199254740992.0);
        d->xf[i] = (u * 2.0f - 1.0f) * 20.0f;
    }
    for (int i = 0; i < ELEMS; i += 4) {
        float32x4_t x4 = vld1q_f32(&d->xf[i]);
        vst1q_f32(&d->gf_exp[i], Sleef_expf4_u10advsimd(x4));
    }
    for (int i = 0; i < ELEMS; ++i) {
        uint64_t r = random64();
        float u = (float)((double)(r >> 11) / 9007199254740992.0) + 1e-30f;
        d->xf[i] = u * 1.0e10f;
    }
    for (int i = 0; i < ELEMS; i += 4) {
        float32x4_t x4 = vld1q_f32(&d->xf[i]);
        vst1q_f32(&d->gf_log[i], Sleef_logf4_u10advsimd(x4));
    }
    return EXIT_SUCCESS;
}

static int sleef_neon_run(struct test *test, int cpu) {
    auto d = CAST(test->data);
    TEST_LOOP(test, i) {
        /* sin/cos double */
        for (int j = 0; j < ELEMS; j += 2) {
            float64x2_t x2 = vld1q_f64(&d->xd[j]);       /* reload each iter */
            vst1q_f64(&d->od[j], Sleef_sind2_u10advsimd(x2));
        }
        /* verify against golden: recompute golden inputs — inputs are
         * stored per-domain; we regenerate the domain inputs the same way.
         * (Simplest correct structure: store the per-domain input arrays.)
         */
        ...
    }
    return EXIT_SUCCESS;
}
```

**⚠ 结构修正（写实现时采纳）**：上面 run 需要每个函数域的输入独立存储（sin/cos 域、exp 域、log 域各一份），否则 verify 时无法重放输入。正确结构：init 里为每个函数族保存独立输入数组 `xd_trig/xd_exp/xd_log`（double）与 `xf_trig/xf_exp/xf_log`（float），run 里逐族 计算→memcmp_or_fail(od, gd_*, ELEMS*sizeof)。内存：3×8KB + 3×4KB + golden 4×8KB + 4×4KB + scratch ≈ 150KB，在 L2 内、部分 L1——符合"足迹旋钮"设计。**实现时以这个修正结构为准**（上面的单数组版本会在 verify 阶段踩错输入域——这是计划评审发现的 bug，留在计划里警示执行者）。

- [x] **Step 4: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep sleef_neon
./builddir/sdcshield -e sleef_neon -t 5000 -n 1       # exit: pass
./builddir/sdcshield -e sleef_neon -t 5000            # 全核 pass
./builddir/sdcshield -e sleef_neon -t 5000 -n 1 -s LCG:12345   # 同 seed 复跑仍 pass（确定性）
```

- [x] **Step 5: 回归 + Commit**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1
git add third-party/sleef/ tests/cpu/sleef/sleef_neon.cpp tests/cpu/meson.build
git commit -m "third-party: vendor SLEEF 3.9.0 + tests/arm64: add sleef_neon

SLEEF vectorized transcendentals as an SDC payload: polynomial FMA chains
over NEON. TLFloat submodule disabled (offline-unreachable pinned tag).
Golden values from the same kernels in init; per-domain input arrays;
byte-exact memcmp per iteration."
```

---

### Task 8: sleef_sve 测试（SVE 变体；本机编译通过、运行时 skip）

**Files:**
- Create: `tests/cpu/sleef/sleef_sve.cpp`
- Modify: `tests/cpu/meson.build`（SVE 构建块：`tests_set_sve.add(...)` + 需要为 SVE 库补 sleef 链接）

**Interfaces:**
- Consumes: Task 7 的 `third-party/sleef/install/`（SVE 符号 `Sleef_sindx_u10sve(svfloat64_t)` 已在 libsleef.a，探针实测 108 族）。
- Produces: 测试 ID `sleef_sve`。

- [x] **Step 1: 写 sleef_sve.cpp**

结构同 Task 7 但用 SVE API。**独立 SVE 静态库**（镜像 `tests_sve_a` 既有模式，`-march=armv8.2-a+sve -msve-vector-bits=128`），不进 NEON 基线库（GCC 会把其他测试自动向量化成 SVE → 无 SVE 机器 SIGILL）。关键差异：

```cpp
#include <arm_sve.h>
#include <sleef.h>

/* runtime gate identical to eigen_svd_cdouble_sve */
static int sleef_sve_init(struct test *test) {
    if (!device_has_feature(cpu_feature_sve)) {
        log_skip("CpuNotSupported", "sleef_sve requires SVE (e.g. Kunpeng 930)");
        return EXIT_SKIP;
    }
    ...
}
/* SVE kernel call: svfloat64_t Sleef_sindx_u10sve(svfloat64_t);
 * fixed vector length 128-bit via -msve-vector-bits=128 (svfloat64_t =
 * 2 lanes) — matches the existing tests_sve_a convention. */
```
（完整代码按 Task 7 修正结构实现：per-domain 输入数组、golden 一次、run 逐族 `Sleef_*dx_u10sve(svld1_f64(...))` + memcmp。）

- [x] **Step 2: meson：tests_set_sve 加文件 + SVE 库依赖加 sleef**

`tests_set_sve.add(when: sleef_dep, if_true: files('sleef/sleef_sve.cpp'))`；
既有 `tests_sve_a` static_library 的 `dependencies:` 列表（当前 `[tests_config_sve.dependencies(), boost_dep]`）追加 `sleef_dep`。

- [x] **Step 3: 构建 + 本机验证（预期 skip）**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep sleef_sve          # 1 行
./builddir/sdcshield --quality=-1 -e sleef_sve -t 2000      # result: skip, CpuNotSupported（本机无 SVE）
```

- [x] **Step 4: 回归 + Commit**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1
git add tests/cpu/sleef/sleef_sve.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add sleef_sve — SVE transcendental SDC stress

Compiled with -march=armv8.2-a+sve -msve-vector-bits=128 in the existing
tests_sve static library; runtime HWCAP_SVE gate returns EXIT_SKIP on
non-SVE silicon (Kunpeng 920). Real execution requires SVE hardware
(Kunpeng 930 / Neoverse) — documented limitation."
```

---

### Task 9: isal_igzip 压缩往返测试（P2，零新依赖）

**Files:**
- Create: `tests/cpu/isa-l/igzip.cpp`（新目录；现有 isa-l CRC 测试在 tests/cpu/crc/）
- Modify: `tests/cpu/meson.build`（isal 块 files 加 igzip.cpp）

**Interfaces:**
- Consumes: 系统 libisal（既有 `isal_lib = cpp.find_library('isal', ...)` gate，`/usr/include/isa-l/igzip_lib.h` 实测存在）；框架 RNG。
- Produces: 测试 ID `isal_igzip`。

- [x] **Step 1: 写 igzip.cpp（完整代码）**

```cpp
/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b isal_igzip
 * @parblock
 * Compression round-trip via isa-l igzip (stateless deflate -> inflate ->
 * byte-compare). Match search (data-dependent branches) + high-density
 * stores + Huffman entropy coding: a third compression scheduling sample
 * beside zstd/zlib (ITHICA: Zlib was the highest-yield fleet SDC detector;
 * CORE179 probe H/X: scheduling diversity is detection rate). Verifies both
 * directions: decompressed data must match the original input exactly, and
 * a fixed-input re-compression must reproduce the identical deflate stream
 * (determinism check on the compressor's data path).
 * @endparblock
 */

#include <sandstone.h>

#include <igzip_lib.h>

#include <string.h>
#include <stdlib.h>

#define IN_SIZE   (256 * 1024)
#define OUT_SIZE  (IN_SIZE + IN_SIZE / 2)

namespace {
struct igzip_test_data {
    uint8_t *input;
    uint8_t *comp_golden;
    uint8_t *comp;
    uint8_t *decomp;
    size_t golden_comp_size;
};
}

#define CAST(_x) static_cast<struct igzip_test_data *>(_x)

static int isal_igzip_init(struct test *test) {
    auto d = new(igzip_test_data);
    test->data = d;
    d->input       = (uint8_t *)malloc(IN_SIZE);
    d->comp_golden = (uint8_t *)malloc(OUT_SIZE);
    d->comp        = (uint8_t *)malloc(OUT_SIZE);
    d->decomp      = (uint8_t *)malloc(IN_SIZE);
    if (!d->input || !d->comp_golden || !d->comp || !d->decomp) {
        report_fail_msg("OOM in isal_igzip init");
    }
    /* High-entropy random input PLUS compressible structure: alternate
     * random blocks with runs/zeros so the match finder does real work
     * (pure random is incompressible and skips the match logic). */
    for (size_t i = 0; i < IN_SIZE; ++i) {
        if ((i & 0x3FFF) < 0x2000) {
            d->input[i] = (uint8_t)(i & 0x0F);      /* short-period pattern */
        } else {
            d->input[i] = (uint8_t)random32();      /* random */
        }
    }
    /* golden compression, once */
    struct isal_zstream stream;
    isal_deflate_stateless_init(&stream);
    stream.end_of_stream = 1;
    stream.flush = NO_FLUSH;
    stream.next_in = d->input;
    stream.avail_in = IN_SIZE;
    stream.next_out = d->comp_golden;
    stream.avail_out = OUT_SIZE;
    stream.level = 1;                 /* fastest level still exercises matches */
    if (isal_deflate_stateless(&stream) != COMP_OK) {
        report_fail_msg("golden isal_deflate_stateless failed");
    }
    d->golden_comp_size = stream.total_out;
    return EXIT_SUCCESS;
}

static int isal_igzip_run(struct test *test, int cpu) {
    auto d = CAST(test->data);
    TEST_LOOP(test, i) {
        /* compress the same input again: must reproduce the identical stream */
        struct isal_zstream stream;
        isal_deflate_stateless_init(&stream);
        stream.end_of_stream = 1;
        stream.flush = NO_FLUSH;
        stream.next_in = d->input;
        stream.avail_in = IN_SIZE;
        stream.next_out = d->comp;
        stream.avail_out = OUT_SIZE;
        stream.level = 1;
        if (isal_deflate_stateless(&stream) != COMP_OK) {
            report_fail_msg("isal_deflate_stateless failed (iteration %ld)", (long)i);
        }
        if (stream.total_out != d->golden_comp_size) {
            report_fail_msg("compressed size %zu != golden %zu (iteration %ld)",
                            (size_t)stream.total_out, d->golden_comp_size, (long)i);
        }
        memcmp_or_fail(d->comp, d->comp_golden, d->golden_comp_size);

        /* decompress round-trip: must reproduce the input exactly */
        struct inflate_state inf;
        memset(&inf, 0, sizeof(inf));
        inf.next_in = d->comp;
        inf.avail_in = d->golden_comp_size;
        inf.next_out = d->decomp;
        inf.avail_out = IN_SIZE;
        if (isal_inflate(&inf) != ISAL_DECOMP_OK) {
            report_fail_msg("isal_inflate failed (iteration %ld)", (long)i);
        }
        if (inf.total_out != IN_SIZE) {
            report_fail_msg("decompressed size %zu != %d", (size_t)inf.total_out, IN_SIZE);
        }
        memcmp_or_fail(d->decomp, d->input, IN_SIZE);
    }
    return EXIT_SUCCESS;
}

static int isal_igzip_cleanup(struct test *test) {
    auto d = CAST(test->data);
    free(d->input); free(d->comp_golden); free(d->comp); free(d->decomp);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(isal_igzip, "isa-l igzip deflate/inflate round-trip SDC stress")
  .groups = DECLARE_TEST_GROUPS(&group_compression),
  .test_init = isal_igzip_init,
  .test_run = isal_igzip_run,
  .test_cleanup = isal_igzip_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
```

注：`group_compression` 组名需先确认存在（`grep -rn "group_compression\|group_compress" framework/sandstone_test_groups.cpp`）——若不存在用现有压缩测试（zstd/test.c）所属的组名。`isal_deflate_stateless` 的 `stream.level` 字段与 `NO_FLUSH`/`COMP_OK`/`ISAL_DECOMP_OK` 常量名以 `/usr/include/isa-l/igzip_lib.h` 实测为准（写码前 grep 确认）。

- [x] **Step 2: meson 挂载**

现有 `isal_lib` gate 的 `if_true : files(...)` 列表（crc/isal_*.cpp 那组）追加 `'isa-l/igzip.cpp',`。

- [x] **Step 3: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep isal_igzip
./builddir/sdcshield -e isal_igzip -t 5000 -n 1       # exit: pass
./builddir/sdcshield -e isal_igzip -t 5000            # 全核 pass
```

- [x] **Step 4: 回归 + Commit**

```bash
./builddir/sdcshield -e isal_crc_ieee -t 3000 -n 1    # 既有 isal 测试回归 pass
./builddir/sdcshield -e zstd19 -t 3000 -n 1
git add tests/cpu/isa-l/igzip.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add isal_igzip — isa-l deflate/inflate round-trip SDC stress

Third compression scheduling sample beside zstd/zlib. Verifies both the
compressed-stream determinism (re-compression byte-identical) and the
decompression round-trip against the original input."
```

---

### Task 10: 引入 pocketfft + pocketfft_fft 测试（P4）

**Files:**
- Create: `third-party/pocketfft/pocketfft-master.tar.gz`（/tmp/dep-probe/pocketfft.tgz）
- Create: `third-party/pocketfft/.gitignore`
- Create: `tests/cpu/pocketfft/fft.cpp`
- Modify: `tests/cpu/meson.build`

**Interfaces:**
- Produces: 测试 ID `pocketfft_fft`。pocketfft 无预编译步骤（`.c` 直接编进测试静态库，`pocketfft.h` include 路径指向解压目录）。

- [x] **Step 1: tarball + 解压约定**

```bash
mkdir -p third-party/pocketfft
cp /tmp/dep-probe/pocketfft.tgz third-party/pocketfft/pocketfft-master.tar.gz
cat > third-party/pocketfft/.gitignore <<'EOF'
pocketfft-master/
EOF
# 解压由 meson 构建时做？不行——meson 不该改 source tree。
# 决策：提交解压后的 pocketfft.h + pocketfft.c 两个文件（共 ~150KB 源码，BSD-3），
# tarball 保留作为溯源证据。LICENSE.md 一并提交。
tar xzf third-party/pocketfft/pocketfft-master.tar.gz -C third-party/pocketfft/
git add -f third-party/pocketfft/pocketfft-master/pocketfft.h \
          third-party/pocketfft/pocketfft-master/pocketfft.c \
          third-party/pocketfft/pocketfft-master/LICENSE.md
```
（修订 .gitignore 为只忽略 tarball 解压出的其余文件：`pocketfft-master/*` 但 `!pocketfft-master/pocketfft.h` 等三文件——用 git add -f 更简单，.gitignore 保持 `pocketfft-master/` 并显式 -f 添加三个文件。）

- [x] **Step 2: meson 块**

```meson
# pocketfft (vendored third-party/pocketfft, BSD-3, header+single .c).
# FFT butterflies (twiddle FMA chains) + bit-reversal permutation
# (store->load scatter pressure — the CORE179-discriminating access
# structure) + multi-pass iterative amplification.
pocketfft_dir = meson.current_source_dir() / '../../third-party/pocketfft/pocketfft-master'
if host_machine.cpu_family() == 'aarch64' and run_command('test', '-f',
        pocketfft_dir / 'pocketfft.h', check: false).returncode() == 0
    pocketfft_files = files(pocketfft_dir / 'pocketfft.c')  # 编进 tests_base_a
    pocketfft_dep = declare_dependency(
        include_directories: include_directories(pocketfft_dir, is_system: true),
    )
    tests_set_base.add(
        when : pocketfft_dep,
        if_true : files(
            'pocketfft/fft.cpp',
            pocketfft_files,
        )
    )
else
    pocketfft_dep = disabler()
endif
```
`tests_base_a` 的 `dependencies:` 追加 `pocketfft_dep`。

- [x] **Step 3: 写 fft.cpp（完整代码）**

```cpp
/**
 * @file
 *
 * @copyright
 * Copyright 2026 ISCAS.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b pocketfft_fft
 * @parblock
 * FFT stress via pocketfft (BSD-3, the SciPy FFT engine): forward cfft
 * (twiddle-FMA butterflies + bit-reversal permutation, i.e. store->load
 * scatter pressure) followed by inverse cfft; the round-trip result must
 * reproduce the input byte-exactly after the 1/N scaling. Additionally the
 * forward spectrum is byte-compared against the init-time golden spectrum
 * (deterministic for fixed input + fixed binary).
 * @endparblock
 */

#include <sandstone.h>

#include <pocketfft.h>

#include <string.h>
#include <stdlib.h>
#include <math.h>

#define FFT_N 4096   /* power of 2; 4096 doubles = 32KB working set (L1/L2 boundary) */

namespace {
struct fft_test_data {
    double *input;       /* interleaved re,im — length 2*FFT_N */
    double *golden_spec;
    double *work;
};
}

#define CAST(_x) static_cast<struct fft_test_data *>(_x)

static int pocketfft_fft_init(struct test *test) {
    auto d = new(fft_test_data);
    test->data = d;
    d->input      = (double *)malloc(2 * FFT_N * sizeof(double));
    d->golden_spec = (double *)malloc(2 * FFT_N * sizeof(double));
    d->work       = (double *)malloc(2 * FFT_N * sizeof(double));
    if (!d->input || !d->golden_spec || !d->work) {
        report_fail_msg("OOM in pocketfft_fft init");
    }
    for (int i = 0; i < 2 * FFT_N; ++i) {
        uint64_t r = random64();
        double u = (double)(r >> 11) / 9007199254740992.0;
        d->input[i] = (u * 2.0 - 1.0) * 1.0;   /* [-1,1) both re & im */
    }
    cfft_plan plan = make_cfft_plan(FFT_N);
    memcpy(d->golden_spec, d->input, 2 * FFT_N * sizeof(double));
    cfft_forward(plan, d->golden_spec, 1.0);
    destroy_cfft_plan(plan);
    return EXIT_SUCCESS;
}

static int pocketfft_fft_run(struct test *test, int cpu) {
    auto d = CAST(test->data);
    TEST_LOOP(test, i) {
        cfft_plan plan = make_cfft_plan(FFT_N);
        /* forward: spectrum must be byte-identical to golden */
        memcpy(d->work, d->input, 2 * FFT_N * sizeof(double));
        cfft_forward(plan, d->work, 1.0);
        memcmp_or_fail(d->work, d->golden_spec, 2 * FFT_N * sizeof(double));
        /* inverse with 1/N scaling: must reproduce the input byte-exactly */
        cfft_backward(plan, d->work, 1.0 / FFT_N);
        memcmp_or_fail(d->work, d->input, 2 * FFT_N * sizeof(double));
        destroy_cfft_plan(plan);
    }
    return EXIT_SUCCESS;
}

static int pocketfft_fft_cleanup(struct test *test) {
    auto d = CAST(test->data);
    free(d->input); free(d->golden_spec); free(d->work);
    delete d;
    return EXIT_SUCCESS;
}

DECLARE_TEST(pocketfft_fft, "pocketfft complex FFT round-trip SDC stress (bit-reversal scatter + twiddle FMA)")
  .groups = DECLARE_TEST_GROUPS(&group_math),
  .test_init = pocketfft_fft_init,
  .test_run = pocketfft_fft_run,
  .test_cleanup = pocketfft_fft_cleanup,
  .fracture_loop_count = 4,
  .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST
```

注：`cfft_forward/backward` 原地操作 `double c[]`（interleaved）。**计划评审警示**：forward→backward 往返的浮点舍入**不保证**逐位回到输入（FFT round-trip 有 O(ε·log N) 误差）——**第二个 memcmp 可能天然 fail**。写实现时先本机实测往返误差；若非零（几乎必然），去掉第二个 memcmp，只保留 golden spectrum 比对（第一个 memcmp 已足够：它就是"同输入同二进制逐位一致"的确定性检查）。往返比较改为对 N 的缩放校验（`cfft_backward(forward(x), 1/N) ≈ x` 在容差内）**不作 SDC 判据**，仅 log。**SDC 判据唯一来源：golden spectrum 字节精确 memcmp。**

- [x] **Step 4: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep pocketfft_fft
./builddir/sdcshield -e pocketfft_fft -t 5000 -n 1    # exit: pass
./builddir/sdcshield -e pocketfft_fft -t 5000         # 全核 pass
```

- [x] **Step 5: 回归 + Commit**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1
git add third-party/pocketfft/ tests/cpu/pocketfft/fft.cpp tests/cpu/meson.build
git commit -m "third-party: vendor pocketfft + tests/arm64: add pocketfft_fft

Bit-reversal permutation stresses store->load scatter (the CORE179
discriminating structure); twiddle-FMA butterflies stress the vector FP
multiply-add path. SDC verdict: byte-exact golden-spectrum memcmp only
(round-trip is not bit-exact due to FFT rounding; documented)."
```

---

### Task 11: README + 文档全量同步 + 全套终验

**Files:**
- Modify: `README.md`（用例计数、新增依赖说明、SDC 研究依据链接）
- Modify: `docs/offline-build-dependencies.md`（四个新 third-party 条目 + 构建顺序说明）
- Modify: `CLAUDE.md`（third-party 目录清单更新）

- [x] **Step 1: 全量重新构建（干净 builddir）**

```bash
rm -rf builddir
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
ninja -C builddir
```

- [x] **Step 2: 新测试全量验证**

```bash
./builddir/sdcshield --list-tests | wc -l                          # 计数（记录实际值，README 用）
./builddir/sdcshield -e openblas_dgemm -e openblas_sgemm -e openblas_zgemm -t 5000 -n 1   # 3 pass
./builddir/sdcshield -e sleef_neon -t 5000 -n 1                    # pass
./builddir/sdcshield --quality=-1 -e sleef_sve -t 2000             # skip (CpuNotSupported)
./builddir/sdcshield -e isal_igzip -t 5000 -n 1                    # pass
./builddir/sdcshield -e pocketfft_fft -t 5000 -n 1                 # pass
./builddir/sdcshield -e openssl_sha -t 3000 -n 1                   # pass
```

- [x] **Step 3: 全核压力终验（关键——SDC 工具必须全核跑）**

```bash
./builddir/sdcshield -e openblas_dgemm,openblas_sgemm,openblas_zgemm,sleef_neon,isal_igzip,pocketfft_fft,openssl_sha -t 30000   # 全核 30s, 全 pass
```

- [x] **Step 4: 回归抽样**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1
./builddir/sdcshield -e eigen_gemm_double_dynamic_square -t 5000 -n 1
./builddir/sdcshield -e isal_crc_ieee -t 3000 -n 1
```

- [x] **Step 5: 文档同步**

README.md：用例计数更新为 `--list-tests` 实测值；"依赖"或测试清单节补一段（OpenBLAS/SLEEF/pocketfft vendored 源码 + build.sh 一次性构建 + 触发 SSL 默认启用）；引用 `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md` 作为选型依据。
docs/offline-build-dependencies.md：§1 依赖分层表补 third-party 自建库行；§2 说明首次构建需依次跑三个 build.sh（openssl/openblas/sleef）。
CLAUDE.md：third-party 段补三个新目录一句话说明。

- [x] **Step 6: Commit**

```bash
git add README.md docs/offline-build-dependencies.md CLAUDE.md
git commit -m "docs: sync README/CLAUDE/offline-build docs for vendored SDC dependency suite

Test count from live --list-tests; build order (3x build.sh then meson);
selection rationale linked to docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md."
```

---

## 验证总表（每个 Task 完成后勾选）

| Task | 验证命令 | 预期 |
|---|---|---|
| 1 | `./third-party/openssl/build.sh` + nm 符号 | install/lib/libcrypto.a + EVP/SHA 符号 |
| 2 | reconfigure+ninja + `-e openssl_sha` + ldd 无 libcrypto.so | pass + 自包含 |
| 3 | `./third-party/openblas/build.sh` + nm 6 符号 | libopenblas.a + 无 pthread_create |
| 4 | `-e openblas_dgemm -t 5000` 单核/全核 | pass ×2 |
| 5 | `-e openblas_sgemm -t 5000` | pass ×2 |
| 6 | `-e openblas_zgemm -t 5000` | pass ×2 |
| 7 | `-e sleef_neon -t 5000` + 同 seed 复跑 | pass + 确定性 |
| 8 | `--quality=-1 -e sleef_sve -t 2000` | skip (无 SVE) |
| 9 | `-e isal_igzip -t 5000` | pass ×2 |
| 10 | `-e pocketfft_fft -t 5000` | pass ×2 |
| 11 | 全量 list-tests + 7 测试全核 30s | 计数正确 + 全 pass |

## 诚实性边界声明

1. **SVE 测试（Task 8）本机无法运行验证**——只能验证编译通过 + 干净 skip。真实验证需要 SVE 硬件。计划的验证步骤如实写为预期 skip。
2. **SDC 检出率提升是统计命题**——本计划交付的是"更高覆盖的负载组合"，其检出率优势来自文献（31 篇）而非本机可复现实验；CORE179 是唯一本机实测锚点。
3. **每个测试的同 seed 确定性验证（如 Task 4 Step 5）是必要非充分条件**——它排除非确定源，但不能证明跨编译器/跨机器可复现（后者靠 CI 多版本构建保证，不在本计划范围）。
4. tarball 来源：OpenSSL/OpenBLAS/SLEEF/pocketfft 均为 codeload.github.com 官方 tag（2026-09-15 下载，gzip -t 校验完整）。上游 hash 未另行记录（tarball 本身入仓即溯源）。

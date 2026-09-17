# SDC 敏感单元最大化压力覆盖实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use Markdown task-list checkboxes for tracking (live plan = single source of truth for done vs pending).

**Goal:** 把 vendored 库（OpenBLAS/SLEEF/OpenSSL/pocketfft/isa-l）从"单一入口采样"扩展为"SDC 敏感单元最大化压力"覆盖：每个库补齐文献点名的正交代码路径（不同算法内核 = 不同指令调度相位 = 不同缺陷检出面），并把所有新旋钮纳入统一战役脚本。

**Architecture:** 严格沿用现有四种已验证模式，零框架改动：(1) 新测试文件模式（sleef_neon.cpp 骨架：init 算 golden → run 重放 → 字节精确 memcmp + 非有限哨兵）；(2) test knob 模式（`get_testspecific_knob_value_int(test, "key", default)`，openblas mdim 先例）；(3) shim 扩展模式（`framework/sandstone_ssl.h` 宏列表加 `F(EVP_sm4_*)`，静态链接路径自动可用）；(4) 战役脚本模式（scripts/run/ 现有风格）。每任务一个 commit、自验证后推送。

**Tech Stack:** C++17 + C（测试），meson（构建，sourceset 机制），bash（战役脚本），vendored 静态库（OpenBLAS 0.3.29 TSV110 单线程 / SLEEF 3.9.0 / OpenSSL 3.5.0 / pocketfft C / 系统 libisal）。

**Spec:** 本计划 §依据 即 spec（文献结论均引自 `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md` 31 篇综合 + README 战役模式章节）。符号名全部经 `nm --defined-only` 于 2026-09-16 实测确认存在于 vendored 静态库。

## 依据（为什么是这些改进 — 文献 → 缺口 → 动作）

| # | 文献结论（综合文档 §7.2/7.3） | 当前缺口（实测） | 本计划动作 |
|---|---|---|---|
| A | 优先级 2"哈希/加密"点名 SHA-2**/3**、 avalanche 位扩散是兜底 | SHA-3/SHAKE 符号在 libcrypto.a 里存在（`EVP_sha3_224..512`、`EVP_shake128/256`、`EVP_DigestFinalXOF` 均 nm 实测在），shim 宏也已声明，但零测试 | Task 1: `openssl_sha3` 新测试（Keccak 置换 = AND/旋转/χθ 步，与 SHA-2 加法链正交的 FU 混合） |
| B | 优先级 4"SVE 超越函数"+ PinDrop Obs12 新指令代际风险 | SLEEF 451 个 advsimd + 451 个 sve 符号只用了 12 个；无 u35 第二精度档、无 float SVE、无反/双曲函数族 | Task 2/3: u35 档 + 新函数族进现有两测试；Task 4: `nelems` 足迹旋钮（当前固定 1024 元素 ≈30KB 永驻 L1，违背"footprint 全谱"原则 §7.3-6） |
| C | 优先级 1 GEMM + §7.3-5"同数学多调度" | 三个 gemm 全是 `NoTrans,α=1,β=0` 单一形态；TransA/B 走 OpenBLAS 不同 packing 例程、β≠0 命中 C 读改写路径；complex-float `cgemm` 零覆盖 | Task 5: `transab`/`beta` 旋钮；Task 6: `openblas_cgemm` |
| D | 优先级 7 FFT"scatter 正中 CORE179 判别条件"；反推荐自适应库 | pocketfft 只测 N=4096（2^12 单一因子分解）；质数 N 触发 Bluestein（FFT→卷积，完全不同调度）；rfft 零覆盖 | Task 7: `pocketfft_fft` 加 `n=` 旋钮（2^k 与质数双谱系）+ rfft 前向 golden 比对 |
| E | 优先级 3 压缩 + 多样性=检出率 | igzip 只测 stateless level 1 | Task 8: `level=` 旋钮（0/1/2/3，level 2/3 需 `level_buf`，尺寸常量 `ISAL_DEF_LVL2_REQ`/`ISAL_DEF_LVL3_REQ` 实测于 /usr/include/isa-l/igzip_lib.h:278-283） |
| F | 文献盲区自知（IRF/LSQ）；LAPACKE 2605 符号零覆盖 | LU 主元交换 = 数据依赖分支 + 换行 store（GEMM 没有的结构）；Cholesky = 纯 FMA 下三角链 | Task 9: `openblas_lu`（dgesv，kernel 自验证：分解因子回代残差重算比较——文献"每 bit 参与最终比对"的可复现形态） |
| G | 国密自然适配（国产 ARM 检测工具） | `EVP_sm3` shim 已声明、`EVP_sm4_*` 在 libcrypto.a nm 实测存在但 shim 未声明 | Task 10: `openssl_sm3sm4`（sm3 摘要 + sm4-cbc 加解密往返） |
| H | §7.4 战役协议"两阶段 + 持续化" | README 三种战役模式只覆盖 GEMM | Task 11: `scripts/run/run_sdc_spectrum.sh` 统一扫谱脚本 + README/文档同步 |

**明确不做**（YAGNI / 文献反推荐）：FFTW（GPL 冲突）、多线程 BLAS SMP 模式（非确定归约）、运行时自适应库、PMC 哨兵（辅助信号，主判据永远是 golden memcmp，框架级改动超出本计划"零框架改动"边界）。

## Global Constraints

- **一补丁一单元**：每个 Task 恰好一个 commit（Task 11 含脚本+README+docs 为同一"战役框架"单元），按 Task 顺序串行执行，前一个 verify→commit→push 完成后才开工下一个。
- **每 commit 前自验证 100% 真实**：`ninja -C builddir` 零新增 warning/error；`-e <新测试> -t 5000 -n 1` 输出 `result: pass`（或 SVE 在本机 Kunpeng 920 上为干净 skip）并引用真实输出；回归 `-e zstd19 -t 3000 -n 1` pass。跳过任何验证 = 禁止 commit。
- **x86-64 零改动**：所有新测试仅入 aarch64 guard 的 sourceset 分支（`host_machine.cpu_family() == 'aarch64'`）或沿用现有 `#if SANDSTONE_SSL_BUILD` 模式；x86 构建路径文件零触碰。
- **golden 模式统一**：init 一次算 golden（同一二进制同一输入 → 位一致）→ run 每迭代重放 → 字节精确 `memcmp_or_fail` + 非有限值 init 期 fail-loudly 哨兵（SEVI：exponent/符号位也翻，字节精确是必须的）。
- **随机数只用框架 RNG**（`random64()`/`random32()`，libc rand 被框架 trap）。
- **线程纪律**：run 期写入的缓冲全部 `thread_local` + 懒分配 + 故意不 free（fork 子进程退出即回收，openblas_dgemm 先例）；库调用并发安全性：OpenBLAS 已 `USE_LOCKING=1`、pocketfft plan 只读、SLEEF/OpenSSL EVP 纯函数/每调用独立 CTX、isal stateless 每调用独立 stream。
- **knob 纪律**：`-O <testid>.<key>=N` 必须带测试 ID 前缀；越界 fail-loudly（`report_fail_msg` 提示合法域，mdim 先例 dgemm.cpp:73-75）；默认值 = 历史行为零变化。
- **commit message 不以 `Co-Authored-By: Claude` 结尾**（CLAUDE.md 规则）；每个 commit 后 `git push` 到远端分支 `feat/sdc-coverage-max`。
- **大颗粒度修改同步 README.md + docs**（CLAUDE.md 验证第 5 条）：Task 11 集中做测试表/战役模式/测试计数同步（各 Task 的计数增减记入 Task 11 的更新清单，避免中途 README 与现实脱节——但每个 Task commit message 里记录新增测试 ID）。
- **工作区纪律**：开工前从 `feat/third-party-sdc-libs` HEAD 切新分支 `feat/sdc-coverage-max`；未跟踪文件 `docs/superpowers/plans/2026-09-16-memcpy-rewr-default-mode.md` 属他人在途工作，**绝不 `git add -A`/`git add .`，只用显式路径 add**。
- SLEEF 静态库符号名已 nm 实测确认：u35 advsimd 缺 `Sleef_expd2_u35advsimd`/`Sleef_expf4_u35advsimd`（**exp 的 u35 NEON 档不存在**），但 `exp2`/`exp10`/`sin`/`cos`/`log`/`tanh`/`pow`/`asin`/`atan`/`cbrt` 的 u35 advsimd 均存在；SVE 侧 `sindx/cosdx/logdx/expdx/sinfx/cosfx/logfx/expfx` 的 u10+u35 全存在。计划中的函数清单以此为准，**不得写未实测存在的符号**。

---

### Task 1: `openssl_sha3` — SHA-3/SHAKE 位扩散压力测试

**Files:**
- Create: `tests/cpu/openssl/openssl_sha3.cpp`
- Modify: `tests/cpu/meson.build`（`SANDSTONE_SSL_BUILD` aarch64+crypto_dep 分支，`'openssl/openssl_sha.cpp',` 行后加 `'openssl/openssl_sha3.cpp',`，约 :272）

**Interfaces:**
- Consumes: `s_EVP_MD_CTX_new`/`s_EVP_get_digestbyname`/`s_EVP_DigestInit_ex`/`s_EVP_DigestUpdate`/`s_EVP_DigestFinal_ex`/`s_EVP_DigestFinalXOF`（shim 已声明，sandstone_ssl.h:827 实测）、`random64()`、`memcmp_or_fail`、`TEST_LOOP`。
- Produces: 测试 ID `openssl_sha3`（Task 11 README 表新增行引用）。

**依据**: Keccak-f[1600] 置换（θ/ρ/π/χ/ι 步）是 AND-XOR-旋转数据通路，与 SHA-2 的模 2^64 加法链正交；SHAKE XOF 是可变长输出扩散。OpenSSL 3.5 aarch64 后端有优化 Keccak 实现。文献 §7.2 优先级 2 明确点名 "SHA-2/3"。

- [x] **Step 1: 写测试文件**

结构完全照 `openssl_sha.cpp` 模式（`#if SANDSTONE_SSL_BUILD` … `#else` skip 桩 … `#endif` + DECLARE_TEST）。核心差异：算法集 = sha3_224/256/384/512（定长，走 `EVP_get_digestbyname("sha3-256")` 等）+ shake128（XOF，输出 32 字节，`s_EVP_DigestFinalXOF(ctx, out, 32)`）。golden 元素结构：

```cpp
#define PLAINTEXT_SIZE (512UL)
#define SHA3_GOLDEN_ELEMS (256UL)
#define SHA3_MAX_OFFSET (512UL)
#define SHA3_DIGEST_MAX (64UL)   /* sha3-512 = 64B; shake128 XOF 输出取 32B */

struct sha3_elem {
    uint8_t plain_text[PLAINTEXT_SIZE];
    uint8_t sha3_224sum[28];
    uint8_t sha3_256sum[32];
    uint8_t sha3_384sum[48];
    uint8_t sha3_512sum[64];
    uint8_t shake128sum[32];
};
```

init：`memset_random` 填 256 个 golden 元素，逐个算 5 种摘要存入元素；run：TEST_LOOP(test, 128) 每迭代随机挑 golden_idx、mmap 偏移复刻 plaintext、重算 5 种摘要、逐字段 `memcmp_or_fail`（消息字符串 "sha3-224sum values does not match." 等）。所有 EVP 调用走 `s_` shim 前缀，`md_len` 用 `unsigned int`。skip 桩照抄 openssl_sha.cpp:157-165（`log_skip(TestResourceIssueSkipCategory, ...)` + `EXIT_SKIP`）。`DECLARE_TEST(openssl_sha3, ...)` 无 `.groups`（同 openssl_sha 先例）、`.quality_level = TEST_QUALITY_PROD`、`.fracture_loop_count = 4`。

- [x] **Step 2: meson 挂线**

`tests/cpu/meson.build` aarch64 `SANDSTONE_SSL_BUILD == 1` 分支 `'openssl/openssl_sha.cpp',` 之后加一行 `'openssl/openssl_sha3.cpp',`。

- [x] **Step 3: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep openssl_sha3     # 预期: openssl_sha3
./builddir/sdcshield -e openssl_sha3 -t 5000 -n 1         # 预期: result: pass
```

- [x] **Step 4: 回归验证**

```bash
./builddir/sdcshield -e openssl_sha -t 3000 -n 1    # 预期: pass（邻接 SSL 测试）
./builddir/sdcshield -e zstd19 -t 3000 -n 1         # 预期: pass
```

- [x] **Step 5: Commit + push**

```bash
git add tests/cpu/openssl/openssl_sha3.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openssl_sha3 — SHA-3/SHAKE Keccak bit-diffusion SDC stress"
git push -u origin feat/sdc-coverage-max
```

---

### Task 2: `sleef_neon` u35 第二精度档 + 新函数族

**Files:**
- Modify: `tests/cpu/sleef/sleef_neon.cpp`

**Interfaces:**
- Consumes: 现有 `sleef_test_data` 结构、`uniform01()`、`thread_local od/of`。
- Produces: 新增 golden 数组字段（Task 3 复用同一结构扩展 SVE 侧）；新 knob 键 `nelems`（Task 4 同名键）。

**依据**: u10 与 u35 是 SLEEF 的两套独立多项式实现（不同系数、不同 FMA 链调度）= §7.3-5"同数学多调度"的免费第二相位样本。新函数族符号全部 nm 实测存在：`Sleef_asind2_u35advsimd`/`Sleef_asinf4_u35advsimd`、`Sleef_atand2_u35advsimd`/`Sleef_atanf4_u35advsimd`、`Sleef_cbrtd2_u35advsimd`/`Sleef_cbrtf4_u35advsimd`、`Sleef_powd2_u10advsimd`/`Sleef_powf4_u10advsimd`（sleef.h:583/743 签名实测）、`Sleef_tanhd2_u35advsimd`/`Sleef_tanhf4_u35advsimd`、`Sleef_sinhd2_u35advsimd`/`Sleef_sinhf4_u35advsimd`、`Sleef_log10d2_u10advsimd`/`Sleef_log10f4_u10advsimd`。**注意：exp 的 u35 NEON 不存在，u35 档用 exp2**（`Sleef_exp2d2_u35advsimd`/`Sleef_exp2f4_u35advsimd`）。

- [x] **Step 1: 扩展数据结构**

`sleef_test_data` 增加字段（u35 档 golden + 新族输入/golden）：

```cpp
/* u35 second-accuracy tier (independent polynomial chains) */
double *gd_sin35, *gd_cos35, *gd_log35, *gd_exp2_35;
float  *gf_sin35, *gf_cos35, *gf_log35, *gf_exp2_35;
/* new function families */
double *xd_inv,  *gd_asin, *gd_atan, *gd_cbrt, *gd_log10, *gd_sinh, *gd_tanh;
double *xd_pow2, *gd_pow;   /* pow: base from xd_inv family range, exponent own array */
float  *xf_inv,  *gf_asin, *gf_atan, *gf_cbrt, *gf_log10, *gf_sinh, *gf_tanh;
float  *xf_pow2, *gf_pow;
```

- [x] **Step 2: init 补输入映射 + golden 计算 + 哨兵**

域映射（全部经边界分析，同现有注释风格写明理由）：
- asin/atan 输入：`u*2-1` ∈ [-1,1)（asin 定义域含端点 -1，u=0 时取 -1 合法；atan 同域）；
- cbrt：`u*2000-1000` ∈ [-1000,1000)（过 0、负数、大数三态，cbrt 定义域全实数）；
- log10：复用 log 的正数映射（[1e-290,1e10)）；
- sinh/tanh：复用 exp 的 [-20,20)（tanh 输出 ∈ (-1,1)，sinh(20)≈2.4e8 有限）；
- pow：底数 `1+u*9` ∈ [1,10)（避开 0^0/负底数分支复杂度），指数 `u*8-4` ∈ [-4,4)（pow(10,4)=1e4 有限；float 档 powf(10,4)=1e4f 有限）。

golden 用同核计算 + 逐元素有限性哨兵（asin/atan ∈ [-π/2,π/2]、tanh ∈ (-1,1)、cbrt ∈ [-1000,1000)、pow ∈ (0,1e4]、sinh ∈ (−1e9,1e9)、log10 ∈ (−700,25]、exp2(20)≈1e6 等，越界 report_fail_msg）。

- [x] **Step 3: run 补重放段**

在现有 8 段（sin/cos/exp/log × double/float）之后追加：u35 四段（sin/cos/log/exp2）、新族七段（asin/atan/cbrt/log10/sinh/tanh/pow），每段"load→kernel→store→`memcmp_or_fail(od/of, golden, ELEMS, "<name>")`"。pow 段两输入数组分别 `vld1q` 后传双参数。

- [x] **Step 4: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield -e sleef_neon -t 5000 -n 1    # 预期: result: pass
```

- [x] **Step 5: 回归验证**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1    # 预期: pass
```

- [x] **Step 6: Commit + push**

```bash
git add tests/cpu/sleef/sleef_neon.cpp
git commit -m "tests/arm64: sleef_neon — add u35 tier + inverse/hyperbolic/pow/cbrt families"
git push
```

---

### Task 3: `sleef_sve` float 变体 + u35 档 + 新族

**Files:**
- Modify: `tests/cpu/sleef/sleef_sve.cpp`

**Interfaces:**
- Consumes: 现有 HWCAP_SVE 探针（getauxval）、`svfloat64_t` 128-bit 定长模式、`svptrue_b64()`。
- Produces: 无外部接口（自包含扩展）。

**依据**: 当前 SVE 只测 double u10 四函数——SVE float 通路（`svfloat32_t` 4 lane）完全未测；SVE 符号 nm 实测全存在：`Sleef_sindx_u35sve`/`Sleef_cosdx_u35sve`/`Sleef_logdx_u35sve`/`Sleef_exp2dx_u35sve`（**注意 exp 的 u35 SVE 也不存在，用 exp2**，`Sleef_exp2dx_u35sve` 实测）、float 侧 `Sleef_sinfx_u10sve`/`Sleef_cosfx_u10sve`/`Sleef_logfx_u10sve`/`Sleef_expfx_u10sve` + u35 `Sleef_sinfx_u35sve`/`Sleef_cosfx_u35sve`/`Sleef_logfx_u35sve`。

- [x] **Step 1: 扩展结构 + init**

`sleef_test_data` 加：`float *xf_trig, *xf_exp, *xf_log;`（输入）+ float golden 四组（u10）+ double u35 golden 四组 + `svfloat32_t` 用 `svptrue_b32()` 定长 4 lane（`-msve-vector-bits=128` 与现有 double 注释同理）。域映射照抄 sleef_neon 的 float 分析（含 CAUTION 注释：narrowing 可达边界值，无害）。哨兵照抄。

- [x] **Step 2: run 补重放段**

现有 4 段（double u10）后追加：float u10 四段（`svld1_f32`/`Sleef_sinfx_u10sve` 等）、double u35 四段（sin/cos/log/exp2）。本机 Kunpeng 920 无 SVE，验证预期是**干净 skip**（CpuNotSupported）。

- [x] **Step 3: 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield -e sleef_sve -t 5000 -n 1
# 预期（本机无 SVE）: skip, 原因 CpuNotSupported "requires SVE"
# 若在 SVE 主机: result: pass
```

- [x] **Step 4: 回归验证**

```bash
./builddir/sdcshield -e sleef_neon -t 3000 -n 1    # 预期: pass（邻接）
./builddir/sdcshield -e zstd19 -t 3000 -n 1         # 预期: pass
```

- [x] **Step 5: Commit + push**

```bash
git add tests/cpu/sleef/sleef_sve.cpp
git commit -m "tests/arm64: sleef_sve — add float32 tier + u35 second-accuracy chains"
git push
```

---

### Task 4: `sleef_neon` `nelems` 足迹旋钮

**Files:**
- Modify: `tests/cpu/sleef/sleef_neon.cpp`

**Interfaces:**
- Consumes: `get_testspecific_knob_value_int(test, "nelems", 1024)`（test_knobs.h:33 签名实测）。
- Produces: knob `-O sleef_neon.nelems=N`（Task 11 战役脚本引用）。

**依据**: 当前 ELEMS=1024 编译期常量 ≈ 30KB 工作集永驻 L1D（64KB），违背 §7.3-6"足迹 L1/L2/LLC/DRAM 全谱"（Biswas：不同 cache 结构的 ACE 驻留不同；mdim 旋钮同一教训的 GEMM 版）。旋钮化后 `nelems=16384` → 每 thread-local 缓冲 ~400KB 进 L2、`nelems=65536` → ~1.6MB 进 LLC。

- [x] **Step 1: ELEMS 改运行期**

- `#define ELEMS 1024` 改为 `#define ELEMS_DEFAULT 1024` + `#define ELEMS_MIN 128`（向量步进 2/4 的公倍数，128 是 4 的倍数）+ `#define ELEMS_MAX 262144`（每线程 double+float scratch + golden 各 2×2MB+1MB ≈ 7MB×192 核上限可控）。
- init 开头：`int64_t knob = get_testspecific_knob_value_int(test, "nelems", ELEMS_DEFAULT);` 越界 `report_fail_msg("nelems knob out of range: %ld (valid %d..%d, default %d)", ...)`（mdim 先例句式）；`d->nelems = (int)knob;` 存入结构体，所有 `ELEMS` 循环边界改读 `d->nelems`。
- **不传 knob = 默认 1024 = 行为零变化**（golden 数组尺寸/内容与历史完全一致——RNG 消费序不变）。

- [x] **Step 2: 构建 + 三档功能验证**

```bash
ninja -C builddir
./builddir/sdcshield -e sleef_neon -t 5000 -n 1                          # 预期: pass（默认 1024）
./builddir/sdcshield -O sleef_neon.nelems=16384 -e sleef_neon -t 5000 -n 1   # 预期: pass（L2 档）
./builddir/sdcshield -O sleef_neon.nelems=262144 -e sleef_neon -t 8000 -n 1  # 预期: pass（LLC/DRAM 档）
./builddir/sdcshield -O sleef_neon.nelems=99 -e sleef_neon -t 3000 -n 1      # 预期: fail-loudly "out of range"
```

- [x] **Step 3: 回归验证**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1    # 预期: pass
```

- [x] **Step 4: Commit + push**

```bash
git add tests/cpu/sleef/sleef_neon.cpp
git commit -m "tests/arm64: sleef_neon footprint knob (-O sleef_neon.nelems=N, 128..262144, default 1024)"
git push
```

---

### Task 5: `openblas_{d,s,z}gemm` `transab`/`beta` 旋钮

**Files:**
- Modify: `tests/cpu/openblas_gemm/dgemm.cpp`、`sgemm.cpp`、`zgemm.cpp`（三个文件同一模式，同一 commit——同一"参数空间"单元）

**Interfaces:**
- Consumes: `get_testspecific_knob_value_int`。
- Produces: knob `-O openblas_dgemm.transab=0..3`（0=NN 默认,1=NT,2=TN,3=TT）、`-O openblas_dgemm.beta_permille=1000`（β=1.0 千分比表示，绕开浮点 knob 解析；默认 0 = 历史行为）。

**依据**: TransA/TransB 在 OpenBLAS 内部走不同 packing 例程（A 列主转置打包 vs B 行主复制）= 不同指令调度；β≠0 命中 C 读改写路径（`C = αAB + βC`，内核读-改-写 C 而非纯写）——两者都是 mdim 之外的真实正交轴。cblas_dgemm 的 TransA/TransB 语义下 A/B 仍是行列可读的方阵，golden 直接以同参数计算，无需预转置。

- [x] **Step 1: dgemm.cpp 加双旋钮（其余两文件同构复制）**

init 段（mdim 解析后）：

```cpp
int64_t transab = get_testspecific_knob_value_int(test, "transab", 0);
if (transab < 0 || transab > 3)
    report_fail_msg("transab knob out of range: %ld (valid 0..3: 0=NN 1=NT 2=TN 3=TT, default 0)", (long)transab);
int64_t beta_pm = get_testspecific_knob_value_int(test, "beta_permille", 0);
if (beta_pm < 0 || beta_pm > 1000000)
    report_fail_msg("beta_permille knob out of range: %ld (valid 0..1000000, default 0; beta=permille/1000)", (long)beta_pm);
```

存入结构体；`CBLAS_TRANSPOSE ta = (transab & 2) ? CblasTrans : CblasNoTrans; tb = (transab & 1) ? CblasTrans : CblasNoTrans;`；β = `beta_pm / 1000.0`。golden 计算与 run 调用都换成 ta/tb/β 三参数。**β≠0 时 C 的初始内容参与结果**——golden 与 run 都先 `memset(c, 0, bytes)` 后 `cblas_*gemm(..., beta, c, ...)`？不对：β≠0 且 C 未初始化会读垃圾。正确做法：golden 与 run 都对 C **预填固定非零模式**（`for i: c[i] = 0.5 * (double)(i & 63) / 64.0;` init 算 golden 前对 `d->golden` 预填、run 对线程 `c` 预填——每迭代 memcpy 这份模式），保证 β 路径输入确定。zgemm 的 β 是 `double BETA[2] = {beta, 0.0}` 数组（ONE/ZERO 先例，zgemm.cpp:66）。init 末尾的 golden 有限性哨兵不变（β≤1000 时 |C| 上界 ~2×旧上界，仍远离 1e300）。

- [x] **Step 2: 构建 + 四象限功能验证**

```bash
ninja -C builddir
./builddir/sdcshield -e openblas_dgemm -t 5000 -n 1                                      # pass（默认 NN,β=0）
./builddir/sdcshield -O openblas_dgemm.transab=3 -O openblas_dgemm.beta_permille=1000 -e openblas_dgemm -t 5000 -n 1   # pass（TT,β=1）
./builddir/sdcshield -O openblas_dgemm.transab=1 -e openblas_dgemm -t 5000 -n 1          # pass（NT）
./builddir/sdcshield -O openblas_dgemm.transab=9 -e openblas_dgemm -t 3000 -n 1          # fail-loudly out of range
# sgemm/zgemm 同样跑 transab=2 + beta_permille=500 一档 + 默认档
```

- [x] **Step 3: 回归验证**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1    # pass
```

- [x] **Step 4: Commit + push**

```bash
git add tests/cpu/openblas_gemm/dgemm.cpp tests/cpu/openblas_gemm/sgemm.cpp tests/cpu/openblas_gemm/zgemm.cpp
git commit -m "tests/arm64: openblas GEMM transab/beta knobs — packing-path and C read-modify-write phase coverage"
git push
```

---

### Task 6: `openblas_cgemm` — complex-float 第四 GEMM 变体

**Files:**
- Create: `tests/cpu/openblas_gemm/cgemm.cpp`
- Modify: `tests/cpu/meson.build`（openblas_dep 分支 `'openblas_gemm/zgemm.cpp',` 后加 `'openblas_gemm/cgemm.cpp',`，约 :471）

**Interfaces:**
- Consumes: `cblas_cgemm`（cblas.h:302 签名实测）、`float[2]` 交错复数约定（zgemm 先例）。
- Produces: 测试 ID `openblas_cgemm`。

**依据**: complex-float（8 字节元素）是第四种 GEMM 内存形态——zgemm 用 `fmla` 成对链，cgemm 的 NEON 微内核每向量 4 复数（16 lane 复用），与 dgemm(2)/sgemm(4)/zgemm(2 复) 的 lane 组织各不相同 = 第四份调度样本（§7.3-5）。

- [x] **Step 1: 写测试文件**

整体复制 `zgemm.cpp`，改动点：`cblas_zgemm`→`cblas_cgemm`；元素类型 double→float（`float *a/b/golden`、`random_bounded` 返回 float：`(float)((double)(r >> 11) / 9007199254740992.0 * 2.0e-3f + 1.0e-6f)` 保持随机符号逻辑）；缓冲 n2 语义变为 `2*mdim*mdim` 个 float；`ONE`/`ZERO`/`BETA` 改 `static const float ONE[2] = {1.0f, 0.0f};`；文件头 @test 块改写为 complex-float 微内核描述；mdim 范围检查与 knob 键名 `mdim`（默认 256）照抄；β 预填模式照抄 Task 5 约定（本文件直接带 transab/beta 旋钮，与 Task 5 同构——新文件一步到位）；有限性哨兵上界 float 版 `1.0e30f`。

- [x] **Step 2: meson 挂线 + 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep openblas_cgemm    # 预期: openblas_cgemm
./builddir/sdcshield -e openblas_cgemm -t 5000 -n 1        # 预期: pass
./builddir/sdcshield -O openblas_cgemm.transab=3 -e openblas_cgemm -t 5000 -n 1   # pass
```

- [x] **Step 3: 回归验证**

```bash
./builddir/sdcshield -e openblas_dgemm -t 3000 -n 1    # pass
./builddir/sdcshield -e zstd19 -t 3000 -n 1             # pass
```

- [x] **Step 4: Commit + push**

```bash
git add tests/cpu/openblas_gemm/cgemm.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openblas_cgemm — complex-float NEON FMA GEMM SDC stress"
git push
```

---

### Task 7: `pocketfft_fft` `n=` 旋钮（Bluestein 质数谱系）+ rfft 前向段

**Files:**
- Modify: `tests/cpu/pocketfft/fft.cpp`

**Interfaces:**
- Consumes: `make_cfft_plan(size_t)`/`cfft_forward`、`make_rfft_plan`/`rfft_forward`（pocketfft.h:28-32 实测）、`get_testspecific_knob_value_int`。
- Produces: knob `-O pocketfft_fft.n=N`。

**依据**: N=4096 只走 2^12 单一因子分解。pocketfft.c:2073 实测：`length<50 || largest_prime_factor(length)<=sqrt(length)` 走 packplan，否则 Bluestein（FFT→卷积化，额外两次复 FFT + 逐点乘 = 完全不同的指令序列）。质数 N（如 4099/8191）强制 Bluestein 路径。rfft（实输入半复 packed 输出）是第三种变换结构。

- [x] **Step 1: n 旋钮 + 允许值表**

- init：`int64_t knob = get_testspecific_knob_value_int(test, "n", 4096);` 合法值白名单（固定表，不是连续区间——FFT 对任意 N 合法但战役谱系要挑有代表性的）：`{512, 1024, 2048, 4096(默认), 8192, 16384, 4099(质数,Bluestein), 8191(质数,Bluestein), 6144(3×2^11,radf3+radf2 混合), 10000(2^4×5^4,radf5)}`，越界/不在表内 `report_fail_msg("n knob invalid: %ld (valid: 512..16384 pow2, 4099/8191 prime-Bluestein, 6144/10000 mixed)", ...)`。
- `FFT_N` 常量改 `d->n` 运行期；所有循环边界跟随。默认 4096 = 行为零变化。
- golden 谱尺寸 `2*d->n` double；有限性哨兵上界随 N 缩放：`|X_k| ≤ N`，上界取 `4.0 * d->n`（实测 max ~142 ≈ N/29，4N 余量充足）。

- [x] **Step 2: rfft 前向段（golden 比对）+ inverse 复用说明**

- init：另建 `rfft_plan rplan = make_rfft_plan(d->n);`、独立随机实输入 `d->rinput`（n double，random_bounded）、golden `d->rgolden`（`rfft_forward(rplan, rgolden, 1.0)` 就地变换；输出为 halfcomplex packed，**n 个 double**——`rfft_length(plan)` 返回 n，缓冲分配 n double）。
- run：memcpy rinput→work_r（thread_local，懒分配）、`rfft_forward`、`memcmp_or_fail(work_r, d->rgolden, d->n, "rfft golden spectrum")`。
- 现有 cfft_backward 纯载荷段保持不比对（注释已论证非位精确）；rfft_backward 同理只跑不比。
- 注：质数 N 时 cfft 与 rfft 都走 Bluestein；`make_*_plan` 对 N<50 会走 packplan——白名单已排除。

- [x] **Step 3: 构建 + 谱系验证**

```bash
ninja -C builddir
./builddir/sdcshield -e pocketfft_fft -t 5000 -n 1                                       # pass（默认 4096）
./builddir/sdcshield -O pocketfft_fft.n=4099 -e pocketfft_fft -t 5000 -n 1                # pass（Bluestein 质数）
./builddir/sdcshield -O pocketfft_fft.n=10000 -e pocketfft_fft -t 5000 -n 1               # pass（radf5 混合）
./builddir/sdcshield -O pocketfft_fft.n=4097 -e pocketfft_fft -t 3000 -n 1                # fail-loudly 不在白名单
```

- [x] **Step 4: 回归验证**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1    # pass
```

- [x] **Step 5: Commit + push**

```bash
git add tests/cpu/pocketfft/fft.cpp
git commit -m "tests/arm64: pocketfft n-knob (pow2/prime-Bluestein/mixed radix) + rfft forward golden compare"
git push
```

---

### Task 8: `isal_igzip` `level=` 旋钮（0..3）

**Files:**
- Modify: `tests/cpu/isa-l/igzip.cpp`

**Interfaces:**
- Consumes: `ISAL_DEF_MAX_LEVEL`(=3)、`ISAL_DEF_LVL2_REQ`/`ISAL_DEF_LVL3_REQ`（igzip_lib.h:275/278/281 实测常量）、`get_testspecific_knob_value_int`。
- Produces: knob `-O isal_igzip.level=N`。

**依据**: level 0 = 静态 Huffman 无 match search；level 1 = 基础 hash 链；level 2 = 更深 hash 历史（`IGZIP_LVL2_HASH_SIZE`）；level 3 = hash map + 长匹配——四档是四套 match-finder 数据结构 = 数据依赖分支 + 整数乘法器负载的四个相位（§7.2 优先级 3）。

- [x] **Step 1: level 旋钮 + level_buf**

- init：`int64_t lvl = get_testspecific_knob_value_int(test, "level", 1);` 越界（<0 或 >`ISAL_DEF_MAX_LEVEL`）fail-loudly；存 `d->level`。
- golden 压缩与 run 重压缩的 stream 设置统一改用 `d->level`；**level 2/3 必须供 level_buf**（isa-l 头文件 :807 文档：stateless 下 level 1 可选、2/3 必需）：
  - 结构体/线程 scratch 各加 `uint8_t *level_buf`，尺寸 = `(d->level == 3) ? ISAL_DEF_LVL3_REQ : (d->level == 2) ? ISAL_DEF_LVL2_REQ : 0`（level 0/1 传 `nullptr`/0——stateless level 1 头文件明说可选）。
  - `stream.level_buf = lb; stream.level_buf_size = size;`（golden 与 run 两侧都要设；run 侧 level_buf 是 thread_local 懒分配，与 comp/decomp 同生命周期故意不 free）。
- 默认 level 1 = 行为零变化。

- [x] **Step 2: 构建 + 四档功能验证**

```bash
ninja -C builddir
for L in 0 1 2 3; do ./builddir/sdcshield -O isal_igzip.level=$L -e isal_igzip -t 5000 -n 1; done   # 全部 pass
./builddir/sdcshield -O isal_igzip.level=4 -e isal_igzip -t 3000 -n 1                                # fail-loudly
```

- [x] **Step 3: 回归验证**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1    # pass
```

- [x] **Step 4: Commit + push**

```bash
git add tests/cpu/isa-l/igzip.cpp
git commit -m "tests/arm64: isal_igzip level knob (0..3) — four match-finder data-structure phases"
git push
```

---

### Task 9: `openblas_lu` — LAPACK dgesv 主元分解压力

**Files:**
- Create: `tests/cpu/openblas_gemm/lu.cpp`
- Modify: `tests/cpu/meson.build`（openblas_dep 分支加 `'openblas_gemm/lu.cpp',`）

**Interfaces:**
- Consumes: `LAPACKE_dgesv(LAPACK_ROW_MAJOR=101, n, nrhs, a, lda, ipiv, b, ldb)`（lapacke.h:872 签名 + lapacke.h:52 布局常量实测）、`cblas_dgemm`（残差重算）。
- Produces: 测试 ID `openblas_lu`。

**依据**: LU 带部分主元 = 每列数据依赖的分支决策（选主元）+ 行交换 store 模式 + 三角回代——GEMM 完全没有的微结构负载。LAPACKE 2605 符号当前零覆盖。**golden 策略**：LU 分解结果依赖主元选择顺序，仍确定（同输入同二进制 → 同主元序列），但"分解因子 + ipiv 数组逐字节比对"之外再加**数学自验证**：`‖P·A − L·U‖` 重算比较——这是"每 bit 参与最终比对"的可复现形态，且同时压 dgemm 残差路径（双负载）。

- [x] **Step 1: 写测试文件**

结构照 dgemm.cpp 骨架：

```cpp
struct lu_test_data {
    int n;              /* -O openblas_lu.n=N, 默认 256, 范围 16..2048（O(n^3) 同 mdim 预算） */
    double *a;          /* 原始矩阵，n^2，read-only（残差重算用） */
    double *lu;         /* golden 分解因子（L 下三角单位对角 + U 上三角打包在同一矩阵） */
    int *ipiv;          /* golden 主元序列, n ints */
    double *b, *x;      /* nrhs=1 右端项 + golden 解 */
};
```

- init：`random_bounded()` 填 a 与 b（值域 [1e-6,2e-3] 保证非奇异概率极高；若 `LAPACKE_dgesv != 0` 或对角出现 0 → `report_fail_msg("singular operand set, rerun" ...)` fail-loudly——随机矩阵奇异概率≈0，不静默重试）。golden：拷贝 a→t、b→x，`LAPACKE_dgesv(LAPACK_ROW_MAJOR, n, 1, t, n, ipiv, x, n)`，t 即 golden lu。哨兵：x 全有限。
- run（每迭代）：memcpy a/b/lua → thread_local `ac`/`bc`/`luc`、`ipivc`；`LAPACKE_dgesv` 于拷贝；**三重比对**：(1) `memcmp_or_fail(luc, golden_lu, n²)`，(2) `memcmp_or_fail(ipivc, golden_ipiv, n·sizeof(int))`——主元序列错 = 分支/比较通路 SDC 的直接证据，(3) `memcmp_or_fail(xc, golden_x, n)`。
- 文件头注释：LU 主元分支 + 行交换 store + 三角回代的微结构论证；`.groups = DECLARE_TEST_GROUPS(&group_math)`、`.fracture_loop_count = 4`、PROD。

- [x] **Step 2: meson 挂线 + 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep openblas_lu    # openblas_lu
./builddir/sdcshield -e openblas_lu -t 5000 -n 1        # pass
./builddir/sdcshield -O openblas_lu.n=64 -e openblas_lu -t 5000 -n 1     # pass
```

- [x] **Step 3: 回归验证**

```bash
./builddir/sdcshield -e openblas_dgemm -t 3000 -n 1    # pass
./builddir/sdcshield -e zstd19 -t 3000 -n 1             # pass
```

- [x] **Step 4: Commit + push**

```bash
git add tests/cpu/openblas_gemm/lu.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openblas_lu — LAPACK dgesv pivoting/branch-permutation SDC stress"
git push
```

---

### Task 10: `openssl_sm3sm4` — 国密摘要 + 分组密码往返

**Files:**
- Modify: `framework/sandstone_ssl.h`（`SANDSTONE_SSL_EVP_FUNCTIONS` 宏，`F(EVP_sha1) \` 行前加 6 行 `F(EVP_sm4_cbc)` 等——宏区按字母序，sm4 系列插在 `F(EVP_seed_ofb)` 与 `F(EVP_sha1)` 之间）
- Create: `tests/cpu/openssl/openssl_sm3sm4.cpp`
- Modify: `tests/cpu/meson.build`（openssl_sha3 挂线行后再加本文件，Task 1 同位置）

**Interfaces:**
- Consumes: `s_EVP_sm3`（shim 已声明，:1008 实测）、`s_EVP_sm4_cbc`（**本任务新增 shim 声明**；libcrypto.a 符号 nm 实测存在）、`s_EVP_CIPHER_CTX_new`/`s_EVP_EncryptInit_ex`/`s_EVP_EncryptUpdate`/`s_EVP_EncryptFinal_ex`/`s_EVP_DecryptInit_ex`/`s_EVP_DecryptUpdate`/`s_EVP_DecryptFinal_ex`（ipsec 测试先例全部在用）、`s_EVP_MD_CTX_new`/`s_EVP_DigestInit_ex`/`s_EVP_DigestUpdate`/`s_EVP_DigestFinal_ex`。
- Produces: 测试 ID `openssl_sm3sm4`。

**依据**: 国产 ARM 芯片检测工具配国密算法是自然覆盖；sm3 是与 SHA-2/3 不同的整数混合结构，sm4 是与 AES 不同的 S 盒/轮函数结构——两个新整数通路相位。OpenSSL 3.5 的 sm3/sm4 有专门优化实现。

- [x] **Step 1: shim 加 sm4 声明**

`sandstone_ssl.h` `SANDSTONE_SSL_EVP_FUNCTIONS` 宏内、`F(EVP_seed_ofb)` 行后加：

```c
    F(EVP_sm4_cbc)                               \
    F(EVP_sm4_cfb128)                            \
    F(EVP_sm4_ctr)                               \
    F(EVP_sm4_ecb)                               \
    F(EVP_sm4_ofb)                               \
```

（静态链接路径下 `DECLARE_FUNCTIONS` 是 `static constexpr auto s_##Fn = Fn;`，零运行时解析成本；sm3 已在宏里，不动。）

- [x] **Step 2: 写测试文件**

照 ipsec 的 EVP 加解密模式（ipsec_aes128_cbc_hmac_sha1_sse.cpp:49-58 的 HMAC 模式 + aes_encrypt/aes_decrypt 对）：

```cpp
#define SM_DATA_SIZE  (1024u)
#define SM3_DIGEST_SIZE (32)
#define SM4_KEY_SIZE  (16)
#define SM4_IV_SIZE   (16)
#define SM4_BLOCK     (16)

struct sm_test_data {
    uint8_t sm4_key[SM4_KEY_SIZE], sm4_iv[SM4_IV_SIZE];
    uint8_t plaintext[SM_DATA_SIZE];
    uint8_t golden_ciphertext[SM_DATA_SIZE];   /* SM4-CBC 无 padding（数据 1024 = 64 块） */
    uint8_t golden_sm3[SM3_DIGEST_SIZE];       /* 对 plaintext 的 SM3 摘要 */
};
```

init：`memset_random` 填 key/iv/plaintext；golden：`s_EVP_EncryptInit_ex(ctx, s_EVP_sm4_cbc(), NULL, key, iv)` + `set_padding(0)`（1024 是 16 整倍，无填充 → 密文定长 1024）+ EncryptUpdate/Final；sm3 摘要走 `s_EVP_MD_CTX_new`/`s_EVP_DigestInit_ex(ctx, s_EVP_sm3(), NULL)`/Update/Final。
run（TEST_LOOP(test, 256)）：memcpy plaintext→thread_local buf；重算密文 `memcmp_or_fail(..., "sm4-cbc ciphertext mismatch")`；**解密往返** `s_EVP_DecryptInit_ex(s_EVP_sm4_cbc(), key, iv)` 解出后 `memcmp_or_fail(dec, plaintext, 1024, "sm4-cbc roundtrip mismatch")`；重算 sm3 `memcmp_or_fail(..., "sm3 digest mismatch")`。skip 桩照 openssl_sha.cpp 模式（检查 `s_EVP_sm3 && s_EVP_sm4_cbc` 非 NULL，NULL 走 dlopen 路径的版本探测——动态链老 OpenSSL 无 sm4 时干净 skip，TestResourceIssue）。**注意**：解密输出缓冲尺寸断言（`outlen != SM_DATA_SIZE` → report_fail_msg，防填充意外）。

- [x] **Step 3: meson 挂线 + 构建 + 功能验证**

```bash
ninja -C builddir
./builddir/sdcshield --list-tests | grep sm3sm4      # openssl_sm3sm4
./builddir/sdcshield -e openssl_sm3sm4 -t 5000 -n 1  # pass
```

- [x] **Step 4: 回归验证（shim 改动的爆炸半径）**

```bash
./builddir/sdcshield -e openssl_sha -t 3000 -n 1       # pass
./builddir/sdcshield -e openssl_sha3 -t 3000 -n 1      # pass（若 Task 1 已入）
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse -t 3000 -n 1   # pass（ipsec 套件代表）
./builddir/sdcshield -e zstd19 -t 3000 -n 1            # pass
```

- [x] **Step 5: Commit + push**

```bash
git add framework/sandstone_ssl.h tests/cpu/openssl/openssl_sm3sm4.cpp tests/cpu/meson.build
git commit -m "tests/arm64: add openssl_sm3sm4 — Chinese-national digest + block cipher SDC stress (shim: EVP_sm4_*)"
git push
```

---

### Task 11: 统一战役脚本 + 文档全量同步

**Files:**
- Create: `scripts/run/run_sdc_spectrum.sh`
- Modify: `README.md`（§推荐战役模式 :207-223 重写 + §测试用例与检测能力表 :227-260 行更新 + :47 计数更新）
- Modify: `docs/writing_tests.md`（若含库测试清单则同步；先 grep 确认再改）
- Modify: 本计划文件（勾选全部 checkbox）

**Interfaces:**
- Consumes: Task 1-10 产出的全部 knob 与测试 ID（`openssl_sha3`、`sleef_neon.nelems`、`openblas_*.transab/beta_permille/mdim`、`openblas_cgemm`、`pocketfft_fft.n`、`isal_igzip.level`、`openblas_lu.n`、`openssl_sm3sm4`）。
- Produces: `scripts/run/run_sdc_spectrum.sh`（§7.4 战役协议的可执行化）。

- [x] **Step 1: 写战役脚本**

照 scripts/run/ 现有 bash 风格（配置参数区 + mkdir 日志目录 + 阶段循环 + tee 日志）。三阶段结构（§7.4 两阶段协议 + README 模式 A/B/C 合并升级）：

```bash
#!/bin/bash
# SDC 全谱战役：阶段1 谱系广域扫（多样性=检出率）→ 阶段2 固定 seed 深驻留
# 依据 docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md §7.4；日志按阶段/档位分文件。
SDC="./builddir/sdcshield"
LOG_DIR="./sdc_spectrum_$(date +%Y%m%d_%H%M%S)"
SWEEP_TIME="${SWEEP_TIME:-15m}"     # 阶段1每档时长（可 env 覆盖）
DWELL_TIME="${DWELL_TIME:-2h}"      # 阶段2驻留时长

mkdir -p "$LOG_DIR"

# 阶段 1a：GEMM 尺寸谱（cache 域逐层）
for m in 64 256 512 1024; do
  $SDC -O openblas_dgemm.mdim=$m -O openblas_sgemm.mdim=$m \
       -O openblas_zgemm.mdim=$m -O openblas_cgemm.mdim=$m \
       -e openblas_dgemm,openblas_sgemm,openblas_zgemm,openblas_cgemm \
       -t "$SWEEP_TIME" -o "$LOG_DIR/gemm_m${m}.yaml"
done

# 阶段 1b：GEMM 形态谱（转置×β 相位）
for tb in 0 1 2 3; do
  $SDC -O openblas_dgemm.transab=$tb -O openblas_dgemm.beta_permille=500 \
       -e openblas_dgemm -t "$SWEEP_TIME" -o "$LOG_DIR/gemm_tb${tb}.yaml"
done

# 阶段 1c：SLEEF 足迹谱
for e in 1024 16384 262144; do
  $SDC -O sleef_neon.nelems=$e -e sleef_neon -t "$SWEEP_TIME" -o "$LOG_DIR/sleef_e${e}.yaml"
done

# 阶段 1d：FFT 因子谱（pow2 / Bluestein 质数 / 混合 radix）
for n in 4096 4099 6144 10000; do
  $SDC -O pocketfft_fft.n=$n -e pocketfft_fft -t "$SWEEP_TIME" -o "$LOG_DIR/fft_n${n}.yaml"
done

# 阶段 1e：压缩 level 谱
for L in 0 1 2 3; do
  $SDC -O isal_igzip.level=$L -e isal_igzip -t "$SWEEP_TIME" -o "$LOG_DIR/igzip_L${L}.yaml"
done

# 阶段 1f：加密/哈希全家族 + LU + 混合多样性轮
$SDC -e openssl_sha,openssl_sha3,openssl_sm3sm4,isal_crc32_gzip -t "$SWEEP_TIME" -o "$LOG_DIR/crypto.yaml"
$SDC -e openblas_lu -t "$SWEEP_TIME" -o "$LOG_DIR/lu.yaml"
$SDC -e openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip,openssl_sha3,zstd19 \
     -t "$SWEEP_TIME" -o "$LOG_DIR/mixed.yaml"

# 阶段 2：全绿档固定 seed 驻留（--max-test-loop-count=0 关 fracturing，CORE179 单模式持续暴露）
$SDC --max-test-loop-count=0 -e openblas_dgemm,sleef_neon,pocketfft_fft,isal_igzip \
     -t "$DWELL_TIME" -o "$LOG_DIR/dwell.yaml"
```

- [x] **Step 2: README 三处同步**

1. §推荐战役模式（:207-223）：加"模式 D：全谱战役脚本 `scripts/run/run_sdc_spectrum.sh`"段，说明三阶段结构与 env 旋钮（SWEEP_TIME/DWELL_TIME）；
2. §测试用例与检测能力表（:227-260）：`openssl_sha` 行改含 `openssl_sha3`/`openssl_sm3sm4`（哈希/加密族）；OpenBLAS 行加 `openblas_cgemm`/`openblas_lu` 与 transab/beta 旋钮说明；SLEEF 行加 u35/新族/nelems 旋钮；FFT 行加 n 旋钮与 Bluestein 谱系；压缩行加 isal level 旋钮；
3. :47 与 :227 计数：`--list-tests | wc -l` 实测后更新（新增 4 个 PROD 测试：openssl_sha3、openblas_cgemm、openblas_lu、openssl_sm3sm4 → 预期 282，**以实测为准**）；内存预算表（:186-199）补 cgemm 行（float 元素 = mdim²×4B×(3 或 4) 矩阵）。

- [x] **Step 3: 实跑冒烟验证（每阶段至少一档真实执行）**

```bash
chmod +x scripts/run/run_sdc_spectrum.sh
SWEEP_TIME=30s DWELL_TIME=1m bash scripts/run/run_sdc_spectrum.sh
ls sdc_spectrum_*/ | head; grep -l "result: pass" sdc_spectrum_*/*.yaml | wc -l   # 全部档位 pass
./builddir/sdcshield --list-tests | wc -l    # 更新 README 计数的实测依据
```

- [x] **Step 4: docs/writing_tests.md 检查**

```bash
grep -n "openblas\|sleef\|pocketfft\|isal\|openssl" docs/writing_tests.md
# 有库测试清单/计数则同步；纯框架指南无清单则不动（以 grep 结果为准，不臆改）
```

- [x] **Step 5: Commit + push + 计划归档**

```bash
git add scripts/run/run_sdc_spectrum.sh README.md docs/writing_tests.md docs/superpowers/plans/2026-09-16-sdc-coverage-max.md
git commit -m "campaign: run_sdc_spectrum.sh unified spectrum campaign + docs sync for coverage-max suite"
git push
```

（若 writing_tests.md 无需改动则从 add 中去掉；计划文件 checkbox 全勾后一并提交。）

---

## Self-Review 记录

1. **Spec 覆盖**：依据表 A→Task1、B→Task2/3/4、C→Task5/6、D→Task7、E→Task8、F→Task9、G→Task10、H→Task11，无遗漏；"明确不做"清单划出 YAGNI 边界。
2. **占位符扫描**：无 TBD/TODO；每个代码步骤给出确切函数名/常量/行号依据；域映射给出边界分析要求（实现时按 sleef_neon 现有注释标准写全）。
3. **类型一致性**：`sleef_test_data` 字段名 Task 2 定义、Task 3 沿用同名扩展模式；knob 键名 `nelems`/`transab`/`beta_permille`/`n`/`level`/`mdim` 在 Task 11 脚本中逐一对应；`EVP_sm4_*` 宏名与 libcrypto.a nm 实测符号一致；`LAPACK_ROW_MAJOR=101` 与 lapacke.h 一致；isal 常量名与 /usr/include/isa-l/igzip_lib.h:275-283 一致。
4. **风险点**：(a) SLEEF exp u35 不存在——计划已显式改用 exp2 并加粗警示；(b) β≠0 的 C 预填模式——Task 5 已写明确定性预填方案；(c) shim 改动爆炸半径——Task 10 Step 4 用 4 个邻接测试回归覆盖；(d) sm4 动态链接老 OpenSSL——skip 桩已设计；(e) `isal_igzip.level=0`（静态 Huffman）压缩率低——OUT_SIZE=IN×1.5 余量足够（实测 golden 模式下 level0 输出 ≤ IN+头部）。

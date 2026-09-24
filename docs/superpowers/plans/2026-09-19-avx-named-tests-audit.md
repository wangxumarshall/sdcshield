# 53 个 avx 命名测试源码审计：全部无 SVE（2026-09-19）

> 研究者：xu + Claude（plan 模式审计）。三层证据全部实测，无臆测。
> 背景：用户判断"这些测试是在没有 SVE 指令的机器上写的"——审计证实。

## 结论

**53 个名字含 avx 的测试全部不含任何 SVE 代码。** 它们是旧机器（无 SVE 的鲲鹏 920 时代）的产物：名字沿用 x86 参考树的 AVX/AVX2/AVX512 命名，实现全部落在 NEON 128-bit 或纯 C 库调用。

## 三层证据

### 证据 1：源码扫描（53 个文件全查）

```
grep 命中 #include <arm_sve.h> / __ARM_FEATURE_SVE / svptrue / svld1_ / svcntb：
  0 / 53 个文件
与"真 SVE 用户"（10 个文件）的交集：空集
  真 SVE 用户 = tests/cpu/arm64/sve512_*_arm.cpp (9 个) + tests/cpu/sleef/sleef_sve.cpp
```
（`svd_cdouble_noavx512.cpp` 初扫 6 个"SVE痕迹"是误报：匹配到 `svd_` 子串，实际是 Eigen 模板代码。）

### 证据 2：编译产物反汇编（最硬证据，53 个 .o 全查）

```bash
# SVE 操作数模式：z 寄存器 (zN.[bhsd]) 或谓词 (pN/m, pN/z)
对象文件总数 = 53, 含 SVE 指令的 = 0
```
抽样明细：
| 对象 | SVE 操作数 | NEON 操作数 |
|---|---|---|
| `fma_fma_tail_avx2.cpp.o` | 0 | 20 |
| `fma_fmatail_avx512.cpp.o` | 0 | 22 |
| `fma_fma_patterns_avx512_pd.cpp.o` | 0 | 8 |
| `mesh_mesh_upi_avx_sym.cpp.o` | 0 | 26 |
| `mesh_mesh_upi_avx512_symm_int.cpp.o` | 0 | 18 |
| `ipsec_*_avx*.cpp.o`（23 个） | 0 | 0（纯函数调用） |
| **对照组** `sve512_f64_chain_arm.cpp.o` | **5**（`fmla z0.d, p0/m...`） | 1 |

对照组命中证明扫描方法有效。

### 证据 3：真实实现三分类（抽查源码）

| 域 | 文件数 | 真实实现 | 抽查实例 |
|---|---|---|---|
| **FMA** | 11 | NEON 128-bit（`float64x2_t` + `vld1q_f64`/`vfmaq`/`vst1q_f64`） | `fma/fma_tail_avx2.cpp:43-56` |
| **Mesh/UPI** | 18 | NEON 128-bit（`float32x4_t` + `vld1q_f32`/`vst1q_f32`，`#ifdef __aarch64__` 包裹） | `mesh/mesh_upi_avx_sym.cpp:122-168` |
| **IPSec** | 23 | **零 intrinsic**——纯 C 调 OpenSSL EVP（`s_EVP_EncryptInit_ex` 等），向量计算在 OpenSSL 库内部；`_avx/_avx512` 后缀只是沿用 x86 参考树命名 | `ipsec/aes192_gcm/ipsec_aes192_gcm_avx.cpp:26-44` |
| **Eigen SVD** | 1 | Eigen 模板（NEON 后端） | `eigen_svd/svd_cdouble_noavx512.cpp` |

## 53 个测试 ID 清单（按域）

**FMA（11）**：`fma_tail_avx2` `fma_tail_avx512` `fmatail_avx2` `fmatail_avx512` `fmatail_nested_avx2` `fmatail_nested_avx512` `fmatail_double_nested_avx2` `fmatail_double_nested_avx512` `fma_patterns_avx512_ps` `fma_patterns_avx512_pd` + `eigen_svd_cdouble_noavx512`（Eigen 域）

**Mesh/UPI（18）**：`mesh_upi_avx_sym`；avx2 8 个：`mesh_upi_avx2_{symm,asymm,asymm_write,asymm_distrib,asymm_distrib_write,read_only,write_only,read_L3}_int`；avx512 9 个：`mesh_upi_avx512_{sym,symm_int,asymm_int,asymm_write_int,asymm_distrib_int,asymm_distrib_write_int,asymm_read_only_int,asymm_write_only_int,asymm_read_L3_int}`

**IPSec（23）**：
- `ipsec_3des_docsis_{aes_cmac,aes_xcbc,hmac_sha1,hmac_sha224,hmac_sha256,hmac_sha384,hmac_sha512}_avx512`（7）
- `ipsec_aes128_cbc_{aes_cmac,hmac_sha1,sha2_224}_avx`（3）
- `ipsec_aes192_cbc_{sha2_384,sha2_512,xcbc_96}_avx`（3）
- `ipsec_aes192_ctr_{hmac_sha1,hmac_sha224,hmac_sha256,hmac_sha384,hmac_sha512,xcbc}_avx`（6）
- `ipsec_aes192_gcm_avx` `ipsec_aes192_gcm_avx512`（2）
- `ipsec_aes256_cbc_{hmac_sha1,sha2_224,sha2_256}_avx`（3）

本机可运行性抽验（2026-09-19 实测）：`fma_tail_avx2`、`ipsec_aes192_gcm_avx512`、`mesh_upi_avx_sym`（需 ≥2 线程）均 `result: pass`。

## 覆盖缺口与 `tests/cpu/sve/` 的机会

叫 `avx512` 的测试实际只压 **128-bit NEON** 数据通路——x86 参考版的 512-bit 向量宽度从未在 ARM 侧真正复现。本机实测有 **256-bit SVE**（VL=32 字节）+ `f32mm/f64mm/bf16/i8mm` 矩阵指令（见 [2026-09-18-sve-capability-research.md](2026-09-18-sve-capability-research.md)，全部 golden 验证可用）。

`tests/cpu/sve/`（2026-09-19 已建空目录）的两个方向：
1. **移植**：把 FMA/Mesh 域负载移植成真 SVE 版（对齐 `sve512_*` 系列思路，向量宽度真正达到/超过 x86 512-bit）
2. **新写**：FMMLA f32/f64 链、BFDOT bf16、i8mm DOT、FCMLA 复数蝶形（本机独有、现有测试零覆盖）

## 审计命令存档

```bash
# 源码层
grep -rln "arm_sve.h\|__ARM_FEATURE_SVE\|svptrue\|svld1_" tests/ --include="*.cpp" --include="*.c"
comm -12 <(真SVE用户) <(53个avx文件)   # 交集 = 空
# 产物层
for f in $(find builddir/tests -name "*avx*.cpp.o"); do
  objdump -d "$f" | grep -cE "(z[0-9]+\.[bhsd]|p[0-9]+/[zm])"   # 全部 = 0
done
# 测试 ID 层
./builddir/sdcshield --quality=-1 --list-test-ids | grep -i avx   # 53 个
```

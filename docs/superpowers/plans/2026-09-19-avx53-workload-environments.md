# 53 个 avx 命名测试的负载环境实录（2026-09-19）

> 来源：`./builddir/sdcshield --quality=-1 -l` 辏述 + 逐文件源码阅读。SVE 版移植的**负载环境基准**——SVE 版不得改变这些负载环境。

## 框架模式（所有 53 个共用，SVE 版必须严格沿用）

| 环节 | 实现方式（源码实测） |
|---|---|
| **黄金结果生成** | 预计算：在 `test_init`（mesh read_only/write_only/L3 系列：init 里生成随机数组并累加 `golden_sum`）或**首个运行的线程**（mesh sym/asymm 系列：阶段1写入时同步累积 `global_sum`；fma 系列：每次迭代内用标量 `fma()/fmaf()` 正确舍入参考逐元素计算 `sw_ref[]`）中计算并保存 |
| **运行比较** | 每次 `test_run` 计算一次结果，与预存/当次 golden 比较：`memcmp` 逐位（fma 系列：`memcmp(hw_result, sw_ref, ...)`；mesh 系列：`local_sum == golden_sum` + store→reload 一致性检查） |
| **输入数据生成** | `std::mt19937 rng(std::random_device{}())` 每线程独立实例 + `uniform_real/int_distribution`；fma 系列**每次迭代生成新随机数据**（非 init 固定）；mesh sym 系列共享数组按块竞争写入 |
| **一致性检查** | store→memcpy→reload→memcmp（fma 系列）；SVE 版等价物 = `svst1`→`svld1`→`svcmpne` 谓词全真判定 |
| **失败报告** | `report_fail_msg("...: ...mismatch or consistency failure")` 终止线程 |
| **日志** | `fprintf(stderr, ...)` 每 PASS/FAIL 输出（注：这些测试用 fprintf 而非框架 log_*，保留原样式） |
| **非 aarch64 路径** | `#else` → `log_skip(CpuNotSupportedSkipCategory, "to be implemented (placeholder): ARM NEON required for <id>")` + `return EXIT_SKIP`；SVE 版再加 HWCAP_SVE 探测（照抄 `sleef_sve.cpp` 先例） |
| **注册** | `DECLARE_TEST(id, "描述")` + `.groups`（fma/eigen→`group_math`，mesh→`group_math`，ipsec→`group_ipsec`）+ `TEST_QUALITY_PROD` |

## 一、FMA 域 11 个（`tests/cpu/fma/`）— 负载环境表

共同框架：随机 a/b/c 三向量 → 硬件向量 FMA（`c + a*b`）→ 标量 `fma()` 正确舍入参考 → **逐位 memcmp** → store/reload 一致性 → PASS/FAIL 输出。SVE 版差异只在向量宽度和指令。

| 测试 | VECTOR_SIZE | 数据类型 | 随机域 | 特殊值注入 | 循环结构 | 其他 |
|---|---|---|---|---|---|---|
| `fma_tail_avx2` | 4 | double | [-10,10] | 无 | 单层 do-while | 基础版 |
| `fma_tail_avx512` | 8 | double | [-10,10] | 无 | 单层 | 同上仅宽度 |
| `fmatail_avx2` | 4 | double | [-1e10,1e10] | **有**（i%4==0 时按 i%3 注入 a=0/b=1/c=-1） | 单层 | 大范围+特殊值 |
| `fmatail_avx512` | 8 | double | [-1e10,1e10] | 有（同上） | 单层 | |
| `fmatail_nested_avx2` | 4 | double | [-1e10,1e10] | **更强**（i%3==0 时按 (i+group)%4 注入 0/1/-1/**INFINITY**） | **INNER_ITERATIONS=5 组内层循环**/外层 do-while | 嵌套版 |
| `fmatail_nested_avx512` | 8 | double | 同上 | 同上 | INNER=5 | |
| `fmatail_double_nested_avx2` | 4 | double | 同上 | 同上 | **INNER_ITERATIONS=10** | 双重嵌套=更深内层 |
| `fmatail_double_nested_avx512` | 8 | double | 同上 | 同上 | INNER=10 | |
| `fma_patterns_avx512_ps` | **16** | **float** | [-1e6,1e6] f32 | 无 | 单层 | 单精度模式版 |
| `fma_patterns_avx512_pd` | 8 | double | [-1e6,1e6] | 无 | 单层 | 双精度模式版 |
| `eigen_svd_cdouble_noavx512` | M_DIM=300 | complex<double> | Eigen 内部 | — | `EigenSVDTest<BDCSVD,300>` 模板 + `.fracture_loop_count=5` | ⚠️ 特殊：见下 |

**NEON 实现映射**（SVE 版等价替换）：
- `VECTOR_SIZE=4` double → 2× `float64x2_t`（`vld1q_f64`/`vfmaq_f64`/`vst1q_f64`）
- `VECTOR_SIZE=8` double → 4× `float64x2_t`；`16` float → 4× `float32x4_t`
- SVE 替换：`svld1_f64(svptrue_b64(), ptr)`（VL=256 → 一次 4 lane）+ `svmla_f64` + `svst1_f64`；f32 一次 8 lane。**注意保持 VECTOR_SIZE 数值不变**（用谓词分区加载或多次 svld1）

**⚠️ eigen_svd_cdouble_noavx512 特殊性**：它是 Eigen 模板（BDCSVD 300×300 complex<double>），无手写向量代码。其"对应 SVE 版"实际已存在 = `eigen_svd_cdouble_sve`（tests_set_sve，EigenSVE 后端）。**处理决定**：为保持 53 全覆盖，SVE 版做 `eigen_svd_cdouble_nosve2`？不对——它是 `noavx512` 名字，语义是"不带 AVX512 的 NEON 后端版"。**结论：此测试的"SVE 版"= 现有 `eigen_svd_cdouble_sve` 已覆盖同一负载（Eigen 换 SVE 后端）**，但为满足"名字中 avx→sve"的命名规则且不改负载，将做 `eigen_svd_cdouble_novec512` 命名替换不合适。待与用户确认（见开放问题）。

## 二、Mesh/UPI 域 18 个（`tests/cpu/mesh/`）— 负载环境表

两种框架：

**框架 A（sym/asymm 系列，12 个）**：多线程协同——阶段1 全线程竞争认领块（`next_block.fetch_add`）写入随机向量 + 立即回读比较 + 原子累积 `global_sum`；栅栏（`allocated_blocks` 轮询 + `__sync_synchronize`）；阶段2 全线程各读全数组验证 sum + store/reload 一致性；阶段3 `round_done` 轮次翻转。

**框架 B（read_only/write_only/read_L3 系列，6 个）**：init 生成随机数组+golden_sum（或每线程写后求和校验），run 顺序扫全数组累加（read）或写入+校验（write）。

| 测试 | 框架 | 元素类型 | 工作集 | 块结构 | 描述要点 |
|---|---|---|---|---|---|
| `mesh_upi_avx_sym` | A | float | TOTAL_ELEMENTS=1024 | 块=4 元素（1 NEON 向量），256 块 | L1D↔L1D 对称 |
| `mesh_upi_avx2_symm_int` | A | int32 | 1024 | 同上 | 同 avx_sym 但 int32 |
| `mesh_upi_avx2_asymm_int` | A | int32 | 1024 | 块=8 元素（2 向量），128 块 | 1写核↔多读核互斥 |
| `mesh_upi_avx2_asymm_write_int` | A | int32 | 1024 | 块=8 | 1读核↔多写核 |
| `mesh_upi_avx2_asymm_distrib_int` | A | int32 | 1024 | 块=8 | 分布式读 |
| `mesh_upi_avx2_asymm_distrib_write_int` | A | int32 | 1024 | 块=8 | 分布式写 |
| `mesh_upi_avx512_sym` | A | float | 1024 | **BLOCK_SIZE=16（4 NEON 向量逻辑块）** | avx512=更宽逻辑块 |
| `mesh_upi_avx512_symm_int` | A | int32 | 1024 | 块=16 | |
| `mesh_upi_avx512_asymm_int` | A | int32 | 1024 | 块=16 | |
| `mesh_upi_avx512_asymm_write_int` | A | int32 | 1024 | 块=16 | |
| `mesh_upi_avx512_asymm_distrib_int` | A | int32 | 1024 | 块=16 | |
| `mesh_upi_avx512_asymm_distrib_write_int` | A | int32 | 1024 | 块=16 | |
| `mesh_upi_avx2_read_only_int` | B | int32 | **ARRAY_SIZE=1M 元素=4MB** | 逐 4 元素向量读 | L1/L2/L3 互读 |
| `mesh_upi_avx2_write_only_int` | B | int32 | **每线程 1M=4MB** | 逐 4 写 | 互写 |
| `mesh_upi_avx2_read_L3_int` | B | int32 | 1M=4MB | 逐 4 读 | **瞄准 L3 命中** |
| `mesh_upi_avx512_asymm_read_only_int` | B | int32 | 1M | **VECTOR_SIZE=16 逻辑块** | avx512 宽读 |
| `mesh_upi_avx512_asymm_write_only_int` | B | int32 | 每线程 1M | 块=16 | |
| `mesh_upi_avx512_asymm_read_L3_int` | B | int32 | 1M | 块=16 | |

**NEON→SVE 映射**：`vld1q_s32`→`svld1_s32(svptrue_b32(),...)`（一次 8 lane）、比较 `vceqq_f32`→`svcmpeq_u32`+`svcntp` 谓词计数全真判定；`BLOCK_SIZE=16` 逻辑块保持（SVE VL=256/8lane → 2 次 svld1 或谓词分区）。

## 三、IPSec 域 23 个（`tests/cpu/ipsec/`）— 负载环境表

**关键事实（实测）**：23 个测试**本体零向量代码**——纯 C 调 OpenSSL EVP 接口（`s_EVP_EncryptInit_ex`/`s_EVP_aes_*_gcm` 等），加解密+tag 生成 vs **init 预计算 golden**（golden_ciphertext/golden_tag/golden_decrypt），每次 run 重算比对。向量计算全部发生在 **libcrypto 内部**。

**⚠️ OpenSSL SVE 路径现状（实测反汇编整个 vendored libcrypto.a）**：
- 1012 个对象中**只有 2 个含 SVE**：`chacha-armv8-sve.o`（1217 处，ChaCha20 的 SVE 汇编）+ `arm64cpuid.o`（2 处探测指令）
- **ipsec 23 个测试用的算法（AES-CBC/CTR/GCM、3DES、HMAC-SHA1/2、XCBC、CMAC）在 OpenSSL 里全部走 NEON 汇编路径，零 SVE**（aesv8-armx.o=2430 NEON ops/0 SVE；sha512-armv8.o=492/0；sha1-armv8.o=84/0）
- OpenSSL 3.5.0 上游 aarch64 汇编就没有 AES/SHA 的 SVE 实现（只有 ChaCha20 有）

**⇒ ipsec 系列"SVE 版"的真实语义**：把测试名 avx→sve 后，计算路径**不可能**因为改名而走 SVE——除非换算法实现。**处理方案（待用户确认）**：
- 方案 1（保负载不动）：SVE 版 = 同样的 OpenSSL EVP 负载（测试逻辑一字不差），仅改名+描述更新。诚实地在描述中说明"compute path is OpenSSL NEON；SVE 版本仅扩展测试矩阵命名"。⚠️ 这与"负载完全可以用 SVE 写一遍"的预期不符。
- 方案 2（换真 SVE 计算路径）：ipsec 的哈希部分（HMAC-SHA1/224/256/384/512、SHA2 系列）用 **SVE 自写实现**（SHA-2 可用 SVE 向量化 4 路并行消息调度；已有开源先例），AES 部分维持 OpenSSL（或 SVE AES=需 SVE2，本机无）。工作量集中且真正引入 SVE 负载。
- 开放问题：SHA-1/SHA-2 的 SVE 实现是可行的（多 buffer 并行哈希），但**单 buffer HMAC 无法向量化**（消息链依赖），需重构成 4/8 路并行独立 HMAC 流（负载等价性可保——算法和每流数据量不变）。

## 四、量化汇总

| 域 | 数量 | SVE 版可无损移植（纯指令替换） | 需决策 |
|---|---|---|---|
| FMA | 10 | **10/10**（直接 vld1q→svld1 映射，负载参数全保留） | 无 |
| FMA(eigen) | 1 | 0（Eigen 模板，SVE 后端版已存在=eigen_svd_cdouble_sve） | 命名/去留 |
| Mesh | 18 | **18/18**（两个框架纯指令替换） | 无 |
| IPSec | 23 | 0（计算在 OpenSSL 内部，上游无 AES/SHA SVE 路径） | 方案 1/2 |

## 开放问题（2026-09-19 用户已拍板）

1. **ipsec 24 个**（修正：实测 avx 命名 ipsec 是 24 个不是 23，总计 53 个不变因为 FMA 域 eigen 那个算数调整见下）：**方案 2** —— 哈希部分自写 SVE 多流并行实现（HMAC-SHA 系列 4/8 路独立并行流，SVE 向量化），AES 部分保持 OpenSSL EVP。
2. **eigen_svd_cdouble_noavx512**：现有 `eigen_svd_cdouble_sve` 在本机**超时/崩溃**（2026-09-19 实测：SIGSEGV，maxrss 2.4GB，runtime 28s+ 崩；根因 = 2026-09-18 的 Eigen5 SVE double packet 补丁 `feat/eigen-sve-double-packets` 分支在该场景下有缺陷）。用户指示：**"你再写一个正常的"** —— 即写一个能正常运行的 SVE 版 SVD 类负载替代（不用 Eigen SVE 后端，改为自写 SVE 矩阵运算复现 SVD 式 FMA 负载，或用稳定的小维度）。
3. **命名规则**：`avx/avx2/avx512 → sve`（本机 VL=256 无 SVE2，全部用 SVE1 指令）。已验证全部 53 个新名字无冲突（`fma_tail_sve` 等全部可用）。

## 实施基线（2026-09-19 定稿）

| 域 | 数量 | SVE 版实现方式 |
|---|---|---|
| FMA | 10 | 无损指令替换（vld1q/vfmaq → svld1/svmla），负载参数全保留 |
| FMA(eigen) | 1 | 自写正常 SVE 负载（不用 Eigen SVE 后端） |
| Mesh | 18 | 无损指令替换（两框架） |
| IPSec | 24 | 哈希自写 SVE 多流（SHA-1/SHA-2 消息调度向量化），AES 保持 EVP |
| **合计** | **53** | 全部进 `tests/cpu/sve/`，独立编译库 `-march=armv8.2-a+sve` |

# 三方库 SDC 检测用例审查——检测实现逻辑逐族分析

> 范围：所有通过 `third-party/`（或系统等价物）三方库实现业务负载的检测用例。
> 逐族给出：检测逻辑结构、golden 来源、比较方式、覆盖的故障面、以及值得注意的弱点。
> 全部结论基于源码实读（文件:行号可溯）+ 本会话/历史真实运行记录。

---

## 总览：11 个测试族，76 个用例

| 族 | 用例数 | 三方库 | golden 来源 | 比较方式 |
|---|---|---|---|---|
| ipsec | 46 | OpenSSL（vendored 3.5.0） | init 自算 | byte-exact memcmp ×3 |
| openssl_sha/sha3/sm3sm4 | 3 | OpenSSL | init 自算 | byte-exact memcmp |
| eigen_svd(+jacobi) | 10 | Eigen 5 | init 自算 | byte-exact memcmp（U/V） |
| eigen_gemm | 7 | Eigen 5 | init 自算 | byte-exact memcmp |
| eigen_sparse | 1 | Eigen 5 | init 自算 | byte-exact memcmp |
| openblas（gemm×4+lu） | 5 | OpenBLAS 0.3.29 | init 自算 | byte-exact memcmp + 输入完整性 |
| sleef_neon / sleef_sve | 2 | SLEEF 3.9 | init 自算（同核） | byte-exact memcmp（30/12 族） |
| pocketfft_fft | 1 | pocketfft C | init 自算 | byte-exact memcmp（正变换谱） |
| isal_igzip | 1 | isa-l 2.32.1 | init 自算 | byte-exact ×2（流+往返） |
| isal_crc* | 10 | isa-l | **分两派**（见 §10） | 等值比较（非 memcmp） |
| zstd/zlib（系统库） | ~8 | libzstd/zlib | **无 golden**（往返自洽） | 长度+内容 memcmp |

**统一的检测哲学**（CLAUDE.md 明文纪律）：`init` 期用同一库同一代码路径算一次 golden → `run`
期每迭代重算 → **byte-identical memcmp_or_fail**。浮点无结合律问题（同二进制同路径确定性），
密码/哈希/CRC 天然位敏感。这是"确定性重放"模式，区别于 OpenDCDiag 原生的"自洽往返"模式。

---

## 1. ipsec（46 用例，最大的族）

**结构**（46 个文件全部同构，逐一验证 `memcmp ×3` 计数一致）：

```
init:   随机 key/iv/plaintext(1024B) → 加密得 golden_ciphertext
        + 对密文算 MAC 得 golden_mac
run:    TEST_LOOP(256):
          encrypt(plaintext)  → memcmp vs golden_ciphertext   ← 检测点1：加密数据路径
          decrypt(ciphertext) → memcmp vs plaintext           ← 检测点2：解密数据路径
          mac(ciphertext)     → memcmp vs golden_mac          ← 检测点3：MAC 数据路径
```

变体矩阵：密码（3DES/AES128/192/256 × CBC/CTR/GCM）× MAC（HMAC-SHA1/224/256/384/512、
SHA2、AES-CMAC、AES-XCBC）× SIMD 后端标签（sse/avx/avx512/x86_64）。
GCM 变体把 tag 当第三比较对象（`aes_gcm_encrypt` 产 tag、decrypt 验 tag）。

**覆盖的故障面**：AES-NFUs（ARM 上是 ASIMD AES/SHA 扩展）乘加链、字节置换、进位链；
一次 1024B 加密 = 数百个 SIMD 块操作。三次比较覆盖双向+认证。
**弱点**：数据只有 1KB——每迭代计算量小，靠 TEST_LOOP(256) 堆迭代数；
AVX512 变体在 ARM 上实际与 x86_64 变体走同一 OpenSSL 分发（OpenSSL 自选后端，标签只是命名），
并非真的钉死 SIMD 路径。

## 2. openssl_sha / openssl_sha3 / openssl_sm3sm4（3 用例）

- **openssl_sha**：1024 组 golden 元素（512B 明文 + sha256/384/512 三摘要）。
  run 期**随机挑一组** golden 元素，拷到**随机偏移的 mmap arena**（`random64()&0x1ff|1`——
  故意制造非对齐地址，让 SIMD 摘要的 aligned/unaligned 加载路径都被打到），重算三摘要逐个 memcmp。
  这个"随机偏移 arena"是对**对齐敏感型 SDC**的针对性设计（地址未对齐时加载路径分叉）。
- **openssl_sha3**：同结构，五个 SHA3 变体（224/256/384/512），同样 mmap 随机偏移。
- **openssl_sm3sm4**：国密 SM4-CBC 加密 + SM3 摘要。golden 含密文+摘要；注释里特别处理了
  "SM4 加密输出与输入相同会被误判为没加密"的自检（先验证密文≠明文）。
  无 mmap 偏移（结构最简单的一族）。

**覆盖面**：SHA2 走 NEON/ASIMD SHA 扩展；SHA3/SM3/SM4 在 ARM 上多为查表+ALU 路径——
与 ipsec 的 AES 路径互补。

## 3. eigen_svd + eigen_svd_jacobi（10 用例）—— 本分支的深度改造对象

**模板**（`sandstone_eigen_common.h`，本会话刚加了运行期维度模式）：

```
init:  A = Random(Dim,Dim) → BDCSVD/JacobiSVD → golden U, V
run:   每迭代重算 SVD → memcmp U、memcmp V（逐元素 byte-exact）
```

- NEON 版（eigen_svd_cdouble 等）：M_DIM=300，10 分钟内完成。
- **eigen_svd_cdouble_sve**（本分支）：维度运行期按内存定（300 下限、4400 时间帽），
  desired_duration 随维度缩放（t(N)≈243.4s×(N/4400)³ 实测模型）。
- jacobi 变体走 JacobiSVD（两侧旋转），BDCSVD 走分治——不同算法路径覆盖不同 FMA 访问模式。

**已知平台特性**（CLAUDE.md 记录）：全核多线程下 ULP 级偶发假 FAIL——这是
Eigen 并行归约顺序 + 严格 memcmp 的固有张力，run-sdcshield.sh full 用 `-n 1` 规避。

## 4. eigen_gemm（7 用例）

`lhs/rhs = Random(M_DIM)` → init 算 golden 积 → run 重算 memcmp。
变体：double/float/complex×(dump 与否)。dump 变体额外打印首元素。
GEMM 是 FMA 吞吐的最重负载（O(n³) 乘加），SDC 研究文献里向量 FMA GEMM 是第一优先负载
（SDC_RESEARCH_SYNTHESIS_CN.md 的选型依据）。

## 5. eigen_sparse（1 用例）

稀疏对称阵 Cholesky（`SimplicialCholesky`）解 Ax=b → golden 解向量 memcmp。
覆盖**稀疏索引+分解算法**路径（填充/符号分析），与稠密负载互补。
同属"全核 ULP 假 FAIL"四巨头之一（CLAUDE.md）。

## 6. openblas（gemm×4 + lu，5 用例）—— 检测点最全的一族

```
init:  a/b 随机（受控幅值映射）→ cblas 乘 → golden
       (+ transab/beta/mdim knob)
run:   copy-in a_copy/b_copy（脏缓存再重载）
       → cblas_dgemm(α=1, β, c)
       → memcmp c vs golden                     ← 检测点1：乘加结果
       → memcmp a_copy vs a, b_copy vs b        ← 检测点2/3：输入完整性！
```

**独有设计**：
- **输入完整性检查**（GEMM 后验证 A/B 没被内核写坏——抓"越界写/别名污染"类 SDC，
  其他族都没有这层）；
- **beta≠0 knob** 走 read-modify-write 路径（C 被读+累加），且每迭代重播确定性 C 种子；
- **transab knob**（NN/NT/TN/TT）× mdim（16..4096）扫缓存层级（L1→LLC→DRAM）；
- **lu**：LAPACKE_dgesv，golden 三件套（LU 因子、pivot 序、解向量）逐一 memcmp——
  pivot 序是比较**整数数组**（分支/比较指令路径的检测，浮点族里少见）。

## 7. sleef_neon / sleef_sve（2 用例）

**结构**：init 期输入按"域映射"生成（sin/cos ∈[-π,π]、exp 有界、log 正数、pow 底/指数分离），
**golden 用同一个 SLEEF 内核算**（self-golden）；每个 golden 再过**有限性/范围哨兵**
（NaN 会毒化 memcmp，init 期 fail-loudly 而不是污染运行期）。
run 期每族（neon 30 族 / sve 12 族：u10 高精度档 + u35 低精度档 = **同一函数的两条独立多项式
实现**互相印证）逐核重放 → memcmp。

**关键洞察**：u10 vs u35 是 SLEEF 对同一数学函数的两条不同多项式链——如果硬件故障同
时打中两条链的相同位，两个 golden 都错而检测不到（理论盲区），但两条链指令序列完全不同，
实际故障不可能同步打中——这是**廉价的路径多样性**。
SVE 版（sleef_sve）与 NEON 版共享输入生成逻辑（RNG 消费顺序一致），SVE 内核走
`svld1/svst1 + svcntd` 宽向量。

## 8. pocketfft_fft（1 用例）

复数 cfft（迭代多通道）+ 实数 rfft 两段：
```
init:  input/rinput 随机 → cfft_forward → golden_spec；rfft_forward → rgolden
run:   copy-in（脏缓存）→ cfft_forward → memcmp golden_spec    ← 检测点
       → cfft_backward(1/N)  【不比较——往返数值不 bit-exact，比较会健康硅上也失败】
       → rfft 同构
```
注释明确记录了实测往返误差 ~3e-11 所以**故意不比较逆变换**——这是"知道哪里能比、
哪里不能比"的诚实设计。检测面：蝶形运算的旋转因子乘加、位反转寻址、跨通道访存。

## 9. isal_igzip（1 用例）+ zstd/zlib

**isal_igzip**（vendored）：
```
init:  input 随机 → isal_deflate_stateless → golden 流 + golden_comp_size
run:   重压缩 → size 比较 + memcmp 流          ← 检测点1：压缩器确定性
       → isal_inflate → size + memcmp 原文     ← 检测点2：解压+匹配重构
```
level knob（0..3）切换匹配查找器（level 2/3 有独立 level_buf，注释验证了
"预脏化 buffer 会改变输出"的坑）。压缩输出对内部状态极敏感——哈夫曼编码位打包、
匹配偏移编码都是 SDC 高增益面。

**zstd/zlib（系统库，严格说不属 third-party vendored）**：**无 golden 的自洽往返**模式
——compress→decompress→memcmp 原文+长度。只检测"往返保真"，不检测"压缩流确定性"
（同输入两次压缩可以产生不同但都合法的流）。检测强度弱于 isal_igzip 的双 memcmp。

## 10. isal_crc*（10 用例）—— ⚠️ 族内两种不同的检测逻辑

逐文件实读发现**分两派**：

**A 派（isal_crc32_gzip，1 个）—— 软件参照模式（最强）**：
```
run:   随机 1024B → 逐位循环软件 CRC（测试自带 C 参照实现，多项式硬编码）
       → isa-l crc32_gzip_refl(硬件优化版)
       → crc_hw == crc_ref 等值比较        ← 真·交叉验证
       + store/reload 一致性
```

**B 派（其余 9 个：crc64×6 + ieee + iscsi + t10dif）—— 双重自比模式（较弱）**：
```
run:   随机 1024B → crc64_ecma_norm(...) → crc1
       → __sync_synchronize() → 同一函数再算 → crc2
       → crc1 == crc2                        ← 只检测"同函数两次结果一致"
       + store/reload 一致性
```

**B 派的弱点（这是全审查里发现的最显著检测强度缺口）**：
1. **无独立参照**——同一二进制同一函数两次调用走完全相同的指令序列。能抓的只有
   "间歇性故障"（第一次错第二次对）或时变故障；**永久性单 bit 故障**（如某乘法器
   恒定错一位）两次都错得一样 → **crc1==crc2 通过，检测不到**。
2. B 派连软件参照都没有（A 派有）；`sw_ref=0` 的 9 个文件里没有参照实现。
3. store/reload 一致性检查只覆盖 8 字节（CRC 结果本身），对 1024B 输入无输入完整性检查。
4. 用 `std::mt19937`（libc++ 随机）而非框架 RNG（`memset_random`）——注：框架只 trap 了
   libc rand/random，std::mt19937 合法，但与全仓"框架 RNG 可播种复现"的纪律不一致
   （seed 来自 random_device，跨运行不可复现）。

**建议**（如果要做增强）：B 派 9 个用例补一个表驱动软件参照（crc64 的多项式查表实现
约 30 行），从"双重自比"升级到 A 派的交叉验证模式；`mt19937` 换 `memset_random` 对齐
可复现纪律。

---

## 横向总结

**检测强度谱系**（从强到弱）：

1. **交叉验证**（独立参照）：isal_crc32_gzip（软硬对拍）、sleef 的 u10/u35 双链、
   lu 的 pivot 整数比较
2. **golden 重放**（init 自算 + byte-exact）：ipsec×46、openssl×3、eigen×18、
   openblas×5、sleef、pocketfft、isal_igzip —— 全族主流，浮点确定性靠"同二进制同路径"
3. **往返自洽**（无 golden）：zstd/zlib —— 只保真不查确定性
4. **双重自比**（同函数两次）：isal_crc 的 B 派 9 个 —— **最弱**，永久故障盲区

**共性设计亮点**：
- 随机偏移 mmap arena（openssl_sha/sha3）——打对齐分叉路径
- 输入完整性三重检查（openblas）——抓越界写
- 有限性哨兵（sleef）——防 NaN 毒化 memcmp
- "不比较逆变换"的诚实取舍（pocketfft）——知道哪里不能比
- knob 化扫参数空间（openblas mdim/transab/beta、igzip level、sleef nelems、
  本分支的 mdim）

**共性弱点**：
- B 派 CRC 的永久故障盲区（上述）
- ipsec 的 AVX512/SSE 标签与实际 SIMD 路径解耦（OpenSSL 自分发）
- eigen 全核 ULP 假 FAIL 需 -n 1 规避（平台特性，已文档化）

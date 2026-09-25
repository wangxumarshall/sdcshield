# D-tests.md — SDCShield 测试全景盘点与 FP/向量激发覆盖矩阵

**Agent D 交付物**(2026-09-16)。盘点对象是测试**设计形态**(用什么指令、什么数据流、什么校验策略),不是跑结果。目标是找出与「最大化激发 ARM64 服务器芯片(cn23154 = 2×304 核 HiSilicon 0xd22 TaiShan,SVE 512-bit VL=64B)浮点向量单元 SDC」之间的覆盖差距。

---

## 1. 测试全景统计

### 1.1 总数

- 源码树 `DECLARE_TEST` 唯一测试名:**304 个**(tests/common + tests/cpu + tests/gpu;grep `DECLARE_TEST(` 去重,含 gpu 2 个、idxd 0 个)。
- 本机 aarch64 构建实际注册:`./builddir/sdcshield --list-tests` = **279 个**(默认 PROD 质量;GPU/idxd 不构建,部分测试因 vendored 库缺失而不编译,eigen_svd_cdouble_sve 在 SVE 构建集中)。
- 质量:PROD 340 处声明、BETA 9 处、SKIP 6 处(`grep quality_level` 计数,一文件可多次声明)。BETA/SKIP 文件:`tests/cpu/eigen_svd_jacobi/*`(4)、`tests/cpu/arm64/{neon_test,sdc_test,crypto_test}.cpp`、`tests/common/{smi_count,mce_check}`、`tests/cpu/{ifs,ist}`、`tests/gpu/*`。
- 每测试一个 fork 子进程;每 CPU 一个 worker 线程(框架自动绑核);fracturing 默认开(同测试多次 fork、不同 RNG 种子)。

### 1.2 按目录分布(唯一测试名,aarch64 视角)

| 目录 | 数量 | 性质 |
|---|---|---|
| tests/cpu/ipsec/ | 46 | OpenSSL EVP 加解密+哈希(库级,ARM64 上全变体可建) |
| tests/cpu/arm64/ | 36 | ARM64 原生 NEON/SVE/inline-asm(含 9 个 sve512、movbe 系列 13、core-179 探针) |
| tests/cpu/mesh/ | 27 | 跨核 NEON int32 流量(symm/asymm/read/write/L3) |
| tests/cpu/memory/ | 21 | memcpy/vmovnt/mmu_stress_arm/memcpy_rewr(MPSC NUMA) |
| tests/cpu/spinlock/ | 19 | std::atomic 自旋锁(可移植) |
| tests/cpu/crc/ | 15 | zlib/isal/ACLE CRC |
| tests/cpu/fma/ | 12 | NEON vfma 家族(见 §2) |
| tests/cpu/misc/ + vector/ | 11+11 | kreg/swizzle/insert_extract、mrn_* 探针 |
| tests/cpu/arithmetic_arm/ | 11 | GMP/ACL/compiler-rt 大整数与 FP 库 |
| tests/cpu/virtualization/ | 9 | ARM64 sysreg/模拟一致性 |
| tests/cpu/lock/ | 9 | std::atomic 锁 |
| tests/cpu/gather/ | 10 | **标量模拟** gather/scatter(非指令级) |
| tests/cpu/jit/ | 7 | 运行时代码生成(move elim/store fwd/x86 概念) |
| tests/cpu/atomic/ | 6 | 128/256/512 位原子(NEON 模拟) |
| eigen_gemm 7 / eigen_svd 7 / svd_jacobi 4 / sparse 1 | 19 | Eigen 库级 GEMM/SVD/sparse |
| openblas_gemm 3 / sleef 2 / pocketfft 1 | 6 | vendored 库级(NEON/SVE kernel) |
| zstd 4 / zlib 5 / openssl 1 / isa-l 1 | 11 | 压缩/哈希 |
| arithmetic 4 / ifs 3 / ist 3 / cache 2 / load_port 1 / arm-0102 1 | 14 | 进位链、IST、cache、AGU |
| tests/common/ | 2 | mce_check(EDAC 真实现)、smi_count(x86-only 占位) |

### 1.3 架构适用性

- x86-64 仍是参考架构;ARM64 全部以 `#elif defined(__aarch64__)` 或 aarch64-only meson 块接入。
- SVE 硬编码档位:`tests_set_sve` 用 `-march=armv8.2-a+sve -msve-vector-bits=128`(tests/cpu/meson.build:883-886);sve512 家族运行时探测 `svcntd()==8`(stencil)或 HWCAP_SVE。
- SME:simd-arm.conf:77-87 已定义 `sme`/`sme_f64f64` 等 HWCAP2 位,**但没有任何测试使用 SME**。

---

## 2. FP/向量激发覆盖矩阵(核心交付)

图例:●=专门激发 ◎=间接覆盖 ○=不覆盖。文件引用均可追溯。

| 测试(组) | 标量FP | NEON | SVE | FMA链 | 特殊值 | 舍入模式 | gather/scatter | load/store带宽 | 谓词 | 寄存器堆压力 | 依赖链深度 | ILP宽度 | 跨核共享 | 校验策略 |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **fma.cpp** (fma) | ○ | ●f32 | ○ | 短(1 op) | ○ | ○ | ○ | ○ | ○ | ○(4reg) | 浅 | 2 | ○ | **1e-6 容差**+fmaf 参考(fma.cpp:21-30) |
| **fma_patterns_{pd,ps}** | ○ | ●f32 | ○ | 4×1 op | ○ | ○ | ○ | ○ | ○ | ○(12reg) | 浅 | 4 | ○ | 1e-6 容差(:73-78) |
| **fmatail_** avx2/avx512/nested/double_nested (7) | ○ | ●f64/f32 | ○ | 2-4 op 尾链 | 部分{0,±1,Inf} | ○ | ○ | ○ | ○ | ○ | 浅中 | 2-4 | ○ | **byte-exact memcmp**(fmatail_avx2.cpp:73) |
| **fpu_special_values** | ◎fmaf参考 | ●f32+f64 | ○ | 1 op | ●12值笛卡尔(qNaN/sNaN/±Inf/±0/denorm/max/min) | ○(默认) | ○ | ○ | ○ | ○ | 浅 | 2-4 | ○ | bit-exact(:151-160) |
| **fsu_byteexact_arm** | ◎fma参考 | ●f32 | ○ | 链+宽窄转换 | ●NaN/Inf 分类检查 | ○ | ○ | ●128B跨线 | ○ | ○ | 中 | 4 | ○ | bit-exact+分类 |
| **sve512_f64_chain_arm** | ◎fma参考 | ○ | ●f64 FMLA 512步串行 | ●512 深 | ○(有限高Hamming) | ○ | ○ | ○ | ○ | ○(2 Zreg) | ●512 | 1 lane/步 | ○ | bit-exact vs 标量 fma(:111,129) |
| **sve512_f32_chain_arm** | ◎fmaf参考 | ○ | ●f32 16 lane | ●512 步 | ○ | ○ | ○ | ○ | ○ | ○ | ● | 16 lane | ○ | bit-exact |
| **sve512_f64_special_arm** | ◎ | ○ | ● | 64 步 | ●IEEE 特殊表按分类 | ○ | ○ | ○ | ○ | ○ | 中 | 8 | ○ | 分类比较 |
| **sve512_gather_scatter_arm** | ◎ | ○ | ● | ●1 FMA/gather | ○ | ○ | ●svld1_gather_u64index + svst1_scatter(:78-82) | ○ | ○ | ○ | 浅 | 8 | ○ | bit-exact vs 标量同索引 |
| **sve512_*_chain/svd 家族 (3)** | ◎ | ○ | ● | ● | ◎ | ○ | ○ | ●~2.7GiB/流 L3/TLB 压力 | ○ | ○ | ● | 8-16 | ○ | bit-exact |
| **sve512_stencil_axis_arm** | ○ | ○ | ● | ●双向量 svmla_x | ○(整数结果) | ○ | ○ | ●ld1rd 系数重装载 | ○ | ○ | 深(全展开) | 16 | ○ | 精确整数期望 1.5·iter·(j+1)(:197-199) |
| **power_virus_dit** | ◎fmaf参考 | ●f32 | ○ | ●2048 burst 交替操作数 | ○ | ○ | ○ | ○ | ○ | ○ | ●2048 | 4 | ○(全核同步靠框架) | bit-exact 4 lane(:173-176) |
| **openblas_{d,s,z}gemm** | ○ | ●NEON kernel | ○ | ●手写 fmla 微核(K=mdim) | ○(有界操作数) | ○ | ○ | ●copy-in/compute/verify | ○ | ●kernel 重用 | ●K 链 | ◎(kernel 内) | ○ | **byte-exact memcmp**(dgemm.cpp:141-147)+ mdim 旋钮 16..4096 |
| **sleef_neon / sleef_sve** | ○ | ●f32/f64 | ●(sve 变体) | ●多项式 FMA 长链 | ○(域内有界) | ○ | ○ | ○ | ●svptrue(满) | ◎ | ●多项式 | 2-16 | ○ | byte-exact(同一 kernel 出 golden) |
| **pocketfft_fft** | ○ | ◎(库内 dispatch) | ○ | ●twiddle-FMA 蝶形 | ○ | ○ | ◎位反转抽取 | ●散布 store→load | ○ | ◎ | ●logN | ◎ | ○ | byte-exact golden 频谱(fft.cpp:20-22) |
| **eigen_gemm_** (7) | ○ | ◎(库内 NEON) | ◎(svd_cdouble_sve) | ● | ○ | ○ | ○ | ○ | ○ | ◎ | ● | ◎ | ○ | byte-exact(gemm_…:62) |
| **eigen_svd_** (7+1sve) | ○ | ◎ | ◎ | ●BDCSVD 底层 GEMM | ○ | ○ | ○ | ○ | ○ | ◎ | ● | ◎ | ○ | **byte-exact memcmp U/V**(eigen_common.h:53-64)— 已知 192 核偶发 ULP 假阳(CLAUDE.md) |
| **eigen_svd_jacobi_** (4) | ○ | ◎ | ○ | ●Jacobi 旋转 | ○ | ○ | ○ | ○ | ○ | ◎ | ● | ◎ | ○ | byte-exact |
| **eigen_sparse** | ○ | ◎ | ○ | ●稀疏求解 | ○ | ○ | ○ | ●稀疏 gather 式访问 | ○ | ○ | 中 | 低 | ○ | byte-exact |
| **acl_gemm** | ○ | ●F32 64×64 | ○ | ● | ○ | ○ | ○ | ○ | ○ | ◎ | ● | ◎ | ○ | **1e-3 容差**(acl_gemm.cpp:118-127)— 弱校验 |
| **fisttp_arm** | ◎ | ●FCVTZS 转换 | ○ | ○(转换) | ○ | ○(截断固有) | ○ | ○ | ○ | ○ | 浅 | 4 | ○ | byte-exact int32 |
| **crt_builtins** | ●软浮点 __addsf3 等 | ○ | ○ | ○ | ○ | ○ | ○ | ○ | ○ | ○ | 浅 | 1 | ○ | byte-exact(:82-92) |
| **gather/gatherscatter_** (10) | ●标量数组 | ○(纯标量模拟) | ○ | ○ | ○ | ○ | ◎软件模拟 | ○ | ○ | ○ | 浅 | 1 | ○ | memcmp + 双载一致性 |
| **mesh_upi_** (27) | ○ | ●int32(vld1q_s32) | ○ | ○ | —(整型) | — | ○ | ●跨核 L3 流量 | ○ | ○ | 浅 | 4 | ●(生产/消费对) | memcmp(vceqq_s32) |
| **neon_test / neon_add** | ○ | ●u32 add | ○ | ○ | — | — | ○ | ○ | ○ | ○ | 浅 | 4 | ○ | memcmp_or_fail |
| **neon_rot_2src / neon_rot_ldr_at_top** | ○ | ●u64x2 ALU 旋转 | ○ | —(整型) | — | — | ○ | ●str q→ldr q 背靠背 | ○ | ○ | 中 | 2 | ○ | memcmp |
| **agu_stress_2src** | ○ | ○(标量 ldr/str asm) | ○ | — | — | — | ○ | ●3 ldr+2 str/元素 | ○ | ○ | 中 | 1 | ○ | memcmp |
| **ooo_dep_chain_arm** | ○ | ○(ALU) | ○ | — | — | — | ○ | ◎指针追逐 | ○ | ○ | ●ROB 填满 | 1 | ○ | 软件校验和 |
| **movbe 系列 (13)** | ○ | ○(bswap 标量) | ○ | — | — | — | ○ | ◎store/reload | ○ | ○ | 浅 | 1 | ○ | memcmp |
| **kreg 系列 + swizzle + insert_extract (11)** | ○ | ◎(5 个真 NEON) | ○ | — | — | — | ○ | ○ | ○(无 NEON 谓词) | ○ | 浅 | 低 | ○ | memcmp |
| **crypto_test / openssl_sha / ipsec (47)** | ○ | ◎(库内 AES/SHA) | ○ | — | — | — | ○ | ○ | ○ | ○ | 中 | ◎ | ○ | byte/hash |
| **memcpy/vmovnt (21)** | ○ | ◎(7 个 NEON) | ○ | — | — | — | ○ | ●L1/L2/L3 尺寸 | ○ | ○ | 浅 | 宽 | ◎ | memcmp |
| **zstd/zlib/isal (11)** | ○ | ○(库内) | ○ | — | — | — | ○ | ◎压缩窗口 | ○ | ○ | 中 | 低 | ○ | byte-exact 往返 |

### 矩阵读法(按列汇总)

- **SVE 真·指令级只有 11 个测试**:9×sve512 + sleef_sve + eigen_svd_cdouble_sve。全部集中在 f64/f32 算术链 + gather/scatter + stencil;**无 SVE 谓词压力**(全部 `svptrue` 满谓词)、无 SVE2、无 SME、无 FCMLA(复数 FMA)、无 BFDOT/SDOT/UDOT 点积、无 F16/BF16。
- **NEON FMA 密度最高**的是 openblas_{d,s,z}gemm(手写 fmla 微核)与 sleef(多项式链);fma/ 家族多为 1-4 op 短序列,寄存器堆占用低。
- **特殊值**:fpu_special_values(NEON 12 值笛卡尔)与 sve512_f64_special(分类)覆盖了 NaN/Inf/denorm 的输入侧;**FPCR 舍入模式切换、FP exception trap 路径零覆盖**(全仓 `grep fesetround|FPCR` 无命中,仅注释提及)。
- **校验策略三层**:byte-exact memcmp(主流,最强)、容差(fma/fma_patterns 1e-6、acl_gemm 1e-3 — 对 1-bit SDC **盲**)、算法自检(ooo_dep_chain 校验和、simple_add 线程一致)。

---

## 3. 框架能力盘点

### 能支持

- **每 CPU 一线程 + 绑核**:框架自动 pin(docs/writing_tests.md "Test execution" 步骤 3);`-n`/`--cpuset` 可缩小;`test->max_threads` 支持 slicing(声明字段存在,当前测试无一使用)。
- **fork 隔离 + fracturing**:崩溃归 child(CrashContext);重复运行换种子。
- **RNG 可复现**:框架 RNG(Constant/LCG/AES),`-s seed` 重放(docs/writing_tests.md "Random numbers")。
- **校验时机自由**:TEST_LOOP 粒度 N 自选 — 立即校验(openblas_dgemm 每迭代 memcmp)或累积后校验均可;`memcmp_or_fail` 类型感知、失败打印 actual/expected/mask 64B hex。
- **测试专属旋钮**:`get_testspecific_knob_value_int(test,"mdim",256)`(dgemm.cpp:72)→ 命令行 `-O test.knob=N`,可做尺寸扫描。
- **拓扑可见**:`device_info[cpu].numa_id/die_id/core_id`(memcpy_rewr.cpp:534-541 的 numa_split 策略即基于此)。
- **alloc 覆写**:分配失败即退出、返回零内存(writing_tests.md "Memory allocation")。
- **多库静态链接**:Eigen5/OpenBLAS/SLEEF/pocketfft 各自隔离命名空间(EigenSVE/EigenAVX2)避免符号冲突。
- **inline asm 直插**:neon_rot_2src.cpp:45 用 asm 边界保证 `str q→ldr q` 背靠背不被编译器消除 — 指令级控制先例。

### 不能支持 / 缺失

- **无 FPCR/FPSR 控制助手**:没有 helper 读写 FP 舍入模式(FPCR.RMode)、flush-to-zero(FPCR.FZ)、trap 使能 — 需要测试自己 `mrs/msr fpcr`(仓内零先例)。
- **无 cache flush 数据面 helper**:`__clear_cache` 是 I-cache 语义(cache_stress_aggressor.cpp:24-28 注释自承"对数据也有效"是存疑的近似);dc civac 只在 l2c_cross_cache_line_arm.cpp:163 内联 asm 局部使用,未成公共 helper。
- **无跨核同步栅栏助手**:power_virus_dit 想要"多核 lockstep burst"但各线程独立 do-while,实际相位随机漂移;框架无 barrier/启动同步 API。
- **NUMA 归属控制不直接**:框架报 numa_id,但分配策略(first-touch/mbind/set_mempolicy)无封装;memcpy_rewr 用 numa_split 角色分派是唯一近似(不带 mbind)。
- **校验延迟只有测试自己管理**:框架不提供"延迟校验/带内 vs 带外"模式开关。
- **无 per-lane/per-thread故障定位输出标准化**:dump 变体(eigen_gemm_*_dump、movbe_dump)各自手写。

---

## 4. 差距分析(按激发维度,验证后据实)

按「对该 CPU FP/vector SDC 激发的要紧程度」排序:

1. **舍入模式 / FPCR 压力 — 空白(最要紧的空白之一)**。全仓无任何 FPCR 操控(grep 证实)。ARM64 的 RNE/RTZ/RDN/RUP 四模式在向量 FMA 的舍入网络是独立硅区域;denormal 处理还分 FZ 开关。fpu_special_values 只扫了操作数空间,没扫配置空间。SDC 文献(蒸馏综合)确认边界值 × 配置翻转是最脆弱的组合之一。
2. **FP exception/状态路径 — 空白**。FPSR 累积标志(IO/OF/UF/DZ/IX/ID)的读写、sNaN→qNaN 静默化路径只在 fpu_special_values 顺带打到(fpu_special_values.cpp:24-31 特意构造真 sNaN,是好开端),但 FPSR 标志校验、异常 trap 模式完全没测。
3. **SVE 谓词维度 — 空白**。11 个 SVE 测试全部 `svptrue` 满谓词。SVE 区别于 NEON 的核心机制 — 逐 lane 谓词(`whilelt` 生成、部分活跃、谓词翻转/前导零计数)、尾循环处理 — 是 SVE 硬件独有的新增数据通路(TaiShan v110 上 SVE 谓词网络是全新逻辑),SDC 风险最高的"第一代新指令"(PinDrop Obs12 已在 sleef meson 注释引用)。零覆盖。
4. **半精度/BF16 — 空白**。无 FP16/BF16 NEON/SVE 测试。Kunpeng920 支持 FP16 计算;BF16 是服务器 ML 负载主流。CLAUDE.md 提到框架在 sandstone_data.cpp 统一静默 SNaN 正是"为 FP16/BF16 NaN 位模式跨架构一致"设计的 — 但没有任何测试用上这个能力。
5. **FCMLA(复数 FMA)— 空白**。eigen_svd_cdouble 走 Eigen 复数乘(标量展开),没有测试直接打 NEON/SVE FCMLA 指令;zgemm 走 OpenBLAS 复数 kernel(间接)。通信/AI 负载的真实复数 FMA 通路无指令级测试。
6. **SVE2/SME — 空白**。simd-arm.conf 定义了特征位但无测试。本 CPU(0xd22)无 SME,但 SVE2 特定子集亦未探明使用 — 该维度对"这颗 CPU"价值为 0,对未来服务器(SME 是 ARM 服务器路线图主线)是前瞻空白。
7. **跨核共享数据 + FP 组合 — 弱**。mesh_upi 27 个测试全是整型(int32x4_t);**没有一个跨核测试在共享数据上做浮点计算**。真实 SDC 故障(如 NUMA3 VA 通路)是"共享/远端访问 × 计算"复合,当前只有 memcpy_rewr(整型 memcpy)覆盖前半。
8. **校验容差掩盖 1-bit SDC — 设计缺陷(已部分自愈)**。fma.cpp:21-30 与 fma_patterns(1e-6)、acl_gemm(1e-3)容差会放过单 bit 翻转 — fsu_byteexact_arm.cpp:10-15 的注释已自我诊断此问题并给出 byte-exact 版本;fpu_special_values 同理。但 fma/fma_patterns/acl_gemm 本身未改,仍在 PROD 默认集合里以弱校验运行。
9. **RNG 不可复现缺陷 — 已知缺陷(如实记录)**。fma/、fma_patterns、fmatail、gather 共 20 个文件用 `std::mt19937(std::random_device{}())` 而非框架 RNG(文件清单见 grep) — 违反 writing_tests.md 的可重放要求,失败无法用 `-s seed` 复现,且 libc rand 覆写在此时不起作用。
10. **Eigen SVD 192 核 ULP 假阳 — 已知缺陷**(CLAUDE.md "Known platform quirks"):并行 BDCSVD 的归约顺序在多线程下 ULP 漂移,byte-exact memcmp 造成偶发假阳;`-n 1` 才稳。这是 golden-memcmp 模式对"非确定性并行算法"的固有错配(见 §5)。
11. **寄存器堆压力 — 弱**。只有 openblas/sleef/eigen(库内自然压力)和 sve512 SVD 大工作集;显式 NEON 32×128-bit / SVE 32×512-bit 寄存器堆满压力(如 24+ 活跃向量寄存器的手写链)无专门测试。fma/ 家族最多 12 寄存器。
12. **FMA 与非 FMA 混排 — 空白**。无测试在同一链中交替 vfma/vmul+vadd(或 FADD 与 FMUL 分离舍入路径),而两路径的舍入网络不同,混排是真实的边界压力。
13. **验证延迟维度未利用**:openblas_dgemm 的 copy→compute→verify 每迭代即时校验是 CORE179 判别结构;但没有测试做"长窗口累积后才校验"(捕捉瞬态窗口故障需要暴露窗口,而非即时捕捉)。sve512_stencil 的 1e8 迭代后校验是唯一长窗口例子(固定 100'000'000,tests/arm64/sve512_stencil_axis_arm.cpp:70)。
14. **CLZ/桶形移位/向量-标量混合 SVE(svaddv 归约、svsqrt、svdiv)— 空白**。SVE 测试全是 svmla+访存;除法/开方/横向归约(每个都是独立数据通路)零覆盖。

**对 SDC 激发最要紧的前 5**(结合该 CPU 已知故障签名 NUMA3 VA 通路 × ld1rd/stnt1d 重装载窗口 × 簇饱和):
(a) 谓词/非满活跃 SVE(新指令第一代风险);(b) FPCR 舍入×操作数笛卡尔(配置×边界组合);(c) 跨簇共享数据上的 FP 计算(复现已知签名路径);(d) FCMLA/BF16/FP16(未打过的数据通路);(e) 弱校验测试(fma/acl)升级为 byte-exact(否则激发再多也测不出)。

---

## 5. 测试设计形态对比:golden-memcmp vs 算法自检

| 维度 | golden-memcmp(本仓主流) | 算法自检/在线校验(论文式) |
|---|---|---|
| 代表 | openblas_dgemm、sleef、eigen_*、fpu_special_values、sve512 家族 | ooo_dep_chain(校验和)、simple_add(跨线程一致)、memcpy_rewr(生产-消费一致)、crc 家族 |
| 检测强度 | 1-bit ULP 即捕(前提 byte-exact) | 取决于校验函数;校验和有碰撞概率、逆运算有舍入等价盲区 |
| 假阳风险 | **高** — 对并行非确定算法(Eigen 并行 SVD/sparse 归约顺序)ULP 漂移即假阳,192 核实测(CLAUDE.md) | 低 — 自检与实现同序或用等价类 |
| 覆盖代价 | golden 需确定性可复现(同 kernel/同顺序);限制库选型(OpenBLAS USE_LOCKING=1 保证字节一致) | 可测任意黑盒,但有效载荷密度被校验稀释 |
| 适用场景 | 纯函数/固定顺序核(手写 kernel、SLEEF 多项式、确定归约)— **应当用 golden** | 并行库内部顺序不可控(Eigen 多线程)、或需在故障现场保留现场(逆运算定位 lane) |

**结论性判断(供增强提案引用)**:本仓 304 个测试里 golden-memcmp 约 7 成、自检约 1 成、容差约 2 成(fma 家族+acl)。形态本身选对了 — 对"手写 NEON/SVE kernel + 框架可控顺序"的字节级比较是 SDC 检测的最强形态;问题集中在 (i) 少数容差测试是盲区,(ii) Eigen 并行算法上 memcmp 错配(应换自检或 -n 1),(iii) 没有"长延迟+高密度激发"的混合形态(stencil 是孤例)。

---

## 附:证据文件清单(主要)

- tests/cpu/fma/{fma,fpu_special_values,fmatail_avx2,fma_patterns_avx512_ps}.cpp
- tests/cpu/openblas_gemm/{dgemm,zgemm}.cpp(mdim 旋钮:72;copy/verify:131-147)
- tests/cpu/sleef/{sleef_neon,sleef_sve}.cpp
- tests/cpu/pocketfft/fft.cpp
- tests/cpu/eigen_svd/sandstone_eigen_common.h(memcmp U/V:53-64)
- tests/cpu/eigen_gemm/{gemm_double_dynamic_square,gemm_double_dynamic_square_dump}.cpp
- tests/cpu/arithmetic_arm/{acl_gemm(容差 118-127),fisttp_arm,crt_builtins}.cpp
- tests/cpu/arm64/{sve512_f64_chain_arm,sve512_f32_chain_arm,sve512_f64_special_arm,sve512_gather_scatter_arm,sve512_stencil_axis,power_virus_dit,fsu_byteexact_arm,ooo_dep_chain_arm,agu_stress_2src,neon_rot_2src,l2c_cross_cache_line_arm(dc civac:163)}.cpp
- tests/cpu/gather/gatherscatter_f64.cpp(标量模拟证据:30-70)
- tests/cpu/mesh/mesh_upi_avx512_symm_int.cpp(int32 证据:87-127)
- tests/cpu/memory/{memcpy_rewr(numa_split:534-541),mmu_stress_arm}.cpp
- tests/cpu/meson.build(SVE 编译档:882-917;openblas/sleef/pocketfft 接入:459-554)
- tests/cpu/arm64/meson.build(sve512 家族注释:143-227)
- framework/sandstone.h(TEST_LOOP:144;memcmp_or_fail:707-812;aligned_alloc_safe:559)
- framework/device/cpu/simd-arm.conf(sme 位:77-87)

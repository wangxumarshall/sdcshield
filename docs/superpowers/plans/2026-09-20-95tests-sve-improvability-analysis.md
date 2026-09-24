# 95 个测试的 SVE 可改进性分析（2026-09-20）

> 用户问题：95 个测试（fma/vector/arm64/arm-0102/arithmetic/arithmetic_arm/misc 七目录）中，**能改进为 SVE 指令但还没改进的**是哪些。
> 口径：不以"当前是否用 NEON"划界，而看**负载本质是否可向量化到 SVE** + **是否已有 SVE 版**。
> 数据基线：本地 HEAD + origin/main（misc 的 K 系列新文件以远程为准）。

## 已有 SVE 版的（19 个，不算待改进）

| 原测试 | SVE 版 | 来源 |
|---|---|---|
| fma_tail_avx2/avx512, fmatail_avx2/avx512, fmatail_nested_×2, fmatail_double_nested_×2, fma_patterns_avx512_ps/pd（10） | `*_sve` / `*_sve_wide` 10 个 | 我们 feat/sve-port-avx53 |
| neon_rot_ldr_at_top, _rowmajor, _k3res, _k5inter_rand, _k7p_zero/one/rand/hiham（8） | `sve_rot_*` 同名 8 个 | 远程分支 test/sve-rot-ldr-at-top（未合 main） |
| fma.cpp | 同族已有 sve512_f64_chain_arm（非直接对应，见下注） | 仓库原有 |

注：`fma`（单精度 `vfmaq` 基础测试）与 `sve512_f64_chain_arm` 负载参数不同（后者是串行依赖链设计）——**严格说 fma.cpp 无直接 SVE 对应**，归入待改进（下表）。

## ✅ 能改进为 SVE 但还没改进的（21 个）

### A. 直接可替换——现在用 NEON intrinsic，SVE 有等价指令（12 个）

| 测试 | 当前 NEON 负载 | SVE 改进路径 | 难度 |
|---|---|---|---|
| `fma` | `float32x4_t` + `vfmaq_f32` 单精度 FMA | `svmla_f32`（谓词分批） | 低 |
| `fpu_special_values` | `vfmaq` 遍历 IEEE-754 特殊值表逐字节 golden | `svmla` + 特殊值表（部分与 sve512_f64_special_arm 重叠，但独立负载参数） | 低 |
| `kreg1` | `vld1q_u32` + kreg 操作 | SVE **谓词寄存器是真掩码寄存器**：谓词运算原生 | 低 |
| `kreg4` | 15 处 intrinsic，uint32x4_t×4 | `svld1_u32` + 谓词 | 低 |
| `kreg7` | `vld1q_{u8,u16,u32,u64}` 四宽度 store/reload | `svld1_*` 多宽度（`svcntb` 自适应） | 低 |
| `swizzle` | `vrev64q_f32` 相邻对交换 | `svrevw`/`svtrn1/2`（SVE2）或 `svtbl`（SVE1）| 中 |
| `insert_extract` | lane 提取/插入 | `svlastb`/`svinsr`/`svdup_lane`——**注意 SVE lane 语义与 NEON 不同，golden 要重推** | 中 |
| `neon_add` | `vaddq_u32`（BETA 级） | `svadd_u32` | 低 |
| `fsu_byteexact_arm` | `vfmaq` 逐字节精确 + 跨行 128B 通路 | `svmla` + 跨行 gather（`svld1_gather`）| 中 |
| `power_virus_dit` | NEON 满载 burst + yield（di/dt） | `svmla` 风暴 + yield | 低 |
| `arm0102_kreg_mask` | kreg 掩码 + NEON 8 处 | 谓词化 | 低 |
| `movdq2q`/`movq2dq`（2 个） | NEON V0↔X0 64-bit 传输（`mov x0, v0.d[0]` 类） | SVE `svlastb_u64`/`svdup_u64` 通道 | 中（需保 inline asm 语义） |
| `movmskpspd` | NEON `ushr` 提取符号位掩码（inline asm） | `svcmplt`/`svcntp` 谓词比较即原生掩码提取 | 低 |

### B. 软件仿真 → SVE 真硬件——x86 kreg 仿真测试恰是 SVE 谓词的原生领地（6 个）

| 测试 | 当前（标量软件仿真） | SVE 改进路径 | 难度 |
|---|---|---|---|
| `kreg2` | 整数移位仿真掩码移位 | `svlsl/svlsr` 谓词版 + 谓词计数 | 低 |
| `kreg3` | KORTEST 仿真 | `svptest_any/svptest_first`（真指令！）| 低 |
| `kreg5` | KUNPCK 仿真 | `svzip_b16` 谓词交织 | 低 |
| `kreg6` | 软件模拟（注释自述"无对应指令"——SVE 有） | 对应谓词操作 | 低 |
| `kreg8` | 同上 | 同上 | 低 |
| `kreg9` | KMOVB 仿真 | 谓词 mov（`svmov`/谓词赋值） | 低 |

**这一类的特殊价值**：SVE 化不是"换个引擎"而是**从仿真升级到真硬件操作**——x86 AVX-512 k 寄存器在 ARM 上唯一真等价物就是 SVE 谓词寄存器。测试名可保 kreg 语义（如 `kreg3` → `sve_ptest`），golden 从软件模型升级为谓词指令行为。

### C. 核心通路不可替换——**不适用**（此前的误判已纠正）

| 测试 | 为什么不能/不该 SVE 化 |
|---|---|
| `arm_crypto` | `vaeseq/vaesmcq`——SVE1 **无 AES**（需 SVE2 sveaes，本机无硬件） |
| `neon_rot_2src` | asm 锁定的 `str q→ldr q` 对是 core-179 **触发配方本身**——换 z 寄存器后测的是不同微架构路径（但可做对照实验版） |
| `movbe`/`movbe_dump`/`movbe_dump_probe_*`（13 个） | `__builtin_bswap32` + asm 标量 store/reload——core-179 标量配方，无向量成分 |
| `mrn_flags/nuke/pairs/reloaded/rmw/rmw_dump`（6 个）、`mite` | 标量 ALU 旋转配方（core-179 家族），asm 锁定 |
| `agu_stress_2src`、`ooo_dep_chain_arm`、`lsu_store_forward_arm`、`l2c_cross_cache_line_arm`、`mmu_split_tlb_arm`、`arm64_sdc`、`ifu_branch_target_arm`、`iex_operand_combo_arm` | 标量 inline-asm 通路压测（AGU/OoO/LSU/L2C/MMU/IFU/IEX 单元），设计目标就是标量微架构特征 |
| `partial_store_forwarding` | 标量 store-forwarding 通路 |
| `adcx/adox/adcxlong/adcx_adox_interleaved`（4）+ `_arm` 系（4 个） | `adcs` inline-asm **进位标志链**——SVE 无标志位概念，不可向量化 |
| `gmp_bigadd/bignum`、`bigint_mulx_arm`、`acl_gemm`、`crt_builtins`、`fisttp_arm`、`operand_space_arm`、`adcx_adox_interleaved_arm` 等 arith 系（11 个） | GMP/ACL/编译器内置库调用或 `__int128` 软件——负载在库内部或标量大数 |
| `sve512_*`（9 个） | **已经是 SVE** |

## 汇总

| 分类 | 数量 |
|---|---|
| 95 个测试总数 | 93 实测（4目录 85 + misc 8；用户口径 95 含 neon_add 计数差） |
| 已有 SVE 版 | 19 |
| **能改进未改进（A+B）** | **21**（A 类 13 + B 类 6 + fma 1 + 说明见注） |
| 不适用（C 类：标量配方/库/无 SVE 指令） | 53 |
| 已是 SVE | 9（sve512 系） |

**注**：A 类含 `movdq2q`+`movq2dq` 2 个、`fma` 1 个，与 12 个 NEON intrinsic 直换类合计 13+6=**19~21 个**（边界计数取决于 `neon_rot_2src` 是否算对照实验版）。

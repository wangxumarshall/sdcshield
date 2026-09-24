# 95 个测试的 SVE 可转换性分析 v2（2026-09-20，修正口径）

> **用户纠正后的正确口径**：计算类负载（矩阵乘加、向量计算、大数运算、数据变换）——只要计算内容本身可以向量表达，就应该能转换 SVE，而不因"asm 锁定标量配方会改变微架构特征"被排除。上一版（v1）用了错误的排除标准，本版修正。
> **判断标准改为**：负载的**计算语义**能否映射到 SVE 指令（含谓词/进位处理重构），且保持"输入→golden 输出"的计算等价。微架构特征变化是 SVE 化的**固有属性**（正如 53 个 avx 测试从 NEON→SVE 也改变了特征），不是障碍。

## 修正后：能转换 SVE 但还没转换的（大幅扩大）

### A. NEON intrinsic/asm 直换（14 个）——v1 已正确识别

`fma`、`fpu_special_values`、`kreg1`、`kreg4`、`kreg7`、`swizzle`（vrev64q→svtbl/svtrn）、`insert_extract`、`neon_add`、`fsu_byteexact_arm`、`power_virus_dit`、`arm0102_kreg_mask`、`movdq2q`、`movq2dq`、`movmskpspd`（ushr→谓词比较）

### B. kreg 软件仿真 → SVE 谓词真硬件（6 个）——v1 已正确识别

`kreg2`（移位→谓词移位）、`kreg3`（KORTEST→svptest_any）、`kreg5`（KUNPCK→svzip）、`kreg6`、`kreg8`、`kreg9`（KMOVB→谓词 mov）

### C. 【v1 误排除，本次修正】计算类标量负载——计算语义可向量表达（15 个）

| 测试 | 标量计算负载 | SVE 转换路径 | 备注 |
|---|---|---|---|
| `adcx` | 大数加进位链（a+b+carry 逐元素，golden=`__int128` 模拟） | `svadd` + 谓词控进位传播：每 64-bit lane `svadd` 出和与溢出（`svcmphi` 比较/`svqadd`），进位用 `svaddv`/前缀处理重算——**计算等价可证**（golden 不变，逐元素比对不变） | 多字进位跨 lane 传播需分两步（lane 内加 + 进位收集再前缀加），负载等价 |
| `adcx_arm` | 同上（ARM adcs 版） | 同上 | |
| `adcxlong` | 长进位链 | 同上 | |
| `adox` | 溢出链（O 标志版） | 同 adcx（溢出检测 `svcmplt` 判 wrap） | |
| `adox_arm` | 同上 | 同上 | |
| `adcx_adox_interleaved` | 双链交错 | 两条 svadd 链交错 | |
| `adcx_adox_interleaved_arm` | 同上 | 同上 | |
| `operand_space_arm` | 加法器/进位链/乘法器部分积路径压测（`__int128` 软件） | `svadd`/`svmul`/`svmla` 向量化操作数空间（高位操作数即高汉明距离 lane 数据） | 压测对象从标量 ALU 变向量 ALU——正是 SVE 化意义 |
| `fisttp_arm` | float→int 截断转换（FCVTZS） | **`svcvt_s32_f32_z` 系即原生 SVE 转换指令**，逐元素 golden 不变 | 最直接的转换之一 |
| `movbe` 系 13 个（`movbe`、`movbe_dump`、`movbe_dump_probe_a..xn`） | 字节交换往返（bswap32 + store/reload/compare） | **`svrevb`/`svrevw`（SVE 字节反转）** + `svst1/svld1`——字节交换是纯数据变换，向量表达完全等价 | core-179 探针组：换 SVE 后测的是"向量通路上的字节交换触发"——配方判别实验的自然延伸（与 neon_rot_2src→SVE 对照同理） |
| `mrn_flags` | 标志位操作 | 标志是标量概念；但底层旋转/ALU 数据流可 `svror` 类向量化 | 需重构 |
| `mrn_nuke`/`mrn_pairs`/`mrn_reloaded`/`mrn_rmw`/`mrn_rmw_dump` | 旋转 ALU + store/reload（core-179 家族） | `svadd/sveor/svand/svorr` 旋转组合 + `svst1/svld1`——数据流等价 | 同 movbe 系 |
| `mite` | 标量 ALU 微 ops | `svadd` 等向量微 ops 等价负载 | |
| `partial_store_forwarding` | 部分 store→load 转发 | `svst1`/`svld1` 谓词控部分写（谓词=部分宽度的原生表达！SVE 谓词部分写比 NEON 更精确） | 谓词部分 store 是 SVE 独有能力 |
| `agu_stress_2src` | 2 源加载 + ALU + store/reload（AGU 压力） | `svld1_gather`（**SVE gather 即多源加载的原生形态**）+ svALU + svst1 | gather/scatter 正是 AGU 压力的向量等价物 |

### D. 库负载（计算在 GMP/ACL 内部）——转换 = 换自实现（5 个，工作量另计）

| 测试 | 当前 | 转换路径 |
|---|---|---|
| `gmp_bigadd` | GMP `mpz_add` | SVE `svadd` 进位链自实现大数加（与 C 类 adcx 同技术） |
| `gmp_bignum` | GMP 乘/幂 | `svmul`/`svmadd` 部分积 |
| `bigint_mulx_arm` | `__int128` 大数乘 | `svmul` 高低半拆分 |
| `acl_gemm` | Arm Compute Library GEMM | 已有 openblas SVE 化先例路径（但 Arm CL 本身可能内部已有 NEON） |
| `crt_builtins` | compiler-rt 内置 | 部分可向量（除法/软浮点模拟） |

### E. 微架构单元压测（LSU/OoO/L2C/MMU/IFU/IEX）——数据流可向量表达但单元语义会变（7 个，转=测不同单元）

`lsu_store_forward_arm`（store/reload→svst1/svld1 转发）、`l2c_cross_cache_line_arm`（跨行 RMW→向量版）、`ooo_dep_chain_arm`（依赖链→svmla 串行链=**已有 sve512_f64_chain 同构**）、`mmu_split_tlb_arm`（页表访问→向量 gather 跨页）、`ifu_branch_target_arm`（分支目标→SVE 无向量分支，**真不可转**）、`iex_operand_combo_arm`（操作数组合→sv ALU 组合，**可转**）、`arm64_sdc`（混合→部分可转）

## 修正后总计

| 类别 | 数量 |
|---|---|
| A（NEON 直换） | 14 |
| B（kreg 仿真→谓词） | 6 |
| C（计算类标量→SVE，v1 误排除） | 15+（含 movbe 系 13 个则 27） |
| D（库负载自实现） | 5 |
| E（单元压测，转=换测对象） | 6 可转 + 1 真不可转（ifu） |
| **能转换未转换合计** | **约 40-46 个**（不含已 19 个有 SVE 版 + 9 个已是 SVE） |

**真不可转的只剩**：`arm_crypto`（SVE1 无 AES）、`ifu_branch_target_arm`（SVE 无向量分支指令）、以及进位标志链若坚持"必须测标志位微架构"的窄口径（但计算等价口径下 adcx 系可转）。

# 64 个可转换未转换测试的名单与分批计划（2026-09-20）

> 统计口径：95 个测试全集（fma/vector/arm64/arm-0102/arithmetic/arithmetic_arm/misc 七目录，以 origin/main 为准），
> 排除：已有 SVE 版 18 个（avx53 移植 10 + 远程 sve-rot 分支 8）、已是 SVE 9 个（sve512 系）、真不可转 2 个（arm_crypto：SVE1 无 AES；ifu_branch_target_arm：SVE 无向量分支）。
> **可转换未转换：64 个**。
> 转换原则（延续 avx53）：计算负载内容不变（输入/golden/比对结构保留），引擎换 SVE1 谓词化指令；命名原测试语义名 + SVE 引擎后缀（无 NEON/AVX 字样的直接加 `_sve` 后缀）；每批走 122 两阶段验证（offline 122 → 60s 全 pass → online 122 → 全核验证）。

## 名单（64 个，按批次分组）

### 批次 1：kreg 软件仿真 → SVE 谓词真硬件（7 个）【价值最高：从仿真升级到真指令】

| # | 测试 | 当前实现 | SVE 目标指令 |
|---|---|---|---|
| 1 | `kreg2` | 整数移位仿真掩码移位 | 谓词移位 + svcntp |
| 2 | `kreg3` | KORTEST 软件仿真 | `svptest_any/svptest_first`（已验证 GCC12 可用） |
| 3 | `kreg5` | KUNPCK 软件仿真 | 谓词交织（向量位操作载体） |
| 4 | `kreg6` | 软件模拟（自述"无对应指令"） | 对应谓词操作 |
| 5 | `kreg8` | 软件模拟 | 对应谓词操作 |
| 6 | `kreg9` | KMOVB 仿真 | 谓词 mov/加载 |
| 7 | `arm0102_kreg_mask` | kreg 掩码 + NEON | 谓词化掩码运算 |

### 批次 2：NEON intrinsic 直换（12 个）【机械性最强，快速铺量】

| # | 测试 | NEON → SVE 映射 |
|---|---|---|
| 8 | `neon_add` | `vaddq_u32` → `svadd_u32` |
| 9 | `fma` | `vfmaq_f32` → `svmla_f32` |
| 10 | `fpu_special_values` | `vfmaq` 特殊值表 → `svmla` 同表 |
| 11 | `kreg1` | `vld1q_u32` kreg 操作 → 谓词版 |
| 12 | `kreg4` | 15 处 intrinsic → 谓词版 |
| 13 | `kreg7` | `vld1q_{u8,u16,u32,u64}` 四宽度 → `svld1_*` |
| 14 | `swizzle` | `vrev64q_f32` → `svtbl` 周期索引 |
| 15 | `insert_extract` | lane 提取/插入 → `svlastb/svinsr/svdup`（lane 语义重推 golden） |
| 16 | `fsu_byteexact_arm` | `vfmaq` 逐字节精确 + 跨行 → `svmla` + 跨行访问 |
| 17 | `power_virus_dit` | NEON burst+yield → `svmla` 风暴+yield |
| 18 | `movdq2q` | asm `mov x0, v0.d[0]` → SVE 向量↔标量通道 |
| 19 | `movq2dq` | 反向同上 |
| 20 | `movmskpspd` | asm `ushr` 符号位 → `svcmplt`+`svcntp`（谓词即掩码） |

### 批次 3：转换换算类（2 个）【一比一指令映射，已验证语义】

| # | 测试 | SVE 路径 |
|---|---|---|
| 21 | `fisttp_arm` | FCVTZS 截断 → `svcvt_s32_f32_z`（已实测 round-toward-zero 语义一致） |
| 22 | `iex_operand_combo_arm` | 标量 ALU 操作数组合 → svALU 组合（rbit/rev/clz 有 SVE 版） |

### 批次 4：进位链/大数运算 SVE 化（9 个）【技术难点批：进位跨 lane 传播】

| # | 测试 | SVE 路径 |
|---|---|---|
| 23 | `adcx` | `svadd`+`svcmplt` 逐 lane 和/进位 + 前缀传播（已验证可行） |
| 24 | `adcx_arm` | 同上 |
| 25 | `adcxlong` | 同上（长链） |
| 26 | `adox` | 溢出检测 `svcmplt` wrap 判断 |
| 27 | `adox_arm` | 同上 |
| 28 | `adcx_adox_interleaved` | 双 svadd 链交错 |
| 29 | `adcx_adox_interleaved_arm` | 同上 |
| 30 | `operand_space_arm` | `svadd/svmul/svmla` 高汉明 lane 操作数空间 |
| 31 | `bigint_mulx_arm` | `svmul` 高低半拆分大数乘 |

### 批次 5：core-179 数据流配方向量版（21 个）【数量最大批：同一骨架模板化】

| # | 测试 | SVE 路径 |
|---|---|---|
| 32 | `movbe` | bswap32 往返 → `svtbl` 字节交换 + `svst1/svld1` |
| 33 | `movbe_dump` | 同上 + dump |
| 34-45 | `movbe_dump_probe_{a,b,c,d,e,f,g1,g2,h,x,xn}`（12 个） | 同骨架逐 probe 语义保留 |
| 46 | `mrn_flags` | 旋转 ALU → svALU 组合 |
| 47 | `mrn_nuke` | 同上 |
| 48 | `mrn_pairs` | 同上 |
| 49 | `mrn_reloaded` | 同上 + store/reload |
| 50 | `mrn_rmw` | 同上 + RMW |
| 51 | `mrn_rmw_dump` | 同上 + dump |
| 52 | `mite` | 标量微 ops → 向量微 ops |
| 53 | `neon_rot_2src` | uint64x2 旋转+str/ldr 对 → SVE 向量版（对照实验价值：判别配方是否走 SVE 通路） |

### 批次 6：访存/单元压测向量版（7 个）【SVE 独有能力发挥批】

| # | 测试 | SVE 路径 |
|---|---|---|
| 54 | `partial_store_forwarding` | **谓词部分 store**（SVE 比 NEON 更精确的原生表达） |
| 55 | `agu_stress_2src` | `svld1_gather`（gather=多源加载原生形态） |
| 56 | `lsu_store_forward_arm` | `svst1/svld1` 转发，多宽度 |
| 57 | `l2c_cross_cache_line_arm` | 跨行向量 RMW |
| 58 | `mmu_split_tlb_arm` | 向量 gather 跨页/分裂访问 |
| 59 | `ooo_dep_chain_arm` | svmla 串行依赖链（sve512_f64_chain 同构先例） |
| 60 | `arm64_sdc` | 混合负载部分向量化 |

### 批次 7：库负载自实现（4 个）【工作量最大批，SVE 重写库语义】

| # | 测试 | SVE 路径 |
|---|---|---|
| 61 | `gmp_bigadd` | SVE 进位链大数加自实现（批次 4 技术复用） |
| 62 | `gmp_bignum` | `svmul` 部分积大数乘 |
| 63 | `crt_builtins` | 软浮点/除法向量化 |
| 64 | `acl_gemm` | SVE GEMM 自实现（或如实标注 ArmCL 内部路径） |

## 批次排序依据

1. **批次 1 先行**：kreg 仿真是"x86 k 寄存器无真等价"的历史妥协，SVE 谓词是唯一真等价物——转换价值最独特，且实现直接（谓词指令已验证可用）
2. **批次 2 铺量**：与 avx53 移植同模式，机械性最强，快速产出
3. **批次 3 顺手**：svcvt 一比一映射已验证
4. **批次 4 攻坚**：进位链 SVE 化是技术难点也是亮点（已写探针验证计算等价），为批次 7 的 gmp_bigadd 提供技术
5. **批次 5 模板化**：21 个同骨架，写一个模板批量生成+逐个核对 probe 语义
6. **批次 6 发挥 SVE 独有优势**：谓词部分写、gather 是 NEON 没有的精确表达
7. **批次 7 收官**：依赖批次 4 的进位/乘法技术积累

每批完成 = 一个 commit + 122 两阶段验证（阶段 1 排除 122 全 pass 证明逻辑正确；阶段 2 全核观察 122 是否触发——每批都是 122 故障特性的新探针）。

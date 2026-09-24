# 我们扩展的 SVE 指令测试用例清单（2026-09-21 锁定）

> 统计口径：`tests/cpu/sve/` 目录下我们新增的全部测试（分支 `feat/sve-port-avx53`，main 上该目录不存在——全部为本次扩展）。
> 仓库既有的 SVE 测试（sleef_sve、eigen_svd_cdouble_sve、sve512_* 系列，共 11 个）不计入。

## 总数：116 个已注册（117 个源文件；crt_builtins_sve 因本机无 libclang_rt.builtins 与原版同 gate 未构建）

### 一、avx53 移植（53 个）— 53 个 avx 命名 NEON 测试的 SVE 版

| 域 | 数量 | 测试 |
|---|---|---|
| FMA | 10 | fma_tail_sve(_wide), fmatail_sve(_wide), fmatail_nested_sve(_wide), fmatail_double_nested_sve(_wide), fma_patterns_sve_wide_ps/pd |
| Mesh 框架A+B | 18 | mesh_upi_sve_* 全系（sym/symm/asymm/distrib/read/write/L3 × 普通与 _wide） |
| eigen | 1 | eigen_svd_bidiag_sve（自写 Householder 双对角化） |
| IPSec | 24 | ipsec_*_sve(_wide) 全系（17 个 EVP+多流 SVE HMAC + 7 个 EVP-only） |

### 二、64 转换计划（63 个本机构建 + 1 个同 gate）— "能转未转"清单的全覆盖

| 批次 | 主题 | 数量 | 代表测试 |
|---|---|---|---|
| 批1 | kreg 软件仿真 → SVE 谓词真硬件 | 7 | kreg2/3/5/6/8/9_sve, arm0102_kreg_mask_sve |
| 批2 | NEON intrinsic 直换 | 13 | neon_add_sve, fma_sve, fpu_special_values_sve, power_virus_dit_sve, movdq2q/movq2dq/movmskpspd_sve, fsu_byteexact_sve, kreg1/4/7_sve, swizzle_sve, insert_extract_sve |
| 批3 | 换算类 | 2 | fisttp_sve (svcvt), iex_operand_combo_sve (svclz+svtbl) |
| 批4 | 进位链/大数 | 10 | adcx/adox/adcxlong/interleaved ×2 系, operand_space_sve, bigint_mulx_sve |
| 批5 | core-179 数据流（含保真修复） | 21 | movbe_sve, movbe_dump_sve, 11×movbe_dump_probe_*_sve, 6×mrn_*_sve, neon_rot_2src_sve, mite_sve |
| 批6 | 访存/单元压测 | 7 | partial_store_forwarding_sve (谓词部分写), agu_stress_2src_sve, lsu_store_forward_sve, l2c_cross_cache_line_sve, mmu_split_tlb_sve, ooo_dep_chain_sve, arm64_sdc_sve |
| 批7 | 库负载 SVE 自实现 | 3+1 | gmp_bigadd_sve, gmp_bignum_sve, acl_gemm_sve + crt_builtins_sve(本机 gate，源码在库) |

## 质量等级分布

- PROD：114 个
- BETA：2 个（neon_add_sve、arm64_sdc_sve——均照抄原版等级）

## 支撑资产（非测试）

- `tests/cpu/sve/ipsec/sve_sha_kernels.h` — 多流 SVE SHA-1/224/256/384/512 压缩内核（OpenSSL 比对 5200/5200）
- `tests/cpu/sve/ipsec/sve_hmac.h` — 多流 SVE HMAC 层（OpenSSL 比对 1360/1360）
- `scripts/sve-sha-verify/` — 内核验证程序

## 全部已完成两阶段验证（122 号核心流程）

每批：阶段1（--cpuset='!122'）零 fail 证明逻辑正确；阶段2（全核含 122）观察触发。
累计数据点 #1–#11：122 在所有 60s 窗口负载下未复现 fail（唯一线索：批次 2 首轮的 1 次未归因 fail，记入候选名单待长时窗检测）。

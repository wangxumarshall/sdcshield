# SVE 全 VL 验证运动 — 190×3 证据矩阵（Task 8 收口）

- 日期：2026-10-30（Task 8 产物）
- 分支：`feat/sve-all-vl-verification-2026-10-28`
- 生成方式：`matrix-gen.sh` 从本目录已入库工件机械生成（**零手填**，脚本一并入库，重跑即复现本文件）
- 行定义：`sve-test-list.txt` 190 行（Task 1 枚举：tag∪name 并集 193 − 3 件不在本机默认集）
- 规模：**190 × 3 = 570 格**（Ruling A：spec 的 564 为 name-grep 估算，被实测枚举取代）
- 验收判据（brief Step 2）：570 格全绿**或有显式机制性理由**（1 格合法 SKIP + 9 格 gem5 边界，见 §2/§5）

## 0. 代码基线与二进制身份

- **代码基线（Ruling G 冻结）**：`12ccda540fd4` + 本运动分支 commits。后者实测只含
  docs / campaign 工具脚本 / gem5 启动脚本（`scripts/eigen-sve-double/gem5/`），
  `git diff 12ccda54..HEAD -- tests/ framework/` 为空——**矩阵证据描述的测试代码与
  基线完全一致**（Task 7 runbook：三腿 570 格分诊 = 零测试代码缺陷）。
- 二进制：native 两腿 `sdcshield-12ccda540fd4`（四份 summary 头 `version:` 行一致，实测）；
  512 腿 gem5 `/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt`
  md5 `3f9011f40dbe80ec1df478c62ddb0b06`（Task 5 终版 9-stub 链）+ 同源 sdcshield。

### 0.1 Ruling G delta：运动期间并行合并、不在本矩阵代码基线内的 PR

本矩阵钉在冻结基线上，不追逐并行 PR（各自 PR-loop 自带验证义务）。核实方式：
`git merge-base --is-ancestor <merge> HEAD` 双双为否（实测）。

| PR | 状态 | 涉及的 campaign 用例（∈190 清单） | 与本矩阵的关系 |
|---|---|---|---|
| #302 `fix(eigen): P0-P3——NUMA 感知联合求解 sizing + 内存最大化 + OOM 防护`（merge `51868e40`） | MERGED | `eigen_gemm_double_max_arm`、`eigen_svd_cdouble_sve`、`eigen_svd_bidiag_sve`（另改 framework sizing） | **不在本矩阵代码基线内**——本矩阵 3 行（9 格）eigen 结论不描述 #302 之后的行为 |
| #316 `fix(sve): #309 同型越界四连修——谓词钳制/尾块 slack/索引纠偏 (#312-#315)`（merge `29da76ef`，2026-10-29） | MERGED | `iex_operand_combo_sve`(#312)、`partial_store_forwarding_sve`(#313)、`mrn_nuke_dense_sve`(#314)、`mesh_upi_sve_wide_asymm_read_L3_int` + `mesh_upi_sve_wide_asymm_read_only_int`(#315)——4 缺陷 5 文件 | 同上——本矩阵上述 15 格的结论不描述 #316 之后的行为 |

（完整"运动期间合并的 PR × 涉及用例"delta 表由 Task 9 文档收尾时统一列出——Ruling G。）

## 1. 512 列采集条件（triage-512.md §0 逐字引用）

> **512 列采集条件**：`-f no` nofork 模式 + 26 项有界 `-O` 覆盖 + gem5 SE
> （binary md5 `3f9011f40dbe80ec1df478c62ddb0b06`，Task 5 终版 9-stub 链）+
> sdcshield `12ccda540fd4`。命令形状：
> `run_sve.sh sdcshield -e <test> -t 500 -n 1 -f no --ignore-timeout
> <26 baked -O> [gem5-overrides.txt 额外降容] --vl 4`（`--vl 4` = 512-bit）。
> 额外降容以 `gem5-overrides.txt` 为权威（现存 **16 条**）：批前预置 2 条
> （sleef nelems=1048576 生效；acl_gemm dim=512 超时 → 预置 256 仍超时 →
> 修订为 **dim=128**）+ 分诊期为超时格补设 14 条（fisttp / eigen_gemm /
> ipsec_cmac / mesh×8 / movdq2q / movq2dq 共 13 条生效转绿；mrn_rmw_diff_1m
> 1 条无效仍边界）。另曾预置 eigen_svd_cdouble mdim 条目——其 clamp 下限
> 即 300，更低 -O 被静默钳回、**无效**，条目已删；**eigen_svd_cdouble_sve
> 为 timeout 边界格**（§5.2），非降容格。
> 判定 = **进程退出码为权威** + 输出 `exit:` 行为佐证（nofork 掩蔽归因
> 细节，单测单进程；exit 行会被叠印间歇抑制，见 §3）。
> **工作量语义（本腿核心限定，见 §2）**：gem5 nofork 下 `-t` 时限预算被 shmem
> 叠印结构性禁用——每测试恰执行 ~1 轮内层循环（TEST_LOOP 粒度或 do-while
> 首轮），pass = 该轮全量 golden 字节比对成立。压力/重复维度由 native 腿
> 承担，本列语义 = 512 档功能性正确性。

## 2. 格值语义与证据列

| 格值 | 含义 | 出处 |
|---|---|---|
| `pass` | 腿内默认/自适应几何下直绿（native：全核 `-t 2000` 实测；512：1 轮全量 golden 字节比对成立） | 各 summary / verdicts |
| `pass(bounded: <knob>=<v>)` | 以有界几何 knob 覆盖后绿。**@128** = 26 项腿内 baked（triage-128 §1：逐字拷贝自 256 腿 seed2，两 seed 腿内有界实测）；**@256** = 25 格 seed1 有界重跑（triage-256 §5：clamp 下限一次通过、零上调）+ seed2 腿内有界 + `acl_gemm_sve.dim=512` 标定 baked（triage-256 §1，Ruling B）；**@512** = 26 项 baked + `gem5-overrides.txt` 16 条中生效的 15 条（triage-512 §1.1/§5.2；`mrn_rmw_diff_1m` 那条无效，见其边界格） | 对应 triage 分节 |
| `SKIP(legal: F64MM UNDEFINED <VL=256)` | 架构性合法 SKIP，唯一格 = (sve512_fmmla_arm, 128)，两 seed 同格同理 | triage-128 §2 + §3 逐字源码引文 |
| `BOUNDARY(gem5: <标签>)` | gem5 侧限制边界，Ruling I 终局：`PMULL64` = gem5 无该 64-bit PMULL 编码（OpenSSL GHASH `pmull v0.1q`，2 格）；`DC-CIVAC` = SE 模式 `dc civac` 缓存维护路径 raise UndefinedInstruction（3 格）；`SIM-SPEED` = knob 在 clamp 下限或无杠杆，单轮指令量 × ~1400 仿真速超出实用预算（4 格）。native 两腿对应格全绿 | triage-512 §5.1/§5.2 |

- **证据列** = `128s1,128s2 · 256s1,256s2 · rc`。`to` = 该 seed 腿内 timed out（其绿证据 =
  triage-256 §5 有界重跑，同 seed 单测）；`rc` = gem5 进程退出码（512 列权威判定，
  triage-512 §0）。工件映射：`128s1/s2` → `native-128-LCG100{1,2}.summary.txt`，
  `256s1/s2` → `native-256-LCG100{1,2}.summary.txt`，`rc` → `gem5-512-verdicts.txt`
  （四份 summary + verdicts 的行名与清单 190 名逐一同序，生成脚本断言守卫）。
- **种子语义**：native 格 = 任一 seed pass 即 pass；实测两 seed 无分歧绿（128 腿
  189+1 双 seed 一致；256 腿唯 seed1 的 25 个超时格经 §5 重跑补绿，seed2 腿内直绿）。
- **VL 不敏感性注记**：7 个无 tag 的 ipsec 免检件（`ipsec_3des_docsis_aes_cmac_sve_wide` /
  `ipsec_3des_docsis_aes_xcbc_sve_wide` / `ipsec_aes128_cbc_aes_cmac_sve` /
  `ipsec_aes192_cbc_xcbc_96_sve` / `ipsec_aes192_ctr_xcbc_sve` / `ipsec_aes192_gcm_sve` /
  `ipsec_aes192_gcm_sve_wide`；enumeration §2.2：OpenSSL EVP 自行分派、无 SVE 数据面）
  三档 VL 行为完全相同——它们的格只构成"同条件可跑绿"证据，不构成 VL 敏感性证据。

## 3. fmmla@128 合法 SKIP 的逐字源码引文（T3 minor fix）

triage-128 §2 曾意译此注释（丢了 measured-SIGILL 关键句）；按裁定本矩阵补**逐字**引用
——`tests/cpu/arm64/sve512_fmmla_arm.cpp:193-196`（当前基线）：

```c
    /* F64MM needs a 256-bit segment to hold its 2x2 double matrix: at
       VL=128 svmmla_f64 raises SIGILL (measured 2026-10-27 on silicon —
       the instruction is UNDEFINED there, not a no-op). Gate from 256. */
    const long svl = arm64_require_sve_vl(test, 256, 1024);
```

日志侧 skip 理由原文（两 seed 同格同理，raw log `skip-reason:` 行，triage-128 §2 引）：

```
sve512_fmmla_arm: SVE vector length 128 bits is outside the supported 256..1024-bit range
```

交叉证据：同一测试在 256 腿两 seed pass（`native-256-LCG100{1,2}.summary.txt`），
512 腿 rc=0 pass 且 Exec 追踪直证 514 条 `fmmla` 真实执行（triage-512 §5.3）——
门只挡 128，不误伤更高档。

## 4. 矩阵（190 行 × 3 列）

| # | test | 128 | 256 | 512 | 证据 |
|---|---|---|---|---|---|
| 1 | acl_gemm_sve | pass(bounded: dim=512) | pass(bounded: dim=512) | pass(bounded: dim=128) | pass,pass · pass,pass · rc=0 |
| 2 | adcx_adox_interleaved_arm_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 3 | adcx_adox_interleaved_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 4 | adcx_arm_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 5 | adcx_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 6 | adcxlong_arm_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 7 | adcxlong_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 8 | adox_arm_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 9 | adox_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 10 | agu_stress_2src_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 11 | agu_stress_store_only_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 12 | arm0102_fma_f32_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 13 | arm0102_kreg_logic_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 14 | arm0102_kreg_mask_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 15 | arm0102_kreg_not_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 16 | arm0102_kreg_select_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 17 | bigint_mulx_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 18 | eigen_gemm_double_max_arm | pass | pass | pass(bounded: mdim=1024) | pass,pass · pass,pass · rc=0 |
| 19 | eigen_svd_bidiag_sve | pass(bounded: mdim=64) | pass(bounded: mdim=64) | pass(bounded: mdim=64) | pass,pass · to,pass · rc=0 |
| 20 | eigen_svd_cdouble_sve | pass(bounded: mdim=300) | pass(bounded: mdim=300) | BOUNDARY(gem5: SIM-SPEED) | pass,pass · to,pass · rc=124 |
| 21 | fisttp_sve | pass | pass | pass(bounded: num_elements=1024) | pass,pass · pass,pass · rc=0 |
| 22 | fma_patterns_sve_wide_pd | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 23 | fma_patterns_sve_wide_ps | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 24 | fma_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 25 | fma_tail_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 26 | fma_tail_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 27 | fmatail_double_nested_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 28 | fmatail_double_nested_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 29 | fmatail_nested_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 30 | fmatail_nested_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 31 | fmatail_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 32 | fmatail_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 33 | fpu_special_values_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 34 | fsu_byteexact_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 35 | gmp_bigadd_sve | pass(bounded: limbs=256) | pass(bounded: limbs=256) | pass(bounded: limbs=256) | pass,pass · to,pass · rc=0 |
| 36 | gmp_bignum_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 37 | iex_operand_combo_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 38 | insert_extract_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 39 | ipsec_3des_docsis_aes_cmac_sve_wide | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass,pass · to,pass · rc=0 |
| 40 | ipsec_3des_docsis_aes_xcbc_sve_wide | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass,pass · to,pass · rc=0 |
| 41 | ipsec_3des_docsis_hmac_sha1_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 42 | ipsec_3des_docsis_hmac_sha224_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 43 | ipsec_3des_docsis_hmac_sha256_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 44 | ipsec_3des_docsis_hmac_sha384_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 45 | ipsec_3des_docsis_hmac_sha512_sve_wide | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 46 | ipsec_aes128_cbc_aes_cmac_sve | pass | pass | pass(bounded: data_size=1024) | pass,pass · pass,pass · rc=0 |
| 47 | ipsec_aes128_cbc_hmac_sha1_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 48 | ipsec_aes128_cbc_sha2_224_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 49 | ipsec_aes192_cbc_sha2_384_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 50 | ipsec_aes192_cbc_sha2_512_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 51 | ipsec_aes192_cbc_xcbc_96_sve | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass,pass · to,pass · rc=0 |
| 52 | ipsec_aes192_ctr_hmac_sha1_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 53 | ipsec_aes192_ctr_hmac_sha224_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 54 | ipsec_aes192_ctr_hmac_sha256_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 55 | ipsec_aes192_ctr_hmac_sha384_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 56 | ipsec_aes192_ctr_hmac_sha512_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 57 | ipsec_aes192_ctr_xcbc_sve | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass(bounded: data_size=1024) | pass,pass · to,pass · rc=0 |
| 58 | ipsec_aes192_gcm_sve | pass | pass | BOUNDARY(gem5: PMULL64) | pass,pass · pass,pass · rc=134 |
| 59 | ipsec_aes192_gcm_sve_wide | pass | pass | BOUNDARY(gem5: PMULL64) | pass,pass · pass,pass · rc=134 |
| 60 | ipsec_aes256_cbc_hmac_sha1_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 61 | ipsec_aes256_cbc_sha2_224_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 62 | ipsec_aes256_cbc_sha2_256_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 63 | kreg1_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 64 | kreg2_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 65 | kreg3_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 66 | kreg4_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 67 | kreg5_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 68 | kreg6_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 69 | kreg7_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 70 | kreg8_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 71 | kreg9_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 72 | l2c_cross_cache_line_sve | pass | pass | BOUNDARY(gem5: DC-CIVAC) | pass,pass · pass,pass · rc=134 |
| 73 | libomp_ctx_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 74 | lsu_store_forward_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 75 | mem_disambig_alu_sve | pass(bounded: buffer_bytes=4096) | pass(bounded: buffer_bytes=4096) | pass(bounded: buffer_bytes=4096) | pass,pass · to,pass · rc=0 |
| 76 | mesh_upi_sve_asymm_distrib_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 77 | mesh_upi_sve_asymm_distrib_write_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 78 | mesh_upi_sve_asymm_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 79 | mesh_upi_sve_asymm_write_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 80 | mesh_upi_sve_read_L3_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 81 | mesh_upi_sve_read_only_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 82 | mesh_upi_sve_sym | pass(bounded: buffer_bytes=1048576) | pass(bounded: buffer_bytes=1048576) | pass(bounded: buffer_bytes=1048576) | pass,pass · to,pass · rc=0 |
| 83 | mesh_upi_sve_symm_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 84 | mesh_upi_sve_wide_asymm_distrib_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 85 | mesh_upi_sve_wide_asymm_distrib_write_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 86 | mesh_upi_sve_wide_asymm_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 87 | mesh_upi_sve_wide_asymm_read_L3_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 88 | mesh_upi_sve_wide_asymm_read_only_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 89 | mesh_upi_sve_wide_asymm_write_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 90 | mesh_upi_sve_wide_asymm_write_only_int | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 91 | mesh_upi_sve_wide_sym | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 92 | mesh_upi_sve_wide_symm_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 93 | mesh_upi_sve_write_only_int | pass | pass | pass(bounded: buffer_bytes=1048576) | pass,pass · pass,pass · rc=0 |
| 94 | mite_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 95 | mmu_split_tlb_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 96 | movbe_dump_golden_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 97 | movbe_dump_probe_a_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 98 | movbe_dump_probe_b_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 99 | movbe_dump_probe_c_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 100 | movbe_dump_probe_d_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 101 | movbe_dump_probe_e_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 102 | movbe_dump_probe_f_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 103 | movbe_dump_probe_g1_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 104 | movbe_dump_probe_g2_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 105 | movbe_dump_probe_h_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 106 | movbe_dump_probe_x_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 107 | movbe_dump_probe_xn_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 108 | movbe_dump_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 109 | movbe_l1d_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 110 | movbe_l2_sve | pass | pass | BOUNDARY(gem5: DC-CIVAC) | pass,pass · pass,pass · rc=134 |
| 111 | movbe_slf_sweep_sve | pass | pass | BOUNDARY(gem5: SIM-SPEED) | pass,pass · pass,pass · rc=124 |
| 112 | movbe_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 113 | movdq2q_sve | pass | pass | pass(bounded: count=1024) | pass,pass · pass,pass · rc=0 |
| 114 | movmskpspd_sve | pass(bounded: count=1024) | pass(bounded: count=1024) | pass(bounded: count=1024) | pass,pass · to,pass · rc=0 |
| 115 | movq2dq_sve | pass | pass | pass(bounded: count=1024) | pass,pass · pass,pass · rc=0 |
| 116 | mrn_flags_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 117 | mrn_nuke_2src_alu_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 118 | mrn_nuke_3src_alu_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 119 | mrn_nuke_4src_alu_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 120 | mrn_nuke_dense_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 121 | mrn_nuke_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 122 | mrn_pairs_2src_alu_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 123 | mrn_pairs_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 124 | mrn_reloaded_2src_alu_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 125 | mrn_reloaded_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 126 | mrn_rmw_diff_1m_sve | pass | pass | BOUNDARY(gem5: SIM-SPEED) | pass,pass · pass,pass · rc=124 |
| 127 | mrn_rmw_diff_64k_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 128 | mrn_rmw_diff_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 129 | mrn_rmw_dump_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 130 | mrn_rmw_fwd_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 131 | mrn_rmw_l1d_sve | pass | pass | BOUNDARY(gem5: DC-CIVAC) | pass,pass · pass,pass · rc=134 |
| 132 | mrn_rmw_nop16_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 133 | mrn_rmw_nop1_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 134 | mrn_rmw_nop4_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 135 | mrn_rmw_nop64_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 136 | mrn_rmw_nop8_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 137 | mrn_rmw_nop_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 138 | mrn_rmw_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 139 | neon_rot_2src_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 140 | neon_rot_ldr_at_top_ldr4_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 141 | neon_rot_ldr_at_top_mla_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 142 | neon_rot_ldr_at_top_offdiag_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 143 | neon_rot_ldr_at_top_rowmajor_dualcmp_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 144 | neon_rot_ldr_at_top_rowmajor_dump_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 145 | neon_rot_ldr_at_top_rowmajor_gps_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 146 | neon_rot_ldr_at_top_rowmajor_k3res_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 147 | neon_rot_ldr_at_top_rowmajor_k5far_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 148 | neon_rot_ldr_at_top_rowmajor_k5inter_rand_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 149 | neon_rot_ldr_at_top_rowmajor_k7p_hiham_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 150 | neon_rot_ldr_at_top_rowmajor_k7p_one_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 151 | neon_rot_ldr_at_top_rowmajor_k7p_rand_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 152 | neon_rot_ldr_at_top_rowmajor_k7p_zero_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 153 | neon_rot_ldr_at_top_rowmajor_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 154 | neon_rot_ldr_at_top_str3_checkall_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 155 | neon_rot_ldr_at_top_str3_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 156 | neon_rot_ldr_at_top_sub_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 157 | neon_rot_ldr_at_top_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 158 | neon_rot_store_only_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 159 | neon_rot_u8x16_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 160 | ooo_dep_chain_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 161 | operand_space_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 162 | partial_store_forwarding_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 163 | power_virus_dit_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 164 | power_virus_dit_sve_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 165 | sleef_sve | pass | pass | pass(bounded: nelems=1048576) | pass,pass · pass,pass · rc=0 |
| 166 | sve512_f32_chain_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 167 | sve512_f32_chain_svd | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 168 | sve512_f64_chain_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 169 | sve512_f64_chain_svd | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 170 | sve512_f64_special_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 171 | sve512_f64_special_svd | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 172 | sve512_fcmla_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 173 | sve512_fma_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 174 | sve512_fmmla_arm | SKIP(legal: F64MM UNDEFINED <VL=256) | pass | pass | skip,skip · pass,pass · rc=0 |
| 175 | sve512_gather_scatter_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 176 | sve512_gather_scatter_svd | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 177 | sve512_nt_reload_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 178 | sve512_pred_ops_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 179 | sve512_stencil_axis_arm | pass | pass | BOUNDARY(gem5: SIM-SPEED) | pass,pass · pass,pass · rc=124 |
| 180 | sve512_xlswave_ctx_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 181 | sve_fpcr_cartesian_arm | pass | pass | pass | pass,pass · pass,pass · rc=0 |
| 182 | sve_rot_ldr_at_top | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 183 | sve_rot_ldr_at_top_rowmajor | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 184 | sve_rot_ldr_at_top_rowmajor_k3res | pass(bounded: slots=4096) | pass(bounded: slots=4096) | pass(bounded: slots=4096) | pass,pass · to,pass · rc=0 |
| 185 | sve_rot_ldr_at_top_rowmajor_k5inter_rand | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 186 | sve_rot_ldr_at_top_rowmajor_k7p_hiham | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 187 | sve_rot_ldr_at_top_rowmajor_k7p_one | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 188 | sve_rot_ldr_at_top_rowmajor_k7p_rand | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 189 | sve_rot_ldr_at_top_rowmajor_k7p_zero | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass(bounded: slots=1024) | pass,pass · to,pass · rc=0 |
| 190 | swizzle_sve | pass | pass | pass | pass,pass · pass,pass · rc=0 |

## 5. 汇总算术（生成脚本机械统计，与 §4 表体同一数据源）

| 列 | pass（直绿） | pass(bounded) | SKIP(legal) | BOUNDARY(gem5) | 合计 |
|---|---|---|---|---|---|
| 128 | 163 | 26 | 1 | 0 | 190 |
| 256 | 164 | 26 | 0 | 0 | 190 |
| 512 | 142 | 39 | 0 | 9 | 190 |
| **合计** | **469** | **91** | **1** | **9** | **570** |

**570 = 560 pass（469 直绿 + 91 有界）+ 1 SKIP(legal) + 9 BOUNDARY(gem5)**，零 FAIL、零非法 SKIP。

- **128 列 = 189 pass + 1 SKIP(legal)** —— 对账 triage-128 §5（189 直绿 × 2 seed + 1 合法 SKIP：fmmla@128，两 seed 同格同理）；其中 26 格为腿内 baked 有界（triage-128 §1 的 26 项 -O 覆盖的测试）。
- **256 列 = 190 pass** —— 对账 triage-256 §7（seed1 165 腿内直绿 + 25 格 §5 有界重跑 = 190/190；seed2 190 腿内直绿）；26 个有界格 = 25 格 §5 有界重跑（seed1 腿内 to → 同 seed 单测 clamp 下限重跑绿）+ 1 格 baked 直绿（acl dim=512，Ruling B / triage-256 §1）。
- **512 列 = 181 pass + 9 BOUNDARY** —— 对账 triage-512 §6（181 绿 + 9 边界[5 crash + 4 timeout] + 0 fail）；39 个有界格 = baked 26 项中绿 25（eigen_svd_cdouble 转边界格除外）+ gem5-overrides 16 条中绿 15（mrn_rmw_diff_1m 无效仍边界除外）− 两处重叠 1（acl：baked 512 被 extras 覆盖为 128）。
- **有界格计法**：该格绿证据的产生命令里带该测试的几何 knob 覆盖——native 三处命令行头（机械提取）+ gem5-overrides.txt（同键后值覆盖前值）。

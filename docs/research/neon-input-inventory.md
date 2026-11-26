# NEON 类输入数据测试用例清单（九文件夹盘点）

**日期**：2026-11-26 · **扫描机器**：0102（本仓 feat/migrate-opendcdiag-tests 分支）
**范围**：`tests/cpu/` 下九个文件夹 · **路径**：自 `sdcshield/` 仓根的相对地址

## 判据（"符合标准" = 满足其一，以测试代码实际数据面为准）

1. **NEON 向量类型**：`uint8x16_t` / `uint64x2_t` / `uint32x4_t` / `float32x4_t` / `float64x2_t` 等作为输入缓冲与计算类型；
2. **NEON intrinsics**：`vld1q/vst1q/vaddq/…` 加载/存储/加工输入数据；
3. **asm 钉死向量访问**：`ldr/str %q`（`"=w"` 向量约束）、`ldp/stp q`、`ld2 {v…}`、`v0.2d` 等 V 寄存器视图；
4. **共享头 NEON 拷贝**：调用 `neon_memcpy_cache/arm_add_neon_copy.h` 的 `neon_copy()`（vld1q_u8/vst1q_u8 流）。

## 明确排除（源码为证）

| 排除项 | 理由 |
|---|---|
| `misc/loop_check_m.cpp` | 源码含 NEON 路径（`crc_M_ELEM==32` 分支），但**注册档为默认 ELEM=16 标量 LDP/STP X 对**——实际跑标量 |
| `L2_cache/neon_rot_ldr_at_top_rowmajor_scalar64_l2_{128k,256k,512k}.cpp` | asm **X 寄存器标量**底盘（故意标量孪生，8B 元素） |
| `fp64/ma_{atomic,barrier,civac,ldr_random,ldr_stride,nt}_fp64.cpp` | `d` 寄存器 FP64 宽（64-bit）对/独占流，非 128-bit 向量 |
| `arithmetic_arm/` 全部 14 件 | 标量/GMP/标量 asm；`acl_gemm.cpp` 仅经 **ACL 库内部**间接跑 NEON（测试代码喂 ACL Tensor，本身无 NEON 输入） |
| `memory-access/` 8 件标量 asm | `ma_atomic/civac/nt/barrier/copy_stream/str_stream/ldr_random/ldr_stride`（`arm_neon.h` include 为残留） |
| `misc/adaptive_mem_sdc_sve.cpp` | **SVE**——独立扩展，非 NEON（其 NEON 版 `adaptive_mem_sdc.cpp` 在列） |

---

## 总览

| 文件夹 | NEON 类输入（件） | 文件夹总数 |
|---|---|---|
| arithmetic_arm | 0 | 14 |
| memory-access | 5 | 13 |
| neon_memcpy_cache | 26 | 33 |
| L2_cache | 0（2026-11-26 移除，不收） | 54 |
| L3_cache_920 | 25 | 30 |
| fmov | 14 | 14 |
| fp64 | 29 | 40 |
| misc | 62 | 106 |
| vector | 11 | 11 |
| **合计** | **172** | 315 |

---

## arithmetic_arm

无（0/14，全标量/GMP；`acl_gemm` 见排除表）。

## memory-access（5）

```
memory-access/ma_alu_fixed.cpp
memory-access/ma_l2evict_2src.cpp
memory-access/ma_l2evict_plain.cpp
memory-access/ma_l2evict_rev.cpp
memory-access/ma_src_overl2.cpp
```

## neon_memcpy_cache（26）

```
neon_memcpy_cache/barrier_after_lock_store_neon.cpp
neon_memcpy_cache/barrier_after_lock_store_neon_2.cpp
neon_memcpy_cache/cachebounce_neon.cpp
neon_memcpy_cache/cache_stress_clflush_2_neon.cpp
neon_memcpy_cache/cache_stress_clflush_neon.cpp
neon_memcpy_cache/cache_stress_memcpy_2_neon.cpp
neon_memcpy_cache/cache_stress_memcpy_neon.cpp
neon_memcpy_cache/flushed_memcpy_bis_neon.cpp
neon_memcpy_cache/flushed_memcpy_neon.cpp
neon_memcpy_cache/low_load_cache_pressure_neon.cpp
neon_memcpy_cache/memcpy0_neon.cpp
neon_memcpy_cache/memcpy1_neon.cpp
neon_memcpy_cache/memcpy_l1d_size_neon.cpp
neon_memcpy_cache/memcpy_l2_cache_size_neon.cpp
neon_memcpy_cache/memcpy_rewr_neon.cpp
neon_memcpy_cache/memcpy_sem_neon.cpp
neon_memcpy_cache/memcpy_shuffle_neon.cpp
neon_memcpy_cache/memcpy_shuffle_v2_neon.cpp
neon_memcpy_cache/mem_disambig_alu_neon.cpp
neon_memcpy_cache/mem_disambiguation_neon.cpp
neon_memcpy_cache/mfence_neon.cpp
neon_memcpy_cache/mfence_pure_neon.cpp
neon_memcpy_cache/sfence_neon.cpp
neon_memcpy_cache/store_lost_write_detect_neon.cpp
neon_memcpy_cache/store_visibility_cross_class_neon.cpp
neon_memcpy_cache/str_instruction_stress_neon.cpp
```

## L2_cache

**已移除（2026-11-26 用户指示）**：L2_cache 文件夹仍在途未完成（并行会话批次写作中，51 件含 12 件已提交后再改 + 18 件未跟踪新探针），**整族不收入 neon_920 收集包**。原列 51 件（17 探针 × 3 档；例外 `scalar64_l2` 三档 = X 寄存器标量底盘，见"明确排除"表）。该文件夹定稿后如需收录，另行补齐。

## L3_cache_920（25）

```
L3_cache_920/adaptive_l3_sdc.cpp
L3_cache_920/l3_execute_neon.cpp
L3_cache_920/l3_execute_neon_fp64.cpp
L3_cache_920/llc_randomized_load_neon.cpp
L3_cache_920/llc_randomized_load_neon_fp64.cpp
L3_cache_920/llcstress_neon.cpp
L3_cache_920/llcstress_neon_fp64.cpp
L3_cache_920/llc_uniform_load_neon.cpp
L3_cache_920/llc_uniform_load_neon_fp64.cpp
L3_cache_920/memcpy_l3_cache_size_neon.cpp
L3_cache_920/memcpy_l3_cache_size_neon_fp64.cpp
L3_cache_920/mesh_upi_avx2_read_L3_int.cpp
L3_cache_920/mesh_upi_avx2_read_L3_neon_fp64.cpp
L3_cache_920/mesh_upi_avx2_read_only_int.cpp
L3_cache_920/mesh_upi_avx2_write_only_int.cpp
L3_cache_920/mesh_upi_avx2_write_only_neon_fp64.cpp
L3_cache_920/mesh_upi_avx512_asymm_read_L3_int.cpp
L3_cache_920/mesh_upi_avx512_asymm_read_L3_neon_fp64.cpp
L3_cache_920/mesh_upi_avx512_asymm_read_only_int.cpp
L3_cache_920/mesh_upi_avx512_asymm_write_only_int.cpp
L3_cache_920/mesh_upi_avx512_asymm_write_only_neon_fp64.cpp
L3_cache_920/mesh_upi_sse_read_L3_int.cpp
L3_cache_920/mesh_upi_sse_read_only_int.cpp
L3_cache_920/mesh_upi_sse_write_only_int.cpp
L3_cache_920/mesh_upi_sse_write_only_neon_fp64.cpp
```

（注：`mesh_upi_*_int.cpp` 系 x86 SSE/AVX 谱系的 ARM 移植——在 ARM64 上以 NEON 向量数据面实现，故列入。）

## fmov（14）

```
fmov/adaptive_ldp_stp.cpp
fmov/adaptive_ldp_stp_intr.cpp
fmov/adaptive_ldp_stp_intr_l2.cpp
fmov/ldp_stp_cpy_p05.cpp
fmov/ldp_stp_cpy_p05_intr.cpp
fmov/ldp_stp_cpy_p05_intr_l2.cpp
fmov/ldp_stp_cpy_p06.cpp
fmov/ldp_stp_cpy_p06_intr.cpp
fmov/ldp_stp_cpy_p06_intr_l2.cpp
fmov/ldp_stp_cpy_p08.cpp
fmov/ldp_stp_cpy_p08_intr.cpp
fmov/ldp_stp_cpy_p08_intr_l2.cpp
fmov/ldp_stp_cpy_p13.cpp
fmov/ldp_stp_cpy_p13_intr.cpp
```

## fp64（29）

```
fp64/barrier_after_lock_store_neon_2_fp64.cpp
fp64/barrier_after_lock_store_neon_fp64.cpp
fp64/cachebounce_neon_fp64.cpp
fp64/cache_stress_clflush_fp64.cpp
fp64/fp64_2ldr_4ext_stp.cpp
fp64/gather_i32_fp64.cpp
fp64/gatherscatter_i32_fp64.cpp
fp64/lockless_cmpxchg16b_neon_fp64.cpp
fp64/lockless_cmpxchg8b_neon_fp64.cpp
fp64/lockless_cmpxchg_neon_fp64.cpp
fp64/low_load_cache_pressure_neon_fp64.cpp
fp64/ma_alu_fixed_fp64.cpp
fp64/ma_l2evict_2src_fp64.cpp
fp64/ma_l2evict_rev_fp64.cpp
fp64/ma_src_overl2_fp64.cpp
fp64/mem_disambig_alu_neon_fp64.cpp
fp64/sfence_neon_fp64.cpp
fp64/spinlock_rmw_cmpxchg_neon_fp64.cpp
fp64/spinlock_rmw_cmpxchg_pause_neon_fp64.cpp
fp64/spinlock_rmw_xchg_neon_fp64.cpp
fp64/spinlock_rmw_xchg_pause_neon_fp64.cpp
fp64/spinlock_stress_bts_neon_fp64.cpp
fp64/spinlock_stress_cmpxchg16b_neon_fp64.cpp
fp64/spinlock_stress_cmpxchg_even_odd_neon_fp64.cpp
fp64/spinlock_stress_cmpxchg_neon_fp64.cpp
fp64/spinlock_stress_neon_fp64.cpp
fp64/store_lost_write_detect_neon_fp64.cpp
fp64/store_visibility_cross_class_neon_fp64.cpp
fp64/str_instruction_stress_neon_fp64.cpp
```

（例外：`ma_{atomic,barrier,civac,ldr_random,ldr_stride,nt}_fp64` 六件 = d 寄存器 FP64 宽，不列。）

## misc（62）

**A. NEON 向量数据面（51）**

```
misc/adaptive_mem_sdc.cpp
misc/core179_kernel_sdc_repro.cpp
misc/movdq2q.cpp
misc/movmskpspd.cpp
misc/movq2dq.cpp
misc/neon_rot_1src.cpp
misc/neon_rot_fwd_probe.cpp
misc/neon_rot_ldr_rand.cpp
misc/neon_rot_ldr_rand_thread.cpp
misc/neon_rot_ldr_at_top.cpp
misc/neon_rot_ldr_at_top_dump.cpp
misc/neon_rot_ldr_at_top_ldr4.cpp
misc/neon_rot_ldr_at_top_ldr5.cpp
misc/neon_rot_ldr_at_top_lsqfill.cpp
misc/neon_rot_ldr_at_top_lsqfill_v2.cpp
misc/neon_rot_ldr_at_top_mla.cpp
misc/neon_rot_ldr_at_top_offdiag.cpp
misc/neon_rot_ldr_at_top_prefetch.cpp
misc/neon_rot_ldr_at_top_priv.cpp
misc/neon_rot_ldr_at_top_rfport.cpp
misc/neon_rot_ldr_at_top_rfport_v2.cpp
misc/neon_rot_ldr_at_top_robfill.cpp
misc/neon_rot_ldr_at_top_robfill_np.cpp
misc/neon_rot_ldr_at_top_rowmajor.cpp
misc/neon_rot_ldr_at_top_rowmajor_dstr4.cpp
misc/neon_rot_ldr_at_top_rowmajor_dualcmp.cpp
misc/neon_rot_ldr_at_top_rowmajor_dump.cpp
misc/neon_rot_ldr_at_top_rowmajor_fp64_dump.cpp
misc/neon_rot_ldr_at_top_rowmajor_fp64_dup2.cpp
misc/neon_rot_ldr_at_top_rowmajor_fp64_dup3.cpp
misc/neon_rot_ldr_at_top_rowmajor_gps.cpp
misc/neon_rot_ldr_at_top_rowmajor_k2pureload.cpp
misc/neon_rot_ldr_at_top_rowmajor_k3res.cpp
misc/neon_rot_ldr_at_top_rowmajor_k4bgap.cpp
misc/neon_rot_ldr_at_top_rowmajor_k4gap.cpp
misc/neon_rot_ldr_at_top_rowmajor_k5far.cpp
misc/neon_rot_ldr_at_top_rowmajor_k5inter_rand.cpp
misc/neon_rot_ldr_at_top_rowmajor_k7pattern.cpp
misc/neon_rot_ldr_at_top_rowmajor_str3_checkall.cpp
misc/neon_rot_ldr_at_top_rowmajor_strfwd.cpp
misc/neon_rot_ldr_at_top_rowmajor_waw2str.cpp
misc/neon_rot_ldr_at_top_str3.cpp
misc/neon_rot_ldr_at_top_str4.cpp
misc/neon_rot_ldr_at_top_str4_checkall.cpp
misc/neon_rot_ldr_at_top_sub.cpp
misc/neon_rot_ldr_at_top_tlbstress.cpp
misc/neon_rot_nop_gap.cpp
misc/neon_rot_stale_reload.cpp
misc/neon_rot_store_only.cpp
misc/neon_rot_u32x4.cpp
misc/neon_rot_u8x16.cpp
```

（例外：`neon_rot_ldr_at_top_rowmajor_scalar64.cpp` = 故意标量孪生，不列。）

**结构标注——"多源 ldr + 负载 + 多源 str + 多源读回"四段全齐件**：

- `misc/neon_rot_ldr_at_top_rowmajor_str3_checkall.cpp`——每元素 3 条 ldr（scratch 死加载 + 双源 srcA/srcB）→ 轮换 ALU（add/eor/and/orr 按 i%4）→ 同一 res 三连 str（temp/junk/dst，dst 永远最后）→ sweep 末**三路 memcmp 全查**（temp/junk/dst 各对金身）+ 失配冷路径重读 srcA/srcB；slot_mask 分诊：`0b111`=上游读/ALU 错，`1<<k`=第 k 条 store 自身写路径坏。0102 实测父本 str3 曾 51+49 次 fail 全指 179 线程；
- 同形邻居：`misc/neon_rot_ldr_at_top_str4_checkall.cpp`（四连写版，四路全查）；L2 驻留版：`L2_cache/neon_rot_ldr_at_top_rowmajor_str3_checkall_l2_{128k,256k,512k}.cpp`；
- 口径注意：其"读回"段为 memcmp（libc）全查 + 失配才显式冷 ldr——热循环内显式 asm 向量读回（每元素即时比对）目前只有 strfwd/dump 系。

**B. V/FP 寄存器类（fmov_train 系，11）**——asm `fmov d/v` 走 V 寄存器文件（含 `v*.2d` 128-bit 视图），payload 为整数位模式；按 ASIMD 寄存器轴列入，若你的口径只认向量数据面可剔除此组：

```
misc/fmov_train_3ldp_stp.cpp
misc/fmov_train_3ldr_str.cpp
misc/fmov_train_droop.cpp
misc/fmov_train_gdroop_ldp_stp.cpp
misc/fmov_train_gdroop_ldp_stp_intr.cpp
misc/fmov_train_gdroop_ldr_str.cpp
misc/fmov_train_gdroop_ldr_str_intr.cpp
misc/fmov_train_ldp_stp.cpp
misc/fmov_train_ldp_stp_l2l3.cpp
misc/fmov_train_ldr_str.cpp
misc/fmov_train_ldr_str_l2l3.cpp
```

## vector（11）

```
vector/insert_extract.cpp
vector/kreg1.cpp
vector/kreg2.cpp
vector/kreg3.cpp
vector/kreg4.cpp
vector/kreg5.cpp
vector/kreg6.cpp
vector/kreg7.cpp
vector/kreg8.cpp
vector/kreg9.cpp
vector/swizzle.cpp
```

---

## 口径备注

- 判据以**测试代码实际数据面**为准，不认残留 include：`memory-access` 8 件、`loop_check_m` 含 `arm_neon.h`/NEON 源码路径但实际标量，全部排除并记录理由；
- `neon_copy`（共享头 `arm_add_neon_copy.h`）视同 NEON 数据面——`neon_memcpy_cache` 与 `L3_cache_920` 的 `*_neon` 双胞胎由此列入；
- 修正记录：初扫 `\bv[a-z]+_` 判据漏掉 `vld1q/vst1q` 等含数字词干 intrinsic，曾漏 `flushed_memcpy_neon`/`movmskpspd` 等，本清单为修正后口径；
- 本清单未提交入库（工作文件）；分支上的 `L2_cache` 54 件为并行会话产物，截止本清单时刻全部已在位。

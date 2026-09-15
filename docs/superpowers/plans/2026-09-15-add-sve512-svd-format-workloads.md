# 集成 sve512 SVD-format 家族（4 个测试）到 arm64 测试分类

日期：2026-09-15

## 背景

用户将前两轮集成的 sve512_fma 家族（workload 1-4）与 eigen_svd_cdouble_sve
做了逐维度对比，结论：四个 workload 工作集最大 64 KB，不覆盖 SVD 的独有
触发路径。本轮 4 个 SVD-format 变体补第一条路径——SVD 尺度工作集 + 16x16
嵌套块遍历（对应 Eigen BDCSVD 的块访问模式）：

- `sve512_f64_chain_svd.cpp` — f64 FMLA 链的 SVD-format 变体
- `sve512_f64_special_svd.cpp` — f64 特殊值链的 SVD-format 变体
- `sve512_f32_chain_svd.cpp` — f32 FMLA 链的 SVD-format 变体
- `sve512_gather_scatter_svd.cpp` — gather/scatter 的 SVD-format 变体
  （2-D 块内置换索引表）

代码原样集成，零修改。多线程共享写与分治递归两条 SVD 独有路径本轮仍未
覆盖（框架本身每 CPU 一线程并发跑 test_run，但这四个测试写线程私有数据）。

## 集成前核查发现（诚实记录；按用户指示代码原样集成，待用户决策）

1. **sve512_gather_scatter_svd init 存在数组越界（严重）**：
   SVD_ROWS/SVD_COLS = 300 不是 16 的整数倍（300 = 18*16 + 12）。br=288
   的尾块按完整 16x16 处理：k 最大 255，i/16 最大 15 → row_i 最大
   288+15 = 303 ≥ 300，col_i 同理最大 303；
   `gather_idx[row_i*300+col_i]` 最大 303*300+303 = 91203，超出 vector
   大小 90000 → `operator[]` 越界（UB，堆越界读写）。该代码仅在 SVE
   机器上执行（本机框架门 + HWCAP 双重 skip），但目标机上 init 阶段就会
   堆损坏/崩溃，且堆损坏可能伪装成 SDC 假信号。其余三个测试只用块原点
   (br,bc) 作基址，已逐一核算 base+lane 上界 < total，无越界。
2. **docblock 内存注释与实际分配不符**：chain 类流大小 =
   ROWS*COLS*STEPS*vl_d。f64_chain_svd 与 f32_chain_svd 各 ~2.9 GB/流
   × 2 流 ≈ 5.9 GB 分配（注释写 1.44 MB）；f64_special_svd ~368 MB/流
   × 2 ≈ 737 MB；gather_scatter_svd ~2.9 MB（与注释同量级）。init 的
   try/catch 使 bad_alloc 干净 skip，但目标机需足够 RAM，且 init 填充
   5.9 GB 需数秒。
   附（步进分析）：run 每轮实际触碰 = 361 块原点 × 512 步 × 64 B ×
   2 流 ≈ 23.7 MB —— 仍远超 L2（512 KB）、构成 L3/TLB 压力（步间距
   5.76 MB），但只是分配量的 ~0.4%。
3. **gather_scatter 系（含已合并的 workload 4）多线程竞态**：
   scatter_dst/scatter_golden 为跨线程共享写，且每线程迭代末
   std::fill(0) 清零 scatter_dst —— 线程 A 比对期间线程 B 清零会产生
   假阳性 SDC 报告。chain 类测试写线程私有栈向量，无此问题。
   建议 SVE 机器上以 -n 1 运行，或将 dst/golden 改为每 CPU 线程局部。

## One-patch-per-unit 分解（4 commit，逐个验证→提交→推送）

### Task 1: sve512_f64_chain_svd — 完成（2026-09-15 实测）
- [x] 写入用户代码（原样）；meson 注册（带注释）
- [x] reconfigure + ninja：322/322 成功；单 TU 重编译 0 warning
- [x] `--list-tests` 注册（第 275 个）；计数实测 default 275
- [x] `-e sve512_f64_chain_svd -t 1000`：result skip / CpuNotSupported /
      `test compiled with sve`，总体 exit: pass，无 SIGILL
- [x] 回归：`-e zstd19 -t 3000` exit: pass
- [x] objdump 实测：`fmla z0.d, p0/m, z1.d, z2.d`、`fmadd d0`、`cntd`、
      `ptrue p0.b`
- [x] README：专项表补该测试 + 计数 275/284；commit + push

### Task 2: sve512_f64_special_svd — 完成（2026-09-15 实测）
- [x] 写入用户代码（原样）；meson 注册（带注释）
- [x] ninja 323/323；单 TU 重编译 0 warning
- [x] `--list-tests` 注册（第 276 个）；计数实测 default 276
- [x] `-e sve512_f64_special_svd -t 1000`：干净 skip（CpuNotSupported /
      `test compiled with sve`），exit: pass，无 SIGILL
- [x] 回归：`-e zstd19 -t 3000` exit: pass
- [x] objdump 实测：`fmla z0.d, p0/m, z1.d, z2.d`、`fmadd d0`、`cntd`、
      `ptrue p0.b`
- [x] README 计数 276/285 + 专项表；commit + push

### Task 3: sve512_f32_chain_svd — 完成（2026-09-15 实测）
- [x] 写入用户代码（原样）；meson 注册（带注释）
- [x] ninja 324/324；单 TU 重编译 0 warning
- [x] `--list-tests` 注册（第 277 个）；计数实测 default 277
- [x] `-e sve512_f32_chain_svd -t 1000`：干净 skip（CpuNotSupported /
      `test compiled with sve`），exit: pass，无 SIGILL
- [x] 回归：`-e zstd19 -t 3000` exit: pass
- [x] objdump 实测（f32 通路）：`fmla z0.s, p0/m, z1.s, z2.s`、
      `fmadd s0`、`cntw`、`ptrue p0.b`
- [x] README 计数 277/286 + 专项表；commit + push

### Task 4: sve512_gather_scatter_svd
- [ ] 同上；预期 default 278 / beta 282 / skip 287；gather/scatter
      证据（ld1d/st1d 间接索引）；README 计数 278/287

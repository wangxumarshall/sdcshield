# 跨 OS 基准采集设计说明

脚本本体见 [`benchmark.sh`](benchmark.sh)（`bash benchmark.sh <bin> <outdir>`），本文说明口径与依据。

## 为什么用固定 loop 数而不是固定墙钟

GHA 的 `ubuntu-24.04-arm` 是**共享 aarch64 云实例**（AWS Graviton 系），不是用户的 kunpeng920 实机。
若用固定墙钟（`-t 30s`），同一测试在不同 OS 上会因 CPU 频率/邻居负载差异跑到**不同迭代数**，
跨 OS 不可比。改用 `--max-test-loop-count N` 让每个 OS 上每个测试跑**恰好 N 次 main loop**，
工作量一致，只比"完成同样工作量的墙钟"（YAML `test-runtime`，秒）。

## 口径声明（诚实边界）

- **相对近似值**：报告的是同架构、同工作量下每个测试的墙钟，作为跨 openEuler 版本
  （gcc-7/10/12、glibc 2.28/2.34/2.38、内核差异）的性能**相对**参考。
- **非绝对对比**：不与 x86、也不与用户本地 kunpeng920 实机做绝对对比（宿主不同，结果不可复现）。
- **固定环境**：所有 OS 跑在同一类 runner 上，已隔离"机器差异"这一最大混淆因子。

## 基准测试集（三系列交集，2026-09-17 探针验证 20.03/22.03/24.03 皆有）

| test | 检测域 | loop 数 |
|---|---|---|
| openblas_dgemm | NEON FMA GEMM（double） | 2 |
| openblas_sgemm | NEON FMA GEMM（float） | 2 |
| zstd | 压缩-解压（默认档） | 1 |
| zlib | 压缩-解压（zlib 默认档） | 1 |
| fma | FMA 算术 | 2 |
| crc32 | CRC32 指令 | 2 |
| pocketfft_fft | 复数 FFT | 2 |
| memcpy_l2_cache_size | L2 带宽 memcpy | 2 |
| eigen_gemm_double_dynamic_square | Eigen GEMM | 2 |
| gmp_bignum | 大整数 | 2 |
| openssl_sha | SHA-256/384/512 | 2 |

> 选**默认压缩档** `zstd`(~80s/loop)/`zlib`(~177s/loop)而非最高档 `zstd19`(700s+)/`zlib1`(168s)，
> 保证基准时长可控；单 loop 值 2026-09-17 本地 kunpeng920 实机量取，GHA 云实例会更快。

## 输出

`<outdir>/benchmark.tsv`：`test<TAB>wall_seconds<TAB>loop_count`。
此基准 tsv 仍上传进 artifact，但不再进最终 report summary（最终 summary 由
`report-summary.py` 生成「用例 × 版本」结果矩阵，见 `report-summary.py` 注释）。
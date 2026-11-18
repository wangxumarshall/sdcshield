# Eigen 测试实现内幕（自适应规模 + SVE dispatch）

> **Provenance**：本文档自 CLAUDE.md「Known platform quirks」的 eigen 条目与 ARM64 spec policy 条目、
> README.md「Eigen SVD」段尾抽离（2026-11-18，源 commit `727cc8d3`），内容逐字搬运。
> CLAUDE.md/README 主文保留操作规则（修复状态、显式 `-O` 旁路、spec 门控机制）并链接至此。

## 自适应规模联合求解（sdc_joint_dim_slice）

（源：CLAUDE.md eigen 条目。）

Adaptive dims are jointly sized (2026-10-27, feat/eigen-p0-p3-fix — also cured the full-core auto timeouts `eigen_svd_cdouble_sve` 321s / `eigen_svd_bidiag_sve` 343s; the latter case — and its `SDC_RSS_BIDIAG` table entry — was retired 2026-11-02, historical note only): `sdc_joint_dim_slice` = min(n_mem, n_run, clamp) — n_mem from a per-NUMA-node four-source budget (phys×‰ / MemAvailable×0.9 / cgroup v1+v2 / CommitLimit−Committed_AS×0.9 when vm.overcommit_memory=2 — no-op otherwise, issue #310) against measured per-family RSS coefficients (RSS_RATIO_TABLE), n_run from a probe power-law fit × the AR all-core-slowdown table (family×topology), refined by a VmHWM feedback loop (per-slice EMA, median aggregation, 0.7× table floor). Explicit `-O <test>.mdim/n` bypasses sizing; the explicit-knob `[内存越界]`/`[欠压力]` expected-memory warning (the T11 block sits inside the aarch64+linux guard — x86-64 prints none for explicit knobs; the tags themselves also fire on x86-64 from the arch-agnostic `sdc_auto_square_dim`/`sdc_auto_linear_count` adaptive paths) fires only for the ten SVD template-family users (knob routed through the joint layer via `calibrated_auto_dim`) — gemm/sparse/sve callers still short-circuit the knob in their own init (before the arch guard), no warning yet (T11b follow-up; 10/47 eigen cases covered — 32 at the 2026-11-03 unification docs count + `eigen_gemm_double_ab_split` (migration #472 content, in the same-day unification merge; no -O mdim knob, dimension from the raw `GEMM_K_DIM` env) + 14 `_sve2` companions (2026-11-13, inits short-circuiting the knob like their `_sve` templates) — the covered ten unchanged; the fourth bidiag caller was retired with its case 2026-11-02).

## SVE dispatch 框架（14+14）

（源：CLAUDE.md spec policy 条；march 切换与 objdump A/B 审计链见 [evidence/sve2-companion-objdump-audit.md](../evidence/sve2-companion-objdump-audit.md)。）

The 14 Eigen SVE cases (2026-11-03, feat/eigen-sve-unification) are all tier-locked by the language (fixed-width packets) and built once per tier with a runtime function-pointer dispatch, unified on the per-tier body + dispatcher framework in `tests/cpu/eigen_svd/sandstone_eigen_sve_dispatch.h` (2 pre-existing: `eigen_svd_cdouble_sve`/`eigen_gemm_double_max_arm`; 12 new: BDCSVD×4, JacobiSVD×4 — SKIP-level, GEMM×4; `eigen_svd_bidiag_sve` retired 2026-11-02, its purpose eliminated by #410). Each of the 14 has an `_sve2` companion (2026-11-13, feat/eigen-sve2-companion, 14+14): the same per-tier bodies are dual-compiled per tier — `armv8.2-a+sve` (Eigen ns `EigenSVE<t>`) vs `armv8.2-a+sve2` (Eigen ns `EigenSVE2<t>`) — and each `<pfx>_sve2` dispatcher is the `_sve` template plus exactly six differences (id/env `_sve2`, pfx at every call site, an `arm64_cpu_has_sve2()` honest-SKIP gate before `arm64_require_sve_vl`, `ARM64_REQUIRES_SVE2`, the "SVE2 packet backend … counterpart" description string, header counterpart + honest-scope note). (Exception, disclosed in the commit: where the `_sve` template's header carried stale build-mechanics claims that would contradict the `_sve2` build, the header is factually corrected — the two known cases: `eigen_svd_cdouble_sve2` ×3, `eigen_gemm_double_max_arm_sve2` ×1.)

## #410 ptranspose scatter 修复（复数 SVE 1024 档启用）

（源：README「Eigen SVD」段尾；CLAUDE.md spec policy 条。）

复数 SVD 的 1024 档自 2026-11-01（issue #410）启用：vendored Eigen 的 SVE 复数 `ptranspose` 原实现（memory-bridge）在 NC>N 时越界读，已改写为 scatter 形状（任意 N/NC 界内、方阵形状与原置换位级相同）；gem5-fi 四档（128/256/512/1024）同源双 kernel 实测 `SDCSHIELD_FAILS=0`，512 档 checksum 与修复前逐字节一致（位级非回归），1024 档为首次真实执行（证据链 `docs/superpowers/inventory/issue410-eigen-complex-1024/`，该目录已随 31f6a99e 清理出库，git 历史 `31f6a99e^` 可取回）。

Every payload runs all four tiers since the vendored-Eigen ptranspose scatter rewrite (issue #410, 2026-11-01: the memory-bridge form read past `buf[N][NC]` when NC > N — 1024-bit NC=8; the scatter form is in bounds for any N/NC and bit-identical in square shapes, gem5-verified at 128/256/512/1024 with the 512 checksum reproducing the pre-fix value).

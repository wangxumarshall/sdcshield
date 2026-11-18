# Eigen `_sve2` 配套——objdump A/B 审计与 WHILEWR 诚实边界

> **Provenance**：本文档自 README.md「用例质量分级」footnote 尾段与 CLAUDE.md「ARM64 spec policy」
> 条目抽离（2026-11-18，源 commit `727cc8d3`），内容逐字搬运。README/CLAUDE.md 主文保留一行结论并链接至此。
> 背景批次：feat/eigen-sve2-companion（2026-11-13）——14 个 Eigen SVE 用例各增一个 `_sve2` 配套（14+14）。

## 诚实边界（WHILEWR，2026-11-17 修正）

（源：README「用例质量分级」footnote。）

诚实边界（2026-11-17 修正）：配套用例压力内核 = SVE1 子集在 SVE2 硬件向后兼容执行；但 GCC 在 +sve2 下把 Eigen 拷贝管线（call_assignment/gemm_pack_rhs 别名检查环路）惯用化为 WHILEWR——SVE2-only 地址重叠测试指令（主仓 40 个 `_sve2` 目标库共 1549 实例分布于其中 35 库；旧「零 SVE2-only」审计清单遗漏此类，binutils SVE1-only 汇编门实测修正；系拷贝管线而非压力内核，#661 时代即已存在，armv8.2-a+sve2 降级后逐库计数不变；用例 HWCAP2_SVE2 门控，仅 SVE2 硬件执行，运行面无影响），功能验证 = gem5 SE 配对矩阵 10 body 族 × 8 腿（同档 sve2↔sve checksum 逐字节一致）+ FS 真二进制腿（dispatcher 路径：`eigen_svd_double_sve2 -n 2`，VL=128，pass）。

（源：CLAUDE.md spec policy 条，英文原文。）

Honesty boundary (corrected 2026-11-17): the stress kernels are an SVE1 subset, but GCC idiom-recognizes Eigen's aliasing-checked copy plumbing (call_assignment/gemm_pack_rhs loops) into WHILEWR — an SVE2-only address-overlap instruction, 1549 instances in 35/40 libraries, present since #661 (armv9-a+sve2) and per-library IDENTICAL after the march switch (the earlier "emits ZERO SVE2-only instructions, objdump-audited 40/40" claim missed this class; caught by the binutils SVE1-only assembler gate). The companions still provide an SVE2-gated entry point, NOT SVE2-only datapath coverage, and runtime is HWCAP2_SVE2-gated so WHILEWR only ever executes on SVE2 hardware. gem5 evidence: SE pairing matrix 10 body families × 8 legs, per-tier sve2↔sve checksums byte-identical, plus an FS real-binary leg validating the dispatcher path (`eigen_svd_double_sve2 -n 2`, VL=128, pass).

## armv9-a → armv8.2-a march 切换与 objdump A/B 审计

（源：CLAUDE.md spec policy 条。）

2026-11-17 前曾用 armv9-a+sve2——armv9-a 架构名需 GCC 11+，曾致 Multi-OS Verify 的 20.03/22.03 gcc-10 镜像 Build 全红（PR #661 回归，run 44011713221）；+sve2 在 armv8.x-a base 上 GCC 10 即可编；双 march 40 库 objdump A/B 实测（2026-11-17 干净同源全量构建对比，flags 除 march 外逐项全等）：40/40 库原始反汇编有差异（分支偏移/地址移位 + 标量调度重排，合计 ~10.2k 行），32/40 库助记符多重集逐库全等，余 8/40 库另有 ±1..13 次纯标量计数微差（mov/ldr/str/stp/ldp/ldur/stur/nop/add/lsl/madd/mul/b.*，含一次整数 mul↔madd 融合翻转；零 SVE 族、零 FP 族计数变化 → 数值面不变），WHILEWR 逐库计数 A/B 全等（合计 1549 实例分布于 35 库），诚实边界不变。

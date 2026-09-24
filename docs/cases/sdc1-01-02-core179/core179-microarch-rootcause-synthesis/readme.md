# Core 179 微架构级根因与故障注入研究索引

## 阅读顺序
1. PLAN.md：五转储交叉根因定位计划（systematic-debugging 驱动）
2. DIAGNOSIS_REPORT.md：主诊断报告（五转储法证、D1/D2 通路、根因陈述、处置建议）
3. MICROARCH_SUPPLEMENT.md：微架构深化（结合 TSV110 几何、D1/D2/D3 三通路、审稿人压力测试）
4. FI_DESIGN_SUPPLEMENT.md：故障注入方案设计（H5–H7、P-D1/D2/D3、诚实执行状态）
5. PAPER.md：顶会论文草稿（ASPLOS/MICRO/HPCA 级，全文）
6. POSITIONAL_PARITY_RESEARCH.md：位置锚定校验前沿探索（三启示论证 + CHAOSPosParity 原型 + 理论开销）
7. posparity_paper_zh.md / posparity_paper_en.md：位置锚定校验论文（中/英，由研究报告重构：三启示论证 + 检出矩阵 + 逐层代价评估）

## 验证状态（诚实）
- ✅ H5 已 gem5 端到端验证闭环（byte_lane_skew → oops 链复现，多 seed）
- ⚠️ H6/H7 的 SE-mode null 根因已源码静态确证（`mmu.cc:1213` SCTLR.M=0→`translateMmuOff` 绕过页表走查器，D2/D3 钩子不触发）；FS 模式下 D2/D3 钩子触发已实证（`o3_chaos_fs.py`：D2 `numAddrFaults=20`、D3 `numFaultsInjected=7963 numSpuriousFaults=7727`，直接证伪 SE 的 0）。另发现并修复 rng-init-order bug（`rng_seed=0` 必崩，patch bc4feb4）。H6/H7 定量谱可分结论仍需 FS 长跑，未完成。
- ✅ P-D1/D2/D3 三注入器已实现、编译进 gem5.opt（`nm` 372 个匹配符号，`.o` 全就绪）
- ✅ CHAOSPosParity 原型：golden 零假阳性；bit_flip 1064/1064、all_zero 695/695、D1 指针链 367/367（100%）、skew 总体 434/452=96.0%；panic 模式 fail-fast 实证（run_posparity.sh，显式非零 seed）
- ✅ 位置锚定校验论文双稿：数字与 POSITIONAL_PARITY_RESEARCH.md 全量对账；三启示逐层代价评估（可用性/硅预算/时间与产线经济）为论文新增分析，新算术假设显式

## 复现
- 构建：`cd CHAOS/gem5 && taskset -c <healthy cpus> scons build/ARM/gem5.opt -j8`
- H5 复现：见 `fi_research/probes/o3_chaos_smoke.py` + `ptrskew_kernel.c`
- vmcore 法证：见 DIAGNOSIS_REPORT §7 命令索引

## 诚实边界
- 本机即 core179 故障机；编译/运行全程 taskset 隔离 cpu179
- 单/多缺陷裁决需供应商 RTL/DFT（软件不可解）
- 链接阶段曾出现 SDC-affected 瞬态失败（最终 -j1 成功）

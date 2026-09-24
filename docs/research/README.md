# SDCShield 研究记录（持续更新）

> **用途**：本目录记录我们（AI + 用户）对 SDCShield 的研究内容、进度和关键结论。
> **动机**：机器可能随时关机 / 网络不稳定导致对话中断，所有关键内容必须及时落盘到仓库，
> 新会话可凭此文件恢复上下文。
> **纪律**：所有结论都基于实际运行过的命令或读过的源码（实证，不臆测）。

## 文档索引

| 文档 | 内容 |
|---|---|
| [usage.md](usage.md) | 如何使用 sdcshield：构建、全部 CLI 选项（含 `--help` 未列出的）、运行模式、日志 |
| [adding-tests.md](adding-tests.md) | 如何给 sdcshield 添加新的测试用例：完整流程 + 模板 + 本仓库特有规则 |
| [progress-log.md](progress-log.md) | 研究进度时间线（每个会话做了什么、下一步计划） |

## 项目一句话总结

SDCShield = CPU/系统静默数据损坏（SDC）检测工具，fork 自 Intel OpenDCDiag，移植到 ARM64
（鲲鹏 920 / 通用 ARMv8.1+），x86-64 保持为不动参考架构。测试以 fork 子进程 + 每核一线程
模型压测计算单元（FMA/SIMD/压缩/SVD/加密），与 golden 值逐字节 memcmp 比对——专门抓
"不崩溃但出错位" 的静默故障。

## 快速事实卡（2026-09-18 实测）

- 二进制：`./builddir/sdcshield`（当前版本 `sdcshield-06f0ef541a61`，main @ 06f0ef54）
- 测试数：PROD(默认) **289**；`--quality=0`(含BETA) **293**；`--quality=-1`(含SKIP) **298**
- 分组：`@compression` `@ipsec` `@math`
- selftests：142 个（`--selftests --list-tests`）
- 回归验证基线：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`（2026-09-18 实测 pass）
- 当前构建 vendored 依赖齐备（openssl/openblas/sleef/isa-l 均已 build）

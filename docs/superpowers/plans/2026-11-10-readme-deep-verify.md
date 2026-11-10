# 2026-11-10 README 深度核对与优化（LSP + 源码 + 运行时实证）

## 目标

用户指令：调用 LSP、结合源码，深度优化 README.md。执行口径：**不改结构、
不做无害润色**——只修「与源码/运行时不符」的事实，并为已知未决矛盾补
如实标注（repo 规则：文档 100% 准确 + 诚实）。

## 方法与证据链

- **LSP（clangd）**：根 `compile_commands.json` → `builddir/` 符号链接生效。
  `documentSymbol(framework/sandstone.h)` 实证 `arm64_requires`(:433)、
  `arm64_min_vl_bits`(:437)、`log_platform_message`(:507)、`memcmp_or_fail`(:747)；
  `workspaceSymbol` 实证 `arm64_spec_get`（`framework/device/cpu/arm64/arm64_vl.h:289`）、
  `arm64_require_sve_vl`（同文件 :391）——README 引用的框架符号全部实存。
- **运行时**：本机（cortex x3b / 0xd02，SVE1 VL=256，无 SME）以 PR #518 树
  构建的二进制实测 `--list-tests` 三档 565/675/683（与 README 2026-11-03
  记录一致），并在 `--quality=-1` 全量名单上逐族 grep 计数。
- **源码**：`tests/cpu/openblas_gemm/dgemm.cpp`（分配结构）、
  `scripts/run/sdc_detect.sh`（分桶正则）、ipsec 双目录声明形态。
- **git 考古 + progress-log**：计数矛盾来龙去脉。

## 核对矩阵（主项）

| README 断言 | 证据 | 结论 |
|---|---|---|
| SVE1 host 565/675/683（L348） | 今日实测同值 | ✓ |
| 分级表 8/110/569/687（L387-394） | 683=687−4（SME/SVE2-only PROD）；565+110+8=683 | ✓ |
| mesh_upi 46 含 18 SVE（L362） | 运行时 grep 46/18 | ✓ |
| lock 族（31）（L363） | 运行时 grep **47** = 31 x86 基础 + 14 `_neon` + 2 `spinlock_stale_read{,_xchg}` | ✗ **修** |
| PROD 源码全量上界「现值 499」（L126） | 分级表机判 569；今日 565+4=569 | ✗ **修** |
| cn23154 320 用例（L182） | progress-log #10 已挂「同日 320 vs 322 矛盾、无复现口径」 | ✗ **补如实标注** |
| ipsec 70（46 NEON + 24 SVE）（L258/L373） | 源文本 DECLARE 140=条件编译 2×；`tests/cpu/sve/ipsec` 24 文件 | ✓（本机构建无 SSL 故运行时为 0，源码口径成立） |
| 806MB 工作集（L455）与预算表 3×核数（L287） | dgemm.cpp:26「6 matrices = 3 shared(a/b/golden) + 3 per-thread(a_copy/b_copy/c)」；6×128MiB=806MB；共享 3 个在 128 核下占 0.8% | ✓ |
| openblas/sleef/openssl/isa-l/eigen 版本（L258-262, L540） | build.sh/tarball/eigen3.pc: OpenSSL 3.5.0、OpenBLAS 0.3.29、SLEEF 3.9.0、isa-l 2.32.1、Eigen 5.0.1 | ✓ |
| cron 时刻（L223/L227） | multi-os-verify.yml `0 4 * * *`、sanitizer.yaml `43 18 * * *` | ✓ |
| SDC_BIN 发现序 / SMOKE_TIMEOUT_S=2400 / 29 选项矩阵 | sdc_detect.sh:43、excite.sh:87、lts-stability README | ✓ |
| P1 分桶正则（L50） | sdc_detect.sh:72 逐项吻合 | ✓ |
| 17 个文档/脚本链接 | ls 全部实存 | ✓ |
| GEMM 足迹/预算表算术 | mdim²×宽度×(3\|6) 逐格复算 | ✓ |
| tests/cpu/sve「171 个」（L382） | 目录级 DECLARE 195/meson 151，含条件编译无法廉价复验；2026-11-03 merge 机判口径 | 不动（无反证） |
| 预构建 221 PROD（L126） | 历史冻结口径（2026-09-12 二进制） | 不动 |

## 修复（本 commit）

1. **L126**：「现值 499」→「现值 569（2026-11-03 机判）」——499 为迁移
   merge 前旧值，与同文分级表自相矛盾。
2. **L182**：cn23154「默认 quality 320 用例」补注「同日『用例质量分级』
   脚注的复现分支口径为 322，两口径未对账（progress-log #10 跟踪）」。
3. **L363**：锁/原子族「（31）」→「（47 = 31 x86 命名基础 + 14 `_neon`
   变体 + 2 `spinlock_stale_read{,_xchg}`）」——与同表 mesh 行「46 含
   SVE 变体」统一为运行时全量口径。

## 验证边界

- 计数证据全部来自今日实际运行输出与源码行号，非推算。
- docs/ 改动不经构建系统；无 tab；codespell 按 `.codespellrc` 政策跳过
  docs/。
- 320/322 无法在本机裁决（cn23154 机器 + 两口径均无复现命令），故只做
  如实标注、不改数——与 progress-log #10 的处置一致。

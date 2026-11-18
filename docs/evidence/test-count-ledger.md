# 用例计数台账（test-count-ledger）

> **Provenance**：本文档自 README.md「测试用例与检测能力」「用例质量分级」footnote 与 CLAUDE.md
> Build & run 注释块抽离（2026-11-18，源 commit `727cc8d3`），内容逐字搬运。
> README/CLAUDE.md 主文保留一行结论并链接至此。计数口径：`--quality` 分级过滤与芯片 spec
> 选择期过滤后的实际注册用例数（host-dependent）。

## SVE1 主机（0xd02）

SVE1 主机实测 565 PROD（2026-11-03 merge 复核；`--quality=0` 675、`--quality=-1` 683；= main `#472` 迁移/增量 + feat/eigen-sve-unification +12 SVE eigen − bidiag）。

## 无 SVE 鲲鹏 920

无 SVE 鲲鹏 920 **主机 2026-11-17 实测 458 PROD（`--quality=0` 566、`--quality=-1` 570，含 111 迁移用例（`arm0102_*` 基础 10 项 2026-11-03 退役）+ 7 arm_add 新探针 + 14 个 _neon 变体 + 9 个 633 cache 族新测试的口径，vendored 5 库 install/ 全在位——依赖缺位会相应减对应测试组；eigen SVE 分支 12 新 + 退役 bidiag 与 SVE2 配套 14 全部 `ARM64_REQUIRES_SVE/SVE2` 门控，无 SVE 主机净变化 0；完整时序链见「用例质量分级」footnote）**（该 footnote 的完整时序链已并入下方「源码全量口径与 merge 审计」小节）。

## 源码全量口径与 merge 审计

（源：README「用例质量分级」footnote；「上表」指该节分级表——-1 SKIP 8 / 0 BETA 110 / 2 PROD 569，合计 687。）

上表为 ARM64 gcc 构建的源码全量口径（2026-11-03 merge 复核，口径为构建出的 sdcshield 二进制 `tests` section 实例机械计数——含 SME/SVE2 组 4 个 PROD 用例 `sme_fmopa_stress_arm`/`sme_fmopa_za_arm`/`sme_za_tileview_arm`/`sve2_cross_precision_arm`，它们仅在相应硬件可执行。本 merge = main 侧 2026-11-02 opendcdiag 研究仓迁移增量 +121（21 PROD + 100 BETA `sdc_fuzz_*`）+ 同日 arm_add 新探针 +7 PROD + _neon 变体 +14 PROD + 633 cache 族新测试 +9 PROD，叠加 feat/eigen-sve-unification 分支 +12 SVE eigen 用例（8 PROD + 4 SKIP）− 退役 `eigen_svd_bidiag_sve`（1 PROD，dense SVE FMA 场景由 `eigen_svd_double_sve` 承接）；main 侧 2026-11-02 对迁移后全量的推算表 652（基于 2026-11-01 旧表 501 叠加——该 501 经 feat/eigen-sve-unification 考古为 SVE1 过滤后集合且较同日实测虚高 1，推算链不可精确拼接）与本 merge 机判 687 的差异未逐例考古，以实测为准）。运行时实际数量随芯片 spec 过滤：SVE1 主机 2026-11-03 merge 后实测 **565 PROD**（q0/q-1 = **675/683**）；无 SVE 鲲鹏 920 **2026-11-02 实测 439 PROD / 547 q0 / 551 q-1**（main 侧迁移/arm_add/_neon/cache 增量后口径；eigen SVE 分支 12 新用例 + 退役 bidiag 全部 `ARM64_REQUIRES_SVE` 门控，无 SVE 主机净变化 0）。无 SVE 鲲鹏 920 **2026-11-17 实测（main `d50ceee0`，vendored 5 库 install/ 全在位）458 PROD / 566 q0 / 570 q-1**（`arm0102_*` 基础 10 项 2026-11-03 退册、迁移组现存 111；`418e4e09`→`d50ceee0` no-SVE 可见集 diff 为空——eigen SVE 12 新/退役 1 与 SVE2 配套 14 全 SVE/SVE2 门控，净变化 0；439→458 的中间推算链不可精确拼接，以实测为准）。cn23154 复现分支（clang/libc++，含 SME 用例）2026-09-28 实测：默认 322，`--quality=-1` 全量 333。2026-11-13 feat/eigen-sve2-companion 批次追加 **14 个 SVE2 配套用例**（SVE 系 14 个的逐一镜像：BDCSVD×5 + JacobiSVD×4——SKIP 级跟随 Jacobi + GEMM×5；门控 `ARM64_REQUIRES_SVE2`，**本机（SVE1）所有 list 口径不可见——spec 过滤先于 list，仅 `-e` 显式注册并诚实 SKIP**；SVE2 主机默认集将 +10 PROD、`--quality=-1` 口径 +14，机判 `tests` section 实例计数 687 → **701**）。

（同 footnote 尾段的 WHILEWR 诚实边界与 gem5 功能验证链抽离至 [sve2-companion-objdump-audit.md](sve2-companion-objdump-audit.md)。）

## 各主机实测速查

（源：CLAUDE.md Build & run `--list-tests` 注释块。）

| 主机 | 默认 PROD | `--quality=0` | `--quality=-1` | 口径日期 |
|---|---|---|---|---|
| SVE1（0xd02，本机） | 565 | 675 | 683 | 2026-11-03 merge 复核 |
| 无 SVE 鲲鹏 920 | 458 | 566 | 570 | 2026-11-17 实测（main `d50ceee0`） |
| cn23154（clang/libc++，含 SME 用例） | 322 | — | 333 | 2026-09-28 实测 |

14 eigen `*_sve2` companions（2026-11-13，feat/eigen-sve2-companion）are INVISIBLE in every list 口径 on this SVE1 host — spec filter precedes list; explicit `-e` registers them and skips honestly. SVE2 host: default set +10 PROD, q-1 +14 (4 Jacobi are SKIP-level).

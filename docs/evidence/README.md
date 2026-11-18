# 证据文档索引（docs/evidence/）

带日期的实测证据链自 README.md / CLAUDE.md 主文抽离至此（2026-11-18 重构，源 commit `727cc8d3`）。
主文只保留一行结论 + 指向此处的链接；查证与审计走本目录。被清理出库的历史证据文件
（如 sve-all-vl-campaign 的原始 `matrix.md` / `triage-*.md`）按仓库惯例从 git 历史取回（`31f6a99e^`）。

| 文档 | 内容 | 覆盖日期 | 源 |
|---|---|---|---|
| [test-count-ledger.md](test-count-ledger.md) | 用例计数台账：各主机 PROD/q0/q-1 实测口径、merge 审计链与 SVE2 配套批次 | 2026-09 ~ 2026-11 | README「测试用例与检测能力」「用例质量分级」、CLAUDE.md Build & run |
| [sve-all-vl-campaign.md](sve-all-vl-campaign.md) | SVE 用例全 VL（128/256/512-bit）验证矩阵证据链，含 seed1/seed2 证据不对称披露 | 2026-10-29 ~ 2026-10-30 | README「SVE 用例全 VL 验证矩阵」 |
| [sve2-companion-objdump-audit.md](sve2-companion-objdump-audit.md) | Eigen `_sve2` 配套用例的 objdump A/B 审计、march 切换回归与 WHILEWR 诚实边界 | 2026-11-13 ~ 2026-11-17 | README「用例质量分级」footnote、CLAUDE.md ARM64 spec policy 条 |

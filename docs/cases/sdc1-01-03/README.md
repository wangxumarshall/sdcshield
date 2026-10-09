# SDC1-01-03：TaiShan 2280 突然复位（整机瞬时死亡，三层日志平面零记录）

| 项 | 内容 |
|---|---|
| 单板 | **sdc1-01-03**（TaiShan 2280，SN 2102312YVY10M6000038，Kunpeng 920 ×2=128 核 TaiShan-v110，30.5GB）——勿与 01-02（R240K V2 192 核，core179）、01-04（J353 G3，CPU122）混淆 |
| 首案 | 2026-10-09 11:47:37–11:48:00 整机瞬时死亡（战役 L4 dwell mesh_upi_sse_asymm_distrib_int 全核驻留 2.4h 处） |
| 现象签名 | 无 panic/无 vmcore/无 pstore-ERST/BERT 全零/SEL 无前兆/restart_cause=unknown/死前遥测全维正常/约 6 分钟无 POST 黑窗 |
| 复发史 | 战役 7×24 满载 13 天 7 次同签名自发复位（BMC 存活）；此前 19 天开发负载零复位 |
| 根因现状 | 【实锤】非软件、瞬时硬件级主机复位；【强推】CPU1 侧随机失效（死亡日更凉 + 一致性风暴强度与存活日全同 + openblas 相反画像也死 → 否证热/平均负载阈值模型）；【假设】触发器 ∈ {电源瞬态, 片上失效保护, 互连偶发楔死}——待实验裁别 |
| 文档 | [初步取证报告](SDC1-01-03-SUDDEN-RESET-PRELIMINARY-REPORT.md) · [根因定位实验设计](SDC1-01-03-ROOT-CAUSE-EXPERIMENT-PLAN.md) · [证据目录](evidence/) |
| 状态 | 初步报告完成（2026-10-09）；实验计划待批准执行（第一波 E0/E1/E5 零风险可立即启动） |

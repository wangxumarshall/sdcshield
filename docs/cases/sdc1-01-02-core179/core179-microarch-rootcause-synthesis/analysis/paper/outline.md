# 论文大纲与证据映射（Phase 2 产出）

标题（拟）：《一条指令的八次死亡：鲲鹏 920 单核装载返回通路间歇性静默数据损坏的微架构级根因诊断》
英文题（拟）：Eight Crashes, One Instruction: A Microarchitecture-Level Root-Cause Diagnosis of Intermittent Silent Data Corruption in the Load-Return Path of a Kunpeng 920 Core

## 叙事主线（福尔摩斯结构）
- 侦探 = 取证者；案件 = 8 次 kdump；凶手 = CPU179 LSU 装载返回通路；凶器 = 相位撕裂交付；动机 = 电压裕量 × 发射相位时序竞态；不在场证明 = RAS 全静默（恰是"凶手在盲区"的证据）

## 章节与证据映射
| § | 章节 | 素材来源 |
|---|---|---|
| 1 | 引言：SDC 之惑与本案 | MICROARCH_EVIDENCE §引言素材；CROSS_CASE §0 |
| 2 | 案发现场：8 次致命崩溃的现象学 | CROSS_CASE §1-2（每案事实表+汇总表） |
| 3 | 排除链：软件层一切嫌疑的系统性排除 | CROSS_CASE §8.1；DIAGNOSIS_REPORT §5-6 |
| 4 | 决定性实验：内存真值 vs 寄存器实收 | DECISIVE_EXPERIMENTS 全部；DIAGNOSIS_REPORT §3 |
| 5 | 形态学：四种撕裂子族与 ARM 不变式 | CROSS_CASE §2/§8.2；DECISIVE_EXPERIMENTS 附录 |
| 6 | 统计画像：138 事件的时空分布 | CROSS_CASE §3-4 |
| 7 | 微架构下钻：从 ISA 到 LSU 返回通路 | MICROARCH_EVIDENCE A/C/G |
| 8 | 物理机制：相位×电压裕量×间歇性 | MICROARCH_EVIDENCE A3/D1/D4 |
| 9 | 静默性解构：为何一切检测手段都失明 | MICROARCH_EVIDENCE A6/B/D5；CROSS_CASE §6 |
| 10 | 启示：规避/消减/暴露 | MICROARCH_EVIDENCE H |
| 11 | 证据边界与未决问题（诚实性） | MICROARCH_EVIDENCE I2；CROSS_CASE §8.2 |
| 12 | 结论 | — |

## 关键数字（全文一致性锚）
- 8 案 = 7 本地 + 1 单板（09-03）；130 WARNING + 8 Oops = 138 事件；100% CPU179
- 7/8 致命命中 find_busiest_group+0x140；Code 五指令字逐字相同
- fbG 6+1=7 案代数闭合 x27=(x1+x20) mod 2^64；FAR=x27+0x120（低48位）
- 子族：零塌缩×2、ROR8×2、ROL16×1、ROR16/相位+污染×1、变址乱码×1、ROR8+污染×1（第8案修正）
- 决定性实验：offset[60] 真值 ffffbe56fa9b6000 vs 实收 a000ffffbe56fb25；offset[12] 真值 ffffb617dc4d6000 vs 实收 00ffffb617dd3940；两案反事实地址均 VALID 且数据健全
- gem5 佐证：byte_lane_skew/phase 100% SDC（64/64）；no-op 相位塌方 100%→10-20%；-30mV 复现；l1d_disable 四反例；冗余重算 Fisher p=1.19e-71；D13 定向检出 7.79×

## 风格
- 中文学术正式语体，术语保留英文
- 反模式自查：无 "delve into"/滥用破折号/清嗓开头；段落长短错落
- 每一 claim 挂证据编号（表/节/附录）

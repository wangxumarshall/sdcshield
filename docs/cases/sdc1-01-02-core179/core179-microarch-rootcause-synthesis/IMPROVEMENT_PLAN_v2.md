# 改进方案 v2（基于 REVIEW_v2.md + 源码/ARM ARM/Python 独立验证）

原则：CLAUDE.md 补丁纪律，一单元一补丁、自验证、feature 分支（已 `fix/paper-review-v1-honesty-hardening`）、不推 main。所有修订须有真实命令/源码/手册支撑，不杜撰。

## 本轮独立验证（本会话，非声称）

| 验证项 | 方法 | 结果 |
|---|---|---|
| D1 15:58 实锤 | Python 复算 | `ror1(slot0=0xffffcc879da2e000)==x20=0x00ffffcc879da2e0` Hamming=0 ✓；`slot0^x20` popcount=30 ✓；单字节位翻转穷举无命中 ✓ |
| D1 真值距离 | Python 复算 | `slot146^x20` popcount=26（slot[146]=0xffffcc879ed92000 是真值，slot[0] 是 stale source 非 truth） |
| D1 0814 slot 索引 | 对照 DIAGNOSIS + 复算 | DIAGNOSIS:64 称 0814 匹配 `offset[0]=0xffffd93715b7e000`；`ror6(slot0)=0xd93715b7e000ffff` Hamming=6 ✓；paper §3.2 误写 slot[1] ✗ |
| D1 0814 tie | Python 复算 | `ror6(slot0)` ≡ `rol2(slot0)`（6+2=8 字节=整字）=0xd93715b7e000ffff，Hamming=6；论文只命名 ror6 ✗ |
| ESR bit6 位域 | ARM ARM DDI 0487 解析 | EC=0x25 ISS bit6=S1PTW 低位（非"Overlay"）；70/73 `0x96000044`→S1PTW=1（walk 中，对 D3 预期）；3/73 `0x96000004`→S1PTW=0（无 walk，异质） |
| 源码 9 项断言 | grep CHAOS/gem5/src | D1 lsq_unit.cc:1498、D2 lsq.cc:1146、D3 table_walker.cc:1959、mmu.cc:1226-1227、conditionalValidBit CHAOSPTW.cc:113-120、ECC 早退 105-108、rng lambda 三处、faults.cc:1086-1087 FAR 不屏蔽、byte_lane_skew 右旋、setPtwInj mmu.hh:107，全部 MATCH ✓ |
| stale_line_replay | grep 构建路径 | 未实现（源码无命中）✓ 确认 DA-F14 |
| 分支名 | git branch | `docs/core179-microarch-rootcause` 本地不存在（仅远程 `-droped` 已删变体）；§5.4 用 `fi-h6-h7-fs-verify` |
| kunpeng.md 路径 | ls | 实际在 `docs/cpu/kunpeng.md`，Supplement 误引 `docs/kunpeng.md` |

## 修订项（按严重度+ROI 排序，A=本机可立即执行）

### Patch G2（MAJOR，实质更正）：0814 slot[1]→slot[0] + tie 披露 + 解析更正
- paper_en/zh §3.2 line 192/192：`ror6(__per_cpu_offset[1])` → `ror6(__per_cpu_offset[0])`（值 `0xffffd93715b7e000`，与 DIAGNOSIS:64 一致）
- 加 tie 披露："ror6 ≡ rol2 for a 64-bit word（6+2=8 字节=整字旋转）；故 0814 旋转在 (rol2, ror6) 歧义下识别，幅度非唯一确定"
- §6 #1（paper_en:363）"ror6 at Hamming-6 (08-14, nearest rotation)" → "a 2-or-6 byte rotation (rol2≡ror6 ambiguity) at Hamming-6 (08-14, nearest)"
- "15b7→15ba differs by 1 bit" 解析更正：`ror6(slot[0])=0xd93715b7e000ffff` vs x20=`0xd93715ba0000ffff`，6 bit 跨 2 字节（`15b7e000`→`15ba0000`）
- 自验证：Python 复算嵌入注释

### Patch G3（CRITICAL-标签，诚实标签）：30-bit 标签修复 + 真值距离 26 双报
- 摘要、§3.2、§5.2：把 "the truth `slot[0]`" / "XOR-distance from the truth" 改为 "the stale-replay source `slot[0]`（the value the load actually returned, per the stale-replay model）"
- 保留 30（stale-source 模型正确上界）；加披露："真值 `__per_cpu_offset[146]` 到观测值 x20 的 XOR 距离为 26 bit（slot[146]=0xffffcc879ed92000）"
- 明示：30 上界依 stale-replay 前提（故障作用于 load 实际返回的 slot[0]）；若故障作用于真值 slot[146]，上界为 26
- 自验证：Python 复算嵌入注释

### Patch G4（MAJOR，措辞降级）：H5 "falsifiable/verified"→consistency-check + stale_line_replay 未实现标注
- §5.2：删 "falsifiable in the Popperian sense"；改 "consistency/closure check（结构模型闭合猜想-验证回路，复现因果链，非独立 Popperian 检验）"
- H5 各处"verified" → "consistency-checked / mechanically reproduced（一致性检查，非独立验证：注入器旋转操作即其复现的 D1 签名同一操作）"
- §4.1 加注：`stale_line_replay` 模式已设计（FI_DESIGN_SUPPLEMENT §3.1）但未实现；D1 模型"stale source"半由 §3.2 法证验证，非 H5 仿真
- §5.2 表 byte_lane_skew 28/30=93%、bit_flip 29/30=97%、all_zero 29/30=97%，"same detection rate (97%)" 改分别标注

### Patch G5（MAJOR，措辞降级）：H7 "robust/5-5 stability"→no-perturbation 对照
- §5.4、§7、摘要：删 "robust" / "5/5 directional stability...is robust"
- 改：ECC-on 臂是 no-perturbation 对照（flip 发生前被 gate），非 ECC-correction 演示；5-seed ECC-off 臂示 1–4 spurious/seed；对照仅立注入器自洽，不立 ECC 在硅上纠正 landed flip

### Patch G6（MAJOR，架构精确化）：ESR S1PTW 纠正 + 3/73 异质披露
- paper_en/zh §3.4：加 ESR 形态：70/73 `0x96000044`（S1PTW=1，walk 中，对 D3 预期）/ 3/73 `0x96000004`（S1PTW=0，无 walk，异质，可能 TLB 竞态/条目损坏）；S1PTW=1 对 D3 是预期非异常
- DIAGNOSIS_REPORT §3.1:44 / §9.5:138："bit6=Overlay 位...应 RES0" → "bit6=S1PTW（stage-1 page-table walk 标志）低位；walk 中取的 translation fault 应 S1PTW=1"

### Patch G7（MAJOR，逻辑更正）：TBI1 "partial recovery"→不适用（地址本身已损坏）
- §3.3、§7：删 D2 的 "partially recovers"/"partial recovery"
- 改：TBI1-off 排除有效内核地址的软件 top-byte 剥离，但 0814/0824 地址非有效内核地址（是 D1 损坏的非规范值），故 TBI 分析对这些案例不适用；D2 硅证据仍被 D1 完全混淆

### Patch G8（MAJOR，可复现性诚实）：§8 FS 表无脚本
- §8：明示 H6 5-seed FS 表与 H7 5-seed ECC 表系手工 `o3_chaos_fs.py` 跨 seed 跑出，无单脚本复现 harness；`run_H6/H7.sh` 仅复现 SE null

### Patch G9（MAJOR，措辞一致）：below-RAS 一致化
- 摘要、§1:19、§2.5:163："below the coverage of architectural RAS checkers" → "consistent with, but not proof of, sub-RAS coverage（RAS-not-probing / firmware-swallow alternatives not excluded — §7）"

### Patch G10（MAJOR，来源诚实）：TSV110 几何来源标注
- §2.1：几何来源标注 "community-documented（`docs/cpu/kunpeng.md`），非厂商 datasheet"；组相联裁决条件依赖该几何正确
- 摘要 "published TSV110 geometry" → "community-documented TSV110 geometry"

### Patch G11（MINOR，完整性）：§3.1 转储完整集声明
- §3.1：加 5/6 转储为 2026-08-14 至 2026-08-26 窗口内 CPU179 全部崩溃、无任何转储被获取后排除

### Patch G12（MINOR，路径）：kunpeng.md 路径 + 分支名一致
- MICROARCH_SUPPLEMENT：`docs/kunpeng.md` → `docs/cpu/kunpeng.md`（多处）
- 前言分支名 vs §5.4：统一标注

### Patch G13（MAJOR，内部一致）：MICROARCH_SUPPLEMENT §2.2 stale 同步
- §2.2 全段对齐 paper §3.2：`rol6` Hamming-0 → `ror6` Hamming-6；slot[1]→slot[0]；删 2⁻⁵⁸；加 tie 披露

### Patch G14（MINOR，精度）：DIAGNOSIS 0814 "仅差 1 字节"→"6 bit 跨 2 字节"
- DIAGNOSIS_REPORT §3.2 row1（:64）："仅差 1 字节" → "6 比特跨 2 字节"

## 执行顺序
G2 → G3 → G4 → G5 → G6 → G7 → G8 → G9 → G10 → G11 → G12 → G13 → G14
每个：编辑双语同步（+ DIAGNOSIS/MICROARCH 如涉） → 自验证（grep 确认措辞落地 + Python 复算嵌入）→ commit → push。

## 不在本次执行范围（future work，诚实保留）
- H6 guest-visible oops 谱（O3 fetch-stall 架构限制，需 non-O3 fault model）
- 跨案例迁移（需第二台故障机）
- H7 严格同路径对照（需 non-cascading / in-place ECC-correction 模型）
- stale_line_replay 实现后补 D1 "stale source" 半的独立仿真证伪
- §2.5 耦合故障经典文献引用（需文献检索）

# 多视角同行评审报告（v2）二轮：在 REVIEW_v1 修复之上的新发现

评审对象：`paper_en.md` / `paper_zh.md`（双语同步草稿，目标 ASPLOS/MICRO/HPCA）
评审日期：2026-08-30
评审模式：full（五席 + 编辑综合），二轮独立评审
前置：REVIEW_v1 已捕获并已修复 F1–F11（0814 Hamming-0 降级、30-bit XOR 上界、single-case 前置、"localized"→"consistent with"、H7 directional、run_H7.sh SE 标注、opendcdiag 迁移标注、gem5 FAR [63:60]、TBI1 runtime 保留、78 事件前置、seed-0 方差入正文）。本评审不重复已修复项，只报告新发现。

评审依据（实证，非声称）：所有技术断言均经本评审独立验证：
- 9 项源码断言对照 `CHAOS/gem5/src/` 真实源码逐行核对（D1 钩 `lsq_unit.cc:1498`、D2 钩 `lsq.cc:1146`、D3 钩 `table_walker.cc:1959`、`mmu.cc:1226-1227`、`conditionalValidBit` `CHAOSPTW.cc:113-120` + ECC 早退 `105-108`、rng lambda 三注入器、`faults.cc:1086-1087` FAR 不屏蔽、`byte_lane_skew` 右旋、`setPtwInj` `mmu.hh:107`），全部 MATCH。
- D1 签名数值经 Python 独立复算：15:58 `ror1(slot0)==x20` Hamming=0 ✓；`slot0^x20` popcount=30 ✓；`slot146^x20` popcount=26 ✓；单字节位翻转穷举无命中 ✓；0814 `ror6`/`rol2` 均 Hamming=6（tie 未披露）✓。
- ESR 经 ARM ARM（DDI 0487）位域解析：bit6 = ISS[6] = S1PTW 低位（非 DIAGNOSIS 所称 "Overlay"）；70/73 `0x96000044`→S1PTW=1（walk 中），3/73 `0x96000004`→S1PTW=0（无 walk）。
- 争议项（30-vs-26、slot0-vs-slot1、ESR bit6）经独立计算裁决，结论见下。


## Phase 0 领域识别与审稿人配置

沿用 REVIEW_v1 的五席配置（HPCA/MICRO EIC、FI 方法学 R1、ARMv8/RAS R2、机群 SDC R3、固定 Devil's Advocate）。本二轮聚焦 REVIEW_v1 漏检的内部不一致、标签错误、循环论证、与架构手册冲突。


## Phase 1 五席评审（仅新发现）

### 席 1 Journal-Fit Reviewer (EIC)

[MAJOR-G1] 摘要/§1/§2.5 仍以"established"措辞陈述"below RAS coverage"，与 §7 的诚实降级冲突。
- §7（paper_en:380）已将"below RAS coverage"诚实降级为 underdetermined（RAS 可能不探 fill-buffer/PTW，或固件吞掉 corrected error）。但摘要（paper_en:11）、§1（:19 "below the coverage of architectural RAS checkers"）、§2.5（:163 "at a granularity below the architectural RAS checkers, which recorded zero events"）仍以 established 口吻陈述该定位。读者只读摘要会带走 over-claim。
- 修复：摘要/§1/§2.5 的"below RAS coverage"改为"consistent with, but not proof of, sub-RAS coverage（RAS-not-probing / firmware-swallow alternatives not excluded — §7）"。

### 席 2 Methodology Reviewer (R1)

[MAJOR-M1] H5 的"可证伪"措辞是 rigged-to-pass：证伪分支按构造不可达。
- §5.2（paper_en:266）称主张"falsifiable in the Popperian sense"。但证伪条件"若有界位翻转能复现字节相位位移签名则证伪"按构造不可达：30-bit XOR 上界证明位翻转复现不了该值，而结构模型定义上就能产生旋转（注入器即 `ror`/`rol` 实现）。证伪分支双重死亡。这不是 Popper 意义上的可证伪，是 construction-from-signature 的一致性检查。
- 修复：§5.2 删除"falsifiable in the Popperian sense"；H5 改述为"consistency/closure check（结构模型闭合猜想-验证回路，复现因果链，非独立 Popperian 检验）"。诚实标注：独立证伪需预测性可观测量（如在新转储上先预测旋转方向/幅度再测量）。

[MAJOR-M2] §5.2 边界测试近同义反复：探针测"any corruption"而非"signature form"。
- §5.2 表（paper_en:258-262）用 `ptr_corrupt`（loaded pointer ≠ golden）作因变量，比较三种故障模型。但该探针对任何值损坏都触发，故三模型都 ~97% 是近乎必然的。"边界测试"测的是"损坏是否发生"（三模型都损坏），而非"签名形态"，它在测一个对所论性质定义上不敏感的量。论文甚至承认结论来自 §3.2 穷举而非该表（:264），即结论是 despite 该表而非 because of 它。该表是 rigor-theater。
- 修复：要么换一个能区分签名形态的探针（检查损坏值是否匹配某陈旧数组条目的旋转副本，而非仅 ≠ golden），要么明确"该表仅测检出率、非签名形态等价性测试"，并删除其"honestly bounds the claim"框架，承重证据只有 §3.2 穷举。

[MAJOR-M3] §5.2 "same detection rate (29/30=97%)" 措辞掩盖 byte_lane_skew 实为 28/30=93%。
- 表中 `byte_lane_skew` 28/30=93%、`bit_flip` 29/30=97%、`all_zero` 29/30=97%。但正文（:264）称"same detection rate (29/30=97%)"，把 D1 相关模式（byte_lane_skew, 93%）的较低检出率抹平为 97%。n=30 下差异不显著，但"same"措辞不精确，且恰好掩盖了 D1 相关模式检出率更低这一事实。
- 修复：改为"byte_lane_skew 28/30=93%、bit_flip 29/30=97%、all_zero 29/30=97%，n=30 统计噪声内，但非相同"。

[MAJOR-M4] §8 可复现性声称对 5-seed FS 表不诚实：run_H7.sh 仅复现 SE null。
- 前言（paper_en:5）"Every quantitative claim...reproducible"；§8（:391）列 `run_H6.sh`/`run_H7.sh` 为实验脚本。但 `run_H7.sh` 顶部注释自陈"runs the SE-mode config...there is no automated FS script"，FS 5-seed ECC 表（:345-351）与 H6 5-seed 表（:297-303）系手工 `o3_chaos_fs.py` 多 seed 跑出。承重量化结果无对应可复现脚本。
- 修复：§8 明示"H6 5-seed FS 表与 H7 5-seed ECC 表系手工调 `o3_chaos_fs.py` 跨 seed 跑出，无单脚本复现 harness；`run_H6/H7.sh` 仅复现 SE null。复现 FS 表需 ~10-20h FS 跑、特定 gem5.opt 构建；每 seed 确切命令见 FI_DESIGN_SUPPLEMENT §7"。要么交付 FS harness，要么撤回该两表的"every quantitative claim reproducible"笼统声称。

[MAJOR-M5] 转储选择偏差仅部分处理。
- §3.1（paper_en:175）报 5 转储为分析基 + 第 6 转储。但未声明这 5/6 是否为 CPU179 在窗口内的完整崩溃集，亦未声明是否有签名不符的转储被排除。0817 vmcore-incomplete 仅 dmesg 入统计。法证案例的外部效度取决于是否存在被排除的不符转储。
- 修复：§3.1 加一句"此 5（及第 6）转储为 2026-08-14 至 2026-08-26 窗口内 CPU179 记录的全部内核恐慌；该窗口内无任何 CPU179（或其他核）的崩溃转储被获取后排除于本分析。0817 转储 vmcore-incomplete（crash 拒载），其 dmesg 入统计"。

### 席 3 Domain Reviewer (R2)

[MAJOR-D1] TSV110 几何来源非一手：`docs/cpu/kunpeng.md` 是无署名社区摘要，非厂商 datasheet。
- §2.1（paper_en:123）/摘要（:11 "published TSV110 geometry"）以"published"口吻引用几何参数（64KB 4-way、2×128bit 端口、store-fwd 6-7cyc）。实际来源 `docs/cpu/kunpeng.md` 无作者、无引用、无厂商归属，措辞如"鲲鹏920是华为海思于2019年发布"。D1"fill-buffer 非 L1D 阵列"论证（MICROARCH_SUPPLEMENT §2.1 set 87 vs 105）完全依赖该几何正确。
- 修复：§2.1 改为"微架构参数来自社区维护摘要（`docs/cpu/kunpeng.md`），非厂商 datasheet；HiSilicon 未公开 TSV110 RTL/datasheet。组相联裁决（set 87 vs 105）条件依赖该几何正确；若厂商披露不同路数/索引位切片，D1'fill-buffer 非 L1D'排除须重评。此为条件论证非既定"。摘要"published TSV110 geometry"降为"community-documented TSV110 geometry"。

[MAJOR-D2] TBI1"partial recovery"对已被破坏地址不适用。
- §3.3（paper_en:198）称 TBI1-off"partially recovers" D2（内核不 top-byte-strip 这些地址 → FAR-MSB 差异非软件 TBI 伪象）。但同段点 (3) 承认 0814/0824 `arch_addr` 高字节 0xd9/0x55 本身是 D1 坏值的非规范高字节。被做 TBI 分析的地址本身是已损坏的非规范值。TBI1 控制 MMU 是否 strip 有效内核 VA 的顶字节；对本身是损坏值的地址做 TBI 分析，混淆了两个问题：(i) TBI1 是否 strip 有效 0xffff 地址（静态分析答否），(ii) 0xd9/0x55 MSB-vs-FAR 差异是否指示地址通路缺陷（不可知，因输入是 D1 垃圾）。"partial recovery"是非 sequitur。
- 修复：删除 D2 的"partially recovers"/"partial recovery"；改为"TBI1-off 排除了有效内核地址的软件 top-byte 剥离，但 0814/0824 地址非有效内核地址（是 D1 损坏的非规范值），故 TBI 分析对这些案例不适用。D2 硅证据仍被 D1 完全混淆；TBI 既不 rescue 也不进一步损害它"。

[MAJOR-D3] ESR bit6 是 S1PTW（非"Overlay"）；3/73 事件 S1PTW=0 与 PTW-readout 假设不符，未披露。
- DIAGNOSIS_REPORT §3.1（:44）/§9.5（:138）称 70/73 ESR=`0x96000044`、3/73=`0x96000004`，并称"bit6=Overlay 位在本代 v8.2 核上应 RES0"。本评审据 ARM ARM（DDI 0487）解析：ESR EC=0x25（Data Abort current EL）的 ISS 中，bit6 是 ISS[6]，即 S1PTW（stage-1 page-table walk 标志）的低位，非"Overlay"。对在 walk 中取的 Data Abort，S1PTW=1 属预期（walk 进行中）。故：
  - 70/73 `0x96000044`：ISS[7:6]=0b01（S1PTW=1，walk 中）→ 与 D3（PTW readout）一致（非异常）。
  - 3/73 `0x96000004`：ISS[7:6]=0b00（S1PTW=0，无 walk）→ 错误非在 walk 中取 → 与 PTW-readout 假设不直接一致，可能为不同机制（TLB invalidation 竞态 / TLB 条目损坏）。
- 论文 §3.4（paper_en:200-202）将 73 事件一律作 D3 证据，未披露此异质性，亦未提 ESR。DIAGNOSIS 把 bit6 错标为"Overlay"且称"应 RES0"（双重错误：错标名 + 错称异常）。
- 修复：(1) §3.4 加"ESR 形态：70/73 为 `0x96000044`（S1PTW=1，walk 中，与 D3 一致），3/73 为 `0x96000004`（S1PTW=0，无 walk，不直接与 PTW-readout 一致，可能为 TLB 竞态/条目损坏等其他机制）；S1PTW=1 对 D3 是预期而非异常"。(2) DIAGNOSIS_REPORT §3.1/§9.5 把"bit6=Overlay，应 RES0"更正为"bit6=S1PTW（stage-1 walk 标志）低位；walk 中取的 TF 应 S1PTW=1"。

### 席 4 Perspective Reviewer (R3)

（沿用 REVIEW_v1 已捕获的外部效度项；二轮无新 R3 级发现。）

### 席 5 Devil's Advocate（固定第五席）：核心论证挑战

[CRITICAL-DA1] "30-bit XOR 上界"的标签错误：slot[0] 被称为"the truth"，实为"stale source"；但 30 这个数本身正确。
- 摘要（paper_en:11）、§3.2（:194 "the truth `slot[0]`"）、§5.2（:254 "XOR-distance from the truth...30 bits"）将 slot[0] 称为"the truth"并据之给 30-bit 上界。但 load 目标是 slot[146]，真值是 slot[146]。本评审独立复算：
  - XOR(slot[0], x20) popcount = 30（slot[0] 是 stale-replay 的损坏源，非真值）
  - XOR(slot[146], x20) popcount = 26（slot[146] 才是真值）
- 裁决（关键）：30 与 26 哪个是"位翻转能否复现损坏"的正确上界，取决于故障作用在哪个值上。论文的论证是"观测值是 slot[0] 的旋转副本"，即 load 实际返回的是 slot[0]（stale entry），故位翻转作用于 slot[0]，30 是 stale-source 模型下的正确上界；26 只在"故障作用于真值 slot[146]"时才相关。故数字 30 内部自洽，但标签"sloppy"：slot[0] 是 stale source 非 truth。
- 修复（非 30→26 替换，而是标签 + 双报）：§3.2/§5.2/摘要 把"the truth `slot[0]`"改为"the stale-replay source `slot[0]`（the value the load actually returned, per the stale-replay model）"；保留 30 作 stale-source 模型上界；同时披露"真值 slot[146] 到观测值的 XOR 距离为 26 bit"，使怀疑者可见两者。明确：30 上界依 stale-replay 前提；若故障作用于真值，上界为 26。

[MAJOR-DA2] H5 循环论证：注入器为复现旋转而造，复现旋转是同义反复。
- H5"verified"（paper_en:13/30/248）。但 `byte_lane_skew` 注入器操作即字节旋转，D1 签名即字节旋转。"为复现签名 X 而造的模型复现了签名 X"是同义反复。H5 证明的是因果链（旋转→非规范 VA→页错误→oops），对任何规范内核地址的旋转都平凡为真；它不独立证明缺陷就是旋转，那仍只靠 §3.2 法证。且 `stale_line_replay` 模式（FI_DESIGN_SUPPLEMENT §3.1 设计，会测"stale"半）未实现（源码 grep 无命中），故 D1 模型的"stale"半与"rotation"半均无独立验证（rotation 半同义反复复现，stale 半未测）。
- 修复：H5 "verified"降为"consistency-checked / mechanically reproduced（一致性检查，非独立验证：注入器的旋转操作即其所复现的 D1 签名同一操作）"；§4.1 标注"`stale_line_replay` 模式已设计但未实现；D1 模型的'stale source'半由 §3.2 法证验证，非由 H5 仿真"。

[MAJOR-DA3] 0814 槽位索引错误：DIAGNOSIS 说 slot[0]，论文说 slot[1]，实质矛盾。
- DIAGNOSIS_REPORT §3.2（:64）称 0814 匹配 `offset[0](=0xffffd93715b7e000)`；本评审独立复算 `ror6(slot[0]=0xffffd93715b7e000)=0xd93715b7e000ffff`，Hamming=6 ✓。但 paper_en §3.2（:192）写 `ror6(__per_cpu_offset[1])`，即 slot[1]。REVIEW_v1（:57）亦误引"offset[1]"（系 REVIEW_v1 自身的转录错误，被 paper 继承）。slot[0] 与 slot[1] 是不同 CPU 的 per-cpu 偏移，值不同。DIAGNOSIS 的 slot[0] 计算经独立复算正确；论文的 slot[1] 与之冲突。
- 修复：paper_en/zh §3.2 把 `__per_cpu_offset[1]` 更正为 `__per_cpu_offset[0]`（值 `0xffffd93715b7e000`，与 DIAGNOSIS 一致）；§3.2 的"15b7→15ba"解析同步更正（`ror6(slot[0])=...15b7e000...` 对 x20 `...15ba0000...`，6 bit 跨 2 字节）。REVIEW_v1 的"offset[1]"误引亦标注更正。

[MAJOR-DA4] 0814 旋转有未披露的 tie：ror6 ≡ rol2（6+2=8 字节=整字旋转），论文只命名 ror6。
- 对 64 位字，`ror6(x)`（右旋 6 字节）与 `rol2(x)`（左旋 2 字节）产生相同值（因 6+2=8 字节=64 位=整字旋转）。本评审独立复算：0814 `ror6(slot[0])` 与 `rol2(slot[0])` 均为 `0xd93715b7e000ffff`，Hamming=6。论文/Supplement 只命名 ror6，未披露 degeneracy。对字节旋转签名，旋转方向与幅度是定义性特征；若两对 (方向,幅度) 给同值，则"签名"未唯一识别。§6 DFT 建议 #1（paper_en:363 "ror6 at Hamming-6"）的 DFT 向量在 ror6 vs rol2 间任意。
- 修复：§3.2 加"注：0814 案例中 rol2 与 ror6 对 slot[0] 产生相同值（0xd93715b7e000ffff，Hamming-6），故旋转在 (rol2, ror6) 歧义下识别；§6 #1 的 DFT 向量应针对幅度 2-or-6 的字节通道相位位移，非唯一确定的旋转"。§6 #1 "ror6 at Hamming-6"改"2-or-6 字节旋转（rol2≡ror6 歧义）at Hamming-6"。

[MAJOR-DA5] H6"separability"实为 stall-separability，非 SDC/Crash-separability；D2 ~3k stall 是通用的（D3 高 prob 也 ~3k）。
- H6 5-seed 表（paper_en:295-305）D1-only→5/5 正常推进（~400k insts）、D2-only→5/5 stall（~3k insts），称"separable across 5 seeds"。但 (1) D1 在 FS 模式的"正常推进"≠"无 SDC"：FS 无 ptrskew 探针，D1 可能静默损坏而不 stall；可分性测的是 stall 行为而非 H6 假设所论的 SDC/Crash 行为。(2) 论文自承（§7:377）D3 高 prob 也 stall ~3k，故 ~3k stall 是对非规范 VA 注入的通用响应，非 D2 特异。
- 修复：§5.3 重述"H6 建立 stall-separability（D2 非规范 VA stall 仿真器；D1 字节旋转不 stall），非 SDC/Crash-separability（后者需 guest 可见 oops 分类，gem5 O3 fetch-stall 模型给不出）。~3k stall 是非规范 VA 的通用响应（D3 高 prob 亦见），非 D2 特异"。

[MAJOR-DA6] H7 ECC-on 臂近同义反复：flip 在发生前被 gate，"ECC-on 0 spurious"="没做实验"。
- §5.4（paper_en:352-353）已承认 ECC-on 臂未被注入（`conditionalValidBit` flip 在 ptwEcc on 时早退）。但正文仍称"5/5 directional stability...is robust"并以之作 H7 directional verdict。怀疑者结论：这只证明"注入产 spurious、不注入产 0"，按构造为真，对硅上 ECC 能否 catch 真 PTW 缺陷无所证。"intended controlled variable"措辞是 sleight-of-hand。
- 修复：删"robust"/"5/5 directional stability"作为 evidence-bearing；改为"5-seed ECC-off 臂示 1–4 spurious/seed（真扰动→真 spurious）；ECC-on 臂是 no-perturbation 对照，非 ECC-correction 演示。该对照仅立注入器自洽，不立 ECC 在硅上纠正 landed flip"。


## Phase 2 编辑综合与决定

### 共识与分歧

共识（corroboration，跨席）：
1. H5/H7 循环论证 + 措辞过强：Devil's Advocate（DA2/DA6）、R1（M1/M3）独立命中，H5"falsifiable"与 H7"robust"均近同义反复/按构造不可证伪。
2. ESR 异质 + bit6 误标：R2（D3）、Devil's Advocate（隐含）命中，3/73 S1PTW=0 未披露，bit6 误标"Overlay"。
3. 标签错误：Devil's Advocate（DA1 标签 / DA3 slot 索引 / DA4 tie）、R1（M3 检出率）独立命中多处标签/精度缺陷。
4. 可复现性 §8：R1（M4）、Devil's Advocate（隐含）共识，FS 表无脚本复现。

分歧（需仲裁）：
- Devil's Advocate 最初主张"30→26 替换"（DA1）；本评审独立计算仲裁：30 是 stale-source 模型正确上界，26 是真值距离。采纳标签修复 + 双报，否决简单 30→26 替换（后者会破坏论文自身 stale-replay 论证）。
- Devil's Advocate 称 bit6"应 RES0"本身亦错；本评审据 ARM ARM 仲裁：bit6=S1PTW，walk 中为 1 是预期。采纳：纠正标签 + 披露 3/73 异质 + 不再称"异常"。

### Devil's Advocate CRITICAL 裁决

- DA-CRITICAL-1（30-bit 标签错误）：validated（标签层）。slot[0] 非"the truth"而是 stale source；论文多处误标。但数字 30 在 stale-source 模型下正确，故不阻止发表，须标签修复 + 披露真值距离 26。`[DA-CRITICAL-VS-ACCEPT: 1 validated, label-level]` 上报。
- DA-MAJOR（H5 循环 / H7 同义反复 / ESR 异质 / slot 索引 / tie / §8 复现 / TBI 不适用 / 几何来源）：validated。均须修正，方向明确可执行，不阻止 Accept 最终化。

### 编辑决定

Major Revision（二轮）。

理由：REVIEW_v1 已闭合数据造假级问题；本轮新发现均为标签错误、循环论证措辞、内部不一致、与架构手册冲突、可复现性声称过宽，属"诚实化未尽"而非"数据不实"。D1 15:58 实锤证据（Hamming-0 + 30-bit stale-source 上界 + 位翻转穷举无命中）经本轮独立复算仍成立（✓）；0814 的 slot[1]→slot[0] 与 tie 披露、ESR S1PTW 纠正、30-bit 标签 + 26 双报、H5/H7 措辞降级、§8 复现性收窄，均可在本机立即执行。

### 修订路线图（Revision Roadmap v2，非排序核心）

按 ROI + 可执行性排序（A=本机可立即执行且高确定）：

| # | 修订项 | 席 | 严重度 | 类别 | 可执行性 | 具体操作 |
|---|---|---|---|---|---|---|
| G2 | 0814 slot 索引 slot[1]→slot[0] + tie 披露 + 解析更正 | DA3+DA4 | MAJOR | 实质更正 | A（本评审已复算） | paper_en/zh §3.2：`__per_cpu_offset[1]`→`[0]`（值 0xffffd93715b7e000）；加"ror6≡rol2 tie"；§6 #1 "ror6"→"2-or-6 字节旋转（rol2≡ror6 歧义）"；"15b7→15ba"解析同步更正 |
| G3 | 30-bit 标签修复 + 真值距离 26 双报 | DA1 | CRITICAL(标签) | 诚实标签 | A（本评审已复算） | 摘要/§3.2/§5.2："the truth `slot[0]`"→"the stale-replay source `slot[0]`（the value the load actually returned, per the stale-replay model）"；保留 30；加"真值 slot[146] 到观测值 XOR 距离 26 bit" |
| G4 | H5 "falsifiable/verified"→consistency-check + stale_line_replay 未实现标注 | DA2+M1 | MAJOR | 措辞降级 | A | §5.2 删"falsifiable in the Popperian sense"；H5→"consistency-checked, mechanically reproduced"；§4.1 标注 stale_line_replay 未实现 |
| G5 | H7 "robust/5-5 stability"→no-perturbation 对照 + 去过强 | DA6+M3 | MAJOR | 措辞降级 | A | §5.4/§7/摘要：删"robust"；ECC-on 臂明示 no-perturbation 对照、非 ECC-correction 演示 |
| G6 | ESR S1PTW 纠正 + 3/73 异质披露 | D3 | MAJOR | 架构精确化 | A（本评审据 ARM ARM 解析） | paper_en/zh §3.4：加 ESR 形态 + S1PTW 70/73=1（一致）/3/73=0（异质，非 walk）；DIAGNOSIS §3.1/§9.5："bit6=Overlay 应 RES0"→"bit6=S1PTW，walk 中 TF 应=1" |
| G7 | TBI1 "partial recovery"→不适用（地址本身已损坏） | D2 | MAJOR | 逻辑更正 | A | §3.3/§7：删"partially recovers"；改 TBI 分析对非规范损坏地址不适用，D2 仍被 D1 完全混淆 |
| G8 | §8 复现性收窄：FS 表无脚本 | M4 | MAJOR | 可复现性诚实 | A | §8：明示 H6/H7 5-seed FS 表系手工跑、无脚本；run_H6/H7.sh 仅复现 SE null |
| G9 | "below RAS coverage"→underdetermined 一致化（摘要/§1/§2.5） | G1 | MAJOR | 措辞一致 | A | 摘要/§1/§2.5："below RAS coverage"→"consistent with, not proof of, sub-RAS coverage（§7）" |
| G10 | TSV110 几何来源标注为社区摘要 + 条件依赖 | D1 | MAJOR | 来源诚实 | A | §2.1：几何来源标注 community-documented、非厂商 datasheet；组相联裁决条件依赖 |
| G11 | §5.2 "same detection rate"→93%/97% 分别 | M3→合并G4区 | MINOR | 精度 | A | §5.2："same 97%"→"byte_lane_skew 93%、bit_flip/all_zero 97%"（并入 G4 同单元） |
| G12 | §3.1 转储选择完整集声明 | M5 | MINOR | 完整性 | A | §3.1 加 5/6 转储为窗口内 CPU179 全部崩溃、无排除 |
| G13 | kunpeng.md 路径更正 + 分支名一致 | — | MINOR | 路径 | A | MICROARCH_SUPPLEMENT `docs/kunpeng.md`→`docs/cpu/kunpeng.md`；前言分支名 vs §5.4 一致 |
| G14 | MICROARCH_SUPPLEMENT §2.2 stale 同步（rol6 Hamming-0 → ror6 Hamming-6 + 撤 2⁻⁵⁸） | F8(沿用) | MAJOR | 内部一致 | A | §2.2 全段对齐 paper §3.2；删 2⁻⁵⁸；标注 slot[0] 非 [1] |
| G15 | 0814 "仅差 1 字节"→"6 bit 跨 2 字节" | DA隐含 | MINOR | 精度 | A | DIAGNOSIS §3.2 row1："仅差 1 字节"→"6 比特跨 2 字节" |

不可当前闭合（诚实保留 future work）：H6 guest-visible oops（O3 架构限制）/ 跨案例迁移（需第二台故障机）/ H7 严格同路径（需 non-cascading 模型）/ stale_line_replay 实现后补独立证伪。


## 评审可信度声明（NOT_CALIBRATED）

本二轮为五席 full-mode，未经校准。所有技术断言已对照真实源码、ARM ARM（DDI 0487）与本机 Python 实算；争议项（30-vs-26、slot0-vs-slot1、ESR bit6）经独立计算裁决并明示结论依据。未实跑的（FS 长跑、0814 原 vmcore 复算）均标注为"基于论文/诊断给定数值的独立复算"。受限于单次评审、单模型、未跨模型验证。

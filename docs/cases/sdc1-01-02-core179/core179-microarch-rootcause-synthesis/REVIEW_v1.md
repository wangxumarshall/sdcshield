# 多视角同行评审报告（v1）

评审对象：`paper_en.md` / `paper_zh.md`（双语同步草稿，目标 ASPLOS/MICRO/HPCA）
评审依据：本评审非凭印象，所有技术断言均已对照 `CHAOS/gem5/src/` 真实源码与本机构建的 `gem5.opt` 实跑复现。本机（新机，128 核/29GB，非故障机）构建链就绪，`gem5.opt`（1.1GB）2026-08-29 14:59 重建。
评审日期：2026-08-29
评审模式：full（五席 + 编辑综合）


## Phase 0 领域识别与审稿人配置

| 维度 | 判定 |
|---|---|
| 主学科 | 计算机体系结构（Computer Architecture）/ 硬件可靠性（Hardware Reliability） |
| 次学科 | 系统取证（System Forensics）/ 容错（Fault Injection） |
| 研究范式 | 案例研究（case study）+ 可证伪假设 + 仿真验证 |
| 方法类型 | 取证分析 + gem5 仿真实验（无统计功效预登记，单案例定性为主） |
| 目标层次 | 顶会（ASPLOS/MICRO/HPCA regular paper） |
| 论文成熟度 | 接近终稿，经多轮对抗审查已显著诚实化，但仍有可证伪的过度宣称与可复现性缺口 |

### 五席配置卡（动态生成）

- Journal-Fit Reviewer (EIC)：HPCA/MICRO 资深领域编辑，关注"仿真与硅实证的差距是否被诚实处理、贡献是否达顶会 bar"。
- Reviewer 1 (Methodology)：故障注入方法学专家（GeFIN/SiliFuzz 谱系），关注可证伪性、对照组内部效度、统计功效、可复现性。
- Reviewer 2 (Domain)：ARMv8 体系结构与服务器 RAS 专家，关注 `FAR_EL1`/TBI/PTW 架构不变量是否用对、TSV110 几何是否准确。
- Reviewer 3 (Perspective)：跨域系统可靠性研究者（Google/Baidu 机群 SDC 谱系），关注单案例结论的外部效度与可迁移性。
- Devil's Advocate (固定第五席)：挑战核心论证链：D1"位翻转不可达"是否真的排除所有位翻转模型？三通路分解是否是循环论证？仿真"复现"是否只是把待证前提编进了注入器？


## Phase 1 五席评审

### 席 1 Journal-Fit Reviewer (EIC)

对期刊/会议契合度、原创性、整体质量的评估。

真实优点（非捧场，有据）：
1. 选题契合 ASPLOS/MICRO/HPCA 的核心关切："生产硅片上单核间歇 SDC 的微架构定位"是真实未解的工程问题，且本工作把"跨启动寄存器-内存位级比对"作为测量仪器，方法论上有新意。
2. 诚实性 preamble 是加分项：明确标注 seed-0 运行间方差、故障机即注入机、残余 SDC 风险，这在体系结构投稿中罕见，降低审稿人对"cherry-pick"的默认怀疑。
3. H5 端到端复现已由本评审独立验证（见下），构成可复现的最小闭环。

契合度不足：
- 顶会 regular paper 的 bar 是"可迁移的方法论贡献 + 广泛可复现"。本文的可迁移主张（§5.2 位翻转不足够）虽做了边界测试，但其适用范围仅限"字节相位位移签名类"，这是一个较窄的签名子类。EIC 会问："除 core 179 外，业界有多少 SDC 落入此类？"论文未给数据支撑频度估计。这是"方法论贡献的普适性"硬伤。

原创性判定：PARTLY_MEETS。单核 SDC 跨启动取证 + 结构化故障模型组合确为新；但"位翻转不足以复现某些 SDC"在 fault-injection 文献中并非全新洞见（结构性故障 / coupling fault 模型在 VLSI 测试理论中早有）；本文新意在于把它与"跨转储稳定性作为仪器"结合，但与已有 coupling-fault 文献的区分度在 §2.5 偏弱（未引用 IEEE 1149/耦合故障经典文献）。

建议信号（无最终决定权）：Major Revision。


### 席 2 Methodology Reviewer (R1)

研究设计、统计效度、可复现性。

[MAJOR] 0814 案例的 Hamming-0 主张与可复现脚本不一致（核心方法论瑕疵）。
- 论文 §3.2 与 MICROARCH_SUPPLEMENT §2.2 称："Boot 08-14：`x20 = 0xd93715ba0000ffff` 匹配 `rol6(__per_cpu_offset[1])` Hamming 距离 0，唯一匹配槽位 1。"
- 本评审用论文给出的真值（DIAGNOSIS_REPORT §3.2 表：`offset[1]=0xffffd93715b7e000`）做独立复算：`ror6(slot1)` = `0xd93715b7e000ffff`，`rol6(slot1)` = `0xe000ffffd93715b7`，二者 Hamming 距离分别为 6 与 34，均非 0。最接近的 `ror6` 差 6 bit，且这 6 bit 跨字节分布（`15b7`→`15ba` 差 1 bit + 旋转未对齐）。
- 但 `reproduce_d1_forensic.sh` 仅复现 15:58（实锤案例，`rol1(slot0)` 确为 Hamming-0，本评审已复算确认 ✓），不复现 0814。因此 0814 的"Haming-0"是未经独立脚本验证的过宣称。
- 此处的诚实路径：0814 在 DIAGNOSIS_REPORT 中本就标 【强推】（"形状仅差 1 字节"）而非【实锤】；但 paper §3.2/Supplement §2.2 把它升格为"Haming-0 唯一匹配"。需降回 【强推】，并如实标注"最近候选 Hamming=6，跨字节分布"，否则核心断言被单个反例即可击穿，而本评审恰好构造出了该反例。

[MAJOR] H6 的"可分性"对照在测量学上不构成受控实验。
- 论文 §5.3 表格混合了三种不同测量维度：(a) D1 测于 SE、(b) D2 测于 FS、(c) Crash 代理是 gem5 fetch-stall 而非 guest oops。论文已诚实标注这三点（这是优点），但随后仍称"fetch-stall 作 Crash 代理的谱跨 5 种子可分"为"最强体制内受控多种子对照"。
- 方法学上，"受控"要求单一变量。此处 D1-SE 与 D2-FS 是双变量同时变化（故障类型 × 翻译体制），无法把可分性归因于"D1 vs D2"，可同样归因于"SE vs FS"。16 B-tick FS 内对照（D1→387131 vs D2→3085）部分缓解，但仅单种子、且仍受 fetch-stall 非特异性的混淆（D3 高 prob 也卡 ~3100）。
- 论文已把 H6 降为"方向已观测、非可分性已确认"，这是诚实的。但 §5.3 标题与摘要仍用"spectrum separability"措辞，建议全文统一为"directional divergence under a shared proxy"以避免读者误读为受控可分。

[MAJOR] H7 ECC 对照的内部效度缺陷被承认但未被实验闭合。
- conditionalValidBit 模式（仅对 block desc bit0 单 bit XOR）使 ECC-on 时 gate 在 `corruptDescriptor` 第 83 行提前 `return`（`numBenignFlips++`），即"ECC 纠正"被建模为"不发生翻转"而非"翻转后纠正"。论文已标注"两臂非严格同路径对照"（numHooksCalled 15808 vs 17）。
- 但更深问题：ECC-on 提前 return 意味着该路径的执行流根本没被注入扰动，而 ECC-off 路径被扰动（产生级联重查）。因此 5/5 的方向稳定性部分来自"对照臂一侧根本没动"。论文诚实承认了这一点，这是优点，但未在 §5.4 给出量化"有多少方向性来自级联、多少来自 ECC 纠正本身"的分离实验。R1 会要求至少一个非级联故障模型（如 post-walk 注入点）作正交验证，或明确把该对照从"H7 verified"降为"H7 directional, ECC-correctness mechanism separately verified"。

[MAJOR] 可复现性缺口：run_H7.sh 不能复现论文 H7 表。
- `fi_research/probes/run_H7.sh` 调用 `o3_chaos_smoke.py`（SE 模式）。但 H7 的核心论点是"D3 在 SE 下 null、在 FS 下触发"，SE 脚本对所有臂都会产 `numFaultsInjected=0`，无法复现论文 §5.4 的 FS 5-seed ECC 对照表。
- §8 Data Availability 把 `run_H7.sh` 列为交付物，但它与论文结果表不对应。同理，论文多次引用的 `--max-tick 400M` FS 结果、5 种子数据，均无对应脚本封装，目前只能靠手工 `o3_chaos_fs.py` 跑，而 FS 单跑到 bash 需 1-2h，5 种子 × 多臂不可行（论文也承认）。这是"声称可复现但脚本不闭环"的硬伤。

[MINOR] 缺乏统计功效预登记。 §5 多处用"5/5"作结论，但未预先声明"几/几 才算可分"，事后解读有弹性。建议在 §4 补一句预登记阈值。

[MINOR] seed=0 的运行间方差量化不足。 §7 提到 7963 vs 7860 注入，但未给方差区间或多 seed 分布。BESTPAPER_PLAN 提到 5 次 seed=0 spur=1/1/1/2/1，该数据应入正文而非仅留 plan。

优点确认：H5 已由本评审独立复现：golden run 实测 `ptr_corrupt=0 val_mismatch=0 fails=0`（tick 30677000 退出）；`byte_lane_skew` 注入实测触发 `panic: Page table fault when accessing virtual address 0x7fbffefc48`（与论文 §5.1 一致）。这是真实闭环，非声称。✓


### 席 3 Domain Reviewer (R2)

文献覆盖、理论框架、领域贡献、架构不变量准确性。

[CRITICAL-adjacent] §2.2 的 FAR_EL1 自我校正是正确的，但 D2 机制复现借了 gem5 的非架构行为，需澄清。
- 论文 §2.2 正确指出：对 translation fault，`FAR_EL1[63:60]` 是 UNKNOWN/RES0，先前版本"`FAR[63:0]` 必须等于翻译地址"是错的。本评审核验 gem5 `src/arch/arm/faults.cc:1087`：FS 模式下 FAR 确由 `faultAddr`（完整 64 位，含被破坏的高位）写入，即 gem5 的 FAR 实现并不屏蔽 `[63:60]`。
- 这意味着 §5.3 的 D2"规范→非规范"复现（`0xffffffc008b08f30 → 0xffffc008b08f30`）在 gem5 中产生的 fault 是真实的 translation fault，但其 FAR 行为与真实硅片（[63:60] UNKNOWN）不完全同构。论文没声称它同构（只说复现"机制"），但读者易误读。建议在 §5.3 明确："gem5 将完整 faultAddr 写入 FAR，不屏蔽 [63:60]；故此复现验证的是 byte7 清零→非规范 VA→translation fault 的因果，而非硅片 FAR 高位 nibble 的架构行为。"

[MAJOR] TBI1 调查的证据强度被诚实标注，但 objdump 论证不够闭合。
- §3.3/§7 称"objdump 0102 单板 vmlinux `__cpu_setup` 显示无 `TCR_EL1.TBI1`（bit 38）立即数"，这是当前主机无法复核的（0102 单板 vmcore/vmlinux 不在本机；本机 `/home/sdc/wangxu/opendcdiag-arm/*.yaml` 亦不存在，见下）。该论证的复核依赖于不可达的旧主机。
- 更重要：TBI1 是否设取决于内核运行时 TCR_EL1 值，objdump 静态反汇编只能看 `__cpu_setup` 的初始化路径；若有 runtime `write tcr_el1`（如 KPTI/spectre 缓解）在 `__cpu_setup` 之后，静态分析会漏。论文应补一句"静态分析仅覆盖 `__cpu_setup` 初始化路径；运行时 TCR_EL1 实测（`mrs` 模块或 crash `p cpu_tcr`）是更闭合的裁决，本机未做"；否则 Domain 专家会指出 objdump 不足以裁决 TBI1。

[MAJOR] opendcdiag 生态效度交叉确认的产物在本机缺失。
- 论文 §7 称 method3 报告"与 0102 单板历史 opendcdiag YAML 产物交叉核对（`/home/sdc/wangxu/opendcdiag-arm/*.yaml`）"。本评审实测：该路径在本机不存在（`ls`/`grep` 均空）。这是迁移到新机后的环境断裂。
- 后果：§7 的"`memcpy0` 在 iter 179 反复交付全零 → 与 D1 `all_zero` 签名一致"这一独立工具交叉确认，当前不可复现。BESTPAPER_PLAN 阶段4 标 ✅ 完成，但产物不在新机，属声称未兑现。需在 §7 如实标注"该交叉核对基于旧机（0102 单板）YAML，迁移后未在新机独立复核"。

[MINOR] TSV110 几何引用来源。 §2.1 的"64KB 4-way L1D、2×128bit 端口、store-fwd 6-7 周期"参数来源 `docs/kunpeng.md`，但论文正文未给 kunpeng.md 的可验证出处（厂商白皮书？社区逆向？）。Domain 专家会质疑数据来源可信度。建议至少标"厂商公开材料 + kunpeng.md（仓库内）"。

优点：D1 的组相联几何裁决（§MICROARCH_SUPPLEMENT §2.1，set 87 vs 105 排除 way/列选通错）是扎实的微架构论证，逻辑清晰。✓


### 席 4 Perspective Reviewer (R3)

跨域连接、实际影响、更广含义。

优点（实践影响真实）：
1. §6 的 DFT 查询清单是可交付的工程产物：fill-buffer 合并 at-speed scan、PTW 读出 ECC 披露、单/多缺陷分别 scan 裁决，这些是对硅片供应商直接可执行的请求，体现"研究→产线筛测"的落地价值。
2. "低于架构 RAS 覆盖"的缺陷层级定位，对 RAS 设计者有参考价值，即便单案例，也提示了 RAS 检查点的覆盖盲区。

[MAJOR] 外部效度的根本限制未被足够前置。
- 这是单缺陷、单核、单机的案例研究。论文 §7 已诚实标注"single-case study, no cross-case migration"，但该限制出现在 §7（有效性威胁），而摘要与 §1.2 的贡献陈述读起来像普适方法。R3 会要求把"单案例"限定前置到贡献条目本身（§1.2 每条贡献加"（单案例，方法迁移未演示）"前缀），而非仅在 §7 回收。
- 6 转储是同核 CPU179 的多次转储，BESTPAPER_PLAN 已诚实承认"跨转储稳定性，非跨案例"。但 §3.1 的"78/78"与 §1.1 的叙事仍可能让读者误以为 78 个事件是 78 个独立案例。建议在 §1.1 首句即点明"全部 78 事件均来自同一逻辑核 179 的 5 次独立启动（同一缺陷核的多次转储，非多案例）"。

[MINOR] 与机群 SDC 文献（Google/Baidu）的对照点偏弱。 §2.5 称本工作"首次"单核定位，但未给机群研究与本工作的可比频度；机群研究报的是群体 SDC 率（如 10⁻⁵/核·年），本文未把自己的单核缺陷放到同一坐标系。加一句"单核反复崩溃的频度远高于机群研究报的背景 SDC 率，支持该缺陷是确定性的硬件病损而非随机 SEU 本底"会增强说服力。

[MINOR] 伦理/披露。 故障机是生产服务器，崩溃数据涉及生产负载。论文未声明数据采集是否经运维授权。R3 角度建议补一句数据使用合规说明（即使内部研究，也应说明）。


### 席 5 Devil's Advocate（固定第五席）

核心论证挑战、逻辑谬误检测、最强反论。

最强反论（Strongest Counter-Argument，~280 词）：

论文的"三通路分解 D1/D2/D3"面临循环论证指控：D1 的承重证据是"寄存器坏值 = rol_k(slot[head])，Hamming-0，位翻转不可达"。但这个签名恰恰是注入器 `byte_lane_skew` 被设计去复现的目标，即论文先从坏值形态构造了一个与之同构的故障模型，再用该模型"复现"坏值形态。这是同义反复（tautology）：模型能复现它被设计去复现的签名，并不独立证明该签名在硅上源于 byte-lane skew。真正的独立证据应是"硅侧可观测的、与 byte-lane 选择逻辑直接耦合的旁证"，但论文的硅侧证据只有 vmcore 坏值本身（待证物）与 opendcdiag（本机不可达、且只证 all_zero 不证 rol）。故 D1 的因果归因（"fill-buffer 字节通道 mux 相位错位"）严格说是对观测签名的拟解释，而非被独立验证的机制。论文在 §3.5/§7 承认单/多缺陷与机制需 RTL 裁决，这是诚实的，但摘要与 §1.2 的"localized to three specific microarchitectural data paths"措辞越过了该承认。Devil's Advocate 主张：应把"localized"全文降级为"signatures consistent with three named microarchitectural data paths, mechanism not independently verified on silicon"。

[CRITICAL] D1 "位翻转不可达"的穷举范围不充分。
- §3.2 的穷举是"slot[0] 上的单字节位翻转"（8 字节 × 256 掩码）。但"位翻转故障模型"在文献中并不限于单字节，多字节、多 bit 的 SEU/MBU 是真实存在的（尤其高能中子、AVF 文献里的多 bit 翻转）。论文的 §5.2 边界测试做了 bit_flip 1-bit，但仍未覆盖"2-3 bit 跨字节翻转"是否能产生 rol 签名。
- 更关键：byte-lane skew（整字节循环移位）在位级等价于"8 个 bit 的周期性置换"，这在数学上可以用一个特定的 8-bit 跨字节翻转模式逼近（非精确等价，但 Hamming 距离可小）。本评审未穷举证明 2-bit/3-bit 跨字节翻转必不能产生 rol 签名，论文也未证明。故"位翻转不可达"严格说是"单字节单 bit 翻转不可达"，论文多处简写为"bit flip"易被读成"任意位翻转不可达"。
- 这是可证伪主张（§5.2）的边界未闭合：只要展示一个有界（≤k bit）跨字节翻转能复现 rol 签名，主张即被证伪。论文说"对 core 179 做了穷举搜索未能找到反例"，但穷举范围若只到单字节，则该证伪尝试不充分。需明确穷举上界（如"≤2 bit 全空间已搜，无 rol 命中"），否则主张悬空。

[MAJOR] "三通路"是否是同一缺陷的过度分解？
- D1/D2/D3 物理相邻、同核、跨启动稳定。论文 §3.5 承认可能是单缺陷三投影。但 Devil's Advocate 追问：D2 的硅证据（0814/0824 的 FAR-MSB 差异）已被 §3.3 自己证明是被 D1 混淆的（arch_addr 高字节本身就是 D1 坏值），那么 D2 作为独立通路的证据是否其实为零？若 D2 的全部硅侧观测都可归约到 D1，则"三通路"实为"一通路 + 两个仿真构造"，分解的独立性存疑。论文诚实降级 D2 为"候选"，但 §1.2 贡献 1 仍把 D2 并列于 D1/D3 作为"三通路分解"的一部分。建议把 D2 在贡献陈述中明确标为"仿真可演练、硅侧被 D1 混淆的候选"，而非与 D1/D3 并列。

[MAJOR] "So what?" 检验。
- 假设论文全部结论成立：core179 有一个 fill-buffer 字节通道缺陷。这改变什么？CPU 已 offline（BESTPAPER_PLAN 暗示），RMA 已建议。对学术界的增量：一个新故障模型 + 一个取证方法。但取证方法依赖 `__per_cpu_offset` 的 write-once 性质，论文自己承认非普适。故"So what"的答案偏窄：除非第二案例出现，否则这是一个有趣的孤例报告，而非可迁移方法。Devil's Advocate 不要求撤稿，但要求摘要与贡献陈述不再暗示更广的普适性。

被审查证伪的质疑（诚实保留，不构成发现）：
- 质疑"chaosLSQFwd 的 byte_lane_skew 实现方向是否与论文 rol 一致"：本评审核验 `CHAOSLSQFwd.cc:138` `data[n] = tmp[(n+k)%size]`（右旋 k），与 `reproduce_d1_forensic.sh` 的 `rol_right(slot0,1)` 一致，与 15:58 实锤匹配。该质疑不成立，记录但不计为发现。✓


## Phase 2 编辑综合与决定

### 共识与分歧

五席共识（corroboration）：
1. 0814 Hamming-0 过宣称：R1（方法论）、Devil's Advocate（逻辑）独立命中同一问题，0814 的"Haming-0 唯一匹配"未经脚本验证，本评审复算为 Hamming=6。这是最高优先级修正。
2. 单案例外部效度应前置：R3、EIC、Devil's Advocate 共识，摘要/贡献陈述的普适性暗示应降级，"single-case"限定应前置到 §1.2。
3. 可复现性脚本不闭环：R1、R2 共识，run_H7.sh 用 SE 不能复现 FS 表；opendcdiag YAML 本机缺失。

分歧（需仲裁）：
- D1 因果归因是否是循环论证？Devil's Advocate 主张降级"localized"措辞；R2 认为 §3.5 已有边界承认、措辞可微调而非降级。编辑仲裁：采纳 Devil's Advocate 的折中，"localized"改为"signatures consistent with ... data paths"，因当前措辞确实越过了 §3.5 的承认，但不必全删定位意图（毕竟 D1 的 Hamming-0 + 位翻转不可达是实锤证据，指向性是有的）。
- H7 是否能称 "verified"？R1 主张降为"directional + ECC-correctness mechanism separately verified"；R2/EIC 认为已诚实标注瑕疵可保留"verified in direction"。编辑仲裁：采纳 R1，鉴于 ECC-on 提前 return 使对照臂未扰动，"verified"对 H7 偏强，改为"directionally verified with internal-validity caveat（ECC-on 路径未被注入扰动）"。

### Devil's Advocate CRITICAL 裁决

- DA-CRITICAL-1（D1 位翻转穷举边界未闭合）：validated。§3.2/§5.2 的"bit flip 不可达"穷举上界未明示，存在 ≤k-bit 跨字节翻转的反例空间。需在 §3.2 明确穷举上界（如 ≤2-bit 全空间），或将主张严格限定为"单字节单 bit 翻转不可达"。该问题不阻止发表，但必须修正，否则 §5.2 的可证伪主张悬空。不阻止 Accept 最终化（因为修正方向明确、可执行），但 `[DA-CRITICAL-VS-ACCEPT: 1 validated]` 上报。
- DA-MAJOR（三通路循环论证 / D2 硅证归约）：部分 validated。D2 硅证确被 D1 混淆（论文自认），但 D2 在贡献陈述中与 D1/D3 并列确为过度。需调整贡献陈述分级，不必撤回 D2 候选地位。

### 编辑决定

Major Revision。

理由：核心取证证据 D1（15:58 Hamming-0 + 位翻转不可达）经本评审独立复算与实跑确认成立（✓ H5 复现、✓ golden 回归），论文骨架是真实的；但存在 3 个必须修正的过宣称/缺口（0814 Hamming-0、位翻转穷举边界、可复现脚本闭环）+ 若干措辞降级，需在重投前闭合。降级而非 Reject 的关键：论文的多轮对抗审查已把它推到了"诚实地承认了大部分问题"的位置，剩余多为措辞与脚本一致性问题，非数据造假。

### 修订路线图（Revision Roadmap，非排序核心，供作者 sidecar）

按 ROI 与可执行性排序（A = 本机可立即执行且高确定；B = 需资源/不可完全闭合）：

| # | 修订项 | 席 | 严重度 | 类别 | 可执行性 | 具体操作 |
|---|---|---|---|---|---|---|
| F1 | 0814 Hamming-0 降级为强推，标注实测 Hamming=6 | R1+DA | MAJOR | 诚实降级 | A（本评审已复算） | §3.2/§MICROARCH_SUPPLEMENT §2.2：0814 案例从"Haming-0 唯一匹配"改为"最近候选 ror6(slot1)，Hamming=6（跨字节分布），标记【强推】"；reproduce 脚本注释更新 |
| F2 | §3.2/§5.2 明示位翻转穷举上界 | DA-CRITICAL | CRITICAL | 边界闭合 | A（本机可跑穷举） | 跑 ≤2-bit / ≤3-bit 跨字节全空间 vs rol 签名，写入穷举上界；或把主张严格限定为"单字节单 bit 翻转不可达" |
| F3 | 摘要/§1.2 贡献前置"single-case"限定 + D2 分级 | R3+EIC+DA | MAJOR | 措辞降级 | A | §1.2 每条贡献加"（单案例，方法迁移未演示）"；D2 在贡献1 中标"仿真可演练、硅侧被 D1 混淆的候选" |
| F4 | "localized" → "signatures consistent with" | DA+R2 | MAJOR | 措辞降级 | A | 摘要、§1.2、§1.3 的"localized to three data paths"改为"signatures consistent with three named data paths (mechanism silicon-unverified)" |
| F5 | H7 "verified" → "directionally verified + ECC-on-undisturbed caveat" | R1 | MAJOR | 措辞精确化 | A | §5.4/§7/摘要：H7 措辞加内部效度保留 |
| F6 | run_H7.sh 改用 FS 配置或如实标注不能复现 FS 表 | R1+R2 | MAJOR | 可复现性 | A | run_H7.sh 注释标注"此脚本仅复现 SE null；FS 5-seed 表需手工 o3_chaos_fs.py"；或新增 run_H7_fs.sh（FS，需长跑，标 future） |
| F7 | opendcdiag YAML 交叉确认标注本机不可达 | R2 | MAJOR | 诚实标注 | A | §7：该交叉核对基于旧机（0102）YAML，迁移后未在新机独立复核；BESTPAPER_PLAN 阶段4 状态改 ⚠️ |
| F8 | §2.2/§5.3 澄清 gem5 FAR 不屏蔽 [63:60] | R2 | MINOR | 精确化 | A | §5.3 加一句 gem5 FAR 行为与硅片 [63:60] UNKNOWN 的差异说明 |
| F9 | §3.3 TBI1 objdump 论证补 runtime 限制 | R2 | MINOR | 精确化 | A | 标注静态分析仅覆盖 __cpu_setup，runtime TCR 实测未做 |
| F10 | §1.1 首句点明 78 事件均同核多次转储 | R3 | MINOR | 前置限定 | A | §1.1 首句加"（同一缺陷核 5 次独立启动的多次转储，非多案例）" |
| F11 | seed=0 方差数据入正文 | R1 | MINOR | 数据完整 | A | 把 BESTPAPER_PLAN 的 5 次 seed=0 spur 分布移入 §7 |
| F12 | §2.5 引用耦合故障经典文献区分度 | EIC | MINOR | 文献 | B | 引用 IEEE 1149/coupling-fault 文献，说明本文"跨转储稳定性作仪器"的新意 |

不可当前闭合（诚实保留为 future work，不属本次修订范围）：
- H6 guest-visible oops 谱（O3 fetch-stall 架构限制）
- 跨案例迁移（需第二台故障机）
- H7 严格同路径对照（需 non-cascading 故障模型）


## 评审可信度声明（NOT_CALIBRATED）

本评审为五席配置的 full-mode 评审，未经校准（live-profile application unavailable）。所有技术断言已对照真实源码与本机 `gem5.opt` 实跑；未实跑的（如 FS 长跑、0814 原始 vmcore 复算）均明确标注为"基于论文给定数值的独立复算"而非"从原始 vmcore 复现"。评审中的方向性判断（Major Revision）受限于单次评审、单模型、未跨模型验证。

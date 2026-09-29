# [20] CHAOS: Controlled Hardware fAult injectOr System for gem5(arXiv 预印本,2026-02-02;13 页文本层,12 页正文)

- PDF: `Chaos Controlled Hardware Fault Injector System for Gem5.pdf`;页数 13(实读全部:p.1–12,末页空白;**arXiv:2602.02119v1 [cs.AR] 2 Feb 2026——预印本,无会议卷号/DOI**,如实记录)
- 作者(全列):Elio Vinciguerra、Enrico Russo、Giuseppe Ascia、Maurizio Palesi(Senior Member, IEEE;共 4 人)
- 机构(学术/企业分列):
  - 学术:卡塔尼亚大学电气、电子与计算机工程系 ×4(意大利卡塔尼亚;vinciguerra 为博士生 @phd.unict.it)
  - 企业:无——**纯学术团队(卡塔尼亚 Ascia/Palesi 组)**
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):无
  - 二级(致谢章节资助/数据/设备):致谢(p.12)——意大利高性能计算/大数据/量子计算研究中心 ICSC Spoke 1 "FutureHPC & BigData"(MUR "Missione 4" Next Generation EU 资助)+ MUR PRIN COLTRANE-V(E53D23008060006);全部意大利公共资助,未列企业方
  - 三级(首页脚注资助声明):无(arXiv 预印本)
  - 间接层:GitHub 开源仓库 [12](elioviguerra/CHAOS);RISC-V ISA 手册 [14][15](UC Berkeley);工具对比引用 gem5-Approxilyzer(UIUC Adve 组,DSN'19)
- 核心结论(各带节号/图表号):
  1. **定位:gem5 注入工具的"老旧化危机"回应(§I/§II.C)**:现有 gem5 FI 框架"过时、不支持新版 gem5、功能受限或闭源"——FIMSIM [7](legacy M5,不兼容现代版)、GeFIN [8](大规模战役+瞬态/永久/间歇+详细结果分类)、GemFI [9](功能+周期精确+快速 checkpoint,但基于旧版 gem5、缺高级故障效果分析)、gem5-Approxilyzer [10](高效瞬态位级+输出质量分类,不支持并发故障/特殊寄存器)、gem5-MARVEL [11](异构 SoC+全 ISA+脆弱性分析,**闭源**);**CHAOS 自称唯一"开源+兼容现代 gem5(20+)+模块化+全面注入支持"组合**;[8]=本集 [21](差分注入 IISWC'15,其工具名 GeFIN)、[9]=本集 [19](GemFI)、[11]=本集 [22](gem5-MARVEL)
  2. **故障与结果分类(§II.A)**:瞬态(辐射/电气噪声)/间歇(焊点松动等温度电压条件性)/永久(晶体管烧毁);结果五类:**Crash / DUE / SDC / Masked / Timeout**
  3. **三模块设计(§III,Algorithm 1–3)**:CHAOReg(架构寄存器)/CHAOSCache(cache)/CHAOSMem(主存)——统一故障类型(bit flip=瞬态;stuck-at-0/1=永久,存专用数据结构周期性重施加);统一参数语义:probability(0–1 每周期激活概率)、start/end(周期区间)、fault type、mask(位掩码,0=随机生成)、faulty bits、PC_target(定位到具体 PC)、cache(目标实例)、corruption size(损坏字节数)、target start/end(地址范围)
  4. **开销(§IV.B)**:每注入故障相对无故障仿真 **0.0004%(Reg)/0.0008%(Cache)/0.0004%(Mem)**;永久故障逐周期监控开销 **6.6/6.1/8.1 ×10⁻⁶%**;开销仅在故障实际注入时产生(与配置概率不成比例)——1M 时钟周期实验,最坏情形测法(运行时随机生成掩码与故障类型)
  5. **实验设置(§IV.A,Table I)**:gem5,1 GHz,DDR3 512 MiB,O3 CPU;L1I 16 KiB/L1D 64 KiB/L2 256 KiB;**RISC-V**;8 个 MiBench 基准(Bitcount/Blowfish/Dijkstra/JPEG/Patricia/Qsort/SHA/Susan);Table I 全特征化:周期 125M–364M、CPI 0.31(Bitcount)–1.19(Patricia)、分支误预测 0.74%–11.68%、L1D miss 0.01%–5%(Dijkstra)、L2 miss 0.59%(Patricia)–98.23%(Susan)
  6. **统计分层(§IV.C,Leveugle 法 [17])**:Low(e=5%,95% CI)→ **384** 故障;Medium(e=5%,99% CI)→ **663**;High(e=1%,99% CI)→ **16,587**;crash 与 DUE 合并统计;单比特战役中 Timeout 零出现
  7. **崩溃结果(§IV.C.1,Fig.1–3)**:寄存器崩溃率随注入概率正相关——High 下 jpeg/qsort/bitcount 崩溃占绝对多数;Low 下系统更可能掩蔽或 SDC(稀疏寄存器故障较少命中存活的关键值);**L1I 敏感性最高——不论概率崩溃率恒高**(指令 opcode/操作数损坏→非法操作/段错误);L1D/L2 崩溃亦很高(Low 概率下略降,让位于 SDC/Masked);**主存崩溃率全场景可忽略**(容量巨大→故障统计上难命中关键段;以 SDC/Masked 为主)
  8. **超时结果(§IV.C.2)**:少数派;Bitcount 最高(13% Low/10% Medium);JPEG/Qsort/SHA 少量 1–4%;**Timeout 与注入概率负相关**——仅 Low/Medium 出现,High 下消失(稀疏故障更易微妙改动控制流如循环条件而不使指令流失效;高密度损坏在挂起前就触发硬件异常崩溃)
  9. **HPC 变化分析(§IV.C.4,Table II–VII)**:RISC-V 20 个硬件性能计数器(mcycle/mtime/minstret/mhpmcounter4–31:指令退休/访存/系统/算术/分支/JAL/JALR/乘除/FP/误预测/cache miss/writeback/TLB miss);指标=平均绝对百分比变化(式 1,n=20);**核心洞察:静默故障即使非致命也可引发"引擎盖下"的巨幅扰动——HPC 可作为非侵入 SDC 检测哨兵,补输出校验之不足/之迟**;极值:Qsort L1D 单比特 20,841.92%(High)/**83,211.58%(Medium)**/52,867.90%(Low)(排序指针/数组边界损坏→数百万额外指令的准无限循环,最终正确终止);L1I:Bitcount 7,767%(High)、Dijkstra 2,460%(Low)(PC 被重定向到合法但非预期代码路径);L2:Bitcount 5,285%(High);**主存变化可忽略(0.00%–0.30%,仅 Qsort 6.58% High 例外)——主存故障"双重沉默":不崩且微架构活动几乎无扰动,HPC 检测最难的目标**
  10. **多比特注入(§IV.D,Fig.4–6,Table VIII–XII)**:寄存器/L1 崩溃率压倒性(即使较低概率;Bitcount/Qsort High 下近 100%);主存仍韧性(Masked/SDC 主导——巨大地址空间"稀释"多比特故障,损坏未用数组/填充区概率远大于命中关键控制结构);HPC 变化更极端:Qsort Reg 多比特 **42,912%(Low)**、L1D 多比特 **>81,000%**(表值 81,920.37/81,817.61)、L2 Qsort 23,822%(High)/Blowfish ~96%;主存多比特 <0.02%
  11. **极化规律与结论(§IV.D.2/§V)**:多比特注入使系统行为**极化**——"要么立即杀死应用(寄存器/L1),要么静默藏进内存背景";Qsort 类反例证明**"存活"有时比"崩溃"更昂贵**(病理状态执行数百万额外指令);故障注入产生的后果远比传统二元失败模型复杂;CHAOS 兼作结构化系统级容错分析方法论
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:根因无关(工具论文;故障模型覆盖瞬态 bit-flip+永久 stuck-at;背景泛指辐射/电气噪声/焊点/晶体管烧毁)
  - 故障模式类型:五类结果(Crash/DUE/SDC/Masked/Timeout);**主存故障="双重沉默"模式**(不崩+HPC 无扰动)——HPC 检测的结构性盲区;L1I=必崩模式(指令流损坏);Timeout 与注入概率负相关(稀疏故障改循环条件更阴险);多比特极化模式(立即杀死 vs 静默隐藏)
  - 检测技术类型:**HPC(硬件性能计数器)作为 SDC 非侵入检测代理**——SDC/Masked 故障的 HPC 平均绝对变化量化;输出完好但 HPC 扰动可作异常哨兵——与 R5 批次 [34](稀疏矩阵 PMC)/[35](硬件计数器)同一技术路线的注入端验证,并给出其适用域边界(主存故障失效)
  - 处理技术类型:无(工具论文)
- 业界观点摘录(纯学术工具论文,业界立场观点按模板略;业界联系证据:无企业署名/资助,仅意大利公共资助)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | 注入开销 | 每故障 0.0004%(Reg)/0.0008%(Cache)/0.0004%(Mem);永久故障监控 6.6/6.1/8.1×10⁻⁶%/周期 | §IV.B |
  | 样本量三档 | Low 384(5%/95%CI);Medium 663(5%/99%CI);High 16,587(1%/99%CI) | §IV.C |
  | HPC 变化极值(单比特) | Qsort L1D 83,211.58%(Medium)/20,841.92%(High)/52,867.90%(Low);Bitcount L1I 7,767%(High);Bitcount L2 5,285%(High) | Table IV–VI |
  | HPC 变化极值(多比特) | Qsort Reg 42,912%(Low);Qsort L1D >81,000%;Qsort L2 23,822%(High);Blowfish L2 ~96% | Table VIII–XI |
  | 主存 HPC 变化 | 单比特 0.00–0.30%(Qsort 6.58% 唯一例外);多比特 <0.02% | Table VII/XII |
  | Timeout | Bitcount 13%(Low)/10%(Medium);仅 Low/Medium 出现,High 消失 | §IV.C.2 |
  | 微架构 | RISC-V O3,1GHz,DDR3 512MiB,L1I 16KiB/L1D 64KiB/L2 256KiB | §IV.A |
  | 基准 | 8 MiBench;周期 125M–364M;CPI 0.31–1.19;L2 miss 0.59%–98.23% | §IV.A, Table I |
  | HPC 监控 | 20 个 RISC-V 计数器(mcycle/mtime/minstret/mhpmcounter4–31) | Table II |

- 方法论要点:概率驱动运行时注入(每周期激活概率+周期区间+PC 定位三重时机控制);永久故障专用数据结构+周期性重施加保证持续性;Leveugle 统计分层三档样本量;golden run 对照判 SDC;HPC 平均绝对百分比变化指标(n=20);单比特 vs 随机多比特对照设计;三模块统一参数语义(可组合性);诚实报告:开销仅在故障实际注入时产生、单比特战役 Timeout 零出现、crash/DUE 合并的声明。
- 横向对比注记:**gem5 注入工具谱系 2026 年现状盘点篇**:FIMSIM(2011,legacy M5)→GeFIN(=[8]=本集 [21] IISWC'15)→GemFI(=[9]=本集 [19] DSN'14)→gem5-Approxilyzer(2019 DSN,UIUC Adve 组)→gem5-MARVEL(=[11]=本集 [22] HPCA'24)→CHAOS(2026)——**CHAOS 对 [22] 的"闭源"指控记录在案(待 [22] 精读核对)**;CHAOS 自我定位为工具生态"老旧化危机"(obsolete/兼容性断裂)的社区回应;**HPC 作 SDC 检测哨兵与 R5 批次 [34]/[35](硬件计数器在线检测)构成"注入端验证↔在线检测"闭环**;主存故障"双重沉默"(不崩+HPC 无扰动)直接限定 HPC 检测适用域——对 [34]/[35] 技术路线边界的注入端实证;L1I 必崩 vs 主存必静的位置谱,与 [19](PC→必崩/访存数据→78% 韧性)、[18](指针/索引→Crash 主导)同一规律 2026 再验证;Timeout 与概率负相关是"稀疏故障更阴险"的量化证据;Qsort 病理状态(存活比崩溃更贵:HPC 变化 83,211%)与 [19]"correct 容差内"细分互补——CHAOS 用 HPC 揭示容差内结果的内部巨变;卡塔尼亚组(Ascia/Palesi,嵌入式/NoC 研究重镇)是希腊双中心(雅典/色萨利)之外的新工具提供方;RISC-V 主实验反映 2020s 生态(对比 [19] Alpha 2014/[17] ARM/[18] x86);**arXiv 2026-02 为本集截至当前最新时间戳论文**。
- 身份核实:标题"CHAOS:gem5 的受控硬件故障注入器系统"与内容(三模块注入框架+开销评估+RISC-V 单/多比特战役+HPC 分析)完全相符;arXiv:2602.02119v1 [cs.AR] 2026-02-02、13 页(12 页正文+空白末页)——预印本无会议卷号/DOI,如实记录。**文本层已知损失**:(1) **Fig.1–6 X 轴标签严重乱码**(图例条目名互相叠压成"CHCHAOAOSCCSHaRAceOhg..."文本流)——全部结果取自散文与 Table III–XII;(2) **Table III–XII 行标签(High/Medium/Low)与数值块对齐部分错位**——极值以散文交叉验证为准:散文明言 Qsort L1D 单比特"High >20,000%、Medium 83,211%"与表值 20,841.92/83,211.58 吻合;Qsort Reg 多比特散文"Low 42,912%"但表值 42,912.66 疑位于 Medium 行(行错位,以散文为准);Table IX Qsort 81,920.37/81,817.61(表)vs 散文"Medium and Low 超 81,000%"(High/Medium vs Medium/Low 归属歧义,如实记录);(3) Table I 部分格空缺/错位(L2 miss 行 Dijkstra 70.52/Qsort 70.59/Susan 98.23 按行错位规律重构;L1D miss 行 Blowfish/Dijkstra/Susan 值缺失不引用;SHA 列 L1D 0.01 疑属 Blowfish,存疑);(4) Table II 部分计数器无描述(mhpmcounter8/11/15/28——不臆测);(5) 无企业资助(意大利 ICSC/NGEU/PRIN 公共资助);(6) 无内部矛盾——散文极值与表值吻合(除已记录行错位);384/663/16,587 样本量与置信/误差判据一致。

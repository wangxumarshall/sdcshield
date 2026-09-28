# [32] 检测静默数据损坏源头的策略(SemiEngineering 产业文章,2024-03-12)

> **类型标注:产业媒体综合报道**(非同行评审论文;行业记者基于 ITC 2022/2023 小组讨论与多方访谈撰写的综合分析,含 14 位具名产业/学术声音的直接引语——本集 53 篇中"业界观点"密度最高的一篇,综述"业界观点"章节的一级素材)。

## 标题与出处

- **标题**: Strategies For Detecting Sources Of Silent Data Corruption
- **出处**: SemiEngineering.com(semiconductor engineering 行业媒体),**2024 年 3 月 12 日**,作者 **Anne Meixner**(contributing editor;曾任 IBM/CMU/Intel,30+ 年从业,3 项美国专利,2 次 ITC 最佳论文,2015 年创立 The Engineers' Daughter 咨询——p.10-11 作者简介)。
- **栏目**: Test, Measurement & Analytics。

## PDF 与实读范围

- **文件**: `ref/Strategies For Detecting Sources Of Silent Data Corruption.pdf`
- **页数**: 23 页(网页打印版;**正文 pp.1-8**,p.9 相关文章链接,p.10 参考文献+标签+作者简介起始,p.11 简介结束,pp.12-23 为网站模板——标签云/评论区/页脚,无正文内容)。
- **阅读方式**: PDF 无文本层(打印为图像)——按 Ruling 2 用 PyMuPDF 以 2.5× 渲染全部 23 页 PNG,Windows WinRT OCR 全文转录(56.9KB),pp.1、2、7 三页经视觉精读与 OCR 互证。

## 作者

Anne Meixner(SemiEngineering 特约编辑,个人;前 IBM/CMU/Intel)——**产业媒体作者,非学术论文**。

## 企业合作证据(三级)——本文本身即"产业生态聚合体"

- **一级(署名引语,14 位声音 × 12 家机构,p 页码)**:
  - **AMD**: Sankar Gurumurthy, director of silicon design engineering(p.2, 5——注意:与 [27] ISCA'24 Harpocrates 作者 Sudhanva Gurumurthi(AMD RAS Architecture)**是两个人,姓名相近勿混淆**)
  - **Intel**: David Lerner, senior principal engineer(p.2, 6, 8;引语出自 ITC 2023 panel)
  - **Meta**: Sriram Sankar, director of engineering(p.2, 8);Harish Dattatraya Dixit, principal engineer(p.5)
  - **Google**: Sandeep Bhatia, DFX lead for TPUs(p.4, 7, 8;引语出自 ITC 2023 panel)
  - **PDF Solutions**: John Kibarian, CEO(p.3);Indranil De, e-beam tools GM(p.3)
  - **National Instruments(NI)**: Ron Chaffee, senior director of applications engineering(p.4)
  - **Advantest Europe**: Klaus-Dieter Hilliges, platform extension manager(p.5, 6)
  - **Synopsys**: Adam Cron, distinguished architect(p.6)
  - **Siemens EDA**: Janusz Rajski, VP of engineering for Tessent(p.6, ITC panel)
  - **Teradyne**: Ed Seng, strategic marketing manager for advanced digital(p.6)
  - **proteanTecs**: Noam Brousard, VP of solutions engineering(p.7)
  - **学术(唯一)**: Jennifer Dworak 教授, Southern Methodist University(p.5, ITC panel)
- **二级(致谢/资助)**: 不适用(媒体文章);素材来源为 ITC 2022/2023 小组讨论、PDF Solutions 用户大会、多方访谈。
- **三级(版权页)**: 网页页脚 Copyright ©2013-2026 SMG(Sperling Media Group)。
- **间接层(本文披露的协作关系,综述"生态叙事"的一级证据)**: ① Sankar(Meta):"with iterative partnerships across our vendors, and by sharing valuable field data over time, we've been able to fix a large percentage of the faults due to the test escapes using vendor system-level test (SLT)"(p.2)——**超大规模运营商↔芯片厂商数据回流闭环的运作实证**;② Dixit(Meta):"with collaborative partnerships with Meta, other hyperscalers, and vendors, we have seen improvements in the content selection, testing sequence approaches, and test execution strategy"(p.5);③ 全文引用链覆盖 OCP 规格 [1]/Meta arXiv [2][5]/Google [3]/Intel DCDIAG [4]/ITC 海报 [7]/PEPR [8]。

## 核心结论

1. **问题定调(p.1)**: SDC 根因识别"比修复单一缺陷更宽更复杂";RAS 团队即使有最好工具也无法阻止随机硬件错误;单纯加大测试/检测不保证芯片生命周期后期不出现问题。2021 年 Meta 与 Google 工程师分别公布发现 [2][3],指向**制造测试逃逸**;业界目标:把 SDC 测试逃逸从千级降到 **10 DPPM**。
2. **四情景分类(p.2, [5])**: SDC 发生可归于四情景——①测试逃逸;②边际缺陷(marginal defects,Intel 定义 [6]=Ryan et al. ITC'14);③潜伏缺陷(latent defects,即早期寿命失效);④电路退化(磨损失效)。Fig.1 浴盆曲线(Siemens EDA 供图)——**本集论文中把 SDC 根因映射到经典可靠性浴盆曲线的唯一直接表述**。
3. **Lerner(Intel)根因画像(p.2, ITC 2023 panel)**: "Timing error defects — these escapes are our biggest contributor, and it is a long random tail of failures... We don't see any novel defects. We do observe a bias toward open defects. Latent defects certainly are a factor. Today, they are the minority of in-field returns. As we get better at cleaning up time zero with the test coverage, I'm more concerned with latent defects in the future. I'm not too concerned with aging degradation, because we don't see much of evidence of that."——**时序缺陷是最大贡献者 + 开路缺陷偏置 + 对老化不太担心的三段式产业证词**(与 [28] Vega 老化路线、[31] SDF 路线形成产业-学术张力)。
4. **Sankar(Meta)演化叙事(p.2)**: 初期 SDC 大比例源于测试逃逸+架构选择、小部分源于退化;经与厂商迭代合作+共享现场数据,用厂商 SLT 与集成商测试修复了大部分逃逸;剩余为逃逸+边际性问题+时变失效,"For the latter we can't conclusively say degradation or data dependency."
5. **制造筛查改进(p.3)**: 共识——定向筛查可减少逃逸,超大规模运营商愿为额外检测/测试付费;100% 晶圆检测可嵌入制造流。e-beam voltage contrast 可识别阻性接触/通孔甚至潜伏缺陷,但吞吐代价大;定向区域扫描提速 ≥10×。Kibarian(PDF Solutions): 接触/通孔密度 <10% 晶圆面积,"90% of the wafer surface is not worth rastering";De: 每晶体管至少 3 个接触,大 NVIDIA 芯片 300+ 亿晶体管 → **每 die 1000+ 亿(100+ billion)接触点**;M1-M3 金属层通孔少、更可检;晶体管级吞吐急剧下降。
6. **测试条件有效性排序(p.4)**: 全部根因行为证据指向**影响时序关系的缺陷**;既有结构/功能测试内容通过压迫局部电气环境可减少逃逸。**按价值/有效性降序:电压裕度调整(voltage margining)> 多频率点 > 温度**。Intel 测试工程师在 SLT 上仅需**降低电源轨电压 10mV** 即检出 SDC 可归因逃逸(ITC 2022 海报 [7],Rojas 等,"Improving System Level Screening Efficiency Through Negative Voltage Margining")。
7. **Bhatia(Google)电气敏感化(p.4, ITC 2023)**: 现有故障模型聚焦**逻辑敏感化**(logical sensitization),需引入**电气敏感化**(electrical sensitization);实验室跑功能模式+测量电压传感器→电源网格噪声大、峰值触及过冲/欠冲;"想象一个只在电源网格恰好达到特定电压 level 时才触发的边际缺陷"。筛查需关注故障覆盖率之外的应力条件:电压裕度、冷热温度、开关噪声、电源噪声、信号完整性噪声。
8. **Chaffee(NI)生命周期数据连接(p.4)**: 识别更复杂缺陷画像的关键是**连接产品全生命周期数据**使筛查模型获得预测模式;SDC 的源与影响"convoluted——隐藏在多种变量或多个部件与环境条件的微妙交互中";正确数据集把 SDC 事件连接到该单元的历史数据→测试条件现场↔制造关联→持续自适应改进。
9. **低频角缺陷(p.4-5)**: 部分缺陷仅出现在低频角;DVFS 语境下(Hilliges/Advantest): 100 个功能测试中,可能 #1/#5 对高频关键、#5/#7 对低频关键——不同功能测试对不同电压/频率角敏感,低压特定路径的缺陷只在对应功能测试暴露。
10. **Dixit(Meta)测试投资回报(p.5)**: 评估测试投资回报、按 fleet 反馈持续调整内容;制造测试时间=成本的直接函数且影响良率;"我们终究要在硅生命周期的某个点为这些 SDC 买单";为更大数据依赖与随机化增加测试时间有益;早期厂商侧几乎没有失效数据,如今经 Meta/其他超大规模运营商/厂商协作伙伴关系,内容选择、测试序列、执行策略均已改善。
11. **先进故障模型阶梯(p.5)**: 行为证据=小延迟(边际缺陷 [6]);结构测试可用先进故障模型生成新内容,**按仿真工作量与模式数粗略升序**:stuck-at → transition → path delay → slack-based transition → cell-aware → slack-based cell-aware transition → pseudo-exhaustive physically-aware(PEPR [8])。Dwork(SMU)引 Li C. Wang 1995/1996 ITC 论文 [9][10]: 故障模型与缺陷不匹配时,覆盖率末端"偏向你的模型而非你的缺陷"→故障数爆炸+模式爆炸、浪费资源。Gurumurthy(AMD): 没有单一故障模型是解决方案;先进模型通常优于随机模式,但必须比随机模式更有效、"needs to [target] real bad parts"。
12. **Cron(Synopsys)成本权衡(p.6)**: "For SDCs, time on the tester seems negotiable"(测试时间对 SDC 而言似乎有商量余地——SDC 严重性改变测试经济学);slack-based cell-aware 昂贵(模式数/生成/算力);slack-based transition delay 是折中,SNUG 数十篇论文证明设备在 slack-based 模式下以更低频率失效。
13. **Rajski(Siemens EDA)多角模式生成(p.6)**: 先进 CMOS 节点对工艺变化极敏感;设计界有多模多角范式而测试界拒绝为多角(电压/温度/工艺)生成模式——过去被认为代价过高,如今对成本-质量权衡有了新理解;模式应按真实条件生成——目前厂商只为标称工艺角生成模式却在不同电压施加,换工艺角则时序特性不同、新路径变关键(**与 [31] ETS'25 的 COND 空间主张完全同一命题,产业侧与学术侧呼应**)。
14. **功能测试与噪声环境(p.6)**: Google/Meta RAS 团队通过回溯失败代码段发现 SDC=使命模式(mission-mode)电路激活;可在晶圆/单元级 ATE 或 SLT 上施加。Lerner(Intel): "scan is the foundation of structural tests. Because of the DI/DT of noise contribution, functional test is needed due to defect sensitivity. The goal is to eventually derive scan tests with advanced fault models such that we don't need functional tests. But we won't know that until we're able to measure with functional tests that we're not finding nothing."(终极目标是先进故障模型的 scan 完全取代功能测试,但只有先测到"功能测试已无发现"才能知道达到)。噪声环境增加 SDC 检出概率→利于 SLT 上跑功能内容;Seng(Teradyne): 关键是活动水平(activity level)而非数学等式本身,先进封装与 chiplet 会加剧;Hilliges(Advantest): 需要更系统的定向 bare metal 内容(无 OS 汇编代码)而非仅经 OS 的系统级测试。
15. **在线检测与遥测(p.7)**: 依赖特定代码集触发→单靠制造测试**不具成本效益**(p.7 原文 "100 DPPM",p.1 为 "10 DPPM"——两个数字并存于原文,非 OCR 损失;可解读为"制造测试单独连 100 DPPM 都难保证,10 DPPM 总体目标需在线检测补充",亦可能是原文笔误,双记不作单向裁决)。遥测电路可测电源轨电压跌落、局部温度、微架构事务活动;Brousard(proteanTecs): 环境传感器太宽泛、计数器"只在问题成为问题后才发现";需要**片内电路实际性能监控**——信号从源到目标传播是否满足时序约束,"温度、老化、潜伏缺陷、电压跌落、甚至软件压力升高,最终都会导致信号传播延迟增加,把电路推向时序失败"——失败的前兆(precursors)。
16. **Bhatia(Google)遥测怀疑论(p.7)**: 遥测可帮助排查部分根因,但其检查的芯片类别问题是**随机缺陷**——"缺陷是随机的,可在芯片任何位置,且每颗芯片位置不同",遥测对此无能为力。
17. **"静默→可听"转变(p.7-8)**: RAS/设计工程师共识:需把 silent DC 变为 audible DC——**不是纠错而是检测,使执行能在微秒级停止**;软硬件协同。Bhatia(Google, p.8): "They're doing damage all along until we happen to find it... You convert silent data corruption to loud data corruption. You still have a defect... but you've drastically reduced its impact by exposing it... parity, ECC, chip monitors, compute replay, algorithmically based fault tolerance, and residue checkers... time to look at them and see how we can cost-effectively introduce them into our current designs."
18. **Lerner(Intel)多层金字塔(p.8, Fig.5, Intel/David Lerner 供图)**: "We need a multi-layered approach, a sort of 'all of the above' answer... The bottom of the pyramid is sub-basement, because this is really a test conversation. But it's necessary to talk about architecture here because we need to maximize architectural detection — parity, fabric protection, residue, etc. These are key tradeoffs of performance and cost." 金字塔四层(图示): SBT 工作负载显式检查数据以筛查/验证/测量 → 故障分级的 ATE 功能测试补充 scan(筛查 transition/边际缺陷)→ 高 scan 覆盖+先进故障模型 → 最大化架构级检测(parity/互连保护/残差校验)——**Intel 版检测技术栈总纲,综述对策章节的直接产业框架**。
19. **Sankar(Meta)结语(p.8)**: "Should we aim to screen out? Absolutely... Will we get zero SDCs? Most likely not. But we may get asymptotically to zero with efforts across the stack... we don't think screening alone is a solution. Design, architectural and software solutions are required to mitigate this at scale... we should explore how we can make our applications fault tolerant and how an infrastructure can actually tolerate SDCs. Meta is committed on this part of the journey in the stack. In addition, there is a longer-term horizon effort around improving and incentivizing architectural solutions and design patterns which are inherently SDC resilient."

## 分类学标注

- **根因机理**: 四情景分类(逃逸/边际/潜伏/退化)映射浴盆曲线;Lerner 证词(时序缺陷最大贡献+开路偏置+老化证据少)——**产业侧根因画像与学术侧重构([28] 老化、[31] SDF)存在主张张力,综述根因章节需并列呈现**;随机缺陷空间分布(Bhatia)。
- **故障模式**: 数据依赖性(hyperscaler 共识,难检测主因);低压/低频角激活;DVFS×测试选择交互;电源网格噪声触发的边际缺陷(电气敏感化);噪声环境(活动水平)作为激活条件。
- **检测技术**: 全栈扫描——制造筛查(e-beam voltage contrast/定向检测)、测试条件(电压裕度>频率>温度,10mV 实证)、七级故障模型阶梯、SLT/功能测试/bare metal、遥测(片内路径延迟监控)、架构级检测(parity/ECC/回放/ABFT/残差);Lerner 四层金字塔作为产业总纲。
- **处理技术**: "静默→可听"转化(微秒级停执行);应用级容错与基础设施容忍 SDC(Meta 主张);SDC-resilient 设计模式(长期路线)。
- **生命周期**: 设计→制造→现场全周期;Chaffee"连接全生命周期数据"主张与 Sankar 厂商-运营商数据回流闭环——生命周期数据闭环的产业运作证据。

## 业界观点摘录(本文即业界观点集,以上 19 条均含直接引语;再摘最核心三则)

- "For SDCs, time on the tester seems negotiable."(Adam Cron, Synopsys, p.6——SDC 改变测试经济学的最凝练表述)
- "You convert silent data corruption to loud data corruption. You still have a defect... but you've drastically reduced its impact by exposing it."(Sandeep Bhatia, Google, p.8)
- "Will we get zero SDCs? Most likely not. But we may get asymptotically to zero with efforts across the stack."(Sriram Sankar, Meta, p.8)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 10 DPPM | SDC 测试逃逸目标(p.1);p.7 另见 "100 DPPM"(原文两数并存,双记) | p.1, 7 |
| 10 mV | SLT 上压低电源轨电压即检出 SDC 逃逸(Intel ITC'22 海报) | p.4 |
| 4 | SDC 四情景(逃逸/边际/潜伏/退化),浴盆曲线映射 | p.2, Fig.1 |
| 7 | 故障模型阶梯级数(stuck-at→…→PEPR) | p.5 |
| <10% / 90% | 晶圆上接触/通孔密度占比 / 不值得扫描的表面占比 | p.3 |
| ≥3 / 30+ 亿 / 1000+ 亿 | 每晶体管接触数 / 大 NVIDIA 芯片晶体管数 / 每 die 接触点数 | p.3 |
| ≥10× | 定向 e-beam 检测提速 | p.3 |
| 100 / #1,#5 / #5,#7 | 功能测试总数 / 高频关键测试 / 低频关键测试示例 | p.5 |
| 14 / 12 | 具名引语人数 / 所属机构数(11 企业+1 学术) | 全文 |
| 2024-03-12 | 发表日期 | p.1 |
| 8 / 23 | 正文页数 / PDF 总页数 | 全文 |

## 方法论要点

- **产业媒体综合法**: 记者聚合 ITC panel 现场 + 用户大会 + 访谈,把分散的产业判断组织成叙事——与学术论文互证的方法(本文多数主张可在本集其他论文中找到学术对应)。
- **有效性排序的实证性**: "电压裕度>频率>温度"排序来自 published + anecdotal reports 的综合——非受控实验,但来自多家独立来源的一致性判断。
- **引语的三角化价值**: Lerner(Intel)/Bhatia(Google)/Sankar·Dixit(Meta)三方对同一问题(遥测有用性、老化贡献、筛查极限)的分歧表述——综述呈现产业内部不一致的第一手材料。

## 横向对比注记

- **本集引用链**: [5]=arXiv 2203.08989 即本集 [03] Fleetscanner/Ripple 的 arXiv 版;[2]=Dixit 2102.11245 即 [01] SOSP'23 前身;[3]=Hochschild Google(多文引用);[6] Ryan ITC'14 与 [8] PEPR 同时被 [30] IRPS'25 引用([30] 的 [5][8])——**Intel 引证簇跨论文重合,印证 Lerner 系话语的稳定性**。
- **Lerner 三角**: 本文(2024-03 媒体引语)+ [30] IRPS'25(2025 共同作者)+ [29] ETS'24(2024-05 学术引用其量化)——David Lerner 在本集三类文体(媒体/产业论文/学术综述)中均为 Intel SDE 话语核心。
- **Sankar/Dixit(Meta)连续性**: 本文引语(2024-03)与 [29] ETS'24 共同作者(2024-05)、[01]/[03] 作者身份——Meta RAS 话语四人组(Dixit/Sankar/…)+媒体渠道的完整传播链。
- **Sankar Gurumurthy(AMD,本文)≠ Sudhanva Gurumurthi(AMD,[27] Harpocrates)**: 姓名相近的两位 AMD 工程师,已分别记录,综述中不得混用。
- **proteanTecs 双节点**: 本文 Brousard 引语(片内路径延迟监控/失败前兆)与 [53] proteanTecs 白皮书(R8)直接呼应——R8 阅读时回链,媒体版与厂商版主张对照。
- **Rajski(Siemens EDA)与 [31]**: 本文"多角模式生成"引语与 [31] ETS'25 的 COND 空间命题完全一致——同一 DFT 生命周期管理主张在产业 panel 与学术论文的双重表达;[31] 所引 Rajski D&T'23 与本文引语同源。
- **与 [28] Vega/[31] SDF 的张力**: Lerner"对老化不太担心,没看到多少证据"vs [28] 整篇以 BTI 老化为根因 vs [31] 把 aging 列入 COND 四维——**老化在 SDC 根因中的权重是产业-学术分歧点**,综述需如实并列(Intel fleet 经验 vs 自底向上方法学各自视角)。
- **"静默→可听"谱系**: Bhatia 表述与 [06] ITHICA(线程内指令检查)、[07] Hardware Sentinel(应用级哨兵)的检测哲学同一取向——把不可见故障转为可观测信号;微秒级停止目标对应该谱系的低延迟端。
- **电压维度聚类**: 10mV 负电压裕度筛查(Intel)→ 电气敏感化(Bhatia)→ [51] NAVIgator GPU 电压极限探索(R8)——电压作为 SDC 检测/激发杠杆的跨论文主题。
- **OCP**: [1] 引 OCP server component resilience 规格——与 [26] Micro'25 的 OCP 引用呼应,OCP 作为 SDC 标准化话语平台的又一证据。
- **ATE/EDA 产业链入场**: Advantest/Teradyne/NI(设备)+ Synopsys/Siemens EDA(工具)在 SDC 议题的集体发声——SDC 从 hyperscaler 问题扩展为全产业链议题的标志文本(2024-03)。

## 身份核实

- **出处核实**: 每页页眉含打印时间戳(9/28/26, 5:26 AM)与文章标题,页脚含规范 URL `semiengineering.com/strategies-for-detecting-sources-of-silent-data-corruption/ N/23`——网页打印版,出处确凿;作者/日期(p.1 "MARCH 12TH, 2024 - BY: ANNE MEIXNER")与作者简介(p.10-11,IBM/CMU/Intel 背景)一致。
- **页数与实读**: 23 页;正文 pp.1-8 全读 + p.9-10(相关文章/参考文献/作者简介)全读 + pp.11-23 网页模板略读确认无正文;关键页 pp.1、2、7 视觉精读与 OCR 互证一致。
- **文本层/OCR 已知损失**: PDF 无文本层(网页打印为图像);WinRT OCR 整体质量良好但存在链接文字乱码(URL 被识别成字母 soup)、"I O DPPM"=10 DPPM、"PDE-Solutjons"=PDF Solutions 等;Fig.1(浴盆曲线)/Fig.2(e-beam 对比)/Fig.3(电源噪声)/Fig.4(proteanTecs 监控,图内 788/685/615mV 等数值 OCR 乱码)/Fig.5(金字塔)为图像,内容经图注+正文互证复原,Fig.4 图内数值不可靠已弃用。
- **10 vs 100 DPPM**: p.1 "10 DPPM"(目标)与 p.7 "100 DPPM"(制造测试单独保证的不成本效益门槛)两数并存——**p.7 已视觉核对原文确为 100**,非 OCR 损失;两种解读(故意的松紧两档 vs 原文笔误)并列记录,不作单向裁决。
- **内部矛盾检查**: 无实质矛盾;Lerner"不太担心老化"与 [28][31] 主张的差异属观点分歧而非事实错误,已在横向对比中如实标注。
- **引语人名核实**: 14 位引语者姓名/头衔/公司均出自 OCR+视觉双通道,头衔转录与机构拼写(如 proteanTecs 之 Brousard)以原文为准。

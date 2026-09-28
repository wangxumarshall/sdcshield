# [50] SDC:理解与量化——Intel/AMD/Arm 三厂观点(IOLTS 2024)

## 标题与出处

- **标题**: Silent Data Corruptions in Computing: Understand and Quantify
- **出处**: **IOLTS 2024**(IEEE 在线测试与鲁棒系统设计国际研讨会;文本层未含页眉/DOI,venue 依文件名 iolts2024_macieira 并经 digest [47] 引文 [16]"IOLTS'24"交叉确认);7 页内容+空尾页。
- **类型**: **产业观点论文**——Intel、AMD、Arm 三大 CPU 设计商各出一节+雅典大学学术框架的六作者联合文本;53 篇中**厂商作者来源最广**的一篇,也是 **OpenDCDiag(SDCShield 的直接上游)的权威学术描述文献**。

## PDF 与实读范围

- **文件**: `ref/Silent Data Corruptions in Computing Understand and Quantify iolts2024_macieira.pdf`
- **页数**: 8 页 PDF(7 页内容+空尾页),**全文实读,无截断**(txt 两段读完,424 行)。

## 作者全列(p.1)

Thiago Macieira(**Intel Corp., Portland, Oregon**)、Sankar Gurumurthy(**AMD, Inc., Austin, Texas**)、Sudhanva Gurumurthi(**AMD, Inc., Austin, Texas**)、Amr Haggag(**Arm Ltd., San Francisco, California**)、George Papadimitriou(雅典大学)、Dimitris Gizopoulos(雅典大学)——厂商×4+学术×2;邮箱逐列(intel.com / amd.com / arm.com / di.uoa.gr)。

## 机构分列(p.1)

- **Intel Corp.** ×1(Macieira——OpenDCDiag 社区维护者);
- **AMD, Inc.** ×2(Sankar Gurumurthy、Sudhanva Gurumurthi——后者兼弗吉尼亚大学教授/AMD Research,此处以 AMD 署名);
- **Arm Ltd.** ×1(Haggag);
- **雅典大学** ×2(Papadimitriou、Gizopoulos)。
- **四机构三厂商:53 篇中唯一"三大 CPU 设计商同框"的文本**。

## 企业合作证据(三级)

- **一级(作者机构)**: **有,三厂商并列**——Intel×1、AMD×2、Arm×1 直接署名(非致谢级);每节以厂商视角独立成文(§II Intel/§III AMD/§IV Arm)。
- **二级(致谢/资助)**: **有**——致谢:"The authors affiliated with the University of Athens have been supported by **Meta Platforms, AMD Research, the Open Computer Project (OCP)**, as well as the European Union's Horizon Europe... **No 101093062 (Vitamin-V)**"(附欧盟免责声明)——**AMD 既出资又出人**(作者+资助双一级/二级),Meta/OCP 仅出资,EU Vitamin-V 延续([49] 的 REBECCA 未续现于此文)。
- **三级(版权页)**: 文本层未见版权行/DOI(提取缺失,见身份核实);IEEE IOLTS 会议论文格式。
- **间接层**: ① **Intel DCDiag**(Data Center Diagnostics Tool,公开发布)与其开源内核 **OpenDCDiag**(GitHub opendcdiag/opendcdiag)[21][23];② **AMD Open Field Health Check(OFHC)**(GitHub amd/Open-Field-Health-Check)[22]——AMD 对标工具;③ Shamsa & Lerner(Meta)**IRPS 2024"导致静默数据错误的缺陷机制"**[27];④ 超大规模厂商报告 [1][2][24](Hochschild/Dixit/[01]);⑤ Ryan 等 ITC'14(Intel,工艺缺陷趋势与测试缺口)[26];⑥ OCP SDC 学术研究奖+Meta 2022 RFP 博客 [19][20]。**判定:一级三厂商+二级四源(Meta/AMD Research/OCP/EU)+间接双工具链——语料中企业合作证据密度之最**。

## 核心结论

1. **错误分类学(§I)**: 计算系统产出错误结果且未在发生点检出=**SDC**;区别于**DCE(可检测可纠正错误)**与**UOE(用户可观察错误)**——三分法比"静默/可见"二分更精细;SDC 足够稀有(大多数硬件终身不遇),但**数十万至百万台规模的云厂商必然遇到**[1][2]——规模必然性论证;配图 Fig.1:云厂商/CPU 设计者/软件开发者/学术界四方共同投入("3+4=11"示例)。
2. **Intel 节(§II):OpenDCDiag 框架**(SDCShield 上游):软硬件测试两要素=**触发**(重负载/非常规使用)+**检出**(对金标准比对或再加工揭示错误);随机输入与可复现性的矛盾→**确定性伪随机数发生器 DPRNG**(种子真随机、子线程状态确定、状态写入日志、可命令行重放);**三多样性策略**——时间多样性(重复执行比对)、拓扑多样性(不同核交叉验证)、实现多样性(不同实现技术交叉验证);框架自动实现时间+拓扑多样性。
3. **OpenDCDiag 工程细节(§II-B–G)**: ①**测试隔离**:每测试 fork() 子进程+写时复制,父进程在子进程 UOE/挂起时存活并监控(信号捕获、时限强杀、报告未完成线程);②**拓扑枚举与负载分布**:NUMA 感知(不跨域除非专测),**可扩展至数千核**;故障数据与工艺代/缓存大小/die 物理位置关联;③**分诊**:物理 socket 隔离使故障单元定位"通常轻而易举"——至少定位到 die/socket;**系统管理员可将坏核移出编排或 OS 禁用,节点带病运行至计划维护**——云厂商不中断长任务的关键收益;④**日志**:YAML 默认(共三种格式),pass/fail/inconclusive 判定;**禁用互斥锁**(防单线程死锁扩散污染根因);每测试前刷盘(定位致死测试);⑤**内存管理**:覆写 C 标准库——零初始化分配(保证可复现)+永不失败分配(失败即中止子进程,问题受控);**拦截内存分配失败把另一类 SDC 转化为 UOE**(内存腐坏可能正由先前 SDC 造成——SDC 的下游可见化)。
4. **AMD 节(§III-A):超大规模报告的共同特征**: SDC 根因定位到硬件;**间歇性故障**特征——非瞬态/随机,而是**可重复可复现**、绑定于少数 CPU/核;也见于其他 SoC [25];业界报告浮现的根因类别=**小延迟故障(small delay faults)**——受影响位点的延迟小幅增加 [26];深亚微米工艺更常见;**[27](Shamsa/Lerner, Meta IRPS'24):这些缺陷与制造测试 normally 捕获的缺陷并无不同——是逃逸,不是新缺陷**——"测试逃逸论"的产业定调。
5. **小延迟故障机理(§III-B)**: 点缺陷→时钟周期一小部分的延迟增量;**路径裕量充足则良性,低裕量/关键路径则致错**;**制造时良性的小延迟可随寿命退化为大延迟**——老化放大机制;制造测试流(晶圆→封装→系统级)中结构测试(stuck-at/transition delay)与功能测试(ISA 序列)互补;结构测试四大局限——stuck-at 抓不到纯延迟缺陷、transition 不优先低裕量路径、path delay 模型路径枚举量爆炸、**现代 CPU 在电压/频率曲线上多点运行→每点关注路径不同**;**ATE 稳压+强温控使任务态事件(电压跌落/局部发热)在结构测试中 unlikely 出现**——制造测试的原理性盲区;小延迟故障的特征=**高参数敏感性+特定电压/温度/负载组合激活**[26]。
6. **在线测试(§III-C)**: 弥补制造测试逃逸(含 burn-in 不足的潜在缺陷逃逸);两条路线——**in-production**(与业务负载并行:须最小化干扰、限制共享资源如内存带宽、**单次运行限于数百毫秒级**[30])vs **out-of-production**(利用固件/内核更新等停机窗:**整个系统可用、可 pervasive 测试数分钟**,但窗口间隔数周至数年);硬件在环方法 [13](=Harpocrates ISCA'24,digest [27])可生成定向于特定结构/故障模型的高覆盖功能测试,适配两种场景。
7. **测试+RAS 合论(§III-D)**: 测试非银弹(现代处理器规模+参数敏感性→穷举不可行);**测试降 DPPM,RAS 降逃逸导致的数据腐坏风险**;RAS 提升系统级测试的可观测性、反哺制造测试;有先验可量化收益的定向 RAS 技术最有价值 [31]——产业版"检测×容忍并举"。
8. **Arm 节(§IV-A):传感框架与预测性维护**: AI 需求→近乎零服务中断;数据中心诊断模式(制造后+现场部署)可**将 SDC 事件从数千 DPPM 降至数十 DPPM**;残余 DPPM 需片上传感器(电压跌落/老化/热事件——皆可致 SDC);**两阶段 ML**:训练阶段(云厂商采集传感数据并与 SDC 事件关联)+部署阶段(传感器组合风险过高→标记 SoC 先行诊断再故障);传感数据还可**动态调整运行条件**(如超预期老化→加电压裕量)——全寿命期监控与自适应+预测性维护。
9. **Arm 节(§IV-B):DFX 方法论**: DFT 高覆盖(stuck-at+transition+**path delay**)、cell/布局感知测试、架构感知的功能 ATE 测试;DFD 快速调试武器库——**ELA(嵌入式逻辑分析仪,逻辑)、EMA(额外裕量调整,存储器)、scan/array dump、ODEM(片上眼图监视器,高速 I/O)、传感器**;潜在缺陷筛查:DFR/DFM(老化/EMIR 裕量、SER FIT 防护)+ **HVS 高压应力筛查(1.5×Vmax 无闩锁)**——先进节点需特殊代工厂 deck 规则;DFX 做好→保证 SDC 弹性、**解除产后筛选的大部分负担**。
10. **结论(§V)**: Intel=OpenDCDiag 框架;AMD=小延迟缺陷检测的重要性与难度;Arm=DFX+片上传感器的预测性维护;**产业共识**:识别静默影响程序执行的初级错误源+发展建模/检测创新方法;近期意识提升与学界动员 [19][20]。

## 分类学标注

- **SDC 核心特征**: SDC/DCE/UOE 三分分类学;规模必然性(数十万台→必然发生);间歇性(可重复可复现、绑定少数核)。
- **根因机理分类**: 小延迟故障(点缺陷→延迟增量→裕量依赖显错)+**寿命退化**(良性→恶性);**"与常被抓缺陷无异的逃逸"论**;老化/电压跌落/热事件激活条件。
- **故障模式**: 电压/温度/负载组合激活;任务态 droop 触发(制造态不可现)。
- **检测技术**: ①软件诊断框架(三多样性+DPRNG 可复现+fork 隔离——OpenDCDiag);②制造测试改进(path delay 模型/cell 布局感知/功能 ATE);③在线测试(in/out-of-production 双路线);④片上传感+ML 预测性维护;⑤DFD 调试武器库(ELA/EMA/ODEM)。
- **处理技术**: 坏核移出编排/OS 禁用+带病运行(可逆降级);RAS 容错设计;HVS 筛查;电压裕量动态调整——**产业处理技术全景**。
- **生命周期**: 制造(测试流/burn-in/HVS)→部署(诊断模式)→服役(在线测试/传感监控/预测性维护)→老化(裕量调整)→退役(退厂分析)——**Arm 节"全寿命期监控与自适应"为 53 篇中生命周期最完整的产业表述**(与 [48] 五阶段框架呼应)。
- **两界关系**: **三厂商+雅典大学的"产业自述+学术组织"文本**——厂商以第一人称公开自家工具与方法(OpenDCDiag/OFHC/传感框架),学术方提供框架与引用;资助结构(AMD 出资+出人;Meta/OCP 出资)显示**厂商-学界双层耦合**。

## 业界观点摘录

1. "the views of the CPU design powerhouses (Intel, AMD, Arm) that deliver most of the computing power of our days at scale, are summarized. These industry giants share their insights..."(摘要——三厂自述定位)
2. "the defects behind these events in hyperscalar infrastructure are **no different than the defects normally caught in the manufacturing test**"(§III-A [27]——逃逸论,去神秘化)
3. "events that can occur in mission mode operations... such as voltage droops and local heating, are **less likely to occur during the application of structural tests**"(§III-B——ATE 原理性盲区)
4. "system administrators may choose to remove them from orchestration or disable them completely in the operating system, and continue operating the affected node until such a time as full maintenance becomes possible."(§II-D——带病运行的产业实践)
5. "This can help reduce SDC incidents from **1000's to 10's DPPM**. But there is a need to do more and this is where on-chip sensors can help."(§IV-A——诊断模式的量化效果)
6. "While testing aims to reduce DPPM, RAS can reduce data corruption risks from test escapes."(§III-D——测试与 RAS 的分工)
7. "intercepting those conditions allows another class of SDCs to become UOEs."(§II-G——SDC 可见化的工程手段)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 4+2 | 厂商作者数(Intel 1+AMD 2+Arm 1 / 雅典 2) | p.1 |
| 3 | 多样性策略数(时间/拓扑/实现) | §II-A |
| 3 | 日志格式数(YAML 默认) | §II-E |
| 数千核 | OpenDCDiag 拓扑枚举可扩展规模 | §II-C |
| 数百毫秒 | in-production 测试单次时长上限 | §III-C [30] |
| 数分钟 | out-of-production 窗口可用时长 | §III-C |
| 数周–数年 | out-of-production 窗口间隔 | §III-C |
| 1000's → 10's DPPM | 数据中心诊断模式的 SDC 事件率降幅 | §IV-A |
| 1.5×Vmax | HVS 高压应力筛查电压(无闩锁) | §IV-B |
| 3 | 错误分类数(SDC/DCE/UOE) | §I |
| 101093062 | EU Vitamin-V 资助号 | 致谢 |
| 2 | 公开工具链(OpenDCDiag/OFHC) | [22][23] |

## 方法论要点

1. **厂商分节自述体**: 每厂独立成节、各自署名——比联合署名更透明的责任划分;综述"业界观点"章节的方法学范本。
2. **框架设计原则显式化**: OpenDCDiag 节把十条工程决策(隔离/拓扑/日志/内存/DPRNG)的设计动机逐一交代——**开源诊断框架的设计文档式论文**;对 SDCShield 的架构理解有直接参照价值。
3. **可复现性工程**: DPRNG 状态外化+命令行重放、零初始化内存、无锁日志——把"科学可复现"落实到框架原语层。
4. **DPPM 贯穿**: 制造测试(DPPM 压缩)→诊断模式(千→数十)→传感(残余)→RAS(逃逸兜底)——**以 DPPM 为主线的产业质量漏斗叙事**。
5. **ML 谨慎引入**: 传感框架的 ML 只做"训练-部署"两阶段风险标记与条件调整,不承诺根因——产业界对 ML 的克制用法。

## 横向对比注记

1. **与 SDCShield 仓库的直接血缘**: §II 即 **OpenDCDiag**——本仓库(CLAUDE.md:"Forked from Intel's OpenDCDiag")的上游权威文献;fork 隔离/DPRNG/三多样性/YAML 日志/NUMA 拓扑/`--list-tests` 等全部对应 SDCShield 现有架构(docs/architecture.md);**这是 53 篇中唯一直接描述我们工具祖先的论文**——综述"产业工具谱系"与仓库 docs/misc/OPEN_SOURCE_PROVENANCE.md 的学术锚点(Macieira 为上游维护者)。
2. **与 [47]/[48]/[49](Gizopoulos 生态)**: 本文是 [47] 的引文 [16]、[49] 的产业版续作(2023 雅典×Meta → 2024 三厂×雅典);致谢链演进:[49] Meta+AMD gifts+EU(Vitamin-V+REBECCA)→本文 Meta+AMD Research+OCP+EU(Vitamin-V)→[47] Meta+AMD+OCP——**AMD Research 与 OCP 在 2024 年进入资助名单**;Gurumurthi(AMD)从 [15] DelayAVF/[27] Harpocrates 的共同作者升级为本文分节作者。
3. **与 digest [27] Harpocrates**: [13] 引文确认 "Breaking the Silence...Hardware-in-the-Loop" = **ISCA 2024(2024-06)**——与 note 27 记录(印刷页 516–531, DOI 10.1109/ISCA59077.2024.00045)完全吻合✓;note 26/27 的 slug 互换问题两文已自洽;[48] 的 A2 Harpocrates++(Micro 46(1) 2026)= note 26 的期刊版✓——**Harpocrates 三部曲(26/27/48-A2)在语料内闭环**。
4. **与 digest [01] SOSP23 + note 27 的 361 DPPM 三角**: note 27 记 "Meta 2023.10 披露 361('3.61 CPUs per 10,000')",[47] Table 1 记 "Alibaba '3.61%...'→361 DPPM",note 01 原记 "3.61‰"——**三方数字一致(361 DPPM=3.61/万),但 2023.10 披露方归属(Meta vs Alibaba)与单位(‰ vs /万)需 Task 10 对 SOSP23 原文终裁**;本文未直接涉及该数字。
5. **与 R2 批次 AVF/延迟故障理论**: AMD 节的小延迟故障与 digest [15] DelayAVF(MICRO'24,计算延迟故障的 AVF)构成理论-产业呼应;[28][29] Gurumurthy 早期工作(ETS'07 延迟缺陷指令自动生成、ITC'14 cache 常驻测试)显示 AMD 作者在该领域的 17 年纵深。
6. **与 R4 测试生成批次**: path delay 路径爆炸/ATE 盲区/功能测试人工成本三大痛点正是 digest [25] SiLFuzz、[26][27] Harpocrates、[31] SDF-pattern、[32] strategies-detect 的靶问题——本文 §III 是 R4 批次的产业动机总纲;[32](Strategies for Detecting Sources of SDC)与本文 §III/§IV 同为测试策略视角,Task 10 可比对。
7. **与 [44] recommendation(He ISCA'23 归属)**: [25] 再次以"other SoCs"语境引 He et al. ISCA'23(标题此处理解为 DL 训练加速器系统)——**三度确认([47]/[48]/[50])He ISCA'23 = Google TPU 线**,note 44 的修正稳固。
8. **"逃逸论"与 [09] test-escapes 对读**: [09](10× test escapes 论文)与本文 [27] Shamsa/Lerner IRPS'24 同持"缺陷与常规制造缺陷无异、是逃逸"立场——Meta IRPS'24(缺陷机制)与 [09](测试逃逸建模)是同一论点的学术/产业双证;综述根因章节的"新缺陷 vs 逃逸"之争可据此立论。
9. **带病运行 vs [47] 的"更换为纲"**: 本文 §II-D 的"坏核禁用+节点续运行"与 [47]"唯一负责任的动作是更换,唯一例外是已知受控缺陷的降级运行"——**同一立场**(受控降级为更换的过渡态),产业文本给出了工程细节(编排摘除/OS 禁用)。
10. **传感+ML 与 [35]/[36](PMC 检测线)**: Arm 传感框架(片上 droop/老化/热)与 R5 批次 [34][35](硬件性能计数器检测 SDC)同属"旁路信号→SDC 预警"家族——前者器件级传感、后者微架构事件计数;综述检测技术章节可归并为"传感/计数器路线"。

## 身份核实

- **IOLTS 2024**: 文本层**无页眉/版权行/DOI**(提取缺失;对比 [49] 有完整 IEEE 页眉)——venue 依文件名 iolts2024_macieira + digest [47] 引文 [16]("IOLTS'24, Macieira, Gurumurthy, Gurumurthi, Haggag, Papadimitriou, Gizopoulos 'Silent data corruptions in computing: understand and quantify'")**双重交叉确认**;8 页 PDF(末页空),全文实读。
- **六作者四机构**: p.1 作者块双栏交错但邮箱域完整可解(intel.com/amd.com×2/arm.com/di.uoa.gr×2);"San Fransisco"为原文拼写错误(应 San Francisco)——照录。
- **致谢(p.6 左栏)**: "authors affiliated with the University of Athens... supported by Meta Platforms, AMD Research, the Open Computer Project (OCP), as well as the European Union's Horizon Europe... No 101093062 (Vitamin-V)"——**资助限定于雅典作者**(厂商作者不受此资助),表述精确,二级证据确认。
- **文本层问题**: 首页 Fig.1 图内文字(四方角色)与正文混排;§III-C 末"in-production testing... can be run more frequently"段与 §III-D 衔接处文本层有一处跨栏倒置,按语义归位;参考文献 **[4] 与 [6] 为同一文献重复著录**(stealthy-saboteurs IOLTS'23)——编辑部/作者疏漏,照录为证。
- **引文链核对**: [1]=Hochschild✓、[2]=Dixit 2021✓、[3]=[08] TC'23 正式版(72(11):3072–3085——为 note 08 的卷期页码补全)✓、[4]/[6]=[49]✓(重复)、[9]=[17]✓、[13]=[27] ISCA'24✓、[24]=[01]✓、[25]=He ISCA'23✓、[27]=Shamsa/Lerner IRPS'24(Meta)✓、[30]=Dixit 2022✓、[22]/[23]=AMD/Intel 工具仓库✓——**12 条引文全通**。
- **内部一致性**: SDC/DCE/UOE 三分与 [01]/[47] 的二分叙述兼容(细化);"间歇性可复现"与 [03] Ripple 的"周期性重测捕获"一致;1000's→10's DPPM 与 [47] Table 1(~1000/<1000/361)量级自洽。
- **OpenDCDiag 描述与 SDCShield 现状对勘**: fork 隔离/YAML/DPRNG/NUMA/`--list-tests` 与本仓库实现一一对应(CLAUDE.md 与 docs/architecture.md 可互证);本文未涉及 ARM64 移植(SDCShield 的增量),属我们项目的自主扩展——**引用时须区分上游能力与本仓库扩展**。

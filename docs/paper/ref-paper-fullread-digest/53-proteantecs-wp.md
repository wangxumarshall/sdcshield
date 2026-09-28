# [53] proteanTecs 白皮书:两阶段检测制服 AI 处理器中的 SDC(2024,厂商白皮书)

## 标题与出处

- 标题:Outsmarting Silent Data Corruption in AI Processors With Two-Stage Detection
- 出品:proteanTecs Ltd.(半导体遥测/深度数据分析公司,据公开资料为以色列公司;白皮书内仅见公司名与版权行)
- 版权:© 2024 proteanTecs. All rights reserved.(p.13);发布于公司官网 www.proteantecs.com
- **类型:厂商白皮书(技术营销文档)**——53 篇语料中唯一非学术/非工程会议文档,按用户决定"纳入并标注类型"收录
- 无署名作者、无 DOI、无评审信息——企业匿名署名即白皮书形态判定特征

## PDF 与实读范围

PDF 14 页(p.1、p.14 空白;正文 p.2–13),经 ocr32 提取为 53-proteantecs-wp.txt(24,183 B,414 行),单次读完,无跳读。含目录、§1–§10(含 References 9 条)、末页联系方式。文本层基本干净,p.11 有 "inmminent/inminnet" 拼写错误(原文或文本层,不影响语义)。

## 作者全列(p.1)

无署名作者。企业主体:proteanTecs Ltd.。

## 机构分列(p.1)

- proteanTecs Ltd.(唯一主体;片上 agent IP + 测试机边缘软件 + 云端分析平台的商业模式见 §7–§8 自述)

## 企业合作证据(三级)

- **一级(作者隶属)**:不适用——无署名作者,文档本身即企业产物。
- **二级(资助/致谢)**:不适用——无资助节、无致谢;文档为营销资产,无独立性声明。
- **三级(版权页)**:"© 2024 proteanTecs. All rights reserved." + "proteanTecs Ltd." + www.proteantecs.com(p.13)。
- **间接层(被引用的业界实体,构成本文的"证据借用"结构)**:Google(HotOS'21 mercurial cores、"engineer-decades" 引语)、Meta(Dixit 2021/2022 排障案例,arXiv 2102.11245 / 2203.08989)、Alibaba(SOSP23 361 DPPM + 三案例)、Intel("Data Center Silent Data Errors" 2024 文章引语)、Broadcom(ITS Asia 2023,50% NTF)、IEEE 学术两条(Papadimitriou & Gizopoulos TC'23;Upasani TC'15)、MRHIEP 产业路线图(2024)。**问题定义全部来自上述二手文献,解法部分为自家产品主张,无一手实验数据**——白皮书的知识生产形态,综述引用须标注证据等级。

## 核心结论

1. **SDC 定义与产业定调**:不可追溯的硬件故障所致,"silent"即无异常机制、无系统日志,错误数据持续传播引发级联失败;超大规模厂商估计约 1/1000 机器受影响(Google/Meta,仅为估计),**Alibaba 精确统计 361 DPPM**(致 SDC 的缺陷率)(§1)。
2. **根因定位**:SDC 主要源于**计算故障而非内存位翻转**——ECC 类冗余可护存储/传输,但对指令错误执行类 SDC 无效;IEEE 归纳三类根因(时序错误、设计 bug、制造缺陷);Alibaba 归纳五大脆弱特征(算术逻辑、向量运算、浮点、缓存一致性、事务内存)(§1)。
3. **测试逃逸论证**:新工艺节点+复杂架构超出硅测试能力;Scan/ATPG/BIST/功能测试只给 pass/fail 二值结果,对工艺变异所致细微 SDC 异常不敏感;软错误率从 65nm 的 1 次/年升至 16nm 的 1 次/1.5 小时(引 Upasani TC'15,Fig.1)(§2)。
4. **在役检测两盲区**:金丝雀电路在位测试对真实关键路径时序裕量盲(老化/工艺变异/OCV 增大,引 MRHIEP);周期性维护测试灵敏度不足、且设备离线脱离真实工况(§2)。
5. **SDC 上升第二动因**:AI/ML 训练与推理等计算密集负载空前规模、峰值性能运行提高 SDC 概率;给出制造(高密度节点/工艺变异/测试短板/潜在缺陷)× 在役(老化/裕量不足/大规模基础设施/计算密集负载)两阶段成因表(§2)。
6. **后果五案例实录**:Meta 数据库文件丢失(Int(1.153) 误算致压缩文件大小 0 而非 156);Alibaba 校验和指令间歇出错、缓存一致性致校验不一致、哈希计算错误三案例;Google 罕见指令暴露制造缺陷(§4)。
7. **排障经济学**:Google "applied many engineer-decades to it";Meta 从 Scala 高层负载到汇编的近十级排障流水(Scala→Java→JBC→汇编,3 个工具 + 5 项逆向修改),"不可扩展、只能事后调试不能预防";**Broadcom 50% SDC 调查以 No Trouble Found 告终**;MRHIEP:"只是冰山一角,未来 5–10 年需要新技术"(§3–§4)。
8. **冗余计算否证**:同功能多核执行、全一致才算正确——可防传播,但大规模下硬件代价不可行(§3)。
9. **两阶段检测主张**:制造阶段参数化分级(parametric grading:按工艺变异与预测性能裕量给参数化评级而非 pass/fail,标记 outlier、"walking-wounded" 不出厂)+ 在役连续监测(裕量危险时实时干预、自动调压/调频补偿老化与环境)(§5–§6)。引 Google "testing becomes part of the full lifecycle of a CPU, not just an issue for vendors or burn-in testing" 为纲。
10. **产品一 Outlier Detection™(制造段)**:片上 agent 参数测量(含百万级 IC 路径时序)+ 测试机边缘软件实时内联评估 + 云端平台分析;ML 学习正常行为、识别常规方法不可见的异常;每设备个性化 IDDQ+时序裕量"预期 vs 实测"模型;系统级测试(SLT)阶段用时间序列分析捕捉短测试中不显现的 SDC;案例:某客户 DUT 裕量从 22 时间单位骤降至 4(通过全部常规测试、性能属"边缘而非不合格"),被弃用,**潜在 SDC 率降低 up to 40%**(未具名客户案例,§7,Fig.2–3)。
11. **产品二 RTHM™(在役段)**:**预测性+处方性维护**替代预防性维护(周期性全测成本高且仍漏检);固件应用实时识别问题、给出纠正建议与失败预防中断;检出事件"尚非实际错误",但累积为低芯片健康指数、常先于 SDC;性能指数(PI)综合实际负载/老化/受影响区域/时钟与电源域/历史事件,连续反映"距失败的距离";低健康指数触发中断,由 BMC 决策处置并记录即将失败的位置(§8,Fig.4,PI 降幅示例 44%)。
12. **结论**:AI 爆炸(语音/图像/视频/文本)→ 硬件复杂度与多样性上升 → 数据完整性风险上升;制造缺陷、加速老化、环境因素致存储/传输/处理各环节损坏;传统制造预防+数据中心运维手段不足;两阶段(ML Outlier Detection + RTHM)可"显著降低 SDC 率"(§9)。

## 分类学标注

- **文档类型**:厂商白皮书(监测/遥测供应链立场)——语料中唯一纯产业营销技术文档;问题定义二手、解法一手(自家产品)。
- **检测技术主张**:参数化 ML outlier 检测(IDDQ+时序裕量,ATE/SLT 两点)+ 实时健康监测(PI 指数、阈值中断)。
- **处理技术主张**:参数化分级拦截出厂、实时调压/调频补偿、预测性+处方性维护、BMC 处置。
- **生命周期主张**:制造+在役两阶段全生命周期检测——直接呼应 Google "测试成为 CPU 全生命周期一部分" 论纲,把 [47]/[48] 的五阶段生命周期操作化为两段产品架构。
- **根因口径**:时序错误/设计 bug/制造缺陷(引 IEEE)+ 工艺变异/老化/裕量不足;注意其将软错误与 SDC 混同(Fig.1 "Soft errors such as SDC"),与学术谱系(制造缺陷/老化根因)口径不同。
- **企业关系**:本文即企业(proteanTecs);间接层借用 Google/Meta/Alibaba/Intel/Broadcom/IEEE/MRHIEP 七方权威。
- **证据等级**:二手文献组装 + 单一未具名客户案例;无方法学、无可复现实验——综述引用时必须标注。

## 业界观点摘录

- "SDC rate is much higher than software engineers expect, undermining the hardware reliability they used to take for granted."(§1,时代判断)
- "SDCs don't trigger exception mechanisms or leave any record in system logs."(§1,定义)
- "Conventional Scan, ATPG, BIST and functional testing are often unable to detect the causes of silent data corruption, making it an inevitable fault, however destructive."(§2,测试逃逸判词)
- Intel:"In a data center running multiple millions of servers, 24 hours a day, the rare event becomes an expected occurrence, and the system administrator must be proactive in managing the event."(§3)
- Google:"The problem is serious enough for us to have applied many engineer-decades to it."(§4)
- MRHIEP:"The industry sentiment is that we are only seeing the tip of the iceberg of this fault type, and new techniques will be required over the next five to ten years to drive down their rate of occurrence."(§4)
- Google:"For detecting mercurial cores as quickly as possible; in effect, testing becomes part of the full lifecycle of a CPU, not just an issue for vendors or burn-in testing."(§6,被白皮书立为总纲)
- 自我定位:"an industry-only approach that combines parametric on-chip monitoring at the physical layer of the silicon, with advanced and continuous analytics, connecting the HW and SW layers for the first time."(§6,营销修辞,"first time" 须打折)
- 维护哲学升级:"Predictive and Prescriptive Maintenance instead of preventive maintenance."(§8)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| ~1/1000 | Google/Meta 估计的机群 SDC 受影响机器比例 | §1 |
| 361 DPPM | Alibaba 精确统计的致 SDC 缺陷率(云系统) | §1 |
| 1 次/年 → 1 次/1.5 小时 | 软错误率 65nm → 16nm(引 Upasani TC'15;软错误≠SDC,口径见身份核实) | §2, Fig.1 |
| 50% | Broadcom SDC 调查以 No Trouble Found 告终的比例(ITS Asia 2023) | §3 |
| 0 vs 156 | Meta 案例:Int(1.153) 误算致压缩文件大小 0 而非 156 | §4 |
| ~10 级 / 3 工具 / 5 项 | Meta Scala→汇编排障流水线级数 / 所用工具 / 逆向修改项 | §4 |
| 5 | Alibaba 归纳的 SDC 脆弱特征数(算术/向量/浮点/缓存一致性/事务内存) | §1 |
| 22 → 4 时间单位 | 案例 DUT 时序裕量骤降(通过全部常规测试后被弃用) | §7, Fig.3 |
| up to 40% | 未具名客户应用 Outlier Detection 后潜在 SDC 率降幅 | §7 |
| 44% | RTHM 性能指数(PI)跌幅示例(真实系统可视化) | §8, Fig.4 |
| 2 阶段 / 2 产品 | 制造(Outlier Detection)+ 在役(RTHM)两阶段产品架构 | §6 |
| 9 条 / 6 家 | 参考文献 9 条;其中引用机构 6 家(Google/Meta×2/Alibaba/IEEE×2/Intel/Broadcom/MRHIEP,按主体计) | §10 |

## 方法论要点

- **证据借用结构**:问题定义 100% 引自超大规模厂商与学术公开文献(9 条引文),解法 100% 为自家产品——白皮书的标准论证形态;引用其论点时须区分"其引用的权威"与"其产品主张"。
- **参数化 vs 二值化**:全篇方法学核心是把 pass/fail 二值测试升级为"预期 vs 实测"参数化分级(每设备个性化 ML 模型),从判决式质检变为统计式风险标记。
- **两阶段闭环**:制造段拦截(walking-wounded 不出厂)+ 在役段监测(PI 指数→中断→BMC 处置),对应其两阶段成因表的对称解法。
- **维护哲学升级**:预防性(周期全测)→ 预测性(健康指数预警)+ 处方性(带纠正建议)——把 [28] 老化先兆思想产品化。
- **不可验证性**:40%、22→4、44% 等关键数字均无方法学、无样本量、无置信区间,且 case study 为外部链接——综述引用须整体降级为"厂商主张"。

## 横向对比注记

1. **与 [47]**:同引 Google/Meta/Alibaba 数字体系(1/1000、361 DPPM);[47] 是学术布道文( Gizopoulos ),本文是厂商白皮书——同一问题域的两种话语形态;[47] "estimate, never measure" 与本文 "only provide estimates…exact statistics" 相互印证。
2. **与 [50] Arm 传感器框架**:Arm 两阶段 ML(1000's→10's DPPM)与 proteanTecs 两阶段产品为同一"参数化+ML+两阶段"范式的并行演化——芯片厂商自建 vs 第三方监测供应商,两类主体的同构解法。
3. **与 [28] aging-asplos24**:老化先兆→主动检测哲学;RTHM 的 PI 指数+中断+BMC 处置是该哲学的商用实现,且加上"处方性"一层。
4. **与 [30] irps25-rl**:降 DPPM 的两条产业路径——RL 优化测试生成(学术+产线)vs ML 参数化 outlier 分级(监测 IP 商);殊途同归于"测试逃逸后仍有第二道闸"。
5. **与 [09] test-escapes**:10× 测试逃逸论(学术)与本文 Scan/ATPG/BIST 逃逸论(产业)互为回声;[09] 的 DPPM 语境被白皮书直接产业化。
6. **与 [08] tc23-micropersp**:白皮书 ref 4 直接引用;学术微架构定义被产业文档吸收为权威背书——学术→产业的话语传导链标本。
7. **与 [01]/[03] 语料内引用链**:ref 3=[01] SOSP23、ref 2=[03] Fleetscanner(arXiv 版)、ref 4=[08] TC'23——53 语料内部引用链在白皮书中的第三次出现(前两次为 [47]/[49] 引用体系);Meta Int(1.153) 案例出自语料外 arXiv 2102.11245。
8. **与 [46] fs-metadata / [39] exabyte-db**:软件层 SDC 后果叙事(文件系统/数据库)与白皮书五案例的跨层重叠——后果维度上软件层文献与产业文档共用同一批故障史。
9. **与 [51]/[52] R8 批"非学术署名"光谱**:[51] 零资助独立测量、[52] 零资助独立评测、[53] 零引用一手数据的自我主张——独立性与证据强度的两端,为企业卷入形态学补上"监测供应链"一角。
10. **Broadcom ITS Asia 2023(语料外独家线索)**:50% NTF 是 53 语料中唯一 Broadcom SDC 来源,为综述"排障经济学/根因不明"主题的补充证据线索。

## 身份核实

- **361 DPPM 归属第三方确认(Task 10 关键)**:白皮书明确"Alibaba recently published exact statistics revealing 361 DPPM"(§1),与 [47] Table 1 一致;结合 2023.10=SOSP23=[01](Alibaba),note 27 所记"Meta 2023.10, 3.61/万"的归属疑误进一步坐实——**361 DPPM=3.61/万 应归 Alibaba(SOSP23)**,Task 10 终核时以此为准。**(2026-09-28 终核已完成:note 01(单位 ‰→/万)与 note 27(归属 Meta→Alibaba)均已按此更正;[27] 原文参考文献 [3] 即 S. Wang et al. SOSP'23,归属链坐实;连带更正 note 05 SEVI fleet 率 0.072 ‰→/万——其原文自证与 [01] 0.348"aligns"同单位对比)**
- **软错误/SDC 混同**:Fig.1 题注"Soft errors such as SDC"并把 Upasani TC'15(声学波探测器,软错误方向)的 65nm→16nm 数据用作 SDC 上升趋势论据——软错误(辐射根因)与 SDC(制造缺陷/老化根因)在学术谱系中分属两族(参见 [52]),白皮书混用属营销口径的宽松,综述引用须拆分。
- **Meta 案例转写**:§4 "producing zero instead of 156 when executing Int(1.153)" 将计算(Int(1.153) 应为 1)与结果(大小 0 而非 156)压缩在一句;与 Meta 原文(arXiv 2102.11245,语料外)对照时留意转写差异,本笔记按白皮书原文记录。
- **证据等级**:40% 降幅、22→4 裕量、44% PI 均为单一未具名客户案例或示例截图,无方法学细节;白皮书自称"industry-only""first time"为营销修辞。
- **引文映射**:ref 1=HotOS'21 Cores that don't count(语料外)、ref 2=[03] arXiv 2203.08989✓、ref 3=[01] SOSP23 pp.216–230✓、ref 4=[08] TC'23✓、ref 5=Upasani TC'15(语料外)、ref 6=MRHIEP 2024(语料外)、ref 7=Intel 2024(语料外)、ref 8=Meta arXiv 2102.11245(语料外)、ref 9=Broadcom ITS Asia 2023(语料外)。
- 无作者/无日期(仅 © 2024)/无 DOI,出版信息以版权行与官网为准;p.1、p.14 空白页确认非缺页。
- p.11 "inmminent/inminnet" 拼写错误为原文或文本层,不影响语义。

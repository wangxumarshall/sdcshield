# [39] Google Spanner 超大规模数据库系统的 SDC 检测与预防(SELSE 2022,产业界)

## 标题与出处

- **标题**: Detection and Prevention of Silent Data Corruption in an Exabyte-scale Database System
- **出处**: IEEE conference on Silicon Errors in Logic – System Effects (**SELSE**), 2022-05-19,线上举行(p.1 脚注 "Appeared in the IEEE conference on Silicon Errors in Logic – System Effects, May 19, 2022 (held virtually)")。
- **与 [31] 的引文链互证**: [31] sdf-pattern 引 Bacon SELSE'22 为 [2]——即本篇,版本关系**确证**(单作者同一文)。
- **与 [38] 的引文错配确证**: [38] SENTRY 相关工作段描述的 "exabyte-scale database systems…end-to-end checksums, fail-stop invariants…prioritization of computation-intensive components" 与本文 §VII 结论清单逐项吻合——[38] 意引的正是本篇,其引文 [26] 指向 Yakhchi 系错配,坐实。
- **类型**: **产业界论文**(Google 单作者,SELSE 会议报告;SELSE 为硅错误领域工业界+学术界工作坊)。

## PDF 与实读范围

- **文件**: `ref/Detection and Prevention of Silent Data Corruption in an Exabyte-scale Database System.pdf`
- **页数**: 6 页(pp.1–5 内容 + p.6 空),**全文实读,无截断**。

## 作者全列(p.1)

David F. Bacon(Google, New York;单作者——著名编程语言/系统研究者,GHC 编译器作者)。

## 机构分列(p.1)

- **Google**(纽约)——单作者纯产业界。

## 企业合作证据(三级)

- **一级(作者机构)**: **Google**(最强形式:产业界一手生产经验论文)。
- **二级(致谢/资助)**: 无致谢/资助节(企业内部工作,不适用)。
- **三级(版权页)**: 无版权行/DOI/ISBN(SELSE 工作坊报告形态);仅 p.1 出现声明行。
- **间接层(企业协作的显性证据)**: ① **与 CPU 厂商联合筛选**: "Programs specifically designed to screen for bad machines were developed internally **and with CPU vendors**"(§II-B)——Google 与芯片厂商共同开发坏机筛选程序的直接陈述;② **退片给厂商分析**: "we may return the chip to the vendor who may be able to verify that the chip is bad"(§II-B);③ 引文圈: Dixit et al. Meta arXiv 2021 [3]、Hochschild et al. Google HotOS'21 "Cores that don't count" [4]——两大产业 SDC 起源文献;④ 运营建议直接面向 **hardware vendors** 群体(§VII)。

## 核心结论

1. **系统背景与规模(§I)**: Spanner 支撑 Google 7 个十亿级用户产品中的 5 个 + AdWords/Zanzibar/Google Cloud 控制面;**>10 亿查询/秒、多个 EB 数据、数万数据库**、Google 机群资源最大消耗者之一;可用性 SLA 99.999%(五个九);3+ 地理副本,Zanzibar 再加数十只读副本。**SDC 事件(硬件错误所致)被 Spanner 检出/阻止的频率为每周数次**。负载多样性(模式/读写比/并发语义/简单读写 vs 复杂 SQL 子程序)远超同规模系统(如 Colossus 文件系统)→ 更大 bug "攻击面"。
2. **检测——崩溃分诊(§II-A)**: 生产环境每次 Spanner 服务器崩溃均由工程师分诊——聚类失效、报 bug、路由根因;与 SRE 协同干预(回滚发布/禁特性/隔离 tablet)。Spanner 是 **Google 测试资源的单一最大消费者**,但规模使极罕见 bug 仍被某负载触及。
3. **检测——生产 SDC 症状画像(§II-B,核心经验)**: 软件 bug 几乎总有相关性(栈轨迹/对象大小类);多年前分诊团队观察到**唯一相关性是同一台机器**的崩溃上升,且趋势恶化超出机群增长能解释的范围。坏机症状指标(单独或组合):① 内核 panic 水平升高;② Spanner 等"高可靠性"二进制中 SIGSEGV/SIGILL/SIGFPE 升高,**尤其集中于单核或超线程对**;③ 无关的 fail-stop 不变式检查失败升高;④ 任何 fail-stop 校验和不匹配(最强指标)。**时间间隔一年以上的事件仍可构成诊断信号**(memtable 校验失败 + 9 个月前的内核 panic 即可标记坏机)。
4. **"惯犯"与弹出流程(§II-B)**: 常规修复流程检不出这类坏机、修完放回机群——称为 **"recidivists"(惯犯)**;遂建立显式弹出流程,下放停机权限给分诊工程师;与 CPU 厂商联合开发筛选程序,能抓相当一部分但绝非全部。
5. **SDC 判别方法论(§II-B.1)**: ① vs 磁盘/网络错误——盘/网已有硬软件校验和,Spanner 压缩逻辑再算应用级校验和(写前解码/解密/验证,关文件时经特殊 API 对照内存级校验和);② vs 软件错误——**软件错误几乎必然跨多机复发**;判定多机损坏为硬件所致需极高门槛+旁证(如全部嫌疑机未过筛选);③ vs DRAM 故障——**SDC 几乎总是限于单核,偶尔限于一组共享末级缓存(LLC)的核**;④ 片上缓存 vs 功能单元(ALU/FPU)**运营上不作区分**——反正只能换芯片;⑤ 芯片好坏判定"一半艺术一半科学"——单芯片失效率太低、失效模式太杂,统计分析与建模仍然缺位。
6. **检测——跨副本审计(§II-C)**: 每数据库**默认每周**全副本审计 + 索引/基表一致性检查 + 结构不变式检查;检出即由操作员销毁少数派损坏 tablet、从其他副本自动重建(兼修硬件/软件致损)。局限:① 检出滞后最长一周;② 全副本扫描极昂贵;③ 多版本数据库 + 副本间压缩不同步 → 只能比**语义相等性**(选定快照时间戳上全表扫描的用户可见行序列),不能比结构相等;④ **t1 时刻损坏若在 t2 被覆写,数据库仍处损坏态(t2 之前的读会看到脏数据)但审计快照 ≥t2 即漏检**——被覆写损坏是状态比对的原理性盲区。
7. **软硬件 bug 拮抗作用(§II-D,概念性贡献)**: SDC 把水搅浑,同时拖慢硬件与软件 bug 的检测与根因:不可信组件越多,工程师越不愿深挖任一组件;追查数小时/数天后结论为瞬态硬件错误的工程师,下次追查热情下降。**高 SDC 水平实质性地拖慢关键软件错误的检出,有时足以让 bug 未及根因就撞上生产**——"This isn't theoretical: we have suffered bugs in production because a very rare failure was mis-classified as SDC."。**SDC 在硬件可靠性之外还降低软件可靠性**。
8. **预防——校验和体系(§III-A)**: 校验和从静态数据保护升级为 SDC 检测+预防的核心:① 既有 memtable/LSM 层校验(最可靠坏机指标之一);② 新增低频路径缓冲区校验;③ **端到端校验**——数据在层间流转永不"裸奔"(理想不可全达——很多变换无法在校验和域内计算——但仍有效);④ **SDC 可产生错误地址→不能依赖 ECC 硬件保护 RAM**→长寿命结构(RAM 块缓存)周期性重校验+每次查表的块头验证;⑤ **小而高爆炸半径结构**单独保护(加密密钥;覆盖整个 LSM 层的 key/时间戳范围摘要——损坏则整层数据被错误略过)。
9. **预防——不变式检查(§III-B)**: 根本理想是 **fail-stop**——即使拦不住错误执行,也要在错误结果写出/返回前崩溃。有效不变式须覆盖大范围计算与内存:输入断言 "a<b" 类基本无效;**"扫描返回的 key 有序" / "append-only 队列时间戳递增"**类在实践中有效。fail-stop 检查对无相关性的低概率故障(硬件)有效,但对相关性软件 bug 会造成**崩溃循环**——"每个不变式检查都是一次判断权衡"(硬件容错 vs 软件 bug 韧性)。
10. **核级剔除的讨论(§III-C)**: 现行做法是整机弹出;备选是内核调度屏蔽坏核(类比闪存坏块管理)。坏核仅对 fail-stop 失效可可靠识别;崩溃明确集中于单核的情形"大概值得做";**但最险恶的是真静默损坏——检测在时间与空间上都远离成因**;弹出后用筛选程序定位坏核是最大希望,成效未明。
11. **单芯片解析模型(§IV,定量贡献)**: 参数:R=坏芯片故障率;K=检出并剔除所需故障数;E=故障致未检出用户可见错误的比例;D=故障产生可检信号的比例(D 与 E 独立);H=任何产错程序运行的 CPU 时间占比;Q=Spanner 运行占比。**核心结果:坏芯片服役期供给的损坏数 ni = K·Q·E/(D·H),与故障率 R 无关**(反直觉)。含义:① 提高 D(把错误转成检测事件)直接减损——"我们常把这类检查当作阻止错误本身,而低估了其阻止未来损坏结果的作用";② 提高 H(更多检测器、热路径校验、借用其他服务当检测器)减损;③ 降 K(更快剔除)减损;④ 引入机龄退役 Y 后:ki2=min(ki,Y)、ni2=min(ni,Y·R·Q·E)。**Google 正处于"弹出周期(数周至数月)≪芯片寿命(数年)"的区间——坏芯片故障率降 10× 也不会减少供给的损坏数**;对厂商的含义:**减少坏芯片的数量,而不是坏芯片的故障率**。
12. **硬件 vs 软件 bug 的未来判断(§V)**: SDC 威胁取决于应用类型:小部署→不足虑;bug 多的软件→SDC 淹没在噪声底下;**高度可靠+大规模部署的软件→SDC 是显著问题**(Spanner 已然:分诊的独特失效中相当数量源于坏机)。软件 bug 随复杂度/团队规模增长、有失效 locus、单点修复、随舰队规模**次线性**增长;硬件 bug 随**舰队规模**增长、表现完全无关、修复极耗工程时——"A steady stream of faults is death by a thousand cuts."。当前判断:**软件 bug 仍是更大危险**(低到能进生产但又频发致损的那类;OS/编译器 bug 也可类 SDC 地违反底层正确性契约);若硬件 SDC 继续增长,可能需要 Tandem NonStop 式冗余执行或更根本的软件失效冗余设计。
13. **扩展效应(§VI)**: 系统持续增长→**per-CPU-second 错误率必须持续压低才能保持总错误率不变**;Spanner 的 per-CPU-second SDC 改进已被服务增长抵消;厂商侧晶体管数增长也可能淹没 per-transistor 故障率改进——双向挤压构成对关键任务系统的重大威胁。
14. **结论清单(§VII)**: 软件侧——语义允许处用端到端校验和;复杂语义操作用 fail-stop 不变式;不只保护大数据结构,聚焦**"计算质量"(computational mass = 数据量 × 其上计算量)最大**的组件(SDC 倾向打 CPU);压低其他组件的噪声底。运营侧——集中监控疑似 SDC 事件并尽量标注嫌疑机器源;**高可靠服务共置**以借其高保真筛选加速坏机弹出。**给硬件厂商**——最小化坏芯片数量而非坏芯片故障率;投资软件检测方法并主动共享。

## 分类学标注

- **根因机理**: 不研究物理机理;给出运营层根因画像:损坏**几乎总限单核(或共享 LLC 核组)**;片上缓存 vs 功能单元不作区分(运营等价:换芯片);单芯片失效率极低+模式极杂→统计建模缺位。
- **故障模式**: ① 核局限损坏(单核/超线程对/共享 LLC 核组);② **时间-空间远距检测**(检测点远离成因点——最险恶类);③ **被覆写损坏**(t1 损坏 t2 被覆写,快照审计漏检);④ **错误地址**(SDC 生成错误地址→ECC 假设失效);⑤ 高爆炸半径小结构损坏(密钥/LSM 层摘要→整层数据遗漏);⑥ "惯犯"机器(常规流程放回的坏机);⑦ 软硬件 bug 拮抗(SDC 掩蔽软件 bug、软件 bug 类 SDC 表现)。
- **检测技术**: 生产运行时全栈:① 崩溃分诊(人工聚类);② 症状画像(4 指标,跨年时窗关联);③ 跨副本周期审计(语义相等性);④ 校验和体系(memtable/LSM/端到端/周期重校验/块头);⑤ 语义不变式(有序性/单调性,fail-stop 化);⑥ 内部+与 CPU 厂商联合筛选;⑦ 离线筛选验证。生命周期:**运行期+运维期**(检测与运维闭环,全生命周期运营视角)。
- **处理技术**: ① tablet 级修复(少数派销毁+副本自动重建);② **机器弹出**(显式弹出的惯犯流程、分诊工程师停机权);③ 核级调度屏蔽(讨论未定);④ SRE 干预(回滚/禁特性/隔离);⑤ 发布前测试(Spanner 为 Google 测试资源最大消费者)。

## 业界观点摘录(本批最密集的产业一手观点)

- **频率**: "Silent data corruption events due to hardware error are detected/prevented by Spanner several times per week."(Abstract)
- **空间分布规律**: "SDC is almost always limited to a single core, or occasionally to a group of cores sharing a last-level cache."(§II-B)
- **判定现状**: "In the end, determining whether a chip is bad remains part art, part science. The per-chip failure rate is so low and the failure modes sufficiently diverse that statistical analysis and modeling remain elusive."(§II-B)
- **ECC 不足以应对**: "SDC can generate various types of incorrect computation, which includes incorrect addresses. Thus we can no longer rely on hardware protection of RAM against corruption (e.g. with ECC)."(§III-A)——与 [35] 引 Meta/Alibaba 的 ECC 论证同声。
- **拮抗作用**: "SDC therefore reduces software reliability in addition to hardware reliability."(§II-D);"we have suffered bugs in production because a very rare failure was mis-classified as SDC."(§II-D)
- **检查的双重价值**: "We tend to think of the role of such checks as preventing the errors themselves, and insufficiently value their role in preventing future corrupted results."(§IV)
- **工程经济学**: "A steady stream of faults is death by a thousand cuts."(§V)
- **对厂商的直言**: "Minimize the number of bad chips, not the rate of faults once a chip goes bad." / "Invest in software detection methods, and share them proactively."(§VII)
- **当前威胁排序**: "At the present time, I still believe software bugs are the bigger danger, in particular bugs that are low enough probability to reach production but frequent enough to cause significant damage."(§V)
- **最险恶形态**: "the most insidious cases are true silent corruptions where the detection is far removed in time and space from the cause"(§III-C)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 每周数次 | Spanner 检出/阻止的硬件 SDC 事件频率 | Abstract |
| 5 / 7 | Spanner 支撑的 Google 十亿级用户产品数 | §I |
| >10 亿 QPS / 多 EB / 数万数据库 | Spanner 规模 | Abstract, §I |
| 99.999% | 可用性 SLA(五个九) | §I |
| 3+ / 数十 | 地理副本数 / Zanzibar 追加只读副本数 | §I |
| 每周 1 次 | 跨副本审计默认频率(滞后上限一周) | §II-C |
| >1 年 / 9 个月 | 症状事件仍可关联的时距 / 实例(内核 panic+校验失败) | §II-B |
| 数周–数月 / 数年 | 坏芯片弹出周期 / 芯片舰队寿命(模型适用区间) | §IV |
| 10× | 厂商降坏芯片故障率的假想幅度(不减少损坏供给) | §IV |
| ni = K·Q·E/(D·H) | 坏芯片服役期损坏供给模型(与 R 无关) | §IV |
| 单核 / 共享 LLC 核组 | SDC 空间局限粒度 | §II-B |
| 5 | 参考文献数(Spanner×2/Dixit/Hochschild/Zanzibar) | References |

## 方法论要点

- **症状画像而非器件机理**: 不建物理模型,靠运营症状(4 指标+跨年时窗关联)圈定坏机——产业界在"单芯片失效率太低无法统计建模"现实下的实用主义方法论;与学术界 AVF/FI 路线形成方法论对照。
- **被覆写损坏作为状态检测的原理性盲区**: 审计快照 ≥ 覆写时刻即漏检——该盲区同时被 [38] SENTRY(第一层 25–50% 检出)独立发现;两文互证"状态比对必须辅以过程/依赖验证"。
- **单芯片解析模型的杠杆分析**: ni 对 R 的不变性 + Google 处于"弹出快于老化"区间 → 全部杠杆在 K(更快弹出)、D(更多检测事件)、H(更多检测器)——把"该投哪类防御"翻译成代数;建模虽自谦 "rough first cut",杠杆结论(K/D/H 优先于 R)对产业策略直接可用。
- **端到端校验的理想-现实张力**: "数据永不裸奔"作为理想 + "很多变换无法在校验和域内计算"的现实——校验和体系的设计本质是覆盖范围与计算可行性的折衷。
- **失效模式对抗性设计**: fail-stop 检查对硬件(无相关性)有效、对软件 bug(相关性)致崩溃循环——**同一机制对不同故障族的适得其反**,每个不变式都是权衡;这是检测设计中罕见明说的对抗性视角。
- **测试规模的极限承认**: Google 最大测试消费者 + "bugs do make it past testing" + "scale means even extremely rare bugs are exercised"——产业界对"测试不能兜底"的直接经验陈述,支撑 test-escape 叙事([09])。

## 横向对比注记

- **与 [38] SENTRY(同批)**: 数据库层 SDC 检测的产业翼 vs 学术翼——Bacon/Google 生产经验(每周审计、语义相等性、被覆写盲区)vs HIT 双线程仿真(增量快照、依赖图、100% 依赖检出);[38] 的被覆写盲区发现与本文审计局限互证;[38] 意引本文而错配引文(见身份核实)。审计周期:周级 [39] vs 亚秒级 [38]——生产系统与仿真的时间尺度差两个数量级以上。
- **与 [01][04] 产业舰队论文(R1)**: 本文是 Google 数据库侧的早期(2022)回应,与 Meta Dixit arXiv 2021([3],其前身)、Google Hochschild HotOS'21 "Cores that don't count"([4])构成现代 SDC 话语的产业起点三件套;R1 舰队测量论文([01] SOSP'23 等)是其后继的系统化。
- **与 [07] Hardware Sentinel(Meta ASPLOS'25)**: 两大超大规模数据库/服务运营商的对策对照——Meta 应用层哨兵(日志/崩溃信号/异常行为)vs Google Spanner 库内校验和+审计+分诊;同为产业一手经验,层次不同(应用 vs 数据库内核)。
- **与 [33] Orthrus(同批)**: 云用户态确定性重执行(87–96% 检出、4% 开销)vs Spanner 生产校验和/审计体系——重执行路线在 Spanner 语境的对应物是"Tandem NonStop 式冗余执行"(本文 §V 作为兜底未来项提及);Bacon 的计算质量(computational mass)聚焦原则与 Orthrus 的选择性验证(数据通路优先)同源于"SDC 倾向打 CPU"。
- **与 [34][35] ECC 论证链**: "SDC 可产生错误地址→不能依赖 ECC" 与 [35] 引 Meta/Alibaba 的 ECC 不足论证([16]/[52])同声——产业界对 ECC 兜底神话的共识性否定,综述中可汇成一条产业观点主线。
- **与 [31] sdf-pattern**: [31] 引本文为 [2]——ATPG 测试生成侧与数据库运维侧在同一问题(坏芯片检出)上的产业分工:厂商测试 vs 云商运营筛选。
- **与 [09] test-escapes(Mitra D&T 2025)**: 本文(2022)的"测试不能兜底"经验与 [09](2025)的 test-escape 定量(5K/1K DPM)前后呼应,构成 Google 内部叙事的时间线;[36] SHOUT 引的 Google 数据即 [09]。
- **单作者论文的罕见性**: 53 篇中唯一单作者产业论文——资深研究者(GHC 作者)以个人经验总结形态发表的工业实践报告,SELSE 工作坊作为产业-学术交流缝隙地带的典型出口。
- **"recidivists"/"computational mass"/"bug antagonism"**: 三个术语造词——产业话语对学术综述的概念馈赠,建议综述术语表收录。

## 身份核实

- **出处确证**: p.1 脚注明示 SELSE 2022-05-19(线上);无 DOI/ISBN/版权行(工作坊报告形态);与 [31] 的 [2] 引文(Bacon SELSE'22)及 [38] 的描述内容双重互证。
- **作者身份**: David F. Bacon,Google(纽约),dfb@google.com——单作者;业界知名(编程语言研究者),身份无疑。
- **页数核对**: 6 页,p.6 空;全文实读。
- **模型公式文本层歧义**: "ni = K/R·H·D · R·Q·E = K·E·Q/D·H" 的文本层无括号,按运算优先级与观察 (3)(H 增大→ni 减小)裁定为 **ni = K·Q·E/(D·H)**(H 在分母);第三种写法 "K·E/D·H/Q" 与此一致但排版歧义,已在正文按裁定形式呈现。
- **下标扁平化**: k_i2/n_i2(ki2/ni2)、R·H·D 等乘法点号部分丢失——按上下文复原,复原项均已标注。
- **拼写瑕疵**: "repertoir"(repertoire)一处;项目符号(•)在文本层部分丢失,列表结构按语义复原。
- **无数值表、无图表**: 全文纯文字+两个列表(症状清单/结论清单);所有数字均在散文中,无表格恢复负担。
- **无致谢/资助**: 企业内部工作,不适用——如实记录而非缺漏。
- **时态证据**: "A number of years ago…began to observe"(§II-B)——观察起点约在 2017–2019,与 Dixit 2021/Hochschild 2021 公开化的时间线吻合,内部叙述自洽。

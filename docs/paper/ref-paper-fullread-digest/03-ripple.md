# [03] Detecting silent data corruptions in the wild(Fleetscanner/Ripple;arXiv 2022;工业研究报告/会议风格论文)

- PDF: `Fleetscanner_Ripple Detecting silent data corruptions in the wild.pdf`;页数 8(实读 p.1–7,末页空白)
- 作者(全列):Harish Dattatraya Dixit、Laura Boyle、Gautham Vunnam、Sneha Pendharkar、Matt Beadon、Sriram Sankar(共 6 人)
- 机构(学术/企业分列):
  - 学术:无
  - 企业:Meta Platforms, Inc. ×6(全部)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Meta ×6(p.1)
  - 二级(致谢章节资助/数据/设备):致谢(p.7)感谢者均为 Meta 内部(Aslan Bakirov、Melita Mihaljevic、Thiara Ortiz、Tejasvi Chakravarthy、Manish Modi、Vijay Rao、T.S. Khurana、Nishant Yadav、Aravind Anbudurai、Bill Holland、Chris BeSerra、Marty Humphrey、Fred Lin、Daniel Moore);**另 §5.2.3 明确记录厂商合作:Meta 向芯片厂商分享 fleet 数据后,厂商在自家测试工具中启用了适配 Ripple 模式的测试,Intel 并据此发表 trickle testing 白皮书 [29](Arjan van de Ven,Intel Data Center Diagnostic Tool)**——本文方法论的产业扩散证据
  - 三级(首页脚注资助声明):原文无(arXiv 预印本,无资助脚注)
- 核心结论(各带节号/图表号):
  1. **硅测试漏斗新增第六级**:传统止于基础设施 burn-in,但 SDC 无任何系统日志残留/健康信号,必须在产线 fleet 内周期性专项测试(§3.6,Fig.1)——这是 Meta 自 2019 年起建产线 SDC 检测的根本动机。
  2. **四大故障加速因子刻画**(§4,以 3×5 计算为例):数据随机化(数据相关位模式:3×5=15 但 3×4=10,测试状态空间巨大)、电气变化(f/V/I 波动,多变量状态空间)、环境变化(机房热点/季节性,机房 A 算 15 机房 B 算 12)、生命周期变化(早于传统 bathtub 预测的退化:同一计算每日重复 6 个月后在第 6 个月失败)——**"今天正确不保证明天正确"**。
  3. **Fleetscanner(停产机会式测试)**:搭维护流程便车(固件/内核升级、provisioning、维修的 drain/undrain 窗口),分钟级测试,生命周期累计 6800 万测试、40 亿 fleet 秒;对所研究 CPU 缺陷族覆盖 93%,其中 23% 为 Ripple 不可达的独占覆盖;约 5–6 个月实现全 fleet 覆盖(§5.1,§6.1,Table 1)。
  4. **Ripple(在产常开测试)**:与生产负载同置协同,毫秒级测试片段注入,月 25 亿测试实例、仅 1 亿 fleet 秒/月;独占覆盖 7%(依赖测试指令与负载的频繁切换、同数据数千次迭代,长时测试无法触发);70% 共同覆盖 15 天达成(Fleetscanner 需 5–6 个月);总覆盖 77%(§5.2,§6.2,Table 1)。
  5. **两种模式互补且按缺陷族变化**(§6.3):不同类型缺陷的覆盖分割不同;新签名可在约 2 周内全 fleet 扩展(§5.2.2);Ripple 对硅转换缺陷(silicon transition defects)与退化中器件特别有效。
  6. **历史对照**:过去每颗 CPU 仅在 burn-in 中测几小时,之后靠抽检——对 SDC 完全不够(§6.3 末)。
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**无检查逻辑覆盖的内部电路缺陷**(摘要),被数据路径变化/温度/年龄加速;四加速因子框架(§4);具体物理根因不展开
  - 故障模式类型:**数据相关位模式故障**(特定输入子集必错);**f/V/I 工作点相关**;**位置/季节相关**;**时间退化型**(早发 bathtub);错误无日志残留、跨服务远端传播(摘要/§1)
  - 检测技术类型:**停产机会式测试**(维护窗口搭车);**在产常开微测试**(ms 级共置、负载感知缩放);**影子测试 A/B**(§5.2.1,防测试缺陷全 fleet 爆炸的保险);测试模式来自产线经验+硅厂商输入(§5.1)
  - 处理技术类型:**隔离池(quarantine pool)→ 深入调查/测试精化 → 回归产线**(§5.1);软修复或换件(§5.1 维修流程)
- 业界观点摘录(工业实践论文,虽以论文体裁发表):
  - "以千分之一器件的故障率,在数百万器件的基础设施中,错误传播到应用的概率是必然的"(§4)
  - "唯一能可度量地保护 fleet 的方法是反复测试+持续改进的测试例程与高级测试模式生成"(§4)
  - "我们已将该测试机制作为 SDC 工具的重要演进方向向业界推荐;基于我们的发现与数据共享实践,厂商已在测试工具中启用适配模式,并已围绕 trickle testing 发表白皮书"(§5.2.3)
  - OCP 2021 专家面板 [4] 汇集工业界与学术界讨论该领域路线(§2)
- 关键数字(表):

  | 指标 | Fleetscanner | Ripple | 出处 |
  |---|---|---|---|
  | 测试量 | 6800 万(生命周期) | 25 亿/月 | Table 1 |
  | 测试时长 | 40 亿 fleet 秒(生命周期) | 1 亿 fleet 秒/月 | Table 1 |
  | 单次测试时长 | 分钟级 | 毫秒级(数百 ms 上限) | §6.1/§6.2 |
  | 性能感知(不扰产) | 否 | 是 | Table 1 |
  | 缺陷族覆盖 | 93%(独占 23%) | 77%(独占 7%) | §6.1/§6.2 |
  | 70% 共同覆盖耗时 | 5–6 个月 | 15 天 | §6.3 |
  | 全 fleet 覆盖周期 | 5–6 个月 | 新签名 2 周内全 fleet | §6.1/§5.2.2 |
  | 历史基线 | burn-in 数小时/颗 + 抽检 | — | §6.3 |

- 方法论要点:3 年产线数据聚合;单一 CPU 缺陷族上的双方法等价覆盖对比(明确"等价覆盖"定义:不同方法检出同一失败集合的能力,§6);共置税(footprint tax)按负载类型逐一测定并设阈(§5.2.1);安全护栏(测试缺陷防全 fleet 扩散的影子/A-B 机制)。
- 横向对比注记:本文即 02(PinDrop)引用的 [6],是其 IRMS 连续测试基础设施的方法论前篇(PinDrop §III-A"advanced implementation of the methodology in Fleetscanner");与 01(Alibaba)的分组 2 周快照式形成方法论对照——Meta 路线演化为"常开在产测试",直击 02 批评的快照盲区。前篇 [10](Meta at scale,arXiv 2102.11245,Spark 案例 1.1^53=0)不在本 53 篇集内;Google "Cores that don't count"[14] 亦不在集内(其 10x 逃逸后续 09 在集内)。Intel DCDT/trickle testing 白皮书 [29] 是本文方法论的厂商落地。
- 身份核实:标题"检测野生产环境中的 SDC"与内容(Fleetscanner/Ripple 双策略 3 年对比)相符;arXiv:2203.08989v1 时间戳 2022-03-16 印于左缘(p.1),Meta 域名邮箱(@fb.com)齐全,名实相符。注意其为 arXiv 预印本(工业报告体裁,无同行评审标记),时效上早于 02,引用时按"工业研究报告/预印本"类型标注。

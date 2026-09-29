# [09] Silent Data Corruption by 10× Test Escapes Threatens Reliable Computing(Google 技术白皮书/立场论文,无正式出版信息)

- PDF: `Silent Data Corruption by 10x Test Escapes Threatens Reliable Computing.pdf`;页数 21(实读 p.1–20,末页空白;无会议页眉/DOI/日期/版权行——白皮书体裁)
- 作者(全列):Subhasish Mitra、Subho Banerjee、Martin Dixon、Mike Fuller、Rama Govindaraju、Peter Hochschild、Eric X. Liu、Bharath Parthasarathy、Parthasarathy Ranganathan(共 9 人)
- 机构(学术/企业分列):
  - 学术:斯坦福大学 ×1(Mitra;脚注 1:研究在 Google 完成,主隶属 Stanford)
  - 企业:Google ×9(全部署名);另 Govindaraju 脚注 2:研究在 Google 完成时署名,**现隶属 NVIDIA**——业界人才流动样本
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Google ×9(p.1;含 Stanford 的 Mitra 以"工作在 Google 完成"署名)
  - 二级(致谢章节资助/数据/设备):致谢(p.17)感谢 CMU Shawn Blanton、Stanford Phil Levis、Google Stephanie Morton(编辑),披露使用 LLM 做文体编辑;**§2 记录与芯片厂商协作根因分析("driving root-cause analysis in collaboration with our vendors"),<10% 检出芯片送回厂商诊断**;§3.2.1 列举 CASP 工业落地:Amazon Deterministic In-Fleet Scan Test [Trock 24]、Intel In-Field Scan [Inkley 24]、NVIDIA In-System Test [Jagannadha 19]
  - 三级(首页脚注资助声明):无(白皮书)
- 核心结论(各带节号/图表号):
  1. **总问题定性:制造测试逃逸至少 10× 于工业目标**——数据中心实测终身约 5,000 DPM(含 t=0 缺陷与 ELF 早期失效),而工业目标为 100–500 DPM;适用所有计算芯片类型(CPU/GPU/ML 加速器);估计 test escapes 导致 0.5% 芯片被更换(Obs.1,标题"10×"出处)
  2. **SDC-causing 芯片约 1,000 DPM(≈千分之一)**——Google fleet 4 代平台、多厂商、22nm–5nm 工艺跨 11 个平台实测 345–1857 DPM(Table 1);新世代制造测试更多但问题未实质改善;因系统级测试彻底性局限,该数字是**下界**(Obs.2,Table 1)——**本文是业界"约千分之一器件受 SDC 影响"共识数字的原始出处之一**(05/06/07 均引用此量级)
  3. **SDC-causing 芯片高频出错且具惯犯性(recidivist)**:已知 SDC-causing CPU 中位错误率 820K 次/十亿芯片小时(Platforms 2/5,Table 2);ML 加速器训练负载内嵌细粒度检查(抽查 1% 运行时)得 916K/十亿芯片小时(Table 3)——远高于辐射软错误;系统级测试结果对真实应用有代表性(Obs.3)
  4. **诊断与根因分析极其有限(Obs.4,Fig.1)**:<10%"有趣"案例送回厂商;结果分布——**NTF(No Trouble Found)36%**(厂商环境无法复现,三大原因:厂商无法跑完整负载/部分实为设计 bug/多芯片系统难以定位单芯片)、**ELF 29%**(厂商原通过测试现在失败)、Test gap fixed 18%、Test gap 未解决 10%、**运输损坏 7%(异常高)**;现有诊断需多名工程师数月、常无果
  5. **多数逃逸部署后才检出(Obs.5,Table 4)**:某大量部署平台——预部署测试 12%(479/百万)、在线/离线测试 29%(1099/百万)、**系统健康与取证 49%(1886/百万,最大头)**、用户级检查 10%(393/百万);逐行成本递增,理想应是制造测试全检出
  6. **三路径行动方案(§3)**:(a) 从系统级错误行为快速诊断(区分硬件缺陷 vs 软件 bug、定位缺陷——挑战:错误检测延迟可达数十亿时钟周期、复现极难、仿真慢真实硅数个数量级);(b) 现场检测(t=0+ELF);(c) 克服既往工业实验陷阱的新测试实验
  7. **In-field testing 两翼(§3.2.1)**:系统级测试(数小时)是主力但"ad hoc、缺乏坚实技术基础"——多次出现"看似全面、加测又现大量逃逸";scan DFT 现场化路径 **CASP**(片外存储压缩测试模式+高速 I/O+在线无感模式)已工业落地(Amazon/Intel/NVIDIA),开销仅 1% 面积/1% 功耗/3% 性能 [Li 13]
  8. **现有测试度量的根本缺陷(§3.2.1)**:stuck-at 已知严重不准 [McCluskey 00];cell-aware 依赖难获得的缺陷分布;**Nigh 24:TIC 缺陷 90%+ 的检出无法用测试度量施加的 0/1 解释——scan 检出多为幸运(fortuitous)**;测试条件(电压/时钟/温度)几乎未被 ATPG 系统分析;新度量方向 PEPR [Li 22](伪穷尽物理感知区域测试,模式多 10×–100× 但现场测试经济性可容)
  9. **In-field error detection 与采样检查谱系(§3.2.2)**:开销三降策略(特定原因/特定应用定向、关键块选择性保护、时间采样);缺陷永久性+惯犯性→**采样检查可行,形成从低代价"最终检出"(缺陷芯片可能在检出前已产生 SDC)到昂贵连续检查防 SDC 的谱系**;软件重执行+检查 [Lin 14, Oh 02] 有效(内部状态使"同序列同错"非主要担忧);两大开放问题——test escapes 的关键硬件块识别(缺准确缺陷故障模型)、软件技术有效性
  10. **CCKC 启发式(§3.2.3)**:Core-Concentrated Kernel Crashes——30 天内 ≥5 次内核崩溃、80%+ 集中于单物理核、≥3 个不同 top-of-stack 符号(过滤软件 bug)→**70%+ 被指控核确为 SDC-causing、<10% 假阳性**;系统软件信号最有价值(处处运行、成熟、加固)——与 07(HWSentinel)核浓度方法互证(本文引 [Dutta 25])
  11. **AI/ML 三前沿角色(§4)**:从系统级信号识别缺陷机器("大海捞针"模式,超越规则方法);数据驱动调度现场测试/错误检测(何时/何块/何内容);研究 test escape 错误对 AI/ML 负载的应用级影响
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**制造测试逃逸**(t=0 缺陷 + ELF 早期失效,ELF 远早于预期寿命、非终末老化);边际缺陷假设(marginal defects [Ryan 14]:电压-温度-频率包络内特定工作点失效);明确区分制造缺陷 vs 设计 bug vs 模拟电路问题
  - 故障模式类型:**SDC 与 crash/hang 并存**;**惯犯性但非每次显现**(内部状态+电压/频率/温度变化);高频错误率(820K–916K/十亿芯片小时);核集中失败(CCKC);跨多平台多厂商多工艺节点的普遍性
  - 检测技术类型:**三路径框架**——系统级行为快速诊断(硬件缺陷 vs 软件 bug 区分+定位);in-field testing(系统级测试+CASP scan 现场化);in-field error detection(指令重执行/多样性增强/采样检查谱系);system health forensics(CCKC 规则)
  - 处理技术类型:检出即换件(vendor swap);CASP 片外测试模式按可靠性约束与失败特征持续更新;"最终检出"谱系(接受检出前 SDC 风险换低开销);厂商测试内容迭代(test gap fixed 18%)
- 业界观点摘录(纯工业界立场论文,摘原话):
  - "今天有太多缺陷计算芯片逃逸制造测试——数据中心所有计算芯片类型的逃逸量至少比工业目标高一个数量级"(摘要)
  - "大多数被换芯片未做完整诊断,因为管理与分诊的经济性不可行——从商业角度直接更换不幸更实际"(Obs.1)
  - "制造测试实践缺乏准确估计出厂逃逸的方法(中略)缺乏稳健反馈机制,行业对 test escapes 的真实原因实际上处于盲态(effectively blind)"(§3/§4)
  - "现有创建系统级测试的方法(含随机指令与 fuzzing)是 ad hoc 的、缺乏坚实技术基础"(§3.2.1)
  - "今天的 scan 测试大多是幸运地检出缺陷芯片——90%+ 的检出无法用测试度量所施加的 0/1 解释"(§3.2.1)
  - "传统容错计算作为兜底也不可行——对广泛的商品化部署太贵"(§4)
  - "多家超大规模厂商(Alibaba、Amazon、Google、Meta、Microsoft)与硬件公司(AMD、ARM、Intel、NVIDIA)均已讨论 test escapes 与 SDC"(§1)
  - "厂商因后勤、技术与法律原因常无法在自家测试环境运行完整负载"(脚注 6,NTF 根因)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | test escapes 率 | ~5,000 DPM 终身 vs 工业目标 100–500(≥10×);0.5% 芯片被换 | Obs.1 |
  | SDC-causing 芯片 | ~1,000 DPM;11 平台 345–1857 DPM(下界;Table 1 逐平台映射文本层错乱) | Obs.2, Table 1 |
  | 平台跨度 | 4 代、多厂商、22nm–5nm;2016/2017 年开始关注 | Obs.2, §2 |
  | 错误率(CPU) | 中位 820K 次/十亿芯片小时(Platforms 2/5) | Obs.3, Table 2 |
  | 错误率(ML 加速器) | 中位 916K 次/十亿芯片小时(训练负载 1% 运行时抽查) | Obs.3, Table 3 |
  | 厂商根因分析 | <10% 送回;NTF 36%/ELF 29%/gap fixed 18%/gap 未解 10%/Damaged 7% | Obs.4, Fig.1 |
  | 检出时机 | 预部署 12%(479/百万)、在线离线测试 29%(1099)、系统健康取证 49%(1886)、用户级 10%(393) | Obs.5, Table 4 |
  | CCKC 阈值 | ≥5 次崩溃/30 天、80%+ 单物理核、≥3 个不同 top-of-stack 符号;70%+ 确为 SDC-causing、<10% 假阳性 | §3.2.3 |
  | CASP 开销 | 1% 面积、1% 功耗、3% 性能(在线模式) | §3.2.1 [Li 13] |
  | PEPR 代价 | 测试模式多 10×–100×(现场经济性可容) | §3.2.1 [Nigh 25] |

- 方法论要点:超大规模 fleet 统计(数百万级、多代多厂商多工艺);Table 1 平台归一化+声明测试能力随时间改进(下界语义);ML 加速器 1% 运行时细粒度内嵌检查验证系统级测试代表性;CCKC 阈值化规则设计(核集中+符号多样性双条件过滤软件 bug);引用学术 Murphy/ELF 实验 [McCluskey 00] 作为新实验设计范本(专测芯片+穷尽模式+广泛条件;局限:样本少、芯片小、布局访问受限);对既有工业实验三大陷阱(ground truth 不彻底/scan 覆盖率低+幸运检出/条件受限诊断不足)的元批评。
- 横向对比注记:**与 06(ITHICA)同一 Stanford×Google 团队**(Mitra/Banerjee/Liu/Fuller/Hochschild/Ranganathan 6 人重叠),06 引本文为 [53]("产线负载可检出相当比例缺陷"动机)——同一研究纲领的两阶段:09(问题宣言+三路径)→ 06(路径 b 软件重执行+检查的工业化实现,实测验证"内部状态使同序列不同错")。引用网络密集覆盖本集:01[Wang 23]、07[Dutta 25,即 CCKC 与核浓度互证]、Meta at scale [Dixit 21]、Cores that don't count [Hochschild 21](两者均不在集内)、Meta ISCA23 DNN 加速器 [He 23]、OCP 行动 [Parthasarathy 24];Amazon ITC24 [Trock 24]、Intel IFS [Inkley 24]、NVIDIA IST [Jagannadha 19] 三例 CASP 落地是"云厂商↔芯片厂商"生态协作证据;~1,000 DPM SDC-causing 是 05/06/07 所引"千分之一"共识的原始出处之一(与 Meta 1/1000 实测互证)。
- 身份核实:标题"10× 测试逃逸的 SDC 威胁可靠计算"与内容(5 个 Observation 数据+三路径行动方案)完全相符;**体裁说明**:全文无会议页眉、DOI、日期、版权行——Google 技术白皮书/立场论文(与 06 引其为 [53] 的身份一致),引用时按白皮书类型标注;署名页 Google 徽标+两枚隶属脚注(Mitra=Stanford 在 Google 完成工作、Govindaraju=现 NVIDIA)齐全。**文本层已知损失**:(1) **Table 1 数值与平台行映射在文本层错乱**(Generation 列与 DPM 列交错,11 个数值 318/1175/653/1097/794/1605/1096/495/345/1857/625 无法逐平台可靠对应)——引用时给范围 345–1857 DPM 与平台数,不做逐平台映射;(2) Table 4 数值(479/1099/1886/393 per million)与百分比(12/29/49/10%)自洽(合计约 3,857–3,992/百万≈0.4%,与 Obs.1 的 0.5% 量级一致);(3) Fig.1 为图像,五类百分比均引自正文散文;(4) ML 加速器检查"1% of their total runtime"为抽查比例非覆盖率,按原文引用。

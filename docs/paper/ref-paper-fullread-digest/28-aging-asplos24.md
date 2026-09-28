# [28] Proactive Runtime Detection of Aging-Related Silent Data Corruptions: A Bottom-Up Approach(ASPLOS 2024,印刷页 220–235)

- PDF: `ASPOLOS2024 Proactive Runtime Detection of Aging-Related Silent Data Corruptions A Bottom-Up Approach.pdf`;页数 17(实读全部:p.1–16 即印刷页 220–235,末页空白;ACM ISBN 979-8-4007-0391-1/24/04,DOI 10.1145/3622781.3674182,ASPLOS '24, April 27–May 1, 2024, La Jolla)
- 作者(全列):Jiacheng Ma、Majd Ganaiem、Madeline Burbage、Theo Gregersen、Rachel McAmis、Freddy Gabbay、Baris Kasikci(共 7 人)
- 机构(学术/企业分列):
  - 学术:密歇根大学 ×1(Ma)、以色列理工学院 Technion ×1(Ganaiem)、华盛顿大学 ×3(Burbage/Gregersen/McAmis)、耶路撒冷希伯来大学 ×1(Gabbay)
  - 企业:**Kasikci 双聘"华盛顿大学与 Google"(University of Washington and Google)——通信作者级 Google 共同署名**
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Kasikci 挂 "University of Washington and Google, USA"(p.1)— **Google 双聘署名**
  - 二级(致谢章节资助/数据/设备):致谢(p.232)— **Intel TSA Center 与 PRISM Research Center(JUMP 中心,由 SRC 与 DARPA 联合资助)**;感谢 shepherd Iulian Neamtiu 与审稿人
  - 三级(首页脚注资助声明):无(标准 ACM 版权页)
  - 间接层:**动机数据全来自业界——Alibaba SDC 观测([75]=SOSP'23):已用 CPU 中出现 73.5% 的 SDC;Google SiliFuzz ~500,000 测试用例([69]=[25]);Intel DCDiag 全程执行 45 分钟([10])+ OpenDCDiag([1],GitHub 引用)**;§1/§6.1 与 SiliFuzz/OpenDCDiag 的对照定位贯穿全文;§6.3 商业化愿景("芯片制造商为数据中心运营商生成测试套件,运营商回采 trace 改进 Aging Analysis")
- 核心结论(各带节号/图表号):
  1. **SDC 检测频率是核心矛盾(§1,Takeaway #1)**:Alibaba 数据——**73.5% 的 SDC 出现在已投入使用的 CPU(63.9% 系统重装时+9.6% 生产中)** [75];现行长测试套件(Alibaba 每三个月一次)无法及时捕获渐进积累的老化;Google SiliFuzz ~50 万用例、Intel DCDiag 全程 45 分钟——**自上而下(top-down)方法把硬件当黑盒,必须生成海量复杂测试,无法高频调度**
  2. **硬件不是黑盒(Takeaway #2/#3,§2.2/§2.3)**:网表级实现细节+晶圆厂验证过的物理模型可指导 SDC 检测;信号概率(SP)剖面+老化感知静态时序分析(STA)可识别易老化路径
  3. **老化物理机理(§2.3)**:BTI(偏置温度不稳定性)主导——静态电压长期施加→阈值电压漂移→开关延迟增大;**反应-扩散模型 ΔVth∝(t)^(1/6)**(式 1),**10 年退化中 ~70% 发生在第 1 年**;老化非均匀——SP 低(闲置在固定态)的单元退化更快,时钟门控是分布不均的主因,pMOS 比 nMOS 更易受 BTI
  4. **两类时序违规的严重性二分(§2.3.2)**:setup 违规(信号到迟)可降频补救;**hold 违规(信号过早翻转)降频无效,必须修芯片——更严重**;两者都使触发器采样错值→SDC
  5. **Vega 三阶段工作流(§3,Fig.2)**:Phase 1 **Aging Analysis**——SP 剖面仿真(代表性负载)→SPICE 模拟构建老化感知时序库→STA 找违规路径(10 年寿命假设,最悲观电压/温度/工艺角,晶圆厂规范);Phase 2 **Error Lifting**——时序违规的逻辑模型(式 2/3:前后周期值不变则碰巧采样对,否则采到错值)+MUX 失效模型植入网表(影子副本 shadow replica)+**形式验证 cover property 反向生成可证明激发故障的模块级输入 trace**→依据微架构专家知识转为汇编指令(查表法,一次性人力);Phase 3 **Test Integration**——软件老化库(多语言 wrapper/顺序或随机调度/异常钩子)或 **profile-guided 剖面引导插桩**(找"不频繁但常被访问"的插入点,IR 指令数估算开销,超阈值则概率触发)
  6. **实验平台(§4/§5.1)**:CV32E40P(开源 32 位顺序 RISC-V)的 ALU(167MHz)与 FPU(250MHz),28nm 工艺;Cadence Genus/Synopsys DC 综合、Cadence Innovus 布局布线、JasperGold 形式验证、Yosys(+3,700 行 C++)、LLVM pass(800 行 C++)、Verilator;embench 基准
  7. **老化路径识别结果(§5.2.1,Fig.8/Table 3)**:ALU 11 条老化敏感路径(WNS -76ps);**FPU 1,363 条 setup+3 条 hold(WNS -157ps/-1ps)**;老化退化非均匀分布——ALU/FPU 各 52%/35% 逻辑单元延迟+6%,35%/25% 单元+1.9%;去重后 ALU 6 对/FPU 41 对唯一起止点
  8. **测试用例构造(§5.2.2,Table 4/5)**:无缓解时成功率 ALU 66.7%/FPU 51.2%;**形式验证证明 33.3%/43.9% 的路径对"不可能产生实际错误"(fuzzing 无法给出的证明)**;FPU 有 4.9% 超时+FC(波形不可转指令:需多指令传播+状态标志已被先前指令改变);最终用例 **ALU 8 条/124 周期、FPU 42 条/685 周期**(缓解启用后 8/134、66/1202)——**数百到数千周期即可全量执行,实现秒级高频检测**(对比 DCDiag 45 分钟)
  9. **检测质量(§5.2.3,Table 6)**:目标失效检测率 ALU 100%(全部模式)、FPU 95.4–100%;多数失效被"为其他失效设计的用例"提前检测到;两例 ready/valid 握手信号失效使 CPU 停滞(应用不响应中断)→可检测;仅 1 例因初值依赖漏检,§3.3.4 缓解(上升/下降沿触发)修复
  10. **对照随机测试(§5.2.3,Table 7)**:ALU Vega 100% vs 随机 45–50%;**FPU 意外发现——β=1 或随机模式时随机测试也可达 95.4–97.2%**;但随机法无"证明某些失效永不发生"的能力
  11. **运行时开销(§5.3,Fig.9)**:profile-guided 集成平均 **0.8%** 性能开销(部分基准为负——与编译器优化噪声不可区分)
  12. **与自上而下框架的正交定位(§6.1/§8)**:SiliFuzz/OpenDCDiag(黑盒、海量、宽覆盖)vs Vega(白盒、极小、精准、高频);**"数据中心运营商可用 Vega 将定向测试内嵌于软件持续监控,同时将自上而下框架的长测试套件安排在定期维护周期"**;副产品失效网表(failing netlists)可作后续可靠性研究的电路级失效模型
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**晶体管老化(BTI/NBTI 反应-扩散机理)——本集迄今唯一将 SDC 根因建模到门级物理机理的检测论文**;非均匀老化三定律(SP 依赖/时钟门控致分布不均/pMOS>nMOS 敏感)
  - 故障模式类型:**时序违规(setup/hold)二分及其严重性阶梯(hold>setup:降频可救 setup、hold 必须修片)**;失效表现为错算指令/位翻转/缓存一致性破坏;可检测性的意外模式——握手信号失效→CPU 停滞→应用层"无进展+不响应中断"即检测信号;初值依赖是漏检模式
  - 检测技术类型:**自下而上(bottom-up)功能性测试生成:老化感知 STA+形式验证 trace 反向生成+微架构专家指令构造——与 SiliFuzz(fuzzing 代理)/OpenDCDiag(库移植)/Harpocrates(硬件在环迭代)并列为第四条技术路线**;运行时集成(库/剖面引导插桩)将检测从"维护窗口"搬入"应用生命周期"
  - 处理技术类型:轻量——库支持异常钩子(检测到失效抛异常交 catch 块处理);§6.3 商业化闭环愿景(厂商-运营商数据回流)
- 业界观点摘录:
  - §1:"近期云计算提供商(Meta、Google、Alibaba)报告了集群内 SDC 事件" [37][52][75]
  - §1:"在 Alibaba,此类测试仅每三个月调度一次" [75];"Google 的 SiliFuzz 生成约 500,000 个测试用例,Intel 官方 CPU 诊断工具 DCDiag 的完整执行需 45 分钟"
  - §1:"Alibaba 观察到其 CPU 中相当一部分 SDC 仅在一段使用期后出现" [75]——老化假说的产业观测依据
  - §6.1:"我们视 Vega 与这些自上而下框架为正交工具(中略)数据中心运营商可用 Vega 将定向测试内嵌软件持续监控,而自上而下框架的长测试套件可安排在定期维护周期以获得更广覆盖"
  - §6.3:"未来研究可探索将 Vega 配置为商业环境:芯片制造商为数据中心运营商生成测试套件;运营商在自有环境中回采有价值的 trace 与统计;厂商再利用这些数据精化 Aging Analysis,生成针对特定数据中心负载的测试套件"——**"厂商生成-运营商执行-数据回流"的产业分工闭环愿景**
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | Alibaba 已用 CPU SDC 占比 | **73.5%**(重装 63.9%+生产 9.6%);测试每 3 个月一次 | §1 [75] |
  | 业界测试体量 | SiliFuzz ~500,000 用例;DCDiag 全程 45 分钟 | §1 |
  | 老化退化时间分布 | 10 年总退化的 ~70% 发生在第 1 年 | §2.3.3 |
  | ALU/FPU 老化敏感路径 | ALU 11 条(WNS -76ps);FPU 1,363 setup+3 hold(-157ps/-1ps) | §5.2.1, Table 3 |
  | 延迟退化分布 | ALU 52%/FPU 35% 单元 +6%;35%/25% 单元 +1.9% | §5.2.1, Fig.8 |
  | 唯一起止点对 | ALU 6 对/FPU 41 对 | §5.2.1 |
  | 用例构造成功率 | ALU 66.7%/FPU 51.2%(形式证明无害:33.3%/43.9%) | §5.2.2, Table 4 |
  | 最终测试套件 | ALU 8 条/124 周期;FPU 42 条/685 周期(缓解版 8/134、66/1202) | §5.2.2, Table 5 |
  | 检测率 | ALU 100%;FPU 95.4–100%(漏检 1 例经 §3.3.4 缓解修复) | §5.2.3, Table 6 |
  | vs 随机测试 | ALU 100% vs 45–50%;FPU 100% vs 35.3–97.2% | §5.2.3, Table 7 |
  | 运行时开销 | 平均 **0.8%**(profile-guided) | §5.3, Fig.9 |
  | 平台 | CV32E40P RISC-V,28nm,ALU 167MHz/FPU 250MHz | §4/§5.1 |

- 方法论要点:自下而上方法论宣言——"硬件不是黑盒"(网表+晶圆厂验证的物理模型是可利用的先验);覆盖代理链:SP 剖面→老化感知时序库(SPICE 预计算)→STA 违规路径;形式验证作"测试生成器"而非"验证器"——cover property+影子副本将"激发故障的输入存在性"问题转化为反例求解;失效建模用 MUX+DFF 影子副本植入(同款网表可综合到仿真/FPGA 作后续研究的失效模型库);两处关键缓解:初值依赖(上升/下降沿触发)与 unrealistic trace(assume property 约束);profile-guided 插桩选点准则"不频繁但常被访问"(冷点而非热点);β 常量化(0/1/随机)限制形式验证搜索空间。
- 横向对比注记:**与 [25] SiliFuzz/[26][27] Harpocrates/[30] IRPS'25 构成测试生成四路线对照**——SiliFuzz=fuzzing 代理(硬件无关)、OpenDCDiag=库移植(硬件无关)、Harpocrates=硬件在环迭代(微架构仿真反馈)、Vega=物理机理+形式验证(网表级,唯一从根因物理出发);**本篇对 OpenDCDiag 的定性:"Intel 在 zlib 与 eigen 等流行库之上设计 OpenDCDiag"(§1)——再次印证 sdcshield 父项目的"库移植"路线归类**;[75]=[01] SOSP'23(Alibaba)、[69]=[25] SiliFuzz、[52]=Hochschild HotOS'21、[37]=Dixit 2102.11245、[36]=[03] Ripple、[23]=Bacon exabyte=[39] exabyte-db;**[50] He et al. ISCA'23(深度学习训练系统硬件失效)为 R7 批次潜在相关文献**;老化根因与 [27] §VII 的"marginal defect 为制造测试最难"论述互补——[27] 面向边缘缺陷的连续筛查需求,本篇给出老化子类的完整技术栈;检测频率论(3 个月→秒级)与 [03] Ripple(在产周期快扫)的生命周期定位同构,但 Vega 把检测嵌入应用运行时而非独立扫描进程;**Kasikci(UW+Google)是本集 Google 网络的新节点——与 [25] SiliFuzz(Google 5 人)、[03] Ripple(Meta)构成"学界-产业人才旋转门"维度的实例**;Intel TSA 中心资助与 [32] strategies-detect(Intel)潜在呼应。
- 身份核实:标题"老化相关 SDC 的前瞻运行时检测:自下而上方法"与内容(三阶段网表级工作流+CV32E40P 实证)完全相符;ASPLOS '24、DOI 10.1145/3622781.3674182、印刷页 220–235 齐全,名实相符;"Volume 4"为 ASPLOS'24 多卷本文集之卷号,如实记录;作者 7 人 5 机构(跨美以两国,Kasikci 双聘含 Google)。**文本层已知损失**:(1) **式(1) 物理符号全部丢失**(ΔVth=(α·T/eU)^(1/6) 类符号在文本层为空格——反应-扩散模型的标准形式由上下文重构,标"重构");(2) **式(2)(3) 的 β/ε 符号丢失**("β is either held at 0, 1…"的 β 由 §3.3.1 上下文重构为失效模型常量错值);(3) Fig.9 x 轴 19 个基准名连排乱码(aha-mont64…wikisort);(4) Fig.8 柱状图数值由散文完整给出;(5) 末页(p.17)空白;(6) **无内部矛盾**——73.5=63.9+9.6 ✓;ALU:6 对×66.7%≈4 对×2 用例=8 ✓、33.3% UR=6×(1/3)=2 对(2×2=4 用例被排除,8=12-4 自洽);FPU:41 对×51.2%≈21 对×2=42 ✓、41×40.2%≈16.5×4=66(缓解版)✓、51.2+43.9+4.9+0=100 ✓;ALU 缓解版 6×33.3%=2 对×4=8 ✓;Table 6 与 Table 5 用例数对应;检测率与 §5.2.3 叙述一致。

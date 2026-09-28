# [10 的会议完整版][11] A Systematic Methodology to Compute the Architectural Vulnerability Factors for a High-Performance Microprocessor(MICRO-36,2003-12;会议论文)

- PDF: `A Systematic Methodology to Compute the Architectural Vulnerability Factors for a High-Performance Microprocessor.pdf`;页数 13(实读 p.1–12,末页空白;每页页眉 "To appear in the Proceedings of the 36th Annual International Symposium on Microarchitecture (MICRO), December 2003")
- 作者(全列):Shubhendu S. Mukherjee、Christopher Weaver、Joel Emer、Steven K. Reinhardt、Todd Austin(共 5 人,与 10 完全同队)
- 机构(学术/企业分列):
  - 学术:密歇根大学 Advanced Computer Architecture Lab ×2(Weaver、Reinhardt 双挂;Austin 唯一全职学术署名)
  - 企业:Intel Corporation VSSAD/MMDC(麻省 Shrewsbury)×4(Mukherjee/Weaver/Emer/Reinhardt)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Intel ×4(p.1 署名页明文 1=VSSAD, MMDC, Intel Corporation)
  - 二级(致谢章节资助/数据/设备):致谢(p.11–12)感谢 **VSSAD 成员、Intel 可靠性专家、Intel Asim 组(性能模型支持)、Intel SoftSDV 组(协助在 Asim 模型上启动 OS 镜像)**——产业内部研发基础设施的完整支撑链
  - 三级(首页脚注资助声明):无(会议版无资助脚注)
  - 间接层:§2 引 IBM Power4 系统错误预算(SDC 系统 MTBF 1000 年/DUE 25 年/10 年,Bossen RPS02 教程)、Sun 2000 宇宙射线宕机流失大客户给 IBM、Fujitsu SPARC 80% 锁存器保护——产业事件链与 10 相同
- 核心结论(各带节号/图表号):
  1. **业界可靠性度量基座(§2.1)**:厂商以 MTBF 表达参考海拔错误预算;SDC=未检出错误,DUE=检出不可恢复(检出可恢复不算错误);FIT 可加和故设计者常用;IBM Power4 目标——SDC 系统 MTBF 1000 年(=114 FIT)、DUE 崩溃 25 年、应用崩溃 10 年;latch/SRAM 原始率 0.001–0.01 FIT/bit(海平面),预计数代持平除非激进降压;**"今天逻辑门的总 FIT 贡献相比锁存器可忽略"——只考虑锁存器与 SRAM 单元**
  2. **AVF 对保护经济性的定量演示(Fig.1)**:2003 年假设 200,000 位暴露于宇宙射线、FIT/bit=0.001;2005 年达 IBM 114 FIT 目标——100% AVF 需保护 80% 位,10% AVF 则一位都不用保护;2010 年 10% AVF+80% 保护仍达标而 100% AVF 不可达——脆弱性评估直接决定保护预算数量级
  3. **un-ACE 位完整分类学(§3)**:微架构级四类(idle/无效、误推测、预测器结构、ex-ACE)+架构级五源(NOP、性能增强指令、谓词假、动态死代码 FDD/TDD、逻辑掩蔽);输出定义讨论——I/O 送出值为准,调试器观察变量也算输出,多处理器竞态按具体执行判定(本文仅单处理器)
  4. **指令流 55% 是 un-ACE(Fig.2,§6.1)**:平均 45% ACE 指令;NOP 26%(IA64 三指令 bundle 对齐引入)、谓词假 6.7%、prefetch 1.5%、FDD_reg 9.4%、FDD_mem 2%、TDD_reg 6.6%、TDD_mem 1.6%(IA64 大寄存器堆致 FDD_reg 显著高于内存侧);UNKNOWN+NOT_PROCESSED 约 1%
  5. **指令队列 AVF 平均 28%(§6.2,Fig.3)**:范围 14%–47%;平均 30% 周期 idle、42% 含 un-ACE 位;FP 程序 31% > 整数 25%(长延迟指令多、分支误预测少、队列使用更充分);每条目约 100 bit(41 bit IA64 指令+飞行态位),5 控制位不可 derate
  6. **Little 定律指令级近似的偏差解剖(Table 2,§6.2)**:指令级近似平均 19% vs 实际 28%——差 9% 源于 un-ACE 指令(prefetch/死代码)携带的 ACE 位(如 opcode)未计;比特级分析可弥合;lucas(高 ACE IPC×低 ACE latency)≈ammp(低×高)案例说明 AVF 是带宽×驻留的乘积
  7. **执行单元 AVF 平均 11%→9%(§6.3,Fig.4)**:范围 4%–27%;显著低于指令队列三因——指令在队列等待时间远超执行时间、cache-miss 重放仅最后一遍为 ACE、整数代码期 FP 管线空闲;再经逻辑掩蔽(−0.5%)与数据通路空闲(−1.5%,如 IA64 compare 双谓词位走 64 位总线 62 道空闲)derating 至平均 9%
  8. **对 RTL 统计注入的四优势(§7)**:单次实验出确定性估计(统计注入需大量样本);经架构寄存器/内存跟踪正确判死值/掩蔽(注入法只在固定周期后比对架构态快照);给出 un-ACE 原因分解的行为洞察;性能模型在架构探索期可得(RTL 通常没有)——架构师可在 RTL 开发前预判保护需求
  9. **设计流程(§8)**:大结构+高 AVF 为保护首选;奇偶/ECC 保护位不再贡献 FIT;以 AVF 为导航迭代压低整芯片 FIT 率
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**单粒子翻转(宇宙射线中子+封装 α 粒子)**——2003 辐射软错误范式;海拔效应(Denver 1.5km=海平面 3–5×)
  - 故障模式类型:未保护锁存器/SRAM 单比特翻转→SDC 或 DUE(检测使 SDC→DUE 转化);**逻辑门贡献当时可忽略**(历史口径,与 04/05/18 的计算结构焦点形成时代对照)
  - 检测技术类型:**设计时脆弱性评估方法论**(ACE 位分析三部件算法:驻留时间记录/提交后分析窗口 FDD-TDD-掩蔽判定/期末 AVF 汇总);40,000 指令分析窗口
  - 处理技术类型:奇偶/ECC 选择性投放(大结构高 AVF 优先);迭代式 FIT 压低;架构冗余谱系(DIVA/AR-SMT/冗余多线程/IBM S390 G5/Compaq Tandem 白皮书 [26])
- 业界观点摘录(Intel 主导论文,摘原话):
  - "IBM 为其 Power4 处理器系统设定目标:SDC 错误系统 MTBF 1000 年、致系统崩溃的 DUE 25 年、致应用崩溃的 DUE 10 年"(§2.1)
  - "今天逻辑门的总 FIT 贡献相比锁存器的 FIT 贡献是可忽略的一小部分,所以我们只考虑锁存器与 SRAM 单元上的击中"(§2.1)
  - "除非微处理器为降低总功耗而激进降压,FIT/bit 预计在未来数个技术世代保持在 0.001–0.01 区间"(§2.1)
  - "检测可恢复错误不算错误……给结构加错误检测(非纠正)即消除 SDC,把故障转为 DUE"(§2.1)
  - Sun 2000/IBM/Fujitsu 产业事件同 10(p.1)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | IBM Power4 预算 | SDC 1000 年系统 MTBF(=114 FIT)/DUE 崩溃 25 年/应用崩溃 10 年 | §2.1 |
  | 原始 FIT 率 | latch/SRAM 0.001–0.01 FIT/bit(海平面);逻辑门可忽略 | §2.1 |
  | 保护需求投影 | 2005 年 114 FIT:100% AVF 需保护 80% 位/10% AVF 零保护;2010 年 100% AVF 不可达 | Fig.1 |
  | 指令流分解 | 45% ACE/55% un-ACE;NOP 26%/谓词假 6.7%/prefetch 1.5%/FDD 11.4%/TDD 8.2% | Fig.2, §6.1 |
  | 指令队列 AVF | 平均 28%,范围 14%–47%(§8 另给 14%–40%,见身份核实);FP 31% vs 整数 25% | §6.2, Fig.3 |
  | Little 定律近似 | 指令级平均 19% vs 实际 28%(差 9%=un-ACE 指令的 ACE 位) | Table 2, §6.2 |
  | 执行单元 AVF | 平均 11%(范围 4%–27%)→derating 后 9%(§8 另给 2%–17%) | §6.3, Fig.4 |
  | 指令队列参数 | ~100 bit/条目/64 条目/5 控制位不可 derate | §6.2 |
  | 动态死指令跨架构 | IA64 12% FDD+8% TDD;Alpha 9%+3%(仅寄存器)或 14%(寄存器+内存);Itanium 退休指令 27% NOP | §3.2 |
  | 仿真设置 | Itanium2 类 6 发射(4 整数+2 FP);Asim+SoftSDV 启动 RH Linux 7.2;SimPoint 首点×1 亿指令;Intel electron 编译器 v7.0 最高优化 | §5 |
  | 基准 | Table 1 列 26 个(12 整数+14 浮点);§3.2 另称 18 个(见身份核实) | Table 1, §3.2 |

- 方法论要点:保守上界(默认 ACE)+尽力枚举 un-ACE 的工程方法论;三部件 AVF 算法(驻留记录/提交后分析窗口/期末汇总);FDD/TDD 经寄存器与内存双通道跟踪(两连写无插读即死);逻辑掩蔽仅实现约 2000 种静态指令类型的小子集(OR/AND 等)+估计另 20% 无直接掩蔽;微基准汇编验证分析窗口正确性(注入已知数量死指令→检出数匹配);诚实局限:性能模型只含影响性能的组件、未做传递性逻辑掩蔽、未 derate IA64 load 提示位。
- 横向对比注记:**与 10 为同一 Intel-Michigan 团队的孪生发表**(10=IEEE Micro Top Picks 杂志版,11=MICRO-36 会议完整版;10 引本篇为其 [12])——AVF 理论谱系(R2 批次 10–18)的奠基双文献;后续 12(地址类结构)、13(一阶力学模型)、14(硬故障)、15(DelayAVF)、16(MeRLiN)、17/18(跨层/门级)全部立于 SDC/DUE 二分+ACE/un-ACE+Little 定律这套概念基座之上。**"逻辑门 FIT 可忽略"(2003)↔ 04(Veritas 算术单元门级建模)/05(SEVI 向量单元)/18(从门到 SDC)——研究焦点二十年间从存储结构转向计算结构的历史性转移,综述演进趋势章的核心对照**;IBM Power4 114 FIT 目标+Fujitsu 80% 锁存器保护是"芯片厂商时代"可靠性工程实证,与 2020 年代云厂商 fleet 研究(01/02/07/09)构成产业主导权转移叙事;Intel 内部支撑链(VSSAD/可靠性专家/Asim 组/SoftSDV 组)是产业研发基础设施的致谢级证据;Wang & Patel Alpha 21164、Kim & Somani picoJava II 的 RTL 注入对照延续 10 的讨论;08(雅典组)§I 批判的"[1]–[4] 辐射软错误范式"即本文所处时代立场。
- 身份核实:标题"计算高性能微处理器 AVF 的系统化方法"与内容(un-ACE 分类学+AVF 算法+Itanium2 类实测)完全相符;每页 MICRO-36 December 2003 页眉齐全;无 DOI/版权行(会议 to-appear 版式),按会议论文标注,名实相符。**文本层已知损失与内部口径不一致**:(1) **摘要/§6 与 §8 的范围口径不一致**——摘要给平均 IQ 28%/EU 9%;§6.2 给 IQ 范围 14%–47%、§6.3 给 EU 范围 4%–27%(再 derating 后平均 9%);而 §8 结论给 IQ 14%–40%、EU 2%–17%——疑 §8 为全 derating 后范围,但未明说;10(Top Picks 版)采 §6 前值口径(14–47%/4–27%);引用时统一采用摘要+§6 口径并注明 §8 差异;(2) §3.2 称 FDD/TDD 评估基于"18 个 SPEC2000 基准",而 Table 1 与 Fig.2–4 均为 26 个(12 整数+14 浮点)——疑 18 为该特定统计的子集,按原文各自引用;(3) Fig.2/3/4 为图像,百分比引自正文散文与表格叙述;(4) Table 1 "gzip-graphic 2,9000 M" 文本层乱序(应为 29,000 M);(5) Fig.2 纵轴刻度(0.9/0.8/…)与基准名交错,数值以散文为准;(6) 无资助脚注(三级证据如实标无)。

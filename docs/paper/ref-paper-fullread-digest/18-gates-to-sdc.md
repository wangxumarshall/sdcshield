# [18] From Gates to SDCs: Understanding Fault Propagation Through the Compute Stack(DATE 2025;文本层 8 页,7 页正文+参考文献,末页空白)

- PDF: `From_Gates_to_SDCs_Understanding_Fault_Propagation_Through_the_Compute_Stack.pdf`;页数 8(实读全部:p.1–7 正文+参考文献,p.8 空白;2025 Design, Automation & Test in Europe Conference,978-3-9826741-0-0/DATE25/© 2025 EDAA,IEEE Xplore 版)
- 作者(全列):Odysseas Chatzopoulos、George Papadimitriou、Dimitris Gizopoulos、Harish D. Dixit、Sriram Sankar(共 5 人)
- 机构(学术/企业分列):
  - 学术:雅典大学信息与电信系 ×3(Chatzopoulos/Papadimitriou/Gizopoulos——**Gizopoulos 组,与 [08]/[16]/[17] 同组**)
  - 企业:**Meta Platforms Inc(Menlo Park,California)×2**(Dixit/Sankar——**即 Meta"Silent Data Corruptions at Scale"(arXiv 2102.11245,本文 [1])原班作者,亦为本集 [04] Veritas(HPCA'25)的共同班底**)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):**Meta ×2**(p.1,hdd/sriramsankar@meta.com)
  - 二级(致谢章节资助/数据/设备):致谢(p.6)——**OCP(Open Compute Project)、Meta、AMD 三方 research gifts**;欧盟 Horizon Europe(Vitamin-V 101093062 / NEUROPULS 101070238 / REBECCA 101097224);希腊 HFRI 16973(REDESIGN)
  - 三级(首页脚注资助声明):无(资助在致谢)
  - 间接层:[1] Meta SDC at Scale;[2] Google Cores That Don't Count(HotOS'21);[3] Alibaba TACO'24;[4] ITC'24 keynote(Hesley);[20] AMD Gurumurthi/Sridharan"Emerging fault modes"SIGARCH 博文;[35] **OpenDCDiag(Intel 开源工具,直接作为基准来源)**
- 核心结论(各带节号/图表号):
  1. **SDC 非瞬态故障共识 + 算术单元无保护缺口(§I/§II)**:超大规模厂商(Meta/Google/Alibaba)报告**约 1/1000 CPU** 产生 SDC(硅缺陷,逃逸硬件/系统/应用层检测);"CPU 的 SDC 问题并非瞬态故障所致已是共识 [1][2][17][18]"——聚焦永久/间歇故障;整数 ALU(加法器/乘法器)通常无保护、使用频率最高,且同时影响控制流(条件/指针计算)与数据流;cache/大 buffer/队列等阵列通常有检测/纠正保护、"更不可能贡献 SDC"(§V)
  2. **混合注入方法(§III.A,五步)**:gem5 微架构级仿真 + 功能单元门级注入——(1) C++ 门级模型自动生成(基于 ArithsGen [30]);(2) 任意门可注入 [31];(3) **smart hybrid 集成进 gem5:吞吐损失 <4%**(旧外挂门级仿真器方案每次调用暂停切换,损失 >2× [19]);(4) 定制注入控制器;(5) 并行注入战役管理器——兼得门级硬件/故障建模精度与程序仿真速度,程序跑到底以观测 SDC(软件层硬件无关、纯门级极慢,两层都做不到)
  3. **六个研究问题(§I)**:门级故障传到 FU 输出的概率;FU 输出 BER;Crash/SDC/掩蔽概率;BER 与最终程序结果的相关性;错误位数的影响;单指令错误率及其与结果的相关性
  4. **实验设置(§III.A/B/§IV,Table I)**:每组件 500 门×stuck-at-0/1=**1000 注入/FU/基准**;17 基准(14 MiBench + 3 个 OpenDCDiag Linux 库基准 [35]);x86-64 OoO(PRF 192 Int/160 FP;LQ/SQ/IQ/ROB 44/48/148/256;标量 Int 5 Add+1 Mul、标量 FP 2 Add+2 Mul;向量 Int 4 Add+2 Mul、向量 FP 2 Add+2 Mul;L1 I/D 各 32KB 8-way;L2 512KB 8-way);加法器=64 位超前进位(4 位 CLA 块)、乘法器=64 位 Dadda 树(脚注 1);**>100,000 次全系统仿真**(6 FU×1000×17=102,000,自洽),跑完整个含 Linux OS 的全系统(gem5 ~1M 指令/秒)
  5. **观察 #1(§IV.A,Fig.1)**:故障传播至 FU 输出(至少一次出错):加法器 **90%–98%**、乘法器 **65%–98%**——门级 stuck-at 在加法器更易传播;掩蔽强烈依赖 workload 与调度(二者决定输入值);Sha 两实现掩蔽最小(哈希大量使用加法器且被哈希数据随机,每次计算都重要)
  6. **观察 #2(§IV.B,Fig.2)**:BER——加法器跨基准均匀(0.035–0.055,最重用 FU、输入流恒定);乘法器仅 untoast/jpegc 两基准高达 ~0.06,其余 ~0.01(乘法器利用率低→输入多样性不足)
  7. **观察 #3(§IV.C,Fig.3)**:加法器故障结果以 **Crash 为主(>80%)**——其处理的数据(指针、数组与循环索引、栈地址)损坏几乎必然崩溃;SDC 0%–18%(Sha 两实现最大);掩蔽低且外部掩蔽(超出 FU 边界,微架构或软件层)更常见。乘法器:Crash 30%–65%;SDC 5%–20%(bitcount/fft inv/fft/untoast/jpegc);掩蔽大得多——内部掩蔽归因更深的树结构,外部掩蔽归因软件对乘积的用法(64×64 位乘法高 64 位常被丢弃)
  8. **观察 #4(§IV.D,Fig.4)**:加法器按结果分的 BER 分布——Crash(主导)跨宽 BER 范围(均值 0.057/σ 0.15/最大 0.87);SDC/Masked 窄且低(均值 **2.2×10⁻⁴/4.4×10⁻⁴**,最大 0.03/0.063)——**加法器要产生 SDC 或被掩蔽,BER 必须极低**;乘法器三类分布皆散(Crash/Masked 均值 0.12,SDC 均值 0.05/最大 0.41)——结果与 BER 相关性显著更低
  9. **观察 #5(§IV.D,Fig.5)**:错误位数效应——加法器 SDC 随错误位数增加稳定下降,**25–52 位区间明显回升**;乘法器 SDC 在 **17–64 位区间上升**,模式更均匀
  10. **观察 #6(§IV.E,Fig.6)**:单指令错误率(用错数据的该类指令数/该类指令总执行数,脚注 2)——SDC 结果的指令错误率平均更低;控制流与栈管理指令(ret/call/push/pop/leave/sysret)在 SDC 中错误率极低甚至缺席——**其损坏几乎总是触发段错误或内核崩溃,从不产生 SDC**
  11. **结论与定位(§V/§VI)**:细粒度发现可用于硬件/软件层面知情的故障检测与容忍策略;阵列结构(瞬态)已被 [5]–[15] 充分研究且如今大多受保护;算术单元"几乎无保护且被近期报告 [1] 与 SDC 关联";[19](Li et al., HPCA'09)为早期 ALU 建模先驱,但外挂门级仿真器损失 >2×、统计模型损失精度——本篇 smart hybrid 是其技术兑现
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**制造缺陷/磨损类永久故障(门级 stuck-at-0/1,作为最坏情形注入)**——背景明确"SDC 非瞬态"共识 + 边缘缺陷(marginal defects,温度/workload 条件性)致间歇故障(§II);**作用部位为算术单元(组合逻辑)——非 SRAM 阵列,本集 R2 谱系中唯一门级组合逻辑篇**
  - 故障模式类型:SDC/Crash/Masked 三分,掩蔽再分 Internal(FU 输出无错)/External(输出有错但不影响最终输出);**数据类型决定模式**:指针/索引→Crash 主导、哈希→SDC 偏好、乘积高位丢弃→外部掩蔽;控制流与栈管理指令"从不 SDC"模式
  - 检测技术类型:无直接机制(分析论文;发现"可用于指导检测/容忍策略设计"——如按指令类别/数据类型差异化布防)
  - 处理技术类型:无(方法论/发现论文)
- 业界观点摘录(本篇有业界作者——Meta Dixit/Sankar 署名,直接引):
  - "超大规模数据中心……带来一个关键问题:CPU 中 SDC 发生率出人意料地高。这些错误由大约千分之一的 CPU 产生,源于硅缺陷,并逃逸硬件、系统或应用层的错误检测机制"(§I,p.1)
  - "CPU 的 SDC 问题并非源于瞬态故障,这已是共识"(§I,引 [1][2][17][18])
  - "尽管算术单元中的瞬态故障较少见 [16],超大规模云厂商报告的出人意料的高 SDC 率凸显了紧迫需求:必须应对其中其他类型的故障"(§I)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | SDC 发生率 | ~1/1000 CPU(Meta 等超大规模厂商) | §I |
  | FU 输出传播率 | 加法器 90%–98%;乘法器 65%–98% | §IV.A, Fig.1 |
  | BER | 加法器 0.035–0.055(均匀);乘法器 ~0.01(untoast/jpegc ~0.06) | §IV.B, Fig.2 |
  | 加法器结果分布 | Crash >80%;SDC 0%–18% | §IV.C, Fig.3 |
  | 乘法器结果分布 | Crash 30%–65%;SDC 5%–20% | §IV.C, Fig.3 |
  | BER×结果(加法器) | Crash 均值 0.057/σ 0.15/最大 0.87;SDC 均值 2.2×10⁻⁴/最大 0.03;Masked 均值 4.4×10⁻⁴/最大 0.063 | §IV.D, Fig.4 |
  | BER×结果(乘法器) | Crash/Masked 均值 0.12;SDC 均值 0.05/最大 0.41 | §IV.D, Fig.4 |
  | 注入规模 | 500 门×2=1000 注入/FU/基准;>100,000 全系统仿真(6×1000×17=102,000 自洽) | §III.A, §IV.C |
  | 混合集成开销 | 吞吐损失 <4%;外挂门级仿真器方案 >2× [19] | §III.A |
  | 微架构 | x86-64 OoO:PRF 192Int/160FP;LQ/SQ/IQ/ROB 44/48/148/256;标量 Int 5Add+1Mul、FP 2Add+2Mul;向量 Int 4Add+2Mul、FP 2Add+2Mul;L1 32KB×2 8-way;L2 512KB 8-way | Table I |
  | FU 电路 | 加法器 64 位 CLA(4 位块);乘法器 64 位 Dadda 树 | 脚注 1 |
  | 基准与仿真 | 14 MiBench + 3 OpenDCDiag 库基准;gem5 ~1M 指令/秒;stuck-at 永久故障 | §III.A/B, §IV |

- 方法论要点:门级模型内嵌微架构仿真器(smart hybrid,<4% 开销)解决"精度 vs 速度"两难——软件层硬件无关、纯门级慢到无法跑完程序观测 SDC;永久 stuck-at 作为最坏情形;全系统 Linux 跑到底再分类(Crash=程序或内核崩溃;SDC=跑完但输出损坏;Masked 分 Internal/External);指令错误率指标设计;并行注入战役管理;研究边界诚实——只注入标量整数 FU(5 加法器+1 乘法器),向量 FU 列于配置未注入;阵列与算术单元的责任划分有明确文献定位。
- 横向对比注记:**AVF 谱系第八篇——门级/制造缺陷范式收束篇**:10/11 奠基(辐射瞬态)→12 地址类→13 一阶模型→14 硬故障(明言"AVF 不适用硬故障与组合逻辑"——本篇恰以门级组合逻辑回应)→15 延迟故障→16 注入加速→17 跨层批判→**18 门级算术单元永久故障**——2003 辐射范式到 2025 制造缺陷范式的完整闭环;**雅典组×Meta 轴心**:Chatzopoulos/Papadimitriou/Gizopoulos 与本集 [04]/[08]/[16]/[17] 同组,Dixit/Sankar 即 Meta "SDC at Scale" 原班作者([1])并共同署名 [04] Veritas;**本文参考文献 [5][8][17][26][31][33][38][39] 分别对应本集 [08][17][01][49][04][22][50][29]——单篇引用本集 8 篇,跨集引用网络之最**;Crash>80%(加法器)与 [17]"指针/索引损坏→Crash 主导"互证;SDC 0–18%/5–20% 与 [16] L1D 15–20% 同量级但结构类型相反(组合逻辑 vs SRAM);"阵列大多受保护"与 [17] Cho 批判(Exynos 5250 无 ECC)存在张力——"大多"≠必然;**[35] OpenDCDiag 直接作为基准来源——本仓库(sdcshield)上游工具与学术研究的直接交汇点**;[19] Li HPCA'09 外挂门级方案的 16 年后技术兑现;1/1000 CPU 与 [01](SOSP23)/[04](Veritas)fleet 数据一脉相承。
- 身份核实:标题"从门到 SDC:理解故障穿越计算栈的传播"与内容(门级 stuck-at 注入→FU 输出→程序结果三分类 + 六个观察)完全相符;DATE 2025、978-3-9826741-0-0/DATE25/© 2025 EDAA 齐全,名实相符。**文本层已知损失**:(1) **Fig.1–6 仅存图注,图内数据标签完全不在文本层**——全部引用数值取自散文(观察 #1–#6 段落给出所有关键区间,散文自足);(2) 作者行分隔符编码乱码("�")——5 人姓名与两行机构归属清晰可辨;(3) 参考文献少量编码乱码([30]"J. Klhufek"等按原样转录不改);(4) 末页(p.8)空白;(5) IEEE Xplore 下载戳(Shanghai Jiaotong University,2026-06-29)为访问来源标记,非内容;(6) 无内部矛盾——散文六观察数值区间与图注/Table I 一致;"6 个整数功能单元"(5 加法器+1 乘法器)与 Table I"5 Add;1 Mul"一致;注入总数 6×1000×17=102,000 与">100,000"算术自洽。

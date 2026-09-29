# [22] gem5-MARVEL: Microarchitecture-Level Resilience Analysis of Heterogeneous SoC Architectures(HPCA 2024;印刷页 543–559,21 页正文)

- PDF: `Gem5-MARVEL_Microarchitecture-Level_Resilience_Analysis_of_Heterogeneous_SoC_Architectures.pdf`;页数 22(实读全部:p.1–21 即印刷页 543–559,末页空白;2024 IEEE International Symposium on High-Performance Computer Architecture,DOI 10.1109/HPCA57654.2024.00047,979-8-3503-9313-2/24)
- 作者(全列):Odysseas Chatzopoulos、George Papadimitriou、Vasileios Karakostas、Dimitris Gizopoulos(共 4 人)
- 机构(学术/企业分列):
  - 学术:雅典大学信息与电信系 ×4(邮箱全为 @di.uoa.gr,单一机构栏无歧义)——**Gizopoulos 组;Chatzopoulos 即 [18](DATE'25 门级注入)一作,本篇为其 Meta 合作前的工具奠基作**
  - 企业:无(作者层)——**但致谢列 AMD/Meta 研究赠礼(见下),属雅典-Meta-AMD 轴心的资金级合作**
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):无
  - 二级(致谢章节资助/数据/设备):致谢(p.556/印刷页,§ACKNOWLEDGEMENTS)——欧盟 Horizon Europe **NEUROPULS(101070238)+ REBECCA(101097224)** 双项目 + **AMD 与 Meta 研究赠礼(research gifts)**;"观点仅属作者"标准免责声明——**与 [18] 的资助组合(NEUROPULS+REBECCA+Vitamin-V+OCP/Meta/AMD 赠礼)高度重叠,轴心延续性铁证**
  - 三级(首页脚注资助声明):无
  - 间接层:动机引数据中心 CPU+加速器宕机成本(§I);RISC-V 生态多厂商引用(DE-RISC 航天级平台 [32]/安全关键 [33])
- 核心结论(各带节号/图表号):
  1. **首个异构 SoC 一体化微架构级注入框架(§I/§III,Table I)**:gem5(CPU 侧)+ gem5-SALAM(LLVM 动态图执行引擎建模 DSA 侧)合体——覆盖三主流 64 位 ISA(x86/Arm/RISC-V)+ DSA(SPM/RegBank)+ 瞬态/永久故障 + AVF/HVF 双度量;Table I 对照七个既有工具(FIMSIM/GeFIN/MaFIN/GemFI/Thales/Fidelity/LLFI/LLTFI/gem5-Approxilyzer)均不全覆盖
  2. **gem5-SALAM 的 RISC-V 全系统移植(§III.C)**:Arm GIC→RISC-V PLIC 中断系统翻译 + 配置脚本生成器模板替换 + 加速器 MMIO 地址并入 RISC-V 平台地址空间——DSA 仿真从 Arm 独占扩展到双 RISC ISA
  3. **注入与分类(§IV.A,Table III)**:瞬态(单周期翻转)+ 永久(置 0/1)故障,支持多比特(结果仅单比特);AVF 战役三类 Masked/SDC/Crash;**HVF 战役二类 Masked/Corruption(提交级不匹配:指令/操作数/数据事务/程序序)**;同一故障掩码可双度量复用→故障传播路径细粒度关联(据其所知独有能力)
  4. **实验规模(§III.D)**:15 MiBench ×3 ISA ×5 CPU 结构(整数 PRF/L1I/L1D/LQ/SQ)+ 8 MachSuite 加速器设计(BFS/FFT/GEMM/MD KNN/MERGESORT/SPMV/STENCIL2D/STENCIL3D)注入其 SPM/RegBank;每结构 1000 次均匀单比特故障(Leveugle 法:3% 误差/95% 置信)→总计近 **250,000 次注入**(CPU 侧 5×15×3×1000=225,000 + DSA 侧组件≈19×1000,自洽);三 ISA 用**同一 OoO 微架构**(8 发射,PRF 128int/128fp,LQ/SQ/IQ/ROB 32/32/64/128,L2 1MB 8 路,Table II)以隔离 ISA 变量
  5. **ISA × 结构脆弱性(§V.B,Fig.4–8,Observations #1–#4)**:整数 PRF——RISC-V 显著最脆弱(AVF 5.1%–20.8% vs Arm 6%–14%/x86 4.7%–13.2%);L1I——RISC-V 最低(16.4%–34.9%,归因编码逻辑简单→更高掩蔽)、Arm 最高;L1D——Arm/RISC-V 高于 x86(4.3%–44.9%/5.9%–40.9% vs 3.4%–35.1%,归因 x86 复杂指令→更精细访存模式→更高硬件掩蔽);LQ/SQ——Arm 最低(内存序模型利于队列韧性);**附显式免责:结论仅适用于该微架构配置与负载集,不构成 ISA 一般性排序**(§V.B 开头+§VI 重申)
  6. **SDC 对 AVF 的贡献分解(§V.C,Fig.9–11,Observation #5)**:PRF——SDC wAVF 比 AVF 低 4.6×(Arm)/5×(x86)/4×(RISC-V)→**Crash 主导**(寄存器错误值→非法访存);L1I——低 9×/11.2×/17×→**Crash 压倒性主导**(指令块损坏→非法指令);L1D——SDC wAVF 20.6% vs 23.8%(Arm)/13.7% vs 17.1%(x86)/17.8% vs 21.7%(RISC-V)→**SDC 主导**(数据缓存损坏易直达输出)
  7. **永久故障的 SDC 概率(§V.D,Fig.12/13)**:L1I 永久故障 SDC 概率极低(0.1%–2.7% 三 ISA,x86 最低/RISC-V 最高);L1D 永久故障 SDC 概率高达 **4.4%–70.8%**,RISC-V 显著最高——**永久故障比瞬态更"沉默"** 的一贯图景
  8. **DSA 故障模式二分(§V.E,Fig.14,Table IV,Observation #6)**:BFS EDGES RegBank AVF 35%/NODES 20% 且几乎全 Crash(**索引型数据**→图遍历越界/超长执行);FFT IMG/REAL SPM 44.5%/45.1% 且**全为 SDC**(**纯数据型**→不经控制逻辑直染输出);GEMM 输出 SPM(MATRIX3)显著低于输入 SPM(全程持续覆写);MERGESORT TEMP 低于 MAIN(同理);**加速器=数据通路面密集、控制依赖稀少→SDC 率普遍极高,缓解策略应聚焦数据损坏而非控制流**(Architectural Implication #6)
  9. **PRF 规模敏感性(§V.F,Fig.15)**:物理寄存器 96→128→192:越少越脆弱(单寄存器利用率与复用率升高),三 ISA 一致
  10. **OPF 性能-可靠性合一度量(§V.G,Fig.16,Observation #7)**:**OPF=OPS/AVF**(失效前执行的操作数)——四算法(GEMM/BFS/FFT/KNN)DSA 的 AVF 均显著高于 RISC-V CPU,但 **OPF 均显著更高**——加速器虽更脆弱,却在失效前完成更多正确执行,性能-可靠性折衷更优
  11. **加速器设计空间探索(§V.H,Fig.17,Observation #8)**:GEMM 加速器并行功能单元 32→16→8→4→2:单元越少 AVF 越显著上升(SPM 访问变慢→故障更易传播到输出);可据 AVF+性能+面积三元寻优
  12. **HVF≥AVF 恒等关系(§V.I,Fig.18)**:HVF 的 Corruption 恒高于 AVF(HVF 只到软件层入口,AVF 还要扣软件层掩蔽)——六基准 PRF/L1D 实证
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:根因无关(工具论文;注入模型为瞬态单比特翻转+永久置 0/1;背景为制造缺陷/老化磨损/辐射/电压调节)
  - 故障模式类型:**结构×模式规律的三工具三角验证收口**——PRF/L1I→Crash 主导、L1D→SDC 主导(与 [21] L1D SDC 3–5× 互证、[19] 位置×模式映射互证);**DSA 侧新规律:索引型数据→Crash vs 纯数据型→SDC 的二分**(BFS vs FFT/GEMM);永久故障→更高 SDC 概率(L1D 4.4%–70.8%);HVF/AVF 层级关系(Corruption⊇SDC+软件掩蔽)
  - 检测技术类型:无(注入工具;HVF 的提交级 Corruption 检测为框架内置机制)
  - 处理技术类型:无直接方案;**OPF 度量与"加速器缓解聚焦数据损坏"指导原则**为处理层设计输入;PRF 规模与并行功能单元数的脆弱性敏感度为架构级预防性设计杠杆
- 业界观点摘录(学术论文,业界联系为资金级;无立场引语):致谢 AMD/Meta 研究赠礼(§ACKNOWLEDGEMENTS);动机章引数据中心 CPU+加速器大规模部署的宕机/数据损坏成本(§I)——产业关切以问题动机形式呈现,无企业署名观点
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | 总注入规模 | 近 250,000(CPU 225,000+DSA ≈19,000) | §III.D |
  | 抽样精度 | 1000 次/结构=3% 误差/95% 置信(Leveugle 法) | §III.D |
  | ISA×结构 AVF 范围 | PRF:4.7%–20.8%;L1I:16.4%–38.2%;L1D:3.4%–44.9%;LQ:2.4%–12.9%;SQ:1.8%–12.0% | §V.B, Fig.4–8 |
  | L1D 的 SDC 主导 | SDC wAVF/AVF:20.6/23.8(Arm)、13.7/17.1(x86)、17.8/21.7(RISC-V) | §V.C, Fig.11 |
  | L1I 的 Crash 主导 | SDC wAVF 比 AVF 低 9×/11.2×/17×(Arm/x86/RISC-V) | §V.C, Fig.10 |
  | 永久故障 L1D SDC | 4.4%–70.8%(RISC-V 最高) | §V.D, Fig.13 |
  | DSA AVF | BFS EDGES 35%/NODES 20%(全 Crash);FFT IMG 44.5%/REAL 45.1%(全 SDC) | §V.E, Fig.14 |
  | PRF 规模敏感性 | 96/128/192 寄存器:越少越脆弱(三 ISA 一致) | §V.F, Fig.15 |
  | OPF 结论 | DSA 比 CPU 高 OPF(四算法一致,AVF 相反) | §V.G, Fig.16 |
  | 并行功能单元 | 32→2:AVF 显著上升 | §V.H, Fig.17 |
  | 微架构(Table II) | 64 位 OoO 8 发射;PRF 128int/128fp;LQ/SQ/IQ/ROB 32/32/64/128;L1 I/D 32KB 4 路;L2 1MB 8 路 | Table II |
  | 负载 | 15 MiBench ×3 ISA;8 MachSuite 加速器(BFS/FFT/GEMM/MD KNN/MERGESORT/SPMV/STENCIL2D/STENCIL3D) | §III.D |

- 方法论要点:同微架构跨 ISA 差分设计(隔离 ISA 变量)并对结论外推边界做显式免责声明(方法论诚实样板);每结构独立验证程序(Sanity Checking——L1D 验证程序实测 AVF 必须为 100%,Listing 1:-O0 编译+寄存器驻留+对齐数组+伪指令 m5_checkpoint/m5_switch_cpu 界定注入窗口);执行时间加权 wAVF 聚合;OPF=OPS/AVF 把纯可靠性度量与性能合并为单一可比量;HVF/AVF 同掩码双度量复用→单故障传播路径跨层追踪;checkpoint 同时保存微架构态与架构态(含 cache 数据)免长预热。
- 横向对比注记:**R3 工具链 2024 年集大成节点,也是 [20] CHAOS 所引 [11]**——其工具景观表中 gem5-MARVEL 被标"closed-source":**本篇全文确无任何开源发布声明/工件链接/GitHub 仓库(仅引 gem5 官方库 [67]),与 CHAOS 表述相容;但本篇亦未自称闭源,严格记录为"无公开可用性声明"**;Table I 即 2015→2024 工具谱系快照(FIMSIM→GeFIN/MaFIN=[21]→GemFI=[19]→gem5-Approxilyzer→本篇);**[18](DATE'25)的 [33] 即本篇**——Chatzopoulos 在此工具上完成 Meta 合作前的能力建设,次年携 Meta 完成门级混合注入;资助组合(NEUROPULS+REBECCA+AMD/Meta 赠礼)与 [18] 重叠,轴心三阶段递进:赠礼(本篇)→联合致谢级([18] 的 OCP/Meta/AMD)→共同署名([18] 的 Dixit/Sankar);**L1D→SDC/L1I·PRF→Crash 与 [21](L1D SDC 3–5×)和 [19](位置×模式)跨十年三工具互证,构成综述"结构×故障模式"规律的最强证据链**;ISA 结论与 [23](ITC'23 跨 ISA,本篇 [15])同组互补——[23] 用舰队实测、本篇用受控注入;[64]=[24](Bodmann/Rech)被引为软错误评估方法论对话对象;DSA"索引型→Crash/纯数据型→SDC"二分与 [18] "数据类型决定模式"(指针/索引→Crash)在加速器域重现;加速器 SDC 主导+OPF 度量为 R7(AI/软件)与 R8(GPU/白皮书)批次的注入侧先导;RISC-V 三发现(PRF 最脆弱/永久故障 SDC 最高/生态安全化诉求)使其成为 ISA 演化叙事的当前前沿([20] CHAOS 2026 全面转向 RISC-V 的先声)。**企业合作维度:R3 批次首个资金级产业联系(AMD/Meta 赠礼)——注入工具层从纯公共资助([19][20][21])向产业赠礼过渡的转折点**。
- 身份核实:标题"gem5-MARVEL:异构 SoC 架构的微架构级韧性分析"与内容(gem5+gem5-SALAM 一体化注入框架+三 ISA 五结构+八加速器+AVF/HVF/OPF)完全相符;HPCA 2024、DOI 10.1109/HPCA57654.2024.00047、印刷页 543–559 齐全,名实相符;作者四人全 @di.uoa.gr 单一机构,无归属歧义。**文本层已知损失**:(1) **Fig.4–13/15–18 数据标签与基准轴标签严重乱码**(轴标签连排如"dijkstraebdagseiscmapthriccioarners…")——全部数值取自散文(Observations/Implications 段落给全范围值);(2) **Fig.14 十八个数值与十九个组件标签无法可靠配对**(数值块先于标签块)——DSA 数值仅引散文给出者(EDGES 35%/NODES 20%/IMG 44.5%/REAL 45.1%),GEMM/MERGESORT 按散文定性(输出<输入、TEMP<MAIN);(3) Fig.16 OPF 数值乱码——按散文定性(DSA OPF 更高);(4) **p.7–10 文本层近乎空白**(Fig.2/3 图形跨页排版)——按 §IV.C/§IV.D 散文重构流程描述;(5) **Table IV 组件-设计-内存类型行错位**(如"FFT NODES RegBank"应为 BFS 组件)——按散文重构(BFS:EDGES+NODES RegBank;FFT:IMG+REAL SPM;GEMM:MATRIX1+MATRIX3 SPM;MERGESORT:MAIN+TEMP SPM),其余设计组件集存疑不引;(6) Table III 行标签与描述块错位,按描述内容对应;(7) 版权/分隔符编码乱码("�");(8) 末页(p.22)空白;(9) IEEE Xplore 下载戳为访问来源标记;(10) 无内部矛盾——CPU 注入 225,000+DSA ≈19,000≈"近 250,000"自洽;HVF≥AVF 与定义一致;SDC wAVF<AVF 恒成立;散文数值与 Observation 文本一致。

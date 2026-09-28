# [06] ITHICA: Intra-Thread Instruction Checking Approach for Defect-Induced Silent Data Corruptions(arXiv 2605.15638,2026-05-15;预印本,未见会议页眉/DOI)

- PDF: `ITHICA Intra-Thread Instruction Checking Approach for Defect-Induced Silent Data Corruptions.pdf`;页数 16(实读 p.1–15,末页空白)
- 作者(全列):Ioanna Vavelidou、Subho S. Banerjee、Eric X. Liu、Mike Fuller、Subhasish Mitra、Caroline Trippel(共 6 人)
- 机构(学术/企业分列):
  - 学术:斯坦福大学 ×3(Vavelidou、Mitra、Trippel)
  - 企业:Google LLC ×3(Banerjee、Liu、Fuller)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Google ×3(p.1)
  - 二级(致谢章节资助/数据/设备):致谢(p.13):**NSF 资助 2321489 + Sloan Research Fellowship + Google、Meta、Open Compute Project 三方 gifts**;**研究载体为 Google 数百万服务器 fleet 与 >3000 台隔离服务器池(QPool)+ 20 台专测服务器(DPool)**(§5.2);基线测试为 Google 开源 cpu-check/Fleetbench(§5.1)
  - 三级(首页脚注资助声明):原文无(arXiv 预印本,无资助脚注)
- 核心结论(各带节号/图表号):
  1. **核心洞见:最棘手的缺陷(最可能逃逸制造测试者)产生"不一致错误"(inconsistent errors)——同线程内同指令、同架构输入,两次执行可产生不同架构输出,取决于执行上下文(微架构+电气状态)——首次在真实缺陷硬件上验证并利用于检测**(§1/§3,Fig.1);现有全部功能测试(cpu-check/OpenDCDiag/SiliFuzz/Harpocrates 等)隐含或显式假设"缺陷→一致错误",ITHICA 证明该假设不成立且限制可选测试程序范围(§1,Table 10)。RTL 教学示例:CVA6 rs1_valid_o stuck-at 永久故障→两次 MUL 输出不同(§3.2)。
  2. **ITHICA 方法**:LLVM IR 编译 pass 对任意程序插入线程内指令级检查(指令复制+输出比对);4 种变换——Arith(计算类)、Mem(load/store 复制回读)、**MemDiv(新颖的主动多样性:mfence/clflush 强制与不同内存层级交互)**、Br(条件分支目标校验);不依赖专有规范与故障模型,桥接产线实用测试与公开研究的鸿沟(§4,Table 1)。
  3. **规模与战绩:QPool >3000 台隔离服务器(≥10 微架构、2 大 CPU 厂商)+ DPool 20 台(每台 >2000h 测试,超既往详测研究 1–2 个数量级);共检出 100 台缺陷服务器——是 Alibaba 详测数(27–30)的 3× 余、SEVI(18)的 5× 余;ITHICA 检查比同二进制内 Native 终检多检出 39%,比 SiliFuzz 多 69%(SiliFuzz 42 vs CC-ITHICA 71,共同子集)**(§5.2/§7.1/§8,Fig.3/4/10)。
  4. **不一致错误确证(Finding 1)**:DPool 14 台全部被 CC-ITHICA 检出(Arith 独检 11/14);**24% 检出场合原指令与验证指令"双错且错得不同"——两者几乎必然命中同一缺陷部件却输出不同**,强约束了不一致的机理解释(Table 4);58 台双检服务器上 ITHICA 检出率 EDR 1.78×、错误频度 EF 7.51×、首检时间快 1.47×(§7.1,Table 5)。
  5. **执行上下文是错误显现的首要预测因子(Finding 5/6)**:多数失败 opcode 的 PC 敏感度 <1%(同 opcode 绝大多数程序位置表现正确);59% 场合(13/22)检出测试并非失败 opcode 执行频率最高者——**直接否定 Alibaba 的 instruction usage stress 检测预测与指令定位法**;31 台 Native 漏检服务器中 8 台仅被库代码内 ITHICA 检出(错误在到达库输出前被掩蔽);FB 生产负载借 ITHICA 检出 24 台(11 台唯一)——无 ITHICA 则 FB 根本无法作测试(无内建可检输出)(§7.3,Fig.7,Table 7/8)。
  6. **复现极其困难,短测试系统性偏差(Finding 8/9)**:固定输入下 TTD 仍高度可变→非架构态(微架构/电气)参与显现;**单指令与基本块级复现测试几乎全部检不出错(仅 D9 单指令可复现、D3 基本块可复现)——基于短测试的错误表征(如 SEVI)可能向"短序列有效"的少数案例偏倚**(§7.4,Fig.8);输入广度高——"完美输入"既非必要也非充分(Table 7)。
  7. **同一缺陷跨指令类型、速率差达 6 个数量级(Finding 10)**:44% 服务器(93 台中)错误跨多指令类型;92.9% 错误量来自 FP+向量指令,但 FP 受累服务器数(26)反少于内存类(28)——高错误频度可能反映易复现性而非普遍性;D1 仅被向量专用测试检出的案例中,ITHICA 揭示非向量指令同样出错——**挑战 SEVI"向量错误归因向量单元"的硬件定位结论**(§7.5,Fig.9,Table 9)。
  8. **硬件定位从 ISA 层本质上不可知**:缺陷可能在指令微架构执行路径任意处或物理相邻无关电路;编译器/硬件的多重映射引入非确定性——一切 ISA 级测试(含 ITHICA)皆受此限,错误定位结论(01 的 ALU/Cache/TrxMem、SEVI 的向量乘法器)应被审慎对待(§7.5)。
  9. **开销可调(Finding 4)**:Arith 开销 2.18×(块大小 1)→1.52×(块大小 8)而覆盖不损;Mem 1.17×、Br 1.17×、MemDiv 53.67×(个人级 Xeon 6226R 实测);缺陷永久性→每服务器检出一次即可,为低开销在线在产测试铺路(§7.2,Table 6)。
  10. **缺陷归因的排除法论证(Finding 3)**:频率+可重复性排除瞬态故障;设备特定+跨微架构排除设计 bug;跨年龄段(10–81 月)排除纯老化;剩余制造缺陷为最可能根因(§7.1)。
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**制造缺陷(排除法论证)**;永久故障经执行上下文(微架构+电气态)调制为不一致架构错误——"序列驱动的执行上下文"为显现首要因子,温度/使用强度/opcode/输入单独均不足
  - 故障模式类型:**不一致错误**(同输入不同输出;双错不同值 24%);序列依赖显现;PC/BB 敏感度低+输入广度高;**跨指令类型(44% 服务器,速率差至 6 个数量级)**;FP/向量贡献 92.9% 错误量
  - 检测技术类型:**线程内指令级检查**(指令复制+比对,LLVM 化的 EDDI/QED 谱系);**主动多样性**(MemDiv 内存层级扰动);**任意程序即测试**(生产负载/库测试化);指令定位与检测并发(非事后推断)
  - 处理技术类型:检出即隔离(QPool 机制);为在线在产测试铺路(开销可调);纠正错误硬件定位以指导缓解方向
- 业界观点摘录:学术论文(预印本),按模板略。(注:引言给出业界共识数字"约千分之一的硅器件"受 SDC 影响 [14,15,29,53,82,83];§1 批评公开测试套件与厂商专有版本结构性不同→已发表 fleet 结论难以复现/验证/改进;Table 10 将 Google cpu-check、Intel OpenDCDiag、Google SiliFuzz、Harpocrates、ITHICA 并列对照——OpenDCDiag 即本仓库上游;§9 引 PinDrop 哲学"用多样复杂测试做连续测试"与本文目标一致;致谢中披露 Gemini/Claude 用于文稿小改)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | fleet/池 | 数百万服务器;QPool >3000(≥10 微架构、2 厂商);DPool 20(14 台可报告) | §5.2 |
  | 检出总数 | 100 台(89 CC-ITHICA:75/14 QPool/DPool;+11 FB 唯一);vs Native +39%、vs SiliFuzz +69% | §7.1/§7.3, Fig.4 |
  | 双检 58 台指标 | EDR 1.78×、EF 7.51×、TTD 快 1.47× | Table 5 |
  | 详测时长 | >2000 h/DPool 服务器(超既往 1–2 个数量级);每测试 100 轮×1h | §5.2/§6.1 |
  | 业界共识患病率 | ~1 缺陷器件/1000 | §1 |
  | 不一致错误 | 24% 场合双错不同值 | Table 4 |
  | 使用强度反例 | 13/22(59%)检出测试非失败 opcode 最高频者 | Obs.16, Fig.7 |
  | 短测试复现 | 仅 D9 单指令可复现;D3 基本块可复现;其余皆不可 | Obs.18, Fig.8 |
  | 跨类型 | 44% 服务器多类型;92.9% 错误量来自 FP+向量;FP 频度超内存 6 个数量级 | Obs.20–22, Fig.9 |
  | Native 漏检 | 31 台中 8 台仅库内 ITHICA 检出(Zlib 6/OpenSSL 1/两者 1) | Obs.12 |
  | 开销 | Arith 2.18×→1.52×(块 1→8);Mem 1.17×;MemDiv 53.67×;Br 1.17× | Table 6 |
  | DPool 服务器 | 14 台,机龄 10–81 月,6 种架构(u1–u6) | Table 2 |

- 方法论要点:两池策略(QPool 大规模真实产线管道 + DPool 受控专测)互补;同二进制内 ITHICA vs Native 检查对比(排除编译差异干扰);3 种 QPool 下线机制(客户数据损坏投诉/筛查测试/异常取证);逐 opcode 的 PC/BB 敏感度与输入广度指标体系(Table 3);RTL 教学注入(CVA6+Verilator)建立直觉;覆盖缺口诚实声明(原子/易失内存操作、线程不安全代码内存指令、Libc 内核路径)。
- 横向对比注记:Stanford(Mitra/Trippel)×Google(Banerjee/Liu/Fuller)联合——**与本集 09(10x test escapes)同一团队**(Mitra/Banerjee/Liu/Hochschild/Ranganathan),引用其"产线负载可检出相当比例缺陷"作为动机 [53]。对三大 fleet 研究构成正面对话(Table 9 三方对照为综述核心素材):**挑战 01(Alibaba [82,83])的 instruction usage stress 定位法**(Obs.16 直接否定)、**挑战 05(SEVI [51])的一致错误假设与向量单元归因**(Obs.18/24)、与 02(PinDrop [11])的连续测试哲学结盟;SiliFuzz(25)被实测为检出显著更少(42 vs 71)且"短测试低效"论获佐证。引用网络密集覆盖本集:01[82,83]、02[11]、03[14]、04[7]、05[51]、07[16]、08[64]、09[53]、15[12]、18[8]、25[23]、26[27 对应其 35]、28[46]、29[21]、49[65]。EDDI/CFCSS/SWIFT/QED 软错误与后硅验证谱系是方法来源。Google/Meta/OCP 三方 gift + NSF 资助——与 04 同款的"学界-业界多边联合"样本。
- 身份核实:标题"缺陷诱发 SDC 的线程内指令检查方法"与内容(LLVM 指令级检查+两池实测+10 条 Findings)完全相符;arXiv:2605.15638v1 [cs.AR] 2026-05-15 时间戳印于 p.1 左缘,Stanford/Google 双挂作者与致谢资助链齐全,名实相符。**体裁与文本层说明**:(1) 未见会议页眉/DOI/版权行,按 arXiv 预印本标注(与 03 同类处理),引用时注明未经同行评审;(2) Fig.3(DPool EDR 热图)数字在文本层严重乱序不可逐值复原,已按正文叙述引用关键数字;(3) Table 2 架构标签 u1–u6 与 D1–D14 对应可读,机龄行完整;(4) Fig.7 部分纵轴标签缺失,不影响结论 59% 的正文数字;(5) 仓库承诺发表后公开(脚注 2)。

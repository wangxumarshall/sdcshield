# [14] Applying Architectural Vulnerability Analysis to Hard Faults in the Microprocessor(SIGMETRICS/Performance'06,2006-06-26–30,Saint Malo;2 页短文,印刷页 375–376)

- PDF: `Applying Architectural Vulnerability Analysis to Hard Faults in the Microprocessor.pdf`;页数 3(实读 p.1–2 全部,末页空白;"Copyright is held by the author/owner(s). SIGMETRICS/Performance'06 … ACM 1-59593-320-4/06/0006"——workshop/短文体裁,无 DOI 印于文本层)
- 作者(全列):Fred A. Bower、Derek Hower、Mahmut Yilmaz、Daniel J. Sorin、Sule Ozev(共 5 人)
- 机构(学术/企业分列):
  - 学术:Duke University 电气与计算机工程系 ×4(Hower、Yilmaz、Sorin、Ozev;Sorin 组——可靠性研究重镇)
  - 企业:**IBM ×1**(Bower,Research Triangle Park NC,3039 Cornwallis Rd.;bowerf@us.ibm.com)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):IBM ×1(p.1)
  - 二级(致谢章节资助/数据/设备):致谢(§5)——资助:NSF CCF-0444516/CCR-0309164、**NASA NNG04GQ06G**、**Intel Corporation**、Warren Faculty Scholarship;另感谢 Alvin Lebeck 评论
  - 三级(首页脚注资助声明):无(资助并入致谢)
- 核心结论(各带节号/表号):
  1. **H-AVF 新指标(§2)**:Hard-Fault Architectural Vulnerability Factor——结构中一故障(遍历全部故障点×故障模型)导致某条指令提交错误架构状态的概率;按 workload 平均以正确计入掩蔽效应;支持多故障模型(最常见单比特 stuck-at-0/1)
  2. **动机:MTTF/FIT 与 AVF 的双重空白(§1)**:MTTF/FIT 不考虑组件输入(掩蔽依赖输入)与利用率;AVF [2](即本集 [11])**只适用于存储结构的软错误,不适用于硬故障与组合逻辑**——H-AVF 填此空白
  3. **晶体管归一化(§2)**:故障密度对给定工艺视为常数,原始 H-AVF 跨实现比较有误导性——除以晶体管数得 H-AVF/transistor 才可比较功能等同、实现迥异的设计;可与面积/功耗复合成成本效益指标
  4. **三结构案例研究(§3/§3.1)**:寄存器堆(126×64 bit)+L1 数据 cache(16KB=256×64B,按 Pentium 4 建模)+64 位整数加法器——两存储结构用 ECC、加法器用 TMR 的加固前后 H-AVF 对比
  5. **结果(Table 1,SPEC CPU2000 平均)**:寄存器堆 base 0.08388→ECC 0.00871(归一化 0.00958)≈**8.4× 抗硬故障**;L1 D-cache 0.00486→0.00076(归一化 0.00084)≈**5.8×**;加法器 0.1488→TMR 0.0161(归一化 0.0533)≈**2.8×**;数值量级小因结构远大于所存值+基准 cache miss 率低
  6. **面积归一化因子(§3.2)**:寄存器堆 ~1.15、L1 cache ~1.10、TMR 加法器 ~3.31——TMR 以 3.31× 面积换 2.8× 容错,性价比显著低于 ECC 存储保护
  7. **方法(§3.2)**:64 位全输入空间枚举不可行→以 SPEC CPU2000 为输入加权;RF/cache 用 SimpleScalar sim-cache 前 1000 万指令;加法器用 SimPoint 采样 1 亿指令(modified sim-mase)
  8. **用途(§4)**:先评估某子结构是否值得加固,再单独或复合其他设计指标比较竞争性容错设计——给微架构师的通用工具
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**硬故障(永久性单比特 stuck-at-0/1,制造缺陷类)**——**本集 R2 谱系首篇非辐射根因**;2006 年设计期视角的制造缺陷脆弱性评估
  - 故障模式类型:存储结构(RF/L1D)与**组合逻辑(加法器)**的永久性故障→提交错误架构状态;掩蔽效应依赖输入与利用率
  - 检测技术类型:设计时脆弱性评估——H-AVF 指标(软错误 AVF 的硬故障泛化;多故障模型框架)
  - 处理技术类型:ECC(存储结构)与 TMR(组合逻辑)的**成本效益定量**——H-AVF/晶体管 × 面积/功耗复合指标指导加固投放
- 业界观点摘录(技术短文,业界立场观点按模板略;业界联系证据:IBM 署名×1+Intel 资助(致谢级)+NASA 资助)
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | H-AVF 寄存器堆 | base 0.08388 / ECC 0.00871 / 归一化 0.00958(表)——prose 称 ~8.4×(见身份核实) | Table 1, §3.2 |
  | H-AVF L1 D-cache | 0.00486 / 0.00076 / 归一化 0.00084 → **5.8×** | Table 1, §3.2 |
  | H-AVF 64 位加法器 | 0.1488 / TMR 0.0161 / 归一化 0.0533 → **2.8×** | Table 1, §3.2 |
  | 面积归一化因子 | RF ~1.15 / cache ~1.10 / TMR 加法器 ~3.31 | §3.2 |
  | 结构规模 | RF 126×64bit;L1D 16KB=256×64B(P4 建模);64bit 整数加法器 | §3.1 |
  | 工作负载 | SPEC CPU2000;RF/cache 前 1000 万指令(sim-cache);加法器 SimPoint 1 亿(modified sim-mase) | §3.2 |

- 方法论要点:H-AVF 公式(对指令平均的 Σ故障点Σ故障模型 insts_error/insts,文本层公式乱码已重构);不可行的全输入枚举→基准加权的工程妥协;面积归一化的公平比较原则(故障密度/工艺常数假设);ECC 与 TMR 的统一定量框架;诚实局限:结构远大于所存值+低 miss 率导致数值量级小、未报逐基准方差(全基准平均)。
- 横向对比注记:**AVF 谱系第五篇——首次把 AVF 概念从辐射软错误扩展到硬故障与组合逻辑**:"AVF does not apply to hard faults or to combinational logic"的空白声明直接前驱了 [15](DelayAVF 延迟故障)与 [18](从门到 SDC);**2006 年的"硬故障 AVF"是 2003 辐射范式(10/11/12)与 2021+ fleet 制造缺陷范式(01/02/09)之间的概念桥梁**——设计期视角的制造缺陷(stuck-at 最简模型)脆弱性评估;[2] 引用即本集 [11](Mukherjee MICRO-36),概念血缘明确;Duke Sorin 组×IBM 署名(Bower)+Intel/NASA 资助——设计期可靠性研究的业界多源支撑样本;TMR 加法器 3.31× 面积换 2.8× 容错的低性价比定量,与 [52](MicroBlaze-V TMR 辐射实测)的 TMR 主题遥相呼应;stuck-at 故障模型与 [09](Google)所引 McCluskey 00"stuck-at 度量已严重不准"形成设计期假设 vs 制造测试现实的对话——设计期 stuck-at 评估的局限在 fleet 时代被重新审视。
- 身份核实:标题"把架构脆弱性分析应用于微处理器硬故障"与内容(H-AVF 指标+三结构 ECC/TMR 案例定量)完全相符;SIGMETRICS/Performance'06 Saint Malo 2006-06-26–30、ACM 1-59593-320-4/06/0006、印刷页 375–376 齐全,名实相符。**文本层已知损失与内部不一致**:(1) **§2 H-AVF 公式文本层乱码**("=1 1 / insts H-AVF faultsites fault insts mod elsinsts")——重构为 H-AVF=(1/insts)·Σ_faultsites Σ_faultmodels (insts_error/insts),与散文描述("计算给定指令输入下会提交错误架构状态的指令绝对数,除以故障点数,对指令平均")自洽,引用时注明重构;(2) **Table 1 寄存器堆行的内部不一致**:表给归一化 0.00958(=0.00871×1.10,却对应 cache 的面积因子而非 RF 的 ~1.15),而 prose 称"~8.4×"(0.08388/0.01002=8.37,即 0.00871×1.15=0.01002 才与 8.4× 自洽)——L1D 行(0.00076×1.10=0.00084,5.79≈5.8×)与加法器行(0.0161×3.31=0.0533,2.79≈2.8×)交叉验证完全吻合,证明行-组映射无误,RF 行归一化格疑为表格笔误(误用 1.10);引用时按 prose "~8.4×" 并注明表值差异;(3) 短文无逐基准数据(全基准平均);(4) 无资助脚注(资助在致谢 §5,如实标注)。

# [04] Veritas — Demystifying Silent Data Corruptions: μArch-Level Modeling and Fleet Data of Modern x86 CPUs(HPCA 2025;会议论文)

- PDF: `Veritas__Demystifying_Silent_Data_Corruptions_Arch-Level_Modeling_and_Fleet_Data_of_Modern_x86_CPUs.pdf`;页数 15(实读 p.1–14,末页空白)
- 作者(全列):Odysseas Chatzopoulos、Nikos Karystinos、George Papadimitriou、Dimitris Gizopoulos、Harish D. Dixit、Sriram Sankar(共 6 人)
- 机构(学术/企业分列):
  - 学术:雅典大学(University of Athens, Greece)×4
  - 企业:Meta Platforms Inc ×2(Dixit、Sankar)
- 企业合作证据:
  - 一级(作者 affiliation 挂企业):Meta ×2(p.1)
  - 二级(致谢章节资助/数据/设备):**研究资助来自 OCP(Open Compute Project)、Meta 与 AMD 的 research gifts,以及欧盟 Horizon Europe(101093062 Vitamin-V / 101070238 NEUROPULS / 101097224 REBECCA)与希腊 HFRI(16973 REDESIGN)**(p.12);**fleet 数据(6 年、数十万 CPU、数十亿 CPU 小时、相对 DPPM)由 Meta 提供**(§III-G)
  - 三级(首页脚注资助声明):原文无首页脚注资助声明(资助全部在 p.12 致谢)
- 核心结论(各带节号/图表号):
  1. **填补空白:首次对 CPU 算术单元(整型/浮点、标量/向量)做门级故障注入的 SDC 分析**——既往研究几乎全部聚焦阵列结构(cache 等,已有 ECC 保护),而 SDC 真正源头是无保护的算术硬件(§I/§II-C,Table I 列 4 个开放研究问题,Table II 给出单元关键性分级:4 类算术单元 SDC 可能性 Large/保护 None-to-Weak)。
  2. **孪生实验设计:gem5 门级注入(67,500,000 次注入=模拟 22.5 万颗缺陷 CPU)+ Meta fleet 实测(6 年)互相独立、互相校准**(§III):仿真给出细粒度 PSDC(单元级 SDC 概率),fleet 给出真实 DPPM;式(1)–(4) 从 PSDC 推导到 fleet 级 SDC 率;永久 stuck-at 故障作为边际缺陷/小延迟缺陷的**悲观上界**(§III-F)。
  3. **Observation #1:标量整数加法器最不可能致 SDC**(结果多影响控制流→崩溃;错误率 1/31K–1/1M,§IV-F);整数乘法器因门数 30× 于加法器+结果多进数据流,SDC 率高 2 个数量级(§IV-A)。
  4. **Observation #2/#4:向量单元(整型与浮点)致 SDC 高出数个数量级;FP 重载(向量化线性代数/数值算法)工作负载 SDC 率最高,且 fleet 数据确认同样模式**(§IV-A/§IV-C,Fig.6/8)——fleet 中向量 FP 指令锤击测试的相对 SDC 率最高(VMUL/VFMUL 类达数千级 vs 标量 ADD 的个位数,Fig.8)。
  5. **Observation #3:乘法器门级实现(Array/Wallace/Dadda)对 SDC 率影响极小**(§IV-B,Fig.7)——电路级设计选择不改变可靠性量级,也佐证"对五种 x86 用同一门级模型"的方法论有效性。
  6. **Observation #5:微架构选择显著影响 SDC 率**——统一 DPPM 下,单元数量最多(脆弱向量/FP 单元)的 CPU B 总 SDC 率最高(Fig.9:A 1.09×,B 1.51×,C 1.47×,D 1.40×,E 基准);**Observation #6:乘入 fleet 实测相对 DPPM(A=0.85,B=0.78,C=0.67,E=1,D 无足够数据)后排名洗牌:B 仍最高(1.19×),A/E 反转(A 0.93× 低于基准 E),C 0.98×**(§IV-E,Fig.10)——内在(设计)与外在(制造质量)因素须分开评估。
  7. **指令错误率画像**(§IV-F,Fig.11):标量 FP 加法器故障时 x87 指令错误率 16%–77%;双 FADD 单元的 CPU B 错误率约减半(77%/2=38.5% 验证);向量指令错误率低于标量(一次 stuck-at 通常只错 4 lane 之一)但 SDC 贡献不变(软件掩蔽极低,"almost every computation matters");Meta fleet 中崩溃比 SDC 多 2–3×(§III-E)。
  8. 方法论效率:门级模型内嵌 gem5(微码 op 级接口)仅损失 0–4% 吞吐 vs 外部门级仿真器 220%(§III-D,Fig.4);fleet 测试参数对照(Table IV:Fleetscanner 分钟级/60 天周期/93% 检出;Ripple 毫秒级/7 天周期/77% 检出)。
- 分类学标注(按论文实际内容归类):
  - 根因机理类型:**算术单元永久硅缺陷(stuck-at 为上界建模的边际/延迟缺陷族)**;缺陷概率 ∝ 单元面积(§III-F 假设 2);制造质量(DPPM)作为外在因子独立于设计
  - 故障模式类型:**单元级 PSDC 概率谱**(向量 FP > 向量整型 ≈ 标量 FP >> 标量整型加法);SDC/崩溃/掩蔽三分结果;指令级错误率谱(x87 标量 FP 16–77% vs 标量整型 ~10⁻⁶–10⁻⁵)
  - 检测技术类型:**早期阶段仿真预测**(gem5 门级 SFI,设计期即可用);**fleet 定向锤击测试**(对目标单元随机输入轰炸);**6 年遥测 DPPM 数据**(§III-G)
  - 处理技术类型:**面向设计期的方法论输出**——脆弱单元数最少的微架构选择、软硬件保护特征部署的优先级依据(§IV-D 末/§VI);Meta 侧检出即退役(一缺陷一芯片假设,Table IV #3)
- 业界观点摘录:学术论文,按模板略。(注:§II-B 引述 Meta 用 Fleetscanner/Ripple 六年生产实践检出数百台 SDC 设备、横跨世代的系统性问题;§I 强调"field data arrives late and brings high-level coarse-grained information…game changer 是 field 数据与早期预测的结合")
- 关键数字(表):

  | 指标 | 数值 | 出处 |
  |---|---|---|
  | 仿真规模 | 6750 万注入 = 22.5 万颗模拟缺陷 CPU;每单元每架构 3000 次(1500 s-a-0 + 1500 s-a-1),99% 置信/误差 <2% | §IV |
  | 微架构 | 5 种 x86 OoO(2014–2020),12–17 个 FU/核 | Table III |
  | 工作负载 | 30 个分 5 类(检索排序/压缩哈希/图像处理/FP 重载/整型重载) | §IV |
  | fleet 规模 | 数十万 CPU、数十亿 CPU 小时、6 年遥测 | §III-G |
  | 总 SDC 率(同 DPPM) | Fig.9:A 1.09×,B 1.51×,C 1.47×,D 1.40×,E 基准(正文散文数字 B 1.59×/A 1.03×/C 1.30×/D 1.43× 与图表链不一致,以图表为准,见身份核实) | Fig.9, §IV-D |
  | 乘入 DPPM 后 | B 1.19× > E 基准 > C 0.98× > A 0.93× | Fig.10,§IV-E |
  | 乘法器/加法器 SDC 比 | ~30×(门数比) | §IV-A |
  | x87 指令错误率 | 16%–77%(标量 FP 加法器故障) | §IV-F |
  | 崩溃:SDC | ≥2–3:1(Meta fleet) | §III-E |
  | gem5 集成开销 | 0–4% 吞吐损失 | §III-D, Fig.4 |

- 方法论要点:ArithsGen 自动生成算术电路 C++ 门级模型→Python 模块插桩注入 wrapper→微码 op 级嵌入 gem5 IEW 流水→并行注入战役管理器(§III-A 五步);金标运行收集单元活跃度避免对不活跃单元注入(全部计为掩蔽);仅公布相对 SDC 率(绝对 DPPM 保密,§III-F 假设 3);fleet 侧取每 CPU-故障组合最新快照+历史数据保证代表性(§III-G)。
- 横向对比注记:雅典大学 Gizopoulos 组与 Meta(Dixit/Sankar)的长期合作系列之一——同组关联本集内 08(TC23 微架构视角 [12])、18(gates-to-sdc,DATE25 [36])、22(gem5-marvel [42])、23(cross-isa ITC23 [44])、26/27(Harpocrates)、29(ETS24 [47])、49(stealthy-saboteurs [31]);Meta 侧连接 02/03。与 01(Alibaba)互补:01 是 fleet 实测端(粗粒度),04 补上"早期预测"端并把 01 的"算术单元是嫌疑"细化到 17 类单元粒度([3],[4] 即 01 及其 TACO 扩展版)。Table II 的单元关键性分级表是综述"根因机理分类"维度的直接素材。AMD/OCP/Meta 三方 research gift 是"学界-业界联合"模式的典型样本。
- 身份核实:标题"去神秘化 SDC:x86 CPU 的架构级建模与 fleet 数据"与内容(孪生实验+算术单元门级注入)完全相符;HPCA 2025 页眉、DOI 10.1109/HPCA61900.2025.00012、Xplore 授权水印齐全(p.1),名实相符。**文本层已知损失与原文内部矛盾**:Table V 与 Fig.9/10 的数值在双栏-图表交错下乱序,已用算术交叉验证复原(Fig.9 值 × Table V DPPM = Fig.10 值:A 1.09×0.85≈0.93、B 1.51×0.78≈1.18、C 1.47×0.67≈0.98,精确吻合,且与 §IV-E"C 成为第二低"叙述一致)——故采信 Fig.9 数值;§IV-D 正文散文数字(B 1.59×、A 1.03×、C 1.30×、D 1.43×)与该图表链及 Fig.10 均不符,疑为旧版图表残留,引用时以图表链为准;Table IV 部分属性行(#4/#5)乱序不可完全复原,已按可确认字段引用。

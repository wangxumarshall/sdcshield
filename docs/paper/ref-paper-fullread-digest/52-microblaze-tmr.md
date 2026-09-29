# [52] AMD MicroBlaze-V TMR 架构可靠性评估:故障注入与质子辐照(Microprocessors and Microsystems 2026,期刊论文)

## 标题与出处

- 标题:Reliability assessment of AMD MicroBlaze-V TMR architecture using fault injection and proton irradiation
- 期刊:Microprocessors and Microsystems(Elsevier)123 (2026) 105301
- DOI:10.1016/j.micpro.2026.105301;开放获取(CC BY 4.0)
- 时间线:收稿 2026-01-09,修回 2026-05-15,录用 2026-06-10,在线 2026-06-12
- 类型:期刊论文·实验型(抗辐射可靠性评测:故障注入 + 15/230 MeV 质子辐照)

## PDF 与实读范围

PDF 全文 10 页(ref/ 下),经 ocr32 提取为 52-microblaze-tmr.txt(66,544 B,545 行),分两段读完:p.1–6 前半(1–400 行)+ p.6 后半–p.10(400–545 行)。含摘要、正文 §1–§5、Table 1–5、Fig.1–4、Funding、CRediT、利益冲突声明、致谢、参考文献 [1]–[20]、三位作者传记。无跳读;Table 3/4 双栏并排存在文本层错位(见身份核实)。

## 作者全列(p.1)

1. Jorge Cano-Páez(通讯作者,jocanop@ing.uc3m.es;第一作者,博士生)
2. Luis Entrena
3. Almudena Lindoso(Senior Member, IEEE)

## 机构分列(p.1)

- Universidad Carlos III de Madrid(马德里卡洛斯三世大学,UC3M)电子技术系,西班牙莱加内斯——三位作者全部。

作者传记补充(p.8–9):Cano-Páez 2023 年马德里康普顿斯大学本科、2024 年 UC3M 硕士、在读博士;Entrena 1995 年马德里理工博士,1990–1993 年 AT&T Microelectronics 贝尔实验室(美国)、1993–1996 年 TGI(西班牙)项目负责人,现任 UC3M 教授(系主任经历,200+ 论文 2 专利);Lindoso 2003 年起 UC3M 副教授(80+ 论文)。

## 企业合作证据(三级)

- **一级(作者隶属)**:无。三位作者均 UC3M,纯学术团队。
- **二级(资助/致谢)**:零企业资助。Funding(p.8):马德里自治区 PIPF-2024/TEC-34925;西班牙 MCIN/AEI 项目 PID2022-138696OB-C21(ERDF "A way of making Europe");欧盟 Horizon 2020 **RADNEXT**(Grant 101008126,辐射设施网络,提供辐照实验条件)。致谢(p.8):西班牙塞维利亚 CNA(Centro Nacional de Aceleradores)与瑞士 PSI(Paul Scherrer Institute)提供束流时间——均为公共研究设施。利益冲突声明:无。
- **三级(首页脚注/版权页)**:无。
- **间接层(被评测对象)**:**AMD/Xilinx 全栈**——研究对象为 AMD MicroBlaze-V(RISC-V 软核,UG1629)、AMD TMR Manager IP(PG268)、AMD SEM 软错误缓解 IP(PG036,用作故障注入接口)、Vitis IDE、Artix-7 35T FPGA(Digilent CMOD-A7 板)。AMD 未出资、未署名,本文属"厂商商业 IP 的独立第三方学术评测"形态;参考文献 [3][10][17][18][19] 直接引用 AMD 官方文档。另 Entrena 的 AT&T 贝尔实验室经历(1990–93)系履历沿革,非本文合作。

## 核心结论

1. **总体**:AMD TMR MicroBlaze-V 对配置存储器故障的错误检出率约 99.84%;约 1% 的失败无预先警告(全部为 DBE,与三模冗余的存储器相关);无预警失败的截面比总截面低约 47×(230 MeV)与 65×(15 MeV)(摘要;§5。注:摘要/结论与正文的能量-倍数对应关系互换,见身份核实)。
2. **架构选择**:TMR Manager IP 提供三种架构(容错子系统+本地三模存储器 / 容错子系统+ECC 存储器 / Fail-Safe 子系统),本文选最鲁棒的 Fail-Safe;以程序轨迹(program trace)比较器 + SECDED ECC 构成"容错(Fault Tolerant)→ 锁步(Lockstep)→ 致命(Fatal)"三态机,lockstep 态即"预警降级"窗口(§3、§3.1、Fig.1)。
3. **故障注入 campaign**:对 essential bits(设计实际用到的配置位,N=1,423,619)做统计注入(式(1),Leveugle DATE'09;误差 0.8%、置信 99.9%);实注 40,175 次 → 5,463 个错误(13.6%)、2,744 个 run;失败构成 Fatal 71.06% / DBE 27.73% / SDC 0.29%(仅 8 例);99.09% 的失败先有 lockstep 预警;无预警失败 25 例全为 DBE、0 例 SDC(§4.3.1,Table 2)。
4. **15 MeV 质子辐照(CNA)**:213 事件,**零 SDC**——暴露时间内 100% 检出;截面:总 1.72×10⁻⁹ cm²、失败 8.97×10⁻¹⁰、无预警 2.66×10⁻¹¹(比总截面低 64.5×);SPENVIS 外推 11 年/1000 km 轨道任务注量 1.44×10¹¹ p/cm² ≈ 实验注量 2.72×10¹¹;MTTF(95% CI):有预警失败 31–41 天,无预警 2–85 年(§4.3.2,Table 3)。
5. **230 MeV 质子辐照(PSI)**:222 事件,**SDC 2 例(0.9%)**——证明故障注入对低概率事件也有代表性;截面:总 1.13×10⁻⁹ cm²、失败 5.92×10⁻¹⁰、无预警 2.39×10⁻¹¹(低 47×);MTTF:有预警 3–9 年,11 年任务期内无预警失败≈0;若加配置存储器清洗器则任务期应无失败(§4.3.3,Table 4)。
6. **FI↔辐照相关性(方法论核心贡献)**:三组 campaign 的失败分布均呈对数正态(MLE 拟合;直方图 Fig.2,PDF/CDF Fig.3),故障注入可建模质子辐照效应——束流时间难求且昂贵,FI 可作辐照前的低成本替代与外推基础(§4.4)。
7. **寿命定量**:平均 6.57 次注入→lockstep 预警;预警后 8.21→Fatal、7.78→DBE、5.63→SDC;失败平均需 11.87 次注入——**系统寿命 ≈ 2× 首次预警时间**,给出恢复操作的时序窗口;无预警失败平均需约 13 次注入(均值 13.24/中位 12)(§4.4,Table 5,Fig.4)。
8. **清洗频率量化优化**:有分布知识后,10× 经验规则的失败概率仅 0.03%;若应用可接受 1% 失败概率,清洗频率可降一档(对应 2.481 注入率),**功耗省 50%**——量化可靠性需求对功耗预算的兑换(§4.4)。
9. **与 NOEL-V 对比**:SDC 率 2×10⁻⁴,比 NOEL-V 分布式 TMR(UltraScale+,矩阵乘基准)低约一个数量级;功能错误率 4.9×10⁻² 高约一个数量级(§4.3.1 引 [16])。
10. **动机谱系**:New Space 时代推动 COTS + 开放 ISA(RISC-V)替代专用抗辐照器件;SRAM FPGA 配置存储器对辐射诱发翻转敏感是根因;汽车自动驾驶同列驱动场景(§1)。
11. **方法论警示**:全程**不开清洗**(scrubbing)——带清洗会高估架构自身纠错能力;且清洗不充分:软核配置受损后必须与另两核重新同步,配置恢复≠系统恢复(§4.3)。

## 分类学标注

- **领域**:空间/抗辐射 SRAM FPGA 软核处理器(RISC-V);New Space + 汽车自动驾驶延伸。
- **根因家族**:**辐射诱发软错误(SEU/配置存储器位翻转)**——与 R1/R2 批的制造缺陷/老化谱系互补,是语料中"辐射→SDC"路径量化最完整的论文(另见 [24])。
- **故障模式**:配置存储器 essential bits 累积翻转、BRAM 双位错(DBE)、多故障乘性效应、失败时间呈对数正态分布。
- **检测技术**:块级 TMR 多数表决 + 程序轨迹比较器(表决保护)+ SECDED ECC + 金签名比对(CoreMark 签名 vs 预计算 golden;比较器自身也三模表决)。
- **处理技术**:三模冗余(TMR)、ECC、lockstep 降级运行 + 恢复窗口(寿命 2× 定量)、配置存储器清洗(频率按失败概率分布量化优化)。
- **量化方法**:统计故障注入(式(1))、截面 σ + 95% CI、SPENVIS 轨道注量外推、MTTF、MLE 对数正态拟合。
- **企业关系**:AMD 间接层(被评测对象),零资助——独立第三方评测形态。

## 业界观点摘录

- "The New Space Era has fundamentally reshaped the development of high-reliability space systems, promoting a shift from traditionally costly and specialised components towards more accessible, flexible, and scalable technological solutions."(§1,新空间时代叙事)
- "The space industry is currently focused on adopting RISC-V because of the benefits it provides in a sector characterized by low volume production."(§1,RISC-V 产业采用逻辑)
- "COTS components offer a compelling alternative to custom-built hardware…enable a configurable design suitable for demanding applications while reducing Intellectual Property (IP) licensing costs."(§1,开放 ISA 的成本动机)
- "Xilinx-AMD offers an all-in-one environment for fast development of RISC-V architectures using MicroBlaze-V IP."(§3,厂商工具链自述)
- "These errors [SDCs] pose a great threat to the system, because the system generates incorrect data while no problem is detected."(§4.3.1——与数据中心谱系 [47]/[49] 的 SDC 定义跨域同构)
- "…fault injection campaigns are more feasible than irradiation experiments due to difficulties in finding available beamtime and the cost of such experiments."(§4.4,束流资源稀缺)
- "…setting a high scrubbing frequency increases the power consumption without providing a real value when dealing failures."(§4.4,对经验规则的批评)

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 99.84% | TMR MicroBlaze-V 错误检出率(摘要口径,正文无显式推导) | 摘要 |
| ~1% | 无预先警告的失败占比(全为 DBE) | 摘要/§4.3.1 |
| 40,175 / 26,239 | 实际注入次数 / 统计所需注入数(式(1)) | §4.3.1 |
| 1,423,619 | 设计 essential bits 总数 N | §4.3.1 |
| 5,463(13.6%)/ 2,744 | 注入产生错误数 / run 数 | §4.3.1 |
| 71.06% / 27.73% / 0.29% | FI 失败构成 Fatal / DBE / SDC(8 例) | Table 2 |
| 4.5× / 4.3× | TMR 架构相对单核的 LUT / 寄存器开销 | §4.1,Table 1 |
| 1.72×10⁻⁹ / 8.97×10⁻¹⁰ / 2.66×10⁻¹¹ cm² | 15 MeV:总截面 / 失败截面 / 无预警截面(64.5×) | §4.3.2 |
| 1.13×10⁻⁹ / 5.92×10⁻¹⁰ / 2.39×10⁻¹¹ cm² | 230 MeV:总 / 失败 / 无预警截面(47×) | §4.3.3 |
| 2.72×10¹¹ / 4.28×10¹¹ p/cm² | 15 MeV(CNA)/ 230 MeV(PSI)实验注量 | §4.3.2/§4.3.3 |
| 1.44×10¹¹ / 3.77×10¹⁰ p/cm² | 11 年 1000 km 轨道任务注量(SPENVIS,>15 MeV / >230 MeV) | §4.3.2/§4.3.3 |
| 31–41 天 / 2–85 年 | 15 MeV MTTF:有预警 / 无预警 | §4.3.2 |
| 3–9 年 / ≈0 | 230 MeV MTTF:有预警 / 无预警(11 年任务期) | §4.3.3 |
| 6.57 / 8.21 / 7.78 / 5.63 | 平均注入数→lockstep / Fatal / DBE / SDC(预警后) | Table 5 |
| 11.87 / 13.24 | 平均注入数→失败 / 无预警失败(中位 12) | §4.4,Table 5 |
| 0.03% → 50% | 10× 清洗规则的失败概率;接受 1% 概率可省的功耗 | §4.4 |
| 2×10⁻⁴ / 4.9×10⁻² | SDC 率 / 功能错误率(对比 NOEL-V 各差一个数量级) | §4.3.1 |
| 0 SDC / 2 SDC(0.9%) | 15 MeV 零 SDC / 230 MeV SDC 数 | §4.3.2/§4.3.3 |
| 0.15% | lockstep 错误中 TMR Manager 自身故障占比 | §4.3.1 |

## 方法论要点

- **统计故障注入**:不逐位注入全部 essential bits,而按式(1)(Leveugle DATE'09)以误差/置信/错误率参数采样,把不可行的全量注入压缩到 2.6 万次量级——对 SDCShield 类全遍历思路的统计学替代。
- **essential bits 定向注入**:只注入设计实际占用的配置位,提高错误命中密度;故障注入管理器自身做成 TMR 且设为部分可重构 Pblock,把自己排除出注入域。
- **"预警-失败"两级截面**:把截面拆成"总失败截面"与"无预警失败截面",后者才对应 SDC 式静默风险——度量维度从"是否失败"细化为"失败前是否有信号"。
- **分布驱动的运维参数**:用 MLE 拟合的对数正态分布把经验规则(10× 清洗)替换为按失败概率容忍度选频率——可靠性需求→功耗预算的显式兑换公式。
- **不开清洗的评测立场**:累积故障条件下评估架构自身能力,避免清洗器代偿掩盖架构缺陷;并指出配置恢复≠系统恢复(需重同步)。
- **金签名兜底**:SDC 只能靠 CoreMark 签名与预计算 golden 比对发现,且比较器输出经表决以保证检测路径本身不静默——检测机制的自防护(self-checking)设计。

## 横向对比注记

1. **与 [24] arm-soft-error**:同为质子束流实验,但 [24] 测商用 Arm 核(裸机 23.7%/Linux 59.3% SDC 率),本文测 SRAM FPGA 软核 TMR **系统级**;辐射根因同族、观测层级不同。
2. **与 [23] cross-isa**:[23] 跨 ISA/微架构估错误率,本文单架构深挖并做 FI-辐照校准——广度与深度的互补。
3. **与 [49] Table I**:本文实证了"仿真/注入 vs 束流"两层方法学**相关性**(对数正态分布跨三 campaign 一致),为 [49] 的六方法分层提供落地证据;[49] 的 FIT=AVF×rawFIT×bits 桥梁在本文对应截面×注量×轨道环境外推。
4. **与 [47]/[49] SDC 定义**:"系统继续运行但产生错误数据"在数据中心与空间辐射两域完全同构——SDC 概念跨根因家族(制造缺陷 vs 辐射软错误)的普适性。
5. **金签名方法论同构**:CoreMark 签名 vs 预计算 golden,与 SDCShield 的全量字节比对、[25] SiLiffuzz、[26]/[27] Harpocrates 的金值思想同源——"以已知对测未知"是 SDC 检测的公约数。
6. **与 [40]/[41] futures/Parallaft**:lockstep 概念跨层复用(硬件 TMR 锁步 vs 软件任务复制/运行时锁步);"预警后还有 2× 时间窗"与 [40] 复制调度的时序余量呼应。
7. **与 [28] aging-asplos24**:同为"预警先于失败"的主动哲学(老化先兆 vs lockstep 预警),本文把预警价值量化为 2× 寿命窗口。
8. **与 [50] AMD 线两面**:[50] 是 AMD 工程师自述数据中心 CPU 小延迟故障,本文是学术界独立评测 AMD FPGA 空间产品——同一厂商两条产品线、署名合作 vs 被评测两种形态,供"企业关系形态学"章节对读。
9. **与 [46] fs-metadata**:清洗(scrubbing)跨层同名——FPGA 配置清洗 vs 文件系统 scrubber;按失败概率分布定周期 vs 按静默风险定策略,思想同构。
10. **R8 批"独立评测 AMD"双例**:本文(FPGA 空间线,零企业资助)与 [51](GPU 线,零企业资助、EU+HFRI)构成同一形态双证据;对照 [50](三厂商署名)与 [49](Meta 署名+资助),企业卷入谱系从署名、资助到纯被评测完整展开。

## 身份核实

- **47×/65× 能量归属矛盾(论文内部不一致)**:摘要与 §5 均写"47(15 MeV)、65(230 MeV)",但 §4.3.2/§4.3.3 正文算术为 1.72×10⁻⁹/2.66×10⁻¹¹=**64.7(15 MeV)**、1.13×10⁻⁹/2.39×10⁻¹¹=**47.3(230 MeV)**——摘要/结论疑将两个倍数互换,以正文算术为准。
- §4.3.3 叙述句"2.39×10⁻¹⁰ cm²"与 Table 4 及 47× 算术(须为 2.39×10⁻¹¹)矛盾,疑正文指数排版/文本层错位;采 10⁻¹¹。
- 99.84% 检出率仅见于摘要,正文未见推导;(5463−8)/5463≈99.85% 最近似(仅 8 例 SDC 需金签名兜底的口径),标记为本文笔记的推断,非论文原文。
- Table 3/4 双栏并排致文本层截面列错位(如 SDC 0% 行挂着 8.89×10⁻¹⁰ 等值),一律以 §4.3.2/§4.3.3 叙述段为准。
- 作者名西班牙字符(Cano-Páez/Leganés/Entrena 等)文本层乱码,按 p.1 邮箱域(uc3m.es)与 p.8–9 作者传记还原;机构归属无歧义。
- Entrena 传记含 AT&T 贝尔实验室(1990–93)经历,系履历沿革,不计入本文企业合作证据。
- 期刊卷期 123 (2026) 105301、收录用日期链(2026-01-09→06-12)在 p.1/p.2 页眉交叉一致。

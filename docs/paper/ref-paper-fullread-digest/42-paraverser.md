# [42] ParaVerser:用异构并行性实现数据中心可负担的故障检测(DSN 2025,硬件微架构级)

## 标题与出处

- **标题**: ParaVerser: Harnessing Heterogeneous Parallelism for Affordable Fault Detection in Data Centers
- **出处**: **DSN 2025**(IEEE 可靠系统与网络国际会议),dsn25.pdf;**CC-BY 开放获取**(p.1 声明 "For the purpose of open access, the author has applied a Creative Commons Attribution (CC BY) license");16 页;源码双渠道公开(Zenodo 10.5281/zenodo.15080017 + github.com/CompArchCam/DSN25-AE)——**同行评议 + 开放工件**。
- **与 [41] 的引文链互证**: 本篇引文 [89] = Parallaft CGO 2025(即 digest [41]),且脚注 14 明言 "our own testing [89]"(Intel 14 代小核无独立电压域的实测来自 Parallaft 工作)——同一团队跨栈层双线(软件 CGO'25 + 硬件 DSN'25)同年发表,**确证**。
- **类型**: 学术界论文(英国 4 校 + 西班牙 BSC)。

## PDF 与实读范围

- **文件**: `ref/ParaVerser Harnessing Heterogeneous Parallelism for Affordable Fault Detection in Data Centers, dsn25.pdf`
- **页数**: 16 页(pp.1–15 内容 + p.16 空),**全文实读,无截断**(txt 分两段:1–510 / 511–1011 行)。

## 作者全列(p.1)

Minli Julie Liao(剑桥)、Sam Ainsworth(爱丁堡)、Lev Mukhanov(伦敦玛丽女王大学)、Adrian Barredo(巴塞罗那超算中心 BSC)、Markos Kynigos(曼彻斯特大学)、Timothy M. Jones(剑桥)——6 人 5 机构。

## 机构分列(p.1)

- **剑桥大学计算机实验室**(Liao、Jones)×2;
- **爱丁堡大学**(Ainsworth);
- **伦敦玛丽女王大学**(Mukhanov);
- **巴塞罗那超算中心 BSC**(Barredo);
- **曼彻斯特大学**(Kynigos)。
- **谱系注记**: Ainsworth/Mukhanov/Jones 与 [41] Parallaft 完全重叠——即硬件异构容错 ParaMedic 线(DSN'18 [11]、DSN'19 [12]、HPCA'21 ParaDox [13])的原班人马;本篇是该谱系十年后的硬件正统续作(2025 年软件版 [41] 与硬件版本篇并行发表)。

## 企业合作证据(三级)

- **一级(作者机构)**: 无——6 作者全为学术机构(4 英国高校 + 1 西班牙超算中心),无企业任职。
- **二级(致谢/资助)**: **全文无致谢/资助节**(p.12 §VIII 结论后直接进入 p.13 参考文献)——如实记录缺失,非"无资助声明"。
- **三级(版权页)**: p.1 有 CC-BY 开放获取声明——英国机构开放获取政策的典型痕迹,但论文文本未注明具体资助方。
- **间接层(企业协作的显性证据)**: 无实质企业协作。技术依赖密集但全为公开材料:① 动机三件套——Meta Dixit [1][6]、Google Hochschild [2];② 建模基础**全部是 Arm 公开文档**——Cortex-X2/A510/A55 软件优化指南 [90][93][95]、X2 TRM [4]、Neoverse V2/E2 报道 [14];③ 面积估计用 Exynos die shot 像素计数 [77](三星 4LPE 工艺);④ 计算机会成本实验用 **Rockchip RK3588 现货开发板**(footnote 20,A76@2.4GHz+A55@1.8GHz);⑤ 引 IBM POWER RAS [5][33]/S390 G5 [43]、AMD EPYC RAS [34]、Intel Xeon RAS [3]/13 代混合架构 [73]、Compaq Tandem 白皮书 [44] 作商用容错现状;⑥ **引 Intel OpenDCDiag GitHub [79](2025)作为超大规模厂商 CPU 错误关注证据之一**(正文 §IV-E "recent focus on CPU errors across hyperscalers [1],[2],[18],[78],[79]"——即 sdcshield 的上游项目被本文引用)。**判定:零企业合作的纯学术研究**,但存在对 Arm 公开生态文档的深度单向技术依赖。

## 核心结论

1. **问题定位(§I/Abstract)**: 数据中心运营商已觉醒——缺陷硅计算单元导致的 SDC 在规模上已是常态(Meta [1]/Google [2] 报警),现有 RAS [3]–[5] 漏掉这些错误;软件扫描器已规模部署但"要么覆盖低、要么耗时数月"(最长 6 个月才把故障硬件移出生产 [6]),留下长期错误行为窗口;车规锁step [7][10] 使功率/面积翻倍,"对数据中心不现实"。**解空间两极分化(软件诊断 vs 全硬件锁step)之间缺一个服务器可负担的硬件方案**——这是本文出发点。
2. **核心思想(§I)**: **复用而非新增**——通过对重复运行的归纳并行(induction parallelism [11]–[13]),把一次运行按数据依赖切成段,在多个慢而高效的并行核上重放检查;**常规异构服务器芯片中的任一核可动态充当主核或 checker 核**;big.LITTLE 风格大小核动态配对 + 频率缩放,实现能效检测。关键反驳前作 [13]:16 个极小 checker 核不可行,因为真实异构系统的"小"核是超标量大核,应当直接复用而非另造专用核。
3. **两种运行模式(§I/§IV)**: **full-coverage 模式**捕获全部永久+瞬时错误(checker 耗尽时主核暂停等待);**opportunistic 模式**只检查资源空闲范围内的计算(关闭 checkpoint/日志,超额指令丢弃检查)——把可靠性做成运行时可调参数。
4. **微架构改动(§IV)**: 每核仅 **1064B** 存储开销:Load-Store Push Unit(512-bit 缓存行,主动把 loads/stores/不可重复事件推过 NoC 给 checker,免一致性协议)、Register Checkpointing Unit(776B)、13-bit 指令计时器、48B 2-wide Load-Store Comparator、前后端 LSL$ 索引各 16-bit、每缓存行 1 bit;**L1 数据缓存复用为线性 Load-Store Log(LSL$)**——512-bit 行 = 4 条目(7B 地址 + 1B 大小 + 8B 对齐载荷),一个额外 tag bit 区分数据行/日志行,64KiB 容量远超前作 3KiB 专用 SRAM [11];5000 指令超时上限;ISA 格式存储允许大小核微架构分歧。
5. **与前作定量对比(§I/§VII-A, Fig.6)**: DSN18 式 12 个专用 checker 不足(geomean 减速 9%);Paradox 式 16 个 checker 低减速(1.2%)但 **35% 面积开销**;ParaVerser 同构 checker 1.6% 减速、2×X2@1.5GHz ≈ 同构、4×A510@2GHz 3.4%(bwaves 因 fdiv 指令密集且大小核除法性能差距大而最差)——**用已在系统里的核实现同等能力,不占硅面积**。
6. **能耗结果(§VII-E/Abstract)**: 相对典型锁step 系统**同等保证下能耗开销降 70%**(95%→29%);McPAT 22nm 建模:4×A510@2GHz = 49%、2×X2@1.5GHz = 45%、同构 1×X2@3GHz(可对比双核锁step [10][45][46])= 95%;DVFS 扫频(2GHz→1.4GHz)找每基准 ED2P 最优点:**4×A510 ED2P 最优 = 29% 能耗 @ 4.3% 减速**;前作专用 checker [11][13] 25% 能耗 + 35% 面积,总代价并不优于复用已有 A510。
7. **覆盖率(§VII-B, Fig.8)**: opportunistic 模式 **94–99% 运行时指令覆盖 @ ~1% 开销**(3GHz X2 checker >98%;4×A510 = 97%/96%/95% @ 2/1.8/1.6GHz;bwaves 仅 71%——FP 密集);硬错误注入(标准传播模型 [53],单比特 stuck-at 于 FU 输出寄存器/LSQ 地址,注入在 checker 侧避免干扰主核):full-coverage 10M 指令注入中 **76% 检出、其余 24% 被正确掩蔽**(未改变执行——不是漏检);非掩蔽错误几乎全部在 100M 指令内被 1×A510@500MHz 检出;多数负载 100%,bwaves/deepsjeng/imagick/perlbench 87–99%,除 imagick 外 1×A510@1GHz 达 100%,imagick 需 2×A510@2GHz。
8. **NoC 敏感性与 Hash Mode(§IV-I/§VII-D, Fig.11)**: 慢 NoC(128-bit/1.5GHz)下 geomean 开销飙至 >15%——LSL 流量与 LLC 需求流量的次生竞争是主要开销源;**Hash Mode**(SHA-256,验证数据在源端哈希、只传输复现数据,128-bit 碰撞抗性 [88])将 LSL 流量削减 ≥50%,开销回到快 NoC 的 0.8% 以内;快 NoC(256-bit/2GHz,类 CMN-700 [100])本身 NoC 开销 1.5%(同构)/<1%(异构)。
9. **计算机会成本反视角(§VII-F)**: 用"这些小核做计算能加速多少"量化检查的真实代价:GAP 1 大+2 小核并行仅 **1.52× 加速**,而同配置 full-coverage 检查只 10% 开销;PARSEC 1 大+3 小仅 1.44× vs 检查 7.6%;同构 2 大核也只有 1.9×/1.8×——**重复执行对并行应用的检查比把小核用于并行计算更便宜**(并行应用扩展性差、内存密集、互相竞争);诚实声明"重复执行没有免费午餐":资源被完美占满时 full-coverage 最坏 2×,此时要么加异构资源、要么接受 opportunistic 的部分覆盖。实测在 RK3588 上进行(footnote 20)。
10. **面积重估(§VII-E)**: 用 Exynos die shot 像素计数重估:X2 ≈ 2.43mm²、A510 ≈ 0.44mm²(三星 4LPE);外推 A35(28nm TSMC→新工艺)16 个 ≈ 0.84mm² → 前作 [13] 的 35% 面积开销;**指出前作面积对比方法学不公平**(用 RISC-V 核面积对比 ARM 主核,而本技术与前作都要求同 ISA,双方都应取 Arm 核)。
11. **设计细节(§IV)**: checker 支持推测乱序执行(indexed-access 方案:解码时取推测索引、错误以精确异常在提交点抛出、squash 时调整索引);checker 提前唤醒但以 LSL$ 容量为限流器**永不超前主核**;多进程/多核(checkpoint 在中断/上下文切换时建立 = 每核同时只保一个进程;竞态在重放中精确复现首次执行;**checker 自身出错产生的"假阳性"仍是真错误**——检测是对称的,同样有助于淘汰不可靠核);取证 repeat-replay 缓冲 776B/核;**SoR = 核本身,边界在 LSQ**——checker 不重复地址翻译(PTW 冗余才能全覆盖晶体管,留作 [12] 式扩展)。
12. **服务器域定位(§I 脚注 1)**: **只检测不纠正**——回滚+动态 checkpoint(其 ParaMedic [12] 已实现)在检测机制之上只需再加 ~1% 开销;核心目标是 [6] 提出的**淘汰不可靠核**(retiring)——检测信息直接服务预测性维护 [16] 与老化预警 [17]。
13. **为什么是 CPU(§IV-E)**: 核/控制流代码(kernel 等)输入小变化→输出大变化,最不容错;caches 可用 ECC 覆盖而 CPU 核不能(脚注 2:cache controller 足够小可以锁step);GPU/NPU 的异构 trick 不适用(无单线程吞吐目标);**AI 时代 CPU 在 LLM 推理中的地位上升** [80][81]——"recent focus on CPU errors across hyperscalers [1],[2],[18],[78],[79]"。
14. **x86 可移植性(脚注 14)**: 概念适用于其他 ISA;x86 单宏操作(如 REP MOVS)处理数据超 LSL$ 容量时需微操作级匹配(而非常规宏操作级);x86 ISA 复杂度可能限制 checker 核能做得多小;**Intel 14 代小核与大核无独立电压域**(与 Arm/Apple 系统不同,自测 [89] 即 Parallaft),限制能效收益——但预期未来改变。
15. **实验方法学(§VI, Table I)**: ParaDox 模拟器 [13] 移植到 **gem5 v22.0.0.1**;指出前作用 gem5 MinorCPU 模型的 FPU 过简(所有 FP 一律 6 周期、除法可达 22 周期、无共享 FU 变延迟建模)——不可与本文详细模型公平比较,故另建基于 Cortex-A55 限标量模拟 A34/35(最小支持 AArch64 的顺序核)的专用 checker 模型;主核 3GHz X2(5 宽乱序、288-entry ROB、150 Int/256 FP 寄存器、64KiB MPP-TAGE);负载 SPECspeed 2017(10B 快进 + 1B 详细仿真)、GAP(跳过初始化)、PARSEC simmedium 跑完、4 核多进程随机 mix;NoC 延迟用 MM1 排队网络模型从 gem5 参数外推;4×4 mesh 布局(LLC 切片在中间 4 交叉点,主核取无 LLC 的交叉点,相邻核为 checker)。

## 分类学标注

- **根因机理**: 采信产业画像不自行研究——制造缺陷永久故障(FleetScanner 停产扫 6 个月 93% 为永久 [6] 相关叙述)、Ripple 在产扫描 70%;数据依赖性错误(Meta FPU 对特定输入返回 0 而非真值 [1]——mSWAT 类症状检测漏掉的典型);温度/电压条件性间歇错误;老化 [17]。
- **故障模式**: ① 永久错误(stuck-at,FU 输出/LSQ 地址);② 瞬时软错误;③ 间歇错误;④ 掩蔽错误(注入的 24% 不改变执行——正确不报);⑤ checker 侧错误(对称检测,假阳性也是真错误)。
- **检测技术**: **硬件微架构级冗余执行检测**——归纳并行分段 + 多 checker 并行重放 + LSQ 边界比较;两档覆盖(full/opportunistic);Hash Mode 流量压缩;规格:检出即报、时延受段长(≤5000 指令)+LSL$ 容量约束。**BIST vs 冗余执行分类学定位**: Ripple/FleetScanner/Silifuzz [9]/Harpocrates [18] 均属 BIST [19]–[21],本篇属全冗余执行——互补而非替代(BIST 不能覆盖瞬时错误、不能生成实际运行代码的代表性测试)。
- **处理技术**: **仅检测**——检测服务于淘汰不可靠核/预测性维护(检测→运维动作链);不纠正(纠正 = ParaMedic [12] 式回滚 +checkpoint,+1% 扩展,明确剥离);取证 repeat-replay 支持事后诊断。
- **生命周期**: **运行期在线检测**(生产环境连续/机会式)+ 支撑**预测性维护**(老化前预警);栈层级:**硬件(微架构)**——R6 批四篇中的硬件层顶点。

## 业界观点摘录

1. "Data-center operators have awoken to the fact that silent data corruption resulting from defective silicon compute units is endemic at scale."(Abstract——运营商觉醒叙事)
2. 软件扫描器 "either have low coverage or take months, leaving long windows of incorrect behaviour";"it can take up to six months [6] to remove faulty hardware from production"(§I——产业扫描器现状的负面定量)
3. 车规锁step 不可迁移论:"the halving in compute performance for a given area and power budget makes such a strategy unrealistic for data centers"(§I)
4. "Intel has implemented similar error correction and detection mechanisms for the CPU logic as IBM [3], but these mechanisms miss errors in Google and Meta data centers [1], [2]"(§II——对 IBM/Intel 现役 RAS 的直接否定性引用)
5. mSWAT 类症状检测 "would miss the SDCs that are plaguing the industry, such as the example highlighted by Meta (an FPU returning 0 for some inputs rather than the real value)"(§II)
6. "The emerging blight of silent-error detection can be mitigated more effectively at the hardware level than with software scanners alone"(§VIII 结论——硬件优先论)
7. AI 时代 CPU 论:LLM 推理中 CPU 需求上升 [80][81],CPU 错误的 hyperscaler 关注度激增 [1][2][18][78][79](§IV-E——含引 OpenDCDiag [79])
8. Intel 14 代小核无独立电压域(脚注 14,自测 [89])——商业混合架构对异构容错能效的现实约束("that is likely to change in future")。

## 关键数字表

| 数字 | 含义 | 出处 |
|---|---|---|
| 70% | 相对典型锁step 同等保证下的能耗开销降低 | Abstract |
| 4.3% | ED2P 最优配置性能减速 | Abstract/§VII-E |
| 1064B | 每核存储开销(LSPU 512b + RCU 776B + 计时器 13b + LSC 48B + 索引 2×16b + tag) | Abstract/§VII-E |
| 1–4% | full-coverage 模式 geomean 性能开销区间 | Abstract/§I |
| 95%→29% | 能耗开销两极端(同构锁step 级→ED2P 最优) | Abstract/§I/§VII-E |
| 94–99% | opportunistic 模式运行时指令覆盖 | Abstract/§VII-B |
| 1.6% / 9% / 1.2% | 同构 / DSN18 12-checker / Paradox 16-checker 减速 | §VII-A |
| 35% | Paradox 16 专用 checker 面积开销(RISC-V 基准重估后维持) | §I/§VII-E |
| 3.4% | 4×A510@2GHz full-coverage 减速 | §VII-A |
| 76% / 24% | 注入硬错误检出率 / 正确掩蔽率(10M 指令) | §VII-B |
| 87–99% | 最难负载(bwaves/deepsjeng/imagick/perlbench)最低配置检出率 | §VII-B |
| 71% | bwaves 在 4×A510@2GHz 下的指令覆盖(FP 密集最难) | §VII-B |
| 49% / 45% / 95% | 4×A510@2GHz / 2×X2@1.5GHz / 同构 X2@3GHz 能耗开销 | §VII-E |
| 29% | ED2P 最优 4×A510 能耗开销(@4.3% 减速) | §VII-E |
| >15% | 慢 NoC(128-bit/1.5GHz)下 geomean 开销 | §VII-D |
| ≥50% | Hash Mode 的 LSL 流量削减 | §IV-I/§VII-D |
| 1.52× / 1.44× | GAP/PARSEC 小核做计算的加速 vs 检查的 10%/7.6% 开销 | §VII-F |
| 2.43 / 0.44 mm² | X2/A510 面积(Exynos die shot,三星 4LPE) | §VII-E |
| 0.84 mm² | 16 个 A35 外推面积(→35% 面积开销) | §VII-E |
| 64KiB / 3KiB | LSL$(复用 L1D)vs 前作专用 SRAM 容量 | §IV |
| 5000 | 指令超时(段长上限) | §IV/Table I |
| 776B | 每核取证 repeat-replay 缓冲 | §IV |
| 7.6% | PARSEC 2 线程 full-coverage(3×A510/主核)减速 | §VII-C |
| 1% / <0.6% | 4 核多进程 mix 同构 / 4×A510 geomean 开销 | §VII-C |

## 方法论要点

1. **"复用而非新增"范式**: 不新增专用 checker(前作 35% 面积),把常规核动态角色化(任一核可主可查)——硬件版的"资源自适应"思想,与 [33] Orthrus(软件资源自适应)隔层呼应。
2. **归纳并行谱系的自我修正**: 同一团队十年四作(DSN'18→DSN'19→HPCA'21→DSN'25),本篇推翻自家"极小专用 checker"路线,转向"复用真实大核"——**谱系内范式转移的诚实记录**。
3. **BIST vs 冗余执行的互补性分类**: 明确各自不可替代的覆盖盲区(瞬态错误/实际运行代码的代表性),避免"谁取代谁"的伪命题——检测技术分类学的清洁切分。
4. **覆盖-开销可调谱**: full/opportunistic 双模式 + DVFS 扫频找 ED2P 点 + 时间采样 [69] 提议(脚注 18 诚实承认采样比例"高度依赖具体硅上具体核的具体故障行为")——可靠性作为运行时连续可调参数。
5. **模拟器诚实升级 + 基线双重修正**: 指出前作 MinorCPU FPU 过简(6 周期一刀切)、面积对比 RISC-V vs ARM 不公平——**对自家前作的两种方法学批判**,重建基线后再比较。
6. **计算机会成本反视角**: 用"小核做计算能加速多少"量化检查的真实代价——评估冗余方案的新维度,打破"开销百分比"单一视角。
7. **错误注入的诚实报告**: 区分"检出 76%"与"正确掩蔽 24%"(掩蔽≠漏检);对称性注入(checker 侧)避免干扰主核;多 FU 时"用哪个 FU 决定是否注入"的如实建模。
8. **跨 ISA 边界如实披露**: 脚注 14 的 x86 三重限制(宏操作匹配/核最小尺寸/电压域)来自自测 [89](Parallaft 实测),不回避自家方案的平台依赖。

## 横向对比注记

1. **vs [41] Parallaft(同团队前作,[89],CGO 2025)**: 软件版(运行时二进制级,15.9% 性能/44.3% 能耗,免重编译免硬件)vs 硬件版(微架构级,1–4% 性能/29–49% 能耗,1064B/核)——**同一归纳并行思想的跨栈层双实现,2025 年同年发表**;[41] 的 Intel 电压域实测被本文脚注 14 直接复用;构成综述"检测技术×栈层级"矩阵的教科书对照。
2. **vs [40] futures-replication**: 任务级双子(AMT,~80–100% 开销 + 负载均衡补偿,在线纠正)vs 微架构分段重放(1–4%,只检测)——R6 批"侵入性-开销-能力"三角的三个顶点已齐(任务级/二进制级/微架构级/指令级 [43])。
3. **vs [25] Silifuzz / [26] Harpocrates / [03] Ripple**: BIST 线(离线/在产扫描,月级时延,93%/70% 永久故障占比)vs 本文冗余执行(连续,瞬态也覆盖)——论文明确定位**互补**,本文补 BIST 两个结构性盲区。
4. **vs [33] Orthrus**: 资源自适应思想在软件层(云服务算力余量)与硬件层(核分配/DVFS)的对偶;两者都把"可靠性档位"做成运行时决策。
5. **vs [01][04]**: 动机直接引用其 fleet 数据([78]=[01] SOSP23;[1] Dixit arXiv);Meta FPU 返 0 例为关键反例(mSWAT 漏检论据);[6]=Ripple SELSE'22 的 6 个月/70% 数字被反复使用。
6. **vs [43] hpc-duplication(Didehban/Shrivastava 线,[23] nZDC 被引)**: 指令级选择性复制(编译器,需源码)vs 全段重放(硬件,免编译)——软件指令级与硬件段级的对照;[41] Table 1 已给出 SWIFT 45%/InCheck 197% 等软件级锚点。
7. **vs [22] Gem5-MARVEL([41] HPCA 2024 被引)**: 微架构级弹性分析工具与本文评估工具链同源(gem5)——仿真工具链复用。
8. **Arm 生态单向依赖**: 建模基础全部是 Arm 公开文档(TRM/SOG [90][93][95])+ Exynos die shot [77] + RK3588 实测——**零合作但深度技术依赖**,是"学术界站在 Arm 公开生态上做容错研究"的样本(综述企业生态叙事的间接层证据)。
9. **UK 学术生态系统**: 剑桥-爱丁堡-QMUL-曼彻斯特四校 + BSC;与 [41] 共享三位作者(Ainsworth/Mukhanov/Jones)——Cambridge 微架构容错组 DSN'18→CGO'25→DSN'25 谱系;"异构并行容错"这一研究纲领(idea→硬件→软件→硬件复用)由该组独立走完闭环。
10. **检测-淘汰闭环**: [6] Ripple 提出的"淘汰不可靠核"目标被本文接住——产业界提出需求(扫描数月时延太长),学术界给出微架构级答案;预测性维护 [16]/老化 [17]/混沌工程 [74] 的运维衔接;"false positive 也是真错误"的设计巧思直接服务核淘汰决策。

## 身份核实

- **DSN 2025 正式论文**,CC-BY 开放获取(p.1),16 页(p.16 空白);源码双渠道(Zenodo DOI + GitHub CompArchCam/DSN25-AE)——可复现性最高档。
- **作者 6 人 5 机构**(p.1 邮箱逐一列出,含 gmail 后缀的 Barredo/Kynigos——BSC/曼彻斯特 affiliation 明确标注);与 [41] 重叠 3 人,[89] 自引 + 脚注 "our own testing [89]" 确证团队延续。
- **无致谢/资助节**——如实记录;作者以英国机构为主,CC-BY 是英式开放获取政策痕迹,但无具体资助方可考。
- **文本层问题**: 双栏 PDF 提取交错——Table I 行序错乱(数值漂移到错误行)、Fig.6/7/9 benchmark 名连写("bwaves gcc xmaclafncbdmeek…")、部分脚注/图例文字混排;**关键数字经 §I 与 §VII-A/E 多处交叉验证一致**(9%/1.2%/35%/1.6%/3.4%/49%/45%/95%/29%),数字表取正文叙述为准,引表时需回 PDF 原表核对。
- **内部一致性**: Abstract 70%/4.3%/1064B 与正文逐一对上;95%×(1−70%)≈28.5%≈29% 自洽;94–99% 与 §VII-B 一致;1064B 分解(512b+776B+13b+48B+2×16b+tag)加总吻合。
- **诚实标记**: "no free lunch when it comes to duplicate execution"(§VII-F 最坏 2× 情形如实给出);脚注 18 承认采样比例硅依赖不直接研究;脚注 14 的 x86 三重限制;对自家前作 [13] 的面积方法学批判。
- **无矛盾发现**;一处表述需注意: Abstract "identical guarantees"(与锁step 同等保证)指 full-coverage 模式的检出保证,不含纠正——正文脚注 1 已明确剥离,不构成夸大。

# 微架构证据汇编：鲲鹏920 CPU179 核 LSU 装载数据返回通路间歇故障

> 用途：为学术论文的微架构根因论证与"启示"章节（SDC 规避/消减/暴露方法）提供旁证素材。
> 证据组织：按证据类型分节，每条注明来源文件路径（绝对路径）与关键原始数字；区分【已验证】（有真机 gem5 输出 / vmcore 可复核命令佐证）与【计划/推测】（预登记假设、代理模型、诚实阴性）。
> 取证日期：2026-09-03。所有数字均从本机文件原文摘录，未做二次换算（除明确标注的脚本计算）。


## 0. 证据地图（本文结构与故障判定结论的关系）

| 章节 | 证据类型 | 支撑的论文论点 |
|---|---|---|
| A | gem5 LSU/LSQ 转发注入（CHAOSLSQFwd 五模式 + AddrPath + PTW） | "装载返回通路结构故障（字节相位撕裂）可复现现场签名"，根因论证主体 |
| B | L1D ECC 风险反转实验 | "ECC 对单比特全吸收 → 现场多比特结构撕裂 + RAS 静默自洽" |
| C | PRF/RAT/ROB 对照单元注入 | "排除寄存器堆/重命名等替代根因"（阴性/对照证据） |
| D | 现场实验（-30mV 欠压、l1d_disable、method3 相位、七案普查） | 电压裕量×发射相位机制、间歇性、单核私有性 |
| E | silifuzz 单板运行记录 | SDC 暴露方法的有效性边界（定向生成 vs 随机） |
| F | sdc_long 用户态探针逆向 | 针对该故障的活体捕获陷阱设计（可作论文方法学素材） |
| G | V110 微架构参数基线 | D1/D2/D3 三通路映射的物理依据 |
| H | 启示章节素材汇总 | 规避/消减/暴露三层建议的实验支撑点 |


## A. gem5 故障注入：LSU / store→load 转发通路（与判定最直接相关）

### A1. LSQ 转发故障模式矩阵（formal，n=64/cell）【已验证】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/artifacts/lsq-matrix/fpfwd_*.csv`、`/home/sdc/wangxu/gem5-fi-wangxu/docs/paper/tables/t3-lsq-matrix.md`、progress.md（2026-09-02 续，commit `4f032007`）

fp_fwd_kernel（asm back-to-back store→load 转发 probe）× 5 故障模式 × n=64，单故障 max_faults=1，fail_count oracle：

| 故障模式 | SDC | Masked | P_SDC | 与本故障的关系 |
|---|---|---|---|---|
| bitflip（F1 单比特） | 64/64 | 0 | 1.000 | 基线：转发数据任一比特翻转必致 SDC |
| **structural（byte_lane_skew rol1）** | 64/64 | 0 | 1.000 | core179 D1 撕裂移位的直接模型 |
| **phase（phase_offset=2）** | 64/64 | 0 | 1.000 | F6 相位偏移 = "发射相位×时序边界"机制的模型 |
| fwd_source_sub（F5 错源） | 0/64 | 64 | 0.000 | 诚实阴性（见下） |
| stale_line_replay（陈旧行回放） | 0/64 | 64 | 0.000 | 诚实阴性（见下） |

关键细节：
- bitflip/structural/phase 三模式 SDC 率全部 64/64 = 100%。在转发通路上，只要故障落到被消费的转发数据，传播就是确定性的，不存在"掩蔽缓冲"。
- fwdsrc/stale 的 Masked 阴性不代表"F5 错源无害"：注入确认发生（numFwdSourceSub/numStaleLineReplay 计数=1），但 fp_fwd_kernel 是同址转发，替换源后 ring buffer 内仍是同 vaddr 的等值数据（fails=0）。这是单几何 kernel 的限制，多几何转发需新 kernel（asm 构造不同地址候选），论文中须如实标注。
- 原 7 几何轴（fwd_7case）被诚实废弃：volatile-no-barrier C 模式在 -O2 下不触达 gem5 转发路径（注入日志 0 字节）。

### A2. byte_lane_skew 复现 core179 D1 撕裂签名（H5）【已验证】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/docs/KUNPENG920-SDC研究方案-系统完备版.md` §5.0 锚点表、§6.1 H5 行；`/home/sdc/wangxu/gem5-fi-wangxu/docs/cases/core179-microarch-rootcause-synthesis/FI_DESIGN_SUPPLEMENT.md` 执行状态节

主线单注入锚点（CHAOSLSQFwd byte_lane_skew rol1，fp_fwd_kernel，max_faults=1）：
```
actual=003ffad444e28000   （golden 右旋 1 字节）
xor=3fc52e90a6628000      （多位散布）
```
rol1 签名方向与现场 XOR 汉明重量 35/36 均匀散布一致："整字节通道旋转"在仿真注入下产生与现场同族的多位散布 XOR，而非位翻转的孤立单 bit。

侧分支端到端闭环（分支 `origin/fi-h6-h7-fs-verify`，ptrskew_kernel）：
- golden 0-fail；注入 `byte_lane_skew rot1 prob=0.05` → `numStructuralByteLaneSkew=30`，28 PTR_CORRUPT 检出（93%），fails=28；多 seed 可复现。
- 该闭环使用 `__per_cpu_offset` 装载-使用-作指针的 kernel，与 CPU179 内核崩溃链（ldr x20,[x0,w25,sxtw#3] → add x27 → ldr x23,[x27,#288]）同构。
- 注意诚信标注：主线 `8320daf` 已补齐 structuralFault 并验证 rol1 签名；93% 检出率的 formal ptrskew 复现在主线待跑（方案 §6.1 H5 行）。

H5 的可证伪逻辑（FI_DESIGN_SUPPLEMENT §2）：若仅位翻转注入能复现撕裂签名、结构故障不能，则 H5 证伪、D1 应为位翻转。实测结构故障（字节旋转）复现成功 → 支持"D1 是结构性字节重路由而非位翻转"。

### A3. 相位敏感性（H9 / F6 phaseOffset）【已验证-方向性】

来源：progress.md（2026-09-01，commit `17367bb`）；方案 §6.1 H9 行；`artifacts/lsq-matrix/fpfwd_phase.csv`

- gem5 侧：phase_offset=2（返回历史 N 步前数据，gem5 同步转发的诚实相位代理）→ `numPhaseOffset=3`，SDC 64/64（P=1.000）；StaleVaddr≠当前 vaddr 证相位错位。offset≥1 即 100% SDC（N=0 时为 0）。
- 现场对照（method3）：热路径加一条语义 no-op ALU 指令 → 触发率 100% → 10–20%（probe H=1/10、X=1/5，方向性非精确率）。1 个发射槽的相位移动即改变触发概率量级。
- 塌方比 ≥5× 成立（offset≥1 即 100% vs 现场加 no-op 后 10–20%），方向一致；绝对值差异为代理 kernel 单几何限制（gem5 发射时序 ≠ V110），诚实标注为 E3。
- 论证价值："故障概率对发射相位极度敏感"是间歇性（MTBF≈5h、0.000s~343330s 间隔双态分布）的微架构解释：只有特定相位窗口命中时序边界违例。

### A4. 位谱（bit spectrum）仿真-现场对照【已验证】

工具：`/home/sdc/wangxu/gem5-fi-wangxu/fi_research/bit_spectrum.py`（P6，输出 sign/exp/mantissa/popcount）；现场数据 `/home/sdc/wangxu/gem5-fi-wangxu/docs/reproduce-method3.md` §6

现场（method3，全部 core179，36 样本/562 翻转位）：

| 路径 | mantissa | exponent | sign | popcount |
|---|---|---|---|---|
| float32（207 位） | 177（85%） | 29（14%） | 1（0%） | 中位 12（6–21） |
| double64（355 位） | 331（93%） | 24（6%） | 0（0%） | 中位 28（20–39，最密） |
| SVD（迭代型） | — | — | — | 中位 3，5/11 恰 1 位；double2 复测恰 1 bit（bit0，1-ULP） |
| mrn_rmw（整数 RMW） | — | — | — | 35/64 位（54%，整数最密） |
| movbe（整数） | — | — | — | 14–21 位，恒多位，xor 每次不同（无卡死位） |

gem5 复现（CHAOSLSQFwd → fp_fwd_kernel，P4→P6 管线，EXPERIMENT_DESIGN §12.1）：
```
SDC@it=0 i=98  golden=3ff31fa3dda00000 actual=3ff376a3dda00000 xor=0000690000000000
SDC@it=0 i=173 golden=3ff38e7a81000000 actual=3ff38e7a81008800 xor=0000000000008800
P6: sign 0(0%)  exponent 0(0%)  mantissa 10(100%)  popcount median 4
=> MATCHES method2 v3 §6 data-path-corruption signature
```
- 单注入锚点 XOR `0x04000000` 落在 double 尾数高位（bit 30，非符号位），与现场"尾数主导/符号免疫"方向一致。
- 整数路径 `int_rmw`：`xor=000000002a000000` 多位单字节翻转，符合 method2 §8.2 整数签名。
- 指纹库（`/home/sdc/wangxu/gem5-fi-wangxu/docs/paper/tables/fingerprint-library.json`）：lsq_fwd 指纹 mantissa_share=0.7121、sign_exp_share=0.2879、popcount_median=16（n=6）；CLI `tools/sdc_fingerprint.py` 支持"现场 xor → Top-K 候选单元"反查。
- 论证价值："尾数集中+符号免疫+路径相关计数"排除 ALU 语义错误（会产生系统性指数错）与单 bit SEU，指向数据通路/缓存行级损坏，与 LSU 装载返回通路判定互洽。

### A5. D2 地址通路（CHAOSAddrPath）【已验证-FS 单注入】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/docs/cases/core179-microarch-rootcause-synthesis/FI_DESIGN_SUPPLEMENT.md`（FS-mode 验证进展节，patch `0ff3ce5`）；方案 §6.1 H6 行

- FS 下复现 byte7 清零签名：prob=0.001、400M tick、seed 42 → `numAddrFaults=1`，日志 `Orig: 0xffffffc008b08f30 → Corrupted: 0xffffc008b08f30`（byte7 清零使规范内核地址变非规范），正是 core179 D2 签名（arch MSB=ff 但 MMU 看到 byte7=00）。
- SE 模式 D2-only 50 注入 → 0 可观察失败（根因已源码闭环：SE 物理内存从 0 起、仅 512MiB，byte7 清零后仍落物理内存；`mmu.cc:1213` SCTLR.M=0 走 translateMmuOff）。
- D2 load 钩子密度 ≈ D3 walk 密度的 3500×（400M tick：61081 loads vs 17 walks），地址通路采样基数充足。
- 现场对照：FAR 高 16 位与寄存器高 16 位不一致现象在 7 案中 3 案出现（08-14 x27 高16=d936 vs FAR=0036；08-24 x3 高8=0x55 vs FAR=0x00；08-31 x27 高16=a000 vs FAR=0000，低 48 位差恰 0x120）。该现象支持"异常注入窗口内多次受扰"，但不能排除 MMU 对非规范地址的 FAR 截位（两种解释并存，如实标注）。
- 诚实边界：H6 的 2×2 谱可分性（D1-only vs D2-only vs D1+D2）定量未确立。这是"单缺陷三投影 vs 多缺陷共址"裁决的仿真侧代理，尚未产出（论文中作为 open question）。

### A6. D3 PTW 通路（CHAOSPTW / H7 ECC 对照）【已验证-机制级】

来源：FI_DESIGN_SUPPLEMENT（H7 ECC 对照实验结果节，2026-08-27）；progress.md（`de48432`、`c82e59a`）；方案 §6.1 H7 行

Linux 内核态启动期（FS，57B tick，prob=1e-3，seed 42，两臂）：

| 模式 | numHooksCalled | numFaultsInjected | numSpuriousFaults | numBenignFlips |
|---|---|---|---|---|
| 单 bit XOR（ECC-off） | 37,305 | 40 | 0 | 40 |
| 单 bit XOR（ECC-on） | 54,149 | 0 | 0 | 60（全纠正） |
| clearValidBit（2-bit clear，ECC-off） | 37,305 | 40 | 40（100%） | 0 |

- ECC 纠正效应实证：ECC-on 把全部 1-bit flip 纠正为 benign（注入 40→0）；spurious 制造机制实证：clearValidBit（`data[0] &= ~0x3`，2-bit 不可纠正、绕过 ECC）把 40 注入全转为 spurious（`Orig 0x80600701 → Corrupted 0x80600700, BecameInvalid=1`）。
- 主线 FS checkpoint 流水线（`c82e59a`）：boot 890s → checkpoint → restore → PTW clearValidBit 3× `BecameInvalid:1` 从 Tick 220355816607 注入。
- 真实 walk 密度：内核态启动期 54074/78,286,260 inst = 0.069%（早期 boot 的 10 倍）；prob=0.5 时的 7963 注入是"坏 PTE→翻译错→重查→再注入"连锁放大，非真实密度（诚实修正）。
- 对 CPU179 D3（73 例 spurious）的含义：spurious 翻译故障可由"PTW 读出通路的 2-bit/结构性损坏"产生且 ECC-on 单比特全吸收，与现场"RAS 全静默 + 73 例重走成功"自洽：现场故障若在 PTW 读出，必须是超 ECC 纠错粒度的结构化损坏（或位于 ECC 覆盖之外的读出段）。
- 现场反驳合法竞态：72/73 FAR 落在静态线性映射（boot 写一次永不释放）+ 100% 单点 CPU179，不满足主线 Will Deacon 补丁的"并发新建映射"前提（DIAGNOSIS_REPORT §3.1）。


## B. L1D ECC 实验（风险反转）【已验证】

### B1. formal 数据（n=384/cell × 6 cell = 2304 runs）

来源：`/home/sdc/wangxu/gem5-fi-wangxu/artifacts/l1d-ecc/summary.md`、`raw-b1..3.csv`、`secded-b1..3.csv`；`docs/paper/tables/t2-l1d-riskreversal.md`

| cell（l1d_reduce，随机 block/byte，单故障） | 结果 |
|---|---|
| raw-b1 / raw-b2 / raw-b3（n=384 各） | Masked 384（100.0%） |
| secded-b1（n=384） | Corrected 384（100.0%） |
| secded-b2 / secded-b3（n=384 各） | Masked 384（100.0%） |

### B2. 机制锚点（S7-5，progress.md 2026-09-01 续）

- raw 2-bit：`numRawEscaped=1`（escape，数据留脏）；secded 2-bit：`numDetectedContained=1`（ECC 检出+毒化，contained DUE）。
- raw 1-bit：`numRawEscaped=1`；secded 1-bit：`numEccCorrected=1`（纠正恢复）。
- 风险反转结论：SECDED 把 raw 的 1-bit 逃逸 100% 转为 Corrected；2-bit 转为 DetectedContained（contained，不逃逸）。

### B3. 定向 L1D 注入（STATUS.md Directed-cache 节，patch `642dfef`）

- 随机 pilot（5 seed）5/5 Masked，属 cache AVF 采样效应：随机瞬态字节很少命中被读的活值。
- 定向到活数据（驻留块 862656，byte 0）：输出 `d128c62843ca82a1` ≠ golden → SDC，可复现（2/2 相同）；byte 4 → 不同 SDC `c104da9d94a173cd`。L1D SDC 在故障落到活数据字节时可达，"L1D 不敏感"不成立。
- L1I 定向到执行指令（块 51392 byte 38/0）→ Hang（指令字节翻→循环控制破坏→死循环）；随机 10 seed 10/10 Hang（已验证为真超时 exit 124 非 Crash 误判）。

### B4. 对 CPU179 判定的论证价值

1. 单比特 L1D 数据阵列故障会被 ECC 全额吸收（secded-b1 100% Corrected）→ 现场零 CE/UE 记录 + 撕裂形态（多比特结构化）共同指向：故障不在 ECC 可纠的 L1D 阵列单比特层，而在 fill-buffer/replay 合并级或读出组装级（ECC 校验点下游），与 MICROARCH_SUPPLEMENT §4 的推理一致。
2. 现场四种腐化形态（零塌缩/ROR8/ROL16/ROR16）均为字节/半字粒度相位撕裂，等效多比特，超 SECDED 单纠能力，且若发生在 ECC 后数据段则完全不受保护（PCE，见 H 节）。
3. 注意诚实边界：华为不公开 V110 逐结构保护表，方案用 Noverse N1 TRM Table 9-1 代理（E3）；"若 V110 L1D 实际无 ECC，D1 亦可为阵列读出失效，两种情形处置建议相同，但需供应商澄清"（MICROARCH_SUPPLEMENT §4）。


## C. 对照单元注入（PRF/RAT/ROB/窗口）：排除性证据【已验证】

### C1. PRF formal（X3 位段全扫）

来源：`/home/sdc/wangxu/gem5-fi-wangxu/artifacts/prf-formal/summary.md`、`docs/paper/tables/t1-prf-bits.md`

X3（数据累加器）× 8 位段 {0,11,12,31,32,47,48,63} × n=96（C2-KP V110 代理参数：ROB128/PRF int160/float192/LQ48/SQ42/4-wide/2.6GHz）：
- 每 cell SDC=96/96，P_SDC=1.000 [0.962,1.000]；合计 768/768，0 Hang / 0 Crash / 0 Masked。
- X3 任意单 bit 翻转确定性传播为 SDC，"数据累加器全位 SDC"正式确认。

### C2. X2/X3 寄存器角色区分（STATUS.md Grid 1）

| reg | bit0 | bit31 | bit32 | bit63 |
|---|---|---|---|---|
| X2（循环计数器） | SDC | Hang | Hang | Hang |
| X3（数据累加器） | SDC | SDC | SDC | SDC |

- SDC-vs-Hang 是按寄存器语义角色区分的（X2 高位→控制流破坏→Hang；X3→数据路径→SDC），不是笼统"高位→Hang"（旧说法被 64 位掩码修复后重跑否定）。
- 三次 Hang 均验证为真超时（exit 124、无 trap/SIGSEGV）。

### C3. h2-window 窗口扫描（H2 判定：本域内不成立）【已验证-阴性】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/artifacts/h2-window/summary.md`（12 cell × 96 rep = 1152 runs，commit `befc7db0`）

- ROB{96,128,160} × {X3bit0, X3bit63, X2bit0, X2bit63}：X3 全部 cell SDC=96/96（P=1.000 饱和），X2bit63 全部 Hang 0/96。
- H2（深窗口→SDC 高）在本域内不成立，属天花板效应：饱和 cell 无法分辨窗口梯度；需低概率/多 bit 未饱和 cell 重测（诚实标注，留后续）。
- read-trace 列：X3 cell 的 `reads_median=1,975,000`、`P_SDC_given_reads_gt0=1.0000`、`RT_SDC=96`（X3bit63 的 X2 Hang cell reads_median=7,450,000 但 RT_SDC=0）。

### C4. read-trace 传播闭环（H1/H3）

来源：`/home/sdc/wangxu/gem5-fi-wangxu/fi_research/read_trace_stats.py`（P7 四分类 Benign/Masked/SDC/Crash）；progress.md 锚点

- GPR SDC 锚点（X3 arch_frontend bit_flip）：注入后该物理寄存器 `reads_before_overwrite=25000/50000/75000/100000/125000, overwritten=0`，即 125,000+ 次读传播窗口，SDC 经长读窗口扩散（method1"状态泄漏"的仿真侧可观察代理）。
- 四分类把 AVF 分母拆细（reads=0 → Benign），使 AVF/SDC 跨研究可比，是方法学贡献。

### C5. method1 状态泄漏两臂 Fisher（冗余重算抑制）【已验证】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/artifacts/m1-formal-verdict.txt`、`m1-formal-num/summary.md`、`m1-formal-both/`；`docs/paper/tables/t4-method1-fisher.txt`（commit `38715ccc`）

F5 合法域替换（RAT 偷映射）× 长存活累加器（accum_kernel asm-pinned x9）× n=384 × 2 臂：
- numeric-only：SDC=114/148，P(history_residue)=0.7703 [0.6962,0.8307]（读回 donor 值 = method1"读回值=其它活变量"签名）；另 232/384 为 SimulatorError（偷映射→donor 作指针→SE page-table panic，method2 野指针形态在 gem5 SE 的分类边界，现场对应 DUE）。
- compute-both：SDC=0/266，P=0.0000 [0.0000,0.0113]：冗余重算（x10 独立累加交叉校验）完全抑制状态泄漏 SDC。
- Fisher exact（单侧）p=1.189e-71 << 0.05，PASS。
- 诚实边界：代理抑制比 ∞ 强于现场 [2,8]（numeric-only 1.0% ≈ 4× compute-both 0.27%），单注入无法双命中两份累加；cholesky V0-V7 载体阴性（40 冒烟 runs 全 Masked，d0 短存活）作诚实对照。

### C6. 对 CPU179 根因判定的排除逻辑（MICROARCH_SUPPLEMENT §5 + DIAGNOSIS_REPORT §5）

- 流程-A（PRF 活性误判）降级排除：① PTW 类事件完全不经过寄存器重命名；② 内核侧坏值是真实内存内容的相位错位副本而非"另一变量之值"，重命名混淆无法产生字节移位结构；③ 地址路径 FAR≠RF 无法用架构寄存器活性解释。
- PRF 单 bit 翻转在 gem5 中是持续性损坏（C1 的 100% SDC、125000 次读窗口），与现场一次性瞬态交付（下次读同地址正常、spurious 重走成功）不符，进一步支持故障在装载返回通路而非寄存器堆。
- U1（L1D way/列选通错）弱化保留：无法解释"恰好命中近期访问行"偏好（应送达任意同组现役行）；U4（fill-buffer/LQ 陈旧项回放 + 合并选路错位）采纳为最优模型（① 坏值=数组头历史行内容 ✓ ② 字节相位 k·8bit 错位=合并 mux 选路错 ✓ ③ 全零=空/无效槽位态 ✓ ④ PTW 读出同族受累 ✓）。


## D. 现场实验佐证（欠压 / 活体 / 缓解反证 / 七案普查）

### D1. -30mV 欠压复现（SDC1-01-02 案例）【已验证-现场】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/docs/reproduce-method2.md`（零、复现方法 + 二、关键证据链）；方案 §1.5 method2 行

- 复现方法：四路 CPU 的 VDDAVS 电压拉偏 -30mV（0.88V→0.85V，Vmin=0.73，Vmax=0.99），经 BMC maint_debug_cli I2C 写 VRD（MP2975，busid 7，addr 0xE0/0xC0/0xD4，val 0x21 0x7D 0x00）；随后运行 `./kunpeng-stl-kp920 -L 9999 -r all`。
- 欠压告警：CPU2 VDDAVS 实测下探 0.810V 触及阈值（12:05–12:09 四次 WARN）；复位后欠压未消除（VRD 寄存器保持）。
- 崩溃记录（全部 CPU 179）：
  - uptime 3014s：`find_busiest_group+0x1b8`，`ldr x21,[x10,#0xa0]`（f9405155），x10=`0x0ffe809021e0b2ae`，ESR=0x9600004（TF-L0）
  - uptime 480s：同指令，x10=`0xa23fa5817371856b`，x9=`0xa24000ffff5cd22b`（`__per_cpu_offset` 高 32 位非 0 形态：0ffe/a240/00ff）
  - uptime 722s：第三次；同日复现 ≥3 次、相同调用栈；复位码 0x2C00000F
- 论证价值：同一台机、同一 CPU179、同一 `__per_cpu_offset` 装载族，在 -30mV 电压裕量压缩 + 高负载下可复现。"电压裕量×负载"是该装载返回通路时序边界失效的现场实证（物理本质：small-delay-fault 类建立/保持违例）。这是论文"机制"部分连接微架构（相位撕裂）与物理层（时序裕量）的关键现场数据。
- 注意：该案例为更早内核（EulerOS 5.10.0-136），与七案普查（openEuler 6.6）不同 boot，但故障签名同族。

### D2. gem5-fi 在故障机上的"活体"报告【已验证-现场观察】

来源：FI_DESIGN_SUPPLEMENT 执行状态节（2026-08-26/27）

- "⚠️ 本机为故障机：编译全程 `taskset` 隔离 cpu179（见 /tmp/cpus.txt），但仍存在残余 SDC 风险，链接阶段曾出现多次瞬态 param-文件编译失败（SDC-affected 编译的典型表现），最终 -j1 单线程链接成功。验证结果需在第二台健康机复现才算最终确认。"
- "`boot_emm.arm64` open() 间歇返回 ENOENT，故障机 SDC 影响内核文件系统层"（无注入的纯 FS 启动同样受间歇性失败影响）。
- 论证价值：故障机的间歇 SDC 不仅命中内核调度器路径，也影响编译/文件 I/O 等通用路径："谁在 CPU179 上执行"无关、"在哪执行"有关（与七案普查 §7 的跨进程观察一致）。

### D3. l1d_disable 缓解反证（四组独立反例）【已验证-阴性】

来源：`/home/sdc/wangxu/vmcore0102/CROSS_CASE_STATISTICS.md` §5

| 案 | 实验 | 结果 |
|---|---|---|
| 案 4（15:42） | 4 轮加载，disable 累计 9,565s（2.66h），期间 0 WARNING | 卸载 3.7h 后 panic |
| 案 7（08-31） | 3 轮共 1,201s，期间 0 WARNING | 卸载 86.7h 后 panic |
| 案 5/6 | 从未加载 | 照样致命 |

- SCTLR_EL1.C 清零（L1D 禁用）对致命崩溃零抑制，处置建议明确"不要部署 l1d_disable"。
- 微架构含义（旁证）：仅关闭 L1D 数据阵列访问不足以绕过故障 → 故障点在装载返回通路的更深层（fill-buffer/replay 合并、读出选路）或 disable 路径并未真正旁路该段，与 U4 模型一致。

### D4. method3 触发条件与相位塌方（现场受控实验）【已验证-现场】

来源：`/home/sdc/wangxu/gem5-fi-wangxu/docs/reproduce-method3.md` §3（v3，2026-08-20）

三必要条件（probe 移除任一 → PASS 归零）：

| Probe | 移除的条件 | 结果 |
|---|---|---|
| A | store（无存储副作用） | PASS，store 是必需的 |
| D | 地址推进（定址 store） | PASS，store 地址必须推进 |
| E | 同 LLC 域（跨 NUMA） | PASS，store 与 reload 须同 LLC 域 |
| F | 跨 cache line 推进（i&15 限单行） | PASS，store 必须跨行推进 |

相位塌方：

| Probe | 改变 | 触发率 |
|---|---|---|
| baseline | 无 | ~100%（5/5 seeds） |
| H | 加 `and x2,x19,x20`（语义 no-op，store↔reload 仍 back-to-back） | ~10%（1/10） |
| X | 加 `eor w?,w?,wzr`（语义 no-op，back-to-back 被打破） | ~20%（1/5） |

- 判别式是指令调度时序相位而非 back-to-back 相邻性；1 个发射槽的相位移动使触发率塌方一个量级。
- 诚实标注：H=1/10、X=1/5 差异不显著（方向性结论）。
- 跨路径确认：8 测试 × 15min 受控 campaign，6/8 触发（movbe/mrn_rmw/float/double 各原始与 dump 变体成对等失败数，dump 插桩不扰动触发）；6 类无关负载（字节交换/整数 RMW/float GEMM/double GEMM/cdouble/SVD）同核同病 → 缺陷非指令特异。
- 用户态 memcpy1（零 ALU、纯 load→store→load 往返）也触发，强化"缺陷在访存路径而非任何算术单元"。

### D5. 七案普查关键数字（ vmcore0102 同目录）【已验证-现场】

来源：`/home/sdc/wangxu/vmcore0102/CROSS_CASE_STATISTICS.md`（2026-09-03，全部数字为对 7 份 vmcore-dmesg.txt 的重新统计）

- 102 起事件（95 WARNING + 7 致命 Oops）100% CPU179；其余 191 核在七开机累计约 397.5 小时零事件。
- 6/7 致命命中同一指令 `find_busiest_group+0x140`（Code 五指令字逐字相同：`f9400782 f879d814 2a1903e0 8b14003b (f9409377)`）；1/7 在 `bio_add_page+0xf0`（`ldr x1,[x3]`，其 x3 来自上一条缩放变址装载 `ldr x3,[x3,x2]` 的返回值）。
- 四种腐化形态完备：零塌缩（x20=0，FSC=L3 pte=0）、ROR8（≫8）、ROL16、ROR16（08-31 新子族 x20=`a000ffffbe56fb25`，ROL16(x20)=`ffffbe56fb25a000` 高16=ffff 页对齐）。全部为字节/半字粒度相位撕裂，无位翻转形态。
- 寄存器-内存证据（案 5）：`__per_cpu_offset[146]` 内存真值 `ffffcc879ed92000` vs 寄存器实收 `00ffffcc879da2e0` = offset[0]（ffffcc879da2e000）右移 1 字节，Hamming 距离 0。
- 代数闭合：x27=(x1+x20) mod 2^64 于 7/7 逐位闭合（含 bio 案 FAR=x3 低 48 位）；FAR=x27+0x120（低 48 位）6/6。
- 位翻转等价性证伪（MICROARCH_SUPPLEMENT §2.2 穷举）：坏值无法由 slot[0] 任何单字节 bit-flip 产生（8 字节 × 256 掩码无命中）；192 槽 × 8 旋转 = 1536 候选中坏值唯一命中数组头部（两例概率 ≈ 2⁻⁵⁸）。
- 组相联几何裁决：坏值源（offset[0]@set 87）与装载目标（offset[146]@set 105）不同 set → 排除 L1D way/列选通错（U1）为主因，强化 fill-buffer 合并级（跨 set 陈旧行回放）。
- ESR 分布：spurious 92/95（96.8%）= 0x96000044（WnR=1 写）、3 = 0x96000004（读）。读、写、页表遍历三类访存全部受扰，指向 LSU/DCU 公共返回通路。
- 间隔分布：min=0.000s、max=343,330s（95.4h）、median=270s，双态（周期性 procfs 读驱动的簇 + 与触发节律解耦的超长静默）；3/7 案最后事件距 panic <20s（临近致命时事件加密的"相变"特征）；案 5（15:58）无前兆直接致命，前兆监控有效率 6/7。
- RAS 静默：7 案零 CE/UE；rasnode.ko（案 6）192 核 × 5 ERR 节点，CPU179 五节点 FR/CTLR/STATUS/ADDR/MISC 读数与其余 191 核逐位一致（如 node0: FR=0x4842 CTLR=0x101 STATUS=0xff）。
- 08-31 新事实：x25=60（迭代号首次 <100，"雷区不在特定被遍历对象，而在遍历循环本身在 CPU179 上的执行"）；x22≠x26（差 0x60，破前六案"不变式"，该不变式降级为高频巧合）。


## E. silifuzz 单板运行记录

### E1. 检出链路验证（激发→检出→部署证据链）【已验证】

来源：`/home/sdc/wangxu/silifuzz/docs/kunpeng920_sdc_research_report.md`（2026-08-26，分支 feat/sdc-detection-cases-kunpeng920）

- 激发（gem5-fi）：50 次单 bit 翻转注入 → 2 diverge，SDC 检出率 4.0%；其一命中 `integer[9] bit19` → SUM `...748788→...6217780` + CRC `5b8846f3→a8d05814` 双 diverge。扩大规模：500 次注入（417 有效）→ 18 干净 diverge，4.3%；最敏感寄存器 integer[9]（5 次）、[12]/[1]/[7]（各 3 次）。多 bit（max-faults=3）50 次 → 4 diverge 8.0%（单 bit 翻倍，多 bit 更难掩蔽）。
- 检出（silifuzz 注错）：篡改代码字节后 runner 精准检出 `outcome=3 (end-state mismatch)`，报出翻转寄存器值，检出链路对单寄存器位翻转敏感。
- 部署（真机）：19 个微架构定向压力模板（覆盖 7 薄弱模块 MMU 20%/L2C 40%/LSU 54%/OoO 56%/IEX 70%/FSU 80%/IFU 66% 基线覆盖率）→ 125 snapshot（65 确定性 + 60 Centipede 探索）部署 0101/0102/0103；3 单板 ~446 核满载分布式扫描，总真 SDC=0（严格分类修正后：真 SDC=outcome 2/3/4，5/6=噪声）。
- exp03（local-0103，30min）：SDC=0，play_count=3840，噪声全分类；exp04（remote-0101，600s）：SDC=0，远程全链路（注册→部署→扫描→回收）通过。

### E2. 故障机上的 silifuzz 运行（案 7 dmesg 交叉引用）【已验证-观察】

来源：`/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-31-00:47:32/vmcore-dmesg.txt`（行 2814-2825）；CROSS_CASE_STATISTICS §7

- `silifuzz_orches` 5 次 memfd_create 记录 @18149/21472/21943/22675/80947s（编排器生成测试载荷二进制），最后活动距 panic 87.5h。
- silifuzz 活动窗口（18.1kh–81.0kh）与 WARNING 静默期（13.4kh–282.1kh）重叠：压测在跑、故障静默，负载强度与发作频率无正相关。
- 论文价值（诚实）：通用 end-state 比对式模糊测试在故障机上未捕获该间歇单核装载通路故障，因为故障只在特定微架构相位窗口触发、且多数损坏被下游消费前未形成端态分歧。这正是"SDC 暴露方法"章节需要定向生成（E3/E4）的动机。

### E3. 定向进化 vs 随机（byte_lane_skew 结构故障检出率）【已验证-极显著】

来源：`/home/sdc/wangxu/silifuzz/docs/kunpeng920_sdc_research_report.md` §7.1 表（F4/完整 D13 数据）；`/home/sdc/wangxu/silifuzz/docs/experiments/2026-09-02-sdcfuzz-verification-report.md` exp02

gem5+CHAOS 注入检出率对照（各 n=500，Fisher 精确检验）：

| 引擎 | bit-flip 检出率 | 结构故障（byte_lane_skew）检出率 | vs 随机 B |
|---|---|---|---|
| A（朴素字典） | 3.9%（18/458） | 2.0%（10/500） | 显著更差（C/B=0.46×, p=0.0083 / 0.33×, p=0.0001，静态字典因逻辑掩蔽失败） |
| B（SiliFuzz 式随机） | 8.0%（40/500） | 8.4%（42/500） | 基线 |
| D8（carry/toggle 去 volatile + lsu 保留 volatile） | 3.2% | 26.6%（133/500） | 3.17×，z=7.57，p<0.001 |
| D10（全 volatile+多引用+广操作数） | 8.0%（持平） | 17.0%（85/500） | 2.02×，z=4.08，p=0.000044 |
| **D13（随机+定向变异选择）** | 24.6%（123/500） | 65.4%（327/500） | 3.07× / 7.79×，z=7.11/18.68，均 p<0.001 |

- 复验（exp02，100-run 规模）：D13 bit 22%（D/B=3.143×，p=0.00429）、struct 64%（D/B=12.8×，p=5.6e-20）：BEAT。
- D8 成功机制：volatile 混合 → forwarding + 寄存器双路径 → byte_lane_skew diverge 暴增。
- 根因（AVF 定理）：B 赢在 ACE-比例高（随机分散输出相关数据），不是"无数学结构"；静态字典因逻辑掩蔽失败。
- 论文价值：针对本文故障类（byte_lane_skew = core179 D1 模型）的检测用例生成，定向操作数/序列进化比 SiliFuzz 随机高 7.79×，是"SDC 暴露"方法的核心量化支撑。

### E4. E8 功耗应力与检出率（方向性信号）【已验证-方向性/统计不足】

来源：`/home/sdc/wangxu/silifuzz/docs/experiments/2026-09-03-e8-power-sdc.md`

- A baseline 0/15=0.0%、B Type-I 持续高功耗 3/45=6.7%、C Type-II 高低交替 6/45=13.3%（真实 gem5，135 次 bit 注入；McPAT 功耗 + Unicorn toggle 代理同测）。
- A < B < C 单调上升；Type-II（振荡）> Type-I（持续），与 scheme §5.3 H2"跳变更易触发"方向一致（di/dt 电压波动 → 瞬时时序违例）。
- 统计 INSUFFICIENT（Fisher B-vs-C p=0.48、A-vs-C p=0.32；45 样本/组需 ~100+）。
- 三个诚实观察：McPAT duty 映射无组间区分度（三组同 2.5238W）；toggle_proxy 方向相反（应力块稀释 per-insn 均值，代理口径陷阱）；gem5 O3 无电压/时序模型，仿真中 Type-I/II 只是指令构成差异，真实 di/dt 功耗-SDC 因果需真机验证。
- 与 D1（-30mV 欠压现场复现）互证：仿真方向（功耗跳变↑检出）与现场（电压裕量压缩↑触发）同向。

### E5. 诚实阴性结果（防过度声称）【已验证-阴性】

来源：`/home/sdc/wangxu/silifuzz/docs/experiments/`（E7、exp01、exp05）

- E7（闭环演化 vs 纯随机）：4/60（6.7%）vs 3/60（5.0%），Fisher p=1 → TIE/INSUFFICIENT（4 代浅探索 + 种子近饱和）。
- exp01（A/B 基线复现）：A=5%、B=7%，B/A=1.4 < 1.5× 预注册阈值 → NOT_REPRODUCED（方向与 F3 一致，100-run CI 宽）。
- exp05（12 组 sim→HW 关联）：Spearman ρ=-0.2219、p=0.74733 → NOT_SIGNIFICANT（sim 面为 Unicorn T 代理混用，非 gem5 diverge 率）。
- E7 期间发现并修复的注入语义 bug（重要方法学）：CHAOS `--first-clock` 是 CPU cycles 而非 gem5 tick（CHAOSReg.cc Cycles 类型；tick/385 换算 @2.6GHz）。修复前注入永不触发（fault_injections.log 全空为证），修复后 20/20 触发。


## F. sdc_long 用户态探针逆向分析【已验证-二进制实存与功能还原】

对象：`/home/sdc/wangxu/vmcore0102/127.0.0.1-2026-08-24-18:03:07/sdc_long`（71,568 字节，ELF64 LSB aarch64 动态链接，未 strip，源文件名 sdc_long.c，GCC 12.3.1 openEuler；同目录另有 116GB vmcore 与 vmcore-dmesg.txt，案 3 转储）

### F1. 导入函数与输出格式（strings/objdump 实证）

- 导入：`sched_setaffinity`、`posix_memalign`、`clock_gettime`、`snprintf`、`write`、`open`、`printf`、`fflush`、`perror`、`strtol`、`strtoull`、`abort`。
- 格式串（.rodata @0x400d18-0x400de8）：
  - `HIT r=%llu err=%lu obs=%016lx xor=%016lx t=%llu\n`（文件输出形态，含耗时）
  - `HIT r=%llu obs=%016lx xor=%016lx\n`（stdout 形态）
  - `tick cpu=%d r=%llu err=%lu elapsed=%llu sink=%016lx\n`（心跳）
  - `DONE cpu=%d r=%llu err=%lu sink=%016lx\n`（收尾）
  - `setaffinity`（失败 perror）

### F2. 还原后的程序逻辑（main 反汇编逐段核实）

1. 绑核：`sched_setaffinity(0, 128, mask)`，默认 mask 置位 bit 179（`mov x0,#0xb3`=179 硬编码；argv[1] 可覆盖 CPU 号，>1023 拒绝）。绑核失败 perror 退出。
2. 缓冲区：`posix_memalign(&buf, 64, 0x80)`，64 字节对齐（恰一个 L1D cache line）；向 buf 前 64 字节（8 个 u64）循环写入同一个魔数 `0xffffd937172de000`。
   - 脚本核实：该魔数经 movk 三段拼装后 = `0xffffd937172de000`，逐位等于案 1（08-14）崩溃时装载目标 `__per_cpu_offset[176]` 的内存真值（寄存器实收为 `d93715ba0000ffff`，ROL16 族撕裂）。8 个连续同值 u64 恰好模拟 `__per_cpu_offset[]` 数组在一条 cache line 内的布局。
3. 热循环（无函数调用，全部寄存器化）：
   - `ldr x2, [buf]`，反复装载 line 首字；
   - `sink ^= x2`，sink 累积异或（每次装载都被架构消费，防 DCE/编译器消除，且与现场"装载值被下游消费"语义一致）；
   - `cmp x2, golden` / `b.ne`，与真值比较；不等则进入 HIT 路径；
   - 相位采样：`mul x1, iter, 0xc767074b22e90e21; ror x1,x1,#9; cmp x1, 0x15798ee230; b.hi`，基于迭代号的乘法-旋转伪随机 tick，阈值占比 ≈5e-9/迭代（约每 2×10⁸ 次迭代一次心跳）。
4. HIT 路径：err 计数 +1；`clock_gettime(CLOCK_MONOTONIC)` 取耗时；argv[3] 给定（`open(path, O_WRONLY|O_CREAT|O_APPEND, 0644)`，flags=0x441/mode=0644 实证）则 `snprintf` 进 160 字节栈缓冲后 `write(fd)`，否则 `printf` 到 stdout 并 `fflush`。
5. 收尾：迭代上限（argv[2]，strtoull，0=无限，实际经 `sub x24,x24,#1` 回绕为最大值）到达后打印 `DONE`。

### F3. 设计意图判定（对论文的方法学价值）

这是一个针对 CPU179 装载返回通路间歇故障的活体捕获陷阱，设计要素与本故障一一对应：

| 设计要素 | 对应故障特征 |
|---|---|
| 默认绑核 CPU179 | 100% 事件单核私有性 |
| 64B 对齐单 line + 8×percpu-offset 形态真值 | 复现 `__per_cpu_offset[]` 数组内单 line 布局与崩溃时的确切数据 |
| 魔数 = 案 1 装载目标的内存真值 | 使探针能区分"零塌缩/ROR8/ROL16/ROR16"四种撕裂形态（均 ≠ 真值）与正常读 |
| 纯 load 热循环（无 store） | 复刻内核崩溃模式（find_busiest_group 的 ldr 无前置 store；区别于 method3 的 store+load 转发模式） |
| sink 累积 | 每次装载架构可见，排除优化器消除 |
| HIT 行输出 obs+xor+迭代号+时间戳 | 一次命中即可定位撕裂形态（xor 直接对照四子族）与触发上下文 |
| tick 心跳 + DONE | 长时间无人值守运行的可观测性（_elapsed/sink 即活性证明） |

- 部署证据：该二进制出现在案 3（08-24，存活 149h、34 WARNING、bio_add_page 崩溃）的转储目录中，说明探针在故障机长存活窗口内实际部署过（CROSS_CASE §7 亦记载"sdc_long ELF 二进制，导入 sched_setaffinity/posix_memalign/clock_gettime，即绑核内存压测探针"）。
- 边界说明：dmesg 内无该进程名的 WARNING 记录（34 个 WARNING 中 33 个来自 irqbalance、1 个 bash），探针自身运行期间未命中故障（或命中未达内核告警路径）；其 HIT 输出（若在案 3 期间产生）需查探针自身的输出文件/终端记录，本目录未含。
- 论文用法：作为"现场捕获方法"的一环，用户态、零权限、可长期驻留的装载通路监视器；与 `grep "Ignoring spurious"` 内核侧前兆监控（有效率 6/7）互补。


## G. TaiShan V110 微架构参数基线（D1/D2/D3 映射的物理依据）

来源：`/home/sdc/wangxu/gem5-fi-wangxu/docs/kunpeng.md` §3（公开资料整理，E1 容量/E3 微结构估计）

| 单元 | 参数 | 与本故障的关联 |
|---|---|---|
| L1D | 64KB，4-way，64B line，ECC；2×128bit(16B)/周期（2 load 或 1 load+1 store） | 组相联几何裁决（set 87 vs 105 排除 way 选通）；字节相位错位的物理可行性边界（16B 通道） |
| LSU | 2×AGU；L1D hit load-to-use 4 周期（indexed +1–2）；store forwarding 6–7 周期，跨 16B 边界 +1–2 周期 | 地址生成与数据返回分离 → D1/D2 可分离；转发通路时序窗口 |
| dTLB / L2 TLB | 32-entry FA / 1024-entry（11-cycle hit） | D3 PTW 读出通路的对应结构 |
| 乱序中枢 | PRF-based 重命名；分布式四调度器（每调度器约 33 项）；flag rename 约 31 项；move elimination | C 节排除 PRF/RAT 根因的参照几何 |
| L2 | 每核私有 512KB，10-cycle | — |
| RAS | L1I/L1D ECC、内存毒化隔离、PCIe AER、MCA（标称 99.999%） | "RAS 全静默"的对照基线（宣称覆盖 vs 实际静默的张力） |

gem5 代理参数（C2-KP，`--kp920_proxy`，commit `1564328`）：4-wide、ROB128（扫描{96,128,160}）、PhysIntRegs160、PhysFloatRegs192、LQ48/SQ42、numIQEntries66、2.6GHz。E3 代理，非周期精确（统一 IQ ≠ 分布式四调度器；classic cache 无分区 L3；无 bufferless NoC）。


## H. "启示"章节素材汇总（SDC 规避 / 消减 / 暴露）

### H1. 规避（avoid：不让故障核承担关键计算）

| 措施 | 实验支撑 | 支撑章节 |
|---|---|---|
| 永久 offline CPU179 + RMA | 102/102 事件单核、191 核 397.5h 零事件 | D5 |
| 不要部署 l1d_disable（SCTLR_EL1.C 清零） | 四组独立反例（3.7h/86.7h 后仍 panic；未加载也致命） | D3 |
| 调度层规避：关键路径避开可疑核 | 触发与负载无关、与"在哪执行"有关（触发进程横跨 idle/kworker/RCU/守护/交互 shell/压测） | D5 §7 |
| 电压裕量管理（AVS/Vmin 校准） | -30mV 欠压使同通路复现（0.810V 触阈值） | D1 |

### H2. 消减（mitigate：故障发生但降低 SDC 概率/传播）

| 措施 | 实验支撑 | 支撑章节 |
|---|---|---|
| 冗余重算交叉校验（compute-both 型） | gem5 formal：numeric-only P=0.770 vs compute-both P=0.000，Fisher p=1.19e-71；现场 4× 抑制（1.0% vs 0.27%） | C5 |
| ECC 保护向装载返回通路下游延伸（fill-buffer 合并/读出组装段加 parity） | L1D ECC 对单 bit 100% Corrected 但对结构撕裂无效 → 保护必须覆盖 ECC 后数据段（PCE） | B1/B2/A6 |
| 转发决策与数据组装分离到不同流水级（抗相位） | 一条 no-op ALU 使触发率 100%→10–20%；gem5 phaseOffset |offset|≥1 → 100% SDC | A3/D4 |
| 编号/指针字段（RAT idx/freelist bit/转发源 seqNum/TLB pfn）加 parity 或 range-check | F5 六载体"合法域校验"反向证明：仿真器都须校验，硬件更应校验 | A1/C5 |
| PRF 活性回收双校验 + squash 写回溯 | spec_leak numSpecLeak=3（PhysReg 104/105/106 跳过归还）证明该路径可被单点跳过 | C 节（方案 §8.3） |

### H3. 暴露（expose：让潜伏 SDC 可被检测）

| 措施 | 实验支撑 | 支撑章节 |
|---|---|---|
| 定向操作数/序列生成 >> 随机（vs SiliFuzz） | byte_lane_skew 检出：D13=65.4% vs B=8.4%（7.79×，p<0.001）；bit-flip 24.6% vs 8.0%（3.07×） | E3 |
| volatile 混合构造 forwarding+寄存器双路径 | D8 结构故障 26.6%（3.17×，p<0.001） | E3 |
| 功耗跳变应力（Type-II 振荡）放大触发 | E8：0%→6.7%→13.3% 单调（方向性，统计不足需 ~100+/组） | E4 |
| 多 bit 注入/压力加倍 | max-faults=3 检出 8.0% vs 单 bit 4.3% | E1 |
| 内核侧前兆监控：`grep "Ignoring spurious kernel translation fault"` | 有效率 6/7（案 5 无前兆）；最后事件距 panic <20s 者 3/7（临近加密可预警） | D5 |
| 用户态装载通路陷阱探针（sdc_long 模式） | 绑核+单 line percpu 真值+load-verify+HIT 捕获，零权限长期驻留 | F |
| 位谱指纹库反查（现场 xor → Top-K 候选单元） | lsq_fwd 指纹 mantissa_share=0.7121/popcount_median=16 与现场签名方向一致；CLI tools/sdc_fingerprint.py | A4 |
| 微架构定向模板补覆盖率短板 | LSU 54%→、MMU 20%→、L2C 40%→（19 模板 7 模块） | E1 |

### H4. 暴露方法的边界（诚实）

- 通用 end-state 比对式 fuzz 在故障机上未捕获该间歇故障（silifuzz 活动窗与故障静默期重叠）：间歇性+相位依赖使随机快照式检测天然低效（PinDrop N10：单次阴性不可靠，须 ≤30 天重访连续测试）。
- sim→HW 组粒度统计关联未确立（ρ=-0.2219, p=0.747，且 sim 面为 Unicorn 代理），跨层验证仍是 open problem。
- 静态操作数字典输给随机（逻辑掩蔽，AVF 定理）："有数学结构"不等于"高 ACE 比例"。


## I. 已验证结论 vs 计划/推测 总表

### I1. 已验证（E2：真机 gem5 输出 / vmcore 可复核命令佐证）

| # | 结论 | 关键数字 | 来源 |
|---|---|---|---|
| 1 | LSQ 转发通路 bitflip/byte_lane_skew rol1/phaseOffset 三模式 SDC 率 100% | 64/64 × 3 | A1 |
| 2 | byte_lane_skew rol1 单注入产生多位散布 XOR（撕裂签名方向） | xor=3fc52e90a6628000 | A2 |
| 3 | 侧分支 H5 闭环：ptrskew rot1 93% 检出 | 28/30 | A2 |
| 4 | 相位偏移 ≥1 即 100% SDC（塌方比 ≥5× 方向成立） | 64/64 | A3 |
| 5 | 位谱仿真复现现场签名：mantissa 100%/sign 0%（gem5 3 样本）vs 现场 85–93%/0–1% | 见 A4 表 | A4 |
| 6 | L1D 风险反转：secded 1-bit 100% Corrected；raw 全 Masked | 384/384 × 6 cell | B1 |
| 7 | L1D 定向活数据注入可达 SDC（随机为 AVF 掩蔽） | d128c62843ca82a1（2/2 复现） | B3 |
| 8 | PRF X3 全 8 位段 SDC=1.000（数据累加器任意单 bit 确定性传播） | 768/768 | C1 |
| 9 | X2 高位→Hang / X3 全位→SDC（寄存器角色决定归宿） | SDC=5 Hang=3 | C2 |
| 10 | H2 窗口敏感性本域不成立（天花板效应） | 12 cell × 96 全饱和 | C3 |
| 11 | read-trace：注入值 125,000+ 次读传播窗口 | reads_before_overwrite=125000 | C4 |
| 12 | method1 两臂 Fisher PASS：冗余重算完全抑制状态泄漏 SDC | p=1.189e-71 | C5 |
| 13 | PTW ECC-on 单 bit 全纠正（40→0）；clearValidBit 2-bit 100% spurious | 40/40 | A6 |
| 14 | FS 复现 D2 byte7 清零签名 | 0xffffffc008b08f30→0xffffc008b08f30 | A5 |
| 15 | -30mV 欠压 + STL 在 CPU179 复现 __per_cpu_offset 族崩溃（≥3 次） | 0.810V；x9=a24000ffff5cd22b | D1 |
| 16 | l1d_disable 四组反例零抑制 | 3.7h/86.7h 后 panic | D3 |
| 17 | method3 三必要条件 + no-op 相位塌方 | 100%→10–20% | D4 |
| 18 | 七案 102 事件 100% CPU179；6/7 同指令；四撕裂子族；代数闭合 7/7 | 见 D5 | D5 |
| 19 | 位翻转等价性穷举证伪（D1 为结构性字节重路由） | 8B×256 掩码无命中 | D5/MICROARCH |
| 20 | silifuzz 检出链路端到端（gem5 4.0–4.3% 激发 + outcome=3 检出 + 3 板 SDC=0） | 2/50、18/417 | E1 |
| 21 | D13 定向生成对 byte_lane_skew 检出 7.79× 极显著超随机 | 65.4% vs 8.4%，p<0.001 | E3 |
| 22 | E8 功耗应力单调方向（0%→6.7%→13.3%，统计不足） | Fisher p=0.32–0.48 | E4 |
| 23 | sdc_long 探针功能还原（绑核179/单line真值/load-verify/HIT 捕获） | 魔数=ffffd937172de000 | F |

### I2. 计划 / 推测 / 诚实阴性（不得作为已证结论引用）

| # | 项 | 状态 | 来源 |
|---|---|---|---|
| 1 | H6 谱可分性（D1-only vs D2-only vs D1+D2），单/多缺陷裁决的仿真代理 | 未产出（需 FS 多臂长跑）；单/多缺陷裁决在软件层不可解，须 RTL/DFT（scan-at-speed 分别对 fill-buffer 合并级/地址 byte7 锁存/PTW 读出施 LBIST） | A5/MICROARCH §3 |
| 2 | H5 主线 93% formal 复现 | 侧分支已闭环，主线 ptrskew formal 待跑 | A2 |
| 3 | fwd_source_sub/stale 的 Masked 推广 | 单几何限制（同址转发等值）；多几何 kernel 待写 | A1 |
| 4 | H2 窗口效应 | 本域不成立；需未饱和 cell 重测 | C3 |
| 5 | E8 功耗-SDC 因果 | gem5 无电压/时序模型，Type-I/II 仅为指令构成差异；需真机 | E4 |
| 6 | sim→HW 组粒度关联 | NOT_SIGNIFICANT（ρ=-0.22, p=0.75） | E5 |
| 7 | E7 进化 vs 随机（代际） | TIE/INSUFFICIENT（6.7% vs 5.0%） | E5 |
| 8 | V110 逐结构保护表 | N1 TRM Table 9-1 代理（E3）；"L1D 无 ECC 则 D1 可为阵列读出失效"两解并存 | B4 |
| 9 | 现场 FAR 高位 vs 寄存器高位不一致的解释 | "多次受扰"与"MMU 非规范地址截位"两解释并存 | A5/D5 |
| 10 | x22==x26 "不变式" | 被案 7 打破（差 0x60），降级为高频巧合 | D5 |
| 11 | 首症时刻前移趋势 | 被案 7 打断（2.9h），无单调趋势，不宜拟合 MTBF | D5 |
| 12 | 所有 P_SDC 为 gem5 O3 代理条件概率 | 非产品 FIT（无 raw device rate，不换算） | 各 artifacts summary 诚实边界 |
| 13 | 现场数据单机未确认 | 未在第二台健康机复现（S6-1 至今未做） | 方案 §9.4 |


## J. 来源文件索引（绝对路径）

gem5-fi 主仓库（`/home/sdc/wangxu/gem5-fi-wangxu/`）：
- `progress.md`：全部 commit 级 provenance 与锚点（71,597 字节，至 2026-09-03）
- `docs/KUNPENG920-SDC研究方案-系统完备版.md`：系统方案（§5.0 锚点表、§6.1 H1–H10 闭环状态、§8.3 抗 SDC 机制建议）
- `docs/arm64-sdc-STATUS.md`：工具闸门 G0–G7 与 P0 诚实重跑（Grid 1/2b/3b/4、Directed-cache）
- `docs/arm64-fi-plan-based-on-CHAOS.md`：原始方案（七闸门、F1–F6、E1–E4 证据等级）
- `docs/paper/sdc-fi-paper.md` + `docs/paper/tables/{t1-prf-bits,t2-l1d-riskreversal,t3-lsq-matrix,t4-method1-fisher,t5-anchors,fingerprint-library}`：论文骨架与四组正式数据集
- `fi_research/{bit_spectrum.py, read_trace_stats.py, EXPERIMENT_DESIGN.md, probes/}`：P6/P7 分析工具与 H0–H4 假设体系（§12 闭环验证）
- `artifacts/l1d-ecc/`、`artifacts/lsq-matrix/`、`artifacts/prf-formal/`、`artifacts/h2-window/`、`artifacts/m1-formal-{num,both}/`、`artifacts/m1-formal-verdict.txt`、`artifacts/s7b/`：formal 数据（cells.csv + summary.md）
- `docs/cases/core179-microarch-rootcause-synthesis/{DIAGNOSIS_REPORT, MICROARCH_SUPPLEMENT, FI_DESIGN_SUPPLEMENT, paper_zh}.md`：五转储取证、D1/D2/D3 三通路、注入器设计与侧分支验证
- `docs/reproduce-method2.md`：-30mV 欠压复现全记录（SDC1-01-02 案例）
- `docs/reproduce-method3.md`：method3 v3（触发条件/位谱/相位塌方/跨路径确认）
- `docs/kunpeng.md`：V110 微架构参数基线

故障机转储与普查（`/home/sdc/wangxu/vmcore0102/`）：
- `CROSS_CASE_STATISTICS.md`：七案法医级普查（102 事件、四子族、代数闭合、l1d_disable、RAS、负载时间线）
- `127.0.0.1-2026-08-24-18:03:07/sdc_long`：用户态装载通路探针二进制（本文 F 节逆向对象）
- `127.0.0.1-2026-08-26-10:37:27/DIAGNOSIS_REPORT.md`：第 6 案深度诊断（指令级反事实）
- `127.0.0.1-2026-08-31-00:47:32/vmcore-dmesg.txt`：案 7（含 silifuzz_orches 活动）

SDC 检测用例方案：
- `/home/sdc/wangxu/kunpeng920_sdc_plan.md`：三维压测空间/操作数字典/19 模板/覆盖率路线图

silifuzz（`/home/sdc/wangxu/silifuzz/`）：
- `docs/kunpeng920_sdc_research_report.md`：端到端研究报告（gem5-fi 激发 + silifuzz 检出 + 3 板扫描 + A–D13 引擎对比）
- `docs/experiments/2026-09-02-sdcfuzz-verification-report.md`：exp00–exp05 复验（含诚实阴性）
- `docs/experiments/2026-09-03-e7-evolve-vs-random.md`、`2026-09-03-e8-power-sdc.md`：E7/E8
- `docs/scheme.md`：sdcfuzz 架构设计（vs SiliFuzz/Harpocrates/Harpocrates++）


## K. 一页结论（供论文微架构根因论证直接取用）

1. 通路上：gem5 O3 的 store→load 转发数据通路上，bitflip / byte_lane_skew（字节通道旋转）/ phaseOffset（相位偏移）三类故障的 SDC 率均为 100%（64/64），装载返回通路是"无掩蔽缓冲"的确定性传播点；而 PRF（X3）同为 100% 但为持续性损坏，与现场一次性瞬态交付（spurious 重走成功、下次读正常）不符，构成排除性对照。
2. 形态上：byte_lane_skew rol1 单注入产生 `xor=3fc52e90a6628000` 多位散布，与现场 XOR 汉明重量 35/36 均匀散布同族；现场四种腐化形态（零塌缩/ROR8/ROL16/ROR16）全部为字节/半字相位撕裂，且穷举证伪位翻转等价性（8 字节 × 256 掩码无命中）。
3. 机制上：一条语义 no-op ALU 使现场触发率 100%→10–20%，gem5 phaseOffset |offset|≥1 → 100% SDC，即发射相位×时序边界竞态；-30mV 欠压（VDDAVS 0.810V）在同一 CPU179 同一装载族复现 ≥3 次，电压裕量为另一轴。两者乘积解释间歇性（间隔 0.000s~343,330s 双态分布）。
4. 静默性上：L1D ECC 对单比特 100% Corrected（n=384）、PTW ECC-on 对单 bit 全纠正（40→0）；现场 RAS 全静默 + 多比特结构撕裂共同指向故障位于 ECC 粒度之下（结构化多比特）或 ECC 校验点之后（fill-buffer 合并/读出组装段）；l1d_disable 四组反例进一步把故障点压向装载返回通路深处。
5. 启示上：规避（offline + 前兆监控有效率 6/7 + 用户态装载陷阱探针）、消减（冗余重算 Fisher p=1.19e-71 完全抑制；ECC 向 ECC 后数据段延伸；转发决策与数据组装分级）、暴露（定向生成对 byte_lane_skew 检出 7.79× 极显著超 SiliFuzz 随机；功耗跳变应力方向性放大）三层均有量化实验支撑，且全部诚实标注了代理边界与未决项（单/多缺陷裁决须 RTL/DFT）。

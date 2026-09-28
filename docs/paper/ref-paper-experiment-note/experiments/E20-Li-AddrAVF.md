# Computing Architectural Vulnerability Factors for Address-Based Structures(实测为 ISCA 2005,Intel FACT 组;任务名 Li DSN'06 勘误)

> 实验复现档案 E20(2026-09-21)
> **身份勘误(必须先行声明)**:任务将本 PDF 标注为 "Li et al., DSN 2006 左右"。对 PDF 全文 12 页逐句精读后确认:
> 1. 作者为 **Arijit Biswas、Paul Racunas、Razvan Cheveresan、Joel Emer、Shubhendu S. Mukherjee、Ram Rangan**(Intel FACT/VSSAD + Sun + Princeton);**没有姓 Li 的作者**(参考文献 [8] 有 S.S. Mukherjee 等,无第一作者 Li);
> 2. 出处页脚每页均为 "Proceedings of the 32nd International Symposium on Computer Architecture (ISCA'05)",即 **ISCA 2005**;
> 3. 这正是任务想要的那篇「Biswas ISCA'05」——地址位 AVF、L1 D-tag/TLB/store buffer 的 hamming-distance-one 分析全部在此文中。
> 本档案按论文实际内容记录,即任务重点「地址位去耦合、full-tag 罕见 ACE、L1 D-tag 数据」的原始出处。
> PDF:`docs/paper/ref/Computing_architectural_vulnerability_factors_for_address-based_structures.pdf`(12 页,全部精读,实验章节 §6–§8 逐句读)。

---

## 1. 研究问题与核心贡献(≤5 行)

- **问题**:Mukherjee MICRO'03 的 ACE 分析只做了指令队列/执行单元;**地址类结构(L1 D-cache、DTLB、store buffer)的 AVF 怎么算?**其难点:数据阵列有 fill/read/write/evict 的复杂生命周期,tag CAM 阵列的故障语义(假命中/假缺失)与数据位完全不同。
- **贡献 1**:**生命周期分析(lifetime analysis)**——把位的寿命切成不重叠组件(idle、fill-to-read、read-to-write、write-to-evict、evict-to-fill……),逐组件判 ACE/un-ACE/unknown(本文把 Mukherjee 的方法工程化为组件表)。
- **贡献 2**:**hamming-distance-one 分析**——tag 阵列新算法:单粒翻转只可能让与 incoming 地址**汉明距为 1** 的 tag 产生假阳性(错误命中);假阴性(应命中而未中)在 write-through cache/DTLB 无害(仅引发 refetch)。由此 tag AVF 极低。
- **贡献 3**:**cooldown 技术**——仿真结束后的"降温期"继续跑 10M 指令以消解 unknown 状态(warmup 的对偶)。
- **贡献 4**:两个 AVF 削减技术(周期性 flush、增量 scrubbing),把 ACE 时间转化为 un-ACE,性能损失 <1.25%。

## 2. 实验方法(ACE 分析方法、模拟器、故障注入验证)

### 2.1 生命周期分析(数据阵列,§4)

- 位的寿命划分为非重叠组件:idle、fill-to-read、read-to-write、write-to-write、write-to-read、read-to-evict、write-to-evict、evict-to-fill、fill-to-evict、read-to-read、以及到仿真结束的 any-to-end。
- **判定规则**(Table 1,复现核心):
  - **恒 un-ACE**:idle、read-to-write、write-to-write、evict-to-fill、fill-to-write(wt-cache)。
  - **恒 ACE(基本)**:fill-to-read、read-to-read、write-to-read——但若读取本身是死读(wrong-path/谓词假/动态死),则转为 un-ACE(原文:"Dynamically dead reads or writes convert ACE into un-ACE components")。
  - **结构特例**:
    - **Write-through D-cache**:fill-to-evict、read-to-evict 为 un-ACE。
    - **Write-back D-cache**:行内任一字节被写 → **同组其余未写字节从 fill 到 evict 全程 ACE**(回写会把它们带出去);fill-to-evict/read-to-evict 条件性转 ACE;已写字节 = write-to-evict + 之前的 write-to-read 为 ACE。
    - **DTLB**:仅 fill-to-read、read-to-read 为 ACE;tag/数据只读、低周转。
    - **Store buffer**:fill-to-read、fill-to-evict、read-to-read、read-to-evict 及到末尾全部 ACE(持有唯一有效副本);仅 idle 与 evict-to-fill 为 un-ACE;**新 store 覆盖同地址字节时,旧条目对应位即刻转 un-ACE**(跨条目 ACE 状态变更,单处理器系统)。
  - **Unknown**:fill-to-end、read-to-end、write-to-end(仿真截断)。
- **粒度**:cache 数据阵列**按字节**维护;DTLB 按表项;store buffer 数据按字节(§4.4)。粒度影响巨大:cache 块 32–128B,fill-to-evict(取进来只被初始访问或从未访问的字节)占 write-through cache 总 un-ACE 时间约 **45%**。
- **DUE AVF 推导**(§4.6):DUE AVF = SDC AVF + false-DUE AVF。false-DUE 来自带奇偶保护时**动态死 load 的误报**;write-through cache/DTLB 可 refetch → DUE AVF 可降为 0。

### 2.2 hamming-distance-one 分析(tag 阵列,§5)——任务重点「地址位去耦合」

- **故障语义解耦**(Table 2):

| 情景 | 应命中→实际未中(false negative) | 应未中→实际命中(false positive) |
|---|---|---|
| Write-through cache | 无害(引发 miss+refetch) | **有害**(取错数据) |
| DTLB | 无害(走 page table) | **有害**(错物理地址/保护位) |
| Store buffer | **有害**(漏合并/排序错乱) | **有害** |
| Write-back cache | 有害(evict 时用错 tag 写错位置) | 有害 |

- **核心推导**:单粒翻转的假阳性只可能发生在与 incoming 地址**恰好差一位**的 tag 上 → **tag 的 ACE 分析必须按位做**(数据阵列按字节/表项):CAM 比较时,把"与 incoming 汉明距 1"的那**一个位**标记为 potentially-ACE,同条目其余位保持 un-ACE。假阴性则整条 tag 的位按"该结构假 miss 是否有害"统一标 ACE/un-ACE。
- **为何 full-tag ACE 罕见**:一个 tag 位要成为 ACE,必须恰有一次访问的地址与该 tag 内容差在这一位上——地址空间中汉明距为 1 的碰撞概率极低;且 4-way 组相联要求同组 4 个成员中出现这种碰撞(故 write-through tag AVF 低至 0.41%)。DTLB tag 稍高(3%)因为全相联、比较对象多。
- **保守性声明**:假阴性导致 tag 阵列翻转、硬件换入新条目从而改变执行流——这一效应**未建模**;作者用商用级 RTL 模型上的**有限统计故障注入实验**(DTLB + 微基准)证明该效应可忽略(§5.2,这是本文唯一的故障注入,用于验证近似而非产出 AVF)。

### 2.3 cooldown(§4.5)

- warmup 的对偶:统计采集结束后继续仿真 10M 指令,期间只跟踪判定 write-to-end/read-to-end 等 unknown 组件 ACE 与否的事件;仍无法判定者记 unknown。
- 效果:除 DTLB tag 外,所有结构 unknown 分量降 **>50%**;unknown → un-ACE 解析比:cache/DTLB tag **>60:1**,各数据阵列 **>10:1**;cooldown 期间平均 SDC AVF 绝对增量 <0.2%(§7.3)。
- **best estimate AVF** 的定义:不含 unknown 分量的值(§7.1.1)——因为 unknown 几乎必然解析为 un-ACE。

### 2.4 模拟器

- Asim 框架(Itanium2-like IA64,22 级流水线、2 GHz、6 发射),Red Hat Linux 7.2 OS 前端;wrong-path 取指但无正确访存地址(同 E18)。

## 3. 实验配置(模拟器配置:核模型/频率/结构大小;基准;参数)

| 配置项 | 值(原文 §6) |
|---|---|
| 核 | Itanium2-like IA64,22 级流水线,2 GHz,6 发射 |
| L1 D-cache | **16KB,4-way 组相联,32B 行,write-through(基线)**;另测 write-back 变体 |
| L2 / L3 | 512KB 8-way / 4MB |
| DTLB | **128 表项,全相联** |
| Store buffer | **32 表项,每表项最多 16 字节**,带 per-byte mask 位;store retire 后进 coalescing merge buffer |
| 基准 | SPEC CPU2000 全 26 个(Table 3;cc-166 跳过 4,700M,其余同 E18 Table 1) |
| 采样 | 每基准第一个 SimPoint,**10M 指令/点**(含 no-op) |
| 编译器 | Intel electron 7.0 最高优化(SimPoint 用 PinPoints 适配 IA64) |
| cooldown | 10M 指令 |
| scrubbing 实验 | 16KB cache、2 GHz、**40ns scrub 间隔**(AMD Opteron 的最优间隔),仅空闲周期 scrub |
| flush 实验 | 间隔 5M / 1M / 100K 指令,假设 flush 瞬时完成 |
| 灵敏度 | 结构规模减半:8K cache、64-entry DTLB、16-entry STB |

## 4. 实验步骤(可操作流程,编号)

1. 在性能模型上对三个结构各建**位级寿命状态机**:事件(fill/read/write/evict/flush/scrub)驱动组件切换;cache/STB 数据按字节、DTLB 按表项。
2. 按 Table 1 给每类组件赋 ACE/un-ACE/unknown;死读/死写检测复用 Mukherjee 的 40K 分析窗口。
3. **tag 阵列单独做汉明距-1 分析**:每次 CAM 比较时,对每个有效 tag 计算与 incoming 的汉明距;距离为 1 → 该差异位记 potentially-ACE(加累计时间);命中(距离 0)→ 按结构语义决定假阴性是否有害,无害则整条 tag 位记 un-ACE。
4. 实现结构特例:write-back 行 dirty 传播、store buffer 跨条目覆盖、DTLB 只读语义。
5. 跑 26 基准 × 10M 指令,统计各组件累计时间,得 SDC AVF。
6. 挂 cooldown 10M 指令,消解 unknown,输出 best estimate AVF。
7. (可选)加 parity 模型算 false-DUE → DUE AVF。
8. AVF 削减实验:以 5M/1M/100K 间隔周期性 flush(重跑统计);或 40ns 间隔 idle-cycle scrubbing(DUE)。
9. 规模减半灵敏度重跑。
10. 交叉验证:与商用 RTL 上的有限故障注入对照(原文仅对 DTLB 假阴性执行流效应做了一次)。

## 5. 实验数据(关键 AVF 数值表 + 图表号)

### 5.1 主结果:三结构数据/ tag 阵列 AVF(§7.1,Figure 4;best estimate = 不含 unknown)

| 结构 | 数据阵列 best-est SDC AVF | 数据阵列含 unknown | tag 阵列 best-est SDC AVF | tag 含 unknown |
|---|---|---|---|---|
| 16KB 4-way **write-through** L1 D-cache | **6%** | 9% | **0.41%** | 4.3% |
| 16KB 4-way **write-back** L1 D-cache | **25%** | 28% | **25%** | — |
| 128-entry DTLB | **36%** | 38% | **3%** | 16% |
| 32-entry store buffer | **4%** | 4% | **7.7%** | — |

- **L1 D-tag(wt)0.41% vs 数据 6%**:tag 比数据低一个数量级,原因是 full-tag ACE 罕见(汉明距-1 碰撞稀有 + 4-way 组内碰撞 + 每次碰撞只贡献 1 个位)。
- **store buffer 是唯一 tag(7.7%)> 数据(4%)的结构**:tag 从 fill 到 evict 恒 ACE;数据位平均只写 6/16 字节(valid 字节才 ACE)。
- **write-back 25% vs write-through 6%**:dirty 行回写把未写字节的 fill-to-evict 变成 ACE,AVF 升 4 倍——**回写策略是 AVF 的一阶决定因子**。
- **DTLB 数据 36% 为最高**:只读 + 低周转。

### 5.2 write-through cache 生命周期分解(§7.1.2,Figure 5)

- fill-to-evict(取入后只被初始访问或从未访问):**>45%** 的字节如此 → 最大的 un-ACE 源。
- read-to-evict:**>20%**(write-through 下 un-ACE)。
- read-to-write + fill-to-write + write-to-write 合计:略低于 5%。
- (Figure 5 不含死 load 去除与 cooldown。)

### 5.3 DUE AVF(§7.2)

| 结构 | DUE AVF | 构成 |
|---|---|---|
| write-back cache 数据(parity) | 25.5% | SDC 25% + false-DUE 0.5% |
| store buffer 数据(parity) | 4.2% | SDC 4% + false-DUE 0.2% |
| write-through cache / DTLB 数据(parity + refetch) | **0** | 可恢复 |
| tag 阵列(两结构) | = SDC AVF | tag 即使对死 store 也必须正确(evict 写错位置) |

### 5.4 cooldown 效果(§7.3,Figure 6)

- 除 DTLB tag 外全部结构 unknown 降 >50%;tag unknown 解析比 >60:1 un-ACE,数据 >10:1;平均 SDC AVF 绝对增量 <0.2%。

### 5.5 flush 削减(§8.1,Figure 7)

| 间隔 | AVF 削减 | 平均 IPC 损失 | 最大 IPC 损失(cache / DTLB) |
|---|---|---|---|
| 5M 指令 | — | 0.02%(cache) | 0.3% / 0.05% |
| 1M 指令 | — | (原文只给两端) | — |
| 100K 指令 | **每结构 >50%**(wt-tag 除外,本就近零) | 0.19%(cache)、0.56%(DTLB) | 1.25% / 1.77% |

### 5.6 scrubbing(§8.2)

- 40ns 间隔、仅空闲周期:**write-back cache DUE AVF 降 42%**(16KB/2GHz/Opteron 最优间隔)。

### 5.7 规模减半(§8.3,Figure 8)

- 一般趋势:规模减半 → AVF 略升(热点行占比提高);例外:DTLB tag 降(汉明距-1 机会从 128 → 64 个对手减少);STB 数据三点均值被高占用基准的反向变化抵消。
- 最大 IPC 损失:D-cache 7.5%、DTLB 28%、STB 6.3%。

## 6. 实验结论(编号列出)

1. **数据位与地址位的 AVF 必须解耦计算**:数据阵列按字节/表项做生命周期分析,tag 阵列按位做汉明距-1 分析;两者数值差可达一个数量级(wt-cache 6% vs 0.41%)。
2. **full-tag ACE 状态罕见**:假阴性(应命中而 miss)在可 refetch 的结构里无害;假阳性只让汉明距-1 的那一个 tag 位成为 ACE → tag AVF 天然极低(wt-cache 0.41%、DTLB 3%)。
3. **持有唯一有效副本的结构(store buffer、write-back dirty 行)是地址结构的 AVF 高地**:store buffer tag 恒 ACE(fill→evict);write-back 使 cache 数据 AVF 从 6% 升至 25%。
4. **AVF 可用微架构手段主动削减**:周期 flush(100K 间隔)>50% 削减,平均 IPC 损失 <0.6%;scrubbing 削 DUE 42%——本质都是把 ACE 寿命组件转成 un-ACE(提高周转率)。
5. **cooldown 消除仿真截断偏差**:unknown 几乎必然解析为 un-ACE(>10:1 至 >60:1),故 best-estimate(不含 unknown)是更真实的口径。
6. **工作集规模与粒度是一阶敏感项**:DTLB 只用 1 表项则 AVF ≤ 1/128;cache 按行不按字节统计会高估 AVF(45% 的字节在 fill-to-evict 中)。
7. 高周转 = 低 AVF,低周转 + 只读 = 高 AVF(DTLB 数据 36% 为三结构之最)。

## 7. 复现要点(gem5/ARM64 复现最小版本)

### 可直接复用

| 要素 | 说明 |
|---|---|
| Table 1 寿命组件判定表 | ISA 无关,可直接搬进 gem5/自研模拟器的 cache/TLB/STB 状态机 |
| 汉明距-1 分析 | 纯算法:每次访问对有效 tag 做 popcount(incoming ^ tag)==1 检测,标记差异位;**在 gem5 的 Tags/BaseTags 上加钩子即可**;对 ARM64 同样成立(地址位语义不依赖 ISA) |
| cooldown 方法 | 对任何采样式模拟(包括 SDCShield 若做 trace 重放)都适用:统计窗后追加判定窗 |
| flush/scrubbing 干预实验 | gem5 里周期性发 flush 全 cache / 模拟 ECC scrubber 均可实现;ARM64 真机有 DC CISW/CSIW(维护操作)可做实机 flush 实验 |
| 死读判定 | 复用 E18 的 40K 分析窗口 |
| best-estimate 口径 | 复现报告应同时给含/不含 unknown 两个数,沿用本文定义 |

### 需替代/不可行

| 要素 | 替代方案 |
|---|---|
| Asim/IA64 | gem5 ARM64(O3CPU + 经典内存系统);16KB 4-way wt-cache 需自配(write-through 在 gem5 中可配但非默认);DTLB 全相联 128 项用 gem5 TLB 模型;store buffer 在 gem5 中是 LSQ,需对 store 生命周期加插桩 |
| write-through L1 | 真实 ARM64(Kunpeng 920)L1 D$ 是 write-back,与本文 wt 基线不同类;对标应取本文 **write-back 25%** 一列 |
| per-byte mask 的 STB | gem5 LSQ 无 byte-mask 粒度统计,需自行维护 |
| RTL 故障注入对照 | 不可行(无商用 RTL);可用 gem5 的 faultinject 模型(FI 对象)做有限抽样验证汉明距-1 近似 |
| SPEC2000/electron | SPEC2017 + GCC -O2;10M 指令 SimPoint |
| 2 GHz / 40ns Opteron 参数 | 按目标机重定(Kunpeng 920 @2.6GHz;scrub 间隔按内存控制器行为假设) |

**最小复现配方(gem5,约 3–4 周)**:ARM O3CPU,16KB/4-way/32B L1(write-back 与 write-through 各跑一轮)、128 项全相联 DTLB、32×16B STB;在 Cache::handleFill/Access 事件上挂字节级寿命计时;Tag 数组挂汉明距-1 位级计时;8 个 SPEC2017 基准 × 10M 指令 + 10M cooldown;输出 Figure 4 式双柱图 + Figure 5 分解图;再跑 100K 间隔 flush 重放。

### 复现实现细节补充(汉明距-1 分析的代码级展开)

**A. tag 位级 ACE 计时状态机(对每次访存的 CAM 比较挂钩)**:

```
on access(addr_incoming, set/way 或 entry):
    for each valid tag T in 对应组(或全相联全部条目):
        d = popcount(addr_tag_bits(incoming) ^ T.bits)
        if d == 0:      # 命中(假阴性分析:翻转任一位 → 不命中)
            if 结构.假miss有害(STB, WB-cache tag):   # Table 2
                for each bit in T: ACE_time[bit] += Δt_since_last_event
            else:                                  # WT-cache, DTLB:无害,不加 ACE
                pass
        elif d == 1:    # 汉明距-1:翻转那一位 → 假阳性(错误命中)
            ACE_time[diff_bit] += Δt_since_last_event   # 只有这一个位计 ACE
        # d >= 2:单粒翻转不可能产生假阳性 → 不加
```

- 关键:**ACE 只在"事件间隙"上累计**——每个 tag 位维护"上次事件时间戳",事件到来时把间隙时间入账(ACE 或 un-ACE),再刷新时间戳。这等价于 Mukherjee 的驻留周期口径,只是粒度到"单个 tag 位"。
- 复杂度:每次访存对组内 ways(4-way 即 4 次)或全相联 128 项做 XOR+popcount;10M 指令 × ~0.3 访存/指令 × 128 ≈ 4×10⁸ 次 64 位 popcount——C++ 下分钟级,gem5 插桩完全可行。

**B. 字节级寿命状态机(cache 数据阵列)**:

```
per byte state: {IDLE, FILLED}, last_event_cycle, current_component
events: fill(line), read(byte), write(byte), evict(line), flush(line), scrub(line)
component 累加规则按 Table 1;write-through 下 write 同时触发"写穿到 STB"(不计 L1 ACE 变化)
write-back 特例:line.dirty=1 时,同 line 全部字节的 fill-to-evict/read-to-evict 改记 ACE
```

**C. cooldown 的实现**:统计窗结束后不清空状态机,继续跑 10M 指令,但只允许事件把 any-to-end 类组件**结算**成具体组件,不再新增统计窗分量;仿真结束时仍未结算的记 unknown。

**D. 面向 SDCShield 真机的对照实验设计(不依赖 gem5)**:
- **汉明距-1 邻接构造**:分配对齐数组,令相邻两个对象的地址差恰为 2^k(单一位差异);测试循环交替访问两对象。若 CPU 地址/tag 比较逻辑有缺陷,该模式比随机地址访问的检出率高得多——这是把本文 0.41% tag AVF 的"低"反转为测试设计武器:普通负载几乎打不中 tag 逻辑,**必须刻意构造汉明距-1 邻接**。
- **dirty 行驻留放大**:golden 写满 cache 行 → sleep/计算延迟 → 读回校验;对应 write-back 25% vs write-through 6% 的差值来源(唯一有效副本 + 回写传播)。
- **store buffer 压力**:连续 store 同一 16B 窗内不同字节(打 per-byte mask)与跨条目覆盖(打"旧条目转 un-ACE"边界逻辑)。

## 8. 作为对比基线的价值(可测对比轴 + 论文基线数值)

对 SDCShield:本文给出**访存类结构**的 AVF 语言,与 E18(计算类结构)合成完整覆盖矩阵;SDC excitation 实验若涉及 load/store 密集流(如 zstd/isal 压缩、memcpy 类),须与本文数值对照。

| 对比轴 | 论文基线值 | SDCShield 侧应用 |
|---|---|---|
| L1 D-tag AVF(wt) | 0.41%(best-est) | 「翻地址位几乎总是无害」的量化证据——**激发实验不能指望随机翻 tag 位出错**;要么打数据位,要么制造汉明距-1 邻接(地址求和取模、相邻地址矩阵遍历) |
| L1 D 数据 AVF(wb) | 25% | dirty 行驻留是脆弱窗口;SDCShield 可设计"写后长驻留再读回"模式(golden 写→延迟读)拉高暴露 |
| DTLB 数据 36% / tag 3% | — | 页表项/翻译通路的测试价值主要在数据(物理页号/权限位),不在 tag |
| store buffer tag 7.7% > data 4% | — | 例外结构:持有唯一副本;store-重载测试(sve512 栈重装载类)天然打这里 |
| flush 每 100K 指令 → AVF 减半、IPC 损失 0.19% | — | 定量反驳"高周转测试更易检出"的直觉?**反向**印证:低周转+唯一副本才是脆弱态,测试应驻留而非抖动 |
| write-through 6% vs write-back 25% | — | 写策略 4 倍差;真机实验设计需记录目标结构的写策略 |
| fill-to-evict 45% | — | cache 行内未访问字节是天然掩蔽——激发实验应整行消费(full-line read)提高有效暴露密度 |

## 9. 局限与坑

1. **文件名/署名坑(本任务已中招)**:PDF 文件名是描述性的,不含作者;实际是 **Biswas 等 ISCA'05**,不是 "Li DSN'06"。引用时按 ISCA'05 记。任务清单中 E19/E20 两篇的归属互相写反了,本档案已勘误。
2. **只做单粒翻转**;汉明距-1 分析在多位翻转下失效(需汉明距-k 推广)。
3. **假阴性改变执行流的效应未建模**(RTL 有限注入显示可忽略,但仅微基准 + 仅 DTLB)。
4. **wrong-path 无正确访存地址**(同 E18):错误路径读写对寿命的扰动被低估/高估不明。
5. **wt-cache 基线与当代全部实际产品(write-back)不同类**;对标真机要用 wb 列。
6. **10M 指令单 SimPoint + 26 基准均值**:相位敏感性与 E18 同样未评估。
7. store buffer 的合并/排序实现(merge buffer)依赖特定微架构,per-byte mask(平均 6/16 字节有效)是 Itanium2 类实现的数据。
8. flush 实验假设 flush 瞬时完成(真实有代价);scrub 参数绑定 Opteron。
9. 「Li DSN'06」若确有所指(如 Xin Li 等),则与本 PDF 无关——本档案只覆盖手头这份 PDF 的实际内容。

## 附:任务重点问题的原文逐条回答(地址位去耦合)

任务要求本篇讲清三件事,原文证据位置如下,便于复核:

**Q1:address-bit AVF(地址位去耦合)方法是什么?**
- 方法 = (a) 数据阵列按字节/表项的**寿命组件分析**(§4,Table 1)+(b) tag 阵列按**位的汉明距-1 分析**(§5.2)。「去耦合」的实质:数据位与地址位的故障语义不同(数据位错误局部化;tag 位错误经由 CAM 匹配全局化),所以**同一结构的数据阵列与 tag 阵列必须用两套不同的 ACE 规则、两个不同的粒度分别计算**,不能像 IQ/EU 那样对整结构统一算。
- 进一步的语义分层:tag 的假阴性(漏命中)对 WT-cache/DTLB 无害(miss+refetch)、对 STB/WB-cache 有害(evict 写错位置 / 漏合并);假阳性(错命中)对所有结构有害但只贡献 1 个位(Table 2 + §5.1)。

**Q2:为何 full-tag ACE 罕见?**
- 三重稀释(§7.1.3 的机理解释):(1) 一次假阳性只让**一个** tag 位变 ACE,不是整条 tag;(2) 假阳性要求 incoming 地址与某有效 tag **恰好差一位**——随机地址流中该碰撞概率 ~ (位数/地址空间),极低;(3) 组相联还要求碰撞发生在**同组**成员之间(WT-cache 4-way → 又除以组数)。结果:WT-cache tag 0.41%(数据阵列的 1/15)、DTLB tag 3%(数据的 1/12)。
- 反例结构:store buffer tag 7.7% > data 4%——因为 STB 的 tag 从 fill 到 evict **无条件** ACE(持有唯一有效副本,Table 1 最后一行),不享受任何稀释。

**Q3:L1 D-tag 实验数据?**
- 16KB 4-way WT L1 D-cache tag 阵列:**best-estimate SDC AVF = 0.41%**(含 unknown 4.3%)(§7.1.3,Figure 4b);对照组:同 cache 数据阵列 6%、write-back 变体 tag 25%、DTLB tag 3%、STB tag 7.7%。
- 含 unknown 时 tag 升到 4.3% 的原因:DTLB/cache 的 tag 位若从未遇到汉明距-1 匹配,直到 evict 前都停在 unknown;作者论证这些几乎必然解析为 un-ACE(解析比 >60:1),且上下文切换本身就会 flush 结构,故 best-estimate 更真实(§7.1.3)。

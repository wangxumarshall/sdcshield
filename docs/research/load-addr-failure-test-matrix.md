# 数据库机 Load 地址侧失效 — 测试矩阵清单（合并去重版）

> **生成**：2026-11-26 · 来源：外部建议清单 + 本仓核对补充，重名合并只写一次
> **核对口径**：本仓 ARM64 构建 `--quality=-1 --list-tests`（SVE1 核对主机，732 id 注册表）逐一比对。
> §1–§8 全部 id 实证在册、可直接跑；核对 = 注册表比对，**非逐件运行验证**（目标机部署时先现场 `--list-tests` 对表）。
> **零 SVE 依赖**：附录 A 的 159 件全部为 NEON/标量载荷，三层实证——① 附录零 `_sve` 后缀件；
> ② 与全仓 198 个 ARM64_REQUIRES_SVE 门控文件的测试名交集为空；③ 逐件源码层核查零 SVE 特征
> （`arm_sve.h`/`__ARM_FEATURE_SVE`/SVE intrinsic/asm 谓词与 z 寄存器助记符零命中）。
> no-SVE 主机（如鲲鹏 920）全量可跑；其中 45 件与 neon_920 收集包（零门控实证包）重合，
> L2 65 件曾在 no-SVE 920 主机（0102）全核验证 pass。
> **用法**：`./sdcshield -e <id>` 单跑（注意 `-e` 是**前缀匹配**）；整包跑用附录 A 存成文件后 `--test-list-file`。

## 0. 三类现象速查

| 现象 | 失效环节 | 直接取证 | 激发加压 | 判别定位 |
|---|---|---|---|---|
| 读取错误地址 | AGU 算错 / 地址寄存器写入错值 | `rowmajor_gps`（tag 自报来源偏移，直接解出来自哪个地址） | `agu_stress_*`、`rfport_v2`、`robfill*`、`tlbstress` | `rowmajor_k5far`、`movbe_dump_probe_*` |
| 读到上一次读取的地址 | 地址寄存器写丢失 / 操作数捕获旧值 / RF 读端口 stale | `rowmajor_gps`（旧 off32 + 旧 gen 直接暴露） | `lsqfill_v2`、`ldr4/ldr5`、`robfill*`、`rfport_v2` | `movbe_l1d_v_rev*`、`mrn_rmw_*` |
| 读到 0 地址 | 地址寄存器被清零路径污染 / zero idiom 误作用 | `rowmajor_gps`（全 0 特征分类） | 仓库暂无专项（见 §10 新写建议） | `mce_check` 伴查 EDAC |

故障率背景：现网 60 次 / 数万台 / 数月 → 单一用例不能承诺复现，**必须用故障板 vs 正常板差分结果说话**。

## 1. AGU 地址生成轴（4 id 在册）

| id | 轴 |
|---|---|
| `agu_stress_1src` | 单源 load + 旋转 ALU + store/reload/store |
| `agu_stress_2src` | 双源 load 版 |
| `agu_stress_unaligned` | 非对齐旋转偏移访问 |
| `agu_stress_store_only` | 纯 store 侧 |

注：另有 `_sve` 变体 2 件在源码（`tests/cpu/sve/mem/`），本核对机 q-1 列表未见——目标机现场确认。

## 2. neon_rot_ldr_at_top L1 族（41 id 在册）

0102 病机实测最有效的谱系（极简 asm 钉死范式）。按轴分组：

**基件与计算底盘（11）**

| id | 说明 |
|---|---|
| `neon_rot_ldr_at_top` | 族基件 |
| `neon_rot_ldr_at_top_rowmajor` | rowmajor 计算底盘本体 |
| `neon_rot_ldr_at_top_rowmajor_dump` | dump 底盘 |
| `neon_rot_ldr_at_top_rowmajor_fp64_dump` | FP64 dump 底盘（**0102 实测最有效单件**） |
| `neon_rot_ldr_at_top_rowmajor_fp64_dup2` | 同一 (a,b) 算 N 次防 CSE（轮换槽位） |
| `neon_rot_ldr_at_top_rowmajor_fp64_dup3` | 同上三槽 |
| `neon_rot_ldr_at_top_rowmajor_dualcmp` | 双 store 腿互查（哪条腿带错 / 上游）※外部清单写作 `_dualcmp`，实际 id 带 `rowmajor` |
| `neon_rot_ldr_at_top_rowmajor_scalar64` | 同骨架 GP 标量路径（NEON vs GP 对比）※实际 id 带 `rowmajor` |
| `neon_rot_ldr_at_top_str3_checkall` | 存储槽位分诊轴（L1 版 id 不带 rowmajor；L2 版带，见 §3） |
| `neon_rot_ldr_at_top_rowmajor_k3res` | K3 驻留判别格底盘（一次性判别使命已披露退役） |
| `neon_rot_ldr_at_top_rowmajor_gps` | **GPS 标签**：每 64b 字自报来源 (magic\|off32\|gen)，三类现象一次判别 |

**k 系激发轴（10）**

| id | 说明 |
|---|---|
| `neon_rot_ldr_at_top_rowmajor_k4gap` | 4 跨距 ldr 前置 8 连 nop（RF 旁路窗口关闭） |
| `neon_rot_ldr_at_top_rowmajor_k4bgap` | 4 跨距 store 背靠背（ALU→store-data 旁路臂） |
| `neon_rot_ldr_at_top_rowmajor_k5far` | 5 跨距远邻（地址捕获 vs 选路 vs 缓冲身份） |
| `neon_rot_ldr_at_top_rowmajor_k5inter_rand` | 5 跨距随机交错（排除 tag 干扰、单看布局效应） |
| `neon_rot_ldr_at_top_rowmajor_k2pureload` | 纯 load 风暴臂（无 store，判 store 干扰）※0102 教训：全核含病核曾整机复位 |
| `neon_rot_ldr_at_top_rowmajor_k7p_zero` | zero 图案轴 |
| `neon_rot_ldr_at_top_rowmajor_k7p_one` | one 图案轴 |
| `neon_rot_ldr_at_top_rowmajor_k7p_hiham` | hiham 图案轴 ※0102 教训同 k2pureload |
| `neon_rot_ldr_at_top_rowmajor_k7p_rand` | 随机图案轴 |
| `neon_rot_ldr_at_top_dump` | 族 dump 基件 |

**写侧三探针（3）**

| id | 说明 |
|---|---|
| `neon_rot_ldr_at_top_rowmajor_strfwd` | 同址 str→ldr 转发窗 |
| `neon_rot_ldr_at_top_rowmajor_waw2str` | 同址双写 WAW 序 |
| `neon_rot_ldr_at_top_rowmajor_dstr4` | 四连写密度 |

**L1 激发族（17）**

| id | 对应现象/轴 |
|---|---|
| `neon_rot_ldr_at_top_ldr4` | load 密度梯度（4 ldr/元素） |
| `neon_rot_ldr_at_top_ldr5` | load 密度梯度（5 ldr/元素） |
| `neon_rot_ldr_at_top_lsqfill` | 在途 load 深度 ~3→~11（LSQ 饱和） |
| `neon_rot_ldr_at_top_lsqfill_v2` | LSQ v2（**"读到上一次地址"最对口**） |
| `neon_rot_ldr_at_top_rfport` | 纯 RF 端口/旁路压力，无内存流量 |
| `neon_rot_ldr_at_top_rfport_v2` | 6 条独立 2 深 asm vmov 链（RF 读端口污染→错地址） |
| `neon_rot_ldr_at_top_robfill` | ROB/发射窗填充（深乱序窗内地址捕获最脆弱） |
| `neon_rot_ldr_at_top_robfill_np` | 纯 nop 对照（区分 ROB 压力 vs 时间拉长） |
| `neon_rot_ldr_at_top_tlbstress` | 持续 dTLB miss + 页表遍历压力 |
| `neon_rot_ldr_at_top_mla` | MAC 管道 + 累加链签名 |
| `neon_rot_ldr_at_top_sub` | 借位 vs 组合逻辑签名 |
| `neon_rot_ldr_at_top_offdiag` | 地址间隔必要性（源对相邻布局） |
| `neon_rot_ldr_at_top_prefetch` | mod-256 置换访问破坏预取器 |
| `neon_rot_ldr_at_top_priv` | 私有 per-thread scratch（共享缓冲竞争对照） |
| `neon_rot_ldr_at_top_str3` | store 侧三连 |
| `neon_rot_ldr_at_top_str4` | store 侧四连 |
| `neon_rot_ldr_at_top_str4_checkall` | store 侧四连全查 |

## 3. L2 三档版（63 id 在册 + memcpy_l2 2 件）

数据库 buffer pool 场景 128K–512K 窗口可能比 L1 版更贴近真实故障域。GPS 标签为 32 位偏移档族格式
`(magic<<48)|(off32<<16)|gen`（16 位 byte_off 在 L2 窗溢出，弃补码校验位换宽度）。
512k 档热集 2.5MB 跨 L3 域。

以下 21 族前缀各有 `_l2_128k` / `_l2_256k` / `_l2_512k` 三档（k7p 三件合并计）：

```
neon_rot_ldr_at_top_rowmajor           neon_rot_ldr_at_top_rowmajor_dump
neon_rot_ldr_at_top_rowmajor_fp64_dump neon_rot_ldr_at_top_rowmajor_dualcmp
neon_rot_ldr_at_top_rowmajor_fp64_dup2 neon_rot_ldr_at_top_rowmajor_fp64_dup3
neon_rot_ldr_at_top_rowmajor_str3_checkall
neon_rot_ldr_at_top_rowmajor_scalar64  neon_rot_ldr_at_top_rowmajor_gps
neon_rot_ldr_at_top_rowmajor_k4gap     neon_rot_ldr_at_top_rowmajor_k4bgap
neon_rot_ldr_at_top_rowmajor_k5far     neon_rot_ldr_at_top_rowmajor_k5inter_rand
neon_rot_ldr_at_top_rowmajor_k2pureload
neon_rot_ldr_at_top_rowmajor_k7p_zero neon_rot_ldr_at_top_rowmajor_k7p_one neon_rot_ldr_at_top_rowmajor_k7p_hiham
neon_rot_ldr_at_top_rowmajor_k3res
neon_rot_ldr_at_top_rowmajor_strfwd    neon_rot_ldr_at_top_rowmajor_waw2str
neon_rot_ldr_at_top_rowmajor_dstr4
```

另有 `memcpy_l2_cache_size`、`memcpy_l2_cache_size_neon` 两件 L2 缓存尺寸探针。

## 4. movbe 判别矩阵（27 id 在册）

**故障来源判别矩阵**（probe 系，逐一对应一条路径）：

| id | 判别轴 |
|---|---|
| `movbe_dump_probe_a` | 去 store 副作用 |
| `movbe_dump_probe_b` | nop 隔离时序 |
| `movbe_dump_probe_c` | store-to-load forwarding |
| `movbe_dump_probe_d` | 去 cache-line 耦合 |
| `movbe_dump_probe_e` | 跨 NUMA/LLC |
| `movbe_dump_probe_f` | 同缓存行步进 |
| `movbe_dump_probe_g1` / `_g2` | LLC set/way 压力 |
| `movbe_dump_probe_h` | LLC 足迹扫描 |
| `movbe_dump_probe_x` / `_xn` | 外部清单未列的同族补充件 |

**L1D vs L2 与时序间隙**：

| id | 判别轴 |
|---|---|
| `movbe_l1d` | reload 命中 L1D（core-179 SDC 预期件） |
| `movbe_l2` | 先逐出再 reload——若 SDC 是 L1D 路径则消失 |
| `movbe_l1d_v_rev`（基件） | str→ldr 间隙扫描 |
| `movbe_l1d_v_rev1` / `_rev2` / `_rev3` / `_rev4` / `_rev8` / `_rev64` | N nop 间隙梯度，定位 stale 窗口时序 |
| `movbe_l1d_v_add` / `_v_mul` / `_v_nop` | 外部清单未列的同族间隙类型梯度 |

**族基件**：`movbe`、`movbe_dump`、`movbe_dump_golden`、`movbe_str_multiload`。

## 5. mrn_rmw：store→load 转发 vs L1D 数组（14 id 在册）

| id | 判别轴 |
|---|---|
| `mrn_rmw` | 族基件 |
| `mrn_rmw_nop` | nop 基件（无 nop 对照） |
| `mrn_rmw_fwd` | 同址 str→ldr（store-buffer **转发**路径） |
| `mrn_rmw_diff` | 不同址 str tempA→ldr tempB（**L1D 数组**路径，非转发） |
| `mrn_rmw_l1d` | 中间逐出（区分转发 vs L1D 数组） |
| `mrn_rmw_nop1` / `_nop4` / `_nop8` / `_nop16` / `_nop64` | nop 间隙梯度 |
| `mrn_rmw_diff_nop64` | 不同址 + 64 nop |
| `mrn_rmw_diff_64k` / `_diff_1m` | 足迹 64K / 1M |
| `mrn_rmw_dump` | dump 侧 |

## 6. 一致性与缓存行为（4 id 在册）

| id | 轴 |
|---|---|
| `spinlock_stale_read` | 自旋于空闲锁（atomic vs 普通 load 背靠背一致性，**stale-read 直接检测**） |
| `spinlock_stale_read_xchg` | 无条件交换获取路径 |
| `cachebounce_sdc` | 全核缓存行弹跳 + str→ldr 重载探针 |
| `cache_stress_civac` | dc civac 逐出 + str→ldr 重载探针 |

## 7. 缓存驻留边界（4 id 在册）

| id | 轴 |
|---|---|
| `ma_l2evict_plain` | L2 逐出判别格基件 |
| `ma_l2evict_rev` | 反向遍历 |
| `ma_l2evict_2src` | 双源 |
| `ma_src_overl2` | 源超 L2 |

注：同目录另有 `_fp64` 变体在册（ma_ldr_random_fp64 等 10 件），外部清单与本清单均未列入主推，按需现场选用。

## 8. EDAC 对照（1 id 在册）

| id | 轴 |
|---|---|
| `mce_check` | EDAC-backed 真实检测件——**框架平台件**（内建于 `framework/sandstone_tests.cpp`），不走 `--list-tests`、无需列入清单，每次运行自动伴跑；"读到 0"/疑似 ECC 类故障时看它的输出 |

## 9. x86-only（在册但 ARM64 诚实 SKIP，不可作火力）

| id | 说明 |
|---|---|
| `zero_control_vec_evex_jit` / `_vex_jit` / `_sse_jit` | x86 JIT 测试，ARM64 走 `log_skip(..."requires x86_64")`（源码+实跑确认）。外部清单仅借其 zero-idiom 设计思路，非可跑项 |

## 10. 仓库没有的（外部建议新写 5 件，仓库 0 命中）

| 建议名 | 目标现象 | 仓库覆盖现状 | 判定 |
|---|---|---|---|
| `load_addr_capture` | 读取错误地址 | **已被 `rowmajor_gps` 覆盖**——tag 自报来源，读到错地址直接解出来源偏移，检测+归因一体 | 不必新写 |
| `load_stale_address_probe` | 读到上一次地址 | 检测面已被 gps 覆盖（旧 off32+旧 gen 暴露）；**激发面是真增量**：地址寄存器写窗口时序（写丢失→保留旧值），现有测试无专项 | 值得新写 |
| `load_zero_address_probe` | 读到 0 地址 | **真增量**——清零 idiom / move-elimination 污染地址寄存器，现有测试无此轴 | 值得新写 |
| `load_addr_rf_port_stress` | 错误地址 / 旧值 | 半增量——`rfport_v2` 是纯 RF 压力无内存流量，"RF 压力+地址消费+load"组合无人做 | 可选 |
| `load_addr_dependency_chain` | 读取错误地址 | 基本被 `agu_stress` 覆盖（rotating-offset 即地址链），深度梯度仅是参数化 | 不必新写 |

新写约定（若立项）：本仓既有范式 = asm 钉死序列 + objdump 门 + GPS 标签金身 + 预注册判别实验链；动笔前先读备份仓 neon_rot 方法论（105 文档判别链）。

### §10.1 已实装（2026-11-27，三件，BETA）

按外部三段生成提示词落地为 `tests/cpu/misc/db_load_*` 三件（与提示词的工程偏差全部在各文件头注披露）：

| 实装 id | 覆盖 §10 中的建议 | 对口现象 | 状态 |
|---|---|---|---|
| `db_load_agu_pointer_chase` | load_addr_capture + load_addr_dependency_chain 的 AGU 面 | 读错误地址 / 读 0 地址 | BETA，已提交（asm 钉死指针追逐 + and 钳位防 SIGSEGV + 终点/异或双金身） |
| `db_load_stale_data_probe` | load_stale_address_probe 的检测面 | 读到上一次的值 | BETA，已提交（MemDiv 式 str→dc civac+dsb→重读，q2==newv 不变量） |
| `db_load_store_mixed_stress` | （超出 §10 建议）Buffer Pool 组合压力 | 全现象混合 | BETA，已提交（1MiB 页池 + vld2 交错 + 跨 4K 窗 + 恒等直通 + 双金身） |

计数影响：PROD 618 不变（BETA 不进默认集）；q0 731 / q-1 735（各 +3）。验证门全过（build 零新警告、191 线程全核实跑 pass【179 offline】、objdump 门、zstd19 回归 pass）。§10 剩余真增量：load_zero_address_probe（zero-idiom 污染地址寄存器）与 load_addr_rf_port_stress（RF+地址组合压力）仍未实装。

### §10.2 语义修正与地址捕获窗三件（2026-11-27 第二轮，BETA）

用户第二轮澄清后修正判定：故障语义 = load 消费的**地址寄存器**（RF 读端口/旁路捕获）突然读出错值/上一次的值/0——是地址操作数捕获失败（179 store 数据寄存器故障的 load 地址镜像），**不是数据侧**。据此诚实修正：上表三件（agu/stale_data/mixed）对该捕获窗**覆盖弱**（地址分别来自内存间接寻址/固定槽遍历/页表计算，均无"寄存器刚写入→立即被 ldr 消费"的背靠背窗压力）；`db_load_stale_data_probe` 压的是数据侧 load-return。三件保留（各自轴仍有效），直击件为下列三件套：

| 实装 id | 直击模式 | 机制 |
|---|---|---|
| `db_load_addr_capture` | 错值 / 上一次的地址（核心件） | 地址寄存器 add 写入→下一条 ldr 零间隙消费（2 行 asm 钉死，objdump 实证邻接）；自描述槽标签 fail 时读回值自行解出实际访问槽位，三分类一步归因 |
| `db_load_addr_zero_probe` | 读到 0 地址 | A/B 交替差分：B 类 `eor x1,x1,x1` 清零后立即写地址再 ldr——0 值粘住覆盖地址写则 load 访问槽 0 且标签点名；仅 B 类 fail 即零化路径实锤 |
| `db_load_addr_gap_sweep` | 窗宽定位（判别件） | add 与 ldr 间插 nop×N 八档 {0..64}：仅小 N fail=旁路前递病、仅大 N fail=写值未提交、平坦=纯值污染 |

三件金身/校验/归因全 C++（用户"多 C++ 少 asm"指令，已入会话持久记忆）；asm 仅关键窗 2-6 行（neon_fp64 程度）。验证门：build 零警告 + 191 线程全核（--quality=-1）+ objdump 门 + zstd19 回归 pass。计数：PROD 620 不变（含 11-27 memcpy_l2_pointer_chase +2 后基线），q0 736 / q-1 740（本轮 +3）。§10 的 load_addr_capture/load_stale_address_probe/load_zero_address_probe 三建议至此全部实装覆盖。

### §10.3 Eaglebu 轴 + 银行轴增量（2026-11-28，BETA 五件）

不属于 §10 外部建议谱系（来源 = Eaglebu 三段建议系统化 + 用户银行并发负载提取指令"多记录同时并行存入读写"）：`mix_pressure_spectrum` / `neon_regcalc_chain`（计算:访存配比谱 + 零访存纯计算链——vector unit 与 cache/MMU 假设解耦判别的两极端）、`memcpy_l2_pointer_chase_calc`（memcpy 舰队第二件：追逐环逐跳向量消费链，序不变金身）、`db_bank_transfer_cas` / `db_bank_transfer_lse`（银行并发转账守恒判别对：256 账户全核冲突 RMW，持续脏行一致性流量——私有缓冲件不可达路径；四本账对账四分类归因，cas 独占对 / lse ldaddal 两原子路径互为判别臂；60s×191 线程健康基线零误报双臂）。与 §11 工作区搁置件无关。计数：PROD 620 不变；q0 736+5=741 / q-1 740+5=745（0102 2026-11-28 实测对账）。部署编排见 §12.7。

## 11. 工作区未完成件（当前跑不到，防混淆）

1c 波 28 件 L2 版已在源码树但**未注册 meson、未验证、未提交**（str3 / str4 / str4_checkall / ldr4 / ldr5 / lsqfill_v2 / rfport_v2 / robfill / robfill_np / tlbstress 的 L2 三档）——
`--list-tests` 里**没有**它们，与 §3 已注册 L2 版无重叠、不冲突，但部署清单勿混入。

## 12. 部署注意

1. **生产机窗口**：目标机常年跑数据库，压力测试需安排窗口；渐进加压先单核后全核。
2. **0102 教训**：`k2pureload` / `k7p_hiham` 全核含病核曾**两次整机复位**（骤死→死寂→平台复位）；怀疑目标机有病核时该两族谨慎全核。
3. **`-e` 前缀匹配陷阱**：`-e neon_rot_ldr_at_top` 会匹配全族；精确控制用附录 A + `--test-list-file`。
4. **差分协议**：故障板 vs 正常板同参数对比；单一用例不承诺复现。
5. SVE 变体按目标机 spec 门控自动出现/跳过，先 `--quality=-1 --list-tests` 现场对表。
6. **BETA 件验证陷阱（实测 2026-11-27）**：不带 `--quality=-1`（或 `--beta`）跑 BETA 级测试（本清单 §10.1/§10.2 全部 db_load_*）会**静默 SKIP 且控制台显示与真跑完全相同的 `exit: pass`**——唯一判别物是运行时长（空跑 ~0.1s / 真跑 ≥2s）。本机核对机 191 在线核（179 offline），三件 db_load_addr_* 门跑 `--quality=-1` 全核 wall 11.3s 实证真跑；部署 BETA 件一律同口径并核对时长。
7. **0203 编排协议（激发件+检测件并跑，2026-11-28 立）**：背景激发件池 = `fmov_train_gdroop_{ldr_str,ldp_stp}` 对 + `fmov_train_gdroop_{ldr_str,ldp_stp}_intr` 对 + `fmov_train_droop`（core 轨）+ fp64 满载臂——**排除 `k2pureload`/`k7p_hiham`**（0102 两次整机复位史，§12.2）；前台检测件池 = rowmajor 族（含 L2 底盘族）+ db_load 六件 + `memcpy_l2_pointer_chase{,_ro,_calc}` + `mix_pressure_spectrum` + `neon_regcalc_chain` + `db_bank_transfer_{cas,lse}`（§10.3）。阶段：① 病核单核（31 号核跑前台检测件，背景全核激发——0102 locv_b2 满载激发 + rowmajor 300s 检出 179 的先例协议）→ ② 受控核组 → ③ 全核；db_bank 判别对与追逐件属最高电流密度类（跨核脏行乒乓），渐进加压强制。锚定负空间报告结论：火力 = 结构轴 × 近核域，不再扩 L2/L3 驻留轴。**0102 侧诚实更正（2026-11-28 评审实测）**：本批五件的全核门跑在 **179 在线**（nproc=192）下完成——会话台账沿用前日 §12.6 的"179 offline→191 线程"口径为过时前提，提交信息中"191 线程"实为 192 含病核；银行对 60s 全核零复位零误报如实记录，但此为对 k2pureload 教训的边界外推——后续在 0102 复跑 bank 对/追逐件全核**必先 offline 179**。

## 附录 A：全部可跑 id 平铺（基线 159 + 2026-11-28 §10.3 五件 = 164，可直接 `--test-list-file`；db_load 六件不在清单内）

```
db_bank_transfer_cas
db_bank_transfer_lse
memcpy_l2_pointer_chase_calc
mix_pressure_spectrum
neon_regcalc_chain
agu_stress_1src
agu_stress_2src
agu_stress_unaligned
agu_stress_store_only
neon_rot_ldr_at_top
neon_rot_ldr_at_top_dump
neon_rot_ldr_at_top_ldr4
neon_rot_ldr_at_top_ldr5
neon_rot_ldr_at_top_lsqfill
neon_rot_ldr_at_top_lsqfill_v2
neon_rot_ldr_at_top_mla
neon_rot_ldr_at_top_offdiag
neon_rot_ldr_at_top_prefetch
neon_rot_ldr_at_top_priv
neon_rot_ldr_at_top_rfport
neon_rot_ldr_at_top_rfport_v2
neon_rot_ldr_at_top_robfill
neon_rot_ldr_at_top_robfill_np
neon_rot_ldr_at_top_str3
neon_rot_ldr_at_top_str3_checkall
neon_rot_ldr_at_top_str4
neon_rot_ldr_at_top_str4_checkall
neon_rot_ldr_at_top_sub
neon_rot_ldr_at_top_tlbstress
neon_rot_ldr_at_top_rowmajor
neon_rot_ldr_at_top_rowmajor_dump
neon_rot_ldr_at_top_rowmajor_fp64_dump
neon_rot_ldr_at_top_rowmajor_fp64_dup2
neon_rot_ldr_at_top_rowmajor_fp64_dup3
neon_rot_ldr_at_top_rowmajor_dualcmp
neon_rot_ldr_at_top_rowmajor_scalar64
neon_rot_ldr_at_top_rowmajor_k3res
neon_rot_ldr_at_top_rowmajor_gps
neon_rot_ldr_at_top_rowmajor_k2pureload
neon_rot_ldr_at_top_rowmajor_k4gap
neon_rot_ldr_at_top_rowmajor_k4bgap
neon_rot_ldr_at_top_rowmajor_k5far
neon_rot_ldr_at_top_rowmajor_k5inter_rand
neon_rot_ldr_at_top_rowmajor_k7p_zero
neon_rot_ldr_at_top_rowmajor_k7p_one
neon_rot_ldr_at_top_rowmajor_k7p_hiham
neon_rot_ldr_at_top_rowmajor_k7p_rand
neon_rot_ldr_at_top_rowmajor_strfwd
neon_rot_ldr_at_top_rowmajor_waw2str
neon_rot_ldr_at_top_rowmajor_dstr4
neon_rot_ldr_at_top_rowmajor_l2_128k
neon_rot_ldr_at_top_rowmajor_l2_256k
neon_rot_ldr_at_top_rowmajor_l2_512k
neon_rot_ldr_at_top_rowmajor_dump_l2_128k
neon_rot_ldr_at_top_rowmajor_dump_l2_256k
neon_rot_ldr_at_top_rowmajor_dump_l2_512k
neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_128k
neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_256k
neon_rot_ldr_at_top_rowmajor_fp64_dump_l2_512k
neon_rot_ldr_at_top_rowmajor_dualcmp_l2_128k
neon_rot_ldr_at_top_rowmajor_dualcmp_l2_256k
neon_rot_ldr_at_top_rowmajor_dualcmp_l2_512k
neon_rot_ldr_at_top_rowmajor_fp64_dup2_l2_128k
neon_rot_ldr_at_top_rowmajor_fp64_dup2_l2_256k
neon_rot_ldr_at_top_rowmajor_fp64_dup2_l2_512k
neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_128k
neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_256k
neon_rot_ldr_at_top_rowmajor_fp64_dup3_l2_512k
neon_rot_ldr_at_top_rowmajor_str3_checkall_l2_128k
neon_rot_ldr_at_top_rowmajor_str3_checkall_l2_256k
neon_rot_ldr_at_top_rowmajor_str3_checkall_l2_512k
neon_rot_ldr_at_top_rowmajor_scalar64_l2_128k
neon_rot_ldr_at_top_rowmajor_scalar64_l2_256k
neon_rot_ldr_at_top_rowmajor_scalar64_l2_512k
neon_rot_ldr_at_top_rowmajor_gps_l2_128k
neon_rot_ldr_at_top_rowmajor_gps_l2_256k
neon_rot_ldr_at_top_rowmajor_gps_l2_512k
neon_rot_ldr_at_top_rowmajor_k4gap_l2_128k
neon_rot_ldr_at_top_rowmajor_k4gap_l2_256k
neon_rot_ldr_at_top_rowmajor_k4gap_l2_512k
neon_rot_ldr_at_top_rowmajor_k4bgap_l2_128k
neon_rot_ldr_at_top_rowmajor_k4bgap_l2_256k
neon_rot_ldr_at_top_rowmajor_k4bgap_l2_512k
neon_rot_ldr_at_top_rowmajor_k5far_l2_128k
neon_rot_ldr_at_top_rowmajor_k5far_l2_256k
neon_rot_ldr_at_top_rowmajor_k5far_l2_512k
neon_rot_ldr_at_top_rowmajor_k5inter_rand_l2_128k
neon_rot_ldr_at_top_rowmajor_k5inter_rand_l2_256k
neon_rot_ldr_at_top_rowmajor_k5inter_rand_l2_512k
neon_rot_ldr_at_top_rowmajor_k2pureload_l2_128k
neon_rot_ldr_at_top_rowmajor_k2pureload_l2_256k
neon_rot_ldr_at_top_rowmajor_k2pureload_l2_512k
neon_rot_ldr_at_top_rowmajor_k7p_zero_l2_128k
neon_rot_ldr_at_top_rowmajor_k7p_zero_l2_256k
neon_rot_ldr_at_top_rowmajor_k7p_zero_l2_512k
neon_rot_ldr_at_top_rowmajor_k7p_one_l2_128k
neon_rot_ldr_at_top_rowmajor_k7p_one_l2_256k
neon_rot_ldr_at_top_rowmajor_k7p_one_l2_512k
neon_rot_ldr_at_top_rowmajor_k7p_hiham_l2_128k
neon_rot_ldr_at_top_rowmajor_k7p_hiham_l2_256k
neon_rot_ldr_at_top_rowmajor_k7p_hiham_l2_512k
neon_rot_ldr_at_top_rowmajor_k3res_l2_128k
neon_rot_ldr_at_top_rowmajor_k3res_l2_256k
neon_rot_ldr_at_top_rowmajor_k3res_l2_512k
neon_rot_ldr_at_top_rowmajor_strfwd_l2_128k
neon_rot_ldr_at_top_rowmajor_strfwd_l2_256k
neon_rot_ldr_at_top_rowmajor_strfwd_l2_512k
neon_rot_ldr_at_top_rowmajor_waw2str_l2_128k
neon_rot_ldr_at_top_rowmajor_waw2str_l2_256k
neon_rot_ldr_at_top_rowmajor_waw2str_l2_512k
neon_rot_ldr_at_top_rowmajor_dstr4_l2_128k
neon_rot_ldr_at_top_rowmajor_dstr4_l2_256k
neon_rot_ldr_at_top_rowmajor_dstr4_l2_512k
memcpy_l2_cache_size
memcpy_l2_cache_size_neon
movbe
movbe_dump
movbe_dump_golden
movbe_str_multiload
movbe_dump_probe_a
movbe_dump_probe_b
movbe_dump_probe_c
movbe_dump_probe_d
movbe_dump_probe_e
movbe_dump_probe_f
movbe_dump_probe_g1
movbe_dump_probe_g2
movbe_dump_probe_h
movbe_dump_probe_x
movbe_dump_probe_xn
movbe_l1d
movbe_l2
movbe_l1d_v_rev
movbe_l1d_v_rev1
movbe_l1d_v_rev2
movbe_l1d_v_rev3
movbe_l1d_v_rev4
movbe_l1d_v_rev8
movbe_l1d_v_rev64
movbe_l1d_v_add
movbe_l1d_v_mul
movbe_l1d_v_nop
mrn_rmw
mrn_rmw_nop
mrn_rmw_fwd
mrn_rmw_diff
mrn_rmw_l1d
mrn_rmw_dump
mrn_rmw_nop1
mrn_rmw_nop4
mrn_rmw_nop8
mrn_rmw_nop16
mrn_rmw_nop64
mrn_rmw_diff_nop64
mrn_rmw_diff_64k
mrn_rmw_diff_1m
spinlock_stale_read
spinlock_stale_read_xchg
cachebounce_sdc
cache_stress_civac
ma_l2evict_plain
ma_l2evict_rev
ma_l2evict_2src
ma_src_overl2
```

（§9 的 3 件 x86-only SKIP 件与 §8 的框架平台件 mce_check 不在附录 A 内——前者 ARM64 部署无意义，后者自动伴跑无需选择。附录 A 全部 164 id 均在注册表实证在册 = 159 基线 + §10.3 五件。）

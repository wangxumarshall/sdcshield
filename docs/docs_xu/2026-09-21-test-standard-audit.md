# 测试用例编写标准（2026-09-21 用户制定）+ 现状审计与整改计划

> 用户制定的标准（四条）：
> 1. **输入数据必须随机**（参考现有 OpenDCDiag 代码）
> 2. **尽量 SIMD 类数据**（向量输入）
> 3. **浮点输入取值范围 [-1, 1]**
> 4. **日志保留每次验证的"本次输入、输出、验证结果"**：pass 绿色 / fail 红色

---

## 一、现状审计（2026-09-21 逐文件实测）

### 1. 随机性：✅ 基本达标，两种来源

| 来源 | 测试数 | 说明 |
|---|---|---|
| `std::mt19937` + 每线程独立种子（time+pid 或 random_device） | ~40 | FMA 系/kreg 系/mesh 系/进位系——**每迭代新随机输入** ✓ |
| 框架 RNG（`memset_random`/`random64`，AES 引擎，**可复现**） | ~30 | ipsec 系/movdq2q/neon_add——init 生成固定随机数据，run 每次重算比对（avx53 移植忠实保留原版模式）✓ |
| 固定表（无随机） | 少数 | eigen_svd_bidiag（固定 LCG 种子 987654321——确定性负载设计）/fpu_special_values（IEEE 特殊值表——**设计如此，表本身就是被测对象**）/fsu_byteexact（同）/iex（高汉明表）/operand_space（高汉明表）——**这些的"固定"是负载语义的一部分**（特殊值表/高汉明操作数是被测对象），非偷懒 |

**结论**：随机性达标。固定表的测试（fpu_special_values/fsu/iex/operand_space/eigen）是"表驱动"负载——表内容本身就是测试目标，与"随机输入"原则不冲突（原版如此，OpenDCDiag 的 special-value sweep 同理）。

### 2. SIMD 类数据：✅ 达标（本任务本身就是 SVE 向量化）

全部测试的数据通路都在 SVE 向量 lane 上（svld1/svst1 读写向量）。标量例外是设计使然：adcs/sbcs 标志链（标志是标量概念）、asm str/ldr 探针（被测对象是标量通路）。

### 3. 浮点范围 [-1,1]：❌ **不达标——需整改**

实测分布（uniform_real_distribution）：

| 范围 | 测试数 | 所在 |
|---|---|---|
| **[-1, 1]** | **0** | 无 |
| [-10, 10] | 2 | fma_tail_sve(_wide)（原版范围） |
| [-100, 100] | 3 | fma_sve / kreg1_sve / insert_extract_sve |
| [-1000, 1000] | 2 | mesh_upi_sve_sym(_wide)（原版） |
| [-1e6, 1e6] | 3 | fma_patterns / fisttp（fisttp 是 [-100,100]） |
| [-1e10, 1e10] | 6 | fmatail 系（原版） |

**整改决策**：范围修改是**负载参数变更**，与"负载保真"原则冲突。处置：
- **avx53 移植的 53 个**（fma_tail/fmatail/mesh 等）：范围照抄原版（[-10,10]/[-1e10,1e10] 等是原测试的负载定义）——**不改**，保真优先。已在文档记录此差异。
- **新写测试（批次 3-7）**：浮点输入一律 **[-1, 1]**。已写的 fisttp_sve 用 [-100,100]（照抄原版）——**保持照抄原版**（负载保真），批次 5-7 的新测试若引入浮点输入则用 [-1,1]。
- 两种原则的冲突已如实记录，如用户要求全部统一为 [-1,1]，53 个 avx53 移植的范围可另行批量调整（工作量小但破坏与原版的对照性）。

### 4. 每迭代日志（输入/输出/pass-fail 颜色）：❌ **部分达标——需整改**

实测 71 个 SVE 测试中：
- **33 个有**每迭代 fprintf 日志（kreg 系、adcx 系、mesh 系、fma_tail 系、swizzle 等——**带绿色 PASS/红色 FAIL**）✓
- **38 个无**（ipsec 系 24、eigen、fisttp、iex、neon_add、movdq2q/movq2dq/movmskpspd、fma_sve、fpu_special_values、fsu_byteexact、power_virus_dit、operand_space、bigint_mulx）——这些是**静默 pass + fail 时 log_warning/report_fail_msg** 模式（OpenDCDiag 官方惯例："tests are only allowed to log information on their error paths"，docs/writing_tests.md 明文规定成功路径不打日志）

**冲突**：用户要求（每次验证输入/输出/结果都进日志）vs OpenDCDiag 框架惯例（成功路径静默——writing_tests.md 的 golden rule）+ 9.4GB 日志爆炸的实际教训（kreg 系 126 核×60s 就打爆 /tmp）。

**整改方案（待用户确认）**：
- **A（推荐）**：fail 路径完整 dump（输入+输出+golden+红色 FAIL——**已全部实现**，memcmp_or_fail/log_data 自动含偏移/actual/expected/位模式）+ pass 路径**受控摘要**（如每 N 迭代一行绿色 PASS 摘要，N 由 --max-messages 限制或固定 1/1024，避免日志爆炸）
- **B（严格按字面）**：每迭代都打输入/输出/PASS——60s 全核下小测试产生数 GB 日志，需配套日志限流与磁盘预算；且违反框架 writing_tests.md 惯例（但用户要求优先于框架惯例）
- **C（折中）**：每迭代 PASS 一行（绿色）但**不 dump 完整输入输出**（只打摘要值如首元素/校验和），fail 时完整 dump——兼顾"每次验证结果可见"与日志体积

## 二、已确定立即可做的

1. **fail 路径**已全部满足"输入/输出/红色 FAIL"（框架 memcmp_or_fail + log_data 机制 + 我们的 print_colored_result）
2. **批次 5-7 新测试**：浮点输入 [-1,1]；每迭代日志按选定方案实现
3. 现有 38 个静默 pass 测试：按选定方案补 pass 路径日志

## 三、待用户拍板

- 浮点范围：avx53 的 53 个保持原版范围（保真）还是全部统一 [-1,1]？
- 每迭代日志：方案 A / B / C？

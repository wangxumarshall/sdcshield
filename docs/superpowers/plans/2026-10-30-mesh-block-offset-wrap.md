# 2026-10-30 mesh 族 uint32 块偏移回绕修复（假 SDC 根因）

## 背景

2026-10-30 07:21 L4 深驻留 `mesh_upi_sse_sym`（169 GB 缓冲，64 线程）报
"Sum mismatch or consistency failure"，框架同 seed 内部重试 3 次全部失败
（4/4，全部 64 线程），事件流水线记为 SDC 候选。战役外部 3 次定向复跑
"通过"实为无效：重试命令带 `-n 0`，框架拒绝后按 1 线程运行，用例
`requires at least 2 threads` 直接 SKIP（retest1.out 实证），未验证任何东西。

## 根因（已实证）

`tests/cpu/mesh/mesh_upi_sse_sym.cpp:125-127`：

```c
uint32_t block = td->next_block.fetch_add(1, ...);
size_t offset = block * BLOCK_ELEMENTS;   // uint32_t × int → 32 位乘法
```

C++ 常用算术转换把乘法提升为 `unsigned int`，结果 **mod 2^32 回绕**后再
赋给 `size_t`。`BLOCK_ELEMENTS=8192` → 每 524288 块（16 GiB 元素区）回绕
一次，块 ≥ 524288 的写落到数组低地址，覆盖已写块；读相按
`total_elements` 全量求和 → `read_sum != expected_sum`。

**决定性实验**（同一预修复二进制 `/tmp/sdcshield-prefix`，-n 8）：

| 缓冲 | 块数 | 结果 |
|---|---|---|
| 16 GiB | 524288（不触发） | PASS，read_sum == expected_sum |
| 16 GiB + 32 KiB | 524289（1 块回绕） | FAIL，read_sum=-67833339，expected=-67836732，差值 3393 |

差值恰为被回绕覆盖块的浮点和。169 GB 默认缓冲 = 550 万块，每 16 GiB
回绕一次 → 只要相位 2 跑完（dwell 场景耗时 ~117 s/轮）即确定性 FAIL。
`-t 60s` 的全量轮转因相位 2 未开始而"空过"（另一独立缺陷，不在本单）。

## 受影响文件（运行时块数，可超 524288）

仅 4 个 mesh 用例的 `num_blocks` 由物理内存运行时计算且块索引为
`uint32_t`；其余 mesh 用例为固定 `NUM_BLOCKS ≤ 256`，无回绕风险：

1. `tests/cpu/mesh/mesh_upi_sse_sym.cpp:127`（BLOCK_ELEMENTS=8192）
2. `tests/cpu/mesh/mesh_upi_avx_sym.cpp:128`（BLOCK_ELEMENTS=8192）
3. `tests/cpu/mesh/mesh_upi_avx512_sym.cpp:105`（BLOCK_SIZE=8192）
4. `tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp:161`（BLOCK_SIZE=4096，
   回绕阈值 1048576 块 = 32 GiB，更早触发）

## 改动（4 文件，一个工作单元）

偏移乘法显式 64 位化：`size_t offset = (size_t)block * BLOCK_ELEMENTS;`
（f64 为 `BLOCK_SIZE`）。块索引原子量保持 uint32（num_blocks 上限
0xFFFFFFFF，读取侧 `num_blocks` 为 size_t，无其他 32 位乘法索引）。

## 验证（rule 2）

1. `./scripts/build.sh` 零新警告（唯一 warning 为既有
   `_XOPEN_SOURCE redefined`）。
2. 边界对照（-n 8）：预修复二进制 16 GiB+32 KiB → FAIL；
   新二进制同参数 → PASS。
3. f64 边界（-n 8，BLOCK_SIZE=4096）：32 GiB+32 KiB（1048577 块），
   预修复二进制 → FAIL，新二进制 → PASS。
4. 大缓冲回归：32 GiB 缓冲跑满相位 2 → PASS。
5. 标准回归：`-e zstd19 -t 2000 -n 1` → `exit: pass`。
6. x86-64 零触碰：4 处改动均位于 `#ifdef __aarch64__` 分支内
   （各文件 `#else` 起始行 260/259/254/309 之前）。

## 边界与后续（不在本单）

- 用例在 `-t` 到期于相位 1 时"空过"（未执行任何校验即 PASS）——独立缺陷。
- 169 GB 缓冲 × 2 个 test plan 并发 → 事件 20261030-092537 SIGKILL
  （code 9，OOM）——内存预算独立缺陷，另行处置。

## 检视轮修订（2026-10-31，local-claude 两轮 12 条意见处置）

本节修订正文的四处事实性偏差，原文保留于上（记录即历史，修订不抹除）：

1. **「受影响文件」范围（正文 §受影响文件）**：「仅 4 个 mesh 用例的
   num_blocks 由物理内存运行时计算」只对 `tests/cpu/mesh/`（NEON）成立。
   `tests/cpu/sve/mesh/` 实测 **13** 个文件（progress-log 原记 12，少数 1）
   同款运行时块数 + `size_t offset = block * BLOCK_ELEMENTS` 32 位回绕：
   sym / symm_int / asymm_int / asymm_write_int / asymm_distrib_int /
   asymm_distrib_write_int / wide_sym / wide_symm_int / wide_asymm_int /
   wide_asymm_write_int / wide_asymm_write_only_int / wide_asymm_distrib_int /
   wide_asymm_distrib_write_int。该 13 文件**已由并行 PR #308 修复**（commit
   30bcd256 同款乘法修复 + b3d77acb 块索引空间 64 位化，覆盖含本单 4 文件的
   全部 17 个），本单不重复修——两个 PR 修同一缺陷类、4 文件重叠，合并顺序
   人工决策（#308 先合并则本单 4 文件修复大部分成重复；反之亦然）。
2. **根因叙述的适用面（正文 §根因）**：「读相按 total_elements 全量求和 →
   read_sum != expected_sum」对 sse_sym/avx_sym（f32 求和）与
   avx512_symm_f64（f64 双侧精确和）成立；对 **avx512_sym 不成立**——该
   文件读相位只做 store_buf 自一致性比较，不对照写入值（文件 135-137 行
   注释自认），`local_sum` 累加后从未使用（死代码），`passed = consistent`。
   即：32 位回绕在 avx512_sym 写相位回读窗口外**不产生任何可观测 FAIL**，
   修复恢复的是全缓冲写入覆盖（上半段曾被永久跳写），不能引用 sum 边界
   实验佐证该文件。
3. **验证边界（正文 §验证 项 2/3/4）**：决定性实验表只含 sse_sym 的
   read_sum/expected 数字；avx512_sym 无读相内容校验，边界实验（16 GiB
   ±32 KiB）对该文件不适用。该文件的修复正确性由同型算术单行改动 + 全量
   回归背书；其读相校验能力缺口（自拷贝零检出）与 sse_sym/avx_sym 的
   聚合和盲区、写侧逐 lane 比对丢弃一并由 **issue #332** 立项跟踪（先例：
   PR #308 二轮意见 #9/#10 同主题变通）。
4. **战役复测链路（正文 §背景 / §边界第 1 条）**：本分支 12d5c7a5 只修了
   复测的线程维度（-n 0 → 沿用原命令）；时长维度（硬编码 -t 120s 对 169GB
   mesh 整轮 ~117s，相位 2 跑不完即沿 EXIT_SUCCESS 空过 → "retest rc=0
   fail/crash=0" 假台账）与拓扑/旋钮维度（--cpuset/-O 丢弃）在本检视轮
   补齐（四维条件保真 + -t 预算沿用/900s 封顶 + 台账 skip/t/elapsed 诚实
   标注，见 sdc_common.sh `extract_retest_flags`/`retest_t_budget` 与
   驱动 handle_failure）。用例侧「-t 到期空过」（相位 1 超时 break →
   EXIT_SUCCESS 零校验 PASS，违反 CLAUDE.md 规则 6 精神）仍是独立缺陷，
   与 #332 分立跟踪。

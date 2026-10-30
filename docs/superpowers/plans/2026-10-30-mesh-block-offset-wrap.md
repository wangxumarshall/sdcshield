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

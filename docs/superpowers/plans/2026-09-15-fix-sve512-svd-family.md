# 修复 sve512 SVD-format 家族四个测试(目标机单核崩溃)

日期:2026-09-15

## 背景

用户在 SVE 目标机(HiSilicon 0xd22)单核运行,SVD 家族把程序跑崩。
单核崩溃与线程数无关 → 锁定集成前预警的 init 越界 bug:

- **#278 sve512_gather_scatter_svd init 尾块越界(必崩)**:300 不是 16
  的倍数,br/bc=288 的尾块按完整 16×16(256 元素)置换,row/col 最大
  303,`gather_idx[303*300+303]` 越 90000 元素 vector 约 1.2k 元素,
  init 即堆越界读写(OOM 不可区分的 SEGV/abort)。
- **三个 chain/special SVD 分配失控**:流大小 = 300*300*STEPS*VL,
  f64/f32_chain_svd 各 ~5.9 GiB、special_svd ~737 MB,与 docblock 宣称
  的 "1.44 MB" 矛盾 2000×;OOM 风险 + init 填充耗时数秒。
- **gather_scatter_svd 竞态**:scatter_dst/scatter_golden 跨线程共享写
  + 每线程 std::fill(0),满核假 FAIL(单核不触发,但同修)。

## 修复方案(2 commit)

### Task 1: sve512_gather_scatter_svd — OOB + 竞态
- 置换循环 k 上界 clamp 到 `min(16,300-br) × min(16,300-bc)`,
  行列索引用实际块宽 cols_in 计算
- golden 移到 init 一次性预计算(run 中只读)
- scatter_dst 改 run 内每线程局部 vector(索引表是满置换,每轮全覆盖
  写,无需 fill(0))
- 热循环谓词改 svwhilelt(90000 不保证被任意 VL 整除,防尾向量越界)
- 验证:主机侧边界模拟(纯整数,本机实跑)+ 编译/零告警/干净 skip/
  zstd19/反汇编

### Task 2: 三个 chain/special SVD — block-major 有界分配
- 布局从 [row][col][step][lane] 巨跨度(步距 5.76 MB、总量 2.95 GB)
  改为块主序 [block_row][block_col][step][lane]:每块原点拥有
  STEPS×VL 连续元素,总量 = 19×19×STEPS×VL
- 新分配:f64_chain_svd / f32_chain_svd 各 2×11.8 MB(总 23.7 MB,
  L2 的 46 倍),special_svd 各 1.44 MB(总 2.9 MB,恰与 docblock
  的 1.44 MB 量级吻合)
- 保留全部结构要素:16×16 块遍历、512/64 步串行 FMLA 链、VL 全宽、
  byte-exact golden(公式 golden/hw 同步改,逐 op 一致)
- docblock/meson 注释同步为真实数字;验证:主机侧边界模拟 + 常规链

主机侧模拟覆盖:四测试全部索引公式的 max_index < total 断言 +
置换后 idx 仍为满置换的断言。

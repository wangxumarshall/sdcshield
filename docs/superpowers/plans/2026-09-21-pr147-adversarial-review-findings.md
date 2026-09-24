# PR #147 对抗性审查结论（2026-09-21，实证核查）

> 本文档记录对 PR #147（SDC 随机性加固第二批，10 个新 commit，128 文件 +1137/−887，
> 在 PR #146 的 11 个 commit 之上）的对抗性审查结果。PR #146 部分的审查结论见
> `2026-09-20-pr146-adversarial-review-findings.md`（F1-F8 编号沿用）。
> 所有结论均来自真实命令输出（构建、运行、故障注入、LSP 诊断），非推测。

## 审查方法

1. `git fetch origin refs/pull/147/head:pr-147`，逐 commit 读 diff + 完整文件
2. 全部 127 个触碰源文件中 124 个进 compile_commands（2 个 unaligned 本就不构建，
   1 个是头文件经 10 个 SVD TU 覆盖）；25+ 文件过 clangd 17.0.6 `--check`，
   逐个区分真实诊断与 tweak 自测噪音
3. 构建（`-Dssl_link_type=static` + ACL vendored）+ 全家族实跑
4. **故障注入实验**（gmp_bignum 对称/非对称，机制证明）
5. **运行时行为实测**：stderr 捕获、mesh 家族屏障协议追踪、`-f no` 直连 stderr

## 实证复现的 PR 声明（全部通过）

- ninja 零新告警；`--list-tests` 329（README 声称 328 — 见 F7'）
- crc 13 测试（含 `crc32_fixed_shuffled` OOB 修复）、gmp 2、atomic 3、eigen SVD 6、
  gemm 4、sparse、mesh 27（-n 2）、lock/spinlock 全家族（-n 4）、vector/gather/memcpy 24、
  fma 12、sve512 8、arm64 5 — 全部 `exit: pass`
- fma stderr flood 修复真实：`-v` 下 fma_patterns_avx512_pd 从 1,049,890 字节降到
  20,948 字节，0 个 stderr 块（PR #146 审查的 F3/F6 已在此 PR 修复）
- `__crc32b` 多项式声明独立复核：hw 链 = sw 0xEDB88320 = 0xCBF43926（IEEE 802.3，
  非 CRC32C）✓；isa-l `_base()` 真为独立路径（`crc_multibinary_arm.o` PMULL vs
  `crc_base.o` 查表）✓；kreg4 SIGTRAP 修复的 32 位回绕数学正确 ✓
- fma 家族 stderr 已正确 fail-gated（`if (!passed)` 内）✓
- atomic_simd_256 的 run-constant 设计合理（多写者 seqlock 无写者互斥，per-iteration
  随机会在真实 all-core 运行中撕裂误报 — commit 记录了失败的首设计，符合事实）

## 确认的缺陷（按严重度排序）

### 【F1'｜高｜纯同义反复】gmp_bignum + gmp_bigadd：golden 与 DUT 是同一函数调用两次

`gmp_bignum.cpp` run 循环：`mpz_mul(golden, a, b)` 后紧跟 `mpz_mul(result, a, b)`
（同一操作数、同一线程、同一迭代）；`gmp_bigadd.cpp` 同构（`mpz_add` ×2）。
**这是 PR #147 自己在 019d82f（crc commit）里定义为 "audit D11" 并修复的缺陷类别** —
"hw-vs-hw duplicate computes: a deterministic CRC-unit defect produces identical wrong
values and passes forever" — 但 951e994（gmp commit）在两个 GMP 测试里新造了完全相同的
模式。

**实证**（gmp_bignum 故障注入）：
- 对 golden 和 result **对称**注入相同的确定性错误（模拟持续坏掉的乘法单元）→
  `exit: pass`（缺陷不可见）
- 只对 result **非对称**注入 → `exit: fail`（瞬态检出正常）

与 PR #146 审查的 F2（bigint_mulx_arm 同一 `mpz_mul` 调用两次）完全同构 —
三个 GMP 测试（bigint_mulx_arm / gmp_bignum / gmp_bigadd）需要同样的独立 golden 修复。
adcx.cpp 的模式（`__int128` 标量 golden vs ADCS 汇编 DUT）是正确参照。

### 【F2'｜高｜commit 声明与代码不符】c805065 声称清除 28 个 lock 测试的 stderr flood，实际只清了 8 个

commit message: "the per-iteration stderr spam is removed (0 flood lines in a 3s
spinlock_stress run, was 1+/iteration/thread)"。实测（逐文件比对 pr-146 与 pr-147 的
in-loop ungated fprintf）：26 个触碰文件中 **18 个的每迭代 PASS flood 原样保留**
（locks.cpp、lockgen、lockless_cmpxchg{8b,16b}、locks_ccb{,_xch_only}、locks_xch_only、
spinlock_crosses_cacheline、spinlock_rmw_{cmpxchg,xchg}{,_pause}、spinlock_same_core、
spinlock_stress_{bts,cmpxchg,cmpxchg16b}、spinlock_unaligned、spinlock_with_hle）。
只有 8 个真正清除（spinlock_stress、lock_instr、lockless_cmpxchg、spinlock_array、
spinlock_bank、spinlock_cross_socket、spinlock_mini、spinlock_randomsweep）。

**实测规模**：`locks -t 3000 -n 4 -v` → 10,080 条 "Thread PASS" 行 / 742KB YAML。
commit 里那句 "0 flood lines" 只对 spinlock_stress 一个文件为真 —
以单文件证据支撑 28 文件声明，属于不实验证声明（CLAUDE.md 诚实条款问题）。

### 【F3'｜高｜既有缺陷曝光】mesh 6 文件屏障协议死锁 → 空转 pass（vacuous pass）

`mesh_upi_{avx2,sse}_{symm,asymm}_int.cpp` + `mesh_upi_{avx,sse}_sym.cpp`（6 文件）的
phase-1 屏障：`while ((read_done != 0 || iter == current_iter) && time)`。初始化
`iter=0, read_done=0` → 第一轮所有线程满足 `iter == current_iter` 自旋等待；而 `iter`
只在 phase-3（屏障之后的阶段）由最后完成的线程推进 → **循环死锁**，仅由
`test_time_condition` 超时 break 解除 → 直接 `return EXIT_SUCCESS`。
**阶段 2 的 Σ(local)==golden 校验、per-vector store/reload 比较、fprintf 全部从未执行**
（实测 `-f no` 直连 stderr，多轮运行 0 字节输出）。

**这是 pre-existing**（main 的 098d4aa 原始移植就有，PR #147 只换了 RNG），但
6e06c24 commit message 声称 "the per-iteration Σ(local)==golden check and the
per-vector store/reload compare are kept (they already verify every operation)" —
对这 6 个文件而言该声明不成立（检查从未运行）。其余 21 个 mesh 文件（read_only/
write_only/asymm_distrib/read_L3 家族）循环真实运转且校验有效（已实测确认其输出）。

注：PR #146 审查记录过 "mesh_upi_* 需多 NUMA 真实硅，CI 中因挂死被 disable" —
挂死根因就是这个屏障。GHA verify-params.py 的 `--disable mesh_upi*` 正是在掩盖它。

### 【F4'｜中｜死代码+时序模型失配】svd_cdouble_sve：init 的 BDCSVD 变纯死代码

`sandstone_eigen_common.h` 的 run() 现在自带 per-thread orig/u_gold/v_gold 并在循环前
prime — 完全忽略 `eigen_test_data` 里的 `orig_matrix/u_matrix/v_matrix`。但
`svd_cdouble_sve.cpp` 的 `sve_probe_and_init` 仍在 init 里做一次完整 BDCSVD
（`calculate_once(d->orig_matrix, ...)`，第 174 行）— 对 auto-size N=4400 是 ~243 秒
的**纯死计算**（结果永不被比较）。

时序模型失配：`size_matrix_preinit` 的 `desired_duration = 0.95 × t(N)` 按旧模型
（init golden + run DUT = 2 次分解）标定；新代码实际执行 **3 次**（init 死算 1 + run
prime 1 + DUT 1），超出模型 50% — 5% 余量设计吸收的是负载方差，不是第三次分解。
该测试的头部注释（两分解模型、内存 12x 预算）也因此失准。其余 9 个 SVD 模板用户
（compile-time Dim）的 init golden 同样变死代码（量小，`Mat::Random`+1 次 SVD），
但 commit 声称 "×10 files" 都改了 — 未提 init 死算问题。

本机（Kunpeng 920 无 SVE）该测试 skip（`test compiled with sve`），无法实测时长 —
结论来自代码结构分析，标注为"待 SVE 主机验证"。

### 【F5'｜中｜LSP 真实诊断】kreg4.cpp：vgetq_lane 非常量 lane 参数

clangd 17 对 `tests/cpu/vector/kreg4.cpp:58-61,86-89` 报 8 处
`[constant_integer_arg_type] argument to '__builtin_neon_vgetq_lane_i32' must be a
constant integer`。这是 ACLE 规范违规：lane 索引是循环变量 `i`。GCC 在 `-O3` 把
4 次迭代的循环完全展开成常量 lane（objdump 确认 `mov w2, v6.s[3]` 等常量提取指令），
运行时行为正确；但 (a) 这是 PR #147 自己在 insert_extract.cpp 里修过的同一问题
（commit 0445a21："vsetq_lane_f32 loop variable became a hard error after the lambda
change exposed it" — 同族修复漏了 kreg4），(b) 任何非 -O3 或未来编译器变更都可能
变成硬错误。**这是本次 LSP 全覆盖扫描中唯一的真实代码诊断**（其余文件的 "errors"
全部是 clangd 内部 tweak 自测失败，可忽略）。

### 【F6'｜中｜stderr flood 未清】vector/kreg 家族 + mesh read_only 家族

PR 声称的 stderr 清理覆盖 fma（真）+lock（部分，见 F2'），但：
- **kreg1-9**（9 文件）：每迭代 4-14 行 ungated stderr（`kreg4 -t 2000 -v` →
  59 个 stderr 块 / 642KB YAML；kreg8 有 14 行/迭代）
- **mesh read_only/write_only/asymm_distrib/read_L3 家族**（21 文件中运行的那些）：
  每迭代 Thread PASS 行（`mesh_upi_sse_read_only_int -t 3000 -n 2 -v` → 1,720 行 /
  329KB YAML）
- gather/gatherscatter 家族的 fprintf 已在 fail 分支内（抽查 gather_f32 正确）✓

### 【F7'｜低｜README 计数偏差】声称 328，实测 329

7e40e3f 把 README 的 291 更新为 "328（--list-tests 实测）"；本机同条件实测 **329**
（vendored 齐备 + ACL）。差 1 可能是 fisttp_arm/acl_gemm 的 ACL 探测差异或计数时点
不同 — 需要在提交前重新对齐（数字本身低影响，但 README 声明 "实测" 就必须可复现）。

### 【F8'｜低｜注释失真（小）】

- `crc32.cpp:46` 注释仍写 "第一次硬件 CRC 计算（CRC-32C）" — 同文件 H15' 注释已
  纠正为 IEEE 802.3（非 CRC32C），旧注释自相矛盾；`crc32c_software` 函数名同样
  带误导性的 "c" 后缀
- `sve512_f64_chain_arm.cpp:172` 注释说 "uniform exponent 0x3FE..0x400 covers
  [0.5, 4)" — 代码只用 0x3FE/0x3FF（[0.5, 2)），注释描述的区间过宽（代码是保守
  正确的，注释错）
- `lsu_store_forward_arm.cpp:133` `static uint64_t golden_cycle = 0;` 成为死变量
  （改动后无任何使用）— 且它原是多线程共享 static，留着会误导后续维护
- fma 家族 `static std::atomic<uint64_t> iter` 保留（fail 分支使用）合理，但
  `fma.cpp` 顶部 `#include <cstdio>`/`<atomic>` 等残留 include 值得顺手清

## 无问题确认（对抗性检查后排除的疑点）

- **crc 独立参考的正确性**：软件逐位 CRC（0xEDB88320）与 `__crc32b` 链独立复核
  一致（'123456789' → 0xCBF43926 双路径同值）；`crc32_fixed_shuffled` 的尾步钳位
  修复数学正确（`pos+step > BLOCK_SIZE` 时 `step=1` 保证 `pos+1 ≤ BLOCK_SIZE`，
  OOB 消除）；steps 序列只喂 crc1、crc2 走软件全量 — 两路径真正独立
- **kreg4/kreg7 SIGTRAP 修复**：`(0xFFFFFFFF)-0+1` 在 32 位回绕为 0 → `%0` 除零
  (SIGTRAP) 的分析正确；直接 `random32()/random64()` 全域映射是正解
- **insert_extract 展开修复**：vsetq_lane 常量 lane 展开正确，switch 分支的
  `default: case 3` 合并安全（lane∈[0,4)）
- **eigen_common 模板**：`fill_matrix_random` 的 [-1,1) 映射保持 SVD 良态；complex
  分支 re/im 独立；动态矩阵 resize 处理（commit 里提到的 SEGV 修复）正确；
  `kRerollEvery=16` 的 cadence 与 golden 同批重算的 same-source-repeat 语义自洽
  （该家族的 verify 属性本就是 same-source repeat，非独立 golden — 与 F1' 的
  GMP 情形不同，不算回归）
- **atomic_simd_512 的 per-iteration 随机**：每通道单写者前提成立
  （`my_id % channels.size()`，MAX_POSSIBLE_CPUS=512 ≥ 线程数）；spin_count>10000
  break 后的校验是 pre-existing 行为，未变
- **spinlock finish 汇总输出**（每 fracture 一次的 counter 汇总行）是设计内合理输出，
  非 flood
- **sve512 家族**：指数带 {0x3FE,0x3FF} ∪ 25% 高汉明表保持 |x|≤2 不变量（512 步链
  不溢出）✓；Fisher-Yates 用 random64 正确；gather/scatter 索引域未变
- **eigen_sparse**：只换 b 向量 RNG、不做 per-iteration re-roll 的决策与 CLAUDE.md
  记录的 ULP flakiness 一致（合理保留）
- **x86-64 非回归**：lock/spinlock/mesh/gather/vector 系列为 arch-neutral（std::atomic/
  NEON 在 #ifdef 内）；crc32 三件套 aarch64 门内、isa-l 九件 arch-neutral 双架构受益；
  eigen 系列 arch-neutral；kreg/insert_extract 改动在 `#ifdef __aarch64__` run 体内

## 与 PR #146 审查结论的关系

PR #147 = PR #146 全部 commit + 10 个新 commit。PR #146 审查发现的 F3（fma stderr
flood）、F6（fma mt19937）已由 5998967 修复；F1（openssl/ipsec 同迭代 golden）、
F2（bigint_mulx_arm 同义反复）**未**修复且 F2 的同构模式在 951e994 里扩展到了
gmp_bignum/gmp_bigadd。PR #146 修复计划（2026-09-20-pr146-sdc-randomization-
hardening-fixes.md）的任务 1 需扩展覆盖这三个 GMP 测试。

## 修复计划

见 `2026-09-21-pr147-sdc-randomization-hardening-fixes.md`（任务 G1′…G7′，
与 PR #146 修复计划互补：G1′ 扩展其任务 1 的范围）。

## 修复落地记录（2026-09-22，7 commits 已推 feat/sdc-randomization-hardening / PR #147）

| 缺陷 | Commit | 决定性验证 |
|---|---|---|
| F1′ GMP 同义反复（gmp_bignum/bigadd/bigint_mulx_arm） | 65b396c | 对称/非对称注入 6/6 fail；golden 与 GMP 200/1000/10000 组独立验证 |
| F2′+F3′ mesh 6 文件屏障死锁空转 | 2731971 | 读端验证 0→104-297 行/测试；sum 注入 fail（原不可达）；all-core 191 线程 470+ fracture 零 fail |
| F2′ lock stderr 声明未兑现（18→19 文件） | d18238c | awk 全目录扫描零残留；locks -v 742KB→23.8KB；in-loop PASS 10080→0 |
| F5′+F6′ kreg 常量 lane + stderr | 300fba0 | LSP 诊断 15→0（kreg4 8 + kreg1 3 + kreg7 4）；kreg4 -v 642KB→12.3KB；sw_mask 注入 fail |
| F6′ mesh 运行家族 stderr（14→16 文件） | 7889116 | sse_read_only -v 329KB→8.1KB；27 NEON mesh 全 pass |
| F4′+F7′+F8′ 死 init/注释/README + block_ok | 87ebc4b | SVD 6+4 测试 pass；README 329/39/31 实测复现；block_ok 注入 fail×4 |
| （追加）mesh asymm 饥饿 3 文件 + SVE wide block_ok 5 文件 | 8502e49 | 读端验证 0→6595/6592/377 行；2 个 avx512 实测已在验证（未动）；SVE 仅 build+LSP（无 SVE 主机，已披露） |

全量回归：16 组全 pass；x86-64 审查 75 文件 0 需注意；final review（CLEAN）。
方法论修正：框架将子进程 stderr dup2 进 -o 日志 memfd — 终端 stderr 恒 0，此前对 avx512 双文件的"0 行"判断是测量通道错误（已记入计划文件防复发）。

### 遗留跟进（非本系列范围，final review 建议的下一批工作）
- **I1**：SVE mesh 12 文件仍带旧缺陷代（3-stable-reads、-n 1 死循环、sysconf reader 计数、mt19937、ghost-reader 假失败窗口）— 需专门移植任务
- **M1**：3 个 NEON asymm 空 block_ok（模式已在 9 兄弟文件验证）
- M2-M5：gmp_bignum per-call 分配、空 struct、~8 未用 include、SVD 头部叙述等卫生项

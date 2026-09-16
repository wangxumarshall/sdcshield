# 集成 sve512_stencil_axis_arm(SCF/stencil 触发配方复现器)

日期:2026-09-15

## 背景

用户提供 cn23154 NUMA3 故障的独立复现程序 example.cpp(testwang/SCF
laplacian 触发配方的最小提取),要求集成为 sdcshield 测试用例,热循环
结构逐字保留。核心结构要素(全部保留):

- `axis_generic_16x2` 内核:16-wide tile、RADIUS=6 双方向 svmla_x 链、
  `svdup_f64(coeff[r])`(clang 下生成 ld1rd 复制装载 —— 诊断报告故障
  指令之一)、`#pragma clang loop unroll(full)`
- 三组 `temp = res0;` 间距赋值 + 保留注释行(寄存器调度扰动,配方成分)
- 文件作用域循环计数器 `j`(内存驻留,非寄存器分配 —— 载入到使用压力)
- 每迭代 `&res0` 高 16 位金丝雀(VA[63:48] 损坏探测器)
- iterations = 100'000'000(现场标定剂量)
- src[i]=i%16+1 / coeff=0.125:全整数精确累加,lane j 精确期望可闭式算出

## 唯一语义修改(依据用户自己的诊断报告 §1.1/§7.1)

原码期望公式 `iterations*(j+1)` 是 RADIUS=4 时代遗留。报告已数学闭环:
RADIUS=6 下每调用每 lane 增量 = 2 方向 × 6 半径 × (j+1) × 0.125 =
1.5×(j+1),且 5 次重跑 bit-identical 证明原判据是软件误报(健康核
deterministic FAIL)。按报告 §7.1 修复为 `1.5*iterations*(j+1)`。

## 框架适配(结构保持,每项均为框架所迫)

- main() → test_init/run/cleanup;src/s_base 在 init 建立后跨线程只读共享
- `j` 改 `static thread_local`:sdcshield 每 CPU 一线程并发跑 test_run,
  全局 j 会跨线程数据竞争(线程 A 写满 iterations 导致线程 B 跳过迭代
  → 假 FAIL);TLS 保持其内存驻留特性
- 金丝雀 printf → log_warning
- `#pragma clang loop` 加 `#if defined(__clang__)` 守卫(GCC -Wall 下
  未知 pragma 告警;目标机 BiSheng clang 行为不变)
- coeff 保持 run 内栈上局部表(与原码一致,保留 ld1rd 的栈源)
- 逐 lane 结果仅在失配时打印(否则 16 lane × 线程数 × 循环轮数刷爆
  日志);chrono 计时删除(框架自带计时)
- svcntd()!=8(非 512-bit VL 的 SVE 机器)干净 EXIT_SKIP;非 SVE 由
  框架编译门跳过

## Task 1(单 commit)— 完成(2026-09-15 实测)
- [x] 写入 tests/cpu/arm64/sve512_stencil_axis_arm.cpp + meson 注册进
      tests_arm64_sve 库
- [x] 编译 326/326,单 TU 重编译 0 warning
- [x] `--list-tests` 注册(第 279 个);计数实测 default 279 / skip 288
- [x] 本机干净 skip(CpuNotSupported / `test compiled with sve`);
      zstd19 回归 pass
- [x] 反汇编:24 条 fmla/fmad(RADIUS=6 双方向全展开链);st1d mul vl
      存在。诚实标注:本机 GCC 未生成 ld1rd(GCC 对 svdup_f64 内存源
      用其它指令),ld1rd 是 BiSheng clang 的典型生成 — 目标机 clang
      工具链下验证
- [x] README 同步(288/279 + 专项表);commit + push

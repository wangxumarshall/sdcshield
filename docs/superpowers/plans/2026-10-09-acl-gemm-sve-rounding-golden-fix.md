# Plan: acl_gemm_sve golden 舍入序修复(fmaf/svmla 单次舍入对齐 + 有限值过滤)

**日期**: 2026-10-09
**分支**: `fix/acl-gemm-sve-rounding`(基于 main)
**文件**: `tests/cpu/sve/arithmetic/acl_gemm_sve.cpp`(唯一改动点)
**状态**: 已实现并验证

## 背景:战役实证的确定性缺陷(非 SDC)

战役 cn23208/cn23218/cn23220 三节点真实失败(608 核全发 6688 error),
同签名字节级一致、RNG 无关 → 确定性测试缺陷,非硬件 SDC。

## 根因(两轮实证)

1. **舍入序倒挂**: 最初假设是 golden `acc += A*B` 被 -ffp-contract 融合为
   fmadd(单次舍入)而 SVE svmul+svadd 为两次舍入;第一版修复用 volatile
   拆分 golden 为两次舍入 —— **该假设被反汇编推翻**: clang -O3 会把
   `svadd(svmul())` intrinsic 图案融合成 `FMLA`(单次舍入),golden 侧
   volatile(fmul+fadd 两次舍入)与之仍然倒挂。2965 万组单步探针证明
   两次舍入路径之间 0 mismatch —— 差异纯来自编译器融合行为。
2. **NaN/Inf 输入**: memset_random 任意位模式,4096 元素/矩阵中约 1/256 为
   NaN/Inf;NaN payload 传播是 IEEE-754 实现自由(见 fpu_special_values_sve
   同类定性),位级比较无意义。

## 最终方案(两路径同为 fused 单次舍入)

1. `naive_gemm` 内积 `acc = std::fmaf(A, B, acc)`(标量 fmadd,单次舍入)。
2. SVE 内积显式 `svmla_f32_x(pg, acc, svdup_f32(A), vb)`(FMLA,单次舍入;
   不再依赖/对抗编译器对 intrinsic 图案的融合)。
3. `test_init` 把 A/B 过滤为有限值(std::isfinite,非有限换 1.5f)。
   特殊值边界行为由 fpu_special_values_sve 专门覆盖。
4. 反汇编复核:golden = `fmadd s0`,SVE = `fmla z2.s`,舍入序一致。

## 验证(CLAUDE.md 规则 2,全部实证)

- 登录节点 builddir2(战役 builddir 严禁 rebuild,md5 12ef01f8… 保持):
  meson setup rc=0,ninja rc=0,修复文件零警告(增量警告均为 Floats.h
  既有 deprecation)。
- cn23201(Kunpeng 920, SVE-512): `-e acl_gemm_sve -t 5000 -n 1` 与全核
  均 `exit: pass`(volatile 版仍 fail,反汇编定位后本方案全绿)。
- 回归: `-e zstd19 -t 2000` 单线程/全核均 pass。
- x86 非回归: 改动全部在 tests/cpu/sve/(整目录 aarch64-only meson guard)。

## 硬约束

- 战役 builddir 严禁 rebuild(md5 保持);构建一律 builddir2。
- One patch per commit: 独立 commit,`git commit -s`,推 feature 分支。
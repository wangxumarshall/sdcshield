# 集成 sve512_gather_scatter_arm 到 arm64 测试分类

日期：2026-09-14

## 背景

用户提供了一个新的 SVE gather/scather SDC 测试源文件（从 sve512_fma 研究
中单独提取的 workload 4）：

- `sve512_gather_scatter_arm.cpp` — SVE 全向量长度 gather/scatter 压力：
  `svld1_gather_u64index_f64`（按元素索引从置换索引表 gather）+
  `svmla_f64_x`（v = 1.5 + v*v）+ `svst1_scatter_u64index_f64`（按同一
  索引 scatter 回暂存区），与走同一间接索引的标量 golden 逐字节比较。
- 本机（Kunpeng 920）**无 SVE**（/proc/cpuinfo 实测无 sve 特性），用户
  要求编译通过即可，不要求本机跑通（test_init 入口即探测 HWCAP_SVE，
  无 SVE 时干净 EXIT_SKIP）。

## 决策

- 文件放入 `tests/cpu/arm64/`（整个子目录仅在 aarch64 构建，x86-64 不受
  影响）。
- **不并入 `tests_arm64` 库**：该库基线 `-march=armv8.1-a+crc+crypto` 不含
  +sve，`<arm_sve.h>` 内联函数无法编译；且不能给整个库加 +sve——GCC 会把
  其他 NEON 测试自动向量化成 SVE 指令，无 SVE 机器上 SIGILL。
- 在 `tests/cpu/arm64/meson.build` 新建独立静态库 `tests_arm64_sve`，
  `-march=armv8.2-a+sve`（与 tests/cpu/meson.build 已有 SVE 构建块相同的
  march 组合），仅含本文件。用户代码原样，零修改。
- 运行时安全：test_init 第一行即探测 HWCAP_SVE，失败即 return EXIT_SKIP，
  无 SVE 机器上不执行任何 SVE 指令。
- README.md 同步：用例总数以 `--list-tests` 实测数为准更新，ARM64 SDC
  专项表补该测试。

## One-patch-per-unit 分解

单个 unit（一个 commit）。

### Task 1: 集成 sve512_gather_scatter_arm

文件变更：
- 新建 `tests/cpu/arm64/sve512_gather_scatter_arm.cpp`（用户代码原样）
- `tests/cpu/arm64/meson.build`：新增 `tests_arm64_sve` 静态库块
- `README.md`：用例计数 + ARM64 SDC 专项表
- 本计划文件

验证（真实命令，引用真实输出）：
1. `ninja -C builddir` — 零新增 error/warning
2. `./builddir/sdcshield --list-tests | grep sve512_gather_scatter_arm` — 已注册
3. `./builddir/sdcshield -e sve512_gather_scatter_arm -t 1000` — 本机无 SVE，
   预期干净 skip（证明 HWCAP 守卫生效、无 SVE 指令泄漏到 skip 路径）
4. 回归：`./builddir/sdcshield -e zstd19 -t 3000` — `exit: pass`
5. x86-64 非回归：全部变更在 aarch64 guard 内（meson 检查确认）

## 验证结果（2026-09-14 实测，基座 origin/main 38eb721）

基座说明：开始集成时本地 main 落后 origin/main 12 个提交（上游 movbe 系列 + rowmajor
移入恰好改了同一个 tests/cpu/arm64/meson.build），已将 feat/sve512-gather-scatter-arm
重置到 origin/main 后重放 SVE 库块，使验证状态 == 合并后状态。

1. `ninja -C builddir`（reconfigure 后全量）— 318/318 成功，0 error；单独 touch 重编
   `sve512_gather_scatter_arm.cpp` — 0 warning
2. `--list-tests | grep sve512_gather_scatter_arm` — 已注册（第 271 个）；
   计数实测：PROD 271、+BETA 275、+SKIP 280
3. `-e sve512_gather_scatter_arm -t 1000` — `result: skip`，category
   `CpuNotSupported`（本机 Kunpeng 920 无 SVE；框架层 `test compiled with sve`
   守卫在 fork 前生效，sandstone_run.cpp:1558；测试内 HWCAP_SVE 探测为第二层守卫，
   与 eigen_svd_cdouble_sve 行为一致）。总体 `exit: pass`，无 SIGILL —— 无 SVE
   机器上无 SVE 指令泄漏
4. 回归：`-e zstd19 -t 3000` — `exit: pass`
5. x86-64 非回归：全部变更在 `host_machine.cpu_family() == 'aarch64'` meson guard
   内（diff 仅 meson guard 块内新增），x86-64 构建路径不进入该子目录
6. SVE 代码生成证据（无 qemu，编译产物反汇编实测）：`ptrue p0.b`、
   `ld1d {z0.d}, p0/z, [x0, z1.d, lsl #3]`（gather）、`fmad z0.d, p0/m, z0.d, z2.d`
   （svmla_f64_x 的 SVE 编码）、`st1d {z0.d}, p0, [x1, z1.d, lsl #3]`（scatter）、
   `cntd`（svcntd）、标量 golden `fmadd` —— gather/FMA/scatter 数据通路全部真实生成

- [x] Task 1 完成

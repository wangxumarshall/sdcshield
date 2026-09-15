# 集成 sve512_fma 家族 workload 1/2/3 到 arm64 测试分类

日期：2026-09-15

## 背景

用户提供 sve512_fma 研究的 workload 1/2/3 三个独立提取测试源文件
（workload 4 `sve512_gather_scatter_arm` 已于 09-14 集成为 a198ac8）。
代码原样集成，零修改：

- `sve512_f64_chain_arm.cpp` — f64 FMLA 串行依赖链：512 步 `svmla_f64_x`，
  高汉明有限操作数（|x|<=2 保证 512 步累加不溢出到 Inf/NaN，全链 byte-exact
  可比），标量 golden 以相同运算序重算，逐 lane 字节比对。
- `sve512_f64_special_arm.cpp` — f64 特殊值链：同链结构跑 IEEE-754 特殊值表
  （NaN/sNaN/±Inf/±0/±1），按类别比对（fsu_byteexact_arm 先例：NaN payload
  跨 FMA 实现不保证确定，类别变化才是失败；有限值仍 byte-exact）。
- `sve512_f32_chain_arm.cpp` — f32 FMLA 串行依赖链：`svmla_f32_x` 16-lane
  f32 数据通路，byte-exact vs 标量 `fmaf()` 参考 golden。

三个测试均不设 `-msve-vector-bits`（sizeless 类型取 CPU 运行时 VL，
HiSilicon 0xd22 上为 512-bit）；`test_init` 先探测 HWCAP_SVE 再执行任何
SVE 指令，无 SVE CPU 干净 `EXIT_SKIP`。

本机（Kunpeng 920）无 SVE：验收标准与 09-14 的 gather_scatter 相同 —
编译零告警 + 注册 + 框架级干净 skip（`test compiled with sve`，
sandstone_run.cpp:1558，fork 前生效）+ zstd19 回归 + 反汇编 SVE 代码生成
证据。SVE 硬件上的实际 pass 需后续在 SVE 机器复验。

## 决策

- 三个文件放 `tests/cpu/arm64/`（用户指定），加入 a198ac8 建立的
  `tests_arm64_sve` 静态库（`-march=armv8.2-a+sve`，无 `-msve-vector-bits`，
  与 gather_scatter 相同编译目标）。
- 分支沿用 `feat/sve512-gather-scatter-arm`（同族测试堆叠在 a198ac8 之上，
  一个 PR 覆盖 workload 1-4）。
- 集成前核查发现 README 两处既有问题，作为独立 unit 先修（Task 0）：
  1. `## 运行测试` 节前残留合并冲突标记 `>>>>>>> main`；
  2. 质量分级表仍是旧计数（PROD 264 / 合计 273），与 a198ac8 更新后的
     头部计数（PROD 271 / 合计 280）矛盾。
- README 计数沿用 09-14 约定：以 builddir（ssl_link_type=dynamic）实测为准，
  每个 commit 保持 README 自洽（task 1→272/281，task 2→273/282，
  task 3→274/283）。

## One-patch-per-unit 分解（4 个 commit，逐个验证→提交→推送）

### Task 0: README 修复 — 冲突标记 + 过期质量分级表
文件变更：`README.md`
- [ ] 核实并清除合并冲突标记（若存在配对 `<<<<<<<`/`=======` 一并正确解决）
- [ ] 质量分级表 PROD 264→271、合计 273→280（对齐 a198ac8 实测）
- [ ] commit + push

### Task 1: sve512_f64_chain_arm — 完成（2026-09-15 实测）
- [x] 写入用户代码（原样）到 `tests/cpu/arm64/sve512_f64_chain_arm.cpp`
- [x] meson 注册进 `tests_arm64_sve` files()（带注释）
- [x] reconfigure + ninja：319/319 成功；单 TU 重编译 0 warning（grep rc=1）
- [x] `--list-tests` 注册（第 272 个）；计数实测 default 272
- [x] `-e sve512_f64_chain_arm -t 1000`：result skip / CpuNotSupported /
      `test compiled with sve`，总体 exit: pass，无 SIGILL
- [x] 回归：`-e zstd19 -t 3000` exit: pass
- [x] objdump 实测：`fmla z0.d, p0/m, z1.d, z2.d`（svmla_f64_x 的 SVE
      编码）、`fmadd d0`（标量 golden）、`cntd`、`ptrue p0.b`
- [x] README：ARM64 SDC 专项行补测试 + 计数 272/281；commit + push

### Task 2: sve512_f64_special_arm — 完成（2026-09-15 实测）
- [x] 写入用户代码（原样）到 `tests/cpu/arm64/sve512_f64_special_arm.cpp`
- [x] meson 注册；ninja 320/320；单 TU 重编译 0 warning
- [x] `--list-tests` 注册（第 273 个）；计数实测 default 273
- [x] `-e sve512_f64_special_arm -t 1000`：result skip / CpuNotSupported /
      `test compiled with sve`，总体 exit: pass，无 SIGILL
- [x] 回归：`-e zstd19 -t 3000` exit: pass
- [x] objdump 实测：`fmla z0.d, p0/m, z1.d, z2.d`、`fmadd d0`、`cntd`、
      `ptrue p0.b`
- [x] README 计数 273/282 + 专项表；commit + push

### Task 3: sve512_f32_chain_arm — 完成（2026-09-15 实测）
- [x] 写入用户代码（原样）到 `tests/cpu/arm64/sve512_f32_chain_arm.cpp`
- [x] meson 注册；ninja 321/321；单 TU 重编译 0 warning
- [x] `--list-tests` 注册（第 274 个）；计数实测 default 274 / beta 278 /
      skip 283
- [x] `-e sve512_f32_chain_arm -t 1000`：result skip / CpuNotSupported /
      `test compiled with sve`，总体 exit: pass，无 SIGILL
- [x] 回归：`-e zstd19 -t 3000` exit: pass
- [x] objdump 实测（f32 通路）：`fmla z0.s, p0/m, z1.s, z2.s`、`cntw`
      （svcntw）、`ptrue p0.b`
- [x] README 计数 274/283 + 专项表；commit + push

## 完成汇总（4 commit 全部推送）

| Task | 测试 | commit |
|---|---|---|
| 0 | README 冲突标记 + 过期计数修复 | 7179185 |
| 1 | sve512_f64_chain_arm | f94fd72 |
| 2 | sve512_f64_special_arm | 8669cd3 |
| 3 | sve512_f32_chain_arm | （本提交） |

至此 sve512_fma 家族 workload 1/2/3/4 全部就位（4 = gather_scatter，
a198ac8 已合并 main）。本机（Kunpeng 920 无 SVE）验证到编译 + 代码生成 +
框架级干净 skip 层面；SVE 硬件上的实际 pass 需在 SVE 机器上复验。

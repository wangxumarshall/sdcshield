# issue #520: SVE1024 float tier 触发 Eigen OBJECT_ALLOCATED_ON_STACK_IS_TOO_BIG 修复计划

> **For agentic workers:** 本计划是单任务小修复（一处 meson.build 改动 + 验证矩阵）。顺序执行，
> 验证通过后按 DCO 提交。步骤用 checkbox 跟踪。

**Goal:** 恢复 main 在 `--buildtype=debugoptimized` 下的可编译性（CI 四档全红的根因），
不改变 release 运行时行为（除受影响的 1024 档 alloca/malloc 切换窗口），
x86-64 零触碰。

**判定:** **A — 真实缺陷**（批判性分析结论，复现 100%，见 §1/§2）。

## 1. 批判性分析（已完成的复核，全部实测）

### 1.1 issue 事实主张逐条核对

| issue 主张 | 复核结果 | 证据 |
|---|---|---|
| main 自 b12d9ada 起 debugoptimized 不可编译 | ✅ 100% 复现 | `meson setup /tmp/bd-dbg --buildtype=debugoptimized` 后编译 `tests/cpu/libtests_eigen_svd_f32_vl1024.a.p/eigen_svd_svd_float_sve_body.cpp.o` → `third-party/eigen5/Eigen/src/Core/DenseStorage.h:61:5: error: static assertion failed: OBJECT_ALLOCATED_ON_STACK_IS_TOO_BIG`，`note: the comparison reduces to '(147456 <= 131072)'` |
| 断言激活条件：debugoptimized 无 NDEBUG → EIGEN_NO_DEBUG 未定义 | ✅ 源码级确认 | `DenseStorage.h:56-62`：`#if defined(EIGEN_NO_DEBUG)` 时 `plain_array() = default`（断言编译掉）；否则构造函数带 `EIGEN_MAKE_STACK_ALLOCATION_ASSERT(Size * sizeof(T))` |
| 147456 = BDCSVD triangularBuffer 192×192×4B | ✅ 实例化栈确认 | 报错栈（逐帧）：`svd_float_sve_body.cpp:40` → `sizing_probe` → `calculate_once` → `BDCSVD::compute_impl`（BDCSVD.h:371 `HouseholderQR::compute` / BDCSVD.h:434 `copyUV`）→ `product_triangular_matrix_matrix` → `TriangularMatrixMatrix.h:116` 的 `Matrix<float, 192, 192>` 固定尺寸 `triangularBuffer` |
| release 口径不受影响（本地全绿） | ✅ 机制成立 | NDEBUG → EIGEN_NO_DEBUG → 断言不存在于 release 产物；main 的 release 构建/实跑历史全绿与此吻合 |
| cdouble（packet=16）不触发 | ✅ 实测对照组 | 见 §1.2 矩阵 |

**注意**：issue 正文说 "BDCSVD triangularBuffer" —— 精确地说该缓冲属于 Eigen 三角乘积内核
`product_triangular_matrix_matrix`（TriangularMatrixMatrix.h:116），是 BDCSVD 的 HouseholderQR/
copyUV 路径触发的 TriangularShape 乘积实例化；`SmallPanelWidth = 2 * plain_enum_max(Traits::mr,
Traits::nr)`（TriangularMatrixMatrix.h:73）在 f32 packet=32（VL=1024）下得 192。issue 的表述
在根因方向上正确，缓冲归属略欠精确，不影响判定。

### 1.2 同类面全量排查（10 个 vl1024 per-tier body TU 逐个编译，实测）

| body TU（vl1024） | pfx | scalar | 断言 | 实测 |
|---|---|---|---|---|
| eigen_svd/svd_cdouble_sve_body.cpp | svd | cdouble | 不触发 | ✅ .o 产出 |
| eigen_gemm/gemm_double_max_arm_body.cpp | gemm | f64 | 不触发 | ✅ .o 产出 |
| eigen_gemm/gemm_f64_sve_body.cpp | gemm_f64 | f64 | 不触发 | ✅ .o 产出 |
| eigen_gemm/gemm_f32_sve_body.cpp | gemm_f32 | f32 | 不触发 | ✅ .o 产出（纯 GEMM 走 level3_blocking，不进 TriangularShape 乘积路径） |
| eigen_gemm/gemm_c128_sve_body.cpp | gemm_c128 | cfloat | 不触发 | ✅ .o 产出 |
| eigen_svd/svd_double_sve_body.cpp | svd_f64 | f64 | 不触发 | ✅ .o 产出 |
| **eigen_svd/svd_float_sve_body.cpp** | svd_f32 | **f32** | **触发 147456>131072** | ❌ BDCSVD→HouseholderQR/copyUV→triangular product |
| eigen_svd_jacobi/svd_cdouble_sve_body.cpp | svd_j_c128 | cdouble | 不触发 | ✅ .o 产出 |
| eigen_svd_jacobi/svd_double_sve_body.cpp | svd_j_f64 | f64 | 不触发 | ✅ .o 产出 |
| **eigen_svd_jacobi/svd_float_sve_body.cpp** | svd_j_f32 | **f32** | **触发 147456>131072** | ❌ JacobiSVD→qr_preconditioner_impl→triangular product |

f32 SVD 两 spec 的 128/256/512 档在 debugoptimized 下全部编译通过（0 error，6 .o 产出）——
受害面严格锁定 **f32 SVD body × tier=1024**。f64/cfloat/cdouble packet 减半使 SmallPanelWidth ≤ 96，
76×96×8B=73.7KB / 96×96×4B=36.9KB 均低于 128KB 哨兵。

受害 spec → 受服务 test id：
- `svd_f32` body → `eigen_svd_sve` + `eigen_svd_fvectors_sve`（两 dispatcher 共享）
- `svd_j_f32` body → `eigen_svd_jacobi_sve`（SKIP 级，--quality=-1）

## 2. 修复方案论证

issue 给出三条方向，论证如下：

**方向②（panel/triangularBuffer heap 化）— 否决。** 该缓冲是 vendored Eigen
（`third-party/eigen5`）内部实现，heap 化必须改 Eigen 源码：a) vendored 第三方应保持上游形状，
upstream 同步负担与 provenance 负担；b) 这份 Eigen 同源服务 x86-64 TU（rule 5：x86-64 untouched），
改它就动了 x86-64 构建面；c) 上游 Eigen 官方旋钮（EIGEN_STACK_ALLOCATION_LIMIT）本就是为
此类下游场景设计，不必侵入上游。

**方向③（#518 联动）— 不阻塞、不捆绑。** #518（arm64-march-tiers-unify）改变 meson 目标集
暴露面，但无论目标集怎么统一，f32 SVD payload 在 1024 档编译都会撞同一断言——本修复不依赖
#518；#518 落地后本修复依然必要且正确。按 rule 1（一 patch 一 unit）不合并处理，仅在 PR
正文注明联动关系。

**方向①（对受害 TU 定义 EIGEN_STACK_ALLOCATION_LIMIT）— 采纳，且限定最小暴露面。**

关键考证（决定实现形态）：`EIGEN_STACK_ALLOCATION_LIMIT` 不只控制断言哨兵，还控制
**运行时** alloca/malloc 切换：
- `Memory.h:803-810` `ei_declare_aligned_stack_constructed_variable`：
  `sizeof(TYPE)*(SIZE) <= EIGEN_STACK_ALLOCATION_LIMIT` → `__builtin_alloca_with_align`，
  否则 `aligned_malloc`；
- `TriangularMatrixVector.h:306/324` 同款切换。

该切换**不受 NDEBUG 控制**（release 构建同样生效）。因此把上限放宽到 262144 意味着
128KB..256KB 窗口的运行时缓冲从堆改走栈——数值 bit-identical（alloca/malloc 不改计算，
只是内存来源），但栈用量最多 +128KB/帧。框架 worker 线程栈 `THREAD_STACK_SIZE = 8192*1024`
（sandstone.h:63，8MB），余量充足。

由此，三个候选放置位置的决断：

| 放置位置 | 影响面 | 结论 |
|---|---|---|
| 公共头 sandstone_eigen_common.h | 全部 eigen_svd TU（含 x86-64 的 svd.cpp 等） | ❌ 违反 rule 5，殃及无害 TU |
| 受害 body TU 源文件内 #define | 该 body 全部 4 档（含本来无害的 128/256/512 档的 alloca 切换窗口） | ⚠️ 可行但面大：三个无害档与 main 不再 bit-identical |
| **meson per-tier：仅受害 spec × tier==1024 加 -D** | 只动实际炸的那两个编译单元；128/256/512 档、f64/cfloat/cdouble/gemm 全部 bit-identical；x86-64 不编这些 TU（aarch64 guard）零触碰 | ✅ **采纳** |

## 3. 修改内容（单 commit）

**Modify: `tests/cpu/meson.build`** —— `eigen_sve_body_specs` 表加第三列
`eigen_stack_limit_1024`（0 = 不覆盖），仅 `svd_f32`/`svd_j_f32` 两行置 262144；
tier 循环内 `tier == '1024' and spec[2] != 0` 时向 `eigen_tier_args` 追加
`-DEIGEN_STACK_ALLOCATION_LIMIT=262144`。附注释说明：
- 192 panel 的来源（f32 packet@1024=32 → SmallPanelWidth=2×max(mr,nr)=192 →
  `Matrix<float,192,192>`=147456B > 默认 131072B）；
- 仅 1024 档（其余档实测不触发）；
- 262144 的依据（>147456 的最小 2 幂取整、issue 建议值、下游生态常用值）；
- 副作用如实声明（断言哨兵与 alloca/malloc 切换阈值同抬，仅本 TU 本档）。

- [x] meson.build 修改（如上）
- [x] `meson setup` 两个新构建目录（release/debugoptimized）+ reconfigure 既有 builddir

## 4. 验证矩阵（rule 2，100% 真实，引用真实输出）

| # | 验证项 | 口径 | 预期 |
|---|---|---|---|
| a | debugoptimized 全量构建 | /tmp/bd-dbg 修复后 `ninja` | 零 error（CI 四档全红的直接复核） |
| b | release 构建 | `PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release && ./scripts/build.sh` | 零新警告 |
| c | 本机 SVE-256 实跑受害用例 | `eigen_svd_sve`、`eigen_svd_fvectors_sve`、`eigen_svd_jacobi_sve`（SKIP 级 --quality=-1）各 `-t 5000` | 非 SKIP、无 FAIL（用例本身行为不受 meson 改动影响——256 档 args 不变，跑通即证明无回归） |
| d | 1024 档 gem5-fi 验证 | /home/sdc/wangxu/gem5-fi，CHAOS/SE，同源双内核，`--sve_vl 8`（=1024bit），受影响用例 | tier=1024、checksum、`SDCSHIELD_FAILS=0`（本机芯片不可跑 1024 档，rule 2 gem5-fi 条款） |
| e | 回归 | `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `-v` 确认 | `exit: pass` 非 skip |
| f | x86-64 非回归 | inspection | 改动在 `host_machine.cpu_family() == 'aarch64'` guard 内；x86-64 不编译这些 TU；meson 表第三列对 x86 路径无副作用 |

补充项（超出 issue 最低要求但廉价的加固）：debugoptimized 产物的 `--list-tests` 正常列出
受害用例（确认链接完整），以及 release 二进制 `--list-tests` 计数与 main 基线一致。

### 4.1 实测结果（2026-11-05，全部引用真实输出）

- **(a) debugoptimized 全量构建**：`ninja -C /tmp/bd-dbg -j 63` → `[779/779] Linking
  target sdcshield`，exit 0，零 error。两个此前失败 TU
  （`libtests_eigen_svd_f32_vl1024.a.p/eigen_svd_svd_float_sve_body.cpp.o`、
  `libtests_eigen_svd_j_f32_vl1024.a.p/eigen_svd_jacobi_svd_float_sve_body.cpp.o`）
  编译通过；`ninja -t commands` 确认两 TU 命令行含
  `-DEIGEN_STACK_ALLOCATION_LIMIT=262144`，且 svd_f32 的 vl512 档与 svd (cdouble)
  的 vl1024 档命令行**不含**该宏（对照组，精确命中）。`--list-tests` 列出
  eigen_svd_sve / eigen_svd_fvectors_sve（PROD），`--quality=-1` 列出
  eigen_svd_jacobi_sve。加强项：debugoptimized 口径（运行时 assert 激活）实跑
  三受害用例各 `-t 5000` 全核 → 三次 `exit: pass`。
- **(b) release 构建**：`./scripts/build.sh` → `[769/769] Linking target sdcshield`，
  exit 0。全日志仅 1 条 warning（`mesh/mesh_upi_sse_sym.cpp:202:70` -Wformat）——
  `git stash` 后以 main 口径重编同一 TU 警告原样重现，证明为 main 既有警告，
  **本次修复零新警告**。
- **(c) 本机 SVE-256 实跑**（release 口径，全核 `-t 5000`）：
  - `eigen_svd_sve` → `exit: pass`
  - `eigen_svd_fvectors_sve` → `exit: pass`
  - `eigen_svd_jacobi_sve`（`--quality=-1`）→ `exit: pass`（test-runtime 114.252s）
  三个用例 256/128 档 args 与 main bit-identical（本修复只动 1024 档），跑通即无回归。
- **(d) gem5-fi 1024 档**（CHAOS/SE，`configs/se/arm_chaos.py --cpu O3 --sve_vl 8`，
  float SVD 载荷 artifact-matrix ruling 指定 1024 档跑 O3）——**双口径对照实验**：
  - BDCSVD（eigen_svd_sve 镜像内核 eigen_svd_sve_vl1024_dual）：
    main 口径 → `SDCSHIELD_FAILS=0`，checksum `e487897d65f3c710`，math-error-max
    3.874e-06；fixed 口径（+`-DEIGEN_STACK_ALLOCATION_LIMIT=262144` 重编）→
    `SDCSHIELD_FAILS=0`，checksum `e487897d65f3c710`，math-error-max 3.874e-06。
    两口径 checksum **逐字节一致**，且均复现 2026-11-02 上次会话先例
    `e487897d65f3c710`（commit ed88fd13 记录）。
  - JacobiSVD（eigen_svd_jacobi_sve 镜像内核 eigen_svd_jacobi_sve_vl1024_dual）：
    main 口径 → `SDCSHIELD_FAILS=0`，checksum `d8477b6e1a735f3c`，math-error-max
    1.532e-05（复现先例 commit 4f8d2a41 记录 `d8477b6e1a735f3c`）；
    fixed 口径结果见 PR 正文（与本节同时实测）。
    双口径逐字节一致 ⇒ alloca/malloc 窗口切换数值零变化（§5 风险项的实证封堵）。
- **(e) zstd19 回归**：`./builddir/sdcshield -e zstd19 -t 2000 -n 1 -v` →
  `result: pass`、`result: pass`、`exit: pass`（-v 确认非 skip）。
- **(f) x86-64 非回归（inspection 逐条）**：
  1. 全部改动位于 meson.build `if host_machine.cpu_family() == 'aarch64'` 块内
     （表 + tier 循环），x86-64 configure 时整块跳过，`eigen_sve_body_specs`
     表根本不评估，第三列对 x86-64 零副作用；
  2. 零 C/C++ 源文件改动——x86-64 全部 TU（含同样使用 vendored Eigen 的
     svd.cpp/svd_float.cpp 等非 SVE eigen 用例）编译输入逐字节原样；
  3. vendored Eigen（third-party/eigen5）零改动；
  4. 主仓构建面唯一变化是两个 ARM64-only per-tier 静态库的编译参数。
- 用例集/文档计数不受影响：无 test 注册变化，README/CLAUDE.md 计数链无需更新。

### 4.2 CI 实证与边界（PR #532 run 41985710099，2026-11-05；对照 PR #519 run 41816857017）

修复前（PR #519，2026-11-04T10:58:32 日志）：`Build for CPU (Linux)` step 死于
`../third-party/eigen5/Eigen/src/Core/DenseStorage.h:61:5: error: static assertion failed: OBJECT_ALLOCATED_ON_STACK_IS_TOO_BIG`。

修复后（PR #532）step 级结论（actions/jobs/127105915306）：
- `Build for CPU (Linux)` => **success**（debugoptimized 断言消除，CI 实证）；
- `run unittests` / `verify selftests` / `run selftests` => success。

job 级仍红与 Multi-version 红的定性（**独立故障，非 #520 范围，sweep 红线只报告不修**）：
- `run tests quickly` failure：`low_load_cache_pressure` 与 `low_load_cache_pressure_neon`
  在 GitHub hosted ARM runner 上 `result: crash, code: 6 (Aborted), core-dump: true`；
  本机复测两用例 `-t 2000` 均 `exit: pass` ⇒ 环境性（与 host memory 记录的
  Build-binary 恒红"非载荷所致"同性质）。pre-b12d9ada 对照 run（41790777945）同样
  Build step 成功而 job 红于 quick——即该环境故障在 b12d9ada 之前已存在。
- `Multi-version`×3：容器内 `--buildtype=release`（container-build.sh:329，NDEBUG 下
  断言本就编译掉）构建成功（binary-sha256 已产出），失败在 `[5/5] 功能验证`
  （smoke = `--list-tests` + `zstd19 -t 1000 -n 1`，verify-built-pristine.sh:66-70）；
  pre-b12d9ada run 的 Multi-version×3 全 success，post-b12d9ada 变红——直接死因在
  smoke verify，与本 static_assert 正交，需另行排查（候选：#471 merge 引入的
  `--list-tests` 输出面变化或镜像/环境因素）。

结论：issue #520 的"Build binary 不可编译"已消除（CI step 级实证）；issue 声称的
"四档全红"中 Multi-version×3 与 quick suite 红是两个独立故障面，超出本修复范围，
如实移交后续排查。

## 5. 风险与诚实边界

- **alloca 窗口副作用仅存在于 1024 档**：128KB..256KB 运行时缓冲改走 alloca；本机无 1024 档
  真机，该窗口的运行时行为由 gem5-fi 1024 档验证兜底（SDCSHIELD_FAILS=0 + checksum 与
  main 口径互证）。若 gem5 也不可行则如实声明验证边界。
- gem5 SE 模式线程栈语义与真机可能不同——如实记录（gem5 验证边界条款）。
- `#518` 落地时若把 per-tier 目标集重构，本修复的表第三列语义随之迁移——在 PR 注明联动。
- CI 的 Multi-version 三档与 Build binary 用同一 debugoptimized 哨兵机制；本修复消除的是
  编译期断言，不改变任何 release 产物（除 1024 档 alloca 窗口）。

## 6. 提交与 PR

- 单 commit：`fix(build): SVE1024 f32 SVD tier 过 Eigen 栈哨兵——per-TU 提 EIGEN_STACK_ALLOCATION_LIMIT (#520)`
- DCO `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By；push `fix/issue-520`。
- PR body：判定 A + 根因链 + 方向论证 + 逐测试完整验证证据（禁跨测试斜杠简写）+ `fixes #520`。
- 推送后 `gh run list --limit 3` 确认 workflow 触发（已知瞬时丢失风险，未触发则记录在 PR）。

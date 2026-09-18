# ACL 源码级 vendor + pocketfft 逆变换容差校验

> **For agentic workers:** REQUIRED SUB-SKILL: superpowers:subagent-driven-development (recommended) or superpowers:executing-plans. Steps use Markdown task-list checkboxes; live plan = single source of truth for done vs pending.

**Goal:** (1) 把 ACL(Arm Compute Library) 从"host 预装二进制 + 版本割裂 + acl_gemm 永久死码"改为**源码级 vendor**，与 openssl/openblas/sleef 同构：`third-party/acl/` 存 tarball + `build.sh` → `install/lib/*.a`，meson 用 install/-probe 模式链接；并修掉 acl_gemm 的 fork-safety SIGSEGV 与 1e-3f 容差，让 NEGEMM 通路真实压测。**单一 ACL v23.02 在三个 OS 各自容器内原生编译（不降级）**，实现 20.03/22.03/24.03 全部覆盖。(2) pocketfft 逆变换(cfft_backward/rfft_backward)从"算了不比较的死负载"改为带容差的 round-trip 校验。

## Architecture

- **vendor 模式**：复制仓库已有 openssl/openblas 的 vendored 模式——tarball 进 git 做 provenance，`build.sh` 用 CMake 编译并 `cmake --install` 到 `install/lib/libarm_compute-core.a`；meson 探测 `install/` 命中则链静态库，缺省则打 message 优雅跳过。**不再**依赖 `/usr/lib64/libarm_compute.so`、`-Dacl_incdir` host 头树、或 container-build.sh 里的 sed/挂载 hack。
- **版本钉死 = 单一 v23.02**（首个纯 CMake 官方版）。**三 OS 全覆盖的关键实证**：v23.02 `CMakeLists.txt:50-54` 要求 `gcc >= 10.2`（cmake≥3.13, C++14）——20.03 经 `supplement-20.03-gcc10.sh` 已补 gcc-toolset-10(10.3)、22.03 自带 10.3、24.03 自带 12.3，**三者都满足**，无需按版本矩阵 vendor 多份。Task 1 第一步用真实源码 grep 核对两个测试文件用到的一整套 API（NEGEMM/NECast/NEScheduler::get().set_num_threads/Tensor/TensorInfo/import_memory/ConvertPolicy）在 v23.02 里都存在，任一缺失则记录并改测试（不赌兼容）。
- **fork-safety 修复**：把 `acl_gemm.cpp` 三个 tensor(a/b/d) 从 `allocator()->allocate()` 改为 `import_memory(alignas(128) per-thread 缓冲)`，直接复用 `fisttp_arm.cpp:105-106` 已验证跑通的模式（ACL 不 own 任何需跨 fork 存活的内存）。
- **容差修复**：naive 三重循环 vs ACL NEGEMM 的累加顺序不同 → 无法 byte-exact；但不能用现 1e-3f 绝对容差（放过所有单比特错）。改为**高精度参考**：init 用 `long double` 累加求 golden，run 结果做相对容差比较（约 1e-5 相对，>1e3 ulp 才判失败），保证一致性错能逃逸、瞬态 1-ulp 错交给同源 byte-exact。
- **pocketfft 逆变换**：run 里 cfft_backward/rfft_backward 结果加 round-trip 容差校验。
- **三 OS 容器内原生编译**（对齐 openssl/openblas 的 `.glibc-build-tag`→容器内重建）：host `build.sh` 提供有网主机快速构建；离线容器构建则用 ACL 源码 + 各自工具链在容器内编译。**唯一新增供给 = cmake≥3.13 noarch**（20.03/22.03 容器无 cmake，须像补 meson 0.59 一样补 noarch 包；24.03 已有）。20.03 容器编译 ACL 时须用 gcc-toolset-10 而非系统 gcc 7.3。
- **scope 明确**：host 24.03 源码级构建 + 20.03/22.03 容器内各自原生编译，**三 OS 全压测 ACL，无降级**。

## Global Constraints

- **一补丁一单元**：Task 1..5 各一个 commit；Task 内若再遇 bug 修复，各自独立 commit。
- **验证 100% 真实**：所有 RESULT 行来自真实运行，引用实测输出；禁止 assumed-to-pass。
- **分支**：开工先 `git checkout -b feat/vendor-acl-source-pocketfft-inverse`；每 commit 后 push，不推 main。
- **x86-64 非回归**：ACL 仅 aarch64（arithmetic_arm 整子目录已被 x86 guard 隔离）；pocketfft 为共享源，改动须 `#ifdef __aarch64__` 或对 x86 语义等价。
- **文档同步**：改完更新 CLAUDE.md 的 vendored-lib 清单 + README 第三方库节。

## 已确认的关键事实(实测/已读源码)

1. ACL 源码/二进制/tarball 均不在本仓库；`enable_acl` 默认 `auto`(meson_options.txt:45)，本机 `libarm_compute` 不存在且 `arm-opt-install` 头树不存在 → 本机当前 acl_sources=[]（零编译）。
2. `arithmetic_arm/meson.build:13-16` 注释自称"本机验证 libarm_compute.so 在 /usr/lib64"——与现状矛盾，过时注释需一并修。
3. `acl_gemm.cpp:59`(init) 与 `:109`(run) 均裸 `return EXIT_SKIP`，无 log_skip，其后全部死码；死码比较逻辑 `:124` 用 1e-3f 绝对容差。
4. `fisttp_arm.cpp` **是活的** ACL 测试(FCVTZS/NECast)，且 `:105-106` 用 `import_memory` 指向 per-thread `alignas(128)` 栈缓冲解决 fork-safety——acl_gemm 应照抄。
5. `acl_gemm.cpp:42-52` 现 golden 用 float naive 三重循环(`naive_gemm`)；`openblas_gemm/dgemm.cpp` 用同源 cblas 算 golden(byte-exact)——两个 GEMM 测试的 golden 策略本就不同，acl 用 float naive 是"独立参考"方向，只需把 float 换 high-precision 收窄容差即可。
6. network 可达 github；cmake 4.4.3；scons 缺、pip 超时(离线)——**必须走 CMake 构建路径**（钉死 v23.02+，不要踩 scons 依赖）。
7. ACL 源码 tarball ~30–45MB（< 已入库的 openssl-3.5.0 53MB），进 git 可行；无 git-lfs。

---

### Task 1: vendor ACL 源码 + build.sh + meson install/-probe 改写 ✅ done (2026-09-19)

**一个 commit**。产出 `third-party/acl/{ComputeLibrary-*.tar.gz, build.sh}` + `tests/cpu/arithmetic_arm/meson.build` 改写。

1. `init-session`/clone：`git clone --depth 1 --branch v23.02 https://github.com/ARM-software/ComputeLibrary.git` 到临时目录，确认 CMakeLists.txt 存在、导出 `libarm_compute-core.a`（v23.02 已拆分 core/graph，测试只用到 runtime/NEON = core）。
2. **API 兼容核对**（决定版本能否落地）：在克隆的源码里 grep 测试用到的符号，确认全部存在：
   `grep -rnE "class NEGEMM|class NECast|class NEScheduler|set_num_threads|class Tensor|class TensorInfo|import_memory|ConvertPolicy" arm_compute/`。任一缺失 → 换版本回退，记录。
3. 打包源码 tarball 存入 `third-party/acl/`（provenance），写 `build.sh`：
   - CMake 关键参数：`-DARM_COMPUTE_ENABLE_SVE=0`（TSV110/鲲鹏920 无 SVE，防 illegal instruction）、`-DARM_COMPUTE_OPENMP=0`（单线程，测试里 NEScheduler set_num_threads(1)）、`-DBUILD_SHARED_LIBS=OFF`、`-DCMAKE_BUILD_TYPE=Release`、`-DARM_COMPUTE_BUILD_EXAMPLES=OFF`、`-DARM_COMPUTE_BUILD_TESTING=OFF`、`-DCMAKE_INSTALL_PREFIX=install`。
   - 幂等（`install/lib/libarm_compute-core.a` 已存在则 exit 0，与 openssl 同）。
4. meson 改写 `arithmetic_arm/meson.build`：删 `cc.find_library('arm_compute', dirs:['/usr/lib64'])`、`acl_incdir`、`has_header` 三段，换成 install/-probe：`[ -f third-party/acl/install/lib/libarm_compute-core.a ]` 命中则 `declare_dependency(include install/include, link_args '-l:libarm_compute-core.a' + 依赖库 pthread/dl)`；缺省 `acl_sources=[]` + message。**保留 `enable_acl` option 语义**(enabled 时缺 install/ 报错而非静默降级，auto 才降级)，与 openblas/sleef 的 auto 降级对齐但保留 enabled 的硬报错。
5. **验证**：
   - `bash third-party/acl/build.sh` → `install/lib/libarm_compute-core.a` 真实生成（引用 `ls -la` 输出）。
   - `PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` + `ninja -C builddir` 零新错误。
   - `./builddir/sdcshield --list-tests | grep -E "acl_gemm|fisttp_arm"` 两项都列出。
   - `nm builddir/sdcshield | grep -c '_test_acl_gemm'` ≥1（符号在二进制里）。
- [x] 版本 API 兼容 grep 全通过(有输出)
- [x] build.sh 幂等且产出 install/lib/libarm_compute-core.a(实测 ls)
- [x] ninja 零新错误
- [x] --list-tests 列出 acl_gemm + fisttp_arm

---

### Task 2: 修 acl_gemm fork-safety + 去除 EXIT_SKIP + 收窄容差 ✅ done (2026-09-19)

**一个 commit**。只改 `tests/cpu/arithmetic_arm/acl_gemm.cpp`。

1. 删 init:`59` 与 run:`109` 两处 `return EXIT_SKIP;` 及其注释。
2. fork-safety：仿 fisttp_arm，`acl_gemm_data` 里 a/b/d 三个 tensor 加三套 `alignas(128) float[64*64]` per-thread 缓冲，`allocator()->init(info)` 后改 `allocator()->import_memory(...)`，去掉 `allocator()->allocate()`。golden 仍在 init 单线程算一次(naive 参考)。
3. 容差：`naive_gemm` 改 **long double** 累加(golden 独立参考)；run 比较把 `diff > 1e-3f` 改成相对容差：`diff > 1e-5f * fmaxf(1e-3f, fabsf(golden[i]))`（>1e3 ulp 才 fail，抓到一致/大错，放过 tiling 顺序引起的正常 ulp 差）。
4. run 里 `if (cpu != 0) return EXIT_SUCCESS;` 保持（NEGEMM 单 op 单线程执行，避免并发 scheduler 状态竞争——注释已论证）。
5. **验证**（真实运行）：
   - `ninja -C builddir` 零新错误。
   - `./builddir/sdcshield -e acl_gemm -t 5000 -n 1` → `exit: pass`（首次真跑 NEGEMM，不再是 skip）。
   - `./builddir/sdcshield -e fisttp_arm -t 5000 -n 1` → `exit: pass`（回归：改过的共享 struct 未破坏活的 fisttp）。
   - 回归 `./builddir/sdcshield -e zstd19 -t 3000 -n 1` → pass。
- [x] acl_gemm 真实 `exit: pass`(引用输出)
- [x] fisttp_arm 仍 pass(回归)
- [x] zstd19 pass(非回归)

---

### Task 3: 容器内 ACL 原生编译 + 去掉 sed/挂载 hack + 补 cmake ✅ done (2026-09-19)

**一个 commit**。改 `scripts/offline-build/`（container-build.sh + supplement-20.03-gcc10.sh + download-deps.sh）。

1. 删除 ACL 头挂载(`ACL_HDR=...`、`-Dacl_incdir=$ACL_HDR`)、22.03/20.03 的 `-Denable_acl=disabled` sed 相关行。
2. 容器内 ACL 原生编译（对齐 openssl/openblas 的 `.glibc-build-tag`→重建）：host 的 ACL install/ 与容器 glibc 不同时，用 ACL 源码 + 容器内工具链重编。
   - 24.03：容器 cmake + gcc 12.3 直接编。
   - 22.03：容器无 cmake → 补 cmake≥3.13 noarch（同 supplement 补 meson 的机制）。
   - 20.03：补 cmake noarch + 用 `gcc-toolset-10`（`/opt/openEuler/gcc-toolset-10/root/`）而非系统 gcc 7.3 编 ACL；若 scl 路径未设，在容器内 `source /opt/openEuler/gcc-toolset-10/enable` 后再 cmake。
3. `supplement-20.03-gcc10.sh` / `download-deps.sh` 增加 cmake 包供给；`container-build.sh` 移除 `enable_acl=disabled` 相关分支，改为按"cmake + gcc 是否达标"决定编/降级。
4. 文档注释同步：明确"ACL 三 OS 都编入，20.03 用 gcc-toolset-10，22.03/20.03 需补 cmake noarch"。
5. **验证**：
   - `bash -n` 三个脚本通过。
   - grep 确认 `ACL_HDR`/`acl_incdir`/`enable_acl=disabled` 三处 hack 已消失。
   - host `ninja -C builddir` 零新错误。
   - **20.03/22.03/24.03 三容器各一次 ACL 原生编译冒烟**（若本机 podman 可用），`--list-tests | grep acl_gemm` 三处都命中（引用实测输出）。
- [x] bash -n 通过
- [x] ACL hack grep 清零
- [x] 三 OS 容器: 本机无 podman — supplement-cmake.sh 真实下载 5 RPM + 免 root 解包实测 cmake 3.22.0 可运行可 configure; 容器端到端冒烟留待有 podman 的环境(commit 9a51d4ee 记录)
- [x] host ninja 零新错误

---

### Task 4: pocketfft 逆变换加 round-trip 容差校验 ✅ done (2026-09-19)

**一个 commit**。只改 `tests/cpu/pocketfft/fft.cpp`。

1. run 循环里 cfft_backward(work, 1/n) 之后：结果的输入是 golden_spec 的**副本**(无需重算正向)，backward 输出应 ≈ 原始 `d->input`。加 `for i: if |work[i] - d->input[i]| > tol_i → report_fail_msg`。
2. 容差选取(依据 fft.cpp:17 已实测逆/正向非位精确 ~3e-11)：`tol_i = 1e-6 * fmax(1e-12, fabs(d->input[i]))`——相对 1e-6(>4 个数量级余量)，绝对下限 1e-12 防零附近误报。只揪"整段崩"大错(butterfly 失效、旋转因子表错=O(1)误差)，1-ulp 瞬态交给正向 byte-exact。
3. rfft_backward 同法：输出 vs `d->rinput`。
4. **注意**：cfft_backward(work) 会**改掉 work**，正向 golden 比较在 backward **之前**做(现有顺序已如此，勿调换)；backward 校验用退化的 work vs input。
5. **验证**：
   - `ninja -C builddir` 零新错误。
   - `./builddir/sdcshield -e pocketfft_fft -t 5000 -n 1` → `exit: pass`（容差足够宽，健康硅不误报——引用输出）。
   - 临时把容差打 1000 倍自证能抓错(可选，若做则 quote 实测 FAIL 后撤回)。
- [x] pocketfft_fft `exit: pass`(引用输出)
- [x] 容差自证(可选)已记录

---

### Task 5: 文档同步 ✅ done (2026-09-19)

**一个 commit**。CLAUDE.md vendored-lib 清单加 ACL 条目 + README 第三方库节 + `docs/SDC_LIBRARY_STRESS_COVERAGE_AUDIT_2026-09-18.md` 的 ACL/pocketfft 两行结论更新为"已修复"。

- [x] CLAUDE.md / README / audit 报告三处一致

## Decisions Made

| Decision | Rationale |
|----------|-----------|
| vendor 用 tarball+build.sh(非 eigen5 式直提源码) | ACL 是大库需编译，对齐 openssl/openblas 模式 |
| 默认钉 v23.02(首个纯 CMake) | scons 缺失且 pip 离线，必须 CMake；v23.02 最贴近测试所验证的 v22.11 API |
| **单一 v23.02 三 OS 全覆盖(不降级)** | 实证 v23.02 只需 GCC≥10.2；20.03 经 supplement 补 gcc-toolset-10(10.3)、22.03 自带 10.3、24.03 自带 12.3，三者都达标 |
| 20.03/22.03 补 cmake≥3.13 noarch | 容器无 cmake(仅 24.03 有)，对齐补 meson 0.59 的既有机制 |
| 20.03 用 gcc-toolset-10 编 ACL | 系统 gcc 7.3 < 10.2 下限，须走 supplement 已备好的 SCL 工具链 |
| acl_gemm 走 import_memory 而非换 fork-mode | fisttp_arm 已验证该模式，改动最小 |
| pocketfft 加容差而非删逆变换 | 逆变换是真实反向数据通路压测，删掉缩小压测面 |

## Errors Encountered
| Error | Attempt | Resolution |
|-------|---------|------------|
| (待 Task 1 实跑填充) | | |
# SDCShield

SDCShield is an open-source project designed to identify defects and bugs in ARM CPUs derived from Intel's OpenDCDiag (ARM64 port). It consists of a set of tests built around a sophisticated CPU testing framework.

## Origin

SDCShield is derived from OpenDCDiag (which contains the Intel `sandstone` framework), licensed under the [Apache License 2.0](LICENSE). See [NOTICE](NOTICE) for full derivation and upstream provenance.

## 快速开始

两条入口，按需选一。

### 直接运行预构建二进制（不想编译）

`third-party/rpms/` 聚合了三个 git 子模块，按 openEuler 大版本分仓，覆盖各 LTS/SP 版本的预构建二进制：

| 子模块目录 | 覆盖版本 |
|---|---|
| [`third-party/rpms/openEuler-20.03/`](https://github.com/wangxumarshall/sdcshield-rpm-20.03.git) | 20.03 LTS / SP1–SP4 |
| [`third-party/rpms/openEuler-22.03/`](https://github.com/wangxumarshall/sdcshield-rpm-22.03.git) | 22.03 LTS / SP1–SP4 |
| [`third-party/rpms/openEuler-24.03/`](https://github.com/wangxumarshall/sdcshield-rpm-24.03.git) | 24.03 LTS / SP1–SP4（SP3 = 基准版本） |

```bash
git clone --recurse-submodules <repo-url>   # 含子模块
cd third-party/rpms/openEuler-24.03/openEuler-24.03LTS_SP3/built
./run-sdcshield.sh --list-tests            # 自动设 LD_LIBRARY_PATH=./libs
./run-sdcshield.sh -T forever -t 60s -Y -F                # 首次检测到SDC后，停止 
./run-sdcshield.sh -T forever -t 60s -Y -ignore-timeout   # 一起跑，就算检测到SDC后，也一直往后跑
./run-sdcshield.sh -T forever -t 600s -Y -e "eigen_svd*" -e "eigen_gemm*" -e "fma*" -e "zstd*" -e "zlib*" 
./run-sdcshield.sh -t 60s -n 1 -e zstd19 # 单线程，规避大核数 ULP 数值 flakiness
```

> **SP 必须与目标机一致**：SP3 的 `glibc-devel` 携带 `Requires: glibc = <sp3-N>`，装到 SP4 会触发受保护 `glibc` 降级死结。`install-deps.sh` 通过 `.os-version` 标记在安装前拦截错配。

**从 CI 下载预构建二进制（无需 clone 子模块）**：每次 [Multi-OS Verify](.github/workflows/multi-os-verify.yml) CI 运行（每日 cron + 手动触发）都为 15 个 SP 各产出一个自包含 tarball（`built-<series>-<sp>` artifact，保留 90 天）——仓库 → Actions → 任意运行 → Artifacts 下载，或 `gh run download <run-id> --name built-24.03-SP3`。解包后 `./run-sdcshield.sh` 用法同上。要**永久保存**某次构建：手动触发时勾选 `publish_release`，15 个 tarball 会作为 GitHub Release 资产上传（详见 [docs/multi-version-build-deploy.md §6.2.1](docs/multi-version-build-deploy.md)）。

### 从源码构建

```bash
# openEuler 24.03（基准平台）依赖；Ubuntu/Fedora 见 docs/offline-build-dependencies.md
sudo dnf install -y meson ninja-build gcc g++ cmake boost-devel zlib-devel libzstd-devel libisa-l-devel gtest-devel
# 首次构建前：依次构建 5 个 vendored 依赖库（幂等，install/ 已存在则秒过；
# pocketfft 无需此步——头文件+源码直接编入测试库）
./third-party/openssl/build.sh           # → install/lib/libcrypto.a（SSL 测试默认启用）
./third-party/openblas/build.sh          # → install/lib/libopenblas.a（openblas_{d,s,z,c}gemm + openblas_lu）
./third-party/sleef/build.sh             # → install/lib/libsleef.a（sleef_neon / sleef_sve）
./third-party/isa-l/build.sh             # → install/lib/libisal.a（isal_igzip + isal_crc*；缺省回退系统 libisal）
./third-party/acl/build.sh               # → install/lib/libarm_compute-core.a（acl_gemm NEGEMM + fisttp_arm；NEON runtime core 目标，v23.02）
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
ninja -C builddir
./builddir/sdcshield --list-tests        # 应列出 282 个 PROD 用例（实测于 2026-09-17）
```

> ARM64 要求 Eigen 5.0.0+（系统 Eigen 3.3.x 在 GCC 12+ 下编译失败）。仓库自带 `third-party/eigen5/`，aarch64 构建路径在 `tests/cpu/meson.build` 中直接 `include_directories` 指向它，**无需系统安装 eigen3**；`PKG_CONFIG_PATH` 仅为兼容 x86 路径而保留，带上无害。
>
> 四个 `build.sh` 均为可选步骤：任一 `install/` 缺失时 meson 打印提示并跳过对应测试组或回退系统库（不阻断构建、不影响其余用例；isa-l 缺失时回退系统 `libisal`，两者皆无才剔除 `isal_*` 测试）。vendored 库的选型依据（向量 FMA GEMM > 哈希/加密 > 压缩 > SVE 超越函数）见 [docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md](docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md)（31 篇 SDC 文献综合）。



## 离线与多版本构建

离线工具在 `scripts/offline-build/`。单机原生路径（`download-deps.sh`/`install-deps.sh`/`build.sh`，共享 `_common.sh`，用 `.os-version` 标记强制版本匹配）只支持 **24.03**——`_common.sh` 基线硬编码 `24.03LTS`，`detect_os_version_full` 对 22.03/20.03 会误报。22.03/20.03 必须走容器路径（D）。

```bash
cd scripts/offline-build/
# A. 下载 RPM 树（有网机，与目标机同 SP）
./download-deps.sh                     # 当前 SP 一版 → ./sdcshield-rpms/
./download-all-versions.sh             # 全 LTS/SP，对 24.03 SP3 基准取同名包集交集
./supplement-20.03-gcc10.sh all         # 20.03 专用：补 gcc-10 工具集 + meson 0.59（20.03 自带 gcc-7）
# B. 离线装依赖（目标机，无网）→ 传对应 SP 的 RPM 目录，版本会被核对
./install-deps.sh ../../third-party/rpms/openEuler-24.03/openEuler-24.03LTS_SP3
# C. 目标机原生构建（依赖已装，仅 24.03）
./build.sh                              # meson + ninja + 冒烟（zstd19 -n 1）
# D. 按版本容器构建（免主机安装；源码只读挂载）—— 22.03/20.03 必走此路
./container-build.sh 24.03 SP3          # → build-out/openEuler-24.03LTS_SP3/sdcshield
./container-build.sh 22.03 SP3          # 注入 -DOPENEULER_22_03 + C++23 polyfill
./container-build.sh 20.03 SP4          # gcc-toolset-10 + 仓内 vendored meson 0.59.4
# E. 打包到对应版本 built/ 目录
./package-built-artifacts.sh 24.03 SP3
# F. 纯净容器验证（仅挂 built/，验证可直接运行）
./verify-built-pristine.sh 24.03 SP3        # 全量；eigen 类 -n 1
./verify-built-pristine.sh 24.03 SP3 smoke  # 快：--list-tests + zstd19
# G. 构建容器内跑全量用例
./run-full-tests.sh 24.03 SP3 [build]
```

> **22.03 / 20.03 适配仅在容器内进行**：旧工具链（gcc-10、meson 0.59/0.54、binutils 2.34）缺 C++20/23 特性。`container-build.sh` 只读挂载源码树，在可丢弃副本上注入 `framework/compat/cpp23_polyfill.h`、版本宏（`OPENEULER_22_03`/`OPENEULER_20_03`）和 `-D` meson option（`-Denable_acl=disabled`）——host 源码不动，x86/24.03 参考路径不受影响。历史上的 `sed` 补丁（`string::contains`→`.find()`、`udf`→`.inst`、ACL 置空、`meson_version` 放宽）**已全部收敛到源码/meson option**，容器内对源码副本零修改，只剩 CXXFLAGS/polyfill 注入。

完整依赖与排坑见 [docs/offline-build-dependencies.md](docs/offline-build-dependencies.md)。

### 多版本一键构建与部署（15 个 OS 版本）

上面 D→E→F 的单 SP 手动链，由 `build-all.sh` 自动化封装（含镜像烘焙、哈希 skip、`-P` 并行、`--since`、smoke/full 五档）。要一次性构建全 15 个版本（20.03/22.03/24.03 × LTS+SP1~SP4）并发布现场包：

```console
# 克隆（含 RPM 依赖子模块）→ 切到方案分支
git clone --recurse-submodules https://github.com/wangxumarshall/sdcshield.git
cd sdcshield && git checkout feat/multi-version-build-deploy
# 全 15 版本构建 + 纯净验证（发布闸门）→ 各 SP 产物进 built/
./scripts/offline-build/build-all.sh --all --full --jobs 3   # 期望 PASS: 15
# 产出现场 tarball（~6MB，含自动检测 OS 的 run.sh）
./scripts/offline-build/package-release.sh 24.03 SP3
# 目标机：解压后 ./run.sh -e zstd19 -t 2000 -n 1   # run.sh 自动精确匹配 OS 版本，不匹配则硬停指路
#          ./run.sh full -t 120s                    # 全核满载 eigen 运算（两段式，详见下注）
```

> `run.sh` 部署逻辑：检测本机 OS → 精确匹配 `built-index.tsv`（不跨版本回退）→ 校验 `binary-sha256` → `exec run-sdcshield.sh`（设 `LD_LIBRARY_PATH` 指向随包 `libs/`）。
>
> `run-sdcshield.sh full`（首参数 `full`）：两段式全核 eigen 满载——第一段 11 个稳定 eigen 测试不带 `-n`（默认使用系统全部 CPU）；第二段 4 个数值敏感测试（`eigen_svd_double`/`eigen_sparse`/`eigen_svd_cdouble`/`eigen_svd_cdouble_sve`）以 `-n 1` 补跑，规避大规模多线程下的 ULP 级偶发假 FAIL（平台已知特性）。默认每测试 60s（可 `-t` 覆盖）；透传的 `-n` 只作用于第一段；`eigen_svd_cdouble_sve` 自 2026-09-19 起为真实 SVE 向量化压测（vendored Eigen 已补 `double`/`complex<double>` packet，本机 VL=256 上 300×300 复数 BDCSVD ~0.7 s），无 SVE 的机器上报 `CpuNotSupported`。

#### 一键式全流程：`release-all.sh`（agent/CI 首选入口）

**一条命令完成全部工作**：base 镜像拉取（docker hub 加速器拉取→podman load→tag 为 quay.io 命名，绕过 quay 直连挂起）→ 15 版本镜像烘焙 → 容器内构建 → 打包 → full 纯净验证 → 提交推送（3 个 RPM submodule + 主仓）：

```console
./scripts/offline-build/release-all.sh              # 全流程（唯一需要记住的命令）
./scripts/offline-build/release-all.sh --skip-base  # base 镜像已齐时（推荐，省去 docker 桥接）
./scripts/offline-build/release-all.sh --help       # 全部选项
```

五个阶段（幂等，已就位自动 skip；任何阶段失败即停，退出码非零）：

| 阶段 | 内容 | 通过标准 |
|---|---|---|
| 1 preflight | 分支/submodule/RPM 树自检 | 在 `main` 或 detached HEAD 上**拒绝**运行（CLAUDE.md 禁止推 main） |
| 2 base | 15 个 `quay.io/openeuler/openeuler:*-lts[-spN]` 缺失则补齐 | `podman images` 核验 15/15 |
| 3 build | `build-all.sh --all --full`（哈希 skip：输入未变的 SP 自动复用） | 退出码 0 |
| 4 check | 日志硬闸 | **15 × `RESULT: PASS` 且 0 FAIL**，否则绝不进入推送 |
| 5 push | 3 个 submodule `built/` 产物（快进校验后推 `origin/main`）+ 主仓指针/manifest（推当前特性分支） | 全部推送成功 |

**给 agent 的调用要点**：
- 耗时约 2~3 小时（15 × 构建+full 验证），建议后台运行 + 定期轮询 `build-out/release-all-build.latest.log` 中的 `RESULT:` 行。
- 全程日志：`build-out/release-all-<时间戳>.log`（gitignored）；完成后核对末尾 `release-all 完成`。
- stage 2 拉 base 需要 root docker（sudo 交互提示）；已有全部镜像时该阶段秒级 skip，无需 sudo。
- 提交信息由脚本自动生成（含当日日期与验证结论），无需人工干预。

> 2026-09-02/03 全链路实测：15 × `RESULT: PASS`（fail=0, crashes=0, eigen_fails=[]），stage 5 自动完成 3 个 submodule + 主仓的提交与推送。

完整设计与操作指南见 [docs/multi-version-build-deploy.md](docs/multi-version-build-deploy.md)（设计方案）与 [docs/multi-version-build-deploy-retrospective.md](docs/multi-version-build-deploy-retrospective.md)（一键复现指南 + 15 SP 全 full 验证基线 + 复现 gotcha）。快速开始速查见 [scripts/offline-build/README.md](scripts/offline-build/README.md)。

### GitHub Actions 每日多 OS 自动验证（`.github/workflows/multi-os-verify.yml`）

上面的 `release-all.sh` 是**发布链路**（本地/agent 一键）。仓库另配了一条 **CI 哨兵**：`.github/workflows/multi-os-verify.yml`，用 GitHub Actions **原生 `container:` 属性**（作业直接运行在 ghcr.io 构建镜像里，代码由 checkout 自动挂载，最简洁高效），每天 **UTC 04:00 = 北京时间 12:00** 自动触发（也可手动 `workflow_dispatch`）：

- **15 个镜像 × 15 个 job 并行**（`fail-fast: false`，互不拖累，全跑完出结论）：openEuler 20.03 / 22.03 / 24.03 × LTS+SP1~SP4。
- **每 job**：`actions/checkout`（`submodules: false`，镜像已烘焙依赖）→ `actions/cache` 缓存 vendored 库构建 → 镜像内 `meson+ninja` 构建原生二进制 → `scripts/gha/verify-params.py` 做**全量用例的全量参数**功能测试 → `scripts/gha/benchmark.sh` 采跨 OS 基准 → 上传日志/基准。
- **全量参数扫描**（`verify-params.py`，纯 stdlib 适配容器无 PyYAML）：`--quality=-1` 覆盖 PROD+BETA+SKIP；`-n 1/4/8` 三档并发；openblas `mdim` 扫谱；selftests `@positive` + 逐条负面 selftest（断言非零退出且非 insn 崩溃）。
- **基准对比**：固定 `--max-test-loop-count`（同工作量墙钟）对三系列交集的 11 个测试采 `benchmark.tsv`，`report` job 汇总成一张跨 OS 对比表写入 job summary。
- **运行器**：`ubuntu-24.04-arm`（GitHub hosted aarch64，GA）；换自建 kunpeng920 runner 改一行 `runs-on` 即可。
- **前置**：15 个镜像需先 `./scripts/offline-build/images/build-images.sh <series> <sp> --push` 推到 `ghcr.io/wangxumarshall/sdcshield-offline`（manifest `remote=yes`）。

详见 [docs/multi-version-build-deploy.md](docs/multi-version-build-deploy.md) 的「GitHub Actions 每日多 OS 验证」章节与 [scripts/gha/README.md](scripts/gha/README.md)。

### PR CI 门禁、安全扫描与基准趋势

PR（`pr.yaml`，arm64-only）：lint（tabs/codespell/actionlint）→ git 历史/DCO → `build-cpu`（GCC-arm64：构建 + unittests + selftests + quick 全量跑，quick 结果以「用例 × 结果[耗时]」矩阵写入 PR 页 summary）→ `multi-version` 角落三（20.03-LTS/22.03-SP3/24.03-SP3，源码/适配层改动才跑，docs-only PR 自动跳过）。同 PR 推新 commit 自动取消旧 run；所有 job 有超时上限。

安全（`security.yaml` + `codeql.yaml`，每周一全量 + PR 增量）：zizmor（workflow 自身安全，噪声策略见 `zizmor.yml`）、gitleaks（全历史凭据扫描）、osv-scanner（vendored 依赖 CVE）、CodeQL cpp（x86 构建，排除 `third-party/`；托管 arm 裸 VM 机群曾随机关机 job，aarch64 专属测试不进分析库、其编译覆盖由每日 multi-os-verify 承担）。全部 checkout `persist-credentials: false`；Dependabot 管 action 版本（分组 + 7 天冷却）。

基准趋势：nightly 的 24.03-LTS-SP3 `benchmark.tsv` 由 `benchmark-trend` job 转换后经 github-action-benchmark 推到 gh-pages 分支，出跨 commit 趋势图 + 150% 回归告警（观测期不硬门）。网页侧操作清单（原生失败通知、分支保护、Code scanning 确认）见 [docs/build-deploy/ci-web-operations.md](docs/build-deploy/ci-web-operations.md)。

### 15 镜像全严格选项稳定性验证（`scripts/lts-stability/`）

本地 podman 上的**最严格**验证入口（GHA 日报的本地超集,29 条目选项矩阵 × 全部测试用例,含 `--quality=-1` SKIP 级、三 RNG 引擎、cpuset 跨 NUMA、全部 `-O` 测试旋钮、selftests、`--on-crash=context` 等）:

```console
./scripts/lts-stability/run-all-15.sh            # 15 镜像 × 29 条目(3 系列并行,~5h)
./scripts/lts-stability/run-all-15.sh --smoke    # 链路快检(每镜像只跑 m01 基线)
./scripts/lts-stability/run-lts-stability.sh 24.03 SP3   # 单镜像全矩阵
```

每镜像流程:容器内原生构建全功能二进制（`-Dssl_link_type=static` + vendored openssl/openblas/isa-l,glibc 不匹配时容器内重建）→ 29 条目严格矩阵 → `RESULT: PASS|FAIL <tag>`。全部 15 PASS 才 exit 0。2026-09-17 全链路实测 **15/15 PASS**(main `ebe0bf1`,290 测试集),过程中发现并修复 8 个跨版本软件 bug(详见 [docs/cases/lts-stability-2026-09-17/README.md](docs/cases/lts-stability-2026-09-17/README.md))。



## OpenSSL SHA（`openssl_sha`，默认构建）

`openssl_sha` 经 OpenSSL 计算 SHA-256/384/512 与 golden 比对。默认 `ssl_link_type=dynamic`，优先使用 vendored OpenSSL（`third-party/openssl/`，需先执行 `./third-party/openssl/build.sh` 构建），若 `install/` 缺失则回退系统 `libcrypto`，两者都不可用时 meson 打印提示并跳过 SSL 测试。

| `ssl_link_type` | 行为 |
|---|---|
| `dynamic`（默认）| 优先 vendored OpenSSL，回退系统 `libcrypto`，构建期动态链接 |
| `static` | 同上，链接静态 `libcrypto` |
| `loaded` | 运行期 `dlopen()` 加载 `libcrypto` |
| `none` | 禁用 OpenSSL；无 `openssl_sha` |

```bash
# 若 third-party/openssl/install/ 已存在则无需手动构建 vendored OpenSSL，
# 否则需先执行：
./third-party/openssl/build.sh
# 仅当回退系统库且未装 openssl-devel 时才需要：
sudo dnf install -y openssl-devel
PKG_CONFIG_PATH=./third-party/eigen5 meson setup --reconfigure builddir --buildtype=release
ninja -C builddir && ./builddir/sdcshield --list-tests | grep openssl_sha
```

## Vendored 计算库（third-party/，SDC 负载扩展）

自 2026-09-15 起，仓库 vendor 了高优化度计算库作为 SDC 压测负载（2026-09-16 增补 isa-l）——文献结论（见 `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`，31 篇论文综合）：高度优化的第三方库等价于人工写好的数据流型 FU 饱和序列，其中向量 FMA/GEMM 是第一大 SDC 源（SEVI：>92% SDC 事故由 FMA 指令贡献）。每个目录保留上游官方 tag tarball 以溯源，`build.sh` 幂等构建出静态归档到 `install/`（gitignore；meson 探测 `install/`，缺失时打提示跳过对应测试或回退系统库，不阻断构建）。

| 目录 | 内容 | 产出 | 测试 |
|---|---|---|---|
| `third-party/openssl/` | OpenSSL 3.5.0 | 静态 `libcrypto.a` | `openssl_sha`（SHA-2）+ `openssl_sha3`（SHA-3/SHAKE Keccak）+ `openssl_sm3sm4`（国密 SM3 摘要 + SM4-CBC 加解密往返）+ 46 个 `ipsec_*`（默认启用） |
| `third-party/openblas/` | OpenBLAS 0.3.29（TSV110 内核，单线程 + USE_LOCKING=1） | 静态 `libopenblas.a` | `openblas_dgemm` / `openblas_sgemm` / `openblas_zgemm` / `openblas_cgemm`（NEON FMA 微内核，每迭代 copy/compute/verify）+ `openblas_lu`（LAPACK dgesv 主元分解） |
| `third-party/sleef/` | SLEEF 3.9.0（TLFLOAT=OFF） | 静态 `libsleef.a` | `sleef_neon`（NEON 多项式 FMA 链：sin/cos/exp/log double+float × u10/u35 双精度档 + asin/atan/cbrt/log10/sinh/tanh/pow 函数族，足迹旋钮 `-O sleef_neon.nelems=N`）；`sleef_sve`（SVE 变体，double+float × u10/u35，**需 SVE 硬件**——无 SVE 的机器如鲲鹏 920 干净 skip：`CpuNotSupported`） |
| `third-party/pocketfft/` | pocketfft C 版（BSD-3，头文件+源码直接入库，无需 build.sh） | 编入测试库 | `pocketfft_fft`（位反转抽取重排 + 旋转因子 FMA 蝶形链） |
| `third-party/isa-l/` | Intel isa-l 2.32.1（BSD-3，Makefile.unx 路径，aarch64 无需 autoconf/nasm） | 静态 `libisal.a` | `isal_igzip`（igzip deflate/inflate 往返，aarch64 汇编内核）；10 个 `isal_crc*`（NEON pmull CRC16/32/64）。`install/` 缺失时**回退系统 `libisal`**（openssl 式两级 gate） |

OpenBLAS 单线程（`USE_THREAD=0`）是刻意设计：库内多线程会引入非确定性归约顺序（假阳性）；`USE_LOCKING=1` 仅为让框架"每核一 worker 线程"并发调用 `cblas_*gemm` 时内部 packing 缓冲池不互相踩踏（实证：无锁 8 线程×10s 出 62 字节错配，加锁后 128 核（本机全核）30s 压力零错配，见 `docs/cases/allcore-2026-09-15/`），锁只保护缓冲表元数据、不改计算结果。

isa-l（`isal_igzip` + 10 个 `isal_crc*`）自 2026-09-16 起 vendor 到 `third-party/isa-l/`（v2.32.1，静态 `libisal.a`）；`install/` 缺失时回退系统 `libisal`，两者皆无才剔除 `isal_*` 测试。

```bash
# 首次构建顺序（pocketfft 无需预构建）：
./third-party/openssl/build.sh && ./third-party/openblas/build.sh && ./third-party/sleef/build.sh && ./third-party/isa-l/build.sh
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release && ninja -C builddir
# 全核 30s 压测终验（实测 8806/8806 迭代全 pass，2026-09-15，鲲鹏 920 128 核）：
./builddir/sdcshield -e openblas_dgemm,openblas_sgemm,openblas_zgemm,sleef_neon,isal_igzip,pocketfft_fft,openssl_sha -t 30000
```

**GEMM 矩阵尺寸扫谱（test knob）**：四个 `openblas_*gemm` 测试（double/float/complex-double/complex-float）的矩阵尺寸**默认 256**（L2 域；不传 knob 时行为与历史版本完全一致），可经 test knob 扫谱：`-O <testid>.mdim=N`，N∈[16,4096]，越界 fail-loudly 报错并提示合法域。**必须带测试 ID 前缀**——框架的 TestKeyWrapper 以 `<testid>.<key>` 查找（`framework/test_knobs.cpp`），裸 `mdim=N` 会被静默忽略、回退默认 256（用 `-v` 可在日志中核对每个 knob 的实际生效值）。

**GEMM 形态扫谱（test knob）**：`-O <testid>.transab=0..3`（0=NN 默认、1=NT、2=TN、3=TT——TransA/TransB 在 OpenBLAS 内部走不同 packing 例程）+ `-O <testid>.beta_permille=P`（P∈[0,1000000]，β=P/1000，默认 0；β≠0 命中 C 读改写路径，C 以固定非零模式预填保证确定性）。四个 gemm 测试均支持。

**每测试每档工作集足迹（每线程 scratch = 3 矩阵；矩阵字节数 = mdim² × 元素宽度）**：

| 档位 | dgemm (double) | sgemm (float) | cgemm (complex float) | zgemm (complex double) | cache 域 |
|---|---|---|---|---|---|
| `mdim=64` | 32KB/矩阵 | 16KB | 32KB | 64KB | L1D |
| `mdim=256`（默认） | 512KB | 256KB | 512KB | 1MB | L2 |
| `mdim=512` | 2MB | 1MB | 2MB | 4MB | LLC |
| `mdim=1024` | 8MB | 4MB | 8MB | 16MB | DRAM |
| `mdim=2048` | 32MB | 16MB | 32MB | 64MB | DRAM/远端 NUMA |
| `mdim=4096` | 128MB | 64MB | 128MB | 256MB | 大页/NUMA 交错 |

（cgemm 元素 = 交错 (re,im) float 对 = 8 字节/元素，是 zgemm 的 complex-float 对应物（zgemm = complex double = 16 字节/元素）；矩阵字节数与 dgemm 相同。）

**全核内存预算表（scratch 合计 = 3 矩阵 × 核数；选档前先对照主机内存）**：

| 核数 | dgemm@1024 | dgemm@4096 | sgemm@4096 | cgemm@4096 | zgemm@4096 |
|---|---|---|---|---|---|
| 128 核 | ~3GB | ~49GB | ~25GB | ~49GB | ~98GB |
| 512 核 | ~13GB | ~197GB | ~98GB | ~197GB | ~393GB |
| 2048 核 | ~50GB | ~786GB | ~393GB | ~786GB | ~1.6TB |

（zgemm 内存紧张时推荐档 **512**；1024 档 128 核实测 ~7.7GB。内存不足时 OOM 走 `report_fail_msg` fail-loudly，不是静默或崩溃。）

**推荐战役模式**：

```bash
# 模式 A：谱系扫档（cache 域逐层压测；openblas 单独跑避免混跑 cache 污染）
for m in 64 256 512 1024; do
  ./builddir/sdcshield -O openblas_dgemm.mdim=$m -O openblas_sgemm.mdim=$m \
    -O openblas_zgemm.mdim=$m -e openblas_dgemm,openblas_sgemm,openblas_zgemm -t 15m
done

# 模式 B：混合套件（多样性 = 检出率；默认 256 档，与全部其他测试同跑）
./builddir/sdcshield -e openblas_dgemm,openblas_sgemm,openblas_zgemm,zstd19,sleef_neon,isal_igzip,pocketfft_fft,openssl_sha -t 30m

# 模式 C：大矩阵深压（大内存主机；4096 档单迭代分钟级，需长窗口，必要时 -n 限并发控内存）
./builddir/sdcshield -O openblas_dgemm.mdim=4096 -e openblas_dgemm -t 2h        # 或 -n 64 限制并发

# 模式 E：单命令 knob 全谱（一条命令同时挂多个 -O，每个 -O 只作用于它前缀的测试，
#         无前缀匹配的测试按默认参数跑——多样性轮 + 参数扫谱二合一；9 个测试 12 个 knob）
./builddir/sdcshield \
    -e openblas_dgemm,openblas_sgemm,openblas_zgemm,openblas_lu,sleef_neon,pocketfft_fft,isal_igzip,eigen_svd_cdouble_sve,ipsec_aes128_cbc_hmac_sha1_sse \
    -O ipsec_aes128_cbc_hmac_sha1_sse.datasize=4194304 \
    -O openblas_dgemm.mdim=1024 -O openblas_sgemm.mdim=1024 -O openblas_zgemm.mdim=1024 \
    -O openblas_dgemm.transab=1 -O openblas_sgemm.transab=2 -O openblas_zgemm.transab=3 \
    -O openblas_dgemm.beta_permille=500 \
    -O openblas_lu.n=1024 \
    -O sleef_neon.nelems=16384 \
    -O pocketfft_fft.n=6144 \
    -O isal_igzip.level=2 \
    -t 30m
```

**模式 D：全谱战役脚本 `scripts/run/run_sdc_spectrum.sh`**——把模式 A/B/C 与全部 test knob（mdim/transab/beta_permille/nelems/n/level）合并为一条命令的两阶段战役（§7.4 战役协议的可执行化）：

```bash
bash scripts/run/run_sdc_spectrum.sh                    # 默认：15m/档扫谱 + 2h 深驻留
SWEEP_TIME=30s DWELL_TIME=1m bash scripts/run/run_sdc_spectrum.sh   # 冒烟（约 10 分钟）
```

- **阶段 1 谱系广域扫**（每档独立日志，多样性 = 检出率）：1a GEMM 尺寸谱（mdim 64/256/512/1024，四个 gemm 同跑）；1b GEMM 形态谱（transab 0..3 × beta_permille=500）；1c SLEEF 足迹谱（nelems 1024/16384/262144）；1d FFT 因子谱（n 4096/4099/6144/10000 = pow2/Bluestein 质数/混合 radix）；1e 压缩 level 谱（isal_igzip level 0..3）；1f 加密/哈希全家族（openssl_sha/sha3/sm3sm4 + CRC）+ `openblas_lu` + 混合多样性轮（六种负载同跑）。
- **阶段 2 深驻留**：`--max-test-loop-count=0` 关闭 fracturing，全绿档四负载（dgemm/sleef/pocketfft/isal）固定 seed 持续运行——CORE179 类单模式持续暴露。
- 时长经 env 旋钮覆盖：`SWEEP_TIME`（默认 15m，阶段 1 每档）、`DWELL_TIME`（默认 2h，阶段 2）。日志按阶段/档位分文件存 `./sdc_spectrum_<时间戳>/`，脚本末尾汇总各日志 pass/fail。

模式选择依据：CORE179 探针证明 store→reload 的 cache-domain 与跨线模式是触发判别条件（模式 A 逐层覆盖）；文献共识"负载多样性即检出率"（模式 B 与模式 D 阶段 1，详见 `docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md`）；大矩阵档把分块 GEMM 的 packing/回写路径推进 DRAM 与 NUMA 远端域（模式 C）；固定 seed 长驻留提升单模式的统计采样深度（模式 D 阶段 2）。mdim>256 档在固定时间窗内迭代数按 mdim³ 骤减，是计算密度换覆盖广度的交换——统计采样请加长 `-t`。

**模式 E：单板机器扫描 `scripts/run/sdc_machine_scan.sh`**——压测战役第 0 步，新单板可直接复用：一次性采集 DMI/BMC 传感器/SEL/NUMA 内存位置/RAS/软件栈，产出 `capabilities.env` 能力开关（`NCORES/HAS_SVE/HAS_CPUFREQ/HAS_IPMI/...`）与人读摘要，并自动标记硬件异常（SEL 周期故障、无本地内存 node、无 SVE/cpufreq 等），战役参数据此适配：

```bash
SUDO_PW=<密码> bash scripts/run/sdc_machine_scan.sh          # root 全量模式
bash scripts/run/sdc_machine_scan.sh                         # 无 root 降级模式
```

**模式 F：24h+ 压测执行器 `scripts/run/run_sdc_campaign.sh`**——把模式 D 的两阶段协议扩展为 24h+ 持续测试（PinDrop 模式：持续高频测试比快照式多数量级地抓出缺陷）：P1 全量广域扫（`--quality=0` 全测试 × 全核，fracturing seed 自动轮换）→ P2 文献优先级加权 soak（GEMM 尺寸/形态谱×三调度、crypto、压缩 level 谱、SLEEF 足迹谱、FFT 因子谱、mesh 一致性、混合负载×RNG 引擎）→ P3 固定 seed 深驻留 → P4×N 循环（`--test-list-randomize` 随机序重扫 + NUMA 拓扑/并发档 + 轮换驻留）。全程 ipmitool/EDAC/SEL 环境监测（30s 采样 → monitor.csv），fail-continue + 自动失败分类（known_benign_ulp / full_core_only / sdc_suspect）+ 可复现嫌疑自动逐核二分（CORE179 式定位），`.done` 阶段标记支持断点续跑：

```bash
nohup setsid bash scripts/run/run_sdc_campaign.sh > campaign.log 2>&1 &   # 24h+ 正式
SMOKE=1 bash scripts/run/run_sdc_campaign.sh                            # 冒烟（~15min）
bash scripts/run/run_sdc_campaign.sh --selftest-classify                # 解析器自测
```


## 测试用例与检测能力

当前 ARM64 构建（Kunpeng 920 / openEuler 24.03 SP3，vendored 依赖齐备时）默认 quality 下共 **329 个用例**（`--list-tests` 实测，含 SVE 全向量长家族）。许多用例沿用上游 x86 名字（如 `mesh_upi_avx2_*`、`ipsec_*_avx`、`fma_*_avx512`），但实现已落到 NEON / ARM 原生指令，命名保留是为与 x86 参考用例跨架构比对。

| 检测域 | 代表用例 | 检测能力 |
|---|---|---|
| 内存 / 拷贝 | `memcpy_l{1d,2,3}_cache_size`、`memcpy_rewr`、`mem_disambiguation`、`mmu_stress_arm`、`random_access_sweep` | 各级缓存带宽、store-to-load 转发、跨行一致性与内存序、TLB 扰动、大 mmap 随机偏移/块长扫掠（L1..DRAM 尺度 + NEON RMW 字节精确校验，knob: mode/valmode/size_mb） |
| 缓存 / 互联 | `cachebounce`、`mesh_upi_*`（39，含 12 个 SVE 变体） | cache line 弹跳、CLFLUSH 压力、MESH/UPI 多核读写协同 |
| 锁 / 原子 | `lock*`、`lockless_cmpxchg*`、`atomic_simd_*`、`spinlock_*`（31） | 锁指令、无锁 cmpxchg、128/256/512 位原子、自旋锁各类竞争（ARM64 atomic） |
| 向量 / SIMD | `swizzle`、`insert_extract`、`kreg1`–`kreg9`、`gather*` | NEON 排列/插入抽取、掩码寄存器（x86 k-reg 仿真）、gather/scatter |
| FMA / 浮点 | `fma`、`fma_patterns_*`、`fma_tail*`、`fpu_special_values` | FMA 模式与尾数精度穷举、特殊值逐字节 golden 比对 |
| 算术 / 大整数 | `adcx`、`adox`、`adcxlong`、`adcx_arm`、`bigint_mulx_arm`、`gmp_big*` | 进位/溢出链、GMP 大整数乘加、高汉明距离操作数压满加法器 |
| CRC / 校验 | `crc32`、`isal_crc{32,64}_*`、`zpclmul*` | `crc32` 指令、isa-l CRC32/CRC64 各标准、zlib PCLMUL 折叠 |
| 压缩 | `zlib*`、`zstd*`、`zfuzz`、`isal_igzip` | zlib/zstd 压缩-解压往返、各级别、fuzz、isa-l deflate/inflate 往返（`-O isal_igzip.level=0..3` 扫四套 match-finder 数据结构：静态 Huffman/基础 hash 链/深历史/hash map） |
| 线性代数（Eigen） | `eigen_gemm_*`、`eigen_sparse`、`eigen_svd*`（含 `_cdouble_sve`） | GEMM、稀疏 Cholesky、SVD（BDCSVD/Jacobi）施压 FMA/向量 |
| 线性代数（OpenBLAS） | `openblas_dgemm`、`openblas_sgemm`、`openblas_zgemm`、`openblas_cgemm`、`openblas_lu` | OpenBLAS NEON FMA 微内核 GEMM（double/float/complex-double/complex-float 四种 lane 组织），每迭代 copy/compute/verify；矩阵尺寸 `-O <testid>.mdim=N`（16..4096，默认 256）可扫 L1D→L2→LLC→DRAM→NUMA 工作集；`-O <testid>.transab=0..3` + `.beta_permille` 扫 packing/读改写相位；`openblas_lu` = LAPACK dgesv 主元分支 + 行交换 store + 三重 golden 比对（vendored，见上节） |
| 超越函数（SLEEF） | `sleef_neon`、`sleef_sve` | 多项式 FMA 依赖链 × 宽向量，逐字节 golden 比对；u10 + u35 双精度档（两套独立多项式链）+ asin/atan/cbrt/log10/sinh/tanh/pow 函数族；`sleef_neon` 足迹旋钮 `-O sleef_neon.nelems=N`（128..262144，默认 1024，4 的倍数）扫 L1/L2/LLC 工作集；`sleef_sve` 需 SVE 硬件（无则干净 skip）（vendored，见上节） |
| FFT（pocketfft） | `pocketfft_fft` | 复数 FFT + 实数 rfft 前向 golden 比对：位反转抽取重排（store→load scatter）+ 旋转因子 FMA 蝶形链；`-O pocketfft_fft.n=N` 扫因子谱系（512..16384 pow2 / 4099/8191 质数触发 Bluestein 卷积化 / 6144/10000 混合 radix）（vendored，见上节） |
| IPSec / 密码 | `ipsec_*`（46） | AES-CBC/CTR/GCM、HMAC-SHA1/2、XCBC/CMAC/3DES-DOCSIS 于 NEON |
| OpenSSL SHA | `openssl_sha`、`openssl_sha3`、`openssl_sm3sm4` | SHA-256/384/512（SHA-2 加法链）、SHA-3-224/256/384/512 + SHAKE128 XOF（Keccak 置换 AND/旋转/χθ 步，与 SHA-2 正交的 FU 混合）、SM3 摘要 + SM4-CBC 加解密往返（国密整数通路）vs golden（默认构建，优先 vendored OpenSSL） |
| ARM 加密扩展 | `arm_crypto` | AES（AESE/AESMC）crypto 数据通路 |
| 虚拟化 / 系统寄存器 | `vmx_vmexit_*`、`vmxmsr` | guest 触发 vmexit 退出路径一致性 |
| ARM64 SDC 专项 | `arm64_sdc`、`power_virus_dit`、`ooo_dep_chain_arm`、`lsu_store_forward_arm`、`l2c_cross_cache_line_arm`、`mmu_split_tlb_arm`、`sve512_gather_scatter_arm`、`sve512_f64_chain_arm`、`sve512_f64_special_arm`、`sve512_f32_chain_arm` | di/dt 电压骤降、乱序依赖链、LSU 转发、L2 跨行、MMU/TLB/页表遍历器、SVE 全向量长度 gather/scatter 间接索引数据通路、SVE 全向量长度 f64 FMLA 串行依赖链、SVE f64 特殊值链（NaN/Inf 类别比对）、SVE f32 FMLA 串行依赖链（16-lane f32 数据通路）、SVD 尺度工作集 f64 FMLA 链（L2 溢出 + 16x16 块遍历）、SVD 尺度工作集 f64 特殊值链、SVD 尺度工作集 f32 FMLA 链（16-lane f32 通路）、SVD 尺度工作集 gather/scatter 往返（2-D 块索引置换）、SCF/stencil 轴核触发配方复现器（svdup 系数装载 + RADIUS=6 双向 svmla 链 + VA[63:48] 累加器地址金丝雀） |
| ARM64 触发配方 | `agu_stress_2src`、`neon_rot_2src`、`neon_rot_ldr_at_top_rowmajor`、`movbe` 系列（`movbe`、`movbe_dump`、11 个 `movbe_dump_probe_*`） | AGU 吞吐施压（2 源加载 + 旋转 ALU + store/reload/store）、core-179 配方的 NEON 向量通路判别（uint64x2 旋转 ALU + 向量 store/reload/store）、ldr_at_top 扫描顺序变体（升/降序交替，区分槽位局部 vs 前进位置特征）、core-179 字节交换往返触发探针组 |
| SVE 版 avx53 套件（tests/cpu/sve/，53 个） | `fma_tail_sve{,_wide}`、`fmatail_sve{,_wide}`×4 对、`fma_patterns_sve_wide_{ps,pd}`、`mesh_upi_sve_*` 18 个、`eigen_svd_bidiag_sve`、`ipsec_*_sve{,_wide}` 24 个 | 53 个 avx 命名 NEON 测试的 SVE1 移植（负载环境不变：向量宽度/随机域/特殊值注入/块结构/原子协议/golden 模式全保留；命名 `_avx/_avx2→_sve`、`_avx512→_sve_wide`）。FMA 10 + Mesh 18 = 纯指令替换（SVE 谓词访存）；eigen 1 = 自写 Householder 双对角化 SVE 负载（替代崩溃的 Eigen-SVE 后端路径）；ipsec 17 = EVP 加密 + 多流 SVE HMAC（SHA 内核与 OpenSSL 全向量比对 5200+1360 全对，lane0=原 MAC 语义+变体填 lane），7 个 EVP-only（GCM/CMAC/XCBC）如实标注计算路径。构建于 `tests_sve_avx53`/`tests_sve_avx53_ipsec` 库（`-march=armv8.2-a+sve`，init 探 HWCAP_SVE 干净 skip） |
| IST 硬件自检 | `ist`、`ist_array`、`ist_sbaf` | ARM64 In-Silicon Test（当前 placeholder，见下表） |

### 用例质量分级

| 级别 | 名称 | 运行条件 | 实有数量 |
|---|---|---|---|
| -1 | SKIP | `quality >= -1` | 5 |
| 0 | BETA | `quality >= 0` | 4 |
| 2 | PROD（默认）| `quality >= 2` | 282 |
| | **合计** | | **291** |

- **BETA（`--quality=0`）**：`arm64_sdc`、`arm_crypto`、`ist_sbaf`、`neon_add`
- **SKIP（`--quality=-1`）**：`smi_count`、`eigen_svd_jacobi`、`eigen_svd_jacobi_cdouble`、`eigen_svd_jacobi_double`、`eigen_svd_jacobi_fvectors`

### 占位用例的诚实跳过

暂未实现的特性用例返回 `EXIT_SKIP` 并附理由（而非 `EXIT_SUCCESS` 伪装通过）。实测 `skip-reason`：

| 用例 | 跳过理由 |
|---|---|
| `ist` / `ist_array` / `ist_sbaf` | `to be implemented (placeholder): ARM64 In-Silicon Test (IST) backend not yet available; test reserved as the counterpart of Intel IFS` |
| `smi_count` | `to be implemented (placeholder): SMI counting requires a per-CPU firmware/RAS-interrupt counter not available on this architecture` |

> `mce_check` 是真实 EDAC 后端测试（统计 `/proc/interrupts` 的 EDAC `ce/ue_count`），实测 `exit: pass`，非 placeholder。

### `memcpy_rewr`：MPSC memcpy 一致性压测（使用选项与推荐参数）

GlusterFS IOT 调度器衍生的多生产者/单消费者队列 memcpy 一致性压测（ARM64 专属）：producer 经 4 优先级链表队列投递，consumer 对线程私有 `mem_info_s` 的 b1..b5 做五路 memcpy 并逐字节比对——拷贝不崩溃但不一致即为 SDC。角色分配与拷贝长度由策略配置驱动，无 conf 文件时自动退化成原版独立工具的默认模式（不 skip）。

**conf 模式**（`tests/cpu/memory/memcpy_rewr_strategies.conf`，仓库默认提供）——`SANDSTONE_STRATEGY_INDEX` 轮转选策略，`SANDSTONE_STRATEGY_CONF` 覆盖路径：

```bash
for i in 0 1 2; do SANDSTONE_STRATEGY_INDEX=$i ./builddir/sdcshield -e memcpy_rewr -t 5000; done
```

| INDEX | 策略 | 拓扑意图 | 推荐场景 |
|---|---|---|---|
| 0 | `numa_cross_node`（`numa_split`） | 跨 NUMA 节点 barrage，所有数据移动穿跨 die 一致性 Fabric | 多路/多 NUMA 机器的互联压力 |
| 1 | `same_die_l3_brawl`（`die_even_odd`） | 同 die/cluster 内 even/odd 核对打，一致性流量锁死在单 L3 slice + ring bus | 单 die 内 L3/环网压力 |
| 2 | `few_producer_many_consumer_storm`（`first_n_producers`，`producer_count=4`） | 少写多读，诱发目录控制器 Invalidate 广播风暴 | snoop/目录控制器压力 |

**默认模式**（conf 缺席，如 `SANDSTONE_STRATEGY_CONF=/nonexistent`，或部署机无源码树）——参数按机器自适应（全核并行，`test_schedule_fullsystem`）：

- `block_size = clamp(MemAvailable×50% ÷ 线程数 ÷ 6, 64 KiB, 2 MiB)`（6 = 每线程实际触碰的 buffer 前沿数保守上界；2 MiB = `mem_info_s` 字段宽，即原版 `g_size` 溢出边界的安全上限）
- `producer_count = max(1, CPU核数/12)`（48 核 → 4:44，对齐原版参考参数 p 0..3 / c 0..47）
- 忽略 `SANDSTONE_STRATEGY_INDEX`；conf 存在但损坏/不可读仍然 loud skip（不静默吞错）

**参考实测**（Kunpeng 920，128 核，MemAvailable ≈ 22.5 GB；`-vv` 日志）：

- 默认模式全核：`strategy[0]=default_original block_size=2097152 threads=128 producer_count=10`（自适应封顶 2 MiB），峰值 RSS ≈ 1.52 GiB，无 OOM
- conf 三策略全核（`-t 3000`）：`block_size=65536 threads=128 producer_count=4`，三档均 `result: pass`
- 单跑推荐：`-t 5000` 全核（约 5 s/策略）；策略覆盖推荐 `for i in 0 1 2` 轮转各一轮

## 运行测试

```console
./builddir/sdcshield --list-tests                        # 列 PROD 用例（默认）
./builddir/sdcshield -e zstd19 -t 5000                   # 单测试，5 秒，全核
./builddir/sdcshield -e zstd19 -t 5000 -n 1             # 单线程，规避 192 核 ULP flakiness
./builddir/sdcshield --quality=0 -e arm64_sdc -t 5000   # 跑 BETA 用例
./builddir/sdcshield --quality=-1 -e eigen_svd_jacobi   # 跑 SKIP 用例
./builddir/sdcshield -l                                  # 用例 + 描述 + 分组
./builddir/sdcshield --list-groups                       # @compression / @ipsec / @math
./builddir/sdcshield --dump-cpu-info                     # CPU + 特性 + 拓扑
./builddir/sdcshield --on-crash=context -e selftest_sigsegv -vv   # 崩溃回溯
```

### Test knob（`-O`，运行期测试参数）

部分测试支持**运行期参数旋钮**——不重编译即可改变矩阵尺寸、工作集大小、压缩级别等压力参数，参数值写入 YAML 日志（可复现）。语法：`-O <测试ID>.<参数名>=<值>`（可重复传多个）。

```console
# 默认 256 档（L2 域）→ 4096 档（806MB 工作集，压到 DRAM/NUMA 远端域）
./builddir/sdcshield -e openblas_dgemm -O openblas_dgemm.mdim=4096 -t 30m

# 形态扫谱：转置组合 + β 读改写路径
./builddir/sdcshield -e openblas_zgemm -O openblas_zgemm.transab=3 -O openblas_zgemm.beta_permille=500 -t 15m

# ipsec 载荷 1024B→16MB：AES/3DES/SHA 数据路径从 L1 压到 DRAM（全部 46 个 ipsec 用例同款参数名）
./builddir/sdcshield -e ipsec_aes128_cbc_hmac_sha1_sse -O ipsec_aes128_cbc_hmac_sha1_sse.datasize=16777216 -t 5000

# SLEEF 足迹扩到 6MB（默认 1024 元素 ≈ 128KB）
./builddir/sdcshield -e sleef_neon -O sleef_neon.nelems=262144 -t 30m

# FFT 走 Bluestein 质数路径（默认 4096 是纯 radix-4/2）
./builddir/sdcshield -e pocketfft_fft -O pocketfft_fft.n=4099 -t 15m
```

**两个易错点**：① 必须带测试 ID 前缀——框架以 `<测试ID>.<参数名>` 为查找键（`framework/test_knobs.cpp`），裸 `-O mdim=4096` 会被**静默忽略**并回退默认值（`-v` 可核对每个 knob 的实际生效值）；② 越界值 fail-loudly 报错（不静默钳制），报错信息含合法范围。

当前支持 knob 的测试全集：

| 测试 | 参数 | 合法域 | 默认 | 作用（档位→压力域） |
|---|---|---|---|---|
| `openblas_{d,s,z,c}gemm` | `.mdim` | 16..4096 | 256 | 矩阵尺寸：64→L1 / 256→L2 / 1024→L3 / 2048+→DRAM |
| `openblas_{d,s,z,c}gemm` | `.transab` | 0..3 | 0 | NN/NT/TN/TT（OpenBLAS 不同 packing 例程） |
| `openblas_{d,s,z,c}gemm` | `.beta_permille` | 0..10⁶ | 0 | β=P/1000；β≠0 命中 C 读改写路径 |
| `openblas_lu` | `.n` | 16..2048 | 256 | LU 分解矩阵尺寸（64→32KB … 2048→32MB） |
| `sleef_neon` | `.nelems` | 128..262144（4 的倍数） | 1024 | 每函数族元素数（工作集 128KB→6MB） |
| `pocketfft_fft` | `.n` | 512..16384（pow2）/ 4099 / 6144 / 10000 | 4096 | FFT 点数与因数结构（pow2=radix-4/2，4099=Bluestein 质数） |
| `isal_igzip` | `.level` | 0..3 | 1 | deflate 级别（不同匹配查找器；2/3 有独立 level_buf） |
| `zstd` / `zstd1` / `zstd19` / `zfuzz` | `.level` / `.maxbuffersize` | 1..22 / 4096.. | 按变体 | 压缩级别 / 每迭代缓冲上限 |
| `eigen_svd_cdouble_sve` | `.mdim` | 300..6000 | **按内存自适应** | 强制矩阵维度（覆盖自适应选择） |
| `ipsec_*`（全部 46 个） | `.datasize` | 1024..64MB（16 的倍数） | 1024 | 加密/解密/MAC 载荷尺寸：1024→L1 / 64KB→L2 / 1MB+→L3/DRAM——AES/3DES/SHA 数据路径全谱扫（2026-09-19；默认 1024 与历史逐字节一致） |
| `memcpy_rewr` | 环境变量 `SANDSTONE_STRATEGY_INDEX` / `SANDSTONE_STRATEGY_CONF` | 0..2 / conf 路径 | — | MPSC 策略选择（env 而非 `-O` 机制） |

仍无 knob 的测试（eigen NEON 家族、isal_crc×10、openssl×3 等）参数为编译期常量——参数审查（`docs/research/third-party-sdc-param-critique.md`）记录的已知状态；ipsec×46 已于 2026-09-19 补齐 `datasize`。

Eigen SVD：`eigen_svd_cdouble` 跑在 NEON 后端；`eigen_svd_cdouble_sve` 跑在 SVE 向量后端（vendored Eigen 5.0 已补 `PacketXd`/`PacketXcd` double 与 complex<double> packet——svcmla 复数乘法、ptranspose 复数转置、gather/scatter 等全套，2026-09-19）。本机（VL=256）实测 300×300 复数 BDCSVD ~0.7 s（标量回退时代 >10 分钟）；VL=512 场景在 gem5 SE 模式功能验证（`scripts/eigen-sve-double/gem5/`）。注意：size-specific SVE 代码要求运行时向量长度等于编译期 `-msve-vector-bits`（128），本机 256 硬件上独立运行该测试需 prctl 固定任务 VL。

## 架构支持

| 架构 | 状态 | 说明 |
|---|---|---|
| ARM64（AArch64） | ✅ 全支持 | Kunpeng 920、通用 ARMv8.1+ |
| x86-64 | 参考架构 | Intel 原始代码路径，移植中不动 |

ARM64 能力：CPU 特性检测（FP/NEON/CRC32/Crypto/SVE/SVE2）、拓扑检测（ACPI PPTT/sysfs/device tree）、SDC 检测（EDAC ECC、CRC32/CRC64）、SIMD（NEON 128 位 + 256/512 仿真）、RAS/ECC（EDAC/ACPI APEI）。

## 延伸

- [编写测试指南](docs/writing_tests.md) — 框架处理了测试生命周期、线程模型、CPU 特性识别、RNG 等样板代码
- [离线构建依赖与排坑](docs/offline-build-dependencies.md) — 完整依赖树、版本管制、坑点
- [SDC 前沿研究综合](docs/paper/SDC_RESEARCH_SYNTHESIS_CN.md) — 31 篇 SDC 文献（SOSP/HPCA/ISCA/MICRO/ASPLOS 等，2003–2026）系统性总结，负载设计与 vendored 依赖库选型依据
- [贡献指南](CONTRIBUTING.md) · [行为准则](CODE_OF_CONDUCT.md)

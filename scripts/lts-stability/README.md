# scripts/lts-stability — 15 个 openEuler LTS 镜像 × SDCShield 全量用例 × 全严格选项稳定性验证

在本地 podman 的全部 15 个 openEuler LTS 容器镜像(20.03/22.03/24.03 × LTS+SP1..SP4)上,
对当前源码构建的全功能 SDCShield 二进制做**全量测试用例 × 全严格 CLI 选项**的稳定性验证。

设计前提(与 `docs/superpowers/plans/2026-09-16-lts-stability-validation.md` 一致):

- **每镜像原生构建**:host 二进制(glibc 2.38)无法在 20.03(glibc 2.28)/22.03(glibc 2.34)
  容器运行,必须每镜像用其原生工具链(gcc 7.3+toolset-10 / gcc 10.3 / gcc 12.3)构建。
- **全功能二进制**:`-Dssl_link_type=static`(vendored OpenSSL 3.5.0 静态链接,ipsec 46 个 +
  openssl_sha 编入)+ vendored openblas/sleef/pocketfft(随 `/src` 挂载,`third-party/*/install/`
  产物)——每镜像 list-tests ≥ 280 才算构建合格。
- **core179 已隔离**(用户 off-line 了该 SDC 核):任何 fail/crash 都是**软件 bug**,不是硬件噪声。

## 用法

```bash
# 一键式:15 镜像全矩阵(3 系列并行,~5 小时;全 15 PASS 才 exit 0)
./scripts/lts-stability/run-all-15.sh

# 一键式快速链路验证(每镜像只跑 m01)
./scripts/lts-stability/run-all-15.sh --smoke

# 单镜像全矩阵(29 个选项组合)
./scripts/lts-stability/run-lts-stability.sh 24.03 SP3

# 单镜像快速冒烟(只跑 m01 基线)
./scripts/lts-stability/run-lts-stability.sh 24.03 SP3 --smoke

# 一个系列 5 个 SP 串行
./scripts/lts-stability/run-lts-stability.sh 22.03 all

# 环境变量调档(默认全严格档):
T_BASE=1000 T_SHORT=500 ./scripts/lts-stability/run-lts-stability.sh 20.03 LTS
SKIP_BUILD=1 ./scripts/lts-stability/run-lts-stability.sh 24.03 SP3   # 复用已构建二进制
```

stdout 每镜像一行 `RESULT: PASS|FAIL <tag> (...)`,15 个全 PASS 才算通过。

## 选项矩阵(29 个组合,`option-matrix.sh`)

| 条目 | 覆盖维度 |
|---|---|
| m01 | 全量基线(全部测试,-n 8) |
| m02 | eigen 数值敏感 4 测试 -n 1(CLAUDE.md 平台特性:大规模多线程 ULP 假 FAIL) |
| m03/m04 | `--quality=-1` / `--quality=0`(SKIP/BETA 级测试也跑) |
| m05 | 全核(默认 -n = 全部 CPU)+ flaky 补跑 |
| m06-m08 | 三种 RNG 引擎(Constant/LCG/AES) |
| m09/m10 | `--cpuset` 单核 / 跨 NUMA 8 核 |
| m11/m12 | `-F --strict-runtime` / `--test-list-randomize` |
| m13-m16 | zstd/zlib level+maxbuffersize 测试旋钮(`-O`) |
| m17/m18 | openblas GEMM mdim=16/4096 旋钮(含 cgemm.mdim / openblas_lu.n) |
| m19-m22 | memcpy_rewr 策略 0/1/2(SANDSTONE_STRATEGY_INDEX)+ 默认模式 |
| x23 | 框架 selftests(freeze 家族 disable——它们等待框架 SIGQUIT,会把单条目拖挂;其余 137 个全跑,注入 fail/crash 为预期) |
| x24 | `--on-crash=context -e selftest_sigsegv -vv`(崩溃上下文捕获) |
| m25 | `-T 10s --strict-runtime` 总时长模式 |
| m26 | `-vv` 抽样 5 测试 |
| m27 | ipsec 46 + openssl_sha(SSL 静态链接验证) |
| m28 | `--1sec` 覆盖驱动模式 |
| m29 | isal_igzip.level=3 压缩等级旋钮 |
| m30 | openssl_sha3 + openssl_sm3sm4 + isal_igzip(coverage-max 新测试集) |

判定(m 系):exit 0 + yaml 无 `result: fail` + 无工具级崩溃标志;
x 系(预期注入失败):完成执行且不挂死即 PASS。

## 输出

```
build-out/lts-stability/<tag>/
├── build.log    # 容器内 meson+ninja 全程日志
├── matrix.log   # 28 个条目的完整 stdout/stderr
└── *.yaml       # 每条目的测试结果日志
```

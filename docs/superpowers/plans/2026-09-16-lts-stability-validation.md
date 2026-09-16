# 15 个 openEuler LTS 镜像 × SDCShield 全量用例 × 全严格选项 稳定性验证

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use Markdown task-list checkboxes for tracking (live plan = single source of truth for done vs pending).

**Goal:** 在本地 podman 的全部 15 个 openEuler LTS 镜像(20.03/22.03/24.03 × LTS+SP1..SP4)上,对当前 HEAD(901ca96)构建的 SDCShield 二进制做全量测试用例 × 全部严格 CLI 选项组合的稳定性验证;遇到软件 bug 修复后重验;最终把全过程固化为 `scripts/lts-stability/` 下的一键式执行脚本。

**Hardware 前提(已实测确认):** Kunpeng 920 192 CPU,core179 已被用户隔离(`cpu179/online=0`,affinity `0-178,180-191`)——这是**软件稳定性验证**(不是硬件 SDC 检测),失败即软件 bug。

**Architecture:**
- 每个镜像 = 一个独立 podman 容器,挂载同一份新构建的全功能二进制(builddir-full/sdcshield,ssl_link_type=static,286 测试,含 ipsec46+openssl_sha+sleef+openblas+pocketfft+isal_igzip)。
- 22.03/20.03 容器需捆绑 libatomic/libs(built/libs);24.03 挂 host /usr/lib64(与既有 run-full-tests.sh 相同的挂载策略)。
- "全严格选项" = 覆盖 CLI 的关键维度:--quality 全档(-1/0/2)、-n 1/8/all、--cpuset(单核/跨NUMA)、-s 三种 RNG、-O 全部测试旋钮、-F/--fatal-errors、--strict-runtime、--test-list-randomize、--on-crash=context、-vv、--ignore-timeout、-T 总时长模式、selftests 全套。
- 多 subagent 并行:每 subagent 负责 1-2 个镜像的全矩阵;执行脚本内部 xargs -P 并行跑选项矩阵。

**Tech Stack:** bash + podman(已有 offline-build 体系),复用其挂载/自愈逻辑;新增 `scripts/lts-stability/run-lts-stability.sh`。

## Global Constraints

- **一补丁一单元**:Task 1(脚本)一个 commit,Task 2(执行+报告)产出 docs 报告一个 commit;遇到 bug 的修复各自独立 commit(修复→重验→再提交报告)。
- **验证 100% 真实**:所有 RESULT 行必须来自真实运行;报告引用实测输出。
- **main 分支不直接提交**:开工先切 `feat/lts-stability-validation` 分支。
- **x86-64 非回归**:本计划不改任何 framework/tests 源码(除非修 bug);脚本只在 scripts/。
- **既有 README/docs 同步**:Task 2 完成后更新 README 提及新脚本(大颗粒度)。

## 已确认的关键事实(实测)

1. 15 个镜像全部在本地:localhost/openeuler-offline:{20.03,22.03,24.03}-{LTS,LTS-SP1..SP4},gcc 7.3/10.3/12.3。
2. 全功能二进制(builddir-full):286 测试(PROD),--quality=0 时 290,+--selftests 时 142 个 selftest。构建选项 `-Dssl_link_type=static`(vendored openssl 3.5.0)+ openblas/sleef/pocketfft install/ 已就绪。
3. 22.03/20.03 镜像缺 libatomic → 从 RPM 树自愈(run-full-tests.sh 既有逻辑)。
4. isal_igzip 仅 24.03 系列可用(仅 host 有 libisal);20.03/22.03 会 skip-lib(generate 阶段就不编译)——需要在各镜像**分别构建**原生二进制而非共享一份?**否**——本任务用"每镜像原生构建"路线(container-build.sh),保证 15 份二进制各自匹配其 OS 库;共享二进制仅用于本机快速冒烟。
5. 既有验证(2026-09-02,README 记录)已 15×full PASS,但那是 220 测试版本、未开 SSL/vendored 库、未扫严格选项矩阵。本次是超集验证。
6. 测试旋钮(-O):zstd/zlib(level,maxbuffersize)、openblas_{d,s,z}gemm(mdim 16..4096)、ifs(test_file,enforce_run)。
7. memcpy_rewr 环境旋钮:SANDSTONE_STRATEGY_INDEX(0..2)、SANDSTONE_STRATEGY_CONF。
8. eigen 4 个 flaky 测试(eigen_svd_double/eigen_sparse/eigen_svd_cdouble/eigen_svd_cdouble_sve)在全核多线程下有 ULP 级假 FAIL——CLAUDE.md 已记录为平台特性,验证策略:全核跑 + -n1 补跑,两者都过才算过(与 run-sdcshield.sh full 模式一致)。
9. core179 已隔离:任何 crash/fail 都不是 core179 硬件问题,是软件 bug(修复对象)。

---

### Task 1: 编写 scripts/lts-stability/ 一键式验证脚本

**Files:**
- Create: `scripts/lts-stability/run-lts-stability.sh`(主入口:单镜像全矩阵 → 15 镜像编排)
- Create: `scripts/lts-stability/option-matrix.sh`(选项矩阵定义,被主脚本 source)
- Create: `scripts/lts-stability/README.md`

**设计:**

每镜像执行阶段(输入:series sp):
1. 镜像就绪检查(podman image inspect)。
2. 容器内原生构建全功能二进制(meson -Dssl_link_type=static;22.03/20.03 注入 polyfill,复用 container-build.sh 的 EXTRA_MESON 机制,追加 -Dssl_link_type=static;vendored 库的 install/ 随 /src 只读挂载天然可用)。
3. 选项矩阵执行(每个组合独立 podman run,timeout 保护,-o yaml 输出到挂载目录):
   - M1 全量基线:`-t 1000 -n 8 --ignore-timeout`(286 测试)
   - M2 eigen flaky -n1:4 测试各 `-n 1 -t 1000`
   - M3 quality 扫描:`--quality=-1 -t 500 -n 8`(295 测试,含 SKIP 级)与 `--quality=0 -t 500 -n 8`(290)
   - M4 全核:`-t 1000`(默认 -n = 191 线程,排除 4 个 eigen flaky,单独 -n1 补)
   - M5 RNG 扫描:zstd19/crc32/fma 三测试 × `-s Constant:1/LCG:1/AES:1`(AES 不在则 default)
   - M6 cpuset:zstd19 × `--cpuset=0` / `--cpuset=0,24,48,72`(跨 NUMA)
   - M7 严格模式:`-F --strict-runtime -t 800 -n 8` + `--test-list-randomize -t 800 -n 8`
   - M8 测试旋钮:zstd19 `-O zstd19.level=1/19`、zlib9 `-O zlib9.level=1/9`、openblas_dgemm `-O openblas_dgemm.mdim=16/4096`、memcpy_rewr × SANDSTONE_STRATEGY_INDEX=0/1/2
   - M9 selftests:`--selftests -t 300 -n 4 --ignore-timeout`(142 个)
   - M10 崩溃上下文:`--on-crash=context -e selftest_sigsegv -vv`(期望:干净捕获崩溃,非工具本身 crash)
   - M11 -T 模式:`-T 10s --strict-runtime -n 8`(限制测试数 --max-test-count 20 防超时)
   - M12 verbose:抽样 5 个测试 `-vv`
4. 判定:全部组合 exit code 0(yaml 无 result: fail、无 crash 标志、期望 fail 的 selftest 除外)→ `RESULT: PASS <tag>`。

15 镜像编排:`run-lts-stability.sh --all`,xargs -P 3 并行(3 系列,每系列 5 SP 串行,避免 podman/RAM 过载),汇总 15 行 RESULT。

**验证(Task 1 完成标准):**
- [ ] `bash -n` 语法通过;`--help` 输出正常
- [ ] 单镜像冒烟:`./run-lts-stability.sh 24.03 SP3 --smoke`(只跑 M1 短版)真实输出 `RESULT: PASS`

---

### Task 2: 执行 15 镜像全矩阵验证(多 subagent 并行)

**流程:** 3 个 subagent 各负责一个系列(24.03/22.03/20.03 的 5 个 SP),跑 `run-lts-stability.sh <series> all`(脚本内部对 5 SP 串行、镜像内矩阵完整执行)。主 agent 汇总 15 个 RESULT 行;任何 FAIL → 定位(log 在 build-out/lts-stability/<tag>/)→ 修复(bug fix 独立 commit)→ 重跑该镜像全矩阵 → 全 15 PASS。

- [ ] 24.03 系列(5 SP)全矩阵 PASS
- [ ] 22.03 系列(5 SP)全矩阵 PASS
- [ ] 20.03 系列(5 SP)全矩阵 PASS
- [ ] 汇总报告写入 `docs/cases/lts-stability-2026-09-16/`(15×RESULT 行 + 问题修复记录)
- [ ] README 增补脚本用法(大颗粒度文档同步)

**Bug 修复纪律:** 每个发现的 bug = 独立 commit(含根因分析 + 真实重验输出),修复后受影响镜像全矩阵重跑。

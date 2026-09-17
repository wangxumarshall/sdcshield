# 15 个 openEuler LTS 镜像 × SDCShield 全量用例 × 全严格选项稳定性验证报告

**日期**:2026-09-16 ~ 2026-09-17
**代码基线**:main `ebe0bf1`(Merge PR #123,coverage-max 测试集:290 测试含
openssl_sha3/openssl_sm3sm4/openblas_cgemm/openblas_lu + vendored isa-l)
**硬件前提**:Kunpeng 920 192 CPU,**core179 已被用户隔离下线**(`cpu179/online=0`,
可用 CPU = 191)——本验证为**软件稳定性验证**:任何测试 fail/crash 都按软件 bug 处置。
**工具链矩阵**:24.03(gcc 12.3)/ 22.03(gcc 10.3)/ 20.03(gcc-toolset-10 + glibc 2.28)

## 最终结果:15/15 镜像全 PASS

每镜像 = 容器内原生构建全功能二进制(ssl_link_type=static + vendored
openssl/openblas/isa-l)+ 29 条目严格选项矩阵 × 全部测试用例:

```
RESULT: PASS openEuler-24.03LTS      (matrix 29/29 yaml_fail=0)   list-tests 290
RESULT: PASS openEuler-24.03LTS_SP1  (matrix 29/29 yaml_fail=0)   list-tests 282
RESULT: PASS openEuler-24.03LTS_SP2  (matrix 29/29 yaml_fail=0)   list-tests 282
RESULT: PASS openEuler-24.03LTS_SP3  (matrix 29/29 yaml_fail=0)   list-tests 292
RESULT: PASS openEuler-24.03LTS_SP4  (matrix 29/29 yaml_fail=0)   list-tests 282
RESULT: PASS openEuler-22.03LTS      (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-22.03LTS_SP1  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-22.03LTS_SP2  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-22.03LTS_SP3  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-22.03LTS_SP4  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-20.03LTS      (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-20.03LTS_SP1  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-20.03LTS_SP2  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-20.03LTS_SP3  (matrix 29/29 yaml_fail=0)   list-tests 287
RESULT: PASS openEuler-20.03LTS_SP4  (matrix 29/29 yaml_fail=0)   list-tests 287
```

list-tests 差异说明(镜像库差异,非缺陷):isal 10 个测试仅 24.03-SP3 有系统
libisal(其余镜像靠 vendored isa-l 获得 isal_igzip);sleef 2 个测试仅在有 cmake
的环境编入(22.03/20.03 容器无 cmake,优雅缺席);ACL 2 个测试仅 24.03
(enable_acl=disabled 于 22.03/20.03,ABI 不兼容);crt_builtins 仅 clang-rt 所在
host 编入。判定硬线:ipsec=46 + openssl_sha=1(SSL 生效标志)+ 总数 ≥270。

## 验证覆盖(29 条目 × 全部测试)

m01 全量基线(-n 8)/ m02 eigen flaky -n1 / m03-m04 quality -1 与 0(SKIP+BETA
级)/ m05 全核(默认 -n = 191 线程)/ m06-m08 三 RNG 引擎(Constant/LCG/AES)/
m09-m10 cpuset 单核与跨 NUMA / m11 -F --strict-runtime / m12 --test-list-randomize /
m13-m16 zstd/zlib level+maxbuffersize 旋钮 / m17-m18 openblas GEMM mdim 16 与 4096
(含 cgemm.mdim、openblas_lu.n=2048)/ m19-m22 memcpy_rewr 策略 0/1/2+默认 /
x23 框架 selftests(137 个,freeze 家族除外)/ x24 --on-crash=context 崩溃捕获 /
m25 -T 总时长模式 / m26 -vv / m27 ipsec46+openssl_sha / m28 --1sec /
m29 isal_igzip.level=3 / m30 sha3+sm3sm4+igzip。

执行方式:3 个 subagent 并行(每系列一份 cp -al 硬链接源码副本,消除共享挂载的
SELinux :Z 并发 relabel 竞态),系列内 5 SP 串行。总墙钟 ~5 小时/轮。

## 过程中发现并修复的软件 bug(全部有独立 commit + 重验)

### 第一轮(旧 main 901ca96,28 条目矩阵)—— 7 个修复

1. **ACL 头挂载缺 `:Z`**(container-build.sh):rootless podman + SELinux
   enforcing 下 `fatal error: Types.h: Permission denied`。
2. **crypto_dep 未传给 framework 库自身**(framework/meson.build):openssl 头
   靠系统路径侥幸命中(24.03 openssl 3.0),22.03/20.03(openssl 1.1.1)编译失败
   `openssl/core_names.h: No such file or directory`。加 `framework_ssl_deps`。
3. **libclang_rt.builtins.a 无条件链接**(tests/cpu/arithmetic_arm/meson.build):
   22.03/20.03 容器无该文件,链接失败。probe 门控优雅降级。
4. **vendored 库宿主产物跨 glibc 链接失败**:host(glibc 2.38)构建的
   libcrypto.a/libopenblas.a 引用 `__isoc23_strtol`,22.03(2.34)/20.03(2.28)
   链接 undefined。inner 脚本按 `.glibc-build-tag` 容器内原生重建。
5. **glibc 探测在 pipefail 下产生双行值**(container-build.sh):
   `ldd|head|grep` 链使 ldd 收 SIGPIPE,`|| echo unknown` 把 "unknown" 追加在
   版本号后 → tag 永假 → 每次都重建。改用 `getconf GNU_LIBC_VERSION`。
6. **20.03-SP4 镜像缺 gcc-toolset-10 烘焙层**:自愈链逐步补齐 —— toolset RPM
   强装 + gcc-toolset-10-cpp(cc1)+ libmpc/mpfr/gmp(cc1 运行时)+ binutils
   (ld)+ glibc-devel(crt1.o)+ toolset libstdc++(`-lstdc++`)+ ld 符号链接
   兜底(烘焙分支)。
7. **共享 /src 挂载的并发 :Z relabel 竞态**:三系列并行构建时 cp -a 读到正在
   被 relabel 的文件 → 残缺源码 → meson 缺文件。per-series cp -al 源码副本。

### 第二轮(新 main ebe0bf1,29 条目矩阵)—— 1 个关键修复

8. **openEuler-22.03-LTS 的 binutils 2.37-25 汇编 OpenSSL SM4 ARMv8-CE 汇编产出
   错误机器码**(container-build.sh):`openssl_sm3sm4` SM4-CBC 解密自第二块起
   确定性错位(roundtrip mismatch @offset16,`-s LCG:614153748` 100% 复现)。
   证据链:LTS 二进制在任何环境(LTS/SP1 容器、host)都失败,SP1..SP4 二进制
   (binutils 2.37-23)处处 PASS;同源码同 gcc 同 Configure flags;裸 EVP harness
   5 万次随机不触发(需完整二进制上下文);no-asm 重建后失败 seed PASS。
   修复:容器内 openssl 重建统一 `no-asm`(SM3/SM4 走纯 C),tag 加 `-noasm`
   后缀强制一次性迁移。

### 竞态教训(工具链自身,非产品 bug)

运行中编辑被容器挂载的 `option-matrix.sh` 会在编辑窗口产生
`/matrix.sh: Permission denied` → exit 127(6 个条目受累,全部单独重跑 PASS)。
一键脚本(run-all-15.sh)运行期间不要再编辑 scripts/lts-stability/ 下文件。

## 一键式执行

```bash
./scripts/lts-stability/run-all-15.sh            # 15 镜像全矩阵(3 系列并行)
./scripts/lts-stability/run-all-15.sh --smoke    # 链路快检
```

明细产物:`build-out/lts-stability/<tag>/{build.log,matrix.log,*.yaml}`。

另有 CI 日报体系(`.github/workflows/multi-os-verify.yml` + `scripts/gha/`):
GHA 原生容器模式每日 15 镜像全量验证 + 跨 OS 基准,与本脚本互补
(本地 podman 全严格矩阵 ↔ 云端每日回归)。

## 日志索引(关键轮次)

| 轮次 | 日志 | 说明 |
|---|---|---|
| R1 首跑 | /tmp/lts-{24,22,20}.03-all.log | 15/15 FAIL(并行 :Z 竞态) |
| R1 修复后 | /tmp/lts-*-all2.log + reverify-* | 14/15 PASS(20.03-SP4 构建链) |
| R2 首跑(新 main) | /tmp/r2-*-all.log | 9/15(22.03-LTS sm3sm4 + 6 个 127 竞态) |
| R2 修复后 | /tmp/r2-22.03-LTS-fix.log, /tmp/r2-20.03-reverify.log, /tmp/r2-24.03-LTS-reverify.log | **15/15 PASS** |

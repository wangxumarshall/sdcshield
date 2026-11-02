# eigen 系列 SVE 统一支持 + eigen_svd_bidiag_sve 退役 — 设计文档

日期:2026-11-02。前置讨论:本会话头脑风暴(范围/档位/bidiag/sparse/dump
五项决策均获用户批准)。计划文档(后续):docs/superpowers/plans/
(writing-plans 产出)。

## 1. 目标与非目标

**目标**:
1. eigen 系列全部 dense 用例获得 SVE 支持(用户决策:全 eigen 系列),
   覆盖 float/double/cfloat/cdouble × BDCSVD/JacobiSVD/GEMM;
2. 复用既有 eigen 框架(`EigenSVDTest` 模板 + per-tier body/dispatcher
   双文件模式),**不单独实现**——通过抽取公共 dispatcher 框架头把
   svd_cdouble_sve.cpp 中 ~300 行重复样板收敛为单点;
3. 每用例一个单独文件实现 SVE 支持(用户决策),跟随既有
   `eigen_svd_cdouble_sve` 命名/布局先例;
4. 四档全建(128/256/512/1024,用户决策:跟随现有模式),dispatcher
   运行时按框架 pin 的实际 VL 选择,主机无 SVE 时诚实 SKIP;
5. 退役删除 `eigen_svd_bidiag_sve`(手写 SVE 内核,存在理由已消失)。

**非目标(范围外,如实声明)**:
- `eigen_sparse` SVE 版——**实证排除**(见 §5);
- GEMM 三个 dump 变体 SVE 版——用户决策不做(调试记录工具,基版
  SVE 化后现场可定位);
- `eigen_svd_cdouble_noavx512` 的 SVE 对应物——它是 x86 AVX-512 gate
  变体,其 ARM 对应物就是既有的 `eigen_svd_cdouble_sve`,无需新做;
- SVE2/SME——本设计只涉及 SVE1(与既有 eigen SVE 先例一致);
- 修改 vendored Eigen 的 SVE 后端(issue #410 已修好 complex 全档,
  本设计只消费不改动)。

## 2. 事实基线(2026-11-02 本会话探索与实测)

| 事实 | 证据 |
|---|---|
| SVD/Jacobi 全家族(10 用例)是 `EigenSVDTest<SVD, Dynamic>` 模板用户 | tests/cpu/eigen_svd/{svd,svd_double,svd_fvectors,svd_cdouble}.cpp、eigen_svd_jacobi/*.cpp 逐文件阅读 |
| GEMM 家族是独立实现(不走 SVD 模板),已有 per-tier 先例 | gemm_float/cdouble/double_dynamic_square.cpp(独立 run 循环)+ gemm_double_max_arm.cpp(既有 dispatcher)+ _body.cpp |
| vendored Eigen SVE 后端覆盖 float/double/complex | third-party/eigen5/.../arch/SVE/:PacketXf(svfloat32)/PacketXd(svfloat64)/Complex.h;meson 注释确认 real 全档 + complex 全档(#410 后) |
| 既有 per-tier 模式 | tests/cpu/meson.build:1141-1245:每 body TU 按 `-DEigen=EigenSVE<n> -msve-vector-bits=<n> -D*_TEST_NAMESPACE=*_vl<n>` 编译 ×4;dispatcher TU 无 -msve-vector-bits 进 tests_sve |
| bidiag 存在理由已消失 | 文件头自述绕开 "vendored Eigen5 SVE double packet SIGSEGV";issue #410(2026-11-01 ptranspose scatter 重写)已修复,CLAUDE.md 确认 complex SVD 四档全跑 |
| dump 变体 3 个均 PROD 级 | grep quality_level |
| Jacobi 四件套 SKIP 级 | 各文件 quality_level = TEST_QUALITY_SKIP(issue #322 待满核晋升验证) |

**sparse spike 实证(本会话,g++ armv8.2-a+sve + EigenSVE128 反汇编)**:
`SimplicialCholesky::_solve_impl` 数值内核 **0 处** SVE 指令;全程序仅
29 处 SVE,全部位于 AMDOrdering 排序/置换/nonZeros 等**外围**(GCC 自
动向量化);对照组 dense GEMM 同条件 667 处(gebp_kernel 数值内核
密集)。Eigen sparse 模块无 packet 化路径,做 SVE 版是假 SVE 用例。

## 3. 设计:共享框架 + 薄用例文件(方案 B)

### 3.1 总体结构

```
tests/cpu/eigen_svd/sandstone_eigen_sve_dispatch.h   [新增] 公共 dispatcher 框架
  ├── tier 校验(128/256/512/1024 白名单,非法档 SKIP)
  ├── run 期 re-pin + readback 校验(pinned≠want → SKIP;readback 不符 → FAIL)
  ├── 联合求解探针 per-tier 分派(solve_once)
  ├── make_data/cleanup/run_body per-tier 分派
  └── namespace 声明宏(参数化前缀 NS##_vl{128,256,512,1024})

每 SVD 用例 = 1 个薄 dispatcher TU(~60 行)
  ├── include 框架头 + sandstone_eigen_common.h
  ├── EIGEN_SVE_DECLARE_BODY_NS(<ns>)   /* 4 个 per-tier namespace 前向声明 */
  ├── tier→namespace 静态表 / 分派宏
  ├── init:knob/env 联合求解(参数 = 原用例 calibrated_auto_dim 同值)
  └── DECLARE_TEST(<test_id>_sve, ...)  /* 元数据镜像原用例 */

SVD body = 复用 EigenSVDTest 模板(per-tier TU,每用例一文件)
  └── 既有 svd_cdouble_sve_body.cpp 同构:typedef Mat/SVD;
      make_data/solve_once/cleanup/run_body 一行委托 test_type::*

GEMM 用例 = 1 个薄 dispatcher TU(同上)+ 1 个 body 模板 TU
  └── run 语义镜像 dynamic_square 系(fill_rand + golden 同源首算
      + kRerollEvery 重掷 + memcmp_or_fail,T10 bad_alloc SKIP 契约),
      标量类型参数化(float/double/cdouble)
```

### 3.2 复用件与漂移防护

- **dispatcher 框架头**承载全部 tier 逻辑(校验/pin/分派/日志),
  各用例 TU 不再持有任何 tier 代码——12 个新用例的 tier 语义单点
  维护。既有 `svd_cdouble_sve.cpp`/`gemm_double_max_arm.cpp` 的
  dispatcher 段**迁移**到框架头(同 commit 收敛,消除两份既有拷贝
  与新 TU 的四方漂移面)。
- **SVD body**:`EigenSVDTest` 模板单点承载 run/golden/reroll/联合
  求解语义,per-tier body 只做 typedef + 委托(既有
  svd_cdouble_sve_body 模式)。
- **GEMM body**:run 语义从 gemm_*_dynamic_square.cpp 收敛成参数化
  模板(标量类型),消重 4 份(dynamic_square ×3 + double14 语义
  同源)。
- **meson 泛化**:现有 foreach spec [['gemm','128'],…] 列表驱动
  扩展为 (kind, tier, source) 三元组表,新增用例 = 加行,不改块。

### 3.3 命名与文件布局(跟随既有先例)

- test id:原用例名 + `_sve` 后缀(`eigen_svd_sve`、
  `eigen_svd_double_sve`、`eigen_svd_fvectors_sve`、
  `eigen_svd_double2_sve`、`eigen_svd_jacobi_sve`、
  `eigen_svd_jacobi_double_sve`、`eigen_svd_jacobi_fvectors_sve`、
  `eigen_svd_jacobi_cdouble_sve`、
  `eigen_gemm_float_dynamic_square_sve`、
  `eigen_gemm_cdouble_dynamic_square_sve`、
  `eigen_gemm_double_dynamic_square_sve`、`eigen_gemm_double14_sve`)。
- 文件:dispatcher `eigen_svd/svd_float_sve.cpp` 等(与
  svd_cdouble_sve.cpp 同目录、同命名法);body
  `eigen_svd/svd_float_sve_body.cpp` 等;GEMM 侧
  `eigen_gemm/gemm_float_dynamic_square_sve{,_body}.cpp`。
  fvectors 与 float 版实现相同(随机化改造后 init-time
  interesting-vector 已删,两者已是同一计算)——SVE 版共享 body
  TU、dispatcher 各自一文件(knob 名/env 名按 test id 区分)。
- tier namespace:`<ns>_vl{128,256,512,1024}`(跟随 svd_vl128 先例)。
- 元数据镜像原用例:groups=group_math、fracture_loop_count、
  quality_level(BDCSVD/GEMM 系 PROD;Jacobi 系 SKIP 跟随原级,
  晋升验证独立进行)、arm64_requires=ARM64_REQUIRES_SVE、
  desired_duration 链(preinit 上界模型按原用例口径)。
- knob/env:`-O <test_id>.mdim=N` 与
  `SDC_<TEST_ID>_MDIM`(联合求解 lo/hi 沿用原用例 calibrated_auto_dim
  实参;RSS family 复用既有表项 BDCSVD_F32/F64/C128、JACOBI_*、
  GEMM_F32/F64/C128——SVE 与 NEON 版同算法同矩阵数,系数共用,
  VmHWM 反馈自动校准)。

## 4. 用例清单与分期(每用例一个 commit,规则 1)

| 期 | commit | 内容 |
|---|---|---|
| P1-1 | 框架 | 新增 sandstone_eigen_sve_dispatch.h;svd_cdouble_sve/gemm_double_max_arm dispatcher 段迁移到框架头(行为零变化,既有两用例四档回归) |
| P1-2 | 用例 | eigen_svd_double_sve(dispatcher+body+meson 行)——**承接 bidiag 的 SVE FMA 场景**(Eigen BDCSVD 第一阶段即 Householder 双对角化) |
| P1-3 | 退役 | 删除 tests/cpu/sve/eigen/eigen_svd_bidiag_sve.cpp + tests/cpu/sve/meson.build:79-86 引用与注释;--list-tests 与 PROD 计数变化如实记录 |
| P2 | 3 用例 | eigen_svd_sve、eigen_svd_fvectors_sve、eigen_svd_double2_sve(double2 与 double 共享 body,dispatcher 各自) |
| P3 | 4 用例 | eigen_svd_jacobi_sve、_double_sve、_fvectors_sve、_cdouble_sve(SKIP 级跟随) |
| P4 | 4 用例 | eigen_gemm_{float,cdouble,double}_dynamic_square_sve、eigen_gemm_double14_sve(GEMM body 模板在 P4-1 随首用例落地) |

## 5. sparse 排除的实证记录

eigen_sparse 不做 SVE 版。实证(§2 末行):数值内核 0 SVE 指令,
29 处全在外围(AMDOrdering/置换/nonZeros,GCC 自动向量化);对照
dense GEMM 667 处。Eigen sparse 模块无 SVE packet 路径是上游事实,
不是本仓可修项;做"SVE 版"只会产出名义 SVE 用例,违反规则 6 的
placeholder honesty 精神。该结论已写入本 spec(用户批准),后续
若上游 sparse packet 化可重新评估。

## 6. 验证策略(规则 2)

每 commit:
- `./scripts/build.sh` 干净、零新告警;
- 新用例实跑引用真实输出(-e <test> -t N -n 1 与全核档);
- 回归 `-e zstd19 -t 2000 -n 1` → exit: pass;
- x86-64 非回归:本设计全部落在 aarch64 guard 内(dispatcher 框架头
  按既有 #elif __aarch64__ 模式),x86 分支零改动。

档位覆盖(规则 2 gem5-fi 条款):
- 本机原生档位(探测 arm64_spec_get 后按实际):原生实跑;
- 缺失档位:gem5-fi(/home/sdc/wangxu/gem5-fi,CHAOS/SE,
  --sve_vl {1,2,4,8})在受影响档位跑新用例,引用真实输出
  (tier、checksum、SDCSHIELD_FAILS=0);
- P1-1 框架迁移 commit:既有 eigen_svd_cdouble_sve 与
  eigen_gemm_double_max_arm 四档 gem5 回归(迁移零行为变化的证明);
- bidiag 退役 commit:删除后全量 --list-tests diff + 默认集回归。

## 7. 文档同步(规则 7)

- README「测试用例与检测能力」:PROD/SKIP 计数链按实测更新
  (P1-3 删 1 PROD;P1-P4 新增 12 用例按各自 quality 计入);
- CLAUDE.md 既有 bidiag 引用(超时历史、"Eigen SVE pair"表述)
  更新为 12+ 用例的统一表述;
- docs/coding/writing_tests.md 若涉及 per-tier 模式描述则同步。

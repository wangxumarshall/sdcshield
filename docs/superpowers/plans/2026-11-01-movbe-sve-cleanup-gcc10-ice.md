# movbe SVE cleanup gcc-10 expand ICE 修复计划

**Goal:** 修复 main 分支 20.03/22.03 全系构建红（10 个每日 CI job + PR 角落构建）：gcc-10.x 对 4 个新增 movbe SVE cleanup 函数在 -O2/-O3 下 RTL expand ICE。

**Architecture:** 跟随家族既有先例（run 函数自 626f6554 起携带 `__attribute__((optimize("O1")))` 规避同族 gcc-10 工具链脆弱性）——给 4 个 cleanup 补同款属性。cleanup 为冷路径（post-join 16 槽校验），O1/O3 语义零差异。

## 根因链（已实证，2026-11-01）

1. **现象**：每日 multi-os-verify 10 个 20.03/22.03 job 红 + PR #431 角落构建 2 红，其余全绿（24.03 gcc 12.3 正常）。
2. **崩点**：`tests/cpu/sve/case/movbe_dump_probe_{f,g1,g2,h}_sve.cpp` 的 `*_cleanup` 函数，`internal compiler error: Segmentation fault, during RTL pass: expand`（f:214:12、g1:204:12，与 CI 一致）。
3. **完整清单**（CI ninja 提前停止只见 2 个；容器内 `-k 0` 全库扫出全部 4 个）：probe_{f,g1,g2,h}_sve。
4. **本地复现**（20.03-LTS 容器，gcc-toolset-10 = gcc 10.3.0）：最小旗标 `g++ -std=gnu++20 -march=armv8.2-a+sve -O2 -c` 于预处理源 `ice-probe-f.i` 即崩；**-O0/-O1 OK，-O2/-O3 ICE**。
5. **引入**：fd7684f7（今日，issue #374 W4b）新增 4 个 cleanup post-join slot 校验；f307f3cf 加 workers_ran 守卫。既有 run 函数带 `optimize("O1")`（626f6554），**cleanup 漏带**。
6. **排除**：log_warning/log_error/svcntw 单独均非触发（同库既有文件早已使用且 10-28~31 每日绿；v1/v2/v3 变体全 OK——崩塌需函数整体形态 + O≥2，属性是本家族的既有规避位）。

## 修法（4 处单行属性，逐字跟随 run 先例）

每文件在 `static int movbe_dump_probe_{X}_sve_cleanup(struct test *test)` 上一行加：

```c
__attribute__((optimize("O1")))
```

注：probe_h 的 run 本就无该属性且从不崩（其 run 形态不触发），本次只修崩的 cleanup，不顺手改 run（最小修复原则）。

## 验证矩阵（规则 2，100% 实证）

- (a) 20.03-LTS 容器：tests_sve_avx53 全库 `-k 0` 编译 4/4 绿（修复对象的直接复验）；
- (b) 22.03-LTS-SP3 容器（系统 gcc 10.3）：同上；
- (c) 本机（24.03 gcc 12.3）：`./scripts/build.sh` 全量零新告警；
- (d) 受影响行为实跑：本机 SVE1 可跑 4 用例 `-e movbe_dump_probe_{f,g1,g2,h}_sve` → exit: pass（同时实证 fd7684f7 新 cleanup 校验逻辑本身 + O1 下运行正确）；
- (e) 回归：`-e zstd19 -t 2000 -n 1` → pass；
- (f) x86：tests/cpu/sve 为 aarch64 专属库（-march=armv8.2-a+sve），x86 构建不含——检视即证零影响。

## 提交

单 commit（一个修复单元）：`fix(tests): movbe sve cleanup 补 optimize("O1")——gcc-10 -O2/-O3 RTL expand ICE（每日 15 镜像 CI 10 job 红根因）`，推 `fix/movbe-sve-cleanup-gcc10-ice`（自 main），不推 main。

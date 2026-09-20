# run-sdcshield.sh 增加 full 选项（全核满载 eigen 运算）

日期：2026-09-14

## 背景

用户需求：`third-party/rpms/` 目录下的 `run-sdcshield.sh`（目标机离线运行随包
sdcshield 二进制的入口脚本）增加 `full` 选项，使当前 OS 中的**所有核**都能
**满负荷**执行 **eigen 运算**。

### 现状（实测确认）

- `run-sdcshield.sh` **不是手写文件，是生成产物**：源头是
  `scripts/offline-build/package-built-artifacts.sh:162-171` 的 heredoc。当前
  15 份拷贝（3 系列 × 5 SP 的 `built/run-sdcshield.sh`）内容完全一致
  （md5 `4c67d114…` 全部相同）。改 15 份拷贝而不改生成器会在下次
  `package-built-artifacts.sh` 运行时被覆盖——**必须改生成器，再刷新 15 份产物**。
- 二进制线程语义（用 `build-out/openEuler-24.03LTS_SP3/sdcshield` 实测）：
  - **省略 `-n` → 默认全部 CPU**（本机 128 核，test-plans 显示 64+64 满配）。
  - `-n 0` 反而**缩到 1 线程**（`sandstone_opts.cpp:908-918` 的 `Saturate`
    钳位到 min=1，与 `--help` 文本相矛盾）——**full 模式绝不能带 `-n 0`**。
  - `-e 'eigen*'` 通配符由二进制自身展开，实测选中 15 个 PROD 级 eigen 测试。
- eigen 测试集（`--list-tests` 实测 15 个 PROD）分两类：
  - **11 个稳定测试**（gemm 系列 + eigen_svd + eigen_svd_cdouble_noavx512 +
    eigen_svd_double2 + eigen_svd_fvectors）：15-SP 矩阵 full 验证一直以
    多线程（-n8）跑且稳定。
  - **4 个 flaky 测试**（`eigen_svd_double/eigen_sparse/eigen_svd_cdouble/
    eigen_svd_cdouble_sve`）：CLAUDE.md 已记录——大规模多线程（192 核）下
    因并行 SVD/sparse 求解顺序 ULP 级差异对严格 `memcmp` golden 偶发假 FAIL。
    项目所有现有验证脚本（`run-full-tests.sh`、`verify-built-pristine.sh`）
    都刻意用 `-n 1` 跑它们。`eigen_svd_cdouble_sve` 在无 SVE 的机器上自动
    skip（`skip-category: CpuNotSupported`，实测确认）。
- 其余链路**无需改动**：`package-release.sh` 产的 tarball 直接拷
  `built/run-sdcshield.sh`，其 `run.sh` 入口是 `exec run-sdcshield.sh "$@"`
  透传——`full` 选项自动对 tarball 部署可用；`verify-built-pristine.sh` /
  `run-full-tests.sh` 直接跑二进制不经过该脚本，不受影响。
- `-t <time>` 每测试时长选项实测可覆盖（重复 `-t` 后者生效）。

## 用户已确认的决策（AskUserQuestion）

1. **flaky 处理**：稳定测试全核 + flaky 单线程补跑（两段式）。与项目现有
   验证脚本的处理方式一致，不会假 FAIL。
2. **默认时长**：每测试 60s，可用 `-t` 覆盖（如
   `./run-sdcshield.sh full -t 120s`）。一轮约 14 分钟（14 个可跑测试 × 60s，
   其中 eigen_svd_cdouble_sve 在无 SVE 机器上 skip 不耗时）。

## 设计

### full 模式行为（两段式）

```bash
./run-sdcshield.sh full                    # 满载 eigen 一轮（默认 -t 60s）
./run-sdcshield.sh full -t 120s            # 每测试 120s
./run-sdcshield.sh full -T 1h              # 循环至少 1 小时（-T 透传）
./run-sdcshield.sh [任意原参数]             # 原行为完全不变
```

第一段（满载）：
```bash
"$SCRIPT_DIR/sdcshield" -e 'eigen*' \
    --disable eigen_svd_double --disable eigen_sparse \
    --disable eigen_svd_cdouble --disable eigen_svd_cdouble_sve \
    -t "${FULL_T}" <extra args>            # 无 -n → 默认全部 CPU
```
（实测验证：`-e 'eigen*'` + 4 个 `--disable` 精确选出 11 个稳定测试）

第二段（flaky 补跑，单线程）：
```bash
"$SCRIPT_DIR/sdcshield" -e eigen_svd_double -e eigen_sparse \
    -e eigen_svd_cdouble -e eigen_svd_cdouble_sve \
    -t "${FULL_T}" -n 1 <extra args>       # -n 1 → 无 ULP 假 FAIL
```
（实测验证：4 测试 `-n 1` 全 pass；`eigen_svd_cdouble_sve` 无 SVE 机器自动 skip）

**参数规则**：
- `full` 必须是第一个参数（`"$1" = "full"` 时拦截），其余参数原样透传。
- 透传参数中若用户自带 `-t`，优先于默认 60s（从透传参数里 grep `-t`/
  `--test-time` 判定，有则不再注入默认值——实测重复 `-t` 后者生效，但显式
  判定更清晰且避免依赖该未文档化行为）。
- 第二段固定 `-n 1`：这是数值正确性要求（CLAUDE.md 平台特性），**不**接受
  透传的 `-n` 覆盖 flaky 段的线程数（若用户传了 `-n`，只作用于第一段；
  文档中说明）。实际上更简单：透传参数里出现的 `-n`/`--threads` 原样进
  第一段，第二段永远追加 `-n 1` 在末尾（后出现者优先，实测确认）。
- 不注入 `--ignore-timeout` 等：保持与现场真实运行一致。
- 二进制退出码：第一段非零（真 FAIL）时仍继续跑第二段（让 flaky 段也完成），
  最终以**第一段与第二段退出码的或**作为脚本退出码（`exec` 不再适用，改
  普通调用 + `exit`）。日志输出到 stdout（用户可自行 `-o` 重定向）。

### 15 份产物的刷新方式

`run-sdcshield.sh` 改的是文本脚本，**不涉及二进制重编**——不需要重跑
`build-all.sh`（BUILD-HASH 不变：它只含源码树/容器构建脚本哈希）。
刷新方式：直接用新的 heredoc 内容重写 15 份
`third-party/rpms/openEuler-XX.03/*/built/run-sdcshield.sh`（与生成器输出
字节一致），MANIFEST.tsv 中该文件的 sha256/size 行同步更新，然后按 CLAUDE.md
自动推送规范在 3 个 RPM submodule 各提交一次、主仓提交指针。

> 不走 `package-built-artifacts.sh` 全量重跑：它会重拷/strip 二进制并重算
> 所有元数据，还依赖 podman 镜像在场；而本次只有 541 字节的脚本变化。
> 刷新脚本用一段 bash 循环对 15 个目录重写文件 + `sed` 更新 MANIFEST.tsv
> 对应行 + `sha256sum` 校验（见 Task 2）。

## One-patch-per-unit 分解

按 CLAUDE.md 补丁纪律，一个 unit 一个 commit，顺序执行。

### Task 1: 生成器 heredoc 加 full 选项 ✅ (2026-09-15 完成, commit 0c61a655)

文件变更：
- `scripts/offline-build/package-built-artifacts.sh`：重写第 162-171 行的
  heredoc（`cat > "$OUTDIR/run-sdcshield.sh" <<'RUN_EOF' … RUN_EOF`）为带
  `full` 两段式选项的新版本；同步更新文件头部注释（第 9-14 行产物结构说明
  里 `run-sdcshield.sh` 一行注明 full 选项）。

新 heredoc 内容（约 40 行）：
```bash
#!/bin/bash
# run-sdcshield.sh — 在目标机上运行随包的 sdcshield 二进制。
# 自动设置 LD_LIBRARY_PATH 指向随包 libs/ 目录。
#
# 用法:
#   ./run-sdcshield.sh [sdcshield 参数...]     原样透传
#   ./run-sdcshield.sh full [参数...]          全核满载 eigen 运算:
#     第一段: 11 个稳定 eigen 测试, 不带 -n (默认全部 CPU 满载)
#     第二段: 4 个数值敏感测试 (eigen_svd_double/eigen_sparse/
#             eigen_svd_cdouble/eigen_svd_cdouble_sve) -n 1 补跑,
#             避免大规模多线程下 ULP 级偶发假 FAIL (平台已知特性)
#   可透传 -t <time> 覆盖默认每测试 60s (如 full -t 120s);
#   透传的 -n 只作用于第一段, 第二段恒为 -n 1。
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export LD_LIBRARY_PATH="$SCRIPT_DIR/libs${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
# 20.03 的二进制 RPATH 指向 /opt/openEuler/gcc-toolset-10/root/usr/lib64;
# 若目标机没装 toolset, 上面的 libs/ 提供了同名库, LD_LIBRARY_PATH 优先于 RPATH。

if [ "$1" = "full" ]; then
    shift
    FULL_T="60s"
    # 用户自带 -t/--test-time 则不注入默认值
    for a in "$@"; do
        case "$a" in -t|--test-time|-t*|--test-time=*) FULL_T="" ;; esac
    done
    [ -n "$FULL_T" ] && set -- -t "$FULL_T" "$@"
    RC=0
    # 第一段: 11 个稳定 eigen 测试, 全核满载 (无 -n → 默认所有 CPU)
    "$SCRIPT_DIR/sdcshield" -e 'eigen*' \
        --disable eigen_svd_double --disable eigen_sparse \
        --disable eigen_svd_cdouble --disable eigen_svd_cdouble_sve \
        "$@" || RC=$?
    # 第二段: 数值敏感的 4 个测试, 单线程补跑 (-n 1 追加在后, 优先于透传 -n)
    "$SCRIPT_DIR/sdcshield" -e eigen_svd_double -e eigen_sparse \
        -e eigen_svd_cdouble -e eigen_svd_cdouble_sve \
        "$@" -n 1 || RC=$?
    exit "$RC"
fi

exec "$SCRIPT_DIR/sdcshield" "$@"
```

验证（全部真实执行）：
1. `bash -n scripts/offline-build/package-built-artifacts.sh` — 语法检查。
2. 本机直接执行新生成的 run-sdcshield.sh 逻辑：
   - `SP3BUILT=third-party/rpms/openEuler-24.03/openEuler-24.03LTS_SP3/built`
     手动渲染 heredoc 到临时文件（或 Task 2 完成后直接用产物），
     `bash -n` 通过。
   - 用 build-out 的二进制 + 该 SP3 的 libs 组装临时目录，
     `./run-sdcshield.sh --list-tests | wc -l` = 221（原路径不回归）。
   - `./run-sdcshield.sh full -t 2s` → 两段都执行：第一段 11 个测试
     全核（test-plans 无 -n 限制）、第二段 4 个测试 threads:1；
     `exit: pass`（本机 128 核实测）。`eigen_svd_cdouble_sve` 在本机
     （无 SVE）skip 属预期。
   - `./run-sdcshield.sh full -t 2s -n 4` → 第一段 4 线程、第二段仍 1 线程。
   - `./run-sdcshield.sh -e zstd19 -t 2s -n 1` → `exit: pass`（回归检查）。

### Task 2: 刷新 15 份 built/run-sdcshield.sh + MANIFEST.tsv ✅ (2026-09-15 完成)

文件变更：
- 15 份 `third-party/rpms/openEuler-{20.03,22.03,24.03}/openEuler-*LTS*/built/run-sdcshield.sh`
  重写为与 Task 1 生成器输出**字节一致**的内容。
- 15 份对应 `built/MANIFEST.tsv`：`run-sdcshield.sh` 行的 sha256/size 列更新
  （其他行不动）。
- `chmod +x` 保持。

执行方式（一次性 bash，不落新脚本文件——避免给仓里添一次性工具）：
```bash
# 从生成器精确提取 heredoc 渲染结果（RUN_EOF 之间内容 + 前后行）,
# 对 15 个 built/ 重写; 再 sed 更新 MANIFEST.tsv 的 run-sdcshield.sh 行
for d in third-party/rpms/openEuler-{20.03,22.03,24.03}/openEuler-*LTS*/built; do
    # (用与 Task 1 相同的 heredoc 内容重写 $d/run-sdcshield.sh)
    # MANIFEST 行更新: awk 重写 file=run-sdcshield.sh 行的 sha256+size
done
```
实际实现：在 Task 1 的验证步骤里已把渲染好的脚本内容放在临时文件，这里
`cp` 到 15 处 + `awk` 更新 MANIFEST（保持 TSV 列结构
`file\tsha256\tsize\tsource`）。

验证：
1. `md5sum` 15 份新脚本全部一致；与生成器渲染输出 diff 为空。
2. 15 份 MANIFEST.tsv 中 `run-sdcshield.sh` 行的 sha256 与实际文件
   `sha256sum` 一致（脚本断言循环，全部打印 ✓）。
3. 其余行（sdcshield、libs/*）未被改动（`git -C submodule diff` 只含
   run-sdcshield.sh + MANIFEST.tsv 两文件）。
4. 抽 1 个 SP（24.03 SP3，本机可跑）做真实运行验证：
   `./run-sdcshield.sh full -t 2s` 两段全 pass（同 Task 1 验证 2）。

### Task 3: 提交推送（3 个 submodule + 主仓）+ 文档同步 ✅ (2026-09-15 完成)

按 CLAUDE.md 自动推送规范（feature 分支、不推 main）：

主仓当前在 `main`（已与 origin/main 同步），先切分支
`feat/run-sdcshield-full-option`。

1. **主仓 Task 1 commit**：`scripts/offline-build/package-built-artifacts.sh`
   （Task 1 完成即提交推送——一个 unit 一个 commit，不等 Task 2）。
2. **3 个 submodule**（Task 2 完成后）：每个
   `git add built/ && git commit -m "built: add full option to run-sdcshield.sh (all-core eigen soak)"`
   + `git push origin HEAD:main`（三个 submodule 均已在 main 且与 origin
   同步，快进推送；release-all.sh 的推送惯例就是推 submodule 的 main）。
3. **主仓 Task 2/3 commit**：submodule 指针 + 刷新用的一次性变更不落仓
   （15 份产物在 submodule 里），主仓 commit 含：
   - 3 个 submodule 指针 bump
   - 文档更新（见下）
   commit message: `feat(offline-build): add full option to run-sdcshield.sh + refresh 15-SP artifacts`
4. **文档同步**（CLAUDE.md 大颗粒度修改要求，100% 准确）：
   - `README.md`「多版本一键构建与部署」节：目标机用法示例补一行
     `./run.sh full -t 120s   # 全核满载 eigen 运算`。
   - `docs/multi-version-build-deploy-usermanual.md`：部署/运行章节补
     `full` 选项说明（两段式语义、-t 默认 60s、-n 只作用第一段）。
   - `docs/multi-version-build-deploy.md` 5.3 run.sh 骨架附近：补 full
     选项一句话说明（指向 run-sdcshield.sh 的实现）。
   - `scripts/offline-build/README.md`：产物结构说明里 run-sdcshield.sh
     一行注明 full。
5. **推送**：主仓 `git push`（feature 分支）。

## 验证总表（每 task 完成前逐项真实执行并引用输出）

| Task | 验证命令 | 期望 |
|---|---|---|
| 1 | `bash -n scripts/offline-build/package-built-artifacts.sh` | 无输出（语法 OK） |
| 1 | 临时组装 + `./run-sdcshield.sh --list-tests \| wc -l` | 221 |
| 1 | `./run-sdcshield.sh full -t 2s`（128 核机） | 两段 `exit: pass`，第一段全核、第二段 threads:1 |
| 1 | `./run-sdcshield.sh full -t 2s -n 4` | 第一段 4 线程，第二段 1 线程 |
| 1 | `./run-sdcshield.sh -e zstd19 -t 2s -n 1` | `exit: pass`（回归） |
| 2 | `md5sum` 15 份脚本 | 全部一致 |
| 2 | MANIFEST sha256 vs `sha256sum` 断言循环 | 15 × ✓ |
| 2 | `git -C <submodule> diff --stat` | 仅 run-sdcshield.sh + MANIFEST.tsv |
| 3 | `git log --oneline` / push 输出 | 3 submodule + 主仓 feature 分支推送成功 |

## 风险与边界

- **x86-64 非回归**：本变更不触碰任何框架/测试源码，纯离线打包脚本 +
  部署产物，x86 构建路径零影响。
- **`-n` 透传语义**：`-n 0` 透传时第一段会被二进制钳到 1 线程（框架
  Saturate 行为，非本脚本引入）；文档不鼓励 full 模式带 `-n`。
- **`eigen_svd_cdouble_sve`**：无 SVE 机器自动 skip（`CpuNotSupported`），
  满载 soaking 覆盖 13 个实际执行 + mce_check 收尾，属预期而非缺陷。
- **heredoc 渲染一致性**：Task 2 的 15 份拷贝以 Task 1 的 heredoc 输出为
  唯一来源，diff 校验，杜绝手抄偏差。

# CodeQL workflow 大量失败修复（runner OOM）Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 codeql.yaml 63% 的运行失败——根因是 CodeQL 注入构建下重模板 eigen TU 并发内存峰值耗尽 16GB runner，被平台判为 "runner received a shutdown signal"（exit 143），并非机群不稳。

**Architecture:** 两分支走法：先在 `debug/codeql-oom-proof` 分支加实时内存/dmesg 探针复现一次 -j4 死亡（拿直接证据：内存曲线 + 内核 OOM 记录）；再在 `fix/codeql-oom` 分支落地修复（swap 兜底 + `ninja -j2` 限并发 + timeout 余量 + 修正注释里的错误归因），用 PR 触发的多次真实 CI 运行做统计与机制双重验证。

**Tech Stack:** GitHub Actions（codeql-action v3、ubuntu-24.04-arm 4核/16GB）、meson/ninja、GitHub REST API（运行状态取证）。

**Spec:** 本计划自带完整根因证据链（下方"根因证据"节），无独立 spec 文件。

## 根因证据（2026-09-29 实测取证）

1. **失败签名完全一致**：最近 100 个 run 中 62 failure；抽样 14 个失败 run 全部死在 `Build (meson, for CodeQL injection)` 步骤、唯一 failure 注解 `exit code 143`（SIGTERM）、日志尾部 `##[error]The runner has received a shutdown signal`。13/13 个死亡日志的最后一个 ninja 行都在 eigen 模板簇（`eigen_svd_jacobi/*`、`eigen_gemm/*`），死前 slot 周转逐步变慢（3s→5s→9s→36s→∞）——内存压力渐增签名。
2. **单编译器内存实测**（本机 aarch64 GCC 12.3.1，`--buildtype=plain`，/usr/bin/time -v）：
   - BDCSVD 簇 `tests/cpu/eigen_svd/`：svd_cdouble_sve 2795MB/74s、svd_cdouble 2555MB/81s、svd_cdouble_noavx512 2547MB、svd 1927MB、svd_fvectors 1911MB、svd_double 1870MB
   - JacobiSVD 簇 `tests/cpu/eigen_svd_jacobi/`：约 864–922MB / 20–22s ×4 个 TU
   - GEMM 簇：约 448–486MB ×7 个 TU
3. **runner 内存上限**：CodeQL init 自报 `CODEQL_RAM: 14535`（16GB 减系统预留）。
4. **鉴别实验**：multi-os-verify 在**同款** ubuntu-24.04-arm 上编译**同一批** TU（无 CodeQL 跟踪）每 job 失败仅 ~3%；CodeQL job（有 clang 系提取器叠加）63% 失败。x86 机群（同为 16GB）历史上同样死法（7413d7a9→06fb2215）。⇒ 变量是"提取器内存叠加"，与机群、架构无关。
5. **结论**：`ninja` 默认 `-j4` 下 4 条重模板边（编译器 + CodeQL 提取器）并发，峰值超 14.5GB → runner 服务被杀/VM 失响应 → 平台报 shutdown signal。

## Global Constraints

- DCO：`git commit -s`，末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容（无 Co-Authored-By）。
- 只推 feature 分支，禁止推 main；提交前 `git branch --show-current` 确认。
- 一个提交一个单元：workflow 修复、progress-log 文档各成独立提交。
- x86-64 代码零改动（本修复只动 CI YAML 与 docs）。
- 本机多会话共用检出：动手前 `git branch --show-current` + `git log --oneline -1` 校验未被并行会话切换。
- GitHub API 取证用 PAT：`eval "$(grep '^export GITHUB_PERSONAL_ACCESS_TOKEN=' ~/.bashrc)"`（勿用 `source ~/.bashrc`，非交互会提前 return）。

## Review Focus

- swap 步骤在 runner 无磁盘余量/fallocate 失败时不得把 job 拖红（swap 是兜底，`-j2` 才是主修复）——降级路径必须 `::warning` + 继续。
- 已有 swap 的环境不得重复创建（幂等守卫 `swapon --show`）。
- `-j2` 后总时长超 timeout 的风险：预期 ~20min，timeout 升到 60min 留余量，验证时核对真实墙钟。
- 修复后 CI 仍偶发失败的可能性：验证判据是"连续 ≥3 次全绿 + 仪器化运行峰值 <12GB"，若不达标按决策阶梯升级 `-j1`。
- 错误归因（"平台回收/两机群不稳"）残留在 workflow 注释与 progress-log 中——必须一并修正，否则未来维护者会被误导。

---

### Task 1: 复现取证分支（debug/codeql-oom-proof）

**Files:**
- Create: 分支 `debug/codeql-oom-proof`（自 main @ 1d457e0f）
- Modify: `.github/workflows/codeql.yaml`（仅此分支，加探针 + `workflow_dispatch` 触发器；**不修 bug**，保持 -j4 复现）

**Interfaces:**
- Produces: 一份含内存曲线与（若有）内核 OOM 记录的 CI 运行日志，存 `/tmp/oom-proof-<runid>.txt`，供 Task 3 文档引用。

- [ ] **Step 1: 建分支并加探针**

```bash
cd /home/sdc/wangxu/sdcshield && git branch --show-current   # 应为 main
git checkout -b debug/codeql-oom-proof
```

编辑 `.github/workflows/codeql.yaml`：

`on:` 块加 `workflow_dispatch:`（与 push/pull_request/schedule 并列）。

Build 步骤改为（探针部分后台运行、输出走 stdout 实时上云，runner 被杀也不丢现场）：

```yaml
      - name: Build (meson, for CodeQL injection)
        run: |
          # 复现取证:实时内存曲线 + 内核消息流。输出随步骤日志实时上传,
          # 即使 runner 被 OOM 杀掉,死前现场仍在日志里。
          sudo dmesg --follow --notime 2>/dev/null &
          DMESG_PID=$!
          ( while sleep 5; do free -m | sed "s/^/[$(date +%T)] /"; done ) &
          FREE_PID=$!
          PKG_CONFIG_PATH="$PWD/third-party/eigen5" meson setup builddir --buildtype=plain
          ninja -C builddir
          rc=$?
          kill $DMESG_PID $FREE_PID 2>/dev/null || true
          exit $rc
```

- [ ] **Step 2: 校验 YAML 并提交**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/codeql.yaml'))" && echo YAML_OK
git add .github/workflows/codeql.yaml
git commit -s -m "ci(codeql): 复现取证——构建期实时内存曲线与 dmesg 流(仅 debug 分支)"
git push -u origin debug/codeql-oom-proof
```

- [ ] **Step 3: 触发运行并等待**

```bash
eval "$(grep '^export GITHUB_PERSONAL_ACCESS_TOKEN=' ~/.bashrc)"
curl -s -X POST -H "Authorization: token $GITHUB_PERSONAL_ACCESS_TOKEN" \
  -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/wangxumarshall/sdcshield/actions/workflows/codeql.yaml/dispatches" \
  -d '{"ref":"debug/codeql-oom-proof"}' && echo DISPATCHED
# 每 2 分钟轮询一次,直到 conclusion 非 null(约 6-13 分钟)
```

- [ ] **Step 4: 下载日志、提取证据**

用 run id 拉日志（jobs → job id → logs），存 `/tmp/oom-proof-<runid>.txt`，提取：

```bash
grep -E "^\[[0-9:]+\] +Mem|Out of memory|oom-kill|Killed process" /tmp/oom-proof-*.txt | tail -40
```

预期（若复现）：Mem available 单调下滑至 <1GB → 出现（或来不及出现）oom-kill → `shutdown signal`。若本次幸存（~37% 概率）：重开一次 dispatch 再取证；两次都幸存则记录曲线（应见峰值贴近 14.5GB 的紧贴天花板形态）直接进 Task 2。

- [ ] **Step 5: 记录结论后停在分支上**（Task 3 完成后再删）

### Task 2: 修复本体（fix/codeql-oom）

**Files:**
- Create: 分支 `fix/codeql-oom`（自 main @ 1d457e0f）
- Modify: `.github/workflows/codeql.yaml`

**Interfaces:**
- Consumes: Task 1 的证据结论（写进提交信息与注释）。
- Produces: 修复后的 workflow；PR `fix/codeql-oom → main`。

- [ ] **Step 1: 建分支**

```bash
git checkout main && git checkout -b fix/codeql-oom
```

- [ ] **Step 2: 修改 workflow（四处，一次成型）**

(a) 头部注释（第 3–10 行）替换为：

```yaml
# C/C++ 静态分析,排除 third-party/ vendored 代码去噪。
# meson 项目 autobuild 不可靠,显式 meson 构建供注入(eigen5 为 in-repo
# vendored,无需 submodule)。
# 跑 arm64 裸 VM(ubuntu-24.04-arm):本项目全线 arm64-only(尤其 Kunpeng),
# SVE 等 aarch64 专属测试直接进分析库。
#
# 2026-09-29 根因勘定:历史 62% 失败并非"平台回收机群"——是 CodeQL 跟踪
# 构建下重模板 eigen TU 并发内存峰值超限(runner 16GB,CODEQL_RAM=14535)。
# 实测单编译器峰值:BDCSVD 簇 1.8-2.7GB、JacobiSVD 簇 ~0.9GB、GEMM ~0.5GB;
# ninja 默认 -j4 时 4 边并发(编译器+clang 系提取器叠加)峰值>14.5GB → runner
# 服务被杀,表现为 "received a shutdown signal" + exit 143。同款 runner 无
# CodeQL 的 multi-os-verify 编译同一批 TU 每 job 仅 ~3% 失败;x86 机群(同
# 16GB)历史同样死法——变量是提取器内存,与机群/架构无关。
# 对策:swap 兜底 + ninja -j2 限并发(峰值压回 ~13GB 以下)。
```

(b) `timeout-minutes: 45` → `timeout-minutes: 60`（行内注释：`# -j2 构建变慢,预期总时长 ~20min,留 3 倍余量`）。

(c) Build 步骤前新增 swap 步骤：

```yaml
      - name: Add swap (OOM guard)
        run: |
          # 托管 runner 默认无 swap:内存尖峰直接硬杀 runner 服务。加 swap 把
          # 硬杀变成慢速换页;配合 -j2 后尖峰本就罕见,swap 只是兜底,失败降级。
          if swapon --show=NAME --noheadings | grep -q .; then
            echo "swap already active:"; swapon --show; exit 0
          fi
          if ! sudo fallocate -l 12G /swapfile 2>/dev/null; then
            echo "::warning::cannot allocate 12G swapfile — continuing without swap"
            exit 0
          fi
          sudo chmod 600 /swapfile
          sudo mkswap /swapfile
          if ! sudo swapon /swapfile 2>/dev/null; then
            echo "::warning::swapon failed — continuing without swap"
            sudo rm -f /swapfile
            exit 0
          fi
          echo "12G swap enabled:"; swapon --show
```

(d) Build 步骤中 `ninja -C builddir` → `ninja -C builddir -j2`（行内注释：`# -j2:4 边并发(编译器+提取器)峰值超 CODEQL_RAM=14535,2 边压回 ~13GB`）。

- [ ] **Step 3: 本地自检 + 提交推送**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/codeql.yaml'))" && echo YAML_OK
git add .github/workflows/codeql.yaml
git commit -s -m "ci(codeql): 根治构建期 OOM——swap 兜底 + ninja -j2 限并发

62% 失败实为 CodeQL 跟踪构建下重模板 eigen TU 并发峰值超 14.5GB 杀死
runner 服务(非机群回收):BDCSVD 簇单编译器实测 1.8-2.7GB,-j4 时 4 边
编译+提取叠加超限。-j2 压回 ~13GB,12G swap 兜底,timeout 45→60min。
取证:debug/codeql-oom-proof 分支运行日志;对照 multi-os-verify(同款
runner、无 CodeQL)每 job 仅 ~3% 失败。"
git push -u origin fix/codeql-oom
```

- [ ] **Step 4: 开 PR 触发验证运行**

```bash
eval "$(grep '^export GITHUB_PERSONAL_ACCESS_TOKEN=' ~/.bashrc)"
curl -s -X POST -H "Authorization: token $GITHUB_PERSONAL_ACCESS_TOKEN" \
  -H "Accept: application/vnd.github+json" \
  "https://api.github.com/repos/wangxumarshall/sdcshield/pulls" \
  -d '{"title":"ci(codeql): 根治构建期 OOM(swap + -j2)","head":"fix/codeql-oom","base":"main","body":"根因与证据见提交信息。验证判据:连续 ≥3 次全绿。"}' | python3 -c "import json,sys; print(json.load(sys.stdin)['html_url'])"
```

- [ ] **Step 5: 验证判据（统计 + 墙钟）**

对 PR 的 CodeQL run 轮询（API 同 Task 1 Step 3）。通过标准：
- 连续 ≥3 次 `success`（3 次全绿在未修复的 63% 失败率下概率仅 ~5%）；
- Build 步骤墙钟 ≤ 20min、总时长 ≤ 25min（防 -j2 拖慢超预期）；
- swap 步骤日志确认 `12G swap enabled`（或记录降级告警）。

若仍出现 Build 步骤 eigen 区失败：升级 `ninja -j1`（提交同分支），重走 Step 5。

### Task 3: 文档同步与收尾

**Files:**
- Modify: `docs/research/progress-log.md`（新增 2026-09-29 会话条目，修正"平台回收"旧归因）
- Delete: 远端 `debug/codeql-oom-proof` 分支

**Interfaces:**
- Consumes: Task 1 证据文件 `/tmp/oom-proof-*.txt` 摘要、Task 2 验证数据（run id 与结论）。
- Produces: 修正后的历史记录。

- [ ] **Step 1: progress-log 增补**（文件顶部最新会话区，倒序格式与既有条目一致）：根因勘定过程（5 条证据）、修复内容、验证数据（run id + 连续绿次数）、指出 2026-09-25 条目中"平台侧事件/回收"归因有误及正确解读。

- [ ] **Step 2: 提交推送并清理**

```bash
git checkout fix/codeql-oom
git add docs/research/progress-log.md
git commit -s -m "docs(progress): CodeQL 失败根因勘定为构建期 OOM,修正回收误判"
git push
eval "$(grep '^export GITHUB_PERSONAL_ACCESS_TOKEN=' ~/.bashrc)"
curl -s -X DELETE -H "Authorization: token $GITHUB_PERSONAL_ACCESS_TOKEN" \
  "https://api.github.com/repos/wangxumarshall/sdcshield/git/refs/heads/debug/codeql-oom-proof"
```

- [ ] **Step 3: 请求用户审阅合并 PR**（main 有分支保护，由用户决定合并时机）。

# DCO 豁免 Copilot Autofix 机器提交（根治 alert-autofix-* PR git-sanity 失败）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修改 `.github/scripts/check-git-history.sh`，豁免 GitHub Copilot Autofix（code scanning "Security and quality" 告警自动修复）机器提交的 `Signed-off-by` 要求，使未来 `alert-autofix-*` PR 的必需检查 `git-sanity` 不再失败、merge 不再被阻。

**Architecture:** 只改一个 bash 脚本 + 文档同步，不新增 workflow、不新增 secret。识别采用**双标记**：committer 为 `GitHub <noreply@github.com>` **且**提交消息含 `github-advanced-security[bot]@users.noreply.github.com` trailer，二者缺一不可（单纯伪造 trailer 不够；但两者均为无认证元数据、均可本地伪造——残余风险与改动前的假 Signed-off-by 等价，merge 审查才是真实认证）。命中则输出 `::notice`（可审计）并放行；否则维持原 `::error`。人写提交的行为完全不变——人工在 merge 时对机器提交作出接受决定即视为认证。

**Tech Stack:** bash（`.github/scripts/check-git-history.sh`）；本地测试用 scratch-repo harness（`mktemp -d` + git plumbing，精确模拟 CI 的 merge-ref 场景：`refs/remotes/origin/main` + `git merge --no-ff` 造双亲 merge SHA）。

**Spec:** 本计划内嵌（无独立 spec 文档）。设计依据为本会话实证调查：
- main 分支保护必需检查共 10 个 context，含 `git-sanity`（真实阻塞 merge，非噪声）；
- **GITHUB_TOKEN 推送不会触发 CI 重跑**（GitHub 递归防护）→ "CI 内自动补签再推"方案会让 PR 停在 "waiting for status"，比不修更糟，直接排除；
- 仓库 Actions secrets 仅 `GHCR_TOKEN`，无 PAT → "PAT 自动补签推送"方案需新增凭据管理面（且扩大 zizmor/gitleaks 扫描面），弃；
- `pr.yaml` 的 `git-sanity` job 以 merge-ref（`refs/pull/N/merge`）checkout（`fetch-depth: 0`）运行——脚本取自 merge commit，即 **main 当前版本**。豁免逻辑合入 main 后，未来所有 PR（含 autofix PR）立即生效，无需改 workflow；
- 真实 autofix 提交形态（PR #193/#194 实测）：Author=仓库主 noreply，Committer=`GitHub <noreply@github.com>`，消息含 `Co-authored-by: Copilot Autofix powered by AI <62310815+github-advanced-security[bot]@users.noreply.github.com>`；
- 背景关联：progress-log 记录的 "CodeQL 2 个 integer-multiplication-cast 告警（eigen_svd common 头）" 正是 PR #193/#194 要修的——autofix PR 是消化这些告警的载体，必须保持可合并，不可一关了之。

## Global Constraints

- **一补丁一单元**：本计划全部改动（脚本 + README + CONTRIBUTING + ci-web-operations（如涉及）+ progress-log + 本计划文件）为**同一个 commit**（同一单元：DCO 豁免策略及其文档记录）。
- 提交格式：`git commit -s`，尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，其后无任何内容、无 Co-Authored-By；提交前 `git branch --show-current` 确认。
- 分支 `ci/dco-exempt-copilot-autofix`（自最新 `origin/main` 建）；推 feature 分支，**绝不推 main**；PR 合并后生效。
- 脚本注释用英文（匹配 `check-git-history.sh` 现有风格）；README/docs 中文。
- **x86-64 untouched**：本单元仅改 `.github/` 与 `docs/`，不触任何架构代码（vacuous，如实声明即可）。
- 验证 100% 真实：harness 六用例引用真实输出；`ninja -C builddir` 无新警告（无源码改动应为 no-op）；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`；codespell 通过。
- 工作区现状：当前停在 `alert-autofix-22` 本地分支（前序 PR 修复遗留），开工前切回并从 `origin/main` 建新分支；untracked 文件（`campaign_*`、`scripts/run/run_sdc_supplement.sh`）不受影响、勿动。

## Review Focus

（最可能咬人的输入类别 → 钉住它的测试用例）

1. **伪造 trailer**：人类本地提交带 bot 的 Co-authored-by 但 committer 非 GitHub → 必须仍报错（双标记缺一不可）→ Case 4。
2. **已签名人提交**：豁免逻辑不得改变原路径 → 通过且无 notice → Case 3。
3. **混合 PR**（autofix 提交 + 人类未签名提交同分支）→ 逐提交判定，人类那条仍报错 → Case 5。
4. **分支含 merge commit**：未触碰的 no-merge 规则必须原样生效 → Case 6。
5. **`origin/<target>` 解析**：脚本第 22 行用 `origin/${target}` 求 merge-base，harness 必须 `git update-ref refs/remotes/origin/main`，否则 base 计算错、用例误绿/误红 → 贯穿全部用例的 `check()`。

---

### Task 1: 失败测试先行——本地 harness（六用例）

**Files:**
- Create: `/tmp/dco-harness.sh`（scratch，不入库；完整内容如下，执行时原样落盘）
- Test: 即本 harness（repo 无 CI 脚本测试设施，YAGNI 不新建永久测试文件；harness 全文沉淀在本计划与 progress-log，可随时复跑）

**Interfaces:**
- Consumes: `.github/scripts/check-git-history.sh <target> <merge_sha>` 既有 CLI。
- Produces: `check()` 约定输出变量 `OUT`/`RC`；`expect <case> <expected_rc> <must_contain> <must_not_contain>` 断言助手；`BOT_TRAILER` 常量（Task 2 的实现必须让 Case 2/5 中的 autofix 提交被豁免）。

- [ ] **Step 1: 落盘 harness**

```bash
cat > /tmp/dco-harness.sh <<'HARNESS'
#!/bin/bash
# DCO exemption harness — locally simulates CI's merge-ref invocation of
# check-git-history.sh. Usage: bash dco-harness.sh /path/to/repo
set -u
SCRIPT="$1/.github/scripts/check-git-history.sh"
BOT_TRAILER="Co-authored-by: Copilot Autofix powered by AI <62310815+github-advanced-security[bot]@users.noreply.github.com>"
FAILED=0

setup() {  # fresh repo: main holds one base commit; caller lands on branch "feature"
  T=$(mktemp -d /tmp/dco-case.XXXXXX)
  cd "$T" || exit 9
  git init -q -b main .
  git config user.name "Human Dev"
  git config user.email "dev@example.com"
  git config commit.gpgsign false
  echo base > f.txt && git add f.txt && git commit -qm "base"
  git checkout -qb feature
}

# run the script exactly as CI does: origin/main is the PRE-merge target tip,
# the merge SHA is a temporary two-parent commit (refs/pull/N/merge analogue)
check() {  # sets OUT and RC
  git checkout -q main
  git update-ref refs/remotes/origin/main refs/heads/main
  git merge -q --no-ff feature -m "merge feature into main"
  local merge
  merge=$(git rev-parse HEAD)
  OUT=$(bash "$SCRIPT" main "$merge" 2>&1)
  RC=$?
}

expect() {  # $1=case $2=expected_rc $3=must_contain $4=must_not_contain(""=any)
  local case_name=$1 erc=$2 want=$3 avoid=$4
  if [[ $RC -eq $erc && "$OUT" == *"$want"* ]] && { [[ -z "$avoid" ]] || [[ "$OUT" != *"$avoid"* ]]; }; then
    echo "PASS: ${case_name} (rc=$RC)"
  else
    echo "FAIL: ${case_name} (rc=$RC, want rc=$erc)"
    echo "--- output ---"; echo "$OUT"; echo "--------------"
    FAILED=1
  fi
}

# Case 1: unsigned human commit -> error
setup
echo c1 > f.txt && git add f.txt && git commit -qm "human unsigned"
check
expect "case1-unsigned-human-errors" 1 "does not contain Signed-off-by" ""

# Case 2: Copilot Autofix commit (GitHub committer + bot trailer) -> exempt
setup
echo c2 > f.txt && git add f.txt
GIT_COMMITTER_NAME="GitHub" GIT_COMMITTER_EMAIL="noreply@github.com" \
  git commit -qm "autofix without signoff

$BOT_TRAILER"
check
expect "case2-autofix-exempt" 0 "Copilot Autofix" "::error"

# Case 3: signed human commit -> pass, no notice, no error
setup
echo c3 > f.txt && git add f.txt && git commit -qsm "human signed"
check
expect "case3-signed-human-passes" 0 "" "::notice"

# Case 4: spoofed trailer with local committer -> still error
setup
echo c4 > f.txt && git add f.txt
git commit -qm "spoofed trailer

$BOT_TRAILER"
check
expect "case4-spoof-still-errors" 1 "does not contain Signed-off-by" ""

# Case 5: mixed PR: autofix commit + unsigned human commit -> errors on the human one
setup
echo c5a > f.txt && git add f.txt
GIT_COMMITTER_NAME="GitHub" GIT_COMMITTER_EMAIL="noreply@github.com" \
  git commit -qm "autofix without signoff

$BOT_TRAILER"
echo c5b > g.txt && git add g.txt && git commit -qm "human unsigned"
check
expect "case5-mixed-pr-errors-on-human" 1 "does not contain Signed-off-by" ""

# Case 6: merge commit on the branch still rejected (no-merge rule untouched)
setup
git checkout -qb side
echo side > s.txt && git add s.txt && git commit -qsm "side signed"
git checkout -q feature
echo c6 > f.txt && git add f.txt && git commit -qsm "feature signed"
git merge -q --no-ff side -m "merge side into feature"
check
expect "case6-merge-on-branch-rejected" 1 "contains merge commits" ""

cd /
echo
if [[ $FAILED -eq 0 ]]; then echo "ALL 6 CASES PASS"; else echo "SOME CASES FAILED"; exit 1; fi
HARNESS
```

- [ ] **Step 2: 从最新 origin/main 建工作分支**

```bash
git fetch origin main
git checkout -B ci/dco-exempt-copilot-autofix origin/main
git branch --show-current   # 必须输出 ci/dco-exempt-copilot-autofix
```

- [ ] **Step 3: 跑 harness，确认失败用例恰为预期（红灯先行）**

Run: `bash /tmp/dco-harness.sh /home/sdc/wangxu/sdcshield`
Expected: Case 1/3/4/6 PASS（现状行为），**Case 2 FAIL**（现状把 autofix 提交报 `does not contain Signed-off-by`，rc=1）、**Case 5 PASS**（现状混合 PR 也报错，rc=1——它此刻"碰巧"通过，实现后必须仍通过且报错条目只剩人类提交）。整体退出码非 0。把真实输出贴进验证记录。

### Task 2: 最小实现——check-git-history.sh 豁免逻辑

**Files:**
- Modify: `.github/scripts/check-git-history.sh:32-39`（signoff 循环段，前置插入函数）

**Interfaces:**
- Consumes: Task 1 的 `BOT_TRAILER` 形态、真实提交的 committer 格式 `%cn <%ce>` → `GitHub <noreply@github.com>`。
- Produces: `commit_is_copilot_autofix <sha>`（返回 0=豁免，非 0=不豁免）；新增输出行 `::notice:: Commit <sha> has no Signed-off-by but is a Copilot Autofix commit (github-advanced-security[bot]) - exempt.`（Case 2/5 断言依据）。

- [ ] **Step 1: 应用如下 diff（旧循环整段替换为新函数 + 新循环）**

旧（现行 32-39 行）：

```bash
# look sign offs in all the non-merge commits
for sha in $(git log --no-merges --format=%H ${base}..${right}); do
    signoff=$(git show -s --format=%B ${sha} | grep '^Signed-off-by:')
    if [[ -z "${signoff}" ]]; then
        echo "::error:: Commit ${sha} does not contain Signed-off-by. Rebase and amend with 'git commit --amend --signoff'."
        err=1
    fi
done
```

新：

```bash
# Copilot Autofix commits (code scanning "Security and quality" alerts, pushed
# by GitHub on alert-autofix-* branches) are created by the GHAS bot and cannot
# carry a human Signed-off-by at creation time; the human certifies them by
# merging the PR. Identified by BOTH markers: committed by GitHub on behalf of
# the bot AND carrying the bot's co-authored-by trailer. The committer check
# keeps a locally crafted trailer from spoofing the exemption.
commit_is_copilot_autofix() {
    local sha=$1
    local committer trailer
    committer=$(git show -s --format='%cn <%ce>' ${sha})
    trailer=$(git show -s --format=%B ${sha} | grep -F 'github-advanced-security[bot]@users.noreply.github.com')
    [[ "${committer}" == "GitHub <noreply@github.com>" && -n "${trailer}" ]]
}

# look sign offs in all the non-merge commits
for sha in $(git log --no-merges --format=%H ${base}..${right}); do
    signoff=$(git show -s --format=%B ${sha} | grep '^Signed-off-by:')
    if [[ -z "${signoff}" ]]; then
        if commit_is_copilot_autofix ${sha}; then
            echo "::notice:: Commit ${sha} has no Signed-off-by but is a Copilot Autofix commit (github-advanced-security[bot]) - exempt."
        else
            echo "::error:: Commit ${sha} does not contain Signed-off-by. Rebase and amend with 'git commit --amend --signoff'."
            err=1
        fi
    fi
done
```

- [ ] **Step 2: 跑 harness 全量，六用例全绿**

Run: `bash /tmp/dco-harness.sh /home/sdc/wangxu/sdcshield`
Expected: `ALL 6 CASES PASS`，退出码 0。Case 5 输出须含 notice（autofix 条目）与 error（人类条目）并存、rc=1。引用真实输出。

### Task 3: 文档同步（README / CONTRIBUTING / ci-web-operations / progress-log）

**Files:**
- Modify: `README.md`（「PR CI 门禁、安全扫描与基准趋势」节，`git 历史/DCO` 字样行）
- Modify: `CONTRIBUTING.md:10-14`（Certificate of Origin 节）
- Modify: `docs/build-deploy/ci-web-operations.md`（仅当 grep 命中 DCO/git-sanity/签名相关描述时）
- Modify: `docs/research/progress-log.md`（按文件尾现有格式追加结论行）

**Interfaces:**
- Consumes: Task 2 的最终行为（双标记、notice 输出、人工提交不变）。
- Produces: 文档描述与脚本行为 100% 一致（CLAUDE.md 规则 7）。

- [ ] **Step 1: README CI 段——`→ git 历史/DCO →` 改为**

旧：`→ git 历史/DCO →`
新：`→ git 历史/DCO（人工提交必须 Signed-off-by；GitHub 生成的 Copilot Autofix 机器提交豁免——双标记识别 GitHub 提交者 + `github-advanced-security[bot]` trailer，git-sanity 输出 notice 可审计）→`

- [ ] **Step 2: CONTRIBUTING.md 在 Certificate of Origin 节（"project." 之后、"## Commit message format" 之前）插入**

```markdown
Commits created by GitHub's Copilot Autofix for code scanning alerts (machine
commits on `alert-autofix-*` branches) are exempt: they cannot carry a human
sign-off at creation time and are certified by the maintainer when the pull
request is merged. The CI check identifies them by their GitHub committer and
the `github-advanced-security[bot]` co-authored-by trailer (see
`.github/scripts/check-git-history.sh`).
```

- [ ] **Step 3: ci-web-operations.md 条件同步**

Run: `grep -n "git-sanity\|DCO\|Signed-off\|sign-off" docs/build-deploy/ci-web-operations.md`
命中则在该文档描述分支保护必过检查处补一句豁免说明（同 Step 1 措辞精简版）；未命中则跳过并在验证记录注明"grep 未命中，无需同步"。

- [ ] **Step 4: progress-log 追加结论行（按现有条目格式）**

在文件尾部合适的 dated 小节追加（措辞按现场格式微调，事实保持）：

```
- 2026-09-26: PR #193/#194（Copilot Autofix 修 eigen_svd CodeQL 告警）缺签致
  git-sanity 必过检查失败，已手动补签强推（git-sanity 绿）；根治方案
  ci/dco-exempt-copilot-autofix：check-git-history.sh 双标记豁免 autofix
  机器提交（GITHUB_TOKEN 推送不触发 CI 重跑、仓库无 PAT secret，故弃
  CI 自动补签路线）
```

### Task 4: 全量验证 + 单 commit + push + PR

**Files:**
- Commit: 上述全部改动 + 本计划文件
- Ship: 分支 `ci/dco-exempt-copilot-autofix` → GitHub PR

**Interfaces:**
- Consumes: Task 1-3 全部产物。
- Produces: 一个 PR；合入 main 后未来 autofix PR 的 `git-sanity` 即时豁免生效。

- [ ] **Step 1: 规则 2 验证四件套（引用真实输出）**

```bash
ninja -C builddir                                  # 无源码改动 → no-op、零新警告
./builddir/sdcshield -e zstd19 -t 2000 -n 1        # 结尾 exit: pass
~/.local/bin/codespell                             # 拼写检查通过（CI lint 同款）
bash /tmp/dco-harness.sh /home/sdc/wangxu/sdcshield # ALL 6 CASES PASS
```

x86-64 non-regression by inspection：改动仅 `.github/` 与 `docs/`，无任何架构代码路径——vacuous，如实声明。

- [ ] **Step 2: 提交（先确认分支）**

```bash
git branch --show-current        # 必须是 ci/dco-exempt-copilot-autofix
git add .github/scripts/check-git-history.sh README.md CONTRIBUTING.md \
        docs/build-deploy/ci-web-operations.md docs/research/progress-log.md \
        docs/superpowers/plans/2026-09-26-dco-exempt-copilot-autofix.md
git commit -s -m "ci: DCO 豁免 Copilot Autofix 机器提交（双标记识别，根治 alert-autofix-* PR git-sanity 失败）"
git log -1 --format=%B           # 尾行 Signed-off-by: wangxu <wangxumarshall@qq.com>，其后无内容
```

（ci-web-operations.md 未改动则从 `git add` 中去掉；`git status` 确认无遗漏、无误加 untracked。）

- [ ] **Step 3: 推送并开 PR**

```bash
git push -u origin ci/dco-exempt-copilot-autofix
```

PR（curl + PAT，本机无 gh CLI）：标题 `ci: DCO 豁免 Copilot Autofix 机器提交（根治 alert-autofix-* PR git-sanity 失败）`；正文含：背景（PR #193/#194 实例）、方案（双标记 + notice）、被拒备选（GITHUB_TOKEN 自动补签→检查不重跑卡死；PAT secret→新增凭据面）、六用例与四件套验证证据。PR 建成后确认其 `git-sanity` 检查绿（本 PR 人工提交均带 signoff，走原路径）。

---

## 被拒备选（记录供追溯）

1. **CI 内自动补签 + GITHUB_TOKEN 推送**：token 推送不触发 pull_request/push workflow（GitHub 递归防护），PR 新头 SHA 无任何检查记录 → "waiting for status" 永久卡死，比现状更糟。
2. **CI 内自动补签 + PAT secret 推送**：可行但需向仓库注入个人 PAT（新增凭据管理面与泄露面，gitleaks/zizmor 扫描面扩大），且改变提交内容（给机器提交加人签）违背"机器提交由 merge 认证"的更诚实语义。
3. **关闭 Copilot Autofix**：autofix PR 是消化 progress-log 所记 CodeQL integer-multiplication-cast 告警的载体（PR #193/#194 即是），关掉即失去该能力。
4. **仅凭 trailer 豁免（不看 committer）**：本地一行 `git commit -m` 即可伪造豁免，门禁形同虚设——双标记把伪造门槛提高到"同时伪造 GitHub 提交者身份"。注意这是**提高门槛而非防伪造保证**：committer 元数据同样可本地伪造（终审探针实证双伪造可通过豁免）；残余风险与改动前的假 signoff 等价，安全水位不变，真实控制在 merge 审查。

# SDCShield CI 能力补全 Campaign 实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 补全 SDCShield 仓库的 GitHub Actions 能力——4 批 18 任务:快赢、正确性闸门、安全扫描、无人值守闭环与趋势,每项独立 commit,每批一个 feature 分支 + PR。

> **执行状态速览(2026-09-25 14:00 更新)**
>
> | 任务 | 状态 | 落点 |
> |---|---|---|
> | T0 x86 移除 | ✅ merged | PR#159 |
> | T5 警告清零(~30 处/11 commits) | ✅ merged | PR#162 |
> | 批 1: T1 timeout+concurrency / 死文件清理 / typo+codespell / actionlint / T4 issue 模板 | ✅ 6 commits | 分支 `ci/quickwins` |
> | 批 3: T9-T15 zizmor/persist-credentials/权限收紧/gitleaks/osv/codeql/dependabot | ✅ 6 commits,zizmor 全绿 | 分支 `ci/security-scanning`(叠 quickwins) |
> | 批 4: T16 PR 结果矩阵 / T17 benchmark 趋势 / T18 docs+网页清单 | ✅ 4 commits | 分支 `ci/loop-and-trends`(叠 security) |
> | 批 2: T6 --werror 门禁 / T7 sanitizer / T8 paths-filter | ⬜ 待做(T6 前提 T5 已就绪) | — |
>
> 顺序 merge:quickwins → security-scanning → loop-and-trends(分支相叠,无冲突)。
> T6/T7/T8 待 main 的既有红(build-cpu GCC-arm64 / multi-version 20.03+22.03 / nightly 10 job)修复后进行——CI 红期间挂 werror/sanitizer 门禁只会叠加噪声。
> API 配额耗尽(匿名 60/h)致远程诊断中断;本机无 podman 无法复现容器构建。

**Architecture:** 全部改动限于 `.github/`(workflows、composite actions、模板、dependabot)与少量 framework 警告清理。不触碰 x86-64 逻辑(mesh_*.cpp 仅删未用变量,属维护性修复)。CI 改动的验证回路 = feature 分支 push → 开 draft PR → Actions 真跑 → 绿了才 merge。

**Tech Stack:** GitHub Actions(composite actions、matrix、concurrency、job 级 permissions)、codespell 2.4、zizmor 1.30、actionlint、gitleaks-action、osv-scanner-action、CodeQL(cpp)、Dependabot、dorny/paths-filter、benchmark-action/github-action-benchmark。

**Spec:** 无独立 spec 文件——2026-09-25 brainstorming 会话共识,由本文件「背景与决策」一节承载(含本地基线实测数据)。

## 背景与决策(brainstorming 共识,含实测基线)

用户请求:补全仓库未用到的 GitHub Actions 能力。经核实(`.github/` 全树遍历)与本地基线验证,以下决策已定:

| ID | 项 | 决策(依据) |
|----|----|------|
| T0 | **移除 x86 构建(先行)** | **2026-09-25 用户决定:本项目仅支持 arm64,不再构建 x86**(起因:PR #158 的 Clang-x86_64 job 无意义)。build-cpu matrix 删 GCC-x86_64/NoLogging/Clang 3 项,删 build-cpu-win/gpu/idxd 三个 job,unittests 移到 GCC-arm64 项维持覆盖;独立分支 `ci/arm64-only-builds`,先于本计划四批合并。本计划其余任务均已按 T0 后形态修订。 |
| F2+A3 | pr.yaml 资源治理 | 补 `timeout-minutes` + `concurrency`(现挂死白烧 6h、旧 run 不取消) |
| E12 | codespell | 接入但**不扫 docs/**(实测 docs/ 2491 行误报,信噪比不可治);修 framework/ meson.build 真错拼 ~10 处;sme/UE/ist/addin/unx 为术语进 ignore 列表 |
| I2 | actionlint | pr.yaml lint job 加两步(官方安装脚本,不引第三方 action) |
| G2 | issue 模板 | 中文模板,bug 模板强制 `--dump-cpu-info` |
| A2 | --werror | **本地实测 9 处存量警告**(tests/cpu/mesh/×6、framework/device/cpu/topology.cpp×2、kunpeng920_ecc.cpp×1),必须先清再上门禁;CI 侧 x86/gcc-sid 可能另有,红了迭代修 |
| A1 | sanitizer | 本地先验证 ASan/UBSan + unittests + `--quick -n1`;初版 `ASAN_OPTIONS=detect_leaks=0`(fork 型框架,LSan 误报风险);CI matrix 加一项 |
| D10 | paths-filter | `changes` job + multi-version job `if` 条件;filter key 用 `src`(白名单语义,docs-only 跳过) |
| C7-C8,H1-H2 | 安全 | 新 security.yaml(zizmor + gitleaks + osv)+ codeql.yaml + dependabot.yml;zizmor 噪声治理:unpinned-uses 忽略(major tag + Dependabot 管)、template-injection 降 low(无 pull_request_target);artipacked(persist-credentials: false ×10)与 excessive-permissions(job 级收紧)真修 |
| C6 | CodeQL | codeql.yaml 手动构建(meson 项目 autobuild 不可靠),paths-ignore third-party |
| F1 | nightly 通知 | **用户已选 GitHub 原生通知**——零 CI 改动,写网页操作清单 |
| J1 | PR 结果矩阵 | quick 跑 `-o -` → `-o quick.yaml` 落盘(实测该格式与 allquality.yaml 同构),report-summary.py **零改动**复用(目录摆位 `all-results/verify-<matrix.name>/results/allquality.yaml`) |
| B4 | benchmark 趋势 | benchmark.tsv(test/wall_seconds/loop_count)→ 新转换脚本 → benchmark-action `tool: custom` → gh-pages;只取 24.03-LTS-SP3(基准版本) |
| G1 | 分支保护 | 纯网页操作,写清单 |

**基线实测记录(2026-09-25,本机 aarch64 gcc-12.3,openEuler 24.03):**
- `meson setup --werror --buildtype=release` 全量构建失败,9 处 -Werror 级警告:tests/cpu/mesh/{mesh_upi_sse_symm_int.cpp:111(cmp), mesh_upi_avx512_asymm_int.cpp:94(block_ok), mesh_upi_avx2_symm_int.cpp:112-113(loaded0/loaded1), mesh_upi_avx2_asymm_int.cpp:114-115(cmp0/cmp1)} 全为 unused-variable 类;framework/device/cpu/topology.cpp:1609,1646 attributes-ignored;framework/device/cpu/arm64/kunpeng920_ecc.cpp:236 format-truncation(%s 255 字节写入 251 区域)
- **T5 完成修正(2026-09-25)**:上述 9 处只是第一层——每修一批 ninja 推进更远,洋葱共 ~30 处,分 11 个 commit 清零:mesh 探针 maybe_unused ×4 文件、topology [[assume]]→__builtin_unreachable ×2、ecc dimm_name 精度限制 ×1、sandstone_run sysv_abi 条件化 ×1、logging cmdline 拆分追加(GCC12 -Wrestrict 系统头误判的源码级根治)×1、jit x86 辅助 maybe_unused ×3 文件、vmxmsr read_midr_el1(arm64-only 构建,引用全在死码)×1、crt_builtins ×2 双关读改 memcpy(strict-aliasing)、movbe %zu ×1、sve512 孤儿 temp 赋值删除 ×1、sve 探针组 maybe_unused ×4 文件。终态:`ninja -C builddir-werror` 零错误全绿;普通构建零警告;`-e zstd19 -t 2000 -n 1` exit: pass;command-line 输出不变。分支 fix/werror-warnings-cleanup(基于 main,与 T0 无文件交集)。
- `zizmor .github/` = 78 findings:unpinned-uses 52、artipacked 30、template-injection 16、self-repository 12、github-env 6、excessive-permissions 2
- `codespell docs/` = 2491 行(误报为主);`codespell framework/ tests/` = 102 行(真错拼 ~10);`codespell scripts/` = 26 行
- `sdcshield --quick -n1 -e zstd19 -t 200 -o -` 的 stdout yaml 与 report-summary.py 解析格式完全同构(`- test:` / `result:` / `test-runtime:`);`-o FILE` 落盘同格式,且 `-o` 指定的文件全过也不删

## Global Constraints(每任务隐含遵守)

- **一补丁一单元**:每个功能点独立 commit;批 = 分支 = PR;`git commit -s`,末行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,其后无任何行、无 Co-Authored-By(CLAUDE.md 规则 3 明令,优先于默认 attribution)。
- **提交前自验证**:每个 commit 前跑该任务的本地验证命令;CI 类改动的最终验证 = draft PR 上 Actions 全绿(见文末「CI 验证工作流」)。
- **x86-64 untouched**:本 campaign 唯一触及 x86 专属文件的是 tests/cpu/mesh/ 警告清理(删未用变量/加 `[[maybe_unused]]`,零行为变化)。
- **分支纪律**:commit 前 `git branch --show-current` 确认;push 只推 feature 分支,永不 main。
- **每批完成后**:`git push` 分支 → 开 PR → CI 绿 → 用户决定 merge → 下一批。
- 第三方 action 一律用 major tag(`@v3`),由 Dependabot(T15)统一升级——与 zizmor 忽略策略配套。

## Review Focus(本计划最可能翻车的五处及对策)

| # | 风险 | 对策(归属任务) |
|---|------|------|
| 1 | concurrency 误取消非 PR run | pr.yaml `on:` 仅 pull_request,`cancel-in-progress: true` 安全;不改 multi-os-verify 的 concurrency(cancel: false)(T1 内核对) |
| 2 | timeout 过紧误杀 | build-cpu-win 90min、multi-version 90min、其余 45/10min,取实测时长 ×2 余量(T1) |
| 3 | paths-filter 白名单漏路径 → 该跑的 multi-version 被跳过 | filter `src` 覆盖 framework/tests/scripts/third-party/meson.build/bats/.github;CI 验证:故意只改 docs 的分支上 draft PR 观察 skip(T8) |
| 4 | ASan 在 fork 型框架下 LSan 误报/崩溃 | 本地先跑(T7 步骤 1-3),不过 CI;初版 detect_leaks=0 |
| 5 | benchmark auto-push 权限/竞态 | 独立非容器 job + job 级 `contents: write`;T11 收紧在前、T17 最小放开,顺序不可倒(T11/T17) |
| 6 | gitleaks 翻出 fork 历史(OpenDCDiag)遗留泄漏 | 执行时若报:立即停手报告用户,不自行处理(T12) |

---

## 批 1:快赢(分支 `ci/quickwins`,4 任务 6 commits)

### Task 1: pr.yaml 资源治理(timeout + concurrency)

**Files:**
- Modify: `.github/workflows/pr.yaml`

**Interfaces:**
- Produces: 后续所有批次都在此文件上叠加;`concurrency` 块位于 `permissions:` 之后、`jobs:` 之前。

- [ ] **Step 1: 加 concurrency 块**(pr.yaml 的 `on:` 仅 pull_request,新 push 即 synchronize 事件,取消旧的省时长)

在 `permissions:\n  contents: read` 之后插入:

```yaml
# 推新 commit 取消同 PR 旧 run(本 workflow 仅 pull_request 触发,synchronize 必为新 push)
concurrency:
  group: pr-${{ github.event.pull_request.number }}
  cancel-in-progress: true
```

- [ ] **Step 2: 每个 job 加 timeout-minutes**(挂死不再白烧默认 360min)

在 4 个 job(T0 后仅存)的 `runs-on:` 行后各插入一行:

| job | 值 | 依据 |
|-----|-----|------|
| lint | `timeout-minutes: 10` | 两个 checkout + 一个 shell 脚本 |
| git-sanity | `timeout-minutes: 10` | 同上 |
| build-cpu | `timeout-minutes: 45` | apt + 全量构建 + unittests + quick,实测 <20min |
| multi-version | `timeout-minutes: 90` | 首次拉镜像烘 deps ~10min + 构建 + smoke |

格式(以 lint 为例):

```yaml
  lint:
    runs-on: ubuntu-latest
    timeout-minutes: 10
```

- [ ] **Step 3: 本地验证语法**

```bash
python3 -c "import yaml,sys; yaml.safe_load(open('.github/workflows/pr.yaml'))" && echo YAML-OK
~/.local/bin/zizmor .github/workflows/pr.yaml 2>&1 | tail -1   # 不应新增 finding(存量 78 已知)
```

- [ ] **Step 4: Commit + push**

```bash
git branch --show-current   # 确认在 ci/quickwins
git add .github/workflows/pr.yaml
git commit -s -m "ci: add job timeouts and PR concurrency cancellation to pr.yaml

挂死 job 不再白烧平台默认 360 分钟;同 PR 推新 commit 取消旧 run。
各 job 超时取实测时长 ~2 倍余量。"
git push origin ci/quickwins
```

### Task 2: codespell 接入(修真错拼 + 配置 + CI step)

**Files:**
- Create: `.codespellrc`
- Modify: `meson.build`、`framework/interrupt_monitor.hpp`、`framework/sandstone.cpp`、`framework/sandstone_opts.cpp`、`framework/sandstone_run.cpp`、`scripts/eigen-sve-double/test_packet_xd.cpp`、`.github/workflows/pr.yaml`(lint job)

**Interfaces:**
- Produces: `.codespellrc`(codespell 无参数时自动读取);后续 PR 若新增术语,执行者往 `ignore-words-list` 追加。

- [ ] **Step 1: 修真错拼(commit 1)**

逐处确认后修改(注释/字符串 typo,零行为变化):

| 文件:行 | 错 → 对 |
|---------|---------|
| meson.build:248 | accomodates → accommodates |
| framework/interrupt_monitor.hpp:11 | accummulate → accumulate |
| framework/sandstone.cpp:248 | precendence → precedence |
| framework/sandstone.cpp:707 | mutliply → multiply |
| framework/sandstone.cpp:910 | timestaps → timestamps |
| framework/sandstone_opts.cpp:107 | syntethic → synthetic |
| framework/sandstone_run.cpp:1641 | prefered → preferred |

`scripts/eigen-sve-double/test_packet_xd.cpp:93-96` 的 `preverse`×3:先读上下文——若是注释 typo 改之;若是标识符(SVE 代码常用 `preverse` 表示 "pre-reverse"?),不动,进 Step 2 的 ignore 列表。

验证 + 提交:

```bash
ninja -C builddir 2>&1 | tail -1   # 增量构建无新警告
git add -u && git commit -s -m "fix: correct spelling typos in framework and meson.build

codespell 基线扫描发现的 8 处真实错拼(注释/字符串,零行为变化)。"
```

- [ ] **Step 2: 写 .codespellrc + CI step(commit 2)**

`.codespellrc`:

```ini
[codespell]
# docs/ 不扫:翻译/研究大树,实测 2491 行误报(德语 ist、缩写 UE 等),信噪比不可治;
# 后续如需治理 docs 另立专项。构建产物与 vendored 树同样跳过。
skip = .git,third-party,builddir*,compile_commands.json,*.tsv,docs,campaign_*
# 术语白名单:sme=ARM SME 指令集;UE=uncorrected error(EDAC);addin=NIST SP800-90
# additional input;ist=德语文档常见;unx=URL/包名片段
ignore-words-list = sme,ist,unx,UE,addin,preverse
count =
```

(`preverse` 视 Step 1 结论增删。)

pr.yaml `lint` job 的 `steps:` 里,`Check if the PR adds tabs` 步骤之后加:

```yaml
    - name: Run codespell
      run: |
        pipx install codespell
        codespell
```

(pipx 在 ubuntu-latest 预装;.codespellrc 从仓库根自动生效。)

- [ ] **Step 3: 本地验证**

```bash
~/.local/bin/codespell && echo CLEAN   # 期望:无输出,CLEAN
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/pr.yaml'))" && echo YAML-OK
```

- [ ] **Step 4: Commit + push**

```bash
git add .codespellrc .github/workflows/pr.yaml
git commit -s -m "ci: add codespell to PR lint

.codespellrc 跳过构建产物/vendored/docs(误报治理成本 > 收益),
术语白名单;core 源码树与 README 纳入拼写检查。"
git push origin ci/quickwins
```

### Task 3: actionlint 接入

**Files:**
- Modify: `.github/workflows/pr.yaml`(lint job)

- [ ] **Step 1: lint job 加两步**

在 Task 2 的 codespell 步骤之后加:

```yaml
    - name: Install actionlint
      run: bash <(curl -fsSL https://raw.githubusercontent.com/rhysd/actionlint/main/scripts/download-actionlint.bpf)
    - name: Run actionlint
      run: actionlint -shellcheck= -pyflakes= .github/workflows .github/actions
```

(`-shellcheck=` 禁用 shellcheck 联动——run 块多为 bash 逻辑片段,shellcheck 噪声另立专项;`-pyflakes=` 同理。)

- [ ] **Step 2: 本地验证(若网络可达)**

```bash
bash <(curl -fsSL https://raw.githubusercontent.com/rhysd/actionlint/main/scripts/download-actionlint.bpf) && ./actionlint -shellcheck= -pyflakes= .github/workflows .github/actions
```

期望:无输出(退出 0)。若报既有 workflow 问题,逐条修复后再 commit(问题本身是这个任务的价值)。网络不可达则跳过本地,靠 draft PR CI 验证。

- [ ] **Step 3: Commit + push**

```bash
git add .github/workflows/pr.yaml
git commit -s -m "ci: add actionlint to PR lint

GitHub Actions 表达式/语法静态检查,覆盖 workflows 与 composite actions。"
git push origin ci/quickwins
```

### Task 4: issue/PR 模板

**Files:**
- Create: `.github/ISSUE_TEMPLATE/config.yml`、`.github/ISSUE_TEMPLATE/bug_report.md`、`.github/ISSUE_TEMPLATE/feature_request.md`

- [ ] **Step 1: 写模板(中文,与仓库文档语言一致)**

`.github/ISSUE_TEMPLATE/config.yml`:

```yaml
blank_issues_enabled: false
contact_links:
  - name: 文档与用法
    url: https://github.com/wangxumarshall/sdcshield/blob/main/docs/research/usage.md
    about: 全部命令行选项(含未文档化项)见 docs/research/usage.md
```

`.github/ISSUE_TEMPLATE/bug_report.md`:

```markdown
---
name: 缺陷报告
about: 测试失败 / 结果异常 / 构建问题
labels: bug
---

**环境(必填)**

- 版本(`sdcshield --version` 输出):
- CPU/OS(`sdcshield --dump-cpu-info | head -3` 输出):

**复现命令(必填,完整命令行)**

```console
$ ./sdcshield ...
```

**期望结果 / 实际结果**

<!-- 期望:exit: pass;实际:贴 exit 行或失败摘要,不要贴全量日志 -->

**运行日志**

<!-- 失败时日志文件默认保留;若用 -o 指定了路径,附上或粘贴关键段 -->
```

`.github/ISSUE_TEMPLATE/feature_request.md`:

```markdown
---
name: 功能建议
about: 新测试 / 新特性 / CI 与构建改进
labels: enhancement
---

**想解决什么问题**

<!-- 先说问题/场景,再说方案 -->

**建议的做法**

**涉及文件/模块(若已知)**
```

- [ ] **Step 2: 验证 + Commit + push**

```bash
python3 -c "import yaml,glob; [yaml.safe_load(open(f)) for f in glob.glob('.github/ISSUE_TEMPLATE/*.yml')]" && echo YAML-OK
git add .github/ISSUE_TEMPLATE/
git commit -s -m "ci: add issue templates (bug report requires dump-cpu-info and repro command)

bug 模板强制采集 --version / --dump-cpu-info / 完整命令行,
减少 SDC 类报告来回追问。"
git push origin ci/quickwins
```

---

## 批 2:正确性闸门(分支 `ci/correctness-gates`,4 任务 6+ commits)

### Task 5: 清 aarch64 侧存量警告(3 commits)

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_sse_symm_int.cpp`、`tests/cpu/mesh/mesh_upi_avx512_asymm_int.cpp`、`tests/cpu/mesh/mesh_upi_avx2_symm_int.cpp`、`tests/cpu/mesh/mesh_upi_avx2_asymm_int.cpp`、`framework/device/cpu/topology.cpp`、`framework/device/cpu/arm64/kunpeng920_ecc.cpp`

**Interfaces:**
- Produces: `--werror` 全量构建通过(aarch64 本地),是 Task 6 CI 门禁的前置。

- [x] **Step 1: mesh 组 6 处 unused-variable(commit 1)**

对每处先读上下文确认变量确未使用(`unused-but-set` 的 `block_ok` 可能是断言被 NDEBUG 编译掉——若是,加 `(void)block_ok;` 或改用 `__attribute__((unused))`,**不删逻辑**)。修复模式二选一,以最小侵入为准:

```cpp
// 模式 A:变量纯属残留 → 删声明
// 模式 B:仅 debug 构建使用 → 声明处加 [[maybe_unused]]
[[maybe_unused]] int cmp = ...;
```

验证:

```bash
ninja -C builddir-werror 2>&1 | grep -c '错误\|error:'   # mesh 相关 6 条归零(topology/ecc 仍报,下一 commit 修)
```

```bash
git add tests/cpu/mesh/ && git commit -s -m "fix: remove unused variables in mesh tests (-Werror cleanup)

mesh_upi_{sse,avx2,avx512}_{symm,asymm}_int 的 SIMD 分支残留声明;
--werror 门禁前置清理,零行为变化(注:mesh 虽是 x86 专属测试,
arm64 全量构建同样编译这些文件——本地 aarch64 构建警告已证明——故清理仍需)。"
```

- [x] **Step 2: topology.cpp 2 处 attributes-ignored(commit 2)**

读 `framework/device/cpu/topology.cpp:1609,1646` 上下文( GCC 报 "attributes at the beginning of statement are ignored"),典型成因是 `__attribute__((...))` 或 `[[...]]` 放在 label/语句开头。修复:把属性挪到声明处(参考 GCC 文档 attributes 位置规则)。修后:

```bash
ninja -C builddir-werror 2>&1 | grep -c '错误\|error:'   # topology 2 条归零
git add framework/device/cpu/topology.cpp && git commit -s -m "fix: silence attributes-ignored warnings in topology.cpp

属性放置位置修正(-Werror 门禁前置清理)。"
```

- [x] **Step 3: kunpeng920_ecc.cpp format-truncation(commit 3)**

`framework/device/cpu/arm64/kunpeng920_ecc.cpp:236`:`%s` 最多写 255 字节但目标区 251。读代码确认 buffer 语义后,二选一:

```cpp
// 模式 A(推荐,不改 buffer 大小):限制写入宽度
snprintf(buf, sizeof(buf), "%.250s", src);   // 数值按实际字段宽度
// 模式 B:目标 buffer 扩到 ≥ 256(若 buffer 属可扩的本地数组)
```

验证(完整回归,该文件是 mce_check 依赖):

```bash
ninja -C builddir-werror 2>&1 | grep -c '错误\|error:'   # 期望 0,构建全绿
./builddir-werror/sdcshield -e mce_check -t 2000 -n 1 -o - 2>&1 | grep 'exit:'
# 期望: exit: pass
git add framework/device/cpu/arm64/kunpeng920_ecc.cpp && git commit -s -m "fix: silence format-truncation in kunpeng920_ecc.cpp (-Werror cleanup)

%s 输出宽度限制到目标区大小;ARM64 EDAC 路径回归 mce_check pass。"
git push origin ci/correctness-gates
```

### Task 6: pr.yaml 加 --werror 门禁项

**Files:**
- Modify: `.github/workflows/pr.yaml`(build-cpu matrix)

- [ ] **Step 1: matrix 加一项**

在 build-cpu matrix include 列表末尾加(T0 后矩阵仅 GCC-arm64):

```yaml
        - { name: GCC-arm64-werror, archsuffix: "-arm", unittests: "unittests", meson_args: "--werror" }
```

(与 GCC-arm64 项同构 + `--werror`;走 build-cpu composite 的 `meson_args` 透传,buildtype 仍 debugoptimized。本机 gcc-12.3 与 CI ubuntu-24.04-arm 的 gcc-13 警告集可能有差,红了迭代修。)

- [ ] **Step 2: 本地验证 + push 开 draft PR**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/pr.yaml'))" && echo YAML-OK
git add .github/workflows/pr.yaml
git commit -s -m "ci: add --werror matrix gate to build-cpu (arm64)

CLAUDE.md 规则 2 的\"零新警告\"从自觉变 CI 硬门;
aarch64 侧存量警告已清(前置 commit),CI gcc-13 差异红了即修。"
git push origin ci/correctness-gates
```

- [ ] **Step 3: 开 draft PR、看 Actions、迭代修 CI 侧**

流程见文末「CI 验证工作流」。若 GCC-arm64-werror 项红:读 Actions 日志中的具体警告 → 每处一个独立 fix commit(同 Task 5 模式)→ push 直至全绿。**修法只允许删声明/`[[maybe_unused]]`/宽度限制三类零行为模式;需要改逻辑的必须先回报用户。**(本机即 arm64 + gcc-12,绝大多数警告本地可复现验证。)

### Task 7: sanitizer 矩阵项

**Files:**
- Modify: `.github/workflows/pr.yaml`(build-cpu matrix + 运行环境)

- [ ] **Step 1: 本地先证可行(不 commit,只探路)**

```bash
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir-asan --buildtype=debugoptimized -Db_sanitize=address,undefined
ninja -C builddir-asan
ASAN_OPTIONS=detect_leaks=0 ./builddir-asan/unittests && echo UNITTESTS-OK
ASAN_OPTIONS=detect_leaks=0 ./builddir-asan/sdcshield --quick -n1 --retest-on-failure=0 -e zstd19 -t 2000 -o - 2>&1 | grep 'exit:'
# 期望: exit: pass
```

(Sanitizer CI 项 T0 后为 GCC-arm64-Sanitizers,与本机同架构——本地绿的置信度直接迁移。)

- [ ] **Step 2: 决策点**

本地全绿 → 继续 Step 3。**若 ASan/UBSan 报错:停下,把报错原文回报用户**(可能是框架真 bug——那本身是高价值发现;也可能是误报——需要用户决策 detect_leaks/抑制策略)。不允许带着失败上 CI。

- [ ] **Step 3: CI matrix 加一项 + 环境变量**

matrix include 列表加:

```yaml
        - { name: GCC-arm64-Sanitizers, archsuffix: "-arm", unittests: "unittests", meson_args: "-Db_sanitize=address,undefined" }
```

并在 build-cpu job 顶层(`strategy:` 之前)加:

```yaml
    env:
      # fork 型测试框架:子进程异常退出路径多,LeakSanitizer 误报风险高,初版关闭;
      # 待观察期后再评估开启。
      ASAN_OPTIONS: detect_leaks=0
```

- [ ] **Step 4: 验证 + commit + push(并入批 2 draft PR)**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/pr.yaml'))" && echo YAML-OK
git add .github/workflows/pr.yaml
git commit -s -m "ci: add ASan+UBSan matrix build running unittests and quick suite

本地 aarch64 验证 unittests + --quick 全绿后接入;
ASAN_OPTIONS=detect_leaks=0 规避 fork 框架 LSan 误报。"
git push origin ci/correctness-gates
```

draft PR 上确认 `GCC-x86_64-Sanitizers` 项绿(ASan 下 quick 慢 2-5×,在 45min timeout 内)。

### Task 8: paths-filter(docs-only PR 跳过 multi-version)

**Files:**
- Modify: `.github/workflows/pr.yaml`

- [ ] **Step 1: 加 changes job + multi-version 条件**

`jobs:` 下新增(置于 lint 之前):

```yaml
  # 路径门控:multi-version 的意义是源码/适配层改动的回归哨兵,
  # docs-only PR 跳过(镜像构建 ~10min/次)。白名单语义:src 命中才跑。
  changes:
    runs-on: ubuntu-latest
    timeout-minutes: 5
    permissions:
      contents: read
    outputs:
      src: ${{ steps.filter.outputs.src }}
    steps:
      - uses: actions/checkout@v6
        with:
          persist-credentials: false
      - uses: dorny/paths-filter@v3
        id: filter
        with:
          filters: |
            src:
              - 'framework/**'
              - 'tests/**'
              - 'scripts/**'
              - 'bats/**'
              - 'third-party/**'
              - '.github/**'
              - 'meson.build'
              - 'meson.options'
```

multi-version job 的 `needs:` 从 `[ lint, git-sanity ]` 改为 `[ lint, git-sanity, changes ]`,并加:

```yaml
    if: ${{ needs.changes.outputs.src == 'true' }}
```

(其余 job 的 needs 不动——lint/git-sanity/build 全部 PR 都跑,保 DCO 与构建门禁不被跳过。)

- [ ] **Step 2: 本地验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/pr.yaml'))" && echo YAML-OK
git add .github/workflows/pr.yaml
git commit -s -m "ci: skip multi-version corner-three on docs-only PRs (dorny/paths-filter)

src 白名单(framework/tests/scripts/bats/third-party/.github/meson.*)
命中才跑 multi-version;lint/git-sanity/build 不受影响。"
git push origin ci/correctness-gates
```

draft PR 验证:本批 PR 触碰了 `.github/**` → multi-version 应照常跑;另开一个只改 README 的 throwaway 分支 draft PR,确认 multi-version 显示 skipped 而其余 job 照跑。

---

## 批 3:安全扫描(分支 `ci/security-scanning`,7 任务 7 commits)

### Task 9: security.yaml + zizmor(含噪声治理配置)

**Files:**
- Create: `.github/workflows/security.yaml`、`.zizmor.yml`
- Modify: 无(zizmor 本地基线 78 findings 由配置处置;真修项在 Task 10/11)

- [ ] **Step 1: 写 .zizmor.yml(先本地验证归零再上 CI)**

```yaml
# zizmor 噪声治理(基线 2026-09-25:78 findings):
# - unpinned-uses(52):官方/社区 action 用 major tag,版本治理交给 Dependabot
#   (依赖升级 PR 统一审),不 pin SHA——单人公开仓的维护成本权衡。
# - template-injection(16):本仓 workflow 无 pull_request_target,pull_request
#   事件下注入面限于 PR 作者自己的 runner,降级 low 观察不阻断。
# - self-repository(12)/github-env(6):风格类,关闭。
# 真修项:artipacked(persist-credentials)、excessive-permissions(job 级收紧),
# 见后续 commit。
rules:
  unpinned-uses:
    level: disabled
  template-injection:
    level: low
  self-repository:
    level: disabled
  github-env:
    level: disabled
```

- [ ] **Step 2: 本地验证配置生效**

```bash
~/.local/bin/zizmor --config .zizmor.yml .github/ 2>&1 | tail -2
# 期望:无 high/critical(artipacked 30 是 help 级?——若仍报 high,
# artipacked 在 zizmor 属 pedantic/help 档;excessive-permissions 2 条属真修,
# 允许暂存,Task 11 消除后归零)
```

若 zizmor 版本语法差异导致配置报错,以 `zizmor --help` / 官方 docs 校正键名后重跑。

- [ ] **Step 3: 写 security.yaml**

```yaml
name: Security

# CI 流水线自身与依赖面的安全扫描(zizmor / gitleaks / osv-scanner)。
# PR 即时反馈 + 每周一全量(cron 避整点,见 ScheduleWakeup 同理的错峰惯例)。

on:
  pull_request:
  push:
    branches: [ main ]
  schedule:
    - cron: '23 3 * * 1'   # 每周一 UTC 03:23 = 北京 11:23

permissions:
  contents: read

jobs:
  zizmor:
    name: zizmor (workflow security)
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@v6
        with:
          persist-credentials: false
      - name: Run zizmor
        uses: zizmorcore/zizmor-action@v1
        env:
          GH_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

- [ ] **Step 4: 验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/security.yaml'))" && echo YAML-OK
git add .zizmor.yml .github/workflows/security.yaml
git commit -s -m "ci: add zizmor workflow-security scanning with noise policy

78 findings 基线治理:unpinned/self-repository/github-env 关闭(major tag
+ Dependabot 治理),template-injection 降 low(无 pull_request_target);
artipacked/excessive-permissions 真修见后续 commit。"
git push origin ci/security-scanning
```

### Task 10: artipacked 真修(persist-credentials: false)

**Files:**
- Modify: `.github/workflows/pr.yaml`(8 处 checkout)、`.github/workflows/multi-os-verify.yml`(2 处 checkout)

- [ ] **Step 1: 全部 checkout 加 persist-credentials: false**

现有 checkout 调用(T0 后 pr.yaml 约 6 处:lint×2、git-sanity×1、build-cpu×1、multi-version×1、changes×1;multi-os-verify 2 处),无一后续依赖 git 凭据(pr.yaml 各 job 不 push;multi-os-verify 的 release job 用 `GH_TOKEN` + `gh release`,不依赖 checkout 凭据;report job 无 git 操作)。统一加:

```yaml
      - uses: actions/checkout@v6
        with:
          persist-credentials: false
```

注意:Task 8 的 changes job 已带此参数;本任务补齐其余全部(T0 后 pr.yaml 原有 5 处、multi-os-verify 2 处)。

- [ ] **Step 2: 本地验证 zizmor 该项归零 + commit**

```bash
~/.local/bin/zizmor --config .zizmor.yml .github/ 2>&1 | grep -c artipacked   # 期望 0
git add .github/workflows/ && git commit -s -m "ci: set persist-credentials=false on all checkouts (zizmor artipacked)

10 处 checkout 均无后续 git 写操作,消除凭据驻留注入面。"
git push origin ci/security-scanning
```

### Task 11: excessive-permissions 真修(顶层权限收紧到 job 级)

**Files:**
- Modify: `.github/workflows/multi-os-verify.yml`

- [ ] **Step 1: 顶层收紧 + release job 补 job 级权限**

顶层 `permissions:` 改为:

```yaml
permissions:
  contents: read                # 顶层最小化;写权限只给真正需要的 job
  packages: read                # verify job 拉 ghcr 镜像仍需
```

`release:` job(`runs-on: ubuntu-latest` 之后)加:

```yaml
    permissions:
      contents: write            # gh release create 需要;GitHub job 级权限覆盖顶层
```

- [ ] **Step 2: 验证(轻档)+ commit**

验证口径(全量 dispatch 重验证成本 15 镜像 × ~30min,留待下次真实 publish_release 时观察):

```bash
~/.local/bin/zizmor --config .zizmor.yml .github/ 2>&1 | grep -c excessive-permissions   # 期望 0
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/multi-os-verify.yml'))" && echo YAML-OK
```

job 级覆盖顶层是 GitHub 文档明确行为;release job 用 `GH_TOKEN: ${{ github.token }}` 随 job 权限走。

```bash
git add .github/workflows/multi-os-verify.yml
git commit -s -m "ci: narrow multi-os-verify permissions to job scope (zizmor excessive-permissions)

顶层 contents: write 收紧为 read;仅 release job 保留写权限创建 Release。
验证:zizmor 归零;完整 release 链路留待下次 publish_release 观察确认。"
git push origin ci/security-scanning
```

### Task 12: gitleaks(凭据泄漏扫描)

**Files:**
- Modify: `.github/workflows/security.yaml`

- [ ] **Step 1: security.yaml 加 gitleaks job**

```yaml
  gitleaks:
    name: gitleaks (credential leaks)
    runs-on: ubuntu-latest
    timeout-minutes: 10
    steps:
      - uses: actions/checkout@v6
        with:
          fetch-depth: 0          # 全历史:本仓 fork 自 OpenDCDiag,历史长
          persist-credentials: false
      - uses: gitleaks/gitleaks-action@v2
        env:
          GITHUB_TOKEN: ${{ secrets.GITHUB_TOKEN }}
```

- [ ] **Step 2: 验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/security.yaml'))" && echo YAML-OK
git add .github/workflows/security.yaml
git commit -s -m "ci: add gitleaks full-history credential scan

fetch-depth 0 覆盖 fork 自 OpenDCDiag 的全部历史;公开仓 gitleaks-action 免费。"
git push origin ci/security-scanning
```

draft PR 观察:**若报历史泄漏——立即停手,原文回报用户**(见 Review Focus #6)。

### Task 13: OSV-Scanner(vendored 依赖 CVE 扫描,探索性)

**Files:**
- Modify: `.github/workflows/security.yaml`

**Interfaces:**
- 退出条件:本任务是探索性的——若 CI 实跑证明对无 lockfile/vendored 源码树产不出有效结果(全空或全误报),以「结果 + 建议移除或保留」回报用户,由用户定夺,不强留空转的 job。

- [ ] **Step 1: security.yaml 加 osv job**

```yaml
  osv:
    name: osv-scanner (vendored CVEs)
    runs-on: ubuntu-latest
    timeout-minutes: 15
    steps:
      - uses: actions/checkout@v6
        with:
          submodules: false
          persist-credentials: false
      - uses: google/osv-scanner-action/osv-scanner-action@v2
        with:
          scan-args: |-
            -r
            --skip-git
            ./
```

(`-r` 递归扫源码树(vendored openssl/openblas/sleef 等的版本识别);`--skip-git` 不查 git 依赖,聚焦 vendored。)

- [ ] **Step 2: 验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/security.yaml'))" && echo YAML-OK
git add .github/workflows/security.yaml
git commit -s -m "ci: add osv-scanner for vendored dependency CVEs (exploratory)

third-party/ 自建依赖(openssl/openblas/sleef 等)为主要暴露面;
识别效果待 CI 实跑评估,无有效产出则回报再定去留。"
git push origin ci/security-scanning
```

### Task 14: CodeQL(cpp)

**Files:**
- Create: `.github/workflows/codeql.yaml`

**Interfaces:**
- 需用户网页确认:Settings → Code security → Code scanning 应为空或仅 advanced setup(本 workflow);若已开 default setup,先关(与本文件冲突)。

- [ ] **Step 1: 写 codeql.yaml(meson 项目用手动构建,不用 autobuild)**

架构说明:T0 后仓库仅构建 arm64——CodeQL 的构建注入也走 arm64(ubuntu-24.04-arm + 系统 gcc),与项目唯一支持的目标一致。**风险点:codeql-action 对 linux-arm64 runner 的支持需 CI 实跑确认;若 action 报不支持,回报用户二选一(放弃 CodeQL / 例外允许 x86 runner 仅做分析注入、产物不发布),不得自行决定。**

```yaml
name: CodeQL

# C/C++ 静态分析,排除 third-party/ vendored 代码去噪。
# meson 项目 autobuild 不可靠,手动构建让 CodeQL 注入编译(arm64,与仓库唯一支持目标一致)。

on:
  push:
    branches: [ main ]
  pull_request:
  schedule:
    - cron: '37 2 * * 1'   # 每周一 UTC 02:37 = 北京 10:37

permissions:
  contents: read

jobs:
  analyze:
    name: analyze (cpp)
    runs-on: ubuntu-24.04-arm
    timeout-minutes: 45
    permissions:
      security-events: write    # 上传 SARIF 到 Security 标签页
      contents: read
    steps:
      - uses: actions/checkout@v6
        with:
          persist-credentials: false
      - uses: github/codeql-action/init@v3
        with:
          languages: cpp
          config: |
            paths-ignore:
              - third-party
              - bats
              - scripts
      - name: Install build deps (arm64)
        run: |
          sudo apt-get -y update -qq
          sudo apt-get -y install -qq meson ninja-build libboost-dev libeigen3-dev \
            libgtest-dev libhwloc-dev libssl-dev libzstd-dev zlib1g-dev
      - uses: github/codeql-action/autobuild@v3
```

(arm64 目标用 autobuild 走 meson 的自动识别;若 autobuild 失败,替换为显式 `meson setup builddir && ninja -C builddir` run 步骤——执行时以 CI 日志为准。)

- [ ] **Step 2: 验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/codeql.yaml'))" && echo YAML-OK
git add .github/workflows/codeql.yaml
git commit -s -m "ci: add CodeQL cpp analysis on arm64 (vendored third-party excluded)

arm64 runner 与仓库唯一支持目标一致;结果入 Security and quality 页。"
git push origin ci/security-scanning
```

draft PR 观察:arm64 runner 上 codeql-action 是否正常初始化/出 SARIF;不支持则按 Step 1 风险条款回报。

### Task 15: Dependabot

**Files:**
- Create: `.github/dependabot.yml`

- [ ] **Step 1: 写 dependabot.yml**

```yaml
version: 2
updates:
  - package-ecosystem: github-actions
    directory: /
    schedule:
      interval: weekly
      day: monday
      time: "01:23"    # UTC,避开整点
    open-pull-requests-limit: 5
    groups:
      actions-minor:
        update-types:
          - minor
          - patch
```

(分组升级把 minor/patch 合一个 PR,major 单独审;与 zizmor 的 unpinned-uses 忽略策略配套——版本治理责任移交此处。)

- [ ] **Step 2: 验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/dependabot.yml'))" && echo YAML-OK
git add .github/dependabot.yml
git commit -s -m "ci: add Dependabot for github-actions ecosystem (grouped minor/patch)

checkout/upload/download 版本漂移(v4/v7/v8)治理;
major 升级单独开 PR 审。"
git push origin ci/security-scanning
```

---

## 批 4:闭环与趋势(分支 `ci/loop-and-trends`,3 任务 5 commits)

### Task 16: PR 结果矩阵(复用 report-summary.py,零脚本改动)

**Files:**
- Modify: `.github/workflows/pr.yaml`(build-cpu job)

**Interfaces:**
- Consumes: `scripts/gha/report-summary.py` 的既有 glob `all-results/verify-*/results/allquality.yaml`(实测 quick 跑的 `-o FILE` 输出与其解析格式同构,目录摆位即复用)。

- [ ] **Step 1: quick 跑落盘 + 摆位 + summary step**

build-cpu job 的 `run tests quickly` 步骤,把 `-o -` 改为落盘并追加 summary 步骤:

```yaml
      - name: run tests quickly
        shell: bash
        run: |
          ulimit -St 120
          nproc=`nproc`
          nproc=$((nproc > 4 ? 4 : nproc))
          builddir/sdcshield --quick -n$nproc --retest-on-failure=0 -o quick.yaml
      # quick 结果矩阵进 PR 页 step summary(复用 nightly 的 report-summary.py;
      # -o 指定的文件失败时保留,失败现场也出矩阵)。目录名 verify-<matrix.name>
      # 对齐脚本的 all-results/verify-*/results/allquality.yaml glob,零脚本改动。
      - name: test-result matrix summary
        if: ${{ always() }}
        shell: bash
        run: |
          if [ -f quick.yaml ]; then
            mkdir -p "all-results/verify-${{ matrix.name }}/results"
            cp quick.yaml "all-results/verify-${{ matrix.name }}/results/allquality.yaml"
            python3 scripts/gha/report-summary.py >> "$GITHUB_STEP_SUMMARY" || true
          fi
```

- [ ] **Step 2: 本地等价验证(摆位模拟)**

```bash
cd /tmp && rm -rf j1test && mkdir -p j1test/all-results/verify-GCC-x86_64/results && cd j1test
/home/sdc/wangxu/sdcshield/builddir/sdcshield --quick -n1 --retest-on-failure=0 -e zstd19 -t 200 -o all-results/verify-GCC-x86_64/results/allquality.yaml >/dev/null 2>&1
python3 /home/sdc/wangxu/sdcshield/scripts/gha/report-summary.py | head -8
# 期望:输出 markdown 表头 + zstd19 行(单列矩阵)
```

- [ ] **Step 3: commit + push**

```bash
git add .github/workflows/pr.yaml
git commit -s -m "ci: surface quick-run test-result matrix on PR page (reuse report-summary.py)

quick 从 -o - 改落盘 quick.yaml,按脚本 glob 摆位,PR step summary
直接出「用例 × 结果[耗时]」矩阵;失败时同样出(现场可见)。"
git push origin ci/loop-and-trends
```

### Task 17: benchmark 趋势(gh-pages,固定 24.03-LTS-SP3)

**Files:**
- Create: `scripts/gha/benchmark-to-json.py`
- Modify: `.github/workflows/multi-os-verify.yml`

**Interfaces:**
- Consumes: `benchmark/benchmark.tsv`(列:test/wall_seconds/loop_count,skip 行 wall_seconds=skip)。
- Produces: `benchmark/benchmark.json`(`[{name, value, unit:"s"}]`,github-action-benchmark 的 `tool: custom` 格式)。
- gh-pages 分支由 benchmark-action 自动创建/更新;趋势页 = gh-pages 的 index.html。

- [ ] **Step 1: 写 benchmark-to-json.py(commit 1)**

```python
#!/usr/bin/env python3
# benchmark-to-json.py — benchmark.tsv → github-action-benchmark custom JSON。
#
# 输入:benchmark.sh 产出的 benchmark.tsv(列:test\twall_seconds\tloop_count;
#   不可跑的测试 wall_seconds 为 skip)。
# 输出:[{"name": <test>, "value": <wall_seconds>, "unit": "s"}, ...](skip 行剔除)。
# 供 benchmark-action/github-action-benchmark 的 tool: custom 消费,趋势线只取
# 固定基准 SP(24.03-LTS-SP3),跨 commit 可比。
#
# SPDX-License-Identifier: Apache-2.0
import json
import sys

if len(sys.argv) != 3:
    sys.exit(f"usage: {sys.argv[0]} <benchmark.tsv> <out.json>")

tsv_path, out_path = sys.argv[1], sys.argv[2]
entries = []
with open(tsv_path, encoding="utf-8") as f:
    header = f.readline()  # test\twall_seconds\tloop_count
    for line in f:
        parts = line.rstrip("\n").split("\t")
        if len(parts) != 3:
            continue
        name, wall, _loop = parts
        try:
            value = float(wall)
        except ValueError:
            continue  # skip 行
        entries.append({"name": name, "value": value, "unit": "s"})

with open(out_path, "w", encoding="utf-8") as f:
    json.dump(entries, f, indent=2)
    f.write("\n")
print(f"benchmark json written: {out_path} ({len(entries)} entries)")
```

本地验证(用现有 benchmark.tsv 若有,否则造 3 行假数据):

```bash
printf 'test\twall_seconds\tloop_count\nzstd\t12.3\t1\nzlib\tskip\t0\nfma\t4.5\t2\n' > /tmp/b.tsv
python3 scripts/gha/benchmark-to-json.py /tmp/b.tsv /tmp/b.json && cat /tmp/b.json
# 期望:[{"name": "zstd", "value": 12.3, "unit": "s"}, {"name": "fma", "value": 4.5, "unit": "s"}]
git add scripts/gha/benchmark-to-json.py
git commit -s -m "ci: add benchmark.tsv to github-action-benchmark JSON converter

skip 行剔除,value=同工作量墙钟秒数,unit=s;
只服务固定基准 SP 的趋势线。"
```

- [ ] **Step 2: multi-os-verify.yml 加独立 trend job(commit 2)**

`release:` job 之前加:

```yaml
  # ---- 基准趋势:只取 24.03-LTS-SP3(基准版本)的 benchmark.tsv 上 gh-pages。
  # 独立非容器 job(避免 container 内 git push 兼容问题);benchmark-action
  # auto-push 自动创建/更新 gh-pages 分支。共享 runner 噪声大,先观测告警
  # 不硬门(fail-on-alert: false),趋势稳定后再评估收紧。----
  benchmark-trend:
    name: benchmark trend (gh-pages)
    needs: verify
    if: ${{ !cancelled() }}
    runs-on: ubuntu-latest
    timeout-minutes: 15
    permissions:
      contents: write            # auto-push 到 gh-pages
    steps:
      - uses: actions/checkout@v6
        with:
          persist-credentials: false
      - uses: actions/download-artifact@v8
        with:
          pattern: verify-24.03-LTS-SP3
          path: bench
          merge-multiple: true
      - name: Convert to benchmark JSON
        run: |
          tsv=$(ls bench/benchmark/benchmark.tsv 2>/dev/null || ls bench/*/benchmark/benchmark.tsv 2>/dev/null | head -1)
          [ -n "$tsv" ] || { echo "no benchmark.tsv (smoke run?) — skip trend update"; exit 0; }
          python3 scripts/gha/benchmark-to-json.py "$tsv" bench/benchmark.json
      - uses: benchmark-action/github-action-benchmark@v1
        with:
          name: sdcshield benchmark (24.03-LTS-SP3)
          tool: custom
          output-file-path: bench/benchmark.json
          github-token: ${{ secrets.GITHUB_TOKEN }}
          auto-push: true
          alert-threshold: '150%'
          comment-on-alert: true
          fail-on-alert: false
```

注意两个衔接点:① verify job 上传的 artifact 名是 `verify-${{ matrix.series }}-${{ matrix.sp }}` = `verify-24.03-LTS-SP3`(下载 pattern 与其一致);② 该 job 的 `contents: write` 是 Task 11 收紧后的最小放开(zizmor 若报 excessive-permissions,在 `.zizmor.yml` 为该 job 加 allowlist 并注释理由)。若 benchmark-action 文档要求 checkout 保留凭据才能 push,则把本 job 的 `persist-credentials: false` 去掉并在 commit 信息里注明(zizmor 配置同步放行)。

- [ ] **Step 3: 验证 + commit + push**

```bash
python3 -c "import yaml; yaml.safe_load(open('.github/workflows/multi-os-verify.yml'))" && echo YAML-OK
git add .github/workflows/multi-os-verify.yml .zizmor.yml
git commit -s -m "ci: add nightly benchmark trend to gh-pages (24.03-LTS-SP3 baseline)

固定基准 SP 的同工作量墙钟跨 commit 趋势 + 150% 回归告警;
独立非容器 job,auto-push gh-pages。"
git push origin ci/loop-and-trends
```

验证:draft PR 合并后手动 `workflow_dispatch` 一次(verify_depth: smoke 也行,benchmark step 无条件跑),确认 gh-pages 分支出现、趋势页可访问。

### Task 18: docs 同步 + 网页操作清单

**Files:**
- Create: `docs/build-deploy/ci-web-operations.md`
- Modify: `README.md`(CI 章节)、`docs/multi-version-build-deploy.md`(GHA 章节,若涉及权限/趋势变化)

**Interfaces:**
- Consumes: 批 1-4 全部落地的最终形态。

- [ ] **Step 1: 写网页操作清单(F1 原生通知 + G1 分支保护 + CodeQL 确认)**

`docs/build-deploy/ci-web-operations.md` 核心内容(执行时补全每步的菜单路径,以下为骨架):

```markdown
# CI 网页操作清单(GitHub Settings 侧,代码无法完成的部分)

## nightly 失败通知(GitHub 原生,已选定方案)
1. 个人 Settings → Notifications → Actions → 勾选 **Workflow failures**
   (scheduled workflow 失败只通知最近编辑该 workflow 的用户——保持 workflow 文件由本人维护)
2. 仓库页 Watch → Custom → 勾选 **Actions**(Issues/PRs 按需)
   生效:multi-os-verify 每日 cron 失败推送邮件。

## 分支保护 + required checks(把"一补丁一 PR"变硬门)
1. Settings → Branches → Add branch ruleset → target: main
2. Require a pull request before merging(要求 DCO 已由 CI 检查)
3. Require status checks:勾选 `lint`、`git-sanity`、`build-cpu (GCC-arm64)`、
   `build-cpu (GCC-arm64-werror)`、`build-cpu (GCC-arm64-Sanitizers)`、
   `multi-version (24.03-SP3)`(名称以 Actions 实际显示为准)
4. Do not allow bypassing(单人仓可选,保留紧急直推能力则不勾)

## CodeQL / Security 页确认
1. Settings → Code security → Code scanning:应显示 CodeQL(advanced setup = codeql.yaml)
2. Dependabot / secret scanning / zizmor 结果按页面提示启用(均免费)
```

- [ ] **Step 2: README CI 章节同步**

README.md「GitHub Actions 每日多 OS 自动验证」一节(约 141 行起)追加一段:PR CI 门禁清单(lint/codespell/actionlint/werror/sanitizer/paths 门控)、Security workflow、benchmark 趋势页链接(gh-pages URL)、PR 结果矩阵说明。**内容以批 1-4 合并后的真实形态为准,不提前写。**

- [ ] **Step 3: 验证 + commit + push**

自检:文档中每个 URL/路径/action 名与仓库实际一致(逐条核对)。

```bash
git add docs/build-deploy/ci-web-operations.md README.md docs/multi-version-build-deploy.md
git commit -s -m "docs: sync CI capability additions (gates, security, trends) + web-ops checklist

README CI 章节对齐 4 批新增;网页操作清单覆盖原生通知/
分支保护/CodeQL 启用等代码侧无法完成的部分。"
git push origin ci/loop-and-trends
```

---

## CI 验证工作流(所有任务通用)

pr.yaml / security.yaml 仅在 `pull_request` 或 `main` push 时触发,**分支 push 不跑**。因此每批的验证回路:

1. 批内任务逐个 commit 并 push 到 feature 分支
2. `gh pr create --draft`(本机无 gh CLI 时请用户在网页开,或用户执行 `! gh auth login` 后由我操作)
3. 看 Actions:全绿 → 请用户 review → merge;红 → 读日志 → 独立 fix commit → push → 直至绿
4. security.yaml 的 schedule 路径(每周一)与 codeql.yaml 的 main push 路径,merge 后首次触发时人工看一眼

## 执行顺序与依赖

- **T0(移除 x86 构建,分支 ci/arm64-only-builds)先行**——本计划所有任务按 T0 合并后的 pr.yaml 形态编写;T0 未合并前不开批 1 分支(同文件冲突)
- **T5(清存量警告)独立分支 fix/werror-warnings-cleanup 先行**——纯源码修复,与 T0 的 pr.yaml 无文件交集,不依赖 T0 merge;本地 builddir-werror 全量验证
- 批 1(T1-T4 + 死文件清理)无依赖,任意顺序(均待 T0 merge 后开分支)
  - 死文件清理:删除 `.github/actions/build-cpu-win|build-gpu|build-idxd` 三个无引用 composite(T0 合并后才无引用;T0 前删除会打断 main 的 CI)。一个 commit,消息 `ci: remove unused win/gpu/idxd composite actions (dead after T0)`
- 批 2:T5 → T6(强制:先清警告再上门禁);T7/T8 独立
- 批 3:T9 → T10 → T11(强制:zizmor 配置先行,artipacked/excessive-permissions 真修依赖其验证口径);T12-T15 任意
- 批 4:T17 依赖 T11(权限收紧后再最小放开);T16 独立;T18 最后(以全部落地形态为准)
- 批间串行:上一批 merge 后再开下一批分支(避免同文件冲突:批 2/3/4 都改 pr.yaml 或 multi-os-verify.yml)

# issue #449：plan/spec 564 格判据随 enumeration 190 行裁决同步（570 格）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 把 SVE 全 VL campaign 的 plan 与 spec 两份判据文档中残留的 "188 用例 / 564 格" 口径全部更新为 Task 1 枚举实测裁决的 "190 用例 / 570 格"，消除验收判据与实际清单工件（190 行）及已收口矩阵（570 格）的直接冲突。

**Architecture:** 纯 docs 一致性修复，零代码改动。根因：plan/spec 写作于 Task 1 枚举之前（564 = name-grep 188 × 3 是计划期估算），Task 1 的 enumeration.md §3 实测裁决清单 = union(193) − {neon_add_sve, arm64_sdc_sve, crt_builtins_sve} = 190 行，后续 triage-256/triage-512/matrix.md 全部按 190/570 口径执行（matrix.md:7 已记 Ruling A："spec 的 564 为 name-grep 估算，被实测枚举取代"），唯独 plan/spec 未回写。修法：逐处改为 190/570，并在 Goal/目标处加一行裁决依据注记（指向 enumeration.md §3），使判据文档与工件一致、且未来审计者可循证据链。

**Tech Stack:** Markdown 文档；grep/wc 核对；仓库 git 纪律（commit -s、一 patch 一 unit）。

**Spec:** 本计划的依据是 issue #449（审计发现）与 `docs/superpowers/inventory/sve-all-vl-campaign/enumeration.md` §3 的实测裁决（非新 spec——这是对既有判据文档的勘误）。

## Global Constraints

- 工作分支：`fix/issue-449`（从 origin/main 起），绝不推 main；`git commit -s`，最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By。
- 保留事实性数字不改：plan:38 的 "479 行（本机默认集）"、plan:39 的 "预期 188 行"（name 路 grep 实测确为 188，enumeration.md §1 表格佐证）、spec:31 的事实基线行、spec:35-36 的 "171 vs 188"（描述 spec 写作时基线，enumeration.md §1 已记录其后演化）——这些是"当时实测事实"，不是验收判据，不在修复范围。
- 判据数字的裁决依据必须可循：凡 564→570、188（清单口径）→190 之处，至少 Goal/目标一处注明依据 = enumeration.md §3。
- 纯文档改动，构建与回归不受影响，但按 CLAUDE.md 规则 2 仍需跑 `./scripts/build.sh`（零新警告）与 `-e zstd19 -t 2000 -n 1`（exit: pass）作为非回归证据。

---

### Task 1: plan 与 spec 的 564/188 判据口径同步（单一 docs 单元）

**Files:**
- Create: 本计划文档自身（`docs/superpowers/plans/2026-11-02-issue-449-564-to-570-criteria-sync.md`）
- Modify: `docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md`（:1 标题、:5 Goal、:72 Task 1 Step 4 预期注释、:289/:294/:298 Task 6、:395/:406/:407 Task 8）
- Modify: `docs/superpowers/specs/2026-10-28-sve-all-vl-verification-campaign-design.md`（:8-9 目标、:42 §3、:94 §7、:101 §8、:123 §10）

**Interfaces:**
- Consumes: `enumeration.md` §3 裁决（190 行 = union(193) − 3 件）、`sve-test-list.txt` 实测 190 行、`matrix.md` 实际 570 格。
- Produces: 判据文档与工件一致（全文无残留 564/188 清单口径），本计划文档留档。

- [ ] **Step 1: 修改 plan 文档（7 处）**

1. `:1` 标题 `（188 用例 × 128/256/512）` → `（190 用例 × 128/256/512）`
2. `:5` Goal `为 188 个 SVE1 用例在 VL=128/256/512 三档采集真实通过证据（564 格矩阵），发现即修。` → `为 190 个 SVE 用例在 VL=128/256/512 三档采集真实通过证据（570 格矩阵），发现即修。`并在其后追加一行 `> 判据口径（2026-11-02 勘误，issue #449）：清单 = 枚举实测裁决 190 行（union(193) − 3 件不在本机默认集，enumeration.md §3），非计划期 name-grep 估算的 188×3=564；Task 2/3/6/8 的一切格数以 190 为基（570 = 190×3）。`
3. `:72` 注释 `（预期 ≈189：188 + gemm_double_max_arm）` → `（实测 190：188 named + eigen_gemm_double_max_arm + libomp_ctx_arm；union 193 − 3 件不在默认集，enumeration.md §3）`
4. `:289` Task 6 标题 `512 批量执行（188 用例）` → `（190 用例）`
5. `:294` `gem5-512/<test>.log × 188` → `× 190`
6. `:298` `Produces: 512 档 188 格证据` → `512 档 190 格证据`
7. `:395` `matrix.md（564 格总表）` → `（570 格总表）`；`:406` Step 2 `564 格全绿` → `570 格全绿`；`:407` commit message `564 格证据矩阵汇总` → `570 格证据矩阵汇总`

- [ ] **Step 2: 修改 spec 文档（5 处）**

1. `:8-9` §1 目标 `188 个 SVE1 用例……（564 格矩阵）` → `190 个 SVE 用例……（570 格矩阵）`，句后括注 `（勘误 2026-11-02，issue #449：清单按 Task 1 实测枚举裁决为 190 行，见 enumeration.md §3；本节原 188×3=564 为 name-grep 估算）`
2. `:42` §3 `作为 564 格矩阵的行定义` → `作为 570 格矩阵的行定义`
3. `:94` §7 `564 格矩阵（188 × 3）全绿` → `570 格矩阵（190 × 3）全绿`
4. `:101` §8 `（188 × 3 全绿 + 证据索引）` → `（190 × 3 全绿 + 证据索引）`
5. `:123` §10 `564 格证据矩阵` → `570 格证据矩阵`

- [ ] **Step 3: 自查无残留**

```bash
grep -n "564\|× 188\|188 用例\|188 格\|188 个" \
  docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md \
  docs/superpowers/specs/2026-10-28-sve-all-vl-verification-campaign-design.md
```
Expected: 仅剩**事实性**引用（plan:38-39 的 479/188 named 实测、Goal 注记里的 "188×3=564" 历史勘误说明、spec §2/§3 基线数字），判据处（Goal、Task 6 产量、Task 8 验收、spec §1/§7/§10）全部为 190/570。

```bash
wc -l docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt   # 190
grep -c "570" docs/superpowers/inventory/sve-all-vl-campaign/matrix.md   # 判据文档与工件同口径
```

- [ ] **Step 4: 构建与回归（CLAUDE.md 规则 2）**

```bash
./scripts/build.sh 2>&1 | tail -5        # 零新警告
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -3   # exit: pass
```

- [ ] **Step 5: Commit + push**

```bash
git add docs/superpowers/plans/2026-11-02-issue-449-564-to-570-criteria-sync.md \
        docs/superpowers/plans/2026-10-28-sve-all-vl-verification-campaign.md \
        docs/superpowers/specs/2026-10-28-sve-all-vl-verification-campaign-design.md
git commit -s -m "docs(campaign): plan/spec 564 格判据随 enumeration 190 行裁决同步为 570 格（issue #449）"
git push -u origin fix/issue-449
```

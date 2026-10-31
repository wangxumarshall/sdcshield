#!/bin/bash
# matrix-gen.sh — SVE 全 VL 验证运动 Task 8：从已入库工件机械生成 190×3 证据矩阵。
#
# Usage:   ./matrix-gen.sh > matrix.md        （在任意 cwd 执行均可，路径相对本脚本解析）
#
# 输入（全部为本目录已 commit 的只读工件 + 仓库内源文件）：
#   sve-test-list.txt                    190 行清单 = 矩阵行定义（Task 1）
#   native-128-LCG1001.summary.txt       128 腿 seed1 逐用例结果（Task 3）
#   native-128-LCG1002.summary.txt       128 腿 seed2（Task 3）
#   native-256-LCG1001.summary.txt       256 腿 seed1（Task 2）
#   native-256-LCG1002.summary.txt       256 腿 seed2（Task 2）
#   gem5-512-verdicts.txt                512 腿逐用例判定（Task 6）
#   gem5-overrides.txt                   512 腿额外降容表，16 条（Task 6）
#   triage-512.md                        §0 头部块（逐字引用进矩阵头部）
#   tests/cpu/arm64/sve512_fmmla_arm.cpp :193-196（fmmla@128 SKIP 的逐字源码引文，
#                                        T3 minor fix：triage-128 曾意译丢句，此处补逐字）
#
# 纪律（brief Step 1）：每一格的值都由本脚本从上述工件机械推导，**不许手填**。
# 脚本内嵌的字面量只有四类，均为对已入库文档的引用而非数据编造：
#   (a) triage 分节号引用（triage-256 §1+§5 / triage-128 §1+§2 / triage-512 §0+§5.1+§5.2）；
#   (b) 9 个 512 边界格的机制性理由短标签（内容归类引自 triage-512 §5.1/§5.2）；
#   (c) SKIP(legal) 的理由短标签（内容引自 triage-128 §2 + 上述源码 :193-196）；
#   (d) §4 表体的 † 弱证据行标记集合 = 5 个 PR #316 已修复越界缺陷测试
#       （名单机械对齐 §0.1 delta 表 #316 行，oob_marked==5 断言守卫）。
#
# 一致性断言（任一失败 → 非零退出、不产出 matrix.md）：
#   清单 190 行；4 份 native summary + 1 份 verdicts 各覆盖全部 190 名；
#   26 项 baked -O 在 128 seed1/seed2 与 256 seed2 三处命令行头逐字一致；
#   无未知 result 值；无 FAIL / MISSING 格；每列恰 190 格、总 570 格；
#   128 列双 seed skip 仅限钉死集合 {sve512_fmmla_arm}（PR #307 #8——
#   其余测试双 skip 或单 seed 分歧 skip 均 FATAL，防「合法 SKIP」标签
#   张冠李戴 / seed 依赖型 skip 被 pass 掩蔽）；512 列 crash/timeout 格
#   必有机制理由标签；† 标记行恰 5 行（PR #307 #22）。
#
# locale：全链 LC_ALL=C（enumeration.md §6 的 comm/sort 教训）。

set -euo pipefail
export LC_ALL=C

DIR=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$DIR/../../../.." && pwd)

LIST="$DIR/sve-test-list.txt"
N128S1="$DIR/native-128-LCG1001.summary.txt"
N128S2="$DIR/native-128-LCG1002.summary.txt"
N256S1="$DIR/native-256-LCG1001.summary.txt"
N256S2="$DIR/native-256-LCG1002.summary.txt"
G512="$DIR/gem5-512-verdicts.txt"
GOVR="$DIR/gem5-overrides.txt"
TRI512="$DIR/triage-512.md"
FMMLA_SRC="$REPO/tests/cpu/arm64/sve512_fmmla_arm.cpp"

for f in "$LIST" "$N128S1" "$N128S2" "$N256S1" "$N256S2" "$G512" "$GOVR" "$TRI512" "$FMMLA_SRC"; do
    [ -f "$f" ] || { echo "ERROR: input not found: $f" >&2; exit 2; }
done

TMP=$(mktemp -d)
trap 'rm -rf "$TMP"' EXIT

# ---- 26 项 baked -O：机械取自命令行头（三处一致由 diff 断言守卫；尾引号剥掉）。
extract_baked() { grep -m1 '^# command-line: ' "$1" | grep -o -- '-O [^ ]*' \
                    | sed -e 's/^-O //' -e "s/'\$//"; }
extract_baked "$N128S1" | sort > "$TMP/baked26.txt"
extract_baked "$N128S2" | sort | diff -q - "$TMP/baked26.txt" >/dev/null \
    || { echo "ERROR: baked -O drift between 128 seed1/seed2" >&2; exit 2; }
extract_baked "$N256S2" | sort | diff -q - "$TMP/baked26.txt" >/dev/null \
    || { echo "ERROR: baked -O drift between 128 leg and 256 seed2" >&2; exit 2; }
[ "$(wc -l < "$TMP/baked26.txt")" -eq 26 ] \
    || { echo "ERROR: expected 26 baked -O, got $(wc -l < "$TMP/baked26.txt")" >&2; exit 2; }
# → <test><TAB><knob>=<value>（在首个 '.' 处切分：测试名不含 '.'）
awk '{
    dot = index($0, ".")
    printf "%s\t%s\n", substr($0, 1, dot - 1), substr($0, dot + 1)
}' "$TMP/baked26.txt" > "$TMP/knob_baked.tsv"

# ---- 512 腿额外降容（gem5-overrides.txt，<test>|-O <test>.<k>=<v>）→ 同形 TSV。
# 键唯一契约（PR #307 #32）：本生成器与 gem5-512-batch.sh 统一拒绝重复键
# （批量取首行 vs 本处旧语义取末行——重复键曾会使 512 列标签与实际执行
# 命令的 knob 脱钩）；契约 = 一测试一行，修订改原行不追加。
ovrdup=$(awk -F'|' '$0 ~ /^[A-Za-z0-9_]+\|/ { print $1 }' "$GOVR" | sort | uniq -d)
if [ -n "$ovrdup" ]; then
    echo "ERROR: duplicate test keys in gem5-overrides.txt (contract: one line per test — edit the line, do not append):" >&2
    echo "$ovrdup" >&2
    exit 2
fi
awk -F'|' '$0 ~ /^[A-Za-z0-9_]+\|/ {
    opt = $2; sub(/^-O [^.]+\./, "", opt); print $1 "\t" opt
}' "$GOVR" > "$TMP/knob_extra.tsv"

# ---- triage-512 §0 头部块（逐字；sed 界定首尾标题，剥标题本身与空行）。
sed -n '/^## 0\./,/^## 1\./p' "$TRI512" | sed '1d;$d' | sed '/^$/d' > "$TMP/sec0.quote"

# ---- fmmla@128 源码逐字引文（tests/cpu/arm64/sve512_fmmla_arm.cpp:193-196）。
# 内容断言（PR #307 #11）：行号是活源码上的脆弱锚——上游漂移一行即静默
# 错引；断言引文恰 4 行且含门调用与实测 SIGILL 句，漂移即 FATAL。
sed -n '193,196p' "$FMMLA_SRC" > "$TMP/fmmla.quote"
quote_ok=1
grep -q 'arm64_require_sve_vl(test, 256, 1024)' "$TMP/fmmla.quote" || quote_ok=0
grep -q 'raises SIGILL (measured 2026-10-27 on silicon' "$TMP/fmmla.quote" || quote_ok=0
[ "$(wc -l < "$TMP/fmmla.quote")" -eq 4 ] || quote_ok=0
[ "$quote_ok" -eq 1 ] \
    || { echo "ERROR: fmmla quote drift: $FMMLA_SRC:193-196 no longer holds the expected 4 lines (gate call 'arm64_require_sve_vl(test, 256, 1024)' + measured-SIGILL sentence) — re-anchor the sed range" >&2; exit 2; }

# ---- 主生成：awk 依次加载 8 个输入；END 段产 (a) 表体 (b) §5 汇总算术块。
#      任一断言失败 → exit 1，不写产物。
awk -F'\t' -v OUT="$TMP" '
FNR == 1 { fileno++ }
# 输入 1: sve-test-list.txt —— 行定义（清单序 = 矩阵序）
fileno == 1 {
    if ($0 !~ /^#/ && NF) { n++; order[n] = $1 }
    next
}
# 输入 2..5: native summaries —— <test>\t<result>\t<iters>\t<ms>（注释/头行跳过）
fileno >= 2 && fileno <= 5 {
    if ($0 ~ /^#/ || $1 == "test") next
    res[fileno "," $1] = $2
    seen[fileno "," $1] = 1
    next
}
# 输入 6: gem5-512-verdicts.txt —— 列 = test rc wall exit tick verdict；
#   crash/timeout 行的空 tick 列在默认空白切分下折叠，$NF 仍为 verdict。
fileno == 6 {
    if ($0 ~ /^#/ || $1 == "test") next
    rc[$1] = $2; verdict[$1] = $NF; gseen[$1] = 1
    next
}
# 输入 7: 26 项 baked knob（128/256 两列共用；512 列以此为底）
fileno == 7 { knob[$1] = $2; knob512[$1] = $2; next }
# 输入 8: gem5 extras（16 条；键唯一由前置守卫断言 → 512 列取该键唯一值；
#   extra 单独留档用于 §5 有界格分解统计）
fileno == 8 { knob512[$1] = $2; extra[$1] = $2; next }

# 格值 → 统计类（在 END 里对每格调用）
function cls(c, arr) {
    if (c == "pass") arr["pass"]++
    else if (substr(c, 1, 13) == "pass(bounded:") arr["bounded"]++
    else if (substr(c, 1, 5) == "SKIP(") arr["skip"]++
    else if (substr(c, 1, 9) == "BOUNDARY(") arr["boundary"]++
    else arr["fail"]++
}

END {
    # 128 列合法双 seed SKIP 钉死集合（PR #307 #8）：唯一格 = fmmla（理由
    # 内容引自 triage-128 §2 + 源码 :193-196）。任何其它测试双 seed skip
    # 都不许静默套用该标签——FATAL 交人工分诊。
    legal_skip["sve512_fmmla_arm"] = "F64MM UNDEFINED <VL=256"

    # PR #316 已修复越界缺陷测试（#312-#315；冻结基线上含缺陷——弱证据
    # 行标记，PR #307 #22）。名单 = §0.1 delta 表 #316 行；oob_marked==5
    # 断言守卫（清单漂移丢名即 FATAL，防标记静默失配）。
    oob316["iex_operand_combo_sve"] = 1
    oob316["partial_store_forwarding_sve"] = 1
    oob316["mrn_nuke_dense_sve"] = 1
    oob316["mesh_upi_sve_wide_asymm_read_L3_int"] = 1
    oob316["mesh_upi_sve_wide_asymm_read_only_int"] = 1
    oob_marked = 0

    # 9 个 512 边界格机制标签（归类出处 = triage-512 §5.1 PMULL64/DC-CIVAC、§5.2 SIM-SPEED）
    breason["ipsec_aes192_gcm_sve"]      = "PMULL64"
    breason["ipsec_aes192_gcm_sve_wide"] = "PMULL64"
    breason["l2c_cross_cache_line_sve"]  = "DC-CIVAC"
    breason["movbe_l2_sve"]              = "DC-CIVAC"
    breason["mrn_rmw_l1d_sve"]           = "DC-CIVAC"
    breason["eigen_svd_cdouble_sve"]     = "SIM-SPEED"
    breason["movbe_slf_sweep_sve"]       = "SIM-SPEED"
    breason["mrn_rmw_diff_1m_sve"]       = "SIM-SPEED"
    breason["sve512_stencil_axis_arm"]   = "SIM-SPEED"

    fatal = 0
    if (n != 190) { printf "FATAL: list has %d rows (expected 190)\n", n > "/dev/stderr"; fatal = 1 }

    for (i = 1; i <= n; i++) {
        t = order[i]
        for (f = 2; f <= 5; f++)
            if (!seen[f "," t]) { printf "FATAL: %s missing in native summary #%d\n", t, f > "/dev/stderr"; fatal = 1 }
        if (!gseen[t]) { printf "FATAL: %s missing in gem5 verdicts\n", t > "/dev/stderr"; fatal = 1 }

        # ---- 128 列：两 seed 无分歧是断言不变量（matrix.md §2「实测两 seed
        #      无分歧绿」）；双 seed skip 仅限钉死集合（legal_skip），单 seed
        #      skip（seed 依赖、非架构性）FATAL 交人工分诊——PR #307 #8。
        a1 = res[2 "," t]; a2 = res[3 "," t]
        if (a1 != "pass" && a1 != "skip") { printf "FATAL: 128s1 bad result [%s] %s\n", a1, t > "/dev/stderr"; fatal = 1 }
        if (a2 != "pass" && a2 != "skip") { printf "FATAL: 128s2 bad result [%s] %s\n", a2, t > "/dev/stderr"; fatal = 1 }
        if (a1 == "skip" && a2 == "skip") {
            if (!(t in legal_skip)) { printf "FATAL: unexpected double-skip: %s (only sve512_fmmla_arm is a legal SKIP, triage-128 §2)\n", t > "/dev/stderr"; fatal = 1 }
            c128 = (t in legal_skip) ? "SKIP(legal: " legal_skip[t] ")" : "FAIL"
        } else if (a1 == "skip" || a2 == "skip") {
            printf "FATAL: seed-disagreement skip (non-architectural, manual triage): %s (%s/%s)\n", t, a1, a2 > "/dev/stderr"; fatal = 1; c128 = "FAIL"
        } else {
            c128 = (t in knob) ? "pass(bounded: " knob[t] ")" : "pass"
        }

        # ---- 256 列：seed1 腿内 timed out 的格由 triage-256 §5 有界重跑转绿
        #      （同 seed 单测、clamp 下限一次通过）；seed2 腿内有界直绿。
        #      断言：timed out 的格必有 knob（§5 表全覆盖）。
        b1 = res[4 "," t]; b2 = res[5 "," t]
        if (b1 != "pass" && b1 != "timed out") { printf "FATAL: 256s1 bad result [%s] %s\n", b1, t > "/dev/stderr"; fatal = 1 }
        if (b2 != "pass")                    { printf "FATAL: 256s2 bad result [%s] %s\n", b2, t > "/dev/stderr"; fatal = 1 }
        if (b1 == "timed out" && !(t in knob)) { printf "FATAL: 256s1 timeout without knob: %s\n", t > "/dev/stderr"; fatal = 1 }
        if (b1 == "pass" || b2 == "pass")
            c256 = (t in knob) ? "pass(bounded: " knob[t] ")" : "pass"
        else { printf "FATAL: 256 cell not green: %s (%s/%s)\n", t, b1, b2 > "/dev/stderr"; fatal = 1; c256 = "FAIL" }

        # ---- 512 列：判定 = 进程退出码权威（triage-512 §0）；
        #      crash/timeout = BOUNDARY(gem5)（Ruling I 终局，9 格全有机制标签）
        v = verdict[t]
        if (v == "pass" && rc[t] != 0) { printf "FATAL: 512 %s verdict pass but rc=%s\n", t, rc[t] > "/dev/stderr"; fatal = 1 }
        if (v == "pass")
            c512 = (t in knob512) ? "pass(bounded: " knob512[t] ")" : "pass"
        else if (v == "crash" || v == "timeout") {
            if (!(t in breason)) { printf "FATAL: 512 %s without boundary reason tag\n", t > "/dev/stderr"; fatal = 1 }
            c512 = "BOUNDARY(gem5: " breason[t] ")"
        } else { printf "FATAL: 512 bad verdict [%s] %s\n", v, t > "/dev/stderr"; fatal = 1; c512 = "FAIL" }

        # ---- 证据列：128s1,128s2 · 256s1,256s2 · rc（to = timed out → §5 重跑）
        s = a1 "," a2 " · " (b1 == "timed out" ? "to" : b1) "," b2 " · rc=" rc[t]

        tdisp = t ((t in oob316) ? "†" : "")
        if (t in oob316) oob_marked++
        printf "| %d | %s | %s | %s | %s | %s |\n", i, tdisp, c128, c256, c512, s > (OUT "/table.md")

        # ---- 有界格分解统计（§5 对账行用，全部机械计数）
        if (b1 == "timed out") n_to++                       # 256 列 §5 重跑格
        if (v == "pass") {
            if (t in knob)  bg++                            # 512 列 baked-绿
            if (t in extra) eg++                            # 512 列 extras-绿
            if (t in knob && t in extra) bo++               # 两处重叠-绿
        }
        cls(c128, cnt128); cls(c256, cnt256); cls(c512, cnt512)
        rows++; cells += 3
    }

    if (rows != 190 || cells != 570) { printf "FATAL: %d rows / %d cells (expected 190 / 570)\n", rows, cells > "/dev/stderr"; fatal = 1 }
    if (oob_marked != 5) { printf "FATAL: expected 5 #316-OOB-marked rows, marked %d (list/marker drift)\n", oob_marked > "/dev/stderr"; fatal = 1 }
    if (cnt128["fail"] || cnt256["fail"] || cnt512["fail"]) { printf "FATAL: FAIL cells present\n" > "/dev/stderr"; fatal = 1 }
    if (cnt512["bounded"] != bg + eg - bo) { printf "FATAL: 512 bounded %d != %d + %d - %d\n", cnt512["bounded"], bg, eg, bo > "/dev/stderr"; fatal = 1 }
    if (cnt256["bounded"] < n_to) { printf "FATAL: 256 bounded %d < timeout-rerun %d\n", cnt256["bounded"], n_to > "/dev/stderr"; fatal = 1 }
    c128n = cnt128["pass"] + cnt128["bounded"] + cnt128["skip"] + cnt128["boundary"]
    c256n = cnt256["pass"] + cnt256["bounded"] + cnt256["skip"] + cnt256["boundary"]
    c512n = cnt512["pass"] + cnt512["bounded"] + cnt512["skip"] + cnt512["boundary"]
    if (c128n != 190) { printf "FATAL: 128 column sums to %d\n", c128n > "/dev/stderr"; fatal = 1 }
    if (c256n != 190) { printf "FATAL: 256 column sums to %d\n", c256n > "/dev/stderr"; fatal = 1 }
    if (c512n != 190) { printf "FATAL: 512 column sums to %d\n", c512n > "/dev/stderr"; fatal = 1 }

    if (fatal) exit 1

    # ---- §5 汇总算术块（全部由上述统计机械写出，含与各 triage 结论的对账行）
    p128 = cnt128["pass"]; q128 = cnt128["bounded"]; s128 = cnt128["skip"]; y128 = cnt128["boundary"]
    p256 = cnt256["pass"]; q256 = cnt256["bounded"]; s256 = cnt256["skip"]; y256 = cnt256["boundary"]
    p512 = cnt512["pass"]; q512 = cnt512["bounded"]; s512 = cnt512["skip"]; y512 = cnt512["boundary"]
    M = OUT "/math.md"
    printf "| 列 | pass（直绿） | pass(bounded) | SKIP(legal) | BOUNDARY(gem5) | 合计 |\n" > M
    printf "|---|---|---|---|---|---|\n" > M
    printf "| 128 | %d | %d | %d | %d | %d |\n", p128, q128, s128, y128, c128n > M
    printf "| 256 | %d | %d | %d | %d | %d |\n", p256, q256, s256, y256, c256n > M
    printf "| 512 | %d | %d | %d | %d | %d |\n", p512, q512, s512, y512, c512n > M
    P = p128 + p256 + p512; Q = q128 + q256 + q512; S = s128 + s256 + s512; Y = y128 + y256 + y512
    printf "| **合计** | **%d** | **%d** | **%d** | **%d** | **%d** |\n", P, Q, S, Y, P + Q + S + Y > M
    printf "\n" > M
    printf "**%d = %d pass（%d 直绿 + %d 有界）+ %d SKIP(legal) + %d BOUNDARY(gem5)**，零 FAIL、零非法 SKIP。\n\n", P + Q + S + Y, P + Q, P, Q, S, Y > M
    printf "- **128 列 = %d pass + %d SKIP(legal)** —— 对账 triage-128 §5（189 直绿 × 2 seed + 1 合法 SKIP：fmmla@128，两 seed 同格同理）；其中 %d 格为腿内 baked 有界（triage-128 §1 的 26 项 -O 覆盖的测试）。\n", p128 + q128, s128, q128 > M
    printf "- **256 列 = %d pass** —— 对账 triage-256 §7（seed1 165 腿内直绿 + 25 格 §5 有界重跑 = 190/190；seed2 190 腿内直绿）；%d 个有界格 = %d 格 §5 有界重跑（seed1 腿内 to → 同 seed 单测 clamp 下限重跑绿）+ %d 格 baked 直绿（acl dim=512，Ruling B / triage-256 §1）。\n", p256 + q256, q256, n_to, q256 - n_to > M
    printf "- **512 列 = %d pass + %d BOUNDARY** —— 对账 triage-512 §6（181 绿 + 9 边界[5 crash + 4 timeout] + 0 fail）；%d 个有界格 = baked 26 项中绿 %d（eigen_svd_cdouble 转边界格除外）+ gem5-overrides 16 条中绿 %d（mrn_rmw_diff_1m 无效仍边界除外）− 两处重叠 %d（acl：baked 512 被 extras 覆盖为 128）。\n", p512 + q512, y512, q512, bg, eg, bo > M
    printf "- **有界格计法**：该格绿证据的产生命令里带该测试的几何 knob 覆盖——native 三处命令行头（机械提取）+ gem5-overrides.txt（键唯一契约：一测试一行，重复即被双消费者拒绝）。\n" > M
}
' "$LIST" "$N128S1" "$N128S2" "$N256S1" "$N256S2" "$G512" "$TMP/knob_baked.tsv" "$TMP/knob_extra.tsv" \
  || { echo "ERROR: matrix generation assertions failed" >&2; exit 1; }

# ---- 头部（引用性文本 + 两段机械提取的逐字引文）+ 表体 + 汇总算术装配输出。
cat <<'HDR'
# SVE 全 VL 验证运动 — 190×3 证据矩阵（Task 8 收口）

- 日期：2026-10-30（Task 8 产物）
- 分支：`feat/sve-all-vl-verification-2026-10-28`
- 生成方式：`matrix-gen.sh` 从本目录已入库工件机械生成（**零手填**，脚本一并入库，重跑即复现本文件）
- 行定义：`sve-test-list.txt` 190 行（Task 1 枚举：tag∪name 并集 193 − 3 件不在本机默认集）
- 规模：**190 × 3 = 570 格**（Ruling A：spec 的 564 为 name-grep 估算，被实测枚举取代）
- 验收判据（brief Step 2）：570 格全绿**或有显式机制性理由**（1 格合法 SKIP + 9 格 gem5 边界，见 §2/§5）

## 0. 代码基线与二进制身份

- **代码基线（Ruling G 冻结）**：`12ccda540fd4` + 本运动分支 commits。后者实测只含
  docs / campaign 工具脚本 / gem5 启动脚本（`scripts/eigen-sve-double/gem5/`），
  `git diff 12ccda54..HEAD -- tests/ framework/` 为空——**矩阵证据描述的测试代码与
  基线完全一致**（Task 7 runbook：三腿 570 格分诊 = 零测试代码缺陷）。
- 二进制：native 两腿 `sdcshield-12ccda540fd4`（四份 summary 头 `version:` 行一致，实测）；
  512 腿 gem5 `/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt`
  md5 `3f9011f40dbe80ec1df478c62ddb0b06`（Task 5 终版 9-stub 链）+ 同源 sdcshield。

### 0.1 Ruling G delta：运动期间并行合并、不在本矩阵代码基线内的 PR

本矩阵钉在冻结基线上，不追逐并行 PR（各自 PR-loop 自带验证义务）。核实方式：
`git merge-base --is-ancestor <merge> HEAD` 双双为否（实测）。

| PR | 状态 | 涉及的 campaign 用例（∈190 清单） | 与本矩阵的关系 |
|---|---|---|---|
| #302 `fix(eigen): P0-P3——NUMA 感知联合求解 sizing + 内存最大化 + OOM 防护`（merge `51868e40`） | MERGED | `eigen_gemm_double_max_arm`、`eigen_svd_cdouble_sve`、`eigen_svd_bidiag_sve`（另改 framework sizing） | **不在本矩阵代码基线内**——本矩阵 3 行（9 格）eigen 结论不描述 #302 之后的行为 |
| #316 `fix(sve): #309 同型越界四连修——谓词钳制/尾块 slack/索引纠偏 (#312-#315)`（merge `29da76ef`，2026-10-29） | MERGED | `iex_operand_combo_sve`(#312)、`partial_store_forwarding_sve`(#313)、`mrn_nuke_dense_sve`(#314)、`mesh_upi_sve_wide_asymm_read_L3_int` + `mesh_upi_sve_wide_asymm_read_only_int`(#315)——4 缺陷 5 文件 | 同上——本矩阵上述 15 格的结论不描述 #316 之后的行为 |

（完整"运动期间合并的 PR × 涉及用例"delta 表由 Task 9 文档收尾时统一列出——Ruling G。）

## 1. 512 列采集条件（triage-512.md §0 逐字引用）

HDR
cat "$TMP/sec0.quote"
cat <<'HDR2'

## 2. 格值语义与证据列

| 格值 | 含义 | 出处 |
|---|---|---|
| `pass` | 腿内默认/自适应几何下直绿（native：全核 `-t 2000` 实测；512：1 轮全量 golden 字节比对成立） | 各 summary / verdicts |
| `pass(bounded: <knob>=<v>)` | 以有界几何 knob 覆盖后绿。**@128** = 26 项腿内 baked（triage-128 §1：逐字拷贝自 256 腿 seed2，两 seed 腿内有界实测）；**@256** = 25 格 seed1 有界重跑（triage-256 §5：clamp 下限一次通过、零上调）+ seed2 腿内有界 + `acl_gemm_sve.dim=512` 标定 baked（triage-256 §1，Ruling B）；**@512** = 26 项 baked + `gem5-overrides.txt` 16 条中生效的 15 条（triage-512 §1.1/§5.2；`mrn_rmw_diff_1m` 那条无效，见其边界格） | 对应 triage 分节 |
| `SKIP(legal: F64MM UNDEFINED <VL=256)` | 架构性合法 SKIP，唯一格 = (sve512_fmmla_arm, 128)，两 seed 同格同理 | triage-128 §2 + §3 逐字源码引文 |
| `BOUNDARY(gem5: <标签>)` | gem5 侧限制边界，Ruling I 终局：`PMULL64` = gem5 无该 64-bit PMULL 编码（OpenSSL GHASH `pmull v0.1q`，2 格）；`DC-CIVAC` = SE 模式 `dc civac` 缓存维护路径 raise UndefinedInstruction（3 格）；`SIM-SPEED` = knob 在 clamp 下限或无杠杆，单轮指令量 × ~1400 仿真速超出实用预算（4 格）。native 两腿对应格全绿 | triage-512 §5.1/§5.2 |

- **证据列** = `128s1,128s2 · 256s1,256s2 · rc`。`to` = 该 seed 腿内 timed out（其绿证据 =
  triage-256 §5 有界重跑，同 seed 单测）；`rc` = gem5 进程退出码（512 列权威判定，
  triage-512 §0）。工件映射：`128s1/s2` → `native-128-LCG100{1,2}.summary.txt`，
  `256s1/s2` → `native-256-LCG100{1,2}.summary.txt`，`rc` → `gem5-512-verdicts.txt`
  （四份 summary + verdicts 的行名与清单 190 名逐一同序，生成脚本断言守卫）。
- **种子语义**：native 格 = 任一 seed pass 即 pass；实测两 seed 无分歧绿（128 腿
  189+1 双 seed 一致；256 腿唯 seed1 的 25 个超时格经 §5 重跑补绿，seed2 腿内直绿）。
- **VL 不敏感性注记**：7 个无 tag 的 ipsec 免检件（`ipsec_3des_docsis_aes_cmac_sve_wide` /
  `ipsec_3des_docsis_aes_xcbc_sve_wide` / `ipsec_aes128_cbc_aes_cmac_sve` /
  `ipsec_aes192_cbc_xcbc_96_sve` / `ipsec_aes192_ctr_xcbc_sve` / `ipsec_aes192_gcm_sve` /
  `ipsec_aes192_gcm_sve_wide`；enumeration §2.2：OpenSSL EVP 自行分派、无 SVE 数据面）
  三档 VL 行为完全相同——它们的格只构成"同条件可跑绿"证据，不构成 VL 敏感性证据。

## 3. fmmla@128 合法 SKIP 的逐字源码引文（T3 minor fix）

triage-128 §2 曾意译此注释（丢了 measured-SIGILL 关键句）；按裁定本矩阵补**逐字**引用
——`tests/cpu/arm64/sve512_fmmla_arm.cpp:193-196`（当前基线）：

```c
HDR2
cat "$TMP/fmmla.quote"
cat <<'HDR3'
```

日志侧 skip 理由原文（两 seed 同格同理，raw log `skip-reason:` 行，triage-128 §2 引）：

```
sve512_fmmla_arm: SVE vector length 128 bits is outside the supported 256..1024-bit range
```

交叉证据：同一测试在 256 腿两 seed pass（`native-256-LCG100{1,2}.summary.txt`），
512 腿 rc=0 pass 且 Exec 追踪直证 514 条 `fmmla` 真实执行（triage-512 §5.3）——
门只挡 128，不误伤更高档。

## 4. 矩阵（190 行 × 3 列）

| # | test | 128 | 256 | 512 | 证据 |
|---|---|---|---|---|---|
HDR3
cat "$TMP/table.md"
cat <<'FTNR'

† = 该测试在冻结基线上含 PR #316（#312-#315）已修复的越界缺陷：本行
pass 仅构成「同条件可跑绿」证据，**不构成基线洁净性证据**（缺陷由
#316 的检视发现，非本运动检出；见 §0.1 delta 表）。

FTNR
cat <<'HDR4'

## 5. 汇总算术（生成脚本机械统计，与 §4 表体同一数据源）

HDR4
cat "$TMP/math.md"

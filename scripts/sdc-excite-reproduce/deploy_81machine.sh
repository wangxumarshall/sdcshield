#!/bin/bash
# deploy_81machine.sh — 81 机（RCSIT TG225 B1）部署打包器（v5 §14.2 补丁单元 13）
#
# 81 机不可从本机直达——本脚本交付"repo 快照 tarball + 入役清单"，实际入役是
# 用户远端操作（不虚报已部署）。
#
# 产物（默认 dist/，--dist 可改）：
#   sdc-excite-reproduce-<git12>.tar.gz   repo 快照（git archive HEAD）+ deploy/ 附加件
#   <git12>-deploy-checklist.md           离线入役清单（五段：rasdaemon 启用前置 /
#                                         known_faults 预填 / 无 cpufreq 降级 /
#                                         温度基线差异 / 巡检模板新路径）
#
# deploy/ 附加件（tarball 内，包自含）：
#   DEPLOY-CHECKLIST.md                   清单副本（与 dist/ 产物同文）
#   known_faults.tg225b1.csv              预填模板（FAN3 0rpm + SEL #0x84 两行
#                                         log_only；源文件 configs/sdc-excite-reproduce/
#                                         known_faults.tg225b1.csv——安装到目标机数据根
#                                         后 monitor 每周期重读 = 热更新）
#
# 用法：
#   bash scripts/sdc-excite-reproduce/deploy_81machine.sh [--dist DIR] [--from-tree DIR]
#     --dist DIR      产物目录（默认 <repo>/dist/）
#     --from-tree DIR 测试模式：以 DIR 小树替换 git archive 快照步骤（tar 组装 /
#                     附加件注入 / 清单生成与生产同一代码路径——pytest 用，
#                     不打 ~204MB 真实大包；真实打包属交付验证）
set -euo pipefail

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
DIST="$REPO/dist"
FROM_TREE=""

while [ $# -gt 0 ]; do
    case "$1" in
        --dist)      DIST="$2"; shift 2 ;;
        --from-tree) FROM_TREE="$2"; shift 2 ;;
        *) echo "用法错误: 未知参数 $1（见脚本头注释）" >&2; exit 2 ;;
    esac
done

# --from-tree 校验先行（fail-fast：不留半成品）
if [ -n "$FROM_TREE" ] && [ ! -d "$FROM_TREE" ]; then
    echo "错误: --from-tree 目录不存在: $FROM_TREE" >&2
    exit 2
fi

GIT12="$(git -C "$REPO" rev-parse --short=12 HEAD)"
BRANCH="$(git -C "$REPO" branch --show-current || true)"
[ -n "$BRANCH" ] || BRANCH="detached"
DATE="$(date '+%F %T')"

# 生产模式诚实注记：git archive 只含已提交内容——工作区脏须提醒（不阻断）
if [ -z "$FROM_TREE" ]; then
    dirty="$(git -C "$REPO" status --porcelain | wc -l)"
    if [ "$dirty" -gt 0 ]; then
        echo "警告: 工作区有 ${dirty} 个未提交变更——tarball 只含已提交内容（HEAD=${GIT12}）" >&2
    fi
fi

# ---- 入役清单（离线依赖检查 + 81 机差异五段 + 步骤索引）----
# 写法约束：heredoc 无引号定界（需变量替换）——正文不用反引号，代码一律缩进块。
gen_checklist() {  # $1 = 输出文件
cat > "$1" <<EOF
# 81 机（RCSIT TG225 B1）sdc-excite-reproduce 入役清单

- 包身份: sdc-excite-reproduce-${GIT12}.tar.gz（分支 ${BRANCH}，打包 ${DATE}）
- 执行方式: 打包机不可直达 81 机——本清单由用户在目标机**远端逐项执行**；每项
  完成后在 [ ] 内打 x 并记录输出摘要。**不虚报"已部署"**——未执行的项保持未勾。
- 画像依据: v5 附录 B（docs/sdc-excite-reproduce/sdc-excite-reproduce-7x24.md）。
  与参考机（TaiShan 2280）关键差异: 96 核/2 NUMA/内存全 node0（node1 跨 HCCS 访存）/
  平台固定 2.6GHz（无 cpufreq）/BMC Hi1711 fw 3.11（132 传感器）/rasdaemon inactive。
- 操作手册: 包内 scripts/sdc-excite-reproduce/NEW_BOARD_ONBOARDING.md 第 1-7 步 +
  81 机专属段（本清单的操作化，逐项命令）。

## 1. 离线依赖检查（缺项补齐再继续；81 机管理网隔离——vendored 三方随包自带）

    [ ] rasdaemon      rpm -q rasdaemon && systemctl is-active rasdaemon
                       （81 机画像 = inactive——已知缺项，启用见 1.1）
    [ ] ipmitool       ipmitool mc info（81 机 in-band 通道已验证可用）
    [ ] perf           perf --version（内核 6.6 同源，预期在案）
    [ ] python3        python3 --version（>=3.11；工具链 stdlib only 零第三方依赖）
    [ ] stress-ng      stress-ng --version（81 机 24h 战役用过，预期在案）
    [ ] 构建链         gcc --version && meson --version && ninja --version
                       （缺 → 包内 scripts/offline-build/ 体系：有网机
                       download-deps.sh 预下载 → install-deps.sh 离线装；
                       或 third-party/rpms/ 版本树）
    [ ] vendored 三方  包内 third-party/（openssl/openblas/sleef/isa-l/acl/eigen5
                       源码随包，各 build.sh 离线可构建）

### 1.1 rasdaemon 启用前置（采集器 @ras 的 ras_edac.csv 依赖 rasdaemon 记账）

    systemctl enable --now rasdaemon
    systemctl is-active rasdaemon     # 期望 active
    ras-mc-ctl --status               # 期望 rasdaemon 记录在案

    未启用后果（如实）: collector@ras 的 EDAC 列空采（journal 流仍在）——M1 验收门
    ras_edac.csv 形态不通过；不得在未启用状态下虚报 RAS 通道就绪。

## 2. known_faults 预填（FAN3 0rpm + SEL #0x84 两行 log_only）

    # tarball 内模板 → 数据根（monitor 每周期重读 = 热更新，无需重启监控）
    install -m 644 deploy/known_faults.tg225b1.csv ~/sdc-excite-reproduce/known_faults.csv
    # 验证：恰两行数据（FAN3 Speed / Slot/Connector），行尾 log_only
    grep -v '^#' ~/sdc-excite-reproduce/known_faults.csv

    生效形态: discrete_events.log 的 TRANSITION 行尾出现 [known_fault:log_only]，
    断言告警豁免；SEL #0x84 为 Fault Asserted（非 Critical）——不触发 SEL Critical
    粘性 PAUSE，5min 增量窗口计入 sel5m 列（~8.5min 周期，约 0.6 条/5min，远低于
    风暴告警线 20 条/5min）。

    **as-built 诚实注记（用户决策点，不得静默绕过）**: 风扇联锁是模拟量安全联锁
    （FAN2/3 读数为 0 或缺失 → 3 个采样内 fan PAUSE），**不查 known_faults 白名单**——
    FAN3 恒 0rpm 会使该板战役持续 PAUSE（采集不断、压测暂停）。三选项:
      A) 维持联锁如实触发——该板只采集不压测（诚实但无战役价值）;
      B) 经用户授权后为该板型加风扇传感器豁免代码路径（独立补丁单元，本包不含）;
      C) 先修 FAN3 传感器/风扇再入役。
    v5 §15.1 安全纪律: 自动系统不得为提高事件数关闭安全联锁——选项 B 须用户明确
    决策后另行开发，本清单不代替该决策。

## 3. 无 cpufreq 列降级（预期行为，非缺陷）

    平台固定 2.6GHz、无 cpufreq、cpuinfo 无 MHz → 逐核频率不可观测:
    - percore.csv 频率列空、freq_residency.log 无数据——按设计降级;
    - monitor.csv 的 fmin/favg/fmax 列空为预期;
    - --vary-frequency / --vary-uncore-frequency skip（graceful）;
    - governor 请求路径 no-op（无 scaling_governor 可写）。
    验收口径: 上述列空/skip 不算部署失败；di/dt 激发由负载阶跃承担（v5 §7.4.1）。

## 4. 温度基线差异（vs 参考机 TaiShan 2280）

    - 传感器面不同: 81 机独有 CPU Power / MEM Power / 1711 Core Temp / SSD2 Temp /
      NIC OM Temp；参考机独有（81 机没有）N_VDDAVS / HVCC / VDDQ Temp / VRD Temp 等
      ——列集不同属预期，monitor v3 发现式列集自动适应（sensors_v3.json 持久化）。
    - 实测基线（2026-09-23 24h 战役）: 温度 56 → 91°C、功耗 234 → 402W。
    - 入役前置: 空载基线距 Tjmax >= 30°C 余量；热联锁阈值由 ACPI trips 自动推导
      （campaign.env），首日观察 monitor.csv 温度列形态并人工确认。

## 5. 巡检模板新路径

    巡检模板以 docs/sdc-excite-reproduce/operations-runbook.md（M5 运维手册）为准
    ——替换过时的 scripts/campaign 模板（v4 遗留路径，不再使用）。
    若包内该文件缺失（打包时点早于运维手册合入），以仓库 main 最新版本为准。

## 6. 入役步骤索引（NEW_BOARD_ONBOARDING.md 逐步执行 + 81 机差异段）

    [ ] 解包     mkdir sdc-excite-reproduce-${GIT12}
                 tar xzf sdc-excite-reproduce-${GIT12}.tar.gz -C sdc-excite-reproduce-${GIT12}
    [ ] 第 1 步  画像 collect_inventory.sh → 00_MACHINE_PROFILE.md 核对
                 96 核 / 2 NUMA / 内存全 node0
    [ ] 第 2 步  构建 + L0 冒烟（vendored build.sh → meson/ninja → zstd19 -n 1 pass）
    [ ] 1.1      rasdaemon 启用（先于采集器）
    [ ] 第 3 步  install.sh + smoke；第 2 节 known_faults 预填（含风扇联锁用户决策）
    [ ] 第 4 步  M1 采集器 + 0.5h / 24h 验收门
    [ ] 第 5 步  M2 四守护（消费基线顺序不可换）
    [ ] 第 6/7 步 M3 复现队列 / M4 离线分析（按需）
    [ ] 首个 4h 巡检用第 5 节新模板路径

---
生成: deploy_81machine.sh（${GIT12} / ${BRANCH} / ${DATE}）。81 机不可从打包机
直达——实际入役是用户远端操作；本清单完成度以勾选与输出摘要为准，不虚报。
EOF
}

# ---- 组装 ----
WORK="$(mktemp -d)"
trap 'rm -rf "$WORK"' EXIT
RAW_TAR="$WORK/pack.tar"

if [ -n "$FROM_TREE" ]; then
    tar -cf "$RAW_TAR" -C "$FROM_TREE" .
else
    git -C "$REPO" archive --format=tar -o "$RAW_TAR" HEAD
fi

# deploy/ 附加件注入（tar 追加后统一 gzip——附加件与快照同包）
mkdir -p "$WORK/stage/deploy"
gen_checklist "$WORK/stage/deploy/DEPLOY-CHECKLIST.md"
cp "$REPO/configs/sdc-excite-reproduce/known_faults.tg225b1.csv" "$WORK/stage/deploy/"
tar -rf "$RAW_TAR" -C "$WORK/stage" deploy

mkdir -p "$DIST"
TARBALL="$DIST/sdc-excite-reproduce-${GIT12}.tar.gz"
gzip -c "$RAW_TAR" > "$TARBALL"

# 清单双落：dist/ 产物（交付面）+ tarball 内 deploy/ 副本（包自含，上面已注入）
CHECKLIST="$DIST/${GIT12}-deploy-checklist.md"
gen_checklist "$CHECKLIST"

# ---- 完整性门 + 产物清单 ----
tar -tzf "$TARBALL" > /dev/null            # gzip/tar 完整性（坏包不出厂）
N_ENTRIES="$(tar -tzf "$TARBALL" | wc -l)"
SIZE="$(du -h "$TARBALL" | cut -f1)"

echo "== 81 机部署包打包完成 =="
echo "tarball:   $TARBALL (${SIZE}, ${N_ENTRIES} entries)"
echo "checklist: $CHECKLIST"
echo "deploy 附加件: deploy/DEPLOY-CHECKLIST.md + deploy/known_faults.tg225b1.csv（tarball 内）"
echo "入役: 81 机不可从本机直达——tarball + checklist 交用户远端执行（不虚报已部署）"

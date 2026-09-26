#!/bin/bash
# install.sh — 以 root 运行：安装 systemd 单元 + logrotate（模板替换，新单板通用）
#
# 用法（root）: bash install.sh [战役数据目录]     # 默认 /home/<仓库属主>/sdc-excite-reproduce
# 幂等：重复安装安全（覆盖式写入同样内容）。
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
RUN_USER="${SDC_USER:-$(stat -c %U "$REPO")}"
CAMPAIGN_DIR="${1:-/home/$RUN_USER/sdc-excite-reproduce}"

sed_inplace() { sed "s|@REPO@|$REPO|g; s|@CAMPAIGN_DIR@|$CAMPAIGN_DIR|g; s|@USER@|$RUN_USER|g" "$1" > "$2"; }

sed_inplace "$HERE/systemd/sdc-excite-reproduce.service.in" /etc/systemd/system/sdc-excite-reproduce.service
sed_inplace "$HERE/systemd/sdc-monitor.service.in"  /etc/systemd/system/sdc-monitor.service
sed_inplace "$HERE/systemd/sdc-collector@.service.in" /etc/systemd/system/sdc-collector@.service
sed_inplace "$HERE/systemd/sdc-root-helper.service.in" /etc/systemd/system/sdc-root-helper.service
sed_inplace "$HERE/systemd/sdc-eventd.service.in"     /etc/systemd/system/sdc-eventd.service
sed_inplace "$HERE/systemd/sdc-controller.service.in" /etc/systemd/system/sdc-controller.service
sed "s|@CAMPAIGN_DIR@|$CAMPAIGN_DIR|g" "$HERE/logrotate/sdc-excite-reproduce" > /etc/logrotate.d/sdc-excite-reproduce

# 目录树：归运行用户所有（root 监控服务补建目录时会归还属主，但这里先建好最稳）。
# spool 必须预建：root-helper 首建会得 root 属主 → 用户态 eventd/controller 不可写。
mkdir -p "$CAMPAIGN_DIR"/{logs,monitor,events,cmd,spool,stressng,inventory,monitor/snapshots,monitor/sel_events}
chown -R "${RUN_USER}:${RUN_USER}" "$CAMPAIGN_DIR"

systemctl daemon-reload
systemctl enable sdc-monitor.service sdc-excite-reproduce.service sdc-root-helper.service \
                sdc-eventd.service sdc-controller.service

echo "安装完成:"
echo "  REPO=$REPO"
echo "  数据根=$CAMPAIGN_DIR (owner=$RUN_USER)"
echo "  单元: /etc/systemd/system/sdc-{excite-reproduce,monitor,collector@,root-helper,eventd,controller}.service (enabled, 未启动)"
echo "  logrotate: /etc/logrotate.d/sdc-excite-reproduce"
echo "  采集器模板: /etc/systemd/system/sdc-collector@.service（systemctl start sdc-collector@percore 等）"
echo "  spool/ 已预建（$RUN_USER 属主——用户态 eventd/controller 可写；root-helper 首建会得 root 属主）"
echo "  M2 三守护启动顺序: root-helper → eventd →（回填完成后）m2_controller_baseline.sh → controller"
echo "启动: systemctl start sdc-monitor sdc-excite-reproduce（或经 root 运行 start.sh）"

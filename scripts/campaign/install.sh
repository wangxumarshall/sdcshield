#!/bin/bash
# install.sh — 以 root 运行：安装 systemd 单元 + logrotate（模板替换，新单板通用）
#
# 用法（root）: bash install.sh [战役数据目录]     # 默认 /home/<仓库属主>/sdc-campaign
# 幂等：重复安装安全（覆盖式写入同样内容）。
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
RUN_USER="${SDC_USER:-$(stat -c %U "$REPO")}"
CAMPAIGN_DIR="${1:-/home/$RUN_USER/sdc-campaign}"

sed_inplace() { sed "s|@REPO@|$REPO|g; s|@CAMPAIGN_DIR@|$CAMPAIGN_DIR|g; s|@USER@|$RUN_USER|g" "$1" > "$2"; }

sed_inplace "$HERE/systemd/sdc-campaign.service.in" /etc/systemd/system/sdc-campaign.service
sed_inplace "$HERE/systemd/sdc-monitor.service.in"  /etc/systemd/system/sdc-monitor.service
sed "s|@CAMPAIGN_DIR@|$CAMPAIGN_DIR|g" "$HERE/logrotate/sdc-campaign" > /etc/logrotate.d/sdc-campaign

# 目录树：归运行用户所有（root 监控服务补建目录时会归还属主，但这里先建好最稳）
mkdir -p "$CAMPAIGN_DIR"/{logs,monitor,events,cmd,stressng,inventory,monitor/snapshots,monitor/sel_events}
chown -R "${RUN_USER}:${RUN_USER}" "$CAMPAIGN_DIR"

systemctl daemon-reload
systemctl enable sdc-monitor.service sdc-campaign.service

echo "安装完成:"
echo "  REPO=$REPO"
echo "  CAMPAIGN_DIR=$CAMPAIGN_DIR (owner=$RUN_USER)"
echo "  单元: /etc/systemd/system/sdc-{campaign,monitor}.service (enabled, 未启动)"
echo "  logrotate: /etc/logrotate.d/sdc-campaign"
echo "启动: systemctl start sdc-monitor sdc-campaign（或经 root 运行 start.sh）"

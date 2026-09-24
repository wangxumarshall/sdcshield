#!/bin/bash
# stop.sh — 停止战役（需 root）。断点状态保留在 state.json，重启后续跑。
set -eu
if [ "$(id -u)" -ne 0 ]; then
    echo "需 root 运行（systemctl stop 权限）。例如: su -c '$0' 或 sudo $0" >&2
    exit 1
fi
systemctl stop sdc-campaign.service sdc-monitor.service
echo "已停止。进度: $(cat "${SDC_CAMPAIGN_DIR:-$HOME/sdc-campaign}/state.json" 2>/dev/null)"

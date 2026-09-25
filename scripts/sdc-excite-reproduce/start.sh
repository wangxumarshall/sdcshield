#!/bin/bash
# start.sh — 启动 7×24 战役（需 root：systemctl start）
set -eu
if [ "$(id -u)" -ne 0 ]; then
    echo "需 root 运行（systemctl start 权限）。例如: su -c '$0' 或 sudo $0" >&2
    exit 1
fi
systemctl start sdc-monitor.service sdc-excite-reproduce.service
systemctl --no-pager status sdc-monitor.service sdc-excite-reproduce.service | head -12

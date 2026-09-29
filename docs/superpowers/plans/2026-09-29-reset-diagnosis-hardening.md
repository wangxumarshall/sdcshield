# 复位根因取证：历史诊断 + 下次复位取证加固 plan

> 执行日：2026-09-29。目标机：TaiShan 2280（本机，战役运行中——**全程不中断战役、不主动重启机器**；所有改动幂等可回滚、grub 改动仅在下次自然 boot 生效）。

## 背景

战役期三次非计划复位（09-26 20:41 / 09-27 00:16 / 09-28 23:08），统一签名：全部落盘写入者在复位前 **6.2–8.2 分钟同时停摆**、末拍供电/温度正常、wtmp 无干净关机记录、无内核痕迹（journal 非持久 + pstore 空 + kdump 未配）。主假设：**存储 I/O 挂起 → 全系统无响应 → 复位（BMC watchdog 或电源事件）**；复位 #3 后 BMC SDR 磁盘类传感器消失佐证存储子系统卷入。root 通道已随复位丢失，用户已提供密码（经 env 每次传入，不落盘）。

## Phase A — 历史三次复位取证（只读）

| # | 动作 | 判读 |
|---|---|---|
| A1 | 重建 root 通道 `/tmp/sdc_inv_stage/su_run.py`（env 读密码，不入文件） | 通道可用性 |
| A2 | `ipmitool sel list` 全量 + 三时点前后记录（reset/watchdog/power/crit/voltage） | 谁执行了复位（watchdog expired？PSU？） |
| A3 | `ipmitool mc watchdog get` + `lsmod | grep ipmi` | BMC 看门狗是否开启/超时窗/谁在喂 |
| A4 | `/var/log/messages` 三时点前末 60 行（root 可读，**跨 boot 持久——过去复位的直接证据**） | 挂死前的最后内核/服务记录（hung task？I/O error？静默？） |
| A5 | `dmesg` 当前 boot 存储/内核错误 | 当前纪元健康 |
| A6 | `smartctl -H -A /dev/sda` + smartd 状态 | 磁盘健康基线 |
| A7 | `/proc/cmdline`、kexec-tools/kdump 安装状态、`/var` 与 `/` 空间、rasdaemon/GHES | Phase B 可行性 |

## Phase B — 下次复位取证加固（写配置，全部备份原文件）

| # | 配置 | 内容 | 生效时点 |
|---|---|---|---|
| B1 | **journal 持久化** | `/etc/systemd/journald.conf`: `Storage=persistent`；`mkdir /var/log/journal`；`systemctl restart systemd-journald`（安全，不杀服务）；加 `SystemMaxUse=2G` 限幅 | 立即（本 boot 起持久） |
| B2 | **kdump/vmcore** | 检查/安装 `kexec-tools`；grub `GRUB_CMDLINE_LINUX` 追加 `crashkernel=512M`；`grub2-mkconfig`；`/etc/kdump.conf`: `path /var/crash` + `core_collector makedumpfile -l --message-level 1 -d 31`（压缩+剥用户页，32GB 机器 vmcore 预计 1-3G）；`systemctl enable kdump` | **下次 boot**（crashkernel 预留需 boot 时生效——恰是被复位拉起的那次） |
| B3 | **挂死转 panic**（让 kdump 能抓"挂死"类复位） | `/etc/sysctl.d/99-reset-diagnosis.conf`: `kernel.hung_task_panic=1`（默认 120s 阈值）、`kernel.softlockup_panic=1`、`kernel.sysrq=1`；`sysctl --system` | 立即 |
| B4 | **smartd** | 已装则 `systemctl enable --now smartd`；未装则记录（不联网安装） | 立即 |
| B5 | **BMC watchdog** | 仅按 A3 结果记录；不默认改（误配会导致周期性假复位） | — |
| B6 | （默认不做，记 runbook 待办）netconsole/远端 syslog | 需第二台接收机 | — |
| B7 | 收尾 | 运维手册 §9.3 增"复位取证能力清单"；plan + 手册变更 commit；记忆更新 | — |

## 风险与边界（诚实声明）

- B3 使"静默挂死"变为"panic → vmcore → 自动重启 → 战役 systemd 断点续跑"：把**必丢证据的复位**换成**留 vmcore 的复位**，战役中断时长与现状相当（现况本就以复位收场）；正常重 I/O 不会触发 120s D-state/softlockup。
- B2 的 crashkernel 预留 512M 内存在下次 boot 前不生效——**中间再发生一次复位则该次仍无 vmcore**，journal/messages 持久化仍能覆盖。
- 不主动重启机器（不中断战役）；全部改动先备份 `/etc/*.bak-20260929`。
- vmcore 落 `/var/crash`（先核实根分区空间）；SEL/messages 均跨复位持久。

## 验收

- A 项结论入报告（三复位定性或排除若干假设）；
- B1: `journalctl --list-boots` 有历史 + 新日志落 /var/log/journal；
- B2: grub.cfg 含 crashkernel=512M、kdump enabled、`kdumpctl status` 如实（未 reboot 前为未加载）；
- B3: `sysctl kernel.hung_task_panic kernel.softlockup_panic kernel.sysrq` 输出 1/1/1；
- 战役服务全程 active、driver.log 推进。

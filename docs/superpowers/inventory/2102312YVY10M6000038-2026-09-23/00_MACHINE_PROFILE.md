# 机器档案：Huawei TaiShan 200 (Model 2280) — SN 2102312YVY10M6000038

画像日期：2026-09-23 ｜ 采集人：Claude（sdcshield 7×24 战役 Phase 1）｜ 原始输出见本目录 01–15 号文件

## 1. 档案表

| 维度 | 值 | 来源文件 |
|---|---|---|
| 整机 | Huawei TaiShan 200 (Model 2280)，基板 BC82AMDD，BIOS 7.44（2025-07-03，Huawei） | 08/09/10 |
| 系统 SN / UUID | 2102312YVY10M6000038 / e795379b-a416-a7e7-f011-219d1ece8528 | 08 |
| 架构 | aarch64（ARMv8.2，无 SVE），HiSilicon Kunpeng 920 7260 × 2 socket | 01/11 |
| 核数 | 128 逻辑 CPU = 2 socket × 64 core × 1 thread（**无 SMT**），stepping 0x1 | 01/11 |
| 拓扑 | 4 NUMA 域：node0=CPU0-31，node1=32-63，node2=64-95，node3=96-127；每 NUMA 域一个 32MB L3（共 128MB）；L2 512KB/核私有；L1d/L1i 64KB/核 | 01 |
| ISA 特性 | fp asimd aes pmull sha1 sha2 crc32 atomics fphp asimdhp cpuid asimdrdm jscvt fcma dcpop asimddp asimdfhm（**无 sve/sve2** → sleef_sve、eigen SVE 用例将 clean-skip） | 01 |
| 主频 | cppc_cpufreq 驱动，performance governor，**128 核全部钉在 2600 MHz**（min 200 / max 2600 MHz），boost=0（disabled）。频率作为可操作杠杆存在（CLAUDE.md 记录的"无 cpufreq"是另一块 SP3/192核 板，本机 SP4 有 cpufreq） | 01 + 本文件 §4 |
| 内存 | **32 GB 总量 = 2 × 16GB DDR4-2933 RDIMM（Hynix），Multi-bit ECC**；32 槽仅占 2：DIMM000 J27（Socket0 Ch0）、DIMM100 J43（Socket1 Ch0）；**NUMA0/NUMA2 无本地内存**（MemTotal=0，访问全走远端） | 12 + 01 |
| RAS 栈 | GHES/APEI firmware-first 已启用、ERST、EDAC ghes_edac（mc0，ce=0 ue=0，32 DIMM 槽）、rasdaemon 0.8.0 active+enabled、kdump active+enabled（crashkernel=1024M）；内核 cmdline：nospectre_bhb arm64.nopauth | 13/14 + 01 |
| OS | openEuler 24.03 LTS-SP4，内核 6.6.0-159.4.3.154.oe2403sp4.aarch64 | 01 |
| 工具链 | gcc/g++ 12.3.1、cmake 3.31.12、meson 1.3.1、ninja 1.13.0；stress-ng 0.18.12、sensors 3.6.0、perf、cpupower 可用；**缺**：numactl、hwloc/lstopo、turbostat（x86-only）、stress | 01 |
| BMC | 9.3.1.8（静态），IPMI 通道 /dev/ipmi0（root-only）经 su 可用 | 02 + 采集过程 |
| 磁盘 | / 46G 可用；**/home 124G 可用（88% 已用，战役日志与取证放这里）** | 01 |

## 2. 基线读数（2026-09-23 19:21，本底负载下：llama-server 16t + qemu 4vCPU + TDengine + piper 突发）

| 项 | CPU1 侧 | CPU2 侧 |
|---|---|---|
| Core Rem 温度 | 65–66°C | 48°C（不对称：本底负载集中在 CPU1 侧） |
| MEM 温度 | 37°C | 30°C |
| VRD / VDDQ 温度 | 41 / 38°C | 37 / 38°C |
| VDDAVS / N_VDDAVS / VDDFIX | 0.88 / 0.91 / 0.80 V | 0.88 / 0.89 / 0.80 V |
| HVCC / VDDQ_AB/CD | 1.20 / 1.25 / 1.25 V | 1.19 / 1.25 / 1.23 V |
| Prochot | 0x0（未断言） | 0x0 |
| 整机功耗 | 276 W（PS2 VIN 220V，POut 245W） | — |
| 风扇 | FAN2 11475 RPM、FAN3 11550 RPM ok；**FAN1/FAN4 no reading**（Presence=ok） | — |
| ACPI 热区 | TZ0=65.9°C（对应 CPU1）、TZ1=48.5°C | |

## 3. 固件保护阈值（战役安全联锁的依据）

| 传感器 | Critical 阈值 | 备注 |
|---|---|---|
| CPU1/CPU2 Core Rem | **105°C**（=Tjmax，ACPI trip 同为 105°C） | 战役硬阈取 **Tjmax−10 = 95°C** |
| CPU MEM Temp | 95°C | |
| VDDQ/VRD Temp | 120°C | |
| Outlet Temp | 75°C | |
| Inlet Temp | 46°C | |
| 电压跌落 unc | N_VDDAVS 0.77V / VDDAVS 0.73V / VDDFIX 0.72V / HVCC·VDDQ 1.08V | 供电骤降监测线 |

## 4. RAS/事件现状

- EDAC mc0：ce_count=0，ue_count=0（干净基线）
- SEL：691 条（34% 用量，无溢出），**0 条 Critical**；2026-08-20 19:00–19:10 有一段 "Temperature #0x04 Upper Non-critical" 断续越限（非致命温度边际事件）；"Event Logging Disabled #0x76" 每日周期性 assert/deassert（BMC 记录管理行为，非硬件故障）
- dmesg/journal：无 MCE/EDAC/thermal 故障记录

## 5. 异常与风险清单（影响战役设计的事实）

1. **内存极度稀疏**：32 槽 2 条、单通道/路，理论带宽 ~23GB/s/socket；**全核(128线程) GEMM mdim=4096 需 ~49GB scratch —— 本机不可行**；mdim 预算推导见 plan。NUMA0/2 无本地内存 → 所有跨域访问走互联（memcpy_rewr 跨 NUMA 策略反而是压互联的好机会）。
2. **非测试专用机**：llama-server（16 线程，自 9/7 常驻）、qemu VM（4 vCPU）、TDengine、piper TTS 突发（7 核）——压测与本底负载互相污染（逐核隔离、频温基线）。需用户决策。
3. **NTP 未同步**：chronyd 全部源 reach=0（UDP/123 疑被网络策略挡），时钟自由运行。
4. **FAN1/FAN4 无读数**（Presence=ok）：风扇冗余存疑，战役期间监控 FAN2/3 + SEL Critical 兜底。
5. **PS1 传感器缺失**：疑单电源供电，7×24 断电风险自担（PSU 告警联锁只盯 PS2）。
6. cluster_id 单调递增（138/4314/8544/12720）、physical_package_id 大值（36/8442）—— ACPI-PPTT 固件伪影，非缺陷，按原样读。
7. /home 88% 已用（124G 剩余）：日志水位线 85%/95% 必须生效。

## 6. 想采但采不到的项（如实记录）

| 项 | 原因 | 影响/替代 |
|---|---|---|
| 每 CPU 核温度（128 点） | BMC 只暴露每 socket 一个 "Core Rem" 遥测（最热点） | 以 2 点 Core Rem + ACPI TZ + Prochot 替代；阈值联锁用 95°C |
| 官方 Tjmax 数据手册值 | 华为未公开 Kunpeng 920 datasheet；本环境外网搜索不可用 | 采用**本机固件阈值 105°C**（ACPI trip + BMC Critical 一致）作为权威值 |
| 当前每核实时主频（负载下波动） | cppc_cpufreq 无 scaling_cur_freq 粒度差异时 performance governor 恒 2600MHz；无 ARM 版 turbostat | 频率侧以 governor/setspeed 操作 + 功耗/温度侧证 |
| numactl/lstopo 拓扑工具 | 未安装（网络可达可装） | sysfs 直读已覆盖拓扑；战役脚本用 --cpuset 拓扑语法不依赖 numactl |
| 真·空载基线 | 本底服务常驻 | 基线=「本底负载下」基线，如实标注 |

## 7. 对战役参数的直接推导（禁止硬编码 → 全部溯源本表）

- 线程数上限：128（-n 0 = 全核）；eigen ULP 敏感用例 -n 1 补跑（CLAUDE.md 已知多核数值噪声）
- GEMM mdim 档：64（L1）/256（L2，默认）/512/1024（LLC，全核 3.2GB scratch）/2048（DRAM，需 -n ≤64 或内存水位联锁，全核需 12.9GB）；**4096 排除**
- SLEEF nelems：1024/16384/262144（L1→6MB 足迹）
- 测试集裁剪：无 SVE → 主力 NEON 家族（openblas gemm×4、sleef_neon、eigen、acl_gemm、pocketfft、isal、zstd、openssl/ipsec、fma、cachebounce、lock、atomic_simd、memcpy_rewr）
- 温度硬阈：95°C（Tjmax−10）；Prochot 断言=记录并告警；MEM 90°C、Outlet 70°C 预警线
- 逐核隔离单位：4×32 核 L3 域（p0/p1/p2/p3 语法）轮转 + 命中后 --cpuset 二分定位
- 日志/证据目录：/home 下独立目录（不入 git），摘要与事件台账入仓库

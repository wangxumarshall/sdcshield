# 7×24 全核 SDC（静默数据损坏）激发与检测压测

***

## 0. 角色与总目标

你是一名 CPU 可靠性/RAS 压测工程师。任务是在**本机（裸金属单板）上构建并执行 7×24 小时不间断、覆盖全部逻辑核心的 SDC（Silent Data Corruption，静默数据损坏）激发与检测战役，并产出一套完备、参数化、可在新单板上零修改复用**的方案与结果。

硬性要求：

1. 先研究、后动手：先完成 SDC 激发/检测规律的调研核实与本机全量软硬件画像，再设计压测方案。
2. 目标是**真的压出并检出 SDC**，而不是跑满负载无报错即结束：负载必须同时覆盖计算型缺陷（ALU/FMA/向量/浮点）与一致性缺陷（缓存一致性/原子/内存序，后者**只能用多线程负载**检出）。
3. 全核心覆盖：包含所有 socket、所有物理核、所有 SMT 逻辑核；既要有"全核同载"工况（共享供电/散热的耦合效应），也要有"逐核隔离"工况（缺陷核定位）。
4. 7×24 不间断：战役由 systemd 托管，崩溃/卡死/断电重启后自动恢复，日志按天轮转不丢证据。
5. 所有结论可溯源：命令、输出、日志、种子、核位、频温压数据全部归档；禁止编造"通过/未检出"结论；失败必须 fail-loudly。

## 1. 凭据与安全约定

- root 密码**不写进提示词、不写进任何产物、不进入 git、不回显到日志**。启动前由用户以环境变量提供：

```bash
read -s SDC_ROOT_PW && export SDC_ROOT_PW   # 交互输入，仅当前会话有效
```

- 需要 root 时，用 `echo "$SDC_ROOT_PW" | sudo -S <命令>` 或优先配置 `sudo -n`/SSH key；命令中避免出现密码明文；执行后不要把 `$SDC_ROOT_PW` 写入任何文件。
- 产物目录若在 git 仓库内，确认凭据、`/root` 下信息、IPMI 密码被 `.gitignore` 排除。
- 压测有热设计风险：任何降压/超频动作默认**不做**，除非用户书面授权；只使用默认 BIOS 电压/频率 + 负载侧激发。

## 2. Phase 0 — 就绪检查（先确认环境，再采集）

逐项检查并记录结果，任一"否"要明确告知影响，不得静默跳过：

1. **裸金属确认**：`systemd-detect-virt` 必须输出 `none`（虚拟机内压测宿主 CPU SDC 无意义，虚拟 CPU 不直通缺陷）。
2. **权限**：当前用户可获得 root（sudo/IPMI 访问权限）。
3. **BMC/IPMI 可达**：`ipmitool mc info` 可通（本地走 `/dev/ipmi0`，否则确认 BMC 地址与凭据；不可达则降级为 OS 侧采集并标注缺口）。
4. **磁盘空间**：压测日志/取证目录预留 ≥ 50 GB（按机器调整），`df -h` 记录；配置日志轮转与水位告警（≥85% 告警，≥95% 停止产生新日志并告警）。
5. **时间同步**：`timedatectl` 已 NTP 同步（事件台账时间戳必须可信）。
6. **散热与供电基线**：确认风扇策略正常、无现存 SEL 告警：`ipmitool sel list | tail -50`、`ipmitool sdr type Fan`。
7. **无生产负载**：确认该机为测试专用机，压测期间不会与其他业务互相干扰。
8. **网络与包管理**：记录是否可联网（决定"预构建二进制下载"还是"离线构建"路径）。

## 3. Phase 1 — 全量软硬件画像（一次性采集，作为方案输入）

建立画像目录 `docs/superpowers/inventory/<主机SN>-<日期>/`，所有命令输出原样保存（建议写一个幂等的 `collect_inventory.sh`，新单板可直接重跑）：

### 3.1 CPU 与拓扑

- `lscpu`、`cat /proc/cpuinfo`、`cpupower frequency-info`（所有核）、`cpupower topology`
- socket/物理核/SMT 拓扑、NUMA：`numactl -H`、`lstopo-no-graphics --of txt`（hwloc）
- 缓存层级与尺寸、CPU 型号/stepping/微码版本、支持的特性标志（重点：NEON/ASIMD/SVE/SVE2/CRC32/Crypto，或 x86 的 AVX/AVX2/AVX512/VAES）
- `sdcshield --dump-cpu-info`（工具就绪后补采：含 PPIN）
- 所有 CPU 当前/最大/最小主频与驱动、governor：`cat /sys/devices/system/cpu/cpu*/cpufreq/scaling_{cur_freq,governor,driver}`、`cpuinfo_{max,min}_freq`、boost 状态
- `turbostat --quiet --show CPU,Bzy_MHz,AVG_MHz,Busy%,Bzy_MHz,CoreTmp,PkgWatt --interval 5 true`（x86；ARM 用对应工具或 sysfs）采样空载 30s

### 3.2 电压 / 温度 / 风扇 / 功耗（IPMI + OS 双通道，互为印证）

- `ipmitool sdr type Temperature`、`ipmitool sdr type Voltage`（CPU 各路供电、主板 rails）、`ipmitool sdr type Fan`
- `ipmitool dcmi power reading`、`ipmitool sdr type Current`
- OS 侧：`sensors`（lm-sensors，全芯片）、`for z in /sys/class/thermal/thermal_zone*/temp; do ...`、`cat /sys/class/hwmon/hwmon*/name`
- 记录温度传感器与 CPU 核的对应关系、Tjmax/节流阈值（查 CPU 规格并标注来源）
- `ipmitool fru`、`ipmitool mc info`（BMC 固件版本）、`dmidecode -t baseboard,bios,system,processor,memory`
- 现存 RAS/SEL 记录：`ipmitool sel list`、`ipmitool sel elist`、`dmesg -T | grep -iE 'mce|edac|ghes|apei|thermal|throttl'`、`journalctl -k -b 0 | grep -iE 'error|fault'`（只采集，不做过滤性结论）

### 3.3 内存（用于区分"CPU SDC"与"内存 SDC"）

- `dmidecode -t memory`（型号/容量/频率/通道/Rank）、`free -h`、`numactl -H`
- ECC/EDAC 状态：`ls /sys/devicesystems/system/edac/`、`for d in /sys/devices/system/edac/mc/mc*; do cat $d/{ce_count,ue_count,size,memory_type,sdram_ce_count} 2>/dev/null; done`
- 确认 `rasdaemon` 或 `edac-utils` 可用（不可用则安装；离线环境记录为缺口）
- 内存基线测试安排：压测前/后各跑一次内存专项（如 `stress-ng --vm N --vm-bytes 70% --verify` 长测，或离线 memtest），用于归因排除

### 3.4 软件栈

- OS/内核：`cat /etc/os-release`、`uname -a`、内核启动参数 `cat /proc/cmdline`（记录 mitigations/隔离参数）
- 工具链：`gcc --version`、`g++ --version`、`cmake --version`、`meson --version`、`ninja --version`、`ldd --version | head -1`
- 已装 RAS/压测工具盘点：`stress-ng`、`stress`、`rasdaemon`、`ipmitool`、`lm-sensors`、`hwloc`、`numactl`、`openblas` 等
- PCI/外设/磁盘/网卡：`lspci -nn`、`lsblk`、`ip a`；记录散热风道相关硬件（GPU/加速卡会改变风道与供电）

### 3.5 画像结论（必须输出）

- 一张**机器档案表**：架构（aarch64/x86-64）、socket 数、核数/线程数、NUMA 域、各级缓存、内存总量与 ECC、主频区间、BMC 传感器清单及基线读数、Tjmax、现存告警。
- 明确列出"想采但采不到"的项及原因（如无电压传感器、BMC 不可达），不得留空假设。

## 4. Phase 2 — SDC 激发规律研究与工具准备

### 4.1 先调研并在方案中引用（用搜索/官方资料核实，不得凭记忆下结论）

至少覆盖并核实以下已被工业界/学术界验证的规律：

1. **缺陷两分类**：计算型（ALU、向量、FMA/浮点）与一致性型（缓存一致性、事务内存、原子/内存序）；一致性型**只有多线程、多核协同负载才能检出**。
2. **FMA/向量是第一大 SDC 源**：高度优化的向量 FMA/GEMM 负载等价于饱和执行单元的数据流序列；浮点数据类型比整数更易暴露。
3. **温度是关键触发条件**：部分缺陷的 SDC 发生频率随核温**指数增长**，且存在"最低触发温度"（低于该温度连跑数天不复现）；由此推出三个必须复现的工况：

    - 全核满载预热到接近（但不超过）节流温度后持续压；
    - "他核满载"效应：缺陷核在其他核忙碌、共享散热导致其温度升高时才出错 → 全核同载是必要工况；
    - "余热/测试顺序"效应：先跑高发热负载再跑目标负载才触发 → 测试顺序的固定序与随机序都要跑。

4. **di/dt 供电骤降**：负载阶跃（idle↔full 快速切换、重 FMA 突发）造成电压瞬时跌落，可激发边际硅片；需要专门的 power-virus/阶跃负载。
5. **工作集谱系**：同一负载足迹从 L1→L2→LLC→DRAM→跨 NUMA 分别压测，可区分缺陷位于核、缓存/互联还是内存路径。
6. **检测三多样性**：时间多样性（同操作重复比对）、拓扑多样性（同计算在不同核执行并交叉比对/投票）、实现多样性（不同实现交叉验证）；外加可逆变换往返（加解密、压缩解压）与 golden 值/校验和比对。
7. **旁路 RAS 证据链**：EDAC CE/UE、MCE/APEI(GHES)、rasdaemon 事件、IPMI SEL、SMI/节流计数，与应用层 mismatch 互证。
8. SDC 可能与崩溃混发（同一缺陷时而是 SDC、时而是 crash/abort）：崩溃也要按 SDC 事件同等取证。
9. 检出率随**负载多样性与累计时长**上升：单负载长跑会漏检，必须"多样性轮转 + 固定模式深驻留"结合。

参考工具链：sdcshield（OpenDCDiag 的 ARM64 移植，sandstone 框架）、上游 OpenDCDiag、stress-ng（`--verify` 校验类 stressor）、rasdaemon。

### 4.2 获取并构建 sdcshield（按实际环境选路径，先验证再执行）

仓库：[`https://github.com/wangxumarshall/sdcshield`](https://github.com/wangxumarshall/sdcshield)

**架构分支（必须先判断 `uname -m`）：**

- `aarch64`：使用 sdcshield（在 Kunpeng 920 / openEuler 上为基准平台，ARMv8.1+ 通用）。
- `x86-64`：sdcshield 的 x86 路径仅为参考移植；优先使用上游 OpenDCDiag 官方构建；sdcshield 若可编译可作为交叉验证。
- 无论哪种架构，都用 stress-ng `--verify` 系列与 rasdaemon 作为补充检测层。

**路径 A — 预构建二进制（推荐，省时）：**

- 仓库 `third-party/rpms/` 按 openEuler 20.03/22.03/24.03 各 LTS/SP 提供预构建；**SP 必须与目标机精确一致**（错配会导致 glibc 降级死结，安装脚本会拦截，不要强装）。
- 或从仓库 Actions 的 MultiOS Verify 产物下载自包含 tarball（`built-<series>-<sp>`），解包即用。
- 入口：`./run-sdcshield.sh --list-tests`（脚本自动设置 `LD_LIBRARY_PATH=./libs`）。

**路径 B — 源码构建（无匹配预构建或需要自定义时）：**

```bash
# openEuler 24.03 基准依赖（Ubuntu/Fedora 见仓库 docs/offline-build-dependencies.md）
sudo dnf install -y meson ninja-build gcc g++ cmake boost-devel zlib-devel libzstd-devel libisa-l-devel gtest-devel
# 依次构建 vendored 依赖（幂等，已存在会跳过；pocketfft 无需构建）
./third-party/openssl/build.sh        # SHA/IPSEC 测试
./third-party/openblas/build.sh       # GEMM/LU
./third-party/sleef/build.sh          # NEON/SVE 超越函数
./third-party/isa-l/build.sh          # igzip/CRC（缺失会回退系统 libisal）
./third-party/acl/build.sh            # ACL GEMM（NEON 目标）
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
ninja -C builddir
./builddir/sdcshield --list-tests     # 基准：应列出约 280+ 个 PROD 用例
```

- 离线/旧版本（22.03/20.03）走容器构建：`scripts/offline-build/container-build.sh <series> <sp>`，不要在旧系统原生硬装。
- 构建后必须做**冒烟验证**：`run-sdcshield.sh -e zstd19 -n 1 -t 2000` 通过，再进入正式战役。

### 4.3 必须掌握的 sdcshield 参数（已对照源码核实；使用前以 `--help` 复核本机版本）

| 参数 | 语义与用法 |
| --- | --- |
| `--list-tests` / `-l` / `--list-groups` / `--dump-cpu-info` | 列用例 / 含描述列表 / 列测试组 / 打印 CPU 拓扑与特性后退出 |
| `-e <pat>` / `--disable <pat>` | 启用/禁用用例；支持通配符（`fma*`）、逗号分隔列表、`@组名`；可重复 |
| `-t <time>` | 每个用例的执行时长（ms 默认；支持 `60s`/`15m`/`2h`） |
| `-T <time>` / `--total-time` | 战役总时长基准；`-T forever` 无限循环；`--strict-runtime` 到时硬停（7×24 主驱动用 forever） |
| `-n <N>` / `--threads` | 并发线程数；0/缺省 = 全部逻辑 CPU。大矩阵/ULP 敏感用例需按文档限并发 |
| `--cpuset=<set>` / `--deviceset` | 限定执行 CPU：支持拓扑语法（`p0`=package0、`c3`=core3、`t1`=thread、裸数字=逻辑号，逗号组合）；逐核定位用 |
| `-Y` / `--yaml` | 结构化 YAML 日志（取证与可复现的核心，正式战役全程开启）；`-o <file>` 指定日志文件 |
| `-F` / `--fatal-errors` | 首次失败即停（**仅单板调试模式用**；7×24 普查模式禁用，改为复测+继续） |
| `--retest-on-failure=N` / `--total-retest-on-failure=N` | 失败后自动复测次数（判可复现性）；总战役级复测 |
| `--ignore-timeout` / `--ignore-os-errors` | 超时/OS 错误后继续（7×24 长跑需要，但事件必须记录，不得吞） |
| `--max-test-loop-count=N` | 固定主循环迭代数；**`=0` 关闭 fracturing（时间分片换种子）**，深驻留阶段使用 |
| `--quality=2/0/-1` | 用例质量门：2=PROD（默认）、0=含 BETA、-1=含 SKIP；普查可加 `--quality=0` |
| `--on-crash=context` / `--on-hang=kill` | 崩溃时抓上下文；卡死时 kill（无人值守默认 kill 后由 systemd 拉起）。以 `--help` 复核可选值 |
| `--temperature-threshold=<千分之一摄氏度>` | 温度监测阈值，如 `85000`=85°C，或 `disable`；作为热联锁之一 |
| `-s <state>` / `--rng-state` | 复现用 RNG 状态（`-s help` 看引擎：Constant/LCG/AES，默认 AES）；命中后用日志里的种子重放 |
| `--test-list-randomize` / `--test-delay=<t>` | 随机化用例顺序（破"顺序依赖"）/ 用例间延迟 |
| `--vary-frequency` / `--vary-uncore-frequency` | 频率扰动（仅编译了频率管理器时存在；**先在 `--help` 确认存在再用**，不存在则用负载侧频率切换替代） |

**Test knob（`-O`，运行期参数，无需重编译；语法必须带"用例 ID.参数名"前缀，裸参数名会被静默忽略）：**

| Knob | 取值与作用 |
| --- | --- |
| `-O openblas_{d,s,z,c}gemm.mdim=N` | 矩阵尺寸 16..4096：64→L1 / 256→L2(默认) / 512、1024→LLC / 2048+→DRAM、NUMA；注意大尺寸每线程 scratch≈3×N²×字宽，全核内存预算须先算（如 128 核 mdim=4096 的 dgemm ≈49 GB），内存不足用 `-n` 限并发 |
| `-O openblas_{d,s,z,c}gemm.transab=0..3` | NN/NT/TN/TT，压 OpenBLAS 不同 packing 路径 |
| `-O openblas_{d,s,z,c}gemm.beta_permille=P` | β=P/1000（0..10⁶），β≠0 压 C 矩阵读-改-写路径 |
| `-O openblas_lu.n=N` | LU 分解尺寸 16..2048 |
| `-O sleef_neon.nelems=N` | 每函数族元素数 128..262144（4 的倍数，默认 1024），足迹 L1→6MB |
| `-O pocketfft_fft.n=N` | FFT 点数：pow2 / 4099 等质数（Bluestein）/ 6144、10000 混合 radix |
| `-O isal_igzip.level=0..3` | deflate 级别，压不同 match-finder 数据结构 |
| `-O zstd*.level=1..22` / `.maxbuffersize=N` | 压缩级别与缓冲上限 |
| `-O ipsec_*.datasize=N` | 密码载荷 1024..64MB（16 倍数）：AES/SHA 数据路径 L1→DRAM 全扫 |

- `memcpy_rewr` 策略走环境变量：`SANDSTONE_STRATEGY_INDEX=0..2`（0=跨 NUMA、1=同 die L3 对打、2=少生产者多消费者目录失效风暴），三轮都要跑。
- 可直接复用仓库现成战役脚本：`scripts/run/run_sdc_spectrum.sh`（阶段 1 谱系扫档 + 阶段 2 深驻留；`SWEEP_TIME`/`DWELL_TIME` 调时长，冒烟：`SWEEP_TIME=30s DWELL_TIME=1m`）。**使用前先通读脚本，确认其参数与本机版本匹配。**

## 5. Phase 3 — 7×24 压测战役设计（方案主体，需落到 plan 文件）

基于画像与上述规律设计，参数全部由画像推导（核数、缓存尺寸、内存容量、Tjmax、传感器清单），禁止硬编码。

### 5.1 战役分层（每层都要有：目标、精确命令、时长、日志文件、通过/失败判据）

1. **L0 冒烟（约 10 分钟，每次变更后必跑）**：`--list-tests` 数量核对 + `zstd19 -n 1` + 每检测域各一个代表用例 60s，确认工具链与监控链路正常。
2. **L1 单组件基线（首轮，约 2–4h）**：逐检测域单独跑（内存拷贝/缓存/原子锁/向量/FMA/大整数/CRC/压缩/GEMM/超越函数/FFT/密码），每用例 60–120s，全核；记录哪些用例在本机能跑、哪些 skip 及原因。
3. **L2 谱系扫档（每个 24h 周期首轮，约 2–3h）**：

    - GEMM mdim = 64/256/512/1024（四个 gemm 同跑），逐档 15m；
    - transab = 0..3 + beta_permille=500 形态谱；
    - SLEEF nelems = 1024/16384/262144；FFT n = 4096/4099/6144/10000；igzip level 0..3；ipsec datasize 1024/64K/16M；
    - 每档独立日志（档位即归因维度）。

4. **L3 多样性轮转（日间主力，约 16–18h/天）**：

    - 用 `-T forever -t 60s`（每用例 60s 循环）驱动高检出组合套件，例如：

`openblas_dgemm,openblas_sgemm,openblas_zgemm,sleef_neon,pocketfft_fft,isal_igzip,zstd19,openssl_sha,openssl_sha3,fma*,cachebounce,lock*,atomic_simd_*,memcpy_rewr`（按本机实际用例名裁剪）；

    - 计算型与一致性型用例**混编同跑**（一致性缺陷必须在多核对打时才暴露）；
    - 每天用 `--test-list-randomize` 跑一个随机序周期，破顺序依赖；固定序周期保留可对比基线；
    - 全核 `-n 0`；ULP 数值敏感的 eigen SVD 类按文档 `-n 1` 单独补跑，避免假阳性。

5. **L4 深驻留（夜间/周末，长时段）**：`--max-test-loop-count=0` 关闭分片，固定 RNG 种子，对当周期最可疑/最高价值的 3–5 个负载（如 dgemm、sleef_neon、power-virus 类）连续 8–12h 长跑，做统计采样深度。
6. **L5 专项激发（穿插，每次 1–2h）**：

    - di/dt 阶跃：power-virus/阶跃类用例（如 `power_virus_dit`、`arm64_sdc`，BETA 级加 `--quality=0`）；
    - 热激发：先预热至目标温度带（距 Tjmax 留 10–15°C 安全裕度）再跑目标用例；逐温度带记录 SDC 频率，验证是否存在阈值/指数关系；
    - 冷机窗口：每天散热恢复后的冷态首轮跑 L1 代表集；
    - 逐核隔离：用 `--cpuset` 逐核（或逐 CCX/簇）各跑 30–60min 高检出套件，建立**逐核健康台账**；
    - 跨 NUMA/互联：`memcpy_rewr` 三策略 + 大 mdim(2048/4096，按内存预算) 压远端 NUMA。

7. **补充检测层（全程并行）**：

    - `stress-ng --verifiable` 列出可校验 stressor，选 `--verify` 类（矩阵/浮点/向量重排/merkle 校验和/搜索排序等）按小时轮转；
    - `rasdaemon` 作为服务全程运行，记录 EDAC/MCE/APEI/PCIe 错误；
    - 每 5 分钟采一次 IPMI（温度/电压/风扇/功耗）与 OS 频温数据，存为时序 CSV（命中事件时可回溯当时工况）。

### 5.2 7×24 排班（以 24h 为大周期，循环执行，周期数无上限）

- 给出一张时刻表（示例结构，具体按画像调整）：冷机首轮 → L2 扫档 → L3 多样性轮转（含一个随机序子周期）→ L4 深驻留 → L5 专项轮换；每小时自检监控链路。
- 战役总时长目标：**至少连续 7×24h（168h）**，可延长；输出"累计无 SDC 运行小时数"作为统计结论（注意：未检出 ≠ 无缺陷，需给出置信度说明与覆盖维度清单）。
- 周期之间不断电、不重置工具；每天固定时间做日志归档与台账汇总。

### 5.3 安全联锁（不可省略）

- 温度硬阈：任一核温 ≥ Tjmax−10°C（或用户指定值）→ 自动退载/暂停并告警，回落到安全区再恢复；结合 `--temperature-threshold` 与外部监控脚本双保险。
- 风扇故障/PSU 告警/SEL 新增 Critical 事件 → 立即暂停战役并告警。
- 任何死机/卡死：`--on-hang=kill` + systemd 自动拉起；单次用例配置合理 `--timeout`/`--timeout-kill`，防止永久挂死。
- 禁止为了"压出问题"而拆除温度保护或关闭节流。

## 6. Phase 4 — SDC 命中后的取证、复测与分诊流程

发现任何 mismatch / crash / RAS 事件，按序列执行（7×24 普查模式下**战役不停止**，事件进入分诊流水线）：

1. **即时取证**（自动）：保存该用例完整 YAML 日志（含 RNG 种子/`rng-state`、核位、用例参数/knob、时间戳）、错误类型与实际 vs 期望的差异字节、当时前后 5 分钟的频/温/压/功耗采样、`dmesg`/journal、`ipmitool sel list`、EDAC 计数快照。
2. **可复现性判定**：用原种子、原 cpuset、原参数立即 `--retest-on-failure` 复测 ≥3 次（冷/热两态各做），区分为：高可复现（确定性缺陷）/ 条件触发（温度/顺序/他核负载）/ 不可复现（疑似软错误或一次性事件）。
3. **逐核定位**：用 `--cpuset` 对相关核做二分/逐个重放，定位到具体逻辑核/物理核/簇/NUMA 域；再以全核同载验证"他核效应"。
4. **CPU vs 内存归因**：

    - 用缓存驻留档（mdim=64/256、nelems 小足迹）重放 → 仍错 = 核内缺陷；
    - 仅大足迹/DRAM 档错 → 结合内存基线测试与 EDAC 事件，排查内存/内存控制器；
    - 仅多核对打时错 → 缓存一致性/互联缺陷方向；
    - 记录归因依据，证据不足时标"待判定"，不得下结论。

5. **事件台账**：每个事件一个编号目录 `events/<日期>-<编号>-<用例>-<核位>/`，含证据、复测记录、定位过程、初步定性；事件同步写入汇总台账（CSV/MD 表）。
6. **回归验证**：若后续做 BIOS/微码/更换部件等处置，用原触发用例与种子做回归，结果归档。
7. crash/abort/超时同样按本流程处理（SDC 与崩溃可同源）。

## 7. Phase 5 — 7×24 工程化交付

1. **systemd 托管**：提供 unit 文件（`sdc-campaign.service`、`sdc-monitor.service`、确认 rasdaemon 服务），要求：

    - `Restart=always`、`RestartSec=10`；`StartLimitIntervalSec=0`（避免反复崩溃后不再拉起）；
    - 战役主进程用 `-T forever`；监控/采集进程独立，互不阻断；
    - 如适用启用 watchdog（`WatchdogSec=` + 进程定期 notify）；
    - 输出重定向到按天命名的日志文件或 journal（带 ident）。

2. **断点恢复**：战役驱动记录当前阶段/周期进度（状态文件），重启后从断点续跑；冷机首轮在恢复时自动补做。
3. **日志轮转与磁盘保护**：logrotate（按天 + 大小上限 + 压缩 + 保留天数），监控磁盘水位；YAML/事件证据目录单独保留、不被轮转删除。
4. **告警**：温度/风扇/电源/磁盘/战役退出/SDC 命中 → 通过本机可用渠道告警（日志醒目标记；若有 webhook/邮件由用户提供后接入，禁止擅自外发）。
5. **一键启停与状态查询**：提供 `start.sh`/`stop.sh`/`status.sh`（显示当前阶段、周期、已运行时长、事件数、最新频温压）。
6. **冒烟与正式两档**：所有脚本支持 smoke（10 分钟）与 full（7×24）两种模式，便于新单板验收。

## 8. 交付物规范（路径、命名、内容）

目录基准（若执行环境是 superpowers 工作流，沿用其约定）：

1. **方案（plan）**：`docs/superpowers/plans/YYYY-MM-DD-<主机SN>-sdc-7x24-stress-plan.md`

    - 内容：机器档案表、SDC 规律研究结论（附来源链接）、工具获取/构建记录、L0–L5 完整战役设计（含可直接复制执行的命令）、7×24 排班表、安全联锁、取证分诊流程、systemd/脚本清单、验收标准、复用说明、已知缺口。

2. **结果（output）**：`docs/superpowers/output/YYYY-MM-DD-<主机SN>-sdc-7x24-stress-plan-output.md`

    - 文件名与 plan 同名并增加 `output` 后缀；内容：实际执行的命令与时间线、各阶段结果、逐核健康台账、事件台账（每个 SDC/崩溃/RAS 事件的证据与定性）、温度-SDC 频率等实测数据、累计运行小时数与覆盖维度、结论与建议。

3. **配套产物**（随 plan 一起交付，全部可复用）：

    - `collect_inventory.sh`（画像采集，幂等）；
    - 战役驱动脚本、systemd units、启停/状态脚本、logrotate 配置；
    - `inventory/` 画像原始输出、`events/` 事件证据目录、监控时序数据；
    - 一份 **NEW_BOARD_ONBOARDING.md**：新单板接入步骤（采集画像 → 冒烟 → 自动生成/裁剪 plan → 启动），明确哪些参数自动推导、哪些需人工确认。

4. **复用性验收**：方案中禁止硬编码核数、路径、用例名假设；所有随机器变化的参数来自画像；新单板上仅靠"跑画像 + 冒烟"即可得到可执行 plan。
5. 每次正式启动前，先向用户展示 plan 摘要与安全边界并取得确认（brainstorming 审批门），再执行。

## 9. 质量红线（自检清单，交付前逐项核对）
- [ ] 裸金属、权限、BMC、磁盘、时间同步、散热基线已确认；缺口已明示。
- [ ] 软硬件画像完整：CPU 特性/全核主频/电压/温度/风扇/功耗/内存 ECC/软件栈，且 IPMI 与 OS 侧数据互证。
- [ ] 架构分支正确（aarch64→sdcshield）；工具冒烟通过。
- [ ] 战役同时覆盖计算型与一致性型（多线程）缺陷；含热激发、冷机窗口、di/dt 阶跃、工作谱系、逐核隔离、多样性轮转、深驻留。
- [ ] 7×24 工程化齐备：systemd 自动拉起、看门狗/卡死 kill、断点恢复、日志轮转、温度与硬件安全联锁。
- [ ] 命中后取证（YAML/种子/核位/频温压）→ 复测可复现性 → 逐核定位 → CPU/内存归因 → 台账，闭环可追溯。
- [ ] 所有数字与结论有命令输出或来源支撑；"未检出"附运行时长与覆盖维度，不夸大为"无 SDC"。
- [ ] 密码与凭据未出现在任何产物、日志、git 中。
## 10. 建议的启动语（在新会话中，本文件之后追加）

```
目标机就是本机，现在开始：先做 Phase 0 就绪检查与 Phase 1 全量画像（root 密码我已通过 SDC_ROOT_PW 环境变量提供），
完成后把画像摘要和初步战役设计（含预计排班与安全阈值）发给我确认，再进入构建与 7×24 压测。
战役总时长目标 168 小时起步，全程全核；任何 SDC/崩溃/RAS 事件按取证分诊流程处理，不得中断整体战役。
```

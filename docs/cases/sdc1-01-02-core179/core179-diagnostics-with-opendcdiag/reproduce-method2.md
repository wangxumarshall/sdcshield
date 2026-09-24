SDC1-01-02 故障复现与根因诊断（偏压+STL） | 项目 | 内容 | |------|------| | 案例编号 | 1003-SDC案例-【计算SDC】调压运行stl导致OS panic | | 设备型号 | Yangtze Computing R240K V2（BC82AMQA / BCB2AMQA） | | 设备SN | 50A6886522221659 | | 主机名 | STODC-P-POD13-C3K-YANGTZE-R240KV2-122-ITC42 | | BMC版本 | 见 dump_info/SpLogDump/version.json | | BIOS版本 | 7.48（06/15/2026） | | 内核版本 | 5.10.0-136.12.0.86.h1339.eulerosv2r12.aarch64（EulerOS V2R12，aarch64） | | CPU | ARM Kunpeng 920，多路（CPU0~CPU4插槽，180+逻辑核） | | 故障时间 | 2026-07-31 12:08:25（首次复位） | | 复现性 | 可复现（同日复现≥3次，相同调用栈） | | Dump来源 | R240KV2_71006932A034_20260731-1301/dump_info（13:01:56采集） |
--------------------------------------------------------------------------------
零、复现方法
步骤1：四路CPU的VDDAVS电压拉偏30mv（从0.88V到0.85V，Vmin=0.73，Vmax=0.99）
步骤1-1：ssh登录BMC控制台，执行maint_debug_cli，进入debug模式
步骤1-2：VDDAVS电压拉偏
特别注意！！！：要三个命令（连带最后一个命令后面的换行符）一起拷贝发送
CPU1：VDDAVS电压拉偏
CPU1 电压值读取 i2cwrite 7 2 0xe0 0x00 0x01 i2cwrite 7 2 0xc0 0x00 0x00 i2cread 7 2 0xc0 0x21
CPU1电压拉偏30mv i2cwrite 7 2 0xe0 0x00 0x01 i2cwrite 7 2 0xc0 0x00 0x00 i2cwrite 7 2 0xc0 0x21 0x7D 0x00
--------------------------------------------------------------------------------
CPU2：VDDAVS电压拉偏
CPU2 电压值读取 i2cwrite 7 2 0xe0 0x00 0x01 i2cwrite 7 2 0xd4 0x00 0x00 i2cread 7 2 0xd4 0x21
CPU2电压拉偏30mv i2cwrite 7 2 0xe0 0x00 0x01 i2cwrite 7 2 0xd4 0x00 0x00 i2cwrite 7 2 0xd4 0X21 0x7D 0X00
--------------------------------------------------------------------------------
CPU3：VDDAVS电压拉偏
CPU3 电压值读取 i2cwrite 7 2 0xe0 0x00 0x02 i2cwrite 7 2 0xc0 0x00 0x00 i2cread 7 2 0xc0 0x21
CPU3电压拉偏30mv i2cwrite 7 2 0xe0 0x00 0x02 i2cwrite 7 2 0xc0 0x00 0x00 i2cwrite 7 2 0xc0 0X21 0x7D 0X00
--------------------------------------------------------------------------------
CPU4：VDDAVS电压拉偏
CPU4 电压值读取 i2cwrite 7 2 0xe0 0x00 0x02 i2cwrite 7 2 0xd4 0x00 0x00 i2cread 7 2 0xd4 0x21
CPU4电压拉偏30mv i2cwrite 7 2 0xe0 0x00 0x02 i2cwrite 7 2 0xd4 0x00 0x00 i2cwrite 7 2 0xd4 0X21 0x7D 0X00
步骤2：运行海思stl工具
步骤2-1：执行./kunpeng-stl-kp920 -L 10 -r all。 或其他高负载用例
步骤2-2：收集系统日志，包括panic、电压、温度等。
登录BMC首页，点击“一键收集”下载全量日志。
--------------------------------------------------------------------------------
一、问题现象
用户于 2026-07-31 在主机 OS 上执行鲲鹏 STL 压测命令：
./kunpeng-stl-kp920 -L 9999 -r all
压测期间系统发生复位（自动重启），且复位后再次运行相同压测可稳定复现复位。Dump 采集时间为 13:01:56，距离首次复位约 53 分钟。
--------------------------------------------------------------------------------
二、关键证据链（按时间轴）
1. 调压操作（11:53 – 11:55）
operate_log 显示 Administrator（90.255.95.108）经 CLI 登录后，在 11:53:51–11:55:43 期间通过 I2C 持续写 VRD 寄存器（busid 7，addr 0xE0 / 0xC0 / 0xD4），写入值含 0x21 0x7D 0x00 等，为 CPU VRD（MP2975）电压调整操作：
2026-07-31 11:53:51 MAINT ... Write i2c busid(7) len(2) addr(0xE0) val(0x00 0x01)
2026-07-31 11:54:03 MAINT ... Write i2c busid(7) ... addr(0xC0) val(0x21 0x7D 0x00)
2026-07-31 11:54:34 MAINT ... Write i2c busid(7) ... addr(0xD4) val(0x21 0x7D 0x00)
2026-07-31 11:55:43 MAINT ... Write i2c busid(7) ... addr(0xD4) val(0x21 0x7D 0x00)
对应 VRD 芯片：2480v2_mp2975_*.psf（cpu1~cpu4 vrd0/vrd1/vrd2），见 Register/vrd_reg_info。
2. CPU2 VDDAVS 欠压告警（12:05 – 12:09）
maintenance_log 在复位前后连续上报 CPU2 VDDAVS 欠压告警（阈值 0.810 V）：
2026-07-31 12:05:11 WARN : CPU 2 VDDAVS voltage (0.810 V) is lower than the undervoltage threshold (0.810 V)
2026-07-31 12:06:02 WARN : CPU 2 VDDAVS voltage (0.840 V) is lower than the undervoltage threshold (0.810 V)
2026-07-31 12:08:54 WARN : CPU 2 VDDAVS voltage (0.810 V) is lower than the undervoltage threshold (0.810 V)
2026-07-31 12:09:24 WARN : CPU 2 VDDAVS voltage (0.840 V) is lower than the undervoltage threshold (0.810 V)
AppDump/sensor_alarm/sensor_info.txt 显示当前各 CPU VDDAVS 读数及阈值：
N_VDDAVS_CPU2  当前 0.950 V | lcr 0.810 V | ucr 1.050 V
CPU2 VDDAVS    当前 0.840 V | lcr 0.730 V | ucr 0.990 V
即：调压后 CPU2 VDDAVS 实测下探到 0.810 V，触及欠压阈值。
3. 内核数据损坏 → Oops → Panic（OS 串口 systemcom.dat）
OSDump/systemcom.tar → systemcom.dat（主机串口循环缓冲）记录到压测进程 kunpeng-stl-kp9 触发数据异常：
首次崩溃（uptime 3014s，即 12:06:57 前后）：
[3014.352762][T46002] Internal error: Oops: 0000000096000004 [#1] SMP
[3014.359073][T46002] CPU: 179 PID: 46002 Comm: kunpeng-stl-kp9 Kdump: loaded Tainted: G W O
[3014.381214][T46002] Hardware name: Yangtze Computing R240K V2/BC82AMQA, BIOS 7.48 06/15/2026
[3014.387937][T46002] pstate: 80400089 (Nzcv daIf +PAN -UAO -TCO)
[3014.393179][T46002] pc : find_busiest_group+0x1b8/0xb00      ← CFS调度器负载均衡，即内核正沿着调度域链表遍历CPU负载，但读到指针x10已经变成垃圾数据
[3014.398421][T46002] lr : find_busiest_group+0x190/0xb00
[3014.462675][T46002] x10: 0ffe809021e0b2ae                     ← 已损坏的指针
[3014.562377][T46002] Code: f94007e1 f8657a89 8b090039 9102032a (f9405155)   ← ldr x21,[x10,#0xa0]
[3014.495946][T46002] Call trace:
[3014.500837][T46002]  find_busiest_group+0x1b8/0xb00
[3014.505206][T46002]  load_balance+0x174/0x890
[3014.509835][T46002]  newidle_balance+0x198/0x3a0
[3014.514725][T46002]  pick_next_task_fair+0xe4/0x680
[3014.518919][T46002]  __schedule+0x248/0x914
[3014.522770][T46002]  schedule+0x50/0xdc
[3014.527659][T46002]  futex_wait_queue_me+0xc0/0x150          ← 用户态 futex 等待进入调度
[3014.746120][T46002] Kernel panic - not syncing: Oops: Fatal exception
[3014.828274][T46002] Starting crashdump kernel...
[3014.833337][T46002] Bye!
Oops: 0000000096000004 是ESR_EL1（异常状态寄存器），其中0x96000000 (EC = 0x25)表示EL1特权级发生Data Abort（数据访问终止），0x00000004 (DFSC = 0x04)表示 Translation fault, level 0级地址转换故障，说明MMU从x10寄存器（0ffe809021e0b2ae）里的地址，发现这个地址不在TTBR0（0x0000_XXXX_XXXX_XXXX）、TTBR1（0xFFFF_XXXX_XXXX_XXXX）映射的虚拟地址空间。
复位后再次运行压测，第二次崩溃（uptime 480s，即 12:16:25 前后）：
[480.711842][T23736] Internal error: Oops: 0000000096000004 [#1] SMP
[480.718157][T23736] CPU: 179 PID: 23736 Comm: kunpeng-stl-kp9 Kdump: loaded
[480.747012][T23736] pc : find_busiest_group+0x1b8/0xb00
[480.815742][T23736] x10: a23fa5817371856b                     ← 又一个不同的损坏指针
[480.821773][T23736] x9 : a24000ffff5cd22b
[480.855072][T23736] Call trace: find_busiest_group → load_balance → newidle_balance → ...
[481.105931][T23736] Kernel panic - not syncing: Oops: Fatal exception
第三次崩溃（uptime 722s）：
[722.807153][T41154] Internal error: Oops: 0000000096000004 [#1] SMP
[723.204028][T41154] Kernel panic - not syncing: Oops: Fatal exception
systemcom.dat 中所有同签名 Oops 汇总：
uptime
PID
进程
Oops码
PC
故障寄存器x10
344.88
1231
kworker/179:1
0x96000007
—
—
3014.35
46002
kunpeng-stl-kp9
0x9600004
find_busiest_group+0x1b8
0x0ffe809021e0b2ae
480.71
23736
kunpeng-stl-kp9
0x9600004
find_busiest_group+0x1b8
0xa23fa5817371856b
722.81
41154
—
0x9600004
—
—
14562.56
1581010
—
0x9600007
—
—
45104.88
643036
—
0x9600004
—
—
178088.58
1847506
—
0x9600004
—
—
结论： 多次崩溃调用栈一致（find_busiest_group → load_balance），故障寄存器每次都不同但均为非法内核地址，属典型 CPU 计算结果被静默损坏（SDC） 表现——非软件 bug（同一指令组合不可能产生随机损坏指针），而是硬件在欠压下产生错误数据。
4. kdump 转储过程中 OOM（12:06:58 – 12:08:25）
OSDump/img2_20260731120827.jpeg（12:08:27 截屏）显示 crashdump 内核中 makedumpfile 触发 OOM：
Fri Jul 31 12:06:57 CST 2026: kdump: saving vbox.img.gz complete
Fri Jul 31 12:06:58 CST 2026: kdump: saving vcore begin...
[100.891184][T642] makedumpfile invoked oom: gfp_mask=0x40cd0, order=0
[100.916621][T642] CPU: 0 PID: 642 Comm: makedumpfile
[100.936432][T642] Hardware name: Yangtze Computing R240K V2/BCB2AMQA, BIOS 7.48 06/15/2026
Call trace: out_of_memory ← __alloc_pages ← allocate_slab ← kmem_cache_alloc ←
            xas_nomem ← __add_to_page_cache_locked ← ext4_da_write_begin ← vfs_write
crashkernel 预留 512 MB，转储 vcore 时 ext4 写页缓存耗尽，makedumpfile OOM；kdump 流程无法正常完成。
5. 系统复位时刻（12:08:25 – 12:08:26）
linux_kernel_log（BMC内核）：
AppDump/sensor_alarm/sel_extracted/eo_sel.csv：
LogDump/fdm_output：
LogDump/app_debug_log_all.2（extracted）：
复位原因 0x2C00000F / restart cause=0（Unknown/unrecognized）：非看门狗、非手动、非电源策略，是 CPU 硬件层面的异常复位。
6. 复位后重启流程（12:09 – 12:11）
operate_log / maintenance_log 显示 12:09:03 起设置看门狗（BIOS/POST → OS Load），系统进入 BIOS POST：
重启后 CPU2 VDDAVS 欠压告警仍在（12:08:54、12:09:24），说明 欠压状态在复位后未消除（VRD 寄存器保持调压后的值）。
--------------------------------------------------------------------------------
三、根本原因（Root Cause）
直接原因
CPU2 VDDAVS（自适应电压调节核心电压）在调压后下探至 0.810 V，达到欠压阈值，触发 CPU 内核数据通路的静默数据损坏（SDC）。 损坏的指针被 CFS 调度器 find_busiest_group 解引用，引发 ARM64 Data Abort（ESR=0x9600004，Level-0 翻译错误）→ 内核 Oops → Kernel panic → kdump → 系统复位。
证据指向 SDC（而非软件缺陷）
同一指令、同一调用栈、不同损坏值：pc = find_busiest_group+0x1b8，故障指令 ldr x21,[x10,#0xa0]（编码 f9405155）固定，但 x10 每次为不同的非法地址（0x0ffe809021e0b2ae、0xa23fa5817371856b…）。软件 bug 会在固定输入下产生固定错误，随机损坏值是硬件 SDC 的典型特征。
触发源为压测进程：Comm: kunpeng-stl-kp9，与用户运行的 ./kunpeng-stl-kp920 -L 9999 -r all 一致。压测高强度运算在欠压下更易暴露 CPU 计算错误。
时序强相关：调压（11:53-11:55）→ 欠压告警（12:05-12:09）→ Oops/Panic（12:06:57）→ 复位（12:08:25），因果链清晰。
复位后欠压未消除、再次运行再次崩溃：12:08:54/12:09:24 仍有欠压告警；第二次压测（uptime 480s）同一调用栈再次 panic，可复现。
复位原因 "Unknown"：排除了看门狗/电源按钮/AC 失效等常规复位源，指向 CPU 硬件自保护复位。
触发链路
用户 I2C 写 VRD (MP2975) 调低 CPU2 VDDAVS
        │
        ▼
CPU2 VDDAVS 实测 0.810 V（触及欠压阈值）
        │
        ▼
CPU 内核数据通路在欠压下产生错误计算结果（SDC：寄存器/指针被损坏）
        │
        ▼
CFS 调度器 find_busiest_group 解引用损坏指针 x10
        │
        ▼
ARM64 Data Abort (ESR=0x9600004, L0 translation fault)
        │
        ▼
Kernel Oops → Kernel panic - not syncing: Oops: Fatal exception
        │
        ▼
kexec 进入 crashdump kernel (512M)，makedumpfile 转储 vcore 时 OOM
        │
        ▼
kdump 流程异常 + CPU 持续欠压 → 系统复位 (restart cause=0, 0x2C00000F)
        │
        ▼
BIOS POST → OS 重启 → VRD 寄存器保持调压值 → 欠压告警再现 → 再次压测再次 panic
--------------------------------------------------------------------------------
四、次要问题
kdump 转储 OOM：crashkernel=512M 在 192+ 核、大内存机器上偏小，makedumpfile 写 ext4 时页缓存耗尽导致 OOM，未能生成完整 vmcore。本次 vmcore 转储失败，仅靠串口/kbox 保留了 Oops 文本。
风扇类型识别异常：复位前后 cooling_app 持续报 fantype_identify: fan type identify state = 3 与 fan status error，但与复位无直接因果（属独立告警，需另行确认风扇模块）。
BMC Set global enables failed（12:11:16）：复位后 BMC 重新启用全局中断失败，通常为瞬态，建议观察后续是否复现。
--------------------------------------------------------------------------------
五、定级与影响
故障类别：计算 SDC（Silent Data Corruption）— 欠压导致的 CPU 数据损坏。
影响面：系统不可用（复位重启），且 kdump 无法成功转储，故障现场丢失。多次复现，持续影响业务。
SDC 危害：若损坏未触发 panic 而是被业务数据写回，将造成静默数据错误，风险高于本次复位场景。
--------------------------------------------------------------------------------
六、处置建议
紧急处置
恢复 CPU2 VDDAVS 电压至默认值：通过 BMC 维护界面或 ipmitool 将 VRD（MP2975）寄存器还原为出厂配置（CPU2 VDDAVS 标称约 0.84–0.85 V，不低于 lcr 0.730 V，但需保证不触欠压告警 0.810 V）。
停止压测：在电压恢复前不再执行 kunpeng-stl-kp920 -L 9999 -r all，避免再次 panic/复位。
下电彻底复位 VRD：若 BMC 侧 I2C 写入仍保留调压值，建议整机下电再上电，使 VRD 回到固件默认输出。
根因整改
规范调压流程：调压操作应在 OS 闲置、无压测/业务负载时进行；调压后先做轻负载稳定性验证（小范围 kunpeng-stl-kp920 -L <小值>），再逐步加压，禁止一次性 -L 9999 满载运行。
设置欠压保护基线：在 BMC 侧对 N_VDDAVS_CPUx 配置硬阈值，低于 0.810 V 时自动告警并拒绝继续下调；对带外调压接口增加二次确认与限幅。
增大 crashkernel：将 crashkernel=512M 调整为 1G（或按内存比例），并配置 makdumpfile 过滤策略（如 -d 31 或 --message-level 压缩），避免 kdump 内核 OOM 导致转储失败。
开启 RAS/CE 告警上报：确保 BMC FDM（Fault Diagnosis Management）与 OS rasdaemon 对 CPU machine check、内存 CE/UE 持续监控并上报，便于 SDC 早期发现。
排查 CPU2 硬件：若恢复默认电压后仍出现 SDC/Oops，需现场排查 CPU2 插槽接触、供电相序、主板 VRM 健康度，必要时更换主板或 CPU2（参考告警建议 1. Replace the mainboard.）。
长期改进
压测工具增加电压门限前置检查：kunpeng-stl-kp920 启动前读取 BMC 电压传感器，若 VDDAVS 接近欠压阈值则告警退出，避免在欠压工况下直接满载。
SDC 检测机制：在 OS 侧引入 ARM RAS + 内存 ECC 巡检 + 关键指针校验，对计算结果做交叉校验（如 STL 运算结果自检），将"静默"损坏显性化。
--------------------------------------------------------------------------------
七、附录：关键日志路径
证据
路径
调压 I2C 操作
dump_info/LogDump/operate_log（2026-07-31 11:53–11:55）
CPU2 VDDAVS 欠压告警
dump_info/LogDump/maintenance_log（2026-07-31 12:05–12:09）
电压传感器读数/阈值
dump_info/AppDump/sensor_alarm/sensor_info.txt（N_VDDAVS_CPU2 / CPU2 VDDAVS）
VRD 寄存器历史
dump_info/Register/vrd_reg_info（2480v2_mp2975_*）
内核 Oops/Panic 现场串口
dump_info/OSDump/systemcom.tar → systemcom.dat（uptime 3014/480/722s）
复位时刻截图（makedumpfile OOM）
dump_info/OSDump/img2_20260731120827.jpeg
SEL 复位事件 0x2C00000F
dump_info/AppDump/sensor_alarm/sel.tar → eo_sel.csv ID=2673
FDM 复位记录
dump_info/LogDump/fdm_output（System Restart [Unknown][IPMB]）
BMC 内核 subsys_rst_irq
dump_info/LogDump/linux_kernel_log（12:08:25 int_sts 0x467f）
复位前后应用日志
dump_info/LogDump/extracted/app_debug_log_all.2（12:08:25 restart cause=0）
kbox 黑盒
dump_info/LogDump/kbox_info / kbox_info.1
CPLD/复位寄存器
dump_info/Register/cpld_reg_info
--------------------------------------------------------------------------------
八、诊断结论 & 详细源码级定位分析
本次 2026-07-31 12:08:25 系统复位为计算 SDC 类故障： 用户对 CPU2 VRD（MP2975）执行带外调压后，CPU2 VDDAVS 核心电压下探至 0.810 V（欠压阈值），在运行 ./kunpeng-stl-kp920 -L 9999 -r all 满载压测时，欠压导致 CPU 数据通路产生静默损坏（损坏指针被 find_busiest_group 解引用），触发 ARM64 Data Abort（ESR 0x9600004）→ Kernel panic → kdump 转储 OOM → 系统复位（原因码 0x2C00000F / Unknown）。复位后 VRD 调压值未还原，欠压告警持续，再次运行压测可稳定复现同一调用栈 panic。根因为调压操作导致 CPU 欠压引发 SDC，非软件缺陷。
1. 完整寄存器转储对比（4次 find_busiest_group 崩溃）
寄存器
Crash#1 (3014s)
Crash#2 (480s)
Crash#3 (722s)
Crash#4 (14562s)
x1
ffffac7020e0b2c0
ffffa4817414b2c0
ffffd20f652ab2c0
ffffbfbd1957b2c0
x5
0000000000000090 (CPU144)
00000000000000b0 (CPU176)
0000000000000047 (CPU71)
0000000000000015 (CPU21)
x9
0ffed42000ffff6e
a24000ffff5cd22b
00ffff74043a96e0
0000000000000000
x10
0ffe809021e0b2ae
a23fa5817371856b
00ffd18369654a20
ffffbfbd1957b340
x19
ffffac7020e0b2c0
ffffa4817414b2c0
ffffd20f652ab2c0
ffffbfbd1957b2c0
x20
ffffac702114a290
ffffa4817448a290
ffffd20f655ea290
ffffbfbd198ba290
x25
0ffe809021e0b22e
a23fa581737184eb
00ffd183696549a0
ffffbfbd1957b2c0
ESR
0x96000004
0x96000004
0x96000004
0x96000007
2. 故障指令序列逐条解码
Code: f94007e1 f8657a89 8b090039 9102032a (f9405155)
序号
机器码
ARM64汇编
操作
1
f94007e1
ldr x1, [sp, #8]
从栈帧加载 &runqueues（per-CPU runqueue变量基址）
2
f8657a89
ldr x9, [x20, w5, sxtw #3]
从 __per_cpu_offset[] 数组加载偏移量，w5=CPU号，sxtw#3=符号扩展×8
3
8b090039
add x25, x1, x9
x25 = &runqueues + __per_cpu_offset[cpu] = cpu_rq(cpu)
4
9102032a
add x10, x25, #0x80
x10 = cpu_rq(cpu) + 0x80（struct rq内部偏移，指向cfs_rq子结构）
5
(f9405155)
ldr x21, [x10, #0xa0]
← 崩溃点：读取 rq->cfs_rq 内偏移0xa0的字段
3. 算术链验证 — x10的精确来源
以Crash#1为例，验证x10的计算链：
x1  = ffffac7020e0b2c0    ← &runqueues（per-CPU runqueue基址，有效内核地址 0xffff...）
x9  = 0ffed42000ffff6e    ← __per_cpu_offset[144]（已损坏！正常应为 0x0000_0000_XXXX_XXXX）
x25 = x1 + x9
    = ffffac7020e0b2c0 + 0ffed42000ffff6e
    = 0ffe809021e0b22e    ← 溢出截断后得到（验证通过 ✓）
x10 = x25 + 0x80
    = 0ffe809021e0b22e + 80
    = 0ffe809021e0b2ae    ← 崩溃时的x10值（验证通过 ✓）
关键发现：x1 (&runqueues) 始终是合法内核地址（0xffff_...），而x9 (__per_cpu_offset[cpu]) 每次都是不同的垃圾值 — 这就是损坏源。
Crash#2/Crash#3 同样验证通过：
Crash#2: ffffa4817414b2c0 + a24000ffff5cd22b = a23fa581737184eb → +0x80 = a23fa5817371856b ✓
Crash#3: ffffd20f652ab2c0 + 00ffff74043a96e0 = 00ffd183696549a0 → +0x80 = 00ffd18369654a20 ✓
4. 映射到Linux 5.10内核源码
4.1 调用链与内联展开
find_busiest_group()                         ← pc + 0x1b8, 函数大小0xb00
  └→ update_sd_lb_stats()  [被内联]
       └→ update_sg_lb_stats()  [被内联]
            └→ for_each_cpu_and(i, group->cpumask, env->cpus)
                 └→ struct rq *rq = cpu_rq(i);    ← ★ 损坏的计算点
                      = &per_cpu(runqueues, i)
                      = (struct rq *)(&runqueues + __per_cpu_offset[i])
                                                    ^^^^^^^^^^^^^^^^^^^^
                                                    x9 — 此处被SDC损坏
4.2 源码级对应（kernel/sched/fair.c + kernel/sched/sched.h）
cpu_rq(i) 宏展开（kernel/sched/sched.h）：
// kernel/sched/sched.h
DECLARE_PER_CPU(struct rq, runqueues);
#define cpu_rq(cpu)  (&per_cpu(runqueues, (cpu)))

// 展开为：
#define per_cpu_ptr(ptr, cpu)  SHIFT_PERCPU_PTR((ptr), per_cpu_offset(cpu))
#define per_cpu_offset(x)      (__per_cpu_offset[x])
#define SHIFT_PERCPU_PTR(p, off)  ((typeof(p))((unsigned long)(p) + (off)))

// 最终等价于：
cpu_rq(cpu) = (struct rq *)((unsigned long)&runqueues + __per_cpu_offset[cpu])
编译器生成的ARM64指令对应关系：
C源码
ARM64指令
寄存器映射
&runqueues（存入x19并压栈）
ldr x1, [sp, #8]
x1 = &runqueues = x19
__per_cpu_offset[cpu]
ldr x9, [x20, w5, sxtw #3]
x20 = __per_cpu_offset数组基址, w5 = CPU号
&runqueues + __per_cpu_offset[cpu]
add x25, x1, x9
x25 = cpu_rq(cpu)
&rq->cfs_rq（struct rq偏移0x80）
add x10, x25, #0x80
x10 = &rq->cfs_rq
读取cfs_rq内部字段
ldr x21, [x10, #0xa0]
← 崩溃
4.3 关键数据结构偏移
struct rq {                            ← x25 指向此结构
    raw_spinlock_t  lock;              // +0x00
    unsigned int    nr_running;        // +0x04
    ...
    u64             clock;             // +0x10
    u64             clock_task;        // +0x18
    ...
    struct cfs_rq   cfs;              // +0x80  ← x10 = x25 + 0x80 指向这里
    ...
};

struct cfs_rq {                        ← x10 指向此结构
    struct load_weight  load;          // +0x00
    unsigned int        nr_running;    // +0x08
    u64                 exec_clock;    // +0x10
    u64                 min_vruntime;  // +0x18
    struct rb_root_cached tasks_timeline; // +0x20
    ...
    unsigned int        h_nr_running;  // ~+0x40
    ...
    struct sched_avg    avg;           // ~+0x60-0x70 (因配置而异)
    ...
};                                     // +0xa0 ← ldr x21, [x10, #0xa0] 访问的位置
偏移 +0xa0 在 cfs_rq 内对应的字段，根据 update_sg_lb_stats() 中的访问模式，最可能是 cfs_rq.avg 中的 PELT（Per-Entity Load Tracking）负载追踪字段（如 load_avg / runnable_load_avg / util_avg），用于计算调度组的负载统计。
4.4 寄存器-变量映射汇总
寄存器
值(Crash#1)
含义
状态
x1 / x19
ffffac7020e0b2c0
&runqueues（per-CPU runqueue全局基址）
正常（0xffff开头，有效内核地址）
x20
ffffac702114a290
__per_cpu_offset[] 数组基址
正常（0xffff开头，有效内核地址）
w5 / x24
0x00000090 (144)
当前遍历的CPU编号
正常（0-179范围内）
x9
0ffed42000ffff6e
__per_cpu_offset[144]
★ 损坏（正常应为 0x0000_0000_XXXX_XXXX 量级的小正整数）
x25
0ffe809021e0b22e
cpu_rq(144) = &runqueues + 坏x9
派生损坏
x10
0ffe809021e0b2ae
&cpu_rq(144)->cfs (偏移+0x80)
派生损坏 → 崩溃
5. SDC损坏机理：为什么是x9？
5.1 三次崩溃x9对比
崩溃
x9（__per_cpu_offset[cpu]）
遍历CPU
特征
#1
0ffed42000ffff6e
144
高位0ffe（应为0x0000），含ffff模式
#2
a24000ffff5cd22b
176
高位a240（应为0x0000），含ffff模式
#3
00ffff74043a96e0
71
高位00ff（应为0x0000），含ffff模式
正常值应为：0x0000_0000_00XX_XXXX（每CPU per-cpu区域偏移量，典型值在几十KB到几MB范围，远小于4GB）。
三次崩溃中x9的高32位本应为0，但实际为 0ffe/a240/00ff — 每次不同，是典型的随机SDC损坏。
5.2 损坏发生位置判定
x9 = __per_cpu_offset[cpu] 的损坏可能发生在以下三个位置之一：
可能性1：内存中的 __per_cpu_offset[] 数组条目已被先前SDC写入损坏
         → 后续所有读操作都会读到坏数据

可能性2：ldr x9, [x20, w5, sxtw #3] 指令的地址计算被SDC损坏
         → CPU从错误地址加载了数据

可能性3：ldr x9 指令的数据通路被SDC损坏
         → 从内存/缓存读出的正确数据，到写入x9寄存器之间被篡改
无法仅从Oops区分以上三种（需要vmcore对比内存实际值），但根因相同：CPU2 VDDAVS欠压导致计算/访存通路SDC。
5.3 Crash#4的特殊性
Crash#4（14562s）的ESR为 0x96000007（Permission fault, level 3），而非前三次的 0x96000004（Translation fault, level 0）。但x9 = 0、x10 = ffffbfbd1957b340、x25 = ffffbfbd1957b2c0，x1 = x19 = x25，这些值看起来正常。
x25 = ffffbfbd1957b2c0    ← 看起来像有效的cpu_rq(cpu)指针
x10 = ffffbfbd1957b340    ← x25 + 0x80
但访问 [x10 + 0xa0] = ffffbfbd1957b3e0 时触发 Permission fault (level 3)，说明该地址的页表条目权限位不正确。这可能是因为：
__per_cpu_offset[0x15] (CPU21) 的值使x25指向了一个有效映射但无读权限的页面
或SDC损坏了页表项的权限位
6. 源码定位结论
0ffe809021e0b2ae 的精确来源链路：
1. kernel/sched/fair.c, find_busiest_group() 函数
   └→ 内联的 update_sg_lb_stats() 中 for_each_cpu_and() 循环

2. 循环体: struct rq *rq = cpu_rq(i);
   └→ 宏展开: (struct rq *)((unsigned long)&runqueues + __per_cpu_offset[i])

3. ARM64指令序列:
   ldr x1, [sp, #8]              → x1 = &runqueues = 0xffffac7020e0b2c0  [正常]
   ldr x9, [x20, w5, sxtw #3]    → x9 = __per_cpu_offset[144] = 0x0ffed42000ffff6e  [★ SDC损坏]
   add x25, x1, x9               → x25 = cpu_rq(144) = 0x0ffe809021e0b22e  [派生损坏]
   add x10, x25, #0x80           → x10 = &rq->cfs = 0x0ffe809021e0b2ae  [派生损坏]

4. 崩溃: ldr x21, [x10, #0xa0]
   → MMU查表发现 0x0ffe809021e0b2ae 不在 TTBR0/TTBR1 映射范围
   → Translation fault, level 0 (ESR.DFSC=0x04)
   → Data Abort → Kernel Oops → Panic
根因：CPU2 VDDAVS 欠压（0.810V）导致 CPU 数据通路 SDC，ldr x9, [x20, w5, sxtw #3] 指令从 __per_cpu_offset[] 数组加载的 per-CPU 偏移量被静默损坏为垃圾值 0ffed42000ffff6e，后续 add 指令传播此损坏，最终产生非法指针 0ffe809021e0b2ae，被 ldr x21, [x10, #0xa0] 解引用时触发 ARM64 Data Abort。

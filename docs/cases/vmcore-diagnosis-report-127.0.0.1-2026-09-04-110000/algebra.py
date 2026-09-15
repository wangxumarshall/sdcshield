#!/usr/bin/env python3
# 案例代数复算脚本：127.0.0.1-2026-09-04-11:00:00 (CPU179 / sftp-server / find_busiest_group)
# 所有运算按 mod 2^64（AArch64 寄存器宽度），每一步都标注数据来源（dmesg 行号或 crash 实测）
M = 1 << 64

def h(x):
    return f"0x{x:016x}"

print("=" * 78)
print("P1  崩溃点指令语义与寄存器现场闭合")
print("=" * 78)

# ---- dmesg 2580-2629 行实测寄存器现场 ----
x27 = 0xffffd77069c696c0   # dmesg x27
x23 = 0x0000000000000564   # dmesg x23
x20 = 0x0000000000000000   # dmesg x20
x1  = 0xffffd77069c696c0   # dmesg x1
x25 = 0x0000000000000061   # dmesg x25 (=97)
x22 = 0xffff604003ed3a80   # dmesg x22 (sched_group*)
x26 = 0xffff604003ed3900   # dmesg x26 (sched_group*)
FAR = 0xffffd77069c697e0   # dmesg 2580 行

print(f"\n[1] 致命指令编码解码 (dmesg 2629 行 Code 窗口末帧 f9409377):")
enc = 0xf9409377
imm12 = (enc >> 10) & 0xFFF
rn = (enc >> 5) & 0x1F
rt = enc & 0x1F
print(f"    f9409377 → LDR x{rt}, [x{rn}, #{imm12*8}]  (imm12={imm12:#x}, 64bit scale=8)")
print(f"    即: ldr x23, [x27, #0x120]  ← pc = find_busiest_group+0x140")

ea = (x27 + 0x120) % M
print(f"\n[2] EA = x27 + 0x120 = {h(x27)} + 0x120 = {h(ea)}")
print(f"    dmesg FAR (2580行)          = {h(FAR)}")
print(f"    EA == FAR ? {'是【实锤】' if ea == FAR else '否'}")

print(f"\n[3] x27 从哪里来: crash dis 实测 +316 帧为 add x27, x1, x20")
print(f"    x1  (dmesg) = {h(x1)}")
print(f"    x20 (dmesg) = {h(x20)}  ← 0 !")
x27_recalc = (x1 + x20) % M
print(f"    x1 + x20 = {h(x27_recalc)} 与 dmesg x27 = {h(x27)} 一致? {x27_recalc == x27}")
print(f"    → x27 是纯 ALU 结果, 输入为 x1 与 x20, 计算本身无误 (0+x1=x1)")

print(f"\n[4] x20 从哪里来: crash dis 实测 +308 帧为 ldr x20, [x0, w25, sxtw #3]")
enc2 = 0xf879d814
print(f"    f879d814 解码: LDR (register), Rm=x{(enc2>>16)&0x1F}, Rn=x{(enc2>>5)&0x1F}, Rt=x{enc2&0x1F}")
print(f"    即: ldr x20, [x0, x25, sxtw #3]  (有符号扩展字索引, scale 8)")
ea20 = (0xffffd7706a0655d0 + (x25 << 3)) % M
print(f"    x0 此时应为 &__per_cpu_offset[0] = 0xffffd7706a0655d0 (见 P3 栈取证)")
print(f"    EA20 = 0xffffd7706a0655d0 + 97*8 = {h(ea20)}")

print("\n" + "=" * 78)
print("P2  crash 实测内存真值 vs 寄存器值 【决定性对照】")
print("=" * 78)

# ---- crash 实测 (out13/out17) ----
pco97_mem = 0xffffa89017090000   # crash: rd 0xffffd7706a0658d8 → ffffa89017090000
print(f"\n[5] __per_cpu_offset[97] 内存真值 (crash rd 实测) = {h(pco97_mem)}")
print(f"    x20 寄存器装载结果 (dmesg)     = {h(x20)}")
print(f"    真值 != 寄存器值 → 装载结果塌缩为 0 【实锤】")
print(f"    (数组 192 项全非零、单调递增, 无一项为 0 —— crash rd 0xffffd7706a0655d0 192 实测)")

# ---- crash 实测 runqueues per-cpu ----
rq179 = 0xffff8000817dd6c0
rq97  = 0xffff800080cf96c0
print(f"\n[6] 交叉验证: &runqueues + __per_cpu_offset[179] :")
r = (x1 + 0xffffa89017b74000) % M
print(f"    {h(x1)} + 0xffffa89017b74000 = {h(r)}")
print(f"    crash px runqueues[179] = {h(rq179)}  一致? {r == rq179} ✓")

print(f"\n[7] 反事实推演 (若 x20 装载正确 = {h(pco97_mem)}):")
x27_cf = (x1 + pco97_mem) % M
print(f"    x27 = x1 + x20 = {h(x27_cf)} = per_cpu(runqueues, 97)")
print(f"    crash px runqueues[97] = {h(rq97)}  一致? {x27_cf == rq97} ✓")
print(f"    ldr x23, [x27, #0x120] → 访问 {h((x27_cf+0x120)%M)} = rq(97).cfs.avg.load_avg")
print(f"    crash rd 实测该地址值 = 0x414 (1044) 非零, 页有效 → 不会缺页, 内核继续正常运行")

print(f"\n[8] 实际发生 (x20=0):")
print(f"    x27 = {h(x1)} = &runqueues 链接地址本身 (.data..percpu 模板段)")
print(f"    crash vtop ffffd77069c697e0: PGD=10006057fffff403 PUD=10006057ffffe403 PMD=0")
print(f"    dmesg 2608 行独立同证: pmd=0000000000000000 → FSC=0x06 level 2 translation fault")
print(f"    → 模板段该 2MB 区间本就无映射(运行时 percpu 实例在动态区), 缺页是必然结果")

print("\n" + "=" * 78)
print("P3  栈取证: ldp 的源值完好")
print("=" * 78)

print(f"\n[9] crash rd ffff8000e6f6b740 32 实测 (崩溃线程内核栈):")
print(f"    sp+8  (ffff8000e6f6b748) = 0xffffd7706a0655d0 = &__per_cpu_offset[0] ✓")
print(f"    sp+16 (ffff8000e6f6b750) = 0xffffd77069c696c0 = &runqueues ✓")
print(f"    → +300 帧 ldp x0,x1,[sp,#8] 的两个源(栈内存)完好无损")
print(f"    → 排除'栈被写坏导致基址错'的软件解释")

print("\n" + "=" * 78)
print("P4  调度域结构完好性 (排除共享结构被写坏)")
print("=" * 78)

print(f"\n[10] crash 实测:")
print(f"     rq(179).sd = 0xffff604004407000, name=\"MC\", level=1, flags=4631, span_weight=24")
print(f"     sd->parent = 0xffff60400440de00, name=\"NUMA\", level=4, flags=25623, span_weight=48")
print(f"     MC groups: 0x...ec60c0→ec61e0→ec6cc0→ec67e0(环) 全部 next 指针有效, ref=24")
print(f"     NUMA groups: 0x...ed3600(weight24) / 0x...ed3a80,ed3060,ed3900,ed3360 (weight120, 4组环闭合)")
print(f"     x22=0xffff604003ed3a80 与 x26=0xffff604003ed3900 均在此 4 组环上, 内存真值完好")
print(f"     → 全部调度域/组结构完好, 故障不在共享数据")

print("\n" + "=" * 78)
print("P5  时间线复算")
print("=" * 78)

import datetime
audit_epoch = 1788489321.820    # dmesg 2484 行 audit(1788489321.820:3)
audit_uptime = 21.844830       # 同行时间戳
boot_wall = audit_epoch - audit_uptime
bt_ = datetime.datetime.fromtimestamp(boot_wall)
print(f"\n[11] 开机墙钟锚点: audit(1788489321.820)@uptime 21.84s")
print(f"     boot = {boot_wall:.1f} = {bt_}")
panic_uptime = 1456.227941     # dmesg 2580 行
panic_wall = boot_wall + panic_uptime
pt_ = datetime.datetime.fromtimestamp(panic_wall)
print(f"[12] panic @ uptime {panic_uptime}s → 墙钟 {pt_} (与目录名 11:00:00 一致, kdump 立即启动)")
print(f"[13] 崩溃前日志静默: t=91.30s(block dm-2) → t=1456.22s(panic), 静默 {1456.227941-91.300631:.2f}s ≈ 22.8 分钟")
print(f"     静默期内 0 条 WARNING/BUG/hardware error —— 无任何前兆")

print("\n" + "=" * 78)
print("P6  最终闭合等式")
print("=" * 78)

print(f"""
  FAR == x27 + 0x120                    【实锤】 dmesg 2580/2629 vs 寄存器
      x27 == x1 + x20  (add x27,x1,x20) 【实锤】 crash dis +316
      x1  == &runqueues (栈 sp+16 实测)  【实锤】 crash rd 栈
      x20 == 0  (dmesg 寄存器)          【实锤】
      mem[__per_cpu_offset+97*8] == {h(pco97_mem)}  【实锤】 crash rd
      → x20 装载塌缩: 源非零, 寄存器为零  【实锤】

  微架构定位: ldr x20 的 load 数据通路 (L1D→寄存器文件) 单次受扰,
              64bit 装载结果被替换为全零。ALU(add)无误、栈无误、
              页表无误(PMD=0 是模板段固有状态)、调度结构无误。
  排除项: ①软件bug(该路径每秒执行千万次) ②__per_cpu_offset[97]真为0
          (实测非零且 CPU97 在线) ③栈被扰(实测完好) ④共享结构被写坏(实测完好)
  置信: 【实锤】级(内存真值 vs 寄存器直接对照) + 微架构归因【强推】
        (受扰环节精确到 load 通路, 但无法区分 L1D阵列/总线/寄存器写入,
         需芯片级 RAS/在线测试定位 —— 见报告第 9 节)
""")

print("=" * 78)
print("P7  业务/负载上下文 (crash runq + ps 实测)")
print("=" * 78)
print("""
[14] crash runq 179 实测: 191 个 CPU 的 RUNQUEUE CURRENT 全部是
     "neon_rot_ldr_at" (PID 58051+, 每核一个, 父进程 opendcdiag PID 57625
     ← bash(57622) ← systemd/init)。仅 CPU 179 例外 (CURRENT = sftp-server,
     即崩溃任务本身, 处于 TASK_WAKING/PANIC 状态)。
     opendcdiag 是 openEuler 平台内存在线诊断工具, 其派生的每核
     neon_rot_ldr_at = "NEON 旋转 load 地址" 压测进程 —— 在做负载通路
     (load path) 的满载压测。系统 192 核被压满, LOAD AVERAGE 15.67。
     
[15] 时空关联: 压测满载运行约 24 分钟 (uptime 1456s) 后, CPU179 上
     sftp-server 的 write() → schedule() → newidle_balance → 
     find_busiest_group 中一次普通的 percpu 基址装载塌缩为 0。
     两者是否同一物理受扰源无法从软件侧证实 —— 记为【假设】,
     验证途径见报告第 9 节 (opendcdiag 侧的压测结果文件与其内存布局交叉比对)。
""")

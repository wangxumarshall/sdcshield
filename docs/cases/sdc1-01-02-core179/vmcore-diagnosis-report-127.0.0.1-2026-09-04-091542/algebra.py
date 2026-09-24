#!/usr/bin/env python3
# 案例代数复算脚本 (模 2^64) — 127.0.0.1-2026-09-04-09:15:42
# 铁律: 所有 64 位运算用本脚本计算, 输出存 algebra_out.txt
M = 1 << 64

def h(x): return hex(x)
def add(a, b): return (a + b) % M
def sub(a, b): return (a - b) % M

print("== A. 致命故障地址 vs 寄存器关系 ==")
FAR  = 0x2cd7ddf3a9089790   # dmesg: Unable to handle kernel paging request at ...
x27  = 0x2cd7ddf3a9089670   # dmesg 寄存器行 x27
x20  = 0x2cd80e2000ffffb0   # dmesg 寄存器行 x20
print("FAR            =", h(FAR))
print("x27            =", h(x27))
print("FAR - x27      =", h(sub(FAR, x27)))
print("x27 - FAR      =", h(sub(x27, FAR)))
print("FAR ^ x27      =", h(FAR ^ x27))
print("x20            =", h(x20))
print("FAR - x20      =", h(sub(FAR, x20)))

print()
print("== B. 前兆1 (uptime 2582.85, CPU179 rcu_sched) 载入地址 ==")
# _find_next_and_bit+0x18 附近 ldr xN,[x27,#imm]
# x27 = 0x2cd7ddf3a9089670 (前兆1 寄存器), FAR1 = 0xffff604003ed3d58
FAR1 = 0xffff604003ed3d58
x27_1 = 0x2cd7ddf3a9089670
print("前兆1 FAR1     =", h(FAR1))
print("前兆1 x27      =", h(x27_1))
print("FAR1 - x27_1   =", h(sub(FAR1, x27_1)))
print("注意: 前兆1 x27 与致命 x27 低48位模式对照")

print()
print("== C. 前兆2 (uptime 13867.75, CPU179 ps) 载入地址 ==")
FAR2 = 0xffffcfd3a750a057
print("前兆2 FAR2     =", h(FAR2))

print()
print("== D. 前兆间时间间隔与到 panic 间隔 ==")
t_p1   = 2582.852896
t_p2   = 13867.756739
t_panc = 52269.758693
print("前兆2 - 前兆1 (s)  =", t_p2 - t_p1)
print("panic - 前兆1 (s)  =", t_panc - t_p1)
print("panic - 前兆2 (s)  =", t_panc - t_p2)
print("panic - 前兆1 (h)  =", (t_panc - t_p1)/3600.0)
print("panic - 前兆2 (h)  =", (t_panc - t_p2)/3600.0)

print()
print("== E. 墙上时间反推 (目录名 2026-09-04-09:15:42, panic uptime 52269.758693s) ==")
import datetime
wall_panic = datetime.datetime(2026, 9, 4, 9, 15, 42)
boot_wall = wall_panic - datetime.timedelta(seconds=t_panc)
print("panic 墙钟(目录名):", wall_panic)
print("开机墙钟(反推)    :", boot_wall)
print("前兆1 墙钟        :", boot_wall + datetime.timedelta(seconds=t_p1))
print("前兆2 墙钟        :", boot_wall + datetime.timedelta(seconds=t_p2))

print()
print("== F. crash sys: UPTIME 14:31:10 与 52269.76s 一致性 ==")
print("14*3600+31*60+10 =", 14*3600+31*60+10, "s vs dmesg panic ts 52269.758693 s")

# ================= 补充: 会话取证后扩展的闭合计算 =================
M = 1 << 64

print()
print("== K. KASLR 滑移量 ==")
runtime_fbg = 0xffffcfd3a665ae44
static_fbg  = 0xffff80008013ad08
print("bt: find_busiest_group 运行时地址 =", hex(runtime_fbg))
print("nm: find_busiest_group 静态地址    =", hex(static_fbg))
slide = (runtime_fbg - static_fbg) % M
print("KASLR slide =", hex(slide))
pc_static = (static_fbg + 0x140) % M
print("致命 pc 静态地址 (find_busiest_group+0x140) =", hex(pc_static))

print()
print("== L. _find_next_and_bit+0x18 / seq_put_hex_ll+0xb8 静态反汇编定位 ==")
print("_find_next_and_bit 静态 = ffff8000807459d0; +0x18 = ldr x3,[x0,x4,lsl#3]")
print("seq_put_hex_ll 静态    = ffff80008051b768; +0xb8 = ldrb w5,[x6,x3]")

print()
print("== M. 前兆2 hex_asc 闭合 (实锤) ==")
hexasc  = 0xffffcfd3a750a048   # crash p &hex_asc 实测
FAR2    = 0xffffcfd3a750a057   # dmesg 前兆2
print("hex_asc[] 基址 (crash 实测) =", hex(hexasc))
print("FAR2 (dmesg)               =", hex(FAR2))
print("FAR2 - hex_asc =", FAR2 - hexasc, "=> FAR2 = hex_asc[15] ('f')")
print("闭合:", FAR2 == hexasc + 15)

print()
print("== N. 前兆1 x0 身份闭合 (实锤) ==")
print("x0 = ffff0020250d1500; crash p task->pid = 16, ->comm = 'rcu_sched', cpu=179")
print("应然 x0 = sched_group 位图 (group=ffff604003ed3540, cpumask=ffff604003ed3578)")
print("=> x0 装载结果 = 无关 task 指针, 参数装载通路受损")

print()
print("== O. percpu 与 runqueues 闭合 ==")
pco = 18446656305406230528  # crash p __per_cpu_offset[179] 十进制原值
print("__per_cpu_offset[179] =", hex(pco))
print("runqueues 符号 (crash px &runqueues) = ffffcfd3a80896c0")
print("CPU179 rq 实例 = pco + (runqueues - __per_cpu_start) ... crash runq 显示 per-rq 基址 ffff8000817dd6c0")

print()
print("== P. 毒化值 x20 与合法 percpu 指针的海明距离 ==")
x20_bad = 0x2cd80e2000ffffb0
percpu = 0xffffb02cd9754000
d = bin(x20_bad ^ percpu).count('1')
print("x20 =", hex(x20_bad))
print("合法 percpu 指针样例 =", hex(percpu))
print("海明距离 =", d, "位")

print()
print("== Q. 前兆与致命事件的间隔(小时) ==")
print("前兆1 -> 前兆2: ", 11284.903843/3600, "h")
print("前兆2 -> 致命:   ", 38402.001954/3600, "h")
print("前兆1 -> 致命:   ", 49686.905797/3600, "h")

# -*- coding: utf-8 -*-
# 代数复算脚本：所有 64 位运算 mod 2^64
# 案例：127.0.0.1-2026-08-14-19:07:04  (CPU179 find_busiest_group+0x140 崩溃)
M = (1 << 64) - 1

def h(x): return hex(x & M)

print("== [1] 崩溃闭合等式：FAR 与寄存器 ==")
far   = 0x0036bc836a4a97df
x27   = 0xd936bc836a4a96bf
x1    = 0xffffa6c96a4996c0
x20   = 0xd93715ba0000ffff
x27_x20_sum = (x1 + x20) & M
print("FAR                              =", h(far))
print("x27 (add 结果, Oops 报告值)      =", h(x27))
print("x1 (pervcpu基址, add 输入A)      =", h(x1))
print("x20 (node_data[22] 真值, add输入B)=", h(x20))
print("x1 + x20 (mod 2^64)              =", h(x27_x20_sum))
print("FAR == x1 + x20 ?", far == x27_x20_sum)
print("FAR == x27 ?", far == x27)
print("FAR - x27 (mod 2^64)             =", h((far - x27) & M))
print("FAR - x1  (mod 2^64)             =", h((far - x1) & M))
print("x20 - (FAR - x1) (mod 2^64)      =", h((x20 - ((far - x1) & M)) & M))
print("x20 XOR (FAR - x1)               =", h(x20 ^ ((far - x1) & M)))

print()
print("== [2] x20 真值 vs 崩溃值（node_data[22]） ==")
# crash rd ffffa6c96a8955d0 32 实测：node_data 数组每项 8 字节，基址 0xffffa6c96a894dd0
node_data_base = 0xffffa6c96a894dd0
slot22 = (node_data_base + 22*8) & M
print("node_data 基址                   =", h(node_data_base))
print("node_data[22] 槽位地址           =", h(slot22))
print("x20 崩溃值                       =", h(x20))
print("x20 == 0xd93715ba0000ffff (高32位=0xd93715ba 低32位=0x0000ffff)")
print("真值低32位模式: 0x0000ffff 出现在多个 pg_data_t 地址? 见 crash rd 输出分析")
# rd 输出中相邻指针差
p_a = 0xffffd93715b7e000
p_b = 0xffffd93715ba0000
print("rd 相邻两指针差(应为 NODE_DATA 步长) =", h((p_b - p_a) & M))
p_c = 0xffffd93715bc2000
print("第三指针与第二指针差              =", h((p_c - p_b) & M))

print()
print("== [3] 前兆假 fault 地址 vs 时间线 ==")
precursors = [
    (104485.842925, 0xffff6040060997d6, "pmdalinux", "show_interrupts"),
    (104976.079640, 0xffff6040080fc818, "irqbalance", "show_interrupts"),
    (106219.051773, 0xffff6040eb922450, "irqbalance", "show_interrupts"),
    (111245.743067, 0xffff60408e264558, "pmdalinux", "show_interrupts"),
    (111299.044942, 0xffff60408e265768, "irqbalance", "show_interrupts"),
    (111735.723142, 0xffff60408e26761e, "pmdalinux", "show_interrupts"),
    (111849.058107, 0xffff60408e2673e2, "irqbalance", "show_interrupts"),
    (113245.726167, 0xffff6040ffbc5608, "pmdalinux", "show_interrupts"),
    (113435.800238, 0xffff604017fdba1c, "pmdalinux", "show_interrupts"),
    (113745.790031, 0xffff60408e2673a0, "pmdalinux", "show_interrupts"),
    (113810.970846, 0xffff604003e52218, "memcpy1", "_find_next_and_bit/load_balance"),
    (113979.669798, 0xffff604003e547b8, "control", "_find_next_and_bit/select_task_rq_fair"),
]
panic_t = 113997.282188
for t, a, c, path in precursors:
    print(f"  t={t:>14.6f}  addr={h(a)}  comm={c:<10} path={path}  距panic={panic_t-t:10.3f}s")
print(f"  panic t={panic_t} (CPU179 kworker/179:1H find_busiest_group)")

print()
print("== [4] 同地址复发：ffff60408e26xxxx 簇 ==")
cluster = [0xffff60408e264558, 0xffff60408e265768, 0xffff60408e26761e, 0xffff60408e2673e2, 0xffff60408e2673a0]
base = 0xffff60408e260000
for a in cluster:
    print(f"  {h(a)}  页内偏移 = {h(a - base)}")

print()
print("== [5] 墙上时间推算（目录名 19:07:04） ==")
# 目录时间戳 2026-08-14 19:07:04 为 kdump 完成时刻附近；panic 在 uptime 113997.28s
# crash sys 显示 dump 时间 Fri Aug 14 19:06:15 CST 2026
import datetime
panic_wall = datetime.datetime(2026, 8, 14, 19, 6, 15)
boot_wall = panic_wall - datetime.timedelta(seconds=113997.282188 + 0.0)
print("crash sys DUMP DATE = 2026-08-14 19:06:15 (panic 时刻)")
print("开机时刻(反推)      =", boot_wall)
first = panic_wall - datetime.timedelta(seconds=113997.282188 - 104485.842925)
print("首个前兆墙钟        =", first)
print("前兆窗口总长        =", 113997.282188 - 104485.842925, "s =", (113997.282188-104485.842925)/3600.0, "h")

print()
print("== [6] ESR 解码 ==")
esr = 0x0000000096000004
print("ESR =", h(esr))
print("EC  =", h((esr >> 26) & 0x3f), "= 0x25 DABT current EL")
print("ISV =", (esr >> 24) & 1, " SAS=", h((esr>>22)&3), " SSE=", (esr>>21)&1, " SRT=", h((esr>>16)&0x1f))
print("SF  =", (esr >> 15) & 1, " WnR=", (esr>>6)&1, " CM=", (esr>>8)&1)
print("FSC =", h(esr & 0x3f), "= 0x04 level 0 translation fault")
esr2 = 0x0000000096000044
print("前兆 ESR =", h(esr2), " FSC =", h(esr2 & 0x3f), "= 0x04 level 0 translation fault (同型)")

print()
print("== [7] 最终闭合验证 (补充) ==")
M=(1<<64)-1
i0=0xffffd93715b7e000; i1=0xffffd93715ba0000
stream=i0.to_bytes(8,'little')+i1.to_bytes(8,'little')
v=int.from_bytes(stream[6:14],'little')
x20=0xd93715ba0000ffff
T176=0xffffd937172de000
print("数组头错位读 [idx0+6, idx0+14) =", hex(v), "== x20 崩溃值?", v==x20)
print("__per_cpu_offset[176] 真值        =", hex(T176))
print("海明距离(x20, T176) =", bin(T176^x20).count('1'), "bit (35/64 位翻转 → 非单粒子翻转形态)")
print("rol64(T176,16) =", hex(((T176<<16)|(T176>>48))&M), "≠ x20 → 与槽176数据非同源")
print("rol64(__per_cpu_offset[1],16) =", hex(((i1<<16)|(i1>>48))&M), "== x20 ✓ (16bit lane 循环移位)")
print()
print("== [8] 指令语义与地址闭合 ==")
x0=0xffffa6c96a8955d0; w25=176
addr=(x0+(w25<<3))&M
print("LDR X20,[X0, X25, SXTW #3] (f879d814) 地址 =", hex(addr), "= &__per_cpu_offset[176] (架构正确)")
print("ADD X27,X1,X20 (8b14003b): x1+x20 =", hex((0xffffa6c96a4996c0+x20)&M), "== x27 崩溃值 ✓")
print("LDR X23,[X27,#288] (f9409377): x27+288 =", hex((0xd936bc836a4a96bf+288)&M))
print("FAR = 0x0036bc836a4a97df; FAR - (x27+288) =", hex((0x0036bc836a4a97df-((0xd936bc836a4a96bf+288)&M))&M))
print("→ FAR 高 16 位 = 0x0036 (x27=0xd936...) — 说明实际寻址用 x27 全 64 位, FAR 丢弃了 bit63-48")

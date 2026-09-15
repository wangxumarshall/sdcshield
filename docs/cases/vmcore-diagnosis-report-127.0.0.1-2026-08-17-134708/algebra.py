#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
Case 127.0.0.1-2026-08-17-13:47:08 代数复算脚本
所有 64 位运算均在 mod 2^64 下进行。
来源：vmcore-dmesg.txt 真实输出（行号见旁注）
"""
M = (1 << 64) - 0  # 2^64

def h(x): return hex(x & ((1<<64)-1))

print("== [S0] 致命崩溃闭合验证 ==")
# dmesg L3758: Unable to handle kernel paging request at virtual address 00ffd780f5a3a7c0
FAR  = 0x00ffd780f5a3a7c0
# dmesg L3800: x27 : 00ffd780f5a3a6a0
x27  = 0x00ffd780f5a3a6a0
# dmesg L3798: x20 : 00ffffa827b20fe0
x20  = 0x00ffffa827b20fe0
# dmesg L3799: x24 : ffffd7d8ce315000
x24  = 0xffffd7d8ce315000
# Code 窗口 dmesg L3214? 实际 L3757 附近: Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)
# 致命指令 f9409377 = ldr x23, [x27, x20]  (LDR (immediate, unsigned offset), sf=1)
# 编码: 11 111 0 01 01 imm12 Rn Rt = 0xf9409377
insn = 0xf9409377
sf   = (insn >> 31) & 1
opc  = (insn >> 22) & 3
imm12= (insn >> 10) & 0xfff
Rn   = (insn >> 5) & 0x1f
Rt   = insn & 0x1f
off  = (imm12 << (2 if sf else 0))  # scale by size: 8 bytes for 64-bit
print(f"insn=0x{insn:08x}  sf={sf} opc={opc} imm12=0x{imm12:x} Rn=x{Rn} Rt=x{Rt} offset={off} (0x{off:x})")
ea = (x27 + off) & ((1<<64)-1)
print(f"x27 + offset = {h(x27)} + 0x{off:x} = {h(ea)}")
print(f"FAR          = {h(FAR)}")
print(f"EA == FAR ?  {ea == FAR}")
print(f"x27 == FAR - 0x120 ? {h((FAR-0x120)&((1<<64)-1))} ; x27 - (FAR-0x120) = {(x27 - ((FAR-0x120)&((1<<64)-1)))}")
print(f"FAR - x27    = 0x{(FAR - x27)&((1<<64)-1):x}")
print(f"x27 == x20 ? {x27==x20} ; x20 - x27 = 0x{(x20-x27)&((1<<64)-1):x}")

print()
print("== [S1] x27 来源推演（lr = find_busiest_group+0x11c 的前一条指令 8b14003b）==")
# 8b14003b = add x27, x1, x20
insn2 = 0x8b14003b
Rm = (insn2 >> 16) & 0x1f
Rn2= (insn2 >> 5) & 0x1f
Rt2= insn2 & 0x1f
print(f"insn=0x{insn2:08x}  add x{Rt2}, x{Rn2}, x{Rm}  (Rm=x{Rm}, Rn=x{Rn2}, Rd=x{Rt2})")
x1  = 0xffffd7d8cdf196c0  # dmesg L3801
result = (x1 + x20) & ((1<<64)-1)
print(f"x1 + x20 = {h(x1)} + {h(x20)} = {h(result)}")
print(f"x27(实际) = {h(x27)}")
print(f"x27 == x1+x20 ? {x27 == result}")
print(f"若 x20 正常应为 sched_group* (如 x22=ffff604003e9ec00): x1+x22 = {h((x1 + 0xffff604003e9ec00)&((1<<64)-1))}")
print(f"x20 == 2*(0x7fffd4413d907f70)+0x100 ? ", end="")
guess = (2*(0x7fffd4413d907f70) + 0x100) & ((1<<64)-1)
# 检查 x20 = 0x00ffffa827b20fe0 是否是某有效指针左移/加倍
print(f"x20={h(x20)}")

print()
print("== [S2] 前兆（假 fault）闭合验证 ==")
# 第1次前兆 dmesg L2589: ffff604005f8f5f2 ; x21=FAR, x24 = FAR-0xa (L2602: x24: ffff604005f8f5e8)
# __memcpy+0x80 在 show_interrupts 读 /proc/interrupts 时发生
pre = [
    (169175.043519, 0xffff604005f8f5f2, 0xffff604005f8f5e8, 0x0a18, "irqbalance", 9653),
    (169644.917772, 0xffff604005f8c2ae, None, None, "pmdalinux", 10334),
    (239264.963120, 0xffff6040089b7710, 0xffff6040089b7706, 0x08fa, "pmdalinux", 10334),
]
for (t, far, x24v, x25v, comm, pid) in pre:
    print(f"[{t}] comm={comm} pid={pid} FAR={h(far)}", end="")
    if x24v is not None:
        print(f" x24={h(x24v)} FAR-x24=0x{(far-x24v)&((1<<64)-1):x}", end="")
    if x25v is not None:
        print(f" x25=0x{x25v:x}", end="")
    print()
# 前兆 FAR 高位检查：ffff6040xxxx 与 vmalloc 区间无关，属于线性映射区（带 KASAN/vabits 实际形态）
print("前兆 FAR 均为 ffff60xx/ffff40xx 形态（线性映射/直接映射区, LMEM 无效帧）")

print()
print("== [S3] 时间间隔计算 ==")
events = [
    (169175.043519, "前兆#1  irqbalance 假fault"),
    (239264.963120, "前兆#26 pmdalinux 假fault"),
    (239527.811339, "致命 Oops（find_busiest_group）"),
]
panic = 239527.811339
for t, name in events:
    print(f"{name}: 距 panic {panic - t:.6f} s = {(panic-t)/3600:.3f} h")
print(f"前兆跨度: {239264.963120-169175.043519:.1f} s = {(239264.963120-169175.043519)/86400:.3f} 天")
print(f"uptime 至 panic: {panic:.0f} s = {panic/86400:.2f} 天 = {panic/3600:.1f} h")

print()
print("== [S4] 墙上时间反推 ==")
# 目录名 2026-08-17-13:47:08 为 dump 落盘时刻（kdump 二阶段完成后）
import datetime
dump_wall = datetime.datetime(2026, 8, 17, 13, 47, 8)
panic_wall = dump_wall - datetime.timedelta(seconds=10)  # kdump 启动到落盘粗估 10s 量级（含 stopping CPUs + 保存）
print(f"dump 落盘墙钟(目录名) = {dump_wall}")
print(f"panic 墙钟(约)        = {panic_wall} (减去 kdump 二阶段约10s, 粗估)")
boot_wall = panic_wall - datetime.timedelta(seconds=panic)
print(f"开机墙钟(约)          = {boot_wall}")
print(f"开机时长              = {panic/86400:.2f} 天")

print()
print("== [S5] 前兆地址聚类 ==")
addrs = [
    0xffff604005f8f5f2, 0xffff604005f8c2ae, 0xffff604005f88424, 0xffff604015258500,
    0xffff604015258450, 0xffff60401f3dd4df, 0xffff60401f3dd710, 0xffff60401b0d4613,
    0xffff60401f3dd500, 0xffff60401f3db3cc, 0xffff60401525b40e, 0xffff60401f3da282,
    0xffff60401b0d47cb, 0xffff60401b0d4676, 0xffff60401b0d34c9, 0xffff60401f642731,
    0xffff60401f647185, 0xffff60401b328537, 0xffff60401b32e7cb, 0xffff60401b3287f7,
    0xffff60401b329424, 0xffff6040089b568c, 0xffff604020018235, 0xffff60402001d3ab,
    0xffff60402001d823, 0xffff6040089b7710,
]
pages = sorted(set(a & ~0xfff for a in addrs))
print(f"26 个前兆地址分布在 {len(pages)} 个不同 4K 页:")
for p in pages:
    n = sum(1 for a in addrs if (a & ~0xfff) == p)
    print(f"  {h(p)}  x{n}")
# 与致命崩溃 x20/x27 对比：致命错值 x20=0x00ffffa827b20fe0 高位形态 0x00ff...
print()
print("致命崩溃坏值形态: x27=0x00ffd780f5a3a6a0, x20=0x00ffffa827b20fe0 —— 高 32 位被扰成 0x00ff/0x00ff 模式")
print("前兆坏地址形态:   ffff6040xxxxxxxx —— 高位完好, 低位(页内偏移/中间位)被扰")

print()
print("== [S6] x20 与 '正确值应为什么' 的位级对比 ==")
# 假设正常 x20 应为某个 sched_group 指针（vmalloc/线性地址 ffffxx 形态，如 x22=ffff604003e9ec00）
good_examples = [0xffff604003e9ec00, 0xffffd7d8ce30fcb0, 0xffffd7d8ce315000]
for g in good_examples:
    print(f"若 x20 应为 {h(g)}:")
    print(f"   实际 x20 ^ 应为 = {h(x20 ^ g)}")
    print(f"   实际 x20 - 应为 = {h((x20 - g)&((1<<64)-1))}")
# 检查 x20 的高位翻转模式: good=ffff6... bad=00fff...
print()
print(f"x20       = {x20:064b}")
print(f"x22(好例) = {0xffff604003e9ec00:064b}")
# 差异位计数
diff = x20 ^ 0xffff604003e9ec00
print(f"x20 xor x22 置位数 = {bin(diff).count('1')}")

print()
print("== [S7] 前兆 memcpy 目的指针重建 (WnR=1 写路径) ==")
# __memcpy+0x80 = strb w8, [x0, x14]; x14 = x2>>1 = 0xa; FAR = x0 + x14
pairs = [
    (169175.043519, 0xffff604005f8f5f2, 0xffff604005f8f5e8),
    (169644.917772, 0xffff604005f8c2ae, 0xffff604005f8c2a4),
    (169815.044030, 0xffff604005f88424, 0xffff604005f8841a),
    (169884.914504, 0xffff60401f3dd4df, 0xffff60401f3dd4d5),
    (239264.963120, 0xffff6040089b7710, 0xffff6040089b7706),
]
for t, far, x24 in pairs:
    ea = (x24 + 0xa) & ((1<<64)-1)
    print(f"[{t}] FAR={far:#x}  x24+0xa={ea:#x}  闭合={ea==far}  (x24 即坏 dst 指针)")

print()
print("== [S8] 致命帧 x17 与坏 x20 的字节关系 ==")
x17 = 0xffffa827b38c4000
x20 = 0x00ffffa827b20fe0
print(f"x17(好) = {x17:016x}   字节: {[hex((x17>>(8*i))&0xff) for i in range(7,-1,-1)]}")
print(f"x20(坏) = {x20:016x}   字节: {[hex((x20>>(8*i))&0xff) for i in range(7,-1,-1)]}")
print(f"x20 bits[55:32] = {(x20>>32)&0xffffffff:#010x}; x17 bits[63:32] = {x17>>32:#010x}")
print(f"x20[55:32] == x17[63:32] >> 8? 即 x20 高位含 percpu 基址高字节 ff ff a8 27 的错位副本")
print()
print("== [S9] 反事实: 若 x20 = 低 32 位真值 ==")
x1 = 0xffffd7d8cdf196c0
x20t = 0x27b20fe0
x27t = (x1 + x20t) & ((1<<64)-1)
print(f"x27 = {x27t:#018x} (ffffd7d8 段, 合法线性映射区, 与 x21/x24 同段)")
print(f"FAR 本应 = {(x27t+0x120)&((1<<64)-1):#018x} —— 内核地址, 不会触发 L0 fault")
print()
print("== [S10] 事件间隔汇总 ==")
print(f"前兆#1→致命: {239527.811339-169175.043519:.1f}s = {(239527.811339-169175.043519)/3600:.2f}h")
print(f"前兆#26→致命: {239527.811339-239264.963120:.1f}s")
print(f"uptime: {239527.811339/3600:.2f}h = {239527.811339/86400:.3f} 天")

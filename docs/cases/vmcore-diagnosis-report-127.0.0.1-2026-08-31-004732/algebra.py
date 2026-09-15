#!/usr/bin/env python3
# -*- coding: utf-8 -*-
# 代数复算脚本：127.0.0.1-2026-08-31-00:47:32 转储 SDC 根因诊断
# 所有 64 位运算均 mod 2^64。数据来源：
#   - vmcore-dmesg.txt 致命 Oops 块 (行3195-3243) 与 13 次前兆 WARNING
#   - crash 会话真实输出 (rd/p/dis/struct/vtop)
M = (1 << 64) - 1

def u64(x): return x & M
def hx(x): return "0x%016x" % u64(x)

print("=" * 78)
print("A. 寄存器闭合验证：致命指令 find_busiest_group+0x140 (ldr x23,[x27,#288])")
print("=" * 78)
x1_true   = 0xffffc1a985e596c0   # &runqueues（crash: px &runqueues；dmesg x1 = ffffc1a985e596c0）
x20_true  = 0xffffbe56fa9b6000   # __per_cpu_offset[60]（crash: rd __per_cpu_offset+480；x25=0x3c=60）
x20_obs   = 0xa000ffffbe56fb25   # dmesg x20（被腐化的装载结果）
x27_obs   = 0xa000c1a9443c91e5   # dmesg x27
far       = 0x0000c1a9443c9305   # dmesg FAR

print("x1  (真值 &runqueues)      =", hx(x1_true))
print("x20 (真值 per_cpu_off[60]) =", hx(x20_true))
print("x20 (Oops 报告值)          =", hx(x20_obs))
print("x27 (Oops 报告值)          =", hx(x27_obs))
print()
s = u64(x1_true + x20_obs)
print("[闭合等式1] x27_obs == x1_true + x20_obs ?", s == x27_obs,
      "  (%s)" % hx(s))
print("  → 加法器本身算对了：它把【已被腐化的 x20】忠实地加进了 x27")
s2 = u64(x1_true + x20_true)
print("[反事实]   x1_true + x20_true =", hx(s2), " ← 本应得到的 cpu_rq(60)")
print("           +0x120 =", hx(u64(s2 + 0x120)), " ← 本应访问的有效 percpu 线性映射地址")
print()
print("[闭合等式2] FAR == (x27_obs + 0x120) & 0x0000ffffffffffff ?",
      hex(u64(x27_obs + 0x120) & 0x0000ffffffffffff), "==", hex(far))
print("  → ldr 的 #288(=0x120) 偏移与 FAR 低位逐位吻合；")
print("     0xa000 顶 16 位被 48 位 VA/TBI 规则丢弃，bits[55:48]=0x00 → 走 TTBR0(用户页表)")
print()

print("=" * 78)
print("B. x20 腐化形态分解：16 位右移 + 0xa000 顶标签 + 低 9 位乱码")
print("=" * 78)
shifted = u64((x20_true >> 16) | (0xa000 << 48))
print("x20_true >> 16               =", hx(x20_true >> 16))
print("(true>>16) | (0xa000<<48)    =", hx(shifted))
print("x20_obs                      =", hx(x20_obs))
d = shifted ^ x20_obs
print("XOR                          =", hx(d), " 置位数 =", bin(d).count("1"))
print("  → 差异仅集中在 bits[8:1]（0x01be，7 位）；bits[47:9] 完全等于 (x20_true>>16)")
print()
dx = x20_true ^ x20_obs
print("x20_true ^ x20_obs =", hx(dx), " 置位数 =", bin(dx).count("1"))
print("内存真值在转储中完好（rd __per_cpu_offset+480 实测）→ 不是内存被写坏，是装载结果被腐化")
print()

print("=" * 78)
print("C. 13 次前兆 spurious translation fault 统计（全部 CPU 179）")
print("=" * 78)
events = [
 (10520.595451, 0xffff60400826514e, 14074, "pmdalinux"),
 (11620.539654, 0xffff604005a392ae, 14074, "pmdalinux"),
 (11775.035222, 0xffff6040076271f3,  9678, "irqbalance"),
 (12975.056038, 0xffff20200adbc2f0,  9678, "irqbalance"),
 (13390.527723, 0xffff604088acb4d4, 14074, "pmdalinux"),
 (282138.527751, 0xffff60415e428327, 14074, "pmdalinux"),
 (345159.035134, 0xffff202016b2d4b3,  9678, "irqbalance"),
 (362639.097000, 0xffff202018a253b6,  9678, "irqbalance"),
 (363149.069000, 0xffff604349d14101,  9678, "irqbalance"),
 (363388.500461, 0xffff604349d10726, 14074, "pmdalinux"),
 (396119.593918, 0xffff604349d163ab, 14074, "pmdalinux"),
 (396119.598079, 0xffff604349d16466, 14074, "pmdalinux"),
 (396119.621180, 0xffff604349d1649d, 14074, "pmdalinux"),
]
FATAL = 396122.719381
for ts, va, pid, comm in events:
    print("  t=%12.6f  VA=%#018x  pid=%-6d %-10s  距致命=%10.3fs  低12位=0x%03x" %
          (ts, va, pid, comm, FATAL - ts, va & 0xfff))
print("  t=%12.6f  致命 Oops  CPU179 rcu_sched  find_busiest_group+0x140" % FATAL)
print()
print("全部 13 个 VA 均为非对齐地址（低 12 位≠0）→ 与 __memcpy 16 字节非对齐装载的读取足迹一致")
print("W11/W12/W13 间隔 0.00416s / 0.02310s，距致命 Oops 仅 3.098s → 终末期错误爆发")
print("W9..W13 全部落在线性映射 0xffff6043_49d1_xxxx 同一 64KB 区域（/proc/interrupts 文本缓冲）")
print()

print("=" * 78)
print("D. 墙上时间线（开机时刻 = 崩溃墙钟 - uptime）")
print("=" * 78)
import datetime
crash_dt = datetime.datetime(2026, 8, 31, 0, 46, 39)   # crash sys 输出 DATE
uptime = 4 * 86400 + 14 * 3600 + 2 * 60 + 3             # 4 days, 14:02:03 = 396123s
boot = crash_dt - datetime.timedelta(seconds=uptime)
print("崩溃墙钟(来自 crash sys DATE):", crash_dt, " uptime:", uptime, "s")
print("开机墙钟:", boot)
for ts, va, pid, comm in events:
    print("  %-24s  %s" % (boot + datetime.timedelta(seconds=ts), comm))
print("  %-24s  致命 Oops" % (boot + datetime.timedelta(seconds=FATAL)))
print("转储目录名 00:47:32 − 崩溃 00:46:39 = 43s（kdump 落盘延迟）")
print()

print("=" * 78)
print("E. 指令窗口解码（Code: f9400782 f879d814 2a1903e0 8b14003b (f9409377)）")
print("=" * 78)
def dec_ldr_imm(f):
    imm = ((f >> 10) & 0xfff) * 8; rn = (f >> 5) & 31; rt = f & 31
    return "ldr x%d, [x%d, #%d]" % (rt, rn, imm)
for f, note in [(0xf9400782, "+0x128"), (0xf879d814, "+0x12c"), (0x2a1903e0, "+0x138"),
                (0x8b14003b, "+0x13c"), (0xf9409377, "+0x140 ← 致命指令")]:
    if (f >> 22) == 0x3e5:
        print("  %s  %-10s %s" % (hex(f), note, dec_ldr_imm(f)))
    else:
        print("  %s  %-10s %s" % (hex(f), note,
              {0xf879d814: "ldr x20,[x0,w25,sxtw#3] ← 装载 __per_cpu_offset[60]",
               0x2a1903e0: "mov w0,w25",
               0x8b14003b: "add x27,x1,x20 ← 闭合验证点"}[f]))

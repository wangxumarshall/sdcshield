#!/usr/bin/env python3
# algebra.py — vmcore-diagnosis-report-127.0.0.1-2026-09-04-223938
# 寄存器代数复算（本次独立会话重写，禁止手算，全部模 2^64）
# 输入均为实测值（dmesg 原文行号 / crash_session_*.log 原文），来源标注在每行。

M = 1 << 64

def hx(v):
    return "0x%016x" % (v % M)

print("=" * 78)
print("A. 崩溃指令闭合（FAR - x27 = 指令 immediate）")
print("=" * 78)
# dmesg 行 2941: FAR = ffffbda5543597e0
# dmesg 行 2943: x27: ffffbda5543596c0
# crash_session_2: +320: ldr x23, [x27, #288]  (0xf9409377)
FAR = 0xffffbda5543597e0
x27 = 0xffffbda5543596c0
diff = (FAR - x27) % M
print("FAR          =", hx(FAR), "(dmesg 行2941)")
print("x27          =", hx(x27), "(dmesg 行2943)")
print("FAR - x27    =", hx(diff))
print("指令 immediate #0x120 = 288 =", 0x120)
assert diff == 0x120, "闭合失败"
print(">> 闭合: FAR = x27 + 0x120 ✓  (出错指令确为 ldr x23,[x27,#0x120])")

# 指令手工解码
instr = 0xf9409377  # dmesg 行 2992 Code: ...(f9409377)
size = (instr >> 30) & 3; V = (instr >> 26) & 1; opc = (instr >> 22) & 3
imm12 = (instr >> 10) & 0xfff; Rn = (instr >> 5) & 0x1f; Rt = instr & 0x1f
print("\n指令 0xf9409377 解码: size=%d V=%d opc=%d imm12=0x%x Rn=x%d Rt=x%d"
      % (size, V, opc, imm12, Rn, Rt))
print("  → LDR x%d, [x%d, #0x%x]  (byte offset = imm12 << 3 = 0x%x)"
      % (Rt, Rn, imm12 << size, imm12 << size))
assert Rn == 27 and Rt == 23 and (imm12 << size) == 0x120
print("  ✓ 与 crash dis 完全一致")

print()
print("=" * 78)
print("B. x27 生成链闭合（x27 = x1 + x20，崩溃时 x20 = 0）")
print("=" * 78)
# crash_session_2 反汇编:
#   +300: ldp x0, x1, [sp, #8]      → x1 = sp[16]
#   +308: ldr x20, [x0, w25, sxtw #3]  → x20 = __per_cpu_offset[w25]
#   +316: add x27, x1, x20
#   +320: ldr x23, [x27, #0x120]    ← 崩溃
# crash_session_22 实测栈槽（sp = ffff8000ce5ab390，dmesg 行2946）:
#   rd ffff8000ce5ab398 8 →
#     sp+8  = ffffbda5547555d0 = &__per_cpu_offset[0]  (x0 来源)
#     sp+16 = ffffbda5543596c0 = &runqueues(静态)      (x1 来源)
sp8 = 0xffffbda5547555d0   # session22: rd ffff8000ce5ab398 第一项
sp16 = 0xffffbda5543596c0  # session22: 第二项
print("崩溃栈 sp+8  =", hx(sp8), "=&__per_cpu_offset[0] (crash sym 实证)")
print("崩溃栈 sp+16 =", hx(sp16), "= &runqueues 静态地址 (crash sym 实证)")
# dmesg 行 2943: x1 = ffffbda5543596c0（崩溃时 x1 寄存器值）
# dmesg 行 2943: x20 = 0000000000000000（崩溃时 x20 寄存器值）
x1 = 0xffffbda5543596c0
x20 = 0x0000000000000000
print("x1 (dmesg)   =", hx(x1), "== sp+16 ✓ (x1 数据通路完好)")
print("x20 (dmesg)  =", hx(x20))
x27_calc = (x1 + x20) % M
print("x27 = x1+x20 =", hx(x27_calc), "== 实测 x27", hx(x27), "→", x27_calc == x27)
assert x27_calc == x27
print(">> 闭合: x27 = x1 + 0 ✓  —— add 指令本身算术正确，异常在 x20 的取值")

print()
print("=" * 78)
print("C. x20 期望值 vs 实测（内存实值非零，寄存器值为零）")
print("=" * 78)
# dmesg 行 2943: x25 = 0x38 = 56 → w25 = 56
# crash_session_22: __per_cpu_offset[56] = ffffc25b2c42e000（表内实值）
# crash_session_4/5/21: __per_cpu_offset[179] = ffffc25b2d484000
w25 = 0x38
off56 = 0xffffc25b2c42e000
off179 = 0xffffc25b2d484000
print("w25 (x25低32位, dmesg) =", w25)
print("__per_cpu_offset[56]  (内存实值, session22) =", hx(off56))
print("__per_cpu_offset[179] (内存实值, session4)  =", hx(off179))
print("x20 实测 (dmesg) = 0x0000000000000000")
xor56 = off56 ^ 0
print("\n若 x20 应为 [56]:  期望→实测 XOR =", hx(xor56),
      "popcount =", bin(xor56).count("1"), "位同时翻转")
x27_exp56 = (x1 + off56) % M
print("  若正常: x27 = x1 + [56] =", hx(x27_exp56),
      "（该地址在 percpu vmalloc 区，有有效映射，不会 fault）")
x27_exp179 = (x1 + off179) % M
print("  若 w25=179: x27 = x1 + [179] =", hx(x27_exp179),
      "（crash 会话12 实证 rq->cpu=179 位于此）")
print("\n实测 x27 =", hx(x27), "= runqueues 静态链接地址（.data..percpu 段镜像）")
print("  该地址 +0x120 落在内核映像静态 percpu 镜像，PTE=0（vtop session7 实证）→ level 3 fault ✓")

print()
print("=" * 78)
print("D. 反事实验证（若 x20 正常，崩溃不会发生）")
print("=" * 78)
# crash_session_7: vtop 0xffffbda5543597e0 → PTE: 0（无映射）
# crash_session_12: rd 0xffff8000817dd6c0 → rq->cpu = 179 可读（有效映射）
print("反事实1: x20 = [56]  → x27 =", hx(x27_exp56), "（percpu 区，有效映射）→ 无 fault")
print("反事实2: x20 = [179] → x27 =", hx(x27_exp179), "（session12 已成功读取 rq->cpu=179）→ 无 fault")
print("事实:     x20 = 0     → x27 =", hx(x27), "（内核映像静态 percpu 镜像，PTE=0）→ Oops")
print("\n>> 唯一异常量是 x20 = 0；其内存源 __per_cpu_offset[56]/[179] 在转储中实值非零。")
print(">> 内存侧完好 + 寄存器侧错误 → 故障在 CPU 的 LDR 执行/数据返回/写回路径。")

print()
print("=" * 78)
print("E. 8 次 WARNING 的 spurious 地址聚集性（位形态）")
print("=" * 78)
addrs = [0xffff60400839317a, 0xffff6040083931fe, 0xffff604008391676,
         0xffff604008392584, 0xffff604008397747, 0xffff6040083937b5,
         0xffff6040083935bb, 0xffff60400839235e]  # dmesg 行2582..2897
pages = sorted(set(a & ~0xfff for a in addrs))
print("8 个 spurious 虚地址（dmesg 实测）:")
for a in addrs:
    print("   ", hx(a))
print("涉及页帧:", [hex(p) for p in pages], "共", len(pages), "页")
print("全部位于 ffff60400839xxxx = linear map 直接映射（session7 vtop: 1GB 大页 VALID|SHARED|AF）")
print(">> 同一 seq_file 缓冲区附近反复出现虚假 translation fault，跨 110 秒 8 次，全部 CPU179")

print()
print("=" * 78)
print("F. x27 破坏形态：不是位翻转，是数值替换（offset 丢失）")
print("=" * 78)
expected = x27_exp179
actual = x27
xor = expected ^ actual
print("期望 x27(若w25=179) =", hx(expected))
print("实测 x27            =", hx(actual))
print("XOR =", hx(xor), "popcount =", bin(xor).count("1"))
print("低 12 位: 期望 %03x 实测 %03x → 相同！" % (expected & 0xfff, actual & 0xfff))
print("实测值 = 期望值 - __per_cpu_offset[179] (mod 2^64):",
      hx((expected - off179) % M), "== 实测 ✓")
print(">> 形态结论: x27 并非随机位翻转（17 位翻转的 SEU 概率极低），")
print("   而是加数 x20 整体丢失（=0）——数值型 SDC，非位翻转型。")
print()
print("G. 总结闭合链")
print("  1. FAR = x27 + 0x120 ✓ (指令语义)")
print("  2. x27 = x1 + x20 = x1 + 0 ✓ (寄存器代数)")
print("  3. x1 与栈槽 sp+16 一致 ✓ (源数据完好)")
print("  4. x20 应为 __per_cpu_offset[w25]，内存实值非零 ✓ (表完好)")
print("  5. x27 = &runqueues 静态地址 → PTE=0 → level 3 fault ✓ (vtop 实证)")
print("  6. rq(179) 真实地址 ffff8000817dd6c0 可读，rq->cpu=179 ✓ (session12)")

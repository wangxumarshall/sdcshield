#!/usr/bin/env python3
# algebra.py — 64-bit arithmetic verification for vmcore 127.0.0.1-2026-09-04-23:37:57
# All computations done with python3 (no hand arithmetic). Every value sourced from
# crash output (crash_*.log) or vmcore-dmesg.txt.

def h(x): return hex(x)

print("== 1. pfn / section arithmetic ==")
pfn = 0x6040b1d7c                       # dmesg reg dump x1
print("pfn (x1)                 =", h(pfn))
print("pfn >> 15 (section_nr)   =", h(pfn >> 15), "=", pfn >> 15)
print("pfn >> 23 (root index)   =", h(pfn >> 23), "=", pfn >> 23)
print("(pfn>>15) & 0xff         =", h((pfn >> 15) & 0xff))
print("phys = pfn << 12          =", h(pfn << 12))
print("node7 phys range          = 0x604000000000 .. 0x6057ffffffff (dmesg SRAT/initmem line 117)")
print("phys in node7             =", 0x604000000000 <= (pfn << 12) <= 0x6057ffffffff)
print("node7 last section nr     =", h(0x6057ffffffff >> 27), "== __highest_present_section_nr (0xc0aff) ==",
      hex(0x6057ffffffff >> 27) == '0xc0aff')

print()
print("== 2. mem_section two-level addressing ==")
bss_mem_section = 0xffffa7cc032cacf0    # crash: sym mem_section
root = 0xffff6057fffafb00               # crash: p mem_section
print("&mem_section (BSS)       =", h(bss_mem_section))
print("mem_section (root array)  =", h(root))
print("adrp base +0xcf0          =", h(0xffffa7cc032ca000 + 0xcf0), "== &mem_section ==",
      (0xffffa7cc032ca000 + 0xcf0) == bss_mem_section)
x5 = pfn >> 23
target = root + x5 * 8
print("root + x5*8               =", h(target), "(== 0xffff6057fffb5b40 verified by rd)")
mem_val = 0xffff6057fffaeb00            # crash rd -64 0xffff6057fffb5b40
print("memory[root[0xc08]]       =", h(mem_val), "(non-zero, valid)")
sec_struct = mem_val + ((pfn >> 15) & 0xff) * 16
print("section struct addr       =", h(sec_struct), "(== 0xffff6057fffaec60 verified by rd)")
print("  section_mem_map         = 0xfffffc000000000f (present|has_mem_map|online|early)")
print("  usage                   = 0xffff6057fffa27d0 (non-NULL, verified)")
print("root[0xc09]               = 0xffff6057fffadb00, root[0xc0a] = 0xffff6057fffacb00 (both valid)")
print("node6 roots 0xc05-0xc07   = ffff6037fffffb00-style (node6) — array consistent with SRAT")

print()
print("== 3. discriminating which ldr returned 0 ==")
print("register dump: x3=0, x4=0x160, x5=0xc08")
x4_old = pfn >> 15
ubfiz = ((x4_old & 0xff) << 4)
print("ubfiz(x4_old, #4, #8)     =", h(ubfiz), "== x4 dump (0x160) ==", ubfiz == 0x160)
print("=> ubfiz at +0x2c EXECUTED => cbz at +0x24 NOT taken => +0x20 load returned non-zero (correct root)")
print("=> +0x28 ldr executed; if it returned G (garbage):")
print("   G != 0 : csel -> x3 = G+0x160, FAR = G+0x168 != 8  [contradicts FAR=8]")
print("   G == 0 : cmp eq, csel -> x3 = 0, x4 = 0+0x160 = 0x160, FAR = 8  [EXACT match]")
print("=> unique consistent solution: the +0x28 load returned 0")

print()
print("== 4. fault == FAR / ESR ==")
print("FAR = x3 + 8 = 0x8        (dmesg: 'NULL pointer dereference at virtual address 0000000000000008')")
print("ESR = 0x96000004: EC=0x25 DABT(current EL), SET=0, FnV=0, EA=0, S1PTW=0, WnR=0, FSC=0x04 L0 translation fault")
print("WnR=0 → READ. The faulting access ldr x0,[x3,#8] is itself a *read*; the corruption happened on the *prior* read (+0x28).")

print()
print("== 5. x27 discrepancy resolution (main-session hypothesis check) ==")
x27 = 0xffff6057fffbfb00
print("x27 (reg dump)            =", h(x27))
print("node_data[7] (crash p)    = 0xffff6057fffbfb00  → x27 == NODE_DATA(7), NOT the mem_section root")
print("mem_section root          = 0xffff6057fffafb00; delta =", h(x27 - root),
      "(=64KB; pglist_data sits just above root array in memblock)")
print("=> main-session guess that 'x27 carries the root pointer' is WRONG; x27 = pgdat of node 7")
print("   (set by free_unref_folios+612: ldr x27,[x23,x3,lsl#3], x23=&node_data, x3=7)")

print()
print("== 6. vmemmap / struct page ==")
vmemmap = 0xfffffc0000000000            # implied: x28 - pfn*64
x28 = 0xfffffd8102c75f00
print("x28 (reg dump, folio)     =", h(x28))
print("implied vmemmap base      =", h(x28 - pfn * 64))
print("x28 == base + pfn*64      =", (x28 - pfn * 64) == 0xfffffc0000000000)
print("page->flags               = 0x075ffffe00000000 (crash rd)")
print("(flags>>53)&7 = node id   =", (0x075ffffe00000000 >> 53) & 7, "== x26 dump (2) ==",
      ((0x075ffffe00000000 >> 53) & 7) == 2)
print("flags>>56                 =", 0x075ffffe00000000 >> 56, "(x3 at +592 in caller; node_data index)")

print()
print("== 7. normal-path reconstruction (what SHOULD have happened) ==")
print("x3 = 0xffff6057fffaeb00 (root[0xc08]); x4 = 0x160")
print("x4 = x3 + x4 =", h(0xffff6057fffaeb00 + 0x160), "; csel(ne) -> x3 = 0xffff6057fffaec60")
print("ldr x0,[x3,#8] -> usage = 0xffff6057fffa27d0  (valid, non-NULL)")
usage = 0xffff6057fffa27d0
word = (pfn >> 13) & 3
shift = ((pfn >> 9) & 63) * 4
print("pfnblock word idx         =", word)
print("bit shift                 =", shift)
print("pageblock_flags[0]        = 0x1111111111111111 (crash rd)")
val = (0x1111111111111111 >> shift) & 7
print("(word >> shift) & mask(7) =", val, "(PB_migratetype=1 MIGRATE_MOVABLE) — no fault would occur")

print()
print("== 8. timeline ==")
print("uptime at fault           = 2838.824881 s =", round(2838.824881/60, 2), "min")
print("task start_time           = 2831.226468 s; task age at fault =", round(2838.824881-2831.226468, 2), "s")
print("fault CPU 179 = MPIDR 0x7a0300 (dmesg GIC line), node 7 (CPUs 168-191)")
print("target phys 0x6057fffb5b40 ∈ node 7 → node-local load")
print()
print("== 9. fleet crash-CPU tally (17 dumps on this node) ==")
tally = {179: 16, 168: 1}
print(tally, "— 16/17 on CPU 179; the single CPU-168 crash is also node 7")

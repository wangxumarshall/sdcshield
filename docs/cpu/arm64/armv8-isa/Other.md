# ARMv8-A Other Instructions

Total: 81 instructions

---

## AXFLAG

**Description:** Convert floating-point condition flags from Arm to external format

**Details:**
```
AXFLAG: Convert floating-point condition flags from Arm to external format.
B: Branch.
B.cond: Branch conditionally.
BC.cond: Branch Consistent conditionally.
BFC: Bitfield Clear: an alias of BFM.
BFI: Bitfield Insert: an alias of BFM.
BFM: Bitfield Move.
BFXIL: Bitfield extract and insert at low end: an alias of BFM.
BIC (shifted register): Bitwise Bit Clear (shifted register).
BICS (shifted register): Bitwise Bit Clear (shifted register), setting flags.
BL: Branch with Link.
A64 -- Base Instructions (alphabetic order)
Page 2
RETIRED

=
```

---

## BC.cond

**Description:** Branch Consistent conditionally

**Details:**
```
BC.cond: Branch Consistent conditionally.
BFC: Bitfield Clear: an alias of BFM.
BFI: Bitfield Insert: an alias of BFM.
BFM: Bitfield Move.
BFXIL: Bitfield extract and insert at low end: an alias of BFM.
BIC (shifted register): Bitwise Bit Clear (shifted register).
BICS (shifted register): Bitwise Bit Clear (shifted register), setting flags.
BL: Branch with Link.
A64 -- Base Instructions (alphabetic order)
Page 2
RETIRED

=
```

---

## BFXIL

**Description:** Bitfield extract and insert at low end:

**Details:**
```
BFXIL: Bitfield extract and insert at low end: an alias of BFM.
BIC (shifted register): Bitwise Bit Clear (shifted register).
BICS (shifted register): Bitwise Bit Clear (shifted register), setting flags.
BL: Branch with Link.
A64 -- Base Instructions (alphabetic order)
Page 2
RETIRED

=
```

---

## CFINV

**Description:** Invert Carry Flag

**Details:**
```
CFINV: Invert Carry Flag.
CFP: Control Flow Prediction Restriction by Context: an alias of SYS.
CINC: Conditional Increment: an alias of CSINC.
CINV: Conditional Invert: an alias of CSINV .
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CFP

**Description:** Control Flow Prediction Restriction by Context:

**Details:**
```
CFP: Control Flow Prediction Restriction by Context: an alias of SYS.
CINC: Conditional Increment: an alias of CSINC.
CINV: Conditional Invert: an alias of CSINV .
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CLREX

**Description:** Clear Exclusive

**Details:**
```
CLREX: Clear Exclusive.
CLS: Count Leading Sign bits.
CLZ: Count Leading Zeros.
CMN (extended register): Compare Negative (extended register): an alias of ADDS (extended register).
CMN (immediate): Compare Negative (immediate): an alias of ADDS (immediate).
CMN (shifted register): Compare Negative (shifted register): an alias of ADDS (shifted register).
CMP (extended register): Compare (extended register): an alias of SUBS (extended register).
CMP (immediate): Compare (immediate): an alias of SUBS (immediate).
CMP (shifted register): Compare (shifted register): an alias of SUBS (shifted register).
CMPP: Compare with Tag: an alias of SUBPS.
CNEG: Conditional Negate: an alias of CSNEG.
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CPP

**Description:** Cache Prefetch Prediction Restriction by Context:

**Details:**
```
CPP: Cache Prefetch Prediction Restriction by Context: an alias of SYS.
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CPYFP , CPYFM, CPYFE

**Description:** Memory Copy Forward-only

**Details:**
```
CPYFP , CPYFM, CPYFE: Memory Copy Forward-only.
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CPYFPN, CPYFMN, CPYFEN

**Description:** Memory Copy Forward-only, reads and writes non-temporal

**Details:**
```
CPYFPN, CPYFMN, CPYFEN: Memory Copy Forward-only, reads and writes non-temporal.
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CPYFPRN, CPYFMRN, CPYFERN

**Description:** Memory Copy Forward-only, reads non-temporal

**Details:**
```
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads non-temporal.
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CPYFPRT, CPYFMRT, CPYFERT

**Description:** Memory Copy Forward-only, reads unprivileged

**Details:**
```
CPYFPRT, CPYFMRT, CPYFERT: Memory Copy Forward-only, reads unprivileged.
A64 -- Base Instructions (alphabetic order)
Page 3
RETIRED

=
```

---

## CPYFPRTN, CPYFMRTN, CPYFERTN

**Description:** Memory Copy Forward-only, reads unprivileged, reads and writes non-temporal

**Details:**
```
CPYFPRTN, CPYFMRTN, CPYFERTN: Memory Copy Forward-only, reads unprivileged, reads and writes non-temporal.
CPYFPRTRN, CPYFMRTRN, CPYFERTRN: Memory Copy Forward-only, reads unprivileged and non-temporal.
CPYFPRTWN, CPYFMRTWN, CPYFERTWN: Memory Copy Forward-only, reads unprivileged, writes non-temporal.
CPYFPT, CPYFMT, CPYFET: Memory Copy Forward-only, reads and writes unprivileged.
CPYFPTN, CPYFMTN, CPYFETN: Memory Copy Forward-only, reads and writes unprivileged and non-temporal.
CPYFPTRN, CPYFMTRN, CPYFETRN: Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal.
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes n
```

---

## CPYFPRTRN, CPYFMRTRN, CPYFERTRN

**Description:** Memory Copy Forward-only, reads unprivileged and non-temporal

**Details:**
```
CPYFPRTRN, CPYFMRTRN, CPYFERTRN: Memory Copy Forward-only, reads unprivileged and non-temporal.
CPYFPRTWN, CPYFMRTWN, CPYFERTWN: Memory Copy Forward-only, reads unprivileged, writes non-temporal.
CPYFPT, CPYFMT, CPYFET: Memory Copy Forward-only, reads and writes unprivileged.
CPYFPTN, CPYFMTN, CPYFETN: Memory Copy Forward-only, reads and writes unprivileged and non-temporal.
CPYFPTRN, CPYFMTRN, CPYFETRN: Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal.
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Co
```

---

## CPYFPRTWN, CPYFMRTWN, CPYFERTWN

**Description:** Memory Copy Forward-only, reads unprivileged, writes non-temporal

**Details:**
```
CPYFPRTWN, CPYFMRTWN, CPYFERTWN: Memory Copy Forward-only, reads unprivileged, writes non-temporal.
CPYFPT, CPYFMT, CPYFET: Memory Copy Forward-only, reads and writes unprivileged.
CPYFPTN, CPYFMTN, CPYFETN: Memory Copy Forward-only, reads and writes unprivileged and non-temporal.
CPYFPTRN, CPYFMTRN, CPYFETRN: Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal.
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Cop
```

---

## CPYFPT, CPYFMT, CPYFET

**Description:** Memory Copy Forward-only, reads and writes unprivileged

**Details:**
```
CPYFPT, CPYFMT, CPYFET: Memory Copy Forward-only, reads and writes unprivileged.
CPYFPTN, CPYFMTN, CPYFETN: Memory Copy Forward-only, reads and writes unprivileged and non-temporal.
CPYFPTRN, CPYFMTRN, CPYFETRN: Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal.
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unpriv
```

---

## CPYFPTN, CPYFMTN, CPYFETN

**Description:** Memory Copy Forward-only, reads and writes unprivileged and non-temporal

**Details:**
```
CPYFPTN, CPYFMTN, CPYFETN: Memory Copy Forward-only, reads and writes unprivileged and non-temporal.
CPYFPTRN, CPYFMTRN, CPYFETRN: Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal.
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB
```

---

## CPYFPTRN, CPYFMTRN, CPYFETRN

**Description:** Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal

**Details:**
```
CPYFPTRN, CPYFMTRN, CPYFETRN: Memory Copy Forward-only, reads and writes unprivileged, reads non-temporal.
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: C
```

---

## CPYFPTWN, CPYFMTWN, CPYFETWN

**Description:** Memory Copy Forward-only, reads and writes unprivileged, writes non-

**Details:**
```
CPYFPTWN, CPYFMTWN, CPYFETWN: Memory Copy Forward-only, reads and writes unprivileged, writes non-
temporal.
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV
```

---

## CPYFPWN, CPYFMWN, CPYFEWN

**Description:** Memory Copy Forward-only, writes non-temporal

**Details:**
```
CPYFPWN, CPYFMWN, CPYFEWN: Memory Copy Forward-only, writes non-temporal.
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYFPWT, CPYFMWT, CPYFEWT

**Description:** Memory Copy Forward-only, writes unprivileged

**Details:**
```
CPYFPWT, CPYFMWT, CPYFEWT: Memory Copy Forward-only, writes unprivileged.
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYFPWTN, CPYFMWTN, CPYFEWTN

**Description:** Memory Copy Forward-only, writes unprivileged, reads and writes non-

**Details:**
```
CPYFPWTN, CPYFMWTN, CPYFEWTN: Memory Copy Forward-only, writes unprivileged, reads and writes non-
temporal.
CPYFPWTRN, CPYFMWTRN, CPYFEWTRN
: Memory Copy Forward-only, writes unprivileged, reads non-temporal.
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYFPWTWN, CPYFMWTWN, CPYFEWTWN

**Description:** Memory Copy Forward-only, writes unprivileged and non-temporal

**Details:**
```
CPYFPWTWN, CPYFMWTWN, CPYFEWTWN: Memory Copy Forward-only, writes unprivileged and non-temporal.
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYP , CPYM, CPYE

**Description:** Memory Copy

**Details:**
```
CPYP , CPYM, CPYE: Memory Copy.
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPN, CPYMN, CPYEN

**Description:** Memory Copy, reads and writes non-temporal

**Details:**
```
CPYPN, CPYMN, CPYEN: Memory Copy, reads and writes non-temporal.
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPRN, CPYMRN, CPYERN

**Description:** Memory Copy, reads non-temporal

**Details:**
```
CPYPRN, CPYMRN, CPYERN: Memory Copy, reads non-temporal.
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPRT, CPYMRT, CPYERT

**Description:** Memory Copy, reads unprivileged

**Details:**
```
CPYPRT, CPYMRT, CPYERT: Memory Copy, reads unprivileged.
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPRTN, CPYMRTN, CPYERTN

**Description:** Memory Copy, reads unprivileged, reads and writes non-temporal

**Details:**
```
CPYPRTN, CPYMRTN, CPYERTN: Memory Copy, reads unprivileged, reads and writes non-temporal.
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPRTRN, CPYMRTRN, CPYERTRN

**Description:** Memory Copy, reads unprivileged and non-temporal

**Details:**
```
CPYPRTRN, CPYMRTRN, CPYERTRN: Memory Copy, reads unprivileged and non-temporal.
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPRTWN, CPYMRTWN, CPYERTWN

**Description:** Memory Copy, reads unprivileged, writes non-temporal

**Details:**
```
CPYPRTWN, CPYMRTWN, CPYERTWN: Memory Copy, reads unprivileged, writes non-temporal.
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPT, CPYMT, CPYET

**Description:** Memory Copy, reads and writes unprivileged

**Details:**
```
CPYPT, CPYMT, CPYET: Memory Copy, reads and writes unprivileged.
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPTN, CPYMTN, CPYETN

**Description:** Memory Copy, reads and writes unprivileged and non-temporal

**Details:**
```
CPYPTN, CPYMTN, CPYETN: Memory Copy, reads and writes unprivileged and non-temporal.
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPTRN, CPYMTRN, CPYETRN

**Description:** Memory Copy, reads and writes unprivileged, reads non-temporal

**Details:**
```
CPYPTRN, CPYMTRN, CPYETRN: Memory Copy, reads and writes unprivileged, reads non-temporal.
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPTWN, CPYMTWN, CPYETWN

**Description:** Memory Copy, reads and writes unprivileged, writes non-temporal

**Details:**
```
CPYPTWN, CPYMTWN, CPYETWN: Memory Copy, reads and writes unprivileged, writes non-temporal.
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPWN, CPYMWN, CPYEWN

**Description:** Memory Copy, writes non-temporal

**Details:**
```
CPYPWN, CPYMWN, CPYEWN: Memory Copy, writes non-temporal.
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPWT, CPYMWT, CPYEWT

**Description:** Memory Copy, writes unprivileged

**Details:**
```
CPYPWT, CPYMWT, CPYEWT: Memory Copy, writes unprivileged.
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPWTN, CPYMWTN, CPYEWTN

**Description:** Memory Copy, writes unprivileged, reads and writes non-temporal

**Details:**
```
CPYPWTN, CPYMWTN, CPYEWTN: Memory Copy, writes unprivileged, reads and writes non-temporal.
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPWTRN, CPYMWTRN, CPYEWTRN

**Description:** Memory Copy, writes unprivileged, reads non-temporal

**Details:**
```
CPYPWTRN, CPYMWTRN, CPYEWTRN: Memory Copy, writes unprivileged, reads non-temporal.
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CPYPWTWN, CPYMWTWN, CPYEWTWN

**Description:** Memory Copy, writes unprivileged and non-temporal

**Details:**
```
CPYPWTWN, CPYMWTWN, CPYEWTWN: Memory Copy, writes unprivileged and non-temporal.
CRC32B, CRC32H, CRC32W , CRC32X: CRC32 checksum.
CRC32CB, CRC32CH, CRC32CW , CRC32CX: CRC32C checksum.
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CSDB

**Description:** Consumption of Speculative Data Barrier

**Details:**
```
CSDB: Consumption of Speculative Data Barrier.
CSEL: Conditional Select.
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CSET

**Description:** Conditional Set:

**Details:**
```
CSET: Conditional Set: an alias of CSINC.
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## CSETM

**Description:** Conditional Set Mask:

**Details:**
```
CSETM: Conditional Set Mask: an alias of CSINV .
CSINC: Conditional Select Increment.
A64 -- Base Instructions (alphabetic order)
Page 4
RETIRED

=
```

---

## DGH

**Description:** Data Gathering Hint

**Details:**
```
DGH: Data Gathering Hint.
DMB: Data Memory Barrier.
DRPS: Debug restore process state.
DSB: Data Synchronization Barrier.
DVP: Data Value Prediction Restriction by Context: an alias of SYS.
EON (shifted register): Bitwise Exclusive OR NOT (shifted register).
EOR (immediate): Bitwise Exclusive OR (immediate).
EOR (shifted register): Bitwise Exclusive OR (shifted register).
ERET: Exception Return.
ERETAA, ERETAB: Exception Return, with pointer authentication.
ESB: Error Synchronization Barrier.
EXTR: Extract register.
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## DRPS

**Description:** Debug restore process state

**Details:**
```
DRPS: Debug restore process state.
DSB: Data Synchronization Barrier.
DVP: Data Value Prediction Restriction by Context: an alias of SYS.
EON (shifted register): Bitwise Exclusive OR NOT (shifted register).
EOR (immediate): Bitwise Exclusive OR (immediate).
EOR (shifted register): Bitwise Exclusive OR (shifted register).
ERET: Exception Return.
ERETAA, ERETAB: Exception Return, with pointer authentication.
ESB: Error Synchronization Barrier.
EXTR: Extract register.
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## DVP

**Description:** Data Value Prediction Restriction by Context:

**Details:**
```
DVP: Data Value Prediction Restriction by Context: an alias of SYS.
EON (shifted register): Bitwise Exclusive OR NOT (shifted register).
EOR (immediate): Bitwise Exclusive OR (immediate).
EOR (shifted register): Bitwise Exclusive OR (shifted register).
ERET: Exception Return.
ERETAA, ERETAB: Exception Return, with pointer authentication.
ESB: Error Synchronization Barrier.
EXTR: Extract register.
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## ERET

**Description:** Exception Return

**Details:**
```
ERET: Exception Return.
ERETAA, ERETAB: Exception Return, with pointer authentication.
ESB: Error Synchronization Barrier.
EXTR: Extract register.
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## ERETAA, ERETAB

**Description:** Exception Return, with pointer authentication

**Details:**
```
ERETAA, ERETAB: Exception Return, with pointer authentication.
ESB: Error Synchronization Barrier.
EXTR: Extract register.
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## ESB

**Description:** Error Synchronization Barrier

**Details:**
```
ESB: Error Synchronization Barrier.
EXTR: Extract register.
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## GMI

**Description:** Tag Mask Insert

**Details:**
```
GMI: Tag Mask Insert.
HINT: Hint instruction.
HLT: Halt instruction.
HVC: Hypervisor Call.
IC: Instruction Cache operation: an alias of SYS.
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## IRG

**Description:** Insert Random Tag

**Details:**
```
IRG: Insert Random Tag.
ISB: Instruction Synchronization Barrier.
LD64B: Single-copy Atomic 64-byte Load.
LDADD, LDADDA, LDADDAL, LDADDL: Atomic add on word or doubleword in memory.
LDADDB, LDADDAB, LDADDALB, LDADDLB: Atomic add on byte in memory.
LDADDH, LDADDAH, LDADDALH, LDADDLH: Atomic add on halfword in memory.
LDAPR: Load-Acquire RCpc Register.
LDAPRB: Load-Acquire RCpc Register Byte.
LDAPRH: Load-Acquire RCpc Register Halfword.
LDAPUR: Load-Acquire RCpc Register (unscaled).
LDAPURB: Load-Acquire RCpc Register Byte (unscaled).
LDAPURH: Load-Acquire RCpc Register Halfword (unscaled).
LDAPURSB: Load-Acquire RCpc Register Signed Byte (unscaled).
A64 -- Base Instructions (alphabetic order)
Page 5
RETIRED

=
```

---

## MNEG

**Description:** Multiply-Negate:

**Details:**
```
MNEG: Multiply-Negate: an alias of MSUB.
MOV (bitmask immediate): Move (bitmask immediate): an alias of ORR (immediate).
MOV (inverted wide immediate): Move (inverted wide immediate): an alias of MOVN.
MOV (register): Move (register): an alias of ORR (shifted register).
MOV (to/from SP): Move between register and stack pointer: an alias of ADD (immediate).
MOV (wide immediate): Move (wide immediate): an alias of MOVZ.
MOVK: Move wide with keep.
MOVN: Move wide with NOT.
MOVZ: Move wide with zero.
MRS: Move System Register.
MSR (immediate): Move immediate value to Special Register.
MSR (register): Move general-purpose register to System Register.
MSUB: Multiply-Subtract.
MUL: Multiply: an alias of MADD.
MVN: Bitwise NOT: an alias of ORN (shifted register).
NEG (shifted register): Negate (shifted register): an alias of SUB (shifted register).
NEGS: Negate, setting flags: an alias of SUBS (shifted register).
NGC: Negate with Carry: an alias of SBC.
NGCS: Negate with Carry, setting flags: an alias of SBCS.
NOP: No Operation.
ORN (shifted register): Bitwise OR NOT (shifted register).
ORR (immediate): Bitwise OR (immediate).
ORR (shifted register): Bitwise OR (shifted register).
PACDA, PACDZA: Pointer Authentication Code for Data address, using key A.
PACDB, PACDZB: Pointer Authentication Code for Data address, using key B.
PACGA: Pointer Authentication Code, using Generic key.
PACIA, PACIA1716, PACIASP , PACIAZ, PACIZA: Pointer Authentication Code for Instruction address, using key A.
PACIB, PACIB1716, PACIBSP , PACIBZ, PACIZB: Pointer Authentication Code for Instruction address, using key B.
PRFM (immediate): Prefetch Memory (immediate).
PRFM (literal): Prefetch Memory (literal).
PRFM (register): Prefetch Memory (register).
PRFUM: Prefetch Memory (unscaled offset).
A64 -- Base Instructions (alphabetic order)
Page 8
RETIRED

=
```

---

## NGC

**Description:** Negate with Carry:

**Details:**
```
NGC: Negate with Carry: an alias of SBC.
NGCS: Negate with Carry, setting flags: an alias of SBCS.
NOP: No Operation.
ORN (shifted register): Bitwise OR NOT (shifted register).
ORR (immediate): Bitwise OR (immediate).
ORR (shifted register): Bitwise OR (shifted register).
PACDA, PACDZA: Pointer Authentication Code for Data address, using key A.
PACDB, PACDZB: Pointer Authentication Code for Data address, using key B.
PACGA: Pointer Authentication Code, using Generic key.
PACIA, PACIA1716, PACIASP , PACIAZ, PACIZA: Pointer Authentication Code for Instruction address, using key A.
PACIB, PACIB1716, PACIBSP , PACIBZ, PACIZB: Pointer Authentication Code for Instruction address, using key B.
PRFM (immediate): Prefetch Memory (immediate).
PRFM (literal): Prefetch Memory (literal).
PRFM (register): Prefetch Memory (register).
PRFUM: Prefetch Memory (unscaled offset).
A64 -- Base Instructions (alphabetic order)
Page 8
RETIRED

=
```

---

## NGCS

**Description:** Negate with Carry, setting flags:

**Details:**
```
NGCS: Negate with Carry, setting flags: an alias of SBCS.
NOP: No Operation.
ORN (shifted register): Bitwise OR NOT (shifted register).
ORR (immediate): Bitwise OR (immediate).
ORR (shifted register): Bitwise OR (shifted register).
PACDA, PACDZA: Pointer Authentication Code for Data address, using key A.
PACDB, PACDZB: Pointer Authentication Code for Data address, using key B.
PACGA: Pointer Authentication Code, using Generic key.
PACIA, PACIA1716, PACIASP , PACIAZ, PACIZA: Pointer Authentication Code for Instruction address, using key A.
PACIB, PACIB1716, PACIBSP , PACIBZ, PACIZB: Pointer Authentication Code for Instruction address, using key B.
PRFM (immediate): Prefetch Memory (immediate).
PRFM (literal): Prefetch Memory (literal).
PRFM (register): Prefetch Memory (register).
PRFUM: Prefetch Memory (unscaled offset).
A64 -- Base Instructions (alphabetic order)
Page 8
RETIRED

=
```

---

## PSB CSYNC

**Description:** Profiling Synchronization Barrier

**Details:**
```
PSB CSYNC: Profiling Synchronization Barrier.
PSSBB: Physical Speculative Store Bypass Barrier: an alias of DSB.
RBIT: Reverse Bits.
RET: Return from subroutine.
RETAA, RETAB: Return from subroutine, with pointer authentication.
REV: Reverse Bytes.
REV16: Reverse bytes in 16-bit halfwords.
REV32: Reverse bytes in 32-bit words.
REV64: Reverse Bytes: an alias of REV .
RMIF: Rotate, Mask Insert Flags.
ROR (immediate): Rotate right (immediate): an alias of EXTR.
ROR (register): Rotate Right (register): an alias of RORV .
RORV: Rotate Right Variable.
SB: Speculation Barrier.
SBC: Subtract with Carry.
SBCS: Subtract with Carry, setting flags.
SBFIZ: Signed Bitfield Insert in Zero: an alias of SBFM.
SBFM: Signed Bitfield Move.
SBFX: Signed Bitfield Extract: an alias of SBFM.
SDIV: Signed Divide.
SETF8, SETF16: Evaluation of 8 or 16 bit flag values.
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## PSSBB

**Description:** Physical Speculative Store Bypass Barrier:

**Details:**
```
PSSBB: Physical Speculative Store Bypass Barrier: an alias of DSB.
RBIT: Reverse Bits.
RET: Return from subroutine.
RETAA, RETAB: Return from subroutine, with pointer authentication.
REV: Reverse Bytes.
REV16: Reverse bytes in 16-bit halfwords.
REV32: Reverse bytes in 32-bit words.
REV64: Reverse Bytes: an alias of REV .
RMIF: Rotate, Mask Insert Flags.
ROR (immediate): Rotate right (immediate): an alias of EXTR.
ROR (register): Rotate Right (register): an alias of RORV .
RORV: Rotate Right Variable.
SB: Speculation Barrier.
SBC: Subtract with Carry.
SBCS: Subtract with Carry, setting flags.
SBFIZ: Signed Bitfield Insert in Zero: an alias of SBFM.
SBFM: Signed Bitfield Move.
SBFX: Signed Bitfield Extract: an alias of SBFM.
SDIV: Signed Divide.
SETF8, SETF16: Evaluation of 8 or 16 bit flag values.
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## RMIF

**Description:** Rotate, Mask Insert Flags

**Details:**
```
RMIF: Rotate, Mask Insert Flags.
ROR (immediate): Rotate right (immediate): an alias of EXTR.
ROR (register): Rotate Right (register): an alias of RORV .
RORV: Rotate Right Variable.
SB: Speculation Barrier.
SBC: Subtract with Carry.
SBCS: Subtract with Carry, setting flags.
SBFIZ: Signed Bitfield Insert in Zero: an alias of SBFM.
SBFM: Signed Bitfield Move.
SBFX: Signed Bitfield Extract: an alias of SBFM.
SDIV: Signed Divide.
SETF8, SETF16: Evaluation of 8 or 16 bit flag values.
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SB

**Description:** Speculation Barrier

**Details:**
```
SBFM.
ASR (register): Arithmetic Shift Right (register): an alias of ASRV .
ASRV: Arithmetic Shift Right Variable.
AT: Address Translate: an alias of SYS.
AUTDA, AUTDZA: Authenticate Data address, using key A.
AUTDB, AUTDZB: Authenticate Data address, using key B.
AUTIA, AUTIA1716, AUTIASP , AUTIAZ, AUTIZA: Authenticate Instruction address, using key A.
AUTIB, AUTIB1716, AUTIBSP , AUTIBZ, AUTIZB: Authenticate Instruction address, using key B.
AXFLAG: Convert floating-point condition flags from Arm to external format.
B: Branch.
B.cond: Branch conditionally.
BC.cond: Branch Consistent conditionally.
BFC: Bitfield Clear: an alias of BFM.
BFI: Bitfield Insert: an alias of BFM.
BFM: Bitfield Move.
BFXIL: Bitfield extract and insert at low end: an alias of BFM.
BIC (shifted register): Bitwise Bit Clear (shifted register).
BICS (shifted register): Bitwise Bit Clear (shifted register), setting flags.
BL: Branch with Link.
A64 -- Base Instructions (alphabetic order)
Page 2
RETIRED

=
```

---

## SBFIZ

**Description:** Signed Bitfield Insert in Zero:

**Details:**
```
SBFIZ: Signed Bitfield Insert in Zero: an alias of SBFM.
SBFM: Signed Bitfield Move.
SBFX: Signed Bitfield Extract: an alias of SBFM.
SDIV: Signed Divide.
SETF8, SETF16: Evaluation of 8 or 16 bit flag values.
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETF8, SETF16

**Description:** Evaluation of 8 or 16 bit flag values

**Details:**
```
SETF8, SETF16: Evaluation of 8 or 16 bit flag values.
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETGP , SETGM, SETGE

**Description:** Memory Set with tag setting

**Details:**
```
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETGPN, SETGMN, SETGEN

**Description:** Memory Set with tag setting, non-temporal

**Details:**
```
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETGPT, SETGMT, SETGET

**Description:** Memory Set with tag setting, unprivileged

**Details:**
```
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETGPTN, SETGMTN, SETGETN

**Description:** Memory Set with tag setting, unprivileged and non-temporal

**Details:**
```
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETP , SETM, SETE

**Description:** Memory Set

**Details:**
```
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETPN, SETMN, SETEN

**Description:** Memory Set, non-temporal

**Details:**
```
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETPT, SETMT, SETET

**Description:** Memory Set, unprivileged

**Details:**
```
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SETPTN, SETMTN, SETETN

**Description:** Memory Set, unprivileged and non-temporal

**Details:**
```
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SMNEGL

**Description:** Signed Multiply-Negate Long:

**Details:**
```
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SMSUBL

**Description:** Signed Multiply-Subtract Long

**Details:**
```
SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SSBB

**Description:** Speculative Store Bypass Barrier:

**Details:**
```
SSBB: Physical Speculative Store Bypass Barrier: an alias of DSB.
RBIT: Reverse Bits.
RET: Return from subroutine.
RETAA, RETAB: Return from subroutine, with pointer authentication.
REV: Reverse Bytes.
REV16: Reverse bytes in 16-bit halfwords.
REV32: Reverse bytes in 32-bit words.
REV64: Reverse Bytes: an alias of REV .
RMIF: Rotate, Mask Insert Flags.
ROR (immediate): Rotate right (immediate): an alias of EXTR.
ROR (register): Rotate Right (register): an alias of RORV .
RORV: Rotate Right Variable.
SB: Speculation Barrier.
SBC: Subtract with Carry.
SBCS: Subtract with Carry, setting flags.
SBFIZ: Signed Bitfield Insert in Zero: an alias of SBFM.
SBFM: Signed Bitfield Move.
SBFX: Signed Bitfield Extract: an alias of SBFM.
SDIV: Signed Divide.
SETF8, SETF16: Evaluation of 8 or 16 bit flag values.
SETGP , SETGM, SETGE: Memory Set with tag setting.
SETGPN, SETGMN, SETGEN: Memory Set with tag setting, non-temporal.
SETGPT, SETGMT, SETGET: Memory Set with tag setting, unprivileged.
SETGPTN, SETGMTN, SETGETN: Memory Set with tag setting, unprivileged and non-temporal.
SETP , SETM, SETE: Memory Set.
SETPN, SETMN, SETEN: Memory Set, non-temporal.
SETPT, SETMT, SETET: Memory Set, unprivileged.
SETPTN, SETMTN, SETETN: Memory Set, unprivileged and non-temporal.
SEV: Send Event.
SEVL: Send Event Local.
SMADDL: Signed Multiply-Add Long.
SMC: Secure Monitor Call.
SMNEGL: Signed Multiply-Negate Long: an alias of SMSUBL.
SMSUBL: Signed Multiply-Subtract Long.
SMULH: Signed Multiply High.
A64 -- Base Instructions (alphabetic order)
Page 9
RETIRED

=
```

---

## SXTB

**Description:** Signed Extend Byte:

**Details:**
```
SXTB: Signed Extend Byte: an alias of SBFM.
SXTH: Sign Extend Halfword: an alias of SBFM.
SXTW: Sign Extend Word: an alias of SBFM.
SYS: System instruction.
SYSL: System instruction with result.
TBNZ: Test bit and Branch if Nonzero.
TBZ: Test bit and Branch if Zero.
TLBI: TLB Invalidate operation: an alias of SYS.
TSB CSYNC: Trace Synchronization Barrier.
TST (immediate): Test bits (immediate): an alias of ANDS (immediate).
TST (shifted register): Test (shifted register): an alias of ANDS (shifted register).
UBFIZ: Unsigned Bitfield Insert in Zero: an alias of UBFM.
UBFM: Unsigned Bitfield Move.
UBFX: Unsigned Bitfield Extract: an alias of UBFM.
UDF: Permanently Undefined.
UDIV: Unsigned Divide.
UMADDL: Unsigned Multiply-Add Long.
A64 -- Base Instructions (alphabetic order)
Page 12
RETIRED

=
```

---

## SXTH

**Description:** Sign Extend Halfword:

**Details:**
```
SXTH: Sign Extend Halfword: an alias of SBFM.
SXTW: Sign Extend Word: an alias of SBFM.
SYS: System instruction.
SYSL: System instruction with result.
TBNZ: Test bit and Branch if Nonzero.
TBZ: Test bit and Branch if Zero.
TLBI: TLB Invalidate operation: an alias of SYS.
TSB CSYNC: Trace Synchronization Barrier.
TST (immediate): Test bits (immediate): an alias of ANDS (immediate).
TST (shifted register): Test (shifted register): an alias of ANDS (shifted register).
UBFIZ: Unsigned Bitfield Insert in Zero: an alias of UBFM.
UBFM: Unsigned Bitfield Move.
UBFX: Unsigned Bitfield Extract: an alias of UBFM.
UDF: Permanently Undefined.
UDIV: Unsigned Divide.
UMADDL: Unsigned Multiply-Add Long.
A64 -- Base Instructions (alphabetic order)
Page 12
RETIRED

=
```

---

## SXTW

**Description:** Sign Extend Word:

**Details:**
```
SXTW: Sign Extend Word: an alias of SBFM.
SYS: System instruction.
SYSL: System instruction with result.
TBNZ: Test bit and Branch if Nonzero.
TBZ: Test bit and Branch if Zero.
TLBI: TLB Invalidate operation: an alias of SYS.
TSB CSYNC: Trace Synchronization Barrier.
TST (immediate): Test bits (immediate): an alias of ANDS (immediate).
TST (shifted register): Test (shifted register): an alias of ANDS (shifted register).
UBFIZ: Unsigned Bitfield Insert in Zero: an alias of UBFM.
UBFM: Unsigned Bitfield Move.
UBFX: Unsigned Bitfield Extract: an alias of UBFM.
UDF: Permanently Undefined.
UDIV: Unsigned Divide.
UMADDL: Unsigned Multiply-Add Long.
A64 -- Base Instructions (alphabetic order)
Page 12
RETIRED

=
```

---

## TSB CSYNC

**Description:** Trace Synchronization Barrier

**Details:**
```
TSB CSYNC: Trace Synchronization Barrier.
TST (immediate): Test bits (immediate): an alias of ANDS (immediate).
TST (shifted register): Test (shifted register): an alias of ANDS (shifted register).
UBFIZ: Unsigned Bitfield Insert in Zero: an alias of UBFM.
UBFM: Unsigned Bitfield Move.
UBFX: Unsigned Bitfield Extract: an alias of UBFM.
UDF: Permanently Undefined.
UDIV: Unsigned Divide.
UMADDL: Unsigned Multiply-Add Long.
A64 -- Base Instructions (alphabetic order)
Page 12
RETIRED

=
```

---

## UBFIZ

**Description:** Unsigned Bitfield Insert in Zero:

**Details:**
```
UBFIZ: Unsigned Bitfield Insert in Zero: an alias of UBFM.
UBFM: Unsigned Bitfield Move.
UBFX: Unsigned Bitfield Extract: an alias of UBFM.
UDF: Permanently Undefined.
UDIV: Unsigned Divide.
UMADDL: Unsigned Multiply-Add Long.
A64 -- Base Instructions (alphabetic order)
Page 12
RETIRED

=
```

---

## UDF

**Description:** Permanently Undefined

**Details:**
```
UDF: Permanently Undefined.
UDIV: Unsigned Divide.
UMADDL: Unsigned Multiply-Add Long.
A64 -- Base Instructions (alphabetic order)
Page 12
RETIRED

=
```

---

## UMNEGL

**Description:** Unsigned Multiply-Negate Long:

**Details:**
```
UMNEGL: Unsigned Multiply-Negate Long: an alias of UMSUBL.
UMSUBL: Unsigned Multiply-Subtract Long.
UMULH: Unsigned Multiply High.
UMULL: Unsigned Multiply Long: an alias of UMADDL.
UXTB: Unsigned Extend Byte: an alias of UBFM.
UXTH: Unsigned Extend Halfword: an alias of UBFM.
WFE: Wait For Event.
WFET: Wait For Event with Timeout.
WFI: Wait For Interrupt.
WFIT: Wait For Interrupt with Timeout.
XAFLAG: Convert floating-point condition flags from external format to Arm format.
XPACD, XPACI, XPACLRI: Strip Pointer Authentication Code.
YIELD: YIELD.
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---

## UMSUBL

**Description:** Unsigned Multiply-Subtract Long

**Details:**
```
UMSUBL.
UMSUBL: Unsigned Multiply-Subtract Long.
UMULH: Unsigned Multiply High.
UMULL: Unsigned Multiply Long: an alias of UMADDL.
UXTB: Unsigned Extend Byte: an alias of UBFM.
UXTH: Unsigned Extend Halfword: an alias of UBFM.
WFE: Wait For Event.
WFET: Wait For Event with Timeout.
WFI: Wait For Interrupt.
WFIT: Wait For Interrupt with Timeout.
XAFLAG: Convert floating-point condition flags from external format to Arm format.
XPACD, XPACI, XPACLRI: Strip Pointer Authentication Code.
YIELD: YIELD.
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---

## UXTB

**Description:** Unsigned Extend Byte:

**Details:**
```
UXTB: Unsigned Extend Byte: an alias of UBFM.
UXTH: Unsigned Extend Halfword: an alias of UBFM.
WFE: Wait For Event.
WFET: Wait For Event with Timeout.
WFI: Wait For Interrupt.
WFIT: Wait For Interrupt with Timeout.
XAFLAG: Convert floating-point condition flags from external format to Arm format.
XPACD, XPACI, XPACLRI: Strip Pointer Authentication Code.
YIELD: YIELD.
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---

## UXTH

**Description:** Unsigned Extend Halfword:

**Details:**
```
UXTH: Unsigned Extend Halfword: an alias of UBFM.
WFE: Wait For Event.
WFET: Wait For Event with Timeout.
WFI: Wait For Interrupt.
WFIT: Wait For Interrupt with Timeout.
XAFLAG: Convert floating-point condition flags from external format to Arm format.
XPACD, XPACI, XPACLRI: Strip Pointer Authentication Code.
YIELD: YIELD.
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---

## XAFLAG

**Description:** Convert floating-point condition flags from external format to Arm format

**Details:**
```
XAFLAG: Convert floating-point condition flags from external format to Arm format.
XPACD, XPACI, XPACLRI: Strip Pointer Authentication Code.
YIELD: YIELD.
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---

## Internal version only

**Description:** isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33

**Details:**
```
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---


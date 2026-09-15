# ARMv8-A Pointer-Authentication Instructions

Total: 11 instructions

---

## AUTDA, AUTDZA

**Description:** Authenticate Data address, using key A

**Details:**
```
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

## AUTDB, AUTDZB

**Description:** Authenticate Data address, using key B

**Details:**
```
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

## AUTIA, AUTIA1716, AUTIASP , AUTIAZ, AUTIZA

**Description:** Authenticate Instruction address, using key A

**Details:**
```
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

## AUTIB, AUTIB1716, AUTIBSP , AUTIBZ, AUTIZB

**Description:** Authenticate Instruction address, using key B

**Details:**
```
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

## BTI

**Description:** Branch Target Identification

**Details:**
```
BTI: Branch Target Identification.
CAS, CASA, CASAL, CASL: Compare and Swap word or doubleword in memory.
CASB, CASAB, CASALB, CASLB: Compare and Swap byte in memory.
CASH, CASAH, CASALH, CASLH: Compare and Swap halfword in memory.
CASP , CASPA, CASPAL, CASPL: Compare and Swap Pair of words or doublewords in memory.
CBNZ: Compare and Branch on Nonzero.
CBZ: Compare and Branch on Zero.
CCMN (immediate): Conditional Compare Negative (immediate).
CCMN (register): Conditional Compare Negative (register).
CCMP (immediate): Conditional Compare (immediate).
CCMP (register): Conditional Compare (register).
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

## PACDA, PACDZA

**Description:** Pointer Authentication Code for Data address, using key A

**Details:**
```
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

## PACDB, PACDZB

**Description:** Pointer Authentication Code for Data address, using key B

**Details:**
```
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

## PACGA

**Description:** Pointer Authentication Code, using Generic key

**Details:**
```
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

## PACIA, PACIA1716, PACIASP , PACIAZ, PACIZA

**Description:** Pointer Authentication Code for Instruction address, using key A

**Details:**
```
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

## PACIB, PACIB1716, PACIBSP , PACIBZ, PACIZB

**Description:** Pointer Authentication Code for Instruction address, using key B

**Details:**
```
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

## XPACD, XPACI, XPACLRI

**Description:** Strip Pointer Authentication Code

**Details:**
```
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


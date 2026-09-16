# ARMv8-A Barrier-Hint Instructions

Total: 16 instructions

---

## DMB

**Description:** Data Memory Barrier

**Details:**
```
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

## DSB

**Description:** Data Synchronization Barrier

**Details:**
```
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

## HINT

**Description:** Hint instruction

**Details:**
```
Hint.
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

## ISB

**Description:** Instruction Synchronization Barrier

**Details:**
```
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

## NOP

**Description:** No Operation

**Details:**
```
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

## PRFM (immediate)

**Description:** Prefetch Memory (immediate)

**Details:**
```
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

## PRFM (literal)

**Description:** Prefetch Memory (literal)

**Details:**
```
PRFM (literal): Prefetch Memory (literal).
PRFM (register): Prefetch Memory (register).
PRFUM: Prefetch Memory (unscaled offset).
A64 -- Base Instructions (alphabetic order)
Page 8
RETIRED

=
```

---

## PRFM (register)

**Description:** Prefetch Memory (register)

**Details:**
```
PRFM (register): Prefetch Memory (register).
PRFUM: Prefetch Memory (unscaled offset).
A64 -- Base Instructions (alphabetic order)
Page 8
RETIRED

=
```

---

## PRFUM

**Description:** Prefetch Memory (unscaled offset)

**Details:**
```
PRFUM: Prefetch Memory (unscaled offset).
A64 -- Base Instructions (alphabetic order)
Page 8
RETIRED

=
```

---

## SEV

**Description:** Send Event

**Details:**
```
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

## SEVL

**Description:** Send Event Local

**Details:**
```
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

## WFE

**Description:** Wait For Event

**Details:**
```
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

## WFET

**Description:** Wait For Event with Timeout

**Details:**
```
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

## WFI

**Description:** Wait For Interrupt

**Details:**
```
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

## WFIT

**Description:** Wait For Interrupt with Timeout

**Details:**
```
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

## YIELD

**Description:** YIELD

**Details:**
```
YIELD: YIELD.
Internal version only: isa v33.16decrel, AdvSIMD v29.05, pseudocode v2021-12_rel, sve v2021-12 ; Build timestamp: 2021-12-15T12:33
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
A64 -- Base Instructions (alphabetic order)
Page 13
RETIRED

=
```

---


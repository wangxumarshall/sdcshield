# ARMv8-A Branch Instructions

Total: 13 instructions

---

## B.cond

**Description:** Branch conditionally

**Details:**
```
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

## BL

**Description:** Branch with Link

**Details:**
```
BLE FOR ANY DAMAGES, 
INCLUDING WITHOUT LIMITA TION ANY DIRECT, INDIRECT, SPECIAL, INCIDENTAL, PUNITIVE, OR 
CONSEQUENTIAL DAMAGES, HOWEVER CAUSED AND REGARDLESS OF THE THEORY OF LIABILITY , ARISING 
OUT OF ANY USE OF THIS DOCUMENT, EVEN IF ARM HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH 
DAMAGES.
This document consists solely of commercial items. You shall be responsible for ensuring that any use, duplication or disclosure 
of this document complies fully with any relevant export laws and regulations to assure that this document or any portion thereof 
is not exported, directly or indirectly, in violation of such export laws. Use of the word “partner” in reference to Arm’s customers 
is not intended to create or refer to any partnership relationship with any other company. Arm may make changes to this document 
at any time and without notice.
This document may be translated into other languages for convenience, and you agree that if there is any conflict between the 
English version of this document and any translation, the terms of the English version of the Agreement shall prevail.
The Arm corporate logo and words marked with 
™ or © are registered trademarks or trademarks of Arm Limited (or its affiliates) 
in the US and/or elsewhere. All rights reserved. Other brands and names mentioned in this document may be the trademarks of 
their respective owners. You must follow the Arm’s trademark usage guidelines 
http://www.arm.com/company/policies/trademarks.
Copyright © 2010-2021 Arm Limited (or its affiliates). All rights reserved.
Arm Limited. Company 02557590 registered in England.
110 Fulbourn Road, Cambridge, England CB1 9NJ.
(LES-PRE-20349 version 21.0)
Confidentiality Status
This document is Non-Confidential. The right to use, copy and disclose this document may be subject to license restrictions in 
accordance with the terms of the agreement entered into by Arm and the party that Arm delivered this document to.
Product Status
This release covers multiple ver
```

---

## BLR

**Description:** Branch with Link to Register

**Details:**
```
BLR: Branch with Link to Register.
BLRAA, BLRAAZ, BLRAB, BLRABZ: Branch with Link to Register, with pointer authentication.
BR: Branch to Register.
BRAA, BRAAZ, BRAB, BRABZ: Branch to Register, with pointer authentication.
BRK: Breakpoint instruction.
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
CPYFPRN, CPYFMRN, CPYFERN: Memory Copy Forward-only, reads
```

---

## BLRAA, BLRAAZ, BLRAB, BLRABZ

**Description:** Branch with Link to Register, with pointer authentication

**Details:**
```
BLRAA, BLRAAZ, BLRAB, BLRABZ: Branch with Link to Register, with pointer authentication.
BR: Branch to Register.
BRAA, BRAAZ, BRAB, BRABZ: Branch to Register, with pointer authentication.
BRK: Breakpoint instruction.
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
CPYFPRT, CPYFMRT, CP
```

---

## BR

**Description:** Branch to Register

**Details:**
```
brands and names mentioned in this document may be the trademarks of 
their respective owners. You must follow the Arm’s trademark usage guidelines 
http://www.arm.com/company/policies/trademarks.
Copyright © 2010-2021 Arm Limited (or its affiliates). All rights reserved.
Arm Limited. Company 02557590 registered in England.
110 Fulbourn Road, Cambridge, England CB1 9NJ.
(LES-PRE-20349 version 21.0)
Confidentiality Status
This document is Non-Confidential. The right to use, copy and disclose this document may be subject to license restrictions in 
accordance with the terms of the agreement entered into by Arm and the party that Arm delivered this document to.
Product Status
This release covers multiple versions of the architecture. The content relating to different versions is given different quality ratings.
The information related to the 2021 Architecture Extensions is at Alpha quality. Alpha quality means that most major features of 
the specification are described in the manual, some features and details might be missing.
The information related to the remaining Armv8-A features which was also published in previous releases, is at Beta quality. Beta 
quality means that all major features of the specification are described, some details might be missing.
RETIRED

=
```

---

## BRAA, BRAAZ, BRAB, BRABZ

**Description:** Branch to Register, with pointer authentication

**Details:**
```
BRAA, BRAAZ, BRAB, BRABZ: Branch to Register, with pointer authentication.
BRK: Breakpoint instruction.
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


```

---

## BRK

**Description:** Breakpoint instruction

**Details:**
```
BRK: Breakpoint instruction.
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

## CBNZ

**Description:** Compare and Branch on Nonzero

**Details:**
```
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

## CBZ

**Description:** Compare and Branch on Zero

**Details:**
```
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

## RET

**Description:** Return from subroutine

**Details:**
```
RETIRED
This document is now RETIRED.
The Arm A-profile A64 Instruction Set Architecture 
(DDI0602) is the definitive reference for this 
architecture specification.

=
```

---

## RETAA, RETAB

**Description:** Return from subroutine, with pointer authentication

**Details:**
```
RETAA, ERETAB: Exception Return, with pointer authentication.
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

## TBNZ

**Description:** Test bit and Branch if Nonzero

**Details:**
```
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

## TBZ

**Description:** Test bit and Branch if Zero

**Details:**
```
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


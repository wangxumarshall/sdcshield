# DCPS3

## 分类
System

## 描述
Debug Change PE State to EL3

## 详细信息

```
326 ===
DCPS3
Debug Change PE State to EL3, when executed in Debug state:
• If executed at EL3 selects SP_EL3.
• Otherwise, changes the current Exception level and SP to EL3 using SP_EL3.
The target exception level of a DCPS3 instruction is EL3.
On executing a DCPS3 instruction:
• ELR_EL3 becomes UNKNOWN.
• SPSR_EL3 becomes UNKNOWN.
• ESR_EL3 becomes UNKNOWN.
• DLR_EL0 and DSPSR_EL0 become UNKNOWN.
• The endianness is set according to SCTLR_EL3.EE.
This instruction is UNDEFINED at all exception levels if either:
• EDSCR.SDD == 1.
• EL3 is not implemented.
This instruction is always UNDEFINED in Non-debug state.
For more information on the operation of the DCPS<n> instructions, see DCPS.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 1 0 1 imm16 0 0 0 1 1
LL
DCPS3 {#<imm>}
if !Halted() then UNDEFINED;
Assembler Symbols
<imm> Is an optional 16-bit unsigned immediate, in the range 0 to 65535, defaulting to 0 and encoded in the
"imm16" field.
Operation
DCPSInstruction(LL);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
DCPS3 Page 323
RETIRED


```

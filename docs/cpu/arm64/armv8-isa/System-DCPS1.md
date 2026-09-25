# DCPS1

## 分类
System

## 描述
Debug Change PE State to EL1

## 详细信息

```
324 ===
DCPS1
Debug Change PE State to EL1, when executed in Debug state:
• If executed at EL0 changes the current Exception level and SP to EL1 using SP_EL1.
• Otherwise, if executed at ELx, selects SP_ELx.
The target exception level of a DCPS1 instruction is:
• EL1 if the instruction is executed at EL0.
• Otherwise, the Exception level at which the instruction is executed.
When the target Exception level of a DCPS1 instruction is ELx, on executing this instruction:
•
ELR_ELx becomes UNKNOWN.
• SPSR_ELx becomes UNKNOWN.
• ESR_ELx becomes UNKNOWN.
• DLR_EL0 and DSPSR_EL0 become UNKNOWN.
• The endianness is set according to SCTLR_ELx.EE.
This instruction is UNDEFINED at EL0 in Non-secure state if EL2 is implemented and HCR_EL2.TGE == 1.
This instruction is always UNDEFINED in Non-debug state.
For more information on the operation of the DCPS<n> instructions, see DCPS.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 1 0 1 imm16 0 0 0 0 1
LL
DCPS1 {#<imm>}
if !Halted() then UNDEFINED;
Assembler Symbols
<imm> Is an optional 16-bit unsigned immediate, in the range 0 to 65535, defaulting to 0 and encoded in the
"imm16" field.
Operation
DCPSInstruction(LL);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
DCPS1 Page 321
RETIRED


```

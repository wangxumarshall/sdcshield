# MOV (bitmask immediate)

## 分类
Logical

## 描述
Move (bitmask immediate):

## 详细信息

```
522 ===
MOV (bitmask immediate)
Move (bitmask immediate) writes a bitmask immediate value to a register.
This is an alias of ORR (immediate). This means:
• The encodings in this description are named to match the encodings of ORR (immediate).
• The description of ORR (immediate) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 1 0 0 1 0 0 N immr imms 1 1 1 1 1 Rd
opc Rn
32-bit (sf == 0 && N == 0)
MOV <Wd|WSP>, #<imm>
is equivalent to
ORR <Wd|WSP>, WZR, #<imm>
and is the preferred disassembly when
! MoveWidePreferred(sf, N, imms, immr).
64-bit (sf == 1)
MOV <Xd|SP>, #<imm>
is equivalent to
ORR <Xd|SP>, XZR, #<imm>
and is the preferred disassembly when
! MoveWidePreferred(sf, N, imms, immr).
Assembler Symbols
<Wd|WSP> Is the 32-bit name of the destination general-purpose register or stack pointer, encoded in the "Rd"
field.
<Xd|SP> Is the 64-bit name of the destination general-purpose register or stack pointer, encoded in the "Rd"
field.
<imm> For the 32-bit variant: is the bitmask immediate, encoded in "imms:immr", but excluding values which
could be encoded by MOVZ or MOVN.
For the 64-bit variant: is the bitmask immediate, encoded in "N:imms:immr", but excluding values which
could be encoded by MOVZ or MOVN.
Operation
The description of ORR (immediate) gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
MOV (bitmask immediate) Page 519
RETIRED


```

# MOV (wide immediate)

## 分类
Logical

## 描述
Move (wide immediate):

## 详细信息

```
527 ===
MOV (wide immediate)
Move (wide immediate) moves a 16-bit immediate value to a register.
This is an alias of MOVZ. This means:
• The encodings in this description are named to match the encodings of MOVZ.
• The description of MOVZ gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 0 0 1 0 1 hw imm16 Rd
opc
32-bit (sf == 0 && hw == 0x)
MOV <Wd>, #<imm>
is equivalent to
MOVZ <Wd>, #<imm16>, LSL #<shift>
and is the preferred disassembly when
! (IsZero(imm16) && hw != '00').
64-bit (sf == 1)
MOV <Xd>, #<imm>
is equivalent to
MOVZ <Xd>, #<imm16>, LSL #<shift>
and is the preferred disassembly when
! (IsZero(imm16) && hw != '00').
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<imm> For the 32-bit variant: is a 32-bit immediate which can be encoded in "imm16:hw".
For the 64-bit variant: is a 64-bit immediate which can be encoded in "imm16:hw".
<shift> For the 32-bit variant: is the amount by which to shift the immediate left, either 0 (the default) or 16,
encoded in the "hw" field as <shift>/16.
For the 64-bit variant: is the amount by which to shift the immediate left, either 0 (the default), 16, 32
or 48, encoded in the "hw" field as <shift>/16.
Operation
The description of MOVZ
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
MOV (wide immediate) Page 524
RETIRED


```

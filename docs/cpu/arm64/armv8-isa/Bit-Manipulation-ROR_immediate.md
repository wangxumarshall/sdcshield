# ROR (immediate)

## 分类
Bit-Manipulation

## 描述
Rotate right (immediate):

## 详细信息

```
583 ===
ROR (immediate)
Rotate right (immediate) provides the value of the contents of a register rotated by a variable number of bits. The bits
that are rotated off the right end are inserted into the vacated bit positions on the left.
This is an alias of EXTR. This means:
• The encodings in this description are named to match the encodings of EXTR.
• The description of EXTR gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 0 0 1 1 1 N 0 Rm imms Rn Rd
32-bit (sf == 0 && N == 0 && imms == 0xxxxx)
ROR <Wd>, <Ws>, #<shift>
is equivalent to
EXTR <Wd>, <Ws>, <Ws>, #<shift>
and is the preferred disassembly when
Rn == Rm.
64-bit (sf == 1 && N == 1)
ROR <Xd>, <Xs>, #<shift>
is equivalent to
EXTR <Xd>, <Xs>, <Xs>, #<shift>
and is the preferred disassembly when
Rn == Rm.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Ws> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" and "Rm" fields.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xs> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" and "Rm" fields.
<shift> For the 32-bit variant: is the amount by which to rotate, in the range 0 to 31, encoded in the "imms"
field.
For the 64-bit variant: is the amount by which to rotate, in the range 0 to 63, encoded in the "imms"
field.
Operation
The description of EXTR
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
ROR (immediate) Page 580
RETIRED


```

# MOV (to/from SP)

## 分类
Logical

## 描述
Move between register and stack pointer:

## 详细信息

```
526 ===
MOV (to/from SP)
Move between register and stack pointer
: Rd = Rn.
This is an alias of ADD (immediate). This means:
• The encodings in this description are named to match the encodings of ADD (immediate).
• The description of ADD (immediate) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 0 0 0 1 0 0 0 0 0 0 0 0 0 0 0 0 0 0 Rn Rd
op S sh imm12
32-bit (sf == 0)
MOV <Wd|WSP>, <Wn|WSP>
is equivalent to
ADD <Wd|WSP>, <Wn|WSP>, #0
and is the preferred disassembly when
(Rd == '11111' || Rn == '11111').
64-bit (sf == 1)
MOV <Xd|SP>, <Xn|SP>
is equivalent to
ADD <Xd|SP>, <Xn|SP>, #0
and is the preferred disassembly when
(Rd == '11111' || Rn == '11111').
Assembler Symbols
<Wd|WSP> Is the 32-bit name of the destination general-purpose register or stack pointer, encoded in the "Rd"
field.
<Wn|WSP> Is the 32-bit name of the source general-purpose register or stack pointer, encoded in the "Rn" field.
<Xd|SP> Is the 64-bit name of the destination general-purpose register or stack pointer, encoded in the "Rd"
field.
<Xn|SP> Is the 64-bit name of the source general-purpose register or stack pointer, encoded in the "Rn" field.
Operation
The description of ADD (immediate) gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
MOV (to/from SP) Page 523
RETIRED


```

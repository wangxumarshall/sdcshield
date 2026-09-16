# TST (immediate)

## 分类
Logical

## 描述
Test bits (immediate):

## 详细信息

```
790 ===
TST (immediate)
Test bits (immediate), setting the condition flags and discarding the result
: Rn AND imm.
This is an alias of ANDS (immediate). This means:
• The encodings in this description are named to match the encodings of ANDS (immediate).
• The description of ANDS (immediate) gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 1 0 0 1 0 0 N immr imms Rn 1 1 1 1 1
opc Rd
32-bit (sf == 0 && N == 0)
TST <Wn>, #<imm>
is equivalent to
ANDS WZR, <Wn>, #<imm>
and is always the preferred disassembly.
64-bit (sf == 1)
TST <Xn>, #<imm>
is equivalent to
ANDS XZR, <Xn>, #<imm>
and is always the preferred disassembly.
Assembler Symbols
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<imm> For the 32-bit variant: is the bitmask immediate, encoded in "imms:immr".
For the 64-bit variant: is the bitmask immediate, encoded in "N:imms:immr".
Operation
The description of ANDS (immediate)
gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
TST (immediate) Page 787
RETIRED


```

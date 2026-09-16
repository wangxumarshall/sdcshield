# MUL

## 分类
Arithmetic

## 描述
Multiply:

## 详细信息

```
540 ===
MUL
Multiply
: Rd = Rn * Rm.
This is an alias of MADD. This means:
• The encodings in this description are named to match the encodings of MADD.
• The description of MADD gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 1 0 0 0 Rm 0 1 1 1 1 1 Rn Rd
o0 Ra
32-bit (sf == 0)
MUL <Wd>, <Wn>, <Wm>
is equivalent to
MADD <Wd>, <Wn>, <Wm>, WZR
and is always the preferred disassembly.
64-bit (sf == 1)
MUL <Xd>, <Xn>, <Xm>
is equivalent to
MADD <Xd>, <Xn>, <Xm>, XZR
and is always the preferred disassembly.
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register holding the multiplicand, encoded in the
"Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register holding the multiplier, encoded in the
"Rm" field.
Operation
The description of MADD
gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
MUL Page 537
RETIRED


```

# CMPP

## 分类
Compare

## 描述
Compare with Tag:

## 详细信息

```
113 ===
CMPP
Compare with Tag subtracts the 56-bit address held in the second source register from the 56-bit address held in the
first source register, updates the condition flags based on the result of the subtraction, and discards the result.
This is an alias of SUBPS. This means:
• The encodings in this description are named to match the encodings of SUBPS.
• The description of SUBPS gives the operational pseudocode for this instruction.
Integer
(Armv8.5)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 1 1 1 0 1 0 1 1 0 Xm 0 0 0 0 0 0 Xn 1 1 1 1 1
Xd
CMPP <Xn|SP>, <Xm|SP>
is equivalent to
SUBPS XZR, <Xn|SP>, <Xm|SP>
and is always the preferred disassembly.
Assembler Symbols
<Xn|SP> Is the 64-bit name of the first source general-purpose register or stack pointer, encoded in the "Xn"
field.
<Xm|SP> Is the 64-bit name of the second general-purpose source register or stack pointer, encoded in the "Xm"
field.
Operation
The description of SUBPS gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CMPP Page 110
RETIRED


```

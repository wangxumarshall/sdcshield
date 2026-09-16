# CBNZ

## 分类
Branch

## 描述
Compare and Branch on Nonzero

## 详细信息

```
88 ===
CBNZ
Compare and Branch on Nonzero compares the value in a register with zero, and conditionally branches to a label at a
PC-relative offset if the comparison is not equal. It provides a hint that this is not a subroutine call or return. This
instruction does not affect the condition flags.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 1 0 1 0 1 imm19 Rt
op
32-bit (sf == 0)
CBNZ <Wt>, <label>
64-bit (sf == 1)
CBNZ <Xt>, <label>
integer t = UInt
(Rt);
integer datasize = if sf == '1' then 64 else 32;
bits(64) offset = SignExtend
(imm19:'00', 64);
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be tested, encoded in the "Rt" field.
<Xt> Is the 64-bit name of the general-purpose register to be tested, encoded in the "Rt" field.
<label> Is the program label to be conditionally branched to. Its offset from the address of this instruction, in
the range +/-1MB, is encoded as "imm19" times 4.
Operation
bits(datasize) operand1 = X
[t];
if IsZero(operand1) == FALSE then
BranchTo(PC[] + offset, BranchType_DIR, TRUE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CBNZ Page 85
RETIRED


```

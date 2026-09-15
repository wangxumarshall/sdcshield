# BL

## 分类
Branch

## 描述
Branch with Link

## 详细信息

```
68 ===
BL
Branch with Link branches to a PC-relative offset, setting the register X30 to PC+4. It provides a hint that this is a
subroutine call.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 0 1 imm26
op
BL <label>
bits(64) offset = SignExtend(imm26:'00', 64);
Assembler Symbols
<label> Is the program label to be unconditionally branched to. Its offset from the address of this instruction, in
the range +/-128MB, is encoded as "imm26" times 4.
Operation
X[30] = PC[] + 4;
BranchTo(PC[] + offset, BranchType_DIRCALL, FALSE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
BL Page 65
RETIRED


```

# TBNZ

## 分类
Branch

## 描述
Test bit and Branch if Nonzero

## 详细信息

```
784 ===
TBNZ
Test bit and Branch if Nonzero compares the value of a bit in a general-purpose register with zero, and conditionally
branches to a label at a PC-relative offset if the comparison is not equal. It provides a hint that this is not a subroutine
call or return. This instruction does not affect condition flags.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
b5 0 1 1 0 1 1 1 b40 imm14 Rt
op
TBNZ <R><t>, #<imm>, <label>
integer t = UInt
(Rt);
integer datasize = if b5 == '1' then 64 else 32;
integer bit_pos = UInt
(b5:b40);
bits(64) offset = SignExtend(imm14:'00', 64);
Assembler Symbols
<R> Is a width specifier, encoded in “b5”:
b5 <R>
0 W
1 X
In assembler source code an 'X' specifier is always permitted, but a 'W' specifier is only permitted when
the bit number is less than 32.
<t> Is the number [0-30] of the general-purpose register to be tested or the name ZR (31), encoded in the
"Rt" field.
<imm> Is the bit number to be tested, in the range 0 to 63, encoded in "b5:b40".
<label> Is the program label to be conditionally branched to. Its offset from the address of this instruction, in
the range +/-32KB, is encoded as "imm14" times 4.
Operation
bits(datasize) operand = X
[t];
if operand<bit_pos> == op then
BranchTo(PC[] + offset, BranchType_DIR, TRUE);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
TBNZ Page 781
RETIRED


```

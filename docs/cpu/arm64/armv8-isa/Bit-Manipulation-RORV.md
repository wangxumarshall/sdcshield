# RORV

## 分类
Bit-Manipulation

## 描述
Rotate Right Variable

## 详细信息

```
586 ===
RORV
Rotate Right Variable provides the value of the contents of a register rotated by a variable number of bits. The bits
that are rotated off the right end are inserted into the vacated bit positions on the left. The remainder obtained by
dividing the second source register by the data size defines the number of bits by which the first source register is
right-shifted.
This instruction is used by the alias ROR (register)
.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 0 1 1 0 Rm 0 0 1 0 1 1 Rn Rd
op2
32-bit (sf == 0)
RORV <Wd>, <Wn>, <Wm>
64-bit (sf == 1)
RORV <Xd>, <Xn>, <Xm>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
ShiftType shift_type = DecodeShift(op2);
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register holding a shift amount from 0 to 31 in
its bottom 5 bits, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register holding a shift amount from 0 to 63 in
its bottom 6 bits, encoded in the "Rm" field.
Operation
bits(datasize) result;
bits(datasize) operand2 = X
[m];
result = ShiftReg(n, shift_type, UInt(operand2) MOD datasize);
X[d] = result;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
RORV Page 583
RETIRED


```

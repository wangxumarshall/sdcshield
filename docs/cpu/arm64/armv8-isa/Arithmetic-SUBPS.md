# SUBPS

## 分类
Arithmetic

## 描述
Subtract Pointer, setting Flags

## 详细信息

```
764 ===
SUBPS
Subtract Pointer, setting Flags subtracts the 56-bit address held in the second source register from the 56-bit address
held in the first source register, sign-extends the result to 64-bits, and writes the result to the destination register. It
updates the condition flags based on the result of the subtraction.
This instruction is used by the alias CMPP
.
Integer
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 1 1 1 0 1 0 1 1 0 Xm 0 0 0 0 0 0 Xn Xd
SUBPS <Xd>, <Xn|SP>, <Xm|SP>
if !HaveMTEExt() then UNDEFINED;
integer d = UInt(Xd);
integer n = UInt(Xn);
integer m = UInt(Xm);
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Xd" field.
<Xn|SP> Is the 64-bit name of the first source general-purpose register or stack pointer, encoded in the "Xn"
field.
<Xm|SP> Is the 64-bit name of the second general-purpose source register or stack pointer, encoded in the "Xm"
field.
Alias Conditions
Alias Is preferred when
CMPP S == '1' && Xd == '11111'
Operation
bits(64) operand1 = if n == 31 then SP[] else X[n];
bits(64) operand2 = if m == 31 then SP[] else X[m];
operand1 = SignExtend(operand1<55:0>, 64);
operand2 = SignExtend(operand2<55:0>, 64);
bits(64) result;
bits(4) nzcv;
operand2 = NOT(operand2);
(result, nzcv) = AddWithCarry(operand1, operand2, '1');
PSTATE.<N,Z,C,V> = nzcv;
X
[d] = result;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SUBPS Page 761
RETIRED


```

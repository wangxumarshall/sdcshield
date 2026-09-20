# PACGA

## 分类
Pointer-Authentication

## 描述
Pointer Authentication Code, using Generic key

## 详细信息

```
558 ===
PACGA
Pointer Authentication Code, using Generic key. This instruction computes the pointer authentication code for an
address in the first source register, using a modifier in the second source register, and the Generic key. The computed
pointer authentication code is returned in the upper 32 bits of the destination register.
Integer
(FEAT_PAuth)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 1 0 1 0 1 1 0 Rm 0 0 1 1 0 0 Rn Rd
PACGA <Xd>, <Xn>, <Xm|SP>
boolean source_is_sp = FALSE;
integer d = UInt
(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
if !HavePACExt() then
UNDEFINED;
if m == 31 then source_is_sp = TRUE;
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm|SP> Is the 64-bit name of the second general-purpose source register or stack pointer, encoded in the "Rm"
field.
Operation
if source_is_sp then
X
[d] = AddPACGA(X[n], SP[]);
else
X[d] = AddPACGA(X[n], X[m]);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
PACGA Page 555
RETIRED


```

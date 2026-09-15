# RMIF

## 分类
Other

## 描述
Rotate, Mask Insert Flags

## 详细信息

```
582 ===
RMIF
Performs a rotation right of a value held in a general purpose register by an immediate value, and then inserts a
selection of the bottom four bits of the result of the rotation into the PSTATE flags, under the control of a second
immediate mask.
Integer
(FEAT_FlagM)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 1 1 1 0 1 0 0 0 0 imm6 0 0 0 0 1 Rn 0 mask
sf
RMIF <Xn>, #<shift>, #<mask>
if !HaveFlagManipulateExt
() then UNDEFINED;
integer lsb = UInt(imm6);
integer n = UInt(Rn);
Assembler Symbols
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<shift> Is the shift amount, in the range 0 to 63, defaulting to 0 and encoded in the "imm6" field,
<mask> Is the flag bit mask, an immediate in the range 0 to 15, which selects the bits that are inserted into the
NZCV condition flags, encoded in the "mask" field.
Operation
bits(4) tmp;
bits(64) tmpreg = X
[n];
tmp = (tmpreg:tmpreg)<lsb+3:lsb>;
if mask<3> == '1' then PSTATE.N = tmp<3>;
if mask<2> == '1' then PSTATE.Z = tmp<2>;
if mask<1> == '1' then PSTATE.C = tmp<1>;
if mask<0> == '1' then PSTATE.V = tmp<0>;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
RMIF Page 579
RETIRED


```

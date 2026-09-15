# CSEL

## 分类
Conditional-Select

## 描述
Conditional Select

## 详细信息

```
313 ===
CSEL
If the condition is true, Conditional Select writes the value of the first source register to the destination register. If the
condition is false, it writes the value of the second source register to the destination register.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 1 0 1 0 1 0 0 Rm cond 0 0 Rn Rd
op o2
32-bit (sf == 0)
CSEL <Wd>, <Wn>, <Wm>, <cond>
64-bit (sf == 1)
CSEL <Xd>, <Xn>, <Xm>, <cond>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Rm" field.
<cond> Is one of the standard conditions, encoded in the "cond" field in the standard way.
Operation
bits(datasize) result;
if ConditionHolds
(cond) then
result = X[n];
else
result = X[m];
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
CSEL Page 310
RETIRED


```

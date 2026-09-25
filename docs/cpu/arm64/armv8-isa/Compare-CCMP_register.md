# CCMP (register)

## 分类
Compare

## 描述
Conditional Compare (register)

## 详细信息

```
93 ===
CCMP (register)
Conditional Compare (register) sets the value of the condition flags to the result of the comparison of two registers if
the condition is TRUE, and an immediate value otherwise.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 1 1 0 1 0 0 1 0 Rm cond 0 0 Rn 0 nzcv
op
32-bit (sf == 0)
CCMP <Wn>, <Wm>, #<nzcv>, <cond>
64-bit (sf == 1)
CCMP <Xn>, <Xm>, #<nzcv>, <cond>
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
bits(4) flags = nzcv;
Assembler Symbols
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Rm" field.
<nzcv> Is the flag bit specifier, an immediate in the range 0 to 15, giving the alternative state for the 4-bit
NZCV condition flags, encoded in the "nzcv" field.
<cond> Is one of the standard conditions, encoded in the "cond" field in the standard way.
Operation
if ConditionHolds
(cond) then
bits(datasize) operand1 = X[n];
bits(datasize) operand2 = X[m];
operand2 = NOT(operand2);
(-, flags) = AddWithCarry(operand1, operand2, '1');
PSTATE.<N,Z,C,V> = flags;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CCMP (register) Page 90
RETIRED


```

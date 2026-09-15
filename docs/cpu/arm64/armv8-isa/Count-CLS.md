# CLS

## 分类
Count

## 描述
Count Leading Sign bits

## 详细信息

```
99 ===
CLS
Count Leading Sign bits counts the number of leading bits of the source register that have the same value as the most
significant bit of the register, and writes the result to the destination register. This count does not include the most
significant bit of the source register.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 1 0 1 0 1 1 0 0 0 0 0 0 0 0 0 1 0 1 Rn Rd
op
32-bit (sf == 0)
CLS <Wd>, <Wn>
64-bit (sf == 1)
CLS <Xd>, <Xn>
integer d = UInt
(Rd);
integer n = UInt(Rn);
integer datasize = if sf == '1' then 64 else 32;
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
Operation
integer result;
bits(datasize) operand1 = X
[n];
result = CountLeadingSignBits(operand1);
X[d] = result<datasize-1:0>;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CLS Page 96
RETIRED


```

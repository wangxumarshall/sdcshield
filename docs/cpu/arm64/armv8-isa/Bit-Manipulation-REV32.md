# REV32

## 分类
Bit-Manipulation

## 描述
Reverse bytes in 32-bit words

## 详细信息

```
580 ===
REV32
Reverse bytes in 32-bit words reverses the byte order in each 32-bit word of a register.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 1 0 1 0 1 1 0 0 0 0 0 0 0 0 0 0 1 0 Rn Rd
sf opc
REV32 <Xd>, <Xn>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer datasize = if sf == '1' then 64 else 32;
integer container_size;
case opc of
when '00'
Unreachable();
when '01'
container_size = 16;
when '10'
container_size = 32;
when '11'
if sf == '0' then UNDEFINED;
container_size = 64;
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
Operation
bits(datasize) operand = X
[n];
bits(datasize) result;
integer containers = datasize DIV container_size;
integer elements_per_container = container_size DIV 8;
integer index = 0;
integer rev_index;
for c = 0 to containers-1
rev_index = index + ((elements_per_container - 1) * 8);
for e = 0 to elements_per_container-1
result<rev_index+7:rev_index> = operand<index+7:index>;
index = index + 8;
rev_index = rev_index - 8;
X
[d] = result;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
REV32 Page 577
RETIRED


```

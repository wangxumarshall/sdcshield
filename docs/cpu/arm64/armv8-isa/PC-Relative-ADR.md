# ADR

## 分类
PC-Relative

## 描述
Form PC-relative address

## 详细信息

```
32 ===
ADR
Form PC-relative address adds an immediate value to the PC value to form a PC-relative address, and writes the result
to the destination register.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 immlo 1 0 0 0 0 immhi Rd
op
ADR <Xd>, <label>
integer d = UInt(Rd);
bits(64) imm;
imm = SignExtend(immhi:immlo, 64);
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<label> Is the program label whose address is to be calculated. Its offset from the address of this instruction, in
the range +/-1MB, is encoded in "immhi:immlo".
Operation
bits(64) base = PC[];
X[d] = base + imm;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
ADR Page 29
RETIRED


```

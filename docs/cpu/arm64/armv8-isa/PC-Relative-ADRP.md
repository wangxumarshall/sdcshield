# ADRP

## 分类
PC-Relative

## 描述
Form PC-relative address to 4KB page

## 详细信息

```
33 ===
ADRP
Form PC-relative address to 4KB page adds an immediate value that is shifted left by 12 bits, to the PC value to form a
PC-relative address, with the bottom 12 bits masked out, and writes the result to the destination register.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 immlo 1 0 0 0 0 immhi Rd
op
ADRP <Xd>, <label>
integer d = UInt(Rd);
bits(64) imm;
imm = SignExtend(immhi:immlo:Zeros(12), 64);
Assembler Symbols
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<label> Is the program label whose 4KB page address is to be calculated. Its offset from the page address of
this instruction, in the range +/-4GB, is encoded as "immhi:immlo" times 4096.
Operation
bits(64) base = PC[];
base<11:0> = Zeros(12);
X[d] = base + imm;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
ADRP Page 30
RETIRED


```

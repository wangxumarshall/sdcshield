# LDRSW (literal)

## 分类
Load-Store

## 描述
Load Register Signed Word (literal)

## 详细信息

```
445 ===
LDRSW (literal)
Load Register Signed Word (literal) calculates an address from the PC value and an immediate offset, loads a word
from memory, and writes it to a register. For information about memory accesses, seeLoad/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 0 1 1 0 0 0 imm19 Rt
opc
LDRSW <Xt>, <label>
integer t = UInt(Rt);
bits(64) offset;
offset = SignExtend(imm19:'00', 64);
Assembler Symbols
<Xt> Is the 64-bit name of the general-purpose register to be loaded, encoded in the "Rt" field.
<label> Is the program label from which the data is to be loaded. Its offset from the address of this instruction,
in the range +/-1MB, is encoded as "imm19" times 4.
Operation
bits(64) address = PC[] + offset;
bits(32) data;
if HaveMTE2Ext() then
SetTagCheckedInstruction(FALSE);
data = Mem[address, 4, AccType_NORMAL];
X[t] = SignExtend(data, 64);
Operational information
If PSTATE.DIT is 1, the timing of this instruction is insensitive to the value of the data being loaded or stored.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
LDRSW (literal) Page 442
RETIRED


```

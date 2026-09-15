# LDXRB

## 分类
Load-Store

## 描述
Load Exclusive Register Byte

## 详细信息

```
508 ===
LDXRB
Load Exclusive Register Byte derives an address from a base register value, loads a byte from memory, zero-extends it
and writes it to a register. The memory access is atomic. The PE marks the physical address being accessed as an
exclusive access. This exclusive access mark is checked by Store Exclusive instructions. See
Synchronization and
semaphores. For information about memory accesses see Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 0 0 0 1 0 0 0 0 1 0 (1) (1) (1) (1) (1) 0 (1) (1) (1) (1) (1) Rn Rt
size L Rs o0 Rt2
LDXRB <Wt>, [<Xn|SP>{,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
boolean tag_checked = n != 31;
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
Operation
bits(64) address;
bits(8) data;
if HaveMTE2Ext() then
SetTagCheckedInstruction(tag_checked);
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
// Tell the Exclusives monitors to record a sequence of one or more atomic
// memory reads from virtual address range [address, address+dbytes-1].
// The Exclusives monitor will only be set if all the reads are from the
// same dbytes-aligned physical address, to allow for the possibility of
// an atomicity break if the translation is changed between reads.
AArch64.SetExclusiveMonitors
(address, 1);
data = Mem[address, 1, AccType_ATOMIC];
X[t] = ZeroExtend(data, 32);
Operational information
If PSTATE.DIT is 1, the timing of this instruction is insensitive to the value of the data being loaded or stored.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
LDXRB Page 505
RETIRED


```

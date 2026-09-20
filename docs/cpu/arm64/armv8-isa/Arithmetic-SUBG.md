# SUBG

## 分类
Arithmetic

## 描述
Subtract with Tag

## 详细信息

```
762 ===
SUBG
Subtract with Tag subtracts an immediate value scaled by the Tag granule from the address in the source register,
modifies the Logical Address Tag of the address using an immediate value, and writes the result to the destination
register. Tags specified in GCR_EL1.Exclude are excluded from the possible outputs when modifying the Logical
Address Tag.
Integer
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 0 0 1 1 0 uimm6 (0) (0) uimm4 Xn Xd
op3
SUBG <Xd|SP>, <Xn|SP>, #<uimm6>, #<uimm4>
if !HaveMTEExt
() then UNDEFINED;
integer d = UInt(Xd);
integer n = UInt(Xn);
bits(64) offset = LSL(ZeroExtend(uimm6, 64), LOG2_TAG_GRANULE);
Assembler Symbols
<Xd|SP> Is the 64-bit name of the destination general-purpose register or stack pointer, encoded in the "Xd"
field.
<Xn|SP> Is the 64-bit name of the source general-purpose register or stack pointer, encoded in the "Xn" field.
<uimm6> Is an unsigned immediate, a multiple of 16 in the range 0 to 1008, encoded in the "uimm6" field.
<uimm4> Is an unsigned immediate, in the range 0 to 15, encoded in the "uimm4" field.
Operation
bits(64) operand1 = if n == 31 then SP
[] else X[n];
bits(4) start_tag = AArch64.AllocationTagFromAddress(operand1);
bits(16) exclude = GCR_EL1.Exclude;
bits(64) result;
bits(4) rtag;
if AArch64.AllocationTagAccessIsEnabled
(AccType_NORMAL) then
rtag = AArch64.ChooseNonExcludedTag(start_tag, uimm4, exclude);
else
rtag = '0000';
(result, -) = AddWithCarry(operand1, NOT(offset), '1');
result = AArch64.AddressWithAllocationTag(result, AccType_NORMAL, rtag);
if d == 31 then
SP[] = result;
else
X[d] = result;
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SUBG Page 759
RETIRED


```

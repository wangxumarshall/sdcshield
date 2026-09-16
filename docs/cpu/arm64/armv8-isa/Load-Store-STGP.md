# STGP

## 分类
Load-Store

## 描述
Store Allocation Tag and Pair of registers

## 详细信息

```
675 ===
STGP
Store Allocation Tag and Pair of registers stores an Allocation Tag and two 64-bit doublewords to memory, from two
registers. The address used for the store is calculated from the base register and an immediate signed offset scaled by
the Tag granule. The Allocation Tag is calculated from the Logical Address Tag in the base register.
This instruction generates an Unchecked access.
It has encodings from 3 classes: Post-index
, Pre-index and Signed offset
Post-index
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 0 1 0 0 0 1 0 simm7 Xt2 Xn Xt
STGP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>
if !HaveMTEExt() then UNDEFINED;
integer n = UInt(Xn);
integer t = UInt(Xt);
integer t2 = UInt(Xt2);
bits(64) offset = LSL(SignExtend(simm7, 64), LOG2_TAG_GRANULE);
boolean writeback = TRUE;
boolean postindex = TRUE;
Pre-index
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 0 1 0 0 1 1 0 simm7 Xt2 Xn Xt
STGP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!
if !HaveMTEExt() then UNDEFINED;
integer n = UInt(Xn);
integer t = UInt(Xt);
integer t2 = UInt(Xt2);
bits(64) offset = LSL(SignExtend(simm7, 64), LOG2_TAG_GRANULE);
boolean writeback = TRUE;
boolean postindex = FALSE;
Signed offset
(FEAT_MTE)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 0 1 0 0 1 0 0 simm7 Xt2 Xn Xt
STGP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
if !HaveMTEExt() then UNDEFINED;
integer n = UInt(Xn);
integer t = UInt(Xt);
integer t2 = UInt(Xt2);
bits(64) offset = LSL(SignExtend(simm7, 64), LOG2_TAG_GRANULE);
boolean writeback = FALSE;
boolean postindex = FALSE;
Assembler Symbols
<Xt1> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Xt" field.
STGP Page 672
RETIRED


```

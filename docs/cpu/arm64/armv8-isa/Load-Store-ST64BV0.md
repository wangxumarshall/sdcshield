# ST64BV0

## 分类
Load-Store

## 描述
Single-copy Atomic 64-byte EL0 Store with Return

## 详细信息

```
658 ===
ST64BV0
Single-copy Atomic 64-byte EL0 Store with Return stores eight 64-bit doublewords from consecutive registers, Xt to
X(t+7), to a memory location, with the bottom 32 bits taken from ACCDATA_EL1, and writes the status result of the
store to a register. The data that is stored is atomic and is required to be 64-byte aligned.
Integer
(FEAT_LS64_ACCDATA)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 1 1 1 0 0 0 0 0 1 Rs 1 0 1 0 0 0 Rn Rt
ST64BV0 <Xs>, <Xt>, [<Xn|SP>]
if !HaveFeatLS64_ACCDATA() then UNDEFINED;
if Rt<4:3> == '11' || Rt<0> == '1' then UNDEFINED;
integer n = UInt(Rn);
integer t = UInt(Rt);
integer s = UInt(Rs);
boolean tag_checked = n != 31;
Assembler Symbols
<Xs> Is the 64-bit name of the general-purpose register into which the status result of this instruction is
written, encoded in the "Rs" field.
The value returned is:
0xFFFFFFFF_FFFFFFFF
If the memory location accessed does not support this instruction. In this case, the value at the
memory location is UNKNOWN.
!= 0xFFFFFFFF_FFFFFFFF
If the memory location accessed does support this instruction. In this case, the peripheral that
provides the response defines the returned value and provides information on the state of the
memory update at the memory location.
If XZR is used, then the return value is ignored.
<Xt> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
ST64BV0 Page 655
RETIRED


```

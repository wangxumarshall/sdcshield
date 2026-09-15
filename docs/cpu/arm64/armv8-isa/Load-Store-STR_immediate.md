# STR (immediate)

## 分类
Load-Store

## 描述
Store Register (immediate)

## 详细信息

```
700 ===
STR (immediate)
Store Register (immediate) stores a word or a doubleword from a register to memory. The address that is used for the
store is calculated from a base register and an immediate offset. For information about memory accesses, see Load/
Store addressing modes.
It has encodings from 3 classes: Post-index , Pre-index and Unsigned offset
Post-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 0 0 0 0 imm9 0 1 Rn Rt
size opc
32-bit (size == 10)
STR <Wt>, [<Xn|SP>], #<simm>
64-bit (size == 11)
STR <Xt>, [<Xn|SP>], #<simm>
boolean wback = TRUE;
boolean postindex = TRUE;
integer scale = UInt
(size);
bits(64) offset = SignExtend(imm9, 64);
Pre-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 0 0 0 0 imm9 1 1 Rn Rt
size opc
32-bit (size == 10)
STR <Wt>, [<Xn|SP>, #<simm>]!
64-bit (size == 11)
STR <Xt>, [<Xn|SP>, #<simm>]!
boolean wback = TRUE;
boolean postindex = FALSE;
integer scale = UInt
(size);
bits(64) offset = SignExtend(imm9, 64);
Unsigned offset
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 1 0 0 imm12 Rn Rt
size opc
STR (immediate) Page 697
RETIRED


```

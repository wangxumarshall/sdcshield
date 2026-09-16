# LDR (immediate)

## 分类
Load-Store

## 描述
Load Register (immediate)

## 详细信息

```
414 ===
LDR (immediate)
Load Register (immediate) loads a word or doubleword from memory and writes it to a register. The address that is
used for the load is calculated from a base register and an immediate offset. For information about memory accesses,
see
Load/Store addressing modes. The Unsigned offset variant scales the immediate offset value by the size of the
value accessed before adding it to the base register value.
It has encodings from 3 classes: Post-index , Pre-index and Unsigned offset
Post-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 0 0 1 0 imm9 0 1 Rn Rt
size opc
32-bit (size == 10)
LDR <Wt>, [<Xn|SP>], #<simm>
64-bit (size == 11)
LDR <Xt>, [<Xn|SP>], #<simm>
boolean wback = TRUE;
boolean postindex = TRUE;
integer scale = UInt
(size);
bits(64) offset = SignExtend(imm9, 64);
Pre-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 0 0 1 0 imm9 1 1 Rn Rt
size opc
32-bit (size == 10)
LDR <Wt>, [<Xn|SP>, #<simm>]!
64-bit (size == 11)
LDR <Xt>, [<Xn|SP>, #<simm>]!
boolean wback = TRUE;
boolean postindex = FALSE;
integer scale = UInt
(size);
bits(64) offset = SignExtend(imm9, 64);
Unsigned offset
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 1 0 1 imm12 Rn Rt
size opc
LDR (immediate) Page 411
RETIRED


```

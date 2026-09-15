# LDRSH (immediate)

## 分类
Load-Store

## 描述
Load Register Signed Halfword (immediate)

## 详细信息

```
437 ===
LDRSH (immediate)
Load Register Signed Halfword (immediate) loads a halfword from memory, sign-extends it to 32 bits or 64 bits, and
writes the result to a register. The address that is used for the load is calculated from a base register and an
immediate offset. For information about memory accesses, see
Load/Store addressing modes.
It has encodings from 3 classes: Post-index , Pre-index and Unsigned offset
Post-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 1 1 0 0 0 1 x 0 imm9 0 1 Rn Rt
size opc
32-bit (opc == 11)
LDRSH <Wt>, [<Xn|SP>], #<simm>
64-bit (opc == 10)
LDRSH <Xt>, [<Xn|SP>], #<simm>
boolean wback = TRUE;
boolean postindex = TRUE;
bits(64) offset = SignExtend
(imm9, 64);
Pre-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 1 1 0 0 0 1 x 0 imm9 1 1 Rn Rt
size opc
32-bit (opc == 11)
LDRSH <Wt>, [<Xn|SP>, #<simm>]!
64-bit (opc == 10)
LDRSH <Xt>, [<Xn|SP>, #<simm>]!
boolean wback = TRUE;
boolean postindex = FALSE;
bits(64) offset = SignExtend
(imm9, 64);
Unsigned offset
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 1 1 0 0 1 1 x imm12 Rn Rt
size opc
LDRSH (immediate) Page 434
RETIRED


```

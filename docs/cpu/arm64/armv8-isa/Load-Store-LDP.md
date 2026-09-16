# LDP

## 分类
Load-Store

## 描述
Load Pair of Registers

## 详细信息

```
408 ===
LDP
Load Pair of Registers calculates an address from a base register value and an immediate offset, loads two 32-bit
words or two 64-bit doublewords from memory, and writes them to two registers. For information about memory
accesses, see
Load/Store addressing modes.
It has encodings from 3 classes: Post-index , Pre-index and Signed offset
Post-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
x 0 1 0 1 0 0 0 1 1 imm7 Rt2 Rn Rt
opc L
32-bit (opc == 00)
LDP <Wt1>, <Wt2>, [<Xn|SP>], #<imm>
64-bit (opc == 10)
LDP <Xt1>, <Xt2>, [<Xn|SP>], #<imm>
boolean wback = TRUE;
boolean postindex = TRUE;
Pre-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
x 0 1 0 1 0 0 1 1 1 imm7 Rt2 Rn Rt
opc L
32-bit (opc == 00)
LDP <Wt1>, <Wt2>, [<Xn|SP>, #<imm>]!
64-bit (opc == 10)
LDP <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!
boolean wback = TRUE;
boolean postindex = FALSE;
Signed offset
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
x 0 1 0 1 0 0 1 0 1 imm7 Rt2 Rn Rt
opc L
32-bit (opc == 00)
LDP <Wt1>, <Wt2>, [<Xn|SP>{, #<imm>}]
64-bit (opc == 10)
LDP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
boolean wback = FALSE;
boolean postindex = FALSE;
LDP Page 405
RETIRED


```

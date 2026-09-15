# LDPSW

## 分类
Load-Store

## 描述
Load Pair of Registers Signed Word

## 详细信息

```
411 ===
LDPSW
Load Pair of Registers Signed Word calculates an address from a base register value and an immediate offset, loads
two 32-bit words from memory, sign-extends them, and writes them to two registers. For information about memory
accesses, see
Load/Store addressing modes.
It has encodings from 3 classes: Post-index , Pre-index and Signed offset
Post-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 0 1 0 0 0 1 1 imm7 Rt2 Rn Rt
opc L
LDPSW <Xt1>, <Xt2>, [<Xn|SP>], #<imm>
boolean wback = TRUE;
boolean postindex = TRUE;
Pre-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 0 1 0 0 1 1 1 imm7 Rt2 Rn Rt
opc L
LDPSW <Xt1>, <Xt2>, [<Xn|SP>, #<imm>]!
boolean wback = TRUE;
boolean postindex = FALSE;
Signed offset
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 0 1 0 0 1 0 1 imm7 Rt2 Rn Rt
opc L
LDPSW <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
boolean wback = FALSE;
boolean postindex = FALSE;
For information about the CONSTRAINED UNPREDICTABLE behavior of this instruction, see
Architectural Constraints on
UNPREDICTABLE behaviors, and particularly LDPSW.
Assembler Symbols
<Xt1> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Xt2> Is the 64-bit name of the second general-purpose register to be transferred, encoded in the "Rt2" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<imm> For the post-index and pre-index variant: is the signed immediate byte offset, a multiple of 4 in the
range -256 to 252, encoded in the "imm7" field as <imm>/4.
For the signed offset variant: is the optional signed immediate byte offset, a multiple of 4 in the range
-256 to 252, defaulting to 0 and encoded in the "imm7" field as <imm>/4.
LDPSW Page 408
RETIRED


```

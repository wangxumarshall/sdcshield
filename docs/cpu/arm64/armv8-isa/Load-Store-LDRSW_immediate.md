# LDRSW (immediate)

## 分类
Load-Store

## 描述
Load Register Signed Word (immediate)

## 详细信息

```
443 ===
LDRSW (immediate)
Load Register Signed Word (immediate) loads a word from memory, sign-extends it to 64 bits, and writes the result to
a register. The address that is used for the load is calculated from a base register and an immediate offset. For
information about memory accesses, see
Load/Store addressing modes.
It has encodings from 3 classes: Post-index , Pre-index and Unsigned offset
Post-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 1 1 1 0 0 0 1 0 0 imm9 0 1 Rn Rt
size opc
LDRSW <Xt>, [<Xn|SP>], #<simm>
boolean wback = TRUE;
boolean postindex = TRUE;
bits(64) offset = SignExtend
(imm9, 64);
Pre-index
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 1 1 1 0 0 0 1 0 0 imm9 1 1 Rn Rt
size opc
LDRSW <Xt>, [<Xn|SP>, #<simm>]!
boolean wback = TRUE;
boolean postindex = FALSE;
bits(64) offset = SignExtend
(imm9, 64);
Unsigned offset
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 0 1 1 1 0 0 1 1 0 imm12 Rn Rt
size opc
LDRSW <Xt>, [<Xn|SP>{, #<pimm>}]
boolean wback = FALSE;
boolean postindex = FALSE;
bits(64) offset = LSL
(ZeroExtend(imm12, 64), 2);
For information about the CONSTRAINED UNPREDICTABLE behavior of this instruction, see Architectural Constraints on
UNPREDICTABLE behaviors, and particularly LDRSW (immediate).
Assembler Symbols
<Xt> Is the 64-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<simm> Is the signed immediate byte offset, in the range -256 to 255, encoded in the "imm9" field.
<pimm> Is the optional positive immediate byte offset, a multiple of 4 in the range 0 to 16380, defaulting to 0
and encoded in the "imm12" field as <pimm>/4.
LDRSW (immediate) Page 440
RETIRED


```

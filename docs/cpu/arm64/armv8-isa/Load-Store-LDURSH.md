# LDURSH

## 分类
Load-Store

## 描述
Load Register Signed Halfword (unscaled)

## 详细信息

```
501 ===
LDURSH
Load Register Signed Halfword (unscaled) calculates an address from a base register and an immediate offset, loads a
signed halfword from memory, sign-extends it, and writes it to a register. For information about memory accesses, see
Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 1 1 1 1 0 0 0 1 x 0 imm9 0 0 Rn Rt
size opc
32-bit (opc == 11)
LDURSH <Wt>, [<Xn|SP>{, #<simm>}]
64-bit (opc == 10)
LDURSH <Xt>, [<Xn|SP>{, #<simm>}]
bits(64) offset = SignExtend(imm9, 64);
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xt> Is the 64-bit name of the general-purpose register to be transferred, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<simm> Is the optional signed immediate byte offset, in the range -256 to 255, defaulting to 0 and encoded in
the "imm9" field.
Shared Decode
integer n = UInt
(Rn);
integer t = UInt(Rt);
MemOp memop;
boolean signed;
integer regsize;
if opc<1> == '0' then
// store or zero-extending load
memop = if opc<0> == '1' then MemOp_LOAD else MemOp_STORE;
regsize = 32;
signed = FALSE;
else
// sign-extending load
memop = MemOp_LOAD
;
regsize = if opc<0> == '1' then 32 else 64;
signed = TRUE;
boolean tag_checked = memop != MemOp_PREFETCH
&& (n != 31);
LDURSH Page 498
RETIRED


```

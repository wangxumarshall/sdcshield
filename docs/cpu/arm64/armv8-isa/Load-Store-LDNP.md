# LDNP

## 分类
Load-Store

## 描述
Load Pair of Registers, with non-temporal hint

## 详细信息

```
406 ===
LDNP
Load Pair of Registers, with non-temporal hint, calculates an address from a base register value and an immediate
offset, loads two 32-bit words or two 64-bit doublewords from memory, and writes them to two registers.
For information about memory accesses, see Load/Store addressing modes. For information about Non-temporal pair
instructions, see Load/Store Non-temporal pair.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
x 0 1 0 1 0 0 0 0 1 imm7 Rt2 Rn Rt
opc L
32-bit (opc == 00)
LDNP <Wt1>, <Wt2>, [<Xn|SP>{, #<imm>}]
64-bit (opc == 10)
LDNP <Xt1>, <Xt2>, [<Xn|SP>{, #<imm>}]
// Empty.
For information about the CONSTRAINED UNPREDICTABLE behavior of this instruction, see Architectural Constraints on
UNPREDICTABLE behaviors, and particularly LDNP.
Assembler Symbols
<Wt1> Is the 32-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Wt2> Is the 32-bit name of the second general-purpose register to be transferred, encoded in the "Rt2" field.
<Xt1> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Xt2> Is the 64-bit name of the second general-purpose register to be transferred, encoded in the "Rt2" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<imm> For the 32-bit variant: is the optional signed immediate byte offset, a multiple of 4 in the range -256 to
252, defaulting to 0 and encoded in the "imm7" field as <imm>/4.
For the 64-bit variant: is the optional signed immediate byte offset, a multiple of 8 in the range -512 to
504, defaulting to 0 and encoded in the "imm7" field as <imm>/8.
Shared Decode
integer n = UInt
(Rn);
integer t = UInt(Rt);
integer t2 = UInt(Rt2);
if opc<0> == '1' then UNDEFINED;
integer scale = 2 + UInt
(opc<1>);
integer datasize = 8 << scale;
bits(64) offset = LSL
(SignExtend(imm7, 64), scale);
boolean tag_checked = n != 31;
boolean rt_unknown = FALSE;
if t == t2 then
Constraint
c = ConstrainUnpredictable(Unpredictable_LDPOVERLAP);
assert c IN {Constraint_UNKNOWN, Constraint_UNDEF, Constraint_NOP};
case c of
when Constraint_UNKNOWN rt_unknown = TRUE;    // result is UNKNOWN
when Constraint_UNDEF UNDEFINED;
when Constraint_NOP EndOfInstruction();
LDNP Page 403
RETIRED


```

# BTI

## 分类
Pointer-Authentication

## 描述
Branch Target Identification

## 详细信息

```
76 ===
BTI
Branch Target Identification. ABTI instruction is used to guard against the execution of instructions which are not the
intended target of a branch.
Outside of a guarded memory region, a BTI instruction executes as a NOP. Within a guarded memory region while
PSTATE.BTYPE != 0b00, a BTI instruction compatible with the current value of PSTATE.BTYPE will not generate a
Branch Target Exception and will allow execution of subsequent instructions within the memory region.
The operand <targets> passed to a BTI instruction determines the values of
PSTATE.BTYPE which the BTI instruction
is compatible with.
Note
Within a guarded memory region, when PSTATE.BTYPE != 0b00, all instructions will generate a Branch Target
Exception, other than BRK, BTI, HLT, PACIASP, and PACIBSP, which might not. See the individual instructions for
more information.
System
(FEAT_BTI)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 1 0 0 x x 0 1 1 1 1 1
CRm op2
BTI {<targets>}
SystemHintOp op;
if CRm:op2 == '0100 xx0' then
op = SystemHintOp_BTI;
// Check branch target compatibility between BTI instruction and PSTATE.BTYPE
SetBTypeCompatible(BTypeCompatible_BTI(op2<2:1>));
else
EndOfInstruction();
Assembler Symbols
<targets> Is the type of indirection, encoded in “op2<2:1>”:
op2<2:1> <targets>
00 (omitted)
01 c
10 j
11 jc
BTI Page 73
RETIRED


```

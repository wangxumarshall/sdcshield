# LDAXP

## 分类
Load-Store

## 描述
Load-Acquire Exclusive Pair of Registers

## 详细信息

```
380 ===
LDAXP
Load-Acquire Exclusive Pair of Registers derives an address from a base register value, loads two 32-bit words or two
64-bit doublewords from memory, and writes them to two registers. For information on single-copy atomicity and
alignment requirements, see
Requirements for single-copy atomicity and Alignment of data accesses. The PE marks
the physical address being accessed as an exclusive access. This exclusive access mark is checked by Store Exclusive
instructions. See
Synchronization and semaphores. The instruction also has memory ordering semantics, as described
in Load-Acquire, Store-Release. For information about memory accesses, see Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 sz 0 0 1 0 0 0 0 1 1 (1) (1) (1) (1) (1) 1 Rt2 Rn Rt
L Rs o0
32-bit (sz == 0)
LDAXP <Wt1>, <Wt2>, [<Xn|SP>{,#0}]
64-bit (sz == 1)
LDAXP <Xt1>, <Xt2>, [<Xn|SP>{,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
integer t2 = UInt(Rt2);
integer elsize = 32 << UInt(sz);
integer datasize = elsize * 2;
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
For information about the CONSTRAINED UNPREDICTABLE behavior of this instruction, see Architectural Constraints on
UNPREDICTABLE behaviors, and particularly LDAXP.
Assembler Symbols
<Wt1> Is the 32-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Wt2> Is the 32-bit name of the second general-purpose register to be transferred, encoded in the "Rt2" field.
<Xt1> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Xt2> Is the 64-bit name of the second general-purpose register to be transferred, encoded in the "Rt2" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
LDAXP Page 377
RETIRED


```

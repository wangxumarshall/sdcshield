# STLXP

## 分类
Load-Store

## 描述
Store-Release Exclusive Pair of registers

## 详细信息

```
687 ===
STLXP
Store-Release Exclusive Pair of registers stores two 32-bit words or two 64-bit doublewords to a memory location if the
PE has exclusive access to the memory address, from two registers, and returns a status value of 0 if the store was
successful, or of 1 if no store was performed. See
Synchronization and semaphores. For information on single-copy
atomicity and alignment requirements, see Requirements for single-copy atomicity and Alignment of data accesses. If
a 64-bit pair Store-Exclusive succeeds, it causes a single-copy atomic update of the 128-bit memory location being
updated. The instruction also has memory ordering semantics, as described in
Load-Acquire, Store-Release. For
information about memory accesses, see Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 sz 0 0 1 0 0 0 0 0 1 Rs 1 Rt2 Rn Rt
L o0
32-bit (sz == 0)
STLXP <Ws>, <Wt1>, <Wt2>, [<Xn|SP>{,#0}]
64-bit (sz == 1)
STLXP <Ws>, <Xt1>, <Xt2>, [<Xn|SP>{,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
integer t2 = UInt(Rt2);    // ignored by load/store single register
integer s = UInt(Rs);    // ignored by all loads and store-release
integer elsize = 32 << UInt(sz);
integer datasize = elsize * 2;
boolean tag_checked = n != 31;
boolean rt_unknown = FALSE;
boolean rn_unknown = FALSE;
if s == t || (s == t2) then
Constraint
c = ConstrainUnpredictable(Unpredictable_DATAOVERLAP);
assert c IN {Constraint_UNKNOWN, Constraint_UNDEF, Constraint_NOP};
case c of
when Constraint_UNKNOWN rt_unknown = TRUE;    // store UNKNOWN value
when Constraint_UNDEF UNDEFINED;
when Constraint_NOP EndOfInstruction();
if s == n && n != 31 then
Constraint c = ConstrainUnpredictable(Unpredictable_BASEOVERLAP);
assert c IN {Constraint_UNKNOWN, Constraint_UNDEF, Constraint_NOP};
case c of
when Constraint_UNKNOWN rn_unknown = TRUE;    // address is UNKNOWN
when Constraint_UNDEF UNDEFINED;
when Constraint_NOP EndOfInstruction();
For information about the CONSTRAINED UNPREDICTABLE behavior of this instruction, see Architectural Constraints on
UNPREDICTABLE behaviors, and particularly STLXP.
Assembler Symbols
<Ws> Is the 32-bit name of the general-purpose register into which the status result of the store exclusive is
written, encoded in the "Rs" field. The value returned is:
0
If the operation updates memory.
1
If the operation fails to update memory.
<Xt1> Is the 64-bit name of the first general-purpose register to be transferred, encoded in the "Rt" field.
<Xt2> Is the 64-bit name of the second general-purpose register to be transferred, encoded in the "Rt2" field.
STLXP Page 684
RETIRED


```

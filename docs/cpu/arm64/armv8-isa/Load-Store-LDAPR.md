# LDAPR

## 分类
Load-Store

## 描述
Load-Acquire RCpc Register

## 详细信息

```
361 ===
LDAPR
Load-Acquire RCpc Register derives an address from a base register value, loads a 32-bit word or 64-bit doubleword
from the derived address in memory, and writes it to a register.
The instruction has memory ordering semantics as described in Load-Acquire, Load-AcquirePC, and Store-Release,
except that:
• There is no ordering requirement, separate from the requirements of a Load-AcquirePC or a Store-Release,
created by having a Store-Release followed by a Load-AcquirePC instruction.
• The reading of a value written by a Store-Release by a Load-AcquirePC instruction by the same observer does
not make the write of the Store-Release globally observed.
This difference in memory ordering is not described in the pseudocode.
For information about memory accesses, see
Load/Store addressing modes.
Integer
(FEAT_LRCPC)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 x 1 1 1 0 0 0 1 0 1 (1) (1) (1) (1) (1) 1 1 0 0 0 0 Rn Rt
size Rs
32-bit (size == 10)
LDAPR <Wt>, [<Xn|SP> {,#0}]
64-bit (size == 11)
LDAPR <Xt>, [<Xn|SP> {,#0}]
integer n = UInt(Rn);
integer t = UInt(Rt);
integer elsize = 8 << UInt(size);
integer regsize = if elsize == 64 then 64 else 32;
boolean tag_checked = n != 31;
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be loaded, encoded in the "Rt" field.
<Xt> Is the 64-bit name of the general-purpose register to be loaded, encoded in the "Rt" field.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
Operation
bits(64) address;
bits(elsize) data;
constant integer dbytes = elsize DIV 8;
if HaveMTE2Ext
() then
SetTagCheckedInstruction(tag_checked);
if n == 31 then
CheckSPAlignment();
address = SP[];
else
address = X[n];
data = Mem[address, dbytes, AccType_ORDERED];
X[t] = ZeroExtend(data, regsize);
LDAPR Page 358
RETIRED


```

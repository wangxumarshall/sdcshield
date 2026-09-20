# LDR (literal)

## 分类
Load-Store

## 描述
Load Register (literal)

## 详细信息

```
417 ===
LDR (literal)
Load Register (literal) calculates an address from the PC value and an immediate offset, loads a word from memory,
and writes it to a register. For information about memory accesses, see Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
0 x 0 1 1 0 0 0 imm19 Rt
opc
32-bit (opc == 00)
LDR <Wt>, <label>
64-bit (opc == 01)
LDR <Xt>, <label>
integer t = UInt(Rt);
MemOp memop = MemOp_LOAD;
boolean signed = FALSE;
integer size;
bits(64) offset;
case opc of
when '00'
size = 4;
when '01'
size = 8;
when '10'
size = 4;
signed = TRUE;
when '11'
memop = MemOp_PREFETCH
;
offset = SignExtend(imm19:'00', 64);
Assembler Symbols
<Wt> Is the 32-bit name of the general-purpose register to be loaded, encoded in the "Rt" field.
<Xt> Is the 64-bit name of the general-purpose register to be loaded, encoded in the "Rt" field.
<label> Is the program label from which the data is to be loaded. Its offset from the address of this instruction,
in the range +/-1MB, is encoded as "imm19" times 4.
Operation
bits(64) address = PC
[] + offset;
bits(size*8) data;
if HaveMTE2Ext() then
SetTagCheckedInstruction(FALSE);
case memop of
when MemOp_LOAD
data = Mem[address, size, AccType_NORMAL];
if signed then
X[t] = SignExtend(data, 64);
else
X[t] = data;
when MemOp_PREFETCH
Prefetch(address, t<4:0>);
LDR (literal) Page 414
RETIRED


```

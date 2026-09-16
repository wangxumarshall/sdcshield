# ORR (immediate)

## 分类
Logical

## 描述
Bitwise OR (immediate)

## 详细信息

```
552 ===
ORR (immediate)
Bitwise OR (immediate) performs a bitwise (inclusive) OR of a register value and an immediate register value, and
writes the result to the destination register.
This instruction is used by the alias MOV (bitmask immediate).
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 1 1 0 0 1 0 0 N immr imms Rn Rd
opc
32-bit (sf == 0 && N == 0)
ORR <Wd|WSP>, <Wn>, #<imm>
64-bit (sf == 1)
ORR <Xd|SP>, <Xn>, #<imm>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer datasize = if sf == '1' then 64 else 32;
bits(datasize) imm;
if sf == '0' && N != '0' then UNDEFINED;
(imm, -) = DecodeBitMasks
(N, imms, immr, TRUE);
Assembler Symbols
<Wd|WSP> Is the 32-bit name of the destination general-purpose register or stack pointer, encoded in the "Rd"
field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd|SP> Is the 64-bit name of the destination general-purpose register or stack pointer, encoded in the "Rd"
field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<imm> For the 32-bit variant: is the bitmask immediate, encoded in "imms:immr".
For the 64-bit variant: is the bitmask immediate, encoded in "N:imms:immr".
Alias Conditions
Alias Is preferred when
MOV (bitmask immediate)
Rn == '11111' && ! MoveWidePreferred(sf, N, imms, immr)
Operation
bits(datasize) result;
bits(datasize) operand1 = X[n];
result = operand1 OR imm;
if d == 31 then
SP
[] = result;
else
X[d] = result;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
ORR (immediate) Page 549
RETIRED


```

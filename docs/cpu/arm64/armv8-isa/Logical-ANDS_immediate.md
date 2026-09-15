# ANDS (immediate)

## 分类
Logical

## 描述
Bitwise AND (immediate), setting flags

## 详细信息

```
37 ===
ANDS (immediate)
Bitwise AND (immediate), setting flags, performs a bitwise AND of a register value and an immediate value, and writes
the result to the destination register. It updates the condition flags based on the result.
This instruction is used by the alias TST (immediate).
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 1 1 0 0 1 0 0 N immr imms Rn Rd
opc
32-bit (sf == 0 && N == 0)
ANDS <Wd>, <Wn>, #<imm>
64-bit (sf == 1)
ANDS <Xd>, <Xn>, #<imm>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer datasize = if sf == '1' then 64 else 32;
bits(datasize) imm;
if sf == '0' && N != '0' then UNDEFINED;
(imm, -) = DecodeBitMasks
(N, imms, immr, TRUE);
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<imm> For the 32-bit variant: is the bitmask immediate, encoded in "imms:immr".
For the 64-bit variant: is the bitmask immediate, encoded in "N:imms:immr".
Alias Conditions
Alias Is preferred when
TST (immediate)
Rd == '11111'
Operation
bits(datasize) result;
bits(datasize) operand1 = X[n];
result = operand1 AND imm;
PSTATE.<N,Z,C,V> = result<datasize-1>:IsZeroBit
(result):'00';
X[d] = result;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
ANDS (immediate) Page 34
RETIRED


```

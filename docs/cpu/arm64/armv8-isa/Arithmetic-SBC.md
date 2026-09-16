# SBC

## 分类
Arithmetic

## 描述
Subtract with Carry

## 详细信息

```
588 ===
SBC
Subtract with Carry subtracts a register value and the value of NOT (Carry flag) from a register value, and writes the
result to the destination register.
This instruction is used by the alias NGC.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 1 0 1 0 0 0 0 Rm 0 0 0 0 0 0 Rn Rd
op S
32-bit (sf == 0)
SBC <Wd>, <Wn>, <Wm>
64-bit (sf == 1)
SBC <Xd>, <Xn>, <Xm>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer m = UInt(Rm);
integer datasize = if sf == '1' then 64 else 32;
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Wm> Is the 32-bit name of the second general-purpose source register, encoded in the "Rm" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the first general-purpose source register, encoded in the "Rn" field.
<Xm> Is the 64-bit name of the second general-purpose source register, encoded in the "Rm" field.
Alias Conditions
Alias Is preferred when
NGC
Rn == '11111'
Operation
bits(datasize) result;
bits(datasize) operand1 = X[n];
bits(datasize) operand2 = X[m];
operand2 = NOT(operand2);
(result, -) = AddWithCarry(operand1, operand2, PSTATE.C);
X[d] = result;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
SBC Page 585
RETIRED


```

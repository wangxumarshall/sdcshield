# MOVZ

## 分类
Logical

## 描述
Move wide with zero

## 详细信息

```
531 ===
MOVZ
Move wide with zero moves an optionally-shifted 16-bit immediate value to a register.
This instruction is used by the alias MOV (wide immediate).
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 0 0 1 0 1 hw imm16 Rd
opc
32-bit (sf == 0 && hw == 0x)
MOVZ <Wd>, #<imm>{, LSL #<shift>}
64-bit (sf == 1)
MOVZ <Xd>, #<imm>{, LSL #<shift>}
integer d = UInt(Rd);
integer datasize = if sf == '1' then 64 else 32;
integer pos;
if sf == '0' && hw<1> == '1' then UNDEFINED;
pos = UInt(hw:'0000');
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<imm> Is the 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field.
<shift> For the 32-bit variant: is the amount by which to shift the immediate left, either 0 (the default) or 16,
encoded in the "hw" field as <shift>/16.
For the 64-bit variant: is the amount by which to shift the immediate left, either 0 (the default), 16, 32
or 48, encoded in the "hw" field as <shift>/16.
Alias Conditions
Alias Is preferred when
MOV (wide immediate)
! (IsZero(imm16) && hw != '00')
Operation
bits(datasize) result;
result = Zeros();
result<pos+15:pos> = imm16;
X[d] = result;
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
MOVZ Page 528
RETIRED


```

# MOV (inverted wide immediate)

## 分类
Logical

## 描述
Move (inverted wide immediate):

## 详细信息

```
523 ===
MOV (inverted wide immediate)
Move (inverted wide immediate) moves an inverted 16-bit immediate value to a register.
This is an alias of MOVN. This means:
• The encodings in this description are named to match the encodings of MOVN.
• The description of MOVN gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 0 0 1 0 1 hw imm16 Rd
opc
32-bit (sf == 0 && hw == 0x)
MOV <Wd>, #<imm>
is equivalent to
MOVN <Wd>, #<imm16>, LSL #<shift>
and is the preferred disassembly when
! (IsZero(imm16) && hw != '00') && ! IsOnes(imm16).
64-bit (sf == 1)
MOV <Xd>, #<imm>
is equivalent to
MOVN <Xd>, #<imm16>, LSL #<shift>
and is the preferred disassembly when
! (IsZero(imm16) && hw != '00').
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<imm> For the 32-bit variant: is a 32-bit immediate, the bitwise inverse of which can be encoded in
"imm16:hw", but excluding 0xffff0000 and 0x0000ffff
For the 64-bit variant: is a 64-bit immediate, the bitwise inverse of which can be encoded in
"imm16:hw".
<shift> For the 32-bit variant: is the amount by which to shift the immediate left, either 0 (the default) or 16,
encoded in the "hw" field as <shift>/16.
For the 64-bit variant: is the amount by which to shift the immediate left, either 0 (the default), 16, 32
or 48, encoded in the "hw" field as <shift>/16.
Operation
The description of MOVN
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
MOV (inverted wide
immediate) Page 520
RETIRED


```

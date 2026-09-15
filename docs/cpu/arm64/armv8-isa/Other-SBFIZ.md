# SBFIZ

## 分类
Other

## 描述
Signed Bitfield Insert in Zero:

## 详细信息

```
592 ===
SBFIZ
Signed Bitfield Insert in Zeros copies a bitfield of <width> bits from the least significant bits of the source register to
bit position <lsb> of the destination register, setting the destination bits below the bitfield to zero, and the bits above
the bitfield to a copy of the most significant bit of the bitfield.
This is an alias of SBFM
. This means:
• The encodings in this description are named to match the encodings of SBFM.
• The description of SBFM gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 0 0 1 0 0 1 1 0 N immr imms Rn Rd
opc
32-bit (sf == 0 && N == 0)
SBFIZ <Wd>, <Wn>, #<lsb>, #<width>
is equivalent to
SBFM <Wd>, <Wn>, #(-<lsb> MOD 32), #(<width>-1)
and is the preferred disassembly when
UInt(imms) < UInt(immr).
64-bit (sf == 1 && N == 1)
SBFIZ <Xd>, <Xn>, #<lsb>, #<width>
is equivalent to
SBFM <Xd>, <Xn>, #(-<lsb> MOD 64), #(<width>-1)
and is the preferred disassembly when
UInt(imms) < UInt(immr).
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<lsb> For the 32-bit variant: is the bit number of the lsb of the destination bitfield, in the range 0 to 31.
For the 64-bit variant: is the bit number of the lsb of the destination bitfield, in the range 0 to 63.
<width> For the 32-bit variant: is the width of the bitfield, in the range 1 to 32-<lsb>.
For the 64-bit variant: is the width of the bitfield, in the range 1 to 64-<lsb>.
Operation
The description of SBFM
gives the operational pseudocode for this instruction.
Operational information
If PSTATE.DIT is 1:
• The execution time of this instruction is independent of:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
• The response of this instruction to asynchronous exceptions does not vary based on:
◦ The values of the data supplied in any of its registers.
◦ The values of the NZCV flags.
SBFIZ Page 589
RETIRED


```

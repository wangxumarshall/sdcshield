# UBFM

## 分类
Bit-Manipulation

## 描述
Unsigned Bitfield Move

## 详细信息

```
795 ===
UBFM
Unsigned Bitfield Move is usually accessed via one of its aliases, which are always preferred for disassembly.
If <imms> is greater than or equal to <immr>, this copies a bitfield of (<imms>-<immr>+1) bits starting from bit
position <immr> in the source register to the least significant bits of the destination register.
If <imms> is less than <immr>, this copies a bitfield of (<imms>+1) bits from the least significant bits of the source
register to bit position (regsize-<immr>) of the destination register, where regsize is the destination register size of 32
or 64 bits.
In both cases the destination bits below and above the bitfield are set to zero.
This instruction is used by the aliases LSL (immediate)
, LSR (immediate), UBFIZ, UBFX, UXTB, and UXTH.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 0 0 1 1 0 N immr imms Rn Rd
opc
32-bit (sf == 0 && N == 0)
UBFM <Wd>, <Wn>, #<immr>, #<imms>
64-bit (sf == 1 && N == 1)
UBFM <Xd>, <Xn>, #<immr>, #<imms>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer datasize = if sf == '1' then 64 else 32;
integer R;
bits(datasize) wmask;
bits(datasize) tmask;
if sf == '1' && N != '1' then UNDEFINED;
if sf == '0' && (N != '0' || immr<5> != '0' || imms<5> != '0') then UNDEFINED;
R = UInt
(immr);
(wmask, tmask) = DecodeBitMasks(N, imms, immr, FALSE);
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
<immr> For the 32-bit variant: is the right rotate amount, in the range 0 to 31, encoded in the "immr" field.
For the 64-bit variant: is the right rotate amount, in the range 0 to 63, encoded in the "immr" field.
<imms> For the 32-bit variant: is the leftmost bit number to be moved from the source, in the range 0 to 31,
encoded in the "imms" field.
For the 64-bit variant: is the leftmost bit number to be moved from the source, in the range 0 to 63,
encoded in the "imms" field.
Alias Conditions
Alias Of variant Is preferred when
LSL (immediate)
32-bit imms != '011111' && imms + 1 == immr
LSL (immediate) 64-bit imms != '111111' && imms + 1 == immr
LSR (immediate) 32-bit imms == '011111'
LSR (immediate) 64-bit imms == '111111'
UBFM Page 792
RETIRED


```

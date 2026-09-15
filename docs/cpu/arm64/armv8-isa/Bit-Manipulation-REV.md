# REV

## 分类
Bit-Manipulation

## 描述
Reverse Bytes

## 详细信息

```
576 ===
REV
Reverse Bytes reverses the byte order in a register.
This instruction is used by the pseudo-instruction REV64.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
sf 1 0 1 1 0 1 0 1 1 0 0 0 0 0 0 0 0 0 0 1 x Rn Rd
opc
32-bit (sf == 0 && opc == 10)
REV <Wd>, <Wn>
64-bit (sf == 1 && opc == 11)
REV <Xd>, <Xn>
integer d = UInt(Rd);
integer n = UInt(Rn);
integer datasize = if sf == '1' then 64 else 32;
integer container_size;
case opc of
when '00'
Unreachable();
when '01'
container_size = 16;
when '10'
container_size = 32;
when '11'
if sf == '0' then UNDEFINED;
container_size = 64;
Assembler Symbols
<Wd> Is the 32-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Wn> Is the 32-bit name of the general-purpose source register, encoded in the "Rn" field.
<Xd> Is the 64-bit name of the general-purpose destination register, encoded in the "Rd" field.
<Xn> Is the 64-bit name of the general-purpose source register, encoded in the "Rn" field.
Operation
bits(datasize) operand = X
[n];
bits(datasize) result;
integer containers = datasize DIV container_size;
integer elements_per_container = container_size DIV 8;
integer index = 0;
integer rev_index;
for c = 0 to containers-1
rev_index = index + ((elements_per_container - 1) * 8);
for e = 0 to elements_per_container-1
result<rev_index+7:rev_index> = operand<index+7:index>;
index = index + 8;
rev_index = rev_index - 8;
X
[d] = result;
REV Page 573
RETIRED


```

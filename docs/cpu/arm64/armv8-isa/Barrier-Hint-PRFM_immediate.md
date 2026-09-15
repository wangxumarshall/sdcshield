# PRFM (immediate)

## 分类
Barrier-Hint

## 描述
Prefetch Memory (immediate)

## 详细信息

```
563 ===
PRFM (immediate)
Prefetch Memory (immediate) signals the memory system that data memory accesses from a specified address are
likely to occur in the near future. The memory system can respond by taking actions that are expected to speed up the
memory accesses when they do occur, such as preloading the cache line containing the specified address into one or
more caches.
The effect of anPRFM instruction is IMPLEMENTATION DEFINED. For more information, see
Prefetch memory.
For information about memory accesses, see Load/Store addressing modes.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 1 1 1 0 0 1 1 0 imm12 Rn Rt
size opc
PRFM (<prfop>|#<imm5>), [<Xn|SP>{, #<pimm>}]
bits(64) offset = LSL(ZeroExtend(imm12, 64), 3);
Assembler Symbols
<prfop> Is the prefetch operation, defined as <type><target><policy>.
<type> is one of:
PLD
Prefetch for load, encoded in the "Rt<4:3>" field as 0b00.
PLI
Preload instructions, encoded in the "Rt<4:3>" field as 0b01.
PST
Prefetch for store, encoded in the "Rt<4:3>" field as 0b10.
<target> is one of:
L1
Level 1 cache, encoded in the "Rt<2:1>" field as 0b00.
L2
Level 2 cache, encoded in the "Rt<2:1>" field as 0b01.
L3
Level 3 cache, encoded in the "Rt<2:1>" field as 0b10.
<policy> is one of:
KEEP
Retained or temporal prefetch, allocated in the cache normally. Encoded in the "Rt<0>" field as 0.
STRM
Streaming or non-temporal prefetch, for data that is used only once. Encoded in the "Rt<0>" field
as 1.
For more information on these prefetch operations, see
Prefetch memory.
For other encodings of the "Rt" field, use <imm5>.
<imm5> Is the prefetch operation encoding as an immediate, in the range 0 to 31, encoded in the "Rt" field.
This syntax is only for encodings that are not accessible using <prfop>.
<Xn|SP> Is the 64-bit name of the general-purpose base register or stack pointer, encoded in the "Rn" field.
<pimm> Is the optional positive immediate byte offset, a multiple of 8 in the range 0 to 32760, defaulting to 0
and encoded in the "imm12" field as <pimm>/8.
Shared Decode
integer n = UInt(Rn);
integer t = UInt(Rt);
PRFM (immediate) Page 560
RETIRED


```

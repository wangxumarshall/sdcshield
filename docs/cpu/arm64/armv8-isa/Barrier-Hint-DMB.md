# DMB

## 分类
Barrier-Hint

## 描述
Data Memory Barrier

## 详细信息

```
328 ===
DMB
Data Memory Barrier is a memory barrier that ensures the ordering of observations of memory accesses, see Data
Memory Barrier.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 1 CRm 1 0 1 1 1 1 1 1
opc
DMB <option>|#<imm>
MBReqDomain domain;
MBReqTypes types;
case CRm<3:2> of
when '00' domain = MBReqDomain_OuterShareable;
when '01' domain = MBReqDomain_Nonshareable;
when '10' domain = MBReqDomain_InnerShareable;
when '11' domain = MBReqDomain_FullSystem;
case CRm<1:0> of
when '00' types = MBReqTypes_All; domain = MBReqDomain_FullSystem;
when '01' types = MBReqTypes_Reads;
when '10' types = MBReqTypes_Writes;
when '11' types = MBReqTypes_All;
Assembler Symbols
<option> Specifies the limitation on the barrier operation. Values are:
SY
Full system is the required shareability domain, reads and writes are the required access types,
both before and after the barrier instruction. This option is referred to as the full system barrier.
Encoded as CRm = 0b1111.
ST
Full system is the required shareability domain, writes are the required access type, both before
and after the barrier instruction. Encoded as CRm = 0b1110.
LD
Full system is the required shareability domain, reads are the required access type before the
barrier instruction, and reads and writes are the required access types after the barrier
instruction. Encoded as CRm = 0b1101.
ISH
Inner Shareable is the required shareability domain, reads and writes are the required access
types, both before and after the barrier instruction. Encoded as CRm = 0b1011.
ISHST
Inner Shareable is the required shareability domain, writes are the required access type, both
before and after the barrier instruction. Encoded as CRm = 0b1010.
ISHLD
Inner Shareable is the required shareability domain, reads are the required access type before the
barrier instruction, and reads and writes are the required access types after the barrier
instruction. Encoded as CRm = 0b1001.
NSH
Non-shareable is the required shareability domain, reads and writes are the required access, both
before and after the barrier instruction. Encoded as CRm = 0b0111.
NSHST
Non-shareable is the required shareability domain, writes are the required access type, both
before and after the barrier instruction. Encoded as CRm = 0b0110.
NSHLD
Non-shareable is the required shareability domain, reads are the required access type before the
barrier instruction, and reads and writes are the required access types after the barrier
instruction. Encoded as CRm = 0b0101.
DMB Page 325
RETIRED


```

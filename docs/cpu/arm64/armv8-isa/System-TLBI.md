# TLBI

## 分类
System

## 描述
TLB Invalidate operation:

## 详细信息

```
786 ===
TLBI
TLB Invalidate operation. For more information, see op0==0b01, cache maintenance, TLB maintenance, and address
translation instructions.
This is an alias of SYS. This means:
• The encodings in this description are named to match the encodings of SYS.
• The description of SYS gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 1 op1 1 0 0 0 CRm op2 Rt
L CRn
TLBI <tlbi_op>{, <Xt>}
is equivalent to
SYS #<op1>, C8, <Cm>, #<op2>{, <Xt>}
and is the preferred disassembly when SysOp(op1,'1000',CRm,op2) == Sys_TLBI.
Assembler Symbols
<op1> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op1" field.
<Cm> Is a name 'Cm', with 'm' in the range 0 to 15, encoded in the "CRm" field.
<op2> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op2" field.
<tlbi_op> Is a TLBI instruction name, as listed for the TLBI system instruction group, encoded in “op1:CRm:op2”:
TLBI Page 783
RETIRED


```

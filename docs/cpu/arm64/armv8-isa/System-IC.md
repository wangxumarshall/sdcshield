# IC

## 分类
System

## 描述
Instruction Cache operation:

## 详细信息

```
350 ===
IC
Instruction Cache operation. For more information, see op0==0b01, cache maintenance, TLB maintenance, and
address translation instructions.
This is an alias of SYS. This means:
• The encodings in this description are named to match the encodings of SYS.
• The description of SYS gives the operational pseudocode for this instruction.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 1 op1 0 1 1 1 CRm op2 Rt
L CRn
IC <ic_op>{, <Xt>}
is equivalent to
SYS #<op1>, C7, <Cm>, #<op2>{, <Xt>}
and is the preferred disassembly when SysOp(op1,'0111',CRm,op2) == Sys_IC.
Assembler Symbols
<ic_op> Is an IC instruction name, as listed for the IC system instruction pages, encoded in “op1:CRm:op2”:
op1 CRm op2 <ic_op>
000 0001 000 IALLUIS
000 0101 000 IALLU
011 0101 001 IVAU
<op1> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op1" field.
<Cm> Is a name 'Cm', with 'm' in the range 0 to 15, encoded in the "CRm" field.
<op2> Is a 3-bit unsigned immediate, in the range 0 to 7, encoded in the "op2" field.
<Xt> Is the 64-bit name of the optional general-purpose source register, defaulting to '11111', encoded in the
"Rt" field.
Operation
The description of SYS
gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
IC Page 347
RETIRED


```

# CFP

## 分类
Other

## 描述
Control Flow Prediction Restriction by Context:

## 详细信息

```
95 ===
CFP
Control Flow Prediction Restriction by Context prevents control flow predictions that predict execution addresses
based on information gathered from earlier execution within a particular execution context. Control flow predictions
determined by the actions of code in the target execution context or contexts appearing in program order before the
instruction cannot be used to exploitatively control speculative execution occurring after the instruction is complete
and synchronized.
For more information, see
CFP RCTX, Control Flow Prediction Restriction by Context.
This is an alias of SYS. This means:
• The encodings in this description are named to match the encodings of SYS.
• The description of SYS gives the operational pseudocode for this instruction.
System
(FEAT_SPECRES)
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 1 0 1 1 0 1 1 1 0 0 1 1 1 0 0 Rt
L op1 CRn CRm op2
CFP RCTX, <Xt>
is equivalent to
SYS #3, C7, C3, #4, <Xt>
and is always the preferred disassembly.
Assembler Symbols
<Xt> Is the 64-bit name of the general-purpose source register, encoded in the "Rt" field.
Operation
The description of SYS gives the operational pseudocode for this instruction.
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CFP Page 92
RETIRED


```

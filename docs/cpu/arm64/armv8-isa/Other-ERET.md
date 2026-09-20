# ERET

## 分类
Other

## 描述
Exception Return

## 详细信息

```
340 ===
ERET
Exception Return using the ELR and SPSR for the current Exception level. When executed, the PE restores PSTATE
from the SPSR, and branches to the address held in the ELR.
The PE checks the SPSR for the current Exception level for an illegal return event. See Illegal return events from
AArch64 state.
ERET is UNDEFINED at EL0.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 1 0 1 0 0 1 1 1 1 1 0 0 0 0 0 0 1 1 1 1 1 0 0 0 0 0
A M Rn op4
ERET
if PSTATE.EL == EL0 then UNDEFINED;
Operation
AArch64.CheckForERetTrap(FALSE, TRUE);
bits(64) target = ELR[];
AArch64.ExceptionReturn(target, SPSR[]);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
ERET Page 337
RETIRED


```

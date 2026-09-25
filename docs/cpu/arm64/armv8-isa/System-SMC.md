# SMC

## 分类
System

## 描述
Secure Monitor Call

## 详细信息

```
647 ===
SMC
Secure Monitor Call causes an exception to EL3.
SMC is available only for software executing at EL1 or higher. It is UNDEFINED in EL0.
If the values of HCR_EL2.TSC and SCR_EL3.SMD are both 0, execution of an SMC instruction at EL1 or higher
generates a Secure Monitor Call exception, recording it in ESR_ELx, using the EC value 0x17, that is taken to EL3.
If the value of HCR_EL2.TSC is 1 and EL2 is enabled in the current Security state, execution of an SMC instruction at
EL1 generates an exception that is taken to EL2, regardless of the value of SCR_EL3.SMD.
If the value of HCR_EL2.TSC is 0 and the value of SCR_EL3.SMD is 1, the SMC instruction is UNDEFINED.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 0 0 0 imm16 0 0 0 1 1
SMC #<imm>
// Empty.
Assembler Symbols
<imm> Is a 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field.
Operation
AArch64.CheckForSMCUndefOrTrap(imm16);
AArch64.CallSecureMonitor(imm16);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
SMC Page 644
RETIRED


```

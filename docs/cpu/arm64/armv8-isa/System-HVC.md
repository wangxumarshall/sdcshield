# HVC

## 分类
System

## 描述
Hypervisor Call

## 详细信息

```
349 ===
HVC
Hypervisor Call causes an exception to EL2. Software executing at EL1 can use this instruction to call the hypervisor
to request a service.
The HVC instruction is UNDEFINED:
• When EL3 is implemented and SCR_EL3.HCE is set to 0.
• When EL3 is not implemented and HCR_EL2.HCD is set to 1.
• When EL2 is not implemented.
• At EL1 if EL2 is not enabled in the current Security state.
• At EL0.
On executing an HVC instruction, the PE records the exception as a Hypervisor Call exception in
ESR_ELx, using the
EC value 0x16, and the value of the immediate argument.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 0 0 0 0 imm16 0 0 0 1 0
HVC #<imm>
// Empty.
Assembler Symbols
<imm> Is a 16-bit unsigned immediate, in the range 0 to 65535, encoded in the "imm16" field.
Operation
if !HaveEL(EL2) || PSTATE.EL == EL0 || (PSTATE.EL == EL1 && (!IsSecureEL2Enabled() && IsSecure())) then
UNDEFINED;
hvc_enable = if HaveEL(EL3) then SCR_EL3.HCE else NOT(HCR_EL2.HCD);
if hvc_enable == '0' then
UNDEFINED;
else
AArch64.CallHypervisor(imm16);
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
HVC Page 346
RETIRED


```

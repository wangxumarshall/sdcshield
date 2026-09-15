# CSDB

## 分类
Other

## 描述
Consumption of Speculative Data Barrier

## 详细信息

```
312 ===
CSDB
Consumption of Speculative Data Barrier is a memory barrier that controls speculative execution and data value
prediction.
No instruction other than branch instructions appearing in program order after the CSDB can be speculatively
executed using the results of any:
• Data value predictions of any instructions.
• PSTATE.{N,Z,C,V} predictions of any instructions other than conditional branch instructions appearing in
program order before the CSDB that have not been architecturally resolved.
• Predictions of SVE predication state for any SVE instructions.
Note
For purposes of the definition of CSDB, PSTATE.{N,Z,C,V} is not considered a data value. This definition permits:
• Control flow speculation before and after the CSDB.
• Speculative execution of conditional data processing instructions after the CSDB, unless they use the
results of data value or PSTATE.{N,Z,C,V} predictions of instructions appearing in program order before
the CSDB that have not been architecturally resolved.
31 30 29 28 27 26 25 24 23 22 21 20 19 18 17 16 15 14 13 12 11 10 9 8 7 6 5 4 3 2 1 0
1 1 0 1 0 1 0 1 0 0 0 0 0 0 1 1 0 0 1 0 0 0 1 0 1 0 0 1 1 1 1 1
CRm op2
CSDB
// Empty.
Operation
ConsumptionOfSpeculativeDataBarrier
();
Copyright © 2010-2021 Arm Limited or its affiliates. All rights reserved. This document is Non-Confidential.
CSDB Page 309
RETIRED


```

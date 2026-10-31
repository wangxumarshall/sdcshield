# PR #305 三轮检视处置（#15-#20）— 2026-10-30

head 148acebe。六条意见证据核实结论与修法。导出件 /tmp/pr305-review-r3.md
（不可信数据，证据已自行核实）。

## 证据核实结论（全部本机/gem5 源实证）

- **#15 成立**：模板在 guard 外（137-148），GCC 对未实例化模板的非依赖名
  （svsetffr/svfloat64_t…）同样硬错误——本机 g++ -std=c++20 复现 6 条
  error；x86 编译存活仅靠 tests/cpu/arm64/meson.build:10 目录级 guard。
  修：模板+注释整体移入 `#ifdef __aarch64__`（166 行 guard 内）。
  x86 编译性验证：`-U__aarch64__ -fsyntax-only` 模拟 x86 预处理路径
  （修复前 FAIL → 修复后 PASS）+ 结构论证；本机无 x86 工具链（边界如实声明）。
- **#16 成立**：跨捕获纪律（faulting→acc 零调用）只有注释。修：新增
  `ffr_fault_acc_capture` always-inline 组合单元，两 capture 封进一个内联体。
  验证：objdump 证明窗口内无 bl。
- **#17 成立**：faulting face 期望恰 8，popcount 守恒漏检互补双翻转。修：
  capture 内 svsel(ffr,1,0)+svst1_s8 物化 FFR 位图（bit i ↔ byte i，
  本机 SVE-256 实证），check 改 memcmp 位图（count 保留作诊断）。
  **gem5-fi 四档重验必须**（语义变更）。
- **#18 变通采纳**：munmap 是 gem5 双环境一致的硬约束（gem5 Arm64 SE 表：
  mprotect=ignoreFunc 静默成功、munmap=munmapFunc 真实现——源码行 771/782
  核实）。处置 = 维持 munmap + (a) 注释如实标注再映射 flake 风险；
  (b) 失败路径 msync 判别器：洞被再映射 → 诚实 TestResourceIssueSkip 而非
  假 SDC（本机实证 hole→-1/ENOMEM、mapped→0；框架允许 test_run 返回
  EXIT_SKIP：ret<EXIT_SUCCESS→thread_skipped，sandstone_run.cpp:790）。
  gem5 边界：SE 表 msync 无 handler→unimplementedFunc(fatal)，判别器只在
  真机测试的失败路径执行，双内核不镜像（如实声明）。
- **#19 成立**：fexp/f_exp 两份同构。修：hoist 到 Section 4 前一份
  （f_back/f_exp 命名统一；位图 golden m_gold/f_gold 同址 hoist；
  ffr_mem/ffr_back 向量同 hoist 供两处复用）。
- **#20 维持（驳回改动）**：onl/clen 是 origin/main 存量
  （sdc_numa.cpp:120 `FILE *onl`、sdc_stress_config.h:147 `size_t clen`）；
  本 PR 基点 43c1f42e 早于其落地，lint 跑在 pull/305/merge ref 上才波及本
  PR。与 #328/#308 同款处置：main 存量、lint 承重、后续专项治理；回复如实
  承认全局 ignore-words-list 的残余盲区。

## 提交序列（一补丁一单元）— as-built

1. 5bf59051 #15 模板移入 arch guard（-U__aarch64__ 前后对照：6+ 模板硬
   错误 → 恰 3 条与兄弟文件同形 DECLARE_TEST 错误）
2. a3000d7b #16 ffr_fault_acc_capture 组合窗（objdump 0x8cc-0x8f4 与
   0xfdc-0x1004 两窗零 bl）
3. 3f0b9e77 #17 位图逐位 memcmp + 零指令编译屏障（GCC 12 DCE/CSE 实证
   修复）；gem5 四档双内核 Phase7/8 全绿
4. e7d7cf3e #19 golden hoist（纯重构，先行以减 #18 churn）
5. dbadd354 #18 msync 判别器 + 风险注释（干扰路径端到端实证为诚实 SKIP）
6. 本文件 docs 提交

#20 维持（.codespellrc 不动）：onl/clen 为 origin/main 存量
（sdc_numa.cpp:120 / sdc_stress_config.h:147），本 PR 基点 43c1f42e 早于
其落地，CI 在 pull/305/merge ref 跑 codespell 才波及本 PR；与 #328/#308
同款处置，回复如实承认全局 ignore-words-list 残余盲区。

## 过程中发现（超出检视意见本身的实测事实）

- GCC 12.3.1 不为优化目的建模 SVE FFR：未消费 svldff1 被 DCE（最小复现
  setffr;(void)ldff1;rdffr 生成码无 ldff1d）；svrdffr 值可跨
  SETFFR/LDFF1/调用 CSE 成陈旧值。仓库测试因此加零指令编译屏障（每面
  rdffr 新鲜由反汇编核实）；双内核 capture 标 noinline（调用边界为唯一
  可靠 CSE 扑杀点）。
- 硬件与 gem5 的 LDFF1 语义分歧（acc 面 DATA）：Kunpeng 920 FFR 纯输出
  （FFR=partial 甚至 0 时映射加载仍全量装载，无调用污染探针双证）；
  gem5 把 FFR 建模为隐式输入掩码（sve_mem.isa fautlingLoadmemAccCode）。
  两侧一致的是映射加载不触 FFR——即测试所断言；acc 数据两侧均不断言。
- LDFF1 连 freshly-mmap 匿名页的 demand-paging minor fault 也抑制
  （MAP_FIXED 回填未触访的洞 golden 不破）——#18 干扰需驻留页才成立。

## 验证（最终态 dbadd354）

- 干净重建 meson setup + build -j 32：460/460 零新警告。
- 本机 SVE-256：-e sve512_pred_ops_arm -t 2000 -n 1 → pass；
  -t 5000 全核 → pass；-e zstd19 -t 2000 -n 1 → pass；绿路径 94 次
  执行零误触发。
- gem5-fi 四档 --sve_vl {1,2,4,8}：SDCSHIELD_FAILS=0，checksum
  8c11e0ffbf395643 / 1d7e3c5a7226d67d / 103da6e36ab42579 /
  632f0ef689858821（本机 cddfd6b5d5776a78）。
- x86 非回归（检视）：全部改动在 #ifdef __aarch64__ 内；-U__aarch64__
  语法检查 3 错与兄弟文件同形（目录既有设计）。

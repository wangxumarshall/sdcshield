# PR #336 五轮检视处置（r5，head 703752ae）— 2026-10-31

local-claude 两批（06:08 / 06:25）重复发同 3 位点共 7 条意见（5/6/7 与
9/10/11 同位点互补），合并为 3 主题 + codex ×2 信息性。导出件
/tmp/pr336-review-r1.md（不可信数据，证据已自行核实）。

## 证据核实结论（全部本仓/本机实证）

- **意见 5+9 之 (a) 成立（Important）——gate 条件宽于干扰语义**：
  :454 gate 进入门为 `ffr_bad || data_bad || acc_bad`（任一），:457 SKIP
  条件为 `remapped && all_passed`；而 :423-426 注释自述干扰语义是
  「the fault never happened, ALL THREE goldens break at once」。环境可
  产生的坏集实测/推导只有三种：
  - 非驻留落洞：LDFF1 连 minor fault 也抑制（r3 实测 MAP_FIXED 回填未
    触访洞 golden 不破）→ 坏集 = **空集**（gate 根本不进）；
  - 驻留零填页落洞：无 fault → f_img/a_img 全 1（ffr_bad+acc_bad），
    数据尾读 0 == f_exp → data_bad **可假**；
  - 驻留脏页落洞：三面全坏。
  关键不变量：**环境解释 ⇒ f_img == m_gold（全 1）∧ a_img == m_gold**。
  「acc 面单独坏 + 非驻留落洞」可达且环境解释不了：本机实测
  /tmp/msync_probe.c —— 非驻留 MAP_FIXED 回填页 **msync=0**（VMA 判定
  非驻留判定），probe 判 remapped，acc 面从堆 ffr_mem 加载（:807/:905，
  与洞无关）真 SDC 被吞，且消息谎称 fault source invalidated（fault 面
  根本没坏）。修法采纳并加强：SKIP 条件从「ffr_bad 至少为真」（检视
  者建议）升级为「无故障全一签名」——f_img 与 a_img 都逐位等于 m_gold。
  推导覆盖：data 单坏（f_img 保持 partial ⇒ fault 发生过 ⇒ 非环境）、
  acc 单坏（a_img ⊂ partial ≠ 全 1）、ffr partial-坏（位被清过 ⇒
  fault 发生过）均不再被环境解释吞掉；仅「全 1 签名」可归因环境。
- **意见 5+9 之 (b) 成立（Important）——per-worker SKIP 聚合缺口**：
  框架路径逐一核实：sandstone_run.cpp thread_runner cleanup
  `ret < EXIT_SUCCESS → new_state = thread_skipped`（:788-793）；
  父侧权威聚合 logging.cpp AbstractLogger 构造（:1712-1771）——
  any has_failed → Failed，`pc == sc` 仅全员 SKIP 才 Skipped，**混合
  skip+pass 保持 Passed**；fork 模式父读子 exit code（:1020
  test_result_from_exit_code）同为 0。检视者两个修法方向评估：
  - 「确定性全员同判」**不可实现**：洞状态是时变量（mid-run 事件），
    arm64_spec_prepare.h:62-76 先例成立仅因其判据（spec 探测）时间不
    变且在 worker 出现前判一次；已返回 EXIT_SUCCESS 的 worker 无法
    事后改判，框架无 worker 发起 test-level SKIP 的通道（实测
    TestResult 枚举 + 聚合代码无此路径）。
  - 「混合态走 FAIL」：任何 fail 线程 → 测试级 Failed，聚合缺口消失，
    但把**可证明的环境事件**（precise signature + msync 证明）报成
    FAIL = 假 SDC 告警，倒退 r3 #18 引入判别器的初衷。
  - **本轮采纳第三方向：per-worker 洞 + 再武装（re-arm）**。洞从
    init 共享资源改为 per-worker run-local（共享洞 re-arm 与兄弟
    capture 竞争——munmap 自有页 1 可令在捕获中的兄弟元素 0 SIGSEGV，
    指针换写又是 data race，全部不可取）；干扰处置 = 警告 + munmap
    自有页 1 + 武装新洞 + **继续压测**。runner 内不再有任何
    EXIT_SKIP：混合态、截断、聚合缺口一并消失；再武装同时把环境
    归因变成持续重验——新鲜洞（probe= intact）上无故障签名复现 ⇒
    上报 SDC，正向 TOCTOU 吞真窗口从「mapper 在 probe 前落地」收窄
    到「mapper 在 probe 前落地 **且** 连续落进每个新洞」（r4 #25 否决
    的是「重捕获→SKIP 吞已观测失配」的撤销极性；re-arm 是检测极性
    ——把失效资源换新继续测，之后任何失配照报——不冲突）。持续再
    映射（cap=8 次）说明主机病态 → report_fail 大声（消息明示非
    SDC），不许静默降覆盖。
- **意见 6+11 成立（Minor）——ffr_mapped 死字段**：grep 全文件
  `d->ffr_mapped` 在 run（:604-920）零引用；:143 声明、:589 赋值、
  :591-592 仅 init 局部填充用。处置：随 (b) 的洞搬迁**整体删除**
  （比降级为 init 局部变量更彻底——run 侧 mapped/acc 面本就从局部
  ffr_mem 加载）。
- **意见 7+10（Minor，新事实核实）——codespell 白名单**：根治分支
  fix/codespell-typos-2026-10-31 = **PR #337（WIP，OPEN，未合入）**，
  commit 09733668 基于 main tip ec8d254e（onl→online、clen→ctrl_len、
  re-uses→reuses，三处均 main 侧存量、非本 PR 改动集）；issue #333
  OPEN 跟踪专项。处置**维持**白名单三词：回退 = 把本 PR 的 merge-ref
  lint 耦合到 #337 先合入的次序（#336 先合则 main 树内三词仍在 →
  lint 红 → CI 阻塞）；#328/#308 树内同款三词，单方回退制造三 PR 不
  一致。#337 合入后三词条成为死重，由 #333 一次性协调清理（本 PR 的
  .codespellrc 注释已逐词标明来源文件/标识符，清理是机械的）。回复
  如实承认全局 ignore-words-list 残余盲区 + 指向 #337/#333。
- **codex ×2（意见 2/3）**：一次 review failed（usage limits）+ 一次
  usage-limits 提示，无技术内容，如实归类为信息性，无需处置。

## 提交序列（一补丁一单元，结构先行）

1. **洞搬迁**：ffr_pages/ffr_page_bytes/ffr_mapped/ffr_fault_k1 四字段
   从 PredOpsData 删除；新增 run-local `struct ffr_hole` + arm/disarm
   自有单元（#22 单页规则随所有权下放到 per-worker disarm）；
   ffr_hole_probe 改收 hole 参数。意见 6+11 随之闭环。
2. **gate 无故障全一签名**：ffr_check_fault_acc_faces 增 m_gold 参数，
   SKIP 条件改 `remapped && f_img==m_gold && a_img==m_gold && all_passed`；
   remapped-但非签名走警告 + SDC 判决；注释改写（环境坏集三形态、
   零填驻留页 data_bad 可假、acc 单坏不可环境解释）。
3. **SKIP→再武装**：runner 废除 EXIT_SKIP（gate 返回
   `{faces_ok, env_interference}`）；ffr_hole_rearm 单一单元（警告 +
   换洞 + cap=8 → report_fail 非 SDC 明示）；洞/probe/文件头注释改写
   （聚合缺口实证、#25 不冲突论证、TOCTOU 收窄推导）。
4. 本文件 docs 提交。

## 验证计划

- 每提交：meson setup（PKG_CONFIG_PATH=./third-party/eigen5）+
  build.sh -j 32 零新警告；`-e sve512_pred_ops_arm -t 2000 -n 1` pass。
- **当前 head 缺陷实证**（临时补丁，验证后回退，不入库）：单 worker
  强制 mid-run EXIT_SKIP → -n 4 观察测试级 **Passed**（混合态被聚合
  吞）——意见 9 的经验证明；acc 面单字节毒化 + 非驻留 MAP_FIXED 回填
  洞 → 观察 SKIP 吞真 SDC——意见 5(a) 的经验证明。
- **修复后行为**：同两场景 → 前者不再存在 SKIP（无混合态可言），后者
  report_fail（SDC 判决）；驻留页落洞 → 警告+re-arm+压测继续+pass；
  持续落洞 → cap → report_fail 非 SDC 明示。
- **gem5-fi 红线**：双内核 sve_ffr_pr305_dual.c（a7eb3617，捕获/黄金
  语义未动）四档 --sve_vl {1,2,4,8} 重跑——SDCSHIELD_FAILS=0、四档
  checksum 与 a7eb3617 记录一致 = 健康路径不受搬迁影响的回归证据；
  判别/gate/re-arm 为失败路径且 msync 在 gem5 SE 表 unimplementedFunc
  （r3 实测），只能本机实证（边界如实声明）。
- zstd19 -t 2000 -n 1 → pass；x86 非回归（检视：全部改动在
  #ifdef __aarch64__ 内）。

## as-built 验证实录（最终 head d1f92ce7）

- **当前 head 双缺陷实证**（临时补丁，已回退）：
  - 意见 9：4 线程 run 单 worker 强制中途 SKIP → `exit: pass`，SKIP
    仅在日志；且 Passed 态使 fracture 循环持续重跑（3s 内 147 段，
    每段恰 1 worker skip、结果 pass）。
  - 意见 5(a)：毒化 a_img[20] + 非驻留 MAP_FIXED 回填 → `result:
    skip`「fault source invalidated ... not an SDC」+ `exit: pass`
    ——真 acc-SDC 被吞实锤。
- **修复后行为**（临时补丁，已回退）：
  - 同 5(a) 场景 → warning「not the no-fault signature — the fault
    DID happen」+ acc 面 report + 「SVE predicate-network SDC
    detected」→ `exit: fail`（干净失败）。
  - R1 一次性驻留落洞 → warning「re-arming a fresh page pair (1/8)
    and continuing the stress」→ 段内后续迭代全绿 → `result: pass`。
  - R2 持续落洞 → 第 9 次 `report_fail`「re-mapped 9 times this run
    — persistently invalidates the fault source (not an SDC)」。
- 干净重建（meson setup + build.sh -j 32）：零 warning/error。
- 本机 SVE-256：`-e sve512_pred_ops_arm -t 2000 -n 1` → pass；
  `-t 5000 -n 16` → pass；`-e zstd19 -t 2000 -n 1` → pass。
- **gem5-fi 四档重跑**（gem5.opt @ a7eb3617 双内核，源未动）：
  `--sve_vl 1` 8c11e0ffbf395643 / `--sve_vl 2` 1d7e3c5a7226d67d /
  `--sve_vl 4` 103da6e36ab42579 / `--sve_vl 8` 632f0ef689858821，
  全部 SDCSHIELD_FAILS=0——四 checksum 与 r4 记录逐一相同，健康路径
  FFR 捕获/golden 契约在 per-worker 洞搬迁+签名门+re-arm 后逐位
  不变；本机双内核同绿 cddfd6b5d5776a78（亦与 r4 相同）。gate/re-arm
  失败路径 gem5 边界（SE 表 msync=unimplementedFunc）如实声明，本机
  实证如上。
- x86 非回归：`-U__aarch64__ -fsyntax-only` 错误数 1 = 兄弟文件
  sve512_fcmla_arm.cpp 同款 1（目录既有 DECLARE_TEST 形态），无新增；
  .codespellrc 零改动（意见 7+10 维持处置），改动文件 codespell 自检
  零输出。

## 过程中发现（超出检视意见本身的实测事实）

- **非驻留再映射页 msync=0**（/tmp/msync_probe.c，三态实测：intact
  hole → -1/ENOMEM；非驻留回填 → 0；驻留回填 → 0）——msync 判 VMA
  存在性而非驻留性，意见 5(a) 的「acc 单坏 + 非驻留落洞」可达性前提
  由本机测量坐实。
- **存量框架缺陷（非本 PR 引入，将另行立 issue）**：report_fail 后
  偶发 SIGSEGV（RIP=thread_runner cleanup sandstone_run.cpp:798 读
  sApp->shmem，FAR=0x7e4f940006d 恒定、任何子进程 maps 中不存在）。
  pristine 703752ae 同型「worker 内 mmap + report_fail」序列 3/3 run
  稳定复现（每 run 11 次）——report_fail_common 的
  PTHREAD_CANCEL_ASYNCHRONOUS 在取消点附近强制栈展开的寄存器恢复
  缺陷。SDC/失败消息先于 SEGV 落盘（检出不丢失），结果从 fail 退化
  为 crash。修复属框架侧（如改 DEFERRED 取消），超出本测试 PR 范围。

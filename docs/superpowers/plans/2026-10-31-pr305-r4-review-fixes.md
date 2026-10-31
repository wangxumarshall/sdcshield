# PR #305 四轮（收敛轮）检视处置（#22-#33）— 2026-10-31

head 348ec850。11 条意见归并 7 个独立主题，逐条本机/gem5 源实证后处置。
导出件 /tmp/pr305-review-r4.md（不可信数据，证据已自行核实）。

## 证据核实结论（全部本机/gem5 源实证）

- **#22 成立（Important）**：cleanup 无条件 `munmap(pg, 2*ps)`。残余风险注
  （:111-129）+ 实测事实（r3 计划文档：LDFF1 连 freshly-mmap 未触访页的
  minor fault 也抑制）⇒ 非驻留 mapper 落洞不破 golden ⇒ 绿路径可携带
  被占洞走到 cleanup，munmap 销毁他人映射，炸点远离肇因。修：只 munmap
  自有第一页；`ffr_pages_bytes`→`ffr_page_bytes`（单义：页大小，cleanup
  与判别器共用派生）。
- **#23/#31 成立（同主题）**：`msync==0→false；errno==ENOMEM→true` 二值折叠，
  任何第三态 errno（seccomp/LSM 的 EPERM/ENOSYS）落 false→SKIP——真 SDC
  可能被吞。本机探针实测该形状仅可达 {0, ENOMEM}，但检测器安全方向不应
  押在该实测上。修：判别器改三态 `ffr_hole_state{intact,remapped,unknown}`
  + errno 出参；仅**可证明**再映射（msync==0）才 SKIP，未知 errno 走 SDC
  判定并 log_warning 声明不可判别。
- **#24 成立**：Section 4 mapped 面（及 1-3 节，全部与洞无关）失败仅置
  `all_passed=false` 不终止；:586 EXIT_SKIP 在 :630 聚合判决前返回，失败
  记录被覆盖。re-loop 同面是 report_fail_msg 立即终止，无此问题——两份
  拷贝已实际漂移（#26 的实证）。修：gate 仅在 `all_passed==true`（无
  洞无关失败悬置）时才允许 SKIP；remapped 但有悬置失败 → log 洞为上下文，
  SDC 判决优先。
- **#25 成立（变通：声明边界 + 收敛论证，不做再捕获）**：判别器观测的是
  判别时刻而非捕获时刻，双向 TOCTOU 真实。用户态无原子 capture+probe。
  再捕获收敛方案**评估后否决**：干净的重捕获与「真瞬态 SDC」同样自洽，
  SDC 检测器不得以重跑吞掉已观测失配。修：判别器注释声明两方向 + 窗口
  量化 + 否决理由；SKIP 消息内声明探测时序（"probed AFTER the capture"）。
- **#26/#32 成立（同主题）**：:578-608 与 :709-740 三面校验+gate 两份同构
  手工拷贝（消息/终止策略已漂移）；另 mapped 面对 :531-544/:686-699 同病
  （检查顺序与消息前缀已漂移——#24 证据自认）。修：参数化为单一单元
  `ffr_check_mapped_face<Immediate>` + `ffr_check_fault_acc_faces<Immediate>`
  （`<false>`=Section 4 聚合语义，`<true>`=re-loop 立即终止语义），消息
  统一、顺序统一。**先重构后语义**（r3 e7d7cf3e→dbadd354 同序，减 churn）。
- **#27/#30 成立（同主题）**：`asm volatile("":::"memory")` 按语义只约束
  内存不约束寄存器驻留值，注释「forbids substituting an EARLIER capture's
  RDFFR value」不成立；「per-iteration golden comparison makes any
  staleness fail loudly」对 acc 面（golden 恰等 fault 面）与跨迭代 mapped
  面（恒期望全 1）是错的。**双内核已实测 GCC 12 连调用边界都 CSE
  svrdffr 值**（sve_ffr_pr305_dual.c ab8db6fa：Phase7 直排 one-shot 被
  Phase6 陈值假失败，故 capture_img/capture_pair 标 noinline——「调用
  边界为唯一可靠 CSE 扑杀点」）。当前仓库测试的 always_inline 恰缺该
  边界，新鲜性实靠「GCC 对 builtin 非 pure 建模 + 一次人工反汇编」。
  修：(a) 注释全部改如实表述；(b) 机械钉死：`ffr_ldff1_capture`
  always_inline→**noinline**——调用方从此不拼 svrdffr（无调用方级 CSE
  对象），且函数经指针实参写内存（非 pure，调用不可合并/不可外提循环），
  每面 RDFFR 必然执行；与双内核 r3 设计收敛。跨捕获间隙变为两条相邻 bl
  到本叶函数：bl 非 syscall、叶体非 libc，FFR 架构上存活（gem5
  capture_pair 同形 2000 迭代×4 档已证；真机由本机 acc 面 golden 实证）。
  屏障保留于体内，角色降格如实（窗口内存序，非 CSE 保证）。
- **#28 成立**：r3 计划文档 #20 处置 :32-36 与 :50-53 双写。修：删后者，
  单一权威陈述。
- **#33 维持（驳回改动，如实承认）**：onl/clen/re-uses 为 main 存量
  （sdc_numa.cpp:120 / sdc_stress_config.h:147），本 PR 基点 43c1f42e 后
  经 merge ref lint 混入。跨 PR 统一口径（#305/#308/#328 三 PR 同款处置），
  专项治理已立 **issue #333**（OPEN）。回复如实承认全局 ignore-words-list
  残余盲区 + 指向专项。

## 提交序列（一补丁一单元，重构先行）— as-built

1. f3357212 #22 cleanup 只 munmap 自有第一页（ffr_pages_bytes→
   ffr_page_bytes 单义化）
2. b2279d88 #26/#32 三面校验+gate+mapped 面参数化为单一单元
   （ffr_check_mapped_face / ffr_check_fault_acc_faces；纯结构重构）
3. 81b2bbbe #23/#31+#24+#25 判别器三态（ffr_hole_state）+ SDC 判决
   优先于干扰 SKIP + TOCTOU 双向声明与再捕获否决理由
4. 68a60a73 #27/#30 capture always_inline→noinline + 屏障注释如实降格
   + 跨捕获间隙纪律改述（两条相邻 bl 到自有叶）
5. bda6ccf5 #28 r3 计划文档 #20 双写去重
6. 本文件 docs 提交

## as-built 验证实录（最终 head bda6ccf5）

- 干净重建（rm builddir + meson setup + build -j 32）：460/460，
  零 warning/error。
- 本机 SVE-256：`-e sve512_pred_ops_arm -t 2000 -n 1` → exit: pass；
  `-t 5000` 全核 → exit: pass；`-e zstd19 -t 2000 -n 1` → exit: pass。
- 判别器三态探针实证（/tmp/hole_probe_r4.c，与 ffr_hole_probe 同式）：
  凿洞 → intact(errno=12/ENOMEM)；mapper 落洞 → remapped(msync=0)；
  mapper 撤离 → 回 intact（即反方向 TOCTOU 形——边界已在注释与 SKIP
  消息声明）。unknown 本机不可达（仅 {0,ENOMEM}，与 r3 实测一致），
  处理路径（log_warning + 走 SDC 判定）检视确认。
- noinline objdump 实证（-O3 目标文件）：全文件恰 2 条 rdffr——
  每 noinline capture 函数体（ILb1: setffr@0x4/ldff1d/rdffr@0x10/ret@0x20；
  ILb0: ldff1d/rdffr/ret）内恰 1 条、调用方零 rdffr；两窗体内零 bl；
  S4 与 re-loop 的 fault→acc 配对间隙仅 add/mov/str 零 bl；7 个调用点
  （S4×3 + re-loop×3 + GCC 剥离的循环首迭代×1）无一合并/外提。
- x86 非回归（检视）：.codespellrc 零改动；代码改动全在
  `#ifdef __aarch64__` 内（guard 外仅 PredOpsData 字段改名+注释，无
  arch 依赖内容）；`-U__aarch64__ -fsyntax-only` 错误数 1 = 兄弟文件
  sve512_fcmla_arm.cpp 同款 1（目录既有 DECLARE_TEST 形态），无新增。
- **gem5-fi 四档重跑**（gem5.opt @ gem5-fi a7eb3617，双内核含 r4 注释
  同步）：`--sve_vl {1,2,4,8}` 全部 SDCSHIELD_FAILS=0——
  128-bit vl=2 checksum 8c11e0ffbf395643；256-bit vl=4
  1d7e3c5a7226d67d；512-bit vl=8 103da6e36ab42579；1024-bit vl=16
  632f0ef689858821。四 checksum 与 r3（ab8db6fa）逐一相同——健康
  路径 FFR 捕获/golden 契约在四轮重构+noinline 收敛后逐位不变。
  本机同绿（cddfd6b5d5776a78）。gem5-fi 提交 a7eb3617 已推
  feat/sve-ffr-pr305-dual-2026-10-30。
- 判别器/gate 失败路径 gem5 边界（如实声明）：SE 表 msync=
  unimplementedFunc(fatal)，三态判别器与 gate 仅真机失败路径可达，
  双内核不镜像（r3 已声明，本次复核仍成立）；其本机验证为上述
  探针实证 + 检视。

## 遗留

- #33（.codespellrc）维持：onl/clen/re-uses 为 main 存量，跨 PR
  （#305/#308/#328）统一口径，专项治理 issue #333（OPEN）。

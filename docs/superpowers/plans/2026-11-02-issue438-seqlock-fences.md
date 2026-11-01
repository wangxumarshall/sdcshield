# 计划:MemFeedback seqlock 协议补全(issue #438)

日期:2026-11-02
分支:`fix/issue-438`(自 origin/main)
类型:bug 修复(并发/内存序)

## 1. 背景与现状

- issue #438(audit,针对 commit 09c94fc6):「反馈撕裂窗口闭合」在 ARM64 弱内存序下
  并未真正闭合——跨进程普通 store 无 release/acquire,读者仍可观察到
  `{hash=新, samples>0, eff_ratio=上一测试值}` 这一源序不可能状态。
- 09c94fc6 次日,main 已合入 77a705a2「MemFeedback 槽 seqlock 发布/读取」
  (PR #302 检视 #9/#20 的响应),把字段访问 seqlock 化。issue 引用的行号/代码
  是 09c94fc6 时点的;本 issue 的**实质批评**(混合态可达)需对当前 main 重新裁定。

## 2. 根因分析(对当前 main)

对照 canonical seqlock(内核 `write_seqcount_begin` 的 `smp_wmb` /
`read_seqcount_retry` 的 `smp_rmb`),当前实现缺**两条**序约束,任一条都独立
重开撕裂窗口。证据 = 线上 .o 的反汇编(builddir/framework/libframework.a.p/
sandstone_run.cpp.o,release 构建):

- **写侧缺口 W**:`sdc_feedback_write` 置奇是普通 `str`(relaxed),与后续字段
  store(hash/eff/samples)之间无任何屏障(仅发布偶数用 `stlr`/release)。ARM64
  不保证其他观察者按源序观察不同地址的 store——字段 store 可先于置奇传播。
  现注释以「字段 store 被提前时读者见到的仍是完整(新或旧)快照」为由用 relaxed,
  该论证不成立:字段 store 之间同样无序,可**部分**提前,读者见到混合态。
- **读侧缺口 R**:`sdc_feedback_eff_median` 两次 `ldar` 读 seq,但字段是普通
  `ldr`,且第二次 `ldar` 之前无屏障。LDAR 只约束其**后**的访问——s2 可在字段
  加载完成前完成绑定,采纳的字段可晚于 seq 对应的写入。

两缺口任一即可让读者接受 `{hash=新, samples>0, eff=旧}`(issue 的禁止态):
W:hash/samples store 先于置奇传播而 eff store 未传播;R:s2 早于字段绑定,
eff 绑定早于复位、hash 绑定晚于新 hash 发布。风险有界(median+15% 死区
+0.7× 锚),与 issue 定级 Important 一致。

## 3. 修复方案(最小改动,一个单元)

按 canonical seqlock 补两条屏障,并修正注释里两处不成立的论证:

1. 写侧 `sdc_feedback_write`:置奇(relaxed store)之后、字段更新之前加
   `__atomic_thread_fence(__ATOMIC_RELEASE)`(= 内核 `write_seqcount_begin`
   的 `smp_wmb`;aarch64 上 GCC 发出 dmb)。
2. 读侧 `sdc_feedback_eff_median`:字段加载之后、s2 重读之前加
   `__atomic_thread_fence(__ATOMIC_ACQUIRE)`(= 内核 `read_seqcount_retry`
   的 `smp_rmb`;GCC 发出 `dmb ishld`)。
3. 注释:删除「读者见到的仍是完整(新或旧)快照,语义无损」的不成立论证;
   「共享字段的重排混合态不再可能进入 median」在补屏障后改为如实成立的主张;
   引用内核对应函数名与屏障语义。

不变项:结构体布局、字段访问指令(str/ldr)、单写者/多读者模型、T9 源序双保险、
被杀写者恢复(|1)、s2 复检与有效性判据全部保留。改动全部位于
`#if defined(__aarch64__) && defined(__linux__)` 门内,x86-64 不编译此段
(非回归:纯 ARM64 门内修改)。

设计取舍(备选方案与否决理由):
- 只把置奇改为 seq_cst store:依赖 STLR 的后续序语义,不如内核型 fence 直白;
  否决。
- 字段全部改 std::atomic:改变 fork 共享结构体的布局/ABI 且 std::atomic 跨进程
  MAP_SHARED 上形式语义存疑;现方案(屏障 + 既有字段访问)与内核 seqlock 的
  plain-access+barrier 型完全同构;否决。C++ 形式模型下字段普通访问的 UB 已是
  PR#302 检视 #9 披露过的 pragmatic 口径,本修复不扩大也不重复立项。

## 4. 验证(CLAUDE.md 规则 2)

- **复现(已完成,硬件级)**:litmus harness(/tmp/issue438/litmus438.c,不进仓库)
  逐指令复刻线上 .o 的访问型(布局 hash@48/eff@56 line A,samples@64/seq@68
  line B;写侧 str 置奇→字段 store→stlr 置偶;读侧 ldar→3×ldr→ldar;FIXED=1
  编译对照验证 dmb/dmb ishld 发出)。跨进程 MAP_SHARED,写者核 0 / 读者核 96
  (跨 NUMA),每配置 30 s,5 个写者延迟档:

  | delay_ns | 前修复 V1(跨测试撕裂) | 前修复 V2(同测试混合) | 修复后 V1 | 修复后 V2 |
  |---|---|---|---|---|
  | 0    | 39     | 118  | 0 | 0 |
  | 50   | 73,250 | 0    | 0 | 0 |
  | 200  | 53,123 | 0    | 0 | 0 |
  | 1000 | 12,099 | 0    | 0 | 0 |
  | 4000 | 5,274  | 0    | 0 | 0 |

  前修复合计 143,903 次源序不可能快照被接受(delay=0 与 50 档的 V1 eff 极值
  端点为奇数——**恰好观察到 issue 禁止态 `{hash=新, eff=上一测试值}`**);
  修复后同型 5 档共 2.0×10⁹ 次读取、8.46×10⁸ 次采纳,0/0。
- **构建**:`./scripts/build.sh` 干净,零新增警告(实测:3 targets,无输出)。
- **反汇编(实测)**:修复后 .o 读侧 `ldar s1 → 3×ldr → dmb ishld → ldr s2`;
  写侧 `str 置奇 → dmb ish → 字段 store → stlr 置偶`。
- **受影响行为(实测)**:`-e eigen_svd_double -t 3000 -n 1 -v` →
  `[联合求解] eigen_svd_double.mdim: n_mem=9222 n_run=1215 AR=10.0 c=138.4 → 1215`
  (读侧执行;首跑无同 test 槽,反馈 median 无生效行——预期)+ 尾部
  `[反馈] eigen_svd_double: VmHWM=198.4 MiB / (n=1215²×8B×1线程) → eff_ratio=17.620
  (EMA α=0.5, 样本=1)`(写侧执行)+ `exit: pass`。边界如实:正向反馈消费
  (反馈生效行)需同 test 二次 fracture,单跑不出现;读路径协议已由 harness
  A/B 闭环。
- **回归(实测)**:`-e zstd19 -t 2000 -n 1` → `exit: pass`;
  `-e eigen_gemm_double_dynamic_square -t 2000 -n 1` → `exit: pass`;
  `-e eigen_svd_double -t 3000 -n 1` → `exit: pass`。
- **x86-64 非回归**:改动在 `#if defined(__aarch64__) && defined(__linux__)`
  门内,x86-64 不编译此段(检视声明,纯门内修改)。

## 5. 提交与交付

- 单 commit:`fix(framework): MemFeedback seqlock 补 canonical 两屏障——置奇后 wmb/复检前 rmb (issue #438)`,DCO `-s`。
- push `fix/issue-438`,开 PR(base main,body 含根因/改动/验证证据,`Closes #438`),
  报告贴 issue #438。

## 6. 文档同步

architecture.md/README 对反馈闭环只描述 EMA/median/0.7 锚(本修复不改任何已
文档化行为,非大颗粒度);协议主张全部在代码注释中就地修正。本计划文档即
docs/ 侧工件。

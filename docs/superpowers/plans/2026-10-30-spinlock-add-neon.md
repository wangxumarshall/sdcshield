# spinlock_add_neon 实施计划（qspinlock 硬锁死检测用例）

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 新增 sdcshield 测试 `spinlock_add_neon`，闭合"qspinlock 硬锁死（锁 cacheline 一致性故障→锁字冻结/丢写→挂死）无检测"的缺口。

**Architecture:** 三检测通道并行：①NEON vfma 计算结果 vs init 同源 golden（数据 SDC）；②全局锁保护共享计数器的 Σ 逐线程校验（数据行写丢失）；③获锁等待 >10s 看门狗 + 锁行 64B dump（锁字冻结/丢写→硬锁死签名，**核心增量**——现有 lock/spinlock 用例遇此故障只会无声挂死）。负载形态：每线程 NEON 实算（[-1,1] 随机 float）+ 全局 CAS 自旋锁深争用。

**Tech Stack:** C++20（sdcshield 框架）、ARM NEON intrinsics（arm_neon.h）、GCC 12.3、meson。

**Spec:** 用户指令（2026-10-30）：新建 `spinlock_add` 文件夹；sdcshield 格式（init 生成固定随机数据所有线程共享 + golden 预计算 + run 比对）；随机输入、NEON 类数据、浮点 [-1,1]；日志保留每次验证的输入/输出/结果，绿 pass 红 fail；多核结果不受程序逻辑影响；给出完整代码/路径/编译/单核与多核 10s 无日志测试指令。现象依据：qiling-vmcore 五案诊断（中行/国债 qspinlock 死锁 = 锁行一致性元数据瞬态故障）。

## Global Constraints

- CLAUDE.md 规则 1：one patch per unit（测试本体一个 commit，计数同步一个 commit）
- CLAUDE.md 规则 2：build.sh 零新告警 + 真实运行引用输出 + zstd19 回归 + x86 非回归（新文件 aarch64 门控）
- CLAUDE.md 规则 3：`git commit -s`，feature 分支（feat/spinlock-add-neon），不推 main
- CLAUDE.md 规则 5/6：x86 不受影响（meson `host_machine.cpu_family() == 'aarch64'` 门，spinlock_unaligned 先例）；非 aarch64 构建不含本用例（检视处置 2026-10-30：原「log_skip 占位」分支在门控下永不编译，已删除——见 PR#328 检视 :760 处置）
- 检测器自检纪律（coherence_probe SELFTEST_W 同款）：新机部署必跑 selftest 两通道
- 机器现状约束：-O0/-O3 两臂实验占用 cpuset 0-123 至 ~14:44 —— 验证运行全部钉到 124-127 空闲核，收官后再补全核跑

---

### Task 1: 测试本体 + meson 接线

**Files:**
- Create: `tests/cpu/spinlock_add/spinlock_add_neon.cpp`（完整代码见 Step 1）
- Modify: `tests/cpu/meson.build`（spinlock_unaligned aarch64 块之后追加新块，~753 行后）

**Interfaces:**
- Consumes: sandstone.h（DECLARE_TEST/test_time_condition/random32/report_fail_msg/log_error）、test_knobs.h（get_testspecific_knob_value_int/string——`-O testid.key=value` 键按测试 id 定域，须用 testspecific 变体，裸键查不到；PR#328 三轮检视 :31 修正：原稿误写 get_test_knob_value_int，且 log_skip/CpuNotSupportedSkipCategory 在首轮检视 :760 处置中随 arch 死分支删除——非 aarch64 靠 meson 门控不编译，无 SKIP 消费）、arm_neon.h（vld1q_f32/vfmaq_f32）
- Produces: 注册测试 `spinlock_add_neon`；knobs：`logverif`（0/1，默认 0）、`selftest`（0=off/1=stall/2=data）；日志 `/var/tmp/spinlock_add_neon_<pid>.log`（仅 logverif=1 时创建）

**设计定案（实现者须逐条遵守）：**
- 锁行结构 `alignas(64)` 恰 64B：`lock_word(0=free/1=held) | epoch | holder | magic | pad[4]`——dump 格式对齐五案"8 字节窗口、同行其余字段完好"指纹
- 内核：4 组 × 4 向量 × 4 轮 vfma 链（`a=vfmaq(a,b,c)` 轮换），输入 |x|≤1 时输出理论上界 ~3e20（<<FLT_MAX、有限、确定性、无 NaN 路径；探针实测 10^6 slot max|out|≈1.36e20——原稿 ~1806 系错误数学，首轮检视 :304 已修正代码注释，此处同步）
- 输入映射：`(float)((int32)random32() / 2147483648.0)` ∈ [-1,1)，init 一次性生成 SLOTS(128)×64 float，所有线程只读共享
- 获锁：`__atomic_compare_exchange_n(..., __ATOMIC_ACQUIRE, __ATOMIC_RELAXED)` 自旋 + `yield`；等待 >10.0s（对齐 sbsa-gwdt）后进入 1s 确认窗两段式判定（PR#328 四轮检视 :157）：确认窗内锁行 epoch 停走且 CAS 持续失败 = 确认冻结 → dump 全行 + report_fail；epoch 增长或 CAS 成功 = 锁在正常流转、本线程被调度饥饿 → log 告警 + 重置等待预算继续争用（不 FAIL，防共享机高负载假阳性），跨 3 个窗口仍无获锁以进展异常 FAIL（真冻结时 epoch 必停走——其唯一递增路径在持锁者临界区内，不引入新假阴性）
- 释放：临界区内 `shared_counter++/local_counts[slot]++/holder/epoch++` 后 `__atomic_store_n(lock_word,0,__ATOMIC_RELEASE)`
- 多核逻辑安全：slot=cpu%128 输入只读、内核纯函数、out 线程栈私有、计数对（shared/local）同在临界区（窗口 < locks.cpp 先例）、日志 O_APPEND 单 write()
- selftest=stall：首个进临界区线程持锁 sleep(15)（须 > 看门狗 10s + 确认窗 1s + 裕量，否则确认窗内 CAS 成功被判为饥饿、阳性击穿——PR#328 四轮检视 :157）；前置条件（≥2 线程、-t ≥16000ms 派生下限、推荐 25000）由 init fail loudly 强制（PR#328 四轮检视 :219——单线程时唯一线程自睡自放、循环至 -t 绿 pass，通道 C 永不触发，假绿；线程数经 thread_count() 公开 API、生效时长经 sApp->current_test_duration 校验）；selftest=data：init 对各 slot golden[0] 位级 XOR 最低尾数位（原设计 `+=1.0f` 在 |x|≥2^24 时舍入后位型不变、毒化静默失效——首轮检视 :304 已改位级毒化，此处同步）
- 日志行：`iter=N cpu=C in=[4 floats] out=[4 floats] result=\033[32mPASS\033[0m`（失败 \033[31mFAIL）；每线程上限 10000 行后仅记失败（防爆炸）；失败行永不受限
- 非 aarch64：不编译（meson aarch64 门控；检视处置删除了原设计的 log_skip 占位死分支）

- [x] **Step 1: 创建测试文件**（完整代码，见下方代码块——本计划的权威版本）

- [x] **Step 2: meson 接线**（aarch64 门控块，spinlock_unaligned 之后）

- [x] **Step 3: 轻量构建**（两臂实验期间 -j8，不扰 cpuset 0-123）
  Run: `ninja -C builddir -j 8` → 新 TU 零告警、链接成功

- [x] **Step 4: 功能验证（钉 124-127 空闲核）**
  - 单核无日志: `./builddir/sdcshield --cpuset=124 -n 1 -e spinlock_add_neon -t 10000` → exit: pass
  - 多核无日志(4核): `./builddir/sdcshield --cpuset=124,125,126,127 -e spinlock_add_neon -t 10000` → exit: pass（多核逻辑安全实证）
  - 自检 data: `... --cpuset=124 -n 1 -e spinlock_add_neon -t 10000 -O spinlock_add_neon.selftest=data` → **FAIL + 红 FAIL 行**（通道 A 阳性）
  - 自检 stall: `... --cpuset=124,125,126,127 -e spinlock_add_neon -t 30000 -O spinlock_add_neon.selftest=stall` → **FAIL + 停滞 dump**（通道 C 阳性）
  - 带日志: `... -O spinlock_add_neon.logverif=1` → /var/tmp 日志含绿 PASS 行

- [x] **Step 5: 两臂实验收官后（~14:44 后）补全核验证**
  - `./builddir/sdcshield -e spinlock_add_neon -t 10000`（128 核无日志）→ pass
  - `./builddir/sdcshield -e spinlock_add_neon -t 10000 -O spinlock_add_neon.logverif=1`（单核带日志样本）

- [x] **Step 6: 回归 + 计数**
  - `./builddir/sdcshield -e zstd19 -t 2000 -n 1` → exit: pass
  - `./builddir/sdcshield --list-tests | wc -l` → 480（2026-10-30 检视处置期复测：PROD 480 / --quality=0 486 / --quality=-1 491；原记录 479 系当时手数错误，docs commit 6da0f83b 的 480 为实测真值）

- [x] **Step 7: Commit 1**
  `git add tests/cpu/spinlock_add/spinlock_add_neon.cpp tests/cpu/meson.build && git commit -s -m "feat(spinlock_add): ..."`

### Task 2: 计数同步（CLAUDE.md/README）

- [x] `--list-tests` 计数写入 CLAUDE.md（SVE 主机口径行）与 README 对应行 → commit 2（-s）——已完成于 6da0f83b（写入 480 实测口径）；检视处置（PR#328 意见 7/8/9）补齐其遗漏：README 合计行 500→501、脚注 479→480 与 363 行口径统一

---

## Self-Review

1. **Spec 覆盖**：新文件夹✓ sdcshield 格式(init 随机共享数据/golden 预计算/run 比对)✓ 随机 NEON [-1,1]✓ 逐次日志绿/红✓ 多核安全✓ 完整代码/路径/编译/测试指令（执行后交付）✓ 现象针对性(10s 看门狗+64B dump+Σ校验)✓ 第三方库：用户消息截断（"它不用第三方库的代码如下："后无内容）——本单元取自包含 NEON 内核，第三方库（openblas/sleef）升级留作后续单元待用户重发草稿确认。
2. **占位符扫描**：无 TBD/TODO；所有常量/API/路径具体。
3. **类型一致性**：SpinlockAddNeonData 字段、kernel/acquire/release 签名在 Task 1 内自洽。

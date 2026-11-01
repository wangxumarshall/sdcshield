# Issue #323: mesh_upi_avx512_symm_f64 — sum snapshot 竞态 + 恒真断言 修复计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 消除 `mesh_upi_avx512_symm_f64` 的跨轮 global_sum 累加竞态（假 FAIL 风险）与恒真的 `consistent` 断言（零数据检出力），并补齐 #414 家族协议修复（sizing + 等待节流）——本文件是 mesh FP 族唯一漏掉 #414 的成员。

**Architecture:** 三提交序列：(1) 移植 #414 的四源 sizing + 131072 自旋一档等待节流（竞态触发面的削减 + 家族对齐 + 可验证性前提）；(2) sum 校验闭式化——期望值改为缓冲内容的确定性数学常量（`block_value` 是 `(block,j)` 的纯函数，总和有闭式），彻底删除跨轮累加器 `global_sum`，使校验结构性无竞态且检出力严格增强；(3) 写侧逐 lane 立即读回硬 FAIL（#332 缺口 B 先行项模式，先例 `avx512_sym` 的 `block_ok`）+ 读侧 `consistent` 拆分为诚实命名的 `fma_ok`/`store_ok` 并给出精确失败语义。

**Tech Stack:** C++20 / std::atomic(seq_cst) / ARM NEON `float64x2_t` / sdc_stress_config.h knob 体系（`sdc_knob_int_clamped`、`sdc_budget_bytes`、`sdc_total_threads`、`sdc_quick_run`）。

**Spec:** GitHub issue #323（两点论断）+ 编排者边界：不重构 sym 族校验语义（#332 的活，仅本文件）；gem5-fi 规则不适用（NEON f64 本机原生可跑，无 HWCAP 缺口）。

## 诊断结论（已实证，实施依据）

### claim 1（consistent 恒真）——属实
`tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp:263-268`：`fma_chain(vals[j])` 与
`fma_chain_scalar(vals[j])` 消费**同一个** `vals[j]`（刚从共享缓冲载入的值）。
内存内容损坏对两臂是同输入 → NEON 链与标量链永远一致 → 对数据损坏零检出。
它唯一能检的是 NEON-FMA 单元与标量加法流水线的**计算路径分歧**（合法但极窄，
文件头注释 :104-107 早已自认「检 FMA 计算路径分歧,不检内存内容」）。
:272-288 的 store_buf 自比较同理：比较的是私有栈副本 vs 其副本，对共享缓冲损坏
零检出（检私有 load/store 通路）。数据校验全靠聚合 sum——而 sum 被 claim 2 的
竞态破坏后连这仅有的校验都会误报。

### claim 2（sum 超时后不完整重置 → 下一轮损坏）——属实，机理链如下
框架语义（`framework/sandstone_run.cpp:624-642, 1935-1937`）：本测试
`fracture_loop_count=0`（默认）→ auto-fracture，`current_max_loop_count=40`
且 `inner_loop_count` 是**每线程**计数——`test_time_condition()` 返回 false
≠ 全局时间到，而是**该线程**自己的 40 次调用预算耗尽。本文件三个等待
（:235-238 写等待、:313-316 重置门、:327-330 锁等待）**每自旋**调用一次
`test_time_condition` → 预算在 µs 级烧穿 → 线程在轮协议中途异步死亡
（实测：128 MiB + `-t 5 -n 8` 下 8/8 线程在首轮等待中预算死亡 → 全体诚实
SKIP，零校验）。机理链（每步均有代码依据）：

1. 线程 T 在轮 N 的重置门（`read_done == total`）等待中预算死亡 →
   `:317-320` 只释放 `reset_lock`、**不重置任何计数器** → 脏轮次：
   `next_block` 已耗尽、`write_done==num_blocks`、`global_sum` 为轮 N 全量、
   `read_done` 陈旧值 k < total。
2. 幸存者（读循环无时间检查、预算未耗——恰是刚读完的慢线程）过锁等待进入
   轮 N+1：写循环瞬时退出（`next_block` 耗尽）→ 写等待瞬时通过
   （`write_done` 陈旧完成态）→ 快照 = 陈旧 global_sum → 读全缓冲 →
   恰好相等（内容轮不变）→ 过，`read_done`++。
3. 陈旧 `read_done` 被幸存者的递增推过 `total` → 下一个到达重置门的线程
   **提前过闸** → reset 落在其他线程读/写中途（`next_block` 归零）。
4. 归零后：旧代认领者（reset 前已 fetch_add、reset 后仍在写）与新代认领
   （从 0 重新派发）覆盖同一块 → 该块的 local_sum **双重累加**进 reset 后
   的 `global_sum`；旧代 `write_done`++ 同样给新代计数垫背 → 写门提前开。
5. 后续快照取到膨胀的 `global_sum`，读侧走全缓冲 → `read_sum != expected`
   → `report_fail_msg` → **幽灵 SDC 误报**（本工具最恶劣的失效模式）。

墙钟超时不能触发此链（过期后 do-while 对所有线程退出，无人进入下一轮）；
触发源正是每线程预算死亡——而无节流等待使它成为常态。修复前对照复现
（本机 84 次短跑：n∈{4,8} × buf∈{64,96,128}MiB × t∈{1,2,3}）未中奖
（窗口是时序彩票），按编排者预案以机理链论证 + 修复后多轮长跑无假阳性
收口。

### 修复为什么是结构性的（而非概率缓解）
内容确定性（`block_value` 纯函数、逐轮值不变）+ 期望值与参与者无关
（闭式常量）⇒ 任何破窗的轮协议交错（早开闸、mid-write reset、双重写）
下缓冲内容恒为确定性值 ⇒ `read_sum == expected_total` 只可能因**真实损坏**
失败。闭式期望同时是编排者「部分线程 SKIP 时期望怎么算」的终极答案：
期望值与参与线程数无关，每线程独立对全缓冲校验，无需按参与者重算或降级。
检出力严格增强：旧校验「读和==写和」对写前 vals 损坏双盲（两臂同错），
闭式校验对写路径/读路径/求和路径损坏均单边可见。

## Global Constraints

- 一个单元一个 commit（CLAUDE.md 规则 1），`git commit -s` 尾行
  `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By。
- 每 commit 前：`SDC_BUILD_JOBS=20 ./scripts/build.sh` 零新警告 + 受影响
  行为实跑引用真实输出。
- x86-64 不触碰：本文件 `#ifdef __aarch64__` 专属，`#else` 占位不动。
- 不重构 sym 族校验语义（#332 范围）；不扩到其他 mesh 文件。
- 绝不 push main；PR `Fixes #323`。
- 大缓冲验证用 env/knob 限流，不把本机推进 swap（61 GiB 物存、并发 agent）。

---

### Task 1: 移植 #414 协议修复到 f64 孪生（四源 sizing + 131072 等待节流）

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp`
- Modify: `docs/SDC_STRESS_CONFIG_CN.md`（NEON mesh FP 表从「对」扩为三文件）

**Interfaces:**
- Consumes: `framework/sdc_stress_config.h` 的 `sdc_knob_int_clamped(test,key,def,lo,hi)`、
  `sdc_env_knob_int_clamped(test_id,key,&v,lo,hi)`、`sdc_budget_bytes(permille)`、
  `sdc_total_threads()`、`sdc_quick_run()`。
- Produces: `static size_t mesh_target_bytes(struct test *test)`（四源自适应，
  返回 0 = 预算耗尽 → init 诚实 SKIP）；run 内三处节流等待 + `round_aborted` 标签。

- [x] **Step 1.1: 头部 include 与注释重写**

`#include <sandstone.h>` 块后增加 `#include "sdc_stress_config.h"`（在
`#ifdef __aarch64__` 内，与 sym 孪生同位）。删除 :95-113 的旧 sizing 注释段
（"内存消耗目标…"整段，其描述的等待病理由本任务修复），替换为 sym 孪生
:32-59 的注释段（孪生同步，"双文件"改"三文件"，补一句「f64 孪生由 #323
补齐，#414 当时只改了 sym 对」）。

- [x] **Step 1.2: 替换 mesh_target_bytes 为四源版**

删除现 :114-128 的 `static size_t mesh_target_bytes(void)`，替换为 sym 孪生
:60-142 的逐字节同款（签名改 `(struct test *test)`，函数体不变——所有日志/
env 名均由 `test->id` 派生，自动正确）。init 内 `mesh_target_bytes()` 调用改
`mesh_target_bytes(test)`，并按 sym :154-163 补 `target == 0` 的诚实 SKIP：

```c
    size_t target = mesh_target_bytes(test);
    if (target == 0) {
        log_skip(TestResourceIssueSkipCategory,
                 "%s: four-source memory budget is zero (cgroup limit / "
                 "commit headroom exhausted); cannot size a meaningful "
                 "mesh buffer", test->id);
        return EXIT_SKIP;
    }
```

- [x] **Step 1.3: 三处等待换 131072 节流 + round_aborted 标签**

写等待（原 :235-239）替换为：

```c
        /* 等待所有块写完成：131072 自旋一档节流预算检查（sym 孪生同款，
         * #357 病理的修复）——原「每自旋都调 test_time_condition」把 40 次
         * auto-fracture 迭代预算在 µs 级烧穿，等待方在填充完成前提前退出，
         * 该轮校验被整轮跳过；节流后耐心 ~40×131072 自旋（≈0.1-0.3 s），
         * 等待条件本身每自旋都查，响应不受影响。 */
        {
            uint32_t spin = 0;
            while (td->write_done.load(std::memory_order_seq_cst) < td->num_blocks) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }
```

（原写等待后的 `if (!test_time_condition(test)) break;` 删除——节流等待自身
已覆盖超时，读循环允许在过期后完成本轮，见 sym 孪生 :292-298 注释。）

重置门胜者（原 :313-320）与锁等待败者（原 :327-331）同款节流，胜者超时
路径保持 `reset_lock.store(0)` 后 `goto round_aborted`。do-while 之后加：

```c
round_aborted:
    if (!validated_any) {
```

（原 `if (!validated_any)` 前的 `}` 位置调整，SKIP 文案不变。）

- [x] **Step 1.4: 构建零警告 + 行为验证**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh
./builddir/sdcshield -e mesh_upi_avx512_symm_f64 -t 30 -n 8 -v
```

期望：构建零新警告；`[压力映射] mesh_upi_avx512_symm_f64.buffer_bytes = …`
值日志出现；`result: pass`（不再是 8/8 SKIP——四源 sizing 下默认缓冲
clamp ≤1 GiB ÷ 线程，轮次亚秒级，真实校验多轮）。

- [x] **Step 1.5: SDC_STRESS_CONFIG_CN.md 同步**

「NEON mesh FP 对」小节标题与首行改为三文件
（`mesh_upi_avx_sym / mesh_upi_avx512_sym / mesh_upi_avx512_symm_f64`），
补一句「symm_f64 由 #323 补齐（#414 当时只改了 sym 对）」。

- [x] **Step 1.6: Commit**

```bash
git add tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp docs/SDC_STRESS_CONFIG_CN.md
git commit -s -m "port(mesh): symm_f64 孪生补齐 #414 协议修复——四源 sizing + 131072 等待节流

#414 (PR #414) 只改了 avx_sym/avx512_sym 对，mesh_upi_avx512_symm_f64 是
mesh FP 族唯一漏掉成员:三处等待每自旋调 test_time_condition,40 次
auto-fracture 每线程预算 µs 级烧穿,线程在轮协议中途异步死亡(实测 128 MiB
+t5+n8 下 8/8 线程首轮预算死亡全体 SKIP,零校验)。移植 sym 孪生的四源
安全预算 sizing(knob buffer_bytes + env + clamp [64MiB,1GiB] ÷ 线程)与
131072 自旋一档等待节流。这是 #323 sum 竞态的触发面削减(每线程预算死亡
是脏轮次的唯一入口)与可验证性前提(默认 sizing 下轮次须在 -t 内完成,
sum 修复才可端到端验证)。

验证:构建零新警告;-t 30 -n 8 -v: [压力映射] buffer_bytes 值日志 +
result: pass(修复前同配置 8/8 SKIP);84 次短跑扫描无回归。"
```

---

### Task 2: sum 校验闭式化——删除跨轮 global_sum 累加器

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp`

**Interfaces:**
- Produces: `TestData::expected_total`（double，init 一次写入、此后只读——
  无竞态）；`block_value` 不变（#332 引用它作家族先例，签名勿动）。

- [x] **Step 2.1: 头部文档 Global sum 段重写（:24-31）**

替换为闭式口径：期望总和是缓冲内容的确定性函数（`block_value` 纯函数、
每块 4096 元素恰 128 个完整 32 残差周期 → 每块和与块号无关 = 33792.0，
总期望 = 33792 × num_blocks，任意物理可分配缓冲下 < 2^53 精确可表示）；
读侧逐元素累加与期望常量字节比对；无跨线程/跨轮累加状态 → 结构性无竞态；
且比旧「读和==写和」严格更强（写前损坏双盲 → 单边可见）。

- [x] **Step 2.2: TestData 与 init 改造**

`struct TestData` 删 `std::atomic<uint64_t> global_sum;`，增
`double expected_total;`。init 中删 `td->global_sum.store(0, …);`，增：

```c
    /* 期望总和闭式基线:块值是 (block, j) 纯函数,每块 4096 元素恰 128 个
     * 完整 32 残差周期 → 每块和与块号无关。全部为 0.5 倍数、总幅 < 2^51,
     * 任意加法序下精确无舍入(与读侧累加同构,顺序无关)。 */
    double per_block = 0.0;
    for (int j = 0; j < BLOCK_SIZE; ++j)
        per_block += block_value(0, j);
    td->expected_total = per_block * (double)td->num_blocks;
```

- [x] **Step 2.3: 写循环删累加、读循环换期望源**

写循环删 `local_sum` 累加、整段 CAS 重试循环（:216-229）及其注释块。
读循环删快照 `expected_sum_bits`（:241-248，含其历史注释——该快照竞态
随累加器一起消失）与 `memcpy(&expected_sum, …)`，改：

```c
        bool sum_ok = (read_sum == td->expected_total);   // 双侧精确,顺序无关
```

失败日志改 `"thread %d read_sum=%a expected=%a (deterministic golden)"`，
`report_fail_msg` 文案改 `"cross-core FP sum mismatch vs deterministic golden"`。

- [x] **Step 2.4: 重置段删 global_sum 清零**

胜者四连 store 删 `td->global_sum.store(0, …);`（其余三个计数器保留——
轮屏障机制仍需）。

- [x] **Step 2.5: 构建零警告 + 验证**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh
./builddir/sdcshield -e mesh_upi_avx512_symm_f64 -t 30 -n 8 -v
for i in 1 2 3 4 5; do ./builddir/sdcshield -e mesh_upi_avx512_symm_f64 -t 2 -n 8; done
```

期望：pass；`grep -n global_sum tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp`
仅余注释性历史引用（代码引用清零；实测全文 5 处，全为注释——检视轮 2
修正：原文「grep -c = 0」字面不实）。
（Step 4 做竞态针对性扫掠。）

- [x] **Step 2.6: Commit**

```bash
git add tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp
git commit -s -m "fix(mesh): symm_f64 sum 校验闭式化——消除跨轮 global_sum 累加竞态 (Fixes #323 claim 2)

global_sum 是跨轮共享累加器,其生命周期依赖重置门 read_done==total;线程
在门上每线程预算死亡后只释放锁不重置(脏轮次),幸存者的递增把陈旧计数推过
total → 门提前开 → reset 落在旧代认领者写中途 → 同一块新旧两代双重累加
→ 后续快照取到膨胀和,读侧全缓冲比对 → 假 FAIL(幽灵 SDC)。

修法:期望值闭式化。block_value 是 (block,j) 纯函数,每块 4096 元素恰 128
个完整 32 残差周期 → 每块和恒 33792.0,总期望 = 33792×num_blocks(0.5 倍数,
<2^51,任意序精确)。读侧 read_sum 与该常量字节比对:无跨线程/跨轮状态,
结构性无竞态;内容逐轮不变 + 期望与参与者无关 ⇒ 任何破窗交错下只会因真实
损坏失败。与 #325/#331 口径协调:部分线程 SKIP 不影响期望值(参与者无关),
validated_any 诚实 SKIP 语义不变。检出力严格增强:旧「读和==写和」对写前
vals 损坏双盲,闭式校验单边可见。#332 的家族 golden 重构方向(per-block
可重算公式)与本修法同构,本文件公式保持不动。

验证:构建零警告;-t 30 -n 8 pass;-t 2×5 无假阳性;grep global_sum = 0。"
```

---

### Task 3: 写侧逐 lane 读回硬 FAIL + 读侧恒真断言诚实化

**Files:**
- Modify: `tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp`

**Interfaces:**
- Produces: 写侧 `block_ok`（逐 lane 立即读回，硬 FAIL）；读侧 `fma_ok` /
  `store_ok`（诚实命名的分立判定）。

- [x] **Step 3.1: 头部文档读写阶段描述更新（:17-23）**

写阶段补「每块存储后立即逐 lane 读回比对，硬 FAIL（#332 缺口 B 先行项
模式，先例 avx512_sym）——本文件唯一的字节级写入内容校验」；读阶段改述为
三检：闭式聚合和（内容）、FMA 链 vs 标量 golden（计算路径分歧——**非**
数据校验）、store_buf 自比较（私有 store/reload 通路）。

- [x] **Step 3.2: 写侧加立即读回（store + __sync_synchronize 之后）**

```c
            /* 写侧立即读回:本文件唯一逐 lane 写入内容校验(#332 缺口 B 的
             * 低成本高价值先行项模式,先例 avx512_sym 的 block_ok)——同线程
             * 紧随 own store,字节级抓 store/load 通路损坏,确定性强。 */
            bool block_ok = true;
            for (int v = 0; v < VECTORS_PER_BLOCK; ++v) {
                float64x2_t written = vld1q_f64(vals + v * NEON_VECTOR_SIZE);
                float64x2_t loaded = vld1q_f64(td->data + offset + v * NEON_VECTOR_SIZE);
                uint64x2_t cmp = vceqq_u64(vreinterpretq_u64_f64(written),
                                           vreinterpretq_u64_f64(loaded));
                if (!(vgetq_lane_u64(cmp, 0) == 0xFFFFFFFFFFFFFFFFULL &&
                      vgetq_lane_u64(cmp, 1) == 0xFFFFFFFFFFFFFFFFULL)) {
                    block_ok = false;
                    break;
                }
            }
            if (!block_ok) {
                log_warning("mesh_upi_avx512_symm_f64: thread %d block %llu "
                            "immediate read-back mismatch at offset %zu "
                            "(store/load corruption)", id,
                            (unsigned long long)block, offset);
                report_fail_msg("mesh_upi_avx512_symm_f64: write-phase "
                                "immediate read-back mismatch (store/load "
                                "corruption)");
                return EXIT_FAILURE;
            }
```

- [x] **Step 3.3: 读侧 consistent 拆分与精确失败语义**

`bool consistent = true;` → `bool fma_ok = true;` + `bool store_ok = true;`
（分立置位/分立 break，注释分别声明检出边界：FMA 检计算路径分歧不检内存
内容（两臂同源输入）；store_buf 检私有通路不检共享缓冲）。判定与报错改：

```c
        bool sum_ok = (read_sum == td->expected_total);   // 双侧精确,顺序无关
        if (!sum_ok) {
            log_warning("mesh_upi_avx512_symm_f64: thread %d read_sum=%a "
                        "expected=%a (deterministic golden)", id, read_sum,
                        td->expected_total);
            report_fail_msg("mesh_upi_avx512_symm_f64: cross-core FP sum "
                            "mismatch vs deterministic golden");
            return EXIT_FAILURE;
        }
        if (!fma_ok) {
            log_warning("mesh_upi_avx512_symm_f64: thread %d NEON FMA chain "
                        "diverged from scalar golden (compute path, not a "
                        "data check)", id);
            report_fail_msg("mesh_upi_avx512_symm_f64: NEON FMA chain "
                            "diverged from scalar golden");
            return EXIT_FAILURE;
        }
        if (!store_ok) {
            log_warning("mesh_upi_avx512_symm_f64: thread %d private "
                        "store/reload self-consistency failure", id);
            report_fail_msg("mesh_upi_avx512_symm_f64: private store/reload "
                            "self-consistency failure");
            return EXIT_FAILURE;
        }
```

- [x] **Step 3.4: 构建零警告 + 验证 + Commit**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh
./builddir/sdcshield -e mesh_upi_avx512_symm_f64 -t 30 -n 8
git add tests/cpu/mesh/mesh_upi_avx512_symm_f64.cpp
git commit -s -m "fix(mesh): symm_f64 写侧逐 lane 读回硬 FAIL + 读侧恒真断言诚实化 (Fixes #323 claim 1)

旧 consistent 是恒真断言:FMA 链与标量 golden 消费同一个刚载入的 vals[j],
内存内容损坏对两臂同输入 → 永远一致,零数据检出(唯一检出力是 NEON-FMA vs
标量流水线计算分歧);store_buf 自比较同理只覆盖私有通路。两者却共用
「consistency failure」报错文案,误导为共享数据校验。

修法:(a) 写侧加立即逐 lane 读回硬 FAIL(先例 avx512_sym block_ok,#332
缺口 B 先行项)——本文件从此有字节级写入内容校验;(b) 读侧拆分 fma_ok/
store_ok 诚实命名,失败信息逐项精确到「计算路径分歧(非数据校验)」/
「私有通路自一致」/「确定性 golden 聚合和」,不再笼统 consistency。

验证:构建零警告;-t 30 -n 8 pass。"
```

---

### Task 4: 全量验证电池 + 收尾

- [x] **Step 4.1: 竞态针对性扫掠**（修复后无假阳性；对照修复前 84 跑）

```bash
fails=0; for n in 4 8; do for buf in 67108864 100663296 134217728; do \
  for t in 1 2 3; do for i in 1 2 3; do \
  SDC_MESH_BUFFER_BYTES=$buf ./builddir/sdcshield -e mesh_upi_avx512_symm_f64 \
  -t $t -n $n 2>&1 | grep -q mismatch && fails=$((fails+1)); done; done; done; done; echo fails=$fails
```

期望 `fails=0`。本测试无 RNG 消费（block_value 确定性），方差源是调度
时序——扫掠即多 seed 等价物，如实陈述。

- [x] **Step 4.2: 端到端 + 全核 + 回归**

```bash
./builddir/sdcshield -e mesh_upi_avx512_symm_f64 -t 30 -n 8 -v   # pass + [压力映射]
./builddir/sdcshield -e mesh_upi_avx512_symm_f64 -t 30           # 全核一次
./builddir/sdcshield -e zstd19 -t 2000 -n 1                      # exit: pass
```

- [x] **Step 4.3: x86 目检 + gem5 边界声明**

mesh f64 为 `#ifdef __aarch64__` 专属（`#else` 占位 SKIP 不动）→ x86 零
接触。NEON f64 无 HWCAP 缺口、本机原生可跑 → gem5-fi 规则不适用（如实陈述）。

- [x] **Step 4.4: push 分支 + PR `Fixes #323` + 检视 + merge + issue 收尾**

分支 `fix/issue323-mesh-symm-f64-sum-race`；PR 体含机理链与验证摘录；
code-reviewer 检视处置后 merge（编排者已授权）；issue #323 评论收口。

## Self-Review

- 覆盖：issue 两点论断 ↔ Task 2（claim 2）/ Task 3（claim 1）；#414 补齐 ↔
  Task 1（编排者情报纠偏：文件实际只含 #308/#331，不含 sizing/节流）；文档
  同步 ↔ Task 1.5 + 各 commit 的头部注释更新。
- 无占位符：全部代码块完整可落地。
- 命名一致：`expected_total`/`fma_ok`/`store_ok`/`block_ok`/`round_aborted`
  各任务间一致；`block_value` 签名不动（#332 依赖）。

## 执行后记（2026-11-01 收尾轮）

- **rebase**：分支重放到 origin/main（基线 d6dc42e7 → 04c60ac3 一线，4 commit
  重放为 64594dc1/eb744f58/91a00be5/9d8b6680），本 PR 两文件在 main 上零
  改动、零冲突；rebase 后全量重建 + 验证电池复跑全绿。
- **追加 commit 04c60ac3 ci(lint)**：main 侧 #419 引入的
  `scripts/check-bswap-golden.sh` 小写 `valu` 变量撞 codespell 字典，把每个
  后续 PR 的 lint（merge 树全树扫描）打红（#427 lint FAILURE 实测即此因，
  本 PR 未触碰该文件）。按 #337 先例重命名 `valu`→`vec_alu`（标识符重命名
  优于豁免清单），本地 codespell 2.4.3 全树 0 hit，CI lint 复绿。
- **观察（不入本 PR，报编排者）**：main 侧 `tests/cpu/mesh/mesh_upi_sse_sym.cpp:202`
  有既有 `%u` vs `uint64_t` 格式串警告（#414 系列带入，全量干净重建实测唯一
  警告，非本 diff 引入——本文件 TU 强制重编零警告）。

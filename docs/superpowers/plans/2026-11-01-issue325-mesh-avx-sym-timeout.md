# Issue #325: mesh_upi_avx_sym / avx512_sym 超时修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** mesh_upi_avx_sym / mesh_upi_avx512_sym 在 -t 到期时自行退出并 pass（不再被战役外部 timeout 杀成 Timed Out），内存量在 sizing 预算内，mesh 互连压力语义（全线程全量读的访问模式 + 流量总量）保持。

**Architecture:** 根因是 sizing 语义错配而非迭代粒度：读阶段每线程全量读取，每轮聚合读流量 = 线程数 × 缓冲字节数，而默认 sizing（物存 45%，9b7ee76c knob-ize 引入）只按 2 路战役内存均摊计、不含线程数因子——128 线程 × 57.6 GiB = 7372 GiB/轮 ≈ 74 s，读循环无时间检查，-t 无法打断。修法 = 默认 sizing 改为「读流量总量恒定」口径（45% 物存 ÷ 线程数，再 1 GiB 绝对上限收口），辅以等待自旋的 131072 一档预算节流（#357 病理的 NEON 侧修复，SVE 侧先例）与 fprintf 失败才打印（avx512_sym 先例）。显式 knob/env 保留大足迹压力能力。

**Tech Stack:** C++20（tests 内 C 风格）、meson/ninja、sdc_stress_config.h 自适应 sizing/knob 体系。

**Spec:** GitHub issue #325（战役超时报告）+ 编排卡（本文件同目录上下文）。相关但不越界：#357（mesh 族大缓冲根治跟踪）、#332（读阶段校验缺口重构，后续批）、#324（sse_sym 的 fprintf，另行立案）。

## Global Constraints

- 一个逻辑单元一个 commit：本卡 = 一个 commit（两孪生文件 + 文档 + 本计划）。
- `git commit -s`，尾行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By；只 push 特性分支，**绝不 push main**。
- 构建 `PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release` → `SDC_BUILD_JOBS=20 ./scripts/build.sh`（共享服务器限并发），零新增警告。
- **本机内存红线**：61 GiB / 可用 ~27 GiB，禁止默认 sizing（27.7 GiB）的修复前全量复现——用 `SDC_MESH_BUFFER_BYTES` 限流取证；修复后默认 ≤1 GiB，安全。
- 回归 `-e zstd19 -t 2000 -n 1` → `exit: pass` 不过不 commit。
- x86-64 非回归：改动全部在 `#ifdef __aarch64__` 内，`#else` 占位与 meson 源列表不动，目检确认。
- 本机为 NEON 主机，被测行为原生可跑，无需 gem5-fi。

## 取证结论（实现者必读）

1. **57.6 GiB 来源**：`tests/cpu/mesh/mesh_upi_avx_sym.cpp`（修复前 :56-70）与 `mesh_upi_avx512_sym.cpp`（:46-60）的 `mesh_target_bytes()` = `sysinfo.totalram × 0.9 / 2` = 物存 45%；128 GiB 战役主机 → 57.6 GiB（issue 数字精确吻合 128×0.45）。
2. **超时机制**：读阶段 `for (i = 0; i < total_elements; ...)`（avx_sym 修复前 :241-256）每线程全量读、无 `test_time_condition`；每轮聚合读流量 = 128 × 57.6 GiB = 7372.8 GiB（issue 的 7372 GiB 即此算术）；~74 s/轮。战役外部 `timeout -k 30 <-t/1000+15>`（scheduler.py，PR #347 记载）先于框架自身 SIGQUIT 宽限 `max(5×-t+30s, 300s)`（framework/sandstone.cpp:185-187）杀进程 → "Timed Out (3min)"。
3. **判定：sizing 语义错配为主**。非 #365 型「每线程误分配全尺寸」（缓冲是 init 一次性共享分配）；是「全线程全量读」设计（mesh 互连压力的访问模式）被 9b7ee76c 的 45% 物存 sizing 推出设计包络（原始 Intel 设计是 KB 级 L1/L2 缓冲，#357 明载）。粒度修法（读循环加时间检查）单独不可行：`test_time_condition` 每调用烧 1 次 auto-fracture 迭代预算、上限 40 次（fracture_loop_count=0，framework/sandstone_run.cpp:1935-1937），频繁检查会把好轮截断成废轮。
4. **伴生缺陷（本卡范围内，issue 点名）**：等待自旋每圈调 `test_time_condition`（avx_sym :221-224、:289-293；avx512_sym :185-188、:250-253、:265-268）→ 40 次预算 µs 级烧穿 → 等待方在填充完成前提前退出 → 该轮零校验（#357 病理；SVE 侧 mesh_upi_sve_sym.cpp:227-236 已修，NEON 侧漏）。**不修此项则 sizing 修好后轮次仍走不完整**。avx_sym 每轮每线程 fprintf PASS 行（:261-268）——sizing 修好后轮次亚秒级、数百轮 × 全线程会灌爆 stderr/memfd（#324 同款）；avx512_sym 已是失败才打印（其 :225-240 先例）。

## 修法设计

### sizing（两文件同款）

```
默认 = min( sdc_target_workset_bytes(900, 2 × thread_count()), 1 GiB )
```

- **读流量总量恒定**（#365 论证方式）：每轮聚合读流量 = 线程数 × 缓冲 ≈ 45% 物存（与修复前每轮填充同一量级字节搬移；61 GiB 主机 128 线程 → 缓冲 216 MiB，每轮聚合 ~27 GiB ≈ 0.3-0.6 s）。
- **1 GiB 绝对上限**：低线程（-n 2/8）与超大内存主机的轮时长/分配安全收口；足迹仍远超 LLC（走 DRAM/mesh 路径）。
- **访问模式不变**：全线程全量读、块认领填充、每块立即回读，一概不动；显式 `-O <test>.buffer_bytes` / env 不受 1 GiB 上限约束（大足迹压力可显式申请，其超时语义由 #357 族级根治跟踪）。
- knob 与 SVE 侧同名同 clamp：`buffer_bytes` ∈ [1 MiB, 16 TiB]（`1ll<<44`，SVE 侧 36 处一致；文档原写 4TiB 是错的，顺手修正）；env 链 `SDC_MESH_UPI_AVX_SYM_BUFFER_BYTES`（avx512 同理）→ 旧脚本兼容 `SDC_MESH_BUFFER_BYTES` → 自适应。
- quick 模式（CI 冒烟）：`sdc_target_workset_bytes` 内部收缩到 16 MiB，min 后仍 16 MiB，正确。

### 等待节流（两文件，avx_sym 2 处 / avx512_sym 3 处）

SVE 侧 mesh_upi_sve_sym.cpp:227-236 同款：131072 自旋一档调 `test_time_condition`，失败 `goto round_aborted`（落入既有 validated_any 检查：零完整轮 → 诚实 SKIP，有 → SUCCESS）；耐心 ~40×131072 自旋 ≈ 0.1-0.3 s。等待条件本身每自旋查，响应不受影响。省掉原「等待退出后再调一次」的重复预算消耗。

### fprintf（仅 avx_sym）

搬进 `if (!passed)` 失败分支（表达式形态保持 avx512_sym 先例的 `passed ? GREEN : RED`，避免 GREEN 宏未用告警），注释注明 #324 同款与先例。

### 预算经济学（为什么这样够）

每轮消耗预算 ≈ 填充等待 2-4 次 + 屏障等待 0-2 次 + do-while 1 次 ≈ 4-8 次；40 次预算 → 每 fracture 段 5-10 轮（~3-5 s），段重启重跑 test_init（重分配+memset ≤1 GiB ≈ 数十 ms，~1-2% 开销）。-t 到期：等待/do-while 处 ≤1 轮（≤~1 s）内退出；读循环无检查的越界量 = 单轮时长，默认 sizing 下亚秒级，远小于战役外杀的 15 s 余量。

## File Structure

- Modify: `tests/cpu/mesh/mesh_upi_avx_sym.cpp`（sizing 函数+注释重写、init 调用点、2 处等待节流、fprintf 收口、读阶段注释）
- Modify: `tests/cpu/mesh/mesh_upi_avx512_sym.cpp`（sizing 函数+注释重写、init 调用点、3 处等待节流、读阶段注释）
- Modify: `docs/SDC_STRESS_CONFIG_CN.md`（§5.2 补口径；§6 表加 NEON mesh FP 对行；修 SVE 行 4TiB→16TiB）
- Create: 本计划文档

---

### Task 1: 基线构建 + 修复前行为取证

**Files:** 无代码改动；产出基线二进制与 before 证据。

- [x] **Step 1: 干净构建**

```bash
cd <worktree>
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
SDC_BUILD_JOBS=20 ./scripts/build.sh
```

Expected: 构建成功、零警告（记录输出供对比）。

- [x] **Step 2: before 取证（限流 1 GiB，避开默认 27.7 GiB 的 swap 红线）**

```bash
SDC_MESH_BUFFER_BYTES=1073741824 ./builddir/sdcshield -e mesh_upi_avx_sym -t 30000 -n 8 2>&1 | tail -40
SDC_MESH_BUFFER_BYTES=1073741824 ./builddir/sdcshield -e mesh_upi_avx512_sym -t 30000 -n 8 2>&1 | tail -40
```

Expected（模型预测，如实记录实际）：8 线程中仅 0-2 条 `result=PASS` 校验线（等待自旋预算烧穿，#357 病理），测试早于 -t 结束；记录 exit code、runtime、实际 PASS 线数。默认 sizing 的规模论证（45% 物存 = 27.7 GB、×128 线程 = 3.5 TiB/轮）以算术引用 issue 数据，不实跑。

### Task 2: mesh_upi_avx_sym.cpp 修复

**Files:** Modify `tests/cpu/mesh/mesh_upi_avx_sym.cpp`

**Interfaces:** `static size_t mesh_target_bytes(struct test *test)`（新增 test 参数）；label `round_aborted:`（函数尾、validated_any 检查前）。

- [x] **Step 1: 头部加 include**

```c
#include <sandstone.h>
#include "sdc_stress_config.h"
```

- [x] **Step 2: 重写 mesh_target_bytes 及其边界声明注释**

旧注释块 + 函数（`// 内存消耗目标: ...` 至 `return (size_t)1 << 30;   // 1 GiB 兜底\n}`）整体替换为：

```c
// 目标字节数: -O mesh_upi_avx_sym.buffer_bytes=N /
// SDC_MESH_UPI_AVX_SYM_BUFFER_BYTES 环境变量 / 旧脚本兼容
// SDC_MESH_BUFFER_BYTES / 自适应默认(下)。与 SVE 侧 mesh_upi_sve_sym
// 的同名 knob 语义一致(issue #325 修复引入)。
//
// 自适应默认口径(issue #325):本用例读阶段是「每线程全量读取」——每轮
// 聚合读流量 = 线程数 × 缓冲字节数。原默认(物理内存 45%,9b7ee76c
// knob-ize 引入)只按 2 路战役内存均摊计,不含线程数因子:128 线程 ×
// 57.6 GiB(128 GiB 主机)= 7372 GiB/轮 ≈ 74 s,读循环内无时间检查
// (检查频度撑不起 40 次 auto-fracture 迭代预算),-t 到期无法打断当轮
// → 战役外部 timeout(scheduler.py: timeout -k 30 <-t/1000+15>)先于
// 框架 SIGQUIT 宽限 max(5×-t+30s, 300s) 杀进程 → Timed Out 误报。
// 修法 = sizing 语义修正:默认缓冲 = 45% 物存工作集 ÷ 线程数(读流量
// 总量恒定——每轮字节搬移与修复前同量级,足迹仍远超 LLC 走 DRAM/mesh
// 路径,「全线程全量读」访问模式不变),再以 1 GiB 绝对上限收口低线程/
// 超大内存主机的轮时长与分配安全;显式 knob/env 不受该上限约束(大足迹
// 压力仍可显式申请,其超时语义由 #357 族级根治跟踪)。
static size_t mesh_target_bytes(struct test *test) {
    int64_t v = sdc_knob_int_clamped(test, "buffer_bytes", 0,
                                     1 << 20, 1ll << 44);
    if (v == 0)
        sdc_env_knob_int_clamped(test->id, "buffer_bytes", &v, 1 << 20, 1ll << 44);
    if (v > 0)
        return (size_t)v;
    /* 旧脚本兼容: SDC_MESH_BUFFER_BYTES 直接指定字节 */
    if (const char *e = getenv("SDC_MESH_BUFFER_BYTES")) {
        char *end = nullptr;
        unsigned long long b = strtoull(e, &end, 10);
        if (end && *end == '\0' && b > 0)
            return (size_t)b;
    }
    /* 读流量总量恒定: 45% 物存 ÷(2 路战役均摊 × 线程数),1 GiB 轮时长上限 */
    uint64_t budget = sdc_target_workset_bytes(900, 2 * thread_count(), nullptr);
    constexpr uint64_t cap = 1ull << 30;
    return budget < cap ? budget : cap;
}
```

- [x] **Step 3: init 调用点**

`size_t target = mesh_target_bytes();` → `size_t target = mesh_target_bytes(test);`

- [x] **Step 4: 填充后等待改节流**

旧：
```c
        // 等待所有线程完成写入
        while (td->allocated_blocks.load(std::memory_order_seq_cst) < td->num_blocks &&
               test_time_condition(test)) {
            __asm__ volatile("yield");
        }
        if (!test_time_condition(test)) break;
```
新：
```c
        /* 等待所有线程完成写入：131072 自旋一档节流预算检查（SVE 侧
         * mesh_upi_sve_sym 同款，#357 病理的 NEON 侧修复）——原「每自旋
         * 都调 test_time_condition」把 40 次 auto-fracture 迭代预算在
         * µs 级烧穿，等待方在填充完成前提前退出，该轮校验被整轮跳过；
         * 节流后耐心 ~40×131072 自旋（≈0.1-0.3 s），等待条件本身每自旋
         * 都查，响应不受影响。 */
        {
            uint32_t spin = 0;
            while (td->allocated_blocks.load(std::memory_order_seq_cst) < td->num_blocks) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }
```

- [x] **Step 5: 读阶段注释补充（读循环仍不加时间检查的理由）**

在 `// ---------- 阶段 2：所有线程读取完整数组并验证 ----------` 注释后追加：
```c
        // （读循环内不查 test_time_condition：40 次迭代预算撑不起按向量
        // 检查的频度，预算耗尽的假「超时」会把好轮截断成废轮；轮时长已
        // 由默认 sizing 钉在亚秒级，显式大缓冲的超时语义由 #357 跟踪。）
```

- [x] **Step 6: fprintf 收口（每轮每线程 → 仅失败）**

旧（`bool passed = ...` 起至 `validated_any = true;` 前）：
```c
        // 输出结果（每个线程输出一次）
        fprintf(stderr, "mesh_upi_avx_sym: Thread %d, data[0..3]=(%.4f,%.4f,%.4f,%.4f), read_sum=%f, expected_sum=%f, consistent=%d, result=%s%s%s\n",
                id,
                td->data[0], td->data[1], td->data[2], td->data[3],
                read_sum, expected_sum, consistent,
                passed ? GREEN : RED,
                passed ? "PASS" : "FAIL",
                RESET);
        fflush(stderr);

        if (!passed) {
            report_fail_msg("mesh_upi_avx_sym: Sum mismatch or consistency failure");
            return EXIT_FAILURE;
        }
```
新：
```c
        if (!passed) {
            /* 失败证据（原先每轮每线程都打印 PASS 行——修复 sizing 后轮次
             * 亚秒级、数百轮 × 全线程会把 stderr/memfd 灌成噪音，#324 在
             * sse_sym 立案同款问题；此处改为仅失败时输出，avx512_sym 先例）。 */
            fprintf(stderr, "mesh_upi_avx_sym: Thread %d, data[0..3]=(%.4f,%.4f,%.4f,%.4f), read_sum=%f, expected_sum=%f, consistent=%d, result=%s%s%s\n",
                    id,
                    td->data[0], td->data[1], td->data[2], td->data[3],
                    read_sum, expected_sum, consistent,
                    passed ? GREEN : RED,
                    passed ? "PASS" : "FAIL",
                    RESET);
            fflush(stderr);
            report_fail_msg("mesh_upi_avx_sym: Sum mismatch or consistency failure");
            return EXIT_FAILURE;
        }
```

- [x] **Step 7: 屏障等待改节流 + round_aborted 落点**

旧：
```c
        } else {
            while (td->epoch.load(std::memory_order_acquire) == my_epoch &&
                   test_time_condition(test)) {
                __asm__ volatile("yield");
            }
            if (!test_time_condition(test)) break;
        }
        ++my_epoch;

    } while (test_time_condition(test));

    if (!validated_any) {
```
新：
```c
        } else {
            /* 同款 131072 一档节流（先例 mesh_upi_sve_sym 的 epoch 等待） */
            uint32_t spin = 0;
            while (td->epoch.load(std::memory_order_acquire) == my_epoch) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }
        ++my_epoch;

    } while (test_time_condition(test));

round_aborted:
    if (!validated_any) {
```

- [x] **Step 8: 构建 + 冒烟**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh && ./builddir/sdcshield -e mesh_upi_avx_sym -t 5000 -n 8 2>&1 | tail -15
```
Expected: 零新增警告；测试 ~5 s 自行退出，`exit: pass`（或极端短 -t 下诚实 SKIP）。

### Task 3: mesh_upi_avx512_sym.cpp 修复

**Files:** Modify `tests/cpu/mesh/mesh_upi_avx512_sym.cpp`

**Interfaces:** 同 Task 2（`mesh_target_bytes(struct test *)`、`round_aborted:`）。

- [x] **Step 1-3:** 与 Task 2 Step 1-3 同款（include、mesh_target_bytes 重写——注释中 env 名换 `SDC_MESH_UPI_AVX512_SYM_BUFFER_BYTES`、knob 查询用本 test 的 `buffer_bytes`、init 调用点加 test 参数）。
- [x] **Step 4: 填充后等待（:184-189 旧码）改节流**

旧：
```c
        // 等待所有块写完成
        while (td->write_done.load(std::memory_order_seq_cst) < td->num_blocks &&
               test_time_condition(test)) {
            __asm__ volatile("yield");
        }
        if (!test_time_condition(test)) break;
```
新（同 Task 2 Step 4 的节流块，条件换 `td->write_done ... < td->num_blocks`）。
- [x] **Step 5: 重置阶段两处等待（:250-269 旧码）改节流**

锁持有者（`read_done < total` 等待）失败路径保留清锁再 goto：
```c
        uint32_t expected_lock = 0;
        if (td->reset_lock.compare_exchange_strong(expected_lock, 1,
                                                   std::memory_order_seq_cst,
                                                   std::memory_order_seq_cst)) {
            /* 131072 一档节流预算检查（同填充后等待） */
            uint32_t spin = 0;
            while (td->read_done.load(std::memory_order_seq_cst) < total) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) {
                        td->reset_lock.store(0, std::memory_order_seq_cst);
                        goto round_aborted;
                    }
                } else {
                    __asm__ volatile("yield");
                }
            }
            ...（原重置序列不动）...
        } else {
            uint32_t spin = 0;
            while (td->reset_lock.load(std::memory_order_seq_cst) != 0) {
                if ((++spin & 131071u) == 0) {
                    if (!test_time_condition(test)) goto round_aborted;
                } else {
                    __asm__ volatile("yield");
                }
            }
        }
    } while (test_time_condition(test));

round_aborted:
    if (!validated_any) {
```
（其余：读阶段加与 Task 2 Step 5 同款「不加时间检查」注释；fprintf 已是失败才打印，不动。）
- [x] **Step 6: 构建 + 冒烟**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh && ./builddir/sdcshield -e mesh_upi_avx512_sym -t 5000 -n 8 2>&1 | tail -15
```

### Task 4: 文档同步

**Files:** Modify `docs/SDC_STRESS_CONFIG_CN.md`

- [x] §5.2 末尾追加一段：NEON mesh FP 对（avx_sym/avx512_sym）默认口径 = 读流量总量恒定（45% 物存 ÷ 线程数）+ 1 GiB 上限，issue #325；显式 knob 不受限。
- [x] §6 表「SVE mesh」行后加：

```markdown
### NEON mesh FP 对（tests/cpu/mesh：mesh_upi_avx_sym / mesh_upi_avx512_sym）
| knob | 含义 | 范围 | 缺省 |
|---|---|---|---|
| `buffer_bytes` | 共享缓冲目标字节 | 1MiB~16TiB | min(45% 物存 ÷ 线程数, 1 GiB) |

兼容旧变量 `SDC_MESH_BUFFER_BYTES`（直接指定字节）。读流量总量恒定口径
（issue #325）：每轮聚合读流量 = 线程数 × 缓冲，默认钉在 45% 物存。
```
- [x] 修 SVE mesh 行 `1MiB~4TiB` → `1MiB~16TiB`（代码 36 处均 `1ll<<44`，文档原值错误）。

### Task 5: 全量验证 + 提交 + PR

- [x] **修复后对照（与 Task 1 before 同参）**：
```bash
./builddir/sdcshield -e mesh_upi_avx_sym -t 30000 -n 8 2>&1 | tail -20
./builddir/sdcshield -e mesh_upi_avx512_sym -t 30000 -n 8 2>&1 | tail -20
```
Expected: 默认 sizing（-n 8 → 1 GiB 上限生效，-n 128 → 216 MiB）下 ~30 s 自行退出 `exit: pass`；记录 runtime 与 maxrss（yaml `--dump-cpu-info` 不需要，用 `-Dlogging_format=yaml` 或 stderr 的 [压力映射]/maxrss 行）。
- [x] **全核一次**：`./builddir/sdcshield -e mesh_upi_avx_sym -e mesh_upi_avx512_sym -t 30000`（128 线程，双用例合计 ~2×216 MiB）→ 双 pass。
- [x] **-t 响应边沿**：`-t 2000 -n 8` → ≥1 完整轮 → pass（或 2 s 内确实走不完一轮时诚实 SKIP，如实记录）。
- [x] **显式 knob 生效**：`-O mesh_upi_avx_sym.buffer_bytes=536870912 -t 10000 -n 8` → maxrss 跟随 ~512 MiB。
- [x] **回归**：`./builddir/sdcshield -e zstd19 -t 2000 -n 1` → `exit: pass`。
- [x] **x86 目检**：`git diff` 确认全部 hunk 在 `#ifdef __aarch64__` 内；`#else` 占位与 `tests/cpu/meson.build` 源列表未动。
- [x] **commit（单 commit，含两文件 + 文档 + 本计划）**：
```bash
git add tests/cpu/mesh/mesh_upi_avx_sym.cpp tests/cpu/mesh/mesh_upi_avx512_sym.cpp docs/SDC_STRESS_CONFIG_CN.md docs/superpowers/plans/2026-11-01-issue325-mesh-avx-sym-timeout.md
git commit -s -m "fix(mesh): avx_sym/avx512_sym 超时修复——读流量总量恒定 sizing + 等待预算节流 (Fixes #325)"
```
（尾行 Signed-off-by 由 -s 生成，确认无 Co-Authored-By。）
- [x] push 特性分支、开 PR（`Fixes #325`）、code-review 检视、merge（若与后续 main 前进冲突，按编排卡避让纪律处理）。

## Self-Review

- 覆盖：issue 两文件（avx_sym + avx512_sym）✓；sizing 根因 ✓；fprintf 点名项 ✓（avx_sym 收口，avx512 已是先例形态）；等待节流是 sizing 生效的必要伴生修复 ✓；#332/#357/#324/SVE 侧明确不动 ✓。
- 占位符：无 TBD/「适当处理」；所有代码块给出完整新文本。
- 类型一致：`mesh_target_bytes(struct test *)` 两文件签名一致；`round_aborted` 标签语义一致（落入 validated_any 检查）；knob/env 名逐文件正确。
- 内存红线：before 取证限流 1 GiB；after 默认 ≤1 GiB（-n 128 时 216 MiB）。

## 检视轮处置（PR #414，第 5 bot 本地对抗检视 2026-11-01）

| 意见 | 定性 | 处置 |
|---|---|---|
| Important·正确性: 旧 env SDC_MESH_BUFFER_BYTES 裸通道无 clamp——`=1` 静默缩到 num_blocks 兜底 1 块 32 KiB 仍 pass，且 knob 哨兵行误导「未显式配置」 | 成立（bot 实测复现） | 已修：套 [1MiB,16TiB] clamp + 越界警告 + 值日志（两文件）；实测 `=1` → `已 clamp 为 1048576` |
| Important·正确性: 自适应默认无下限无值日志——小内存主机工作集缩进 LLC 量级内、欠压力不可见 | 成立 | 已修：下限 64 MiB（达 LLC 量级；让位于预算不越 OOM 边界）+ [压力映射] 值日志；注释改为诚实的「达 LLC 量级（跨核共享行 snoop/mesh 流量即互连压力语义）」而非「远超 LLC」 |
| Important·内存: 单源物存模型绕过四源预算 sdc_budget_bytes——cgroup 限额下 1 GiB cap 仍可超分 OOM（#310 前科） | 成立 | 已修：默认口径改 sdc_budget_bytes(900) 四源取 min ÷ (2×线程×slice)；实测全核默认随 MemAvailable 逐段微调（206178307→206162971 字节）。框架级 sdc_target_workset_bytes 的四源迁移波及 SVE mesh 18 文件，留 #357 族级批次 |
| Minor·正确性: 新旧 env 解析进制分叉（base 0 八进制 vs base 10） | 成立但框架级 | 不动：sdc_env_knob_int 是全仓 knob 通道既有语义，改解析影响所有消费者，超出本卡；PR 回复中明示 |
| Minor·可维护性: 计划文档 checkbox 与实际不同步 | 成立 | 已修：本轮补勾 |

第二轮验证（构建零警告）：全核双用例 -t 10 → 0 skip、exit: pass；-t 2 边沿 / -n 2 / 回归 zstd19 全 pass；`SDC_MESH_BUFFER_BYTES=1` → clamp 警告实测；默认值日志 `= 196.6 MiB（45%安全预算 ÷ 256 线程）` + maxrss 199.6 MiB 跟随。

## 检视轮 2 处置（PR #414 第 5 bot 第二轮，commit 4e4d4acd 上）

| 意见 | 定性 | 处置 |
|---|---|---|
| Important·内存: 64 MiB 下限在紧预算下抬到每测试整份预算,2×slices 并发实例合计超四源安全预算;budget=0 缩 1 块空转 | 成立 | 已修:下限改为 min(64MiB, budget÷(2×slices))——不超过本实例战役份额;budget==0 由 init 诚实 SKIP(TestResourceIssueSkipCategory) |
| Important·可维护性: 文档表「min(45%物存÷线程,1GiB)」与合入代码(四源+下限+旧 env clamp)不符 | 成立 | 已修:§6 行与 §5.2 段更新为最终语义 |
| Minor·正确性: 旧 env 负值经 strtoull 模 2^64 回绕成 ULLONG_MAX → 向上 clamp 16 TiB → memset OOM | 成立 | 已修:改 strtoll 解析、>0 为有效;负值/畸形串告警后走自适应(实测 -1/1GiB 两用例) |
| Minor·正确性: 畸形串静默落自适应无告警 | 成立 | 已修:同上告警 |
| Minor·可维护性: mesh_target_bytes 两孪生字节级复制 | 成立,合并属 #357 批 | 已加孪生同步注释(改必须双文件同步,#357 迁移时并入 sdc_target_workset_bytes 体系) |
| Minor·可维护性: 值日志「÷ %d 线程」打印的是 consumers(2×cur×slices) | 成立 | 已修:标签改「÷ 2(战役)×%d(线程)×%d(slice)」 |
| Minor·正确性: 读阶段新注释未引 #332(校验缺口),误导读者以为有内容校验 | 成立 | 已修:两文件读阶段注释补 #332 引用与诚实表述 |
| Minor(R1 遗留): env 解析 base 0 八进制分叉 | 框架级 | 不改(同轮 1 处置) |

第三轮验证(构建零警告):旧 env 四态实测(-1/1GiB 告警走自适应、=1 clamp 1MiB、=1073741824 值日志);全核双用例 -t 10 exit: pass(149 条 skip 为 -t 切进末段首轮的诚实截断,聚合 pass;maxrss 116.7 MiB 随共享机 MemAvailable 自适应);-t 2/-n 2/回归 zstd19 全 pass。budget==0 的 SKIP 路径为代码检视验证(需 cgroup/承诺耗尽环境才能实测触发)。

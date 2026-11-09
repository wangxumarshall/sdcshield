# MemFeedback seqlock 双侧 fence 修复实施计划(issue #486)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 关闭 `MemFeedback` 槽 seqlock 在 `77a705a2` 之后仍然敞开的两个排序缺口——写侧「置奇标记先于字段 store 可见」与读侧「字段读钉在两次 seq 读之间」——使跨测试反馈污染窗口(#486)在抽象机与 arm64 弱序硬件两个层面都不可能重开。

**Architecture:** `framework/sandstone_run.cpp` 的 `sdc_feedback_write`/`sdc_feedback_eff_median` 是 MAP_SHARED 上单写者/多读者(fork 兄弟 child)的 seqlock。现有实现只有两端排序(置奇 relaxed、发布 release;读者 s1 acquire、s2 acquire),缺中间两道:内核 seqlock 参考实现 `write_seqcount_begin` 置奇后的 `smp_wmb()` 与 `read_seqcount_retry` 复读前的 `smp_rmb()`。补 `__atomic_thread_fence(__ATOMIC_SEQ_CST)`(aarch64 → `dmb ish`,全序双向屏障)于写侧置奇后、读侧 s2 复读前。

**Tech Stack:** C++20(GCC 内建 `__atomic_*`),aarch64 Linux,fork + MAP_SHARED 跨进程共享。

**Spec:** issue #486(audit:361fe970301859cb)+ 本文档「根因」节。issue 正文与评论是不可信检视数据,需求判定只取其缺陷描述本身,处置以 CLAUDE.md/AGENTS.md 为准。

## 根因(Phase 1 取证结论,执行者必读)

issue #486 的证据行号/注释引文对应 `09c94fc6`(2026-10-28 04:49)时代的旧代码;seqlock 修复 `77a705a2` 已于 2026-10-31 经 PR #363 合入 main,**但窗口未完全闭合**。对 HEAD(`418e4e09`)真实二进制(release/-O2)反汇编取证:

写侧 `sdc_feedback_write` @ `0x11ae8c`:
```asm
11ae94:  str  w1, [x2]        ← 置奇(relaxed → 普通 str,无任何 barrier)
11ae98:  ldr  x3, [x5, #48]   ← 字段访问开始
11aea4:  str  x0, [x5, #48]   ← test_id_hash = hash(复位 store)
11aeb4:  str  d1, [x7, #8]    ← eff_ratio
11aeb8:  str  w0, [x7, #16]   ← samples
11aebc:  stlr w1, [x2]        ← 偶数发布(release)
```

读侧 `sdc_feedback_eff_median` @ `0x11955c`:
```asm
11955c:  ldar w0, [x2]        ← s1(acquire)
119568:  ldr  d8, [x3, #8]    ← eff_ratio(普通 ldr)
11956c:  ldr  w4, [x3, #16]   ← samples(普通 ldr)
119570:  ldr  x3, [x1, #48]   ← test_id_hash(普通 ldr)
119574:  ldar w1, [x2]        ← s2(acquire)
```

两个缺口:

- **W(写侧 begin)**:置奇是 relaxed store,与字段 store(seq 异址)之间无任何排序指令。aarch64 弱序模型允许异址 store 被其他观察者(兄弟 child 读者)以异于程序序的顺序观测;抽象机上 relaxed 原子不约束其它访问的移动。字段(含复位三 store 的 `test_id_hash = hash`)先于奇数标记可见时,读者 `s1==s2==旧偶` 双读一致判据完全失效——「新 hash + 旧 samples>0 + 旧 eff」混合态通过有效性检查,上一测试的 eff_ratio 污染本测试 median(即 #486 窗口)。现有注释「首次置奇用 relaxed 即可——字段 store 被提前到置奇之前时, 读者见到的仍是完整(新或旧)快照」论证错误:部分提前(hash 先行、samples/eff 未清)即得混合态。
- **R(读侧)**:s2 的 acquire 是单向屏障——只阻止**其后**的访问前移,不阻止**其前**的字段读下沉越过 s2 复读。字段读到 [s1,s2] 校验窗口之外的世代值时,判据形同虚设。内核 `read_seqcount_retry` 在复读前有 `smp_rmb()`,本实现缺等价物。

修复后不变式(三道排序合围):奇数可见 ⟹ 先于本代全部字段 store(新 fence);字段 store ⟹ 先于偶数发布(既有 release/stlr);读者字段读钉死在 [s1,s2] 窗口内(s1 acquire 钉上沿、新 fence 钉下沿)。由此:读者采纳 ⟹ 未观测到任何本代字段 store ⟹ 快照为完整上一稳定世代。

## Global Constraints

- 一个补丁一个单元:本计划产出一个 fix 提交(代码+行内注释)+ 一个 docs(plan) 提交(本文档),不捆绑其它改动。
- DCO:`git commit -s`,最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`,其后无内容;提交前 `git branch --show-current` 必须 = `fix/issue-486`。
- x86-64 非回归(rule 5):全部改动位于 `#if defined(__aarch64__) && defined(__linux__)` 门内,不触碰 `framework/test_data.h`(Main 布局/sizeof 不变),x86-64 编译产物逐字节不变(按检视声明)。
- 零新警告:`./scripts/build.sh` 干净。
- 已知口径诚实声明:字段仍是普通非原子访问(与 77a705a2 同 pragmatic 口径,aarch64 对齐 ≤8B 单指令访问无撕裂;C++ 抽象机的形式 data-race UB 保留为文档化边界),本修复只封排序,不改字段原子性。
- 本缺陷是潜伏重排窗口而非已观测失败(与 09c94fc6→77a705a2 同类):验证 = 反汇编前后对照(barrier 缺失→存在)+ 协议探针不变式 + 模型论证,不以「压力跑未复现撕裂」充当闭合证据。

---

### Task 1: 协议探针(先于修复构建,含缺陷机制确定性演示)

**Files:**
- Create: `/tmp/seqlock_fence_probe.c`(不进仓库,源码以本任务为准;与 77a705a2 的 /tmp/seqlock_probe 同惯例)

**Interfaces:**
- Consumes: 无(独立可编译,复刻 `framework/test_data.h` 的 `MemFeedback` 结构与 `sandstone_run.cpp` 的读写协议形态)
- Produces: 探针可执行文件 `/tmp/seqlock_fence_probe`;场景 S1–S5 的断言结果(供 Task 3 引用)

- [ ] **Step 1: 写探针源码**

```c
/* /tmp/seqlock_fence_probe.c — issue #486 MemFeedback 槽协议探针
 * 复刻 sandstone_run.cpp 的 seqlock 读写协议(当前 HEAD 形态 _cur /
 * 修复形态 _fixed 并列),fork + MAP_SHARED 环境下验证:
 *   S1 正常写读采纳(+EMA 跨写续接)   —— 协议不变式,修复前后都必须过
 *   S2 奇数槽丢弃                     —— 协议不变式,修复前后都必须过
 *   S3 被杀写者恢复(|1 不变式)       —— 协议不变式,修复前后都必须过
 *   S4 begin-重排机制演示              —— 缺陷机制确定性实证(威胁模型)
 *   S5 跨进程压力一致性               —— 回归护栏(撕裂为概率事件,0 违例
 *                                         是护栏不是闭合证据)
 * 编译: gcc -O2 -Wall -Wextra -o /tmp/seqlock_fence_probe /tmp/seqlock_fence_probe.c
 */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

struct FB {                      /* 与 framework/test_data.h MemFeedback 同构 */
    uint64_t test_id_hash;
    double   eff_ratio;
    uint32_t samples;
    uint32_t seq;
};

#define HASH_A 0xAAAA1111AAAA1111ull
#define HASH_B 0xBBBB2222BBBB2222ull

/* ---- 写者: 当前 HEAD 形态(77a705a2) ---- */
static void fb_write_cur(struct FB *fb, uint64_t hash, double eff)
{
    uint32_t seq0 = __atomic_load_n(&fb->seq, __ATOMIC_RELAXED);
    uint32_t s1 = seq0 | 1;
    __atomic_store_n(&fb->seq, s1, __ATOMIC_RELAXED);
    if (fb->test_id_hash != hash) {
        fb->samples = 0;
        fb->test_id_hash = hash;
        fb->eff_ratio = 0.0;
    }
    if (fb->samples == 0)
        fb->eff_ratio = eff;
    else
        fb->eff_ratio = 0.5 * fb->eff_ratio + 0.5 * eff;
    ++fb->samples;
    __atomic_store_n(&fb->seq, s1 + 1, __ATOMIC_RELEASE);
}

/* ---- 写者: 修复形态(置奇后全序 fence) ---- */
static void fb_write_fixed(struct FB *fb, uint64_t hash, double eff)
{
    uint32_t seq0 = __atomic_load_n(&fb->seq, __ATOMIC_RELAXED);
    uint32_t s1 = seq0 | 1;
    __atomic_store_n(&fb->seq, s1, __ATOMIC_RELAXED);
    __atomic_thread_fence(__ATOMIC_SEQ_CST);          /* ← 修复点 */
    if (fb->test_id_hash != hash) {
        fb->samples = 0;
        fb->test_id_hash = hash;
        fb->eff_ratio = 0.0;
    }
    if (fb->samples == 0)
        fb->eff_ratio = eff;
    else
        fb->eff_ratio = 0.5 * fb->eff_ratio + 0.5 * eff;
    ++fb->samples;
    __atomic_store_n(&fb->seq, s1 + 1, __ATOMIC_RELEASE);
}

/* ---- 读者: 当前 HEAD 形态 ---- */
static int fb_read_cur(const struct FB *fb, uint64_t hash, double *out)
{
    uint32_t s1 = __atomic_load_n(&fb->seq, __ATOMIC_ACQUIRE);
    if (s1 & 1)
        return 0;
    uint64_t h = fb->test_id_hash;
    uint32_t samples = fb->samples;
    double eff = fb->eff_ratio;
    uint32_t s2 = __atomic_load_n(&fb->seq, __ATOMIC_ACQUIRE);
    if (s1 != s2)
        return 0;
    if (h == hash && samples > 0 && eff > 0.0) {
        *out = eff;
        return 1;
    }
    return 0;
}

/* ---- 读者: 修复形态(s2 复读前全序 fence) ---- */
static int fb_read_fixed(const struct FB *fb, uint64_t hash, double *out)
{
    uint32_t s1 = __atomic_load_n(&fb->seq, __ATOMIC_ACQUIRE);
    if (s1 & 1)
        return 0;
    uint64_t h = fb->test_id_hash;
    uint32_t samples = fb->samples;
    double eff = fb->eff_ratio;
    __atomic_thread_fence(__ATOMIC_SEQ_CST);          /* ← 修复点 */
    uint32_t s2 = __atomic_load_n(&fb->seq, __ATOMIC_ACQUIRE);
    if (s1 != s2)
        return 0;
    if (h == hash && samples > 0 && eff > 0.0) {
        *out = eff;
        return 1;
    }
    return 0;
}

static struct FB *fb_new(void)
{
    struct FB *fb = mmap(NULL, sizeof *fb, PROT_READ | PROT_WRITE,
                         MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (fb == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
    memset(fb, 0, sizeof *fb);
    return fb;
}

static int fails;

static void check(const char *what, int cond)
{
    printf("  %-58s %s\n", what, cond ? "PASS" : "FAIL");
    if (!cond)
        fails++;
}

/* S1: 正常写读采纳 + EMA 续接 */
static void s1(void)
{
    printf("S1 正常写读采纳:\n");
    double v = -1;
    struct FB *fb = fb_new();
    fb_write_cur(fb, HASH_A, 4.0);
    check("cur   写后读采纳 eff=4.0", fb_read_cur(fb, HASH_A, &v) && v == 4.0);
    check("fixed 写后读采纳 eff=4.0", fb_read_fixed(fb, HASH_A, &v) && v == 4.0);
    fb_write_cur(fb, HASH_A, 6.0);
    check("cur   同 hash 二写 EMA=5.0 samples=2",
          fb_read_cur(fb, HASH_A, &v) && v == 5.0 && fb->samples == 2);
    *fb = (struct FB){0};
    fb_write_fixed(fb, HASH_A, 4.0);
    fb_write_fixed(fb, HASH_A, 6.0);
    check("fixed 同 hash 二写 EMA=5.0 samples=2",
          fb_read_fixed(fb, HASH_A, &v) && v == 5.0 && fb->samples == 2);
    munmap(fb, sizeof *fb);
}
```

- [ ] **Step 2: 补全 S2/S3/S4/S5(要点)**

  - **S2 奇数丢弃**:`__atomic_store_n(&fb->seq, 1u, __ATOMIC_RELAXED)` 后,`fb_read_cur/fb_read_fixed` 均须返回 0。
  - **S3 被杀写者恢复**:`fork()` 子进程执行「seq0 读 → 置奇 → 只写 test_id_hash → `_exit(0)`」(模拟超时/OOM 被杀于两次 seq store 之间),父 `waitpid` 后:恢复前读必须丢弃(奇);`fb_write_fixed(fb, HASH_A, 7.0)` 后读必须采纳 7.0 且 `__atomic_load_n(&fb->seq, __ATOMIC_RELAXED)` 为偶。对 `fb_write_cur` 重复同样断言(|1 逻辑两种形态共有)。
  - **S4 begin-重排机制演示(核心)**:`fb_write_cur(fb, HASH_A, 4.5)` 后,手工构造 relaxed 置奇在抽象机/弱序硬件上允许的可见序——`fb->test_id_hash = HASH_B;`(模拟被提前到置奇之前的复位 store,此刻 seq 仍为旧偶数、samples/eff 仍是测试 A 残留),随即 `fb_read_cur(fb, HASH_B, &v)`:**预期采纳且 v==4.5**——双读一致判据对该混合态完全失效,即 #486 污染窗口的确定性实证。同状态下 `fb_read_fixed` 同样采纳(读侧 fence 单独不设防,写侧 fence 才是必要一半——两处缺口各自独立)。此场景输出标注为「机制演示」不计入 PASS/FAIL。
  - **S5 跨进程压力一致性**:`fork()` 1 写者 + 2 读者;写者循环 `g=1..N` 交替 `HASH_A/HASH_B` 写 `eff=(double)g`(交替 hash 强制每次走复位路径,写后槽内恒 `{hash, eff=g, samples=1}`);读者循环对两 hash 读取,采纳值须满足奇偶自洽(采纳 `HASH_A` ⟹ `(int)eff` 为奇;`HASH_B` ⟹ 偶),违例即撕裂混采。运行 ~2s 后 `waitpid`,断言两读者违例数 == 0(护栏)。`_cur` 与 `_fixed` 写者各跑一轮。

- [ ] **Step 3: 编译并运行(修复前基线)**

Run: `gcc -O2 -Wall -Wextra -o /tmp/seqlock_fence_probe /tmp/seqlock_fence_probe.c && /tmp/seqlock_fence_probe`
Expected: S1/S2/S3 全 PASS(协议既有不变式);S4 打印「机制演示: cur 采纳 v=4.500(旧测试 eff)——判据失效实证」;S5 两轮 0 违例。`echo $?` = 0。

### Task 2: 修复(双侧 fence + 注释更正)

**Files:**
- Modify: `framework/sandstone_run.cpp`(仅 `#if defined(__aarch64__) && defined(__linux__)` 门内两函数)

**Interfaces:**
- Consumes: Task 1 探针基线结果
- Produces: fence 后的 `sdc_feedback_write`/`sdc_feedback_eff_median`;反汇编中两处 `dmb ish`(Task 3 取证)

- [ ] **Step 1: 读侧插入 fence 并更正注释**

`sdc_feedback_eff_median` 槽循环内,`double eff = fb->eff_ratio;` 与 `uint32_t s2 = __atomic_load_n(&fb->seq, __ATOMIC_ACQUIRE);` 之间插入:

```c
        __atomic_thread_fence(__ATOMIC_SEQ_CST);
```

注释更正(替换原「共享字段的重排混合态不再可能进入 median」的错误断言):字段读必须钉死在 [s1,s2] 窗口内——s2 的 acquire 只单向往后钉,不阻止其前字段读下沉越过复读(下沉即读到窗口外世代值,判据失效,#486 读侧缺口);fence 补上该方向。空槽 seq=0 恒偶的既有判据不变。

- [ ] **Step 2: 写侧插入 fence 并更正注释**

`sdc_feedback_write` 中 `__atomic_store_n(&fb->seq, s1, __ATOMIC_RELAXED);` 之后、`if (fb->test_id_hash != hash)` 之前插入:

```c
    __atomic_thread_fence(__ATOMIC_SEQ_CST);
```

注释块更正要点:
- 写序改为 `[seq→奇(relaxed)] → 全序 fence(dmb ish) → 复位+EMA 字段更新 → [s1+1→偶(release)]`。
- 删除/替换「首次置奇用 relaxed 即可……语义无损」的错误论证,改为:relaxed 置奇不约束其后字段 store 前移、亦不约束置奇自身后移(arm64 弱序,seq 与字段异址);字段先于奇数可见时读者 `s1==s2==旧偶` 判据失效,混合态(新 hash + 旧 samples/eff)通过有效性检查——即 #486 窗口。fence 保证「任一字段值可见 ⟹ 奇数已可见」,读者要么见奇数丢弃、要么 s2 复读 ≥ 奇数而丢弃(内核 `write_seqcount_begin` 的 `smp_wmb` 等价物)。
- 「T9 源序双保险」表述降级:正确性由 fence+release 承担,复位源序保留为历史口径(无害,不再必要)。
- 被杀写者恢复(|1 vs +1)段落原样保留——该逻辑不变。
- 读侧 fence 的对称性在写侧注释中交叉引用(或反之),说明三道排序缺一不可。

- [ ] **Step 3: 构建 + 探针复跑 + 反汇编取证**

Run: `./scripts/build.sh 2>&1 | tail -3`
Expected: 无新警告,构建成功。

Run: `/tmp/seqlock_fence_probe`
Expected: 与基线一致(S1/S2/S3 PASS、S4 机制演示、S5 两轮 0 违例)。

Run: `objdump -d builddir/sdcshield | grep -A12 '<sdc_feedback_write'`(定位 seqlock 段)
Expected: 置奇 `str` 与字段 `str` 之间出现 `dmb ish`;`sdc_feedback_eff_median` 中字段 `ldr` 与第二次 `ldar` 之间出现 `dmb ish`。引用真实反汇编输出。

### Task 3: 端到端 + 回归验证(CLAUDE.md 规则 2)

**Files:**
- Test: 真实二进制 `builddir/sdcshield`(无新文件)

**Interfaces:**
- Consumes: Task 2 的修复后二进制
- Produces: 验证证据(引用进提交信息与 issue 报告)

- [ ] **Step 1: 端到端反馈闭环(受影响行为)**

Run: `./builddir/sdcshield -e <联合求解 eigen 用例> -t 6000 -T <总预算>s`(<用例>选实际写 `[反馈]` 日志的 auto 尺寸用例,预算调到 ≥2 轮,先以短预算试探轮次结构)
Expected: 第 1 轮出现 `[反馈] <test>: VmHWM=... → eff_ratio=... (EMA α=0.5, 样本=1)`;第 2 轮同 test 出现 `样本=2`(EMA 跨轮续接)且出现 `[联合求解] ... 反馈生效` 类日志——证明 fence 未破坏写/读闭环。引用真实日志行。

- [ ] **Step 2: 标准回归**

Run: `./builddir/sdcshield -e zstd19 -t 2000 -n 1`
Expected: `exit: pass`。

- [ ] **Step 3: x86-64 非回归(检视声明)**

改动全部位于 `#if defined(__aarch64__) && defined(__linux__)` 门内;`framework/test_data.h` 零改动;x86-64 无该段编译产物,行为逐字节不变。如实声明(无 x86 机器可跑,按规则以门控检视为准)。

### Task 4: 提交与推送

- [ ] **Step 1: docs(plan) 提交**

```bash
git add docs/superpowers/plans/2026-11-04-issue486-memfeedback-seqlock-fences.md
git commit -s -m "docs(plan): issue #486 MemFeedback seqlock 双侧 fence 修复计划"
```

- [ ] **Step 2: fix 提交(单一单元)**

```bash
git branch --show-current   # 必须 fix/issue-486
git add framework/sandstone_run.cpp
git commit -s -m "fix(framework): MemFeedback seqlock 补双侧 fence——关闭置奇前移与字段读下沉缺口 (#486)

<根因/改动/验证证据摘要——引用反汇编前后对照、探针结果、端到端日志、
zstd19 回归、x86 检视声明>"
```

- [ ] **Step 3: 推送 + PR + issue 报告**

```bash
git push -u origin fix/issue-486
gh pr create --base main --head fix/issue-486 \
  --title "fix: MemFeedback seqlock 补双侧 fence,关闭反馈槽重排污染窗口 (#486)" \
  --body "Closes #486 + 实现/修复报告"
# 若因分支已有 open PR 失败: gh pr edit fix/issue-486 --title … --body …(必须含 Closes #486)
# 报告贴 issue #486;结束语: task-loop: done fix/issue-486
```

## Self-Review

- 覆盖:issue #486 的诉求(封编译器/架构重排的复位 store 污染窗口)→ Task 2 写侧 fence 直接回应;读侧缺口是同一窗口的读者半边(混合态进入 median 需两侧同时失守,两侧都封才闭合)→ Task 2 读侧 fence。✔
- 占位符:S1 代码块为可直接落盘的完整源码段;S2–S5 给出可执行要点、精确 API 调用与预期断言值,无 TBD。✔
- 一致性:探针函数名/断言与 Task 3 引用一致;fence 统一 `__atomic_thread_fence(__ATOMIC_SEQ_CST)`。✔

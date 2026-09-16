# memcpy_rewr 无 conf 时退化原版默认逻辑（自适应参数）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** `memcpy_rewr` 在策略配置文件缺席时不再 `EXIT_SKIP`，而是退化成原版独立工具（GlusterFS-IOT demo）的默认语义运行，且默认参数按 CPU 核数与可用内存自适应取最大压力；conf 存在时行为完全不变。

**Architecture:** 只改 `tests/cpu/memory/memcpy_rewr.cpp` 的 `memcpy_rewr_init` 配置解析段：用 `access(R_OK)` 探测有效 conf 路径（与 `strategy_config_load` 同样的 env 解析规则），**仅 ENOENT/ENOTDIR**（文件不存在）时合成一个内建 `Strategy`（`default_original`：`role_rule=first_n_producers`、`producer_count=max(1, threads/12)`、`block_size` 按 MemAvailable 自适应），走既有的 `first_n_producers` 代码路径；文件存在但解析失败/不可读仍按现状 loudly skip。结构体几何（`mem_info_s` 五字段布局、队列、同步原语）零改动。

**Tech Stack:** 纯 C++（测试文件内），`/proc/meminfo` 的 `MemAvailable:`（框架无现成读取器，grep 过 `framework/` 仅 gpu/windows 无关匹配，测试内自读）、`access(2)`、既有 `strategy_config.h`（`Strategy`/`StrategySet` 结构，不改其 API）。

**Spec:** 用户需求（2026-09-16 会话）+ 原版 demo 代码（`memcpy_rewr.c`，已在会话中全文分析）+ 参考参数表（p 0..3 / c 0..47 / g_size=g_size_t=65536，48 核机）。两个已确认的决策：**block_size 内存自适应上限 2 MiB**（"每线程实占内存更大些，但跟可用内存相关，避免过多也避免过小"）；**producer 数 N=核数/12**（48 核→4，与参考表 4:44 比例一致）。

## 参考参数表 → 本方案的映射（含一处修正）

| 原版参数 | 参考值 | 本方案默认模式取值 | 说明 |
|---|---|---|---|
| `p_start`/`p_end` | 0..3 | 前 N 个 CPU 为 producer，`N = max(1, thread_count()/12)` | 参考表 p=0..3 与 c=0..47 **完全重叠**，按原版"重叠区间 consumer 赢"规则实际产生 0 个 producer（测试空转）——已按用户确认修正为 first-N 解读；48 核时 N=4 恰好复现参考表 4:44 |
| `c_start`/`c_end` | 0..47 | 其余全部 CPU 为 consumer（`test_schedule_fullsystem` 旗标本就全核并行，不限制 CPU） | "尽可能多 CPU 核并行"由此保证 |
| `g_size` | 65536 | `block_size = clamp(MemAvailable×50% ÷ threads ÷ 6, 64 KiB, 2 MiB)` | 64 KiB 下限=参考表值；2 MiB 上限=`c_size`（结构字段宽，也是原版 `g_size > c_size` 溢出 bug 的安全边界）。除数 6 = 每线程实际触碰的 buffer 前沿数保守上界（consumer 触碰 `m_tester.b1..b5` 五份、producer 触碰 `m_tmp` 一份；producer/consumer 互不触碰对方 buffer，取 6 为安全上界） |
| `g_size_t` | 65536 | `= block_size`（与 conf 模式现状一致，单一尺寸键） | 原版两旋钮独立；本方案沿用 port 现有合并语义（conf 模式亦然），参考表两值本就相等 |

结构一致性承诺：`mem_info_s` 布局（五 × 2 MiB 字段 + tag 数组 + 128 字节对齐）、MPSC 四优先级链表队列、`__yawn/__wake/__yield`、b1..b5 五路拷贝+比对、ARM64 arch-timer——**全部零改动**；`block_size` 只决定每个字段前沿被触碰的长度（与 conf 模式的 `block_size` 同一语义，仅取值来源不同）。

## Global Constraints

- **conf 存在 → 行为零变化**：`SANDSTONE_STRATEGY_INDEX` 选策略、`SANDSTONE_STRATEGY_CONF` 覆盖路径、conf 值（block_size=65536 等）全部照旧；本计划所有验证以真实 conf 跑一遍作回归。
- **仅"文件不存在"触发默认模式**：`access()` 失败且 `errno ∈ {ENOENT, ENOTDIR}`。文件存在但解析失败 → 仍 `EXIT_SKIP`（TestResourceIssue，现状）；存在但不可读（EACCES 等）→ 走 `strategy_config_load` 失败 → skip（现状）。用户指错路径不静默吞错。
- **默认模式忽略 `SANDSTONE_STRATEGY_INDEX`**（无策略可索引），文档写明。
- **block_size 值域 [64 KiB, `c_size`]**：下限 64 KiB（参考表值，也是 MemAvailable 不可读时的兜底）；上限 `c_size`=2 MiB（字段宽）。既有 init 范围检查 `1..c_size` 不变且天然放行 2 MiB（`>` 而非 `>=`）。
- **producer_count 值域 `max(1, thread_count()/12)`**：整数除法；2 线程时 max(1,0)=1（1 producer + 1 consumer，最小可跑形态）；`thread_count() < 2` 的既有 CpuTopology skip 不变（先于 conf 逻辑）。
- **内存预算 = MemAvailable 的 50%**：跨全部 worker 线程、每线程 6 份 block_size 前沿的保守上界估算。本机（128 核 / 22559200 kB）验证：22559200×1024/2/128/6 ≈ 15 MiB → 封顶 2 MiB，全系统实触 ≈ 118×5×2MiB(consumer) + 10×2MiB(producer) + 共享 ≈ 1.3 GiB，远低于预算——"尽可能大"由上限保证，"避免过多"由 50% 预算在小内存机器上兜底。
- **每线程 10 MiB `m_tester` + 2 MiB `m_tmp` 的 malloc 本身不随 block_size 变化**（懒分配、只有触碰前沿进 RSS，既有 `ensure_thread_buffers` 机制不动）。
- **x86-64 非回归**：全部新代码位于 `memcpy_rewr.cpp` 的 `#ifdef __aarch64__` 区内，且 meson 仅在 aarch64 构建该文件（`tests/cpu/meson.build:105` guard）——按检查确认，无 x86 路径改动。
- **不动 `strategy_config.h`**（共享头文件，保持 API 稳定；env 路径解析规则在测试内镜像 2 行即可）。
- **工作区纪律**：当前分支 `feat/third-party-sdc-libs` 上有**他人/在途的未提交改动**（`tests/cpu/openblas_gemm/dgemm.cpp` 修改 + 未跟踪的 `docs/superpowers/plans/2026-09-16-gemm-mdim-knob.md`，属 gemm-mdim 工作）。开工前从 HEAD 切新分支 `feat/memcpy-rewr-default-mode`；**所有 `git add` 用显式路径**（只加本计划列出的文件），绝不 `git add -A`/`git add .`，绝不提交 dgemm.cpp 与 gemm 计划文件。
- **一补丁一单元**：Task 1（代码）一个 commit，Task 2（文档+计划归档）一个 commit，共 2 个；每个 commit 前完成其验证步骤并引用真实输出。
- **commit message 不得以 `Co-Authored-By: Claude` 结尾**（CLAUDE.md 规则）；每个 commit 后 `git push` 到远端同名分支。
- **验证 100% 真实**：每条验证命令必须实际运行并引用观察到的输出；预期值给出推导，观察值必须粘贴。

---

### Task 1: 默认模式回退 + 自适应参数（memcpy_rewr.cpp）

**Files:**
- Modify: `tests/cpu/memory/memcpy_rewr.cpp`（includes 区 39-51 行；新增两个 helper 于 `role_for_cpu` 之后 555 行与 "SDCShield test entry points" 注释之间；`memcpy_rewr_init` 的 conf 解析段 583-617 行重排；log_info 行 655-658 加 producer_count）

**Interfaces:**
- Consumes: 既有 `strategy_config_load`/`strategy_config_pick`（`strategy_config.h`，签名不变）、`Strategy{name, params}`（`params` 为 `std::map<std::string,std::string>`）、`thread_count()`（init 期有效，既有 561 行已在用）、`log_info`/`log_skip(TestResourceIssueSkipCategory, ...)`。
- Produces: `static long read_mem_available_kib(void)`（返回 KiB，失败 -1）、`static int default_block_size(int threads)`（返回字节，[64 KiB, c_size]）；内建策略名 `"default_original"`（出现在 `-vv` 日志）；日志行新增 `producer_count=%d` 字段（conf 模式同样输出——它是配置旋钮值，numa_split/die_even_odd 策略下无语义，值恒为默认 4 或 conf 键值）。

- [ ] **Step 1: 切工作分支**

```bash
cd /home/sdc/wangxu/sdcshield
git checkout -b feat/memcpy-rewr-default-mode
git status --short   # 确认 dgemm.cpp 修改与 gemm 计划仍在未跟踪/修改区，未被带过来
```

预期：新分支创建成功；`git status --short` 仍显示 ` M tests/cpu/openblas_gemm/dgemm.cpp` 与 `?? docs/superpowers/plans/2026-09-16-gemm-mdim-knob.md`（它们留在工作区但不归本分支管，后续不 add）。

- [ ] **Step 2: 加两个 include**

在 `#include <sys/types.h>`（约 48 行）之后追加：

```cpp
#include <unistd.h>   /* access() for conf-existence probe */
#include <cerrno>     /* ENOENT/ENOTDIR discrimination     */
```

- [ ] **Step 3: 加 MemAvailable 读取 helper**

插入位置：`role_for_cpu` 函数结束（约 555 行 `}`）之后、`/* ---- SDCShield test entry points ---- */` 注释之前：

```cpp
/* ---- default-mode parameters (no conf file: original-tool semantics) ----
 * Read MemAvailable from /proc/meminfo, in KiB; -1 if unreadable.
 * Each meminfo line is "Key:  value kB" — consume the whole line so the
 * trailing unit token cannot desynchronise the fscanf scan.
 */
static long read_mem_available_kib(void)
{
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f)
        return -1;
    long val = -1;
    char key[64];
    while (fscanf(f, "%63s %ld", key, &val) == 2) {
        if (strcmp(key, "MemAvailable:") == 0)
            break;
        val = -1;
        int c;
        while ((c = fgetc(f)) != EOF && c != '\n')
            ;
    }
    fclose(f);
    return val;
}

/*
 * Default-mode block_size: scale the per-transfer payload to the largest
 * value the machine safely affords. Budget = half of MemAvailable spread
 * over all worker threads, each touching six block_size fronts (consumer:
 * m_tester b1..b5; producer: m_tmp — 6 is a conservative upper bound since
 * a thread only ever touches its own role's buffers). Clamped to
 * [64 KiB (reference-table floor), c_size (struct field width — also the
 * original tool's g_size>c_size overflow boundary)]. Returns bytes.
 */
static int default_block_size(int threads)
{
    const long lo = 64L * 1024;
    const long hi = (long)c_size;          /* 2 MiB */
    long mem_kib = read_mem_available_kib();
    if (mem_kib <= 0 || threads < 1)
        return (int)lo;                    /* unreadable/minimal: floor */
    long bytes = mem_kib * 1024L;          /* available bytes   */
    bytes /= 2;                            /* budget: half      */
    bytes /= threads;                      /* per worker thread */
    bytes /= 6;                            /* touched fronts    */
    if (bytes < lo)
        return (int)lo;
    if (bytes > hi)
        return (int)hi;
    return (int)bytes;
}
```

- [ ] **Step 4: 重排 init 的 conf 解析段**

将现有 583-617 行的 conf 定位+加载+选择段（`/* Locate the default config... */` 注释到 `strategy_config_pick` 的 `if (!s) {...}` 结束）整体替换为：

```cpp
    /* Locate the default config beside this source file. The meson build
     * injects -DMEMCPY_REWR_SRC_DIR=<absolute source dir of the memory/>
     * subdir so the path is valid regardless of the caller's CWD (the
     * framework may run from a build dir or a different working directory).
     * Fall back to deriving from __FILE__ if the macro wasn't supplied. */
    std::string default_conf;
#ifdef MEMCPY_REWR_SRC_DIR
    default_conf = std::string(MEMCPY_REWR_SRC_DIR) + "/memcpy_rewr_strategies.conf";
#else
    {
        std::string src_path = __FILE__;
        std::string::size_type slash = src_path.find_last_of('/');
        std::string dir = (slash == std::string::npos) ? "." : src_path.substr(0, slash);
        default_conf = dir + "/memcpy_rewr_strategies.conf";
    }
#endif

    /* Resolve the effective conf path exactly as strategy_config_load
     * would (env override wins, even if empty), so the existence probe
     * below cannot disagree with the loader. */
    std::string conf_path = default_conf;
    if (const char *ov = std::getenv("SANDSTONE_STRATEGY_CONF"))
        conf_path = ov;

    const Strategy *s = nullptr;
    size_t idx = 0;
    const int probe_errno = (access(conf_path.c_str(), R_OK) != 0) ? errno : 0;
    const bool conf_absent = (probe_errno == ENOENT || probe_errno == ENOTDIR);
    if (!conf_absent) {
        /* A config is present (or present-but-unreadable): load it; open
         * and parse failures still skip loudly, exactly as before. */
        if (!strategy_config_load(st->strategy_set, conf_path)) {
            log_skip(TestResourceIssueSkipCategory,
                     "memcpy_rewr: strategy config error: %s",
                     st->strategy_set.error.c_str());
            delete st;
            return EXIT_SKIP;
        }
        s = strategy_config_pick(st->strategy_set, "SANDSTONE_STRATEGY_INDEX", &idx);
        if (!s) {
            log_skip(TestResourceIssueSkipCategory,
                     "memcpy_rewr: no strategies available");
            delete st;
            return EXIT_SKIP;
        }
    } else {
        /* No config file anywhere: degrade to the original standalone
         * tool's semantics instead of skipping. Roles split
         * first-N-producers (N = threads/12 — a 48-core box gets the
         * reference 4:44 split) via the existing role_rule code path;
         * block_size scales with MemAvailable (see default_block_size).
         * SANDSTONE_STRATEGY_INDEX has nothing to index here and is
         * ignored. */
        st->strategy_set.strategies.push_back(Strategy{});
        Strategy &d = st->strategy_set.strategies.back();
        d.name = "default_original";
        d.params["role_rule"] = "first_n_producers";
        d.params["producer_count"] =
            std::to_string(std::max(1, thread_count() / 12));
        d.params["block_size"] =
            std::to_string(default_block_size(thread_count()));
        s = &d;
        idx = 0;
        log_info("memcpy_rewr: no strategy config at %s (errno %d); "
                 "using original-tool default mode",
                 conf_path.c_str(), probe_errno);
    }
```

其后的既有代码（`st->strategy = s; st->strategy_index = idx;`、三个 `get_long` 参数提取、NUMA 集合构建、范围检查、`init_iot`、gl_tester 播种、`test->data = st;`）**原样不动**——默认模式的参数经 `get_long` 从合成 Strategy 读出，与 conf 模式同一条代码路径。注意：`st->strategy_set` 是值成员，合成策略压入后 `s` 指向其内部，测试生命周期内有效（既有所有权注释 600-602 行描述的同一机制）。

- [ ] **Step 5: 日志行加 producer_count**

将既有 log_info（约 655 行）：

```cpp
    log_info("memcpy_rewr: strategy[%zu]=%s block_size=%d threads=%d "
             "numa_nodes=%zu arch_timer_freq=%lu",
             idx, s->name.c_str(), st->g_size, thread_count(),
             st->numa_ids_sorted.size(), user_archtimer_get_cntfrq());
```

替换为：

```cpp
    log_info("memcpy_rewr: strategy[%zu]=%s block_size=%d threads=%d "
             "producer_count=%d numa_nodes=%zu arch_timer_freq=%lu",
             idx, s->name.c_str(), st->g_size, thread_count(),
             st->producer_count, st->numa_ids_sorted.size(),
             user_archtimer_get_cntfrq());
```

（`st->producer_count` 在此行之前已由 `get_long("producer_count", 4)` 赋值；conf 模式下输出的是配置旋钮值——numa_split/die_even_odd 策略不用它，打印 4 或 conf 键值，属诚实输出。）

- [ ] **Step 6: 构建干净**

```bash
ninja -C /home/sdc/wangxu/sdcshield/builddir
```

预期：`ninja: no work to do` 或正常重编链接成功，**零新增 error/warning**（预存在的良性警告可接受）。任何由本次改动引入的警告/错误 = 失败，修复后重跑。

- [ ] **Step 7: 默认模式全核验证（conf 缺席 → 自适应跑起来）**

```bash
SANDSTONE_STRATEGY_CONF=/nonexistent ./builddir/sdcshield -e memcpy_rewr -t 5000 -n 128 -vv 2>&1 \
  | grep -E "default mode|strategy\[0\]|result:|exit:"
```

预期（128 核 / MemAvailable≈22 GB 的本机）：
- 一条 `no strategy config at /nonexistent (errno 2); using original-tool default mode`
- `strategy[0]=default_original block_size=2097152 threads=128 producer_count=10 numa_nodes=1 arch_timer_freq=100000000`
- `result: pass`（该测试）与 `exit: pass`

推导核对（必须独立复算并粘贴）：`awk '/^MemAvailable:/ {b=$2*1024/2/128/6; if (b<65536) b=65536; if (b>2097152) b=2097152; printf "expect block_size=%d\n", b}' /proc/meminfo` → 本机 MemAvailable 22559200 kB → 22559200×1024/2/128/6 ≈ 15 MiB > 2 MiB → **expect block_size=2097152**（封顶）。producer：`echo $((128/12))` → 10。日志观察值必须与推导一致。

内存实触合理性（观察方式）：另开窗口 `grep VmRSS /proc/$(pgrep -f 'sdcshield -e memcpy_rewr')/status` 或事后 `/usr/bin/time -v` 观察峰值 RSS 量级 ≈ 1.3 GiB（118 consumer×10 MiB + 10 producer×2 MiB + 共享），确认无 OOM、无异常膨胀。

- [ ] **Step 8: 默认模式最小形态验证（max(1,·) 钳位）**

```bash
SANDSTONE_STRATEGY_CONF=/nonexistent ./builddir/sdcshield -e memcpy_rewr -t 2000 -n 2 -vv 2>&1 \
  | grep -E "strategy\[0\]|result:"
```

预期：`strategy[0]=default_original block_size=<按 2 线程重算：MemAvailable×1024/2/2/6 → 仍封顶 2097152> threads=2 producer_count=1 numa_nodes=1 ...`，`result: pass`。`producer_count=1` 证明 `max(1, 2/12)=1` 钳位生效（1 producer + 1 consumer 最小可跑形态）。

- [ ] **Step 9: conf 模式回归（存在即赢，值不变）**

```bash
SANDSTONE_STRATEGY_INDEX=1 ./builddir/sdcshield -e memcpy_rewr -t 3000 -n 8 -vv 2>&1 \
  | grep -E "strategy\[1\]|result:"
```

预期：`strategy[1]=same_die_l3_brawl block_size=65536 threads=8 producer_count=4 numa_nodes=1 ...`（block_size 来自 conf 而非自适应；producer_count=4 是 `get_long` 默认——该策略无此键），`result: pass`。再跑 `SANDSTONE_STRATEGY_INDEX=2`，预期 `strategy[2]=few_producer_many_consumer_storm ... producer_count=4`（该策略 conf 显式给 4）。

- [ ] **Step 10: 解析错误仍然 loudly skip**

```bash
printf 'role_rule = numa_split\n' > /tmp/bad_conf.conf
SANDSTONE_STRATEGY_CONF=/tmp/bad_conf.conf ./builddir/sdcshield -e memcpy_rewr -t 2000 -n 4 -vv 2>&1 \
  | grep -E "result:|skip-reason:"
```

预期：`result: skip`，`skip-reason: 'memcpy_rewr: strategy config error: line 1: parameter outside any [strategy: ...] block'`——文件存在但坏 → 不进默认模式、不静默吞错（现状保持）。

- [ ] **Step 11: 无关测试回归**

```bash
./builddir/sdcshield -e zstd19 -t 3000 -n 1 2>&1 | tail -1
```

预期：`exit: pass`，零 SIGSEGV。

- [ ] **Step 12: x86-64 非回归检查（按检查确认）**

`git diff feat/third-party-sdc-libs -- tests/` 审查：全部改动位于 `tests/cpu/memory/memcpy_rewr.cpp` 的 `#ifdef __aarch64__` 区内（整个文件体本就在 37 行的 guard 内），meson guard（`tests/cpu/meson.build:105` `host_machine.cpu_family() == 'aarch64'`）未动，x86 构建不编译此文件。结论写进 commit message。

- [ ] **Step 13: 提交并推送**

```bash
git add tests/cpu/memory/memcpy_rewr.cpp
git commit -m "tests/cpu/memory/memcpy_rewr: fall back to original-tool default mode when conf absent

Instead of EXIT_SKIP when no strategy config file exists (default path
missing or SANDSTONE_STRATEGY_CONF dangling - ENOENT/ENOTDIR only),
synthesize a built-in 'default_original' strategy and run the original
standalone tool's semantics: role_rule=first_n_producers with
producer_count = max(1, threads/12) (48 cores -> the reference 4:44
split), and a MemAvailable-adaptive block_size (half of available memory
spread over all workers at six touched buffer fronts each, clamped to
64 KiB..c_size). A conf that exists but fails to open/parse still skips
loudly, exactly as before. conf mode behavior is unchanged.

The init log line now also reports the resolved producer_count knob.

All changes are inside the aarch64 guard of an aarch64-only-built test
file; x86-64 paths untouched.

Verified on Kunpeng 920 (128 CPUs, MemAvailable 22559200 kB):
- conf absent: strategy[0]=default_original block_size=2097152
  threads=128 producer_count=10, result pass (-t 5000 -n 128)
- conf absent, 2 threads: producer_count=1 (max(1, 2/12) clamp), pass
- conf mode regression: SANDSTONE_STRATEGY_INDEX=1 -> same_die_l3_brawl
  block_size=65536 (conf values win), pass
- parse-error conf: still skips (parameter outside any [strategy] block)
- zstd19 -t 3000 -n 1: exit pass"
git push -u origin feat/memcpy-rewr-default-mode
```

（commit message 里的 "Verified" 段必须与 Step 7-11 实际粘贴的输出一致；若任何一步观察值与预期不符，先修复重验，不得带病提交。）

---

### Task 2: 文档同步 + 计划归档

**Files:**
- Modify: `tests/cpu/memory/memcpy_rewr_strategies.conf`（头部注释块，"role_rule values" 段之前追加默认模式说明）
- Modify: `tests/cpu/memory/memcpy_rewr.cpp`（文件头部注释 24-27 行 "Strategy selection" 段落后追加默认模式一句）
- Add: `docs/superpowers/plans/2026-09-16-memcpy-rewr-default-mode.md`（本计划文件，git add 归档）

**Interfaces:**
- Consumes: Task 1 的实际行为与其验证输出（文档断言必须与之一致）。
- Produces: 无代码接口；文档准确性。

- [ ] **Step 1: conf 头部注释追加默认模式说明**

在 `memcpy_rewr_strategies.conf` 的头部注释块中、`# role_rule values` 行之前插入：

```
# If this file (or the SANDSTONE_STRATEGY_CONF override target) does not
# exist, the test does NOT skip: it falls back to the original standalone
# tool's default mode - role_rule=first_n_producers with
# producer_count = max(1, threads/12), and a MemAvailable-adaptive
# block_size (half of available memory across all workers at six touched
# buffer fronts each, clamped to 64 KiB..2 MiB). SANDSTONE_STRATEGY_INDEX
# is ignored in default mode. A file that exists but fails to open or
# parse still skips loudly.
```

- [ ] **Step 2: 测试文件头注释追加一句**

在 `memcpy_rewr.cpp` 头部注释的 "Strategy selection is handled by the reusable strategy_config framework (SANDSTONE_STRATEGY_INDEX cycles strategies round-robin across repeated invocations; SANDSTONE_STRATEGY_CONF overrides the config path)." 段末（约 27 行 `*/` 前）追加：

```
 *  When no config file is present (default path missing or
 *  SANDSTONE_STRATEGY_CONF dangling), the test degrades to the original
 *  tool's default mode instead of skipping: first-N-producers roles
 *  (N = threads/12) and a MemAvailable-adaptive block_size clamped to
 *  [64 KiB, c_size] — see default_block_size() in the implementation.
```

- [ ] **Step 3: 文档断言核对（对照 Task 1 真实输出）**

逐条核对上述两处文档断言与 Task 1 Step 7/8/9/10 粘贴的观察输出一致：默认模式确实运行（非 skip）、block_size 自适应封顶 2097152、producer_count=10/1 钳位、解析错误仍 skip、conf 模式不变。任何不一致 → 修文档（或如果是代码错，回 Task 1 修复重验）。conf 文件与头注释不参与编译，无需重编；但跑一次 `ninja -C builddir` 确认无意外（预期 no work to do 或仅时间戳重编）。

README.md 不改：其测试总表（191 行）只列检测域与用例名，不描述 conf/默认行为，无准确性问题；CLAUDE.md 同样未涉及该测试的 conf 行为。本任务的两处头注释即该机制的权威文档。

- [ ] **Step 4: 提交（含计划归档）并推送**

```bash
git add tests/cpu/memory/memcpy_rewr_strategies.conf tests/cpu/memory/memcpy_rewr.cpp \
        docs/superpowers/plans/2026-09-16-memcpy-rewr-default-mode.md
git commit -m "docs: document memcpy_rewr original-tool default mode (conf header, test header, plan)

The strategy-config header and the test's file header now describe the
no-conf fallback: first-N-producers (threads/12) + MemAvailable-adaptive
block_size clamped to 64 KiB..2 MiB, ENOENT/ENOTDIR-only trigger,
parse errors still skip. Archived the implementation plan."
git push
```

（显式路径 add——工作区里在途的 dgemm.cpp 修改与 gemm 计划文件不得混入。）

---

## Self-Review 记录

- **需求覆盖**：① conf 未指定→原版默认逻辑不 skip（Task 1 Step 4 else 分支）② 按 CPU 核数配参（producer_count=threads/12；全核并行由既有 fullsystem 旗标保证，不限制 CPU）③ 按可用内存配参（block_size 自适应 50% 预算/6 前沿，clamp [64 KiB, 2 MiB]）④ 压力最大化（上限 2 MiB=字段宽极大值；50% 预算防小机 OOM，双向满足"避免过多/过小"）⑤ 结构与 demo 一致（Global Constraints 明示零结构改动）⑥ 参考表映射含重叠矛盾修正（用户已确认）。无缺口。
- **占位符扫描**：无 TBD/TODO/"适当处理"；所有代码步骤给出完整代码，所有验证步骤给出命令+预期值+推导。
- **类型一致性**：`read_mem_available_kib()→long`、`default_block_size(int)→int`、`Strategy::{name:string, params:map<string,string>}`、日志 `%d` 对 `int st->producer_count`、`%zu` 对 `size_t idx`——跨任务一致。
- **边界**：threads<2 既有 skip 先行；MemAvailable 不可读→64 KiB 兜底；EACCES→skip（非默认模式）；空 SANDSTONE_STRATEGY_CONF→路径空串→ENOENT→默认模式（与 loader 解析规则镜像，无特判）。

# issue #366: mem_disambig_alu_sve sizing 语义修正 + THP 首触风暴消除 — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 `tests/cpu/sve/mem/mem_disambig_alu_sve.cpp` 的两个同根缺陷——每线程全尺寸工作集分配（N 线程合计 = N × 预算）与整缓冲清零风暴（实测首个检查点 > 60 s / 120 s 零检查点）——使默认满跑回到亚秒级检查点、RSS 有界。

**Architecture:** 只动这一个测试文件（#365 并行卡负责 arm64_sdc_sve.cpp，绝不触碰）。**根因（实测修正，见下）**：框架的 malloc 覆盖层（`framework/sysdeps/linux/malloc.cpp:80-85,101-104`）对 `aligned_alloc` 全量 `memset` 清零——每线程 18.3 GiB 分配 = 18.3 GiB 顺序清零 + 全量 4 KiB 缺页（本机实测 AnonHugePages=0、majflt=0，issue 的"THP 大页风暴"归因不成立；THP 与否只改缺页粒度，不改清零字节数）。修法三件套：(1) sizing 按本 slice 线程数均摊预算（同目录 `mmu_split_tlb_sve.cpp:105-106` 口径）；(2) 分配改 `mmap(MAP_PRIVATE|MAP_ANONYMOUS)`——内核按需零填充，绕开覆盖层的急切清零，零内容确定性语义不降级（未触页读作 0）；(3) `madvise(MADV_NOHUGEPAGE)` + 预触 512 个访问窗——RSS 上界与宿主 THP 策略解耦（每线程 512 × 4 KiB ≈ 2 MiB）。TEST_LOOP N=8192 不动（issue 定性"非粒度问题"），修后实测检查点间隔定标。

**实测根因链（2026-10-31 本机，全部可复核）:**
- `-n 4` 默认自适应 → 单一 73.4 GiB 连续 VMA = 4 × 18.3 GiB/线程（`/proc/<ctrl>/maps`）；RSS 4 s 冲 32.6 GiB；
- knob 阶梯（4 线程）: 64 MiB → minflt 65,634 = 4×64MiB/4KiB 精确全缓冲; 256 MiB → 262,245; 1 GiB → 1,048,678；maxrss ≈ 全缓冲; stime 占 89%（内核缺页清零）；AnonHugePages=0、majflt=0（无 THP、无换页）；
- 测试 run 热循环反汇编（objdump）: 512 偏移 × 32 B store+load，无扫描；
- 独立程序裸调 libc `aligned_alloc(64,1GiB)` + 512 稀疏触碰 → minflt 591、RSS 3.4 MB（内核/glibc 无辜）；
- 二进制符号: `nm builddir/sdcshield | grep aligned_alloc` → 本地 T 符号；objdump 显示 `aligned_alloc → __libc_memalign → malloc_usable_size → memset(ptr,0,usable)`；
- 外推: 默认满跑 = 128 × 18.3 GiB = 2.34 TiB 清零 → RSS 必然超 61 GiB 物理 → reclaim/swap 崩溃（issue 实测孤儿 52 GiB + swap 抖动 14 分钟的机制）。

**Tech Stack:** C++20 / arm_sve.h / Linux madvise / meson+node / sdcshield knob 链（`sdc_knob_int_clamped` → `sdc_env_knob_int_clamped` → 自适应）。

**Spec:** GitHub issue #366（`gh issue view 366`）；上游审计 issue #360 项 B2；范本 PR #347/#359（TEST_LOOP 粒度）；`CLAUDE.md` 规则 2/3/5/6。

## Global Constraints

- 永不 push main；`git branch --show-current` 提交前确认。
- 不动 `tests/cpu/sve/mem/arm64_sdc_sve.cpp`（#365 地盘）；framework 层建议只进 issue comment，不进代码。
- 复现**不许把本机推进 swap**（61 GiB 物理 / ~36 GiB 可用 / 128 核）：满尺寸默认口径（128 × 18.3 GiB VA、最坏 128 GiB RSS）**不跑**，用小 `-n` 或 `-O buffer_bytes` 限流对照；issue #366 的满尺度实测（-n 8 零检查点 > 60 s、孤儿 52 GiB RSS）直接引用。
- 构建零新警告；回归 `-e zstd19 -t 2000 -n 1` → `exit: pass`；x86 目检非回归（本 TU 是 meson aarch64 守卫的 SVE 测试）。
- 本机 SVE-256（无 SVE2/SME 可见）；本修复无 VL 条件行为（sizing/madvise/预触均 VL 无关，`lanes` 参数化已被 256 档覆盖），本机原生验证即覆盖改动行为；gem5-fi 边界在 PR 正文如实声明。
- `git commit -s`，最后一行且仅此一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，禁 Co-Authored-By。
- 4 并发 agent 共机：`SDC_BUILD_JOBS=20`，跑测试用小 `-n`。

---

### Task 1: 修复前基线——安全尺度复现 THP 首触代价

**Files:**
- 无代码改动；产出实测数据（进 PR 正文）。

**Interfaces:**
- Consumes: 现有二进制（若 builddir 缺失先构建）。
- Produces: 修复前 `-n 4`（自适应 573 MiB/线程，首触最坏 4 × 512 × 2 MiB = 4 GiB RSS，有界安全）的 yaml `maxrss/majflt` 与超时行为基线。

- [ ] **Step 1: 构建**

```bash
cd <worktree>
PKG_CONFIG_PATH=./third-party/eigen5 meson setup builddir --buildtype=release
SDC_BUILD_JOBS=20 ./scripts/build.sh
```

Expected: 零新警告。

- [ ] **Step 2: 小 knob 冒烟（确认测试本身可跑）**

```bash
./builddir/sdcshield --quality=-1 -Dlogging_format=yaml -e mem_disambig_alu_sve \
  -O mem_disambig_alu_sve.buffer_bytes=67108864 -n 4 -t 5 2>&1 | tail -30
```

Expected: `exit: pass`；resource-usage 的 maxrss ≈ 每线程 ≤ 1 GiB（64 MiB 跨度 × 512 偏移 THP 最坏 512 × 2 MiB，受 64 MiB 跨度截断）。

- [ ] **Step 3: 自适应口径 + THP 首触基线（-n 4，有界）**

```bash
./builddir/sdcshield --quality=-1 -Dlogging_format=yaml -e mem_disambig_alu_sve -n 4 -t 10 2>&1 | tail -40
```

记录: maxrss、majflt、是否 Timed Out / 到期自行退出。自适应值推演：cur=4, total=128 → slices=32 → consumers=64 → 61 GiB × 0.6 / 64 ≈ 573 MiB/线程。THP 首触 ≈ 4 × 512 × 2 MiB = 4 GiB（有界，安全）。若首检查点明显拖长或 majflt 巨大 → 机制证实；若 buddy 健康、代价温和 → 如实记录量级，满尺度外推靠算术（128 × 512 × 2 MiB = 128 GiB > 61 GiB）+ issue 实测引用。

- [ ] **Step 4: THP 论证材料**

```bash
cat /sys/kernel/mm/transparent_hugepage/enabled   # 本机 [always]
cat /sys/kernel/mm/transparent_hugepage/defrag    # [madvise]
```

对照: 大页缺页 = order-9 分配（伴 reclaim/慢路径）vs 4 KiB 普通缺页；`ru_majflt`（yaml resource-usage）是直接证据通道。

### Task 2: 修复实现

**Files:**
- Modify: `tests/cpu/sve/mem/mem_disambig_alu_sve.cpp`

**Interfaces:**
- Consumes: `thread_count()`（sandstone.h，init 期 = 本 slice 线程数）、`sdc_target_workset_bytes(600,2,nullptr)`（框架已乘 slice 数）、`madvise`（`<sys/mman.h>`）、`log_warning`、`strerror`（`<cstring>` 已含）、`errno`（需 `<cerrno>`）。
- Produces: knob 链语义不变（`-O buffer_bytes` / `SDC_MEM_DISAMBIG_ALU_SVE_BUFFER_BYTES` 优先级与 clamp [4096, 1<<40] 原样）；自适应层变为 `budget / thread_count()`，下限 4096。

- [ ] **Step 1: sizing 均摊（`disambig_alu_sve_buffer_bytes`）**

```c
static size_t disambig_alu_sve_buffer_bytes(struct test *test)
{
    int64_t v = sdc_knob_int_clamped(test, "buffer_bytes", 0, 4096, 1ll << 40);
    if (v == 0)
        sdc_env_knob_int_clamped("mem_disambig_alu_sve", "buffer_bytes", &v, 4096, 1ll << 40);
    if (v > 0)
        return (size_t)v;
    /* 自适应: 预算(60%物理/2消费者, 框架已按并行 slice 数放大 consumers)
     * 再按本 slice 线程数均摊——run 里每线程各 mmap 一份 buffer, 不除
     * 则 N 线程合计 = N × 预算(issue #366)。同 mmu_split_tlb_sve 口径。
     * 跨度只服务 512 个稀疏偏移的地址随机性, RSS 由预触页数决定。 */
    int threads = thread_count();
    if (threads < 1)
        threads = 1;
    uint64_t bytes = sdc_target_workset_bytes(600, 2, nullptr) / (uint64_t)threads;
    if (bytes < 4096)
        bytes = 4096;
    return (size_t)bytes;
}
```

同步更新文件头 48-53 行注释（"占物理内存 60%, 2 个大内存消费者均摊" → 补"再按线程数均摊"）。

- [ ] **Step 2: 分配改 mmap + NOHUGEPAGE + 预触（run 内）**

```c
    /* 跨度分配走裸 mmap 而非 aligned_alloc_safe: 框架的 malloc 覆盖层
     * (sysdeps/linux/malloc.cpp)对每块分配全量 memset 清零, 18.3 GiB/线程
     * 的"稀疏跨度"会被急切清零成全缓冲常驻 + 全量缺页——issue #366 实测
     * 首个检查点 >60 s 零检查点、孤儿 RSS 52 GiB swap 抖动的真因(本机
     * AnonHugePages=0/majflt=0, 非 THP 大页风暴; THP 与否只改缺页粒度)。
     * MAP_ANONYMOUS 内核按需零填充, 未触页不占内存, 确定性语义不降级
     * (覆盖层的"测试拿到零化内存"承诺由内核保证)。 */
    uint8_t *buffer = static_cast<uint8_t *>(
        mmap(nullptr, data->buffer_bytes, PROT_READ | PROT_WRITE,
             MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (buffer == MAP_FAILED) {
        log_skip(OSResourceIssueSkipCategory,
                 "mem_disambig_alu_sve: disambig buffer mmap failed: %s",
                 strerror(errno));
        return EXIT_SKIP;
    }
    /* 稀疏探针不需要 2 MiB 大页: NOHUGEPAGE 把 512 个随机偏移的首触
     * 锁定在 4 KiB 粒度, 每线程 RSS 上界 = 512 窗 × 至多 2 页 × 4 KiB
     * ≤ 4 MiB(随机偏移下期望 ~2 MiB; 按 4 KiB 主机页计),
     * 与宿主 THP 策略解耦(THP=always 且分配成功的机器上, 不设则每偏移
     * 首触 2 MiB, 128 线程 × 512 × 2 MiB = 128 GiB 又回 OOM)。失败
     * (老内核/gem5 SE 未实现该 advice)仅记录并降级, 探针语义不变。 */
    if (madvise(buffer, data->buffer_bytes, MADV_NOHUGEPAGE) != 0)
        log_warning("mem_disambig_alu_sve: MADV_NOHUGEPAGE failed (%s), "
                    "first sweep keeps host-default page faults", strerror(errno));
    /* 预触 512 个访问窗([off, off+lanes*8) 首尾各一字节; 8 对齐 + <=128 B
     * 窗最多跨 2 页), 把残余 4 KiB 缺页移出计时区。写 0 不影响 body
     * 语义(store->load 回读比较, 缓冲旧内容无关)。 */
    for (int i = 0; i < DISAMBIG_ALU_SVE_PATTERNS; i++) {
        uint8_t *p = buffer + data->offsets[i];
        p[0] = 0;
        p[(size_t)data->lanes * 8 - 1] = 0;
    }
```

原 `aligned_alloc_safe(64, data->buffer_bytes)` 行删除；TEST_LOOP 结束后的 `free(buffer)` 改 `munmap(buffer, data->buffer_bytes)`。顶部 include 区加 `<sys/mman.h>` 与 `<cerrno>`（`<cstring>` 已有，strerror 在内）。同文件目录先例: `mmu_split_tlb_sve.cpp:247-258`（mmap + MAP_FAILED → log_skip(OSResourceIssueSkipCategory) + EXIT_SKIP）。

- [ ] **Step 3: 重新构建，零新警告**

```bash
SDC_BUILD_JOBS=20 ./scripts/build.sh
```

- [ ] **Step 4: 修后对照（与 Task 1 Step 3 同口径）**

```bash
./builddir/sdcshield --quality=-1 -Dlogging_format=yaml -e mem_disambig_alu_sve -n 4 -t 10 2>&1 | tail -40
```

Expected: 自适应跨度 ≈ 573 MiB/4 = 143 MiB；`exit: pass`（到期自行退出）；maxrss ≈ 每线程 2 MiB 量级 + 框架底噪；majflt 显著低于修前（无 order-9 风暴）。

- [ ] **Step 5: 检查点间隔定标（决定 N=8192 去留）**

```bash
./builddir/sdcshield --quality=-1 -Dlogging_format=yaml -e mem_disambig_alu_sve -n 4 -t 3 2>&1 | tail -20
```

-t 3 到期自行退出且无 Timed Out → 首检查点 ≪ 3 s，N=8192 保留（body µs 级 × 8192 ≈ 数十 ms 间隔，issue 定性成立）。把量级写进 PR 正文。

- [ ] **Step 6: 默认满跑（修复的最终验收——现在安全）**

```bash
./builddir/sdcshield --quality=-1 -Dlogging_format=yaml -e mem_disambig_alu_sve -t 30 2>&1 | tail -40
```

Expected: 128 线程全跑 `exit: pass`；每线程跨度 18.3 GiB/128 ≈ 146 MiB；合计预触 RSS ≈ 256 MiB + 底噪；无 swap 抖动（跑前跑后 `free -g` 对照）。

### Task 3: 回归 + x86 目检

**Files:** 无改动。

- [ ] **Step 1: 回归**

```bash
./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -5
```

Expected: `exit: pass`。

- [ ] **Step 2: x86 目检**

确认 diff 只在 `#if defined(__aarch64__)` 守卫的 TU 内（meson tests/cpu/sve 守卫，x86 不编）；`<sys/mman.h>/<cerrno>` 为 POSIX 通用头，非 x86 回归。VL 门控 `arm64_require_sve_vl(128,1024)` 未动；修复无 VL 条件行为，本机 SVE-256 原生验证覆盖。

### Task 4: 提交 + PR + 检视 + 合并

- [ ] **Step 1: 提交**

```bash
git branch --show-current   # 非 main
git add tests/cpu/sve/mem/mem_disambig_alu_sve.cpp docs/superpowers/plans/2026-10-31-issue366-mem-disambig-thp-sizing.md
git commit -s -m "fix(sve/mem): mem_disambig_alu_sve sizing 按线程均摊 + 跨度改 mmap 绕开清零风暴 (issue #366)

每线程全尺寸 sdc_target_workset_bytes(600,2) 使 N 线程合计 = N x 预算
(默认满跑 128 x 18.3 GiB); 叠加框架 malloc 覆盖层(sysdeps/linux/malloc.cpp)
对 aligned_alloc 的全量 memset, 每线程 18.3 GiB 跨度被急切清零成全缓冲
常驻——本机实测: 1 GiB x 4 线程 knob 跑 minflt=1,048,678=全缓冲页数、
stime 89%、AnonHugePages=0/majflt=0(非 issue 归因的 THP 大页风暴; THP
与否只改缺页粒度不改清零字节数); 默认口径 -n 4 外部 120 s 杀零检查点,
满跑外推 2.34 TiB 清零必然超物理(issue 实测孤儿 RSS 52 GiB + swap 抖动)。
修法: 预算再按本 slice 线程数均摊(同 mmu_split_tlb_sve 口径) + 跨度改
裸 mmap(内核按需零填充, 确定性语义不降级) + MADV_NOHUGEPAGE + 预触
512 个访问窗(每线程 RSS 上界 ≤ 4 MiB、期望 ~2 MiB, 与宿主 THP 策略解耦)。
TEST_LOOP N=8192 保留(body µs 级, 修后检查点间隔实测定标)。"
git push origin <branch>
```

- [ ] **Step 2: PR + 检视 + 合并**

```bash
gh pr create --base main --title "fix(sve/mem): mem_disambig_alu_sve sizing 按线程均摊 + THP 首触预热 (issue #366)" --body <正文含根因/选型/证据/Fixes #366/framework 建议(孤儿 control 回收)>
```

派 `feature-dev:code-reviewer` 检视处置 → `gh pr merge <PR> --merge` → issue 收尾（`gh issue comment` 留 framework 层建议）。

## Self-Review

- Spec 覆盖: issue #366 三条建议（sizing 均摊 ✓、NOHUGEPAGE+预触 ✓、孤儿回收→issue comment 不进码 ✓）；"不是 TEST_LOOP 粒度问题" → N 不动 + 实测定标 ✓；不动 arm64_sdc_sve.cpp ✓。
- 占位符: 无。
- 类型一致: `disambig_alu_sve_buffer_bytes` 返回 size_t 不变；`data->buffer_bytes/offsets/lanes` 均现有字段。

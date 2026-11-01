# SVE 全 VL 验证运动（188 用例 × 128/256/512）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 188 个 SVE1 用例在 VL=128/256/512 三档采集真实通过证据（564 格矩阵），发现即修。

**Architecture:** native 两腿（128=env 降 spec / 256=原生）在本机全量跑 `--test-list-file`；512 腿走 gem5-fi SE 真二进制，先补 `prctl(PR_SVE_GET_VL/SET_VL)` stub 打通框架 VL 探测（A′），时间盒失败则回退 FS（A）。修复走独立 commit 三档重验。

**Tech Stack:** sdcshield（meson/ninja, GCC, aarch64）、gem5 v25.1.0.1+CHAOS（scons, SE 模式）、bash 批处理。

**Spec:** `docs/superpowers/specs/2026-10-28-sve-all-vl-verification-campaign-design.md`（本计划从 spec 论证；执行者两份都读）

## Global Constraints

- 工作分支：`feat/sve-all-vl-verification-2026-10-28`（已存在，含 spec commit `3119ac87`，基于 main `12ccda54`）。绝不直接提交 main。
- gem5-fi 仓（`/home/sdc/wangxu/gem5-fi`，git 仓）改动走它自己的 feature 分支 + 它的纪律（one-patch-per-unit、自验后自动推送非 main 分支、commit 末尾无 Co-Authored-By）。
- 本机事实：cortex x3b（48-d02-00），SVE-256 原生、无 SVE2/SME；页大小 4096；128 核；gem5 源码树 `/home/sdc/wangxu/gem5-fi/CHAOS/gem5`（v25.1.0.1-6-g62c7bf28）；gem5.opt 现存于 `gem5-fi/build/ARM/`（Sep 19 构建）。
- 每 commit 前 100% 真实验证（CLAUDE.md rule 2），引用真实输出；失败就修复重验。
- `git commit -s`，最后一行 `Signed-off-by: wangxu <wangxumarshall@qq.com>`，无 Co-Authored-By。提交前 `git branch --show-current`。
- 构建入口统一 `./scripts/build.sh`（并行度 nproc/2）；**campaign 跑批与构建绝不并行**（gem5-fi 仓纪律，也适用本仓大跑批）。
- 时间盒纪律（spec §5b/§9）：gem5 stub 链累计 ~5-6 个仍不闭合 → 停止迭代，升级到用户决策切方案 A（FS）。

---

### Task 1: 用例枚举、tag 调和与清单工件

**Files:**
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt`
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/enumeration.md`（调和结论）

**Interfaces:**
- Produces: `sve-test-list.txt`（每行 `<test_name>`，无时长列 → 沿用全局 `-t`；格式由 `framework/sandstone_tests.cpp` `parse_test_list_line` 定义：`名称 [时长|default]`，`#` 注释）。Task 2/3/6 全部以它为行定义。

- [ ] **Step 1: 采集两路枚举**

```bash
cd /home/sdc/wangxu/sdcshield-main
./builddir/sdcshield --list-tests > /tmp/all-default.txt          # 479 行（本机默认集）
grep -i "sve" /tmp/all-default.txt > /tmp/sve-named.txt           # 预期 188 行
# tag 路：源码内 ARM64_REQUIRES_SVE 声明的 DECLARE_TEST 名
grep -rln "ARM64_REQUIRES_SVE\b" tests/cpu/ | sort > /tmp/sve-tagged-files.txt
for f in $(cat /tmp/sve-tagged-files.txt); do
  grep -ho "DECLARE_TEST(\s*[a-zA-Z0-9_]*" "$f" | sed 's/DECLARE_TEST(\s*//'
done | sort -u > /tmp/sve-tagged-tests.txt
```

- [ ] **Step 2: 调和两集合**

```bash
comm -23 /tmp/sve-tagged-tests.txt /tmp/sve-named.txt > /tmp/tagged-but-not-named.txt
comm -13 /tmp/sve-tagged-tests.txt /tmp/sve-named.txt > /tmp/named-but-not-tagged.txt
comm -12 /tmp/sve-tagged-tests.txt /tmp/sve-named.txt > /tmp/intersection.txt
cat /tmp/sve-tagged-tests.txt /tmp/sve-named.txt | sort -u > /tmp/sve-union.txt
```
逐类解释写入 `enumeration.md`：
- `tagged-but-not-named`：非 sve 命名但 SVE tag（预期至少 `gemm_double_max_arm`——Eigen 分发 TU）→ **并入清单**。
- `named-but-not-tagged`：sve 命名但无 tag（预期 5 个 `sve_rot_ldr_*` 走 `compiler_minimum_device` + init 门控；核实每个的文件确实有 HWCAP 探测）→ 并入清单并在 `enumeration.md` 记录门控方式；若发现真正裸奔（无探测无 tag）→ 这是 checker 违规，修法见 Step 3。
- 不在本机默认集的 SVE 测试（被 spec 过滤的 SVE2/SME 4 件）列出但**不入清单**（引用既有证据，spec §1）。

- [ ] **Step 3: checker 零违规门**

```bash
python3 scripts/check-arm64-requires.py
```
Expected: 无违规输出，exit 0。有违规 → 每个违规是独立修复（`--fix` 或手工），修复进 Task 7 循环，本 Task 不携带代码修复。

- [ ] **Step 4: 生成清单并验证可执行**

```bash
mkdir -p docs/superpowers/inventory/sve-all-vl-campaign
cp /tmp/sve-union.txt docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt
wc -l docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt   # 记入 enumeration.md（预期 ≈189：188 + gemm_double_max_arm）
# 可执行性抽查：清单跑 3 个代表测试
./builddir/sdcshield --test-list-file docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt -t 1000 -n 1 2>&1 | tail -3
```
Expected: 无 "Syntax error"/"Unknown test" 行，`exit: pass`（-t 1000 短时）。若抽查超过几分钟，用 `-e` 单测 3 个代表（一个 sve_rot_ldr、一个 fma_tail_sve、一个 ipsec hmac）替代。

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/
git commit -s -m "docs(campaign): SVE 全 VL 验证清单——tag∪name 枚举与调和结论"
```

---

### Task 2: native 256 腿全量跑（2 seeds）

**Files:**
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/native-256-<seed>.log`（原始输出归档）
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md`（分诊表）

**Interfaces:**
- Consumes: Task 1 的 `sve-test-list.txt`。
- Produces: 256 档 seed1/seed2 两个 `exit: pass`（或分诊后的边界格），triage 表驱动 Task 7。

- [ ] **Step 1: seed1 全量跑（过夜预算）**

```bash
cd /home/sdc/wangxu/sdcshield-main
LIST=docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt
LOG=docs/superpowers/inventory/sve-all-vl-campaign/native-256-LCG1001.log
./builddir/sdcshield --test-list-file $LIST -t 2000 -s LCG:1001 -o $LOG -Y > /tmp/native-256-LCG1001.stdout 2>&1
echo "exit=$?"   # 记录退出码（不经管道，$? 即 sdcshield 的）
tail -5 /tmp/native-256-LCG1001.stdout
```
Expected: `exit: pass`。注意：eigen_svd_cdouble_sve / eigen_svd_bidiag_sve 有全核 TIMEOUT 前科（node0 带宽，见 memory eigen-mdim-memory-measurement）；若这两格 TIMEOUT → 进 Step 3 分诊，不算本步失败。

- [ ] **Step 2: seed2 全量跑**：同 Step 1，`-s LCG:1002`，日志 `native-256-LCG1002.log`。

- [ ] **Step 3: 分诊**

从两个 log 提取非 pass 项：
```bash
grep -E "result: (fail|timeout)|exit: (fail|skip)" native-256-LCG*.log | sort | uniq -c
```
每个异常项写入 `triage-256.md`：测试名 / 现象（fail 消息、crash PC、timeout） / 初判（代码 bug / 环境 / 边界）。凡代码 bug → Task 7 修复循环。**crash 类必须先复现**（`-e <test> -t 2000 -n 1 -s <同状态>` retry 2 次确认确定性）再定性。

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/
git commit -s -m "docs(campaign): native 256 腿全量证据（2 seeds）+ 分诊表"
```

---

### Task 3: native 128 腿全量跑（2 seeds）

**Files:**
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/native-128-<seed>.log`
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/triage-128.md`

**Interfaces:**
- Consumes: Task 1 清单。
- Produces: 128 档证据（机制已实证：`SDCSHIELD_ARM64_SPEC_MAX_VL=128` → `ARM64 spec: SVE yes (max VL 128 bits)` + 框架 pin）。

- [ ] **Step 1: 前置确认降档生效**

```bash
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --dump-cpu-info 2>&1 | grep "ARM64 spec"
```
Expected: `ARM64 spec: SVE yes (max VL 128 bits) | SVE2 no | SME no | SME2 no`。

- [ ] **Step 2/3/4:** 与 Task 2 Step 1-3 相同，外加 env 前缀：
```bash
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --test-list-file $LIST -t 2000 -s LCG:1001 -o native-128-LCG1001.log -Y
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --test-list-file $LIST -t 2000 -s LCG:1002 -o native-128-LCG1002.log -Y
```
**特别关注**：128 档下 `eigen_svd_cdouble_sve` / `eigen_gemm_double_max_arm` 的 per-tier 分发（128 变体必须被选中而非 SKIP/FAIL）；`sleef_sve` 的 128..1024 门。SKIP 类异常全部记录理由并逐条判合法性（本机不应出现 HWCAP 类 SKIP）。

- [ ] **Step 5: Commit**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/
git commit -s -m "docs(campaign): native 128 腿全量证据（2 seeds，env 降 spec）+ 分诊表"
```

---

### Task 4: gem5-fi prctl stub（打通 VL 探测屏障）

**工作仓：`/home/sdc/wangxu/gem5-fi`（独立 git 仓，独立 feature 分支）。**
**Files（gem5-fi 仓内）:**
- Create: `docs/superpowers/plans/2026-10-28-sve-se-prctl-stub.md`（该仓纪律要求 plan 先行）
- Modify: `CHAOS/gem5/src/arch/arm/linux/se_workload.cc`（表项 `{base + 167, "prctl"}` 在第 ~679 行；handler 函数加在同文件表定义之前）

**Interfaces:**
- Produces: gem5.opt 支持 `prctl(PR_SVE_GET_VL=51)`（返回当前 VL 字节）与 `PR_SVE_SET_VL=50`（请求 ≥ 当前 VL → 返回当前 VL；请求 < 当前 VL → -EINVAL，单档限制如实警告）；其它 option 忽略返回 0。契约对齐 `framework/device/cpu/arm64/arm64_vl.h`：**返回 0 会被框架判为"执行层忽略"→ENOSYS→SKIP，所以成功路径必须返回正的 VL 字节数**。

- [ ] **Step 1: gem5-fi 仓建分支与 plan**

```bash
cd /home/sdc/wangxu/gem5-fi
git status --short | head        # 确认干净
git switch -c feat/sve-se-prctl-stub-2026-10-28
```
写入 `docs/superpowers/plans/2026-10-28-sve-se-prctl-stub.md`：目标（打通 SDCShield 真二进制 SE 运行的 prctl 屏障）、改动点（下述代码）、验证（本计划 Step 3-5）、交付（stub commit + 重建二进制）。commit 该 plan。

- [ ] **Step 2: 实现 prctlFunc 并挂表**

`se_workload.cc` syscall 表（64-bit 表，`{base + 167, "prctl"}` 处）改为 `{base + 167, "prctl", prctlFunc}`，并在表定义前加：

```cpp
/// prctl() for SE: SVE vector-length get/pin, mirroring the Linux uapi
/// semantics SDCShield's framework layer (arm64_vl.h) depends on — a
/// successful PR_SVE_SET_VL returns the granted VL IN BYTES (returning 0
/// would be read as "execution layer ignored" -> ENOSYS -> test SKIP).
/// gem5 SE has exactly one tier (ArmISA::sve_vl_se, in quadwords), so a
/// request at or above the configured VL is granted at the configured VL;
/// anything lower is refused with EINVAL (single-tier limitation, warned
/// once). Other prctl options are ignored, returning success.
static SyscallReturn
prctlFunc(SyscallDesc *desc, ThreadContext *tc, long option, long arg2)
{
    switch (option) {
      case 51 /* PR_SVE_GET_VL */:
        return SyscallReturn(
            (int64_t)tc->getIsaPtr()->getVectorLengthInBytes());
      case 50 /* PR_SVE_SET_VL */: {
        const int64_t vl = tc->getIsaPtr()->getVectorLengthInBytes();
        if (arg2 >= vl)
            return SyscallReturn(vl);
        warn_once("%s: PR_SVE_SET_VL %ld below the single configured SE "
                  "tier (%ld bytes) refused with EINVAL",
                  desc->name(), arg2, vl);
        return SyscallReturn(-EINVAL);
      }
      default:
        return SyscallReturn(0);
    }
}
```
（若带参 handler 签名编译不匹配，参照同文件 `timesFunc<ArmLinux64>` 与 `syscall_emul.hh` 的 `closeFunc(SyscallDesc*, ThreadContext*, int)` 模式修正参数形式；`getVectorLengthInBytes()` 为 `BaseISA` 公有虚方法，返回 `sveVL*16` 字节，已在 `src/arch/arm/isa.hh` 核实。`SyscallReturn(-EINVAL)` 的负值即 errno 语义，同仓既有 handler 惯例。）

- [ ] **Step 3: 重建 gem5.opt（campaign 与构建不并行）**

```bash
cd /home/sdc/wangxu/gem5-fi
scons -C CHAOS/gem5 build/ARM/gem5.opt -j16 2>&1 | tail -5
```
**已知陷阱（该仓 README.md:113-116 / findings.md:81）**：`scons -C` 产物落在**仓库根** `build/ARM/gem5.opt`，必须拷回 `CHAOS/gem5/build/ARM/`（否则 runner 用陈旧二进制）：
```bash
mkdir -p CHAOS/gem5/build/ARM
cp -f build/ARM/gem5.opt CHAOS/gem5/build/ARM/gem5.opt
tools/verify_gem5_injectors.sh CHAOS/gem5/build/ARM/gem5.opt   # 或按该脚本实际调用约定
ls -la build/ARM/gem5.opt CHAOS/gem5/build/ARM/gem5.opt        # mtime 一致
```
Expected: 构建零新 warning；injector 验证通过（19 CHAOS 类在位）。

- [ ] **Step 4: spike 复跑——prctl 屏障已过**

```bash
cd /home/sdc/wangxu/sdcshield-main/scripts/eigen-sve-double/gem5
G5=/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt \
  timeout 540 ./run_sve.sh /home/sdc/wangxu/sdcshield-main/builddir/sdcshield \
  -e sve_rot_ldr_at_top -t 500 -n 1 -O sve_rot_ldr_at_top.slots=1024 --vl 4 2>&1 | tail -20
```
Expected: **不再出现 `fatal: syscall prctl (#167) unimplemented`**。出现新的 fatal（下一个 syscall）→ 记录之（Task 5 处理），本 Task 仍算达成（prctl 屏障已过）。若仍死于 prctl → stub 有误，修复重验。
证据判读：输出走到更深处（如新的 warn/fatal、或测试输出/`exit:`）均算通过本步。跑完 `rm -rf m5out`（gem5 输出目录落 cwd，防入库）。

- [ ] **Step 5: 回归（gem5-fi 仓纪律：一个不受影响的既有 kernel 跑绿）**

```bash
cd /home/sdc/wangxu/gem5-fi
# 任选一个既有 dual kernel 按 workloads/sdcshield/Makefile 的既定方式跑一次
# （如 sve2_fmmla_dual 或 fpu_special_sve_dual），SDCSHIELD_FAILS=0 即回归通过
```

- [ ] **Step 6: Commit + push（gem5-fi 仓）**

```bash
git add CHAOS/gem5/src/arch/arm/linux/se_workload.cc docs/superpowers/plans/2026-10-28-sve-se-prctl-stub.md
git commit -s -m "feat(se): prctl SVE VL get/pin stub——打通 SDCShield 真二进制 SE 运行"
git push -u origin feat/sve-se-prctl-stub-2026-10-28
```
（plan doc 若已单独 commit 则此处只提交代码；一 patch 一 unit。）

---

### Task 5: gem5 stub 链迭代（时间盒 5-6 个 stub）

**工作仓：gem5-fi。Files 按 spike 揭示的 syscall 逐个定，已知候选：`sched_setaffinity`/`getaffinity`、`signalfd`/`pidfd`（forkfd 后端）、`futex`。**

**Interfaces:**
- Consumes: Task 4 的 stub 二进制与 spike 命令。
- Produces: `run_sve.sh ./builddir/sdcshield -e sve_rot_ldr_at_top ... --vl 4` 输出 `exit: pass`（真二进制在 SE 下完整跑通一个测试）——或时间盒耗尽 → 升级决策。

- [ ] **Step 1: 迭代循环（每个 fatal 一轮）**

每轮：
1. 跑 Task 4 Step 4 的 spike 命令，读末尾 fatal：`fatal: syscall <name> (#N) unimplemented`。
2. 在 `se_workload.cc` 表中给 `<name>` 挂 handler。语义选择：
   - 只影响调度/亲和性（`sched_setaffinity` 等）→ `ignoreFunc` 或返回成功的 stub（返回 0 = 成功）。
   - 语义必须真实（如 `gettid` 已有；futex 若 fatal → 优先用 gem5 既有的 futexEmulation 路径而非手写——先 grep `src/sim/syscall_emul.cc` 与 `src/arch/arm/linux/` 有无现成 futex/robust-list 支持）。
   - forkfd 后端 syscall（`signalfd`/`pidfd_open`/`eventfd`）→ 先查 gem5 是否已实现（`eventfd` 常有）；未实现的做最小 stub（返回 fd=0 的成功或 -ENOSYS 让 forkfd 走 pipe 回退路径——**读 forkfd 源码判定哪条回退路径会走通**，仓库内 vendored forkfd 或 glibc 内嵌，`grep -rn "forkfd" framework/`）。
3. 重建（Task 4 Step 3 命令）+ 复跑 spike。
4. 每个 stub = gem5-fi 仓独立 commit（一 patch一 unit，验证 = spike 输出推进到更深屏障）。

- [ ] **Step 2: 终止条件判定**

- **成功**：spike 输出出现 `exit: pass`（含 `result: pass`）。→ 进入 Task 6。
- **时间盒**：累计新增 stub ≥6 个仍未跑通 → 停止。如实记录链条（每个 fatal 的 syscall 名与已做 stub），**升级用户决策**：切方案 A（FS：需先评估 FS 镜像资产建设——本机 `gem5-fs/` 为空，参考 `configs/se/arm_chaos_fs.py` 与 eigen FS 先例），或降级接受边界格。不许静默放弃。

- [ ] **Step 3: 全量回归（gem5-fi 仓）**：Task 4 Step 5 的 kernel 复跑 + `tools/verify_gem5_injectors.sh`。

---

### Task 6: 仿真速标定 + 512 批量执行（188 用例）

**Files:**
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-batch.sh`（批处理脚本）
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-overrides.txt`（重负载用例的 -O 降容表，标定产物）
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-512/<test>.log` × 188（原始输出）

**Interfaces:**
- Consumes: Task 5 的可跑通 SE 二进制路径；Task 1 清单。
- Produces: 512 档 188 格证据。

- [ ] **Step 1: 标定仿真速（5 个代表用例）**

```bash
cd /home/sdc/wangxu/sdcshield-main/scripts/eigen-sve-double/gem5
G5=/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt
for t in sleef_sve fma_tail_sve mesh_upi_sve_sym kreg2_sve; do
  /usr/bin/time -f "$t %e s" timeout 1800 env G5=$G5 ./run_sve.sh \
    /home/sdc/wangxu/sdcshield-main/builddir/sdcshield \
    -e $t -t 500 -n 1 --vl 4 2>&1 | tail -2
done
# slots 类重负载用例单独标定（-O 只挂它自己的 knob）：
/usr/bin/time -f "sve_rot_ldr_at_top %e s" timeout 1800 env G5=$G5 ./run_sve.sh \
  /home/sdc/wangxu/sdcshield-main/builddir/sdcshield \
  -e sve_rot_ldr_at_top -t 500 -n 1 -O sve_rot_ldr_at_top.slots=1024 --vl 4 2>&1 | tail -2
rm -rf m5out   # spike 标定会在 cwd 产生 gem5 输出目录，清掉防入库
```
记录每用例墙钟。**预算规则**：>30 min 的用例 → 降 `-t`（500→200→100）或加 `-O` 降容（slots/mdim），仍超 → 边界格候选（理由必须机制性）。标定结论写入 `gem5-overrides.txt`（格式：`<test>|<额外参数>`，如 `eigen_svd_cdouble_sve|-O eigen_svd_cdouble_sve.mdim=64`）。

- [ ] **Step 2: 批处理脚本**

`gem5-512-batch.sh`（并行度 64——本机 128 核，gem5 单线程；**与任何构建错峰**）：

```bash
#!/bin/bash
# SVE 全 VL campaign — gem5 SE 512-bit leg. Usage: ./gem5-512-batch.sh
# 每 gem5 实例独占工作目录（gem5 把 m5out 写进 cwd，64 路并发共用 cwd 会互相覆写）。
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
LIST="$HERE/sve-test-list.txt"
OVR="$HERE/gem5-overrides.txt"
OUT="$HERE/gem5-512"; mkdir -p "$OUT"
G5=/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt
RUN=/home/sdc/wangxu/sdcshield-main/scripts/eigen-sve-double/gem5/run_sve.sh
BIN=/home/sdc/wangxu/sdcshield-main/builddir/sdcshield
run_one() {
    local t="$1" extra=""
    extra=$(grep -m1 "^$t|" "$OVR" 2>/dev/null | cut -d'|' -f2-)
    mkdir -p "$OUT/$t.d"
    ( cd "$OUT/$t.d" && timeout 3600 env G5="$G5" "$RUN" "$BIN" -e "$t" -t 500 -n 1 $extra --vl 4 ) \
        > "$OUT/$t.log" 2>&1
    echo "$t exit=$?"
}
export -f run_one; export LIST OVR OUT G5 RUN BIN
xargs -a "$LIST" -P 64 -I{} bash -c 'run_one "$@"' _ {}
echo "green cells: $(grep -l 'exit: pass' "$OUT"/*.log | wc -l) / $(wc -l < "$LIST")"
```

- [ ] **Step 3: 执行与分诊**

```bash
chmod +x docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-batch.sh
docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-batch.sh
grep -L "exit: pass" docs/superpowers/inventory/sve-all-vl-campaign/gem5-512/*.log   # 非绿格清单
```
非绿格逐个分诊（crash → 复现定性；timeout → 降容重跑；仿真器屏障 → 记录 syscall 链证据）。代码 bug → Task 7。

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/
git commit -s -m "docs(campaign): gem5 512 腿批量证据（188 格）+ 标定与降容表"
```

---

### Task 7: 发现-修复循环（runbook，per-finding 触发）

**本 Task 是协议不是单次任务：Task 2/3/6 每产出一个"代码 bug"类分诊项即触发一轮。**

**Files:** 按发现的测试源文件（`tests/cpu/sve/**` 等）。

**Interfaces:**
- Consumes: triage 表（Task 2/3/6）。
- Produces: 修复 commit 序列；矩阵对应格转绿。

- [ ] **Step 1: 复现与定位**——`-e <test> -t 2000 -n 1 -s <同状态>` retry 2 次（确定性 = 代码 bug 标志；偶发 = 需 soak 定性）。用 `-vv` 取 crash 上下文（PC/backtrace）。
- [ ] **Step 2: 修复**——最小改动；同构族 grep 排查传播（PR #294 教训：kreg 四兄弟/mask slack 都是漏网）：
```bash
grep -rln "<同构模式>" tests/cpu/sve/   # 族内其它文件逐个核对
```
- [ ] **Step 3: 验证（三档）**——修复后：
```bash
./builddir/sdcshield -e <test> -t 2000 -n 1                          # 256 原生
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield -e <test> -t 2000 -n 1   # 128
# 512: Task 6 的 run_one 单测命令复跑（gem5）
```
外加 zstd19 回归（`-e zstd19 -t 2000 -n 1` → `exit: pass`）与本族抽查。gem5 不可达时如实声明边界（CLAUDE.md rule 2）。
- [ ] **Step 4: Commit**——一 finding 一 commit，message 含三档重验输出摘录 + 同构族排查结论。
- [ ] **Step 5: 矩阵回写**——对应格改绿并引用本 commit hash。

---

### Task 8: 证据矩阵汇总与验收

**Files:**
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/matrix.md`（564 格总表）

**Interfaces:**
- Consumes: Task 2/3/6 全部日志 + Task 7 修复记录。

- [ ] **Step 1: 汇总矩阵**——`matrix.md` 表头 `| test | 128 | 256 | 512 | 证据 |`；每格 pass/边界（理由）/FAIL（不允许残留）。生成方式：
```bash
# 以 grep 从归档日志机械生成，不许手填
for f in docs/superpowers/inventory/sve-all-vl-campaign/native-128-LCG1001.log; do ...; done
```
（执行者写 10 行以内的 awk/grep 管道从日志提取，脚本一并入库。）
- [ ] **Step 2: 验收自查**——564 格全绿或有显式机制性理由；抽查 10 格回放原始日志确认引用真实。
- [ ] **Step 3: Commit**——`docs(campaign): 564 格证据矩阵汇总（128/256/512）`。

---

### Task 9: 文档同步与收尾

**Files:**
- Modify: `README.md`（验证矩阵结论 + 证据索引一节）
- Modify: `CLAUDE.md`（两处勘误：gem5-fi 本机路径 `~/wangxu/gem5-fi`（非 operator/）；删除/更正 "`tests_sve` compiles at fixed 128-bit; `sleef_sve` stays there by design" 过时声明——sleef_sve 现在tests_sve_vla 真 VLA）
- Modify: `docs/research/progress-log.md`（运动全程入档：矩阵结论、发现-修复序列、gem5 stub 链交付）

- [ ] **Step 1: 三处文档按事实更新**（引用 matrix.md 与 commit 序列）。
- [ ] **Step 2: 验证文档准确性**——每个声明可指回证据（格/commit/日志路径）。
- [ ] **Step 3: Commit + push**——`git commit -s -m "docs: SVE 全 VL 验证运动结论同步（README/CLAUDE.md 勘误/progress-log）"`，push campaign 分支，提示用户 merge。

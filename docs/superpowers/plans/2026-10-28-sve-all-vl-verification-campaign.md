# SVE 全 VL 验证运动（190 用例 × 128/256/512）实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 为 190 个 SVE1 用例在 VL=128/256/512 三档采集真实通过证据（570 格矩阵），发现即修。（计数口径：计划撰写时点预期 188 用例/564 格，清单定稿 190——`wc -l sve-test-list.txt` = 190、matrix.md 实收 190×3=570 格；正文 Task 6/8 与 Goal 按定稿值，Task 1 命令块内「预期 188/≈189」为历史时点预期原文。）

**范围追记（过程叙事，非目标定义）:** 计数对齐与 Task 6 骨架执行语义对齐系 PR #418 检视处置轮次追加、超出 issue #403「.log 入库主张消歧」原始边界——因与本同步产出的工件协议注记交织于同一 hunk 邻域不再拆分 PR，处置按轮次独立 commit（git log 逐轮可追溯）+ PR 描述逐轮记录。

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
- **证据工件协议（控制器裁定 E；本计划工件清单 2026-11-01 同步，issue #403）**：原始
  运行日志**一律不入库**——campaign 目录 `.gitignore` 屏蔽 `*.log` 与 `/gem5-512/*`
  （仅豁免 `batch.tsv`/`identity.txt` 两个小体积溯源记录——**identity.txt 历史例外**：
  write_identity 机制 2026-10-30 后才入库，历史批（2026-10-29 legacy）跑时无此机制，
  identity.txt 从未产出入库（`git ls-files` 下 gem5-512/ 仅 batch.tsv）；已入库
  verdicts.txt 的身份头为 extract 时重算，其 :5 头注自述「RECOMPUTED AT EXTRACT
  TIME … no batch-time identity record exists」；照本计划含 write_identity 的脚本
  重跑将产出它）。入库证据：native 腿 =
  `extract-summary.sh` 机械提取的 `<leg>-LCG<seed>.summary.txt`（每用例一行 + 命令行/
  exit 头，行数=清单校验）+ 腿 stdout 跑完即回拷的 `.stdout.txt`（含 `exit: pass` 行；
  历史例外：256 腿 seed1 的 stdout 在该回拷步骤写入计划前已随重启丢失，未入库，其
  `exit: pass` 证据由 summary 头 exit 行承担——triage-256.md §7，入库集以
  `git ls-files` 为准）；
  512 腿 = `gem5-512-verdicts.txt`（逐测判定）+ `gem5-512/batch.tsv`。各 Task 的
  `git add docs/superpowers/inventory/sve-all-vl-campaign/` 对被忽略 `.log` **零警告
  静默跳过**——不入库是预期行为，勿以 `git add -f` 强塞百 MB 级原始 yaml；裁定 E
  原文见 triage-256.md §1。

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
- Create（留工作区，**不入库**——`.gitignore` `*.log`，裁定 E）: `docs/superpowers/inventory/sve-all-vl-campaign/native-256-<seed>.log`（原始 yaml 输出，Step 3 分诊源）
- Create（入库证据）: `docs/superpowers/inventory/sve-all-vl-campaign/native-256-<seed>.summary.txt`（`extract-summary.sh` 机械提取，行数=清单校验）
- Create（入库证据）: `docs/superpowers/inventory/sve-all-vl-campaign/native-256-<seed>.stdout.txt`（腿 stdout 跑完即回拷，含 `exit: pass` 行。seed1 历史例外：其 stdout 在 Step 1 回拷行写入计划之前已随重启丢失、未入库，`exit: pass` 由 summary 头 exit 行承担——triage-256.md §7；本计划同步时（issue #403）入库的 native stdout 实为 3 份：256-seed2/128×2，`git ls-files` 可复核）
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/triage-256.md`（分诊表）

**Interfaces:**
- Consumes: Task 1 的 `sve-test-list.txt`。
- Consumes: 仓库既有 `extract-summary.sh`（已在库，`git ls-files` 可复核；Step 1 的机械提取工具——issue #403 同步补行，此前 Step 调用而清单无行，属清单-步骤失配镜像）。
- Produces: 256 档 seed1/seed2 两个 `exit: pass`（或分诊后的边界格），triage 表驱动 Task 7。

- [ ] **Step 1: seed1 全量跑（过夜预算）**

```bash
cd /home/sdc/wangxu/sdcshield-main
LIST=docs/superpowers/inventory/sve-all-vl-campaign/sve-test-list.txt
LOG=docs/superpowers/inventory/sve-all-vl-campaign/native-256-LCG1001.log
./builddir/sdcshield --test-list-file $LIST -t 2000 -s LCG:1001 -o $LOG -Y > /tmp/native-256-LCG1001.stdout 2>&1
echo "exit=$?"   # 记录退出码（不经管道，$? 即 sdcshield 的）
tail -5 /tmp/native-256-LCG1001.stdout
cp /tmp/native-256-LCG1001.stdout docs/superpowers/inventory/sve-all-vl-campaign/native-256-LCG1001.stdout.txt   # 裁定 E：stdout 跑完即回拷入库（防 /tmp 随重启丢失）
docs/superpowers/inventory/sve-all-vl-campaign/extract-summary.sh $LOG $LIST docs/superpowers/inventory/sve-all-vl-campaign/native-256-LCG1001.summary.txt   # 裁定 E：机械提取入库 summary（每用例一行 + 命令行/exit 头，行数=清单校验）
```
Expected: `exit: pass`。注意：eigen_svd_cdouble_sve / eigen_svd_bidiag_sve 有全核 TIMEOUT 前科（node0 带宽，见 memory eigen-mdim-memory-measurement）；若这两格 TIMEOUT → 进 Step 3 分诊，不算本步失败。

- [ ] **Step 2: seed2 全量跑**：同 Step 1，`-s LCG:1002`，日志 `native-256-LCG1002.log`。**替换范围**：Step 1 命令块中所有 `LCG1001` 字样——共 5 行（`LOG=` 行的日志路径、stdout 捕获重定向、`tail -5` 目检、`cp` 回拷、`extract-summary.sh` 目标的 /tmp 源与目标文件名）——一律相应改为 `LCG1002`，非仅 `-s` 与 `.log` 两处。否则照字面增量执行会把 seed2 stdout 覆写进 `native-256-LCG1001.stdout.txt`（漏改 `LOG=` 行则 seed2 的 `-o` 覆写 seed1 的 .log、LCG1002 summary 提取自 seed1 输出——证据污染）。

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
- Create（留工作区，**不入库**——`.gitignore` `*.log`，裁定 E）: `docs/superpowers/inventory/sve-all-vl-campaign/native-128-<seed>.log`（原始 yaml 输出，分诊源）
- Create（入库证据）: `docs/superpowers/inventory/sve-all-vl-campaign/native-128-<seed>.summary.txt`（`extract-summary.sh` 机械提取，行数=清单校验）
- Create（入库证据）: `docs/superpowers/inventory/sve-all-vl-campaign/native-128-<seed>.stdout.txt`（腿 stdout 跑完即回拷，含 `exit: pass` 行）
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/triage-128.md`（分诊表）

**Interfaces:**
- Consumes: Task 1 清单。
- Produces: 128 档证据（机制已实证：`SDCSHIELD_ARM64_SPEC_MAX_VL=128` → `ARM64 spec: SVE yes (max VL 128 bits)` + 框架 pin）。

- [ ] **Step 1: 前置确认降档生效**

```bash
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --dump-cpu-info 2>&1 | grep "ARM64 spec"
```
Expected: `ARM64 spec: SVE yes (max VL 128 bits) | SVE2 no | SME no | SME2 no`。

- [ ] **Step 2/3/4:** 与 Task 2 Step 1-3 相同（**完整继承**其命令块全部行——cd/`LIST` 等与 Task 2 逐字相同的原样行原封继承，stdout 捕获、`echo` 退出码、`tail -5` 目检、`cp` 回拷、`extract-summary.sh` 提取各行均在下方命令块内按继承形态示出；下方命令块按被继承的完整形态改写，非仅主命令示意），外加 env 前缀；全部 `native-256` 文件名前缀相应改为 `native-128`（替换范围同 Task 2 Step 2 注记）：
```bash
# seed1（seed2 同理：LCG1001→LCG1002 文件名全量替换，替换范围见 Task 2 Step 2 注记）
LOG=docs/superpowers/inventory/sve-all-vl-campaign/native-128-LCG1001.log
SDCSHIELD_ARM64_SPEC_MAX_VL=128 ./builddir/sdcshield --test-list-file $LIST -t 2000 -s LCG:1001 -o $LOG -Y > /tmp/native-128-LCG1001.stdout 2>&1
echo "exit=$?"   # 记录退出码（不经管道，$? 即 sdcshield 的）
tail -5 /tmp/native-128-LCG1001.stdout
cp /tmp/native-128-LCG1001.stdout docs/superpowers/inventory/sve-all-vl-campaign/native-128-LCG1001.stdout.txt   # 裁定 E：stdout 跑完即回拷入库（防 /tmp 随重启丢失）
docs/superpowers/inventory/sve-all-vl-campaign/extract-summary.sh $LOG $LIST docs/superpowers/inventory/sve-all-vl-campaign/native-128-LCG1001.summary.txt   # 裁定 E：机械提取入库 summary
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

### Task 6: 仿真速标定 + 512 批量执行（190 用例）

**Files:**
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-batch.sh`（批处理脚本）
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-overrides.txt`（重负载用例的 -O 降容表，标定产物）
- Create（留工作区，**不入库**——`.gitignore` `/gem5-512/*`，裁定 E）: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-512/<test>.log` × 190（原始输出，连同 `.d`/`.done` 一并被忽略）
- Create（入库证据）: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-512/batch.tsv`（batch 脚本逐测追加的溯源记录；姊妹件 `gem5-512/identity.txt` 记批时二进制身份——**历史例外**：identity.txt 从未产出入库，历史批早于 write_identity 机制（见 Global Constraints 注记）；照本 Task 脚本重跑将产出它）
- Create（入库证据）: `docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-verdicts.txt`（`gem5-512-extract.sh` 从 batch.tsv 机械提取的逐测判定）

**Interfaces:**
- Consumes: Task 5 的可跑通 SE 二进制路径；Task 1 清单。
- Consumes: 仓库既有 `gem5-512-extract.sh`（已在库，`git ls-files` 可复核；Step 3 的机械提取工具——issue #403 同步补行，此前 Step 调用而清单无行，属清单-步骤失配镜像）。
- Produces: 512 档 190 格证据。

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

`gem5-512-batch.sh`（并行度 20——控制器修正案 2：标定实测单实例 RSS ≤ ~1.2 GB，20 路典型 ~24 GB，远低于 57 GB 可用；本机 128 核，gem5 单线程；**与任何构建错峰**）。骨架如下——只承载**结构语义**（批时身份记录、断点续跑、`batch.tsv` 逐格追加、执行形状 `-f no --ignore-timeout`、绿格统计口径）。骨架与入库版是双份同源逻辑、仅以注释文字维系同步（无机械校验，纪律已失效过一次——round-3 只修骨架统计行、round-4 检视才补修入库脚本）；**分歧一律以本 Task Create 的 `gem5-512-batch.sh` 为准，骨架不可照抄执行**。已知分叉（勿以骨架结果对照入库证据）：

- **执行输入缺陷**：骨架 `xargs` 直喂原始 `$LIST`——清单含注释行时会作 bogus `-e` 参数被真实执行、`|time` 后缀名格的 batch.tsv 行全部落空；入库版先过滤 `list.effective`（PR #307 #21）。
- **结果级分叉**：骨架主命令缺 26 项 baked `-O` 降容数组（native-256 腿裁定 D 同集）——无降容则 `eigen_svd_bidiag_sve`/`eigen_svd_cdouble_sve` 等全核 TIMEOUT 前科用例收不了口，照骨架跑出的 pass/timeout 格分布与入库证据分叉。
- **完整版另有的特性**（骨架均缺）：`--one` 单测模式、重复名守卫（同名并发 `--one` 实例互覆 `.d`/`.log`，开跑前即失败，PR #307 #19）、重复键守卫（gem5-overrides.txt 单键单行契约，双键使矩阵标注未用过的 knob，PR #307 #32）、批间二进制变更告警（full batch 对照 identity 记录、变更即大声警告）、逐行 stdout 回显、PR #307 检视加固注释。

（骨架的执行语义重写系 PR #418 round-1/round-2 检视处置产物，超出本计划原始目标（工件清单同步），为消除「入库证据声明与同页内嵌脚本矛盾」而做，处置记录见 commit d7ca9374/9fa0787b。）

```bash
#!/bin/bash
# SVE 全 VL campaign — gem5 SE 512-bit leg. Usage: ./gem5-512-batch.sh
# 每 gem5 实例独占工作目录（gem5 把 m5out 写进 cwd，多路并发共用 cwd 会互相覆写）。
# 执行形状（Task 5 裁定）：-f no（nofork——gem5 SE 无异步信号投递/跨 fork mmap 共享，
# fork 模式结构性不可行）+ --ignore-timeout。
# 裁定 E：每格一行 O_APPEND 追加 gem5-512/batch.tsv（<test>\trc=<n>\twall=<n>s\t
# <exit-line>\t<sim-exit-line>\tsrc=batch——入库溯源记录，gem5-512-extract.sh 的
# 机械提取与 provenance audit 以它为唯一输入）；批时二进制身份 write_identity()
# 写 gem5-512/identity.txt（extract 以批时记录盖戳 verdicts 头，防 rebuild 后
# 张冠李戴——历史例外：2026-10-29 legacy 批早于 write_identity 机制，identity.txt
# 从未产出，已入库 verdicts 身份头为 extract 时重算，见其头注 RECOMPUTED AT
# EXTRACT TIME；照本骨架重跑将产出 identity.txt）；断点续跑：rc=0 已收口的格 touch .done 跳过，超时/崩溃格不标记、
# 幂等重跑重新执行（batch.tsv 追加写、提取取末行，新行覆盖旧行）。
# 绿格统计 = 原始清单内、末行为 exit: pass 的唯一测试数（与入库版同款清单约束 +
# 幂等口径：batch.tsv 为追加写，重跑已绿格会追加新 pass 行——行计数双计，取清单内
# 每测试末行即幂等且分子≤分母由构造保证）；分母与执行输入分叉——骨架 xargs 直喂
# 原始 $LIST（清单含注释行时会作 bogus -e 参数被真实执行、`|time` 后缀名格
# batch.tsv 行全部落空），入库版分子/分母均取先过滤的 list.effective（PR #307
# #21，骨架免列）——速览近似而非判定：rc=0
# 不蕴含 pass，exit: pass 行亦不区分 pass 与诚实 init-skip（overlay 抑制 per-test
# yaml，无日志行可判——tick 地板是唯一机械句柄）；权威判定 = gem5-512-extract.sh
# （rc=0 && exit: pass && tick≥地板），本 echo 仅收尾速览（口径详见其头注释）。
set -u
HERE="$(cd "$(dirname "$0")" && pwd)"
LIST="$HERE/sve-test-list.txt"
OVR="$HERE/gem5-overrides.txt"
OUT="$HERE/gem5-512"; mkdir -p "$OUT"
G5=/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt
RUN=/home/sdc/wangxu/sdcshield-main/scripts/eigen-sve-double/gem5/run_sve.sh
BIN=/home/sdc/wangxu/sdcshield-main/builddir/sdcshield
write_identity() {
    printf 'gem5_md5\t%s\nsdcshield_version\t%s\nrecorded_at\t%s\n' \
        "$(md5sum "$G5" | cut -d' ' -f1)" \
        "$("$BIN" --version 2>/dev/null || echo unknown)" \
        "$(date '+%F %T')" > "$OUT/identity.txt"
}
run_one() {
    local t="$1" extra="" rc start end exitline simexit; local -a extra_args=()
    if [ -f "$OUT/$t.done" ]; then   # 断点续跑：末行 rc=0 已收口（或无矛盾行）即跳过
        lastrc=$(awk -F'\t' -v t="$t" '$1==t{last=$2} END{print last}' \
            "$OUT/batch.tsv" 2>/dev/null || true)
        if [ "$lastrc" = "rc=0" ] || [ -z "$lastrc" ]; then return 0; fi
        echo "NOTE: $t carries .done but its last batch.tsv row is $lastrc" >&2
        echo "      (stale marker, pre-2026-10-30 attempted-semantics) — re-running" >&2
        rm -f "$OUT/$t.done"         # 陈旧标记（末行为非 rc=0 收口的矛盾行）→ 告警后重跑（入库版 --one 分支同款审计告警——陈旧 .done 被强制重跑须可观测）
    fi
    [ -f "$OUT/identity.txt" ] || write_identity
    extra=$(awk -v t="$t" 'index($0, t "|")==1 {print substr($0, length(t)+2); exit}' \
        "$OVR" 2>/dev/null || true)
    read -r -a extra_args <<< "${extra:-}"   # 词分割不 glob（PR #307 #13 语义）：裸 $extra 的 * 会展开打到 $OUT/$t.d
    mkdir -p "$OUT/$t.d"; start=$(date +%s)
    # 完整版主命令此处另注入 26 项 baked -O 降容数组（结果级分叉，见 Step 2 引言）：
    # 骨架缺之则 eigen_svd_bidiag/cdouble_sve 等全核 TIMEOUT 前科用例收不了口。
    ( cd "$OUT/$t.d" && timeout 3600 env G5="$G5" "$RUN" "$BIN" \
        -e "$t" -t 500 -n 1 -f no --ignore-timeout \
        ${extra_args[@]+"${extra_args[@]}"} --vl 4 ) \
        > "$OUT/$t.log" 2>&1
    rc=$?; end=$(date +%s)
    exitline=$(grep -a -m1 "^exit: " "$OUT/$t.log" || true)
    simexit=$(grep -a "Simulated exit code" "$OUT/$t.log" | tail -1)
    printf '%s\trc=%s\twall=%ss\t%s\t%s\tsrc=batch\n' "$t" "$rc" "$((end-start))" \
        "${exitline:-<no-exit-line>}" "${simexit:-<no-sim-exit>}" >> "$OUT/batch.tsv"
    [ "$rc" -eq 0 ] && touch "$OUT/$t.done"
}
export -f run_one write_identity   # write_identity 亦须导出：run_one 内的调用在子 bash 执行，
                                    # 未导出则 command not found（现结构被顶层预写掩护不触发，防御性导出）
export LIST OVR OUT G5 RUN BIN
[ -f "$OUT/identity.txt" ] || write_identity
xargs -a "$LIST" -P 20 -I{} bash -c 'run_one "$@"' _ {}
n=$(awk -F'\t' 'FILENAME == ARGV[1] { list[$1] = 1; next } $1 in list { last[$1] = $4 } END { c = 0; for (t in last) if (last[t] == "exit: pass") c++; print c }' \
    "$LIST" "$OUT/batch.tsv" 2>/dev/null)   # 清单内、末行=exit: pass 的唯一测试数（幂等 + 分子≤分母构造保证）；零行单 0；文件缺失捕获空
echo "cells with 'exit: pass': ${n:-0} / $(wc -l < "$LIST")"   # 分母 = 原始清单（xargs 同喂它，执行输入同款免列——见 Step 2 引言）；入库版分子/分母均取 list.effective（PR #307 #21）
```

- [ ] **Step 3: 执行、机械提取与分诊**

```bash
chmod +x docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-batch.sh
docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-batch.sh
docs/superpowers/inventory/sve-all-vl-campaign/gem5-512-extract.sh docs/superpowers/inventory/sve-all-vl-campaign   # 裁定 E：从 batch.tsv 机械提取 gem5-512-verdicts.txt（逐测判定 + 行数=清单校验，Step 4 入库）
grep -L "exit: pass" docs/superpowers/inventory/sve-all-vl-campaign/gem5-512/*.log   # 非绿格清单
```
非绿格逐个分诊（crash → 复现定性；timeout → 降容重跑；仿真器屏障 → 记录 syscall 链证据）。代码 bug → Task 7。

- [ ] **Step 4: Commit**

```bash
git add docs/superpowers/inventory/sve-all-vl-campaign/
git commit -s -m "docs(campaign): gem5 512 腿批量证据（190 格）+ 标定与降容表"
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
- Create: `docs/superpowers/inventory/sve-all-vl-campaign/matrix.md`（570 格总表，190×3——matrix.md 已实收入库，标题自述 190×3）

**Interfaces:**
- Consumes: Task 2/3/6 的**入库**证据（`<leg>-LCG<seed>.summary.txt`、`gem5-512-verdicts.txt`、`gem5-overrides.txt`；裁定 E——原始 `.log` 不入库，不得作为矩阵输入）+ Task 7 修复记录。

- [ ] **Step 1: 汇总矩阵**——`matrix.md` 表头 `| test | 128 | 256 | 512 | 证据 |`；每格 pass/边界（理由）/FAIL（不允许残留）。生成方式：
```bash
# 以 grep/awk 从入库 summary/verdicts 工件机械生成，不许手填
for f in docs/superpowers/inventory/sve-all-vl-campaign/native-128-LCG1001.summary.txt; do ...; done
```
（执行者写 10 行以内的 awk/grep 管道从入库工件提取，脚本一并入库。）
- [ ] **Step 2: 验收自查**——570 格全绿或有显式机制性理由（190×3，定稿口径见 Goal 注记）；抽查 10 格回放**入库工件**（`.summary.txt` / `gem5-512-verdicts.txt`）确认引用真实（裁定 E：原始 `.log` 留工作区不入库——Step 1 刚声明其不得作为矩阵输入，新 clone 的验收者拿不到，同样不得作为验收输入）。
- [ ] **Step 3: Commit**——`docs(campaign): 570 格证据矩阵汇总（190×3：128/256/512）`（实际执行 commit e4720831 同款）。

---

### Task 9: 文档同步与收尾

**Files:**
- Modify: `README.md`（验证矩阵结论 + 证据索引一节）
- Modify: `CLAUDE.md`（两处勘误：gem5-fi 本机路径 `~/wangxu/gem5-fi`（非 operator/）；删除/更正 "`tests_sve` compiles at fixed 128-bit; `sleef_sve` stays there by design" 过时声明——sleef_sve 现在tests_sve_vla 真 VLA）
- Modify: `docs/research/progress-log.md`（运动全程入档：矩阵结论、发现-修复序列、gem5 stub 链交付）

- [ ] **Step 1: 三处文档按事实更新**（引用 matrix.md 与 commit 序列）。
- [ ] **Step 2: 验证文档准确性**——每个声明可指回证据（格/commit/日志路径）。
- [ ] **Step 3: Commit + push**——`git commit -s -m "docs: SVE 全 VL 验证运动结论同步（README/CLAUDE.md 勘误/progress-log）"`，push campaign 分支，提示用户 merge。

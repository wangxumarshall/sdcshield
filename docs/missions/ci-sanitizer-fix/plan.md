# ci-sanitizer-fix: 修复 Sanitizer 夜巡构建红

## Intent

Sanitizer 夜巡（`ubuntu-24.04-arm`，GCC 13.3.0，`-fsanitize=address,undefined`）在
2026-11-20 00:55 UTC 的 run 44649642083 起构建红：PR #814（5cca3484）合并后首次夜巡，
`tests/cpu/misc/adaptive_mem_sdc.cpp` 里 `xfmov_rot`（L342）与 `xfmov_rot3`（L391）的
`return vr;` 返回了一个 `register uint64x2_t vr __asm__("v18")` 显式寄存器变量，GCC 13.3.0
报 `error: address of explicit register variable 'vr' requested`。本 mission 修掉它，且**不改变
FMOV 压力内核的寄存器钉扎与指令序列**（改动只影响返回值取出方式）。

## Done when

- `adaptive_mem_sdc.cpp` 在 GCC 13.3.0 + `-fsanitize=address,undefined -fno-sanitize=alignment`
  下编译通过、零新告警（在 `ubuntu:24.04` arm64 容器内实证复现/验证，非推断）。
- v16/v17/v18/v19/v20 寄存器钉扎与 fmov/add/eor/sub 指令链逐条不变——只换返回值取出方式。
- 本机 `./scripts/build.sh` 干净、`adaptive_mem_sdc` 实跑 `exit: pass`、回归 `-e zstd19 -t 2000 -n 1` `exit: pass`。
- x86-64 非回归：改动整体落在 `#if defined(__aarch64__)` 内（检视确认）。

## Out of scope

- Multi-OS Verify 的 20.03 `HWCAP2_SVE2` 编译错与 22.03/24.03 `crash=2` 已在 f0375ab6、63f1b3c2
  修好，本 mission 不重改；那只确认边界。
- 不重写 FMOV 压力内核本身、不改 `XFMOV_ROT`、不改 golden/标量参考值计算。

## Decisions for you

1. 是否在本 mission 内顺手补一次 fresh Multi-OS Verify 确认（#814 全文件在多 OS 下绿）？
   **Recommended: 否**，因为最近一次绿的 Multi-OS（11-18 schedule）早于 #814（11-19 合并），
   而当前 in-progress run 44456953718 是积压的旧 commit（3633cd85）——它报的 20.03 `HWCAP2_SVE2`
   错误在主线上已修。要确认需手动 `workflow_dispatch` 一次并等 1-2h（self-hosted runner 积压）；
   这属于验证而非新代码，建议放到合并后另行触发，不阻塞本 mission。

## How the work is split

单文件、单一故障点，一个 builder 一个 reviewer。没有共享文件，无 `depends_on`。

```yaml
troupe: 1
goal: "修掉 adaptive_mem_sdc.cpp 里显式寄存器变量返回值导致的 Sanitizer 构建红，且不改变压力内核语义"
why: "PR #814 引入的 adaptive_mem_sdc.cpp 让 Sanitizer 夜巡 2026-11-20 首次转红"
workflow: troupe-mission
base: mission/ci-sanitizer-fix
profiles:
  claude-build:
    agent: claude
    model: opus
    effort: high
    flags: [--permission-mode, acceptEdits]
    text: workflow:builder
roles:
  fix:
    use: claude-build
    goal: "改 xfmov_rot/xfmov_rot3 的返回值取出方式，使 GCC 13.3.0 + ASan/UBSan 编译通过且寄存器钉扎不变"
    touches: [tests/cpu/misc/adaptive_mem_sdc.cpp]
  review:
    agent: claude
    model: sonnet
    effort: high
    flags: [--permission-mode, acceptEdits]
    goal: "Review 分支，确认寄存器钉扎与指令链逐条不变"
    touches: []
    reviews: [fix]
    text: workflow:reviewer
```

## Role: fix

修 `tests/cpu/misc/adaptive_mem_sdc.cpp` 的 `xfmov_rot`（约 L300-343）与 `xfmov_rot3`（约 L345-392），
根因是 `register uint64x2_t vr __asm__("v18")` 被 `return vr;` 按值返回，GCC 13.3.0 + sanitizer 拒绝
（本机 GCC 10.3.1 不报错，故此错只在 CI 的 GCC 13.3.0 上出现）。

**先复现**：在 `ubuntu:24.04` arm64 容器（本机是 aarch64，镜像会自动解析成 arm64，GCC 13.3.0 与 CI
runner 同款）里对单 TU 用 `-std=gnu++23 -O2 -fsanitize=address,undefined -fno-sanitize=alignment` 加
本仓库的 `-DSANDSTONE -DMEMCPY_REWR_SRC_DIR=… -isystem third-party/eigen5` 等编译旗标，确认 L342/L391
的两处报错，再动手。参考：`sudo docker run --rm -v "$PWD":/src ubuntu:24.04 bash -c "…"`。

**修法约束（必须全部满足）**：
- 只改「返回值怎么从 v18 取出来」，**不得改动** asm 指令序列、`XFMOV_ROT`、寄存器绑定
  v16/v17/v18/v19/v20、以及 golden/标量参考值的计算（`op(slot, b, a + XFMOV_ROT)` 那段）。
- 推荐做法：保留 `register uint64x2_t vr __asm__("v18")` 作为 asm 输出（钉扎是 load-bearing），
  随后用一条 asm `mov %0.16b, v18.16b`（或等价）拷到普通 `uint64x2_t result` 变量再 `return result;`。
  这条 `mov` 正是 `return vr` 在 ABI 层本来就会生成的 `mov v0, v18`，故压力链逐条不变、输出值逐位不变。

**验证（100% 真实，不得“should work”）**：
1. 容器 GCC 13.3.0 单 TU 编译：修前复现两处报错、修后零错误零告警。
2. 本机 `./scripts/build.sh` 干净；`./builddir/sdcshield -e adaptive_mem_sdc -t 2000 -n 1`（或按 README 的
   单测/VERIFY 口径）`exit: pass`；回归 `./builddir/sdcshield -e zstd19 -t 2000 -n 1` `exit: pass`。
3. 用 objdump 对比修前/修后（本机 GCC 10 修前也能编，作对照）被内联进 nc_30~nc_35 combo 的
   fmov/add/eor/sub 链，证明除返回值取出外逐条不变。
4. x86-64 检视：改动整体在 `#if defined(__aarch64__)` 内。

**不得改动**：`framework/`、其它测试文件、`XFMOV_ROT`、combo 表、底盘宏拼接。若发现还需碰别的文件
才能过，停下回报（见 `.troupe/result.md`），不要擅自扩大 `touches`。

## Role: review

分支落地后逐条核对（超出 reviewer 规则之外还要看）：
- 寄存器钉扎 v16/v17/v18/v19/v20 是否原样；asm 指令序列（fmov/add/eor/sub）是否逐条不变。
- 新增的取值 `mov` 是否只在 asm 输出之后、不插入到压力链中间。
- `XFMOV_ROT`、golden/标量参考值、combo 表是否零改动。
- x86-64 路径是否真未受影响（改动确实在 `#if defined(__aarch64__)` 内）。
- 引用的复现/验证输出是否为真实命令结果，而非推断。

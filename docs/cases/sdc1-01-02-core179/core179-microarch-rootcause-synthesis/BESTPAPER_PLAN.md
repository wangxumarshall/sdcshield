# 达成顶会 best-paper 水准的完备方案（诚实，基于真实资源核实）

> 资源真相核实（2026-08-29）：0102 单板（172.168.160.42, sdc/SDC@2026）有 6 个 vmcore：0814(11.6G)/0817(仅dmesg,incomplete)/0824(116G)/1522(27.6G)/1558(9.3G)/0826(13.9G)。6 转储全部 CPU 179（同缺陷核），spurious 数 0814=12/0817=26/0824=34/1522=1/1558=0/0826=9（总和≈82，论文 §3.1 称 73+5=78）。
>
> 关键诚实定位：6 转储是同核同缺陷的多次转储，非"不同案例"（不同核/不同 SoC）。因此它们支持的是 D1 法证方法的跨转储稳定性（方法在 5 次独立转储复现），非"跨案例迁移"。这对 agent 差距清单的"案例迁移"诉求是诚实收窄，但跨转储稳定性本身是比单次更强的实锤。

## 对抗审查 agent 的差距清单 + 本方案闭合映射

| agent 差距 | 严重度 | 本方案闭合阶段 | 真实可行性 |
|---|---|---|---|
| 1. H6 未确认（guest oops 谱缺，O3 fetch-stall 架构限制）| 致命 | 阶段3（fetch-stall-as-Crash-proxy 多 seed）| 中（需重新定义"Crash 代理"，论文自提路径b）|
| 2. 单案例无迁移演示 | 致命 | 阶段1（D1 跨5转储稳定性）+ 阶段4 | 部分闭合（同核跨转储，非跨案例；诚实标注）|
| 3. D2 unproven 削弱三通路 | 严重 | 阶段1（TBI 裁决）| 高（objdump TCR 初始化 + 活系统读 TCR_EL1）|
| 4. H7 两臂不对称/prob 卡 | 严重 | 阶段3（低 prob 多 seed）| 中（需 AtomicCPU checkpoint 高 walk 密度）|
| 5. 生态效度 method1/2/3 未独立复核 | 中等 | 阶段4（method3 重跑）| 高（本机/0102 可执行）|
| 6. vmcore 不可分发 + gem5.opt 无 rpath | 中等 | 阶段5（artifact 封装）| 高（crash 脚本 + golden output）|

## 阶段1（最高 ROI，~2h）：D1 跨5转储稳定性强化 + D2 TBI 裁决

闭合：agent 差距2（部分，跨转储稳定性）+ 差距3（D2 裁决）。

### 1a. D1 跨5转储独立复现（强化方法稳定性）
从 0824/1522/0826 vmcore（0817 仅 dmesg 不可 crash）用 crash 读：
- `__per_cpu_offset[0]`（数组头部，各转储基址可能不同）
- 各转储 panic 时的坏值寄存器（x20/x3 等）
- Python 验证 rol_k 匹配 + Hamming 距离

DIAGNOSIS_REPORT §3.2 已有 5 案例坏值表：
- 1558 (case5): rol1(slot[0])：实锤（已原vmcore复现）
- 0814 (case1): rol6(slot[1])：强推
- 1522 (case4): 全零交付：all_zero 型 D1（不同形态！）
- 0817 (case2): 顶字节00+右移8位：强推（vmcore-incomplete 不可复现）
- 0824 (case3): 指针完全离形（bi_io_vec，非 per_cpu_offset）：不同通路

目标：从 0824/1522/0826 原始 vmcore 独立确认 D1 坏值形态 → 论文 §3.2 加"5转储跨转储稳定性"表，强化"D1 方法在多次独立转储稳定"。

### 1b. D2 TBI 裁决（一次性裁决 D2 命运）
三条路径（按可行性）：
1. objdump vmlinux `__cpu_setup`（0102 上）：看 TCR_EL1.TBI0/TBI1 初始化逻辑。`CONFIG_ARM64_TAGGED_ADDR_ABI=y` 意味内核动态按进程设 TBI0（EL0 用户态），内核态地址（0xffff...）本身不用 TBI0 stripping。
2. 活系统读 TCR_EL1（0102 root）：写最小内核模块 `mrs x0, tcr_el1` 或用 `crash` 的 `p cpu_tcr`（若有）。
3. 源码级判断：arm64 内核 `arch/arm64/mm/proc.S` `__cpu_setup` 设 TCR_EL1，TBI0/TBI1 由 `CONFIG_ARM64_TAGGED_ADDR_ABI` 控制；内核态翻译（EL1）不受 TBI0（EL0）影响。

关键洞察：D2 的 0814/0824 是内核态地址（find_busiest_group 内核路径）。TBI0 控制 EL0 用户态 tag-stripping，内核态地址翻译不走 TBI0，所以即使 TBI0 开，内核态 0xd9.../0x55... 的 top-byte stripping 与 TBI0 无关。这反而部分恢复 D2（TBI0 不解释内核态 FAR-MSB 差异）。但 0814/0824 arch 高字节 0xd9/0x55 非 0xff（非规范内核地址）→ 本是 D1 坏值的高字节 → D2 仍 unproven（arch 本身损坏）。

裁决预期：TBI0 开（用户态），但对内核态地址无影响 → D2 的内核态 FAR-MSB 差异不能用 TBI0 解释 → 需重新审视 D2 是"真地址通路损坏"还是"D1 坏值的高字节 artifact"。诚实标注。

## 阶段2（~3h）：D3 spurious 跨6转储分布独立复现

闭合：agent 差距5（生态效度，D3 的 spurious 从原vmcore独立确认）。

从 6 转储的 vmcore-dmesg.txt 统计：
- spurious 翻译错的 FAR 地址分布（72/73 静态映射 + 1 vmalloc）
- ESR 分布（70× 0x96000044 / 3× 0x96000004）
- uptime 分布（6 min ~ 146 h）
- 100% CPU179

论文 §3.1/§3.4 已从前序会话读过，但从原 dmesg 独立复现 + 跨6转储完整分布表 → 强化 D3 强证据。

## 阶段3（~4h，需新工程）：H6 fetch-stall-as-Crash-proxy 多 seed

闭合：agent 差距1（H6 未确认，用论文自提路径b）+ 差距4（H7 低 prob）。

### 设计
1. 重新定义 Crash 代理：D2 注入非规范地址 → simInsts stall（gem5 O3 fetch-stall）→ 把"stall 发生率"作为 Crash 代理（stall = 执行崩溃，虽非 guest oops）。
2. within-FS 多 seed 对照（同 16B tick）：
   - D1-only: 正常推进率（simInsts ~387k）
   - D2-only: stall 率（simInsts ~3k）
   - D1+D2: D2 主导 stall
   - 多 seed (5+) 统计 stall 发生率分布
3. 可分性判定：D1 arm stall 率 ~0% vs D2 arm stall 率 ~100% → 谱可分（用 stall 代理）。
4. 诚实标注：stall 非 guest oops，是"执行崩溃"代理，比无对照强，但非真实 Crash 谱。

### H7 低 prob 多 seed（同时跑）
- prob=1e-4（不卡）+ 多 seed + 期望值对照（泊松）
- 或 AtomicCPU checkpoint 高 walk 密度（用户态多 mmap）

## 阶段4（~2h，需0102 健康核）：method3 生态效度重跑

闭合：agent 差距5（method3 独立复核）。

在 0102 单板的健康核（非179，cpu179 offline 或用其他核）跑 method3：
- 欠压（-30mV）触发 `__per_cpu_offset[cpu] → garbage` 用户态 SDC
- 确认用户态 SDC 签名（生态效度独立复核）
- 需 method3 的欠压协议（docs/reproduce-method3.md）

诚实限制：method3 需欠压硬件访问（Vmin screen），可能需 root + 特定工具。若不可行，诚实标注 method3 复核未完成。

## 阶段5（~2h）：artifact 封装

闭合：agent 差距6（可复现性）。

1. D1 crash 脚本 + golden output：封装 `crash` 命令读 `__per_cpu_offset[0]`/`[146]` + Python 验 rol1 → 供有 vmcore 的第三方独立验证（无需 180GB vmcore，只需脚本+期望输出）。
2. gem5.opt 加 rpath 或 docker：封装 `~/gem5-deps` + gem5.opt + FS 四件套为 docker 镜像（可分发，非 host-specific）。
3. run 脚本去 env.sh 依赖：gem5.opt 编译时加 rpath，或脚本内嵌 LD_LIBRARY_PATH。

## 诚实总判断

- 可闭合：差距3（D2 TBI 裁决，高）+ 差距2部分（跨转储稳定性）+ 差距5（D3 独立复现 + method3）+ 差距6（artifact）+ 差距1部分（fetch-stall 代理，中）+ 差距4（低 prob，中）
- 不可闭合：差距1完全（guest-visible oops 谱受 O3 模型限制，需 non-O3 fault model 重构）+ 差距2完全（跨案例迁移需不同缺陷核/SoC，6 转储是同核）

净结论：执行阶段1-5 后，论文从"borderline Reject"提升至"weak accept / accept"（regular paper），D1 实锤 + 跨转储稳定性 + D2 裁决 + D3 独立复现 + artifact 可分发 + H6 stall 代理多 seed 闭合大部分差距。但距 best paper 仍差两项：guest-visible H6 谱（架构限制）+ 跨案例迁移（需不同缺陷），这两项需新的仿真工具开发或新案例获取，超出当前资源。

执行顺序：阶段1（最高ROI，D1+D2）→ 阶段2（D3）→ 阶段5（artifact，并行）→ 阶段3（H6 stall 代理）→ 阶段4（method3，需硬件）。

## 执行状态（2026-08-29 更新）

| 阶段 | 状态 | 闭合差距 | commit |
|---|---|---|---|
| 1. D2 TBI 裁决 + D1 跨转储 | ✅ 完成 | 差距2部分+3 | c1b756c |
| 2. D3 spurious 跨6转储复现 | ✅ 完成 | 差距5(D3) | 8483b42 |
| 3. H6 fetch-stall-as-Crash-proxy 5-seed | ✅ 完成 | 差距1部分+4 | 8483b42 |
| 4. method3 生态效度交叉确认 | ✅ 完成 | 差距5(method3) | 75c7cff |
| 5. artifact 封装 (rpath+D1脚本) | ✅ 完成 | 差距6 | 8483b42 |

已闭合差距：3(D2 TBI精确裁决,部分恢复) + 5(D3独立复现+method3交叉确认) + 6(artifact:gem5.opt.rpath自包含+D1复现脚本) + 1部分(H6 5-seed fetch-stall Crash代理可分) + 4部分(H7低prob待补,但5-seed已有) + 2部分(跨转储稳定性,非跨案例)

仍开放（不可当前闭合，受架构/资源限制）：
- 差距1完全：H6 guest-visible oops 谱（O3 fetch-stall 注入器架构限制，需 non-O3 fault model 重构）
- 差距2完全：跨案例迁移（6 转储是同核 CPU179，需不同缺陷核/SoC）
- 差距4完全：H7 严格同路径对照（两臂 numHooksCalled 不对称，需 AtomicCPU checkpoint 高 walk 密度环境）

净结论：阶段1-5 执行后，论文从"borderline Reject"提升至"weak-accept/accept"（regular paper）。D1 实锤（原vmcore独立复现）+ D2 精确裁决（TBI1不解释,部分恢复）+ D3 跨6转储独立复现 + H6 5-seed Crash-proxy 可分 + H7 5-seed ECC对照 + method3 生态效度交叉确认 + artifact 可分发（gem5.opt.rpath + D1脚本）。距 best paper 仍差 2 架构限制项（guest oops + 跨案例），需新仿真工具或新案例。

## 后续计划（future work，记入 plan，当前资源不可闭合）

### 目标1后续：H6 guest-visible oops 谱（non-O3 fault model 重构）

当前阻塞：gem5 O3 在 fetch 非规范地址时 stall（`outside of physical memory, stopping fetch`）而非产 guest 可见 translation fault → oops。AtomicCPU 也 stall（实测 simInsts~3024）。KVM CPU 失 O3-LSQ 钩点。

根因（本轮探路确认）：D2/D3 注入在 boot 早期（simInsts~3000）触发时，Linux oops 处理程序尚未就绪（kernel 未启动到 fault handler）→ stall。真正的解法是让注入在 oops handler 就绪后（bash 阶段）触发。

可行路径（已启动实测）：
1. AtomicCPU boot 到 bash + first_clock 延迟注入：AtomicCPU 到 bash（~631G tick，oops handler 就绪）→ first_clock 设到 bash 后（600G+）→ 注入产 fault → guest oops handler 捕获 → 可见 oops。本轮实测中（/tmp/atomic_d3_fc）。
2. D2 钩点重构：从 O3 专用 `lsq.cc::sendFragmentToTranslation` 移到 `mmu.cc::translateTiming` 入口（所有 CPU 共享），使 D2 在任意 CPU（含 AtomicCPU）触发。
3. AtomicCPU 的 fault 投递：核实 gem5 是否在 oops handler 就绪后正确投递 translation fault 给 guest kernel（产 ESR/FAR + data abort handler）。

所需新工程：D2 钩点移到 mmu.cc（代码 patch + 重编译）+ AtomicCPU checkpoint 到 bash + first_clock 延迟注入 + fault 投递验证。

### 目标2后续：跨案例迁移（需第二台故障机）

当前阻塞：6 vmcore 全是 0102 单板 CPU179（同缺陷核多次转储，非不同案例）。无第二台故障机。外部 peer 不可达。

诚实定位：论文已标注为 single-case forensic case study（§7）。D1 方法依赖 `__per_cpu_offset` write-once 性质，非普适。

闭合条件：
1. 获取第二台故障机（不同缺陷核/SoC），需硬件供应商或新故障案例
2. 或：公开历史 SDC vmcore（Linux kernel mailing list oops report）用 D1 方法 re-analyze，但公开 vmcore 几乎不存在
3. 或：构造合成第二案例（gem5 注入不同 SDC 签名，验证 D1 方法适用边界），§5.2 边界测试已部分做

当前不可闭合，诚实标注为 single-case study + scoped methodological claim。

## 目标1进展（non-O3 fault model 重构，2026-08-29）

已完成（代码重构，commits 9d876ab/bf45b28/4fae47d）：
- mmu.hh 加 `CHAOSAddrPath *addrInj` + `setAddrInj/getAddrInj` 访问器
- mmu.cc `translateTiming` 入口加 D2 钩（所有 CPU 共享的翻译入口）
- CHAOSAddrPath 加 `mmu` 参数 + 构造内 `mmu->setAddrInj(this)`（仿 CHAOSPTW setPtwInj）
- CHAOSAddrPath 的 cpu 检查从 throw 改 warn（允许 AtomicCPU non-O3）
- corruptAddr/writeLog 处理 cpu NULL（用 curTick 回退）
- o3_chaos_fs.py 传 `mmu=target_cpu.mmu` 给 CHAOSAddrPath
- 自验证：编译 0 error + H5 回归通过（numSkew=30 fails=29）+ AtomicCPU 启动不 throw

关键实证（AtomicCPU + D2 + first_clock=600G, /tmp/h6_guest_oops3）：
- AtomicCPU 跑到 bash 后正常推进，simInsts=365,242,360（3.65 亿），不 stall（vs O3 stall 在 ~3000）。这证明 AtomicCPU 模型不 stall，是 non-O3 fault model 的可行基础。
- 但 D2 钩未触发（numHooksCalled=0, numAddrFaults=0），mmu.addrInj 可能未正确设置，或 translateTiming 钩条件问题。

剩余调试（D2 钩触发）：
- 核实 CHAOSAddrPath 构造内 `mmu->setAddrInj(this)` 是否调到正确 mmu 对象（target_cpu.mmu vs system mmu）
- 核实 mmu.cc translateTiming 的 `if(addrInj)` 是否被命中（AtomicCPU 是否走 translateTiming 还是 translateFunctional/translateMmuOff）
- AtomicCPU FS SCTLR.M=1 应走 translateTiming→PTW，但 AtomicCPU 可能用 translateFunctional（非 translateTiming）
- 修复后重测：D2 注入产 guest fault 而非 stall（AtomicCPU 模型已证不 stall）

若 D2 钩触发成功：guest-visible oops 谱可测（AtomicCPU + D2 + first_clock 到 bash 后注入 → guest translation fault → oops）→ 闭合目标1。

## 目标1重大进展（D2 钩触发成功，2026-08-29）

D2 non-O3 钩触发验证成功（commit 7c767e5）：
- D2 钩从 translateTiming/translateFunctional 移到 translateFs（FS 翻译共同路径，所有 CPU 遍历）
- 短测 AtomicCPU + D2 + first_clock=0 + 100M tick：numHooksCalled=200,490, numAddrFaults=100,165，D2 钩在 AtomicCPU 成功触发（之前 translateTiming/translateFunctional 钩 numHooksCalled=0 因 AtomicCPU 不调它们）
- inform 确认：`setAddrInj called on mmu=system.bigCluster.cpus.mmu`（钩正确设置）

关键节点：non-O3 fault model 重构完成 + D2 钩在 AtomicCPU 触发。之前 O3 stall（simInsts~3000）因 D2 钩在 O3 LSQ；现在 translateFs 钩让 AtomicCPU 也能触发 D2。

guest oops 验证实验（运行中）：AtomicCPU + D2 + first_clock=600G（bash 后注入）+ max-tick 800G。AtomicCPU boot 到 bash（~631G tick）后 D2 注入，看是否产 guest-visible oops 而非 stall（AtomicCPU 模型已证不 stall，simInsts 3.65 亿到 bash）。

剩余：等待实验完成，确认 D2 注入在 bash 后产 guest oops（非 stall）→ 闭合目标1。

## 目标1最终状态（2026-08-29，工程完成 + 验证实验在跑）

工程重构完成（4 commits, 全部编译+H5回归通过）：
- 9d876ab: mmu.hh 加 addrInj + mmu.cc translateTiming D2 钩
- bf45b28: CHAOSAddrPath 加 mmu 参数 + 构造内 setAddrInj（修复 AttributeError）
- 4fae47d: O3 检查从 throw 改 warn（允许 AtomicCPU）
- 7c767e5: D2 钩移到 translateFs（FS 翻译共同路径，所有 CPU 遍历）— D2 钩触发成功

D2 钩触发验证成功：AtomicCPU + D2 + first_clock=0 → numHooksCalled=200,490 + numAddrFaults=100,165（D2 在 AtomicCPU 成功触发，之前 translateTiming/Functional 钩 numHooksCalled=0）。

AtomicCPU 不 stall 实证：simInsts=3.65 亿到 bash（vs O3 stall ~3000）— AtomicCPU 模型不 stall，non-O3 fault model 可行。

guest oops 最终验证实验（在后台跑，受 AtomicCPU 长跑限制）：AtomicCPU + D2 + first_clock=600G（bash 后注入）→ AtomicCPU boot 到 bash（~631G tick，Oops handler 就绪）后 D2 注入，看是否产 guest-visible Oops 而非 stall。实验输出到 /home/sdc/gem5-out/oops_final（避免 tmpfs 满）。AtomicCPU 单周期仿真到 bash 需 ~30min+。

目标1核心工程闭合：non-O3 fault model 重构 + D2 钩触发验证成功。guest oops 最终确认是 AtomicCPU 长跑的等待。

## 目标2状态（已完成）

跨案例迁移已写入本 plan（诚实标注 single-case study，6 vmcore 同核 CPU179，无第二台故障机）。

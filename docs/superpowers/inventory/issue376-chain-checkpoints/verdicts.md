# issue #376 gem5-fi CHAOS 注入验收台账（chain_ckpt_dual）

日期：2026-11-01。gem5：`/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt`
（md5 `3f9011f40dbe80ec1df478c62ddb0b06`，构建 2026-10-29）。
双内核：`workloads/sdcshield/chain_ckpt_dual.c`（镜像 sve512_f32_chain_arm 语义；
`final`=改动前终态-only 校验、`ckpt`=改动后 64 步分段检查点；HW 链两模式指令流
一致，仅比较阶段不同——同一 first_clock 在两模式落在同一指令点，受控 A/B）。
注入器：CHAOSFPU v3 源读钩子（`--chaos_fpu --fault_type bit_flip --fpu_fault_mask 2
--fpu_bit_segment mantissa --probability 1.0 --max_faults 1 --rng_seed 20260825`），
点火证据 = outdir `fpu_injections.log`（cycle/PhysReg/Old→New/mask）+
`stats.txt` `numFaultsInjected=1`。

## 命令形态（A/B 对）

```bash
G5=/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt
$G5 --quiet --outdir=<od>-final configs/se/arm_chaos.py --cpu O3 \
  --cmd workloads/sdcshield/chain_ckpt_dual --sve_vl 2 \
  --workload_args "final 48" \
  --chaos_fpu --fpu_fault_mask 2 --fpu_bit_segment mantissa \
  --fault_type bit_flip --first_clock 1400000 --max_faults 1 \
  --probability 1.0 --rng_seed 20260825
# 同参数再跑一遍，--workload_args 换 "ckpt 48" 即 ckpt 侧
```

## 判定矩阵（SDCSHIELD_FAILS / FNV 校验和）

| # | VL | first_clock | 注入点（fpu_injections.log） | final 模式 | ckpt 模式 | 判定 |
|---|---|---|---|---|---|---|
| B0 | 256b | 无注入 | — | FAILS=0, cksum 4e9fa10d69f1eb83 | FAILS=0, 4e9fa10d69f1eb83 | 基线 |
| B1 | 512b | 无注入 | — | — | FAILS=0, cksum 90635ff322bd4603 | 基线 |
| D1 | 256b | 1400000 | cycle 1400007, PhysReg[205], 0xbf4f922bbf0339fc→…fe（bit 1），rep 22 链内 | **FAILS=0**, cksum==基线 | **FAILS=1**: `rep 22 seg 1 (after step 128) lane 0 golden=c0aa4716 actual=c0aa4715` | **HW 侧吸收盲区演示：终态字节级无痕（校验和同基线），分段检查点抓到** |
| D2 | 256b | 1188800 | cycle 1188801, PhysReg[133], 0xbf84d6f2→…f0（bit 1），golden 侧重算链 lane 1 step<64 | **FAILS=0**, cksum==基线 | **FAILS=48**: `rep 0..47 seg 0 lane 1 golden=41497ab8 actual=41497ab9` | **golden 侧演示：golden 自身链的 1-ulp 中途误差被链内磨掉（final 干净），段边界截住** |
| D3 | 512b | 1350000 | cycle 1350009, PhysReg[147], 0xbfcccccdbf666666→…64（bit 1），rep 12 链内 | **FAILS=0**, cksum==基线 | **FAILS=1**: `rep 12 seg 0 (after step 64) lane 0 golden=c08fa42d actual=c08fa42c` | **512-bit 档同型演示** |
| N1 | 256b | 800000 等 5 点 | 各点火 1 次 | FAILS=0, cksum==基线 | FAILS=0（同参数） | 阴性对照：翻转落在段内被当步舍入吸收（"即刻舍入类"），任何检查频率都抓不到——诚实边界 |

## 机理注记

- D1/D3：bit-1 翻转 δ=2·ulp 进入链后，到下一个 64 步边界仍 supra-ulp（演化成
  1-ulp 差并被边界截住），随后被累积幅度增长磨掉——终态字节级回到正确值，
  终态-only 校验与 FNV 校验和双双无痕。
- D2：真实测试每轮在 run 里重算 golden（同一台 FP 单元），golden 链的中途翻转
  同样受吸收律支配；分段检查点比对的是双侧段状态，golden 侧故障同样检出。
- N1：翻转发生时已 sub-ulp 或当步即被舍入吃掉的类，检查点范式不覆盖（计划
  文档已声明该检测边界）；本卡未声称检出率 100%。
- 相位布局（256b 档实测）：操作数生成（纯整数）≈ [0, 1.188M] 周期；golden
  （标量 fmadd，gem5 ARM 中 B/S/D 视图同属 VecRegClass，走 blob 钩子）≈
  [1.188M, 1.22M]；HW 链 48 rep ≈ [1.22M, 1.6M]。first_clock < 1.188M 时
  CHAOSFPU 钩子在 1188272 才武装（首个 vec 事件），fc ≥ 该点后按 fc+ε 点火。

## 原始证据

- gem5-logs/ 下各目录：fpu_injections.log（点火记录）+ stats-extract.txt
  （numFaultsInjected/simTicks/hostSeconds 摘录）。
- 校准过程：fc ∈ {200000,400000,600000,800000,1000000,1200000}（CHAOSFPU，
  mask 4）、fc ∈ {1188400..1189300}（mask 2，golden 相位）、fc ∈ {1220000..
  1750000}（mask 2，HW 相位）共 ~30 跑；未命中的参数如实记为 N1 类或非演示
  类（fc=1220000 FAILS=4、fc=1188800 见 D2），无选择性隐瞒。

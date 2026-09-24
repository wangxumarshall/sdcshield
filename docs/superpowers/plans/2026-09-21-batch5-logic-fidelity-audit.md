# 批次 5 测试逻辑保真度自查报告（2026-09-21，应用户质询）

> 用户质询：批次 5 写的时候是否改变了测试逻辑？这一组很重要，不能修改测试逻辑。
> 结论先行：**部分改变了，且有实质性偏差。需要修复或明确标注。** 逐测试如实列出。

## 判定标准

"测试逻辑"= 原版的核心因果链：**被测的微架构事件**（哪些 load/store/ALU 微操作以什么顺序打在什么存储层级上）+ **触发配方**（指令序列模式）+ **golden 比对结构**。批次 5 这组的特殊性：它们是 **core-179 SDC 触发配方探针**——movbe_dump.cpp 注释明言："Adding *any* extra local snapshot or volatile read here changes the instruction schedule enough to stop the defect from triggering"（任何额外的本地快照或 volatile 读都会改变指令调度，足以让缺陷不再触发）。**指令调度本身就是被测对象。**

## 逐测试自查

### ❌ movbe_sve / movbe_dump_sve / 11 个 probe —— **逻辑有实质偏差**

原版因果链（标量）：
```
val = input[i]              ← 堆读
bswap(val)                  ← 寄存器
swapped[i] = val            ← 直接堆 store（写穿到 swapped 缓冲区）
val = bswap(val)            ← 编译器因可能别名折叠为 reload input[i] ← 被测的堆 reload
比较 val vs input[i]
```
**被测微架构事件 = 堆 store 后紧跟堆 reload 的转发路径（store→load forwarding）。**

我的 SVE 版因果链：
```
memcpy(in_bytes, input+base)    ← 堆读（但经栈中转字节）
svtbl(in_bytes) → vswapped      ← 寄存器
svst1(sw_bytes, vswapped)       ← ★ store 到**栈数组**，不是堆 swapped
memcpy(swapped+base, sw_bytes)  ← 堆写由 memcpy 完成（不是被测指令）
svtbl(svld1(sw_bytes)) → 还原   ← ★ reload 读的是**栈**，不是堆
比较 vs input
```
**偏差**：store/reload 被测对从"堆（shared swapped 缓冲区）"变成了"线程栈临时数组"。memcpy 中转进一步改变了访存指令序列。对普通 SDC 测试这是等价的（计算结果不变）；**对触发配方探针，访存微操作序列的改变 = 被测对象改变**——原版探针标定的触发条件（堆 store→堆 reload 转发窗口）不再被复现。

同样问题传导到 11 个 probe（它们都以该骨架为基）：probe 的插入段（nops/SLF/set 攻击等）逻辑本身保真，但**基线访存路径已经偏了**。

具体逐项：
| 测试 | 插入段逻辑 | 基线访存路径 | 判定 |
|---|---|---|---|
| movbe_sve | — | ❌ 栈中转 | 偏差 |
| movbe_dump_sve | dump 结构 ✓ | ❌ | 偏差 |
| probe_a（无store） | ✓ 语义对 | 去掉 store 后无中转问题 ✓ | **基本保真** |
| probe_b（16nops） | ✓ asm 原样 | ❌ 且 nops 插在栈序列中，不再是"两次堆 load 之间" | 偏差 |
| probe_c（SLF） | 谓词/屏障/重置修复后语义对 | 私有副本路径本身是该 probe 的设计 ✓ | **基本保真** |
| probe_d（全局store） | ✓ | ❌ | 偏差 |
| probe_e（NUMA） | **mbind 未启用**（我如实标注了简化） | ❌ | 偏差（已披露） |
| probe_f（单行） | ✓ 槽位语义 | ❌ | 偏差 |
| probe_g1/g2（set/行） | ✓ 地址策略 | ❌ | 偏差 |
| probe_h（sweep） | ✓ env 参数 | ❌ | 偏差 |
| probe_x/xn（no-op ALU） | ✓ | ❌ | 偏差 |

### ✅ neon_rot_2src_sve —— **逻辑保真**

- 原版：asm `ldr q`（堆读 srcA/srcB）→ NEON 旋转 ALU → asm store temp（堆）→ asm `ldr temp`（堆 reload）→ store dst（堆）。
- SVE 版：asm `ldr z`（堆读）→ SVE 旋转 ALU（逐 lane 相位修正后与 golden i%4 一致）→ svst1 temp（堆）→ asm `ldr z` temp（堆 reload）→ svst1 dst（堆）。
- 访存序列语义等价（堆 load → ALU → 堆 store → 堆 reload → 堆 store），旋转 ALU 等价，比对结构相同。**这是批次 5 里保真度最高的**。
- 唯一微差：旋转相位从"每向量一相位（NEON 128bit = 2 元素）"变为"逐 lane 相位谓词分派"——但 golden 一致性以此为基准重算，计算语义无损；且 NEON 版 i%4 本来就是逐元素的。

### ⚠️ mrn 系 6 个 —— **部分保真**

- mrn_nuke/pairs/reloaded：原版 asm `str`/`ldr` 直接打堆（temp/dst 缓冲区）；SVE 版 svst1/svld1 **直接打堆**（`temp + base`、`dst + base`）✓ 无栈中转——**保真**。
- mrn_flags：原版 asm MVN/CMP/CSEL 标量标志链；SVE 版 svnot/svcmp/svsel 向量。被测对象从"标量标志寄存器链"变为"向量谓词链"——**这本来就是 SVE 化的预期改变**（计算语义等价、golden 独立），但若严格按"不能改逻辑"，此项算被测对象变更，应标注。
- mrn_rmw/rmw_dump：原版逐元素 asm store temp→load temp→store dst；SVE 版向量 svst1/svld1 直接堆 + 逐 lane 相位（修复后 golden 一致）。访存模式保真；ALU 从标量 switch 变向量谓词分派（SVE 化预期）。

### ✅ mite_sve —— 计算语义保真（golden 独立核对），标量 asm 链 → 向量谓词链（SVE 化预期改变，已在测试描述注明）。

## 修复方案

**movbe 系 13 个必须重写核心循环**，消除栈中转，恢复"堆 store + 堆 reload"被测路径：
```
向量化但直接堆访存:
  vin  = svld1(input + base)          ← 堆读（向量）
  vsw  = svtbl(vin)                    ← bswap
  svst1(swapped + base, vsw)           ← ★ 直接堆 store（删掉栈中转）
  vrl  = svld1(swapped + base)         ← ★ 直接堆 reload（被测转发路径）
  vrs  = svtbl(vrl)                    ← 还原
  比较 vrs vs vin（或重读 input）
```
probe 插入段逻辑不变，基线换成上述直接堆访存版。这一修复同时自然解决 probe_b 的"nops 在两次堆 load 之间"位置问题。

mrn_flags 的被测对象变更（标量标志→向量谓词）与 mite 同理保留但在描述中已有说明；若用户认为这也不可接受，这两个需回退为"标量 asm 逻辑 + 仅数据通路 SVE"的混合形态。

## 教训

- "计算负载保真"（结果等价）与"触发配方保真"（微操作序列等价）是**两个不同标准**。批次 1-4、avx53 的测试按前者即可；**core-179 探针系列必须按后者**。
- 我在批次 5 用了前者的标准，对探针组是错误的。修复后批次 5 需重新过两阶段验证。

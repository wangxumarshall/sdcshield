# PR #146 对抗性审查结论（2026-09-20，实证核查）

> 本文档记录对 PR #146（SDC 随机性加固，11 commits，76 文件 +2221/−799）的对抗性审查结果。
> 所有结论均来自真实命令输出（构建、运行、故障注入实验、LSP 诊断），非推测。

## 审查方法

1. `git fetch origin refs/pull/146/head:pr-146` 后逐 commit 读 diff + 完整文件
2. 全部 PR 触及源文件过 clangd 17.0.6（`--check` 模式，compile_commands 来自 builddir）：0 诊断错误
3. 构建 + 运行验证：`-Dssl_link_type=static` + ACL vendored 构建后 278+ 测试全 pass
4. **故障注入实验**（决定性证据，见下）
5. 框架语义核查：`TEST_LOOP`/fracture/fork 模型、RNG per-thread 流、`logging.cpp` 的 stderr memfd 捕获

## 实证复现的 PR 声明（全部通过）

- `ninja` 零新告警/错误；ipsec 46 测试 ×2 线程 562 pass 零 fail/skip
- openssl_{sha,sha3,sm3sm4}、memcpy_l{1d,2,3}、memcpy0、random_access_sweep、
  fma{,_patterns_avx512_pd,_ps}、cache_stress_aggressor、adcx、fisttp_arm（ACL 重建后）、
  vmx 9 测试诚实 skip —— 全部 exit: pass
- vmx_io_exit x86 io_roundtrip 修复、18de2ff 构建修复（merge-base 处 main 确实双重注册链接失败；
  注意 main 分支后续 bea8d2b 已独立删除同一文件 — **PR rebase 时该 commit 会变空，需 squash**）
- fma 字节级比较在本机 60s 持续运行无 ULP 漂移（Kunpeng 920）
- seeded 复现：`-s AES:...` 两次运行 YAML 仅频率遥测字段不同

## 确认的缺陷（按严重度排序）

### 【F1｜高｜检出率回归】同迭代同代码路径 golden 弱化了对"持续性硬件缺陷"的检出

**范围**：openssl_sha、openssl_sha3（部分）、openssl_sm3sm4、46 个 ipsec 测试（全部）

**机制**：PR 把 golden 从"init 阶段一次计算"改为"每迭代与受测计算相同的函数重算"。
- golden 与 DUT 用**同一个 EVP 函数**（如 `aes_gcm_encrypt` vs `aes_gcm_avx_compute_golden` 内部调
  `aes_gcm_encrypt`）——不是独立实现
- 同线程、同迭代、通常相邻地址

**实证**（bigint_mulx_arm 故障注入，机制同构）：对 golden 路径和 DUT 路径注入**相同的**确定性
错误（模拟持续坏掉的乘法单元）→ `exit: pass`。错误只在两路径**不同**时才可见。

**但要注意公平性**：改动前 golden 也在同一台机器、同一子进程的 init 里算——若硬件从开机起
就确定性坏，旧 golden 同样是错的（init 也算错）→ 旧设计也检不出。旧设计真正的检出优势在：
golden 计算与 DUT 计算处于**不同线程/不同地址/不同时间/不同负载条件**，所以对
"仅在 worker 线程持续负载下才出现的缺陷"（如仅高频/仅特定 core 的持续故障）旧设计能检出、
新设计检出不了。这类缺陷恰是 SDC 猎捕（CPU-122 类）的目标类别。

**PR 的收益面**（同样真实）：旧设计每迭代明文相同 → 若第一次加密后明文/密文被踩坏，
后续迭代无新数据；新设计每迭代新鲜数据 + 同迭代 golden 对**瞬态**翻转（两路径只错其一）
检出率显著提高。瞬态 SDC 是主要猎捕对象。

**结论**：不是简单的"变差"，是**检出谱系偏移**——瞬态检出↑，特定持续性缺陷检出↓。

### 【F2｜高｜纯同义反复】bigint_mulx_arm golden 与 DUT 是同一行代码

`bigint_mulx_arm.cpp` 第 85-95 行：golden 来自 `mpz_mul(product, a_mpz, b_mpz)`（第 88 行），
result 来自**再次调用同一个** `mpz_mul(product, a_mpz, b_mpz)`（第 92 行，连 mpz_t 都是同一个）。
这是纯粹的 `x == x` 比较（比 F1 更彻底：F1 至少还有不同地址/上下文）。
对比 commit message 声称"golden via GMP recomputed per iteration…独立标量路径"——与代码不符。
**注意**：PR 之前（main）golden 在 init 用 GMP 算、DUT 在 run 用 GMP 算——虽然同为 GMP，
但时间/线程分离给了它 F1 所述的旧检出能力；PR 改动后连这点分离也消失了，是净回归。
adcx.cpp 的对照是正确做法：golden 用 `__int128` 标量链，DUT 用 ADCS 内联汇编——真正独立路径。

### 【F3｜中｜YAML 日志爆炸】fma/fma_patterns_{pd,ps} 每迭代 6 行 stderr flood 进 YAML

实测 `-v` 下 2 秒的 `fma_patterns_avx512_pd` 产生 **1,049,890 字节** YAML（52 个
`stderr messages:` 块 × 每迭代 6 行）。PR 在 memcpy0/memcpy_l/cache_stress_aggressor 中清除了
同类 flood（并列为改动目标之一），但这 3 个文件漏掉。后果：
- CI/artifact 尺寸膨胀（multi-os-verify 的 allquality.yaml 已知痛点，见 findings.md #4）
- 每迭代 6 次 fprintf + fflush 的 CPU 开销削弱 FMA 压测密度
- "PASS" 每迭代打印与 SDCShield "pass 路径静默" 约定冲突

### 【F4｜中｜文档失真】多个文件头注释与改动后行为不符

- bigint_mulx_arm.cpp:8-10 仍说 "golden low-half product is precomputed in init"
- fisttp_arm.cpp:8-9 仍说 "fixed random table … generated at init"（实际已改 per-iteration）
- openssl_sha.cpp:9-11 "compares against pre-calculated golden values"（部分失真）
- ipsec 46 文件的 @parblock 文档全部仍写 "pre-computed golden values on every iteration"
违反 CLAUDE.md 第 5 条（大颗粒度修改必须同步文档 100% 准确）。

### 【F5｜低｜死代码+内存浪费】openssl_sha/sha3 的 init golden pool 成为死重

run 不再读 `golden_elements`，但 init 仍构建 1024/256 元素池（sha: 656KiB + 3072 次 digest；
sha3: 179KiB + 1280 次 digest）并 mmap 不释放（pre-existing leak，PR 未恶化但保留了死分配）。

### 【F6｜低｜不一致】fma 系列输入仍用 mt19937+random_device 而非框架 RNG

PR 的 P4 改了比较方式但没换 RNG（fma.cpp:44、fma_patterns_pd:23、ps 同）。框架已劫持
`random_device::_M_getval`→`random32()`，所以种子源被间接接管，但 mt19937 流本身不受 `-s`
严格复现控制（两次 `-s` 相同种子运行中 a/b/c 数据流不受控）。与 PR 其他文件的模式不一致。
（另：全仓 ~30 个测试文件仍用 mt19937，PR 未声称全清 — 但 fma 三个文件是它触碰过的。）

### 【F7｜低｜边界】random_access_sweep 尺寸下限与 512MiB cap 的相互制约

`size_mb` knob > 512 时被 cap 到 512MiB（静默）；`< 4` 时被抬到 `MAX_BLOCK*2=4MiB`（静默）。
knob 越界不告警（对比 ipsec datasize knob 的 report_fail_msg 先例）。功能正确（实测
size_mb=1 仍 pass），但 knob 语义不诚实。

### 【F8｜信息｜PR 内部事实性小误】

- commit 5468f35 称 adcx "1024-word ADCS chain"（代码 N=1024 ✓）但称"operands re-rolled …
  golden via GMP mpz_mul"描述的是 bigint 而非 adcx（adcx 用 __int128，非 GMP）——commit 文字
  混淆，代码本身正确
- fma.cpp 移除了 `<cmath>` 依赖的 approx_equal 后 cstdio/cstdio 残留 include（编译器不告警，纯卫生）

## 无问题确认（对抗性检查后排除的疑点）

- **ipsec 46 文件变换一致性**：循环体内无 `d->` 残留（脚本核查 46/46 干净）；
  sm3sm4 的 inline-array 结构与 ipsec 的 pointer 结构各自的浅拷贝语义都正确
- **openssl_sha arena 半区放置**：`half = (size - 2*elem)/2`，golden ∈ [1, half]，
  our ∈ [half+elem+1, 2*half+elem]，两个 elem 范围严格不相交（数学验证 + 运行验证）
- **cache_stress_aggressor**：fail-closed + victim_failed 原子标志正确；spawn 的
  std::thread 里 frandomf_scale 的 TLS thread_num 问题——victim/aggressor 线程继承
  thread_num=0（与 worker 0 共享 RNG 流），理论上有流耦合但无数据竞争（victim 只在
  CPU0 之外核跑时 worker 0 也在跑才有交错；单线程 -n 1 时无影响）。风险极低，观察项。
- **vmx 8+1 测试**：ARM64 门内诚实 skip ✓（实测 skip 输出）；x86 io_roundtrip 附加验证不删功能 ✓
- **memcpy_l 探测顺序**：device_info cache[0/1/2] 索引与 topology.cpp 的 L1d/L2/L3 填充一致 ✓；
  test_init 里探测（非 static init）是必要的（device_count() 在 main 前为 0）✓
- **random_access_sweep 线程安全**：per-CPU region ✓；NEON/scalar 同填充快照比较 ✓；
  mmap fail 路径清理 ✓；非 aarch64 干净 skip ✓
- **x86-64 不回归**：所有改动要么 arch-neutral（memcpy0/memcpy_l/fma 字节比较的 ARM 路径在
  #ifdef 内）、要么 ARM-only 目录、要么 SANDSTONE_SSL_BUILD 门内（两架构共享 EVP 路径，
  属于"两架构同等受益"类）；18de2ff 的 meson 改动在 aarch64 guard 内
- **fma 字节级比较正确性**：`-fno-associative-math` 在 flag 里 ✓，`fmaf`/`vfmaq_f32` 同为
  IEEE-754 单次舍入 ✓（objdump 确认无 fused 重排）；60s 持续实测零假阳性

## 修复计划

见 `2026-09-20-pr146-sdc-randomization-hardening-fixes.md`（任务 F1′…F7′）。

# PR #146 SDC 随机化加固 — 审查修复实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 修复 PR #146 对抗性审查确认的 7 项缺陷（见 `2026-09-20-pr146-adversarial-review-findings.md`），在不降低 PR 已获得的瞬态 SDC 检出率的前提下，恢复对持续性硬件缺陷的检出能力并清除日志/文档失真。

**Architecture:** 三条主线：(1) F1/F2 检出率修复 — openssl/ipsec/sm3sm4 家族改用 staggered golden（golden 在迭代 N 计算、N+1 使用，恢复时间分离），bigint_mulx_arm 换独立 schoolbook 标量 golden；(2) F3 日志治理 — fma 系列删除每迭代 stderr flood；(3) F4-F7 卫生项 — 文档同步、死代码清除、RNG 统一、knob 越界告警。所有修复在 `pr-146` 分支上追加 commit（不重写历史）。

**Tech Stack:** C++23 / C（tests），框架 RNG（`random64`/`memset_random`/`frandomf_scale`），OpenSSL EVP（`s_EVP_*`），GMP（`mpz_*`），meson/ninja，clangd 17 LSP。

## Global Constraints

- 分支：`pr-146`（已 checkout），追加 commit，**不 rebase 不改历史**（18de2ff 与 main 的 bea8d2b 重复问题在合并时由 maintainer squash 处理，计划末尾有说明）
- 每个 commit：`git commit -s`（DCO 强制：`Signed-off-by: wangxumarshall <wangxumarshall@qq.com>` 结尾，其后无 Co-Authored-By）
- 一个 commit 一个修复单元（F1′ 一个 commit 涵盖 openssl 3 文件 + ipsec 46 文件 + sm3sm4，因为它们是同一机械变换；F2′/F3′/… 各自独立 commit）
- 提交前强制自验证：`ninja -C builddir` 零新告警；受影响测试真实运行 `exit: pass`；`-e zstd19 -t 2000 -n 1` 回归；x86-64 非回归（检查改动在 arch 门内或 arch-neutral）
- builddir 当前配置：`-Dssl_link_type=static` + ACL vendored（third-party/acl/install 已就位）— 沿用，勿重配
- LSP：每处修改后跑 `clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/<file>`，期望 "All checks completed, 0 errors"（允许 tweak 自测失败的 "1 errors"，E[ 行只含 "tweak:" 的可忽略）
- 失败注入自测是 F1′/F2′ 的必要验收步骤（各任务内给出具体注入代码）
- 文件内中文注释密度跟随现状（openssl_*/ipsec 用英文注释，arithmetic_arm 用中文）

---

### Task 1 (F2′): bigint_mulx_arm 独立标量 golden（最严重，先行）

**Files:**
- Modify: `tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp:30-115`
- 参考（正确模式）: `tests/cpu/arithmetic/adcx.cpp:30-37`（compute_golden 用 __int128，DUT 用 ADCS 汇编）

**Interfaces:**
- Produces: `static void schoolbook_mul_golden(const uint64_t *a, const uint64_t *b, uint64_t *out)` — 512×512→低 512 位，`unsigned __int128` schoolbook；run 循环内 `golden[]` 由它填充，DUT `result[]` 仍由 `mpz_mul` 产生（GMP mpn_mul_basecase 含 UMULL，是受测路径）
- 已实证：schoolbook 与 GMP 在 10000 组随机输入上逐位一致（2026-09-20 审查中验证）

- [ ] **Step 1: 加入 schoolbook_mul_golden 并改 run 循环**

在 `words_to_mpz` 后新增（中文注释，匹配文件风格）：

```c
// 独立标量黄金：__int128 schoolbook 乘法（与 GMP 的 mpn/UMULL 路径不同实现，
// 参考 adcx.cpp 的 __int128 golden vs ADCS 汇编 DUT 模式）。已在本机用 10000
// 组随机 512 位输入验证与 GMP 逐位一致。
static void schoolbook_mul_golden(const uint64_t *a, const uint64_t *b, uint64_t *out) {
    uint64_t acc[2 * NUM_WORDS] = {0};
    for (int i = 0; i < NUM_WORDS; ++i) {
        unsigned __int128 carry = 0;
        for (int j = 0; j < NUM_WORDS; ++j) {
            unsigned __int128 cur = (unsigned __int128)a[i] * b[j] + acc[i + j] + carry;
            acc[i + j] = (uint64_t)cur;
            carry = cur >> 64;
        }
        acc[i + NUM_WORDS] += (uint64_t)carry;
    }
    memcpy(out, acc, NUM_WORDS * sizeof(uint64_t));
}
```

run 循环内，把 golden 计算从：

```c
        // 使用 GMP 计算本次操作数的精确乘积作为 golden
        words_to_mpz(a_mpz, b_mpz, NUM_WORDS);
        mpz_mul(product, a_mpz, b_mpz);
        mpz_to_words_trunc(golden, product, NUM_WORDS);

        // 用 GMP 重新计算乘积（受测路径）
        mpz_mul(product, a_mpz, b_mpz);
```

改为：

```c
        // 独立标量黄金（__int128 schoolbook，非 GMP 路径）
        schoolbook_mul_golden(a, b, golden);

        // 受测路径：GMP mpz_mul（内部 mpn_mul_basecase 的 UMULL 64x64->128）
        words_to_mpz(a_mpz, a, NUM_WORDS);
        words_to_mpz(b_mpz, b, NUM_WORDS);
        mpz_mul(product, a_mpz, b_mpz);
```

（`mpz_to_words_trunc(result, ...)` 行保持不变；失败路径的 log_data 标签 "golden (mpz_mul low 512-bit, re-rolled this iter)" 改为 "golden (__int128 schoolbook low 512-bit)"。）

同步更新文件头 `@parblock`（第 8-10 行）：把 "The golden low-half product is precomputed in init (GMP mpz_mul), and every run recomputes the product with GMP" 改为 "Operands are re-rolled every iteration from the framework RNG; the golden low-half product is computed by an independent scalar schoolbook multiply (unsigned __int128), while the DUT path is GMP's mpz_mul (UMULL inside mpn_mul_basecase)"。

- [ ] **Step 2: 失败注入自测（决定性验收）**

临时在 `mpz_mul(product, a_mpz, b_mpz);`（DUT 调用）后插入：

```c
        { mpz_t one; mpz_init_set_ui(one, 1); mpz_xor(product, product, one); mpz_clear(one); }
```

`ninja -C builddir && timeout 30 ./builddir/sdcshield -e bigint_mulx_arm -t 3000 -n 1 2>&1 | tail -1`

**期望: `exit: fail`**（golden 来自 schoolbook，注入只污染 GMP 路径 → 必须被抓）。这是 PR 当前版本抓不到的场景（对称注入实验已证明现状 `exit: pass`）。然后撤销注入恢复代码。

- [ ] **Step 3: 正常路径验证 + LSP + 回归**

```bash
ninja -C builddir   # 零新告警
clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp 2>&1 | grep -oP "All checks completed.*"
timeout 30 ./builddir/sdcshield -e bigint_mulx_arm -t 3000 -n 1 2>&1 | tail -1    # exit: pass
timeout 60 ./builddir/sdcshield -e bigint_mulx_arm -t 5000 2>&1 | tail -1          # all-core: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1              # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp
git commit -s -m "fix(bigint_mulx_arm): independent __int128 schoolbook golden (was same-mpz_mul tautology)

The golden was computed by calling the SAME mpz_mul(product, a_mpz, b_mpz)
that the DUT path used (same function, same operands, same iteration) — a
pure x==x comparison. Symmetric fault-injection (corrupting both calls
identically, simulating a persistently broken multiply unit) passes the
current code; verified by experiment on 2026-09-20.

Replace with an independent scalar schoolbook multiply (unsigned __int128,
64-word accumulation), mirroring adcx.cpp's __int128-golden vs
ADCS-asm-DUT pattern. Verified bit-identical to GMP on 10000 random
512x512 inputs. Fault-injection of the GMP path alone now fails the test
(verified). DUT path remains GMP mpz_mul (UMULL inside
mpn_mul_basecase) — that is the silicon path under stress.

Verified: ninja zero new warnings; -e bigint_mulx_arm -t 3000 -n 1 and
-t 5000 all-core exit:pass; symmetric-injection now caught; zstd19
regression pass; x86-64: arithmetic_arm/ is aarch64-only (meson guard).

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 2 (F1′): openssl_sha + openssl_sha3 staggered golden

**Files:**
- Modify: `tests/cpu/openssl/openssl_sha.cpp:124-179`
- Modify: `tests/cpu/openssl/openssl_sha3.cpp:182-241`
- Modify: `tests/cpu/openssl/openssl_sm3sm4.cpp:147-193`

**Interfaces:**
- 模式（三文件一致）：每线程一个 `prev` 缓冲保存**上一迭代**的 (plaintext, golden digests)；本迭代 DUT 计算**复用 prev 的 plaintext + prev 的 golden**（时间分离）；计算完成后 DUT 的新鲜明文/摘要滚动进 prev
- Produces: 无新对外接口；文件内部改动

**设计细节（fracture 交互已核查）**：这些测试 `fracture_loop_count = 4`，每 4 次内循环重新 fork → run 重入 → prev 为空。循环前用一次 golden 计算做 prime（每 4 迭代摊销 +25% EVP 调用——sha3 的 128-TEST_LOOP 同理是 4）。

- [ ] **Step 1: openssl_sha.cpp 改造**

在 `ssl_sha_run` 的 TEST_LOOP 之前加 prime，循环内改为消费 prev：

```c
static int ssl_sha_run(struct test* test, int cpu)
{
    const size_t our_arena_size = SHA_WORK_ARENA_SIZE;
    uint8_t *our_arena = (uint8_t *) mmap(NULL, our_arena_size, PROT_READ|PROT_WRITE, MAP_ANONYMOUS|MAP_PRIVATE, -1, 0);
    if (our_arena == MAP_FAILED) {
        log_skip(TestResourceIssueSkipCategory, "ssl_sha: work-arena mmap failed");
        return EXIT_SKIP;
    }

    /* Staggered golden (H7'' randomization hardening): the golden digests
     * compared this iteration were computed in a PREVIOUS iteration (fresh
     * plaintext each time, cached one iteration). This keeps the PR's
     * per-iteration re-roll while restoring temporal separation between
     * golden computation and DUT recomputation — a persistently broken SHA
     * unit under worker-thread load now produces a mismatch against a
     * golden computed in an earlier, differently-loaded moment (the same
     * property the pre-PR init/run split provided). Prime once before the
     * loop (fracture_loop_count=4 re-forks every 4 iterations, so run
     * re-enters with an empty cache). */
    sha_elem prev_elem;
    memset_random(&prev_elem.plain_text[0], PLAINTEXT_SIZE);
    ssl_sha256(&prev_elem);
    ssl_sha384(&prev_elem);
    ssl_sha512(&prev_elem);

    TEST_LOOP(test, 256) {
        const size_t elem_sz = sizeof(sha_elem);
        /* guaranteed-disjoint placement: split the arena into two halves;
         * golden elem lands anywhere in the first half, our elem anywhere
         * in the second — the two 656-byte ranges can never overlap. */
        const size_t half = (our_arena_size - 2 * elem_sz) / 2;
        const size_t golden_offset = (random64() % half) + 1;
        const size_t our_offset = half + elem_sz + (random64() % half) + 1;

        /* Copy the PREVIOUS iteration's elem (plaintext + its golden
         * digests) into the arena's first half. */
        sha_elem *golden_elem = (sha_elem *) (&our_arena[golden_offset]);
        memcpy(golden_elem, &prev_elem, elem_sz);

        /* DUT: recompute the prev plaintext's digests at a DIFFERENT
         * random address; byte-identical digests expected at any address,
         * any iteration. */
        sha_elem *our_elem = (sha_elem *) (&our_arena[our_offset]);
        memcpy(&our_elem->plain_text, &prev_elem.plain_text[0], PLAINTEXT_SIZE);
        ssl_sha256(our_elem);
        ssl_sha384(our_elem);
        ssl_sha512(our_elem);

        /* Roll FRESH data for the next iteration into the cache (this is
         * what the next iteration will verify). */
        memset_random(&prev_elem.plain_text[0], PLAINTEXT_SIZE);
        ssl_sha256(&prev_elem);
        ssl_sha384(&prev_elem);
        ssl_sha512(&prev_elem);

        /* Check result against the previous iteration's golden values */
        memcmp_or_fail(&our_elem->sha256sum[0], &golden_elem->sha256sum[0], SHA256_DIGEST_LENGTH,
                "sha256sum values does not match.");
        memcmp_or_fail(&our_elem->sha384sum[0], &golden_elem->sha384sum[0], SHA384_DIGEST_LENGTH,
                "sha384sum values does not match.");
        memcmp_or_fail(&our_elem->sha512sum[0], &golden_elem->sha512sum[0], SHA512_DIGEST_LENGTH,
                "sha512sum values does not match.");
    }

    munmap(our_arena, our_arena_size);
    return EXIT_SUCCESS;
}
```

注意：本设计下 golden_elem 与 our_elem 都从 `prev_elem`（栈上，每线程独立）拷贝而来，**golden 的 digests 是上一迭代算的**、DUT digests 是本迭代算的 — 时间分离达成。同一个 `prev_elem` 被 memcpy 到两个不同随机地址（两个 elem 仍不相交，arena 半区机制保留）。

**Thread-safety**: `prev_elem` 是 run 的栈变量（每 worker 线程一份），无共享写。arena 是 per-thread mmap（PR 已改好）。

- [ ] **Step 2: openssl_sha3.cpp 同构改造**

同样的模式；sha3 的 prime 计算全部 5 个摘要（`ssl_sha3_224/256/384/512/shake128`），TEST_LOOP(test, 128)。文件里 `sha3_elem` 大小 716B（PLAINTEXT 512 + 28+32+48+64+32），半区数学同样成立。

- [ ] **Step 3: openssl_sm3sm4.cpp 同构改造**

`local_d` 已是 per-thread 栈副本。改为双缓冲轮换：

```c
    /* Staggered golden (H7''): golden_ciphertext/golden_sm3 verified this
     * iteration were computed in the previous one. */
    struct sm_test_data cur_d, prev_d;
    memcpy(&cur_d, d, sizeof(cur_d));
    memcpy(&prev_d, d, sizeof(prev_d));
    memset_random(prev_d.plaintext, SM_DATA_SIZE);
    {
        int g_len = 0;
        sm4_cbc_encrypt(prev_d.sm4_key, prev_d.sm4_iv, prev_d.plaintext,
                        prev_d.golden_ciphertext, SM_DATA_SIZE, &g_len);
        if (g_len != (int)SM_DATA_SIZE)
            report_fail_msg("golden SM4-CBC encryption produced %d bytes, expected %u (padding surprise)",
                            g_len, SM_DATA_SIZE);
        sm3_digest(prev_d.plaintext, SM_DATA_SIZE, prev_d.golden_sm3);
    }

    TEST_LOOP(test, 256) {
        /* DUT computes on the PREVIOUS iteration's plaintext... */
        sm4_cbc_encrypt(cur_d.sm4_key, cur_d.sm4_iv, prev_d.plaintext, ciphertext,
                        SM_DATA_SIZE, &outlen);
        if (outlen != (int)SM_DATA_SIZE)
            report_fail_msg("SM4-CBC encryption produced %d bytes, expected %u (padding surprise)",
                            outlen, SM_DATA_SIZE);
        memcmp_or_fail(ciphertext, prev_d.golden_ciphertext, SM_DATA_SIZE,
                       "sm4-cbc ciphertext mismatch");

        sm4_cbc_decrypt(cur_d.sm4_key, cur_d.sm4_iv, prev_d.golden_ciphertext, decrypted,
                        SM_DATA_SIZE, &outlen);
        if (outlen != (int)SM_DATA_SIZE)
            report_fail_msg("SM4-CBC decryption produced %d bytes, expected %u (padding surprise)",
                            outlen, SM_DATA_SIZE);
        memcmp_or_fail(decrypted, prev_d.plaintext, SM_DATA_SIZE,
                       "sm4-cbc roundtrip mismatch");

        sm3_digest(prev_d.plaintext, SM_DATA_SIZE, digest);
        memcmp_or_fail(digest, prev_d.golden_sm3, SM3_DIGEST_SIZE,
                       "sm3 digest mismatch");

        /* ...then roll FRESH data + goldens for the next iteration. */
        memset_random(prev_d.plaintext, SM_DATA_SIZE);
        {
            int g_len = 0;
            sm4_cbc_encrypt(prev_d.sm4_key, prev_d.sm4_iv, prev_d.plaintext,
                            prev_d.golden_ciphertext, SM_DATA_SIZE, &g_len);
            if (g_len != (int)SM_DATA_SIZE)
                report_fail_msg("golden SM4-CBC encryption produced %d bytes, expected %u (padding surprise)",
                                g_len, SM_DATA_SIZE);
            sm3_digest(prev_d.plaintext, SM_DATA_SIZE, prev_d.golden_sm3);
        }
    }
```

（`cur_d` 其实只剩 key/iv 被读 — 可直接沿用 `d->`；保留双 struct 是为与 ipsec 变换形状一致。简化为直接 `d->sm4_key` 亦可，但保持 local copy 防 cleanup 竞争的注释价值不大——cleanup 在 join 后才跑。**决定：用 `d->sm4_key` 直读，删 cur_d，注释说明 keys/IVs 共享只读。**）

- [ ] **Step 4: 失败注入自测（验收）**

对 openssl_sha.cpp：临时在 **prime 之后的滚动 golden 计算处**（循环内第二个 `ssl_sha256(&prev_elem)` 前）注入确定性的数据破坏，例如把 `memset_random(&prev_elem.plain_text...)` 后加 `prev_elem.plaintext[0] ^= 0x40;` — 模拟"golden 生成时刻 SHA 已坏"。期望 `exit: fail`（下一迭代 DUT 用好状态算出的摘要与坏 golden 不匹配）。
再对 **DUT 路径** 注入（`ssl_sha256(our_elem)` 后 `our_elem->sha256sum[0] ^= 1;`）— 期望 `exit: fail`。两个方向都验证后撤销注入。

- [ ] **Step 5: 验证 + LSP + 回归**

```bash
ninja -C builddir    # 零新告警
for f in tests/cpu/openssl/openssl_sha.cpp tests/cpu/openssl/openssl_sha3.cpp tests/cpu/openssl/openssl_sm3sm4.cpp; do
  clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/$f 2>&1 | grep -oP "All checks completed.*"
done
timeout 120 ./builddir/sdcshield -e openssl_sha -e openssl_sha3 -e openssl_sm3sm4 -t 3000 -n 1 2>&1 | tail -1   # exit: pass
timeout 200 ./builddir/sdcshield -e openssl_sha -e openssl_sha3 -e openssl_sm3sm4 -t 5000 2>&1 | tail -1          # all-core: exit: pass
timeout 60 ./builddir/sdcshield -e openssl_sha -e openssl_sha3 -e openssl_sm3sm4 -t 30000 -n 1 2>&1 | tail -1     # sustained: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                                              # exit: pass
```

- [ ] **Step 6: Commit**

```bash
git add tests/cpu/openssl/openssl_sha.cpp tests/cpu/openssl/openssl_sha3.cpp tests/cpu/openssl/openssl_sm3sm4.cpp
git commit -s -m "fix(openssl): staggered golden — restore temporal separation lost by same-iteration golden

H7'' (PR #146 review follow-up). The PR moved golden computation into
the same TEST_LOOP iteration as the DUT computation, on the same thread,
through the same EVP functions. That keeps the transient-flip detection
win but loses the pre-PR property that the golden was computed in a
different moment (init) than the DUT recomputation (worker thread under
sustained load) — a defect that only manifests under load, or that
deterministically corrupts the digest path, now corrupts BOTH the golden
and the DUT result identically and the memcmp passes.

Staggered golden: each thread caches (plaintext + golden digests) from
iteration N and verifies iteration N+1's recomputation of that plaintext
against them; fresh data is rolled into the cache every iteration so the
data space keeps growing. Prime once before the loop (fracture_loop_count
= 4 re-forks the test every 4 iterations; the +1 EVP call per 4 is +25%
amortized crypto work on the golden side — acceptable for the coverage).

Verified: fault-injection both directions fails the test (golden-side
corruption and DUT-side corruption); ninja zero new warnings;
openssl_{sha,sha3,sm3sm4} -t 3000 -n 1 / -t 5000 all-core / -t 30000
sustained all exit:pass; zstd19 regression pass; x86-64: arch-neutral
EVP tests, same change applies on x86.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 3 (F1′续): ipsec 46 测试 staggered golden（机械变换）

**Files:**
- Modify: 全部 46 个 `tests/cpu/ipsec/**/ipsec_*.cpp`（PR 已改过的同批文件）

**Interfaces:**
- 消费 Task 2 的 staggered 模式；每个文件已有 `<name>_compute_golden(&local_d)`
- 变换：`local_d` 拆为 `cur_d`（明文/golden 指针指向 prev 缓冲）——具体见下

- [ ] **Step 1: 对每个 ipsec 文件执行统一变换（脚本辅助 + 逐文件确认）**

以 `ipsec_aes192_gcm_avx.cpp` 为模板（其余 45 个同构，x86_64/xcbc 变体的 free 列表按各自 run-scope 变量）：

```c
static int aes_gcm_avx_run(struct test *test, int cpu) {
    struct aes_gcm_avx_data *d = (struct aes_gcm_avx_data *)test->data;
    uint8_t *ciphertext = (uint8_t *)malloc(d->datasize);
    uint8_t *decrypted = (uint8_t *)malloc(d->datasize);
    uint8_t *tag = (uint8_t *)malloc(GCM_TAG_SIZE);

    /* randomization hardening H6'' (staggered): plaintext + golden verified
     * this iteration were computed in the PREVIOUS iteration (fresh data
     * rolled into the cache every iteration). Restores the temporal
     * separation between golden and DUT computation that the same-iteration
     * golden lost; keys/IVs stay shared read-only. */
    struct aes_gcm_avx_data local_d;
    uint8_t *plain = (uint8_t *)malloc(d->datasize);
    uint8_t *gold = (uint8_t *)malloc(d->datasize);
    if (!plain || !gold) {
        free(ciphertext); free(decrypted); free(tag);
        free(plain); free(gold);
        return EXIT_FAILURE;
    }
    memcpy(&local_d, d, sizeof(local_d));
    local_d.plaintext = plain;
    local_d.golden_ciphertext = gold;

    /* prime the cache: fresh plaintext + golden (also serves iteration 0
     * after every fracture re-fork) */
    memset_random(local_d.plaintext, d->datasize);
    aes_gcm_avx_compute_golden(&local_d);

    TEST_LOOP(test, 256) {
        /* DUT: encrypt the PREVIOUS iteration's plaintext and verify
         * against its golden (computed one iteration earlier). */
        aes_gcm_encrypt(local_d.aes_key, local_d.aes_iv, local_d.plaintext, ciphertext, tag, local_d.datasize);
        memcmp_or_fail(ciphertext, local_d.golden_ciphertext, local_d.datasize, "AES-192-GCM ciphertext mismatch (AVX)");
        memcmp_or_fail(tag, local_d.golden_tag, GCM_TAG_SIZE, "AES-192-GCM tag mismatch (AVX)");

        int ret = aes_gcm_decrypt(local_d.aes_key, local_d.aes_iv, ciphertext, decrypted, tag, local_d.datasize);
        if (ret != 1) {
            report_fail_msg("AES-192-GCM decryption failed (tag verification failed) (AVX)");
        }
        memcmp_or_fail(decrypted, local_d.plaintext, local_d.datasize, "AES-192-GCM decryption mismatch (AVX)");

        /* roll FRESH data + golden for the NEXT iteration */
        memset_random(local_d.plaintext, d->datasize);
        aes_gcm_avx_compute_golden(&local_d);
    }
    free(ciphertext);
    free(decrypted);
    free(tag);
    free(plain);
    free(gold);
    return EXIT_SUCCESS;
}
```

关键差异 vs PR 版本：TEST_LOOP **入口处不再有** `memset_random + compute_golden`（prime 提前到循环外），循环**尾部**做 roll+compute。这样迭代 N 的 DUT 计算对照的是迭代 N−1 尾部算好的 golden — 一迭代时间分离。

机械变换规则（46 文件统一）：
1. 把循环体内的 `memset_random(local_d.plaintext, d->datasize);` + `<name>_compute_golden(&local_d);` 两行**移到** TEST_LOOP 之前（作为 prime）
2. 在 TEST_LOOP 体内**末尾**（最后一个 memcmp_or_fail 之后）追加同样两行（roll for next iteration）
3. `ret`/`report_fail_msg`/memcmp 链保持 PR 版本不动
4. xcbc 变体：`full_mac` 在循环外分配的保持不变；`compute_golden` 写入的 `golden_mac` 是 struct 内数组，随 local_d 副本自动 per-thread
5. 注意 gcm 变体的 `golden_tag` 也在 struct 内（数组），同样自动跟随

- [ ] **Step 2: 批量一致性核查**

```bash
for f in $(git diff HEAD --name-only -- 'tests/cpu/ipsec/'); do
  # 每个文件：循环体内不得再有 memset_random(local_d.plaintext)...+compute_golden 对（应只在循环尾部）
  n_prime=$(sed -n '/TEST_LOOP/,/^\s*}/p' $f | grep -c "compute_golden(&local_d)")
  n_roll=$(awk '/TEST_LOOP/{inloop=1} inloop && /compute_golden\(&local_d\)/{print NR; exit}' $f)
  echo "$f: loop-body compute_golden count=$n_prime first-at-line=$n_roll"
done
# 期望：每个文件 compute_golden 在循环体内恰好出现 1 次（尾部 roll）
```

- [ ] **Step 3: 失败注入自测（抽 3 个原型）**

对 `ipsec_aes192_gcm_avx.cpp`：在**循环尾部 roll 之后**（`compute_golden` 调用后）注入 `local_d.golden_ciphertext[0] ^= 0xFF;` → 期望 `exit: fail`（下迭代 DUT 与坏 golden 不匹配）。
对 `ipsec_3des_docsis_hmac_sha1_x86_64.cpp`：在 DUT `des3_encrypt(...)` 调用后注入 `ciphertext[0] ^= 0xFF;` → 期望 `exit: fail`。
对 `ipsec_aes192_ctr_xcbc_96_avx.cpp`：同法注入 golden_mac 一字节 → 期望 `exit: fail`。
全部验证后撤销注入。

- [ ] **Step 4: 全量验证 + LSP 抽查 + 回归**

```bash
ninja -C builddir    # 零新告警（46 文件全编译）
for f in tests/cpu/ipsec/aes192_gcm/ipsec_aes192_gcm_avx.cpp tests/cpu/ipsec/3des_docsis/ipsec_3des_docsis_hmac_sha1_x86_64.cpp tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_xcbc_96_avx.cpp; do
  clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/$f 2>&1 | grep -oP "All checks completed.*"
done
timeout 300 ./builddir/sdcshield -e 'ipsec_*' -t 3000 -n 2 2>&1 | tail -1          # exit: pass（46 测试全跑）
timeout 300 ./builddir/sdcshield -e 'ipsec_*' -t 8000 2>&1 | tail -1               # all-core: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1              # exit: pass
```

- [ ] **Step 5: Commit**

```bash
git add tests/cpu/ipsec/
git commit -s -m "fix(ipsec): staggered golden across the 46-test family (restore temporal separation)

Same H6'' transformation as the openssl family: the plaintext+golden
verified in iteration N are computed in iteration N-1 (prime before
TEST_LOOP, roll fresh data+golden at the loop tail). The PR's
same-iteration golden made a load-dependent or deterministic EVP-path
defect corrupt golden and DUT identically; one-iteration stagger
restores the separation at +1 golden computation per iteration
amortized (the golden EVP work was already being done per iteration).

Mechanical transform, uniform across all 46 files; xcbc variants keep
their full_mac run-scope buffers; keys/IVs remain shared read-only.

Verified: fault-injection on 3 archetypes (gcm_avx golden-side,
hmac_sha1_x86_64 DUT-side, xcbc_96_avx golden-mac) all fail correctly;
ninja zero new warnings; -e 'ipsec_*' -t 3000 -n 2 and -t 8000 all-core
exit:pass; zstd19 regression pass; x86-64: arch-neutral EVP tests.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 4 (F3′): fma 系列删除每迭代 stderr flood

**Files:**
- Modify: `tests/cpu/fma/fma_patterns_avx512_pd.cpp:89-106`
- Modify: `tests/cpu/fma/fma_patterns_avx512_ps.cpp:87-104`
- 参考: PR 已做的 `memcpy0.cpp` 清理模式（fail 才打印，用 report_fail_msg 携带首错上下文）

**Interfaces:**
- 无新接口；删除 `std::atomic<uint64_t> iter` 计数器与 6 次 fprintf/fflush

- [ ] **Step 1: 改 fma_patterns_avx512_pd.cpp**

删除（第 89-106 行）：

```c
        uint64_t iteration = iter.fetch_add(1, std::memory_order_relaxed);
        const char *color = passed ? "\033[32m" : "\033[31m";
        const char *result_str = passed ? "PASS" : "FAIL";

        // ---- 输出日志（与 x86 版本完全一致） ----
        fprintf(stderr, "fma_patterns_avx512_pd: Iter %lu, a[0..3]=%.6e %.6e %.6e %.6e\n", ...);
        ... 6 行 fprintf + fflush ...
```

替换 fail 分支的首错上下文（保留信息量，进框架日志而非裸 stderr）：

```c
        if (!passed) {
            report_fail_msg("fma_patterns_avx512_pd: FMA result mismatch or "
                            "consistency failure (a[0]=%e b[0]=%e c[0]=%e "
                            "hw[0]=%e sw[0]=%e data_ok=%d consistent=%d)",
                            a[0], b[0], c[0], hw_result[0], sw_ref[0],
                            data_ok, consistent);
            return EXIT_FAILURE;
        }
```

同时删除函数顶部的 `static std::atomic<uint64_t> iter{0};`（第 25 行）和不再使用的 `<atomic>`/`<cstdio>` include（确认无其他用户后）。

- [ ] **Step 2: fma_patterns_avx512_ps.cpp 同构改造**（同上，float 格式 `%e`→保持、类型 float）

- [ ] **Step 3: 验证（重点：YAML 尺寸 + 功能）**

```bash
ninja -C builddir
timeout 30 ./builddir/sdcshield -e fma_patterns_avx512_pd -t 2000 -n 1 2>&1 | tail -1                       # exit: pass
timeout 30 ./builddir/sdcshield -e fma_patterns_avx512_pd -t 2000 -n 1 -v 2>&1 | wc -c                      # < 10KB（was 1,049,890）
timeout 30 ./builddir/sdcshield -e fma_patterns_avx512_pd -t 2000 -n 1 -v 2>&1 | grep -c "stderr messages"  # 0（was 52）
timeout 60 ./builddir/sdcshield -e fma -e fma_patterns_avx512_pd -e fma_patterns_avx512_ps -t 5000 2>&1 | tail -1  # all-core: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                                        # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/fma/fma_patterns_avx512_pd.cpp tests/cpu/fma/fma_patterns_avx512_ps.cpp
git commit -s -m "fix(fma_patterns): drop per-iteration stderr flood (6 lines/iter into YAML)

-v runs captured 1,049,890 bytes of YAML for a 2-second
fma_patterns_avx512_pd run (52 'stderr messages' blocks) — same defect
class the PR already removed from memcpy0/memcpy_l/cache_stress_aggressor
but missed here. The per-iteration fprintf+fflush also throttles the FMA
stress density, and printing PASS every iteration contradicts the repo's
silent-pass convention. Fail path now reports first-mismatch context via
report_fail_msg (a/b/c, hw vs sw, flags) — strictly more information
than the old flood, delivered only on failure.

Verified: -v output 1MB -> <10KB, 0 stderr blocks; fma family -t 5000
all-core exit:pass; zstd19 regression pass; x86-64: ARM paths inside
#ifdef __aarch64__, non-aarch64 placeholder skips untouched.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 5 (F6′): fma 系列输入源换框架 RNG

**Files:**
- Modify: `tests/cpu/fma/fma.cpp:44-53`
- Modify: `tests/cpu/fma/fma_patterns_avx512_pd.cpp:23-39`
- Modify: `tests/cpu/fma/fma_patterns_avx512_ps.cpp:23-39`

**Interfaces:**
- 消费框架 RNG：`frandomf_scale(scale)` 返回 [0, scale)；区间 [-100,100] 用 `frandomf_scale(200.0f) - 100.0f`；[-1e6,1e6] 用 `frandom_scale(2.0e6) - 1.0e6`
- 每线程独立流，`-s` 可复现（对齐 PR 其余文件的模式）

- [ ] **Step 1: fma.cpp 输入生成替换**

删除：
```c
    std::mt19937 rng(std::random_device{}());
    std::uniform_real_distribution<float> dist(-100.0f, 100.0f);
```
循环内改为：
```c
        for (int i = 0; i < VECTOR_SIZE; ++i) {
            a[i] = frandomf_scale(200.0f) - 100.0f;
            b[i] = frandomf_scale(200.0f) - 100.0f;
            c[i] = frandomf_scale(200.0f) - 100.0f;
        }
```
（同步删 `<random>` include，`<cmath>` 保留给 fmaf。）

- [ ] **Step 2: fma_patterns_avx512_pd.cpp / _ps.cpp 同构**（double: `frandom_scale(2.0e6) - 1.0e6`；float: `frandomf_scale(2.0e6f) - 1.0e6f`）

- [ ] **Step 3: 验证 + 复现性**

```bash
ninja -C builddir
timeout 60 ./builddir/sdcshield -e fma -e fma_patterns_avx512_pd -e fma_patterns_avx512_ps -t 5000 -n 1 2>&1 | tail -1   # exit: pass（字节级比较仍零假阳性）
timeout 90 ./builddir/sdcshield -e fma -t 30000 -n 1 2>&1 | tail -1                                                        # sustained: exit: pass
# 复现性：同 seed 两次运行，无 stderr flood 后 YAML 应字节级一致（除频率/时间字段）
timeout 30 ./builddir/sdcshield -s LCG:12345 -e fma -t 1000 -n 1 -v > /tmp/fma_s1.yaml 2>&1
timeout 30 ./builddir/sdcshield -s LCG:12345 -e fma -t 1000 -n 1 -v > /tmp/fma_s2.yaml 2>&1
diff <(grep -v "freq\|elapsed\|now:\|runtime" /tmp/fma_s1.yaml) <(grep -v "freq\|elapsed\|now:\|runtime" /tmp/fma_s2.yaml) && echo REPRODUCIBLE
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1   # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/fma/
git commit -s -m "fix(fma): operands from framework RNG (per-thread stream, -s reproducible)

fma/fma_patterns_avx512_{pd,ps} still seeded std::mt19937 from
std::random_device for operand generation — inconsistent with the PR's
framework-RNG pattern (random64/memset_random/frandomf_scale) everywhere
else. The framework redirects random_device::_M_getval to random32(), so
the seed was indirectly framework-sourced, but the mt19937 stream itself
was outside -s reproducibility control. Switch to frandomf_scale/
frandom_scale interval mapping; byte-exact comparison re-verified with
zero false positives over sustained runs.

Verified: fma family -t 5000 -n 1 and -t 30000 sustained exit:pass;
same-seed runs YAML-identical (modulo frequency/time telemetry); zstd19
regression pass; x86-64: ARM paths inside #ifdef __aarch64__.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 6 (F4′): 文档同步（文件头 @parblock 与行为一致）

**Files:**
- Modify: `tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp`（头注释，若 Task 1 未覆盖到位）
- Modify: `tests/cpu/arithmetic_arm/fisttp_arm.cpp:6-22`
- Modify: `tests/cpu/openssl/openssl_sha.cpp:7-12`、`openssl_sha3.cpp`、`openssl_sm3sm4.cpp`（描述 staggered golden 后的语义）
- Modify: 全部 46 个 `tests/cpu/ipsec/**/ipsec_*.cpp` 的 @parblock（"pre-computed golden values on every iteration" → 描述 staggered 语义）

**Interfaces:**
- 无代码改动；纯注释/文档。若 Task 1-3 的 commit 已顺带改了部分头注释，此处只补遗漏

- [ ] **Step 1: 逐文件更新 @parblock**

ipsec 统一文案（替换 "It generates an authentication tag, decrypts the ciphertext, and verifies the tag. The ciphertext, tag, and decrypted data are compared against pre-computed golden values on every iteration to detect silent data corruption." 中的 golden 语义段）：

> The plaintext and golden values are re-rolled every iteration from the framework's per-thread RNG: the data verified in iteration N (fresh plaintext, golden ciphertext/MAC computed through the same EVP path) is computed in iteration N−1 and cached, so the golden carries one iteration of temporal separation from the DUT recomputation.

openssl_sha/sha3/sm3sm4 与 fisttp_arm/bigint_mulx_arm 类似（fisttp: "fixed random table generated at init" → "operands re-rolled every iteration from the framework's per-thread RNG into per-thread buffers"; bigint: 已在 Task 1 改）。

- [ ] **Step 2: 批量核查无残留失真**

```bash
grep -rn "pre-calculated golden\|precomputed in init\|pre-computed golden\|fixed random table" tests/cpu/ipsec/ tests/cpu/openssl/ tests/cpu/arithmetic_arm/fisttp_arm.cpp tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp | grep -v "^Binary"
# 期望：空输出（所有过时描述已更新）
```

- [ ] **Step 3: 构建 + 抽测（注释改动不应影响二进制，但按规矩验证）**

```bash
ninja -C builddir
timeout 60 ./builddir/sdcshield -e openssl_sha -e fisttp_arm -e bigint_mulx_arm -t 2000 -n 1 2>&1 | tail -1   # exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                                            # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add -u
git commit -s -m "docs(tests): sync file-header @parblock with randomized/staggered-golden behavior

The PR changed the data lifecycle (per-iteration re-roll, staggered
golden) but left the file headers describing the old precomputed-in-init
golden and fixed-table semantics — 46 ipsec files plus
openssl_{sha,sha3,sm3sm4} and fisttp_arm. Per the repo rule that
doc changes accompany every substantial modification, update all
headers to describe the actual behavior.

Verified: grep shows zero stale descriptions; build + spot runs pass.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 7 (F5′): openssl_sha/sha3 死 golden pool 清除

**Files:**
- Modify: `tests/cpu/openssl/openssl_sha.cpp:28,39-45,92-122`
- Modify: `tests/cpu/openssl/openssl_sha3.cpp:43,63-69,155-181`

**Interfaces:**
- 删除 `SHA_GOLDEN_ELEMS`/`SHA3_GOLDEN_ELEMS`、`golden_elements[]` 数组、init 中的填充循环
- init 保留：mmap arena（框架需要 test->data 非 NULL 吗？**不需要** — 无 cleanup 的测试 test->data 可为 NULL；但保留 arena+skip 检查 OpenSSL 可用性的 init 逻辑。**决定：init 只做 OpenSSL 函数指针探测 + return，test->data 置 NULL**，与 SANDSTONE_SSL_BUILD=0 分支形状对齐）

- [ ] **Step 1: openssl_sha.cpp 清理**

```c
struct sha_test
{
    uint8_t *arena;
    size_t arena_size;
};
```
（删 `sha_elem golden_elements[SHA_GOLDEN_ELEMS];` 和 `#define SHA_GOLDEN_ELEMS`；`PLAINTEXT_SIZE`/`SHA_MAX_OFFSET` 中 SHA_MAX_OFFSET 已无用户 — 一并删。）

init 变为：

```c
static int ssl_sha_init(struct test* test)
{
    if (s_EVP_DigestInit_ex && s_EVP_DigestUpdate && s_EVP_DigestFinal_ex && s_EVP_get_digestbyname) {
        /* The per-iteration work arena is mmap'd per-thread in
         * ssl_sha_run (staggered golden, H7''); init only probes OpenSSL
         * availability. */
        test->data = NULL;
        return EXIT_SUCCESS;
    }
    else {
        log_skip(TestResourceIssueSkipCategory, "OpenSSL library is not available or the current version is not supported");
        return EXIT_SKIP;
    }
}
```

（原 init 的 arena/mmap/golden 填充全删 — run 已经不读它们；656KiB+3072 次 digest 的 init 开销与 mmap 泄漏一起消失。）

- [ ] **Step 2: openssl_sha3.cpp 同构清理**（SHA3_GOLDEN_ELEMS 256 → 删；179KiB+1280 digest 消失）

- [ ] **Step 3: 验证**

```bash
ninja -C builddir
clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/tests/cpu/openssl/openssl_sha.cpp 2>&1 | grep -oP "All checks completed.*"
timeout 60 ./builddir/sdcshield -e openssl_sha -e openssl_sha3 -t 3000 -n 1 2>&1 | tail -1   # exit: pass
timeout 90 ./builddir/sdcshield -e openssl_sha -e openssl_sha3 -t 5000 2>&1 | tail -1          # all-core: exit: pass
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                            # exit: pass
```

- [ ] **Step 4: Commit**

```bash
git add tests/cpu/openssl/openssl_sha.cpp tests/cpu/openssl/openssl_sha3.cpp
git commit -s -m "fix(openssl): drop dead init golden pools (sha/sha3 no longer read them)

The staggered-golden run path computes everything per-thread in the work
arena; the init-built 1024/256-element golden pools (656KiB/179KiB of
mmap plus 3072/1280 EVP digest calls at init, never freed) became dead
weight the moment the run stopped reading golden_elements[]. Remove the
pools and shrink init to an OpenSSL-availability probe; also removes a
pre-existing init-arena munmap leak.

Verified: openssl_{sha,sha3} -t 3000 -n 1 / -t 5000 all-core exit:pass;
zstd19 regression pass; x86-64: arch-neutral.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 8 (F7′): random_access_sweep knob 越界告警

**Files:**
- Modify: `tests/cpu/memory/random_access_sweep.cpp:184-191`

**Interfaces:**
- 消费 `log_skip(TestResourceIssueSkipCategory, ...)` 或 `report_fail_msg`；参照 ipsec datasize knob 的 `report_fail_msg` 先例（fail-loudly on invalid）

- [ ] **Step 1: init 中 knob 校验**

```c
    int64_t size_knob = get_testspecific_knob_value_int(test, "size_mb", 0);
    size_t requested = (size_knob > 0) ? (size_t)size_knob * (1u << 20)
                                       : default_region_bytes();
    if (size_knob > 0) {
        if ((size_t)size_knob * (1u << 20) > DEFAULT_SIZE_CAP) {
            report_fail_msg("random_access_sweep: size_mb=%ld exceeds the %zu MiB cap",
                            (long)size_knob, DEFAULT_SIZE_CAP >> 20);
        }
        if ((size_t)size_knob * (1u << 20) < MAX_BLOCK * 2) {
            report_fail_msg("random_access_sweep: size_mb=%ld below the %zu MiB minimum "
                            "(must fit one 2 MiB block + slack)",
                            (long)size_knob, (MAX_BLOCK * 2) >> 20);
        }
    }
    data->region_size = requested;
    if (data->region_size > DEFAULT_SIZE_CAP)
        data->region_size = DEFAULT_SIZE_CAP;
    if (data->region_size < MAX_BLOCK * 2)
        data->region_size = MAX_BLOCK * 2;    /* must fit one max block + slack */
```

（`report_fail_msg` 在 init 中可用——ipsec datasize 先例正是 init 中调用。）

- [ ] **Step 2: 验证（越界 fail-loudly + 边界值正常）**

```bash
ninja -C builddir
timeout 30 ./builddir/sdcshield -e random_access_sweep -O random_access_sweep.size_mb=1024 -t 1000 -n 1 2>&1 | grep -E "result|exit" | head -2   # result: fail + knob 消息
timeout 30 ./builddir/sdcshield -e random_access_sweep -O random_access_sweep.size_mb=1 -t 1000 -n 1 2>&1 | grep -E "result|exit" | head -2     # result: fail + minimum 消息
timeout 30 ./builddir/sdcshield -e random_access_sweep -O random_access_sweep.size_mb=8 -t 2000 -n 1 2>&1 | tail -1                                # exit: pass（合法边界）
timeout 30 ./builddir/sdcshield -e random_access_sweep -O random_access_sweep.size_mb=512 -t 2000 -n 1 2>&1 | tail -1                               # exit: pass（cap 边界）
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                                                                                 # exit: pass
```

- [ ] **Step 3: Commit**

```bash
git add tests/cpu/memory/random_access_sweep.cpp
git commit -s -m "fix(random_access_sweep): fail-loudly on out-of-range size_mb knob

size_mb > 512 was silently clamped to the cap and < 4 silently raised to
the 4 MiB floor — knob semantics that lie to the operator. Follow the
ipsec datasize knob precedent (report_fail_msg from init on invalid
values); the clamps remain only for the memory-adaptive default path.

Verified: size_mb=1024 and size_mb=1 both fail with the specific knob
message; 8/512 boundary values pass; zstd19 regression pass; x86-64:
inside the aarch64-only #ifdef.

Signed-off-by: wangxumarshall <wangxumarshall@qq.com>"
```

---

### Task 9: 收尾 — 全量回归 + LSP 全覆盖 + 推送

**Files:**
- 无新改动（验证任务）

- [ ] **Step 1: 全量功能回归**

```bash
ninja -C builddir    # 零新告警
timeout 300 ./builddir/sdcshield -e 'ipsec_*' -t 3000 -n 2 2>&1 | tail -1                                   # exit: pass
timeout 120 ./builddir/sdcshield -e openssl_sha -e openssl_sha3 -e openssl_sm3sm4 -t 3000 -n 1 2>&1 | tail -1 # exit: pass
timeout 60 ./builddir/sdcshield -e bigint_mulx_arm -e fisttp_arm -e adcx -t 3000 -n 1 2>&1 | tail -1          # exit: pass
timeout 60 ./builddir/sdcshield -e fma -e fma_patterns_avx512_pd -e fma_patterns_avx512_ps -t 5000 2>&1 | tail -1 # exit: pass
timeout 60 ./builddir/sdcshield -e memcpy0 -e memcpy_l1d_size -e memcpy_l2_cache_size -e memcpy_l3_cache_size -e random_access_sweep -t 3000 -n 1 2>&1 | tail -1  # exit: pass
timeout 60 ./builddir/sdcshield -e cache_stress_aggressor -e vmx_vmexit_cpuid -t 2000 -n 1 2>&1 | tail -1     # exit: pass（vmx 为 skip）
timeout 30 ./builddir/sdcshield -e zstd19 -t 2000 -n 1 2>&1 | tail -1                                         # exit: pass
```

- [ ] **Step 2: LSP 全覆盖（所有本计划触碰的文件）**

```bash
for f in tests/cpu/arithmetic_arm/bigint_mulx_arm.cpp tests/cpu/arithmetic_arm/fisttp_arm.cpp \
         tests/cpu/openssl/openssl_sha.cpp tests/cpu/openssl/openssl_sha3.cpp tests/cpu/openssl/openssl_sm3sm4.cpp \
         tests/cpu/fma/fma.cpp tests/cpu/fma/fma_patterns_avx512_pd.cpp tests/cpu/fma/fma_patterns_avx512_ps.cpp \
         tests/cpu/memory/random_access_sweep.cpp \
         tests/cpu/ipsec/aes192_gcm/ipsec_aes192_gcm_avx.cpp \
         tests/cpu/ipsec/3des_docsis/ipsec_3des_docsis_hmac_sha1_x86_64.cpp \
         tests/cpu/ipsec/aes192_ctr/ipsec_aes192_ctr_xcbc_96_avx.cpp; do
  echo "== $f"; clangd --background-index=false --compile-commands-dir=$PWD/builddir --check=$PWD/$f 2>&1 | grep -oP "All checks completed.*"
done
# 期望全部 0 errors（tweak-only "1 errors" 可忽略，需 grep '^E\[' 确认只含 tweak: 行）
```

- [ ] **Step 3: x86-64 非回归审查（inspection）**

```bash
git diff pr-146-orig..HEAD --stat    # pr-146-orig 是本计划开始前的分支 tag/commit
# 逐文件确认：openssl/ipsec/fma/memcpy 系列改动为 arch-neutral 或 #ifdef __aarch64__ 门内；
# random_access_sweep 的 knob 校验在 aarch64 #ifdef 内。
```

- [ ] **Step 4: 推送**

```bash
git push origin pr-146:randomization-hardening-review-fixes   # 或直接推 pr-146 的 HEAD 到其原分支名
# 实际操作时先 git branch --show-current 确认；PR #146 的 head 分支名以 gh pr view 为准
# （本机 gh 未认证 — 推送后提示用户在 GitHub 上确认 PR 更新）
```

- [ ] **Step 5: 更新 findings 文档**

在 `docs/superpowers/plans/2026-09-20-pr146-adversarial-review-findings.md` 末尾追加"修复落地记录"小节：每个 F# 对应的 commit hash + 验证输出摘录。

---

## 合并注意事项（非本计划任务，给 maintainer 的信息）

1. **18de2ff 会变空**：main 的 bea8d2b（2026-09-20 已合入）删除了同一个 `tests/cpu/arm64/neon_rot_ldr_at_top_rowmajor.cpp`。PR rebase 到最新 main 时 18de2ff 的文件删除与 bea8d2b 相同 → 该 commit 变空，应 drop（其 meson.build 注释差异保留）。已在本计划的 Task 9 Step 3 之外，不影响本计划执行（我们在 PR 分支顶端追加，不动历史）。
2. **修复 commit 的粒度**：F1′ 拆成 Task 2（openssl 3 文件）+ Task 3（ipsec 46 文件）两个 commit——同构变换但影响面差异大（46 文件的机械变换值得独立 review gate）。

## Self-Review 记录

- **覆盖核对**：F1→Task 2+3；F2→Task 1；F3→Task 4；F4→Task 6；F5→Task 7；F6→Task 5；F7→Task 8；收尾→Task 9。审查文档中"无问题确认"部分无需任务。
- **占位符扫描**：无 TBD/TODO；所有代码块完整可编译（schoolbook 已在 /tmp 验证 10000 组）。
- **类型一致性**：`schoolbook_mul_golden(const uint64_t*, const uint64_t*, uint64_t*)` 在 Task 1 定义且仅 Task 1 使用；staggered 模式三处（sha/sha3/sm3sm4）各自独立实现，无跨任务类型依赖；Task 6 依赖 Task 1-3 完成后的最终代码形态（文案与 Task 1-3 中给出的实现一致）。
- **执行顺序**：Task 1（最严重）→ Task 2 → Task 3 → Task 4/5（fma 两项，可并行）→ Task 6（文档，须在 1-3 后）→ Task 7/8 → Task 9。

/* memcpy2_neon — 大窗口非对齐 NEON 拷贝、紧窗口 STLF 与翻译回收压力
 *
 * 目标（arm64 服务器芯片 SDC 复现，怀疑面 LSU + MMU）：
 *   - LSU：非对齐 vld1q/vst1q 16B 流、store-to-load forwarding（含窄写宽读
 *     partial-STLF）、存储消歧、页尾 16B 撕裂访问（split-page load/store）。
 *   - MMU：页尾锚定段（锚定段首个 16B 向量必跨页）、随机页选择 + 小概率
 *     MiB 级大段（> L2）、mprotect 翻转 + 立即逐页触读（TLBI 后 TLB 重填 /
 *     PTW 重播）、周期性 MADV_DONTNEED（PTE 拆除 + 重填，可能换物理页）。
 *
 *  Oracle 使用 mapping-relative logical VA：相同 seed/线程数/模式在 ASLR 改变
 *  后仍生成相同逻辑图样，实际 VA 仅用于失败取证。图样 =
 *      pattern(lva, epoch, cpu) = blob[lva & 1MiB-1] ^ blk_key(lva>>20)[lva & 7]
 *  blob 为 init 一次性生成的 1 MiB 伪随机基表，blk_key 由双射 mix64 从
 *  (epoch, cpu, lva>>20) 派生：不同 epoch 必不同，且同 epoch 不同 1 MiB 块
 *  必不同（mix64 双射）——VA 高位（bit>=20）翻转映射到相邻 1 MiB 块时必
 *  失配。这是 2026-11-27 修复的混叠盲区：此前密钥只含 (epoch, cpu)，VA
 *  相差 1 MiB 整数倍图样相同，MMU/TLB 高位错映射完全不可检。跨 epoch
 *  跨块的密钥碰撞概率 2^-64/事件对，可接受。填充/校验按 1 MiB 子块外层
 *  循环，块入口重算 kv 一次，块内 16B 仍一条 veor；2026-11-27 perf 实测
 *  旧版 SplitMix 版 mem_access/instructions 仅 0.116，约 88% 指令耗在
 *  图样 ALU，12h 标称运行里真正 LSU/MMU 占比可忽略。
 *
 *  校验分阶段：源填充 → 源校验 → 拷贝 → 目的校验 → 源复核。目的失配时同时
 *  回读同一偏移的源字节 s[off]（STLF 重叠区源已被拷贝合法覆盖，显式标注
 *  不可观测），用于区分 fill(ALU) 与 copy(LSU/MMU)。重叠 dst>src 用倒序
 *  memmove 语义。
 *
 *  观测通路不对称化（2026-11-27 P1b）：校验的数据读走 GPR ldp（2x8B），
 *  期望仍由 NEON veorq 计算后经 mov x,v.d[] 搬 GPR 做 eor/cmp——写侧
 *  fill/copy 走 NEON 128b 数据通路，观测侧走 GPR 64b 通路，打破「NEON
 *  lane k 写坏、校验再经同坏 lane k 读」的自洽掩蔽。STLF 回读本来就是
 *  混合位宽 GPR，无需处理。
 *
 *  取证口径：失败日志先快照 16B 现场与 128B 上下文，再做缓存维护；triage
 *  优先 DC IVAC（不回写），避免 DC CIVAC 把脏 L1D 错值写回后被误判为
 *  store/DRAM 持久错。PFN 按运行时真实基页查 pagemap，无权限输出
 *  unavailable。重读/维护结果只作观测，不直接命名 LSU/TLB/cache/DRAM 根因。
 *  MADV_DONTNEED/mprotect 的系统调用错误属于基础设施错误，不是 SDC。
 *
 *  SDCSHIELD_MEMCPY2_MODE=all|copy|stlf|regen|protect（默认 all）用于单变量
 *  诊断。SDCSHIELD_MEMCPY2_INJECT=source|destination|guard 是默认关闭的检测器
 *  自检；只在 framework cpu 0 的首次主拷贝注入，日志标 SELFTEST-INJECTED。
 *  SDCSHIELD_MEMCPY2_PAGE_SIZE=<4096|32768|65536> 覆盖几何页大小（默认
 *  sysconf），仅用于在 4K 主机上模拟 64K 页的 VA 跨页几何；pagemap 始终按
 *  真实页大小查询。
 *
 *  DC ZVA/IVAC/CIVAC 在 init 用 SIGILL 探测；不可用只禁用对应子步。覆盖率
 *  必须用迭代、字节、跨页/锚定计数与系统调用计数描述，不能由 1 GiB VA 或
 *  墙钟时长推断。跨 NUMA 共享 / 一致性 Snoop 压力不属本私有映射用例，见
 *  docs/superpowers/plans/2026-11-27-memcpy2-neon-stable-repro.md §2 P3。
 */
#include <sandstone.h>
#include "arm_add_neon_copy.h"
#include <algorithm>
#include <arm_neon.h>
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <optional>
#include <sched.h>
#include <string>
#include <sys/mman.h>
#include <unistd.h>

using arm_add_neon::neon_copy;

/* ---- 布局参数（每 worker 私有映射，2 x REGION_HALF） ---- */
static constexpr size_t REGION_HALF = 512UL * 1024 * 1024;   /* src / dst 各半区 */
static constexpr size_t SEG_SPAN    = 480UL * 1024 * 1024;   /* 段偏移散布范围 */
static constexpr size_t SCRATCH_OFF = 504UL * 1024 * 1024;   /* dst 半区尾部 scratch */
static constexpr size_t SCRATCH_LEN = 8UL  * 1024 * 1024;
static constexpr size_t MIN_SEG     = 4UL  * 1024;           /* 常规段 4K..64K */
static constexpr size_t MIN_HUGE    = 1UL  * 1024 * 1024;    /* 大段 1..4 MiB：> L2 */
static constexpr size_t MAX_HUGE    = 4UL  * 1024 * 1024;
static constexpr unsigned MAX_SEG_N = 16;                    /* 每迭代段数上限 */

static constexpr unsigned MAX_MISALIGN = 16;
static constexpr size_t GUARD = 64;

/* 周期性动作按外层迭代计（不按 epoch：epoch 每段涨 2+，语义混淆且成本失控） */
static constexpr uint64_t PROT_EVERY  = 16;   /* 每 16 次迭代翻转一次 mprotect */
static constexpr uint64_t REGEN_EVERY = 4;    /* 每 4 次迭代全区 MADV_DONTNEED */

static constexpr uint64_t GOLDEN  = 0x9e3779b97f4a7c15ULL;
static constexpr uint64_t GOLDEN2 = 0xc2b2ae3d27d4eb4fULL;

static size_t g_page_size = 4096;   /* 真实基页：pagemap 查询 / mmap 对齐 */
static size_t g_geom_page = 4096;   /* 几何页：跨页锚定（可模拟 64K） */
static size_t g_geom_mask = 4095;
static bool   g_have_dczva;      /* DC ZVA @ EL0 (SCTLR_EL1.DZE) */
static bool   g_have_civac;      /* DC CIVAC @ EL0 (SCTLR_EL1.UCI) */
static bool   g_have_ivac;       /* DC IVAC @ EL0 (UCI；不回写，triage 优先) */

enum run_mode { MODE_ALL, MODE_COPY, MODE_STLF, MODE_REGEN, MODE_PROTECT };
enum inject_mode { INJECT_NONE, INJECT_SOURCE, INJECT_DESTINATION, INJECT_GUARD };
static run_mode g_mode = MODE_ALL;
static inject_mode g_inject = INJECT_NONE;
static thread_local uint64_t g_pattern_base;

static inline uint64_t logical_va(const void *p)
{
    return (uint64_t)(uintptr_t)p - g_pattern_base;
}

/* ---- 图样 oracle ----
 * 预生成基表 + (epoch, cpu, 1MiB 块号) 密钥 XOR。16B 对齐分块保证块内
 * (VA & 7) 相位恒为 0..7,0..7；标量头/尾按字节取同一函数。密钥按块号
 * 折入（blk_key）：同 epoch 不同 1 MiB 块必得不同密钥，1 MiB 图样混叠
 * （高位 VA 错映射）必失配；fill/verify 按 1 MiB 子块外层循环，每块入口
 * 重算 kv 一次，块内每 16B 仍只 1 条 veorq。 */
static constexpr unsigned BLOB_BITS = 20;
static constexpr size_t BLOB_LEN = size_t{1} << BLOB_BITS;
static constexpr size_t BLOB_MASK = BLOB_LEN - 1;
static constexpr size_t BLOB_PAD = 32;   /* 32B 展开时允许越过表尾读镜像 */
alignas(64) static uint8_t g_blob[BLOB_LEN + BLOB_PAD];

static inline uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static void blob_init(void)
{
    for (size_t i = 0; i < BLOB_LEN; i += 8) {
        const uint64_t w = mix64(i * GOLDEN + GOLDEN2);
        std::memcpy(g_blob + i, &w, sizeof w);
    }
    std::memcpy(g_blob + BLOB_LEN, g_blob, BLOB_PAD);
}

static inline uint64_t epoch_key(uint64_t epoch, int cpu)
{
    /* mix64 双射 + GOLDEN 为奇数 => 不同 epoch 必得不同密钥 */
    return mix64(epoch * GOLDEN ^ ((uint64_t)(cpu + 1) * GOLDEN2));
}

/* 把 1 MiB 块号折进密钥：VA 高位（bit>=20）翻转 → 块号变 → 密钥变。
 * mix64 双射 + GOLDEN2 为奇数 => 同 key 下不同块必得不同 blk_key，
 * 1 MiB 图样混叠（MMU/TLB 高位错映射到相邻 1 MiB 块）因此必失配检出。
 * 跨 epoch 跨块的密钥碰撞概率 2^-64/事件对（见头注释）。 */
static inline uint64_t blk_key(uint64_t key, uint64_t lva)
{
    return key ^ mix64((lva >> BLOB_BITS) * GOLDEN2);
}

static inline uint8_t pat_byte_k(uint64_t lva, uint64_t key)
{
    return (uint8_t)(g_blob[lva & BLOB_MASK]
                     ^ (uint8_t)(key >> ((unsigned)(lva & 7) * 8)));
}

static inline uint8_t pat_byte(uint64_t va, uint64_t epoch, int cpu)
{
    return pat_byte_k(va, blk_key(epoch_key(epoch, cpu), va));
}

static inline uint8x16_t key_vec(uint64_t key)
{
    uint8_t kb[16];
    for (unsigned j = 0; j < 16; ++j)
        kb[j] = (uint8_t)(key >> ((j & 7) * 8));
    return vld1q_u8(kb);
}

static void fill_pat(uint8_t *mem, size_t n, uint64_t epoch, int cpu)
{
    const uint64_t va = logical_va(mem);
    const uint64_t key = epoch_key(epoch, cpu);
    size_t i = 0;
    while (i < n) {
        /* 外层按 1 MiB 子块推进：块边界与 16B/32B 网格、blob 回卷点
         * 重合（1 MiB 是 16 的倍数），kv 每块入口只算一次，块内每
         * 16B 仍只 1 条 veorq。标量路径逐字节折 blk_key（跨块窗口）。 */
        const uint64_t lva = va + i;
        const size_t blk_rem = BLOB_LEN - (size_t)(lva & BLOB_MASK);
        const size_t rem = std::min(n - i, blk_rem);
        const uint8x16_t kv = key_vec(blk_key(key, lva));
        size_t j = 0;
        while (j < rem && ((lva + j) & 15u)) {
            mem[i + j] = pat_byte_k(lva + j, blk_key(key, lva + j));
            ++j;
        }
        for (; j + 32 <= rem; j += 32) {
            const size_t o = (size_t)((lva + j) & BLOB_MASK);
            const uint8x16_t b0 = veorq_u8(vld1q_u8(g_blob + o), kv);
            const uint8x16_t b1 = veorq_u8(vld1q_u8(g_blob + o + 16), kv);
            vst1q_u8(mem + i + j, b0);
            vst1q_u8(mem + i + j + 16, b1);
        }
        for (; j + 16 <= rem; j += 16) {
            const uint8x16_t b = vld1q_u8(g_blob + ((lva + j) & BLOB_MASK));
            vst1q_u8(mem + i + j, veorq_u8(b, kv));
        }
        for (; j < rem; ++j)
            mem[i + j] = pat_byte_k(lva + j, blk_key(key, lva + j));
        i += rem;
    }
}

/* 返回首个失配偏移并保存首次 got/want；全对返回 n。
 * exp_va 是 logical VA；重叠拷贝时按源 logical VA 取期望。 */
static size_t verify_pat(const uint8_t *mem, uint64_t exp_va, size_t n,
                         uint64_t epoch, int cpu, uint8_t *got_out,
                         uint8_t *want_out)
{
    const uint64_t key = epoch_key(epoch, cpu);
    size_t i = 0;
    while (i < n) {
        /* 与 fill_pat 对称的 1 MiB 子块外层循环；失配精化窗口可能跨块，
         * 逐字节折 blk_key（冷路径）。 */
        const uint64_t lva = exp_va + i;
        const size_t blk_rem = BLOB_LEN - (size_t)(lva & BLOB_MASK);
        const size_t rem = std::min(n - i, blk_rem);
        const uint8x16_t kv = key_vec(blk_key(key, lva));
        size_t j = 0;
        while (j < rem && ((lva + j) & 15u)) {
            const uint8_t want = pat_byte_k(lva + j, blk_key(key, lva + j));
            if (mem[i + j] != want) {
                *got_out = mem[i + j]; *want_out = want;
                return i + j;
            }
            ++j;
        }
        for (; j + 32 <= rem; j += 32) {
            const size_t o = (size_t)((lva + j) & BLOB_MASK);
            /* 数据读走 GPR ldp（2x8B）：打破 fill(vst1q) 与校验(vld1q) 同走
             * NEON 128b lane 的自洽掩蔽（头注释 P1b）。期望仍 NEON veorq
             * 算，vgetq_lane_u64 生成 mov x,v.d[] 搬 GPR 后 eor/cmp。 */
            uint64_t g0 = 0, g1 = 0, g2 = 0, g3 = 0;
            asm volatile("ldp %0, %1, [%2]" : "=r"(g0), "=r"(g1)
                         : "r"(mem + i + j) : "memory");
            asm volatile("ldp %0, %1, [%2]" : "=r"(g2), "=r"(g3)
                         : "r"(mem + i + j + 16) : "memory");
            const uint8x16_t w0 = veorq_u8(vld1q_u8(g_blob + o), kv);
            const uint8x16_t w1 = veorq_u8(vld1q_u8(g_blob + o + 16), kv);
            const uint64x2_t w0d = vreinterpretq_u64_u8(w0);
            const uint64x2_t w1d = vreinterpretq_u64_u8(w1);
            const uint64_t x = (g0 ^ vgetq_lane_u64(w0d, 0))
                             | (g1 ^ vgetq_lane_u64(w0d, 1))
                             | (g2 ^ vgetq_lane_u64(w1d, 0))
                             | (g3 ^ vgetq_lane_u64(w1d, 1));
            if (x != 0) {
                for (size_t k = 0; k < 32; ++k) {
                    const uint8_t wb = pat_byte_k(lva + j + k,
                                                  blk_key(key, lva + j + k));
                    if (mem[i + j + k] != wb) {
                        *got_out = mem[i + j + k]; *want_out = wb;
                        return i + j + k;
                    }
                }
                return i + j;
            }
        }
        for (; j + 16 <= rem; j += 16) {
            uint64_t g0 = 0, g1 = 0;
            asm volatile("ldp %0, %1, [%2]" : "=r"(g0), "=r"(g1)
                         : "r"(mem + i + j) : "memory");
            const uint8x16_t want =
                veorq_u8(vld1q_u8(g_blob + ((lva + j) & BLOB_MASK)), kv);
            const uint64x2_t wd = vreinterpretq_u64_u8(want);
            const uint64_t x = (g0 ^ vgetq_lane_u64(wd, 0))
                             | (g1 ^ vgetq_lane_u64(wd, 1));
            if (x != 0) {
                for (size_t k = 0; k < 16; ++k) {
                    const uint8_t wb = pat_byte_k(lva + j + k,
                                                  blk_key(key, lva + j + k));
                    if (mem[i + j + k] != wb) {
                        *got_out = mem[i + j + k]; *want_out = wb;
                        return i + j + k;
                    }
                }
                return i + j;
            }
        }
        for (; j < rem; ++j) {
            const uint8_t want = pat_byte_k(lva + j, blk_key(key, lva + j));
            if (mem[i + j] != want) {
                *got_out = mem[i + j]; *want_out = want;
                return i + j;
            }
        }
        i += rem;
    }
    return n;
}

static bool poison_intact(const uint8_t *p, size_t n, uint8_t v)
{
    const uint64_t w = (uint64_t)v * 0x0101010101010101ULL;
    size_t i = 0;
    while (i < n && (((uintptr_t)(p + i)) & 7u)) {
        if (p[i] != v)
            return false;
        ++i;
    }
    for (; i + 8 <= n; i += 8) {
        uint64_t g;
        std::memcpy(&g, p + i, sizeof g);
        if (g != w)
            return false;
    }
    for (; i < n; ++i)
        if (p[i] != v)
            return false;
    return true;
}

/* 倒序 NEON copy：高地址 -> 低地址（dst > src 重叠时即 memmove 语义）。
 * 前向逐块在 delta < 16 时会自我涂抹，校验必然失配。 */
static inline void neon_copy_rev(void *dst, const void *src, size_t n)
{
    uint8_t *d = static_cast<uint8_t *>(dst);
    const uint8_t *s = static_cast<const uint8_t *>(src);
    const size_t tail = n % 16;
    const size_t v = n / 16;
    for (size_t k = v; k-- > 0; )
        vst1q_u8(d + tail + k * 16, vld1q_u8(s + tail + k * 16));
    for (size_t i = tail; i-- > 0; )
        d[i] = s[i];
}

struct worker_ctx {
    uint8_t *src_base;
    uint8_t *dst_base;
    uint8_t *scratch;
    void    *raw;          /* mmap 原始基址（对齐前） */
    size_t   raw_len;
    uint8_t *region;       /* 页几何对齐后的 2 x REGION_HALF 映射 */
    size_t region_len;
    int cpu;
    uint64_t epoch;
    uint64_t iter;
    size_t segment;
    bool injected;
    uint64_t segments;
    uint64_t copied_bytes;
    uint64_t cross_page_segments;
    uint64_t anchored_segments;
    uint64_t regen_count;
    uint64_t protect_count;
};

static const char *mode_name(run_mode mode)
{
    switch (mode) {
        case MODE_COPY: return "copy";
        case MODE_STLF: return "stlf";
        case MODE_REGEN: return "regen";
        case MODE_PROTECT: return "protect";
        default: return "all";
    }
}

static bool maybe_inject(worker_ctx &c, inject_mode kind, uint8_t *addr)
{
    if (g_inject != kind || c.cpu != 0 || c.iter != 1 || c.segment != 0 || c.injected)
        return false;
    *addr ^= 1;
    c.injected = true;
    return true;
}

static std::optional<uint64_t> va_to_pfn(uint64_t va)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0)
        return std::nullopt;
    uint64_t entry = 0;
    ssize_t r = pread(fd, &entry, sizeof(entry),
                      (off_t)((va / g_page_size) * sizeof(uint64_t)));
    close(fd);
    if (r != (ssize_t)sizeof(entry) || !(entry & (1ULL << 63)))
        return std::nullopt;
    uint64_t pfn = entry & ((1ULL << 55) - 1);
    if (!pfn)                       /* Linux 对无 CAP_SYS_ADMIN 的读取者清零 PFN */
        return std::nullopt;
    return pfn;
}

struct triage_observation {
    uint8_t reread[4];
    int after_inval;
    const char *method;
};

/* DC IVAC 不回写：若故障只在脏 L1D 行内，IVAC 后重读回正确值；
 * DC CIVAC 会先把脏行写回，无法区分 L1D SRAM 翻转与 store 通路错误，
 * 因此仅在 IVAC 不可用时降级使用，并在日志里写明归因边界。 */
static triage_observation observe_byte(const uint8_t *addr)
{
    triage_observation obs {};
    for (uint8_t &v : obs.reread)
        v = addr[0];
    obs.after_inval = -1;
    obs.method = "none";
    if (g_have_ivac) {
        asm volatile("dc ivac, %0" :: "r"(addr) : "memory");
        asm volatile("dsb sy" ::: "memory");
        obs.after_inval = addr[0];
        obs.method = "DC-IVAC(no-writeback)";
    } else if (g_have_civac) {
        asm volatile("dc civac, %0" :: "r"(addr) : "memory");
        asm volatile("dsb sy" ::: "memory");
        obs.after_inval = addr[0];
        obs.method = "DC-CIVAC(writes-back-dirty-line:ambiguous)";
    }
    return obs;
}

static bool check_data(worker_ctx &c, uint64_t epoch, const uint8_t *mem,
                       uint64_t exp_va, size_t len, unsigned s_mis,
                       unsigned d_mis, const char *stage,
                       const uint8_t *src_obs = nullptr);

/* src_obs: 同一偏移的源字节（非重叠拷贝时有效）；STLF 重叠区源已被拷贝
 * 合法覆盖，调用方必须传 nullptr，不得把当前值当作"源端损坏"证据。 */
[[noreturn]] static void report_mismatch(worker_ctx &c, uint64_t epoch,
                            const uint8_t *mem, size_t off, uint64_t exp_va,
                            size_t len, unsigned s_mis, unsigned d_mis,
                            const char *stage, const uint8_t *src_obs,
                            uint8_t got, uint8_t want)
{
    const uint8_t *addr = mem + off;
    const size_t addr_off = (size_t)(addr - c.region);
    const size_t first_n = std::min<size_t>(16, c.region_len - addr_off);
    uint8_t initial[16] = {};
    std::memcpy(initial, addr, first_n);

    /* 先快照上下文：后面的 IVAC 会丢弃脏行，现场不能等到维护后再读。 */
    const size_t ctx_start = addr_off > 64 ? addr_off - 64 : 0;
    const size_t ctx_end = std::min(c.region_len, addr_off + 64);
    const size_t ctx_len = ctx_end - ctx_start;
    uint8_t context[128] = {};
    std::memcpy(context, c.region + ctx_start, ctx_len);

    const triage_observation obs = observe_byte(addr);
    char pfn_text[32];
    if (std::optional<uint64_t> pfn = va_to_pfn((uint64_t)(uintptr_t)addr))
        std::snprintf(pfn_text, sizeof pfn_text, "%llu", (unsigned long long)*pfn);
    else
        std::snprintf(pfn_text, sizeof pfn_text, "unavailable");

    auto hex_bytes = [](const uint8_t *p, size_t n) {
        std::string out;
        char byte[4];
        for (size_t i = 0; i < n; ++i) {
            std::snprintf(byte, sizeof byte, "%02x", p[i]);
            if (i) out += ' ';
            out += byte;
        }
        return out;
    };

    /* report_fail_msg 是 noreturn：所有证据必须合并到第一次调用。 */
    std::string evidence;
    char line[2048];
    const unsigned xorv = got ^ want;
    std::snprintf(line, sizeof line,
                  "memcpy2_neon: stage=%s selftest=%s mode=%s iter=%llu segment=%zu "
                  "epoch=%llu off=%zu/%zu got=%02x want=%02x xor=%02x first_bit=%d "
                  "lane=%zu s_mis=%u d_mis=%u cfg_cpu=%d os_cpu=%d sched_cpu=%d "
                  "srcVA=%p dstVA=%p PFN=%s",
                  stage, c.injected ? "SELFTEST-INJECTED" : "no", mode_name(g_mode),
                  (unsigned long long)c.iter, c.segment, (unsigned long long)epoch,
                  off, len, got, want, xorv, __builtin_ctz(xorv), off & 15,
                  s_mis, d_mis, c.cpu, device_info[c.cpu].cpu_number,
                  sched_getcpu(),
                  (const void *)(uintptr_t)(g_pattern_base + exp_va + off),
                  (const void *)addr, pfn_text);
    evidence = line;
    std::snprintf(line, sizeof line,
                  "\n  observation: reread=%02x,%02x,%02x,%02x after=%02x via=%s "
                  "(observations only; root cause undetermined)",
                  obs.reread[0], obs.reread[1], obs.reread[2], obs.reread[3],
                  obs.after_inval < 0 ? 0 : (unsigned)obs.after_inval, obs.method);
    evidence += line;
    if (src_obs) {
        const uint8_t sb = src_obs[off];
        std::snprintf(line, sizeof line,
                      "\n  source: s[off]=%02x src_xor(want)=%02x => %s",
                      sb, sb ^ want,
                      sb == want
                          ? "source still equals expected; copy/destination path suspect"
                          : sb == got
                              ? "source equals destination; source-path corruption suspect"
                              : "source differs from both; multi-site or unstable");
    } else {
        std::snprintf(line, sizeof line,
                      "\n  source: not observable (overlapping STLF copy legally "
                      "overwrote the source bytes)");
    }
    evidence += line;
    evidence += "\n  initial[" + std::to_string(first_n) + "]: " +
                hex_bytes(initial, first_n);
    for (size_t i = 0; i < ctx_len; i += 16) {
        const size_t n = std::min<size_t>(16, ctx_len - i);
        std::snprintf(line, sizeof line, "\n  initial_ctx[%+lld]: ",
                      (long long)(ctx_start + i) - (long long)addr_off);
        evidence += line;
        evidence += hex_bytes(context + i, n);
    }
    report_fail_msg("%s", evidence.c_str());
}

static bool check_data(worker_ctx &c, uint64_t epoch, const uint8_t *mem,
                       uint64_t exp_va, size_t len, unsigned s_mis,
                       unsigned d_mis, const char *stage, const uint8_t *src_obs)
{
    uint8_t got = 0, want = 0;
    const size_t bad = verify_pat(mem, exp_va, len, epoch, c.cpu, &got, &want);
    if (bad == len)
        return true;
    report_mismatch(c, epoch, mem, bad, exp_va, len, s_mis, d_mis, stage,
                    src_obs, got, want);
}

static void note_copy(worker_ctx &c, const uint8_t *src, const uint8_t *dst,
                      size_t len, bool anchored)
{
    ++c.segments;
    c.copied_bytes += len;
    if (anchored)
        ++c.anchored_segments;
    const bool src_cross = ((uintptr_t)src & g_geom_mask) + len > g_geom_page;
    const bool dst_cross = ((uintptr_t)dst & g_geom_mask) + len > g_geom_page;
    if (src_cross || dst_cross)
        ++c.cross_page_segments;
}

/* 数据与实际拷贝长度两侧的 guard 分开报告；guard 失配本身不证明 OOB store。 */
static bool check_guard(worker_ctx &c, uint64_t epoch, const uint8_t *p,
                        size_t n, uint8_t poison, const char *stage, size_t len)
{
    if (!n || poison_intact(p, n, poison))
        return true;
    size_t bad = 0;
    while (bad < n && p[bad] == poison)
        ++bad;
    report_fail_msg("memcpy2_neon: stage=%s selftest=%s mode=%s iter=%llu segment=%zu "
                    "epoch=%llu guard_off=%zu/%zu guard_len=%zu got=%02x want=%02x VA=%p",
                    stage, c.injected ? "SELFTEST-INJECTED" : "no", mode_name(g_mode),
                    (unsigned long long)c.iter, c.segment, (unsigned long long)epoch,
                    bad, n, len, p[bad], poison, (const void *)(p + bad));
    return false;
}

static bool check_seg(worker_ctx &c, uint64_t epoch, const uint8_t *mem,
                      uint64_t exp_va, size_t len, unsigned s_mis,
                      unsigned d_mis, const char *stage, uint8_t poison,
                      const uint8_t *lo, size_t lo_n,
                      const uint8_t *hi, size_t hi_n, const uint8_t *src_obs)
{
    if (!check_data(c, epoch, mem, exp_va, len, s_mis, d_mis, stage, src_obs))
        return false;
    if (!check_guard(c, epoch, lo, lo_n, poison, "lo-guard", len))
        return false;
    return check_guard(c, epoch, hi, hi_n, poison, "hi-guard", len);
}

static inline size_t rand_span_off(void)
{
    /* 下限 g_geom_page：anchor=false && mis=0 时 d=dst_base+off，此前 off
     * 可为 0，memset(d-GUARD) 越出 dst 半区尾部写 src 半区末 64B（仅靠
     * 480M/512M 布局空隙侥幸不损数据）。下限保证 d-GUARD 恒在 dst 半区。 */
    const size_t span = SEG_SPAN - MAX_HUGE - g_geom_page;
    return g_geom_page + (size_t)((random64() % span) & ~g_geom_mask);
}

static inline size_t pick_seg_len(void)
{
    if ((random64() % 16) == 0) {  /* 1/16：> L2 的大段，压 MMU/page-walk */
        /* 步长跟随几何页（合并自并发会话）：64K 几何模拟下大段保持 64K
         * 对齐，避免半页长度把"是否跨页"变成随机噪声。 */
        const size_t steps = (MAX_HUGE - MIN_HUGE) / g_geom_page;
        return MIN_HUGE + (size_t)(random64() % steps) * g_geom_page;
    }
    return MIN_SEG << (unsigned)(random64() % 5);   /* 4K..64K，2 的幂 */
}

/* 段放置：一半概率把地址锚在页尾前 mis 字节，使首个 16B 向量跨页撕裂。
 * 非锚定段保留 0..15 的普通错位。base 必须按几何页对齐（run 里保证）。 */
struct placement {
    size_t off;
    unsigned mis;
    bool anchor;
};

static placement pick_placement(void)
{
    placement p;
    p.off = rand_span_off();
    p.anchor = (random64() & 1) != 0;
    p.mis = p.anchor ? 1 + (unsigned)(random64() % 15)
                     : (unsigned)(random64() % MAX_MISALIGN);
    return p;
}

static inline uint8_t *place_at(uint8_t *base, const placement &p)
{
    return base + p.off + (p.anchor ? (g_geom_page - p.mis) : p.mis);
}

/* partial-STLF：对同一 16B 区域做 2..4 条错位窄写（strb/strh/str，写入新
 * epoch 值），随后不等任何校验立即 ldp 宽读，专打 store-buffer 合并/转发
 * 掩码。读回按"被写位置=新 epoch、其余=拷贝 epoch"逐字节核对。本步会改变
 * 该 16B 区域内容，故必须在全段校验之后调用。 */
static bool stlf_partial(worker_ctx &c, uint8_t *dst_ov, const uint8_t *src_ov,
                         size_t ov_len, uint64_t copy_epoch)
{
    if (ov_len < 64)
        return true;
    const unsigned delta = (unsigned)(dst_ov - src_ov);

    for (unsigned t = 0; t < 8; ++t) {
        const size_t off = (size_t)(random64() % (ov_len - 32));
        uint8_t *p = dst_ov + off;

        /* 自包含期望图样（合并自并发会话）：每个探针用自己的 16B 初值，
         * 后续探针窗口重叠也不会互相污染，无需依赖拷贝内容的残留状态。 */
        const uint64_t w0 = mix64(copy_epoch * GOLDEN
                                  ^ ((uint64_t)(c.cpu + 1) * GOLDEN2) ^ t);
        const uint64_t w1 = mix64(w0);
        uint8_t init[16], expect[16];
        std::memcpy(init, &w0, 8);
        std::memcpy(init + 8, &w1, 8);
        std::memcpy(expect, init, sizeof init);

        struct narrow_op { unsigned o, width, val; };
        narrow_op ops[4];
        const unsigned nw = 2 + (unsigned)(random64() % 3);   /* 2..4 条窄写 */
        for (unsigned w = 0; w < nw; ++w) {
            const unsigned width = 1u << (unsigned)(random64() % 3);  /* 1/2/4 */
            const unsigned o = (unsigned)(random64() % (16 - width + 1));
            const unsigned mask = width == 4 ? 0xFFFFFFFFu
                                             : (1u << (8 * width)) - 1u;
            const unsigned val = (unsigned)mix64(w0 ^ (GOLDEN2 + w)) & mask;
            for (unsigned j = 0; j < width; ++j)
                expect[o + j] = (uint8_t)(val >> (8 * j));
            ops[w] = { o, width, val };
        }

        /* 宽写旧图样 -> 窄写新值 -> 立即宽读；中间不插校验/函数调用，
         * 让窄写真正落在 store buffer 尚未排空的窗口内。 */
        vst1q_u8(p, vld1q_u8(init));
        for (unsigned w = 0; w < nw; ++w) {
            uint8_t *tp = p + ops[w].o;
            switch (ops[w].width) {
            case 1:
                asm volatile("strb %w0, [%1]" :: "r"(ops[w].val), "r"(tp)
                             : "memory");
                break;
            case 2:
                asm volatile("strh %w0, [%1]" :: "r"(ops[w].val), "r"(tp)
                             : "memory");
                break;
            default:
                asm volatile("str %w0, [%1]" :: "r"(ops[w].val), "r"(tp)
                             : "memory");
                break;
            }
        }
        uint64_t r1 = 0, r2 = 0;
        asm volatile("ldp %0, %1, [%2]" : "=r"(r1), "=r"(r2) : "r"(p)
                     : "memory");
        uint8_t got[16];
        std::memcpy(got, &r1, 8);
        std::memcpy(got + 8, &r2, 8);
        for (unsigned j = 0; j < 16; ++j) {
            if (got[j] != expect[j]) {
                report_mismatch(c, copy_epoch, p, j, logical_va(p),
                                sizeof expect, 0, delta, "stlf-partial", nullptr,
                                got[j], expect[j]);
            }
        }
    }
    return true;
}

/* STLF：dst=src+delta 的倒序 memmove-up；混合位宽回读先于任何全段校验。 */
static bool do_stlf(worker_ctx &c, uint8_t *d, size_t seg_len, size_t len)
{
    const size_t ov_len = len / 2;
    if (ov_len < 64)
        return true;
    const unsigned delta = 1 + (unsigned)(random64() % 15);
    if (seg_len <= ov_len + delta)
        return true;
    uint8_t *src_ov = d + (size_t)(random64() % (seg_len - ov_len - delta));
    uint8_t *dst_ov = src_ov + delta;
    const uint64_t copy_epoch = c.epoch;
    const uint64_t src_logical = logical_va(src_ov);
    const uint8_t poison = (uint8_t)~(0xC3 ^ (unsigned)(copy_epoch & 0xFF));

    fill_pat(src_ov, ov_len, copy_epoch, c.cpu);
    if (!check_data(c, copy_epoch, src_ov, src_logical, ov_len, 0, delta,
                    "stlf-source-verify"))
        return false;
    std::memset(dst_ov + ov_len, poison, GUARD);
    neon_copy_rev(dst_ov, src_ov, ov_len);
    note_copy(c, src_ov, dst_ov, ov_len, false);

    /* 倒序最后写低地址：前 8 次先读最新低地址；后 8 次保留高地址/随机对照。
     * 中间不插全段校验，尽量让回读落在 store queue 尚未排空的窗口内。 */
    for (unsigned t = 0; t < 16; ++t) {
        const size_t span = ov_len - 16;
        size_t off;
        if (t < 8) {
            const size_t recent = std::min<size_t>(span, 63);
            off = (size_t)(random64() % (recent + 1));
        } else if (t < 12) {
            const size_t tail_start = span > 64 ? span - 64 : 0;
            off = tail_start + (size_t)(random64() % (span - tail_start + 1));
        } else {
            off = (size_t)(random64() % (span + 1));
            if (t & 1)
                off &= ~(size_t)7;
        }
        const uint8_t *a = dst_ov + off;
        uint64_t w = 0, p1 = 0, p2 = 0;
        unsigned hw = 0, width = 0;
        uint8_t b = 0, got[16] = {};
        const char *stage = nullptr;
        const unsigned kind = random64() % 4;

        switch (kind) {
        case 0:
            asm volatile("ldrb %w0, [%1]" : "=r"(b) : "r"(a) : "memory");
            got[0] = b; width = 1; stage = "stlf-ldrb";
            break;
        case 1:
            asm volatile("ldrh %w0, [%1]" : "=r"(hw) : "r"(a) : "memory");
            std::memcpy(got, &hw, 2); width = 2; stage = "stlf-ldrh";
            break;
        case 2:
            asm volatile("ldr %0, [%1]" : "=r"(w) : "r"(a) : "memory");
            std::memcpy(got, &w, 8); width = 8; stage = "stlf-ldr";
            break;
        default:
            asm volatile("ldp %0, %1, [%2]" : "=r"(p1), "=r"(p2) : "r"(a)
                         : "memory");
            std::memcpy(got, &p1, 8);
            std::memcpy(got + 8, &p2, 8);
            width = 16; stage = "stlf-ldp";
            break;
        }
        for (unsigned j = 0; j < width; ++j) {
            const uint8_t want = pat_byte(src_logical + off + j, copy_epoch, c.cpu);
            if (got[j] != want) {
                report_mismatch(c, copy_epoch, dst_ov, off + j, src_logical,
                                ov_len, 0, delta, stage, nullptr, got[j], want);
            }
        }
    }

    if (!check_seg(c, copy_epoch, dst_ov, src_logical, ov_len, 0, delta,
                   "stlf-overlap", poison, nullptr, 0, dst_ov + ov_len, GUARD,
                   nullptr))
        return false;
    ++c.epoch;
    return stlf_partial(c, dst_ov, src_ov, ov_len, copy_epoch);
}

/* 一个段：正反向各按五阶段校验；all 模式再叠 STLF。 */
static bool do_segment(worker_ctx &c, size_t seg_len, bool with_stlf)
{
    uint8_t *last_d = nullptr;
    size_t last_len = 0;

    auto one_copy = [&](bool reverse) -> bool {
        const placement ps = pick_placement();
        placement pd = pick_placement();
        if ((random64() % 4) == 0)      /* 1/4：dst 与 src 同页偏移（VA 别名偏置） */
            pd.off = ps.off;
        const size_t len = seg_len / 2 + (size_t)(random64() % (seg_len / 2));
        uint8_t *s = place_at(c.src_base, ps);
        uint8_t *d = place_at(c.dst_base, pd);
        last_d = d;
        last_len = len;
        const uint64_t copy_epoch = c.epoch;
        const uint64_t src_logical = logical_va(s);
        const uint8_t poison = (uint8_t)~((reverse ? 0x5A : 0xA5)
                                         ^ (unsigned)(copy_epoch & 0xFF));
        const char *copy_stage = reverse ? "rev-copy" : "fwd-copy";

        std::memset(d - GUARD, poison, GUARD + seg_len + GUARD);
        fill_pat(s, len, copy_epoch, c.cpu);
        maybe_inject(c, INJECT_SOURCE, s);
        if (!check_data(c, copy_epoch, s, src_logical, len, ps.mis, pd.mis,
                        reverse ? "rev-source-verify" : "fwd-source-verify"))
            return false;

        if (reverse)
            neon_copy_rev(d, s, len);
        else
            neon_copy(d, s, len);
        note_copy(c, s, d, len, ps.anchor || pd.anchor);
        maybe_inject(c, INJECT_DESTINATION, d);
        maybe_inject(c, INJECT_GUARD, d + len);
        if (!check_seg(c, copy_epoch, d, src_logical, len, ps.mis, pd.mis,
                       copy_stage, poison, d - GUARD, GUARD,
                       d + len, (seg_len - len) + GUARD, s))
            return false;
        /* 不再做"拷贝后源复核"：源与目的分属两个半区，拷贝不可能改源；
         * 目的失配时 report_mismatch 已回读 s[off] 完成 fill/copy 归因。
         * 少一遍全段校验，把 LSU 占空比从 ~19% 抬到 ~25%（perf 实测口径）。 */
        ++c.epoch;
        return true;
    };

    if (!one_copy(false) || !one_copy(true))
        return false;
    return with_stlf ? do_stlf(c, last_d, seg_len, last_len) : true;
}

/* scratch 指令多样性：DC ZVA（按 256B 对齐刷 256B + 两侧哨兵不出界）、
 * STNP/LDP 非临时对存储（对齐与非对齐各一，含邻域不被误写）。 */
static bool do_scratch(worker_ctx &c)
{
    /* 页边距按真实页大小留（合并自并发会话）：64K 页目标机上 4096 不够。 */
    const size_t usable = SCRATCH_LEN - g_page_size;

    if (g_have_dczva) {
        /* DC ZVA 的完成依赖 DSB（架构要求），这里的 dsb 不是多余串行化；
         * 保留它，避免把"维护未完成"误报成 zva residue。 */
        /* off 下限 GUARD：z-GUARD 不越出 scratch 基址（布局参数防御，
         * 方案 P2；scratch 前沿本就在段散布范围之外，属双保险）。 */
        size_t off = GUARD + ((size_t)(random64() % (usable - 384 - GUARD))
                              & ~(size_t)255);
        uint8_t *z = c.scratch + off;
        std::memset(z - GUARD, 0xFF, 256 + 2 * GUARD);
        for (unsigned b = 0; b < 4; ++b)
            asm volatile("dc zva, %0" :: "r"(z + b * 64) : "memory");
        asm volatile("dsb sy" ::: "memory");
        for (size_t i = 0; i < 256; ++i)
            if (z[i] != 0) {
                report_fail_msg("memcpy2_neon: DC ZVA residue epoch=%llu "
                                "off=%zu got=%02x", (unsigned long long)c.epoch,
                                i, z[i]);
                return false;
            }
        if (!poison_intact(z - GUARD, GUARD, 0xFF) ||
            !poison_intact(z + 256, GUARD, 0xFF)) {
            report_fail_msg("memcpy2_neon: DC ZVA overran 256B block "
                            "epoch=%llu", (unsigned long long)c.epoch);
            return false;
        }
        ++c.epoch;
    }

    /* STNP 16B 对存储：对齐 / 非对齐各一；前后各 8B 哨兵必须不动。
     * 同线程同地址的 stnp->ldp 由程序序保证，无需 DSB（旧版在这里的 dsb
     * 会排空写合并缓冲，削弱非临时写路径的压力）。 */
    for (unsigned t = 0; t < 2; ++t) {
        /* off 下限 8：np-8 哨兵不越出 scratch 基址（同上，防御性）。 */
        size_t off = 8 + (size_t)(random64() % (usable - 64 - 8));
        if (t == 0)
            off &= ~(size_t)7;            /* 对齐 */
        else if ((off & 7u) == 0)
            off += 1 + (size_t)(random64() % 7);   /* 强制非对齐 */
        uint8_t *np = c.scratch + off;
        const uint64_t v1 = mix64(c.epoch * GOLDEN);
        const uint64_t v2 = mix64(c.epoch * GOLDEN + 1);
        std::memset(np - 8, 0xEE, 16 + 16);
        asm volatile("stnp %0, %1, [%2]" :: "r"(v1), "r"(v2), "r"(np)
                     : "memory");
        uint64_t r1 = 0, r2 = 0;
        asm volatile("ldp %0, %1, [%2]" : "=r"(r1), "=r"(r2) : "r"(np)
                     : "memory");
        if (r1 != v1 || r2 != v2) {
            report_fail_msg("memcpy2_neon: STNP/LDP mismatch epoch=%llu "
                            "align=%zu off=%zu got=%016llx,%016llx "
                            "want=%016llx,%016llx",
                            (unsigned long long)c.epoch, (size_t)(off & 7u), off,
                            (unsigned long long)r1, (unsigned long long)r2,
                            (unsigned long long)v1, (unsigned long long)v2);
            return false;
        }
        if (!poison_intact(np - 8, 8, 0xEE) ||
            !poison_intact(np + 16, 8, 0xEE)) {
            report_fail_msg("memcpy2_neon: STNP clobbered neighbours "
                            "epoch=%llu align=%zu",
                            (unsigned long long)c.epoch, (size_t)(off & 7u));
            return false;
        }
        ++c.epoch;
    }
    return true;
}

/* ---- EL0 能力探测（init 单线程执行；不探测则 SIGILL 会杀死整个测试） ---- */
static sigjmp_buf g_probe_jb;
static volatile sig_atomic_t g_probe_trapped;
alignas(256) static uint8_t g_probe_buf[256];

static void probe_sigill_handler(int)
{
    g_probe_trapped = 1;
    siglongjmp(g_probe_jb, 1);
}

template <typename Fn>
static bool el0_instruction_available(Fn fn)
{
    struct sigaction sa, old;
    std::memset(&sa, 0, sizeof sa);
    std::memset(&old, 0, sizeof old);
    sa.sa_handler = probe_sigill_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_NODEFER;
    if (sigaction(SIGILL, &sa, &old) != 0)
        return false;

    g_probe_trapped = 0;
    bool ok = true;
    if (sigsetjmp(g_probe_jb, 1) == 0)
        fn();
    else
        ok = false;

    sigaction(SIGILL, &old, nullptr);
    return ok && !g_probe_trapped;
}

static int memcpy2_init(struct test *test)
{
    (void)test;
    const long ps = sysconf(_SC_PAGESIZE);
    g_page_size = ps > 0 ? (size_t)ps : 4096;

    g_geom_page = g_page_size;
    if (const char *pv = std::getenv("SDCSHIELD_MEMCPY2_PAGE_SIZE")) {
        char *end = nullptr;
        const unsigned long v = std::strtoul(pv, &end, 0);
        if (!end || *end || v < g_page_size || v > 65536 || (v & (v - 1)) != 0) {
            log_error("memcpy2_neon: SDCSHIELD_MEMCPY2_PAGE_SIZE must be a power "
                      "of two in [%zu, 65536] (got '%s')", g_page_size, pv);
            return EXIT_FAILURE;
        }
        g_geom_page = (size_t)v;
    }
    g_geom_mask = g_geom_page - 1;

    if (const char *mode = std::getenv("SDCSHIELD_MEMCPY2_MODE")) {
        if (!std::strcmp(mode, "all")) g_mode = MODE_ALL;
        else if (!std::strcmp(mode, "copy")) g_mode = MODE_COPY;
        else if (!std::strcmp(mode, "stlf")) g_mode = MODE_STLF;
        else if (!std::strcmp(mode, "regen")) g_mode = MODE_REGEN;
        else if (!std::strcmp(mode, "protect")) g_mode = MODE_PROTECT;
        else {
            log_error("memcpy2_neon: SDCSHIELD_MEMCPY2_MODE must be "
                      "all|copy|stlf|regen|protect (got '%s')", mode);
            return EXIT_FAILURE;
        }
    }
    if (const char *inject = std::getenv("SDCSHIELD_MEMCPY2_INJECT")) {
        if (!std::strcmp(inject, "source")) g_inject = INJECT_SOURCE;
        else if (!std::strcmp(inject, "destination")) g_inject = INJECT_DESTINATION;
        else if (!std::strcmp(inject, "guard")) g_inject = INJECT_GUARD;
        else {
            log_error("memcpy2_neon: SDCSHIELD_MEMCPY2_INJECT must be "
                      "source|destination|guard (got '%s')", inject);
            return EXIT_FAILURE;
        }
    }
    if (g_inject != INJECT_NONE && g_mode == MODE_STLF) {
        log_error("memcpy2_neon: fault-injection selftest requires "
                  "all/copy/regen/protect mode");
        return EXIT_FAILURE;
    }

    blob_init();

    g_have_dczva = el0_instruction_available([] {
        asm volatile("dc zva, %0" :: "r"(g_probe_buf) : "memory");
    });
    g_have_ivac = el0_instruction_available([] {
        asm volatile("dc ivac, %0" :: "r"(g_probe_buf) : "memory");
    });
    g_have_civac = el0_instruction_available([] {
        asm volatile("dc civac, %0" :: "r"(g_probe_buf) : "memory");
    });
    log_info("memcpy2_neon: mode=%s inject=%s page_size=%zu geom_page=%zu "
             "DC_ZVA=%d DC_IVAC=%d DC_CIVAC=%d",
             mode_name(g_mode), g_inject == INJECT_NONE ? "none" : "enabled",
             g_page_size, g_geom_page, g_have_dczva, g_have_ivac, g_have_civac);
    return EXIT_SUCCESS;
}

/* pass 运行默认不保留 INFO 日志，覆盖率必须能被执行器留存：
 * SDCSHIELD_MEMCPY2_STATS=<file> 时每 worker 追加一行（O_APPEND 跨线程安全）。 */
static void emit_coverage(const worker_ctx &c)
{
    const char *path = std::getenv("SDCSHIELD_MEMCPY2_STATS");
    if (!path || !*path)
        return;
    const int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
    if (fd < 0)
        return;
    dprintf(fd, "mode=%s cfg_cpu=%d os_cpu=%d iter=%llu copies=%llu bytes=%llu "
                "cross_page=%llu anchored=%llu regen=%llu protect=%llu "
                "geom_page=%zu\n",
            mode_name(g_mode), c.cpu, device_info[c.cpu].cpu_number,
            (unsigned long long)c.iter, (unsigned long long)c.segments,
            (unsigned long long)c.copied_bytes,
            (unsigned long long)c.cross_page_segments,
            (unsigned long long)c.anchored_segments,
            (unsigned long long)c.regen_count, (unsigned long long)c.protect_count,
            g_geom_page);
    close(fd);
}

static int memcpy2_run(struct test *test, int cpu)
{
    /* MAP_NORESERVE 只控制 VA/commit；实际 RSS 仍须由执行器监控。
     * 映射基址按 max(真实页, 几何页) 对齐，使偏移锚定在绝对页边界上。 */
    const size_t align = g_geom_page > g_page_size ? g_geom_page : g_page_size;
    const size_t raw_len = REGION_HALF * 2 + align;
    void *raw = mmap(nullptr, raw_len, PROT_READ | PROT_WRITE,
                     MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (raw == MAP_FAILED) {
        report_fail_msg("memcpy2_neon: INFRASTRUCTURE-ERROR stage=mmap "
                        "bytes=%zu error=%s", raw_len, std::strerror(errno));
        return EXIT_FAILURE;
    }
    uint8_t *region = reinterpret_cast<uint8_t *>(
        ((uintptr_t)raw + align - 1) & ~(uintptr_t)(align - 1));

    worker_ctx c {};
    c.raw = raw;
    c.raw_len = raw_len;
    c.region = region;
    c.region_len = REGION_HALF * 2;
    c.src_base = c.region;
    c.dst_base = c.src_base + REGION_HALF;
    c.scratch = c.dst_base + SCRATCH_OFF;
    c.cpu = cpu;
    g_pattern_base = (uint64_t)(uintptr_t)c.region;

    bool ok = true;
    do {
        ++c.iter;
        const unsigned k = 1 + (unsigned)(random64() % MAX_SEG_N);
        for (unsigned si = 0; si < k && ok; ++si) {
            c.segment = si;
            const size_t seg_len = pick_seg_len();
            if (g_mode == MODE_STLF) {
                const placement pd = pick_placement();
                const size_t len = seg_len / 2
                                 + (size_t)(random64() % (seg_len / 2));
                ok = do_stlf(c, place_at(c.dst_base, pd), seg_len, len);
            } else {
                ok = do_segment(c, seg_len, g_mode == MODE_ALL);
            }
        }
        if (!ok)
            break;

        c.segment = SIZE_MAX;
        if ((g_mode == MODE_ALL || g_mode == MODE_PROTECT) &&
            c.iter % PROT_EVERY == 0) {
            constexpr size_t protect_len = 1UL << 20;   /* 4K/16K/64K 均整除 */
            const size_t fl_off = (size_t)(random64() % (SCRATCH_LEN - protect_len))
                                & ~g_geom_mask;
            uint8_t *fl = c.scratch + fl_off;
            if (mprotect(fl, protect_len, PROT_READ) != 0) {
                report_fail_msg("memcpy2_neon: INFRASTRUCTURE-ERROR "
                                "stage=mprotect-read iter=%llu error=%s",
                                (unsigned long long)c.iter, std::strerror(errno));
                ok = false;
                break;
            }
            /* 只读状态下按真实页逐页触读：强制 TLBI 之后的 TLB 重填与 PTW
             * 重播；恢复写权限前完成，避免这段 TLB 压力空转（合并自并发
             * 会话的实现）。 */
            for (size_t o = 0; o < protect_len; o += g_page_size) {
                uint8_t value = 0;
                asm volatile("ldrb %w0, [%1]" : "=r"(value) : "r"(fl + o)
                             : "memory");
                (void)value;
            }
            if (mprotect(fl, protect_len, PROT_READ | PROT_WRITE) != 0) {
                report_fail_msg("memcpy2_neon: INFRASTRUCTURE-ERROR "
                                "stage=mprotect-write iter=%llu error=%s",
                                (unsigned long long)c.iter, std::strerror(errno));
                ok = false;
                break;
            }
            ++c.protect_count;
        }

        if (g_mode == MODE_ALL && !do_scratch(c)) {
            ok = false;
            break;
        }

        if ((g_mode == MODE_ALL || g_mode == MODE_REGEN) &&
            c.iter % REGEN_EVERY == 0) {
            if (madvise(region, c.region_len, MADV_DONTNEED) != 0) {
                report_fail_msg("memcpy2_neon: INFRASTRUCTURE-ERROR stage=madvise "
                                "iter=%llu error=%s", (unsigned long long)c.iter,
                                std::strerror(errno));
                ok = false;
                break;
            }
            ++c.regen_count;
        }
    } while (test_time_condition(test));

    log_info("memcpy2_neon: coverage mode=%s cpu=%d os_cpu=%d iter=%llu copies=%llu "
             "copy_bytes=%llu cross_page=%llu anchored=%llu regen=%llu protect=%llu "
             "epoch=%llu geom_page=%zu",
             mode_name(g_mode), c.cpu, device_info[c.cpu].cpu_number,
             (unsigned long long)c.iter, (unsigned long long)c.segments,
             (unsigned long long)c.copied_bytes,
             (unsigned long long)c.cross_page_segments,
             (unsigned long long)c.anchored_segments,
             (unsigned long long)c.regen_count, (unsigned long long)c.protect_count,
             (unsigned long long)c.epoch, g_geom_page);
    emit_coverage(c);
    munmap(c.raw, c.raw_len);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int memcpy2_finish(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(memcpy2_neon, "Replayable staged NEON copy + STLF/MMU isolation modes")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = memcpy2_init,
    .test_run = memcpy2_run,
    .test_cleanup = memcpy2_finish,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

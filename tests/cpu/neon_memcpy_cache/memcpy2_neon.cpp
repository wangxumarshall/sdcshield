/*  memcpy2_neon — 大区域窗口轮换 + 错位双向 NEON 拷贝 + STLF 重叠 / 混合位宽回读
 *
 *  目标（arm64 服务器芯片 SDC 复现，怀疑面 LSU + MMU）：
 *    - LSU：非对齐 vld1q/vst1q 16B 流、LDP/STP 非临时对存储、store-to-load
 *      forwarding、存储消歧（dst 与 src 同偏移 = VA 低 29 位全同的别名偏置）。
 *    - MMU：64K 页上跨页段、随机页选择 + 小概率 MiB 级大段（> L2），
 *      mprotect 翻转（TLB invalidation）、周期性 MADV_DONTNEED（PTE 拆除+
 *      重填、page-table walker 重播）、重填后可能换物理页（物理别名）。
 *
 *  Oracle（本测试最重要的部分）：图样是**地址的纯函数**
 *      pattern(VA, epoch, cpu)
 *  拷贝正确 <=> dst[i] == pattern(src_va + i)。因此
 *    - 不需要影子副本，重叠拷贝（memmove 语义）也能判对错；
 *    - 源端被读坏（load-path SDC）与目的端被写坏（store-path SDC）同样检出；
 *    - 错地址 load/store（承载错误、偏移写、stale TLB 旧映射）会被定位到
 *      具体 VA 偏移，并反查脏值来自哪个 (epoch, 偏移)。
 *  epoch 每次动作 +1，图样永不重用 —— 迟到的旧写会被识别为 STALE。
 *
 *  与草案（memcpy1_ab_forever 内的 A+B 改造版）相比的实质修正：
 *    1. 重叠拷贝必须**倒序**（dst > src 的前向逐块拷贝不是 memmove：
 *       16B 块大小下 delta 1..15 会自我涂抹，草案按 fwd 校验必然恒 fail）。
 *    2. DC ZVA 的块按 256B 对齐再刷，且只校验被刷过的 256B；草案用未对齐
 *       地址 + 4x64B 假定，末块覆盖不全（63/64 概率误报 "dc zva residue"）。
 *    3. STLF 段 forensics 的 lo-guard 指针指向刚写入的数据（必然非 poison），
 *       草案会在任何真实失配时先误报 "GUARD ZONE overwritten"；本版只校验
 *       真实存在的 guard 区间。
 *    4. dst 守卫/尾哨兵放在**实际拷贝长度 len** 之后并逐字节核对；草案把
 *       guard 放在 seg_len 之后、[len, seg_len) 区间从不校验，溢出检测留洞。
 *    5. 图样按 8B 块计算（一次 mix 出 8 字节）。逐字节 SplitMix 使校验占
 *       总时间 ~97%，实际访存被饿死；8B 块把填充/校验降到 ~1 cycle/B，
 *       检测力不变（任一位翻转必定偏离期望值）。
 *    6. mprotect/madvise 改用**外层迭代计数**触发；草案的 epoch%64<48 实际
 *       是 75% 的 epoch 都触发（与注释"每 64 次一次"不符），MADV_DONTNEED
 *       1000 epoch 一次则把 1 GiB 页表拆除成本摊进每 ~30 次迭代。
 *    7. 1 GiB/线程 x 128 线程 = 128 GiB VA，必须 MAP_NORESERVE；RSS 由
 *       周期性 DONTNEED 钉在 ~20 MiB/线程（机测 61 GiB 内存可跑全核）。
 *    8. 补 END_DECLARE_TEST（草案缺，无法编译）。
 *
 *  能力边界（诚实声明）：DC ZVA 需 SCTLR_EL1.DZE、DC CIVAC 需 UCI 对 EL0
 *  开放。init 用一次性 SIGILL 捕获探测；缺失时仅禁用对应**子步**（指令多样性
 *  / 失败 triage 冲刷），主拷贝 oracle 与判定不受影响 —— 不是占位测试。
 */
#include <sandstone.h>
#include "arm_add_neon_copy.h"
#include <cerrno>
#include <csetjmp>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
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

static size_t g_page_size = 4096;
static size_t g_page_mask = 4095;
static bool   g_have_dczva;      /* DC ZVA @ EL0 (SCTLR_EL1.DZE) */
static bool   g_have_civac;      /* DC CIVAC @ EL0 (SCTLR_EL1.UCI) */

/* ---- 图样 oracle：8B 块粒度，块内第 j 字节 = pat_word 的第 j 字节 ----
 * mix64 每次出 8 字节期望值；任一位翻转必然偏离期望（不是概率检测）。 */
static inline uint64_t mix64(uint64_t z)
{
    z += 0x9e3779b97f4a7c15ULL;
    z = (z ^ (z >> 30)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31);
}

static inline uint64_t pat_word(uint64_t va8, uint64_t epoch, int cpu)
{
    return mix64(va8 ^ (epoch * GOLDEN) ^ ((uint64_t)(cpu + 1) * GOLDEN2));
}

static inline uint8_t pat_byte(uint64_t va, uint64_t epoch, int cpu)
{
    uint64_t w = pat_word(va & ~(uint64_t)7, epoch, cpu);
    return (uint8_t)(w >> ((unsigned)(va & 7) * 8));
}

/* 以 VA 为基准填图样（mem 与自身 VA 恒定相等） */
static void fill_pat(uint8_t *mem, size_t n, uint64_t epoch, int cpu)
{
    const uint64_t va = (uint64_t)mem;
    size_t i = 0;
    while (i < n && ((va + i) & 7u)) {
        mem[i] = pat_byte(va + i, epoch, cpu);
        ++i;
    }
    for (; i + 8 <= n; i += 8) {
        uint64_t w = pat_word(va + i, epoch, cpu);
        std::memcpy(mem + i, &w, sizeof w);
    }
    for (; i < n; ++i)
        mem[i] = pat_byte(va + i, epoch, cpu);
}

/* 校验 mem[i] == pat_byte(exp_va + i)；返回首个失配偏移，全对返回 n。
 * exp_va 与 mem 解耦：重叠拷贝时期望值按**源地址**取图样。 */
static size_t verify_pat(const uint8_t *mem, uint64_t exp_va, size_t n,
                         uint64_t epoch, int cpu)
{
    size_t i = 0;
    while (i < n && ((exp_va + i) & 7u)) {
        if (mem[i] != pat_byte(exp_va + i, epoch, cpu))
            return i;
        ++i;
    }
    for (; i + 8 <= n; i += 8) {
        uint64_t want = pat_word(exp_va + i, epoch, cpu);
        uint64_t got;
        std::memcpy(&got, mem + i, sizeof got);
        if (got != want) {
            for (size_t j = 0; j < 8; ++j)
                if (mem[i + j] != pat_byte(exp_va + i + j, epoch, cpu))
                    return i + j;
            return i;
        }
    }
    for (; i < n; ++i)
        if (mem[i] != pat_byte(exp_va + i, epoch, cpu))
            return i;
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
 * 草案用的前向逐块在 delta < 16 时会自我涂抹，校验必然失配。 */
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

/* ---- 失败取证 ---- */
static uint64_t va_to_pfn(uint64_t va)
{
    int fd = open("/proc/self/pagemap", O_RDONLY);
    if (fd < 0)
        return 0;
    uint64_t entry = 0;
    ssize_t r = pread(fd, &entry, sizeof(entry),
                      (off_t)((va / 4096) * sizeof(uint64_t)));
    close(fd);
    if (r != (ssize_t)sizeof(entry) || !(entry & (1ULL << 63)))
        return 0;
    return entry & ((1ULL << 55) - 1);   /* 需 CAP_SYS_ADMIN，否则恒 0 */
}

static void hexdump_128(const uint8_t *p)
{
    for (int half = -64; half < 64; half += 16) {
        char buf[16 * 3 + 8];
        int o = 0;
        for (int j = 0; j < 16; ++j)
            o += snprintf(buf + o, sizeof(buf) - (size_t)o, "%02x ", p[half + j]);
        report_fail_msg("  ctx %+d: %s", half, buf);
    }
}

struct worker_ctx {
    uint8_t *src_base;
    uint8_t *dst_base;
    uint8_t *scratch;
    uint8_t *region;
    size_t region_len;
    int cpu;
    uint64_t epoch;   /* 全局动作号：每个拷贝/填充动作 +1，图样不重用 */
};

/* 失败字节分类：连续重读 -> transient；DC CIVAC + DSB -> 是否缓存数组问题 */
static const char *triage_byte(const uint8_t *addr, uint8_t expect,
                               int *transient_out, int *after_flush_out)
{
    int transient = 0;
    for (int i = 0; i < 4; ++i)
        if (addr[0] == expect) {
            transient = 1;
            break;
        }
    *transient_out = transient;
    if (g_have_civac) {
        asm volatile("dc civac, %0" :: "r"(addr) : "memory");
        asm volatile("dsb sy" ::: "memory");
        *after_flush_out = (addr[0] == expect);
    } else {
        *after_flush_out = -1;
    }
    if (transient)
        return "TRANSIENT(load/LSU)";
    if (*after_flush_out == 1)
        return "CACHE-ARRAY/COHERENCY";
    if (*after_flush_out == 0)
        return "PERSISTENT(store-path/DRAM)";
    return "PERSISTENT(store-path/DRAM; flush n/a)";
}

/* 单个失配字节的完整取证（不负责 munmap；由单一退出点清理）。
 * epoch 显式传入：STLF 回读校验的是"拷贝时"的图样（c.epoch 已前进）。 */
static void report_mismatch(int cpu, uint64_t epoch, const uint8_t *mem,
                            size_t off, uint64_t exp_va, size_t len,
                            unsigned s_mis, unsigned d_mis, const char *dir)
{
    const uint64_t want_va = exp_va + off;
    const uint8_t want = pat_byte(want_va, epoch, cpu);
    const uint8_t got = mem[off];
    int transient = 0, after_flush = -1;
    const char *verdict = triage_byte(mem + off, want, &transient, &after_flush);
    uint64_t pfn = va_to_pfn((uint64_t)(mem + off));

    report_fail_msg("memcpy2_neon: %s mismatch epoch=%llu off=%zu/%zu got=%02x "
                    "want=%02x s_mis=%u d_mis=%u cpu=%d VA=%p PFN=%llu | %s",
                    dir, (unsigned long long)epoch, off, len, got, want,
                    s_mis, d_mis, cpu, (const void *)(mem + off),
                    (unsigned long long)pfn, verdict);
    report_fail_msg("  triage: transient_reread=%d after-dc-civac-correct=%d",
                    transient, after_flush);
    hexdump_128(mem + off);

    /* 反查脏值来源：近 5 个 epoch x +/-64B 内哪个图样等于 got（地址纯函数的
     * 好处：源已被覆盖也能推出它"应该"是什么） */
    for (uint64_t e2 = epoch > 4 ? epoch - 4 : 0; e2 <= epoch; ++e2)
        for (int64_t adv = -64; adv <= 64; ++adv) {
            if (adv == 0)
                continue;
            if (pat_byte(want_va + (uint64_t)adv, e2, cpu) == got) {
                report_fail_msg("  dirty byte = pattern of %s epoch=%llu "
                                "delta=%+lld => %s",
                                e2 == epoch ? "SAME-epoch" : "STALE",
                                (unsigned long long)e2, (long long)adv,
                                e2 == epoch
                                    ? "STORE-MISFORWARD/OFFSET write"
                                    : "STALE-TLB / old-epoch remap");
                break;
            }
        }
}

/* 校验一段：数据 -> guard/尾哨兵（放在**实际长度 len** 之后） */
static bool check_seg(int cpu, uint64_t epoch, const uint8_t *mem,
                      uint64_t exp_va, size_t len, unsigned s_mis,
                      unsigned d_mis, const char *dir, uint8_t poison,
                      const uint8_t *lo, size_t lo_n,
                      const uint8_t *hi, size_t hi_n)
{
    size_t bad = verify_pat(mem, exp_va, len, epoch, cpu);
    if (bad != len) {
        report_mismatch(cpu, epoch, mem, bad, exp_va, len, s_mis, d_mis, dir);
        return false;
    }
    if (lo_n && !poison_intact(lo, lo_n, poison)) {
        report_fail_msg("memcpy2_neon: %s LO-GUARD overwritten (epoch=%llu) "
                        "=> OOB store below dst", dir,
                        (unsigned long long)epoch);
        return false;
    }
    if (hi_n && !poison_intact(hi, hi_n, poison)) {
        report_fail_msg("memcpy2_neon: %s TAIL/HI-GUARD overwritten "
                        "(epoch=%llu, len=%zu) => write past copy end", dir,
                        (unsigned long long)epoch, len);
        return false;
    }
    return true;
}

static inline size_t rand_span_off(void)
{
    return (size_t)(random64() % (SEG_SPAN - MAX_HUGE)) & ~g_page_mask;
}

static inline size_t pick_seg_len(void)
{
    if ((random64() % 16) == 0)   /* 1/16：> L2 的大段，压 MMU/page-walk */
        return MIN_HUGE + (size_t)(random64() % ((MAX_HUGE - MIN_HUGE) >> 12)) * 4096;
    return MIN_SEG << (unsigned)(random64() % 5);   /* 4K..64K，2 的幂 */
}

/* STLF：dst = src + delta (1..15) 的重叠 memmove-up + 立即混合位宽回读 */
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
    const uint8_t poison = (uint8_t)~(0xC3 ^ (unsigned)(c.epoch & 0xFF));

    /* lo 侧与源数据物理重叠，不能作 guard；只设 hi guard */
    fill_pat(src_ov, ov_len, c.epoch, c.cpu);
    std::memset(dst_ov + ov_len, poison, GUARD);
    neon_copy_rev(dst_ov, src_ov, ov_len);   /* 倒序 = memmove-up */

    /* 期望值按**源地址**取图样；源已被部分覆盖也不影响 */
    if (!check_seg(c.cpu, c.epoch, dst_ov, (uint64_t)src_ov, ov_len, 0, delta,
                   "stlf-overlap", poison, nullptr, 0,
                   dst_ov + ov_len, GUARD))
        return false;
    const uint64_t copy_epoch = c.epoch;
    ++c.epoch;

    /* 混合位宽立即回读：半数靶向刚写过的尾部行（最大化 STLF 窗口），
     * 半数为随机偏移 + 随机对齐态（含非 8B 对齐的 ldr/ldp 跨行拆分） */
    for (unsigned t = 0; t < 16; ++t) {
        const size_t span = ov_len - 16;
        size_t off;
        if (t < 8)
            off = ov_len - 16 - (size_t)(random64() % (span < 64 ? span : 64));
        else
            off = (t & 1) ? (size_t)(random64() % span)
                          : ((size_t)(random64() % span) & ~(size_t)7);
        uint8_t *a = dst_ov + off;
        const uint64_t as = (uint64_t)(src_ov + off);   /* 期望图样基准 */
        uint64_t w = 0, p1 = 0, p2 = 0;
        unsigned hw = 0;
        uint8_t b = 0;

        switch (random64() % 4) {
        case 0:
            asm volatile("ldrb %w0, [%1]" : "=r"(b) : "r"(a) : "memory");
            if (b != pat_byte(as, copy_epoch, c.cpu)) {
                report_mismatch(c.cpu, copy_epoch, dst_ov, off,
                                (uint64_t)src_ov, ov_len, 0, 0, "stlf-ldrb");
                return false;
            }
            break;
        case 1:
            asm volatile("ldrh %w0, [%1]" : "=r"(hw) : "r"(a) : "memory");
            for (unsigned j = 0; j < 2; ++j)
                if (((const uint8_t *)&hw)[j] !=
                    pat_byte(as + j, copy_epoch, c.cpu)) {
                    report_mismatch(c.cpu, copy_epoch, dst_ov, off + j,
                                    (uint64_t)src_ov, ov_len, 0, 0,
                                    "stlf-ldrh");
                    return false;
                }
            break;
        case 2:
            asm volatile("ldr %0, [%1]" : "=r"(w) : "r"(a) : "memory");
            for (unsigned j = 0; j < 8; ++j)
                if (((const uint8_t *)&w)[j] !=
                    pat_byte(as + j, copy_epoch, c.cpu)) {
                    report_mismatch(c.cpu, copy_epoch, dst_ov, off + j,
                                    (uint64_t)src_ov, ov_len, 0, 0,
                                    "stlf-ldr");
                    return false;
                }
            break;
        default:
            asm volatile("ldp %0, %1, [%2]" : "=r"(p1), "=r"(p2) : "r"(a)
                         : "memory");
            for (unsigned j = 0; j < 16; ++j) {
                uint8_t got = (j < 8) ? ((const uint8_t *)&p1)[j]
                                      : ((const uint8_t *)&p2)[j - 8];
                if (got != pat_byte(as + j, copy_epoch, c.cpu)) {
                    report_mismatch(c.cpu, copy_epoch, dst_ov, off + j,
                                    (uint64_t)src_ov, ov_len, 0, 0,
                                    "stlf-ldp");
                    return false;
                }
            }
            break;
        }
    }
    return true;
}

/* 一个段：正序、倒序两次拷贝（各自重抽长度/错位），再叠 STLF 探针 */
static bool do_segment(worker_ctx &c, size_t seg_len)
{
    const size_t s_off = rand_span_off();
    const size_t d_off = (random64() % 4 == 0) ? s_off : rand_span_off();

    unsigned s_mis = (unsigned)(random64() % MAX_MISALIGN);
    unsigned d_mis = (random64() % 4 == 0) ? s_mis
                                           : (unsigned)(random64() % MAX_MISALIGN);
    size_t len = seg_len / 2 + (size_t)(random64() % (seg_len / 2));
    uint8_t *s = c.src_base + s_off + s_mis;
    uint8_t *d = c.dst_base + d_off + d_mis;
    uint8_t poison = (uint8_t)~(0xA5 ^ (unsigned)(c.epoch & 0xFF));

    /* 投毒 [d-GUARD, d+seg_len+GUARD)；拷贝后 [d, d+len) 被覆盖，
     * 其余（lo guard + 尾部哨兵）必须仍是 poison */
    std::memset(d - GUARD, poison, GUARD + seg_len + GUARD);
    fill_pat(s, len, c.epoch, c.cpu);
    neon_copy(d, s, len);                       /* 正序 */
    if (!check_seg(c.cpu, c.epoch, d, (uint64_t)s, len, s_mis, d_mis,
                   "fwd-seg", poison, d - GUARD, GUARD,
                   d + len, (seg_len - len) + GUARD))
        return false;
    ++c.epoch;

    /* 倒序：重抽错位与长度 */
    s_mis = (unsigned)(random64() % MAX_MISALIGN);
    d_mis = (random64() % 4 == 0) ? s_mis
                                  : (unsigned)(random64() % MAX_MISALIGN);
    len = seg_len / 2 + (size_t)(random64() % (seg_len / 2));
    s = c.src_base + s_off + s_mis;
    d = c.dst_base + d_off + d_mis;
    poison = (uint8_t)~(0x5A ^ (unsigned)(c.epoch & 0xFF));

    std::memset(d - GUARD, poison, GUARD + seg_len + GUARD);
    fill_pat(s, len, c.epoch, c.cpu);
    neon_copy_rev(d, s, len);                   /* 倒序 */
    if (!check_seg(c.cpu, c.epoch, d, (uint64_t)s, len, s_mis, d_mis,
                   "rev-seg", poison, d - GUARD, GUARD,
                   d + len, (seg_len - len) + GUARD))
        return false;
    ++c.epoch;

    return do_stlf(c, d, seg_len, len);
}

/* scratch 指令多样性：DC ZVA（按 256B 对齐刷 256B + 两侧哨兵不出界）、
 * STNP/LDP 非临时对存储（对齐与非对齐各一，含邻域不被误写） */
static bool do_scratch(worker_ctx &c)
{
    const size_t usable = SCRATCH_LEN - 4096;

    if (g_have_dczva) {
        /* 256B 对齐：对 64/128/256B 的 DC ZVA 块，4 x 64B 步进都覆盖
         * [z, z+256)，且不会波及两侧哨兵 */
        size_t off = (size_t)(random64() % (usable - 384)) & ~(size_t)255;
        uint8_t *z = c.scratch + off;
        std::memset(z - GUARD, 0xFF, 256 + 2 * GUARD);
        asm volatile("dsb sy" ::: "memory");
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

    /* STNP 16B 对存储：对齐 / 非对齐各一；前后各 8B 哨兵必须不动 */
    for (unsigned t = 0; t < 2; ++t) {
        size_t off = (size_t)(random64() % (usable - 64));
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
        asm volatile("dsb sy" ::: "memory");
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
    long ps = sysconf(_SC_PAGESIZE);
    g_page_size = ps > 0 ? (size_t)ps : 4096;
    g_page_mask = g_page_size - 1;

    g_have_dczva = el0_instruction_available([] {
        asm volatile("dc zva, %0" :: "r"(g_probe_buf) : "memory");
    });
    g_have_civac = el0_instruction_available([] {
        asm volatile("dc civac, %0" :: "r"(g_probe_buf) : "memory");
    });
    if (!g_have_dczva || !g_have_civac)
        log_info("memcpy2_neon: DC ZVA=%d DC CIVAC=%d at EL0; "
                 "unavailable sub-steps disabled (copy oracle unaffected)\n",
                 g_have_dczva, g_have_civac);
    return EXIT_SUCCESS;
}

static int memcpy2_run(struct test *test, int cpu)
{
    (void)test;

    /* 1 GiB/线程 x 全核：MAP_NORESERVE 避免严格 overcommit 下 ENOMEM；
     * RSS 由 REGEN_EVERY 的 MADV_DONTNEED 钉住（实测 ~20 MiB/线程） */
    void *region = mmap(nullptr, REGION_HALF * 2, PROT_READ | PROT_WRITE,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0);
    if (region == MAP_FAILED) {
        report_fail_msg("memcpy2_neon: mmap %zu bytes failed: %s",
                        REGION_HALF * 2, std::strerror(errno));
        return EXIT_FAILURE;
    }

    worker_ctx c;
    c.region = static_cast<uint8_t *>(region);
    c.region_len = REGION_HALF * 2;
    c.src_base = c.region;
    c.dst_base = c.src_base + REGION_HALF;
    c.scratch = c.dst_base + SCRATCH_OFF;
    c.cpu = cpu;
    c.epoch = 0;

    bool ok = true;
    uint64_t iter = 0;
    do {
        ++iter;
        unsigned k = 1 + (unsigned)(random64() % MAX_SEG_N);
        for (unsigned si = 0; si < k && ok; ++si)
            ok = do_segment(c, pick_seg_len());

        if (!ok)
            break;

        /* 真 TLB invalidation 流量（scratch 内翻转 1 MiB） */
        if (iter % PROT_EVERY == 0) {
            /* mprotect 要求页对齐（草案直接用随机偏移 -> EINVAL） */
            size_t fl_off = (size_t)(random64() % (SCRATCH_LEN - (2UL << 20)))
                            & ~g_page_mask;
            uint8_t *fl = c.scratch + fl_off;
            if (mprotect(fl, 1UL << 20, PROT_READ) != 0 ||
                mprotect(fl, 1UL << 20, PROT_READ | PROT_WRITE) != 0) {
                report_fail_msg("memcpy2_neon: mprotect flip failed: %s",
                                std::strerror(errno));
                ok = false;
                break;
            }
        }

        if (!do_scratch(c)) {
            ok = false;
            break;
        }

        /* PTE 拆除 + 重填：page-table walker 重播，并可能换物理页 */
        if (iter % REGEN_EVERY == 0)
            if (madvise(region, c.region_len, MADV_DONTNEED) != 0) {
                report_fail_msg("memcpy2_neon: madvise MADV_DONTNEED failed: %s",
                                std::strerror(errno));
                ok = false;
                break;
            }
    } while (test_time_condition(test));

    munmap(region, c.region_len);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int memcpy2_finish(struct test *test)
{
    (void)test;
    return EXIT_SUCCESS;
}

DECLARE_TEST(memcpy2_neon, "Large-window misaligned NEON copy + STLF readback")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = memcpy2_init,
    .test_run = memcpy2_run,
    .test_cleanup = memcpy2_finish,
    .fracture_loop_count = -1,   /* never fracture (arm_add convention) */
    .quality_level = TEST_QUALITY_PROD,
END_DECLARE_TEST

/**
 * @copyright
 * Copyright 2026.
 * SPDX-License-Identifier: Apache-2.0
 *
 * @test @b sve512_pred_ops_arm
 * @parblock
 * SVE predicate-dimension SDC stress — the predicate machinery every
 * other SVE test leaves at full-true (all 11 existing SVE tests use
 * svptrue exclusively). The per-lane predicate network (whilelt
 * generation, partial activity, predicate inversion, FFR) is new silicon
 * SVE introduced relative to NEON, i.e. first-generation risk (PinDrop
 * HPCA'26), and a predicate bit corruption is silent by construction:
 * the wrong lanes compute or fail to compute, and the wrong result is
 * still a bit-legal value — exactly an SDC shape.
 *
 * Four sections, each with an exact golden:
 *
 * 1. Predicate-density sweep: for active-lane counts k = 1..VL, whilelt
 *    masks the first k lanes; an svmla_f64_m under that predicate must
 *    update exactly lanes [0, k) and leave lanes [k, VL) untouched. The
 *    active lanes carry a finite addend (so the computed result has a
 *    deterministic golden), the untouched lanes a sentinel (0xdead...);
 *    the test verifies BOTH the computed lanes (byte-exact golden) and
 *    the untouched lanes (sentinel preserved) — a predicate that fails to
 *    mask, or masks the wrong lanes, fails either half.
 *
 * 2. Predicate inversion: svnot_b_z of a whilelt predicate drives a
 *    second FMA whose active set is the complement; golden computed per
 *    lane by the same conditional rule. Together with section 1 this
 *    exercises predicate NOT + both polarity paths.
 *
 * 3. FADDA chain: the architecturally-sequential predicate-driven
 *    horizontal add (svadda_f64) over 256 steps — a serial dependency
 *    through the reduction adder, golden in the same lane order
 *    (FADDA is defined left-to-right, so the scalar mirror is exact).
 *
 * 4. LDFF1 + FFR, three faces: (a) first-fault load from valid mapped
 *    memory must leave FFR all-true — svrdffr() materialized into a byte
 *    image (one byte per FFR bit) and compared against the golden bitmap
 *    BIT-FOR-BIT by memcmp, FFR primed with svsetffr() first
 *    (an LDFF1 only clears the bit of a faulting element, never sets
 *    one, so the readback is only meaningful against an all-ones start);
 *    (b) a FAULTING first-fault load — element 0 mapped, element 1 on an
 *    unmapped page — must suppress the fault, clear the FFR from lane 1
 *    onward, and zero the unloaded tail (first-fault suppression is
 *    precisely what LDFF1 exists for, not kernel territory: the expected
 *    partially-cleared bit pattern is the only in-process way to drive
 *    FFR bits to 0 and the only face that can see a stuck-at-1 bit in
 *    [8, 8*VL); a fault on the FIRST active element is architecturally
 *    delivered, so lane 0's bits can never be expected-0 and a
 *    stuck-at-1 there is untestable by LDFF1 — measured on hardware and
 *    gem5, which agree); (c) a mapped LDFF1 without a SETFFR must leave
 *    the partially-cleared FFR untouched (accumulation). The RDFFR is
 *    issued back-to-back with the LDFF1,
 *    and the LDFF1 back-to-back with the SETFFR, with no function call
 *    anywhere in that window — each capture is encapsulated in the
 *    single small noinline function ffr_ldff1_capture (whose call
 *    boundary doubles as the mechanical CSE killer for RDFFR
 *    freshness), and the (b)+(c) pair (whose between-captures gap must
 *    contain nothing but the two capture calls) in the combined
 *    wrapper ffr_fault_acc_capture (see there): the arm64
 *    Linux syscall ABI discards live
 *    SVE state at syscall entry (measured on a 6.6 kernel: FFR is
 *    zeroed, Z/P preserved; kernel sve_flush_{z,p,ffr} macros exist and
 *    newer kernels may flush more), and a
 *    first-fault load from mapped memory only ever CLEARS FFR bits —
 *    it never sets one — so a syscall anywhere between the SETFFR and
 *    the RDFFR (not just between LDFF1 and RDFFR) leaves the FFR
 *    reading as 0 — a false SDC. FMAXV tree reduction
 *    (order-independent, exactly checkable) over the whilelt-masked
 *    prefix.
 *
 * test_init gates via the framework layer (arm64_require_sve_vl:
 * HWCAP_SVE capability + 128..1024-bit range in one call, all-VL
 * Phase 2 P6) and returns a clean EXIT_SKIP on SVE-less CPUs; test_run
 * re-checks the VL and fails closed on drift. Built in tests_arm64_sve.
 * @endparblock
 */

#include <sandstone.h>
#include <cstdint>
#include <cinttypes>
#include <cstring>
#include <cmath>
#include <memory>
#include <vector>

#ifdef __aarch64__
#include <arm_sve.h>
#include <device/cpu/arm64/arm64_vl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <errno.h>
#endif

static constexpr size_t FADDA_STEPS = 256;

// Sentinel for untouched-lane detection (a quiet NaN payload; any
// untouched lane keeps these exact bits).
static constexpr uint64_t SENTINEL = 0x7FF8DEADBEEF0000ULL;

struct PredOpsData {
    size_t vl_d;
    std::vector<double> src;            // exact operands: src[j] = (j%4)-1.5
    size_t page_bytes = 0;              // page size, gated in init
                                        // (>= 8*vl_d), reused by every
                                        // worker's ffr_hole_arm
};

#ifdef __aarch64__

/* Per-WORKER page pair for the faulting-LDFF1 face (round-5: the pair used
 * to live in PredOpsData — one hole shared by every worker of the forked
 * child; per-worker ownership removes that cross-thread lifetime coupling
 * and is the precondition for the interference recovery in
 * ffr_check_fault_acc_faces, which swaps in a fresh pair without racing a
 * sibling's capture): [pages, pages+ps) mapped and filled with src at its
 * tail, [pages+ps, pages+2*ps) punched out (munmap) so element 1 of
 * fault_k1 lands on an unmapped page. The hole is made with munmap, not
 * mprotect(PROT_NONE): gem5 SE maps mprotect to a no-op (ignoreFunc in
 * the ArmLinux64 syscall table), and the fault source must behave
 * identically on hardware and in the gem5-fi dual-kernel validation.
 *
 * Honest residual risk (round-3 review #18): munmap keeps no reservation,
 * so the kernel may legally hand the hole page to a LATER <=1-page mmap
 * request — with the default top-down allocator the hole sits directly
 * below the mapping that preceded it, i.e. exactly at the allocation
 * frontier, so it IS the first candidate. If that happens mid-run the
 * fault never occurs, the fault/acc goldens break, and a naive check
 * would cry SDC. Probability is low (in-process anonymous mmaps during a
 * test run are glibc thread stacks at 8MB and malloc arenas far above the
 * 128KB mmap threshold — a 1-page anonymous mmap has no ordinary
 * producer; the per-worker arms are 2-page requests and cannot land in a
 * 1-page hole), and even a landed mapper only breaks the goldens if its
 * page is RESIDENT: LDFF1 suppresses the demand-paging minor fault of a
 * freshly-mapped anonymous page too (measured: a MAP_FIXED
 * refilled-but-untouched hole still fault-clears the FFR tail exactly
 * like the punched hole). Not zero either way, so the fault/acc failure
 * paths carry the ffr_hole_probe() discriminator — see there for the
 * three probe states, the TOCTOU boundary, and the verdict mapping. */
struct ffr_hole {
    void *pages = nullptr;            // mmap base (2 pages)
    size_t page_bytes = 0;            // ONE page: disarm unmaps ONLY page
                                      // 1 (round-4 #22 — the hole page may
                                      // have been re-mapped by an external
                                      // mapper by disarm time, and
                                      // munmapping it would destroy a
                                      // mapping the test does not own;
                                      // process exit reclaims the rest)
    const double *fault_k1 = nullptr; // elem0 = last mapped double
};

/* Arm a fresh pair: mmap 2 pages, punch out page 2, fill the page-1 tail
   with the exact src operands. The governing predicate of the faulting
   capture is all-true, so element 0 (the FIRST active element) must be
   mapped — a fault there would be DELIVERED (first-fault loads only
   suppress faults on elements after the first active one; measured on
   hardware and in gem5) — while element 1 sits on the unmapped page and
   its fault is suppressed into an FFR clear from lane 1 onward. Needs
   8*vl bytes of mapped tail, i.e. 8*vl <= page size (128 <= 4096 at the
   widest supported VL — gated in init, fail closed, not by arithmetic
   luck). Runs OUTSIDE every capture window: the syscall ban is only
   inside ffr_ldff1_capture's [SETFFR, RDFFR] body and the two-call gap
   around ffr_fault_acc_capture. Fails closed via report_fail — a 2-page
   anonymous mmap failing means the address space is exhausted, and a
   loud failure beats a per-worker nondeterministic SKIP (the framework
   aggregates a mixed skip+pass run as test-level PASS — see
   ffr_check_fault_acc_faces). */
static void ffr_hole_arm(struct ffr_hole *h, const struct PredOpsData *d)
{
    const size_t ps = d->page_bytes;
    void *pg = mmap(nullptr, 2 * ps, PROT_READ | PROT_WRITE,
                    MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (pg == MAP_FAILED || munmap((char *)pg + ps, ps) != 0) {
        report_fail_msg("sve512_pred_ops_arm: cannot arm the "
                        "faulting-LDFF1 page pair (errno=%d)", errno);
        return;   /* unreachable: report_fail_msg kills the thread */
    }
    h->pages = pg;
    h->page_bytes = ps;
    h->fault_k1 = (const double *)((char *)pg + ps - 8);
    double *tail = (double *)((char *)pg + ps - 8 * d->vl_d);
    for (size_t j = 0; j < d->vl_d; ++j)
        tail[j] = d->src[j];
}

/* Disarm: unmap ONLY page 1 — the one page this worker owns
   unconditionally (the round-4 #22 rule, now per-worker: no sibling can
   be mid-capture on this pair because nobody else ever held it). If the
   hole page is still unmapped, leaving it alone changes nothing; if a
   foreign mapper landed in it, munmapping the full range would destroy
   that mapping and SIGSEGV its owner far from the cause. A worker that
   died to report_fail leaves its pair mapped — the forked child exits
   right after the test and the kernel reclaims everything. */
static void ffr_hole_disarm_own(struct ffr_hole *h)
{
    if (h->pages)
        munmap(h->pages, h->page_bytes);
}

/* The whole LDFF1/FFR capture window as ONE small NOINLINE function —
 * the mechanical guard for the TWO invariants this test depends on
 * (round-4 #27/#30 switched this from always-inline to noinline; see
 * invariant 2 for why).
 *
 * Invariant 1, the zero-call window: everything from the SETFFR to the
 * FFR bit count landing in a GPR (and the loaded data in memory) lives
 * in this single template body, which itself contains no call, so the
 * window can never silently grow one: any future call has to be
 * inserted into this one small body, right under this comment, where
 * the constraint is stated (the body is one contiguous instruction run
 * at every -O level, exactly because it is a function of its own). The
 * window must stay call-free end to end:
 *
 *  - the arm64 Linux syscall ABI discards live FFR state at syscall entry
 *    (measured on a 6.6 kernel: FFR reads 0 after a syscall while Z/P are
 *    preserved; kernels carrying the newer fpsimd syscall-flush rework may
 *    flush P as well — the sve_flush_{z,p,ffr} macros all exist), and
 *  - a plain libc call can clobber it with no syscall at all: measured on
 *    this host's glibc, memcpy() zeroes the FFR (SVE-optimized callee;
 *    the AAPCS64 does not preserve FFR across calls) while this build's
 *    memcmp() happened to preserve it — per-call behavior is luck,
 *
 * so a call anywhere between the SETFFR and the RDFFR zeroes/clobbers the
 * FFR just the same as the Sanitizer CI run 40596064101 it fixed (count=0,
 * expected 2 on two workers; the guilty calls there sat after the LDFF1,
 * but any call landing in the window reproduces it). Closing the window
 * only at the scalar count would still leave the loaded Z data exposed on
 * a Z-flushing kernel, so the svst1 is inside the unit too. PrimeFFR=false
 * (accumulation face) additionally requires its capture to follow the
 * previous one with NOTHING but the two capture calls in between: the
 * partially-cleared FFR it observes is live state that a foreign call
 * would already have destroyed — that cross-capture discipline is
 * mechanized by the ffr_fault_acc_capture wrapper right below.
 *
 * Invariant 2, fresh RDFFR: GCC 12 does not model the FFR effects of
 * svldff1/svsetffr/svrdffr for optimization purposes (measured on
 * 12.3.1: an unused svldff1 is DCE'd outright, and an svrdffr result
 * gets CSE'd into a stale earlier value — the gem5 dual kernel measured
 * the same CSE even across calls, which is why its captures have been
 * noinline since round 3). A compiler that modeled svrdffr as a pure
 * read could therefore substitute an earlier capture's value for a
 * later one's, and the golden comparison would NOT catch it: the acc
 * face's golden equals the fault face's, and the mapped face expects
 * all-ones every iteration, so both substitutions are silent. The
 * noinline call boundary is the mechanical kill: the intrinsic is
 * spelled only inside this function, so there is no caller-level
 * expression to CSE, and this function writes memory through its
 * pointer arguments, so it is not pure — two calls can neither be
 * merged nor hoisted out of the re-loop. Every face's RDFFR executes,
 * on every toolchain that honors noinline.
 *
 * Count EVERY predicate bit, not just the per-element first bit: CNTP with
 * a b64 pattern tests only bit 0 of each 8-bit slice, so a stuck bit
 * anywhere in bits 1..7 of a slice kept the old count at vl and passed —
 * an SDC blind spot. The governing predicate must be b8 all-true as well:
 * CNTP activates an element only where BOTH predicates have their first
 * slice bit set, so a b64 governing predicate would mask bytes 1..7 and
 * keep the blind spot.
 *
 * BIT-FOR-BIT, not popcount: a count is an aggregate, and against a
 * PARTIAL golden (the faulting face expects exactly 8 set bits) two
 * complementary flips — one expected-1 bit dropping, one expected-0 bit
 * rising — conserve the count and pass (round-3 review #17). The capture
 * therefore also MATERIALIZES the FFR predicate into memory as a byte
 * image (byte i = 1 iff FFR bit i is set; svsel over b8 elements, so each
 * predicate bit governs exactly one image byte) and the checks memcmp the
 * image against a golden bitmap — any single-bit deviation, in either
 * direction, lands in exactly one byte. The count is still computed and
 * returned, but only as a diagnostic for the failure messages; the pass
 * condition is the image. The svsel/svst1_s8 pair is inside the window
 * (intrinsics, no call).
 *
 * PrimeFFR=true re-primes the FFR (SETFFR) before the load; PrimeFFR=false
 * leaves the FFR as-is so back-to-back captures observe accumulation.
 *
 * Lives inside the #ifdef __aarch64__ self-guard, like every other
 * arm64-only definition in this file and its siblings (sve512_fcmla_arm
 * et al.): the template body names (svsetffr, svfloat64_t, ...) are
 * non-dependent, so even a never-instantiated template is a HARD ERROR on
 * a compiler without arm_sve.h (measured: g++ -std=c++20 rejects it in
 * the definition itself — two-phase lookup does not wait for an
 * instantiation that never comes). The file's single-file x86 compile is
 * prevented at DECLARE_TEST by design (the registration references
 * guard-hidden functions, same as every test in this directory); the
 * load-bearing x86 defense for the directory is the meson host guard —
 * but this template was the one piece of arm64-only code sitting OUTSIDE
 * the file's own guard, so any future restructure that lifts the
 * DECLARE_TEST pattern would have tripped on it first. */
template <bool PrimeFFR>
static __attribute__((noinline)) long
ffr_ldff1_capture(const double *from, double *to, uint8_t *ffr_image)
{
    /* Zero-instruction compiler barrier — emits nothing, NOT a call (the
     * window below stays one contiguous call-free instruction run). Its
     * role is ORDERING, not CSE prevention, and that is all it is
     * claimed to do (round-4 #27/#30): a memory clobber constrains
     * memory accesses only — by GCC semantics it does not forbid
     * reusing a register-resident value computed earlier, so the
     * barrier alone cannot guarantee a fresh RDFFR. That guarantee is
     * the noinline call boundary of this function (invariant 2 in the
     * comment above): the caller never spells the intrinsic, and this
     * function writes memory through its pointer arguments so its calls
     * can be neither merged nor hoisted. The barrier keeps the window's
     * loads and stores from being reordered across the window start
     * under aggressive scheduling, and mirrors the gem5 dual kernel's
     * capture_img, which carries the same barrier inside its own
     * noinline body. Every value this unit loads is consumed (stored to
     * a buffer the caller memcmps), so the LDFF1 cannot be deleted. */
    __asm__ volatile ("" : : : "memory");
    if constexpr (PrimeFFR)
        svsetffr();
    svfloat64_t v = svldff1_f64(svptrue_b64(), from);
    svbool_t ffr = svrdffr();
    const long bits = svcntp_b8(svptrue_b8(), ffr);
    svst1_f64(svptrue_b64(), to, v);
    svst1_s8(svptrue_b8(), reinterpret_cast<int8_t *>(ffr_image),
             svsel(ffr, svdup_n_s8(1), svdup_n_s8(0)));
    return bits;
}

/* The CROSS-capture discipline as one unit. The accumulation face
 * observes the partially-cleared FFR the faulting capture just left —
 * live state that any FOREIGN call in between would destroy (measured:
 * this host's glibc memcpy zeroes the FFR; a syscall entry flushes it).
 * ffr_ldff1_capture's own noinline body closes the window WITHIN one
 * capture; this wrapper closes the gap BETWEEN the two captures: with
 * the captures noinline (round 4), the gap is exactly two adjacent
 * calls to this same leaf, and a bl to our own leaf preserves the FFR —
 * bl is not a syscall, the leaf is not libc, and neither writes the FFR
 * after its own RDFFR. The gem5 dual kernel's capture_pair has run this
 * exact two-call shape for 2000 iterations x 4 VL tiers, and every
 * native run of this test asserts the acc-face golden on real silicon.
 * The discipline: NOTHING but these two calls may ever sit in this
 * body — a future call inserted between them breaks the acc face
 * exactly like the Sanitizer-CI bug this PR fixed (before round 3 the
 * gap was comment-only discipline, structurally the same unprotected
 * shape as that bug). */
static inline __attribute__((always_inline)) void
ffr_fault_acc_capture(const double *fault_from, double *fault_to,
                      uint8_t *fault_image, const double *acc_from,
                      double *acc_to, uint8_t *acc_image,
                      long &fault_bits, long &acc_bits)
{
    fault_bits = ffr_ldff1_capture<true>(fault_from, fault_to, fault_image);
    acc_bits = ffr_ldff1_capture<false>(acc_from, acc_to, acc_image);
}

/* First differing FFR bit between a captured image and its golden —
 * diagnostics for the failure messages only (failure path, outside every
 * capture window; -1 = identical). */
static ptrdiff_t ffr_first_diff(const uint8_t *image, const uint8_t *golden,
                                size_t n)
{
    for (size_t i = 0; i < n; ++i)
        if (image[i] != golden[i])
            return (ptrdiff_t)i;
    return -1;
}

/* Failure-path-only discriminator for the munmap hole (see PredOpsData):
 * is the punched-out page mapped NOW? msync(MS_ASYNC) on an anonymous
 * mapping is a no-op flush, so success vs ENOMEM is exactly "mapped vs
 * unmapped" (measured on this host: hole -> -1/ENOMEM, mapped -> 0; the
 * page-aligned exactly-one-page probe shape has no other errno reachable
 * here — but a seccomp/LSM filter can turn any syscall into -1/EPERM, so
 * the third state exists and must be handled). THREE states, and the
 * verdict mapping in ffr_check_fault_acc_faces carries the detector's
 * bias (round-4 #23/#31): remapped is PROVEN interference (msync
 * succeeded on a page the test never mapped), intact is PROVEN
 * still-hole, and unknown proves NEITHER — only a proven re-map may
 * downgrade a failure to SKIP; an unknown probe is reported as a
 * possible SDC instead of being folded into the SKIP the old two-state
 * version silently chose. Runs only AFTER a fault/acc check has already
 * tripped, long after the capture completed — a syscall here cannot
 * touch the captured state.
 *
 * TOCTOU, stated honestly (round-4 #25): the probe observes the hole at
 * PROBE time, not at capture time, and userspace has no atomic
 * capture+probe. Both directions are real: forward (hole intact at
 * capture — the golden break is a true SDC — then a mapper lands in the
 * hole before the probe -> SKIP swallows a real SDC) and reverse (hole
 * occupied at capture — no fault, goldens broken — then the mapper
 * unmaps before the probe -> intact -> false SDC). Both are strictly
 * narrower than the flake class this probe exists to kill: each needs a
 * <=1-page anonymous mapper to land in (and for the reverse, fully
 * vacate) this exact page inside the few-hundred-instruction
 * capture-to-probe window, on a host where the residual-risk note above
 * already bounds the mapper itself as having no ordinary producer. A
 * re-capture-based convergence ("capture again and SKIP if it now
 * passes") was evaluated and REJECTED: a clean re-capture is equally
 * consistent with a genuine transient SDC, and an SDC detector must not
 * swallow an observed mismatch on a re-run. The SKIP message therefore
 * states the probe timing so a triager knows the classification
 * boundary.
 * gem5 boundary: the repo test never runs under gem5 SE (whose syscall
 * table maps msync to unimplementedFunc, i.e. fatal); the dual kernel
 * mirrors only the healthy-path goldens and does not exercise this. */
enum class ffr_hole_state { intact, remapped, unknown };

static ffr_hole_state ffr_hole_probe(const ffr_hole &h, int *probe_errno)
{
    const size_t ps = h.page_bytes;   // the hole: page 2 of the pair
    errno = 0;
    if (msync((char *)h.pages + ps, ps, MS_ASYNC) == 0)
        return ffr_hole_state::remapped;  // someone re-mapped the hole
    *probe_errno = errno;
    if (errno == ENOMEM)
        return ffr_hole_state::intact;    // ENOMEM == still unmapped
    return ffr_hole_state::unknown;       // seccomp/LSM/...: neither proven
}

/* Mapped-face verification — the SINGLE source of truth for both call
 * sites (round-4 #26/#32: Section 4 and the re-loop used to carry two
 * isomorphic hand copies of this check, and they had already drifted —
 * check order and message prefixes differed; the drift disease this PR
 * has removed three times now, #10, #19, here). Immediate=false:
 * Section 4 aggregate semantics — log_warning + all_passed=false,
 * execution continues so the remaining faces still run and the caller
 * issues the verdict. Immediate=true: re-loop semantics —
 * report_fail_msg kills the worker on the first broken face. Two
 * DISTINCT failure faces, reported separately so a message never
 * contradicts itself (a combined `||` once printed "ffr_bits=16,
 * expected 16" — equal numbers in an SDC claim — whenever only the
 * data half tripped): the image is the FFR predicate datapath
 * (bit-for-bit memcmp on the materialized bitmap, not popcount — a
 * bare count passes any count-conserving corruption), the readback is
 * the load datapath. <false> never returns nonzero (failures are
 * recorded in all_passed); <true> returns EXIT_FAILURE only as the
 * unreachable tail after the noreturn report. */
template <bool Immediate>
static int ffr_check_mapped_face(size_t vl, const uint8_t *m_gold,
                                 const uint8_t *m_img,
                                 const double *readback, const double *golden,
                                 long m_bits, bool &all_passed)
{
    const char *tag = Immediate ? " (re-loop)" : "";
    if (memcmp(m_img, m_gold, 8 * vl) != 0) {
        if constexpr (Immediate) {
            report_fail_msg("sve512_pred_ops_arm: FFR not all-true after "
                            "mapped LDFF1%s (count=%ld, expected %zu, first "
                            "wrong bit %ld)", tag, m_bits, 8 * vl,
                            ffr_first_diff(m_img, m_gold, 8 * vl));
            return EXIT_FAILURE;
        }
        log_warning("sve512_pred_ops_arm: FFR not all-true after mapped "
                    "LDFF1%s (count=%ld, expected %zu, first wrong bit %ld)",
                    tag, m_bits, 8 * vl,
                    ffr_first_diff(m_img, m_gold, 8 * vl));
        all_passed = false;
    }
    if (memcmp(readback, golden, vl * 8) != 0) {
        if constexpr (Immediate) {
            report_fail_msg("sve512_pred_ops_arm: mapped LDFF1 readback "
                            "mismatch%s (load datapath — distinct from the "
                            "FFR image face)", tag);
            return EXIT_FAILURE;
        }
        log_warning("sve512_pred_ops_arm: mapped LDFF1 readback mismatch%s "
                    "(load datapath — distinct from the FFR image face)",
                    tag);
        all_passed = false;
    }
    return EXIT_SUCCESS;
}

/* Faulting + accumulation face verification + the environment-
 * interference gate — the SINGLE source of truth for both call sites
 * (round-4 #26/#32: two isomorphic hand copies differing in message
 * text AND failure semantics; the mapped-face behavior inconsistency
 * round-4 #24 caught was this drift made real). The gate maps the hole
 * probe (ffr_hole_probe — see there for the TOCTOU boundary) to
 * verdicts with the detector's bias (round-3 #18, round-4 #23/#24/#25/
 * #31):
 *   - remapped hole AND no hole-independent failure pending: the three
 *     mismatches are fully explained by the environment (the fault
 *     never happened, ALL THREE goldens break at once) — honest
 *     TestResourceIssue SKIP.
 *   - remapped hole AND a hole-independent failure already on record
 *     (all_passed == false — the mapped face and sections 1-3 never
 *     touch the hole): SKIP is FORBIDDEN, it would swallow real SDC
 *     evidence (round-4 #24). The hole is logged as context; the SDC
 *     verdict takes precedence.
 *   - unknown errno: neither proven — a possible SDC is NOT downgraded
 *     into a SKIP; the mismatch is reported with the indeterminacy
 *     logged (round-4 #23/#31).
 * Immediate=false: Section 4 aggregate semantics (log_warning +
 * all_passed=false, all three faces checked, the caller issues the
 * verdict; returns EXIT_SUCCESS or EXIT_SKIP). Immediate=true: re-loop
 * semantics — reaching the gate there implies every earlier face of the
 * iteration already passed, so all_passed is true by construction;
 * report_fail_msg kills the worker on the first broken face. */
template <bool Immediate>
static int ffr_check_fault_acc_faces(const ffr_hole &hole, size_t vl,
                                     bool &all_passed,
                                     const uint8_t *f_gold,
                                     const uint8_t *f_img,
                                     const double *f_back,
                                     const double *f_exp,
                                     const uint8_t *a_img, long fault_bits,
                                     long acc_bits)
{
    const bool ffr_bad = memcmp(f_img, f_gold, 8 * vl) != 0;
    const bool data_bad = memcmp(f_back, f_exp, vl * 8) != 0;
    const bool acc_bad = memcmp(a_img, f_gold, 8 * vl) != 0;
    if (ffr_bad || data_bad || acc_bad) {
        int probe_errno = 0;
        const ffr_hole_state probe_state = ffr_hole_probe(hole, &probe_errno);
        if (probe_state == ffr_hole_state::remapped && all_passed) {
            log_skip(TestResourceIssueSkipCategory,
                     "sve512_pred_ops_arm: the unmapped hole for the "
                     "faulting-LDFF1 face was re-mapped by an external "
                     "mapper%s — fault source invalidated by the "
                     "environment, not an SDC (hole probed AFTER the "
                     "capture; capture-time state is unobservable — see "
                     "ffr_hole_probe)",
                     Immediate ? " (re-loop)" : "");
            return EXIT_SKIP;
        }
        if (probe_state == ffr_hole_state::unknown) {
            log_warning("sve512_pred_ops_arm: hole discriminator "
                        "indeterminate (msync errno=%d) — a re-mapped "
                        "hole cannot be proven, the mismatch is "
                        "reported as a possible SDC", probe_errno);
        } else if (!all_passed) {
            log_warning("sve512_pred_ops_arm: the hole was re-mapped, "
                        "but a hole-independent failure is already on "
                        "record — the SDC verdict takes precedence over "
                        "the environment SKIP");
        }
    }
    const char *tag = Immediate ? " (re-loop)" : "";
    if (ffr_bad) {
        if constexpr (Immediate) {
            report_fail_msg("sve512_pred_ops_arm: FFR partial-clear wrong "
                            "after faulting LDFF1%s (count=%ld, expected 8, "
                            "first wrong bit %ld — clear must start at the "
                            "first faulting lane)", tag, fault_bits,
                            ffr_first_diff(f_img, f_gold, 8 * vl));
            return EXIT_FAILURE;
        }
        log_warning("sve512_pred_ops_arm: FFR partial-clear wrong after "
                    "faulting LDFF1%s (count=%ld, expected 8, first wrong "
                    "bit %ld — clear must start at the first faulting "
                    "lane)", tag, fault_bits,
                    ffr_first_diff(f_img, f_gold, 8 * vl));
        all_passed = false;
    }
    if (data_bad) {
        if constexpr (Immediate) {
            report_fail_msg("sve512_pred_ops_arm: faulting LDFF1 data "
                            "wrong%s (count=%ld — prefix must load, tail "
                            "must zero from the first fault)", tag,
                            fault_bits);
            return EXIT_FAILURE;
        }
        log_warning("sve512_pred_ops_arm: faulting LDFF1 data wrong%s "
                    "(count=%ld — prefix must load, tail must zero from "
                    "the first fault)", tag, fault_bits);
        all_passed = false;
    }
    if (acc_bad) {
        if constexpr (Immediate) {
            report_fail_msg("sve512_pred_ops_arm: mapped LDFF1 touched the "
                            "FFR outside a fault%s (count=%ld, expected 8, "
                            "first wrong bit %ld — accumulation broken)",
                            tag, acc_bits,
                            ffr_first_diff(a_img, f_gold, 8 * vl));
            return EXIT_FAILURE;
        }
        log_warning("sve512_pred_ops_arm: mapped LDFF1 touched the FFR "
                    "outside a fault%s (count=%ld, expected 8, first wrong "
                    "bit %ld — accumulation broken)", tag, acc_bits,
                    ffr_first_diff(a_img, f_gold, 8 * vl));
        all_passed = false;
    }
    return EXIT_SUCCESS;
}

static int sve512_pred_ops_arm_init(struct test *test)
{
#ifndef __aarch64__
    (void)test;
    return EXIT_SUCCESS;
#else
    /* All-VL Phase 2 P6: capability + 128..1024-bit range check in one
       framework-layer call, replacing the getauxval hand-probe.
       arm64_require_sve_vl returns the VL in BYTES — f64 lanes =
       svl/8, range-checked below (release-effective, NDEBUG-proof). */
    const long svl = arm64_require_sve_vl(test, 128, 1024);
    if (svl < 0)
        return EXIT_SKIP;

    try {
        auto data = std::make_unique<PredOpsData>();
        data->vl_d = (size_t)svl / 8;   /* f64 lanes = SVL bytes / 8 */
        if (data->vl_d < 2 || data->vl_d > 16) {
            log_error("sve512_pred_ops_arm: f64 lanes %zu outside "
                      "[2,16] — unit drift (svl %ld bytes)",
                      data->vl_d, svl);
            return EXIT_FAILURE;
        }
        data->src.resize(data->vl_d);
        for (size_t j = 0; j < data->vl_d; ++j)
            data->src[j] = (double)(j % 4) - 1.5;   // exact values

        /* Page-size gate for the faulting-LDFF1 face (the per-worker
           ffr_hole_arm reuses this): 8*vl bytes of mapped tail must fit
           in one page (128 <= 4096 at the widest supported VL, but gate
           anyway: fail closed, not by arithmetic luck). Init-time SKIPs
           are deterministic and all-worker — the honest shape the
           framework supports (a per-worker mid-run SKIP aggregates into
           a test-level PASS when mixed with passes). */
        const long ps = sysconf(_SC_PAGESIZE);
        if (ps <= 0 || (size_t)ps < 8 * data->vl_d) {
            log_skip(TestResourceIssueSkipCategory,
                     "sve512_pred_ops_arm init: page size %ld unusable "
                     "for the faulting-LDFF1 face (need >= %zu)",
                     ps, 8 * data->vl_d);
            return EXIT_SKIP;
        }
        data->page_bytes = (size_t)ps;

        test->data = data.release();
        return EXIT_SUCCESS;
    } catch (const std::exception &e) {
        log_skip(TestResourceIssueSkipCategory,
                 "sve512_pred_ops_arm init: %s", e.what());
        return EXIT_SKIP;
    }
#endif
}

static int sve512_pred_ops_arm_run(struct test *test, int cpu)
{
    (void)cpu;
#ifndef __aarch64__
    (void)test;
    log_skip(CpuNotSupportedSkipCategory,
             "to be implemented (placeholder): aarch64 SVE required for "
             "sve512_pred_ops_arm");
    return EXIT_SKIP;
#else
    auto *d = static_cast<PredOpsData *>(test->data);

    /* All-VL policy #3: fail closed on init/run VL drift (src and the
       density-sweep buffers are sized in init at its VL). */
    if (svcntd() != d->vl_d) {
        log_error("sve512_pred_ops_arm: VL drifted between init (%zu "
                  "lanes) and run (%u lanes)", d->vl_d,
                  (unsigned)svcntd());
        report_fail_msg("sve512_pred_ops_arm: SVE VL drifted between "
                        "init and run");
    }

    const size_t vl = d->vl_d;
    const svbool_t all = svptrue_b64();
    bool all_passed = true;

    // Finite FMA addend for the active lanes: a distinctive, exactly
    // representable value so the computed lanes have a deterministic
    // golden. The masked-out lanes keep the NaN SENTINEL, which the
    // merging (_m) form must preserve bit-for-bit.
    const double BASE = 1.5;
    uint64_t base_bits;
    memcpy(&base_bits, &BASE, 8);

    // ---------------- Section 1+2: density sweep + inversion ----------------
    for (size_t k = 1; k <= vl; ++k) {
        // ---- section 1: whilelt(k) partial FMA ----
        svbool_t pg = svwhilelt_b64_u64((uint64_t)0, (uint64_t)k);
        // dst: active lanes carry BASE (the addend), the rest the SENTINEL.
        std::vector<uint64_t> dst(vl, SENTINEL);
        for (size_t i = 0; i < k; ++i)
            dst[i] = base_bits;
        svfloat64_t vdst = svld1_f64(all, reinterpret_cast<const double *>(dst.data()));
        svfloat64_t vsrc = svld1_f64(all, d->src.data());
        // ACLE _m: op1 + op2*op3, merging — acc = fma(src, src, BASE) on
        // the active lanes; inactive lanes retain the SENTINEL.
        vdst = svmla_f64_m(pg, vdst, vsrc, vsrc);
        svst1_f64(all, reinterpret_cast<double *>(dst.data()), vdst);
        for (size_t i = 0; i < vl; ++i) {
            if (i < k) {
                // golden: fma(src_i, src_i, BASE)
                double g = std::fma(d->src[i], d->src[i], BASE);
                uint64_t gw, aw;
                memcpy(&gw, &g, 8);
                memcpy(&aw, &dst[i], 8);
                if (gw != aw) {
                    log_warning("sve512_pred_ops: density %zu active lane "
                                "%zu golden=0x%016" PRIx64 " actual=0x%016"
                                PRIx64, k, i, gw, aw);
                    all_passed = false;
                }
            } else if (dst[i] != SENTINEL) {
                log_warning("sve512_pred_ops: density %zu INACTIVE lane "
                            "%zu sentinel clobbered: 0x%016" PRIx64,
                            k, i, dst[i]);
                all_passed = false;
            }
        }

        // ---- section 2: inverted predicate ----
        svbool_t pg_inv = svnot_b_z(all, pg);
        // active set is the complement [k, vl); inactive (i < k) = SENTINEL.
        std::vector<uint64_t> dst2(vl, SENTINEL);
        for (size_t i = k; i < vl; ++i)
            dst2[i] = base_bits;
        svfloat64_t w = svld1_f64(all, reinterpret_cast<const double *>(dst2.data()));
        w = svmla_f64_m(pg_inv, w, vsrc, vsrc);
        svst1_f64(all, reinterpret_cast<double *>(dst2.data()), w);
        for (size_t i = 0; i < vl; ++i) {
            if (i >= k) {   // complement: active iff NOT (i < k)
                double g = std::fma(d->src[i], d->src[i], BASE);
                uint64_t gw, aw;
                memcpy(&gw, &g, 8);
                memcpy(&aw, &dst2[i], 8);
                if (gw != aw) {
                    log_warning("sve512_pred_ops: inverted density %zu lane "
                                "%zu golden=0x%016" PRIx64 " actual=0x%016"
                                PRIx64, k, i, gw, aw);
                    all_passed = false;
                }
            } else if (dst2[i] != SENTINEL) {
                log_warning("sve512_pred_ops: inverted density %zu inactive "
                            "lane %zu written: 0x%016" PRIx64,
                            k, i, dst2[i]);
                all_passed = false;
            }
        }
    }

    // ---------------- Section 3: FADDA chain ----------------
    {
        double acc = 0.5;   // exact initial scalar
        svfloat64_t vsrc = svld1_f64(all, d->src.data());
        double hw = acc;
        for (size_t step = 0; step < FADDA_STEPS; ++step) {
            hw = svadda_f64(all, hw, vsrc);
        }
        // golden: FADDA adds active lanes left-to-right onto the scalar;
        // mirror exactly.
        double g = acc;
        for (size_t step = 0; step < FADDA_STEPS; ++step) {
            for (size_t i = 0; i < vl; ++i)
                g = g + d->src[i];   // values are exact halves: every sum
                                     // exact in f64, order fully defined
        }
        uint64_t gw, aw;
        memcpy(&gw, &g, 8);
        memcpy(&aw, &hw, 8);
        if (gw != aw) {
            log_warning("sve512_pred_ops: FADDA chain golden=0x%016" PRIx64
                        " actual=0x%016" PRIx64, gw, aw);
            all_passed = false;
        }
    }

    /* FFR face goldens + shared capture buffers for Section 4 AND the
       sustained re-loop below — hoisted to ONE construction site (round-3
       review #19: the fault-layout golden used to exist as two isomorphic
       hand-built copies, fexp/f_exp, with drifting fback/f_back names; a
       fault-layout change now edits this one place). Built outside every
       capture window — only intrinsics and memcmps run per face. One
       byte per FFR bit: 8*vl bytes, vl <= 16 (init gate) bounds every
       byte array at 128. m_gold: mapped face, every FFR bit set.
       f_gold: faulting AND accumulation faces — element 0's 8 bits set
       (the fault sits at element 1 = byte 8), the rest clear. ffr_mem
       carries the src pattern for the mapped/accumulation loads and
       doubles as their readback buffer; f_back/f_exp are the faulting
       face's data buffer and golden (prefix loads the last mapped
       double, tail zeroed at the fault). */
    uint8_t m_gold[128], f_gold[128];
    uint8_t m_img[128], f_img[128], a_img[128];
    std::vector<double> ffr_mem(vl), ffr_back(vl);
    double f_back[16], f_exp[16];   // vl <= 16 (init gate)
    for (size_t i = 0; i < 8 * vl; ++i) {
        m_gold[i] = 1;
        f_gold[i] = (i < 8) ? 1 : 0;
    }
    for (size_t i = 0; i < vl; ++i)
        ffr_mem[i] = d->src[i];
    f_exp[0] = d->src[vl - 1];
    for (size_t i = 1; i < vl; ++i)
        f_exp[i] = 0.0;

    /* This worker's own fault source (see ffr_hole): armed once here,
       used by Section 4 and the sustained re-loop below, disarmed at the
       end of the run. */
    ffr_hole hole;
    ffr_hole_arm(&hole, d);

    // ---------------- Section 4: LDFF1 + FFR + FMAXV ----------------
    {
        // valid mapped memory -> first-fault must not fault, FFR all-true.
        // Prime FFR to all-ones before the first-fault load (LDFF1 only
        // *clears* the FFR bit of each faulting element — it never sets a
        // bit — so without SETFFR the readback is the previous,
        // undefined FFR state, not the set of successfully-loaded lanes),
        // then capture through the single noinline capture function: the
        // call-free [SETFFR, image-and-data-in-memory] discipline, the b8
        // full-bit count, the bit-for-bit image materialization and the
        // call-boundary RDFFR-freshness guarantee all live in
        // ffr_ldff1_capture's one body. The readback buffers are
        // allocated BEFORE the window so no call can land between the
        // capture and the store even on a Z-flushing kernel.
        const long ffr_bits = ffr_ldff1_capture<true>(ffr_mem.data(),
                                                      ffr_back.data(),
                                                      m_img);
        /* mapped face through the single parameterized checker shared
           with the re-loop (round-4 #26/#32 — aggregate semantics here:
           failures are recorded in all_passed, execution continues) */
        (void)ffr_check_mapped_face<false>(vl, m_gold, m_img,
                                           ffr_back.data(), ffr_mem.data(),
                                           ffr_bits, all_passed);
        /* Faulting face: element 0 mapped, element 1 on the unmapped
           hole. The fault must be SUPPRESSED and converted into an FFR
           clear from lane 1 onward: expected count is exactly element
           0's 8 bits, the loaded prefix is the last mapped double, and
           every lane from the first faulting one on is zeroed (loads
           stop at the first fault). This is the only in-process way to
           drive FFR bits to an expected-0 state, so it is the ONLY face
           that can see a stuck-at-1 bit anywhere in [8, 8*vl) — the
           all-mapped face expects 1s everywhere and is blind to
           stuck-at-1 by construction. Honest boundary: a fault on the
           FIRST active element is architecturally delivered (SIGSEGV),
           and no lane precedes lane 0, so lane 0's 8 bits can never be
           driven to expected-0 by any first-faulting load — a stuck-at-1
           in exactly those bits is untestable by LDFF1 (hardware and
           gem5 agree on this rule). */
        /* Accumulation face: a mapped LDFF1 WITHOUT a SETFFR must leave
           the partially-cleared FFR untouched — LDFF1 never sets a bit
           and clears one only on a fault, so the count must still be 8
           and not snap back to all-ones (or to 0). Catches an FFR
           write-on-success defect the all-true face cannot see. Its
           capture follows the faulting one through the combined wrapper
           ffr_fault_acc_capture — two adjacent noinline capture calls
           and NOTHING else in between (see there: the cross-capture
           discipline is mechanical, not comment-only; the
           partially-cleared FFR is live state, and a foreign call —
           memcpy, measured — would clobber it). */
        long fault_bits, acc_bits;
        ffr_fault_acc_capture(hole.fault_k1, f_back, f_img, ffr_mem.data(),
                              ffr_back.data(), a_img, fault_bits, acc_bits);
        /* faulting + accumulation faces and the interference gate through
           the single parameterized checker (aggregate semantics: failures
           recorded in all_passed, the caller issues the verdict at the
           end; EXIT_SKIP = proven environment interference) */
        const int fr = ffr_check_fault_acc_faces<false>(
                hole, vl, all_passed, f_gold, f_img, f_back, f_exp, a_img,
                fault_bits, acc_bits);
        if (fr != EXIT_SUCCESS) {
            ffr_hole_disarm_own(&hole);
            return fr;
        }
        // FMAXV over the whilelt(k=vl/2) masked prefix: exact max
        {
            size_t k = vl / 2;
            svbool_t pg = svwhilelt_b64_u64(0, k);
            svfloat64_t vs = svld1_f64(all, d->src.data());
            double hw_max = svmaxv_f64(pg, vs);
            double g_max = -INFINITY;
            for (size_t i = 0; i < k; ++i)
                if (d->src[i] > g_max)
                    g_max = d->src[i];
            uint64_t gw, aw;
            memcpy(&gw, &g_max, 8);
            memcpy(&aw, &hw_max, 8);
            if (gw != aw) {
                log_warning("sve512_pred_ops: FMAXV golden=0x%016" PRIx64
                            " actual=0x%016" PRIx64, gw, aw);
                all_passed = false;
            }
        }
    }

    if (!all_passed) {
        report_fail_msg("sve512_pred_ops_arm: SVE predicate-network SDC "
                        "detected (whilelt/svnot/FADDA/FFR/FMAXV)");
        return EXIT_FAILURE;
    }

    // Sustained-stress re-loop of the FFR datapath, reusing the hoisted
    // goldens and buffers above (the loop body adds only SVE instructions
    // and memcmps per iteration; the FMA part below still allocates per
    // iteration, which is fine — the only call-free requirement is the
    // capture window). It drives all THREE FFR faces every iteration:
    // mapped (all-ones golden), faulting (partial-clear golden — the
    // suppressed fault also keeps the suppression path itself under
    // sustained stress), and accumulation (mapped load must leave the
    // partial FFR alone).
    while (test_time_condition(test)) {
        // light re-loop of the density sweep (the full sweep already ran)
        svbool_t pg = svwhilelt_b64_u64(0, vl - 1);
        svfloat64_t vsrc = svld1_f64(all, d->src.data());
        std::vector<uint64_t> z(vl, 0);
        svfloat64_t w = svld1_f64(all, reinterpret_cast<const double *>(z.data()));
        w = svmla_f64_m(pg, w, vsrc, vsrc);   // 0 + src*src on active lanes
        std::vector<uint64_t> out(vl, 0);
        svst1_f64(all, reinterpret_cast<double *>(out.data()), w);
        for (size_t i = 0; i + 1 < vl; ++i) {
            double g = std::fma(d->src[i], d->src[i], 0.0);
            uint64_t gw, aw;
            memcpy(&gw, &g, 8);
            memcpy(&aw, &out[i], 8);
            if (gw != aw) {
                report_fail_msg("sve512_pred_ops_arm: predicate FMA SDC "
                                "detected (re-loop)");
                return EXIT_FAILURE;
            }
        }

        // Re-sample the first-fault datapath every iteration — Section 4
        // above is the detailed one-shot pass, but the framework calls
        // test_run once per worker (sandstone_run.cpp), so without this
        // the LDFF1/FFR path gets a single sampling opportunity and a
        // marginal SDC that only shows under sustained stress escapes
        // (AGENTS.md: timed continuous stress, short-run pass proves
        // nothing). The capture goes through the same single noinline
        // capture function as Section 4 — the zero-call window and the
        // call-boundary RDFFR-freshness invariants are enforced in
        // ffr_ldff1_capture's one body, not re-maintained by hand here:
        // test_time_condition() above, the
        // FMA-part allocations, and the memcmp below all sit outside it.
        const long ffr_bits = ffr_ldff1_capture<true>(ffr_mem.data(),
                                                      ffr_back.data(), m_img);
        /* mapped face through the same single parameterized checker as
           Section 4 (immediate semantics: the first broken face kills
           the worker; the two DISTINCT failure faces and their
           non-self-contradicting messages live in that one body) */
        const int mr = ffr_check_mapped_face<true>(vl, m_gold, m_img,
                                                   ffr_back.data(),
                                                   ffr_mem.data(), ffr_bits,
                                                   all_passed);
        if (mr != EXIT_SUCCESS)
            return mr;
        /* faulting + accumulation captures as the combined wrapper —
           two adjacent noinline capture calls and nothing else in
           between (the cross-capture discipline is mechanical; the
           partially-cleared FFR is live state a foreign call, memcpy
           measured, would clobber — see ffr_fault_acc_capture) */
        long fault_bits, acc_bits;
        ffr_fault_acc_capture(hole.fault_k1, f_back, f_img, ffr_mem.data(),
                              ffr_back.data(), a_img, fault_bits, acc_bits);
        /* faulting + accumulation faces + the interference gate through
           the single parameterized checker (immediate semantics);
           reaching here means the FMA and mapped faces of this iteration
           already passed, so no hole-independent failure can be pending */
        const int fr = ffr_check_fault_acc_faces<true>(
                hole, vl, all_passed, f_gold, f_img, f_back, f_exp, a_img,
                fault_bits, acc_bits);
        if (fr != EXIT_SUCCESS) {
            ffr_hole_disarm_own(&hole);
            return fr;
        }
    }

    ffr_hole_disarm_own(&hole);
    return EXIT_SUCCESS;
#endif
}

static int sve512_pred_ops_arm_cleanup(struct test *test)
{
#ifdef __aarch64__
    /* The per-worker page pairs live and die inside test_run
       (ffr_hole_arm / ffr_hole_disarm_own) — round-5 moved the fault
       source out of this shared struct, so cleanup only releases the
       init-time data. A worker that died to report_fail leaves its pair
       to process exit (the forked child exits right after the test). */
    delete static_cast<PredOpsData *>(test->data);
#else
    (void)test;
#endif
    return EXIT_SUCCESS;
}

#endif // __aarch64__

DECLARE_TEST(sve512_pred_ops_arm,
             "SVE predicate-dimension SDC stress: whilelt density sweep "
             "(1..VL active lanes, sentinel-guarded inactive lanes), "
             "predicate inversion (svnot_b_z complement FMA), FADDA "
             "sequential reduction chain, LDFF1+FFR first-fault state, "
             "FMAXV masked-tree reduction — the predicate network every "
             "full-true SVE test leaves untested")
    .groups = DECLARE_TEST_GROUPS(&group_math),
    .test_init = sve512_pred_ops_arm_init,
    .test_run = sve512_pred_ops_arm_run,
    .test_cleanup = sve512_pred_ops_arm_cleanup,
    .quality_level = TEST_QUALITY_PROD,
    .arm64_requires = ARM64_REQUIRES_SVE,
END_DECLARE_TEST

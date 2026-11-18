/*
 * Copyright 2022 Intel Corporation.
 * SPDX-License-Identifier: Apache-2.0
 */

/*
 * Minimal page-size helpers -- the single canonical home of ROUND_UP_TO,
 * sandstone_page_size() and ROUND_UP_TO_PAGE.
 *
 * Included by both sandstone_p.h (framework proper) and
 * sandstone_test_utils.h (unit-tested constructs), so the implementation
 * the unit tests exercise is byte-for-byte the one the framework's
 * mmap/mprotect users run.  Keep this header usable from C and C++ and
 * free of any include other than <unistd.h>, <stdio.h> and <stdlib.h>
 * (the fatal helper reports straight to stderr -- logging may not be
 * initialised yet where the page size is first needed; on Windows
 * <unistd.h> resolves to the sysdeps/windows shim, whose getpagesize()
 * is backed by GetSystemInfo).
 */

#ifndef INC_SANDSTONE_PAGE_H
#define INC_SANDSTONE_PAGE_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>

/* align to a power of 2.
 * The mask must be computed in size_t: with a 32-bit n (sandstone_page_size()
 * returns unsigned) ~((n) - 1) is a 32-bit mask that silently drops bits
 * >= 2^32, so ROUND_UP_TO_PAGE on a >=4 GiB size returned only its low
 * 32 bits (probe-verified 2026-11-18, ps=65536: 4 GiB -> 0, 4 GiB+1 -> ps,
 * 4.14 GiB -> 143.4 MiB).  Honesty note: this truncation was ONE candidate
 * mechanism for the 2026-11-11 stderr-dump SIGSEGV (a >4 GiB memfd would
 * have been mmap'ed as ~143 MiB and the scan would run off the mapping),
 * but the crash-scene numbers recorded at the time (size 4294261107 < 2^32,
 * mapping ending at data+58 KiB, see
 * docs/superpowers/plans/2026-11-12-memcpy1-neon-stderr-fix.md) are NOT
 * explained by it — the old mask rounds that size up to 4 GiB, not 58 KiB.
 * The coredump that would have settled the conflict is gone, and the dump
 * path no longer mmaps at all (print_child_stderr_common preads the
 * clamped head), so it is robust under either scene.  The fix stands on
 * its own probe- and unit-test-verified merits (RoundUpToPagePast4GiB). */
#define ROUND_UP_TO(value, n)       (((value) + (n) - 1) & ~((size_t)(n) - 1))

/*
 * Page size used for mmap/mprotect alignment.  MUST match the system's actual
 * page size, otherwise mprotect() will round to the kernel's page boundary and
 * may make adjacent data read-only (causing SIGSEGV on kernels with >4 KB
 * pages, e.g. ARM64 16 KB / 64 KB configurations).  Resolve once via
 * sysconf(_SC_PAGESIZE) (getpagesize() through the sysdeps/windows shim on
 * Windows) and cache it.  The cache is an atomic lazy-init: per-CPU worker
 * threads race on the first call (issues #446/#589), so loads/stores go
 * through __atomic_ builtins on BOTH branches and neither caches an
 * unvalidated value.
 */
static inline bool sandstone_is_valid_page_size(long page_size)
{
    return page_size > 0 && (page_size & (page_size - 1)) == 0;
}

/* There is no safe fallback page size (a hardcoded one breaks kernels with
 * bigger pages, see the comment above) and logging may not be initialized
 * yet, so write the diagnosis to stderr directly and abort — like
 * malloc.cpp's null_pointer_consumption().  Fail loudly, never cache garbage. */
static __attribute__((unused, noinline, noreturn)) void sandstone_page_size_fatal(long page_size, const char *source)
{
    fprintf(stderr, "internal error: unusable page size from %s: 0x%lx\n",
            source, (unsigned long)page_size);
    abort();
}

#ifdef __cplusplus
extern "C++"
{
#endif
static inline unsigned sandstone_page_size(void)
{
    static long cached = 0;
    long page_size = __atomic_load_n(&cached, __ATOMIC_ACQUIRE);
    if (__builtin_expect(!page_size, false)) {
        const char *source;
#ifdef _WIN32
        source = "getpagesize()";             /* sysdeps/windows/unistd.h shim */
        page_size = getpagesize();
#else
        source = "sysconf(_SC_PAGESIZE)";
        page_size = sysconf(_SC_PAGESIZE);
#endif
        if (__builtin_expect(!sandstone_is_valid_page_size(page_size), false))
            sandstone_page_size_fatal(page_size, source);
        __atomic_store_n(&cached, page_size, __ATOMIC_RELEASE);
    }
    return (unsigned)page_size;
}
#ifdef __cplusplus
}
#endif

#define ROUND_UP_TO_PAGE(value)     ROUND_UP_TO(value, sandstone_page_size())

#endif /* INC_SANDSTONE_PAGE_H */

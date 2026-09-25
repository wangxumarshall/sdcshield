# Copyright 2022 Intel Corporation.
# SPDX-License-Identifier: Apache-2.0
#
# arithmetic_arm: tests that exercise ARM-optimised third-party libraries
# (GMP / Arm Compute Library / compiler-rt) to catch silent data corruption
# in the integer, floating-point and memory subsystems on ARM64 (Kunpeng 920).
#
# These tests are ARM-native: GMP, ACL and compiler-rt soft-float builtins are
# consumed on aarch64, and the whole subdir is entered only under the aarch64
# guard in tests/cpu/meson.build (see subdir('arithmetic_arm') there). On x86-64
# the sources are not compiled at all, so x86 behaviour is untouched.
#
# Dependency availability on this host (verified):
#   * GMP  .................. system package (/usr/include/gmp.h, /usr/lib64/libgmp.so)
#   * Arm Compute Library ... vendored source build, third-party/acl (v23.02,
#                             build.sh → install/lib/libarm_compute-core.a); no
#                             host libarm_compute.so is used or required.
#   * compiler-rt builtins .. /usr/lib/clang/17/lib/aarch64-openEuler-linux-gnu/
#                             libclang_rt.builtins.a (soft-float __addsf3 et al.)
#
# The Arm Optimized Routines (libmathlib.a / libstringlib.a) static archives are
# NOT available on this host (only the .so versions ship, and the reference port
# links them statically to avoid a runtime GLIBC_2.38 dependency). The 3 AOR
# tests (aor_math.cpp, aor_string.cpp, cbw_arm.cpp) are therefore excluded
# below — they are listed as comments and not built, so there is no link error.
#
# Test IDs use an `_arm` suffix (adcx_arm, adox_arm, adcxlong_arm,
# adcx_adox_interleaved_arm) and are DISTINCT from the already-ported
# tests/cpu/arithmetic IDs (adcx, adox, adcxlong, adcx_adox_interleaved — no
# suffix, which use __int128). The _arm variants use GMP as the golden
# reference; both sets coexist without ID clash.

arm_cpp_flags = [
    '-march=armv8.1-a+crc+crypto',
]

# ---------------------------------------------------------------------------
# GMP: system package. Prefer pkg-config, fall back to find_library.
# ---------------------------------------------------------------------------
gmp_dep = dependency('gmp', static : dep_static, required : false)
if not gmp_dep.found()
    gmp_lib = cc.find_library('gmp', required : false, static : dep_static)
    gmp_dep = declare_dependency(
        compile_args : ['-isystem', '/usr/include'],
        dependencies : [gmp_lib],
    )
endif

# ---------------------------------------------------------------------------
# Arm Compute Library: VENDORED source build (third-party/acl, Arm Compute
# Library v23.02 built by its build.sh — first official pure-CMake release;
# requires gcc >= 10.2 which 24.03 (12.3), 22.03 (10.3) and 20.03
# (gcc-toolset-10 via supplement-20.03-gcc10.sh) all satisfy). This replaces
# the old scheme of linking the host's /usr/lib64/libarm_compute.so against a
# separate header tree via -Dacl_incdir, which was ABI-broken on 22.03/20.03
# (v20.02 runtime vs v22.11 headers, GLIBCXX mismatch) and disabled there.
# The vendored build produces ONLY the arm_compute_core target (NEON runtime:
# NEGEMM/NECast/Tensor — src/CMakeLists.txt:879/904/960); the sve/sve2/graph
# targets are neither built nor linked, so no SVE machine code enters the
# binary and non-SVE target machines stay safe.
# Probe pattern identical to openssl/openblas/sleef: prefer
# third-party/acl/install (built by build.sh); when absent under enable_acl=auto
# the 2 ACL tests are simply not built (acl_sources empty, acl_dep a no-op
# declare_dependency()); enable_acl=enabled makes a missing install/ a hard
# error instead of a silent degrade.
# ---------------------------------------------------------------------------
acl_enable = get_option('enable_acl')
acl_install = meson.current_source_dir() / '../../../third-party/acl/install'
acl_sources = []
acl_dep = declare_dependency()
if acl_enable != 'disabled'
  if run_command('test', '-f', acl_install / 'lib' / 'libarm_compute-core.a', check : false).returncode() == 0
    acl_dep = declare_dependency(
        include_directories : include_directories('../../../third-party/acl/install/include', is_system : true),
        link_args : ['-L' + acl_install / 'lib', '-l:libarm_compute-core.a'],
    )
    message('Using vendored ACL (third-party/acl/install)')
    acl_sources = files(
        'acl_gemm.cpp',
        'fisttp_arm.cpp',
    )
  elif acl_enable == 'enabled'
    error('enable_acl=enabled but third-party/acl/install/lib/libarm_compute-core.a not found — run third-party/acl/build.sh first')
  else
    message('ACL install/ not found under third-party/acl — run its build.sh to enable the 2 ACL tests.')
  endif
endif

# ---------------------------------------------------------------------------
# compiler-rt builtins: static soft-float integer implementation (__addsf3,
# __subsf3, __mulsf3, __divsf3 ...). -Wl,--whole-archive prevents the linker
# from dropping the soft-float symbols as "unused".
# Probe-gated: the archive only exists where clang-rt is installed (this
# openEuler 24.03 host). In the 22.03/20.03 offline build containers the path
# does not exist and the unconditional link_args broke the final link
# ("c++: error: .../libclang_rt.builtins.a: No such file or directory").
# When absent, crt_sources is empty and crt_tests_lib is an empty static lib
# (same graceful-degrade pattern as acl_sources/openblas/sleef).
# ---------------------------------------------------------------------------
crt_builtins_path = '/usr/lib/clang/17/lib/aarch64-openEuler-linux-gnu/libclang_rt.builtins.a'
if run_command('test', '-f', crt_builtins_path, check: false).returncode() == 0
    crt_builtins_lib = declare_dependency(
        link_args : ['-Wl,--whole-archive',
                      crt_builtins_path,
                      '-Wl,--no-whole-archive'],
    )
    crt_sources = files('crt_builtins.cpp',)
else
    crt_builtins_lib = declare_dependency()
    crt_sources = []
    message('libclang_rt.builtins.a not found at ' + crt_builtins_path +
            ' — the crt_builtins test is not built on this host.')
endif

# ---------------------------------------------------------------------------
# Source grouping. The 10 dep-available tests (7 GMP + 2 ACL + 1 compiler-rt)
# are grouped by library so each static_library pulls only the deps it needs.
# The 3 AOR tests are intentionally excluded (see header comment).
# ---------------------------------------------------------------------------

# 8 GMP-backed tests: adcx_arm, adox_arm, adcxlong_arm, adcx_adox_interleaved_arm,
# bigint_mulx_arm, gmp_bigadd, gmp_bignum, operand_space_arm.
# operand_space_arm is the operand-space SDC stressor: it feeds the ARM64
# adder/carry-chain/multiplier a deterministic high-Hamming-distance operand
# table (0x0000...<->0xFFFF... alternating, walking-one) instead of uniform
# random, to force maximum per-cycle gate toggling (the "版图一.1 操作数空间"
# coverage dimension that uniform-random operands miss).
gmp_sources = files(
    'adcx_arm.cpp',
    'adcxlong_arm.cpp',
    'adox_arm.cpp',
    'adcx_adox_interleaved_arm.cpp',
    'bigint_mulx_arm.cpp',
    'gmp_bigadd.cpp',
    'gmp_bignum.cpp',
    'operand_space_arm.cpp',
)

# 2 ACL-backed tests: acl_gemm (NEON GEMM), fisttp_arm (FCVTZS cast). Both are
# REAL tests: the historical acl_gemm SIGSEGV was root-caused to a missing
# `test->data = data` assignment in init (never a fork-safety issue); it is
# fixed and acl_gemm runs NEGEMM against a long-double naive golden with a
# tight relative tolerance.
# acl_sources is set above (empty when -Denable_acl=disabled or ACL not found).

# 1 compiler-rt test: soft-float __addsf3/__mulsf3/__divsf3 vs golden.
# (crt_sources is set in the probe-gated block above — empty when the
# clang-rt archive is absent on this build host.)

# Excluded — AOR libmathlib.a / libstringlib.a static archives are not present
# on this host, so these 3 tests would fail to link:
#   'aor_math.cpp',
#   'aor_string.cpp',
#   'cbw_arm.cpp',

# ---------------------------------------------------------------------------
# Per-library static libraries. Each is added to sandstone_tests so the test
# objects are linked into the final sdcshield binary. build_by_default:false
# keeps the objects out of the default ninja target until the main binary pulls
# them in (matches the pattern used by tests_crc / tests_arm64).
# ---------------------------------------------------------------------------
gmp_tests_lib = static_library(
    'tests_arithmetic_arm_gmp',
    gmp_sources,
    build_by_default: false,
    include_directories: [
        framework_incdir,
        tests_common_incdirs,
    ],
    dependencies: [
        boost_dep,
        gmp_dep,
    ],
    cpp_args: [
        debug_c_flags,
        default_cpp_flags,
        default_cpp_warn,
        march_generic_flags,
        arm_cpp_flags,
    ],
)

acl_tests_lib = static_library(
    'tests_arithmetic_arm_acl',
    acl_sources,
    build_by_default: false,
    include_directories: [
        framework_incdir,
        tests_common_incdirs,
    ],
    dependencies: [
        boost_dep,
        acl_dep,
    ],
    cpp_args: [
        debug_c_flags,
        default_cpp_flags,
        default_cpp_warn,
        march_generic_flags,
        arm_cpp_flags,
    ],
)

crt_tests_lib = static_library(
    'tests_arithmetic_arm_crt',
    crt_sources,
    build_by_default: false,
    include_directories: [
        framework_incdir,
        tests_common_incdirs,
    ],
    dependencies: [
        boost_dep,
        crt_builtins_lib,
    ],
    cpp_args: [
        debug_c_flags,
        default_cpp_flags,
        default_cpp_warn,
        march_generic_flags,
        arm_cpp_flags,
    ],
)

sandstone_tests += [
    gmp_tests_lib,
    acl_tests_lib,
    crt_tests_lib,
]

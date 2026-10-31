#!/bin/bash
# Run a binary in gem5 SE mode with a specific SVE vector length.
# Usage: ./run_sve.sh <binary> [args...] --vl <quadwords>
#   1 quadword = 128-bit, 2 = 256-bit, 4 = 512-bit. REQUIRED: se_sve.py
#   refuses to run without it (the VL tier is the verification's identity).
#
# The runtime VL must match the -msve-vector-bits the binary was compiled
# with: size-specific SVE code (arm_sve_vector_bits(N) types) is only
# guaranteed correct when the process vector length equals N.
set -euo pipefail
# Default: the verified CHAOS gem5.opt on this host (md5
# 3f9011f40dbe80ec1df478c62ddb0b06 — the binary the SVE 全 VL campaign
# gem5 legs ran with); override with G5=/path/to/gem5.opt.
G5=${G5:-/home/sdc/wangxu/gem5-fi/CHAOS/gem5/build/ARM/gem5.opt}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CFG="$SCRIPT_DIR/se_sve.py"

# gem5 needs the user-local python + zlib (see README.md)
export PATH="$HOME/.local/bin:$PATH"
export C_INCLUDE_PATH=$HOME/.local/zlib/include
export CPLUS_INCLUDE_PATH=$HOME/.local/zlib/include
export LIBRARY_PATH=$HOME/.local/zlib/lib

# Runtime visibility (PR #307 #27): the default G5 is the CHAOS copy-back
# path — exactly the spot the README TRAP note warns goes STALE after a
# `scons -C` rebuild (the artifact lands in the repo-root build/). Echo the
# resolved binary + its md5 so every run (and every per-test batch log)
# records WHICH gem5 actually executed; compare against the campaign binary
# 3f9011f40dbe80ec1df478c62ddb0b06 before trusting tier evidence.
g5md5=$(md5sum "$G5" 2>/dev/null | cut -d' ' -f1 || true)
echo "run_sve.sh: G5=$G5  md5=${g5md5:-unavailable}  cfg=$CFG" >&2

exec "$G5" "$CFG" "$@"

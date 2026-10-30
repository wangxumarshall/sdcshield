#!/bin/bash
# Run a binary in gem5 SE mode with a specific SVE vector length.
# Usage: ./run_sve.sh <binary> [args...] --vl <quadwords>
#   1 quadword = 128-bit, 2 = 256-bit, 4 = 512-bit.
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

exec "$G5" "$CFG" "$@"

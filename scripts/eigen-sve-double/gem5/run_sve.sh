#!/bin/bash
# Run a binary in gem5 SE mode with a specific SVE vector length.
# Usage: ./run_sve.sh <binary> [args...] --vl <quadwords>
#   1 quadword = 128-bit, 2 = 256-bit, 4 = 512-bit.
#
# The runtime VL must match the -msve-vector-bits the binary was compiled
# with: size-specific SVE code (arm_sve_vector_bits(N) types) is only
# guaranteed correct when the process vector length equals N.
set -euo pipefail
G5=${G5:-/home/sdc/wangxu/gem5-fi-fuzz/build/ARM/gem5.opt}
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
CFG="$SCRIPT_DIR/se_sve.py"

# gem5 needs the user-local python + zlib (see README.md)
export PATH="$HOME/.local/bin:$PATH"
export C_INCLUDE_PATH=$HOME/.local/zlib/include
export CPLUS_INCLUDE_PATH=$HOME/.local/zlib/include
export LIBRARY_PATH=$HOME/.local/zlib/lib

exec "$G5" "$CFG" "$@"

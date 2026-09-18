# gem5 SE-mode validation for the SVE double/complex Eigen packets

The host (cortex x3b) has a 256-bit SVE vector length, so binaries compiled
with `-msve-vector-bits=512` cannot run natively (size-specific SVE code
requires the runtime VL to equal the compile-time VL). Those binaries are
validated functionally in gem5 syscall-emulation mode instead.

## One-time setup (this host, no-root environment)

gem5 needs a Python with headers and a zlib with headers; the host has
neither devel package. Both were built user-local:

```console
# Python 3.11.6 (headers + static libpython + zlib module) -> ~/.local/python311
curl -LO https://mirrors.aliyun.com/python-release/source/Python-3.11.6.tgz
tar xzf Python-3.11.6.tgz && cd Python-3.11.6
./configure --prefix=$HOME/.local/python311 --with-ensurepip=no
make -j32 && make install          # C_INCLUDE_PATH=$HOME/.local/zlib/include
                                   # LIBRARY_PATH=$HOME/.local/zlib/lib

# zlib 1.3.1 (headers + libs) -> ~/.local/zlib
curl -LO https://zlib.net/fossils/zlib-1.3.1.tar.gz
tar xzf zlib-1.3.1.tar.gz && cd zlib-1.3.1
./configure --prefix=$HOME/.local/zlib && make && make install
ln -sf ~/.local/zlib/lib/libz.so.1.3.1 ~/.local/zlib/lib/libz.so

# python3-config shim (gem5's ParseConfig calls it with BOTH --ldflags and
# --includes in one invocation; it must also yield -lpython and -lz):
cat > ~/.local/bin/python3-config <<'EOF'
#!/bin/sh
PY=$HOME/.local/python311
OUT=""
for a in "$@"; do
  case "$a" in
    --includes) OUT="$OUT -I$PY/include/python3.11" ;;
    --cflags)   OUT="$OUT -I$PY/include/python3.11" ;;
    --ldflags)  OUT="$OUT -L$PY/lib -lpython3.11 -lz -lm -ldl" ;;
    --libs)     OUT="$OUT -L$PY/lib -lpython3.11 -lz -lm -ldl" ;;
  esac
done
echo $OUT
EOF
chmod +x ~/.local/bin/python3-config
pip3 install --user -i https://mirrors.aliyun.com/pypi/simple/ scons

# Build gem5.opt from the vendored CHAOS tree (note: the artifact lands in
# the REPO ROOT build/, not CHAOS/gem5/build/ — verified again here):
cd /home/sdc/wangxu/gem5-fi-fuzz
export PATH="$HOME/.local/bin:$PATH" \
       C_INCLUDE_PATH=$HOME/.local/zlib/include \
       CPLUS_INCLUDE_PATH=$HOME/.local/zlib/include \
       LIBRARY_PATH=$HOME/.local/zlib/lib
scons -C CHAOS/gem5 build/ARM/gem5.opt -j32     # ~25 min at -j32 on 127 cores
# -> /home/sdc/wangxu/gem5-fi-fuzz/build/ARM/gem5.opt (1.1 GB)
```

Pitfalls hit and solved (all reproducible from the log above):

- **Python.h check failed** until `python3-config --cflags` emitted the
  include path AND the shim handled `--ldflags --includes` in one call.
- **`import zlib` abort inside gem5py**: the first python311 build silently
  skipped the zlib extension module (no system zlib headers). Rebuilt with
  the user-local zlib on `C_INCLUDE_PATH`; the extension then links 1.3.1.
- **-static binaries impossible**: no `libstdc++.a` on the host; the
  dynamically-linked binaries work fine in gem5 SE mode as-is.

## Running a size-specific SVE binary

```console
./run_sve.sh <binary> [args...] --vl <quadwords>
# --vl 1 = 128-bit, 2 = 256-bit, 4 = 512-bit
```

`se_sve.py` is a ~70-line canonical SE config (AtomicSimpleCPU, 2 GiB
SimpleMemory, ArmISA with `sve_vl_se=<quadwords>`). `sve_vl_se` is the
SE-mode-only parameter that sets the emulated vector length
(`src/arch/arm/ArmISA.py`; the default `ArmDefaultSERelease` already enables
FEAT_SVE/FEAT_F64MM, so no other ISA flags are needed).

## Verification matrix (2026-09-19, gem5 25.1.0.1)

| Test binary (VL=512, 8 f64 lanes) | gem5 result | Simulated exit |
|---|---|---|
| `vl_probe_512` | `VL=512 bits, 8 f64 lanes` | 0 |
| `test_packet_xd_512` | ALL PASS (8 lanes) | 0 |
| `test_packet_xcd_512` | ALL PASS (4 complex lanes) | 0 |
| `test_math_xd_512` | ALL PASS (8 f64 lanes) | 0 |
| `test_e2e_512 --mini` | E2E ALL PASS (complex BDCSVD rel err 4.99e-15) | 0 |

Cross-validation at VL=256 (hardware-native): `test_packet_xd/xcd/math_xd`
compiled with `-msve-vector-bits=256` and run in gem5 with `--vl 2` give
ALL PASS — identical to the hardware results, so the simulator and the
silicon agree on the packet semantics.

Build the test binaries:

```console
for T in test_packet_xd test_packet_xcd test_math_xd test_e2e_xd; do
  g++ -O2 -std=c++17 -Ithird-party/eigen5 -march=armv8.2-a+sve \
      -msve-vector-bits=512 -DEIGEN_ARM64_USE_SVE \
      scripts/eigen-sve-double/$T.cpp -o /tmp/gem5_bins/${T}_512
done
```

Note: gem5 validation is **functional** (ISA semantics, result correctness);
it is not a performance measurement. The 60 s performance gate was validated
on hardware at VL=128/256 (Task 4 of the plan).

// Standalone correctness test for Eigen SVE double math functions
// (arch/SVE/MathFunctions.h: pround/print) and double<->int32 type casting
// (arch/SVE/TypeCasting.h: type_casting_traits, pcast, preinterpret).
//
// Semantics (hardware-probe verified twice, controller ruling):
//   pround = std::round      (half away from zero) -> svrinta_f64_x
//   print  = std::nearbyint  (half to even)        -> svrintn_f64_x
// The original brief asserted nearbyint semantics for pround; corrected per
// controller ruling (std::round for pround, separate print check added).
//
// Build at VL=256 (host). Must also compile at VL=128/512; VL=128 runs on
// this host via the /tmp/vlrun pinning launcher (prctl PR_SVE_SET_VL before
// exec). psqrt/pfloor/pceil live in PacketMath.h since Task 1; psqrt is
// covered here per the brief (its coverage was deferred from Task 1).
#include <Eigen/Core>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstring>

using namespace Eigen;
using namespace Eigen::internal;

static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else printf("ok: %s\n", what);
}

int main() {
  const int N = packet_traits<double>::size;            // f64 lanes = VL/64
  const int NI = packet_traits<numext::int32_t>::size;  // i32 lanes = VL/32
  printf("PacketXd size=%d, PacketXi size=%d (VL=%d bits) backend=%s\n",
         N, NI, EIGEN_ARM64_SVE_VL, Eigen::SimdInstructionSetsInUse());
  check(N == EIGEN_ARM64_SVE_VL / 64, "f64 packet size == VL/64");
  check(NI == 2 * N, "i32 packet size == 2*f64 (cross-width pair)");

  alignas(64) double r[16];

  // --- psqrt (in PacketMath.h since Task 1; covered here per the brief)
  alignas(64) double nn[16];
  for (int i = 0; i < 16; i++) nn[i] = 0.25 * i + 0.5;  // all positive
  pstore<double>(r, psqrt<PacketXd>(pload<PacketXd>(nn)));
  for (int i = 0; i < N; i++) check(r[i] == std::sqrt(nn[i]), "psqrt");

  // Half-way values distinguish round-half-away (2.5->3) from half-to-even
  // (2.5->2) on both signs. Lanes >= 4 are exact integers. At VL=128 (N=2)
  // only lanes 0..1 exist, so the explicit checks are lane-guarded.
  alignas(64) double hz[16] = {1.5, 2.5, -1.5, -2.5};
  for (int i = 4; i < 16; i++) hz[i] = double(i);
  PacketXd vh = pload<PacketXd>(hz);

  // --- pround: std::round semantics (half AWAY from zero) -> svrinta
  pstore<double>(r, pround<PacketXd>(vh));
  check(r[0] == 2.0, "pround 1.5 -> 2");
  if (N >= 2) check(r[1] == 3.0, "pround 2.5 -> 3 (half away from zero)");
  if (N >= 3) check(r[2] == -2.0, "pround -1.5 -> -2");
  if (N >= 4) check(r[3] == -3.0, "pround -2.5 -> -3 (half away, negative)");
  for (int i = 0; i < N; i++) check(r[i] == std::round(hz[i]), "pround == std::round");

  // --- print: nearbyint semantics (half to EVEN) -> svrintn
  pstore<double>(r, print<PacketXd>(vh));
  check(r[0] == 2.0, "print 1.5 -> 2");
  if (N >= 2) check(r[1] == 2.0, "print 2.5 -> 2 (half to even)");
  if (N >= 3) check(r[2] == -2.0, "print -1.5 -> -2");
  if (N >= 4) check(r[3] == -2.0, "print -2.5 -> -2 (half to even, negative)");
  for (int i = 0; i < N; i++) check(r[i] == std::nearbyint(hz[i]), "print == std::nearbyint");

  // --- pcast double -> int32: truncation towards zero (svcvt / C++ cast
  //     semantics). 1-packet form: first N lanes converted, upper zero-filled.
  alignas(64) int32_t oi[32] = {0};
  PacketXi vi = pcast<PacketXd, PacketXi>(vh);
  pstore<numext::int32_t>(oi, vi);
  for (int i = 0; i < N; i++) check(oi[i] == int32_t(hz[i]), "pcast d->i32 (1-pkt)");
  for (int i = N; i < NI; i++) check(oi[i] == 0, "pcast d->i32 (1-pkt) upper lanes zero");

  // 2-packet form: lanes [0,N) from a, lanes [N,2N) from b. This is the
  // signature Eigen's cast evaluator dispatches to for double->int32
  // (DstPacketSize == 2 * SrcPacketSize), so its lane order matters.
  alignas(64) double hz2[16];
  for (int i = 0; i < 16; i++) hz2[i] = 100.0 + i;
  PacketXd vh2 = pload<PacketXd>(hz2);
  PacketXi vi2 = pcast<PacketXd, PacketXi>(vh, vh2);
  pstore<numext::int32_t>(oi, vi2);
  for (int i = 0; i < N; i++) check(oi[i] == int32_t(hz[i]), "pcast d->i32 (2-pkt) low half");
  for (int i = 0; i < N; i++) check(oi[N + i] == int32_t(hz2[i]), "pcast d->i32 (2-pkt) high half");

  // --- pcast int32 -> double: converts the first (low) N i32 lanes — the
  //     NEON pcast<Packet4i, Packet2d> "low half" convention, which is also
  //     where Eigen's cast evaluator places the data.
  pstore<double>(r, pcast<PacketXi, PacketXd>(vi));
  for (int i = 0; i < N; i++) check(r[i] == double(int32_t(hz[i])), "pcast i32->d");

  // --- preinterpret: bit-level reinterpret between the f64 and i32 views of
  //     the same VL bits (both size-specific types fill VL). Round-trips
  //     bitwise; also must not crash on direct use.
  PacketXd ri = preinterpret<PacketXd, PacketXi>(vi);
  PacketXi rt = preinterpret<PacketXi, PacketXd>(ri);
  alignas(64) int32_t o_orig[32] = {0}, o_rt[32] = {0};
  pstore<numext::int32_t>(o_orig, vi);
  pstore<numext::int32_t>(o_rt, rt);
  check(memcmp(o_orig, o_rt, NI * sizeof(int32_t)) == 0, "preinterpret round-trip");
  pstore<double>(r, ri);
  check(true, "preinterpret no-crash");

  // --- end-to-end through Eigen's cast evaluator: exercises
  //     type_casting_traits<double,int32_t> / <int32_t,double> and the
  //     2-packet d->i32 / 1-packet i32->d dispatch on real expressions.
  {
    const int M = 2 * NI;  // multiple of both N and NI
    Eigen::Matrix<double, Eigen::Dynamic, 1> src(M);
    for (int i = 0; i < M; i++) src[i] = -5.0 + 0.5 * i;
    Eigen::Matrix<int32_t, Eigen::Dynamic, 1> mid = src.cast<int32_t>();
    bool ok1 = true;
    for (int i = 0; i < M; i++)
      if (mid[i] != int32_t(src[i])) ok1 = false;
    Eigen::Matrix<double, Eigen::Dynamic, 1> back2 = mid.cast<double>();
    bool ok2 = true;
    for (int i = 0; i < M; i++)
      if (back2[i] != double(int32_t(src[i]))) ok2 = false;
    check(ok1, "end-to-end .cast<int32_t>()");
    check(ok2, "end-to-end .cast<double>()");
  }

  if (failures == 0) { printf("ALL PASS (%d f64 lanes)\n", N); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}

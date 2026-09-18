// Standalone correctness test for Eigen SVE PacketXd (double).
// Build at VL=256 (host) — the same source must also compile at VL=128/512
// (compile-only check on host; run on gem5 for 512).
// Each op is compared bitwise against a scalar reference loop.
#include <Eigen/Core>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cstdint>

using namespace Eigen;
using namespace Eigen::internal;

static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else printf("ok: %s\n", what);
}

static bool bits_eq(double a, double b) {
  // NaN compare: both NaN passes; else bitwise (catches -0.0 vs +0.0)
  if (std::isnan(a) && std::isnan(b)) return true;
  uint64_t x, y; memcpy(&x, &a, 8); memcpy(&y, &b, 8);
  return x == y;
}

int main() {
  const int N = packet_traits<double>::size;
  printf("PacketXd: size=%d (VL=%d bits) backend=%s\n",
         N, EIGEN_ARM64_SVE_VL, Eigen::SimdInstructionSetsInUse());
  check(N == EIGEN_ARM64_SVE_VL / 64, "packet size == VL/64");
  check((int)unpacket_traits<PacketXd>::size == N, "unpacket size");

  alignas(64) double a[64], b[64], r[64], ref[64];
  for (int i = 0; i < 64; i++) { a[i] = -3.0 + i * 0.5; b[i] = 2.0 - i * 0.25; }
  // exercise a few exact-power-of-two / negative / denormal-free values

  // --- pset1 / pload / pstore round trip
  PacketXd v = pset1<PacketXd>(7.5);
  pstore<double>(r, v);
  for (int i = 0; i < N; i++) check(r[i] == 7.5, "pset1/pstore");

  PacketXd va = pload<PacketXd>(a), vb = pload<PacketXd>(b);
  pstore<double>(r, va); check(memcmp(r, a, N*8) == 0, "pload/pstore aligned");
  PacketXd vu = ploadu<PacketXd>(a + 1); pstoreu<double>(r, vu);
  check(memcmp(r, a + 1, N*8) == 0, "ploadu/pstoreu");

  // --- arithmetic vs scalar reference
  #define OP1_TEST(NAME, EXPR_SCALAR) do { \
    pstore<double>(r, NAME(va)); \
    for (int i = 0; i < N; i++) ref[i] = EXPR_SCALAR; \
    for (int i = 0; i < N; i++) check(bits_eq(r[i], ref[i]), #NAME); \
  } while (0)
  #define OP2_TEST(NAME, EXPR_SCALAR) do { \
    pstore<double>(r, NAME(va, vb)); \
    for (int i = 0; i < N; i++) ref[i] = EXPR_SCALAR; \
    for (int i = 0; i < N; i++) check(bits_eq(r[i], ref[i]), #NAME); \
  } while (0)

  OP2_TEST(padd, a[i] + b[i]);
  OP2_TEST(psub, a[i] - b[i]);
  OP2_TEST(pmul, a[i] * b[i]);
  OP2_TEST(pdiv, a[i] / b[i]);
  OP2_TEST(pmin, a[i] < b[i] ? a[i] : b[i]);
  OP2_TEST(pmax, a[i] > b[i] ? a[i] : b[i]);
  OP1_TEST(pnegate, -a[i]);
  OP1_TEST(pabs, std::abs(a[i]));
  OP1_TEST(pfloor, std::floor(a[i]));
  OP1_TEST(pceil, std::ceil(a[i]));

  // pmadd: c + a*b
  PacketXd vc = pset1<PacketXd>(0.25);
  pstore<double>(r, pmadd(va, vb, vc));
  for (int i = 0; i < N; i++) check(bits_eq(r[i], 0.25 + a[i]*b[i]), "pmadd");

  // --- comparisons: all-lanes-true / all-lanes-false patterns
  PacketXd ones = pset1<PacketXd>(1.0), zeros = pzero(ones);
  PacketXd eq = pcmp_eq(va, va);
  pstore<double>(r, eq);
  for (int i = 0; i < N; i++) check(r[i] != 0.0, "pcmp_eq self");
  PacketXd ne = pcmp_eq(ones, zeros);
  pstore<double>(r, ne);
  for (int i = 0; i < N; i++) check(r[i] == 0.0, "pcmp_eq diff");

  // --- redux
  double s = 0; for (int i = 0; i < N; i++) s += a[i];
  check(bits_eq(predux(va), s), "predux");
  double mx = a[0]; for (int i = 0; i < N; i++) mx = mx > a[i] ? mx : a[i];
  check(bits_eq(predux_max(va), mx), "predux_max");
  double mn = a[0]; for (int i = 0; i < N; i++) mn = mn < a[i] ? mn : a[i];
  check(bits_eq(predux_min(va), mn), "predux_min");

  // --- pfirst / preverse / ploaddup
  check(pfirst(va) == a[0], "pfirst");
  PacketXd rv = preverse(va); pstore<double>(r, rv);
  for (int i = 0; i < N; i++) check(r[i] == a[N-1-i], "preverse");
  PacketXd dd = ploaddup<PacketXd>(a); pstore<double>(r, dd);
  for (int i = 0; i < N/2; i++) { check(r[2*i] == a[i] && r[2*i+1] == a[i], "ploaddup"); }

  // --- gather/scatter with stride 3
  double g[300]; for (int i = 0; i < 300; i++) g[i] = i * 1.5;
  PacketXd pg_ = pgather<double, PacketXd>(g, 3); pstore<double>(r, pg_);
  for (int i = 0; i < N; i++) check(r[i] == g[3*i], "pgather");
  double sc[300] = {0};
  pscatter<double, PacketXd>(sc, va, 2);
  for (int i = 0; i < N; i++) check(sc[2*i] == a[i], "pscatter");

  // --- plset
  PacketXd ls = plset<PacketXd>(10.0); pstore<double>(r, ls);
  for (int i = 0; i < N; i++) check(r[i] == 10.0 + i, "plset");

  if (failures == 0) { printf("ALL PASS (%d lanes)\n", N); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}

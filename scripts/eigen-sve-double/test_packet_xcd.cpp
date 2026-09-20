// Standalone correctness test for Eigen SVE PacketXcd (complex<double>).
// Build at VL=256 (host) — the same source must also compile at VL=128/512
// (VL=128 runs via the task-VL pinning launcher; VL=512 compile-only on host).
// Each op is compared bitwise against a std::complex<double> reference.
#include <Eigen/Core>
#include <complex>
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
  const int N = packet_traits<std::complex<double>>::size;  // complex count
  printf("PacketXcd: %d complex/lane (VL=%d bits)\n", N, EIGEN_ARM64_SVE_VL);
  check(N == EIGEN_ARM64_SVE_VL / 128, "complex packet size == VL/128");

  std::complex<double> a[32], b[32], r[32];
  // deterministic non-trivial values, exact in f64 (halves and integers)
  for (int i = 0; i < 32; i++) {
    a[i] = {0.5 * i - 3.0, 0.25 * i + 1.0};
    b[i] = {-1.5 + 0.5 * i, 2.0 - 0.125 * i};
  }

  // --- load/store round trip
  PacketXcd va = pload<PacketXcd>(a), vb = pload<PacketXcd>(b);
  pstore<std::complex<double>>(r, va);
  check(memcmp(r, a, N * sizeof(std::complex<double>)) == 0, "pload/pstore complex");

  // --- ploadu/pstoreu round trip (offset by one complex = unaligned in f64 lanes at VL=128)
  PacketXcd vu = ploadu<PacketXcd>(a + 1);
  pstoreu<std::complex<double>>(r, vu);
  check(memcmp(r, a + 1, N * sizeof(std::complex<double>)) == 0, "ploadu/pstoreu complex");

  // --- pset1
  PacketXcd vc = pset1<PacketXcd>({2.0, -1.0});
  pstore<std::complex<double>>(r, vc);
  for (int i = 0; i < N; i++) check(r[i] == std::complex<double>(2.0, -1.0), "pset1 complex");

  // --- add/sub
  pstore<std::complex<double>>(r, padd(va, vb));
  for (int i = 0; i < N; i++) check(r[i] == a[i] + b[i], "padd complex");

  pstore<std::complex<double>>(r, psub(va, vb));
  for (int i = 0; i < N; i++) check(r[i] == a[i] - b[i], "psub complex");

  // --- complex multiply: THE critical op (svcmla rot0+rot90 recipe)
  pstore<std::complex<double>>(r, pmul(va, vb));
  for (int i = 0; i < N; i++)
    check(r[i] == a[i] * b[i], "pmul complex (svcmla rot0+rot90)");

  // --- pmadd
  PacketXcd acc = pset1<PacketXcd>({0.5, 0.5});
  pstore<std::complex<double>>(r, pmadd(va, vb, acc));
  for (int i = 0; i < N; i++)
    check(r[i] == std::complex<double>(0.5, 0.5) + a[i] * b[i], "pmadd complex");

  // --- conj / negate
  PacketXcd cj = pconj(va);
  pstore<std::complex<double>>(r, cj);
  for (int i = 0; i < N; i++) check(r[i] == std::conj(a[i]), "pconj");

  PacketXcd ng = pnegate(va);
  pstore<std::complex<double>>(r, ng);
  for (int i = 0; i < N; i++) check(r[i] == -a[i], "pnegate");

  // --- conj_helper paths (what apply_rotation uses):
  //   conj_helper<PacketXcd,PacketXcd,true,false>::pmul = conj(a)*b
  {
    conj_helper<PacketXcd, PacketXcd, true, false> h;
    pstore<std::complex<double>>(r, h.pmul(va, vb));
    for (int i = 0; i < N; i++) check(r[i] == std::conj(a[i]) * b[i], "conj_helper L");
    conj_helper<PacketXcd, PacketXcd, false, true> h2;
    pstore<std::complex<double>>(r, h2.pmul(va, vb));
    for (int i = 0; i < N; i++) check(r[i] == a[i] * std::conj(b[i]), "conj_helper R");
    conj_helper<PacketXcd, PacketXcd, true, true> h3;
    pstore<std::complex<double>>(r, h3.pmul(va, vb));
    for (int i = 0; i < N; i++) check(r[i] == std::conj(a[i] * b[i]), "conj_helper LR");
  }

  // --- pcmp_eq
  PacketXcd same = pload<PacketXcd>(a);
  PacketXcd eq = pcmp_eq(va, same);
  std::complex<double> eqr[32]; pstore<std::complex<double>>(eqr, eq);
  for (int i = 0; i < N; i++) check(eqr[i].real() != 0.0, "pcmp_eq complex");
  PacketXcd eqn = pcmp_eq(va, vb);
  std::complex<double> eqn2[32]; pstore<std::complex<double>>(eqn2, eqn);
  for (int i = 0; i < N; i++) check(eqn2[i].real() == 0.0, "pcmp_eq complex (diff)");
  // partial match: re== but im!= must zero BOTH lanes of the pair
  // (per-complex semantics, NEON/AVX pand(eq, swap(eq)) contract).
  std::complex<double> aimp[32];
  for (int i = 0; i < 32; i++) aimp[i] = {a[i].real(), a[i].imag() + 7.0};
  PacketXcd eqp = pcmp_eq(va, pload<PacketXcd>(aimp));
  std::complex<double> eqp2[32]; pstore<std::complex<double>>(eqp2, eqp);
  for (int i = 0; i < N; i++)
    check(eqp2[i].real() == 0.0 && eqp2[i].imag() == 0.0, "pcmp_eq complex (re==, im!=)");

  // --- bitwise ops (pass-through of real packet bit ops)
  PacketXcd bi = pand(pcmp_eq(va, same), pcmp_eq(va, same));
  (void)bi;
  PacketXcd bo = por(pcmp_eq(va, same), pcmp_eq(va, vb));
  std::complex<double> bor[32]; pstore<std::complex<double>>(bor, bo);
  for (int i = 0; i < N; i++) check(bor[i].real() != 0.0, "por complex");
  PacketXcd bx = pxor(pcmp_eq(va, same), pcmp_eq(va, same));
  std::complex<double> bxr[32]; pstore<std::complex<double>>(bxr, bx);
  for (int i = 0; i < N; i++) check(bxr[i].real() == 0.0, "pxor complex");
  PacketXcd bn = pandnot(pcmp_eq(va, same), pcmp_eq(va, vb));
  std::complex<double> bnr[32]; pstore<std::complex<double>>(bnr, bn);
  for (int i = 0; i < N; i++) check(bnr[i].real() != 0.0, "pandnot complex");

  // --- pfirst
  std::complex<double> f = pfirst(va);
  check(f == a[0], "pfirst complex");

  // --- preverse (complex granularity: result[j] = a[N-1-j] as complex values)
  PacketXcd rv = preverse(va);
  pstore<std::complex<double>>(r, rv);
  for (int i = 0; i < N; i++)
    check(r[i].real() == a[N - 1 - i].real() && r[i].imag() == a[N - 1 - i].imag(),
          "preverse complex");

  // --- ploaddup (each complex from memory broadcast into consecutive lanes)
  PacketXcd dd = ploaddup<PacketXcd>(a);
  pstore<std::complex<double>>(r, dd);
  for (int i = 0; i < N; i++)
    check(r[i] == a[i / 2], "ploaddup complex");

  if (failures == 0) { printf("ALL PASS (%d complex lanes)\n", N); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}

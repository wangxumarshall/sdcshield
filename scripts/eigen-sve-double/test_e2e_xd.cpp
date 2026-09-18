// End-to-end: real Eigen expression layer on the SVE double/complex<double>
// packet backend (Tasks 1-3). Correctness vs independent golden references
// plus the BDCSVD performance gate that motivated this whole branch.
//
// Modes:
//   ./test_e2e_xd          — full sizes (GEMM 96, SVD 300): real hardware
//   ./test_e2e_xd --mini   — mini sizes  (GEMM 48, SVD 48): gem5 SE mode
//                            (atomic CPU is 3-4 orders slower; same code
//                            paths, smaller work)
#include <Eigen/Dense>
#include <Eigen/Eigenvalues>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdio>
#include <cstring>
#include <string>

using namespace Eigen;

static int failures = 0;
static void check(bool ok, const char* what) {
  if (!ok) { printf("FAIL: %s\n", what); failures++; }
  else printf("ok: %s\n", what);
}

int main(int argc, char** argv) {
  const bool mini = (argc > 1 && std::string(argv[1]) == "--mini");
  const int GN = mini ? 48 : 96;    // GEMM size
  const int SN = mini ? 48 : 300;   // SVD size

  printf("backend=%s packet_traits<double>::size=%d packet_traits<complex<double>>::size=%d%s\n",
         Eigen::SimdInstructionSetsInUse(),
         (int)internal::packet_traits<double>::size,
         (int)internal::packet_traits<std::complex<double>>::size,
         mini ? " [MINI]" : "");

  // --- 1. double GEMM vs independent triple-loop golden. GEMM blocking
  //        reorders FP ops, so compare with a relative tolerance, not
  //        bitwise (unlike the packet-op tests which are bit-exact).
  MatrixXd A = MatrixXd::Random(GN, GN), B = MatrixXd::Random(GN, GN);
  MatrixXd C = A * B;
  MatrixXd G(GN, GN);
  for (int j = 0; j < GN; j++)
    for (int i = 0; i < GN; i++) {
      double s = 0;
      for (int k = 0; k < GN; k++) s += A(i, k) * B(k, j);
      G(i, j) = s;
    }
  double maxrel = 0;
  for (int i = 0; i < GN; i++)
    for (int j = 0; j < GN; j++) {
      double rel = std::abs(C(i, j) - G(i, j)) / std::abs(G(i, j));
      if (std::isfinite(rel)) maxrel = std::max(maxrel, rel);
    }
  printf("GEMM %dx%d max relative error vs triple-loop: %.3e\n", GN, GN, maxrel);
  check(maxrel <= 1e-13, "double GEMM accuracy");

  // --- 2. complex GEMM vs triple-loop golden (exercises PacketXcd through
  //        the GEBP kernel incl. conj_helper paths).
  MatrixXcd Ac = MatrixXcd::Random(64, 64), Bc = MatrixXcd::Random(64, 64);
  MatrixXcd Cc = Ac * Bc;
  MatrixXcd Gc(64, 64);
  for (int j = 0; j < 64; j++)
    for (int i = 0; i < 64; i++) {
      std::complex<double> s = 0;
      for (int k = 0; k < 64; k++) s += Ac(i, k) * Bc(k, j);
      Gc(i, j) = s;
    }
  double maxrelc = 0;
  for (int i = 0; i < 64; i++)
    for (int j = 0; j < 64; j++) {
      double rel = std::abs(Cc(i, j) - Gc(i, j)) / std::abs(Gc(i, j));
      if (std::isfinite(rel)) maxrelc = std::max(maxrelc, rel);
    }
  printf("complex GEMM 64x64 max relative error: %.3e\n", maxrelc);
  check(maxrelc <= 1e-13, "complex GEMM accuracy");

  // --- 3. Jacobi rotation path (the apply_rotation_in_the_plane selector
  //        that was the scalar-fallback bottleneck before this branch).
  MatrixXd M = MatrixXd::Random(300, 300);
  JacobiRotation<double> jr(0.6, 0.8);  // c^2+s^2 = 1
  MatrixXd M2 = M;
  M2.applyOnTheLeft(3, 7, jr);
  MatrixXd gold = M;
  for (int j = 0; j < 300; j++) {
    double x = gold(3, j), y = gold(7, j);
    gold(3, j) = 0.6 * x + 0.8 * y;
    gold(7, j) = -0.8 * x + 0.6 * y;
  }
  double rot_err = (M2 - gold).cwiseAbs().maxCoeff();
  printf("applyOnTheLeft max abs err: %.3e\n", rot_err);
  check(rot_err <= 1e-13, "Jacobi rotation accuracy");

  // --- 4. double BDCSVD — the eigen_svd_cdouble_sve workload family.
  MatrixXd S = MatrixXd::Random(SN, SN);
  auto t0 = std::chrono::steady_clock::now();
  BDCSVD<MatrixXd, ComputeFullU | ComputeFullV> svd(S);
  auto t1 = std::chrono::steady_clock::now();
  double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  printf("%dx%d double BDCSVD: %.0f ms\n", SN, SN, ms);

  MatrixXd rec = svd.matrixU() * svd.singularValues().asDiagonal() * svd.matrixV().transpose();
  double svd_err = (rec - S).cwiseAbs().maxCoeff() / S.cwiseAbs().maxCoeff();
  printf("BDCSVD reconstruction rel err: %.3e\n", svd_err);
  check(svd_err <= 1e-10, "double BDCSVD accuracy");
  if (!mini && ms > 60000.0) {
    printf("FAIL: performance gate (60 s) — hot path still scalar?\n");
    return 2;
  }

  // --- 5. complex<double> BDCSVD — the eigen_svd_cdouble_sve matrix type.
  MatrixXcd Sc = MatrixXcd::Random(SN, SN);
  auto t2 = std::chrono::steady_clock::now();
  BDCSVD<MatrixXcd, ComputeFullU | ComputeFullV> svdc(Sc);
  auto t3 = std::chrono::steady_clock::now();
  double msc = std::chrono::duration<double, std::milli>(t3 - t2).count();
  printf("%dx%d complex BDCSVD: %.0f ms\n", SN, SN, msc);
  MatrixXcd recc = svdc.matrixU() * svdc.singularValues().asDiagonal() * svdc.matrixV().adjoint();
  double svdc_err = (recc - Sc).cwiseAbs().maxCoeff() / Sc.cwiseAbs().maxCoeff();
  printf("complex BDCSVD reconstruction rel err: %.3e\n", svdc_err);
  check(svdc_err <= 1e-10, "complex BDCSVD accuracy");
  if (!mini && msc > 60000.0) {
    printf("FAIL: complex performance gate (60 s)\n");
    return 2;
  }

  if (failures == 0) { printf("E2E ALL PASS\n"); return 0; }
  printf("%d FAILURES\n", failures);
  return 1;
}

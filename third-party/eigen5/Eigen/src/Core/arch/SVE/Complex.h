// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2026
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_COMPLEX_SVE_H
#define EIGEN_COMPLEX_SVE_H

// IWYU pragma: private
#include "../../InternalHeaderCheck.h"

namespace Eigen {
namespace internal {

// Odd (imaginary) lane predicate: true on lanes 1,3,5,... of each
// f64 pair. Used for complex conjugation via sign flip.
// (svbool_t is a sizeless type: it cannot have namespace-scope static
// storage, so this is a function that the compiler folds at -O1+.)
static EIGEN_STRONG_INLINE svbool_t SVE_pg_odd_f64() {
  return svzip1_b64(svpfalse_b(), svptrue_b64());
}

// Complex<double> packet: interleaved (re,im) pairs carried in a real
// PacketXd. A packet holds unpacket_traits<PacketXd>::size / 2 complex
// numbers (2 f64 lanes each).
struct PacketXcd {
  EIGEN_STRONG_INLINE PacketXcd() {}
  EIGEN_STRONG_INLINE explicit PacketXcd(const PacketXd& a) : v(a) {}
  PacketXd v;
};

template <>
struct packet_traits<std::complex<double>> : default_packet_traits {
  typedef PacketXcd type;
  typedef PacketXcd half;
  enum {
    Vectorizable = 1,
    AlignedOnScalar = 0,
    size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size / 2,

    HasAdd = 1,
    HasSub = 1,
    HasMul = 1,
    HasDiv = 1,
    HasNegate = 1,
    HasSqrt = 0,  // no psqrt<PacketXcd> yet: needs PacketXd MathFunctions
    HasLog = 0,   // no plog<PacketXcd> yet
    HasExp = 0,   // no pexp<PacketXcd> yet
    HasAbs = 0,
    HasAbs2 = 0,
    HasMin = 0,
    HasMax = 0,
    HasSetLinear = 0
  };
};

template <>
struct unpacket_traits<PacketXcd> {
  typedef std::complex<double> type;
  typedef PacketXcd half;
  typedef PacketXd as_real;
  enum {
    size = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size / 2,
    alignment = unpacket_traits<PacketXd>::alignment,
    vectorizable = true,
    masked_load_available = false,
    masked_store_available = false
  };
};

//---------- load/store ----------

template <>
EIGEN_STRONG_INLINE PacketXcd pload<PacketXcd>(const std::complex<double>* from) {
  EIGEN_DEBUG_ALIGNED_LOAD return PacketXcd(
      pload<PacketXd>(assume_aligned<unpacket_traits<PacketXcd>::alignment>(reinterpret_cast<const double*>(from))));
}

template <>
EIGEN_STRONG_INLINE PacketXcd ploadu<PacketXcd>(const std::complex<double>* from) {
  EIGEN_DEBUG_UNALIGNED_LOAD return PacketXcd(ploadu<PacketXd>(reinterpret_cast<const double*>(from)));
}

template <>
EIGEN_STRONG_INLINE void pstore<std::complex<double>>(std::complex<double>* to, const PacketXcd& from) {
  EIGEN_DEBUG_ALIGNED_STORE pstore(assume_aligned<unpacket_traits<PacketXcd>::alignment>(reinterpret_cast<double*>(to)),
                                   from.v);
}

template <>
EIGEN_STRONG_INLINE void pstoreu<std::complex<double>>(std::complex<double>* to, const PacketXcd& from) {
  EIGEN_DEBUG_UNALIGNED_STORE pstoreu(reinterpret_cast<double*>(to), from.v);
}

//---------- set / zero ----------

template <>
EIGEN_STRONG_INLINE PacketXcd pset1<PacketXcd>(const std::complex<double>& from) {
  // svld1 reads full VL worth of lanes: reading directly from &from would
  // pull in whatever follows the 2-double object. Broadcast the pair.
  return PacketXcd(svzip1_f64(svdup_n_f64(from.real()), svdup_n_f64(from.imag())));
}

template <>
EIGEN_STRONG_INLINE PacketXcd pzero<PacketXcd>(const PacketXcd& a) {
  return PacketXcd(pzero<PacketXd>(a.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd ptrue<PacketXcd>(const PacketXcd& a) {
  return PacketXcd(ptrue<PacketXd>(a.v));
}

//---------- arithmetic ----------

template <>
EIGEN_STRONG_INLINE PacketXcd padd<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return PacketXcd(padd<PacketXd>(a.v, b.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd psub<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return PacketXcd(psub<PacketXd>(a.v, b.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd pnegate(const PacketXcd& a) {
  return PacketXcd(pnegate<PacketXd>(a.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd pconj(const PacketXcd& a) {
  // Flip the sign of the odd (imaginary) lanes only.
  return PacketXcd(svneg_f64_x(SVE_pg_odd_f64(), a.v));
}

// Complex multiply via SVCMLA: rot0 accumulates xe*ye / xe*yo,
// rot90 adds -xo*yo / +xo*ye (semantics probe-verified 2026-09-18).
// pmadd seeds the accumulator with c.v, saving the separate padd
// (NEON __ARM_FEATURE_COMPLEX precedent: vcmlaq then vcmlaq_rot90).
template <>
EIGEN_STRONG_INLINE PacketXcd pmadd<PacketXcd>(const PacketXcd& a, const PacketXcd& b, const PacketXcd& c) {
  svfloat64_t t = svcmla_f64_m(svptrue_b64(), c.v, a.v, b.v, 0);
  t = svcmla_f64_m(svptrue_b64(), t, a.v, b.v, 90);
  return PacketXcd(t);
}

template <>
EIGEN_STRONG_INLINE PacketXcd pmul<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  svfloat64_t t = svcmla_f64_m(svptrue_b64(), pzero<PacketXd>(a.v), a.v, b.v, 0);
  t = svcmla_f64_m(svptrue_b64(), t, a.v, b.v, 90);
  return PacketXcd(t);
}

//---------- comparison ----------

template <>
EIGEN_STRONG_INLINE PacketXcd pcmp_eq<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  // Per-complex equality (NEON/AVX contract: re(a)==re(b) && im(a)==im(b) must
  // zero BOTH lanes of the pair). Materialize the per-lane comparison mask,
  // swap re/im lanes within each pair (index j^1), then AND the two vectors.
  svbool_t eq = svcmpeq_f64(svptrue_b64(), a.v, b.v);
  svfloat64_t mask = svreinterpret_f64_u64(svdup_n_u64_z(eq, ~0ull));
  svfloat64_t mask_swapped =
      svtbl_f64(mask, sveor_n_u64_x(svptrue_b64(), svindex_u64(0, 1), 1));
  return PacketXcd(svreinterpret_f64_u64(svand_u64_x(svptrue_b64(), svreinterpret_u64_f64(mask),
                                                           svreinterpret_u64_f64(mask_swapped))));
}

//---------- bitwise ----------

template <>
EIGEN_STRONG_INLINE PacketXcd pand<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return PacketXcd(pand<PacketXd>(a.v, b.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd por<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return PacketXcd(por<PacketXd>(a.v, b.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd pxor<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return PacketXcd(pxor<PacketXd>(a.v, b.v));
}

template <>
EIGEN_STRONG_INLINE PacketXcd pandnot<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return PacketXcd(pandnot<PacketXd>(a.v, b.v));
}

//---------- first / reverse / dup ----------

template <>
EIGEN_STRONG_INLINE std::complex<double> pfirst<PacketXcd>(const PacketXcd& a) {
  // lane0 = real, lane1 = imag of the first complex. The store predicate must
  // govern exactly 2 lanes: svptrue_b64() would write all VL/64 lanes and
  // overflow the 2-double buffer (first observed at VL=256, 16-byte OOB).
  EIGEN_ALIGN_TO_BOUNDARY(64) double res[2];
  svst1_f64(svptrue_pat_b64(SV_VL2), res, a.v);
  return std::complex<double>(res[0], res[1]);
}

template <>
EIGEN_STRONG_INLINE PacketXcd preverse(const PacketXcd& a) {
  // Complex-granularity reversal: complex j of the result is complex
  // NC-1-j of the input, re/im kept in place. In f64-lane terms the
  // source lane of target lane j is 2*(NC-1 - j/2) + (j%2)
  // (odd/even lanes keep their within-pair position):
  //   idx = (j & 1) + 2*(NC-1) - (j & ~1)
  // one svtbl_f64 (verified VL=128/256: {c1},{c0}... complex order).
  const uint64_t NC = sve_packet_size_selector<double, EIGEN_ARM64_SVE_VL>::size / 2;
  svuint64_t j = svindex_u64(0, 1);
  svuint64_t odd = svand_n_u64_x(svptrue_b64(), j, 1);
  svuint64_t evn = svand_n_u64_x(svptrue_b64(), j, ~(uint64_t)1);
  svuint64_t idx = svadd_u64_x(svptrue_b64(), odd, svsub_u64_x(svptrue_b64(), svdup_n_u64(2 * (NC - 1)), evn));
  return PacketXcd(svtbl_f64(a.v, idx));
}

template <>
EIGEN_STRONG_INLINE PacketXcd ploaddup<PacketXcd>(const std::complex<double>* from) {
  // Broadcast each complex to two consecutive result complexes:
  // result complex i = from[i/2]. Source lane of target lane j is
  //   idx = ((j >> 1) & ~1) | (j & 1)
  // i.e. lanes {0,1,0,1,2,3,2,3,...} — verified at VL=128/256.
  svuint64_t j = svindex_u64(0, 1);
  svuint64_t idx = svorr_u64_x(svptrue_b64(),
                               svand_n_u64_x(svptrue_b64(), svlsr_n_u64_x(svptrue_b64(), j, 1), ~(uint64_t)1),
                               svand_n_u64_x(svptrue_b64(), j, 1));
  return PacketXcd(svld1_gather_u64index_f64(svptrue_b64(), reinterpret_cast<const double*>(from), idx));
}

//---------- complex helpers ----------

EIGEN_STRONG_INLINE PacketXcd pcplxflip /*<PacketXcd>*/(const PacketXcd& x) {
  // Swap re/im within each complex pair: f64-lane index {1,0,3,2,...}.
  svuint64_t idx = sveor_n_u64_x(svptrue_b64(), svindex_u64(0, 1), 1);
  return PacketXcd(svtbl_f64(x.v, idx));
}

EIGEN_MAKE_CONJ_HELPER_CPLX_REAL(PacketXcd, PacketXd)

template <>
EIGEN_STRONG_INLINE PacketXcd pdiv<PacketXcd>(const PacketXcd& a, const PacketXcd& b) {
  return pdiv_complex(a, b);
}

//---------- reduction ----------

template <>
EIGEN_STRONG_INLINE std::complex<double> predux<PacketXcd>(const PacketXcd& a) {
  // Horizontal sum at complex granularity: fold the reversed vector in
  // (pairwise) until one complex remains, then extract it.
  svfloat64_t prod = a.v;
  svfloat64_t half;
  if (EIGEN_ARM64_SVE_VL >= 2048) {
    half = svtbl_f64(prod, svindex_u64(16, 1));
    prod = svadd_f64_x(svptrue_b64(), prod, half);
  }
  if (EIGEN_ARM64_SVE_VL >= 1024) {
    half = svtbl_f64(prod, svindex_u64(8, 1));
    prod = svadd_f64_x(svptrue_b64(), prod, half);
  }
  if (EIGEN_ARM64_SVE_VL >= 512) {
    half = svtbl_f64(prod, svindex_u64(4, 1));
    prod = svadd_f64_x(svptrue_b64(), prod, half);
  }
  if (EIGEN_ARM64_SVE_VL >= 256) {
    half = svtbl_f64(prod, svindex_u64(2, 1));
    prod = svadd_f64_x(svptrue_b64(), prod, half);
  }
  // VL=128: a single complex, nothing to fold.
  return pfirst<PacketXcd>(PacketXcd(prod));
}

}  // namespace internal
}  // namespace Eigen

#endif  // EIGEN_COMPLEX_SVE_H

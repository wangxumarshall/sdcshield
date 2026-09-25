// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2020, Arm Limited and Contributors
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_MATH_FUNCTIONS_SVE_H
#define EIGEN_MATH_FUNCTIONS_SVE_H

// IWYU pragma: private
#include "../../InternalHeaderCheck.h"

namespace Eigen {
namespace internal {

template <>
EIGEN_STRONG_INLINE PacketXf pexp<PacketXf>(const PacketXf& x) {
  return pexp_float(x);
}

template <>
EIGEN_STRONG_INLINE PacketXf plog<PacketXf>(const PacketXf& x) {
  return plog_float(x);
}

template <>
EIGEN_STRONG_INLINE PacketXf psin<PacketXf>(const PacketXf& x) {
  return psin_float(x);
}

template <>
EIGEN_STRONG_INLINE PacketXf pcos<PacketXf>(const PacketXf& x) {
  return pcos_float(x);
}

// Hyperbolic Tangent function.
template <>
EIGEN_STRONG_INLINE PacketXf ptanh<PacketXf>(const PacketXf& x) {
  return ptanh_float(x);
}

/********************************** double ***********************************/

// Rounding semantics (hardware-probe verified, 2026-09-18):
//   pround -> svrinta = round half AWAY FROM ZERO (std::round semantics)
//   print  -> svrintn = round half TO EVEN (std::nearbyint semantics under
//             the default round-to-nearest mode, matching NEON's vrndnq_f64
//             choice for its Packet2d print). The half-away/half-even
//             distinction is pinned by scripts/eigen-sve-double/test_math_xd.cpp.
template <>
EIGEN_STRONG_INLINE PacketXd pround<PacketXd>(const PacketXd& a) {
  return svrinta_f64_x(svptrue_b64(), a);
}

template <>
EIGEN_STRONG_INLINE PacketXd print<PacketXd>(const PacketXd& a) {
  return svrintn_f64_x(svptrue_b64(), a);
}

}  // end namespace internal
}  // end namespace Eigen

#endif  // EIGEN_MATH_FUNCTIONS_SVE_H

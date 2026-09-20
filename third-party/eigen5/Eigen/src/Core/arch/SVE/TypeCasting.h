// This file is part of Eigen, a lightweight C++ template library
// for linear algebra.
//
// Copyright (C) 2020, Arm Limited and Contributors
//
// This Source Code Form is subject to the terms of the Mozilla
// Public License v. 2.0. If a copy of the MPL was not distributed
// with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

#ifndef EIGEN_TYPE_CASTING_SVE_H
#define EIGEN_TYPE_CASTING_SVE_H

// IWYU pragma: private
#include "../../InternalHeaderCheck.h"

namespace Eigen {
namespace internal {

template <>
struct type_casting_traits<float, numext::int32_t> {
  enum { VectorizedCast = 1, SrcCoeffRatio = 1, TgtCoeffRatio = 1 };
};

template <>
struct type_casting_traits<numext::int32_t, float> {
  enum { VectorizedCast = 1, SrcCoeffRatio = 1, TgtCoeffRatio = 1 };
};

template <>
EIGEN_STRONG_INLINE PacketXf pcast<PacketXi, PacketXf>(const PacketXi& a) {
  return svcvt_f32_s32_x(svptrue_b32(), a);
}

template <>
EIGEN_STRONG_INLINE PacketXi pcast<PacketXf, PacketXi>(const PacketXf& a) {
  return svcvt_s32_f32_x(svptrue_b32(), a);
}

template <>
EIGEN_STRONG_INLINE PacketXf preinterpret<PacketXf, PacketXi>(const PacketXi& a) {
  return svreinterpret_f32_s32(a);
}

template <>
EIGEN_STRONG_INLINE PacketXi preinterpret<PacketXi, PacketXf>(const PacketXf& a) {
  return svreinterpret_s32_f32(a);
}

//==============================================================================
// double <-> int32 (cross-width: the f64 packet holds VL/64 lanes, the i32
// packet VL/32 — twice as many). Follows the NEON pcast<Packet2d,Packet4i> /
// pcast<Packet4i,Packet2d> conventions: the 2-packet d->i32 form packs a's
// lanes then b's lanes; the i32->d form converts the low (first) half. All
// conversions truncate towards zero (svcvt / C++ static_cast semantics).
// The lane-count mismatch makes a pure intrinsics shuffle nontrivial, so we
// bridge through memory per element — correctness over speed (not a hot
// path; SVD/GEMM never cast double<->int32 in inner loops).
//==============================================================================

template <>
struct type_casting_traits<double, numext::int32_t> {
  enum { VectorizedCast = 1, SrcCoeffRatio = 2, TgtCoeffRatio = 1 };
};

template <>
struct type_casting_traits<numext::int32_t, double> {
  enum { VectorizedCast = 1, SrcCoeffRatio = 1, TgtCoeffRatio = 2 };
};

// One f64 packet -> i32 packet: first N lanes converted, upper half zero.
template <>
EIGEN_STRONG_INLINE PacketXi pcast<PacketXd, PacketXi>(const PacketXd& a) {
  constexpr int N = unpacket_traits<PacketXd>::size;
  EIGEN_ALIGN_MAX double src[N];
  EIGEN_ALIGN_MAX numext::int32_t dst[2 * N];
  pstoreu<double>(src, a);
  for (int i = 0; i < N; i++) dst[i] = static_cast<numext::int32_t>(src[i]);
  for (int i = N; i < 2 * N; i++) dst[i] = 0;
  return ploadu<PacketXi>(dst);
}

// Two f64 packets -> one full i32 packet: a's lanes then b's lanes. This is
// the signature Eigen's cast evaluator dispatches to for double -> int32
// (DstPacketSize == 2 * SrcPacketSize).
template <>
EIGEN_STRONG_INLINE PacketXi pcast<PacketXd, PacketXi>(const PacketXd& a, const PacketXd& b) {
  constexpr int N = unpacket_traits<PacketXd>::size;
  EIGEN_ALIGN_MAX double src[2 * N];
  EIGEN_ALIGN_MAX numext::int32_t dst[2 * N];
  pstoreu<double>(src, a);
  pstoreu<double>(src + N, b);
  for (int i = 0; i < 2 * N; i++) dst[i] = static_cast<numext::int32_t>(src[i]);
  return ploadu<PacketXi>(dst);
}

// One i32 packet -> one f64 packet, converting the first (low) N lanes —
// the NEON pcast<Packet4i,Packet2d> convention, and where Eigen's cast
// evaluator places the coefficients it feeds in.
template <>
EIGEN_STRONG_INLINE PacketXd pcast<PacketXi, PacketXd>(const PacketXi& a) {
  constexpr int N = unpacket_traits<PacketXd>::size;
  EIGEN_ALIGN_MAX numext::int32_t src[2 * N];
  EIGEN_ALIGN_MAX double dst[N];
  pstoreu<numext::int32_t>(src, a);
  for (int i = 0; i < N; i++) dst[i] = static_cast<double>(src[i]);
  return ploadu<PacketXd>(dst);
}

// Bit-level reinterpretation between the f64 and i32 views of the same VL
// bits — legal for size-specific SVE types since both fill the full vector.
template <>
EIGEN_STRONG_INLINE PacketXd preinterpret<PacketXd, PacketXi>(const PacketXi& a) {
  return svreinterpret_f64_s32(a);
}

template <>
EIGEN_STRONG_INLINE PacketXi preinterpret<PacketXi, PacketXd>(const PacketXd& a) {
  return svreinterpret_s32_f64(a);
}

}  // namespace internal
}  // namespace Eigen

#endif  // EIGEN_TYPE_CASTING_SVE_H

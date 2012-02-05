// Copyright 2006-2008 the V8 project authors. All rights reserved.

#include <stdlib.h>

#include "cctest.h"
#include "diy-fp.h"
#include "double.h"
#include "utils.h"


using namespace double_conversion;


TEST(Uint64Conversions) {
  // Start by checking the byte-order.
  uint64_t ordered = UINT64_2PART_C(0x01234567, 89ABCDEF);
  CHECK_EQ(3512700564088504e-318, Double(ordered).value());

  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  CHECK_EQ(5e-324, Double(min_double64).value());

  uint64_t max_double64 = UINT64_2PART_C(0x7fefffff, ffffffff);
  CHECK_EQ(1.7976931348623157e308, Double(max_double64).value());
}

TEST(AsDiyFp) {
  uint64_t ordered = UINT64_2PART_C(0x01234567, 89ABCDEF);
  DiyFp diy_fp = Double(ordered).AsDiyFp();
  CHECK_EQ(0x12 - 0x3FF - 52, diy_fp.e());
  // The 52 mantissa bits, plus the implicit 1 in bit 52 as a UINT64.
  CHECK(UINT64_2PART_C(0x00134567, 89ABCDEF) == diy_fp.f());  // NOLINT

  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  diy_fp = Double(min_double64).AsDiyFp();
  CHECK_EQ(-0x3FF - 52 + 1, diy_fp.e());
  // This is a denormal; so no hidden bit.
  CHECK(1 == diy_fp.f());  // NOLINT

  uint64_t max_double64 = UINT64_2PART_C(0x7fefffff, ffffffff);
  diy_fp = Double(max_double64).AsDiyFp();
  CHECK_EQ(0x7FE - 0x3FF - 52, diy_fp.e());
  CHECK(UINT64_2PART_C(0x001fffff, ffffffff) == diy_fp.f());  // NOLINT
}


TEST(AsNormalizedDiyFp) {
  uint64_t ordered = UINT64_2PART_C(0x01234567, 89ABCDEF);
  DiyFp diy_fp = Double(ordered).AsNormalizedDiyFp();
  CHECK_EQ(0x12 - 0x3FF - 52 - 11, diy_fp.e());
  CHECK((UINT64_2PART_C(0x00134567, 89ABCDEF) << 11) ==
        diy_fp.f());  // NOLINT

  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  diy_fp = Double(min_double64).AsNormalizedDiyFp();
  CHECK_EQ(-0x3FF - 52 + 1 - 63, diy_fp.e());
  // This is a denormal; so no hidden bit.
  CHECK(UINT64_2PART_C(0x80000000, 00000000) == diy_fp.f());  // NOLINT

  uint64_t max_double64 = UINT64_2PART_C(0x7fefffff, ffffffff);
  diy_fp = Double(max_double64).AsNormalizedDiyFp();
  CHECK_EQ(0x7FE - 0x3FF - 52 - 11, diy_fp.e());
  CHECK((UINT64_2PART_C(0x001fffff, ffffffff) << 11) ==
        diy_fp.f());  // NOLINT
}


TEST(IsDenormal) {
  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  CHECK(Double(min_double64).IsDenormal());
  uint64_t bits = UINT64_2PART_C(0x000FFFFF, FFFFFFFF);
  CHECK(Double(bits).IsDenormal());
  bits = UINT64_2PART_C(0x00100000, 00000000);
  CHECK(!Double(bits).IsDenormal());
}


TEST(IsSpecial) {
  CHECK(Double(Double::Infinity()).IsSpecial());
  CHECK(Double(-Double::Infinity()).IsSpecial());
  CHECK(Double(Double::NaN()).IsSpecial());
  uint64_t bits = UINT64_2PART_C(0xFFF12345, 00000000);
  CHECK(Double(bits).IsSpecial());
  // Denormals are not special:
  CHECK(!Double(5e-324).IsSpecial());
  CHECK(!Double(-5e-324).IsSpecial());
  // And some random numbers:
  CHECK(!Double(0.0).IsSpecial());
  CHECK(!Double(-0.0).IsSpecial());
  CHECK(!Double(1.0).IsSpecial());
  CHECK(!Double(-1.0).IsSpecial());
  CHECK(!Double(1000000.0).IsSpecial());
  CHECK(!Double(-1000000.0).IsSpecial());
  CHECK(!Double(1e23).IsSpecial());
  CHECK(!Double(-1e23).IsSpecial());
  CHECK(!Double(1.7976931348623157e308).IsSpecial());
  CHECK(!Double(-1.7976931348623157e308).IsSpecial());
}


TEST(IsInfinite) {
  CHECK(Double(Double::Infinity()).IsInfinite());
  CHECK(Double(-Double::Infinity()).IsInfinite());
  CHECK(!Double(Double::NaN()).IsInfinite());
  CHECK(!Double(0.0).IsInfinite());
  CHECK(!Double(-0.0).IsInfinite());
  CHECK(!Double(1.0).IsInfinite());
  CHECK(!Double(-1.0).IsInfinite());
  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  CHECK(!Double(min_double64).IsInfinite());
}


TEST(IsNan) {
  CHECK(Double(Double::NaN()).IsNan());
  uint64_t other_nan = UINT64_2PART_C(0xFFFFFFFF, 00000001);
  CHECK(Double(other_nan).IsNan());
  CHECK(!Double(Double::Infinity()).IsNan());
  CHECK(!Double(-Double::Infinity()).IsNan());
  CHECK(!Double(0.0).IsNan());
  CHECK(!Double(-0.0).IsNan());
  CHECK(!Double(1.0).IsNan());
  CHECK(!Double(-1.0).IsNan());
  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  CHECK(!Double(min_double64).IsNan());
}


TEST(Sign) {
  CHECK_EQ(1, Double(1.0).Sign());
  CHECK_EQ(1, Double(Double::Infinity()).Sign());
  CHECK_EQ(-1, Double(-Double::Infinity()).Sign());
  CHECK_EQ(1, Double(0.0).Sign());
  CHECK_EQ(-1, Double(-0.0).Sign());
  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  CHECK_EQ(1, Double(min_double64).Sign());
}


TEST(NormalizedBoundaries) {
  DiyFp boundary_plus;
  DiyFp boundary_minus;
  DiyFp diy_fp = Double(1.5).AsNormalizedDiyFp();
  Double(1.5).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // 1.5 does not have a significand of the form 2^p (for some p).
  // Therefore its boundaries are at the same distance.
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 10) == diy_fp.f() - boundary_minus.f());  // NOLINT

  diy_fp = Double(1.0).AsNormalizedDiyFp();
  Double(1.0).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // 1.0 does have a significand of the form 2^p (for some p).
  // Therefore its lower boundary is twice as close as the upper boundary.
  CHECK(boundary_plus.f() - diy_fp.f() > diy_fp.f() - boundary_minus.f());
  CHECK((1 << 9) == diy_fp.f() - boundary_minus.f());  // NOLINT
  CHECK((1 << 10) == boundary_plus.f() - diy_fp.f());  // NOLINT

  uint64_t min_double64 = UINT64_2PART_C(0x00000000, 00000001);
  diy_fp = Double(min_double64).AsNormalizedDiyFp();
  Double(min_double64).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // min-value does not have a significand of the form 2^p (for some p).
  // Therefore its boundaries are at the same distance.
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  // Denormals have their boundaries much closer.
  CHECK((static_cast<uint64_t>(1) << 62) ==
        diy_fp.f() - boundary_minus.f());  // NOLINT

  uint64_t smallest_normal64 = UINT64_2PART_C(0x00100000, 00000000);
  diy_fp = Double(smallest_normal64).AsNormalizedDiyFp();
  Double(smallest_normal64).NormalizedBoundaries(&boundary_minus,
                                                 &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // Even though the significand is of the form 2^p (for some p), its boundaries
  // are at the same distance. (This is the only exception).
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 10) == diy_fp.f() - boundary_minus.f());  // NOLINT

  uint64_t largest_denormal64 = UINT64_2PART_C(0x000FFFFF, FFFFFFFF);
  diy_fp = Double(largest_denormal64).AsNormalizedDiyFp();
  Double(largest_denormal64).NormalizedBoundaries(&boundary_minus,
                                                  &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 11) == diy_fp.f() - boundary_minus.f());  // NOLINT

  uint64_t max_double64 = UINT64_2PART_C(0x7fefffff, ffffffff);
  diy_fp = Double(max_double64).AsNormalizedDiyFp();
  Double(max_double64).NormalizedBoundaries(&boundary_minus, &boundary_plus);
  CHECK_EQ(diy_fp.e(), boundary_minus.e());
  CHECK_EQ(diy_fp.e(), boundary_plus.e());
  // max-value does not have a significand of the form 2^p (for some p).
  // Therefore its boundaries are at the same distance.
  CHECK(diy_fp.f() - boundary_minus.f() == boundary_plus.f() - diy_fp.f());
  CHECK((1 << 10) == diy_fp.f() - boundary_minus.f());  // NOLINT
}


TEST(NextDouble) {
  CHECK_EQ(4e-324, Double(0.0).NextDouble());
  CHECK_EQ(0.0, Double(-0.0).NextDouble());
  CHECK_EQ(-0.0, Double(-4e-324).NextDouble());
  Double d0(-4e-324);
  Double d1(d0.NextDouble());
  Double d2(d1.NextDouble());
  CHECK_EQ(-0.0, d1.value());
  CHECK_EQ(0.0, d2.value());
  CHECK_EQ(4e-324, d2.NextDouble());
  CHECK_EQ(-1.7976931348623157e308, Double(-Double::Infinity()).NextDouble());
  CHECK_EQ(Double::Infinity(),
           Double(UINT64_2PART_C(0x7fefffff, ffffffff)).NextDouble());
}

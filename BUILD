# Bazel(http://bazel.io) BUILD file

licenses(["notice"])

cc_library(
  name = "double-conversion",
  visibility = ["//visibility:public"],
  hdrs = [
    "src/bignum-dtoa.h",
    "src/bignum.h",
    "src/cached-powers.h",
    "src/diy-fp.h",
    "src/double-conversion.h",
    "src/fast-dtoa.h",
    "src/fixed-dtoa.h",
    "src/ieee.h", 
    "src/strtod.h",
  ],
  srcs = [
    "src/bignum-dtoa.cc",
    "src/bignum.cc",
    "src/cached-powers.cc",
    "src/diy-fp.cc",
    "src/double-conversion.cc",
    "src/fast-dtoa.cc",
    "src/fixed-dtoa.cc",
    "src/strtod.cc",
    "src/utils.h"
  ],
  includes = [
    "src",
  ],
)

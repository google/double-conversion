// Copyright 2010 the V8 project authors. All rights reserved.
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
//       notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
//       copyright notice, this list of conditions and the following
//       disclaimer in the documentation and/or other materials provided
//       with the distribution.
//     * Neither the name of Google Inc. nor the names of its
//       contributors may be used to endorse or promote products derived
//       from this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <climits>
#include <locale>
#include <cmath>

#include "string-to-double.h"

#include "ieee.h"
#include "strtod.h"
#include "utils.h"

#ifdef _MSC_VER
#  if _MSC_VER >= 1900
// Fix MSVC >= 2015 (_MSC_VER == 1900) warning
// C4244: 'argument': conversion from 'const uc16' to 'char', possible loss of data
// against Advance and friends, when instantiated with **it as char, not uc16.
 __pragma(warning(disable: 4244))
#  endif
#  if _MSC_VER <= 1700 // VS2012, see IsDecimalDigitForRadix warning fix, below
#    define VS2012_RADIXWARN
#  endif
#endif

namespace double_conversion {

namespace {

// Widens an input character to its unsigned code-unit value. Symbol matching
// compares characters in this form, so that a 16-bit input character is never
// truncated into the range of the symbol byte it is compared against.
inline uint32_t CodeUnit(char ch) {
  return static_cast<unsigned char>(ch);
}

inline uint32_t CodeUnit(uc16 ch) {
  return ch;
}

inline uint32_t ToLower(uint32_t ch) {
  if (ch > 0x7F) return ch;
  static const std::ctype<char>& cType =
      std::use_facet<std::ctype<char> >(std::locale::classic());
  return static_cast<unsigned char>(cType.tolower(static_cast<char>(ch)));
}

inline uint32_t Pass(uint32_t ch) {
  return ch;
}

template <class Iterator, class Converter>
static inline bool ConsumeSubStringImpl(Iterator* current,
                                        Iterator end,
                                        const char* substring,
                                        Converter converter) {
  DOUBLE_CONVERSION_ASSERT(
      converter(CodeUnit(**current)) == converter(CodeUnit(*substring)));
  for (substring++; *substring != '\0'; substring++) {
    ++*current;
    if (*current == end ||
        converter(CodeUnit(**current)) != converter(CodeUnit(*substring))) {
      return false;
    }
  }
  ++*current;
  return true;
}

// Consumes the given substring from the iterator.
// Returns false, if the substring does not match.
template <class Iterator>
static bool ConsumeSubString(Iterator* current,
                             Iterator end,
                             const char* substring,
                             bool allow_case_insensitivity) {
  if (allow_case_insensitivity) {
    return ConsumeSubStringImpl(current, end, substring, ToLower);
  } else {
    return ConsumeSubStringImpl(current, end, substring, Pass);
  }
}

// Consumes first character of the str is equal to ch
template <class Char>
inline bool ConsumeFirstCharacter(Char ch,
                                         const char* str,
                                         bool case_insensitivity) {
  const uint32_t c = CodeUnit(ch);
  const uint32_t first = CodeUnit(str[0]);
  return case_insensitivity ? ToLower(c) == ToLower(first) : c == first;
}
}  // namespace

// Maximum number of significant digits in decimal representation.
// The longest possible double in decimal representation is
// (2^53 - 1) * 2 ^ -1074 that is (2 ^ 53 - 1) * 5 ^ 1074 / 10 ^ 1074
// (768 digits). If we parse a number whose first digits are equal to a
// mean of 2 adjacent doubles (that could have up to 769 digits) the result
// must be rounded to the bigger one unless the tail consists of zeros, so
// we don't need to preserve all the digits.
const int kMaxSignificantDigits = 772;


static const char kWhitespaceTable7[] = { 32, 13, 10, 9, 11, 12 };
static const int kWhitespaceTable7Length = DOUBLE_CONVERSION_ARRAY_SIZE(kWhitespaceTable7);


static const uc16 kWhitespaceTable16[] = {
  160, 8232, 8233, 5760, 6158, 8192, 8193, 8194, 8195,
  8196, 8197, 8198, 8199, 8200, 8201, 8202, 8239, 8287, 12288, 65279
};
static const int kWhitespaceTable16Length = DOUBLE_CONVERSION_ARRAY_SIZE(kWhitespaceTable16);


static bool isWhitespace(int x) {
  if (x < 128) {
    for (int i = 0; i < kWhitespaceTable7Length; i++) {
      if (kWhitespaceTable7[i] == x) return true;
    }
  } else {
    for (int i = 0; i < kWhitespaceTable16Length; i++) {
      if (kWhitespaceTable16[i] == x) return true;
    }
  }
  return false;
}


// Returns true if a nonspace found and false if the end has reached.
template <class Iterator>
static inline bool AdvanceToNonspace(Iterator* current, Iterator end) {
  while (*current != end) {
    if (!isWhitespace(**current)) return true;
    ++*current;
  }
  return false;
}


static bool isDigit(int x, int radix) {
  return (x >= '0' && x <= '9' && x < '0' + radix)
      || (radix > 10 && x >= 'a' && x < 'a' + radix - 10)
      || (radix > 10 && x >= 'A' && x < 'A' + radix - 10);
}


static double SignedZero(bool sign) {
  return sign ? -0.0 : 0.0;
}


// Returns true if 'c' is a decimal digit that is valid for the given radix.
//
// The function is small and could be inlined, but VS2012 emitted a warning
// because it constant-propagated the radix and concluded that the last
// condition was always true. Moving it into a separate function and
// suppressing optimisation keeps the compiler from warning.
#ifdef VS2012_RADIXWARN
#pragma optimize("",off)
static bool IsDecimalDigitForRadix(int c, int radix) {
  return '0' <= c && c <= '9' && (c - '0') < radix;
}
#pragma optimize("",on)
#else
static bool inline IsDecimalDigitForRadix(int c, int radix) {
  return '0' <= c && c <= '9' && (c - '0') < radix;
}
#endif
// Returns true if 'c' is a character digit that is valid for the given radix.
// The 'a_character' should be 'a' or 'A'.
//
// The function is small and could be inlined, but VS2012 emitted a warning
// because it constant-propagated the radix and concluded that the first
// condition was always false. By moving it into a separate function the
// compiler wouldn't warn anymore.
static bool IsCharacterDigitForRadix(int c, int radix, char a_character) {
  return radix > 10 && c >= a_character && c < a_character + radix - 10;
}

// Returns true, when the iterator is equal to end.
template<class Iterator>
static bool Advance (Iterator* it, uc16 separator, int base, Iterator& end) {
  if (separator == StringToDoubleConverter::kNoSeparator) {
    ++(*it);
    return *it == end;
  }
  if (!isDigit(**it, base)) {
    ++(*it);
    return *it == end;
  }
  ++(*it);
  if (*it == end) return true;
  if (*it + 1 == end) return false;
  if (**it == separator && isDigit(*(*it + 1), base)) {
    ++(*it);
  }
  return *it == end;
}

// Checks whether the string in the range start-end is a hex-float string.
// This function assumes that the leading '0x'/'0X' is already consumed.
//
// Hex float strings are of one of the following forms:
//   - hex_digits+ 'p' ('+'|'-')? exponent_digits+
//   - hex_digits* '.' hex_digits+ 'p' ('+'|'-')? exponent_digits+
//   - hex_digits+ '.' 'p' ('+'|'-')? exponent_digits+
template<class Iterator>
static bool IsHexFloatString(Iterator start,
                             Iterator end,
                             uc16 separator,
                             bool allow_trailing_junk,
                             bool allow_trailing_spaces) {
  DOUBLE_CONVERSION_ASSERT(start != end);

  Iterator current = start;

  bool saw_digit = false;
  while (isDigit(*current, 16)) {
    saw_digit = true;
    if (Advance(&current, separator, 16, end)) return false;
  }
  if (*current == '.') {
    if (Advance(&current, separator, 16, end)) return false;
    while (isDigit(*current, 16)) {
      saw_digit = true;
      if (Advance(&current, separator, 16, end)) return false;
    }
  }
  if (!saw_digit) return false;
  if (*current != 'p' && *current != 'P') return false;
  // The separator is only allowed between significand digits, not in the
  // exponent, so advance through the exponent with no separator.
  const uc16 kNoSeparator = StringToDoubleConverter::kNoSeparator;
  if (Advance(&current, kNoSeparator, 16, end)) return false;
  if (*current == '+' || *current == '-') {
    if (Advance(&current, kNoSeparator, 16, end)) return false;
  }
  if (!isDigit(*current, 10)) return false;
  if (Advance(&current, kNoSeparator, 16, end)) return true;
  while (isDigit(*current, 10)) {
    if (Advance(&current, kNoSeparator, 16, end)) return true;
  }
  // Trailing whitespace is junk unless ALLOW_TRAILING_SPACES is set, as it is
  // for decimal numbers.
  if (allow_trailing_junk) return true;
  return allow_trailing_spaces && !AdvanceToNonspace(&current, end);
}


// Parsing integers with radix 2, 4, 8, 16, 32. Assumes current != end.
//
// If parse_as_hex_float is true, then the string must be a valid
// hex-float.
template <int radix_log_2, class Iterator>
static double RadixStringToIeee(Iterator* current,
                                Iterator end,
                                bool sign,
                                uc16 separator,
                                bool parse_as_hex_float,
                                bool allow_trailing_junk,
                                bool allow_trailing_spaces,
                                double junk_string_value,
                                bool read_as_double,
                                bool* result_is_junk) {
  DOUBLE_CONVERSION_ASSERT(*current != end);
  DOUBLE_CONVERSION_ASSERT(!parse_as_hex_float ||
      IsHexFloatString(*current, end, separator, allow_trailing_junk,
                       allow_trailing_spaces));

  const int kDoubleSize = Double::kSignificandSize;
  const int kSingleSize = Single::kSignificandSize;
  // A hex-float is formed here as a double and rounded to float by the caller
  // (StringToFloat casts the result). Rounding the significand to single
  // precision here would double-round both subnormal floats and floats whose
  // exact significand exceeds 53 bits, so keep the full double significand and
  // round it to odd, which makes that final single-precision cast correct.
  const bool round_hex_float_to_single = parse_as_hex_float && !read_as_double;
  const int kSignificandSize =
      (read_as_double || parse_as_hex_float) ? kDoubleSize : kSingleSize;

  *result_is_junk = true;

  int64_t number = 0;
  int exponent = 0;
  const int radix = (1 << radix_log_2);
  // Whether we have encountered a '.' and are parsing the decimal digits.
  // Only relevant if parse_as_hex_float is true.
  bool post_decimal = false;

  // Skip leading 0s.
  while (**current == '0') {
    if (Advance(current, separator, radix, end)) {
      *result_is_junk = false;
      return SignedZero(sign);
    }
  }

  while (true) {
    int digit;
    if (IsDecimalDigitForRadix(**current, radix)) {
      digit = static_cast<char>(**current) - '0';
      if (post_decimal) exponent -= radix_log_2;
    } else if (IsCharacterDigitForRadix(**current, radix, 'a')) {
      digit = static_cast<char>(**current) - 'a' + 10;
      if (post_decimal) exponent -= radix_log_2;
    } else if (IsCharacterDigitForRadix(**current, radix, 'A')) {
      digit = static_cast<char>(**current) - 'A' + 10;
      if (post_decimal) exponent -= radix_log_2;
    } else if (parse_as_hex_float && **current == '.') {
      post_decimal = true;
      Advance(current, separator, radix, end);
      DOUBLE_CONVERSION_ASSERT(*current != end);
      continue;
    } else if (parse_as_hex_float && (**current == 'p' || **current == 'P')) {
      break;
    } else {
      // Trailing whitespace is junk unless ALLOW_TRAILING_SPACES is set, as it
      // is for decimal numbers.
      if (allow_trailing_junk ||
          (allow_trailing_spaces && !AdvanceToNonspace(current, end))) {
        break;
      } else {
        return junk_string_value;
      }
    }

    number = number * radix + digit;
    int overflow = static_cast<int>(number >> kSignificandSize);
    if (overflow != 0) {
      // Overflow occurred. Need to determine which direction to round the
      // result.
      int overflow_bits_count = 1;
      while (overflow > 1) {
        overflow_bits_count++;
        overflow >>= 1;
      }

      int dropped_bits_mask = ((1 << overflow_bits_count) - 1);
      int dropped_bits = static_cast<int>(number) & dropped_bits_mask;
      number >>= overflow_bits_count;
      exponent += overflow_bits_count;

      bool zero_tail = true;
      for (;;) {
        if (Advance(current, separator, radix, end)) break;
        if (parse_as_hex_float && **current == '.') {
          // Just run over the '.'. We are just trying to see whether there is
          // a non-zero digit somewhere.
          Advance(current, separator, radix, end);
          DOUBLE_CONVERSION_ASSERT(*current != end);
          post_decimal = true;
        }
        if (!isDigit(**current, radix)) break;
        zero_tail = zero_tail && **current == '0';
        if (!post_decimal) {
          if (exponent <= INT_MAX - radix_log_2) {
            exponent += radix_log_2;
          } else {
            exponent = INT_MAX;
          }
        }
      }

      if (!parse_as_hex_float && !allow_trailing_junk) {
        if (allow_trailing_spaces ? AdvanceToNonspace(current, end)
                                  : *current != end) {
          return junk_string_value;
        }
      }

      if (round_hex_float_to_single) {
        // Round the significand to odd: set the lowest kept bit whenever any
        // bit was dropped. The caller rounds this double to float; rounding to
        // nearest here would double-round, but round-to-odd leaves that final
        // single rounding correct for normal and subnormal results alike.
        if (dropped_bits != 0 || !zero_tail) {
          number |= 1;
        }
      } else {
        int middle_value = (1 << (overflow_bits_count - 1));
        if (dropped_bits > middle_value) {
          number++;  // Rounding up.
        } else if (dropped_bits == middle_value) {
          // Rounding to even to consistency with decimals: half-way case rounds
          // up if significant part is odd and down otherwise.
          if ((number & 1) != 0 || !zero_tail) {
            number++;  // Rounding up.
          }
        }

        // Rounding up may cause overflow.
        if ((number & ((int64_t)1 << kSignificandSize)) != 0) {
          exponent++;
          number >>= 1;
        }
      }
      break;
    }
    if (Advance(current, separator, radix, end)) break;
  }

  DOUBLE_CONVERSION_ASSERT(number < ((int64_t)1 << kSignificandSize));
  DOUBLE_CONVERSION_ASSERT(static_cast<int64_t>(static_cast<double>(number)) == number);

  *result_is_junk = false;

  if (parse_as_hex_float) {
    DOUBLE_CONVERSION_ASSERT(**current == 'p' || **current == 'P');
    // The separator is only allowed between significand digits, not in the
    // exponent, so advance through the exponent with no separator. This must
    // match IsHexFloatString, which validated the string the same way.
    const uc16 kNoSeparator = StringToDoubleConverter::kNoSeparator;
    Advance(current, kNoSeparator, radix, end);
    DOUBLE_CONVERSION_ASSERT(*current != end);
    bool is_negative = false;
    if (**current == '+') {
      Advance(current, kNoSeparator, radix, end);
      DOUBLE_CONVERSION_ASSERT(*current != end);
    } else if (**current == '-') {
      is_negative = true;
      Advance(current, kNoSeparator, radix, end);
      DOUBLE_CONVERSION_ASSERT(*current != end);
    }
    int written_exponent = 0;
    while (IsDecimalDigitForRadix(**current, 10)) {
      // No need to read exponents if they are too big. That could potentially overflow
      // the `written_exponent` variable.
      if (abs(written_exponent) <= 100 * Double::kMaxExponent) {
        written_exponent = 10 * written_exponent + **current - '0';
      }
      if (Advance(current, kNoSeparator, radix, end)) break;
    }
    if (is_negative) written_exponent = -written_exponent;
    exponent += written_exponent;
  }

  if (exponent == 0 || number == 0) {
    if (sign) {
      if (number == 0) return -0.0;
      number = -number;
    }
    return static_cast<double>(number);
  }

  DOUBLE_CONVERSION_ASSERT(number != 0);
  // number is an exact integer below 2^kSignificandSize, so number * 2^exponent
  // can be formed directly. Double(DiyFp(number, exponent)) would instead assume
  // a normalized significand: a hex-float like "0x1p1000" or "0x2p-1075" reaches
  // here with a small number and a large exponent, which DiyFpToUint64 then reads
  // as an overflow (infinity) or underflow (zero) rather than the finite result.
  double result = ldexp(static_cast<double>(number), exponent);
  return sign ? -result : result;
}

template <class Iterator>
double StringToDoubleConverter::StringToIeee(
    Iterator input,
    int length,
    bool read_as_double,
    int* processed_characters_count) const {
  Iterator current = input;
  Iterator end = input + length;

  *processed_characters_count = 0;

  const bool allow_trailing_junk = (flags_ & ALLOW_TRAILING_JUNK) != 0;
  const bool allow_leading_spaces = (flags_ & ALLOW_LEADING_SPACES) != 0;
  const bool allow_trailing_spaces = (flags_ & ALLOW_TRAILING_SPACES) != 0;
  const bool allow_spaces_after_sign = (flags_ & ALLOW_SPACES_AFTER_SIGN) != 0;
  const bool allow_case_insensitivity = (flags_ & ALLOW_CASE_INSENSITIVITY) != 0;

  // To make sure that iterator dereferencing is valid the following
  // convention is used:
  // 1. Each '++current' statement is followed by check for equality to 'end'.
  // 2. If AdvanceToNonspace returned false then current == end.
  // 3. If 'current' becomes equal to 'end' the function returns or goes to
  // 'parsing_done'.
  // 4. 'current' is not dereferenced after the 'parsing_done' label.
  // 5. Code before 'parsing_done' may rely on 'current != end'.
  if (current == end) return empty_string_value_;

  if (allow_leading_spaces || allow_trailing_spaces) {
    if (!AdvanceToNonspace(&current, end)) {
      *processed_characters_count = static_cast<int>(current - input);
      return empty_string_value_;
    }
    if (!allow_leading_spaces && (input != current)) {
      // No leading spaces allowed, but AdvanceToNonspace moved forward.
      return junk_string_value_;
    }
  }

  // Exponent will be adjusted if insignificant digits of the integer part
  // or insignificant leading zeros of the fractional part are dropped.
  int exponent = 0;
  // Leading fractional zeros and dropped integer digits are both moved into the
  // exponent, and both are bounded only by the input length. Saturating the
  // accumulation at this magnitude keeps it inside int; any exponent this large
  // is far outside the double range, so the clamped result is unchanged.
  const int max_exponent = INT_MAX / 2;
  int significant_digits = 0;
  int insignificant_digits = 0;
  bool nonzero_digit_dropped = false;

  bool sign = false;

  if (*current == '+' || *current == '-') {
    sign = (*current == '-');
    ++current;
    Iterator next_non_space = current;
    // Skip following spaces (if allowed).
    if (!AdvanceToNonspace(&next_non_space, end)) return junk_string_value_;
    if (!allow_spaces_after_sign && (current != next_non_space)) {
      return junk_string_value_;
    }
    current = next_non_space;
  }

  if (infinity_symbol_ != DOUBLE_CONVERSION_NULLPTR) {
    if (ConsumeFirstCharacter(*current, infinity_symbol_, allow_case_insensitivity)) {
      if (!ConsumeSubString(&current, end, infinity_symbol_, allow_case_insensitivity)) {
        return junk_string_value_;
      }

      if (!(allow_trailing_spaces || allow_trailing_junk) && (current != end)) {
        return junk_string_value_;
      }
      if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
        return junk_string_value_;
      }

      *processed_characters_count = static_cast<int>(current - input);
      return sign ? -Double::Infinity() : Double::Infinity();
    }
  }

  if (nan_symbol_ != DOUBLE_CONVERSION_NULLPTR) {
    if (ConsumeFirstCharacter(*current, nan_symbol_, allow_case_insensitivity)) {
      if (!ConsumeSubString(&current, end, nan_symbol_, allow_case_insensitivity)) {
        return junk_string_value_;
      }

      if (!(allow_trailing_spaces || allow_trailing_junk) && (current != end)) {
        return junk_string_value_;
      }
      if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
        return junk_string_value_;
      }

      *processed_characters_count = static_cast<int>(current - input);
      return sign ? -Double::NaN() : Double::NaN();
    }
  }

  bool leading_zero = false;
  if (*current == '0') {
    if (Advance(&current, separator_, 10, end)) {
      *processed_characters_count = static_cast<int>(current - input);
      return SignedZero(sign);
    }

    leading_zero = true;

    // It could be hexadecimal value.
    if (((flags_ & ALLOW_HEX) || (flags_ & ALLOW_HEX_FLOATS)) &&
        (*current == 'x' || *current == 'X')) {
      ++current;

      if (current == end) return junk_string_value_;  // "0x"

      bool parse_as_hex_float = (flags_ & ALLOW_HEX_FLOATS) &&
                IsHexFloatString(current, end, separator_, allow_trailing_junk,
                                 allow_trailing_spaces);

      if (!parse_as_hex_float && !isDigit(*current, 16)) {
        return junk_string_value_;
      }

      bool result_is_junk;
      double result = RadixStringToIeee<4>(&current,
                                           end,
                                           sign,
                                           separator_,
                                           parse_as_hex_float,
                                           allow_trailing_junk,
                                           allow_trailing_spaces,
                                           junk_string_value_,
                                           read_as_double,
                                           &result_is_junk);
      if (!result_is_junk) {
        if (allow_trailing_spaces) AdvanceToNonspace(&current, end);
        *processed_characters_count = static_cast<int>(current - input);
      }
      return result;
    }

    // Ignore leading zeros in the integer part.
    while (*current == '0') {
      if (Advance(&current, separator_, 10, end)) {
        *processed_characters_count = static_cast<int>(current - input);
        return SignedZero(sign);
      }
    }
  }

  bool octal = leading_zero && (flags_ & ALLOW_OCTALS) != 0;

  // The longest form of simplified number is: "-<significant digits>.1eXXX\0".
  const int kBufferSize = kMaxSignificantDigits + 10;
  DOUBLE_CONVERSION_STACK_UNINITIALIZED char
      buffer[kBufferSize];  // NOLINT: size is known at compile time.
  int buffer_pos = 0;

  // Copy significant digits of the integer part (if any) to the buffer.
  while (*current >= '0' && *current <= '9') {
    if (significant_digits < kMaxSignificantDigits) {
      DOUBLE_CONVERSION_ASSERT(buffer_pos < kBufferSize);
      buffer[buffer_pos++] = static_cast<char>(*current);
      significant_digits++;
      // Will later check if it's an octal in the buffer.
    } else {
      insignificant_digits++;  // Move the digit into the exponential part.
      nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
    }
    octal = octal && *current < '8';
    if (Advance(&current, separator_, 10, end)) goto parsing_done;
  }

  if (significant_digits == 0) {
    octal = false;
  }

  if (*current == '.') {
    if (octal && !allow_trailing_junk) return junk_string_value_;
    if (octal) goto parsing_done;

    if (Advance(&current, separator_, 10, end)) {
      if (significant_digits == 0 && !leading_zero) {
        return junk_string_value_;
      } else {
        goto parsing_done;
      }
    }

    if (significant_digits == 0) {
      // octal = false;
      // Integer part consists of 0 or is absent. Significant digits start after
      // leading zeros (if any).
      while (*current == '0') {
        if (Advance(&current, separator_, 10, end)) {
          *processed_characters_count = static_cast<int>(current - input);
          return SignedZero(sign);
        }
        // Saturate to avoid underflow on a pathologically long zero run.
        if (exponent > -(max_exponent / 2)) exponent--;  // Move this 0 into the exponent.
      }
    }

    // There is a fractional part.
    // We don't emit a '.', but adjust the exponent instead.
    while (*current >= '0' && *current <= '9') {
      if (significant_digits < kMaxSignificantDigits) {
        DOUBLE_CONVERSION_ASSERT(buffer_pos < kBufferSize);
        buffer[buffer_pos++] = static_cast<char>(*current);
        significant_digits++;
        if (exponent > -(max_exponent / 2)) exponent--;
      } else {
        // Ignore insignificant digits in the fractional part.
        nonzero_digit_dropped = nonzero_digit_dropped || *current != '0';
      }
      if (Advance(&current, separator_, 10, end)) goto parsing_done;
    }
  }

  if (!leading_zero && exponent == 0 && significant_digits == 0) {
    // If leading_zeros is true then the string contains zeros.
    // If exponent < 0 then string was [+-]\.0*...
    // If significant_digits != 0 the string is not equal to 0.
    // Otherwise there are no digits in the string.
    return junk_string_value_;
  }

  // Parse exponential part.
  if (*current == 'e' || *current == 'E') {
    if (octal && !allow_trailing_junk) return junk_string_value_;
    if (octal) goto parsing_done;
    Iterator junk_begin = current;
    ++current;
    if (current == end) {
      if (allow_trailing_junk) {
        current = junk_begin;
        goto parsing_done;
      } else {
        return junk_string_value_;
      }
    }
    char exponen_sign = '+';
    if (*current == '+' || *current == '-') {
      exponen_sign = static_cast<char>(*current);
      ++current;
      if (current == end) {
        if (allow_trailing_junk) {
          current = junk_begin;
          goto parsing_done;
        } else {
          return junk_string_value_;
        }
      }
    }

    if (current == end || *current < '0' || *current > '9') {
      if (allow_trailing_junk) {
        current = junk_begin;
        goto parsing_done;
      } else {
        return junk_string_value_;
      }
    }

    DOUBLE_CONVERSION_ASSERT(-max_exponent / 2 <= exponent && exponent <= max_exponent / 2);
    int num = 0;
    do {
      // Check overflow.
      int digit = *current - '0';
      if (num >= max_exponent / 10
          && !(num == max_exponent / 10 && digit <= max_exponent % 10)) {
        num = max_exponent;
      } else {
        num = num * 10 + digit;
      }
      ++current;
    } while (current != end && *current >= '0' && *current <= '9');

    exponent += (exponen_sign == '-' ? -num : num);
  }

  if (!(allow_trailing_spaces || allow_trailing_junk) && (current != end)) {
    return junk_string_value_;
  }
  if (!allow_trailing_junk && AdvanceToNonspace(&current, end)) {
    return junk_string_value_;
  }
  if (allow_trailing_spaces) {
    AdvanceToNonspace(&current, end);
  }

  parsing_done:
  // insignificant_digits counts integer digits dropped past the significand
  // limit and is bounded only by the input length, so exponent + it can exceed
  // int. Saturate: such a value is out of the double range regardless.
  {
    const int64_t combined =
        static_cast<int64_t>(exponent) + insignificant_digits;
    exponent = combined > max_exponent ? max_exponent
                                       : static_cast<int>(combined);
  }

  if (octal) {
    double result;
    bool result_is_junk;
    char* start = buffer;
    result = RadixStringToIeee<3>(&start,
                                  buffer + buffer_pos,
                                  sign,
                                  separator_,
                                  false, // Don't parse as hex_float.
                                  allow_trailing_junk,
                                  allow_trailing_spaces,
                                  junk_string_value_,
                                  read_as_double,
                                  &result_is_junk);
    DOUBLE_CONVERSION_ASSERT(!result_is_junk);
    *processed_characters_count = static_cast<int>(current - input);
    return result;
  }

  if (nonzero_digit_dropped) {
    buffer[buffer_pos++] = '1';
    exponent--;
  }

  DOUBLE_CONVERSION_ASSERT(buffer_pos < kBufferSize);
  buffer[buffer_pos] = '\0';

  // Code above ensures there are no leading zeros and the buffer has fewer than
  // kMaxSignificantDecimalDigits characters. Trim trailing zeros.
  Vector<const char> chars(buffer, buffer_pos);
  chars = TrimTrailingZeros(chars);
  exponent += buffer_pos - chars.length();

  double converted;
  if (read_as_double) {
    converted = StrtodTrimmed(chars, exponent);
  } else {
    converted = StrtofTrimmed(chars, exponent);
  }
  *processed_characters_count = static_cast<int>(current - input);
  return sign? -converted: converted;
}


double StringToDoubleConverter::StringToDouble(
    const char* buffer,
    int length,
    int* processed_characters_count) const {
  return StringToIeee(buffer, length, true, processed_characters_count);
}


double StringToDoubleConverter::StringToDouble(
    const uc16* buffer,
    int length,
    int* processed_characters_count) const {
  return StringToIeee(buffer, length, true, processed_characters_count);
}


float StringToDoubleConverter::StringToFloat(
    const char* buffer,
    int length,
    int* processed_characters_count) const {
  return static_cast<float>(StringToIeee(buffer, length, false,
                                         processed_characters_count));
}


float StringToDoubleConverter::StringToFloat(
    const uc16* buffer,
    int length,
    int* processed_characters_count) const {
  return static_cast<float>(StringToIeee(buffer, length, false,
                                         processed_characters_count));
}


template<>
double StringToDoubleConverter::StringTo<double>(
    const char* buffer,
    int length,
    int* processed_characters_count) const {
    return StringToDouble(buffer, length, processed_characters_count);
}


template<>
float StringToDoubleConverter::StringTo<float>(
    const char* buffer,
    int length,
    int* processed_characters_count) const {
    return StringToFloat(buffer, length, processed_characters_count);
}


template<>
double StringToDoubleConverter::StringTo<double>(
    const uc16* buffer,
    int length,
    int* processed_characters_count) const {
    return StringToDouble(buffer, length, processed_characters_count);
}


template<>
float StringToDoubleConverter::StringTo<float>(
    const uc16* buffer,
    int length,
    int* processed_characters_count) const {
    return StringToFloat(buffer, length, processed_characters_count);
}

}  // namespace double_conversion

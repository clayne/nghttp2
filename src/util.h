/*
 * nghttp2 - HTTP/2 C Library
 *
 * Copyright (c) 2012 Tatsuhiro Tsujikawa
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
#ifndef UTIL_H
#define UTIL_H

#include "nghttp2_config.h"

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif // HAVE_UNISTD_H
#include <getopt.h>
#ifdef HAVE_NETDB_H
#  include <netdb.h>
#endif // HAVE_NETDB_H
#ifdef __QNX__
#  include <sys/time.h>
#endif // __QNX__

#include <cmath>
#include <cstring>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <sstream>
#include <memory>
#include <chrono>
#include <map>
#include <random>
#include <optional>
#include <ranges>

#ifdef HAVE_LIBEV
#  include <ev.h>
#endif // HAVE_LIBEV

#include "urlparse.h"

#include "template.h"
#include "network.h"
#include "allocator.h"

namespace nghttp2 {

constexpr auto NGHTTP2_H2_ALPN = "\x2h2"_sr;
constexpr auto NGHTTP2_H2 = "h2"_sr;

// The additional HTTP/2 protocol ALPN protocol identifier we also
// supports for our applications to make smooth migration into final
// h2 ALPN ID.
constexpr auto NGHTTP2_H2_16_ALPN = "\x5h2-16"_sr;
constexpr auto NGHTTP2_H2_16 = "h2-16"_sr;

constexpr auto NGHTTP2_H2_14_ALPN = "\x5h2-14"_sr;
constexpr auto NGHTTP2_H2_14 = "h2-14"_sr;

constexpr auto NGHTTP2_H1_1_ALPN = "\x8http/1.1"_sr;
constexpr auto NGHTTP2_H1_1 = "http/1.1"_sr;

constexpr size_t NGHTTP2_MAX_UINT64_DIGITS = str_size("18446744073709551615");

namespace util {

extern const char UPPER_XDIGITS[];

constexpr bool is_alpha(const char c) noexcept {
  return ('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z');
}

constexpr bool is_digit(const char c) noexcept { return '0' <= c && c <= '9'; }

constexpr bool is_hex_digit(const char c) noexcept {
  return is_digit(c) || ('A' <= c && c <= 'F') || ('a' <= c && c <= 'f');
}

// Returns true if a range [|first|, |last|) is hex string.
template <std::input_iterator I> constexpr bool is_hex_string(I first, I last) {
  if (std::ranges::distance(first, last) % 2) {
    return false;
  }

  for (; first != last; ++first) {
    if (!is_hex_digit(*first)) {
      return false;
    }
  }

  return true;
}

// Returns true if |r| is hex string.
template <std::ranges::input_range R> constexpr bool is_hex_string(R &&r) {
  return is_hex_string(std::ranges::begin(r), std::ranges::end(r));
}

bool in_rfc3986_unreserved_chars(const char c);

bool in_rfc3986_sub_delims(const char c);

// Returns true if |c| is in token (HTTP-p1, Section 3.2.6)
constexpr bool in_token(char c) noexcept {
  switch (c) {
  case '!':
  case '#':
  case '$':
  case '%':
  case '&':
  case '\'':
  case '*':
  case '+':
  case '-':
  case '.':
  case '^':
  case '_':
  case '`':
  case '|':
  case '~':
    return true;
  }

  return is_alpha(c) || is_digit(c);
}

bool in_attr_char(char c);

constinit const auto hex_to_uint_tbl = []() {
  std::array<uint32_t, 256> tbl;

  std::ranges::fill(tbl, 256);

  for (char i = '0'; i <= '9'; ++i) {
    tbl[static_cast<uint8_t>(i)] = i - '0';
  }

  for (char i = 'A'; i <= 'F'; ++i) {
    tbl[static_cast<uint8_t>(i)] = i - 'A' + 10;
  }

  for (char i = 'a'; i <= 'f'; ++i) {
    tbl[static_cast<uint8_t>(i)] = i - 'a' + 10;
  }

  return tbl;
}();

// Returns integer corresponding to hex notation |c|.  If
// is_hex_digit(c) is false, it returns 256.
constexpr uint32_t hex_to_uint(char c) noexcept {
  return hex_to_uint_tbl[static_cast<uint8_t>(c)];
}

template <std::input_iterator I, std::weakly_incrementable O>
constexpr O percent_decode(I first, I last, O result) {
  for (; first != last; ++first) {
    if (*first != '%') {
      *result++ = *first;
      continue;
    }

    auto dig1 = std::ranges::next(first, 1);
    if (dig1 == last || !is_hex_digit(*dig1)) {
      *result++ = *first;
      continue;
    }

    auto dig2 = std::ranges::next(dig1, 1);
    if (dig2 == last || !is_hex_digit(*dig2)) {
      *result++ = *first;
      continue;
    }

    *result++ = (hex_to_uint(*dig1) << 4) | hex_to_uint(*dig2);

    first = dig2;
  }

  return result;
}

template <std::input_iterator I>
constexpr std::string percent_decode(I first, I last) {
  std::string result;

  result.resize(std::ranges::distance(first, last));

  auto p = percent_decode(std::move(first), std::move(last),
                          std::ranges::begin(result));

  result.resize(std::ranges::distance(std::ranges::begin(result), p));

  return result;
}

template <std::ranges::input_range R>
constexpr std::string percent_decode(R &&r) {
  return percent_decode(std::ranges::begin(r), std::ranges::end(r));
}

template <std::ranges::input_range R>
StringRef percent_decode(BlockAllocator &balloc, R &&r) {
  auto iov = make_byte_ref(balloc, std::ranges::distance(r) + 1);

  auto p = percent_decode(std::ranges::begin(r), std::ranges::end(r),
                          std::ranges::begin(iov));

  *p = '\0';

  return as_string_ref(std::ranges::begin(iov), p);
}

// Percent encode a range [|first|, |last|) if a character is not in
// token or '%'.
template <std::input_iterator I, std::weakly_incrementable O>
constexpr O percent_encode_token(I first, I last, O result) noexcept {
  for (; first != last; ++first) {
    uint8_t c = *first;

    if (c != '%' && in_token(c)) {
      *result++ = c;
      continue;
    }

    *result++ = '%';
    *result++ = UPPER_XDIGITS[c >> 4];
    *result++ = UPPER_XDIGITS[(c & 0x0f)];
  }

  return result;
}

template <std::ranges::input_range R, std::weakly_incrementable O>
constexpr O percent_encode_token(R &&r, O result) {
  return percent_encode_token(std::ranges::begin(r), std::ranges::end(r),
                              std::move(result));
}

// Returns the number of bytes written by percent_encode_token with
// the same |r| parameter.  The return value does not include a
// terminal NUL byte.
template <std::ranges::input_range R>
constexpr size_t percent_encode_tokenlen(R &&r) noexcept {
  size_t n = 0;

  for (auto c : r) {
    if (c != '%' && in_token(c)) {
      ++n;
      continue;
    }

    // percent-encoded character '%ff'
    n += 3;
  }

  return n;
}

// Quote a range [|first|, |last|) and stores the result in another
// range, beginning at |result|.  It returns an output iterator to the
// element past the last element stored.  Currently, this function
// just replace '"' with '\"'.
template <std::input_iterator I, std::weakly_incrementable O>
constexpr O quote_string(I first, I last, O result) noexcept {
  for (; first != last; ++first) {
    if (*first == '"') {
      *result++ = '\\';
      *result++ = '"';
    } else {
      *result++ = *first;
    }
  }

  return result;
}

template <std::ranges::input_range R, std::weakly_incrementable O>
constexpr O quote_string(R &&r, O result) {
  return quote_string(std::ranges::begin(r), std::ranges::end(r),
                      std::move(result));
}

template <std::ranges::input_range R>
StringRef quote_string(BlockAllocator &balloc, R &&r) {
  auto cnt = std::ranges::count(r, '"');

  if (cnt == 0) {
    return make_string_ref(balloc, std::forward<R>(r));
  }

  auto iov = make_byte_ref(balloc, std::ranges::distance(r) + cnt + 1);
  auto p = quote_string(std::forward<R>(r), std::ranges::begin(iov));

  *p = '\0';

  return as_string_ref(std::ranges::begin(iov), p);
}

// Returns the number of bytes written by quote_string with the same
// |r| parameter.  The return value does not include a terminal NUL
// byte.
template <std::ranges::input_range R> constexpr size_t quote_stringlen(R &&r) {
  size_t n = 0;

  for (auto c : r) {
    if (c == '"') {
      n += 2;
    } else {
      ++n;
    }
  }

  return n;
}

static constexpr char LOWER_XDIGITS[] = "0123456789abcdef";

// Converts a range [|first|, |last|) in hex format, and stores the
// result in another range, beginning at |result|.  It returns an
// output iterator to the element past the last element stored.
template <std::input_iterator I, std::weakly_incrementable O>
requires(sizeof(std::iter_value_t<I>) == sizeof(uint8_t))
constexpr O format_hex(I first, I last, O result) noexcept {
  for (; first != last; ++first) {
    uint8_t c = *first;
    *result++ = LOWER_XDIGITS[c >> 4];
    *result++ = LOWER_XDIGITS[c & 0xf];
  }

  return result;
}

// Converts |R| in hex format, and stores the result in another range,
// beginning at |result|.  It returns an output iterator to the
// element past the last element stored.
template <std::ranges::input_range R, std::weakly_incrementable O>
requires(sizeof(std::ranges::range_value_t<R>) == sizeof(uint8_t))
constexpr O format_hex(R &&r, O result) noexcept {
  return format_hex(std::ranges::begin(r), std::ranges::end(r),
                    std::move(result));
}

// Converts |R| in hex format, and stores the result in a buffer
// allocated by |balloc|.  It returns StringRef that is backed by the
// allocated buffer.  The returned string is NULL terminated.
template <std::ranges::input_range R>
requires(sizeof(std::ranges::range_value_t<R>) == sizeof(uint8_t))
StringRef format_hex(BlockAllocator &balloc, R &&r) {
  auto iov = make_byte_ref(balloc, std::ranges::distance(r) * 2 + 1);
  auto p = format_hex(std::forward<R>(r), std::ranges::begin(iov));

  *p = '\0';

  return as_string_ref(std::ranges::begin(iov), p);
}

// Converts |R| in hex format, and returns the result.
template <std::ranges::input_range R>
requires(sizeof(std::ranges::range_value_t<R>) == sizeof(uint8_t))
constexpr std::string format_hex(R &&r) {
  std::string res;

  res.resize(std::ranges::distance(r) * 2);

  format_hex(std::forward<R>(r), std::ranges::begin(res));

  return res;
}

// decode_hex decodes hex string in a range [|first|, |last|), and
// stores the result in another range, beginning at |result|.  It
// returns an output iterator to the element past the last element
// stored.  This function assumes a range [|first|, |last|) is hex
// string, that is is_hex_string(|first|, |last|) == true.
template <std::input_iterator I, std::weakly_incrementable O>
constexpr O decode_hex(I first, I last, O result) {
  for (; first != last; first = std::ranges::next(first, 2)) {
    *result++ =
      (hex_to_uint(*first) << 4) | hex_to_uint(*std::ranges::next(first, 1));
  }

  return result;
}

// decode_hex decodes hex string |r|, and stores the result in another
// range, beginning at |result|.  It returns an output iterator to the
// element past the last element stored.  This function assumes |r| is
// hex string, that is is_hex_string(r) == true.
template <std::ranges::input_range R, std::weakly_incrementable O>
constexpr O decode_hex(R &&r, O result) {
  return decode_hex(std::ranges::begin(r), std::ranges::end(r),
                    std::move(result));
}

// decode_hex decodes hex string in a range [|first|, |last|), returns
// the decoded byte string, which is not NULL terminated.  This
// function assumes a range [|first|, |last|) is hex string, that is
// is_hex_string(|first|, |last|) == true.
template <std::input_iterator I>
std::span<const uint8_t> decode_hex(BlockAllocator &balloc, I first, I last) {
  auto iov = make_byte_ref(balloc, std::ranges::distance(first, last) / 2);
  auto p =
    decode_hex(std::move(first), std::move(last), std::ranges::begin(iov));

  return {std::ranges::begin(iov), p};
}

// decode_hex decodes hex string |r|, returns the decoded byte string,
// which is not NULL terminated.  This function assumes |r| is hex
// string, that is is_hex_string(r) == true.
template <std::ranges::input_range R>
std::span<const uint8_t> decode_hex(BlockAllocator &balloc, R &&r) {
  return decode_hex(balloc, std::ranges::begin(r), std::ranges::end(r));
}

// Returns given time |t| from epoch in HTTP Date format (e.g., Mon,
// 10 Oct 2016 10:25:58 GMT).
std::string http_date(time_t t);
// Writes given time |t| from epoch in HTTP Date format into the
// buffer pointed by |res|.  The buffer must be at least 29 bytes
// long.  This function returns the one beyond the last position.
char *http_date(char *res, time_t t);

// Returns given time |t| from epoch in Common Log format (e.g.,
// 03/Jul/2014:00:19:38 +0900)
std::string common_log_date(time_t t);
// Writes given time |t| from epoch in Common Log format into the
// buffer pointed by |res|.  The buffer must be at least 26 bytes
// long.  This function returns the one beyond the last position.
char *common_log_date(char *res, time_t t);

// Returns given millisecond |ms| from epoch in ISO 8601 format (e.g.,
// 2014-11-15T12:58:24.741Z or 2014-11-15T12:58:24.741+09:00)
std::string iso8601_date(int64_t ms);
// Writes given time |t| from epoch in ISO 8601 format into the buffer
// pointed by |res|.  The buffer must be at least 29 bytes long.  This
// function returns the one beyond the last position.
char *iso8601_date(char *res, int64_t ms);

// Writes given time |t| from epoch in ISO 8601 basic format into the
// buffer pointed by |res|.  The buffer must be at least 24 bytes
// long.  This function returns the one beyond the last position.
char *iso8601_basic_date(char *res, int64_t ms);

time_t parse_http_date(const StringRef &s);

// Parses time formatted as "MMM DD HH:MM:SS YYYY [GMT]" (e.g., Feb 3
// 00:55:52 2015 GMT), which is specifically used by OpenSSL
// ASN1_TIME_print().
time_t parse_openssl_asn1_time_print(const StringRef &s);

char upcase(char c);

static constexpr uint8_t lowcase_tbl[] = {
  0,   1,   2,   3,   4,   5,   6,   7,   8,   9,   10,  11,  12,  13,  14,
  15,  16,  17,  18,  19,  20,  21,  22,  23,  24,  25,  26,  27,  28,  29,
  30,  31,  32,  33,  34,  35,  36,  37,  38,  39,  40,  41,  42,  43,  44,
  45,  46,  47,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
  60,  61,  62,  63,  64,  'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j',
  'k', 'l', 'm', 'n', 'o', 'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y',
  'z', 91,  92,  93,  94,  95,  96,  97,  98,  99,  100, 101, 102, 103, 104,
  105, 106, 107, 108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
  120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131, 132, 133, 134,
  135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149,
  150, 151, 152, 153, 154, 155, 156, 157, 158, 159, 160, 161, 162, 163, 164,
  165, 166, 167, 168, 169, 170, 171, 172, 173, 174, 175, 176, 177, 178, 179,
  180, 181, 182, 183, 184, 185, 186, 187, 188, 189, 190, 191, 192, 193, 194,
  195, 196, 197, 198, 199, 200, 201, 202, 203, 204, 205, 206, 207, 208, 209,
  210, 211, 212, 213, 214, 215, 216, 217, 218, 219, 220, 221, 222, 223, 224,
  225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239,
  240, 241, 242, 243, 244, 245, 246, 247, 248, 249, 250, 251, 252, 253, 254,
  255,
};

constexpr char lowcase(char c) noexcept {
  return static_cast<char>(lowcase_tbl[static_cast<uint8_t>(c)]);
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
constexpr bool starts_with(R1 &&s, R2 &&prefix) {
  auto prefixlen = std::ranges::distance(prefix);
  return std::ranges::distance(s) >= prefixlen &&
         std::ranges::equal(std::views::take(std::forward<R1>(s), prefixlen),
                            std::forward<R2>(prefix));
}

struct CaseCmp {
  constexpr bool operator()(char lhs, char rhs) const noexcept {
    return lowcase(lhs) == lowcase(rhs);
  }
};

template <std::ranges::input_range R1, std::ranges::input_range R2>
constexpr bool istarts_with(R1 &&s, R2 &&prefix) {
  auto prefixlen = std::ranges::distance(prefix);
  return std::ranges::distance(s) >= prefixlen &&
         std::ranges::equal(std::views::take(std::forward<R1>(s), prefixlen),
                            std::forward<R2>(prefix), CaseCmp());
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
constexpr bool ends_with(R1 &&s, R2 &&suffix) {
  auto slen = std::ranges::distance(s);
  auto suffixlen = std::ranges::distance(suffix);
  return slen >= suffixlen &&
         std::ranges::equal(
           std::views::drop(std::forward<R1>(s), slen - suffixlen),
           std::forward<R2>(suffix));
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
constexpr bool iends_with(R1 &&s, R2 &&suffix) {
  auto slen = std::ranges::distance(s);
  auto suffixlen = std::ranges::distance(suffix);
  return slen >= suffixlen &&
         std::ranges::equal(
           std::views::drop(std::forward<R1>(s), slen - suffixlen),
           std::forward<R2>(suffix), CaseCmp());
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
constexpr bool strieq(R1 &&a, R2 &&b) {
  return std::ranges::equal(std::forward<R1>(a), std::forward<R2>(b),
                            CaseCmp());
}

template <std::ranges::input_range R1, std::ranges::input_range R2>
constexpr bool streq(R1 &&a, R2 &&b) {
  return std::ranges::equal(std::forward<R1>(a), std::forward<R2>(b));
}

template <typename InputIt> void inp_strlower(InputIt first, InputIt last) {
  std::transform(first, last, first, lowcase);
}

// Lowercase |s| in place.
inline void inp_strlower(std::string &s) {
  inp_strlower(std::begin(s), std::end(s));
}

// Converts characters in a range [|first|, |last|) to lowercase, and
// stores the result in another range, beginning at |result|.  It
// returns an output iterator to the element past the last element
// stored.
template <std::input_iterator I, std::weakly_incrementable O>
constexpr O tolower(I first, I last, O result) {
  return std::ranges::transform(std::move(first), std::move(last),
                                std::move(result), lowcase)
    .out;
}

// Converts characters in a range |r| to lowercase, and stores the
// result in another range, beginning at |result|.  It returns an
// output iterator to the element past the last element stored.
template <std::ranges::input_range R, std::weakly_incrementable O>
constexpr O tolower(R &&r, O result) {
  return std::ranges::transform(std::forward<R>(r), std::move(result), lowcase)
    .out;
}

// Returns string representation of |n| with 2 fractional digits.
std::string dtos(double n);

template <std::integral T> constexpr std::string utos(T n) {
  using namespace std::literals;

  if (n == 0) {
    return "0"s;
  }

  size_t nlen = 0;
  for (auto t = n; t; t /= 10, ++nlen)
    ;

  std::string res;

  res.resize(nlen);

  for (; n; n /= 10) {
    res[--nlen] = (n % 10) + '0';
  }

  return res;
}

template <std::integral T, std::weakly_incrementable O>
constexpr O utos(T n, O result) {
  if (n == 0) {
    *result++ = '0';
    return result;
  }

  size_t nlen = 0;

  for (auto t = n; t; t /= 10, ++nlen)
    ;

  result = std::ranges::next(result, nlen);
  auto p = result;

  for (; n; n /= 10) {
    *--p = (n % 10) + '0';
  }

  return result;
}

template <std::integral T>
StringRef make_string_ref_uint(BlockAllocator &balloc, T n) {
  auto iov = make_byte_ref(balloc, NGHTTP2_MAX_UINT64_DIGITS + 1);
  auto p = std::ranges::begin(iov);

  p = util::utos(n, p);
  *p = '\0';

  return as_string_ref(std::ranges::begin(iov), p);
}

template <std::integral T> constexpr std::string utos_unit(T n) {
  char u;

  if (n >= (1 << 30)) {
    u = 'G';
    n /= (1 << 30);
  } else if (n >= (1 << 20)) {
    u = 'M';
    n /= (1 << 20);
  } else if (n >= (1 << 10)) {
    u = 'K';
    n /= (1 << 10);
  } else {
    return utos(n);
  }

  return utos(n) + u;
}

// Like utos_unit(), but 2 digits fraction part is followed.
template <std::integral T> constexpr std::string utos_funit(T n) {
  char u;
  int b;

  if (n >= (1 << 30)) {
    u = 'G';
    b = 30;
  } else if (n >= (1 << 20)) {
    u = 'M';
    b = 20;
  } else if (n >= (1 << 10)) {
    u = 'K';
    b = 10;
  } else {
    return utos(n);
  }

  return dtos(static_cast<double>(n) / (1 << b)) + u;
}

template <std::integral T> constexpr std::string utox(T n) {
  using namespace std::literals;

  if (n == 0) {
    return "0"s;
  }

  int i = 0;
  T t = n;

  for (; t; t /= 16, ++i)
    ;

  std::string res;

  res.resize(i);

  --i;

  for (; n; --i, n /= 16) {
    res[i] = UPPER_XDIGITS[(n & 0x0f)];
  }

  return res;
}

void to_token68(std::string &base64str);

StringRef to_base64(BlockAllocator &balloc, const StringRef &token68str);

void show_candidates(const char *unkopt, const option *options);

bool has_uri_field(const urlparse_url &u, urlparse_url_fields field);

bool fieldeq(const char *uri1, const urlparse_url &u1, const char *uri2,
             const urlparse_url &u2, urlparse_url_fields field);

bool fieldeq(const char *uri, const urlparse_url &u, urlparse_url_fields field,
             const char *t);

bool fieldeq(const char *uri, const urlparse_url &u, urlparse_url_fields field,
             const StringRef &t);

StringRef get_uri_field(const char *uri, const urlparse_url &u,
                        urlparse_url_fields field);

uint16_t get_default_port(const char *uri, const urlparse_url &u);

bool porteq(const char *uri1, const urlparse_url &u1, const char *uri2,
            const urlparse_url &u2);

void write_uri_field(std::ostream &o, const char *uri, const urlparse_url &u,
                     urlparse_url_fields field);

bool numeric_host(const char *hostname);

bool numeric_host(const char *hostname, int family);

// Returns numeric address string of |addr|.  If getnameinfo() is
// failed, "unknown" is returned.
std::string numeric_name(const struct sockaddr *sa, socklen_t salen);

// Returns string representation of numeric address and port of
// |addr|.  If address family is AF_UNIX, this return path to UNIX
// domain socket.  Otherwise, the format is like <HOST>:<PORT>.  For
// IPv6 address, address is enclosed by square brackets ([]).
std::string to_numeric_addr(const Address *addr);

std::string to_numeric_addr(const struct sockaddr *sa, socklen_t salen);

// Sets |port| to |addr|.
void set_port(Address &addr, uint16_t port);

// Get port from |su|.
uint16_t get_port(const sockaddr_union *su);

// Returns true if |port| is prohibited as a QUIC client port.
bool quic_prohibited_port(uint16_t port);

// Returns ASCII dump of |data| of length |len|.  Only ASCII printable
// characters are preserved.  Other characters are replaced with ".".
std::string ascii_dump(const uint8_t *data, size_t len);

// Returns absolute path of executable path.  If argc == 0 or |cwd| is
// nullptr, this function returns nullptr.  If argv[0] starts with
// '/', this function returns argv[0].  Otherwise return cwd + "/" +
// argv[0].  If non-null is returned, it is NULL-terminated string and
// dynamically allocated by malloc.  The caller is responsible to free
// it.
char *get_exec_path(int argc, char **const argv, const char *cwd);

// Validates path so that it does not contain directory traversal
// vector.  Returns true if path is safe.  The |path| must start with
// "/" otherwise returns false.  This function should be called after
// percent-decode was performed.
bool check_path(const std::string &path);

// Returns the |tv| value as 64 bit integer using a microsecond as an
// unit.
int64_t to_time64(const timeval &tv);

// Returns true if ALPN ID |proto| is supported HTTP/2 protocol
// identifier.
bool check_h2_is_selected(const StringRef &proto);

// Selects h2 protocol ALPN ID if one of supported h2 versions are
// present in |in| of length inlen.  Returns true if h2 version is
// selected.
bool select_h2(const unsigned char **out, unsigned char *outlen,
               const unsigned char *in, unsigned int inlen);

// Selects protocol ALPN ID if one of identifiers contained in |protolist| is
// present in |in| of length inlen.  Returns true if identifier is
// selected.
bool select_protocol(const unsigned char **out, unsigned char *outlen,
                     const unsigned char *in, unsigned int inlen,
                     std::vector<std::string> proto_list);

// Returns default ALPN protocol list, which only contains supported
// HTTP/2 protocol identifier.
std::vector<unsigned char> get_default_alpn();

// Parses delimited strings in |s| and returns the array of substring,
// delimited by |delim|.  The any white spaces around substring are
// treated as a part of substring.
std::vector<std::string> parse_config_str_list(const StringRef &s,
                                               char delim = ',');

// Parses delimited strings in |s| and returns Substrings in |s|
// delimited by |delim|.  The any white spaces around substring are
// treated as a part of substring.
std::vector<StringRef> split_str(const StringRef &s, char delim);

// Behaves like split_str, but this variant splits at most |n| - 1
// times and returns at most |n| sub-strings.  If |n| is zero, it
// falls back to split_str.
std::vector<StringRef> split_str(const StringRef &s, char delim, size_t n);

// Writes given time |tp| in Common Log format (e.g.,
// 03/Jul/2014:00:19:38 +0900) in buffer pointed by |out|.  The buffer
// must be at least 27 bytes, including terminal NULL byte.  Expected
// type of |tp| is std::chrono::time_point.  This function returns
// StringRef wrapping the buffer pointed by |out|, and this string is
// terminated by NULL.
template <typename T> StringRef format_common_log(char *out, const T &tp) {
  auto t =
    std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
  auto p = common_log_date(out, t.count());
  *p = '\0';
  return StringRef{out, p};
}

// Returns given time |tp| in ISO 8601 format (e.g.,
// 2014-11-15T12:58:24.741Z or 2014-11-15T12:58:24.741+09:00).
// Expected type of |tp| is std::chrono::time_point
template <typename T> std::string format_iso8601(const T &tp) {
  auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
    tp.time_since_epoch());
  return iso8601_date(t.count());
}

// Writes given time |tp| in ISO 8601 format (e.g.,
// 2014-11-15T12:58:24.741Z or 2014-11-15T12:58:24.741+09:00) in
// buffer pointed by |out|.  The buffer must be at least 30 bytes,
// including terminal NULL byte.  Expected type of |tp| is
// std::chrono::time_point.  This function returns StringRef wrapping
// the buffer pointed by |out|, and this string is terminated by NULL.
template <typename T> StringRef format_iso8601(char *out, const T &tp) {
  auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
    tp.time_since_epoch());
  auto p = iso8601_date(out, t.count());
  *p = '\0';
  return StringRef{out, p};
}

// Writes given time |tp| in ISO 8601 basic format (e.g.,
// 20141115T125824.741Z or 20141115T125824.741+0900) in buffer pointed
// by |out|.  The buffer must be at least 25 bytes, including terminal
// NULL byte.  Expected type of |tp| is std::chrono::time_point.  This
// function returns StringRef wrapping the buffer pointed by |out|,
// and this string is terminated by NULL.
template <typename T> StringRef format_iso8601_basic(char *out, const T &tp) {
  auto t = std::chrono::duration_cast<std::chrono::milliseconds>(
    tp.time_since_epoch());
  auto p = iso8601_basic_date(out, t.count());
  *p = '\0';
  return StringRef{out, p};
}

// Writes given time |tp| in HTTP Date format (e.g., Mon, 10 Oct 2016
// 10:25:58 GMT) in buffer pointed by |out|.  The buffer must be at
// least 30 bytes, including terminal NULL byte.  Expected type of
// |tp| is std::chrono::time_point.  This function returns StringRef
// wrapping the buffer pointed by |out|, and this string is terminated
// by NULL.
template <typename T> StringRef format_http_date(char *out, const T &tp) {
  auto t =
    std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch());
  auto p = http_date(out, t.count());
  *p = '\0';
  return StringRef{out, p};
}

// Return the system precision of the template parameter |Clock| as
// a nanosecond value of type |Rep|
template <typename Clock, typename Rep> Rep clock_precision() {
  std::chrono::duration<Rep, std::nano> duration = typename Clock::duration(1);

  return duration.count();
}

#ifdef HAVE_LIBEV
template <typename Duration = std::chrono::steady_clock::duration>
Duration duration_from(ev_tstamp d) {
  return std::chrono::duration_cast<Duration>(std::chrono::duration<double>(d));
}

template <typename Duration> ev_tstamp ev_tstamp_from(const Duration &d) {
  return std::chrono::duration<double>(d).count();
}
#endif // HAVE_LIBEV

int make_socket_closeonexec(int fd);
int make_socket_nonblocking(int fd);
int make_socket_nodelay(int fd);

int create_nonblock_socket(int family);
int create_nonblock_udp_socket(int family);

int bind_any_addr_udp(int fd, int family);

bool check_socket_connected(int fd);

// Returns the error code (errno) by inspecting SO_ERROR of given
// |fd|.  This function returns the error code if it succeeds, or -1.
// Returning 0 means no error.
int get_socket_error(int fd);

// Returns true if |host| is IPv6 numeric address (e.g., ::1)
bool ipv6_numeric_addr(const char *host);

// Parses |s| as unsigned integer and returns the parsed integer.
// Additionally, if |s| ends with 'k', 'm', 'g' and its upper case
// characters, multiply the integer by 1024, 1024 * 1024 and 1024 *
// 1024 respectively.  If there is an error, returns no value.
std::optional<int64_t> parse_uint_with_unit(const StringRef &s);

// Parses |s| as unsigned integer and returns the parsed integer..
std::optional<int64_t> parse_uint(const StringRef &s);

// Parses |s| as unsigned integer and returns the parsed integer
// casted to double.  If |s| ends with "s", the parsed value's unit is
// a second.  If |s| ends with "ms", the unit is millisecond.
// Similarly, it also supports 'm' and 'h' for minutes and hours
// respectively.  If none of them are given, the unit is second.  This
// function returns no value if error occurs.
std::optional<double> parse_duration_with_unit(const StringRef &s);

// Returns string representation of time duration |t|.  If t has
// fractional part (at least more than or equal to 1e-3), |t| is
// multiplied by 1000 and the unit "ms" is appended.  Otherwise, |t|
// is left as is and "s" is appended.
std::string duration_str(double t);

// Returns string representation of time duration |t|.  It appends
// unit after the formatting.  The available units are s, ms and us.
// The unit which is equal to or less than |t| is used and 2
// fractional digits follow.
std::string format_duration(const std::chrono::microseconds &u);

// Just like above, but this takes |t| as seconds.
std::string format_duration(double t);

// The maximum buffer size including terminal NULL to store the result
// of make_hostport.
constexpr size_t max_hostport = NI_MAXHOST + /* [] for IPv6 */ 2 + /* : */ 1 +
                                /* port */ 5 + /* terminal NULL */ 1;

// Just like make_http_hostport(), but doesn't treat 80 and 443
// specially.
StringRef make_hostport(BlockAllocator &balloc, const StringRef &host,
                        uint16_t port);

template <typename OutputIt>
StringRef make_hostport(OutputIt first, const StringRef &host, uint16_t port) {
  auto ipv6 = ipv6_numeric_addr(host.data());
  auto serv = utos(port);
  auto p = first;

  if (ipv6) {
    *p++ = '[';
  }

  p = std::copy(std::begin(host), std::end(host), p);

  if (ipv6) {
    *p++ = ']';
  }

  *p++ = ':';

  p = std::copy(std::begin(serv), std::end(serv), p);

  *p = '\0';

  return as_string_ref(first, p);
}

// Creates "host:port" string using given |host| and |port|.  If
// |host| is numeric IPv6 address (e.g., ::1), it is enclosed by "["
// and "]".  If |port| is 80 or 443, port part is omitted.
StringRef make_http_hostport(BlockAllocator &balloc, const StringRef &host,
                             uint16_t port);

template <typename OutputIt>
StringRef make_http_hostport(OutputIt first, const StringRef &host,
                             uint16_t port) {
  if (port != 80 && port != 443) {
    return make_hostport(first, host, port);
  }

  auto ipv6 = ipv6_numeric_addr(host.data());
  auto p = first;

  if (ipv6) {
    *p++ = '[';
  }

  p = std::copy(std::begin(host), std::end(host), p);

  if (ipv6) {
    *p++ = ']';
  }

  *p = '\0';

  return as_string_ref(first, p);
}

// hexdump dumps |data| of length |datalen| in the format similar to
// hexdump(1) with -C option.  This function returns 0 if it succeeds,
// or -1.
int hexdump(FILE *out, const void *data, size_t datalen);

// Copies 2 byte unsigned integer |n| in host byte order to |buf| in
// network byte order.
void put_uint16be(uint8_t *buf, uint16_t n);

// Copies 4 byte unsigned integer |n| in host byte order to |buf| in
// network byte order.
void put_uint32be(uint8_t *buf, uint32_t n);

// Retrieves 2 byte unsigned integer stored in |data| in network byte
// order and returns it in host byte order.
uint16_t get_uint16(const uint8_t *data);

// Retrieves 4 byte unsigned integer stored in |data| in network byte
// order and returns it in host byte order.
uint32_t get_uint32(const uint8_t *data);

// Retrieves 8 byte unsigned integer stored in |data| in network byte
// order and returns it in host byte order.
uint64_t get_uint64(const uint8_t *data);

// Reads mime types file (see /etc/mime.types), and stores extension
// -> MIME type map in |res|.  This function returns 0 if it succeeds,
// or -1.
int read_mime_types(std::map<std::string, std::string> &res,
                    const char *filename);

// Fills random alpha and digit byte to the range [|first|, |last|).
// Returns the one beyond the |last|.
template <typename OutputIt, typename Generator>
OutputIt random_alpha_digit(OutputIt first, OutputIt last, Generator &gen) {
  // If we use uint8_t instead char, gcc 6.2.0 complains by shouting
  // char-array initialized from wide string.
  static constexpr char s[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
  std::uniform_int_distribution<> dis(0, 26 * 2 + 10 - 1);
  for (; first != last; ++first) {
    *first = s[dis(gen)];
  }
  return first;
}

// Fills random bytes to the range [|first|, |last|).
template <typename OutputIt, typename Generator>
void random_bytes(OutputIt first, OutputIt last, Generator &gen) {
  std::uniform_int_distribution<uint8_t> dis;
  std::generate(first, last, [&dis, &gen]() { return dis(gen); });
}

// Shuffles the range [|first|, |last|] by calling swap function |fun|
// for each pair.  |fun| takes 2 RandomIt iterators.  If |fun| is
// noop, no modification is made.
template <typename RandomIt, typename Generator, typename SwapFun>
void shuffle(RandomIt first, RandomIt last, Generator &&gen, SwapFun fun) {
  auto len = std::distance(first, last);
  if (len < 2) {
    return;
  }

  using dist_type = std::uniform_int_distribution<size_t>;
  using param_type = dist_type::param_type;

  dist_type d;

  for (decltype(len) i = 0; i < len - 1; ++i) {
    fun(first + i, first + d(gen, param_type(i, len - 1)));
  }
}

// Returns x**y
double int_pow(double x, size_t y);

uint32_t hash32(const StringRef &s);

// Computes SHA-256 of |s|, and stores it in |buf|.  This function
// returns 0 if it succeeds, or -1.
int sha256(uint8_t *buf, const StringRef &s);

// Computes SHA-1 of |s|, and stores it in |buf|.  This function
// returns 0 if it succeeds, or -1.
int sha1(uint8_t *buf, const StringRef &s);

// Returns host from |hostport|.  If host cannot be found in
// |hostport|, returns empty string.  The returned string might not be
// NULL-terminated.
StringRef extract_host(const StringRef &hostport);

// split_hostport splits host and port in |hostport|.  Unlike
// extract_host, square brackets enclosing host name is stripped.  If
// port is not available, it returns empty string in the second
// string.  The returned string might not be NULL-terminated.  On any
// error, it returns a pair which has empty strings.
std::pair<StringRef, StringRef> split_hostport(const StringRef &hostport);

// Returns new std::mt19937 object.
std::mt19937 make_mt19937();

// daemonize calls daemon(3).  If __APPLE__ is defined, it implements
// daemon() using fork().
int daemonize(int nochdir, int noclose);

// Returns |s| from which trailing white spaces (SPC or HTAB) are
// removed.  If any white spaces are removed, new string is allocated
// by |balloc| and returned.  Otherwise, the copy of |s| is returned
// without allocation.
StringRef rstrip(BlockAllocator &balloc, const StringRef &s);

// contains returns true if |r| contains |value|.
template <std::ranges::input_range R, typename T>
bool contains(R &&r, const T &value) {
  return std::ranges::find(r, value) != std::ranges::end(r);
}

// contains returns true if |value| is contained in a range [|first|,
// |last|).
template <std::input_iterator I, typename T>
constexpr bool contains(I first, I last, const T &value) {
  return std::ranges::find(std::move(first), last, value) != last;
}

#ifdef ENABLE_HTTP3
int msghdr_get_local_addr(Address &dest, msghdr *msg, int family);

uint8_t msghdr_get_ecn(msghdr *msg, int family);

// msghdr_get_udp_gro returns UDP_GRO value from |msg|.  If UDP_GRO is
// not found, or UDP_GRO is not supported, this function returns 0.
size_t msghdr_get_udp_gro(msghdr *msg);
#endif // ENABLE_HTTP3

} // namespace util

} // namespace nghttp2

#endif // UTIL_H

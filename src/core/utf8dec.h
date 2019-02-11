/**
 * optimized utf8 decoder. Source: https://ndsol.com/volcano.
 *
 * This software Copyright (C) 2017-2018 The Volcano Authors.
 * Licensed under the LGPLv2.
 *
 * This file also defines __builtin_clz for compilers that lack it.
 */

#include <stdint.h>
#include <string.h>  // for memcpy

#ifdef _MSC_VER

#ifdef __MSC_TEMP_MAJOR
#undef __MSC_TEMP_MAJOR
#endif
#ifdef __MSC_TEMP_MINOR
#undef __MSC_TEMP_MINOR
#endif
#if _MSC_VER >= 1400
#define __MSC_TEMP_MAJOR (_MSC_FULL_VER / 1000000)
#define __MSC_TEMP_MINOR ((_MSC_FULL_VER / 10000) % 100)
#elif _MSC_VER >= 1200
#define __MSC_TEMP_MAJOR (_MSC_FULL_VER / 100000)
#define __MSC_TEMP_MINOR ((_MSC_FULL_VER / 1000) % 100)
#else
#define __MSC_TEMP_MAJOR (_MSC_VER / 100)
#define __MSC_TEMP_MINOR (_MSC_VER % 100)
#endif

#if __MSC_TEMP_MAJOR > 14 || (__MSC_TEMP_MAJOR == 14 && __MSC_TEMP_MINOR >= 10)
#include <intrin.h>
inline uint32_t __builtin_clz(uint32_t v) {
  unsigned long r = 0;
  if (_BitScanReverse(&r, v)) {
    return 31 - r;
  }
  return 31;
}
#endif

#elif defined(__GNUC__) && __GNUC__ > 3 || \
    (__GNUC__ == 3 && __GNUC_MINOR__ >= 4)

// Clang defines __GNUC__ also.
// __builtin_clz is built in to the compiler. No header file needed.
#define ALWAYS_INLINE __attribute__((always_inline))

#else /* Unknown compiler, old gcc, old msvc */

#define ALWAYS_INLINE __attribute__((always_inline))
inline uint32_t __builtin_clz(uint32_t v) ALWAYS_INLINE {
  static const uint32_t DeBruijn[32] = {
      31, 22, 30, 21, 18, 10, 29, 2,  20, 17, 15, 13, 9, 6,  28, 1,
      23, 19, 11, 3,  16, 14, 7,  24, 12, 4,  8,  25, 5, 26, 27, 0,
  };
  v |= v >> 1;
  v |= v >> 2;
  v |= v >> 4;
  v |= v >> 8;
  v |= v >> 16;
  return DeBruijn[(uint32_t)(v * 0x07C4ACDDU) >> 27];
}

#endif

// unaligned_uint32be() reads a uint32_t stored as big-endian.
#ifdef ALWAYS_INLINE
inline uint32_t unaligned_uint32be(void* src) ALWAYS_INLINE;
#endif
inline uint32_t unaligned_uint32be(void* src) {
  uint32_t val;
  memcpy(&val, src, sizeof(val));
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((val & 0xFF000000u) >> 24u) | ((val & 0x00FF0000u) >> 8u) |
         ((val & 0x0000FF00u) << 8u) | ((val & 0x000000FFu) << 24u);
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
  return val;
#else
#error byte order on this machine is unsupported.
#endif
}

// utf8_decode decodes one code point into utf32 and stores it in *c.
// *e is an error code: 0=no error, non-zero=decode failed.
// Returns a pointer to the next utf8 code point.
void* utf8_decode(void* buf, uint32_t* c, int* e);

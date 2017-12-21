/**
 * optimized utf8 decoder. Source: https://ndsol.com/volcano.
 *
 * This software Copyright (C) 2017 The Volcano Authors.
 * Licensed under the LGPLv2.
 *
 * The header file "utf8dec.h" also defines __builtin_clz for compilers that
 * lack it.
 *
 * Test carefully before committing changes! Stay small *and* fast:
 *
 * clang-802.0.42 -O3 -march=native on a Core i7-4770HQ @ 2.20GHz:
 *  branchless: 490.666725 MB/s, 0 errors
 *  this code:  392.000005 MB/s, 0 errors
 *  Hoehrmann:  337.333374 MB/s, 0 errors
 *
 * With gcc 4.8.4 -O3 -march=native on a Xeon E5-1650v3 @ 3.50GHz:
 *  branchless: 436.000052 MB/s, 0 errors
 *  this code:  396.000047 MB/s, 0 errors
 *  Hoehrmann:  412.000049 MB/s, 0 errors
 *
 * With clang 3.4 -O3 -march=native on a Xeon E5-1650v3 @ 3.50GHz:
 *  this code:  444.000053 MB/s, 0 errors
 *
 * With gcc-7, objdump -s -j .text -j .rodata indicates this file uses only
 * 221 bytes.
 *
 * Faster versions, such as https://news.ycombinator.com/item?id=15425658, run
 * at about 577 MB/s on hardware that runs branchless at 410 MB/s, but are not
 * written strictly in C99.
 */

#include "utf8dec.h"

// Add -march=native on the gcc command line to upgrade from "bswap" to "movbe".
// Clang requires -O3 to be specified on the command line; no pragma will do it.
#if defined(__GNUC__) && !defined(__clang__)
#pragma GCC optimize("-O3")
#endif

void *utf8_decode(void *buf, uint32_t *c, int *e) {
  uint32_t encoded = unaligned_uint32be(buf);
  uint32_t coded_len =
      (__builtin_clz(~encoded | 0xfffffffu /*ensure bit 27 is always set*/) ^
       31) -
      27;

#if 1
#define ENCSH (0u)
  uint32_t err = (encoded ^ (0x808080 >> ENCSH));
  // Cost: -7 bytes (gcc-7). This saves 7 bytes and runs faster.
  // gcc-7:368MB/s clang:381MB/s
  // decoded: 0abcdefg 00hijklm 00nopqrs 00tuvwxy
  uint32_t decoded = encoded & 0x7f3f3f3f;
  uint32_t half1 = decoded & 0x3f003f;
  // half1: 00hijklm 00000000 00tuvwxy
  decoded += half1 * 3;
  decoded >>= 2u;
  // decoded: 000abcde fghijklm 0000nopq rstuvwxy
  uint16_t half2 = decoded << 4u;
  decoded = (decoded & ~0xffff) | (half2 & 0xffff);
  // decoded: 000abcde fghijklm nopqrstu vwxy0000
  decoded >>= 4u;
  decoded &= ((uint32_t)-1) >> (11 - coded_len);
#else
  // gcc-7:324MB/s clang:373MB/s
  static const uint8_t masks[] = {0x07, 0x0f, 0x1f, 0x00, 0x7f};
  uint32_t decoded = encoded & 0x3f;
  encoded >>= 6u;

  decoded |= ((encoded) & (0x3f << 2u)) << 4u;
  decoded |= ((encoded) & (0x3f << 10u)) << 2u;
  decoded |= (encoded) & (masks[coded_len] << 18u);
#define ENCSH (6u)
#endif
  err &= (0xc0c0c0 >> ENCSH);

  /* Compute the pointer to the next character early so that the next
   * iteration can start working on the next character. Neither Clang
   * nor GCC figure out this reordering on their own.
   */
#if 0
    // Cost: +9 bytes (gcc-7)
#define LEN_TAB ((1 << 20) | 0 | (2 << 10) | (3 << 5) | 4)
    // gcc-7:337MB/s clang:360MB/s
    uint32_t len = (LEN_TAB >> (coded_len*5)) & 7;
    unsigned char* next = (unsigned char*) buf + (len + !len);
#else
  static const uint8_t lengths[] = {
      4,  // utf8[0] 0xf0..0xff
      3,  // utf8[0] 0xe0..0xef
      2,  // utf8[0] 0xc0..0xdf
      1,  // utf8[0] 0x80..0xbf (invalid encoding)
      1,  // utf8[0] 0..0x7f
  };
  // gcc-7:345MB/s clang:392MB/s
  unsigned char *next = (unsigned char *)buf + lengths[coded_len];
#endif

#ifdef SHIFT_FROM_CODED_LEN
  // gcc-7:368MB/s clang:381MB/s
  uint32_t shift = 2 * (coded_len - (coded_len >> 2u));
#else
  static const uint8_t shifts[] = {0, 2, 4, 0, 6};
  // gcc-7:324MB/s clang:373MB/s
  uint32_t shift = shifts[coded_len];
#endif
  err >>= (shift * 4);
  *c = decoded >> (shift * 3);

#if 1
  // Cost: +4 bytes (gcc-7)
#define MINS_TAB ((25 << 20) | (15 << 15) | (0 << 10) | (4 << 5) | 9)
  // gcc-7:328MB/s clang:375MB/s
  uint32_t overlong =
      (uint32_t)((1llu << 7) << ((MINS_TAB >> (coded_len * 5)) & 0x1f));
#else
  static const uint8_t mins[] = {
      9,  4, 0, 15,
      25,  // Rely on 64-bit shift-left followed by cast to 32-bit to get 0.
  };
  // gcc-7:324MB/s clang:373MB/s
  uint32_t overlong = (uint32_t)((1llu << 7) << mins[coded_len]);
#endif
  err |= (*c < overlong);        // overlong form?
  err |= ((*c >> 11u) == 0x1b);  // surrogate half?
  *e = err;
#undef ENCSH

  /* clang 3.8.1 57 ret */
  /* clang 5.0 54 ret */
  /* gcc 5.4 55 ret 67 end */
  /* gcc 7.2 55 ret 67 end */
  return next;
}

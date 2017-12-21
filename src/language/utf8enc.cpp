/**
 * optimized utf8 encoder. Source: https://ndsol.com/volcano.
 *
 * This software Copyright (C) 2017 The Volcano Authors.
 * Licensed under the LGPLv2.
 */

#include "utf8enc.h"

int utf8_encode(char* utf8, size_t utf8len, uint32_t utf32) {
  if ((utf32 >= 0xd800 && utf32 < 0xe000) || utf32 > 0x10ffff) {
    // Code point violates RFC 3629.
    return 0;
  }
  char* writer = utf8;

  // Calculate len of utf8.
  static const unsigned transitions[] = {
      0x00000080,  // 1-byte utf8
      0x00000800,  // 2-byte utf8
      0x00010000,  // 3-byte utf8
      0x00200000,  // 4-byte utf8
  };
  constexpr static int maxlen = sizeof(transitions) / sizeof(transitions[0]);
  int len;
  for (len = 0; len < maxlen; len++) {
    if (utf32 < transitions[len]) {
      break;
    }
  }
  // Encode length in first utf8 byte.
  *(writer++) = (utf32 >> len) | ((0xfc << (5 - len)) & (-!!len));
  // Encode continuation bytes.
  for (len *= 6; len;) {
    utf8len--;
    if (!utf8len) return writer - utf8;
    len -= 6;
    *(writer++) = ((utf32 >> len) & 0x3f) | 0x80;
  }
  *(writer++) = 0;
  return writer - utf8;
}

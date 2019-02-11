/**
 * optimized utf8 encoder. Source: https://ndsol.com/volcano.
 *
 * This software Copyright (C) 2017-2018 The Volcano Authors.
 * Licensed under the LGPLv2.
 */

#include "utf8dec.h"

/* utf8_encode encodes a utf32 code point and returns the number of bytes
 * written to utf8. The buffer size, utf8len, is never passed and the buffer
 * is always null-terminated, so if the return value == utf8len, the buffer
 * was too small.
 *
 * A buffer of 5 bytes will always suffice for one code point, but for
 * utf32 string processing, carefully protect against overflow.
 *
 * utf8_encode returns 0 if the utf32 code point is disallowed by RFC 3629.
 */
int utf8_encode(char* utf8, size_t utf8len, uint32_t utf32);

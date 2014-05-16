/* Copyright (c) 2002 Tom Holroyd
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */
/* This is a 32 bit FNV hash.  For more information, see
http://www.isthe.com/chongo/tech/comp/fnv/index.html */

#ifndef FNV_H
#define FNV_H

#include <sys/types.h>
#include "config.h"

#include "inline.h"

#define FNV1_32_INIT 0x811C9DC5
#define FNV_32_PRIME 0x01000193

static GCC_INLINE u_int32_t fnv_hash(const char x, const u_int32_t hash)
{
    return ((hash * FNV_32_PRIME) ^ x);
}

/* Hash a buffer. */

static GCC_INLINE u_int32_t fnv_hash_buf(const u_char *s, const int len)
{
    u_int32_t h = FNV1_32_INIT;
    const u_char * const end = s + len;

    while (s < end)
    {
        h = fnv_hash((*(s++)), h);
    }

    return h;
}

/* Hash a 0 terminated string. */

static GCC_INLINE u_int32_t fnv_hash_str(const u_char *s)
{
    u_int32_t h = FNV1_32_INIT;

    u_char c;
    while ((c = *(s++)))
    {
        h = fnv_hash(c, h);
    }

    return h;
}

#endif

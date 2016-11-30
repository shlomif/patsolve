/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */
/* This is a 32 bit FNV hash.  For more information, see
http://www.isthe.com/chongo/tech/comp/fnv/index.html */

#pragma once

#include <sys/types.h>
#include "config.h"

#include "inline.h"
#include "portable_int32.h"

#define FNV1_32_INIT 0x811C9DC5
#define FNV_32_PRIME 0x01000193

static GCC_INLINE u_int32_t fnv_hash(const char x, const u_int32_t hash)
{
    return ((hash * FNV_32_PRIME) ^ x);
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

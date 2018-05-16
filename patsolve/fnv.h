/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://github.com/shlomif/patsolve/blob/master/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */
// This is a 32 bit FNV hash.  For more information, see
// http://www.isthe.com/chongo/tech/comp/fnv/index.html
#pragma once
#include "fcs_conf.h"

#define FNV1_32_INIT 0x811C9DC5
#define FNV_32_PRIME 0x01000193

static inline uint32_t fnv_hash(const unsigned char x, const uint32_t hash)
{
    return ((hash * FNV_32_PRIME) ^ (uint32_t)x);
}

static inline __attribute__((pure)) uint32_t fnv_hash_str(
    const unsigned char *s)
{
    uint32_t h = FNV1_32_INIT;
    unsigned char c;
    while ((c = *(s++)))
    {
        h = fnv_hash(c, h);
    }

    return h;
}

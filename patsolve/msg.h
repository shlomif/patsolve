/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://github.com/shlomif/patsolve/blob/master/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */
#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

static inline void fc_solve_msg(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
}

#ifdef __cplusplus
}
#endif

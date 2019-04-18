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

#include <stdarg.h>
#include <stdio.h>

#include "freecell-solver/fcs_conf.h"
#include "msg.h"

static const char *program_name = NULL;

/* Print a message and exit. */
static void fatalerr(const char *msg, ...)
{
    va_list ap;

    if (program_name)
    {
        fprintf(stderr, "%s: ", program_name);
    }
    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
    fputc('\n', stderr);

    exit(1);
}

static const char Usage[];

#define USAGE() fc_solve_msg(Usage, program_name)

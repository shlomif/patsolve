/*
 * This file is part of patsolve. It is subject to the license terms in
 * the LICENSE file found in the top-level directory of this distribution
 * and at https://bitbucket.org/shlomif/patsolve-shlomif/src/LICENSE . No
 * part of patsolve, including this file, may be copied, modified, propagated,
 * or distributed except according to the terms contained in the COPYING file.
 *
 * Copyright (c) 2002 Tom Holroyd
 */

#pragma once

#include <stdarg.h>
#include <stdio.h>

#include "config.h"

static void print_msg(const char *msg, ...)
{
    va_list ap;

    va_start(ap, msg);
    vfprintf(stderr, msg, ap);
    va_end(ap);
}

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

#define USAGE() print_msg(Usage, program_name)
